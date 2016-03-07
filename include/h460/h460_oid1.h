/*
* h460_oid1.h
*
* Copyright (c) 2015 Spranto International Pte Ltd. All Rights Reserved.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, you can obtain one at http://mozilla.org/MPL/2.0/
*
* Software distributed under the License is distributed on an "AS IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
* the License for the specific language governing rights and limitations
* under the License.
*
* Contributor(s): ______________________________________.
*
*
*/

#ifndef H_H460_FeatureOID1
#define H_H460_FeatureOID1

#if _MSC_VER
#pragma once
#endif 

class MyH323EndPoint;
class MyH323Connection;
class H460_FeatureOID1 : public H460_FeatureOID 
{
    PCLASSINFO(H460_FeatureOID1,H460_FeatureOID);

public:

    H460_FeatureOID1();
    virtual ~H460_FeatureOID1();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);
    virtual void AttachConnection(H323Connection * _con);

    static PStringList GetFeatureName() { return PStringList("OID1"); };
	static PStringArray GetFeatureFriendlyName() { return PStringList("Text Messaging"); };
    static int GetPurpose()	{ return FeatureSignal; };
	static PStringArray GetIdentifier();

	virtual PBoolean CommonFeature() { return remoteSupport; }

    virtual PBoolean SupportNonCallService();

    // H.323 Message Manuipulation
        // Advertise the Feature
    virtual PBoolean OnSendSetup_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendCallProceeding_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveCallProceeding_UUIE(const H225_FeatureDescriptor & pdu); 

    virtual PBoolean OnSendAlerting_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveAlerting_UUIE(const H225_FeatureDescriptor & pdu);

	// Send/Recieve Message
    virtual PBoolean OnSendFacility_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveFacility_UUIE(const H225_FeatureDescriptor & pdu);

	// Release Complete
    virtual PBoolean OnSendReleaseComplete_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveReleaseComplete_UUIE(const H225_FeatureDescriptor & pdu);  

	virtual PBoolean OnSendAdmissionRequest(H225_FeatureDescriptor & pdu);
	

private:
	PString callToken;      // Call
	PBoolean remoteSupport;
	PBoolean sessionOpen;

private:
    H323EndPoint   * m_ep;
    H323Connection * m_con;

};

// Need to declare for Factory Loader
#ifndef _WIN32_WCE
	#if PTLIB_VER > 260
	   PPLUGIN_STATIC_LOAD(OID1, H460_Feature);
	#else
	   PWLIB_STATIC_LOAD_PLUGIN(OID1, H460_Feature);
	#endif
#endif

#endif
