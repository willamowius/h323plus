/*
 * h460_h225.cxx
 *
 * H.460.18 H225 NAT Traversal class.
 *
 * h323plus library
 *
 * Copyright (c) 2008 ISVO (Asia) Pte. Ltd.
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
 * Portions of this code were written with the assisance of funding from
 * triple-IT. http://www.triple-it.nl.
 *
 * Contributor(s): ______________________________________.
 *
 * $Log$
 * Revision 1.2  2009/06/28 04:47:53  shorne
 * Fixes for H.460.19 NAT Method loading
 *
 * Revision 1.1  2009/06/28 00:11:03  shorne
 * Added H.460.18/19 Support
 *
 *
 *
 */

#include "ptlib.h"
#include "openh323buildopts.h"

#ifdef H323_H46018

#include <h323ep.h>
#include <h323pdu.h>
#include <h460/h46018_h225.h>
#include <h460/h46018.h>

PCREATE_NAT_PLUGIN(H46019);

///////////////////////////////////////////////////////////////////////////////////

// Listening/Keep Alive Thread

class H46018TransportThread : public PThread
{
   PCLASSINFO(H46018TransportThread, PThread)

   public:
	H46018TransportThread(H323EndPoint & endpoint, H46018Transport * transport);

   protected:
	void Main();

	PBoolean    isConnected;
	H46018Transport * transport;

	PTime   lastupdate;

};

/////////////////////////////////////////////////////////////////////////////

H46018TransportThread::H46018TransportThread(H323EndPoint & ep, H46018Transport * t)
  : PThread(ep.GetSignallingThreadStackSize(), AutoDeleteThread,
            NormalPriority,"H225 Answer:%0x"),transport(t)
{  

	isConnected = false;

	// Start the Thread
	Resume();
}

void H46018TransportThread::Main()
{
	PTRACE(3, "H46018\tStarted Listening Thread");

	PBoolean ret = true;
	while ((transport->IsOpen()) &&		// Transport is Open
	(!isConnected) &&			// Does not have a call connection 
	(ret) &&	                        // is not a Failed connection
	(!transport->CloseTransport())) {	// not close due to shutdown
		ret = transport->HandleH46018SignallingChannelPDU();

		if (!ret && transport->CloseTransport()) {  // Closing down Instruction
			PTRACE(3, "H46018\tShutting down H46018 Thread");
			transport->ConnectionLost(true);
		} 
	}

	PTRACE(3, "H46018\tTransport Closed");
}

///////////////////////////////////////////////////////////////////////////////////////


H46018SignalPDU::H46018SignalPDU(const OpalGloballyUniqueID & callIdentifier)
{
	// Build facility msg
	q931pdu.BuildFacility(0, FALSE);

	// Build the UUIE
	m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_facility);
	H225_Facility_UUIE & fac = m_h323_uu_pdu.m_h323_message_body;

	PString version = "0.0.8.2250.0." + PString(H225_PROTOCOL_VERSION);
	fac.m_protocolIdentifier.SetValue(version);
	fac.m_reason.SetTag(H225_FacilityReason::e_undefinedReason);
	fac.IncludeOptionalField(H225_Facility_UUIE::e_callIdentifier);
	fac.m_callIdentifier.m_guid = callIdentifier;

	// Put UUIE into user-user of Q931
	BuildQ931();
}

//////////////////////////////////////////////////////////////////////////////////////

H46018Transport::H46018Transport(H323EndPoint & endpoint)	
   : H323TransportTCP(endpoint)
{
	ReadTimeOut = PMaxTimeInterval;
	isConnected = false;
	closeTransport = false;
	remoteShutDown = false;
}

H46018Transport::~H46018Transport()
{
	Close();
}

PBoolean H46018Transport::HandleH46018SignallingSocket(H323SignalPDU & pdu)
{
	for (;;) {

	  if (!IsOpen())
		  return false;

	  H323SignalPDU rpdu;
	  if (!rpdu.Read(*this)) { 
          	PTRACE(3, "H46018\tSocket Read Failure");
          	if (GetErrorNumber(PChannel::LastReadError) == 0) {
         	PTRACE(3, "H46018\tRemote SHUT DOWN or Intermediary Shutdown!");
          	remoteShutDown = true;
          	}
		return false;
	  } else if (rpdu.GetQ931().GetMessageType() == Q931::SetupMsg) {
          	pdu = rpdu;
          	return true;
	  } else {
	  	PTRACE(3, "H46018\tUnknown PDU Received");
          	return false;
	  }

	}
}

PBoolean H46018Transport::HandleH46018SignallingChannelPDU()
{

	H323SignalPDU pdu;
	if (!HandleH46018SignallingSocket(pdu)) {
		if (remoteShutDown) {   // Intentional Shutdown?
			Close();
    		}
		return false;
	}

    // We are connected
 	isConnected = true;

	// Process the Tokens
	unsigned callReference = pdu.GetQ931().GetCallReference();
	PString token = endpoint.BuildConnectionToken(*this, callReference, true);

	H323Connection * connection = endpoint.CreateConnection(callReference, NULL, this, &pdu);
		if (connection == NULL) {
			PTRACE(1, "H46018\tEndpoint could not create connection, " <<
					  "sending release complete PDU: callRef=" << callReference);
			Q931 pdu;
			pdu.BuildReleaseComplete(callReference, true);
			PBYTEArray rawData;
			pdu.Encode(rawData);
			WritePDU(rawData);
			return true;
		}

	PTRACE(3, "H46018\tCreated new connection: " << token);
	connectionsMutex.Wait();
	GetEndPoint().GetConnections().SetAt(token, connection);
	connectionsMutex.Signal();

	connection->AttachSignalChannel(token, this, true);
 
	PThread * thread = PThread::Current();
	AttachThread(thread);
	thread->SetNoAutoDelete();

	if (connection->HandleSignalPDU(pdu)) {
		// All subsequent PDU's should wait forever
		SetReadTimeout(PMaxTimeInterval);
		connection->HandleSignallingChannel();
	}
	else {
		connection->ClearCall(H323Connection::EndedByTransportFail);
		PTRACE(1, "H46018\tSignal channel stopped on first PDU.");
	}

	return true;
}


PBoolean H46018Transport::WritePDU( const PBYTEArray & pdu )
{
	PWaitAndSignal m(WriteMutex);
	return H323TransportTCP::WritePDU(pdu);

}
	
PBoolean H46018Transport::ReadPDU(PBYTEArray & pdu)
{
	return H323TransportTCP::ReadPDU(pdu);
}

PBoolean H46018Transport::Connect(const OpalGloballyUniqueID & callIdentifier) 
{ 
	PTRACE(4, "H46018\tConnecting to H.460.18 Server");

	if (!H323TransportTCP::Connect())
		return false;
	
	return InitialPDU(callIdentifier);
}

void H46018Transport::ConnectionLost(PBoolean established)
{
	if (closeTransport)
		return;
}

PBoolean H46018Transport::IsConnectionLost()  
{ 
	return false; 
}


PBoolean H46018Transport::InitialPDU(const OpalGloballyUniqueID & callIdentifier)
{
	PWaitAndSignal mutex(IntMutex);

	if (!IsOpen())
		return false;

	H46018SignalPDU pdu(callIdentifier);

	PTRACE(6, "H46018\tCall Facility PDU: " << pdu);

	PBYTEArray rawData;
	pdu.GetQ931().Encode(rawData);

	if (!WritePDU(rawData)) {
		PTRACE(3, "H46018\tError Writing PDU.");
		return false;
	}

	PTRACE(4, "H46018\tSent PDU Call: " << callIdentifier.AsString() << " awaiting response.");

	return true;
}

PBoolean H46018Transport::Close() 
{ 
	PTRACE(4, "H46018\tClosing H46018 NAT channel.");	
	closeTransport = true;
	return H323TransportTCP::Close(); 
}

PBoolean H46018Transport::IsOpen () const
{
	return H323TransportTCP::IsOpen();
}

PBoolean H46018Transport::IsListening() const
{	  
	if (isConnected)
		return false;

	if (h245listener == NULL)
		return false;

	return h245listener->IsOpen();
}

/////////////////////////////////////////////////////////////////////////////

H46018Handler::H46018Handler(H323EndPoint * ep)
: EP(ep)
{
	PTRACE(4, "H46018\tCreating H46018 Handler.");

	nat = (PNatMethod_H46019 *)ep->GetNatMethods().LoadNatMethod("H46019");
	lastCallIdentifer = PString();
	m_h46018inOperation = false;

	if (nat != NULL) {
	  nat->AttachHandler(this);
	  nat->SetAvailable();
	  ep->GetNatMethods().AddMethod(nat);
	}

	SocketCreateThread = NULL;
}

H46018Handler::~H46018Handler()
{
	PTRACE(4, "H46018\tClosing H46018 Handler.");
	EP->GetNatMethods().RemoveMethod("H46019");
}

PBoolean H46018Handler::CreateH225Transport(const PASN_OctetString & information)
{

	H46018_IncomingCallIndication callinfo;
	PPER_Stream raw(information);

	if (!callinfo.Decode(raw)) {
        PTRACE(2,"H46018\tUnable to decode incoming call Indication."); 
		return false;
	}

    PTRACE(4, "H46018\t" << callinfo );

	m_address = H323TransportAddress(callinfo.m_callSignallingAddress);
	m_callId = OpalGloballyUniqueID(callinfo.m_callID.m_guid);

	// Fix for Tandberg boxes that send duplicate SCI messages.
	if (m_callId.AsString() == lastCallIdentifer) {
		PTRACE(2,"H46018\tDuplicate Call Identifer " << lastCallIdentifer << " Ignoring request!"); 
		return false;
	}

	PTRACE(5, "H46018\tCreating H225 Channel");

	// We throw the socket creation onto another thread as with UMTS networks it may take several 
	// seconds to actually create the connection and we don't want to wait before signalling back
	// to the gatekeeper. This also speeds up connection time which is also nice :) - SH
	SocketCreateThread = PThread::Create(PCREATE_NOTIFIER(SocketThread), 0, PThread::AutoDeleteThread);

	return true;
}

void H46018Handler::SocketThread(PThread &, INT)
{
	if (m_callId == PString()) {
		PTRACE(3, "H46018\tTCP Connect Abort: No Call identifier");
		return;
	}

	H46018Transport * transport = new H46018Transport(*EP);
	transport->SetRemoteAddress(m_address);

	if (transport->Connect(m_callId)) {
		PTRACE(3, "H46018\tConnected to " << transport->GetRemoteAddress());
	    new H46018TransportThread(transport->GetEndPoint(), transport);
		lastCallIdentifer = m_callId.AsString();
	} else {
		PTRACE(3, "H46018\tCALL ABORTED: Failed to TCP Connect to " << transport->GetRemoteAddress());
	}

	m_address = H323TransportAddress();
	m_callId = PString();
}

void H46018Handler::Enable()
{
   m_h46018inOperation = true;
}

PBoolean H46018Handler::IsEnabled()
{
	return m_h46018inOperation;
}

H323EndPoint * H46018Handler::GetEndPoint() 
{ 
	return EP; 
}

///////////////////////////////////////////////////////////////////////////////////////////
	
PNatMethod_H46019::PNatMethod_H46019()
{
	curSession = 0;
	curToken = PString();
	handler = NULL;
}

PNatMethod_H46019::~PNatMethod_H46019()
{
}

void PNatMethod_H46019::AttachHandler(H46018Handler * _handler)
{
	handler = _handler;

	if (handler->GetEndPoint() == NULL) 
		return;

	WORD portPairBase = handler->GetEndPoint()->GetRtpIpPortBase();
	WORD portPairMax = handler->GetEndPoint()->GetRtpIpPortMax();

	// Initialise
	// ExternalAddress = 0;
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

	available = FALSE;
}

PBoolean PNatMethod_H46019::GetExternalAddress(PIPSocket::Address & /*externalAddress*/, /// External address of router
					const PTimeInterval & /* maxAge */         /// Maximum age for caching
					)
{
	return FALSE;
}


PBoolean PNatMethod_H46019::CreateSocketPair(PUDPSocket * & socket1,
					PUDPSocket * & socket2,
					const PIPSocket::Address & /*binding*/
					)
{

	if (pairedPortInfo.basePort == 0 || pairedPortInfo.basePort > pairedPortInfo.maxPort)
	{
		PTRACE(1, "H46019\tInvalid local UDP port range "
			   << pairedPortInfo.currentPort << '-' << pairedPortInfo.maxPort);
		return FALSE;
	}

	socket1 = new H46019UDPSocket();  /// Data 
	socket2 = new H46019UDPSocket();  /// Signal

	/// Make sure we have sequential ports
	while ((!OpenSocket(*socket1, pairedPortInfo)) ||
		   (!OpenSocket(*socket2, pairedPortInfo)) ||
		   (socket2->GetPort() != socket1->GetPort() + 1) )
		{
			delete socket1;
			delete socket2;
			socket1 = new H46019UDPSocket();  /// Data 
			socket2 = new H46019UDPSocket();  /// Signal
		}

		PTRACE(5, "H46019\tUDP ports "
			   << socket1->GetPort() << '-' << socket2->GetPort());

	if (curSession > 0)
		SetConnectionSockets(socket1,socket2);

	return TRUE;
}

PBoolean PNatMethod_H46019::OpenSocket(PUDPSocket & socket, PortInfo & portInfo) const
{
	PWaitAndSignal mutex(portInfo.mutex);

	WORD startPort = portInfo.currentPort;

	do {
		portInfo.currentPort++;
		if (portInfo.currentPort > portInfo.maxPort)
		portInfo.currentPort = portInfo.basePort;

		if (socket.Listen(1, portInfo.currentPort)) {
			socket.SetReadTimeout(500);
			return true;
		}

	} while (portInfo.currentPort != startPort);

	PTRACE(2, "H46019\tFailed to bind to local UDP port in range "
		<< portInfo.currentPort << '-' << portInfo.maxPort);
  	return false;
}

void PNatMethod_H46019::SetSessionInfo(unsigned sessionid, const PString & callToken)
{
	curSession = sessionid;
	curToken = callToken;
}

void PNatMethod_H46019::SetConnectionSockets(PUDPSocket * data,PUDPSocket * control)
{
	if (handler->GetEndPoint() == NULL)
		return;

	H323Connection * connection = handler->GetEndPoint()->FindConnectionWithLock(curToken);
	if (connection != NULL) {
		connection->SetRTPNAT(curSession,data,control);
		connection->Unlock();
	}
	
	curSession = 0;
	curToken = PString();
}

bool PNatMethod_H46019::IsAvailable(const PIPSocket::Address & /*address*/) 
{ 
	return handler->IsEnabled();
}

/////////////////////////////////////////////////////////////////////////////////////////////

H46019UDPSocket::H46019UDPSocket()
{
	keepStartTime = NULL;
}

H46019UDPSocket::~H46019UDPSocket()
{
	Keep.Stop();
	delete keepStartTime;
}

void H46019UDPSocket::Activate(const H323TransportAddress & keepalive, unsigned _payload, unsigned _ttl)
{

	keepalive.GetIpAndPort(keepip,keepport);
	keeppayload = _payload;
	keepseqno = 100;  // Some arbitory number
	keepStartTime = new PTime();
		
	PTRACE(4,"H46019UDP\tStart pinging " << keepip << ":" << keepport << " every " << _ttl << " secs.");

	SendPing();
	Keep.SetNotifier(PCREATE_NOTIFIER(Ping));
	Keep.RunContinuous(_ttl * 1000); 
}

void H46019UDPSocket::Ping(PTimer &, INT)
{ 
	SendPing();
}

void H46019UDPSocket::SendPing() {

	RTP_DataFrame rtp;

	rtp.SetSequenceNumber(keepseqno);

	rtp.SetPayloadType((RTP_DataFrame::PayloadTypes)keeppayload);
	rtp.SetPayloadSize(0);

	// determining correct timestamp
	PTime currentTime = PTime();
	PTimeInterval timePassed = currentTime - *keepStartTime;
	rtp.SetTimestamp((DWORD)timePassed.GetMilliSeconds() * 8);

	rtp.SetMarker(TRUE);

	if (!WriteTo(rtp.GetPointer(),
                 rtp.GetHeaderSize()+rtp.GetPayloadSize(),
                 keepip, keepport)) {
		switch (GetErrorNumber()) {
		case ECONNRESET :
		case ECONNREFUSED :
			PTRACE(2, "H46019UDP\t" << keepip << ":" << keepport << " not ready.");
			break;

		default:
			PTRACE(1, "H46019UDP\t" << keepip << ":" << keepport 
				<< ", Write error on port ("
				<< GetErrorNumber(PChannel::LastWriteError) << "): "
				<< GetErrorText(PChannel::LastWriteError));
		}
	} else {
		PTRACE(6, "H46019UDP\tKeepAlive sent: " << keepip << ":" << keepport << " seq: " << keepseqno);	
	    keepseqno++;
	}
}

PBoolean H46019UDPSocket::GetLocalAddress(Address & addr, WORD & port)
{
  PIPSocket::Address _addr;
  return PUDPSocket::GetLocalAddress(_addr, port);
}

#endif  // H323_H460




