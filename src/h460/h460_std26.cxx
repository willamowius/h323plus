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
#include <h460/h46026mgr.h>

#include <h460/h460_std17.h>

#undef PACKET_ANALYSIS
#undef LOCAL_FASTUPDATE

#define REC_FRAME_RATE       15.0
#define REC_FRAME_TIME       (1.0/REC_FRAME_RATE) * 1000
#define MAX_STACK_DESCRETION  REC_FRAME_TIME * 2
#define FAST_UPDATE_INTERVAL  REC_FRAME_TIME * 3

/////////////////////////////////////////////////////////////////////////////////

#ifdef PACKET_ANALYSIS
static void AnalysePacket(PBoolean out, int session, const RTP_DataFrame & frame)
{
    PTRACE(1, "RTP\t" << (out ? "> " : "< ")
           << " s=" << session
           << " ver=" << frame.GetVersion()
           << " pt=" << frame.GetPayloadType()
           << " psz=" << frame.GetPayloadSize()
           << " m=" << frame.GetMarker()
           << " x=" << frame.GetExtension()
           << " seq=" << frame.GetSequenceNumber()
           << " ts=" << frame.GetTimestamp()
           << " src=" << frame.GetSyncSource()
           << " ccnt=" << frame.GetContribSrcCount());
}
#endif // PACKET_ANALYSIS

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


void H460_FeatureStd26::AttachHandler(H46017Handler * m_handler)
{
    handler = m_handler;
    isSupported = true;
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
    return (handler && handler->IsH46026Tunnel()); 
}

void PNatMethod_H46026::AttachHandler(H46017Handler * m_handler)
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

void PNatMethod_H46026::SetInformationHeader(PUDPSocket & data, PUDPSocket & control, 
                                         H323Connection::SessionInformation * info)
{
    if (!handler || handler->GetEndPoint() == NULL)
        return;

    const H323Transport * transport = NULL;
    H323SignalPDU informationMsg;
    H323Connection * connection = PRemoveConst(H323Connection, info->GetConnection());
    if (connection != NULL) {
        informationMsg.BuildInformation(*connection);
        transport = connection->GetSignallingChannel();
    }

    ((H46026UDPSocket &)data).SetInformationHeader(informationMsg, transport);
    ((H46026UDPSocket &)control).SetInformationHeader(informationMsg, transport);
}

/////////////////////////////////////////////////////////////////////////////////////////////

H46026UDPSocket::H46026UDPSocket(H46017Handler & _handler, H323Connection::SessionInformation * info, bool _rtpSocket)
: m_transport(NULL), m_Session(info->GetSessionID()), rtpSocket(_rtpSocket), m_frameCount(0), m_payloadSize(0), m_frame(NULL), shutDown(false)
{
}

H46026UDPSocket::~H46026UDPSocket()
{
    shutDown = true;
    writeMutex.Wait();
      while (!rtpQueue.empty()) {
        delete rtpQueue.front();
        rtpQueue.pop();
      }
    writeMutex.Signal();
    delete m_frame;
}

void H46026UDPSocket::SetInformationHeader(const H323SignalPDU & _InformationMsg, const H323Transport * _transport)
{
    msg = _InformationMsg;
    m_transport = (H46017Transport *)PRemoveConst(H323Transport,_transport);
}

PBoolean H46026UDPSocket::WriteBuffer(const void * buf, PINDEX len)
{
    PWaitAndSignal m(writeMutex);
    
    if (!shutDown) {
         rtpQueue.push(new PBYTEArray((BYTE *)buf,len));
         return true;
    } else
         return false;
}

PBoolean H46026UDPSocket::DoPseudoRead(int & selectStatus)
{
   PAdaptiveDelay selectBlock;
   while (rtpSocket && rtpQueue.size() == 0) {
       selectBlock.Delay(2);
       if (shutDown) break;
   }

   selectStatus += ((rtpQueue.size() > 0) ? (rtpSocket ? -1 : -2) : 0);
   return rtpSocket;
}

PBoolean H46026UDPSocket::ReadFrom(void * buf, PINDEX len, Address & addr, WORD & port)
{
    addr = "0.0.0.0";
    port = 0;

    while (!shutDown) {
      if (rtpQueue.empty()) {
        PThread::Sleep(5);
        continue;
      }

      writeMutex.Wait();
        PBYTEArray * rtp = rtpQueue.front();
        rtpQueue.pop();
      writeMutex.Signal();
        buf = rtp->GetPointer();
        len = rtp->GetSize();
        break;
    }

    return true;
}

PBoolean H46026UDPSocket::WriteTo(const void * buf, PINDEX len, const Address & /*addr*/, WORD /*port*/)
{
    // Write here....
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

#endif // H323_H46026