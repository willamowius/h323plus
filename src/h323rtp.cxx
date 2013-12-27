/*
 * h323rtp.cxx
 *
 * H.323 RTP protocol handler
 *
 * Open H323 Library
 *
 * Copyright (c) 1999-2000 Equivalence Pty. Ltd.
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
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "h323rtp.h"
#endif

#include "h323rtp.h"

#include "h323ep.h"
#include "h323pdu.h"


#define new PNEW


/////////////////////////////////////////////////////////////////////////////

H323_RTP_Session::H323_RTP_Session(const H323Connection & conn)
  : connection(conn)
{
}


void H323_RTP_Session::OnTxStatistics(const RTP_Session & session) const
{
  //connection.OnRTPStatistics(session);
}


void H323_RTP_Session::OnRxStatistics(const RTP_Session & session) const
{
   connection.OnRTPStatistics(session);
}

void H323_RTP_Session::OnFinalStatistics(const RTP_Session & session) const
{
   connection.OnRTPFinalStatistics(session);
}

void H323_RTP_Session::OnRxSenderReport(unsigned sessionID,
                                        const RTP_Session::SenderReport & send,
                                        const RTP_Session::ReceiverReportArray & recv
                                        ) const
{
  connection.OnRxSenderReport(sessionID,send,recv);
}

/////////////////////////////////////////////////////////////////////////////

H323_RTP_UDP::H323_RTP_UDP(const H323Connection & conn,
                                        RTP_UDP & rtp_udp,
                                        RTP_QOS * rtpQos)
  : H323_RTP_Session(conn),
    rtp(rtp_udp)
{
  const H323Transport & transport = connection.GetControlChannel();
  PIPSocket::Address localAddress;
  transport.GetLocalAddress().GetIpAddress(localAddress);

  H323EndPoint & endpoint = connection.GetEndPoint();

  PIPSocket::Address remoteAddress;
  transport.GetRemoteAddress().GetIpAddress(remoteAddress);

#ifdef P_STUN
  PNatMethod * meth = NULL;
  if (conn.HasNATSupport()) {
      meth = conn.GetPreferedNatMethod(remoteAddress);
      if (meth != NULL) {
#if PTLIB_VER >= 2130
         PString name = meth->GetMethodName(); 
#else
         PString name = meth->GetName();
#endif
         PTRACE(4, "RTP\tNAT Method " << name << " selected for call.");
      }
  }
#endif

  WORD firstPort = endpoint.GetRtpIpPortPair();
  WORD nextPort = firstPort;
  while (!rtp.Open(localAddress,
                   nextPort, nextPort,
                   endpoint.GetRtpIpTypeofService(),
                   conn,
#ifdef P_STUN
                   meth,
#else
                   NULL,
#endif
                   rtpQos)) {
    nextPort = endpoint.GetRtpIpPortPair();
    if (nextPort == firstPort)
      return;
  }

  localAddress = rtp.GetLocalAddress();
  endpoint.InternalTranslateTCPAddress(localAddress, remoteAddress, &conn);
  rtp.SetLocalAddress(localAddress);
}

unsigned H323_RTP_UDP::GetSessionID() const
{
    return rtp.GetSessionID();
}

PBoolean H323_RTP_UDP::OnSendingPDU(const H323_RTPChannel & channel,
                                H245_H2250LogicalChannelParameters & param) const
{
  PTRACE(3, "RTP\tOnSendingPDU");

  param.m_sessionID = rtp.GetSessionID();

  param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaGuaranteedDelivery);
  param.m_mediaGuaranteedDelivery = FALSE;

  // unicast must have mediaControlChannel
  if (rtp.GetLocalDataPort() > 0) {  // if a valid Data port
      param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel);
      H323TransportAddress mediaControlAddress(rtp.GetLocalAddress(), rtp.GetLocalControlPort());
      mediaControlAddress.SetPDU(param.m_mediaControlChannel);
  }

  if ((channel.GetDirection() == H323Channel::IsReceiver) && rtp.GetLocalDataPort() > 0) {
    // set mediaChannel
    param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
    H323TransportAddress mediaAddress(rtp.GetLocalAddress(), rtp.GetLocalDataPort());
    mediaAddress.SetPDU(param.m_mediaChannel);
  }

  H323Codec * codec = channel.GetCodec();

#ifdef H323_AUDIO_CODECS
  // Set flag for we are going to stop sending audio on silence
  if (codec != NULL &&
      PIsDescendant(codec, H323AudioCodec) &&
      channel.GetDirection() != H323Channel::IsReceiver) {
    param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_silenceSuppression);
    param.m_silenceSuppression = ((H323AudioCodec*)codec)->GetSilenceDetectionMode() != H323AudioCodec::NoSilenceDetection;
  }
#endif

  // Set dynamic payload type, if is one
  RTP_DataFrame::PayloadTypes rtpPayloadType = channel.GetRTPPayloadType();
  if (rtpPayloadType >= RTP_DataFrame::DynamicBase && rtpPayloadType <= RTP_DataFrame::MaxPayloadType) {
    param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_dynamicRTPPayloadType);
    param.m_dynamicRTPPayloadType = rtpPayloadType;
  }

  // Set the media packetization field if have an option to describe it.
  if (codec != NULL) {
    param.m_mediaPacketization.SetTag(H245_H2250LogicalChannelParameters_mediaPacketization::e_rtpPayloadType);
    if (H323SetRTPPacketization(param.m_mediaPacketization, codec->GetMediaFormat(), rtpPayloadType))
      param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaPacketization);
  }

  // GQoS
#if P_QOS
  if (connection.H245QoSEnabled() && WriteTransportCapPDU(param.m_transportCapability,channel)) {
        param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_transportCapability);
  }
#endif
  return TRUE;
}

PBoolean H323_RTP_UDP::OnSendingAltPDU(const H323_RTPChannel & channel,
                H245_ArrayOf_GenericInformation & generic) const
{
        return connection.OnSendingOLCGenericInformation(channel.GetSessionID(),generic,false);
}

void H323_RTP_UDP::OnSendingAckPDU(const H323_RTPChannel & channel,
                                   H245_H2250LogicalChannelAckParameters & param) const
{
  PTRACE(3, "RTP\tOnSendingAckPDU");

  if (rtp.GetLocalDataPort() > 0) { // if a valid Data Port
      // set mediaControlChannel
      param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaControlChannel);
      H323TransportAddress mediaControlAddress(rtp.GetLocalAddress(), rtp.GetLocalControlPort());
      mediaControlAddress.SetPDU(param.m_mediaControlChannel);

      // set mediaChannel
      param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
      H323TransportAddress mediaAddress(rtp.GetLocalAddress(), rtp.GetLocalDataPort());
      mediaAddress.SetPDU(param.m_mediaChannel);
  }

  // Set dynamic payload type, if is one
  int rtpPayloadType = channel.GetRTPPayloadType();
  if (rtpPayloadType >= RTP_DataFrame::DynamicBase && rtpPayloadType <= RTP_DataFrame::MaxPayloadType) {
    param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_dynamicRTPPayloadType);
    param.m_dynamicRTPPayloadType = rtpPayloadType;
  }
}

PBoolean H323_RTP_UDP::ExtractTransport(const H245_TransportAddress & pdu,
                                    PBoolean isDataPort,
                                    unsigned & errorCode)
{
  if (pdu.GetTag() != H245_TransportAddress::e_unicastAddress) {
    PTRACE(1, "RTP_UDP\tOnly unicast supported at this time");
    errorCode = H245_OpenLogicalChannelReject_cause::e_multicastChannelNotAllowed;
    return FALSE;
  }

  H323TransportAddress transAddr = pdu;

  PIPSocket::Address ip;
  WORD port = 0;
  if (transAddr.GetIpAndPort(ip, port))
    return rtp.SetRemoteSocketInfo(ip, port, isDataPort);

  return FALSE;
}


PBoolean H323_RTP_UDP::OnReceivedPDU(H323_RTPChannel & channel,
                                 const H245_H2250LogicalChannelParameters & param,
                                 unsigned & errorCode)
{
  if (param.m_sessionID != rtp.GetSessionID()) {
    PTRACE(1, "RTP_UDP\tOpen of " << channel << " with invalid session: " << param.m_sessionID);
    errorCode = H245_OpenLogicalChannelReject_cause::e_invalidSessionID;
    return FALSE;
  }

  PBoolean ok = FALSE;

  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel)) {
    if (!ExtractTransport(param.m_mediaControlChannel, FALSE, errorCode)) {
      PTRACE(1, "RTP_UDP\tFailed to extract mediaControl transport for " << channel);
      return FALSE;
    }
    ok = TRUE;
  }

  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaChannel)) {
    if (ok && channel.GetDirection() == H323Channel::IsReceiver)
      PTRACE(3, "RTP_UDP\tIgnoring media transport for " << channel);
    else if (!ExtractTransport(param.m_mediaChannel, TRUE, errorCode)) {
      PTRACE(1, "RTP_UDP\tFailed to extract media transport for " << channel);
      return FALSE;
    }
    ok = TRUE;
  }

  if (channel.IsMediaTunneled())
     ok = TRUE;

  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_dynamicRTPPayloadType))
    channel.SetDynamicRTPPayloadType(param.m_dynamicRTPPayloadType);

  H323Codec * codec = channel.GetCodec();

  if (codec != NULL &&
      param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaPacketization) &&
      param.m_mediaPacketization.GetTag() == H245_H2250LogicalChannelParameters_mediaPacketization::e_rtpPayloadType)
    H323GetRTPPacketization(codec->GetWritableMediaFormat(), param.m_mediaPacketization);

  // GQoS
#if P_QOS
  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_transportCapability) && connection.H245QoSEnabled()) {
     H245_TransportCapability trans = param.m_transportCapability;
        ReadTransportCapPDU(trans,channel);
  }
#endif

  if (ok)
    return TRUE;

  PTRACE(1, "RTP_UDP\tNo mediaChannel or mediaControlChannel specified for " << channel);
  errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
  return FALSE;
}

PBoolean H323_RTP_UDP::OnReceivedAckPDU(H323_RTPChannel & channel,
                                    const H245_H2250LogicalChannelAckParameters & param)
{
  if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID)) {
    PTRACE(1, "RTP_UDP\tNo session specified");
  }

  if (param.m_sessionID != rtp.GetSessionID()) {
    if (rtp.GetSessionID() == 0) {
     if (param.m_sessionID > 3) {
       //update SessionID of the channel with the new one
       //fix for Tandberg as it allows open only with SessioID=0, but then in OLC ACK sends Session ID 32 up
       PTRACE(2, "RTP_UDP\tAck for invalid session: " << param.m_sessionID << "  Change the LC SessionID: "<< rtp.GetSessionID() << "  to " << param.m_sessionID);
       rtp.SetSessionID(param.m_sessionID);
     }
    } else {
       PTRACE(1, "RTP_UDP\tAck for invalid session: " << param.m_sessionID);
    }
  }

  if (!channel.IsMediaTunneled()) {
      if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaControlChannel)) {
        PTRACE(1, "RTP_UDP\tNo mediaControlChannel specified");
        return FALSE;
      }
      unsigned errorCode;
      if (!ExtractTransport(param.m_mediaControlChannel, FALSE, errorCode))
        return FALSE;

      if (!channel.IsMediaTunneled() && !param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel)) {
        PTRACE(1, "RTP_UDP\tNo mediaChannel specified");
        return FALSE;
      }
      if (!ExtractTransport(param.m_mediaChannel, TRUE, errorCode))
        return FALSE;
  }

  if (param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_dynamicRTPPayloadType))
    channel.SetDynamicRTPPayloadType(param.m_dynamicRTPPayloadType);

  return TRUE;
}

PBoolean H323_RTP_UDP::OnReceivedAckAltPDU(H323_RTPChannel & channel,
      const H245_ArrayOf_GenericInformation & alternate)
{
    return connection.OnReceiveOLCGenericInformation(channel.GetSessionID(),alternate, true);
}

void H323_RTP_UDP::OnSendRasInfo(H225_RTPSession & info)
{
  info.m_sessionId = rtp.GetSessionID();
  info.m_ssrc = rtp.GetSyncSourceOut();
  info.m_cname = rtp.GetCanonicalName();

  const H323Transport & transport = connection.GetControlChannel();

  transport.SetUpTransportPDU(info.m_rtpAddress.m_recvAddress, rtp.GetLocalDataPort());
  H323TransportAddress ta1(rtp.GetRemoteAddress(), rtp.GetRemoteDataPort());
  ta1.SetPDU(info.m_rtpAddress.m_sendAddress);

  transport.SetUpTransportPDU(info.m_rtcpAddress.m_recvAddress, rtp.GetLocalControlPort());
  H323TransportAddress ta2(rtp.GetRemoteAddress(), rtp.GetRemoteDataPort());
  ta2.SetPDU(info.m_rtcpAddress.m_sendAddress);
}

#if P_QOS
PBoolean H323_RTP_UDP::WriteTransportCapPDU(H245_TransportCapability & cap, 
                                            const H323_RTPChannel & channel) const
{
    cap.IncludeOptionalField(H245_TransportCapability::e_mediaChannelCapabilities); 
    H245_ArrayOf_MediaChannelCapability & mediaCaps = cap.m_mediaChannelCapabilities;
    mediaCaps.SetSize(1);
    H245_MediaChannelCapability & mediaCap = mediaCaps[0];
    mediaCap.IncludeOptionalField(H245_MediaChannelCapability::e_mediaTransport);
    H245_MediaTransportType & transport = mediaCap.m_mediaTransport;
    if (rtp.GetLocalDataPort() == 0) {
       transport.SetTag(H245_MediaTransportType::e_ip_TCP);
       return true;
    }
    transport.SetTag(H245_MediaTransportType::e_ip_UDP);

    PQoS & qos = rtp.GetQOS();
    int m_dscp = qos.GetDSCP();
    if (m_dscp == 0) 
        return true;

    cap.IncludeOptionalField(H245_TransportCapability::e_qOSCapabilities);
    H245_ArrayOf_QOSCapability & QoSs = cap.m_qOSCapabilities;

     H245_QOSCapability Cap = H245_QOSCapability();
      Cap.IncludeOptionalField(H245_QOSCapability::e_localQoS);
       PASN_Boolean & localqos = Cap.m_localQoS;
       localqos.SetValue(TRUE);

      Cap.IncludeOptionalField(H245_QOSCapability::e_dscpValue);
       PASN_Integer & dscp = Cap.m_dscpValue;
       dscp = m_dscp;

    if (PUDPSocket::SupportQoS(rtp.GetLocalAddress())) {        
      Cap.IncludeOptionalField(H245_QOSCapability::e_rsvpParameters);
      H245_RSVPParameters & rsvp = Cap.m_rsvpParameters; 

      if (channel.GetDirection() == H323Channel::IsReceiver) {   /// If Reply don't have to send body
          rtp.EnableGQoS(TRUE);
          return true;
      }
      rsvp.IncludeOptionalField(H245_RSVPParameters::e_qosMode); 
        H245_QOSMode & mode = rsvp.m_qosMode;
          if (qos.GetServiceType() == SERVICETYPE_GUARANTEED) 
             mode.SetTag(H245_QOSMode::e_guaranteedQOS); 
          else 
             mode.SetTag(H245_QOSMode::e_controlledLoad); 
        
      rsvp.IncludeOptionalField(H245_RSVPParameters::e_tokenRate); 
           rsvp.m_tokenRate = qos.GetTokenRate();
      rsvp.IncludeOptionalField(H245_RSVPParameters::e_bucketSize);
           rsvp.m_bucketSize = qos.GetTokenBucketSize();
      rsvp.IncludeOptionalField(H245_RSVPParameters::e_peakRate);
           rsvp.m_peakRate = qos.GetPeakBandwidth();
    }
    QoSs.SetSize(1);
    QoSs[0] = Cap;
    return true;
}

void H323_RTP_UDP::ReadTransportCapPDU(const H245_TransportCapability & cap,
                                                    H323_RTPChannel & channel)
{
    if (!cap.HasOptionalField(H245_TransportCapability::e_qOSCapabilities)) 
        return;    


    const H245_ArrayOf_QOSCapability QoSs = cap.m_qOSCapabilities;
    for (PINDEX i =0; i < QoSs.GetSize(); i++) {
      PQoS & qos = rtp.GetQOS();
      const H245_QOSCapability & QoS = QoSs[i];
//        if (QoS.HasOptionalField(H245_QOSCapability::e_localQoS)) {
//           PASN_Boolean & localqos = QoS.m_localQoS;
//        }
        if (QoS.HasOptionalField(H245_QOSCapability::e_dscpValue)) {
            const PASN_Integer & dscp = QoS.m_dscpValue;
            qos.SetDSCP(dscp);
        }

        if (PUDPSocket::SupportQoS(rtp.GetLocalAddress())) {
            if (!QoS.HasOptionalField(H245_QOSCapability::e_rsvpParameters)) {
                PTRACE(4,"TRANS\tDisabling GQoS");
                rtp.EnableGQoS(FALSE);  
                return;
            }
        
            const H245_RSVPParameters & rsvp = QoS.m_rsvpParameters; 
            if (channel.GetDirection() != H323Channel::IsReceiver) {
                rtp.EnableGQoS(TRUE);
                return;
            }      
            if (rsvp.HasOptionalField(H245_RSVPParameters::e_qosMode)) {
                    const H245_QOSMode & mode = rsvp.m_qosMode;
                    if (mode.GetTag() == H245_QOSMode::e_guaranteedQOS) {
                        qos.SetWinServiceType(SERVICETYPE_GUARANTEED);
                        qos.SetDSCP(PQoS::guaranteedDSCP);
                    } else {
                        qos.SetWinServiceType(SERVICETYPE_CONTROLLEDLOAD);
                        qos.SetDSCP(PQoS::controlledLoadDSCP);
                    }
            }
            if (rsvp.HasOptionalField(H245_RSVPParameters::e_tokenRate)) 
                qos.SetAvgBytesPerSec(rsvp.m_tokenRate);
            if (rsvp.HasOptionalField(H245_RSVPParameters::e_bucketSize))
                qos.SetMaxFrameBytes(rsvp.m_bucketSize);
            if (rsvp.HasOptionalField(H245_RSVPParameters::e_peakRate))
                qos.SetPeakBytesPerSec(rsvp.m_peakRate);    
        }
    }
}
#endif
/////////////////////////////////////////////////////////////////////////////
