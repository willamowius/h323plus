// Tests that feed invalid, malformed, or malicious-shaped data into the
// plugin and check it gets rejected safely - i.e. no crash, no memory
// corruption, no garbage output silently accepted as valid. This is the
// plugin's actual attack surface: H264Frame::SetFromRTPFrame() processes
// raw bytes taken directly off the network (from whatever peer is on the
// other end of the call, not necessarily well-behaved or friendly), and
// H264EncoderContext::EncodeFrames() trusts caller-supplied frame
// dimensions to compute buffer pointers.
//
// Several of these tests exist because writing them found real bugs:
// three separate out-of-bounds heap reads in H264Frame (an empty RTP
// payload, a truncated FU-A packet, and a malformed RTP CSRC/extension
// field making GetPayloadSize() wrap around to a huge value - all
// confirmed via AddressSanitizer before being fixed), and a missing
// bounds check in H264EncoderContext::EncodeFrames() where a caller
// claiming large frame dimensions with a too-small actual buffer would
// make the encoder compute pointers past the end of the real
// allocation. See h264frame.cxx/openh264.cxx and ReadMe.txt's
// "Known-fixed issues" for the details of each.
//
// The H264Frame-level tests here link the real h264frame.cxx directly,
// same as test_h264frame.cxx, and don't need the fake OpenH264 SDK. The
// encoder/decoder-ABI-level tests do (fake_wels/), same as
// test_plugin_abi.cxx.
#include "framework.h"
#include "fake_wels/fake_control.h"

#include "../h264frame.h"
#include <codec/opalplugin.h>
#ifdef _MSC_VER
#include "../../../common/rtpframe.h"
#else
#include "rtpframe.h"
#endif

#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>

extern "C" {
  struct PluginCodec_Definition * OpalCodecPlugin_GetCodecs(unsigned * count, unsigned version);
}

namespace {

typedef int (*ControlFn)(const struct PluginCodec_Definition *, void *, const char *, void *, unsigned *);

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

// A correctly-sized raw I420 frame, for tests that need a valid baseline
// to check recovery-after-rejection behaviour.
std::vector<uint8_t> BuildRawFrame(unsigned width, unsigned height) {
  size_t ySize = (size_t)width * height;
  size_t cSize = ySize / 4;
  std::vector<uint8_t> buf(12 + sizeof(PluginCodec_Video_FrameHeader) + ySize + 2 * cSize, 0);
  PluginCodec_Video_FrameHeader * hdr = (PluginCodec_Video_FrameHeader *)(buf.data() + 12);
  hdr->width = width;
  hdr->height = height;
  return buf;
}

// A minimally valid single-NAL IDR RTP packet, for tests that need a
// valid baseline. Payload byte 0 = NAL header (type 5 = IDR slice).
std::vector<uint8_t> BuildValidIdrPacket() {
  std::vector<uint8_t> buf(16, 0);
  buf[0] = 0x80;
  buf[1] = 0x80; // marker set
  buf[12] = 0x65;
  buf[13] = 0xAA; buf[14] = 0xBB; buf[15] = 0xCC;
  return buf;
}

} // namespace

// =======================================================================
// H264Frame::SetFromRTPFrame - malformed RTP payloads
// =======================================================================

TEST(InvalidInput, EmptyPayload_RejectedNotCrashed) {
  std::vector<uint8_t> buf(12, 0);
  buf[0] = 0x80; buf[1] = 0x80;
  RTPFrame rtp(buf.data(), (int)buf.size());
  ASSERT_EQ(rtp.GetPayloadSize(), 0u);
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, RegularNal_HeaderByteOnly_AcceptedAsZeroLengthNal) {
  // Degenerate but not unsafe: a NAL with a header and no payload bytes.
  std::vector<uint8_t> buf(13, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 0x65;
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_TRUE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, FUA_IndicatorByteOnly_RejectedNotCrashed) {
  std::vector<uint8_t> buf(13, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 0x1C; // FU-A indicator, type=28
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, FUA_IndicatorAndHeaderNoData_AcceptedAsZeroLengthNal) {
  std::vector<uint8_t> buf(14, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 0x1C; buf[13] = 0x85; // start bit, nal type 5
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_TRUE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, FUA_StartAndEndBothSet_Rejected) {
  // RFC 3984: S and E bits must never both be set.
  std::vector<uint8_t> buf(20, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 0x1C; buf[13] = 0xC5;
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, FUA_MiddleFragmentWithoutPrecedingStart_Rejected) {
  std::vector<uint8_t> buf(20, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 0x1C; buf[13] = 0x05; // neither S nor E
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, FUA_EndFragmentWithoutPrecedingStart_Rejected) {
  std::vector<uint8_t> buf(20, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 0x1C; buf[13] = 0x45; // E only
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, STAPA_TypeByteOnly_AcceptedAsEmpty) {
  std::vector<uint8_t> buf(13, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 24;
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_TRUE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, STAPA_IncompleteLengthField_Rejected) {
  std::vector<uint8_t> buf(14, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 24; buf[13] = 0x00; // only 1 of 2 length bytes present
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, STAPA_ClaimedLengthLargerThanPacket_Rejected) {
  std::vector<uint8_t> buf(20, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 24; buf[13] = 0xFF; buf[14] = 0xFF; // claims 65535 bytes
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, STAPA_ZeroLengthSubNal_Rejected) {
  std::vector<uint8_t> buf(20, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 24; buf[13] = 0x00; buf[14] = 0x00;
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, STAPA_ValidFirstNal_CorruptedSecondNal_Rejected) {
  std::vector<uint8_t> buf(30, 0);
  buf[0] = 0x80; buf[1] = 0x80; buf[12] = 24;
  buf[13] = 0x00; buf[14] = 0x03; buf[15] = 0x67; buf[16] = 0xAA; buf[17] = 0xBB; // valid: len=3
  buf[18] = 0xFF; buf[19] = 0xFF; // corrupted: huge bogus length
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, UnsupportedNalTypes_Rejected) {
  for (int t : {0, 13, 23, 29, 31}) {
    std::vector<uint8_t> buf(15, 0);
    buf[0] = 0x80; buf[1] = 0x80; buf[12] = (uint8_t)t;
    RTPFrame rtp(buf.data(), (int)buf.size());
    H264Frame dec;
    unsigned flags = 0;
    ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
  }
}

TEST(InvalidInput, MalformedCsrcCount_ImplausiblePayloadSize_Rejected) {
  // Byte 0's low nibble is the RTP CSRC count (0-15, each 4 bytes of
  // header). Claiming 15 CSRCs in a 20-byte packet makes the computed
  // header size (72) exceed the packet length - GetHeaderSize()/
  // GetPayloadSize() (common/rtpframe.h) can wrap around to a huge
  // unsigned value for this. Confirmed via AddressSanitizer to be an
  // out-of-bounds read before H264Frame validated against it.
  std::vector<uint8_t> buf(20, 0);
  buf[0] = 0x8F; // version 2, CSRC count 15
  buf[1] = 0x80;
  RTPFrame rtp(buf.data(), (int)buf.size());
  H264Frame dec;
  unsigned flags = 0;
  ASSERT_FALSE(dec.SetFromRTPFrame(rtp, flags));
}

TEST(InvalidInput, RandomFuzz_H264Frame_ManyIterationsNoCrash) {
  srand(0xC0FFEE);
  for (int i = 0; i < 20000; i++) {
    int len = 12 + (rand() % 60);
    std::vector<uint8_t> buf(len);
    for (auto & b : buf) b = (uint8_t)(rand() & 0xff);
    buf[1] |= 0x80; // bias toward the marker being set, to reach more code paths
    RTPFrame rtp(buf.data(), (int)buf.size());
    H264Frame dec;
    unsigned flags = 0;
    dec.SetFromRTPFrame(rtp, flags); // outcome doesn't matter, must not crash
  }
  ASSERT_TRUE(true); // reaching here at all is the assertion
}

// =======================================================================
// H264EncoderContext::EncodeFrames() - invalid raw-frame input, via the
// real plugin ABI (fake OpenH264 SDK - the fake doesn't care about frame
// content, so these tests specifically target the plugin's own input
// validation, not codec behaviour).
// =======================================================================

TEST(InvalidInput, Encoder_SrcLenZero_RejectedNotCrashed) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.encoder->createCodec(defs.encoder);

  std::vector<uint8_t> out(4096, 0);
  unsigned fromLen = 0;
  unsigned toLen = (unsigned)out.size();
  unsigned flags = 0;
  const uint8_t dummy = 0;
  defs.encoder->codecFunction(defs.encoder, ctx, &dummy, &fromLen, out.data(), &toLen, &flags);
  ASSERT_EQ(toLen, 0u);

  defs.encoder->destroyCodec(defs.encoder, ctx);
}

TEST(InvalidInput, Encoder_SrcLenOneByteShortOfMinimum_Rejected) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.encoder->createCodec(defs.encoder);

  std::vector<uint8_t> raw(12 + sizeof(PluginCodec_Video_FrameHeader) - 1, 0); // one byte short
  std::vector<uint8_t> out(4096, 0);
  unsigned fromLen = (unsigned)raw.size();
  unsigned toLen = (unsigned)out.size();
  unsigned flags = 0;
  defs.encoder->codecFunction(defs.encoder, ctx, raw.data(), &fromLen, out.data(), &toLen, &flags);
  ASSERT_EQ(toLen, 0u);
  ASSERT_EQ(fakewels::GetInitializeExtCallCount(), 0);

  defs.encoder->destroyCodec(defs.encoder, ctx);
}

TEST(InvalidInput, Encoder_HeightZero_WidthNonzero_Rejected) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.encoder->createCodec(defs.encoder);

  std::vector<uint8_t> raw = BuildRawFrame(352, 0);
  std::vector<uint8_t> out(4096, 0);
  unsigned fromLen = (unsigned)raw.size();
  unsigned toLen = (unsigned)out.size();
  unsigned flags = 0;
  defs.encoder->codecFunction(defs.encoder, ctx, raw.data(), &fromLen, out.data(), &toLen, &flags);
  ASSERT_EQ(toLen, 0u);
  ASSERT_EQ(fakewels::GetInitializeExtCallCount(), 0);

  defs.encoder->destroyCodec(defs.encoder, ctx);
}

TEST(InvalidInput, Encoder_WidthZero_HeightNonzero_Rejected) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.encoder->createCodec(defs.encoder);

  std::vector<uint8_t> raw = BuildRawFrame(0, 288);
  std::vector<uint8_t> out(4096, 0);
  unsigned fromLen = (unsigned)raw.size();
  unsigned toLen = (unsigned)out.size();
  unsigned flags = 0;
  defs.encoder->codecFunction(defs.encoder, ctx, raw.data(), &fromLen, out.data(), &toLen, &flags);
  ASSERT_EQ(toLen, 0u);
  ASSERT_EQ(fakewels::GetInitializeExtCallCount(), 0);

  defs.encoder->destroyCodec(defs.encoder, ctx);
}

// Regression test for the missing srcLen-vs-claimed-dimensions check:
// a header claiming a large picture backed by a much smaller actual
// buffer used to be trusted blindly, computing plane pointers past the
// end of the real allocation before ever calling the (real) encoder.
TEST(InvalidInput, Encoder_ClaimedDimensionsExceedActualBuffer_Rejected) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.encoder->createCodec(defs.encoder);

  // Header claims 1920x1080 but the buffer is only big enough for the
  // header itself - nowhere near 1920*1080*1.5 bytes of I420 data.
  std::vector<uint8_t> raw(12 + sizeof(PluginCodec_Video_FrameHeader), 0);
  PluginCodec_Video_FrameHeader * hdr = (PluginCodec_Video_FrameHeader *)(raw.data() + 12);
  hdr->width = 1920;
  hdr->height = 1080;

  std::vector<uint8_t> out(4096, 0);
  unsigned fromLen = (unsigned)raw.size();
  unsigned toLen = (unsigned)out.size();
  unsigned flags = 0;
  defs.encoder->codecFunction(defs.encoder, ctx, raw.data(), &fromLen, out.data(), &toLen, &flags);
  ASSERT_EQ(toLen, 0u);
  ASSERT_EQ(fakewels::GetInitializeExtCallCount(), 0); // must not have touched the encoder

  defs.encoder->destroyCodec(defs.encoder, ctx);
}

TEST(InvalidInput, Encoder_RecoversAfterRejectedFrames_ThenValidFrameWorks) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.encoder->createCodec(defs.encoder);
  unsigned flags = 0;

  // A handful of different invalid frames in a row.
  std::vector<std::vector<uint8_t>> invalidFrames;
  invalidFrames.push_back(std::vector<uint8_t>(5, 0)); // too short
  invalidFrames.push_back(BuildRawFrame(0, 0));         // 0x0
  {
    std::vector<uint8_t> tooClaimed(12 + sizeof(PluginCodec_Video_FrameHeader), 0);
    PluginCodec_Video_FrameHeader * hdr = (PluginCodec_Video_FrameHeader *)(tooClaimed.data() + 12);
    hdr->width = 4096; hdr->height = 4096;
    invalidFrames.push_back(tooClaimed);
  }
  for (auto & bad : invalidFrames) {
    std::vector<uint8_t> out(4096, 0);
    unsigned fromLen = (unsigned)bad.size();
    unsigned toLen = (unsigned)out.size();
    flags = 0;
    defs.encoder->codecFunction(defs.encoder, ctx, bad.data(), &fromLen, out.data(), &toLen, &flags);
    ASSERT_EQ(toLen, 0u);
  }

  // Now a valid frame - must still work normally afterward.
  std::vector<uint8_t> raw = BuildRawFrame(352, 288);
  std::vector<uint8_t> out(4096, 0);
  unsigned fromLen = (unsigned)raw.size();
  unsigned toLen = (unsigned)out.size();
  flags = 0;
  defs.encoder->codecFunction(defs.encoder, ctx, raw.data(), &fromLen, out.data(), &toLen, &flags);
  ASSERT_TRUE(toLen > 0);

  defs.encoder->destroyCodec(defs.encoder, ctx);
}

// =======================================================================
// H264DecoderContext::DecodeFrames() - invalid RTP input, via the real
// plugin ABI. This is the highest-value surface: genuine, untrusted
// network data.
// =======================================================================

TEST(InvalidInput, Decoder_SrcLenZero_NoCrashNoOutput) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.decoder->createCodec(defs.decoder);

  std::vector<uint8_t> out(200000, 0);
  unsigned fromLen = 0;
  unsigned toLen = (unsigned)out.size();
  unsigned flags = 0;
  const uint8_t dummy = 0;
  defs.decoder->codecFunction(defs.decoder, ctx, &dummy, &fromLen, out.data(), &toLen, &flags);
  ASSERT_EQ(toLen, 0u);

  defs.decoder->destroyCodec(defs.decoder, ctx);
}

TEST(InvalidInput, Decoder_GarbageBuffers_NoCrashNoOutput) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.decoder->createCodec(defs.decoder);

  srand(42);
  for (int i = 0; i < 200; i++) {
    int len = 1 + (rand() % 64);
    std::vector<uint8_t> in(len);
    for (auto & b : in) b = (uint8_t)(rand() & 0xff);
    std::vector<uint8_t> out(200000, 0);
    unsigned fromLen = (unsigned)in.size();
    unsigned toLen = (unsigned)out.size();
    unsigned flags = 0;
    defs.decoder->codecFunction(defs.decoder, ctx, in.data(), &fromLen, out.data(), &toLen, &flags);
    // Random bytes essentially never form a valid IDR-containing frame,
    // so no output is expected, but the point of this test is simply
    // that none of these 200 random buffers crash the decoder.
  }

  defs.decoder->destroyCodec(defs.decoder, ctx);
}

TEST(InvalidInput, Decoder_MarkerNeverSet_AccumulatesSafelyNoOutput) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.decoder->createCodec(defs.decoder);

  for (int i = 0; i < 50; i++) {
    std::vector<uint8_t> in = BuildValidIdrPacket();
    in[1] &= 0x7f; // clear marker bit - "picture never finishes"
    std::vector<uint8_t> out(200000, 0);
    unsigned fromLen = (unsigned)in.size();
    unsigned toLen = (unsigned)out.size();
    unsigned flags = 0;
    defs.decoder->codecFunction(defs.decoder, ctx, in.data(), &fromLen, out.data(), &toLen, &flags);
    ASSERT_EQ(toLen, 0u);
  }

  defs.decoder->destroyCodec(defs.decoder, ctx);
}

TEST(InvalidInput, Decoder_RandomFuzz_ThroughFullAbi_ManyIterationsNoCrash) {
  fakewels::ResetAllFakes();
  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.decoder->createCodec(defs.decoder);

  srand(1234567);
  for (int i = 0; i < 20000; i++) {
    int len = rand() % 80; // includes 0
    std::vector<uint8_t> in(len);
    for (auto & b : in) b = (uint8_t)(rand() & 0xff);
    std::vector<uint8_t> out(200000, 0);
    unsigned fromLen = (unsigned)in.size();
    unsigned toLen = (unsigned)out.size();
    unsigned flags = 0;
    const uint8_t * inPtr = in.empty() ? (const uint8_t *)"" : in.data();
    defs.decoder->codecFunction(defs.decoder, ctx, inPtr, &fromLen, out.data(), &toLen, &flags);
  }

  defs.decoder->destroyCodec(defs.decoder, ctx);
  ASSERT_TRUE(true); // reaching here at all is the assertion
}

TEST(InvalidInput, Decoder_RecoversAfterGarbage_ThenValidPacketDecodes) {
  fakewels::ResetAllFakes();
  fakewels::DecodeResult dr;
  dr.status = dsErrorFree;
  dr.hasPicture = true;
  dr.width = 352;
  dr.height = 288;
  fakewels::QueueDecodeResult(dr);

  CodecDefs defs = FindCifCodecs();
  void * ctx = defs.decoder->createCodec(defs.decoder);

  srand(99);
  for (int i = 0; i < 20; i++) {
    int len = rand() % 40;
    std::vector<uint8_t> in(len);
    for (auto & b : in) b = (uint8_t)(rand() & 0xff);
    std::vector<uint8_t> out(200000, 0);
    unsigned fromLen = (unsigned)in.size();
    unsigned toLen = (unsigned)out.size();
    unsigned flags = 0;
    const uint8_t * inPtr = in.empty() ? (const uint8_t *)"" : in.data();
    defs.decoder->codecFunction(defs.decoder, ctx, inPtr, &fromLen, out.data(), &toLen, &flags);
  }

  std::vector<uint8_t> good = BuildValidIdrPacket();
  std::vector<uint8_t> out(200000, 0);
  unsigned fromLen = (unsigned)good.size();
  unsigned toLen = (unsigned)out.size();
  unsigned flags = 0;
  defs.decoder->codecFunction(defs.decoder, ctx, good.data(), &fromLen, out.data(), &toLen, &flags);
  ASSERT_TRUE(toLen > 0);

  defs.decoder->destroyCodec(defs.decoder, ctx);
}

int main() {
  return testfw::RunAll();
}
