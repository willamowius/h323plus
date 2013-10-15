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
#include <h460/h460_Std22.h>
#include <h460/h4609.h>
#include <h460/h460.h>


#ifdef _MSC_VER
#pragma warning(disable : 4239)
#endif

// Must Declare for Factory Loader.
H460_FEATURE(Std22);


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
H460_FeatureStd22::H460_FeatureStd22()
: H460_FeatureStd(22), EP(NULL), localSupport(0), isEnabled(false)
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
   localSupport = EP->GetSignalSecurity();
}

void H460_FeatureStd22::AttachConnection(H323Connection * _con)
{
   CON = _con;
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

void BuildFeature(int localSupport, H323EndPoint * ep, H460_FeatureStd & feat)
{
    if (localSupport == H323EndPoint::e_h225_tls ||
        localSupport == H323EndPoint::e_h225_tls_ipsec) {

        H460_FeatureStd sets;
        sets.Add(1,H460_FeatureContent(1,8)); // Priority 1
        sets.Add(2,H460_FeatureContent(ep->GetListeners()[0].GetTransportAddress()));
        feat.Add(1,H460_FeatureContent(sets.GetCurrentTable()));
    }

    // NOT YET Supported...
    if (localSupport == H323EndPoint::e_h225_ipsec ||
        localSupport == H323EndPoint::e_h225_tls_ipsec) {

        H460_FeatureStd sets;
        sets.Add(1,H460_FeatureContent(2,8)); // Priority 2
        feat.Add(2,H460_FeatureContent(sets.GetCurrentTable()));
    }
}

PBoolean H460_FeatureStd22::OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu)
{
    if (!localSupport)
        return false;

    H460_FeatureStd feat = H460_FeatureStd(22);  
    BuildFeature(localSupport, EP, feat);

    pdu = feat;
	return true;

}

void H460_FeatureStd22::OnReceiveGatekeeperConfirm(const H225_FeatureDescriptor & pdu)
{
   H460_FeatureStd & feat = (H460_FeatureStd &)pdu;
   if (!feat.Contains(1)) 
	   return;

    isEnabled = true;
}

PBoolean H460_FeatureStd22::OnSendRegistrationRequest(H225_FeatureDescriptor & pdu)
{
    if (!localSupport)
        return false;

    H460_FeatureStd feat = H460_FeatureStd(22);  
    BuildFeature(localSupport, EP, feat);

    pdu = feat;

	return true;
}

void H460_FeatureStd22::OnReceiveRegistrationConfirm(const H225_FeatureDescriptor & pdu)
{
   H460_FeatureStd & feat = (H460_FeatureStd &)pdu;
   if (!feat.Contains(1)) 
	   return;

    isEnabled = true;
}

PBoolean H460_FeatureStd22::OnSendAdmissionRequest(H225_FeatureDescriptor & pdu)
{
    if (!isEnabled)
        return false;

    H460_FeatureStd feat = H460_FeatureStd(22);
    BuildFeature(localSupport, EP, feat);
    pdu = feat;

    return true;
}

void H460_FeatureStd22::OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu)
{
   H460_FeatureStd & feat = (H460_FeatureStd &)pdu;
   if (!feat.Contains(1)) 
	   return;

   // TODO: Enable Security on the call.
}

#ifdef _MSC_VER
#pragma warning(default : 4239)
#endif

#endif

