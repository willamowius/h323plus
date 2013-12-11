/* h460_std22.h
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

#ifndef H_H460_FeatureStd22
#define H_H460_FeatureStd22


#include <h460/h4601.h>

// Must call the following
#include <ptlib/plugin.h>

#if _MSC_VER
#pragma once
#endif 

class H323TransportSecurity;
class H460_FeatureStd22 : public H460_FeatureStd 
{
    PCLASSINFO(H460_FeatureStd22,H460_FeatureStd);

public:

    H460_FeatureStd22();
    virtual ~H460_FeatureStd22();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);
    virtual void AttachConnection(H323Connection * _con);
 
    static PStringArray GetFeatureName() { return PStringArray("Std22"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("H.225.0 Sec-H.460.22"); };
    static int GetPurpose();
	static PStringArray GetIdentifier() { return PStringArray("22"); };

    virtual PBoolean FeatureAdvertised(int mtype);
	virtual PBoolean CommonFeature() { return isEnabled; }

	// Messages
    virtual PBoolean OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveGatekeeperConfirm(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendRegistrationRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveRegistrationConfirm(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendAdmissionRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu);


private:
    H323EndPoint * EP;
    H323Connection * CON;
    PBoolean isEnabled;
};

// Need to declare for Factory Loader
#ifndef _WIN32_WCE
	#if PTLIB_VER > 260
	   PPLUGIN_STATIC_LOAD(Std22, H460_Feature);
	#else
	   PWLIB_STATIC_LOAD_PLUGIN(Std22, H460_Feature);
	#endif
#endif

#endif

