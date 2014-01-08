/*
 * rfc2833.h
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

#ifndef __OPAL_RFC2833_H
#define __OPAL_RFC2833_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include "rtp.h"


///////////////////////////////////////////////////////////////////////////////

class OpalRFC2833Info : public PObject {
    PCLASSINFO(OpalRFC2833Info, PObject);
  public:
    OpalRFC2833Info(
      char tone,
      unsigned duration = 0,
      unsigned timestamp = 0
    );

    char GetTone() const { return tone; }
    unsigned GetDuration() const { return duration; }
    unsigned GetTimestamp() const { return timestamp; }
    PBoolean IsToneStart() const { return duration == 0; }

  protected:
    char     tone;
    unsigned duration;
    unsigned timestamp;
};


class OpalRFC2833 : public PObject {
    PCLASSINFO(OpalRFC2833, PObject);
  public:
    OpalRFC2833(
      const PNotifier & receiveNotifier
    );

    virtual PBoolean SendTone(
      char tone,              ///<  DTMF tone code
      unsigned duration       ///<  Duration of tone in milliseconds
    );

    virtual PBoolean BeginTransmit(
      char tone  ///<  DTMF tone code
    );
    virtual PBoolean EndTransmit();

    virtual void OnStartReceive(
      char tone
    );
    virtual void OnEndReceive(
      char tone,
      unsigned duration,
      unsigned timestamp
    );

    RTP_DataFrame::PayloadTypes GetPayloadType() const { return payloadType; }

    void SetPayloadType(
      RTP_DataFrame::PayloadTypes type ///<  new payload type
    ) { payloadType = type; }

    const PNotifier & GetReceiveHandler() const { return receiveHandler; }
    const PNotifier & GetTransmitHandler() const { return transmitHandler; }

  protected:
    PMutex mutex;
    PDECLARE_NOTIFIER(RTP_DataFrame, OpalRFC2833, ReceivedPacket);
    PDECLARE_NOTIFIER(RTP_DataFrame, OpalRFC2833, TransmitPacket);
    PDECLARE_NOTIFIER(PTimer, OpalRFC2833, ReceiveTimeout);
    PDECLARE_NOTIFIER(PTimer, OpalRFC2833, TransmitEnded);

    PNotifier receiveNotifier;
    PNotifier receiveHandler;
    PNotifier transmitHandler;

    RTP_DataFrame::PayloadTypes payloadType;
    PBoolean      receiveComplete;
    BYTE      receivedTone;
    unsigned  receivedDuration;
    unsigned  receiveTimestamp;
    PTimer    receiveTimer;

    enum {
      TransmitIdle,
      TransmitActive,
      TransmitEnding
    }         transmitState;
    BYTE      transmitCode;
    unsigned  transmitTimestamp;
    PTimer    transmitTimer;
};


#endif // __OPAL_RFC2833_H


/////////////////////////////////////////////////////////////////////////////
