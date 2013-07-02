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
 *                 Simon Horne (s.horne@packetizer.com)
 *
 *
 */

/*
  Notes
  -----

 */

#ifndef __H264_X264_H__
#define __H264_X264_H__ 1

#include "plugin-config.h"

#include <stdarg.h>

#ifndef _SIGNAL_ONLY

#if defined(_WIN32) && _MSC_VER < 1600
 #include "../common/vs-stdint.h"
 #include "../common/critsect.h"
#else
 #include <stdint.h>
 #include "critsect.h"
#endif

#include "shared/h264frame.h"

#if defined(H323_STATIC_H264)
#include "h264pipe_static.h" 
#else
#ifdef WIN32
#include "h264pipe_win32.h"
#else
#include "h264pipe_unix.h"
#endif
#endif

#include <list>


extern "C" {
#ifdef _MSC_VER
  #include "libavcodec/avcodec.h"
#else
  #include LIBAVCODEC_HEADER
#endif
};

#endif

#define P1080_WIDTH 1920
#define P1080_HEIGHT 1080
#define P720_WIDTH 1280
#define P720_HEIGHT 720
#define CIF4_WIDTH 704
#define CIF4_HEIGHT 576
#define VGA_WIDTH 640
#define VGA_HEIGHT 480
#define CIF_WIDTH 352
#define CIF_HEIGHT 288
#define QCIF_WIDTH 176
#define QCIF_HEIGHT 144
#define SQCIF_WIDTH 128
#define SQCIF_HEIGHT 96
#define IT_QCIF 0
#define IT_CIF 1

#ifndef _SIGNAL_ONLY

typedef unsigned char u_char;

static void logCallbackFFMPEG (void* v, int level, const char* fmt , va_list arg);

// Input formats from the input device.
struct inputFormats 
{
  unsigned mb;   
  unsigned w;
  unsigned h;
  unsigned r;
};

    // Settings
static double minFPS   = 13.5;       // Minimum FPS allowed
static double minSpeedFPS = 20;     // Minimum speed for Emphasis Speed
static double kbtoMBPS = 12.2963;   // Magical Conversion factor from kb/s to MBPS

class H264EncoderContext 
{
  public:
    H264EncoderContext ();
    ~H264EncoderContext ();

    int EncodeFrames (const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags);

    void FastUpdateRequested(void);
    
    void SetMaxRTPFrameSize (unsigned size);
    void SetMaxKeyFramePeriod (unsigned period);
    void SetTargetBitrate (unsigned rate);
    void SetFrameWidth (unsigned width);
    void SetFrameHeight (unsigned height);
    void SetFrameRate (unsigned rate);
    void SetTSTO (unsigned tsto);
    void SetProfileLevel (unsigned profile, unsigned constraints, unsigned level);
    void SetMaxNALSize (unsigned size);
    void SetEmphasisSpeed (bool speed);
    void ApplyOptions ();
    void Lock ();
    void Unlock ();

    void AddInputFormat(inputFormats & fmt);
    int GetInputFormat(inputFormats & fmt);
    void ClearInputFormat();

    void SetMaxMB(unsigned mb);
    unsigned GetMaxMB();
    void SetMaxMBPS(unsigned mbps);
    unsigned GetMaxMBPS();
    bool GetEmphasisSpeed();

  protected:
    CriticalSection _mutex;
    H264EncCtx H264EncCtxInstance;

    unsigned maxMBPS;
    unsigned maxMB;
    bool emphasisSpeed;
    std::list<inputFormats> videoInputFormats;
};

class H264DecoderContext
{
  public:
    H264DecoderContext();
    ~H264DecoderContext();
    int DecodeFrames(const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned int & flags);

  protected:
    CriticalSection _mutex;

    AVCodec* _codec;
    AVCodecContext* _context;
    AVFrame* _outputFrame;
    H264Frame* _rxH264Frame;

    bool _gotIFrame;
    bool _gotAGoodFrame;
	unsigned _lastTimeStamp;
    unsigned _lastSeqNo;
    int _frameCounter;
    int _frameFPUInt;
    int _frameAutoFPU;
    int _skippedFrameCounter;
};

static int valid_for_protocol    ( const struct PluginCodec_Definition *, void *, const char *,
                                   void * parm, unsigned * parmLen);
static int get_codec_options     ( const struct PluginCodec_Definition * codec, void *, const char *, 
                                   void * parm, unsigned * parmLen);
static int free_codec_options    ( const struct PluginCodec_Definition *, void *, const char *, 
                                   void * parm, unsigned * parmLen);

static int to_normalised_options ( const struct PluginCodec_Definition *, void *, const char *,
                                   void * parm, unsigned * parmLen);
static int to_customised_options ( const struct PluginCodec_Definition *, void *, const char *, 
                                   void * parm, unsigned * parmLen);
static int encoder_set_options   ( const struct PluginCodec_Definition *, void * _context, const char *, 
                                   void * parm, unsigned * parmLen);
static int encoder_event_handler(const struct PluginCodec_Definition * codec, void * _context, const char *, 
                                   void * parm, unsigned * parmLen);
static int encoder_flowcontrol   ( const struct PluginCodec_Definition *, void *, const char *, 
                                   void * parm, unsigned * parmLen);
static int encoder_formats       ( const struct PluginCodec_Definition *, void *, const char *, 
                                   void * parm, unsigned * parmLen);
static int encoder_get_output_data_size ( const PluginCodec_Definition *, void *, const char *,
                                   void *, unsigned *);
static int decoder_get_output_data_size ( const PluginCodec_Definition * codec, void *, const char *,
                                   void *, unsigned *);

///////////////////////////////////////////////////////////////////////////////
static int merge_profile_level_h264(char ** result, const char * dest, const char * src);
static int merge_packetization_mode(char ** result, const char * dest, const char * src);
static void free_string(char * str);
///////////////////////////////////////////////////////////////////////////////

#endif  // Signal Only

static void * create_encoder     ( const struct PluginCodec_Definition *);
static void destroy_encoder      ( const struct PluginCodec_Definition *, void * _context);
static int codec_encoder         ( const struct PluginCodec_Definition *, void * _context,
                                   const void * from, unsigned * fromLen,
                                   void * to, unsigned * toLen,
                                   unsigned int * flag);

static void * create_decoder     ( const struct PluginCodec_Definition *);
static void destroy_decoder      ( const struct PluginCodec_Definition *, void * _context);
static int codec_decoder         ( const struct PluginCodec_Definition *, void * _context, 
                                   const void * from, unsigned * fromLen,
                                   void * to, unsigned * toLen,
                                   unsigned int * flag);


static struct PluginCodec_information licenseInfo = {
  1143692893,                                                   // timestamp = Thu 30 Mar 2006 04:28:13 AM UTC

  "Matthias Schneider",                                         // source code author
  "1.0",                                                        // source code version
  "ma30002000@yahoo.de",                                        // source code email
  "",                                                           // source code URL
  "Copyright (C) 2006 by Matthias Schneider",                   // source code copyright
  "MPL 1.0",                                                    // source code license
  PluginCodec_License_MPL,                                      // source code license

  "x264 / ffmpeg H.264",                                        // codec description
  "x264: Laurent Aimar, ffmpeg: Michael Niedermayer",           // codec author
  "", 							        // codec version
  "fenrir@via.ecp.fr, ffmpeg-devel-request@mplayerhq.hu",       // codec email
  "http://developers.videolan.org/x264.html, \
   http://ffmpeg.mplayerhq.hu", 				// codec URL
  "x264: Copyright (C) 2003 Laurent Aimar, \
   ffmpeg: Copyright (c) 2002-2003 Michael Niedermayer",        // codec copyright information
  "x264: GNU General Public License as published Version 2, \
   ffmpeg: GNU Lesser General Public License, Version 2.1",     // codec license
  PluginCodec_License_GPL                                       // codec license code
};

static const char YUV420PDesc[]  = { "YUV420P" };



static PluginCodec_ControlDefn EncoderControls[] = {
#ifndef _SIGNAL_ONLY
  { PLUGINCODEC_CONTROL_VALID_FOR_PROTOCOL,    valid_for_protocol },
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_FREE_CODEC_OPTIONS,    free_codec_options },
  { PLUGINCODEC_CONTROL_TO_NORMALISED_OPTIONS, to_normalised_options },
  { PLUGINCODEC_CONTROL_TO_CUSTOMISED_OPTIONS, to_customised_options },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,     encoder_set_options },
  { PLUGINCODEC_CONTROL_CODEC_EVENT,           encoder_event_handler },
  { PLUGINCODEC_CONTROL_FLOW_OPTIONS,	       encoder_flowcontrol },
  { PLUGINCODEC_CONTROL_SET_FORMAT_OPTIONS,    encoder_formats },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  encoder_get_output_data_size },
#endif
  { NULL }
};

static PluginCodec_ControlDefn DecoderControls[] = {
#ifndef _SIGNAL_ONLY
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_TO_CUSTOMISED_OPTIONS, to_customised_options },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  decoder_get_output_data_size },
#endif
  { NULL }
};


/////////////////////////////////////////////////////////////////////////////
// SIP definitions
/*
Still to consider
       sprop-parameter-sets: this may be a NAL
       max-mbps, max-fs, max-cpb, max-dpb, and max-br
       parameter-add
       max-rcmd-nalu-size:
*/

static const char h264Desc[]      = { "H.264" };
static const char sdpH264[]       = { "h264" };

#define H264_CLOCKRATE        90000
#define H264_BITRATE         768000
#define H264_PAYLOAD_SIZE      1400
#define H264_FRAME_RATE          30
#define H264_KEY_FRAME_INTERVAL  (30*120) // Send a key frame no more than once every two minutes (unless requested through fast update)


/////////////////////////////////////////////////////////////////////////////

#ifndef _SIGNAL_ONLY

static struct PluginCodec_Option const RFC3984packetizationMode =
{
  PluginCodec_StringOption,             // Option type
  "CAP RFC3894 Packetization Mode",     // User visible name
  false,                                // User Read/Only flag
  PluginCodec_CustomMerge,              // Merge mode
  "1",                                  // Initial value (Baseline, Level 3)
  "packetization-mode",                 // FMTP option name 
  "5",                                  // FMTP default value
  0,
  "1",
  "5",
  merge_packetization_mode,             // Function to do merge
  free_string                           // Function to free memory in string
};

static struct PluginCodec_Option const RFC3984profileLevel =
{
  PluginCodec_StringOption,             // Option type
  "CAP RFC3894 Profile Level",          // User visible name
  false,                                // User Read/Only flag
  PluginCodec_CustomMerge,              // Merge mode
  "42C01E",                             // Initial value (Baseline, Level 3)
  "profile-level-id",                   // FMTP option name 
  "000000",                             // FMTP default value
  0,
  "000000",
  "58F033",
  merge_profile_level_h264,             // Function to do merge
  free_string                           // Function to free memory in string
};

#endif //_SIGNAL_ONLY

static struct PluginCodec_Option const * const optionTable[] = {
#ifndef _SIGNAL_ONLY
  &RFC3984packetizationMode,
  &RFC3984profileLevel,
#endif
  NULL
};


///////////////////////////////////////////////////////////////////////////
// H.323 Definitions

/////////////////////////////////////////////////////////////////////////////
// Codec Definitions

// SIP 42E015 is 
//  Profile : H264_PROFILE_BASE + H264_PROFILE_MAIN 
//  Level : 2:1  compatible H.323 codec is 4CIF.
#define H264_PROFILE_BASE      64   // only do base and main at this stage
#define H264_PROFILE_MAIN      32
#define H264_PROFILE_EXTENDED  16
#define H264_PROFILE_HIGH       8   

#define H264_BASE_IDC		   66   // RFC3984  Baseline
#define H264_HIGH_IDC		  100   // RFC3984  High
#define PLUGINCODEC_OPTION_PROFILE			"Generic Parameter 41"
#define PLUGINCODEC_OPTION_LEVEL			"Generic Parameter 42"
#define PLUGINCODEC_OPTION_ASPECT			"Generic Parameter 10"
#define PLUGINCODEC_OPTION_MAXFRAMERATE	    "Generic Parameter 13"
#define PLUGINCODEC_OPTION_CUSMBPS			"Generic Parameter 3"
#define PLUGINCODEC_OPTION_CUSMFS			"Generic Parameter 4"
#define PLUGINCODEC_OPTION_STATICMBPS	    "Generic Parameter 7"
#define PLUGINCODEC_OPTION_MAXNALSIZE	    "Generic Parameter 9"

#define H264_H323_RFC3984	OpalPluginCodec_Identifer_H264_Aligned   // Single NAL packetization H.241 Annex A
#define H264_ASPECT_43			2
#define H264_ASPECT_HD			13

// NOTE: All these values are subject to change Values need to be confirmed!
#define H264_LEVEL1           15      // SQCIF  30 fps
#define H264_LEVEL1_MBPS      640
#define H264_LEVEL1_1         22      // QCIF 30 fps
#define H264_LEVEL1_1_MBPS    1920
#define H264_LEVEL1_2         29
#define H264_LEVEL1_2_MBPS    3840
#define H264_LEVEL1_3         36      //  CIF 30 fps 
#define H264_LEVEL1_3_MBPS    3840
#define H264_LEVEL2_1         50
#define H264_LEVEL2_1_MBPS    5120    //
#define H264_LEVEL2_2         57
#define H264_LEVEL2_2_MBPS    5120
#define H264_LEVEL3           64      // 720p 30fps
#define H264_LEVEL3_MBPS      12000
#define H264_LEVEL3_1         71      // 720p 30fps
#define H264_LEVEL3_1_MBPS    14000
#define H264_LEVEL4           85      // 1080p 30fps 
#define H264_LEVEL4_MBPS      20480


// H.264 QCIF
static const char     H264QCIF_Desc[]          = { "H.264-QCIF" };
static const char     H264QCIF_MediaFmt[]      = { "H.264-QCIF" };                             
static unsigned int   H264QCIF_FrameHeight     = QCIF_HEIGHT;               
static unsigned int   H264QCIF_FrameWidth      = QCIF_WIDTH;
static unsigned int   H264QCIF_Profile         = H264_PROFILE_BASE; 
static unsigned int   H264QCIF_Level           = H264_LEVEL1_1;
static unsigned int   H264QCIF_MaxBitRate      = H264_LEVEL1_1_MBPS*100;
static const char	  H264QCIF_TargetBitRate[]=  { "192000" }; 
static unsigned int   H264QCIF_VideoType       = PluginCodec_MediaTypeVideo;
static unsigned int   H264QCIF_Generic3        = 0;        
static unsigned int   H264QCIF_Generic4        = 0;
static unsigned int   H264QCIF_Generic5        = 0;
static unsigned int   H264QCIF_Generic6        = 0;
static unsigned int   H264QCIF_Generic7        = 0;
static unsigned int   H264QCIF_Generic9        = 0;
static unsigned int   H264QCIF_Generic10       = 0; //H264_ASPECT_43;
static unsigned int   H264QCIF_Generic13       = 3000; 


// H.264 CIF
static const char     H264CIF_Desc[]          = { "H.264-CIF" };
static const char     H264CIF_MediaFmt[]      = { "H.264-CIF" };                             
static unsigned int   H264CIF_FrameHeight     = CIF_HEIGHT;               
static unsigned int   H264CIF_FrameWidth      = CIF_WIDTH; 
static unsigned int   H264CIF_Profile         = H264_PROFILE_BASE; 
static unsigned int   H264CIF_Level           = H264_LEVEL1_3;
static unsigned int   H264CIF_MaxBitRate      = H264_LEVEL1_3_MBPS*100;
static const char	  H264CIF_TargetBitRate[] =  { "384000" }; 
static unsigned int   H264CIF_VideoType       = PluginCodec_MediaTypeVideo; 
static unsigned int   H264CIF_Generic3        = 0;        
static unsigned int   H264CIF_Generic4        = 0;
static unsigned int   H264CIF_Generic5        = 0;
static unsigned int   H264CIF_Generic6        = 0;
static unsigned int   H264CIF_Generic7        = 0;
static unsigned int   H264CIF_Generic9        = 0;
static unsigned int   H264CIF_Generic10       = 0; //H264_ASPECT_43;
static unsigned int   H264CIF_Generic13       = 3000; 


// H.264 VGA
static const char     H264VGA_Desc[]          = { "H.264-VGA" };
static const char     H264VGA_MediaFmt[]      = { "H.264-VGA" };                             
static unsigned int   H264VGA_FrameHeight     = VGA_HEIGHT;               
static unsigned int   H264VGA_FrameWidth      = VGA_WIDTH; 
static unsigned int   H264VGA_Profile         = H264_PROFILE_BASE; 
static unsigned int   H264VGA_Level           = H264_LEVEL2_1;
static unsigned int   H264VGA_MaxBitRate      = H264_LEVEL2_1_MBPS*100;
static const char	  H264VGA_TargetBitRate[] =  { "512000" }; 
static unsigned int   H264VGA_VideoType       = PluginCodec_MediaTypeVideo; 
static unsigned int   H264VGA_Generic3        = 72;        
static unsigned int   H264VGA_Generic4        = 5;
static unsigned int   H264VGA_Generic5        = 0;
static unsigned int   H264VGA_Generic6        = 0;
static unsigned int   H264VGA_Generic7        = 0;
static unsigned int   H264VGA_Generic9        = 0;
static unsigned int   H264VGA_Generic10       = 0;
static unsigned int   H264VGA_Generic13       = 3000; 


// H.264 CIF4   // compatible SIP profile
static const char     H264CIF4_Desc[]          = { "H.264-4CIF" };
static const char     H264CIF4_MediaFmt[]      = { "H.264-4CIF" };                             
static unsigned int   H264CIF4_FrameHeight     = CIF4_HEIGHT;               
static unsigned int   H264CIF4_FrameWidth      = CIF4_WIDTH;
static unsigned int   H264CIF4_Profile         = H264_PROFILE_BASE; //+ H264_PROFILE_MAIN;
static unsigned int   H264CIF4_Level           = H264_LEVEL3;
static unsigned int   H264CIF4_MaxBitRate      = H264_LEVEL3_MBPS*100;
static const char	  H264CIF4_TargetBitRate[]=  { "10000000" }; 
static unsigned int   H264CIF4_VideoType       = PluginCodec_MediaTypeVideo; // | PluginCodec_MediaTypeExtVideo;
static unsigned int   H264CIF4_Generic3        = 0;        
static unsigned int   H264CIF4_Generic4        = 0;
static unsigned int   H264CIF4_Generic5        = 0;
static unsigned int   H264CIF4_Generic6        = 0;
static unsigned int   H264CIF4_Generic7        = 0;
static unsigned int   H264CIF4_Generic9        = 0;
static unsigned int   H264CIF4_Generic10       = 0; //H264_ASPECT_43;
static unsigned int   H264CIF4_Generic13       = 3000; 


// HD 720P
static const char     H264720P_Desc[]          = { "H.264-720" };
static const char     H264720P_MediaFmt[]      = { "H.264-720" };                             
static unsigned int   H264720P_FrameHeight     = P720_HEIGHT;               
static unsigned int   H264720P_FrameWidth      = P720_WIDTH;
static unsigned int   H264720P_Profile         = H264_PROFILE_BASE; 
static unsigned int   H264720P_Level           = H264_LEVEL3_1;
static unsigned int   H264720P_MaxBitRate      = H264_LEVEL4_MBPS*100; //H264_LEVEL3_1_MBPS*100;
static const char	  H264720P_TargetBitRate[] =  { "2048000" }; 
static unsigned int   H264720P_VideoType       = PluginCodec_MediaTypeVideo;
static unsigned int   H264720P_Generic3        = 231;
static unsigned int   H264720P_Generic4        = 15;
static unsigned int   H264720P_Generic5        = 0;
static unsigned int   H264720P_Generic6        = 0;
static unsigned int   H264720P_Generic7        = 0;
static unsigned int   H264720P_Generic9        = 0;
static unsigned int   H264720P_Generic10       = 0;    //H264_ASPECT_HD;
static unsigned int   H264720P_Generic13       = 3000; 

// HD H.239 1920 x 1152 
static const char     H264H239_Desc[]           = { "H.264" };
static const char     H264H239_MediaFmt[]       = { "H.264" };
static unsigned int   H264H239_FrameHeight      = 1152;
static unsigned int   H264H239_FrameWidth       = 1920;
static unsigned int   H264H239_Profile          = H264_PROFILE_BASE;
static unsigned int   H264H239_Level            = H264_LEVEL4;
static unsigned int   H264H239_MaxBitRate       = 540000;
static const char     H264H239_TargetBitRate[]  = { "540000" };
static unsigned int   H264H239_VideoType        = PluginCodec_MediaTypeExtended | PluginCodec_MediaTypeH239;
static unsigned int   H264H239_Generic3         = 492; 
static unsigned int   H264H239_Generic4         = 34; 
static unsigned int   H264H239_Generic5         = 0;
static unsigned int   H264H239_Generic6         = 0;
static unsigned int   H264H239_Generic7         = 0;
static unsigned int   H264H239_Generic9         = 0;
static unsigned int   H264H239_Generic10        = 0; //H264_ASPECT_HD;
static unsigned int   H264H239_Generic13        = 3000; 


static const char     H2641080P_Desc[]          = { "H.264-720" };
static const char     H2641080P_MediaFmt[]      = { "H.264-720" };                             
static unsigned int   H2641080P_FrameHeight     = P1080_HEIGHT;               
static unsigned int   H2641080P_FrameWidth      = P1080_WIDTH;
static unsigned int   H2641080P_Profile         = H264_PROFILE_BASE; //H264_PROFILE_HIGH;
static unsigned int   H2641080P_Level           = H264_LEVEL2_2; //H264_LEVEL4;
static unsigned int   H2641080P_MaxBitRate      = H264_LEVEL4_MBPS*100;
static const char	  H2641080P_TargetBitRate[] =  { "2084000" }; 
static unsigned int   H2641080P_VideoType       = PluginCodec_MediaTypeVideo;
static unsigned int   H2641080P_Generic3        = 492;
static unsigned int   H2641080P_Generic4        = 32;
static unsigned int   H2641080P_Generic5        = 0;
static unsigned int   H2641080P_Generic6        = 0;
static unsigned int   H2641080P_Generic7        = 0;
static unsigned int   H2641080P_Generic9        = 0;
static unsigned int   H2641080P_Generic10       = 0; //H264_ASPECT_HD;
static unsigned int   H2641080P_Generic13       = 3000; 


// MEGA MACRO to set options
#define DECLARE_GENERIC_OPTIONS(prefix) \
static struct PluginCodec_Option const prefix##_h323packetization = \
 { PluginCodec_StringOption, PLUGINCODEC_MEDIA_PACKETIZATION, true, PluginCodec_EqualMerge, H264_H323_RFC3984 }; \
static struct PluginCodec_Option const prefix##_targetBitRate = \
 { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_TARGET_BIT_RATE,  true, PluginCodec_MinMerge, prefix##_TargetBitRate }; \
static struct PluginCodec_Option const * const prefix##_OptionTable[] = { \
  &prefix##_h323packetization, \
  &prefix##_targetBitRate, \
  NULL \
}; \
static const struct PluginCodec_H323GenericParameterDefinition prefix##_h323params[] = \
{ \
	{{1,0,0,0,0},3, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMin, {prefix##_Generic3}}, \
	{{1,0,0,0,0},4, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMin, {prefix##_Generic4}}, \
	{{1,0,0,0,0},5, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMin, {prefix##_Generic5}}, \
	{{1,0,0,0,0},6, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMin, {prefix##_Generic6}}, \
	{{1,0,0,0,0},7, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMin, {prefix##_Generic7}}, \
	{{1,0,0,0,0},9, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_Unsigned32Min, {prefix##_Generic9}}, \
	{{1,0,0,0,0},10, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMin, {prefix##_Generic10}}, \
	{{1,0,0,0,0},13, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMin, {prefix##_Generic13}}, \
	{{1,0,0,0,0},41, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_BooleanArray,{prefix##_Profile}}, \
	{{1,0,0,0,0},42, PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMin, {prefix##_Level}}, \
}; \
static struct PluginCodec_H323GenericCodecData prefix##_h323GenericData[] = { \
    { OpalPluginCodec_Identifer_H264_Generic, prefix##_MaxBitRate, 10, prefix##_h323params } \
}; \

DECLARE_GENERIC_OPTIONS(H264QCIF)
DECLARE_GENERIC_OPTIONS(H264CIF)
DECLARE_GENERIC_OPTIONS(H264VGA)
DECLARE_GENERIC_OPTIONS(H264CIF4)
DECLARE_GENERIC_OPTIONS(H264720P)
DECLARE_GENERIC_OPTIONS(H264H239)
DECLARE_GENERIC_OPTIONS(H2641080P)

#define DECLARE_H323PARAM(prefix) \
{ \
  /* encoder */ \
  PLUGIN_CODEC_VERSION_OPTIONS,	      /* codec API version */ \
  &licenseInfo,                       /* license information */ \
  prefix##_VideoType |                /* video type */ \
  PluginCodec_RTPTypeShared |         /* specified RTP type */ \
  PluginCodec_RTPTypeDynamic,         /* specified RTP type */ \
  prefix##_Desc,                      /* text decription */ \
  YUV420PDesc,                        /* source format */ \
  prefix##_MediaFmt,                  /* destination format */ \
  prefix##_OptionTable,			      /* user data */ \
  H264_CLOCKRATE,                     /* samples per second */ \
  prefix##_MaxBitRate,				  /* raw bits per second */ \
  20000,                              /* nanoseconds per frame */ \
  {{ prefix##_FrameWidth,             /* samples per frame */ \
  prefix##_FrameHeight,			      /* bytes per frame */ \
  10,                                 /* recommended number of frames per packet */ \
  60, }},                             /* maximum number of frames per packet  */ \
  0,                                  /* IANA RTP payload code */ \
  sdpH264,                            /* RTP payload name */ \
  create_encoder,                     /* create codec function */ \
  destroy_encoder,                    /* destroy codec */ \
  codec_encoder,                      /* encode/decode */ \
  EncoderControls,                    /* codec controls */ \
  PluginCodec_H323Codec_generic,      /* h323CapabilityType */ \
  (struct PluginCodec_H323GenericCodecData *)&prefix##_h323GenericData /* h323CapabilityData */ \
}, \
{  \
  /* decoder */ \
  PLUGIN_CODEC_VERSION_OPTIONS,	      /* codec API version */ \
  &licenseInfo,                       /* license information */ \
  prefix##_VideoType |                /* video codec */ \
  PluginCodec_RTPTypeShared |         /* specified RTP type */ \
  PluginCodec_RTPTypeDynamic,         /* specified RTP type */ \
  prefix##_Desc,                      /* text decription */ \
  prefix##_MediaFmt,                  /* source format */ \
  YUV420PDesc,                        /* destination format */ \
  prefix##_OptionTable,			      /* user data */ \
  H264_CLOCKRATE,                     /* samples per second */ \
  prefix##_MaxBitRate,				  /* raw bits per second */ \
  20000,                              /* nanoseconds per frame */ \
  {{ prefix##_FrameWidth,             /* samples per frame */ \
  prefix##_FrameHeight,			      /* bytes per frame */ \
  10,                                 /* recommended number of frames per packet */ \
  60, }},                             /* maximum number of frames per packet  */ \
  0,                                  /* IANA RTP payload code */ \
  sdpH264,                            /* RTP payload name */ \
  create_decoder,                     /* create codec function */ \
  destroy_decoder,                    /* destroy codec */ \
  codec_decoder,                      /* encode/decode */ \
  DecoderControls,                    /* codec controls */ \
  PluginCodec_H323Codec_generic,      /* h323CapabilityType */ \
  (struct PluginCodec_H323GenericCodecData *)&prefix##_h323GenericData /* h323CapabilityData */ \
} 

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Definition h264CodecDefn[] = {
 // DECLARE_H323PARAM(H264QCIF),
   DECLARE_H323PARAM(H264CIF),
 // DECLARE_H323PARAM(H264VGA),
 // DECLARE_H323PARAM(H264CIF4)
 // DECLARE_H323PARAM(H264720P),
   DECLARE_H323PARAM(H264H239)
  ,DECLARE_H323PARAM(H2641080P)
};

#endif /* __H264-X264_H__ */
