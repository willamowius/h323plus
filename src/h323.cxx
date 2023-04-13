/*
 * h323.cxx
 *
 * H.323 protocol handler
 *
 * H323plus Library
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
 * Contributor(s): ______________________________________.
 *
 * $Id$

 */

#include <ptlib.h>


#ifdef __GNUC__
#pragma implementation "h323con.h"
#endif

#include "h323con.h"

#include "h323ep.h"
#include "h323neg.h"
#include "h323rtp.h"

#ifdef H323_H450
#include "h450/h4501.h"
#include "h450/h4503.h"
#include "h450/h4504.h"
#include "h450/h45011.h"
#include "h450/h450pdu.h"
#endif

#ifdef H323_H460
#include "h460/h460.h"
#include "h460/h4601.h"

#ifdef H323_H46018
#include "h460/h460_std18.h"
#include "h460/h46018_h225.h"
#include "h460/h46019.h"
#endif

#ifdef H323_H46024B
#include "h460/h46024b.h"
#endif
#endif

#ifdef P_STUN
#include <ptclib/random.h>
#endif

#include "gkclient.h"
#include "rfc2833.h"

#ifdef H323_T120
#include "t120proto.h"
#endif

#ifdef H323_T38
#include "t38proto.h"
#endif

#ifdef H323_H224
#include "h323h224.h"
#endif

#ifdef H323_FILE
#include "h323filetransfer.h"
#endif

#ifdef H323_AEC
#include <etc/h323aec.h>
#endif

#include "h235auth.h"

const PTimeInterval MonitorCallStatusTime(0, 10); // Seconds

#define new PNEW

#ifdef H323_SIGNAL_AGGREGATE

class AggregatedH225Handle : public H323AggregatedH2x5Handle
{
//  PCLASSINFO(AggregatedH225Handle, H323AggregatedH2x5Handle)
  public:
    AggregatedH225Handle(H323Transport & _transport, H323Connection & _connection)
      : H323AggregatedH2x5Handle(_transport, _connection)
    {
    }

    ~AggregatedH225Handle()
    {
    }

    PBoolean OnRead()
    {
      PBoolean ret = H323AggregatedH2x5Handle::OnRead();
      if (connection.controlChannel == NULL)
        connection.MonitorCallStatus();
      return ret;
    }

    PBoolean HandlePDU(PBoolean ok, PBYTEArray & dataPDU)
    {
      H323SignalPDU pdu;
      if (ok && dataPDU.GetSize() > 0)
        ok = pdu.ProcessReadData(transport, dataPDU);
      return connection.HandleReceivedSignalPDU(ok, pdu);
    }

    void OnClose()
    {  DeInit(); }

    void DeInit()
    {
      if (connection.controlChannel == NULL) {
        connection.endSessionReceived.Signal();
      }
    }
};

class AggregatedH245Handle : public H323AggregatedH2x5Handle
{
//  PCLASSINFO(AggregatedH245Handle, H323AggregatedH2x5Handle)
  public:
    AggregatedH245Handle(H323Transport & _transport, H323Connection & _connection)
      : H323AggregatedH2x5Handle(_transport, _connection)
    {
    }

    ~AggregatedH245Handle()
    {
    }

    PBoolean OnRead()
    {
      PBoolean ret = H323AggregatedH2x5Handle::OnRead();
      connection.MonitorCallStatus();
      return ret;
    }

    PBoolean HandlePDU(PBoolean ok, PBYTEArray & pdu)
    {
      PPER_Stream strm(pdu);
      return connection.HandleReceivedControlPDU(ok, strm);
    }

    void OnClose()
    {
      PPER_Stream strm;
      connection.HandleReceivedControlPDU(FALSE, strm);
    }

    void DeInit()
    {
      connection.EndHandleControlChannel();
    }
};

#endif

/////////////////////////////////////////////////////////////////////////////

#if PTRACING
ostream & operator<<(ostream & o, H323Connection::CallEndReason r)
{
  static const char * const CallEndReasonNames[H323Connection::NumCallEndReasons] = {
    "EndedByLocalUser",         /// Local endpoint application cleared call
    "EndedByNoAccept",          /// Local endpoint did not accept call OnIncomingCall()=FALSE
    "EndedByAnswerDenied",      /// Local endpoint declined to answer call
    "EndedByRemoteUser",        /// Remote endpoint application cleared call
    "EndedByRefusal",           /// Remote endpoint refused call
    "EndedByNoAnswer",          /// Remote endpoint did not answer in required time
    "EndedByCallerAbort",       /// Remote endpoint stopped calling
    "EndedByTransportFail",     /// Transport error cleared call
    "EndedByConnectFail",       /// Transport connection failed to establish call
    "EndedByGatekeeper",        /// Gatekeeper has cleared call
    "EndedByNoUser",            /// Call failed as could not find user (in GK)
    "EndedByNoBandwidth",       /// Call failed as could not get enough bandwidth
    "EndedByCapabilityExchange",/// Could not find common capabilities
    "EndedByCallForwarded",     /// Call was forwarded using FACILITY message
    "EndedBySecurityDenial",    /// Call failed a security check and was ended
    "EndedByLocalBusy",         /// Local endpoint busy
    "EndedByLocalCongestion",   /// Local endpoint congested
    "EndedByRemoteBusy",        /// Remote endpoint busy
    "EndedByRemoteCongestion",  /// Remote endpoint congested
    "EndedByUnreachable",       /// Could not reach the remote party
    "EndedByNoEndPoint",        /// The remote party is not running an endpoint
    "EndedByHostOffline",       /// The remote party host off line
    "EndedByTemporaryFailure",  /// The remote failed temporarily app may retry
    "EndedByQ931Cause",         /// The remote ended the call with unmapped Q.931 cause code
    "EndedByDurationLimit",     /// Call cleared due to an enforced duration limit
    "EndedByInvalidConferenceID", /// Call cleared due to invalid conference ID
    "EndedByOSPRefusal"         // Call cleared as OSP server unable or unwilling to route
  };

  if ((PINDEX)r >= PARRAYSIZE(CallEndReasonNames))
    o << "InvalidCallEndReason<" << (unsigned)r << '>';
  else if (CallEndReasonNames[r] == NULL)
    o << "CallEndReason<" << (unsigned)r << '>';
  else
    o << CallEndReasonNames[r];
  return o;
}


ostream & operator<<(ostream & o, H323Connection::AnswerCallResponse s)
{
  static const char * const AnswerCallResponseNames[H323Connection::NumAnswerCallResponses] = {
    "AnswerCallNow",
    "AnswerCallDenied",
    "AnswerCallPending",
    "AnswerCallDeferred",
    "AnswerCallAlertWithMedia",
    "AnswerCallDeferredWithMedia",
    "AnswerCallNowWithAlert"
  };
  if ((PINDEX)s >= PARRAYSIZE(AnswerCallResponseNames))
    o << "InvalidAnswerCallResponse<" << (unsigned)s << '>';
  else if (AnswerCallResponseNames[s] == NULL)
    o << "AnswerCallResponse<" << (unsigned)s << '>';
  else
    o << AnswerCallResponseNames[s];
  return o;
}


ostream & operator<<(ostream & o, H323Connection::SendUserInputModes m)
{
  static const char * const SendUserInputModeNames[H323Connection::NumSendUserInputModes] = {
    "SendUserInputAsQ931",
    "SendUserInputAsString",
    "SendUserInputAsTone",
    "SendUserInputAsRFC2833",
    "SendUserInputAsSeparateRFC2833"
#ifdef H323_H249
    ,"SendUserInputAsNavigation",
    "SendUserInputAsSoftkey",
    "SendUserInputAsPointDevice",
    "SendUserInputAsModal"
#endif
  };

  if ((PINDEX)m >= PARRAYSIZE(SendUserInputModeNames))
    o << "InvalidSendUserInputMode<" << (unsigned)m << '>';
  else if (SendUserInputModeNames[m] == NULL)
    o << "SendUserInputMode<" << (unsigned)m << '>';
  else
    o << SendUserInputModeNames[m];
  return o;
}


const char * const H323Connection::ConnectionStatesNames[NumConnectionStates] = {
  "NoConnectionActive",
  "AwaitingGatekeeperAdmission",
  "AwaitingTransportConnect",
  "AwaitingSignalConnect",
  "AwaitingLocalAnswer",
  "HasExecutedSignalConnect",
  "EstablishedConnection",
  "ShuttingDownConnection"
};

const char * const H323Connection::FastStartStateNames[NumFastStartStates] = {
  "FastStartDisabled",
  "FastStartInitiate",
  "FastStartResponse",
  "FastStartAcknowledged"
};
#endif
#ifdef H323_H460
static bool ReceiveSetupFeatureSet(const H323Connection * connection, const H225_Setup_UUIE & pdu, bool nonCall = false)
{
    H225_FeatureSet fs;
    PBoolean hasFeaturePDU = FALSE;

    if (pdu.HasOptionalField(H225_Setup_UUIE::e_neededFeatures)) {
        fs.IncludeOptionalField(H225_FeatureSet::e_neededFeatures);
        H225_ArrayOf_FeatureDescriptor & fsn = fs.m_neededFeatures;
        fsn = pdu.m_neededFeatures;
        hasFeaturePDU = TRUE;
    }

    if (pdu.HasOptionalField(H225_Setup_UUIE::e_desiredFeatures)) {
        fs.IncludeOptionalField(H225_FeatureSet::e_desiredFeatures);
        H225_ArrayOf_FeatureDescriptor & fsn = fs.m_desiredFeatures;
        fsn = pdu.m_desiredFeatures;
        hasFeaturePDU = TRUE;
    }

    if (pdu.HasOptionalField(H225_Setup_UUIE::e_supportedFeatures)) {
        fs.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);
        H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
        fsn = pdu.m_supportedFeatures;
        hasFeaturePDU = TRUE;
    }

    if (hasFeaturePDU && (!nonCall || connection->FeatureSetSupportNonCallService(fs))) {
        connection->OnReceiveFeatureSet(H460_MessageType::e_setup, fs);
        return true;

    } else if (!nonCall)
        connection->DisableFeatureSet(H460_MessageType::e_setup);

    return false;
}

template <typename PDUType>
static void ReceiveFeatureData(const H323Connection * connection, unsigned code, const PDUType & pdu)
{
    if (pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_genericData)) {
        H225_FeatureSet fs;
        fs.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);
        H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
        const H225_ArrayOf_GenericData & data = pdu.m_h323_uu_pdu.m_genericData;
        for (PINDEX i=0; i < data.GetSize(); i++) {
             PINDEX lastPos = fsn.GetSize();
             fsn.SetSize(lastPos+1);
             fsn[lastPos] = (H225_FeatureDescriptor &)data[i];
        }
        connection->OnReceiveFeatureSet(code, fs, true);
    }
}
#endif

template <typename PDUType>
static void ReceiveFeatureSet(const H323Connection * connection, unsigned code, const PDUType & pdu)
{
    if (pdu.HasOptionalField(PDUType::e_featureSet))
      connection->OnReceiveFeatureSet(code, pdu.m_featureSet);
}

template <typename PDUType>
static PBoolean ReceiveAuthenticatorPDU(const H323Connection * connection,
                                   const PDUType & pdu, unsigned code)
{

PBoolean AuthResult = FALSE;
H235Authenticators authenticators = connection->GetEPAuthenticators();
PBYTEArray strm;

  if (!pdu.HasOptionalField(PDUType::e_tokens) && !pdu.HasOptionalField(PDUType::e_cryptoTokens)) {
        PTRACE(2, "H235EP\tReceived unsecured EPAuthentication message (no crypto tokens),"
            " expected one of:\n" << setfill(',') << connection->GetEPAuthenticators() << setfill(' '));
#ifdef H323_H235
        if (H235Authenticators::GetEncryptionPolicy() == 2) {
           PTRACE(2, "H235EP\tCall rejected due to Media Encryption Policy!");
           return false;
        }
#endif
       return connection->OnEPAuthenticationFailed(H235Authenticator::e_Absent);
  } else {

    H235Authenticator::ValidationResult result = authenticators.ValidateSignalPDU(code,
                                                    pdu.m_tokens, pdu.m_cryptoTokens,strm);
      if (result == H235Authenticator::e_Failed) {
          PTRACE(4, "H235EP\tSecurity Failure!");
          return false;
      }  else if (result == H235Authenticator::e_OK) {
          PTRACE(4, "H235EP\tAuthentication succeeded");
          AuthResult = TRUE;
      }
      return AuthResult ? TRUE : connection->OnEPAuthenticationFailed(result);
  }
}

#ifdef H323_H46018
const char * H46018OID = "0.0.8.460.18.0.1";
const char * H46019OID = "0.0.8.460.19.0.1";
const unsigned defH46019payload = 127;
#endif

#ifdef H323_H46024A
const char * H46024AOID = "0.0.8.460.24.1";
#endif
#ifdef H323_H46024B
const char * H46024BOID = "0.0.8.460.24.2";
#endif

H323Connection::H323Connection(H323EndPoint & ep,
                               unsigned ref,
                               unsigned options)
  : endpoint(ep),
    localAliasNames(ep.GetAliasNames()),
    localPartyName(ep.GetLocalUserName()),
	localLanguages(ep.GetLocalLanguages()),
    localCapabilities(ep.GetCapabilities()),
    gkAccessTokenOID(ep.GetGkAccessTokenOID()),
    alertingTime(0),
    connectedTime(0),
    callEndTime(0),
    reverseMediaOpenTime(0),
    noMediaTimeOut(ep.GetNoMediaTimeout().GetMilliSeconds()),
    roundTripDelayRate(ep.GetRoundTripDelayRate().GetMilliSeconds()),
    releaseSequence(ReleaseSequenceUnknown)
    ,EPAuthenticators(ep.CreateEPAuthenticators())
#ifdef H323_H239
    ,h239SessionID(0)
#endif
#ifdef H323_H460
    ,features(ep.GetFeatureSet())
#endif
{
  localAliasNames.MakeUnique();

  callAnswered = FALSE;
  gatekeeperRouted = FALSE;
  distinctiveRing = 0;
  callReference = ref;
  remoteCallWaiting = -1;

  h225version = H225_PROTOCOL_VERSION;
  h245version = H245_PROTOCOL_VERSION;
  h245versionSet = FALSE;

  signallingChannel = NULL;
  controlChannel = NULL;

#ifdef H323_H450
  holdAudioMediaChannel = NULL;
  holdVideoMediaChannel = NULL;
  isConsultationTransfer = FALSE;
  isCallIntrusion = FALSE;
  callIntrusionProtectionLevel = endpoint.GetCallIntrusionProtectionLevel();

  mwiInformation.mwiCtrId = ep.GetMWIMessageCentre();
  mwiInformation.mwiUser = ep.GetLocalUserName();
#endif

  switch (options&H245TunnelingOptionMask) {
    case H245TunnelingOptionDisable :
      h245Tunneling = FALSE;
      break;

    case H245TunnelingOptionEnable :
      h245Tunneling = TRUE;
      break;

    default :
      h245Tunneling = !ep.IsH245TunnelingDisabled();
      break;
  }

  h245TunnelTxPDU = NULL;
  h245TunnelRxPDU = NULL;
  alertingPDU = NULL;
  connectPDU = NULL;

  connectionState = NoConnectionActive;
  callEndReason = NumCallEndReasons;
  q931Cause = Q931::ErrorInCauseIE;

  bandwidthAvailable = endpoint.GetInitialBandwidth();

  useQ931Display = endpoint.UseQ931Display();

  uuiesRequested = 0; // Empty set
  addAccessTokenToSetup = TRUE; // Automatic inclusion of ACF access token in SETUP
  sendUserInputMode = endpoint.GetSendUserInputMode();

  mediaWaitForConnect = FALSE;
  transmitterSidePaused = FALSE;

  switch (options&FastStartOptionMask) {
    case FastStartOptionDisable :
      fastStartState = FastStartDisabled;
      break;

    case FastStartOptionEnable :
      fastStartState = FastStartInitiate;
      break;

    default :
      fastStartState = ep.IsFastStartDisabled() ? FastStartDisabled : FastStartInitiate;
      break;
  }

  mustSendDRQ = FALSE;
  earlyStart = FALSE;
  enableMERAHack = FALSE;

#ifdef H323_T120
  startT120 = TRUE;
#endif

#ifdef H323_H224
  startH224 = FALSE;
#endif

  lastPDUWasH245inSETUP = FALSE;
  endSessionNeeded = FALSE;
  endSessionSent = FALSE;

  switch (options&H245inSetupOptionMask) {
    case H245inSetupOptionDisable :
      doH245inSETUP = FALSE;
      break;

    case H245inSetupOptionEnable :
      doH245inSETUP = TRUE;
      break;

    default :
      doH245inSETUP = !ep.IsH245inSetupDisabled();
      break;
  }

  doH245QoS = !ep.IsH245QoSDisabled();

#ifdef H323_AUDIO_CODECS
  remoteMaxAudioDelayJitter = 0;
  minAudioJitterDelay = endpoint.GetMinAudioJitterDelay();
  maxAudioJitterDelay = endpoint.GetMaxAudioJitterDelay();
#endif

  switch (options&DetectInBandDTMFOptionMask) {
    case DetectInBandDTMFOptionDisable :
      detectInBandDTMF = FALSE;
      break;

    case DetectInBandDTMFOptionEnable :
      detectInBandDTMF = TRUE;
      break;

    default :
      detectInBandDTMF = !ep.DetectInBandDTMFDisabled();
      break;
  }

  masterSlaveDeterminationProcedure = new H245NegMasterSlaveDetermination(endpoint, *this);
  capabilityExchangeProcedure = new H245NegTerminalCapabilitySet(endpoint, *this);
  logicalChannels = new H245NegLogicalChannels(endpoint, *this);
  requestModeProcedure = new H245NegRequestMode(endpoint, *this);
  roundTripDelayProcedure = new H245NegRoundTripDelay(endpoint, *this);

#ifdef H323_H450
  h450dispatcher = new H450xDispatcher(*this);
  h4502handler = new H4502Handler(*this, *h450dispatcher);
  h4503handler = new H4503Handler(*this, *h450dispatcher);
  h4504handler = new H4504Handler(*this, *h450dispatcher);
  h4506handler = new H4506Handler(*this, *h450dispatcher);
  h45011handler = new H45011Handler(*this, *h450dispatcher);
#endif

  rfc2833InBandDTMF = !ep.RFC2833InBandDTMFDisabled();
  if (rfc2833InBandDTMF)
    rfc2833handler = new OpalRFC2833(PCREATE_NOTIFIER(OnUserInputInlineRFC2833));
  else
    rfc2833handler = NULL;

  extendedUserInput = !ep.ExtendedUserInputDisabled();

#ifdef H323_T120
  t120handler = NULL;
#endif

#ifdef H323_T38
  t38handler = NULL;
#endif

  endSync = NULL;
#ifdef P_DTMF
  dtmfTones = PString();
#endif
  remoteIsNAT = false;
  NATsupport =  true;
  sameNAT = false;

  AuthenticationFailed = FALSE;
  hasAuthentication = FALSE;

  // set aggregation options
#ifdef H323_RTP_AGGREGATE
  useRTPAggregation        = (options & RTPAggregationMask)        != RTPAggregationDisable;
#endif
#ifdef H323_SIGNAL_AGGREGATE
  signalAggregator = NULL;
  controlAggregator = NULL;
  useSignallingAggregation = (options & SignallingAggregationMask) != SignallingAggregationDisable;
#endif

#ifdef H323_AEC
    aec = NULL;
#endif

#ifdef H323_H460
  disableH460 = ep.FeatureSetDisabled();
  features->LoadFeatureSet(H460_Feature::FeatureSignal,this);

#ifdef H323_H460IM
  m_IMsupport = false;
  m_IMsession = false;
  m_IMcall = false;
  m_IMmsg = PString();
#endif

#ifdef H323_H4609
  m_h4609enabled = false;
  m_h4609Final = false;
#endif
#ifdef H323_H46017
  m_maintainConnection = ep.RegisteredWithH46017();
#else
  m_maintainConnection = false;
#endif

#ifdef H323_H46018
  m_H46019CallReceiver = false;
  m_H46019enabled = false;
  m_H46019multiplex = false;
  m_H46019offload = false;
  m_h245Connect = false;
#endif
#ifdef H323_H46024A
  m_H46024Aenabled = false;
  m_H46024Ainitator = false;
  m_H46024Astate = 0;
#endif
#ifdef H323_H46024B
  m_H46024Benabled = false;
  m_H46024Bstate = 0;
#endif
#ifdef H323_H46026
  m_H46026enabled = false;
#endif
#ifdef H323_H461
  m_H461Mode = e_h461EndpointCall;
#endif
#endif

  nonCallConnection = FALSE;
}


H323Connection::~H323Connection()
{

  delete masterSlaveDeterminationProcedure;
  delete capabilityExchangeProcedure;
  delete logicalChannels;
  delete requestModeProcedure;
  delete roundTripDelayProcedure;
#ifdef H323_AEC
  delete aec;
#endif
#ifdef H323_H450
  delete h450dispatcher;
#endif
  if (rfc2833handler)
    delete rfc2833handler;
#ifdef H323_T120
  delete t120handler;
#endif
#ifdef H323_T38
  delete t38handler;
#endif
  if (!m_maintainConnection)
    delete signallingChannel;

  delete controlChannel;
  delete alertingPDU;
  delete connectPDU;
#ifdef H323_H450
  delete holdAudioMediaChannel;
  delete holdVideoMediaChannel;
#endif
#ifdef H323_H460
  delete features;
#ifdef H323_H4609
  if (m_h4609Stats.GetSize() > 0) {
    H4609Statistics * stat = m_h4609Stats.Dequeue();
    while (stat) {
        delete stat;
        stat = m_h4609Stats.Dequeue();
    }
  }
#endif
#endif
#ifdef P_STUN
    m_NATSockets.clear();
#endif

  PTRACE(3, "H323\tConnection " << callToken << " deleted.");

  if (endSync != NULL)
    endSync->Signal();
}


PBoolean H323Connection::Lock()
{
  outerMutex.Wait();

  // If shutting down don't try and lock, just return failed. If not then lock
  // it but do second test for shut down to avoid a possible race condition.

  if (connectionState == ShuttingDownConnection) {
    outerMutex.Signal();
    return FALSE;
  }

  innerMutex.Wait();
  return TRUE;
}


int H323Connection::TryLock()
{
  if (!outerMutex.Wait(0))
    return -1;

  if (connectionState == ShuttingDownConnection) {
    outerMutex.Signal();
    return 0;
  }

  innerMutex.Wait();
  return 1;
}


void H323Connection::Unlock()
{
  innerMutex.Signal();
  outerMutex.Signal();
}


void H323Connection::SetCallEndReason(CallEndReason reason, PSyncPoint * sync)
{
  // Only set reason if not already set to something
  if (callEndReason == NumCallEndReasons) {
    PTRACE(3, "H323\tCall end reason for " << callToken << " set to " << reason);
    callEndReason = reason;
  }

  // only set the sync point if it is NULL
  if (endSync == NULL)
    endSync = sync;
  else
    PAssert(sync == NULL, "SendCallEndReason called to overwrite syncpoint");

  if (!callEndTime.IsValid())
    callEndTime = PTime();

  if (endSessionSent)
    return;

  endSessionSent = TRUE;

  PTRACE(2, "H225\tSending release complete PDU: callRef=" << callReference);
  H323SignalPDU rcPDU;
  rcPDU.BuildReleaseComplete(*this);

#ifdef H323_H450
  h450dispatcher->AttachToReleaseComplete(rcPDU);
#endif

  PBoolean sendingReleaseComplete = OnSendReleaseComplete(rcPDU);

  if (endSessionNeeded) {
    if (sendingReleaseComplete)
      h245TunnelTxPDU = &rcPDU; // Piggy back H245 on this reply

    // Send an H.245 end session to the remote endpoint.
    H323ControlPDU pdu;
    pdu.BuildEndSessionCommand(H245_EndSessionCommand::e_disconnect);
    WriteControlPDU(pdu);
  }

  if (sendingReleaseComplete) {
    h245TunnelTxPDU = NULL;
    if (releaseSequence == ReleaseSequenceUnknown)
      releaseSequence = ReleaseSequence_Local;
    WriteSignalPDU(rcPDU);
  }
}


PBoolean H323Connection::ClearCall(H323Connection::CallEndReason reason)
{
  return endpoint.ClearCall(callToken, reason);
}

PBoolean H323Connection::ClearCallSynchronous(PSyncPoint * sync, H323Connection::CallEndReason reason)
{
  return endpoint.ClearCallSynchronous(callToken, reason, sync);
}


void H323Connection::CleanUpOnCallEnd()
{
  PTRACE(3, "H323\tConnection " << callToken << " closing: connectionState=" << connectionState);

  /* The following double mutex is designed to guarentee that there is no
     deadlock or access of deleted object with a random thread that may have
     just called Lock() at the instant we are trying to get rid of the
     connection.
   */

  outerMutex.Wait();
  connectionState = ShuttingDownConnection;
  outerMutex.Signal();
  innerMutex.Wait();

  // Unblock sync points
  digitsWaitFlag.Signal();

  // stop various timers
  masterSlaveDeterminationProcedure->Stop();
  capabilityExchangeProcedure->Stop();

  // Clean up any fast start "pending" channels we may have running.
  PINDEX i;
  for (i = 0; i < fastStartChannels.GetSize(); i++)
    fastStartChannels[i].CleanUpOnTermination();
  fastStartChannels.RemoveAll();

  // Dispose of all the logical channels
  logicalChannels->RemoveAll();

  if (endSessionNeeded) {
    // Calculate time since we sent the end session command so we do not actually
    // wait for returned endSession if it has already been that long
    PTimeInterval waitTime = endpoint.GetEndSessionTimeout();
    if (callEndTime.IsValid()) {
      PTime now;
      if (now > callEndTime) { // Allow for backward motion in time (DST change)
        waitTime -= now - callEndTime;
        if (waitTime < 0)
          waitTime = 0;
      }
    }

    // Wait a while for the remote to send an endSession
    PTRACE(4, "H323\tAwaiting end session from remote for " << waitTime << " seconds");
    if (!endSessionReceived.Wait(waitTime)) {
      PTRACE(3, "H323\tDid not receive an end session from remote.");
    }
  }

  // Wait for control channel to be cleaned up (thread ended).
  if (controlChannel != NULL)
    controlChannel->CleanUpOnTermination();

#ifdef H323_SIGNAL_AGGREGATE
  if (controlAggregator != NULL)
    endpoint.GetSignallingAggregator()->RemoveHandle(controlAggregator);
#endif

  // Wait for signalling channel to be cleaned up (thread ended).
  if (signallingChannel != NULL)
    signallingChannel->CleanUpOnTermination();

#ifdef H323_SIGNAL_AGGREGATE
  if (signalAggregator != NULL)
    endpoint.GetSignallingAggregator()->RemoveHandle(signalAggregator);
#endif

  // Check for gatekeeper and do disengage if have one
  if (mustSendDRQ) {
    H323Gatekeeper * gatekeeper = endpoint.GetGatekeeper();
    if (gatekeeper != NULL)
      gatekeeper->DisengageRequest(*this, H225_DisengageReason::e_normalDrop);
  }

  PTRACE(1, "H323\tConnection " << callToken << " terminated.");
}

void H323Connection::AttachSignalChannel(const PString & token,
                                         H323Transport * channel,
                                         PBoolean answeringCall)
{
  callAnswered = answeringCall;

  if (signallingChannel != NULL && signallingChannel->IsOpen()) {
    PTRACE(1, "H323\tLogic error: signalling channel is open");
    return;
  }

  delete signallingChannel;
  signallingChannel = channel;

  // Set our call token for identification in endpoint dictionary
  callToken = token;

  SetAuthenticationConnection();
}

void H323Connection::ChangeSignalChannel(H323Transport * channel)
{
  if (signallingChannel == NULL || controlChannel == NULL || !h245Tunneling) {
    PTRACE(1, "H323\tLogic error: no signaling and no control channel");
    return;
  }

  signallingMutex.Wait();
    H323Transport * oldTransport = signallingChannel;
    signallingChannel = channel;
      controlMutex.Wait();
        H323Transport * oldControl = controlChannel;
        (void)StartControlChannel();
      controlMutex.Signal();
  signallingMutex.Signal();

  oldControl->CleanUpOnTermination();
  delete oldControl;

  oldTransport->CleanUpOnTermination();
  delete oldTransport;

}


PBoolean H323Connection::WriteSignalPDU(H323SignalPDU & pdu)
{
  lastPDUWasH245inSETUP = FALSE;

  PBoolean success = false;
  if (signallingChannel != NULL) {
    pdu.m_h323_uu_pdu.m_h245Tunneling = h245Tunneling;

    H323Gatekeeper * gk = endpoint.GetGatekeeper();
    if (gk)
      gk->InfoRequestResponse(*this, pdu.m_h323_uu_pdu, TRUE);

    signallingMutex.Wait();
#ifdef H323_H46017
    if (m_maintainConnection) {
        if (!pdu.GetQ931().HasIE(Q931::UserUserIE) && pdu.m_h323_uu_pdu.m_h323_message_body.IsValid())
              pdu.BuildQ931();

        if (!signallingChannel->WriteSignalPDU(pdu)) {
            PTRACE(2,"H225\tERROR: Signalling Channel Failure: PDU was not sent!");
            success = HandleSignalChannelFailure();
        } else
            success = true;
    } else
#endif
    {
        // We don't have to take down the call if the signalling channel fails.
        // We may want to wait until the media fails or the local hangs up.
        if (!pdu.Write(*signallingChannel,this)) {
            PTRACE(2,"H225\tERROR: Signalling Channel Failure: PDU was not sent!");
            success = HandleSignalChannelFailure();
        } else
            success = true;
    }
    signallingMutex.Signal();
  }

  if (!success)
    ClearCall(EndedByTransportFail);

  return success;
}

void H323Connection::HandleSignallingChannel()
{

  PTRACE(2, "H225\tReading PDUs: callRef=" << callReference);

  while (signallingChannel && signallingChannel->IsOpen()) {
    H323SignalPDU pdu;
    PBoolean readStatus = pdu.Read(*signallingChannel);
	// skip keep-alives
    if (readStatus && pdu.GetQ931().GetMessageType() == 0)
      continue;
    if (!HandleReceivedSignalPDU(readStatus, pdu))
      break;
  }

  // If we are the only link to the far end then indicate that we have
  // received endSession even if we hadn't, because we are now never going
  // to get one so there is no point in having CleanUpOnCallEnd wait.
  if (controlChannel == NULL)
    endSessionReceived.Signal();

  // if the signalling thread ends, make sure we end the call if it hasn't been done, yet
  // otherwise we have error conditions where the connection is never deleted
  if (!endSessionSent)
    ClearCall(EndedByTransportFail);

  PTRACE(2, "H225\tSignal channel closed.");
}

PBoolean H323Connection::HandleReceivedSignalPDU(PBoolean readStatus, H323SignalPDU & pdu)
{
  if (readStatus) {
    if (!HandleSignalPDU(pdu)) {
      if (AuthenticationFailed)
          ClearCall(EndedBySecurityDenial);
      else
        ClearCall(EndedByTransportFail);
      return FALSE;
    }
    switch (connectionState) {
      case EstablishedConnection :
        signallingChannel->SetReadTimeout(MonitorCallStatusTime);
        break;
      default:
        break;
    }
  }
  else if (signallingChannel->GetErrorCode() != PChannel::Timeout) {
    if (controlChannel == NULL || !controlChannel->IsOpen())
      ClearCall(EndedByTransportFail);
    signallingChannel->Close();
    return FALSE;
  }
  else {
    switch (connectionState) {
      case AwaitingSignalConnect :
        // Had time out waiting for remote to send a CONNECT
        ClearCall(EndedByNoAnswer);
        break;
      case HasExecutedSignalConnect :
        // Have had minimum MonitorCallStatusTime delay since CONNECT but
        // still no media to move it to EstablishedConnection state. Must
        // thus not have any common codecs to use!
        ClearCall(EndedByCapabilityExchange);
        break;
      default :
        break;
    }
  }

  if (controlChannel == NULL)
    MonitorCallStatus();

  return TRUE;
}

PBoolean CallEstablishmentMessage(const Q931 & q931)
{
  switch (q931.GetMessageType()) {
    case Q931::SetupMsg :
    case Q931::ProgressMsg :
    case Q931::AlertingMsg :
    case Q931::ConnectMsg :
      return true;
    default:
      return false;
  }
}


PBoolean H323Connection::HandleSignalPDU(H323SignalPDU & pdu)
{
  // Process the PDU.
  const Q931 & q931 = pdu.GetQ931();

  PTRACE(3, "H225\tHandling PDU: " << q931.GetMessageTypeName()
                    << " callRef=" << q931.GetCallReference());

  if (!Lock()) {
    // Continue to look for endSession/releaseComplete pdus
    if (pdu.m_h323_uu_pdu.m_h245Tunneling) {
      for (PINDEX i = 0; i < pdu.m_h323_uu_pdu.m_h245Control.GetSize(); i++) {
        PPER_Stream strm = pdu.m_h323_uu_pdu.m_h245Control[i].GetValue();
        if (!InternalEndSessionCheck(strm))
          break;
      }
    }
    if (q931.GetMessageType() == Q931::ReleaseCompleteMsg)
      endSessionReceived.Signal();
    return FALSE;
  }

  if (CallEstablishmentMessage(q931)) {
      // If remote does not do tunneling, so we don't either. Note that if it
      // gets turned off once, it stays off for good.
      if (h245Tunneling && !pdu.m_h323_uu_pdu.m_h245Tunneling.GetValue()) {
        masterSlaveDeterminationProcedure->Stop();
        capabilityExchangeProcedure->Stop();
        PTRACE(3, "H225\tFast Start DISABLED!");
        h245Tunneling = FALSE;
      }
  }

  h245TunnelRxPDU = &pdu;

#ifdef H323_H450
  // Check for presence of supplementary services
  if (pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_h4501SupplementaryService)) {
    if (!h450dispatcher->HandlePDU(pdu)) { // Process H4501SupplementaryService APDU
      Unlock();
      return FALSE;
    }
  }
#endif

#ifdef H323_H460
    if (pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_genericData)) {
       if (q931.GetMessageType() == Q931::SetupMsg) {
          remotePartyName = pdu.GetQ931().GetDisplayName();
          remoteAliasNames = pdu.GetSourceAliasNames();
       }
       ReceiveFeatureData<H323SignalPDU>(this,q931.GetMessageType(),pdu);
    }
#endif

  // Add special code to detect if call is from a Cisco and remoteApplication needs setting
  if (remoteApplication.IsEmpty() && pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_nonStandardControl)) {
    for (PINDEX i = 0; i < pdu.m_h323_uu_pdu.m_nonStandardControl.GetSize(); i++) {
      const H225_NonStandardIdentifier & id = pdu.m_h323_uu_pdu.m_nonStandardControl[i].m_nonStandardIdentifier;
      if (id.GetTag() == H225_NonStandardIdentifier::e_h221NonStandard) {
        const H225_H221NonStandard & h221 = id;
        if (h221.m_t35CountryCode == 181 && h221.m_t35Extension == 0 && h221.m_manufacturerCode == 18) {
          remoteApplication = "Cisco IOS\t12.x\t181/18";
          PTRACE(2, "H225\tSet remote application name: \"" << remoteApplication << '"');
          break;
        }
      }
    }
  }

  PBoolean ok;
  switch (q931.GetMessageType()) {
    case Q931::SetupMsg :
      setupTime = PTime();
      ok = OnReceivedSignalSetup(pdu);
      break;

    case Q931::CallProceedingMsg :
      ok = OnReceivedCallProceeding(pdu);
      break;

    case Q931::ProgressMsg :
      ok = OnReceivedProgress(pdu);
      break;

    case Q931::AlertingMsg :
      ok = OnReceivedAlerting(pdu);
      break;

    case Q931::ConnectMsg :
      connectedTime = PTime();
      ok = OnReceivedSignalConnect(pdu);
      break;

    case Q931::FacilityMsg :
      ok = OnReceivedFacility(pdu);
      break;

    case Q931::SetupAckMsg :
      ok = OnReceivedSignalSetupAck(pdu);
      break;

    case Q931::InformationMsg :
      ok = OnReceivedSignalInformation(pdu);
      break;

    case Q931::NotifyMsg :
      ok = OnReceivedSignalNotify(pdu);
      break;

    case Q931::StatusMsg :
      ok = OnReceivedSignalStatus(pdu);
      break;

    case Q931::StatusEnquiryMsg :
      ok = OnReceivedStatusEnquiry(pdu);
      break;

    case Q931::ReleaseCompleteMsg :
      if (releaseSequence == ReleaseSequenceUnknown)
        releaseSequence = ReleaseSequence_Remote;
      OnReceivedReleaseComplete(pdu);
      ok = FALSE;
      break;

    default :
      ok = OnUnknownSignalPDU(pdu);
  }

  if (ok) {
    // Process tunnelled H245 PDU, if present.
    HandleTunnelPDU(NULL);

    // Check for establishment criteria met
    InternalEstablishedConnectionCheck();
  }

  h245TunnelRxPDU = NULL;

  PString digits = pdu.GetQ931().GetKeypad();
  if (!digits)
    OnUserInputString(digits);

  H323Gatekeeper * gk = endpoint.GetGatekeeper();
  if (gk != NULL)
    gk->InfoRequestResponse(*this, pdu.m_h323_uu_pdu, FALSE);

  Unlock();

  return ok;
}


void H323Connection::HandleTunnelPDU(H323SignalPDU * txPDU)
{
  if (h245TunnelRxPDU == NULL || !h245TunnelRxPDU->m_h323_uu_pdu.m_h245Tunneling)
    return;

  if (!h245Tunneling && h245TunnelRxPDU->m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_setup)
    return;

  H323SignalPDU localTunnelPDU;
  if (txPDU != NULL)
    h245TunnelTxPDU = txPDU;
  else {
    /* Compensate for Cisco bug. IOS cannot seem to accept multiple tunnelled
       H.245 PDUs insode the same facility message */
    if (remoteApplication.Find("Cisco IOS") == P_MAX_INDEX) {
      // Not Cisco, so OK to tunnel multiple PDUs
      localTunnelPDU.BuildFacility(*this, TRUE);
      h245TunnelTxPDU = &localTunnelPDU;
    }
  }

  // if a response to a SETUP PDU containing TCS/MSD was ignored, then shutdown negotiations
  PINDEX i;
  if (lastPDUWasH245inSETUP &&
      (h245TunnelRxPDU->m_h323_uu_pdu.m_h245Control.GetSize() == 0) &&
      (h245TunnelRxPDU->GetQ931().GetMessageType() != Q931::CallProceedingMsg)) {
    PTRACE(4, "H225\tH.245 in SETUP ignored - resetting H.245 negotiations");
    masterSlaveDeterminationProcedure->Stop();
    lastPDUWasH245inSETUP = FALSE;
    capabilityExchangeProcedure->Stop();
  } else {
    for (i = 0; i < h245TunnelRxPDU->m_h323_uu_pdu.m_h245Control.GetSize(); i++) {
      PPER_Stream strm = h245TunnelRxPDU->m_h323_uu_pdu.m_h245Control[i].GetValue();
      HandleControlData(strm);
    }
  }

  // Make sure does not get repeated, clear tunnelled H.245 PDU's
  h245TunnelRxPDU->m_h323_uu_pdu.m_h245Control.SetSize(0);

  if (h245TunnelRxPDU->m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_setup) {
    H225_Setup_UUIE & setup = h245TunnelRxPDU->m_h323_uu_pdu.m_h323_message_body;

    if (setup.HasOptionalField(H225_Setup_UUIE::e_parallelH245Control)) {
      for (i = 0; i < setup.m_parallelH245Control.GetSize(); i++) {
        PPER_Stream strm = setup.m_parallelH245Control[i].GetValue();
        HandleControlData(strm);
      }

      // Make sure does not get repeated, clear tunnelled H.245 PDU's
      setup.m_parallelH245Control.SetSize(0);
    }
  }

  h245TunnelTxPDU = NULL;

  // If had replies, then send them off in their own packet
  if (txPDU == NULL && localTunnelPDU.m_h323_uu_pdu.m_h245Control.GetSize() > 0)
    WriteSignalPDU(localTunnelPDU);
}


static PBoolean BuildFastStartList(const H323Channel & channel,
                               H225_ArrayOf_PASN_OctetString & array,
                               H323Channel::Directions reverseDirection)
{
  H245_OpenLogicalChannel open;
  const H323Capability & capability = channel.GetCapability();

  if (channel.GetDirection() != reverseDirection) {
    if (!capability.OnSendingPDU(open.m_forwardLogicalChannelParameters.m_dataType))
      return FALSE;
  }
  else {
    if (!capability.OnSendingPDU(open.m_reverseLogicalChannelParameters.m_dataType))
      return FALSE;

    open.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(
                H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_none);
    open.m_forwardLogicalChannelParameters.m_dataType.SetTag(H245_DataType::e_nullData);
    open.IncludeOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
  }

  if (!channel.OnSendingPDU(open))
    return FALSE;

  PTRACE(4, "H225\tBuild fastStart:\n  " << setprecision(2) << open);
  PINDEX last = array.GetSize();
  array.SetSize(last+1);
  array[last].EncodeSubType(open);

  PTRACE(3, "H225\tBuilt fastStart for " << capability);
  return TRUE;
}

void H323Connection::OnEstablished()
{
  endpoint.OnConnectionEstablished(*this, callToken);
}

void H323Connection::OnCleared()
{
  endpoint.OnConnectionCleared(*this, callToken);
}


void H323Connection::SetRemoteVersions(const H225_ProtocolIdentifier & protocolIdentifier)
{
  if (protocolIdentifier.GetSize() < 6)
    return;

  h225version = protocolIdentifier[5];

  if (h245versionSet) {
    PTRACE(3, "H225\tSet protocol version to " << h225version);
    return;
  }

  // If has not been told explicitly what the H.245 version use, make an
  // assumption based on the H.225 version
  switch (h225version) {
    case 1 :
      h245version = 2;  // H.323 version 1
      break;
    case 2 :
      h245version = 3;  // H.323 version 2
      break;
    case 3 :
      h245version = 5;  // H.323 version 3
      break;
    case 4 :
      h245version = 7;  // H.323 version 4
      break;
    case 5 :
      h245version = 9;  // H.323 version 5
      break;
	case 6 :
	  h245version = 13; // H.323 version 6
	  break;
    default:
      h245version = 15; // H.323 version 7
      break;
  }
  PTRACE(3, "H225\tSet protocol version to " << h225version
         << " and implying H.245 version " << h245version);
}

PBoolean H323Connection::DecodeFastStartCaps(const H225_ArrayOf_PASN_OctetString & fastStartCaps)
{
  if (!capabilityExchangeProcedure->HasReceivedCapabilities())
    remoteCapabilities.RemoveAll();

  PTRACE(3, "H225\tFast start detected");

  // Extract capabilities from the fast start OpenLogicalChannel structures
  PINDEX i;
  for (i = 0; i < fastStartCaps.GetSize(); i++) {
    H245_OpenLogicalChannel open;
    if (fastStartCaps[i].DecodeSubType(open)) {
      PTRACE(4, "H225\tFast start open:\n  " << setprecision(2) << open);
      unsigned error;
      H323Channel * channel = CreateLogicalChannel(open, TRUE, error);
      if (channel != NULL) {
        if (channel->GetDirection() == H323Channel::IsTransmitter)
          channel->SetNumber(logicalChannels->GetNextChannelNumber());
        fastStartChannels.Append(channel);
      }
    }
    else {
      PTRACE(1, "H225\tInvalid fast start PDU decode:\n  " << open);
    }
  }

  PTRACE(3, "H225\tOpened " << fastStartChannels.GetSize() << " fast start channels");

  // If we are incapable of ANY of the fast start channels, don't do fast start
  if (!fastStartChannels.IsEmpty())
    fastStartState = FastStartResponse;

  return !fastStartChannels.IsEmpty();
}



PBoolean H323Connection::OnReceivedSignalSetup(const H323SignalPDU & setupPDU)
{
  if (setupPDU.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_setup)
    return FALSE;

  const H225_Setup_UUIE & setup = setupPDU.m_h323_uu_pdu.m_h323_message_body;

  if (!CheckRemoteApplication(setup.m_sourceInfo)) {
        PTRACE(2,"SETUP\tRemote Application check FAILURE.");
        return false;
  }

  /// Do Authentication of Incoming Call before anything else
  if (!ReceiveAuthenticatorPDU<H225_Setup_UUIE>(this,setup,
                            H225_H323_UU_PDU_h323_message_body::e_setup)) {
     if (GetEndPoint().GetEPSecurityPolicy() == H323EndPoint::SecRequired) {
        PTRACE(4, "H235EP\tAuthentication Failed. Ending Call");
        AuthenticationFailed = TRUE;
        return FALSE;
     }
     PTRACE(6, "H235EP\tAuthentication Failed but allowed by policy");
  } else {
     hasAuthentication = TRUE;
  }

  switch (setup.m_conferenceGoal.GetTag()) {
    case H225_Setup_UUIE_conferenceGoal::e_create:
    case H225_Setup_UUIE_conferenceGoal::e_join:
      break;

    case H225_Setup_UUIE_conferenceGoal::e_invite:
      return endpoint.OnConferenceInvite(FALSE,this,setupPDU);

    case H225_Setup_UUIE_conferenceGoal::e_callIndependentSupplementaryService:
      nonCallConnection = OnReceiveCallIndependentSupplementaryService(setupPDU);
      if (!nonCallConnection) return FALSE;
       break;

    case H225_Setup_UUIE_conferenceGoal::e_capability_negotiation:
      return endpoint.OnNegotiateConferenceCapabilities(setupPDU);
  }

  // Check Language Support
  if (setup.HasOptionalField(H225_Setup_UUIE::e_language)) {
      PStringList remoteLang;
	  if (!H323GetLanguages(remoteLang, setup.m_language) || !MergeLanguages(remoteLang, true)) {
		  PTRACE(2,"SETUP\tMissing or no common language support");
	  }
  }

  SetRemoteVersions(setup.m_protocolIdentifier);

  // Get the ring pattern
  distinctiveRing = setupPDU.GetDistinctiveRing();

  // Save the identifiers sent by caller
  if (setup.HasOptionalField(H225_Setup_UUIE::e_callIdentifier))
    callIdentifier = setup.m_callIdentifier.m_guid;
  conferenceIdentifier = setup.m_conferenceID;
  SetRemoteApplication(setup.m_sourceInfo);

  // Determine the remote parties name/number/address as best we can
  setupPDU.GetQ931().GetCallingPartyNumber(remoteQ931Number);
  remotePartyNumber = remoteQ931Number;
  remoteQ931Display = setupPDU.GetQ931().GetDisplayName();
  remoteAliasNames = setupPDU.GetSourceAliasNames();
  remotePartyAddress = signallingChannel->GetRemoteAddress();

  if (useQ931Display && !remotePartyNumber)
      remotePartyName = remotePartyNumber;
  else if (remoteAliasNames.GetSize() > 0)
      remotePartyName = remoteAliasNames[0];
  else
      remotePartyName = remotePartyAddress;

  if (setup.m_sourceAddress.GetSize() > 0) {
    if (!remotePartyAddress.IsEmpty())
        remotePartyAddress = H323GetAliasAddressString(setup.m_sourceAddress[0]) + '@' + remotePartyAddress;
    else
        remotePartyAddress = H323GetAliasAddressString(setup.m_sourceAddress[0]);
  }

#ifdef H323_H460
     ReceiveSetupFeatureSet(this, setup);
#endif

  // compare the source call signalling address
  if (setup.HasOptionalField(H225_Setup_UUIE::e_sourceCallSignalAddress)) {

    PIPSocket::Address srcAddr, sigAddr;
    H323TransportAddress sourceAddress(setup.m_sourceCallSignalAddress);
    sourceAddress.GetIpAddress(srcAddr);
    signallingChannel->GetRemoteAddress().GetIpAddress(sigAddr);

    NatDetection(srcAddr, sigAddr);
  }

  // Anything else we need from setup PDU
  mediaWaitForConnect = setup.m_mediaWaitForConnect;

  // Get the local capabilities before fast start or tunnelled TCS is handled
   if (!nonCallConnection)
      OnSetLocalCapabilities();

  // Send back a H323 Call Proceeding PDU in case OnIncomingCall() takes a while
  PTRACE(3, "H225\tSending call proceeding PDU");
  H323SignalPDU callProceedingPDU;
  H225_CallProceeding_UUIE & callProceeding = callProceedingPDU.BuildCallProceeding(*this);

#ifdef H323_H450
  if (!isConsultationTransfer) {
#endif
    if (OnSendCallProceeding(callProceedingPDU)) {
      if (fastStartState == FastStartDisabled)
        callProceeding.IncludeOptionalField(H225_CallProceeding_UUIE::e_fastConnectRefused);

      if (!WriteSignalPDU(callProceedingPDU))
        return FALSE;
    }

    /** Here is a spot where we should wait in case of Call Intrusion
    for CIPL from other endpoints
    if (isCallIntrusion) return TRUE;
    */

    // if the application indicates not to contine, then send a Q931 Release Complete PDU
    alertingPDU = new H323SignalPDU;
    alertingPDU->BuildAlerting(*this);

    /** If we have a case of incoming call intrusion we should not Clear the Call*/
    {
      CallEndReason incomingCallEndReason = EndedByNoAccept;
      if (!OnIncomingCall(setupPDU, *alertingPDU, incomingCallEndReason)
#ifdef H323_H450
        && (!isCallIntrusion)
#endif
      ) {
        ClearCall(incomingCallEndReason);
        PTRACE(1, "H225\tApplication not accepting calls");
        return FALSE;
      }
    }

    // send Q931 Alerting PDU
    PTRACE(3, "H225\tIncoming call accepted");

    // Check for gatekeeper and do admission check if have one
    H323Gatekeeper * gatekeeper = endpoint.GetGatekeeper();
    if (gatekeeper != NULL) {
      H225_ArrayOf_AliasAddress destExtraCallInfoArray;
      H323Gatekeeper::AdmissionResponse response;
      response.destExtraCallInfo = &destExtraCallInfoArray;
      response.languageSupport = &localLanguages;
      if (!gatekeeper->AdmissionRequest(*this, response)) {
        PTRACE(1, "H225\tGatekeeper refused admission: "
               << (response.rejectReason == UINT_MAX
                    ? PString("Transport error")
                    : H225_AdmissionRejectReason(response.rejectReason).GetTagName()));
        switch (response.rejectReason) {
          case H225_AdmissionRejectReason::e_calledPartyNotRegistered :
            ClearCall(EndedByNoUser);
            break;
          case H225_AdmissionRejectReason::e_requestDenied :
            ClearCall(EndedByNoBandwidth);
            break;
          case H225_AdmissionRejectReason::e_invalidPermission :
          case H225_AdmissionRejectReason::e_securityDenial :
            ClearCall(EndedBySecurityDenial);
            break;
          case H225_AdmissionRejectReason::e_resourceUnavailable :
            ClearCall(EndedByRemoteBusy);
            break;
          default :
            ClearCall(EndedByGatekeeper);
        }
        return FALSE;
      }

      if (destExtraCallInfoArray.GetSize() > 0)
        destExtraCallInfo = H323GetAliasAddressString(destExtraCallInfoArray[0]);
      mustSendDRQ = TRUE;
      gatekeeperRouted = response.gatekeeperRouted;
    }
#ifdef H323_H450
  }
#endif

  if (!nonCallConnection) {
      // See if remote endpoint wants to start fast
      if (fastStartState != FastStartDisabled &&
           setup.HasOptionalField(H225_Setup_UUIE::e_fastStart) &&
           localCapabilities.GetSize() > 0) {

        DecodeFastStartCaps(setup.m_fastStart);
      }

      // Check that if we are not doing Fast Connect that we have H.245 channel info
      if (fastStartState != FastStartResponse &&
          setup.HasOptionalField(H225_Setup_UUIE::e_h245Address)) {
             if (!StartControlChannel(setup.m_h245Address))
                       return FALSE;
      }
  }

  // Build the reply with the channels we are actually using
  connectPDU = new H323SignalPDU;
  connectPDU->BuildConnect(*this);

#ifdef H323_H450
  /** If Call Intrusion is allowed we must answer the call*/
  if (IsCallIntrusion()) {
    AnsweringCall(AnswerCallDeferred);
  }
  else {
    if (!isConsultationTransfer) {
#endif
      // call the application callback to determine if to answer the call or not
      connectionState = AwaitingLocalAnswer;
      AnsweringCall(OnAnswerCall(remotePartyName, setupPDU, *connectPDU));
#ifdef H323_H450
    }
    else
      AnsweringCall(AnswerCallNow);
  }
#endif

  return connectionState != ShuttingDownConnection;
}

void H323Connection::SetLocalPartyName(const PString & name)
{
  localPartyName = name;

  if (!name.IsEmpty()) {
    localAliasNames.RemoveAll();
    localAliasNames.SetSize(0);
    localAliasNames.AppendString(name);
  }
}


void H323Connection::SetRemotePartyInfo(const H323SignalPDU & pdu)
{
  PString number=PString();
  if (pdu.GetQ931().GetCalledPartyNumber(number) && !number.IsEmpty()) {
    remoteQ931Number = number;
    remotePartyNumber = remoteQ931Number;
  }

  PString newRemotePartyName = pdu.GetQ931().GetDisplayName();
  if (!newRemotePartyName.IsEmpty()) {
    remoteQ931Display = newRemotePartyName;
    remotePartyName = newRemotePartyName;
  } else if (!remotePartyNumber.IsEmpty())
    remotePartyName = remotePartyNumber;
  else
    remotePartyName = signallingChannel->GetRemoteAddress().GetHostName();

  PTRACE(2, "H225\tSet remote party name: \"" << remotePartyName << '"');
}


void H323Connection::SetRemotePartyName(const PString & name)
{
    if (useQ931Display)
        remoteQ931Number = name;

    remotePartyName = name;
}


void H323Connection::SetRemoteApplication(const H225_EndpointType & pdu)
{
  if (pdu.HasOptionalField(H225_EndpointType::e_vendor)) {
    remoteApplication = H323GetApplicationInfo(pdu.m_vendor);
    PTRACE(2, "H225\tSet remote application name: \"" << remoteApplication << '"');
  }
}

PBoolean H323Connection::CheckRemoteApplication(const H225_EndpointType & pdu)
{
#ifdef H323_H461
    if (endpoint.GetEndPointASSETMode() == H323EndPoint::e_H461ASSET &&
        (!pdu.HasOptionalField(H225_EndpointType::e_set) || !pdu.m_set[3]))
        return false;
#endif
    return true;
}

PBoolean H323Connection::OnSendCallIndependentSupplementaryService(H323SignalPDU & pdu) const
{
    return endpoint.OnSendCallIndependentSupplementaryService(this,pdu);
}

PBoolean H323Connection::OnReceiveCallIndependentSupplementaryService(const H323SignalPDU & pdu)
{

#ifdef H323_H450
    if (pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_h4501SupplementaryService)) {
        PTRACE(2,"CON\tReceived H.450 Call Independent Supplementary Service");
        return h450dispatcher->HandlePDU(pdu);
    }
#endif

#ifdef H323_H460
    if (!disableH460) {
        const H225_Setup_UUIE & setup = pdu.m_h323_uu_pdu.m_h323_message_body;
        if (ReceiveSetupFeatureSet(this, setup, true)) {
            PTRACE(2,"CON\tProcessed H.460 Call Independent Supplementary Service");
            return true;
        }

#ifdef H323_H461
        if (setup.m_sourceInfo.HasOptionalField(H225_EndpointType::e_set) && setup.m_sourceInfo.m_set[3]) {
            H323EndPoint::H461Mode mode = endpoint.GetEndPointASSETMode();
            if (mode == H323EndPoint::e_H461Disabled) {
                PTRACE(2,"CON\tLogic error SET call to regular endpoint");
                return false;
            }

            if (pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_genericData) && pdu.m_h323_uu_pdu.m_genericData.GetSize() > 0) {
                const H225_GenericIdentifier & id = pdu.m_h323_uu_pdu.m_genericData[0].m_id;
                if (id.GetTag() == H225_GenericIdentifier::e_oid) {
                    const PASN_ObjectId & val = id;
                    if (val.AsString() == "0.0.8.461.0") {
                        SetH461Mode(e_h461Associate);
                        return true;
                    }
                }
            }
        }
#endif
    }
#endif
    return endpoint.OnReceiveCallIndependentSupplementaryService(this,pdu);
}


PBoolean H323Connection::OnReceivedSignalSetupAck(const H323SignalPDU & /*setupackPDU*/)
{
  OnInsufficientDigits();
  return TRUE;
}


PBoolean H323Connection::OnReceivedSignalInformation(const H323SignalPDU & /*infoPDU*/)
{
  return TRUE;
}


PBoolean H323Connection::OnReceivedCallProceeding(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_callProceeding)
    return FALSE;
  const H225_CallProceeding_UUIE & call = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(call.m_protocolIdentifier);
  //SetRemotePartyInfo(pdu);  - Call proceeding could be fake do not set call party info
  SetRemoteApplication(call.m_destinationInfo);

  if (!ReceiveAuthenticatorPDU<H225_CallProceeding_UUIE>(this,call,
                 H225_H323_UU_PDU_h323_message_body::e_callProceeding)) {
//          don't do anything
  }

#ifdef H323_H460
  ReceiveFeatureSet<H225_CallProceeding_UUIE>(this, H460_MessageType::e_callProceeding, call);
#endif

  // Check for fastStart data and start fast
/*  MERA gatekeepers send fast start in call proceeding then
    decide to send a different one in either Alert,Progress or Connect and has assumed
    you have ignored the one in the CallProceeding.. Very Frustrating - S.H.
*/
 if (!enableMERAHack && call.HasOptionalField(H225_CallProceeding_UUIE::e_fastStart))
    HandleFastStartAcknowledge(call.m_fastStart);
  if (fastStartState == FastStartAcknowledged) {
        lastPDUWasH245inSETUP = FALSE;
        masterSlaveDeterminationProcedure->Stop();
        capabilityExchangeProcedure->Stop();
  } else {
        if (call.HasOptionalField(H225_CallProceeding_UUIE::e_h245Address))
               return StartControlChannel(call.m_h245Address);
  }

  return TRUE;
}


PBoolean H323Connection::OnReceivedProgress(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_progress)
    return FALSE;
  const H225_Progress_UUIE & progress = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(progress.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(progress.m_destinationInfo);

  // Check for fastStart data and start fast
  if (progress.HasOptionalField(H225_Progress_UUIE::e_fastStart))
    HandleFastStartAcknowledge(progress.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (progress.HasOptionalField(H225_Progress_UUIE::e_h245Address))
    return StartControlChannel(progress.m_h245Address);

  return TRUE;
}


PBoolean H323Connection::OnReceivedAlerting(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_alerting)
    return FALSE;
  const H225_Alerting_UUIE & alert = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(alert.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(alert.m_destinationInfo);

  if (!ReceiveAuthenticatorPDU<H225_Alerting_UUIE>(this,alert,
                         H225_H323_UU_PDU_h323_message_body::e_alerting)){
//          don't do anything
  }

#ifdef H323_H248
   if (alert.HasOptionalField(H225_Alerting_UUIE::e_serviceControl))
          OnReceiveServiceControlSessions(alert.m_serviceControl);
#endif

#ifdef H323_H460
  ReceiveFeatureSet<H225_Alerting_UUIE>(this, H460_MessageType::e_alerting, alert);
#endif

  // Check for fastStart data and start fast
  if (alert.HasOptionalField(H225_Alerting_UUIE::e_fastStart))
    HandleFastStartAcknowledge(alert.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (alert.HasOptionalField(H225_Alerting_UUIE::e_h245Address))
    if (!StartControlChannel(alert.m_h245Address))
      return FALSE;

  alertingTime = PTime();

  if (nonCallConnection)
      return true;
  else
      return OnAlerting(pdu, remotePartyName);
}


PBoolean H323Connection::OnReceivedSignalConnect(const H323SignalPDU & pdu)
{

  if (nonCallConnection) {
    connectedTime = PTime();
    connectionState = EstablishedConnection;
    return TRUE;
  }

  if (connectionState == ShuttingDownConnection)
    return FALSE;
  connectionState = HasExecutedSignalConnect;

  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_connect)
    return FALSE;
  const H225_Connect_UUIE & connect = pdu.m_h323_uu_pdu.m_h323_message_body;

  SetRemoteVersions(connect.m_protocolIdentifier);
  SetRemotePartyInfo(pdu);
  SetRemoteApplication(connect.m_destinationInfo);

  if (connect.HasOptionalField(H225_Connect_UUIE::e_language)) {
	  PStringList remoteLang;
	  if (!H323GetLanguages(remoteLang, connect.m_language) || !MergeLanguages(remoteLang, false)) {
		  PTRACE(2,"SETUP\tMissing or no common language support");
	  }
  }

   if (!ReceiveAuthenticatorPDU<H225_Connect_UUIE>(this,connect,
                         H225_H323_UU_PDU_h323_message_body::e_connect)) {
//          don't do anything
   }

#ifdef H323_H460
  ReceiveFeatureSet<H225_Connect_UUIE>(this, H460_MessageType::e_connect, connect);
#endif

  if (!OnOutgoingCall(pdu)) {
    ClearCall(EndedByNoAccept);
    return FALSE;
  }

#ifdef H323_H450
  // Are we involved in a transfer with a non H.450.2 compatible transferred-to endpoint?
  if (h4502handler->GetState() == H4502Handler::e_ctAwaitSetupResponse &&
      h4502handler->IsctTimerRunning())
  {
    PTRACE(4, "H4502\tRemote Endpoint does not support H.450.2.");
    h4502handler->OnReceivedSetupReturnResult();
  }
#endif

  // have answer, so set timeout to interval for monitoring calls health
  signallingChannel->SetReadTimeout(MonitorCallStatusTime);

  // If we are already faststartacknowledged (early media)
  // there is no need to proceed any further
  if (fastStartState == FastStartAcknowledged) {
      PTRACE(4, "H225\tConnect Accepted: Early Media already negotiated.");
      return TRUE;
  }

  // Check for fastStart data and start fast
  if (connect.HasOptionalField(H225_Connect_UUIE::e_fastStart))
    HandleFastStartAcknowledge(connect.m_fastStart);

  // Check that it has the H.245 channel connection info
  // ignore if we already have a Fast Start connection
  if (connect.HasOptionalField(H225_Connect_UUIE::e_h245Address) &&
             fastStartState != FastStartAcknowledged) {
    if (!StartControlChannel(connect.m_h245Address))
        return FALSE;
  }

  // If didn't get fast start channels accepted by remote then clear our
  // proposed channels so we can renegotiate
  if (fastStartState != FastStartAcknowledged) {
    fastStartState = FastStartDisabled;
    fastStartChannels.RemoveAll();
#ifdef P_STUN
    m_NATSockets.clear();
#endif
  }

  PTRACE(4, "H225\tFast Start " << (h245Tunneling ? "TRUE" : "FALSE")
                                    << " fastStartState " << fastStartState);

  // If we have a H.245 channel available, bring it up. We either have media
  // and this is just so user indications work, or we don't have media and
  // desperately need it!
  if (h245Tunneling || controlChannel != NULL)
      return OnStartHandleControlChannel();

  // We have no tunnelling and not separate channel, but we really want one
  // so we will start one using a facility message
  PTRACE(2, "H225\tNo H245 address provided by remote, starting control channel");

  if (!StartControlChannel())
    return FALSE;

  H323SignalPDU want245PDU;
  H225_Facility_UUIE * fac = want245PDU.BuildFacility(*this, FALSE, H225_FacilityReason::e_startH245);

  fac->IncludeOptionalField(H225_Facility_UUIE::e_h245Address);
  controlChannel->SetUpTransportPDU(fac->m_h245Address, TRUE);

  return WriteSignalPDU(want245PDU);
}


PBoolean H323Connection::OnReceivedFacility(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_empty)
    return TRUE;

  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_facility)
    return FALSE;
  const H225_Facility_UUIE & fac = pdu.m_h323_uu_pdu.m_h323_message_body;

  if (!ReceiveAuthenticatorPDU<H225_Facility_UUIE>(this,fac,
                            H225_H323_UU_PDU_h323_message_body::e_facility)) {
//          don't do anything
  }

#ifdef H323_H248
   if (fac.HasOptionalField(H225_Facility_UUIE::e_serviceControl))
          OnReceiveServiceControlSessions(fac.m_serviceControl);
#endif

#ifdef H323_H460
   // Do not process H.245 Control PDU's
   if (!pdu.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_h245Control))
         ReceiveFeatureSet<H225_Facility_UUIE>(this, H460_MessageType::e_facility, fac);
#endif

  SetRemoteVersions(fac.m_protocolIdentifier);

  // Check for fastStart data and start fast
  if (fac.HasOptionalField(H225_Facility_UUIE::e_fastStart))
    HandleFastStartAcknowledge(fac.m_fastStart);

  // Check that it has the H.245 channel connection info
  if (fac.HasOptionalField(H225_Facility_UUIE::e_h245Address)) {
    if (controlChannel != NULL && !controlChannel->IsOpen()) {
      // Fix race condition where both side want to open H.245 channel. we have
      // channel bit it is not open (ie we are listening) and the remote has
      // sent us an address to connect to. To resolve we compare the addresses.

      H225_TransportAddress myAddress;
      controlChannel->GetLocalAddress().SetPDU(myAddress);
      PPER_Stream myBuffer;
      myAddress.Encode(myBuffer);

      PPER_Stream otherBuffer;
      fac.m_h245Address.Encode(otherBuffer);

      if (myBuffer < otherBuffer || OnH245AddressConflict()) {
        PTRACE(2, "H225\tSimultaneous start of H.245 channel, connecting to remote.");
        controlChannel->CleanUpOnTermination();
        delete controlChannel;
        controlChannel = NULL;
      }
      else {
        PTRACE(2, "H225\tSimultaneous start of H.245 channel, using local listener.");
      }
    }

    if (!StartControlChannel(fac.m_h245Address))
      return FALSE;
  }

  if ((fac.m_reason.GetTag() != H225_FacilityReason::e_callForwarded)
      && (fac.m_reason.GetTag() != H225_FacilityReason::e_routeCallToGatekeeper)
      && (fac.m_reason.GetTag() != H225_FacilityReason::e_routeCallToMC))
    return TRUE;

  PString address;
  if (fac.m_reason.GetTag() == H225_FacilityReason::e_routeCallToGatekeeper) {
      PIPSocket::Address add;
      H323TransportAddress(remotePartyAddress).GetIpAddress(add);
      if (add.IsValid() && !add.IsAny() && !add.IsLoopback())
            address = add.AsString();
      else if (remotePartyAddress.Find('@') != P_MAX_INDEX)
            address = remotePartyAddress.Left(remotePartyAddress.Find('@'));
      else
            address = remotePartyAddress;
  }

  if (fac.HasOptionalField(H225_Facility_UUIE::e_alternativeAliasAddress) &&
      fac.m_alternativeAliasAddress.GetSize() > 0)
    address = H323GetAliasAddressString(fac.m_alternativeAliasAddress[0]);

  if (fac.HasOptionalField(H225_Facility_UUIE::e_alternativeAddress)) {
    if (!address)
      address += '@';
    address += H323TransportAddress(fac.m_alternativeAddress);
  }

  if (endpoint.OnConnectionForwarded(*this, address, pdu)) {
    ClearCall(EndedByCallForwarded);
    return FALSE;
  }

  if (!endpoint.CanAutoCallForward())
    return TRUE;

  if (!endpoint.ForwardConnection(*this, address, pdu))
    return TRUE;

  // This connection is on the way out and a new one has the same token now
  // so change our token to make sure no accidents can happen clearing the
  // wrong call
  callToken += "-forwarded";
  return FALSE;
}


PBoolean H323Connection::OnReceivedSignalNotify(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_notify) {
    const H225_Notify_UUIE & notify = pdu.m_h323_uu_pdu.m_h323_message_body;
    SetRemoteVersions(notify.m_protocolIdentifier);
  }
  return TRUE;
}


PBoolean H323Connection::OnReceivedSignalStatus(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_status) {
    const H225_Status_UUIE & status = pdu.m_h323_uu_pdu.m_h323_message_body;
    SetRemoteVersions(status.m_protocolIdentifier);
  }
  return TRUE;
}


PBoolean H323Connection::OnReceivedStatusEnquiry(const H323SignalPDU & pdu)
{
  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() == H225_H323_UU_PDU_h323_message_body::e_statusInquiry) {
    const H225_StatusInquiry_UUIE & status = pdu.m_h323_uu_pdu.m_h323_message_body;
    SetRemoteVersions(status.m_protocolIdentifier);
  }

  H323SignalPDU reply;
  reply.BuildStatus(*this);
  return reply.Write(*signallingChannel,this);
}


void H323Connection::OnReceivedReleaseComplete(const H323SignalPDU & pdu)
{
  if (!callEndTime.IsValid())
    callEndTime = PTime();

  endSessionReceived.Signal();

  if (q931Cause == Q931::ErrorInCauseIE)
    q931Cause = pdu.GetQ931().GetCause();

  if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_releaseComplete)
    return;
  const H225_ReleaseComplete_UUIE & rc = pdu.m_h323_uu_pdu.m_h323_message_body;

  switch (connectionState) {
    case EstablishedConnection :
      if (rc.m_reason.GetTag() == H225_ReleaseCompleteReason::e_facilityCallDeflection)
        ClearCall(EndedByCallForwarded);
      else
        ClearCall(EndedByRemoteUser);
      break;

    case AwaitingLocalAnswer :
      if (rc.m_reason.GetTag() == H225_ReleaseCompleteReason::e_facilityCallDeflection)
        ClearCall(EndedByCallForwarded);
      else
        ClearCall(EndedByCallerAbort);
      break;

    default :
      if (callEndReason == EndedByRefusal)
        callEndReason = NumCallEndReasons;

#ifdef H323_H450
      // Are we involved in a transfer with a non H.450.2 compatible transferred-to endpoint?
      if (h4502handler->GetState() == H4502Handler::e_ctAwaitSetupResponse &&
          h4502handler->IsctTimerRunning())
      {
        PTRACE(4, "H4502\tThe Remote Endpoint has rejected our transfer request and does not support H.450.2.");
        h4502handler->OnReceivedSetupReturnError(H4501_GeneralErrorList::e_notAvailable);
      }
#endif

#ifdef H323_H460
    ReceiveFeatureSet<H225_ReleaseComplete_UUIE>(this, H460_MessageType::e_releaseComplete, rc);
#endif

      if (pdu.m_h323_uu_pdu.m_h323_message_body.GetTag() != H225_H323_UU_PDU_h323_message_body::e_releaseComplete)
        ClearCall(EndedByRefusal);
      else {
        SetRemoteVersions(rc.m_protocolIdentifier);
        ClearCall(H323TranslateToCallEndReason(pdu.GetQ931().GetCause(), rc.m_reason));
      }
  }
}

PBoolean H323Connection::OnIncomingCall(const H323SignalPDU & setupPDU,
                                          H323SignalPDU & alertingPDU,
                                          CallEndReason & reason)
{
  return endpoint.OnIncomingCall(*this, setupPDU, alertingPDU, reason);
}

PBoolean H323Connection::OnIncomingCall(const H323SignalPDU & setupPDU,
                                          H323SignalPDU & alertingPDU)
{
  return endpoint.OnIncomingCall(*this, setupPDU, alertingPDU);
}


PBoolean H323Connection::ForwardCall(const PString & forwardParty)
{
  if (forwardParty.IsEmpty())
    return FALSE;

  PString alias;
  H323TransportAddress address;

  PStringList Addresses;
  if (!endpoint.ResolveCallParty(forwardParty, Addresses))
      return FALSE;

  if (!endpoint.ParsePartyName(Addresses[0], alias, address)) {
      PTRACE(2, "H323\tCould not parse forward party \"" << forwardParty << '"');
      return FALSE;
  }

  H323SignalPDU redirectPDU;
  H225_Facility_UUIE * fac = redirectPDU.BuildFacility(*this, FALSE, H225_FacilityReason::e_callForwarded);

  if (!address) {
    fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAddress);
    address.SetPDU(fac->m_alternativeAddress);
  }

  if (!alias) {
    fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAliasAddress);
    fac->m_alternativeAliasAddress.SetSize(1);
    H323SetAliasAddress(alias, fac->m_alternativeAliasAddress[0]);
  }

  return WriteSignalPDU(redirectPDU);
}

PBoolean H323Connection::ForwardCall(const H225_ArrayOf_AliasAddress & alternativeAliasAddresses, const H323TransportAddress & alternativeAddress)
{
  H323SignalPDU redirectPDU;
  H225_Facility_UUIE * fac = redirectPDU.BuildFacility(*this, FALSE, H225_FacilityReason::e_callForwarded);

  if (!alternativeAddress) {
    fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAddress);
    alternativeAddress.SetPDU(fac->m_alternativeAddress);
  }

  if (alternativeAliasAddresses.GetSize() > 0) {
    fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAliasAddress);
    fac->m_alternativeAliasAddress = alternativeAliasAddresses;
  }

  return WriteSignalPDU(redirectPDU);
}


PBoolean H323Connection::RouteCallToMC(const PString & forwardParty, const H225_ConferenceIdentifier & confID)
{
  if (forwardParty.IsEmpty())
    return FALSE;

  PString alias;
  H323TransportAddress address;

  PStringList Addresses;
  if (!endpoint.ResolveCallParty(forwardParty, Addresses))
      return FALSE;

  if (!endpoint.ParsePartyName(Addresses[0], alias, address)) {
      PTRACE(2, "H323\tCould not parse forward party \"" << forwardParty << '"');
      return FALSE;
  }

  H323SignalPDU redirectPDU;
  H225_Facility_UUIE * fac = redirectPDU.BuildFacility(*this, FALSE, H225_FacilityReason::e_routeCallToMC);

  if (!address) {
    fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAddress);
    address.SetPDU(fac->m_alternativeAddress);
  }

  if (!alias) {
    fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAliasAddress);
    fac->m_alternativeAliasAddress.SetSize(1);
    H323SetAliasAddress(alias, fac->m_alternativeAliasAddress[0]);
  }

  fac->IncludeOptionalField(H225_Facility_UUIE::e_conferenceID);
  fac->m_conferenceID = confID;

  return WriteSignalPDU(redirectPDU);
}

PBoolean H323Connection::RouteCallToMC(const H225_ArrayOf_AliasAddress & alternativeAliasAddresses, const H323TransportAddress & alternativeAddress, const H225_ConferenceIdentifier & confID)
{
  H323SignalPDU redirectPDU;
  H225_Facility_UUIE * fac = redirectPDU.BuildFacility(*this, FALSE, H225_FacilityReason::e_routeCallToMC);

  if (!alternativeAddress) {
    fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAddress);
    alternativeAddress.SetPDU(fac->m_alternativeAddress);
  }

  if (alternativeAliasAddresses.GetSize() > 0) {
    fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAliasAddress);
    fac->m_alternativeAliasAddress = alternativeAliasAddresses;
  }

  fac->IncludeOptionalField(H225_Facility_UUIE::e_conferenceID);
  fac->m_conferenceID = confID;

  return WriteSignalPDU(redirectPDU);
}


H323Connection::AnswerCallResponse
     H323Connection::OnAnswerCall(const PString & caller,
                                  const H323SignalPDU & setupPDU,
                                  H323SignalPDU & connectPDU)
{
  return endpoint.OnAnswerCall(*this, caller, setupPDU, connectPDU);
}


void H323Connection::AnsweringCall(AnswerCallResponse response)
{
  PTRACE(2, "H323\tAnswering call: " << response);

  if (!Lock())
    return;

  switch (response) {
    default : // AnswerCallDeferred
      break;

    case AnswerCallDeferredWithMedia :
      if (!mediaWaitForConnect) {
        // create a new facility PDU if doing AnswerDeferredWithMedia
        H323SignalPDU want245PDU;
        H225_Progress_UUIE & prog = want245PDU.BuildProgress(*this);

        PBoolean sendPDU = TRUE;

        if (SendFastStartAcknowledge(prog.m_fastStart))
          prog.IncludeOptionalField(H225_Progress_UUIE::e_fastStart);
        else {
          // See if aborted call
          if (connectionState == ShuttingDownConnection)
            break;

          // Do early H.245 start
          H225_Facility_UUIE & fac = *want245PDU.BuildFacility(*this, FALSE, H225_FacilityReason::e_startH245);
          earlyStart = TRUE;
          if (!h245Tunneling && (controlChannel == NULL)) {
            if (!StartControlChannel())
              break;

            fac.IncludeOptionalField(H225_Facility_UUIE::e_h245Address);
            controlChannel->SetUpTransportPDU(fac.m_h245Address, TRUE);
          }
          else
            sendPDU = FALSE;
        }

        if (sendPDU) {
          HandleTunnelPDU(&want245PDU);
          WriteSignalPDU(want245PDU);
        }
      }
      break;

    case AnswerCallAlertWithMedia :
      if (alertingPDU != NULL && !mediaWaitForConnect) {
        H225_Alerting_UUIE & alerting = alertingPDU->m_h323_uu_pdu.m_h323_message_body;

        PBoolean sendPDU = TRUE;
        if (SendFastStartAcknowledge(alerting.m_fastStart))
          alerting.IncludeOptionalField(H225_Alerting_UUIE::e_fastStart);
        else {
          alerting.IncludeOptionalField(H225_Alerting_UUIE::e_fastConnectRefused);

          // See if aborted call
          if (connectionState == ShuttingDownConnection)
            break;

          // Do early H.245 start
          earlyStart = TRUE;
          if (!h245Tunneling && (controlChannel == NULL)) {
            if (!StartControlChannel())
              break;
            alerting.IncludeOptionalField(H225_Alerting_UUIE::e_h245Address);
            controlChannel->SetUpTransportPDU(alerting.m_h245Address, TRUE);
          }
          else
            sendPDU = FALSE;
        }

        if (sendPDU) {
          HandleTunnelPDU(alertingPDU);

#ifdef H323_H450
          h450dispatcher->AttachToAlerting(*alertingPDU);
#endif

          WriteSignalPDU(*alertingPDU);
          alertingTime = PTime();
        }
        break;
      }
      // else clause falls into AnswerCallPending case

    case AnswerCallPending :
      if (alertingPDU != NULL) {
        // send Q931 Alerting PDU
        PTRACE(3, "H225\tSending Alerting PDU");

        HandleTunnelPDU(alertingPDU);

#ifdef H323_H450
        h450dispatcher->AttachToAlerting(*alertingPDU);
#endif

        // commented out by CRS: no need to check for lack of fastStart channels
        // as this Alerting is not associated with media channel. And doing so
        // screws up deferred fastStart setup
        //
        //if (fastStartChannels.IsEmpty()) {
        //  H225_Alerting_UUIE & alerting = alertingPDU->m_h323_uu_pdu.m_h323_message_body;
        //  alerting.IncludeOptionalField(H225_Alerting_UUIE::e_fastConnectRefused);
        //}

        WriteSignalPDU(*alertingPDU);
        alertingTime = PTime();
      }
      break;

    case AnswerCallDenied :
      // If response is denied, abort the call
      PTRACE(1, "H225\tApplication has declined to answer incoming call");
      ClearCall(EndedByAnswerDenied);
      break;

    case AnswerCallDeniedByInvalidCID :
      // If response is denied, abort the call
      PTRACE(1, "H225\tApplication has refused to answer incoming call due to invalid conference ID");
      ClearCall(EndedByInvalidConferenceID);
      break;

    case AnswerCallNowWithAlert :
      if (alertingPDU != NULL) {
        // send Q931 Alerting PDU
        PTRACE(3, "H225\tSending Alerting PDU prior to AnswerCall Now");

        HandleTunnelPDU(alertingPDU);

#ifdef H323_H450
        h450dispatcher->AttachToAlerting(*alertingPDU);
#endif
        WriteSignalPDU(*alertingPDU);
        alertingTime = PTime();
      }
        // Now we progress with AnswerCallNow.
    case AnswerCallNow :
      if (connectPDU != NULL) {
        H225_Connect_UUIE & connect = connectPDU->m_h323_uu_pdu.m_h323_message_body;

        // If we have not already negotiated Fast Connect (early media)
        if (fastStartState != FastStartAcknowledged) {
          // Now ask the application to select which channels to start
          if (SendFastStartAcknowledge(connect.m_fastStart))
            connect.IncludeOptionalField(H225_Connect_UUIE::e_fastStart);
          else
            connect.IncludeOptionalField(H225_Connect_UUIE::e_fastConnectRefused);
        }

        // See if aborted call
        if (connectionState == ShuttingDownConnection)
          break;

        // Set flag that we are up to CONNECT stage
        connectionState = HasExecutedSignalConnect;

#ifdef H323_H450
        h450dispatcher->AttachToConnect(*connectPDU);
#endif
        if (!nonCallConnection) {
            if (h245Tunneling) {
              // If no channels selected (or never provided) do traditional H245 start
              if (fastStartState == FastStartDisabled) {
                h245TunnelTxPDU = connectPDU; // Piggy back H245 on this reply
                PBoolean ok = StartControlNegotiations();
                h245TunnelTxPDU = NULL;
                if (!ok)
                  break;
              }

              HandleTunnelPDU(connectPDU);
            }
            else { // Start separate H.245 channel if not tunneling.
              if (!StartControlChannel())
                break;
              connect.IncludeOptionalField(H225_Connect_UUIE::e_h245Address);
              controlChannel->SetUpTransportPDU(connect.m_h245Address, TRUE);
            }
        }

        connectedTime = PTime();
        WriteSignalPDU(*connectPDU); // Send H323 Connect PDU
        delete connectPDU;
        connectPDU = NULL;
        delete alertingPDU;
        alertingPDU = NULL;
      }
  }

  InternalEstablishedConnectionCheck();
  Unlock();
}

H323Connection::CallEndReason H323Connection::SendSignalSetup(const PString & alias,
                                                              const H323TransportAddress & address)
{
  CallEndReason reason = NumCallEndReasons;

  // Start the call, first state is asking gatekeeper
  connectionState = AwaitingGatekeeperAdmission;

  // Indicate the direction of call.
  if (alias.IsEmpty())
    remotePartyName = remotePartyAddress = address;
  else {
    remotePartyName = alias;
    remoteAliasNames.AppendString(alias);
    if (!address.IsEmpty())
       remotePartyAddress = alias + '@' + address;
    else
       remotePartyAddress = alias;
  }

  // Start building the setup PDU to get various ID's
  H323SignalPDU setupPDU;
  H225_Setup_UUIE & setup = setupPDU.BuildSetup(*this, address);

#ifdef H323_H450
  h450dispatcher->AttachToSetup(setupPDU);
#endif

  // Save the identifiers generated by BuildSetup
  setupPDU.GetQ931().GetCalledPartyNumber(remotePartyNumber);

  H323TransportAddress gatekeeperRoute = address;

  // Check for gatekeeper and do admission check if have one
  H323Gatekeeper * gatekeeper = endpoint.GetGatekeeper();
  H225_ArrayOf_AliasAddress newAliasAddresses;
  PStringList callLanguages;
  if (gatekeeper != NULL) {
    H323Gatekeeper::AdmissionResponse response;
    response.transportAddress = &gatekeeperRoute;
    response.aliasAddresses = &newAliasAddresses;
    response.languageSupport = &callLanguages;
    if (!gkAccessTokenOID)
      response.accessTokenData = &gkAccessTokenData;
    while (!gatekeeper->AdmissionRequest(*this, response, alias.IsEmpty())) {
      PTRACE(1, "H225\tGatekeeper refused admission: "
             << (response.rejectReason == UINT_MAX
                  ? PString("Transport error")
                  : H225_AdmissionRejectReason(response.rejectReason).GetTagName()));
#ifdef H323_H450
      h4502handler->onReceivedAdmissionReject(H4501_GeneralErrorList::e_notAvailable);
#endif

      switch (response.rejectReason) {
        case H225_AdmissionRejectReason::e_calledPartyNotRegistered :
          return EndedByNoUser;
        case H225_AdmissionRejectReason::e_requestDenied :
          return EndedByNoBandwidth;
        case H225_AdmissionRejectReason::e_invalidPermission :
        case H225_AdmissionRejectReason::e_securityDenial :
          return EndedBySecurityDenial;
        case H225_AdmissionRejectReason::e_resourceUnavailable :
          return EndedByRemoteBusy;
        case H225_AdmissionRejectReason::e_incompleteAddress :
          if (OnInsufficientDigits())
            break;
          // Then default case
        default :
          return EndedByGatekeeper;
      }

      PString lastRemotePartyName = remotePartyName;
      while (lastRemotePartyName == remotePartyName) {
        Unlock(); // Release the mutex as can deadlock trying to clear call during connect.
        digitsWaitFlag.Wait();
        if (!Lock()) // Lock while checking for shutting down.
          return EndedByCallerAbort;
      }
    }
    mustSendDRQ = TRUE;
    if (response.gatekeeperRouted) {
      setup.IncludeOptionalField(H225_Setup_UUIE::e_endpointIdentifier);
      setup.m_endpointIdentifier = gatekeeper->GetEndpointIdentifier();
      gatekeeperRouted = TRUE;
    }
  }

  // Update the field e_destinationAddress in the SETUP PDU to reflect the new
  // alias received in the ACF (m_destinationInfo).
  if (newAliasAddresses.GetSize() > 0) {
    setup.IncludeOptionalField(H225_Setup_UUIE::e_destinationAddress);
    setup.m_destinationAddress = newAliasAddresses;

    // Update the Q.931 Information Element (if is an E.164 address)
    PString e164 = H323GetAliasAddressE164(newAliasAddresses);
    if (!e164) {
      remotePartyNumber = e164;
      setupPDU.GetQ931().SetCalledPartyNumber(e164);
    }
  }

  if (addAccessTokenToSetup && !gkAccessTokenOID && !gkAccessTokenData.IsEmpty()) {
    PString oid1, oid2;
    PINDEX comma = gkAccessTokenOID.Find(',');
    if (comma == P_MAX_INDEX)
      oid1 = oid2 = gkAccessTokenOID;
    else {
      oid1 = gkAccessTokenOID.Left(comma);
      oid2 = gkAccessTokenOID.Mid(comma+1);
    }
    setup.IncludeOptionalField(H225_Setup_UUIE::e_tokens);
    PINDEX last = setup.m_tokens.GetSize();
    setup.m_tokens.SetSize(last+1);
    setup.m_tokens[last].m_tokenOID = oid1;
    setup.m_tokens[last].IncludeOptionalField(H235_ClearToken::e_nonStandard);
    setup.m_tokens[last].m_nonStandard.m_nonStandardIdentifier = oid2;
    setup.m_tokens[last].m_nonStandard.m_data = gkAccessTokenData;
  }

  if (H323SetLanguages(callLanguages.GetSize() > 0 ? callLanguages : localLanguages, setup.m_language))
     setup.IncludeOptionalField(H225_Setup_UUIE::e_language);

#ifdef H323_H46017
  if (endpoint.RegisteredWithH46017()) {
       Unlock();
  } else
#endif
  {
      if (gatekeeper != NULL) {
          if (signallingChannel->InitialiseSecurity(&m_transportSecurity) &&
              !m_transportSecurity.GetRemoteTLSAddress().IsEmpty()) {
              gatekeeperRoute = m_transportSecurity.GetRemoteTLSAddress();
              PTRACE(4, "H225\tChanged remote address to secure " << gatekeeperRoute);
          }
      }

      if (!signallingChannel->IsOpen() && !signallingChannel->SetRemoteAddress(gatekeeperRoute)) {
        PTRACE(1, "H225\tInvalid "
               << (gatekeeperRoute != address ? "gatekeeper" : "user")
               << " supplied address: \"" << gatekeeperRoute << '"');
        connectionState = AwaitingTransportConnect;
        return EndedByConnectFail;
      }

      // Do the transport connect
      connectionState = AwaitingTransportConnect;


      // Release the mutex as can deadlock trying to clear call during connect.
      Unlock();

      PBoolean connectFailed = false;
      if (!signallingChannel->IsOpen()) {
        signallingChannel->SetWriteTimeout(100);
        connectFailed = !signallingChannel->Connect();
      }

      // See if transport connect failed, abort if so.
      if (connectFailed) {
        connectionState = NoConnectionActive;
        switch (signallingChannel->GetErrorNumber()) {
          case ENETUNREACH :
            reason = EndedByUnreachable;
            break;
          case ECONNREFUSED :
            reason = EndedByNoEndPoint;
            break;
          case ETIMEDOUT :
            reason = EndedByHostOffline;
            break;
          default:
             reason = EndedByConnectFail;
        }
      }
  }

  // Lock while checking for shutting down.
  if (!Lock())
    return EndedByCallerAbort;
  else if (reason != NumCallEndReasons)
    return reason;

  PTRACE(3, "H225\tSending Setup PDU");
  connectionState = AwaitingSignalConnect;

 // Add CryptoTokens and H460 features if available (need to do this after the ARQ/ACF)
   setupPDU.InsertCryptoTokensSetup(*this,setup);

#ifdef H323_H460
   if (!disableH460) {
     if (IsNonCallConnection())
         setupPDU.InsertH460Generic(*this);

     setupPDU.InsertH460Setup(*this,setup);
   }
#endif

  // Put in all the signalling addresses for link
  setup.IncludeOptionalField(H225_Setup_UUIE::e_sourceCallSignalAddress);
  signallingChannel->SetUpTransportPDU(setup.m_sourceCallSignalAddress, TRUE, this);
  if (!setup.HasOptionalField(H225_Setup_UUIE::e_destCallSignalAddress)) {
    setup.IncludeOptionalField(H225_Setup_UUIE::e_destCallSignalAddress);
    signallingChannel->SetUpTransportPDU(setup.m_destCallSignalAddress, FALSE, this);
  }

  // If a standard call do Fast Start (if required)
if (setup.m_conferenceGoal.GetTag() == H225_Setup_UUIE_conferenceGoal::e_create) {

  // Get the local capabilities before fast start is handled
  OnSetLocalCapabilities();

  // Adjust the local userInput capabilities.
  OnSetLocalUserInputCapabilities();

  // Ask the application what channels to open
  PTRACE(3, "H225\tCheck for Fast start by local endpoint");
  fastStartChannels.RemoveAll();
  OnSelectLogicalChannels();

  // If application called OpenLogicalChannel, put in the fastStart field
  if (!fastStartChannels.IsEmpty()) {
    PTRACE(3, "H225\tFast start begun by local endpoint");
    for (PINDEX i = 0; i < fastStartChannels.GetSize(); i++)
      BuildFastStartList(fastStartChannels[i], setup.m_fastStart, H323Channel::IsReceiver);
    if (setup.m_fastStart.GetSize() > 0)
      setup.IncludeOptionalField(H225_Setup_UUIE::e_fastStart);
  }

  // Search the capability set and see if we have video capability
  for (PINDEX i = 0; i < localCapabilities.GetSize(); i++) {
    switch (localCapabilities[i].GetMainType()) {
      case H323Capability::e_Audio :
      case H323Capability::e_UserInput :
      case H323Capability::e_GenericControl :
      case H323Capability::e_ConferenceControl :
      case H323Capability::e_Security :
        break;

      default : // Is video or other data (eg T.120)
        unsigned transferRate = 384000;  // default to 384 kb/s;
        OnBearerCapabilityTransferRate(transferRate);
        unsigned rate = setupPDU.GetQ931().SetBearerTransferRate(transferRate);
        setupPDU.GetQ931().SetBearerCapabilities(Q931::TransferUnrestrictedDigital, rate);
        i = localCapabilities.GetSize(); // Break out of the for loop
        break;
    }
  }
}

  if (!OnSendSignalSetup(setupPDU))
    return EndedByNoAccept;

  setupPDU.GetQ931().GetCalledPartyNumber(remotePartyNumber);

  //fastStartState = FastStartDisabled;
  PBoolean set_lastPDUWasH245inSETUP = FALSE;

  if (h245Tunneling && doH245inSETUP) {
    h245TunnelTxPDU = &setupPDU;

    // Try and start the master/slave and capability exchange through the tunnel
    // Note: this used to be disallowed but is now allowed as of H323v4
    PBoolean ok = StartControlNegotiations();

    h245TunnelTxPDU = NULL;

    if (!ok)
      return EndedByTransportFail;


    if (setup.m_fastStart.GetSize() > 0) {
      // Now if fast start as well need to put this in setup specific field
      // and not the generic H.245 tunneling field
      setup.IncludeOptionalField(H225_Setup_UUIE::e_parallelH245Control);
      setup.m_parallelH245Control = setupPDU.m_h323_uu_pdu.m_h245Control;
      setupPDU.m_h323_uu_pdu.RemoveOptionalField(H225_H323_UU_PDU::e_h245Control);
      set_lastPDUWasH245inSETUP = TRUE;
    }
  }

  // Send the initial PDU
  setupTime = PTime();
  if (!WriteSignalPDU(setupPDU))
    return EndedByTransportFail;

  // WriteSignalPDU always resets lastPDUWasH245inSETUP.
  // So set it here if required
  if (set_lastPDUWasH245inSETUP)
    lastPDUWasH245inSETUP = TRUE;

  // Set timeout for remote party to answer the call
  if (!m_maintainConnection)
    signallingChannel->SetReadTimeout(endpoint.GetSignallingChannelCallTimeout());

  return reason;
}

void H323Connection::NatDetection(const PIPSocket::Address & srcAddress, const PIPSocket::Address & sigAddress)
{
    // if the peer address is a public address, but the advertised source address is a private address
    // then there is a good chance the remote endpoint is behind a NAT but does not know it.
    // in this case, we active the NAT mode and wait for incoming RTP to provide the media address before
    // sending anything to the remote endpoint
    if ((!sigAddress.IsRFC1918() && srcAddress.IsRFC1918()) ||    // Internet Address
        ((sigAddress.IsRFC1918() && srcAddress.IsRFC1918()) && (sigAddress != srcAddress)))  // LAN on another LAN
    {
      PTRACE(3, "H225\tSource signal address " << srcAddress << " and TCP peer address " << sigAddress << " indicate remote endpoint is behind NAT");
      if (OnNatDetected())
          remoteIsNAT = true;
    }
}

PBoolean H323Connection::OnNatDetected()
{
#ifdef H323_H46018
    if (m_H46019enabled)
       return false;
#endif
    return true;
}

void H323Connection::DisableNATSupport() {
#ifdef H323_H46018
    if (!IsH46019Multiplexed())
#endif
        NATsupport = false;

    remoteIsNAT = false;
}

#ifdef P_STUN

PNatMethod * H323Connection::GetPreferedNatMethod(const PIPSocket::Address & ip) const
{
    return endpoint.GetPreferedNatMethod(ip);
}

PUDPSocket * H323Connection::GetNatSocket(unsigned session, PBoolean rtp)
{
    std::map<unsigned,NAT_Sockets>::const_iterator sockets_iter = m_NATSockets.find(session);
    if (sockets_iter != m_NATSockets.end()) {
        NAT_Sockets sockets = sockets_iter->second;
        if (rtp)
            return sockets.rtp;
        else
            return sockets.rtcp;
    }
    return NULL;
}

void H323Connection::SetRTPNAT(unsigned sessionid, PUDPSocket * _rtp, PUDPSocket * _rtcp)
{
    PWaitAndSignal m(NATSocketMutex);

    PTRACE(4,"H323\tRTP NAT Connection Callback! Session: " << sessionid);

    NAT_Sockets sockets;
     sockets.rtp = _rtp;
     sockets.rtcp = _rtcp;
     sockets.isActive = false;

    m_NATSockets.insert(pair<unsigned, NAT_Sockets>(sessionid, sockets));
}

void H323Connection::SetNATChannelActive(unsigned sessionid)
{
    std::map<unsigned,NAT_Sockets>::iterator sockets_iter = m_NATSockets.find(sessionid);
    if (sockets_iter != m_NATSockets.end())
        sockets_iter->second.isActive = true;
}

PBoolean H323Connection::IsNATMethodActive(unsigned sessionid)
{
    std::map<unsigned,NAT_Sockets>::iterator sockets_iter = m_NATSockets.find(sessionid);
    if (sockets_iter != m_NATSockets.end())
        return sockets_iter->second.isActive;

    return false;
}
#endif

void H323Connection::SetEndpointTypeInfo(H225_EndpointType & info) const
{
    return endpoint.SetEndpointTypeInfo(info);
}


PBoolean H323Connection::OnSendSignalSetup(H323SignalPDU & /*setupPDU*/)
{
  return TRUE;
}


PBoolean H323Connection::OnSendCallProceeding(H323SignalPDU & /*callProceedingPDU*/)
{
  return TRUE;
}


PBoolean H323Connection::OnSendReleaseComplete(H323SignalPDU & /*releaseCompletePDU*/)
{
  return TRUE;
}


PBoolean H323Connection::OnAlerting(const H323SignalPDU & alertingPDU,
                                const PString & username)
{
  return endpoint.OnAlerting(*this, alertingPDU, username);
}


PBoolean H323Connection::OnInsufficientDigits()
{
  return FALSE;
}


void H323Connection::SendMoreDigits(const PString & digits)
{
  remotePartyNumber += digits;
  remotePartyName = remotePartyNumber;
  if (connectionState == AwaitingGatekeeperAdmission)
    digitsWaitFlag.Signal();
  else {
    H323SignalPDU infoPDU;
    infoPDU.BuildInformation(*this);
    infoPDU.GetQ931().SetCalledPartyNumber(digits);
    WriteSignalPDU(infoPDU);
  }
}

PBoolean H323Connection::OnOutgoingCall(const H323SignalPDU & connectPDU)
{
  return endpoint.OnOutgoingCall(*this, connectPDU);
}

PBoolean H323Connection::SendFastStartAcknowledge(H225_ArrayOf_PASN_OctetString & array)
{
  PINDEX i;

  // See if we have already added the fast start OLC's
  if (array.GetSize() > 0)
    return TRUE;

  // See if we need to select our fast start channels
  if (fastStartState == FastStartResponse)
    OnSelectLogicalChannels();

  // Remove any channels that were not started by OnSelectLogicalChannels(),
  // those that were started are put into the logical channel dictionary
  for (i = 0; i < fastStartChannels.GetSize(); i++) {
    if (fastStartChannels[i].IsRunning())
      logicalChannels->Add(fastStartChannels[i]);
    else
      fastStartChannels.RemoveAt(i--);
  }

  // None left, so didn't open any channels fast
  if (fastStartChannels.IsEmpty()) {
    fastStartState = FastStartDisabled;
    return FALSE;
  }

  // The channels we just transferred to the logical channels dictionary
  // should not be deleted via this structure now.
  fastStartChannels.DisallowDeleteObjects();

  PTRACE(3, "H225\tAccepting fastStart for " << fastStartChannels.GetSize() << " channels");

  for (i = 0; i < fastStartChannels.GetSize(); i++)
    BuildFastStartList(fastStartChannels[i], array, H323Channel::IsTransmitter);

  // Have moved open channels to logicalChannels structure, remove all others.
  fastStartChannels.RemoveAll();

  // Last minute check to see that the remote has not decided
  // to send slow connect while we are doing fast!
  if (fastStartState == FastStartDisabled)
      return FALSE;

  // Set flag so internal establishment check does not require H.245
  fastStartState = FastStartAcknowledged;

  endSessionNeeded = FALSE;

  return TRUE;
}


PBoolean H323Connection::HandleFastStartAcknowledge(const H225_ArrayOf_PASN_OctetString & array)
{
  if (fastStartChannels.IsEmpty()) {
    PTRACE(3, "H225\tFast start response with no channels to open");
    return FALSE;
  }

  // record the time at which media was opened
  reverseMediaOpenTime = PTime();

  PTRACE(3, "H225\tFast start accepted by remote endpoint");

  PINDEX i;

  // Go through provided list of structures, if can decode it and match it up
  // with a channel we requested AND it has all the information needed in the
  // m_multiplexParameters, then we can start the channel.
  for (i = 0; i < array.GetSize(); i++) {
    H245_OpenLogicalChannel open;
    if (array[i].DecodeSubType(open)) {
      PTRACE(4, "H225\tFast start open:\n  " << setprecision(2) << open);
      PBoolean reverse = open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
      const H245_DataType & dataType = reverse ? open.m_reverseLogicalChannelParameters.m_dataType
                                               : open.m_forwardLogicalChannelParameters.m_dataType;
      H323Capability * replyCapability = localCapabilities.FindCapability(dataType);
      if (replyCapability != NULL) {
        for (PINDEX ch = 0; ch < fastStartChannels.GetSize(); ch++) {
          H323Channel & channelToStart = fastStartChannels[ch];
          H323Channel::Directions dir = channelToStart.GetDirection();
          if ((dir == H323Channel::IsReceiver) == reverse &&
               channelToStart.GetCapability() == *replyCapability) {
            unsigned error = 1000;
            if (channelToStart.OnReceivedPDU(open, error)) {
              H323Capability * channelCapability;
              if (dir == H323Channel::IsReceiver)
                channelCapability = replyCapability;
              else {
                // For transmitter, need to fake a capability into the remote table
                channelCapability = remoteCapabilities.FindCapability(channelToStart.GetCapability());
                if (channelCapability == NULL) {
                  channelCapability = remoteCapabilities.Copy(channelToStart.GetCapability());
                  remoteCapabilities.SetCapability(0, channelCapability->GetDefaultSessionID()-1, channelCapability);
                }
              }
              // Must use the actual capability instance from the
              // localCapability or remoteCapability structures.
              if (OnCreateLogicalChannel(*channelCapability, dir, error)) {
                if (channelToStart.SetInitialBandwidth()) {
                    if (open.HasOptionalField(H245_OpenLogicalChannel::e_genericInformation))
                        OnReceiveOLCGenericInformation(channelToStart.GetSessionID(),open.m_genericInformation,true);
                  channelToStart.Start();
                  break;
                }
                else
                  PTRACE(2, "H225\tFast start channel open fail: insufficent bandwidth");
              }
              else
                PTRACE(2, "H225\tFast start channel open error: " << error);
            }
            else
              PTRACE(2, "H225\tFast start capability error: " << error);
          }
        }
      }
    }
    else {
      PTRACE(1, "H225\tInvalid fast start PDU decode:\n  " << setprecision(2) << open);
    }
  }

  // Remove any channels that were not started by above, those that were
  // started are put into the logical channel dictionary
  for (i = 0; i < fastStartChannels.GetSize(); i++) {
    if (fastStartChannels[i].IsRunning())
      logicalChannels->Add(fastStartChannels[i]);
    else
      fastStartChannels.RemoveAt(i--);
  }

  // The channels we just transferred to the logical channels dictionary
  // should not be deleted via this structure now.
  fastStartChannels.DisallowDeleteObjects();

  PTRACE(2, "H225\tFast starting " << fastStartChannels.GetSize() << " channels");
  if (fastStartChannels.IsEmpty())
    return FALSE;

  // Have moved open channels to logicalChannels structure, remove them now.
  fastStartChannels.RemoveAll();

  fastStartState = FastStartAcknowledged;

  endSessionNeeded = FALSE;

  return TRUE;
}


PBoolean H323Connection::StartControlChannel()
{
  // Already have the H245 channel up.
  if (controlChannel != NULL)
    return TRUE;

  controlChannel = signallingChannel->CreateControlChannel(*this);
  if (controlChannel == NULL) {
    ClearCall(EndedByTransportFail);
    return FALSE;
  }

  controlChannel->StartControlChannel(*this);
  return TRUE;
}


PBoolean H323Connection::StartControlChannel(const H225_TransportAddress & h245Address)
{
  // Check that it is an IP address, all we support at the moment
  if (h245Address.GetTag() != H225_TransportAddress::e_ipAddress
#ifdef H323_IPV6
        && h245Address.GetTag() != H225_TransportAddress::e_ip6Address
#endif
      ) {
    PTRACE(1, "H225\tConnect of H245 failed: Unsupported transport");
    return FALSE;
  }

  // Already have the H245 channel up.
  if (controlChannel != NULL)
    return TRUE;

  unsigned m_version = 4;
  if (h245Address.GetTag() == H225_TransportAddress::e_ip6Address)
           m_version = 6;

  H323TransportSecurity m_h245Security;
  controlChannel = new H323TransportTCP(endpoint, PIPSocket::Address::GetAny(m_version));
  controlChannel->InitialiseSecurity(&m_h245Security);
  if (!controlChannel->SetRemoteAddress(h245Address)) {
    PTRACE(1, "H225\tCould not extract H245 address");
    delete controlChannel;
    controlChannel = NULL;
    return FALSE;
  }

  if (!controlChannel->Connect()) {
    PTRACE(1, "H225\tConnect of H245 failed: " << controlChannel->GetErrorText());
    delete controlChannel;
    controlChannel = NULL;
    return FALSE;
  }

  controlChannel->StartControlChannel(*this);
  return TRUE;
}


PBoolean H323Connection::OnUnknownSignalPDU(const H323SignalPDU & PTRACE_PARAM(pdu))
{
  PTRACE(2, "H225\tUnknown signalling PDU: " << pdu);
  return TRUE;
}


PBoolean H323Connection::WriteControlPDU(const H323ControlPDU & pdu)
{
  PWaitAndSignal m(controlMutex);

  PPER_Stream strm;
  pdu.Encode(strm);
  strm.CompleteEncoding();

  H323TraceDumpPDU("H245", TRUE, strm, pdu, pdu, 0,
                   (controlChannel == NULL) ? H323TransportAddress("") : controlChannel->GetLocalAddress(),
                   (controlChannel == NULL) ? H323TransportAddress("") : controlChannel->GetRemoteAddress()
                  );

  if (!h245Tunneling) {
    if (controlChannel == NULL) {
      PTRACE(1, "H245\tWrite PDU fail: no control channel.");
      return FALSE;
    }

    if (controlChannel->IsOpen() && controlChannel->WritePDU(strm))
      return TRUE;

    PTRACE(1, "H245\tWrite PDU fail: " << controlChannel->GetErrorText(PChannel::LastWriteError));
    return HandleControlChannelFailure();
  }

  // If have a pending signalling PDU, use it rather than separate write
  H323SignalPDU localTunnelPDU;
  H323SignalPDU * tunnelPDU;
  if (h245TunnelTxPDU != NULL)
    tunnelPDU = h245TunnelTxPDU;
  else {
    localTunnelPDU.BuildFacility(*this, TRUE);
    tunnelPDU = &localTunnelPDU;
  }

  tunnelPDU->m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_h245Control);
  PINDEX last = tunnelPDU->m_h323_uu_pdu.m_h245Control.GetSize();
  tunnelPDU->m_h323_uu_pdu.m_h245Control.SetSize(last+1);
  tunnelPDU->m_h323_uu_pdu.m_h245Control[last] = strm;

  if (h245TunnelTxPDU != NULL)
    return TRUE;

  return WriteSignalPDU(localTunnelPDU);
}


PBoolean H323Connection::StartControlNegotiations(PBoolean renegotiate)
{
  PTRACE(2, "H245\tStart control negotiations");

  if(renegotiate)  // makes reopening of media channels possible
    connectionState = HasExecutedSignalConnect;

  // Begin the capability exchange procedure
  if (!capabilityExchangeProcedure->Start(renegotiate)) {
    PTRACE(1, "H245\tStart of Capability Exchange failed");
    return FALSE;
  }

  // Begin the Master/Slave determination procedure
  if (!masterSlaveDeterminationProcedure->Start(renegotiate)) {
    PTRACE(1, "H245\tStart of Master/Slave determination failed");
    return FALSE;
  }

  endSessionNeeded = TRUE;
  return TRUE;
}

PBoolean H323Connection::OnStartHandleControlChannel()
{
     if (fastStartState == FastStartAcknowledged)
         return true;

     if (controlChannel == NULL)
         return StartControlNegotiations();
#ifndef H323_H46018
     else {
         PTRACE(2, "H245\tHandle control channel");
         return StartHandleControlChannel();
     }
#else
    if (!m_H46019enabled) {
        PTRACE(2, "H245\tHandle control channel");
        return StartHandleControlChannel();
    }

    // according to H.460.18 cl.11 we have to send a generic Indication on the opening of a
    // H.245 control channel. Details are specified in H.460.18 cl.16
    // This must be the first PDU otherwise gatekeeper/proxy will close the channel.

    PTRACE(2, "H46018\tStarted control channel");

        if (endpoint.H46018IsEnabled() && !m_h245Connect) {

        H323ControlPDU pdu;
        H245_GenericMessage & cap = pdu.Build(H245_IndicationMessage::e_genericIndication);

            H245_CapabilityIdentifier & id = cap.m_messageIdentifier;
                id.SetTag(H245_CapabilityIdentifier::e_standard);
                PASN_ObjectId & gid = id;
                gid.SetValue(H46018OID);

            cap.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
                PASN_Integer & sub = cap.m_subMessageIdentifier;
                sub = 1;

            cap.IncludeOptionalField(H245_GenericMessage::e_messageContent);
              H245_ArrayOf_GenericParameter & msg = cap.m_messageContent;

              // callIdentifer
                H245_GenericParameter call;
                   H245_ParameterIdentifier & idx = call.m_parameterIdentifier;
                     idx.SetTag(H245_ParameterIdentifier::e_standard);
                     PASN_Integer & m = idx;
                     m =1;
                   H245_ParameterValue & conx = call.m_parameterValue;
                     conx.SetTag(H245_ParameterValue::e_octetString);
                     PASN_OctetString & raw = conx;
                     raw.SetValue(callIdentifier);
                msg.SetSize(1);
                msg[0] = call;

              // Is receiver
                if (m_H46019CallReceiver) {
                     H245_GenericParameter answer;
                    H245_ParameterIdentifier & an = answer.m_parameterIdentifier;
                        an.SetTag(H245_ParameterIdentifier::e_standard);
                        PASN_Integer & n = an;
                        n =2;
                    H245_ParameterValue & aw = answer.m_parameterValue;
                    aw.SetTag(H245_ParameterValue::e_logical);
                    msg.SetSize(2);
                    msg[1] = answer;
                }
            PTRACE(4,"H46018\tSending H.245 Control PDU " << pdu);

            if (!WriteControlPDU(pdu))
                  return false;

              m_h245Connect = true;
        }

     return StartHandleControlChannel();

#endif
}

PBoolean H323Connection::StartHandleControlChannel()
{
  // If have started separate H.245 channel then don't tunnel any more
  h245Tunneling = FALSE;

  // Start the TCS and MSD operations on new H.245 channel.
  if (!StartControlNegotiations())
    return FALSE;

  // Disable the signalling channels timeout for monitoring call status and
  // start up one in this thread instead. Then the Q.931 channel can be closed
  // without affecting the call.
  signallingChannel->SetReadTimeout(PMaxTimeInterval);
  controlChannel->SetReadTimeout(MonitorCallStatusTime);

  return TRUE;
}

void H323Connection::EndHandleControlChannel()
{
  // If we are the only link to the far end or if we have already sent our
  // endSession command then indicate that we have received endSession even
  // if we hadn't, because we are now never going to get one so there is no
  // point in having CleanUpOnCallEnd wait.
  if (signallingChannel == NULL || endSessionSent == TRUE)
    endSessionReceived.Signal();
}

void H323Connection::HandleControlChannel()
{
  if (!OnStartHandleControlChannel())
    return;

  PBoolean ok = TRUE;
  while (ok) {
    MonitorCallStatus();
    PPER_Stream strm;
    PBoolean readStatus = controlChannel->ReadPDU(strm);
    ok = HandleReceivedControlPDU(readStatus, strm);
  }

  EndHandleControlChannel();

  PTRACE(2, "H245\tControl channel closed.");
}


PBoolean H323Connection::HandleReceivedControlPDU(PBoolean readStatus, PPER_Stream & strm)
{
  PBoolean ok = FALSE;

  if (readStatus) {
    // Lock while checking for shutting down.
    if (Lock()) {
      // Process the received PDU
      PTRACE(4, "H245\tReceived TPKT: " << strm);
      ok = HandleControlData(strm);
      Unlock(); // Unlock connection
    }
    else
      ok = InternalEndSessionCheck(strm);
  }
  else if (controlChannel->GetErrorCode() == PChannel::Timeout) {
    ok = true;
  }
  else {
      PTRACE(1, "H245\tRead error: " << controlChannel->GetErrorText(PChannel::LastReadError)
          << " endSessionSent=" << endSessionSent);
    // If the connection is already shutting down then don't overwrite the
    // call end reason.  This could happen if the remote end point misbehaves
    // and simply closes the H.245 TCP connection rather than sending an
    // endSession.
      if(endSessionSent == FALSE) {
         PTRACE(1, "H245\tTCP Socket closed Unexpectedly.");
         if (!HandleControlChannelFailure()) {
             PTRACE(1, "H245\tAborting call");
             ClearCall(EndedByTransportFail);
             return false;
         }
         PTRACE(1, "H245\tTCP Socket closed Unexpectedly. Attempting to reconnect.");
         if (!controlChannel->Connect()) {
             PTRACE(1, "H245\tTCP Socket could not reconnect. Proceed without control channel.");
             PThread::Sleep(500);
         }
        ok = true;
      } else {
         PTRACE(1, "H245\tendSession already sent assuming H245 connection closed by remote side");
         ok = false;
      }
  }

  return ok;
}


PBoolean H323Connection::InternalEndSessionCheck(PPER_Stream & strm)
{
  H323ControlPDU pdu;

  if (!pdu.Decode(strm)) {
    PTRACE(1, "H245\tInvalid PDU decode:\n  " << setprecision(2) << pdu);
    return FALSE;
  }

  PTRACE(3, "H245\tChecking for end session on PDU: " << pdu.GetTagName()
         << ' ' << ((PASN_Choice &)pdu.GetObject()).GetTagName());

  if (pdu.GetTag() != H245_MultimediaSystemControlMessage::e_command)
    return TRUE;

  H245_CommandMessage & command = pdu;
  if (command.GetTag() == H245_CommandMessage::e_endSessionCommand)
    endSessionReceived.Signal();
  return FALSE;
}


PBoolean H323Connection::HandleControlData(PPER_Stream & strm)
{
  while (!strm.IsAtEnd()) {
    H323ControlPDU pdu;
    if (!pdu.Decode(strm)) {
      PTRACE(1, "H245\tInvalid PDU decode!"
                "\nRaw PDU:\n" << hex << setfill('0')
                               << setprecision(2) << strm
                               << dec << setfill(' ') <<
                "\nPartial PDU:\n  " << setprecision(2) << pdu);
      return TRUE;
    }

    H323TraceDumpPDU("H245", FALSE, strm, pdu, pdu, 0,
                     (controlChannel == NULL) ? H323TransportAddress("") : controlChannel->GetLocalAddress(),
                     (controlChannel == NULL) ? H323TransportAddress("") : controlChannel->GetRemoteAddress()
                    );

    if (!HandleControlPDU(pdu))
      return FALSE;

    InternalEstablishedConnectionCheck();

    strm.ByteAlign();
  }

  return TRUE;
}


PBoolean H323Connection::HandleControlPDU(const H323ControlPDU & pdu)
{
  switch (pdu.GetTag()) {
    case H245_MultimediaSystemControlMessage::e_request :
      return OnH245Request(pdu);

    case H245_MultimediaSystemControlMessage::e_response :
      return OnH245Response(pdu);

    case H245_MultimediaSystemControlMessage::e_command :
      return OnH245Command(pdu);

    case H245_MultimediaSystemControlMessage::e_indication :
      return OnH245Indication(pdu);
  }

  return OnUnknownControlPDU(pdu);
}


PBoolean H323Connection::OnUnknownControlPDU(const H323ControlPDU & pdu)
{
  PTRACE(2, "H245\tUnknown Control PDU: " << pdu);

  H323ControlPDU reply;
  reply.BuildFunctionNotUnderstood(pdu);
  return WriteControlPDU(reply);
}


PBoolean H323Connection::OnH245Request(const H323ControlPDU & pdu)
{
  const H245_RequestMessage & request = pdu;

  switch (request.GetTag()) {
    case H245_RequestMessage::e_masterSlaveDetermination :
      if (fastStartState == FastStartResponse) {
         PTRACE(4,"H245\tIgnoring masterSlaveDetermination, already doing Fast Connect");
         return TRUE;
      }
      return masterSlaveDeterminationProcedure->HandleIncoming(request);

    case H245_RequestMessage::e_terminalCapabilitySet :
    {
      if (fastStartState == FastStartResponse) {
         PTRACE(4,"H245\tIgnoring TerminalCapabilitySet, already doing Fast Connect");
         return TRUE;
      }
      const H245_TerminalCapabilitySet & tcs = request;
      if (tcs.m_protocolIdentifier.GetSize() >= 6) {
        h245version = tcs.m_protocolIdentifier[5];
        h245versionSet = TRUE;
        PTRACE(3, "H245\tSet protocol version to " << h245version);
      }
      return capabilityExchangeProcedure->HandleIncoming(tcs);
    }

    case H245_RequestMessage::e_openLogicalChannel :
      return logicalChannels->HandleOpen(request);

    case H245_RequestMessage::e_closeLogicalChannel :
      return logicalChannels->HandleClose(request);

    case H245_RequestMessage::e_requestChannelClose :
      return logicalChannels->HandleRequestClose(request);

    case H245_RequestMessage::e_requestMode :
      return requestModeProcedure->HandleRequest(request);

    case H245_RequestMessage::e_roundTripDelayRequest :
      return roundTripDelayProcedure->HandleRequest(request);

    case H245_RequestMessage::e_conferenceRequest :
      if (OnHandleConferenceRequest(request))
        return TRUE;
      break;

    case H245_RequestMessage::e_genericRequest :
      if (OnHandleH245GenericMessage(h245request,request))
        return TRUE;
      break;
  }

  return OnUnknownControlPDU(pdu);
}


PBoolean H323Connection::OnH245Response(const H323ControlPDU & pdu)
{
  const H245_ResponseMessage & response = pdu;

  switch (response.GetTag()) {
    case H245_ResponseMessage::e_masterSlaveDeterminationAck :
      return masterSlaveDeterminationProcedure->HandleAck(response);

    case H245_ResponseMessage::e_masterSlaveDeterminationReject :
      return masterSlaveDeterminationProcedure->HandleReject(response);

    case H245_ResponseMessage::e_terminalCapabilitySetAck :
      return capabilityExchangeProcedure->HandleAck(response);

    case H245_ResponseMessage::e_terminalCapabilitySetReject :
      return capabilityExchangeProcedure->HandleReject(response);

    case H245_ResponseMessage::e_openLogicalChannelAck :
      return logicalChannels->HandleOpenAck(response);

    case H245_ResponseMessage::e_openLogicalChannelReject :
      return logicalChannels->HandleReject(response);

    case H245_ResponseMessage::e_closeLogicalChannelAck :
      return logicalChannels->HandleCloseAck(response);

    case H245_ResponseMessage::e_requestChannelCloseAck :
      return logicalChannels->HandleRequestCloseAck(response);

    case H245_ResponseMessage::e_requestChannelCloseReject :
      return logicalChannels->HandleRequestCloseReject(response);

    case H245_ResponseMessage::e_requestModeAck :
      return requestModeProcedure->HandleAck(response);

    case H245_ResponseMessage::e_requestModeReject :
      return requestModeProcedure->HandleReject(response);

    case H245_ResponseMessage::e_roundTripDelayResponse :
      return roundTripDelayProcedure->HandleResponse(response);

    case H245_ResponseMessage::e_conferenceResponse :
      if (OnHandleConferenceResponse(response))
        return TRUE;
      break;

    case H245_ResponseMessage::e_genericResponse :
      if (OnHandleH245GenericMessage(h245response,response))
        return TRUE;
      break;
  }

  return OnUnknownControlPDU(pdu);
}


PBoolean H323Connection::OnH245Command(const H323ControlPDU & pdu)
{
  const H245_CommandMessage & command = pdu;

  switch (command.GetTag()) {
    case H245_CommandMessage::e_sendTerminalCapabilitySet :
      return OnH245_SendTerminalCapabilitySet(command);

    case H245_CommandMessage::e_flowControlCommand :
      return OnH245_FlowControlCommand(command);

    case H245_CommandMessage::e_miscellaneousCommand :
      return OnH245_MiscellaneousCommand(command);

    case H245_CommandMessage::e_endSessionCommand :
      endSessionNeeded = TRUE;
      endSessionReceived.Signal();
      switch (connectionState) {
        case EstablishedConnection :
          ClearCall(EndedByRemoteUser);
          break;
        case AwaitingLocalAnswer :
          ClearCall(EndedByCallerAbort);
          break;
        default :
          ClearCall(EndedByRefusal);
      }
      return FALSE;

    case H245_CommandMessage::e_conferenceCommand:
      if (OnHandleConferenceCommand(command))
        return TRUE;
      break;

    case H245_CommandMessage::e_genericCommand :
      if (OnHandleH245GenericMessage(h245command,command))
        return TRUE;
      break;
  }

  return OnUnknownControlPDU(pdu);
}


PBoolean H323Connection::OnH245Indication(const H323ControlPDU & pdu)
{
  const H245_IndicationMessage & indication = pdu;

  switch (indication.GetTag()) {
    case H245_IndicationMessage::e_masterSlaveDeterminationRelease :
      return masterSlaveDeterminationProcedure->HandleRelease(indication);

    case H245_IndicationMessage::e_terminalCapabilitySetRelease :
      return capabilityExchangeProcedure->HandleRelease(indication);

    case H245_IndicationMessage::e_openLogicalChannelConfirm :
      return logicalChannels->HandleOpenConfirm(indication);

    case H245_IndicationMessage::e_requestChannelCloseRelease :
      return logicalChannels->HandleRequestCloseRelease(indication);

    case H245_IndicationMessage::e_requestModeRelease :
      return requestModeProcedure->HandleRelease(indication);

    case H245_IndicationMessage::e_miscellaneousIndication :
      return OnH245_MiscellaneousIndication(indication);

    case H245_IndicationMessage::e_jitterIndication :
      return OnH245_JitterIndication(indication);

    case H245_IndicationMessage::e_userInput :
      OnUserInputIndication(indication);
      break;

    case H245_IndicationMessage::e_conferenceIndication :
      if (OnHandleConferenceIndication(indication))
        return TRUE;
      break;

    case H245_IndicationMessage::e_flowControlIndication :
        PTRACE(3,"H245\tFlow Indication received NOT HANDLED!");
        return TRUE;
      break;

    case H245_IndicationMessage::e_genericIndication :
      if (OnHandleH245GenericMessage(h245indication,indication))
        return TRUE;
      break;
  }

  return TRUE; // Do NOT call OnUnknownControlPDU for indications
}


PBoolean H323Connection::OnH245_SendTerminalCapabilitySet(
                 const H245_SendTerminalCapabilitySet & pdu)
{
  if (pdu.GetTag() == H245_SendTerminalCapabilitySet::e_genericRequest)
    return capabilityExchangeProcedure->Start(TRUE);

  PTRACE(2, "H245\tUnhandled SendTerminalCapabilitySet: " << pdu);
  return TRUE;
}


PBoolean H323Connection::OnH245_FlowControlCommand(
                 const H245_FlowControlCommand & pdu)
{
  PTRACE(3, "H245\tFlowControlCommand: scope=" << pdu.m_scope.GetTagName());

  long restriction;
  if (pdu.m_restriction.GetTag() == H245_FlowControlCommand_restriction::e_maximumBitRate)
    restriction = (const PASN_Integer &)pdu.m_restriction;
  else
    restriction = -1; // H245_FlowControlCommand_restriction::e_noRestriction

  switch (pdu.m_scope.GetTag()) {
    case H245_FlowControlCommand_scope::e_wholeMultiplex :
      OnLogicalChannelFlowControl(NULL, restriction);
      break;

    case H245_FlowControlCommand_scope::e_logicalChannelNumber :
    {
      H323Channel * chan = logicalChannels->FindChannel((unsigned)(const H245_LogicalChannelNumber &)pdu.m_scope, FALSE);
      if (chan != NULL)
        OnLogicalChannelFlowControl(chan, restriction);
    }
  }

  return TRUE;
}


PBoolean H323Connection::OnH245_MiscellaneousCommand(
                 const H245_MiscellaneousCommand & pdu)
{
  H323Channel * chan = logicalChannels->FindChannel((unsigned)pdu.m_logicalChannelNumber, FALSE);
  if (chan != NULL)
    chan->OnMiscellaneousCommand(pdu.m_type);
  else
    PTRACE(3, "H245\tMiscellaneousCommand: is ignored chan=" << pdu.m_logicalChannelNumber
           << ", type=" << pdu.m_type.GetTagName());

  return TRUE;
}


PBoolean H323Connection::OnH245_MiscellaneousIndication(
                 const H245_MiscellaneousIndication & pdu)
{
  H323Channel * chan = logicalChannels->FindChannel((unsigned)pdu.m_logicalChannelNumber, TRUE);
  if (chan != NULL)
    chan->OnMiscellaneousIndication(pdu.m_type);
  else
    PTRACE(3, "H245\tMiscellaneousIndication is ignored. chan=" << pdu.m_logicalChannelNumber
           << ", type=" << pdu.m_type.GetTagName());

  return TRUE;
}


PBoolean H323Connection::OnH245_JitterIndication(
                 const H245_JitterIndication & pdu)
{
  PTRACE(3, "H245\tJitterIndication: scope=" << pdu.m_scope.GetTagName());

  static const DWORD mantissas[8] = { 0, 1, 10, 100, 1000, 10000, 100000, 1000000 };
  static const DWORD exponents[8] = { 10, 25, 50, 75 };
  DWORD jitter = mantissas[pdu.m_estimatedReceivedJitterMantissa]*
                 exponents[pdu.m_estimatedReceivedJitterExponent]/10;

  int skippedFrameCount = -1;
  if (pdu.HasOptionalField(H245_JitterIndication::e_skippedFrameCount))
    skippedFrameCount = pdu.m_skippedFrameCount;

  int additionalBuffer = -1;
  if (pdu.HasOptionalField(H245_JitterIndication::e_additionalDecoderBuffer))
    additionalBuffer = pdu.m_additionalDecoderBuffer;

  switch (pdu.m_scope.GetTag()) {
    case H245_JitterIndication_scope::e_wholeMultiplex :
      OnLogicalChannelJitter(NULL, jitter, skippedFrameCount, additionalBuffer);
      break;

    case H245_JitterIndication_scope::e_logicalChannelNumber :
    {
      H323Channel * chan = logicalChannels->FindChannel((unsigned)(const H245_LogicalChannelNumber &)pdu.m_scope, FALSE);
      if (chan != NULL)
        OnLogicalChannelJitter(chan, jitter, skippedFrameCount, additionalBuffer);
    }
  }

  return TRUE;
}


H323Channel * H323Connection::GetLogicalChannel(unsigned number, PBoolean fromRemote) const
{
  return logicalChannels->FindChannel(number, fromRemote);
}


H323Channel * H323Connection::FindChannel(unsigned rtpSessionId, PBoolean fromRemote) const
{
  return logicalChannels->FindChannelBySession(rtpSessionId, fromRemote);
}

#ifdef H323_H450

void H323Connection::TransferCall(const PString & remoteParty,
                                  const PString & callIdentity)
{
  // According to H.450.4, if prior to consultation the primary call has been put on hold, the
  // transferring endpoint shall first retrieve the call before Call Transfer is invoked.
  if (!callIdentity.IsEmpty() && IsLocalHold())
    RetrieveCall();
  h4502handler->TransferCall(remoteParty, callIdentity);
}

void H323Connection::OnReceivedInitiateReturnError()
{
    endpoint.OnReceivedInitiateReturnError();
}

void H323Connection::ConsultationTransfer(const PString & primaryCallToken)
{
  h4502handler->ConsultationTransfer(primaryCallToken);
}


void H323Connection::HandleConsultationTransfer(const PString & callIdentity,
                                                H323Connection& incoming)
{
  h4502handler->HandleConsultationTransfer(callIdentity, incoming);
}


PBoolean H323Connection::IsTransferringCall() const
{
  switch (h4502handler->GetState()) {
    case H4502Handler::e_ctAwaitIdentifyResponse :
    case H4502Handler::e_ctAwaitInitiateResponse :
    case H4502Handler::e_ctAwaitSetupResponse :
      return TRUE;

    default :
      return FALSE;
  }
}


PBoolean H323Connection::IsTransferredCall() const
{
   return (h4502handler->GetInvokeId() != 0 &&
           h4502handler->GetState() == H4502Handler::e_ctIdle) ||
           h4502handler->isConsultationTransferSuccess();
}


void H323Connection::HandleTransferCall(const PString & token,
                                        const PString & identity)
{
  if (!token.IsEmpty() || !identity)
    h4502handler->AwaitSetupResponse(token, identity);
}


int H323Connection::GetCallTransferInvokeId()
{
  return h4502handler->GetInvokeId();
}


void H323Connection::HandleCallTransferFailure(const int returnError)
{
  h4502handler->HandleCallTransferFailure(returnError);
}


void H323Connection::SetAssociatedCallToken(const PString& token)
{
  h4502handler->SetAssociatedCallToken(token);
}


void H323Connection::OnConsultationTransferSuccess(H323Connection& /*secondaryCall*/)
{
   h4502handler->SetConsultationTransferSuccess();
}

void H323Connection::SetCallLinkage(H225_AdmissionRequest& /*arq*/ )
{
}

void H323Connection::GetCallLinkage(const H225_AdmissionRequest& /*arq*/)
{
}

void H323Connection::HoldCall(PBoolean localHold)
{
  h4504handler->HoldCall(localHold);
  holdAudioMediaChannel = SwapHoldMediaChannels(holdAudioMediaChannel,RTP_Session::DefaultAudioSessionID);
  holdVideoMediaChannel = SwapHoldMediaChannels(holdVideoMediaChannel,RTP_Session::DefaultVideoSessionID);
}

PBoolean H323Connection::GetRedirectingNumber(
    PString &originalCalledNr,
    PString &lastDivertingNr,
    int &divCounter,
    int &originaldivReason,
    int &divReason)
{
  return h4503handler->GetRedirectingNumber(originalCalledNr,lastDivertingNr,
                                         divCounter,originaldivReason,divReason);
}

void H323Connection::RetrieveCall()
{
  // Is the current call on hold?
  if (IsLocalHold()) {
    h4504handler->RetrieveCall();
    holdAudioMediaChannel = SwapHoldMediaChannels(holdAudioMediaChannel,RTP_Session::DefaultAudioSessionID);
    holdVideoMediaChannel = SwapHoldMediaChannels(holdVideoMediaChannel,RTP_Session::DefaultVideoSessionID);
  }
  else if (IsRemoteHold()) {
    PTRACE(4, "H4504\tRemote-end Call Hold not implemented.");
  }
  else {
    PTRACE(4, "H4504\tCall is not on Hold.");
  }
}


void H323Connection::SetHoldMedia(PChannel * audioChannel)
{

  holdAudioMediaChannel = audioChannel;
}

void H323Connection::SetVideoHoldMedia(PChannel * videoChannel)
{
  holdVideoMediaChannel = videoChannel;
}

PBoolean H323Connection::IsMediaOnHold() const
{
  return holdAudioMediaChannel != NULL;
}


PChannel * H323Connection::SwapHoldMediaChannels(PChannel * newChannel,unsigned sessionId)
{
  if (IsMediaOnHold()) {
      if (newChannel == NULL) {
         PTRACE(4, "H4504\tCannot Retrieve session " << sessionId << " as hold media is NULL.");
         return NULL;
      }
  }

  PChannel * existingTransmitChannel = NULL;

  PINDEX count = logicalChannels->GetSize();

  for (PINDEX i = 0; i < count; ++i) {
    H323Channel* channel = logicalChannels->GetChannelAt(i);

    if (!channel) {
         PTRACE(4, "H4504\tLogical Channel " << i << " Empty or closed! Session ID: " << sessionId);
        // Fire off to ensure if channel is being Held that it is retrieved in derived application
         OnCallRetrieve(TRUE,sessionId, 0, newChannel);
         return NULL;
    }

    unsigned int session_id = channel->GetSessionID();
    if (session_id == sessionId) {
      const H323ChannelNumber & channelNumber = channel->GetNumber();

      H323_RTPChannel * chan2 = reinterpret_cast<H323_RTPChannel*>(channel);

      H323Codec * c = channel->GetCodec();
      if (!c) {
          return NULL;
      }
      H323Codec & codec = *c;
      PChannel * rawChannel = codec.GetRawDataChannel();
      unsigned frameRate = codec.GetFrameRate()*2;

      if (!channelNumber.IsFromRemote()) { // Transmit channel
        if (IsMediaOnHold()) {
          if (IsCallOnHold()) {
             PTRACE(4, "H4504\tHold Media OnHold Transmit " << i);
          existingTransmitChannel = codec.SwapChannel(newChannel);
              existingTransmitChannel = OnCallHold(TRUE,session_id, frameRate, existingTransmitChannel);
          } else {
             PTRACE(4, "H4504\tRetrieve Media OnHold Transmit " << i);
         existingTransmitChannel = codec.SwapChannel(OnCallRetrieve(TRUE, session_id, frameRate, existingTransmitChannel));
          }
        }
        else {
          // Enable/mute the transmit channel depending on whether the remote end is held
       if (IsCallOnHold()) {
              PTRACE(4, "H4504\tHold Transmit " << i);
              chan2->SetPause(TRUE);
              if (codec.SetRawDataHeld(TRUE))
                codec.SwapChannel(OnCallHold(TRUE, session_id, frameRate, rawChannel));
           } else {
              PTRACE(4, "H4504\tRetreive Transmit " << i);
              codec.SwapChannel(OnCallRetrieve(TRUE, session_id, frameRate, rawChannel));
              if (codec.SetRawDataHeld(FALSE))
                chan2->SetPause(FALSE);
           }
        }
      }
      else {
        // Enable/mute the receive channel depending on whether the remote end is held
          if (IsCallOnHold()) {
            PTRACE(4, "H4504\tHold Receive " << i);
            chan2->SetPause(TRUE);
             if (codec.SetRawDataHeld(TRUE))
                 codec.SwapChannel(OnCallHold(FALSE, session_id, frameRate, rawChannel));
          } else {
             PTRACE(4, "H4504\tRetrieve Receive " << i);
             codec.SwapChannel(OnCallRetrieve(FALSE, session_id, frameRate, rawChannel));
             if (codec.SetRawDataHeld(FALSE))
                 chan2->SetPause(FALSE);
          }
      }
    }
  }

  return existingTransmitChannel;
}

PChannel * H323Connection::OnCallHold(PBoolean /*IsEncoder*/,
                                  unsigned /*sessionId*/,
                                  unsigned /*bufferSize*/,
                                  PChannel * channel)
{
         return channel;
}

PChannel * H323Connection::OnCallRetrieve(PBoolean /*IsEncoder*/,
                                 unsigned /*sessionId*/,
                                 unsigned bufferSize,
                                 PChannel * channel)
{
    if (bufferSize == 0)
        return NULL;
    else
        return channel;
}

PBoolean H323Connection::IsLocalHold() const
{
  return h4504handler->GetState() == H4504Handler::e_ch_NE_Held;
}


PBoolean H323Connection::IsRemoteHold() const
{
  return h4504handler->GetState() == H4504Handler::e_ch_RE_Held;
}


PBoolean H323Connection::IsCallOnHold() const
{
  return h4504handler->GetState() != H4504Handler::e_ch_Idle;
}


void H323Connection::IntrudeCall(unsigned capabilityLevel)
{
  h45011handler->IntrudeCall(capabilityLevel);
}


void H323Connection::HandleIntrudeCall(const PString & token,
                                       const PString & identity)
{
  if (!token.IsEmpty() || !identity)
    h45011handler->AwaitSetupResponse(token, identity);
}


PBoolean H323Connection::GetRemoteCallIntrusionProtectionLevel(const PString & intrusionCallToken,
                                                           unsigned intrusionCICL)
{
  return h45011handler->GetRemoteCallIntrusionProtectionLevel(intrusionCallToken, intrusionCICL);
}


void H323Connection::SetIntrusionImpending()
{
  h45011handler->SetIntrusionImpending();
}


void H323Connection::SetForcedReleaseAccepted()
{
  h45011handler->SetForcedReleaseAccepted();
}


void H323Connection::SetIntrusionNotAuthorized()
{
  h45011handler->SetIntrusionNotAuthorized();
}


void H323Connection::SendCallWaitingIndication(const unsigned nbOfAddWaitingCalls)
{
  h4506handler->AttachToAlerting(*alertingPDU, nbOfAddWaitingCalls);
}

void H323Connection::SetMWINonCallParameters(const H323Connection::MWIInformation & mwiInfo)
{
    SetNonCallConnection();

    // Replace the default information
    mwiInformation = mwiInfo;
}

const H323Connection::MWIInformation & H323Connection::GetMWINonCallParameters()
{
    return mwiInformation;
}

PBoolean H323Connection::OnReceivedMWI(const MWIInformation & mwiInfo)
{
    return endpoint.OnReceivedMWI(mwiInfo);
}

PBoolean H323Connection::OnReceivedMWIClear(const PString & user)
{
    return endpoint.OnReceivedMWIClear(user);
}

PBoolean H323Connection::OnReceivedMWIRequest(const PString & user)
{
    return endpoint.OnReceivedMWIRequest(this, user);
}

#endif // H323_H450


PBoolean H323Connection::OnControlProtocolError(ControlProtocolErrors /*errorSource*/,
                                            const void * /*errorData*/)
{
  return TRUE;
}


static void SetRFC2833PayloadType(H323Capabilities & capabilities,
                                  OpalRFC2833 & rfc2833handler)
{
  H323Capability * capability = capabilities.FindCapability(H323_UserInputCapability::SubTypeNames[H323_UserInputCapability::SignalToneRFC2833]);
  if (capability != NULL) {
    RTP_DataFrame::PayloadTypes pt = ((H323_UserInputCapability*)capability)->GetPayloadType();
    if (rfc2833handler.GetPayloadType() != pt) {
      PTRACE(2, "H323\tUser Input RFC2833 payload type set to " << pt);
      rfc2833handler.SetPayloadType(pt);
    }
  }
}


void H323Connection::OnSendCapabilitySet(H245_TerminalCapabilitySet & /*pdu*/)
{
  // If we originated call, then check for RFC2833 capability and set payload type
  if (!callAnswered && rfc2833handler)
    SetRFC2833PayloadType(localCapabilities, *rfc2833handler);
}


void H323Connection::OnReceivedCapabilitySet(const H245_TerminalCapabilitySet & /*pdu*/)
{
    // do nothing
}

PBoolean H323Connection::OnReceivedCapabilitySet(const H323Capabilities & remoteCaps,
                                             const H245_MultiplexCapability * muxCap,
                                             H245_TerminalCapabilitySetReject & /*rejectPDU*/)
{
  if (muxCap != NULL) {
    if (muxCap->GetTag() != H245_MultiplexCapability::e_h2250Capability) {
      PTRACE(1, "H323\tCapabilitySet contains unsupported multiplex.");
      return FALSE;
    }

    const H245_H2250Capability & h225_0 = *muxCap;
    remoteMaxAudioDelayJitter = h225_0.m_maximumAudioDelayJitter;
  }

  // save this time as being when the reverse media channel was opened
  if (!reverseMediaOpenTime.IsValid())
    reverseMediaOpenTime = PTime();

  if (remoteCaps.GetSize() == 0) {
    // Received empty TCS, so close all transmit channels
    for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
      H245NegLogicalChannel & negChannel = logicalChannels->GetNegLogicalChannelAt(i);
      H323Channel * channel = negChannel.GetChannel();
      if (channel != NULL && !channel->GetNumber().IsFromRemote())
        negChannel.Close();
    }
    transmitterSidePaused = TRUE;
  }
  else { // Received non-empty TCS

    // If we had received a TCS=0 previously, or we have a remoteCapabilities which
    // was "faked" from the fast start data, overwrite it, don't merge it.
    if (transmitterSidePaused || !capabilityExchangeProcedure->HasReceivedCapabilities())
      remoteCapabilities.RemoveAll();

    if (!remoteCapabilities.Merge(remoteCaps))
      return FALSE;

    if (transmitterSidePaused) {
      transmitterSidePaused = FALSE;
      connectionState = HasExecutedSignalConnect;
      capabilityExchangeProcedure->Start(TRUE);
    }
    else {
      if (localCapabilities.GetSize() > 0)
        capabilityExchangeProcedure->Start(FALSE);

      // If we terminated call, then check for RFC2833 capability and set payload type
      if (callAnswered && rfc2833handler)
        SetRFC2833PayloadType(remoteCapabilities, *rfc2833handler);
    }
  }

  return OnCommonCapabilitySet(remoteCapabilities);
}

PBoolean H323Connection::OnCommonCapabilitySet(H323Capabilities & caps) const
{
    return TRUE;
}


void H323Connection::SendCapabilitySet(PBoolean empty)
{
  capabilityExchangeProcedure->Start(TRUE, empty);
}

void H323Connection::OnBearerCapabilityTransferRate(unsigned & bitRate)
{
    endpoint.OnBearerCapabilityTransferRate(bitRate);
}

void H323Connection::SetInitialBandwidth(H323Capability::MainTypes captype, int bitRate)
{
#ifdef H323_VIDEO
  for (PINDEX i=0; i< localCapabilities.GetSize(); ++i) {
    if (localCapabilities[i].GetMainType() == captype) {
      OpalMediaFormat & fmt = localCapabilities[i].GetWritableMediaFormat();
      if (fmt.GetOptionInteger(OpalVideoFormat::MaxBitRateOption) > bitRate)
             fmt.SetOptionInteger(OpalVideoFormat::MaxBitRateOption,bitRate);
    }
  }
#endif
}

void H323Connection::SetMaxPayloadSize(H323Capability::MainTypes captype, int size)
{
#ifdef H323_VIDEO
  for (PINDEX i=0; i< localCapabilities.GetSize(); ++i) {
    if (localCapabilities[i].GetMainType() == captype) {
      OpalMediaFormat & fmt = localCapabilities[i].GetWritableMediaFormat();
      if (fmt.HasOption(OpalVideoFormat::MaxPayloadSizeOption)) {
             fmt.SetOptionInteger(OpalVideoFormat::MaxPayloadSizeOption,size);
  //           if (fmt.HasOption("Generic Parameter 9"))   // for H.264....
  //               fmt.SetOptionInteger("Generic Parameter 9",size);
      }
    }
  }
#endif
}

void H323Connection::SetEmphasisSpeed(H323Capability::MainTypes captype, bool speed)
{
#ifdef H323_VIDEO
  for (PINDEX i=0; i< localCapabilities.GetSize(); ++i) {
    if (localCapabilities[i].GetMainType() == captype) {
      OpalMediaFormat & fmt = localCapabilities[i].GetWritableMediaFormat();
      if (fmt.HasOption(OpalVideoFormat::EmphasisSpeedOption))
          fmt.SetOptionBoolean(OpalVideoFormat::EmphasisSpeedOption,speed);
    }
  }
#endif
}


void H323Connection::OnSetLocalCapabilities()
{
}

void H323Connection::OnSetLocalUserInputCapabilities()
{
    if (!rfc2833InBandDTMF)
        localCapabilities.Remove("UserInput/RFC2833");

    if (!extendedUserInput)
        localCapabilities.Remove("UserInput/H249_*");
}

PBoolean H323Connection::IsH245Master() const
{
  return masterSlaveDeterminationProcedure->IsMaster();
}


void H323Connection::StartRoundTripDelay()
{
  if (Lock()) {
    if (masterSlaveDeterminationProcedure->IsDetermined() &&
        capabilityExchangeProcedure->HasSentCapabilities()) {
      if (roundTripDelayProcedure->IsRemoteOffline()) {
        PTRACE(2, "H245\tRemote failed to respond to PDU.");
        if (endpoint.ShouldClearCallOnRoundTripFail())
          ClearCall(EndedByTransportFail);
      }
      else
        roundTripDelayProcedure->StartRequest();
    }
    Unlock();
  }
}


PTimeInterval H323Connection::GetRoundTripDelay() const
{
  return roundTripDelayProcedure->GetRoundTripDelay();
}


void H323Connection::InternalEstablishedConnectionCheck()
{
  PTRACE(3, "H323\tInternalEstablishedConnectionCheck: "
            "connectionState=" << connectionState << " "
            "fastStartState=" << fastStartState);

  PBoolean h245_available = masterSlaveDeterminationProcedure->IsDetermined() &&
                        capabilityExchangeProcedure->HasSentCapabilities() &&
                        capabilityExchangeProcedure->HasReceivedCapabilities();

  if (h245_available)
    endSessionNeeded = TRUE;

  // Check for if all the 245 conditions are met so can start up logical
  // channels and complete the connection establishment.
  if (fastStartState != FastStartAcknowledged) {
    if (!h245_available)
      return;

    // If we are early starting, start channels as soon as possible instead of
    // waiting for connect PDU
    if (earlyStart && FindChannel(RTP_Session::DefaultAudioSessionID, FALSE) == NULL)
      OnSelectLogicalChannels();
  }

#ifdef H323_T120
  if (h245_available && startT120) {
    if (remoteCapabilities.FindCapability("T.120") != NULL) {
      H323Capability * capability = localCapabilities.FindCapability("T.120");
      if (capability != NULL)
        OpenLogicalChannel(*capability, 3, H323Channel::IsBidirectional);
    }
    startT120 = FALSE;
  }
#endif

#ifdef H323_H224
  if (h245_available && startH224) {
    if(remoteCapabilities.FindCapability("H.224") != NULL) {
      H323Capability * capability = localCapabilities.FindCapability("H.224");
      if(capability != NULL)
         OpenLogicalChannel(*capability,RTP_Session::DefaultH224SessionID, H323Channel::IsBidirectional);
    }
    startH224 = FALSE;
  }
#endif

  // Special case for Cisco CCM, when it does "early start" and opens its audio
  // channel to us, we better open one back or it hangs up!
  if ( h245_available &&
      !mediaWaitForConnect &&
       connectionState == AwaitingSignalConnect &&
       FindChannel(RTP_Session::DefaultAudioSessionID, TRUE) != NULL &&
       FindChannel(RTP_Session::DefaultAudioSessionID, FALSE) == NULL)
    OnSelectLogicalChannels();

  if (connectionState != HasExecutedSignalConnect)
    return;

  // Check if we have already got a transmitter running, select one if not
  if (FindChannel(RTP_Session::DefaultAudioSessionID, FALSE) == NULL)
    OnSelectLogicalChannels();

  connectionState = EstablishedConnection;

  if (signallingChannel)
      signallingChannel->SetCallEstablished();

  OnEstablished();
}

#if defined(H323_AUDIO_CODECS) || defined(H323_VIDEO) || defined(H323_T38) || defined(H323_FILE)

static void StartFastStartChannel(H323LogicalChannelList & fastStartChannels,
                                  unsigned sessionID, H323Channel::Directions direction)
{
  for (PINDEX i = 0; i < fastStartChannels.GetSize(); i++) {
    H323Channel & channel = fastStartChannels[i];
    if (channel.GetSessionID() == sessionID && channel.GetDirection() == direction) {
      fastStartChannels[i].Start();
      break;
    }
  }
}

#endif


void H323Connection::OnSelectLogicalChannels()
{
  PTRACE(2, "H245\tDefault OnSelectLogicalChannels, " << fastStartState);

  // Select the first codec that uses the "standard" audio session.
  switch (fastStartState) {
    default : //FastStartDisabled :
#ifdef H323_AUDIO_CODECS
      if (endpoint.CanAutoStartTransmitAudio())
         SelectDefaultLogicalChannel(RTP_Session::DefaultAudioSessionID);
#endif
#ifdef H323_VIDEO
      if (endpoint.CanAutoStartTransmitVideo())
        SelectDefaultLogicalChannel(RTP_Session::DefaultVideoSessionID);
#ifdef H323_H239
      if (endpoint.CanAutoStartTransmitExtVideo())
        SelectDefaultLogicalChannel(RTP_Session::DefaultExtVideoSessionID);
#endif
#endif // H323_VIDEO
#ifdef H323_T38
      if (endpoint.CanAutoStartTransmitFax())
        SelectDefaultLogicalChannel(RTP_Session::DefaultFaxSessionID);
#endif
      break;

    case FastStartInitiate :
#ifdef H323_AUDIO_CODECS
      SelectFastStartChannels(RTP_Session::DefaultAudioSessionID,
                              endpoint.CanAutoStartTransmitAudio(),
                              endpoint.CanAutoStartReceiveAudio());
#endif
#ifdef H323_VIDEO
      SelectFastStartChannels(RTP_Session::DefaultVideoSessionID,
                              endpoint.CanAutoStartTransmitVideo(),
                              endpoint.CanAutoStartReceiveVideo());
#ifdef H323_H239
      SelectFastStartChannels(RTP_Session::DefaultExtVideoSessionID,
                              endpoint.CanAutoStartTransmitExtVideo(),
                              endpoint.CanAutoStartReceiveExtVideo());
#endif
#endif // H323_VIDEO

#if defined(H323_T38) || defined(H323_FILE)
      SelectFastStartChannels(RTP_Session::DefaultFaxSessionID,
                              endpoint.CanAutoStartTransmitFax(),
                              endpoint.CanAutoStartReceiveFax());
#endif
      break;

    case FastStartResponse :
#ifdef H323_AUDIO_CODECS
      if (endpoint.CanAutoStartTransmitAudio())
        StartFastStartChannel(fastStartChannels, RTP_Session::DefaultAudioSessionID, H323Channel::IsTransmitter);
      if (endpoint.CanAutoStartReceiveAudio())
        StartFastStartChannel(fastStartChannels, RTP_Session::DefaultAudioSessionID, H323Channel::IsReceiver);
#endif
#ifdef H323_VIDEO
      if (endpoint.CanAutoStartTransmitVideo())
        StartFastStartChannel(fastStartChannels, RTP_Session::DefaultVideoSessionID, H323Channel::IsTransmitter);
      if (endpoint.CanAutoStartReceiveVideo())
        StartFastStartChannel(fastStartChannels, RTP_Session::DefaultVideoSessionID, H323Channel::IsReceiver);

#ifdef H323_H239
      if (endpoint.CanAutoStartTransmitExtVideo())
        StartFastStartChannel(fastStartChannels, RTP_Session::DefaultExtVideoSessionID, H323Channel::IsTransmitter);
      if (endpoint.CanAutoStartReceiveExtVideo())
        StartFastStartChannel(fastStartChannels, RTP_Session::DefaultExtVideoSessionID, H323Channel::IsReceiver);
#endif
#endif  // H323_VIDEO

#ifdef H323_T38
      if (endpoint.CanAutoStartTransmitFax())
        StartFastStartChannel(fastStartChannels, RTP_Session::DefaultFaxSessionID, H323Channel::IsTransmitter);
      if (endpoint.CanAutoStartReceiveFax())
        StartFastStartChannel(fastStartChannels, RTP_Session::DefaultFaxSessionID, H323Channel::IsReceiver);
#endif
      break;
  }
}


void H323Connection::SelectDefaultLogicalChannel(unsigned sessionID)
{
  if (FindChannel (sessionID, FALSE))
    return;

  for (PINDEX i = 0; i < localCapabilities.GetSize(); i++) {
    H323Capability & localCapability = localCapabilities[i];
    if (localCapability.GetDefaultSessionID() == sessionID) {
      H323Capability * remoteCapability = remoteCapabilities.FindCapability(localCapability);
      if (remoteCapability != NULL) {
        PTRACE(3, "H323\tSelecting " << *remoteCapability);

        MergeCapabilities(sessionID, localCapability, remoteCapability);

        if (OpenLogicalChannel(*remoteCapability, sessionID, H323Channel::IsTransmitter))
          break;
        PTRACE(2, "H323\tOnSelectLogicalChannels, OpenLogicalChannel failed: "
               << *remoteCapability);
      }
    }
  }
}


PBoolean H323Connection::MergeCapabilities(unsigned sessionID, const H323Capability & local, H323Capability * remote)
{

   OpalMediaFormat & remoteFormat = remote->GetWritableMediaFormat();
   const OpalMediaFormat & localFormat = local.GetMediaFormat();

   if (remoteFormat.Merge(localFormat)) {
#ifdef H323_VIDEO
       unsigned maxBitRate = remoteFormat.GetOptionInteger(OpalVideoFormat::MaxBitRateOption);
       unsigned targetBitRate = remoteFormat.GetOptionInteger(OpalVideoFormat::TargetBitRateOption);
       if (targetBitRate > maxBitRate)
          remoteFormat.SetOptionInteger(OpalVideoFormat::TargetBitRateOption, maxBitRate);
#endif
#if PTRACING
      PTRACE(6, "H323\tCapability Merge: ");
      OpalMediaFormat::DebugOptionList(remoteFormat);
#endif
      return TRUE;
   }
   return FALSE;
}

PBoolean H323Connection::MergeLanguages(const PStringList & remote, PBoolean /*isCaller*/)
{
    return MergeLanguages(remote);
}

PBoolean H323Connection::MergeLanguages(const PStringList & remote)
{
	PStringList common;
	common.SetSize(0);
	for (PINDEX i=0; i < remote.GetSize(); ++i) {
		for (PINDEX j=0; j < localLanguages.GetSize(); ++j) {
			if (remote[i] == localLanguages[j])
				common.AppendString(remote[i]);
		}
	}
	localLanguages = common;
	return OnCommonLanguages(localLanguages);
}

PBoolean H323Connection::OnCommonLanguages(const PStringList & lang)
{
	return (lang.GetSize() > 0);
}

void H323Connection::DisableFastStart()
{
    fastStartState = FastStartDisabled;
}


void H323Connection::SelectFastStartChannels(unsigned sessionID,
                                             PBoolean transmitter,
                                             PBoolean receiver)
{
  // Select all of the fast start channels to offer to the remote when initiating a call.
  for (PINDEX i = 0; i < localCapabilities.GetSize(); i++) {
    H323Capability & capability = localCapabilities[i];
    if (capability.GetDefaultSessionID() == sessionID) {
      if (receiver) {
        if (!OpenLogicalChannel(capability, sessionID, H323Channel::IsReceiver)) {
          PTRACE(2, "H323\tOnSelectLogicalChannels, OpenLogicalChannel rx failed: " << capability);
        }
      }
      if (transmitter) {
        if (!OpenLogicalChannel(capability, sessionID, H323Channel::IsTransmitter)) {
          PTRACE(2, "H323\tOnSelectLogicalChannels, OpenLogicalChannel tx failed: " << capability);
        }
      }
    }
  }
}


PBoolean H323Connection::OpenLogicalChannel(const H323Capability & capability,
                                        unsigned sessionID,
                                        H323Channel::Directions dir)
{
  switch (fastStartState) {
    default : // FastStartDisabled
      if (dir == H323Channel::IsReceiver)
        return FALSE;

      // Traditional H245 handshake
      return logicalChannels->Open(capability, sessionID);

    case FastStartResponse :
      // Do not use OpenLogicalChannel for starting these.
      return FALSE;

    case FastStartInitiate :
      break;
  }

  /*If starting a receiver channel and are initiating the fast start call,
    indicated by the remoteCapabilities being empty, we do a "trial"
    listen on the channel. That is, for example, the UDP sockets are created
    to receive data in the RTP session, but no thread is started to read the
    packets and pass them to the codec. This is because at this point in time,
    we do not know which of the codecs is to be used, and more than one thread
    cannot read from the RTP ports at the same time.
  */
  H323Channel * channel = capability.CreateChannel(*this, dir, sessionID, NULL);
  if (channel == NULL)
    return FALSE;

  if (dir != H323Channel::IsReceiver)
    channel->SetNumber(logicalChannels->GetNextChannelNumber());

  fastStartChannels.Append(channel);
  return TRUE;
}


PBoolean H323Connection::OnOpenLogicalChannel(const H245_OpenLogicalChannel & openPDU,
                                          H245_OpenLogicalChannelAck & ackPDU,
                                          unsigned & /*errorCode*/,
                                          const unsigned & sessionID)

{
  // If get a OLC via H.245 stop trying to do fast start
  fastStartState = FastStartDisabled;
  if (!fastStartChannels.IsEmpty()) {
    fastStartChannels.RemoveAll();
#ifdef P_STUN
    m_NATSockets.clear();
#endif
    PTRACE(1, "H245\tReceived early start OLC, aborting fast start");
  }

#ifdef H323_H46018
  PTRACE(4,"H323\tOnOpenLogicalChannel");
  if (openPDU.HasOptionalField(H245_OpenLogicalChannel::e_genericInformation)) {
         OnReceiveOLCGenericInformation(sessionID,openPDU.m_genericInformation,false);

         if (OnSendingOLCGenericInformation(sessionID,ackPDU.m_genericInformation,true))
             ackPDU.IncludeOptionalField(H245_OpenLogicalChannelAck::e_genericInformation);
  }
#endif

  //errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
  return true;
}


PBoolean H323Connection::OnConflictingLogicalChannel(H323Channel & conflictingChannel)
{
  unsigned session = conflictingChannel.GetSessionID();
  PTRACE(2, "H323\tLogical channel " << conflictingChannel
         << " conflict on session " << session
         << ", codec: " << conflictingChannel.GetCapability());

  /* Matrix of conflicts:
       Local EP is master and conflicting channel from remote (OLC)
          Reject remote transmitter (function is not called)
       Local EP is master and conflicting channel to remote (OLCAck)
          Should not happen (function is not called)
       Local EP is slave and conflicting channel from remote (OLC)
          Close sessions reverse channel from remote
          Start new reverse channel using codec in conflicting channel
          Accept the OLC for masters transmitter
       Local EP is slave and conflicting channel to remote (OLCRej)
          Start transmitter channel using codec in sessions reverse channel

      Upshot is this is only called if a slave and require a restart of
      some channel. Possibly closing channels as master has precedence.
   */

  PBoolean fromRemote = conflictingChannel.GetNumber().IsFromRemote();
  H323Channel * channel = FindChannel(session, !fromRemote);
  if (channel == NULL) {
    PTRACE(1, "H323\tCould not resolve conflict, no reverse channel.");
    return FALSE;
  }

  if (!fromRemote) {
    conflictingChannel.CleanUpOnTermination();
    H323Capability * capability = remoteCapabilities.FindCapability(channel->GetCapability());
    if (capability == NULL) {
      PTRACE(1, "H323\tCould not resolve conflict, capability not available on remote.");
      return FALSE;
    }
    OpenLogicalChannel(*capability, session, H323Channel::IsTransmitter);
    return TRUE;
  }

  // Shut down the conflicting channel that got in before our transmitter
  channel->CleanUpOnTermination();

  // Get the conflisting channel number to close
  H323ChannelNumber number = channel->GetNumber();

  // Must be slave and conflict from something we are sending, so try starting a
  // new channel using the master endpoints transmitter codec.
  logicalChannels->Open(conflictingChannel.GetCapability(), session, number);

  // Now close the conflicting channel
  CloseLogicalChannelNumber(number);
  return TRUE;
}


H323Channel * H323Connection::CreateLogicalChannel(const H245_OpenLogicalChannel & open,
                                                   PBoolean startingFast,
                                                   unsigned & errorCode)
{
  const H245_H2250LogicalChannelParameters * param = NULL;
  const H245_DataType * dataType = NULL;
  H323Channel::Directions direction;

  if (startingFast && open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
    if (open.m_reverseLogicalChannelParameters.m_multiplexParameters.GetTag() !=
              H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters
                                                      ::e_h2250LogicalChannelParameters) {
      errorCode = H245_OpenLogicalChannelReject_cause::e_unsuitableReverseParameters;
      PTRACE(2, "H323\tCreateLogicalChannel - reverse channel, H225.0 only supported");
      return NULL;
    }

    PTRACE(3, "H323\tCreateLogicalChannel - reverse channel");
    dataType = &open.m_reverseLogicalChannelParameters.m_dataType;
    param = &(const H245_H2250LogicalChannelParameters &)
                      open.m_reverseLogicalChannelParameters.m_multiplexParameters;
    direction = H323Channel::IsTransmitter;
  }
  else {
    if (open.m_forwardLogicalChannelParameters.m_multiplexParameters.GetTag() !=
              H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters
                                                      ::e_h2250LogicalChannelParameters) {
      PTRACE(2, "H323\tCreateLogicalChannel - forward channel, H225.0 only supported");
      errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
      return NULL;
    }

    PTRACE(3, "H323\tCreateLogicalChannel - forward channel");
    dataType = &open.m_forwardLogicalChannelParameters.m_dataType;
    param = &(const H245_H2250LogicalChannelParameters &)
                      open.m_forwardLogicalChannelParameters.m_multiplexParameters;
    direction = H323Channel::IsReceiver;
  }

  unsigned sessionID = param->m_sessionID;

#ifdef H323_VIDEO
#ifdef H323_H239
  if (sessionID == 0) {
    if (IsH245Master()) {
      // as master we assign the session ID
      sessionID = GetExtVideoRTPSessionID();
      const_cast<H245_H2250LogicalChannelParameters *>(param)->m_sessionID = sessionID; // TODO: make PDU (open) non-const all the way ?
      PTRACE(2, "H323\tAssigned RTP session ID " << sessionID);
    } else {
      PTRACE(2, "H323\tCreateLogicalChannel - received RTP session ID 0 as slave");
      return NULL;
    }
  }

  if (!startingFast &&
      open.HasOptionalField(H245_OpenLogicalChannel::e_genericInformation)) {  // check for extended Video OLC

    unsigned roleLabel = 0;
    H323ChannelNumber channelnum = H323ChannelNumber(open.m_forwardLogicalChannelNumber, TRUE);

    const H245_ArrayOf_GenericInformation & cape = open.m_genericInformation;
    for (PINDEX i = 0; i < cape.GetSize(); i++) {
       const H245_GenericMessage & gcap = cape[i];
       const PASN_ObjectId & object_id = gcap.m_messageIdentifier;
       if (object_id.AsString() == OpalPluginCodec_Identifer_H239_Video) {
           if (gcap.HasOptionalField(H245_GenericMessage::e_messageContent)) {
               const H245_ArrayOf_GenericParameter & params = gcap.m_messageContent;
               for (PINDEX j = 0; j < params.GetSize(); j++) {
                   const H245_GenericParameter & content = params[j];
                   const H245_ParameterValue & paramval = content.m_parameterValue;
                   if (paramval.GetTag() == H245_ParameterValue::e_booleanArray) {
                       const PASN_Integer & val = paramval;
                       roleLabel = val;
                   }
               }
           }
          OnReceivedExtendedVideoSession(roleLabel, channelnum);
       }
    }
  }
#endif
#endif // H323_VIDEO

  // See if datatype is supported
  H323Capability * capability = localCapabilities.FindCapability(*dataType);
  if (capability == NULL) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_unknownDataType;
    PTRACE(2, "H323\tCreateLogicalChannel - unknown data type");
    return NULL; // If codec not supported, return error
  }

  if (!capability->OnReceivedPDU(*dataType, direction == H323Channel::IsReceiver)) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeNotSupported;
    PTRACE(2, "H323\tCreateLogicalChannel - data type not supported");
    return NULL; // If codec not supported, return error
  }

  if (startingFast && (direction == H323Channel::IsTransmitter)) {
    H323Capability * remoteCapability = remoteCapabilities.FindCapability(*capability);
    if (remoteCapability != NULL)
      capability = remoteCapability;
    else {
      capability = remoteCapabilities.Copy(*capability);
      remoteCapabilities.SetCapability(0, 0, capability);
    }
  }

  if (!OnCreateLogicalChannel(*capability, direction, errorCode))
    return NULL; // If codec combination not supported, return error

  H323Channel * channel = capability->CreateChannel(*this, direction, sessionID, param);
  if (channel == NULL) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeNotAvailable;
    PTRACE(2, "H323\tCreateLogicalChannel - data type not available");
    return NULL;
  }

#ifdef H323_H239
  if ((channel->GetCapability().GetMainType() == H323Capability::e_Video)
      && (channel->GetCapability().GetSubType() == H245_VideoCapability::e_extendedVideoCapability)
      && !IsH245Master()) {
    // as slave remember the session ID for H.239 that the master has used
    SetExtVideoRTPSessionID(sessionID);
  }
#endif // H323_H239

  if (startingFast &&
      open.HasOptionalField(H245_OpenLogicalChannel::e_genericInformation))
          OnReceiveOLCGenericInformation(sessionID, open.m_genericInformation, false);

  if (!channel->SetInitialBandwidth())
    errorCode = H245_OpenLogicalChannelReject_cause::e_insufficientBandwidth;
  else if (channel->OnReceivedPDU(open, errorCode))
    return channel;

  PTRACE(2, "H323\tOnReceivedPDU gave error " << errorCode);
  delete channel;
  return NULL;
}


H323Channel * H323Connection::CreateRealTimeLogicalChannel(const H323Capability & capability,
                                                           H323Channel::Directions dir,
                                                           unsigned sessionID,
                               const H245_H2250LogicalChannelParameters * param,
                                                           RTP_QOS * rtpqos)
{
#ifdef H323_H235
  if (PIsDescendant(&capability, H323SecureCapability) || PIsDescendant(&capability, H323SecureExtendedCapability)) {
        // Override this function to add Secure ExternalRTPChannel Support
        // H323Channel * extChannel = new H323_ExternalRTPChannel(*this, capability, dir, sessionID, externalIpAddress, externalPort);
        // return new H323SecureChannel(this, capability, extChannel);

        // call H323_ExternalRTPChannel::OnReadFrame(RTP_DataFrame & frame) and H323_ExternalRTPChannel::OnWriteFrame(RTP_DataFrame & frame)
        // to encrypt and decrypt media
      return NULL;
  }
#endif

  RTP_Session * session = NULL;

  if (
#ifdef H323_H46026
     H46026IsMediaTunneled() ||
#endif
     !param || !param->HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel)) {
        // Make a fake transmprt address from the connection so gets initialised with
        // the transport type (IP, IPX, multicast etc).
        H245_TransportAddress addr;
        GetControlChannel().SetUpTransportPDU(addr, H323Transport::UseLocalTSAP);
        session = UseSession(sessionID, addr, dir, rtpqos);
  } else {
    session = UseSession(sessionID, param->m_mediaControlChannel, dir, rtpqos);
  }

  if (session == NULL)
    return NULL;

  return new H323_RTPChannel(*this, capability, dir, *session);
}


PBoolean H323Connection::OnCreateLogicalChannel(const H323Capability & capability,
                                            H323Channel::Directions dir,
                                            unsigned & errorCode)
{
  if (connectionState == ShuttingDownConnection) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
    return FALSE;
  }

  // Default error if returns FALSE
  errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeALCombinationNotSupported;

  // Check if in set at all
  if (dir != H323Channel::IsReceiver) {
    if (!remoteCapabilities.IsAllowed(capability)) {
      PTRACE(2, "H323\tOnCreateLogicalChannel - transmit capability " << capability << " not allowed.");
      return FALSE;
    }
  }
  else {
    if (!localCapabilities.IsAllowed(capability)) {
      PTRACE(2, "H323\tOnCreateLogicalChannel - receive capability " << capability << " not allowed.");
      return FALSE;
    }
  }

  // Check all running channels, and if new one can't run with it return FALSE
  for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
    H323Channel * channel = logicalChannels->GetChannelAt(i);
    if (channel != NULL && channel->GetDirection() == dir) {
      if (dir != H323Channel::IsReceiver) {
        if (!remoteCapabilities.IsAllowed(capability, channel->GetCapability())) {
          PTRACE(2, "H323\tOnCreateLogicalChannel - transmit capability " << capability
                 << " and " << channel->GetCapability() << " incompatible.");
          return FALSE;
        }
      }
      else {
        if (!localCapabilities.IsAllowed(capability, channel->GetCapability())) {
          PTRACE(2, "H323\tOnCreateLogicalChannel - receive capability " << capability
                 << " and " << channel->GetCapability() << " incompatible.");
          return FALSE;
        }
      }
    }
  }

  return TRUE;
}


PBoolean H323Connection::OnStartLogicalChannel(H323Channel & channel)
{
  if (channel.GetSessionID() == OpalMediaFormat::DefaultAudioSessionID &&
      PIsDescendant(&channel, H323_RTPChannel)) {
    H323_RTPChannel & rtp = (H323_RTPChannel &)channel;
    if (channel.GetNumber().IsFromRemote()) {
      if (rfc2833InBandDTMF && rfc2833handler)
        rtp.AddFilter(rfc2833handler->GetReceiveHandler());

      if (detectInBandDTMF) {
        H323Codec * codec = channel.GetCodec();
        if (codec != NULL)
          codec->AddFilter(PCREATE_NOTIFIER(OnUserInputInBandDTMF));
      }
    }
    else if (rfc2833InBandDTMF && rfc2833handler)
      rtp.AddFilter(rfc2833handler->GetTransmitHandler());
  }

#ifdef H323_H239
  if ((channel.GetCapability().GetMainType() == H323Capability::e_Video) &&
      (channel.GetCapability().GetSubType() == H245_VideoCapability::e_extendedVideoCapability)) {
          OnH239SessionStarted(channel.GetNumber(),
                channel.GetNumber().IsFromRemote() ? H323Capability::e_Receive : H323Capability::e_Transmit);
  }
#endif

  return endpoint.OnStartLogicalChannel(*this, channel);
}

PBoolean H323Connection::OnInitialFlowRestriction(H323Channel & channel)
{
#if H323_VIDEO
    if (channel.GetSessionID() == OpalMediaFormat::DefaultAudioSessionID)
         return true;

    if (!channel.GetNumber().IsFromRemote())
         return true;

    H323Codec * codec = channel.GetCodec();
    if (codec == NULL) return true;

    const OpalMediaFormat & fmt = codec->GetMediaFormat();
    unsigned maxBitRate = fmt.GetOptionInteger(OpalVideoFormat::MaxBitRateOption);
    unsigned targetBitRate = fmt.GetOptionInteger(OpalVideoFormat::TargetBitRateOption);

    if (targetBitRate < maxBitRate) {
        return SendLogicalChannelFlowControl(channel,targetBitRate/100);
    }
#endif
    return true;
}

#ifdef H323_AUDIO_CODECS
PBoolean H323Connection::OpenAudioChannel(PBoolean isEncoding, unsigned bufferSize, H323AudioCodec & codec)
{
#ifdef H323_AEC
  if (endpoint.AECEnabled() && (aec == NULL)) {
    PTRACE(2, "H323\tCreating AEC instance.");
    int rate = codec.GetMediaFormat().GetTimeUnits() * 1000;
    aec = new H323Aec(rate, codec.GetMediaFormat().GetFrameTime(), endpoint.GetSoundChannelBufferDepth());
  }
   codec.AttachAEC(aec);
#endif

  return endpoint.OpenAudioChannel(*this, isEncoding, bufferSize, codec);
}
#endif

#ifdef H323_VIDEO
PBoolean H323Connection::OpenVideoChannel(PBoolean isEncoding, H323VideoCodec & codec)
{
  return endpoint.OpenVideoChannel(*this, isEncoding, codec);
}
#endif // NO_H323_VIDEO


void H323Connection::CloseLogicalChannel(unsigned number, PBoolean fromRemote)
{
  if (connectionState != ShuttingDownConnection)
    logicalChannels->Close(number, fromRemote);
}


void H323Connection::CloseLogicalChannelNumber(const H323ChannelNumber & number)
{
  CloseLogicalChannel(number, number.IsFromRemote());
}


void H323Connection::CloseAllLogicalChannels(PBoolean fromRemote)
{
  for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
    H245NegLogicalChannel & negChannel = logicalChannels->GetNegLogicalChannelAt(i);
    H323Channel * channel = negChannel.GetChannel();
    if (channel != NULL && channel->GetNumber().IsFromRemote() == fromRemote)
      negChannel.Close();
  }
}


PBoolean H323Connection::OnClosingLogicalChannel(H323Channel & /*channel*/)
{
  return TRUE;
}


void H323Connection::OnClosedLogicalChannel(const H323Channel & channel)
{
#ifdef H323_H239
  if ((channel.GetCapability().GetMainType() == H323Capability::e_Video) &&
      (channel.GetCapability().GetSubType() == H245_VideoCapability::e_extendedVideoCapability)) {
          OnH239SessionEnded(channel.GetNumber(),
                channel.GetNumber().IsFromRemote() ? H323Capability::e_Receive : H323Capability::e_Transmit);
  }
#endif

  endpoint.OnClosedLogicalChannel(*this, channel);
}


void H323Connection::OnLogicalChannelFlowControl(H323Channel * channel,
                                                 long bitRateRestriction)
{
  if (channel != NULL)
    channel->OnFlowControl(bitRateRestriction);
}

PBoolean H323Connection::SendLogicalChannelFlowControl(const H323Channel & channel,
                                                       long restriction)
{
    H323ControlPDU pdu;
    H245_CommandMessage & command = pdu.Build(H245_CommandMessage::e_flowControlCommand);
    H245_FlowControlCommand & flowCommand = command;

    H245_FlowControlCommand_scope & scope = flowCommand.m_scope;
    scope.SetTag(H245_FlowControlCommand_scope::e_logicalChannelNumber);
    H245_LogicalChannelNumber & lc = scope;
    lc = channel.GetNumber();

    H245_FlowControlCommand_restriction & restrict = flowCommand.m_restriction;
    restrict.SetTag(H245_FlowControlCommand_restriction::e_maximumBitRate);
    PASN_Integer & bitRate = restrict;
    bitRate = restriction;

    return WriteControlPDU(pdu);
}

void H323Connection::OnLogicalChannelJitter(H323Channel * channel,
                                            DWORD jitter,
                                            int skippedFrameCount,
                                            int additionalBuffer)
{
  if (channel != NULL)
    channel->OnJitterIndication(jitter, skippedFrameCount, additionalBuffer);
}


// TODO: the bandwidth value returned here is wrong, usually, 1280
unsigned H323Connection::GetBandwidthUsed() const
{
  unsigned used = 0;

  for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
    H323Channel * channel = logicalChannels->GetChannelAt(i);
    if (channel != NULL)
      used += channel->GetBandwidthUsed();
  }

  PTRACE(3, "H323\tBandwidth used: " << used);

  return used;
}

#ifdef H323_VIDEO
void H323Connection::OnSetInitialBandwidth(H323VideoCodec * codec)
{
    endpoint.OnSetInitialBandwidth(codec);
}
#endif

PBoolean H323Connection::UseBandwidth(unsigned bandwidth, PBoolean removing)
{
  PTRACE(3, "H323\tBandwidth request: "
         << (removing ? '-' : '+')
         << bandwidth/10 << '.' << bandwidth%10
         << "kb/s, available: "
         << bandwidthAvailable/10 << '.' << bandwidthAvailable%10
         << "kb/s");

  if (removing)
    bandwidthAvailable += bandwidth;
  else {
    if (bandwidth > bandwidthAvailable) {
      PTRACE(2, "H323\tAvailable bandwidth exceeded");
      return FALSE;
    }

    bandwidthAvailable -= bandwidth;
  }

  return TRUE;
}


PBoolean H323Connection::SetBandwidthAvailable(unsigned newBandwidth, PBoolean force)
{
  unsigned used = GetBandwidthUsed();

  if (!OnSetBandwidthAvailable(newBandwidth*100,used*100))
      return false;

  if (used > newBandwidth) {
    if (!force)
      return FALSE;

    // Go through logical channels and close down some.
    PINDEX chanIdx = logicalChannels->GetSize();
    while (used > newBandwidth && chanIdx-- > 0) {
      H323Channel * channel = logicalChannels->GetChannelAt(chanIdx);
      if (channel != NULL) {
        used -= channel->GetBandwidthUsed();
        CloseLogicalChannelNumber(channel->GetNumber());
      }
    }
  }

  bandwidthAvailable = newBandwidth - used;

  return true;
}

PBoolean H323Connection::OnSetBandwidthAvailable(unsigned /*newBandwidth*/, unsigned /*available*/)
{
  return true;
}


void H323Connection::SetSendUserInputMode(SendUserInputModes mode)
{
  PAssert(mode != SendUserInputAsSeparateRFC2833, PUnimplementedFunction);

  PTRACE(2, "H323\tSetting default User Input send mode to " << mode);
  sendUserInputMode = mode;
}


static PBoolean CheckSendUserInputMode(const H323Capabilities & caps,
                                   H323Connection::SendUserInputModes mode)
{
  // If have remote capabilities, then verify we can send selected mode,
  // otherwise just return and accept it for future validation
  static const H323_UserInputCapability::SubTypes types[H323Connection::NumSendUserInputModes] = {
    H323_UserInputCapability::NumSubTypes,
    H323_UserInputCapability::BasicString,
    H323_UserInputCapability::SignalToneH245,
    H323_UserInputCapability::SignalToneRFC2833
#ifdef H323_H249
//    H323_UserInputCapability::SignalToneSeperateRFC2833,  // Not implemented
   ,H323_UserInputCapability::H249A_Navigation,
    H323_UserInputCapability::H249B_Softkey,
    H323_UserInputCapability::H249C_PointDevice,
    H323_UserInputCapability::H249D_Modal,
    H323_UserInputCapability::NumSubTypes
#endif
  };

  if (types[mode] == H323_UserInputCapability::NumSubTypes)
    return mode == H323Connection::SendUserInputAsQ931;

  return caps.FindCapability(H323_UserInputCapability::SubTypeNames[types[mode]]) != NULL;
}


H323Connection::SendUserInputModes H323Connection::GetRealSendUserInputMode() const
{
  // If have not yet exchanged capabilities (ie not finished setting up the
  // H.245 channel) then the only thing we can do is Q.931
  if (!capabilityExchangeProcedure->HasReceivedCapabilities())
    return SendUserInputAsQ931;

  // First try recommended mode
  if (CheckSendUserInputMode(remoteCapabilities, sendUserInputMode))
    return sendUserInputMode;

  // Then try H.245 tones
  if (CheckSendUserInputMode(remoteCapabilities, SendUserInputAsTone))
    return SendUserInputAsTone;

  // Finally if is H.245 alphanumeric or does not indicate it could do other
  // modes we use H.245 alphanumeric as per spec.
  return SendUserInputAsString;
}


void H323Connection::SendUserInput(const PString & value)
{
  SendUserInputModes mode = GetRealSendUserInputMode();

  PTRACE(2, "H323\tSendUserInput(\"" << value << "\"), using mode " << mode);
  PINDEX i;

  switch (mode) {
    case SendUserInputAsQ931 :
      SendUserInputIndicationQ931(value);
      break;

    case SendUserInputAsString :
      SendUserInputIndicationString(value);
      break;

    case SendUserInputAsTone :
      for (i = 0; i < value.GetLength(); i++)
        SendUserInputIndicationTone(value[i]);
      break;

    case SendUserInputAsInlineRFC2833 :
      for (i = 0; i < value.GetLength(); i++)
        if (rfc2833handler) rfc2833handler->SendTone(value[i], 180);
      break;

    default :
      ;
  }
}


void H323Connection::OnUserInputString(const PString & value)
{
  endpoint.OnUserInputString(*this, value);
}


void H323Connection::SendUserInputTone(char tone,
                                       unsigned duration,
                                       unsigned logicalChannel,
                                       unsigned rtpTimestamp)
{
  SendUserInputModes mode = GetRealSendUserInputMode();

  PTRACE(2, "H323\tSendUserInputTone("
         << tone << ','
         << duration << ','
         << logicalChannel << ','
         << rtpTimestamp << "), using mode " << mode);

  switch (mode) {
    case SendUserInputAsQ931 :
      SendUserInputIndicationQ931(PString(tone));
      break;

    case SendUserInputAsString :
      SendUserInputIndicationString(PString(tone));
      break;

    case SendUserInputAsTone :
      SendUserInputIndicationTone(tone, duration, logicalChannel, rtpTimestamp);
      break;

    case SendUserInputAsInlineRFC2833 :
      if (rfc2833handler) rfc2833handler->SendTone(tone, duration);
      break;

    default :
      ;
  }
}


void H323Connection::OnUserInputTone(char tone,
                                     unsigned duration,
                                     unsigned logicalChannel,
                                     unsigned rtpTimestamp)
{
  endpoint.OnUserInputTone(*this, tone, duration, logicalChannel, rtpTimestamp);
}


void H323Connection::SendUserInputIndicationQ931(const PString & value)
{
  PTRACE(2, "H323\tSendUserInputIndicationQ931(\"" << value << "\")");

  H323SignalPDU pdu;
  pdu.BuildInformation(*this);
  pdu.GetQ931().SetKeypad(value);
  WriteSignalPDU(pdu);
}


void H323Connection::SendUserInputIndicationString(const PString & value)
{
  PTRACE(2, "H323\tSendUserInputIndicationString(\"" << value << "\")");

  H323ControlPDU pdu;
  PASN_GeneralString & str = pdu.BuildUserInputIndication(value);
  if (!str.GetValue())
    WriteControlPDU(pdu);
  else {
    PTRACE(1, "H323\tInvalid characters for UserInputIndication");
  }
}


void H323Connection::SendUserInputIndicationTone(char tone,
                                                 unsigned duration,
                                                 unsigned logicalChannel,
                                                 unsigned rtpTimestamp)
{
  PTRACE(2, "H323\tSendUserInputIndicationTone("
         << tone << ','
         << duration << ','
         << logicalChannel << ','
         << rtpTimestamp << ')');

  H323ControlPDU pdu;
  pdu.BuildUserInputIndication(tone, duration, logicalChannel, rtpTimestamp);
  WriteControlPDU(pdu);
}

#ifdef H323_H249

void H323Connection::SendUserInputIndicationNavigate(H323_UserInputCapability::NavigateKeyID keyID)
{
 if (!CheckSendUserInputMode(remoteCapabilities,SendUserInputAsNavigation))
     return;

  PTRACE(2, "H323\tSendUserInputIndicationNavigate(" << keyID << ')');

  H323ControlPDU pdu;
  H245_UserInputIndication & ind = pdu.Build(H245_IndicationMessage::e_userInput);
  ind.SetTag(H245_UserInputIndication::e_genericInformation);
  H245_ArrayOf_GenericInformation & infolist = ind;

  H245_GenericInformation * info =
             H323_UserInputCapability::BuildGenericIndication(H323_UserInputCapability::SubTypeOID[0]);

   info->IncludeOptionalField(H245_GenericMessage::e_messageContent);
   H245_ArrayOf_GenericParameter & contents = info->m_messageContent;

   H245_GenericParameter * content =
        H323_UserInputCapability::BuildGenericParameter(1,H245_ParameterValue::e_unsignedMin,keyID);

   contents.Append(content);
   contents.SetSize(contents.GetSize()+1);

  infolist.Append(info);
  infolist.SetSize(infolist.GetSize()+1);
  WriteControlPDU(pdu);
}

void H323Connection::SendUserInputIndicationSoftkey(unsigned key, const PString & keyName)
{
 if (!CheckSendUserInputMode(remoteCapabilities,SendUserInputAsSoftkey))
     return;

  PTRACE(2, "H323\tSendUserInputIndicationSoftkey(" << key << ')');

  H323ControlPDU pdu;
  H245_UserInputIndication & ind = pdu.Build(H245_IndicationMessage::e_userInput);
  ind.SetTag(H245_UserInputIndication::e_genericInformation);
  H245_ArrayOf_GenericInformation & infolist = ind;

  H245_GenericInformation * info =
             H323_UserInputCapability::BuildGenericIndication(H323_UserInputCapability::SubTypeOID[1]);

   info->IncludeOptionalField(H245_GenericMessage::e_messageContent);
   H245_ArrayOf_GenericParameter & contents = info->m_messageContent;

   H245_GenericParameter * content =
        H323_UserInputCapability::BuildGenericParameter(2,H245_ParameterValue::e_unsignedMin,key);
    contents.Append(content);
    contents.SetSize(contents.GetSize()+1);

    if (keyName.GetLength() > 0) {
      H245_GenericParameter * contentstr =
         H323_UserInputCapability::BuildGenericParameter(1,H245_ParameterValue::e_octetString,keyName);
      contents.Append(contentstr);
      contents.SetSize(contents.GetSize()+1);
    }

  infolist.Append(info);
  infolist.SetSize(infolist.GetSize()+1);
  WriteControlPDU(pdu);
}

void H323Connection::SendUserInputIndicationPointDevice(unsigned x, unsigned y, unsigned button,
                                                           unsigned buttonstate, unsigned clickcount)
{
 if (!CheckSendUserInputMode(remoteCapabilities,SendUserInputAsPointDevice))
     return;

  PTRACE(6, "H323\tSendUserInputIndicationPointDevice");

  H323ControlPDU pdu;
  H245_UserInputIndication & ind = pdu.Build(H245_IndicationMessage::e_userInput);
  ind.SetTag(H245_UserInputIndication::e_genericInformation);
  H245_ArrayOf_GenericInformation & infolist = ind;

  H245_GenericInformation * info =
             H323_UserInputCapability::BuildGenericIndication(H323_UserInputCapability::SubTypeOID[2]);

   info->IncludeOptionalField(H245_GenericMessage::e_messageContent);
   H245_ArrayOf_GenericParameter & contents = info->m_messageContent;

/// Add X and Y co-ords
    H245_GenericParameter * X =
        H323_UserInputCapability::BuildGenericParameter(1,H245_ParameterValue::e_unsignedMin,x);
    contents.Append(X);
    contents.SetSize(contents.GetSize()+1);

    H245_GenericParameter * Y =
        H323_UserInputCapability::BuildGenericParameter(2,H245_ParameterValue::e_unsignedMin,y);
    contents.Append(Y);
    contents.SetSize(contents.GetSize()+1);

/// Optional values
    if (button > 0) {
      H245_GenericParameter * but =
        H323_UserInputCapability::BuildGenericParameter(3,H245_ParameterValue::e_unsignedMin,button);
      contents.Append(but);
      contents.SetSize(contents.GetSize()+1);
    }

    if (buttonstate > 0) {
      H245_GenericParameter * butstate =
        H323_UserInputCapability::BuildGenericParameter(4,H245_ParameterValue::e_unsignedMin,buttonstate);
      contents.Append(butstate);
      contents.SetSize(contents.GetSize()+1);
    }

    if (clickcount > 0) {
      H245_GenericParameter * cc =
        H323_UserInputCapability::BuildGenericParameter(5,H245_ParameterValue::e_unsignedMin,clickcount);
      contents.Append(cc);
      contents.SetSize(contents.GetSize()+1);
    }

  infolist.Append(info);
  infolist.SetSize(infolist.GetSize()+1);
  WriteControlPDU(pdu);
}

void H323Connection::SendUserInputIndicationModal()
{
 if (!CheckSendUserInputMode(remoteCapabilities,SendUserInputAsModal))
     return;

}
#endif

void H323Connection::SendUserInputIndication(const H245_UserInputIndication & indication)
{
  H323ControlPDU pdu;
  H245_UserInputIndication & ind = pdu.Build(H245_IndicationMessage::e_userInput);
  ind = indication;
  WriteControlPDU(pdu);
}


void H323Connection::OnUserInputIndication(const H245_UserInputIndication & ind)
{
  switch (ind.GetTag()) {
    case H245_UserInputIndication::e_alphanumeric :
      OnUserInputString((const PASN_GeneralString &)ind);
      break;

    case H245_UserInputIndication::e_signal :
    {
      const H245_UserInputIndication_signal & sig = ind;
      OnUserInputTone(sig.m_signalType[0],
                      sig.HasOptionalField(H245_UserInputIndication_signal::e_duration)
                                ? (unsigned)sig.m_duration : 0,
                      sig.m_rtp.m_logicalChannelNumber,
                      sig.m_rtp.m_timestamp);
      break;
    }
    case H245_UserInputIndication::e_signalUpdate :
    {
      const H245_UserInputIndication_signalUpdate & sig = ind;
      OnUserInputTone(' ', sig.m_duration, sig.m_rtp.m_logicalChannelNumber, 0);
      break;
    }
#ifdef H323_H249
    case H245_UserInputIndication::e_genericInformation :
    {
      const H245_ArrayOf_GenericInformation & sig = ind;
      if ((sig.GetSize() > 0) &&
         sig[0].HasOptionalField(H245_GenericMessage::e_subMessageIdentifier)) {
           const H245_CapabilityIdentifier & id = sig[0].m_messageIdentifier;
           if (id.GetTag() == H245_CapabilityIdentifier::e_standard) {
               const PASN_ObjectId & gid = id;
               PString sid = gid.AsString();
               if (sid == H323_UserInputCapability::SubTypeOID[0]) {          // Navigation
                    OnUserInputIndicationNavigate(sig[0].m_messageContent);
               } else if (sid == H323_UserInputCapability::SubTypeOID[1]) {   // Softkey
                    OnUserInputIndicationSoftkey(sig[0].m_messageContent);
               } else if (sid == H323_UserInputCapability::SubTypeOID[2]) {   // PointingDevice
                    OnUserInputIndicationPointDevice(sig[0].m_messageContent);
               } else if (sid == H323_UserInputCapability::SubTypeOID[3]) {   // Mode interface
                    OnUserInputIndicationModal(sig[0].m_messageContent);
               }
           }
      }
    }
#endif
  }
}


void H323Connection::OnUserInputInlineRFC2833(OpalRFC2833Info & info, H323_INT)
{
  if (!info.IsToneStart())
    OnUserInputTone(info.GetTone(), info.GetDuration(), 0, info.GetTimestamp());
}


void H323Connection::OnUserInputInBandDTMF(H323Codec::FilterInfo & info, H323_INT)
{
  // This function is set up as an 'audio filter'.
  // This allows us to access the 16 bit PCM audio (at 8Khz sample rate)
  // before the audio is passed on to the sound card (or other output device)

#ifdef P_DTMF
  // Pass the 16 bit PCM audio through the DTMF decoder
  dtmfTones = dtmfDecoder.Decode((short *)info.buffer, info.bufferLength/sizeof(short));
  if (!dtmfTones.IsEmpty()) {
    PTRACE(1, "DTMF detected. " << dtmfTones);
    for (PINDEX i = 0; i < dtmfTones.GetLength(); i++) {
#if PTLIB_VER < 270
      OnUserInputTone(dtmfTones[i], 0, 0, 0);
#else
      OnUserInputTone(dtmfTones[i], 0, 0, PDTMFDecoder::DetectTime);
#endif
    }
  }
#endif
}

#ifdef H323_H249
void H323Connection::OnUserInputIndicationNavigate(const H245_ArrayOf_GenericParameter & contents)
{
}

void H323Connection::OnUserInputIndicationSoftkey(const H245_ArrayOf_GenericParameter & contents)
{
}

void H323Connection::OnUserInputIndicationPointDevice(const H245_ArrayOf_GenericParameter & contents)
{
}

void H323Connection::OnUserInputIndicationModal(const H245_ArrayOf_GenericParameter & contents)
{
}
#endif

RTP_Session * H323Connection::GetSession(unsigned sessionID) const
{
  return rtpSessions.GetSession(sessionID);
}


H323_RTP_Session * H323Connection::GetSessionCallbacks(unsigned sessionID) const
{
  RTP_Session * session = rtpSessions.GetSession(sessionID);
  if (session == NULL)
    return NULL;

  PTRACE(3, "RTP\tFound existing session " << sessionID);
  PObject * data = session->GetUserData();
  PAssert(PIsDescendant(data, H323_RTP_Session), PInvalidCast);
  return (H323_RTP_Session *)data;
}


RTP_Session * H323Connection::UseSession(unsigned sessionID,
                                         const H245_TransportAddress & taddr,
                                                   H323Channel::Directions dir,
                                         RTP_QOS * rtpqos)
{
  // We only support unicast IP at this time.
  if (taddr.GetTag() != H245_TransportAddress::e_unicastAddress) {
    return NULL;
  }

  // We must have a valid sessionID  H.239 sometimes negotiates 0
  if (sessionID > 255)
      return NULL;

  const H245_UnicastAddress & uaddr = taddr;
  if (uaddr.GetTag() != H245_UnicastAddress::e_iPAddress
#ifdef H323_IPV6
        && uaddr.GetTag() != H245_UnicastAddress::e_iP6Address
#endif
     ) {
    return NULL;
  }

  RTP_Session * session = rtpSessions.UseSession(sessionID);
  if (session != NULL) {
    ((RTP_UDP *) session)->Reopen(dir == H323Channel::IsReceiver);
    return session;
  }

  RTP_UDP * udp_session = new RTP_UDP(
#ifdef H323_RTP_AGGREGATE
                  useRTPAggregation ? endpoint.GetRTPAggregator() : NULL,
#endif
                  sessionID, remoteIsNAT
#ifdef H323_H46026
                  , m_H46026enabled
#endif
                  );

  udp_session->SetUserData(new H323_RTP_UDP(*this, *udp_session, rtpqos));
  rtpSessions.AddSession(udp_session);
  return udp_session;
}

PBoolean H323Connection::OnHandleH245GenericMessage(h245MessageType type, const H245_GenericMessage & pdu)
{
    //if (!pdu.HasOptionalField(H245_GenericMessage::e_subMessageIdentifier)) {
    //    PTRACE(2,"H323\tUnIdentified Generic Message Received!");
    //    return false;
    //}

    PString guid = PString();
    const H245_CapabilityIdentifier & id = pdu.m_messageIdentifier;

    if (id.GetTag() == H245_CapabilityIdentifier::e_standard) {
              const PASN_ObjectId & gid = id;
              guid = gid.AsString();
    }
    else if (id.GetTag() == H245_CapabilityIdentifier::e_h221NonStandard) {
        PTRACE(2,"H323\tUnknown NonStandard Generic Message Received!");
              return false;
    }
    else if (id.GetTag() == H245_CapabilityIdentifier::e_uuid) {
              const PASN_OctetString & gid = id;
              guid = gid.AsString();
    }
    else if (id.GetTag() == H245_CapabilityIdentifier::e_domainBased) {
              const PASN_IA5String & gid = id;
              guid = gid;
    }

    if (pdu.HasOptionalField(H245_GenericMessage::e_messageContent))
        return OnReceivedGenericMessage(type,guid,pdu.m_messageContent);
    else
        return OnReceivedGenericMessage(type,guid);
}


#ifdef H323_H46024A

PBoolean H323Connection::ReceivedH46024AMessage(bool toStart)
{
    if (m_H46024Astate < 3) {
        if (m_H46024Ainitator && !toStart) {
            PTRACE(4,"H46024A\tCONFLICT: wait for Media initiate Indication");
            return true;
        }

        PTRACE(4,"H46024A\tReceived Indication to " << (toStart ? "initiate" : "wait for") << " direct connection");

            if (m_H46024Astate == 0)                // We are the receiver
                m_H46024Astate = (toStart ? 2 : 1);

            for (std::map<unsigned,NAT_Sockets>::const_iterator r = m_NATSockets.begin(); r != m_NATSockets.end(); ++r) {
                NAT_Sockets sockets = r->second;
                ((H46019UDPSocket *)sockets.rtp)->H46024Adirect(toStart);
                ((H46019UDPSocket *)sockets.rtcp)->H46024Adirect(toStart);
            }
    //    }

        if (!toStart) {
            PTRACE(4,"H46024A\tReply for remote to " << (!toStart ? "initiate" : "wait for") << " direct connection");
            SendH46024AMessage(!toStart);
        }
       m_H46024Astate = 3;
   }
    return true;
}

bool GetUnsignedGenericMessage(unsigned id, const H245_ArrayOf_GenericParameter & params, unsigned & val)
{
   for (PINDEX i=0; i < params.GetSize(); i++)
   {
      const H245_GenericParameter & param = params[i];
      const H245_ParameterIdentifier & idm = param.m_parameterIdentifier;
      if (idm.GetTag() == H245_ParameterIdentifier::e_standard) {
          const PASN_Integer & idx = idm;
          if (idx == id) {
             const H245_ParameterValue & genvalue = params[i].m_parameterValue;
             if ((genvalue.GetTag() == H245_ParameterValue::e_unsignedMin) ||
                (genvalue.GetTag() == H245_ParameterValue::e_unsignedMax) ||
                (genvalue.GetTag() == H245_ParameterValue::e_unsigned32Min) ||
                (genvalue.GetTag() == H245_ParameterValue::e_unsigned32Max)) {
                    const PASN_Integer & xval = genvalue;
                    val = xval;
                    return true;
             }
          }
      }
   }
    PTRACE(4,"H46024A\tError finding Transport parameter " << id);
    return false;
}

bool GetStringGenericOctetString(unsigned id, const H245_ArrayOf_GenericParameter & params, PString & str)
{
    for (PINDEX i = 0; i < params.GetSize(); i++) {
        const H245_GenericParameter & param = params[i];
        const H245_ParameterIdentifier & idm = param.m_parameterIdentifier;
        if (idm.GetTag() == H245_ParameterIdentifier::e_standard) {
            const PASN_Integer & idx = idm;
            if (idx == id) {
                const H245_ParameterValue & genvalue = params[i].m_parameterValue;
                if (genvalue.GetTag() == H245_ParameterValue::e_octetString) {
                    const PASN_OctetString & valg = genvalue;
                    PASN_IA5String data;
                    if (valg.DecodeSubType(data)) {
                        str = data;
                        return true;
                    }
                }
            }
        }
    }
    PTRACE(4,"H46024A\tError finding String parameter " << id);
    return false;
}

bool GetUnsignedGeneric(unsigned id, const H245_ArrayOf_GenericParameter & params, unsigned & num)
{
   for (PINDEX i=0; i < params.GetSize(); i++)
   {
      const H245_GenericParameter & param = params[i];
      const H245_ParameterIdentifier & idm = param.m_parameterIdentifier;
      if (idm.GetTag() == H245_ParameterIdentifier::e_standard) {
         const PASN_Integer & idx = idm;
          if (idx == id) {
             const H245_ParameterValue & genvalue = params[i].m_parameterValue;
             if (genvalue.GetTag() == H245_ParameterValue::e_unsigned32Min) {
                   const PASN_Integer & valg = genvalue;
                   num = valg;
                   return true;
             }
         }
      }
   }
    PTRACE(4,"H46024A\tError finding unsigned parameter " << id);
    return false;
}

bool GetTransportGenericOctetString(unsigned id, const H245_ArrayOf_GenericParameter & params, H323TransportAddress & str)
{
    for (PINDEX i = 0; i < params.GetSize(); i++) {
        const H245_GenericParameter & param = params[i];
        const H245_ParameterIdentifier & idm = param.m_parameterIdentifier;
        if (idm.GetTag() == H245_ParameterIdentifier::e_standard) {
            const PASN_Integer & idx = idm;
            if (idx == id) {
                const H245_ParameterValue & genvalue = params[i].m_parameterValue;
                if (genvalue.GetTag() == H245_ParameterValue::e_octetString) {
                    const PASN_OctetString & valg = genvalue;
                    H245_TransportAddress addr;
                    if (valg.DecodeSubType(addr)) {
                        str = H323TransportAddress(addr);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

H245_GenericParameter & BuildGenericOctetString(H245_GenericParameter & param, unsigned id, const PASN_Object & data)
{
     H245_ParameterIdentifier & idm = param.m_parameterIdentifier;
         idm.SetTag(H245_ParameterIdentifier::e_standard);
         PASN_Integer & idx = idm;
         idx = id;
         H245_ParameterValue & genvalue = param.m_parameterValue;
         genvalue.SetTag(H245_ParameterValue::e_octetString);
         PASN_OctetString & valg = genvalue;
         valg.EncodeSubType(data);
    return param;
}

H245_GenericParameter & BuildGenericOctetString(H245_GenericParameter & param, unsigned id, const H323TransportAddress & transport)
{
    H245_TransportAddress data;
    transport.SetPDU(data);
    return BuildGenericOctetString(param, id, data);
}

H245_GenericParameter & BuildGenericInteger(H245_GenericParameter & param, unsigned id, unsigned val)
{
     H245_ParameterIdentifier & idm = param.m_parameterIdentifier;
         idm.SetTag(H245_ParameterIdentifier::e_standard);
         PASN_Integer & idx = idm;
         idx = id;
         H245_ParameterValue & genvalue = param.m_parameterValue;
         genvalue.SetTag(H245_ParameterValue::e_unsignedMin);
         PASN_Integer & xval = genvalue;
         xval = val;
    return param;
}

H245_GenericParameter & BuildGenericUnsigned(H245_GenericParameter & param, unsigned id, unsigned val)
{
     H245_ParameterIdentifier & idm = param.m_parameterIdentifier;
         idm.SetTag(H245_ParameterIdentifier::e_standard);
         PASN_Integer & idx = idm;
         idx = id;
         H245_ParameterValue & genvalue = param.m_parameterValue;
         genvalue.SetTag(H245_ParameterValue::e_unsigned32Min);
         PASN_Integer & xval = genvalue;
         xval = val;
    return param;
}

void BuildH46024AIndication(H323ControlPDU & pdu, const PString & oid, bool sender)
{
      H245_GenericMessage & cap = pdu.Build(H245_IndicationMessage::e_genericIndication);
  //    cap.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
      H245_CapabilityIdentifier & id = cap.m_messageIdentifier;
      id.SetTag(H245_CapabilityIdentifier::e_standard);
      PASN_ObjectId & gid = id;
      gid.SetValue(oid);
    // Indicate whether remote can start channel. // standard does specify who starts - SH
      //cap.IncludeOptionalField(H245_GenericMessage::e_messageContent);
      //H245_ArrayOf_GenericParameter & data = cap.m_messageContent;
      //data.SetSize(1);
      //BuildGenericInteger(data[0], 0, (sender ? 1 : 0));
}
#endif // H323_H46024A

#ifdef H323_H46024B
bool DecodeH46024BRequest(unsigned id, const H245_ArrayOf_GenericParameter & params, H46024B_ArrayOf_AlternateAddress & val)
{
   for (PINDEX i=0; i < params.GetSize(); i++)
   {
      const H245_GenericParameter & param = params[i];
      const H245_ParameterIdentifier & idm = param.m_parameterIdentifier;
      if (idm.GetTag() == H245_ParameterIdentifier::e_standard) {
          const PASN_Integer & idx = idm;
          if (idx == id) {
             const H245_ParameterValue & genvalue = params[i].m_parameterValue;
             if (genvalue.GetTag() == H245_ParameterValue::e_octetString) {
                    const PASN_OctetString & xval = genvalue;
                    if (xval.DecodeSubType(val))
                      return true;
             }
          }
      }
   }
   PTRACE(4,"H46024B\tError finding H46024BRequest " << id);
   return false;
}

void BuildH46024BResponse(H323ControlPDU & pdu)
{
    H245_GenericMessage & cap = pdu.Build(H245_ResponseMessage::e_genericResponse);
      H245_CapabilityIdentifier & id = cap.m_messageIdentifier;
      id.SetTag(H245_CapabilityIdentifier::e_standard);
      PASN_ObjectId & gid = id;
      gid.SetValue(H46024BOID);

      cap.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
      PASN_Integer & num = cap.m_subMessageIdentifier;
      num = 1;

}

void BuildH46024BIndication(H323ControlPDU & pdu)
{
    H245_GenericMessage & cap = pdu.Build(H245_IndicationMessage::e_genericIndication);
      H245_CapabilityIdentifier & id = cap.m_messageIdentifier;
      id.SetTag(H245_CapabilityIdentifier::e_standard);
      PASN_ObjectId & gid = id;
      gid.SetValue(H46024BOID);
}

#endif  // H323_H46024B

#ifdef H323_H46024A
PBoolean H323Connection::SendH46024AMessage(bool sender)
{
    if ((sender && m_H46024Astate == 2) ||  // Message already sent
        (!sender && m_H46024Astate == 1))    // Message already sent
                return false;

    m_H46024Ainitator = sender;
    if (m_H46024Astate == 0)                // We are instigator
        m_H46024Astate = (sender ? 2 : 1);

    PTRACE(4,"H46024A\tSending Control DirectMedia " << (sender ? "Wait" : "Initiate"));

    H323ControlPDU pdu;
    BuildH46024AIndication(pdu,H46024AOID,sender);
    return WriteControlPDU(pdu);
}

#endif

PBoolean H323Connection::OnReceivedGenericMessage(h245MessageType type, const PString & id )
{
#ifdef H323_H46024A
    if (id == H46024AOID && type == h245indication) {
        PTRACE(4,"H46024A\tReceived Generic Message.");
        return ReceivedH46024AMessage(m_H46024Ainitator);
    }
#endif
#ifdef H323_H46024B
    if (id == H46024BOID && type == h245response) {
        PTRACE(4,"H46024B\tReceived Generic Response.");
        return true;
    }
#endif
    return false;
}

PBoolean H323Connection::OnReceivedGenericMessage(h245MessageType type, const PString & id, const H245_ArrayOf_GenericParameter & content)
{
#ifdef H323_H46024A
    if (id == H46024AOID && type == h245indication) {
        PTRACE(4,"H46024A\tReceived Generic Indication.");
          //  unsigned start=0;
          //  if (GetUnsignedGenericMessage(0,content,start))
                return ReceivedH46024AMessage(m_H46024Ainitator);
    }
#endif

#ifdef H323_H46024B
    if (id == H46024BOID && type == h245request) {
        H46024B_ArrayOf_AlternateAddress address;
        if (DecodeH46024BRequest(1, content, address)) {
            PTRACE(4,"H46024B\tReceived\n" << address);
            for (PINDEX i=0; i < address.GetSize(); ++i) {
                unsigned muxID = 0;
                if (address[i].HasOptionalField(H46024B_AlternateAddress::e_multiplexID))
                      muxID = address[i].m_multiplexID;
                std::map<unsigned,NAT_Sockets>::const_iterator sockets_iter = m_NATSockets.find(address[i].m_sessionID);
                    if (sockets_iter != m_NATSockets.end()) {
                        NAT_Sockets sockets = sockets_iter->second;
                        if (address[i].HasOptionalField(H46024B_AlternateAddress::e_rtpAddress)) {
                            H323TransportAddress add = H323TransportAddress(address[i].m_rtpAddress);
                            ((H46019UDPSocket *)sockets.rtp)->H46024Bdirect(add,muxID);
                        }
                    }
            }
            H323ControlPDU pdu;
            BuildH46024BResponse(pdu);
            return WriteControlPDU(pdu);
        }
    }
#endif

#ifdef H323_H239
   if (id == OpalPluginCodec_Identifer_H239_GenericMessage) {
     H239Control * ctrl = (H239Control *)remoteCapabilities.FindCapability("H.239 Control");
     if (!ctrl) return false;

     switch (type) {
        case h245request:
            return ctrl->HandleGenericMessage(H239Control::e_h245request,this, &content);
        case h245response:
            return ctrl->HandleGenericMessage(H239Control::e_h245response,this, &content);
        case h245command:
            return ctrl->HandleGenericMessage(H239Control::e_h245command,this, &content);
        case h245indication:
            return ctrl->HandleGenericMessage(H239Control::e_h245indication,this, &content);
     }
   }
#endif
    return false;
}

#ifdef H323_H239
PBoolean H323Connection::SendH239GenericResponse(PBoolean response)
{
   H239Control * ctrl = (H239Control *)remoteCapabilities.FindCapability("H.239 Control");
   if (ctrl)
       return ctrl->SendGenericMessage(H239Control::e_h245response,this,response);

   return false;
}

H245NegLogicalChannels * H323Connection::GetLogicalChannels()
{
    return logicalChannels;
}
#endif

PBoolean H323Connection::OnReceiveOLCGenericInformation(unsigned sessionID,
                        const H245_ArrayOf_GenericInformation & alternate,
                        PBoolean isAck
                        ) const
{
    PBoolean success = false;

#ifdef H323_H460
        PTRACE(4,"Handling Generic OLC Session " << sessionID );
        for (PINDEX i=0; i<alternate.GetSize(); i++) {
          const H245_GenericInformation & info = alternate[i];
          const H245_CapabilityIdentifier & id = info.m_messageIdentifier;
          if (id.GetTag() != H245_CapabilityIdentifier::e_standard)
              break;

#ifdef H323_H46018
            const PASN_ObjectId & oid = id;
            const H245_ArrayOf_GenericParameter & msg = info.m_messageContent;
            if (m_H46019enabled && (oid.AsString() == H46019OID) && msg.GetSize() > 0) {
                H245_GenericParameter & val = msg[0];
                 if (val.m_parameterValue.GetTag() != H245_ParameterValue::e_octetString)
                     break;

                    PASN_OctetString & raw = val.m_parameterValue;
                    PPER_Stream pdu(raw);
                    H46019_TraversalParameters params;
                    if (!params.Decode(pdu)) {
                        PTRACE(2,"H46019\tError decoding Traversal Parameters!");
                        break;
                    }

                  PTRACE(4,"H46019\tTraversal Parameters: Rec'd Session " << sessionID
                               << " " << (isAck ? "OLCack" : "OLC")  << "\n" << params);
#ifdef H323_H46019M
                  H323TransportAddress multiRTPaddress;
                  H323TransportAddress multiRTCPaddress;
                  unsigned             multiID=0;

                    if (params.HasOptionalField(H46019_TraversalParameters::e_multiplexedMediaChannel)) {
                        H245_TransportAddress & mRTP = params.m_multiplexedMediaChannel;
                        multiRTPaddress = H323TransportAddress(mRTP);
                    }

                    if (params.HasOptionalField(H46019_TraversalParameters::e_multiplexedMediaControlChannel)) {
                        H245_TransportAddress & mRTCP = params.m_multiplexedMediaControlChannel;
                        multiRTCPaddress = H323TransportAddress(mRTCP);
                    }

                    if (params.HasOptionalField(H46019_TraversalParameters::e_multiplexID)) {
                        PASN_Integer & mID = params.m_multiplexID;
                        multiID = mID;
                    }

                    if (!m_H46019multiplex && multiID > 0) {
                        PTRACE(2,"H46019\tMultiplex remote detected. To send Multiplexed!");
                    }
#endif

                    H323TransportAddress RTPaddress;
                    H323TransportAddress RTCPaddress;
                    bool keepAliveAddress = false;
                    if (params.HasOptionalField(H46019_TraversalParameters::e_keepAliveChannel)) {
                        H245_TransportAddress & k = params.m_keepAliveChannel;
                        RTPaddress = H323TransportAddress(k);
                        PIPSocket::Address add; WORD port = 0;
                        RTPaddress.GetIpAndPort(add,port);
                        RTCPaddress = H323TransportAddress(add,port+1);  // Compute the RTCP Address
                        keepAliveAddress = true;
                    }

                    unsigned payload = 0;
                    if (params.HasOptionalField(H46019_TraversalParameters::e_keepAlivePayloadType)) {
                        PASN_Integer & p = params.m_keepAlivePayloadType;
                        payload = p;
                    }

                    unsigned ttl = 0;
                    if (params.HasOptionalField(H46019_TraversalParameters::e_keepAliveInterval)) {
                        H225_TimeToLive & a = params.m_keepAliveInterval;
                        ttl = a;
                    }

                    std::map<unsigned,NAT_Sockets>::const_iterator sockets_iter = m_NATSockets.find(sessionID);
                        if (sockets_iter != m_NATSockets.end()) {
                            NAT_Sockets sockets = sockets_iter->second;
#ifdef H323_H46019M
                            if (multiID > 0) {
                               ((H46019UDPSocket *)sockets.rtp)->SetMultiplexID(multiID, isAck);
                               ((H46019UDPSocket *)sockets.rtcp)->SetMultiplexID(multiID, isAck);
                               if (keepAliveAddress) {
                                   PIPSocket::Address multiAddr;
                                   multiRTPaddress.GetIpAddress(multiAddr);    // Sanity check....
                                   if (!multiAddr.IsValid() || multiAddr.IsAny()|| multiAddr.IsLoopback()) {
                                      PTRACE(2,"H46019M\tInvalid Multiplex Address! Use Keepalive Address");
                                      multiRTPaddress = RTPaddress;
                                      multiRTCPaddress = RTCPaddress;
                                   }
                                  ((H46019UDPSocket *)sockets.rtp)->Activate(multiRTPaddress,payload,ttl);
                                  ((H46019UDPSocket *)sockets.rtcp)->Activate(multiRTCPaddress,payload,ttl);
                               }
                            } else
#endif
                            {
                              if (keepAliveAddress) {
                                ((H46019UDPSocket *)sockets.rtp)->Activate(RTPaddress,payload,ttl);
                                ((H46019UDPSocket *)sockets.rtcp)->Activate(RTCPaddress,payload,ttl);
                              }
                            }
                        }
                 success = true;
            }
#ifdef H323_H46024A
            if (m_H46024Aenabled && (oid.AsString() == H46024AOID)) {
                PTRACE(4,"H46024A\tAlt Port Info:\n" << msg);
                PString m_CUI = PString();  H323TransportAddress m_altAddr1, m_altAddr2; unsigned m_altMuxID=0;
                bool error = false;
                if (!GetStringGenericOctetString(0,msg,m_CUI))  error = true;
                if (!GetTransportGenericOctetString(1,msg,m_altAddr1))  error = true;
                if (!GetTransportGenericOctetString(2,msg,m_altAddr2))  error = true;
                GetUnsignedGeneric(3,msg,m_altMuxID);

                if (!error) {
                    std::map<unsigned,NAT_Sockets>::const_iterator sockets_iter = m_NATSockets.find(sessionID);
                        if (sockets_iter != m_NATSockets.end()) {
                            NAT_Sockets sockets = sockets_iter->second;
                            ((H46019UDPSocket *)sockets.rtp)->SetAlternateAddresses(m_altAddr1,m_CUI,m_altMuxID);
                            ((H46019UDPSocket *)sockets.rtcp)->SetAlternateAddresses(m_altAddr2,m_CUI,m_altMuxID);
                            success = true;
                        }
                }
            }
#endif
#endif  // H323_H46018
        }

#endif  // H323_H460
    return success;
}

PBoolean H323Connection::OnSendingOLCGenericInformation(const unsigned & sessionID,
                H245_ArrayOf_GenericInformation & generic, PBoolean isAck) const
{
#ifdef H323_H46018
    PTRACE(4,"Set Generic " << (isAck ? "OLCack" : "OLC") << " Session " << sessionID );
    if (m_H46019enabled) {
        unsigned payload=0; unsigned ttl=0; //H323TransportAddress m_keepAlive;
#ifdef H323_H46019M
        H323TransportAddress m_multiRTPAddress, m_multiRTCPAddress;
        unsigned multiID=0;
#endif
#ifdef H323_H46024A
        PString m_cui = PString();
        H323TransportAddress m_altAddr1, m_altAddr2;
        unsigned m_altMuxID=0;
#endif
        std::map<unsigned,NAT_Sockets>::const_iterator sockets_iter = m_NATSockets.find(sessionID);
            if (sockets_iter != m_NATSockets.end()) {
                NAT_Sockets sockets = sockets_iter->second;
                H46019UDPSocket * rtp = ((H46019UDPSocket *)sockets.rtp);
                H46019UDPSocket * rtcp = ((H46019UDPSocket *)sockets.rtcp);
                if (rtp->GetPingPayload() == 0)
                    rtp->SetPingPayLoad(defH46019payload);
                payload = rtp->GetPingPayload();

                if (rtp->GetTTL() == 0)
                    rtp->SetTTL(ttl);
                ttl = rtp->GetTTL();

                // Traversal Clients do not need to send a keepalive address
                // ToDo Server implementation  - SH
                //PIPSocket::Address addr;  WORD port=0;
                //if (rtp->GetLocalAddress(addr,port)) {
                //   m_keepAlive = H323TransportAddress(addr,port);
                //}
#ifdef H323_H46019M
                if (/*!isAck &&*/ m_H46019multiplex) {
                   rtp->GetMultiplexAddress(m_multiRTPAddress,multiID, isAck);
                   rtcp->GetMultiplexAddress(m_multiRTCPAddress,multiID, isAck);
                }
#endif
                if (isAck) {
                    rtp->Activate();  // Start the RTP Channel if not already started
                    rtcp->Activate();  // Start the RTCP Channel if not already started
                }
#ifdef H323_H46024A
              if (m_H46024Aenabled) {
                rtp->GetAlternateAddresses(m_altAddr1,m_cui, m_altMuxID);
                rtcp->GetAlternateAddresses(m_altAddr2,m_cui, m_altMuxID);
              }
#endif
            } else {
                PTRACE(4,"H46019\tERROR NAT Socket not found for " << sessionID << " ABORTING!" );
                return false;
            }

          H245_GenericInformation info;
          H245_CapabilityIdentifier & id = info.m_messageIdentifier;
            id.SetTag(H245_CapabilityIdentifier::e_standard);
            PASN_ObjectId & oid = id;
            oid.SetValue(H46019OID);

              bool h46019msg = false;
              H46019_TraversalParameters params;
#ifdef H323_H46019M
              if (m_H46019multiplex) {   // TODO: Change bool to direction multiplex to go
                    params.IncludeOptionalField(H46019_TraversalParameters::e_multiplexedMediaChannel);
                    H245_TransportAddress & mRTP = params.m_multiplexedMediaChannel;
                    m_multiRTPAddress.SetPDU(mRTP);

                    params.IncludeOptionalField(H46019_TraversalParameters::e_multiplexedMediaControlChannel);
                    H245_TransportAddress & mRTCP = params.m_multiplexedMediaControlChannel;
                    m_multiRTCPAddress.SetPDU(mRTCP);

                    params.IncludeOptionalField(H46019_TraversalParameters::e_multiplexID);
                    PASN_Integer & mID = params.m_multiplexID;
                    mID = multiID;
                    h46019msg = true;
              }
#endif
               // Traversal Clients do not need to send a keepalive address only Servers - SH
              //if (!isAck) {
              //      params.IncludeOptionalField(H46019_TraversalParameters::e_keepAliveChannel);
              //      H245_TransportAddress & mKeep = params.m_keepAliveChannel;
              //      m_keepAlive.SetPDU(mKeep);
              //      h46019msg = true;
              //}

              if (!isAck && ttl > 0) {
                    params.IncludeOptionalField(H46019_TraversalParameters::e_keepAliveInterval);
                    H225_TimeToLive & a = params.m_keepAliveInterval;
                    a = ttl;
                    h46019msg = true;
              }

              if (isAck && payload > 0) {
                    params.IncludeOptionalField(H46019_TraversalParameters::e_keepAlivePayloadType);
                    PASN_Integer & p = params.m_keepAlivePayloadType;
                    p = payload;
                    h46019msg = true;
              }

              if (h46019msg) {
                PTRACE(4,"H46019\tTraversal Parameters: Send Session " << sessionID
                                    << " " << (isAck ? "OLCack" : "OLC")  << "\n" << params);
                info.IncludeOptionalField(H245_GenericMessage::e_messageContent);
                H245_ArrayOf_GenericParameter & msg = info.m_messageContent;
                H245_GenericParameter genericParameter;
                H245_ParameterIdentifier & idm = genericParameter.m_parameterIdentifier;
                    idm.SetTag(H245_ParameterIdentifier::e_standard);
                    PASN_Integer & idx = idm;
                    idx = 1;
                genericParameter.m_parameterValue.SetTag(H245_ParameterValue::e_octetString);
                H245_ParameterValue & octetValue = genericParameter.m_parameterValue;
                PASN_OctetString & raw = octetValue;
                raw.EncodeSubType(params);
                msg.SetSize(1);
                msg[0] = genericParameter;
              }
          PINDEX sz = generic.GetSize();
          generic.SetSize(sz+1);
          generic[sz] = info;

#ifdef H323_H46024A
          if (m_H46024Aenabled) {
              H245_GenericInformation alt;
              H245_CapabilityIdentifier & altid = alt.m_messageIdentifier;
                id.SetTag(H245_CapabilityIdentifier::e_standard);
                PASN_ObjectId & oid = altid;
                oid.SetValue(H46024AOID);
                alt.IncludeOptionalField(H245_GenericMessage::e_messageContent);
                H245_ArrayOf_GenericParameter & msg = alt.m_messageContent;
                msg.SetSize(3);
                  BuildGenericOctetString(msg[0],0,(PASN_IA5String)m_cui);
                  BuildGenericOctetString(msg[1],1,m_altAddr1);
                  BuildGenericOctetString(msg[2],2,m_altAddr2);

                  if (m_altMuxID) {
                      msg.SetSize(4);
                      BuildGenericUnsigned(msg[3],3,m_altMuxID);
                  }
               PTRACE(5,"H46024A\tAltInfo:\n" << alt);
              PINDEX sz = generic.GetSize();
              generic.SetSize(sz+1);
              generic[sz] = alt;
          }
#endif
          if (generic.GetSize() > 0)
                return true;
    }
#endif

  return false;
}

void H323Connection::ReleaseSession(unsigned sessionID)
{
    // Clunge for H.239 which opens with a sessionID of 0
    if ((rtpSessions.GetSession(sessionID) == NULL) && (sessionID > 3))
               sessionID = 0;

#ifdef H323_H46024A
   const RTP_Session * sess = GetSession(sessionID);
   if (sess && sess->GetReferenceCount() == 1) {  // last session reference
      std::map<unsigned,NAT_Sockets>::iterator sockets_iter = m_NATSockets.find(sessionID);
      if (sockets_iter != m_NATSockets.end())
         m_NATSockets.erase(sockets_iter);
      else {
         sockets_iter = m_NATSockets.find(0);
         if (sockets_iter != m_NATSockets.end())
              m_NATSockets.erase(sockets_iter);
      }
   }
#endif
  rtpSessions.ReleaseSession(sessionID);
}

void H323Connection::UpdateSession(unsigned oldSessionID, unsigned newSessionID)
{
    rtpSessions.MoveSession(oldSessionID,newSessionID);
}


void H323Connection::OnRTPStatistics(const RTP_Session & session) const
{
#ifdef H323_H4609
       if (!m_h4609Final && session.GetPacketsReceived() > 0)
           (PRemoveConst(H323Connection,this))->H4609QueueStats(session);
#endif
  endpoint.OnRTPStatistics(*this, session);
}

void H323Connection::OnRTPFinalStatistics(const RTP_Session & session) const
{
#ifdef H323_H4609
       if (session.GetPacketsReceived() > 0)
           (PRemoveConst(H323Connection,this))->H4609QueueStats(session);
#endif
  endpoint.OnRTPFinalStatistics(*this, session);
}

void H323Connection::OnRxSenderReport(unsigned sessionID, const RTP_Session::SenderReport & send,
        const RTP_Session::ReceiverReportArray & recv) const
{
}

#ifdef H323_H460IM

PBoolean H323Connection::IMSupport()
{
    return m_IMsupport;
}

void H323Connection::SetIMSupport(PBoolean state)
{
    m_IMsupport = state;
}

PBoolean H323Connection::IMSession()
{
    return m_IMsession;
}

void H323Connection::SetIMSession(PBoolean state)
{
    m_IMsession = state;
}

PBoolean H323Connection::IMCall()
{
    return m_IMcall;
}

void H323Connection::SetIMCall(PBoolean state)
{
    m_IMcall = state;
}

const PString & H323Connection::IMMsg()
{
    return m_IMmsg;
}

void H323Connection::SetIMMsg(const PString & msg)
{
    m_IMmsg = msg;
}

#endif

#ifdef H323_H4609

H323Connection::H4609Statistics::H4609Statistics()
{
    meanEndToEndDelay = 0;
    worstEndToEndDelay = 0;
    packetsReceived = 0;
    accumPacketLost = 0;
    packetLossRate = 0;
    fractionLostRate = 0;
    meanJitter = 0;
    worstJitter = 0;
    bandwidth = 0;
    sessionid = 1;
}

void H323Connection::H4609QueueStats(const RTP_Session & session)
{
   if (!m_h4609enabled)
       return;

    H4609Statistics * stat = new H4609Statistics();
    stat->sendRTPaddr  = H323TransportAddress(session.GetLocalTransportAddress());
    stat->recvRTPaddr  = H323TransportAddress(session.GetRemoteTransportAddress());
//     stat->sendRTCPaddr = H323TransportAddress();
//   stat->recvRTCPaddr = H323TransportAddress();
    stat->sessionid = session.GetSessionID();
    stat->meanEndToEndDelay = session.GetAverageSendTime();
    stat->worstEndToEndDelay = session.GetMaximumSendTime();
    stat->packetsReceived = session.GetPacketsReceived();
    stat->accumPacketLost = session.GetPacketsLost();
    stat->packetLossRate = session.GetPacketsLost() / session.GetPacketsReceived();
    stat->fractionLostRate = stat->packetLossRate * 100;
    stat->meanJitter = session.GetAvgJitterTime();
    stat->worstJitter = session.GetMaxJitterTime();
    if (session.GetPacketsReceived() > 0 && session.GetAverageReceiveTime() > 0)
      stat->bandwidth  = (unsigned)((session.GetOctetsReceived()/session.GetPacketsReceived()/session.GetAverageReceiveTime())*1000);

    m_h4609Stats.Enqueue(stat);
}


H323Connection::H4609Statistics * H323Connection::H4609DequeueStats()
{
    if (m_h4609Stats.GetSize() == 0)
        return NULL;

    return (m_h4609Stats.Dequeue());
}

void H323Connection::H4609EnableStats()
{
    m_h4609enabled = true;
}

void H323Connection::H4609StatsFinal(PBoolean final)
{
    m_h4609Final = final;
}
#endif

static void AddSessionCodecName(PStringStream & name, H323Channel * channel)
{
  if (channel == NULL)
    return;

  H323Codec * codec = channel->GetCodec();
  if (codec == NULL)
    return;

  OpalMediaFormat mediaFormat = codec->GetMediaFormat();
  if (mediaFormat.IsEmpty())
    return;

  if (name.IsEmpty())
    name << mediaFormat;
  else if (name != mediaFormat)
    name << " / " << mediaFormat;
}


PString H323Connection::GetSessionCodecNames(unsigned sessionID) const
{
  PStringStream name;

  AddSessionCodecName(name, FindChannel(sessionID, FALSE));
  AddSessionCodecName(name, FindChannel(sessionID, TRUE));

  return name;
}


PBoolean H323Connection::RequestModeChange(const PString & newModes)
{
  return requestModeProcedure->StartRequest(newModes);
}


PBoolean H323Connection::RequestModeChange(const H245_ArrayOf_ModeDescription & newModes)
{
  return requestModeProcedure->StartRequest(newModes);
}


PBoolean H323Connection::OnRequestModeChange(const H245_RequestMode & pdu,
                                         H245_RequestModeAck & /*ack*/,
                                         H245_RequestModeReject & /*reject*/,
                                         PINDEX & selectedMode)
{
  for (selectedMode = 0; selectedMode < pdu.m_requestedModes.GetSize(); selectedMode++) {
    PBoolean ok = TRUE;
    for (PINDEX i = 0; i < pdu.m_requestedModes[selectedMode].GetSize(); i++) {
      if (localCapabilities.FindCapability(pdu.m_requestedModes[selectedMode][i]) == NULL) {
        ok = FALSE;
        break;
      }
    }
    if (ok)
      return TRUE;
  }

  PTRACE(1, "H245\tMode change rejected as does not have capabilities");
  return FALSE;
}


void H323Connection::OnModeChanged(const H245_ModeDescription & newMode)
{
  CloseAllLogicalChannels(FALSE);

  // Start up the new ones
  for (PINDEX i = 0; i < newMode.GetSize(); i++) {
    H323Capability * capability = localCapabilities.FindCapability(newMode[i]);
    if (PAssertNULL(capability) != NULL)  {// Should not occur as OnRequestModeChange checks them
      if (!OpenLogicalChannel(*capability,
                              capability->GetDefaultSessionID(),
                              H323Channel::IsTransmitter)) {
        PTRACE(1, "H245\tCould not open channel after mode change: " << *capability);
      }
    }
  }
}


void H323Connection::OnAcceptModeChange(const H245_RequestModeAck & pdu)
{
#if H323_T38
  if (t38ModeChangeCapabilities.IsEmpty())
    return;

  PTRACE(2, "H323\tT.38 mode change accepted.");

  // Now we have conviced the other side to send us T.38 data we should do the
  // same assuming the RequestModeChangeT38() function provided a list of \n
  // separaete capability names to start. Only one will be.

  CloseAllLogicalChannels(FALSE);

  PStringArray modes = t38ModeChangeCapabilities.Lines();

  PINDEX first, last;
  if (pdu.m_response.GetTag() == H245_RequestModeAck_response::e_willTransmitMostPreferredMode) {
    first = 0;
    last = 1;
  }
  else {
    first = 1;
    last = modes.GetSize();
  }

  for (PINDEX i = first; i < last; i++) {
    H323Capability * capability = localCapabilities.FindCapability(modes[i]);
    if (capability != NULL && OpenLogicalChannel(*capability,
                                                 capability->GetDefaultSessionID(),
                                                 H323Channel::IsTransmitter)) {
      PTRACE(1, "H245\tOpened " << *capability << " after T.38 mode change");
      break;
    }

    PTRACE(1, "H245\tCould not open channel after T.38 mode change");
  }

  t38ModeChangeCapabilities = PString::Empty();
#endif
}


void H323Connection::OnRefusedModeChange(const H245_RequestModeReject * /*pdu*/)
{
#ifdef H323_T38
  if (!t38ModeChangeCapabilities) {
    PTRACE(2, "H323\tT.38 mode change rejected.");
    t38ModeChangeCapabilities = PString::Empty();
  }
#endif
}

void H323Connection::OnSendARQ(H225_AdmissionRequest & arq)
{
#ifdef H323_H460
    if (OnSendFeatureSet(H460_MessageType::e_admissionRequest, arq.m_featureSet, true))
        arq.IncludeOptionalField(H225_AdmissionRequest::e_featureSet);

    H225_FeatureSet fs;
    if (OnSendFeatureSet(H460_MessageType::e_admissionRequest, fs, false)) {
          if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)) {
            arq.IncludeOptionalField(H225_AdmissionRequest::e_genericData);

            H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
            H225_ArrayOf_GenericData & data = arq.m_genericData;

            for (PINDEX i=0; i < fsn.GetSize(); i++) {
                 PINDEX lastPos = data.GetSize();
                 data.SetSize(lastPos+1);
                 data[lastPos] = fsn[i];
            }
          }
     }
#endif
  endpoint.OnSendARQ(*this, arq);
}

void H323Connection::OnReceivedACF(const H225_AdmissionConfirm & acf)
{
#ifdef H323_H460
    if (acf.HasOptionalField(H225_AdmissionConfirm::e_featureSet))
        OnReceiveFeatureSet(H460_MessageType::e_admissionConfirm, acf.m_featureSet);

    if (acf.HasOptionalField(H225_AdmissionConfirm::e_genericData)) {
        const H225_ArrayOf_GenericData & data = acf.m_genericData;

      if (data.GetSize() > 0) {
         H225_FeatureSet fs;
         fs.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);
         H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
         fsn.SetSize(data.GetSize());
            for (PINDEX i=0; i < data.GetSize(); i++)
                 fsn[i] = (H225_FeatureDescriptor &)data[i];

         OnReceiveFeatureSet(H460_MessageType::e_admissionConfirm, fs);
      }
    }
#endif
  endpoint.OnReceivedACF(*this, acf);
}

void H323Connection::OnReceivedARJ(const H225_AdmissionReject & arj)
{

    if (arj.m_rejectReason.GetTag() == H225_AdmissionRejectReason::e_routeCallToGatekeeper) {
      H323SignalPDU facilityPDU;
      H225_Facility_UUIE * fac = facilityPDU.BuildFacility(*this,false, H225_FacilityReason::e_routeCallToGatekeeper);

      H323Gatekeeper * gatekeeper = endpoint.GetGatekeeper();
      if (gatekeeper) {
         H323TransportAddress add = gatekeeper->GetGatekeeperRouteAddress();
         fac->IncludeOptionalField(H225_Facility_UUIE::e_alternativeAddress);
         add.SetPDU(fac->m_alternativeAddress);
         WriteSignalPDU(facilityPDU);
      }
    }

#ifdef H323_H460
    if (arj.HasOptionalField(H225_AdmissionReject::e_featureSet))
        OnReceiveFeatureSet(H460_MessageType::e_admissionConfirm, arj.m_featureSet);

    if (arj.HasOptionalField(H225_AdmissionReject::e_genericData)) {
        const H225_ArrayOf_GenericData & data = arj.m_genericData;

      if (data.GetSize() > 0) {
         H225_FeatureSet fs;
         fs.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);
         H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
         fsn.SetSize(data.GetSize());
            for (PINDEX i=0; i < data.GetSize(); i++)
                 fsn[i] = (H225_FeatureDescriptor &)data[i];

         OnReceiveFeatureSet(H460_MessageType::e_admissionReject, fs);
      }
    }
#endif
  endpoint.OnReceivedARJ(*this, arj);
}

void H323Connection::OnSendIRR(H225_InfoRequestResponse & irr) const
{
#ifdef H323_H460
    H225_FeatureSet fs;
    if (OnSendFeatureSet(H460_MessageType::e_inforequestresponse, fs, false)) {
        if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)) {
            irr.IncludeOptionalField(H225_InfoRequestResponse::e_genericData);

            H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
            H225_ArrayOf_GenericData & data = irr.m_genericData;

            for (PINDEX i=0; i < fsn.GetSize(); i++) {
                 PINDEX lastPos = data.GetSize();
                 data.SetSize(lastPos+1);
                 data[lastPos] = fsn[i];
            }
        }
     }
#endif
}

void H323Connection::OnSendDRQ(H225_DisengageRequest & drq) const
{
#ifdef H323_H460
    H225_FeatureSet fs;
    if (OnSendFeatureSet(H460_MessageType::e_disengagerequest, fs, false)) {
          if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)) {
            drq.IncludeOptionalField(H225_DisengageRequest::e_genericData);

            H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
            H225_ArrayOf_GenericData & data = drq.m_genericData;

            for (PINDEX i=0; i < fsn.GetSize(); i++) {
                 PINDEX lastPos = data.GetSize();
                 data.SetSize(lastPos+1);
                 data[lastPos] = fsn[i];
            }
          }
     }
#endif
}


#ifdef H323_T120

OpalT120Protocol * H323Connection::CreateT120ProtocolHandler()
{
  if (t120handler == NULL)
    t120handler = endpoint.CreateT120ProtocolHandler(*this);
  return t120handler;
}

#endif

#ifdef H323_T38

OpalT38Protocol * H323Connection::CreateT38ProtocolHandler()
{
  if (t38handler == NULL)
    t38handler = endpoint.CreateT38ProtocolHandler(*this);
  return t38handler;
}


PBoolean H323Connection::RequestModeChangeT38(const char * capabilityNames)
{
  t38ModeChangeCapabilities = capabilityNames;
  if (RequestModeChange(t38ModeChangeCapabilities))
    return TRUE;

  t38ModeChangeCapabilities = PString::Empty();
  return FALSE;
}

#endif // H323_T38

#ifdef H323_H224
OpalH224Handler * H323Connection::CreateH224ProtocolHandler(H323Channel::Directions dir, unsigned sessionID)
{
  return endpoint.CreateH224ProtocolHandler(dir, *this, sessionID);
}

PBoolean H323Connection::OnCreateH224Handler(H323Channel::Directions dir, const PString & id, H224_Handler * m_handler) const
{
    return endpoint.OnCreateH224Handler(dir,*this,id,m_handler);
}

H224_Handler * H323Connection::CreateH224Handler(H323Channel::Directions dir, OpalH224Handler & h224Handler, const PString & id)
{
    if (id == "H281")  // Backwards compatibility
        return CreateH281ProtocolHandler(h224Handler);
    else
        return NULL;
}

#ifdef H224_H281
H224_H281Handler * H323Connection::CreateH281ProtocolHandler(OpalH224Handler & h224Handler)
{
    return endpoint.CreateH281ProtocolHandler(h224Handler);
}
#endif

#endif  // H323_H224


#ifdef H323_T140
H323_RFC4103Handler * H323Connection::CreateRFC4103ProtocolHandler(H323Channel::Directions dir, unsigned sessionID)
{
  return endpoint.CreateRFC4103ProtocolHandler(dir, *this, sessionID);
}
#endif  // H323_T140


#ifdef H323_FILE
PBoolean H323Connection::OpenFileTransferSession(const H323FileTransferList & list, H323ChannelNumber & num)
{
  PBoolean filetransferOpen = FALSE;

  for (PINDEX i = 0; i < localCapabilities.GetSize(); i++) {
    H323Capability & localCapability = localCapabilities[i];
    if ((localCapability.GetMainType() == H323Capability::e_Data) &&
        (localCapability.GetSubType() == H245_DataApplicationCapability_application::e_genericDataCapability)) {
      H323FileTransferCapability * remoteCapability = (H323FileTransferCapability *)remoteCapabilities.FindCapability(localCapability);
      if (remoteCapability != NULL) {
        PTRACE(3, "H323\tFile Transfer Available " << *remoteCapability);
        remoteCapability->SetFileTransferList(list);
        if (logicalChannels->Open(*remoteCapability, OpalMediaFormat::DefaultFileSessionID,num)) {
           filetransferOpen = TRUE;
           break;
        }
        PTRACE(2, "H323\tFileTranfer OpenLogicalChannel failed: " << *remoteCapability);
      }
      break;
    }
  }

  return filetransferOpen;
}

PBoolean H323Connection::CloseFileTransferSession(unsigned num)
{
    CloseLogicalChannel(num,false);
    return TRUE;
}

H323FileTransferHandler * H323Connection::CreateFileTransferHandler(unsigned sessionID,
                                                                    H323Channel::Directions dir,
                                                                    H323FileTransferList & filelist)
{

  if (!filelist.IsMaster() && !OpenFileTransferChannel(dir == H323Channel::IsTransmitter, filelist))
        return NULL;

  return OnCreateFileTransferHandler(sessionID,dir,filelist);
}

H323FileTransferHandler * H323Connection::OnCreateFileTransferHandler(unsigned sessionID,
                                                                    H323Channel::Directions dir,
                                                                    H323FileTransferList & filelist)
{
    return new H323FileTransferHandler(*this, sessionID, dir, filelist);
}


PBoolean H323Connection::OpenFileTransferChannel( PBoolean isEncoder,
                                              H323FileTransferList & filelist
                                             )
{
   return endpoint.OpenFileTransferChannel(*this,isEncoder,filelist);
}


#endif


PBoolean H323Connection::GetAdmissionRequestAuthentication(const H225_AdmissionRequest & /*arq*/,
                                                       H235Authenticators & /*authenticators*/)
{
  return FALSE;
}


const H323Transport & H323Connection::GetControlChannel() const
{
  return *(controlChannel != NULL ? controlChannel : signallingChannel);
}


void H323Connection::SetAudioJitterDelay(unsigned minDelay, unsigned maxDelay)
{
  PAssert(minDelay <= 1000 && maxDelay <= 1000, PInvalidParameter);

  if (minDelay < 10)
    minDelay = 10;
  minAudioJitterDelay = minDelay;

  if (maxDelay < minDelay)
    maxDelay = minDelay;
  maxAudioJitterDelay = maxDelay;
}


void H323Connection::SendLogicalChannelMiscCommand(H323Channel & channel,
                                                   unsigned commandIdentifier)
{
  if (channel.GetDirection() == H323Channel::IsReceiver) {
    H323ControlPDU pdu;
    H245_CommandMessage & command = pdu.Build(H245_CommandMessage::e_miscellaneousCommand);
    H245_MiscellaneousCommand & miscCommand = command;
    miscCommand.m_logicalChannelNumber = (unsigned)channel.GetNumber();
    miscCommand.m_type.SetTag(commandIdentifier);
    WriteControlPDU(pdu);
  }
}


void H323Connection::SetEnforcedDurationLimit(unsigned seconds)
{
  enforcedDurationLimit.SetInterval(0, seconds);
}


void H323Connection::MonitorCallStatus()
{
  if (!Lock())
    return;

  if (roundTripDelayRate > 0 && !roundTripDelayTimer.IsRunning()) {
    roundTripDelayTimer.SetInterval(roundTripDelayRate);
    StartRoundTripDelay();
  }

  if (noMediaTimeOut > 0) {
    PBoolean oneRunning = FALSE;
    PBoolean allSilent = TRUE;
    for (PINDEX i = 0; i < logicalChannels->GetSize(); i++) {
        H323Channel * channel = logicalChannels->GetChannelAt(i);
        if (channel && channel->IsRunning()) {
            oneRunning = TRUE;
            if (channel->GetSilenceDuration() < noMediaTimeOut) {
                allSilent = FALSE;
                break;
            }
        }
    }
    if (oneRunning && allSilent)
      ClearCall(EndedByTransportFail);
  }

  if (enforcedDurationLimit.GetResetTime() > 0 && enforcedDurationLimit == 0)
    ClearCall(EndedByDurationLimit);

  Unlock();
}

#ifdef P_STUN

H323Connection::SessionInformation::SessionInformation(const OpalGloballyUniqueID & id, unsigned crv, const PString & token, unsigned session, const H323Connection * conn)
: m_callID(id), m_crv(crv), m_callToken(token), m_sessionID(session), m_recvMultiID(0), m_sendMultiID(0), m_connection(conn)
{

#ifdef H323_H46019M
    if (conn->IsH46019Multiplexed())
        m_recvMultiID = conn->GetEndPoint().GetMultiplexID();
#endif

#ifdef H323_H46024A
    // Some random number bases on the session id (for H.460.24A)
    int rand = PRandom::Number((session *100),((session+1)*100)-1);
    m_CUI = PString(rand);
    PTRACE(4,"H46024A\tGenerated CUI s: " << session << " value: " << m_CUI);
#else
    m_CUI = PString();
#endif
}

unsigned H323Connection::SessionInformation::GetCallReference()
{
    return m_crv;
}

const PString & H323Connection::SessionInformation::GetCallToken()
{
    return m_callToken;
}

unsigned H323Connection::SessionInformation::GetSessionID() const
{
    return m_sessionID;
}

void H323Connection::SessionInformation::SetSendMultiplexID(unsigned id)
{
    m_sendMultiID = id;
}

unsigned H323Connection::SessionInformation::GetRecvMultiplexID() const
{
    return m_recvMultiID;
}

H323Connection::SessionInformation * H323Connection::BuildSessionInformation(unsigned sessionID) const
{
    return new SessionInformation(GetCallIdentifier(),GetCallReference(),GetCallToken(),sessionID, this);
}

const OpalGloballyUniqueID & H323Connection::SessionInformation::GetCallIdentifer()
{
    return m_callID;
}

const PString & H323Connection::SessionInformation::GetCUI()
{
    return m_CUI;
}

const H323Connection * H323Connection::SessionInformation::GetConnection()
{
    return m_connection;
}

#endif

#ifdef H323_H460
void H323Connection::DisableFeatures(PBoolean disable)
{
     disableH460 = disable;
}

#ifdef H323_H46018
void H323Connection::H46019SetCallReceiver()
{
    PTRACE(4,"H46019\tCall is receiver.");
    m_H46019CallReceiver = true;
}

void H323Connection::H46019Enabled()
{
    if (!m_H46019offload)
       m_H46019enabled = true;
}

PBoolean H323Connection::IsH46019Enabled() const
{
    return m_H46019enabled;
}

void H323Connection::H46019SetOffload()
{
    if (!m_H46019multiplex)
         m_H46019enabled = false;

    m_H46019offload = true;
}

void H323Connection::H46019MultiEnabled()
{
    m_H46019enabled = true;
    m_H46019multiplex = true;
    NATsupport = true;
}

PBoolean H323Connection::IsH46019Multiplexed() const
{
    return m_H46019multiplex;
}
#endif   // H323_H46018

#ifdef H323_H46024A
void H323Connection::H46024AEnabled()
{
    m_H46024Aenabled = true;
}
#endif   // H323_H46024A

#ifdef H323_H46024B
void H323Connection::H46024BEnabled()
{
    m_H46024Benabled = true;
}
#endif   // H323_H46024A


#ifdef H323_H46026
void H323Connection::H46026SetMediaTunneled()
{
    m_H46026enabled = true;
#ifdef H323_H46018
    if (features->HasFeature(19)) {
        H460_Feature * feat = features->GetFeature(19);
        if (feat) {
            PTRACE(4,"H46026\tDisabling H.460.19 support for call");
            ((H460_FeatureStd19 *)feat)->SetAvailable(false);
        }
    }
    m_H46019enabled = false;
#endif // H323_H46018
}

PBoolean H323Connection::H46026IsMediaTunneled()
{
    return m_H46026enabled;
}
#endif   // H323_H46026

#ifdef H323_H460COM
void H323Connection::OnRemoteVendorInformation(const PString & product, const PString & version)
{

}
#endif // H323_H460COM

#ifdef H323_H461
// Normally first message will be a listRequest. ie (4)
H323Connection::H461MessageInfo::H461MessageInfo()
: m_message(4), m_assocToken(PString()), m_callToken(PString()),
  m_applicationID(-1), m_invokeToken(PString()), m_aliasAddress(PString()), m_approved(false)
{

}

void H323Connection::SetH461Mode(ASSETCallType mode)
{
    m_H461Mode = mode;
}

H323Connection::ASSETCallType H323Connection::GetH461Mode() const
{
    return m_H461Mode;
}

void H323Connection::SetH461MessageInfo(int type, const PString & assocCallToken, const PString & assocCallIdentifier, int applicationID,
                                        const PString & invokeToken, const PString & aliasAddress, bool approved )
{
    m_H461Info.m_message = type;
    m_H461Info.m_assocToken = assocCallToken;
    m_H461Info.m_callToken = assocCallIdentifier;
    m_H461Info.m_applicationID = applicationID;
    m_H461Info.m_invokeToken = invokeToken;
    m_H461Info.m_aliasAddress = aliasAddress;
    m_H461Info.m_approved = approved;
}

H323Connection::H461MessageInfo & H323Connection::GetH461MessageInfo()
{
    return m_H461Info;
}
#endif // H323_H461

#endif   // H323_H461

PBoolean H323Connection::OnH245AddressConflict()
{
    // if the other side requests us to start a H.245 channel, we always do that instead of insisting that we already opened a listener
    return true;
}


PBoolean H323Connection::OnSendFeatureSet(unsigned code, H225_FeatureSet & feats, PBoolean advertise) const
{
#ifdef H323_H460
   if (disableH460)
       return FALSE;

   return features->SendFeature(code, feats, advertise);
#else
   return endpoint.OnSendFeatureSet(code, feats, advertise);
#endif
}

void H323Connection::OnReceiveFeatureSet(unsigned code, const H225_FeatureSet & feats, PBoolean genericData) const
{
#ifdef H323_H460
   if (disableH460)
       return;

   features->ReceiveFeature(code, feats, genericData);
#else
   endpoint.OnReceiveFeatureSet(code, feats, genericData);
#endif
}

#ifdef H323_H460
void H323Connection::DisableFeatureSet(int msgtype) const
{
    features->DisableAllFeatures(msgtype);
}
#endif

void H323Connection::SetAuthenticationConnection()
{
     for (PINDEX i = 0; i < EPAuthenticators.GetSize(); i++) {
        EPAuthenticators[i].SetConnection(this);
     }
}

const H235Authenticators & H323Connection::GetEPAuthenticators() const
{
      return EPAuthenticators;
}

PBoolean H323Connection::OnCallAuthentication(const PString & username,
                                         PString & password)
{
    return endpoint.OnCallAuthentication(username,password);
}

PBoolean H323Connection::OnEPAuthenticationFailed(H235Authenticator::ValidationResult result) const
{
    return FALSE;
}

void H323Connection::OnAuthenticationFinalise(unsigned pdu,PBYTEArray & rawData)
{
     for (PINDEX i = 0; i < EPAuthenticators.GetSize(); i++) {
       if (EPAuthenticators[i].IsSecuredSignalPDU(pdu,FALSE))
           EPAuthenticators[i].Finalise(rawData);
     }
}

#ifdef H323_H235
void H323Connection::OnMediaEncryption(unsigned session, H323Channel::Directions dir, const PString & cipher)
{
    endpoint.OnMediaEncryption(session, dir, cipher);
}
#endif

void H323Connection::SetTransportSecurity(const H323TransportSecurity & m_callSecurity)
{
    m_transportSecurity = m_callSecurity;
}

const H323TransportSecurity & H323Connection::GetTransportSecurity() const
{
    return m_transportSecurity;
}

#ifdef H323_SIGNAL_AGGREGATE

void H323Connection::AggregateSignalChannel(H323Transport * transport)
{
  signalAggregator = new AggregatedH225Handle(*transport, *this);
  endpoint.GetSignallingAggregator()->AddHandle(signalAggregator);
}

void H323Connection::AggregateControlChannel(H323Transport * transport)
{
  if (!OnStartHandleControlChannel())
    return;

  controlAggregator = new AggregatedH245Handle(*transport, *this);
  endpoint.GetSignallingAggregator()->AddHandle(controlAggregator);
}

H323AggregatedH2x5Handle::H323AggregatedH2x5Handle(H323Transport & _transport, H323Connection & _connection)
  : PAggregatedHandle(TRUE),
    fd(((PIPSocket *)_transport.GetBaseReadChannel())->GetHandle()),
    transport(_transport),
    connection(_connection)
{
  pduDataLen = 0;
}

H323AggregatedH2x5Handle::~H323AggregatedH2x5Handle()
{
}

PAggregatorFDList_t H323AggregatedH2x5Handle::GetFDs()
{
  PAggregatorFDList_t list;
  list.push_back(&fd);
  return list;
}

PBoolean H323AggregatedH2x5Handle::OnRead()
{
  //
  // pduDataLen  : size of PDU data read so far, always less than pduBufferLen , always same as pduBuffer.GetSize()
  //

  // if the transport is not open, then there is no need process it further
  if (!transport.IsOpen())
    return FALSE;

  // make sure have a minimum headroom in the PDU buffer
  PINDEX pduSize = pduBuffer.GetSize();
  if ((pduSize - pduDataLen) < 100) {
    pduSize += 1000;
    pduBuffer.SetSize(pduSize);
  }

  // save the timeout
  PTimeInterval oldTimeout = transport.GetReadTimeout();
  transport.SetReadTimeout(0);

  // read PDU until no more data is available
  PINDEX numRead = 0;
  PBoolean ok = TRUE;
  {
    char * ptr = (char *)(pduBuffer.GetPointer() + pduDataLen);
    int lenLeftInBuffer = pduSize - pduDataLen;
    while (ok && (lenLeftInBuffer > numRead) && transport.Read(ptr + numRead, lenLeftInBuffer - numRead)) {
      ok = transport.GetLastReadCount() > 0;
      if (ok)
        numRead += transport.GetLastReadCount();
    }
    if (pduBuffer[0] != 0x03) {
      PTRACE(1, "Error");
      ok = FALSE;
    }
  }

  // reset timeout
  transport.SetReadTimeout(oldTimeout);

  // if data was actually received, then process it
  if (ok) {

    // if PDU length was zero, must be timeout
    if (numRead == 0) {
      PBYTEArray dataPDU;
      ok = HandlePDU(FALSE, dataPDU);
    }

    else {

      // update pdu size for new data that was read
      pduDataLen += numRead;

      while (pduDataLen > 0) {

        // convert data to PDU. If PDU is invalid, return error
        PINDEX pduLen = pduDataLen;
        if (!transport.ExtractPDU(pduBuffer, pduLen)) {
          ok = FALSE;
          break;
        }

        // if PDU is not yet complete, then no error but stop looping
        else if (pduLen <= 0) {
          ok = TRUE;
          break;
        }

        // otherwise process the data
        else {
          transport.SetErrorValues(PChannel::NoError, 0, PChannel::LastReadError);
          {
            // create the new PDU
            PBYTEArray dataPDU((const BYTE *)pduBuffer+4, pduLen-4, FALSE);
            ok = HandlePDU(ok, dataPDU);
          }

          // remove processed data
          if (pduLen == pduDataLen)
            pduDataLen = 0;
          else {
            pduDataLen -= pduLen;
            memcpy(pduBuffer.GetPointer(), pduBuffer.GetPointer() + pduLen, pduDataLen);
          }
        }
      }
    }
  }


  return ok;
}

#endif

#ifdef H323_H248

PBoolean H323Connection::OnSendServiceControlSessions(
                   H225_ArrayOf_ServiceControlSession & serviceControl,
                   H225_ServiceControlSession_reason reason) const
{

    PString amount;
    PBoolean credit=TRUE;
    unsigned time=0;
    PString url;

    if (!OnSendServiceControl(amount, credit,time, url) &&
                        (serviceControlSessions.GetSize() == 0))
        return FALSE;

    H323Dictionary<POrdinalKey, H323ServiceControlSession> SCS = serviceControlSessions;

    if (!amount) {
        H323CallCreditServiceControl * csc =
                 new H323CallCreditServiceControl(amount,credit,time);
        SCS.SetAt(H323ServiceControlSession::e_CallCredit, csc);
    }

    if (!url) {
        H323HTTPServiceControl * scs = new H323HTTPServiceControl(url);
        SCS.SetAt(H323ServiceControlSession::e_URL, scs);
    }

    for (PINDEX j = 0; j < SCS.GetSize(); j++) {

      PINDEX last = serviceControl.GetSize();
      serviceControl.SetSize(last+1);
      H225_ServiceControlSession & pdu = serviceControl[last];

      unsigned type = ((H323ServiceControlSession *)SCS.GetAt(j))->GetType();
      pdu.m_sessionId = type;
      pdu.m_reason = reason;

      if (SCS[type].OnSendingPDU(pdu.m_contents))
        pdu.IncludeOptionalField(H225_ServiceControlSession::e_contents);

    }

   return TRUE;
}

void H323Connection::OnReceiveServiceControlSessions(const H225_ArrayOf_ServiceControlSession & serviceControl)
{

  PBoolean isContent=FALSE;

  for (PINDEX i = 0; i < serviceControl.GetSize(); i++) {
    H225_ServiceControlSession & pdu = serviceControl[i];

    H323ServiceControlSession * session = NULL;
    unsigned sessionId = pdu.m_sessionId;

    if (serviceControlSessions.Contains(sessionId)) {
      session = &serviceControlSessions[sessionId];
      if (pdu.HasOptionalField(H225_ServiceControlSession::e_contents)) {
          if (session->OnReceivedPDU(pdu.m_contents))
              isContent = TRUE;
      }
    }

    if (session == NULL && pdu.HasOptionalField(H225_ServiceControlSession::e_contents)) {
      session = endpoint.CreateServiceControlSession(pdu.m_contents);
      serviceControlSessions.SetAt(sessionId, session);
    }
  }

  if (isContent) {
    PString amount;
    PBoolean credit=TRUE;
    unsigned time;
    PString url;
    PString ldapURL;
    PString baseDN;

    for (PINDEX j = 0; j < serviceControlSessions.GetSize(); j++) {
      H323ServiceControlSession & sess = serviceControlSessions[j];
      switch (sess.GetType()) {
         case H323ServiceControlSession::e_CallCredit:
            ((H323CallCreditServiceControl &)sess).GetValue(amount,credit,time);
                 break;
         case H323ServiceControlSession::e_URL:
            ((H323HTTPServiceControl &)sess).GetValue(url);
                break;
#ifdef H323_H350
         case H323ServiceControlSession::e_NonStandard:
            ((H323H350ServiceControl &)sess).GetValue(ldapURL,baseDN);
                break;
#endif
         default:
                break;
      }
    }
    OnReceiveServiceControl(amount,credit,time,url,ldapURL,baseDN);
  }
}

void H323Connection::OnReceiveServiceControl(const PString & amount,
                                             PBoolean credit,
                                             const unsigned & timelimit,
                                             const PString & url,
                                             const PString & ldapURL,
                                             const PString & baseDN
                                            )
{
    if (!amount)
        endpoint.OnCallCreditServiceControl(amount,credit,timelimit);

    if (!url)
        endpoint.OnHTTPServiceControl(0, 0, url);

#ifdef H323_H350
    if (!ldapURL)
        endpoint.OnH350ServiceControl(ldapURL,baseDN);
#endif
}

PBoolean H323Connection::OnSendServiceControl(PString & /*amount*/,
                                          PBoolean /*credit*/,
                                          unsigned & /*timelimit*/,
                                          PString & /*url*/
                                         ) const
{
    return FALSE;
}

#endif

void H323Connection::DisableH245inSETUP()
{
    doH245inSETUP = FALSE;
}

void H323Connection::DisableH245QoS()
{
    doH245QoS = FALSE;
}

PBoolean H323Connection::H245QoSEnabled() const
{
    return doH245QoS;
}

void H323Connection::SetNonCallConnection()
{
    nonCallConnection = TRUE;
}

PBoolean H323Connection::IsNonCallConnection() const
{
    return nonCallConnection;
}

#ifdef H323_H460
H460_FeatureSet * H323Connection::GetFeatureSet()
{
    return features;
}

PBoolean H323Connection::FeatureSetSupportNonCallService(const H225_FeatureSet & fs) const
{
    return (features && features->SupportNonCallService(fs));
}
#endif

#ifdef H323_VIDEO
#ifdef H323_H239
PBoolean H323Connection::AcceptH239ControlRequest(PBoolean & delay)
{
    delay = false;
    return true;
}

PBoolean H323Connection::OnH239ControlRequest(H239Control * ctrl)
{
    if (!ctrl)
        return false;

    PBoolean delay = false;
    if (AcceptH239ControlRequest(delay)) {
      if (delay) return true;
      return ctrl->SendGenericMessage(H239Control::e_h245response, this, true);
    }  else
      return ctrl->SendGenericMessage(H239Control::e_h245response, this, false);
}

PBoolean H323Connection::OnH239ControlCommand(H239Control * ctrl)
{
    return true;
}

PBoolean H323Connection::OpenH239Channel()
{
   if (callToken.IsEmpty()) {
      PTRACE(2,"H239\tERROR Open Channel. Not in a call");
      return false;
   }

   H239Control * ctrl = (H239Control *)remoteCapabilities.FindCapability("H.239 Control");
   if (ctrl)
      return ctrl->SendGenericMessage(H239Control::e_h245request, this);

   PTRACE(2,"H239\tERROR Open Channel. No Remote Support");
   return false;
}

PBoolean H323Connection::CloseH239Channel(H323Capability::CapabilityDirection dir)
{
  H239Control * ctrl = (H239Control *)remoteCapabilities.FindCapability("H.239 Control");
  if (ctrl)
     return ctrl->CloseChannel(this, dir);

  return false;
}

void H323Connection::OnH239SessionStarted(int sessionNum, H323Capability::CapabilityDirection dir)
{
  if (!sessionNum)
      return;

  H239Control * ctrl = (H239Control *)remoteCapabilities.FindCapability("H.239 Control");
  if (ctrl)
     return ctrl->SetChannelNum(sessionNum,dir);
}

void H323Connection::OnH239SessionEnded(int sessionNum, H323Capability::CapabilityDirection dir)
{
  if (!sessionNum)
      return;

  H239Control * ctrl = (H239Control *)remoteCapabilities.FindCapability("H.239 Control");
  if (ctrl)
     return ctrl->SetChannelNum(0,dir);
}

void H323Connection::OpenExtendedVideoSessionDenied()
{
    PTRACE(2,"H239\tOpen Request denied from remote");
}

PBoolean H323Connection::OpenExtendedVideoSession(H323ChannelNumber & num, int defaultSession)
{
  PBoolean applicationOpen = false;
  // First need to check if extended video channel is opened
  if (logicalChannels->FindChannelBySession(OpalMediaFormat::DefaultExtVideoSessionID, false) ||
      logicalChannels->FindChannelBySession(defaultSession, false)) {// is opened
    PTRACE(3, "Extended video channel is opened, no need open it");
    return true;
  }

  // extended video channel is not opened, try to open it
  for (PINDEX i = 0; i < localCapabilities.GetSize(); i++) {
    H323Capability & localCapability = localCapabilities[i];
    if ((localCapability.GetMainType() == H323Capability::e_Video) &&
        (localCapability.GetSubType() == H245_VideoCapability::e_extendedVideoCapability)) {
      H323ExtendedVideoCapability * remoteCapability = (H323ExtendedVideoCapability *)remoteCapabilities.FindCapability(localCapability);
      if (remoteCapability != NULL) {
        PTRACE(3, "H323\tApplication Available " << *remoteCapability);

#ifdef H323_H235
        H323SecureExtendedCapability* secureCap = dynamic_cast<H323SecureExtendedCapability*>(remoteCapability);
        if (secureCap) { // this is case of H.235
          if (logicalChannels->Open(*secureCap, defaultSession, num)) {
             applicationOpen = TRUE;
             break;
          } else {
             PTRACE(2, "H323\tApplication OpenLogicalChannel failed: " << *secureCap);
          }
        } else
#endif
        { // this is case of normal
          for (PINDEX j = 0; j < remoteCapability->GetSize(); j++) {
            if (logicalChannels->Open(remoteCapability[j], defaultSession, num)) {
               applicationOpen = TRUE;
               break;
            } else {
               PTRACE(2, "H323\tApplication OpenLogicalChannel failed: " << *remoteCapability);
            }
          }
        }
        if (applicationOpen)
            break;
      }
    }
  }
  return applicationOpen;
}

PBoolean H323Connection::CloseExtendedVideoSession(const H323ChannelNumber & num)
{
    CloseLogicalChannel(num, num.IsFromRemote());
    return TRUE;
}

PBoolean H323Connection::OpenExtendedVideoChannel(PBoolean isEncoding,H323VideoCodec & codec)
{
   return endpoint.OpenExtendedVideoChannel(*this, isEncoding, codec);
}

unsigned H323Connection::GetExtVideoRTPSessionID() const
{
  if (IsH245Master()) {
    return 32;  // TODO: make a constant ?
  } else {
    return h239SessionID;
  }
}

#endif  // H323_H239
#endif  // NO_H323_VIDEO

#ifdef H323_H230
PBoolean H323Connection::OpenConferenceControlSession(PBoolean & chairControl, PBoolean & extControls)
{
  chairControl = FALSE;
  extControls = FALSE;
  for (PINDEX i = 0; i < localCapabilities.GetSize(); i++) {
    H323Capability & localCapability = localCapabilities[i];
    if (localCapability.GetMainType() == H323Capability::e_ConferenceControl) {
      H323_ConferenceControlCapability * remoteCapability = (H323_ConferenceControlCapability *)remoteCapabilities.FindCapability(localCapability);
      if (remoteCapability != NULL) {
          chairControl = remoteCapability->SupportChairControls();
          extControls = remoteCapability->SupportExtControls();
          PTRACE(3, "H323\tConference Controls Available for " << GetCallToken() << " Chair " << chairControl << " T124 " << extControls);
          return TRUE;
      }
    }
  }
  PTRACE(4, "H323\tConference Controls not available for " << GetCallToken());
  return FALSE;
}

#endif  // H323_H230

#ifdef H323_FRAMEBUFFER
void H323Connection::EnableVideoFrameBuffer(PBoolean enable)
{
    endpoint.EnableVideoFrameBuffer(enable);
}

PBoolean H323Connection::HasVideoFrameBuffer()
{
    return endpoint.HasVideoFrameBuffer();
}
#endif

PBoolean H323Connection::IsMaintainedConnection() const
{
   return m_maintainConnection;
}

/////////////////////////////////////////////////////////////////////////////

