/*
 * H46017.cxx
 *
 * H46017 NAT Traversal class.
 *
 * h323plus library
 *
 * Copyright (c) 2011 ISVO (Asia) Pte. Ltd.
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
 * The Initial Developer of the Original Code is ISVO (Asia) Pte. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H46017

#include <h323ep.h>
#include <h323pdu.h>
#include <h460/h460_std17.h>
#ifdef H323_H46026
 #include <h460/h460.h> 
 #include <h460/h46026.h>
#endif

#include <ptclib/pdns.h>
#include <ptclib/delaychan.h>

#ifdef H323_H46026
//#define PACKET_ANALYSIS    1
#define LOCAL_FASTUPDATE    1
#endif

#define MAX_AUDIO_FRAMES     3
#define MAX_VIDEO_PAYLOAD    4000         // 4k Frame
#define REC_FRAME_RATE       15.0
#define REC_FRAME_TIME       (1.0/REC_FRAME_RATE) * 1000
#define MAX_STACK_DESCRETION  REC_FRAME_TIME * 2
#define FAST_UPDATE_INTERVAL  REC_FRAME_TIME * 3
#define MAX_VIDEO_KBPS       384000
#define MAX_PIPE_SHAPING     MAX_VIDEO_KBPS * 0.5   //175000.0  <- Agressive
#define MAX_STACK_DELAY      350


struct LookupRecord {
  PIPSocket::Address addr;
  WORD port;
};

static PBoolean FindSRVRecords(std::vector<LookupRecord> & recs,
                    const PString & domain,
                    const PString & srv)
{
  PDNS::SRVRecordList srvRecords;
  PString srvLookupStr = srv + domain;
  PBoolean found = PDNS::GetRecords(srvLookupStr, srvRecords);
  if (found) {
    PDNS::SRVRecord * recPtr = srvRecords.GetFirst();
    while (recPtr != NULL) {
      LookupRecord rec;
      rec.addr = recPtr->hostAddress;
      rec.port = recPtr->port;
      recs.push_back(rec);
      recPtr = srvRecords.GetNext();
      PTRACE(4, "H323\tFound " << rec.addr << ":" << rec.port << " with SRV " << srv << " for domain " << domain);
    }
  } 
  return found;
}

static PBoolean FindRoutes(const PString & domain, std::vector<LookupRecord> & routes)
{
  FindSRVRecords(routes, domain, "_h323rs._tcp.");
  return routes.size() != 0;
}

static PInt64 lastFUP =0;
static PBoolean HandleH245Command(PBoolean /*remote*/, const H245_CommandMessage & pdu) 
{
    if (pdu.GetTag() != H245_CommandMessage::e_miscellaneousCommand)
        return true;

    const H245_MiscellaneousCommand & misc = pdu;
#ifndef LOCAL_FASTUPDATE
    PInt64 now = PTimer::Tick().GetMilliSeconds();
#endif
    switch (misc.m_type.GetTag()) {
        case H245_MiscellaneousCommand_type::e_videoFastUpdatePicture :
#ifdef LOCAL_FASTUPDATE
            return false;
#else
            if (now - lastFUP < FAST_UPDATE_INTERVAL) {
                PTRACE(2,"H46017\tFastPicture request blocked!");
                 return false;
            } else {
                lastFUP = now;
                PTRACE(2,"H46017\tFastPicture request...");
                return true;
            } 
#endif

        case H245_MiscellaneousCommand_type::e_videoFreezePicture :
        case H245_MiscellaneousCommand_type::e_videoFastUpdateGOB :
        case H245_MiscellaneousCommand_type::e_videoFastUpdateMB :
        case H245_MiscellaneousCommand_type::e_lostPartialPicture :
        case H245_MiscellaneousCommand_type::e_lostPicture :
        case H245_MiscellaneousCommand_type::e_videoTemporalSpatialTradeOff :
        default:
            break;
    }
    return true;
}

#ifdef H323_H46026
static void BuildFastUpdatePicture(unsigned sessionId, H323ControlPDU & pdu)
{
    H245_CommandMessage & command = pdu.Build(H245_CommandMessage::e_miscellaneousCommand);
    H245_MiscellaneousCommand & miscCommand = command;
    miscCommand.m_logicalChannelNumber = sessionId;
    miscCommand.m_type.SetTag(H245_MiscellaneousCommand_type::e_videoFastUpdatePicture);
}

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
#endif // H323_H46026

///////////////////////////////////////////////////////////////////////////////////
// Must Declare for Factory Loader.
H460_FEATURE(Std17);

PBoolean H460_FeatureStd17::isEnabled = false;
H460_FeatureStd17::H460_FeatureStd17()
: H460_FeatureStd(17), EP(NULL), CON(NULL), m_handler(NULL)
{

}

H460_FeatureStd17::~H460_FeatureStd17()
{
     isEnabled = false;
     delete m_handler;
}

void H460_FeatureStd17::AttachEndPoint(H323EndPoint * _ep)
{
    if (!EP)
       EP = _ep;
}

void H460_FeatureStd17::AttachConnection(H323Connection * _con)
{
    CON = _con;
    if (!EP)
        EP = &CON->GetEndPoint();

}

int H460_FeatureStd17::GetPurpose()	
{ 
    if (isEnabled)
      return FeatureRas;
    else
      return FeatureBase; 
}

PBoolean H460_FeatureStd17::Initialise(const PString & remoteAddr, PBoolean srv)
{

 if (!srv) {  // We are not doing SRV lookup
    H323TransportAddress rem(remoteAddr);
    if (!InitialiseTunnel(rem)) {
         PTRACE(2,"H46017\tTunnel to " << rem << " Failed!"); 
         return false;
    }
#ifdef H323_H46018
    EP->H46018Enable(false);
#endif
    isEnabled = true;
    return (m_handler && m_handler->RegisterGatekeeper());

 } else {
    std::vector<LookupRecord> routes;

    if (!FindRoutes(remoteAddr,routes)) {
        PTRACE(2,"H46017\tNo Gatekeeper SRV Records (_h323rs._tcp.) found!");
        return false;
    }

    std::vector<LookupRecord>::const_iterator r;
    for (r = routes.begin(); r != routes.end(); ++r) {
       const LookupRecord & rec = *r;
       H323TransportAddress rem(rec.addr,rec.port);

       if (!InitialiseTunnel(rem)) {
         PTRACE(2,"H46017\tTunnel to " << rem << " Failed!"); 
         continue;
       }
#ifdef H323_H46018
        EP->H46018Enable(false);
#endif
       isEnabled = true;
       if (!m_handler->RegisterGatekeeper())
           continue;

       return true;
    }
    return false;
 }


}


PBoolean H460_FeatureStd17::InitialiseTunnel(const H323TransportAddress & remoteAddr)
{
    if (!m_handler)
       m_handler = new H46017Handler(*EP, remoteAddr);

    return m_handler->CreateNewTransport();
}


///////////////////////////////////////////////////////////////////////////////////
// Listening/Keep Alive Thread

class H46017TransportThread : public PThread
{
   PCLASSINFO(H46017TransportThread, PThread)

   public:
    H46017TransportThread(H323EndPoint & endpoint, H46017Transport * transport);

   protected:
    void Main();

    H46017Transport * transport;

};

/////////////////////////////////////////////////////////////////////////////

H46017TransportThread::H46017TransportThread(H323EndPoint & ep, H46017Transport * t)
  : PThread(ep.GetSignallingThreadStackSize(), AutoDeleteThread, NormalPriority, "H46017 Answer:%0x"),
    transport(t)
{  

   transport->AttachThread(this);

// Start the Thread
   Resume();
}

void H46017TransportThread::Main()
{
  PTRACE(3, "H46017\tStarted Listening Thread");

  PBoolean ret = TRUE;
  while ((transport->IsOpen()) && (!transport->CloseTransport())) {

      ret = transport->HandleH46017SignallingChannelPDU(this);

      if (!ret && transport->CloseTransport()) {  // Closing down Instruction
          PTRACE(3, "H46017\tShutting down H46017 Thread");
          transport->ConnectionLost(TRUE);

      } else if (!ret) {   // We have a socket failure wait 1 sec and try again.
         PTRACE(3, "H46017\tConnection Lost! Retrying Connection..");
         transport->ConnectionLost(TRUE);

        if (transport->CloseTransport()) {
            PTRACE(3, "H46017\tConnection Lost");
            break;
        } else {
            PTRACE(3, "H46017\tConnection ReEstablished");
            transport->ConnectionLost(FALSE);
            ret = TRUE;            // Signal that the connection has been ReEstablished.
        }
      } 
  }

  PTRACE(3, "H46017\tTransport Closed");
}

///////////////////////////////////////////////////////////////////////////////////////



H46017Transport::H46017Transport(H323EndPoint & endpoint,
                                 PIPSocket::Address binding,
                                 H46017Handler * feat
                )    
   : H323TransportTCP(endpoint,binding), ReadTimeOut(PMaxTimeInterval), con(NULL), m_socketWrite(NULL), 
     Feature(feat), isConnected(false), remoteShutDown(false), closeTransport(false), callToken(PString())
{

}

H46017Transport::~H46017Transport()
{
    Close();
    delete m_socketWrite;
}

PBoolean FindH46017RAS(const H225_H323_UU_PDU & pdu, std::list<PBYTEArray> & ras)
{
    if (pdu.HasOptionalField(H225_H323_UU_PDU::e_genericData)) {
       const H225_ArrayOf_GenericData & data = pdu.m_genericData;
        for (PINDEX i=0; i < data.GetSize(); i++) {
            if (data[i].m_id == H460_FeatureID(17)) {
               H460_Feature feat((const H225_FeatureDescriptor &)data[i]);
               for (PINDEX i=0; i< feat.GetParameterCount(); ++i) {
                   H460_FeatureParameter & param = feat.GetFeatureParameter(i);
                   if (param.ID() == 1 && param.hasContent()) {
                     PASN_OctetString raw = param;
                     ras.push_back(raw.GetValue());
                   }
               }
            }
        }
    }
    return (ras.size() > 0);
}

PBoolean H46017Transport::WriteRasPDU(const PBYTEArray & pdu)
{
  H323SignalPDU rasPDU;
  rasPDU.BuildRasFacility();

  rasPDU.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_genericData);

  H225_ArrayOf_GenericData & gdata = rasPDU.m_h323_uu_pdu.m_genericData;
  int sz = gdata.GetSize();
  gdata.SetSize(sz+1);
  H225_GenericData & data = gdata[sz];

        H460_FeatureStd feat = H460_FeatureStd(17);
        PASN_OctetString encFrame;
        encFrame.SetValue(pdu);
        feat.Add(1,H460_FeatureContent(encFrame));
        data = feat;
        
 rasPDU.BuildQ931();

 PTRACE(6,"H46017\tSend " << rasPDU);

 h225Order::MessageHeader prior;
 prior.sessionId = 0;
 prior.priority = h225Order::Priority_High;
 prior.packTime = PTimer::Tick().GetMilliSeconds();

 return WriteTunnel(rasPDU,prior);
}

PBoolean H46017Transport::HandleH46017RAS(const H323SignalPDU & pdu)
{
    std::list<PBYTEArray> ras;
    if ((pdu.GetQ931().GetMessageType() == Q931::FacilityMsg) &&
                           FindH46017RAS(pdu.m_h323_uu_pdu,ras)) {
       H46017RasTransport * rasTransport = Feature->GetRasTransport(); 
       for (std::list<PBYTEArray>::iterator r = ras.begin(); r != ras.end(); ++r) {
           if (!rasTransport->ReceivedPDU(*r)) 
              return false;
       }
       ras.clear();
       return true;
    }
    return false;
} 

PBoolean H46017Transport::HandleH46017SignallingSocket(H323SignalPDU & pdu)
{
  for (;;) {

      if (!IsOpen())
          return false;

      H323SignalPDU rpdu;
      if (!rpdu.Read(*this)) { 
            PTRACE(3, "H46017\tSocket Read Failure");
            if (GetErrorNumber(PChannel::LastReadError) == 0) {
              PTRACE(3, "H46017\tRemote SHUT DOWN or Intermediary Shutdown!");
              closeTransport = true;
              remoteShutDown = TRUE;
            }
            return false;
      } else {

          if (closeTransport) 
              return false;

          // Inspect the signalling message to see if RAS
          if (HandleH46017RAS(rpdu)) 
              continue;

          if (rpdu.GetQ931().GetMessageType() == Q931::NationalEscapeMsg) {
              PTRACE(5,"H46017\tEscape received. Ignoring...");
              continue;
          }

          // If not token and other then a setup message then something is wrong!!
          if (callToken.IsEmpty() &&
              rpdu.GetQ931().GetMessageType() != Q931::SetupMsg) {
              PTRACE(2,"H46017\tLOGIC ERROR. ABORT! Expecting Setup got " << rpdu);
              continue;
          }

          // return if a call is starting
          pdu = rpdu;
          return true;
      }
  }
}

PBoolean H46017Transport::HandleH46017SignallingChannelPDU(PThread * thread)
{

  H323SignalPDU pdu;
  if (!HandleH46017SignallingSocket(pdu)) {
    if (remoteShutDown)   // Intentional Shutdown?
       Close();
    return false;
  } 

// Create a new transport to the GK as this is now a call Transport.
  isConnected = TRUE;
  H46017Handler::curtransport = NULL;
  Feature->CreateNewTransport();

// Process the Tokens
  unsigned callReference = pdu.GetQ931().GetCallReference();
  callToken = endpoint.BuildConnectionToken(*this, callReference, TRUE);

  con = endpoint.CreateConnection(callReference, NULL, this, &pdu);
    if (con == NULL) {
        PTRACE(1, "H46017\tEndpoint could not create connection, " <<
                  "sending release complete PDU: callRef=" << callReference);
        Q931 pdu;
        pdu.BuildReleaseComplete(callReference, TRUE);
        PBYTEArray rawData;
        pdu.Encode(rawData);
        WritePDU(rawData);
        return true;
    }

    PTRACE(3, "H46017\tCreated new connection: " << callToken);
    connectionsMutex.Wait();
    endpoint.GetConnections().SetAt(callToken, con);
    connectionsMutex.Signal();

    con->AttachSignalChannel(callToken, this, TRUE);

     // AttachThread(thread);  TODO No need to attach Thread when already attached?
     thread->SetNoAutoDelete();

     if (con->HandleSignalPDU(pdu)) {
        // All subsequent PDU's should wait forever
        SetReadTimeout(PMaxTimeInterval);
        ReadTunnel();
     }
     else {
        con->ClearCall(H323Connection::EndedByTransportFail);
        PTRACE(1, "H46017\tSignal channel stopped on first PDU.");
     }
      
     return true;
}

PBoolean H46017Transport::WritePDU(const PBYTEArray & pdu)
{
    PWaitAndSignal m(WriteMutex);
    return H323TransportTCP::WritePDU(pdu);
}

PBoolean H46017Transport::WriteSignalPDU( const H323SignalPDU & pdu )
{

    PPER_Stream strm;
    pdu.Encode(strm);
    strm.CompleteEncoding();

    if (WritePDU(strm))
        return true;
 
   PTRACE(1, "TUNNEL\tWrite PDU failed ("
         << GetErrorNumber(PChannel::LastWriteError)
         << "): " << GetErrorText(PChannel::LastWriteError));

    return false;
}
    
PBoolean H46017Transport::ReadPDU(PBYTEArray & pdu)
{
    return H323TransportTCP::ReadPDU(pdu);
}

PBoolean H46017Transport::Connect() 
{ 
    if (closeTransport)
        return true;

    PTRACE(4, "H46017\tConnecting to remote"  );
    if (!H323TransportTCP::Connect())
        return false;

    if (!m_socketWrite)
        m_socketWrite = PThread::Create(PCREATE_NOTIFIER(SocketWrite), 0, PThread::AutoDeleteThread);

    return true;
}

void H46017Transport::ConnectionLost(PBoolean established)
{
    PWaitAndSignal m(shutdownMutex);

    if (closeTransport)
        return;

    PBoolean lost = IsConnectionLost();
    PTRACE(4,"H46017\tConnection lost " << established << " have " << lost);
}

PBoolean H46017Transport::IsConnectionLost() const  
{ 
    return Feature->IsConnectionLost(); 
}

PBoolean H46017Transport::Close() 
{ 
   PWaitAndSignal m(shutdownMutex);

   PTRACE(4, "H46017\tClosing H46017 NAT channel.");    
   closeTransport = TRUE;
   if (con) con->EndHandleControlChannel();

   return H323TransportTCP::Close(); 
}

PBoolean H46017Transport::IsOpen () const
{
   return H323TransportTCP::IsOpen();
}

PBoolean H46017Transport::IsListening() const
{      
  if (isConnected)
    return FALSE;

  if (h245listener == NULL)
    return FALSE;

  if (IsConnectionLost())
    return FALSE;

  return h245listener->IsOpen();
}

#ifdef H323_H46026

PBoolean H46017Transport::PostFrame(unsigned sessionID,bool rtp, const void * buf, PINDEX len)
{
    if (!con) return false;

    H46026UDPSocket * socket = (H46026UDPSocket *)con->GetNatSocket(sessionID,rtp); 
    if (socket)
        return ((H46026UDPSocket *)socket)->WriteBuffer(buf,len);
 
    PTRACE(3,"H46017\tCannot find Socket " << sessionID << " " << (rtp ? "RTP" : "RTCP"));
    return false;
}

void H46017Transport::GenerateFastUpdatePicture(int session)
{
    PInt64 now = PTimer::Tick().GetMilliSeconds();
    if (now - lastFUP > FAST_UPDATE_INTERVAL) {

      H323ControlPDU pdu;
      BuildFastUpdatePicture(session, pdu);
      con->HandleControlPDU(pdu);
      lastFUP = now;
    }
}
#endif // H323_H46026

PBoolean H46017Transport::WriteTunnel(const H323SignalPDU & msg, const h225Order::MessageHeader & prior)
{
    if (!IsOpen()) return false;

    queueMutex.Wait();
     signalQueue.push(pair<H323SignalPDU, h225Order::MessageHeader>(H323SignalPDU(msg), prior) );
    queueMutex.Signal();
    return true;
}

PBoolean H46017Transport::ReadTunnel()
{
 
  while (IsOpen()) {
     H323SignalPDU pdu;
     if (!pdu.Read(*this)) {
         PTRACE(3, "H46017\tSocket Read Failure");
         return false;
     }

    PTRACE(6,"H46017\tTunnel Message Rec'd");

    switch (pdu.GetQ931().GetMessageType()) {
      case Q931::FacilityMsg:
          if (ReadControl(pdu)) continue;
#ifdef H323_H46026
      case Q931::InformationMsg:
          if (ReadMedia(pdu)) continue;
#endif
      default:
          break;
    }
    
    if (!con->HandleReceivedSignalPDU(true, pdu)) {
          PTRACE(2,"H46017\tError in Tunnel Message");
          return false;
    }
  }
  return true;
}

PBoolean H46017Transport::HandleControlPDU(const H323ControlPDU & pdu)
{

  switch (pdu.GetTag()) {
    case H245_MultimediaSystemControlMessage::e_command:
        if (!HandleH245Command(false,pdu))
            return true;  // Disgard message
    case H245_MultimediaSystemControlMessage::e_request:
    case H245_MultimediaSystemControlMessage::e_response:
    case H245_MultimediaSystemControlMessage::e_indication:
    default:
        break;
  }

  return false;  // Handle normally
}

PBoolean H46017Transport::ReadControl(const H323SignalPDU & msg)
{

    if (!msg.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_h245Control))
        return false;

    for (PINDEX i = 0; i < msg.m_h323_uu_pdu.m_h245Control.GetSize(); i++) {
        PPER_Stream strm = msg.m_h323_uu_pdu.m_h245Control[i].GetValue();
        H323ControlPDU pdu;
        if (pdu.Decode(strm))
            return HandleControlPDU(pdu);
    }
    return true;
}

#ifdef H323_H46026
PBoolean H46017Transport::ReadMedia(const H323SignalPDU & msg)
{
    if (msg.GetQ931().GetMessageType() != Q931::InformationMsg)
        return false;

    const H225_H323_UU_PDU & information = msg.m_h323_uu_pdu;
    if (!information.HasOptionalField(H225_H323_UU_PDU::e_genericData))
        return false;

    if (information.m_genericData.GetSize() == 0)
        return false;

    const H225_GenericData & data = information.m_genericData[0];

    H460_FeatureStd & feat = (H460_FeatureStd &)data;
    PASN_OctetString & media = feat.Value(H460_FeatureID(1));

    H46026_UDPFrame frame;
    media.DecodeSubType(frame);
    int session = frame.m_sessionId;
    bool rtp = frame.m_dataFrame;
  
    for (PINDEX i=0; i < frame.m_frame.GetSize(); i++) {
        const H46026_FrameData & xdata = frame.m_frame[i];
        const PASN_OctetString & payload = xdata;
#if PACKET_ANALYSIS
        if (rtp) {
            RTP_DataFrame frameData(payload.GetSize()-12);
            memcpy(frameData.GetPointer(), payload.GetValue(), payload.GetSize());
            AnalysePacket(false, session, frameData);
        }
#endif
        PostFrame(session,rtp, payload.GetValue(), payload.GetSize());
    }
    return true;
}
#endif // H323_H46026

void H46017Transport::SocketWrite(PThread &, INT)
{
    PAdaptiveDelay delay;
    int buffersz = 0;
    h225Order::MessageHeader msg;
    msg.sessionId = 0;
    msg.priority = 0;
    msg.packTime = PTimer::Tick().GetMilliSeconds();
    H323SignalPDU pdu;

    for (;;) {

        if (!IsOpen()) break;

        queueMutex.Wait();
            buffersz = signalQueue.size();
            if (buffersz > 0) {
                msg = signalQueue.top().second;
                pdu = signalQueue.top().first;
                signalQueue.pop();
            }
        queueMutex.Signal();

       if (buffersz == 0) continue;

        PInt64 stackTime = (PTimer::Tick().GetMilliSeconds() - msg.packTime);

        PTRACE(1,"TEST\tP: " << msg.priority << " pack "  << msg.packTime  << " delay " << stackTime  );

        if ((stackTime > MAX_STACK_DESCRETION) && 
                (msg.priority == h225Order::Priority_Discretion)) {
#ifdef LOCAL_FASTUPDATE
            GenerateFastUpdatePicture(100+msg.sessionId);
#endif
            PTRACE(1,"TEST\tFrame Dropped!");
            continue;
        }

        if (stackTime > MAX_STACK_DELAY) {
             int sz = 0;
          queueMutex.Wait();
            while (signalQueue.size() > 0) 
                signalQueue.pop();  sz++;
          queueMutex.Signal();
            PTRACE(1,"TEST\t" << sz << " Packets Dropped!");
            continue;
        } 

        if (!pdu.Write(*this,NULL)) {
            PTRACE(1,"H46017\tTunnel Write Failure!");
            break;
        }

        PAdaptiveDelay wait;
        double sz = pdu.GetQ931().GetIE(Q931::UserUserIE).GetSize();
        int delay = int((sz / MAX_PIPE_SHAPING) * 1000.0);
PTRACE(1,"TEST\tWait " << delay << " sz " << int(sz));  
        wait.Delay(delay);
    }

    PTRACE(2,"H46017\tTunnel Write Thread ended");
}

/////////////////////////////////////////////////////////////////////////////

H46017Transport * H46017Handler::curtransport = NULL;

H46017Handler::H46017Handler(H323EndPoint & _ep, const H323TransportAddress & _remoteAddress)
 : ep(_ep), ras(NULL), remoteAddress(_remoteAddress)
#ifdef H323_H46026
   , m_h46026tunnel(false)
#endif
{
    PTRACE(4, "H46017\tCreating H46017 Feature.");
    
    PIPSocket::Address remAddr;
    remoteAddress.GetIpAddress(remAddr);
    localBindAddress = PIPSocket::GetRouteInterfaceAddress(remAddr);
}

H46017Handler::~H46017Handler()
{
    callEnded = true;

    if (curtransport != NULL) {
        curtransport->Close();
        curtransport = NULL;
    }

    if (ras != NULL) {
        delete ras;
        ras = NULL;
    }
}


PBoolean H46017Handler::CreateNewTransport()
{
    PTRACE(5, "H46017\tCreating Transport.");

    curtransport = new H46017Transport(ep,
                       PIPSocket::Address::GetAny(remoteAddress.GetIpVersion()), this);
    curtransport->SetRemoteAddress(remoteAddress);

    if (curtransport->Connect()) {
      PTRACE(3, "H46017\tConnected to " << curtransport->GetRemoteAddress());
        new H46017TransportThread(curtransport->GetEndPoint(), curtransport);
        openTransport = true;
        return TRUE;
    } 
     
    PTRACE(3, "H46017\tTransport Failure " << curtransport->GetRemoteAddress());
    delete curtransport;
    curtransport = NULL;
    return FALSE;
}

H323EndPoint * H46017Handler::GetEndPoint() 
{ 
    return &ep; 
}

H323TransportAddress H46017Handler::GetTunnelBindAddress() const
{
    return curtransport->GetLocalAddress();
}

void H46017Handler::AttachRasTransport(H46017RasTransport * _ras)
{
    ras = _ras;
}
   
H46017RasTransport * H46017Handler::GetRasTransport()
{
    return ras;
}

PBoolean H46017Handler::RegisterGatekeeper()
{
    if (!curtransport->IsOpen())
        return false;

    if (ras) delete ras;

    PString dummyAddress("0.0.0.0:0");
    if (!ep.SetGatekeeper(dummyAddress, new H46017RasTransport(ep, this)))
        return false;

    return true;
}

#ifdef H323_H46026
void H46017Handler::SetH46026Tunnel(PBoolean tunnel)
{
    m_h46026tunnel = tunnel;
}

PBoolean H46017Handler::IsH46026Tunnel()
{
    return m_h46026tunnel;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef H323_H46026

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


PBoolean H46026UDPSocket::BuildTunnelMediaPacket(const void * buf, PINDEX len)
{
    PBoolean toSend = true;
    m_frameCount++;
    
    H46026_FrameData fdata;
    fdata.SetTag(!rtpSocket);
    PASN_OctetString & raw = fdata;
            
    
    if (rtpSocket) {
#if PACKET_ANALYSIS
        RTP_DataFrame testFrame(len-12);
        memcpy(testFrame.GetPointer(),buf,len);
        AnalysePacket(true, m_Session, testFrame);
#endif
        if (m_Session == 1) {  // Audio frame bundling
            toSend = (m_frameCount + 1 >= MAX_AUDIO_FRAMES);
            m_frameCount++;
        } else {                // 1 complete frame per packet if possible
            RTP_DataFrame dataFrame(len-12);
            m_payloadSize += len;
            memcpy(dataFrame.GetPointer(),buf,len);
            toSend = ((m_payloadSize + dataFrame.GetSize() >= MAX_VIDEO_PAYLOAD) || dataFrame.GetMarker());
        }
    }       
    raw.SetValue((const BYTE *)buf,len);

    if (!m_frame) {
        m_frame = new H46026_UDPFrame();
        m_frame->m_sessionId.SetValue(m_Session);
        m_frame->m_dataFrame = rtpSocket;
    }

    // add it to the message stack
    int sz = m_frame->m_frame.GetSize();
    m_frame->m_frame.SetSize(sz+1);
    m_frame->m_frame[sz] = fdata;

   if (toSend) {
     PTRACE(1,"TEST\tTUNNEL SEND " << m_Session  
               << " " << m_frame->m_frame.GetSize() << " frames.");

     H323SignalPDU * tunnelmsg = new H323SignalPDU(msg);
     H225_H323_UU_PDU & information = tunnelmsg->m_h323_uu_pdu;
 
        information.IncludeOptionalField(H225_H323_UU_PDU::e_genericData);
        information.m_genericData.SetSize(1);
        H225_GenericData & data = information.m_genericData[0];

        H460_FeatureStd feat = H460_FeatureStd(26);
        PASN_OctetString encFrame;
        encFrame.EncodeSubType(*m_frame);
        feat.Add(1,H460_FeatureContent(encFrame));
        data = feat;
        // Encode the message
        tunnelmsg->BuildQ931();

        h225Order::MessageHeader prior;
        prior.sessionId = m_Session;
        if (rtpSocket)
             prior.priority = ((m_Session == 1) ? h225Order::Priority_Critical : h225Order::Priority_Discretion);
        else
             prior.priority = h225Order::Priority_low;
        prior.packTime = PTimer::Tick().GetMilliSeconds();

     if (m_transport) 
         m_transport->WriteTunnel(*tunnelmsg,prior);

     delete tunnelmsg;

     m_frameCount=0;
     m_payloadSize=0;
     delete m_frame;
     m_frame = NULL;
   }
   return (toSend);
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
    BuildTunnelMediaPacket(buf, len);
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

/////////////////////////////////////////////////////////////////////////

H46017RasTransport::H46017RasTransport(H323EndPoint & endpoint, H46017Handler * handler)
 : H323TransportUDP(endpoint), m_handler(handler), shutdown(false)
{
    endpoint.SetSendGRQ(false);
    m_handler->AttachRasTransport(this);
}

H46017RasTransport::~H46017RasTransport()
{
    Close();
    m_handler->AttachRasTransport(NULL);
}

PBoolean H46017RasTransport::SetRemoteAddress(const H323TransportAddress & /*address*/)
{
    return true;
}

H323TransportAddress H46017RasTransport::GetLocalAddress() const
{
    return H323TransportAddress("0.0.0.0:0");
}

void H46017RasTransport::SetUpTransportPDU(H225_TransportAddress & pdu, PBoolean /*localTsap*/, H323Connection * /*connection*/) const
{
  H323TransportAddress transAddr = GetLocalAddress();
  transAddr.SetPDU(pdu);
}

PBoolean H46017RasTransport::Connect()
{
    return true;
}

PBoolean H46017RasTransport::Close()
{
    if (!shutdown) {
       shutdown = true;
       msgRecd.Signal();
    }
    return true;
}


PBoolean H46017RasTransport::ReceivedPDU(const PBYTEArray & pdu)
{
    recdpdu = pdu;
    msgRecd.Signal();
    return true;
}

PBoolean H46017RasTransport::ReadPDU(PBYTEArray & pdu)
{
    msgRecd.Wait();
    if (shutdown) 
      return false;

    pdu = recdpdu;
    return true;
}

PBoolean H46017RasTransport::WritePDU(const PBYTEArray & pdu)
{
    return H46017Handler::curtransport->WriteRasPDU(pdu);
}

PBoolean H46017RasTransport::DiscoverGatekeeper(H323Gatekeeper & /*gk*/, H323RasPDU & /*pdu*/, const H323TransportAddress & /*address*/)
{
    return true;
}

PBoolean H46017RasTransport::IsRASTunnelled()  
{ 
    return true; 
}

PChannel::Errors H46017RasTransport::GetErrorCode(ErrorGroup /*group*/) const
{
    if (shutdown)
        return PChannel::NotOpen;

    return PChannel::Interrupted;
}

#endif // H323_H46017






