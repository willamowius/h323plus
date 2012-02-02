/*
 * upnpcp.h
 *
 * UPnP NAT Traversal class.
 *
 * h323plus library
 *
 * Copyright (c) 2009 ISVO (Asia) Pte. Ltd.
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
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 *
 */

#ifndef H_UPNP
#define H_UPNP

#include <ptclib/pnat.h>

#if _MSC_VER
#pragma once
#endif

class H323EndPoint;
class UPnPThread;
class PNatMethod_UPnP : public PNatMethod
{
    PCLASSINFO(PNatMethod_UPnP,PNatMethod);

public:

  /**@name Construction */
  //@{
    /** Default Contructor
    */
    PNatMethod_UPnP();

    /** Deconstructor
    */
    ~PNatMethod_UPnP();
  //@}

  /**@name General Functions */
  //@{
   void AttachEndPoint(H323EndPoint * _ep);

   virtual PBoolean GetExternalAddress(
      PIPSocket::Address & externalAddress, /// External address of router
      const PTimeInterval & maxAge = 1000   /// Maximum age for caching
      );

 /**  CreateSocketPair
        Create the UDP Socket pair (not used)
  */
   virtual PBoolean CreateSocketPair(
       PUDPSocket *&,PUDPSocket *&,
       const PIPSocket::Address &) { return false; }

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
   virtual bool IsAvailable(const PIPSocket::Address & ip);

   void SetAvailable();

   void SetAvailable(const PString & devName);

   virtual void Activate(bool act);

   PBoolean OpenSocket(PUDPSocket & socket, PortInfo & portInfo, const PIPSocket::Address & binding) const;


   static PStringList GetNatMethodName() {  return PStringArray("UPnP"); };
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
      );
  //@}

    void SetExtIPAddress(const PString & newAddr);

    PBoolean OnUPnPAvailable(const PString & devName);

    PBoolean CreateUPnPMap(bool pair, const PString & protocol, const PIPSocket::Address & localIP, 
                           const WORD & locPort, PIPSocket::Address & extIP , WORD & extPort);

    void RemoveUPnPMap(WORD port, PBoolean udp = true);

    H323EndPoint * GetEndPoint();

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
#endif

protected:

    void SetConnectionSockets(PUDPSocket * data,  PUDPSocket * control,  
                              H323Connection::SessionInformation * info );

private:
    H323EndPoint*                            ep;
    UPnPThread*                                m_pUPnP;

    PIPSocket::Address                        m_pExtIP;
    
    PBoolean                                m_pShutdown;
    PBoolean                                available;
    PBoolean                                active;
};


#ifndef _WIN32_WCE
    #if PTLIB_VER > 260
       PPLUGIN_STATIC_LOAD(UPnP,PNatMethod);
    #else
       PWLIB_STATIC_LOAD_PLUGIN(UPnP,PNatMethod);
    #endif
#endif


class UPnPUDPSocket : public PUDPSocket
{
  PCLASSINFO(UPnPUDPSocket, PUDPSocket);
  public:
  /**@name Construction/Deconstructor */
  //@{
    /** create a UDP Socket Fully Nat Supported
        ready for H323plus to Call.
    */
    UPnPUDPSocket(PNatMethod_UPnP * nat);

    /** Deconstructor to reallocate Socket and remove any exiting
        allocated NAT ports, 
    */
    ~UPnPUDPSocket();

    virtual PBoolean Close();

    /** Set Masq Address
      */
    void SetMasqAddress(const PIPSocket::Address & ip, WORD port);

    PBoolean GetLocalAddress(Address & addr);

    PBoolean GetLocalAddress(Address & addr, WORD & port);

   //@}

  protected:
    PNatMethod_UPnP*        natMethod;

    PIPSocket::Address        extIP;
    WORD                    extPort;

};

#endif  // H_UPNP

