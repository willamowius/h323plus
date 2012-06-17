/*
 * h460p.h
 *
 * H460 Presence class.
 *
 * h323plus library
 *
 * Copyright (c) 2008 ISVO (Asia) Pte. Ltd.
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
 * $Id$
 *
 *
 */


#pragma once

#include "openh323buildopts.h"

#ifdef H323_H460P

#include <ptlib.h>
#include <h460/h460pres.h>
#include <guid.h>
#include <transports.h>
#include <list>
#include <map>


class H323PresenceNotification : public H460P_PresenceNotification
{

public:

	enum States {
      e_hidden,
      e_available,
      e_online,
      e_offline,
      e_onCall,
      e_voiceMail,
      e_notAvailable,
	  e_away,
	  e_generic
	};

	static PString GetStateString(unsigned state);

    void SetPresenceState(States state, const PString & display = PString());
    void SetGenericState(const PString & state);
	void GetPresenceState(States & state, PString & display) const;

	void AddSupportedFeature(int id);
	void AddSupportedFeature(const H460P_PresenceFeature & id);
	void AddEndpointLocale(const H460P_PresenceGeoLocation & loc);
	void AddGenericData(const H225_ArrayOf_GenericData & data);

	void AddSubscriber(const OpalGloballyUniqueID & guid);
	OpalGloballyUniqueID GetSubscriber(PINDEX i);
	void RemoveSubscribers();
    void AddAlias(const PString & alias);
	PString GetAlias() const;

};

class H323PresenceNotifications : public H460P_PresenceNotify  
{

  public:
    void Add(const H323PresenceNotification & notify);
	H323PresenceNotification & operator[](PINDEX i) const;

	void SetAlias(const PString & alias);
	void GetAlias(PString & alias);

	void SetAliasList(const PStringList & alias);
	void GetAliasList(PStringList & alias) const;

	void SetSize(PINDEX newSize);
	PINDEX GetSize() const;

 private:
	PStringList m_aliasList;
};

struct PresSubDetails
{
    PString m_display;
    PString m_avatar;
};

typedef std::map<PString,PresSubDetails> PresenceSubscriberList;

class H323PresenceSubscription : public H460P_PresenceSubscription
{

public:
	H323PresenceSubscription();
	H323PresenceSubscription(const OpalGloballyUniqueID & guid);

 // Sending Gatekeeper
	void SetSubscriptionDetails(const PString & subscribe, const PStringList & aliases);
	void SetSubscriptionDetails(const H225_AliasAddress & subscribe, const H225_AliasAddress & subscriber, 
                                const PString & display=PString(), const PString & avatar=PString());
	void GetSubscriberDetails(PresenceSubscriberList & aliases) const;
	void GetSubscriberDetails(PStringList & aliases) const;
	PString GetSubscribed();

	void SetGatekeeperRAS(const H323TransportAddress & address);
	H323TransportAddress GetGatekeeperRAS();

 // Receiving Gatekeeper/Endpoint
	void MakeDecision(bool approve);
	bool IsDecisionMade();
	int IsApproved(); // -1 not decided; 0 - not approved; 1 - approved;
	void SetTimeToLive(int t);
	int GetTimeToLive();

	void SetSubscription(const OpalGloballyUniqueID & guid);
    OpalGloballyUniqueID GetSubscription() const;

	void SetApproved(bool success);
	void AddGenericData(const H225_ArrayOf_GenericData & data);
    
};

class H323PresenceSubscriptions : public H460P_PresenceAuthorize
{
   public:
	void Add(const H323PresenceSubscription & sub);
	H323PresenceSubscription & operator[](PINDEX i) const;

	void SetAlias(const PString & alias);
	void GetAlias(PString & alias);

	void SetSize(PINDEX newSize);
	PINDEX GetSize() const;
};

class H323PresenceInstruction  :  public H460P_PresenceInstruction
{

 public:
	enum Instruction {
	  e_subscribe,
      e_unsubscribe,
      e_block,
      e_unblock,
	  e_pending
	};

	static PString GetInstructionString(unsigned instruct);
 
    H323PresenceInstruction(Instruction instruct, const PString & alias, 
                            const PString & display = PString(), const PString & avatar = PString());

	Instruction GetInstruction();
    PString GetAlias() const;
	PString GetAlias(PString & display, PString & avatar) const;

};

class H323PresenceInstructions  : public H460P_PresenceInstruct
{
  public:
	void Add(const H323PresenceInstruction & instruct);
	H323PresenceInstruction & operator[](PINDEX i) const;

	void SetAlias(const PString & alias);
	void GetAlias(PString & alias);

	void SetSize(PINDEX newSize);
	PINDEX GetSize() const;
};

class H323PresenceIdentifiers   : public H460P_ArrayOf_PresenceIdentifier
{

  public:
	void Add(const OpalGloballyUniqueID & guid, PBoolean todelete = false);
	OpalGloballyUniqueID GetIdentifier(PINDEX i);
};

struct H323PresenceRecord
{
	H225_EndpointIdentifier m_identifer;
	H225_AliasAddress		m_locAlias;
	H225_AliasAddress		m_subAlias;
	time_t					m_timeStamp;
	unsigned				m_ttl;
};

struct H323PresenceID
{
	PBoolean								m_isSubscriber;
	H225_AliasAddress						m_subscriber;
	H225_AliasAddress						m_Alias;
    PString                                 m_Display;
    PString                                 m_Avatar;
	H323PresenceInstruction::Instruction	m_Status;
	PBoolean								m_Active;
	PTime									m_Updated;
};

struct H323PresenceEndpoint
{
	H323PresenceSubscriptions	m_Authorize;
    H323PresenceInstructions	m_Instruction;
    H323PresenceNotifications	m_Notify;
	H323PresenceIdentifiers		m_Identifiers;
};

#define H323PresenceStore		std::map<H225_AliasAddress,H323PresenceEndpoint>
#define H323PresenceGkStore		std::map<H225_TransportAddress,H323PresenceEndpoint>

template<class Msg>
struct Order {
  bool operator()(Msg s1, Msg s2) const
  {
    return s1.Compare(s2) < 0;
  }
};

// Indicia
#define H323PresenceInd			std::map<H225_AliasAddress,list<H460P_PresencePDU>,Order<H225_AliasAddress> >


// Gatekeeper functions
#define H323PresenceAlias		std::map<H225_AliasAddress,H225_EndpointIdentifier,Order<H225_AliasAddress> >
#define H323PresenceLocal		std::map<H225_EndpointIdentifier, H323PresenceInd,Order<H225_EndpointIdentifier> >

#define H323PresenceExternal	std::map<H225_AliasAddress,H225_TransportAddress,Order<H225_AliasAddress> >
#define H323PresenceRemote		std::map<H225_TransportAddress, H323PresenceInd>
#define H323PresenceLRQRelay	std::map<H460P_PresenceIdentifier,H225_TransportAddress,Order<H460P_PresenceIdentifier> >

#define H323PresenceIds			std::map<H460P_PresenceIdentifier,H323PresenceID,Order<H460P_PresenceIdentifier> >
#define H323PresencePending		std::map<H225_AliasAddress,H460P_PresenceIdentifier, Order<H225_AliasAddress> >
#define H323PresenceIdMap		std::map<H225_AliasAddress,H323PresencePending, Order<H225_AliasAddress> >


// Derive you implementation from H323PresenceHandler.

class H323PresenceHandler  : public PObject
{
    PCLASSINFO(H323PresenceHandler, PObject);

public:
	bool ReceivedPDU(const PASN_OctetString & pdu, H225_TransportAddress * ip = NULL);

    enum MsgType {
		e_Status,
		e_Instruct,
		e_Authorize,
		e_Notify,
		e_Request,
		e_Response,
		e_Alive,
		e_Remove,
		e_Alert
    };

	enum InstType {
		e_subscribe,
		e_unsubscribe,
		e_block,
		e_unblock,
		e_pending
	};

	class localeInfo {
		public:
			localeInfo();
			bool BuildLocalePDU(H460P_PresenceGeoLocation & pdu);

			PString		m_locale;
			PString		m_region;
			PString		m_country;
			PString		m_countryCode;
			PString		m_latitude;
			PString		m_longitude;
			PString		m_elevation;
	};

	PBoolean BuildPresenceElement(unsigned msgtag,					///< RAS Message ID
							PASN_OctetString & pdu					///< Encoded PresenceElement
							);

	PBoolean BuildPresenceElement(unsigned msgtag,					///< Presence Message ID
							const H225_EndpointIdentifier & ep,		///< Endpoint Identifier
							PASN_OctetString & pdu					///< Presence Message elements
							);

	PBoolean BuildPresenceElement(unsigned msgtag,					///< Presence Message ID
							const H225_TransportAddress & ip,		///< Transport Address
							PASN_OctetString & pdu					///< Presence Message elements
							);

	PBoolean BuildPresenceMessage(unsigned id,						///< Presence Message ID
							H323PresenceStore & store,				///< Presence Store Information
							H460P_ArrayOf_PresenceMessage & element	///< Presence Message elements
							);

	PBoolean BuildPresenceMessage(unsigned id,						///< Presence Message ID 
							const H225_EndpointIdentifier & ep, 	///< Endpoint Identifier
							H460P_ArrayOf_PresenceMessage & msgs	///< Presence Message elements
							);

	PBoolean BuildPresenceMessage(unsigned id,						///< Presence Message ID 
							const H225_TransportAddress & ip, 		///< Transport Address
							H460P_ArrayOf_PresenceMessage & msgs	///< Presence Message elements
							);

	virtual H323PresenceStore & GetPresenceStoreLocked(unsigned msgtag =0);
	virtual void PresenceStoreUnLock(unsigned msgtag = 0);

  // Events Endpoints
	virtual void OnNotification(MsgType /*tag*/,
								const H460P_PresenceNotification & /*notify*/,
								const H225_AliasAddress & /*addr*/
								) {}

	virtual void OnSubscription(MsgType /*tag*/,
								const H460P_PresenceSubscription & /*subscription*/,
								const H225_AliasAddress & /*addr*/
								) {}

	virtual void OnInstructions(MsgType /*tag*/,
								const H460P_ArrayOf_PresenceInstruction & /*instruction*/,
								const H225_AliasAddress & /*addr*/
								) {}

  // Events Gatekeepers
	virtual void OnNotification(MsgType /*tag*/,
								const H460P_PresenceNotification & /*notify*/,
								const H225_TransportAddress & /*ip*/
								) {}

	virtual void OnSubscription(MsgType /*tag*/,
								const H460P_PresenceSubscription & /*subscription*/,
								const H225_TransportAddress & /*ip*/
								) {}

	virtual void OnIdentifiers(MsgType /*tag*/, 
								const H460P_PresenceIdentifier & /*identifier*/,
								const H225_TransportAddress & /*ip*/
								) {}


  // Build Endpoint Callbacks
	virtual PBoolean BuildSubscription(const H225_EndpointIdentifier & /*ep*/,
								H323PresenceStore & /*subscription*/
								) { return false; }

	virtual PBoolean BuildNotification(const H225_EndpointIdentifier & /*ep*/,
								H323PresenceStore & /*notify*/
								) { return false; }

	virtual PBoolean BuildInstructions(const H225_EndpointIdentifier & /*ep*/,
								H323PresenceStore & /*instruction*/
								) { return false; }

  // Build Gatekeeper Callbacks
	virtual PBoolean BuildSubscription(bool /*request*/,
								const H225_TransportAddress & /*ip*/,
								H323PresenceGkStore & /*subscription*/
								) { return false; }

	virtual PBoolean BuildNotification(
								const H225_TransportAddress & /*ip*/,
								H323PresenceGkStore & /*notify*/
								) { return false; }

	virtual PBoolean BuildIdentifiers(bool /*alive*/,
								const H225_TransportAddress & /*ip*/,
								H323PresenceGkStore & /*identifiers*/
								) { return false; }

protected:
// Build Messages
     H460P_PresenceStatus & BuildStatus(H460P_PresenceMessage & msg, 
									const H323PresenceNotifications & notify,
									const H323PresenceInstructions & inst);
     H460P_PresenceInstruct & BuildInstruct(H460P_PresenceMessage & msg, 
									const H323PresenceInstructions & inst);
     H460P_PresenceAuthorize & BuildAuthorize(H460P_PresenceMessage & msg, 
									const H323PresenceSubscriptions & subs);
     H460P_PresenceNotify & BuildNotify(H460P_PresenceMessage & msg, 
									const H323PresenceNotifications & notify);
     H460P_PresenceRequest & BuildRequest(H460P_PresenceMessage & msg, 
									const H323PresenceSubscriptions & subs);
     H460P_PresenceResponse & BuildResponse(H460P_PresenceMessage & msg, 
									const H323PresenceSubscriptions & subs);
     H460P_PresenceAlive & BuildAlive(H460P_PresenceMessage & msg, 
									const H323PresenceIdentifiers & id);
     H460P_PresenceRemove & BuildRemove(H460P_PresenceMessage & msg, 
									const H323PresenceIdentifiers & id);
     H460P_PresenceAlert & BuildAlert(H460P_PresenceMessage & msg, 
									const H323PresenceNotifications & notify);

private:
	 H323PresenceStore m_presenceStore;
	 PMutex storeMutex;
};


#endif


