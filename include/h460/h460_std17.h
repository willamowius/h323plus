/*
 * h460_std17.h
 *
 * H460.17 NAT Traversal class.
 *
 * h323plus library
 *
 * Copyright (c) 2008-11 ISVO (Asia) Pte. Ltd.
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
 *
 */

#ifndef H_H460_FeatureStd17
#define H_H460_FeatureStd17

#pragma once

#include <ptlib/plugin.h>
#include <vector>
#include <queue>

class H323EndPoint;
class H323Connection;
class H46017Handler;
class H460_FeatureStd17 : public H460_FeatureStd 
{
    PCLASSINFO(H460_FeatureStd17,H460_FeatureStd);

public:

    H460_FeatureStd17();
    virtual ~H460_FeatureStd17();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);
    virtual void AttachConnection(H323Connection * _con);

    static PStringArray GetFeatureName() { return PStringArray("Std17"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("NatTraversal-H.460.17"); };
    static int GetPurpose();
	static PStringArray GetIdentifier() { return PStringArray("17"); };

	virtual PBoolean CommonFeature() { return isEnabled; }

    static PBoolean IsEnabled()  { return isEnabled; }

    //////////////////////
    // Public Function  call..
    virtual PBoolean Initialise(const PString & remoteAddr = PString(), PBoolean srv = true);

    H46017Handler * GetHandler() { return m_handler; }

protected:
    PBoolean InitialiseTunnel(const H323TransportAddress & remoteAddr);


private:
    H323EndPoint * EP;
    H323Connection * CON;

    H46017Handler * m_handler;
    static PBoolean isEnabled;

};

// Need to declare for Factory Loader
#ifndef _WIN32_WCE
  PPLUGIN_STATIC_LOAD(Std17, H460_Feature);
#endif

///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////

class h225Order {
public:
     enum priority {
        Priority_Critical=1,    // Audio RTP
        Priority_Discretion,    // Video RTP
        Priority_High,          // Signaling   
        Priority_low            // RTCP
     };

     struct MessageHeader {
        int sessionId;
        int priority;
        PInt64 packTime;
     };

     int operator() ( const std::pair<H323SignalPDU, MessageHeader>& p1,
                      const std::pair<H323SignalPDU, MessageHeader>& p2 ) {
           return ((p1.second.priority >= p2.second.priority) && 
                     (p1.second.packTime > p2.second.packTime));
     }
};

typedef std::priority_queue< std::pair<H323SignalPDU, h225Order::MessageHeader >, 
        std::vector< std::pair<H323SignalPDU, h225Order::MessageHeader> >, h225Order > h225Queue;

////////////////////////////////////////////////////////////////////////

class H46017Handler;
class H46017Transport  : public H323TransportTCP
{
  PCLASSINFO(H46017Transport, H323TransportTCP);

  public:

	enum PDUType {
		e_raw,
	};

    /**Create a new transport channel.
     */
    H46017Transport(
      H323EndPoint & endpoint,        /// H323 End Point object
      PIPSocket::Address binding,     /// Bind Interface
	  H46017Handler * feat		      /// Feature
    );

	~H46017Transport();

	/**Handle the H46017 Signalling
	  */
	PBoolean HandleH46017SignallingChannelPDU(PThread * thread);

	/**Handle the H46017 Tunnelled RAS
	  */
    PBoolean HandleH46017RAS(const H323SignalPDU & pdu);

	/**Handle the H46017 Signalling
	  */
	PBoolean HandleH46017SignallingSocket(H323SignalPDU & pdu);

    /**Write a protocol data unit from the transport.
       This will write using the transports mechanism for PDU boundaries, for
       example UDP is a single Write() call, while for TCP there is a TPKT
       header that indicates the size of the PDU.
      */
    PBoolean WritePDU(
      const PBYTEArray & pdu  ///<  PDU to write
    );

    /**Write a protocol data unit from the transport.
       This will write using the transports mechanism for PDU boundaries, for
       example UDP is a single Write() call, while for TCP there is a TPKT
       header that indicates the size of the PDU.
      */
    virtual PBoolean WriteSignalPDU(
      const H323SignalPDU & pdu  /// PDU to write
    );

    PBoolean WriteRasPDU(
      const PBYTEArray & pdu
    );

    /**Read a protocol data unit from the transport.
       This will read using the transports mechanism for PDU boundaries, for
       example UDP is a single Read() call, while for TCP there is a TPKT
       header that indicates the size of the PDU.
      */
	virtual PBoolean ReadPDU(
         PBYTEArray & pdu  /// PDU to Read
	);

    PBoolean WriteTunnel(const H323SignalPDU & msg, const h225Order::MessageHeader & prior);

    PBoolean ReadTunnel();

    PBoolean HandleControlPDU(const H323ControlPDU & pdu);
#ifdef H323_H46026
    PBoolean PostFrame(unsigned sessionID,bool rtp,  const void * buf, PINDEX len);
#endif
	PBoolean isCall() { return isConnected; };

	void ConnectionLost(PBoolean established);

	PBoolean IsConnectionLost() const;


// Overrides
    /**Connect to the remote party.
      */
    virtual PBoolean Connect();

    /**Close the channel.(Don't do anything)
      */
    virtual PBoolean Close();

    virtual PBoolean IsListening() const;

	virtual PBoolean IsOpen () const;

	PBoolean CloseTransport() { return closeTransport; };

  protected:
    PBoolean ReadControl(const H323SignalPDU & msg);
#ifdef H323_H46026
    PBoolean ReadMedia(const H323SignalPDU & msg);
    void GenerateFastUpdatePicture(int session);
#endif
	 PMutex connectionsMutex;
	 PMutex WriteMutex;
	 PMutex shutdownMutex;

	 PTimeInterval ReadTimeOut;

     H323Connection * con;

     PMutex  queueMutex;
     PThread * m_socketWrite;
     PDECLARE_NOTIFIER(PThread, H46017Transport, SocketWrite);
     h225Queue signalQueue;

	 H46017Handler * Feature;

	 PBoolean   isConnected;
	 PBoolean   remoteShutDown;
	 PBoolean	closeTransport;

     PString    callToken;

};

//////////////////////////////////////////////////////////////////////

class H46017RasTransport;
class H46017Handler : public PObject  
{

	PCLASSINFO(H46017Handler, PObject);

public:
	H46017Handler(H323EndPoint & _ep, 
		const H323TransportAddress & _remoteAddress
		);

	~H46017Handler();

    H323EndPoint * GetEndPoint();

	PBoolean CreateNewTransport();

	PBoolean ReRegister(const PString & newid);

	PBoolean IsOpen() { return openTransport; }

    PBoolean IsConnectionLost() const { return connectionlost; }
    void SetConnectionLost(PBoolean newVal) { connectionlost = newVal; }

    H323TransportAddress GetTunnelBindAddress() const;

    void AttachRasTransport(H46017RasTransport * _ras);
    H46017RasTransport * GetRasTransport();

    PBoolean RegisterGatekeeper();

	static H46017Transport * curtransport;

#ifdef H323_H46026
    /**Set Flag to say media tunneling
      */
    void SetH46026Tunnel(PBoolean tunnel);

    /**Is Media Tunneling
      */
    PBoolean IsH46026Tunnel();
#endif

private:
	H323EndPoint & ep;

    H46017RasTransport * ras;

	H323TransportAddress remoteAddress;
    PIPSocket::Address localBindAddress;

	PBoolean connectionlost;
    PBoolean openTransport;
    PBoolean callEnded;

#ifdef H323_H46026
    PBoolean   m_h46026tunnel;
#endif

};

///////////////////////////////////////////////////////////////////////////

#ifdef H323_H46026

class H460_FeatureStd26 : public H460_FeatureStd 
{
    PCLASSINFO(H460_FeatureStd26,H460_FeatureStd);

public:

    H460_FeatureStd26();
    virtual ~H460_FeatureStd26();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);
    virtual void AttachConnection(H323Connection * _con);

    static PStringArray GetFeatureName() { return PStringArray("Std26"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("TCPTunneling-H.460.26"); };
    static int GetPurpose();
	static PStringArray GetIdentifier() { return PStringArray("26"); };

	virtual PBoolean FeatureAdvertised(int mtype);
	virtual PBoolean CommonFeature() { return isEnabled; }

	// Messages
    virtual PBoolean OnSendAdmissionRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendSetup_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendCallProceeding_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveCallProceeding_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendAlerting_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveAlerting_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendCallConnect_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveCallConnect_UUIE(const H225_FeatureDescriptor & pdu);

    void AttachHandler(H46017Handler * m_handler);

private:
    H323EndPoint * EP;
    H323Connection * CON;

	H46017Handler * handler;			///< handler
    PBoolean isEnabled;
    static PBoolean isSupported;
};

// Need to declare for Factory Loader
#ifndef _WIN32_WCE
  PPLUGIN_STATIC_LOAD(Std26, H460_Feature);
#endif

//////////////////////////////////////////////////////////////////////////////

class PNatMethod_H46026  : public PNatMethod
{
	PCLASSINFO(PNatMethod_H46026,PNatMethod);

public:

  /**@name Construction */
  //@{
	/** Default Contructor
	*/
	PNatMethod_H46026();

	/** Deconstructor
	*/
	~PNatMethod_H46026();
  //@}

  /**@name General Functions */
  //@{
   void AttachEndPoint(H323EndPoint * ep);

   void AttachHandler(H46017Handler * m_handler);

   virtual PBoolean GetExternalAddress(
      PIPSocket::Address & externalAddress, /// External address of router
      const PTimeInterval & maxAge = 1000   /// Maximum age for caching
	  );

	/**  CreateSocketPair
		Create the UDP Socket pair (does nothing)
	*/
   virtual PBoolean CreateSocketPair(
	  PUDPSocket * & /*socket1*/,			///< Created DataSocket
      PUDPSocket * & /*socket2*/,			///< Created ControlSocket
      const PIPSocket::Address & /*binding*/
      ) {  return false; }

  /**  CreateSocketPair
		Create the UDP Socket pair
  */
    virtual PBoolean CreateSocketPair(
      PUDPSocket * & socket1,
      PUDPSocket * & socket2,
	  const PIPSocket::Address & binding,  
	  void * userData
    );

  /** SetStatusHeader
      Build the RTP wrapper message
    */
     void SetInformationHeader(PUDPSocket & data,                        ///< Data Socket
                          PUDPSocket & control,                          ///< Control Socket
					      H323Connection::SessionInformation * info      ///< Socket Information
                          );

  /**  isAvailable.
		Returns whether the Nat Method is ready and available in
		assisting in NAT Traversal. The principal is function is
		to allow the EP to detect various methods and if a method
		is detected then this method is available for NAT traversal
		The Order of adding to the PNstStrategy determines which method
		is used
  */
   virtual bool IsAvailable(const PIPSocket::Address&) { return (handler && handler->IsH46026Tunnel()); };

   void SetAvailable() {};

    /** Activate
		Active/DeActivate the method on a call by call basis
	 */
   virtual void Activate(bool act)  { active = act; }

#if PTLIB_VER > 2120
   static PString GetNatMethodName() { return "H46026"; }
   virtual PString GetName() const
            { return GetNatMethodName(); }
#else
   static PStringArray GetNatMethodName() {  return PStringArray("H46026"); }
   virtual PString GetName() const
            { return GetNatMethodName()[0]; }
#endif

   // All these are virtual and never used. 
    virtual bool GetServerAddress(
      PIPSocket::Address & address,   ///< Address of server
      WORD & port                     ///< Port server is using.
	  ) const { return false; }

    virtual bool GetInterfaceAddress(
      PIPSocket::Address & internalAddress
	  ) const { return false; }

    virtual PBoolean CreateSocket(
      PUDPSocket * & socket,
      const PIPSocket::Address & binding = PIPSocket::GetDefaultIpAny(),
      WORD localPort = 0
	  ) { return false; }

    virtual RTPSupportTypes GetRTPSupport(
      PBoolean force = PFalse    ///< Force a new check
	  )  { return RTPSupported; }
  //@}

#if PTLIB_VER >= 2110
    virtual PString GetServer() const { return PString(); }
    virtual bool GetServerAddress(PIPSocketAddressAndPort & ) const { return false; }
    virtual NatTypes GetNatType(bool) { return UnknownNat; }
    virtual NatTypes GetNatType(const PTimeInterval &) { return UnknownNat; }
    virtual bool SetServer(const PString &) { return false; }
    virtual bool Open(const PIPSocket::Address &) { return false; }
    virtual bool CreateSocket(BYTE component,PUDPSocket * & socket,
            const PIPSocket::Address & binding = PIPSocket::GetDefaultIpAny(),WORD localPort = 0)  { return false; }
    virtual void SetCredentials(const PString &, const PString &, const PString &) {}
protected:
    virtual NatTypes InternalGetNatType(bool, const PTimeInterval &) { return UnknownNat; }
#endif


protected:
	PBoolean active;					///< Whether the method is active for call
	H46017Handler * handler;			///< handler

};


#ifndef _WIN32_WCE
PPLUGIN_STATIC_LOAD(H46026, PNatMethod);
#endif

typedef std::queue<PBYTEArray*> RTPQueue;

class H46026_UDPFrame;
class H46026UDPSocket : public PUDPSocket
{
  PCLASSINFO(H46026UDPSocket, PUDPSocket);
  public:
  /**@name Construction/Deconstructor */
  //@{
	/** create a UDP Socket Fully Nat Supported
		ready for H323plus to Call.
	*/
    H46026UDPSocket(H46017Handler & _handler, H323Connection::SessionInformation * info, bool _rtpSocket);

	/** Deconstructor to reallocate Socket and remove any exiting
		allocated NAT ports, 
	*/
	~H46026UDPSocket();

   //@}

	/**@name Functions */
	//@{
    /**Set Information header
        Set the RTP wrapper message
      */
    void SetInformationHeader(const H323SignalPDU & _informationMsg, const H323Transport * _transport);

  // Overrides from class PIPDatagramSocket.
    /** Read a datagram from a remote computer.
       
       @return
       true if any bytes were sucessfully read.
     */
    virtual PBoolean ReadFrom(
      void * buf,     // Data to be written as URGENT TCP data.
      PINDEX len,     // Number of bytes pointed to by <CODE>buf</CODE>.
      Address & addr, // Address from which the datagram was received.
      WORD & port     // Port from which the datagram was received.
    );

    /** Write a datagram to a remote computer.

       @return
       true if all the bytes were sucessfully written.
     */
    virtual PBoolean WriteTo(
      const void * buf,   // Data to be written as URGENT TCP data.
      PINDEX len,         // Number of bytes pointed to by <CODE>buf</CODE>.
      const Address & addr, // Address to which the datagram is sent.
      WORD port           // Port to which the datagram is sent.
    );

    PBoolean WriteBuffer(const void * buf, PINDEX len);
    PBoolean DoPseudoRead(int & selectStatus);
    PBoolean BuildTunnelMediaPacket(const void * buf, PINDEX len);

    void GetLocalAddress(H245_TransportAddress & add);
    PBoolean GetLocalAddress(Address & addr, WORD & port);

    void SetRemoteAddress(const H245_TransportAddress & add);

   //@}


private:
	H46017Transport * m_transport;
	unsigned m_Session;						///< Current Session ie 1-Audio 2-video
    bool rtpSocket;

    // tunnel data containers
    int m_frameCount;
    int m_payloadSize;
    H46026_UDPFrame * m_frame;
    H323SignalPDU msg;

    RTPQueue rtpQueue;
    PMutex writeMutex;
    PBoolean shutDown;
};
#endif // H323_H46026

//////////////////////////////////////////////////////////////////////

class H46017RasTransport : public H323TransportUDP
{
  PCLASSINFO(H46017RasTransport, H323TransportUDP);

  public:

    H46017RasTransport(
      H323EndPoint & endpoint,
      H46017Handler * handler
    );
    ~H46017RasTransport();

    virtual H323TransportAddress GetLocalAddress() const;

    virtual void SetUpTransportPDU(
      H225_TransportAddress & pdu,
      PBoolean localTsap,
      H323Connection * connection = NULL
    ) const;

    virtual PBoolean SetRemoteAddress(
      const H323TransportAddress & address
    );

    virtual PBoolean Connect();

    /**Close the channel.
      */
    virtual PBoolean Close();


    PBoolean ReceivedPDU(
      const PBYTEArray & pdu  ///<  PDU read from Tunnel
    );

    virtual PBoolean ReadPDU(
      PBYTEArray & pdu   ///<  PDU read input
    );
 
    virtual PBoolean WritePDU(
      const PBYTEArray & pdu  ///<  PDU to write to tunnel
    );

    virtual PBoolean DiscoverGatekeeper(
      H323Gatekeeper & gk,                  ///<  Gatekeeper to set on discovery.
      H323RasPDU & pdu,                     ///<  GatekeeperRequest PDU
      const H323TransportAddress & address  ///<  Address of gatekeeper (if present)
    );

    virtual PBoolean IsRASTunnelled();

    virtual PChannel::Errors GetErrorCode(ErrorGroup group = NumErrorGroups) const;

  protected:
    H46017Handler * m_handler;

  private:
    PSyncPoint  msgRecd;
    PBYTEArray  recdpdu;

    PBoolean    shutdown;

};


#endif



