/*
 * h460_std26.cxx
 *
 * H46026 Media Tunneling class.
 *
 * h323plus library
 *
 * Copyright (c) 2013 Spranto Australia Pty. Ltd.
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
 * The Initial Developer of the Original Code is Spranto Australia Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */


#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H46026

#include <h323ep.h>
#include <h323pdu.h>

#include <h460/h460.h> 
#include <h460/h46026.h>
#include <h460/h460_std26.h>
#include <h460/h460_std17.h>

////////////////////////////////////////////////////////////////////////////////

H460_FEATURE(Std26);

PBoolean H460_FeatureStd26::isSupported = false;
H460_FeatureStd26::H460_FeatureStd26()
: H460_FeatureStd(26), EP(NULL), CON(NULL), handler(NULL), isEnabled(false)
{
       FeatureCategory = FeatureSupported;
}

H460_FeatureStd26::~H460_FeatureStd26()
{
   isSupported = false;
}

void H460_FeatureStd26::AttachEndPoint(H323EndPoint * _ep)
{
    if (!EP)
       EP = _ep;
}

void H460_FeatureStd26::AttachConnection(H323Connection * _con)
{
    CON = _con;
    if (!EP)
        EP = &CON->GetEndPoint();
}

int H460_FeatureStd26::GetPurpose()
{
    if (isSupported)
      return FeatureSignal;
    else
      return FeatureBase;
}


void H460_FeatureStd26::AttachH46017(H46017Handler * m_handler, H323Transport * transport)
{
    handler = m_handler;

    if (PIsDescendant(transport,H46017Transport))
        ((H46017Transport *)transport)->SetTunnel(&h46026mgr);

    isSupported = true;
}

H46026Tunnel * H460_FeatureStd26::GetTunnel()
{
    return &h46026mgr;
}

PBoolean H460_FeatureStd26::FeatureAdvertised(int mtype)
{
     switch (mtype) {
        case H460_MessageType::e_admissionRequest:
        case H460_MessageType::e_admissionConfirm:
        case H460_MessageType::e_admissionReject:
        case H460_MessageType::e_setup:
        case H460_MessageType::e_callProceeding: 
        case H460_MessageType::e_alerting:
        case H460_MessageType::e_connect:
            return true;
        default:
            return false;
     }
}

PBoolean H460_FeatureStd26::OnSendAdmissionRequest(H225_FeatureDescriptor & pdu)
{
    if (!handler)  // if no h46017 handler then no H460.26 support.
        return false;

    // Build Message
    H460_FeatureStd feat = H460_FeatureStd(26); 
    pdu = feat;

    return true;
}

void H460_FeatureStd26::OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu)
{
   if (handler)
       handler->SetH46026Tunnel(true);

   CON->H46026SetMediaTunneled();
   FeatureCategory = FeatureNeeded;
   isEnabled = true;

}

//////////////////////////////////////////////////////////////////////////////////////

PBoolean H460_FeatureStd26::OnSendSetup_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!isEnabled)
        return false;

    // Build Message
    H460_FeatureStd feat = H460_FeatureStd(26); 
    pdu = feat;
   
    return true;
}

void H460_FeatureStd26::OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu)
{
   if (handler)
       handler->SetH46026Tunnel(true);

    isEnabled = true;
}

PBoolean H460_FeatureStd26::OnSendCallProceeding_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!isEnabled)
        return false;

    // Build Message
    H460_FeatureStd feat = H460_FeatureStd(26); 
    pdu = feat;

    return true;
}

void H460_FeatureStd26::OnReceiveCallProceeding_UUIE(const H225_FeatureDescriptor & pdu)
{
    // do nothing
}

PBoolean H460_FeatureStd26::OnSendAlerting_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!isEnabled)
        return false;

    // Build Message
    H460_FeatureStd feat = H460_FeatureStd(26); 
    pdu = feat;

    return true;
}

void H460_FeatureStd26::OnReceiveAlerting_UUIE(const H225_FeatureDescriptor & pdu)
{
    // do nothing
}

PBoolean H460_FeatureStd26::OnSendCallConnect_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!isEnabled)
        return false;

    // Build Message
    H460_FeatureStd feat = H460_FeatureStd(26); 
    pdu = feat;

    return true;
}

void H460_FeatureStd26::OnReceiveCallConnect_UUIE(const H225_FeatureDescriptor & pdu)
{
    // do nothing
}


////////////////////////////////////////////////

PCREATE_NAT_PLUGIN(H46026);
    
PNatMethod_H46026::PNatMethod_H46026()
: active(false), handler(NULL)
{

}

PNatMethod_H46026::~PNatMethod_H46026()
{

}

void PNatMethod_H46026::AttachEndPoint(H323EndPoint * ep)
{

   WORD portPairBase = ep->GetRtpIpPortBase();
   WORD portPairMax = ep->GetRtpIpPortMax();

// Initialise
//  ExternalAddress = 0;
  pairedPortInfo.basePort = 0;
  pairedPortInfo.maxPort = 0;
  pairedPortInfo.currentPort = 0;

// Set the Port Pair Information
  pairedPortInfo.mutex.Wait();

  pairedPortInfo.basePort = (WORD)((portPairBase+1)&0xfffe);
  if (portPairBase == 0) {
    pairedPortInfo.basePort = 0;
    pairedPortInfo.maxPort = 0;
  }
  else if (portPairMax == 0)
    pairedPortInfo.maxPort = (WORD)(pairedPortInfo.basePort+99);
  else if (portPairMax < portPairBase)
    pairedPortInfo.maxPort = portPairBase;
  else
    pairedPortInfo.maxPort = portPairMax;

  pairedPortInfo.currentPort = pairedPortInfo.basePort;

  pairedPortInfo.mutex.Signal();

}

bool PNatMethod_H46026::IsAvailable(const PIPSocket::Address&) 
{ 
    return (handler != NULL); 
}

void PNatMethod_H46026::AttachManager(H46026Tunnel * m_handler)
{
    handler = m_handler;
}

PBoolean PNatMethod_H46026::GetExternalAddress(
      PIPSocket::Address & /*externalAddress*/, /// External address of router
      const PTimeInterval & /* maxAge */         /// Maximum age for caching
      )
{
    return FALSE;
}


PBoolean PNatMethod_H46026::CreateSocketPair(
                            PUDPSocket * & socket1,
                            PUDPSocket * & socket2,
                            const PIPSocket::Address & binding,
                            void * userData
                            )
{

    H323Connection::SessionInformation * info = (H323Connection::SessionInformation *)userData;

    socket1 = new H46026UDPSocket(*handler,info,true);  /// Data 
    socket2 = new H46026UDPSocket(*handler,info,false);  /// Signal

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////

H46026UDPSocket::H46026UDPSocket(H46026Tunnel & _handler, H323Connection::SessionInformation * info, bool _rtpSocket)
: m_transport(_handler), m_crv(info->GetCallReference()), m_session(info->GetSessionID()), m_rtpSocket(_rtpSocket), m_shutdown(false)
{
}

H46026UDPSocket::~H46026UDPSocket()
{
   PWaitAndSignal m(m_rtpMutex);

   PTRACE(4, "Closing c: " << m_crv << " s: " << m_session
                << " "  << (m_rtpSocket ? "RTP" : "RTCP"));

   m_shutdown = true;
   while (!m_rtpQueue.empty()) {
       delete m_rtpQueue.front();
       m_rtpQueue.pop();
   }
}

PBoolean H46026UDPSocket::WriteBuffer(const PBYTEArray & buffer)
{
    PWaitAndSignal m(m_rtpMutex);

    if (!m_shutdown)
        m_rtpQueue.push(new PBYTEArray((const BYTE *)buffer,buffer.GetSize()));
    return true;
}

PBoolean H46026UDPSocket::DoPseudoRead(int & selectStatus)
{
   while (m_rtpSocket && m_rtpQueue.size() == 0) {
       m_selectBlock.Delay(2);
       if (m_shutdown) break;
   }

   bool isData = (!m_shutdown && m_rtpQueue.size() > 0);
   selectStatus += ( isData ? (m_rtpSocket ? -1 : -2) : 0);
   return isData;
}

PBoolean H46026UDPSocket::ReadFrom(void * buf, PINDEX len, Address & addr, WORD & port)
{
    addr = "0.0.0.0";
    port = 0;

    if (m_shutdown || m_rtpQueue.size() == 0) {
        PTRACE(4, "ReadFrom Error c: " << m_crv << " s: " << m_session << " "  << (m_rtpSocket ? "RTP" : "RTCP"));
        return false;
    }

    m_rtpMutex.Wait();
      PBYTEArray * rtp = m_rtpQueue.front();
      m_rtpQueue.pop();
    m_rtpMutex.Signal();

     buf = rtp->GetPointer();
     len = rtp->GetSize();
    return true;
}

PBoolean H46026UDPSocket::WriteTo(const void * buf, PINDEX len, const Address & /*addr*/, WORD /*port*/)
{
    // Write here....
    m_transport.FrameToSend(m_crv,m_session,m_rtpSocket,(const BYTE *)buf,len);
    return true;
}

PBoolean H46026UDPSocket::GetLocalAddress(Address & addr, WORD & port)
{
    addr = PIPSocket::GetDefaultIpAny();
    port = 0;
    return true;
}

void H46026UDPSocket::GetLocalAddress(H245_TransportAddress & add)
{
    PIPSocket::Address m_locAddr = PIPSocket::GetDefaultIpAny();
    WORD m_locPort = 0;
    H323TransportAddress ladd(m_locAddr,m_locPort);
    ladd.SetPDU(add);
}
    
void H46026UDPSocket::SetRemoteAddress(const H245_TransportAddress & add)
{
   // Ignore!
}

//////////////////////////////////////////////////////////////////////////////////////

H46026PortPairs::H46026PortPairs()
: rtp(NULL), rtcp(NULL)
{
}

H46026PortPairs::H46026PortPairs(H46026UDPSocket* _rtp, H46026UDPSocket* _rtcp)
: rtp(_rtp), rtcp(_rtcp)
{
}

//////////////////////////////////////////////////////////////////////////////////////

H46026Tunnel::H46026Tunnel()
: m_transport(NULL)
{

}
    
H46026Tunnel::~H46026Tunnel()
{
}

void H46026Tunnel::AttachTransport(H46017Transport * transport)
{
    m_transport = transport;
}

void H46026Tunnel::RegisterSocket(unsigned crv, int sessionId, H46026UDPSocket* rtp, H46026UDPSocket* rtcp)
{
    PWaitAndSignal m(m_socketMutex);

    H46026CallSocket::iterator r = m_socketMap.find(crv);
    if (r != m_socketMap.end()) {
        H46026SocketMap::iterator c = r->second.find(sessionId);
        if (c != r->second.end()) {
            return;
        }
    }
    m_socketMap[crv][sessionId] = H46026PortPairs(rtp,rtcp);
}

void H46026Tunnel::UnRegisterSocket(unsigned crv, int sessionId)
{
    PWaitAndSignal m(m_socketMutex);

    H46026CallSocket::iterator r = m_socketMap.find(crv);
    if (r != m_socketMap.end()) {
        int sz = r->second.size();
        H46026SocketMap::iterator c = r->second.find(sessionId);
        if (c != r->second.end()) {
            r->second.erase(c);
        }
        if (sz == 1) {
            m_socketMap.erase(r);
            BufferRelease(crv);
        }
    }
}

void H46026Tunnel::SignalToSend(const Q931 & _q931)
{
    SignalMsgOut(_q931);
}

void H46026Tunnel::SignalMsgIn(const Q931 & _q931)
{
  if (m_transport && !m_transport->HandleH46017PDU(_q931)) {
      PTRACE(4,"H46026\tIncoming Signal PDU not handled");
  }
}

void H46026Tunnel::RTPFrameIn(unsigned crv, PINDEX sessionId, PBoolean rtp, const PBYTEArray & data)
{
    PWaitAndSignal m(m_socketMutex);

    H46026CallSocket::iterator r = m_socketMap.find(crv);
    if (r != m_socketMap.end()) {
        H46026SocketMap::iterator c = r->second.find(sessionId);
        if (c != r->second.end()) {
            if (rtp)
                c->second.rtp->WriteBuffer(data);
            else
                c->second.rtcp->WriteBuffer(data);
        }
    }
}

void H46026Tunnel::FrameToSend(unsigned crv, int sessionId, bool rtp, const BYTE * buf, PINDEX len)
{
    H46026ChannelManager::PacketTypes type;
    if (sessionId == 1) type = H46026ChannelManager::e_Audio;
    else if (sessionId == 2) type = H46026ChannelManager::e_Video;
    else if (sessionId == 3) type = H46026ChannelManager::e_Data;
    else type = H46026ChannelManager::e_extVideo;

    RTPFrameOut(crv,type,sessionId,rtp,buf,len);
}

#endif  // H323_H46026


