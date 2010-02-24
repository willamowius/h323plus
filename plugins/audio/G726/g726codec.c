/*
 * G.726 plugin codec for OpenH323/OPAL
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

#include "g726/g72x.h"

#define SAMPLES_PER_FRAME       8

#define MAX_FRAMES_PER_PACKET   240 // Really milliseconds
#define PREF_FRAMES_PER_PACKET  30  // Really milliseconds

#define PAYLOAD_CODE            96  // used to be 2, now uses dynamic payload type (RFC 3551)

/////////////////////////////////////////////////////////////////////////////

static void * create_codec(const struct PluginCodec_Definition * codec)
{
  struct g726_state_s * g726 = malloc(sizeof (struct g726_state_s));
  g726_init_state(g726);
  return g726;
}

static void destroy_codec(const struct PluginCodec_Definition * codec, void * context)
{
  free(context);
}

/////////////////////////////////////////////////////////////////////////////

#define define_coder(dir, bps) \
static int dir##coder_##bps(const struct PluginCodec_Definition * codec, \
                                                           void * context, \
                                                     const void * from, \
                                                       unsigned * fromLen, \
                                                           void * to, \
                                                       unsigned * toLen, \
                                                   unsigned int * flag) \
{ \
  *(int *)to = g726_##bps##_##dir##coder(*(int *)from, AUDIO_ENCODING_LINEAR, (struct g726_state_s *)context); \
  return 1; \
}

define_coder(en, 40)
define_coder(de, 40)

define_coder(en, 32)
define_coder(de, 32)

define_coder(en, 24)
define_coder(de, 24)

define_coder(en, 16)
define_coder(de, 16)

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information licenseInfo = {
  1084181196,                              // timestamp = Mon 10 May 2004 09:26:36 AM UTC

  "Craig Southeren, Post Increment",                           // source code author
  "1.0",                                                       // source code version
  "craigs@postincrement.com",                                  // source code email
  "http://www.postincrement.com",                              // source code URL
  "Copyright (C) 2004 by Post Increment, All Rights Reserved", // source code copyright
  "MPL 1.0",                                                   // source code license
  PluginCodec_License_MPL,                                     // source code license
  
  "CCITT Recommendation G.721",                                // codec description
  "Sun Microsystems, Inc.",                                    // codec author
  NULL,                                                        // codec version
  NULL,                                                        // codec email
  NULL,                                                        // codec URL
  "This source code is a product of Sun Microsystems, Inc. and is provided\n"
  "for unrestricted use.  Users may copy or modify this source code without charge",  // codec copyright information
  NULL,                                                        // codec license
  PluginCodec_Licence_None                                     // codec license code
};

/////////////////////////////////////////////////////////////////////////////

static const char L16Desc[]  = { "L16" };

static const char sdpG726_16[]  = { "G726-16" };
static const char sdpG726_24[]  = { "G726-24" };
static const char sdpG726_32[]  = { "G726-32" };
static const char sdpG726_40[]  = { "G726-40" };

static const char g726_40[] = "G.726-40k";
static const char g726_32[] = "G.726-32k";
static const char g726_24[] = "G.726-24k";
static const char g726_16[] = "G.726-16k";

/////////////////////////////////////////////////////////////////////////////

#define	EQUIVALENCE_COUNTRY_CODE            9
#define	EQUIVALENCE_EXTENSION_CODE          0
#define	EQUIVALENCE_MANUFACTURER_CODE       61


static struct PluginCodec_H323NonStandardCodecData g726_40_Cap =
{
  NULL, 
  EQUIVALENCE_COUNTRY_CODE, 
  EQUIVALENCE_EXTENSION_CODE, 
  EQUIVALENCE_MANUFACTURER_CODE,
  (const unsigned char *)g726_40, sizeof(g726_40)-1,
  NULL
};

static struct PluginCodec_H323NonStandardCodecData g726_32_Cap =
{
  NULL, 
  EQUIVALENCE_COUNTRY_CODE, 
  EQUIVALENCE_EXTENSION_CODE, 
  EQUIVALENCE_MANUFACTURER_CODE,
  (const unsigned char *)g726_32, sizeof(g726_32)-1,
  NULL
};

static struct PluginCodec_H323NonStandardCodecData g726_24_Cap =
{
  NULL, 
  EQUIVALENCE_COUNTRY_CODE, 
  EQUIVALENCE_EXTENSION_CODE, 
  EQUIVALENCE_MANUFACTURER_CODE,
  (const unsigned char *)g726_24, sizeof(g726_24)-1,
  NULL
};

static struct PluginCodec_H323NonStandardCodecData g726_16_Cap =
{
  NULL, 
  EQUIVALENCE_COUNTRY_CODE, 
  EQUIVALENCE_EXTENSION_CODE, 
  EQUIVALENCE_MANUFACTURER_CODE,
  (const unsigned char *)g726_16, sizeof(g726_16)-1,
  NULL
};

static struct PluginCodec_Definition g726CodecDefn[] =
{
  { 
    // encoder 40k
    PLUGIN_CODEC_VERSION,                 // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudioStreamed |  // audio codec
    (5 << PluginCodec_BitsPerSamplePos) | // bits per sample
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeDynamic,           // dynamic RTP type

    g726_40,                              // text decription
    L16Desc,                              // source format
    g726_40,                              // destination format

    0,                                    // user data

    8000,                                 // samples per second
    40000,                                // raw bits per second
    1000,                                 // nanoseconds per frame

    {{
      SAMPLES_PER_FRAME,                  // samples per frame
      5,                                  // bytes per frame
      PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
      MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    }},

    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG726_40,                           // RTP payload name

    create_codec,                         // create codec function
    destroy_codec,                        // destroy codec
    encoder_40,                           // encode/decode
    NULL,                                 // codec controls

    PluginCodec_H323Codec_nonStandard,    // h323CapabilityType 
    &g726_40_Cap                          // h323CapabilityData
  },

  { 
    // decoder 40k
    PLUGIN_CODEC_VERSION,                 // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudioStreamed |  // audio codec
    (5 << PluginCodec_BitsPerSamplePos) | // bits per sample
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeDynamic,           // dynamic RTP type

    g726_40,                              // text decription
    g726_40,                              // source format
    L16Desc,                              // destination format

    0,                                    // user data

    8000,                                 // samples per second
    40000,                                // raw bits per second
    1000,                                 // nanoseconds per frame

    {{
      SAMPLES_PER_FRAME,                  // samples per frame
      5,                                  // bytes per frame
      PREF_FRAMES_PER_PACKET,             // recommended number of frames per packet
      MAX_FRAMES_PER_PACKET,              // maximum number of frames per packe
    }},

    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG726_40,                           // RTP payload name

    create_codec,                         // create codec function
    destroy_codec,                        // destroy codec
    decoder_40,                           // encode/decode
    NULL,                                 // codec controls

    PluginCodec_H323Codec_nonStandard,    // h323CapabilityType 
    &g726_40_Cap                          // h323CapabilityData
  },

  ////////////////////////////////////

  {
    // encoder 32k
    PLUGIN_CODEC_VERSION,                 // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudioStreamed |  // audio codec
    (4 << PluginCodec_BitsPerSamplePos) | // bits per sample
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeDynamic,           // dynamic RTP type

    g726_32,                              // text decription
    L16Desc,                              // source format
    g726_32,                              // destination format

    0,                                    // user data

    8000,                                 // samples per second
    32000,                                // raw bits per second
    1000,                                 // nanoseconds per frame
    SAMPLES_PER_FRAME,                    // samples per frame
    4,                                    // bytes per frame
    PREF_FRAMES_PER_PACKET,               // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,                // maximum number of frames per packe
    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG726_32,                           // RTP payload name

    create_codec,                         // create codec function
    destroy_codec,                        // destroy codec
    encoder_32,                           // encode/decode
    NULL,                                 // codec controls

    PluginCodec_H323Codec_nonStandard,    // h323CapabilityType 
    &g726_32_Cap                          // h323CapabilityData
  },

  { 
    // decoder 32k
    PLUGIN_CODEC_VERSION,                 // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudioStreamed |  // audio codec
    (4 << PluginCodec_BitsPerSamplePos) | // bits per sample
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeDynamic,           // dynamic RTP type

    g726_32,                              // text decription
    g726_32,                              // source format
    L16Desc,                              // destination format

    0,                                    // user data

    8000,                                 // samples per second
    32000,                                // raw bits per second
    1000,                                 // nanoseconds per frame
    SAMPLES_PER_FRAME,                    // samples per frame
    4,                                    // bytes per frame
    PREF_FRAMES_PER_PACKET,               // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,                // maximum number of frames per packe
    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG726_32,                           // RTP payload name

    create_codec,                         // create codec function
    destroy_codec,                        // destroy codec
    decoder_32,                           // encode/decode
    NULL,                                 // codec controls

    PluginCodec_H323Codec_nonStandard,    // h323CapabilityType 
    &g726_32_Cap                          // h323CapabilityData
  },

  ////////////////////////////////////

  {
    // encoder 24k
    PLUGIN_CODEC_VERSION,                 // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudioStreamed |  // audio codec
    (3 << PluginCodec_BitsPerSamplePos) | // bits per sample
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeDynamic,           // dynamic RTP type

    g726_24,                              // text decription
    L16Desc,                              // source format
    g726_24,                              // destination format

    0,                                    // user data

    8000,                                 // samples per second
    24000,                                // raw bits per second
    1000,                                 // nanoseconds per frame
    SAMPLES_PER_FRAME,                    // samples per frame
    3,                                    // bytes per frame
    PREF_FRAMES_PER_PACKET,               // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,                // maximum number of frames per packe
    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG726_24,                           // RTP payload name

    create_codec,                         // create codec function
    destroy_codec,                        // destroy codec
    encoder_24,                           // encode/decode
    NULL,                                 // codec controls

    PluginCodec_H323Codec_nonStandard,    // h323CapabilityType 
    &g726_24_Cap                          // h323CapabilityData
  },

  { 
    // decoder 24k
    PLUGIN_CODEC_VERSION,                 // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudioStreamed |  // audio codec
    (3 << PluginCodec_BitsPerSamplePos) | // bits per sample
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeDynamic,           // dynamic RTP type

    g726_24,                              // text decription
    g726_24,                              // source format
    L16Desc,                              // destination format

    0,                                    // user data

    8000,                                 // samples per second
    24000,                                // raw bits per second
    1000,                                 // nanoseconds per frame
    SAMPLES_PER_FRAME,                    // samples per frame
    3,                                    // bytes per frame
    PREF_FRAMES_PER_PACKET,               // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,                // maximum number of frames per packe
    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG726_32,                           // RTP payload name

    create_codec,                         // create codec function
    destroy_codec,                        // destroy codec
    decoder_24,                           // encode/decode
    NULL,                                 // codec controls

    PluginCodec_H323Codec_nonStandard,    // h323CapabilityType 
    &g726_24_Cap                          // h323CapabilityData
  },

  ////////////////////////////////////

  {
    // encoder 16k
    PLUGIN_CODEC_VERSION,                 // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudioStreamed |  // audio codec
    (2 << PluginCodec_BitsPerSamplePos) | // bits per sample
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeDynamic,           // dynamic RTP type

    g726_16,                              // text decription
    L16Desc,                              // source format
    g726_16,                              // destination format

    0,                                    // user data

    8000,                                 // samples per second
    16000,                                // raw bits per second
    1000,                                 // nanoseconds per frame
    SAMPLES_PER_FRAME,                    // samples per frame
    2,                                    // bytes per frame
    PREF_FRAMES_PER_PACKET,               // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,                // maximum number of frames per packe
    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG726_16,                           // RTP payload name

    create_codec,                         // create codec function
    destroy_codec,                        // destroy codec
    encoder_16,                           // encode/decode
    NULL,                                 // codec controls

    PluginCodec_H323Codec_nonStandard,    // h323CapabilityType 
    &g726_16_Cap                          // h323CapabilityData
  },

  { 
    // decoder 16k
    PLUGIN_CODEC_VERSION,                 // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudioStreamed |  // audio codec
    (2 << PluginCodec_BitsPerSamplePos) | // bits per sample
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeDynamic,           // dynamic RTP type

    g726_16,                              // text decription
    g726_16,                              // source format
    L16Desc,                              // destination format

    0,                                    // user data

    8000,                                 // samples per second
    16000,                                // raw bits per second
    1000,                                 // nanoseconds per frame
    SAMPLES_PER_FRAME,                    // samples per frame
    2,                                    // bytes per frame
    PREF_FRAMES_PER_PACKET,               // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,                // maximum number of frames per packe
    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG726_16,                           // RTP payload name

    create_codec,                         // create codec function
    destroy_codec,                        // destroy codec
    decoder_16,                           // encode/decode
    NULL,                                 // codec controls

    PluginCodec_H323Codec_nonStandard,    // h323CapabilityType 
    &g726_16_Cap                          // h323CapabilityData
  }
};


PLUGIN_CODEC_IMPLEMENT_ALL(G726, g726CodecDefn, PLUGIN_CODEC_VERSION)

/////////////////////////////////////////////////////////////////////////////
