/*
 * h224.cxx
 *
 * H.224 implementation for the OpenH323 Project.
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
 */

#include <ptlib.h>
#include <h323.h>

#ifdef H323_H224

#ifdef __GNUC__
#pragma implementation "h224.h"
#pragma implementation "h224handler.h"
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#endif

#include <h224/h224.h>
#include <h323con.h>
#include <h245.h>


H224_Frame::H224_Frame(PINDEX size)
: Q922_Frame(H224_HEADER_SIZE + size)
{
  SetHighPriority(FALSE);
    
  SetControlFieldOctet(0x03);
    
  BYTE *data = GetInformationFieldPtr();
    
  // setting destination & source terminal address to BROADCAST
  data[0] = 0;
  data[1] = 0;
  data[2] = 0;
  data[3] = 0;
    
  // setting Client ID to CME
  data[4] = 0;
    
  // setting ES / BS / C1 / C0 / Segment number to zero
  data[5] = 0;
}

H224_Frame::~H224_Frame()
{
}

void H224_Frame::SetHighPriority(PBoolean flag)
{
  SetHighOrderAddressOctet(0x00);
    
  if(flag) {
    SetLowOrderAddressOctet(0x71);
  } else {
    SetLowOrderAddressOctet(0x061);
  }
}

WORD H224_Frame::GetDestinationTerminalAddress() const
{
  BYTE *data = GetInformationFieldPtr();
  return (WORD)((data[0] << 8) | data[1]);
}

void H224_Frame::SetDestinationTerminalAddress(WORD address)
{
  BYTE *data = GetInformationFieldPtr();
  data[0] = (BYTE)(address >> 8);
  data[1] = (BYTE) address;
}

WORD H224_Frame::GetSourceTerminalAddress() const
{
  BYTE *data = GetInformationFieldPtr();
  return (WORD)((data[2] << 8) | data[3]);
}

void H224_Frame::SetSourceTerminalAddress(WORD address)
{
  BYTE *data = GetInformationFieldPtr();
  data[2] = (BYTE)(address >> 8);
  data[3] = (BYTE) address;
}

BYTE H224_Frame::GetClientID() const
{
  BYTE *data = GetInformationFieldPtr();
    
  return data[4] & 0x7f;
}

void H224_Frame::SetClientID(BYTE clientID)
{

  BYTE *data = GetInformationFieldPtr();
    
  data[4] = clientID;
}

PBoolean H224_Frame::GetBS() const
{
  BYTE *data = GetInformationFieldPtr();
    
  return (data[5] & 0x80) != 0;
}

void H224_Frame::SetBS(PBoolean flag)
{
  BYTE *data = GetInformationFieldPtr();
    
  if(flag) {
    data[5] |= 0x80;
  }    else {
    data[5] &= 0x7f;
  }
}

PBoolean H224_Frame::GetES() const
{
  BYTE *data = GetInformationFieldPtr();
    
  return (data[5] & 0x40) != 0;
}

void H224_Frame::SetES(PBoolean flag)
{
  BYTE *data = GetInformationFieldPtr();
    
  if(flag) {
    data[5] |= 0x40;
  } else {
    data[5] &= 0xbf;
  }
}

PBoolean H224_Frame::GetC1() const
{
  BYTE *data = GetInformationFieldPtr();
    
  return (data[5] & 0x20) != 0;
}

void H224_Frame::SetC1(PBoolean flag)
{
  BYTE *data = GetInformationFieldPtr();
    
  if(flag) {
    data[5] |= 0x20;
  } else {
    data[5] &= 0xdf;
  }
}

PBoolean H224_Frame::GetC0() const
{
  BYTE *data = GetInformationFieldPtr();
    
  return (data[5] & 0x10) != 0;
}

void H224_Frame::SetC0(PBoolean flag)
{
  BYTE *data = GetInformationFieldPtr();
    
  if(flag) {
    data[5] |= 0x10;
  }    else {
    data[5] &= 0xef;
  }
}

BYTE H224_Frame::GetSegmentNumber() const
{
  BYTE *data = GetInformationFieldPtr();
    
  return (data[5] & 0x0f);
}

void H224_Frame::SetSegmentNumber(BYTE segmentNumber)
{
  BYTE *data = GetInformationFieldPtr();
    
  data[5] &= 0xf0;
  data[5] |= (segmentNumber & 0x0f);
}

PBoolean H224_Frame::Decode(const BYTE *data, 
                        PINDEX size)
{
  PBoolean result = Q922_Frame::Decode(data, size);
    
  if(result == FALSE) {
    return FALSE;
  }
    
  // doing some validity check for H.224 frames
  BYTE highOrderAddressOctet = GetHighOrderAddressOctet();
  BYTE lowOrderAddressOctet = GetLowOrderAddressOctet();
  BYTE controlFieldOctet = GetControlFieldOctet();
    
  if((highOrderAddressOctet != 0x00) ||
     (!(lowOrderAddressOctet == 0x61 || lowOrderAddressOctet == 0x71)) ||
     (controlFieldOctet != 0x03) ||
     (GetClientID() > 0x02))
  {        
      return FALSE;
  }
    
  return TRUE;
}

/////////////////////////////////////

H224_Handler::H224_Handler(const PString & name)
: m_h224Handler(NULL), m_direction(H323Channel::IsTransmitter), m_h224Display(name)
{

}

H224_Handler::~H224_Handler()
{

}

void H224_Handler::AttachH224Handler(OpalH224Handler * h224Handler)
{
    if (!m_h224Handler) {
      m_h224Handler = h224Handler;
      m_direction = h224Handler->GetDirection();
    }
}

////////////////////////////////////////////////////////////////////////////////

static const char H224HandlerBaseClass[] = "H224_Handler";

#if PTLIB_VER >= 2110
template <> H224_Handler * PDevicePluginFactory<H224_Handler>::Worker::Create(const PDefaultPFactoryKey & type) const
#else
template <> H224_Handler * PDevicePluginFactory<H224_Handler>::Worker::Create(const PString & type) const
#endif
{
  return H224_Handler::CreateHandler(type);
}

typedef PDevicePluginAdapter<H224_Handler> PDevicePluginH224;
PFACTORY_CREATE(PFactory<PDevicePluginAdapterBase>, PDevicePluginH224, H224HandlerBaseClass, true);

PStringArray H224_Handler::GetHandlerNames(PPluginManager * pluginMgr)
{
  if (pluginMgr == NULL)
    pluginMgr = &PPluginManager::GetPluginManager();

  return pluginMgr->GetPluginsProviding(H224HandlerBaseClass);
}

H224_Handler * H224_Handler::CreateHandler(const PString & handlerName, PPluginManager * pluginMgr)
{
  if (pluginMgr == NULL)
    pluginMgr = &PPluginManager::GetPluginManager();

  return (H224_Handler *)pluginMgr->CreatePluginsDeviceByName(handlerName, H224HandlerBaseClass);
}

////////////////////////////////////

template <class PAIR>
class deletepair { // PAIR::second_type is a pointer type
public:
    void operator()(const PAIR & p) { delete p.second; }
};

template <class M>
inline void DeleteObjectsInMap(const M & m)
{
    typedef typename M::value_type PAIR;
    std::for_each(m.begin(), m.end(), deletepair<PAIR>());
}

////////////////////////////////////

OpalH224Handler::OpalH224Handler(H323Channel::Directions dir,
                                 H323Connection & connection,
                                 unsigned sessionID)
: canTransmit(FALSE), transmitMutex(), sessionDirection(dir)
{

  H245_TransportAddress addr;
  connection.GetControlChannel().SetUpTransportPDU(addr, H323Transport::UseLocalTSAP);
  session = connection.UseSession(sessionID,addr,H323Channel::IsBidirectional);

  CreateHandlers(connection);
  receiverThread = NULL;
    
}

OpalH224Handler::~OpalH224Handler()
{
    DeleteHandlers();
}

void OpalH224Handler::CreateHandlers(H323Connection & connection)
{
    PStringArray handlers = H224_Handler::GetHandlerNames();

    for (PINDEX i = 0; i < handlers.GetSize(); i++) {
        H224_Handler * handler = NULL;
        handler = connection.CreateH224Handler(sessionDirection,*this, handlers[i]);
        if (!handler) {
            handler = H224_Handler::CreateHandler(handlers[i]);
            if (handler) 
                handler->AttachH224Handler(this);
        }
        if (!handler) continue;

        if (connection.OnCreateH224Handler(sessionDirection, handlers[i], handler)) {
          handlersMutex.Wait();
            m_h224Handlers.insert(pair<BYTE,H224_Handler*>(handler->GetClientID(), handler));
          handlersMutex.Signal();
        } else
            delete handler;
    }
}

void OpalH224Handler::DeleteHandlers()
{
    PWaitAndSignal m(handlersMutex);

    DeleteObjectsInMap(m_h224Handlers);
}

void OpalH224Handler::StartTransmit()
{
  PWaitAndSignal m(transmitMutex);
    
  if(canTransmit == TRUE) {
    return;
  }
    
  canTransmit = TRUE;
    
  transmitFrame = new RTP_DataFrame(300);
  
  // Use payload code 100 as this seems to be common to other implementations
  transmitFrame->SetPayloadType((RTP_DataFrame::PayloadTypes)100);
  transmitBitIndex = 7;
  transmitStartTime = new PTime();
    
  SendClientList();
  SendExtraCapabilities();
}

void OpalH224Handler::StopTransmit()
{
  PWaitAndSignal m(transmitMutex);
    
  delete transmitStartTime;
  transmitStartTime = NULL;
    
  canTransmit = FALSE;
}

void OpalH224Handler::StartReceive()
{
  if(receiverThread != NULL) {
    PTRACE(5, "H.224 handler is already receiving");
    return;
  }
    
  receiverThread = CreateH224ReceiverThread();
  receiverThread->Resume();
}

void OpalH224Handler::StopReceive()
{
  if(receiverThread != NULL) {
    receiverThread->Close();
  }
}

int CalculateClientListSize(const std::map<BYTE,H224_Handler*> & handlers)
{
    int i = 3;
    for (std::map<BYTE,H224_Handler*>::const_iterator it = handlers.begin(); it != handlers.end(); ++it) {
        BYTE clientID = it->first;
        if(clientID == 0x7e) { // extended client ID
          i += 2;
        } else if(clientID == 0x7f) { // non-standard client ID
          i += 6;
        } else { // standard client ID 
          i++;
        }
    }
    return i;
}

PBoolean OpalH224Handler::SendClientList()
{

  if(canTransmit == FALSE)
    return false;

  int count = m_h224Handlers.size();
  if (!count)
    return false;
    
  H224_Frame h224Frame = H224_Frame(CalculateClientListSize(m_h224Handlers));
  h224Frame.SetHighPriority(TRUE);
  h224Frame.SetDestinationTerminalAddress(H224_BROADCAST);
  h224Frame.SetSourceTerminalAddress(H224_BROADCAST);
    
  // CME frame
  h224Frame.SetClientID(0x00);
    
  // Begin and end of sequence
  h224Frame.SetBS(TRUE);
  h224Frame.SetES(TRUE);
  h224Frame.SetC1(FALSE);
  h224Frame.SetC0(FALSE);
  h224Frame.SetSegmentNumber(0);
    
  BYTE *ptr = h224Frame.GetClientDataPtr();
    
  ptr[0] = 0x01; // Client list code
  ptr[1] = 0x00; // Message code
  ptr[2] = (BYTE)(count >> 2); // client count
  int i = 3;
    for (std::map<BYTE,H224_Handler*>::iterator it = m_h224Handlers.begin(); it != m_h224Handlers.end(); ++it) {
        if (it->second->IsActive(sessionDirection)) {
            BYTE clientID = it->first;
            ptr[i] = (0x80 | clientID);
            if(clientID == 0x7e) { // extended client ID
              i += 2;
            } else if(clientID == 0x7f) { // non-standard client ID
              i += 6;
            } else { // standard client ID 
              i++;
            }
        }
    }
  TransmitFrame(h224Frame);
    
  return TRUE;
}

PBoolean OpalH224Handler::SendExtraCapabilities()
{
  if(canTransmit == FALSE)
    return FALSE;

  handlersMutex.Wait();
  for (std::map<BYTE,H224_Handler*>::iterator it = m_h224Handlers.begin(); it != m_h224Handlers.end(); ++it) {
    if (it->second->IsActive(sessionDirection)) 
        it->second->SendExtraCapabilities();
  }
  handlersMutex.Signal();
    
  return TRUE;
}

PBoolean OpalH224Handler::SendExtraCapabilitiesMessage(BYTE clientID, 
                                                   BYTE *data, PINDEX length)
{    
  PWaitAndSignal m(transmitMutex);
    
  if(canTransmit == FALSE)
    return FALSE;
    
  H224_Frame h224Frame = H224_Frame(length+3);
  h224Frame.SetHighPriority(TRUE);
  h224Frame.SetDestinationTerminalAddress(H224_BROADCAST);
  h224Frame.SetSourceTerminalAddress(H224_BROADCAST);
    
  // use clientID zero to indicate a CME frame
  h224Frame.SetClientID(0x00);
    
  // Begin and end of sequence, rest is zero
  h224Frame.SetBS(TRUE);
  h224Frame.SetES(TRUE);
  h224Frame.SetC1(FALSE);
  h224Frame.SetC0(FALSE);
  h224Frame.SetSegmentNumber(0);
    
  BYTE *ptr = h224Frame.GetClientDataPtr();
    
  ptr[0] = 0x02; // Extra Capabilities code
  ptr[1] = 0x00; // Response Code
  ptr[2] = (0x80 | clientID); // EX CAPS and ClientID
    
  memcpy(ptr+3, data, length);
    
  TransmitFrame(h224Frame);
    
  return TRUE;    
}

PBoolean OpalH224Handler::TransmitClientFrame(BYTE clientID, H224_Frame & frame)
{

  if(canTransmit == FALSE)
    return FALSE;

  PWaitAndSignal m(transmitMutex);
    
  frame.SetClientID(clientID);
    
  TransmitFrame(frame);
    
  return TRUE;
}

PBoolean OpalH224Handler::OnReceivedFrame(H224_Frame & frame)
{
  if(frame.GetDestinationTerminalAddress() != H224_BROADCAST) {
    // only broadcast frames are handled at the moment
    PTRACE(3, "Received H.224 frame with non-broadcast address");
    return TRUE;
  }
  BYTE clientID = frame.GetClientID();
    
  if(clientID == 0x00) {
    return OnReceivedCMEMessage(frame);
  }

  PTRACE(5, "H224\tReceived frame for ClientID " << clientID);
    
  handlersMutex.Wait();
  for (std::map<BYTE,H224_Handler*>::iterator it = m_h224Handlers.begin(); it != m_h224Handlers.end(); ++it) {
      if (clientID == it->first)    {
        it->second->OnReceivedMessage(frame);
        break;
      }
  }
  handlersMutex.Signal();

    
  return TRUE;
}

PBoolean OpalH224Handler::OnReceivedCMEMessage(H224_Frame & frame)
{
  BYTE *data = frame.GetClientDataPtr();
    
  if(data[0] == 0x01) { // Client list code
    
    if(data[1] == 0x00) { // Message
      return OnReceivedClientList(frame);
        
    } else if(data[1] == 0xff) { // Command
      return OnReceivedClientListCommand();
    }
      
  } else if(data[0] == 0x02) { // Extra Capabilities code
      
    if(data[1] == 0x00) { // Message
      return OnReceivedExtraCapabilities(frame);
        
    } else if(data[1] == 0xff) {// Command
      return OnReceivedExtraCapabilitiesCommand();
    }
  }
    
  // incorrect frames are simply ignored
  return TRUE;
}

PBoolean OpalH224Handler::OnReceivedClientList(H224_Frame & frame)
{
  BYTE *data = frame.GetClientDataPtr();
    
  BYTE numberOfClients = data[2];
    
  PINDEX i = 3;
    
  while(numberOfClients > 0) {
      
    BYTE clientID = (data[i] & 0x7f);

    for (std::map<BYTE,H224_Handler*>::iterator it = m_h224Handlers.begin(); it != m_h224Handlers.end(); ++it) {
      if (clientID == it->first) {
        it->second->SetRemoteSupport();
        break;
      }
    }
        
    if(clientID == 0x7e) { // extended client ID
      i += 2;
    } else if(clientID == 0x7f) { // non-standard client ID
      i += 6;
    } else { // standard client ID 
      i++;
    }
    numberOfClients--;
  }
    
  return TRUE;
}

PBoolean OpalH224Handler::OnReceivedClientListCommand()
{
  SendClientList();
  return TRUE;
}

PBoolean OpalH224Handler::OnReceivedExtraCapabilities(H224_Frame & frame)
{
  BYTE *data = frame.GetClientDataPtr();
    
  BYTE clientID = (data[2] & 0x7f);
    
  handlersMutex.Wait();
  for (std::map<BYTE,H224_Handler*>::iterator it = m_h224Handlers.begin(); it != m_h224Handlers.end(); ++it) {
      if (clientID == it->first) {
        it->second->OnReceivedExtraCapabilities((data + 3), frame.GetClientDataSize() - 3);
        break;
      }
  }
  handlersMutex.Signal();
    
  return TRUE;
}

PBoolean OpalH224Handler::OnReceivedExtraCapabilitiesCommand()
{
  SendExtraCapabilities();
  return TRUE;
}

OpalH224ReceiverThread * OpalH224Handler::CreateH224ReceiverThread()
{
  return new OpalH224ReceiverThread(this, *session);
}

void OpalH224Handler::TransmitFrame(H224_Frame & frame)
{    
  PINDEX size = frame.GetEncodedSize();
    
  if(!frame.Encode(transmitFrame->GetPayloadPtr(), size, transmitBitIndex)) {
    PTRACE(3, "H224\tFailed to encode H.224 frame");
    return;
  }
    
  // determining correct timestamp
  PTime currentTime = PTime();
  PTimeInterval timePassed = currentTime - *transmitStartTime;
  transmitFrame->SetTimestamp((DWORD)timePassed.GetMilliSeconds() * 8);
  
  transmitFrame->SetPayloadSize(size);
  transmitFrame->SetMarker(TRUE);
  
  // TODO: Add Encryption Support - SH
  if(!session->PreWriteData(*transmitFrame) || !session->WriteData(*transmitFrame)) {
    PTRACE(3, "H224\tFailed to write encoded H.224 frame");
  } else {
    PTRACE(3, "H224\tEncoded H.224 frame sent");
  }
}

////////////////////////////////////

OpalH224ReceiverThread::OpalH224ReceiverThread(OpalH224Handler *theH224Handler, RTP_Session & session)
: PThread(10000, NoAutoDeleteThread, HighestPriority, "H.224 Receiver Thread"),
  rtpSession(session)
{
  h224Handler = theH224Handler;
  timestamp = 0;
  terminate = FALSE;
}

OpalH224ReceiverThread::~OpalH224ReceiverThread()
{
}

void OpalH224ReceiverThread::Main()
{    
  RTP_DataFrame packet = RTP_DataFrame(300);
  H224_Frame h224Frame = H224_Frame();
    
  for (;;) {
      
    inUse.Wait();
        
    if(!rtpSession.ReadBufferedData(timestamp, packet)) {
      inUse.Signal();
      return;
    }
    
    timestamp = packet.GetTimestamp();
        
    if(h224Frame.Decode(packet.GetPayloadPtr(), packet.GetPayloadSize())) {
      PBoolean result = h224Handler->OnReceivedFrame(h224Frame);

      if(result == FALSE) {
        // FALSE indicates a serious problem, therefore the thread is closed
        return;
      }
    } else {
      PTRACE(3, "Decoding of H.224 frame failed");
    }
        
    inUse.Signal();
        
    if(terminate == TRUE) {
      return;
    }
  }
}

void OpalH224ReceiverThread::Close()
{
  rtpSession.Close(TRUE);
    
  inUse.Wait();
    
  terminate = TRUE;
    
  inUse.Signal();
    
  PAssert(WaitForTermination(10000), "H224 receiver thread not terminated");
}

#endif // H323_H224

