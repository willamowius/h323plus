/*
 * gkclient.cxx
 *
 * Gatekeeper client protocol handler
 *
 * Open H323 Library
 *
 * Copyright (c) 1999-2000 Equivalence Pty. Ltd.
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
 * iFace In, http://www.iface.com
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "gkclient.h"
#endif

#if defined(_WIN32) && (_MSC_VER > 1300)
  #pragma warning(disable:4244) // warning about possible loss of data
#endif

#include "gkclient.h"

#include "h323ep.h"
#include "h323rtp.h"
#include "h323pdu.h"

#ifdef H323_H460
#include "h460/h4601.h"
#endif

#define new PNEW


static PTimeInterval AdjustTimeout(unsigned seconds)
{
  // Allow for an incredible amount of system/network latency
  static unsigned TimeoutDeadband = 5; // seconds

  return PTimeInterval(0, seconds > TimeoutDeadband
                              ? (seconds - TimeoutDeadband)
                              : TimeoutDeadband);
}


/////////////////////////////////////////////////////////////////////////////

H323Gatekeeper::H323Gatekeeper(H323EndPoint & ep, H323Transport * trans)
  : H225_RAS(ep, trans),
    requestMutex(1, 1),
    authenticators(ep.CreateAuthenticators())
#ifdef H323_H460
    ,features(ep.GetFeatureSet())
#endif
{
  alternatePermanent = FALSE;
  discoveryComplete = FALSE;
  moveAlternate = FALSE;
  registrationFailReason = UnregisteredLocally;

  pregrantMakeCall = pregrantAnswerCall = RequireARQ;

  autoReregister = TRUE;
  reregisterNow = FALSE;
  requiresDiscovery = FALSE;

  timeToLive.SetNotifier(PCREATE_NOTIFIER(TickleMonitor));
  infoRequestRate.SetNotifier(PCREATE_NOTIFIER(TickleMonitor));

  willRespondToIRR = FALSE;
  monitorStop = FALSE;

  monitor = PThread::Create(PCREATE_NOTIFIER(MonitorMain), 0,
                            PThread::NoAutoDeleteThread,
                            PThread::NormalPriority,
                            "GkMonitor:%x");
#ifdef H323_H460
  features->AttachEndPoint(&ep);
  features->LoadFeatureSet(H460_Feature::FeatureRas);
#endif

  localId = PString();
}


H323Gatekeeper::~H323Gatekeeper()
{
  if (monitor != NULL) {
    monitorStop = TRUE;
    monitorTickle.Signal();
    monitor->WaitForTermination();
    delete monitor;
  }
#ifdef H323_H460
  delete features;
#endif

  StopChannel();
}


PString H323Gatekeeper::GetName() const
{
  PStringStream s;
  s << *this;
  return s;
}


PBoolean H323Gatekeeper::DiscoverAny()
{
  gatekeeperIdentifier = PString();
  return StartDiscovery(H323TransportAddress());
}


PBoolean H323Gatekeeper::DiscoverByName(const PString & identifier)
{
  gatekeeperIdentifier = identifier;
  return StartDiscovery(H323TransportAddress());
}


PBoolean H323Gatekeeper::DiscoverByAddress(const H323TransportAddress & address)
{
  gatekeeperIdentifier = PString();
  return StartDiscovery(address);
}


PBoolean H323Gatekeeper::DiscoverByNameAndAddress(const PString & identifier,
                                              const H323TransportAddress & address)
{
  gatekeeperIdentifier = identifier;
  return StartDiscovery(address);
}

PBoolean H323Gatekeeper::StartDiscovery(const H323TransportAddress & initialAddress)
{
  if (PAssertNULL(transport) == NULL)
    return FALSE;

  /// don't send GRQ if not requested
  if (!endpoint.GetSendGRQ() && !initialAddress.IsEmpty()) {
    transport->SetRemoteAddress(initialAddress);
    if (!transport->Connect()) {
      PTRACE(2, "RAS\tUnable to connect to gatekeeper at " << initialAddress);
      return FALSE;
    }
    transport->SetPromiscuous(H323Transport::AcceptFromRemoteOnly);
    StartChannel();
    PTRACE(2, "RAS\tSkipping gatekeeper discovery for " << initialAddress);
    return TRUE;
  }

  H323RasPDU pdu;
  Request request(SetupGatekeeperRequest(pdu), pdu);

  H323TransportAddress address = initialAddress;
  request.responseInfo = &address;

  requestsMutex.Wait();
  requests.SetAt(request.sequenceNumber, &request);
  requestsMutex.Signal();

  discoveryComplete = FALSE;
  unsigned retries = endpoint.GetGatekeeperRequestRetries();
  while (!discoveryComplete) {
    if (transport->DiscoverGatekeeper(*this, pdu, address)) {
      if (address == initialAddress)
        break;
      /// If get here must have been GRJ with an alternate gk, start again
    }
    else {
      // Transport failure, retry
      if (--retries == 0)
        break;
    }
  }

  requestsMutex.Wait();
  requests.SetAt(request.sequenceNumber, NULL);
  requestsMutex.Signal();

  if (discoveryComplete) {
    if (transport->Connect())
      StartChannel();
  }

  return discoveryComplete;
}


unsigned H323Gatekeeper::SetupGatekeeperRequest(H323RasPDU & request)
{
  if (PAssertNULL(transport) == NULL)
    return 0;

  H225_GatekeeperRequest & grq = request.BuildGatekeeperRequest(GetNextSequenceNumber());

  endpoint.SetEndpointTypeInfo(grq.m_endpointType);
  transport->SetUpTransportPDU(grq.m_rasAddress, TRUE);

  grq.IncludeOptionalField(H225_GatekeeperRequest::e_endpointAlias);
  H323SetAliasAddresses(endpoint.GetAliasNames(), grq.m_endpointAlias);

  if (!gatekeeperIdentifier) {
    grq.IncludeOptionalField(H225_GatekeeperRequest::e_gatekeeperIdentifier);
    grq.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

  grq.IncludeOptionalField(H225_GatekeeperRequest::e_supportsAltGK);

  grq.IncludeOptionalField(H225_GatekeeperRequest::e_supportsAssignedGK);
  grq.m_supportsAssignedGK = TRUE;

  OnSendGatekeeperRequest(grq);

  discoveryComplete = FALSE;

  return grq.m_requestSeqNum;
}


void H323Gatekeeper::OnSendGatekeeperRequest(H225_GatekeeperRequest & grq)
{
  H225_RAS::OnSendGatekeeperRequest(grq);

  for (PINDEX i = 0; i < authenticators.GetSize(); i++) {
    if (authenticators[i].SetCapability(grq.m_authenticationCapability, grq.m_algorithmOIDs)) {
      grq.IncludeOptionalField(H225_GatekeeperRequest::e_authenticationCapability);
      grq.IncludeOptionalField(H225_GatekeeperRequest::e_algorithmOIDs);
    }
  }
}


PBoolean H323Gatekeeper::OnReceiveGatekeeperConfirm(const H225_GatekeeperConfirm & gcf)
{
  if (!H225_RAS::OnReceiveGatekeeperConfirm(gcf))
    return FALSE;

  PINDEX i;

  for (i = 0; i < authenticators.GetSize(); i++) {
    H235Authenticator & authenticator = authenticators[i];
    if (authenticator.UseGkAndEpIdentifiers())
      authenticator.SetRemoteId(gatekeeperIdentifier);
  }

  if (gcf.HasOptionalField(H225_GatekeeperConfirm::e_authenticationMode) &&
      gcf.HasOptionalField(H225_GatekeeperConfirm::e_algorithmOID)) {
    for (i = 0; i < authenticators.GetSize(); i++) {
      H235Authenticator & authenticator = authenticators[i];
      authenticator.Enable(authenticator.IsCapability(gcf.m_authenticationMode,
                                                      gcf.m_algorithmOID));
      PTRACE(4,"RAS\tAuthenticator " << authenticator.GetName() 
                      << (authenticator.IsActive() ? " ACTIVATED" : " disabled"));
    }
  }

  H323TransportAddress locatedAddress = gcf.m_rasAddress;
  PTRACE(2, "RAS\tGatekeeper discovery found " << locatedAddress);

  if (!transport->SetRemoteAddress(locatedAddress)) {
    PTRACE(2, "RAS\tInvalid gatekeeper discovery address: \"" << locatedAddress << '"');
    return FALSE;
  }

  if (gcf.HasOptionalField(H225_GatekeeperConfirm::e_alternateGatekeeper))
    SetAlternates(gcf.m_alternateGatekeeper, FALSE);

  if (gcf.HasOptionalField(H225_GatekeeperConfirm::e_assignedGatekeeper)) {
    SetAssignedGatekeeper(gcf.m_assignedGatekeeper);
    PTRACE(2, "RAS\tAssigned Gatekeeper redirected " << assignedGK);
    // This will force the gatekeeper to register to the assigned Gatekeeper.
    if (lastRequest->responseInfo != NULL) {
      H323TransportAddress & gkAddress = *(H323TransportAddress *)lastRequest->responseInfo;
      gkAddress = assignedGK.rasAddress;
      gatekeeperIdentifier = PString();
    }
  } else {
    endpoint.OnGatekeeperConfirm();
    discoveryComplete = TRUE;
  }
  return TRUE;
}


PBoolean H323Gatekeeper::OnReceiveGatekeeperReject(const H225_GatekeeperReject & grj)
{
  if (!H225_RAS::OnReceiveGatekeeperReject(grj))
    return FALSE;

  if (grj.HasOptionalField(H225_GatekeeperReject::e_altGKInfo)) {
    SetAlternates(grj.m_altGKInfo.m_alternateGatekeeper,
                  grj.m_altGKInfo.m_altGKisPermanent);
  }

  if ((alternates.GetSize() > 0) && (lastRequest->responseInfo != NULL)) {
      H323TransportAddress & gkAddress = *(H323TransportAddress *)lastRequest->responseInfo;
      gkAddress = alternates[0].rasAddress;
  }

  endpoint.OnGatekeeperReject();

  return TRUE;
}


PBoolean H323Gatekeeper::RegistrationRequest(PBoolean autoReg)
{
  PWaitAndSignal m(RegisterMutex);

  if (PAssertNULL(transport) == NULL)
    return FALSE;

  autoReregister = autoReg;

  H323RasPDU pdu;
  H225_RegistrationRequest & rrq = pdu.BuildRegistrationRequest(GetNextSequenceNumber());

  // If discoveryComplete flag is FALSE then do lightweight reregister
  rrq.m_discoveryComplete = discoveryComplete;

  // Check if the IP address might of changed since last registration (for DDNS Type registrations)
  H323TransportAddress newaddress;
  if ((!discoveryComplete) && (endpoint.GatekeeperCheckIP(transport->GetRemoteAddress(),newaddress)))
      transport->SetRemoteAddress(newaddress);

  if (transport->IsRASTunnelled()) {
      rrq.IncludeOptionalField(H225_RegistrationRequest::e_maintainConnection);
      rrq.m_maintainConnection = true;
  } else {
      rrq.m_rasAddress.SetSize(1);
      transport->SetUpTransportPDU(rrq.m_rasAddress[0], TRUE);

      H323TransportAddressArray listeners = endpoint.GetInterfaceAddresses(TRUE, transport);
      if (listeners.IsEmpty()) {
        PTRACE(1, "RAS\tCannot register with Gatekeeper without a H323Listener!");
        return FALSE;
      }

      H323SetTransportAddresses(*transport, listeners, rrq.m_callSignalAddress);
  } 

  endpoint.SetEndpointTypeInfo(rrq.m_terminalType);
  endpoint.SetVendorIdentifierInfo(rrq.m_endpointVendor);

  if (!IsRegistered()) {  // only send terminal aliases on full registration reset localId
    rrq.IncludeOptionalField(H225_RegistrationRequest::e_terminalAlias);
    H323SetAliasAddresses(endpoint.GetAliasNames(), rrq.m_terminalAlias);
        for (PINDEX i = 0; i < authenticators.GetSize(); i++) { 
            H235Authenticator & authenticator = authenticators[i];
            if (authenticator.UseGkAndEpIdentifiers())
                authenticator.SetLocalId(localId);
        }
  }

  rrq.m_willSupplyUUIEs = TRUE;
  rrq.IncludeOptionalField(H225_RegistrationRequest::e_usageReportingCapability);
  rrq.m_usageReportingCapability.IncludeOptionalField(H225_RasUsageInfoTypes::e_startTime);
  rrq.m_usageReportingCapability.IncludeOptionalField(H225_RasUsageInfoTypes::e_endTime);
  rrq.m_usageReportingCapability.IncludeOptionalField(H225_RasUsageInfoTypes::e_terminationCause);
  rrq.IncludeOptionalField(H225_RegistrationRequest::e_supportsAltGK);

  if (!gatekeeperIdentifier) {
    rrq.IncludeOptionalField(H225_RegistrationRequest::e_gatekeeperIdentifier);
    rrq.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

  if (!endpointIdentifier.GetValue().IsEmpty()) {
    rrq.IncludeOptionalField(H225_RegistrationRequest::e_endpointIdentifier);
    rrq.m_endpointIdentifier = endpointIdentifier;
  }

  PTimeInterval ttl = endpoint.GetGatekeeperTimeToLive();
  if (ttl > 0) {
    rrq.IncludeOptionalField(H225_RegistrationRequest::e_timeToLive);
    rrq.m_timeToLive = (int)ttl.GetSeconds();
  }

  if (endpoint.CanDisplayAmountString()) {
    rrq.IncludeOptionalField(H225_RegistrationRequest::e_callCreditCapability);
    rrq.m_callCreditCapability.IncludeOptionalField(H225_CallCreditCapability::e_canDisplayAmountString);
    rrq.m_callCreditCapability.m_canDisplayAmountString = TRUE;
  }

  if (endpoint.CanEnforceDurationLimit()) {
    rrq.IncludeOptionalField(H225_RegistrationRequest::e_callCreditCapability);
    rrq.m_callCreditCapability.IncludeOptionalField(H225_CallCreditCapability::e_canEnforceDurationLimit);
    rrq.m_callCreditCapability.m_canEnforceDurationLimit = TRUE;
  }

  if (assignedGK.IsValid()) {
      rrq.IncludeOptionalField(H225_RegistrationRequest::e_assignedGatekeeper);
      rrq.m_assignedGatekeeper = assignedGK.GetAlternate();
  }

  PStringList lang;
  if (endpoint.GetDefaultLanguages(lang)) {
      H323SetLanguages(lang,rrq.m_language);
      rrq.IncludeOptionalField(H225_RegistrationRequest::e_language);
  }

  if (IsRegistered()) {
    rrq.IncludeOptionalField(H225_RegistrationRequest::e_keepAlive);
    rrq.m_keepAlive = TRUE;
  }

  // After doing full register, do lightweight reregisters from now on
  discoveryComplete = FALSE;

  Request request(rrq.m_requestSeqNum, pdu);
  if (MakeRequest(request))
    return TRUE;

  PTRACE(3, "RAS\tFailed registration of " << endpointIdentifier << " with " << gatekeeperIdentifier);
  switch (request.responseResult) {
    case Request::RejectReceived :
      switch (request.rejectReason) {
        case H225_RegistrationRejectReason::e_discoveryRequired :
          // If have been told by GK that we need to discover it again, set flag
          // for next register done by timeToLive handler to do discovery
          requiresDiscovery = TRUE;
          // Do next case

        case H225_RegistrationRejectReason::e_fullRegistrationRequired :
          registrationFailReason = GatekeeperLostRegistration;
          // Set timer to retry registration
          reregisterNow = TRUE;
          monitorTickle.Signal();
          break;

        // Onse below here are permananent errors, so don't try again
        case H225_RegistrationRejectReason::e_invalidCallSignalAddress :
          registrationFailReason = InvalidListener;
          break;

        case H225_RegistrationRejectReason::e_duplicateAlias :
          registrationFailReason = DuplicateAlias;
          break;

        case H225_RegistrationRejectReason::e_securityDenial :
          registrationFailReason = SecurityDenied;
          break;

        case H225_RegistrationRejectReason::e_neededFeatureNotSupported :
          registrationFailReason = NeededFeatureNotSupported;
          break;

        default :
          registrationFailReason = (RegistrationFailReasons)(request.rejectReason|RegistrationRejectReasonMask);
          break;
      }
      break;

    case Request::BadCryptoTokens :
      registrationFailReason = SecurityDenied;
      break;

    default :
      registrationFailReason = TransportError;
      break;
  }

  return FALSE;
}


PBoolean H323Gatekeeper::OnReceiveRegistrationConfirm(const H225_RegistrationConfirm & rcf)
{
  if (!H225_RAS::OnReceiveRegistrationConfirm(rcf))
    return FALSE;

  registrationFailReason = RegistrationSuccessful;

  if (gatekeeperIdentifier.IsEmpty())
        gatekeeperIdentifier = rcf.m_gatekeeperIdentifier.GetValue();

  if (endpointIdentifier.GetValue().IsEmpty())
        endpointIdentifier = rcf.m_endpointIdentifier;

  PTRACE(3, "RAS\tRegistered " << endpointIdentifier.GetValue() << " with " << gatekeeperIdentifier);

  if (rcf.HasOptionalField(H225_RegistrationConfirm::e_assignedGatekeeper))
    SetAssignedGatekeeper(rcf.m_assignedGatekeeper);

  if (rcf.HasOptionalField(H225_RegistrationConfirm::e_alternateGatekeeper))
    SetAlternates(rcf.m_alternateGatekeeper, FALSE);

  if (rcf.HasOptionalField(H225_RegistrationConfirm::e_timeToLive))
    timeToLive = AdjustTimeout(rcf.m_timeToLive);
  else
    timeToLive = 0; // zero disables lightweight RRQ

  // At present only support first call signal address to GK
  if (rcf.m_callSignalAddress.GetSize() > 0)
    gkRouteAddress = rcf.m_callSignalAddress[0];

#if P_HAS_IPV6
  // Set to only resolve DNS addresses of the type supported by the signalling address of the gatekeeper. 
  if (PIPSocket::GetDefaultIpAddressFamily() == AF_INET6 && gkRouteAddress.GetIpVersion() != 6)
       PIPSocket::SetDefaultIpAddressFamilyV4(); 
#endif

  willRespondToIRR = rcf.m_willRespondToIRR;

  pregrantMakeCall = pregrantAnswerCall = RequireARQ;
  if (rcf.HasOptionalField(H225_RegistrationConfirm::e_preGrantedARQ)) {
    if (rcf.m_preGrantedARQ.m_makeCall)
      pregrantMakeCall = rcf.m_preGrantedARQ.m_useGKCallSignalAddressToMakeCall
                                                      ? PreGkRoutedARQ : PregrantARQ;
    if (rcf.m_preGrantedARQ.m_answerCall)
      pregrantAnswerCall = rcf.m_preGrantedARQ.m_useGKCallSignalAddressToAnswer
                                                      ? PreGkRoutedARQ : PregrantARQ;
    if (rcf.m_preGrantedARQ.HasOptionalField(H225_RegistrationConfirm_preGrantedARQ::e_irrFrequencyInCall))
      SetInfoRequestRate(AdjustTimeout(rcf.m_preGrantedARQ.m_irrFrequencyInCall));
    else
      ClearInfoRequestRate();
  }
  else
    ClearInfoRequestRate();

  // Remove the endpoint aliases that the gatekeeper did not like and add the
  // ones that it really wants us to be.
  if (rcf.HasOptionalField(H225_RegistrationConfirm::e_terminalAlias) &&
                          !endpoint.OnGatekeeperAliases(rcf.m_terminalAlias)) {
    const PStringList & currentAliases = endpoint.GetAliasNames();
    PStringList aliasesToChange;
    PINDEX i, j;

    for (i = 0; i < rcf.m_terminalAlias.GetSize(); i++) {
      PString alias = H323GetAliasAddressString(rcf.m_terminalAlias[i]);
      if (!alias) {
        for (j = 0; j < currentAliases.GetSize(); j++) {
          if (alias *= currentAliases[j])
            break;
        }
        if (j >= currentAliases.GetSize())
          aliasesToChange.AppendString(alias);
      }
    }
    for (i = 0; i < aliasesToChange.GetSize(); i++) {
      PTRACE(2, "RAS\tGatekeeper add of alias \"" << aliasesToChange[i] << '"');
      endpoint.AddAliasName(aliasesToChange[i]);
    }

    aliasesToChange.RemoveAll();

    for (i = 0; i < currentAliases.GetSize(); i++) {
      for (j = 0; j < rcf.m_terminalAlias.GetSize(); j++) {
        if (currentAliases[i] *= H323GetAliasAddressString(rcf.m_terminalAlias[j]))
          break;
      }
      if (j >= rcf.m_terminalAlias.GetSize())
        aliasesToChange.AppendString(currentAliases[i]);
    }
    for (i = 0; i < aliasesToChange.GetSize(); i++) {
      PTRACE(2, "RAS\tGatekeeper removal of alias \"" << aliasesToChange[i] << '"');
      endpoint.RemoveAliasName(aliasesToChange[i]);
    }
  }

  if (rcf.HasOptionalField(H225_RegistrationConfirm::e_language)) {
      PStringList lang;
      H323GetLanguages(lang,rcf.m_language);
      endpoint.OnReceiveLanguages(lang);
  }

#ifdef H323_H248
  if (rcf.HasOptionalField(H225_RegistrationConfirm::e_serviceControl))
    OnServiceControlSessions(rcf.m_serviceControl, NULL);
#endif

// NAT Detection with GNUGK
#ifdef H323_GNUGK
  if (rcf.HasOptionalField(H225_RegistrationConfirm::e_nonStandardData))
  {
    PString NATaddr = rcf.m_nonStandardData.m_data.AsString();
    if (!NATaddr.IsEmpty()) {
      if (NATaddr.Left(4) == "NAT=") {
        endpoint.OnGatekeeperNATDetect(PIPSocket::Address(NATaddr.Right(NATaddr.GetLength()-4)),endpointIdentifier.GetValue(),gkRouteAddress);
      } else {
        endpoint.OnGatekeeperOpenNATDetect(endpointIdentifier.GetValue(),gkRouteAddress);
      }
    }
  }
#endif

  endpoint.OnRegistrationConfirm(gkRouteAddress);

  return TRUE;
}


PBoolean H323Gatekeeper::OnReceiveRegistrationReject(const H225_RegistrationReject & rrj)
{
  if (!H225_RAS::OnReceiveRegistrationReject(rrj))
    return FALSE;

  if (rrj.HasOptionalField(H225_RegistrationReject::e_assignedGatekeeper))
     SetAssignedGatekeeper(rrj.m_assignedGatekeeper);
  else if (rrj.HasOptionalField(H225_RegistrationReject::e_altGKInfo))
    SetAlternates(rrj.m_altGKInfo.m_alternateGatekeeper,
                  rrj.m_altGKInfo.m_altGKisPermanent);
  else 
      endpoint.OnRegistrationReject();

  return TRUE;
}

void H323Gatekeeper::ReRegisterNow()
{
  PTRACE(3, "RAS\tforcing reregistration");
  RegistrationTimeToLive();

}

void H323Gatekeeper::RegistrationTimeToLive()
{
  PTRACE(3, "RAS\tTime To Live reregistration");

  if (requiresDiscovery || moveAlternate) {
    PTRACE(2, "RAS\tRepeating discovery on gatekeepers request.");

    H323RasPDU pdu;
    Request request(SetupGatekeeperRequest(pdu), pdu);
    request.SetUseAlternate(moveAlternate);
    if (!MakeRequest(request) || (!discoveryComplete && !moveAlternate)) {
      PTRACE(2, "RAS\tRediscovery failed, retrying in 1 minute.");
      timeToLive = PTimeInterval(0, 0, 1);
      return;
    } 
    requiresDiscovery = FALSE;
    moveAlternate = FALSE;
    return;
  }

  reregisterNow = FALSE;
  if (!RegistrationRequest(autoReregister) && (!reregisterNow || !requiresDiscovery)) {
    PTRACE(2,"RAS\tTime To Live reregistration failed, continue retrying.");
    endpoint.OnRegisterTTLFail();
  }
  reregisterNow = TRUE;
}


PBoolean H323Gatekeeper::UnregistrationRequest(int reason)
{
  if (PAssertNULL(transport) == NULL)
    return FALSE;

  H323RasPDU pdu;
  H225_UnregistrationRequest & urq = pdu.BuildUnregistrationRequest(GetNextSequenceNumber());

  H225_TransportAddress rasAddress;
  transport->SetUpTransportPDU(rasAddress, TRUE);

  H323SetTransportAddresses(*transport,
                            endpoint.GetInterfaceAddresses(TRUE, transport),
                            urq.m_callSignalAddress);

  urq.IncludeOptionalField(H225_UnregistrationRequest::e_endpointAlias);
  H323SetAliasAddresses(endpoint.GetAliasNames(), urq.m_endpointAlias);

  if (!gatekeeperIdentifier) {
    urq.IncludeOptionalField(H225_UnregistrationRequest::e_gatekeeperIdentifier);
    urq.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

  if (!endpointIdentifier.GetValue().IsEmpty()) {
    urq.IncludeOptionalField(H225_UnregistrationRequest::e_endpointIdentifier);
    urq.m_endpointIdentifier = endpointIdentifier;
  }

  if (reason >= 0) {
    urq.IncludeOptionalField(H225_UnregistrationRequest::e_reason);
    urq.m_reason = reason;
  }

  Request request(urq.m_requestSeqNum, pdu);
  if (MakeRequest(request))
    return TRUE;

  switch (request.responseResult) {
    case Request::NoResponseReceived :
      registrationFailReason = TransportError;
      timeToLive = 0; // zero disables lightweight RRQ
      break;

    case Request::BadCryptoTokens :
      registrationFailReason = SecurityDenied;
      timeToLive = 0; // zero disables lightweight RRQ
      break;

    default :
      break;
  }

  return !IsRegistered();
}


PBoolean H323Gatekeeper::OnReceiveUnregistrationConfirm(const H225_UnregistrationConfirm & ucf)
{
  if (!H225_RAS::OnReceiveUnregistrationConfirm(ucf))
    return FALSE;

  registrationFailReason = UnregisteredLocally;
  timeToLive = 0; // zero disables lightweight RRQ

  endpoint.OnUnRegisterConfirm();
  return TRUE;
}


PBoolean H323Gatekeeper::OnReceiveUnregistrationRequest(const H225_UnregistrationRequest & urq)
{
  if (!H225_RAS::OnReceiveUnregistrationRequest(urq))
    return FALSE;

  PTRACE(2, "RAS\tUnregistration received");
  if (!urq.HasOptionalField(H225_UnregistrationRequest::e_gatekeeperIdentifier) ||
       urq.m_gatekeeperIdentifier.GetValue() != gatekeeperIdentifier) {
    PTRACE(1, "RAS\tInconsistent gatekeeperIdentifier!");
    return FALSE;
  }

  if (!urq.HasOptionalField(H225_UnregistrationRequest::e_endpointIdentifier) ||
       urq.m_endpointIdentifier != endpointIdentifier) {
    PTRACE(1, "RAS\tInconsistent endpointIdentifier!");
    return FALSE;
  }

  endpoint.ClearAllCalls(H323Connection::EndedByGatekeeper, FALSE);

  PTRACE(3, "RAS\tUnregistered, calls cleared");
  registrationFailReason = UnregisteredByGatekeeper;
//  timeToLive = 0; // zero disables lightweight RRQ

  if (urq.HasOptionalField(H225_UnregistrationRequest::e_alternateGatekeeper)) {
    SetAlternates(urq.m_alternateGatekeeper, FALSE);
    if (alternates.GetSize() > 0) {
        PTRACE(2, "RAS\tTry Alternate Gatekeepers");
        moveAlternate = true;
    }
  }

  H323RasPDU response(authenticators);
  response.BuildUnregistrationConfirm(urq.m_requestSeqNum);
  PBoolean ok = WritePDU(response);

  if (autoReregister) {
    PTRACE(3, "RAS\tReregistering by setting timeToLive");
    reregisterNow = TRUE;
    monitorTickle.Signal();
  } else 
    timeToLive = 0; // zero disables lightweight RRQ

  endpoint.OnUnRegisterRequest();

  return ok;
}


PBoolean H323Gatekeeper::OnReceiveUnregistrationReject(const H225_UnregistrationReject & urj)
{
  if (!H225_RAS::OnReceiveUnregistrationReject(urj))
    return FALSE;

  if (lastRequest->rejectReason != H225_UnregRejectReason::e_callInProgress) {
    registrationFailReason = UnregisteredLocally;
    timeToLive = 0; // zero disables lightweight RRQ
  }

  return TRUE;
}


PBoolean H323Gatekeeper::LocationRequest(const PString & alias,
                                     H323TransportAddress & address)
{
  PStringList aliases;
  aliases.AppendString(alias);
  return LocationRequest(aliases, address);
}


PBoolean H323Gatekeeper::LocationRequest(const PStringList & aliases,
                                     H323TransportAddress & address)
{
  if (PAssertNULL(transport) == NULL)
    return FALSE;

  H323RasPDU pdu;
  H225_LocationRequest & lrq = pdu.BuildLocationRequest(GetNextSequenceNumber());

  H323SetAliasAddresses(aliases, lrq.m_destinationInfo);

  if (!endpointIdentifier.GetValue().IsEmpty()) {
    lrq.IncludeOptionalField(H225_LocationRequest::e_endpointIdentifier);
    lrq.m_endpointIdentifier = endpointIdentifier;
  }

  transport->SetUpTransportPDU(lrq.m_replyAddress, TRUE);

  lrq.IncludeOptionalField(H225_LocationRequest::e_sourceInfo);
  H323SetAliasAddresses(endpoint.GetAliasNames(), lrq.m_sourceInfo);

  if (!gatekeeperIdentifier) {
    lrq.IncludeOptionalField(H225_LocationRequest::e_gatekeeperIdentifier);
    lrq.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

  Request request(lrq.m_requestSeqNum, pdu);
  request.responseInfo = &address;
  if (!MakeRequest(request))
    return FALSE;

  // sanity check the address - some Gks return address 0.0.0.0 and port 0
  PIPSocket::Address ipAddr;
  WORD port = 0;
  return address.GetIpAndPort(ipAddr, port) && (port != 0);
}


H323Gatekeeper::AdmissionResponse::AdmissionResponse()
{
  rejectReason = UINT_MAX;

  gatekeeperRouted = FALSE;
  endpointCount = 1;
  transportAddress = NULL;
  accessTokenData = NULL;

  aliasAddresses = NULL;
  destExtraCallInfo = NULL;
}


struct AdmissionRequestResponseInfo {
  AdmissionRequestResponseInfo(
    H323Gatekeeper::AdmissionResponse & r,
    H323Connection & c
  ) : param(r), connection(c) { }

  H323Gatekeeper::AdmissionResponse & param;
  H323Connection & connection;
  unsigned allocatedBandwidth;
  unsigned uuiesRequested;
  PString      accessTokenOID1;
  PString      accessTokenOID2;
};


PBoolean H323Gatekeeper::AdmissionRequest(H323Connection & connection,
                                      AdmissionResponse & response,
                                      PBoolean ignorePreGrantedARQ)
{
  PBoolean answeringCall = connection.HadAnsweredCall();

  if (!ignorePreGrantedARQ) {
    switch (answeringCall ? pregrantAnswerCall : pregrantMakeCall) {
      case RequireARQ :
        break;
      case PregrantARQ :
        return TRUE;
      case PreGkRoutedARQ :
        if (gkRouteAddress.IsEmpty()) {
          response.rejectReason = UINT_MAX;
          return FALSE;
        }
        if (response.transportAddress != NULL)
          *response.transportAddress = gkRouteAddress;
        response.gatekeeperRouted = TRUE;
        return TRUE;
    }
  }

  H323RasPDU pdu;
  H225_AdmissionRequest & arq = pdu.BuildAdmissionRequest(GetNextSequenceNumber());

  arq.m_callType.SetTag(H225_CallType::e_pointToPoint);
  arq.m_endpointIdentifier = endpointIdentifier;
  arq.m_answerCall = answeringCall;
  arq.m_canMapAlias = TRUE; // Stack supports receiving a different number in the ACF 
                            // to the one sent in the ARQ
  arq.m_willSupplyUUIEs = TRUE;

  if (!gatekeeperIdentifier) {
    arq.IncludeOptionalField(H225_AdmissionRequest::e_gatekeeperIdentifier);
    arq.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

  PString destInfo = connection.GetRemotePartyName();
  arq.m_srcInfo.SetSize(1);
  if (answeringCall) {
    H323SetAliasAddress(destInfo, arq.m_srcInfo[0]);
    if (!connection.GetLocalPartyName()) {
      arq.IncludeOptionalField(H225_AdmissionRequest::e_destinationInfo);
      H323SetAliasAddresses(endpoint.GetAliasNames(), arq.m_destinationInfo);
    }
  }
  else {
    H323SetAliasAddresses(endpoint.GetAliasNames(), arq.m_srcInfo);
    if (response.transportAddress == NULL || destInfo != *response.transportAddress) {
      arq.IncludeOptionalField(H225_AdmissionRequest::e_destinationInfo);
      arq.m_destinationInfo.SetSize(1);
      H323SetAliasAddress(destInfo, arq.m_destinationInfo[0], -2);
    }
  }

  const H323Transport * signallingChannel = connection.GetSignallingChannel();
  if (answeringCall) {
    arq.IncludeOptionalField(H225_AdmissionRequest::e_srcCallSignalAddress);
    signallingChannel->SetUpTransportPDU(arq.m_srcCallSignalAddress, FALSE);
    arq.IncludeOptionalField(H225_AdmissionRequest::e_destCallSignalAddress);
    signallingChannel->SetUpTransportPDU(arq.m_destCallSignalAddress, TRUE);
  }
  else {
    if (signallingChannel != NULL && signallingChannel->IsOpen()) {
      arq.IncludeOptionalField(H225_AdmissionRequest::e_srcCallSignalAddress);
      signallingChannel->SetUpTransportPDU(arq.m_srcCallSignalAddress, TRUE);
    }
    if (response.transportAddress != NULL && !response.transportAddress->IsEmpty()) {
      arq.IncludeOptionalField(H225_AdmissionRequest::e_destCallSignalAddress);
      response.transportAddress->SetPDU(arq.m_destCallSignalAddress);
    }
  }

  arq.m_bandWidth = connection.GetBandwidthRequired();
  arq.m_callReferenceValue = connection.GetCallReference();
  arq.m_conferenceID = connection.GetConferenceIdentifier();
  arq.m_callIdentifier.m_guid = connection.GetCallIdentifier();

#ifdef H323_H450
  connection.SetCallLinkage(pdu);
#endif

  AdmissionRequestResponseInfo info(response, connection);
  info.accessTokenOID1 = connection.GetGkAccessTokenOID();
  PINDEX comma = info.accessTokenOID1.Find(',');
  if (comma == P_MAX_INDEX)
    info.accessTokenOID2 = info.accessTokenOID1;
  else {
    info.accessTokenOID2 = info.accessTokenOID1.Mid(comma+1);
    info.accessTokenOID1.Delete(comma, P_MAX_INDEX);
  }

  connection.OnSendARQ(arq);

  Request request(arq.m_requestSeqNum, pdu);
  request.responseInfo = &info;

  if (!authenticators.IsEmpty()) {
    pdu.Prepare(arq.m_tokens, H225_AdmissionRequest::e_tokens,
                arq.m_cryptoTokens, H225_AdmissionRequest::e_cryptoTokens);

    H235Authenticators adjustedAuthenticators;
    if (connection.GetAdmissionRequestAuthentication(arq, adjustedAuthenticators)) {
      PTRACE(3, "RAS\tAuthenticators credentials replaced with \""
             << setfill(',') << adjustedAuthenticators << setfill(' ') << "\" during ARQ");

      for (PINDEX i = 0; i < adjustedAuthenticators.GetSize(); i++) {
        H235Authenticator & authenticator = adjustedAuthenticators[i];
        if (authenticator.UseGkAndEpIdentifiers())
          authenticator.SetRemoteId(gatekeeperIdentifier);
      }

      adjustedAuthenticators.PreparePDU(pdu,
                                        arq.m_tokens, H225_AdmissionRequest::e_tokens,
                                        arq.m_cryptoTokens, H225_AdmissionRequest::e_cryptoTokens);
      pdu.SetAuthenticators(adjustedAuthenticators);
    }
  }

  if (!MakeRequest(request)) {
    response.rejectReason = request.rejectReason;

    // See if we are registered.
    if (request.responseResult == Request::RejectReceived &&
        response.rejectReason != H225_AdmissionRejectReason::e_callerNotRegistered &&
        response.rejectReason != H225_AdmissionRejectReason::e_invalidEndpointIdentifier)
      return FALSE;

    PTRACE(2, "RAS\tEndpoint has become unregistered during ARQ from gatekeeper " << gatekeeperIdentifier);

    // Have been told we are not registered (or gk offline)
    switch (request.responseResult) {
      case Request::NoResponseReceived :
        registrationFailReason = TransportError;
        response.rejectReason = UINT_MAX;
        break;

      case Request::BadCryptoTokens :
        registrationFailReason = SecurityDenied;
        response.rejectReason = H225_AdmissionRejectReason::e_securityDenial;
        break;

      default :
        registrationFailReason = GatekeeperLostRegistration;
    }

    // If we are not registered and auto register is set ...
    if (!autoReregister)
      return FALSE;

    // Then immediately reregister.
    if (!RegistrationRequest(autoReregister))
      return FALSE;

    // Reset the gk info in ARQ
    arq.m_endpointIdentifier = endpointIdentifier;
    if (!gatekeeperIdentifier) {
      arq.IncludeOptionalField(H225_AdmissionRequest::e_gatekeeperIdentifier);
      arq.m_gatekeeperIdentifier = gatekeeperIdentifier;
    }
    else
      arq.RemoveOptionalField(H225_AdmissionRequest::e_gatekeeperIdentifier);

    // Is new request so need new sequence number as well.
    arq.m_requestSeqNum = GetNextSequenceNumber();
    request.sequenceNumber = arq.m_requestSeqNum;

    if (!MakeRequest(request)) {
      response.rejectReason = request.responseResult == Request::RejectReceived
                                                ? request.rejectReason : UINT_MAX;
     
      return FALSE;
    }
  }

  connection.SetBandwidthAvailable(info.allocatedBandwidth);
  connection.SetUUIEsRequested(info.uuiesRequested);

  return TRUE;
}


void H323Gatekeeper::OnSendAdmissionRequest(H225_AdmissionRequest & /*arq*/)
{
  // Override default function as it sets crypto tokens and this is really
  // done by the AdmissionRequest() function.
}


static unsigned GetUUIEsRequested(const H225_UUIEsRequested & pdu)
{
  unsigned uuiesRequested = 0;

  if ((PBoolean)pdu.m_setup)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_setup);
  if ((PBoolean)pdu.m_callProceeding)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_callProceeding);
  if ((PBoolean)pdu.m_connect)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_connect);
  if ((PBoolean)pdu.m_alerting)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_alerting);
  if ((PBoolean)pdu.m_information)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_information);
  if ((PBoolean)pdu.m_releaseComplete)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_releaseComplete);
  if ((PBoolean)pdu.m_facility)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_facility);
  if ((PBoolean)pdu.m_progress)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_progress);
  if ((PBoolean)pdu.m_empty)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_empty);

  if (pdu.HasOptionalField(H225_UUIEsRequested::e_status) && (PBoolean)pdu.m_status)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_status);
  if (pdu.HasOptionalField(H225_UUIEsRequested::e_statusInquiry) && (PBoolean)pdu.m_statusInquiry)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_statusInquiry);
  if (pdu.HasOptionalField(H225_UUIEsRequested::e_setupAcknowledge) && (PBoolean)pdu.m_setupAcknowledge)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_setupAcknowledge);
  if (pdu.HasOptionalField(H225_UUIEsRequested::e_notify) && (PBoolean)pdu.m_notify)
    uuiesRequested |= (1<<H225_H323_UU_PDU_h323_message_body::e_notify);

  return uuiesRequested;
}


static void ExtractToken(const AdmissionRequestResponseInfo & info,
                         const H225_ArrayOf_ClearToken & tokens,
                         PBYTEArray & accessTokenData)
{
  if (!info.accessTokenOID1 && tokens.GetSize() > 0) {
    PTRACE(4, "Looking for OID " << info.accessTokenOID1 << " in ACF to copy.");
    for (PINDEX i = 0; i < tokens.GetSize(); i++) {
      if (tokens[i].m_tokenOID == info.accessTokenOID1) {
        PTRACE(4, "Looking for OID " << info.accessTokenOID2 << " in token to copy.");
        if (tokens[i].HasOptionalField(H235_ClearToken::e_nonStandard) &&
            tokens[i].m_nonStandard.m_nonStandardIdentifier == info.accessTokenOID2) {
          PTRACE(4, "Copying ACF nonStandard OctetString.");
          accessTokenData = tokens[i].m_nonStandard.m_data;
          break;
        }
      }
    }
  }
}


PBoolean H323Gatekeeper::OnReceiveAdmissionConfirm(const H225_AdmissionConfirm & acf)
{
  if (!H225_RAS::OnReceiveAdmissionConfirm(acf))
    return FALSE;

  AdmissionRequestResponseInfo & info = *(AdmissionRequestResponseInfo *)lastRequest->responseInfo;
  info.allocatedBandwidth = acf.m_bandWidth;
  if (info.param.transportAddress != NULL)
    *info.param.transportAddress = acf.m_destCallSignalAddress;

  info.param.gatekeeperRouted = acf.m_callModel.GetTag() == H225_CallModel::e_gatekeeperRouted;

  // Remove the endpoint aliases that the gatekeeper did not like and add the
  // ones that it really wants us to be.
  if (info.param.aliasAddresses != NULL &&
      acf.HasOptionalField(H225_AdmissionConfirm::e_destinationInfo)) {
    PTRACE(3, "RAS\tGatekeeper specified " << acf.m_destinationInfo.GetSize() << " aliases in ACF");
    *info.param.aliasAddresses = acf.m_destinationInfo;
  }

  if (acf.HasOptionalField(H225_AdmissionConfirm::e_uuiesRequested))
    info.uuiesRequested = GetUUIEsRequested(acf.m_uuiesRequested);

  if (info.param.destExtraCallInfo != NULL &&
      acf.HasOptionalField(H225_AdmissionConfirm::e_destExtraCallInfo))
    *info.param.destExtraCallInfo = acf.m_destExtraCallInfo;

  if (info.param.accessTokenData != NULL && acf.HasOptionalField(H225_AdmissionConfirm::e_tokens))
    ExtractToken(info, acf.m_tokens, *info.param.accessTokenData);

  if (info.param.transportAddress != NULL) {
    PINDEX count = 1;
    for (PINDEX i = 0; i < acf.m_alternateEndpoints.GetSize() && count < info.param.endpointCount; i++) {
      if (acf.m_alternateEndpoints[i].HasOptionalField(H225_Endpoint::e_callSignalAddress) &&
          acf.m_alternateEndpoints[i].m_callSignalAddress.GetSize() > 0) {
        info.param.transportAddress[count] = acf.m_alternateEndpoints[i].m_callSignalAddress[0];
        if (info.param.accessTokenData != NULL)
          ExtractToken(info, acf.m_alternateEndpoints[i].m_tokens, info.param.accessTokenData[count]);
        count++;
      }
    }
    info.param.endpointCount = count;
  }

  if (acf.HasOptionalField(H225_AdmissionConfirm::e_irrFrequency))
    SetInfoRequestRate(AdjustTimeout(acf.m_irrFrequency));
  willRespondToIRR = acf.m_willRespondToIRR;

  info.connection.OnReceivedACF(acf);

#ifdef H323_H248
  if (acf.HasOptionalField(H225_AdmissionConfirm::e_serviceControl))
    OnServiceControlSessions(acf.m_serviceControl, &info.connection);
#endif

  return TRUE;
}


PBoolean H323Gatekeeper::OnReceiveAdmissionReject(const H225_AdmissionReject & arj)
{
  if (!H225_RAS::OnReceiveAdmissionReject(arj))
    return FALSE;

  AdmissionRequestResponseInfo & info = *(AdmissionRequestResponseInfo *)lastRequest->responseInfo;
  info.connection.OnReceivedARJ(arj);

#ifdef H323_H248
  if (arj.HasOptionalField(H225_AdmissionConfirm::e_serviceControl))
    OnServiceControlSessions(arj.m_serviceControl,&info.connection);
#endif

  return TRUE;
}


static void SetRasUsageInformation(const H323Connection & connection, 
                                   H225_RasUsageInformation & usage)
{
  unsigned time = connection.GetAlertingTime().GetTimeInSeconds();
  if (time != 0) {
    usage.IncludeOptionalField(H225_RasUsageInformation::e_alertingTime);
    usage.m_alertingTime = time;
  }

  time = connection.GetConnectionStartTime().GetTimeInSeconds();
  if (time != 0) {
    usage.IncludeOptionalField(H225_RasUsageInformation::e_connectTime);
    usage.m_connectTime = time;
  }

  time = connection.GetConnectionEndTime().GetTimeInSeconds();
  if (time != 0) {
    usage.IncludeOptionalField(H225_RasUsageInformation::e_endTime);
    usage.m_endTime = time;
  }
}


PBoolean H323Gatekeeper::DisengageRequest(const H323Connection & connection, unsigned reason)
{
  H323RasPDU pdu;
  H225_DisengageRequest & drq = pdu.BuildDisengageRequest(GetNextSequenceNumber());

  drq.m_endpointIdentifier = endpointIdentifier;
  drq.m_conferenceID = connection.GetConferenceIdentifier();
  drq.m_callReferenceValue = connection.GetCallReference();
  drq.m_callIdentifier.m_guid = connection.GetCallIdentifier();
  drq.m_disengageReason.SetTag(reason);
  drq.m_answeredCall = connection.HadAnsweredCall();

  drq.IncludeOptionalField(H225_DisengageRequest::e_usageInformation);
  SetRasUsageInformation(connection, drq.m_usageInformation);

  drq.IncludeOptionalField(H225_DisengageRequest::e_terminationCause);
  drq.m_terminationCause.SetTag(H225_CallTerminationCause::e_releaseCompleteReason);
  Q931::CauseValues cause = H323TranslateFromCallEndReason(connection, drq.m_terminationCause);
  if (cause != Q931::ErrorInCauseIE) {
    drq.m_terminationCause.SetTag(H225_CallTerminationCause::e_releaseCompleteCauseIE);
    PASN_OctetString & rcReason = drq.m_terminationCause;
    rcReason.SetSize(2);
    rcReason[0] = 0x80;
    rcReason[1] = (BYTE)(0x80|cause);
  }

  if (!gatekeeperIdentifier) {
    drq.IncludeOptionalField(H225_DisengageRequest::e_gatekeeperIdentifier);
    drq.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

  connection.OnSendDRQ(drq);

  Request request(drq.m_requestSeqNum, pdu);
  return MakeRequestWithReregister(request, H225_DisengageRejectReason::e_notRegistered);
}


PBoolean H323Gatekeeper::OnReceiveDisengageRequest(const H225_DisengageRequest & drq)
{
  if (!H225_RAS::OnReceiveDisengageRequest(drq))
    return FALSE;

  OpalGloballyUniqueID id = NULL;
  if (drq.HasOptionalField(H225_DisengageRequest::e_callIdentifier))
    id = drq.m_callIdentifier.m_guid;
  if (id == NULL)
    id = drq.m_conferenceID;

  H323RasPDU response(authenticators);
  H323Connection * connection = endpoint.FindConnectionWithLock(id.AsString());
  if (connection == NULL)
    response.BuildDisengageReject(drq.m_requestSeqNum,
                                  H225_DisengageRejectReason::e_requestToDropOther);
  else {
    H225_DisengageConfirm & dcf = response.BuildDisengageConfirm(drq.m_requestSeqNum);

    dcf.IncludeOptionalField(H225_DisengageConfirm::e_usageInformation);
    SetRasUsageInformation(*connection, dcf.m_usageInformation);

    connection->ClearCall(H323Connection::EndedByGatekeeper);
    connection->Unlock();
  }

#ifdef H323_H248
  if (drq.HasOptionalField(H225_DisengageRequest::e_serviceControl))
    OnServiceControlSessions(drq.m_serviceControl, connection);
#endif

  return WritePDU(response);
}


PBoolean H323Gatekeeper::BandwidthRequest(H323Connection & connection,
                                      unsigned requestedBandwidth)
{
  H323RasPDU pdu;
  H225_BandwidthRequest & brq = pdu.BuildBandwidthRequest(GetNextSequenceNumber());

  brq.m_endpointIdentifier = endpointIdentifier;
  brq.m_conferenceID = connection.GetConferenceIdentifier();
  brq.m_callReferenceValue = connection.GetCallReference();
  brq.m_callIdentifier.m_guid = connection.GetCallIdentifier();
  brq.m_bandWidth = requestedBandwidth;
  brq.IncludeOptionalField(H225_BandwidthRequest::e_usageInformation);
  SetRasUsageInformation(connection, brq.m_usageInformation);

  Request request(brq.m_requestSeqNum, pdu);
  
  unsigned allocatedBandwidth;
  request.responseInfo = &allocatedBandwidth;

  if (!MakeRequestWithReregister(request, H225_BandRejectReason::e_notBound))
    return FALSE;

  connection.SetBandwidthAvailable(allocatedBandwidth);
  return TRUE;
}


PBoolean H323Gatekeeper::OnReceiveBandwidthConfirm(const H225_BandwidthConfirm & bcf)
{
  if (!H225_RAS::OnReceiveBandwidthConfirm(bcf))
    return FALSE;

  if (lastRequest->responseInfo != NULL)
    *(unsigned *)lastRequest->responseInfo = bcf.m_bandWidth;

  return TRUE;
}


PBoolean H323Gatekeeper::OnReceiveBandwidthRequest(const H225_BandwidthRequest & brq)
{
  if (!H225_RAS::OnReceiveBandwidthRequest(brq))
    return FALSE;

  OpalGloballyUniqueID id = brq.m_callIdentifier.m_guid;
  H323Connection * connection = endpoint.FindConnectionWithLock(id.AsString());

  H323RasPDU response(authenticators);
  if (connection == NULL)
    response.BuildBandwidthReject(brq.m_requestSeqNum,
                                  H225_BandRejectReason::e_invalidConferenceID);
  else {
    if (connection->SetBandwidthAvailable(brq.m_bandWidth))
      response.BuildBandwidthConfirm(brq.m_requestSeqNum, brq.m_bandWidth);
    else
      response.BuildBandwidthReject(brq.m_requestSeqNum,
                                    H225_BandRejectReason::e_insufficientResources);
    connection->Unlock();
  }

  return WritePDU(response);
}


void H323Gatekeeper::SetInfoRequestRate(const PTimeInterval & rate)
{
  if (rate < infoRequestRate.GetResetTime() || infoRequestRate.GetResetTime() == 0) {
    // Have to be sneaky here becuase we do not want to actually change the
    // amount of time to run on the timer.
    PTimeInterval timeToGo = infoRequestRate;
    infoRequestRate = rate;
    if (rate > timeToGo)
      infoRequestRate.PTimeInterval::operator=(timeToGo);
  }
}


void H323Gatekeeper::ClearInfoRequestRate()
{
  // Only reset rate to zero (disabled) if no calls present
  if (endpoint.GetAllConnections().IsEmpty())
    infoRequestRate = 0;
}


H225_InfoRequestResponse & H323Gatekeeper::BuildInfoRequestResponse(H323RasPDU & response,
                                                                    unsigned seqNum)
{
  H225_InfoRequestResponse & irr = response.BuildInfoRequestResponse(seqNum);

  endpoint.SetEndpointTypeInfo(irr.m_endpointType);
  irr.m_endpointIdentifier = endpointIdentifier;
  transport->SetUpTransportPDU(irr.m_rasAddress, TRUE);
  H323SetTransportAddresses(*transport,
                            endpoint.GetInterfaceAddresses(TRUE, transport),
                            irr.m_callSignalAddress);

  irr.IncludeOptionalField(H225_InfoRequestResponse::e_endpointAlias);
  H323SetAliasAddresses(endpoint.GetAliasNames(), irr.m_endpointAlias);

  return irr;
}


PBoolean H323Gatekeeper::SendUnsolicitedIRR(H225_InfoRequestResponse & irr,
                                        H323RasPDU & response)
{
  irr.m_unsolicited = TRUE;

  if (willRespondToIRR) {
    PTRACE(4, "RAS\tSending unsolicited IRR and awaiting acknowledgement");
    Request request(irr.m_requestSeqNum, response);
    return MakeRequest(request);
  }

  PTRACE(4, "RAS\tSending unsolicited IRR and without acknowledgement");
  response.SetAuthenticators(authenticators);
  return WritePDU(response);
}


static void AddInfoRequestResponseCall(H225_InfoRequestResponse & irr,
                                       const H323Connection & connection)
{
  irr.IncludeOptionalField(H225_InfoRequestResponse::e_perCallInfo);

  PINDEX sz = irr.m_perCallInfo.GetSize();
  if (!irr.m_perCallInfo.SetSize(sz+1))
    return;

  H225_InfoRequestResponse_perCallInfo_subtype & info = irr.m_perCallInfo[sz];

  info.m_callReferenceValue = connection.GetCallReference();
  info.m_callIdentifier.m_guid = connection.GetCallIdentifier();
  info.m_conferenceID = connection.GetConferenceIdentifier();
  info.IncludeOptionalField(H225_InfoRequestResponse_perCallInfo_subtype::e_originator);
  info.m_originator = !connection.HadAnsweredCall();

  H323_RTP_Session * session = connection.GetSessionCallbacks(RTP_Session::DefaultAudioSessionID);
  if (session != NULL) {
    info.IncludeOptionalField(H225_InfoRequestResponse_perCallInfo_subtype::e_audio);
    info.m_audio.SetSize(1);
    session->OnSendRasInfo(info.m_audio[0]);
  }

  session = connection.GetSessionCallbacks(RTP_Session::DefaultVideoSessionID);
  if (session != NULL) {
    info.IncludeOptionalField(H225_InfoRequestResponse_perCallInfo_subtype::e_video);
    info.m_video.SetSize(1);
    session->OnSendRasInfo(info.m_video[0]);
  }

  const H323Transport & controlChannel = connection.GetControlChannel();
  controlChannel.SetUpTransportPDU(info.m_h245.m_recvAddress, TRUE);
  controlChannel.SetUpTransportPDU(info.m_h245.m_sendAddress, FALSE);

  info.m_callType.SetTag(H225_CallType::e_pointToPoint);
  info.m_bandWidth = connection.GetBandwidthUsed();
  info.m_callModel.SetTag(connection.IsGatekeeperRouted() ? H225_CallModel::e_gatekeeperRouted
                                                          : H225_CallModel::e_direct);

  info.IncludeOptionalField(H225_InfoRequestResponse_perCallInfo_subtype::e_usageInformation);
  SetRasUsageInformation(connection, info.m_usageInformation);
}


static PBoolean AddAllInfoRequestResponseCall(H225_InfoRequestResponse & irr,
                                          H323EndPoint & endpoint,
                                          const PStringList & tokens)
{
  PBoolean addedOne = FALSE;

  for (PINDEX i = 0; i < tokens.GetSize(); i++) {
    H323Connection * connection = endpoint.FindConnectionWithLock(tokens[i]);
    if (connection != NULL) {
      AddInfoRequestResponseCall(irr, *connection);
      connection->OnSendIRR(irr);
      connection->Unlock();
      addedOne = TRUE;
    }
  }

  return addedOne;
}


void H323Gatekeeper::InfoRequestResponse()
{
  PStringList tokens = endpoint.GetAllConnections();
  if (tokens.IsEmpty())
    return;

  H323RasPDU response;
  H225_InfoRequestResponse & irr = BuildInfoRequestResponse(response, GetNextSequenceNumber());

  if (AddAllInfoRequestResponseCall(irr, endpoint, tokens))
    SendUnsolicitedIRR(irr, response);
}


void H323Gatekeeper::InfoRequestResponse(const H323Connection & connection)
{
  H323RasPDU response;
  H225_InfoRequestResponse & irr = BuildInfoRequestResponse(response, GetNextSequenceNumber());

  AddInfoRequestResponseCall(irr, connection);

  connection.OnSendIRR(irr);

  SendUnsolicitedIRR(irr, response);
}


void H323Gatekeeper::InfoRequestResponse(const H323Connection & connection,
                                         const H225_H323_UU_PDU & pdu,
                                         PBoolean sent)
{
  // Are unknown Q.931 PDU
  if (pdu.m_h323_message_body.GetTag() == P_MAX_INDEX)
    return;

  // Check mask of things to report on
  if ((connection.GetUUIEsRequested() & (1<<pdu.m_h323_message_body.GetTag())) == 0)
    return;

  PTRACE(3, "RAS\tSending unsolicited IRR for requested UUIE");

  // Report the PDU
  H323RasPDU response;
  H225_InfoRequestResponse & irr = BuildInfoRequestResponse(response, GetNextSequenceNumber());

  AddInfoRequestResponseCall(irr, connection);

  irr.m_perCallInfo[0].IncludeOptionalField(H225_InfoRequestResponse_perCallInfo_subtype::e_pdu);
  irr.m_perCallInfo[0].m_pdu.SetSize(1);
  irr.m_perCallInfo[0].m_pdu[0].m_sent = sent;
  irr.m_perCallInfo[0].m_pdu[0].m_h323pdu = pdu;

  connection.OnSendIRR(irr);
  SendUnsolicitedIRR(irr, response);
}


PBoolean H323Gatekeeper::OnReceiveInfoRequest(const H225_InfoRequest & irq)
{
  if (!H225_RAS::OnReceiveInfoRequest(irq))
    return FALSE;

  H323RasPDU response(authenticators);
  H225_InfoRequestResponse & irr = BuildInfoRequestResponse(response, irq.m_requestSeqNum);

  if (irq.m_callReferenceValue == 0) {
    if (!AddAllInfoRequestResponseCall(irr, endpoint, endpoint.GetAllConnections())) {
      irr.IncludeOptionalField(H225_InfoRequestResponse::e_irrStatus);
      irr.m_irrStatus.SetTag(H225_InfoRequestResponseStatus::e_invalidCall);
    }
  }
  else {
    OpalGloballyUniqueID id = irq.m_callIdentifier.m_guid;
    H323Connection * connection = endpoint.FindConnectionWithLock(id.AsString());
    if (connection == NULL) {
      irr.IncludeOptionalField(H225_InfoRequestResponse::e_irrStatus);
      irr.m_irrStatus.SetTag(H225_InfoRequestResponseStatus::e_invalidCall);
    }
    else {
      if (irq.HasOptionalField(H225_InfoRequest::e_uuiesRequested))
        connection->SetUUIEsRequested(::GetUUIEsRequested(irq.m_uuiesRequested));

      AddInfoRequestResponseCall(irr, *connection);

      connection->Unlock();
    }
  }

  if (!irq.HasOptionalField(H225_InfoRequest::e_replyAddress))
    return WritePDU(response);

  H323TransportAddress replyAddress = irq.m_replyAddress;
  if (replyAddress.IsEmpty())
    return FALSE;

  H323TransportAddress oldAddress = transport->GetRemoteAddress();

  PBoolean ok = transport->ConnectTo(replyAddress) && WritePDU(response);

  transport->ConnectTo(oldAddress);

  return ok;
}


PBoolean H323Gatekeeper::SendServiceControlIndication()
{
  PTRACE(3, "RAS\tSending Empty ServiceControlIndication");

  H323RasPDU pdu;
  H225_ServiceControlIndication & sci = pdu.BuildServiceControlIndication(GetNextSequenceNumber());

  sci.m_serviceControl.SetSize(0);

  Request request(sci.m_requestSeqNum, pdu);
  return MakeRequest(request);
}

PBoolean H323Gatekeeper::OnReceiveServiceControlIndication(const H225_ServiceControlIndication & sci)
{
  if (!H225_RAS::OnReceiveServiceControlIndication(sci))
    return FALSE;

  H323Connection * connection = NULL;

  if (sci.HasOptionalField(H225_ServiceControlIndication::e_callSpecific)) {
    OpalGloballyUniqueID id = sci.m_callSpecific.m_callIdentifier.m_guid;
    if (id.IsNULL())
      id = sci.m_callSpecific.m_conferenceID;
    connection = endpoint.FindConnectionWithLock(id.AsString());
  }

#ifdef H323_H248
  OnServiceControlSessions(sci.m_serviceControl, connection);
#endif


  H323RasPDU response(authenticators);
  response.BuildServiceControlResponse(sci.m_requestSeqNum);
  return WritePDU(response);
}

#ifdef H323_H248

void H323Gatekeeper::OnServiceControlSessions(const H225_ArrayOf_ServiceControlSession & serviceControl,
                                              H323Connection * connection)
{
  for (PINDEX i = 0; i < serviceControl.GetSize(); i++) {
    H225_ServiceControlSession & pdu = serviceControl[i];

    H323ServiceControlSession * session = NULL;
    unsigned sessionId = pdu.m_sessionId;

    if (serviceControlSessions.Contains(sessionId)) {
      session = &serviceControlSessions[sessionId];
      if (pdu.HasOptionalField(H225_ServiceControlSession::e_contents)) {
        if (!session->OnReceivedPDU(pdu.m_contents)) {
          PTRACE(2, "SvcCtrl\tService control for session has changed!");
          session = NULL;
        }
      }
    }

    if (session == NULL && pdu.HasOptionalField(H225_ServiceControlSession::e_contents)) {
      session = endpoint.CreateServiceControlSession(pdu.m_contents);
      serviceControlSessions.SetAt(sessionId, session);
    }

    if (session != NULL)
      endpoint.OnServiceControlSession(pdu.m_reason.GetTag(),sessionId, *session, connection);
  }
}

#endif // H323_H248


void H323Gatekeeper::SetPassword(const PString & password, 
                                 const PString & username)
{
  localId = username;
  if (localId.IsEmpty())
    localId = endpoint.GetLocalUserName();

  for (PINDEX i = 0; i < authenticators.GetSize(); i++) {
    authenticators[i].SetLocalId(localId);
    authenticators[i].SetPassword(password);
  }
}


void H323Gatekeeper::MonitorMain(PThread &, INT)
{
  PTRACE(3, "RAS\tBackground thread started");

  for (;;) {
    monitorTickle.Wait();
    if (monitorStop)
      break;
    if (reregisterNow || 
                (!timeToLive.IsRunning() && timeToLive.GetResetTime() > 0)) {
      RegistrationTimeToLive();
      timeToLive.Reset();
    }

    if (!infoRequestRate.IsRunning() && infoRequestRate.GetResetTime() > 0) {
      InfoRequestResponse();
      infoRequestRate.Reset();
    }
  }

  PTRACE(3, "RAS\tBackground thread ended");
}


void H323Gatekeeper::TickleMonitor(PTimer &, INT)
{
  monitorTickle.Signal();
}


void H323Gatekeeper::SetAlternates(const H225_ArrayOf_AlternateGK & alts, PBoolean permanent)
{
  PINDEX i;

  if (!alternatePermanent)  {
    // don't want to replace alternates gatekeepers if this is an alternate and it's not permanent
    for (i = 0; i < alternates.GetSize(); i++) {
      if (transport->GetRemoteAddress().IsEquivalent(alternates[i].rasAddress) &&
          gatekeeperIdentifier == alternates[i].gatekeeperIdentifier)
        return;
    }
  }

  alternates.SetSize(0);
  for (i = 0; i < alts.GetSize(); i++) {
	  if (AlternateInfo::IsValid(alts[i])) {
		PTRACE(3, "RAS\tAdded alternate gatekeeper:" << H323TransportAddress(alts[i].m_rasAddress));
		alternates.Append(new AlternateInfo(alts[i]));
	  }
  }

  if (alternates.GetSize() > 0)
      alternatePermanent = permanent;
}

void H323Gatekeeper::SetAssignedGatekeeper(const H225_AlternateGK & gk)
{
   assignedGK.SetAlternate(gk);
}

PBoolean H323Gatekeeper::GetAssignedGatekeeper(H225_AlternateGK & gk)
{
    if (!assignedGK.IsValid())
        return FALSE;

    gk = assignedGK.GetAlternate();
    return TRUE;
}


PBoolean H323Gatekeeper::MakeRequestWithReregister(Request & request, unsigned unregisteredTag)
{
  if (MakeRequest(request))
    return TRUE;

  if (request.responseResult == Request::RejectReceived &&
      request.rejectReason != unregisteredTag)
    return FALSE;

  PTRACE(2, "RAS\tEndpoint has become unregistered from gatekeeper " << gatekeeperIdentifier);

  // Have been told we are not registered (or gk offline)
  switch (request.responseResult) {
    case Request::NoResponseReceived :
      registrationFailReason = TransportError;
      break;

    case Request::BadCryptoTokens :
      registrationFailReason = SecurityDenied;
      break;

    default :
      registrationFailReason = GatekeeperLostRegistration;
  }

  // If we are not registered and auto register is set ...
  if (!autoReregister)
    return FALSE;

  reregisterNow = TRUE;
  monitorTickle.Signal();
  return FALSE;
}


void H323Gatekeeper::Connect(const H323TransportAddress & address,
                             const PString & gkid)
{
  if (transport == NULL)
      transport = new H323TransportUDP(endpoint, PIPSocket::Address::GetAny(4));

  transport->SetRemoteAddress(address);
  transport->Connect();
  if (!gkid.IsEmpty())
    gatekeeperIdentifier = gkid;
}


PBoolean H323Gatekeeper::MakeRequest(Request & request)
{
  if (PAssertNULL(transport) == NULL)
    return FALSE;

  // Set authenticators if not already set by caller
  requestMutex.Wait();

  if (request.requestPDU.GetAuthenticators().IsEmpty())
    request.requestPDU.SetAuthenticators(authenticators);

  /* To be sure that the H323 Cleaner, H225 Caller or Monitor don't set the
     transport address of the alternate while the other is in timeout. We
     have to block the function */

  H323TransportAddress tempAddr = transport->GetRemoteAddress();
  PString tempIdentifier = gatekeeperIdentifier;

  PINDEX alt = 0;
  for (;;) {
    if (!request.useAlternate && H225_RAS::MakeRequest(request)) {
      if (!alternatePermanent &&
            (transport->GetRemoteAddress() != tempAddr ||
             gatekeeperIdentifier != tempIdentifier))
        Connect(tempAddr, tempIdentifier);
      requestMutex.Signal();
      return TRUE;
    }
    
    if (request.responseResult != Request::NoResponseReceived &&
        request.responseResult != Request::TryAlternate) {
      // try alternate in those cases and see if it's successful
      requestMutex.Signal();
      return FALSE;
    }

    if (request.responseResult == Request::NoResponseReceived && 
        endpoint.GetConnections().GetSize() > 0) {
        PTRACE(2,"GK\tRegistration no response. Unregister deferred as on call.");
        requestMutex.Signal();
        return TRUE;
    }
    
    AlternateInfo * altInfo;
    PIPSocket::Address localAddress;
    WORD localPort = 0;
    do {
      if (alt >= alternates.GetSize()) {
        if (!alternatePermanent) 
          Connect(tempAddr,tempIdentifier);
        requestMutex.Signal();
        return FALSE;
      }
      
      altInfo = &alternates[alt++];
      transport->GetLocalAddress().GetIpAndPort(localAddress,localPort);
      transport->CleanUpOnTermination();
      delete transport;

      transport = new H323TransportUDP(endpoint,localAddress,localPort);
      transport->SetRemoteAddress (altInfo->rasAddress);
      transport->Connect();
      gatekeeperIdentifier = altInfo->gatekeeperIdentifier;
      StartChannel();
    } while (altInfo->registrationState == AlternateInfo::RegistrationFailed);
    
    if (altInfo->registrationState == AlternateInfo::NeedToRegister) {
      altInfo->registrationState = AlternateInfo::RegistrationFailed;
      registrationFailReason = TransportError;
      discoveryComplete = FALSE;
      H323RasPDU pdu;
      Request req(SetupGatekeeperRequest(pdu), pdu);
      
      if (H225_RAS::MakeRequest(req)) {
        requestMutex.Signal(); // avoid deadlock...
        if (RegistrationRequest(autoReregister)) {
          altInfo->registrationState = AlternateInfo::IsRegistered;
          // The wanted registration is done, we can return
          if (request.requestPDU.GetChoice().GetTag() == H225_RasMessage::e_registrationRequest) {
            if (!alternatePermanent)
              Connect(tempAddr,tempIdentifier);
          }
          return TRUE;
        }
        requestMutex.Wait();
      }
    }
  }
}
   
H323Gatekeeper::AlternateInfo::AlternateInfo()
{
    H323TransportAddress(PIPSocket::Address::GetAny(4),1719).SetPDU(rasAddress);
}

H323Gatekeeper::AlternateInfo::AlternateInfo(const H225_AlternateGK & alt)
{
    SetAlternate(alt);
}


H323Gatekeeper::AlternateInfo::~AlternateInfo ()
{

}


PObject::Comparison H323Gatekeeper::AlternateInfo::Compare(const PObject & obj) const
{
  PAssert(PIsDescendant(&obj, H323Gatekeeper), PInvalidCast);
  unsigned otherPriority = ((const AlternateInfo & )obj).priority;
  if (priority < otherPriority)
    return LessThan;
  if (priority > otherPriority)
    return GreaterThan;
  return EqualTo;
}

H225_AlternateGK H323Gatekeeper::AlternateInfo::GetAlternate()
{
    H225_AlternateGK gk;
    gk.m_rasAddress = rasAddress;
    gk.m_gatekeeperIdentifier = gatekeeperIdentifier;
    gk.m_priority = priority;
    gk.m_needToRegister = registrationState;

    return gk;
}

void H323Gatekeeper::AlternateInfo::GetAlternate(AlternateInfo & alt)
{
	alt.rasAddress = rasAddress;
    alt.gatekeeperIdentifier = gatekeeperIdentifier;
    alt.priority = priority;
	alt.registrationState = registrationState;
}

void H323Gatekeeper::AlternateInfo::SetAlternate(const H225_AlternateGK & alt)
{
	rasAddress = alt.m_rasAddress;
    gatekeeperIdentifier = alt.m_gatekeeperIdentifier.GetValue();
    priority = alt.m_priority;
	registrationState = alt.m_needToRegister ? NeedToRegister : NoRegistrationNeeded;
}

void H323Gatekeeper::AlternateInfo::PrintOn(ostream & strm) const
{
  if (!gatekeeperIdentifier)
    strm << gatekeeperIdentifier << '@';

  strm << rasAddress;

  if (priority > 0)
    strm << ";priority=" << priority;
}

PBoolean H323Gatekeeper::AlternateInfo::IsValid() const
{
    PIPSocket::Address addr;
    H323TransportAddress(rasAddress).GetIpAddress(addr);

    if (addr.IsValid()) {
        if (!addr.IsAny() && !addr.IsLoopback()) 
            return true;
        else 
            return false;
    }

    PTRACE(2,"GKALT\tAlternate Address " << addr << " is not valid. Ignoring...");
    return false;
}

PBoolean H323Gatekeeper::AlternateInfo::IsValid(const H225_AlternateGK & alt)
{
	H323TransportAddress taddr = alt.m_rasAddress;
    PIPSocket::Address addr;
    taddr.GetIpAddress(addr);

    if (addr.IsValid() && !addr.IsAny() && !addr.IsLoopback()) 
        return true;

    return false;
}

PBoolean H323Gatekeeper::OnSendFeatureSet(unsigned pduType, H225_FeatureSet & feats, PBoolean advertise) const
{
#ifdef H323_H460
    return features->SendFeature(pduType, feats, advertise);
#else
    return endpoint.OnSendFeatureSet(pduType, feats, advertise);
#endif
}

void H323Gatekeeper::OnReceiveFeatureSet(unsigned pduType, const H225_FeatureSet & feats) const
{
#ifdef H323_H460
    features->ReceiveFeature(pduType, feats);
#else
    endpoint.OnReceiveFeatureSet(pduType, feats);
#endif
}

#ifdef H323_H460
void H323Gatekeeper::DisableFeatureSet(int msgtype) const
{
    features->DisableAllFeatures(msgtype);
}

H460_FeatureSet & H323Gatekeeper::GetFeatures()
{
    return *features;
}
#endif

/////////////////////////////////////////////////////////////////////////////
