/*
 * H225_RAS.cxx
 *
 * H.225 RAS protocol handler
 *
 * Open H323 Library
 *
 * Copyright (c) 2001 Equivalence Pty. Ltd.
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
 * $Id $
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "h225ras.h"
#endif

#include "h323.h"

#include "h225ras.h"

#include "h323ep.h"
#include "h323pdu.h"
#include "h235auth.h"

#ifdef H323_H460
#include "h460/h460.h"
#endif

#define new PNEW


/////////////////////////////////////////////////////////////////////////////

H225_RAS::H225_RAS(H323EndPoint & ep, H323Transport * trans)
  : H323Transactor(ep, trans, DefaultRasUdpPort, DefaultRasUdpPort)
{
}


H225_RAS::~H225_RAS()
{
  StopChannel();
}


void H225_RAS::PrintOn(ostream & strm) const
{
  if (gatekeeperIdentifier.IsEmpty())
    strm << "H225-RAS@";
  else
    strm << gatekeeperIdentifier << '@';
  H323Transactor::PrintOn(strm);
}


H323TransactionPDU * H225_RAS::CreateTransactionPDU() const
{
  return new H323RasPDU;
}


PBoolean H225_RAS::HandleTransaction(const PASN_Object & rawPDU)
{
  const H323RasPDU & pdu = (const H323RasPDU &)rawPDU;

  switch (pdu.GetTag()) {
    case H225_RasMessage::e_gatekeeperRequest :
      if (SendCachedResponse(pdu))
        return FALSE;
      OnReceiveGatekeeperRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_gatekeeperConfirm :
      return OnReceiveGatekeeperConfirm(pdu, pdu);

    case H225_RasMessage::e_gatekeeperReject :
      return OnReceiveGatekeeperReject(pdu, pdu);

    case H225_RasMessage::e_registrationRequest :
      if (SendCachedResponse(pdu))
        return FALSE;
      OnReceiveRegistrationRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_registrationConfirm :
      return OnReceiveRegistrationConfirm(pdu, pdu);

    case H225_RasMessage::e_registrationReject :
      return OnReceiveRegistrationReject(pdu, pdu);

    case H225_RasMessage::e_unregistrationRequest :
      if (SendCachedResponse(pdu))
        return FALSE;
      OnReceiveUnregistrationRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_unregistrationConfirm :
      return OnReceiveUnregistrationConfirm(pdu, pdu);

    case H225_RasMessage::e_unregistrationReject :
      return OnReceiveUnregistrationReject(pdu, pdu);

    case H225_RasMessage::e_admissionRequest :
      if (SendCachedResponse(pdu))
        return FALSE;
      OnReceiveAdmissionRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_admissionConfirm :
      return OnReceiveAdmissionConfirm(pdu, pdu);

    case H225_RasMessage::e_admissionReject :
      return OnReceiveAdmissionReject(pdu, pdu);

    case H225_RasMessage::e_bandwidthRequest :
      if (SendCachedResponse(pdu))
        return FALSE;
      OnReceiveBandwidthRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_bandwidthConfirm :
      return OnReceiveBandwidthConfirm(pdu, pdu);

    case H225_RasMessage::e_bandwidthReject :
      return OnReceiveBandwidthReject(pdu, pdu);

    case H225_RasMessage::e_disengageRequest :
      if (SendCachedResponse(pdu))
        return FALSE;
      OnReceiveDisengageRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_disengageConfirm :
      return OnReceiveDisengageConfirm(pdu, pdu);

    case H225_RasMessage::e_disengageReject :
      return OnReceiveDisengageReject(pdu, pdu);

    case H225_RasMessage::e_locationRequest :
      if (SendCachedResponse(pdu))
        return FALSE;
      OnReceiveLocationRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_locationConfirm :
      return OnReceiveLocationConfirm(pdu, pdu);

    case H225_RasMessage::e_locationReject :
      return OnReceiveLocationReject(pdu, pdu);

    case H225_RasMessage::e_infoRequest :
      if (SendCachedResponse(pdu))
        return FALSE;
      OnReceiveInfoRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_infoRequestResponse :
      return OnReceiveInfoRequestResponse(pdu, pdu);

    case H225_RasMessage::e_nonStandardMessage :
      OnReceiveNonStandardMessage(pdu, pdu);
      break;

    case H225_RasMessage::e_unknownMessageResponse :
      OnReceiveUnknownMessageResponse(pdu, pdu);
      break;

    case H225_RasMessage::e_requestInProgress :
      return OnReceiveRequestInProgress(pdu, pdu);

    case H225_RasMessage::e_resourcesAvailableIndicate :
      if (SendCachedResponse(pdu))
        return FALSE;
      OnReceiveResourcesAvailableIndicate(pdu, pdu);
      break;

    case H225_RasMessage::e_resourcesAvailableConfirm :
      return OnReceiveResourcesAvailableConfirm(pdu, pdu);

    case H225_RasMessage::e_infoRequestAck :
      return OnReceiveInfoRequestAck(pdu, pdu);

    case H225_RasMessage::e_infoRequestNak :
      return OnReceiveInfoRequestNak(pdu, pdu);

    case H225_RasMessage::e_serviceControlIndication :
      if (SendCachedResponse(pdu))
        return FALSE;
      OnReceiveServiceControlIndication(pdu, pdu);
      break;

    case H225_RasMessage::e_serviceControlResponse :
      return OnReceiveServiceControlResponse(pdu, pdu);

    default :
      OnReceiveUnknown(pdu);
  }

  return FALSE;
}


void H225_RAS::OnSendingPDU(PASN_Object & rawPDU)
{
  H323RasPDU & pdu = (H323RasPDU &)rawPDU;

  switch (pdu.GetTag()) {
    case H225_RasMessage::e_gatekeeperRequest :
      OnSendGatekeeperRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_gatekeeperConfirm :
      OnSendGatekeeperConfirm(pdu, pdu);
      break;

    case H225_RasMessage::e_gatekeeperReject :
      OnSendGatekeeperReject(pdu, pdu);
      break;

    case H225_RasMessage::e_registrationRequest :
      OnSendRegistrationRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_registrationConfirm :
      OnSendRegistrationConfirm(pdu, pdu);
      break;

    case H225_RasMessage::e_registrationReject :
      OnSendRegistrationReject(pdu, pdu);
      break;

    case H225_RasMessage::e_unregistrationRequest :
      OnSendUnregistrationRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_unregistrationConfirm :
      OnSendUnregistrationConfirm(pdu, pdu);
      break;

    case H225_RasMessage::e_unregistrationReject :
      OnSendUnregistrationReject(pdu, pdu);
      break;

    case H225_RasMessage::e_admissionRequest :
      OnSendAdmissionRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_admissionConfirm :
      OnSendAdmissionConfirm(pdu, pdu);
      break;

    case H225_RasMessage::e_admissionReject :
      OnSendAdmissionReject(pdu, pdu);
      break;

    case H225_RasMessage::e_bandwidthRequest :
      OnSendBandwidthRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_bandwidthConfirm :
      OnSendBandwidthConfirm(pdu, pdu);
      break;

    case H225_RasMessage::e_bandwidthReject :
      OnSendBandwidthReject(pdu, pdu);
      break;

    case H225_RasMessage::e_disengageRequest :
      OnSendDisengageRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_disengageConfirm :
      OnSendDisengageConfirm(pdu, pdu);
      break;

    case H225_RasMessage::e_disengageReject :
      OnSendDisengageReject(pdu, pdu);
      break;

    case H225_RasMessage::e_locationRequest :
      OnSendLocationRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_locationConfirm :
      OnSendLocationConfirm(pdu, pdu);
      break;

    case H225_RasMessage::e_locationReject :
      OnSendLocationReject(pdu, pdu);
      break;

    case H225_RasMessage::e_infoRequest :
      OnSendInfoRequest(pdu, pdu);
      break;

    case H225_RasMessage::e_infoRequestResponse :
      OnSendInfoRequestResponse(pdu, pdu);
      break;

    case H225_RasMessage::e_nonStandardMessage :
      OnSendNonStandardMessage(pdu, pdu);
      break;

    case H225_RasMessage::e_unknownMessageResponse :
      OnSendUnknownMessageResponse(pdu, pdu);
      break;

    case H225_RasMessage::e_requestInProgress :
      OnSendRequestInProgress(pdu, pdu);
      break;

    case H225_RasMessage::e_resourcesAvailableIndicate :
      OnSendResourcesAvailableIndicate(pdu, pdu);
      break;

    case H225_RasMessage::e_resourcesAvailableConfirm :
      OnSendResourcesAvailableConfirm(pdu, pdu);
      break;

    case H225_RasMessage::e_infoRequestAck :
      OnSendInfoRequestAck(pdu, pdu);
      break;

    case H225_RasMessage::e_infoRequestNak :
      OnSendInfoRequestNak(pdu, pdu);
      break;

    case H225_RasMessage::e_serviceControlIndication :
      OnSendServiceControlIndication(pdu, pdu);
      break;

    case H225_RasMessage::e_serviceControlResponse :
      OnSendServiceControlResponse(pdu, pdu);
      break;

    default :
      break;
  }
}


PBoolean H225_RAS::OnReceiveRequestInProgress(const H323RasPDU & pdu, const H225_RequestInProgress & rip)
{
  if (!HandleRequestInProgress(pdu, rip.m_delay))
    return FALSE;

  return OnReceiveRequestInProgress(rip);
}


PBoolean H225_RAS::OnReceiveRequestInProgress(const H225_RequestInProgress & /*rip*/)
{
  return TRUE;
}


template <typename PDUType>
static void SendGenericData(const H225_RAS * ras, unsigned code, PDUType & pdu)
{
 H225_FeatureSet fs;
     if (ras->OnSendFeatureSet(code,fs,false)) {
	    if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)) {
	    pdu.IncludeOptionalField(PDUType::e_genericData);

	    H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
	    H225_ArrayOf_GenericData & data = pdu.m_genericData;

		    for (PINDEX i=0; i < fsn.GetSize(); i++) {
				    PINDEX lastPos = data.GetSize();
				    data.SetSize(lastPos+1);
				    data[lastPos] = fsn[i];
		    }
	    }
     }
}

template <typename PDUType>
static void SendFeatureSet(const H225_RAS * ras, unsigned code, PDUType & pdu)
{

 H225_FeatureSet fs;
   if (ras->OnSendFeatureSet(code,fs,true)) {
        pdu.IncludeOptionalField(PDUType::e_featureSet);
		pdu.m_featureSet = fs;
   }

   if (ras->OnSendFeatureSet(code,fs,false)) {
	  if (fs.HasOptionalField(H225_FeatureSet::e_supportedFeatures)) {
		pdu.IncludeOptionalField(PDUType::e_genericData);

		H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
	    H225_ArrayOf_GenericData & data = pdu.m_genericData;

		for (PINDEX i=0; i < fsn.GetSize(); i++) {
			 PINDEX lastPos = data.GetSize();
			 data.SetSize(lastPos+1);
			 data[lastPos] = fsn[i];
		}
	  }
    }
}


void H225_RAS::OnSendGatekeeperRequest(H323RasPDU &, H225_GatekeeperRequest & grq)
{
  // This function is never called during sending GRQ
  if (!gatekeeperIdentifier) {
    grq.IncludeOptionalField(H225_GatekeeperRequest::e_gatekeeperIdentifier);
    grq.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

  OnSendGatekeeperRequest(grq);
}


void H225_RAS::OnSendGatekeeperRequest(H225_GatekeeperRequest & grq)
{
#ifdef H323_H460
  SendFeatureSet<H225_GatekeeperRequest>(this, H460_MessageType::e_gatekeeperRequest, grq);
#endif
}


void H225_RAS::OnSendGatekeeperConfirm(H323RasPDU &, H225_GatekeeperConfirm & gcf)
{
  if (!gatekeeperIdentifier) {
    gcf.IncludeOptionalField(H225_GatekeeperConfirm::e_gatekeeperIdentifier);
    gcf.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

#ifdef H323_H460
  SendFeatureSet<H225_GatekeeperConfirm>(this, H460_MessageType::e_gatekeeperConfirm, gcf);
#endif

  OnSendGatekeeperConfirm(gcf);
}


void H225_RAS::OnSendGatekeeperConfirm(H225_GatekeeperConfirm & /*gcf*/)
{
}

void H225_RAS::OnSendGatekeeperReject(H323RasPDU &, H225_GatekeeperReject & grj)
{
  if (!gatekeeperIdentifier) {
    grj.IncludeOptionalField(H225_GatekeeperReject::e_gatekeeperIdentifier);
    grj.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

#ifdef H323_H460
  SendFeatureSet<H225_GatekeeperReject>(this, H460_MessageType::e_gatekeeperReject, grj);
#endif

  OnSendGatekeeperReject(grj);
}

void H225_RAS::OnSendGatekeeperReject(H225_GatekeeperReject & /*grj*/)
{
}

template <typename PDUType>
static void ProcessFeatureSet(const H225_RAS * ras, unsigned code, const PDUType & pdu)
{
    if (pdu.HasOptionalField(PDUType::e_genericData)) {
        H225_FeatureSet fs;
	    fs.IncludeOptionalField(H225_FeatureSet::e_supportedFeatures);
	    H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
		const H225_ArrayOf_GenericData & data = pdu.m_genericData;
		for (PINDEX i=0; i < data.GetSize(); i++) {
			 PINDEX lastPos = fsn.GetSize();
			 fsn.SetSize(lastPos+1);
			 fsn[lastPos] = (H225_FeatureDescriptor &)data[i];
		}
        ras->OnReceiveFeatureSet(code, fs);
	}
}

template <typename PDUType>
static void ReceiveGenericData(const H225_RAS * ras, unsigned code, const PDUType & pdu)
{
	ProcessFeatureSet<PDUType>(ras,code,pdu);
}

template <typename PDUType>
static void ReceiveFeatureSet(const H225_RAS * ras, unsigned code, const PDUType & pdu)
{
  if (pdu.HasOptionalField(PDUType::e_featureSet))
      ras->OnReceiveFeatureSet(code, pdu.m_featureSet);

	ProcessFeatureSet<PDUType>(ras,code,pdu);
}


PBoolean H225_RAS::OnReceiveGatekeeperRequest(const H323RasPDU &, const H225_GatekeeperRequest & grq)
{
#ifdef H323_H460
  ReceiveFeatureSet<H225_GatekeeperRequest>(this, H460_MessageType::e_gatekeeperRequest, grq);
#endif

  return OnReceiveGatekeeperRequest(grq);
}


PBoolean H225_RAS::OnReceiveGatekeeperRequest(const H225_GatekeeperRequest &)
{
  return TRUE;
}


PBoolean H225_RAS::OnReceiveGatekeeperConfirm(const H323RasPDU &, const H225_GatekeeperConfirm & gcf)
{
  if (!CheckForResponse(H225_RasMessage::e_gatekeeperRequest, gcf.m_requestSeqNum))
    return FALSE;

  if (gcf.HasOptionalField(H225_GatekeeperConfirm::e_gatekeeperIdentifier)) {
      if (gatekeeperIdentifier.IsEmpty())
        gatekeeperIdentifier = gcf.m_gatekeeperIdentifier;
      else {
        PString gkid = gcf.m_gatekeeperIdentifier;
        if (gatekeeperIdentifier *= gkid)
          gatekeeperIdentifier = gkid;
        else {
          PTRACE(2, "RAS\tReceived a GCF from " << gkid
                 << " but wanted it from " << gatekeeperIdentifier);
          return FALSE;
        }
      }
  }

#ifdef H323_H460
  if (gcf.HasOptionalField(H225_GatekeeperConfirm::e_featureSet))
     ReceiveFeatureSet<H225_GatekeeperConfirm>(this, H460_MessageType::e_gatekeeperConfirm, gcf);
  else
     DisableFeatureSet(H460_MessageType::e_gatekeeperConfirm);
#endif

  return OnReceiveGatekeeperConfirm(gcf);
}


PBoolean H225_RAS::OnReceiveGatekeeperConfirm(const H225_GatekeeperConfirm & /*gcf*/)
{
  return TRUE;
}


PBoolean H225_RAS::OnReceiveGatekeeperReject(const H323RasPDU &, const H225_GatekeeperReject & grj)
{
  if (!CheckForResponse(H225_RasMessage::e_gatekeeperRequest, grj.m_requestSeqNum, &grj.m_rejectReason))
    return FALSE;

#ifdef H323_H460
  ReceiveFeatureSet<H225_GatekeeperReject>(this, H460_MessageType::e_gatekeeperReject, grj);
#endif

  return OnReceiveGatekeeperReject(grj);
}


PBoolean H225_RAS::OnReceiveGatekeeperReject(const H225_GatekeeperReject & /*grj*/)
{
  return TRUE;
}


void H225_RAS::OnSendRegistrationRequest(H323RasPDU & pdu, H225_RegistrationRequest & rrq)
{
  OnSendRegistrationRequest(rrq);

#ifdef H323_H460
  SendFeatureSet<H225_RegistrationRequest>(this, H460_MessageType::e_registrationRequest, rrq);
#endif

  pdu.Prepare(rrq.m_tokens, H225_RegistrationRequest::e_tokens,
              rrq.m_cryptoTokens, H225_RegistrationRequest::e_cryptoTokens);

}


void H225_RAS::OnSendRegistrationRequest(H225_RegistrationRequest & /*rrq*/)
{
}


void H225_RAS::OnSendRegistrationConfirm(H323RasPDU & pdu, H225_RegistrationConfirm & rcf)
{
  if (!gatekeeperIdentifier) {
    rcf.IncludeOptionalField(H225_RegistrationConfirm::e_gatekeeperIdentifier);
    rcf.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

  OnSendRegistrationConfirm(rcf);

#ifdef H323_H460
  SendFeatureSet<H225_RegistrationConfirm>(this, H460_MessageType::e_registrationConfirm, rcf);
#endif

  pdu.Prepare(rcf.m_tokens, H225_RegistrationConfirm::e_tokens,
              rcf.m_cryptoTokens, H225_RegistrationConfirm::e_cryptoTokens);
 
}


void H225_RAS::OnSendRegistrationConfirm(H225_RegistrationConfirm & /*rcf*/)
{
}


void H225_RAS::OnSendRegistrationReject(H323RasPDU & pdu, H225_RegistrationReject & rrj)
{
  if (!gatekeeperIdentifier) {
    rrj.IncludeOptionalField(H225_RegistrationReject::e_gatekeeperIdentifier);
    rrj.m_gatekeeperIdentifier = gatekeeperIdentifier;
  }

  OnSendRegistrationReject(rrj);

#ifdef H323_H460
  SendFeatureSet<H225_RegistrationReject>(this, H460_MessageType::e_registrationReject, rrj);
#endif

  pdu.Prepare(rrj.m_tokens, H225_RegistrationReject::e_tokens,
              rrj.m_cryptoTokens, H225_RegistrationReject::e_cryptoTokens);
}


void H225_RAS::OnSendRegistrationReject(H225_RegistrationReject & /*rrj*/)
{
}


PBoolean H225_RAS::OnReceiveRegistrationRequest(const H323RasPDU &, const H225_RegistrationRequest & rrq)
{
#ifdef H323_H460
  ReceiveFeatureSet<H225_RegistrationRequest>(this, H460_MessageType::e_registrationRequest, rrq);
#endif

  return OnReceiveRegistrationRequest(rrq);
}


PBoolean H225_RAS::OnReceiveRegistrationRequest(const H225_RegistrationRequest &)
{
  return TRUE;
}


PBoolean H225_RAS::OnReceiveRegistrationConfirm(const H323RasPDU & pdu, const H225_RegistrationConfirm & rcf)
{
  if (!CheckForResponse(H225_RasMessage::e_registrationRequest, rcf.m_requestSeqNum))
    return FALSE;

  if (gatekeeperIdentifier.IsEmpty()) {
       if (!rcf.HasOptionalField(H225_RegistrationConfirm::e_gatekeeperIdentifier)) {
           PTRACE(2,"H225RAS\tLOGIC ERROR: No Gatekeeper Identifier received!");
           return false;
       }
       gatekeeperIdentifier = rcf.m_gatekeeperIdentifier;
  } else if (rcf.HasOptionalField(H225_RegistrationConfirm::e_gatekeeperIdentifier)) {
       PString gkId = rcf.m_gatekeeperIdentifier;
       if (gkId != gatekeeperIdentifier) {
           PTRACE(2,"H225RAS\tLOGIC ERROR: Gatekeeper Identifier received does not match one recieved!");
           return false;
       }
  }

  if (lastRequest != NULL) {
    PString endpointIdentifier = rcf.m_endpointIdentifier;
    const H235Authenticators & authenticators = lastRequest->requestPDU.GetAuthenticators();
    for (PINDEX i = 0; i < authenticators.GetSize(); i++) {
      H235Authenticator & authenticator = authenticators[i];
      if (authenticator.UseGkAndEpIdentifiers())
        authenticator.SetLocalId(endpointIdentifier);
    }
  }

  if (!CheckCryptoTokens(pdu,
                         rcf.m_tokens, H225_RegistrationConfirm::e_tokens,
                         rcf.m_cryptoTokens, H225_RegistrationConfirm::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  if (rcf.HasOptionalField(H225_RegistrationConfirm::e_featureSet))
     ReceiveFeatureSet<H225_RegistrationConfirm>(this, H460_MessageType::e_registrationConfirm, rcf);
  else
     DisableFeatureSet(H460_MessageType::e_registrationConfirm);
#endif

  return OnReceiveRegistrationConfirm(rcf);
}


PBoolean H225_RAS::OnReceiveRegistrationConfirm(const H225_RegistrationConfirm & /*rcf*/)
{
  return TRUE;
}


PBoolean H225_RAS::OnReceiveRegistrationReject(const H323RasPDU & pdu, const H225_RegistrationReject & rrj)
{
  if (!CheckForResponse(H225_RasMessage::e_registrationRequest, rrj.m_requestSeqNum, &rrj.m_rejectReason))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         rrj.m_tokens, H225_RegistrationReject::e_tokens,
                         rrj.m_cryptoTokens, H225_RegistrationReject::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveFeatureSet<H225_RegistrationReject>(this, H460_MessageType::e_registrationReject, rrj);
#endif

  return OnReceiveRegistrationReject(rrj);
}


PBoolean H225_RAS::OnReceiveRegistrationReject(const H225_RegistrationReject & /*rrj*/)
{
  return TRUE;
}


void H225_RAS::OnSendUnregistrationRequest(H323RasPDU & pdu, H225_UnregistrationRequest & urq)
{
  OnSendUnregistrationRequest(urq);
  pdu.Prepare(urq.m_tokens, H225_UnregistrationRequest::e_tokens,
              urq.m_cryptoTokens, H225_UnregistrationRequest::e_cryptoTokens);
}


void H225_RAS::OnSendUnregistrationRequest(H225_UnregistrationRequest & /*urq*/)
{
}


void H225_RAS::OnSendUnregistrationConfirm(H323RasPDU & pdu, H225_UnregistrationConfirm & ucf)
{
  OnSendUnregistrationConfirm(ucf);
  pdu.Prepare(ucf.m_tokens, H225_UnregistrationConfirm::e_tokens,
              ucf.m_cryptoTokens, H225_UnregistrationConfirm::e_cryptoTokens);
}


void H225_RAS::OnSendUnregistrationConfirm(H225_UnregistrationConfirm & /*ucf*/)
{
}


void H225_RAS::OnSendUnregistrationReject(H323RasPDU & pdu, H225_UnregistrationReject & urj)
{
  OnSendUnregistrationReject(urj);
  pdu.Prepare(urj.m_tokens, H225_UnregistrationReject::e_tokens,
              urj.m_cryptoTokens, H225_UnregistrationReject::e_cryptoTokens);
}


void H225_RAS::OnSendUnregistrationReject(H225_UnregistrationReject & /*urj*/)
{
}


PBoolean H225_RAS::OnReceiveUnregistrationRequest(const H323RasPDU & pdu, const H225_UnregistrationRequest & urq)
{
  if (!CheckCryptoTokens(pdu,
                         urq.m_tokens, H225_UnregistrationRequest::e_tokens,
                         urq.m_cryptoTokens, H225_UnregistrationRequest::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
   ProcessFeatureSet<H225_UnregistrationRequest>(this, H460_MessageType::e_unregistrationRequest, urq);
#endif

  return OnReceiveUnregistrationRequest(urq);
}


PBoolean H225_RAS::OnReceiveUnregistrationRequest(const H225_UnregistrationRequest & /*urq*/)
{
  return TRUE;
}


PBoolean H225_RAS::OnReceiveUnregistrationConfirm(const H323RasPDU & pdu, const H225_UnregistrationConfirm & ucf)
{
  if (!CheckForResponse(H225_RasMessage::e_unregistrationRequest, ucf.m_requestSeqNum))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         ucf.m_tokens, H225_UnregistrationConfirm::e_tokens,
                         ucf.m_cryptoTokens, H225_UnregistrationConfirm::e_cryptoTokens))
    return FALSE;

  return OnReceiveUnregistrationConfirm(ucf);
}


PBoolean H225_RAS::OnReceiveUnregistrationConfirm(const H225_UnregistrationConfirm & /*ucf*/)
{
  return TRUE;
}


PBoolean H225_RAS::OnReceiveUnregistrationReject(const H323RasPDU & pdu, const H225_UnregistrationReject & urj)
{
  if (!CheckForResponse(H225_RasMessage::e_unregistrationRequest, urj.m_requestSeqNum, &urj.m_rejectReason))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         urj.m_tokens, H225_UnregistrationReject::e_tokens,
                         urj.m_cryptoTokens, H225_UnregistrationReject::e_cryptoTokens))
    return FALSE;

  return OnReceiveUnregistrationReject(urj);
}


PBoolean H225_RAS::OnReceiveUnregistrationReject(const H225_UnregistrationReject & /*urj*/)
{
  return TRUE;
}


void H225_RAS::OnSendAdmissionRequest(H323RasPDU & pdu, H225_AdmissionRequest & arq)
{
  OnSendAdmissionRequest(arq);

#ifdef H323_H460
  SendFeatureSet<H225_AdmissionRequest>(this, H460_MessageType::e_admissionRequest, arq);
#endif

  pdu.Prepare(arq.m_tokens, H225_AdmissionRequest::e_tokens,
              arq.m_cryptoTokens, H225_AdmissionRequest::e_cryptoTokens);
}


void H225_RAS::OnSendAdmissionRequest(H225_AdmissionRequest & /*arq*/)
{
}


void H225_RAS::OnSendAdmissionConfirm(H323RasPDU & pdu, H225_AdmissionConfirm & acf)
{
  OnSendAdmissionConfirm(acf);

  pdu.Prepare(acf.m_tokens, H225_AdmissionConfirm::e_tokens,
              acf.m_cryptoTokens, H225_AdmissionConfirm::e_cryptoTokens);
}


void H225_RAS::OnSendAdmissionConfirm(H225_AdmissionConfirm & /*acf*/)
{
}


void H225_RAS::OnSendAdmissionReject(H323RasPDU & pdu, H225_AdmissionReject & arj)
{
  OnSendAdmissionReject(arj);

#ifdef H323_H460
  SendFeatureSet<H225_AdmissionReject>(this, H460_MessageType::e_admissionReject, arj);
#endif

  pdu.Prepare(arj.m_tokens, H225_AdmissionReject::e_tokens,
              arj.m_cryptoTokens, H225_AdmissionReject::e_cryptoTokens);
}


void H225_RAS::OnSendAdmissionReject(H225_AdmissionReject & /*arj*/)
{
}


PBoolean H225_RAS::OnReceiveAdmissionRequest(const H323RasPDU & pdu, const H225_AdmissionRequest & arq)
{
  if (!CheckCryptoTokens(pdu,
                         arq.m_tokens, H225_AdmissionRequest::e_tokens,
                         arq.m_cryptoTokens, H225_AdmissionRequest::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveFeatureSet<H225_AdmissionRequest>(this, H460_MessageType::e_admissionRequest, arq);
#endif

  return OnReceiveAdmissionRequest(arq);
}


PBoolean H225_RAS::OnReceiveAdmissionRequest(const H225_AdmissionRequest & /*arq*/)
{
  return TRUE;
}


PBoolean H225_RAS::OnReceiveAdmissionConfirm(const H323RasPDU & pdu, const H225_AdmissionConfirm & acf)
{
  if (!CheckForResponse(H225_RasMessage::e_admissionRequest, acf.m_requestSeqNum))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         acf.m_tokens, H225_AdmissionConfirm::e_tokens,
                         acf.m_cryptoTokens, H225_AdmissionConfirm::e_cryptoTokens))
    return FALSE;


  return OnReceiveAdmissionConfirm(acf);
}


PBoolean H225_RAS::OnReceiveAdmissionConfirm(const H225_AdmissionConfirm & /*acf*/)
{
  return TRUE;
}


PBoolean H225_RAS::OnReceiveAdmissionReject(const H323RasPDU & pdu, const H225_AdmissionReject & arj)
{
  if (!CheckForResponse(H225_RasMessage::e_admissionRequest, arj.m_requestSeqNum, &arj.m_rejectReason))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         arj.m_tokens, H225_AdmissionReject::e_tokens,
                         arj.m_cryptoTokens, H225_AdmissionReject::e_cryptoTokens))
    return FALSE;

  return OnReceiveAdmissionReject(arj);
}


PBoolean H225_RAS::OnReceiveAdmissionReject(const H225_AdmissionReject & /*arj*/)
{
  return TRUE;
}


void H225_RAS::OnSendBandwidthRequest(H323RasPDU & pdu, H225_BandwidthRequest & brq)
{
  OnSendBandwidthRequest(brq);
  pdu.Prepare(brq.m_tokens, H225_BandwidthRequest::e_tokens,
              brq.m_cryptoTokens, H225_BandwidthRequest::e_cryptoTokens);
}


void H225_RAS::OnSendBandwidthRequest(H225_BandwidthRequest & /*brq*/)
{
}


PBoolean H225_RAS::OnReceiveBandwidthRequest(const H323RasPDU & pdu, const H225_BandwidthRequest & brq)
{
  if (!CheckCryptoTokens(pdu,
                         brq.m_tokens, H225_BandwidthRequest::e_tokens,
                         brq.m_cryptoTokens, H225_BandwidthRequest::e_cryptoTokens))
    return FALSE;

  return OnReceiveBandwidthRequest(brq);
}


PBoolean H225_RAS::OnReceiveBandwidthRequest(const H225_BandwidthRequest & /*brq*/)
{
  return TRUE;
}


void H225_RAS::OnSendBandwidthConfirm(H323RasPDU & pdu, H225_BandwidthConfirm & bcf)
{
  OnSendBandwidthConfirm(bcf);
  pdu.Prepare(bcf.m_tokens, H225_BandwidthConfirm::e_tokens,
              bcf.m_cryptoTokens, H225_BandwidthConfirm::e_cryptoTokens);
}


void H225_RAS::OnSendBandwidthConfirm(H225_BandwidthConfirm & /*bcf*/)
{
}


PBoolean H225_RAS::OnReceiveBandwidthConfirm(const H323RasPDU & pdu, const H225_BandwidthConfirm & bcf)
{
  if (!CheckForResponse(H225_RasMessage::e_bandwidthRequest, bcf.m_requestSeqNum))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         bcf.m_tokens, H225_BandwidthConfirm::e_tokens,
                         bcf.m_cryptoTokens, H225_BandwidthConfirm::e_cryptoTokens))
    return FALSE;

  return OnReceiveBandwidthConfirm(bcf);
}


PBoolean H225_RAS::OnReceiveBandwidthConfirm(const H225_BandwidthConfirm & /*bcf*/)
{
  return TRUE;
}


void H225_RAS::OnSendBandwidthReject(H323RasPDU & pdu, H225_BandwidthReject & brj)
{
  OnSendBandwidthReject(brj);
  pdu.Prepare(brj.m_tokens, H225_BandwidthReject::e_tokens,
              brj.m_cryptoTokens, H225_BandwidthReject::e_cryptoTokens);
}


void H225_RAS::OnSendBandwidthReject(H225_BandwidthReject & /*brj*/)
{
}


PBoolean H225_RAS::OnReceiveBandwidthReject(const H323RasPDU & pdu, const H225_BandwidthReject & brj)
{
  if (!CheckForResponse(H225_RasMessage::e_bandwidthRequest, brj.m_requestSeqNum, &brj.m_rejectReason))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         brj.m_tokens, H225_BandwidthReject::e_tokens,
                         brj.m_cryptoTokens, H225_BandwidthReject::e_cryptoTokens))
    return FALSE;

  return OnReceiveBandwidthReject(brj);
}


PBoolean H225_RAS::OnReceiveBandwidthReject(const H225_BandwidthReject & /*brj*/)
{
  return TRUE;
}


void H225_RAS::OnSendDisengageRequest(H323RasPDU & pdu, H225_DisengageRequest & drq)
{
  OnSendDisengageRequest(drq);
  pdu.Prepare(drq.m_tokens, H225_DisengageRequest::e_tokens,
              drq.m_cryptoTokens, H225_DisengageRequest::e_cryptoTokens);

#ifdef H323_H460
  SendGenericData<H225_DisengageRequest>(this, H460_MessageType::e_disengagerequest, drq);
#endif
}


void H225_RAS::OnSendDisengageRequest(H225_DisengageRequest & /*drq*/)
{
}


PBoolean H225_RAS::OnReceiveDisengageRequest(const H323RasPDU & pdu, const H225_DisengageRequest & drq)
{
  if (!CheckCryptoTokens(pdu,
                         drq.m_tokens, H225_DisengageRequest::e_tokens,
                         drq.m_cryptoTokens, H225_DisengageRequest::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveGenericData<H225_DisengageRequest>(this, H460_MessageType::e_disengagerequest, drq);
#endif

  return OnReceiveDisengageRequest(drq);
}


PBoolean H225_RAS::OnReceiveDisengageRequest(const H225_DisengageRequest & /*drq*/)
{
  return TRUE;
}


void H225_RAS::OnSendDisengageConfirm(H323RasPDU & pdu, H225_DisengageConfirm & dcf)
{
  OnSendDisengageConfirm(dcf);
  pdu.Prepare(dcf.m_tokens, H225_DisengageConfirm::e_tokens,
              dcf.m_cryptoTokens, H225_DisengageConfirm::e_cryptoTokens);

#ifdef H323_H460
  SendGenericData<H225_DisengageConfirm>(this, H460_MessageType::e_disengageconfirm, dcf);
#endif
}


void H225_RAS::OnSendDisengageConfirm(H225_DisengageConfirm & /*dcf*/)
{
}


PBoolean H225_RAS::OnReceiveDisengageConfirm(const H323RasPDU & pdu, const H225_DisengageConfirm & dcf)
{
  if (!CheckForResponse(H225_RasMessage::e_disengageRequest, dcf.m_requestSeqNum))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         dcf.m_tokens, H225_DisengageConfirm::e_tokens,
                         dcf.m_cryptoTokens, H225_DisengageConfirm::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveGenericData<H225_DisengageConfirm>(this, H460_MessageType::e_disengageconfirm, dcf);
#endif

  return OnReceiveDisengageConfirm(dcf);
}


PBoolean H225_RAS::OnReceiveDisengageConfirm(const H225_DisengageConfirm & /*dcf*/)
{
  return TRUE;
}


void H225_RAS::OnSendDisengageReject(H323RasPDU & pdu, H225_DisengageReject & drj)
{
  OnSendDisengageReject(drj);
  pdu.Prepare(drj.m_tokens, H225_DisengageReject::e_tokens,
              drj.m_cryptoTokens, H225_DisengageReject::e_cryptoTokens);
}


void H225_RAS::OnSendDisengageReject(H225_DisengageReject & /*drj*/)
{
}


PBoolean H225_RAS::OnReceiveDisengageReject(const H323RasPDU & pdu, const H225_DisengageReject & drj)
{
  if (!CheckForResponse(H225_RasMessage::e_disengageRequest, drj.m_requestSeqNum, &drj.m_rejectReason))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         drj.m_tokens, H225_DisengageReject::e_tokens,
                         drj.m_cryptoTokens, H225_DisengageReject::e_cryptoTokens))
    return FALSE;

  return OnReceiveDisengageReject(drj);
}


PBoolean H225_RAS::OnReceiveDisengageReject(const H225_DisengageReject & /*drj*/)
{
  return TRUE;
}


void H225_RAS::OnSendLocationRequest(H323RasPDU & pdu, H225_LocationRequest & lrq)
{
  OnSendLocationRequest(lrq);

#ifdef H323_H460
  SendFeatureSet<H225_LocationRequest>(this, H460_MessageType::e_locationRequest, lrq);
#endif

  pdu.Prepare(lrq.m_tokens, H225_LocationRequest::e_tokens,
              lrq.m_cryptoTokens, H225_LocationRequest::e_cryptoTokens);
}


void H225_RAS::OnSendLocationRequest(H225_LocationRequest & /*lrq*/)
{
}


PBoolean H225_RAS::OnReceiveLocationRequest(const H323RasPDU & pdu, const H225_LocationRequest & lrq)
{
  if (!CheckCryptoTokens(pdu,
                         lrq.m_tokens, H225_LocationRequest::e_tokens,
                         lrq.m_cryptoTokens, H225_LocationRequest::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveFeatureSet<H225_LocationRequest>(this, H460_MessageType::e_locationRequest, lrq);
#endif

  return OnReceiveLocationRequest(lrq);
}


PBoolean H225_RAS::OnReceiveLocationRequest(const H225_LocationRequest & /*lrq*/)
{
  return TRUE;
}


void H225_RAS::OnSendLocationConfirm(H323RasPDU & pdu, H225_LocationConfirm & lcf)
{
  OnSendLocationConfirm(lcf);

#ifdef H323_H460
  SendFeatureSet<H225_LocationConfirm>(this, H460_MessageType::e_locationConfirm, lcf);
#endif

  pdu.Prepare(lcf.m_tokens, H225_LocationRequest::e_tokens,
              lcf.m_cryptoTokens, H225_LocationRequest::e_cryptoTokens);
}


void H225_RAS::OnSendLocationConfirm(H225_LocationConfirm & /*lcf*/)
{
}


PBoolean H225_RAS::OnReceiveLocationConfirm(const H323RasPDU &, const H225_LocationConfirm & lcf)
{
  if (!CheckForResponse(H225_RasMessage::e_locationRequest, lcf.m_requestSeqNum))
    return FALSE;

  if (lastRequest->responseInfo != NULL) {
    H323TransportAddress & locatedAddress = *(H323TransportAddress *)lastRequest->responseInfo;
    locatedAddress = lcf.m_callSignalAddress;
  }

#ifdef H323_H460
  ReceiveFeatureSet<H225_LocationConfirm>(this, H460_MessageType::e_locationConfirm, lcf);
#endif

  return OnReceiveLocationConfirm(lcf);
}


PBoolean H225_RAS::OnReceiveLocationConfirm(const H225_LocationConfirm & /*lcf*/)
{
  return TRUE;
}


void H225_RAS::OnSendLocationReject(H323RasPDU & pdu, H225_LocationReject & lrj)
{
  OnSendLocationReject(lrj);

#ifdef H323_H460
  SendFeatureSet<H225_LocationReject>(this, H460_MessageType::e_locationReject, lrj);
#endif

  pdu.Prepare(lrj.m_tokens, H225_LocationReject::e_tokens,
              lrj.m_cryptoTokens, H225_LocationReject::e_cryptoTokens);
}


void H225_RAS::OnSendLocationReject(H225_LocationReject & /*lrj*/)
{
}


PBoolean H225_RAS::OnReceiveLocationReject(const H323RasPDU & pdu, const H225_LocationReject & lrj)
{
  if (!CheckForResponse(H225_RasMessage::e_locationRequest, lrj.m_requestSeqNum, &lrj.m_rejectReason))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         lrj.m_tokens, H225_LocationReject::e_tokens,
                         lrj.m_cryptoTokens, H225_LocationReject::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveFeatureSet<H225_LocationReject>(this, H460_MessageType::e_locationReject, lrj);
#endif

  return OnReceiveLocationReject(lrj);
}


PBoolean H225_RAS::OnReceiveLocationReject(const H225_LocationReject & /*lrj*/)
{
  return TRUE;
}


void H225_RAS::OnSendInfoRequest(H323RasPDU & pdu, H225_InfoRequest & irq)
{
  OnSendInfoRequest(irq);
  pdu.Prepare(irq.m_tokens, H225_InfoRequest::e_tokens,
              irq.m_cryptoTokens, H225_InfoRequest::e_cryptoTokens);

#ifdef H323_H460
  SendGenericData<H225_InfoRequest>(this, H460_MessageType::e_inforequest, irq);
#endif
}


void H225_RAS::OnSendInfoRequest(H225_InfoRequest & /*irq*/)
{
}


PBoolean H225_RAS::OnReceiveInfoRequest(const H323RasPDU & pdu, const H225_InfoRequest & irq)
{
  if (!CheckCryptoTokens(pdu,
                         irq.m_tokens, H225_InfoRequest::e_tokens,
                         irq.m_cryptoTokens, H225_InfoRequest::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveGenericData<H225_InfoRequest>(this, H460_MessageType::e_inforequest, irq);
#endif

  return OnReceiveInfoRequest(irq);
}


PBoolean H225_RAS::OnReceiveInfoRequest(const H225_InfoRequest & /*irq*/)
{
  return TRUE;
}


void H225_RAS::OnSendInfoRequestResponse(H323RasPDU & pdu, H225_InfoRequestResponse & irr)
{
  OnSendInfoRequestResponse(irr);
  pdu.Prepare(irr.m_tokens, H225_InfoRequestResponse::e_tokens,
              irr.m_cryptoTokens, H225_InfoRequestResponse::e_cryptoTokens);

#ifdef H323_H460
  SendGenericData<H225_InfoRequestResponse>(this, H460_MessageType::e_inforequestresponse, irr);
#endif
}


void H225_RAS::OnSendInfoRequestResponse(H225_InfoRequestResponse & /*irr*/)
{
}


PBoolean H225_RAS::OnReceiveInfoRequestResponse(const H323RasPDU & pdu, const H225_InfoRequestResponse & irr)
{
  if (!CheckForResponse(H225_RasMessage::e_infoRequest, irr.m_requestSeqNum))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         irr.m_tokens, H225_InfoRequestResponse::e_tokens,
                         irr.m_cryptoTokens, H225_InfoRequestResponse::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveGenericData<H225_InfoRequestResponse>(this, H460_MessageType::e_inforequestresponse, irr);
#endif

  return OnReceiveInfoRequestResponse(irr);
}


PBoolean H225_RAS::OnReceiveInfoRequestResponse(const H225_InfoRequestResponse & /*irr*/)
{
  return TRUE;
}


void H225_RAS::OnSendNonStandardMessage(H323RasPDU & pdu, H225_NonStandardMessage & nsm)
{
  OnSendNonStandardMessage(nsm);

#ifdef H323_H460
  SendFeatureSet<H225_NonStandardMessage>(this, H460_MessageType::e_nonStandardMessage, nsm);
#endif

  pdu.Prepare(nsm.m_tokens, H225_InfoRequestResponse::e_tokens,
              nsm.m_cryptoTokens, H225_InfoRequestResponse::e_cryptoTokens);
}


void H225_RAS::OnSendNonStandardMessage(H225_NonStandardMessage & /*nsm*/)
{
}


PBoolean H225_RAS::OnReceiveNonStandardMessage(const H323RasPDU & pdu, const H225_NonStandardMessage & nsm)
{
  if (!CheckCryptoTokens(pdu,
                         nsm.m_tokens, H225_NonStandardMessage::e_tokens,
                         nsm.m_cryptoTokens, H225_NonStandardMessage::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveFeatureSet<H225_NonStandardMessage>(this, H460_MessageType::e_nonStandardMessage, nsm);
#endif

  return OnReceiveNonStandardMessage(nsm);
}


PBoolean H225_RAS::OnReceiveNonStandardMessage(const H225_NonStandardMessage & /*nsm*/)
{
  return TRUE;
}


void H225_RAS::OnSendUnknownMessageResponse(H323RasPDU & pdu, H225_UnknownMessageResponse & umr)
{
  OnSendUnknownMessageResponse(umr);
  pdu.Prepare(umr.m_tokens, H225_UnknownMessageResponse::e_tokens,
              umr.m_cryptoTokens, H225_UnknownMessageResponse::e_cryptoTokens);
}


void H225_RAS::OnSendUnknownMessageResponse(H225_UnknownMessageResponse & /*umr*/)
{
}


PBoolean H225_RAS::OnReceiveUnknownMessageResponse(const H323RasPDU & pdu, const H225_UnknownMessageResponse & umr)
{
  if (!CheckCryptoTokens(pdu,
                         umr.m_tokens, H225_UnknownMessageResponse::e_tokens,
                         umr.m_cryptoTokens, H225_UnknownMessageResponse::e_cryptoTokens))
    return FALSE;

  return OnReceiveUnknownMessageResponse(umr);
}


PBoolean H225_RAS::OnReceiveUnknownMessageResponse(const H225_UnknownMessageResponse & /*umr*/)
{
  return TRUE;
}


void H225_RAS::OnSendRequestInProgress(H323RasPDU & pdu, H225_RequestInProgress & rip)
{
  OnSendRequestInProgress(rip);
  pdu.Prepare(rip.m_tokens, H225_RequestInProgress::e_tokens,
              rip.m_cryptoTokens, H225_RequestInProgress::e_cryptoTokens);
}


void H225_RAS::OnSendRequestInProgress(H225_RequestInProgress & /*rip*/)
{
}


void H225_RAS::OnSendResourcesAvailableIndicate(H323RasPDU & pdu, H225_ResourcesAvailableIndicate & rai)
{
  OnSendResourcesAvailableIndicate(rai);
  pdu.Prepare(rai.m_tokens, H225_ResourcesAvailableIndicate::e_tokens,
              rai.m_cryptoTokens, H225_ResourcesAvailableIndicate::e_cryptoTokens);
}


void H225_RAS::OnSendResourcesAvailableIndicate(H225_ResourcesAvailableIndicate & /*rai*/)
{
}


PBoolean H225_RAS::OnReceiveResourcesAvailableIndicate(const H323RasPDU & pdu, const H225_ResourcesAvailableIndicate & rai)
{
  if (!CheckCryptoTokens(pdu,
                         rai.m_tokens, H225_ResourcesAvailableIndicate::e_tokens,
                         rai.m_cryptoTokens, H225_ResourcesAvailableIndicate::e_cryptoTokens))
    return FALSE;

  return OnReceiveResourcesAvailableIndicate(rai);
}


PBoolean H225_RAS::OnReceiveResourcesAvailableIndicate(const H225_ResourcesAvailableIndicate & /*rai*/)
{
  return TRUE;
}


void H225_RAS::OnSendResourcesAvailableConfirm(H323RasPDU & pdu, H225_ResourcesAvailableConfirm & rac)
{
  OnSendResourcesAvailableConfirm(rac);
  pdu.Prepare(rac.m_tokens, H225_ResourcesAvailableConfirm::e_tokens,
              rac.m_cryptoTokens, H225_ResourcesAvailableConfirm::e_cryptoTokens);
}


void H225_RAS::OnSendResourcesAvailableConfirm(H225_ResourcesAvailableConfirm & /*rac*/)
{
}


PBoolean H225_RAS::OnReceiveResourcesAvailableConfirm(const H323RasPDU & pdu, const H225_ResourcesAvailableConfirm & rac)
{
  if (!CheckForResponse(H225_RasMessage::e_resourcesAvailableIndicate, rac.m_requestSeqNum))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         rac.m_tokens, H225_ResourcesAvailableConfirm::e_tokens,
                         rac.m_cryptoTokens, H225_ResourcesAvailableConfirm::e_cryptoTokens))
    return FALSE;

  return OnReceiveResourcesAvailableConfirm(rac);
}


PBoolean H225_RAS::OnReceiveResourcesAvailableConfirm(const H225_ResourcesAvailableConfirm & /*rac*/)
{
  return TRUE;
}

void H225_RAS::OnSendServiceControlIndication(H323RasPDU & pdu, H225_ServiceControlIndication & sci)
{
  OnSendServiceControlIndication(sci);

#ifdef H323_H460
  SendFeatureSet<H225_ServiceControlIndication>(this, H460_MessageType::e_serviceControlIndication, sci);
#endif

  pdu.Prepare(sci.m_tokens, H225_ServiceControlIndication::e_tokens,
              sci.m_cryptoTokens, H225_ServiceControlIndication::e_cryptoTokens);
}


void H225_RAS::OnSendServiceControlIndication(H225_ServiceControlIndication & /*sci*/)
{
}


PBoolean H225_RAS::OnReceiveServiceControlIndication(const H323RasPDU & pdu, const H225_ServiceControlIndication & sci)
{
  if (!CheckCryptoTokens(pdu,
                         sci.m_tokens, H225_ServiceControlIndication::e_tokens,
                         sci.m_cryptoTokens, H225_ServiceControlIndication::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveFeatureSet<H225_ServiceControlIndication>(this, H460_MessageType::e_serviceControlIndication, sci);
#endif

  return OnReceiveServiceControlIndication(sci);
}


PBoolean H225_RAS::OnReceiveServiceControlIndication(const H225_ServiceControlIndication & /*sci*/)
{
  return TRUE;
}


void H225_RAS::OnSendServiceControlResponse(H323RasPDU & pdu, H225_ServiceControlResponse & scr)
{
  OnSendServiceControlResponse(scr);

#ifdef H323_H460
  SendFeatureSet<H225_ServiceControlResponse>(this, H460_MessageType::e_serviceControlResponse, scr);
#endif

  pdu.Prepare(scr.m_tokens, H225_ResourcesAvailableConfirm::e_tokens,
              scr.m_cryptoTokens, H225_ResourcesAvailableConfirm::e_cryptoTokens);
}


void H225_RAS::OnSendServiceControlResponse(H225_ServiceControlResponse & /*scr*/)
{
}


PBoolean H225_RAS::OnReceiveServiceControlResponse(const H323RasPDU & pdu, const H225_ServiceControlResponse & scr)
{
  if (!CheckForResponse(H225_RasMessage::e_serviceControlIndication, scr.m_requestSeqNum))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         scr.m_tokens, H225_ServiceControlResponse::e_tokens,
                         scr.m_cryptoTokens, H225_ServiceControlResponse::e_cryptoTokens))
    return FALSE;

#ifdef H323_H460
  ReceiveFeatureSet<H225_ServiceControlResponse>(this, H460_MessageType::e_serviceControlResponse, scr);
#endif

  return OnReceiveServiceControlResponse(scr);
}


PBoolean H225_RAS::OnReceiveServiceControlResponse(const H225_ServiceControlResponse & /*scr*/)
{
  return TRUE;
}

void H225_RAS::OnSendInfoRequestAck(H323RasPDU & pdu, H225_InfoRequestAck & iack)
{
  OnSendInfoRequestAck(iack);
  pdu.Prepare(iack.m_tokens, H225_InfoRequestAck::e_tokens,
              iack.m_cryptoTokens, H225_InfoRequestAck::e_cryptoTokens);
}


void H225_RAS::OnSendInfoRequestAck(H225_InfoRequestAck & /*iack*/)
{
}


PBoolean H225_RAS::OnReceiveInfoRequestAck(const H323RasPDU & pdu, const H225_InfoRequestAck & iack)
{
  if (!CheckForResponse(H225_RasMessage::e_infoRequestResponse, iack.m_requestSeqNum))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         iack.m_tokens, H225_InfoRequestAck::e_tokens,
                         iack.m_cryptoTokens, H225_InfoRequestAck::e_cryptoTokens))
    return FALSE;

  return OnReceiveInfoRequestAck(iack);
}


PBoolean H225_RAS::OnReceiveInfoRequestAck(const H225_InfoRequestAck & /*iack*/)
{
  return TRUE;
}


void H225_RAS::OnSendInfoRequestNak(H323RasPDU & pdu, H225_InfoRequestNak & inak)
{
  OnSendInfoRequestNak(inak);
  pdu.Prepare(inak.m_tokens, H225_InfoRequestAck::e_tokens,
              inak.m_cryptoTokens, H225_InfoRequestAck::e_cryptoTokens);
}


void H225_RAS::OnSendInfoRequestNak(H225_InfoRequestNak & /*inak*/)
{
}


PBoolean H225_RAS::OnReceiveInfoRequestNak(const H323RasPDU & pdu, const H225_InfoRequestNak & inak)
{
  if (!CheckForResponse(H225_RasMessage::e_infoRequestResponse, inak.m_requestSeqNum, &inak.m_nakReason))
    return FALSE;

  if (!CheckCryptoTokens(pdu,
                         inak.m_tokens, H225_InfoRequestNak::e_tokens,
                         inak.m_cryptoTokens, H225_InfoRequestNak::e_cryptoTokens))
    return FALSE;

  return OnReceiveInfoRequestNak(inak);
}


PBoolean H225_RAS::OnReceiveInfoRequestNak(const H225_InfoRequestNak & /*inak*/)
{
  return TRUE;
}


PBoolean H225_RAS::OnReceiveUnknown(const H323RasPDU &)
{
  H323RasPDU response;
  response.BuildUnknownMessageResponse(0);
  return response.Write(*transport);
}


/////////////////////////////////////////////////////////////////////////////
