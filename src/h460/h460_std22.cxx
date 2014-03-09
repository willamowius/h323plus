/* h460_std22.cxx
 *
 * Copyright (c) 2013 Spranto International Pte Ltd. All Rights Reserved.
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
 * The Initial Developer of the Original Code is Spranto International Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include "ptlib.h"
#include "openh323buildopts.h"

#ifdef H323_TLS

#include <h323ep.h>
#include <h323con.h>
#include <h460/h460_std22.h>
#include <h460/h4609.h>
#include <h460/h460.h>
#ifdef H323_H46018
#include <h460/h460_std18.h>
#endif


#ifdef _MSC_VER
#pragma warning(disable : 4239)
#endif

// Must Declare for Factory Loader.
H460_FEATURE(Std22);

#define Std22_TLS               1
#define Std22_IPSec             2
#define Std22_Priority          1
#define Std22_Address           2


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
H460_FeatureStd22::H460_FeatureStd22()
: H460_FeatureStd(22), EP(NULL), CON(NULL), isEnabled(false)
{
  PTRACE(6,"Std22\tInstance Created");
  FeatureCategory = FeatureSupported;
}

H460_FeatureStd22::~H460_FeatureStd22()
{

}

void H460_FeatureStd22::AttachEndPoint(H323EndPoint * _ep)
{
   PTRACE(6,"Std22\tEndpoint Attached");
   EP = _ep; 
}

void H460_FeatureStd22::AttachConnection(H323Connection * _con)
{
   CON = _con;
}

int H460_FeatureStd22::GetPurpose()
{
    return FeatureBaseClone; 
}

PObject * H460_FeatureStd22::Clone() const
{
  return new H460_FeatureStd22(*this);
}

PBoolean H460_FeatureStd22::FeatureAdvertised(int mtype)
{
     switch (mtype) {
        case H460_MessageType::e_gatekeeperRequest:
        case H460_MessageType::e_gatekeeperConfirm:
        case H460_MessageType::e_registrationRequest:
        case H460_MessageType::e_registrationConfirm:
        case H460_MessageType::e_admissionRequest:
        case H460_MessageType::e_admissionConfirm:
            return true;
        default:
            return false;
     }
}

void BuildFeature(H323TransportSecurity * transec, H323EndPoint * ep, H460_FeatureStd & feat, PBoolean address = true)
{

    if (transec->IsTLSEnabled()) {
        const H323Listener * tls = ep->GetListeners().GetTLSListener();
        if (tls) {
            H460_FeatureStd sets;
            sets.Add(Std22_Priority,H460_FeatureContent(1,8)); // Priority 1
            if (address)
                sets.Add(Std22_Address,H460_FeatureContent(tls->GetTransportAddress()));
            feat.Add(Std22_TLS,H460_FeatureContent(sets.GetCurrentTable()));
        }
    }

    // NOT YET Supported...Disabled in H323TransportSecurity.
    if (transec->IsIPSecEnabled()) {
        H460_FeatureStd sets;
        sets.Add(Std22_Priority,H460_FeatureContent(2,8)); // Priority 2
        feat.Add(Std22_IPSec,H460_FeatureContent(sets.GetCurrentTable()));
    } 
}

void ReadFeature(H323TransportSecurity * transec, H460_FeatureStd * feat)
{
    if (feat->Contains(Std22_TLS)) {
        H460_FeatureParameter tlsparam = feat->Value(Std22_TLS);
        transec->EnableTLS(true);
        H460_FeatureStd settings;
        settings.SetCurrentTable(tlsparam);
        if (settings.Contains(Std22_Address))
            transec->SetRemoteTLSAddress(settings.Value(Std22_Address));
    }

    // NOT YET Supported...Disabled in H323TransportSecurity.
    if (feat->Contains(Std22_IPSec))
        transec->EnableIPSec(true);
}

PBoolean H460_FeatureStd22::OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu)
{
    if (!EP || !EP->GetTransportSecurity()->HasSecurity())
        return false;

#ifdef H323_H46017
    // H.460.22 is incompatible with H.460.17
    if (EP->TryingWithH46017() || EP->RegisteredWithH46017())
        return false;
#endif

    isEnabled = false;
    H460_FeatureStd feat = H460_FeatureStd(22);  
    BuildFeature(EP->GetTransportSecurity(), EP, feat, false);

    pdu = feat;
	return true;

}

void H460_FeatureStd22::OnReceiveGatekeeperConfirm(const H225_FeatureDescriptor & pdu)
{
    // Do nothing
}

PBoolean H460_FeatureStd22::OnSendRegistrationRequest(H225_FeatureDescriptor & pdu)
{
    if (!EP || !EP->GetTransportSecurity()->HasSecurity())
        return false;

#ifdef H323_H46017
    // H.460.22 is incompatible with H.460.17
    if (EP->TryingWithH46017() || EP->RegisteredWithH46017())
        return false;
#endif

    isEnabled = false;
    H460_FeatureStd feat = H460_FeatureStd(22);  
    BuildFeature(EP->GetTransportSecurity(), EP, feat);

    pdu = feat;
	return true;
}

void H460_FeatureStd22::OnReceiveRegistrationConfirm(const H225_FeatureDescriptor & pdu)
{
   isEnabled = true;
}

PBoolean H460_FeatureStd22::OnSendAdmissionRequest(H225_FeatureDescriptor & pdu)
{
    if (!isEnabled)
        return false;

    H460_FeatureStd feat = H460_FeatureStd(22);
    BuildFeature(EP->GetTransportSecurity(), EP, feat, false);
    pdu = feat;

    return true;
}

void H460_FeatureStd22::OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu)
{

   H460_FeatureStd * feat = PRemoveConst(H460_FeatureStd,&(const H460_FeatureStd &)pdu);

   H323TransportSecurity m_callSecurity(EP);
   ReadFeature(&m_callSecurity,feat);

   if (CON)
       CON->SetTransportSecurity(m_callSecurity);
}

void H460_FeatureStd22::OnReceiveServiceControlIndication(const H225_FeatureDescriptor & pdu) 
{
   H460_FeatureStd * feat = PRemoveConst(H460_FeatureStd,&(const H460_FeatureStd &)pdu);

   H323TransportSecurity m_callSecurity(EP);
   ReadFeature(&m_callSecurity,feat);

#ifdef H323_H46018
    if (EP && EP->GetGatekeeper()->GetFeatures().HasFeature(18)) {
        H460_Feature * feat = EP->GetGatekeeper()->GetFeatures().GetFeature(18);
        if (feat)
            ((H460_FeatureStd18 *)feat)->SetTransportSecurity(m_callSecurity);
    }
#endif
};

#ifdef _MSC_VER
#pragma warning(default : 4239)
#endif

#endif

