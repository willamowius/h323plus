/*
 * GSM 06.10 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) 2004 Post Increment, All Rights Reserved
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
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _CRT_SECURE_NO_WARNINGS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PLUGIN_CODEC_DLL_EXPORTS
#include "plugin-config.h"
#endif

#include <codec/opalplugin.h>

#include <gsm.h>

#define	BITS_PER_SECOND         13200

#define	BYTES_PER_FRAME         33
#define	MSGSM_BYTES_PER_FRAME   65

#define SAMPLES_PER_FRAME       160
#define MSGSM_SAMPLES_PER_FRAME 320

#define MAX_FRAMES_PER_PACKET   7
#define PREF_FRAMES_PER_PACKET  1

#define MIN(a,b)    (((a)<=(b))?(a):(b))

#define PAYLOAD_CODE            3

#if defined(_WIN32) || defined(_WIN32_WCE)
  #define STRCMPI  _strcmpi
#else
  #define STRCMPI  strcasecmp
#endif

/////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(disable:4100)
#endif

static void * create_codec(const struct PluginCodec_Definition * codec)
{
  int opt = codec->userData != 0;
  struct gsm_state * context = gsm_create();
  gsm_option(context, GSM_OPT_WAV49, &opt);
  return context;
}

static void destroy_codec(const struct PluginCodec_Definition * codec, void * _context)
{
  struct gsm_state * context = (struct gsm_state *)_context;
  gsm_destroy(context);
}

static int codec_encoder(const struct PluginCodec_Definition * codec, 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  struct gsm_state * context = (struct gsm_state *)_context;
  int frames;

  if (*toLen < BYTES_PER_FRAME)
    return 0;

  if (*fromLen < SAMPLES_PER_FRAME * 2)
    return 0;

  frames = MIN(*fromLen / (SAMPLES_PER_FRAME * 2), (*toLen / BYTES_PER_FRAME));

  if (frames == 0)
    return 0;

  *fromLen = frames * SAMPLES_PER_FRAME * 2;
  *toLen   = frames * BYTES_PER_FRAME;

  while (frames-- > 0) {
    gsm_encode(context, (void *)from, to);
    to   = ((char *)to) + BYTES_PER_FRAME;
    from = ((const char *)from) + SAMPLES_PER_FRAME * 2;
  }

  return 1; 
}

static int codec_decoder(const struct PluginCodec_Definition * codec, 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  gsm context = (gsm)_context;

  if (*fromLen < BYTES_PER_FRAME)
    return 0;

  // if the packet is not 65 bytes long, assume it is frames of normal GSM
  if (*fromLen != 65) {

    int frames;

    if (*toLen < SAMPLES_PER_FRAME * 2)
      return 0;

    frames = MIN(*toLen / (SAMPLES_PER_FRAME * 2), (*fromLen / BYTES_PER_FRAME));

    if (frames == 0)
      return 0;

    {
      int opt = 0;
      gsm_option(context, GSM_OPT_WAV49, &opt);
    }

    *fromLen = frames * BYTES_PER_FRAME;
    *toLen   = frames * SAMPLES_PER_FRAME * 2;

    while (frames-- > 0) {
      gsm_decode(context, (void *)from, to);
      from = ((const char *)from) + BYTES_PER_FRAME;
      to   = ((char *)to) + SAMPLES_PER_FRAME * 2;
    }

    return 1;
  }

  // MS-GSM packets are always 65 bytes long, and consists of two GSM packets
  if (*toLen < MSGSM_SAMPLES_PER_FRAME * 2)
    return 0;
  {
    int opt = 1;
    gsm_option(context, GSM_OPT_WAV49, &opt);
  }

  gsm_decode(context, (unsigned char *)from,      (short *)to);
  gsm_decode(context, ((unsigned char *)from)+33, ((short *)to)+160);

  *toLen = MSGSM_SAMPLES_PER_FRAME * 2;

  return 1;
}

/////////////////////////////////////////////////////////////////////////////

static int codec_msgsm_encoder(const struct PluginCodec_Definition * codec, 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  struct gsm_state * context = (struct gsm_state *)_context;

  if (*fromLen < (MSGSM_SAMPLES_PER_FRAME*2) || *toLen < MSGSM_BYTES_PER_FRAME) 
    return 0;

  gsm_encode(context, (short *)from,       (unsigned char *)to);
  gsm_encode(context, ((short *)from)+160, ((unsigned char *)to)+32);

  *toLen = MSGSM_BYTES_PER_FRAME;

  return 1; 
}

static int codec_msgsm_decoder(const struct PluginCodec_Definition * codec, 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  struct gsm_state * context = (struct gsm_state *)_context;
  if (*fromLen < MSGSM_BYTES_PER_FRAME || *toLen < (MSGSM_SAMPLES_PER_FRAME*2)) 
    return 0;

  gsm_decode(context, (unsigned char *)from,      (short *)to);
  gsm_decode(context, ((unsigned char *)from)+33, ((short *)to)+160);

  *toLen = MSGSM_SAMPLES_PER_FRAME * 2;

  return 1;
}

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information licenseInfo = {
  1081086550,                              // timestamp = Sun 04 Apr 2004 01:49:10 PM UTC = 

  "Craig Southeren, Post Increment",                           // source code author
  "1.1",                                                       // source code version
  "craigs@postincrement.com",                                  // source code email
  "http://www.postincrement.com",                              // source code URL
  "Copyright (C) 2004 by Post Increment, All Rights Reserved", // source code copyright
  "MPL 1.0",                                                   // source code license
  PluginCodec_License_MPL,                                     // source code license
  
  "GSM 06.10 Full-Rate",                                       // codec description
  "Jutta Degener and Carsten Bormann",                         // codec author
  "Version 10",                                                // codec version
  "jutta@cs.tu-berlin.de\ncabo@cs.tu-berlin.de",               // codec email
  "http://kbs.cs.tu-berlin.de/~jutta/toast.html",              // codec URL
  "Copyright 1992, 1993, 1994 by Jutta Degener and Carsten Bormann, Technische Universitaet Berlin",          // codec copyright information
  NULL,                                                        // codec license
  PluginCodec_Licence_None                                     // codec license code
};

/////////////////////////////////////////////////////////////////////////////

static const char L16Desc[]  = { "L16" };

static const char gsm0610[]  = { "GSM-06.10" };

static const char msGSM[]    = { "MS-GSM" };

static const char sdpGSM[]   = { "gsm" };

// defined in draft-ema-vpim-msgsm-00.txt
static const char sdpMSGSM[] = { "ms-gsm" };

static struct PluginCodec_H323AudioGSMData gsmCaps = {
  0, // int comfortNoise:1;
  0, // int scrambled:1;
};

static const unsigned char msGSMHeader[] = 
{
  // unknown data
  0x02, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x40, 0x01, 
  0x00, 0x00, 0x40, 0x01, 
  0x02, 0x00, 0x08, 0x00, 
  0x00, 0x00, 0x00, 0x00,

#define	GSM_FIXED_START 20  // Offset to this point in header

  // standard MS waveformatex structure follows
  0x31, 0x00,                 //    WORD    wFormatTag;        /* format type */
  0x01, 0x00,                 //    WORD    nChannels;         /* number of channels (i.e. mono, stereo...) */
  0x40, 0x1f, 0x00, 0x00,     //    DWORD   nSamplesPerSec;    /* sample rate */  
  0x59, 0x06, 0x00, 0x00,     //    DWORD   nAvgBytesPerSec;   /* for buffer estimation */
  0x41, 0x00,                 //    WORD    nBlockAlign;       /* block size of data */
  0x00, 0x00,                 //    WORD    wBitsPerSample;    /* Number of bits per sample of mono data */
  0x02, 0x00,                 //    WORD    cbSize;            /* The count in bytes of the size of 

#define	GSM_FIXED_LEN 18  // Number of bytes from GSM_FIXED_START to here 

  // extra GSM information
  0x40, 0x01,                 //    WORD    numberOfSamples    /* 320 */
  
  // unknown data
  0x00, 0x00  
};

#define	MICROSOFT_COUNTRY_CODE	181
#define	MICROSOFT_T35EXTENSION	0
#define	MICROSOFT_MANUFACTURER	21324

int msgsmCompareFunc(struct PluginCodec_H323NonStandardCodecData * data)
{
  int val;

  if ((data == NULL) || (data->objectId != NULL) || (data->data == NULL))
    return -1;

  val = MICROSOFT_COUNTRY_CODE - data->t35CountryCode;
  if (val != 0)
    return val;

  val = MICROSOFT_T35EXTENSION - data->t35Extension;
  if (val != 0)
    return val;

  val = MICROSOFT_MANUFACTURER - data->manufacturerCode;
  if (val != 0)
    return val;

  val = sizeof(msGSMHeader) - data->dataLength;
  if (val != 0)
    return val;

  return memcmp(data->data+GSM_FIXED_START, msGSMHeader+GSM_FIXED_START, GSM_FIXED_LEN);
}

static struct PluginCodec_H323NonStandardCodecData msgsmCap =
{
  NULL,
  MICROSOFT_COUNTRY_CODE, MICROSOFT_T35EXTENSION, MICROSOFT_MANUFACTURER,
  msGSMHeader, sizeof(msGSMHeader),
  msgsmCompareFunc
};

static int valid_for_h323(
      const struct PluginCodec_Definition * codec, 
      void * context, 
      const char * key, 
      void * parm , 
      unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char *))
    return 0;

  return (STRCMPI((const char *)parm, "h.323") == 0 ||
          STRCMPI((const char *)parm, "h323") == 0) ? 1 : 0;
}

static struct PluginCodec_ControlDefn h323CoderControls[] = {
  { "valid_for_protocol",       valid_for_h323 },
  //{ "get_codec_options",      coder_get_sip_options },
  //{ "set_codec_options",      encoder_set_options },
  { NULL }
};

/////////////////////////////////////////////////////////////////////////////


static struct PluginCodec_Definition gsmCodecDefn[4] =
{
  { 
    // encoder
    PLUGIN_CODEC_VERSION,               // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeExplicit,        // specified RTP type

    gsm0610,                            // text decription
    L16Desc,                            // source format
    gsm0610,                            // destination format

    0,                                  // user data (no WAV49)

    8000,                               // samples per second
    BITS_PER_SECOND,                    // raw bits per second
    20000,                              // nanoseconds per frame
    SAMPLES_PER_FRAME,                  // samples per frame
    BYTES_PER_FRAME,                    // bytes per frame
    PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    PAYLOAD_CODE,                       // IANA RTP payload code
    sdpGSM,                             // RTP payload name

    create_codec,                       // create codec function
    destroy_codec,                      // destroy codec
    codec_encoder,                      // encode/decode
    NULL,                               // codec controls

    PluginCodec_H323AudioCodec_gsmFullRate,  // h323CapabilityType 
    &gsmCaps                             // h323CapabilityData
  },

  { 
    // decoder
    PLUGIN_CODEC_VERSION,               // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeExplicit,        // dynamic RTP type

    gsm0610,                            // text decription
    gsm0610,                            // source format
    L16Desc,                            // destination format

    0,                                  // user data (no WAV49)

    8000,                               // samples per second
    BITS_PER_SECOND,                    // raw bits per second
    20000,                              // nanoseconds per frame
    SAMPLES_PER_FRAME,                  // samples per frame
    BYTES_PER_FRAME,                    // bytes per frame
    PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    PAYLOAD_CODE,                       // IANA RTP payload code
    sdpGSM,                             // RTP payload name

    create_codec,                       // create codec function
    destroy_codec,                      // destroy codec
    codec_decoder,                      // encode/decode
    NULL,                               // codec controls

    PluginCodec_H323AudioCodec_gsmFullRate,  // h323CapabilityType 
    &gsmCaps                             // h323CapabilityData
  },


  // MS-GSM

    { 
    // encoder
    PLUGIN_CODEC_VERSION,               // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeDynamic,         // specified RTP type

    msGSM,                              // text decription
    L16Desc,                            // source format
    msGSM,                              // destination format

    (const void *)1,                    // user data (enable WAV49)

    8000,                               // samples per second
    BITS_PER_SECOND,                    // raw bits per second
    40000,                              // nanoseconds per frame
    MSGSM_SAMPLES_PER_FRAME,            // samples per frame
    MSGSM_BYTES_PER_FRAME,              // bytes per frame
    1,                                  // recommended number of frames per packet
    1,                                  // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpMSGSM,                           // RTP payload name

    create_codec,                       // create codec function
    destroy_codec,                      // destroy codec
    codec_msgsm_encoder,                // encode/decode
    h323CoderControls,                  // codec controls

    PluginCodec_H323Codec_nonStandard,  // h323CapabilityType 
    &msgsmCap                           // h323CapabilityData
  },

  { 
    // decoder
    PLUGIN_CODEC_VERSION,               // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    msGSM,                              // text decription
    msGSM,                              // source format
    L16Desc,                            // destination format

    (const void *)1,                    // user data (enable WAV49)

    8000,                               // samples per second
    BITS_PER_SECOND,                    // raw bits per second
    40000,                              // nanoseconds per frame
    MSGSM_SAMPLES_PER_FRAME,            // samples per frame
    MSGSM_BYTES_PER_FRAME,              // bytes per frame
    1,                                  // recommended number of frames per packet
    1,                                  // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpMSGSM,                           // RTP payload name

    create_codec,                       // create codec function
    destroy_codec,                      // destroy codec
    codec_msgsm_decoder,                // encode/decode
    h323CoderControls,                  // codec controls

    PluginCodec_H323Codec_nonStandard,  // h323CapabilityType 
    &msgsmCap                           // h323CapabilityData
  }
};


PLUGIN_CODEC_IMPLEMENT_ALL(GSM_0610, gsmCodecDefn, PLUGIN_CODEC_VERSION)

/////////////////////////////////////////////////////////////////////////////
