/*
 * H.264 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) 2007 Matthias Schneider, All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "plugin-config.h"

#include "enc-ctx.h"

#ifdef _MSC_VER
	#include "../../common/rtpframe.h"
	#include "../../common/trace.h"
#else
	#include "trace.h"
	#include "rtpframe.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#if defined(_WIN32) || defined(_WIN32_WCE)
  #include <malloc.h>
  #define STRCMPI  _strcmpi
#else
  #include <semaphore.h>
  #define STRCMPI  strcasecmp
#endif
#include <string.h>

#ifndef X264_LINK_STATIC
X264Library X264Lib;
#endif

static void logCallbackX264 (void * /*priv*/, int level, const char *fmt, va_list arg) {
  const unsigned BUFSIZE = 2048;
  char buffer[BUFSIZE];
  int severity = 0;
  switch (level) {
    case X264_LOG_NONE:    severity = 1; break;
    case X264_LOG_ERROR:   severity = 1; break;
    case X264_LOG_WARNING: severity = 3; break;
    case X264_LOG_INFO:    severity = 4; break;
    case X264_LOG_DEBUG:   severity = 4; break;
    default:               severity = 4; break;
  }
  sprintf(buffer, "H264\tx264\t"); 
  vsnprintf(buffer + strlen(buffer), BUFSIZE - strlen(buffer), fmt, arg);
  if (strlen(buffer) > 0)
    buffer[strlen(buffer)-1] = 0;
  if (severity == 4)
    { TRACE_UP (severity, buffer); }
  else
    { TRACE (severity, buffer); }
}

X264EncoderContext::X264EncoderContext()
: _codec(NULL), _txH264Frame(NULL), _PFramesSinceLastIFrame(0),
  _IFrameInterval(0), _frameCounter(0), _fastUpdateRequested(false)
{
    Initialise();
}

X264EncoderContext::~X264EncoderContext()
{
   Uninitialise();
}

bool X264EncoderContext::Initialise()
{
   Uninitialise();

  _frameCounter = 0;
  _PFramesSinceLastIFrame = 0;
  _fastUpdateRequested = false;

  _txH264Frame = new H264Frame();
  _txH264Frame->SetMaxPayloadSize(H264_PAYLOAD_SIZE);

  _inputFrame.i_type 			= X264_TYPE_AUTO;
  _inputFrame.i_qpplus1 		= 0;
  _inputFrame.img.i_csp 		= X264_CSP_I420;

#if X264_BUILD > 101
  _inputFrame.prop.quant_offsets = NULL;
  _inputFrame.prop.quant_offsets_free = NULL;
#endif
 
   X264_PARAM_DEFAULT(&_context);

   // No multicore support
   _context.i_threads           = 1;
   _context.b_sliced_threads    = 1;
   _context.b_deterministic     = 1;
   _context.i_sync_lookahead    = 0;
   _context.i_frame_reference   = 1;
   _context.i_bframe            = 0;
   _context.b_vfr_input         = 0;
   _context.rc.b_mb_tree        = 0;
 
  // No aspect ratio correction 
  _context.vui.i_sar_width      = 0;
  _context.vui.i_sar_height     = 0;
  
#if X264_BUILD > 101
  // No automatic keyframe generation
  _context.i_keyint_max               = 90; // X264_KEYINT_MAX_INFINITE;
  _context.i_keyint_min               = 15; // X264_KEYINT_MAX_INFINITE;
#endif

  // Enable logging
  _context.pf_log               = logCallbackX264;
  _context.i_log_level          = X264_LOG_WARNING; //X264_LOG_DEBUG;
  _context.p_log_private        = NULL;

  // Single NAL Mode
#if X264_BUILD > 79
  _context.i_slice_max_size     = H264_SINGLE_NAL_SIZE;   // Default NAL Size
  _context.b_repeat_headers     = 1;     // repeat SPS/PPS before each key frame
  _context.b_annexb             = 1;     // place start codes (4 bytes) before NAL units   
#endif

  SetFrameWidth       (CIF_WIDTH);
  SetFrameHeight      (CIF_HEIGHT);
  SetFrameRate        (H264_FRAME_RATE);
  SetProfileLevel     (H264_PROFILE_LEVEL);
  SetTSTO             (H264_TSTO); // TODO: is this really functional
  SetMaxKeyFramePeriod(H264_KEY_FRAME_INTERVAL);

#ifdef H264_OPTIMAL_QUALITY
   // Rate control set to CRF mode (ignoring bitrate restrictions)
  _context.rc.i_rc_method       	= X264_RC_CRF;
  _context.rc.f_rf_constant       	= 16.0;	// great quality
#else
   // Rate control set to ABR mode
#if 1
    _context.rc.i_rc_method            = X264_RC_ABR;
    _context.rc.f_rate_tolerance       = 1.0;
    _context.rc.i_lookahead            = 0;
    _context.rc.i_qp_step              = 6;
    _context.rc.psz_stat_out           = 0;
    _context.rc.psz_stat_in            = 0;
    _context.rc.f_vbv_buffer_init      = 0;
    _context.i_scenecut_threshold      = 0;
#else
  _context.rc.i_rc_method            = X264_RC_ABR;
  _context.rc.i_qp_min              = 25;
  _context.rc.i_qp_max              = 51;
  _context.rc.i_qp_step             = 6;
  _context.rc.f_rate_tolerance  	= 1.0;
  _context.rc.i_vbv_max_bitrate 	= 0;
  _context.rc.psz_stat_out          = 0;
  _context.rc.psz_stat_in           = 0;
  _context.rc.f_vbv_buffer_init     = 0;
  _context.rc.i_vbv_buffer_size 	= 0;
  _context.rc.i_lookahead       	= 0;
  _context.i_scenecut_threshold     = 0;
#endif
  SetTargetBitrate    ((unsigned)(H264_BITRATE / 1000));
#endif

  // Analysis support
  _context.analyse.intra     		= 3;
  _context.analyse.inter     		= 0;
  _context.analyse.b_transform_8x8 	= 0;
  _context.analyse.i_weighted_pred  = 0;
  _context.analyse.i_direct_mv_pred = 1;
  _context.analyse.i_me_method      = 0;
  _context.analyse.i_me_range       = 16;
  _context.analyse.i_subpel_refine  = 1;
  _context.analyse.i_trellis        = 0;
  _context.analyse.b_psnr           = 0;
  _context.analyse.b_fast_pskip     = 1;
  _context.analyse.b_dct_decimate   = 1;
  _context.analyse.i_noise_reduction= 0;
  _context.analyse.b_ssim           = 0;

#ifndef X264_DELAYLOAD
  _codec = X264_ENCODER_OPEN(&_context);
  if (_codec == NULL) {
    TRACE(1, "H264\tEncoder\tCouldn't init x264 encoder");
    return false;
  } else {
    TRACE(4, "H264\tEncoder\tx264 encoder successfully opened");
  }
#endif
  return true;
}

void X264EncoderContext::Uninitialise()
{
  if (_codec != NULL)
  {
      X264_ENCODER_CLOSE(_codec);
      TRACE(4, "H264\tEncoder\tClosed H.264 encoder, encoded " << _frameCounter << " Frames" );
      _codec = NULL;
  }
  if (_txH264Frame) delete _txH264Frame;
}

void X264EncoderContext::SetMaxRTPFrameSize(unsigned size)
{
  _txH264Frame->SetMaxPayloadSize(size);
}

void X264EncoderContext::SetMaxKeyFramePeriod (unsigned period)
{
    _IFrameInterval = _context.i_keyint_max = period;
    _PFramesSinceLastIFrame = _IFrameInterval + 1; // force a keyframe on the first frame
    TRACE(4, "H264\tEncoder\tx264 encoder key frame period set to " << period);
}

void X264EncoderContext::SetTargetBitrate(unsigned rate)
{
    _context.rc.i_bitrate = rate;
    TRACE(4, "H264\tEncoder\tx264 encoder bitrate set to " << rate);
}

void X264EncoderContext::SetFrameWidth(unsigned width)
{
    _context.i_width = width;
    TRACE(4, "H264\tEncoder\tx264 encoder width set to " << width);
}

void X264EncoderContext::SetFrameHeight(unsigned height)
{
    _context.i_height = height;
    TRACE(4, "H264\tEncoder\tx264 encoder height set to " << height);
}

void X264EncoderContext::SetFrameRate(unsigned rate)
{
    _context.i_fps_num = rate; 
    _context.i_fps_den = 1;
    TRACE(4, "H264\tEncoder\tx264 encoder frame rate set to " << (_context.i_fps_num/_context.i_fps_den));
}

void X264EncoderContext::SetTSTO (unsigned tsto)
{
    _context.rc.i_qp_min = H264_MIN_QUANT;
    if (tsto > 0)
      _context.rc.i_qp_max =  (int)((51 - H264_MIN_QUANT) / 31 * tsto + H264_MIN_QUANT);
    _context.rc.i_qp_step = 4;	
    TRACE(4, "H264\tEncoder\tx264 encoder QP range rate set to [" << _context.rc.i_qp_min << "-" << _context.rc.i_qp_max << "] with a step of " << _context.rc.i_qp_step);
}

void X264EncoderContext::SetProfileLevel (unsigned profileLevel)
{
//  unsigned profile = (profileLevel & 0xff0000) >> 16;
//  bool constraint0 = (profileLevel & 0x008000) ? true : false;
//  bool constraint1 = (profileLevel & 0x004000) ? true : false;
//  bool constraint2 = (profileLevel & 0x002000) ? true : false;
//  bool constraint3 = (profileLevel & 0x001000) ? true : false;
  unsigned level   = (profileLevel & 0x0000ff);

  int i = 0;
  while (h264_levels[i].level_idc) {
    if (h264_levels[i].level_idc == level)
      break;
   i++; 
  }

  if (!h264_levels[i].level_idc) {
    TRACE(1, "H264\tCap\tIllegal Level negotiated");
    return;
  }

  // We make use of the baseline profile, that means:
  // no B-Frames (too much latency in interactive video)
  // CBR (we want to get the max. quality making use of all the bitrate that is available)
  // baseline profile begin
  _context.b_cabac = 0;  // Only >= MAIN LEVEL
  _context.i_bframe = 0; // Only >= MAIN LEVEL

  // Level:
  _context.i_level_idc = level;
  
  TRACE(4, "H264\tEncoder\tx264 encoder level set to " << _context.i_level_idc);
}

void X264EncoderContext::ApplyOptions()
{

#if X264_DELAYLOAD
  return;  // We apply options when we do our first encode
#else
  if (_codec != NULL)
    X264_ENCODER_CLOSE(_codec);

  _codec = X264_ENCODER_OPEN(&_context);
  if (_codec == NULL) {
    TRACE(1, "H264\tEncoder\tCouldn't init x264 encoder");
  } 
  TRACE(4, "H264\tEncoder\tx264 encoder successfully opened");
#endif

}

void X264EncoderContext::fastUpdateRequested(void)
{
  _fastUpdateRequested = true; 
}

void X264EncoderContext::SetMaxNALSize (unsigned size)
{
  _txH264Frame->SetMaxPayloadSize(size);
  _context.i_slice_max_size  = size;
}

int X264EncoderContext::EncodeFrames(const unsigned char * src, unsigned & srcLen, unsigned char * dst, unsigned & dstLen, unsigned int & flags)
{

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen);

  dstLen = 0;

  // from here, we are encoding a new frame
  if (
#ifndef X264_DELAYLOAD
     (!_codec) ||
#endif
     (!_txH264Frame))
  {
    return 0;
  }

  // if there are RTP packets to return, return them
  if  (_txH264Frame->HasRTPFrames())
  {
    _txH264Frame->GetRTPFrame(dstRTP, flags);
    dstLen = dstRTP.GetFrameLen();
    return 1;
  }

  if (srcRTP.GetPayloadSize() < sizeof(frameHeader))
  {
   TRACE(1, "H264\tEncoder\tVideo grab too small, Close down video transmission thread");
   return 0;
  }

  frameHeader * header = (frameHeader *)srcRTP.GetPayloadPtr();
  if (header->x != 0 || header->y != 0)
  {
    TRACE(1, "H264\tEncoder\tVideo grab of partial frame unsupported, Close down video transmission thread");
    return 0;
  }

  x264_nal_t* NALs;
  int numberOfNALs = 0;

  // do a validation of size
  // if the incoming data has changed size, tell the encoder
  if (!_codec || (unsigned)_context.i_width != header->width || (unsigned)_context.i_height != header->height)
  {
    if (_codec)
        X264_ENCODER_CLOSE(_codec);
    _context.i_width = header->width;
    _context.i_height = header->height;
    _codec = X264_ENCODER_OPEN(&_context);
    if (_codec == NULL) {
          TRACE(1, "H264\tEncoder\tCouldn't init x264 encoder");
          return 0;
    } 
#if X264_DELAYLOAD
      TRACE(4, "H264\tEncoder\tx264 encoder successfully opened");
#endif
  } 

  bool wantIFrame = _fastUpdateRequested;
  _fastUpdateRequested = false;
  
  x264_picture_t dummyOutput;
#if X264_BUILD >= 98
  // starting with build 98 applications who allocate a x264_picture_t themselves have to call x264_picture_init()
  X264_PICTURE_INIT(&dummyOutput);
#endif

  // Check whether to insert a keyframe 
  // (On the first frame and every_IFrameInterval)
  _PFramesSinceLastIFrame++;
  if (_PFramesSinceLastIFrame >= _IFrameInterval)
  {
    wantIFrame = true;
    _PFramesSinceLastIFrame = 0;
  }
  // Prepare the frame to be encoded
  _inputFrame.img.plane[0] = (uint8_t *)(((unsigned char *)header) + sizeof(frameHeader));
  _inputFrame.img.i_stride[0] = header->width;
  _inputFrame.img.plane[1] = (uint8_t *)((((unsigned char *)header) + sizeof(frameHeader)) 
                           + (int)(_inputFrame.img.i_stride[0]*header->height));
  _inputFrame.img.i_stride[1] = 
  _inputFrame.img.i_stride[2] = (int) ( header->width / 2 );
  _inputFrame.img.plane[2] = (uint8_t *)(_inputFrame.img.plane[1] + (int)(_inputFrame.img.i_stride[1] *header->height/2));
  _inputFrame.i_type = (wantIFrame || (flags & forceIFrame)) ? X264_TYPE_IDR : X264_TYPE_AUTO;
  _inputFrame.i_pts = _frameCounter;	// x264 needs a time reference

#if X264_BUILD > 79
  _inputFrame.param = NULL; // &_context;
#endif
//TRACE (1,"H264\tEncoder\t" << numberOfNALs);
  while (numberOfNALs==0) { // workaround for first 2 packets being 0
    if (X264_ENCODER_ENCODE(_codec, &NALs, &numberOfNALs, &_inputFrame, &dummyOutput) < 0) {
      TRACE (1,"H264\tEncoder\tEncoding failed");
      return 0;
    } 
  }
  
  _txH264Frame->BeginNewFrame();
#ifndef LICENCE_MPL
  _txH264Frame->SetFromFrame(NALs, numberOfNALs);
#endif
  _txH264Frame->SetTimestamp(srcRTP.GetTimestamp());	// BUG: not set in srcRTP
  _frameCounter++; 

  if (_txH264Frame->HasRTPFrames())
  {
    _txH264Frame->GetRTPFrame(dstRTP, flags);
    dstLen = dstRTP.GetFrameLen();
    return 1;
  }
  return 1;
}
