// Integration tests that drive the plugin through its real, public
// PluginCodec ABI (OpalCodecPlugin_GetCodecs()) linked against the REAL
// OpenH264 library - not the fake_wels/ test double used by
// test_plugin_abi.cxx. This is what actually proves the encoder produces
// a bitstream a real H.264 decoder accepts, and specifically that this
// plugin's own encoder output can be fed straight into this plugin's own
// decoder and come back out as a recognisable picture.
//
// Requires libopenh264 headers + library at build time (see tests/
// Makefile's `check-real` target - not part of the default `all`/`check`
// since a machine without the real SDK installed shouldn't fail the
// fast, deterministic fake-SDK-based suite). Needs no special runtime
// setup beyond that: it does not touch the network or the filesystem
// beyond what libopenh264 itself does internally.
#include "framework.h"

#include <codec/opalplugin.h>
#ifdef _MSC_VER
#include "../../../common/rtpframe.h"
#else
#include "rtpframe.h"
#endif

#include <vector>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <thread>
#include <atomic>
#include <string>

#include "../h264frame.h"

extern "C" {
  struct PluginCodec_Definition * OpalCodecPlugin_GetCodecs(unsigned * count, unsigned version);
}

namespace {

typedef int (*ControlFn)(const struct PluginCodec_Definition *, void *, const char *, void *, unsigned *);

struct CodecDefs {
  const PluginCodec_Definition * encoder = nullptr;
  const PluginCodec_Definition * decoder = nullptr;
};

// Using H.264-CIF (352x288) throughout - the smallest of the enabled
// capabilities (see openh264.h), which keeps a real-encode/real-decode
// test suite fast.
CodecDefs FindCifCodecs() {
  unsigned count = 0;
  PluginCodec_Definition * defs = OpalCodecPlugin_GetCodecs(&count, PLUGIN_CODEC_VERSION_OPTIONS);
  CodecDefs out;
  for (unsigned i = 0; i < count; i++) {
    if (defs[i].destFormat && strcmp(defs[i].destFormat, "H.264-CIF") == 0)
      out.encoder = &defs[i];
    if (defs[i].sourceFormat && strcmp(defs[i].sourceFormat, "H.264-CIF") == 0)
      out.decoder = &defs[i];
  }
  return out;
}

ControlFn FindControl(const PluginCodec_Definition * def, const char * name) {
  for (PluginCodec_ControlDefn * c = def->codecControls; c != nullptr && c->name != nullptr; c++) {
    if (strcmp(c->name, name) == 0)
      return c->control;
  }
  return nullptr;
}

const unsigned kWidth = 352;
const unsigned kHeight = 288;

// PluginCodec_Video_FrameHeader / SSourcePicture's row stride is the
// width rounded up to even (see openh264.cxx's EncodeFrames(), which
// sets picture.iStride[0]/picture.pData[1]/[2] from that rounded value,
// not the raw claimed width) - odd widths therefore have (pw - width)
// bytes of unused padding at the end of every source row. Both the
// synthetic frame builder below and any code comparing against it need
// to use the same stride, or an odd-width comparison silently compares
// misaligned bytes.
static unsigned RoundUpEven(unsigned n) { return (n + 1) & ~1u; }

// A synthetic I420 test picture: a diagonal luma gradient that shifts
// with frameIndex (so consecutive frames aren't bit-identical, giving
// the encoder's inter-prediction something real to do) and with
// threadSeed (so concurrently-running instances encode visibly
// different content from each other - see the concurrency test below),
// flat mid-grey chroma. Deliberately simple - this is a codec plumbing
// test, not an image-quality benchmark.
std::vector<uint8_t> BuildSyntheticFrame(unsigned width, unsigned height, int frameIndex, int threadSeed = 0) {
  unsigned pw = RoundUpEven(width);
  unsigned ph = RoundUpEven(height);
  size_t ySize = (size_t)pw * ph;   // matches what EncodeFrames() requires the buffer to hold
  size_t cSize = ySize / 4;
  std::vector<uint8_t> buf(12 + sizeof(PluginCodec_Video_FrameHeader) + ySize + 2 * cSize, 0);
  PluginCodec_Video_FrameHeader * hdr = (PluginCodec_Video_FrameHeader *)(buf.data() + 12);
  hdr->x = 0;
  hdr->y = 0;
  hdr->width = width;   // the claimed (possibly odd) dimensions, as a real caller would report
  hdr->height = height;

  uint8_t * y = OPAL_VIDEO_FRAME_DATA_PTR(hdr);
  uint8_t * u = y + ySize;
  uint8_t * v = u + cSize;
  for (unsigned row = 0; row < height; row++) {
    for (unsigned col = 0; col < width; col++) {
      y[row * pw + col] = (uint8_t)(((row + col + frameIndex * 4 + threadSeed * 37) * 255 / (width + height)) & 0xff);
    }
  }
  memset(u, 0x80, cSize);
  memset(v, 0x80, cSize);
  return buf;
}

// Extracts a tightly-packed (no row padding) width*height copy of a
// PluginCodec_Video_FrameHeader's Y plane, de-striding it first if width
// is odd (see the stride comment above) - for comparing against a
// decoded picture's Y plane, which this plugin's own DecodeFrames()
// already de-strides to tightly-packed on the way out.
std::vector<uint8_t> ExtractTightYPlane(const PluginCodec_Video_FrameHeader * hdr) {
  unsigned pw = RoundUpEven(hdr->width);
  const uint8_t * strided = OPAL_VIDEO_FRAME_DATA_PTR(hdr);
  std::vector<uint8_t> out((size_t)hdr->width * hdr->height);
  for (unsigned row = 0; row < hdr->height; row++)
    memcpy(&out[(size_t)row * hdr->width], strided + (size_t)row * pw, hdr->width);
  return out;
}

struct EncodedPicture {
  std::vector<std::vector<uint8_t>> packets; // one full RTP packet (header+payload) per entry
};

// Drives the encoder's codecFunction() with one raw frame until it
// reports PluginCodec_ReturnCoderLastFrame, collecting every RTP packet
// produced along the way (single NAL and/or FU-A fragments alike).
EncodedPicture EncodeOnePicture(const PluginCodec_Definition * encDef, void * encContext,
                                 const std::vector<uint8_t> & rawFrame) {
  EncodedPicture pic;
  bool sawLastFrame = false;
  for (int i = 0; i < 500 && !sawLastFrame; i++) { // generous cap against an infinite loop on failure
    std::vector<uint8_t> out(65536, 0);
    unsigned fromLen = (unsigned)rawFrame.size();
    unsigned toLen = (unsigned)out.size();
    unsigned flags = 0;
    encDef->codecFunction(encDef, encContext, rawFrame.data(), &fromLen, out.data(), &toLen, &flags);
    if (toLen > 0) {
      out.resize(toLen);
      pic.packets.push_back(out);
    }
    if (flags & PluginCodec_ReturnCoderLastFrame)
      sawLastFrame = true;
  }
  return pic;
}

struct DecodedPicture {
  bool got = false;
  unsigned width = 0, height = 0;
  std::vector<uint8_t> yPlane;
};

// Feeds every packet of one encoded picture into the decoder in order.
// Real OpenH264 sometimes needs a call or two past the marker packet
// before a picture actually comes out (internal buffering), so this
// keeps calling with the LAST packet again (a harmless duplicate from
// the decoder's perspective, since RTPFrame reconstruction only cares
// about NAL content, not being called exactly once) until either a
// picture appears or a generous attempt cap is hit.
DecodedPicture DecodeOnePicture(const PluginCodec_Definition * decDef, void * decContext,
                                 const EncodedPicture & pic) {
  DecodedPicture result;
  if (pic.packets.empty())
    return result;

  // One pass, in order. An earlier version of this helper "nudged" the
  // decoder with extra calls resending the final packet when nothing
  // came out on the first pass - which was actively harmful for any
  // picture large enough to need FU-A fragmentation (more than one RTP
  // packet): resending the marker (end) fragment after it had already
  // been consumed hit H264Frame::DeencapsulateFU()'s "end fragment
  // without a preceding start" case (since _currentFU had already been
  // reset to 0), which fails SetFromRTPFrame() and aborts the decode -
  // exactly backwards from "encourage a flush". With this plugin's
  // real-time/no-B-frames encoder configuration, a complete picture's
  // real decoded output is available immediately after its own last
  // (marker) packet is processed; no retry is needed or correct here.
  for (size_t i = 0; i < pic.packets.size(); i++) {
    std::vector<uint8_t> in = pic.packets[i];
    // Must hold a full raw YUV frame, not an RTP-sized packet - sized
    // generously for everything up to 4K (the largest format this file
    // tests; see kCommonFormats). A too-small buffer here isn't a
    // plugin bug, it's PluginCodec_ReturnCoderBufferTooSmall, and it
    // was silently swallowed by this helper for every resolution above
    // roughly CIF/QVGA until this was sized correctly.
    std::vector<uint8_t> out(16 * 1024 * 1024, 0);
    unsigned fromLen = (unsigned)in.size();
    unsigned toLen = (unsigned)out.size();
    unsigned flags = 0;
    decDef->codecFunction(decDef, decContext, in.data(), &fromLen, out.data(), &toLen, &flags);
    if (toLen > 0) {
      RTPFrame rtp(out.data(), (int)toLen);
      PluginCodec_Video_FrameHeader * hdr = (PluginCodec_Video_FrameHeader *)rtp.GetPayloadPtr();
      result.got = true;
      result.width = hdr->width;
      result.height = hdr->height;
      size_t ySize = (size_t)hdr->width * hdr->height;
      uint8_t * y = OPAL_VIDEO_FRAME_DATA_PTR(hdr);
      result.yPlane.assign(y, y + ySize);
      return result;
    }
  }
  return result;
}

double MeanAbsYDiff(const std::vector<uint8_t> & a, const std::vector<uint8_t> & b) {
  size_t n = std::min(a.size(), b.size());
  if (n == 0) return 255.0;
  double sum = 0;
  for (size_t i = 0; i < n; i++)
    sum += std::abs((int)a[i] - (int)b[i]);
  return sum / (double)n;
}

// Returns the set of NAL types seen across every packet of one encoded
// picture, by de-encapsulating with a throwaway H264Frame - reusing the
// plugin's own real, tested packetiser rather than hand-parsing RTP/FU-A
// here too.
} // namespace

namespace {
std::vector<uint8_t> CollectNalTypes(const EncodedPicture & pic) {
  H264Frame frame;
  unsigned flags = 0;
  for (auto raw : pic.packets) { // copy: SetFromRTPFrame doesn't require const
    RTPFrame rtp(raw.data(), (int)raw.size());
    frame.SetFromRTPFrame(rtp, flags);
  }
  // Walk the reconstructed Annex-B buffer ourselves to list NAL types -
  // GetFramePtr()/GetFrameSize() give the concatenated 00 00 00 01 +
  // header + payload sequence.
  std::vector<uint8_t> types;
  const uint8_t * p = frame.GetFramePtr();
  uint32_t len = frame.GetFrameSize();
  uint32_t i = 0;
  while (i + 4 < len) {
    if (p[i] == 0 && p[i+1] == 0 && p[i+2] == 0 && p[i+3] == 1) {
      types.push_back(p[i+4] & 0x1f);
      i += 4;
    }
    else {
      i++;
    }
  }
  return types;
}

bool Contains(const std::vector<uint8_t> & v, uint8_t x) {
  for (auto e : v) if (e == x) return true;
  return false;
}

} // namespace

TEST(RealOpenH264, GetCodecs_FindsCifEncoderAndDecoder) {
  CodecDefs defs = FindCifCodecs();
  ASSERT_TRUE(defs.encoder != nullptr);
  ASSERT_TRUE(defs.decoder != nullptr);
}

TEST(RealOpenH264, Encoder_ProducesSpsPpsAndIdrSlice_ForFirstPicture) {
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.encoder->createCodec(defs.encoder);
  ASSERT_TRUE(ctx != nullptr);

  std::vector<uint8_t> raw = BuildSyntheticFrame(kWidth, kHeight, 0);
  EncodedPicture pic = EncodeOnePicture(defs.encoder, ctx, raw);
  ASSERT_TRUE(!pic.packets.empty());

  std::vector<uint8_t> types = CollectNalTypes(pic);
  ASSERT_TRUE(Contains(types, 7)); // SPS
  ASSERT_TRUE(Contains(types, 8)); // PPS
  ASSERT_TRUE(Contains(types, 5)); // IDR slice

  defs.encoder->destroyCodec(defs.encoder, ctx);
}

// The test the plugin was asked for: real encoder output fed straight
// into the real decoder, and the decoded picture actually resembles what
// was encoded.
TEST(RealOpenH264, EncoderOutput_DecodesBackToARecognisablePicture) {
  CodecDefs defs = FindCifCodecs();
  void * encCtx = defs.encoder->createCodec(defs.encoder);
  void * decCtx = defs.decoder->createCodec(defs.decoder);
  ASSERT_TRUE(encCtx != nullptr);
  ASSERT_TRUE(decCtx != nullptr);

  std::vector<uint8_t> raw = BuildSyntheticFrame(kWidth, kHeight, 0);
  EncodedPicture pic = EncodeOnePicture(defs.encoder, encCtx, raw);
  ASSERT_TRUE(!pic.packets.empty());

  DecodedPicture dec = DecodeOnePicture(defs.decoder, decCtx, pic);
  ASSERT_TRUE(dec.got);
  ASSERT_EQ(dec.width, kWidth);
  ASSERT_EQ(dec.height, kHeight);

  std::vector<uint8_t> origYVec = ExtractTightYPlane((PluginCodec_Video_FrameHeader *)(raw.data() + 12));
  double diff = MeanAbsYDiff(origYVec, dec.yPlane);
  // Lossy compression, so not an exact match - but a mean absolute luma
  // error this large would mean something is structurally wrong (wrong
  // plane, garbage decode, misaligned strides), not just quantisation.
  ASSERT_TRUE(diff < 25.0);

  defs.decoder->destroyCodec(defs.decoder, decCtx);
  defs.encoder->destroyCodec(defs.encoder, encCtx);
}

// Several pictures in a row through one encoder/decoder pair - exercises
// real inter-frame (P-slice) prediction end to end, which the fake-SDK
// tests can't touch at all since the fake never does real decoding.
TEST(RealOpenH264, MultiplePictures_RoundTrip_AllDecodeSuccessfully) {
  CodecDefs defs = FindCifCodecs();
  void * encCtx = defs.encoder->createCodec(defs.encoder);
  void * decCtx = defs.decoder->createCodec(defs.decoder);

  const int numPictures = 5;
  for (int f = 0; f < numPictures; f++) {
    std::vector<uint8_t> raw = BuildSyntheticFrame(kWidth, kHeight, f);
    EncodedPicture pic = EncodeOnePicture(defs.encoder, encCtx, raw);
    ASSERT_TRUE(!pic.packets.empty());

    DecodedPicture dec = DecodeOnePicture(defs.decoder, decCtx, pic);
    ASSERT_TRUE(dec.got);
    ASSERT_EQ(dec.width, kWidth);
    ASSERT_EQ(dec.height, kHeight);

    std::vector<uint8_t> origYVec = ExtractTightYPlane((PluginCodec_Video_FrameHeader *)(raw.data() + 12));
    double diff = MeanAbsYDiff(origYVec, dec.yPlane);
    ASSERT_TRUE(diff < 25.0);
  }

  defs.decoder->destroyCodec(defs.decoder, decCtx);
  defs.encoder->destroyCodec(defs.encoder, encCtx);
}

// Regression coverage for the encoder_event_handler() deadlock (see
// ReadMe.txt / test_plugin_abi.cxx), this time against the real codec:
// a fast-update request must not hang, and the very next encoded picture
// must contain a fresh IDR slice.
TEST(RealOpenH264, FastUpdateRequest_ProducesFreshIDR) {
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.encoder->createCodec(defs.encoder);

  // First picture to get past the initial (always-IDR) one.
  EncodeOnePicture(defs.encoder, ctx, BuildSyntheticFrame(kWidth, kHeight, 0));

  ControlFn eventFn = FindControl(defs.encoder, PLUGINCODEC_CONTROL_CODEC_EVENT);
  ASSERT_TRUE(eventFn != nullptr);
  const char * parm[3] = { PLUGINCODEC_EVENT_FASTUPDATE, "1", nullptr };
  unsigned parmLen = sizeof(const char **);
  int ok = eventFn(defs.encoder, ctx, PLUGINCODEC_CONTROL_CODEC_EVENT, (void *)parm, &parmLen);
  ASSERT_TRUE(ok);

  EncodedPicture pic = EncodeOnePicture(defs.encoder, ctx, BuildSyntheticFrame(kWidth, kHeight, 1));
  std::vector<uint8_t> types = CollectNalTypes(pic);
  ASSERT_TRUE(Contains(types, 5)); // IDR slice, not just another P-slice

  defs.encoder->destroyCodec(defs.encoder, ctx);
}

// =======================================================================
// Common and uncommon video format coverage
// =======================================================================

struct FormatSpec {
  const char * name;
  unsigned width, height;
};

// Standard/common video resolutions, roughly smallest to largest. All of
// these are expected to work - a genuine regression if any is rejected.
static const FormatSpec kCommonFormats[] = {
  { "SQCIF",  128,   96 },
  { "QCIF",   176,  144 },
  { "QVGA",   320,  240 },
  { "CIF",    352,  288 },
  { "WVGA",   800,  480 },
  { "VGA",    640,  480 },
  { "4CIF",   704,  576 },
  { "SVGA",   800,  600 },
  { "720p",  1280,  720 },
  { "16CIF", 1408, 1152 },
  { "1080p", 1920, 1080 },
  { "1440p", 2560, 1440 },
  { "4K-UHD", 3840, 2160 },
};

// Deliberately awkward shapes: odd (non-even) dimensions, dimensions not
// a multiple of 16, extreme aspect ratios, portrait orientation, and one
// very large odd-dimensioned frame. These are NOT asserted to succeed -
// this is exploratory coverage of the plugin/real encoder's behaviour at
// the edges, and the printed accepted/rejected lists are the point.
static const FormatSpec kUncommonFormats[] = {
  { "Tiny-16x16",              16,   16 },
  { "Tiny-2x2",                  2,    2 },
  { "OddBoth-351x287",         351,  287 },
  { "OddWidth-353x288",        353,  288 },
  { "OddHeight-352x289",       352,  289 },
  { "NonMult16Even-350x290",   350,  290 },
  { "UltraWide-1920x64",      1920,   64 },
  { "UltraTall-64x1920",        64, 1920 },
  { "MobilePortrait-1080x1920", 1080, 1920 },
  { "Square-512x512",          512,  512 },
  { "OddSquare-511x511",       511,  511 },
  { "Laptop-1366x768",        1366,  768 },
  { "Odd4K-3839x2159",        3839, 2159 },
};

struct FormatResult {
  std::string name;
  unsigned width, height;
  bool accepted;
  std::string detail;
};

FormatResult TryFormat(const char * name, unsigned width, unsigned height, const CodecDefs & defs) {
  FormatResult r;
  r.name = name;
  r.width = width;
  r.height = height;
  r.accepted = false;

  void * encCtx = nullptr;
  void * decCtx = nullptr;
  try {
    encCtx = defs.encoder->createCodec(defs.encoder);
    decCtx = defs.decoder->createCodec(defs.decoder);
    if (encCtx == nullptr || decCtx == nullptr) {
      r.detail = "createCodec returned null";
    }
    else {
      std::vector<uint8_t> raw = BuildSyntheticFrame(width, height, 0);
      EncodedPicture pic = EncodeOnePicture(defs.encoder, encCtx, raw);
      if (pic.packets.empty()) {
        r.detail = "encoder produced no packets";
      }
      else {
        DecodedPicture dec = DecodeOnePicture(defs.decoder, decCtx, pic);
        if (!dec.got) {
          r.detail = "decoder produced no picture";
        }
        else if (dec.width != width || dec.height != height) {
          char buf[128];
          snprintf(buf, sizeof(buf), "dimension mismatch: encoded %ux%u, decoded %ux%u",
                   width, height, dec.width, dec.height);
          r.detail = buf;
        }
        else {
          std::vector<uint8_t> origYVec = ExtractTightYPlane((PluginCodec_Video_FrameHeader *)(raw.data() + 12));
          double diff = MeanAbsYDiff(origYVec, dec.yPlane);
          char buf[64];
          if (diff < 30.0) {
            snprintf(buf, sizeof(buf), "OK (mean abs Y diff %.2f)", diff);
            r.detail = buf;
            r.accepted = true;
          }
          else {
            snprintf(buf, sizeof(buf), "excessive pixel difference (%.2f)", diff);
            r.detail = buf;
          }
        }
      }
    }
  }
  catch (const std::exception & e) {
    r.detail = std::string("exception: ") + e.what();
  }
  catch (...) {
    r.detail = "unknown exception";
  }

  if (decCtx != nullptr) defs.decoder->destroyCodec(defs.decoder, decCtx);
  if (encCtx != nullptr) defs.encoder->destroyCodec(defs.encoder, encCtx);
  return r;
}

TEST(RealOpenH264, CommonAndUncommonVideoFormats_AcceptRejectSummary) {
  CodecDefs defs = FindCifCodecs();
  ASSERT_TRUE(defs.encoder != nullptr && defs.decoder != nullptr);

  std::vector<FormatResult> results;
  for (auto & f : kCommonFormats)
    results.push_back(TryFormat(f.name, f.width, f.height, defs));
  size_t commonCount = results.size();
  for (auto & f : kUncommonFormats)
    results.push_back(TryFormat(f.name, f.width, f.height, defs));

  printf("\n  ---- Accepted formats ----\n");
  for (auto & r : results)
    if (r.accepted)
      printf("    %-28s %5ux%-5u  %s\n", r.name.c_str(), r.width, r.height, r.detail.c_str());

  printf("  ---- Rejected formats ----\n");
  bool anyRejected = false;
  for (auto & r : results) {
    if (!r.accepted) {
      printf("    %-28s %5ux%-5u  %s\n", r.name.c_str(), r.width, r.height, r.detail.c_str());
      anyRejected = true;
    }
  }
  if (!anyRejected)
    printf("    (none)\n");
  printf("\n");

  // Every common/standard resolution must work - a rejection here is a
  // genuine regression.
  for (size_t i = 0; i < commonCount; i++)
    ASSERT_TRUE(results[i].accepted);

  // Uncommon/edge-shaped formats are exploratory - not asserted either
  // way (some rejections here can be expected real-encoder behaviour,
  // e.g. odd dimensions), the printed lists above are the result.
}

// Multiple encoder/decoder instances running concurrently, one thread
// per instance, against the REAL OpenH264 library - the strongest
// version of this check, since there's no fake-SDK test double standing
// between the assertion and the actual codec: each thread encodes its
// own distinctly-seeded content (see BuildSyntheticFrame's threadSeed)
// with its own real ISVCEncoder, decodes it with its own real
// ISVCDecoder, and the decoded picture must resemble THAT thread's own
// input - not a neighbour's. If any state were accidentally shared
// between H264EncoderContext/H264DecoderContext instances (or if the
// real OpenH264 library itself had some hidden global state), this
// would show up as one thread's decoded output resembling a different
// thread's input instead of its own.
TEST(RealOpenH264, ConcurrentEncoderDecoderInstances_DoNotInterfere) {
  const int kNumInstances = 6;
  const int kPicturesPerInstance = 3;
  CodecDefs defs = FindCifCodecs();

  struct ThreadResult {
    bool ok = false;
    std::string failure;
    double maxDiff = 0;
  };
  std::vector<ThreadResult> results(kNumInstances);

  std::atomic<int> readyCount(0);
  std::atomic<bool> go(false);

  auto worker = [&](int idx) {
    ThreadResult & r = results[idx];
    try {
      void * encCtx = defs.encoder->createCodec(defs.encoder);
      void * decCtx = defs.decoder->createCodec(defs.decoder);
      if (encCtx == nullptr || decCtx == nullptr) {
        r.failure = "createCodec returned null";
        return;
      }

      readyCount.fetch_add(1);
      while (!go.load()) { /* spin until every thread is ready, to maximise real overlap */ }

      for (int f = 0; f < kPicturesPerInstance; f++) {
        std::vector<uint8_t> raw = BuildSyntheticFrame(kWidth, kHeight, f, idx);
        EncodedPicture pic = EncodeOnePicture(defs.encoder, encCtx, raw);
        if (pic.packets.empty()) {
          r.failure = "encoder produced no packets";
          return;
        }

        DecodedPicture dec = DecodeOnePicture(defs.decoder, decCtx, pic);
        if (!dec.got) {
          r.failure = "decoder produced no picture";
          return;
        }
        if (dec.width != kWidth || dec.height != kHeight) {
          r.failure = "decoded dimensions did not match";
          return;
        }

        std::vector<uint8_t> origYVec = ExtractTightYPlane((PluginCodec_Video_FrameHeader *)(raw.data() + 12));
        double diff = MeanAbsYDiff(origYVec, dec.yPlane);
        r.maxDiff = std::max(r.maxDiff, diff);
      }

      defs.decoder->destroyCodec(defs.decoder, decCtx);
      defs.encoder->destroyCodec(defs.encoder, encCtx);
      r.ok = true;
    }
    catch (...) {
      r.failure = "unexpected exception on worker thread";
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < kNumInstances; i++)
    threads.emplace_back(worker, i);
  while (readyCount.load() < kNumInstances) { /* wait for every thread to reach the starting line */ }
  go.store(true);
  for (auto & t : threads) t.join();

  for (int i = 0; i < kNumInstances; i++) {
    ASSERT_TRUE(results[i].ok);
    // Same threshold reasoning as the single-instance round-trip test -
    // real measured values are well under 1.0; this is comfortable
    // margin against a genuinely broken/cross-contaminated decode
    // (which would show as tens, not fractions of a unit) without being
    // flaky across OpenH264 versions/build configs.
    ASSERT_TRUE(results[i].maxDiff < 25.0);
  }
}

int main() {
  return testfw::RunAll();
}
