/*
 * H.264 static codec for H323plus
 *
 * Copyright (c) 2012 Spranto Australia Pty Ltd. All Rights Reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the General Public License (the  "GNU License"), in which case the
 * provisions of GNU License are applicable instead of those
 * above. If you wish to allow use of your version of this file only
 * under the terms of the GNU License and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GNU License. If you do not delete
 * the provisions above, a recipient may use your version of this file
 * under either the MPL or the GNU License."
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is derived from and used in conjunction with the 
 * H323Plus Project (www.h323plus.org/)
 *
 * The Original Code is H323plus Library.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

/*
  Notes
  -----

 */



#ifndef __H264PIPE_STATIC_H__
#define __H264PIPE_STATIC_H__ 1

#include "shared/pipes.h"

typedef unsigned char u_char;

class X264EncoderContext;
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

  protected:
     bool InternalLoad();
     static X264EncoderContext * x264;
     static bool loaded;
     static int encCounter;

};

#endif /* __PIPE_STATIC_H__ */

