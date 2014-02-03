/* H460_std9.h
 *
 * Copyright (c) 2014 Spranto International Pte Ltd. All Rights Reserved.
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
 * The Initial Developer of the Original Code is ISVO (Asia) Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef H_H460_FeatureStd25
#define H_H460_FeatureStd25


#include <h460/h4601.h>

// Must call the following
#include <ptlib/plugin.h>

#if _MSC_VER
#pragma once
#endif 

/*
    Note: H.460.25 allows for sending geographic information in the RAS channel
    This implementation restricts transmission to the signalling channel to
    take advantage of a reliable secure transport that is securable. (if supported)
*/

class H460_FeatureStd25 : public H460_FeatureStd 
{
    PCLASSINFO(H460_FeatureStd25,H460_FeatureStd);

public:

    H460_FeatureStd25();
    virtual ~H460_FeatureStd25();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);
    virtual void AttachConnection(H323Connection * _con);

    static PStringArray GetFeatureName() { return PStringArray("Std25"); }
    static PStringArray GetFeatureFriendlyName() { return PStringArray("Geographic Info-H.460.25"); }
    static int GetPurpose()	{ return FeatureSignal; }
	static PStringArray GetIdentifier() { return PStringArray("25"); }

    virtual PBoolean FeatureAdvertised(int mtype);
	virtual PBoolean CommonFeature() { return enabled; }

	// Messages
    virtual PBoolean OnSendAdmissionRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendSetup_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendAlerting_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveAlerting_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendCallConnect_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveCallConnect_UUIE(const H225_FeatureDescriptor & pdu);

protected:
    PBoolean Build_PIDF_LO_Message(const PString & message);

private:
    H323EndPoint * EP;
    H323Connection * CON;
    PBoolean enabled;
    PBoolean dataToSend;
};

// Need to declare for Factory Loader
#ifndef _WIN32_WCE
	#if PTLIB_VER > 260
	   PPLUGIN_STATIC_LOAD(Std25, H460_Feature);
	#else
	   PWLIB_STATIC_LOAD_PLUGIN(Std25, H460_Feature);
	#endif
#endif

#endif

