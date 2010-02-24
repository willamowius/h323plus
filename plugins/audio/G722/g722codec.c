/*
 * G.722 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) 2008 by Hermon Labs, All Rights Reserved
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
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Eugene Mednikov
 *
 * Contributor(s): ______________________________________.
 *
 * $Revision$
 * $Author$
 * $Date$
 */

#ifndef PLUGIN_CODEC_DLL_EXPORTS
#include "plugin-config.h"
#endif

#include <codec/opalplugin.h>

#include "VoIPCodecs/inttypes.h"
#include "VoIPCodecs/g722.h"


#define INCLUDE_SDP_16000_VERSION 0

/* Due to a spec error, clock rate is 8kHz even though this is 16kHz codec,
   see RFC3551/4.5.2. So some of these values have to be lied about so that
   the OPAL system gets it right.
  */
#define CLOCK_RATE              8000  // Clock rate is not samples/second in this case!
#define BITS_PER_SECOND         64000 // raw bits per second
#define FRAME_TIME              1000  // Microseconds in a millisecond
#define SAMPLES_PER_FRAME       16    // Samples in a millisecond
#define	BYTES_PER_FRAME         8     // Bytes in a millisecond
#define MAX_FRAMES_PER_PACKET   90    // 90 milliseconds, which means RTP packets smaller than 1500 bytes typical LAN maximum
#define PREF_FRAMES_PER_PACKET  20    // 20 milliseconds

static const char L16Desc[]  = "PCM-16-16kHz"; // Cannot use "L16" as usual, force 16kHz PCM
static const char g722[]     = "G.722-64k";
static const char sdpG722[]  = "G722";

#if INCLUDE_SDP_16000_VERSION
static const char g722_16[]  = "G.722-16kHz";
#endif

#define PAYLOAD_CODE 9


/////////////////////////////////////////////////////////////////////////////

static void * create_encoder(const struct PluginCodec_Definition * codec)
{
  return g722_encode_init(NULL, BITS_PER_SECOND, 0);
}

static void destroy_encoder(const struct PluginCodec_Definition * codec, void * context)
{
  g722_encode_release(context);
}

static int encode(const struct PluginCodec_Definition * codec,
                                           void * context,
                                     const void * from, 
                                       unsigned * fromLen, 
                                           void * to, 
                                       unsigned * toLen, 
                                   unsigned int * flag) 
{ 
  g722_encode_state_t * state = context;

  if (*toLen < *fromLen / 4)
    return 0; // Destination buffer not big enough

  *toLen = g722_encode(state, to, from, *fromLen / 2);
  return 1; 
} 


/////////////////////////////////////////////////////////////////////////////

static void * create_decoder(const struct PluginCodec_Definition * codec)
{
  return g722_decode_init(NULL, BITS_PER_SECOND, 0);
}

static void destroy_decoder(const struct PluginCodec_Definition * codec, void * context)
{
  g722_decode_release(context);
}


static int decode(const struct PluginCodec_Definition * codec, 
                                           void * _context, 
                                     const void * from, 
                                       unsigned * fromLen, 
                                           void * to, 
                                       unsigned * toLen, 
                                   unsigned int * flag) 
{
  g722_decode_state_t * state = _context;

  if (*toLen < *fromLen * 4)
    return 0; // Destination buffer not big enough

  *toLen = g722_decode(state, to, from, *fromLen) * 2; 
  return 1;
} 


/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information licenseInfo = {
  1084181196,                                   // timestamp = Mon 10 May 2004 09:26:36 AM UTC

  "Eugene Mednikov",                            // source code author
  "1.0",                                        // source code version
  "em@hermonlabs.com",                          // source code email
  "http://www.hermonlabs.com",                  // source code URL
  "Copyright (C) 2008 by Hermon Labs, All Rights Reserved", // source code copyright
  "MPL 1.0",                                    // source code license
  PluginCodec_License_MPL,                      // source code license
  
  "ITU G.722",                                  // codec description
  "Steve Underwood",                            // codec author
  NULL,                                         // codec version
  "steveu@coppice.org",                         // codec email
  NULL,                                         // codec URL
  NULL,                                         // codec copyright information
  NULL,                                         // codec license
  PluginCodec_License_LGPL                      // codec license code
};

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Definition g722CodecDefn[] =
{
#if INCLUDE_SDP_16000_VERSION
  // Include a version for SIP/SDP that indicates the more logical, though
  // incorrect by RFC3551/4.5.2, 16000Hz version of G.722. We use dynamic
  // payload types to avoid conflict with the compliant 8000Hz version.
  { 
    // encoder 
    PLUGIN_CODEC_VERSION_WIDEBAND,        // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudio |          // audio codec
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeDynamic,           // specified RTP type

    g722_16,                              // text decription
    L16Desc,                              // source format
    g722_16,                              // destination format

    0,                                    // user data

    16000,                                // samples per second
    BITS_PER_SECOND,                      // raw bits per second
    FRAME_TIME,                           // microseconds per frame
    SAMPLES_PER_FRAME,                    // samples per frame
    BYTES_PER_FRAME,                      // bytes per frame
    PREF_FRAMES_PER_PACKET,               // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,                // maximum number of frames per packe
    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG722,                              // RTP payload name

    create_encoder,                       // create codec function
    destroy_encoder,                      // destroy codec
    encode,                               // encode/decode
    NULL,                                 // codec controls

    0,                                    // h323CapabilityType 
    NULL                                  // h323CapabilityData
  },

  { 
    // decoder 
    PLUGIN_CODEC_VERSION_WIDEBAND,        // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudio |          // audio codec
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeDynamic,           // specified RTP type

    g722_16,                              // text decription
    g722_16,                              // source format
    L16Desc,                              // destination format

    0,                                    // user data

    16000,                                // samples per second
    BITS_PER_SECOND,                      // raw bits per second
    FRAME_TIME,                           // microseconds per frame
    SAMPLES_PER_FRAME,                    // samples per frame
    BYTES_PER_FRAME,                      // bytes per frame
    PREF_FRAMES_PER_PACKET,               // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,                // maximum number of frames per packe
    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG722,                              // RTP payload name

    create_decoder,                       // create codec function
    destroy_decoder,                      // destroy codec
    decode,                               // encode/decode
    NULL,                                 // codec controls

    0,                                    // h323CapabilityType 
    NULL                                  // h323CapabilityData
  },
#endif

  // Standards compliant version
  { 
    // encoder 
    PLUGIN_CODEC_VERSION_WIDEBAND,        // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudio |          // audio codec
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeExplicit,          // specified RTP type

    g722,                                 // text decription
    L16Desc,                              // source format
    g722,                                 // destination format

    0,                                    // user data

    CLOCK_RATE,                           // samples per second
    BITS_PER_SECOND,                      // raw bits per second
    FRAME_TIME,                           // microseconds per frame
    SAMPLES_PER_FRAME,                    // samples per frame
    BYTES_PER_FRAME,                      // bytes per frame
    PREF_FRAMES_PER_PACKET,               // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,                // maximum number of frames per packe
    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG722,                              // RTP payload name

    create_encoder,                       // create codec function
    destroy_encoder,                      // destroy codec
    encode,                               // encode/decode
    NULL,                                 // codec controls

    PluginCodec_H323AudioCodec_g722_64k,  // h323CapabilityType 
    NULL                                  // h323CapabilityData
  },

  { 
    // decoder 
    PLUGIN_CODEC_VERSION_WIDEBAND,        // codec API version
    &licenseInfo,                         // license information

    PluginCodec_MediaTypeAudio |          // audio codec
    PluginCodec_InputTypeRaw |            // raw input data
    PluginCodec_OutputTypeRaw |           // raw output data
    PluginCodec_RTPTypeExplicit,          // specified RTP type

    g722,                                 // text decription
    g722,                                 // source format
    L16Desc,                              // destination format

    0,                                    // user data

    CLOCK_RATE,                           // samples per second
    BITS_PER_SECOND,                      // raw bits per second
    FRAME_TIME,                           // microseconds per frame
    SAMPLES_PER_FRAME,                    // samples per frame
    BYTES_PER_FRAME,                      // bytes per frame
    PREF_FRAMES_PER_PACKET,               // recommended number of frames per packet
    MAX_FRAMES_PER_PACKET,                // maximum number of frames per packe
    PAYLOAD_CODE,                         // IANA RTP payload code
    sdpG722,                              // RTP payload name

    create_decoder,                       // create codec function
    destroy_decoder,                      // destroy codec
    decode,                               // encode/decode
    NULL,                                 // codec controls

    PluginCodec_H323AudioCodec_g722_64k,  // h323CapabilityType 
    NULL                                  // h323CapabilityData
  }
};


PLUGIN_CODEC_IMPLEMENT_ALL(G722, g722CodecDefn, PLUGIN_CODEC_VERSION_WIDEBAND)

/////////////////////////////////////////////////////////////////////////////
