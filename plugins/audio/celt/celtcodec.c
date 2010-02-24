/*
 * CELT Codec Plugin for Opal
 *
 * Based on the GSM-AMR one
 */

#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _CRT_SECURE_NO_WARNINGS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugin-config.h"

#include <codec/opalplugin.h>
#include <celt/celt.h>

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

// this is what we hand back when we are asked to create an encoder
typedef struct
{
  CELTDecoder *decoder_state;
  CELTEncoder *encoder_state;
  CELTMode    *mode;
  int frame_size;
  int bytes_per_packet;
} CELTContext;


/////////////////////////////////////////////////////////////////////////////

static int init_mode(CELTContext *celt, const struct PluginCodec_Definition * codec)
{
  int error = 0;

  celt->mode = celt_mode_create(codec->sampleRate, 1, codec->parm.audio.samplesPerFrame, &error);
  if (celt->mode == NULL) {
    return FALSE;
  }

  celt_mode_info(celt->mode, CELT_GET_FRAME_SIZE, &celt->frame_size);
  celt->bytes_per_packet = (codec->bitsPerSec * celt->frame_size/codec->sampleRate + 4) / 8;

  return TRUE;
}

static void * celt_create_encoder(const struct PluginCodec_Definition * codec)
{
  CELTContext * celt = malloc(sizeof(CELTContext));
  if (celt == NULL)
    return NULL;

  if (init_mode(celt, codec) == FALSE) {
    free(celt);
    return NULL;
  }
 	
  celt->encoder_state = celt_encoder_create(celt->mode);
  if (celt->encoder_state == NULL ) {
    celt_mode_destroy(celt->mode);
    free(celt);
    return NULL;
  }

  return celt;
}


static void * celt_create_decoder(const struct PluginCodec_Definition * codec)
{
  CELTContext * celt = malloc(sizeof(CELTContext));
  if (celt == NULL)
    return NULL;

  if (init_mode(celt, codec) == FALSE) {
    free(celt);
    return NULL;
  }

  celt->decoder_state = celt_decoder_create(celt->mode);
  if (celt->decoder_state == NULL ) {
    celt_mode_destroy(celt->mode);
    free(celt);
    return NULL;
  }

  return celt;
}


static void celt_destroy_encoder(const struct PluginCodec_Definition * codec, void * context)
{
  CELTContext * celt = (CELTContext *)context;
  celt_encoder_destroy(celt->encoder_state);
  celt_mode_destroy(celt->mode);
  free(celt);
}


static void celt_destroy_decoder(const struct PluginCodec_Definition * codec, void * context)
{
  CELTContext * celt = (CELTContext *)context;
  celt_decoder_destroy(celt->decoder_state);
  celt_mode_destroy(celt->mode);
  free(celt);
}


static int celt_codec_encoder(const struct PluginCodec_Definition * codec, 
                                                            void * context,
                                                      const void * fromPtr, 
                                                        unsigned * fromLen,
                                                            void * toPtr,         
                                                        unsigned * toLen,
                                                    unsigned int * flag)
{
  CELTContext *celt = (CELTContext *)context;
  unsigned byteCount;

  if (*fromLen < codec->parm.audio.samplesPerFrame*sizeof(short))
    return FALSE;

  if (*toLen < celt->bytes_per_packet)
    return FALSE;

#ifdef HAVE_CELT_0_5_0_OR_LATER
  byteCount = celt_encode(celt->encoder_state, (celt_int16_t *)fromPtr, NULL, (char *)toPtr, celt->bytes_per_packet);
#else
  byteCount = celt_encode(celt->encoder_state, (celt_int16_t *)fromPtr, (char *)toPtr, celt->bytes_per_packet);
#endif
  if (byteCount < 0) {
	return 0;
  }
  *toLen = byteCount;
  *fromLen = codec->parm.audio.samplesPerFrame*sizeof(short);

  return TRUE; 
}


static int celt_codec_decoder(const struct PluginCodec_Definition * codec, 
                                                            void * context,
                                                      const void * fromPtr, 
                                                        unsigned * fromLen,
                                                            void * toPtr,         
                                                        unsigned * toLen,
                                                    unsigned int * flag)
{
  CELTContext *celt = (CELTContext *)context;

  if (*toLen < codec->parm.audio.samplesPerFrame*sizeof(short))
    return FALSE;

  if (*fromLen == 0)
    return FALSE;

  if (celt_decode(celt->decoder_state, (char *)fromPtr, *fromLen, (short *)toPtr) < 0) {
    return 0;
  }

  *toLen = codec->parm.audio.samplesPerFrame*sizeof(short);

  return TRUE;
}

/* taken from Speex */
static int valid_for_sip(
      const struct PluginCodec_Definition * codec, 
      void * context, 
      const char * key, 
      void * parm, 
      unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char *))
    return 0;

  return (STRCMPI((const char *)parm, "sip") == 0) ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_ControlDefn celt_codec_controls[] = {
  { "valid_for_protocol",       valid_for_sip },
  { NULL }
};

static struct PluginCodec_information licenseInfo = {
    // Fri Dec 13 2008, 23:37:31 CET =
    1229729851,

    "Stefan Knoblich, axsentis GmbH",                            // source code author
    "0.1",                                                       // source code version
    "s.knoblich@axsentis.de",                                    // source code email
    "http://oss.axsentis.de/",                                   // source code URL
    "Copyright (C) 2008 axsentis GmbH",                          // source code copyright
    "BSD license",                                               // source code license
    PluginCodec_License_BSD,                                     // source code license
    
    "CELT (ultra-low delay audio codec)",                        // codec description
    "Jean-Marc Valin, Xiph Foundation.",                         // codec author
    "",                                                          // codec version
    "jean-marc.valin@hermes.usherb.ca",                          // codec email
    "http://www.celt-codec.org",                                 // codec URL
    "(C) 2008 Xiph.Org Foundation, All Rights Reserved",         // codec copyright information
    "Xiph BSD license",                                          // codec license
    PluginCodec_License_BSD                                      // codec license code
};

static struct PluginCodec_Definition celtCodecDefn[] = {
  /* 32KHz */
  {
    // encoder
    PLUGIN_CODEC_VERSION_OPTIONS,           // codec API version
    &licenseInfo,                           // license information

    PluginCodec_MediaTypeAudio |            // audio codec
    PluginCodec_InputTypeRaw |              // raw input data
    PluginCodec_OutputTypeRaw |             // raw output data
    PluginCodec_RTPTypeShared |
    PluginCodec_RTPTypeDynamic,             // dynamic RTP type
    
    "CELT-32K",                             // text decription
    "L16",                                  // source format
    "CELT-32K",                             // destination format
    
    NULL,                                   // user data

    32000,                                  // samples per second
    32000,                                  // raw bits per second
    10000,                                  // microseconds per frame
    {{
      320,                                  // samples per frame
      40,                                   // bytes per frame
      1,                                    // recommended number of frames per packet
      1,                                    // maximum number of frames per packet
    }},
    0,                                      // IANA RTP payload code
    "CELT",                                 // RTP payload name
    
    celt_create_encoder,                    // create codec function
    celt_destroy_encoder,                   // destroy codec
    celt_codec_encoder,                     // encode/decode
    celt_codec_controls,                    // codec controls

    PluginCodec_H323Codec_NoH323,
    NULL
  },
  { 
    // decoder
    PLUGIN_CODEC_VERSION_OPTIONS,           // codec API version
    &licenseInfo,                           // license information

    PluginCodec_MediaTypeAudio |            // audio codec
    PluginCodec_InputTypeRaw |              // raw input data
    PluginCodec_OutputTypeRaw |             // raw output data
    PluginCodec_RTPTypeShared |
    PluginCodec_RTPTypeDynamic,             // dynamic RTP type

#if 0	/* supported ???*/
    PluginCodec_DecodeSilence,              // Can accept missing (empty) frames and generate silence
#endif

    "CELT-32K",                             // text decription
    "CELT-32K",                             // source format
    "L16",                                  // destination format

    NULL,                                   // user data

    32000,                                  // samples per second
    32000,                                  // raw bits per second
    10000,                                  // microseconds per frame
    {{
      320,                                  // samples per frame
      40,                                   // bytes per frame
      1,                                    // recommended number of frames per packet
      1,                                    // maximum number of frames per packet
    }},
    0,                                      // IANA RTP payload code
    "CELT",                                 // RTP payload name

    celt_create_decoder,                    // create codec function
    celt_destroy_decoder,                   // destroy codec
    celt_codec_decoder,                     // encode/decode
    celt_codec_controls,                    // codec controls
    
    PluginCodec_H323Codec_NoH323,
    NULL
  },

  /* 48 KHz */
  {
    // encoder
    PLUGIN_CODEC_VERSION_OPTIONS,           // codec API version
    &licenseInfo,                           // license information

    PluginCodec_MediaTypeAudio |            // audio codec
    PluginCodec_InputTypeRaw |              // raw input data
    PluginCodec_OutputTypeRaw |             // raw output data
    PluginCodec_RTPTypeShared |
    PluginCodec_RTPTypeDynamic,             // dynamic RTP type
    
    "CELT-48K",                             // text decription
    "L16",                                  // source format
    "CELT-48K",                             // destination format
    
    NULL,                                   // user data

    48000,                                  // samples per second
    48000,                                  // raw bits per second
    10000,                                  // microseconds per frame
    {{
      480,                                  // samples per frame
      60,                                   // bytes per frame
      1,                                    // recommended number of frames per packet
      1,                                    // maximum number of frames per packet
    }},
    0,                                      // IANA RTP payload code
    "CELT",                                 // RTP payload name
    
    celt_create_encoder,                    // create codec function
    celt_destroy_encoder,                   // destroy codec
    celt_codec_encoder,                     // encode/decode
    celt_codec_controls,                    // codec controls

    PluginCodec_H323Codec_NoH323,
    NULL
  },
  { 
    // decoder
    PLUGIN_CODEC_VERSION_OPTIONS,           // codec API version
    &licenseInfo,                           // license information

    PluginCodec_MediaTypeAudio |            // audio codec
    PluginCodec_InputTypeRaw |              // raw input data
    PluginCodec_OutputTypeRaw |             // raw output data
    PluginCodec_RTPTypeShared |
    PluginCodec_RTPTypeDynamic,             // dynamic RTP type

#if 0	/* supported ???*/
    PluginCodec_DecodeSilence,              // Can accept missing (empty) frames and generate silence
#endif

    "CELT-48K",                             // text decription
    "CELT-48K",                             // source format
    "L16",                                  // destination format

    NULL,                                   // user data

    48000,                                  // samples per second
    48000,                                  // raw bits per second
    10000,                                  // microseconds per frame
    {{
      480,                                  // samples per frame
      60,                                   // bytes per frame
      1,                                    // recommended number of frames per packet
      1,                                    // maximum number of frames per packet
    }},
    0,                                      // IANA RTP payload code
    "CELT",                                 // RTP payload name

    celt_create_decoder,                    // create codec function
    celt_destroy_decoder,                   // destroy codec
    celt_codec_decoder,                     // encode/decode
    celt_codec_controls,                    // codec controls
    
    PluginCodec_H323Codec_NoH323,
    NULL
  }
};


PLUGIN_CODEC_IMPLEMENT_ALL(CELT, celtCodecDefn, PLUGIN_CODEC_VERSION_OPTIONS)

/////////////////////////////////////////////////////////////////////////////
