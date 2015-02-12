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

#include "../shared/pipes.h"

#ifdef _MSC_VER
#include "../../common/trace.h"
#define snprintf _snprintf
#else
#include "trace.h"
#endif

#include "enc-ctx.h"

#include <windows.h>
#include <stdio.h>

HANDLE stream;
unsigned msg;
unsigned val;

unsigned srcLen = 0;
unsigned dstLen = 0;
unsigned headerLen;
unsigned frameBufferSize = 0;
unsigned char * src = NULL;
unsigned char * dst  = NULL;
unsigned flags;
int ret;

X264EncoderContext* x264;
#ifndef X264_LINK_STATIC
extern X264Library X264Lib;
#endif

const char* 
ErrorMessage()
{
  static char string [1024];
  DWORD dwMsgLen;

  memset (string, 0, sizeof (string));
  dwMsgLen = FormatMessageA (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL,
                             GetLastError (),
                             MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                             (LPSTR) string,
                             sizeof (string)-1,
                             NULL);
  if (dwMsgLen) {
    string [ strlen(string) - 2 ] = 0;
    snprintf (string, sizeof (string), "%s (%u)", string, (int) GetLastError ());
  }
  else {
    snprintf (string, sizeof (string), "%u", (int) GetLastError ());
  }

  return string;
}

void closeAndExit()
{
  if (!CloseHandle(stream))
 	TRACE(1, "H264\tIPC\tCP: Failure on closing Handle (" << ErrorMessage() << ")");
  exit(1);
}

void openPipe(const char* name) 
{
  DWORD dwMode;
  if (!WaitNamedPipe(name, NMPWAIT_USE_DEFAULT_WAIT)) { 

 	TRACE(1, "H264\tIPC\tCP: Error when waiting for Pipe (" << ErrorMessage() << ")");
    exit (1);
  } 

  stream = CreateFile( name,                         // pipe name 
                       GENERIC_READ | GENERIC_WRITE, // read and write access 
                       0,                            // no sharing 
                       NULL,                         // default security attributes
                       OPEN_EXISTING,                // opens existing pipe 
                       0,                            // default attributes 
                       NULL);                        // no template file 

  if (stream == INVALID_HANDLE_VALUE)  {

	TRACE(1, "H264\tIPC\tCP: Could not open Pipe (" << ErrorMessage() << ")");
    exit (1);
  }

  // The pipe connected; change to message-read mode. 
  dwMode = PIPE_READMODE_MESSAGE; 
  if (!SetNamedPipeHandleState( stream,   // pipe handle 
                                &dwMode,  // new pipe mode 
                                NULL,     // don't set maximum bytes 
                                NULL))    // don't set maximum time 
  {
    TRACE(1, "H264\tIPC\tCP: Failure on activating message mode (" << ErrorMessage() << ")");
    closeAndExit();
  }
}

void readStream (HANDLE stream, LPVOID data, unsigned bytes)
{
  DWORD bytesRead;
  BOOL fSuccess; 
  fSuccess = ReadFile( 
                      stream,     // handle to pipe 
                      data,       // buffer to receive data 
                      bytes,      // size of buffer 
                      &bytesRead, // number of bytes read 
                      NULL        // blocking IO
  );

  if (!fSuccess) {

	TRACE(1, "H264\tIPC\tCP: Failure on reading - terminating (" << ErrorMessage() << ")");
	closeAndExit();
  }

  if (bytes != bytesRead) {

    TRACE(1, "H264\tIPC\tCP: Failure on reading - terminating (Read " << bytesRead << " bytes, expected " << bytes);
	closeAndExit();
  }
}

void writeStream (HANDLE stream, LPCVOID data, unsigned bytes)
{
  DWORD bytesWritten;
  BOOL fSuccess; 
  fSuccess = WriteFile( 
                       stream,         // handle to pipe 
                       data,           // buffer to write from 
                       bytes,          // number of bytes to write 
                       &bytesWritten,  // number of bytes written 
                       NULL          // not overlapped I/O 
  );

  if (!fSuccess) {

	TRACE(1, "H264\tIPC\tCP: Failure on writing - terminating (" << ErrorMessage() << ")");
	closeAndExit();
  }

  if (bytes != bytesWritten) {

    TRACE(1, "H264\tIPC\tCP: Failure on writing - terminating (Written " << bytesWritten << " bytes, intended " << bytes);
	closeAndExit();
  }
}

void flushStream (HANDLE stream)
{
  if (!FlushFileBuffers(stream)) {

	TRACE(1, "H264\tIPC\tPP: Failure on flushing - terminating (" << ErrorMessage() << ")");
	closeAndExit();
  }
}


int main(int argc, char *argv[])
{
  unsigned status = 0;
  if (argc != 2) { fprintf(stderr, "Not to be executed directly - exiting\n"); exit (1); }

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
  dstLen = 1400;

  openPipe(argv[1]);
   
#ifndef X264_LINK_STATIC
  if (X264Lib.Load()) 
    status = 1;
  else 
    status = 0;
#else
  status = 1;
#endif

  if (status == 0) {
    TRACE (1, "H264\tIPC\tCP: Failed to load dynamic library - exiting"); 
    closeAndExit();
  }

  while (1) {
    readStream(stream, (LPVOID)&msg, sizeof(msg));
    switch (msg) {
      case H264ENCODERCONTEXT_CREATE:
          x264 = new X264EncoderContext();
          writeStream(stream,(LPCVOID)&msg, sizeof(msg)); 
          flushStream(stream);
        break;
      case H264ENCODERCONTEXT_DELETE:
          delete x264;
          x264 = NULL;
          writeStream(stream,(LPCVOID)&msg, sizeof(msg)); 
          flushStream(stream);
        break;
      case APPLY_OPTIONS:
          if (x264) {
            x264->ApplyOptions ();
            writeStream(stream,(LPCVOID)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case SET_TARGET_BITRATE:
          readStream(stream, (LPVOID)&val, sizeof(val));
          if (x264) {
            x264->SetTargetBitrate (val);
            writeStream(stream,(LPCVOID)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case SET_FRAME_RATE:
          readStream(stream, (LPVOID)&val, sizeof(val));
          if (x264) {
            x264->SetFrameRate (val);
            writeStream(stream,(LPCVOID)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case SET_FRAME_WIDTH:
          readStream(stream, (LPVOID)&val, sizeof(val));
          if (x264) {
            x264->SetFrameWidth (val);
            writeStream(stream,(LPCVOID)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case SET_FRAME_HEIGHT:
          readStream(stream, (LPVOID)&val, sizeof(val));
          if (x264) {
            x264->SetFrameHeight (val);
            writeStream(stream,(LPCVOID)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case SET_MAX_KEY_FRAME_PERIOD:
          readStream(stream, (char*)&val, sizeof(val));
          if (x264) {
            x264->SetMaxKeyFramePeriod (val);
            writeStream(stream,(char*)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case SET_TSTO:
          readStream(stream, (char*)&val, sizeof(val));
          if (x264) {
            x264->SetTSTO (val);
            writeStream(stream,(char*)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case SET_PROFILE_LEVEL:
          readStream(stream, (char*)&val, sizeof(val));
          if (x264) {
            x264->SetProfileLevel (val);
            writeStream(stream,(char*)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case ENCODE_FRAMES:
          readStream(stream, (LPVOID)&srcLen, sizeof(srcLen));
		  if (srcLen > frameBufferSize) {
			// only grow, never shrink, memory isn't given back to OS anyway
            TRACE (1, "H264\tIPC\tGrowing frame buffer to " << srcLen);
			free(src);
			free(dst);
			frameBufferSize = srcLen;
			src = (unsigned char*)malloc(frameBufferSize);
			dst = (unsigned char*)malloc(frameBufferSize);
		  }
          readStream(stream, (LPVOID)src, srcLen);
          readStream(stream, (LPVOID)&headerLen, sizeof(headerLen));
          readStream(stream, (LPVOID)dst, headerLen);
          readStream(stream, (LPVOID)&flags, sizeof(flags));   
          // fall through intended
      case ENCODE_FRAMES_BUFFERED:     
          if (x264) {
            ret = (x264->EncodeFrames( src,  srcLen, dst, dstLen, flags));
            writeStream(stream,(LPCVOID)&msg, sizeof(msg));
            writeStream(stream,(LPCVOID)&dstLen, sizeof(dstLen));
            writeStream(stream,(LPCVOID)&dst, dstLen);
            writeStream(stream,(LPCVOID)&flags, sizeof(flags));
            writeStream(stream,(LPCVOID)&ret, sizeof(ret));
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case SET_MAX_FRAME_SIZE:
          readStream(stream, (LPVOID)&val, sizeof(val));
          if (x264) {
            x264->SetMaxRTPFrameSize (val);
            writeStream(stream,(LPCVOID)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case FASTUPDATE_REQUESTED:
          if (x264) {
            x264->fastUpdateRequested();
            writeStream(stream,(LPCVOID)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      case SET_MAX_NALSIZE:
          readStream(stream, (LPVOID)&val, sizeof(val));
          if (x264) {
            x264->SetMaxNALSize (val);
            writeStream(stream,(LPCVOID)&msg, sizeof(msg)); 
            flushStream(stream);
          } else {
            TRACE (1, "H264\tIPC\tCodec not created, yet");
          }
        break;
      default:
        break;
    }
  }
  return 0;
}

