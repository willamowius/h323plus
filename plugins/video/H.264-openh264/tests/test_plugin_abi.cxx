// Tests that drive the plugin through its real, public PluginCodec ABI -
// OpalCodecPlugin_GetCodecs(), then createCodec()/codecFunction()/
// codecControls[] exactly as H323Plus's plugin loader (h323pluginmgr.cxx)
// does - rather than reaching into H264EncoderContext/H264DecoderContext
// internals. This links against the REAL, unmodified openh264.o and
// h264frame.o, with the fake OpenH264 SDK (fake_wels/) standing in for
// libopenh264 so behaviour is deterministic and no real codec is needed.
//
// This file exists mainly to hold regression tests for bugs found and
// fixed while bringing this plugin up: the encoder_event_handler()
// self-deadlock, the raw-frame RTP-header offset, the missing per-picture
// timestamp, the layer-flattening bug, and the payload-type clobber. Each
// test says which bug it guards against.
#include "framework.h"
#include "fake_wels/fake_control.h"

#include <codec/opalplugin.h>
#ifdef _MSC_VER
#include "../../../common/rtpframe.h"
#else
#include "rtpframe.h"
#endif

#include <vector>
#include <cstring>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <string>

// Declared via the PLUGIN_CODEC_IMPLEMENT(OpenH264) macro inside
// openh264.cxx's extern "C" block - no header declares this for external
// callers (a real plugin loader gets it via dlsym(), not a header), so we
// declare it ourselves to call it directly.
extern "C" {
  struct PluginCodec_Definition * OpalCodecPlugin_GetCodecs(unsigned * count, unsigned version);
}

namespace {

// ---- locating the plugin's codec definitions ----

struct CodecDefs {
  const PluginCodec_Definition * encoder = nullptr;
  const PluginCodec_Definition * decoder = nullptr;
};

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

typedef int (*ControlFn)(const struct PluginCodec_Definition *, void *, const char *, void *, unsigned *);

ControlFn FindControl(const PluginCodec_Definition * def, const char * name) {
  for (PluginCodec_ControlDefn * c = def->codecControls; c != nullptr && c->name != nullptr; c++) {
    if (strcmp(c->name, name) == 0)
      return c->control;
  }
  return nullptr;
}

// ---- building synthetic raw I420 input frames ----

// EncodeFrames() expects its raw-frame input wrapped in a minimal
// 12-byte pseudo-RTP header (PluginCodec_RTP_MinHeaderSize) before the
// PluginCodec_Video_FrameHeader + I420 payload - this is the real OPAL/
// H323Plus wire convention confirmed empirically while bringing this
// plugin up (see the "0x0 encoder init" bug in ReadMe.txt/git history).
// All-zero bytes here give GetHeaderSize()==12 (no CSRC, no extension).
std::vector<uint8_t> BuildRawFrame(unsigned width, unsigned height) {
  size_t ySize = (size_t)width * height;
  size_t cSize = ySize / 4;
  std::vector<uint8_t> buf(12 + sizeof(PluginCodec_Video_FrameHeader) + ySize + 2 * cSize, 0);
  PluginCodec_Video_FrameHeader * hdr = (PluginCodec_Video_FrameHeader *)(buf.data() + 12);
  hdr->x = 0;
  hdr->y = 0;
  hdr->width = width;
  hdr->height = height;
  return buf;
}

// ---- calling with a timeout guard (see CallWithTimeout below) ----

// Calls fn on a background thread and fails (rather than hanging forever)
// if it doesn't return within timeoutMs. This is the only way to safely
// write a regression test for a deadlock: a plain synchronous call would
// hang the whole test binary forever if the bug ever reappears. On
// timeout the worker thread is detached (it will stay blocked forever,
// same as the real bug) so the rest of the suite can still run and the
// process can still exit.
bool CallWithTimeout(std::function<void()> fn, int timeoutMs) {
  auto done = std::make_shared<std::atomic<bool>>(false);
  std::thread worker([fn, done]() {
    fn();
    done->store(true);
  });
  auto start = std::chrono::steady_clock::now();
  while (!done->load()) {
    if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(timeoutMs)) {
      worker.detach();
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  worker.join();
  return true;
}

} // namespace

TEST(PluginABI, GetCodecs_FindsCifEncoderAndDecoder) {
  CodecDefs defs = FindCifCodecs();
  ASSERT_TRUE(defs.encoder != nullptr);
  ASSERT_TRUE(defs.decoder != nullptr);
  ASSERT_TRUE(defs.encoder->createCodec != nullptr);
  ASSERT_TRUE(defs.encoder->codecFunction != nullptr);
}

// Regression test for the encoder_event_handler() self-deadlock: it used
// to call context->Lock() and then FastUpdateRequested(), which locks the
// same (non-reentrant, POSIX semaphore based) mutex again from the same
// thread - hanging forever the first time a fast-update event arrived,
// and wedging every subsequent EncodeFrames() call for that channel too
// since it opens with the same lock. See openh264.cxx's
// encoder_event_handler() and ReadMe.txt for the full story.
TEST(PluginABI, FastUpdateEvent_DoesNotDeadlock_AndEncoderStillWorksAfterward) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  ASSERT_TRUE(defs.encoder != nullptr);

  void * context = defs.encoder->createCodec(defs.encoder);
  ASSERT_TRUE(context != nullptr);

  ControlFn eventFn = FindControl(defs.encoder, PLUGINCODEC_CONTROL_CODEC_EVENT);
  ASSERT_TRUE(eventFn != nullptr);

  const char * parm[3] = { PLUGINCODEC_EVENT_FASTUPDATE, "1", nullptr };
  unsigned parmLen = sizeof(const char **);

  bool returned = CallWithTimeout([&]() {
    eventFn(defs.encoder, context, PLUGINCODEC_CONTROL_CODEC_EVENT, (void *)parm, &parmLen);
  }, 2000);
  ASSERT_TRUE(returned); // <-- this is what used to hang forever

  // And the encoder must still be usable afterward - the deadlock, when
  // present, wedged every subsequent EncodeFrames() call too because it
  // opens with the same WaitAndSignal(_mutex).
  std::vector<uint8_t> raw = BuildRawFrame(352, 288);
  std::vector<uint8_t> out(4096, 0);
  unsigned fromLen = (unsigned)raw.size();
  unsigned toLen = (unsigned)out.size();
  unsigned flags = 0;

  bool encodeReturned = CallWithTimeout([&]() {
    defs.encoder->codecFunction(defs.encoder, context, raw.data(), &fromLen, out.data(), &toLen, &flags);
  }, 2000);
  ASSERT_TRUE(encodeReturned);
  ASSERT_TRUE(toLen > 0);

  defs.encoder->destroyCodec(defs.encoder, context);
}

// Regression test for the "0x0 raw frame -> ParamValidationExt failure"
// bug: EncodeFrames() must reject a zero-dimension frame without ever
// calling into the encoder, and must recover cleanly once a real frame
// arrives afterward.
TEST(PluginABI, ZeroDimensionFrame_IsIgnored_ThenRecoversOnRealFrame) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * context = defs.encoder->createCodec(defs.encoder);

  std::vector<uint8_t> zero = BuildRawFrame(0, 0);
  std::vector<uint8_t> out(4096, 0);
  unsigned fromLen = (unsigned)zero.size();
  unsigned toLen = (unsigned)out.size();
  unsigned flags = 0;
  defs.encoder->codecFunction(defs.encoder, context, zero.data(), &fromLen, out.data(), &toLen, &flags);
  ASSERT_EQ(toLen, 0u);
  ASSERT_EQ(fakewels::GetInitializeExtCallCount(), 0); // must not have touched the encoder

  std::vector<uint8_t> real = BuildRawFrame(352, 288);
  fromLen = (unsigned)real.size();
  toLen = (unsigned)out.size();
  flags = 0;
  defs.encoder->codecFunction(defs.encoder, context, real.data(), &fromLen, out.data(), &toLen, &flags);
  ASSERT_TRUE(toLen > 0);
  ASSERT_TRUE(fakewels::GetInitializeExtCallCount() > 0);

  defs.encoder->destroyCodec(defs.encoder, context);
}

// Regression test for the layer-flattening bug: OpenH264 commonly splits
// one encoded picture across multiple SLayerInfo entries (e.g. SPS/PPS in
// one layer, the coded slice in another) even with SVC scalability fully
// disabled. EncodeFrames() must collect NALs from every layer, not just
// layer 0.
TEST(PluginABI, MultiLayerEncodeResult_AllNalsReachOutput) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * context = defs.encoder->createCodec(defs.encoder);

  fakewels::EncodeResult r;
  fakewels::Layer paramLayer, sliceLayer;
  paramLayer.nals.push_back({ {0,0,0,1, 0x67, 0x42,0xC0,0x1E} });        // SPS
  paramLayer.nals.push_back({ {0,0,0,1, 0x68, 0xCE,0x3C,0x80} });        // PPS
  sliceLayer.nals.push_back({ {0,0,0,1, 0x65, 0xAA,0xBB,0xCC,0xDD} });   // IDR slice
  r.layers = { paramLayer, sliceLayer };
  fakewels::QueueEncodeResult(r);

  std::vector<uint8_t> raw = BuildRawFrame(352, 288);
  unsigned fromLen = (unsigned)raw.size();
  unsigned flags = 0;

  int packetsWithData = 0;
  bool sawLastFrame = false;
  for (int i = 0; i < 10 && !sawLastFrame; i++) {
    std::vector<uint8_t> out(4096, 0);
    unsigned toLen = (unsigned)out.size();
    fromLen = (unsigned)raw.size(); // some plugin ABIs re-read fromLen each call
    defs.encoder->codecFunction(defs.encoder, context, raw.data(), &fromLen, out.data(), &toLen, &flags);
    if (toLen > 0) packetsWithData++;
    if (flags & PluginCodec_ReturnCoderLastFrame) sawLastFrame = true;
  }

  ASSERT_TRUE(sawLastFrame);
  // Three NALs (SPS, PPS, IDR slice), all small enough to go out as
  // individual "regular NAL unit" packets -> three packets. If layer 1
  // (the slice) were dropped, as the original bug did, this would be 2.
  ASSERT_EQ(packetsWithData, 3);

  defs.encoder->destroyCodec(defs.encoder, context);
}

// Regression test for the missing-timestamp bug: every RTP packet used to
// go out with timestamp 0 because EncodeFrames() never called
// H264Frame::SetTimestamp(). Consecutive pictures must get increasing
// timestamps, advancing by one frame period each time.
TEST(PluginABI, ConsecutivePictures_GetIncreasingTimestamps) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * context = defs.encoder->createCodec(defs.encoder);

  std::vector<uint8_t> raw = BuildRawFrame(352, 288);
  unsigned long lastTimestampOfPicture[2] = {0, 0};

  for (int pic = 0; pic < 2; pic++) {
    unsigned flags = 0;
    bool sawLastFrame = false;
    for (int i = 0; i < 10 && !sawLastFrame; i++) {
      std::vector<uint8_t> out(4096, 0);
      unsigned fromLen = (unsigned)raw.size();
      unsigned toLen = (unsigned)out.size();
      defs.encoder->codecFunction(defs.encoder, context, raw.data(), &fromLen, out.data(), &toLen, &flags);
      if (toLen > 0) {
        RTPFrame rtp(out.data(), (int)toLen);
        lastTimestampOfPicture[pic] = rtp.GetTimestamp();
      }
      if (flags & PluginCodec_ReturnCoderLastFrame) sawLastFrame = true;
    }
  }

  ASSERT_TRUE(lastTimestampOfPicture[1] > lastTimestampOfPicture[0]);

  defs.encoder->destroyCodec(defs.encoder, context);
}

// Regression test for the payload-type-clobber bug: EncodeFrames() used
// to construct its output via RTPFrame's 3-argument constructor, which
// reinitialises byte 0 and overwrites the payload type with a hardcoded
// value - stomping whatever dynamic payload type the RTP session had
// already written into the buffer before handing it to the codec. It
// must use the 2-argument constructor and leave pre-existing header
// bytes (here: payload type and sequence number) alone.
TEST(PluginABI, PreExistingRtpHeaderFields_AreNotClobbered) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * context = defs.encoder->createCodec(defs.encoder);

  std::vector<uint8_t> raw = BuildRawFrame(352, 288);
  unsigned fromLen = (unsigned)raw.size();
  unsigned flags = 0;

  std::vector<uint8_t> out(4096, 0);
  // Simulate what the real RTP session does before calling the codec:
  // pre-populate a valid header with a specific dynamic payload type and
  // sequence number.
  const unsigned char kPayloadType = 96;
  const unsigned kSeqNo = 0xBEEF & 0x7fff;
  out[0] = 0x80;
  RTPFrame preset(out.data(), (int)out.size(), kPayloadType);
  out[2] = (unsigned char)(kSeqNo >> 8);
  out[3] = (unsigned char)(kSeqNo & 0xff);

  unsigned toLen = (unsigned)out.size();
  defs.encoder->codecFunction(defs.encoder, context, raw.data(), &fromLen, out.data(), &toLen, &flags);
  ASSERT_TRUE(toLen > 0);

  RTPFrame result(out.data(), (int)toLen);
  ASSERT_EQ((unsigned)(out[1] & 0x7f), (unsigned)kPayloadType); // RTPFrame has no payload-type getter; read the byte per its own SetPayloadType() bit layout
  ASSERT_EQ(result.GetSequenceNumber(), kSeqNo);

  defs.encoder->destroyCodec(defs.encoder, context);
}

// Multiple encoder/decoder instances run concurrently (one thread per
// instance, matching how H323Plus actually uses this plugin - one
// H264EncoderContext/H264DecoderContext per media stream, each driven
// from its own thread) and must not interfere with each other's state.
// This is only meaningful because fake_openh264.cxx's control state is
// thread_local (see its file header comment) - it used to be a handful
// of plain shared globals/statics, which would have made this test
// either flaky or actively wrong (one thread's queued decode result
// could be consumed by a different thread's decoder). Each thread here
// queues a result distinguishable from every other thread's, so if
// thread_local isolation were ever broken again, this test would catch
// threads reading back a NEIGHBOUR's queued width/height instead of
// their own, not just "some" plausible-looking value.
TEST(PluginABI, ConcurrentEncoderDecoderInstances_DoNotInterfere) {
  const int kNumInstances = 6;
  CodecDefs defs = FindCifCodecs();

  struct ThreadResult {
    bool ok = false;
    std::string failure;
    unsigned expectedWidth = 0, expectedHeight = 0;
    unsigned decodedWidth = 0, decodedHeight = 0;
    std::vector<unsigned long> pictureTimestamps;
    int encoderCreateCountSeenByThisThread = -1;
  };
  std::vector<ThreadResult> results(kNumInstances);

  std::atomic<int> readyCount(0);
  std::atomic<bool> go(false);

  auto worker = [&](int idx) {
    ThreadResult & r = results[idx];
    try {
      fakewels::ResetAllFakes(); // this thread's own thread_local state only

      // A distinct "picture" size per thread, queued for THIS thread's
      // decoder only - if isolation ever breaks, some thread will read
      // back a size that isn't its own.
      r.expectedWidth = 160 + (unsigned)idx * 16;
      r.expectedHeight = 120 + (unsigned)idx * 16;
      fakewels::DecodeResult dr;
      dr.status = dsErrorFree;
      dr.hasPicture = true;
      dr.width = r.expectedWidth;
      dr.height = r.expectedHeight;
      fakewels::QueueDecodeResult(dr);

      void * encCtx = defs.encoder->createCodec(defs.encoder);
      void * decCtx = defs.decoder->createCodec(defs.decoder);
      if (encCtx == nullptr || decCtx == nullptr) {
        r.failure = "createCodec returned null";
        return;
      }

      readyCount.fetch_add(1);
      while (!go.load()) { /* spin until every thread is ready, to maximise real overlap */ }

      // Encode a few pictures on this thread, capturing this instance's
      // own RTP timestamp for each - must be exactly one frame period
      // apart, regardless of what the other 5 threads are doing right
      // now on other H264EncoderContext instances.
      const int numPictures = 3;
      for (int p = 0; p < numPictures; p++) {
        std::vector<uint8_t> raw = BuildRawFrame(352, 288);
        unsigned flags = 0;
        bool sawLastFrame = false;
        for (int i = 0; i < 20 && !sawLastFrame; i++) {
          std::vector<uint8_t> out(4096, 0);
          unsigned fromLen = (unsigned)raw.size();
          unsigned toLen = (unsigned)out.size();
          defs.encoder->codecFunction(defs.encoder, encCtx, raw.data(), &fromLen, out.data(), &toLen, &flags);
          if (toLen > 0 && (flags & PluginCodec_ReturnCoderLastFrame)) {
            RTPFrame rtp(out.data(), (int)toLen);
            r.pictureTimestamps.push_back(rtp.GetTimestamp());
            sawLastFrame = true;
          }
        }
        if (!sawLastFrame) {
          r.failure = "never saw PluginCodec_ReturnCoderLastFrame while encoding";
          return;
        }
      }

      // Decode one picture on this thread and confirm we got back
      // exactly what THIS thread queued, not a neighbour's. Needs a
      // minimally valid single-NAL RTP packet (marker set, an IDR NAL
      // header) to get past SetFromRTPFrame()/IsSync() - the fake
      // decoder's canned output doesn't depend on the actual NAL bytes,
      // but the plugin's own pre-decode validity checks still apply.
      std::vector<uint8_t> encIn(16, 0);
      encIn[0] = 0x80;
      encIn[1] = 0x80; // marker bit set
      encIn[12] = 0x65; // NAL header: nal_ref_idc=3, nal_unit_type=5 (IDR slice)
      encIn[13] = 0xAA; encIn[14] = 0xBB; encIn[15] = 0xCC;
      std::vector<uint8_t> decOut(200000, 0); // must hold a full raw YUV frame, not an RTP-sized packet
      unsigned fromLen = (unsigned)encIn.size();
      unsigned toLen = (unsigned)decOut.size();
      unsigned decFlags = 0;
      defs.decoder->codecFunction(defs.decoder, decCtx, encIn.data(), &fromLen, decOut.data(), &toLen, &decFlags);
      if (toLen == 0) {
        r.failure = "decoder produced no output";
        return;
      }
      RTPFrame decRtp(decOut.data(), (int)toLen);
      PluginCodec_Video_FrameHeader * hdr = (PluginCodec_Video_FrameHeader *)decRtp.GetPayloadPtr();
      r.decodedWidth = hdr->width;
      r.decodedHeight = hdr->height;

      r.encoderCreateCountSeenByThisThread = fakewels::GetEncoderCreateCount();

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
    const ThreadResult & r = results[i];
    ASSERT_TRUE(r.ok);

    // Encoder-side isolation: this instance's own timestamp sequence,
    // unaffected by the other 5 instances encoding concurrently.
    ASSERT_EQ(r.pictureTimestamps.size(), (size_t)3);
    ASSERT_EQ(r.pictureTimestamps[0], 0ul);
    unsigned long step = r.pictureTimestamps[1] - r.pictureTimestamps[0];
    ASSERT_TRUE(step > 0);
    ASSERT_EQ(r.pictureTimestamps[2] - r.pictureTimestamps[1], step);

    // Decoder-side isolation: got back exactly what THIS thread queued.
    ASSERT_EQ(r.decodedWidth, r.expectedWidth);
    ASSERT_EQ(r.decodedHeight, r.expectedHeight);

    // Sanity: this thread's own thread_local create-count is 1, not
    // some aggregate across all 6 concurrently-running instances.
    ASSERT_EQ(r.encoderCreateCountSeenByThisThread, 1);
  }
}

int main() {
  return testfw::RunAll();
}
