#ifndef STUB_WELS_CODEC_API_H
#define STUB_WELS_CODEC_API_H
#include <stdint.h>

// Marker so anything that includes this header can verify it actually
// got OUR fake wels/codec_api.h, not a real libopenh264-dev's version
// that happens to also be reachable (e.g. via a stray -I/-isystem, a
// CPATH-style environment variable, or the tests/fake_wels/wels/
// subdirectory not having made it to disk on this checkout). If the
// real header gets picked up instead, FakeSVCEncoder/FakeSVCDecoder in
// fake_openh264.cxx are compiled against a base class they were never
// written to match, which surfaces as a wall of confusing "does not
// override"/"abstract class" errors rather than anything pointing at
// the actual cause - see the FAKEWELS_CODEC_API_H_INCLUDED check in
// fake_openh264.cxx and fake_control.h.
#define FAKEWELS_CODEC_API_H_INCLUDED 1

typedef enum { cmResultSuccess = 0, cmUnknownReason = 1 } CM_RETURN;
typedef enum { videoFormatI420 = 0 } EVideoFormatType;
typedef enum { CAMERA_VIDEO_REAL_TIME = 0 } EUsageType;
typedef enum { LOW_COMPLEXITY = 0 } ECOMPLEXITY_MODE;
typedef enum { RC_BITRATE_MODE = 0 } RC_MODES;
typedef enum { SM_SINGLE_SLICE = 0, SM_SIZELIMITED_SLICE = 1 } SliceModeEnum;
typedef enum { videoFrameTypeInvalid = 0, videoFrameTypeIDR, videoFrameTypeI, videoFrameTypeP, videoFrameTypeSkip, videoFrameTypeIPMixed } EVideoFrameType;
typedef enum { dsErrorFree = 0, dsFramePending = 1, dsRefLost = 2, dsBitstreamError = 4, dsDepLayerLost = 8,
               dsNoParamSets = 16, dsDataErrorConcealed = 32, dsInvalidArgument = 4096 } DECODING_STATE;
typedef enum { ERROR_CON_DISABLE = 0 } ERROR_CON_IDC;
typedef enum { VIDEO_BITSTREAM_DEFAULT = 0 } VIDEO_BITSTREAM_TYPE;
typedef enum { ENCODER_OPTION_TRACE_LEVEL = 0, ENCODER_OPTION_TRACE_CALLBACK = 1 } ENCODER_OPTION;
typedef enum { DECODER_OPTION_TRACE_LEVEL = 0, DECODER_OPTION_TRACE_CALLBACK = 1, DECODER_OPTION_IDR_PIC_ID = 2 } DECODER_OPTION;
typedef enum { WELS_LOG_QUIET=0, WELS_LOG_ERROR=1, WELS_LOG_WARNING=2, WELS_LOG_INFO=4, WELS_LOG_DEBUG=8, WELS_LOG_DETAIL=16 } WelsLogLevel;

typedef void (*WelsTraceCallback)(void* ctx, int level, const char* msg);

struct SSliceArgument {
  SliceModeEnum uiSliceMode;
  unsigned int  uiSliceSizeConstraint;
};

typedef enum { PRO_UNKNOWN = 0, PRO_BASELINE = 66, PRO_MAIN = 77, PRO_EXTENDED = 88, PRO_HIGH = 100 } EProfileIdc;
typedef enum { LEVEL_UNKNOWN = 0, LEVEL_1_0 = 10, LEVEL_1_B = 9, LEVEL_1_1 = 11, LEVEL_1_2 = 12, LEVEL_1_3 = 13,
               LEVEL_2_0 = 20, LEVEL_2_1 = 21, LEVEL_2_2 = 22, LEVEL_3_0 = 30, LEVEL_3_1 = 31, LEVEL_3_2 = 32,
               LEVEL_4_0 = 40, LEVEL_4_1 = 41, LEVEL_4_2 = 42, LEVEL_5_0 = 50, LEVEL_5_1 = 51, LEVEL_5_2 = 52 } ELevelIdc;

struct SSpatialLayerConfig {
  int iVideoWidth;
  int iVideoHeight;
  float fFrameRate;
  int iSpatialBitrate;
  int iMaxSpatialBitrate;
  EProfileIdc uiProfileIdc;
  ELevelIdc uiLevelIdc;
  SSliceArgument sSliceArgument;
};

struct SEncParamExt {
  EUsageType iUsageType;
  int iPicWidth;
  int iPicHeight;
  int iTargetBitrate;
  RC_MODES iRCMode;
  float fMaxFrameRate;
  int iMaxBitrate;
  int iMinQp;
  int iMaxQp;
  unsigned int uiIntraPeriod;
  bool bPrefixNalAddingCtrl;
  ECOMPLEXITY_MODE iComplexityMode;
  unsigned int uiMaxNalSize;
  SSpatialLayerConfig sSpatialLayers[4];
};

struct SSourcePicture {
  int iColorFormat;
  int iStride[4];
  unsigned char * pData[4];
  int iPicWidth;
  int iPicHeight;
  long long uiTimeStamp;
};

struct SLayerBSInfo {
  unsigned char * pBsBuf;
  int pNalLengthInByte[128];
  int iNalCount;
  unsigned int uiQualityId;
};

struct SFrameBSInfo {
  int iLayerNum;
  SLayerBSInfo sLayerInfo[4];
  EVideoFrameType eFrameType;
};

struct SVideoProperty {
  unsigned int size;
  int eVideoBsType;
};

struct SDecodingParam {
  ERROR_CON_IDC eEcActiveIdc;
  SVideoProperty sVideoProperty;
};

struct SSystemBuffer {
  int iWidth;
  int iHeight;
  int iStride[2];
};

struct SBufferInfo {
  int iBufferStatus;
  union {
    SSystemBuffer sSystemBuffer;
  } UsrData;
};

class ISVCEncoder {
public:
  virtual ~ISVCEncoder() {}
  virtual int SetOption(ENCODER_OPTION, void*) = 0;
  virtual int GetDefaultParams(SEncParamExt*) = 0;
  virtual int InitializeExt(SEncParamExt*) = 0;
  virtual int Uninitialize() = 0;
  virtual int EncodeFrame(const SSourcePicture*, SFrameBSInfo*) = 0;
  virtual int ForceIntraFrame(bool) = 0;
};

class ISVCDecoder {
public:
  virtual ~ISVCDecoder() {}
  virtual int SetOption(DECODER_OPTION, void*) = 0;
  virtual long Initialize(const SDecodingParam*) = 0;
  virtual int Uninitialize() = 0;
  virtual DECODING_STATE DecodeFrameNoDelay(const unsigned char*, int, unsigned char**, SBufferInfo*) = 0;
  virtual long GetOption(DECODER_OPTION, void*) = 0;
};

int WelsCreateSVCEncoder(ISVCEncoder**);
void WelsDestroySVCEncoder(ISVCEncoder*);
long WelsCreateDecoder(ISVCDecoder**);
void WelsDestroyDecoder(ISVCDecoder*);

#endif
