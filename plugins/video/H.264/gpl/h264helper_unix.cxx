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

#include "shared/pipes.h"
#include "enc-ctx.h"
#include <sys/stat.h>
#include <fstream>
#include "trace.h"
#include <stdlib.h> 

#ifndef X264_LINK_STATIC
#include "x264loader_unix.h"
#endif
#define MAX_FRAME_SIZE 608286

std::ifstream dlStream;
std::ofstream ulStream;

unsigned msg;
unsigned val;

unsigned srcLen;
unsigned dstLen;
unsigned headerLen;
unsigned char src [MAX_FRAME_SIZE];
unsigned char dst [MAX_FRAME_SIZE];
unsigned flags;
int ret;

X264EncoderContext* x264;

#ifndef X264_LINK_STATIC
extern X264Library X264Lib;
#endif

void closeAndExit()
{
  dlStream.close();
  ulStream.close();
  exit(1);
}

void writeStream (std::ofstream & stream, const char* data, unsigned bytes)
{
  stream.write(data, bytes);
  if (stream.bad())  { TRACE (1, "H264\tIPC\tCP: Bad flag set on writing - terminating"); closeAndExit(); }
}

void readStream (std::ifstream & stream, char* data, unsigned bytes)
{
  stream.read(data, bytes);
  if (stream.fail()) { TRACE (1, "H264\tIPC\tCP: Terminating");                           closeAndExit(); }
  if (stream.bad())  { TRACE (1, "H264\tIPC\tCP: Bad flag set on reading - terminating"); closeAndExit(); }
  if (stream.eof())  { TRACE (1, "H264\tIPC\tCP: Received EOF - terminating");            closeAndExit(); }
}

void flushStream (std::ofstream & stream)
{
  stream.flush();
  if (stream.bad())  { TRACE (1, "H264\tIPC\tCP: Bad flag set on flushing - terminating"); closeAndExit(); }
}


int main(int argc, char *argv[])
{
  unsigned status;
  if (argc != 3) { fprintf(stderr, "Not to be executed directly - exiting\n"); exit (1); }

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

  x264 = NULL;
  dstLen=1400;
  dlStream.open(argv[1], std::ios::binary);
  if (dlStream.fail()) { TRACE (1, "H264\tIPC\tCP: Error when opening DL named pipe"); exit (1); }
  ulStream.open(argv[2],std::ios::binary);
  if (ulStream.fail()) { TRACE (1, "H264\tIPC\tCP: Error when opening UL named pipe"); exit (1); }

#ifndef X264_LINK_STATIC
  if (X264Lib.Load()) 
    status = 1;
  else 
    status = 0;
#else
  status = 1;
#endif

  readStream(dlStream, (char*)&msg, sizeof(msg));
  writeStream(ulStream,(char*)&msg, sizeof(msg)); 
  writeStream(ulStream,(char*)&status, sizeof(status)); 
  flushStream(ulStream);

  if (status == 0) {
    TRACE (1, "H264\tIPC\tCP: Failed to load dynamic library - exiting"); 
    closeAndExit();
  }

  while (1) {
    readStream(dlStream, (char*)&msg, sizeof(msg));

  switch (msg) {
    case H264ENCODERCONTEXT_CREATE:
        x264 = new X264EncoderContext();
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    case H264ENCODERCONTEXT_DELETE:;
        delete x264;
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    case APPLY_OPTIONS:;
        x264->ApplyOptions ();
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    case SET_TARGET_BITRATE:
        readStream(dlStream, (char*)&val, sizeof(val));
        x264->SetTargetBitrate (val);
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    case SET_FRAME_RATE:
        readStream(dlStream, (char*)&val, sizeof(val));
        x264->SetFrameRate (val);
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    case SET_FRAME_WIDTH:
        readStream(dlStream, (char*)&val, sizeof(val));
        x264->SetFrameWidth (val);
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    case SET_FRAME_HEIGHT:
        readStream(dlStream, (char*)&val, sizeof(val));
        x264->SetFrameHeight (val);
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    case SET_MAX_KEY_FRAME_PERIOD:
        readStream(dlStream, (char*)&val, sizeof(val));
        x264->SetMaxKeyFramePeriod (val);
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    case SET_TSTO:
        readStream(dlStream, (char*)&val, sizeof(val));
        x264->SetTSTO (val);
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    case SET_PROFILE_LEVEL:
        readStream(dlStream, (char*)&val, sizeof(val));
        x264->SetProfileLevel (val);
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    case ENCODE_FRAMES:
        readStream(dlStream, (char*)&srcLen, sizeof(srcLen));
        readStream(dlStream, (char*)&src, srcLen);
        readStream(dlStream, (char*)&headerLen, sizeof(headerLen));
        readStream(dlStream, (char*)&dst, headerLen);
        readStream(dlStream, (char*)&flags, sizeof(flags));
    case ENCODE_FRAMES_BUFFERED:
        ret = (x264->EncodeFrames( src,  srcLen, dst, dstLen, flags));
        writeStream(ulStream,(char*)&msg, sizeof(msg));
        writeStream(ulStream,(char*)&dstLen, sizeof(dstLen));
        writeStream(ulStream,(char*)&dst, dstLen);
        writeStream(ulStream,(char*)&flags, sizeof(flags));
        writeStream(ulStream,(char*)&ret, sizeof(ret));
        flushStream(ulStream);
      break;
    case SET_MAX_FRAME_SIZE:
        readStream(dlStream, (char*)&val, sizeof(val));
        x264->SetMaxRTPFrameSize (val);
        writeStream(ulStream,(char*)&msg, sizeof(msg)); 
        flushStream(ulStream);
      break;
    default:
      break;
    }
  }
  return 0;
}
