/* H460_std9.h
 *
 * Copyright (c) 2009 ISVO (Asia) Pte Ltd. All Rights Reserved.
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
 * $Log$
 * Revision 1.3  2009/09/29 07:23:03  shorne
 * Change the way unmatched features are cleaned up in call signalling. Removed advertisement of H.460.19 in Alerting and Connecting PDU
 *
 * Revision 1.2  2009/08/28 14:36:06  shorne
 * Fixes to enable compilation with PTLIB 2.6.4
 *
 * Revision 1.1  2009/08/21 07:01:06  shorne
 * Added H.460.9 Support
 *
 *
 *
 *
 */

#ifndef H_H460_FeatureStd9
#define H_H460_FeatureStd9


#include <h460/h4601.h>

// Must call the following
#include <ptlib/plugin.h>

#if _MSC_VER
#pragma once
#endif 


class MyH323EndPoint;
class MyH323Connection;
class H4609_ArrayOf_RTCPMeasures;
class H460_FeatureStd9 : public H460_FeatureStd 
{
    PCLASSINFO(H460_FeatureStd9,H460_FeatureStd);

public:

    H460_FeatureStd9();
    virtual ~H460_FeatureStd9();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);
    virtual void AttachConnection(H323Connection * _con);

    static PStringArray GetFeatureName() { return PStringArray("Std9"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("QoS Monitoring-H.460.9"); };
    static int GetPurpose()	{ return FeatureSignal; };
	static PStringArray GetIdentifier() { return PStringArray("9"); };

	virtual PBoolean CommonFeature() { return qossupport; }

	// Messages
    virtual PBoolean OnSendAdmissionRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu);

	// Send QoS information
	virtual PBoolean OnSendInfoRequestResponseMessage(H225_FeatureDescriptor & pdu);
    virtual PBoolean OnSendDisengagementRequestMessage(H225_FeatureDescriptor & pdu);

private:
	PBoolean GenerateReport(H4609_ArrayOf_RTCPMeasures & report);
	PBoolean WriteStatisticsReport(H460_FeatureStd & msg, PBoolean final);

    H323EndPoint * EP;
    H323Connection * CON;
    PBoolean qossupport;
	PBoolean finalonly;

};

// Need to declare for Factory Loader
#ifndef _WIN32_WCE
	#if PTLIB_VER > 260
	   PPLUGIN_STATIC_LOAD(Std9, H460_Feature);
	#else
	   PWLIB_STATIC_LOAD_PLUGIN(Std9, H460_Feature);
	#endif
#endif

#endif

