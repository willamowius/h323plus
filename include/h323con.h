/*
 * h323con.h
 *
 * H.323 protocol handler
 *
 * H323plus Library
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

#ifndef __OPAL_H323CON_H
#define __OPAL_H323CON_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#ifdef P_DTMF
#include <ptclib/dtmf.h>
#endif

#include "openh323buildopts.h"

#include "h235auth.h"
#ifdef H323_H235
#include "h235/h235caps.h"
#else
#include "h323caps.h"
#endif
#include "transports.h"
#include "channels.h"
#include "guid.h"

#include "h225.h"

#ifdef H323_SIGNAL_AGGREGATE
#include <ptclib/sockagg.h>

/** This class defines a handle suitable for use with the aggregation system
  */
class H323AggregatedH2x5Handle : public PAggregatedHandle
{
  PCLASSINFO(H323AggregatedH2x5Handle, PAggregatedHandle)
  public:
    H323AggregatedH2x5Handle(H323Transport & _transport, H323Connection & _connection);
    ~H323AggregatedH2x5Handle();

    PAggregatorFDList_t GetFDs();

    PBoolean OnRead();
    virtual PBoolean HandlePDU(PBoolean ok, PBYTEArray & pdu) = 0;
    PTimeInterval GetTimeout()
    { return transport.GetReadTimeout(); }

  protected:
    PAggregatorFD fd;
    H323Transport & transport;
    H323Connection & connection;
    PBYTEArray pduBuffer;
    PINDEX pduDataLen;
};

#endif

#ifdef H323_H248
 #include "svcctrl.h"
#endif


/* The following classes have forward references to avoid including the VERY
   large header files for H225 and H245. If an application requires access
   to the protocol classes they can include them, but for simple usage their
   inclusion can be avoided.
 */
class PPER_Stream;

class H225_EndpointType;
class H225_TransportAddress;
class H225_ArrayOf_PASN_OctetString;
class H225_ProtocolIdentifier;
class H225_AdmissionRequest;
class H225_AdmissionReject;
class H225_AdmissionConfirm;
class H225_InfoRequestResponse;
class H225_DisengageRequest;
class H225_FeatureSet;
class H225_Setup_UUIE;
class H225_ArrayOf_ServiceControlSession;
class H225_ServiceControlSession_reason;

class H245_TerminalCapabilitySet;
class H245_TerminalCapabilitySetReject;
class H245_OpenLogicalChannel;
class H245_OpenLogicalChannelAck;
class H245_TransportAddress;
class H245_UserInputIndication;
class H245_RequestMode;
class H245_RequestModeAck;
class H245_RequestModeReject;
class H245_ModeDescription;
class H245_ArrayOf_ModeDescription;
class H245_SendTerminalCapabilitySet;
class H245_MultiplexCapability;
class H245_FlowControlCommand;
class H245_MiscellaneousCommand;
class H245_MiscellaneousIndication;
class H245_JitterIndication;
class H245_ConferenceRequest;
class H245_ConferenceResponse;
class H245_ConferenceCommand;
class H245_ConferenceIndication;
class H245_GenericMessage;
class H245_ArrayOf_GenericParameter;

class H323SignalPDU;
class H323ControlPDU;
class H323_RTP_UDP;

class H235Authenticators;

class H245NegMasterSlaveDetermination;
class H245NegTerminalCapabilitySet;
class H245NegLogicalChannels;
class H245NegRequestMode;
class H245NegRoundTripDelay;

#ifdef H323_H450

class H450xDispatcher;
class H4502Handler;
class H4503Handler;
class H4504Handler;
class H4506Handler;
class H45011Handler;

#endif

#ifdef H323_T120
class OpalT120Protocol;
#endif

#ifdef H323_T38
class OpalT38Protocol;
#endif

#ifdef H323_H224
class OpalH224Handler;
class H224_Handler;
class H224_H281Handler;
#endif

#ifdef H323_T140
class H323_RFC4103Handler;
#endif

class OpalRFC2833;
class OpalRFC2833Info;

#ifdef H323_H460
class H460_FeatureSet;
#endif

#if H323_FILE
class H323FileTransferHandler;
class H323FileTransferList;
#endif

///////////////////////////////////////////////////////////////////////////////

/**This class represents a particular H323 connection between two endpoints.
   There are at least two threads in use, this one to look after the
   signalling channel, an another to look after the control channel. There
   would then be additional threads created for each data channel created by
   the control channel protocol thread.
 */

class H323Connection : public PObject
{
  PCLASSINFO(H323Connection, PObject);

  friend class AggregatedH225Handle;
  friend class AggregatedH245Handle;
  public:
  /**@name Construction */
  //@{
    enum Options {
      FastStartOptionDisable       = 0x0001,
      FastStartOptionEnable        = 0x0002,
      FastStartOptionMask          = 0x0003,

      H245TunnelingOptionDisable   = 0x0004,
      H245TunnelingOptionEnable    = 0x0008,
      H245TunnelingOptionMask      = 0x000c,

      H245inSetupOptionDisable     = 0x0010,
      H245inSetupOptionEnable      = 0x0020,
      H245inSetupOptionMask        = 0x0030,

      DetectInBandDTMFOptionDisable= 0x0040,
      DetectInBandDTMFOptionEnable = 0x0080,
      DetectInBandDTMFOptionMask   = 0x00c0,

#ifdef H323_RTP_AGGREGATE
      RTPAggregationDisable        = 0x0100,
      RTPAggregationEnable         = 0x0200,
      RTPAggregationMask           = 0x0300,
#endif

#ifdef H323_SIGNAL_AGGREGATE
      SignallingAggregationDisable = 0x0400,
      SignallingAggregationEnable  = 0x0800,
      SignallingAggregationMask    = 0x0c00
#endif
    };

    /**Create a new connection.
     */
    H323Connection(
      H323EndPoint & endpoint,  ///< H323 End Point object
      unsigned callReference,   ///< Call reference
      unsigned options = 0      ///< Connection option bits
    );

    /**Destroy the connection
     */
    ~H323Connection();

    /**Lock connection.
       When any thread wants exclusive use of the connection, it must use this
       function to gain the mutex. Note this is not a simple mutex to allow
       for the rather complicated mechanism for preventing deadlocks in
       associated threads to the connection (eg Q.931 reader thread).

       Returns FALSE if the lock was not obtainable due to the connection being
       shut down.
     */
    PBoolean Lock();

    /**Try to lock connection.
       When the H323EndPoint::FindConnectionWithLock() function is used to gain
       access to a connection object, this is called to prevent it from being
       closed and deleted by the background threads.

       Note this is an internal function and it is not expected an application
       would use it.

       Returns 0 if the lock was not obtainable due to the connection being
       shut down, -1 if it was not available, and +1 if lock is obtained.
     */
    int TryLock();

    /**Unlock connection.
       If the H323EndPoint::FindConnectionWithLock() function is used to gain
       access to a connection object, this MUST be called to allow it to
       subsequently be closed and disposed of.
     */
    void Unlock();

    /**
      * called when an ARQ needs to be sent to a gatekeeper. This allows the connection
      * to change or check fields in the ARQ before it is sent.
      *
      * By default, this calls the matching function on the endpoint
      */
    virtual void OnSendARQ(
      H225_AdmissionRequest & arq
    );

   /**
      * called when an ACF is received from a gatekeeper. 
      *
      * By default, this calls the matching function on the endpoint
      */
    virtual void OnReceivedACF(
      const H225_AdmissionConfirm & acf
    );

   /**
      * called when an ARJ is received from a gatekeeper. 
      *
      * By default, this calls the matching function on the endpoint
      */
    virtual void OnReceivedARJ(
      const H225_AdmissionReject & arj
    );

    /**
      * called when an IRR needs to be sent to a gatekeeper. This allows the connection
      * to change or check fields in the IRR before it is sent.
      *
      * By default, this does nothing
      */
    virtual void OnSendIRR(
        H225_InfoRequestResponse & irr
    ) const;

    /**
      * called when an DRQ needs to be sent to a gatekeeper. This allows the connection
      * to change or check fields in the DRQ before it is sent.
      *
      * By default, this does nothing
      */
    virtual void OnSendDRQ(
        H225_DisengageRequest & drq
    ) const;

    /** Called when a connection is established.
        Default behaviour is to call H323EndPoint::OnConnectionEstablished
      */
    virtual void OnEstablished();

    /** Called when a connection is cleared, just after CleanUpOnCallEnd()
        Default behaviour is to call H323EndPoint::OnConnectionCleared
      */
    virtual void OnCleared();

    /**Determine if the call has been connected.
       This indicates that Q.931 CONNECT has occurred. This usually means in
       PSTN gateway environments that a charge will be made for the call. This
       is not quite the same as IsEstablished() as that indicates the call is
       connected AND there is media open.
      */
    PBoolean IsConnected() const { return connectionState == HasExecutedSignalConnect || connectionState == EstablishedConnection; }

    /**Determine if the call has been established.
       This can be used in combination with the GetCallEndReason() function
       to determine the three main phases of a call, call setup, call
       established and call cleared.
      */
    PBoolean IsEstablished() const { return connectionState == EstablishedConnection; }

    /**Call clearance reasons.
       NOTE: if anything is added to this, you also need to add the field to
       the tables in h323.cxx and h323pdu.cxx.
      */
    enum CallEndReason {
      EndedByLocalUser,         ///< Local endpoint application cleared call
      EndedByNoAccept,          ///< Local endpoint did not accept call OnIncomingCall()=FALSE
      EndedByAnswerDenied,      ///< Local endpoint declined to answer call
      EndedByRemoteUser,        ///< Remote endpoint application cleared call
      EndedByRefusal,           ///< Remote endpoint refused call
      EndedByNoAnswer,          ///< Remote endpoint did not answer in required time
      EndedByCallerAbort,       ///< Remote endpoint stopped calling
      EndedByTransportFail,     ///< Transport error cleared call
      EndedByConnectFail,       ///< Transport connection failed to establish call
      EndedByGatekeeper,        ///< Gatekeeper has cleared call
      EndedByNoUser,            ///< Call failed as could not find user (in GK)
      EndedByNoBandwidth,       ///< Call failed as could not get enough bandwidth
      EndedByCapabilityExchange,///< Could not find common capabilities
      EndedByCallForwarded,     ///< Call was forwarded using FACILITY message
      EndedBySecurityDenial,    ///< Call failed a security check and was ended
      EndedByLocalBusy,         ///< Local endpoint busy
      EndedByLocalCongestion,   ///< Local endpoint congested
      EndedByRemoteBusy,        ///< Remote endpoint busy
      EndedByRemoteCongestion,  ///< Remote endpoint congested
      EndedByUnreachable,       ///< Could not reach the remote party
      EndedByNoEndPoint,        ///< The remote party is not running an endpoint
      EndedByHostOffline,       ///< The remote party host off line
      EndedByTemporaryFailure,  ///< The remote failed temporarily app may retry
      EndedByQ931Cause,         ///< The remote ended the call with unmapped Q.931 cause code
      EndedByDurationLimit,     ///< Call cleared due to an enforced duration limit
      EndedByInvalidConferenceID, ///< Call cleared due to invalid conference ID
      EndedByOSPRefusal,          ///< Call cleared as OSP server unable or unwilling to route
      EndedByInvalidNumberFormat, ///< Call cleared as number was invalid format
      EndedByUnspecifiedProtocolError, ///< Call cleared due to unspecified protocol error
      EndedByNoFeatureSupport,         ///< Call ended due to Feature not being present.
      NumCallEndReasons
    };
#if PTRACING
    friend ostream & operator<<(ostream & o, CallEndReason r);
#endif

    /**Get the call clearand reason for this connection shutting down.
       Note that this function is only generally useful in the
       H323EndPoint::OnConnectionCleared() function. This is due to the
       connection not being cleared before that, and the object not even
       exiting after that.

       If the call is still active then this will return NumCallEndReasons.
      */
    CallEndReason GetCallEndReason() const { return callEndReason; }

    /**Set the call clearance reason.
       An application should have no cause to use this function. It is present
       for the H323EndPoint::ClearCall() function to set the clearance reason.
      */
    virtual void SetCallEndReason(
      CallEndReason reason,     ///< Reason for clearance of connection.
      PSyncPoint * sync = NULL  ///< syncpoint to use for synchronous destruction
    );

    /**Clear a current connection.
       This hangs up the connection to a remote endpoint. It actually just
       calls the endpoint version of the ClearCall() function to avoid
       possible multithreading race conditions.
      */
    virtual PBoolean ClearCall(
      CallEndReason reason = EndedByLocalUser  ///< Reason for call clearing
    );

    /**Clear a current connection, synchronously
      */
    virtual PBoolean ClearCallSynchronous(
      PSyncPoint * sync,
      CallEndReason reason = EndedByLocalUser  ///< Reason for call clearing
    );

    /**Clean up the call clearance of the connection.
       This function will do any internal cleaning up and waiting on background
       threads that may be using the connection object. After this returns it
       is then safe to delete the object.

       An application will not typically use this function as it is used by the
       H323EndPoint during a clear call.
      */
    virtual void CleanUpOnCallEnd();
  //@}


  /**@name Signalling Channel */
  //@{
    /**Attach a transport to this connection as the signalling channel.
      */
    virtual void AttachSignalChannel(
      const PString & token,    ///< New token to use to identify connection
      H323Transport * channel,  ///< Transport for the PDU's
      PBoolean answeringCall        ///< Flag for if incoming/outgoing call.
    );

    /**Change the transport (signalling channel) for this connection.
      */
    virtual void ChangeSignalChannel(
        H323Transport * channel  ///< New Transport for PDU's
        );

    /**Write a PDU to the signalling channel.
      */
    PBoolean WriteSignalPDU(
      H323SignalPDU & pdu       ///< PDU to write.
    );

    /**Handle reading PDU's from the signalling channel.
       This is an internal function and is unlikely to be used by applications.
     */
    virtual void HandleSignallingChannel();

    /**Handle the situation where the call signalling channel fails
        return TRUE to keep the call alive / False to drop the call
      */
    virtual PBoolean HandleSignalChannelFailure()  { return FALSE; }

    /**Handle the situation where the media channel fails
        return TRUE to keep the call alive / False to drop the call
      */
    virtual PBoolean HandleControlChannelFailure()  { return FALSE; }

    /**Handle a single received PDU from the signalling channel.
       This is an internal function and is unlikely to be used by applications.
    */
    virtual PBoolean HandleReceivedSignalPDU(PBoolean readStatus, H323SignalPDU & pdu);

    /**Handle a single received PDU from the control channel.
       This is an internal function and is unlikely to be used by applications.
    */
    virtual PBoolean HandleReceivedControlPDU(PBoolean readStatus, PPER_Stream & strm);

    /**Handle PDU from the signalling channel.
       This is an internal function and is unlikely to be used by applications.
     */
    virtual PBoolean HandleSignalPDU(
      H323SignalPDU & pdu       ///< PDU to handle.
    );

    /**Handle Control PDU tunnelled in the signalling channel.
       This is an internal function and is unlikely to be used by applications.
     */
    virtual void HandleTunnelPDU(
      H323SignalPDU * txPDU       ///< PDU tunnel response into.
    );

    /**Handle an incoming Q931 setup PDU.
       The default behaviour is to do the handshaking operation calling a few
       virtuals at certain moments in the sequence.

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.
     */
    virtual PBoolean OnReceivedSignalSetup(
      const H323SignalPDU & pdu   ///< Received setup PDU
    );

    /**Handle an incoming Q931 setup acknowledge PDU.
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour does nothing.
     */
    virtual PBoolean OnReceivedSignalSetupAck(
      const H323SignalPDU & pdu   ///< Received setup PDU
    );

    /**Handle an incoming Q931 information PDU.
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour does nothing.
     */
    virtual PBoolean OnReceivedSignalInformation(
      const H323SignalPDU & pdu   ///< Received setup PDU
    );

    /**Handle an incoming Q931 call proceeding PDU.
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour checks for hH245Address field and if present
       starts the separate H245 channel, if successful or not present it
       returns TRUE.
     */
    virtual PBoolean OnReceivedCallProceeding(
      const H323SignalPDU & pdu   ///< Received call proceeding PDU
    );

    /**Handle an incoming Q931 progress PDU.
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour checks for hH245Address field and if present
       starts the separate H245 channel, if successful or not present it
       returns TRUE.
     */
    virtual PBoolean OnReceivedProgress(
      const H323SignalPDU & pdu   ///< Received call proceeding PDU
    );

    /**Handle an incoming Q931 alerting PDU.
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour obtains the display name and calls OnAlerting().
     */
    virtual PBoolean OnReceivedAlerting(
      const H323SignalPDU & pdu   ///< Received Alerting PDU
    );

    /**Handle an incoming Q931 connect PDU.
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour checks for hH245Address field and if present
       starts the separate H245 channel, if successful it returns TRUE.
       If not present and there is no H245Tunneling then it returns FALSE.
     */
    virtual PBoolean OnReceivedSignalConnect(
      const H323SignalPDU & pdu   ///< Received connect PDU
    );

    /**Handle an incoming Q931 facility PDU.
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour checks for hH245Address field and if present
       starts the separate H245 channel, if successful or not present it
       returns TRUE.
     */
    virtual PBoolean OnReceivedFacility(
      const H323SignalPDU & pdu   ///< Received Facility PDU
    );

    /**Handle an incoming Q931 Notify PDU.
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour simply returns TRUE.
     */
    virtual PBoolean OnReceivedSignalNotify(
      const H323SignalPDU & pdu   ///< Received Notify PDU
    );

    /**Handle an incoming Q931 Status PDU.
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour simply returns TRUE.
     */
    virtual PBoolean OnReceivedSignalStatus(
      const H323SignalPDU & pdu   ///< Received Status PDU
    );

    /**Handle an incoming Q931 Status Enquiry PDU.
       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour sends a Q931 Status PDU back.
     */
    virtual PBoolean OnReceivedStatusEnquiry(
      const H323SignalPDU & pdu   ///< Received Status Enquiry PDU
    );

    /**Handle an incoming Q931 Release Complete PDU.
       The default behaviour calls Clear() using reason code based on the
       Release Complete Cause field and the current connection state.
     */
    virtual void OnReceivedReleaseComplete(
      const H323SignalPDU & pdu   ///< Received Release Complete PDU
    );

    /**This function is called from the HandleSignallingChannel() function
       for unhandled PDU types.

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent. The default behaviour returns TRUE.
     */
    virtual PBoolean OnUnknownSignalPDU(
      const H323SignalPDU & pdu  ///< Received PDU
    );

    /**Call back for incoming call.
       This function is called from the OnReceivedSignalSetup() function
       before it sends the Alerting PDU. It gives an opportunity for an
       application to alter the reply before transmission to the other
       endpoint.

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour calls the endpoint function of the same name.
     */
    virtual PBoolean OnIncomingCall(
      const H323SignalPDU & setupPDU,   ///< Received setup PDU
      H323SignalPDU & alertingPDU       ///< Alerting PDU to send
    );
    virtual PBoolean OnIncomingCall(
      const H323SignalPDU & setupPDU,   ///< Received setup PDU
      H323SignalPDU & alertingPDU,      ///< Alerting PDU to send
      CallEndReason & reason            ///< reason for call refusal, if returned false
    );

    /**Forward incoming call to specified address.
       This would typically be called from within the OnIncomingCall()
       function when an application wishes to redirct an unwanted incoming
       call.

       The return value is TRUE if the call is to be forwarded, FALSE
       otherwise. Note that if the call is forwarded the current connection is
       cleared with the ended call code of EndedByCallForwarded.
      */
    virtual PBoolean ForwardCall(
      const PString & forwardParty   ///< Party to forward call to
    );

    /**Forward incoming call to specified address.
      */
    virtual PBoolean ForwardCall(
      const H225_ArrayOf_AliasAddress & alternativeAliasAddresses,
      const H323TransportAddress & alternativeAddress
    );

    /**Forward call to MC.
      */
    virtual PBoolean RouteCallToMC(
      const PString & forwardParty,   ///< Party to forward call to
      const H225_ConferenceIdentifier & confID    /// conference to join
    );

    /**Forward call to MC.
      */
    virtual PBoolean RouteCallToMC(
      const H225_ArrayOf_AliasAddress & alternativeAliasAddresses,
      const H323TransportAddress & alternativeAddress,
      const H225_ConferenceIdentifier & confID
    );

#ifdef H323_H450

    /**Initiate the transfer of an existing call (connection) to a new remote party
       using H.450.2.  This sends a Call Transfer Initiate Invoke message from the
       A-Party (transferring endpoint) to the B-Party (transferred endpoint).
     */
    void TransferCall(
      const PString & remoteParty,   ///< Remote party to transfer the existing call to
      const PString & callIdentity = PString::Empty()
                                    ///< Call Identity of secondary call if present
    );

    /**Transfer the call through consultation so the remote party in the primary call is connected to
       the called party in the second call using H.450.2.  This sends a Call Transfer Identify Invoke 
       message from the A-Party (transferring endpoint) to the C-Party (transferred-to endpoint).
     */
    void ConsultationTransfer(
      const PString & primaryCallToken  ///< Primary call
    );

    /**Called from H.450 OnReceivedInitiateReturnError Error in Transfer
     */
    virtual void OnReceivedInitiateReturnError();

    /**Handle the reception of a callTransferSetupInvoke APDU whilst a secondary call exists.  This 
       method checks whether the secondary call is still waiting for a callTransferSetupInvoke APDU and 
       proceeds to clear the call if the call identies match.
       This is an internal function and it is not expected the user will call
       it directly.
     */
    virtual void HandleConsultationTransfer(
      const PString & callIdentity, /**Call Identity of secondary call 
                                       received in SETUP Message. */
      H323Connection & incoming     ///< Connection upon which SETUP PDU was received.
    );

    /**Determine whether this connection is being transferred.
     */
    PBoolean IsTransferringCall() const;

    /**Determine whether this connection is the result of a transferred call.
     */
    PBoolean IsTransferredCall() const;

    /**Handle the transfer of an existing connection to a new remote.
       This is an internal function and it is not expected the user will call
       it directly.
     */
    virtual void HandleTransferCall(
      const PString & token,
      const PString & identity
    );

    /**Get transfer invoke ID dureing trasfer.
       This is an internal function and it is not expected the user will call
       it directly.
      */
    int GetCallTransferInvokeId();

    /**Handle the failure of a call transfer operation at the Transferred Endpoint.  This method is
       used to handle the following transfer failure cases that can occur at the Transferred Endpoint. 
       The cases are:
       Reception of an Admission Reject
       Reception of a callTransferSetup return error APDU.
       Expiry of Call Transfer timer CT-T4.
     */
    virtual void HandleCallTransferFailure(
      const int returnError    ///< Failure reason code
    );

    /**Store the passed token on the current connection's H4502Handler.
       This is an internal function and it is not expected the user will call
       it directly.
     */
    void SetAssociatedCallToken(
      const PString & token  ///< Associated token
    );

    /**Callback to indicate a successful transfer through consultation.  The paramter passed is a
       reference to the existing connection between the Transferring endpoint and Transferred-to 
       endpoint.
     */
    virtual void OnConsultationTransferSuccess(
      H323Connection & secondaryCall  ///< Secondary call for consultation
    );

     /**Set the call linkage associated with the current call. This is used to include the callToken which is 
      requesting this connection. ie. Call Transfer. This information can be used for billing systems
      to correctly charge the correct party for Transferred or forwarded calls.
       */
     virtual void SetCallLinkage(
        H225_AdmissionRequest & arq   ///< Admission Request PDU
      );

     /**Set the call linkage associated with the current call. This is used to detect the callToken which has 
      requesting this connection. ie. Call Transfer. This information can be used for billing systems
      to correctly charge the correct party for Transferred or forwarded calls.
     */
     virtual void GetCallLinkage(
        const H225_AdmissionRequest & arq  ///< Admission Request PDU
    );

    /**Retrieves the redirecting number(s) and additional call diversion information (div. counter
       and div. reason) as of an incoming redirected call, currently only according to H.450.3 call diversion
       supplementary service 
    */

    PBoolean GetRedirectingNumber(
      PString &originalCalledNr,               
      PString &lastDivertingNr,
      int &divCounter, 
      int &originaldivReason,
      int &divReason);

    /**Place the call on hold, suspending all media channels (H.450.4).  Note it is the responsibility 
       of the application layer to delete the MOH Channel if music on hold is provided to the remote
       endpoint.  So far only Local Hold has been implemented. 
     */
    void HoldCall(
      PBoolean localHold   ///< true for Local Hold, false for Remote Hold
    );

    /**Retrieve the call from hold, activating all media channels (H.450.4).
       This method examines the call hold state and performs the necessary
       actions required to retrieve a Near-end or Remote-end call on hold.
       NOTE: Only Local Hold is implemented so far. 
    */
    void RetrieveCall();

    /**Set the alternative Audio media channel.  This channel can be used to provide
       Media On Hold (MOH) for a near end call hold operation or to provide
       Recorded Voice Anouncements (RVAs).  If this method is not called before
       a call hold operation is attempted, no Audio media on hold will be provided
       for the held endpoint.
      */
    void SetHoldMedia(
      PChannel * audioChannel
    );

    /**Set the alternative Video media channel.  This channel can be used to provide
       Video On Hold (VOH) for a near end call hold operation or to provide
       a fake or onhold video.  If this method is not called before
       a call hold operation is attempted, no Video media on hold will be provided
       for the held endpoint.
      */
    void SetVideoHoldMedia(
      PChannel * videoChannel
    );

    /**CallBack when Call is put on hold. This allows the device to release the 
       Local Input device to be used for another active connection
      */
    virtual PChannel *  OnCallHold(PBoolean IsEncoder,     ///* Direction
                               unsigned sessionId,     ///* Session Id 
                               unsigned bufferSize,    ///* Size of each sound buffer (Audio)
                               PChannel * channel);    ///* Channel being Held

     /**CallBack when call is about to be retrieved. This allows the Local Input device
       to be reattached to the Held Channel.
       */
    virtual PChannel *  OnCallRetrieve(PBoolean IsEncoder,  ///* Direction
                               unsigned sessionId,      ///* Session Id    
                               unsigned bufferSize,     ///* Size of each sound buffer (Audio)
                               PChannel * channel);    ///* Channel being Held

    /**Determine if Meadia On Hold is enabled.
      */
    PBoolean IsMediaOnHold() const;

    /**Determine if held.
      */
    PBoolean IsLocalHold() const;

    /**Determine if held.
      */
    PBoolean IsRemoteHold() const;

    /**Determine if the current call is held or in the process of being held.
      */
    PBoolean IsCallOnHold() const;

    /**Begin a call intrusion request.
       Calls h45011handler->IntrudeCall where SS pdu is added to Call Setup
       message.
      */
    virtual void IntrudeCall(
      unsigned capabilityLevel
    );

    /**Handle an incoming call instrusion request.
       Calls h45011handler->AwaitSetupResponse where we set Handler state to
       CI-Wait-Ack
      */
    virtual void HandleIntrudeCall(
      const PString & token,
      const PString & identity
    );

    /**Set flag indicating call intrusion.
       Used to set a flag when intrusion occurs and to determine if
       connection is created for Call Intrusion. This flag is used when we
       should decide whether to Answer the call or to Close it.
      */
    void SetCallIntrusion() { isCallIntrusion = TRUE; }

    PBoolean IsCallIntrusion() { return isCallIntrusion; }

    /**Get Call Intrusion Protection Level of the local endpoint.
      */
    unsigned GetLocalCallIntrusionProtectionLevel() { return callIntrusionProtectionLevel; }

    /**Get Call Intrusion Protection Level of other endpoints that we are in
       connection with.
      */
    virtual PBoolean GetRemoteCallIntrusionProtectionLevel(
      const PString & callToken,
      unsigned callIntrusionProtectionLevel
    );

    virtual void SetIntrusionImpending();

    virtual void SetForcedReleaseAccepted();

    virtual void SetIntrusionNotAuthorized();

    /**Send a Call Waiting indication message to the remote endpoint using
       H.450.6.  The second paramter is used to indicate to the calling user
       how many additional users are "camped on" the called user. A value of
       zero indicates to the calling user that he/she is the only user
       attempting to reach the busy called user.
     */
    void SendCallWaitingIndication(
      const unsigned nbOfAddWaitingCalls = 0   ///< number of additional waiting calls at the served user
    );

    enum MWIType {
      mwiNone,
      mwiActivate,
      mwiDeactivate,
      mwiInterrogate
    };

    struct MWIInformation {
        MWIInformation()
        : mwiCtrId(PString()), mwiUser(PString()), 
          mwitype(mwiNone), mwiCalls(0) {}

        PString mwiCtrId;                          ///< Message Center ID
        PString mwiUser;                           ///< UserName for MWI service (optional by default is the endpoint alias name)
        MWIType mwitype;                           ///< Type of MWI Call (activate, deactive, interrogate).
        int     mwiCalls;                          ///< Number of calls awaiting (used for activate messages)
        /* To do. Add call detail information */
    };

    /** Set the MWI Parameters
        Use this in H323EndPoint::CreateConnection override to set the call to being a non-call supplimentary MWI
        service with the given parameters
      */
    void SetMWINonCallParameters(
        const MWIInformation & mwiInfo             ///< Message wait indication structure
    );

    /** Get the MWI Parameters
        This is called by the H.450.7 Supplimentary service build to include the parameters in the call
      */
    const MWIInformation & GetMWINonCallParameters();

    /**Received a message wait indication.
        Override to collect MWI messages.
        default calls Endpoint function of same name.
        return false indicates MWI rejected.
      */
    virtual PBoolean OnReceivedMWI(const MWIInformation & mwiInfo);

    /**Received a message wait indication Clear.
        Override to remove any outstanding MWIs.
        default calls Endpoint function of same name.
        return false indicates MWI rejected.
      */
    virtual PBoolean OnReceivedMWIClear(const PString & user);


    /**Received a message wait indication request on a mail server (Interrogate).
        This is used for enquiring on a mail server if 
        there are any active messages for the served user.
        default calls Endpoint function of same name.
        return false indicates MWI request rejected.
      */
    virtual PBoolean OnReceivedMWIRequest(const PString & user);

#endif // H323_H450

    enum AnswerCallResponse {
      AnswerCallNow,               ///< Answer the call continuing with the connection.
      AnswerCallDenied,            ///< Refuse the call sending a release complete.
      AnswerCallPending,           ///< Send an Alerting PDU and wait for AnsweringCall()
      AnswerCallDeferred,          ///< As for AnswerCallPending but does not send Alerting PDU
      AnswerCallAlertWithMedia,    ///< As for AnswerCallPending but starts media channels
      AnswerCallDeferredWithMedia, ///< As for AnswerCallDeferred but starts media channels
      AnswerCallDeniedByInvalidCID, ///< As for AnswerCallDenied but returns e_invalidCID
      AnswerCallNowWithAlert,
      NumAnswerCallResponses
    };
#if PTRACING
    friend ostream & operator<<(ostream & o, AnswerCallResponse s);
#endif

    /**Call back for answering an incoming call.
       This function is used for an application to control the answering of
       incoming calls. It is usually used to indicate the immediate action to
       be taken in answering the call.

       It is called from the OnReceivedSignalSetup() function before it sends
       the Alerting or Connect PDUs. It also gives an opportunity for an
       application to alter the Connect PDU reply before transmission to the
       remote endpoint.

       If AnswerCallNow is returned then the H.323 protocol proceeds with the
       connection. If AnswerCallDenied is returned the connection is aborted
       and a Release Complete PDU is sent. If AnswerCallPending is returned
       then the Alerting PDU is sent and the protocol negotiations are paused
       until the AnsweringCall() function is called. Finally, if
       AnswerCallDeferred is returned then no Alerting PDU is sent, but the
       system still waits as in the AnswerCallPending response.

       Note this function should not block for any length of time. If the
       decision to answer the call may take some time eg waiting for a user to
       pick up the phone, then AnswerCallPending or AnswerCallDeferred should
       be returned.

       The default behaviour calls the endpoint function of the same name
       which in turn will return AnswerCallNow.
     */
    virtual AnswerCallResponse OnAnswerCall(
      const PString & callerName,       ///< Name of caller
      const H323SignalPDU & setupPDU,   ///< Received setup PDU
      H323SignalPDU & connectPDU        ///< Connect PDU to send. 
    );

    /**Indicate the result of answering an incoming call.
       This should only be called if the OnAnswerCall() callback function has
       returned a AnswerCallPending or AnswerCallDeferred response.

       Note sending further AnswerCallPending responses via this function will
       have the result of an Alerting PDU being sent to the remote endpoint.
       In this way multiple Alerting PDUs may be sent.

       Sending a AnswerCallDeferred response would have no effect.
      */
    void AnsweringCall(
      AnswerCallResponse response ///< Answer response to incoming call
    );

    /**Send first PDU in signalling channel.
       This function does the signalling handshaking for establishing a
       connection to a remote endpoint. The transport (TCP/IP) for the
       signalling channel is assumed to be already created. This function
       will then do the SetRemoteAddress() and Connect() calls o establish
       the transport.

       Returns the error code for the call failure reason or NumCallEndReasons
       if the call was successful to that point in the protocol.
     */
    virtual CallEndReason SendSignalSetup(
      const PString & alias,                ///< Name of remote party
      const H323TransportAddress & address  ///< Address of destination
    );

    /**Adjust setup PDU being sent on initialisation of signal channel.
       This function is called from the SendSignalSetup() function before it
       sends the Setup PDU. It gives an opportunity for an application to
       alter the request before transmission to the other endpoint.

       The default behaviour simply returns TRUE. Note that this is usually
       overridden by the transport dependent descendent class, eg the
       H323ConnectionTCP descendent fills in the destCallSignalAddress field
       with the TCP/IP data. Therefore if you override this in your
       application make sure you call the ancestor function.
     */
    virtual PBoolean OnSendSignalSetup(
      H323SignalPDU & setupPDU   ///< Setup PDU to send
    );

    /**Adjust call proceeding PDU being sent. This function is called from
       the OnReceivedSignalSetup() function before it sends the Call
       Proceeding PDU. It gives an opportunity for an application to alter
       the request before transmission to the other endpoint. If this function
       returns FALSE then the Call Proceeding PDU is not sent at all.

       The default behaviour simply returns TRUE.
     */
    virtual PBoolean OnSendCallProceeding(
      H323SignalPDU & callProceedingPDU   ///< Call Proceeding PDU to send
    );

    /**Call back for Release Complete being sent.
       This allows an application to add things to the release complete before
       it is sent to the remote endpoint.

       Returning FALSE will prevent the release complete from being sent. Note
       that this would be very unusual as this is called when the connection
       is being cleaned up. There will be no second chance to send the PDU and
       it must be sent.

       The default behaviour simply returns TRUE.
      */
    virtual PBoolean OnSendReleaseComplete(
      H323SignalPDU & releaseCompletePDU ///< Release Complete PDU to send
    );

    /**Call back for remote party being alerted.
       This function is called from the SendSignalSetup() function after it
       receives the optional Alerting PDU from the remote endpoint. That is
       when the remote "phone" is "ringing".

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour calls the endpoint function of the same name.
     */
    virtual PBoolean OnAlerting(
      const H323SignalPDU & alertingPDU,  ///< Received Alerting PDU
      const PString & user                ///< Username of remote endpoint
    );

    /**This function is called when insufficient digits have been entered.
       This supports overlapped dialling so that a call can begin when it is
       not known how many more digits are to be entered in a phone number.

       It is expected that the application will override this function. It
       should be noted that the application should not block in the function
       but only indicate to whatever other thread is gathering digits that
       more are required and that thread should call SendMoreDigits().

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour simply returns FALSE.
     */
    virtual PBoolean OnInsufficientDigits();

    /**This function is called when sufficient digits have been entered.
       This supports overlapped dialling so that a call can begin when it is
       not known how many more digits are to be entered in a phone number.

       The digits parameter is appended to the existing remoteNumber member
       variable and the call is retried.

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour simply returns TRUE.
     */
    virtual void SendMoreDigits(
      const PString & digits    ///< Extra digits
    );

    /**This function is called from the SendSignalSetup() function after it
       receives the Connect PDU from the remote endpoint, but before it
       attempts to open the control channel.

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour calls H323EndPoint::OnOutgoingCall
     */
    virtual PBoolean OnOutgoingCall(
      const H323SignalPDU & connectPDU   ///< Received Connect PDU
    );

    /**Send an the acknowldege of a fast start.
       This function is called when the fast start channels provided to this
       connection by the original SETUP PDU have been selected and opened and
       need to be sent back to the remote endpoint.

       If FALSE is returned then no fast start has been acknowledged, possibly
       due to no common codec in fast start request.

       The default behaviour uses OnSelectLogicalChannels() to find a pair of
       channels and adds then to the provided PDU.
     */
    virtual PBoolean SendFastStartAcknowledge(
      H225_ArrayOf_PASN_OctetString & array   ///< Array of H245_OpenLogicalChannel
    );

    /**Handle the acknowldege of a fast start.
       This function is called from one of a number of functions after it
       receives a PDU from the remote endpoint that has a fastStart field. It
       is in response to a request for a fast strart from the local endpoint.

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour parses the provided array and starts the channels
       acknowledged in it.
     */
    virtual PBoolean HandleFastStartAcknowledge(
      const H225_ArrayOf_PASN_OctetString & array   ///< Array of H245_OpenLogicalChannel
    );

    /**Start a separate H245 channel.
       This function is called from one of a number of functions when it needs
       to create the h245 channel for the remote endpoint to connect back to.

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.
     */
    virtual PBoolean StartControlChannel();

    /**Start a separate H245 channel.
       This function is called from one of a number of functions after it
       receives a PDU from the remote endpoint that has a h245Address field.

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent.

       The default behaviour checks to see if it is a known transport and
       creates a corresponding H323Transport decendent for the control
       channel.
     */
    virtual PBoolean StartControlChannel(
      const H225_TransportAddress & h245Address   ///< H245 address
    );
  //@}

  /**@name Control Channel */
  //@{
    /**Write a PDU to the control channel.
       If there is no control channel open then this will tunnel the PDU
       into the signalling channel.
      */
    PBoolean WriteControlPDU(
      const H323ControlPDU & pdu
    );

    /**Start control channel negotiations.
      */
    virtual PBoolean StartControlNegotiations(
      PBoolean renegotiate = FALSE  ///< Force renogotiation of TCS/MSD
    );

    /**Handle reading data on the control channel.
     */
    virtual void HandleControlChannel();

    /**Handle incoming data on the control channel.
       This decodes the data stream into a PDU and calls HandleControlPDU().

       If FALSE is returned the connection is aborted. The default behaviour
       returns TRUE.
     */
    virtual PBoolean HandleControlData(
      PPER_Stream & strm
    );

    /**Handle incoming PDU's on the control channel. Dispatches them to the
       various virtuals off this class.

       If FALSE is returned the connection is aborted. The default behaviour
       returns TRUE.
     */
    virtual PBoolean HandleControlPDU(
      const H323ControlPDU & pdu
    );

    /**This function is called from the HandleControlPDU() function
       for unhandled PDU types.

       If FALSE is returned the connection is aborted and a Release Complete
       PDU is sent. The default behaviour returns TRUE.

       The default behaviour send a FunctioNotUnderstood indication back to
       the sender, and returns TRUE to continue operation.
     */
    virtual PBoolean OnUnknownControlPDU(
      const H323ControlPDU & pdu  ///< Received PDU
    );

    /**Handle incoming request PDU's on the control channel.
       Dispatches them to the various virtuals off this class.
     */
    virtual PBoolean OnH245Request(
      const H323ControlPDU & pdu  ///< Received PDU
    );

    /**Handle incoming response PDU's on the control channel.
       Dispatches them to the various virtuals off this class.
     */
    virtual PBoolean OnH245Response(
      const H323ControlPDU & pdu  ///< Received PDU
    );

    /**Handle incoming command PDU's on the control channel.
       Dispatches them to the various virtuals off this class.
     */
    virtual PBoolean OnH245Command(
      const H323ControlPDU & pdu  ///< Received PDU
    );

    /**Handle incoming indication PDU's on the control channel.
       Dispatches them to the various virtuals off this class.
     */
    virtual PBoolean OnH245Indication(
      const H323ControlPDU & pdu  ///< Received PDU
    );

    /**Handle H245 command to send terminal capability set.
     */
    virtual PBoolean OnH245_SendTerminalCapabilitySet(
      const H245_SendTerminalCapabilitySet & pdu  ///< Received PDU
    );

    /**Handle H245 command to control flow control.
       This function calls OnLogicalChannelFlowControl() with the channel and
       bit rate restriction.
     */
    virtual PBoolean OnH245_FlowControlCommand(
      const H245_FlowControlCommand & pdu  ///< Received PDU
    );

    /**Handle H245 miscellaneous command.
       This function passes the miscellaneous command on to the channel
       defined by the pdu.
     */
    virtual PBoolean OnH245_MiscellaneousCommand(
      const H245_MiscellaneousCommand & pdu  ///< Received PDU
    );

    /**Handle H245 miscellaneous indication.
       This function passes the miscellaneous indication on to the channel
       defined by the pdu.
     */
    virtual PBoolean OnH245_MiscellaneousIndication(
      const H245_MiscellaneousIndication & pdu  ///< Received PDU
    );

    /**Handle H245 indication of received jitter.
       This function calls OnLogicalChannelJitter() with the channel and
       estimated jitter.
     */
    virtual PBoolean OnH245_JitterIndication(
      const H245_JitterIndication & pdu  ///< Received PDU
    );

    /**Error discriminator for the OnControlProtocolError() function.
      */
    enum ControlProtocolErrors {
      e_MasterSlaveDetermination,
      e_CapabilityExchange,
      e_LogicalChannel,
      e_ModeRequest,
      e_RoundTripDelay
    };

    /**This function is called from the HandleControlPDU() function or
       any of its sub-functions for protocol errors, eg unhandled PDU types.

       The errorData field may be a string or PDU or some other data depending
       on the value of the errorSource parameter. These are:
          e_UnhandledPDU                    &H323ControlPDU
          e_MasterSlaveDetermination        const char *

       If FALSE is returned the connection is aborted. The default behaviour
       returns TRUE.
     */
    virtual PBoolean OnControlProtocolError(
      ControlProtocolErrors errorSource,  ///< Source of the proptoerror
      const void * errorData = NULL       ///< Data associated with error
    );

    /**This function is called from the HandleControlPDU() function when
       it is about to send the Capabilities Set to the remote endpoint. This
       gives the application an oppurtunity to alter the PDU to be sent.
     */
    virtual void OnSendCapabilitySet(
      H245_TerminalCapabilitySet & pdu  ///< PDU to send
    );

    /**This function is called when the remote endpoint sends its capability
       set. This gives the application an opportunity to inspect the
       incoming H.245 capability set, before the framework handles it.
       It is called before the other OnReceivedCapabilitySet().
       The default implementation does nothing.
    */
    virtual void OnReceivedCapabilitySet(
      const H245_TerminalCapabilitySet & pdu  ///< received PDU
    );

    /**This function is called when the remote endpoint sends its capability
       set. This gives the application an opportunity to determine what codecs
       are available and if it supports any of the combinations of codecs.

       Note any codec types that the remote system supports that are not in
       the codecs list member variable for the endpoint are ignored and not
       included in the remoteCodecs list.

       The default behaviour assigns the table and set to member variables and
       returns TRUE if the remoteCodecs list is not empty.
     */
    virtual PBoolean OnReceivedCapabilitySet(
      const H323Capabilities & remoteCaps,      ///< Capability combinations remote supports
      const H245_MultiplexCapability * muxCap,  ///< Transport capability, if present
      H245_TerminalCapabilitySetReject & reject ///< Rejection PDU (if return FALSE)
    );

    /**This function is called when a common capability set has been calculated
        Override this function to display to the user the various Capabilities
        available (started and not started) for the call.

       The default behaviour returns TRUE.
     */
    virtual PBoolean OnCommonCapabilitySet(
      H323Capabilities & caps                ///< Common Capability Set
    ) const;

    /**Send a new capability set.
      */
    virtual void SendCapabilitySet(
      PBoolean empty  ///< Send an empty set.
    );

    /**Call back to set the local capabilities.
       This is called just before the capabilties are required when a call
       is begun. It is called when a SETUP PDU is received or when one is
       about to be sent, so that the capabilities may be adjusted for correct
       fast start operation.

       The default behaviour does nothing.
      */
    virtual void OnSetLocalCapabilities();

    /**Call back to set the local UserInput capabilities.
       This is called just before the capabilties are required when a call
       is begun. It is called when a SETUP PDU is received or when one is
       about to be sent, so that the UserInput capabilities may be adjusted.

       The default behaviour set the default UserInput Settings.
      */
    virtual void OnSetLocalUserInputCapabilities();

    /**Set the Bearer Capability Transfer Rate
       This will set the Transfer Rate of the Bearer Capabities.
       Default = 768kb/s
      */
    virtual void OnBearerCapabilityTransferRate(
         unsigned & bitRate                            ///< BitRate (in bytes)
    );

    /**Set the initial Capability Bandwidth for the TCS
       This will set the bandwidth limit for the given capability type that
       will be negotiated in the TCS. This fuinction sets the Maximum bitRate
       for the capability. The bandwidth in the OLC and any Flow Control values
       received will not exceed this set maximum value.
      */
    void SetInitialBandwidth(
         H323Capability::MainTypes  captype,    ///< Capability Type
         int bitRate                            ///< BitRate (in bytes)
    );

    /**Set the Payload Size
       This will set the RTP Payloadsize limit.
       By Default it is 1400. 
      */
    void SetMaxPayloadSize(
        H323Capability::MainTypes captype,      ///< Capability Type
        int size                                ///< Payload size
    );

    /**Set the Video to emphasis Speed not Quality
       This will weigh frame rate over frame size
      */
     void SetEmphasisSpeed(
         H323Capability::MainTypes captype,     ///< Capability Type 
         bool speed                             ///< Payload size
     );

    /**Callback to modify the outgoing H245_OpenLogicalChannel.
       The default behaviour does nothing.
      */
    virtual void OnSendH245_OpenLogicalChannel(
        H245_OpenLogicalChannel & /*open*/, 
        PBoolean /*forward*/
        ) { }

    /**Return if this H245 connection is a master or slave
     */
    PBoolean IsH245Master() const;

    /**Start the round trip delay calculation over the control channel.
     */
    void StartRoundTripDelay();

    /**Get the round trip delay over the control channel.
     */
    PTimeInterval GetRoundTripDelay() const;
  //@}

  /**@name Logical Channel Management */
  //@{
    /**Call back to select logical channels to start.

       This function must be defined by the descendent class. It is used
       to select the logical channels to be opened between the two endpoints.
       There are three ways in which this may be called: when a "fast start"
       has been initiated by the local endpoint (via SendSignalSetup()
       function), when a "fast start" has been requested from the remote
       endpoint (via the OnReceivedSignalSetup() function) or when the H245
       capability set (and master/slave) negotiations have completed (via the
       OnControlChannelOpen() function.

       The function would typically examine several member variable to decide
       which mode it is being called in and what to do. If fastStartState is
       FastStartDisabled then non-fast start semantics should be used. The
       H245 capabilities in the remoteCapabilities members should be
       examined, and appropriate transmit channels started using
       OpenLogicalChannel().

       If fastStartState is FastStartInitiate, then the local endpoint has
       initiated a call and is asking the application if fast start semantics
       are to be used. If so it is expected that the function call 
       OpenLogicalChannel() for all the channels that it wishes to be able to
       be use. A subset (possibly none!) of these would actually be started
       when the remote endpoint replies.

       If fastStartState is FastStartResponse, then this indicates the remote
       endpoint is attempting a fast start. The fastStartChannels member
       contains a list of possible channels from the remote that the local
       endpoint is to select which to accept. For each accepted channel it
       simply necessary to call the Start() function on that channel eg
       fastStartChannels[0].Start();

       The default behaviour selects the first codec of each session number
       that is available. This is according to the order of the capabilities
       in the remoteCapabilities, the local capability table or of the
       fastStartChannels list respectively for each of the above scenarios.
      */
    virtual void OnSelectLogicalChannels();

    /**Select default logical channel for normal start.
      */
    virtual void SelectDefaultLogicalChannel(
      unsigned sessionID    ///< Session ID to find default logical channel.
    );

    /** MinMerge the local and remote Video and Extended Video Capabilities to ensure
        correct maximum framesize is negotiated between the parties.
      */
    virtual PBoolean MergeCapabilities(
        unsigned sessionID,                ///< Session ID to find default logical channel.
        const H323Capability & local,     ///< Local Capability
        H323Capability * remote           ///< remote Capability
    );

	/** Merge remote language support to the local supported languages to 
		determine common languages supported.
	  */
	virtual PBoolean MergeLanguages(
		const PStringList & remote,
        PBoolean isCaller
	);

	/** Merge remote language support to the local supported languages to 
		determine common languages supported.
	  */
	virtual PBoolean MergeLanguages(
		const PStringList & remote
	);

	/** Event fires when a common language is determined
		Implementers should override this function to notify the
		user the language of the remote caller.
		returning FALSE drops the call with reason  
	  */
	virtual PBoolean OnCommonLanguages(const PStringList & lang);

    /**Select default logical channel for fast start.
       Internal function, not for normal use.
      */
    virtual void SelectFastStartChannels(
      unsigned sessionID,   ///< Session ID to find default logical channel.
      PBoolean transmitter,     ///< Whether to open transmitters
      PBoolean receiver         ///< Whether to open receivers
    );

    /** Disable FastStart on a call by call basis
      */
    void DisableFastStart();

    /**Open a new logical channel.
       This function will open a channel between the endpoints for the
       specified capability.

       If this function is called while there is not yet a conenction
       established, eg from the OnFastStartLogicalChannels() function, then
       a "trial" receiver/transmitter channel is created. This channel is not
       started until the remote enpoint has confirmed that they are to start.
       Any channels not confirmed are deleted.

       If this function is called later in the call sequence, eg from
       OnSelectLogicalChannels(), then it may only establish a transmit
       channel, ie fromRemote must be FALSE.
      */
    virtual PBoolean OpenLogicalChannel(
      const H323Capability & capability,  ///< Capability to open channel with
      unsigned sessionID,                 ///< Session for the channel
      H323Channel::Directions dir         ///< Direction of channel
    );

    /**This function is called when the remote endpoint want's to open
       a new channel.

       If the return value is FALSE then the open is rejected using the
       errorCode as the cause, this would be a value from the enum
       H245_OpenLogicalChannelReject_cause::Choices.

       The default behaviour simply returns TRUE.
     */
    virtual PBoolean OnOpenLogicalChannel(
      const H245_OpenLogicalChannel & openPDU,            ///< Received PDU for the channel open
      H245_OpenLogicalChannelAck & ackPDU,                ///< PDU to send for acknowledgement
      unsigned & errorCode,                                ///< Error to return if refused
      const unsigned & channelNumber = 0                ///< Channel Number to open
    );

    /**Callback for when a logical channel conflict has occurred.
       This is called when the remote endpoint, which is a master, rejects
       our transmitter channel due to a resource conflict. Typically an
       inability to do asymmetric codecs. The local (slave) endpoint must then
       try and open a new transmitter channel using the same codec as the
       receiver that is being opened.
      */
    virtual PBoolean OnConflictingLogicalChannel(
      H323Channel & channel    ///< Channel that conflicted
    );

    /**Create a new logical channel object.
       This is in response to a request from the remote endpoint to open a
       logical channel.
      */
    virtual H323Channel * CreateLogicalChannel(
      const H245_OpenLogicalChannel & open, ///< Parameters for opening channel
      PBoolean startingFast,                    ///< Flag for fast/slow starting.
      unsigned & errorCode                  ///< Reason for create failure
    );

    /**Create a new real time logical channel object.
       This creates a logical channel for handling RTP data. It is primarily
       used to allow an application to redirect the RTP media streams to other
       hosts to the local one. In that case it would create an instance of
       the H323_ExternalRTPChannel class with the appropriate address. eg:

         H323Channel * MyConnection::CreateRealTimeLogicalChannel(
                                        const H323Capability & capability,
                                        H323Channel::Directions dir,
                                        unsigned sessionID,
                                        const H245_H2250LogicalChannelParameters * param,
                                        RTP_QOS * rtpqos)
         {
 #ifdef H323_H235
           H323Channel * extChannel = new H323_ExternalRTPChannel(*this, capability, dir, sessionID,
                                                                    externalIpAddress, externalPort)
           return new H323SecureChannel(this, capability, extChannel);
 #else
           return new H323_ExternalRTPChannel(*this, capability, dir, sessionID,
                                              externalIpAddress, externalPort);
 #endif
         }

       An application would typically also override the OnStartLogicalChannel()
       function to obtain from the H323_ExternalRTPChannel instance the address
       of the remote endpoints media server RTP addresses to complete the
       setting up of the external RTP stack. eg:

         PBoolean OnStartLogicalChannel(H323Channel & channel)
         {
           H323_ExternalRTPChannel & external = (H323_ExternalRTPChannel &)channel;
           external.GetRemoteAddress(remoteIpAddress, remotePort);
         }

       Note that the port in the above example is always the data port, the
       control port is assumed to be data+1.

       The default behaviour assures there is an RTP session for the session ID,
       and if not creates one, then creates a H323_RTPChannel which will do RTP
       media to the local host.
      */
    virtual H323Channel * CreateRealTimeLogicalChannel(
      const H323Capability & capability, ///< Capability creating channel
      H323Channel::Directions dir,       ///< Direction of channel
      unsigned sessionID,                ///< Session ID for RTP channel
      const H245_H2250LogicalChannelParameters * param,
                                         ///< Parameters for channel
      RTP_QOS * rtpqos = NULL            ///< QoS for RTP
    );

    /**This function is called when the remote endpoint want's to create
       a new channel.

       If the return value is FALSE then the open is rejected using the
       errorCode as the cause, this would be a value from the enum
       H245_OpenLogicalChannelReject_cause::Choices.

       The default behaviour checks the capability set for if this capability
       is allowed to be opened with other channels that may already be open.
     */
    virtual PBoolean OnCreateLogicalChannel(
      const H323Capability & capability,  ///< Capability for the channel open
      H323Channel::Directions dir,        ///< Direction of channel
      unsigned & errorCode                ///< Error to return if refused
    );

    /**Call back function when a logical channel thread begins.

       The default behaviour does nothing and returns TRUE.
      */
    virtual PBoolean OnStartLogicalChannel(
      H323Channel & channel    ///< Channel that has been started.
    );

    /** Initial Flow Restrictions
       
       This is called when the channel has been openAck and allows the receiver
       to specify flow control restrictions.
      */
    virtual PBoolean OnInitialFlowRestriction(
       H323Channel & channel  ///< Channel that may require Flow Restriction
    );

#ifdef H323_AUDIO_CODECS
    /**Open a channel for use by an audio codec.
       The H323AudioCodec class will use this function to open the channel to
       read/write PCM data.

       The default behaviour calls the equivalent function on the endpoint.
      */
    virtual PBoolean OpenAudioChannel(
      PBoolean isEncoding,       ///< Direction of data flow
      unsigned bufferSize,   ///< Size of each sound buffer
      H323AudioCodec & codec ///< codec that is doing the opening
    );
#endif

#ifdef H323_VIDEO
    /**Open a channel for use by an video codec.
       The H323VideoCodec class will use this function to open the channel to
       read/write image data.

       The default behaviour calls the equivalent function on the endpoint.
      */
    virtual PBoolean OpenVideoChannel(
      PBoolean isEncoding,       ///< Direction of data flow
      H323VideoCodec & codec ///< codec doing the opening
    );

#ifdef H323_H239
    /** Open a H.239 Channel
      */
    PBoolean OpenH239Channel();

    /** Close a H.239 Channel
     */
    PBoolean CloseH239Channel(H323Capability::CapabilityDirection dir =  H323Capability::e_Transmit);

    /** Callback when a H239 Session
        has been started
      */
    virtual void OnH239SessionStarted(
       int sessionNum,   ///< Channel Number opened
       H323Capability::CapabilityDirection dir ///< Direction of Channel
    );

    /** Callback when a H239 Session
        has been ended
      */
    virtual void OnH239SessionEnded(
       int sessionNum,   ///< Channel Number to close
       H323Capability::CapabilityDirection dir ///< Direction of Channel
    );


    /** Request to open session denied event
      */
    virtual void OpenExtendedVideoSessionDenied();

    /** On Receiving a H.239 Control Request
        Return False to reject the request to open channel.
        Set Delay to delay opening the channel. 
        Calling OnH239ControlRequest() will invoke
    */
    virtual PBoolean AcceptH239ControlRequest(PBoolean & delay);

    /** On Receiving a H.239 Control Request
        Return False to reject the request to open channel.
    */
    PBoolean OnH239ControlRequest(H239Control * ctrl = NULL);

    /** On Receiving a H.239 Control Command
        This usually means the closing of the H.239 Channel
    */
    virtual PBoolean OnH239ControlCommand(H239Control * ctrl = NULL);

    /** Open an Extended Video Session
        This will open an Extended Video session.
    */
    PBoolean OpenExtendedVideoSession(H323ChannelNumber & num, int defaultSession=0);

    /** On Received an Extended Video OLC
        This indicates the receipt of Extended Video OLC
    */
    virtual void OnReceivedExtendedVideoSession(
           unsigned /*role*/,                           ///< role 1-Presentation 2-Live
           const H323ChannelNumber & /*channelnum*/     ///< Channel number of just opened channel
    ) const {};
    
    /** Close an Extended Video Session matching the channel number
        This will close the Extended Video matching the channel number (if open)
     */
    virtual PBoolean CloseExtendedVideoSession(
       const H323ChannelNumber & num     ///< Channel number to close.
    );

    /**Open a channel for use by an extended video codec.
       The H323VideoCodec class will use this function to open the channel to
       read/write image data.

       The default behaviour returns FALSE.
      */
    virtual PBoolean OpenExtendedVideoChannel(
      PBoolean isEncoding,       ///< Direction of data flow
      H323VideoCodec & codec ///< codec doing the opening
    );

    virtual PBoolean SendH239GenericResponse(PBoolean response);

    H245NegLogicalChannels * GetLogicalChannels();
        
#endif // H323_H239

#endif // NO_H323_VIDEO

    /**Close a logical channel.
      */
    virtual void CloseLogicalChannel(
      unsigned number,    ///< Channel number to close.
      PBoolean fromRemote     ///< Indicates close request of remote channel
    );

    /**Close a logical channel by number.
      */
    virtual void CloseLogicalChannelNumber(
      const H323ChannelNumber & number    ///< Channel number to close.
    );

    /**Close a logical channel.
      */
    virtual void CloseAllLogicalChannels(
      PBoolean fromRemote     ///< Indicates close request of remote channel
    );

    /**This function is called when the remote endpoint has closed down
       a logical channel.

       The default behaviour does nothing.
     */
    virtual void OnClosedLogicalChannel(
      const H323Channel & channel   ///< Channel that was closed
    );

    /**This function is called when the remote endpoint request the close of
       a logical channel.

       The application may get an opportunity to refuse to close the channel by
       returning FALSE from this function.

       The default behaviour returns TRUE.
     */
    virtual PBoolean OnClosingLogicalChannel(
      H323Channel & channel   ///< Channel that is to be closed
    );

    /**This function is called when the remote endpoint wishes to limit the
       bit rate being sent on a channel.

       If channel is NULL, then the bit rate limit applies to all channels.

       The default behaviour does nothing if channel is NULL, otherwise calls
       H323Channel::OnFlowControl() on the specific channel.
     */
    virtual void OnLogicalChannelFlowControl(
      H323Channel * channel,   ///< Channel that is to be limited
      long bitRateRestriction  ///< Limit for channel
    );


    /**This function is called when the local endpoint wishes to limit the
       bit rate being sent on a channel.

       If channel is NULL, then the bit rate limit applies to all channels.

       Return true if the FlowControl Command is sent
     */
    virtual PBoolean SendLogicalChannelFlowControl(
        const H323Channel & channel,     ///< Channel that is to be limited
        long restriction                 ///< Limit for channel
    );

    /**This function is called when the remote endpoint indicates the level
       of jitter estimated by the receiver.

       If channel is NULL, then the jitter applies to all channels.

       The default behaviour does nothing if channel is NULL, otherwise calls
       H323Channel::OnJitter() on the specific channel.
     */
    virtual void OnLogicalChannelJitter(
      H323Channel * channel,   ///< Channel that is to be limited
      DWORD jitter,            ///< Estimated received jitter in microseconds
      int skippedFrameCount,   ///< Frames skipped by decodec
      int additionalBuffer     ///< Additional size of video decoder buffer
    );

    /**Send a miscellaneous command on the associated H245 channel.
    */
    void SendLogicalChannelMiscCommand(
      H323Channel & channel,  ///< Channel to send command for
      unsigned command        ///< Command code to send
    );

    /**Get a logical channel.
       Locates the specified channel number and returns a pointer to it.
      */
    H323Channel * GetLogicalChannel(
      unsigned number,    ///< Channel number to get.
      PBoolean fromRemote     ///< Indicates get a remote channel
    ) const;

    /**Find a logical channel.
       Locates a channel give a RTP session ID. Each session would usually
       have two logical channels associated with it, so the fromRemote flag
       bay be used to distinguish which channel to return.
      */
    H323Channel * FindChannel(
      unsigned sessionId,   ///< Session ID to search for.
      PBoolean fromRemote       ///< Indicates the direction of RTP data.
    ) const;
  //@}

  /**@name Bandwidth Management */
  //@{
    /**Get the bandwidth currently used.
       This totals the open channels and returns the total bandwidth used in
       100's of bits/sec
      */
    unsigned GetBandwidthUsed() const;

    /**Request use the available bandwidth in 100's of bits/sec.
       If there is insufficient bandwidth available, FALSE is returned. If
       sufficient bandwidth is available, then TRUE is returned and the amount
       of available bandwidth is reduced by the specified amount.
      */
    PBoolean UseBandwidth(
      unsigned bandwidth,     ///< Bandwidth required
      PBoolean removing           ///< Flag for adding/removing bandwidth usage
    );

#ifdef H323_VIDEO
    virtual void OnSetInitialBandwidth(H323VideoCodec * codec);
#endif

    /**Get the available bandwidth in 100's of bits/sec.
      */
    unsigned GetBandwidthAvailable() const { return bandwidthAvailable; }

    /**Get the Bandwidth requirement for the call in 100's of bits/sec.
      */
    virtual unsigned GetBandwidthRequired() const { return bandwidthAvailable; }

    /**Set the available bandwidth in 100's of bits/sec.
       Note if the force parameter is TRUE this function will close down
       active logical channels to meet the new bandwidth requirement.
      */
    PBoolean SetBandwidthAvailable(
      unsigned newBandwidth,    ///< New bandwidth limit
      PBoolean force = FALSE        ///< Force bandwidth limit
    );

    /**When gatekeeper setting the bandwidth, OnSetBandwidthAvailable
       provides a method to notify the implementor of the bandwidth change
       and how much available bandwidth is available for the call. 
       Note Bandwidth is in bits/sec
      */
    virtual PBoolean OnSetBandwidthAvailable(
      unsigned newBandwidth,    ///< New bandwidth limit
      unsigned available        ///< Remaining bandwidth
      );
  //@}

  /**@name Indications */
  //@{
    enum SendUserInputModes {
      SendUserInputAsQ931,
      SendUserInputAsString,
      SendUserInputAsTone,
      SendUserInputAsInlineRFC2833,
      SendUserInputAsSeparateRFC2833,  // Not implemented
#ifdef H323_H249
      SendUserInputAsNavigation,
      SendUserInputAsSoftkey,
      SendUserInputAsPointDevice,
      SendUserInputAsModal,
#endif
      NumSendUserInputModes
    };
#if PTRACING
    friend ostream & operator<<(ostream & o, SendUserInputModes m);
#endif

    /**Set the user input indication transmission mode.
      */
    void SetSendUserInputMode(SendUserInputModes mode);

    /**Get the user input indication transmission mode.
      */
    SendUserInputModes GetSendUserInputMode() const { return sendUserInputMode; }

    /**Get the real user input indication transmission mode.
       This will return the user input mode that will actually be used for
       transmissions. It will be the value of GetSendUserInputMode() provided
       the remote endpoint is capable of that mode.
      */
    SendUserInputModes GetRealSendUserInputMode() const;

    /**Send a user input indication to the remote endpoint.
       This is for sending arbitrary strings as user indications.

       The user indication is sent according to the sendUserInputMode member
       variable. If SendUserInputAsString then this uses an H.245 "string"
       UserInputIndication pdu sending the entire string in one go. If
       SendUserInputAsTone then a separate H.245 "signal" UserInputIndication
       pdu is sent for each character. If SendUserInputAsInlineRFC2833 then
       the indication is inserted into the outgoing audio stream as an RFC2833
       RTP data pdu.

       SendUserInputAsSeparateRFC2833 is not yet supported.
      */
    virtual void SendUserInput(
      const PString & value                   ///< String value of indication
    );

    /**Call back for remote endpoint has sent user input.
       This will be called irrespective of the source (H.245 string, H.245
       signal or RFC2833).

       The default behaviour calls the endpoint function of the same name.
      */
    virtual void OnUserInputString(
      const PString & value   ///< String value of indication
    );

    /**Send a user input indication to the remote endpoint.
       This sends DTMF emulation user input. If something more sophisticated
       than the simple tones that can be sent using the SendUserInput()
       function.

       A duration of zero indicates that no duration is to be indicated.
       A non-zero logical channel indicates that the tone is to be syncronised
       with the logical channel at the rtpTimestamp value specified.

       The tone parameter must be one of "0123456789#*ABCD!" where '!'
       indicates a hook flash. If tone is a ' ' character then a
       signalUpdate PDU is sent that updates the last tone indication
       sent. See the H.245 specifcation for more details on this.

       The user indication is sent according to the sendUserInputMode member
       variable. If SendUserInputAsString then this uses an H.245 "string"
       UserInputIndication pdu sending the entire string in one go. If
       SendUserInputAsTone then a separate H.245 "signal" UserInputIndication
       pdu is sent for each character. If SendUserInputAsInlineRFC2833 then
       the indication is inserted into the outgoing audio stream as an RFC2833
       RTP data pdu.

       SendUserInputAsSeparateRFC2833 is not yet supported.
      */
    virtual void SendUserInputTone(
      char tone,                   ///< DTMF tone code
      unsigned duration = 0,       ///< Duration of tone in milliseconds
      unsigned logicalChannel = 0, ///< Logical channel number for RTP sync.
      unsigned rtpTimestamp = 0    ///< RTP timestamp in logical channel sync.
    );

    /**Call back for remote enpoint has sent user input.

       The default behaviour calls the endpoint function of the same name.
      */
    virtual void OnUserInputTone(
      char tone,               ///< DTMF tone code
      unsigned duration,       ///< Duration of tone in milliseconds
      unsigned logicalChannel, ///< Logical channel number for RTP sync.
      unsigned rtpTimestamp    ///< RTP timestamp in logical channel sync.
    );

    /**Send a user input indication to the remote endpoint.
       This sends a Hook Flash emulation user input.
      */
    void SendUserInputHookFlash(
      int duration = 500  ///< Duration of tone in milliseconds
    ) { SendUserInputTone('!', duration); }

    /**Send a user input indication to the remote endpoint.
       This is for sending arbitrary strings as user indications.

       This always uses a Q.931 Keypad Information Element in a Information
       pdu sending the entire string in one go.
      */
    virtual void SendUserInputIndicationQ931(
      const PString & value                   ///< String value of indication
    );

    /**Send a user input indication to the remote endpoint.
       This is for sending arbitrary strings as user indications.

       This always uses an H.245 "string" UserInputIndication pdu sending the
       entire string in one go.
      */
    virtual void SendUserInputIndicationString(
      const PString & value                   ///< String value of indication
    );

    /**Send a user input indication to the remote endpoint.
       This sends DTMF emulation user input.This uses an H.245 "signal"
       UserInputIndication pdu.
      */
    virtual void SendUserInputIndicationTone(
      char tone,                   ///< DTMF tone code
      unsigned duration = 0,       ///< Duration of tone in milliseconds
      unsigned logicalChannel = 0, ///< Logical channel number for RTP sync.
      unsigned rtpTimestamp = 0    ///< RTP timestamp in logical channel sync.
    );

#ifdef H323_H249

    /**Send a user input indication to the remote endpoint.
       This sends a H.249 Annex A Navigation user input. 
      */
    virtual void SendUserInputIndicationNavigate(
        H323_UserInputCapability::NavigateKeyID keyID
    );

    /**Send a user input indication to the remote endpoint.
       This Receives/Sends a H.249 Annex A Navigation user input. 
      */
    virtual void OnUserInputIndicationNavigate(
        const H245_ArrayOf_GenericParameter & contents
    );
    
    /**Send a user input indication to the remote endpoint.
       This Receives/Sends a H.249 Annex B Softkey user input. 
      */
    virtual void SendUserInputIndicationSoftkey(
        unsigned key, 
        const PString & keyName = PString()
    );

    virtual void OnUserInputIndicationSoftkey(
        const H245_ArrayOf_GenericParameter & contents
    );

    /**Send a user input indication to the remote endpoint.
       This Receives/Sends a H.249 Annex C Point Device user input. 
      */
    virtual void SendUserInputIndicationPointDevice(
                        unsigned x,              ///< X coord 
                        unsigned y,              ///< Y coord 
                        unsigned button=0,       ///< Mouse Button 1 = left 2 = right
                        unsigned buttonstate=0,  ///< Button state 1 = button down 2 = button up
                        unsigned clickcount=0    ///< ClickCount 1 = sigle click 2= doubleclick
    );

    virtual void OnUserInputIndicationPointDevice(
        const H245_ArrayOf_GenericParameter & contents
    );
    
    /**Send a user input indication to the remote endpoint.
       This Receives/Sends a H.249 Annex D Softkey user input. 
      */
    virtual void SendUserInputIndicationModal();

    virtual void OnUserInputIndicationModal(
        const H245_ArrayOf_GenericParameter & contents
    );
    
#endif


    /**Send a user input indication to the remote endpoint.
       The two forms are for basic user input of a simple string using the
       SendUserInput() function or a full DTMF emulation user input using the
       SendUserInputTone() function.

       An application could do more sophisticated usage by filling in the 
       H245_UserInputIndication structure directly ans using this function.
      */
    virtual void SendUserInputIndication(
      const H245_UserInputIndication & pdu    ///< Full user indication PDU
    );

    /**Call back for remote enpoint has sent user input.
       The default behaviour calls OnUserInputString() if the PDU is of the
       alphanumeric type, or OnUserInputTone() if of a tone type.
      */
    virtual void OnUserInputIndication(
      const H245_UserInputIndication & pdu  ///< Full user indication PDU
    );
  //@}

  /**@name RTP Session Management */
  //@{
    /**Get an RTP session for the specified ID.
       If there is no session of the specified ID, NULL is returned.
      */
    virtual RTP_Session * GetSession(
      unsigned sessionID
    ) const;

    /**Get an H323 RTP session for the specified ID.
       If there is no session of the specified ID, NULL is returned.
      */
    virtual H323_RTP_Session * GetSessionCallbacks(
      unsigned sessionID
    ) const;

    /**Use an RTP session for the specified ID and for the given direction.
       If there is no session of the specified ID, a new one is created using
       the information provided in the H245_TransportAddress PDU. If the system
       does not support the specified transport, NULL is returned.

       If this function is used, then the ReleaseSession() function MUST be
       called or the session is never deleted for the lifetime of the H323
       connection.
      */
    virtual RTP_Session * UseSession(
      unsigned sessionID,
      const H245_TransportAddress & pdu,
      H323Channel::Directions dir,
      RTP_QOS * rtpqos = NULL
    );

    /**Release the session. If the session ID is not being used any more any
       clients via the UseSession() function, then the session is deleted.
     */
    virtual void ReleaseSession(
      unsigned sessionID
    );

    virtual void UpdateSession(
        unsigned oldSessionID, 
        unsigned newSessionID
    );

    /**Received OLC Generic Information. This is used to supply alternate RTP 
       destination information in the generic information field in the OLC for the
       purpose of probing for an alternate route to the remote party.
      */
    virtual PBoolean OnReceiveOLCGenericInformation(unsigned sessionID, 
                        const H245_ArrayOf_GenericInformation & alternate, 
                        PBoolean isAck
                        ) const;

    /**Send Generic Information in the OLC. This is used to include generic
       information in the openlogicalchannel
      */
    virtual PBoolean OnSendingOLCGenericInformation(
                        const unsigned & sessionID,                        ///< Session Information
                        H245_ArrayOf_GenericInformation & gen,            ///< Generic OLC/OLCack message
                        PBoolean isAck
                        ) const;

#ifdef H323_H4609
  /** H.460.9 Call Statistics
    */
    class  H4609Statistics  : public PObject 
    {
     public:
        H4609Statistics();
        H323TransportAddress sendRTPaddr;      // Send RTP Address
        H323TransportAddress recvRTPaddr;      // Receive RTP Address
        H323TransportAddress sendRTCPaddr;     // Send RTCP Address (not used)
        H323TransportAddress recvRTCPaddr;     // Receive RTCP Address (not used)
        unsigned sessionid;
        unsigned meanEndToEndDelay;               // EstimatedEndtoEnd...
        unsigned worstEndToEndDelay;           // EstimatedEndtoEnd...
        unsigned packetsReceived;
        unsigned accumPacketLost;
        unsigned packetLossRate;
        unsigned fractionLostRate;
        unsigned meanJitter;
        unsigned worstJitter;
        unsigned bandwidth;
    };

  /** H.460.9 Queue statistics
    */
  void H4609QueueStats(const RTP_Session & session);

  /** H.460.9 dequeue statistics
    */
  H4609Statistics * H4609DequeueStats();

  /** H.460.9 Enable statistics collection
    */
  void H4609EnableStats();

  /** H.460.9 Statistics only collected at end of call
    */
  void H4609StatsFinal(PBoolean final);

#endif

    /**Callback from the RTP session for statistics monitoring.
       This is called every so many packets on the transmitter and receiver
       threads of the RTP session indicating that the statistics have been
       updated.

       The default behaviour calls H323EndPoint::OnRTPStatistics().
      */
    virtual void OnRTPStatistics(
      const RTP_Session & session   ///< Session with statistics
    ) const;

    /**Callback from the RTP session for statistics monitoring.
       This is called at the end of the RTP session indicating that the statistics 
       of the call

       The default behaviour calls H323EndPoint::OnFinalRTPStatistics().
      */
    virtual void OnRTPFinalStatistics(
      const RTP_Session & session   ///< Session with statistics
    ) const;

    /**Callback from the RTP session for statistics monitoring.
       This is called when a SenderReport is received

       The default behaviour does nothing.
      */
    virtual void OnRxSenderReport(
        unsigned sessionID,
        const RTP_Session::SenderReport & send,
        const RTP_Session::ReceiverReportArray & recv
       ) const;

    /**Get the names of the codecs in use for the RTP session.
       If there is no session of the specified ID, an empty string is returned.
      */
    virtual PString GetSessionCodecNames(
      unsigned sessionID
    ) const;

    /** NAT Detection algorithm
    */
    virtual void NatDetection(const PIPSocket::Address & srcAddress,  ///< TCP socket source address
                              const PIPSocket::Address & sigAddress   ///< H.225 Signalling Address
                              );

    /** Fires when a NAT may of been detected. Return true to activate NAT media
        mechanism
    */
    virtual PBoolean OnNatDetected();

    /** Return TRUE if the remote appears to be behind a NAT firewall
    */
    PBoolean IsBehindNAT() const
    { return remoteIsNAT; }

    /** Set Remote is behind NAT
    */
    void SetRemoteNAT(bool isNat = true)
    { remoteIsNAT = isNat; }

    /** Is NAT Support Available
      */
    PBoolean HasNATSupport() const
    { return NATsupport; }

    /** Disable NAT Support for allocation of RTP sockets
      */
    void DisableNATSupport();
    
    /** Set the information that the call parties are 
        behind the same NAT device
      */
    void SetSameNAT() { sameNAT = true; };

    /** Determine if the two parties are behind the same NAT
      */
    PBoolean isSameNAT() const { return sameNAT; };

#ifdef P_STUN
    /** GetPreferedNatMethod
        returns the NATMethod to use for a call 
        by default calls the H323Endpoint function of the same name
      */
    virtual PNatMethod * GetPreferedNatMethod(const PIPSocket::Address & ip) const;

    virtual PUDPSocket * GetNatSocket(unsigned session, PBoolean rtp);

    /** Set RTP NAT information callback
      */
    virtual void SetRTPNAT(unsigned sessionid, PUDPSocket * _rtp, PUDPSocket * _rtcp);

    /** Set NAT Channel in effect 
      */
    void SetNATChannelActive(unsigned sessionid);

    /** Is NAT Method Active
      */
    PBoolean IsNATMethodActive(unsigned sessionid);

#endif
    /** Set Endpoint Type Information
      Override this to advertise the Endpoint type on a Call by Call basis

      The default behaviour calls H323EndPoint::SetEndpointTypeInfo().
      */
    virtual void SetEndpointTypeInfo(H225_EndpointType & info) const;

  //@}

  /**@name Request Mode Changes */
  //@{
    /**Make a request to mode change to remote.
       This asks the remote system to stop it transmitters and start sending
       one of the combinations specifed.

       The modes are separated in the string by \n characters, and all of the
       channels (capabilities) are strings separated by \t characters. Thus a
       very simple mode change would be "T.38" which requests that the remote
       start sending T.38 data and nothing else. A more complicated example
       would be "G.723\tH.261\nG.729\tH.261\nG.728" which indicates that the
       remote should either start sending G.723 and H.261, G.729 and H.261 or
       just G.728 on its own.

       Returns FALSE if a mode change is currently in progress, only one mode
       change may be done at a time.
      */
    virtual PBoolean RequestModeChange(
      const PString & newModes  ///< New modes to select
    );

    /**Make a request to mode change to remote.
       This asks the remote system to stop it transmitters and start sending
       one of the combinations specifed.

       Returns FALSE if a mode change is currently in progress, only one mode
       change may be done at a time.
      */
    virtual PBoolean RequestModeChange(
      const H245_ArrayOf_ModeDescription & newModes  ///< New modes to select
    );

    /**Received request for mode change from remote.
      */
    virtual PBoolean OnRequestModeChange(
      const H245_RequestMode & pdu,     ///< Received PDU
      H245_RequestModeAck & ack,        ///< Ack PDU to send
      H245_RequestModeReject & reject,  ///< Reject PDU to send
      PINDEX & selectedMode           ///< Which mode was selected
    );

    /**Completed request for mode change from remote.
       This is a call back that accurs after the ack has been sent to the
       remote as indicated by the OnRequestModeChange() return result. This
       function is intended to actually implement the mode change after it
       had been accepted.
      */
    virtual void OnModeChanged(
      const H245_ModeDescription & newMode
    );

    /**Received acceptance of last mode change request.
       This callback indicates that the RequestModeChange() was accepted by
       the remote endpoint.
      */
    virtual void OnAcceptModeChange(
      const H245_RequestModeAck & pdu  ///< Received PDU
    );

    /**Received reject of last mode change request.
       This callback indicates that the RequestModeChange() was accepted by
       the remote endpoint.
      */
    virtual void OnRefusedModeChange(
      const H245_RequestModeReject * pdu  ///< Received PDU, if NULL is a timeout
    );
  //@}

#ifdef H323_T120
  /**@name Other services */
  //@{
    /**Create an instance of the T.120 protocol handler.
       This is called when the OpenLogicalChannel subsystem requires that
       a T.120 channel be established.

       Note that if the application overrides this and returns a pointer to a
       heap variable (using new) then it is the responsibility of the creator
       to subsequently delete the object. The user of this function (the 
       H323_T120Channel class) will not do so.

       The default behavour returns H323Endpoint::CreateT120ProtocolHandler()
       while keeping track of that variable for autmatic deletion.
      */
    virtual OpalT120Protocol * CreateT120ProtocolHandler();
#endif

#ifdef H323_T38
    /**Create an instance of the T.38 protocol handler.
       This is called when the OpenLogicalChannel subsystem requires that
       a T.38 fax channel be established.

       Note that if the application overrides this and returns a pointer to a
       heap variable (using new) then it is the responsibility of the creator
       to subsequently delete the object. The user of this function (the 
       H323_T38Channel class) will not do so.

       The default behavour returns H323Endpoint::CreateT38ProtocolHandler()
       while keeping track of that variable for autmatic deletion.
      */
    virtual OpalT38Protocol * CreateT38ProtocolHandler();

    /**Request a mode change to T.38 data.
      */
    virtual PBoolean RequestModeChangeT38(
      const char * capabilityNames = "T.38\nT38FaxUDP"
    );
#endif

#ifdef H323_H224

    /** Create an instance of the H.224 protocol handler.
        This is called when the subsystem requires that a H.224 channel be established.
          
        Note that if the application overrides this it should return a pointer
        to a heap variable (using new) as it will be automatically deleted when
        the H323Connection is deleted.
     
        The default behaviour calls the OpalEndpoint function of the same name if
        there is not already a H.224 handler associated with this connection. If there
        is already such a H.224 handler associated, this instance is returned instead.
    */
    virtual OpalH224Handler *CreateH224ProtocolHandler(H323Channel::Directions dir, unsigned sessionID);

    /** On Create an instance of the H.224 handler.
        This is called when the subsystem creates a H.224 Handler.

        Use this to derive implementers classes
        return false to not create Handler.

     The default behavour returns H323Endpoint::OnCreateH224Handler()
     */
    virtual PBoolean OnCreateH224Handler(H323Channel::Directions dir, const PString & id, H224_Handler * m_handler) const;

   /** Create an instance of a H.224 protocol handler.

        This is called when the subsystem requires that a H.224 channel be established.

        Note that if the application overrides this it should return a pointer
        to a heap variable (using new) as it will be automatically deleted when
        the associated H.224 handler is deleted.

     The default behavour returns NULL to have the subsystem instance from the H.224 Factory;
     */
    virtual H224_Handler *CreateH224Handler(H323Channel::Directions dir, OpalH224Handler & h224Handler, const PString & id);

#ifdef H224_H281
    /** Create an instance of the H.281 protocol handler.

        *** NOTE This function is depreciated for H323plus 1.26 ***
        *** Use OnCreateH224Handler function call instead ***

        This is called when the subsystem requires that a H.224 channel be established.
          
        Note that if the application overrides this it should return a pointer
        to a heap variable (using new) as it will be automatically deleted when
        the associated H.224 handler is deleted.
          
     The default behavour returns H323Endpoint::CreateH281ProtocolHandler()
     */
    virtual H224_H281Handler *CreateH281ProtocolHandler(OpalH224Handler & h224Handler);
#endif

#endif  // H323_H224

#ifdef H323_T140
    /** Create an instance of the RFC4103 protocol handler.
          
     The default behavour returns H323Endpoint::CreateRFC4103ProtocolHandler()
     */
    virtual H323_RFC4103Handler * CreateRFC4103ProtocolHandler(H323Channel::Directions dir, unsigned sessionID);
#endif // H323_T140

#ifdef H323_FILE
    /** Open an File Transfer Session
        Use this to open a file transfer session for the tranferring of files
        between H323 clients.
    */
     PBoolean OpenFileTransferSession( const H323FileTransferList & list,
                                       H323ChannelNumber & num   ///< Created Channel number
                                      );


    /** Close the File Transfer Session
        Use this to close the file transfer session. 

     */
     PBoolean CloseFileTransferSession(unsigned num  ///< Channel number
                                       );

    /** Create an instance of the File Transfer handler.
        This is called when the subsystem requires that a a file transfer channel be established.
          
        The default behaviour calls the OnCreateFileTransferHandler function of the same name if
        there is not already a file transfer handler associated with this connection. If there
        is already such a file transfer handler associated, this instance is returned instead.
    */
     H323FileTransferHandler *CreateFileTransferHandler(unsigned sessionID,                    ///< Session Identifier
                                                        H323Channel::Directions dir,        ///< direction of channel
                                                        H323FileTransferList & filelist        ///< Transfer File List
                                                        );


    /** OnCreateFileTransferHandler Function

        Override this function and create your own derived FileTransfer Handler
        to collect File Transfer events

        Note that if the application overrides this it should return a pointer
        to a heap variable (using new) as it will be automatically deleted when
        the H323Connection is deleted.
     */
     virtual H323FileTransferHandler *OnCreateFileTransferHandler(unsigned sessionID,        ///< Session Identifier
                                                        H323Channel::Directions dir,        ///< direction of channel
                                                        H323FileTransferList & filelist        ///< Transfer File List
                                                        );

    /** Open a File Transfer Channel.
        This is called when the subsystem requires that a File Transfer channel be established.

        An implementer should override this function to facilitate file transfer. 
        If transmitting, list of files should be populated to notify the channel which files to read.
        If receiving, the list of files should be altered to include path information for the storage
        of received files.
        
        The default behaviour returns FALSE to indicate File Transfer is not implemented. 
      */
      virtual PBoolean OpenFileTransferChannel( PBoolean isEncoder,                        ///< direction of channel
                                                H323FileTransferList & filelist            ///< Transfer File List
                                         ); 
#endif

    /**Get separate H.235 authentication for the connection.
       This allows an individual ARQ to override the authentical credentials
       used in H.235 based RAS for this particular connection.

       A return value of FALSE indicates to use the default credentials of the
       endpoint, while TRUE indicates that new credentials are to be used.

       The default behavour does nothing and returns FALSE.
      */
    virtual PBoolean GetAdmissionRequestAuthentication(
      const H225_AdmissionRequest & arq,  ///< ARQ being constructed
      H235Authenticators & authenticators ///< New authenticators for ARQ
    );
  //@}

  /**@name Member variable access */
  //@{
    /**Get the owner endpoint for this connection.
     */
    H323EndPoint & GetEndPoint() const { return endpoint; }

    /**Get the call direction for this connection.
     */
    PBoolean HadAnsweredCall() const { return callAnswered; }

    /**Determined if connection is gatekeeper routed.
     */
    PBoolean IsGatekeeperRouted() const { return gatekeeperRouted; }

    /**Get the Q.931 cause code (Q.850) that terminated this call.
       See Q931::CauseValues for common values.
     */
    unsigned GetQ931Cause() const { return q931Cause; }

    /**Set the outgoing Q.931 cause code (Q.850) that is sent for this call
       See Q931::CauseValues for common values.
     */
    void SetQ931Cause(unsigned v) { q931Cause = v; }

    /**Get the distinctive ring code for incoming call.
       This returns an integer from 0 to 7 that may indicate to an application
       that different ring cadences are to be used.
      */
    unsigned GetDistinctiveRing() const { return distinctiveRing; }

    /**Set the distinctive ring code for outgoing call.
       This sets the integer from 0 to 7 that will be used in the outgoing
       Setup PDU. Note this must be called either immediately after
       construction or during the OnSendSignalSetup() callback function so the
       member variable is set befor ethe PDU is sent.
      */
    void SetDistinctiveRing(unsigned pattern) { distinctiveRing = pattern&7; }

    /**Get the internal OpenH323 call token for this connection.
     */
    const PString & GetCallToken() const { return callToken; }

    /**Get the call reference for this connection.
     */
    unsigned GetCallReference() const { return callReference; }

    /**Get the call identifier for this connection.
     */
    const OpalGloballyUniqueID & GetCallIdentifier() const { return callIdentifier; }

    /**Get the conference identifier for this connection.
     */
    const OpalGloballyUniqueID & GetConferenceIdentifier() const { return conferenceIdentifier; }

    /**Get the local name/alias.
      */
    const PString & GetLocalPartyName() const { return localPartyName; }

    /**Set the local name/alias from information in the PDU.
      */
    void SetLocalPartyName(const PString & name);

    /**Set the remote name/alias.
      */
    void SetRemotePartyName(const PString & name);

    /**Set the local Display name
      */
    void SetDisplayName(const PString & name) { localDisplayName = name; }

    /**Get the local Display name
      */
    virtual const PString GetDisplayName() const { return localDisplayName; }

	/**Get  Local Alias Name list
	  */
    virtual const PStringList & GetLocalAliasNames() const { return localAliasNames; }

    /**Get the local supported languages
	  */
	const PStringList & GetLocalLanguages() const { return localLanguages; }

    /**Get the remote party name.
       This returns a string indicating the remote parties names and aliases.
       This can be a complicated string containing all the aliases and the
       remote host name. For example:
              "Fred Nurk (fred, 5551234) [fred.nurk.com]"
      */
    virtual const PString & GetRemotePartyName() const { return remotePartyName; }

    /**Get the remote party number, if there was one one.
       If the remote party has indicated an e164 number as one of its aliases.
      */
    const PString & GetRemotePartyNumber() const { return remotePartyNumber; }

    /**Get the remote party address.
       This will return the "best guess" at an address to use in a
       H323EndPoint::MakeCall() function to call the remote party back again.
       Note that due to the presence of gatekeepers/proxies etc this may not
       always be accurate.
      */
    const PString & GetRemotePartyAddress() const { return remotePartyAddress; }

    /**Get the remote party Alias List, if there was one.
      */
    const PStringArray & GetRemotePartyAliases() const { return remoteAliasNames; }

    /**Get the remote party Q931 DisplayName.
       The remote party name as specified in the Q931::Display.
      */
    const PString & GetRemoteQ931Display() const { return remoteQ931Display; }

    /**Get the remote party Q931 Number.
       The remote party number as specified in the Q931
      */
    const PString & GetRemoteQ931Number() const { return remoteQ931Number; }

    /**Set the name/alias of remote end from information in the PDU.
      */
    void SetRemotePartyInfo(
      const H323SignalPDU & pdu ///< PDU from which to extract party info.
    );

    /**Set the remote application for acceptance.
      */
    virtual PBoolean CheckRemoteApplication(
        const H225_EndpointType & pdu ///< PDU from which to extract application info.
    );

    /**Get the remote application name and version.
       This information is obtained from the sourceInfo field of the H.225
       Setup PDU or the destinationInfo of the call proceeding or alerting
       PDU's. The general format of the string will be information extracted
       from the VendorIdentifier field of the EndpointType. In particular:

          productId <tab> versionId <tab> t35CountryCode/manufacturerCode

       for example
          "Equivalence OpenPhone\t1.4.2\t9/61"
      */
    const PString & GetRemoteApplication() const { return remoteApplication; }

    /**Set the name/alias of remote end from information in the PDU.
      */
    virtual void SetRemoteApplication(
      const H225_EndpointType & pdu ///< PDU from which to extract application info.
    );
    
    /**Get the local capability table for this connection.
     */
    const H323Capabilities & GetLocalCapabilities() const { return localCapabilities; }

    /**Get the local capability table for this connection.
     */
    H323Capabilities * GetLocalCapabilitiesRef()  { return &localCapabilities; }

    /**Get the remotes capability table for this connection.
     */
    const H323Capabilities & GetRemoteCapabilities() const { return remoteCapabilities; }

    /**Get the maximum audio jitter delay.
     */
    unsigned GetRemoteMaxAudioDelayJitter() const { return remoteMaxAudioDelayJitter; }

    /**Get the signalling channel being used.
      */
    const H323Transport * GetSignallingChannel() const { return signallingChannel; }

    /**Get the signalling channel protocol version number.
      */
    unsigned GetSignallingVersion() const { return h225version; }

    /**Get the control channel being used (may return signalling channel).
      */
    const H323Transport & GetControlChannel() const;

    /**Get the control channel protocol version number.
      */
    unsigned GetControlVersion() const { return h245version; }

    /**Get the time at which the connection was begun
      */
    PTime GetSetupUpTime() const { return setupTime; }

    /**Get the time at which the ALERTING was received
      */
    PTime GetAlertingTime() const { return alertingTime; }

    /**Get the time at which the connection was connected. That is the point
       at which charging is likely to have begun.
      */
    PTime GetConnectionStartTime() const { return connectedTime; }

    /**Get the time at which the connection was cleared
      */
    PTime GetConnectionEndTime() const { return callEndTime; }

    /**Get the time at which the remote opened a media channel
      */
    PTime GetReverseMediaOpenTime() const { return reverseMediaOpenTime; }

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

    /**Get the UUIE PDU monitor bit mask.
     */
    unsigned GetUUIEsRequested() const { return uuiesRequested; }

    /**Set the UUIE PDU monitor bit mask.
     */
    void SetUUIEsRequested(unsigned mask) { uuiesRequested = mask; }

    /**Get the iNow Gatekeeper Access Token OID.
     */
    const PString GetGkAccessTokenOID() const { return gkAccessTokenOID; }

    /**Set the iNow Gatekeeper Access Token OID.
     */
    void SetGkAccessTokenOID(const PString & oid) { gkAccessTokenOID = oid; }

    /**Get the iNow Gatekeeper Access Token data.
     */
    const PBYTEArray & GetGkAccessTokenData() const { return gkAccessTokenData; }

    /**Set the Destionation Extra Call Info memeber.
     */
    void SetDestExtraCallInfo(
      const PString & info
    ) { destExtraCallInfo = info; }

    /** Set the remote call waiting flag
     */
    void SetRemoteCallWaiting(const unsigned value) { remoteCallWaiting = value; }

    /**How many caller's are waiting on the remote endpoint?
      -1 - None
       0 - Just this connection
       n - n plus this connection
     */
    int GetRemoteCallWaiting() const { return remoteCallWaiting; }

    /**Set the enforced duration limit for the call.
       This starts a timer that will automatically shut down the call when it
       expires.
      */
    void SetEnforcedDurationLimit(
      unsigned seconds  ///< max duration of call in seconds
    );

#ifdef P_STUN

    /**Session Information 
        This contains session information which is passed to the socket handler
        when creating RTP socket pairs.
      */
    class SessionInformation : public PObject
    {
        public:
            SessionInformation(const OpalGloballyUniqueID & id, unsigned crv, const PString & token, unsigned session, const H323Connection * connection);

            unsigned GetCallReference();

            const PString & GetCallToken();

            unsigned GetSessionID() const;

            const OpalGloballyUniqueID & GetCallIdentifer();

            void SetSendMultiplexID(unsigned id);

            unsigned GetRecvMultiplexID() const;

            const PString & GetCUI();

            const H323Connection * GetConnection(); 

        protected:
            OpalGloballyUniqueID m_callID;
            unsigned m_crv;
            PString m_callToken;
            unsigned m_sessionID;
            unsigned m_recvMultiID;
            unsigned m_sendMultiID;
            PString m_CUI;
            const H323Connection * m_connection;

    };

    SessionInformation * BuildSessionInformation(unsigned sessionID) const;

    class NAT_Sockets 
    {
      public:
        NAT_Sockets() 
        { rtp = NULL; rtcp = NULL; isActive = false; }

        PUDPSocket * rtp;
        PUDPSocket * rtcp;
        PBoolean     isActive;
    };

#endif

    /** Called when an endpoint receives a SETUP PDU with a
        conference goal of "callIndependentSupplementaryService"
      
        The default behaviour is to return FALSE, which will close the connection
     */
    virtual PBoolean OnSendCallIndependentSupplementaryService(
      H323SignalPDU & pdu                 ///< PDU message
    ) const;

    virtual PBoolean OnReceiveCallIndependentSupplementaryService(
      const H323SignalPDU & pdu                 ///< PDU message
    );

    virtual PBoolean OnSendFeatureSet(unsigned, H225_FeatureSet &, PBoolean) const;

    virtual void OnReceiveFeatureSet(unsigned, const H225_FeatureSet &, PBoolean = false) const;

    /** On resolving H.245 Address conflict
      */
    virtual PBoolean OnH245AddressConflict();

#ifdef H323_H460
    /** Disable the feature set as the remote does not support it.
      */
    void DisableFeatureSet(int) const;

    /** Disable Feautures on a call by call basis
      */
    void DisableFeatures(PBoolean disable = true);

#ifdef H323_H46018
    /** Call to set the direction of call establishment
      */
    void H46019SetCallReceiver();

    /** Enable H46019 for this call
      */
    void H46019Enabled();

    /** Is H46019 enabled for this call
      */
    PBoolean IsH46019Enabled() const;

    /** Enable H46019 Multiplexing for this call
      */
    void H46019MultiEnabled();

    /** Is H46019 Multiplexing enabled for this call
      */
    PBoolean IsH46019Multiplexed() const;

    /** Is H46019 offload enabled for this call
      */
    void H46019SetOffload();
#endif

#ifdef H323_H46024A
    /** Enable H46024A for this call
      */
    void H46024AEnabled();

    /** send H46024A notification
      */
    PBoolean SendH46024AMessage(bool sender);

    /** received H46024A notification
      */
    PBoolean ReceivedH46024AMessage(bool toStart);

#endif

#ifdef H323_H46024B
    /** Enable H46024B for this call
      */
    void H46024BEnabled();
#endif

#ifdef H323_H46026
    /** Set Media Tunneled for this call
      */
    void H46026SetMediaTunneled();

    /** Is Media Tunneled
      */
    PBoolean H46026IsMediaTunneled();
#endif

#ifdef H323_H460COM
    /** On Received remote vendor Information
      */
    virtual void OnRemoteVendorInformation(const PString & product, const PString & version);
#endif

#ifdef H323_H461
    enum ASSETCallType {
        e_h461NormalCall,
        e_h461EndpointCall,
        e_h461AssetCall,
        e_h461Associate
    };

    class H461MessageInfo {
    public:
        H461MessageInfo();

        int         m_message;
        PString     m_assocToken;
        PString     m_callToken;
        int         m_applicationID;
        PString     m_invokeToken;
        PString     m_aliasAddress;
        bool        m_approved;
    };
    H461MessageInfo & GetH461MessageInfo();

    void SetH461MessageInfo(int type, const PString & assocCallToken = PString(), const PString & assocCallIdentifier = PString(), int applicationID = -1,
                            const PString & invokeToken = PString(), const PString & aliasAddress = PString(), bool approved = false);

    void SetH461Mode(ASSETCallType mode);
    ASSETCallType GetH461Mode() const;
#endif

#endif  // H323_H460
  //@}

  /**@name Endpoint Authentication */
  //@{
     /** Get Endpoint Authenticator mothods
       */
    const H235Authenticators & GetEPAuthenticators() const;

    /** Set Authentication to support Validation CallBack
    OnCallAuthentication is called when a EPAuthentication CryptoToken 
    is Validated.
      */
    virtual void SetAuthenticationConnection();

    /** EP Authentication CallBack to check username and get Password
    for Call Authentication. By Default it calls the corresponding
    Endpoint Function. 
      */
    virtual PBoolean OnCallAuthentication(const PString & username, 
                                      PString & password);

    /** EP Authentication CallBack to allow the connection to approve the authentication
       even if it has failed. Use this to override authentication on a call by call basis
       Return TRUE to Authenticate.
      */
    virtual PBoolean OnEPAuthenticationFailed(H235Authenticator::ValidationResult result) const;

    /** EP Authentication Finalise Callback. After the PDU has been built this
        Callback allows the Authentication mechanisms to finalise the PDU.
      */
    virtual void OnAuthenticationFinalise(unsigned pdu,PBYTEArray & rawData);

    /** Whether the Call has Authentication
      */
    PBoolean HasAuthentication() const
       { return hasAuthentication; }

    /** Whether the Authentication has Failed
      */
    PBoolean HasAuthenticationFailed() 
       { return AuthenticationFailed; };
  //@}

#ifdef H323_H235
  /**@name Media Encryption */
  //@{
    /** EnableCallMediaEncryption
        Enable media encryption for this call.
        Override this to decide whether media encryption is to be used for this call.
     */
    virtual PBoolean EnableCallMediaEncryption() const { return true; }

    /** On Media Encryption
        Fires when an encryption media session negotiated
        Fires for each encrypted media session direction
        Default calls H323Endpoint::OnMediaEncryption
     */
    virtual void OnMediaEncryption(unsigned session, H323Channel::Directions dir, const PString & cipher);
  //@}
#endif

  /**@name Transport Security */
  //@{
    /** Set Signalling Security
     */
    virtual void SetTransportSecurity(const H323TransportSecurity & m_callSecurity);

    /** Get Transport Security
     */
    const H323TransportSecurity & GetTransportSecurity() const;
  //@}

#ifdef H323_H248
  /**@name Call Service Control Session */
  //@{
    /** On Send Service Control Session
      */
    PBoolean OnSendServiceControlSessions(
             H225_ArrayOf_ServiceControlSession & serviceControl, ///< Service control PDU
                 H225_ServiceControlSession_reason reason           ///< Reason for Service Control
        ) const;

    /** On Receive Service Control Session
      */
    void OnReceiveServiceControlSessions(
        const H225_ArrayOf_ServiceControlSession & serviceControl  ///< Service control PDU
        );

    /** On Send Call Credit 
      */
    virtual void OnReceiveServiceControl(const PString & amount,    ///< Current Balance 
                                         PBoolean credit,               ///< Debit or Credit
                                         const unsigned & timelimit,///< Time Remaining
                                         const PString & url,        ///< url for TopUp
                                          const PString & ldapURL,   ///< LDAP URL
                                         const PString & baseDN     ///< LDAP base DN
                                        );

    /** On Receive Call Credit
     */
    virtual PBoolean OnSendServiceControl(PString & amount,          ///< Current Balance 
                                      PBoolean credit,               ///< Debit or Credit
                                      unsigned & timelimit,      ///< Time Remaining
                                      PString & url         ///< url for TopUp
                                     ) const;
  //@}
#endif

  /**@name Feature Related Function */
  //@{
    /** Disable H245 in Setup
      */
    virtual void DisableH245inSETUP();

    /** Disable QoS in H.245
      */
    virtual void DisableH245QoS();

    /** Has QoS in H.245
      */
    virtual PBoolean H245QoSEnabled() const;


    /** Set the connection as a non Standard Call
      */
    virtual void SetNonCallConnection();

    /** Is a Non-Call related connection like text messaging or file transfer
      */
    virtual PBoolean IsNonCallConnection() const;

#ifdef H323_H460
    /** Get the connection FeatureSet
      */
    virtual H460_FeatureSet * GetFeatureSet();

    /** Get the connection FeatureSet
      */
    PBoolean FeatureSetSupportNonCallService(const H225_FeatureSet & fs) const;
#endif
  //@}

    enum ReleaseSequence {
      ReleaseSequenceUnknown,
      ReleaseSequence_Local,
      ReleaseSequence_Remote
    };

    ReleaseSequence GetReleaseSequence() const
    { return releaseSequence; }

    virtual PBoolean OnHandleConferenceRequest(const H245_ConferenceRequest &)
    { return FALSE; }

    virtual PBoolean OnHandleConferenceResponse(const H245_ConferenceResponse &)
    { return FALSE; }

    virtual PBoolean OnHandleConferenceCommand(const H245_ConferenceCommand &)
    { return FALSE; }

    virtual PBoolean OnHandleConferenceIndication(const H245_ConferenceIndication &)
    { return FALSE; }

    enum h245MessageType {
      h245request,
      h245response,
      h245command,
      h245indication
    };

    virtual PBoolean OnHandleH245GenericMessage(h245MessageType type, const H245_GenericMessage & pdu);

    virtual PBoolean OnReceivedGenericMessage(h245MessageType, const PString & );
    virtual PBoolean OnReceivedGenericMessage(h245MessageType type, const PString & id, const H245_ArrayOf_GenericParameter & content);

#ifdef H323_H230
    /** Open Conference Controls
      */
    PBoolean OpenConferenceControlSession(
                        PBoolean & chairControl,
                        PBoolean & extControls
                        );


#endif

#ifdef H323_FRAMEBUFFER
    void EnableVideoFrameBuffer(PBoolean enable); 
    PBoolean HasVideoFrameBuffer();
#endif

    /** Whether the current connection is a maintained Signalling connection
      */
    PBoolean IsMaintainedConnection() const;

  protected:
    /**Internal function to check if call established.
       This checks all the criteria for establishing a call an initiating the
       starting of media channels, if they have not already been started via
       the fast start algorithm.
    */
    virtual void InternalEstablishedConnectionCheck();
    PBoolean DecodeFastStartCaps(const H225_ArrayOf_PASN_OctetString & fastStartCaps);
    PBoolean InternalEndSessionCheck(PPER_Stream & strm);
    void SetRemoteVersions(const H225_ProtocolIdentifier & id);
    void MonitorCallStatus();
    PDECLARE_NOTIFIER(OpalRFC2833Info, H323Connection, OnUserInputInlineRFC2833);
    PDECLARE_NOTIFIER(H323Codec::FilterInfo, H323Connection, OnUserInputInBandDTMF);

    H323EndPoint & endpoint;
    PSyncPoint     * endSync;

    int                  remoteCallWaiting; // Number of call's waiting at the remote endpoint
    PBoolean                 callAnswered;
    PBoolean                 gatekeeperRouted;
    unsigned             distinctiveRing;
    PString              callToken;
    unsigned             callReference;
    OpalGloballyUniqueID callIdentifier;
    OpalGloballyUniqueID conferenceIdentifier;

    PStringList        localAliasNames;
    PString            localPartyName;
    PString            localDisplayName;
    PStringList	       localLanguages;
#ifdef H323_H235
    H235Capabilities   localCapabilities; // Capabilities local system supports
#else
    H323Capabilities   localCapabilities; // Capabilities local system supports
#endif
    PString            remotePartyName;
    PString            remotePartyNumber;
    PString            remoteQ931Display;
    PString            remoteQ931Number;
    PBoolean           useQ931Display;
    PString            remotePartyAddress;
    PStringArray       remoteAliasNames;
	PStringArray       remoteLanguages;
    PString            destExtraCallInfo;
    PString            remoteApplication;
    H323Capabilities   remoteCapabilities; // Capabilities remote system supports
    unsigned           remoteMaxAudioDelayJitter;
    PTimer             roundTripDelayTimer;
    unsigned           minAudioJitterDelay;
    unsigned           maxAudioJitterDelay;
    unsigned           bandwidthAvailable;
    unsigned           uuiesRequested;
    PString            gkAccessTokenOID;
    PBYTEArray         gkAccessTokenData;
    PBoolean           addAccessTokenToSetup;
    SendUserInputModes sendUserInputMode;

    H323Transport * signallingChannel;
    PMutex            signallingMutex;
    H323Transport * controlChannel;
    PMutex            controlMutex;
    PBoolean            h245Tunneling;
    H323SignalPDU * h245TunnelRxPDU;
    H323SignalPDU * h245TunnelTxPDU;
    H323SignalPDU * alertingPDU;
    H323SignalPDU * connectPDU;

    enum ConnectionStates {
      NoConnectionActive,
      AwaitingGatekeeperAdmission,
      AwaitingTransportConnect,
      AwaitingSignalConnect,
      AwaitingLocalAnswer,
      HasExecutedSignalConnect,
      EstablishedConnection,
      ShuttingDownConnection,
      NumConnectionStates
    } connectionState;

    PTime         setupTime;
    PTime         alertingTime;
    PTime         connectedTime;
    PTime         callEndTime;
    PTime         reverseMediaOpenTime;
    PInt64        noMediaTimeOut;
    PInt64        roundTripDelayRate;
    CallEndReason callEndReason;
    unsigned      q931Cause;
    ReleaseSequence releaseSequence;

    unsigned   h225version;
    unsigned   h245version;
    PBoolean       h245versionSet;
    PBoolean doH245inSETUP;
    PBoolean lastPDUWasH245inSETUP;
    PBoolean detectInBandDTMF;
    PBoolean rfc2833InBandDTMF;
    PBoolean extendedUserInput;
    PBoolean mustSendDRQ;
    PBoolean mediaWaitForConnect;
    PBoolean transmitterSidePaused;
    PBoolean earlyStart;
    PBoolean doH245QoS;
    PBoolean enableMERAHack;

#ifdef H323_T120
    PBoolean startT120;
#endif

#ifdef H323_T38
    PString    t38ModeChangeCapabilities;
#endif

#ifdef H323_H224
    PBoolean startH224;
#endif

    PSyncPoint digitsWaitFlag;
    PBoolean       endSessionNeeded;
    PBoolean       endSessionSent;
    PSyncPoint endSessionReceived;
    PTimer     enforcedDurationLimit;

#ifdef H323_H450
    // Used as part of a local call hold operation involving MOH
    PChannel * holdAudioMediaChannel;
    PChannel * holdVideoMediaChannel;
    PBoolean       isConsultationTransfer;

    /** Call Intrusion flag and parameters */
    PBoolean     isCallIntrusion;
    unsigned callIntrusionProtectionLevel;

    /** MWI parameters */
    MWIInformation mwiInformation;
#endif

    RTP_SessionManager rtpSessions;

    enum FastStartStates {
      FastStartDisabled,
      FastStartInitiate,
      FastStartResponse,
      FastStartAcknowledged,
      NumFastStartStates
    };
    FastStartStates        fastStartState;
    H323LogicalChannelList fastStartChannels;

#if PTRACING
    static const char * const ConnectionStatesNames[NumConnectionStates];
    friend ostream & operator<<(ostream & o, ConnectionStates s) { return o << ConnectionStatesNames[s]; }
    static const char * const FastStartStateNames[NumFastStartStates];
    friend ostream & operator<<(ostream & o, FastStartStates s) { return o << FastStartStateNames[s]; }
#endif


    // The following pointers are to protocol procedures, they are pointers to
    // hide their complexity from the H323Connection classes users.
    H245NegMasterSlaveDetermination  * masterSlaveDeterminationProcedure;
    H245NegTerminalCapabilitySet     * capabilityExchangeProcedure;
    H245NegLogicalChannels           * logicalChannels;
    H245NegRequestMode               * requestModeProcedure;
    H245NegRoundTripDelay            * roundTripDelayProcedure;

#ifdef H323_H450
    H450xDispatcher                  * h450dispatcher;
    H4502Handler                     * h4502handler;
    H4503Handler                     * h4503handler;
    H4504Handler                     * h4504handler;
    H4506Handler                     * h4506handler;
    H45011Handler                    * h45011handler;
#endif

    OpalRFC2833                      * rfc2833handler;

#ifdef H323_T120
    OpalT120Protocol                 * t120handler;
#endif

#ifdef H323_T38
    OpalT38Protocol                  * t38handler;
#endif

#ifdef P_DTMF
    // The In-Band DTMF detector. This is used inside an audio filter which is
    // added to the audio channel.
    PDTMFDecoder                     dtmfDecoder;
    PString                          dtmfTones;
#endif

    // used to detect remote NAT endpoints
    PBoolean remoteIsNAT;    ///< Remote Caller is NAT
    PBoolean NATsupport;     ///< Disable support for NATed callers
    PBoolean sameNAT;        ///< Call parties are behind the same NAT

    PBoolean AuthenticationFailed;
    PBoolean hasAuthentication;
    const H235Authenticators  EPAuthenticators;

#ifdef H323_AEC
    H323Aec * aec;
#endif

    PBoolean nonCallConnection; 

  private:
    PChannel * SwapHoldMediaChannels(PChannel * newChannel,unsigned sessionId);

    PTimedMutex outerMutex;
    PMutex innerMutex;

  public:
    PBoolean StartHandleControlChannel();
    virtual PBoolean OnStartHandleControlChannel();
    void EndHandleControlChannel();

#ifdef H323_RTP_AGGREGATE
  private:
    PBoolean useRTPAggregation;
#endif

#ifdef H323_SIGNAL_AGGREGATE
  public:
    void AggregateSignalChannel(H323Transport * transport);
    void AggregateControlChannel(H323Transport * transport);
  protected:
    PBoolean useSignallingAggregation;
    H323AggregatedH2x5Handle * signalAggregator;
    H323AggregatedH2x5Handle * controlAggregator;
#endif

#ifdef H323_H248
    H323Dictionary<POrdinalKey, H323ServiceControlSession> serviceControlSessions;
#endif

    PBoolean m_maintainConnection;

#ifdef H323_H460
    PBoolean disableH460;
    H460_FeatureSet       * features;

#ifdef H323_H4609
    PBoolean m_h4609enabled;
    PBoolean m_h4609Final;
    PQueue<H4609Statistics> m_h4609Stats;
#endif

#ifdef H323_H46018
    PBoolean m_H46019CallReceiver;
    PBoolean m_H46019enabled;
    PBoolean m_H46019multiplex;
    PBoolean m_H46019offload;
    PBoolean m_h245Connect;
#endif

#ifdef H323_H46024A
    PBoolean m_H46024Aenabled;
    PBoolean m_H46024Ainitator;
    PINDEX m_H46024Astate;
#endif

#ifdef H323_H46024B
    PBoolean m_H46024Benabled;
    PINDEX m_H46024Bstate;
#endif

#ifdef H323_H46026
    PBoolean m_H46026enabled;
#endif

#ifdef H323_H461
    ASSETCallType m_H461Mode;
    H461MessageInfo m_H461Info;
#endif
#endif

#ifdef P_STUN
    PMutex NATSocketMutex;
    std::map<unsigned,NAT_Sockets> m_NATSockets;
#endif

    H323TransportSecurity m_transportSecurity;

};


H323LIST(H323ConnectionList, H323Connection);
H323DICTIONARY(H323ConnectionDict, PString, H323Connection);
H323DICTIONARY(H323CallIdentityDict, PString, H323Connection);


#endif // __OPAL_H323CON_H


/////////////////////////////////////////////////////////////////////////////
