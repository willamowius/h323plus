/*
 * main.h
 *
 * A simple H.323 "net telephone" application.
 *
 * Copyright (c) 2000 Equivalence Pty. Ltd.
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
 * The Original Code is Portable Windows Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef _SimpleH323_MAIN_H
#define _SimpleH323_MAIN_H

#include <ptlib/sound.h>
#include <h323.h>

#if !defined(P_USE_STANDARD_CXX_BOOL) && !defined(P_USE_INTEGER_BOOL)
    typedef int PBoolean;
#endif

class SimpleH323EndPoint : public H323EndPoint
{
  PCLASSINFO(SimpleH323EndPoint, H323EndPoint);

  public:
    SimpleH323EndPoint();
    ~SimpleH323EndPoint();

    // overrides from H323EndPoint
    virtual H323Connection * CreateConnection(unsigned callReference);
    virtual PBoolean OnIncomingCall(H323Connection &, const H323SignalPDU &, H323SignalPDU &);
    virtual H323Connection::AnswerCallResponse OnAnswerCall(H323Connection &, const PString &, const H323SignalPDU &, H323SignalPDU &);
    virtual PBoolean OnConnectionForwarded(H323Connection &, const PString &, const H323SignalPDU &);
    virtual void OnConnectionEstablished(H323Connection & connection, const PString & token);
    virtual void OnConnectionCleared(H323Connection & connection, const PString & clearedCallToken);
    virtual PBoolean OpenAudioChannel(H323Connection &, PBoolean, unsigned, H323AudioCodec &);
#ifdef H323_VIDEO
	virtual PBoolean OpenVideoChannel(H323Connection &, PBoolean, H323VideoCodec &);
#ifdef H323_H239
	virtual PBoolean OpenExtendedVideoChannel(H323Connection &, PBoolean,H323VideoCodec &);
#endif
#endif
#ifdef H323_UPnP
    virtual PBoolean OnUPnPAvailable(const PString & device, const PIPSocket::Address & publicIP, PNatMethod_UPnP * nat);
#endif

#ifdef H323_H460P
    virtual void PresenceInstruction(const PString & locAlias,unsigned type, 
                                     const PString & subAlias, const PString & subDisplay);
#endif
#ifdef H323_H235
    virtual void OnMediaEncryption(unsigned session, H323Channel::Directions dir, const PString & cipher);
#endif

#ifdef H323_TLS
    virtual void OnSecureSignallingChannel(bool isSecured);
#endif
    // New functions
    PBoolean Initialise(PArgList &);
    PBoolean SetSoundDevice(PArgList &, const char *, PSoundChannel::Directions);

    PString currentCallToken;

  protected:
    PBoolean autoAnswer;
    PString busyForwardParty;
    PString videoDriver;
};


class SimpleH323Connection : public H323Connection
{
    PCLASSINFO(SimpleH323Connection, H323Connection);

  public:
    SimpleH323Connection(SimpleH323EndPoint &, unsigned);

    virtual PBoolean OnStartLogicalChannel(H323Channel &);
    virtual void OnUserInputString(const PString &);
};


class SimpleH323Process : public PProcess
{
  PCLASSINFO(SimpleH323Process, PProcess)

  public:
    SimpleH323Process();
    ~SimpleH323Process();

    void Main();

  protected:
    SimpleH323EndPoint * endpoint;
};


#endif  // _SimpleH323_MAIN_H


// End of File ///////////////////////////////////////////////////////////////
