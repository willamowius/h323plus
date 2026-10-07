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

#ifndef PLUGIN_CODEC_DLL_EXPORTS
#include "plugin-config.h"
#endif

#define _CRT_SECURE_NO_DEPRECATE

#ifdef _STATIC_LINK
  #define OPAL_STATIC_CODEC 1
  #if _WIN32 || _WIN64
    #pragma comment(lib,"avcodec.lib")
    #pragma comment(lib,"avutil.lib")
  #endif
#endif

#include <codec/opalplugin.h>

#include "h263-1998.h"
#include <limits>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../common/mpi.h"
#include "../common/trace.h"

#include "tracer.h"

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)

  #define STRCMPI  _strcmpi
  #define STRNCMPI strncmp
#else
  #define STRCMPI  strcasecmp
  #define STRNCMPI strncasecmp
#endif

DECLARE_TRACER

static const char * h263_Prefix = "H.263";
static const char * h263P_Prefix = "H.263+";

static const char YUV420PDesc[]  = { "YUV420P" };
static const char h263PDesc[]    = { "H.263P" };
static const char sdpH263P[]     = { "h263-1998" };

static const char h263Desc[]     = { "H.263" };
static const char h263QCIFDesc[]  = { "H.263-QCIF" };
static const char h263CIFDesc[]   = { "H.263-CIF" };
static const char h263720Desc[]   = { "H.263-720" };
static const char sdpH263[]      = { "h263" };

static struct StdSizes {
  enum {
    SQCIF,
    QCIF,
    CIF,
    CIF4,
    CIF16,
    NumStdSizes,
    UnknownStdSize = NumStdSizes
  };

  int width;
  int height;
  const char * optionName;
} StandardVideoSizes[StdSizes::NumStdSizes] = {
  { SQCIF_WIDTH, SQCIF_HEIGHT, PLUGINCODEC_SQCIF_MPI },
  {  QCIF_WIDTH,  QCIF_HEIGHT, PLUGINCODEC_QCIF_MPI  },
  {   CIF_WIDTH,   CIF_HEIGHT, PLUGINCODEC_CIF_MPI   },
  {  CIF4_WIDTH,  CIF4_HEIGHT, PLUGINCODEC_CIF4_MPI  },
  { CIF16_WIDTH, CIF16_HEIGHT, PLUGINCODEC_CIF16_MPI },
};

/*
  This callback may receive log data from all FFMPEG based codecs.
 */
static void logCallbackFFMPEG (void* v, int level, const char* fmt , va_list arg)
{
  char buffer[512];
  int severity = 0;
  if (v) {
    switch (level)
    {
      case AV_LOG_QUIET:   severity = 0; break;
      case AV_LOG_PANIC:   severity = 0; break;
      case AV_LOG_FATAL:   severity = 0; break;
      case AV_LOG_ERROR:   severity = 1; break;
      case AV_LOG_WARNING: severity = 2; break;
      case AV_LOG_INFO:    severity = 4; break;
      case AV_LOG_VERBOSE: severity = 4; break;
      case AV_LOG_DEBUG:   severity = 4; break;
      case AV_LOG_TRACE:   severity = 4; break;
    }
    sprintf(buffer, "FFMPEG\t");
    vsprintf(buffer + strlen(buffer), fmt, arg);
    if (strlen(buffer) > 0)
      buffer[strlen(buffer)-1] = 0;
    if (severity == 4)
      { TRACE_UP (severity, buffer); }
    else
      { TRACE (severity, buffer); }
  }
}

/////////////////////////////////////////////////////////////////////////////

static char * num2str(int num)
{
  char buf[20];
  sprintf(buf, "%i", num);
  return strdup(buf);
}

#if TRACE_FILE

static void DumpRTPPayload(Tracer & tracer, const RTPFrame & rtp, int max)
{
  // GetPayloadSize() returns unsigned; max is always called with a small
  // positive literal (see the two call sites below), and payload sizes
  // never approach INT_MAX, so comparing as int is safe and avoids
  // -Wsign-compare
  if (max > (int)rtp.GetPayloadSize())
    max = (int)rtp.GetPayloadSize();
  unsigned char * ptr = rtp.GetPayloadPtr();
  tracer.GetStream() << hex << setfill('0') << setprecision(2);
  while (max-- > 0)
    tracer.GetStream() << (int) *ptr++ << ' ';
  tracer.GetStream() << setfill(' ') << dec;
}

static ostream & RTPDump(Tracer & tracer, const RTPFrame & rtp)
{
  tracer.GetStream() << "seq=" << rtp.GetSequenceNumber()
       << ",ts=" << rtp.GetTimestamp()
       << ",mkr=" << rtp.GetMarker()
       << ",pt=" << (int)rtp.GetPayloadType()
       << ",ps=" << rtp.GetPayloadSize();
  return tracer.GetStream();
}

static ostream & RFC2190Dump(Tracer & tracer, const RTPFrame & rtp)
{
  RTPDump(tracer, rtp);
  if (rtp.GetPayloadSize() > 2) {
    bool iFrame = false;
    char mode;
    unsigned char * payload = rtp.GetPayloadPtr();
    if ((payload[0] & 0x80) == 0) {
      mode = 'A';
      iFrame = (payload[1] & 0x10) == 0;
    }
    else if ((payload[0] & 0x40) == 0) {
      mode = 'B';
      iFrame = (payload[4] & 0x80) == 0;
    }
    else {
      mode = 'C';
      iFrame = (payload[4] & 0x80) == 0;
    }
    tracer.GetStream() << "mode=" << mode << ",I=" << (iFrame ? "yes" : "no");
  }
  tracer.GetStream() << ",data=";
  /* 32 bytes, not 10: enough to see a Mode A/B fragment's own header plus a
     genuine GOB header's GN field with margin either side, so a field trace
     can tell a GOB header (GN nonzero possible, non-zero low bits typical)
     apart from a picture start code (fixed zero bits follow) without
     guessing. 10 bytes left this ambiguous when checking a real trace against
     this exact question. */
  DumpRTPPayload(tracer, rtp, 32);
  return tracer.GetStream();
}

static ostream & RFC2429Dump(Tracer & tracer, const RTPFrame & rtp)
{
  RTPDump(tracer, rtp);
  tracer.GetStream() << ",data=";
  DumpRTPPayload(tracer, rtp, 32);
  return tracer.GetStream();
}

#define CODEC_TRACER_RTP(tracer, text, rtp, func) \
tracer.Start(); tracer.GetStream() << text; func(tracer, rtp); tracer.End()

#else

#define CODEC_TRACER_RTP(tracer, text, rtp, func)

#endif

/////////////////////////////////////////////////////////////////////////////

H263_Base_EncoderContext::H263_Base_EncoderContext(const char * _prefix)
  : _inputFrameBuffer(NULL)
  , _inputFrameBufferSize(0)
  , _codec(NULL)
  , _context(NULL)
  , _inputFrame(NULL)
  , m_packet(NULL)
  , _codecId(AV_CODEC_ID_NONE)
  , _frameCount(0)
  , _width(0)
  , _height(0)
  , _keyFramePeriod(H263_KEY_FRAME_INTERVAL)
  , _tsto(H263_DEFAULT_TSTO)
  , _maxRTPFrameSize(H263_PAYLOAD_SIZE)
  , _annexFlags(0)
  , _frameTime(3003)   // ~29.97 fps, matching the previous hardcoded default
  , prefix(_prefix)
#if TRACE_FILE
  , tracer(_prefix, true)
#endif
{
  m_targetBitRate = 0;
}

H263_Base_EncoderContext::~H263_Base_EncoderContext()
{
  CloseCodec();

  if (_inputFrame != NULL) {
    av_frame_free(&_inputFrame);
    _inputFrame = NULL;
  }
  if (m_packet != NULL) {
    av_packet_free(&m_packet);
    m_packet = NULL;
  }

  av_free(_inputFrameBuffer);
  _inputFrameBuffer = NULL;
  _inputFrameBufferSize = 0;

  TRACE_AND_LOG(tracer, 3, "encoder closed");
}

bool H263_Base_EncoderContext::Open(AVCodecID codecId)
{
  TRACE_AND_LOG(tracer, 1, "Opening encoder");

  _codecId = codecId;

  _codec = avcodec_find_encoder(codecId);
  if (_codec == NULL) {
    TRACE_AND_LOG(tracer, 1, "Codec not found for encoder");
    return false;
  }

  _inputFrame = av_frame_alloc();
  if (_inputFrame == NULL) {
    TRACE_AND_LOG(tracer, 1, "Failed to allocate frame for encoder");
    return false;
  }

  m_packet = av_packet_alloc();
  if (m_packet == NULL) {
    TRACE_AND_LOG(tracer, 1, "Failed to allocate packet for encoder");
    return false;
  }

  if (!InitContext())
    return false;

  _width  = CIF_WIDTH;
  _height = CIF_HEIGHT;

  SetTargetBitrate(256000);
  SetTSTO(H263_DEFAULT_TSTO);
  DisableAnnex(D);
  DisableAnnex(F);
  DisableAnnex(I);
  DisableAnnex(K);
  DisableAnnex(J);
  DisableAnnex(S);

  _frameCount = 0;

  TRACE_AND_LOG(tracer, 3, "encoder created");

  return true;
}

void H263_Base_EncoderContext::SetMaxKeyFramePeriod (unsigned period)
{
  _keyFramePeriod = period;
}

void H263_Base_EncoderContext::SetTargetBitrate (unsigned rate)
{
  m_targetBitRate = rate;
  CODEC_TRACER(tracer, "target bit rate set to " << m_targetBitRate);
}

void H263_Base_EncoderContext::SetFrameWidth (unsigned width)
{
  _width = width;
  CODEC_TRACER(tracer, "frame width set to " << width);
}

void H263_Base_EncoderContext::SetFrameHeight (unsigned height)
{
  _height = height;
  CODEC_TRACER(tracer, "frame height set to " << height);
}

void H263_Base_EncoderContext::SetTSTO (unsigned tsto)
{
  _tsto = tsto;
  CODEC_TRACER(tracer, "TSTO set to " << tsto);
}

void H263_Base_EncoderContext::SetTargetFrameTime (unsigned frameTime)
{
  if (frameTime == 0)
    return;   // guard against a bogus zero option value
  _frameTime = frameTime;
  CODEC_TRACER(tracer, "target frame time set to " << frameTime << " (" << ((double)H263_CLOCKRATE / frameTime) << " fps)");
}

void H263_Base_EncoderContext::EnableAnnex (Annex annex)
{
  _annexFlags |= (1 << annex);
}

void H263_Base_EncoderContext::DisableAnnex (Annex annex)
{
  _annexFlags &= ~(1 << annex);
}

/*
  libavcodec does not allow an AVCodecContext to be opened, closed and
  opened again, so the context is thrown away in CloseCodec() and rebuilt
  from the recorded settings here.
 */
bool H263_Base_EncoderContext::OpenCodec()
{
  CloseCodec();

  if (_codec == NULL) {
    TRACE_AND_LOG(tracer, 1, "Codec not initialized");
    return false;
  }

  _context = avcodec_alloc_context3(_codec);
  if (_context == NULL) {
    TRACE_AND_LOG(tracer, 1, "Failed to allocate context for encoder");
    return false;
  }

  _context->opaque = this;

  _context->width  = _width;
  _context->height = _height;
  _context->pix_fmt = AV_PIX_FMT_YUV420P;
  _context->max_b_frames = 0;
  _context->mb_decision = FF_MB_DECISION_SIMPLE;  // choose only one MB type at a time
  _context->thread_count = 1;                     // the RTP fragmenting needs deterministic output
  _context->time_base.num = _frameTime;
  _context->time_base.den = H263_CLOCKRATE;
  _context->framerate.num = H263_CLOCKRATE;
  _context->framerate.den = _frameTime;

  _context->gop_size = _keyFramePeriod;

  _context->error_concealment = 3;
  _context->err_recognition = 5;

  // bit rate control
  _context->bit_rate = (m_targetBitRate * 3) >> 2;       // average bit rate
  _context->bit_rate_tolerance = m_targetBitRate >> 1;
  _context->rc_min_rate = 0;                             // minimum bit rate
  _context->rc_max_rate = m_targetBitRate;               // maximum bit rate
  _context->rc_buffer_size = (m_targetBitRate / 1000) * 64;

  // quantiser limits derived from the temporal/spatial trade off
  _context->max_qdiff = 10;                 // max q difference between frames
  _context->qcompress = 0.5;                // qscale factor between easy & hard scenes (0.0-1.0)
  _context->i_quant_factor = (float)-0.6;   // qscale factor between p and i frames
  _context->i_quant_offset = (float)0.0;    // qscale offset between p and i frames

  /* The temporal/spatial trade off picks the worst quantiser we are willing
     to use.  Note what happens at the bottom of the range: a TSTO of 0 gives
     qmax == qmin, so every frame is coded at QP 2 and the rate controller has
     nothing left to vary.  The output is then whatever the content happens to
     need - several Mbit/s at 4CIF - no matter what bit rate was negotiated,
     the encoder logs "rc buffer underflow" and "max bitrate possibly too
     small" on every frame, and the far end drowns in packets.  So keep a
     working range no matter what we are asked for. */
  _context->qmin = H263P_MIN_QUANT;
  _context->qmax = (int)round((31.0 - H263P_MIN_QUANT) / 31.0 * _tsto + H263P_MIN_QUANT);
  if (_context->qmax > 31)
    _context->qmax = 31;
  if (_context->qmax < H263P_MIN_QUANT + H263_MIN_QUANT_RANGE)
    _context->qmax = H263P_MIN_QUANT + H263_MIN_QUANT_RANGE;

  // these used to be AVCodecContext fields and are private AVOptions now
  FFMPEGSetPrivateOption(_context, "motion_est", "epzs");
  FFMPEGSetPrivateOptionDouble(_context, "qsquish", 0);  // limit q by clipping
  FFMPEGSetPrivateOption(_context, "rc_eq", "1");        // rate control equation
  FFMPEGSetPrivateOption(_context, "lmin", (int64_t)(_context->qmin * FF_QP2LAMBDA));
  FFMPEGSetPrivateOption(_context, "lmax", (int64_t)(_context->qmax * FF_QP2LAMBDA));

  /* Annex support.
     Annex D (unrestricted motion vectors), F (advanced prediction), I
     (advanced intra coding), J (deblocking filter) and S (alternative inter
     VLC) map onto H.263+ encoder options.  Annex F was disabled in the
     original code because libavcodec was not thread safe with it, and
     Annex D/S are left off for interoperability with eyeBeam - keep that
     behaviour and only wire up the ones that were actually enabled. */
  if (_annexFlags & (1 << I))
    _context->flags |= AV_CODEC_FLAG_AC_PRED;
  if (_annexFlags & (1 << J))
    _context->flags |= AV_CODEC_FLAG_LOOP_FILTER;

  if (!ApplyCodecOptions()) {
    TRACE_AND_LOG(tracer, 1, "Failed to apply codec options");
    avcodec_free_context(&_context);
    return false;
  }

  // debugging flags
  if (Trace::CanTraceUserPlane(4)) {
    _context->debug |= FF_DEBUG_RC;
    _context->debug |= FF_DEBUG_PICT_INFO;
    _context->debug |= FF_DEBUG_QP;
  }

  CODEC_TRACER(tracer, "Size is " << _width << "x" << _height);
  CODEC_TRACER(tracer, "rc_max_rate is " << _context->rc_max_rate);
  CODEC_TRACER(tracer, "GOP is " << _context->gop_size);
  CODEC_TRACER(tracer, "qmin set to " << _context->qmin);
  CODEC_TRACER(tracer, "qmax set to " << _context->qmax);
  CODEC_TRACER(tracer, "bit_rate set to " << _context->bit_rate);
  CODEC_TRACER(tracer, "bit_rate_tolerance set to " << _context->bit_rate_tolerance);

  int err = avcodec_open2(_context, _codec, NULL);
  if (err < 0) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, buf, sizeof(buf));
    TRACE_AND_LOG(tracer, 1, "Failed to open encoder: " << buf);
    avcodec_free_context(&_context);
    return false;
  }

  // the input frame describes the picture we hand to avcodec_send_frame()
  _inputFrame->format = AV_PIX_FMT_YUV420P;
  _inputFrame->width  = _width;
  _inputFrame->height = _height;
  _inputFrame->linesize[0] = _width;
  _inputFrame->linesize[1] = _width / 2;
  _inputFrame->linesize[2] = _width / 2;

  TRACE_AND_LOG(tracer, 4, "Codec opened");

  return true;
}

/*
  The frame header states the picture dimensions, but nothing in the plugin
  API guarantees the RTP payload actually holds a picture that big.  Taking
  the header at its word means copying width*height*3/2 bytes out of a buffer
  that may be much smaller, which reads whatever follows it on the heap.

  The encoder then sees a picture that changes completely from frame to frame:
  motion compensation collapses (mc_mb_var_sum ends up several times
  mb_mb_var_sum), libavcodec's scene change detection fires over and over, and
  the output is a run of I frames several times the negotiated bit rate.  It
  also happens to be an out of bounds read of most of a megabyte driven by a
  field that arrives from outside.
 */
bool H263_Base_EncoderContext::ValidateSourceFrame(const RTPFrame & srcRTP,
                                                   const PluginCodec_Video_FrameHeader * header)
{
  if (header->x != 0 || header->y != 0) {
    TRACE_AND_LOG(tracer, 1, "Video grab of partial frame unsupported, dropping frame");
    return false;
  }

  if (header->width == 0 || header->height == 0 ||
      (header->width & 1) != 0 || (header->height & 1) != 0 ||
      header->width > CIF16_WIDTH || header->height > CIF16_HEIGHT) {
    TRACE_AND_LOG(tracer, 1, "Implausible frame size " << header->width << "x" << header->height
                              << ", dropping frame");
    return false;
  }

  size_t needed = sizeof(PluginCodec_Video_FrameHeader)
                + ((size_t)header->width * header->height * 3) / 2;
  size_t have   = (size_t)srcRTP.GetPayloadSize();

  if (have < needed) {
    TRACE_AND_LOG(tracer, 1, "Frame buffer holds " << have << " bytes but the header claims "
                              << header->width << "x" << header->height << ", which needs "
                              << needed << " - dropping frame");
    return false;
  }

  return true;
}

bool H263_Base_EncoderContext::AllocateInputFrameBuffer(unsigned width, unsigned height)
{
  size_t frameSize = (size_t)width * height * 3 / 2;
  size_t required  = frameSize + AV_INPUT_BUFFER_PADDING_SIZE;

  if (_inputFrameBuffer != NULL && _inputFrameBufferSize >= required)
    return true;

  av_free(_inputFrameBuffer);
  _inputFrameBufferSize = 0;

  // av_malloc() gives us the alignment libavcodec wants
  _inputFrameBuffer = (unsigned char *)av_malloc(required);
  if (_inputFrameBuffer == NULL)
    return false;

  _inputFrameBufferSize = required;
  return true;
}

void H263_Base_EncoderContext::CloseCodec()
{
  if (_context != NULL)
    avcodec_free_context(&_context);
  _context = NULL;
}

/*
  Hand one YUV420P frame to the encoder and collect the result.  The old API
  returned the encoded bytes from a single call; the send/receive API may
  need more than one frame before it produces anything, although with
  max_b_frames == 0 and no lookahead the H.263 encoder produces exactly one
  packet per frame.  Returns 1 when m_packet holds a picture, 0 when the
  encoder wants more input and -1 on error.
 */
int H263_Base_EncoderContext::EncodeOneFrame(unsigned int flags)
{
  _inputFrame->pict_type = (flags & PluginCodec_CoderForceIFrame) ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_NONE;
  _inputFrame->pts = _frameCount;

  int err = avcodec_send_frame(_context, _inputFrame);
  if (err < 0 && err != AVERROR(EAGAIN)) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, buf, sizeof(buf));
    TRACE_AND_LOG(tracer, 1, "avcodec_send_frame failed: " << buf);
    return -1;
  }

  av_packet_unref(m_packet);

  err = avcodec_receive_packet(_context, m_packet);
  if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
    return 0;

  if (err < 0) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, buf, sizeof(buf));
    TRACE_AND_LOG(tracer, 1, "avcodec_receive_packet failed: " << buf);
    return -1;
  }

  return 1;
}

void H263_Base_EncoderContext::Lock()
{
  _mutex.Wait();
}

void H263_Base_EncoderContext::Unlock()
{
  _mutex.Signal();
}

void H263_Base_EncoderContext::AddInputFormat(inputFormats & fmt)
{
    videoInputFormats.push_back(fmt);
}

int H263_Base_EncoderContext::GetInputFormat(inputFormats & fmt, unsigned maxWidth, unsigned maxHeight)
{
    /* Picks the largest size the camera offers that still fits within
       maxWidth/maxHeight - which is the negotiated capability's own ceiling
       (H.263-QCIF/H.263-CIF/H.263-720 each represent exactly one nominal
       size, not a range h323plus can renegotiate at runtime).
     */
    for (std::list<inputFormats>::const_iterator r=videoInputFormats.begin(); r!=videoInputFormats.end(); ++r) {
        if (r->w > maxWidth || r->h > maxHeight)
            continue;

        for (int i= 0; i < StdSizes::NumStdSizes; i++) {
            if (StandardVideoSizes[i].width != (int)r->w ||
                StandardVideoSizes[i].height != (int)r->h)
                continue;

            fmt = *r;
            return 1;
        }
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////

H263_RFC2190_EncoderContext::H263_RFC2190_EncoderContext()
  : H263_Base_EncoderContext("RFC2190")
{
}

H263_RFC2190_EncoderContext::~H263_RFC2190_EncoderContext()
{
  WaitAndSignal m(_mutex);
  CloseCodec();
}

bool H263_RFC2190_EncoderContext::Open()
{
  if (!H263_Base_EncoderContext::Open(AV_CODEC_ID_H263))
    return false;

  SetMaxKeyFramePeriod(H263_KEY_FRAME_INTERVAL);
  SetMaxRTPFrameSize(H263_PAYLOAD_SIZE);

  return true;
}

bool H263_RFC2190_EncoderContext::InitContext()
{
  return true;
}

/* PLUGINCODEC_OPTION_MAX_FRAME_SIZE is the largest RTP payload we may
   produce, not the largest datagram, so the RTP header is not deducted
   here - that matches what the RFC 2429 side does with the same option. */
void H263_RFC2190_EncoderContext::SetMaxRTPFrameSize (unsigned size)
{
  if (size < (RFC2190_MODE_B_HEADER_SIZE + 16))
    size = H263_PAYLOAD_SIZE;
  _maxRTPFrameSize = size;
}

/*
  RFC 2190 needs to split the encoded picture on macroblock boundaries.
  AVCodecContext::rtp_callback used to report those boundaries while
  encoding but was removed in FFmpeg 5.0.  There are two supported
  replacements, and this uses only one of them - deliberately.

  "ps" (formerly rtp_payload_size) tells the H.263/MPEG-style encoder to
  insert a real GOB (resync) header into the ELEMENTARY BITSTREAM roughly
  every "ps" bytes - this is what mpegvideo_enc.c calls "rtp_mode", and nothing
  about it is RTP-specific despite the name; it works identically whether or
  not the output is ever put on the wire as RTP at all.  The packetiser then
  finds those GOB headers with FindResyncMarkerReverse() and splits there,
  producing a Mode A packet - no per-fragment metadata needed, just "here are
  the next N bytes of the stream".

  "mb_info" is the OTHER replacement: set it and every encoded AVPacket
  carries an AV_PKT_DATA_H263_MB_INFO side data block describing macroblock
  boundaries, which the packetiser can split on when no GOB header falls
  close enough to the target payload size, producing a Mode B packet with
  accurate QUANT/GOBN/MBA/motion-vector-predictor fields.

  Only "ps" is set here. Two reasons, and they compound:

  1. Setting "mb_info" has a genuine, deterministic libavcodec bug: it
     allocates the side data buffer at exactly mb_width*mb_height*12 bytes
     and resets mb_info_size just ABOVE the vbv_retry label in
     ff_mpv_encode_picture(), so every re-encode of a frame under rate
     control pressure keeps appending 12 more bytes to a buffer sized for a
     single pass, and write_mb_info() eventually writes past the end of it -
     confirmed with ASan, and confirmed this is independent of whether "ps"
     is also set (the accumulation is gated purely by whether "mb_info" is
     non-zero, in update_mb_info(), nothing to do with rtp_mode/"ps").
     Checked against FFmpeg master (September 2026); the reset is still
     above the retry label. "mb_info" must never be set until upstream fixes
     this.

  2. NOT setting "ps" either (the previous state of this function) has its
     own, separate cost: the GOB-header-writing code in mpegvideo_enc.c is
     gated entirely on "ps" (via s->rtp_mode = !!s->rtp_payload_size), so
     without it the encoder writes a picture start code and then nothing
     else - no interior resync points anywhere in the bitstream at all.
     Confirmed empirically: with neither option set, a CIF frame carries
     exactly one start code and no GOB headers; with "ps" set, 2-4 GOB
     headers appear per frame at typical payload sizes. Real H.263 encoders
     - hardware and software alike - overwhelmingly emit periodic GOB
     headers as a matter of course, and a hardware/embedded decoder that has
     mostly been tested against that common case may not be as forgiving of
     a bitstream that never has one as a lenient, reference decoder is. A
     Polycom RealPresence Desktop decoding one good frame and then going
     black matches this: byte-identical reassembly (verified in this
     plugin's own tests) is not the same thing as producing the kind of
     H.263 stream most decoders in the field actually expect to see.

  With only "ps" set, the packetiser gets real GOB boundaries to split on
  for the large majority of packets (Mode A, no metadata needed) and falls
  back to a plain byte-boundary split only on the rare oversized GOB, which
  never touches the vulnerable code path because "mb_info" is never enabled.
 */
bool H263_RFC2190_EncoderContext::ApplyCodecOptions()
{
  // 4 bytes of Mode A or 8 bytes of Mode B payload header come on top;
  // budget for the larger one, same as the packetiser does
  unsigned payload = _maxRTPFrameSize;
  if (payload > RFC2190_MODE_B_HEADER_SIZE)
    payload -= RFC2190_MODE_B_HEADER_SIZE;

  _context->flags &= ~AV_CODEC_FLAG_4MV;

  FFMPEGSetPrivateOption(_context, "ps", (int64_t)payload);

  return true;
}

int H263_RFC2190_EncoderContext::EncodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags)
{
  WaitAndSignal m(_mutex);

  if (_codec == NULL) {
    TRACE_AND_LOG(tracer, 1, "Encoder\tCodec not initialized");
    return 0;
  }

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen);
  dstLen = 0;

  // if still running out packets from previous frame, then return it
  if (packetizer.GetPacket(dstRTP, flags) != 0) {
    CODEC_TRACER_RTP(tracer, "Tx frame:", dstRTP, RFC2190Dump);
    dstLen = dstRTP.GetHeaderSize() + dstRTP.GetPayloadSize();
    return 1;
  }

  // zero payload means do nothing
  if (srcRTP.GetPayloadSize() == 0) {
    TRACE_AND_LOG(tracer, 1, "Zero payload passed");
    dstLen = dstRTP.GetHeaderSize();
    dstRTP.SetPayloadSize(0);
    dstRTP.SetMarker(true);
    flags |= 1;
    return 1;
  }

  // make sure the source frame is legal
  if ((size_t)srcRTP.GetPayloadSize() < sizeof(PluginCodec_Video_FrameHeader)) {
    TRACE_AND_LOG(tracer, 1, "Video grab too small, dropping frame");
    return 0;
  }
  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)srcRTP.GetPayloadPtr();
  if (!ValidateSourceFrame(srcRTP, header))
    return 0;

  // if this is the first frame, or the frame size has changed, deal with it
  if ((_context == NULL) ||
      (_frameCount == 0) ||
      ((unsigned) _width  != header->width) ||
      ((unsigned) _height != header->height)) {

    TRACE_AND_LOG(tracer, 4, "First frame received or resolution has changed - reopening codec");
    SetFrameWidth(header->width);
    SetFrameHeight(header->height);
    if (!OpenCodec()) {
      TRACE_AND_LOG(tracer, 1, "Reopening codec failed");
      return 0;
    }
    if (!AllocateInputFrameBuffer(header->width, header->height)) {
      TRACE_AND_LOG(tracer, 1, "Unable to allocate memory for frame buffer");
      return 0;
    }
  }

  CODEC_TRACER(tracer, "Input:seq=" << _frameCount
                       << ",size=" << header->width << "x" << header->height
                       << ",I=" << ((flags & PluginCodec_CoderForceIFrame) ? "yes" : "no"));

  int size = header->width * header->height;
  int frameSize = (size * 3) >> 1;

  // libavcodec reads in multiples of the SIMD width, so the plane needs
  // AV_INPUT_BUFFER_PADDING_SIZE readable bytes behind it
  memcpy(_inputFrameBuffer, OPAL_VIDEO_FRAME_DATA_PTR(header), frameSize);
  memset(_inputFrameBuffer + frameSize, 0, AV_INPUT_BUFFER_PADDING_SIZE);

  _inputFrame->data[0] = _inputFrameBuffer;
  _inputFrame->data[1] = _inputFrame->data[0] + size;
  _inputFrame->data[2] = _inputFrame->data[1] + (size / 4);

  int gotPacket = EncodeOneFrame(flags);
  ++_frameCount;

  if (gotPacket < 0) {
    TRACE_AND_LOG(tracer, 1, "Encoder failed");
    return 0;
  }

  if (gotPacket == 0) {
    TRACE_AND_LOG(tracer, 1, "Encoder returned empty frame");
    dstRTP.SetPayloadSize(0);
    dstLen = dstRTP.GetHeaderSize();
    flags |= 1;
    return 1;
  }

  size_t mbInfoSize = 0;
  const unsigned char * mbInfo = NULL;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 0, 100)
  size_t sideDataSize = 0;
  mbInfo = av_packet_get_side_data(m_packet, AV_PKT_DATA_H263_MB_INFO, &sideDataSize);
  mbInfoSize = sideDataSize;
#else
  int sideDataSize = 0;
  mbInfo = av_packet_get_side_data(m_packet, AV_PKT_DATA_H263_MB_INFO, &sideDataSize);
  mbInfoSize = (sideDataSize > 0) ? (size_t)sideDataSize : 0;
#endif

  /* libavcodec sizes this block at one 12 byte entry per macroblock.  If it
     comes back longer the encoder has overrun its own buffer, so do not add
     to the damage by reading it. */
  size_t maxMBInfo = (size_t)(((_width + 15) / 16) * ((_height + 15) / 16)) * 12;
  if (mbInfoSize > maxMBInfo) {
    TRACE_AND_LOG(tracer, 1, "Discarding implausible macroblock info: " << mbInfoSize
                              << " bytes for " << (maxMBInfo / 12) << " macroblocks");
    mbInfo = NULL;
    mbInfoSize = 0;
  }

  int result = packetizer.Open(srcRTP.GetTimestamp(),
                               m_packet->data, (size_t)m_packet->size,
                               mbInfo, mbInfoSize,
                               _maxRTPFrameSize);

  CODEC_TRACER(tracer, "Encoder returned " << m_packet->size << " bytes as "
                        << packetizer.GetFragmentCount() << " fragments ("
                        << (mbInfoSize / 12) << " macroblock info entries)");

  av_packet_unref(m_packet);

  if (result < 0) {
    TRACE_AND_LOG(tracer, 1, "Packetizer failed with code " << result);
    flags = 1;
    return 0;
  }

  // return the first encoded block of data
  if (packetizer.GetPacket(dstRTP, flags)) {
    CODEC_TRACER_RTP(tracer, "Tx frame:", dstRTP, RFC2190Dump);
    dstLen = dstRTP.GetHeaderSize() + dstRTP.GetPayloadSize();
  }

  return 1;
}

/////////////////////////////////////////////////////////////////////////////

H263_RFC2429_EncoderContext::H263_RFC2429_EncoderContext()
 : H263_Base_EncoderContext("RFC2429")
{
  _txH263PFrame = NULL;
}

H263_RFC2429_EncoderContext::~H263_RFC2429_EncoderContext()
{
  WaitAndSignal m(_mutex);

  CloseCodec();

  delete _txH263PFrame;
  _txH263PFrame = NULL;
}

bool H263_RFC2429_EncoderContext::Open()
{
  if (!H263_Base_EncoderContext::Open(AV_CODEC_ID_H263P))
    return false;

  SetMaxKeyFramePeriod(H263P_KEY_FRAME_INTERVAL);
  SetMaxRTPFrameSize(H263P_PAYLOAD_SIZE);

  return true;
}

bool H263_RFC2429_EncoderContext::InitContext()
{
  _txH263PFrame = new H263PFrame(MAX_YUV420P_FRAME_SIZE);
  return _txH263PFrame != NULL;
}

void H263_RFC2429_EncoderContext::SetMaxRTPFrameSize (unsigned size)
{
  if (size < 32)
    size = H263P_PAYLOAD_SIZE;

  _maxRTPFrameSize = size;

  if (_txH263PFrame != NULL)
    _txH263PFrame->SetMaxPayloadSize((uint16_t)size);
}

bool H263_RFC2429_EncoderContext::ApplyCodecOptions()
{
  // rtp_payload_size is the private "ps" option these days; leave a little
  // headroom for the RFC 2429 payload header
  unsigned payload = (_maxRTPFrameSize * 6) / 7;
  if (payload == 0)
    payload = _maxRTPFrameSize;

  FFMPEGSetPrivateOption(_context, "ps", (int64_t)payload);

  return true;
}

int H263_RFC2429_EncoderContext::EncodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags)
{
  WaitAndSignal m(_mutex);

  if (_codec == NULL) {
    TRACE_AND_LOG(tracer, 1, "Codec not initialized");
    return 0;
  }

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen);
  dstLen = 0;

  // if there are RTP packets to return, return them
  if (_txH263PFrame->HasRTPFrames())
  {
    _txH263PFrame->GetRTPFrame(dstRTP, flags);
    dstLen = dstRTP.GetFrameLen();
    CODEC_TRACER_RTP(tracer, "Tx frame:", dstRTP, RFC2429Dump);
    return 1;
  }

  if ((size_t)srcRTP.GetPayloadSize() < sizeof(PluginCodec_Video_FrameHeader)) {
    TRACE_AND_LOG(tracer, 1, "Video grab too small, dropping frame");
    return 0;
  }

  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)srcRTP.GetPayloadPtr();
  if (!ValidateSourceFrame(srcRTP, header))
    return 0;

  // if this is the first frame, or the frame size has changed, deal with it
  if ((_context == NULL) ||
      (_frameCount == 0) ||
      ((unsigned) _width  != header->width) ||
      ((unsigned) _height != header->height)) {

    TRACE_AND_LOG(tracer, 4, "First frame received or resolution has changed - reopening codec");
    SetFrameWidth(header->width);
    SetFrameHeight(header->height);
    if (!OpenCodec()) {
      TRACE_AND_LOG(tracer, 1, "Reopening codec failed");
      return 0;
    }
    if (!AllocateInputFrameBuffer(header->width, header->height)) {
      TRACE_AND_LOG(tracer, 1, "Unable to allocate memory for frame buffer");
      return 0;
    }
  }

  CODEC_TRACER(tracer, "Input:seq=" << _frameCount
                       << ",size=" << header->width << "x" << header->height
                       << ",I=" << ((flags & PluginCodec_CoderForceIFrame) ? "yes" : "no"));

  int size = header->width * header->height;
  int frameSize = (size * 3) >> 1;

  memcpy(_inputFrameBuffer, OPAL_VIDEO_FRAME_DATA_PTR(header), frameSize);
  memset(_inputFrameBuffer + frameSize, 0, AV_INPUT_BUFFER_PADDING_SIZE);

  _inputFrame->data[0] = _inputFrameBuffer;
  _inputFrame->data[1] = _inputFrame->data[0] + size;
  _inputFrame->data[2] = _inputFrame->data[1] + (size / 4);

  _txH263PFrame->BeginNewFrame();
  _txH263PFrame->SetTimestamp(srcRTP.GetTimestamp());

  int gotPacket = EncodeOneFrame(flags);
  _frameCount++;

  if (gotPacket < 0) {
    TRACE_AND_LOG(tracer, 1, "Encoder failed");
    return 0;
  }

  if (gotPacket == 0) {
    TRACE_AND_LOG(tracer, 1, "Encoder internal error - there should be outstanding packets at this point");
    return 1;
  }

  if ((size_t)m_packet->size > (size_t)MAX_YUV420P_FRAME_SIZE) {
    TRACE_AND_LOG(tracer, 1, "Encoded frame of " << m_packet->size << " bytes does not fit the frame buffer");
    av_packet_unref(m_packet);
    return 0;
  }

  memcpy(_txH263PFrame->GetFramePtr(), m_packet->data, m_packet->size);
  _txH263PFrame->SetFrameSize(m_packet->size);
  av_packet_unref(m_packet);

  CODEC_TRACER(tracer, "Encoder created " << _txH263PFrame->GetFrameSize() << " bytes of output");

  if (_txH263PFrame->HasRTPFrames())
  {
    _txH263PFrame->GetRTPFrame(dstRTP, flags);
    dstLen = dstRTP.GetFrameLen();
    CODEC_TRACER_RTP(tracer, "Tx frame:", dstRTP, RFC2429Dump);
    return 1;
  }
  return 1;
}

/////////////////////////////////////////////////////////////////////////////

H263_Base_DecoderContext::H263_Base_DecoderContext(const char * _prefix)
  : _codec(NULL)
  , _context(NULL)
  , _outputFrame(NULL)
  , m_packet(NULL)
  , _frameCount(0)
  , prefix(_prefix)
#if TRACE_FILE
  , tracer(_prefix, false)
#endif
{
  // AV_CODEC_ID_H263 decodes both H.263 and H.263+ streams
  if ((_codec = avcodec_find_decoder(AV_CODEC_ID_H263)) == NULL) {
    TRACE_AND_LOG(tracer, 1, "Codec not found for decoder");
    return;
  }

  _outputFrame = av_frame_alloc();
  if (_outputFrame == NULL) {
    TRACE_AND_LOG(tracer, 1, "Failed to allocate frame for decoder");
    return;
  }

  m_packet = av_packet_alloc();
  if (m_packet == NULL) {
    TRACE_AND_LOG(tracer, 1, "Failed to allocate packet for decoder");
    return;
  }

  if (!OpenCodec()) {
    TRACE_AND_LOG(tracer, 1, "Failed to open codec for decoder");
    return;
  }

  TRACE_AND_LOG(tracer, 4, "Decoder created");
}

H263_Base_DecoderContext::~H263_Base_DecoderContext()
{
  CloseCodec();

  if (_outputFrame != NULL) {
    av_frame_free(&_outputFrame);
    _outputFrame = NULL;
  }
  if (m_packet != NULL) {
    av_packet_free(&m_packet);
    m_packet = NULL;
  }
}

bool H263_Base_DecoderContext::OpenCodec()
{
  if (_codec == NULL) {
    TRACE_AND_LOG(tracer, 1, "Codec not initialized");
    return false;
  }

  CloseCodec();

  _context = avcodec_alloc_context3(_codec);
  if (_context == NULL) {
    TRACE_AND_LOG(tracer, 1, "Failed to allocate context for decoder");
    return false;
  }

  _context->opaque = this;
  _context->workaround_bugs = FF_BUG_AUTODETECT;
  _context->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;

  // debugging flags have to be set before the codec is opened
  if (Trace::CanTrace(4)) {
    _context->debug |= FF_DEBUG_RC;
    _context->debug |= FF_DEBUG_PICT_INFO;
  }

  int err = avcodec_open2(_context, _codec, NULL);
  if (err < 0) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, buf, sizeof(buf));
    TRACE_AND_LOG(tracer, 1, "Failed to open H.263 decoder: " << buf);
    avcodec_free_context(&_context);
    return false;
  }

  _frameCount = 0;

  TRACE_AND_LOG(tracer, 4, "Codec opened");

  return true;
}

void H263_Base_DecoderContext::CloseCodec()
{
  if (_context != NULL) {
    avcodec_free_context(&_context);
    _context = NULL;
    TRACE_AND_LOG(tracer, 4, "Closed H.263 decoder");
  }
}

/*
  One whole access unit goes in, at most one picture comes out.  Unlike
  avcodec_decode_video2() the send/receive API does not report how many
  bytes were consumed, so callers that used to look at the byte count now
  just test the return value.
 */
int H263_Base_DecoderContext::DecodeOneFrame(const BYTE * data, size_t length)
{
  if (_context == NULL || m_packet == NULL || _outputFrame == NULL)
    return -1;

  av_packet_unref(m_packet);
  m_packet->data = (uint8_t *)data;
  m_packet->size = (int)length;

  int err = avcodec_send_packet(_context, m_packet);
  m_packet->data = NULL;
  m_packet->size = 0;

  if (err < 0 && err != AVERROR(EAGAIN)) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, buf, sizeof(buf));
    TRACE_AND_LOG(tracer, 1, "avcodec_send_packet failed: " << buf);
    return -1;
  }

  err = avcodec_receive_frame(_context, _outputFrame);
  if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
    return 0;

  if (err < 0) {
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, buf, sizeof(buf));
    TRACE_AND_LOG(tracer, 1, "avcodec_receive_frame failed: " << buf);
    return -1;
  }

  return 1;
}

///////////////////////////////////////////////////////////////////////////////////

H263_RFC2429_DecoderContext::H263_RFC2429_DecoderContext()
  : H263_Base_DecoderContext("RFC2429")
{
  _rxH263PFrame = new H263PFrame(MAX_YUV420P_FRAME_SIZE);
  _skippedFrameCounter = 0;
  _gotIFrame = false;
  _gotAGoodFrame = true;
}

H263_RFC2429_DecoderContext::~H263_RFC2429_DecoderContext()
{
  if (_rxH263PFrame)
    delete _rxH263PFrame;
}

bool H263_RFC2429_DecoderContext::DecodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags)
{
  TRACE_AND_LOG(tracer, 4, "Codec opened");

  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  CODEC_TRACER_RTP(tracer, "Tx frame:", srcRTP, RFC2429Dump);

  // create RTP frame from destination buffer
  RTPFrame dstRTP(dst, dstLen, 0);
  dstLen = 0;

  if (!_rxH263PFrame->SetFromRTPFrame(srcRTP, flags)) {
    _rxH263PFrame->BeginNewFrame();
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return true;
  }

  if (srcRTP.GetMarker()==0)
  {
     return 1;
  }

  if (_rxH263PFrame->GetFrameSize()==0)
  {
    _rxH263PFrame->BeginNewFrame();
    TRACE_AND_LOG(tracer, 4, "Got an empty frame - skipping");
    _skippedFrameCounter++;
    return 1;
  }

  if (!_rxH263PFrame->hasPicHeader()) {
    TRACE_AND_LOG(tracer, 1, "Received frame has no picture header - dropping");
    _rxH263PFrame->BeginNewFrame();
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  // look and see if we have read an I frame.
  if (!_gotIFrame)
  {
    if (!_rxH263PFrame->IsIFrame())
    {
      TRACE_AND_LOG(tracer, 1, "Waiting for an I-Frame");
      _rxH263PFrame->BeginNewFrame();
      flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
      _gotAGoodFrame = false;
      return 1;
    }
    _gotIFrame = true;
  }

  TRACE_AND_LOG(tracer, 4, "Decoding " << _rxH263PFrame->GetFrameSize()  << " bytes");
  int gotPicture = DecodeOneFrame(_rxH263PFrame->GetFramePtr(), _rxH263PFrame->GetFrameSize());

  _rxH263PFrame->BeginNewFrame();

  if (gotPicture <= 0)
  {
    TRACE_AND_LOG(tracer, 1, "Decoder did not produce a picture");
    _skippedFrameCounter++;
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }

  TRACE_AND_LOG(tracer, 4, "Decoded a picture, Resolution: " << _outputFrame->width << "x" << _outputFrame->height);

  // if decoded frame size is not legal, request an I-Frame
  if (_context->width == 0 || _context->height == 0) {
    TRACE_AND_LOG(tracer, 1, "Received frame with invalid size");
    flags = (_gotAGoodFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);
    _gotAGoodFrame = false;
    return 1;
  }
  _gotAGoodFrame = true;

  int frameBytes = (_context->width * _context->height * 12) / 8;
  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)dstRTP.GetPayloadPtr();
  header->x = header->y = 0;
  header->width = _context->width;
  header->height = _context->height;
  int size = _context->width * _context->height;
  if (_outputFrame->data[1] == _outputFrame->data[0] + size
      && _outputFrame->data[2] == _outputFrame->data[1] + (size >> 2)) {
    memcpy(OPAL_VIDEO_FRAME_DATA_PTR(header), _outputFrame->data[0], frameBytes);
  } else {
    unsigned char *dst = OPAL_VIDEO_FRAME_DATA_PTR(header);
    for (int i=0; i<3; i ++) {
      unsigned char *src = _outputFrame->data[i];
      int dst_stride = i ? _context->width >> 1 : _context->width;
      int src_stride = _outputFrame->linesize[i];
      int h = i ? _context->height >> 1 : _context->height;

      if (src_stride==dst_stride) {
        memcpy(dst, src, dst_stride*h);
        dst += dst_stride*h;
      } else {
        while (h--) {
          memcpy(dst, src, dst_stride);
          dst += dst_stride;
          src += src_stride;
        }
      }
    }
  }

  dstRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader) + frameBytes);
  dstRTP.SetTimestamp(srcRTP.GetTimestamp());
  dstRTP.SetMarker(true);

  dstLen = dstRTP.GetFrameLen();

  flags = PluginCodec_ReturnCoderLastFrame ;

  _frameCount++;

  return 1;
}


H263_RFC2190_DecoderContext::H263_RFC2190_DecoderContext()
  : H263_Base_DecoderContext("RFC2190")
{
}

H263_RFC2190_DecoderContext::~H263_RFC2190_DecoderContext()
{
}

/*
  Every DecodeFrames() path that has nothing to show yet (still waiting on
  more RTP fragments, or the picture failed to decode) returns through here
  with a zero length payload.

  This used to also set PluginCodec_ReturnCoderLastFrame - the same flag the
  success path at the end of DecodeFrames() sets - on every one of those
  empty returns.  Per its own definition in opalplugin.h ("indicates when
  video codec returns LAST DATA for frame"), that flag means data is being
  returned, which is never true here: dstLen is 0.  h323plus's own
  H323PluginVideoCodec::WriteInternal() (h323pluginmgr.cxx) trusts this flag
  alone to decide a picture is ready, then reads width/height straight out of
  the (empty) output buffer and calls SetFrameSize() on whatever garbage is
  sitting there - which fails, and a failed SetFrameSize() makes
  WriteInternal() return false, which makes H323_RTPChannel::Receive()
  (channels.cxx) close the receive channel on the spot, no retry.

  Every H.263 picture above a trivial size needs more than one RTP packet,
  and every packet before the last one has its marker bit clear - so with
  RFC2190Depacketizer::SetPacket() correctly waiting for that marker (fixed
  earlier in this file; without the fix, an unmarked first fragment was
  wrongly treated as a complete picture, which is a worse bug on its own),
  the very first video packet of any real call now reliably took this path
  and reliably hit that h323plus bug - regardless of which encoder sent it,
  since nothing about the sender matters here.  Not setting the flag on an
  empty return is what the flag's own definition already says to do, and it
  is enough on its own: WriteInternal() has a separate, correct branch for
  "no payload yet" (toLen < PLUGIN_RTP_HEADER_SIZE) that just waits for the
  next packet.
 */
static bool ReturnEmptyFrame(RTPFrame & dstRTP, unsigned & dstLen, unsigned int & flags)
{
  dstRTP.SetPayloadSize(0);
  dstLen = 0;
  return true;
}

bool H263_RFC2190_DecoderContext::DecodeFrames(const BYTE * src, unsigned & srcLen, BYTE * dst, unsigned & dstLen, unsigned int & flags)
{
  // create RTP frame from source buffer
  RTPFrame srcRTP(src, srcLen);

  // create RTP frame from destination
  RTPFrame dstRTP(dst, dstLen, 0);
  dstRTP.SetTimestamp(srcRTP.GetTimestamp());

  if (dstLen < (12 + sizeof(PluginCodec_Video_FrameHeader))) {
    flags = 0;
    TRACE_AND_LOG(tracer, 1, "Destination buffer " << dstLen << " insufficient for video header");
    ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  dstLen = 0;

  CODEC_TRACER_RTP(tracer, "Rx frame:", srcRTP, RFC2190Dump);

  // push new frame through the depacketiser
  bool requestIFrame, isIFrame;
  int code = depacketizer.SetPacket(srcRTP, requestIFrame, isIFrame);
  if (code <= 0) {
    flags = requestIFrame ? PluginCodec_ReturnCoderRequestIFrame : 0;
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  if ((depacketizer.frame.size() < 3)  ||
      (depacketizer.frame[0] != 0x00) ||
      (depacketizer.frame[1] != 0x00) ||
      (depacketizer.frame[2] & 0x80) != 0x80) {
    TRACE_AND_LOG(tracer, 1, "Frame does not start with correct code");
    flags = PluginCodec_ReturnCoderRequestIFrame;
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  TRACE_AND_LOG(tracer, 4, "Decoder called with " << depacketizer.frame.size()  << " bytes");

  int gotPicture = DecodeOneFrame(&depacketizer.frame[0], depacketizer.frame.size());

  depacketizer.NewFrame();

  if (gotPicture <= 0) {
    flags = PluginCodec_ReturnCoderRequestIFrame;
    TRACE_AND_LOG(tracer, 1, "Decoder did not produce a picture");
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  TRACE_AND_LOG(tracer, 4, "Decoder created frame at " << _outputFrame->width << "x" << _outputFrame->height);

  /* AVCodecContext::decode_error_count is gone; the per frame
     decode_error_flags carry the same information. */
  if (_outputFrame->decode_error_flags != 0) {
    flags = PluginCodec_ReturnCoderRequestIFrame;
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  // if decoded frame size is not legal, request an I-Frame
  if ((_context->width <= 0) ||
      (_context->height <= 0) ||
      (_context->width > CIF4_WIDTH) ||
      (_context->height > CIF4_HEIGHT) ||
      (_context->height * _context->width > (CIF4_WIDTH * CIF4_HEIGHT))) {
    TRACE_AND_LOG(tracer, 1, "Received frame with invalid size");
    flags = PluginCodec_ReturnCoderRequestIFrame;
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  // create RTP frame from destination buffer
  unsigned frameBytes = (_context->width * _context->height * 12) / 8;
  if (dstRTP.GetPayloadSize() - sizeof(PluginCodec_Video_FrameHeader) < frameBytes) {
    TRACE_AND_LOG(tracer, 1, "Destination buffer size " << dstRTP.GetPayloadSize() << " too small for frame of size " << _context->width  << "x" <<  _context->height);
    flags = PluginCodec_ReturnCoderRequestIFrame;
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)dstRTP.GetPayloadPtr();
  header->x      = header->y = 0;
  header->width  = _context->width;
  header->height = _context->height;
  int size = _context->width * _context->height;

  if ((unsigned)dstRTP.GetFrameLen() < (frameBytes + sizeof(PluginCodec_Video_FrameHeader))) {
    flags = PluginCodec_ReturnCoderRequestIFrame;
    TRACE_AND_LOG(tracer, 1, "Destination buffer " << dstLen << " insufficient for decoded data size " << header->width << "x" << header->height);
    return ReturnEmptyFrame(dstRTP, dstLen, flags);
  }

  dstRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader) + frameBytes);
  dstLen = dstRTP.GetHeaderSize() + dstRTP.GetPayloadSize();

  if (
       (_outputFrame->data[1] == (_outputFrame->data[0] + size)) &&
       (_outputFrame->data[2] == (_outputFrame->data[1] + (size >> 2)))
     ) {
    memcpy(OPAL_VIDEO_FRAME_DATA_PTR(header), _outputFrame->data[0], frameBytes);
  } else {
    unsigned char *dst = OPAL_VIDEO_FRAME_DATA_PTR(header);
    for (int i=0; i<3; i ++) {
      unsigned char *src = _outputFrame->data[i];
      int dst_stride = i ? _context->width >> 1 : _context->width;
      int src_stride = _outputFrame->linesize[i];
      int h = i ? _context->height >> 1 : _context->height;

      if (src_stride==dst_stride) {
        memcpy(dst, src, dst_stride*h);
        dst += dst_stride*h;
      } else {
        while (h-- > 0) {
          memcpy(dst, src, dst_stride);
          dst += dst_stride;
          src += src_stride;
        }
      }
    }
  }

  dstRTP.SetTimestamp(srcRTP.GetTimestamp());
  dstRTP.SetMarker(true);

  flags = PluginCodec_ReturnCoderLastFrame |
          (isIFrame ? PluginCodec_ReturnCoderIFrame : 0) |
          (requestIFrame ? PluginCodec_ReturnCoderRequestIFrame : 0);

  _frameCount++;

  return 1;
}

/////////////////////////////////////////////////////////////////////////////

static int get_codec_options(const struct PluginCodec_Definition * codec,
                                                  void *,
                                                  const char *,
                                                  void * parm,
                                                  unsigned * parmLen)
{
    if (parmLen == NULL || parm == NULL || *parmLen != sizeof(struct PluginCodec_Option **))
        return 0;

    *(const void **)parm = codec->userData;
    *parmLen = 0; //FIXME
    return 1;
}

static int free_codec_options ( const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  char ** strings = (char **) parm;
  for (char ** string = strings; *string != NULL; string++)
    free(*string);
  free(strings);
  return 1;
}

static int valid_for_protocol ( const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char *))
    return 0;

  return (STRCMPI((const char *)parm, "sip") == 0) ? 1 : 0;
}

/////////////////////////////////////////////////////////////////////////////

static void * create_encoder(const struct PluginCodec_Definition * codec)
{
  H263_Base_EncoderContext * context;

  if (codec->rtpPayload == RTP_RFC2190_PAYLOAD)
    context = new H263_RFC2190_EncoderContext();
  else
    context = new H263_RFC2429_EncoderContext();

  if (context->Open())
    return context;

  delete context;
  return NULL;
}

static void destroy_encoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  H263_Base_EncoderContext * context = (H263_Base_EncoderContext *)_context;
  delete context;
}

static int codec_encoder(const struct PluginCodec_Definition * ,
                                           void * _context,
                                     const void * from,
                                       unsigned * fromLen,
                                           void * to,
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  H263_Base_EncoderContext * context = (H263_Base_EncoderContext *)_context;
  return context->EncodeFrames((const BYTE *)from, *fromLen, (BYTE *)to, *toLen, *flag);
}

#define PMAX(a,b) ((a)>=(b)?(a):(b))
#define PMIN(a,b) ((a)<=(b)?(a):(b))

static void FindBoundingBox(const char * const * * parm,
                                             int * mpi,
                                             int & minWidth,
                                             int & minHeight,
                                             int & maxWidth,
                                             int & maxHeight,
                                             int & frameTime,
                                             int & targetBitRate,
                                             int & maxBitRate)
{
  // initialise the MPI values to disabled
  int i;
  for (i = 0; i < 5; i++)
    mpi[i] = PLUGINCODEC_MPI_DISABLED;

  // following values will be set while scanning for options
  minWidth      = INT_MAX;
  minHeight     = INT_MAX;
  maxWidth      = 0;
  maxHeight     = 0;
  int rxMinWidth    = QCIF_WIDTH;
  int rxMinHeight   = QCIF_HEIGHT;
  int rxMaxWidth    = QCIF_WIDTH;
  int rxMaxHeight   = QCIF_HEIGHT;
  int frameRate     = 10;      // 10 fps
  int origFrameTime = 900;     // 10 fps in video RTP timestamps
  int maxBR = 0;
  maxBitRate = 0;
  targetBitRate = 0;

  // extract the MPI values set in the custom options, and find the min/max of them
  frameTime = 0;

  for (const char * const * option = *parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], "MaxBR") == 0)
      maxBR = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_BIT_RATE) == 0)
      maxBitRate = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
      targetBitRate = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH) == 0)
      rxMinWidth  = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT) == 0)
      rxMinHeight = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH) == 0)
      rxMaxWidth  = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT) == 0)
      rxMaxHeight = atoi(option[1]);
    else if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
      origFrameTime = atoi(option[1]);
    else {
      for (i = 0; i < 5; i++) {
        if (STRCMPI(option[0], StandardVideoSizes[i].optionName) == 0) {
          mpi[i] = atoi(option[1]);
          if (mpi[i] != PLUGINCODEC_MPI_DISABLED) {
            int thisTime = 3003*mpi[i];
            if (minWidth > StandardVideoSizes[i].width)
              minWidth = StandardVideoSizes[i].width;
            if (minHeight > StandardVideoSizes[i].height)
              minHeight = StandardVideoSizes[i].height;
            if (maxWidth < StandardVideoSizes[i].width)
              maxWidth = StandardVideoSizes[i].width;
            if (maxHeight < StandardVideoSizes[i].height)
              maxHeight = StandardVideoSizes[i].height;
            if (thisTime > frameTime)
              frameTime = thisTime;
          }
        }
      }
    }
  }

  // if no MPIs specified, then the spec says to use QCIF
  if (frameTime == 0) {
    int ft;
    if (frameRate != 0)
      ft = 90000 / frameRate;
    else
      ft = origFrameTime;
    mpi[1] = (ft + 1502) / 3003;

#ifdef DEFAULT_TO_FULL_CAPABILITIES
    minWidth  = QCIF_WIDTH;
    maxWidth  = CIF16_WIDTH;
    minHeight = QCIF_HEIGHT;
    maxHeight = CIF16_HEIGHT;
#else
    minWidth  = maxWidth  = QCIF_WIDTH;
    minHeight = maxHeight = QCIF_HEIGHT;
#endif
  }

  // find the smallest MPI size that is larger than the min frame size
  for (i = 0; i < 5; i++) {
    if (StandardVideoSizes[i].width >= rxMinWidth && StandardVideoSizes[i].height >= rxMinHeight) {
      rxMinWidth = StandardVideoSizes[i].width;
      rxMinHeight = StandardVideoSizes[i].height;
      break;
    }
  }

  // find the largest MPI size that is smaller than the max frame size
  for (i = 4; i >= 0; i--) {
    if (StandardVideoSizes[i].width <= rxMaxWidth && StandardVideoSizes[i].height <= rxMaxHeight) {
      rxMaxWidth  = StandardVideoSizes[i].width;
      rxMaxHeight = StandardVideoSizes[i].height;
      break;
    }
  }

  // the final min/max is the smallest bounding box that will enclose both the MPI information and the min/max information
  minWidth  = PMAX(rxMinWidth, minWidth);
  maxWidth  = PMIN(rxMaxWidth, maxWidth);
  minHeight = PMAX(rxMinHeight, minHeight);
  maxHeight = PMIN(rxMaxHeight, maxHeight);

  // turn off any MPI that are outside the final bounding box
  for (i = 0; i < 5; i++) {
    if (StandardVideoSizes[i].width < minWidth ||
        StandardVideoSizes[i].width > maxWidth ||
        StandardVideoSizes[i].height < minHeight ||
        StandardVideoSizes[i].height > maxHeight)
     mpi[i] = PLUGINCODEC_MPI_DISABLED;
  }

  // find an appropriate max bit rate
  if (maxBitRate == 0) {
    if (maxBR != 0)
      maxBitRate = maxBR * 100;
    else if (targetBitRate != 0)
      maxBitRate = targetBitRate;
    else
      maxBitRate = 327000;
  }
  else if (maxBR > 0)
    maxBitRate = PMIN(maxBR * 100, maxBitRate);

  if (targetBitRate == 0)
    targetBitRate = 327000;
}

static int to_normalised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  // find bounding box enclosing all MPI values
  int mpi[5];
  int minWidth, minHeight, maxHeight, maxWidth, frameTime, targetBitRate, maxBitRate;
  FindBoundingBox((const char * const * *)parm, mpi, minWidth, minHeight, maxWidth, maxHeight, frameTime, targetBitRate, maxBitRate);

  char ** options = (char **)calloc(16+(5*2)+2, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[ 0] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH);
  options[ 1] = num2str(minWidth);
  options[ 2] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT);
  options[ 3] = num2str(minHeight);
  options[ 4] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH);
  options[ 5] = num2str(maxWidth);
  options[ 6] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT);
  options[ 7] = num2str(maxHeight);
  options[ 8] = strdup(PLUGINCODEC_OPTION_FRAME_TIME);
  options[ 9] = num2str(frameTime);
  options[10] = strdup(PLUGINCODEC_OPTION_MAX_BIT_RATE);
  options[11] = num2str(maxBitRate);
  options[12] = strdup(PLUGINCODEC_OPTION_TARGET_BIT_RATE);
  options[13] = num2str(targetBitRate);
  options[14] = strdup("MaxBR");
  options[15] = num2str((maxBitRate+50)/100);
  for (int i = 0; i < 5; i++) {
    options[16+i*2] = strdup(StandardVideoSizes[i].optionName);
    options[16+i*2+1] = num2str(mpi[i]);
  }

  return 1;
}

static int to_customised_options(const struct PluginCodec_Definition *, void *, const char *, void * parm, unsigned * parmLen)
{
  if (parmLen == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  // find bounding box enclosing all MPI values
  int mpi[5];
  int minWidth, minHeight, maxHeight, maxWidth, frameTime, targetBitRate, maxBitRate;
  FindBoundingBox((const char * const * *)parm, mpi, minWidth, minHeight, maxWidth, maxHeight, frameTime, targetBitRate, maxBitRate);

  char ** options = (char **)calloc(14+5*2+2, sizeof(char *));
  *(char ***)parm = options;
  if (options == NULL)
    return 0;

  options[ 0] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_WIDTH);
  options[ 1] = num2str(minWidth);
  options[ 2] = strdup(PLUGINCODEC_OPTION_MIN_RX_FRAME_HEIGHT);
  options[ 3] = num2str(minHeight);
  options[ 4] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_WIDTH);
  options[ 5] = num2str(maxWidth);
  options[ 6] = strdup(PLUGINCODEC_OPTION_MAX_RX_FRAME_HEIGHT);
  options[ 7] = num2str(maxHeight);
  options[ 8] = strdup(PLUGINCODEC_OPTION_MAX_BIT_RATE);
  options[ 9] = num2str(maxBitRate);
  options[10] = strdup(PLUGINCODEC_OPTION_TARGET_BIT_RATE);
  options[11] = num2str(targetBitRate);
  options[12] = strdup("MaxBR");
  options[13] = num2str((maxBitRate+50)/100);
  for (int i = 0; i < 5; i++) {
    options[14+i*2] = strdup(StandardVideoSizes[i].optionName);
    options[14+i*2+1] = num2str(mpi[i]);
  }

  return 1;
}

static int encoder_set_options(const PluginCodec_Definition *,
                               void * _context,
                               const char * ,
                               void * parm,
                               unsigned * parmLen)
{
  H263_Base_EncoderContext * context = (H263_Base_EncoderContext *)_context;
  if (parmLen == NULL || *parmLen != sizeof(const char **) || parm == NULL)
    return 0;

  context->Lock();
  context->CloseCodec();

  // get the "frame width" media format parameter to use as a hint for the encoder to start off
  for (const char * const * option = (const char * const *)parm; *option != NULL; option += 2) {
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0)
      context->SetFrameWidth (atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0)
      context->SetFrameHeight (atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_MAX_FRAME_SIZE) == 0)
      context->SetMaxRTPFrameSize (atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_TARGET_BIT_RATE) == 0)
       context->SetTargetBitrate(atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_TX_KEY_FRAME_PERIOD) == 0)
      context->SetMaxKeyFramePeriod (atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_TEMPORAL_SPATIAL_TRADE_OFF) == 0)
       context->SetTSTO (atoi(option[1]));
    if (STRCMPI(option[0], PLUGINCODEC_OPTION_FRAME_TIME) == 0)
       context->SetTargetFrameTime (atoi(option[1]));

    if (STRCMPI(option[0], "Annex D") == 0) {
      if (atoi(option[1]) == 1) {
        context->EnableAnnex (D);
	  } else {
        context->DisableAnnex (D);
	  }
	}
    if (STRCMPI(option[0], "Annex F") == 0) {
      if (atoi(option[1]) == 1) {
        context->EnableAnnex (F);
       } else {
        context->DisableAnnex (F);
	  }
	}
    if (STRCMPI(option[0], "Annex I") == 0) {
      if (atoi(option[1]) == 1) {
        context->EnableAnnex (I);
	  } else {
        context->DisableAnnex (I);
	  }
	}
    if (STRCMPI(option[0], "Annex K") == 0) {
      if (atoi(option[1]) == 1) {
        context->EnableAnnex (K);
	  } else {
        context->DisableAnnex (K);
	  }
	}
    if (STRCMPI(option[0], "Annex J") == 0) {
      if (atoi(option[1]) == 1) {
        context->EnableAnnex (J);
	  } else {
        context->DisableAnnex (J);
	  }
	}
    if (STRCMPI(option[0], "Annex S") == 0) {
      if (atoi(option[1]) == 1) {
        context->EnableAnnex (S);
	  } else {
        context->DisableAnnex (S);
	  }
	}
  }

  context->OpenCodec();
  context->Unlock();
  return 1;
}

int encoder_formats(
     const struct PluginCodec_Definition * codec,
     void * _context,
     const char *,
     void * parm,
     unsigned * parmLen)
{
  TRACE(4,"Supported Formats");
  if (_context == NULL || parm == NULL || *parmLen != sizeof(char ***))
    return 0;

  H263_Base_EncoderContext * context = (H263_Base_EncoderContext *)_context;

	char ** options = (char **)parm;
	if (options == NULL) return 0;

	for (int i = 0; options[i] != NULL; i += 2) {
       if(STRNCMPI(options[i], PLUGINCODEC_OPTION_INPUT_FORMAT, strlen(PLUGINCODEC_OPTION_INPUT_FORMAT)) == 0) {
           inputFormats f; f.w=0; f.h=0; f.r=0;
           char* token = strtok(options[i+1],",");
           int j=0;
           while (token) {
             switch (j) {
               case 0: f.w = atoi(token);
                       break;
               case 1: f.h = atoi(token);
                       break;
               case 2: f.r = atoi(token);
                       break;
             }
             token = strtok(NULL,",");
             j++;
           }
           TRACE(4,"Frame Size w " << f.w << " h " << f.h << " r " << f.r);
           context->AddInputFormat(f);
       }
    }

    unsigned maxWidth = codec->parm.video.maxFrameWidth;
    unsigned maxHeight = codec->parm.video.maxFrameHeight;
    inputFormats fmt;

      if (context->GetInputFormat(fmt, maxWidth, maxHeight)) {
        TRACE(2,"Adjusted w " << fmt.w << " h " << fmt.h << " r " << fmt.r);

	    context->Lock();
        context->CloseCodec();
	    context->SetFrameWidth(fmt.w);
	    context->SetFrameHeight(fmt.h);
        context->OpenCodec();
	    context->Unlock();

	    for (int i = 0; options[i] != NULL; i += 2) {
          // Written into context-owned buffers, not strdup()'d: nothing here
          // for a caller to free, and nothing leaked if it doesn't.
          if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_TIME) == 0) {
            snprintf(context->m_frameTimeStr, sizeof(context->m_frameTimeStr),
                     "%d", H263_CLOCKRATE/(fmt.r/2));
	         options[i+1] = context->m_frameTimeStr;
          }
          if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_HEIGHT) == 0) {
            snprintf(context->m_frameHeightStr, sizeof(context->m_frameHeightStr), "%u", fmt.h);
	         options[i+1] = context->m_frameHeightStr;
          }
          if (STRCMPI(options[i], PLUGINCODEC_OPTION_FRAME_WIDTH) == 0) {
            snprintf(context->m_frameWidthStr, sizeof(context->m_frameWidthStr), "%u", fmt.w);
	         options[i+1] = context->m_frameWidthStr;
          }
	    }
      }

  return 1;
}

static int encoder_get_output_data_size(const PluginCodec_Definition *, void *, const char *, void *, unsigned *)
{
  return 2000; //FIXME
}

/////////////////////////////////////////////////////////////////////////////

static void * create_decoder(const struct PluginCodec_Definition * codec)
{
  if (codec->rtpPayload == RTP_RFC2190_PAYLOAD)
    return new H263_RFC2190_DecoderContext();
  else
    return new H263_RFC2429_DecoderContext();
}

static void destroy_decoder(const struct PluginCodec_Definition * /*codec*/, void * _context)
{
  H263_Base_DecoderContext * context = (H263_Base_DecoderContext *)_context;
  delete context;
}

static int codec_decoder(const struct PluginCodec_Definition *,
                                           void * _context,
                                     const void * from,
                                       unsigned * fromLen,
                                           void * to,
                                       unsigned * toLen,
                                   unsigned int * flag)
{
  H263_Base_DecoderContext * context = (H263_Base_DecoderContext *)_context;
  return context->DecodeFrames((const BYTE *)from, *fromLen, (BYTE *)to, *toLen, *flag) ? 1 : 0;
}

static int decoder_get_output_data_size(const PluginCodec_Definition * codec, void *, const char *, void *, unsigned *)
{
  return sizeof(PluginCodec_Video_FrameHeader) + ((codec->parm.video.maxFrameWidth * codec->parm.video.maxFrameHeight * 3) / 2);
}

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_information licenseInfo = {
  1145863600,                                                   // timestamp =  Mon 24 Apr 2006 07:26:40 AM UTC

  "Matthias Schneider, Craig Southeren"                         // source code author
  "Guilhem Tardy, Derek Smithies",
  "1.0",                                                        // source code version
  "openh323@openh323.org",                                      // source code email
  "http://sourceforge.net/projects/openh323",                   // source code URL
  "Copyright (C) 2007 Matthias Schneider"                       // source code copyright
  ", Copyright (C) 2006 by Post Increment"
  ", Copyright (C) 2005 Salyens"
  ", Copyright (C) 2001 March Networks Corporation"
  ", Copyright (C) 1999-2000 Equivalence Pty. Ltd.",
  "MPL 1.0",                                                    // source code license
  PluginCodec_License_MPL,                                      // source code license

  "FFMPEG",                                                     // codec description
  "Michael Niedermayer, Fabrice Bellard",                       // codec author
  "",                                                           // codec version
  "ffmpeg-devel-request@mplayerhq.hu",                          // codec email
  "http://ffmpeg.mplayerhq.hu",                                 // codec URL
  "Copyright (c) 2000-2001 Fabrice Bellard"                     // codec copyright information
  ", Copyright (c) 2002-2003 Michael Niedermayer",
  "GNU LESSER GENERAL PUBLIC LICENSE, Version 2.1, February 1999", // codec license
  PluginCodec_License_LGPL                                         // codec license code
};

static const char SQCIF_MPI[]  = PLUGINCODEC_SQCIF_MPI;
static const char QCIF_MPI[]   = PLUGINCODEC_QCIF_MPI;
static const char CIF_MPI[]    = PLUGINCODEC_CIF_MPI;
static const char CIF4_MPI[]   = PLUGINCODEC_CIF4_MPI;
static const char CIF16_MPI[]  = PLUGINCODEC_CIF16_MPI;

static PluginCodec_ControlDefn EncoderControls[] = {
  { PLUGINCODEC_CONTROL_VALID_FOR_PROTOCOL,    valid_for_protocol },
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_FREE_CODEC_OPTIONS,    free_codec_options },
  { PLUGINCODEC_CONTROL_TO_NORMALISED_OPTIONS, to_normalised_options },
  { PLUGINCODEC_CONTROL_TO_CUSTOMISED_OPTIONS, to_customised_options },
  { PLUGINCODEC_CONTROL_SET_CODEC_OPTIONS,     encoder_set_options },
  { PLUGINCODEC_CONTROL_SET_FORMAT_OPTIONS,    encoder_formats },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  encoder_get_output_data_size },
  { NULL }
};

static PluginCodec_ControlDefn DecoderControls[] = {
  { PLUGINCODEC_CONTROL_GET_CODEC_OPTIONS,     get_codec_options },
  { PLUGINCODEC_CONTROL_GET_OUTPUT_DATA_SIZE,  decoder_get_output_data_size },
  { NULL }
};

static struct PluginCodec_Option const sqcifMPI =
{
  PluginCodec_IntegerOption,            // Option type
  SQCIF_MPI,                            // User visible name
  false,                                // User Read/Only flag
  PluginCodec_MaxMerge,                 // Merge mode
  "1",                                  // Initial value
  "SQCIF",                              // FMTP option name
  STRINGIZE(PLUGINCODEC_MPI_DISABLED),  // FMTP default value
  0,                                    // H.245 generic capability code and bit mask
  "1",                                  // Minimum value
  STRINGIZE(PLUGINCODEC_MPI_DISABLED)   // Maximum value
};

static struct PluginCodec_Option const qcifMPI =
{
  PluginCodec_IntegerOption,            // Option type
  QCIF_MPI,                             // User visible name
  false,                                // User Read/Only flag
  PluginCodec_MaxMerge,                 // Merge mode
  "1",                                  // Initial value
  "QCIF",                               // FMTP option name
  STRINGIZE(PLUGINCODEC_MPI_DISABLED),  // FMTP default value
  0,                                    // H.245 generic capability code and bit mask
  "1",                                  // Minimum value
  STRINGIZE(PLUGINCODEC_MPI_DISABLED)   // Maximum value
};

static struct PluginCodec_Option const cifMPI =
{
  PluginCodec_IntegerOption,            // Option type
  CIF_MPI,                              // User visible name
  false,                                // User Read/Only flag
  PluginCodec_MaxMerge,                 // Merge mode
  "1",                                  // Initial value
  "CIF",                                // FMTP option name
  STRINGIZE(PLUGINCODEC_MPI_DISABLED),  // FMTP default value
  0,                                    // H.245 generic capability code and bit mask
  "1",                                  // Minimum value
  STRINGIZE(PLUGINCODEC_MPI_DISABLED)   // Maximum value
};

static struct PluginCodec_Option const cif4MPI =
{
  PluginCodec_IntegerOption,            // Option type
  CIF4_MPI,                             // User visible name
  false,                                // User Read/Only flag
  PluginCodec_MaxMerge,                 // Merge mode
  "1",                                  // Initial value
  "CIF4",                               // FMTP option name
  STRINGIZE(PLUGINCODEC_MPI_DISABLED),  // FMTP default value
  0,                                    // H.245 generic capability code and bit mask
  "1",                                  // Minimum value
  STRINGIZE(PLUGINCODEC_MPI_DISABLED)   // Maximum value
};

static struct PluginCodec_Option const cif16MPI =
{
  PluginCodec_IntegerOption,            // Option type
  CIF16_MPI,                            // User visible name
  false,                                // User Read/Only flag
  PluginCodec_MaxMerge,                 // Merge mode
  "1",                                  // Initial value
  "CIF16",                              // FMTP option name
  STRINGIZE(PLUGINCODEC_MPI_DISABLED),  // FMTP default value
  0,                                    // H.245 generic capability code and bit mask
  "1",                                  // Minimum value
  STRINGIZE(PLUGINCODEC_MPI_DISABLED)   // Maximum value
};

static struct PluginCodec_Option const maxBR =
{
  PluginCodec_IntegerOption,          // Option type
  "MaxBR",                            // User visible name
  false,                              // User Read/Only flag
  PluginCodec_MinMerge,               // Merge mode
  "0",                                // Initial value
  "maxbr",                            // FMTP option name
  "0",                                // FMTP default value
  0,                                  // H.245 generic capability code and bit mask
  "0",                                // Minimum value
  "32767"                             // Maximum value
};

static struct PluginCodec_Option const mediaPacketization =
{
  PluginCodec_StringOption,           // Option type
  PLUGINCODEC_MEDIA_PACKETIZATION,    // User visible name
  true,                               // User Read/Only flag
  PluginCodec_EqualMerge,             // Merge mode
  "RFC2190"                           // Initial value
};

static struct PluginCodec_Option const sifMPI =
  { PluginCodec_StringOption, "SIF MPI", false, PluginCodec_EqualMerge, "320,240,1", "CUSTOM"};

static struct PluginCodec_Option const sif4MPI =
  { PluginCodec_StringOption, "SIF4 MPI", false, PluginCodec_EqualMerge, "640,480,1", "CUSTOM"};

static struct PluginCodec_Option const annexF =
  { PluginCodec_BoolOption,    "Annex F",   false,  PluginCodec_MinMerge, "1", "F", "0" };

static struct PluginCodec_Option const annexI =
  { PluginCodec_BoolOption,    "Annex I",   false,  PluginCodec_MinMerge, "1", "I", "0" };

static struct PluginCodec_Option const annexJ =
  { PluginCodec_BoolOption,    "Annex J",   true,  PluginCodec_MinMerge, "1", "J", "0" };

static struct PluginCodec_Option const annexK =
  { PluginCodec_IntegerOption, "Annex K",   true,  PluginCodec_EqualMerge, "0", "K", "0", 0, "0", "4" };

static struct PluginCodec_Option const annexN =
  { PluginCodec_BoolOption,    "Annex N",   true,  PluginCodec_AndMerge, "0", "N", "0" };

static struct PluginCodec_Option const annexP =
  { PluginCodec_BoolOption,    "Annex P",   true,  PluginCodec_AndMerge, "0", "P", "0" };

static struct PluginCodec_Option const annexT =
  { PluginCodec_BoolOption,    "Annex T",   true,  PluginCodec_AndMerge, "0", "T", "0" };

/* A local, non signalled option: it controls how far the encoder may back
   off on quality to hit the negotiated bit rate.  Without it in the tables
   the plugin manager never calls SetTSTO() and the default applies. */
static struct PluginCodec_Option const temporalSpatialTradeOff =
  { PluginCodec_IntegerOption, PLUGINCODEC_OPTION_TEMPORAL_SPATIAL_TRADE_OFF, false, PluginCodec_NoMerge,
    STRINGIZE(H263_DEFAULT_TSTO), NULL, NULL, 0, "0", "31" };

static struct PluginCodec_Option const annexD =
  { PluginCodec_BoolOption,    "Annex D",   true,  PluginCodec_MinMerge, "1", "D", "0" };

static struct PluginCodec_Option const * const h263POptionTable[] = {
  &temporalSpatialTradeOff,
  &qcifMPI,
  &cifMPI,
  &sqcifMPI,
  &cif4MPI,
  &cif16MPI,
  &sifMPI,
  &sif4MPI,
  &annexF,
  &annexI,
  &annexJ,
  &annexK,
  &annexN,
  &annexP,
  &annexT,
  &annexD,
  NULL
};


static struct PluginCodec_Option const * const h263OptionTable[] = {
  &temporalSpatialTradeOff,
  &mediaPacketization,
  &maxBR,
  //&videoQuality,
  //&minVideoQuality,
  //&maxVideoQuality,
  &qcifMPI,
  &cifMPI,
  &sqcifMPI,
  &cif4MPI,
  &cif16MPI,
  &annexF,
  NULL
};

static struct PluginCodec_Option const * const h263QCIFOptionTable[] = {
  &temporalSpatialTradeOff,
  &mediaPacketization,
  &maxBR,
  &qcifMPI,
  NULL
};

static struct PluginCodec_Option const * const h263CIFOptionTable[] = {
  &temporalSpatialTradeOff,
  &mediaPacketization,
  &maxBR,
  &cifMPI,
  NULL
};

static struct PluginCodec_Option const * const h263CIF4OptionTable[] = {
  &temporalSpatialTradeOff,
  &mediaPacketization,
  &maxBR,
  &cif4MPI,
  NULL
};

/////////////////////////////////////////////////////////////////////////////

static struct PluginCodec_Definition h263CodecDefn[] = {
  {
    // QCIF only encoder
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeExplicit,        // specified RTP type

    h263QCIFDesc,                       // text decription
    YUV420PDesc,                        // source format
    h263QCIFDesc,                       // destination format

    h263QCIFOptionTable,                // user data

    H263_CLOCKRATE,                     // samples per second
    H263_BITRATE,                       // raw bits per second
    20000,                              // nanoseconds per frame

    {{
      QCIF_WIDTH,                         // frame width
      QCIF_HEIGHT,                        // frame height
      10,                                 // recommended frame rate
      60,                                 // maximum frame rate
    }},

    RTP_RFC2190_PAYLOAD,                // IANA RTP payload code
    sdpH263,                            // RTP payload name

    create_encoder,                     // create codec function
    destroy_encoder,                    // destroy codec
    codec_encoder,                      // encode/decode
    EncoderControls,                // codec controls

    PluginCodec_H323VideoCodec_h263,    // h323CapabilityType
    NULL                                // h323CapabilityData
 },
 {
    // QCIF only decoder
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // audio codec
    PluginCodec_RTPTypeExplicit,        // specified RTP type

    h263QCIFDesc,                       // text decription
    h263QCIFDesc,                       // source format
    YUV420PDesc,                        // destination format

    h263QCIFOptionTable,                // user data

    H263_CLOCKRATE,                     // samples per second
    H263_BITRATE,                       // raw bits per second
    20000,                              // nanoseconds per frame

    {{
      QCIF_WIDTH,                         // frame width
      QCIF_HEIGHT,                        // frame height
      10,                                 // recommended frame rate
      60,                                 // maximum frame rate
    }},

    RTP_RFC2190_PAYLOAD,                // IANA RTP payload code
    sdpH263,                            // RTP payload name

    create_decoder,                     // create codec function
    destroy_decoder,                    // destroy codec
    codec_decoder,                      // encode/decode
    DecoderControls,                    // codec controls

    PluginCodec_H323VideoCodec_h263,    // h323CapabilityType
    NULL                                // h323CapabilityData
 },

 {
    // CIF only encoder
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // video codec
    PluginCodec_RTPTypeExplicit,        // specified RTP type

    h263CIFDesc,                        // text decription
    YUV420PDesc,                        // source format
    h263CIFDesc,                        // destination format

    h263CIFOptionTable,                 // user data

    H263_CLOCKRATE,                     // samples per second
    H263_BITRATE,                       // raw bits per second
    20000,                              // nanoseconds per frame

    {{
      CIF_WIDTH,                        // frame width
      CIF_HEIGHT,                       // frame height
      10,                               // recommended frame rate
      60,                               // maximum frame rate
    }},

    RTP_RFC2190_PAYLOAD,                // IANA RTP payload code
    sdpH263,                            // RTP payload name

    create_encoder,                     // create codec function
    destroy_encoder,                    // destroy codec
    codec_encoder,                      // encode/decode
    EncoderControls,					// codec controls

    PluginCodec_H323VideoCodec_h263,    // h323CapabilityType
    NULL                                // h323CapabilityData
  },
  {
    // CIF only decoder
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // video codec
    PluginCodec_RTPTypeExplicit,        // specified RTP type

    h263CIFDesc,                        // text decription
    h263CIFDesc,                        // source format
    YUV420PDesc,                        // destination format

    h263CIFOptionTable,                     // user data

    H263_CLOCKRATE,                     // samples per second
    H263_BITRATE,                       // raw bits per second
    20000,                              // nanoseconds per frame

    {{
      CIF_WIDTH,                          // frame width
      CIF_HEIGHT,                         // frame height
      10,                                 // recommended frame rate
      60,                                 // maximum frame rate
    }},

    RTP_RFC2190_PAYLOAD,                // IANA RTP payload code
    sdpH263,                            // RTP payload name

    create_decoder,                     // create codec function
    destroy_decoder,                    // destroy codec
    codec_decoder,                      // encode/decode
    DecoderControls,					// codec controls

    PluginCodec_H323VideoCodec_h263,    // h323CapabilityType
    NULL                                // h323CapabilityData
  },

  {
    // 720p encoder
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // video codec
    PluginCodec_MediaTypeExtVideo |     // Extended video codec
    PluginCodec_RTPTypeExplicit,        // specified RTP type

    h263720Desc,                        // text decription
    YUV420PDesc,                        // source format
    h263720Desc,                        // destination format

    h263CIF4OptionTable,                // user data

    H263_CLOCKRATE,                     // samples per second
    H263_CIF4_BITRATE,                  // raw bits per second
    20000,                              // nanoseconds per frame

    {{
      CIF4_WIDTH,                       // frame width
      CIF4_HEIGHT,                      // frame height
      10,                               // recommended frame rate
      60,                               // maximum frame rate
    }},

    RTP_RFC2190_PAYLOAD,                // IANA RTP payload code
    sdpH263,                            // RTP payload name

    create_encoder,                     // create codec function
    destroy_encoder,                    // destroy codec
    codec_encoder,                      // encode/decode
    EncoderControls,                    // codec controls

    PluginCodec_H323VideoCodec_h263,    // h323CapabilityType
    NULL                                // h323CapabilityData
  },
  {
    // 720p decoder
    PLUGIN_CODEC_VERSION_OPTIONS,       // codec API version
    &licenseInfo,                       // license information

    PluginCodec_MediaTypeVideo |        // video codec
    PluginCodec_MediaTypeExtVideo |     // Extended video codec
    PluginCodec_RTPTypeExplicit,        // specified RTP type

    h263720Desc,                        // text decription
    h263720Desc,                        // source format
    YUV420PDesc,                        // destination format

    h263CIF4OptionTable,                // user data

    H263_CLOCKRATE,                     // samples per second
    H263_CIF4_BITRATE,                  // raw bits per second
    20000,                              // nanoseconds per frame

    {{
      CIF4_WIDTH,                         // frame width
      CIF4_HEIGHT,                        // frame height
      10,                                 // recommended frame rate
      60,                                 // maximum frame rate
    }},

    RTP_RFC2190_PAYLOAD,                // IANA RTP payload code
    sdpH263,                            // RTP payload name

    create_decoder,                     // create codec function
    destroy_decoder,                    // destroy codec
    codec_decoder,                      // encode/decode
    DecoderControls,                    // codec controls

    PluginCodec_H323VideoCodec_h263,    // h323CapabilityType
    NULL                                // h323CapabilityData
  }
};

/////////////////////////////////////////////////////////////////////////////

extern "C" {
  PLUGIN_CODEC_IMPLEMENT(FFMPEG_H263P)

  PLUGIN_CODEC_DLL_API struct PluginCodec_Definition * PLUGIN_CODEC_GET_CODEC_FN(unsigned * count, unsigned version)
  {
    char * debug_level = getenv ("PTLIB_TRACE_CODECS");
    if (debug_level!=NULL) {
      Trace::SetLevel(atoi(debug_level));
    }
    else {
      Trace::SetLevel(0);
    }

    debug_level = getenv ("PTLIB_TRACE_CODECS_USER_PLANE");
    if (debug_level!=NULL) {
      Trace::SetLevelUserPlane(atoi(debug_level));
    }
    else {
      Trace::SetLevelUserPlane(0);
    }

  if (avcodec_find_encoder(AV_CODEC_ID_H263)  == NULL ||
      avcodec_find_encoder(AV_CODEC_ID_H263P) == NULL ||
      avcodec_find_decoder(AV_CODEC_ID_H263)  == NULL) {
    *count = 0;
    TRACE(1, "H.263\tCodec\tDisabled - libavcodec has no H.263 encoder/decoder");
    return NULL;
  }

  av_log_set_level(AV_LOG_DEBUG);
  av_log_set_callback(&logCallbackFFMPEG);

    if (version < PLUGIN_CODEC_VERSION_OPTIONS) {
      *count = 0;
      TRACE(1, "H.263\tCodec\tDisabled - plugin version mismatch");
      return NULL;
    }
    else {
      *count = sizeof(h263CodecDefn) / sizeof(struct PluginCodec_Definition);
      TRACE(1, "H.263\tCodec\tEnabled with " << *count << " definitions");
      return h263CodecDefn;
    }
  }

};
