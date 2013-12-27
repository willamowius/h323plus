
/*
 * h460_std26.h
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

#ifndef H_H460_FeatureStd26
#define H_H460_FeatureStd26

#pragma once

#include <ptlib/plugin.h>
#include <map>
#include <queue>

#include <h460/h46026mgr.h>

/////////////////////////////////////////////////////////////////////////////////

class H46026UDPSocket;
class H46026PortPairs 
{
public:
    H46026PortPairs();
    H46026PortPairs(H46026UDPSocket* _rtp, H46026UDPSocket* _rtcp);

    H46026UDPSocket* rtp;
    H46026UDPSocket* rtcp;
};

/////////////////////////////////////////////////////////////////////////////////

typedef std::map<int, H46026PortPairs >  H46026SocketMap;
typedef std::map<unsigned, H46026SocketMap> H46026CallSocket;

class H46017Transport;
class H46026Tunnel : public H46026ChannelManager
{
public:
    H46026Tunnel();
    ~H46026Tunnel();

    void AttachTransport(H46017Transport * transport);

    void RegisterSocket(unsigned crv, int sessionId, H46026UDPSocket* rtp, H46026UDPSocket* rtcp);
    void UnRegisterSocket(unsigned crv, int sessionId);

    void SignalToSend(const Q931 & _q931);
    void FrameToSend(unsigned crv, int sessionId, bool rtp, const BYTE * buf, PINDEX len);

    // overrides
    virtual void SignalMsgIn(const Q931 & /*q931*/);
    virtual void RTPFrameIn(unsigned /*crv*/, PINDEX /*sessionId*/, PBoolean /*rtp*/, const PBYTEArray & /*data*/);

protected:
    H46026CallSocket        m_socketMap;
    PMutex                  m_socketMutex;

    H46017Transport *       m_transport;

};

/////////////////////////////////////////////////////////////////////////////////

class H46017Handler;
class H46017Transport;
class PNatMethod_H46026;
class H460_FeatureStd26 : public H460_FeatureStd 
{
    PCLASSINFO(H460_FeatureStd26,H460_FeatureStd);

public:

    H460_FeatureStd26();
    virtual ~H460_FeatureStd26();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);
    virtual void AttachConnection(H323Connection * _con);

    void AttachNatMethod(PNatMethod_H46026 * _method);

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

    void AttachH46017(H46017Handler * m_handler);

    H46026Tunnel * GetTunnel();

private:
    H323EndPoint * EP;
    H323Connection * CON;

    H46026Tunnel h46026mgr;             ///< Channel Manager
    H46017Handler * handler;            ///< handler
    PNatMethod_H46026 * method;
    PBoolean isEnabled;
    static PBoolean isSupported;
};

// Need to declare for Factory Loader
#ifndef _WIN32_WCE
  PPLUGIN_STATIC_LOAD(Std26, H460_Feature);
#endif

//////////////////////////////////////////////////////////////////////////////

class PNatMethod_H46026  : public H323NatMethod
{
    PCLASSINFO(PNatMethod_H46026,H323NatMethod);

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

   void AttachManager(H46026Tunnel * m_handler);


   virtual PBoolean GetExternalAddress(
      PIPSocket::Address & externalAddress, /// External address of router
      const PTimeInterval & maxAge = 1000   /// Maximum age for caching
      );

    /**  CreateSocketPair
        Create the UDP Socket pair (does nothing)
    */
   virtual PBoolean CreateSocketPair(
      PUDPSocket * & /*socket1*/,            ///< Created DataSocket
      PUDPSocket * & /*socket2*/,            ///< Created ControlSocket
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

  /**  isAvailable.
        Returns whether the Nat Method is ready and available in
        assisting in NAT Traversal. The principal is function is
        to allow the EP to detect various methods and if a method
        is detected then this method is available for NAT traversal
        The Order of adding to the PNstStrategy determines which method
        is used
  */
   virtual bool IsAvailable(const PIPSocket::Address&);

   void SetAvailable() {};

    /** Activate
        Active/DeActivate the method on a call by call basis
     */
   virtual void Activate(bool act)  { active = act; }

#if PTLIB_VER >= 2130
   virtual PCaselessString GetMethodName() const { return "H46026"; }
#elif PTLIB_VER > 2120
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
#if PTLIB_VER < 2130
    virtual NatTypes InternalGetNatType(bool, const PTimeInterval &) { return UnknownNat; }
#endif
    virtual PNATUDPSocket * InternalCreateSocket(Component component, PObject * context)  { return NULL; }
    virtual void InternalUpdate() {};
#endif


protected:
    PBoolean active;            ///< Whether the method is active for call
    H46026Tunnel * handler;        ///< Socket Handler

};


#ifndef _WIN32_WCE
PPLUGIN_STATIC_LOAD(H46026, PNatMethod);
#endif

typedef std::queue<PBYTEArray*> RTPQueue;

class H46026_UDPFrame;
class H46026UDPSocket : public H323UDPSocket
{
  PCLASSINFO(H46026UDPSocket, H323UDPSocket);
  public:
  /**@name Construction/Deconstructor */
  //@{
    /** create a UDP Socket Fully Nat Supported
        ready for H323plus to Call.
    */
    H46026UDPSocket(H46026Tunnel & _handler, H323Connection::SessionInformation * info, bool _rtpSocket);

    /** Deconstructor to reallocate Socket and remove any exiting
        allocated NAT ports, 
    */
    ~H46026UDPSocket();

   //@}

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

    PBoolean WriteBuffer(const PBYTEArray & buffer);
    PBoolean DoPseudoRead(int & selectStatus);

    void GetLocalAddress(H245_TransportAddress & add);
    PBoolean GetLocalAddress(Address & addr, WORD & port);

    void SetRemoteAddress(const H245_TransportAddress & add);

   //@}

private:
    H46026Tunnel &          m_transport;
    unsigned                m_crv;
    unsigned                m_session;      ///< Current Session ie 1-Audio 2-video
    bool                    m_rtpSocket;

    RTPQueue                m_rtpQueue;
    PMutex                  m_rtpMutex;
    bool                    m_shutdown;

    PAdaptiveDelay          m_selectBlock;
};

#endif // H_H460_FeatureStd26