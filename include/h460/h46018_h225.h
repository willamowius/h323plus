/*
 * h46018_h225.h
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
 * Revision 1.1  2009/06/28 00:11:03  shorne
 * Added H.460.18/19 Support
 *
 *
 *
 *
 */


#ifndef H46018_NAT
#define H46018_NAT

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "h323pdu.h"

class H46018SignalPDU  : public H323SignalPDU
{
  public:
	/**@name Construction */
	//@{
	/** Default Contructor
		This creates a H.225.0 SignalPDU to notify
		the server that the opened TCP socket is for
		the call with the callidentifier specified
	*/
	H46018SignalPDU(const OpalGloballyUniqueID & callIdentifier);

	//@}
};

class H323EndPoint;
class H46018Handler;
class H46018Transport  : public H323TransportTCP
{
	PCLASSINFO(H46018Transport, H323TransportTCP);

  public:

	enum PDUType {
		e_raw,
	};

	/**@name Construction */
  	//@{
	/**Create a new transport channel.
	*/
	H46018Transport(
		H323EndPoint & endpoint        /// H323 End Point object
	);

	~H46018Transport();
	//@}

	/**@name Functions */
	//@{
	/**Write a protocol data unit from the transport.
		This will write using the transports mechanism for PDU boundaries, for
		example UDP is a single Write() call, while for TCP there is a TPKT
		header that indicates the size of the PDU.
	*/
	virtual PBoolean WritePDU(
		const PBYTEArray & pdu  /// PDU to write
	);

	/**Read a protocol data unit from the transport.
		This will read using the transports mechanism for PDU boundaries, for
		example UDP is a single Read() call, while for TCP there is a TPKT
		header that indicates the size of the PDU.
	*/
	virtual PBoolean ReadPDU(
         PBYTEArray & pdu  /// PDU to Read
	);
	//@}

	/**@name NAT Functions */
	//@{
	/**HandleH46018SignallingChannelPDU
		Handle the H46018 Signalling channel
	  */
	PBoolean HandleH46018SignallingChannelPDU();

	/**HandleH46018SignallingSocket
		Handle the H46018 Signalling socket
	  */
	PBoolean HandleH46018SignallingSocket(H323SignalPDU & pdu);

	/**InitialPDU
		Send the initialising PDU for the call with the specified callidentifer
	  */
	PBoolean InitialPDU(const OpalGloballyUniqueID & callIdentifier);

	/**isCall
	   Do we have a call connected?
	  */
	PBoolean isCall() { return isConnected; };

	/**ConnectionLost
	   Set the connection as being lost
	  */
	void ConnectionLost(PBoolean established);

	/**IsConnectionLost
	   Is the connection lost?
	  */
	PBoolean IsConnectionLost();
	//@}

	// Overrides
	/**Connect to the remote party.
	*/
	virtual PBoolean Connect(const OpalGloballyUniqueID & callIdentifier);

	/**Close the channel.(Don't do anything)
	*/
	virtual PBoolean Close();

	virtual PBoolean IsListening() const;

	virtual PBoolean IsOpen () const;

	PBoolean CloseTransport() { return closeTransport; };

  protected:

	 PMutex connectionsMutex;
	 PMutex WriteMutex;
	 PMutex IntMutex;
	 PTimeInterval ReadTimeOut;
	 PSyncPoint ReadMutex;

	 H46018Handler * Feature;

	 PBoolean   isConnected;
	 PBoolean   remoteShutDown;
	 PBoolean	closeTransport;
	 
};



class H323EndPoint;
class PNatMethod_H46019;
class H46018Handler : public PObject  
{

	PCLASSINFO(H46018Handler, PObject);

  public:
	H46018Handler(H323EndPoint * ep);
	~H46018Handler();

	void Enable();
	PBoolean IsEnabled();

	H323EndPoint * GetEndPoint();

	PBoolean CreateH225Transport(const PASN_OctetString & information);
		
  protected:	
	H323EndPoint * EP;
	PNatMethod_H46019 * nat;
    PString lastCallIdentifer;

  private:
    H323TransportAddress m_address;
	OpalGloballyUniqueID m_callId;
	PThread * SocketCreateThread;
    PDECLARE_NOTIFIER(PThread, H46018Handler, SocketThread);
	PBoolean m_h46018inOperation;
};


class PNatMethod_H46019  : public PNatMethod
{
	PCLASSINFO(PNatMethod_H46019,PNatMethod);

  public:
	/**@name Construction */
	//@{
	/** Default Contructor
	*/
	PNatMethod_H46019();

	/** Deconstructor
	*/
	~PNatMethod_H46019();
	//@}

	/**@name General Functions */
	//@{
	/**  AttachEndpoint
		Attach endpoint reference
	*/
	void AttachHandler(H46018Handler * _handler);

	/**  GetExternalAddress
		Get the external address.
		This function is not used in H46019
	*/
	virtual PBoolean GetExternalAddress(
		PIPSocket::Address & externalAddress, ///< External address of router
		const PTimeInterval & maxAge = 1000   ///< Maximum age for caching
	  );

	/**  CreateSocketPair
		Create the UDP Socket pair
	*/
	virtual PBoolean CreateSocketPair(
		PUDPSocket * & socket1,			///< Created DataSocket
		PUDPSocket * & socket2,			///< Created ControlSocket
		const PIPSocket::Address & binding = PIPSocket::GetDefaultIpAny()
	);

	/**  isAvailable.
		Returns whether the Nat Method is ready and available in
		assisting in NAT Traversal. The principal is function is
		to allow the EP to detect various methods and if a method
		is detected then this method is available for NAT traversal
		The Order of adding to the PNstStrategy determines which method
		is used
	*/
	virtual bool IsAvailable(const PIPSocket::Address & address = PIPSocket::GetDefaultIpAny());

	/* Set Available
		This will enable the natMethod to be enabled when opening the
		sockets
	*/
	void SetAvailable() { available = TRUE; };

	/**  OpenSocket
		Create a single UDP Socket 
	*/
	PBoolean OpenSocket(PUDPSocket & socket, PortInfo & portInfo) const;

	/**  GetMethodName
		Get the NAT method name 
	*/
	static PStringList GetNatMethodName() {  return PStringArray("H46019"); };

    virtual PString GetName() const
            { return GetNatMethodName()[0]; }

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

	/**  SetSessionInfo
		This sents the H323Connection callToken callback to 
		send references to the opened UDP sockets
	*/
	void SetSessionInfo(unsigned sessionid,		///< Session ID 1 - Audio 2 - Video
			const PString & callToken	///< Connection token for the reference connection
			);
	//@}

  protected:

	/**@name General Functions */
	//@{
	/**  SetConnectionSockets
		This function sets the socket references in the 
		H323Connection to allow the implementer to add
		keep-alive info to the sockets
	*/
	void SetConnectionSockets(PUDPSocket * data,	///< Data socket
				PUDPSocket * control	///< control socket (not implemented)
				);
	
	PBoolean available;			///< Whether this NAT Method is available for call
	H46018Handler * handler;	///< handler
	unsigned curSession;		///< Current Session ie 1-Audio 2-video
	PString curToken;		    ///< Current Connection Token


};

#ifndef _WIN32_WCE
#if defined(PPLUGIN_STATIC_LOAD)
   PPLUGIN_STATIC_LOAD(H46019,PNatMethod);
#else
   PWLIB_STATIC_LOAD_PLUGIN(H46019, PNatMethod);
#endif
#endif

class H46019UDPSocket : public PUDPSocket
{
	PCLASSINFO(H46019UDPSocket, PUDPSocket);
  public:
	/**@name Construction/Deconstructor */
	//@{
	/** create a UDP Socket Fully Nat Supported
		ready for H323plus to Call.
	*/
	H46019UDPSocket();

	/** Deconstructor to reallocate Socket and remove any exiting
		allocated NAT ports, 
	*/
	~H46019UDPSocket();
	//@}

	PBoolean GetLocalAddress(Address & addr, WORD & port);

	/**@name Functions */
	//@{
	/** Activate keep-alive mechanism.
	*/
	void Activate(const H323TransportAddress & keepalive,	///< KeepAlive Address
			unsigned _payload,			///< RTP Payload type	
			unsigned _ttl				///< Time interval for keepalive.
			);
	//@}

  protected:
	void SendPing();

	PIPSocket::Address keepip;	///< KeepAlive Address
	WORD keepport;			///< KeepAlive Port
	unsigned keeppayload;		///< KeepAlive RTP payload
	WORD keepseqno;			///< KeepAlive sequence number
	PTime * keepStartTime;      	///< KeepAlive start time for TimeStamp.

	PDECLARE_NOTIFIER(PTimer, H46019UDPSocket, Ping);	///< Timer to notify to poll for External IP
	PTimer	Keep;						///< Polling Timer

};

#endif // H46018_NAT




