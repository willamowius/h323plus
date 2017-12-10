/* h323t140.cxx
 *
 * Copyright (c) 2014 Spranto International Pte Ltd. All Rights Reserved.
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
 *
 * The Initial Developer of the Original Code is Spranto International Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>

#include "openh323buildopts.h"

#ifdef H323_T140

#ifdef __GNUC__
#pragma implementation "h323t140.h"
#endif

#include "h323t140.h"

#include "h323ep.h"
#include "h245.h"

#ifdef H323_H235
#include <h235/h235chan.h>
#endif

#define new PNEW

static const char * RFC4103OID = "0.0.8.323.1.7.0";
#define T140_CPS 6
#define T140_MAX_BIT_RATE 32*T140_CPS
#define RTP_PAYLOADTYPE  98
#define RTP_SENDCOUNT 3
#define RTP_SENDDELAY 300


/////////////////////////////////////////////////////////////////////////////

H323_RFC4103Capability::H323_RFC4103Capability()
  : H323DataCapability(T140_MAX_BIT_RATE)
{

}


PObject * H323_RFC4103Capability::Clone() const
{
  return new H323_RFC4103Capability(*this);
}


unsigned H323_RFC4103Capability::GetSubType() const
{
  return H245_DataApplicationCapability_application::e_genericDataCapability;
}


PString H323_RFC4103Capability::GetFormatName() const
{
  return "RFC4103";
}


H323Channel * H323_RFC4103Capability::CreateChannel(H323Connection & connection,
                                                 H323Channel::Directions direction,
                                                 unsigned sessionID,
                                const H245_H2250LogicalChannelParameters *) const
{

  RTP_Session *session;
  H245_TransportAddress addr;
  connection.GetControlChannel().SetUpTransportPDU(addr, H323Transport::UseLocalTSAP);
  session = connection.UseSession(sessionID, addr, direction);

  if(session == NULL)
    return NULL;

  return new H323_RFC4103Channel(connection, *this, direction, (RTP_UDP &)*session, sessionID);
}


PBoolean H323_RFC4103Capability::OnReceivedPDU(const H245_DataApplicationCapability & pdu)
{
  if (pdu.m_application.GetTag() != H245_DataApplicationCapability_application::e_genericDataCapability)
        return FALSE;

  maxBitRate = pdu.m_maxBitRate;
  const H245_GenericCapability & genCapability = (const H245_GenericCapability &)pdu.m_application;
  return OnReceivedPDU(genCapability);
}


PBoolean H323_RFC4103Capability::OnReceivedPDU(const H245_GenericCapability & pdu)
{

   const H245_CapabilityIdentifier & capId = pdu.m_capabilityIdentifier;

   if (capId.GetTag() != H245_CapabilityIdentifier::e_standard)
       return false;

   const PASN_ObjectId & id = capId;
   if (id.AsString() != RFC4103OID)
       return false;

   if (pdu.HasOptionalField(H245_GenericCapability::e_maxBitRate)) {
      const PASN_Integer & bitRate = pdu.m_maxBitRate;
      maxBitRate = bitRate;
   }

  return true;
}

PBoolean H323_RFC4103Capability::OnSendingPDU(H245_DataApplicationCapability & pdu) const
{
  pdu.m_maxBitRate = maxBitRate;
  pdu.m_application.SetTag(H245_DataApplicationCapability_application::e_genericDataCapability);

  H245_GenericCapability & genCapability = (H245_GenericCapability &)pdu.m_application;
  return OnSendingPDU(genCapability);
}

PBoolean H323_RFC4103Capability::OnSendingPDU(H245_GenericCapability & pdu) const
{
   H245_CapabilityIdentifier & capId = pdu.m_capabilityIdentifier;

   capId.SetTag(H245_CapabilityIdentifier::e_standard);
   PASN_ObjectId & id = capId;
   id.SetValue(RFC4103OID);

   pdu.IncludeOptionalField(H245_GenericCapability::e_maxBitRate);
   PASN_Integer & bitRate = pdu.m_maxBitRate;
   bitRate = maxBitRate;

   pdu.IncludeOptionalField(H245_GenericCapability::e_collapsing);
   pdu.m_collapsing.SetSize(1);

   H245_GenericParameter & cps = pdu.m_collapsing[0];
   cps.m_parameterIdentifier.SetTag(H245_ParameterIdentifier::e_standard);
   PASN_Integer & pId = (PASN_Integer &)cps.m_parameterIdentifier;
   pId = 0;

   cps.m_parameterValue.SetTag(H245_ParameterValue::e_unsignedMin);
   PASN_Integer & pVal = (PASN_Integer &)cps.m_parameterValue;
   pVal = T140_CPS;

   return true;
}

PBoolean H323_RFC4103Capability::OnSendingPDU(H245_DataMode & /*pdu*/) const
{
   return false;
}

/////////////////////////////////////////////////////////////////////////////

RFC4103_Frame::RFC4103_Frame()
: RTP_DataFrame(1200), m_startTime(0)
{
    SetRedundencyLevel(RTP_SENDCOUNT);
    memset(theArray+GetHeaderSize(),0,1200);
}

RFC4103_Frame::~RFC4103_Frame()
{

}

void RFC4103_Frame::AddCharacters(const PString & c)
{
    PWaitAndSignal m(m_frameMutex);

    if (m_charBuffer.back().primaryTime == 0) {
        m_charBuffer.back().characters += c;
        m_charBuffer.back().type = TextData;
    }
}

PBoolean RFC4103_Frame::MoreCharacters()
{
    PThread::Sleep(RTP_SENDDELAY);

    list<T140Data>::iterator r;
    for (r = m_charBuffer.begin(); r != m_charBuffer.end(); ++r) {
        if (r->type != Empty)
            return true;
    }
    return false;
}

PBoolean RFC4103_Frame::GetDataFrame(void * data, int & size)
{
    if (!MoreCharacters())
        return false;

    m_frameMutex.Wait();
       int payloadsize = BuildFrameData();
    m_frameMutex.Signal();

    size = payloadsize + GetHeaderSize();
    memcpy(data, *this, size);
    memset(theArray+GetHeaderSize(),0,payloadsize);

    return (size > 0);
}

void RFC4103_Frame::SetRedundencyLevel(int level)
{
    m_redundencyLevel = level;

    T140Data data;
    data.primaryTime = PTimer::Tick().GetMilliSeconds();
    for (PINDEX i=0; i < m_redundencyLevel; ++i) {
        data.sendCount = m_redundencyLevel -(i+1);
        m_charBuffer.push_back(data);
    }

}

int RFC4103_Frame::GetRedundencyLevel()
{
    return m_redundencyLevel;
}

PBoolean RFC4103_Frame::ReadDataFrame()
{
    int header = GetHeaderSize();
    int pos = header;

    while ((theArray[pos]&0x80) != 0) {
        T140Data data;
        data.primaryTime = *(PUInt16b*)&theArray[pos+1] >> 2;
        data.sendCount = *(PUInt16b*)&theArray[pos+2];
        m_charBuffer.push_back(data);
        pos+=4;
    }
    T140Data data;
    m_charBuffer.push_back(data);
    pos+=1;

    list<T140Data>::iterator r;
    for (r = m_charBuffer.begin(); r != m_charBuffer.end(); r++) {
        if (data.sendCount == 0)
            r->type = Empty;
        else if ((theArray[pos] == 0x00) && (theArray[pos+1] == 0x08)) {
            r->type = BackSpace;
            pos+=2;
        } else if ((theArray[pos] == 0x20) && (theArray[pos+1] == 0x28)) {
            r->type = NewLine;
            pos+=2;
        } else {
            r->type = TextData;
            for (PINDEX i = 0; i < r->sendCount; i+=2) {
                PWCharArray ucs2;
                ucs2[0] = theArray[pos];
                ucs2[1] = theArray[pos+1];
                r->characters += PString(ucs2).Left(1);
                pos+=2;
            }
        }
    }
    return (m_charBuffer.size() > 0);
}

int RFC4103_Frame::BuildFrameData()
{
    DWORD timeDiff;
    PInt64 now = PTimer::Tick().GetMilliSeconds();

    if (m_startTime == 0) {
        m_startTime = now;
        timeDiff = 0;
    } else
        timeDiff = (DWORD)now - m_startTime;

    SetTimestamp(timeDiff);

    int header = GetHeaderSize();
    int pos = header;
    int pay = header + 9;

    list<T140Data>::iterator r;
    for (r = m_charBuffer.begin(); r != m_charBuffer.end(); r++) {
        // Add the header
        if (r->primaryTime > 0) {
            if (r->type == Empty) {
                r->primaryTime = now - RTP_SENDDELAY;
                *(PUInt16b*)&theArray[pos+2] = (DWORD)0;
            } else if (r->type == TextData)
                *(PUInt16b*)&theArray[pos+2] = (DWORD)r->characters.GetSize()*2;
            else
                *(PUInt16b*)&theArray[pos+2] = (DWORD)2;

            *(PUInt16b*)&theArray[pos+1] = (DWORD)(now - r->primaryTime) << 2;
            theArray[pos] &= 0x7f;
            theArray[pos] |= (int)100;
            pos+=4;
        } else {
            theArray[pos] |= 0x80;
            theArray[pos] |= (int)100;
            r->primaryTime = now;
            pos+=1;
        }

        // Add the payload
        switch (r->type) {
            case Empty :
                break;
            case BackSpace :
                theArray[pay] = 0x00;
                theArray[pay+1] = 0x08;
                pay+=2;
                break;
            case NewLine :
                theArray[pay] = 0x20;
                theArray[pay+1] = 0x28;
                pay+=2;
                break;
            default:
                PString str = r->characters;
                PWCharArray ucs2;
                for (PINDEX i=0; i < str.GetLength(); ++i) {
                    ucs2 = str.Mid(i,1).AsUCS2();
                    theArray[pay] = ucs2[0];
                    theArray[pay+1] = ucs2[1];
                    pay+=2;
                }
                break;
        }
        r->sendCount++;
    }

    // reset Values
    if (m_charBuffer.front().sendCount == m_redundencyLevel) {
        m_charBuffer.pop_front();
        T140Data data;
        m_charBuffer.push_back(data);
    }
    return pay;
}


/////////////////////////////////////////////////////////////////////////////

H323_RFC4103Handler::H323_RFC4103Handler(H323Channel::Directions dir, H323Connection & connection, unsigned sessionID)
:   session(NULL), receiverThread(NULL)
#ifdef H323_H235
    ,secChannel(NULL)
#endif
{
  H245_TransportAddress addr;
  connection.GetControlChannel().SetUpTransportPDU(addr, H323Transport::UseLocalTSAP);
  session = connection.UseSession(sessionID,addr,H323Channel::IsBidirectional);
}

H323_RFC4103Handler::~H323_RFC4103Handler()
{

}

H323_RFC4103ReceiverThread * H323_RFC4103Handler::CreateRFC4103ReceiverThread()
{
  return new H323_RFC4103ReceiverThread(this, *session);
}

#ifdef H323_H235
void H323_RFC4103Handler::AttachSecureChannel(H323SecureChannel * channel)
{
    secChannel = channel;
}
#endif

void H323_RFC4103Handler::StartTransmit()
{
  PWaitAndSignal m(transmitMutex);

}

void H323_RFC4103Handler::StopTransmit()
{
  PWaitAndSignal m(transmitMutex);

}

void H323_RFC4103Handler::StartReceive()
{
  if(receiverThread != NULL) {
    PTRACE(5, "H.224 handler is already receiving");
    return;
  }

  receiverThread = CreateRFC4103ReceiverThread();
  receiverThread->Resume();
}

void H323_RFC4103Handler::StopReceive()
{
  if(receiverThread != NULL)
    receiverThread->Close();
}

PBoolean H323_RFC4103Handler::OnReadFrame(RTP_DataFrame & frame)
{
#ifdef H323_H235
    if (secChannel)
        return secChannel->ReadFrame(frame);
    else
#endif
        return true;
}

PBoolean H323_RFC4103Handler::OnWriteFrame(RTP_DataFrame & frame)
{
#ifdef H323_H235
    if (secChannel)
        return secChannel->WriteFrame(frame);
    else
#endif
        return true;
}

void H323_RFC4103Handler::TransmitFrame(RFC4103_Frame & frame, PBoolean replay)
{

}

/////////////////////////////////////////////////////////////////////////////

H323_RFC4103ReceiverThread::H323_RFC4103ReceiverThread(H323_RFC4103Handler *handler, RTP_Session & session)
: PThread(10000, AutoDeleteThread, NormalPriority, "RFC4103 Receiver Thread"),
  rfc4103Handler(handler), rtpSession(session)
{

}

H323_RFC4103ReceiverThread::~H323_RFC4103ReceiverThread()
{

}

void H323_RFC4103ReceiverThread::Main()
{

}

void H323_RFC4103ReceiverThread::Close()
{

}

/////////////////////////////////////////////////////////////////////////////

H323_RFC4103Channel::H323_RFC4103Channel(H323Connection & connection,
                                   const H323Capability & capability,
                                   Directions theDirection,
                                   RTP_UDP & rtp,
                                   unsigned id)
  : H323DataChannel(connection, capability, direction, id),
    rtpSession(rtp), direction(theDirection), sessionID(id),
    rtpCallbacks(*(H323_RTP_Session *)rtp.GetUserData()), rfc4103Handler(NULL),
#ifdef H323_H235
    secChannel(NULL),
#endif
    rtpPayloadType((RTP_DataFrame::PayloadTypes)98)
{
  PTRACE(3, "RFC4103\tCreated logical channel for RFC4103");
}

PBoolean H323_RFC4103Channel::Open()
{
  return H323Channel::Open();
}

PBoolean H323_RFC4103Channel::Start()
{
  if (!Open())
    return false;

  PTRACE(4,"RFC4103\tStarting RFC4103 " << (direction == H323Channel::IsTransmitter ? "Transmitter" : "Receiver") << " Channel");

  if (!rfc4103Handler)
      rfc4103Handler = connection.CreateRFC4103ProtocolHandler(direction,sessionID);

  if (!rfc4103Handler) {
      PTRACE(4,"RFC4103\tError starting " << (direction == H323Channel::IsTransmitter ? "Transmitter" : "Receiver"));
      return false;
  }

#ifdef H323_H235
  if (secChannel)
      rfc4103Handler->AttachSecureChannel(secChannel);
#endif

  if(direction == H323Channel::IsReceiver) {
    rfc4103Handler->StartReceive();
  }    else {
    rfc4103Handler->StartTransmit();
  }

  return true;
}

void H323_RFC4103Channel::Close()
{
  if(terminating) {
    return;
  }

  if(rfc4103Handler != NULL) {

    if(direction == H323Channel::IsReceiver) {
      rfc4103Handler->StopReceive();
    } else {
      rfc4103Handler->StopTransmit();
    }

    delete rfc4103Handler;
  }

}

void H323_RFC4103Channel::Receive()
{
  HandleChannel();
}


void H323_RFC4103Channel::Transmit()
{
  HandleChannel();
}

PBoolean H323_RFC4103Channel::SetDynamicRTPPayloadType(int newType)
{
  if (newType == -1)
    return true;

  if (newType < RTP_DataFrame::DynamicBase || newType >= RTP_DataFrame::IllegalPayloadType)
    return true;

  if (rtpPayloadType < RTP_DataFrame::DynamicBase)
    return true;

  rtpPayloadType = (RTP_DataFrame::PayloadTypes)newType;

  return true;
}

void H323_RFC4103Channel::HandleChannel()
{
  PTRACE(2, "RFC4103\tThread started.");

  connection.CloseLogicalChannelNumber(number);

  PTRACE(2, "RFC4103\tThread ended");
}


PBoolean H323_RFC4103Channel::OnSendingPDU(H245_OpenLogicalChannel & open) const
{
  open.m_forwardLogicalChannelNumber = (unsigned)number;

  if (open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
    open.m_reverseLogicalChannelParameters.IncludeOptionalField(
        H245_OpenLogicalChannel_reverseLogicalChannelParameters::e_multiplexParameters);

    open.m_reverseLogicalChannelParameters.m_multiplexParameters.SetTag(
        H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters);

    return OnSendingPDU(open.m_reverseLogicalChannelParameters.m_multiplexParameters);

  } else {
    open.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(
        H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters);

    return OnSendingPDU(open.m_forwardLogicalChannelParameters.m_multiplexParameters);
  }
}


PBoolean H323_RFC4103Channel::OnSendingPDU(H245_H2250LogicalChannelParameters & param) const
{
  param.m_sessionID = sessionID;

  param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaGuaranteedDelivery);
  param.m_mediaGuaranteedDelivery = FALSE;

  // unicast must have mediaControlChannel
  if (rtpSession.GetLocalControlPort() > 0) {
      H323TransportAddress mediaControlAddress(rtpSession.GetLocalAddress(), rtpSession.GetLocalControlPort());
      param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel);
      mediaControlAddress.SetPDU(param.m_mediaControlChannel);
  }

  if (direction == H323Channel::IsReceiver && rtpSession.GetLocalDataPort() > 0) {
    // set mediaChannel
    H323TransportAddress mediaAddress(rtpSession.GetLocalAddress(), rtpSession.GetLocalDataPort());
    param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
    mediaAddress.SetPDU(param.m_mediaChannel);
  }

  // Set dynamic payload type, if is one
  int rtpPayloadType = GetDynamicRTPPayloadType();

  if (rtpPayloadType >= RTP_DataFrame::DynamicBase && rtpPayloadType < RTP_DataFrame::IllegalPayloadType) {
    param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_dynamicRTPPayloadType);
    param.m_dynamicRTPPayloadType = rtpPayloadType;
  }

  return true;
}


void H323_RFC4103Channel::OnSendOpenAck(const H245_OpenLogicalChannel & openPDU,
                                     H245_OpenLogicalChannelAck & ack) const
{
  PTRACE(3, "RFC4103\tOnSendOpenAck");

  // set forwardMultiplexAckParameters option
  ack.IncludeOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters);

  // select H225 choice
  ack.m_forwardMultiplexAckParameters.SetTag(
    H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters);

  // get H225 params
  H245_H2250LogicalChannelAckParameters & param = ack.m_forwardMultiplexAckParameters;

  // set session ID
  param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID);
  const H245_H2250LogicalChannelParameters & openparam =
      openPDU.m_forwardLogicalChannelParameters.m_multiplexParameters;

  // Set Generic information
  if (connection.OnSendingOLCGenericInformation(GetSessionID(), ack.m_genericInformation,true))
       ack.IncludeOptionalField(H245_OpenLogicalChannel::e_genericInformation);

  unsigned sessionID = openparam.m_sessionID;
  param.m_sessionID = sessionID;

  OnSendOpenAck(param);
}


void H323_RFC4103Channel::OnSendOpenAck(H245_H2250LogicalChannelAckParameters & param) const
{
    // set mediaControlChannel
    if (rtpSession.GetLocalControlPort() > 0) {
        H323TransportAddress mediaControlAddress(rtpSession.GetLocalAddress(), rtpSession.GetLocalControlPort());
        param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaControlChannel);
        mediaControlAddress.SetPDU(param.m_mediaControlChannel);
    }

    // set mediaChannel
    if (rtpSession.GetLocalDataPort() > 0) {
        H323TransportAddress mediaAddress(rtpSession.GetLocalAddress(), rtpSession.GetLocalDataPort());
        param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
        mediaAddress.SetPDU(param.m_mediaChannel);
    }

    // Set dynamic payload type, if is one
    int rtpPayloadType = GetDynamicRTPPayloadType();
    if (rtpPayloadType >= RTP_DataFrame::DynamicBase && rtpPayloadType < RTP_DataFrame::IllegalPayloadType) {
        param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_dynamicRTPPayloadType);
        param.m_dynamicRTPPayloadType = rtpPayloadType;
    }
}



PBoolean H323_RFC4103Channel::OnReceivedPDU(const H245_OpenLogicalChannel & open,
                                     unsigned & errorCode)
{
  if (direction == H323Channel::IsReceiver)
    number = H323ChannelNumber(open.m_forwardLogicalChannelNumber, TRUE);

  PBoolean reverse = open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
  const H245_DataType & dataType = reverse ? open.m_reverseLogicalChannelParameters.m_dataType
                                           : open.m_forwardLogicalChannelParameters.m_dataType;

  if (!capability->OnReceivedPDU(dataType, direction)) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeNotSupported;
    return false;
  }

  if (reverse) {
    if (open.m_reverseLogicalChannelParameters.m_multiplexParameters.GetTag() ==
        H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
            return OnReceivedPDU(open.m_reverseLogicalChannelParameters.m_multiplexParameters, errorCode);
  } else {
    if (open.m_forwardLogicalChannelParameters.m_multiplexParameters.GetTag() ==
        H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
            return OnReceivedPDU(open.m_forwardLogicalChannelParameters.m_multiplexParameters, errorCode);
  }

  errorCode = H245_OpenLogicalChannelReject_cause::e_unsuitableReverseParameters;
  return false;
}


PBoolean H323_RFC4103Channel::OnReceivedPDU(const H245_H2250LogicalChannelParameters & param,
                           unsigned & errorCode)
{
  if (param.m_sessionID != sessionID) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_invalidSessionID;
    return false;
  }

  PBoolean ok = false;

  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel)) {
    if (!ExtractTransport(param.m_mediaControlChannel, FALSE, errorCode))
      return false;

    ok = true;
  }

  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaChannel)) {
    if (ok && direction == H323Channel::IsReceiver)
      ok = true;
    else if (!ExtractTransport(param.m_mediaChannel, TRUE, errorCode))
      return false;
  }

  if (IsMediaTunneled())
      ok = true;

  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_dynamicRTPPayloadType))
    SetDynamicRTPPayloadType(param.m_dynamicRTPPayloadType);

  if (ok)
    return true;

  errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
  return false;
}


PBoolean H323_RFC4103Channel::OnReceivedAckPDU(const H245_OpenLogicalChannelAck & ack)
{
  PTRACE(3, "RFC4103\tOnReceivedAckPDU");
  if (!ack.HasOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters))
    return false;

  if (ack.m_forwardMultiplexAckParameters.GetTag() !=
        H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters)
            return false;

  return OnReceivedAckPDU(ack.m_forwardMultiplexAckParameters);
}

PBoolean H323_RFC4103Channel::OnReceivedAckPDU(const H245_H2250LogicalChannelAckParameters & param)
{
  if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID)) {
    PTRACE(1, "RFC4103\tNo session specified");
  }

  if (!IsMediaTunneled()) {
      if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaControlChannel))
        return false;

      unsigned errorCode;
      if (!ExtractTransport(param.m_mediaControlChannel, false, errorCode))
        return false;

      if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel))
        return false;

      if (!ExtractTransport(param.m_mediaChannel, true, errorCode))
        return false;
  }

  if (param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_dynamicRTPPayloadType)) {
    SetDynamicRTPPayloadType(param.m_dynamicRTPPayloadType);
  }

  return false;
}


PBoolean H323_RFC4103Channel::ExtractTransport(const H245_TransportAddress & pdu,
                                                 PBoolean isDataPort,
                                                unsigned & errorCode)
{
  if (pdu.GetTag() != H245_TransportAddress::e_unicastAddress) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_multicastChannelNotAllowed;
    return false;
  }

  H323TransportAddress transAddr = pdu;

  PIPSocket::Address ip;
  WORD port = 0;
  if (transAddr.GetIpAndPort(ip, port))
    return rtpSession.SetRemoteSocketInfo(ip, port, isDataPort);

  return false;
}

void H323_RFC4103Channel::SetAssociatedChannel(H323Channel * channel)
{
#ifdef H323_H235
    secChannel = (H323SecureChannel *)channel;
#endif
}

#endif  // H323_T140
