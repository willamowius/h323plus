/* SBC Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) 2008 Christian Hoene, All Rights Reserved
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
 * The Initial Developer of the Original Code is Christian Hoene
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
//#include <ptlib.h>
#include <assert.h>

#ifndef PLUGIN_CODEC_DLL_EXPORTS
#include "plugin-config.h"
#endif

//#include <codec/opalplugin.h>

#if defined(_WIN32) || defined(_WIN32_WCE)
  #define STRCMPI  _strcmpi
#else
  #define STRCMPI  strcasecmp
#endif

#include <samplerate.h>
#include "../../../include/codec/g711a1_plc.h"
#include "../../../include/codec/opalplugin.h"

#define MAXRATE 48000
#define CHANNELS 2

extern "C" {
#include "sbc.h" 
}

struct PluginSbcContext {
  sbc_t sbcState;
  SRC_STATE *srcState;
  int lastPayloadSize;
  OpalG711_PLC *plcState[CHANNELS];

  float *data1,*data2;  // do buffers to store samples
  int data1_len, data2_len; // size of the buffers measured in float variables

  short *between;                // buffer between resampling and SBC
  int between_len,between_count; // size of allocated buffer and number of sample stored in buffer

  double ratio;
};



/////////////////////////////////////////////////////////////////////////////

/**
 * allocate and create encoder object
 */
static void * create_encoder(const struct PluginCodec_Definition * codec)
{
  //PTRACE(3, "Codec\tSBC decoder created");

  struct PluginSbcContext * context = new PluginSbcContext;

  context->lastPayloadSize = 0;

  /* set up samplerate convertion */
  int error;
  if((context->srcState = src_new (SRC_SINC_FASTEST, CHANNELS, &error))==NULL) {
    //PTRACE(3, "Codec\tSBC resampling failed\n" << src_strerror(error));
    return NULL;
  }
  context->data1 = context->data2 = NULL;
  context->data1_len = context->data2_len = 0;
  context->between = NULL;
  context->between_len = context->between_count = 0;

  /* init SBC */
  if((error=sbc_init(&context->sbcState, 0))!=0) {
    //PTRACE(3, "Codec\tSBC sbc_init failed " << error);
    return NULL;
  }
#if MAXRATE == 48000
  context->sbcState.frequency = SBC_FREQ_48000; 
#elif MAXRATE == 44100
  context->sbcState.frequency = SBC_FREQ_44100; 
#elif MAXRATE == 32000
  context->sbcState.frequency = SBC_FREQ_32000; 
#elif MAXRATE == 16000
  context->sbcState.frequency = SBC_FREQ_16000; 
#else
  #error
#endif
  context->sbcState.mode = SBC_MODE_JOINT_STEREO;
  context->sbcState.subbands = SBC_SB_8;
  context->sbcState.blocks = SBC_BLK_16;
  context->sbcState.bitpool = 32;

  return context;
}

/**
 * changes the sample rate 
 */
static int changerate(struct PluginSbcContext *context, 
		      const short *in, int *in_len, int in_rate,
		      short *out, int *out_len, int out_rate, bool mono)
{
  // enough output space?
  if(*in_len * in_rate > *out_len * out_rate)
    return -1;
    
  // alloc more memory if needed
  if(*in_len > context->data1_len) {
    context->data1_len = *in_len;
    if((context->data1 = (float*)realloc(context->data1, *in_len*sizeof(float)))==NULL) {
      //PTRACE(3, "Codec\tCONTEXT realloc failed\n");
      return -1;
    }
  }
  if(*out_len > context->data2_len) {
    context->data2_len = *out_len;
    if((context->data2 = (float*)realloc(context->data2, *out_len*sizeof(float)))==NULL) {
      //PTRACE(3, "Codec\tCONTEXT realloc failed\n");
      return -1;
    }
  }

  // copy short to float values
  short *p1=(short*)in;
  float *p2=context->data1;
  for(int i=0;i<*in_len;i++) 
    *p2++ = (float)*p1++;

  // convert the rate
  SRC_DATA src_data = {
    context->data1, context->data2,
    *in_len/CHANNELS, *out_len/CHANNELS,
    0, 0,
    0,
    in_rate/double(out_rate)
  };
  src_process(context->srcState, &src_data);
  *out_len = src_data.output_frames_gen * CHANNELS;
  *in_len = src_data.input_frames_used * CHANNELS;

  // copy float to short buffer
  p1=out;
  p2=context->data2;  
  if(mono) {
    for(int i=0;i<*out_len;i+=2) {
      int d=int(*p2++ + *p2++)/2;
      if(d>32767)
	d=32767;
      if(d<-32768)
	d=-32768;
      *p1++ = short(d);
    }
    *out_len/=2;
  }
  else {
    for(int i=0;i<*out_len;i++) {
      int d=int(*p2++);
      if(d>32767)
	d=32767;
      if(d<-32768)
	d=-32768;
      *p1++ = short(d);
    }
  }
  return *out_len;
}

static int codec_encoder(const struct PluginCodec_Definition * codec, 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flags)
{
  PluginSbcContext * context = (PluginSbcContext *)_context;

  short * sampleBuffer = (short *)from;

  // encode PCM data in sampleBuffer to buffer
  int lenFrom, lenTo;
  int countFrom=0, countTo=0;
  for(;;) {
    lenFrom=sbc_encode(&context->sbcState, (const char*)from+countFrom, *fromLen-countFrom, (char*)to+countTo, *toLen-countTo, &lenTo);
    if(lenFrom<0) {
      //PTRACE(3, "SBC Encode: failed with error code " << lenFrom);
      return 0;
    }
    if(lenFrom==0)
      break;
    countFrom += lenFrom;
    countTo += lenTo;
  }
  *fromLen = countFrom;
  *toLen = countTo;
  return 1; 
}
    
/**
 * Destroy and delete encoder object
 */

static void destroy_encoder(const struct PluginCodec_Definition * codec, void * _context)
{
  PluginSbcContext * context = (PluginSbcContext *)_context;
  src_delete(context->srcState);
  sbc_finish(&context->sbcState); 
  free(context);
}

/**
 * allocate and create SBC decoder object
 */
static void * create_decoder(const struct PluginCodec_Definition * codec)
{
  PluginSbcContext * context = new PluginSbcContext;

  //PTRACE(3, "Codec\tSBC decoder created");

  int error;
  if((context->srcState = src_new (SRC_SINC_FASTEST, CHANNELS, &error))==NULL) {
    //PTRACE(3, "Codec\tSBC resampling failed\n" << src_strerror(error));
    return NULL;
  }
  if((error=sbc_init(&context->sbcState, 0))!=0) {
    //PTRACE(3, "Codec\tSBC sbc_init left failed " << error);
    return NULL;
  }

  for(int i=0;i<CHANNELS;i++) {
    if((context->plcState[i] = new OpalG711_PLC(MAXRATE))!=NULL) {
      //PTRACE(3, "Codec\tSBC creatng PLC left failed " << error);
      return NULL;
    }
  }
  return context;
}

static int codec_decoder(const struct PluginCodec_Definition * codec, 
                                           void * _context,
                                     const void * from, 
                                       unsigned * fromLen,
                                           void * to,         
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  struct PluginSbcContext * context = (struct PluginSbcContext *)_context;

  // decode frame
  int lenFrom, lenTo;
  int countFrom=0, countTo=0;
  for(;;) {
    lenFrom=sbc_decode(&context->sbcState, (const char*)from+countFrom, *fromLen-countFrom, (char*)to+countTo, *toLen-countTo, &lenTo);
    if(lenFrom<0) {
      //PTRACE(3, "SBC Encode: failed with error code " << lenFrom);
      return 0;
    }
    if(lenFrom==0)
      break;
    countFrom += lenFrom;
    countTo += lenTo;
  }
  *fromLen = countFrom;
  *toLen = countTo;
  return 1; 
}

static void destroy_decoder(const struct PluginCodec_Definition * codec, void * _context)
{
  struct PluginSbcContext * context = (struct PluginSbcContext *)_context;
  delete context->plcState;
  src_delete(context->srcState);
  sbc_finish(&context->sbcState); 
  free(context);
}

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information licenseInfo = {
 1205422679,                              // timestamp = Thu Mar 13 16:38 CET 2008
  "Christian Hoene, University of Tübingen",                   // source code author
  "0.1",                                                       // source code version
  "hoene@ieee.org",                                            // source code email
  "http://www.uni-tuebingen.de",                               // source code URL
  "Copyright (C) 2008 by Christian Hoene, All Rights Reserved",// source code copyright
   "MPL 1.0",                                                   // source code license
  PluginCodec_License_MPL,                                     // source code license

  "Subband Coding (SBC)",                                      // codec description
  "Bluetooth SIG",                                             // codec author
  "1.2",                                                       // codec version
  NULL,                                                        // codec email
  "http://www.bluetooth.com/",                                 // codec URL
  NULL,                                                        // codec copyright information
  NULL,                                                        // codec license
  PluginCodec_License_NoRoyalties                              // codec license code
};

#define	EQUIVALENCE_COUNTRY_CODE            9
#define	EQUIVALENCE_EXTENSION_CODE          0
#define	EQUIVALENCE_MANUFACTURER_CODE       61

static const unsigned char sbcCapStr[] = { "BluetoothSBC" };

static struct PluginCodec_H323NonStandardCodecData sbcCap =
{
  NULL, 
  EQUIVALENCE_COUNTRY_CODE, 
  EQUIVALENCE_EXTENSION_CODE, 
  EQUIVALENCE_MANUFACTURER_CODE,
  sbcCapStr, sizeof(sbcCapStr)-1,
  NULL
};

static const char L16Desc[]  = { "L16" };

static const char sbc[]    = { "SBC" };

static const char sdpSBC[] = { "sbc" };

static struct PluginCodec_Definition sbcCodecDefn[2] =
{
  { 
    // encoder
    PLUGIN_CODEC_VERSION,               // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    sbc,                                // text decription
    L16Desc,                            // source format
    sbc,                                // destination format

    (void *)NULL,                       // user data

    48000,                              // samples per second 
    128000,                             // raw bits per second - TODO: is dynamic
    22500,                              // nanoseconds per frame - TODO: is dynamic
    64,                                 // samples per frame - TODO: is dynamic
    64,                                 // bytes per frame - TODO: is dynamic
    1,                                  // recommended number of frames per packet
    16,                                 // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpSBC,                             // RTP payload name

    create_encoder,                     // create codec function
    destroy_encoder,                    // destroy codec
    codec_encoder,                      // encode/decode
    NULL,                               // codec controls

    PluginCodec_H323Codec_nonStandard,  // h323CapabilityType 
    &sbcCap                           // h323CapabilityData
  },

  { 
    // decoder
    PLUGIN_CODEC_VERSION,               // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeAudio |        // audio codec
    PluginCodec_InputTypeRaw |          // raw input data
    PluginCodec_OutputTypeRaw |         // raw output data
    PluginCodec_RTPTypeDynamic,         // dynamic RTP type

    sbc,                                // text decription
    sbc,                                // source format
    L16Desc,                            // destination format

    (const void *)NULL,                 // user data

    48000,                              // samples per second
    128000,                             // raw bits per second - TODO!
    10000,                              // nanoseconds per frame - TODO!
    128,                                // samples per frame - TODO!
    128,                                // bytes per frame - TODO!
    1,                                  // recommended number of frames per packet
    16,                                 // maximum number of frames per packe
    0,                                  // IANA RTP payload code
    sdpSBC,                             // RTP payload name

    create_decoder,                     // create codec function
    destroy_decoder,                    // destroy codec
    codec_decoder,                      // encode/decode
    NULL,                               // codec controls

    PluginCodec_H323Codec_nonStandard,  // h323CapabilityType 
    &sbcCap                             // h323CapabilityData
  }
};


PLUGIN_CODEC_IMPLEMENT_ALL(LPC 10, sbcCodecDefn, PLUGIN_CODEC_VERSION)

/////////////////////////////////////////////////////////////////////////////
