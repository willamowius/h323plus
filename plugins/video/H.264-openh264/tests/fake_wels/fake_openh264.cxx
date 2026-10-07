// Fake implementation of the OpenH264 SDK surface (wels/codec_api.h,
// wels/codec_ver.h) that openh264.cxx is compiled and linked against for
// testing, in place of the real libopenh264. This lets tests drive
// H264EncoderContext/H264DecoderContext (via the plugin's real public ABI
// - OpalCodecPlugin_GetCodecs()) deterministically, without needing the
// real SDK installed, and without modifying any production code.
//
// Behaviour is configured through fake_control.h's QueueEncodeResult() /
// QueueDecodeResult() etc. All of the mutable state below is
// thread_local, specifically so that multiple encoder/decoder instances
// can run concurrently, one per thread, without interfering with each
// other's queued results, call counts, or last-seen snapshots - matching
// how this plugin is actually used (one H264EncoderContext/
// H264DecoderContext per media stream, each driven from its own thread).
// Queue a result on the SAME thread that will create/use that instance,
// before creating it. This does NOT make a single instance itself safe
// to call from multiple threads concurrently - nothing in this plugin
// claims that, and it's not how H323Plus uses it.
#include "fake_control.h"
#include "wels/codec_ver.h"

#include <cstring>
#include <deque>

namespace fakewels {
namespace {

thread_local int g_encoderCreateCount = 0;
thread_local int g_initializeExtCallCount = 0;
thread_local int g_encodeFrameCallCount = 0;
thread_local int g_forceIntraFrameCallCount = 0;
thread_local bool g_encoderUninitialized = false;
thread_local std::deque<EncodeResult> g_encodeQueue;
thread_local LastInitParams g_lastInitParams;
thread_local LastEncodeInput g_lastEncodeInput;

thread_local int g_decoderCreateCount = 0;
thread_local int g_decodeFrameCallCount = 0;
thread_local bool g_decoderUninitialized = false;
thread_local std::deque<DecodeResult> g_decodeQueue;
thread_local std::vector<uint8_t> g_lastDecodeInput;

// Backing storage for the NAL bytes/plane data a fake call hands back via
// raw pointers (SLayerBSInfo::pBsBuf, decoder output planes) - these must
// stay valid until at least the next call, exactly like the real SDK's
// internally-owned buffers, so we keep them alive here (thread_local for
// the same cross-instance-isolation reason as everything else in this
// file - see the file header comment).
struct EncodedLayerStorage {
  std::vector<uint8_t> concatenated;
  std::vector<int> nalLengths;
};

} // anonymous namespace

void ResetEncoderFake() {
  g_encoderCreateCount = 0;
  g_initializeExtCallCount = 0;
  g_encodeFrameCallCount = 0;
  g_forceIntraFrameCallCount = 0;
  g_encoderUninitialized = false;
  g_encodeQueue.clear();
  g_lastInitParams = LastInitParams();
  g_lastEncodeInput = LastEncodeInput();
}

void QueueEncodeResult(const EncodeResult & r) { g_encodeQueue.push_back(r); }
int  GetEncoderCreateCount() { return g_encoderCreateCount; }
int  GetInitializeExtCallCount() { return g_initializeExtCallCount; }
int  GetEncodeFrameCallCount() { return g_encodeFrameCallCount; }
int  GetForceIntraFrameCallCount() { return g_forceIntraFrameCallCount; }
bool WasEncoderUninitialized() { return g_encoderUninitialized; }
LastInitParams GetLastInitParams() { return g_lastInitParams; }
LastEncodeInput GetLastEncodeInput() { return g_lastEncodeInput; }

void ResetDecoderFake() {
  g_decoderCreateCount = 0;
  g_decodeFrameCallCount = 0;
  g_decoderUninitialized = false;
  g_decodeQueue.clear();
  g_lastDecodeInput.clear();
}

void QueueDecodeResult(const DecodeResult & r) { g_decodeQueue.push_back(r); }
int  GetDecoderCreateCount() { return g_decoderCreateCount; }
int  GetDecodeFrameCallCount() { return g_decodeFrameCallCount; }
bool WasDecoderUninitialized() { return g_decoderUninitialized; }
std::vector<uint8_t> GetLastDecodeInput() { return g_lastDecodeInput; }

void ResetAllFakes() { ResetEncoderFake(); ResetDecoderFake(); }

} // namespace fakewels

// ---------------------------------------------------------------------
// ISVCEncoder fake
// ---------------------------------------------------------------------

namespace {

class FakeSVCEncoder : public ISVCEncoder {
public:
  int SetOption(ENCODER_OPTION, void*) override { return 0; }

  int GetDefaultParams(SEncParamExt * p) override {
    memset(p, 0, sizeof(*p));
    return 0;
  }

  int InitializeExt(SEncParamExt * p) override {
    using namespace fakewels;
    g_initializeExtCallCount++;
    LastInitParams snap;
    snap.valid = true;
    snap.picWidth = p->iPicWidth;
    snap.picHeight = p->iPicHeight;
    snap.targetBitrate = p->iTargetBitrate;
    snap.maxFrameRate = p->fMaxFrameRate;
    snap.intraPeriod = p->uiIntraPeriod;
    snap.maxNalSize = p->uiMaxNalSize;
    snap.sliceMode = p->sSpatialLayers[0].sSliceArgument.uiSliceMode;
    snap.profileIdc = p->sSpatialLayers[0].uiProfileIdc;
    snap.levelIdc = p->sSpatialLayers[0].uiLevelIdc;
    g_lastInitParams = snap;
    return 0;
  }

  int Uninitialize() override {
    fakewels::g_encoderUninitialized = true;
    return 0;
  }

  int ForceIntraFrame(bool) override {
    fakewels::g_forceIntraFrameCallCount++;
    return 0;
  }

  int EncodeFrame(const SSourcePicture * pic, SFrameBSInfo * info) override {
    using namespace fakewels;
    g_encodeFrameCallCount++;

    LastEncodeInput in;
    in.valid = true;
    in.picWidth = pic->iPicWidth;
    in.picHeight = pic->iPicHeight;
    g_lastEncodeInput = in;

    EncodeResult result;
    if (!g_encodeQueue.empty()) {
      result = g_encodeQueue.front();
      g_encodeQueue.pop_front();
    }
    else {
      // Default: a single tiny "IDR" NAL, so tests that don't care about
      // exact bitstream content still see plausible success behaviour.
      Nal nal;
      nal.bytes = { 0,0,0,1, 0x65, 0xAA, 0xBB };
      Layer layer;
      layer.nals.push_back(nal);
      result.layers.push_back(layer);
    }

    memset(info, 0, sizeof(*info));
    info->eFrameType = result.frameType;
    if (result.cmResult != cmResultSuccess)
      return result.cmResult;

    int layerCount = (int)result.layers.size();
    if (layerCount > 4) layerCount = 4; // SFrameBSInfo::sLayerInfo[4]
    info->iLayerNum = layerCount;

    // Keep this call's encoded bytes alive until at least the next
    // EncodeFrame() call, matching the real SDK's buffer lifetime.
    // thread_local (not a per-object member) is deliberate and safe here:
    // this class has no identity beyond "the fake encoder for whichever
    // thread created it" (see the file header comment), and a plain
    // per-object member would work just as well for that same usage
    // pattern - thread_local was chosen so this storage follows the same
    // cross-instance-isolation reasoning as the rest of this file.
    thread_local std::vector<EncodedLayerStorage> storage; // indices match sLayerInfo[]
    storage.assign(layerCount, EncodedLayerStorage());

    for (int l = 0; l < layerCount; l++) {
      const Layer & srcLayer = result.layers[l];
      EncodedLayerStorage & dst = storage[l];
      for (auto & nal : srcLayer.nals) {
        dst.concatenated.insert(dst.concatenated.end(), nal.bytes.begin(), nal.bytes.end());
        dst.nalLengths.push_back((int)nal.bytes.size());
      }
      info->sLayerInfo[l].pBsBuf = dst.concatenated.empty() ? nullptr : dst.concatenated.data();
      info->sLayerInfo[l].iNalCount = (int)dst.nalLengths.size();
      int nalCountForCopy = (int)dst.nalLengths.size();
      if (nalCountForCopy > 128) nalCountForCopy = 128; // SLayerBSInfo::pNalLengthInByte[128]
      for (int n = 0; n < nalCountForCopy; n++)
        info->sLayerInfo[l].pNalLengthInByte[n] = dst.nalLengths[n];
    }

    return cmResultSuccess;
  }
};

class FakeSVCDecoder : public ISVCDecoder {
public:
  int SetOption(DECODER_OPTION, void*) override { return 0; }
  long GetOption(DECODER_OPTION, void*) override { return 0; }

  long Initialize(const SDecodingParam *) override { return 0; }

  int Uninitialize() override {
    fakewels::g_decoderUninitialized = true;
    return 0;
  }

  DECODING_STATE DecodeFrameNoDelay(const unsigned char * buf, int len,
                                     unsigned char ** dst, SBufferInfo * info) override {
    using namespace fakewels;
    g_decodeFrameCallCount++;
    g_lastDecodeInput.assign(buf, buf + len);

    DecodeResult result;
    if (!g_decodeQueue.empty()) {
      result = g_decodeQueue.front();
      g_decodeQueue.pop_front();
    }
    // else: default dsErrorFree / no picture (buffering) - matches a
    // fresh FakeSVCDecoder's DecodeResult{} default member values.

    memset(info, 0, sizeof(*info));
    if (result.hasPicture) {
      info->iBufferStatus = 1;
      info->UsrData.sSystemBuffer.iWidth = result.width;
      info->UsrData.sSystemBuffer.iHeight = result.height;
      info->UsrData.sSystemBuffer.iStride[0] = result.width;
      info->UsrData.sSystemBuffer.iStride[1] = result.width / 2;

      // thread_local for the same reason as 'storage' in EncodeFrame()
      // above - see that comment.
      thread_local std::vector<uint8_t> yPlane, uPlane, vPlane;
      size_t ySize = (size_t)result.width * result.height;
      size_t cSize = ySize / 4;
      yPlane.assign(ySize, 0x10);
      uPlane.assign(cSize, 0x80);
      vPlane.assign(cSize, 0x80);
      dst[0] = yPlane.data();
      dst[1] = uPlane.data();
      dst[2] = vPlane.data();
    }
    else {
      info->iBufferStatus = 0;
    }

    return result.status;
  }
};

} // anonymous namespace

int WelsCreateSVCEncoder(ISVCEncoder ** ppEncoder) {
  fakewels::g_encoderCreateCount++;
  *ppEncoder = new FakeSVCEncoder();
  return 0;
}

void WelsDestroySVCEncoder(ISVCEncoder * encoder) {
  delete encoder;
}

long WelsCreateDecoder(ISVCDecoder ** ppDecoder) {
  fakewels::g_decoderCreateCount++;
  *ppDecoder = new FakeSVCDecoder();
  return 0;
}

void WelsDestroyDecoder(ISVCDecoder * decoder) {
  delete decoder;
}

void WelsGetCodecVersionEx(OpenH264Version * v) {
  memset(v, 0, sizeof(*v));
  v->uMajor = 9;
  v->uMinor = 9;
  v->uRevision = 9; // deliberately not a real release - "fake SDK" marker
}
