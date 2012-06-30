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

#ifndef __ENC_CTX_H__
#define __ENC_CTX_H__ 1

#include <stdarg.h>

#if defined(_WIN32)
#include "../../common/critsect.h"
#if _MSC_VER < 1600
#include "../../common/vs-stdint.h"
#endif
#else
#include <stdint.h>
#include "../common/critsect.h"
#endif

#include "../shared/h264frame.h"

extern "C" {
  #include <x264.h>
};

#if X264_BUILD < 80
#if _WIN32
#pragma message("X264 Build > 79 required for single NAL support")
#else
#warning("X264 Build > 79 required for single NAL support")
#endif
#endif

#if defined(_WIN32) || defined(_STATIC_LINK)
/* to keep compatibility with old build */
#define X264_LINK_STATIC 1
#define X264_DELAYLOAD   1
#endif

#define CIF_WIDTH 352
#define CIF_HEIGHT 288
#define QCIF_WIDTH 176
#define QCIF_HEIGHT 144
#define IT_QCIF 0
#define IT_CIF 1

#define H264_BITRATE         768000
#define H264_PAYLOAD_SIZE      1400
#define H264_SINGLE_NAL_SIZE   1400
#define H264_FRAME_RATE          30
#define H264_KEY_FRAME_INTERVAL  (30*60) // Send a key frame no more than once every minute (unless requested through fast update)
#define H264_PROFILE_LEVEL       ((66 << 16) + (0xC0 << 8) +  30)
#define H264_TSTO                31
#define H264_MIN_QUANT           10

#if X264_LINK_STATIC
  #define X264_ENCODER_OPEN x264_encoder_open
  #define X264_PARAM_DEFAULT x264_param_default
  #define X264_ENCODER_ENCODE x264_encoder_encode
  #define X264_NAL_ENCODE x264_nal_encode
  #define X264_ENCODER_RECONFIG x264_encoder_reconfig
  #define X264_ENCODER_HEADERS x264_encoder_headers
  #define X264_ENCODER_CLOSE x264_encoder_close
  #if X264_BUILD >= 98
  #define X264_PICTURE_INIT x264_picture_init
  #endif
#else
#if defined(_WIN32) 
  #include "x264loader_win32.h"
#else
  #include "x264loader_unix.h"
#endif
  #define X264_ENCODER_OPEN  X264Lib.Xx264_encoder_open
  #define X264_PARAM_DEFAULT X264Lib.Xx264_param_default
  #define X264_ENCODER_ENCODE X264Lib.Xx264_encoder_encode
  #define X264_NAL_ENCODE X264Lib.Xx264_nal_encode
  #define X264_ENCODER_RECONFIG X264Lib.Xx264_encoder_reconfig
  #define X264_ENCODER_HEADERS X264Lib.Xx264_encoder_headers
  #define X264_ENCODER_CLOSE X264Lib.Xx264_encoder_close
  #if X264_BUILD >= 98
  #define X264_PICTURE_INIT X264Lib.Xx264_picture_init
  #endif
#endif

class X264EncoderContext 
{
  public:
    X264EncoderContext ();
    ~X264EncoderContext ();

    bool Initialise();
    void Uninitialise();

    int EncodeFrames (const unsigned char * src, unsigned & srcLen, unsigned char * dst, unsigned & dstLen, unsigned int & flags);

    void fastUpdateRequested(void);
    
    void SetMaxRTPFrameSize (unsigned size);
    void SetMaxKeyFramePeriod (unsigned period);
    void SetTargetBitrate (unsigned rate);
    void SetFrameWidth (unsigned width);
    void SetFrameHeight (unsigned height);
    void SetFrameRate (unsigned rate);
    void SetTSTO (unsigned tsto);
    void SetProfileLevel (unsigned profileLevel);
    void SetMaxNALSize (unsigned size);
    void ApplyOptions ();


  protected:

    x264_t* _codec;
    x264_param_t _context;
    x264_picture_t _inputFrame;
    H264Frame* _txH264Frame;

    uint32_t _PFramesSinceLastIFrame; // counts frames since last keyframe
    uint32_t _IFrameInterval; // confd frames between keyframes
    int _frameCounter;
    bool _fastUpdateRequested;

} ;


#endif /* __ENC_CTX_H__ */
