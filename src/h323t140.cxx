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

#define new PNEW

static const char * RFC4103OID = "0.0.8.323.1.7.0";
#define T140_MAX_BIT_RATE 192  // 6 characters per second


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
       return FALSE;
    
   const PASN_ObjectId & id = capId;
   if (id.AsString() != RFC4103OID)
       return FALSE;

   if (pdu.HasOptionalField(H245_GenericCapability::e_maxBitRate)) {
      const PASN_Integer & bitRate = pdu.m_maxBitRate;
      maxBitRate = bitRate;
   }

  return TRUE;
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

   return TRUE;
}

PBoolean H323_RFC4103Capability::OnSendingPDU(H245_DataMode & /*pdu*/) const
{
   return false;
}

/////////////////////////////////////////////////////////////////////////////

H323_RFC4103Channel::H323_RFC4103Channel(H323Connection & connection,
                                   const H323Capability & capability,
                                   Directions theDirection,
                                   RTP_UDP & rtp,
                                   unsigned id)
  : H323DataChannel(connection, capability, direction, id),
    rtpSession(rtp), direction(theDirection), sessionID(id),
    rtpPayloadType((RTP_DataFrame::PayloadTypes)100)
{
  PTRACE(3, "RFC4103\tCreated logical channel for RFC4103");
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
    
  return TRUE;
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
    {
      return OnReceivedPDU(open.m_reverseLogicalChannelParameters.m_multiplexParameters, errorCode);
    }
      
  } else {
    if (open.m_forwardLogicalChannelParameters.m_multiplexParameters.GetTag() ==
            H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
    {
      return OnReceivedPDU(open.m_forwardLogicalChannelParameters.m_multiplexParameters, errorCode);
    }
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
      return FALSE;
     
    ok = true;
  }
    
  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaChannel)) {
    if (ok && direction == H323Channel::IsReceiver)
      ok = true;
    else if (!ExtractTransport(param.m_mediaChannel, TRUE, errorCode))
      return FALSE;
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
  if (!ack.HasOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters)) {
    return false;
  }
    
  if (ack.m_forwardMultiplexAckParameters.GetTag() !=
    H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters)
  {
    return false;
  }
    
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

#endif  // H323_T140
