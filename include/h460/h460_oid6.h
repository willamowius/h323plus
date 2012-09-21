/* H460_OID6.h
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

#ifndef H_H460_FeatureOID6
#define H_H460_FeatureOID6

#include <h460/h4601.h>

// Must call the following
#include <ptlib/plugin.h>

#if _MSC_VER
#pragma once
#endif 

class H323EndPoint;
class H460_FeatureOID6 : public H460_FeatureOID 
{
    PCLASSINFO(H460_FeatureOID6,H460_FeatureOID);

public:

    H460_FeatureOID6();
    virtual ~H460_FeatureOID6();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);

    static PStringArray GetFeatureName() { return PStringArray("OID6"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("Registration Priority & Preemption"); };
    static int GetPurpose()    { return FeatureRas; };
    static PStringArray GetIdentifier();

    virtual PBoolean CommonFeature() { return remoteSupport; }

   // Messages
   // GK -> EP
    virtual PBoolean OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveGatekeeperConfirm(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendRegistrationRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveRegistrationRequest(const H225_FeatureDescriptor & pdu);

    virtual void OnReceiveRegistrationReject(const H225_FeatureDescriptor & pdu);
    virtual void OnReceiveUnregistrationRequest(const H225_FeatureDescriptor & pdu);

 
private:
    PBoolean remoteSupport;
    H323EndPoint * EP;

};

// Need to declare for Factory Loader
#ifndef _WIN32_WCE
    #if PTLIB_VER > 260
       PPLUGIN_STATIC_LOAD(OID6, H460_Feature);
    #else
       PWLIB_STATIC_LOAD_PLUGIN(OID6, H460_Feature);
    #endif
#endif
#endif
