/*
 * h460_oid3.h
 *
 * H460 Presence implementation class.
 *
 * h323plus library
 *
 * Copyright (c) 2009 ISVO (Asia) Pte. Ltd.
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
 * The Initial Developer of the Original Code is ISVO (Asia) Pte. Ltd.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id $
 *
 *
 */

#ifndef H_H460_Featureoid3
#define H_H460_Featureoid3

#include <h460/h4601.h>
#include <h460/h460p.h>

// Must call the following
#include <ptlib/plugin.h>

#if _MSC_VER
#pragma once
#endif 

/////////////////////////////////////////////////////////////

class H460_FeatureOID3;
class H460PresenceHandler : public H323PresenceHandler
{
  public:

   H460PresenceHandler(H323EndPoint & _ep);

   void AttachFeature(H460_FeatureOID3 * _feat);

   void SetRegistered(bool registered);

   void SetPresenceState(const PStringList & epalias, 
						unsigned localstate, 
						const PString & localdisplay,
                        PBoolean updateOnly = false
                        );

   void AddInstruction(const PString & epalias, 
						H323PresenceHandler::InstType instType, 
						const PStringList & subscribe,
                        PBoolean autoSend = true);

   void AddAuthorization(const OpalGloballyUniqueID id,
						const PString & epalias,
						PBoolean approved,
						const PStringList & subscribe);
						

   PStringList & GetSubscriptionList();
   PStringList & GetBlockList();

  // Inherited
	virtual void OnNotification(MsgType tag,
								const H460P_PresenceNotification & notify,
								const H225_AliasAddress & addr
								);
	virtual void OnSubscription(MsgType tag,
								const H460P_PresenceSubscription & subscription,
								const H225_AliasAddress & addr
								);
	virtual void OnInstructions(MsgType tag,
								const H460P_ArrayOf_PresenceInstruction & instruction,
								const H225_AliasAddress & addr
								);

	// Events to notify endpoint
	void PresenceRcvNotification(const H225_AliasAddress & addr, const H323PresenceNotification & notify);
	void PresenceRcvAuthorization(const H225_AliasAddress & addr, const H323PresenceSubscription & subscript);
	void PresenceRcvInstruction(const H225_AliasAddress & addr, const H323PresenceInstruction & instruct);

	void AddEndpointFeature(int feat);
	void AddEndpointH460Feature(const H225_GenericIdentifier & featid, const PString & display);
	void AddEndpointGenericData(const H225_GenericData & data);

	localeInfo & GetLocationInfo() { return EndpointLocale; }

 private:
 	// Lists
	PStringList PresenceSubscriptions;
	PStringList PresenceBlockList;
	list<H460P_PresenceFeature>   EndpointFeatures;
	localeInfo  EndpointLocale;

	H225_ArrayOf_GenericData genericData;

	PBoolean presenceRegistration;
	PBoolean pendingMessages;
	PDECLARE_NOTIFIER(PTimer, H460PresenceHandler, dequeue);
	PTimer	QueueTimer;	

    H323EndPoint & ep;
    H460_FeatureOID3 * feat;
};

/////////////////////////////////////////////////////////////

class H323EndPoint;
class H460_FeatureOID3 : public H460_FeatureOID 
{
    PCLASSINFO(H460_FeatureOID3,H460_FeatureOID);

public:

    H460_FeatureOID3();
    virtual ~H460_FeatureOID3();

    virtual void AttachEndPoint(H323EndPoint * _ep);

    static PStringArray GetFeatureName() { return PStringArray("OID3"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("Presence"); };
    static int GetPurpose()	{ return FeatureRas; };
	static PStringArray GetIdentifier();

	virtual PBoolean CommonFeature() { return remoteSupport; }

    virtual PBoolean OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu);
    virtual PBoolean OnSendRegistrationRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveRegistrationConfirm(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendServiceControlIndication(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveServiceControlIndication(const H225_FeatureDescriptor & pdu);

private:

    PBoolean remoteSupport;
	H460PresenceHandler * handler;

};

#ifndef _WIN32_WCE
	#if PTLIB_VER > 260
	   PPLUGIN_STATIC_LOAD(OID3, H460_Feature);
	#else
	   PWLIB_STATIC_LOAD_PLUGIN(OID3, H460_Feature);
	#endif
#endif


#endif
