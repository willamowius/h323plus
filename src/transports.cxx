/*
 * transports.cxx
 *
 * H.323 transports handler
 *
 * H323Plus Library
 *
 * Copyright (c) 1998-2000 Equivalence Pty. Ltd.
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
 * Portions of this code were written with the assisance of funding from
 * Vovida Networks, Inc. http://www.vovida.com.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "transports.h"
#endif

#include "transports.h"

#include "h323pdu.h"
#include "h323ep.h"
#include "gkclient.h"

#ifdef P_STUN
#include <ptclib/pstun.h>
 #ifdef _MSC_VER
  #pragma warning(disable : 4701)  // initialisation warning
 #endif
#endif

// TCP KeepAlive 
static int KeepAliveInterval = 19;

class H225TransportThread : public PThread
{
  PCLASSINFO(H225TransportThread, PThread)

  public:
    H225TransportThread(H323EndPoint & endpoint, H323Transport * transport);

    ~H225TransportThread();

    void ConnectionEstablished();

  protected:
    void Main();

    H323Transport * transport;

	PDECLARE_NOTIFIER(PTimer, H225TransportThread, KeepAlive);
	PTimer	m_keepAlive;								
	PBoolean useKeepAlive;
};


class H245TransportThread : public PThread
{
  PCLASSINFO(H245TransportThread, PThread)

  public:
    H245TransportThread(H323EndPoint & endpoint,
                        H323Connection & connection,
                        H323Transport & transport);

    ~H245TransportThread();

  protected:
    void Main();

    H323Connection & connection;
    H323Transport  & transport;
#ifdef H323_SIGNAL_AGGREGATE
    PBoolean useAggregator;
#endif

	PDECLARE_NOTIFIER(PTimer, H245TransportThread, KeepAlive);
	PTimer	m_keepAlive;			
};


#define new PNEW


/////////////////////////////////////////////////////////////////////////////

H225TransportThread::H225TransportThread(H323EndPoint & ep, H323Transport * t)
  : PThread(ep.GetSignallingThreadStackSize(),
            AutoDeleteThread,
            NormalPriority,
            "H225 Answer:%0x"),
    transport(t)
{
  useKeepAlive = ep.EnableH225KeepAlive();
  Resume();
}


H225TransportThread::~H225TransportThread()
{
    if (useKeepAlive)
       m_keepAlive.Stop();
}

void H225TransportThread::ConnectionEstablished()
{
  if (useKeepAlive) {
     PTRACE(3, "H225\tStarted KeepAlive");
     m_keepAlive.SetNotifier(PCREATE_NOTIFIER(KeepAlive));
     m_keepAlive.RunContinuous(KeepAliveInterval * 1000);
  }
}


void H225TransportThread::Main()
{
  PTRACE(3, "H225\tStarted incoming call thread");

  if (!transport->HandleFirstSignallingChannelPDU(this))
    delete transport;
}


void H225TransportThread::KeepAlive(PTimer &, INT)
{
  // Send empty RFC1006 TPKT
  int packetLength = 4;
  PBYTEArray tpkt(packetLength);
  memset(tpkt.GetPointer(),0 , packetLength);

  tpkt[0] = 3;
  tpkt[1] = 0;
  tpkt[2] = (BYTE)(packetLength >> 8);
  tpkt[3] = (BYTE)packetLength;

  PTRACE(6, "H225\tSending KeepAlive TPKT packet");

  if (transport)
     transport->Write((const BYTE *)tpkt, packetLength);
}

/////////////////////////////////////////////////////////////////////////////

H245TransportThread::H245TransportThread(H323EndPoint & endpoint,
                                         H323Connection & c,
                                         H323Transport & t)
  : PThread(endpoint.GetSignallingThreadStackSize(),
            NoAutoDeleteThread,
            NormalPriority,
            "H245:%0x"),
    connection(c),
    transport(t)
{
#ifdef H323_SIGNAL_AGGREGATE
  useAggregator = endpoint.GetSignallingAggregator() != NULL;
  if (!useAggregator)
#endif
  {
    transport.AttachThread(this);
    if (endpoint.EnableH245KeepAlive()) {
      m_keepAlive.SetNotifier(PCREATE_NOTIFIER(KeepAlive));
      m_keepAlive.RunContinuous(KeepAliveInterval * 1000);
    }
  }
  Resume();
}


H245TransportThread::~H245TransportThread()
{
  m_keepAlive.Stop();
}


void H245TransportThread::Main()
{
  PTRACE(3, "H245\tStarted thread");

  if (transport.AcceptControlChannel(connection)) {
#ifdef H323_SIGNAL_AGGREGATE
    // if the endpoint is using signalling aggregation, we need to add this connection
    // to the signalling aggregator. 
    if (useAggregator) {
      connection.AggregateControlChannel(&transport);
      SetAutoDelete(AutoDeleteThread);
      return;
    }
#endif

    connection.HandleControlChannel();
  }
}


void H245TransportThread::KeepAlive(PTimer &, INT)
{
  // Send empty RFC1006 TPKT
  int packetLength = 4;
  PBYTEArray tpkt(packetLength);
  tpkt[0] = 3;
  tpkt[1] = 0;
  tpkt[2] = 0;
  tpkt[3] = (BYTE)packetLength;

  PTRACE(6, "H245\tSending KeepAlive TPKT packet");

  transport.Write((const BYTE *)tpkt, packetLength);
}


/////////////////////////////////////////////////////////////////////////////

static const char IpPrefix[] = "ip$";

H323TransportAddress::H323TransportAddress(const char * cstr)
  : PString(cstr)
{
  Validate();
}


H323TransportAddress::H323TransportAddress(const PString & str)
  : PString(str)
{
  Validate();
}


static PString BuildIP(const PIPSocket::Address & ip, unsigned port)
{
  PStringStream str;

  str << IpPrefix;

  if (!ip.IsValid())
    str << '*';
  else
#if P_HAS_IPV6
  if (ip.GetVersion() == 6)
    str << '[' << ip << ']';
  else
#endif
    str << ip;

  if (port != 0)
    str << ':' << port;

  return str;
}


H323TransportAddress::H323TransportAddress(const H225_TransportAddress & transport)
{
  switch (transport.GetTag()) {
    case H225_TransportAddress::e_ipAddress :
    {
      const H225_TransportAddress_ipAddress & ip = transport;
      *this = BuildIP(PIPSocket::Address(ip.m_ip.GetSize(), ip.m_ip.GetValue()), ip.m_port);
      break;
    }
#if P_HAS_IPV6
    case H225_TransportAddress::e_ip6Address :
    {
      const H225_TransportAddress_ip6Address & ip = transport;
      *this = BuildIP(PIPSocket::Address(ip.m_ip.GetSize(), ip.m_ip.GetValue()), ip.m_port);
      break;
    }
#endif
  }
}


H323TransportAddress::H323TransportAddress(const H245_TransportAddress & transport)
{
  switch (transport.GetTag()) {
    case H245_TransportAddress::e_unicastAddress :
    {
      const H245_UnicastAddress & unicast = transport;
      switch (unicast.GetTag()) {
        case H245_UnicastAddress::e_iPAddress :
        {
          const H245_UnicastAddress_iPAddress & ip = unicast;
          *this = BuildIP(PIPSocket::Address(ip.m_network.GetSize(), ip.m_network.GetValue()), ip.m_tsapIdentifier);
          break;
        }
#if P_HAS_IPV6
        case H245_UnicastAddress::e_iP6Address :
        {
          const H245_UnicastAddress_iP6Address & ip = unicast;
          *this = BuildIP(PIPSocket::Address(ip.m_network.GetSize(), ip.m_network.GetValue()), ip.m_tsapIdentifier);
          break;
        }
#endif
      }
      break;
    }
  }
}


H323TransportAddress::H323TransportAddress(const PIPSocket::Address & ip, WORD port)
{
  *this = BuildIP(ip, port);
}


void H323TransportAddress::Validate()
{
  if (IsEmpty())
    return;

  if (Find('$') == P_MAX_INDEX) {
    Splice(IpPrefix, 0, 0);
    return;
  }

  if (strncmp(theArray, IpPrefix, 3) == 0) {
    return;
  }

  *this = PString();
}


PBoolean H323TransportAddress::SetPDU(H225_TransportAddress & pdu) const
{
  PIPSocket::Address ip;
  WORD port = H323EndPoint::DefaultTcpPort;
  if (GetIpAndPort(ip, port)) {
#if P_HAS_IPV6
    if (ip.GetVersion() == 6) {
      pdu.SetTag(H225_TransportAddress::e_ip6Address);
      H225_TransportAddress_ip6Address & addr = pdu;
      for (PINDEX i = 0; i < ip.GetSize(); i++)
        addr.m_ip[i] = ip[i];
      addr.m_port = port;
      return TRUE;
    }
#endif

    pdu.SetTag(H225_TransportAddress::e_ipAddress);
    H225_TransportAddress_ipAddress & addr = pdu;
    for (PINDEX i = 0; i < 4; i++)
      addr.m_ip[i] = ip[i];
    addr.m_port = port;
    return TRUE;
  }

  return FALSE;
}


PBoolean H323TransportAddress::SetPDU(H245_TransportAddress & pdu) const
{
  PIPSocket::Address ip;
  WORD port = 0;
  if (GetIpAndPort(ip, port)) {
    pdu.SetTag(H245_TransportAddress::e_unicastAddress);

    H245_UnicastAddress & unicast = pdu;

#if P_HAS_IPV6
    if (ip.GetVersion() == 6) {
      unicast.SetTag(H245_UnicastAddress::e_iP6Address);
      H245_UnicastAddress_iP6Address & addr = unicast;
      for (PINDEX i = 0; i < ip.GetSize(); i++)
        addr.m_network[i] = ip[i];
      addr.m_tsapIdentifier = port;
      return TRUE;
    }
#endif

    unicast.SetTag(H245_UnicastAddress::e_iPAddress);
    H245_UnicastAddress_iPAddress & addr = unicast;
    for (PINDEX i = 0; i < 4; i++)
      addr.m_network[i] = ip[i];
    addr.m_tsapIdentifier = port;
    return TRUE;
  }

  return FALSE;
}


PBoolean H323TransportAddress::IsEquivalent(const H323TransportAddress & address)
{
  if (*this == address)
    return TRUE;

  if (IsEmpty() || address.IsEmpty())
    return FALSE;

  PIPSocket::Address ip1, ip2;
  WORD port1 = 65535, port2 = 65535;
  return GetIpAndPort(ip1, port1) &&
         address.GetIpAndPort(ip2, port2) &&
         (ip1.IsAny() || ip2.IsAny() || ip1 == ip2) &&
         (port1 == 65535 || port2 == 65535 || port1 == port2);
}


PBoolean H323TransportAddress::GetIpAddress(PIPSocket::Address & ip) const
{
  WORD dummy = 65535;
  return GetIpAndPort(ip, dummy);
}


static PBoolean SplitAddress(const PString & addr, PString & host, PString & service)
{
  if (strncmp(addr, IpPrefix, 3) != 0) {
    PTRACE(2, "H323\tUse of non IP transport address: \"" << addr << '"');
    return FALSE;
  }

  PINDEX lastChar = addr.GetLength()-1;
  if (addr[lastChar] == '+')
    lastChar--;

  PINDEX bracket = addr.FindLast(']');
  if (bracket == P_MAX_INDEX)
    bracket = 0;

  PINDEX colon = addr.Find(':', bracket);
  if (colon == P_MAX_INDEX)
    host = addr(3, lastChar);
  else {
    host = addr.Mid(3, colon-3);
    service = addr.Mid(colon+1, lastChar);
  }

  return TRUE;
}


PBoolean H323TransportAddress::GetIpAndPort(PIPSocket::Address & ip,
                                        WORD & port,
                                        const char * proto) const
{
  PString host, service;
  if (!SplitAddress(*this, host, service))
    return FALSE;

  if (host.IsEmpty()) {
    PTRACE(2, "H323\tIllegal IP transport address: \"" << *this << '"');
    return FALSE;
  }

  if (service == "*")
    port = 0;
  else {
    if (!service)
      port = PIPSocket::GetPortByService(proto, service);
    if (port == 0) {
      PTRACE(2, "H323\tIllegal IP transport port/service: \"" << *this << '"');
      return FALSE;
    }
  }

  if (host == "*") {
    ip = PIPSocket::GetDefaultIpAny();
    return TRUE;
  }

  if (PIPSocket::GetHostAddress(host, ip))
    return TRUE;

  PTRACE(1, "H323\tCould not find host : \"" << host << '"');
  return FALSE;
}


PString H323TransportAddress::GetHostName() const
{
  PString host, service;
  if (!SplitAddress(*this, host, service))
    return *this;

  PIPSocket::Address ip;
  if (PIPSocket::GetHostAddress(host, ip))
    return ip.AsString();

  return host;
}


H323Listener * H323TransportAddress::CreateListener(H323EndPoint & endpoint) const
{
  /*Have transport type name, create the transport object. Hard coded at the
    moment but would like to add some sort of "registration" of transport
    classes so new transports can be added without changing this source file
    every time. As we have one transport type at the moment and may never
    actually have another, we hard code it for now.
   */

  PIPSocket::Address ip;
  WORD port = H323EndPoint::DefaultTcpPort;
  if (GetIpAndPort(ip, port))
    return new H323ListenerTCP(endpoint, ip, port, theArray[GetLength()-1] != '+');

  return NULL;
}


H323Listener * H323TransportAddress::CreateCompatibleListener(H323EndPoint & endpoint) const
{
  /*Have transport type name, create the transport object. Hard coded at the
    moment but would like to add some sort of "registration" of transport
    classes so new transports can be added without changing this source file
    every time. As we have one transport type at the moment and may never
    actually have another, we hard code it for now.
   */

  PIPSocket::Address ip;
  if (GetIpAddress(ip))
    return new H323ListenerTCP(endpoint, ip, 0, FALSE);

  return NULL;
}


H323Transport * H323TransportAddress::CreateTransport(H323EndPoint & endpoint) const
{
  /*Have transport type name, create the transport object. Hard coded at the
    moment but would like to add some sort of "registration" of transport
    classes so new transports can be added without changing this source file
    every time. As we have one transport type at the moment and may never
    actually have another, we hard code it for now.
   */

  if (strncmp(theArray, IpPrefix, 3) == 0)
    return new H323TransportTCP(endpoint);

  return NULL;
}


H323TransportAddressArray H323GetInterfaceAddresses(const H323ListenerList & listeners,
                                                    PBoolean excludeLocalHost,
                                                    H323Transport * associatedTransport)
{
  H323TransportAddressArray interfaceAddresses;

  PINDEX i;
  for (i = 0; i < listeners.GetSize(); i++) {
    H323TransportAddressArray newAddrs = H323GetInterfaceAddresses(listeners[i].GetTransportAddress(), excludeLocalHost, associatedTransport);
    PINDEX size  = interfaceAddresses.GetSize();
    PINDEX nsize = newAddrs.GetSize();
    interfaceAddresses.SetSize(size + nsize);
    PINDEX j;
    for (j = 0; j < nsize; j++)
      interfaceAddresses.SetAt(size + j, new H323TransportAddress(newAddrs[j]));
  }

  return interfaceAddresses;
}


H323TransportAddressArray H323GetInterfaceAddresses(const H323TransportAddress & addr,
                                                    PBoolean excludeLocalHost,
                                                    H323Transport * associatedTransport)
{
  PIPSocket::Address ip;
  WORD port;
  if (!addr.GetIpAndPort(ip, port) || !ip.IsAny())
    return addr;

  PIPSocket::InterfaceTable interfaces;
  if (!PIPSocket::GetInterfaceTable(interfaces))
    return addr;

  if (interfaces.GetSize() == 1)
    return H323TransportAddress(interfaces[0].GetAddress(), port);

  PINDEX i;
  H323TransportAddressArray interfaceAddresses;
  PIPSocket::Address firstAddress(0);

  if (associatedTransport != NULL) {
    if (associatedTransport->GetLocalAddress().GetIpAddress(firstAddress)) {
      for (i = 0; i < interfaces.GetSize(); i++) {
        PIPSocket::Address ip = interfaces[i].GetAddress();
        if (ip == firstAddress)
          interfaceAddresses.Append(new H323TransportAddress(ip, port));
      }
    }
  }

  for (i = 0; i < interfaces.GetSize(); i++) {
    PIPSocket::Address ip = interfaces[i].GetAddress();
    if (ip != firstAddress && !(excludeLocalHost && ip.IsLoopback()))
      interfaceAddresses.Append(new H323TransportAddress(ip, port));
  }

  return interfaceAddresses;
}


void H323SetTransportAddresses(const H323Transport & associatedTransport,
                               const H323TransportAddressArray & addresses,
                               H225_ArrayOf_TransportAddress & pdu)
{
  for (PINDEX i = 0; i < addresses.GetSize(); i++) {
    H323TransportAddress addr = addresses[i];

    PIPSocket::Address ip;
    WORD port;
    if (addr.GetIpAndPort(ip, port)) {
      PIPSocket::Address remoteIP;
      if (associatedTransport.GetRemoteAddress().GetIpAddress(remoteIP)) {
        associatedTransport.GetEndPoint().InternalTranslateTCPAddress(ip, remoteIP);
	associatedTransport.GetEndPoint().TranslateTCPPort(port,remoteIP);
        addr = H323TransportAddress(ip, port);
      }
    }

    if (addresses.GetSize() > 1 && ip.IsLoopback())
      continue;

    PTRACE(4, "TCP\tAppending H.225 transport " << addr
           << " using associated transport " << associatedTransport);

    H225_TransportAddress pduAddr;
    addr.SetPDU(pduAddr);

    PINDEX lastPos = pdu.GetSize();

    // Check for already have had that address.
    PINDEX j;
    for (j = 0; j < lastPos; j++) {
      if (pdu[j] == pduAddr)
        break;
    }

    if (j >= lastPos) {
      // Put new listener into array
      pdu.SetSize(lastPos+1);
      pdu[lastPos] = pduAddr;
    }
  }
}


/////////////////////////////////////////////////////////////////////////////

H323TransportAddressArray::H323TransportAddressArray(const H225_ArrayOf_TransportAddress & addresses)
{
  for (PINDEX i = 0; i < addresses.GetSize(); i++)
    AppendAddress(H323TransportAddress(addresses[i]));
}


void H323TransportAddressArray::AppendString(const char * str)
{
  AppendAddress(H323TransportAddress(str));
}


void H323TransportAddressArray::AppendString(const PString & str)
{
  AppendAddress(H323TransportAddress(str));
}


void H323TransportAddressArray::AppendAddress(const H323TransportAddress & addr)
{
  if (!addr)
    Append(new H323TransportAddress(addr));
}


void H323TransportAddressArray::AppendStringCollection(const PCollection & coll)
{
  for (PINDEX i = 0; i < coll.GetSize(); i++) {
    PObject * obj = coll.GetAt(i);
    if (obj != NULL && PIsDescendant(obj, PString))
      AppendAddress(H323TransportAddress(*(PString *)obj));
  }
}


/////////////////////////////////////////////////////////////////////////////

H323Listener::H323Listener(H323EndPoint & end)
  : PThread(end.GetListenerThreadStackSize(),
            NoAutoDeleteThread,
            NormalPriority,
            "H323 Listener:%0x"),
    endpoint(end)
{
}


void H323Listener::PrintOn(ostream & strm) const
{
  strm << "Listener[" << GetTransportAddress() << ']';
}


/////////////////////////////////////////////////////////////////////////////

H323Transport::H323Transport(H323EndPoint & end)
  : endpoint(end)
{
  thread = NULL;
  canGetInterface = FALSE;
}


H323Transport::~H323Transport()
{
  PAssert(thread == NULL, PLogicError);
}


void H323Transport::PrintOn(ostream & strm) const
{
  strm << "Transport[";
  H323TransportAddress addr = GetRemoteAddress();
  if (!addr)
    strm << "remote=" << addr << ' ';
  strm << "if=" << GetLocalAddress() << ']';
}


PBoolean H323Transport::Close()
{
  PTRACE(3, "H323\tH323Transport::Close");

  /* Do not use PIndirectChannel::Close() as this deletes the sub-channel
     member field crashing the background thread. Just close the base
     sub-channel so breaks the threads I/O block.
   */
  if (IsOpen()) {
    channelPointerMutex.StartRead();
    GetBaseReadChannel()->Close();
    channelPointerMutex.EndRead();
  }

  return TRUE;
}

PBoolean H323Transport::HandleSignallingSocket(H323SignalPDU & pdu)
{

  for (;;) {
	  H323SignalPDU rpdu;
	  if (!rpdu.Read(*this)) { 
            return FALSE;
	  }
	  else if ((rpdu.GetQ931().GetMessageType() == Q931::InformationMsg) &&
              endpoint.OnUnsolicitedInformation(rpdu)) {
           // Handle unsolicited Information Message
                ;
	  } 
    else {
		  pdu = rpdu;
		  return TRUE;
	  }	
  }
	  
  return FALSE;
}

PBoolean H323Transport::HandleFirstSignallingChannelPDU(PThread * thread)
{
  PTRACE(3, "H225\tAwaiting first PDU");
  SetReadTimeout(15000); // Await 15 seconds after connect for first byte
  H323SignalPDU pdu;
//  if (!pdu.Read(*this)) {
  if (!HandleSignallingSocket(pdu)) {
    PTRACE(1, "H225\tFailed to get initial Q.931 PDU, connection not started.");
    return FALSE;
  }

  unsigned callReference = pdu.GetQ931().GetCallReference();
  PTRACE(3, "H225\tIncoming call, first PDU: callReference=" << callReference);

  // Get a new (or old) connection from the endpoint
  H323Connection * connection = endpoint.OnIncomingConnection(this, pdu);
  if (connection == NULL) {
    PTRACE(1, "H225\tEndpoint could not create connection, "
              "sending release complete PDU: callRef=" << callReference);
   
    H323SignalPDU releaseComplete;
    Q931 &q931PDU = releaseComplete.GetQ931();
    q931PDU.BuildReleaseComplete(callReference, TRUE);
    releaseComplete.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_releaseComplete);

    H225_ReleaseComplete_UUIE &release = releaseComplete.m_h323_uu_pdu.m_h323_message_body;
    release.m_protocolIdentifier.SetValue(psprintf("0.0.8.2250.0.%u", H225_PROTOCOL_VERSION));

    H225_Setup_UUIE &setup = pdu.m_h323_uu_pdu.m_h323_message_body;
    if (setup.HasOptionalField(H225_Setup_UUIE::e_callIdentifier)) {
       release.IncludeOptionalField(H225_Setup_UUIE::e_callIdentifier);
       release.m_callIdentifier = setup.m_callIdentifier;
    }

    // Set the cause value
    q931PDU.SetCause(Q931::TemporaryFailure);

    // Send the PDU
    releaseComplete.Write(*this);
    return FALSE;
  }

  connection->Lock();

  // handle the first PDU
  if (connection->HandleSignalPDU(pdu)) {

#ifdef H323_SIGNAL_AGGREGATE
    // if the endpoint is using signalling aggregation, we need to add this connection
    // to the signalling aggregator. 
    if (connection != NULL && endpoint.GetSignallingAggregator() != NULL) {
      connection->AggregateSignalChannel(this);
      connection->Unlock();
      return TRUE;
    }
#endif

    // If aggregation is not being used, then this thread is attached to the transport, 
    // which is in turn attached to the connection so everything from gets cleaned up by the 
    // H323 cleaner thread from now on. So thread must not auto delete and the "transport" 
    // variable is not deleted either
    PAssert(PIsDescendant(thread, H225TransportThread), PInvalidCast);
    ((H225TransportThread *)thread)->ConnectionEstablished();
    AttachThread(thread);
    thread->SetNoAutoDelete();

    connection->Unlock();

    // All subsequent PDU's should wait forever
    SetReadTimeout(PMaxTimeInterval);
    connection->HandleSignallingChannel();
  }
  else {
    connection->ClearCall(H323Connection::EndedByTransportFail);
    connection->Unlock();
    PTRACE(1, "H225\tSignal channel stopped on first PDU.");
  }


  return TRUE;
}


void H323Transport::StartControlChannel(H323Connection & connection)
{
  new H245TransportThread(endpoint, connection, *this);
}


void H323Transport::AttachThread(PThread * thrd)
{
  PAssert(thread == NULL, PLogicError);
  thread = thrd;
}


void H323Transport::CleanUpOnTermination()
{
  Close();

  if (thread != NULL) {
    PTRACE(3, "H323\tH323Transport::CleanUpOnTermination for " << thread->GetThreadName());
    PAssert(thread->WaitForTermination(10000), "Transport thread did not terminate");
    delete thread;
    thread = NULL;
  }
}


PBoolean H323Transport::IsCompatibleTransport(const H225_TransportAddress & /*pdu*/) const
{
  PAssertAlways(PUnimplementedFunction);
  return FALSE;
}


void H323Transport::SetUpTransportPDU(H225_TransportAddress & /*pdu*/,
                                      PBoolean /*localTsap*/,
									  H323Connection * /*connection*/
									  ) const
{
  PAssertAlways(PUnimplementedFunction);
}


void H323Transport::SetUpTransportPDU(H245_TransportAddress & /*pdu*/,
                                      unsigned /*port*/) const
{
  PAssertAlways(PUnimplementedFunction);
}


void H323Transport::SetPromiscuous(PromisciousModes /*promiscuous*/)
{
}


H323TransportAddress H323Transport::GetLastReceivedAddress() const
{
  return GetRemoteAddress();
}


H323Transport * H323Transport::CreateControlChannel(H323Connection & /*connection*/)
{
  PAssertAlways(PUnimplementedFunction);
  return NULL;
}


PBoolean H323Transport::AcceptControlChannel(H323Connection & /*connection*/)
{
  PAssertAlways(PUnimplementedFunction);
  return FALSE;
}


PBoolean H323Transport::DiscoverGatekeeper(H323Gatekeeper & /*gk*/,
                                       H323RasPDU & /*pdu*/,
                                       const H323TransportAddress & /*address*/)
{
  PAssertAlways(PUnimplementedFunction);
  return FALSE;
}


/////////////////////////////////////////////////////////////////////////////

H323ListenerTCP::H323ListenerTCP(H323EndPoint & end,
                                 PIPSocket::Address binding,
                                 WORD port,
                                 PBoolean exclusive)
  : H323Listener(end),
	  listener((port == 0) ? (WORD)H323EndPoint::DefaultTcpPort : port),
    localAddress(binding)
{
  exclusiveListener = exclusive;
}


H323ListenerTCP::~H323ListenerTCP()
{
  Close();
}


PBoolean H323ListenerTCP::Open()
{
  if (listener.Listen(localAddress, 100, 0,
                      exclusiveListener ? PSocket::AddressIsExclusive
                                        : PSocket::CanReuseAddress))
    return TRUE;

  PTRACE(1, "TCP\tListen on " << localAddress << ':' << listener.GetPort()
         << " failed: " << listener.GetErrorText());
  return FALSE;
}


PBoolean H323ListenerTCP::Close()
{
  PBoolean ok = listener.Close();

  PAssert(PThread::Current() != this, PLogicError);

  if (!IsTerminated() && !IsSuspended())
    PAssert(WaitForTermination(10000), "Listener thread did not terminate");

  return ok;
}


H323Transport * H323ListenerTCP::Accept(const PTimeInterval & timeout)
{
  if (!listener.IsOpen())
    return NULL;

  listener.SetReadTimeout(timeout); // Wait for remote connect

  PTRACE(4, "TCP\tWaiting on socket accept on " << GetTransportAddress());
  PTCPSocket * socket = new PTCPSocket;
  if (socket->Accept(listener)) {
    H323TransportTCP * transport = new H323TransportTCP(endpoint);
    if (transport->Open(socket))
      return transport;

    PTRACE(1, "TCP\tFailed to open transport, connection not started.");
    delete transport;
    return NULL;
  }

  if (socket->GetErrorCode() != PChannel::Interrupted) {
    PTRACE(1, "TCP\tAccept error:" << socket->GetErrorText());
    listener.Close();
  }

  delete socket;
  return NULL;
}


H323TransportAddress H323ListenerTCP::GetTransportAddress() const
{
  return H323TransportAddress(localAddress, listener.GetPort());
}


PBoolean H323ListenerTCP::SetUpTransportPDU(H245_TransportAddress & pdu,
                                        const H323Transport & associatedTransport)
{
  if (!localAddress.IsAny())
    return GetTransportAddress().SetPDU(pdu);

  PIPSocket::Address addressOfExistingInterface;
  if (!associatedTransport.GetLocalAddress().GetIpAddress(addressOfExistingInterface))
    return FALSE;

  H323TransportAddress transAddr(addressOfExistingInterface, listener.GetPort());
  transAddr.SetPDU(pdu);
  return TRUE;
}


void H323ListenerTCP::Main()
{
  PTRACE(2, "H323\tAwaiting TCP connections on port " << listener.GetPort());

  while (listener.IsOpen()) {
    H323Transport * transport = Accept(PMaxTimeInterval);
    if (transport != NULL)
      new H225TransportThread(endpoint, transport);
  }
}


/////////////////////////////////////////////////////////////////////////////

H323TransportIP::H323TransportIP(H323EndPoint & end, PIPSocket::Address binding, WORD remPort)
  : H323Transport(end),
    localAddress(binding),
    remoteAddress(0)
{
  localPort = 0;
  remotePort = remPort;
}


H323TransportAddress H323TransportIP::GetLocalAddress() const
{
  return H323TransportAddress(localAddress, localPort);
}


H323TransportAddress H323TransportIP::GetRemoteAddress() const
{
  return H323TransportAddress(remoteAddress, remotePort);
}


PBoolean H323TransportIP::IsCompatibleTransport(const H225_TransportAddress & pdu) const
{
  return pdu.GetTag() == H225_TransportAddress::e_ipAddress
#if P_HAS_IPV6
            || pdu.GetTag() == H225_TransportAddress::e_ip6Address
#endif
         ;
}


void H323TransportIP::SetUpTransportPDU(H225_TransportAddress & pdu, PBoolean localTsap,H323Connection * connection) const
{
  H323TransportAddress transAddr;
  if (!localTsap) 
    transAddr = H323TransportAddress(remoteAddress, remotePort);
  else {
    H323TransportAddress tAddr = GetLocalAddress();
    PIPSocket::Address ipAddr; 
    tAddr.GetIpAddress(ipAddr);
    endpoint.InternalTranslateTCPAddress(ipAddr, remoteAddress,connection);
    WORD tPort = localPort;
    endpoint.TranslateTCPPort(tPort,remoteAddress);
    transAddr = H323TransportAddress(ipAddr, tPort);
  }

  transAddr.SetPDU(pdu);
}


void H323TransportIP::SetUpTransportPDU(H245_TransportAddress & pdu, unsigned port) const
{
  PIPSocket::Address ipAddr = localAddress;
  endpoint.InternalTranslateTCPAddress(ipAddr, remoteAddress);

  switch (port) {
    case UseLocalTSAP :
      port = localPort;
      break;
    case UseRemoteTSAP :
      port = remotePort;
      break;
  }

  H323TransportAddress transAddr(ipAddr, (WORD)port);
  transAddr.SetPDU(pdu);
}


/////////////////////////////////////////////////////////////////////////////

H323TransportTCP::H323TransportTCP(H323EndPoint & end,
                                   PIPSocket::Address binding,
                                   PBoolean listen)
  : H323TransportIP(end, binding, H323EndPoint::DefaultTcpPort)
{
  h245listener = NULL;

  // construct listener socket if required
  if (listen) {
    h245listener = new PTCPSocket;

    localPort = end.GetNextTCPPort();
    WORD firstPort = localPort;
    while (!h245listener->Listen(binding, 5, localPort)) {
      localPort = end.GetNextTCPPort();
      if (localPort == firstPort)
        break;
    }

    if (h245listener->IsOpen()) {
      localPort = h245listener->GetPort();
      PTRACE(3, "H225\tTCP Listen for H245 on " << binding << ':' << localPort);
    }
    else {
      PTRACE(1, "H225\tTCP Listen for H245 failed: " << h245listener->GetErrorText());
      delete h245listener;
      h245listener = NULL;
    }
  }
}


H323TransportTCP::~H323TransportTCP()
{
  delete h245listener;  // Delete any H245 listener that may be present
}


PBoolean H323TransportTCP::OnOpen()
{
  PIPSocket * socket = (PIPSocket *)GetReadChannel();

  // Get name of the remote computer for information purposes
  if (!socket->GetPeerAddress(remoteAddress, remotePort)) {
    PTRACE(1, "H323TCP\tGetPeerAddress() failed: " << socket->GetErrorText());
    return FALSE;
  }

  // get local address of incoming socket to ensure that multi-homed machines
  // use a NIC address that is guaranteed to be addressable to destination
  if (!socket->GetLocalAddress(localAddress, localPort)) {
    PTRACE(1, "H323TCP\tGetLocalAddress() failed: " << socket->GetErrorText());
    return FALSE;
  }

  if (!socket->SetOption(TCP_NODELAY, 1, IPPROTO_TCP)) {
    PTRACE(1, "H323TCP\tSetOption(TCP_NODELAY) failed: " << socket->GetErrorText());
  }

  //if (!socket->SetOption(IP_TOS, endpoint.GetTcpIpTypeofService(), IPPROTO_IP)) { 
  //  PTRACE(1, "H323TCP\tSetOption(IP_TOS) failed: " << socket->GetErrorText()); 
  //}


#ifndef P_VXWORKS // VxWorks has alternative behaviour, so skip it
  // make sure do not lose outgoing packets on close
  const linger ling = { 1, 3 };
  if (!socket->SetOption(SO_LINGER, &ling, sizeof(ling))) {
    PTRACE(1, "H323TCP\tSetOption(SO_LINGER) failed: " << socket->GetErrorText());
    return FALSE;
  }
#endif //P_VXWORKS

  PTRACE(2, "H323TCP\tStarted connection: "
            " host=" << remoteAddress << ':' << remotePort << ","
            " if=" << localAddress << ':' << localPort << ","
            " handle=" << socket->GetHandle());

  return TRUE;
}


PBoolean H323TransportTCP::Close()
{
  // Close listening socket to break waiting accept
  if (IsListening())
    h245listener->Close();

  return H323Transport::Close();
}


PBoolean H323TransportTCP::SetRemoteAddress(const H323TransportAddress & address)
{
  return address.GetIpAndPort(remoteAddress, remotePort, "tcp");
}

PBoolean H323TransportTCP::ExtractPDU(const PBYTEArray & pdu, PINDEX & pduLen)
{
  //
  // TPKT format is :
  //   byte 0   = type identifier - always 0x03
  //   byte 1   = ignored
  //   byte 2   = msb of data length
  //   byte 3   = lsb of data length
  //   byte xx  = data of length
  //
  // this gives minimum length of 4 bytes

  // ensure length is at least one byte
  if (pduLen < 1) {
    pduLen = 0;
    return TRUE;
  }

  // only accept TPKT of type 3
  if (pdu[0] != 3)
    return SetErrorValues(Miscellaneous, 0x41000000);

  // check for minimum header length
  if (pduLen < 4) {
    pduLen = 0;
    return TRUE;
  }

  // see if complete PDU received yet
  PINDEX dataLen = (pdu[2] << 8)|pdu[3];

  // dwarf PDUs are errors
  if (dataLen < 4) {
    PTRACE(1, "H323TCP\tDwarf PDU received (length " << dataLen << ")");
    return FALSE;
  }

  // wait for data to arrive
  if (pduLen < dataLen) {
    pduLen = 0;
    return TRUE;
  }

  // set the length of the complete PDU
  pduLen = dataLen;

  return TRUE;
}

PBoolean H323TransportTCP::ReadPDU(PBYTEArray & pdu)
{
  // Make sure is a RFC1006 TPKT
  switch (ReadChar()) {
    case -1 :
      return FALSE;

    case 3 :  // Only support version 3
      break;

    default :  // Unknown version number
      return SetErrorValues(Miscellaneous, 0x41000000);
  }

  // Save timeout
  PTimeInterval oldTimeout = GetReadTimeout();

  // Should get all of PDU in 5 seconds or something is seriously wrong,
  SetReadTimeout(5000);

  // Get TPKT length
  BYTE header[3];
  PBoolean ok = ReadBlock(header, sizeof(header));
  if (ok) {
    PINDEX packetLength = ((header[1] << 8)|header[2]);
    if (packetLength < 4) {
      PTRACE(1, "H323TCP\tDwarf PDU received (length " << packetLength << ")");
      ok = FALSE;
    } else {
      packetLength -= 4;
      ok = ReadBlock(pdu.GetPointer(packetLength), packetLength);
    }
  }

  SetReadTimeout(oldTimeout);

  return ok;
}


PBoolean H323TransportTCP::WritePDU(const PBYTEArray & pdu)
{
  // We copy the data into a new buffer so we can do a single write call. This
  // is necessary as we have disabled the Nagle TCP delay algorithm to improve
  // network performance.

  int packetLength = pdu.GetSize() + 4;

  // Send RFC1006 TPKT length
  PBYTEArray tpkt(packetLength);
  tpkt[0] = 3;
  tpkt[1] = 0;
  tpkt[2] = (BYTE)(packetLength >> 8);
  tpkt[3] = (BYTE)packetLength;
  memcpy(tpkt.GetPointer()+4, (const BYTE *)pdu, pdu.GetSize());

  return Write((const BYTE *)tpkt, packetLength);
}


PBoolean H323TransportTCP::Connect()
{
  if (IsListening())
    return TRUE;

  PTCPSocket * socket = new PTCPSocket(remotePort);
  Open(socket);

  channelPointerMutex.StartRead();

  socket->SetReadTimeout(endpoint.GetSignallingChannelConnectTimeout());

  localPort = endpoint.GetNextTCPPort();
  WORD firstPort = localPort;
  for (;;) {
    PTRACE(4, "H323TCP\tConnecting to "
           << remoteAddress << ':' << remotePort
           << " (local port=" << localPort << ')');
    if (socket->Connect(localAddress, localPort, remoteAddress))
      break;

    int errnum = socket->GetErrorNumber();
    if (localPort == 0 || (errnum != EADDRINUSE && errnum != EADDRNOTAVAIL)) {
      PTRACE(1, "H323TCP\tCould not connect to "
                << remoteAddress << ':' << remotePort
                << " (local port=" << localPort << ") - "
                << socket->GetErrorText() << '(' << errnum << ')');
      channelPointerMutex.EndRead();
      return SetErrorValues(socket->GetErrorCode(), errnum);
    }

    localPort = endpoint.GetNextTCPPort();
    if (localPort == firstPort) {
      PTRACE(1, "H323TCP\tCould not bind to any port in range " <<
                endpoint.GetTCPPortBase() << " to " << endpoint.GetTCPPortMax());
      channelPointerMutex.EndRead();
      return SetErrorValues(socket->GetErrorCode(), errnum);
    }
  }

  socket->SetReadTimeout(PMaxTimeInterval);

  channelPointerMutex.EndRead();

  return OnOpen();
}


H323Transport * H323TransportTCP::CreateControlChannel(H323Connection & connection)
{
  H323TransportTCP * tcpTransport = new H323TransportTCP(endpoint, localAddress, TRUE);
  tcpTransport->SetRemoteAddress(GetRemoteAddress());
  if (tcpTransport->IsListening()) // Listen() failed
    return tcpTransport;

  delete tcpTransport;
  connection.ClearCall(H323Connection::EndedByTransportFail);
  return FALSE;
}


PBoolean H323TransportTCP::AcceptControlChannel(H323Connection & connection)
{
  if (IsOpen())
    return TRUE;

  if (h245listener == NULL) {
    PAssertAlways(PLogicError);
    return FALSE;
  }

  PTRACE(3, "H245\tTCP Accept wait");

  PTCPSocket * h245Socket = new PTCPSocket;

  h245listener->SetReadTimeout(endpoint.GetControlChannelStartTimeout());
  if (h245Socket->Accept(*h245listener))
    return Open(h245Socket);

  PTRACE(1, "H225\tAccept for H245 failed: " << h245Socket->GetErrorText());
  delete h245Socket;

  if (h245listener->IsOpen() &&
      connection.IsConnected() &&
      connection.FindChannel(RTP_Session::DefaultAudioSessionID, TRUE) == NULL &&
      connection.FindChannel(RTP_Session::DefaultAudioSessionID, FALSE) == NULL)
    connection.ClearCall(H323Connection::EndedByTransportFail);

  return FALSE;
}


PBoolean H323TransportTCP::IsListening() const
{
  if (IsOpen())
    return FALSE;

  if (h245listener == NULL)
    return FALSE;

  return h245listener->IsOpen();
}


/////////////////////////////////////////////////////////////////////////////

static PBoolean ListenUDP(PUDPSocket & socket,
                      H323EndPoint & endpoint,
                      PIPSocket::Address binding,
                      WORD localPort)
{
  if (localPort > 0) {
    if (socket.Listen(binding, 0, localPort))
      return TRUE;
  }
  else {
    localPort = endpoint.GetNextUDPPort();
    WORD firstPort = localPort;

    for (;;) {
      if (socket.Listen(binding, 0, localPort))
        return TRUE;

      int errnum = socket.GetErrorNumber();
      if (errnum != EADDRINUSE && errnum != EADDRNOTAVAIL)
        break;

      localPort = endpoint.GetNextUDPPort();
      if (localPort == firstPort) {
        PTRACE(1, "H323UDP\tCould not bind to any port in range " <<
                  endpoint.GetUDPPortBase() << " to " << endpoint.GetUDPPortMax());
        return FALSE;
      }
    }
  }

  PTRACE(1, "H323UDP\tCould not bind to "
            << binding << ':' << localPort << " - "
            << socket.GetErrorText() << '(' << socket.GetErrorNumber() << ')');
  return FALSE;
}


H323TransportUDP::H323TransportUDP(H323EndPoint & ep,
                                   PIPSocket::Address binding,
                                   WORD local_port,
                                   WORD remote_port)
  : H323TransportIP(ep, binding, remote_port)
{
  if (remotePort == 0)
    remotePort = H225_RAS::DefaultRasUdpPort; // For backward compatibility

  promiscuousReads = AcceptFromRemoteOnly;

  PUDPSocket * udp = new PUDPSocket;
  ListenUDP(*udp, ep, binding, local_port);

  interfacePort = localPort = udp->GetPort();

  Open(udp);

  PTRACE(3, "H323UDP\tBinding to interface: " << binding << ':' << localPort);

#if PTLIB_VER >= 2110
  canGetInterface = binding.IsAny();
#else
  canGetInterface = (binding.IsAny()) && udp->SetCaptureReceiveToAddress();
#endif
}


H323TransportUDP::~H323TransportUDP()
{
  Close();
}


PBoolean H323TransportUDP::SetRemoteAddress(const H323TransportAddress & address)
{
  return address.GetIpAndPort(remoteAddress, remotePort, "udp");
}


PBoolean H323TransportUDP::Connect()
{
  if (remoteAddress == 0 || remotePort == 0)
    return FALSE;

  PUDPSocket * socket;

#ifdef P_STUN
  PSTUNClient * stun = endpoint.GetSTUN(remoteAddress);
  if (stun != NULL) {
#if PTLIB_VER >= 2110
    if (stun->CreateSocket(PNatMethod::eComponent_Unknown,socket)) {
#else
    if (stun->CreateSocket(socket)) {
#endif
      Open(socket);
      socket->GetLocalAddress(localAddress, localPort);
      PTRACE(4, "H323UDP\tSTUN created socket: " << localAddress << ':' << localPort);
    }
    else
      PTRACE(4, "H323UDP\tSTUN could not create socket!");
  }
#endif

  socket = (PUDPSocket *)GetReadChannel();
  socket->SetSendAddress(remoteAddress, remotePort);

  return TRUE;
}


void H323TransportUDP::SetPromiscuous(PromisciousModes promiscuous)
{
  promiscuousReads = promiscuous;
}


H323TransportAddress H323TransportUDP::GetLastReceivedAddress() const
{
  if (!lastReceivedAddress)
    return lastReceivedAddress;

  return H323Transport::GetLastReceivedAddress();
}

PBoolean H323TransportUDP::ExtractPDU(const PBYTEArray & /*pdu*/, PINDEX & /*len*/)
{
  return TRUE;
}

PBoolean H323TransportUDP::ReadPDU(PBYTEArray & pdu)
{
  for (;;) {
    if (!Read(pdu.GetPointer(10000), 10000)) {
      pdu.SetSize(0);
      return FALSE;
    }

    pdu.SetSize(GetLastReadCount());

    PUDPSocket * socket = (PUDPSocket *)GetReadChannel();

#if PTLIB_VER >= 2110
    if (canGetInterface) {
      WORD notused;
      // TODO: verify that this actually does the same as the pre 2.11.x version
      socket->GetLastReceiveAddress(lastReceivedInterface, notused);
    }
#else
    if (canGetInterface)
      lastReceivedInterface = socket->GetLastReceiveToAddress();
#endif

    PIPSocket::Address address;
    WORD port;

    socket->GetLastReceiveAddress(address, port);

    switch (promiscuousReads) {
      case AcceptFromRemoteOnly :
        if (remoteAddress *= address)
          goto accept;
        break;

      case AcceptFromAnyAutoSet :
        remoteAddress = address;
        remotePort = port;
        socket->SetSendAddress(remoteAddress, remotePort);
        goto accept;

      case AcceptFromLastReceivedOnly :
        if (!lastReceivedAddress.IsEmpty()) {
          PIPSocket::Address lastAddr;
          WORD lastPort;

          if (lastReceivedAddress.GetIpAndPort(lastAddr, lastPort, "udp") &&
             (lastAddr *= address) && lastPort == port)
            goto accept;
        }
        break;

      default : //AcceptFromAny
      accept:
        lastReceivedAddress = H323TransportAddress(address, port);
        return TRUE;
    }

    PTRACE(1, "UDP\tReceived PDU from incorrect host: " << address << ':' << port);
  }
}


PBoolean H323TransportUDP::WritePDU(const PBYTEArray & pdu)
{
  return Write((const BYTE *)pdu, pdu.GetSize());
}


PBoolean H323TransportUDP::DiscoverGatekeeper(H323Gatekeeper & gk,
                                          H323RasPDU & request,
                                          const H323TransportAddress & address)
{
  PINDEX i;

  PTRACE(3, "H225\tStarted gatekeeper discovery of \"" << address << '"');

  PIPSocket::Address destAddr = INADDR_BROADCAST;
  WORD destPort = H225_RAS::DefaultRasUdpPort;
  if (!address) {
    if (!address.GetIpAndPort(destAddr, destPort, "udp")) {
      PTRACE(2, "RAS\tError decoding address");
      return FALSE;
    }
  }

  // Skip over the H323Transport::Close to make sure PUDPSocket is deleted.
  PIndirectChannel::Close();

  remoteAddress = 0;
  remotePort = 0;

  // Remember the original info for pre-bound socket
  PIPSocket::Address originalLocalAddress = localAddress;
  WORD originalLocalPort = 0;

  // Get the interfaces to try
  PIPSocket::InterfaceTable interfaces;

  // See if prebound to interface, only use that if so
  if (destAddr.IsLoopback()) {
    PTRACE(3, "RAS\tGatekeeper discovery on loopback interface");
    localAddress = destAddr;
  }
  else if (!localAddress.IsAny()) {
    PTRACE(3, "RAS\tGatekeeper discovery on pre-bound interface: "
              << localAddress << ':' << localPort);
    originalLocalPort = localPort;
  }
  else if (!PIPSocket::GetInterfaceTable(interfaces)) {
    PTRACE(1, "RAS\tNo interfaces on system!");
  }
  else {
    PTRACE(4, "RAS\tSearching interfaces:\n" << setfill('\n') << interfaces << setfill(' '));
    // Check for if destination machine is local machine, if so only use that interface
    for (i = 0; i < interfaces.GetSize(); i++) {
      if (interfaces[i].GetAddress() == destAddr) {
        PTRACE(3, "RAS\tGatekeeper discovery on local interface: " << destAddr);
        localAddress = destAddr;
        interfaces.RemoveAll();
      }
    }
  }

  if (interfaces.IsEmpty())
    interfaces.Append(new PIPSocket::InterfaceEntry("", localAddress, PIPSocket::Address(0xffffffff), ""));

#ifdef P_STUN
  PSTUNClient * stun = endpoint.GetSTUN(remoteAddress);
#endif

  PSocketList sockets;
  PSocket::SelectList selection;
  H225_GatekeeperRequest & grq = request;

  for (i = 0; i < interfaces.GetSize(); i++) {
    localAddress = interfaces[i].GetAddress();
    if (localAddress == 0 || (destAddr != localAddress && localAddress.IsLoopback()))
      continue;

    // Check for already have had that IP address.
    PINDEX j;
    for (j = 0; j < i; j++) {
      if (localAddress == interfaces[j].GetAddress())
        break;
    }
    if (j < i)
      continue;

    PUDPSocket * socket;

    static PIPSocket::Address MulticastRasAddress(224, 0, 1, 41);
    if (destAddr != MulticastRasAddress) {

#ifdef P_STUN
      // Not explicitly multicast
#if PTLIB_VER >= 2110
      if (stun != NULL && stun->CreateSocket(PNatMethod::eComponent_Unknown,socket)) {
#else
      if (stun != NULL && stun->CreateSocket(socket)) {
#endif
        socket->GetLocalAddress(localAddress, localPort);
        PTRACE(4, "H323UDP\tSTUN created socket: " << localAddress << ':' << localPort);
      }
      else
#endif
      {
        socket = new PUDPSocket;
        if (!ListenUDP(*socket, endpoint, localAddress, originalLocalPort)) {
          delete socket;
          return FALSE;
        }
        localPort = socket->GetPort();
      }

      sockets.Append(socket);

      if (destAddr == INADDR_BROADCAST) {
        if (!socket->SetOption(SO_BROADCAST, 1)) {
          PTRACE(2, "RAS\tError allowing broadcast: " << socket->GetErrorText());
          return FALSE;
        }
      }

      // Adjust the PDU to reflect the interface we are writing to.
      PIPSocket::Address ipAddr = localAddress;
      endpoint.InternalTranslateTCPAddress(ipAddr, destAddr);
      endpoint.TranslateTCPPort(localPort, destAddr);
      H323TransportAddress(ipAddr, localPort).SetPDU(grq.m_rasAddress);

      PTRACE(3, "RAS\tGatekeeper discovery on interface: " << localAddress << ':' << localPort);

      socket->SetSendAddress(destAddr, destPort);
      writeChannel = socket;
      if (request.Write(*this))
        selection.Append(socket);
      else
        PTRACE(2, "RAS\tError writing discovery PDU: " << socket->GetErrorText());

      if (destAddr == INADDR_BROADCAST)
        socket->SetOption(SO_BROADCAST, 0);
    }


#ifdef IP_ADD_MEMBERSHIP
    // Now do it again for Multicast
    if (destAddr == INADDR_BROADCAST || destAddr == MulticastRasAddress) {
      socket = new PUDPSocket;
      sockets.Append(socket);

	  if (!ListenUDP(*socket, endpoint, localAddress, 0)) {
	    writeChannel = NULL;
        return FALSE;
	  }

      localPort = socket->GetPort();

      struct ip_mreq mreq;
      mreq.imr_multiaddr = MulticastRasAddress;
      mreq.imr_interface = localAddress;    // ip address of host
      if (socket->SetOption(IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq), IPPROTO_IP)) {
        // Adjust the PDU to reflect the interface we are writing to.
        SetUpTransportPDU(grq.m_rasAddress, TRUE);

        socket->SetOption(SO_BROADCAST, 1);

        socket->SetSendAddress(INADDR_BROADCAST, H225_RAS::DefaultRasMulticastPort);
        writeChannel = socket;
        if (request.Write(*this))
          selection.Append(socket);
        else
          PTRACE(2, "RAS\tError writing discovery PDU: " << socket->GetErrorText());

        socket->SetOption(SO_BROADCAST, 0);
      }
      else
        PTRACE(2, "RAS\tError allowing multicast: " << socket->GetErrorText());
    }
#endif

    writeChannel = NULL;
  }

  if (sockets.IsEmpty()) {
    PTRACE(1, "RAS\tNo suitable interfaces for discovery!");
    return FALSE;
  }

  if (PSocket::Select(selection, endpoint.GetGatekeeperRequestTimeout()) != NoError) {
    PTRACE(3, "RAS\tError on discover request select");
    return FALSE;
  }

  SetReadTimeout(0);

  for (i = 0; i < selection.GetSize(); i++) {
    readChannel = &selection[i];
    promiscuousReads = AcceptFromAnyAutoSet;

    H323RasPDU response;
    if (!response.Read(*this)) {
      PTRACE(3, "RAS\tError on discover request read: " << readChannel->GetErrorText());
      break;
    }

    do {
      if (gk.HandleTransaction(response)) {
        if (!gk.IsDiscoveryComplete()) {
          localAddress = originalLocalAddress;
          localPort = originalLocalPort;
          promiscuousReads = AcceptFromRemoteOnly;
          readChannel = NULL;
          return TRUE;
        }

        PUDPSocket * socket = (PUDPSocket *)readChannel;
        socket->GetLocalAddress(localAddress, localPort);
        readChannel = NULL;
        if (Open(socket) && Connect()) {
          sockets.DisallowDeleteObjects();
          sockets.Remove(socket);
          sockets.AllowDeleteObjects();

          promiscuousReads = AcceptFromRemoteOnly;

          PTRACE(2, "RAS\tGatekeeper discovered at: "
                 << remoteAddress << ':' << remotePort
                 << " (if="
                 << localAddress << ':' << localPort << ')');
          return TRUE;
        }
      }
    } while (response.Read(*this));
  }

  PTRACE(2, "RAS\tGatekeeper discovery failed");
  localAddress = originalLocalAddress;
  localPort = originalLocalPort;
  promiscuousReads = AcceptFromRemoteOnly;
  readChannel = NULL;
  return FALSE;
}

H323TransportAddress H323TransportUDP::GetLocalAddress() const
{
  if (canGetInterface && !lastReceivedInterface.IsLoopback()) 
    return H323TransportAddress(lastReceivedInterface, interfacePort);

  // check for special case of local interface, which means the PDU came from the same machine
  H323TransportAddress taddr = H323TransportIP::GetLocalAddress();
  if (!lastReceivedAddress.IsEmpty()) {
    PIPSocket::Address tipAddr;
    WORD tipPort;
    taddr.GetIpAndPort(tipAddr, tipPort);
    if (tipAddr == PIPSocket::Address(0)) {
      PIPSocket::Address lastRxIPAddr;
      lastReceivedAddress.GetIpAddress(lastRxIPAddr);
      if (lastRxIPAddr != PIPSocket::Address())
        taddr = H323TransportAddress(lastRxIPAddr, tipPort);
    }
  }

  return taddr;
}

/////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
  #pragma warning(default : 4244)
#endif
