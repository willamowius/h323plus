/*
 * gkserver.cxx
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
 * This code was based on original code from OpenGate of Egoboo Ltd. thanks
 * to Ashley Unitt for his efforts.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "gkserver.h"
#endif

#include "gkserver.h"

#ifdef H323_H501
#include "peclient.h"
#endif

const char AnswerCallStr[] = "-Answer";
const char OriginateCallStr[] = "-Originate";


#define new PNEW


/////////////////////////////////////////////////////////////////////////////

H323GatekeeperRequest::H323GatekeeperRequest(H323GatekeeperListener & ras,
                                             const H323RasPDU & pdu)
  : H323Transaction(ras, pdu, new H323RasPDU, new H323RasPDU),
    rasChannel(ras)
{
}


H323TransactionPDU * H323GatekeeperRequest::CreateRIP(unsigned sequenceNumber,
                                                      unsigned delay) const
{
  H323RasPDU * pdu = new H323RasPDU;
  pdu->BuildRequestInProgress(sequenceNumber, delay);
  return pdu;
}


PBoolean H323GatekeeperRequest::WritePDU(H323TransactionPDU & pdu)
{
  PTRACE_BLOCK("H323GatekeeperRequest::WritePDU");

  if (endpoint != NULL)
    replyAddresses = endpoint->GetRASAddresses();

  return H323Transaction::WritePDU(pdu);
}


PBoolean H323GatekeeperRequest::CheckGatekeeperIdentifier()
{
  PString pduGkid = GetGatekeeperIdentifier();
  if (pduGkid.IsEmpty()) // Not present in PDU
    return TRUE;

  PString rasGkid = rasChannel.GetIdentifier();

  // If it is present it has to be correct!
  if (pduGkid == rasGkid)
    return TRUE;

  SetRejectReason(GetGatekeeperRejectTag());
  PTRACE(2, "RAS\t" << GetName() << " rejected, has different identifier,"
            " got \"" << pduGkid << "\", should be \"" << rasGkid << '"');
  return FALSE;
}


PBoolean H323GatekeeperRequest::GetRegisteredEndPoint()
{
  if (endpoint != NULL) {
    PTRACE(4, "RAS\tAlready located endpoint: " << *endpoint);
    return TRUE;
  }

  PString id = GetEndpointIdentifier();
  endpoint = rasChannel.GetGatekeeper().FindEndPointByIdentifier(id);
  if (endpoint != NULL) {
    PTRACE(4, "RAS\tLocated endpoint: " << *endpoint);
    canSendRIP = endpoint->CanReceiveRIP();
    return TRUE;
  }

  SetRejectReason(GetRegisteredEndPointRejectTag());
  PTRACE(2, "RAS\t" << GetName() << " rejected,"
            " \"" << id << "\" not registered");
  return FALSE;
}


PBoolean H323GatekeeperRequest::CheckCryptoTokens()
{
  if (authenticatorResult == H235Authenticator::e_Disabled)
    return H323Transaction::CheckCryptoTokens(endpoint->GetAuthenticators());

  return authenticatorResult == H235Authenticator::e_OK;
}


/////////////////////////////////////////////////////////////////////////////

H323GatekeeperGRQ::H323GatekeeperGRQ(H323GatekeeperListener & rasChannel,
                                     const H323RasPDU & pdu)
  : H323GatekeeperRequest(rasChannel, pdu),
    grq((H225_GatekeeperRequest &)request->GetChoice().GetObject()),
    gcf(((H323RasPDU &)confirm->GetPDU()).BuildGatekeeperConfirm(grq.m_requestSeqNum)),
    grj(((H323RasPDU &)reject->GetPDU()).BuildGatekeeperReject(grq.m_requestSeqNum,
                                          H225_GatekeeperRejectReason::e_terminalExcluded))
{
  // Check the return address, if not the same side of a NAT firewall, then
  // just use the physical reply address already set by ancestor.
  H323TransportAddress rasAddress = grq.m_rasAddress;
  H323EndPoint & ep = rasChannel.GetEndPoint();
  PIPSocket::Address senderIP, rasIP;

  if (rasChannel.GetTransport().IsCompatibleTransport(grq.m_rasAddress) &&
       (!replyAddresses[0].GetIpAddress(senderIP) ||
        !rasAddress.GetIpAddress(rasIP) ||
        ep.IsLocalAddress(senderIP) == ep.IsLocalAddress(rasIP))) {
    PTRACE(4, "RAS\tFound suitable RAS address in GRQ: " << rasAddress);
    replyAddresses[0] = rasAddress;
  }
  else {
    isBehindNAT = TRUE;
    PTRACE(3, "RAS\tUnsuitable RAS address in GRQ, using " << replyAddresses[0]);
  }
}


#if PTRACING
const char * H323GatekeeperGRQ::GetName() const
{
  return "GRQ";
}
#endif


PString H323GatekeeperGRQ::GetGatekeeperIdentifier() const
{
  if (grq.HasOptionalField(H225_GatekeeperRequest::e_gatekeeperIdentifier))
    return grq.m_gatekeeperIdentifier;

  return PString::Empty();
}


unsigned H323GatekeeperGRQ::GetGatekeeperRejectTag() const
{
  return H225_GatekeeperRejectReason::e_terminalExcluded;
}


PString H323GatekeeperGRQ::GetEndpointIdentifier() const
{
  return PString::Empty(); // Dummy, should never be used
}


unsigned H323GatekeeperGRQ::GetRegisteredEndPointRejectTag() const
{
  return H225_GatekeeperRejectReason::e_terminalExcluded;
}


H235Authenticator::ValidationResult H323GatekeeperGRQ::ValidatePDU() const
{
  return request->Validate(grq.m_tokens, H225_GatekeeperRequest::e_tokens,
                           grq.m_cryptoTokens, H225_GatekeeperRequest::e_cryptoTokens);
}


unsigned H323GatekeeperGRQ::GetSecurityRejectTag() const
{
  return H225_GatekeeperRejectReason::e_securityDenial;
}


void H323GatekeeperGRQ::SetRejectReason(unsigned reasonCode)
{
  grj.m_rejectReason.SetTag(reasonCode);
}


H323GatekeeperRequest::Response H323GatekeeperGRQ::OnHandlePDU()
{
  return rasChannel.OnDiscovery(*this);
}


H323GatekeeperRRQ::H323GatekeeperRRQ(H323GatekeeperListener & rasChannel,
                                     const H323RasPDU & pdu)
  : H323GatekeeperRequest(rasChannel, pdu),
    rrq((H225_RegistrationRequest &)request->GetChoice().GetObject()),
    rcf(((H323RasPDU &)confirm->GetPDU()).BuildRegistrationConfirm(rrq.m_requestSeqNum)),
    rrj(((H323RasPDU &)reject->GetPDU()).BuildRegistrationReject(rrq.m_requestSeqNum))
{
  H323EndPoint & ep = rasChannel.GetEndPoint();
  PIPSocket::Address senderIP;
  PBoolean senderIsIP = replyAddresses[0].GetIpAddress(senderIP);
  PBoolean senderIsLocal = senderIsIP && ep.IsLocalAddress(senderIP);

  H323TransportAddressArray unsuitable;

  PBoolean first = TRUE;
  PINDEX i;
  for (i = 0; i < rrq.m_rasAddress.GetSize(); i++) {
    if (rasChannel.GetTransport().IsCompatibleTransport(rrq.m_rasAddress[i])) {
      // Check the return address, if not the same side of a NAT firewall, then
      // just use the physical reply address already set by ancestor.
      H323TransportAddress rasAddress = rrq.m_rasAddress[i];
      PIPSocket::Address rasIP;
      if (!senderIsIP ||
          !rasAddress.GetIpAddress(rasIP) ||
          senderIsLocal == ep.IsLocalAddress(rasIP)) {
        PTRACE(4, "RAS\tFound suitable RAS address in RRQ: " << rasAddress);
        if (first)
          replyAddresses[0] = rasAddress;
        else
          replyAddresses.AppendAddress(rasAddress);
        first = FALSE;
      }
      else
        unsuitable.AppendAddress(rasAddress);
    }
  }

  isBehindNAT = first;
  PTRACE_IF(3, isBehindNAT,
            "RAS\tCould not find suitable RAS address in RRQ, using " << replyAddresses[0]);

  for (i = 0; i < unsuitable.GetSize(); i++)
    replyAddresses.AppendAddress(unsuitable[i]);
}


#if PTRACING
const char * H323GatekeeperRRQ::GetName() const
{
  return "RRQ";
}
#endif


PString H323GatekeeperRRQ::GetGatekeeperIdentifier() const
{
  if (rrq.HasOptionalField(H225_RegistrationRequest::e_gatekeeperIdentifier))
    return rrq.m_gatekeeperIdentifier;

  return PString::Empty();
}


unsigned H323GatekeeperRRQ::GetGatekeeperRejectTag() const
{
  return H225_RegistrationRejectReason::e_undefinedReason;
}


PString H323GatekeeperRRQ::GetEndpointIdentifier() const
{
  return rrq.m_endpointIdentifier;
}


unsigned H323GatekeeperRRQ::GetRegisteredEndPointRejectTag() const
{
  return H225_RegistrationRejectReason::e_fullRegistrationRequired;
}


H235Authenticator::ValidationResult H323GatekeeperRRQ::ValidatePDU() const
{
  return request->Validate(rrq.m_tokens, H225_RegistrationRequest::e_tokens,
                           rrq.m_cryptoTokens, H225_RegistrationRequest::e_cryptoTokens);
}


unsigned H323GatekeeperRRQ::GetSecurityRejectTag() const
{
  return H225_RegistrationRejectReason::e_securityDenial;
}


void H323GatekeeperRRQ::SetRejectReason(unsigned reasonCode)
{
  rrj.m_rejectReason.SetTag(reasonCode);
}


H323GatekeeperRequest::Response H323GatekeeperRRQ::OnHandlePDU()
{
  H323GatekeeperRequest::Response response = rasChannel.OnRegistration(*this);
  if (response != Reject)
    return response;

  H323GatekeeperServer & server = rasChannel.GetGatekeeper();
  server.mutex.Wait();
  server.rejectedRegistrations++;
  server.mutex.Signal();

  return Reject;
}


H323GatekeeperURQ::H323GatekeeperURQ(H323GatekeeperListener & rasChannel,
                                     const H323RasPDU & pdu)
  : H323GatekeeperRequest(rasChannel, pdu),
    urq((H225_UnregistrationRequest &)request->GetChoice().GetObject()),
    ucf(((H323RasPDU &)confirm->GetPDU()).BuildUnregistrationConfirm(urq.m_requestSeqNum)),
    urj(((H323RasPDU &)reject->GetPDU()).BuildUnregistrationReject(urq.m_requestSeqNum))
{
}


#if PTRACING
const char * H323GatekeeperURQ::GetName() const
{
  return "URQ";
}
#endif


PString H323GatekeeperURQ::GetGatekeeperIdentifier() const
{
  if (urq.HasOptionalField(H225_UnregistrationRequest::e_gatekeeperIdentifier))
    return urq.m_gatekeeperIdentifier;

  return PString::Empty();
}


unsigned H323GatekeeperURQ::GetGatekeeperRejectTag() const
{
  return H225_GatekeeperRejectReason::e_terminalExcluded;
}


PString H323GatekeeperURQ::GetEndpointIdentifier() const
{
  return urq.m_endpointIdentifier;
}


unsigned H323GatekeeperURQ::GetRegisteredEndPointRejectTag() const
{
  return H225_UnregRejectReason::e_notCurrentlyRegistered;
}


H235Authenticator::ValidationResult H323GatekeeperURQ::ValidatePDU() const
{
  return request->Validate(urq.m_tokens, H225_UnregistrationRequest::e_tokens,
                           urq.m_cryptoTokens, H225_UnregistrationRequest::e_cryptoTokens);
}


unsigned H323GatekeeperURQ::GetSecurityRejectTag() const
{
  return H225_UnregRejectReason::e_securityDenial;
}


void H323GatekeeperURQ::SetRejectReason(unsigned reasonCode)
{
  urj.m_rejectReason.SetTag(reasonCode);
}


H323GatekeeperRequest::Response H323GatekeeperURQ::OnHandlePDU()
{
  return rasChannel.OnUnregistration(*this);
}


H323GatekeeperARQ::H323GatekeeperARQ(H323GatekeeperListener & rasChannel,
                                     const H323RasPDU & pdu)
  : H323GatekeeperRequest(rasChannel, pdu),
    arq((H225_AdmissionRequest &)request->GetChoice().GetObject()),
    acf(((H323RasPDU &)confirm->GetPDU()).BuildAdmissionConfirm(arq.m_requestSeqNum)),
    arj(((H323RasPDU &)reject->GetPDU()).BuildAdmissionReject(arq.m_requestSeqNum))
{
}


#if PTRACING
const char * H323GatekeeperARQ::GetName() const
{
  return "ARQ";
}
#endif


PString H323GatekeeperARQ::GetGatekeeperIdentifier() const
{
  if (arq.HasOptionalField(H225_AdmissionRequest::e_gatekeeperIdentifier))
    return arq.m_gatekeeperIdentifier;

  return PString::Empty();
}


unsigned H323GatekeeperARQ::GetGatekeeperRejectTag() const
{
  return H225_AdmissionRejectReason::e_undefinedReason;
}


PString H323GatekeeperARQ::GetEndpointIdentifier() const
{
  return arq.m_endpointIdentifier;
}


unsigned H323GatekeeperARQ::GetRegisteredEndPointRejectTag() const
{
  return H225_AdmissionRejectReason::e_callerNotRegistered;
}


H235Authenticator::ValidationResult H323GatekeeperARQ::ValidatePDU() const
{
  return request->Validate(arq.m_tokens, H225_AdmissionRequest::e_tokens,
                           arq.m_cryptoTokens, H225_AdmissionRequest::e_cryptoTokens);
}


unsigned H323GatekeeperARQ::GetSecurityRejectTag() const
{
  return H225_AdmissionRejectReason::e_securityDenial;
}


void H323GatekeeperARQ::SetRejectReason(unsigned reasonCode)
{
  arj.m_rejectReason.SetTag(reasonCode);
}


H323GatekeeperRequest::Response H323GatekeeperARQ::OnHandlePDU()
{
  H323GatekeeperRequest::Response response = rasChannel.OnAdmission(*this);
  if (response != Reject)
    return response;

  H323GatekeeperServer & server = rasChannel.GetGatekeeper();
  PSafePtr<H323GatekeeperCall> call = server.FindCall(arq.m_callIdentifier.m_guid, arq.m_answerCall);
  if (call != NULL)
    server.RemoveCall(call);

  server.mutex.Wait();
  server.rejectedCalls++;
  server.mutex.Signal();

  return Reject;
}


H323GatekeeperDRQ::H323GatekeeperDRQ(H323GatekeeperListener & rasChannel,
                                     const H323RasPDU & pdu)
  : H323GatekeeperRequest(rasChannel, pdu),
    drq((H225_DisengageRequest &)request->GetChoice().GetObject()),
    dcf(((H323RasPDU &)confirm->GetPDU()).BuildDisengageConfirm(drq.m_requestSeqNum)),
    drj(((H323RasPDU &)reject->GetPDU()).BuildDisengageReject(drq.m_requestSeqNum))
{
}


#if PTRACING
const char * H323GatekeeperDRQ::GetName() const
{
  return "DRQ";
}
#endif


PString H323GatekeeperDRQ::GetGatekeeperIdentifier() const
{
  if (drq.HasOptionalField(H225_DisengageRequest::e_gatekeeperIdentifier))
    return drq.m_gatekeeperIdentifier;

  return PString::Empty();
}


unsigned H323GatekeeperDRQ::GetGatekeeperRejectTag() const
{
  return H225_DisengageRejectReason::e_notRegistered;
}


PString H323GatekeeperDRQ::GetEndpointIdentifier() const
{
  return drq.m_endpointIdentifier;
}


unsigned H323GatekeeperDRQ::GetRegisteredEndPointRejectTag() const
{
  return H225_DisengageRejectReason::e_notRegistered;
}


H235Authenticator::ValidationResult H323GatekeeperDRQ::ValidatePDU() const
{
  return request->Validate(drq.m_tokens, H225_DisengageRequest::e_tokens,
                           drq.m_cryptoTokens, H225_DisengageRequest::e_cryptoTokens);
}


unsigned H323GatekeeperDRQ::GetSecurityRejectTag() const
{
  return H225_DisengageRejectReason::e_securityDenial;
}


void H323GatekeeperDRQ::SetRejectReason(unsigned reasonCode)
{
  drj.m_rejectReason.SetTag(reasonCode);
}


H323GatekeeperRequest::Response H323GatekeeperDRQ::OnHandlePDU()
{
  return rasChannel.OnDisengage(*this);
}


H323GatekeeperBRQ::H323GatekeeperBRQ(H323GatekeeperListener & rasChannel,
                                     const H323RasPDU & pdu)
  : H323GatekeeperRequest(rasChannel, pdu),
    brq((H225_BandwidthRequest &)request->GetChoice().GetObject()),
    bcf(((H323RasPDU &)confirm->GetPDU()).BuildBandwidthConfirm(brq.m_requestSeqNum)),
    brj(((H323RasPDU &)reject->GetPDU()).BuildBandwidthReject(brq.m_requestSeqNum))
{
}


#if PTRACING
const char * H323GatekeeperBRQ::GetName() const
{
  return "BRQ";
}
#endif


PString H323GatekeeperBRQ::GetGatekeeperIdentifier() const
{
  if (brq.HasOptionalField(H225_BandwidthRequest::e_gatekeeperIdentifier))
    return brq.m_gatekeeperIdentifier;

  return PString::Empty();
}


unsigned H323GatekeeperBRQ::GetGatekeeperRejectTag() const
{
  return H225_BandRejectReason::e_undefinedReason;
}


PString H323GatekeeperBRQ::GetEndpointIdentifier() const
{
  return brq.m_endpointIdentifier;
}


unsigned H323GatekeeperBRQ::GetRegisteredEndPointRejectTag() const
{
  return H225_BandRejectReason::e_notBound;
}


H235Authenticator::ValidationResult H323GatekeeperBRQ::ValidatePDU() const
{
  return request->Validate(brq.m_tokens, H225_BandwidthRequest::e_tokens,
                           brq.m_cryptoTokens, H225_BandwidthRequest::e_cryptoTokens);
}


unsigned H323GatekeeperBRQ::GetSecurityRejectTag() const
{
  return H225_BandRejectReason::e_securityDenial;
}


void H323GatekeeperBRQ::SetRejectReason(unsigned reasonCode)
{
  brj.m_rejectReason.SetTag(reasonCode);
}


H323GatekeeperRequest::Response H323GatekeeperBRQ::OnHandlePDU()
{
  return rasChannel.OnBandwidth(*this);
}


H323GatekeeperLRQ::H323GatekeeperLRQ(H323GatekeeperListener & rasChannel,
                                     const H323RasPDU & pdu)
  : H323GatekeeperRequest(rasChannel, pdu),
    lrq((H225_LocationRequest &)request->GetChoice().GetObject()),
    lcf(((H323RasPDU &)confirm->GetPDU()).BuildLocationConfirm(lrq.m_requestSeqNum)),
    lrj(((H323RasPDU &)reject->GetPDU()).BuildLocationReject(lrq.m_requestSeqNum))
{
  if (rasChannel.GetTransport().IsCompatibleTransport(lrq.m_replyAddress))
    replyAddresses[0] = lrq.m_replyAddress;
}


#if PTRACING
const char * H323GatekeeperLRQ::GetName() const
{
  return "LRQ";
}
#endif


PString H323GatekeeperLRQ::GetGatekeeperIdentifier() const
{
  if (lrq.HasOptionalField(H225_LocationRequest::e_gatekeeperIdentifier))
    return lrq.m_gatekeeperIdentifier;

  return PString::Empty();
}


unsigned H323GatekeeperLRQ::GetGatekeeperRejectTag() const
{
  return H225_LocationRejectReason::e_undefinedReason;
}


PString H323GatekeeperLRQ::GetEndpointIdentifier() const
{
  return lrq.m_endpointIdentifier;
}


unsigned H323GatekeeperLRQ::GetRegisteredEndPointRejectTag() const
{
  return H225_LocationRejectReason::e_notRegistered;
}


H235Authenticator::ValidationResult H323GatekeeperLRQ::ValidatePDU() const
{
  return request->Validate(lrq.m_tokens, H225_LocationRequest::e_tokens,
                           lrq.m_cryptoTokens, H225_LocationRequest::e_cryptoTokens);
}


unsigned H323GatekeeperLRQ::GetSecurityRejectTag() const
{
  return H225_LocationRejectReason::e_securityDenial;
}


void H323GatekeeperLRQ::SetRejectReason(unsigned reasonCode)
{
  lrj.m_rejectReason.SetTag(reasonCode);
}


H323GatekeeperRequest::Response H323GatekeeperLRQ::OnHandlePDU()
{
  return rasChannel.OnLocation(*this);
}


H323GatekeeperIRR::H323GatekeeperIRR(H323GatekeeperListener & rasChannel,
                                     const H323RasPDU & pdu)
  : H323GatekeeperRequest(rasChannel, pdu),
    irr((H225_InfoRequestResponse &)request->GetChoice().GetObject()),
    iack(((H323RasPDU &)confirm->GetPDU()).BuildInfoRequestAck(irr.m_requestSeqNum)),
    inak(((H323RasPDU &)reject->GetPDU()).BuildInfoRequestNak(irr.m_requestSeqNum))
{
}


#if PTRACING
const char * H323GatekeeperIRR::GetName() const
{
  return "IRR";
}
#endif


PString H323GatekeeperIRR::GetGatekeeperIdentifier() const
{
  return PString::Empty();
}


unsigned H323GatekeeperIRR::GetGatekeeperRejectTag() const
{
  return H225_GatekeeperRejectReason::e_terminalExcluded;
}


PString H323GatekeeperIRR::GetEndpointIdentifier() const
{
  return irr.m_endpointIdentifier;
}


unsigned H323GatekeeperIRR::GetRegisteredEndPointRejectTag() const
{
  return H225_InfoRequestNakReason::e_notRegistered;
}


H235Authenticator::ValidationResult H323GatekeeperIRR::ValidatePDU() const
{
  return request->Validate(irr.m_tokens, H225_InfoRequestResponse::e_tokens,
                           irr.m_cryptoTokens, H225_InfoRequestResponse::e_cryptoTokens);
}


unsigned H323GatekeeperIRR::GetSecurityRejectTag() const
{
  return H225_InfoRequestNakReason::e_securityDenial;
}


void H323GatekeeperIRR::SetRejectReason(unsigned reasonCode)
{
  inak.m_nakReason.SetTag(reasonCode);
}


H323GatekeeperRequest::Response H323GatekeeperIRR::OnHandlePDU()
{
  return rasChannel.OnInfoResponse(*this);
}


/////////////////////////////////////////////////////////////////////////////

H323GatekeeperCall::H323GatekeeperCall(H323GatekeeperServer& gk,
                                       const OpalGloballyUniqueID & id,
                                       Direction dir)
  : gatekeeper(gk),
    callIdentifier(id),
    conferenceIdentifier(NULL),
    alertingTime(0),
    connectedTime(0),
    callEndTime(0)
{
  endpoint = NULL;
  rasChannel = NULL;
  direction = dir;
  callReference = 0;

  drqReceived = FALSE;
  callEndReason = H323Connection::NumCallEndReasons;

  bandwidthUsed = 0;
  infoResponseRate = gatekeeper.GetInfoResponseRate();
}


H323GatekeeperCall::~H323GatekeeperCall()
{
  SetBandwidthUsed(0);
}


PObject::Comparison H323GatekeeperCall::Compare(const PObject & obj) const
{
  // Do not have to lock the object as these fields should never change for
  // life of the object

  PAssert(PIsDescendant(&obj, H323GatekeeperCall), PInvalidCast);
  const H323GatekeeperCall & other = (const H323GatekeeperCall &)obj;
  Comparison result = callIdentifier.Compare(other.callIdentifier);
  if (result != EqualTo)
    return result;

  if (direction == UnknownDirection || other.direction == UnknownDirection)
    return EqualTo;

  if (direction > other.direction)
    return GreaterThan;
  if (direction < other.direction)
    return LessThan;
  return EqualTo;
}


void H323GatekeeperCall::PrintOn(ostream & strm) const
{
  // Do not have to lock the object as these fields should never change for
  // life of the object

  strm << callIdentifier;

  switch (direction) {
    case AnsweringCall :
      strm << AnswerCallStr;
      break;
    case OriginatingCall :
      strm << OriginateCallStr;
      break;
    default :
      break;
  }
}


H323GatekeeperRequest::Response H323GatekeeperCall::OnAdmission(H323GatekeeperARQ & info)
{
  PTRACE_BLOCK("H323GatekeeperCall::OnAdmission");

  if (endpoint != NULL) {
    info.SetRejectReason(H225_AdmissionRejectReason::e_resourceUnavailable);
    PTRACE(2, "RAS\tARQ rejected, multiple use of same call id.");
    return H323GatekeeperRequest::Reject;
  }

  PINDEX i;

  if (!LockReadWrite()) {
    PTRACE(1, "RAS\tARQ rejected, lock failed on call " << *this);
    return H323GatekeeperRequest::Reject;
  }

  PTRACE(3, "RAS\tProcessing OnAdmission for " << *this);

  endpoint = info.endpoint;
  rasChannel = &info.GetRasChannel();

  callReference = info.arq.m_callReferenceValue;
  conferenceIdentifier = info.arq.m_conferenceID;

  for (i = 0; i < info.arq.m_srcInfo.GetSize(); i++) {
    PString alias = H323GetAliasAddressString(info.arq.m_srcInfo[i]);
    if (srcAliases.GetValuesIndex(alias) == P_MAX_INDEX)
      srcAliases += alias;
  }

  srcNumber = H323GetAliasAddressE164(info.arq.m_srcInfo);

  if (!endpoint->IsBehindNAT() &&
          info.arq.HasOptionalField(H225_AdmissionRequest::e_srcCallSignalAddress))
    srcHost = info.arq.m_srcCallSignalAddress;
  else
    srcHost = info.GetReplyAddress();

  if (info.arq.HasOptionalField(H225_AdmissionRequest::e_destinationInfo)) {
    for (i = 0; i < info.arq.m_destinationInfo.GetSize(); i++) {
      PString alias = H323GetAliasAddressString(info.arq.m_destinationInfo[i]);
      if (dstAliases.GetValuesIndex(alias) == P_MAX_INDEX)
        dstAliases += alias;
    }

    dstNumber = H323GetAliasAddressE164(info.arq.m_destinationInfo);
  }

  if (info.arq.HasOptionalField(H225_AdmissionRequest::e_destCallSignalAddress))
    dstHost = info.arq.m_destCallSignalAddress;

  UnlockReadWrite();

  PBoolean isGKRouted = gatekeeper.IsGatekeeperRouted();

  if (direction == AnsweringCall) {

    // See if our policies allow the call
    PBoolean denied = TRUE;
    for (i = 0; i < info.arq.m_srcInfo.GetSize(); i++) {
      if (gatekeeper.CheckAliasAddressPolicy(*endpoint, info.arq, info.arq.m_srcInfo[i])) {
        denied = FALSE;
        break;
      }
    }

    if (info.arq.HasOptionalField(H225_AdmissionRequest::e_srcCallSignalAddress)) {
      if (gatekeeper.CheckSignalAddressPolicy(*endpoint, info.arq, info.arq.m_srcCallSignalAddress))
        denied = FALSE;
    }

    if (denied) {
      info.SetRejectReason(H225_AdmissionRejectReason::e_securityDenial);
      PTRACE(2, "RAS\tARQ rejected, not allowed to answer call");
      return H323GatekeeperRequest::Reject;
    }
  }

  else {
    PSafePtr<H323RegisteredEndPoint> destEP;

    PBoolean denied = TRUE;
    PBoolean noAddress = TRUE;

    // if no alias, convert signalling address to an alias
    if (!info.arq.HasOptionalField(H225_AdmissionRequest::e_destinationInfo) && info.arq.HasOptionalField(H225_AdmissionRequest::e_destCallSignalAddress)) {

      H323TransportAddress origTransAddr(info.arq.m_destCallSignalAddress);
      H225_AliasAddress transportAlias;
      H323SetAliasAddress(origTransAddr, transportAlias);

      if (gatekeeper.CheckAliasAddressPolicy(*endpoint, info.arq, transportAlias)) {
        denied = FALSE;
        H323TransportAddress destAddr;
        if (TranslateAliasAddress(transportAlias,
                                  info.acf.m_destinationInfo,
                                  destAddr,
                                  isGKRouted)) {
          if (info.acf.m_destinationInfo.GetSize())
            info.acf.IncludeOptionalField(H225_AdmissionConfirm::e_destinationInfo);

          destEP = gatekeeper.FindEndPointByAliasAddress(transportAlias);
          if (!LockReadWrite()) {
            PTRACE(1, "RAS\tARQ rejected, lock failed on call " << *this);
            return H323GatekeeperRequest::Reject;
          }
          dstHost = destAddr;
          UnlockReadWrite();
          noAddress = FALSE;
        }
      }
    }

    // if alias(es) specified, check them
    else {

      for (i = 0; i < info.arq.m_destinationInfo.GetSize(); i++) {
        if (gatekeeper.CheckAliasAddressPolicy(*endpoint, info.arq, info.arq.m_destinationInfo[i])) {
          denied = FALSE;
          H323TransportAddress destAddr;
          if (TranslateAliasAddress(info.arq.m_destinationInfo[i],
                                    info.acf.m_destinationInfo,
                                    destAddr,
                                    isGKRouted)) {
            if (info.acf.m_destinationInfo.GetSize())
              info.acf.IncludeOptionalField(H225_AdmissionConfirm::e_destinationInfo);

            destEP = gatekeeper.FindEndPointByAliasAddress(info.arq.m_destinationInfo[i]);
            if (!LockReadWrite()) {
              PTRACE(1, "RAS\tARQ rejected, lock failed on call " << *this);
              return H323GatekeeperRequest::Reject;
            }
            dstHost = destAddr;
            UnlockReadWrite();
            noAddress = FALSE;
            break;
          }
        }
      }

      if (denied) {
        info.SetRejectReason(H225_AdmissionRejectReason::e_securityDenial);
        PTRACE(2, "RAS\tARQ rejected, not allowed to make call");
        return H323GatekeeperRequest::Reject;
      }

      if (noAddress) {
        info.SetRejectReason(H225_AdmissionRejectReason::e_calledPartyNotRegistered);
        PTRACE(2, "RAS\tARQ rejected, destination alias not registered");
        return H323GatekeeperRequest::Reject;
      }

      if (destEP != NULL) {
        destEP.SetSafetyMode(PSafeReadOnly);
        if (!LockReadWrite()) {
          PTRACE(1, "RAS\tARQ rejected, lock failed on call " << *this);
          return H323GatekeeperRequest::Reject;
        }
        dstAliases.RemoveAll();
        dstNumber = PString::Empty();
        for (i = 0; i < destEP->GetAliasCount(); i++) {
          PString alias = destEP->GetAlias(i);
          dstAliases += alias;
          if (strspn(alias, "0123456789*#") == strlen(alias))
            dstNumber = alias;
        }
        UnlockReadWrite();
        destEP.SetSafetyMode(PSafeReference);
      }

      // Has provided an alias and signal address, see if agree
      if (destEP != NULL && info.arq.HasOptionalField(H225_AdmissionRequest::e_destCallSignalAddress)) {
        // Note nested if's used because some compilers (Tornado 2) cannot handle it
        if (gatekeeper.FindEndPointBySignalAddress(info.arq.m_destCallSignalAddress) != destEP) {
          info.SetRejectReason(H225_AdmissionRejectReason::e_aliasesInconsistent);
          PTRACE(2, "RAS\tARQ rejected, destination address not for specified alias");
          return H323GatekeeperRequest::Reject;
        }
      }
    }

    // Have no address from anywhere
    if (dstHost.IsEmpty()) {
      info.SetRejectReason(H225_AdmissionRejectReason::e_incompleteAddress);
      PTRACE(2, "RAS\tARQ rejected, must have destination address or alias");
      return H323GatekeeperRequest::Reject;
    }

    // Now see if security policy allows connection to that address
    if (!gatekeeper.CheckSignalAddressPolicy(*endpoint, info.arq, dstHost)) {
      info.SetRejectReason(H225_AdmissionRejectReason::e_securityDenial);
      PTRACE(2, "RAS\tARQ rejected, not allowed to make call");
      return H323GatekeeperRequest::Reject;
    }
  }

  // See if have bandwidth
  unsigned requestedBandwidth = info.arq.m_bandWidth;
  if (requestedBandwidth == 0)
    requestedBandwidth = gatekeeper.GetDefaultBandwidth();

  unsigned bandwidthAllocated = gatekeeper.AllocateBandwidth(requestedBandwidth);
  if (bandwidthAllocated == 0) {
    info.SetRejectReason(H225_AdmissionRejectReason::e_requestDenied);
    PTRACE(2, "RAS\tARQ rejected, not enough bandwidth");
    return H323GatekeeperRequest::Reject;
  }

  bandwidthUsed = bandwidthAllocated;  // Do we need to lock for atomic assignment?
  info.acf.m_bandWidth = bandwidthUsed;

  // Set the rate for getting unsolicited IRR's
  if (infoResponseRate > 0 && endpoint->GetProtocolVersion() > 2) {
    info.acf.IncludeOptionalField(H225_AdmissionConfirm::e_irrFrequency);
    info.acf.m_irrFrequency = infoResponseRate;
  }
  info.acf.m_willRespondToIRR = TRUE;

  // Set the destination to fixed value of gk routed
  if (isGKRouted)
    info.acf.m_callModel.SetTag(H225_CallModel::e_gatekeeperRouted);

  dstHost.SetPDU(info.acf.m_destCallSignalAddress);

  // If ep can do UUIE's then use that to expidite getting some statistics
  if (info.arq.m_willSupplyUUIEs) {
    info.acf.m_uuiesRequested.m_alerting = TRUE;
    info.acf.m_uuiesRequested.m_connect = TRUE;
  }

  return H323GatekeeperRequest::Confirm;
}


PBoolean H323GatekeeperCall::Disengage(int reason)
{
  if (!LockReadWrite()) {
    PTRACE(1, "RAS\tDRQ not sent, lock failed on call " << *this);
    return FALSE;
  }

  if (drqReceived) {
    UnlockReadWrite();
    PTRACE(1, "RAS\tAlready disengaged call " << *this);
    return FALSE;
  }

  drqReceived = TRUE;

  PTRACE(2, "RAS\tDisengage of call " << *this);

  UnlockReadWrite();

  if (reason == -1)
    reason = H225_DisengageReason::e_forcedDrop;

  PBoolean ok;
  // Send DRQ to endpoint(s)
  if (rasChannel != NULL)
    ok = rasChannel->DisengageRequest(*this, reason);
  else {
    // Can't do DRQ as have no RAS channel to use (probably logic error)
    PAssertAlways("Tried to disengage call we did not receive ARQ for!");
    ok = FALSE;
  }

  gatekeeper.RemoveCall(this);

  return ok;
}


H323GatekeeperRequest::Response H323GatekeeperCall::OnDisengage(H323GatekeeperDRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperCall::OnDisengage");

  if (!LockReadWrite()) {
    PTRACE(1, "RAS\tDRQ rejected, lock failed on call " << *this);
    return H323GatekeeperRequest::Reject;
  }

  if (drqReceived) {
    UnlockReadWrite();
    info.SetRejectReason(H225_DisengageRejectReason::e_requestToDropOther);
    PTRACE(2, "RAS\tDRQ rejected, already disengaged call " << *this);
    return H323GatekeeperRequest::Reject;
  }

  drqReceived = TRUE;

  if (info.drq.HasOptionalField(H225_DisengageRequest::e_usageInformation))
    SetUsageInfo(info.drq.m_usageInformation);

  if (info.drq.HasOptionalField(H225_DisengageRequest::e_terminationCause)) {
    if (info.drq.m_terminationCause.GetTag() == H225_CallTerminationCause::e_releaseCompleteReason) {
      H225_ReleaseCompleteReason & reason = info.drq.m_terminationCause;
      callEndReason = H323TranslateToCallEndReason(Q931::ErrorInCauseIE, reason);
    }
    else {
      PASN_OctetString & cause = info.drq.m_terminationCause;
      H225_ReleaseCompleteReason dummy;
      callEndReason = H323TranslateToCallEndReason((Q931::CauseValues)(cause[1]&0x7f), dummy);
    }
  }

  UnlockReadWrite();

  return H323GatekeeperRequest::Confirm;
}


H323GatekeeperRequest::Response H323GatekeeperCall::OnBandwidth(H323GatekeeperBRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperCall::OnBandwidth");

  if (endpoint != info.endpoint) {
    info.SetRejectReason(H225_BandRejectReason::e_invalidPermission);
    PTRACE(2, "RAS\tBRQ rejected, call is not owned by endpoint");
    return H323GatekeeperRequest::Reject;
  }

  bandwidthUsed = gatekeeper.AllocateBandwidth(info.brq.m_bandWidth, bandwidthUsed);
  if (bandwidthUsed < info.brq.m_bandWidth) {
    info.SetRejectReason(H225_BandRejectReason::e_insufficientResources);
    info.brj.m_allowedBandWidth = bandwidthUsed;
    PTRACE(2, "RAS\tBRQ rejected, no bandwidth");
    return H323GatekeeperRequest::Reject;
  }

  info.bcf.m_bandWidth = bandwidthUsed;

  if (info.brq.HasOptionalField(H225_BandwidthRequest::e_usageInformation))
    SetUsageInfo(info.brq.m_usageInformation);

  return H323GatekeeperRequest::Confirm;
}


H323GatekeeperRequest::Response H323GatekeeperCall::OnInfoResponse(H323GatekeeperIRR &,
                                                                   H225_InfoRequestResponse_perCallInfo_subtype & info)
{
  PTRACE_BLOCK("H323GatekeeperCall::OnInfoResponse");

  PTRACE(2, "RAS\tIRR received for call " << *this);

  if (!LockReadWrite()) {
    PTRACE(1, "RAS\tIRR rejected, lock failed on call " << *this);
    return H323GatekeeperRequest::Reject;
  }

  PTime now;
  lastInfoResponse = now;

  // Detect if have Cisco non-standard version of connect time indication.
  if (!connectedTime.IsValid() &&
      info.HasOptionalField(H225_InfoRequestResponse_perCallInfo_subtype::e_nonStandardData) &&
      info.m_nonStandardData.m_nonStandardIdentifier.GetTag() == H225_NonStandardIdentifier::e_h221NonStandard) {
    H225_H221NonStandard & id = info.m_nonStandardData.m_nonStandardIdentifier;
    if (id.m_t35CountryCode == 181 && id.m_t35Extension == 0 && id.m_manufacturerCode == 18 && // Cisco
        info.m_nonStandardData.m_data.GetSize() == 5 && info.m_nonStandardData.m_data[0] == 0x70) {
      PTime theConnectedTime((info.m_nonStandardData.m_data[1] << 24)|
                            (info.m_nonStandardData.m_data[2] << 16)|
                            (info.m_nonStandardData.m_data[3] << 8 )|
                             info.m_nonStandardData.m_data[4]        );
      if (theConnectedTime > now || theConnectedTime < callStartTime) {
        connectedTime = now;
        OnConnected();
      }
      else {
        connectedTime = theConnectedTime;
        OnConnected();
      }
    }
  }

  SetUsageInfo(info.m_usageInformation);

  UnlockReadWrite();

  return H323GatekeeperRequest::Confirm;
}


void H323GatekeeperCall::OnAlerting()
{
}


void H323GatekeeperCall::OnConnected()
{
}


static PBoolean CheckTimeSince(PTime & lastTime, unsigned threshold)
{
  if (threshold == 0)
    return TRUE;

  PTime now;
  PTimeInterval delta = now - lastTime;
  return delta.GetSeconds() < (int)(threshold+10);
}


PBoolean H323GatekeeperCall::OnHeartbeat()
{
  if (!LockReadOnly()) {
    PTRACE(1, "RAS\tOnHeartbeat lock failed on call " << *this);
    return TRUE;
  }

  if (CheckTimeSince(lastInfoResponse, infoResponseRate)) {
    UnlockReadOnly();
    return TRUE;
  }

  // Can't do IRQ as have no RAS channel to use (probably logic error)
  if (rasChannel == NULL) {
    UnlockReadOnly();
    PAssertAlways("Timeout on heartbeat for call we did not receive ARQ for!");
    return FALSE;
  }

  UnlockReadOnly();

  // Do IRQ and wait for IRR on call
  PTRACE(2, "RAS\tTimeout on heartbeat, doing IRQ for call "<< *this);
  if (!rasChannel->InfoRequest(*endpoint, this))
    return FALSE;

  if (!LockReadOnly()) {
    PTRACE(1, "RAS\tOnHeartbeat lock failed on call " << *this);
    return TRUE;
  }

  // Return TRUE if got a resonse, ie client does not do unsolicited IRR's
  // otherwise did not get a response from client so return FALSE and
  // (probably) disengage the call.
  PBoolean response = CheckTimeSince(lastInfoResponse, infoResponseRate); 
  
  UnlockReadOnly();

  return response;
}

#ifdef H323_H248

PString H323GatekeeperCall::GetCallCreditAmount() const
{
  if (endpoint != NULL)
    return endpoint->GetCallCreditAmount();
  
  return PString::Empty();
}


PBoolean H323GatekeeperCall::GetCallCreditMode() const
{
  return endpoint != NULL ? endpoint->GetCallCreditMode() : FALSE;
}


unsigned H323GatekeeperCall::GetDurationLimit() const
{
  return 0;
}

PBoolean H323GatekeeperCall::SendCallCreditServiceControl()
{
  PString amount;
  if (endpoint->CanDisplayAmountString())
    amount = GetCallCreditAmount();

  unsigned durationLimit = 0;
  if (endpoint->CanEnforceDurationLimit())
    durationLimit = GetDurationLimit();

  if (amount.IsEmpty() && durationLimit == 0)
    return FALSE;

  H323CallCreditServiceControl credit(amount, GetCallCreditMode(), durationLimit);
  return SendServiceControlSession(credit);
}


PBoolean H323GatekeeperCall::AddCallCreditServiceControl(H225_ArrayOf_ServiceControlSession & serviceControl) const
{
  PString amount;
  if (endpoint->CanDisplayAmountString())
    amount = GetCallCreditAmount();

  unsigned durationLimit = 0;
  if (endpoint->CanEnforceDurationLimit())
    durationLimit = GetDurationLimit();

  if (amount.IsEmpty() && durationLimit == 0)
    return FALSE;

  H323CallCreditServiceControl credit(amount, GetCallCreditMode(), durationLimit);
  return endpoint->AddServiceControlSession(credit, serviceControl);
}


PBoolean H323GatekeeperCall::SendServiceControlSession(const H323ServiceControlSession & session)
{
  // Send SCI to endpoint(s)
  if (rasChannel == NULL || endpoint == NULL) {
    // Can't do SCI as have no RAS channel to use (probably logic error)
    PAssertAlways("Tried to do SCI to call we did not receive ARQ for!");
    return FALSE;
  }

  return rasChannel->ServiceControlIndication(*endpoint, session, this);
}

#endif // H323_H248


PBoolean H323GatekeeperCall::SetBandwidthUsed(unsigned newBandwidth)
{
  if (newBandwidth == bandwidthUsed)
    return TRUE;

  bandwidthUsed = gatekeeper.AllocateBandwidth(newBandwidth, bandwidthUsed);
  return bandwidthUsed == newBandwidth;
}


static PString MakeAddress(const PString & number,
                           const PStringArray aliases,
                           const H323TransportAddress & host)
{
  PStringStream addr;

  if (!number)
    addr << number;
  else if (!aliases.IsEmpty())
    addr << aliases[0];

  if (!host) {
    if (!addr.IsEmpty())
      addr << '@';
    addr << host;
  }

  return addr;
}


PString H323GatekeeperCall::GetSourceAddress() const
{
  if (!LockReadOnly()) {
    PTRACE(1, "RAS\tGetSourceAddress lock failed on call " << *this);
    return PString::Empty();
  }
  PString addr = MakeAddress(srcNumber, srcAliases, srcHost);
  UnlockReadOnly();
  return addr;
}


PString H323GatekeeperCall::GetDestinationAddress() const
{
  if (!LockReadOnly()) {
    PTRACE(1, "RAS\tGetDestinationAddress lock failed on call " << *this);
    return PString::Empty();
  }
  PString addr = MakeAddress(dstNumber, dstAliases, dstHost);
  UnlockReadOnly();
  return addr;
}


void H323GatekeeperCall::SetUsageInfo(const H225_RasUsageInformation & usage)
{
  PTime now;

  if (!alertingTime.IsValid() &&
       usage.HasOptionalField(H225_RasUsageInformation::e_alertingTime)) {
    PTime theAlertingTime((unsigned)usage.m_alertingTime);
    if (theAlertingTime > now || theAlertingTime < callStartTime) {
      alertingTime = now;
      OnAlerting();
    }
    else if (theAlertingTime > callStartTime) {
      alertingTime = theAlertingTime;
      OnAlerting();
    }
  }

  if (!connectedTime.IsValid() &&
       usage.HasOptionalField(H225_RasUsageInformation::e_connectTime)) {
    PTime theConnectedTime((unsigned)usage.m_connectTime);
    if (theConnectedTime > now || theConnectedTime < callStartTime) {
      connectedTime = now;
      OnConnected();
    }
    else {
      connectedTime = theConnectedTime;
      OnConnected();
    }
  }

  if (!callEndTime.IsValid() &&
       usage.HasOptionalField(H225_RasUsageInformation::e_endTime)) {
    PTime theCallEndTime = PTime((unsigned)usage.m_endTime);
    if (theCallEndTime > now ||
       (alertingTime.IsValid() && theCallEndTime < alertingTime) ||
       (connectedTime.IsValid() && theCallEndTime < connectedTime) ||
        theCallEndTime < callStartTime)
      callEndTime = now;
    else
      callEndTime = theCallEndTime;
  }
}

PBoolean H323GatekeeperCall::TranslateAliasAddress(
      const H225_AliasAddress & alias,
      H225_ArrayOf_AliasAddress & aliases,
      H323TransportAddress & address,
      PBoolean & gkRouted
    )
{
  return gatekeeper.TranslateAliasAddress(alias, aliases, address, gkRouted, this);
}

/////////////////////////////////////////////////////////////////////////////

H323RegisteredEndPoint::H323RegisteredEndPoint(H323GatekeeperServer & gk,
                                               const PString & id)
  : gatekeeper(gk),
    rasChannel(NULL),
    identifier(id),
    protocolVersion(0),
    isBehindNAT(FALSE),
    canDisplayAmountString(FALSE),
    canEnforceDurationLimit(FALSE),
    timeToLive(0),
    authenticators(gk.GetOwnerEndPoint().CreateAuthenticators())
{
  activeCalls.DisallowDeleteObjects();

  PTRACE(3, "RAS\tCreated registered endpoint: " << id);
}


PObject::Comparison H323RegisteredEndPoint::Compare(const PObject & obj) const
{
  // Do not have to lock the object as these fields should never change for
  // life of the object

  PAssert(PIsDescendant(&obj, H323RegisteredEndPoint), PInvalidCast);
  return identifier.Compare(((const H323RegisteredEndPoint &)obj).identifier);
}


void H323RegisteredEndPoint::PrintOn(ostream & strm) const
{
  // Do not have to lock the object as these fields should never change for
  // life of the object

  strm << identifier;
}


void H323RegisteredEndPoint::AddCall(H323GatekeeperCall * call)
{
  if (call == NULL) {
    PTRACE(1, "RAS\tCould not add NULL call to endpoint " << *this);
    return; 
  }

  if (!LockReadWrite()) {
    PTRACE(1, "RAS\tCould not add call " << *call << ", lock failed on endpoint " << *this);
    return; 
  }

  if (activeCalls.GetObjectsIndex(call) == P_MAX_INDEX)
    activeCalls.Append(call);

  UnlockReadWrite();
}


PBoolean H323RegisteredEndPoint::RemoveCall(H323GatekeeperCall * call)
{
  if (call == NULL) {
    PTRACE(1, "RAS\tCould not remove NULL call to endpoint " << *this);
    return FALSE;
  }

  if (!LockReadWrite()) {
    PTRACE(1, "RAS\tCould not remove call " << *call << ", lock failed on endpoint " << *this);
    return FALSE; 
  }

  PBoolean ok = activeCalls.Remove(call);

  UnlockReadWrite();

  return ok;
}

void H323RegisteredEndPoint::RemoveAlias(const PString & alias)
{ 
  if (!LockReadWrite()) {
    PTRACE(1, "RAS\tCould not remove alias \"" << alias << "\", lock failed on endpoint " << *this);
    return;
  }

  // remove the aliases from the list inside the endpoint
  PINDEX idx;
  while ((idx = aliases.GetValuesIndex(alias)) != P_MAX_INDEX)
    aliases.RemoveAt(idx); 

  // remove the aliases from the list in the gatekeeper
  gatekeeper.RemoveAlias(*this, alias);

  UnlockReadWrite();
}

static PBoolean IsTransportAddressSuperset(const H225_ArrayOf_TransportAddress & pdu,
                                       const H323TransportAddressArray & oldAddresses)
{
  H323TransportAddressArray newAddresses(pdu);

  for (PINDEX i = 0; i < oldAddresses.GetSize(); i++) {
    if (newAddresses.GetValuesIndex(oldAddresses[i]) == P_MAX_INDEX)
      return FALSE;
  }

  return TRUE;
}


static PStringArray GetAliasAddressArray(const H225_ArrayOf_AliasAddress & pdu)
{
  PStringArray aliases;

  for (PINDEX i = 0; i < pdu.GetSize(); i++) {
    PString alias = H323GetAliasAddressString(pdu[i]);
    if (!alias)
      aliases.AppendString(alias);
  }

  return aliases;
}


static PBoolean IsAliasAddressSuperset(const H225_ArrayOf_AliasAddress & pdu,
                                   const PStringArray & oldAliases)
{
  PStringArray newAliases = GetAliasAddressArray(pdu);

  for (PINDEX i = 0; i < oldAliases.GetSize(); i++) {
    if (newAliases.GetValuesIndex(oldAliases[i]) == P_MAX_INDEX)
      return FALSE;
  }

  return TRUE;
}


H323GatekeeperRequest::Response H323RegisteredEndPoint::OnRegistration(H323GatekeeperRRQ & info)
{
  PTRACE_BLOCK("H323RegisteredEndPoint::OnRegistration");

  if (!LockReadWrite()) {
    PTRACE(1, "RAS\tRRQ rejected, lock failed on endpoint " << *this);
    return H323GatekeeperRequest::Reject; 
  }

  rasChannel = &info.GetRasChannel();
  lastRegistration = PTime();
  protocolVersion = info.rrq.m_protocolIdentifier[5];

  timeToLive = gatekeeper.GetTimeToLive();
  if (info.rrq.HasOptionalField(H225_RegistrationRequest::e_timeToLive) &&
      timeToLive > info.rrq.m_timeToLive)
    timeToLive = info.rrq.m_timeToLive;

  if (timeToLive > 0) {
    info.rcf.IncludeOptionalField(H225_RegistrationRequest::e_timeToLive);
    info.rcf.m_timeToLive = timeToLive;
  }

  info.rcf.m_endpointIdentifier = identifier;

  UnlockReadWrite();

  if (info.rrq.m_keepAlive)
    return info.CheckCryptoTokens() ? H323GatekeeperRequest::Confirm
                                    : H323GatekeeperRequest::Reject;

  if (info.rrq.HasOptionalField(H225_RegistrationRequest::e_endpointIdentifier)) {
    // Make sure addresses are a superset of previous registration
    if (!IsTransportAddressSuperset(info.rrq.m_rasAddress, rasAddresses) ||
        !IsTransportAddressSuperset(info.rrq.m_callSignalAddress, signalAddresses) ||
        (info.rrq.HasOptionalField(H225_RegistrationRequest::e_terminalAlias) &&
         !IsAliasAddressSuperset(info.rrq.m_terminalAlias, aliases))) {
      info.SetRejectReason(H225_RegistrationRejectReason::e_discoveryRequired);
      PTRACE(2, "RAS\tRRQ rejected, not superset of existing registration.");
      return H323GatekeeperRequest::Reject;
    }
    PTRACE(3, "RAS\tFull RRQ received for already registered endpoint");
  }

  H323GatekeeperRequest::Response response = OnFullRegistration(info);
  if (response != H323GatekeeperRequest::Confirm)
    return response;

  // Final check, the H.235 security
  if (!info.CheckCryptoTokens())
    return H323GatekeeperRequest::Reject;

  PINDEX i;

  info.rcf.m_callSignalAddress.SetSize(signalAddresses.GetSize());
  for (i = 0; i < signalAddresses.GetSize(); i++)
    signalAddresses[i].SetPDU(info.rcf.m_callSignalAddress[i]);

  if (aliases.GetSize() > 0) {
    info.rcf.IncludeOptionalField(H225_RegistrationConfirm::e_terminalAlias);
    info.rcf.m_terminalAlias.SetSize(aliases.GetSize());
    for (i = 0; i < aliases.GetSize(); i++)
      H323SetAliasAddress(aliases[i], info.rcf.m_terminalAlias[i]);
  }

#ifdef H323_H248
  if (canDisplayAmountString) {
    H323CallCreditServiceControl credit(GetCallCreditAmount(), GetCallCreditMode());
    if (AddServiceControlSession(credit, info.rcf.m_serviceControl))
      info.rcf.IncludeOptionalField(H225_RegistrationConfirm::e_serviceControl);
  }
#endif

#ifdef H323_H501

  // If have peer element, so add/update a descriptor for ep.
  H323PeerElement * peerElement = gatekeeper.GetPeerElement();
  if (peerElement != NULL) {

    H225_ArrayOf_AliasAddress transportAddresses;
    H323SetAliasAddresses(signalAddresses, transportAddresses);
    H225_EndpointType terminalType    = info.rrq.m_terminalType;
    H225_ArrayOf_AliasAddress aliases = info.rcf.m_terminalAlias;

    if (OnSendDescriptorForEndpoint(aliases, terminalType, transportAddresses)) {
      H501_ArrayOf_AddressTemplate addressTemplates;
      addressTemplates.SetSize(1);
      H323PeerElementDescriptor::CopyToAddressTemplate(addressTemplates[0],
                                                       terminalType,        // info.rrq.m_terminalType,
                                                       aliases,             // info.rcf.m_terminalAlias,
                                                       transportAddresses);
      peerElement->AddDescriptor(descriptorID,
                                 H323PeerElement::LocalServiceRelationshipOrdinal,
                                 addressTemplates,
                                 PTime());
    }
  }

#endif

  return H323GatekeeperRequest::Confirm;
}


H323GatekeeperRequest::Response H323RegisteredEndPoint::OnFullRegistration(H323GatekeeperRRQ & info)
{
  if (!LockReadWrite()) {
    PTRACE(1, "RAS\tRRQ rejected, lock failed on endpoint " << *this);
    return H323GatekeeperRequest::Reject;
  }

  isBehindNAT = info.IsBehindNAT();
  rasAddresses = info.GetReplyAddresses();

  signalAddresses = H323TransportAddressArray(info.rrq.m_callSignalAddress);
  if (signalAddresses.IsEmpty()) {
    UnlockReadWrite();
    info.SetRejectReason(H225_RegistrationRejectReason::e_invalidCallSignalAddress);
    return H323GatekeeperRequest::Reject;
  }

  if (isBehindNAT) {
    // Need to (maybe) massage the signalling addresses
    H323EndPoint & ep = rasChannel->GetEndPoint();
    WORD listenerPort = 0;

    PINDEX i;
    for (i = 0; i < signalAddresses.GetSize(); i++) {
      PIPSocket::Address ip;
      WORD port = 0;
      if (signalAddresses[i].GetIpAndPort(ip, port)) {
        if (!ep.IsLocalAddress(ip))
          break;
        if (listenerPort == 0)
          listenerPort = port; // Have a private address, save the port
      }
    }

    if (i < signalAddresses.GetSize()) {
      // Found a public address in the list, make sure it is the first entry
      if (i > 0) {
        H323TransportAddress addr = signalAddresses[0];
        signalAddresses[0] = signalAddresses[i];
        signalAddresses[i] = addr;
      }
    }
    else if (listenerPort != 0) {
      // If did not have a public signalling address, but did have a private
      // one, then we insert at the begining an entry on the same port but
      // using the public address from the RAS. This will not work for all
      // NAT types, but will for some, eg symmetric with port forwarding which
      // is a common configuration for Linux based routers.
      i = signalAddresses.GetSize()-1;
      signalAddresses.AppendAddress(signalAddresses[i]);
      while (--i > 0)
        signalAddresses[i] = signalAddresses[i-1];
      PIPSocket::Address natAddress;
      rasAddresses[0].GetIpAddress(natAddress);
      signalAddresses[0] = H323TransportAddress(natAddress, listenerPort);
    }
  }

  if (info.rrq.HasOptionalField(H225_RegistrationRequest::e_terminalAlias))
    aliases = GetAliasAddressArray(info.rrq.m_terminalAlias);

  const H225_EndpointType & terminalType = info.rrq.m_terminalType;
  if (terminalType.HasOptionalField(H225_EndpointType::e_gateway) &&
      terminalType.m_gateway.HasOptionalField(H225_GatewayInfo::e_protocol)) {
    const H225_ArrayOf_SupportedProtocols & protocols = terminalType.m_gateway.m_protocol;
    for (PINDEX i = 0; i < protocols.GetSize(); i++) {

      // Only voice prefixes are supported
      if (protocols[i].GetTag() == H225_SupportedProtocols::e_voice) {
	      H225_VoiceCaps & voiceCaps = protocols[i];
	      if (voiceCaps.HasOptionalField(H225_VoiceCaps::e_supportedPrefixes)) {
	        H225_ArrayOf_SupportedPrefix & prefixes = voiceCaps.m_supportedPrefixes;
	        voicePrefixes.SetSize(prefixes.GetSize());
	        for (PINDEX j = 0; j < prefixes.GetSize(); j++) {
	          PString prefix = H323GetAliasAddressString(prefixes[j].m_prefix);
	          voicePrefixes[j] = prefix;
	        }
	      }
	      break;  // If voice protocol is found, don't look any further
      }
    }
  }

  applicationInfo = H323GetApplicationInfo(info.rrq.m_endpointVendor);

  canDisplayAmountString = FALSE;
  canEnforceDurationLimit = FALSE;
  if (info.rrq.HasOptionalField(H225_RegistrationRequest::e_callCreditCapability)) {
    if (info.rrq.m_callCreditCapability.HasOptionalField(H225_CallCreditCapability::e_canDisplayAmountString))
      canDisplayAmountString = info.rrq.m_callCreditCapability.m_canDisplayAmountString;
    if (info.rrq.m_callCreditCapability.HasOptionalField(H225_CallCreditCapability::e_canEnforceDurationLimit))
      canEnforceDurationLimit = info.rrq.m_callCreditCapability.m_canEnforceDurationLimit;
  }

  h225Version = 0;
  PUnsignedArray protocolVer = info.rrq.m_protocolIdentifier.GetValue();
  if (protocolVer.GetSize() >= 6)
    h225Version = protocolVer[5];

  H323GatekeeperRequest::Response respone = OnSecureRegistration(info);

  UnlockReadWrite();

  return respone;
}


H323GatekeeperRequest::Response H323RegisteredEndPoint::OnSecureRegistration(H323GatekeeperRRQ & info)
{
  for (PINDEX i = 0; i < aliases.GetSize(); i++) {
    PString password;
    if (gatekeeper.GetUsersPassword(aliases[i], password, *this)) {
      PTRACE(3, "RAS\tFound user " << aliases[i] << " for H.235 security.");
      if (!password)
        SetPassword(password, aliases[i]);
      return H323GatekeeperRequest::Confirm;
    }
  }

  if (gatekeeper.IsRequiredH235()) {
    PTRACE(2, "RAS\tRejecting RRQ, no aliases have a password.");
    info.SetRejectReason(H225_RegistrationRejectReason::e_securityDenial);
    return H323GatekeeperRequest::Reject;
  }

  return H323GatekeeperRequest::Confirm;
}


PBoolean H323RegisteredEndPoint::SetPassword(const PString & password,
                                         const PString & username)
{
  if (authenticators.IsEmpty() || password.IsEmpty())
    return FALSE;

  PTRACE(3, "RAS\tSetting password and enabling H.235 security for " << *this);
  for (PINDEX i = 0; i < authenticators.GetSize(); i++) {
    H235Authenticator & authenticator = authenticators[i];
    authenticator.SetPassword(password);
    if (!username && !authenticator.UseGkAndEpIdentifiers())
      authenticator.SetRemoteId(username);
    authenticator.Enable();
  }

  return TRUE;
}


H323GatekeeperRequest::Response H323RegisteredEndPoint::OnUnregistration(H323GatekeeperURQ & info)
{
  PTRACE_BLOCK("H323RegisteredEndPoint::OnUnregistration");

  if (activeCalls.GetSize() > 0) {
    info.SetRejectReason(H225_UnregRejectReason::e_callInProgress);
    return H323GatekeeperRequest::Reject;
  }

  return H323GatekeeperRequest::Confirm;
}


PBoolean H323RegisteredEndPoint::Unregister(int reason)
{
  if (reason == -1)
    reason = H225_UnregRequestReason::e_maintenance;

  PBoolean ok;

  // Send DRQ to endpoint(s)
  if (rasChannel != NULL)
    ok = rasChannel->UnregistrationRequest(*this, reason);
  else {
    // Can't do DRQ as have no RAS channel to use (probably logic error)
    PAssertAlways("Tried to unregister endpoint we did not receive RRQ for!");
    ok = FALSE;
  }

  gatekeeper.RemoveEndPoint(this);

  return ok;
}


H323GatekeeperRequest::Response H323RegisteredEndPoint::OnInfoResponse(H323GatekeeperIRR & info)
{
  PTRACE_BLOCK("H323RegisteredEndPoint::OnInfoResponse");

  if (!LockReadWrite()) {
    PTRACE(1, "RAS\tIRR rejected, lock failed on endpoint " << *this);
    return H323GatekeeperRequest::Reject; 
  }

  lastInfoResponse = PTime();
  UnlockReadWrite();

  if (info.irr.HasOptionalField(H225_InfoRequestResponse::e_irrStatus) &&
      info.irr.m_irrStatus.GetTag() == H225_InfoRequestResponseStatus::e_invalidCall) {
    PTRACE(2, "RAS\tIRR for call-id endpoint does not know about");
    return H323GatekeeperRequest::Confirm;
  }
    

  if (!info.irr.HasOptionalField(H225_InfoRequestResponse::e_perCallInfo)) {
    // Special case for Innovaphone clients that do not contain a perCallInfo
    // field and expects that to mean "all calls a normal".
    if (protocolVersion < 5 && applicationInfo.Find("innovaphone") != P_MAX_INDEX) {
      H225_InfoRequestResponse_perCallInfo_subtype fakeCallInfo;
      if (!LockReadOnly()) {
        PTRACE(1, "RAS\tIRR rejected, lock failed on endpoint " << *this);
        return H323GatekeeperRequest::Reject; 
      }
      for (PINDEX i = 0; i < activeCalls.GetSize(); i++)
        activeCalls[i].OnInfoResponse(info, fakeCallInfo);
      UnlockReadOnly();
    }

    PTRACE(2, "RAS\tIRR for call-id endpoint does not know about");
    return H323GatekeeperRequest::Confirm;
  }

  if (!LockReadOnly()) {
    PTRACE(1, "RAS\tIRR rejected, lock failed on endpoint " << *this);
    return H323GatekeeperRequest::Reject; 
  }

  for (PINDEX i = 0; i < info.irr.m_perCallInfo.GetSize(); i++) {
    H225_InfoRequestResponse_perCallInfo_subtype & perCallInfo = info.irr.m_perCallInfo[i];

    // Some systems don't say what direction the call is so find it in the
    // list regardless of the direction
    H323GatekeeperCall::Direction callDirection;
    if (!perCallInfo.HasOptionalField(H225_InfoRequestResponse_perCallInfo_subtype::e_originator))
      callDirection = H323GatekeeperCall::UnknownDirection;
    else if (perCallInfo.m_originator)
      callDirection = H323GatekeeperCall::OriginatingCall;
    else
      callDirection = H323GatekeeperCall::AnsweringCall;

    H323GatekeeperCall search(gatekeeper, perCallInfo.m_callIdentifier.m_guid, callDirection);

    PINDEX idx = activeCalls.GetValuesIndex(search);
    if (idx != P_MAX_INDEX) {
      activeCalls[idx].OnInfoResponse(info, perCallInfo);

      if (callDirection == H323GatekeeperCall::UnknownDirection) {
        // There could be two call entries (originator or destination) and the
        // ep did not say which. GetValuesIndex() always give the first one so
        // see if the next one is also a match
        if (idx < activeCalls.GetSize()-1 && activeCalls[idx+1] == search)
          activeCalls[idx+1].OnInfoResponse(info, perCallInfo);
      }
    }
    else {
      PTRACE(2, "RAS\tEndpoint has call-id gatekeeper does not know about: " << search);
    }
  }

  UnlockReadOnly();

  return H323GatekeeperRequest::Confirm;
}


PBoolean H323RegisteredEndPoint::OnTimeToLive()
{
  if (!LockReadOnly()) {
    PTRACE(1, "RAS\tOnTimeToLive lock failed on endpoint " << *this);
    return FALSE;
  }

  if (CheckTimeSince(lastRegistration, timeToLive) ||
      CheckTimeSince(lastInfoResponse, timeToLive)) {
    UnlockReadOnly();
    return TRUE;
  }

  // Can't do IRQ as have no RAS channel to use (probably logic error)
  if (rasChannel == NULL) {
    UnlockReadOnly();
    PAssertAlways("Timeout on time to live for endpoint we did not receive RRQ for!");
    return FALSE;
  }

  UnlockReadOnly();

  // Do IRQ and wait for IRR on call
  PTRACE(2, "RAS\tTime to live, doing IRQ for endpoint "<< *this);
  if (!rasChannel->InfoRequest(*this))
    return FALSE;

  if (!LockReadOnly()) {
    PTRACE(1, "RAS\tOnTimeToLive lock failed on endpoint " << *this);
    return FALSE;
  }

  // Return TRUE if got a resonse, ie client does not do unsolicited IRR's
  // otherwise did not get a response from client so return FALSE and
  // (probably) disengage the call.
  PBoolean response = CheckTimeSince(lastInfoResponse, timeToLive);

  UnlockReadOnly();

  return response;
}

#ifdef H323_H248

PString H323RegisteredEndPoint::GetCallCreditAmount() const
{
  return PString::Empty();
}


PBoolean H323RegisteredEndPoint::GetCallCreditMode() const
{
  return TRUE;
}


PBoolean H323RegisteredEndPoint::SendServiceControlSession(const H323ServiceControlSession & session)
{
  // Send SCI to endpoint(s)
  if (rasChannel == NULL) {
    // Can't do SCI as have no RAS channel to use (probably logic error)
    PAssertAlways("Tried to do SCI to endpoint we did not receive RRQ for!");
    return FALSE;
  }

  return rasChannel->ServiceControlIndication(*this, session);
}


PBoolean H323RegisteredEndPoint::AddServiceControlSession(const H323ServiceControlSession & session,
                                                      H225_ArrayOf_ServiceControlSession & serviceControl)
{
  if (!session.IsValid())
    return FALSE;

  PString type = session.GetServiceControlType();

  H225_ServiceControlSession_reason::Choices reason = H225_ServiceControlSession_reason::e_refresh;
  if (!serviceControlSessions.Contains(type)) {
    PINDEX id = 0;
    PINDEX i = 0;
    while (i < serviceControlSessions.GetSize()) {
      if (id != serviceControlSessions.GetDataAt(i))
        i++;
      else {
        if (++id >= 256)
          return FALSE;
        i = 0;
      }
    }
    serviceControlSessions.SetAt(type, id);
    reason = H225_ServiceControlSession_reason::e_open;
  }

  PINDEX last = serviceControl.GetSize();
  serviceControl.SetSize(last+1);
  H225_ServiceControlSession & pdu = serviceControl[last];

  pdu.m_sessionId = serviceControlSessions[type];
  pdu.m_reason = reason;

  if (session.OnSendingPDU(pdu.m_contents))
    pdu.IncludeOptionalField(H225_ServiceControlSession::e_contents);

  return TRUE;
}

#endif // H323_H248


PBoolean H323RegisteredEndPoint::CanReceiveRIP() const
{ 
  // H225v1 does not support RIP
  // neither does NetMeeting, even though it says it is H225v2. 
  return (h225Version > 1) && (applicationInfo.Find("netmeeting") == P_MAX_INDEX);
}

#ifdef H323_H501

PBoolean H323RegisteredEndPoint::OnSendDescriptorForEndpoint(
        H225_ArrayOf_AliasAddress & aliases,          // aliases for the enndpoint
        H225_EndpointType & terminalType,             // terminal type
        H225_ArrayOf_AliasAddress & transportAddresses  // transport addresses
      )
{ 
  return gatekeeper.OnSendDescriptorForEndpoint(*this, aliases, terminalType, transportAddresses); 
}

#endif

/////////////////////////////////////////////////////////////////////////////

H323GatekeeperListener::H323GatekeeperListener(H323EndPoint & ep,
                                               H323GatekeeperServer & gk,
                                               const PString & id,
                                               H323Transport * trans)
  : H225_RAS(ep, trans),
    gatekeeper(gk)
{
  gatekeeperIdentifier = id;

  transport->SetPromiscuous(H323Transport::AcceptFromAny);

  PTRACE(2, "H323gk\tGatekeeper server created.");
}


H323GatekeeperListener::~H323GatekeeperListener()
{
  StopChannel();
  PTRACE(2, "H323gk\tGatekeeper server destroyed.");
}


H323GatekeeperRequest::Response H323GatekeeperListener::OnDiscovery(H323GatekeeperGRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnDiscovery");

  if (info.grq.m_protocolIdentifier.GetSize() != 6 || info.grq.m_protocolIdentifier[5] < 2) {
    info.SetRejectReason(H225_GatekeeperRejectReason::e_invalidRevision);
    PTRACE(2, "RAS\tGRQ rejected, version 1 not supported");
    return H323GatekeeperRequest::Reject;
  }

  if (!info.CheckGatekeeperIdentifier())
    return H323GatekeeperRequest::Reject;

  // cannot use commented code because transport->remoteAddr is
  // not set on broadcast PDUs
  //      transport->SetUpTransportPDU(info.gcf.m_rasAddress, TRUE);
  {
    PIPSocket::Address localAddr, remoteAddr;
    WORD localPort = 0;
    transport->GetLocalAddress().GetIpAndPort(localAddr, localPort);
    H323TransportAddress(info.grq.m_rasAddress).GetIpAddress(remoteAddr);
    endpoint.InternalTranslateTCPAddress(localAddr, remoteAddr);
    endpoint.TranslateTCPPort(localPort,remoteAddr);
    H323TransportAddress newAddr = H323TransportAddress(localAddr, localPort);

    H225_TransportAddress & pdu = info.gcf.m_rasAddress;
    newAddr.SetPDU(pdu);
  }

  return gatekeeper.OnDiscovery(info);
}


PBoolean H323GatekeeperListener::OnReceiveGatekeeperRequest(const H323RasPDU & pdu,
                                                        const H225_GatekeeperRequest & /*grq*/)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveGatekeeperRequest");

  H323GatekeeperGRQ * info = new H323GatekeeperGRQ(*this, pdu);
  if (!info->HandlePDU())
    delete info;

  return FALSE;
}


H323GatekeeperRequest::Response H323GatekeeperListener::OnRegistration(H323GatekeeperRRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnRegistration");

  if (info.rrq.HasOptionalField(H225_RegistrationRequest::e_endpointIdentifier))
    info.endpoint = gatekeeper.FindEndPointByIdentifier(info.rrq.m_endpointIdentifier);

  if (!info.CheckGatekeeperIdentifier())
    return H323GatekeeperRequest::Reject;

  if (info.rrq.m_protocolIdentifier.GetSize() != 6 || info.rrq.m_protocolIdentifier[5] < 2) {
    info.SetRejectReason(H225_RegistrationRejectReason::e_invalidRevision);
    PTRACE(2, "RAS\tRRQ rejected, version 1 not supported");
    return H323GatekeeperRequest::Reject;
  }

  H323GatekeeperRequest::Response response = gatekeeper.OnRegistration(info);
  if (response != H323GatekeeperRequest::Confirm)
    return response;

  // Adjust the authenticator remote ID to endpoint ID
  if (!info.rrq.m_keepAlive) {
    PSafePtr<H323RegisteredEndPoint> lock(info.endpoint, PSafeReadWrite);
    H235Authenticators authenticators = info.endpoint->GetAuthenticators();
    for (PINDEX i = 0; i < authenticators.GetSize(); i++) {
      H235Authenticator & authenticator = authenticators[i];
      if (authenticator.UseGkAndEpIdentifiers()) {
        authenticator.SetRemoteId(info.endpoint->GetIdentifier());
        authenticator.SetLocalId(gatekeeperIdentifier);
      }
    }
  }

  return H323GatekeeperRequest::Confirm;
}


PBoolean H323GatekeeperListener::OnReceiveRegistrationRequest(const H323RasPDU & pdu,
                                                          const H225_RegistrationRequest & /*rrq*/)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveRegistrationRequest");

  H323GatekeeperRRQ * info = new H323GatekeeperRRQ(*this, pdu);
  if (!info->HandlePDU())
    delete info;

  return FALSE;
}


H323GatekeeperRequest::Response H323GatekeeperListener::OnUnregistration(H323GatekeeperURQ & info)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnUnregistration");

  if (info.urq.HasOptionalField(H225_UnregistrationRequest::e_endpointIdentifier))
    info.endpoint = gatekeeper.FindEndPointByIdentifier(info.urq.m_endpointIdentifier);
  else
    info.endpoint = gatekeeper.FindEndPointBySignalAddresses(info.urq.m_callSignalAddress);

  if (info.endpoint == NULL) {
    info.SetRejectReason(H225_UnregRejectReason::e_notCurrentlyRegistered);
    PTRACE(2, "RAS\tURQ rejected, not registered");
    return H323GatekeeperRequest::Reject;
  }

  return gatekeeper.OnUnregistration(info);
}


PBoolean H323GatekeeperListener::OnReceiveUnregistrationRequest(const H323RasPDU & pdu,
                                                            const H225_UnregistrationRequest & /*urq*/)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveUnregistrationRequest");

  H323GatekeeperURQ * info = new H323GatekeeperURQ(*this, pdu);
  if (!info->HandlePDU())
    delete info;

  return FALSE;
}


PBoolean H323GatekeeperListener::OnReceiveUnregistrationConfirm(const H225_UnregistrationConfirm & pdu)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveUnregistrationConfirm");

  if (!H225_RAS::OnReceiveUnregistrationConfirm(pdu))
    return FALSE;

  return TRUE;
}


PBoolean H323GatekeeperListener::OnReceiveUnregistrationReject(const H225_UnregistrationReject & pdu)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveUnregistrationReject");

  if (!H225_RAS::OnReceiveUnregistrationReject(pdu))
    return FALSE;

  return TRUE;
}


H323GatekeeperRequest::Response H323GatekeeperListener::OnAdmission(H323GatekeeperARQ & info)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnAdmission");

  if (!info.CheckGatekeeperIdentifier())
    return H323GatekeeperRequest::Reject;

  if (!info.GetRegisteredEndPoint())
    return H323GatekeeperRequest::Reject;

  if (!info.CheckCryptoTokens()) {
    H235Authenticators adjustedAuthenticators;
    if (!gatekeeper.GetAdmissionRequestAuthentication(info, adjustedAuthenticators))
      return H323GatekeeperRequest::Reject;

    PTRACE(3, "RAS\tARQ received with separate credentials: "
           << setfill(',') << adjustedAuthenticators << setfill(' '));
    if (!info.H323Transaction::CheckCryptoTokens(adjustedAuthenticators)) {
      PTRACE(2, "RAS\tARQ rejected, alternate security tokens invalid.");

      return H323GatekeeperRequest::Reject;
    }

    if (info.alternateSecurityID.IsEmpty() && !adjustedAuthenticators.IsEmpty())
      info.alternateSecurityID = adjustedAuthenticators[0].GetRemoteId();
  }

  H323GatekeeperRequest::Response response = gatekeeper.OnAdmission(info);
  if (response != H323GatekeeperRequest::Confirm)
    return response;

  if (info.acf.m_callModel.GetTag() == H225_CallModel::e_gatekeeperRouted) {
    H225_ArrayOf_TransportAddress addresses;
    if (SetUpCallSignalAddresses(addresses))
      info.acf.m_destCallSignalAddress = addresses[0];
  }

  return H323GatekeeperRequest::Confirm;
}


PBoolean H323GatekeeperListener::OnReceiveAdmissionRequest(const H323RasPDU & pdu,
                                                       const H225_AdmissionRequest & /*arq*/)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveAdmissionRequest");

  H323GatekeeperARQ * info = new H323GatekeeperARQ(*this, pdu);
  if (!info->HandlePDU())
    delete info;

  return FALSE;
}


PBoolean H323GatekeeperListener::UnregistrationRequest(const H323RegisteredEndPoint & ep,
                                                   unsigned reason)
{
  PTRACE(3, "RAS\tUnregistration request to endpoint " << ep);

  H323RasPDU pdu(ep.GetAuthenticators());
  H225_UnregistrationRequest & urq = pdu.BuildUnregistrationRequest(GetNextSequenceNumber());

  urq.IncludeOptionalField(H225_UnregistrationRequest::e_gatekeeperIdentifier);
  urq.m_gatekeeperIdentifier = gatekeeperIdentifier;

  urq.m_callSignalAddress.SetSize(ep.GetSignalAddressCount());
  for (PINDEX i = 0; i < ep.GetSignalAddressCount(); i++)
    ep.GetSignalAddress(i).SetPDU(urq.m_callSignalAddress[i]);

  urq.IncludeOptionalField(H225_UnregistrationRequest::e_endpointIdentifier);
  urq.m_endpointIdentifier = ep.GetIdentifier();
  urq.m_reason.SetTag(reason);

  Request request(urq.m_requestSeqNum, pdu, ep.GetRASAddresses());
  return MakeRequest(request);
}


PBoolean H323GatekeeperListener::DisengageRequest(const H323GatekeeperCall & call,
                                              unsigned reason)
{
  H323RegisteredEndPoint & ep = call.GetEndPoint();

  PTRACE(3, "RAS\tDisengage request to endpoint " << ep << " call " << call);

  H323RasPDU pdu(ep.GetAuthenticators());
  H225_DisengageRequest & drq = pdu.BuildDisengageRequest(GetNextSequenceNumber());

  drq.IncludeOptionalField(H225_DisengageRequest::e_gatekeeperIdentifier);
  drq.m_gatekeeperIdentifier = gatekeeperIdentifier;

  drq.m_endpointIdentifier = ep.GetIdentifier();
  drq.m_conferenceID = call.GetConferenceIdentifier();
  drq.m_callReferenceValue = call.GetCallReference();
  drq.m_callIdentifier.m_guid = call.GetCallIdentifier();
  drq.m_disengageReason.SetTag(reason);
  drq.m_answeredCall = call.IsAnsweringCall();

#ifdef H323_H248
  if (call.AddCallCreditServiceControl(drq.m_serviceControl))
    drq.IncludeOptionalField(H225_DisengageRequest::e_serviceControl);
#endif

  Request request(drq.m_requestSeqNum, pdu, ep.GetRASAddresses());
  return MakeRequest(request);
}


H323GatekeeperRequest::Response H323GatekeeperListener::OnDisengage(H323GatekeeperDRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnDisengage");

  if (!info.CheckGatekeeperIdentifier())
    return H323GatekeeperRequest::Reject;

  if (!info.GetRegisteredEndPoint())
    return H323GatekeeperRequest::Reject;

  if (!info.CheckCryptoTokens())
    return H323GatekeeperRequest::Reject;

  return gatekeeper.OnDisengage(info);
}


PBoolean H323GatekeeperListener::OnReceiveDisengageRequest(const H323RasPDU & pdu,
                                                       const H225_DisengageRequest & /*drq*/)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveDisengageRequest");

  H323GatekeeperDRQ * info = new H323GatekeeperDRQ(*this, pdu);
  if (!info->HandlePDU())
    delete info;

  return FALSE;
}


PBoolean H323GatekeeperListener::OnReceiveDisengageConfirm(const H225_DisengageConfirm & pdu)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveDisengageConfirm");

  if (!H225_RAS::OnReceiveDisengageConfirm(pdu))
    return FALSE;

  return TRUE;
}


PBoolean H323GatekeeperListener::OnReceiveDisengageReject(const H225_DisengageReject & drj)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveDisengageReject");

  if (!H225_RAS::OnReceiveDisengageReject(drj))
    return FALSE;

  return TRUE;
}


H323GatekeeperRequest::Response H323GatekeeperListener::OnBandwidth(H323GatekeeperBRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnBandwidth");

  if (!info.CheckGatekeeperIdentifier())
    return H323GatekeeperRequest::Reject;

  if (!info.GetRegisteredEndPoint())
    return H323GatekeeperRequest::Reject;

  if (!info.CheckCryptoTokens())
    return H323GatekeeperRequest::Reject;

  return gatekeeper.OnBandwidth(info);
}


PBoolean H323GatekeeperListener::OnReceiveBandwidthRequest(const H323RasPDU & pdu,
                                                       const H225_BandwidthRequest & /*brq*/)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveBandwidthRequest");

  H323GatekeeperBRQ * info = new H323GatekeeperBRQ(*this, pdu);
  if (!info->HandlePDU())
    delete info;

  return FALSE;
}


PBoolean H323GatekeeperListener::OnReceiveBandwidthConfirm(const H225_BandwidthConfirm & pdu)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveBandwidthConfirm");

  if (!H225_RAS::OnReceiveBandwidthConfirm(pdu))
    return FALSE;

  return TRUE;
}


PBoolean H323GatekeeperListener::OnReceiveBandwidthReject(const H225_BandwidthReject & pdu)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveBandwidthReject");

  if (!H225_RAS::OnReceiveBandwidthReject(pdu))
    return FALSE;

  return TRUE;
}


H323GatekeeperRequest::Response H323GatekeeperListener::OnLocation(H323GatekeeperLRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnLocation");

  if (!info.CheckGatekeeperIdentifier())
    return H323GatekeeperRequest::Reject;

  if (info.lrq.HasOptionalField(H225_LocationRequest::e_endpointIdentifier)) {
    if (!info.GetRegisteredEndPoint())
      return H323GatekeeperRequest::Reject;
    if (!info.CheckCryptoTokens())
      return H323GatekeeperRequest::Reject;
  }

  transport->SetUpTransportPDU(info.lcf.m_rasAddress, TRUE);

  return gatekeeper.OnLocation(info);
}


PBoolean H323GatekeeperListener::OnReceiveLocationRequest(const H323RasPDU & pdu,
                                                      const H225_LocationRequest & /*lrq*/)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveLocationRequest");

  H323GatekeeperLRQ * info = new H323GatekeeperLRQ(*this, pdu);
  if (!info->HandlePDU())
    delete info;

  return FALSE;
}


PBoolean H323GatekeeperListener::InfoRequest(H323RegisteredEndPoint & ep,
                                         H323GatekeeperCall * call)
{
  PTRACE(3, "RAS\tInfo request to endpoint " << ep);

  unsigned callReference = 0;
  const OpalGloballyUniqueID * callIdentifier = NULL;
  if (call != NULL) {
    callReference = call->GetCallReference();
    callIdentifier = &call->GetCallIdentifier();
  }

  // As sequence number 1 is used for backward compatibility on unsolicited
  // IRR's we make sure we never makea  solicited IRQ using that number.
  unsigned seqnum = GetNextSequenceNumber();
  if (seqnum == 1)
    seqnum = GetNextSequenceNumber();

  H323RasPDU pdu(ep.GetAuthenticators());
  H225_InfoRequest & irq = pdu.BuildInfoRequest(seqnum, callReference, callIdentifier);

  H225_RAS::Request request(irq.m_requestSeqNum, pdu, ep.GetRASAddresses());
  return MakeRequest(request);
}

#ifdef H323_H248

PBoolean H323GatekeeperListener::ServiceControlIndication(H323RegisteredEndPoint & ep,
                                                      const H323ServiceControlSession & session,
                                                      H323GatekeeperCall * call)
{
  PTRACE(3, "RAS\tService control request to endpoint " << ep);

  OpalGloballyUniqueID id = NULL;
  if (call != NULL)
    id = call->GetCallIdentifier();

  H323RasPDU pdu(ep.GetAuthenticators());

  H225_ServiceControlIndication & sci = pdu.BuildServiceControlIndication(GetNextSequenceNumber(), &id);
  ep.AddServiceControlSession(session, sci.m_serviceControl);
  H225_RAS::Request request(sci.m_requestSeqNum, pdu, ep.GetRASAddresses());
  return MakeRequest(request);
}

#endif

H323GatekeeperRequest::Response H323GatekeeperListener::OnInfoResponse(H323GatekeeperIRR & info)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnInfoResponse");

  H323GatekeeperRequest::Response response;
  if (info.GetRegisteredEndPoint() && info.CheckCryptoTokens())
    response = gatekeeper.OnInfoResponse(info);
  else
    response = H323GatekeeperRequest::Reject;

  if (info.irr.m_unsolicited)
    return response;

  return H323GatekeeperRequest::Ignore;
}


PBoolean H323GatekeeperListener::OnReceiveInfoRequestResponse(const H323RasPDU & pdu,
                                                          const H225_InfoRequestResponse & irr)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveInfoRequestResponse");

  PBoolean unsolicited = irr.m_unsolicited;

  if (!unsolicited) {
    // Got an IRR that is not marked as unsolicited but has sequence number of
    // one, thus according to 7.15.2/H.225.0 it actually IS unsolicited.
    if (irr.m_requestSeqNum == 1)
      unsolicited = TRUE;
    else {
      if (!H225_RAS::OnReceiveInfoRequestResponse(pdu, irr))
        return FALSE;
    }
  }
  else {
    if (SendCachedResponse(pdu))
      return FALSE;
  }

  H323GatekeeperIRR * info = new H323GatekeeperIRR(*this, pdu);

  info->irr.m_unsolicited = unsolicited;

  if (!info->HandlePDU())
    delete info;

  return !unsolicited;
}


PBoolean H323GatekeeperListener::OnReceiveResourcesAvailableConfirm(const H225_ResourcesAvailableConfirm & pdu)
{
  PTRACE_BLOCK("H323GatekeeperListener::OnReceiveResourcesAvailableConfirm");

  if (!H225_RAS::OnReceiveResourcesAvailableConfirm(pdu))
    return FALSE;

  return TRUE;
}

PBoolean H323GatekeeperListener::OnSendFeatureSet(unsigned pduType, H225_FeatureSet & set, PBoolean advertise) const
{
  return gatekeeper.OnSendFeatureSet(pduType, set, advertise);
}

void H323GatekeeperListener::OnReceiveFeatureSet(unsigned pduType, const H225_FeatureSet & set) const
{
  gatekeeper.OnReceiveFeatureSet(pduType, set);
}

/////////////////////////////////////////////////////////////////////////////

H323GatekeeperServer::H323GatekeeperServer(H323EndPoint & ep)
  : H323TransactionServer(ep)
{
  totalBandwidth = UINT_MAX;      // Unlimited total bandwidth
  usedBandwidth = 0;              // None used so far
  defaultBandwidth = 2560;        // Enough for bidirectional G.711 and 64k H.261
  maximumBandwidth = 200000;      // 10baseX LAN bandwidth
  defaultTimeToLive = 3600;       // One hour, zero disables
  defaultInfoResponseRate = 60;   // One minute, zero disables
  overwriteOnSameSignalAddress = TRUE;
  canHaveDuplicateAlias = FALSE;
  canHaveDuplicatePrefix = FALSE;
  canOnlyCallRegisteredEP = FALSE;
  canOnlyAnswerRegisteredEP = FALSE;
  answerCallPreGrantedARQ = FALSE;
  makeCallPreGrantedARQ = FALSE;
  isGatekeeperRouted = FALSE;
  aliasCanBeHostName = TRUE;
  requireH235 = FALSE;
  disengageOnHearbeatFail = TRUE;

  identifierBase = time(NULL);
  nextIdentifier = 1;

  peakRegistrations = 0;
  totalRegistrations = 0;
  rejectedRegistrations = 0;
  peakCalls = 0;
  totalCalls = 0;
  rejectedCalls = 0;

  monitorThread = PThread::Create(PCREATE_NOTIFIER(MonitorMain), 0,
                                  PThread::NoAutoDeleteThread,
                                  PThread::NormalPriority,
                                  "GkSrv Monitor");

#ifdef H323_H501
  peerElement = NULL;
#endif
}


H323GatekeeperServer::~H323GatekeeperServer()
{
  monitorExit.Signal();
  PAssert(monitorThread->WaitForTermination(10000), "Gatekeeper monitor thread did not terminate!");
  delete monitorThread;

#ifdef H323_H501
  delete peerElement;
#endif
}


H323Transactor * H323GatekeeperServer::CreateListener(H323Transport * transport)
{
  return new H323GatekeeperListener(ownerEndPoint, *this, gatekeeperIdentifier, transport);
}


H323GatekeeperRequest::Response H323GatekeeperServer::OnDiscovery(H323GatekeeperGRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperServer::OnDiscovery");

  H235Authenticators authenticators = ownerEndPoint.CreateAuthenticators();
  for (PINDEX auth = 0; auth < authenticators.GetSize(); auth++) {
    for (PINDEX cap = 0; cap < info.grq.m_authenticationCapability.GetSize(); cap++) {
      for (PINDEX alg = 0; alg < info.grq.m_algorithmOIDs.GetSize(); alg++) {
        if (authenticators[auth].IsCapability(info.grq.m_authenticationCapability[cap],
                                              info.grq.m_algorithmOIDs[alg])) {
          PTRACE(3, "RAS\tGRQ accepted on " << H323TransportAddress(info.gcf.m_rasAddress)
                 << " using authenticator " << authenticators[auth]);
          info.gcf.IncludeOptionalField(H225_GatekeeperConfirm::e_authenticationMode);
          info.gcf.m_authenticationMode = info.grq.m_authenticationCapability[cap];
          info.gcf.IncludeOptionalField(H225_GatekeeperConfirm::e_algorithmOID);
          info.gcf.m_algorithmOID = info.grq.m_algorithmOIDs[alg];
          return H323GatekeeperRequest::Confirm;
        }
      }
    }
  }

  PTRACE(3, "RAS\tGRQ accepted on " << H323TransportAddress(info.gcf.m_rasAddress));
  return H323GatekeeperRequest::Confirm;
}


H323GatekeeperRequest::Response H323GatekeeperServer::OnRegistration(H323GatekeeperRRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperServer::OnRegistration");

  PINDEX i;

  // Initialise reply with default stuff
  info.rcf.IncludeOptionalField(H225_RegistrationConfirm::e_preGrantedARQ);
  info.rcf.m_preGrantedARQ.m_answerCall = answerCallPreGrantedARQ;
  info.rcf.m_preGrantedARQ.m_useGKCallSignalAddressToAnswer = answerCallPreGrantedARQ && isGatekeeperRouted;
  info.rcf.m_preGrantedARQ.m_makeCall = makeCallPreGrantedARQ;
  info.rcf.m_preGrantedARQ.m_useGKCallSignalAddressToMakeCall = makeCallPreGrantedARQ && isGatekeeperRouted;
  info.rcf.m_willRespondToIRR = TRUE;

  if (defaultInfoResponseRate > 0 && info.rrq.m_protocolIdentifier[5] > 2) {
    info.rcf.m_preGrantedARQ.IncludeOptionalField(H225_RegistrationConfirm_preGrantedARQ::e_irrFrequencyInCall);
    info.rcf.m_preGrantedARQ.m_irrFrequencyInCall = defaultInfoResponseRate;
  }

  if (info.rrq.m_keepAlive) {
    if (info.endpoint != NULL)
      return info.endpoint->OnRegistration(info);

    info.SetRejectReason(H225_RegistrationRejectReason::e_fullRegistrationRequired);
    PTRACE(2, "RAS\tRRQ keep alive rejected, not registered");
    return H323GatekeeperRequest::Reject;
  }

  for (i = 0; i < info.rrq.m_callSignalAddress.GetSize(); i++) {
    PSafePtr<H323RegisteredEndPoint> ep2 = FindEndPointBySignalAddress(info.rrq.m_callSignalAddress[i]);
    if (ep2 != NULL && ep2 != info.endpoint) {
      if (overwriteOnSameSignalAddress) {
        PTRACE(2, "RAS\tOverwriting existing endpoint " << *ep2);
        RemoveEndPoint(ep2);
      }
      else {
        info.SetRejectReason(H225_RegistrationRejectReason::e_invalidCallSignalAddress);
        PTRACE(2, "RAS\tRRQ rejected, duplicate callSignalAddress");
        return H323GatekeeperRequest::Reject;
      }
    }
  }

  if (info.rrq.HasOptionalField(H225_RegistrationRequest::e_terminalAlias) && !AllowDuplicateAlias(info.rrq.m_terminalAlias)) {
    H225_ArrayOf_AliasAddress duplicateAliases;
    for (i = 0; i < info.rrq.m_terminalAlias.GetSize(); i++) {
      PSafePtr<H323RegisteredEndPoint> ep2 = FindEndPointByAliasAddress(info.rrq.m_terminalAlias[i]);
      if (ep2 != NULL && ep2 != info.endpoint) {
        PINDEX sz = duplicateAliases.GetSize();
        duplicateAliases.SetSize(sz+1);
	      duplicateAliases[sz] = info.rrq.m_terminalAlias[i];
      }
    }
    if (duplicateAliases.GetSize() > 0) {
      info.SetRejectReason(H225_RegistrationRejectReason::e_duplicateAlias);
      H225_ArrayOf_AliasAddress & reasonAliases = info.rrj.m_rejectReason;
      reasonAliases = duplicateAliases;
      PTRACE(2, "RAS\tRRQ rejected, duplicate alias");
      return H323GatekeeperRequest::Reject;
    }
  }

  // Check if the endpoint is trying to register a prefix that can be resolved to another endpoint
  const H225_EndpointType & terminalType = info.rrq.m_terminalType;
  if (terminalType.HasOptionalField(H225_EndpointType::e_gateway) &&
      terminalType.m_gateway.HasOptionalField(H225_GatewayInfo::e_protocol)) {
    const H225_ArrayOf_SupportedProtocols & protocols = terminalType.m_gateway.m_protocol;

    for (i = 0; i < protocols.GetSize(); i++) {

      // Only voice prefixes are supported
      if (protocols[i].GetTag() == H225_SupportedProtocols::e_voice) {
  	    H225_VoiceCaps & voiceCaps = protocols[i];
	      if (voiceCaps.HasOptionalField(H225_VoiceCaps::e_supportedPrefixes)) {
	        H225_ArrayOf_SupportedPrefix & prefixes = voiceCaps.m_supportedPrefixes;
	        for (PINDEX j = 0; j < prefixes.GetSize(); j++) {

	          // Reject if the prefix be matched to a registered alias or prefix
	          PSafePtr<H323RegisteredEndPoint> ep2 = FindEndPointByAliasAddress(prefixes[j].m_prefix);
	          if (ep2 != NULL && ep2 != info.endpoint && !canHaveDuplicatePrefix) {
	            info.SetRejectReason(H225_RegistrationRejectReason::e_duplicateAlias);
              H225_ArrayOf_AliasAddress & aliases = info.rrj.m_rejectReason;
              aliases.SetSize(1);
	            aliases[0] = prefixes[j].m_prefix;
	            PTRACE(2, "RAS\tRRQ rejected, duplicate prefix");
	            return H323GatekeeperRequest::Reject;
      	    }
          }
	      }
	      break;  // If voice protocol is found, don't look any further
      }
    }
  }

  // Are already registered and have just sent another heavy RRQ
  if (info.endpoint != NULL) {
    H323GatekeeperRequest::Response response = info.endpoint->OnRegistration(info);
    switch (response) {
      case H323GatekeeperRequest::Confirm :
        AddEndPoint(info.endpoint);
        break;
      case H323GatekeeperRequest::Reject :
        RemoveEndPoint(info.endpoint);
        break;
      default :
        break;
    }
    return response;
  }

  // Need to create a new endpoint object
  info.endpoint = CreateRegisteredEndPoint(info);
  if (info.endpoint == (H323RegisteredEndPoint *)NULL) {
    PTRACE(1, "RAS\tRRQ rejected, CreateRegisteredEndPoint() returned NULL");
    return H323GatekeeperRequest::Reject;
  }

  H323GatekeeperRequest::Response response = info.endpoint->OnRegistration(info);
  if (response != H323GatekeeperRequest::Confirm) {
    info.endpoint = (H323RegisteredEndPoint *)NULL;
    delete info.endpoint;
    return response;
  }

  // Have successfully registered, save it
  AddEndPoint(info.endpoint);

  PTRACE(2, "RAS\tRRQ accepted: \"" << *info.endpoint << '"');
  return H323GatekeeperRequest::Confirm;
}


H323GatekeeperRequest::Response H323GatekeeperServer::OnUnregistration(H323GatekeeperURQ & info)
{
  PTRACE_BLOCK("H323GatekeeperServer::OnUnregistration");

  H323GatekeeperRequest::Response response = info.endpoint->OnUnregistration(info);
  if (response != H323GatekeeperRequest::Confirm)
    return response;

  if (info.urq.HasOptionalField(H225_UnregistrationRequest::e_endpointAlias)) {
    PINDEX i;
    // See if all aliases to be removed are on the same endpoint
    for (i = 0; i < info.urq.m_endpointAlias.GetSize(); i++) {
      if (FindEndPointByAliasAddress(info.urq.m_endpointAlias[i]) != info.endpoint) {
        info.SetRejectReason(H225_UnregRejectReason::e_permissionDenied);
        PTRACE(2, "RAS\tURQ rejected, alias " << info.urq.m_endpointAlias[i]
               << " not owned by registration");
        return H323GatekeeperRequest::Reject;
      }
    }

    // Remove all the aliases specified in PDU
    for (i = 0; i < info.urq.m_endpointAlias.GetSize(); i++)
      info.endpoint->RemoveAlias(H323GetAliasAddressString(info.urq.m_endpointAlias[i]));

    // if no aliases left, then remove the endpoint
    if (info.endpoint->GetAliasCount() > 0) {
#ifdef H323_H501
      if (peerElement != NULL)
        peerElement->AddDescriptor(info.endpoint->GetDescriptorID(),
                                   info.endpoint->GetAliases(),
                                   info.endpoint->GetSignalAddresses());
#endif
    } else {
      PTRACE(2, "RAS\tRemoving endpoint " << *info.endpoint << " with no aliases");
      RemoveEndPoint(info.endpoint);  // will also remove descriptor if required
    }
  }
  else
    RemoveEndPoint(info.endpoint);

  return H323GatekeeperRequest::Confirm;
}


H323GatekeeperRequest::Response H323GatekeeperServer::OnInfoResponse(H323GatekeeperIRR & info)
{
  PTRACE_BLOCK("H323GatekeeperServer::OnInfoResponse");

  return info.endpoint->OnInfoResponse(info);
}


void H323GatekeeperServer::AddEndPoint(H323RegisteredEndPoint * ep)
{
  PTRACE(3, "RAS\tAdding registered endpoint: " << *ep);

  PINDEX i;

  mutex.Wait();

  if (byIdentifier.FindWithLock(ep->GetIdentifier(), PSafeReference) != ep) {
    byIdentifier.SetAt(ep->GetIdentifier(), ep);

    if (byIdentifier.GetSize() > peakRegistrations)
      peakRegistrations = byIdentifier.GetSize();
    totalRegistrations++;
  }

  for (i = 0; i < ep->GetSignalAddressCount(); i++)
    byAddress.Append(new StringMap(ep->GetSignalAddress(i), ep->GetIdentifier()));

  for (i = 0; i < ep->GetAliasCount(); i++) {
    PString alias = ep->GetAlias(i);
    byAlias.Append(new StringMap(ep->GetAlias(i), ep->GetIdentifier()));
  }

  for (i = 0; i < ep->GetPrefixCount(); i++)
    byVoicePrefix.Append(new StringMap(ep->GetPrefix(i), ep->GetIdentifier()));

  mutex.Signal();
}


PBoolean H323GatekeeperServer::RemoveEndPoint(H323RegisteredEndPoint * ep)
{
  PTRACE(3, "RAS\tRemoving registered endpoint: " << *ep);

  // clear all calls in the endpoint
  while (ep->GetCallCount() > 0)
    RemoveCall(&ep->GetCall(0));

  // remove any aliases from the endpoint
  while (ep->GetAliasCount() > 0)
    ep->RemoveAlias(ep->GetAlias(0));

  PWaitAndSignal wait(mutex);

  PINDEX i;

  // remove prefixes belonging to this endpoint
  for (i = 0; i < byVoicePrefix.GetSize(); i++) {
    StringMap & prefixMap = (StringMap &)*byVoicePrefix.GetAt(i);
    if (prefixMap.identifier == ep->GetIdentifier())
      byVoicePrefix.RemoveAt(i);
  }

  // remove aliases belonging to this endpoint
  for (i = 0; i < byAlias.GetSize(); i++) {
    StringMap & aliasMap = (StringMap &)*byAlias.GetAt(i);
    if (aliasMap.identifier == ep->GetIdentifier())
      byAlias.RemoveAt(i);
  }

  // remove call signalling addresses
  for (i = 0; i < byAddress.GetSize(); i++) {
    StringMap & aliasMap = (StringMap &)*byAddress.GetAt(i);
    if (aliasMap.identifier == ep->GetIdentifier())
      byAddress.RemoveAt(i);
  }

  // remove the descriptor
#ifdef H323_H501
  if (peerElement != NULL)
    peerElement->DeleteDescriptor(ep->GetDescriptorID());
#endif

  // remove the endpoint from the list of active endpoints
  // ep is deleted by this
  return byIdentifier.RemoveAt(ep->GetIdentifier());
}


void H323GatekeeperServer::RemoveAlias(H323RegisteredEndPoint & ep,
                                                const PString & alias)
{
  PTRACE(3, "RAS\tRemoving registered endpoint alias: " << alias);

  mutex.Wait();

  PINDEX pos = byAlias.GetValuesIndex(alias);
  if (pos != P_MAX_INDEX) {
    // Allow for possible multiple aliases
    while (pos < byAlias.GetSize()) {
      StringMap & aliasMap = (StringMap &)byAlias[pos];
      if (aliasMap != alias)
        break;

      if (aliasMap.identifier == ep.GetIdentifier())
        byAlias.RemoveAt(pos);
      else
        pos++;
    }
  }

  if (ep.ContainsAlias(alias))
    ep.RemoveAlias(alias);

  mutex.Signal();
}


H323RegisteredEndPoint * H323GatekeeperServer::CreateRegisteredEndPoint(H323GatekeeperRRQ &)
{
  return new H323RegisteredEndPoint(*this, CreateEndPointIdentifier());
}


PString H323GatekeeperServer::CreateEndPointIdentifier()
{
  PStringStream id;
  PWaitAndSignal wait(mutex);
  id << hex << identifierBase << ':' << nextIdentifier++;
  return id;
}


PSafePtr<H323RegisteredEndPoint> H323GatekeeperServer::FindEndPointByIdentifier(
                                            const PString & identifier, PSafetyMode mode)
{
  return byIdentifier.FindWithLock(identifier, mode);
}


PSafePtr<H323RegisteredEndPoint> H323GatekeeperServer::FindEndPointBySignalAddresses(
                            const H225_ArrayOf_TransportAddress & addresses, PSafetyMode mode)
{
  PWaitAndSignal wait(mutex);

  for (PINDEX i = 0; i < addresses.GetSize(); i++) {
    PINDEX pos = byAddress.GetValuesIndex(H323TransportAddress(addresses[i]));
    if (pos != P_MAX_INDEX)
      return FindEndPointByIdentifier(((StringMap &)byAddress[pos]).identifier, mode);
  }

  return (H323RegisteredEndPoint *)NULL;
}


PSafePtr<H323RegisteredEndPoint> H323GatekeeperServer::FindEndPointBySignalAddress(
                                     const H323TransportAddress & address, PSafetyMode mode)
{
  PWaitAndSignal wait(mutex);

  PINDEX pos = byAddress.GetValuesIndex(address);
  if (pos != P_MAX_INDEX)
    return FindEndPointByIdentifier(((StringMap &)byAddress[pos]).identifier, mode);

  return (H323RegisteredEndPoint *)NULL;
}


PSafePtr<H323RegisteredEndPoint> H323GatekeeperServer::FindEndPointByAliasAddress(
                                         const H225_AliasAddress & alias, PSafetyMode mode)
{
  return FindEndPointByAliasString(H323GetAliasAddressString(alias), mode);
}


PSafePtr<H323RegisteredEndPoint> H323GatekeeperServer::FindEndPointByAliasString(
                                                  const PString & alias, PSafetyMode mode)
{
  {
    PWaitAndSignal wait(mutex);
    PINDEX pos = byAlias.GetValuesIndex(alias);

    if (pos != P_MAX_INDEX)
      return FindEndPointByIdentifier(((StringMap &)byAlias[pos]).identifier, mode);
  }

  return FindEndPointByPrefixString(alias, mode);
}


PSafePtr<H323RegisteredEndPoint> H323GatekeeperServer::FindEndPointByPartialAlias(
                                                  const PString & alias, PSafetyMode mode)
{
  PWaitAndSignal wait(mutex);
  PINDEX pos = byAlias.GetNextStringsIndex(alias);

  if (pos != P_MAX_INDEX) {
    StringMap & possible = (StringMap &)byAlias[pos];
    if (possible.NumCompare(alias) == EqualTo) {
      PTRACE(4, "RAS\tPartial endpoint search for "
                "\"" << alias << "\" found \"" << possible << '"');
      return FindEndPointByIdentifier(possible.identifier, mode);
    }
  }

  PTRACE(4, "RAS\tPartial endpoint search for \"" << alias << "\" failed");
  return (H323RegisteredEndPoint *)NULL;
}


PSafePtr<H323RegisteredEndPoint> H323GatekeeperServer::FindEndPointByPrefixString(
                                                  const PString & prefix, PSafetyMode mode)
{
  PWaitAndSignal wait(mutex);

  if (byVoicePrefix.IsEmpty())
    return (H323RegisteredEndPoint *)NULL;

  for (PINDEX len = prefix.GetLength(); len > 0; len--) {
    PINDEX pos = byVoicePrefix.GetValuesIndex(prefix.Left(len));
    if (pos != P_MAX_INDEX)
      return FindEndPointByIdentifier(((StringMap &)byVoicePrefix[pos]).identifier, mode);
  }

  return (H323RegisteredEndPoint *)NULL;
}


PSafePtr<H323RegisteredEndPoint> H323GatekeeperServer::FindDestinationEndPoint(
                                         const OpalGloballyUniqueID & id,
                                         H323GatekeeperCall::Direction direction)
{
  if ( !id ) {
    PSafePtr<H323GatekeeperCall> call = FindCall(id, direction);
    if (call == NULL)
      return NULL;

    for (PINDEX i = 0; i < call->GetDestinationAliases().GetSize(); i++) {
      const PString alias = call->GetDestinationAliases()[i];
      PSafePtr<H323RegisteredEndPoint> ep = FindEndPointByAliasString(alias);
      if (ep != NULL)
        return ep;
    }
  }

  return NULL;
}


H323GatekeeperRequest::Response H323GatekeeperServer::OnAdmission(H323GatekeeperARQ & info)
{
  PTRACE_BLOCK("H323GatekeeperServer::OnAdmission");

  OpalGloballyUniqueID id = info.arq.m_callIdentifier.m_guid;
  if (id == NULL) {
    PTRACE(2, "RAS\tNo call identifier provided in ARQ!");
    info.SetRejectReason(H225_AdmissionRejectReason::e_undefinedReason);
    return H323GatekeeperRequest::Reject;
  }

  H323GatekeeperRequest::Response response;

  PSafePtr<H323GatekeeperCall> oldCall = FindCall(id, info.arq.m_answerCall);
  if (oldCall != NULL)
    response = oldCall->OnAdmission(info);
  else {
    // If on restarted in thread, then don't create new call, should already
    // have had one created on the last pass through.
    if (!info.IsFastResponseRequired() && info.CanSendRIP()) {
      PTRACE(2, "RAS\tCall object disappeared after starting slow PDU handler thread!");
      info.SetRejectReason(H225_AdmissionRejectReason::e_undefinedReason);
      return H323GatekeeperRequest::Reject;
    }

    H323GatekeeperCall * newCall = CreateCall(id,
                            info.arq.m_answerCall ? H323GatekeeperCall::AnsweringCall
                                                  : H323GatekeeperCall::OriginatingCall);
    PTRACE(3, "RAS\tCall created: " << *newCall);

    response = newCall->OnAdmission(info);

    if (response != H323GatekeeperRequest::Reject) {
      mutex.Wait();

      info.endpoint->AddCall(newCall);
      oldCall = activeCalls.Append(newCall);

      if (activeCalls.GetSize() > peakCalls)
        peakCalls = activeCalls.GetSize();
      totalCalls++;

      PTRACE(2, "RAS\tAdded new call (total=" << activeCalls.GetSize() << ") " << *newCall);
      mutex.Signal();

      AddCall(oldCall);
    } else {
      delete newCall;
    }
  }

#ifdef H323_H248
  switch (response) {
    case H323GatekeeperRequest::Confirm :
      if (oldCall->AddCallCreditServiceControl(info.acf.m_serviceControl))
        info.acf.IncludeOptionalField(H225_AdmissionConfirm::e_serviceControl);
      break;

    case H323GatekeeperRequest::Reject :
      if (oldCall != NULL && oldCall->AddCallCreditServiceControl(info.arj.m_serviceControl))
        info.arj.IncludeOptionalField(H225_AdmissionReject::e_serviceControl);
      break;

    default :
      break;
  }
#endif

  return response;
}


H323GatekeeperRequest::Response H323GatekeeperServer::OnDisengage(H323GatekeeperDRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperServer::OnDisengage");

  OpalGloballyUniqueID callIdentifier = info.drq.m_callIdentifier.m_guid;
  PSafePtr<H323GatekeeperCall> call = FindCall(callIdentifier, info.drq.m_answeredCall);
  if (call == NULL) {
    info.SetRejectReason(H225_DisengageRejectReason::e_requestToDropOther);
    PTRACE(2, "RAS\tDRQ rejected, no call with ID " << callIdentifier);
    return H323GatekeeperRequest::Reject;
  }

  H323GatekeeperRequest::Response response = call->OnDisengage(info);
  if (response != H323GatekeeperRequest::Confirm)
    return response;

  RemoveCall(call);

  return H323GatekeeperRequest::Confirm;
}


H323GatekeeperRequest::Response H323GatekeeperServer::OnBandwidth(H323GatekeeperBRQ & info)
{
  PTRACE_BLOCK("H323GatekeeperServer::OnBandwidth");

  PSafePtr<H323GatekeeperCall> call = FindCall(info.brq.m_callIdentifier.m_guid, info.brq.m_answeredCall);
  if (call == NULL) {
    info.SetRejectReason(H225_BandRejectReason::e_invalidConferenceID);
    PTRACE(2, "RAS\tBRQ rejected, no call with ID");
    return H323GatekeeperRequest::Reject;
  }

  H323GatekeeperRequest::Response response = call->OnBandwidth(info);
  return response;
}


PBoolean H323GatekeeperServer::GetAdmissionRequestAuthentication(H323GatekeeperARQ & /*info*/,
                                                             H235Authenticators & /*authenticators*/)
{
  return FALSE;
}

PBoolean H323GatekeeperServer::GetUsersPassword(const PString & alias,
                                                  PString & password,
                                   H323RegisteredEndPoint & /*registerdEndpoint*/) const
{
  return GetUsersPassword(alias, password);
}

PBoolean H323GatekeeperServer::GetUsersPassword(const PString & alias, PString & password) const
{
  if (!passwords.Contains(alias))
    return FALSE;

  password = passwords(alias);
  return TRUE;
}


unsigned H323GatekeeperServer::AllocateBandwidth(unsigned newBandwidth,
                                                 unsigned oldBandwidth)
{
  PWaitAndSignal wait(mutex);

  // If first request for bandwidth, then only give them a maximum of the
  // configured default bandwidth
  if (oldBandwidth == 0 && newBandwidth > defaultBandwidth)
    newBandwidth = defaultBandwidth;

  // If then are asking for more than we have in total, drop it down to whatevers left
  if (newBandwidth > oldBandwidth && (newBandwidth - oldBandwidth) > (totalBandwidth - usedBandwidth))
    newBandwidth = totalBandwidth - usedBandwidth - oldBandwidth;

  // If greater than the absolute maximum configured for any endpoint, clamp it
  if (newBandwidth > maximumBandwidth)
    newBandwidth = maximumBandwidth;

  // Finally have adjusted new bandwidth, allocate it!
  usedBandwidth += (newBandwidth - oldBandwidth);

  PTRACE(3, "RAS\tBandwidth allocation: +" << newBandwidth << " -" << oldBandwidth
         << " used=" << usedBandwidth << " left=" << (totalBandwidth - usedBandwidth));
  return newBandwidth;
}


void H323GatekeeperServer::RemoveCall(H323GatekeeperCall * call)
{
  if (PAssertNULL(call) == NULL)
    return;

  call->SetBandwidthUsed(0);
  PAssert(call->GetEndPoint().RemoveCall(call), PLogicError);

  PTRACE(2, "RAS\tRemoved call (total=" << (activeCalls.GetSize()-1) << ") id=" << *call);
  PAssert(activeCalls.Remove(call), PLogicError);
}


H323GatekeeperCall * H323GatekeeperServer::CreateCall(const OpalGloballyUniqueID & id,
                                                      H323GatekeeperCall::Direction dir)
{
  return new H323GatekeeperCall(*this, id, dir);
}


PSafePtr<H323GatekeeperCall> H323GatekeeperServer::FindCall(const PString & desc,
                                                            PSafetyMode mode)
{
  PINDEX pos = desc.Find(AnswerCallStr);
  if (pos == P_MAX_INDEX)
    pos = desc.Find(OriginateCallStr);

  OpalGloballyUniqueID id = desc.Left(pos);

  H323GatekeeperCall::Direction dir = H323GatekeeperCall::UnknownDirection;

  PString dirStr = desc.Mid(pos);
  if (dirStr == AnswerCallStr)
    dir = H323GatekeeperCall::AnsweringCall;
  else if (dirStr == OriginateCallStr)
    dir = H323GatekeeperCall::OriginatingCall;

  return FindCall(id, dir, mode);
}


PSafePtr<H323GatekeeperCall> H323GatekeeperServer::FindCall(const OpalGloballyUniqueID & id,
                                                            PBoolean answer,
                                                            PSafetyMode mode)
{
  return FindCall(id, answer ? H323GatekeeperCall::AnsweringCall
                             : H323GatekeeperCall::OriginatingCall, mode);
}


PSafePtr<H323GatekeeperCall> H323GatekeeperServer::FindCall(const OpalGloballyUniqueID & id,
                                                            H323GatekeeperCall::Direction dir,
                                                            PSafetyMode mode)
{
  return activeCalls.FindWithLock(H323GatekeeperCall(*this, id, dir), mode);
}


H323GatekeeperRequest::Response H323GatekeeperServer::OnLocation(H323GatekeeperLRQ & info)
{
  PINDEX i;
  for (i = 0; i < info.lrq.m_destinationInfo.GetSize(); i++) {
    PSafePtr<H323RegisteredEndPoint> ep =
                       FindEndPointByAliasAddress(info.lrq.m_destinationInfo[i], PSafeReadOnly);
    if (ep != NULL) {
      ep->GetSignalAddress(0).SetPDU(info.lcf.m_callSignalAddress);
      ep->GetRASAddress(0).SetPDU(info.lcf.m_rasAddress);
      PTRACE(2, "RAS\tLocation of " << H323GetAliasAddressString(info.lrq.m_destinationInfo[i])
             << " is endpoint " << *ep);
      return H323GatekeeperRequest::Confirm;
    }
  }

  PBoolean isGKRouted = IsGatekeeperRouted();

  for (i = 0; i < info.lrq.m_destinationInfo.GetSize(); i++) {
    H323TransportAddress address;
    if (TranslateAliasAddress(info.lrq.m_destinationInfo[i],
                              info.lcf.m_destinationInfo,
                              address,
                              isGKRouted,
                              NULL)) {
      address.SetPDU(info.lcf.m_callSignalAddress);
      if (info.lcf.m_destinationInfo.GetSize() > 0)
        info.lcf.IncludeOptionalField(H225_LocationConfirm::e_destinationInfo);

      PTRACE(2, "RAS\tLocation of " << H323GetAliasAddressString(info.lrq.m_destinationInfo[i])
             << " is " << address);
      return H323GatekeeperRequest::Confirm;
    }
  }

  info.SetRejectReason(H225_LocationRejectReason::e_requestDenied);
  PTRACE(2, "RAS\tLRQ rejected, location not found");
  return H323GatekeeperRequest::Reject;
}


PBoolean H323GatekeeperServer::TranslateAliasAddress(const H225_AliasAddress & alias,
                                                 H225_ArrayOf_AliasAddress & aliases,
                                                 H323TransportAddress & address,
                                                 PBoolean & /*isGKRouted*/,
                                                 H323GatekeeperCall * /*call*/)
{
  if (!TranslateAliasAddressToSignalAddress(alias, address)) {
#ifdef H323_H501
    H225_AliasAddress transportAlias;
    if ((peerElement != NULL) && (peerElement->AccessRequest(alias, aliases, transportAlias))) {
      // if AccessRequest returns OK, but no aliases, then all of the aliases
      // must have been wildcards. In this case, add the original aliase back into the list
      if (aliases.GetSize() == 0) {
        PTRACE(1, "RAS\tAdding original alias to the top of the alias list");
        aliases.SetSize(1);
        aliases[0] = alias;
      }
      address = H323GetAliasAddressString(transportAlias);
      return TRUE;
    }
#endif
    return FALSE;
  }

  PSafePtr<H323RegisteredEndPoint> ep = FindEndPointBySignalAddress(address, PSafeReadOnly);
  if (ep != NULL)
    H323SetAliasAddresses(ep->GetAliases(), aliases);

  return TRUE;
}

PBoolean H323GatekeeperServer::TranslateAliasAddressToSignalAddress(const H225_AliasAddress & alias,
                                                                H323TransportAddress & address)
{
  PWaitAndSignal wait(mutex);

  PString aliasString = H323GetAliasAddressString(alias);

  if (isGatekeeperRouted) {
    const H323ListenerList & listeners = ownerEndPoint.GetListeners();
    address = listeners[0].GetTransportAddress();
    PTRACE(2, "RAS\tTranslating alias " << aliasString << " to " << address << ", gatekeeper routed");
    return TRUE;
  }

  PSafePtr<H323RegisteredEndPoint> ep = FindEndPointByAliasAddress(alias, PSafeReadOnly);
  if (ep != NULL) {
    address = ep->GetSignalAddress(0);
    PTRACE(2, "RAS\tTranslating alias " << aliasString << " to " << address << ", registered endpoint");
    return TRUE;
  }

  if (!aliasCanBeHostName)
    return FALSE;

  // If is E.164 address then assume isn't a host name or IP address
  if (!H323GetAliasAddressE164(alias).IsEmpty())
    return FALSE;

  H323TransportAddress aliasAsTransport = aliasString;
  PIPSocket::Address ip;
  WORD port = H323EndPoint::DefaultTcpPort;
  if (!aliasAsTransport.GetIpAndPort(ip, port)) {
    PTRACE(4, "RAS\tCould not translate " << aliasString << " as host name.");
    return FALSE;
  }

  address = H323TransportAddress(ip, port);
  PTRACE(2, "RAS\tTranslating alias " << aliasString << " to " << address << ", host name");
  return TRUE;
}


PBoolean H323GatekeeperServer::CheckSignalAddressPolicy(const H323RegisteredEndPoint &,
                                                    const H225_AdmissionRequest &,
                                                    const H323TransportAddress &)
{
  return TRUE;
}


PBoolean H323GatekeeperServer::CheckAliasAddressPolicy(const H323RegisteredEndPoint &,
                                                   const H225_AdmissionRequest & arq,
                                                   const H225_AliasAddress & alias)
{
  PWaitAndSignal wait(mutex);

  if (arq.m_answerCall ? canOnlyAnswerRegisteredEP : canOnlyCallRegisteredEP) {
    PSafePtr<H323RegisteredEndPoint> ep = FindEndPointByAliasAddress(alias);
    if (ep == NULL)
      return FALSE;
  }

  return TRUE;
}


PBoolean H323GatekeeperServer::CheckAliasStringPolicy(const H323RegisteredEndPoint &,
                                                  const H225_AdmissionRequest & arq,
                                                  const PString & alias)
{
  PWaitAndSignal wait(mutex);

  if (arq.m_answerCall ? canOnlyAnswerRegisteredEP : canOnlyCallRegisteredEP) {
    PSafePtr<H323RegisteredEndPoint> ep = FindEndPointByAliasString(alias);
    if (ep == NULL)
      return FALSE;
  }

  return TRUE;
}


void H323GatekeeperServer::SetGatekeeperIdentifier(const PString & id,
                                                   PBoolean adjustListeners)
{
  mutex.Wait();

  gatekeeperIdentifier = id;

  if (adjustListeners) {
    for (PINDEX i = 0; i < listeners.GetSize(); i++)
      listeners[i].SetIdentifier(id);
  }

  mutex.Signal();
}

#ifdef H323_H501

void H323GatekeeperServer::SetPeerElement(H323PeerElement * newPeerElement)
{
  delete peerElement;
  peerElement = newPeerElement;
}


void H323GatekeeperServer::CreatePeerElement(const H323TransportAddress & h501Interface)
{
  if (peerElement == NULL)
    peerElement = new H323PeerElement(ownerEndPoint, h501Interface);
  else
    peerElement->SetTransport(h501Interface);
}


PBoolean H323GatekeeperServer::OpenPeerElement(const H323TransportAddress & remotePeer,
                                           PBoolean append, PBoolean keepTrying)
{
  if (peerElement == NULL)
    peerElement = new H323PeerElement(ownerEndPoint);

  if (append)
    return peerElement->AddServiceRelationship(remotePeer, keepTrying);
  else
    return peerElement->SetOnlyServiceRelationship(remotePeer, keepTrying);
}

#endif // H323_H501

void H323GatekeeperServer::MonitorMain(PThread &, H323_INT)
{
  while (!monitorExit.Wait(1000)) {
    PTRACE(6, "RAS\tAging registered endpoints");

    for (PSafePtr<H323RegisteredEndPoint> ep = GetFirstEndPoint(PSafeReference); ep != NULL; ep++) {
      if (!ep->OnTimeToLive()) {
        PTRACE(2, "RAS\tRemoving expired endpoint " << *ep);
        RemoveEndPoint(ep);
      }
      if (ep->GetAliasCount() == 0) {
        PTRACE(2, "RAS\tRemoving endpoint " << *ep << " with no aliases");
        RemoveEndPoint(ep);
      }
    }

    byIdentifier.DeleteObjectsToBeRemoved();

    for (PSafePtr<H323GatekeeperCall> call = GetFirstCall(PSafeReference); call != NULL; call++) {
      if (!call->OnHeartbeat()) {
        if (disengageOnHearbeatFail)
          call->Disengage();
      }
    }

    activeCalls.DeleteObjectsToBeRemoved();
  }
}

PBoolean H323GatekeeperServer::OnSendFeatureSet(unsigned, H225_FeatureSet &, PBoolean) const
{
  return FALSE;
}

void H323GatekeeperServer::OnReceiveFeatureSet(unsigned, const H225_FeatureSet &) const
{
}


/////////////////////////////////////////////////////////////////////////////

