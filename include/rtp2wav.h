/*
 * rtp2wav.h
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2001 Equivalence Pty. Ltd.
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
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef __RTP_RTP2WAV_H
#define __RTP_RTP2WAV_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <ptclib/pwavfile.h>
#include "rtp.h"


///////////////////////////////////////////////////////////////////////////////

/**This class encapsulates a WAV file that can be used to intercept RTP data
   in the standard H323_RTPChannel class.
  */
class OpalRtpToWavFile : public PWAVFile
{
    PCLASSINFO(OpalRtpToWavFile, PWAVFile);
  public:
    OpalRtpToWavFile();
    OpalRtpToWavFile(
      const PString & filename
    );

    virtual PBoolean OnFirstPacket(RTP_DataFrame & frame);

    const PNotifier & GetReceiveHandler() const { return receiveHandler; }

  protected:
    PDECLARE_NOTIFIER(RTP_DataFrame, OpalRtpToWavFile, ReceivedPacket);

    PNotifier                   receiveHandler;
    RTP_DataFrame::PayloadTypes payloadType;
    PBYTEArray                  lastFrame;
    PINDEX                      lastPayloadSize;
};


#endif // __RTP_RTP2WAV_H


/////////////////////////////////////////////////////////////////////////////
