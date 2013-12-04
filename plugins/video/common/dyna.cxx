/*
 * Common Plugin code for OpenH323/OPAL
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
 * Contributor(s): Guilhem Tardy (gtardy@salyens.com)
 *                 Craig Southeren (craigs@postincrement.com)
 *                 Matthias Schneider (ma30002000@yahoo.de)
 */

#ifndef _STATIC_LINK
#define USE_DLL_AVCODEC 1
#endif

#include "dyna.h"


#ifdef _MSC_VER
#define snprintf _snprintf
#include <string>
#endif

#ifndef PATH_SEP
#ifdef _WIN32
#pragma pack(16)
#define	PATH_SEP  ";"
#else
#define	PATH_SEP  ":"
#endif
#endif


bool DynaLink::Open(const char *name)
{
#ifdef _WIN32
  // first we try the DLL path
  std::string dllPath = Trace::GetDLLPath();
  if (InternalOpen(dllPath.c_str(), name))
    return true;
#endif

  // Then we try without a path
  if (InternalOpen("", name))
    return true;

  // try directories specified in PTLIBPLUGINDIR
  char ptlibPath[1024];
  memset(ptlibPath, 0, sizeof(ptlibPath));
  char * env = ::getenv("PTLIBPLUGINDIR");
  if (env != NULL) 
    strcpy(ptlibPath, env);

  char * p = ::strtok(ptlibPath, PATH_SEP);
  while (p != NULL) {
    if (InternalOpen(p, name))
      return true;
    p = ::strtok(NULL, PATH_SEP);
  }

  // As a last resort, try the current directory
  if (InternalOpen(".", name))
    return true;

  return InternalOpen("/usr/local/lib", name);
}

bool DynaLink::InternalOpen(const char * dir, const char *name)
{
  char path[1024];
  memset(path, 0, sizeof(path));

  // Copy the directory to "path" and add a separator if necessary
  if (strlen(dir) > 0) {
    strcpy(path, dir);
    if (path[strlen(path)-1] != DIR_SEPARATOR[0]) 
      strcat(path, DIR_SEPARATOR);
  }
  strcat(path, name);

  if (strlen(path) == 0) {
    TRACE(1, _codecString << "\tDYNA\tdir '" << (dir != NULL ? dir : "(NULL)") << "', name '" << (name != NULL ? name : "(NULL)") << "' resulted in empty path");
    return false;
  }
TRACE(1, "\tDYNA\tLoading " << path);
#ifndef _WIN32
  strcat(path, ".so");
#endif

  // Load the Libary
#ifdef _WIN32
# ifdef UNICODE
  WITH_ALIGNED_STACK({  // must be called before using avcodec lib
     USES_CONVERSION;
    _hDLL = LoadLibrary(A2T(path));
  });
# else
 // WITH_ALIGNED_STACK({  // must be called before using avcodec lib
    _hDLL = LoadLibrary(path);
 // });
# endif /* UNICODE */
#else
  WITH_ALIGNED_STACK({  // must be called before using avcodec lib
    _hDLL = dlopen((const char *)path, RTLD_NOW);
  });
#endif /* _WIN32 */

  // Check for errors
  if (_hDLL == NULL) {
#ifndef _WIN32
    const char * err = dlerror();
    if (err != NULL) {
      TRACE(1, _codecString << "\tDYNA\tError loading " << path << " - " << err)
    }  
    else {
      TRACE(1, _codecString << "\tDYNA\tError loading " << path);
    }
#else /* _WIN32 */
    TRACE(1, _codecString << "\tDYNA\tError loading '" << path << "' " << GetLastError());
#endif /* _WIN32 */
    return false;
  } 

  TRACE(1, _codecString << "\tDYNA\tSuccessfully loaded '" << path << "'");
  return true;
}

void DynaLink::Close()
{
  if (_hDLL != NULL) {
#ifdef _WIN32
    FreeLibrary(_hDLL);
#else
    dlclose(_hDLL);
#endif /* _WIN32 */
    _hDLL = NULL;
  }
}

bool DynaLink::GetFunction(const char * name, Function & func)
{
  if (_hDLL == NULL)
    return false;
#ifdef _WIN32

# ifdef UNICODE
  USES_CONVERSION;
  FARPROC p = GetProcAddress(_hDLL, A2T(name));
# else
  FARPROC p = GetProcAddress(_hDLL, name);
# endif /* UNICODE */
  if (p == NULL)
    return false;

  func = (Function)p;
  return true;
#else
  void * p = dlsym(_hDLL, (const char *)name);
  if (p == NULL) {
    TRACE(1, _codecString << "\tDYNA\tError " << dlerror());
    return false;
  }
  func = (Function &)p;
  return true;
#endif /* _WIN32 */
}

FFMPEGLibrary::FFMPEGLibrary(FF_CodecID codec)
{
  _codec = codec;
  if (_codec==CODEC_ID_H264)
      snprintf( _codecString, sizeof(_codecString), "H264");
  if (_codec==CODEC_ID_H263)
      snprintf( _codecString, sizeof(_codecString), "H263");
  if (_codec==CODEC_ID_H263P)
      snprintf( _codecString, sizeof(_codecString), "H263+");
  if (_codec==CODEC_ID_MPEG4)
      snprintf( _codecString, sizeof(_codecString), "MPEG4");
  isLoadedOK = false;
}

FFMPEGLibrary::~FFMPEGLibrary()
{
#ifdef USE_DLL_AVCODEC
  libAvcodec.Close();
  libAvutil.Close();
#endif
}

#ifdef USE_DLL_AVCODEC
#define CHECK_AVUTIL(name, func) \
      (seperateLibAvutil ? \
        libAvutil.GetFunction(name,  (DynaLink::Function &)func) : \
        libAvcodec.GetFunction(name, (DynaLink::Function &)func) \
       )
#endif

bool FFMPEGLibrary::Load(int ver)
{
  WaitAndSignal m(processLock);      
  if (IsLoaded())
    return true;
#ifdef USE_DLL_AVCODEC
  bool seperateLibAvutil = false;

  if (libAvcodec.Open("avcodec-52") || libAvcodec.Open("avcodec-51"))
    seperateLibAvutil = true;
  else if (libAvcodec.Open("libavcodec"))
    seperateLibAvutil = false;
  else {
    TRACE (1, _codecString << "\tDYNA\tFailed to load FFMPEG libavcodec library");
    return false;
  }

  if (seperateLibAvutil && !(libAvutil.Open("avutil-50") || libAvutil.Open("avutil-49")) ) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load FFMPEG libavutil library");
    return false;
  }

  strcpy(libAvcodec._codecString, _codecString);
  strcpy(libAvutil._codecString,  _codecString);

  if (!libAvcodec.GetFunction("avcodec_init", (DynaLink::Function &)Favcodec_init)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_init");
    return false;
  }

  if (_codec==CODEC_ID_H264) {
    if (!libAvcodec.GetFunction("h264_decoder", (DynaLink::Function &)Favcodec_h264_decoder)) {
      TRACE (1, _codecString << "\tDYNA\tFailed to load h264_decoder");
      return false;
    }
  }

  if (_codec==CODEC_ID_H263) {
    if (!libAvcodec.GetFunction("h263_encoder", (DynaLink::Function &)Favcodec_h263_encoder)) {
      TRACE (1, _codecString << "\tDYNA\tFailed to load h263_encoder" );
      return false;
    }

    if (!libAvcodec.GetFunction("h263_decoder", (DynaLink::Function &)Favcodec_h263_decoder)) {
      TRACE (1, _codecString << "\tDYNA\tFailed to load h263_decoder" );
      return false;
    }
  }
  
  if (_codec==CODEC_ID_H263P) {
    if (!libAvcodec.GetFunction("h263_encoder", (DynaLink::Function &)Favcodec_h263_encoder)) {
      TRACE (1, _codecString << "\tDYNA\tFailed to load h263_encoder" );
      return false;
    }
  
    if (!libAvcodec.GetFunction("h263p_encoder", (DynaLink::Function &)Favcodec_h263p_encoder)) {
      TRACE (1, _codecString << "\tDYNA\tFailed to load h263p_encoder" );
      return false;
    }

    if (!libAvcodec.GetFunction("h263_decoder", (DynaLink::Function &)Favcodec_h263_decoder)) {
      TRACE (1, _codecString << "\tDYNA\tFailed to load h263_decoder" );
      return false;
    }
  }

  if (_codec==CODEC_ID_MPEG4) {
    if (!libAvcodec.GetFunction("mpeg4_encoder", (DynaLink::Function &)mpeg4_encoder)) {
      TRACE (1, _codecString << "\tDYNA\tFailed to load mpeg4_encoder");
      return false;
    }

    if (!libAvcodec.GetFunction("mpeg4_decoder", (DynaLink::Function &)mpeg4_decoder)) {
      TRACE (1, _codecString << "\tDYNA\tFailed to load mpeg4_decoder");
      return false;
    }
  }

  if (!libAvcodec.GetFunction("register_avcodec", (DynaLink::Function &)Favcodec_register)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load register_avcodec");
    return false;
  }
  
  if (!libAvcodec.GetFunction("avcodec_find_encoder", (DynaLink::Function &)Favcodec_find_encoder)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_find_encoder");
    return false;
  }

  if (!libAvcodec.GetFunction("avcodec_find_decoder", (DynaLink::Function &)Favcodec_find_decoder)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_find_decoder");
    return false;
  }

#if LIBAVCODEC_VERSION_MAJOR < 55
  if (!libAvcodec.GetFunction("avcodec_alloc_context", (DynaLink::Function &)Favcodec_alloc_context)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_alloc_context");
    return false;
  }
#else
  if (!libAvcodec.GetFunction("avcodec_alloc_context3", (DynaLink::Function &)Favcodec_alloc_context)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_alloc_context3");
    return false;
  }
#endif

  if (!libAvcodec.GetFunction("avcodec_alloc_frame", (DynaLink::Function &)Favcodec_alloc_frame)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_alloc_frame");
    return false;
  }

#if LIBAVCODEC_VERSION_MAJOR < 55
  if (!libAvcodec.GetFunction("avcodec_open", (DynaLink::Function &)Favcodec_open)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_open");
    return false;
  }
#else
  if (!libAvcodec.GetFunction("avcodec_open2", (DynaLink::Function &)Favcodec_open)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_open2");
    return false;
  }
#endif

  if (!libAvcodec.GetFunction("avcodec_close", (DynaLink::Function &)Favcodec_close)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_close");
    return false;
  }

#if LIBAVCODEC_VERSION_MAJOR < 55
  if (!libAvcodec.GetFunction("avcodec_encode_video", (DynaLink::Function &)Favcodec_encode_video)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_encode_video" );
    return false;
  }
#else
  if (!libAvcodec.GetFunction("avcodec_encode_video2", (DynaLink::Function &)Favcodec_encode_video)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_encode_video2" );
    return false;
  }
#endif

#if LIBAVCODEC_VERSION_MAJOR < 53
  if (!libAvcodec.GetFunction("avcodec_decode_video", (DynaLink::Function &)Favcodec_decode_video)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_decode_video");
    return false;
  }
#else
  if (!libAvcodec.GetFunction("av_init_packet", (DynaLink::Function &)Fav_init_packet)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load av_init_packet");
    return false;
  }

  if (!libAvcodec.GetFunction("avcodec_decode_video2", (DynaLink::Function &)Favcodec_decode_video)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_decode_video2");
    return false;
  }
#endif

  Favcodec_set_dimensions = NULL;
  if (ver > 0) {
    if (!libAvcodec.GetFunction("avcodec_set_dimensions", (DynaLink::Function &)Favcodec_set_dimensions)) {
      TRACE (1, _codecString << "\tDYNA\tFailed to load avcodec_set_dimensions");
      return false;
    }
  }

  if (!CHECK_AVUTIL("av_malloc", Favcodec_malloc)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load av_malloc");
    return false;
  }

  if (!CHECK_AVUTIL("av_free", Favcodec_free)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load av_free");
    return false;
  }

  if (!libAvcodec.GetFunction("ff_check_alignment", (DynaLink::Function &) Fff_check_alignment)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load ff_check_alignment - alignment checks will be skipped");
    Fff_check_alignment = NULL;
  }

  if(!libAvcodec.GetFunction("avcodec_version", (DynaLink::Function &)Favcodec_version)){
    TRACE (1, _codecString << "DYYNA\tFailed to load avcodec_version");
    return false;
  }
  
  if (!CHECK_AVUTIL("av_log_set_level", FAv_log_set_level)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load av_log_set_level");
    return false;
  }

  if (!CHECK_AVUTIL("av_log_set_callback", FAv_log_set_callback)) {
    TRACE (1, _codecString << "\tDYNA\tFailed to load av_log_set_callback");
    return false;
  }

  WITH_ALIGNED_STACK({  // must be called before using avcodec lib

    unsigned libVer = Favcodec_version();
    if (libVer != LIBAVCODEC_VERSION_INT ) {
      TRACE (1, _codecString << "\tDYNA\tWarning: compiled against libavcodec headers from version "
             << (LIBAVCODEC_VERSION_INT >> 16) << ((LIBAVCODEC_VERSION_INT>>8) & 0xff) << (LIBAVCODEC_VERSION_INT & 0xff)
             << ", loaded " 
             << (libVer >> 16) << ((libVer>>8) & 0xff) << (libVer & 0xff));
    }

    Favcodec_init();

    // register only the codecs needed (to have smaller code)
    if (_codec==CODEC_ID_H264) 
      Favcodec_register(Favcodec_h264_decoder);

    if (_codec==CODEC_ID_H263) {
      Favcodec_register(Favcodec_h263_encoder);
      Favcodec_register(Favcodec_h263_decoder);
    }

    if (_codec==CODEC_ID_H263P) {
      Favcodec_register(Favcodec_h263_encoder);
      Favcodec_register(Favcodec_h263p_encoder);
      Favcodec_register(Favcodec_h263_decoder);
    }

    if (_codec==CODEC_ID_MPEG4) {
      Favcodec_register(mpeg4_encoder);
      Favcodec_register(mpeg4_decoder);
    }

    if (FFCheckAlignment() != 0) {
      TRACE(1, _codecString << "\tDYNA\tff_check_alignment() reports failure - stack alignment is not correct");
    }

  });

#else

    avcodec_register_all();

#endif

  isLoadedOK = true;
  TRACE (4, _codecString << "\tDYNA\tSuccessfully loaded libavcodec library and verified functions");

  return true;
}

AVCodec *FFMPEGLibrary::AvcodecFindEncoder(enum FF_CodecID id)
{

#ifdef USE_DLL_AVCODEC
  WITH_ALIGNED_STACK({
    AVCodec *res = Favcodec_find_encoder(id);
    return res;
  });
#else
    AVCodec *res = avcodec_find_encoder(id);
    return res;
#endif
}

AVCodec *FFMPEGLibrary::AvcodecFindDecoder(enum FF_CodecID id)
{

  WaitAndSignal m(processLock);

#ifdef USE_DLL_AVCODEC
  WITH_ALIGNED_STACK({
    AVCodec *res = Favcodec_find_decoder(id);
    return res;
  });
#else
    AVCodec *res = avcodec_find_decoder(id);
    return res;
#endif
}

AVCodecContext *FFMPEGLibrary::AvcodecAllocContext(AVCodec * codec)
{

  WaitAndSignal m(processLock);

#ifdef USE_DLL_AVCODEC
    #if LIBAVCODEC_VERSION_MAJOR < 55
      WITH_ALIGNED_STACK({
        AVCodecContext *res = Favcodec_alloc_context();
        return res;
      });
    #else
      WITH_ALIGNED_STACK({
        AVCodecContext *res = Favcodec_alloc_context(codec);
        return res;
      });
    #endif
#else
    #if LIBAVCODEC_VERSION_MAJOR < 55
        AVCodecContext *res = avcodec_alloc_context();
    #else
        AVCodecContext *res = avcodec_alloc_context3(codec);
    #endif
    return res;
#endif
}

AVFrame *FFMPEGLibrary::AvcodecAllocFrame(void)
{
  WaitAndSignal m(processLock);

#ifdef USE_DLL_AVCODEC
  WITH_ALIGNED_STACK({
    AVFrame *res = Favcodec_alloc_frame();
    return res;
 });
#else
    AVFrame *res = avcodec_alloc_frame();
    return res;
#endif

}

int FFMPEGLibrary::AvcodecOpen(AVCodecContext *ctx, AVCodec *codec)
{
  WaitAndSignal m(processLock);

#ifdef USE_DLL_AVCODEC
    #if LIBAVCODEC_VERSION_MAJOR < 55
      WITH_ALIGNED_STACK({
        return Favcodec_open(ctx, codec);
      });
    #else
      WITH_ALIGNED_STACK({
       return Favcodec_open(ctx, codec, NULL);
      });
    #endif
#else
#if LIBAVCODEC_VERSION_MAJOR < 55
   return avcodec_open(ctx, codec);
#else
   return avcodec_open2(ctx, codec, NULL);
#endif
#endif
}

int FFMPEGLibrary::AvcodecClose(AVCodecContext *ctx)
{
  WaitAndSignal m(processLock);

#ifdef USE_DLL_AVCODEC
  WITH_ALIGNED_STACK({
    return Favcodec_close(ctx);
  });
#else
  return avcodec_close(ctx);
#endif
}

int FFMPEGLibrary::AvcodecEncodeVideo(AVCodecContext *ctx, BYTE *buf, int buf_size, const AVFrame *pict)
{
#if LIBAVCODEC_VERSION_MAJOR < 55
    #ifdef USE_DLL_AVCODEC
      WITH_ALIGNED_STACK({
        int res = Favcodec_encode_video(ctx, buf, buf_size, pict);
        TRACE_UP(4, _codecString << "\tDYNA\tEncoded " << buf_size << " bytes of YUV420P data into " << res << " bytes");
        return res;
      });
    #else
       int res = avcodec_encode_video(ctx, buf, buf_size, pict);
       return res;
    #endif
#else
     AVPacket pkt; 
     int err = 0;
     int gotFrame = 0;
#ifdef USE_DLL_AVCODEC
      WITH_ALIGNED_STACK({
        Fav_init_packet(&pkt);
      });
      WITH_ALIGNED_STACK({
        err = Favcodec_encode_video(ctx, &pkt, pict, &gotFrame);
      });
#else
      av_init_packet(&pkt);
      err = avcodec_encode_video2(ctx, &pkt, pict, &gotFrame);
#endif

   if (err || !gotFrame)
       return 0;

    memmove(buf, pkt.data, pkt.size);
    TRACE_UP(4, _codecString << "\tDYNA\tEncoded " << buf_size << " bytes of YUV420P data into " << pkt.size << " bytes");
    return pkt.size;

#endif

}

int FFMPEGLibrary::AvcodecDecodeVideo(AVCodecContext *ctx, AVFrame *pict, int *got_picture_ptr, BYTE *buf, int buf_size)
{
int res=0;
#if LIBAVCODEC_VERSION_MAJOR < 53
    #ifdef USE_DLL_AVCODEC
      WITH_ALIGNED_STACK({
        res = Favcodec_decode_video(ctx, pict, got_picture_ptr, buf, buf_size);
        TRACE_UP(4, _codecString << "\tDYNA\tDecoded video of " << res << " bytes, got_picture=" << *got_picture_ptr);
      });
    #else
       res = avcodec_decode_video(ctx, pict, got_picture_ptr, buf, buf_size);
    #endif
#else
     AVPacket pkt; 
     pkt.data = (uint8_t*)buf; 
     pkt.size = buf_size;

    #ifdef USE_DLL_AVCODEC
       WITH_ALIGNED_STACK({
        Fav_init_packet(&pkt);
      });
      WITH_ALIGNED_STACK({
        res = Favcodec_decode_video(ctx, pict, got_picture_ptr, &pkt);
      });
    #else 
      av_init_packet(&pkt);
      res = avcodec_decode_video2(ctx, pict, got_picture_ptr, &pkt);
    #endif
#endif
   return res;
}

void FFMPEGLibrary::AvcodecFree(void * ptr)
{
  WaitAndSignal m(processLock);

#ifdef USE_DLL_AVCODEC
  WITH_ALIGNED_STACK({
     Favcodec_free(ptr);
  });
#else
  av_free(ptr);
#endif
}

void FFMPEGLibrary::AvSetDimensions(AVCodecContext *s, int width, int height)
{
  WaitAndSignal m(processLock);

#ifdef USE_DLL_AVCODEC
  WITH_ALIGNED_STACK({
    Favcodec_set_dimensions(s, width, height);
  });
#else
    avcodec_set_dimensions(s, width, height);
#endif
}
  

void FFMPEGLibrary::AvLogSetLevel(int level)
{
#ifdef USE_DLL_AVCODEC
  WITH_ALIGNED_STACK({
    FAv_log_set_level(level);
  });
#else
    av_log_set_level(level);
#endif
}

void FFMPEGLibrary::AvLogSetCallback(void (*callback)(void*, int, const char*, va_list))
{
#ifdef USE_DLL_AVCODEC
  WITH_ALIGNED_STACK({
    FAv_log_set_callback(callback);
  });
#else
   av_log_set_callback(callback);
#endif
}

int FFMPEGLibrary::FFCheckAlignment(void)
{
#ifdef USE_DLL_AVCODEC
  if (Fff_check_alignment == NULL) {
    TRACE(1, _codecString << "\tDYNA\tff_check_alignment is not supported by libavcodec.so - skipping check");
    return 0;
  }
  else {
    return Fff_check_alignment();
  }
#else
  return 0;
#endif
}

bool FFMPEGLibrary::IsLoaded()
{
  return isLoadedOK;
}

