/*
 * H.264 Plugin codec for OpenH323/OPAL
 *
 * Copyright (C) Matthias Schneider, All Rights Reserved
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

#ifndef __H264PIPE_WIN32_H__
#define __H264PIPE_WIN32_H__ 1

#include "shared/pipes.h"

#include <windows.h>
typedef unsigned char u_char;
class H264EncCtx
{
  public:
     H264EncCtx();
     ~H264EncCtx();
     bool Load();
     bool isLoaded() { return loaded; };
     void call(unsigned msg);
     void call(unsigned msg, unsigned value);
     void call(unsigned msg , const u_char * src, unsigned & srcLen, u_char * dst, unsigned & dstLen, unsigned & headerLen, unsigned int & flags, int & ret);
	 unsigned GetInstanceNumber() { return instances; };

  protected:
     bool InternalLoad();
     bool createPipes();
     void closeAndRemovePipes();
     void writeStream (LPCVOID data, unsigned bytes);
     void readStream (LPVOID data, unsigned bytes);
     void flushStream ();
     bool findGplProcess();
     bool checkGplProcessExists (const char * dir);
     bool execGplProcess();
     const char* ErrorMessage();

     char pipeName [512];
     char gplProcess [512];

     HANDLE stream;
     unsigned width;
     unsigned height;
     unsigned size;
     bool startNewFrame;
     bool loaded;

  private:
     // stores no. of insances created
     static unsigned int instances;
};

#endif /* __PIPE_WIN32_H__ */
