/*
 * ffmpeg_compat.h
 *
 * Thin compatibility layer over the modern (FFmpeg 4.0+) libavcodec API.
 *
 * The old plugin code reached libavcodec through common/dyna.cxx, which
 * dlopen()s the library and resolves symbols such as avcodec_init(),
 * register_avcodec() and the h263_encoder AVCodec object by name.  None of
 * those exist any more, so codecs that want to build against a current
 * FFmpeg link against libavcodec directly and include this header instead
 * of ../common/dyna.h.
 *
 * Copyright (C) 2026 Jan Willamowius
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
 */

#ifndef __FFMPEG_COMPAT_H__
#define __FFMPEG_COMPAT_H__ 1

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <stdint.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
};

#ifndef LIBAVCODEC_VERSION_INT
  #error "libavcodec headers not found or not usable"
#endif

/* avcodec_send_frame()/avcodec_receive_packet(), av_packet_alloc() and the
   "mb_info" private option of the H.263 encoder are all available from
   libavcodec 58 (FFmpeg 4.0) onwards.  Older releases are long out of
   support and needed a completely different code path, so refuse them. */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 18, 100)
  #error "libavcodec is too old - FFmpeg 4.0 or later is required"
#endif

/* avcodec_find_encoder()/avcodec_find_decoder() return a pointer to const
   from libavcodec 59 (FFmpeg 5.0). */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 0, 100)
  typedef const AVCodec FFMPEG_AVCodec;
#else
  typedef AVCodec FFMPEG_AVCodec;
#endif

/* Set a private (codec specific) AVOption, ignoring the error when the
   option is not known to this particular encoder. */
static inline int FFMPEGSetPrivateOption(AVCodecContext * ctx, const char * name, int64_t value)
{
  if (ctx == NULL || ctx->priv_data == NULL)
    return AVERROR(EINVAL);
  return av_opt_set_int(ctx->priv_data, name, value, 0);
}

static inline int FFMPEGSetPrivateOption(AVCodecContext * ctx, const char * name, const char * value)
{
  if (ctx == NULL || ctx->priv_data == NULL)
    return AVERROR(EINVAL);
  return av_opt_set(ctx->priv_data, name, value, 0);
}

static inline int FFMPEGSetPrivateOptionDouble(AVCodecContext * ctx, const char * name, double value)
{
  if (ctx == NULL || ctx->priv_data == NULL)
    return AVERROR(EINVAL);
  return av_opt_set_double(ctx->priv_data, name, value, 0);
}

#endif /* __FFMPEG_COMPAT_H__ */
