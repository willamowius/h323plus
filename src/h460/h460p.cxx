/*
 * h460p.cxx
 *
 * H460 Presence class.
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
 * $Log$
 * Revision 1.3  2008/02/21 00:04:33  shorne
 * fix variable naming issue on linux compile
 *
 * Revision 1.2  2008/01/30 02:53:42  shorne
 * code format tidy up
 *
 * Revision 1.1  2008/01/29 04:38:13  shorne
 * completed Initial implementation
 *
 *
 *
 */

#include <ptlib.h>
#include <h323pdu.h>
#include "h460/h460p.h"

#ifdef H323_H460

static struct {
  unsigned msgid;
  int preStatus;			
  int preInstruct;			
  int preAuthorize;		
  int preNotify;	
  int preRequest;	
  int preResponse;
  int preAlive;
  int preRemove;
  int preAlert;
} RASMessage_attributes[] = {					//   st  ins aut not req res alv rem alt
			{ H225_RasMessage::e_gatekeeperRequest,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_gatekeeperConfirm,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_gatekeeperReject,0,0,0,0,0,0,0,0,0},
	{ H225_RasMessage::e_registrationRequest		,1	,0	,0	,0	,0	,0	,0	,0	,0	 },
	{ H225_RasMessage::e_registrationConfirm		,1	,0	,0	,0	,0	,0	,0	,0	,0	 },
			{ H225_RasMessage::e_registrationReject,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_unregistrationRequest,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_unregistrationConfirm,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_unregistrationReject,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_admissionRequest,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_admissionConfirm,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_admissionReject,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_bandwidthRequest,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_bandwidthConfirm,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_bandwidthReject,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_disengageRequest,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_disengageConfirm,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_disengageReject,0,0,0,0,0,0,0,0,0},
	{ H225_RasMessage::e_locationRequest			,0	,0	,1	,0	,1	,1	,1	,1	,1	 },
	{ H225_RasMessage::e_locationConfirm			,0	,0	,0	,0	,0	,2	,0	,2	,0	 },
			{ H225_RasMessage::e_locationReject,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_infoRequest,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_infoRequestResponse,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_nonStandardMessage,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_unknownMessageResponse,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_requestInProgress,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_resourcesAvailableIndicate,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_resourcesAvailableConfirm,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_infoRequestAck,0,0,0,0,0,0,0,0,0},
			{ H225_RasMessage::e_infoRequestNak,0,0,0,0,0,0,0,0,0},
	{ H225_RasMessage::e_serviceControlIndication	,1	,1	,0	,1	,0	,0	,0	,0	,0	 },
			{ H225_RasMessage::e_serviceControlResponse,0,0,0,0,0,0,0,0,0}
};

static struct {
  unsigned id;
  int     notification;
  int     subscription;
  int     instruction;
  int     identifier;
  int     cryptoTokens;
} PresenceMessage_attributes[] = {
//messageId 									Notification	Subscription	Instruction	 Identifier	 Crypto/Tokens
{ H460P_PresenceMessage::e_presenceStatus			,1				,0				,2			,0          ,0 },	
{ H460P_PresenceMessage::e_presenceInstruct			,0				,0				,1			,0			,0 },		
{ H460P_PresenceMessage::e_presenceAuthorize		,0				,1				,0			,0			,0 },			
{ H460P_PresenceMessage::e_presenceNotify			,1				,0				,0			,0			,0 },				
{ H460P_PresenceMessage::e_presenceRequest			,0				,1				,0			,0			,3 },	
{ H460P_PresenceMessage::e_presenceResponse			,0				,1				,0			,0			,2 },
{ H460P_PresenceMessage::e_presenceAlive			,0				,0				,0			,1			,0 },	
{ H460P_PresenceMessage::e_presenceRemove			,0				,0				,0			,1			,0 },	
{ H460P_PresenceMessage::e_presenceAlert			,1				,0				,0			,0			,0 }				
};
//  0 - not used   1 - mandatory  2 - optional  3 - recommended


// Presence message abbreviations
const char *PresName[] = {
	"Status",
	"Instruct",
	"Authorize",
	"Notify",
	"Request",
	"Response",
	"Alive",
	"Remove",
	"Alert",
	"NotRecognized"		// for new messages 
};
const unsigned MaxPresTag =  H460P_PresenceMessage::e_presenceAlert;


///////////////////////////////////////////////////////////////////

struct H323PresenceMessage {
	H460P_PresenceMessage m_recvPDU;
	H323PresenceHandler * m_handler;

	unsigned GetTag() const { return m_recvPDU.GetTag(); }
	const char *GetTagName() const;
};


const char *H323PresenceMessage::GetTagName() const
{
	return (GetTag() <= MaxPresTag) ? PresName[GetTag()] : PresName[MaxPresTag+1];
}

///////////////////////////////////////////////////////////////////


class H323PresenceBase  
{
 public: 
	H323PresenceBase(const H323PresenceMessage & m);
	H323PresenceBase();
	virtual ~H323PresenceBase() {};

	bool Process();

 protected:

    bool ReadNotification(const H460P_ArrayOf_PresenceNotification & notify, const H225_AliasAddress & addr);
	bool ReadSubscription(const H460P_ArrayOf_PresenceSubscription & subscription, const H225_AliasAddress & addr);
	bool ReadInstruction(const H460P_ArrayOf_PresenceInstruction & instruction, const H225_AliasAddress & addr);

	bool ReadSubscription(const H460P_ArrayOf_PresenceSubscription & subscription);
	bool ReadIdentifier(const H460P_ArrayOf_PresenceIdentifier & identifier);

	virtual bool HandleNotification(bool /*opt*/) { return false; }
	virtual bool HandleSubscription(bool /*opt*/) { return false; }
	virtual bool HandleInstruction(bool /*opt*/) { return false; }
	virtual bool HandleIdentifier(bool /*opt*/) { return false; }
	virtual bool HandleCryptoTokens(bool /*opt*/) { return false; }

    unsigned tag;
    H323PresenceHandler * handler;
};

H323PresenceBase::H323PresenceBase()
 : tag(100), handler(NULL)
{

}

H323PresenceBase::H323PresenceBase(const H323PresenceMessage & m) 
: tag(m.GetTag()), handler(m.m_handler)
{

}

bool H323PresenceBase::Process()
{
  if (tag > MaxPresTag) {
	 PTRACE(2,"PRESENCE\tReceived unrecognised Presence Message!");
	 return false;
  }

  if (PresenceMessage_attributes[tag].notification > 0)
	  HandleNotification((PresenceMessage_attributes[tag].notification >1));

  if (PresenceMessage_attributes[tag].subscription > 0)
	  HandleSubscription((PresenceMessage_attributes[tag].subscription >1));

  if (PresenceMessage_attributes[tag].instruction > 0)
	  HandleInstruction((PresenceMessage_attributes[tag].instruction >1));

  if (PresenceMessage_attributes[tag].identifier > 0)
	  HandleIdentifier((PresenceMessage_attributes[tag].identifier >1));

  if (PresenceMessage_attributes[tag].cryptoTokens > 0)
	  HandleCryptoTokens((PresenceMessage_attributes[tag].cryptoTokens >1));

  return true;

}

bool H323PresenceBase::ReadNotification(const H460P_ArrayOf_PresenceNotification & notify, const H225_AliasAddress & addr)
{
	for (PINDEX i = 0; i < notify.GetSize(); i++)
		handler->OnNotification((H323PresenceHandler::MsgType)tag, notify[i],addr);

	return true;
}

bool H323PresenceBase::ReadSubscription(const H460P_ArrayOf_PresenceSubscription & subscription, const H225_AliasAddress & addr)
{
	for (PINDEX i = 0; i < subscription.GetSize(); i++)
        	handler->OnSubscription((H323PresenceHandler::MsgType)tag, subscription[i],addr);

	return true;
}

bool H323PresenceBase::ReadSubscription(const H460P_ArrayOf_PresenceSubscription & subscription)
{
	for (PINDEX i = 0; i < subscription.GetSize(); i++)
        	handler->OnSubscription((H323PresenceHandler::MsgType)tag, subscription[i]);

	return true;
}

bool H323PresenceBase::ReadInstruction(const H460P_ArrayOf_PresenceInstruction & instruction, const H225_AliasAddress & addr)
{
	handler->OnInstructions((H323PresenceHandler::MsgType)tag, instruction,addr);
	return true;
}

bool H323PresenceBase::ReadIdentifier(const H460P_ArrayOf_PresenceIdentifier & identifier)
{
	handler->OnIdentifiers((H323PresenceHandler::MsgType)tag, identifier);
	return true;
}

////////////////////////////////////////////////////////////////////////


template<class Msg>
class H323PresencePDU : public H323PresenceBase {
  public:

	H323PresencePDU(const H323PresenceMessage & m) : H323PresenceBase(m), request(m.m_recvPDU) {}
	H323PresencePDU(unsigned tag) : request(*new Msg) {}

	operator Msg & () { return request; }
	operator const Msg & () const { return request; }

  protected:
	Msg request;
};

class H323PresenceStatus  : public H323PresencePDU<H460P_PresenceStatus>
{
  public:
	H323PresenceStatus(const H323PresenceMessage & m) : H323PresencePDU<H460P_PresenceStatus>(m) {}

  protected:
	virtual bool HandleNotification(bool opt);
	virtual bool HandleInstruction(bool opt); 
};

class H323PresenceInstruct  : public H323PresencePDU<H460P_PresenceInstruct>
{
  public:
	H323PresenceInstruct(const H323PresenceMessage & m) : H323PresencePDU<H460P_PresenceInstruct>(m) {}

  protected:
	virtual bool HandleInstruction(bool opt); 
};	

class H323PresenceAuthorize  : public H323PresencePDU<H460P_PresenceAuthorize>
{
  public:
	H323PresenceAuthorize(const H323PresenceMessage & m) : H323PresencePDU<H460P_PresenceAuthorize>(m) {}

  protected:
	virtual bool HandleSubscription(bool opt); 
};
		
class H323PresenceNotify  : public H323PresencePDU<H460P_PresenceNotify>
{
  public:
	H323PresenceNotify(const H323PresenceMessage & m) : H323PresencePDU<H460P_PresenceNotify>(m) {}

  protected:
	virtual bool HandleNotification(bool opt);
};
				
class H323PresenceRequest  : public H323PresencePDU<H460P_PresenceRequest>
{
  public:
	H323PresenceRequest(const H323PresenceMessage & m) : H323PresencePDU<H460P_PresenceRequest>(m) {}

  protected:
	virtual bool HandleSubscription(bool opt); 
};

class H323PresenceResponse  : public H323PresencePDU<H460P_PresenceResponse>
{
  public:
	H323PresenceResponse(const H323PresenceMessage & m) : H323PresencePDU<H460P_PresenceResponse>(m) {}

  protected:
	virtual bool HandleSubscription(bool opt);

};

class H323PresenceAlive  : public H323PresencePDU<H460P_PresenceAlive>
{
  public:
	H323PresenceAlive(const H323PresenceMessage & m) : H323PresencePDU<H460P_PresenceAlive>(m) {}

  protected:
	virtual bool HandleIdentifier(bool opt); 
};


class H323PresenceRemove  : public H323PresencePDU<H460P_PresenceRemove>
{
  public:
	H323PresenceRemove(const H323PresenceMessage & m) : H323PresencePDU<H460P_PresenceRemove>(m) {}

  protected:
	virtual bool HandleIdentifier(bool opt);
};

class H323PresenceAlert  : public H323PresencePDU<H460P_PresenceAlert>
{
  public:
	H323PresenceAlert(const H323PresenceMessage & m) : H323PresencePDU<H460P_PresenceAlert>(m) {}

  protected:
	virtual bool HandleNotification(bool opt);

};


////////////////////////////////////////////////////////////////////////////



bool H323PresenceHandler::ReceivedPDU(const PASN_OctetString & pdu)
{
    H460P_PresenceElement element;
	PPER_Stream raw(pdu);
	if (!element.Decode(raw) || element.m_message.GetSize() == 0) {
		PTRACE(2,"PRES\tError Decoding Element. Malformed or no messages."); 
		return false;
	}

	for (PINDEX i=0; i < element.m_message.GetSize(); i++) {
		H323PresenceMessage m;
		m.m_recvPDU = element.m_message[i];
		m.m_handler = this;

		H323PresenceBase handler;
		switch (m.GetTag())
		{
			case H460P_PresenceMessage::e_presenceStatus:
				handler = H323PresenceStatus(m);
				break;
			case H460P_PresenceMessage::e_presenceInstruct:
				handler = H323PresenceInstruct(m);
				break;
			case H460P_PresenceMessage::e_presenceAuthorize:
				handler = H323PresenceAuthorize(m);
				break;
			case H460P_PresenceMessage::e_presenceNotify:
				handler = H323PresenceNotify(m);
				break;
			case H460P_PresenceMessage::e_presenceRequest:
				handler = H323PresenceRequest(m);
				break;
			case H460P_PresenceMessage::e_presenceResponse:
				handler = H323PresenceResponse(m);
				break;
			case H460P_PresenceMessage::e_presenceAlive:
				handler = H323PresenceAlive(m);
				break;
			case H460P_PresenceMessage::e_presenceRemove:
				handler = H323PresenceRemove(m);
				break;
			case H460P_PresenceMessage::e_presenceAlert:
				handler = H323PresenceAlert(m);
				break;
			default:
				break;
		}

		if (!handler.Process()) {
			   PTRACE(2,"PRES\tUnable to handle Message." << m.GetTagName()); 
		}	
	}
	return true;
}


PBoolean H323PresenceHandler::BuildPresenceMessage(unsigned id, H323PresenceStore & store, H460P_ArrayOf_PresenceMessage & msgs)
{

	bool dataToSend = false;
	H323PresenceStore::iterator iter = store.begin();
	while (iter != m_presenceStore.end()) {
		H323PresenceEndpoint & ep = iter->second;
		bool ok = false;
		  if ((PresenceMessage_attributes[id].notification == 1) && (ep.m_Notify.GetSize() > 0)) ok = true;
  		  if ((PresenceMessage_attributes[id].subscription == 1) && (ep.m_Authorize.GetSize() > 0)) ok = true;
		  if ((PresenceMessage_attributes[id].instruction == 1) && (ep.m_Instruction.GetSize() > 0)) ok = true;
		  if ((PresenceMessage_attributes[id].identifier == 1) && (ep.m_Identifiers.GetSize() > 0)) ok = true;

		  if (ok) {
			dataToSend = true;
			int sz = msgs.GetSize();
			msgs.SetSize(sz+1);
			switch (id) {
				case H460P_PresenceMessage::e_presenceStatus:
					BuildStatus(msgs[sz], ep.m_Notify, ep.m_Instruction);
					break;
				case H460P_PresenceMessage::e_presenceInstruct:
					BuildInstruct(msgs[sz],ep.m_Instruction);
					break;
				case H460P_PresenceMessage::e_presenceAuthorize:
					BuildAuthorize(msgs[sz],ep.m_Authorize);
					break;
				case H460P_PresenceMessage::e_presenceNotify:
					BuildNotify(msgs[sz], ep.m_Notify);
					break;
				case H460P_PresenceMessage::e_presenceRequest:
					BuildRequest(msgs[sz],ep.m_Authorize);
					break;
				case H460P_PresenceMessage::e_presenceResponse:
					BuildResponse(msgs[sz],ep.m_Authorize);
					break;
				case H460P_PresenceMessage::e_presenceAlive:
					BuildAlive(msgs[sz], ep.m_Identifiers);
					break;
				case H460P_PresenceMessage::e_presenceRemove:
					BuildRemove(msgs[sz], ep.m_Identifiers);
					break;
				case H460P_PresenceMessage::e_presenceAlert:
					BuildAlert(msgs[sz], ep.m_Notify);
					break;
				default:
					break;
			}

			  if (ep.m_Notify.GetSize() > 0) ep.m_Notify.SetSize(0);
  			  if (ep.m_Authorize.GetSize() > 0) ep.m_Authorize.SetSize(0);
			  if (ep.m_Instruction.GetSize() > 0) ep.m_Instruction.SetSize(0);
			  if (ep.m_Identifiers.GetSize() > 0) ep.m_Identifiers.SetSize(0);
		  }
		iter++;
	}

	return dataToSend;
}

PBoolean H323PresenceHandler::BuildPresenceElement(unsigned msgtag, PASN_OctetString & pdu)
{
	bool success = false;
	H460P_PresenceElement element;
	H460P_ArrayOf_PresenceMessage & msgs = element.m_message;
	H323PresenceStore & store = GetPresenceStoreLocked(msgtag);
	
	if (RASMessage_attributes[msgtag].preStatus >0)
		BuildPresenceMessage(H460P_PresenceMessage::e_presenceStatus,store,msgs);
	if (RASMessage_attributes[msgtag].preInstruct >0)
		BuildPresenceMessage(H460P_PresenceMessage::e_presenceInstruct,store,msgs);
	if (RASMessage_attributes[msgtag].preAuthorize >0)
		BuildPresenceMessage(H460P_PresenceMessage::e_presenceAuthorize,store,msgs);
	if (RASMessage_attributes[msgtag].preNotify >0)
		BuildPresenceMessage(H460P_PresenceMessage::e_presenceNotify,store,msgs);
	if (RASMessage_attributes[msgtag].preRequest >0)
		BuildPresenceMessage(H460P_PresenceMessage::e_presenceRequest,store,msgs);
	if (RASMessage_attributes[msgtag].preResponse >0)
		BuildPresenceMessage(H460P_PresenceMessage::e_presenceResponse,store,msgs);
	if (RASMessage_attributes[msgtag].preAlive >0)
		BuildPresenceMessage(H460P_PresenceMessage::e_presenceAlive,store,msgs);
	if (RASMessage_attributes[msgtag].preRemove >0)
		BuildPresenceMessage(H460P_PresenceMessage::e_presenceRemove,store,msgs);
	if (RASMessage_attributes[msgtag].preAlert >0)
		BuildPresenceMessage(H460P_PresenceMessage::e_presenceAlert,store,msgs);

    success = (msgs.GetSize() > 0);
	if (success) {
		PTRACE(6,"PRES\tPDU to send\n" << element);
		pdu.EncodeSubType(element);
	}

	PresenceStoreUnLock(msgtag);

	return success;
}

H323PresenceStore & H323PresenceHandler::GetPresenceStoreLocked(unsigned /*msgtag*/)
{
	storeMutex.Wait();
	return m_presenceStore;
}

void H323PresenceHandler::PresenceStoreUnLock(unsigned /*msgtag*/)
{
	storeMutex.Signal();
}

///////////////////////////////////////////////////////////////////////

template<class Msg>
class H323PresenceMsg  : public H460P_PresenceMessage
{
 public:
	Msg & Build(unsigned _tag)
	{
		SetTag(_tag);
		Msg & msg = *this;
		return msg;
	}
};


H460P_PresenceStatus &  H323PresenceHandler::BuildStatus(H460P_PresenceMessage & msg, 
						const H323PresenceNotifications & notify,
						const H323PresenceInstructions & inst)
{
	H323PresenceMsg<H460P_PresenceStatus> m;
	H460P_PresenceStatus & pdu = m.Build(H460P_PresenceMessage::e_presenceStatus);
	pdu.m_notification = notify.m_notification;
	pdu.m_alias = notify.m_alias;

	if (inst.GetSize() > 0) {
		pdu.IncludeOptionalField(H460P_PresenceStatus::e_instruction);
		pdu.m_instruction = inst.m_instruction;
	}

	msg = *(H460P_PresenceMessage *)m.Clone();
	return msg;
}

H460P_PresenceInstruct &  H323PresenceHandler::BuildInstruct(H460P_PresenceMessage & msg, const H323PresenceInstructions & inst)
{
	H323PresenceMsg<H460P_PresenceInstruct> m;
	H460P_PresenceInstruct & pdu = m.Build(H460P_PresenceMessage::e_presenceInstruct);
	pdu = inst;

	msg = *(H460P_PresenceMessage *)m.Clone();
	return msg;
}

H460P_PresenceAuthorize &  H323PresenceHandler::BuildAuthorize(H460P_PresenceMessage & msg, const H323PresenceSubscriptions & subs)
{
	H323PresenceMsg<H460P_PresenceAuthorize> m;
	H460P_PresenceAuthorize & pdu = m.Build(H460P_PresenceMessage::e_presenceAuthorize);
	pdu = subs;

	msg = *(H460P_PresenceMessage *)m.Clone();
	return msg;
}

H460P_PresenceNotify &  H323PresenceHandler::BuildNotify(H460P_PresenceMessage & msg, const H323PresenceNotifications & notify)
{
	H323PresenceMsg<H460P_PresenceNotify> m;
	H460P_PresenceNotify & pdu = m.Build(H460P_PresenceMessage::e_presenceNotify);
	pdu = notify;

	msg = *(H460P_PresenceMessage *)m.Clone();
	return msg;
}
     
H460P_PresenceRequest &  H323PresenceHandler::BuildRequest(H460P_PresenceMessage & msg, const H323PresenceSubscriptions & subs)
{
	H323PresenceMsg<H460P_PresenceRequest> m;
	H460P_PresenceRequest & pdu = m.Build(H460P_PresenceMessage::e_presenceRequest);
	pdu.m_subscription = subs.m_subscription;

	msg = *(H460P_PresenceMessage *)m.Clone();
	return msg;
}
     
H460P_PresenceResponse &  H323PresenceHandler::BuildResponse(H460P_PresenceMessage & msg, const H323PresenceSubscriptions & subs)
{
	H323PresenceMsg<H460P_PresenceResponse> m;
	H460P_PresenceResponse & pdu = m.Build(H460P_PresenceMessage::e_presenceResponse);
	pdu.m_subscription = subs.m_subscription;

	msg = *(H460P_PresenceMessage *)m.Clone();
	return msg;;
}
     
H460P_PresenceAlive & H323PresenceHandler::BuildAlive(H460P_PresenceMessage & msg, const H323PresenceIdentifiers & id)
{
	H323PresenceMsg<H460P_PresenceAlive> m;
	H460P_PresenceAlive & pdu = m.Build(H460P_PresenceMessage::e_presenceAlive);
	pdu.m_identifier = id;

	msg = *(H460P_PresenceMessage *)m.Clone();
	return msg;
}
     
H460P_PresenceRemove & H323PresenceHandler::BuildRemove(H460P_PresenceMessage & msg, const H323PresenceIdentifiers & id)
{
	H323PresenceMsg<H460P_PresenceRemove> m;
	H460P_PresenceRemove & pdu = m.Build(H460P_PresenceMessage::e_presenceRemove);
	pdu.m_identifier = id;

	msg = *(H460P_PresenceMessage *)m.Clone();
	return msg;
}
     
H460P_PresenceAlert &  H323PresenceHandler::BuildAlert(H460P_PresenceMessage & msg, const H323PresenceNotifications & notify)
{
	H323PresenceMsg<H460P_PresenceAlert> m;
	H460P_PresenceAlert & pdu = m.Build(H460P_PresenceMessage::e_presenceAlert);
	pdu.m_notification = notify.m_notification;

	msg = *(H460P_PresenceMessage *)m.Clone();
	return msg;
}

////////////////////////////////////////////////////////////////////////

bool H323PresenceStatus::HandleNotification(bool opt)
{
	if (!opt)
		return ReadNotification(request.m_notification,request.m_alias);

    return false;
}

bool H323PresenceNotify::HandleNotification(bool opt)
{
	if (!opt) 
		return ReadNotification(request.m_notification, request.m_alias);

    return false;
}

bool H323PresenceAlert::HandleNotification(bool opt)
{
	if (!opt) {
	   H225_AliasAddress addr;
	   return ReadNotification(request.m_notification,addr);
	}
    return false;
}

bool H323PresenceAuthorize::HandleSubscription(bool opt) 
{ 
	if (!opt) 
	   return ReadSubscription(request.m_subscription,request.m_alias);

	return false; 
}

bool H323PresenceRequest::HandleSubscription(bool opt) 
{ 
	if (!opt)
		return ReadSubscription(request.m_subscription);

	return false; 
}

bool H323PresenceResponse::HandleSubscription(bool opt) 
{ 
	if (!opt)
		return ReadSubscription(request.m_subscription);

	return false; 
}

bool H323PresenceStatus::HandleInstruction(bool opt) 
{ 
	if (!opt || request.HasOptionalField(H460P_PresenceStatus::e_instruction))
	    return ReadInstruction(request.m_instruction,request.m_alias);

    return false; 
}

bool H323PresenceInstruct::HandleInstruction(bool opt) 
{ 
	if (!opt)
		return ReadInstruction(request.m_instruction,request.m_alias);

	return false; 
}

bool H323PresenceAlive::HandleIdentifier(bool opt) 
{ 
   if (!opt)
	   return ReadIdentifier(request.m_identifier);

   return false; 
}

bool H323PresenceRemove::HandleIdentifier(bool opt) 
{ 
   if (!opt)
	   return ReadIdentifier(request.m_identifier);

   return false; 
}

///////////////////////////////////////////////////////////////////////

H323PresenceInstruction::H323PresenceInstruction(Instruction instruct, const PString & alias)
{
	SetTag((unsigned)instruct);
	H225_AliasAddress & addr = *this;
	H323SetAliasAddress(alias, addr);

}

H323PresenceInstruction::Instruction H323PresenceInstruction::GetInstruction()
{
	return (Instruction)GetTag();
}

PString H323PresenceInstruction::GetAlias() const
{
	const H225_AliasAddress & addr = *this;
	return H323GetAliasAddressString(addr);
}

///////////////////////////////////////////////////////////////////////

void H323PresenceInstructions::Add(const H323PresenceInstruction & instruct)
{
    int size = m_instruction.GetSize();
	m_instruction.SetSize(size+1);
	m_instruction[size] = instruct;
}
	
H323PresenceInstruction & H323PresenceInstructions::operator[](PINDEX i) const
{
   return (H323PresenceInstruction &)m_instruction[i];
}

void H323PresenceInstructions::SetAlias(const PString & alias)
{
	if (!alias)
		H323SetAliasAddress(alias,m_alias);
}
	
void H323PresenceInstructions::GetAlias(PString & alias)
{
	alias = H323GetAliasAddressString(m_alias); 
}

void H323PresenceInstructions::SetSize(PINDEX newSize)
{
	m_instruction.SetSize(newSize);
}
	
PINDEX H323PresenceInstructions::GetSize() const
{
	return m_instruction.GetSize();
}

static const char *InstructState[] = {
	"Subscribe",
	"Unsubscribe",
	"Block",
	"Unblock"
};

PString H323PresenceInstruction::GetInstructionString(unsigned instruct)
{
     return InstructState[instruct];
}

///////////////////////////////////////////////////////////////////////

void H323PresenceIdentifiers::Add(const OpalGloballyUniqueID & guid)
{
	H460P_PresenceIdentifier id;
	id.m_guid = guid;
	int size = GetSize();
	SetSize(size+1);
	array.SetAt(size, &id);
}

OpalGloballyUniqueID H323PresenceIdentifiers::GetIdentifier(PINDEX i)
{
	H460P_PresenceIdentifier & id = (H460P_PresenceIdentifier &)array[i];
	return OpalGloballyUniqueID(id.m_guid);
}

////////////////////////////////////////////////////////////////////////

void H323PresenceNotifications::Add(const H323PresenceNotification & notify)
{
	int addsize = m_notification.GetSize();
	m_notification.SetSize(addsize+1);
	m_notification[addsize] = notify;
}

H323PresenceNotification & H323PresenceNotifications::operator[](PINDEX i) const
{
	   return (H323PresenceNotification &)m_notification[i];
}
	
void H323PresenceNotifications::SetAlias(const PString & alias)
{
	if (!alias)
		H323SetAliasAddress(alias,m_alias);
}
	
void H323PresenceNotifications::GetAlias(PString & alias)
{
	alias = H323GetAliasAddressString(m_alias); 
}

void H323PresenceNotifications::SetSize(PINDEX newSize)
{
	m_notification.SetSize(newSize);
}
	
PINDEX H323PresenceNotifications::GetSize() const
{
	return m_notification.GetSize();
}

void H323PresenceNotification::SetPresenceState(H323PresenceNotification::States state, 
						const PString & display)
{
	H460P_Presentity & e = m_presentity;
	e.m_state.SetTag((unsigned)state);

	if (display.GetLength() > 0) {
		e.IncludeOptionalField(H460P_Presentity::e_display);
		e.m_display = display;
	}
}

void H323PresenceNotification::GetPresenceState(H323PresenceNotification::States & state, 
						PString & display) const
{
	const H460P_Presentity & e = m_presentity;
	state = (H323PresenceNotification::States)e.m_state.GetTag();

	if (state != e_generic) {
	   if (e.HasOptionalField(H460P_Presentity::e_display))
		   display = e.m_display;
	} else {
	   const PASN_BMPString & m = e.m_state;
       display = m;
	}
}

void H323PresenceNotification::SetGenericState(const PString & state)
{
	H460P_Presentity & e = m_presentity;
	e.m_state.SetTag(H460P_PresenceState::e_generic);

	PASN_BMPString & display = e.m_state;
	display = state;
}

void H323PresenceNotification::AddSubscriber(const OpalGloballyUniqueID & guid)
{
	if (!HasOptionalField(H460P_PresenceNotification::e_subscribers))
		IncludeOptionalField(H460P_PresenceNotification::e_subscribers);

	H460P_PresenceIdentifier id;
	id.m_guid = guid;
	int size = m_subscribers.GetSize();
	m_subscribers.SetSize(size+1);
	m_subscribers[size] = id;
}

void H323PresenceNotification::RemoveSubscribers()
{
	if (HasOptionalField(H460P_PresenceNotification::e_subscribers)) {
		RemoveOptionalField(H460P_PresenceNotification::e_subscribers);
		m_subscribers.RemoveAll();
	}
}

OpalGloballyUniqueID H323PresenceNotification::GetSubscriber(PINDEX i)
{
	if (HasOptionalField(H460P_PresenceNotification::e_subscribers)) {
		H460P_PresenceIdentifier & id = m_subscribers[i];
		return OpalGloballyUniqueID(id.m_guid);
	}

	return OpalGloballyUniqueID();
}

void H323PresenceNotification::AddAlias(const PString & alias)
{
	if (!HasOptionalField(H460P_PresenceNotification::e_aliasAddress))
		IncludeOptionalField(H460P_PresenceNotification::e_aliasAddress);

	H323SetAliasAddress(alias, m_aliasAddress);
}

PString H323PresenceNotification::GetAlias() const
{
	if (HasOptionalField(H460P_PresenceNotification::e_aliasAddress))
	     return H323GetAliasAddressString(m_aliasAddress);

 	return PString();
}

static const char *PresState[] = {
	"Hidden",
	"Available",
	"Online",
	"Offline",
	"OnCall",
	"VoiceMail",
	"NotAvailable",
	"Generic"
};

PString H323PresenceNotification::GetStateString(unsigned state)
{
	return PresState[state];
}

///////////////////////////////////////////////////////////////////////

void H323PresenceSubscriptions::Add(const H323PresenceSubscription & sub)
{
	int size = m_subscription.GetSize();
	m_subscription.SetSize(size+1);
	m_subscription[size] = sub;
}

H323PresenceSubscription & H323PresenceSubscriptions::operator[](PINDEX i) const
{
   return (H323PresenceSubscription &)m_subscription[i];
}
	
void H323PresenceSubscriptions::SetAlias(const PString & alias)
{
	if (!alias)
		H323SetAliasAddress(alias,m_alias);
}

void H323PresenceSubscriptions::GetAlias(PString & alias)
{
	alias = H323GetAliasAddressString(m_alias); 
}

void H323PresenceSubscriptions::SetSize(PINDEX newSize)
{
	m_subscription.SetSize(newSize);
}
	
PINDEX H323PresenceSubscriptions::GetSize() const
{
	return m_subscription.GetSize();
}

///////////////////////////////////////////////////////////////////////

H323PresenceSubscription::H323PresenceSubscription()
{
}

void H323PresenceSubscription::SetSubscriptionDetails(const PString & subscribe, const PStringList & aliases)
{
	H323SetAliasAddress(subscribe, m_subscribe);

	for (PINDEX i=0; i< aliases.GetSize(); i++) {
		H225_AliasAddress alias;
		H323SetAliasAddress(aliases[i],alias);
		int size = m_aliases.GetSize();
		m_aliases.SetSize(size+1);
		m_aliases[i] = alias;
	}
}

void H323PresenceSubscription::GetSubscriberDetails(PStringList & aliases) const
{
	for (PINDEX i=0; i< m_aliases.GetSize(); i++) {
		PString a = H323GetAliasAddressString(m_aliases[i]);
		aliases.AppendString(a);
	}
}

PString H323PresenceSubscription::GetSubscribed()
{
	return H323GetAliasAddressString(m_subscribe);
}

void H323PresenceSubscription::SetGatekeeperRAS(const H323TransportAddress & address)
{
	IncludeOptionalField(H460P_PresenceSubscription::e_rasAddress);
	address.SetPDU(m_rasAddress);
}

H323TransportAddress H323PresenceSubscription::GetGatekeeperRAS()
{
	if (!HasOptionalField(H460P_PresenceSubscription::e_rasAddress))
		return H323TransportAddress();

	return H323TransportAddress(m_rasAddress);
}

void H323PresenceSubscription::SetApproved(bool success)
{
	if (HasOptionalField(H460P_PresenceSubscription::e_approved))
	        IncludeOptionalField(H460P_PresenceSubscription::e_approved);

	m_approved = success;
}

int H323PresenceSubscription::IsApproved()
{
	if (HasOptionalField(H460P_PresenceSubscription::e_approved))
	  return (int)m_approved;
	else
	  return -1;
}

void H323PresenceSubscription::SetTimeToLive(int t)
{
	IncludeOptionalField(H460P_PresenceSubscription::e_timeToLive);
	m_timeToLive = t;
}
	
void H323PresenceSubscription::SetSubscription(const OpalGloballyUniqueID & guid)
{
	IncludeOptionalField(H460P_PresenceSubscription::e_identifier);
	m_identifier.m_guid = guid;
}

OpalGloballyUniqueID H323PresenceSubscription::GetSubscription()
{
	if (!HasOptionalField(H460P_PresenceSubscription::e_identifier))
		return OpalGloballyUniqueID();

	return OpalGloballyUniqueID(m_identifier.m_guid);
}

void H323PresenceSubscription::MakeDecision(bool approve)
{
	SetApproved(approve);
}

bool H323PresenceSubscription::IsDecisionMade() 
{ 
	return HasOptionalField(H460P_PresenceSubscription::e_approved); 
}

#endif



