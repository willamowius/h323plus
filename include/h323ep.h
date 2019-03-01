/*
 * h323ep.h
 *
 * H.323 protocol handler
 *
 * Open H323 Library
 *
 * Copyright (c) 1998-2001 Equivalence Pty. Ltd.
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

#ifndef __OPAL_H323EP_H
#define __OPAL_H323EP_H

#include "h323.h"
#include "h323con.h"

#ifdef P_USE_PRAGMA
#pragma interface
#endif

// Add Feature Support
#ifdef H323_H460
#include "h460/h4601.h"
#endif

#ifdef H323_H46025
#include "h460/h460_std25_pidf_lo.h"
#endif

// Add Association Support
#ifdef H323_H461
#include "h460/h461_base.h"
#endif

// Add NAT Method Support
#ifdef P_STUN
#include <ptclib/pnat.h>
class PSTUNClient;
#endif

// Add H.224 Handlers
#ifdef H323_H224
#include <h224/h224.h>
#endif

// Add T.140 Methods
#ifdef H323_T140
#include <h323t140.h>
#endif

class PHandleAggregator;

/* The following classes have forward references to avoid including the VERY
   large header files for H225 and H245. If an application requires access
   to the protocol classes they can include them, but for simple usage their
   inclusion can be avoided.
 */
class H225_EndpointType;
class H225_ArrayOf_SupportedProtocols;
class H225_VendorIdentifier;
class H225_H221NonStandard;
class H225_ServiceControlDescriptor;
class H225_ArrayOf_AliasAddress;

class H323SignalPDU;
class H323ConnectionsCleaner;
class H323ServiceControlSession;

#if H323_H224
class H224_H281Handler;
#endif

#ifdef H323_T120
class OpalT120Protocol;
#endif

#ifdef H323_T38
class OpalT38Protocol;
#endif

#ifdef H323_H460P
struct PresSubDetails;
class H460PresenceHandler;
#endif

#ifdef H323_GNUGK
class GNUGK_Feature;
#endif

#ifdef H323_UPnP
class PNatMethod_UPnP;
#endif

#ifdef H323_FILE
class H323FileTransferHandler;
class H323FileTransferList;
#endif

///////////////////////////////////////////////////////////////////////////////

/**This class manages the H323 endpoint.
   An endpoint may have zero or more listeners to create incoming connections
   or zero or more outgoing conenctions initiated via the MakeCall() function.
   Once a conection exists it is managed by this class instance.

   The main thing this class embodies is the capabilities of the application,
   that is the codecs and protocols it is capable of.

   An application may create a descendent off this class and overide the
   CreateConnection() function, if they require a descendent of H323Connection
   to be created. This would be quite likely in most applications.
 */
class H323EndPoint : public PObject
{
  PCLASSINFO(H323EndPoint, PObject);

  public:
    enum {
      DefaultTcpPort = 1720,
      DefaultTLSPort = 1300
    };

  /**@name Construction */
  //@{
    /**Create a new endpoint.
     */
    H323EndPoint();

    /**Destroy endpoint.
     */
    ~H323EndPoint();

    /**Set the endpoint information in H225 PDU's.
      */
    virtual void SetEndpointTypeInfo(
      H225_EndpointType & info
    ) const;

    /**Set the vendor information in H225 PDU's.
      */
    virtual void SetVendorIdentifierInfo(
      H225_VendorIdentifier & info
    ) const;

    /**Set the Gateway supported protocol default always H.323
      */
    PBoolean SetGatewaySupportedProtocol(
        H225_ArrayOf_SupportedProtocols & protocols
    ) const;

   /**Set the gateway prefixes
      Override this to set the acceptable prefixes to the gatekeeper
      */
    virtual PBoolean OnSetGatewayPrefixes(
        PStringList & prefixes
    ) const;

    /**Set the H221NonStandard information in H225 PDU's.
      */
    virtual void SetH221NonStandardInfo(
      H225_H221NonStandard & info
    ) const;
  //@}


  /**@name Capabilities */
  //@{
    /**Add a codec to the capabilities table. This will assure that the
       assignedCapabilityNumber field in the codec is unique for all codecs
       installed on this endpoint.

       If the specific instnace of the capability is already in the table, it
       is not added again. Ther can be multiple instances of the same
       capability class however.
     */
    void AddCapability(
      H323Capability * capability   ///< New codec specification
    );

    /**Set the capability descriptor lists. This is three tier set of
       codecs. The top most level is a list of particular capabilities. Each
       of these consists of a list of alternatives that can operate
       simultaneously. The lowest level is a list of codecs that cannot
       operate together. See H323 section 6.2.8.1 and H245 section 7.2 for
       details.

       If descriptorNum is P_MAX_INDEX, the the next available index in the
       array of descriptors is used. Similarly if simultaneous is P_MAX_INDEX
       the the next available SimultaneousCapabilitySet is used. The return
       value is the index used for the new entry. Note if both are P_MAX_INDEX
       then the return value is the descriptor index as the simultaneous index
       must be zero.

       Note that the capability specified here is automatically added to the
       capability table using the AddCapability() function. A specific
       instance of a capability is only ever added once, so multiple
       SetCapability() calls with the same H323Capability pointer will only
       add that capability once.
     */
    PINDEX SetCapability(
      PINDEX descriptorNum, ///< The member of the capabilityDescriptor to add
      PINDEX simultaneous,  ///< The member of the SimultaneousCapabilitySet to add
      H323Capability * cap  ///< New capability specification
    );

    /**Manually remove capability type. This removes the specified Capability type out of the
       default capability list.
      */
    PBoolean RemoveCapability(
        H323Capability::MainTypes capabilityType  ///< capability type
    );

#ifdef H323_VIDEO
    /**Set the Video Frame Size. This is used for capabilities
       that use 1 definition for all Video Frame Sizes. This will remove all capabilities
       not matching the specified Frame Size and send a message to the remaining video capabilities
       to set the maximum framesize allowed to the specified value
      */
    PBoolean SetVideoFrameSize(H323Capability::CapabilityFrameSize frameSize,
                          int frameUnits = 1
    );

    /**Set the Video Encoder size and rate.
        This is used for generic Video Capabilities to set the appropriate level for a given encoder
        frame size and rate.
      */
    PBoolean SetVideoEncoder(unsigned frameWidth, unsigned frameHeight, unsigned frameRate);

#endif

    /**Add all matching capabilities in list.
       All capabilities that match the specified name are added. See the
       capabilities code for details on the matching algorithm.
      */
    PINDEX AddAllCapabilities(
      PINDEX descriptorNum, ///< The member of the capabilityDescriptor to add
      PINDEX simultaneous,  ///< The member of the SimultaneousCapabilitySet to add
      const PString & name  ///< New capabilities name, if using "known" one.
    );

    /**Add all user input capabilities to this endpoints capability table.
      */
    void AddAllUserInputCapabilities(
      PINDEX descriptorNum, ///< The member of the capabilityDescriptor to add
      PINDEX simultaneous   ///< The member of the SimultaneousCapabilitySet to add
    );

#ifdef H323_VIDEO
#ifdef H323_H239
    /** Open Extended Video Session
      */
    PBoolean OpenExtendedVideoSession(
        const PString & token        ///< Connection Token
    );

    /** Open Extended Video Session
      */
    PBoolean OpenExtendedVideoSession(
        const PString & token,       ///< Connection Token
        H323ChannelNumber & num      ///< Opened Channel number
    );

    PBoolean CloseExtendedVideoSession(
        const PString & token        ///< Connection Token
    );

    PBoolean CloseExtendedVideoSession(
        const PString & token,             ///< Connection Token
        const H323ChannelNumber & num      ///< channel number
    );

    /**Add all Extended Video capabilities to this endpoints capability table.
      */
    void AddAllExtendedVideoCapabilities(
      PINDEX descriptorNum, ///< The member of the capabilityDescriptor to add
      PINDEX simultaneous   ///< The member of the SimultaneousCapabilitySet to add
    );
#endif
#endif

    /**Remove capabilites in table.
      */
    void RemoveCapabilities(
      const PStringArray & codecNames
    );

    /**Reorder capabilites in table.
      */
    void ReorderCapabilities(
      const PStringArray & preferenceOrder
    );

    /**Find a capability that has been registered.
     */
    H323Capability * FindCapability(
      const H245_Capability & cap  ///< H245 capability table entry
    ) const;

    /**Find a capability that has been registered.
     */
    H323Capability * FindCapability(
      const H245_DataType & dataType  ///< H245 data type of codec
    ) const;

    /**Find a capability that has been registered.
     */
    H323Capability * FindCapability(
      H323Capability::MainTypes mainType,   ///< Main type of codec
      unsigned subType                      ///< Subtype of codec
    ) const;
  //@}

  /**@name Gatekeeper management */
  //@{
    /**Use and register with an explicit gatekeeper.
       This will call other functions according to the following table:

           address    identifier   function
           empty      empty        DiscoverGatekeeper()
           non-empty  empty        SetGatekeeper()
           empty      non-empty    LocateGatekeeper()
           non-empty  non-empty    SetGatekeeperZone()

       The localAddress field, if non-empty, indicates the interface on which
       to look for the gatekeeper. An empty string is equivalent to "ip$*:*"
       which is any interface or port.

       If the endpoint is already registered with a gatekeeper that meets
       the same criteria then the gatekeeper is not changed, otherwise it is
       deleted (with unregistration) and new one created and registered to.
     */
    PBoolean UseGatekeeper(
      const PString & address = PString::Empty(),     ///< Address of gatekeeper to use.
      const PString & identifier = PString::Empty(),  ///< Identifier of gatekeeper to use.
      const PString & localAddress = PString::Empty() ///< Local interface to use.
    );

    /**Select and register with an explicit gatekeeper.
       This will use the specified transport and a string giving a transport
       dependent address to locate a specific gatekeeper. The endpoint will
       register with that gatekeeper and, if successful, set it as the current
       gatekeeper used by this endpoint.

       Note the transport being passed in will be deleted by this function or
       the H323Gatekeeper object it becomes associated with. Also if transport
       is NULL then a H323TransportUDP is created.
     */
    PBoolean SetGatekeeper(
      const PString & address,          ///< Address of gatekeeper to use.
      H323Transport * transport = NULL  ///< Transport over which to talk to gatekeeper.
    );

    /**Select and register with an explicit gatekeeper and zone.
       This will use the specified transport and a string giving a transport
       dependent address to locate a specific gatekeeper. The endpoint will
       register with that gatekeeper and, if successful, set it as the current
       gatekeeper used by this endpoint.

       The gatekeeper identifier is set to the spplied parameter to allow the
       gatekeeper to either allocate a zone or sub-zone, or refuse to register
       if the zones do not match.

       Note the transport being passed in will be deleted by this function or
       the H323Gatekeeper object it becomes associated with. Also if transport
       is NULL then a H323TransportUDP is created.
     */
    PBoolean SetGatekeeperZone(
      const PString & address,          ///< Address of gatekeeper to use.
      const PString & identifier,       ///< Identifier of gatekeeper to use.
      H323Transport * transport = NULL  ///< Transport over which to talk to gatekeeper.
    );

    /**Locate and select gatekeeper.
       This function will use the automatic gatekeeper discovery methods to
       locate the gatekeeper on the particular transport that has the specified
       gatekeeper identifier name. This is often the "Zone" for the gatekeeper.

       Note the transport being passed in will be deleted becomes owned by the
       H323Gatekeeper created by this function and will be deleted by it. Also
       if transport is NULL then a H323TransportUDP is created.
     */
    PBoolean LocateGatekeeper(
      const PString & identifier,       ///< Identifier of gatekeeper to locate.
      H323Transport * transport = NULL  ///< Transport over which to talk to gatekeeper.
    );

    /**Discover and select gatekeeper.
       This function will use the automatic gatekeeper discovery methods to
       locate the first gatekeeper on a particular transport.

       Note the transport being passed in will be deleted becomes owned by the
       H323Gatekeeper created by this function and will be deleted by it. Also
       if transport is NULL then a H323TransportUDP is created.
     */
    PBoolean DiscoverGatekeeper(
      H323Transport * transport = NULL  ///< Transport over which to talk to gatekeeper.
    );

    /**Create a gatekeeper.
       This allows the application writer to have the gatekeeper as a
       descendent of the H323Gatekeeper in order to add functionality to the
       base capabilities in the library.

       The default creates an instance of the H323Gatekeeper class.
     */
    virtual H323Gatekeeper * CreateGatekeeper(
      H323Transport * transport  ///< Transport over which gatekeepers communicates.
    );

    /**Get the gatekeeper we are registered with.
     */
    H323Gatekeeper * GetGatekeeper() const { return gatekeeper; }

    /**Return if endpoint is registered with gatekeeper.
      */
    PBoolean IsRegisteredWithGatekeeper() const;

    /**Unregister and delete the gatekeeper we are registered with.
       The return value indicates FALSE if there was an error during the
       unregistration. However the gatekeeper is still removed and its
       instance deleted regardless of this error.
     */
    PBoolean RemoveGatekeeper(
      int reason = -1    ///< Reason for gatekeeper removal
    );

    /**Force the endpoint to reregister with Gatekeeper
      */
    void ForceGatekeeperReRegistration();

    /**Set the H.235 password for the gatekeeper.
      */
    virtual void SetGatekeeperPassword(
      const PString & password
    );

    /**Get the H.235 password for the gatekeeper.
      */
    virtual const PString & GetGatekeeperPassword() const { return gatekeeperPassword; }

    /** Check the IP of the Gatekeeper on reregistration
        Use this to check if Gatekeeper IP had changed (used for DDNS type registrations)
        Default returns FALSE
      */
    virtual PBoolean GatekeeperCheckIP(const H323TransportAddress & oldAddr,H323TransportAddress & newaddress);

    /**Create priority list for Gatekeeper authenticators.
      */
    virtual void SetAuthenticatorOrder(PStringList & names);

    /**Create a list of authenticators for gatekeeper.
      */
    virtual H235Authenticators CreateAuthenticators();

    /**Called when sending a Admission Request to the gatekeeper
     */
    virtual void OnAdmissionRequest(H323Connection & connection);

    /**Called when the gatekeeper sends a GatekeeperConfirm
      */
    virtual void  OnGatekeeperConfirm();

    /**Called when the gatekeeper sends a GatekeeperReject
      */
    virtual void  OnGatekeeperReject();

    /**Called when the gatekeeper sends a RegistrationConfirm
      */
    virtual void OnRegistrationConfirm(const H323TransportAddress & rasAddress);

    /**Called when the gatekeeper sends a RegistrationReject
      */
    virtual void  OnRegistrationReject();

    /**Called when Unregistered by Gatekeeper
     */
    virtual void OnUnRegisterRequest();

    /**Called when reply Unregister to Gatekeeper
     */
    virtual void OnUnRegisterConfirm();

    /**Called when TTL registration fails
     */
    virtual void OnRegisterTTLFail();

    /**When an IP address change has been detected
       This will remove the listener and gatekeeper
       and bind to the new detected IP address
     */
    virtual PBoolean OnDetectedIPChange(PIPSocket::Address newIP = PIPSocket::Address::GetAny(4));
  //@}

  /**@name Connection management */
  //@{
    /**Add a listener to the endpoint.
       This allows for the automatic creating of incoming call connections. An
       application should use OnConnectionEstablished() to monitor when calls
       have arrived and been successfully negotiated.

       Note if this returns TRUE, then the endpoint is responsible for
       deleting the H323Listener listener object. If FALSE is returned then
       the object is not deleted and it is up to the caller to release the
       memory allocated for the object.

       If a listener already exists on the same transport address then it is
       ignored, but TRUE is still returned. The caller does not need to delete
       the object.
      */
    PBoolean StartListener(
      H323Listener * listener ///< Transport dependent listener.
    );

    /**Add a listener to the endpoint.
       This allows for the automatic creating of incoming call connections. An
       application should use OnConnectionEstablished() to monitor when calls
       have arrived and been successfully negotiated.

       If a listener already exists on the same address then it is ignored,
       but TRUE is still returned.

       If the iface string is empty then "*" is assumed which will listen on
       the standard TCP port on INADDR_ANY.
      */
    PBoolean StartListener(
      const H323TransportAddress & iface ///< Address of interface to listen on.
    );

    /**Add listeners to the endpoint.
       Set the collection of listeners which will allow the automatic
       creating of incoming call connections. An application should use
       OnConnectionEstablished() to monitor when calls have arrived and been
       successfully negotiated.

       If a listener already exists on the interface specified in the list
       then it is ignored. If a listener does not yet exist a new one is
       created and if a listener is running that is not in the list then it
       is stopped and removed.

       If the array is empty then the string "*" is assumed which will listen
       on the standard TCP port on INADDR_ANY.

       Returns TRUE if at least one interface was successfully started.
      */
    PBoolean StartListeners(
      const H323TransportAddressArray & ifaces ///< Interfaces to listen on.
    );

    /**Remove a listener from the endpoint.
       If the listener parameter is NULL then all listeners are removed.
      */
    PBoolean RemoveListener(
      H323Listener * listener ///< Transport dependent listener.
    );

    /**Return a list of the transport addresses for all listeners on this endpoint
      */
    H323TransportAddressArray GetInterfaceAddresses(
      PBoolean excludeLocalHost = TRUE,       ///< Flag to exclude 127.0.0.1
      H323Transport * associatedTransport = NULL
                          ///< Associated transport for precedence and translation
    );

     /**Make a Authenticated call to a remote party.
    This Function sets Security Information to be included when calling
    a EP which requires Authentication
       */
      H323Connection * MakeAuthenticatedCall (
                        const PString & remoteParty,  ///* Remote party to call
                        const PString & UserName,     ///* UserName to Use (Default is LocalPartyName)
                        const PString & Password,     ///* Password to Use (MUST NOT BE EMPTY)
                        PString & token,              ///* String to receive token for connection
                        void * userData = NULL        ///* user data to pass to CreateConnection
     );

     /**Make a Supplementary call to a remote party.
        This Function makes a Non Call supplementary connection (lightweight call) for the purpose
        of delivering H.450 & H.460 non call content such as instant messaging and messagebank messages
       */
      H323Connection * MakeSupplimentaryCall (
                        const PString & remoteParty,  ///* Remote party to call
                        PString & token,              ///* String to receive token for connection
                        void * userData = NULL        ///* user data to pass to CreateConnection
      );

    /**Make a call to a remote party. An appropriate transport is determined
       from the remoteParty parameter. The general form for this parameter is
       [alias@][transport$]host[:port] where the default alias is the same as
       the host, the default transport is "ip" and the default port is 1720.

       This function returns almost immediately with the call occurring in a
       new background thread. Note that the call could be created and cleared
       ie OnConnectionCleared is called BEFORE this function returns. It is
       guaranteed that the token variable is set before OnConnectionCleared
       called.

       Note, the returned pointer to the connection is not locked and may be
       deleted at any time. This is extremely unlikely immediately after the
       function is called, but you should not keep this pointer beyond that
       brief time. The the FindConnectionWithLock() function for future
       references to the connection object. It is recommended that
       MakeCallLocked() be usedin instead.
     */
    H323Connection * MakeCall(
      const PString & remoteParty,     ///< Remote party to call
      PString & token,                 ///< String to receive token for connection
      void * userData = NULL,           ///< user data to pass to CreateConnection
      PBoolean supplementary = false   ///< Whether the call is a supplementary call
    );

    /**Make a call to a remote party using the specified transport.
       The remoteParty may be a hostname, alias or other user name that is to
       be passed to the transport, if present.

       If the transport parameter is NULL the transport is determined from the
       remoteParty description.

       This function returns almost immediately with the call occurring in a
       new background thread. Note that the call could be created and cleared
       ie OnConnectionCleared is called BEFORE this function returns. It is
       guaranteed that the token variable is set before OnConnectionCleared
       called.

       Note, the returned pointer to the connection is not locked and may be
       deleted at any time. This is extremely unlikely immediately after the
       function is called, but you should not keep this pointer beyond that
       brief time. The the FindConnectionWithLock() function for future
       references to the connection object. It is recommended that
       MakeCallLocked() be usedin instead.
     */
    H323Connection * MakeCall(
      const PString & remoteParty,  ///< Remote party to call
      H323Transport * transport,    ///< Transport to use for call.
      PString & token,              ///< String to receive token for connection
      void * userData = NULL,        ///< user data to pass to CreateConnection
      PBoolean supplementary = false   ///< Whether the call is a supplementary call
    );

    /**Make a call to a remote party using the specified transport.
       The remoteParty may be a hostname, alias or other user name that is to
       be passed to the transport, if present.

       If the transport parameter is NULL the transport is determined from the
       remoteParty description.

       This function returns almost immediately with the call occurring in a
       new background thread. However the call will not progress very far
     */
    H323Connection * MakeCallLocked(
      const PString & remoteParty,     ///< Remote party to call
      PString & token,                 ///< String to receive token for connection
      void * userData = NULL,          ///< user data to pass to CreateConnection
      H323Transport * transport = NULL ///< Transport to use for call.
    );

#ifdef H323_H450

  /**@name H.450.2 Call Transfer */

    /**Setup the transfer of an existing call (connection) to a new remote party
       using H.450.2.  This sends a Call Transfer Setup Invoke message from the
       B-Party (transferred endpoint) to the C-Party (transferred-to endpoint).

       If the transport parameter is NULL the transport is determined from the
       remoteParty description. The general form for this parameter is
       [alias@][transport$]host[:port] where the default alias is the same as
       the host, the default transport is "ip" and the default port is 1720.

       This function returns almost immediately with the transfer occurring in a
       new background thread.

       Note, the returned pointer to the connection is not locked and may be
       deleted at any time. This is extremely unlikely immediately after the
       function is called, but you should not keep this pointer beyond that
       brief time. The the FindConnectionWithLock() function for future
       references to the connection object.

       This function is declared virtual to allow an application to override
       the function and get the new call token of the forwarded call.
     */
    virtual H323Connection * SetupTransfer(
      const PString & token,        ///< Existing connection to be transferred
      const PString & callIdentity, ///< Call identity of the secondary call (if it exists)
      const PString & remoteParty,  ///< Remote party to transfer the existing call to
      PString & newToken,           ///< String to receive token for the new connection
      void * userData = NULL        ///< user data to pass to CreateConnection
    );

    /**Initiate the transfer of an existing call (connection) to a new remote
       party using H.450.2.  This sends a Call Transfer Initiate Invoke
       message from the A-Party (transferring endpoint) to the B-Party
       (transferred endpoint).
     */
    void TransferCall(
      const PString & token,        ///< Existing connection to be transferred
      const PString & remoteParty,  ///< Remote party to transfer the existing call to
      const PString & callIdentity = PString::Empty()
                                    ///< Call Identity of secondary call if present
    );

    /**Transfer the call through consultation so the remote party in the
       primary call is connected to the called party in the second call
       using H.450.2.  This sends a Call Transfer Identify Invoke message
       from the A-Party (transferring endpoint) to the C-Party
       (transferred-to endpoint).
     */
    void ConsultationTransfer(
      const PString & primaryCallToken,   ///< Token of primary call
      const PString & secondaryCallToken  ///< Token of secondary call
    );

    /**Place the call on hold, suspending all media channels (H.450.4)
    * NOTE: Only Local Hold is implemented so far.
    */
    void HoldCall(
      const PString & token,        ///< Existing connection to be transferred
      PBoolean localHold   ///< true for Local Hold, false for Remote Hold
    );

    /** Initiate Call intrusion
        Designed similar to MakeCall function
      */
    H323Connection * IntrudeCall(
      const PString & remoteParty,  ///< Remote party to intrude call
      PString & token,              ///< String to receive token for connection
      unsigned capabilityLevel,     ///< Capability level
      void * userData = NULL        ///< user data to pass to CreateConnection
    );

    H323Connection * IntrudeCall(
      const PString & remoteParty,  ///< Remote party to intrude call
      H323Transport * transport,    ///< Transport to use for call.
      PString & token,              ///< String to receive token for connection
      unsigned capabilityLevel,     ///< Capability level
      void * userData = NULL        ///< user data to pass to CreateConnection
    );

  //@}


  /**@name H.450.7 MWI */

    /**Received a message wait indication.
        Override to collect MWI messages.
        default returns true.
        return false indicates MWI rejected.
      */
    virtual PBoolean OnReceivedMWI(const H323Connection::MWIInformation & mwiInfo);

    /**Received a message wait indication Clear.
        Override to remove any outstanding MWIs.
        default returns true.
        return false indicates MWI rejected.
      */
    virtual PBoolean OnReceivedMWIClear(const PString & user);

    /**Received a message wait indication request on a mail server (Interrogate).
        This is used for enquiring on a mail server if
        there are any active messages for the served user.
        An implementer may populate a H323Connection::MWInformation struct and call
        H323Connection::SetMWINonCallParameters to set the interrogate reply
        default returns true. if SetMWINonCallParameters called the connect message
        will contain a reply msg to the caller.
        return false indicates MWI rejected.
      */
    virtual PBoolean OnReceivedMWIRequest(
        H323Connection * connection,   ///< Connection the request came in on
        const PString & user           ///< User request made for
    );

    /** Get the Msg Centre name for H450.7 MWI service. */
    const PString & GetMWIMessageCentre()  { return mwiMsgCentre; }

    /** Set the MWI MessageCentre Alias Address (Required when using H323plus as videoMail server for callback) */
    void SetMWIMessageCentre(const PString & msgCtr)  {  mwiMsgCentre = msgCtr; }

  //@}

#endif // H323_H450

    /** Use DNS SRV and ENUM to resolve all the possible addresses a call party
       can be found. Only effective if not registered with Gatekeeper
      */
    PBoolean ResolveCallParty(
      const PString & _remoteParty,
      PStringList & addresses
    );

    /**Parse a party address into alias and transport components.
       An appropriate transport is determined from the remoteParty parameter.
       The general form for this parameter is [alias@][transport$]host[:port]
       where the default alias is the same as the host, the default transport
       is "ip" and the default port is 1720.
      */
    virtual PBoolean ParsePartyName(
      const PString & party,          ///< Party name string.
      PString & alias,                ///< Parsed alias name
      H323TransportAddress & address  ///< Parsed transport address
    );

    /**Clear a current connection.
       This hangs up the connection to a remote endpoint. Note that this function
       is asynchronous
      */
    virtual PBoolean ClearCall(
      const PString & token,  ///< Token for identifying connection
      H323Connection::CallEndReason reason =
                  H323Connection::EndedByLocalUser ///< Reason for call clearing
    );

     /**Clearing a current connection.
        A connection is being cleared callback. This can be used for PBX applications
        to reallocate the line early without waiting for the cleaner thread to clean-up
        the connection.
       */
     virtual void OnCallClearing(H323Connection * connection,       ///* Connection being Cleared
                              H323Connection::CallEndReason reason  ///* Reason for call being cleared
     );

    /**Clear a current connection.
       This hangs up the connection to a remote endpoint. Note that these functions
       are synchronous
      */
    virtual PBoolean ClearCallSynchronous(
      const PString & token,            ///< Token for identifying connection
      H323Connection::CallEndReason reason =
                        H323Connection::EndedByLocalUser ///< Reason for call clearing
    );
    virtual PBoolean ClearCallSynchronous(
      const PString & token,                ///< Token for identifying connection
      H323Connection::CallEndReason reason, ///< Reason for call clearing
      PSyncPoint * sync
    );

    /**Clear all current connections.
       This hangs up all the connections to remote endpoints. The wait
       parameter is used to wait for all the calls to be cleared and their
       memory usage cleaned up before returning. This is typically used in
       the destructor for your descendant of H323EndPoint.
      */
    virtual void ClearAllCalls(
      H323Connection::CallEndReason reason =
                  H323Connection::EndedByLocalUser, ///< Reason for call clearing
      PBoolean wait = TRUE   ///< Flag for wait for calls to e cleared.
    );

    /**Determine if the connectionMutex will block
      */
    virtual PBoolean WillConnectionMutexBlock();

    /**Determine if a connection is active.
      */
    virtual PBoolean HasConnection(
      const PString & token   ///< Token for identifying connection
    );

    /**Find a connection that uses the specified token.
       This searches the endpoint for the connection that contains the token
       as provided by functions such as MakeCall() (as built by the
       BuildConnectionToken() function). if not found it will then search for
       the string representation of the CallIdentifier for the connection, and
       finally try for the string representation of the ConferenceIdentifier.

       Note the caller of this function MUSt call the H323Connection::Unlock()
       function if this function returns a non-NULL pointer. If it does not
       then a deadlock can occur.
      */
    H323Connection * FindConnectionWithLock(
      const PString & token     ///< Token to identify connection
    );

    /**Get all calls current on the endpoint.
      */
    PStringList GetAllConnections();

    /**Call back for incoming call.
       This function is called from the OnReceivedSignalSetup() function
       before it sends the Alerting PDU. It gives an opportunity for an
       application to alter the reply before transmission to the other
       endpoint.

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour simply returns TRUE.
     */
    virtual PBoolean OnIncomingCall(
      H323Connection & connection,    ///< Connection that was established
      const H323SignalPDU & setupPDU,   ///< Received setup PDU
      H323SignalPDU & alertingPDU       ///< Alerting PDU to send
    );
    virtual PBoolean OnIncomingCall(
      H323Connection & connection,           ///< Connection that was established
      const H323SignalPDU & setupPDU,        ///< Received setup PDU
      H323SignalPDU & alertingPDU,           ///< Alerting PDU to send
      H323Connection::CallEndReason & reason ///< reason for call refusal, if returned false
    );

    /**Handle a connection transfer.
       This gives the application an opportunity to abort the transfer.
       The default behaviour just returns TRUE.
      */
    virtual PBoolean OnCallTransferInitiate(
      H323Connection & connection,    ///< Connection to transfer
      const PString & remoteParty     ///< Party transferring to.
    );

    /**Handle a transfer via consultation.
       This gives the transferred-to user an opportunity to abort the transfer.
       The default behaviour just returns TRUE.
      */
    virtual PBoolean OnCallTransferIdentify(
      H323Connection & connection    ///< Connection to transfer
    );

    /**
      * Callback for ARQ send to a gatekeeper. This allows the endpoint
      * to change or check fields in the ARQ before it is sent.
      */
    virtual void OnSendARQ(
      H323Connection & conn,
      H225_AdmissionRequest & arq
    );

   /**
      * Callback for ACF sent back from gatekeeper.
      */
    virtual void OnReceivedACF(
      H323Connection & conn,
      const H225_AdmissionConfirm & acf
    );

   /**
      * Callback for ARJ sent back from  gatekeeper.
      */
    virtual void OnReceivedARJ(
      H323Connection & conn,
      const H225_AdmissionReject & arj
    );

    /**Call back for answering an incoming call.
       This function is called from the OnReceivedSignalSetup() function
       before it sends the Connect PDU. It gives an opportunity for an
       application to alter the reply before transmission to the other
       endpoint.

       It also gives an application time to wait for some event before
       signalling to the endpoint that the connection is to proceed. For
       example the user pressing an "Answer call" button.

       If AnswerCallDenied is returned the connection is aborted and a Release
       Complete PDU is sent. If AnswerCallNow is returned then the H.323
       protocol proceeds. Finally if AnswerCallPending is returned then the
       protocol negotiations are paused until the AnsweringCall() function is
       called.

       The default behaviour simply returns AnswerNow.
     */
    virtual H323Connection::AnswerCallResponse OnAnswerCall(
      H323Connection & connection,    ///< Connection that was established
      const PString & callerName,       ///< Name of caller
      const H323SignalPDU & setupPDU,   ///< Received setup PDU
      H323SignalPDU & connectPDU        ///< Connect PDU to send.
    );

    /**Call back for remote party being alerted.
       This function is called from the SendSignalSetup() function after it
       receives the optional Alerting PDU from the remote endpoint. That is
       when the remote "phone" is "ringing".

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour simply returns TRUE.
     */
    virtual PBoolean OnAlerting(
      H323Connection & connection,    ///< Connection that was established
      const H323SignalPDU & alertingPDU,  ///< Received Alerting PDU
      const PString & user                ///< Username of remote endpoint
    );

    /**A call back function when a connection indicates it is to be forwarded.
       An H323 application may handle this call back so it can make
       complicated decisions on if the call forward ius to take place. If it
       decides to do so it must call MakeCall() and return TRUE.

       The default behaviour simply returns FALSE and that the automatic
       call forwarding should take place. See ForwardConnection()
      */
    virtual PBoolean OnConnectionForwarded(
      H323Connection & connection,    ///< Connection to be forwarded
      const PString & forwardParty,   ///< Remote party to forward to
      const H323SignalPDU & pdu       ///< Full PDU initiating forwarding
    );

    /**Forward the call using the same token as the specified connection.
       Return TRUE if the call is being redirected.

       The default behaviour will replace the current call in the endpoints
       call list using the same token as the call being redirected. Not that
       even though the same token is being used the actual object is
       completely mad anew.
      */
    virtual PBoolean ForwardConnection(
      H323Connection & connection,    ///< Connection to be forwarded
      const PString & forwardParty,   ///< Remote party to forward to
      const H323SignalPDU & pdu       ///< Full PDU initiating forwarding
    );

    /**A call back function whenever a connection is established.
       This indicates that a connection to a remote endpoint was established
       with a control channel and zero or more logical channels.

       The default behaviour does nothing.
      */
    virtual void OnConnectionEstablished(
      H323Connection & connection,    ///< Connection that was established
      const PString & token           ///< Token for identifying connection
    );

    /**Determine if a connection is established.
      */
    virtual PBoolean IsConnectionEstablished(
      const PString & token   ///< Token for identifying connection
    );

    /**A call back function whenever a connection is broken.
       This indicates that a connection to a remote endpoint is no longer
       available.

       The default behaviour does nothing.
      */
    virtual void OnConnectionCleared(
      H323Connection & connection,    ///< Connection that was established
      const PString & token           ///< Token for identifying connection
    );

    /**Build a unique token for the connection.
       This identifies the call using the Q931 transport host name and the
       Q931 call reference number.
      */
    static PString BuildConnectionToken(
      const H323Transport & transport,  ///< Transport for connection
      unsigned callReference,           ///< Call reference of Q.931 link
      PBoolean fromRemote                   ///< Call reference is from remote endpoint
    );

    /**Handle a new incoming connection.
       This will examine the setup PDU and either attach the signalling
       transport to an existing connection that has the same Q.931 call
       reference, or creates a new connection using CreateConnection().
      */
    virtual H323Connection * OnIncomingConnection(
      H323Transport * transport,  ///< Transport for connection
      H323SignalPDU & setupPDU    ///< Setup PDU
    );

    /**Called when an outgoing call connects
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour simply returns TRUE.
      */
    virtual PBoolean OnOutgoingCall(
        H323Connection & conn,
        const H323SignalPDU & connectPDU
    );

    /**Create a connection that uses the specified call reference.
      */
    virtual H323Connection * CreateConnection(
      unsigned callReference,     ///< Call reference to use
      void * userData,            ///< user data to pass to CreateConnection
      H323Transport * transport,  ///< Transport for connection
      H323SignalPDU * setupPDU    ///< Setup PDU, NULL if outgoing call
    );
    virtual H323Connection * CreateConnection(
      unsigned callReference,   ///< Call reference to use
      void * userData           ///< user data to pass to CreateConnection
    );
    virtual H323Connection * CreateConnection(
      unsigned callReference    ///< Call reference to use
    );

    /**Clean up connections.
       This function is called from a background thread and checks for closed
       connections to clean up.

       This would not normally be called by an application.
      */
    virtual void CleanUpConnections();
  //@}

  /**@name Caller Authentication */
  //@{
    /**Create a list of authenticators for Call Authentication.
       To Create a list of Autheniticators the Endpoint MUST have
       set EPSecurityPassword (via SetEPCredentials()) and either
       set CallAuthPolicy (via SetEPSecurityPolicy()) or set
       isSecureCall to TRUE (via MakeAuthenticatedCall())
      */
     virtual H235Authenticators CreateEPAuthenticators();

    /** Retrieve Password and UserName for EPAuthentication
        NOTE: Returns FALSE is EPSecurityPassword.IsEmpty()
       */
    virtual PBoolean GetEPCredentials(PString & password,   ///* Password to use for call
                                  PString & username        ///* Username to use for call
                                  );

     /** Set the Password and UserName for EPAuthentication for Connection
       */
     virtual void SetEPCredentials(PString password,   ///* Password to use for call
                                   PString username    ///* Username to use for call
                                   );

     enum EPSecurityPolicy
     {
         SecNone,           ///* Default: Do Not Include Call Authenication
         SecRequest,        ///* Request Authentication but Accept if Missing/Fail
         SecRequired        ///* Calls are Rejected with EndedBySecurityDenial if Authenitication fails.
     };

     /** Set the EP Security Policy
       */
     virtual void SetEPSecurityPolicy(EPSecurityPolicy policy);

     /** Get the EP Security Policy
       */
     virtual EPSecurityPolicy GetEPSecurityPolicy();

     /** Retrieve the List of UserNames/Passwords to be used
     to Authenticate Incoming Calls.
       */
     H235AuthenticatorList GetAuthenticatorList();

     /** Call Authentication Call Back
     This fires for all the Authentication Methods created by
     CreateEPAuthenticators() The Function Supplies the Name of
     the Authentication process and the supplied SenderID (Username)
     and this is then check against EPAuthList to:
     1. Check if the username exists and if so
     2. Return the password in the clear to validate.
     Returning FALSE indicates that Authentication Failed failed for that Method..
       */
     virtual PBoolean OnCallAuthentication(const PString & username,  ///* UserName of Caller
                                        PString & password         ///* Password related to caller
                                        );

     /** Disable all authenticators using insecure MD5 hashing
       */
     virtual void DisableMD5Authenticators() { m_disableMD5Authenticators = TRUE; }
 //@}

#ifdef H323_H235
  /**@name Media Encryption */
  //@{
     enum H235MediaPolicy
     {
         encyptNone = 0,           ///< Default: Do Not Include Media Encryption
         encyptRequest = 1,        ///< Request Encryption but Accept if Missing/Fail
         encyptRequired = 2        ///< Calls are Rejected with EndedBySecurityDenial if no Media Encryption.
     };

     enum H235MediaCipher
     {
         encypt128 = 128             ///< Default: AES128
#ifdef H323_H235_AES256
         ,encypt192 = 192,           ///< AES192
         encypt256  = 256            ///< AES256
#endif
     };

    /** Enable Media Encryption
        This enables media encryption.
      */
     virtual void SetH235MediaEncryption(H235MediaPolicy policy,     ///< How endpoint handles Encryption
                                         H235MediaCipher level,      ///< Maximum supported Cipher
                                         unsigned maxTokenSize = 1024///< Maximum DH token size
                                         );

    /** Get Media Encryption Policy
        This get the media encryption policy.
      */
    H235MediaPolicy GetH235MediaPolicy();

    /** Get Media Encryption Cipher
        This get the media encryption Cipher.
      */
    H235MediaCipher GetH235MediaCipher();

    /** H235SetDiffieHellmanFiles Set DH prameters from File (can be multiple file paths seperated by ;)
        Data must be stored in INI format with the section being the OID of the Algorith
        Parameter names are as below and all values are base64 encoded. eg:
           [OID]
           PRIME=
           GENERATOR=

       Default Parameters are replaced if OID is one of
            1024bit "0.0.8.235.0.3.43"
            1536bit "0.0.8.235.0.3.44"
            2048bit "0.0.8.235.0.3.45"
            4096bit "0.0.8.235.0.3.47"
            6144bit "0.0.8.235.0.4.77"
            8192bit "0.0.8.235.0.4.78"
       Default operation calls SetEncryptionCacheFiles();
      */
    virtual void H235SetDiffieHellmanFiles(const PString & file);

    /** Set Encryption Cache File Paths (can be multiple file paths seperated by ;)
        Data must be stored in INI format with the section being the OID of the Algorith
        Parameter names are as below and all values are base64 encoded. eg:
           [OID]
           PRIME=
           GENERATOR=
           PUBLIC=  <optional>
           PRIVATE= <optional>

       Default Parameters are replaced if OID is one of
            1024bit "0.0.8.235.0.3.43"
            1536bit "0.0.8.235.0.3.44"
            2048bit "0.0.8.235.0.3.45"
            4096bit "0.0.8.235.0.3.47"
            6144bit "0.0.8.235.0.4.77"
            8192bit "0.0.8.235.0.4.78"

        This function call should only be called by the implementer directly for testing purposes
        use H235SetDiffieHellmanFiles in production.
      */
    virtual void SetEncryptionCacheFiles(const PString & cachefile);

    /** Get Encryption Cache File Path (can be mulitple file paths)
      */
    virtual const PString & GetEncryptionCacheFiles();

    /** Load custom DiffieHellman Parameters for media encryption

       Default Parameters are replaced if OID is one of
            1024bit "0.0.8.235.0.3.43"
            1536bit "0.0.8.235.0.3.44"
            2048bit "0.0.8.235.0.3.45"
            4096bit "0.0.8.235.0.3.47"
            6144bit "0.0.8.235.0.4.77"
            8192bit "0.0.8.235.0.4.78"
      */
    virtual void LoadDiffieHellmanParameters(const PString & oid,
                                             const PBYTEArray & pData,
                                             const PBYTEArray & gData);

    /**Initialise Encryption cache
       Use this to create the encryption information at Startup rather than at the
       start of every call. This speeds up call establishment for high media encryption
       sessions. MUST Call EncryptionCacheRemove() to cleanup cache
      */
    virtual void EncryptionCacheInitialise();

    /**Remove Encryption cache
       Use this to remove the encryption information
      */
    virtual void EncryptionCacheRemove();

    /** On Media Encryption
        Fires when an encryption session negotiated
        Fires for each media session direction
     */
    virtual void OnMediaEncryption(unsigned /*session*/,
                                H323Channel::Directions /*dir*/,
                                const PString & /*cipher*/
                                ) {};
  //@}
#endif

  /**@name Logical Channels management */
  //@{
    /**Call back for opening a logical channel.

       The default behaviour simply returns TRUE.
      */
    virtual PBoolean OnStartLogicalChannel(
      H323Connection & connection,    ///< Connection for the channel
      H323Channel & channel           ///< Channel being started
    );

    /**Call back for closed a logical channel.

       The default behaviour does nothing.
      */
    virtual void OnClosedLogicalChannel(
      H323Connection & connection,    ///< Connection for the channel
      const H323Channel & channel     ///< Channel being started
    );

#ifdef H323_AUDIO_CODECS
    /**Open a channel for use by an audio codec.
       The H323AudioCodec class will use this function to open the channel to
       read/write PCM data.

       The default function creates a PSoundChannel using the member variables
       soundChannelPlayDevice or soundChannelRecordDevice.
      */
    virtual PBoolean OpenAudioChannel(
      H323Connection & connection,  ///< Connection for the channel
      PBoolean isEncoding,              ///< Direction of data flow
      unsigned bufferSize,          ///< Size of each sound buffer
      H323AudioCodec & codec        ///< codec that is doing the opening
    );
#endif

#ifdef H323_VIDEO
    /**Open a channel for use by an video codec.
       The H323VideoCodec class will use this function to open the channel to
       read/write image data (which is one frame in a video stream - YUV411 format).

       The default function creates a PVideoChannel using the member variables.
      */
    virtual PBoolean OpenVideoChannel(
      H323Connection & connection,  ///< Connection for the channel
      PBoolean isEncoding,              ///< Direction of data flow
      H323VideoCodec & codec        ///< codec doing the opening
    );

#ifdef H323_H239
    /**Open a channel for use by an application share application.
       The H323VideoCodec class will use this function to open the channel to
       read/write image data (which is one frame in a video stream - YUV411 format).

       The default function creates a PVideoChannel using the member variables.
      */
    virtual PBoolean OpenExtendedVideoChannel(
      H323Connection & connection,  ///< Connection for the channel
      PBoolean isEncoding,              ///< Direction of data flow
      H323VideoCodec & codec        ///< codec doing the opening
    );
#endif // H323_H239
#endif // NO_H323_VIDEO

    /**Callback from the RTP session for statistics monitoring.
       This is called every so many packets on the transmitter and receiver
       threads of the RTP session indicating that the statistics have been
       updated.

       The default behaviour does nothing.
      */
    virtual void OnRTPStatistics(
      const H323Connection & connection,  ///< Connection for the channel
      const RTP_Session & session         ///< Session with statistics
    ) const;

   /**Callback from the RTP session for statistics monitoring.
       This is called at the end of the RTP session indicating that the statistics
       of the call

       The default behaviour does nothing.
      */
    virtual void OnRTPFinalStatistics(
      const H323Connection & connection,  ///< Connection for the channel
      const RTP_Session & session         ///< Session with statistics
    ) const;

  //@}

  /**@name Indications */
  //@{
    /**Call back for remote enpoint has sent user input as a string.

       The default behaviour does nothing.
      */
    virtual void OnUserInputString(
      H323Connection & connection,  ///< Connection for the input
      const PString & value         ///< String value of indication
    );

    /**Call back for remote enpoint has sent user input.

       The default behaviour calls H323Connection::OnUserInputTone().
      */
    virtual void OnUserInputTone(
      H323Connection & connection,  ///< Connection for the input
      char tone,                    ///< DTMF tone code
      unsigned duration,            ///< Duration of tone in milliseconds
      unsigned logicalChannel,      ///< Logical channel number for RTP sync.
      unsigned rtpTimestamp         ///< RTP timestamp in logical channel sync.
    );

#ifdef H323_GNUGK
     /**Call back from GK admission confirm to notify the Endpoint it is behind a NAT
    (GNUGK Gatekeeper) The default does nothing. Override this to notify the user they are behind a NAT.
     */
    virtual void OnGatekeeperNATDetect(
        PIPSocket::Address publicAddr,         ///< Public address as returned by the Gatekeeper
        const PString & gkIdentifier,                ///< Identifier at the gatekeeper
        H323TransportAddress & gkRouteAddress  ///< Gatekeeper Route Address
        );

     /**Call back from GK admission confirm to notify the Endpoint it is not detected as being NAT
    (GNUGK Gatekeeper) The default does nothing. Override this to notify the user they are not NAT
    so they can confirm that it is true.
     */
    virtual void OnGatekeeperOpenNATDetect(
        const PString & gkIdentifier,                ///< Identifier at the gatekeeper
        H323TransportAddress & gkRouteAddress  ///< Gatekeeper Route Address
        );

    /** Fired with the keep-alive connection to GnuGk fails or is re-established
        This allows the endpoint to re-register.
      */
    virtual void NATLostConnection(PBoolean lost);
#endif

    /** Call back for GK assigned aliases returned from the gatekeeper in the RCF.
        The default returns FALSE which appends the new aliases to the existing alias list.
        By overriding this function and returning TRUE overrides the default operation
      */
    virtual PBoolean OnGatekeeperAliases(
        const H225_ArrayOf_AliasAddress & aliases  ///< Alias List returned from the gatekeeper
        );
  //@}

#ifdef H323_H248
  /**@name Service Control */
  //@{
    /**Call back for HTTP based Service Control.
       An application may override this to use an HTTP link to display
       call information/CDR's or Billing information.

       The default behaviour does nothing.
      */
    virtual void OnHTTPServiceControl(
      unsigned operation,  ///< Control operation
      unsigned sessionId,  ///< Session ID for HTTP page
      const PString & url  ///< URL to use.
    );

    /**Call back for Call Credit Service Control.
       An application may override this to display call credit Information.

       The default behaviour does nothing.
      */
    virtual void OnCallCreditServiceControl(
      const PString & amount,         ///< UTF-8 string for amount, including currency.
      PBoolean mode,                      ///< Flag indicating that calls will debit the account.
      const unsigned & durationLimit  ///< Duration Limit (used to decrement display)
    );

#ifdef H323_H350
    /**Call back for LDAP based Service Control.
       An application may override this to use an LDAP directory to query
       White Page searches.

       The default behaviour does nothing.
      */
    virtual void OnH350ServiceControl(
        const PString & url,
        const PString & BaseDN
        );
#endif

    /**Call back for call credit information.
       An application may override this to display call credit information
       on registration, or when a call is started.

       The canDisplayAmountString member variable must also be set to TRUE
       for this to operate.

       The default behaviour does nothing.
      */
    virtual void OnCallCreditServiceControl(
      const PString & amount,  ///< UTF-8 string for amount, including currency.
      PBoolean mode          ///< Flag indicating that calls will debit the account.
    );

    /**Handle incoming service control session information.
       Default behaviour calls session.OnChange()
     */
    virtual void OnServiceControlSession(
      unsigned type,
      unsigned sessionid,
      const H323ServiceControlSession & session,
      H323Connection * connection
    );

    /**Create the service control session object.
     */
    virtual H323ServiceControlSession * CreateServiceControlSession(
      const H225_ServiceControlDescriptor & contents
    );
  //@}
#endif // H323_H248

  /**@name Other services */
  //@{
#ifdef H323_T120
    /**Create an instance of the T.120 protocol handler.
       This is called when the OpenLogicalChannel subsystem requires that
       a T.120 channel be established.

       Note that if the application overrides this it should return a pointer to a
       heap variable (using new) as it will be automatically deleted when the
       H323Connection is deleted.

       The default behavour returns NULL.
      */
    virtual OpalT120Protocol * CreateT120ProtocolHandler(
      const H323Connection & connection  ///< Connection for which T.120 handler created
    ) const;
#endif

#ifdef H323_T38
    /**Create an instance of the T.38 protocol handler.
       This is called when the OpenLogicalChannel subsystem requires that
       a T.38 fax channel be established.

       Note that if the application overrides this it should return a pointer to a
       heap variable (using new) as it will be automatically deleted when the
       H323Connection is deleted.

       The default behavour returns NULL.
      */
    virtual OpalT38Protocol * CreateT38ProtocolHandler(
      const H323Connection & connection  ///< Connection for which T.38 handler created
    ) const;
#endif

#if H323_H224

    /** Create an instance of the H.224 protocol handler.
        This is called when the subsystem requires that a H.224 channel be established.

        Note that if the application overrides this it should return a pointer to a
        heap variable (using new) as it will be automatically deleted when the Connection
        is deleted.

        The default behaviour creates a new OpalH224Handler.
      */
    virtual OpalH224Handler * CreateH224ProtocolHandler(
      H323Channel::Directions dir,
      H323Connection & connection,
      unsigned sessionID
    ) const;

    /** On Create an instance of the H.224 handler.
        This is called when the subsystem creates a H.224 Handler.

        Use this to derive implementers classes
        return false to not create Handler.

     The default behavour returns false
      */
    virtual PBoolean OnCreateH224Handler(
        H323Channel::Directions dir,
        const H323Connection & connection,
        const PString & id,
        H224_Handler * m_handler
    ) const;


#ifdef H224_H281
    /** Create an instance of the H.281 protocol handler.

        *** NOTE This function is depreciated for H323plus 1.26 ***
        *** Use OnCreateH224Handler function call instead ***

        This is called when the subsystem requires that a H.224 channel be established.

        Note that if the application overrides this it should return a pointer to a
        heap variable (using new) as it will be automatically deleted when the Connection
        is deleted.

        The default behaviour creates a new OpalH281Handler.
      */
    virtual H224_H281Handler * CreateH281ProtocolHandler(
      OpalH224Handler & h224Handler
    ) const;
#endif

#endif  // H323_H224

#ifdef H323_T140
    /** Create an instance of the RFC4103 protocol handler.

     The default behavour returns a new H323_RFC4103Handler;
     */
    virtual H323_RFC4103Handler * CreateRFC4103ProtocolHandler(
        H323Channel::Directions dir,
        H323Connection & connection,
        unsigned sessionID
    );
#endif // H323_T140

#ifdef H323_FILE
    /** Open File Transfer Session
        Use this to initiate a file transfer.
      */
    PBoolean OpenFileTransferSession(
        const H323FileTransferList & list,      ///< List of Files Requested
        const PString & token,                  ///< Connection Token
        H323ChannelNumber & num                 ///< Opened Channel number
    );

    /** Open a File Transfer Channel.
        This is called when the subsystem requires that a File Transfer channel be established.

        An implementer should override this function to facilitate file transfer.
        If transmitting, list of files should be populated to notify the channel which files to read.
        If receiving, the list of files should be altered to include path information for the storage
        of received files.

        The default behaviour returns FALSE to indicate File Transfer is not implemented.
      */
    virtual PBoolean OpenFileTransferChannel(H323Connection & connection,         ///< Connection
                                           PBoolean isEncoder,                    ///< direction of channel
                                           H323FileTransferList & filelist        ///< Transfer File List
                                           );
#endif
  //@}

  /**@name Additional call services */
  //@{
    /** Called when an endpoint receives a SETUP PDU with a
        conference goal of "invite"

        The default behaviour is to return FALSE, which will close the connection
     */
    virtual PBoolean OnConferenceInvite(
      PBoolean sending,                       ///< direction
      const H323Connection * connection,  ///< Connection
      const H323SignalPDU & setupPDU      ///< PDU message
    );

    /** Called when an endpoint receives a SETUP PDU with a
        conference goal of "callIndependentSupplementaryService"

        The default behaviour is to return FALSE, which will close the connection
     */
    virtual PBoolean OnSendCallIndependentSupplementaryService(
      const H323Connection * connection,  ///< Connection
      H323SignalPDU & pdu                 ///< PDU message
    );

    virtual PBoolean OnReceiveCallIndependentSupplementaryService(
      const H323Connection * connection,  ///< Connection
      const H323SignalPDU & pdu                 ///< PDU message
    );

    /** Called when an endpoint receives a SETUP PDU with a
        conference goal of "capability_negotiation"

        The default behaviour is to return FALSE, which will close the connection
     */
    virtual PBoolean OnNegotiateConferenceCapabilities(
      const H323SignalPDU & setupPDU
    );
  //@}

  /**@name Member variable access */
  //@{
    /**Set the user name to be used for the local end of any connections. This
       defaults to the logged in user as obtained from the
       PProcess::GetUserName() function.

       Note that this name is technically the first alias for the endpoint.
       Additional aliases may be added by the use of the AddAliasName()
       function, however that list will be cleared when this function is used.
     */
    virtual void SetLocalUserName(
      const PString & name  ///< Local name of endpoint (prime alias)
    );

    /**Get the user name to be used for the local end of any connections. This
       defaults to the logged in user as obtained from the
       PProcess::GetUserName() function.
     */
    virtual const PString & GetLocalUserName() const
        { return localAliasNames.GetSize() > 0 ? localAliasNames[0] : *(new PString()); }

    /**Add an alias name to be used for the local end of any connections. If
       the alias name already exists in the list then is is not added again.

       The list defaults to the value set in the SetLocalUserName() function.
       Note that calling SetLocalUserName() will clear the alias list.
     */
    PBoolean AddAliasName(
      const PString & name  ///< New alias name to add
    );

    /**Remove an alias name used for the local end of any connections.
       defaults to an empty list.
     */
    PBoolean RemoveAliasName(
      const PString & name  ///< New alias namer to add
    );

    /**Get the user name to be used for the local end of any connections. This
       defaults to the logged in user as obtained from the
       PProcess::GetUserName() function.
     */
    const PStringList & GetAliasNames() const { return localAliasNames; }

    /*Get the supported Speaking language
		Use this to set languages supported by the caller/called
	 */
	const PStringList & GetLocalLanguages() const { return localLanguages; }

    /*Set Whether to use the Q931 Display called and calling numbers instead
      of the default AliasAddresses in the caller display.
	 */
    void SetQ931Display(PBoolean success)  { useQ931Display = success; }

    /*Determine Whether to use the Q931 Display called and calling numbers instead
      of the default AliasAddresses in the caller display.
	 */
    PBoolean UseQ931Display()  { return useQ931Display; }

#if P_LDAP

    /**Get the default ILS server to use for user lookup.
      */
    const PString & GetDefaultILSServer() const { return ilsServer; }

    /**Set the default ILS server to use for user lookup.
      */
    void SetDefaultILSServer(
      const PString & server
      ) { ilsServer = server; }

#endif

    /**Get the default fast start mode.
      */
    PBoolean IsFastStartDisabled() const
      { return disableFastStart; }

    /**Set the default fast start mode.
      */
    void DisableFastStart(
      PBoolean mode ///< New default mode
    ) { disableFastStart = mode; }

    /**Get the default H.245 tunneling mode.
      */
    PBoolean IsH245TunnelingDisabled() const
      { return disableH245Tunneling; }

    /**Set the default H.245 tunneling mode.
      */
    void DisableH245Tunneling(
      PBoolean mode ///< New default mode
    ) { disableH245Tunneling = mode; }

    /**Check if H.245 messages are sent in Setup.
      */
    PBoolean IsH245inSetupDisabled() const
      { return disableH245inSetup; }

    /**Set the default for sending H.245 in Setup.
      */
    void DisableH245inSetup(
      PBoolean mode ///< New default mode
    ) { disableH245inSetup = mode; }

    /** Get the default H.245 QoS mode.
      */
    PBoolean IsH245QoSDisabled() const
      { return disableH245QoS; }

    /** Disable H.245 QoS support
      */
    void DisableH245QoS(
      PBoolean mode ///< New default mode
    ) { disableH245QoS = mode; }

    /**Get the detect in-band DTMF flag.
      */
    PBoolean DetectInBandDTMFDisabled() const
      { return disableDetectInBandDTMF; }

    /**Set the detect in-band DTMF flag.
      */
    void DisableDetectInBandDTMF(
      PBoolean mode ///< New default mode
    ) { disableDetectInBandDTMF = mode; }

    /**Disable RFC2833InBandDTMF.
      */
    PBoolean RFC2833InBandDTMFDisabled() const
      { return disableRFC2833InBandDTMF; }

    /**Set the RFC2833 flag.
      */
    void DisableRFC2833InBandDTMF(
      PBoolean mode ///< New default mode
    ) { disableRFC2833InBandDTMF = mode; }

   /**Disable extended user input.
      */
    PBoolean ExtendedUserInputDisabled() const
      { return disableExtendedUserInput; }

    /**Set the extended user input flag.
      */
    void DisableExtendedUserInput(
      PBoolean mode ///< New default mode
    ) { disableExtendedUserInput = mode; }


    /**Get the flag indicating the endpoint can display an amount string.
      */
    PBoolean CanDisplayAmountString() const
      { return canDisplayAmountString; }

    /**Set the flag indicating the endpoint can display an amount string.
      */
    void SetCanDisplayAmountString(
      PBoolean mode ///< New default mode
    ) { canDisplayAmountString = mode; }

    /**Get the flag indicating the call will automatically clear after a time.
      */
    PBoolean CanEnforceDurationLimit() const
      { return canEnforceDurationLimit; }

    /**Set the flag indicating the call will automatically clear after a time.
      */
    void SetCanEnforceDurationLimit(
      PBoolean mode ///< New default mode
    ) { canEnforceDurationLimit = mode; }

#ifdef H323_RTP_AGGREGATE
    /**Set the RTP aggregation size
      */
    void SetRTPAggregatationSize(
      PINDEX size            ///< max connections per aggregation thread. Value of 1 or zero disables aggregation
    ) { rtpAggregationSize = size; }

    /**Get the RTP aggregation size
      */
    PINDEX GetRTPAggregationSize() const
    { return rtpAggregationSize; }

    /** Get the aggregator used for RTP channels
      */
    PHandleAggregator * GetRTPAggregator();
#endif

#ifdef H323_SIGNAL_AGGREGATE
    /**Set the signalling aggregation size
      */
    void SetSignallingAggregationSize(
      PINDEX size            ///< max connections per aggregation thread. Value of 1 or zero disables aggregation
    ) { signallingAggregationSize = size; }

    /**Get the RTP aggregation size
      */
    PINDEX GetSignallingAggregationSize() const
    { return signallingAggregationSize; }

    /** Get the aggregator used for signalling channels
      */
    PHandleAggregator * GetSignallingAggregator();
#endif

#ifdef H323_H450

  /**@name H.450.11 Call Intrusion */
    /**Get Call Intrusion Protection Level of the end point.
      */
    unsigned GetCallIntrusionProtectionLevel() const { return callIntrusionProtectionLevel; }

    /**Set Call Intrusion Protection Level of the end point.
      */
    void SetCallIntrusionProtectionLevel(
      unsigned level  ///< New level from 0 to 3
    ) { PAssert(level<=3, PInvalidParameter); callIntrusionProtectionLevel = level; }

    /**Called from H.450 OnReceivedInitiateReturnError
      */
    virtual void OnReceivedInitiateReturnError();

  //@}

#endif // H323_H450

#ifdef H323_AUDIO_CODECS
#ifdef P_AUDIO

    /**Set the name for the sound channel to be used for output.
       If the name is not suitable for use with the PSoundChannel class then
       the function will return FALSE and not change the device.

       This defaults to the value of the PSoundChannel::GetDefaultDevice()
       function.
     */
    virtual PBoolean SetSoundChannelPlayDevice(const PString & name);
    virtual PBoolean SetSoundChannelPlayDriver(const PString & name);

    /**Get the name for the sound channel to be used for output.
       This defaults to the value of the PSoundChannel::GetDefaultDevice()
       function.
     */
    const PString & GetSoundChannelPlayDevice() const { return soundChannelPlayDevice; }
    const PString & GetSoundChannelPlayDriver() const { return soundChannelPlayDriver; }

    /**Set the name for the sound channel to be used for input.
       If the name is not suitable for use with the PSoundChannel class then
       the function will return FALSE and not change the device.

       This defaults to the value of the PSoundChannel::GetDefaultDevice()
       function.
     */
    virtual PBoolean SetSoundChannelRecordDevice(const PString & name);
    virtual PBoolean SetSoundChannelRecordDriver(const PString & name);

    /**Get the name for the sound channel to be used for input.
       This defaults to the value of the PSoundChannel::GetDefaultDevice()
       function.
     */
    const PString & GetSoundChannelRecordDevice() const { return soundChannelRecordDevice; }
    const PString & GetSoundChannelRecordDriver() const { return soundChannelRecordDriver; }

    /**Get default the sound channel buffer depth.
      */
    unsigned GetSoundChannelBufferDepth() const { return soundChannelBuffers; }

    /**Set the default sound channel buffer depth.
      */
    void SetSoundChannelBufferDepth(
      unsigned depth    ///< New depth
    );

#endif  // P_AUDIO

    /**Get the default silence detection mode.
      */
    H323AudioCodec::SilenceDetectionMode GetSilenceDetectionMode() const
      { return defaultSilenceDetection; }

    /**Set the default silence detection mode.
      */
    void SetSilenceDetectionMode(
      H323AudioCodec::SilenceDetectionMode mode ///< New default mode
    ) { defaultSilenceDetection = mode; }

#endif  // H323_AUDIO_CODECS

    /**Get the default mode for sending User Input Indications.
      */
    H323Connection::SendUserInputModes GetSendUserInputMode() const { return defaultSendUserInputMode; }

    /**Set the default mode for sending User Input Indications.
      */
    void SetSendUserInputMode(H323Connection::SendUserInputModes mode) { defaultSendUserInputMode = mode; }

#ifdef H323_AUDIO_CODECS

    /**See if should auto-start receive audio channels on connection.
     */
    PBoolean CanAutoStartReceiveAudio() const { return autoStartReceiveAudio; }

    /**See if should auto-start transmit audio channels on connection.
     */
    PBoolean CanAutoStartTransmitAudio() const { return autoStartTransmitAudio; }

#endif

#ifdef H323_VIDEO

    /**See if should auto-start receive video channels on connection.
     */
    PBoolean CanAutoStartReceiveVideo() const { return autoStartReceiveVideo; }

    /**See if should auto-start transmit video channels on connection.
     */
    PBoolean CanAutoStartTransmitVideo() const { return autoStartTransmitVideo; }

#ifdef H323_H239
    /**See if should auto-start receive extended Video channels on connection.
     */
    PBoolean CanAutoStartReceiveExtVideo() const { return autoStartReceiveExtVideo; }

    /**See if should auto-start transmit extended Video channels on connection.
     */
    PBoolean CanAutoStartTransmitExtVideo() const { return autoStartTransmitExtVideo; }

#endif  // H323_H239
#endif  // H323_VIDEO

#ifdef H323_T38

    /**See if should auto-start receive fax channels on connection.
     */
    PBoolean CanAutoStartReceiveFax() const { return autoStartReceiveFax; }

    /**See if should auto-start transmit fax channels on connection.
     */
    PBoolean CanAutoStartTransmitFax() const { return autoStartTransmitFax; }

#endif // H323_T38

    /**See if should automatically do call forward of connection.
     */
    PBoolean CanAutoCallForward() const { return autoCallForward; }

    /**Get the set of listeners (incoming call transports) for this endpoint.
     */
    const H323ListenerList & GetListeners() const { return listeners; }

    /**Get the current capability table for this endpoint.
     */
    const H323Capabilities & GetCapabilities() const { return capabilities; }

    /**Endpoint types.
     */
    enum TerminalTypes {
      e_TerminalOnly = 50,
      e_TerminalAndMC = 70,
      e_GatewayOnly = 60,
      e_GatewayAndMC = 80,
      e_GatewayAndMCWithDataMP = 90,
      e_GatewayAndMCWithAudioMP = 100,
      e_GatewayAndMCWithAVMP = 110,
      e_GatekeeperOnly = 120,
      e_GatekeeperWithDataMP = 130,
      e_GatekeeperWithAudioMP = 140,
      e_GatekeeperWithAVMP = 150,
      e_MCUOnly = 160,
      e_MCUWithDataMP = 170,
      e_MCUWithAudioMP = 180,
      e_MCUWithAVMP = 190,
#ifdef H323_H461
      e_SET_H461 = 240
#endif
    };

    /**Get the endpoint terminal type.
     */
    TerminalTypes GetTerminalType() const { return terminalType; }

    /**Set the endpoint terminal type.
    */
    void SetTerminalType(TerminalTypes type);

    /**Determine if endpoint is terminal type.
     */
    PBoolean IsTerminal() const;

    /**Determine if SET is terminal type.
     */
    PBoolean IsSimpleEndPoint() const;

    /**Determine if endpoint is gateway type.
     */
    PBoolean IsGateway() const;

    /**Determine if endpoint is gatekeeper type.
     */
    PBoolean IsGatekeeper() const;

    /**Determine if endpoint is gatekeeper type.
     */
    PBoolean IsMCU() const;

#ifdef H323_AUDIO_CODECS
    /**Get the default maximum audio jitter delay parameter.
       Defaults to 50ms
     */
    unsigned GetMinAudioJitterDelay() const { return minAudioJitterDelay; }

    /**Get the default maximum audio delay jitter parameter.
       Defaults to 250ms.
     */
    unsigned GetMaxAudioJitterDelay() const { return maxAudioJitterDelay; }

    /**Set the maximum audio delay jitter parameter.
     */
    void SetAudioJitterDelay(
      unsigned minDelay,   ///< New minimum jitter buffer delay in milliseconds
      unsigned maxDelay    ///< New maximum jitter buffer delay in milliseconds
    );
#endif

    /**Get the initial bandwidth parameter.
     */
    unsigned GetInitialBandwidth() const { return initialBandwidth; }

    /**Get the initial bandwidth parameter.
     */
    void SetInitialBandwidth(unsigned bandwidth) { initialBandwidth = bandwidth; }

    /**Set the BitRate to appear in the BearerCapabilities for Setup messages
     */
    virtual void OnBearerCapabilityTransferRate(unsigned & bitRate)
        { if (initialBandwidth > bitRate) bitRate = initialBandwidth; }

#ifdef H323_VIDEO
    virtual void OnSetInitialBandwidth(H323VideoCodec * /*codec*/) {};
#endif

    /**Called when an outgoing PDU requires a feature set
     */
    virtual PBoolean OnSendFeatureSet(unsigned, H225_FeatureSet &, PBoolean);

    /**Called when an incoming PDU contains a feature set
     */
    virtual void OnReceiveFeatureSet(unsigned, const H225_FeatureSet &, PBoolean = false);

#ifdef H323_H460
    /**Get the complete list of Gatekeeper features
      */
    H460_FeatureSet * GetGatekeeperFeatures();
#endif

    /**Load the Base FeatureSet usually called when you initialise the endpoint prior to
       registering with a gatekeeper.
      */
    virtual void LoadBaseFeatureSet();

    /**Callback when creating Feature Instance. This can be used to disable features on
       a case by case basis by returning FALSE
       Default returns TRUE
      */
    virtual PBoolean OnFeatureInstance(int instType, const PString & identifer);

    /**Handle Unsolicited Information PDU received on the signal listening socket not
       associated with a connection.
     */
    virtual PBoolean HandleUnsolicitedInformation(const H323SignalPDU & pdu);

#ifdef H323_H460
    /** Get the Endpoint FeatureSet
        This creates a new instance of the featureSet
     */
    H460_FeatureSet * GetFeatureSet() {  return features.DeriveNewFeatureSet(); };

    /** Is the FeatureSet disabled
      */
    PBoolean FeatureSetDisabled()  {  return disableH460; }

    /** Disable all FeatureSets. Use this for pre H323v4 interoperability
      */
    void FeatureSetDisable()  {  disableH460 = TRUE;  }

    /** Feature Callback
      */
    virtual void FeatureCallBack(const PString & FeatID,        ///< Feature Identifier
                                PINDEX msgID,                    ///< Message Identifer
                                const PString & message            ///< Message
                                ) {};

#ifdef H323_H46017

    /** Create a H.460.17 TCP connection
        to the gatekeeper to tunnel RAS messages through
      */
    PBoolean H46017CreateConnection(
                               const PString & gatekeeper,   ///< Gatekeeper IP/URL (incl port if not SRV) to connect to
                               PBoolean useSRV = true        ///< Whether to use h323rs._tcp SRV records to resolve
                               );

    PBoolean RegisteredWithH46017() const { return m_registeredWithH46017; }
    PBoolean TryingWithH46017() const { return m_tryingH46017; }
    H323Transport * GetH46017Transport() { return m_h46017Transport; }
#endif

#ifdef H323_H46018

    /** Disable H.460.18 Feature. (By Default it is enabled)
      */
    void H46018Enable(PBoolean enable);

    /** Query whether we are using H.460.18
      */
    PBoolean H46018IsEnabled();

    /** Signal that H.460.18 has been received. ie. We are behind a NAT/FW
      */
    void H46018Received() {};

    /** Whether H.460.18 is in Operation for this call
      */
    PBoolean H46018InOperation();
#endif

#ifdef H323_H46019M
    /** Disable H.460.19 Multiplex Feature. (By Default it is enabled)
      */
    void H46019MEnable(PBoolean enable);

    /** Query whether we are using H.460.19 Multiplexing
      */
    PBoolean H46019MIsEnabled();

    /** Enable H.460.19 Multiplex Send. (By Default it is disabled)
      */
    void H46019MSending(PBoolean enable);

    /** Query whether we are using H.460.19 Multiplex Sending (H.460.19M Must be enabled)
      */
    PBoolean H46019MIsSending();
#endif

#ifdef H323_H46023
    /** Disable H.460.23 Feature. (By Default it is enabled)
      */
    void H46023Enable(PBoolean enable);

    /** Query whether we are using H.460.23
      */
    PBoolean H46023IsEnabled();

    /** Based on the available information select the appropriate NAT Method.
        if H.460.17 for signalling and H.460.23 NAT test == blocked then H.460.26 (tunnel in signalling)
        if H.460.17 for signalling and H.460.23 NAT test == blocked but Alternate (UPnP) then H.460.24 (directMedia)
    `   If H.460.17 for signalling and H.460.23 NAT test <> blocked then H.460.19 (classic H.460.18/.19)

        if H.460.18 for signalling and H.460.23 NAT test <> blocked then H.460.24 available
        if H.460.18 for signalling and H.460.23 NAT test == blocked then Media failure!

        Override to change implementors logic
      */
    virtual PBoolean H46023NatMethodSelection(const PString & method);
#endif

#ifdef H323_H46025
    /** Disable H.460.25 Feature. (By Default it is disabled)
      */
    void H46025Enable(PBoolean enable);

    /** Query whether we are using H.460.25
      */
    PBoolean H46025IsEnabled();

    /** Get Device Information
      */
    virtual bool H46025DeviceInformation(H323_H46025_Message::Device & device);

    /** Get Civic Information
      */
    virtual bool H46025CivicInformation(H323_H46025_Message::Civic & civic);

    /** Get GPS Information
      */
    virtual bool H46025GPSInformation(H323_H46025_Message::Geodetic & gps);

#endif

#ifdef H323_H46026
    /** Disable H.460.26 Feature. (By Default it is enabled)
      */
    void H46026Enable(PBoolean enable) { m_h46026enabled = enable; }

    /** Query whether we are using H.460.26
      */
    PBoolean H46026IsEnabled() const { return m_h46026enabled; }
#endif

#ifdef H323_H460IM

    /** Main function to enable the feature
     */
    void EnableIM() { m_IMenabled = true; };

    /** Callback from the H460 feature to check if the
        feature is enabled
      */
    PBoolean IMisDisabled() { return !m_IMenabled; };

	/** Internal Flag to indicate the current call is an IM call
      */
    PBoolean IsIMCall() { return m_IMcall;  }

	/** Internal Flag to set the current call as an IM call
      */
    void SetIMCall(PBoolean state) { m_IMcall = state; }

    /** Main call to open IM session
      */
    virtual PBoolean IMMakeCall(const PString & number,
        PBoolean session,  // set to true if persistent
        PString & token,
        const PString & msg = PString());

	/** Send a message
      */
    virtual void IMSend(const PString & msg);

    /** An IM Message has been received
      */
    virtual void IMReceived(const PString & token, const PString & msg, PBoolean session = TRUE);


    virtual void IMWrite(PBoolean start);
    virtual void IMCloseSession();

    // Internal
    virtual void IMWriteFacility(H323Connection * connection);
    virtual void IMOpenSession(const PString & token);
    virtual void IMClearConnection(const PString & token);
    virtual void IMSupport(const PString & token);
    virtual void IMSessionInvite(const PString & username);

    // Call backs
    virtual PBoolean IMWriteEvent(PBoolean & state);

    // Events
    virtual void IMSessionDetails(const PString & token,
        const PString & number,
        const PString & CallerID,
        const PString & enc
        );

    virtual void IMSessionOpen(const PString & token);
    virtual void IMSessionClosed(const PString & token);
    virtual void IMSessionWrite(const PString & token, PBoolean state);
    virtual void IMSessionError(const PString & token, int reason = 0);


    /** An alert a message has been successfully sent */
    virtual void IMSent(const PString & token, PBoolean success, int reason = 0);

    // Events to pass out.
    enum {
        uiIMIdle = 0,
        uiIMOpen = 1,
        uiIMClose = 2,
        uiIMStartWrite = 3,
        uiIMEndWrite = 4,
        uiIMQuick = 5
    } uiIMstate;

    // Events to the user
    virtual void OnIMSessionState(const PString & session, const short & state) {};
    virtual void OnIMReceived(const PString & session, const PString & message) {};
    virtual void OnIMSent(const PString & session, PBoolean success, const short & errcode) {};
    virtual void OnIMSessionDetails(const PString & token, const PString & number,
        const PString & CallerID, const PString & enc) {};
    virtual void OnIMSessionError(const PString & token, const short & reason) {};

#endif

#ifdef H323_H460P

    /** Get the presence handler. By default it returns NULL
        Implementor must create an instance of the presencehandler
        to enable presence
      */
    H460PresenceHandler * GetPresenceHandler()  { return presenceHandler; }

	enum presenceStates {
        e_preHidden,
        e_preAvailable,
        e_preOnline,
        e_preOffline,
        e_preOnCall,
        e_preVoiceMail,
        e_preNotAvailable,
        e_preAway,
        e_preGeneric
	};

    /** Set the local Presence State.
        Calling this will enable Presence in the endpoint
      */
    void PresenceSetLocalState(const PStringList & alias, presenceStates localstate, const PString & localdisplay = PString(), PBoolean updateOnly = false);

    enum presenceInstruction {
        e_subscribe,
        e_unsubscribe,
        e_block,
        e_unblock
    };

    enum presenceFeature {
        e_preAudio,
        e_preVideo,
        e_preExtVideo,
        e_preData
    };

    struct presenceLocale {
        PString        m_locale;
        PString        m_region;
        PString        m_country;
        PString        m_countryCode;
        PString        m_latitude;
        PString        m_longitude;
        PString        m_elevation;
    };

    void PresenceAddFeature(presenceFeature feat);

    void PresenceAddFeatureH460();

    void PresenceSetLocale(const presenceLocale & info);

    /** Set Presence Instructions.
      */
    void PresenceSetInstruction(const PString & epalias,
                                unsigned type,
                                const PString & alias,
                                const PString & display);

    void PresenceSetInstruction(const PString & epalias,
                                unsigned type,
                                const PStringList & list,
                                 PBoolean autoSend = true);

    /** Submit Presence Authorizations.
      */
    void PresenceSendAuthorization(const OpalGloballyUniqueID & id,
                                    const PString & epalias,
                                    PBoolean approved,
                                    const PStringList & subscribe);

    /** Received Notifications
      */
    virtual void PresenceNotification(const PString & locAlias,
                                    const PString & subAlias,
                                    unsigned state,
                                    const PString & display);

    /** Received Instructions
      */
    virtual void PresenceInstruction(const PString & locAlias,
                                    unsigned type,
                                    const PString & subAlias,
                                    const PString & subDisplay);

    virtual void PresenceInstruction(const PString & locAlias,
                                    unsigned type,
                                    const PString & subAlias,
                                    const PString & subDisplay,
                                    const PString & subAvatar
                                    );

    virtual void PresenceInstruction(const PString & locAlias,
                                    unsigned type,
                                    const PString & subAlias,
                                    const PString & subDisplay,
                                    const PString & subAvatar,
                                    unsigned category
                                    );

    /** Received Request for authorization
      */
    virtual void PresenceAuthorization(const OpalGloballyUniqueID & id,
                                    const PString & locAlias,
                                    const std::map<PString,PresSubDetails> & Aliases);
#endif

#ifdef H323_H460PRE

    /** Get the registration priority
      */
    unsigned GetRegistrationPriority();

    /** Set the registration priority from 0-9 default 0
      */
    void SetRegistrationPriority(unsigned value);

    /** Get Pre-emption status
      */
    PBoolean GetPreempt();

    /** Set to Preempt other registration
      */
    void SetPreempt(PBoolean topreempt);

    /** Set this registration to be Preempted
      */
    void SetPreempted(PBoolean ispreempted);

    /** Is this registration Preempted
      */
    PBoolean IsPreempted();

    /**Prempt the previous registration
      */
    void PreemptRegistration();

    /** Notification of the local registration being PreEmpted.
        Note this does not stop the gatekeeper periodically retrying to register
        Once the other device has deregistered then registration will restored.
      */
    virtual void OnNotifyPreempt(PBoolean unregister);

    /** Notification of Priority.
        This provides an opportunity to preEmpt the previous
        registration. CAll PreemptRegistration() to preEmpt.
      */
    virtual void OnNotifyPriority();


#endif  // H323_H460PRE

#ifdef H323_H461
    void SetASSETEnabled(PBoolean success);
    PBoolean IsASSETEnabled();

    enum H461Mode {
        e_H461Disabled,
        e_H461EndPoint,
        e_H461ASSET
    };

    virtual void SetEndPointASSETMode(H461Mode mode);
    virtual H461Mode GetEndPointASSETMode();

    H461DataStore * GetASSETDataStore();
    void SetASSETDataStore(H461DataStore * dataStore);
#endif  // H323_H461

#endif


#ifdef H323_AEC
    PBoolean AECEnabled()   {  return enableAEC; }

    void SetAECEnabled(PBoolean enabled)  { enableAEC = enabled; }
#endif

#ifdef H323_TLS
    void EnableIPSec(PBoolean enable);

    PBoolean TLS_SetCAFile(const PFilePath & caFile);
    PBoolean TLS_SetCADirectory(const PDirectory & dir);
    PBoolean TLS_AddCACertificate(const PString & caData);
    PBoolean TLS_SetCertificate(const PFilePath & certDir);
    PBoolean TLS_SetPrivateKey(const PFilePath & privFile, const PString & password);
    PBoolean TLS_SetCipherList(const PString & ciphers);
    PBoolean TLS_SetDHParameters(const PFilePath & pkcs3);
    PBoolean TLS_SetDHParameters(const PBYTEArray & dh_p, const PBYTEArray & dh_g);
    PBoolean TLS_Initialise(const PIPSocket::Address & binding = PIPSocket::GetDefaultIpAny(),
                            WORD port = DefaultTLSPort);

    PBoolean InitialiseTransportContext();
    PSSLContext * GetTransportContext();

    virtual void OnSecureSignallingChannel(bool /* isSecured */) {};
#endif

    PBoolean IsTLSEnabled();
    PBoolean IsIPSecEnabled();
    void SetTLSMediaPolicy(H323TransportSecurity::Policy policy);
    H323TransportSecurity * GetTransportSecurity();

#ifdef H323_FRAMEBUFFER
    void EnableVideoFrameBuffer(PBoolean enable);
    PBoolean HasVideoFrameBuffer();
#endif

#ifdef H323_UPnP
    /**Set UPnP
      */
    void SetUPnP(PBoolean active);

    /**Initialise UPnP
      */
    PBoolean InitialiseUPnP();

    /**On UPnP detected and tested.
       return true to make available.
      */
    virtual PBoolean OnUPnPAvailable(const PString & device,
                             const PIPSocket::Address & publicIP,
                             PNatMethod_UPnP * nat);
#endif

#ifdef P_STUN

    /**Return the STUN server to use.
       Returns NULL if address is a local address as per IsLocalAddress().
       Always returns the STUN server if address is zero.
       Note, the pointer is NOT to be deleted by the user.
      */
    PSTUNClient * GetSTUN(
      const PIPSocket::Address & address = 0
    ) const;

    /**Set the STUN server address, is of the form host[:port]
      */
    void SetSTUNServer(
      const PString & server
    );

    /**Type of NAT detected (if available) when initialing STUN Client
      */
    virtual PBoolean STUNNatType(int /*type*/) { return FALSE; };

    /** Retrieve the first available
        NAT Traversal Techniques
     */
    PNatMethod * GetPreferedNatMethod(
        const PIPSocket::Address & address = 0
    );

    /** Get the Nat Methods List
       */
    H323NatStrategy & GetNatMethods() const;

    virtual void NATMethodCallBack(const PString & /*NatID*/,    ///< Method Identifier
                                PINDEX /*msgID*/,                ///< Message Identifer
                                const PString & /*message*/        ///< Message
                                ) {};

#endif // P_STUN

    virtual PBoolean OnUnsolicitedInformation(const H323SignalPDU & /*pdu*/)
    { return FALSE; }

    /**Determine if the address is "local", ie does not need STUN
     */
    virtual PBoolean IsLocalAddress(
      const PIPSocket::Address & remoteAddress
    ) const;

    /**Provide TCP address translation hook
     */
    virtual void TranslateTCPAddress(
      PIPSocket::Address & /*localAddr*/,
      const PIPSocket::Address & /*remoteAddr */
    ) { }
    void InternalTranslateTCPAddress(
      PIPSocket::Address & /*localAddr*/,
      const PIPSocket::Address & /*remoteAddr */,
      const H323Connection * conn = NULL
    );

    /**Provide TCP Port translation hook
     */
    virtual void TranslateTCPPort(
        WORD & /*ListenPort*/,                     ///* Local listening port
        const PIPSocket::Address & /*remoteAddr*/  ///* Remote address
    ) { };

    /**Get the TCP port number base for H.245 channels
     */
    WORD GetTCPPortBase() const { return tcpPorts.base; }

    /**Get the TCP port number base for H.245 channels.
     */
    WORD GetTCPPortMax() const { return tcpPorts.max; }

    /**Set the TCP port number base and max for H.245 channels.
     */
    void SetTCPPorts(unsigned tcpBase, unsigned tcpMax);

    /**Get the next TCP port number for H.245 channels
     */
    WORD GetNextTCPPort();

    /**Get the UDP port number base for RAS channels
     */
    WORD GetUDPPortBase() const { return udpPorts.base; }

    /**Get the UDP port number base for RAS channels.
     */
    WORD GetUDPPortMax() const { return udpPorts.max; }

    /**Set the UDP port number base and max for RAS channels.
     */
    void SetUDPPorts(unsigned udpBase, unsigned udpMax);

    /**Get the next UDP port number for RAS channels
     */
    WORD GetNextUDPPort();

    /**Get the UDP port number base for RTP channels.
     */
    WORD GetRtpIpPortBase() const { return rtpIpPorts.base; }

    /**Get the max UDP port number for RTP channels.
     */
    WORD GetRtpIpPortMax() const { return rtpIpPorts.max; }

    /**Set the UDP port number base and max for RTP channels.
     */
    void SetRtpIpPorts(unsigned udpBase, unsigned udpMax);

    /**Get the UDP port number pair for RTP channels.
     */
    WORD GetRtpIpPortPair();

#ifdef H323_H46019M
   /**Set the UDP port number base for Multiplex RTP/RTCP channels.
     */
    void SetMultiplexPort(unsigned rtpPort);

   /**Get the UDP port number base for Multiplex RTP/RTCP channels.
     */
    WORD GetMultiplexPort();

   /**Get next Multiplex RTP/RTCP channel ID.
      Each call indexes the counter by 1;
     */
    unsigned GetMultiplexID();
#endif

    /**Get the IP Type Of Service byte for RTP channels.
     */
    BYTE GetRtpIpTypeofService() const { return rtpIpTypeofService; }

    /**Set the IP Type Of Service byte for RTP channels.
     */
    void SetRtpIpTypeofService(unsigned tos) { rtpIpTypeofService = (BYTE)tos; }

    /**Get the IP Type Of Service byte for TCP channels.
     */
    BYTE GetTcpIpTypeofService() const { return tcpIpTypeofService; }

    /**Set the IP Type Of Service byte for TCP channels.
     */
    void SetTcpIpTypeofService(unsigned tos) { tcpIpTypeofService = (BYTE)tos; }

    /** Get the default timeout for connecting via TCP
      */
    const PTimeInterval & GetSignallingChannelConnectTimeout() const { return signallingChannelConnectTimeout; }

    /**Get the default timeout for calling another endpoint.
     */
    const PTimeInterval & GetSignallingChannelCallTimeout() const { return signallingChannelCallTimeout; }

    /**Get the default timeout for incoming H.245 connection.
     */
    const PTimeInterval & GetControlChannelStartTimeout() const { return controlChannelStartTimeout; }

    /**Get the default timeout for waiting on an end session.
     */
    const PTimeInterval & GetEndSessionTimeout() const { return endSessionTimeout; }

    /**Get the default timeout for master slave negotiations.
     */
    const PTimeInterval & GetMasterSlaveDeterminationTimeout() const { return masterSlaveDeterminationTimeout; }

    /**Get the default retries for H245 master slave negotiations.
     */
    unsigned GetMasterSlaveDeterminationRetries() const { return masterSlaveDeterminationRetries; }

    /**Get the default timeout for H245 capability exchange negotiations.
     */
    const PTimeInterval & GetCapabilityExchangeTimeout() const { return capabilityExchangeTimeout; }

    /**Get the default timeout for H245 logical channel negotiations.
     */
    const PTimeInterval & GetLogicalChannelTimeout() const { return logicalChannelTimeout; }

    /**Get the default timeout for H245 request mode negotiations.
     */
    const PTimeInterval & GetRequestModeTimeout() const { return logicalChannelTimeout; }

    /**Get the default timeout for H245 round trip delay negotiations.
     */
    const PTimeInterval & GetRoundTripDelayTimeout() const { return roundTripDelayTimeout; }

    /**Get the default rate H245 round trip delay is calculated by connection.
     */
    const PTimeInterval & GetRoundTripDelayRate() const { return roundTripDelayRate; }

    /**Get the flag for clearing a call if the round trip delay calculation fails.
     */
    PBoolean ShouldClearCallOnRoundTripFail() const { return clearCallOnRoundTripFail; }

    /**Get the amount of time with no media that should cause call to clear
     */
    const PTimeInterval & GetNoMediaTimeout() const;

    /**Set the amount of time with no media that should cause call to clear
     */
    PBoolean SetNoMediaTimeout(PTimeInterval newInterval);

    /**Get the default timeout for GatekeeperRequest and Gatekeeper discovery.
     */
    const PTimeInterval & GetGatekeeperRequestTimeout() const { return gatekeeperRequestTimeout; }

    /**Get the default retries for GatekeeperRequest and Gatekeeper discovery.
     */
    unsigned GetGatekeeperRequestRetries() const { return gatekeeperRequestRetries; }

    /**Get the default timeout for RAS protocol transactions.
     */
    const PTimeInterval & GetRasRequestTimeout() const { return rasRequestTimeout; }

    /**Get the default retries for RAS protocol transations.
     */
    unsigned GetRasRequestRetries() const { return rasRequestRetries; }

    /**Get the default time for gatekeeper to reregister.
       A value of zero disables the keep alive facility.
     */
    const PTimeInterval & GetGatekeeperTimeToLive() const { return registrationTimeToLive; }

    /**Get the iNow Gatekeeper Access Token OID.
     */
    const PString & GetGkAccessTokenOID() const { return gkAccessTokenOID; }

    /**Set the iNow Gatekeeper Access Token OID.
     */
    void SetGkAccessTokenOID(const PString & token) { gkAccessTokenOID = token; }

    /**Get flag to indicate whether to send GRQ on gatekeeper registration
     */
    PBoolean GetSendGRQ() const
    { return sendGRQ; }

    /**Sent flag to indicate whether to send GRQ on gatekeeper registration
     */
    void SetSendGRQ(PBoolean v)
    { sendGRQ = v; }

#ifdef H323_H450

    /**Get the default timeout for Call Transfer Timer CT-T1.
     */
    const PTimeInterval & GetCallTransferT1() const { return callTransferT1; }

    /**Get the default timeout for Call Transfer Timer CT-T2.
     */
    const PTimeInterval & GetCallTransferT2() const { return callTransferT2; }

    /**Get the default timeout for Call Transfer Timer CT-T3.
     */
    const PTimeInterval & GetCallTransferT3() const { return callTransferT3; }

    /**Get the default timeout for Call Transfer Timer CT-T4.
     */
    const PTimeInterval & GetCallTransferT4() const { return callTransferT4; }

    /** Get Call Intrusion timers timeout */
    const PTimeInterval & GetCallIntrusionT1() const { return callIntrusionT1; }
    const PTimeInterval & GetCallIntrusionT2() const { return callIntrusionT2; }
    const PTimeInterval & GetCallIntrusionT3() const { return callIntrusionT3; }
    const PTimeInterval & GetCallIntrusionT4() const { return callIntrusionT4; }
    const PTimeInterval & GetCallIntrusionT5() const { return callIntrusionT5; }
    const PTimeInterval & GetCallIntrusionT6() const { return callIntrusionT6; }

    /**Get the dictionary of <callIdentities, connections>
     */
    H323CallIdentityDict& GetCallIdentityDictionary() { return secondaryConnectionsActive; }

    /**Get the next available invoke Id for H450 operations
      */
    unsigned GetNextH450CallIdentityValue() const { return ++nextH450CallIdentity; }

#endif // H323_H450

    /**Get the default stack size of cleaner threads.
     */
    PINDEX GetCleanerThreadStackSize() const { return cleanerThreadStackSize; }

    /**Get the default stack size of listener threads.
     */
    PINDEX GetListenerThreadStackSize() const { return listenerThreadStackSize; }

    /**Get the default stack size of signalling channel threads.
     */
    PINDEX GetSignallingThreadStackSize() const { return signallingThreadStackSize; }

    /**Get the default stack size of control channel threads.
     */
    PINDEX GetControlThreadStackSize() const { return controlThreadStackSize; }

    /**Get the default stack size of logical channel threads.
     */
    PINDEX GetChannelThreadStackSize() const { return logicalThreadStackSize; }

    /**Get the default stack size of RAS channel threads.
     */
    PINDEX GetRasThreadStackSize() const { return rasThreadStackSize; }

    /**Get the default stack size of jitter buffer threads.
     */
    PBoolean UseJitterBuffer() const { return useJitterBuffer; }

    /**Get the default stack size of jitter buffer threads.
     */
    PINDEX GetJitterThreadStackSize() const { return jitterThreadStackSize; }

    /** Get the priority at which channel threads run
      */
    PThread::Priority GetChannelThreadPriority() const { return channelThreadPriority; }

    H323ConnectionDict & GetConnections() { return connectionsActive; };

    PBoolean EnableH225KeepAlive() const { return m_useH225KeepAlive; }
    PBoolean EnableH245KeepAlive() const { return m_useH245KeepAlive; }

    virtual PBoolean GetDefaultLanguages(const PStringList & /*languages*/)  { return false; }
    virtual void OnReceiveLanguages(const PStringList & /*languages*/)  { }

  //@}

    /**
     * default settings H.221 settings
     */
    static BYTE defaultT35CountryCode;
    static BYTE defaultT35Extension;
    static WORD defaultManufacturerCode;

  protected:
    H323Gatekeeper * InternalCreateGatekeeper(H323Transport * transport);
    PBoolean InternalRegisterGatekeeper(H323Gatekeeper * gk, PBoolean discovered);
    H323Connection * FindConnectionWithoutLocks(const PString & token);
    virtual H323Connection * InternalMakeCall(
      const PString & existingToken, /// Existing connection to be transferred
      const PString & callIdentity,  /// Call identity of the secondary call (if it exists)
      unsigned capabilityLevel,      /// Intrusion capability level
      const PString & remoteParty,   /// Remote party to call
      H323Transport * transport,     /// Transport to use for call.
      PString & token,               /// String to use/receive token for connection
      void * userData,               /// user data to pass to CreateConnection
      PBoolean supplementary = false ///< Whether the call is a supplementary call
    );

    // Configuration variables, commonly changed
    PStringList localAliasNames;

	PStringList localLanguages;
    PBoolean    useQ931Display;

#ifdef H323_AUDIO_CODECS
    H323AudioCodec::SilenceDetectionMode defaultSilenceDetection;
    unsigned minAudioJitterDelay;
    unsigned maxAudioJitterDelay;
#ifdef P_AUDIO
    PString     soundChannelPlayDevice;
    PString     soundChannelPlayDriver;
    PString     soundChannelRecordDevice;
    PString     soundChannelRecordDriver;
    unsigned    soundChannelBuffers;
#endif // P_AUDIO
    PBoolean    autoStartReceiveAudio;
    PBoolean    autoStartTransmitAudio;
#endif // H323_AUDIO_CODECS

#ifdef H323_VIDEO
    PString     videoChannelPlayDevice;
    PString     videoChannelRecordDevice;
    PBoolean        autoStartReceiveVideo;
    PBoolean        autoStartTransmitVideo;

#ifdef H323_H239
    PBoolean        autoStartReceiveExtVideo;
    PBoolean        autoStartTransmitExtVideo;
#endif // H323_H239
#endif // H323_VIDEO

#ifdef H323_T38
    PBoolean        autoStartReceiveFax;
    PBoolean        autoStartTransmitFax;
#endif // H323_T38

    PBoolean        autoCallForward;
    PBoolean        disableFastStart;
    PBoolean        disableH245Tunneling;
    PBoolean        disableH245inSetup;
    PBoolean        disableH245QoS;
    PBoolean        disableDetectInBandDTMF;
    PBoolean        disableRFC2833InBandDTMF;
    PBoolean        disableExtendedUserInput;
    PBoolean        canDisplayAmountString;
    PBoolean        canEnforceDurationLimit;

#ifdef H323_H450
    unsigned    callIntrusionProtectionLevel;
    PString     mwiMsgCentre;
#endif // H323_H450

    H323Connection::SendUserInputModes defaultSendUserInputMode;

#ifdef P_LDAP
    PString     ilsServer;
#endif // P_LDAP

    // Some more configuration variables, rarely changed.
    BYTE          rtpIpTypeofService;
    BYTE          tcpIpTypeofService;
    PTimeInterval signallingChannelConnectTimeout;
    PTimeInterval signallingChannelCallTimeout;
    PTimeInterval controlChannelStartTimeout;
    PTimeInterval endSessionTimeout;
    PTimeInterval masterSlaveDeterminationTimeout;
    unsigned      masterSlaveDeterminationRetries;
    PTimeInterval capabilityExchangeTimeout;
    PTimeInterval logicalChannelTimeout;
    PTimeInterval requestModeTimeout;
    PTimeInterval roundTripDelayTimeout;
    PTimeInterval roundTripDelayRate;
    PTimeInterval noMediaTimeout;
    PTimeInterval gatekeeperRequestTimeout;
    unsigned      gatekeeperRequestRetries;
    PTimeInterval rasRequestTimeout;
    unsigned      rasRequestRetries;
    PTimeInterval registrationTimeToLive;
    PString       gkAccessTokenOID;
    PBoolean          sendGRQ;

    unsigned initialBandwidth;  // in 100s of bits/sev
    PBoolean     clearCallOnRoundTripFail;

    struct PortInfo {
      void Set(
        unsigned base,
        unsigned max,
        unsigned range,
        unsigned dflt
      );
      WORD GetNext(
        unsigned increment
      );

      PMutex mutex;
      WORD   base;
      WORD   max;
      WORD   current;
    } tcpPorts, udpPorts, rtpIpPorts;

#ifdef P_STUN
    H323NatStrategy * natMethods;
#endif

#ifdef H323_H46019M
    struct MuxIDInfo {
       PMutex mutex;
         unsigned   base;
         unsigned   max;
         unsigned   current;
       unsigned GetNext(
          unsigned increment
       );
    } rtpMuxID;
    WORD defaultMultiRTPPort;
#endif

    BYTE t35CountryCode;
    BYTE t35Extension;
    WORD manufacturerCode;

    TerminalTypes terminalType;
    PBoolean rewriteParsePartyName;

#ifdef H323_H450

    /* Protect against absence of a response to the ctIdentify reqest
       (Transferring Endpoint - Call Transfer with a secondary Call) */
    PTimeInterval callTransferT1;
    /* Protect against failure of completion of the call transfer operation
       involving a secondary Call (Transferred-to Endpoint) */
    PTimeInterval callTransferT2;
    /* Protect against failure of the Transferred Endpoint not responding
       within sufficient time to the ctInitiate APDU (Transferring Endpoint) */
    PTimeInterval callTransferT3;
    /* May optionally operate - protects against absence of a response to the
       ctSetup request (Transferred Endpoint) */
    PTimeInterval callTransferT4;

    /** Call Intrusion Timers */
    PTimeInterval callIntrusionT1;
    PTimeInterval callIntrusionT2;
    PTimeInterval callIntrusionT3;
    PTimeInterval callIntrusionT4;
    PTimeInterval callIntrusionT5;
    PTimeInterval callIntrusionT6;

    H323CallIdentityDict secondaryConnectionsActive;

    mutable PAtomicInteger nextH450CallIdentity;
            /// Next available callIdentity for H450 Transfer operations via consultation.

#endif // H323_H450

    PINDEX cleanerThreadStackSize;
    PINDEX listenerThreadStackSize;
    PINDEX signallingThreadStackSize;
    PINDEX controlThreadStackSize;
    PINDEX logicalThreadStackSize;
    PINDEX rasThreadStackSize;
    PINDEX jitterThreadStackSize;
    PBoolean useJitterBuffer;

#ifdef H323_RTP_AGGREGATE
    PINDEX rtpAggregationSize;
    PHandleAggregator * rtpAggregator;
#endif

#ifdef H323_SIGNAL_AGGREGATE
    PINDEX signallingAggregationSize;
    PHandleAggregator * signallingAggregator;
#endif

    PThread::Priority channelThreadPriority;

    // Dynamic variables
    H323ListenerList listeners;
    H323Capabilities capabilities;
    H323Gatekeeper * gatekeeper;
    PString          gatekeeperPassword;
    PStringList      gkAuthenticatorOrder;

    H323ConnectionDict       connectionsActive;

    PMutex                   connectionsMutex;
    PMutex                   noMediaMutex;
    PStringSet               connectionsToBeCleaned;
    H323ConnectionsCleaner * connectionsCleaner;
    PSyncPoint               connectionsAreCleaned;

    // Call Authentication
    PString EPSecurityUserName;       /// Local UserName Authenticated Call
    PString EPSecurityPassword;       /// Local Password Authenticated Call
    PBoolean isSecureCall;               /// Flag to Specify Call to make is Authenticated.
    EPSecurityPolicy CallAuthPolicy;   /// Incoming Call Authentication acceptance level
    H235AuthenticatorList EPAuthList;  /// List of Usernames & Password to check incoming call Against
    PBoolean m_disableMD5Authenticators; /// Disable MD5 based authenticatos (MD5 + CAT)

#ifdef H323_H460
    H460_FeatureSet features;
    PBoolean disableH460;

#ifdef H323_H46017
    PBoolean m_tryingH46017;
    PBoolean m_registeredWithH46017;
    H323Transport * m_h46017Transport;
#endif

#ifdef H323_H46018
    PBoolean m_h46018enabled;
#endif

#ifdef H323_H46019M
    PBoolean m_h46019Menabled;
    PBoolean m_h46019Msend;
#endif

#ifdef H323_H46023
    PBoolean m_h46023enabled;
#endif

#ifdef H323_H46023
    PBoolean m_h46025enabled;
#endif

#ifdef H323_H46026
    PBoolean m_h46026enabled;
#endif

#ifdef H323_UPnP
    PBoolean m_UPnPenabled;
#endif

#ifdef H323_H460IM
    PBoolean m_IMenabled;
    PBoolean m_IMcall;
    PBoolean m_IMsession;
    PBoolean m_IMwriteevent;
    PString m_IMmsg;
    PStringList m_IMsessions;
    PMutex m_IMmutex;

    // Multipoint Text functions
    //short m_IMMultiMode;
    //PBoolean m_IMmodeSent;
#endif

#ifdef H323_H460P
    H460PresenceHandler * presenceHandler;
#endif

#ifdef H323_H460PRE
	unsigned m_regPrior;
	PBoolean m_preempt;
    PBoolean m_preempted;
#endif

#ifdef H323_H461
    PBoolean m_ASSETEnabled;
    H323EndPoint::H461Mode m_h461ASSETMode;
    H461DataStore * m_h461DataStore;
#endif

#endif

#ifdef H323_AEC
    PBoolean enableAEC;
#endif

#ifdef H323_GNUGK
    GNUGK_Feature * gnugk;
#endif

#ifdef H323_FRAMEBUFFER
    PBoolean useVideoBuffer;
#endif

    H323TransportSecurity m_transportSecurity;
#ifdef H323_TLS
    PSSLContext * m_transportContext;
#endif

    void RegInvokeReRegistration();
    PMutex reregmutex;
    PThread  *  RegThread;
    PDECLARE_NOTIFIER(PThread, H323EndPoint, RegMethod);

    PBoolean m_useH225KeepAlive;
    PBoolean m_useH245KeepAlive;

};

/////////////////////////////////////////////////////////////////////


#endif // __OPAL_H323EP_H


/////////////////////////////////////////////////////////////////////////////
