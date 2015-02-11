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

#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "trace.h"
#include "rtpframe.h"
#include "h264pipe_unix.h"
#include <string.h>

#define HAVE_MKFIFO 1
#define GPL_PROCESS_FILENAME "h264_video_pwplugin_helper"
#define DIR_SEPERATOR "/"
#define DIR_TOKENISER ":"

unsigned int H264EncCtx::instances = 0;

H264EncCtx::H264EncCtx()
{
  width  = 0;
  height = 0;
  size = 0;
  startNewFrame = true;
  loaded = false;  
  pipesCreated = false;
  pipesOpened = false;
  instances++;
}

H264EncCtx::~H264EncCtx()
{
  closeAndRemovePipes();
}

bool 
H264EncCtx::Load()
{
  snprintf ( dlName, sizeof(dlName), "/tmp/x264-dl-%d-%u", getpid(),GetInstanceNumber());
  snprintf ( ulName, sizeof(ulName), "/tmp/x264-ul-%d-%u", getpid(),GetInstanceNumber());
  if (!createPipes()) {
  
    closeAndRemovePipes(); 
    return false;
  }
  pipesCreated = true;  

  if (!findGplProcess()) { 

    TRACE(1, "H264\tIPC\tPP: Couldn't find GPL process executable: " << GPL_PROCESS_FILENAME)
#ifndef _WIN32
	fprintf(stderr, "ERROR: H.264 plugin couldn't find GPL process executable: " GPL_PROCESS_FILENAME "\n");
#endif
    closeAndRemovePipes(); 
    return false;
  }
    // Check if file is executable!!!!
  int pid = fork();

  if(pid == 0) 

    execGplProcess();

  else if(pid < 0) {

    TRACE(1, "H264\tIPC\tPP: Error when trying to fork");
    closeAndRemovePipes(); 
    return false;
  }

  dlStream.open(dlName, std::ios::binary);
  if (dlStream.fail()) { 

    TRACE(1, "H264\tIPC\tPP: Error when opening DL named pipe");
    closeAndRemovePipes(); 
    return false;
  }
  ulStream.open(ulName, std::ios::binary);
  if (ulStream.fail()) { 

    TRACE(1, "H264\tIPC\tPP: Error when opening UL named pipe")
    closeAndRemovePipes(); 
    return false;
  }
  pipesOpened = true;
  
  unsigned msg = INIT;
  unsigned status;
  writeStream((char*) &msg, sizeof(msg));
  flushStream();
  readStream((char*) &msg, sizeof(msg));
  readStream((char*) &status, sizeof(status));

  if (status == 0) {
    TRACE(1, "H264\tIPC\tPP: GPL Process returned failure on initialization - plugin disabled")
#ifndef _WIN32
	fprintf(stderr, "ERROR: H.264 plugin failure on initialization - plugin disabled");
#endif
    closeAndRemovePipes(); 
    return false;
  }

  TRACE(1, "H264\tIPC\tPP: Successfully forked child process "<<  pid << " and established communication")
  loaded = true;  
  return true;
}

void H264EncCtx::call(unsigned msg)
{
  if (msg == H264ENCODERCONTEXT_CREATE) 
    startNewFrame = true;
  writeStream((char*) &msg, sizeof(msg));
  flushStream();
  readStream((char*) &msg, sizeof(msg));
}

void H264EncCtx::call(unsigned msg, unsigned value)
{
  switch (msg) {
    case SET_FRAME_WIDTH:  width  = value; size = (unsigned) (width * height * 1.5) + sizeof(frameHeader) + 40; break;
    case SET_FRAME_HEIGHT: height = value; size = (unsigned) (width * height * 1.5) + sizeof(frameHeader) + 40; break;
   }
  
  writeStream((char*) &msg, sizeof(msg));
  writeStream((char*) &value, sizeof(value));
  flushStream();
  readStream((char*) &msg, sizeof(msg));
}
     
void H264EncCtx::call(unsigned msg , const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned & headerLen, unsigned int & flags, int & ret)
{
  if (startNewFrame) {

    writeStream((char*) &msg, sizeof(msg));
    if (size) {
      writeStream((char*) &size, sizeof(size));
      writeStream((char*) src, size);
      writeStream((char*) &headerLen, sizeof(headerLen));
      writeStream((char*) dst, headerLen);
      writeStream((char*) &flags, sizeof(flags) );
    }
    else {
      writeStream((char*) &srcLen, sizeof(srcLen));
      writeStream((char*) src, srcLen);
      writeStream((char*) &headerLen, sizeof(headerLen));
      writeStream((char*) dst, headerLen);
      writeStream((char*) &flags, sizeof(flags) );
    }
  }
  else {
  
    msg = ENCODE_FRAMES_BUFFERED;
    writeStream((char*) &msg, sizeof(msg));
  }
  
  flushStream();
  
  readStream((char*) &msg, sizeof(msg));
  readStream((char*) &dstLen, sizeof(dstLen));
  readStream((char*) dst, dstLen);
  readStream((char*) &flags, sizeof(flags));
  readStream((char*) &ret, sizeof(ret));

  if (flags & 1) 
    startNewFrame = true;
   else
    startNewFrame = false;
}

bool H264EncCtx::createPipes()
{
  umask(0);
#ifdef HAVE_MKFIFO
  if (mkfifo((const char*) &dlName, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {

    TRACE(1, "H264\tIPC\tPP: Error when trying to create DL named pipe");
    return false;
  }
  if (mkfifo((const char*) &ulName, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {

    TRACE(1, "H264\tIPC\tPP: Error when trying to create UL named pipe");
    return false;
  }
#else
  if (mknod((const char*) &dlName, S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, 0)) {

    TRACE(1, "H264\tIPC\tPP: Error when trying to create named pipe");
    return false;
  }
  if (mknod((const char*) &ulName, S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, 0)) {

    TRACE(1, "H264\tIPC\tPP: Error when trying to create named pipe");
    return false;
  }
#endif /* HAVE_MKFIFO */
  return true;
}

void H264EncCtx::closeAndRemovePipes()
{
  if (pipesOpened) {
    dlStream.close();
    if (dlStream.fail()) { TRACE(1, "H264\tIPC\tPP: Error when closing DL named pipe"); }
    ulStream.close();
    if (ulStream.fail()) { TRACE(1, "H264\tIPC\tPP: Error when closing UL named pipe"); }  
    pipesOpened = false;
  }
  if (pipesCreated) {
    if (std::remove((const char*) &ulName) == -1) TRACE(1, "H264\tIPC\tPP: Error when trying to remove UL named pipe - " << strerror(errno));
    if (std::remove((const char*) &dlName) == -1) TRACE(1, "H264\tIPC\tPP: Error when trying to remove DL named pipe - " << strerror(errno));
    pipesCreated = false;
  }
}

void H264EncCtx::readStream (char* data, unsigned bytes)
{
  ulStream.read(data, bytes);
  if (ulStream.fail()) { TRACE(1, "H264\tIPC\tPP: Failure on reading - terminating"); closeAndRemovePipes();      }
  if (ulStream.bad())  { TRACE(1, "H264\tIPC\tPP: Bad flag set on reading - terminating"); closeAndRemovePipes(); }
  if (ulStream.eof())  { TRACE(1, "H264\tIPC\tPP: Received EOF - terminating"); closeAndRemovePipes();            }
}

void H264EncCtx::writeStream (const char* data, unsigned bytes)
{
  dlStream.write(data, bytes);
  if (dlStream.bad())  { TRACE(1, "H264\tIPC\tPP: Bad flag set on writing - terminating"); closeAndRemovePipes(); }
}

void H264EncCtx::flushStream ()
{
  dlStream.flush();
  if (dlStream.bad())  { TRACE(1, "H264\tIPC\tPP: Bad flag set on flushing - terminating"); closeAndRemovePipes(); }
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

#ifdef LIB_DIR
  if (checkGplProcessExists(LIB_DIR)) 
    return true;
#endif

  if (checkGplProcessExists("/usr/lib")) 
    return true;

  if (checkGplProcessExists("/usr/local/lib")) 
    return true;

  return checkGplProcessExists(".");
}

bool H264EncCtx::checkGplProcessExists (const char * dir)
{
  struct stat buffer;
  memset(gplProcess, 0, sizeof(gplProcess));
  strncpy(gplProcess, dir, sizeof(gplProcess)-1);

  if (gplProcess[strlen(gplProcess)-1] != DIR_SEPERATOR[0]) 
    strcat(gplProcess, DIR_SEPERATOR);
  strcat(gplProcess, VC_PLUGIN_DIR);

  if (gplProcess[strlen(gplProcess)-1] != DIR_SEPERATOR[0]) 
    strcat(gplProcess, DIR_SEPERATOR);
  strcat(gplProcess, GPL_PROCESS_FILENAME);

  if (stat(gplProcess, &buffer ) ) { 

    TRACE(4, "H264\tIPC\tPP: Couldn't find GPL process executable in " << gplProcess);
    return false;
  }

  TRACE(4, "H264\tIPC\tPP: Found GPL process executable in  " << gplProcess);

  return true;
}

void H264EncCtx::execGplProcess() 
{
  unsigned msg;
  unsigned status = 0;
  if (execl(gplProcess,"h264_video_pwplugin_helper", dlName,ulName, NULL) == -1) {

    TRACE(1, "H264\tIPC\tPP: Error when trying to execute GPL process  " << gplProcess << " - " << strerror(errno));
    cpDLStream.open(dlName, std::ios::binary);
    if (cpDLStream.fail()) { TRACE (1, "H264\tIPC\tCP: Error when opening DL named pipe"); exit (1); }
    cpULStream.open(ulName,std::ios::binary);
    if (cpULStream.fail()) { TRACE (1, "H264\tIPC\tCP: Error when opening UL named pipe"); exit (1); }

    cpDLStream.read((char*)&msg, sizeof(msg));
    if (cpDLStream.fail()) { TRACE (1, "H264\tIPC\tCP: Failure on reading - terminating");       cpCloseAndExit(); }
    if (cpDLStream.bad())  { TRACE (1, "H264\tIPC\tCP: Bad flag set on reading - terminating");  cpCloseAndExit(); }
    if (cpDLStream.eof())  { TRACE (1, "H264\tIPC\tCP: Received EOF - terminating"); exit (1);   cpCloseAndExit(); }

    cpULStream.write((char*)&msg, sizeof(msg));
    if (cpULStream.bad())  { TRACE (1, "H264\tIPC\tCP: Bad flag set on writing - terminating");  cpCloseAndExit(); }

    cpULStream.write((char*)&status, sizeof(status));
    if (cpULStream.bad())  { TRACE (1, "H264\tIPC\tCP: Bad flag set on writing - terminating");  cpCloseAndExit(); }

    cpULStream.flush();
    if (cpULStream.bad())  { TRACE (1, "H264\tIPC\tCP: Bad flag set on flushing - terminating"); }
    cpCloseAndExit();
  }
}

void H264EncCtx::cpCloseAndExit()
{
  cpDLStream.close();
  if (cpDLStream.fail()) { TRACE (1, "H264\tIPC\tCP: Error when closing DL named pipe"); }
  cpULStream.close();
  if (cpULStream.fail()) { TRACE (1, "H264\tIPC\tCP: Error when closing UL named pipe"); }
  exit(1);
}
