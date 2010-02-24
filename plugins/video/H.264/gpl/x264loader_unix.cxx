/*
 * H.264 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) 2007 Matthias Schneider, All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "plugin-config.h"
#include "x264loader_unix.h"
#include "trace.h"
#include <dlfcn.h>
#include <string.h>

X264Library::X264Library()
{
  _dynamicLibrary = NULL;
  _isLoaded = false;
}

X264Library::~X264Library()
{
  if (_dynamicLibrary != NULL) {
     dlclose(_dynamicLibrary);
     _dynamicLibrary = NULL;
  }
}

bool X264Library::Load()
{
  if (_isLoaded)
    return true;

  if (  
#ifdef X264_LIB_NAME
      !Open(X264_LIB_NAME) &&
#endif
      !Open("libx264.so")
      && !Open("libx264"))  {
    TRACE (1, "H264\tDYNA\tFailed to load x264 library - codec disabled");
    return false;
  }

  if (!GetFunction("x264_encoder_open", (Function &)Xx264_encoder_open)) {
    TRACE (1, "H264\tDYNA\tFailed to load x264_encoder_open");
    return false;
  }
  if (!GetFunction("x264_param_default", (Function &)Xx264_param_default)) {
    TRACE (1, "H264\tDYNA\tFailed to load x264_param_default");
    return false;
  }
  if (!GetFunction("x264_encoder_encode", (Function &)Xx264_encoder_encode)) {
    TRACE (1, "H264\tDYNA\tFailed to load x264_encoder_encode");
    return false;
  }
  if (!GetFunction("x264_nal_encode", (Function &)Xx264_nal_encode)) {
    TRACE (1, "H264\tDYNA\tFailed to load x264_nal_encode");
    return false;
  }

  if (!GetFunction("x264_encoder_reconfig", (Function &)Xx264_encoder_reconfig)) {
    TRACE (1, "H264\tDYNA\tFailed to load x264_encoder_reconfig");
    return false;
  }

  if (!GetFunction("x264_encoder_headers", (Function &)Xx264_encoder_headers)) {
    TRACE (1, "H264\tDYNA\tFailed to load x264_encoder_headers");
    return false;
  }

  if (!GetFunction("x264_encoder_close", (Function &)Xx264_encoder_close)) {
    TRACE (1, "H264\tDYNA\tFailed to load x264_encoder_close");
    return false;
  }

  if (!GetFunction("x264_picture_alloc", (Function &)Xx264_picture_alloc)) {
    TRACE (1, "H264\tDYNA\tFailed to load x264_picture_alloc");
    return false;
  }

  if (!GetFunction("x264_picture_clean", (Function &)Xx264_picture_clean)) {
    TRACE (1, "H264\tDYNA\tFailed to load x264_picture_clean");
    return false;
  }

  if (!GetFunction("x264_encoder_close", (Function &)Xx264_encoder_close)) {
    TRACE (1, "H264\tDYNA\tFailed to load x264_encoder_close");
    return false;
  }

  TRACE (4, "H264\tDYNA\tLoader was compiled with x264 build " << X264_BUILD << " present" );

  _isLoaded = true;
  TRACE (4, "H264\tDYNA\tSuccessfully loaded libx264 library and verified functions");

  return true;
}


bool X264Library::Open(const char *name)
{
  TRACE(4, "H264\tDYNA\tTrying to open x264 library " << name)

  if ( strlen(name) == 0 )
    return false;

  _dynamicLibrary = dlopen(name, RTLD_NOW);

  if (_dynamicLibrary == NULL) {
    char * error = (char *) dlerror();
    if (error != NULL) {
        TRACE(4, "H264\tDYNA\tCould not load " << name << " - " << error)
    }
    else {
        TRACE(4, "H264\tDYNA\tCould not load " << name);
    }
    return false;
  } 
  TRACE(4, "H264\tDYNA\tSuccessfully loaded " << name);
  return true;
}

bool X264Library::GetFunction(const char * name, Function & func)
{
  if (_dynamicLibrary == NULL)
    return false;

  void* pFunction = dlsym(_dynamicLibrary, (const char *)name);
  if (pFunction == NULL)
    return false;

  func = (Function &) pFunction;
  return true;
}
