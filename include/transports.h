/*
 * transports.h
 *
 * H.323 protocol handler
 *
 * Open H323 Library
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

#ifndef __TRANSPORTS_H
#define __TRANSPORTS_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <ptlib/sockets.h>
#include "ptlib_extras.h"

#ifdef H323_TLS
#include <ptclib/pssl.h>
#endif

class H225_Setup_UUIE;
class H225_TransportAddress;
class H225_ArrayOf_TransportAddress;
class H225_TransportAddress_ipAddress;

class H245_TransportAddress;

class H323SignalPDU;
class H323RasPDU;
class H323EndPoint;
class H323Connection;
class H323Listener;
class H323Transport;
class H323Gatekeeper;

///////////////////////////////////////////////////////////////////////////////

/**String representation of a transport address.
 */

class H323TransportAddress : public PString
{
  PCLASSINFO(H323TransportAddress, PString);
  public:
    H323TransportAddress() { m_version = 4; }
    H323TransportAddress(const char *);
    H323TransportAddress(const PString &);
    H323TransportAddress(const H225_TransportAddress &);
    H323TransportAddress(const H245_TransportAddress &);
    H323TransportAddress(const PIPSocket::Address &, WORD);

    PBoolean SetPDU(H225_TransportAddress & pdu) const;
    PBoolean SetPDU(H245_TransportAddress & pdu) const;

    /**Determine if the two transport addresses are equivalent.
      */
    PBoolean IsEquivalent(
      const H323TransportAddress & address
    );

    /**Extract the ip address from transport address.
       Returns FALSE, if the address is not an IP transport address.
      */
    PBoolean GetIpAddress(
      PIPSocket::Address & ip
    ) const;

    /**Extract the ip address and port number from transport address.
       Returns FALSE, if the address is not an IP transport address.
      */
    PBoolean GetIpAndPort(
      PIPSocket::Address & ip,
      WORD & port,
      const char * proto = "tcp"
    ) const;

    /**Get the IP Version
       6 = IPv6  4 = IPv4
      */
    unsigned GetIpVersion() const;


    /**Translate the transport address to a more human readable form.
       Returns the hostname if using IP.
      */
    PString GetHostName() const;

    /**Create a listener based on this transport address.

       For example an address of "#ip$10.0.0.1:1720#" would create a TCP
       listening socket that would be bound to the specific interface
       10.0.0.1 and listens on port 1720. Note that the address
       "#ip$*:1720#" can be used to bind to INADDR_ANY.

       Also note that if the address has a trailing '+' character then the
       socket will be bound using the REUSEADDR option.
      */
    H323Listener * CreateListener(
      H323EndPoint & endpoint   ///<  Endpoint object for transport creation.
    ) const;

    /**Create a listener compatible for this address type.
       This is similar to CreateListener() but does not use the TSAP specified
       in the H323Transport. For example an address of "#ip$10.0.0.1:1720#"
       would create a TCP listening socket that would be bound to the specific
       interface 10.0.0.1 but listens on a random OS allocated port number.
      */
    H323Listener * CreateCompatibleListener(
      H323EndPoint & endpoint   ///<  Endpoint object for transport creation.
    ) const;

    /**Create a transport suitable for this address type.
      */
    H323Transport * CreateTransport(
      H323EndPoint & endpoint   ///<  Endpoint object for transport creation.
    ) const;

  protected:
    void Validate();

  private:
    unsigned m_version;
};


PDECLARE_ARRAY(H323TransportAddressArray, H323TransportAddress)
#ifdef DOC_PLUS_PLUS
{
#endif
  public:
    H323TransportAddressArray(
      const H323TransportAddress & address
    ) { AppendAddress(address); }
    H323TransportAddressArray(
      const H225_ArrayOf_TransportAddress & addresses
    );
    H323TransportAddressArray(
      const PStringArray & array
    ) { AppendStringCollection(array); }
    H323TransportAddressArray(
      const PStringList & list
    ) { AppendStringCollection(list); }
    H323TransportAddressArray(
      const PSortedStringList & list
    ) { AppendStringCollection(list); }

    void AppendString(
      const char * address
    );
    void AppendString(
      const PString & address
    );
    void AppendAddress(
      const H323TransportAddress & address
    );

  protected:
    void AppendStringCollection(
      const PCollection & coll
    );
};

///////////////////////////////////////////////////////////////////////////////

/**Transport Security Information.
 */

class H323TransportSecurity {
public:

    H323TransportSecurity(H323EndPoint * ep = NULL);

    enum Method {
        e_unsecure,
        e_tls,
        e_ipsec     // not supported YET
    };
    static PString MethodAsString(Method meth);

    enum Policy {
        e_nopolicy,             // no policy
        e_reqTLSMediaEncHigh,   // require TLS to offer high media encryption
        e_reqTLSMediaEncAll     // require TLS to offer ALL media encryption
    };
    static PString PolicyAsString(Policy policy);

    PBoolean HasSecurity();

    void EnableTLS(PBoolean enable); 
    PBoolean IsTLSEnabled() const;

    void SetRemoteTLSAddress(const H323TransportAddress & address);
    H323TransportAddress GetRemoteTLSAddress();

    void EnableIPSec(PBoolean enable); 
    PBoolean IsIPSecEnabled();

    void SetMediaPolicy(Policy policy);
    Policy GetMediaPolicy() const;

    void Reset();

protected:
    int m_securityMask;
    int m_policyMask;
    H323TransportAddress m_remoteTLSAddress;
};


/**This class describes a "listener" on a transport protocol.
   A "listener" is an object that listens for incoming connections on the
   particular transport. It is executed as a separate thread.

   The Main() function is used to handle incoming H.323 connections and
   dispatch them in new threads based on the actual H323Transport class. This
   is defined in the descendent class that knows what the low level transport
   is, eg H323ListenerIP for the TCP/IP protocol.

   An application may create a descendent off this class and override
   functions as required for operating the channel protocol.
 */
class H323Listener : public PThread
{
  PCLASSINFO(H323Listener, PThread);

  public:
  /**@name Construction */
  //@{
    /**Create a new listener.
     */
    H323Listener(
      H323EndPoint & endpoint,     ///<  Endpoint instance for channel
      H323TransportSecurity::Method security = H323TransportSecurity::e_unsecure
    );
  //@}

  /**@name Overrides from PObject */
  //@{
    virtual void PrintOn(
      ostream & strm
    ) const;
  //@}

  /**@name Operations */
  //@{
    /** Open the listener.
      */
    virtual PBoolean Open() = 0;

    /**Stop the listener thread and no longer accept incoming connections.
     */
    virtual PBoolean Close() = 0;

    /**Accept a new incoming transport.
      */
    virtual H323Transport * Accept(
      const PTimeInterval & timeout  ///<  Time to wait for incoming connection
    ) = 0;

    /**Get the local transport address on which this listener may be accessed.
      */
    virtual H323TransportAddress GetTransportAddress() const = 0;

    /**Set the local transport address on which this listener may be accessed.
      */
    virtual void SetTransportAddress(const H323TransportAddress & address) = 0;

    /**Create Transport.
      */
    virtual H323Transport * CreateTransport(const PIPSocket::Address & address) = 0;

    /**Set up a transport address PDU for bidirectional logical channels.
      */
    virtual PBoolean SetUpTransportPDU(
      H245_TransportAddress & pdu,         ///<  Transport addresses listening on
      const H323Transport & associatedTransport ///<  Associated transport for precendence and translation
    ) = 0;

    /**Get Signalling Security.
      */
    H323TransportSecurity::Method GetSecurity();

    /**Listener Type as string.
      */
    PString TypeAsString() const;
  //@}

  protected:
    H323EndPoint & endpoint;  /// Endpoint that owns the listener.
    H323TransportSecurity::Method   m_security;
};

H323_DECLARELIST(H323ListenerList, H323Listener)
#ifdef DOC_PLUS_PLUS
{
#endif
  public:
	  H323Listener * GetListener() const;
#ifdef H323_TLS
	  H323Listener * GetTLSListener() const;
#endif
};


/** Return a list of transport addresses corresponding to a listener list
  */
H323TransportAddressArray H323GetInterfaceAddresses(
  const H323ListenerList & listeners, ///<  List of listeners
  PBoolean excludeLocalHost = TRUE,       ///<  Flag to exclude 127.0.0.1
  H323Transport * associatedTransport = NULL
                          ///<  Associated transport for precedence and translation
);

H323TransportAddressArray H323GetInterfaceAddresses(
  const H323TransportAddress & addr,  ///<  Possible INADDR_ANY address
  PBoolean excludeLocalHost = TRUE,       ///<  Flag to exclude 127.0.0.1
  H323Transport * associatedTransport = NULL
                          ///<  Associated transport for precedence and translation
);

/**Set the PDU field for the list of transport addresses
  */
void H323SetTransportAddresses(
  const H323Transport & associatedTransport,   ///<  Transport for NAT address translation
  const H323TransportAddressArray & addresses, ///<  Addresses to set
  H225_ArrayOf_TransportAddress & pdu          ///<  List of PDU transport addresses
);


/**This class describes a I/O transport protocol..
   A "transport" is an object that listens for incoming connections on the
   particular transport.
 */
#ifdef H323_TLS
class H323Transport : public PSSLChannel
{
  PCLASSINFO(H323Transport, PSSLChannel);

  public:
  /**@name Construction */
  //@{
    /**Create a new transport channel.
     */
    H323Transport(H323EndPoint & endpoint,
                  PSSLContext * context = NULL,   ///< Context for SSL channel
                  PBoolean autoDeleteContext = false  ///< Flag for context to be automatically deleted.
    );
    ~H323Transport();
  //@}
#else
class H323Transport : public PIndirectChannel
{
  PCLASSINFO(H323Transport, PIndirectChannel);

  public:
  /**@name Construction */
  //@{
    /**Create a new transport channel.
     */
    H323Transport(H323EndPoint & endpoint);
    ~H323Transport();
  //@}
#endif
  /**@name Overrides from PObject */
  //@{
    virtual void PrintOn(
      ostream & strm
    ) const;
  //@}

  /**@name Operations */
  //@{
    /**Get the transport address of the local endpoint.
      */
    virtual H323TransportAddress GetLocalAddress() const = 0;

    /**Get the transport address of the remote endpoint.
      */
    virtual H323TransportAddress GetRemoteAddress() const = 0;

    /**Set remote address to connect to.
       Note that this does not necessarily initiate a transport level
       connection, but only indicates where to connect to. The actual
       connection is made by the Connect() function.
      */
    virtual PBoolean SetRemoteAddress(
      const H323TransportAddress & address
    ) = 0;

    /**Connect to the remote address.
      */
    virtual PBoolean Connect() = 0;

    /**Connect to the specified address.
      */
    PBoolean ConnectTo(
      const H323TransportAddress & address
    ) { return SetRemoteAddress(address) && Connect(); }


    /**Low level read from the channel.
     */
    virtual PBoolean Read(
      void * buf,   ///< Pointer to a block of memory to receive the read bytes.
      PINDEX len    ///< Maximum number of bytes to read into the buffer.
    );

    /**Low level write to the channel. 
     */
    virtual PBoolean Write(
      const void * buf, ///< Pointer to a block of memory to write.
      PINDEX len        ///< Number of bytes to write.
    );

    /**This callback is executed when the OnOpen() function is called with
       open channels. It may be used by descendent channels to do any
       handshaking required by the protocol that channel embodies.

       The default behaviour is to simply return TRUE.

       @return
       Returns TRUE if the protocol handshaking is successful.
     */
    virtual PBoolean OnSocketOpen();

    /**Close the channel.
      */
    virtual PBoolean Close();

    /**Check that the transport address PDU is compatible with transport.
      */
    virtual PBoolean IsCompatibleTransport(
      const H225_TransportAddress & pdu
    ) const;

    /**Set up a transport address PDU for RAS channel.
      */
    virtual void SetUpTransportPDU(
      H225_TransportAddress & pdu,
      PBoolean localTsap,
	  H323Connection * connection = NULL
    ) const;

    enum {
      UseLocalTSAP = 0x10001,
      UseRemoteTSAP
    };

    /**Set up a transport address PDU for logical channel.
       If tsap is UseLocalTSAP or UseRemoteTSAP then the local or remote port
       of the transport is used, otherwise the explicit port number is used.
      */
    virtual void SetUpTransportPDU(
      H245_TransportAddress & pdu,
      unsigned tsap
    ) const;

    /// Promiscious modes for transport
    enum PromisciousModes {
      AcceptFromRemoteOnly,
      AcceptFromAnyAutoSet,
      AcceptFromAny,
      AcceptFromLastReceivedOnly,
      NumPromisciousModes
    };

    /**Set read to promiscuous mode.
       Normally only reads from the specifed remote address are accepted. This
       flag allows packets to be accepted from any remote, provided the
       underlying protocol can do so. For example TCP will do nothing.

       The Read() call may optionally set the remote address automatically to
       whatever the sender host of the last received message was.

       Default behaviour does nothing.
      */
    virtual void SetPromiscuous(
      PromisciousModes promiscuous
    );

    /**Get the transport address of the last received PDU.

       Default behaviour returns GetRemoteAddress().
      */
    virtual H323TransportAddress GetLastReceivedAddress() const;

    /**Read a protocol data unit from the transport.
       This will read using the transports mechanism for PDU boundaries, for
       example UDP is a single Read() call, while for TCP there is a TPKT
       header that indicates the size of the PDU.
      */
    virtual PBoolean ReadPDU(
      PBYTEArray & pdu   ///<  PDU read from transport
    ) = 0;

    /**Extract a protocol data unit from the transport
       This is used by the aggregator to deblock the incoming data stream
       into valid PDUs.
      */
    virtual PBoolean ExtractPDU(
      const PBYTEArray & pdu, 
      PINDEX & len
    ) = 0;

    /**Write a protocol data unit from the transport.
       This will write using the transports mechanism for PDU boundaries, for
       example UDP is a single Write() call, while for TCP there is a TPKT
       header that indicates the size of the PDU.
      */
    virtual PBoolean WritePDU(
      const PBYTEArray & pdu  ///<  PDU to write
    ) = 0;

    /**Write a protocol data unit from the transport.
       This will write using the transports mechanism for PDU boundaries, for
       example UDP is a single Write() call, while for TCP there is a TPKT
       header that indicates the size of the PDU.
      */
    virtual PBoolean WriteSignalPDU(
      const H323SignalPDU & /*pdu*/  /// PDU to write
    ) { return false; }
  //@}

  /**@name Signalling Channel */
  //@{
	/** Handle the PDU Reading for the first connection object
	  */
    PBoolean HandleSignallingSocket(H323SignalPDU & pdu);

    /**Wait for first PDU and find/create connection object.
       If returns FALSE, then the transport is deleted by the calling thread.
      */
    PBoolean HandleFirstSignallingChannelPDU(PThread * thread);
  //@}

  /**@name Control Channel */
  //@{
    /**Begin the opening of a control channel.
       This sets up the channel so that the remote endpoint can connect back
       to this endpoint. This would be called on the signalling channel
       instance of a H323Transport.
      */
    virtual H323Transport * CreateControlChannel(
      H323Connection & connection
    );

    /**Finish the opening of a control channel.
       This waits for the connect backfrom the remote endpoint, completing the
       control channel open sequence.
      */
    virtual PBoolean AcceptControlChannel(
      H323Connection & connection
    );

    /**Connect the control channel.
      */
    virtual void StartControlChannel(
      H323Connection & connection
    );
  //@}

  /**@name RAS Channel */
  //@{
    /**Discover a Gatekeeper on the network.
       This locates a gatekeeper on the network and associates this transport
       object with packet exchange with that gatekeeper.
      */
    virtual PBoolean DiscoverGatekeeper(
      H323Gatekeeper & gk,                  ///<  Gatekeeper to set on discovery.
      H323RasPDU & pdu,                     ///<  GatekeeperRequest PDU
      const H323TransportAddress & address  ///<  Address of gatekeeper (if present)
    );

   /**Whether the RAS is tunnelled AKA H.460.17
      */
    virtual PBoolean IsRASTunnelled()  { return false; }

   /**Initialise Security
      */
    virtual PBoolean InitialiseSecurity(const H323TransportSecurity * /*security*/) { return false; }

    /**Finalise Transport Security.
      */
    virtual PBoolean FinaliseSecurity(PSocket * /*socket*/) { return false; }

   /**Whether the Transport is secured
      */
    PBoolean IsTransportSecure();

    /**Do SSL Connect handshake
      */
    virtual PBoolean SecureConnect() { return false; }

    /**Do SSL Accept handshake
      */
    virtual PBoolean SecureAccept() { return false; }
  //@}

  /**@name Member variable access */
  //@{
    /**Get the associated endpoint to this transport.
      */
    H323EndPoint & GetEndPoint() const { return endpoint; }

    /**Attach a thread to the transport.
      */
    void AttachThread(
      PThread * thread
    );

    /**Wait for associated thread to terminate.
      */
    virtual void CleanUpOnTermination();

    /**Get Error Code
       Default calls PChannel::GetErrorCode(ErrorGroup group)
      */
    virtual PChannel::Errors GetErrorCode(ErrorGroup group = NumErrorGroups) const;
  //@}

  protected:
    /**This callback is executed when the Open() function is called with
       open channels. It may be used by descendent channels to do any
       handshaking required by the protocol that channel embodies.

       The default behaviour is to simply return true.

       @return
       Returns true if the protocol handshaking is successful.
     */
    virtual PBoolean OnOpen();

    H323EndPoint & endpoint;    /// Endpoint that owns the listener.
    PThread      * thread;      /// Thread handling the transport
    PBoolean canGetInterface;

#ifdef H323_TLS
    PBoolean    m_secured;     /// Whether the channel is secure.
#endif
};



///////////////////////////////////////////////////////////////////////////////
// Transport classes for IP

/**This class represents a particular H323 transport using IP.
   It is used by the TCP and UDP transports.
 */
class H323TransportIP : public H323Transport
{
  PCLASSINFO(H323TransportIP, H323Transport);

  public:
    /**Create a new transport channel.
     */

    H323TransportIP(
      H323EndPoint & endpoint,    ///<  H323 End Point object
      PIPSocket::Address binding, ///<  Local interface to use
      WORD remPort                ///<  Remote port to use
#ifdef H323_TLS
      ,PSSLContext * context = NULL,   ///< Context for SSL channel
      PBoolean autoDeleteContext = false  ///< Flag for context to be automatically deleted.
#endif
    );

    /**Get the transport dependent name of the local endpoint.
      */
    virtual H323TransportAddress GetLocalAddress() const;

    /**Get the transport dependent name of the remote endpoint.
      */
    virtual H323TransportAddress GetRemoteAddress() const;

    /**Check that the transport address PDU is compatible with transport.
      */
    virtual PBoolean IsCompatibleTransport(
      const H225_TransportAddress & pdu
    ) const;

    /**Set up a transport address PDU for RAS channel.
      */
    virtual void SetUpTransportPDU(
      H225_TransportAddress & pdu,
      PBoolean localTsap,
	  H323Connection * connection = NULL
    ) const;

    /**Set up a transport address PDU for logical channel.
      */
    virtual void SetUpTransportPDU(
      H245_TransportAddress & pdu,
      unsigned tsap
    ) const;


  protected:
    PIPSocket::Address localAddress;  // Address of the local interface
    WORD               localPort;
    PIPSocket::Address remoteAddress; // Address of the remote host
    WORD               remotePort;
};


///////////////////////////////////////////////////////////////////////////////
// Transport classes for TCP/IP

/**This class manages H323 connections using TCP/IP transport.
 */
class H323ListenerTCP : public H323Listener
{
  PCLASSINFO(H323ListenerTCP, H323Listener);

  public:
    /**Create a new listener for the TCP/IP protocol.
     */
    H323ListenerTCP(
      H323EndPoint & endpoint,    ///<  Endpoint instance for channel
      PIPSocket::Address binding, ///<  Local interface to listen on
      WORD port,                  ///<  TCP port to listen for connections
      PBoolean exclusive = FALSE,    ///<  Fail if listener port in use
      H323TransportSecurity::Method security = H323TransportSecurity::e_unsecure ///< TransportSecurity
    );

    /** Destroy the listener thread.
      */
    ~H323ListenerTCP();
    
  // Overrides from H323Listener
    /** Open the listener.
      */
    virtual PBoolean Open();

    /**Stop the listener thread and no longer accept incoming connections.
     */
    virtual PBoolean Close();

    /**Accept a new incoming transport.
      */
    virtual H323Transport * Accept(
      const PTimeInterval & timeout  ///<  Time to wait for incoming connection
    );

    /**Get the local transport address on which this listener may be accessed.
      */
    virtual H323TransportAddress GetTransportAddress() const;

    /**Set the local transport address on which this listener may be accessed.
        Default does nothing...
      */
    virtual void SetTransportAddress(const H323TransportAddress & address);

    /**Create Transport.
      */
    virtual H323Transport * CreateTransport(const PIPSocket::Address & address);

    /**Set up a transport address PDU for bidirectional logical channels.
      */
    virtual PBoolean SetUpTransportPDU(
      H245_TransportAddress & pdu,        ///<  Transport addresses listening on
      const H323Transport & associatedTransport ///<  Associated transport for precendence and translation
    );

    WORD GetListenerPort() const { return listener.GetPort(); }

  protected:
    /**Handle incoming H.323 connections and dispatch them in new threads
       based on the H323Transport class. This is defined in the descendent
       class that knows what the low level transport is, eg H323ListenerIP
       for the TCP/IP protocol.

       Note this function does not return until the Close() function is called
       or there is some other error.
     */
    virtual void Main();


    PTCPSocket listener;
    PIPSocket::Address localAddress;
    PBoolean exclusiveListener;
};

//////////////////////////////////////////////////////////////////////////////////
// Transport classes for TLS

#ifdef H323_TLS

/**This class manages H323 connections using TLS transport.
 */
class H323ListenerTLS : public H323ListenerTCP
{
  PCLASSINFO(H323ListenerTLS, H323ListenerTCP);

  public:
    /**Create a new listener for the TCP/IP protocol.
     */
    H323ListenerTLS(
      H323EndPoint & endpoint,    ///<  Endpoint instance for channel
      PIPSocket::Address binding, ///<  Local interface to listen on
      WORD port                   ///<  TCP port to listen for connections
    );

    /** Destroy the listener thread.
      */
    ~H323ListenerTLS();

    /**Create Transport.
      */
    virtual H323Transport * CreateTransport(const PIPSocket::Address & address);

    /**Set up a transport address PDU for bidirectional logical channels. return false (Not used)
      */
    virtual PBoolean SetUpTransportPDU(
      H245_TransportAddress & pdu,         ///<  Transport addresses listening on
      const H323Transport & associatedTransport ///<  Associated transport for precendence and translation
    );

    /**Set the local transport address on which this listener may be accessed.
        Default sets the local address
      */
    virtual void SetTransportAddress(const H323TransportAddress & address);
};

#endif // H323_TLS

/////////////////////////////////////////////////////////////////////////////////

/**This class represents a particular H323 transport using TCP/IP.
 */
class H323TransportTCP : public H323TransportIP
{
  PCLASSINFO(H323TransportTCP, H323TransportIP);

  public:
    /**Create a new transport channel.
     */
    H323TransportTCP(
      H323EndPoint & endpoint,    ///<  H323 End Point object
      PIPSocket::Address binding = PIPSocket::GetDefaultIpAny(), ///<  Local interface to use
      PBoolean listen = FALSE         ///<  Flag for need to wait for remote to connect
#ifdef H323_TLS
      ,PSSLContext * context = NULL,   ///< Context for SSL channel
      PBoolean autoDeleteContext = false  ///< Flag for context to be automatically deleted.
#endif
    );

    /**Destroy transport channel.
     */
    ~H323TransportTCP();

    /**Set default remote address to connect to.
       Note that this does not necessarily initiate a transport level
       connection, but only indicates where to connect to. The actual
       connection is made by the Connect() function.
      */
    virtual PBoolean SetRemoteAddress(
      const H323TransportAddress & address
    );

    /**Connect to the remote party.
      */
    virtual PBoolean Connect();

    /**This callback is executed when the OnOpen() function is called with
       open channels. It may be used by descendent channels to do any
       handshaking required by the protocol that channel embodies.

       The default behaviour is to simply return TRUE.

       @return
       Returns TRUE if the protocol handshaking is successful.
     */
    virtual PBoolean OnSocketOpen();

    /**Close the channel.
      */
    virtual PBoolean Close();

    /**Read a protocol data unit from the transport.
       This will read using the transports mechanism for PDU boundaries, for
       example UDP is a single Read() call, while for TCP there is a TPKT
       header that indicates the size of the PDU.
      */
    PBoolean ReadPDU(
      PBYTEArray & pdu   ///<  PDU read from transport
    );

    /**Extract a protocol data unit from the transport
      */
    PBoolean ExtractPDU(
      const PBYTEArray & pdu, 
      PINDEX & len
    );
 
    /**Write a protocol data unit from the transport.
       This will write using the transports mechanism for PDU boundaries, for
       example UDP is a single Write() call, while for TCP there is a TPKT
       header that indicates the size of the PDU.
      */
    PBoolean WritePDU(
      const PBYTEArray & pdu  ///<  PDU to write
    );

    /**Begin the opening of a control channel.
       This sets up the channel so that the remote endpoint can connect back
       to this endpoint.
      */
    virtual H323Transport * CreateControlChannel(
      H323Connection & connection
    );

    /**Finish the opening of a control channel.
       This waits for the connect backfrom the remote endpoint, completing the
       control channel open sequence.
      */
    virtual PBoolean AcceptControlChannel(
      H323Connection & connection
    );

    /**Indicate we are waiting from remote to connect back to us.
      */
    virtual PBoolean IsListening() const;

    /**Initialise Transport Security.
      */
    virtual PBoolean InitialiseSecurity(const H323TransportSecurity * security);

    /**Finalise Transport Security.
      */
    virtual PBoolean FinaliseSecurity(PSocket * socket);

    /**Do SSL Connect handshake
      */
    virtual PBoolean SecureConnect();

    /**Do SSL Accept handshake
      */
    virtual PBoolean SecureAccept();

  protected:
    /**This callback is executed when the Open() function is called with
       open channels. It may be used by descendent channels to do any
       handshaking required by the protocol that channel embodies.

       The default behaviour is to simply return true.

       @return
       Returns true if the protocol handshaking is successful.
     */
    virtual PBoolean OnOpen();


    PTCPSocket * h245listener;
};


///////////////////////////////////////////////////////////////////////////////
// Transport classes for UDP/IP

/**This class represents a particular H323 transport using UDP/IP.
 */
class H323TransportUDP : public H323TransportIP
{
  PCLASSINFO(H323TransportUDP, H323TransportIP);

  public:
    /**Create a new transport channel.
     */
    H323TransportUDP(
      H323EndPoint & endpoint,                  ///<  H323 End Point object
      PIPSocket::Address binding = PIPSocket::GetDefaultIpAny(),  ///<  Local interface to listen on
      WORD localPort = 0,                       ///<  Local port to listen on
      WORD remotePort = 0                       ///<  Remote port to connect on
    );
    ~H323TransportUDP();

    /**Set default remote address to connect to.
       Note that this does not necessarily initiate a transport level
       connection, but only indicates where to connect to. The actual
       connection is made by the Connect() function.
      */
    virtual PBoolean SetRemoteAddress(
      const H323TransportAddress & address
    );

    /**Connect to the remote party.
      */
    virtual PBoolean Connect();

    /**Set read to promiscuous mode.
       Normally only reads from the specifed remote address are accepted. This
       flag allows packets to be accepted from any remote, provided the
       underlying protocol can do so.

       The Read() call may optionally set the remote address automatically to
       whatever the sender host of the last received message was.

       Default behaviour sets the internal flag, so that Read() operates as
       described.
      */
    virtual void SetPromiscuous(
      PromisciousModes promiscuous
    );

    /**Get the transport address of the last received PDU.

       Default behaviour returns the lastReceivedAddress member variable.
      */
    virtual H323TransportAddress GetLastReceivedAddress() const;

    /**Read a protocol data unit from the transport.
       This will read using the transports mechanism for PDU boundaries, for
       example UDP is a single Read() call, while for TCP there is a TPKT
       header that indicates the size of the PDU.
      */
    virtual PBoolean ReadPDU(
      PBYTEArray & pdu   ///<  PDU read from transport
    );

    /**Extract a protocol data unit from the transport
      */
    PBoolean ExtractPDU(
      const PBYTEArray & pdu, 
      PINDEX & len
    );
 
    /**Write a protocol data unit from the transport.
       This will write using the transports mechanism for PDU boundaries, for
       example UDP is a single Write() call, while for TCP there is a TPKT
       header that indicates the size of the PDU.
      */
    virtual PBoolean WritePDU(
      const PBYTEArray & pdu  ///<  PDU to write
    );

    /**Discover a Gatekeeper on the local network.
       This locates a gatekeeper on the network and associates this transport
       object with packet exchange with that gatekeeper. This broadcasts a UDP
       packet on the local network to find the gatekeeper's IP address.
      */
    virtual PBoolean DiscoverGatekeeper(
      H323Gatekeeper & gk,                  ///<  Gatekeeper to set on discovery.
      H323RasPDU & pdu,                     ///<  GatekeeperRequest PDU
      const H323TransportAddress & address  ///<  Address of gatekeeper (if present)
    );

    /**Get the transport address of the local endpoint.
      */
    virtual H323TransportAddress GetLocalAddress() const;

  protected:
    PromisciousModes     promiscuousReads;
    H323TransportAddress lastReceivedAddress;
    PIPSocket::Address   lastReceivedInterface;
    WORD interfacePort;
};


#endif // __TRANSPORTS_H


/////////////////////////////////////////////////////////////////////////////
