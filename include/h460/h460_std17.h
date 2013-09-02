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

////////////////////////////////////////////////////////////////////////
 #ifdef H323_H46026
class H46026Tunnel;
#endif
class H46017Handler;
class H46017Transport  : public H323TransportTCP
{
  PCLASSINFO(H46017Transport, H323TransportTCP);

  public:

    enum PDUType {
        e_raw
    };

    /**Create a new transport channel.
     */
    H46017Transport(
      H323EndPoint & endpoint,        /// H323 End Point object
      PIPSocket::Address binding,     /// Bind Interface
      H46017Handler * feat              /// Feature
#ifdef H323_TLS
      ,PSSLContext * context = NULL,   ///< Context for SSL channel
      PBoolean autoDeleteContext = false  ///< Flag for context to be automatically deleted.
#endif
    );

    ~H46017Transport();

    /**Handle the H46017 Socket
      */
    PBoolean HandleH46017Socket();


    /**Handle incoming H46017 PDU
      */
    PBoolean HandleH46017PDU(const Q931 & q931);
    PBoolean HandleH46017PDU(H323SignalPDU & pdu);

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

    void ConnectionLost(PBoolean established);

    PBoolean IsConnectionLost() const;

#ifdef H323_H46026
    void SetTunnel(H46026Tunnel * mgr);
#endif

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

    virtual void CleanUpOnTermination();

  protected:

    /**Handle the H46017 SetupPDU
      */
    H323Connection * HandleH46017SetupPDU(H323SignalPDU & pdu);

    /**Handle the H46017 Tunnelled RAS
      */
    PBoolean HandleH46017SignalPDU(H323SignalPDU & pdu);

    /**Handle the H46017 Tunnelled RAS
      */
    PBoolean HandleH46017RAS(const H323SignalPDU & pdu);

    /**Handle the H46017 Signalling on Signal Process Thread.
      */
    PBoolean HandleH46017SignallingPDU(unsigned crv, H323SignalPDU & pdu);

    PBoolean WriteTunnel(H323SignalPDU & msg);

     PMutex connectionsMutex;
     PMutex WriteMutex;
     PMutex shutdownMutex;

     PTimeInterval ReadTimeOut;

     H46017Handler * Feature;

     PBoolean   remoteShutDown;
     PBoolean    closeTransport;

     PSyncPoint  msgRecd;
     PMutex signalMutex;
     std::queue<H323SignalPDU>  recdpdu;
     PThread * m_signalProcess;
     PDECLARE_NOTIFIER(PThread, H46017Transport, SignalProcess);

 #ifdef H323_H46026
     PBoolean   m_h46026tunnel;
     H46026Tunnel * m_socketMgr;

     PThread * m_socketWrite;
     PDECLARE_NOTIFIER(PThread, H46017Transport, SocketWrite);
#endif

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

    H46017Transport * GetTransport();
    void AttachRasTransport(H46017RasTransport * _ras);
    H46017RasTransport * GetRasTransport();

    void TransportClosed();

    PBoolean RegisterGatekeeper();

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
    H46017Transport * curtransport;
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

    virtual H323TransportAddress GetRemoteAddress() const;

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

    virtual void CleanUpOnTermination();

  protected:
    H46017Handler * m_handler;

  private:
    PSyncPoint  msgRecd;
    PBYTEArray  recdpdu;

    PBoolean    shutdown;

};


#endif



