/*
 * H.264 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) 2007 Matthias Schneider, All Rights Reserved
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
 * Contributor(s): Matthias Schneider (ma30002000@yahoo.de)
 *
 */

/*
  Notes
  -----

 */


#ifndef _STATIC_LINK
#include "h264pipe_win32.h"

#ifdef _MSC_VER
#include "../common/rtpframe.h"
#include "../common/trace.h"
#else
#include "rtpframe.h"
#include "trace.h"
#endif

#include <string.h>
#include <stdio.h>
#include <tchar.h>
#include <stdio.h> 
#include <tchar.h>


#include <fstream>

#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096
#ifndef _MSC_VER
#define GPL_PROCESS_FILENAME "plugins\\h264_video_pwplugin_helper.exe"
#else
#define GPL_PROCESS_FILENAME "x264plugin_helper.exe"
#endif
#define DIR_SEPERATOR "\\"
#define DIR_TOKENISER ";"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

unsigned int H264EncCtx::instances = 0;

H264EncCtx::H264EncCtx()
{
  width  = 0;
  height = 0;
  size = 0;
  startNewFrame = true;
  loaded = false; 
  instances++; 
}

H264EncCtx::~H264EncCtx()
{
    if (loaded)
       closeAndRemovePipes();
}

bool H264EncCtx::Load()
{
#ifdef _DELAY_LOAD
    return true;
#else
    return InternalLoad();
#endif
}

bool H264EncCtx::InternalLoad()
{
  if (loaded)
      return true;

  snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\x264-%d-%u", GetCurrentProcessId(), GetInstanceNumber());

  if (!createPipes()) {
  
    closeAndRemovePipes(); 
    return false;
  }
  
  if (!findGplProcess()) { 

    TRACE(1, "H264\tIPC\tPP: Couldn't find GPL process executable " << GPL_PROCESS_FILENAME);
    closeAndRemovePipes(); 
    return false;
  }  

  if (!execGplProcess()) { 

	closeAndRemovePipes(); 
    return false;
  }  
  
  if (!ConnectNamedPipe(stream, NULL)) {
	if (GetLastError() != ERROR_PIPE_CONNECTED) {

          TRACE(1, "H264\tIPC\tPP: Could not establish communication with child process (" << ErrorMessage() << ")");
	  closeAndRemovePipes(); 
	  return false;
	}
  } 

  loaded = true;
#ifdef _DELAY_LOAD
  call(H264ENCODERCONTEXT_CREATE);
#endif

  TRACE(1, "H264\tIPC\tPP: Successfully established communication with child process");
  return true;
}

void H264EncCtx::call(unsigned msg)
{
#ifdef _DELAY_LOAD
  if (!InternalLoad())
      return;
#endif

  if (msg == H264ENCODERCONTEXT_CREATE) 
     startNewFrame = true;

  writeStream((LPCVOID)&msg, sizeof(msg));
  flushStream();
  readStream((LPVOID)&msg, sizeof(msg));

  if (msg == H264ENCODERCONTEXT_DELETE) 
     closeAndRemovePipes();
}

void H264EncCtx::call(unsigned msg, unsigned value)
{
#ifdef _DELAY_LOAD
  if (!InternalLoad())
      return;
#endif

  switch (msg) {
    case SET_FRAME_WIDTH:  width  = value; size = (unsigned) (width * height * 1.5) + sizeof(frameHeader) + 40; break;
    case SET_FRAME_HEIGHT: height = value; size = (unsigned) (width * height * 1.5) + sizeof(frameHeader) + 40; break;
   }
  
  writeStream((LPCVOID) &msg, sizeof(msg));
  writeStream((LPCVOID) &value, sizeof(value));
  flushStream();
  readStream((LPVOID) &msg, sizeof(msg));
}

     
void H264EncCtx::call(unsigned msg , const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned & headerLen, unsigned int & flags, int & ret)
{
#ifdef _DELAY_LOAD
  if (!InternalLoad())
      return;
#endif

  if (startNewFrame) {

    writeStream((LPCVOID) &msg, sizeof(msg));
    if (size) {
      writeStream((LPCVOID) &size, sizeof(size));
      writeStream((LPCVOID) src, size);
      writeStream((LPCVOID) &headerLen, sizeof(headerLen));
      writeStream((LPCVOID) dst, headerLen);
      writeStream((LPCVOID) &flags, sizeof(flags) );
    }
    else {
      writeStream((LPCVOID) &srcLen, sizeof(srcLen));
      writeStream((LPCVOID) src, srcLen);
      writeStream((LPCVOID) &headerLen, sizeof(headerLen));
      writeStream((LPCVOID) dst, headerLen);
      writeStream((LPCVOID) &flags, sizeof(flags) );
    }
  }
  else {
  
    msg = ENCODE_FRAMES_BUFFERED;
    writeStream((LPCVOID) &msg, sizeof(msg));
  }
  
  flushStream();
  
  readStream((LPVOID) &msg, sizeof(msg));
  readStream((LPVOID) &dstLen, sizeof(dstLen));
  readStream((LPVOID) dst, dstLen);
  readStream((LPVOID) &flags, sizeof(flags));
  readStream((LPVOID) &ret, sizeof(ret));

  if (flags & 1) 
    startNewFrame = true;
   else
    startNewFrame = false;
}


bool H264EncCtx::createPipes()
{
  stream = CreateNamedPipe(
           pipeName,
           PIPE_ACCESS_DUPLEX, // FILE_FLAG_FIRST_PIPE_INSTANCE (not supported by minGW lib)
           PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, // deny via network ACE
           1,
           BUFSIZE,
           BUFSIZE,
           PIPE_TIMEOUT,
           NULL
  );

	// Break if the pipe handle is valid.
	if (stream != INVALID_HANDLE_VALUE)
		return true;

	// Exit if an error other than ERROR_PIPE_BUSY occurs.
	if (GetLastError() != ERROR_PIPE_BUSY)
	{
		TRACE(1, "H264\tIPC\tPP: Failure on creating Pipe - terminating (" << ErrorMessage() << ")");
		return false;
	}

	// All pipe instances are busy, so wait for 5 seconds.
	if (!WaitNamedPipe(pipeName, 5000))
	{
		TRACE(1, "H264\tIPC\tPP: TimeOut on creating Pipe - terminating (" << ErrorMessage() << ")");
		return false;
	}
    loaded = true;

  return true;
}

void H264EncCtx::closeAndRemovePipes()
{
    if (loaded) {
      if (!DisconnectNamedPipe(stream))
 	    TRACE(1, "H264\tIPC\tPP: Failure on disconnecting Pipe (" << ErrorMessage() << ")");
      if (!CloseHandle(stream))
 	    TRACE(1, "H264\tIPC\tPP: Failure on closing Handle (" << ErrorMessage() << ")");
    }
    loaded = false;
}

void H264EncCtx::readStream (LPVOID data, unsigned bytes)
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

	TRACE(1, "H264\tIPC\tPP: Failure on reading - terminating (" << ErrorMessage() << ")");
	closeAndRemovePipes();
  }

  if (bytes != bytesRead) {

    TRACE(1, "H264\tIPC\tPP: Failure on reading - terminating (Read " << bytesRead << " bytes, expected " << bytes);
	closeAndRemovePipes();
  }
}

void H264EncCtx::writeStream (LPCVOID data, unsigned bytes)
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

	TRACE(1, "H264\tIPC\tPP: Failure on writing - terminating (" << ErrorMessage() << ")");
	closeAndRemovePipes();
  }

  if (bytes != bytesWritten) {

    TRACE(1, "H264\tIPC\tPP: Failure on writing - terminating (Written " << bytesWritten << " bytes, intended " << bytes);
	closeAndRemovePipes();
  }
}

void H264EncCtx::flushStream ()
{
  if (!FlushFileBuffers(stream)) {

	TRACE(1, "H264\tIPC\tPP: Failure on flushing - terminating (" << ErrorMessage() << ")");
	closeAndRemovePipes();
  }
}

bool H264EncCtx::findGplProcess()
{
  char * env = ::getenv("PWLIBPLUGINDIR");
  if (env == NULL)
    env = ::getenv("PTLIBPLUGINDIR");
  if (env != NULL) {
    const char * token = strtok(env, DIR_TOKENISER);
    while (token != NULL) {

      if (checkGplProcessExists(token)) 
        return true;

      token = strtok(NULL, DIR_TOKENISER);
    }
  }
  return checkGplProcessExists(".");
}

bool H264EncCtx::checkGplProcessExists (const char * dir)
{
  fstream fin;
  memset(gplProcess, 0, sizeof(gplProcess));
  strncpy(gplProcess, dir, sizeof(gplProcess)-1);
  if (gplProcess[strlen(gplProcess)-1] != DIR_SEPERATOR[0]) 
    strcat(gplProcess, DIR_SEPERATOR);
  strcat(gplProcess, GPL_PROCESS_FILENAME);

  fin.open(gplProcess,ios::in);
  if( !fin.is_open() ){
    TRACE(1, "H264\tIPC\tPP: Couldn't find GPL process executable in " << gplProcess);
    fin.close();
    return false;
  }
  fin.close();
  TRACE(1, "H264\tIPC\tPP: Found GPL process executable in " << gplProcess);
  return true;
}

bool H264EncCtx::execGplProcess() 
{
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  char command[1024];

  snprintf(command,sizeof(command), "%s %s", gplProcess, pipeName);
  ZeroMemory( &si, sizeof(si) );
  si.cb = sizeof(si);
  ZeroMemory( &pi, sizeof(pi) );

  // Start the child process. 
  if( !CreateProcess( NULL,        // No module name (use command line)
                      command,  // Command line
                      NULL,        // Process handle not inheritable
                      NULL,        // Thread handle not inheritable
                      FALSE,       // Set handle inheritance to FALSE
                      (Trace::GetLevel() == 0) ? CREATE_NO_WINDOW : 0, // Creation flags
                      NULL,        // Use parent's environment block
                      NULL,        // Use parent's starting directory 
                      &si,         // Pointer to STARTUPINFO structure
                      &pi ))       // Pointer to PROCESS_INFORMATION structure
  {
      TRACE(1, "H264\tIPC\tPP: Couldn't create child process: " << ErrorMessage());
      return false;
  }
  TRACE(1, "H264\tIPC\tPP: Successfully created child process " << pi.dwProcessId);
  return true;
}

const char* 
H264EncCtx::ErrorMessage()
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

#endif // 


