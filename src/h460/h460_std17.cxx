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
 #include <h460/h460_std26.h> 
#endif

#include <ptclib/pdns.h>
#include <ptclib/delaychan.h>


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
    if (!m_handler)
        return false;
#ifdef H323_H46018
    EP->H46018Enable(false);
#endif
    isEnabled = true;
    return (m_handler->RegisterGatekeeper());

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
  : PThread(ep.GetSignallingThreadStackSize(), AutoDeleteThread, NormalPriority, "H46017:%0x"),
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

      ret = transport->HandleH46017Socket();

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

#ifdef H323_TLS
H46017Transport::H46017Transport(H323EndPoint & endpoint,
                                 PIPSocket::Address binding,
                                 H46017Handler * feat
                                 ,PSSLContext * context, 
                                 PBoolean autoDeleteContext
                )
   : H323TransportTCP(endpoint,binding,false,context,autoDeleteContext), 
#else
H46017Transport::H46017Transport(H323EndPoint & endpoint,
                                 PIPSocket::Address binding,
                                 H46017Handler * feat
                )    
   : H323TransportTCP(endpoint,binding), 
#endif
     ReadTimeOut(PMaxTimeInterval), 
     Feature(feat), remoteShutDown(false), closeTransport(false), m_signalProcess(NULL)
 #ifdef H323_H46026
     ,m_h46026tunnel(false), m_socketMgr(NULL), m_socketWrite(NULL)
#endif
{

}

H46017Transport::~H46017Transport()
{
    Close();
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
  if (remoteShutDown)
      return false;

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
 return WriteTunnel(rasPDU);
}


PBoolean H46017Transport::HandleH46017Socket()
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
      } else if (closeTransport) {
            PTRACE(3, "H46017\tClosing Transport!");
            return false;
      } else {
          // Keep alive Message
          if (rpdu.GetQ931().GetMessageType() == Q931::NationalEscapeMsg) {
              PTRACE(6,"H46017\tEscape received. Ignoring...");
              continue;
          }
#ifdef H323_H46026 
          if (m_h46026tunnel) {
              m_socketMgr->SocketIn(rpdu.GetQ931());
              continue;
          } else 
#endif
          {
              if (HandleH46017PDU(rpdu.GetQ931()))
                  continue;
          }

          PTRACE(5,"H46017\tMessage not Handled!");
      }
  }
}

PBoolean H46017Transport::HandleH46017PDU(const Q931 & q931)
{
    H323SignalPDU pdu;
    pdu.LoadTunneledQ931(q931);
    return HandleH46017PDU(pdu);
}

PBoolean H46017Transport::HandleH46017PDU(H323SignalPDU & pdu)
{
    // Inspect the signalling message to see if RAS
     if (HandleH46017RAS(pdu)) 
         return true;
     else if (HandleH46017SignalPDU(pdu))
         return true;
     else
         return false;
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

// Unfortunately we have to put the signalling messages onto a seperate thread
// as the messages may block for several seconds.
PBoolean H46017Transport::HandleH46017SignalPDU(H323SignalPDU & pdu)
{
    if (!m_signalProcess) {
        m_signalProcess = PThread::Create(PCREATE_NOTIFIER(SignalProcess), 0,
                    PThread::AutoDeleteThread,
                    PThread::NormalPriority,
                    "h46017signal:%x");
    }
    signalMutex.Wait();
    recdpdu.push(pdu);
    signalMutex.Signal();
    msgRecd.Signal();
    return true;
}

void H46017Transport::SignalProcess(PThread &, INT)
{
    H323SignalPDU pdu;
    PBoolean dataToProcess = false;
    while (!closeTransport) {
        msgRecd.Wait();
        while (!closeTransport && !recdpdu.empty()) {
            signalMutex.Wait();
            if (!recdpdu.empty()) {
                pdu = recdpdu.front();
                dataToProcess = true;
                recdpdu.pop();
            }
            signalMutex.Signal();
            if (dataToProcess) {
                HandleH46017SignallingPDU(pdu.GetQ931().GetCallReference(),pdu);
                dataToProcess = false;
            }
        }
    }
}

PBoolean H46017Transport::HandleH46017SignallingPDU(unsigned crv, H323SignalPDU & pdu)
{
  H323Connection * connection = NULL;
  if ((pdu.GetQ931().GetMessageType() == Q931::SetupMsg))
      connection = HandleH46017SetupPDU(pdu);
  else {
    connectionsMutex.Wait();
    for (PINDEX i = 0; i < endpoint.GetConnections().GetSize(); i++) {
        H323Connection & conn = endpoint.GetConnections().GetDataAt(i);
        if (conn.GetCallReference() == crv)
            connection = &conn;
    }
    connectionsMutex.Signal();
  }
  if (!connection) {
      PTRACE(2, "H46017\tConnection " << crv << " not found or could not process. Q931 not processed.");
      return true;
  }   
  if (!connection->HandleReceivedSignalPDU(true, pdu)) {
      PTRACE(2, "H46017\tMessage not processed dropping call.");
  }
  return true;
}

H323Connection * H46017Transport::HandleH46017SetupPDU(H323SignalPDU & pdu)
{

  unsigned callReference = pdu.GetQ931().GetCallReference();
  PString callToken = endpoint.BuildConnectionToken(*this, callReference, TRUE);

  H323Connection * connection = endpoint.CreateConnection(callReference, NULL, this, &pdu);
  if (!connection) {
        PTRACE(1, "H46017\tEndpoint could not create connection, " <<
                  "sending release complete PDU: callRef=" << callReference);
        Q931 pdu;
        pdu.BuildReleaseComplete(callReference, TRUE);
        PBYTEArray rawData;
        pdu.Encode(rawData);
        WritePDU(rawData);
        return NULL;
  }

  PTRACE(3, "H46017\tCreated new connection: " << callToken);
  connectionsMutex.Wait();
  endpoint.GetConnections().SetAt(callToken, connection);
  connectionsMutex.Signal();

  connection->AttachSignalChannel(callToken, this, TRUE);

  return connection;
}

PBoolean H46017Transport::WritePDU(const PBYTEArray & pdu)
{
    PWaitAndSignal m(WriteMutex);
    return H323TransportTCP::WritePDU(pdu);
}

PBoolean H46017Transport::WriteSignalPDU( const H323SignalPDU & pdu )
{
PTRACE(4, "H46017\tSending Tunnel\t" << pdu);

    PPER_Stream strm;
    const Q931 & q931 = pdu.GetQ931();
    q931.Encode(strm);

    if (WritePDU(strm))
        return true;
 
   PTRACE(1, "H46017\tTunnel write failed ("
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

    PTRACE(4, "H46017\tConnecting to remote");
    if (!H323TransportTCP::Connect())
        return false;

    return true;
}

void H46017Transport::ConnectionLost(PBoolean established)
{
    PWaitAndSignal m(shutdownMutex);

    if (closeTransport) {
        // TODO Handle TCP socket reconnect
        if (Feature) 
            Feature->TransportClosed();
        return;
    }
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

   closeTransport = TRUE;

   signalMutex.Wait();
   while (!recdpdu.empty()) {
        recdpdu.pop();
   }
   signalMutex.Signal();
   msgRecd.Signal();

   PTRACE(4, "H46017\tClosing H46017 NAT channel.");   
   return H323TransportTCP::Close(); 
}

void H46017Transport::CleanUpOnTermination()
{
  // Do nothing at the end of a call. This is a permanent connection
  PTRACE(4, "H46017\tIgnore cleanup of H46017 NAT channel.");  
}

PBoolean H46017Transport::IsOpen () const
{
   return H323TransportTCP::IsOpen();
}

PBoolean H46017Transport::IsListening() const
{      
  if (h245listener == NULL)
    return FALSE;

  if (IsConnectionLost())
    return FALSE;

  return h245listener->IsOpen();
}

PBoolean H46017Transport::WriteTunnel(H323SignalPDU & msg)
{
    if (!IsOpen()) return false;

#ifdef H323_H46026
    if (m_h46026tunnel) {
        m_socketMgr->SignalToSend(msg.GetQ931());
        return true;
    } else
#endif
        return msg.Write(*this,NULL);
}

#ifdef H323_H46026
void H46017Transport::SetTunnel(H46026Tunnel * mgr)
{
    m_socketMgr = mgr;
    m_socketMgr->AttachTransport(this);

    if (!m_socketWrite)
        m_socketWrite = PThread::Create(PCREATE_NOTIFIER(SocketWrite), 0, PThread::AutoDeleteThread);

    m_h46026tunnel = true;
}

void H46017Transport::SocketWrite(PThread &, INT)
{
    PBYTEArray tpkt(10004);  // 10K buffer with RFC1006 Header
    tpkt[0] = 3;
    tpkt[1] = 0;

    PINDEX sz = 0;
    int packetLength = 0;
    while (!closeTransport) {
        if (m_socketMgr->SocketOut(tpkt.GetPointer()+4,sz)) {
            packetLength = sz + 4;
            tpkt[2] = (BYTE)(packetLength >> 8);
            tpkt[3] = (BYTE)packetLength;
            Write((const BYTE *)tpkt, packetLength);
        } else {
            PThread::Sleep(2);
        }
    }
    tpkt.SetSize(0);
    PTRACE(2,"H46017\tTunnel Write Thread ended");
}
#endif

/////////////////////////////////////////////////////////////////////////////

H46017Handler::H46017Handler(H323EndPoint & _ep, const H323TransportAddress & _remoteAddress)
 : ep(_ep), curtransport(NULL), ras(NULL), remoteAddress(_remoteAddress),
   connectionlost(false), openTransport(false)
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

H46017Transport * H46017Handler::GetTransport()
{
    return curtransport;
}

void H46017Handler::AttachRasTransport(H46017RasTransport * _ras)
{
    ras = _ras;
    if (!ras)
        curtransport->Close();
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

void H46017Handler::TransportClosed()
{
    if (ras)
        ras->Close();
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

/////////////////////////////////////////////////////////////////////////

H46017RasTransport::H46017RasTransport(H323EndPoint & endpoint, H46017Handler * handler)
 : H323TransportUDP(endpoint), m_handler(handler), shutdown(false)
{
    endpoint.SetSendGRQ(false);
    m_handler->AttachRasTransport(this);
}

H46017RasTransport::~H46017RasTransport()
{

}

PBoolean H46017RasTransport::SetRemoteAddress(const H323TransportAddress & /*address*/)
{
    return true;
}

H323TransportAddress H46017RasTransport::GetLocalAddress() const
{
    return H323TransportAddress("0.0.0.0:0");
}

H323TransportAddress H46017RasTransport::GetRemoteAddress() const
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
    return m_handler->GetTransport()->WriteRasPDU(pdu);
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

void H46017RasTransport::CleanUpOnTermination()
{
    PTRACE(4,"H46017\tRAS transport cleanup");
    m_handler->AttachRasTransport(NULL);
    H323Transport::CleanUpOnTermination();
}

#endif // H323_H46017






