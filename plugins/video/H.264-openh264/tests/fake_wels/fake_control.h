// Test-only control API for the fake OpenH264 SDK implemented in
// fake_openh264.cxx. Production code (openh264.cxx) never includes this -
// it only ever sees wels/codec_api.h and wels/codec_ver.h, exactly like a
// real build against libopenh264. This header is how test code configures
// and inspects the fake encoder/decoder that WelsCreateSVCEncoder() etc.
// hand out, so tests can drive H264EncoderContext/H264DecoderContext
// through the plugin's real public ABI without linking the real SDK.
//
// All state here is thread_local (see fake_openh264.cxx): queue a result
// on the same thread that will create and use that encoder/decoder
// instance, before creating it. This lets multiple instances run
// concurrently, one per thread, with independently queued results/call
// counts - matching how this plugin is actually used (one
// H264EncoderContext/H264DecoderContext per media stream, each driven
// from its own thread) - but a single instance still isn't safe to call
// from more than one thread at a time.
#ifndef FAKE_OPENH264_CONTROL_H
#define FAKE_OPENH264_CONTROL_H

#include <vector>
#include <cstdint>
#include "wels/codec_api.h"

#ifndef FAKEWELS_CODEC_API_H_INCLUDED
#error "wels/codec_api.h resolved to something other than tests/fake_wels/wels/codec_api.h - check that tests/fake_wels/wels/{codec_api.h,codec_ver.h} exist on disk (a common cause is an incomplete copy of the tests/ directory, which is nested two levels deep) and that -Ifake_wels is being passed and isn't overridden by another -I/-isystem or a CPATH-style environment variable pointing at a real libopenh264-dev install."
#endif

namespace fakewels {

// One NAL unit, complete with its Annex-B start code, exactly as
// SLayerBSInfo::pBsBuf/pNalLengthInByte represent it.
struct Nal {
  std::vector<uint8_t> bytes; // includes the 00 00 00 01 (or 00 00 01) prefix
};

// One SLayerBSInfo's worth of NALs - i.e. what one EncodeFrame() call can
// report as a single "layer". A queued EncodeResult can have more than one
// of these, which is exactly the OpenH264 behaviour (SPS/PPS as one layer,
// coded slice as another) that the layer-flattening bug was about.
struct Layer {
  std::vector<Nal> nals;
};

struct EncodeResult {
  std::vector<Layer> layers;
  EVideoFrameType frameType = videoFrameTypeIDR;
  CM_RETURN cmResult = cmResultSuccess;
};

// Queue results to be returned by successive ISVCEncoder::EncodeFrame()
// calls, in order. If the queue is empty when EncodeFrame() is called, a
// default single-NAL "IDR" result is returned so tests that don't care
// about exact bitstream content still get sensible behaviour.
void QueueEncodeResult(const EncodeResult & r);
void ResetEncoderFake();

int  GetEncoderCreateCount();
int  GetInitializeExtCallCount();
int  GetEncodeFrameCallCount();
int  GetForceIntraFrameCallCount();
bool WasEncoderUninitialized(); // true if Uninitialize() was ever called

// Snapshot of the SEncParamExt passed to the most recent InitializeExt()
// call, for asserting what ApplyOptions() actually configured.
struct LastInitParams {
  bool     valid = false;
  int      picWidth = 0, picHeight = 0;
  int      targetBitrate = 0;
  float    maxFrameRate = 0;
  unsigned intraPeriod = 0;
  unsigned maxNalSize = 0;
  SliceModeEnum sliceMode = SM_SINGLE_SLICE;
  EProfileIdc profileIdc = PRO_UNKNOWN;
  ELevelIdc levelIdc = LEVEL_UNKNOWN;
};
LastInitParams GetLastInitParams();

// Dimensions passed to the most recent EncodeFrame() call's SSourcePicture,
// for asserting what raw frame the encoder actually saw.
struct LastEncodeInput {
  bool valid = false;
  int  picWidth = 0, picHeight = 0;
};
LastEncodeInput GetLastEncodeInput();

// ---- decoder side ----

struct DecodeResult {
  DECODING_STATE status = dsErrorFree;
  bool hasPicture = false;     // iBufferStatus in the returned SBufferInfo
  int width = 0, height = 0;   // only used if hasPicture
};

// Queue results to be returned by successive
// ISVCDecoder::DecodeFrameNoDelay() calls, in order. Empty queue -> a
// default dsErrorFree/no-picture result (matches OpenH264 buffering a
// frame internally before the first picture comes out).
void QueueDecodeResult(const DecodeResult & r);
void ResetDecoderFake();

int  GetDecoderCreateCount();
int  GetDecodeFrameCallCount();
bool WasDecoderUninitialized();

// The exact bytes handed to the most recent DecodeFrameNoDelay() call -
// i.e. what H264Frame::GetFramePtr()/GetFrameSize() actually assembled.
// Lets tests check NAL ordering/reassembly reached the decoder correctly.
std::vector<uint8_t> GetLastDecodeInput();

void ResetAllFakes();

} // namespace fakewels

#endif // FAKE_OPENH264_CONTROL_H
