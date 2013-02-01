/*
 * h224handler.h
 *
 * H.224 protocol handler implementation for the OpenH323 Project.
 *
 * Copyright (c) 2006 Network for Educational Technology, ETH Zurich.
 * Written by Hannes Friederich.
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
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef __H323_H224HANDLER_H
#define __H323_H224HANDLER_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <ptlib.h>
#include <rtp.h>
#ifdef H224_H281
#include <h224/h281.h>
#include <h224/h281handler.h>
#endif
#ifdef H224_H284
#include <h224/h284.h>
#endif
#ifdef H224_T140
#include <h224/t140.h>
#endif
#include <channels.h>
#include <map>


class H224_Frame;
class OpalH224Handler;

class OpalH224ReceiverThread : public PThread
{
  PCLASSINFO(OpalH224ReceiverThread, PThread);
    
public:
    
  OpalH224ReceiverThread(OpalH224Handler *h224Handler, RTP_Session & rtpSession);
  ~OpalH224ReceiverThread();
    
  virtual void Main();
    
  void Close();
    
private:
        
  OpalH224Handler *h224Handler;
  mutable PMutex inUse;
  unsigned timestamp;
  RTP_Session & rtpSession;
  PBoolean terminate;
};


class H323Connection;
class OpalH224Handler : public PObject
{
  PCLASSINFO(OpalH224Handler, PObject);
    
public:
    
  OpalH224Handler(H323Channel::Directions dir, H323Connection & connection, unsigned sessionID);
  ~OpalH224Handler();

  void CreateHandlers(H323Connection & connection);
  void DeleteHandlers();
    
  virtual void StartTransmit();
  virtual void StopTransmit();
  virtual void StartReceive();
  virtual void StopReceive();
    
  PBoolean SendClientList();
  PBoolean SendExtraCapabilities();

  PBoolean SendExtraCapabilitiesMessage(BYTE clientID, BYTE *data, PINDEX length);

  PBoolean TransmitClientFrame(BYTE clientID, H224_Frame & frame);
    
  virtual PBoolean OnReceivedFrame(H224_Frame & frame);
  virtual PBoolean OnReceivedCMEMessage(H224_Frame & frame);
  virtual PBoolean OnReceivedClientList(H224_Frame & frame);
  virtual PBoolean OnReceivedClientListCommand();
  virtual PBoolean OnReceivedExtraCapabilities(H224_Frame & frame);
  virtual PBoolean OnReceivedExtraCapabilitiesCommand();
    
  PMutex & GetTransmitMutex() { return transmitMutex; }
    
  RTP_Session * GetSession() const { return session; }
    
  virtual OpalH224ReceiverThread * CreateH224ReceiverThread();

  H323Channel::Directions GetDirection() { return sessionDirection; }
    
protected:

  RTP_Session * session;

  PBoolean canTransmit;
  PMutex transmitMutex;
  RTP_DataFrame *transmitFrame;
  BYTE transmitBitIndex;
  PTime *transmitStartTime;
    
  OpalH224ReceiverThread *receiverThread;

  PMutex handlersMutex;
  std::map<BYTE, H224_Handler*> m_h224Handlers;

  H323Channel::Directions sessionDirection;
    
private:
        
  void TransmitFrame(H224_Frame & frame);
    
};

#endif // __H323_H224HANDLER_H

