/*
 * iLBC Plugin codec for OpenH323/OPAL
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

#include "iLBC/iLBC_encode.h" 
#include "iLBC/iLBC_decode.h" 
#include "iLBC/iLBC_define.h" 

#define	BITRATE_30MS	NO_OF_BYTES_30MS*8*8000/BLOCKL_30MS
#define	BITRATE_20MS	NO_OF_BYTES_20MS*8*8000/BLOCKL_20MS

#if defined(_WIN32) || defined(_WIN32_WCE)
  #define STRCMPI  _strcmpi
#else
  #define STRCMPI  strcasecmp
#endif


/////////////////////////////////////////////////////////////////////////////

static void * create_encoder(const struct PluginCodec_Definition * codec)
{
  struct iLBC_Enc_Inst_t_ * context = (struct iLBC_Enc_Inst_t_ *)malloc((unsigned)sizeof(struct iLBC_Enc_Inst_t_));
  initEncode(context, codec->bitsPerSec != BITRATE_30MS ? 20 : 30); 
  return context;
}


static void * create_decoder(const struct PluginCodec_Definition * codec)
{
  struct iLBC_Dec_Inst_t_ * context = (struct iLBC_Dec_Inst_t_ *)malloc((unsigned)sizeof(struct iLBC_Dec_Inst_t_));
  initDecode(context, codec->bitsPerSec != BITRATE_30MS ? 20 : 30, 0); 
  return context;
}


static void destroy_context(const struct PluginCodec_Definition * codec, void * context)
{
  free(context);
}


static int codec_encoder(const struct PluginCodec_Definition * codec, 
                                           void * context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  float block[BLOCKL_MAX];
  int i;

  struct iLBC_Enc_Inst_t_ * encoder = (struct iLBC_Enc_Inst_t_ *)context;
  const short * sampleBuffer = (const short *)from;

  if (*fromLen < encoder->blockl*2U)
    return 0;

  /* convert signal to float */
  for (i = 0; i < encoder->blockl; i++)
    block[i] = (float)sampleBuffer[i];

  /* do the actual encoding */
  iLBC_encode(to, block, encoder);

  // set output lengths
  *toLen = encoder->no_of_bytes;
  *fromLen = encoder->blockl*2;

  return 1; 
}


static int codec_decoder(const struct PluginCodec_Definition * codec, 
                                           void * context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  int i;
  float block[BLOCKL_MAX];

  struct iLBC_Dec_Inst_t_ * decoder = (struct iLBC_Dec_Inst_t_ *)context;
  short * sampleBuffer = (short *)to;

  // If packet not have integral number of frames for this mode
  if ((*fromLen % decoder->no_of_bytes) != 0) {
    // Then switch to the other mode
    initDecode(context, decoder->mode == 20 ? 30 : 20, 0); 
    if ((*fromLen % decoder->no_of_bytes) != 0)
      return 0; // Still wrong, what are they sending us?
  }

  /* do actual decoding of block */ 
  iLBC_decode(block, (unsigned char *)from, decoder, 1);

  if (*toLen < decoder->blockl*2U)
    return 0;

  /* convert to short */     
  for (i = 0; i < decoder->blockl; i++) {
    float tmp = block[i];
    if (tmp < MIN_SAMPLE)
      tmp = MIN_SAMPLE;
    else if (tmp > MAX_SAMPLE)
      tmp = MAX_SAMPLE;
    sampleBuffer[i] = (short)tmp;
  }

  // set output lengths
  *toLen = decoder->blockl*2;
  *fromLen = decoder->no_of_bytes;

  return 1;
}


static int valid_for_h323(const struct PluginCodec_Definition * codec,
                          void * context,
                          const char * key,
                          void * parm,
                          unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char *))
    return 0;

  return (STRCMPI((const char *)parm, "h.323") == 0 ||
          STRCMPI((const char *)parm, "h323") == 0) ? 1 : 0;

}


/* generic parameters; see H.245 Annex S */
enum
{
    H245_iLBC_MAXAL_SDUFRAMES = 0 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode,
    H245_iLBC_MODE            = 1 | PluginCodec_H245_Collapsing | PluginCodec_H245_TCS | PluginCodec_H245_OLC | PluginCodec_H245_ReqMode
};


static struct PluginCodec_Option const RxFramesPerPacket =
{
  PluginCodec_IntegerOption,  // PluginCodec_OptionTypes
  "Rx Frames Per Packet",     // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "5",                        // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  H245_iLBC_MAXAL_SDUFRAMES,  // H.245 Generic Capability number and scope bits
  "1",                        // Minimum value
  "10"                        // Maximum value
};

static const char PreferredModeStr[] = "Preferred Mode";
static struct PluginCodec_Option const PreferredMode =
{
  PluginCodec_IntegerOption,  // PluginCodec_OptionTypes
  PreferredModeStr,           // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MaxMerge,       // Merge mode
  "20",                       // Initial value
  "mode",                     // SIP/SDP FMTP name
  "+30",                      // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  H245_iLBC_MODE,             // H.245 Generic Capability number and scope bits
  "20",                       // Minimum value
  "30"                        // Maximum value
};

/* NOTE: There is a small trick in the above. RFC3952 which control the SIP
   FMTP parameters never mentions what the default value should be if the mode
   parameter is missing. Asterisk (typically) does not include the parameter
   AND assumes 30ms as well. So, on receipt we want to make sure a 30ms time
   is used, but for transmission we ALWAYS want to include the parameter so
   the "FMTP default value" must not match the current value. As the latter is
   a string comparison, but the former is converted from a string to an
   integer we take advatage of this by putting the explicit '+' in front of
   the default value. Sneaky, but effective for now.
 */


static struct PluginCodec_Option const * const OptionTable[] = {
  &RxFramesPerPacket,
  &PreferredMode,
  NULL
};

static int get_codec_options(const struct PluginCodec_Definition * defn,
                                                            void * context, 
                                                      const char * name,
                                                            void * parm,
                                                        unsigned * parmLen)
{
  if (parm == NULL || parmLen == NULL || *parmLen != sizeof(struct PluginCodec_Option **))
    return 0;

  *(struct PluginCodec_Option const * const * *)parm = OptionTable;
  *parmLen = 0;
  return 1;
}


static int get_mode(const char * str)
{
  int mode = atoi(str);
  return mode == 0 || mode > 25 ? 30 : 20;
}


static int set_codec_options(const struct PluginCodec_Definition * defn,
                                                            void * context,
                                                      const char * name, 
                                                            void * parm, 
                                                        unsigned * parmLen)
{
  const char * const * option;

  if (context == NULL || parm == NULL || parmLen == NULL || *parmLen != sizeof(const char **))
    return 0;

  for (option = (const char * const *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], PreferredModeStr) == 0) {
      unsigned mode = get_mode(option[1]);
      if (defn->destFormat[0] == 'L')
        initDecode(context, mode, 0);
      else
        initEncode(context, mode);
    }
  }

  return 1;
}


static int to_normalised_options(const struct PluginCodec_Definition * defn,
                                                                void * context,
                                                          const char * name, 
                                                                void * parm, 
                                                            unsigned * parmLen)
{
  char frameTime[20], frameSize[20], newMode[20];
  const char * const * option;

  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  frameTime[0] = frameSize[0] = newMode[0] = '\0';

  for (option = *(const char * const * *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], PreferredModeStr) == 0) {
      int mode = get_mode(option[1]);
      sprintf(frameTime, "%i", mode == 30 ? BLOCKL_30MS      : BLOCKL_20MS);
      sprintf(frameSize, "%i", mode == 30 ? NO_OF_BYTES_30MS : NO_OF_BYTES_20MS);
      sprintf(newMode,   "%i", mode);
    }
  }

  if (frameTime[0] != '\0') {
    char ** options = (char **)calloc(7, sizeof(char *));
    *(char ***)parm = options;
    if (options == NULL)
      return 0;

    options[0] = strdup(PLUGINCODEC_OPTION_FRAME_TIME);
    options[1] = strdup(frameTime);
    options[2] = strdup(PLUGINCODEC_OPTION_MAX_FRAME_SIZE);
    options[3] = strdup(frameSize);
    options[4] = strdup(PreferredModeStr);
    options[5] = strdup(newMode);
  }

  return 1;
}


static int free_codec_options(const struct PluginCodec_Definition * defn,
                                                             void * context,
                                                       const char * name, 
                                                             void * parm, 
                                                         unsigned * parmLen)
{
  char ** strings;
  char ** string;

  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  strings = (char **) parm;
  for (string = strings; *string != NULL; string++)
    free(*string);
  free(strings);
  return 1;
}


static struct PluginCodec_ControlDefn h323CoderControls[] = {
  { PLUGINCODEC_CONTROL_VALID_FOR_PROTOCOL, valid_for_h323 },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,  set_codec_options },
  { NULL }
};

static struct PluginCodec_ControlDefn h323AndSIPCoderControls[] = {
  { PLUGINCODEC_CONTROL_TO_NORMALISED_OPTIONS, to_normalised_options },
  { PLUGINCODEC_CONTROL_FREE_CODEC_OPTIONS,    free_codec_options },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,     set_codec_options },
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { NULL }
};


/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information licenseInfo = {
  //1073187324,				                       // timestamp = Sun 04 Jan 2004 03:35:24 AM UTC =
  1101695533,                            // Mon 29 Nov 2004 12:32:13 PM EST

  "Craig Southeren, Post Increment",                           // source code author
  "2.0",                                                       // source code version
  "craigs@postincrement.com",                                  // source code email
  "http://www.postincrement.com",                              // source code URL
  "Copyright (C) 2004-2007 by Post Increment, All Rights Reserved", // source code copyright
  "MPL 1.0",                                                   // source code license
  PluginCodec_License_MPL,                                     // source code license

  "iLBC (internet Low Bitrate Codec)",                         // codec description
  "Global IP Sound, Inc.",                                     // codec author
  NULL,                                                        // codec version
  "info@globalipsound.com",                                    // codec email
  "http://www.ilbcfreeware.org",                               // codec URL
  "Global IP Sound AB. Portions Copyright (C) 1999-2002, All Rights Reserved",          // codec copyright information
  "Global IP Sound iLBC Freeware Public License, IETF Version, Limited Commercial Use", // codec license
  PluginCodec_License_Freeware                                // codec license code
};

static const char L16Desc[]  = { "L16" };

static const char iLBC13k3[] = { "iLBC-13k3" };
static const char iLBC15k2[] = { "iLBC-15k2" };

static const char sdpILBC[]  = { "iLBC" };

static const struct PluginCodec_H323GenericCodecData ilbcCap =
{
  OpalPluginCodec_Identifer_iLBC, // capability identifier (Ref: Table I.1 in H.245)
  122                             // Must always be this regardless of "Max Bit Rate" option
};


#define	EQUIVALENCE_COUNTRY_CODE            9
#define	EQUIVALENCE_EXTENSION_CODE          0
#define	EQUIVALENCE_MANUFACTURER_CODE       61

static struct PluginCodec_H323NonStandardCodecData ilbc13k3Cap =
{
  NULL, 
  EQUIVALENCE_COUNTRY_CODE, 
  EQUIVALENCE_EXTENSION_CODE, 
  EQUIVALENCE_MANUFACTURER_CODE,
  iLBC13k3, sizeof(iLBC13k3)-1,
  NULL
};

static struct PluginCodec_H323NonStandardCodecData ilbc15k2Cap =
{
  NULL, 
  EQUIVALENCE_COUNTRY_CODE, 
  EQUIVALENCE_EXTENSION_CODE, 
  EQUIVALENCE_MANUFACTURER_CODE,
  iLBC15k2, sizeof(iLBC15k2)-1,
  NULL
};

static struct PluginCodec_Definition iLBCCodecDefn[] =
{
  { 
    // encoder for SIP and H.323 via H.245 Annex S
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeShared |         // share RTP code 
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    sdpILBC,                            // text decription
    L16Desc,                            // source format
    sdpILBC,                            // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITRATE_20MS,                       // raw bits per second (note we use highest value here)
    30000,                              // nanoseconds per frame (note we use highest value here)
    BLOCKL_30MS,                        // samples per frame (note we use highest value here)
    NO_OF_BYTES_30MS,                   // bytes per frame (note we use highest value here)
    2,                                  // recommended number of frames per packet
    5,                                  // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpILBC,                            // RTP payload name

    create_encoder,                     // create codec function
    destroy_context,                    // destroy codec
    codec_encoder,                      // encode/decode
    h323AndSIPCoderControls,            // codec controls

    PluginCodec_H323Codec_generic,      // h323CapabilityType 
    &ilbcCap                            // h323CapabilityData
  },

  { 
    // decoder for SIP and H.323 via H.245 Annex S
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeShared |         // share RTP code 
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    sdpILBC,                            // text decription
    sdpILBC,                            // source format
    L16Desc,                            // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITRATE_20MS,                       // raw bits per second (note we use highest value here)
    30000,                              // nanoseconds per frame (note we use highest value here)
    BLOCKL_30MS,                        // samples per frame (note we use highest value here)
    NO_OF_BYTES_30MS,                   // bytes per frame (note we use highest value here)
    2,                                  // recommended number of frames per packet
    5,                                  // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpILBC,                            // RTP payload name

    create_decoder,                     // create codec function
    destroy_context,                    // destroy codec
    codec_decoder,                      // encode/decode
    h323AndSIPCoderControls,            // codec controls

    PluginCodec_H323Codec_generic,      // h323CapabilityType 
    &ilbcCap                            // h323CapabilityData
  },

  { 
    // encoder for H.323 only using OpenH323 legacy capability at 13k3
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeShared |         // share RTP code 
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    iLBC13k3,                           // text decription
    L16Desc,                            // source format
    iLBC13k3,                           // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITRATE_30MS,                       // raw bits per second
    30000,                              // nanoseconds per frame
    BLOCKL_30MS,                        // samples per frame
    NO_OF_BYTES_30MS,                   // bytes per frame
    1,                                  // recommended number of frames per packet
    1,                                  // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpILBC,                            // RTP payload name

    create_encoder,                     // create codec function
    destroy_context,                    // destroy codec
    codec_encoder,                      // encode/decode
    h323CoderControls,                  // codec controls

    PluginCodec_H323Codec_nonStandard,  // h323CapabilityType
    &ilbc13k3Cap                        // h323CapabilityData
  },

  { 
    // decoder for H.323 only using OpenH323 legacy capability at 13k3
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeShared |         // share RTP code 
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    iLBC13k3,                           // text decription
    iLBC13k3,                           // source format
    L16Desc,                            // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITRATE_30MS,                       // raw bits per second
    30000,                              // nanoseconds per frame
    BLOCKL_30MS,                        // samples per frame
    NO_OF_BYTES_30MS,                   // bytes per frame
    1,                                  // recommended number of frames per packet
    1,                                  // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpILBC,                            // RTP payload name

    create_decoder,                     // create codec function
    destroy_context,                    // destroy codec
    codec_decoder,                      // encode/decode
    h323CoderControls,                  // codec controls

    PluginCodec_H323Codec_nonStandard,  // h323CapabilityType 
    &ilbc13k3Cap                        // h323CapabilityData
  },

  { 
    // encoder for H.323 only using OpenH323 legacy capability at 15k2
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeShared |         // share RTP code 
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    iLBC15k2,                           // text decription
    L16Desc,                            // source format
    iLBC15k2,                           // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITRATE_20MS,                       // raw bits per second
    20000,                              // nanoseconds per frame
    BLOCKL_20MS,                        // samples per frame
    NO_OF_BYTES_20MS,                   // bytes per frame
    1,                                  // recommended number of frames per packet
    1,                                  // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpILBC,                            // RTP payload name

    create_encoder,                     // create codec function
    destroy_context,                    // destroy codec
    codec_encoder,                      // encode/decode
    h323CoderControls,                  // codec controls

    PluginCodec_H323Codec_nonStandard,  // h323CapabilityType 
    &ilbc15k2Cap                        // h323CapabilityData
  },

  { 
    // decoder for H.323 only using OpenH323 legacy capability at 15k2
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeShared |         // share RTP code 
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    iLBC15k2,                           // text decription
    iLBC15k2,                           // source format
    L16Desc,                            // destination format

    NULL,                               // user data

    8000,                               // samples per second
    BITRATE_20MS,                       // raw bits per second
    20000,                              // nanoseconds per frame
    BLOCKL_20MS,                        // samples per frame
    NO_OF_BYTES_20MS,                   // bytes per frame
    1,                                  // recommended number of frames per packet
    1,                                  // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpILBC,                            // RTP payload name

    create_decoder,                     // create codec function
    destroy_context,                    // destroy codec
    codec_decoder,                      // encode/decode
    h323CoderControls,                  // codec controls

    PluginCodec_H323Codec_nonStandard,  // h323CapabilityType 
    &ilbc15k2Cap                        // h323CapabilityData
  }
};


PLUGIN_CODEC_IMPLEMENT_ALL(iLBC, iLBCCodecDefn, PLUGIN_CODEC_VERSION_OPTIONS)

/////////////////////////////////////////////////////////////////////////////
