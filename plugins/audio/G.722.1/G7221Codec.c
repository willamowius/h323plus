/****************************************************************************
 *
 * ITU G.722.1 wideband audio codec plugin for OpenH323/OPAL
 *
 * Copyright (C) 2009 Nimajin Software Consulting, All Rights Reserved
 *
 * Permission to copy, use, sell and distribute this file is granted
 * provided this copyright notice appears in all copies.
 * Permission to modify the code herein and to distribute modified code is
 * granted provided this copyright notice appears in all copies, and a
 * notice that the code was modified is included with the copyright notice.
 *
 * This software and information is provided "as is" without express or im-
 * plied warranty, and with no claim as to its suitability for any purpose.
 *
 ****************************************************************************
 *
 * ITU-T 7/14kHz Audio Coder (G.722.1) AKA Polycom 'Siren'
 * SDP usage described in RFC 5577
 * H.245 capabilities defined in G.722.1 Annex A & H.245 Appendix VIII
 * This implementation employs ITU-T G.722.1 (2005-05) fixed point reference 
 * code, release 2.1 (2008-06)
 * This implements only standard bit rates 24000 & 32000 at 16kHz (Siren7)
 * sampling rate, not extended modes or 32000 sampling rate (Siren14).
 * G.722.1 does not implement any silence handling (VAD/CNG)
 * Static variables are not used, so multiple instances can run simultaneously.
 * G.722.1 is patented by Polycom, but is royalty-free if you follow their 
 * license terms. See:
 * http://www.polycom.com/company/about_us/technology/siren14_g7221c/faq.html
 *
 * Initial development by: Ted Szoczei, Nimajin Software Consulting, 09-12-09
 * Portions developed by: Robert Jongbloed, Vox Lucida
 *
 ****************************************************************************/

#define _CRT_NONSTDC_NO_DEPRECATE 1
#define _CRT_SECURE_NO_WARNINGS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PLUGIN_CODEC_DLL_EXPORTS
#include "plugin-config.h"
#endif

#include <codec/opalplugin.h>


static struct PluginCodec_information licenseInfo =
{
  1260388706,           // version timestamp = Wed Dec 09 19:58:26 2009 UTC

  "Ted Szoczei, Nimajin Software Consulting",                  // source code author
  "1.0",                                                       // source code version
  "ted.szoczei@nimajin.com",                                   // source code email
  NULL,                                                        // source code URL
  "Copyright (c) 2009 Nimajin Software Consulting",            // source code copyright
  "None",                                                      // source code license
  PluginCodec_License_None,                                    // source code license
    
  "ITU-T 7/14kHz Audio Coder (G.722.1 Annex C)",               // codec description
  "Polycom, Inc.",                                             // codec author
  "2.1  2008-06-26",				                           // codec version
  NULL,                                                        // codec email
  "http://www.itu.int/rec/T-REC-G.722.1/e",                    // codec URL
  "(c) 2005 Polycom, Inc. All rights reserved.",               // codec copyright information
  "ITU-T General Public License (G.191)",                      // codec license
  PluginCodec_License_NoRoyalties                              // codec license code
};

/////////////////////////////////////////////////////////////////////////////


#include "G722-1/defs.h"

#define FORMAT_NAME_G722_1_24K  "G.722.1-24k"   // text decription and mediaformat name
#define FORMAT_NAME_G722_1_32K  "G.722.1-32k"
#define RTP_NAME_G722_1  "G7221"                // MIME name rfc's 3047, 5577

// 20 ms frame
#define G722_1_FRAME_SAMPLES  320

// required bandwidth options in bits per second
#define G722_1_48K_FRAME_RATE 48000             // for 32k sample rate only
#define G722_1_24K_FRAME_RATE 24000
#define G722_1_32K_FRAME_RATE 32000

// bits and bytes per 20 ms frame, depending on bitrate
// 60 bytes for 24000, 80 for 32000
#define G722_1_FRAME_BITS(rate) ((Word16)((rate) / 50))
#define G722_1_FRAME_BYTES(rate) ((rate) / 400)


/////////////////////////////////////////////////////////////////////////////
// Compress audio for transport


typedef struct
{
  unsigned bitsPerSec;                  // can be changed between frames
  Word16 history [G722_1_FRAME_SAMPLES];
  Word16 mlt_coefs [G722_1_FRAME_SAMPLES];
  Word16 mag_shift;
} G7221EncoderContext;


static void * G7221EncoderCreate (const struct PluginCodec_Definition * codec)
{
  int i;
  G7221EncoderContext * Context = (G7221EncoderContext *) malloc (sizeof(G7221EncoderContext));
  if (Context == NULL)
    return NULL;

  Context->bitsPerSec = codec->bitsPerSec;

  // initialize the mlt history buffer
  for (i = 0; i < G722_1_FRAME_SAMPLES; i++)
    Context->history[i] = 0;

  return Context;
}


static void G7221EncoderDestroy (const struct PluginCodec_Definition * codec, void * context)
{
  free (context);
}


static int G7221Encode (const struct PluginCodec_Definition * codec, 
                                                       void * context,
                                                 const void * fromPtr, 
                                                   unsigned * fromLen,
                                                       void * toPtr,         
                                                   unsigned * toLen,
                                               unsigned int * flag)
{
  G7221EncoderContext * Context = (G7221EncoderContext *) context;
  if (Context == NULL)
    return 0;

  if (*fromLen < G722_1_FRAME_SAMPLES * sizeof(Word16))
    return 0;                           // Source is not a full frame

  if (*toLen < G722_1_FRAME_BYTES (Context->bitsPerSec))
    return 0;                           // Destination buffer not big enough

  // Convert input samples to rmlt coefs
  Context->mag_shift = samples_to_rmlt_coefs ((Word16 *) fromPtr, Context->history, Context->mlt_coefs, G722_1_FRAME_SAMPLES);

  // Encode the mlt coefs
  encoder (G722_1_FRAME_BITS (Context->bitsPerSec), NUMBER_OF_REGIONS, Context->mlt_coefs, Context->mag_shift, (Word16 *) toPtr);

  // return the number of encoded bytes to the caller
  *fromLen = G722_1_FRAME_SAMPLES * sizeof(Word16);
  *toLen = G722_1_FRAME_BYTES (Context->bitsPerSec);
  return 1;
}


/////////////////////////////////////////////////////////////////////////////
// Convert encoded source to audio


typedef struct
{
  unsigned bitsPerSec;                  // can be changed between frames
  Bit_Obj bitobj;
  Rand_Obj randobj;
  Word16 decoder_mlt_coefs [G722_1_FRAME_SAMPLES];
  Word16 mag_shift;
  Word16 old_samples [G722_1_FRAME_SAMPLES / 2];
  Word16 old_decoder_mlt_coefs [G722_1_FRAME_SAMPLES];
  Word16 old_mag_shift;
  Word16 frame_error_flag;
} G7221DecoderContext;


static void * G7221DecoderCreate (const struct PluginCodec_Definition * codec)
{
  int i;
  G7221DecoderContext * Context = (G7221DecoderContext *) malloc (sizeof(G7221DecoderContext));
  if (Context == NULL)
    return NULL;

  Context->bitsPerSec = codec->bitsPerSec;

  Context->old_mag_shift = 0;
  Context->frame_error_flag = 0;

  // initialize the coefs history
  for (i = 0; i < G722_1_FRAME_SAMPLES; i++)
    Context->old_decoder_mlt_coefs[i] = 0;    

  for (i = 0; i < (G722_1_FRAME_SAMPLES >> 1); i++)
    Context->old_samples[i] = 0;
    
  // initialize the random number generator
  Context->randobj.seed0 =
  Context->randobj.seed1 =
  Context->randobj.seed2 =
  Context->randobj.seed3 = 1;

  return Context;
}


static void G7221DecoderDestroy (const struct PluginCodec_Definition * codec, void * context)
{
  free (context);
}


static int G7221Decode (const struct PluginCodec_Definition * codec, 
                                                       void * context,
                                                 const void * fromPtr, 
                                                   unsigned * fromLen,
                                                       void * toPtr,         
                                                   unsigned * toLen,
                                               unsigned int * flag)
{
  int i;
  G7221DecoderContext * Context = (G7221DecoderContext *) context;
  if (Context == NULL)
    return 0;

  if (*fromLen < G722_1_FRAME_BYTES (Context->bitsPerSec))
    return 0;                           // Source is not a full frame

  if (*toLen < G722_1_FRAME_SAMPLES * sizeof(Word16))
    return 0;                           // Destination buffer not big enough

  // reinit the current word to point to the start of the buffer
  Context->bitobj.code_word_ptr = (Word16 *) fromPtr;
  Context->bitobj.current_word = *((Word16 *) fromPtr);
  Context->bitobj.code_bit_count = 0;
  Context->bitobj.number_of_bits_left = G722_1_FRAME_BITS(Context->bitsPerSec);
        
  // process the out_words into decoder_mlt_coefs
  decoder (&Context->bitobj, &Context->randobj, NUMBER_OF_REGIONS, Context->decoder_mlt_coefs, &Context->mag_shift, &Context->old_mag_shift, Context->old_decoder_mlt_coefs, Context->frame_error_flag);
  
  // convert the decoder_mlt_coefs to samples
  rmlt_coefs_to_samples (Context->decoder_mlt_coefs, Context->old_samples, (Word16 *) toPtr, G722_1_FRAME_SAMPLES, Context->mag_shift);

  // For ITU testing, off the 2 lsbs.
  for (i = 0; i < G722_1_FRAME_SAMPLES; i++)
    ((Word16 *) toPtr) [i] &= 0xFFFC;
      
  // return the number of decoded bytes to the caller
  *fromLen = G722_1_FRAME_BYTES (Context->bitsPerSec);
  *toLen = G722_1_FRAME_SAMPLES * sizeof(Word16);
  return 1;
}


/////////////////////////////////////////////////////////////////////////////

// bitrate is a required SDP parameter in RFC 3047/5577
static const char BitRateOptionName[] = "BitRate";

static struct PluginCodec_Option BitRateOption24k =
{
  PluginCodec_IntegerOption,  // PluginCodec_OptionTypes
  BitRateOptionName,          // Generic (human readable) option name
  1,                          // Read Only flag
  PluginCodec_EqualMerge,     // Merge mode
  "24000",                    // Initial value
  "bitrate",                  // SIP/SDP FMTP name
  "0",                        // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0,                          // H.245 Generic Capability number and scope bits
  "24000",                    // Minimum value (enum values separated by ':')
  "32000"                     // Maximum value
};

static struct PluginCodec_Option BitRateOption32k =
{
  PluginCodec_IntegerOption,  // PluginCodec_OptionTypes
  BitRateOptionName,          // Generic (human readable) option name
  1,                          // Read Only flag
  PluginCodec_EqualMerge,     // Merge mode
  "32000",                    // Initial value
  "bitrate",                  // SIP/SDP FMTP name
  "0",                        // SIP/SDP FMTP default value (option not included in FMTP if have this value)
  0,                          // H.245 Generic Capability number and scope bits
  "24000",                    // Minimum value (enum values separated by ':')
  "32000"                     // Maximum value
};

static struct PluginCodec_Option * const OptionTable24k[] =
{
  &BitRateOption24k,
  NULL
};

static struct PluginCodec_Option * const OptionTable32k[] =
{
  &BitRateOption32k,
  NULL
};


static int get_codec_options (const struct PluginCodec_Definition * defn,
                                                             void * context, 
                                                       const char * name,
                                                             void * parm,
                                                         unsigned * parmLen)
{
  if (parm == NULL || parmLen == NULL || *parmLen != sizeof(struct PluginCodec_Option **))
    return 0;

  *(struct PluginCodec_Option const * const * *)parm = (defn->bitsPerSec == G722_1_24K_FRAME_RATE)? OptionTable24k : OptionTable32k;
  *parmLen = 0;
  return 1;
}


// Options are read-only, so set_codec_options not implemented
// get_codec_options returns pointers to statics, and toCustomized and 
// toNormalized are not implemented, so free_codec_options is not necessary

static struct PluginCodec_ControlDefn G7221Controls[] =
{
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS, get_codec_options },
  { NULL }
};


/////////////////////////////////////////////////////////////////////////////


static const struct PluginCodec_H323GenericCodecData G722124kCap =
{
  OpalPluginCodec_Identifer_G7221,// capability identifier (Ref: G.722.1 (05/2005) Table A.2)
  24000                           // Must always be this regardless of "Max Bit Rate" option
};

static const struct PluginCodec_H323GenericCodecData G722132kCap =
{
  OpalPluginCodec_Identifer_G7221,// capability identifier (Ref: G.722.1 (05/2005) Table A.2)
  32000                           // Must always be this regardless of "Max Bit Rate" option
};


/////////////////////////////////////////////////////////////////////////////


static struct PluginCodec_Definition G7221CodecDefn[] =
{
  {
    // G.722.1 24kHz encoder
    PLUGIN_CODEC_VERSION_OPTIONS,           // codec API version
    &licenseInfo,                           // license information

    PluginCodec_MediaTypeAudio |            // audio codec
    PluginCodec_InputTypeRaw |              // raw input data
    PluginCodec_OutputTypeRaw |             // raw output data
    PluginCodec_RTPTypeDynamic |            // dynamic RTP type
    PluginCodec_RTPTypeShared,              // RTP type shared with other codecs in this definition
    
    FORMAT_NAME_G722_1_24K,                 // text decription
    "L16",                                  // source format
    FORMAT_NAME_G722_1_24K,                 // destination format
    
    NULL,                                   // user data

    16000,                                  // samples per second
    G722_1_24K_FRAME_RATE,                  // raw bits per second
    20000,                                  // microseconds per frame
    G722_1_FRAME_SAMPLES,                   // samples per frame
    G722_1_24K_FRAME_RATE / 400,            // bytes per frame
    
    1,                                      // recommended number of frames per packet
    1,                                      // maximum number of frames per packet
    0,                                      // IANA RTP payload code
    RTP_NAME_G722_1,                        // RTP payload name
    
    G7221EncoderCreate,                     // create codec function
    G7221EncoderDestroy,                    // destroy codec
    G7221Encode,                            // encode/decode
    G7221Controls,                          // codec controls

    PluginCodec_H323Codec_generic,          // h323CapabilityType
    &G722124kCap                            // h323CapabilityData
  },
  { 
    // G.722.1 24kHz decoder
    PLUGIN_CODEC_VERSION_OPTIONS,           // codec API version
    &licenseInfo,                           // license information

    PluginCodec_MediaTypeAudio |            // audio codec
    PluginCodec_InputTypeRaw |              // raw input data
    PluginCodec_OutputTypeRaw |             // raw output data
    PluginCodec_RTPTypeDynamic |            // dynamic RTP type
    PluginCodec_RTPTypeShared,              // RTP type shared with other codecs in this definition

    FORMAT_NAME_G722_1_24K,                 // text decription
    FORMAT_NAME_G722_1_24K,                 // source format
    "L16",                                  // destination format

    NULL,                                   // user data

    16000,                                  // samples per second
    G722_1_24K_FRAME_RATE,                  // raw bits per second
    20000,                                  // microseconds per frame
    G722_1_FRAME_SAMPLES,                   // samples per frame
    G722_1_24K_FRAME_RATE / 400,            // bytes per frame
    1,                                      // recommended number of frames per packet
    1,                                      // maximum number of frames per packet
    0,                                      // IANA RTP payload code
    RTP_NAME_G722_1,                        // RTP payload name

    G7221DecoderCreate,                     // create codec function
    G7221DecoderDestroy,                    // destroy codec
    G7221Decode,                            // encode/decode
    G7221Controls,                          // codec controls
    
    PluginCodec_H323Codec_generic,          // h323CapabilityType
    &G722124kCap                            // h323CapabilityData
  },
  {
    // G.722.1 32kHz encoder
    PLUGIN_CODEC_VERSION_OPTIONS,           // codec API version
    &licenseInfo,                           // license information

    PluginCodec_MediaTypeAudio |            // audio codec
    PluginCodec_InputTypeRaw |              // raw input data
    PluginCodec_OutputTypeRaw |             // raw output data
    PluginCodec_RTPTypeDynamic |            // dynamic RTP type
    PluginCodec_RTPTypeShared,              // RTP type shared with other codecs in this definition
    
    FORMAT_NAME_G722_1_32K,                 // text decription
    "L16",                                  // source format
    FORMAT_NAME_G722_1_32K,                 // destination format
    
    NULL,                                   // user data

    16000,                                  // samples per second
    G722_1_32K_FRAME_RATE,                  // raw bits per second
    20000,                                  // microseconds per frame
    G722_1_FRAME_SAMPLES,                   // samples per frame
    G722_1_32K_FRAME_RATE / 400,            // bytes per frame
    
    1,                                      // recommended number of frames per packet
    1,                                      // maximum number of frames per packet
    0,                                      // IANA RTP payload code
    RTP_NAME_G722_1,                        // RTP payload name
    
    G7221EncoderCreate,                     // create codec function
    G7221EncoderDestroy,                    // destroy codec
    G7221Encode,                            // encode/decode
    G7221Controls,                          // codec controls

    PluginCodec_H323Codec_generic,          // h323CapabilityType
    &G722132kCap                            // h323CapabilityData
  },
  { 
    // G.722.1 32kHz decoder
    PLUGIN_CODEC_VERSION_OPTIONS,           // codec API version
    &licenseInfo,                           // license information

    PluginCodec_MediaTypeAudio |            // audio codec
    PluginCodec_InputTypeRaw |              // raw input data
    PluginCodec_OutputTypeRaw |             // raw output data
    PluginCodec_RTPTypeDynamic |            // dynamic RTP type
    PluginCodec_RTPTypeShared,              // RTP type shared with other codecs in this definition

    FORMAT_NAME_G722_1_32K,                 // text decription
    FORMAT_NAME_G722_1_32K,                 // source format
    "L16",                                  // destination format

    NULL,                                   // user data

    16000,                                  // samples per second
    G722_1_32K_FRAME_RATE,                  // raw bits per second
    20000,                                  // microseconds per frame
    G722_1_FRAME_SAMPLES,                   // samples per frame
    G722_1_32K_FRAME_RATE / 400,            // bytes per frame
    1,                                      // recommended number of frames per packet
    1,                                      // maximum number of frames per packet
    0,                                      // IANA RTP payload code
    RTP_NAME_G722_1,                        // RTP payload name

    G7221DecoderCreate,                     // create codec function
    G7221DecoderDestroy,                    // destroy codec
    G7221Decode,                            // encode/decode
    G7221Controls,                          // codec controls
    
    PluginCodec_H323Codec_generic,          // h323CapabilityType
    &G722132kCap                            // h323CapabilityData
  }
};


PLUGIN_CODEC_IMPLEMENT_ALL(G7221, G7221CodecDefn, PLUGIN_CODEC_VERSION_OPTIONS)


/////////////////////////////////////////////////////////////////////////////
