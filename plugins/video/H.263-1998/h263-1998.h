/*
 * H.263 Plugin codec for OpenH323/OPAL
 *
 * This code is based on the following files from the OPAL project which
 * have been removed from the current build and distributions but are still
 * available in the CVS "attic"
 * 
 *    src/codecs/h263codec.cxx 
 *    include/codecs/h263codec.h 

 * The original files, and this version of the original code, are released under the same 
 * MPL 1.0 license. Substantial portions of the original code were contributed
 * by Salyens and March Networks and their right to be identified as copyright holders
 * of the original code portions and any parts now included in this new copy is asserted through 
 * their inclusion in the copyright notices below.
 *
 * Copyright (C) 2007 Matthias Schneider
 * Copyright (C) 2006 Post Increment
 * Copyright (C) 2005 Salyens
 * Copyright (C) 2001 March Networks Corporation
 * Copyright (C) 1999-2000 Equivalence Pty. Ltd.
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
 *                 Guilhem Tardy (gtardy@salyens.com)
 *                 Craig Southeren (craigs@postincrement.com)
 *
 */

/*
  Notes
  -----

 */

#ifndef __H263P_1998_H__
#define __H263P_1998_H__ 1

#include "../common/ffmpeg_compat.h"
#include "h263pframe.h"
#include "rfc2190.h"
#include "critsect.h"

#if TRACE_FILE
#include "tracer.h"
#endif

typedef unsigned char BYTE;

#define H263P_CLOCKRATE        90000
#define H263P_BITRATE         327600
#define H263P_PAYLOAD_SIZE       600
#define H263P_FRAME_RATE          25
#define H263P_KEY_FRAME_INTERVAL 125
#define H263P_MIN_QUANT            2

/* Smallest usable spread between qmin and qmax.  Below this the rate
   controller cannot track the negotiated bit rate at all. */
#define H263_MIN_QUANT_RANGE      10

/* Default temporal/spatial trade off, 0..31.  Upstream defaulted to 0, which
   pins the quantiser and makes the encoder ignore the negotiated bit rate. */
/* STRINGIZE() comes from codec/opalplugin.h, which every user of this header
   includes first; do not define it again here. */
#define H263_DEFAULT_TSTO         31

/* H263_MIN_BITS_PER_PIXEL and the resolution-downgrade logic it supported
   were removed from GetInputFormat() (h263-1998.cxx) - a real deployment
   showed that silently sending a smaller picture than the negotiated
   capability's own size breaks at least one real decoder outright (see the
   comment at GetInputFormat() for the full story). Left this constant's
   history here rather than deleting it outright: 704x576 at 30 fps needs
   roughly 0.08 bit/pixel to fit an ~984 kbit/s target, which is what
   originally motivated it; 352x288 at the same rate and bit rate sits at
   roughly 0.32. If a bit-rate-aware size decision is ever wanted again, it
   belongs in capability/channel negotiation - deciding which of
   H.263-QCIF/-CIF/-720 to offer or accept for a channel in the first place
   - not in this encoder silently substituting a different size than the one
   already negotiated for it. */

#define H263_CLOCKRATE         90000
#define H263_QCIF_BITRATE         192000
#define H263_CIF_BITRATE          327600
#define H263_BITRATE              327600
#define H263_CIF4_BITRATE         984000
#define H263_PAYLOAD_SIZE         1400
#define RTP_RFC2190_PAYLOAD       34
#define H263_KEY_FRAME_INTERVAL  125
#define H263_FRAME_RATE          30

#define CIF_WIDTH       352
#define CIF_HEIGHT      288

#define CIF4_WIDTH      (CIF_WIDTH*2)
#define CIF4_HEIGHT     (CIF_HEIGHT*2)

#define CIF16_WIDTH     (CIF_WIDTH*4)
#define CIF16_HEIGHT    (CIF_HEIGHT*4)

#define QCIF_WIDTH     (CIF_WIDTH/2)
#define QCIF_HEIGHT    (CIF_HEIGHT/2)

#define QCIF4_WIDTH     (CIF4_WIDTH/2)
#define QCIF4_HEIGHT    (CIF4_HEIGHT/2)

#define SQCIF_WIDTH     128
#define SQCIF_HEIGHT    96

#define MAX_YUV420P_FRAME_SIZE (((CIF16_WIDTH * CIF16_HEIGHT * 3) / 2) + (AV_INPUT_BUFFER_PADDING_SIZE*2))
enum Annex {
    D,
    F,
    I,
    K,
    J,
    S,
    T,
    N,
    P
};

// Input formats from the input device.
struct inputFormats 
{ 
  unsigned w;
  unsigned h;
  unsigned r;
};

class H263_Base_EncoderContext
{
  public:
    H263_Base_EncoderContext(const char * prefix);
    virtual ~H263_Base_EncoderContext();

    virtual bool Open() = 0;
    virtual bool Open(AVCodecID codecId);

    virtual int EncodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags) = 0;
    void SetMaxKeyFramePeriod (unsigned period);
    void SetTargetBitrate (unsigned rate);
    void SetFrameWidth (unsigned width);
    void SetFrameHeight (unsigned height);
    void SetTSTO (unsigned tsto);
    void EnableAnnex (Annex annex);
    void DisableAnnex (Annex annex);

    /* Sets the frame interval libavcodec's rate controller budgets against.
       This used to be hardcoded to ~29.97 fps regardless of how often the
       application actually called EncodeFrames(), which is fine when that
       happens to match, and silently wrong otherwise: at half the assumed
       rate (e.g. a 15 fps call), the controller believes only half as much
       time has passed as really has between frames, so it budgets roughly
       half the bits per frame that the negotiated bit rate actually allows -
       and keeps re-encoding under VBV pressure even though the true output,
       measured against wall clock time, is well within the negotiated rate. */
    void SetTargetFrameTime (unsigned frameTime);
    bool OpenCodec();
    void CloseCodec();

    void Lock();
    void Unlock();

    void AddInputFormat(inputFormats & fmt);
    int GetInputFormat(inputFormats & fmt , unsigned maxWidth, unsigned maxHeight);

    virtual void SetMaxRTPFrameSize (unsigned size) = 0;

    int m_targetBitRate;

    /* Scratch space for encoder_formats() to rewrite FRAME_TIME/WIDTH/HEIGHT
       in place after GetInputFormat() adjusts the resolution.  These used to
       be strdup()'d via num2str() and never freed - a small, genuine leak
       on every renegotiation.  Owned by the context and reused each call, so
       there is nothing for a caller to free and nothing left behind when the
       context is destroyed.  Public because encoder_formats() is a plain
       function taking the context by pointer, not a member. */
    char m_frameTimeStr[16];
    char m_frameWidthStr[16];
    char m_frameHeightStr[16];

  protected:
    virtual bool InitContext() = 0;

    /* libavcodec no longer supports opening and closing the same
       AVCodecContext more than once, so every setter just records the value
       and the context is built from scratch in OpenCodec().  Subclasses add
       their codec specific AVOptions in ApplyCodecOptions(), which runs
       after the context is allocated but before avcodec_open2(). */
    virtual bool ApplyCodecOptions() { return true; }

    /* Feed one YUV420P frame to the encoder and collect the result in
       m_packet.  Returns 1 if a packet was produced, 0 if the encoder wants
       more input, -1 on error.  The caller must av_packet_unref(m_packet). */
    int EncodeOneFrame(unsigned int flags);

    /* (Re)allocate the padded, aligned YUV420P staging buffer handed to
       libavcodec. */
    bool AllocateInputFrameBuffer(unsigned width, unsigned height);

    /* Check that the RTP payload really holds the picture its header claims
       before copying anything out of it. */
    bool ValidateSourceFrame(const RTPFrame & srcRTP,
                             const PluginCodec_Video_FrameHeader * header);

    unsigned char  * _inputFrameBuffer;
    size_t           _inputFrameBufferSize;
    FFMPEG_AVCodec * _codec;
    AVCodecContext * _context;
    AVFrame        * _inputFrame;
    AVPacket       * m_packet;

    AVCodecID   _codecId;
    int         _frameCount;
    int         _width, _height;

    // deferred encoder settings, applied in OpenCodec()
    unsigned    _keyFramePeriod;
    unsigned    _tsto;
    unsigned    _maxRTPFrameSize;
    unsigned    _annexFlags;        // bit (1 << Annex) set when enabled

    /* Target time between frames, in 1/H263_CLOCKRATE (90000) second RTP
       clock units - the same units PLUGINCODEC_OPTION_FRAME_TIME arrives in.
       Defaults to 3003 (~29.97 fps), which is what OpenCodec() always used
       before this was negotiable, so a caller that never sends this option
       sees no change in behaviour. */
    unsigned    _frameTime;

    CriticalSection _mutex;
    const char * prefix;
#if TRACE_FILE
    Tracer tracer;
#endif
    std::list<inputFormats> videoInputFormats;
};

////////////////////////////////////////////////////////////////////////////

class H263_RFC2190_EncoderContext : public H263_Base_EncoderContext
{
  public:
    H263_RFC2190_EncoderContext();
    ~H263_RFC2190_EncoderContext();
    bool Open();
    bool InitContext();
    void SetMaxRTPFrameSize (unsigned size);
    int EncodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags);
  protected:
    bool ApplyCodecOptions();
    RFC2190Packetizer packetizer;
};

////////////////////////////////////////////////////////////////////////////

class H263_RFC2429_EncoderContext : public H263_Base_EncoderContext
{
  public:
    H263_RFC2429_EncoderContext();
    ~H263_RFC2429_EncoderContext();

    bool Open();
    bool InitContext();
    void SetMaxRTPFrameSize (unsigned size);
    int EncodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags);
  protected:
    bool ApplyCodecOptions();
    H263PFrame * _txH263PFrame;
};

////////////////////////////////////////////////////////////////////////////

class H263_Base_DecoderContext
{
  public:
    H263_Base_DecoderContext(const char * prefix);
    virtual ~H263_Base_DecoderContext();

    virtual bool DecodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags) = 0;

  protected:
    bool OpenCodec();
    void CloseCodec();

    /* Returns 1 when _outputFrame holds a decoded picture, 0 when the
       decoder needs more data, -1 on error. */
    int DecodeOneFrame(const BYTE * data, size_t length);

    FFMPEG_AVCodec *_codec;
    AVCodecContext *_context;
    AVFrame        *_outputFrame;
    AVPacket       *m_packet;

    int _frameCount;
    CriticalSection _mutex;
    const char * prefix;
#if TRACE_FILE
    Tracer tracer;
#endif
};

////////////////////////////////////////////////////////////////////////////

class H263_RFC2190_DecoderContext : public H263_Base_DecoderContext
{
  public:
    H263_RFC2190_DecoderContext();
    virtual ~H263_RFC2190_DecoderContext();
    bool DecodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags);

  protected:
    RFC2190Depacketizer depacketizer;
};

////////////////////////////////////////////////////////////////////////////

class H263_RFC2429_DecoderContext : public H263_Base_DecoderContext
{
  public:
     H263_RFC2429_DecoderContext();
     virtual ~H263_RFC2429_DecoderContext();

     bool DecodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags);
  protected:
    unsigned int _skippedFrameCounter;
    bool _gotIFrame;
    bool _gotAGoodFrame;
    H263PFrame* _rxH263PFrame;
};

////////////////////////////////////////////////////////////////////////////

static int valid_for_protocol    ( const struct PluginCodec_Definition *, void *, const char *,
                                   void * parm, unsigned * parmLen);
static int get_codec_options     ( const struct PluginCodec_Definition * codec, void *, const char *, 
                                   void * parm, unsigned * parmLen);
static int free_codec_options    ( const struct PluginCodec_Definition *, void *, const char *, 
                                   void * parm, unsigned * parmLen);

static void * create_encoder     ( const struct PluginCodec_Definition *);
static void destroy_encoder      ( const struct PluginCodec_Definition *, void * _context);
static int codec_encoder         ( const struct PluginCodec_Definition *, void * _context,
                                   const void * from, unsigned * fromLen,
                                   void * to, unsigned * toLen,
                                   unsigned int * flag);
static int to_normalised_options ( const struct PluginCodec_Definition *, void *, const char *,
                                   void * parm, unsigned * parmLen);
static int to_customised_options ( const struct PluginCodec_Definition *, void *, const char *, 
                                   void * parm, unsigned * parmLen);
static int encoder_set_options   ( const struct PluginCodec_Definition *, void * _context, const char *, 
                                   void * parm, unsigned * parmLen);

static int encoder_formats       ( const struct PluginCodec_Definition *, void *, const char *, 
                                   void * parm, unsigned * parmLen);

static int encoder_get_output_data_size ( const PluginCodec_Definition *, void *, const char *,
                                   void *, unsigned *);

static void * create_decoder     ( const struct PluginCodec_Definition *);
static void destroy_decoder      ( const struct PluginCodec_Definition *, void * _context);
static int codec_decoder         ( const struct PluginCodec_Definition *, void * _context, 
                                   const void * from, unsigned * fromLen,
                                   void * to, unsigned * toLen,
                                   unsigned int * flag);
static int decoder_get_output_data_size ( const PluginCodec_Definition * codec, void *, const char *,
                                   void *, unsigned *);
/////////////////////////////////////////////////////////////////////////////

#endif /* __H263P_1998_H__ */
