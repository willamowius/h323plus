/*
 * H.264 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) Matthias Schneider, All Rights Reserved
 *
 * This code is based on the file h261codec.cxx from the OPAL project released
 * under the MPL 1.0 license which contains the following:
 *
 * Copyright (c) 1998-2000 Equivalence Pty. Ltd.
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
 *
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): Matthias Schneider (ma30002000@yahoo.de)
 *                 Michele Piccini (michele@piccini.com)
 *                 Derek Smithies (derek@indranet.co.nz)
 *
 *
 */

/*
  Notes
  -----

 */

#include "h264-x264.h"

#include "plugin-config.h"

#if _MSC_VER < 1600
 #include "../common/dyna.h"
 #include "../common/trace.h"
 #include "../common/rtpframe.h"
#else
 #include "dyna.h"
 #include "trace.h"
 #include "rtpframe.h"
#endif


#include <stdlib.h>
#if defined(_WIN32) || defined(_WIN32_WCE)
  #include <malloc.h>
  #define STRCMPI  _strcmpi
  #define strdup   _strdup
#else
  #include <semaphore.h>
  #define STRCMPI  strcasecmp
#endif
#include <string.h>

FFMPEGLibrary FFMPEGLibraryInstance(CODEC_ID_H264);

static void logCallbackFFMPEG (void* v, int level, const char* fmt , va_list arg) {
  char buffer[512];
  int severity = 0;
  if (v) {
    switch (level)
    {
      case AV_LOG_QUIET: severity = 0; break;
      case AV_LOG_ERROR: severity = 1; break;
      case AV_LOG_INFO:  severity = 4; break;
      case AV_LOG_DEBUG: severity = 4; break;
      default:           severity = 4; break;
    }
    sprintf(buffer, "H264\tFFMPEG\t");
    vsprintf(buffer + strlen(buffer), fmt, arg);
    if (strlen(buffer) > 0)
      buffer[strlen(buffer)-1] = 0;
    if (severity == 4)
      { TRACE_UP (severity, buffer); }
    else
      { TRACE (severity, buffer); }
  }
}

static char * num2str(int num)
{
  char buf[20];
  sprintf(buf, "%i", num);
  return strdup(buf);
}

H264EncoderContext::H264EncoderContext()
{
  if (!H264EncCtxInstance.isLoaded()) {
    if (!H264EncCtxInstance.Load()) {
      TRACE(1, "H264\tCodec\tDisabled");
      // TODO: Throw exception here or assert
      // TODO: make findgplprocess and related method static in H264Encctx class and call from load
    }
  }

  H264EncCtxInstance.call(H264ENCODERCONTEXT_CREATE);
}

H264EncoderContext::~H264EncoderContext()
{
  WaitAndSignal m(_mutex);
  H264EncCtxInstance.call(H264ENCODERCONTEXT_DELETE);
}

void H264EncoderContext::ApplyOptions()
{
  H264EncCtxInstance.call(APPLY_OPTIONS);
}

void H264EncoderContext::SetMaxRTPFrameSize(unsigned size)
{
  H264EncCtxInstance.call(SET_MAX_FRAME_SIZE, size);
}

void H264EncoderContext::SetMaxKeyFramePeriod (unsigned period)
{
  H264EncCtxInstance.call(SET_MAX_KEY_FRAME_PERIOD, period);
}

void H264EncoderContext::SetTargetBitrate(unsigned rate)
{
  H264EncCtxInstance.call(SET_TARGET_BITRATE, rate);
}

void H264EncoderContext::SetFrameWidth(unsigned width)
{
  H264EncCtxInstance.call(SET_FRAME_WIDTH, width);
}

void H264EncoderContext::SetFrameHeight(unsigned height)
{
  H264EncCtxInstance.call(SET_FRAME_HEIGHT, height);
}

void H264EncoderContext::SetFrameRate(unsigned rate)
{
  H264EncCtxInstance.call(SET_FRAME_RATE, rate);
}

void H264EncoderContext::SetTSTO (unsigned tsto)
{
  H264EncCtxInstance.call(SET_TSTO, tsto);
}

void H264EncoderContext::SetProfileLevel (unsigned profile, unsigned constraints, unsigned level)
{
  unsigned profileLevel = (profile << 16) + (constraints << 8) + level;
  H264EncCtxInstance.call(SET_PROFILE_LEVEL, profileLevel);
}


void H264EncoderContext::FastUpdateRequested(void)
{
  H264EncCtxInstance.call(FASTUPDATE_REQUESTED);
}

int H264EncoderContext::EncodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags)
{
  WaitAndSignal m(_mutex);

  int ret;
  unsigned int headerLen;

  RTPFrame dstRTP(dst, dstLen);
  headerLen = dstRTP.GetHeaderSize();

  H264EncCtxInstance.call(ENCODE_FRAMES, src, srcLen, dst, dstLen, headerLen, flags, ret);

  return ret;
}

void H264EncoderContext::Lock ()
{
  _mutex.Wait();
}

void H264EncoderContext::Unlock ()
{
  _mutex.Signal();
}

H264DecoderContext::H264DecoderContext()
{
  if (!FFMPEGLibraryInstance.IsLoaded()) return;

  _gotIFrame = false;
  _gotAGoodFrame = false;
  _frameCounter = 0; 
  _skippedFrameCounter = 0;
  _rxH264Frame = new H264Frame();

  if ((_codec = FFMPEGLibraryInstance.AvcodecFindDecoder(CODEC_ID_H264)) == NULL) {
    TRACE(1, "H264\tDecoder\tCodec not found for decoder");
    return;
  }

  _context = FFMPEGLibraryInstance.AvcodecAllocContext();
  if (_context == NULL) {
    TRACE(1, "H264\tDecoder\tFailed to allocate context for decoder");
    return;
  }

  _outputFrame = FFMPEGLibraryInstance.AvcodecAllocFrame();
  if (_outputFrame == NULL) {
    TRACE(1, "H264\tDecoder\tFailed to allocate frame for encoder");
    return;
  }

  if (FFMPEGLibraryInstance.AvcodecOpen(_context, _codec) < 0) {
    TRACE(1, "H264\tDecoder\tFailed to open H.264 decoder");
    return;
  }
  else
  {
    TRACE(1, "H264\tDecoder\tDecoder successfully opened");
  }
}

H264DecoderContext::~H264DecoderContext()
{
  if (FFMPEGLibraryInstance.IsLoaded())
  {
    if (_context != NULL)
    {
      if (_context->codec != NULL)
      {
        FFMPEGLibraryInstance.AvcodecClose(_context);
        TRACE(4, "H264\tDecoder\tClosed H.264 decoder, decoded " << _frameCounter << " Frames, skipped " << _skippedFrameCounter << " Frames" );
      }
    }

    FFMPEGLibraryInstance.AvcodecFree(_context);
    FFMPEGLibraryInstance.AvcodecFree(_outputFrame);
  }
  if (_rxH264Frame) delete _rxH264Frame;
}

int H264DecoderContext::DecodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags)
{
  if (!FFMPEGLibraryInstance.IsLoaded()) return 0;

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen, 0);
  dstLen = 0;

  if (!_rxH264Frame->SetFromRTPFrame(srcRTP, flags)) {
    _rxH264Frame->BeginNewFrame();
    flags = (_gotAGoodFrame ? requestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  if (srcRTP.GetMarker()==0)
  {
    return 1;
  } 

  if (_rxH264Frame->GetFrameSize()==0)
  {
    _rxH264Frame->BeginNewFrame();
    TRACE(4, "H264\tDecoder\tGot an empty frame - skipping");
    _skippedFrameCounter++;
    flags = (_gotAGoodFrame ? requestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  TRACE_UP(4, "H264\tDecoder\tDecoding " << _rxH264Frame->GetFrameSize()  << " bytes");

  // look and see if we have read an I frame.
  if (_gotIFrame == 0)
  {
    if (!_rxH264Frame->IsSync())
    {
      TRACE(1, "H264\tDecoder\tWaiting for an I-Frame");
      _rxH264Frame->BeginNewFrame();
      flags = (_gotAGoodFrame ? requestIFrame : 0);
      _gotAGoodFrame = false;
      return 1;
    }
    _gotIFrame = 1;
  }

  int gotPicture = 0;
  uint32_t bytesUsed = 0;
  int bytesDecoded = FFMPEGLibraryInstance.AvcodecDecodeVideo(_context, _outputFrame, &gotPicture, _rxH264Frame->GetFramePtr() + bytesUsed, _rxH264Frame->GetFrameSize() - bytesUsed);

  _rxH264Frame->BeginNewFrame();
  if (!gotPicture) 
  {
    TRACE(1, "H264\tDecoder\tDecoded "<< bytesDecoded << " bytes without getting a Picture..."); 
    _skippedFrameCounter++;
    flags = (_gotAGoodFrame ? requestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  TRACE_UP(4, "H264\tDecoder\tDecoded " << bytesDecoded << " bytes"<< ", Resolution: " << _context->width << "x" << _context->height);
  int frameBytes = (_context->width * _context->height * 3) / 2;
  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)dstRTP.GetPayloadPtr();
  header->x = header->y = 0;
  header->width = _context->width;
  header->height = _context->height;

  int size = _context->width * _context->height;
  if (_outputFrame->data[1] == _outputFrame->data[0] + size
      && _outputFrame->data[2] == _outputFrame->data[1] + (size >> 2))
  {
    memcpy(OPAL_VIDEO_FRAME_DATA_PTR(header), _outputFrame->data[0], frameBytes);
  }
  else 
  {
    unsigned char *dstData = OPAL_VIDEO_FRAME_DATA_PTR(header);
    for (int i=0; i<3; i ++)
    {
      unsigned char *srcData = _outputFrame->data[i];
      int dst_stride = i ? _context->width >> 1 : _context->width;
      int src_stride = _outputFrame->linesize[i];
      int h = i ? _context->height >> 1 : _context->height;

      if (src_stride==dst_stride)
      {
        memcpy(dstData, srcData, dst_stride*h);
        dstData += dst_stride*h;
      }
      else
      {
        while (h--)
        {
          memcpy(dstData, srcData, dst_stride);
          dstData += dst_stride;
          srcData += src_stride;
        }
      }
    }
  }

  dstRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader) + frameBytes);
  dstRTP.SetTimestamp(srcRTP.GetTimestamp());
  dstRTP.SetMarker(1);
  dstLen = dstRTP.GetFrameLen();

  flags = PluginCodec_ReturnCoderLastFrame;
  if (_outputFrame->key_frame)
     flags |= PluginCodec_ReturnCoderIFrame;

  _frameCounter++;
  _gotAGoodFrame = true;
  return 1;
}

/////////////////////////////////////////////////////////////////////////////

static int get_codec_options(const struct PluginCodec_Definition * codec,
                                                  void *,
                                                  const char *,
                                                  void * parm,
                                                  unsigned * parmLen)
{
    if (parmLen == NULL || parm == NULL || *parmLen != sizeof(struct PluginCodec_Option **))
        return 0;

    *(const void **)parm = codec->userData;
    *parmLen = 0; //FIXME
    return 1;
}

static int free_codec_options ( const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  char ** strings = (char **) parm;
  for (char ** string = strings; *string != NULL; string++)
    free(*string);
  free(strings);
  return 1;
}

static int int_from_string(std::string str)
{
  if (str.find_first_of("\"") != std::string::npos)
    return (atoi( str.substr(1, str.length()-2).c_str()));

  return (atoi( str.c_str()));
}

static void profile_level_from_string  (std::string profileLevelString, unsigned & profile, unsigned & constraints, unsigned & level)
{

  if (profileLevelString.find_first_of("\"") != std::string::npos)
    profileLevelString = profileLevelString.substr(1, profileLevelString.length()-2);

  unsigned profileLevelInt = strtoul(profileLevelString.c_str(), NULL, 16);

  if (profileLevelInt == 0) {
#ifdef DEFAULT_TO_FULL_CAPABILITIES
    // Baseline, Level 3
    profileLevelInt = 0x42C01E;
#else
    // Default handling according to RFC 3984
    // Baseline, Level 1
    profileLevelInt = 0x42C00A;
#endif  
  }

  profile     = (profileLevelInt & 0xFF0000) >> 16;
  constraints = (profileLevelInt & 0x00FF00) >> 8;
  level       = (profileLevelInt & 0x0000FF);
}

static int valid_for_protocol (const struct PluginCodec_Definition * codec,
                                                  void *,
                                                  const char *,
                                                  void * parm,
                                                  unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char *))
    return 0;

  if (codec->h323CapabilityType != PluginCodec_H323Codec_NoH323)
    return (STRCMPI((const char *)parm, "h.323") == 0 ||
            STRCMPI((const char *)parm, "h323") == 0) ? 1 : 0;	        
   else 
    return (STRCMPI((const char *)parm, "sip") == 0) ? 1 : 0;
}

static int setFrameSizeAndRate(unsigned _level, bool stdaspect, unsigned & w, unsigned & h, unsigned & r, unsigned & h264level)
{
	unsigned j=0;
	unsigned maxMB = 0;
	unsigned maxMBsec = 0;

	if (_level == 0) return 0;

	// Get the Max Frame Size and rate (Macroblocks)
    while (h264_levels[j].level_idc) {
		if (h264_levels[j].h241_level == _level) {
			maxMB = h264_levels[j].frame_size;
			maxMBsec = h264_levels[j].mbps;
			h264level = h264_levels[j].level_idc;
			break;
		}
      j++; 
    }
	if (!maxMB) return 0;

	j=0;
    while (h264_resolutions[j].width) {
		if (h264_resolutions[j].macroblocks <= maxMB && stdaspect == h264_resolutions[j].stdaspect) {
			h = h264_resolutions[j].height;
			w = h264_resolutions[j].width;
			r = (int)maxMBsec/(h * w / 256);
			if (r > 30) r = 30;    // We limit the frame rate to 30 fps regardless of the max negotiated.
			break;
		}
      j++; 
    }
	return 1;
}

static int SetLevelMBPS(unsigned & level, unsigned maxMB)
{
	int j=0;
    while (h264_levels[j].level_idc) {
	if (h264_levels[j].mbps > 500*maxMB) {
            unsigned int nlevel = h264_levels[j-1].h241_level;
            if (level == 0 || nlevel < level)
                level = nlevel;
		break;
	}
      j++; 
    }
    return (level > 0);
}

static int SetLevelMFS(unsigned & level, unsigned mfs)
{
    int j=0;
    while (h264_levels[j].level_idc) {
	if ((h264_levels[j].frame_size >> 8) > mfs) {
            level = h264_levels[j-1].h241_level;
	    break;
	}
      j++; 
    }
    return (level > 0);
}

static int setLevel(unsigned w, unsigned h, unsigned r, unsigned & level, unsigned & h264level)
{	
	// Get the Max Frame Size and rate (Macroblocks)
	uint32_t nbMBsPerFrame = w * h / 256;
	uint32_t nbMBsPerSec = nbMBsPerFrame * r;

	unsigned j=0;
	level = 0;
    while (h264_levels[j].level_idc) {
		if ((nbMBsPerFrame <= h264_levels[j].frame_size) && (nbMBsPerSec <= h264_levels[j].mbps)) {
			level = h264_levels[j-1].h241_level;
			h264level = h264_levels[j-1].level_idc;
			break;
		}
		j++;
    }
	return (level > 0);
}

static int adjust_bitrate_to_level (unsigned & targetBitrate, unsigned level, int idx = -1)
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

  // Correct Target Bitrate
  if (targetBitrate == 0)
	  targetBitrate = h264_levels[i].bitrate;
  else if (targetBitrate > h264_levels[i].bitrate)
	  targetBitrate = h264_levels[i].bitrate;

  TRACE(4, "H264\tCap\tBitrate: " << targetBitrate << "(" << h264_levels[i].bitrate << ")");

  return 1;
}

static int adjust_to_level (unsigned & width, unsigned & height, unsigned & frameTime, unsigned & targetBitrate, unsigned level)
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

// Correct max. number of macroblocks per frame
  uint32_t nbMBsPerFrame = width * height / 256;
  unsigned j = 0;
  TRACE(4, "H264\tCap\tFrame Size: " << nbMBsPerFrame << "(" << h264_levels[i].frame_size << ")");
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

// Correct macroblocks per second
  uint32_t nbMBsPerSecond = width * height / 256 * (90000 / frameTime);
  TRACE(4, "H264\tCap\tMB/s: " << nbMBsPerSecond << "(" << h264_levels[i].mbps << ")");
  if (nbMBsPerSecond > h264_levels[i].mbps)
    frameTime =  (unsigned) (90000 / 256 * width  * height / h264_levels[i].mbps );

  adjust_bitrate_to_level (targetBitrate, level, i);
  return 1;
}

/////////////////////////////////////////////////////////////////////////////

static void * create_encoder(const struct PluginCodec_Definition * /*codec*/)
{
  return new H264EncoderContext;
}

static void destroy_encoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  H264EncoderContext * context = (H264EncoderContext *)_context;
  delete context;
}

static int codec_encoder(const struct PluginCodec_Definition * ,
                                           void * _context,
                                     const void * from,
                                       unsigned * fromLen,
                                           void * to,
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  H264EncoderContext * context = (H264EncoderContext *)_context;
  return context->EncodeFrames((const u_char *)from, *fromLen, (u_char *)to, *toLen, *flag);
}

static int to_normalised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
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
      if (STRCMPI(option[0], "CAP RFC3894 Profile Level") == 0) {
        profile_level_from_string(option[1], profile, constraints, level);
      }
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
        width = atoi(option[1]);
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
        height = atoi(option[1]);
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
        frameTime = atoi(option[1]);
      if (STRCMPI(option[0], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
        targetBitrate = atoi(option[1]);
  }

  TRACE(4, "H264\tCap\tProfile and Level: " << profile << ";" << constraints << ";" << level);

  // Though this is not a strict requirement we enforce 
  //it here in order to obtain optimal compression results
  width -= width % 16;
  height -= height % 16;

  if (!adjust_to_level (width, height, frameTime, targetBitrate, level))
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


static int to_customised_options(const struct PluginCodec_Definition * codec, void * _context, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  unsigned orgframeWidth = codec->parm.video.maxFrameWidth;
  unsigned orgframeHeight = codec->parm.video.maxFrameHeight;

  int i = 0;
  unsigned frameWidth = 0;
  unsigned frameHeight = 0;
  unsigned frameRate = 0;
  unsigned level = 0;
  unsigned h264level = 0;
  unsigned targetBitrate = 0;

    const char ** option = (const char **)parm;
    for (i = 0; option[i] != NULL; i += 2) {
	  if (STRCMPI(option[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
	      frameWidth = atoi(option[i+1]);
	  if (STRCMPI(option[i], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
	      frameHeight = atoi(option[i+1]);
	  if (STRCMPI(option[i], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
	      frameRate = (unsigned)(H264_CLOCKRATE / atoi(option[i+1]));
	  if (STRCMPI(option[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
	      targetBitrate = atoi(option[i+1]);
	}

	// Make sure the custom size does not exceed the codec limits
	if (frameWidth > orgframeWidth || frameHeight > orgframeHeight)
	  return 0;
  
	if (!setLevel(frameWidth,frameHeight,frameRate,level,h264level) && !adjust_bitrate_to_level(targetBitrate, h264level))
	  return 0;

	char ** options = (char **)parm;
	if (options == NULL) return 0;
	for (i = 0; options[i] != NULL; i += 2) {
	  if (STRCMPI(options[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
		options[i+1] = num2str(targetBitrate);	
	  if (STRCMPI(options[i], PLUGINCODEC_OPTION_LEVEL) == 0)
		options[i+1] = num2str(level);
	}

	if (_context != NULL) {
		H264EncoderContext * context = (H264EncoderContext *)_context;
		context->Lock();
		context->SetProfileLevel(66, 0, h264level); 
		context->SetFrameWidth(frameWidth);
		context->SetFrameHeight(frameHeight);
		context->SetFrameRate(frameRate); 
		context->SetTargetBitrate((unsigned) (targetBitrate / 1000) );
		context->ApplyOptions();
		context->Unlock();
	}

  return 1;
}

static int encoder_set_options(
      const struct PluginCodec_Definition * codec, 
      void * _context, 
      const char *, 
      void * parm, 
      unsigned * parmLen)
{
  if (_context == NULL || parmLen == NULL || *parmLen != sizeof(const char **)) 
    return 0;

  H264EncoderContext * context = (H264EncoderContext *)_context;

  context->Lock();
  unsigned profile = 66;
  unsigned constraints = 0;
  unsigned level = 0; //51;
  unsigned orgframeWidth = codec->parm.video.maxFrameWidth;
  unsigned frameWidth = orgframeWidth;
  unsigned orgframeHeight = codec->parm.video.maxFrameHeight;
  unsigned frameHeight = orgframeHeight;
  unsigned orgframeRate = codec->parm.video.maxFrameRate;
  unsigned frameRate = orgframeRate;
  unsigned targetBitrate = codec->bitsPerSec;
  unsigned h264level = 36;
  unsigned cusMBPS = 0;   // Custom maximum MBPS
  unsigned cusMFS = 0;    // Custom maximum FrameSize
  unsigned staticMBPS = 0;  /// static macroblocks per sec
  bool stdAspect = true;

  if (parm != NULL) {
    const char ** options = (const char **)parm;
    int i;
    for (i = 0; options[i] != NULL; i += 2) {
      if (STRCMPI(options[i], "CAP RFC3894 Profile Level") == 0) {
        profile_level_from_string (options[i+1], profile, constraints, h264level);
      }
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
         targetBitrate = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
         frameRate = (unsigned)(H264_CLOCKRATE / atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
         frameHeight = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
         frameWidth = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_MAX_FRAME_SIZE) == 0)
         context->SetMaxRTPFrameSize(atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TX_KEY_FRAME_PERIOD) == 0)
         context->SetMaxKeyFramePeriod (atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_TEMPORAL_SPATIAL_TRADE_OFF) == 0)
         context->SetTSTO (atoi(options[i+1]));
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_LEVEL) == 0)
         level = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_CUSMBPS) == 0)
         cusMBPS = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_CUSMFS) == 0)
         cusMFS = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_STATICMBPS) == 0)
         staticMBPS = atoi(options[i+1]);
      if (STRCMPI(options[i], PLUGINCODEC_OPTION_ASPECT) == 0)
		 stdAspect = (STRCMPI(options[i+1],"2") == 0);  // 2 is scale to stdSize
	}
  }

    if (cusMBPS > 0 && SetLevelMBPS(level,cusMBPS) && SetLevelMFS(level,cusMFS)) {
        frameRate = cusMBPS/cusMFS;
	TRACE(4, "H264\tCap\tCustom level : " << level << " rate " << frameRate);
    }

    if (level > 0) {
	TRACE(1, "H264\tCap\tProfile and Level: " << profile << ";" << constraints << ";" << level);
	// Adjust to level sent in the signalling
	if (!setFrameSizeAndRate(level,stdAspect,frameWidth,frameHeight,frameRate,h264level) || !adjust_bitrate_to_level(targetBitrate, h264level)) {
	     context->Unlock();
	     return 0;
	}

	// adjust the image to the frame limits
	if ((frameHeight > orgframeHeight) || (frameWidth > orgframeWidth)) {
	  if (setLevel(orgframeWidth,orgframeHeight,orgframeRate, level,h264level)) {
	    if (!setFrameSizeAndRate(level,stdAspect,frameWidth,frameHeight,frameRate,h264level) || !adjust_bitrate_to_level(targetBitrate, h264level)) {
	       context->Unlock();
	       return 0;
	    }
	  }
	}

	// Write back the option list the changed information to the level
	if (parm != NULL) {
		char ** options = (char **)parm;
	    if (options == NULL) return 0;
	    for (int i = 0; options[i] != NULL; i += 2) {
		  if (STRCMPI(options[i], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
			 options[i+1] = num2str(targetBitrate);	
		  if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
			 options[i+1] = num2str(H264_CLOCKRATE/frameRate);
		  if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
			 options[i+1] = num2str(frameHeight);
		  if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
			 options[i+1] = num2str(frameWidth);
		  if (STRCMPI(options[i], PLUGINCODEC_OPTION_LEVEL) == 0)
			 options[i+1] = num2str(level);
		}
	}

	// Set the Values in the codec
        context->SetTargetBitrate((unsigned) (targetBitrate / 1000) );
        context->SetProfileLevel(profile, constraints, h264level); 
	    context->SetFrameHeight(frameHeight);
	    context->SetFrameWidth(frameWidth);
	    context->SetFrameRate(frameRate); 
    } else {
        context->SetTargetBitrate((unsigned) (targetBitrate / 1000) );
        context->SetProfileLevel(profile, constraints, h264level); 
    }
    context->ApplyOptions();
    context->Unlock();

  return 1;
}

static int encoder_event_handler(const struct PluginCodec_Definition * codec, 
	  void * _context, 
	  const char *, 
      void * parm, 
      unsigned * parmLen)
{
  if (_context == NULL || parmLen == NULL || *parmLen != sizeof(const char **)) 
    return 0;

  H264EncoderContext * context = (H264EncoderContext *)_context;
  context->Lock();
  if (parm != NULL) {
	char ** parms = (char **)parm;
	 if (parms == NULL) return 0;
	    for (int i = 0; parms[i] != NULL; i += 2) {
		  if (STRCMPI(parms[i], PLUGINCODEC_EVENT_FASTUPDATE) == 0) {
                     TRACE(4, "H264\tEvt\tFAST PICTURE UPDATE:");
			 context->FastUpdateRequested();
                  }	
		  if (STRCMPI(parms[i], PLUGINCODEC_EVENT_FLOWCONTROL) == 0) {
                     TRACE(4, "H264\tEvt\tFLOW CONTROL: " << parms[i+1]);
                  }
 /*		  if (STRCMPI(parms[i], PLUGINCODEC_EVENT_LOSTPARTIAL) == 0)
			 // do something
		  if (STRCMPI(parms[i], PLUGINCODEC_EVENT_LOSTPICTURE) == 0)
			 // do something */
	    }
  }
  context->Unlock();

  return 1;

}

static int encoder_get_output_data_size(const PluginCodec_Definition *, void *, const char *, void *, unsigned *)
{
  return 2000; //FIXME
}
/////////////////////////////////////////////////////////////////////////////

static void * create_decoder(const struct PluginCodec_Definition *)
{
  return new H264DecoderContext;
}

static void destroy_decoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  H264DecoderContext * context = (H264DecoderContext *)_context;
  delete context;
}

static int codec_decoder(const struct PluginCodec_Definition *, 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  H264DecoderContext * context = (H264DecoderContext *)_context;
  return context->DecodeFrames((const u_char *)from, *fromLen, (u_char *)to, *toLen, *flag);
}

static int decoder_get_output_data_size(const PluginCodec_Definition * codec, void *, const char *, void *, unsigned *)
{
  return sizeof(PluginCodec_Video_FrameHeader) + ((codec->parm.video.maxFrameWidth * codec->parm.video.maxFrameHeight * 3) / 2);
}

/////////////////////////////////////////////////////////////////////////////
static int merge_profile_level_h264(char ** result, const char * dst, const char * src)
{
  // c0: obeys A.2.1 Baseline
  // c1: obeys A.2.2 Main
  // c2: obeys A.2.3, Extended
  // c3: if profile_idc profile = 66, 77, 88, and level =11 and c3: obbeys annexA for level 1b

  unsigned srcProfile, srcConstraints, srcLevel;
  unsigned dstProfile, dstConstraints, dstLevel;
  profile_level_from_string(src, srcProfile, srcConstraints, srcLevel);
  profile_level_from_string(dst, dstProfile, dstConstraints, dstLevel);

  switch (srcLevel) {
    case 10:
      srcLevel = 8;
      break;
    default:
      break;
  }

  switch (dstLevel) {
    case 10:
      dstLevel = 8;
      break;
    default:
      break;
  }

  if (dstProfile > srcProfile) {
    dstProfile = srcProfile;
  }

  dstConstraints |= srcConstraints;

  if (dstLevel > srcLevel)
    dstLevel = srcLevel;

  switch (dstLevel) {
    case 8:
      dstLevel = 10;
      break;
    default:
      break;
  }


  char buffer[10];
  sprintf(buffer, "%x", (dstProfile<<16)|(dstConstraints<<8)|(dstLevel));
  
  *result = strdup(buffer);

  TRACE(4, "H264\tCap\tCustom merge profile-level: " << src << " and " << dst << " to " << *result);

  return true;
}

static int merge_packetization_mode(char ** result, const char * dst, const char * src)
{
  unsigned srcInt = int_from_string (src); 
  unsigned dstInt = int_from_string (dst); 

  if (srcInt == 5) {
#ifdef DEFAULT_TO_FULL_CAPABILITIES
    srcInt = 1;
#else
    // Default handling according to RFC 3984
    srcInt = 0;
#endif
  }

  if (dstInt == 5) {
#ifdef DEFAULT_TO_FULL_CAPABILITIES
    dstInt = 1;
#else
    // Default handling according to RFC 3984
    dstInt = 0;
#endif
  }

  if (dstInt > srcInt)
    dstInt = srcInt;

  char buffer[10];
  sprintf(buffer, "%d", dstInt);
  
  *result = strdup(buffer);

  TRACE(4, "H264\tCap\tCustom merge packetization-mode: " << src << " and " << dst << " to " << *result);

//  if (dstInt > 0)
    return 1;

//  return 0;
}

static void free_string(char * str)
{
  free(str);
}

/////////////////////////////////////////////////////////////////////////////

extern "C" {

PLUGIN_CODEC_IMPLEMENT(H264)

PLUGIN_CODEC_DLL_API struct PluginCodec_Definition * PLUGIN_CODEC_GET_CODEC_FN(unsigned * count, unsigned version)
{

  char * debug_level = getenv ("PTLIB_TRACE_CODECS");
  if (debug_level!=NULL) {
    Trace::SetLevel(atoi(debug_level));
  } 
  else {
    Trace::SetLevel(0);
  }

  debug_level = getenv ("PTLIB_TRACE_CODECS_USER_PLANE");
  if (debug_level!=NULL) {
    Trace::SetLevelUserPlane(atoi(debug_level));
  } 
  else {
    Trace::SetLevelUserPlane(0);
  }

  if (!FFMPEGLibraryInstance.Load()) {
    *count = 0;
    TRACE(1, "H264\tCodec\tDisabled");
    return NULL;
  }

  FFMPEGLibraryInstance.AvLogSetLevel(AV_LOG_DEBUG);
  FFMPEGLibraryInstance.AvLogSetCallback(&logCallbackFFMPEG);

  if (version < PLUGIN_CODEC_VERSION_OPTIONS) {
    *count = 0;
    TRACE(1, "H264\tCodec\tDisabled - plugin version mismatch");
    return NULL;
  }
  else {
    *count = sizeof(h264CodecDefn) / sizeof(struct PluginCodec_Definition);
    TRACE(1, "H264\tCodec\tEnabled");
    return h264CodecDefn;
  }

}
};
