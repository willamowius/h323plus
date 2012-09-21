/* H460_oid6.cxx
 *
 * Copyright (c) 2012 Spranto Int'l Pte Ltd. All Rights Reserved.
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
 * The Original Code is derived from and used in conjunction with the 
 * H323plus Project (www.h323plus.org/)
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>
#include <openh323buildopts.h>

#ifdef H323_H460PRE

#include "h460/h460_oid6.h"
#include <h323.h>

#ifdef _MSC_VER
#pragma warning(disable : 4239)
#endif

static const char * baseOID = "1.3.6.1.4.1.17090.0.6";        // Advertised Feature
static const char * priorOID = "1";                              // Priority Value
static const char * preemptOID = "2";                          // Preempt Value
static const char * priNotOID = "3";                          // Prior Notification notify
static const char * preNotOID = "4";                          // Preempt Notification


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// Must Declare for Factory Loader.
H460_FEATURE(OID6);

H460_FeatureOID6::H460_FeatureOID6()
: H460_FeatureOID(baseOID), remoteSupport(false), EP(NULL)
{
 PTRACE(6,"OID6\tInstance Created");

 FeatureCategory = FeatureSupported;

}

H460_FeatureOID6::~H460_FeatureOID6()
{
}

PStringArray H460_FeatureOID6::GetIdentifier()
{
    return PStringArray(baseOID);
}

void H460_FeatureOID6::AttachEndPoint(H323EndPoint * _ep)
{
    EP = _ep; 
}

PBoolean H460_FeatureOID6::OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu) 
{ 
    H460_FeatureOID feat = H460_FeatureOID(baseOID); 
    pdu = feat;

    return true; 
}
    
void H460_FeatureOID6::OnReceiveGatekeeperConfirm(const H225_FeatureDescriptor & pdu) 
{
    remoteSupport = true;
}


PBoolean H460_FeatureOID6::OnSendRegistrationRequest(H225_FeatureDescriptor & pdu) 
{ 
 // Build Message
    H460_FeatureOID feat = H460_FeatureOID(baseOID); 

    if ((EP->GetGatekeeper() != NULL) && 
        (EP->GetGatekeeper()->IsRegistered())) {
            pdu = feat;
            return true;
    }

    feat.Add(priorOID,H460_FeatureContent(EP->GetRegistrationPriority(),8));
    PBoolean preempt = EP->GetPreempt();
    feat.Add(preemptOID,H460_FeatureContent(preempt));
    pdu = feat;

    EP->SetPreempt(false);
    return true; 
}

void H460_FeatureOID6::OnReceiveRegistrationRequest(const H225_FeatureDescriptor & pdu) 
{ 
}

void H460_FeatureOID6::OnReceiveRegistrationReject(const H225_FeatureDescriptor & pdu)
{
  PTRACE(4,"OID6\tReceived Registration Request");

     H460_FeatureOID & feat = (H460_FeatureOID &)pdu;

    if (feat.Contains(preNotOID))   // Preemption notification
             EP->OnNotifyPreempt(false);
}

void H460_FeatureOID6::OnReceiveUnregistrationRequest(const H225_FeatureDescriptor & pdu)
{
  PTRACE(4,"OID6\tReceived Unregistration Request");

     H460_FeatureOID & feat = (H460_FeatureOID &)pdu;

    if (feat.Contains(priNotOID))    // Priority notification
             EP->OnNotifyPriority();

    if (feat.Contains(preNotOID))   // Preemption notification
             EP->OnNotifyPreempt(true);

}

#ifdef _MSC_VER
#pragma warning(default : 4239)
#endif

#endif