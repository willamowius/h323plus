/*
 * OpenH264 H.264 Plugin codec for H323Plus
 *
 * This plugin is a conversion of two source trees:
 *  - the codec logic (parameter setup, NALU handling, RTP packetisation
 *    strategy) is ported from OPAL's plugins/video/openh264/openh264.cxx
 *    (Copyright (C) 2014 Vox Lucida Pty Ltd, MPL 1.0), which drives Cisco's
 *    BSD-licensed OpenH264 SDK
 *  - the plugin ABI (the raw PluginCodec_Definition table, H.245 generic
 *    capability negotiation, level/resolution tables and RTP
 *    (de)packetisation via H264Frame) is ported from H323Plus's
 *    plugins/video/H.264/h264-x264.{h,cxx} (Copyright (C) Matthias
 *    Schneider / Simon Horne, MPL 1.0)
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 */

#define PLUGIN_CODEC_DLL_EXPORTS  1

#include "openh264.h"

#ifdef _MSC_VER
 #include "../common/trace.h"
#else
 #include "trace.h"
#endif

#include <wels/codec_api.h>
#include <wels/codec_ver.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <ios>

#if defined(_WIN32) || defined(_WIN32_WCE)
  #include <malloc.h>
  #define snprintf _snprintf
#endif

// Upper bound on NAL units flattened out of one encoded picture across all
// of OpenH264's SLayerInfo entries (see EncodeFrames()). Comfortably above
// h264frame.h's own internal per-picture cap (MAX_NAL_BUFFER = 150), which
// remains the authoritative limit and will TRACE + truncate if exceeded.
#define MAX_ENCODED_NALS 256

///////////////////////////////////////////////////////////////////////////////
// small helpers (unchanged from the original H323Plus x264 plugin)

static char * num2str(int num)
{
  char buf[20];
  sprintf(buf, "%i", num);
  return strdup(buf);
}

static int int_from_string(std::string str)
{
  if (str.find_first_of("\"") != std::string::npos)
    return (atoi( str.substr(1, str.length()-2).c_str()));

  return (atoi( str.c_str()));
}

static void profile_level_from_string(std::string profileLevelString, unsigned & profile, unsigned & constraints, unsigned & level)
{
  if (profileLevelString.find_first_of("\"") != std::string::npos)
    profileLevelString = profileLevelString.substr(1, profileLevelString.length()-2);

  unsigned profileLevelInt = strtoul(profileLevelString.c_str(), NULL, 16);

  if (profileLevelInt == 0) {
    // Default handling according to RFC 3984 - Baseline, Level 1
    profileLevelInt = 0x42C00A;
  }

  profile     = (profileLevelInt & 0xFF0000) >> 16;
  constraints = (profileLevelInt & 0x00FF00) >> 8;
  level       = (profileLevelInt & 0x0000FF);
}

static int GetLevelLimits(unsigned _level, unsigned & maxMB, unsigned & maxMBPS, unsigned & h264level)
{
  if (_level == 0) return 0;

  unsigned j = 0;
  while (h264_levels[j].level_idc) {
    if (h264_levels[j].h241_level == _level) {
      maxMB = h264_levels[j].frame_size;
      maxMBPS = h264_levels[j].mbps;
      h264level = h264_levels[j].level_idc;
      return 1;
    }
    j++;
  }
  return 0;
}

static int setLevel(unsigned w, unsigned h, unsigned r, unsigned & level, unsigned & h264level)
{
  uint32_t nbMBsPerFrame = w * h / 256;
  uint32_t nbMBsPerSec = nbMBsPerFrame * r;

  unsigned j = 0;
  level = 0;
  while (h264_levels[j].level_idc) {
    if ((nbMBsPerFrame <= h264_levels[j].frame_size) && (nbMBsPerSec <= h264_levels[j].mbps)) {
      if (j > 0) {
        level = h264_levels[j-1].h241_level;
        h264level = h264_levels[j-1].level_idc;
        break;
      }
    }
    j++;
  }
  return (level > 0);
}

static int adjust_bitrate_to_level(unsigned & targetBitrate, unsigned level, int idx = -1)
{
  int i = 0;
  if (idx == -1) {
    while (h264_levels[i].level_idc) {
      if (h264_levels[i].level_idc == level)
        break;
      i++;
    }

    if (!h264_levels[i].level_idc) {
      TRACE(1, "H264\tCap\tIllegal Level negotiated");
      return 0;
    }
  }
  else
    i = idx;

  if (targetBitrate == 0)
    targetBitrate = h264_levels[i].bitrate;
  else if (targetBitrate > h264_levels[i].bitrate)
    targetBitrate = h264_levels[i].bitrate;

  return 1;
}

static int adjust_to_level(unsigned & width, unsigned & height, unsigned & frameTime, unsigned & targetBitrate, unsigned level)
{
  int i = 0;
  while (h264_levels[i].level_idc) {
    if (h264_levels[i].level_idc == level)
      break;
    i++;
  }

  if (!h264_levels[i].level_idc) {
    TRACE(1, "H264\tCap\tIllegal Level negotiated");
    return 0;
  }

  uint32_t nbMBsPerFrame = width * height / 256;
  unsigned j = 0;
  if    ( (nbMBsPerFrame          > h264_levels[i].frame_size)
       || (width  * width  / 2048 > h264_levels[i].frame_size)
       || (height * height / 2048 > h264_levels[i].frame_size) ) {

    while (h264_resolutions[j].width) {
      if  ( (h264_resolutions[j].macroblocks                                <= h264_levels[i].frame_size)
         && (h264_resolutions[j].width  * h264_resolutions[j].width  / 2048 <= h264_levels[i].frame_size)
         && (h264_resolutions[j].height * h264_resolutions[j].height / 2048 <= h264_levels[i].frame_size) )
          break;
      j++;
    }
    if (!h264_resolutions[j].width) {
      TRACE(1, "H264\tCap\tNo Resolution found that has number of macroblocks <=" << h264_levels[i].frame_size);
      return 0;
    }
    else {
      width  = h264_resolutions[j].width;
      height = h264_resolutions[j].height;
    }
  }

  if (frameTime == 0)
    return 0;
  uint32_t nbMBsPerSecond = width * height / 256 * (90000 / frameTime);
  if (nbMBsPerSecond > h264_levels[i].mbps)
    frameTime = (unsigned) (90000 / 256 * width  * height / h264_levels[i].mbps );

  adjust_bitrate_to_level(targetBitrate, level, i);
  return 1;
}

///////////////////////////////////////////////////////////////////////////////
// route OpenH264's own trace output into the H323Plus trace system

static void OpenH264TraceCallback(void*, int welsLevel, const char* message)
{
  unsigned level;
  if (welsLevel <= WELS_LOG_ERROR)
    level = 1;
  else if (welsLevel <= WELS_LOG_WARNING)
    level = 2;
  else if (welsLevel <= WELS_LOG_INFO)
    level = 4;
  else
    level = 5;

  if (!Trace::CanTraceUserPlane(level))
    return;

  TRACE_UP(level, "H264\tOpenH264\t" << message);
}

static int TraceLevel = WELS_LOG_WARNING;
static WelsTraceCallback TraceCallbackPtr = OpenH264TraceCallback;

static void CheckVersion(bool encoder)
{
  OpenH264Version version;
  WelsGetCodecVersionEx(&version);
  TRACE(4, "H264\tOpenH264\tCreated " << (encoder ? "encoder" : "decoder")
        << ", library version " << version.uMajor << '.' << version.uMinor << '.' << version.uRevision);
}

///////////////////////////////////////////////////////////////////////////////
// H264EncoderContext

H264EncoderContext::H264EncoderContext()
  : _encoder(NULL)
  , _frameWidth(CIF_WIDTH)
  , _frameHeight(CIF_HEIGHT)
  , _frameRate(H264_FRAME_RATE)
  , _targetBitrate(H264_BITRATE/1000)
  , _maxKeyFramePeriod(H264_KEY_FRAME_INTERVAL)
  , _maxRTPFrameSize(H264_PAYLOAD_SIZE)
  , _maxNALSize(H264_PAYLOAD_SIZE)
  , _tsto(20)
  , _profile(H264_BASE_IDC)
  , _constraints(0)
  , _level(30)
  , _forceIFrame(false)
  , _needsReset(true)
  , _timestamp(0)
{
  CheckVersion(true);
  _txH264Frame.SetMaxPayloadSize(H264_PAYLOAD_SIZE);
}

H264EncoderContext::~H264EncoderContext()
{
  WaitAndSignal m(_mutex);
  CloseCodec();
}

bool H264EncoderContext::OpenCodec()
{
  if (WelsCreateSVCEncoder(&_encoder) != cmResultSuccess || _encoder == NULL) {
    TRACE(1, "H264\tEnc\tCould not create OpenH264 encoder.");
    _encoder = NULL;
    return false;
  }

  _encoder->SetOption(ENCODER_OPTION_TRACE_CALLBACK, (void*)&TraceCallbackPtr);
  _encoder->SetOption(ENCODER_OPTION_TRACE_LEVEL, (void*)&TraceLevel);

  _needsReset = true;
  return true;
}

void H264EncoderContext::CloseCodec()
{
  if (_encoder != NULL) {
    _encoder->Uninitialize();
    WelsDestroySVCEncoder(_encoder);
    _encoder = NULL;
  }
}

void H264EncoderContext::Lock()
{
  _mutex.Wait();
}

void H264EncoderContext::Unlock()
{
  _mutex.Signal();
}

void H264EncoderContext::FastUpdateRequested()
{
  WaitAndSignal m(_mutex);
  _forceIFrame = true;
}

void H264EncoderContext::SetMaxRTPFrameSize(unsigned size)
{
  _maxRTPFrameSize = size;
  _txH264Frame.SetMaxPayloadSize((uint16_t)size);
}

void H264EncoderContext::SetMaxKeyFramePeriod(unsigned period)
{
  _maxKeyFramePeriod = period;
  _needsReset = true;
}

void H264EncoderContext::SetTargetBitrate(unsigned rate)
{
  // 'rate' is in kbit/s, matching the convention used by the rest of the
  // H323Plus codec plugin API (see encoder_set_options()).
  _targetBitrate = rate;
  _needsReset = true;
}

void H264EncoderContext::SetFrameWidth(unsigned width)
{
  _frameWidth = width;
  _needsReset = true;
}

void H264EncoderContext::SetFrameHeight(unsigned height)
{
  _frameHeight = height;
  _needsReset = true;
}

void H264EncoderContext::SetFrameRate(unsigned rate)
{
  _frameRate = rate;
  _needsReset = true;
}

void H264EncoderContext::SetTSTO(unsigned tsto)
{
  _tsto = tsto;
  _needsReset = true;
}

void H264EncoderContext::SetProfileLevel(unsigned profile, unsigned constraints, unsigned level)
{
  _profile = profile;
  _constraints = constraints;
  _level = level;
  _needsReset = true;
}

void H264EncoderContext::SetMaxNALSize(unsigned size)
{
  if (size > 0 && size < 1400) {
    _maxNALSize = size;
    _needsReset = true;
  }
}

void H264EncoderContext::ApplyOptions()
{
  if (_encoder == NULL || !_needsReset)
    return;

  _encoder->Uninitialize();

  SEncParamExt param;
  memset(&param, 0, sizeof(param));
  _encoder->GetDefaultParams(&param);

  param.iUsageType         = CAMERA_VIDEO_REAL_TIME;
  param.iComplexityMode    = LOW_COMPLEXITY;
  param.iPicWidth          = _frameWidth;
  param.iPicHeight         = _frameHeight;
  param.fMaxFrameRate      = _frameRate > 0 ? (float)_frameRate : (float)H264_FRAME_RATE;
  param.iTargetBitrate     = _targetBitrate * 1000;
  param.iMaxBitrate        = param.iTargetBitrate;
  param.iRCMode            = RC_BITRATE_MODE;
  param.iMinQp              = 12;
  param.iMaxQp              = 11 + (_tsto > 0 ? _tsto : 20);  // _tsto is 1..31
  param.uiIntraPeriod      = _maxKeyFramePeriod > 0 ? _maxKeyFramePeriod : H264_KEY_FRAME_INTERVAL;
  param.bPrefixNalAddingCtrl = false;

  param.sSpatialLayers[0].iVideoWidth        = _frameWidth;
  param.sSpatialLayers[0].iVideoHeight       = _frameHeight;
  param.sSpatialLayers[0].fFrameRate         = param.fMaxFrameRate;
  param.sSpatialLayers[0].iMaxSpatialBitrate = param.iMaxBitrate;
  param.sSpatialLayers[0].iSpatialBitrate    = param.iTargetBitrate;
  // _profile/_level (set via SetProfileLevel(), driven by the H.245/SDP
  // generic-capability negotiation in encoder_set_options()) are already
  // literal H.264 profile_idc/level_idc values (H264_BASE_IDC=66,
  // H264_HIGH_IDC=100; h264_levels[].h264level e.g. 30 for Level 3.0) -
  // the same numeric scale OpenH264's EProfileIdc/ELevelIdc use, so this
  // is a direct cast rather than a real mapping. Without this, OpenH264
  // could emit an SPS whose profile/level doesn't match what was
  // negotiated, which some receivers will refuse to decode at all.
  param.sSpatialLayers[0].uiProfileIdc = (EProfileIdc)_profile;
  param.sSpatialLayers[0].uiLevelIdc   = (ELevelIdc)_level;

  unsigned maxNAL = _maxNALSize > 0 ? _maxNALSize
                  : (_maxRTPFrameSize > PluginCodec_RTP_MinHeaderSize
                     ? _maxRTPFrameSize - PluginCodec_RTP_MinHeaderSize
                     : H264_PAYLOAD_SIZE);
  param.sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_SIZELIMITED_SLICE;
  param.sSpatialLayers[0].sSliceArgument.uiSliceSizeConstraint = param.uiMaxNalSize = maxNAL;

  _txH264Frame.SetMaxPayloadSize(_maxRTPFrameSize > 0 ? (uint16_t)_maxRTPFrameSize : H264_PAYLOAD_SIZE);

  int err = _encoder->InitializeExt(&param);
  TRACE(err == cmResultSuccess ? 3 : 1, "H264\tEnc\t" << (err == cmResultSuccess ? "Initialised" : "FAILED to initialise")
        << " encoder: " << _frameWidth << "x" << _frameHeight << "@" << param.fMaxFrameRate
        << ", " << param.iTargetBitrate << "bps, nal-size=" << param.uiMaxNalSize
        << ", kfr=" << param.uiIntraPeriod);

  _needsReset = false;
}

int H264EncoderContext::EncodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags)
{
  WaitAndSignal m(_mutex);

  if (_encoder == NULL && !OpenCodec()) {
    dstLen = 0;
    return 0;
  }

  if ((flags & PluginCodec_CoderForceIFrame) != 0)
    _forceIFrame = true;
  flags = 0;

  if (!_txH264Frame.HasRTPFrames()) {
    // Raw video frames handed to the encoder are wrapped in a minimal
    // RTP-style header (PluginCodec_RTP_MinHeaderSize bytes), exactly like
    // the decoder's own output side wraps its raw frames - it carries no
    // useful fields here, but has to be skipped to reach the real
    // PluginCodec_Video_FrameHeader + I420 payload.
    if (srcLen < PluginCodec_RTP_MinHeaderSize + sizeof(PluginCodec_Video_FrameHeader)) {
      TRACE(1, "H264\tEnc\tVideo grab frame too small");
      dstLen = 0;
      return 0;
    }

    RTPFrame srcRTP((const unsigned char *)src, (int)srcLen);
    PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)srcRTP.GetPayloadPtr();

    if (header->width == 0 || header->height == 0) {
      TRACE(3, "H264\tEnc\tIgnoring frame with invalid dimensions " << header->width << "x" << header->height);
      dstLen = 0;
      flags |= PluginCodec_ReturnCoderLastFrame;
      return 1;
    }

    // header->width/height are trusted below to compute plane pointers
    // (picture.pData[1]/[2]) into this same buffer - srcLen was only
    // checked against the fixed header size above, never against what
    // these claimed dimensions actually require. A caller reporting
    // large dimensions with a too-small buffer (a malfunctioning video
    // source, not necessarily malicious - this is local capture input,
    // not network data, but still not trustworthy) would make those
    // pointers point past the end of the real allocation, which the
    // real encoder would then read from - an out-of-bounds read outside
    // this plugin's own code entirely. Reject before that can happen.
    {
      unsigned pw = (header->width + 1) & ~1u;
      unsigned ph = (header->height + 1) & ~1u;
      uint64_t requiredPayload = (uint64_t)pw * ph + 2 * ((uint64_t)pw * ph / 4);
      uint64_t required = PluginCodec_RTP_MinHeaderSize + sizeof(PluginCodec_Video_FrameHeader) + requiredPayload;
      if ((uint64_t)srcLen < required) {
        TRACE(1, "H264\tEnc\tVideo grab frame too small for claimed " << header->width << "x" << header->height
              << " (have " << srcLen << " bytes, need " << required << ")");
        dstLen = 0;
        return 0;
      }
    }

    if (header->width != _frameWidth || header->height != _frameHeight) {
      _frameWidth = header->width;
      _frameHeight = header->height;
      _needsReset = true;
    }

    ApplyOptions();
    if (_encoder == NULL) {
      dstLen = 0;
      return 0;
    }

    unsigned planeWidth = (header->width+1)&~1;
    unsigned planeHeight = (header->height+1)&~1;

    SSourcePicture picture;
    memset(&picture, 0, sizeof(picture));
    picture.iColorFormat = videoFormatI420;
    picture.iPicWidth  = header->width;
    picture.iPicHeight = header->height;
    picture.iStride[0] = planeWidth;
    picture.iStride[1] = picture.iStride[2] = planeWidth/2;
    picture.pData[0] = OPAL_VIDEO_FRAME_DATA_PTR(header);
    picture.pData[1] = picture.pData[0] + planeWidth*planeHeight;
    picture.pData[2] = picture.pData[1] + planeWidth*planeHeight/4;
    picture.uiTimeStamp = 0;

    if (_forceIFrame) {
      _encoder->ForceIntraFrame(true);
      _forceIFrame = false;
    }

    SFrameBSInfo bitstream;
    memset(&bitstream, 0, sizeof(bitstream));
    if (_encoder->EncodeFrame(&picture, &bitstream) != cmResultSuccess) {
      TRACE(1, "H264\tEnc\tFatal error encoding frame.");
      dstLen = 0;
      return 0;
    }

    if (bitstream.eFrameType == videoFrameTypeInvalid) {
      TRACE(1, "H264\tEnc\tFatal error encoding frame.");
      dstLen = 0;
      return 0;
    }

    if (bitstream.eFrameType == videoFrameTypeSkip) {
      TRACE(5, "H264\tEnc\tOutput frame skipped.");
      dstLen = 0;
      flags |= PluginCodec_ReturnCoderLastFrame;
      _timestamp += H264_CLOCKRATE / (_frameRate > 0 ? _frameRate : H264_FRAME_RATE);
      return 1;
    }

    // Flatten every NAL unit from every SLayerInfo entry into one list.
    // OpenH264 commonly splits one encoded picture's output across
    // multiple layers even with SVC scalability completely disabled -
    // typically one pseudo-layer carrying SPS/PPS and a separate one
    // carrying the actual coded slice - so iLayerNum > 1 is normal, not
    // an SVC-only case. Each layer's NAL buffer is walked independently
    // (they are not necessarily contiguous with each other), matching how
    // OPAL's own reference implementation loops over all layers.
    H264Frame::NALSource nalList[MAX_ENCODED_NALS];
    unsigned nalCount = 0;
    for (int layer = 0; layer < bitstream.iLayerNum && nalCount < MAX_ENCODED_NALS; layer++) {
      const SLayerBSInfo & layerInfo = bitstream.sLayerInfo[layer];
      const uint8_t * p = layerInfo.pBsBuf;
      for (int nal = 0; nal < layerInfo.iNalCount && nalCount < MAX_ENCODED_NALS; nal++) {
        unsigned len = (unsigned)layerInfo.pNalLengthInByte[nal];
        nalList[nalCount].data = p;
        nalList[nalCount].length = len;
        nalCount++;
        p += len;
      }
    }

    if (nalCount == 0) {
      TRACE(3, "H264\tEnc\tEncoded picture had no NAL units - skipping");
      dstLen = 0;
      flags |= PluginCodec_ReturnCoderLastFrame;
      _timestamp += H264_CLOCKRATE / (_frameRate > 0 ? _frameRate : H264_FRAME_RATE);
      return 1;
    }

    TRACE(4, "H264\tEnc\tStamping picture with timestamp=" << _timestamp);
    _txH264Frame.SetTimestamp(_timestamp);
    _txH264Frame.SetFromFrame(nalList, nalCount);
    _timestamp += H264_CLOCKRATE / (_frameRate > 0 ? _frameRate : H264_FRAME_RATE);
  }

  // dst is a buffer the RTP session has already prepared with a valid
  // header (SSRC, sequence number, and the correct dynamic payload type
  // negotiated for this call) - the 2-argument RTPFrame constructor wraps
  // it as-is and uses GetHeaderSize() to find the payload offset, rather
  // than reinitialising the header the way the 3-argument constructor
  // does. GetRTPFrame() fills in payload, marker bit and timestamp (as
  // set via SetTimestamp() above); it must not touch the payload type or
  // sequence number the session already assigned.
  RTPFrame dstRTP(dst, dstLen);
  if (!_txH264Frame.GetRTPFrame(dstRTP, flags))
    return 0;

  dstLen = dstRTP.GetFrameLen();
  return 1;
}

///////////////////////////////////////////////////////////////////////////////
// H264DecoderContext

H264DecoderContext::H264DecoderContext()
  : _decoder(NULL)
  , _gotIFrame(false)
  , _gotAGoodFrame(true)   // force a Fast Picture Update on the very first frame
  , _lastTimeStamp(0)
  , _lastSeqNo(0)
  , _frameCounter(0)
  , _skippedFrameCounter(0)
{
  CheckVersion(false);

  if (WelsCreateDecoder(&_decoder) != cmResultSuccess || _decoder == NULL) {
    TRACE(1, "H264\tDec\tCould not create OpenH264 decoder.");
    _decoder = NULL;
    return;
  }

  _decoder->SetOption(DECODER_OPTION_TRACE_CALLBACK, (void*)&TraceCallbackPtr);
  _decoder->SetOption(DECODER_OPTION_TRACE_LEVEL, (void*)&TraceLevel);

  SDecodingParam param;
  memset(&param, 0, sizeof(param));
  param.eEcActiveIdc = ERROR_CON_DISABLE;  // let H323Plus's request-IFrame logic handle loss, rather than OpenH264 concealment
  param.sVideoProperty.size = sizeof(param.sVideoProperty);
  param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

  if (_decoder->Initialize(&param) != cmResultSuccess) {
    TRACE(1, "H264\tDec\tCould not initialise OpenH264 decoder.");
    WelsDestroyDecoder(_decoder);
    _decoder = NULL;
    return;
  }

  TRACE(4, "H264\tDec\tOpened OpenH264 decoder.");
}

H264DecoderContext::~H264DecoderContext()
{
  if (_decoder != NULL) {
    TRACE(4, "H264\tDec\tClosing OpenH264 decoder, decoded " << _frameCounter << " frames, skipped " << _skippedFrameCounter << " frames");
    _decoder->Uninitialize();
    WelsDestroyDecoder(_decoder);
  }
}

int H264DecoderContext::DecodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags)
{
  if (_decoder == NULL)
    return 0;

  RTPFrame srcRTP(src, srcLen);
  RTPFrame dstRTP(dst, dstLen, 0);
  dstLen = 0;
  flags = 0;

  if (!_rxH264Frame.SetFromRTPFrame(srcRTP, flags)) {
    _rxH264Frame.BeginNewFrame();
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  _lastSeqNo = srcRTP.GetSequenceNumber();

  if (srcRTP.GetMarker() == 0) {
    // check if we have a change in TimeStamp without Marker i.e. lost frame
    if (_lastTimeStamp > 0 && _lastTimeStamp != srcRTP.GetTimestamp()) {
      flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
      _gotAGoodFrame = false;
    }
    _lastTimeStamp = srcRTP.GetTimestamp();
    return 1;
  }
  else
    _lastTimeStamp = 0;

  if (_rxH264Frame.GetFrameSize() == 0) {
    _rxH264Frame.BeginNewFrame();
    TRACE(4, "H264\tDec\tGot an empty frame - skipping");
    _skippedFrameCounter++;
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  if (!_gotIFrame) {
    if (!_rxH264Frame.IsSync()) {
      TRACE(1, "H264\tDec\tWaiting for an I-Frame");
      _rxH264Frame.BeginNewFrame();
      flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
      _gotAGoodFrame = false;
      return 1;
    }
    _gotIFrame = true;
  }

  uint8_t * bufferData[3] = { NULL, NULL, NULL };
  SBufferInfo bufferInfo;
  memset(&bufferInfo, 0, sizeof(bufferInfo));

  DECODING_STATE status = _decoder->DecodeFrameNoDelay(_rxH264Frame.GetFramePtr(),
                                                         (int)_rxH264Frame.GetFrameSize(),
                                                         bufferData, &bufferInfo);
  _rxH264Frame.BeginNewFrame();

  if (status != dsErrorFree) {
    TRACE(status >= dsInvalidArgument ? 1 : 3, "H264\tDec\tDecode error: status=0x" << std::hex << status << std::dec);
    _skippedFrameCounter++;
    if (status != dsDataErrorConcealed) {
      flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
      _gotAGoodFrame = false;
    }
    if (status >= dsInvalidArgument)
      return 1;
  }

  if (bufferInfo.iBufferStatus == 0) {
    // no complete picture out on this call - OpenH264 may hold on to a
    // frame internally for a call or two, this is not an error.
    return 1;
  }

  unsigned width  = bufferInfo.UsrData.sSystemBuffer.iWidth;
  unsigned height = bufferInfo.UsrData.sSystemBuffer.iHeight;
  int frameBytes = (width * height * 3) / 2;

  if ((sizeof(PluginCodec_Video_FrameHeader) + frameBytes + dstRTP.GetHeaderSize()) > (unsigned)dstRTP.GetFrameLen()) {
    // NOTE: dstRTP was constructed with the full caller-supplied buffer
    // size in dstLen before we zeroed it above; if the negotiated maximum
    // decode buffer (see decoder_get_output_data_size()) is smaller than
    // this picture, we can't deliver it.
    TRACE(1, "H264\tDec\tOutput buffer too small for " << width << "x" << height << " frame");
    flags |= PluginCodec_ReturnCoderBufferTooSmall;
    return 1;
  }

  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)dstRTP.GetPayloadPtr();
  header->x = header->y = 0;
  header->width = width;
  header->height = height;

  unsigned char * dstData = OPAL_VIDEO_FRAME_DATA_PTR(header);
  int strides[3] = {
    bufferInfo.UsrData.sSystemBuffer.iStride[0],
    bufferInfo.UsrData.sSystemBuffer.iStride[1],
    bufferInfo.UsrData.sSystemBuffer.iStride[1]
  };
  for (int plane = 0; plane < 3; plane++) {
    unsigned char * srcData = bufferData[plane];
    int dstStride = plane ? (int)(width >> 1) : (int)width;
    int planeHeight = plane ? (int)(height >> 1) : (int)height;

    if (strides[plane] == dstStride) {
      memcpy(dstData, srcData, dstStride*planeHeight);
      dstData += dstStride*planeHeight;
    }
    else {
      for (int y = 0; y < planeHeight; y++) {
        memcpy(dstData, srcData, dstStride);
        dstData += dstStride;
        srcData += strides[plane];
      }
    }
  }

  dstRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader) + frameBytes);
  dstRTP.SetTimestamp(srcRTP.GetTimestamp());
  dstRTP.SetMarker(1);
  dstLen = dstRTP.GetFrameLen();

  flags |= PluginCodec_ReturnCoderLastFrame;
  if (_rxH264Frame.IsSync())
    flags |= PluginCodec_ReturnCoderIFrame;

  _frameCounter++;
  _gotAGoodFrame = true;
  return 1;
}

///////////////////////////////////////////////////////////////////////////////
// H323Plus PluginCodec_Definition control callbacks
// (unchanged in substance from the original H323Plus H.264 plugin -
//  these are codec-independent, operating only on the negotiation tables)

static int valid_for_protocol(const struct PluginCodec_Definition * codec,
                               void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char *))
    return 0;

  if (codec->h323CapabilityType != PluginCodec_H323Codec_NoH323)
    return (STRCMPI((const char *)parm, "h.323") == 0 ||
            STRCMPI((const char *)parm, "h323") == 0) ? 1 : 0;
  else
    return (STRCMPI((const char *)parm, "sip") == 0) ? 1 : 0;
}

static int get_codec_options(const struct PluginCodec_Definition * codec,
                              void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(struct PluginCodec_Option **))
    return 0;

  *(const void **)parm = codec->userData;
  *parmLen = 0;
  return 1;
}

static int free_codec_options(const struct PluginCodec_Definition *, void *, const char *,
                               void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  char ** strings = (char **) parm;
  for (char ** string = strings; *string != NULL; string++) {
    free(*string);
  }
  free(strings);
  return 1;
}

static int to_normalised_options(const struct PluginCodec_Definition *, void *, const char *,
                                  void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  unsigned profile = 66;
  unsigned constraints = 0;
  unsigned level = 51;
  unsigned width = 352;
  unsigned height = 288;
  unsigned frameTime = 3000;
  unsigned targetBitrate = 64000;

  for (const char * const * option = *(const char * const * *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], "CAP RFC3894 Profile Level") == 0)
      profile_level_from_string(option[1], profile, constraints, level);
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
      width = atoi(option[1]);
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
      height = atoi(option[1]);
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
      frameTime = atoi(option[1]);
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
      targetBitrate = atoi(option[1]);
  }

  // Enforce macroblock-aligned dimensions for optimal compression
  width -= width % 16;
  height -= height % 16;

  if (!adjust_to_level(width, height, frameTime, targetBitrate, level))
    return 0;

  char ** options = (char **)calloc(9, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[0] = strdup(PLUGINCODEC_OPTION_FRAME_WIDTH);
  options[1] = num2str(width);
  options[2] = strdup(PLUGINCODEC_OPTION_FRAME_HEIGHT);
  options[3] = num2str(height);
  options[4] = strdup(PLUGINCODEC_OPTION_FRAME_TIME);
  options[5] = num2str(frameTime);
  options[6] = strdup(PLUGINCODEC_OPTION_TARGET_BIT_RATE);
  options[7] = num2str(targetBitrate);

  return 1;
}

static int to_customised_options(const struct PluginCodec_Definition * codec, void * _context,
                                  const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  unsigned orgframeWidth = codec->parm.video.maxFrameWidth;
  unsigned orgframeHeight = codec->parm.video.maxFrameHeight;

  unsigned frameWidth = 0;
  unsigned frameHeight = 0;
  unsigned frameRate = 0;
  unsigned level = 0;
  unsigned h264level = 0;
  unsigned targetBitrate = 0;

  const char ** option = (const char **)parm;
  for (int i = 0; option[i] != NULL; i += 2) {
    if (STRCMPI(option[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
      frameWidth = atoi(option[i+1]);
    if (STRCMPI(option[i], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
      frameHeight = atoi(option[i+1]);
    if (STRCMPI(option[i], PLUGINCODEC_OPTION_FRAME_TIME) == 0) {
      int frameTime = atoi(option[i+1]);
      frameRate = (frameTime == 0) ? 0 : (unsigned)(H264_CLOCKRATE / frameTime);
    }
    if (STRCMPI(option[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
      targetBitrate = atoi(option[i+1]);
  }

  if (frameWidth > orgframeWidth || frameHeight > orgframeHeight)
    return 0;

  if (!setLevel(frameWidth, frameHeight, frameRate, level, h264level) && !adjust_bitrate_to_level(targetBitrate, h264level))
    return 0;

  char ** options = (char **)parm;
  for (int i = 0; options[i] != NULL; i += 2) {
    if (STRCMPI(options[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
      options[i+1] = num2str(targetBitrate);
    if (STRCMPI(options[i], PLUGINCODEC_OPTION_LEVEL) == 0)
      options[i+1] = num2str(level);
  }

  if (_context != NULL) {
    H264EncoderContext * context = (H264EncoderContext *)_context;
    context->Lock();
    context->SetProfileLevel(H264_BASE_IDC, 0, h264level);
    context->SetFrameWidth(frameWidth);
    context->SetFrameHeight(frameHeight);
    context->SetFrameRate(frameRate);
    context->SetTargetBitrate((unsigned)(targetBitrate / 1000));
    context->ApplyOptions();
    context->Unlock();
  }

  return 1;
}

static int encoder_set_options(const struct PluginCodec_Definition * codec, void * _context, const char *,
                                void * parm, unsigned * parmLen)
{
  if (_context == NULL || parmLen == NULL || *parmLen != sizeof(const char **))
    return 0;

  H264EncoderContext * context = (H264EncoderContext *)_context;

  context->Lock();
  unsigned profile = H264_BASE_IDC;
  unsigned constraints = 0;
  unsigned level = 0;
  unsigned orgframeWidth = codec->parm.video.maxFrameWidth;
  unsigned frameWidth = orgframeWidth;
  unsigned orgframeHeight = codec->parm.video.maxFrameHeight;
  unsigned frameHeight = orgframeHeight;
  unsigned orgframeRate = codec->parm.video.maxFrameRate;
  unsigned frameRate = orgframeRate;
  unsigned targetBitrate = codec->bitsPerSec;
  unsigned h264level = 36;
  unsigned maxNALSize = H264_PAYLOAD_SIZE;

  if (parm != NULL) {
    const char ** options = (const char **)parm;
    for (int i = 0; options[i] != NULL; i += 2) {
      if (STRCMPI(options[i], "CAP RFC3894 Profile Level") == 0)
        profile_level_from_string(options[i+1], profile, constraints, h264level);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
        targetBitrate = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_TIME) == 0) {
        int frameTime = atoi(options[i+1]);
        frameRate = (frameTime == 0) ? 0 : (unsigned)(H264_CLOCKRATE / frameTime);
      }
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
        frameHeight = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
        frameWidth = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_MAX_FRAME_SIZE) == 0)
        context->SetMaxRTPFrameSize(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TX_KEY_FRAME_PERIOD) == 0)
        context->SetMaxKeyFramePeriod(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TEMPORAL_SPATIAL_TRADE_OFF) == 0)
        context->SetTSTO(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_MAX_PAYLOAD) == 0) {
        if (!maxNALSize || maxNALSize > (unsigned)atoi(options[i+1])) {
          maxNALSize = atoi(options[i+1]);
          context->SetMaxNALSize(maxNALSize);
        }
      }
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_PROFILE) == 0)
        profile = ((atoi(options[i+1]) == H264_PROFILE_HIGH) ? H264_HIGH_IDC : H264_BASE_IDC);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_LEVEL) == 0)
        level = atoi(options[i+1]);
    }
  }

  if (level > 0) {
    unsigned maxMB = 0, maxMBPS = 0;
    GetLevelLimits(level, maxMB, maxMBPS, h264level);

    if (!setLevel(frameWidth, frameHeight, frameRate, level, h264level)) {
      // setLevel() couldn't find a matching table entry from the raw
      // negotiated size/rate; fall back to the level signalled directly.
      GetLevelLimits(level, maxMB, maxMBPS, h264level);
    }
    adjust_bitrate_to_level(targetBitrate, h264level);

    context->SetTargetBitrate((unsigned)(targetBitrate / 1000));
    context->SetProfileLevel(profile, constraints, h264level);
    context->SetFrameHeight(frameHeight);
    context->SetFrameWidth(frameWidth);
    context->SetFrameRate(frameRate);
  }
  else {
    context->SetTargetBitrate((unsigned)(targetBitrate / 1000));
    context->SetProfileLevel(profile, constraints, h264level);
    context->SetFrameHeight(frameHeight);
    context->SetFrameWidth(frameWidth);
    context->SetFrameRate(frameRate);
  }

  context->ApplyOptions();
  context->Unlock();

  return 1;
}

static int encoder_event_handler(const struct PluginCodec_Definition * codec, void * _context, const char *,
                                  void * parm, unsigned * parmLen)
{
  if (_context == NULL || parmLen == NULL || *parmLen != sizeof(const char **))
    return 0;

  H264EncoderContext * context = (H264EncoderContext *)_context;
  // NOTE: no Lock()/Unlock() wrapper here - FastUpdateRequested() already
  // takes _mutex itself. _mutex is a plain POSIX semaphore (see
  // critsect.h), which is NOT reentrant: locking it here and then calling
  // FastUpdateRequested() (which locks it again from the same thread)
  // deadlocks permanently on the very first fast-update request - and
  // since EncodeFrames() also opens with WaitAndSignal(_mutex), every
  // subsequent encode call for this channel then blocks forever too,
  // i.e. encoding silently stops dead the moment a receiver ever asks
  // for a picture refresh. Do not wrap calls to already-self-locking
  // H264EncoderContext methods in Lock()/Unlock() here.
  if (parm != NULL) {
    char ** parms = (char **)parm;
    for (int i = 0; parms[i] != NULL; i += 2) {
      if (STRCMPI(parms[i], PLUGINCODEC_EVENT_FASTUPDATE) == 0) {
        TRACE(4, "H264\tEvt\tFAST PICTURE UPDATE");
        context->FastUpdateRequested();
      }
    }
  }

  return 1;
}

static int encoder_get_output_data_size(const PluginCodec_Definition *, void *, const char *, void *, unsigned *)
{
  return 2000;
}

static int decoder_get_output_data_size(const PluginCodec_Definition * codec, void *, const char *, void *, unsigned *)
{
  return sizeof(PluginCodec_Video_FrameHeader) + ((codec->parm.video.maxFrameWidth * codec->parm.video.maxFrameHeight * 3) / 2);
}

static int merge_profile_level_h264(char ** result, const char * dst, const char * src)
{
  unsigned srcProfile, srcConstraints, srcLevel;
  unsigned dstProfile, dstConstraints, dstLevel;
  profile_level_from_string(src, srcProfile, srcConstraints, srcLevel);
  profile_level_from_string(dst, dstProfile, dstConstraints, dstLevel);

  if (srcLevel == 10) srcLevel = 8;
  if (dstLevel == 10) dstLevel = 8;

  if (dstProfile > srcProfile)
    dstProfile = srcProfile;

  dstConstraints |= srcConstraints;

  if (dstLevel > srcLevel)
    dstLevel = srcLevel;

  if (dstLevel == 8) dstLevel = 10;

  char buffer[10];
  sprintf(buffer, "%x", (dstProfile<<16)|(dstConstraints<<8)|(dstLevel));
  *result = strdup(buffer);

  TRACE(4, "H264\tCap\tCustom merge profile-level: " << src << " and " << dst << " to " << *result);
  return true;
}

static int merge_packetization_mode(char ** result, const char * dst, const char * src)
{
  unsigned srcInt = int_from_string(src);
  unsigned dstInt = int_from_string(dst);

  // Default handling according to RFC 3984
  if (srcInt == 5) srcInt = 0;
  if (dstInt == 5) dstInt = 0;

  if (dstInt > srcInt)
    dstInt = srcInt;

  char buffer[10];
  sprintf(buffer, "%d", dstInt);
  *result = strdup(buffer);

  TRACE(4, "H264\tCap\tCustom merge packetization-mode: " << src << " and " << dst << " to " << *result);
  return 1;
}

static void free_string(char * str)
{
  free(str);
}

///////////////////////////////////////////////////////////////////////////////
// PluginCodec_Definition create/destroy/transcode entry points

static void * create_encoder(const struct PluginCodec_Definition * /*codec*/)
{
  return new H264EncoderContext;
}

static void destroy_encoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  delete (H264EncoderContext *)_context;
}

static int codec_encoder(const struct PluginCodec_Definition *, void * _context,
                          const void * from, unsigned * fromLen,
                          void * to, unsigned * toLen,
                          unsigned int * flag)
{
  H264EncoderContext * context = (H264EncoderContext *)_context;
  return context->EncodeFrames((const u_char *)from, *fromLen, (u_char *)to, *toLen, *flag);
}

static void * create_decoder(const struct PluginCodec_Definition *)
{
  return new H264DecoderContext;
}

static void destroy_decoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  delete (H264DecoderContext *)_context;
}

static int codec_decoder(const struct PluginCodec_Definition *, void * _context,
                          const void * from, unsigned * fromLen,
                          void * to, unsigned * toLen,
                          unsigned int * flag)
{
  H264DecoderContext * context = (H264DecoderContext *)_context;
  return context->DecodeFrames((const u_char *)from, *fromLen, (u_char *)to, *toLen, *flag);
}

/////////////////////////////////////////////////////////////////////////////

extern "C" {

PLUGIN_CODEC_IMPLEMENT(OpenH264)

PLUGIN_CODEC_DLL_API struct PluginCodec_Definition * PLUGIN_CODEC_GET_CODEC_FN(unsigned * count, unsigned version)
{
  char * debug_level = getenv("PTLIB_TRACE_CODECS");
  Trace::SetLevel(debug_level != NULL ? atoi(debug_level) : 0);

  debug_level = getenv("PTLIB_TRACE_CODECS_USER_PLANE");
  Trace::SetLevelUserPlane(debug_level != NULL ? atoi(debug_level) : 0);

  if (version < PLUGIN_CODEC_VERSION_OPTIONS) {
    *count = 0;
    TRACE(1, "H264\tOpenH264\tDisabled - plugin version mismatch");
    return NULL;
  }

  *count = sizeof(h264CodecDefn) / sizeof(struct PluginCodec_Definition);
  TRACE(1, "H264\tOpenH264\tEnabled");
  return h264CodecDefn;
}

};
