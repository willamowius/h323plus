/*
 *  RFC3267/RFC4867 GSM-AMR Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) 2004 MX Telecom Ltd.
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

#include "src/interf_enc.h"
#include "src/interf_dec.h"
#include "src/interf_rom.h"
#include "src/rom_dec.h"

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#if defined(_WIN32) || defined(_WIN32_WCE)
  #define STRCMPI  _strcmpi
#else
  #define STRCMPI  strcasecmp
#endif

/*Disable some warnings on VC++*/
#ifdef _MSC_VER
#pragma warning(disable : 4100)
#endif


/***************************************************************************
 *
 * This plugin implements an interface to the GSM-AMR codec for OpenH323 (H.245 Annex I)
 * This should not be confused with GSM-AMR-NB (H.245 Annex R)
 *
 * The amr codec itself is not distributed with this plugin. See amrcodec.txt
 * in the src subdirectory of the plugin source.
 *
 * This code can currently only handle one frame per packet due to the RFC3267
 * packetisation method which is ... difficult ... with the current OPAL plug in
 * architecture. So make sure "Rx Frames Per Packet" doesn;t change till it's fixed!
 *
 **************************************************************************/
    
/* generic parameters; see H.245 Annex I */
enum
{
    H245_AMR_MAXAL_SDUFRAMES    = 0 | PluginCodec_H245_Collapsing                               | PluginCodec_H245_TCS | PluginCodec_H245_OLC,
    H245_AMR_BITRATE            = 1 | PluginCodec_H245_NonCollapsing | PluginCodec_H245_ReqMode,
    H245_AMR_GSMAMRCOMFORTNOISE = 2 | PluginCodec_H245_Collapsing    | PluginCodec_H245_ReqMode                        | PluginCodec_H245_OLC,
    H245_AMR_GSMEFRCOMFORTNOISE = 3 | PluginCodec_H245_Collapsing    | PluginCodec_H245_ReqMode | PluginCodec_H245_TCS | PluginCodec_H245_OLC,
    H245_AMR_IS_641COMFORTNOISE = 4 | PluginCodec_H245_Collapsing    | PluginCodec_H245_ReqMode | PluginCodec_H245_TCS | PluginCodec_H245_OLC,
    H245_AMR_PDCEFRCOMFORTNOISE = 5 | PluginCodec_H245_Collapsing    | PluginCodec_H245_ReqMode | PluginCodec_H245_TCS | PluginCodec_H245_OLC
};

/* values of the bit rate parameter */
enum
{
    AMR_BITRATE_475 = 0,
    AMR_BITRATE_515,
    AMR_BITRATE_590,
    AMR_BITRATE_670,
    AMR_BITRATE_740,
    AMR_BITRATE_795,
    AMR_BITRATE_1020,
    AMR_BITRATE_1220,
    AMR_BITRATE_CN,
    AMR_BITRATE_EFR,
    AMR_BITRATE_IS641,
    AMR_BITRATE_PDCEFR,
    AMR_BITRATE_DTX = 15
};
    

// this is what we hand back when we are asked to create an encoder
typedef struct
{
  void    * encoder_state;  // Encoder interface's opaque state
  unsigned  mode;           // current mode
  int       vad;            // silence suppression
} AmrEncoderContext;


/////////////////////////////////////////////////////////////////////////////

static void * amr_create_encoder(const struct PluginCodec_Definition * codec)
{
  AmrEncoderContext * amr = malloc(sizeof(AmrEncoderContext));
  if (amr == NULL )
    return NULL;

  amr->encoder_state = Encoder_Interface_init(amr->vad);
  if (amr->encoder_state == NULL ) {
    free(amr);
    return NULL;
  }

  amr->mode = AMR_BITRATE_1220; // start off in mode 12.2kbps mode
  amr->vad = TRUE;
  return amr;
}


static void * amr_create_decoder(const struct PluginCodec_Definition * codec)
{
  return Decoder_Interface_init();
}


static void amr_destroy_encoder(const struct PluginCodec_Definition * codec, void * context)
{
  AmrEncoderContext * amr = (AmrEncoderContext *)context;
  Encoder_Interface_exit(amr->encoder_state);
  free(amr);
}


static void amr_destroy_decoder(const struct PluginCodec_Definition * codec, void * context)
{
  Decoder_Interface_exit(context);
}


static int amr_codec_encoder(const struct PluginCodec_Definition * codec, 
                                                            void * context,
                                                      const void * fromPtr, 
                                                        unsigned * fromLen,
                                                            void * toPtr,         
                                                        unsigned * toLen,
                                                    unsigned int * flag)
{
  AmrEncoderContext * amr = (AmrEncoderContext *)context;
  unsigned byteCount;
  unsigned char buffer[100]; // Need this as encoder is very rude and can output more bytes than to pointer might be pointing to

  if (*fromLen < L_FRAME*sizeof(short))
    return FALSE;

  byteCount = Encoder_Interface_Encode(amr->encoder_state, (enum Mode)amr->mode, (const short *)fromPtr, buffer+1, 0);
  if (byteCount <= 1 || *toLen <= byteCount) {
    *toLen = 0;
    return byteCount == 1; // Is a DTX frame
  }

  buffer[0] = 0xf0;  // CMR is always this for us

  memcpy(toPtr, buffer, *toLen);
  *toLen = byteCount+1;
  *fromLen = L_FRAME*sizeof(short);

  return TRUE; 
}


static int amr_codec_decoder(const struct PluginCodec_Definition * codec, 
                                                            void * context,
                                                      const void * fromPtr, 
                                                        unsigned * fromLen,
                                                            void * toPtr,         
                                                        unsigned * toLen,
                                                    unsigned int * flag)
{
  unsigned char * packet;

  if (*toLen < L_FRAME*sizeof(short))
    return FALSE;

  if (fromLen == NULL || *fromLen == 0) {
    unsigned char buffer[32];
    buffer[0] = (AMR_BITRATE_DTX << 3)|4;
    Decoder_Interface_Decode(context, buffer, (short *)toPtr, 0); // Handle missing data
    return TRUE;
  }

  packet = (unsigned char *)fromPtr;
  Decoder_Interface_Decode(context, packet+1, (short *)toPtr, 0); // Skip over CMR

  *fromLen = block_size[packet[1] >> 3] + 1; // Actual bytes consumed
  *toLen = L_FRAME*sizeof(short);

  return TRUE;
}


static int amr_set_quality(const struct PluginCodec_Definition * codec, void * context, 
                           const char * name, void * parm, unsigned * parmLen)
{
  int quality;

  if (parm == NULL || *parmLen != sizeof(quality))
    return FALSE;

  quality = *(int *)parm;

  if (quality < 0)
    quality = 0;
  else if (quality > 31)
    quality = 31;

  /* 1-3   -> mode 7
     4-7   -> mode 6
     ...
     28-31 -> mode 0
  */
  ((AmrEncoderContext *)context)->mode = 7 -(quality/4);
  return TRUE;
}


static int amr_get_quality(const struct PluginCodec_Definition * codec, void * context, 
                           const char * name, void * parm, unsigned * parmLen)
{
  if (parm == NULL || *parmLen != sizeof(int))
    return FALSE;

  *(int *)parm = (7 - ((AmrEncoderContext *)context)->mode)*4;
  return TRUE;
}


/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Option const amrRxFramesPerPacket =
{
  PluginCodec_IntegerOption,  // PluginCodec_OptionTypes
  "Rx Frames Per Packet",     // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_MinMerge,       // Merge mode
  "1",                        // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  H245_AMR_MAXAL_SDUFRAMES,   // H.245 Generic Capability number and scope bits
  "1",                        // Minimum value
  "1"                         // Maximum value    // Do not change!! See above.
};

static const char InitialModeStr[] = "Initial Mode";
static struct PluginCodec_Option const amrInitialMode =
{
  PluginCodec_IntegerOption,  // PluginCodec_OptionTypes
  InitialModeStr,             // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_NoMerge,        // Merge mode
  "7",                        // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  H245_AMR_BITRATE,           // H.245 Generic Capability number and scope bits
  "0",                        // Minimum value
  "7"                         // Maximum value
};

static const char VADStr[] = "VAD";
static struct PluginCodec_Option const amrVAD =
{
  PluginCodec_BoolOption,     // PluginCodec_OptionTypes
  VADStr,                     // Generic (human readable) option name
  0,                          // Read Only flag
  PluginCodec_AndMerge,       // Merge mode
  "True",                     // Initial value
  NULL,                       // SIP/SDP FMTP name
  NULL,                       // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  H245_AMR_GSMAMRCOMFORTNOISE // H.245 Generic Capability number and scope bitsSE,
};

static struct PluginCodec_Option const amrMediaPacketization =
{
  PluginCodec_StringOption, // PluginCodec_OptionTypes
  "Media Packetization",    // Generic (human readable) option name
  0,                        // Read Only flag
  PluginCodec_EqualMerge,   // Merge mode
  "RFC3267"                 // Initial value
};

static struct PluginCodec_Option const * const amrOptionTable[] = {
  &amrRxFramesPerPacket,
  &amrInitialMode,
  &amrVAD,
  &amrMediaPacketization,
  NULL
};

static int get_codec_options(const struct PluginCodec_Definition * defn,
                                                            void * context, 
                                                      const char * name,
                                                            void * parm,
                                                        unsigned * parmLen)
{
  if (parm == NULL || parmLen == NULL || *parmLen != sizeof(struct PluginCodec_Option **))
    return FALSE;

  *(struct PluginCodec_Option const * const * *)parm = amrOptionTable;
  return TRUE;
}


static int set_codec_options(const struct PluginCodec_Definition * defn,
                                                            void * context,
                                                      const char * name, 
                                                            void * parm, 
                                                        unsigned * parmLen)
{
  const char * const * option;
  AmrEncoderContext * amr;

  if (context == NULL || parm == NULL || parmLen == NULL || *parmLen != sizeof(const char **))
    return FALSE;

  amr = (AmrEncoderContext *)context;

  for (option = (const char * const *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], InitialModeStr) == 0) {
      amr->mode = strtoul(option[1], NULL, 10);
      if (amr->mode > AMR_BITRATE_1220)
        amr->mode = AMR_BITRATE_1220;
    }
    else if (STRCMPI(option[0], VADStr) == 0)
      amr->vad = atoi(option[1]) != 0;
  }

  return TRUE;
}


/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information licenseInfo = {
    // Tue 13 Jul 2004 00:11:32 UTC =
    1089677492,

    "Richard van der Hoff, MX Telecom Ltd.",                     // source code author
    "$Ver$",                                                     // source code version
    "richardv@mxtelecom.com",                                    // source code email
    "http://www.mxtelecom.com",                                  // source code URL
    "Copyright (C) 2004 MX Telecom Ltd.",                          // source code copyright
    "None",                                                      // source code license  // FIXME
    PluginCodec_License_None,                                    // source code license
    
    "GSM-AMR (Adaptive Multirate Codec)",                        // codec description
    "3rd Generation Partnership Project",                        // codec author
    NULL,                                                        // codec version
    NULL,                                                         // codec email
    "http://www.3gpp.org",                                         // codec URL
    "",                                                           // codec copyright information
    "",                                                          // codec license
    PluginCodec_License_RoyaltiesRequired                        // codec license code
};

static struct PluginCodec_ControlDefn amrEncoderControlDefn[] = {
  { "get_codec_options", get_codec_options },
  { "set_codec_options", set_codec_options },
  { "set_quality",       amr_set_quality },
  { "get_quality",       amr_get_quality },
  { NULL }
};

static const struct PluginCodec_H323GenericCodecData amrcap =
{
  OpalPluginCodec_Identifer_AMR,  // capability identifier (Ref: Table I.1 in H.245)
  122                             // Must always be this regardless of "Max Bit Rate" option
};

static struct PluginCodec_Definition amrCodecDefn[] = {
  {
    // encoder
    PLUGIN_CODEC_VERSION_OPTIONS,           // codec API version
    &licenseInfo,                           // license information

    PluginCodec_MediaTypeAudio |            // audio codec
    PluginCodec_InputTypeRaw |              // raw input data
    PluginCodec_OutputTypeRaw |             // raw output data
    PluginCodec_RTPTypeDynamic,             // dynamic RTP type
    
    "GSM-AMR",                              // text decription
    "L16",                                  // source format
    "GSM-AMR",                              // destination format
    
    NULL,                                   // user data

    8000,                                   // samples per second
    12200,                                  // raw bits per second
    20000,                                  // microseconds per frame
    L_FRAME,                                // samples per frame
    33,                                     // bytes per frame
    
    1,                                      // recommended number of frames per packet
    1,                                      // maximum number of frames per packet
    0,                                      // IANA RTP payload code
    "AMR",                                  // RTP payload name
    
    amr_create_encoder,                     // create codec function
    amr_destroy_encoder,                    // destroy codec
    amr_codec_encoder,                      // encode/decode
    amrEncoderControlDefn,                  // codec controls

    PluginCodec_H323Codec_generic,          // h323CapabilityType
    &amrcap                                 // h323CapabilityData
  },
  { 
    // decoder
    PLUGIN_CODEC_VERSION_OPTIONS,           // codec API version
    &licenseInfo,                           // license information

    PluginCodec_MediaTypeAudio |            // audio codec
    PluginCodec_InputTypeRaw |              // raw input data
    PluginCodec_OutputTypeRaw |             // raw output data
    PluginCodec_RTPTypeDynamic |            // dynamic RTP type
    PluginCodec_DecodeSilence,              // Can accept missing (empty) frames and generate silence

    "GSM-AMR",                              // text decription
    "GSM-AMR",                              // source format
    "L16",                                  // destination format

    NULL,                                   // user data

    8000,                                   // samples per second
    12200,                                  // raw bits per second
    30000,                                  // microseconds per frame
    L_FRAME,                                // samples per frame
    33,                                     // bytes per frame
    1,                                      // recommended number of frames per packet
    1,                                      // maximum number of frames per packet
    0,                                      // IANA RTP payload code
    "AMR",                                  // RTP payload name

    amr_create_decoder,                     // create codec function
    amr_destroy_decoder,                    // destroy codec
    amr_codec_decoder,                      // encode/decode
    NULL,                                   // codec controls
    
    PluginCodec_H323Codec_generic,          // h323CapabilityType 
    &amrcap                                 // h323CapabilityData
  }
};


PLUGIN_CODEC_IMPLEMENT_ALL(AMR, amrCodecDefn, PLUGIN_CODEC_VERSION_OPTIONS)

/////////////////////////////////////////////////////////////////////////////
