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
 * $Id$
 *
 */

#include <ptlib.h>
#include <h323pdu.h>
#include "h460/h460p.h"

#ifdef H323_H460P

#define H460P_MAXPDUSIZE   10

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
} RASMessage_attributes[] = {                    //   st  ins aut not req res alv rem alt
            { H225_RasMessage::e_gatekeeperRequest,0,0,0,0,0,0,0,0,0},
            { H225_RasMessage::e_gatekeeperConfirm,0,0,0,0,0,0,0,0,0},
            { H225_RasMessage::e_gatekeeperReject,0,0,0,0,0,0,0,0,0},
            { H225_RasMessage::e_registrationRequest,1,0,0,0,0,0,0,0,0},
            { H225_RasMessage::e_registrationConfirm,0,1,0,0,0,0,0,0,0},
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
            { H225_RasMessage::e_locationRequest,0,0,0,0,1,1,1,1,1},
            { H225_RasMessage::e_locationConfirm,0,0,0,0,0,2,0,2,0},
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
            { H225_RasMessage::e_serviceControlIndication,0,1,1,1,0,0,0,0,0},
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
//messageId  Notification  Subscription  Instruction  Identifier  Crypto/Tokens
            { H460P_PresenceMessage::e_presenceStatus,1,0,2,0,0},
            { H460P_PresenceMessage::e_presenceInstruct,0,0,1,0,0},
            { H460P_PresenceMessage::e_presenceAuthorize,0,1,0,0,0},
            { H460P_PresenceMessage::e_presenceNotify,1,0,0,0,0},
            { H460P_PresenceMessage::e_presenceRequest,0,1,0,0,3},
            { H460P_PresenceMessage::e_presenceResponse,0,1,0,0,2},
            { H460P_PresenceMessage::e_presenceAlive,0,0,0,1,0},
            { H460P_PresenceMessage::e_presenceRemove,0,0,0,1,0},
            { H460P_PresenceMessage::e_presenceAlert,1,0,0,0,0}
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
    "NotRecognized"        // for new messages
};
const unsigned MaxPresTag =  H460P_PresenceMessage::e_presenceAlert;

// OID's
static const char * H460P_Meeting = "1.3.6.1.4.1.17090.0.12.0.1";

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

    virtual bool Process();

 protected:

    bool ReadNotification(const H460P_ArrayOf_PresenceNotification & notify, const H225_AliasAddress & addr);
    bool ReadSubscription(const H460P_ArrayOf_PresenceSubscription & subscription, const H225_AliasAddress & addr);
    bool ReadInstruction(const H460P_ArrayOf_PresenceInstruction & instruction, const H225_AliasAddress & addr);

    bool ReadNotification(const H460P_ArrayOf_PresenceNotification & notify, H225_TransportAddress * ip = NULL);
    bool ReadSubscription(const H460P_ArrayOf_PresenceSubscription & subscription, H225_TransportAddress * ip = NULL);
    bool ReadIdentifier(const H460P_ArrayOf_PresenceIdentifier & identifier, H225_TransportAddress * ip = NULL);

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

  bool success = false;

  if (PresenceMessage_attributes[tag].notification > 0)
      success |= HandleNotification((PresenceMessage_attributes[tag].notification >1));

  if (PresenceMessage_attributes[tag].subscription > 0)
      success |= HandleSubscription((PresenceMessage_attributes[tag].subscription >1));

  if (PresenceMessage_attributes[tag].instruction > 0)
      success |= HandleInstruction((PresenceMessage_attributes[tag].instruction >1));

  if (PresenceMessage_attributes[tag].identifier > 0)
      success |= HandleIdentifier((PresenceMessage_attributes[tag].identifier >1));

  if (PresenceMessage_attributes[tag].cryptoTokens > 0)
      success |= HandleCryptoTokens((PresenceMessage_attributes[tag].cryptoTokens >1));

  return success;
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

bool H323PresenceBase::ReadInstruction(const H460P_ArrayOf_PresenceInstruction & instruction, const H225_AliasAddress & addr)
{
    handler->OnInstructions((H323PresenceHandler::MsgType)tag, instruction,addr);
    return true;
}

bool H323PresenceBase::ReadNotification(const H460P_ArrayOf_PresenceNotification & notify, H225_TransportAddress * ip)
{
    if (ip == NULL)
        return false;

    for (PINDEX i = 0; i < notify.GetSize(); i++)
        handler->OnNotification((H323PresenceHandler::MsgType)tag, notify[i],*ip);

    return true;
}

bool H323PresenceBase::ReadSubscription(const H460P_ArrayOf_PresenceSubscription & subscription, H225_TransportAddress * ip)
{
    if (ip == NULL)
        return false;

    for (PINDEX i = 0; i < subscription.GetSize(); i++)
        handler->OnSubscription((H323PresenceHandler::MsgType)tag, subscription[i],*ip);

    return true;
}

bool H323PresenceBase::ReadIdentifier(const H460P_ArrayOf_PresenceIdentifier & identifier, H225_TransportAddress * ip)
{
    if (ip == NULL)
        return false;

    for (PINDEX i = 0; i < identifier.GetSize(); i++)
        handler->OnIdentifiers((H323PresenceHandler::MsgType)tag, identifier[i],*ip);

    return true;
}

////////////////////////////////////////////////////////////////////////


template<class Msg>
class H323PresencePDU : public H323PresenceBase {
  public:

    H323PresencePDU(const H323PresenceMessage & m)
        : H323PresenceBase(m), request(m.m_recvPDU) {}

    H323PresencePDU(unsigned tag)
        : request(*new Msg) {}

    operator Msg & () { return request; }
    operator const Msg & () const { return request; }

    virtual bool Process() { return H323PresenceBase::Process(); }

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
    H323PresenceRequest(const H323PresenceMessage & m, H225_TransportAddress * ip)
        : H323PresencePDU<H460P_PresenceRequest>(m), remoteIP(ip) {}

  protected:
    virtual bool HandleSubscription(bool opt);

  private:
    H225_TransportAddress * remoteIP;
};

class H323PresenceResponse  : public H323PresencePDU<H460P_PresenceResponse>
{
  public:
    H323PresenceResponse(const H323PresenceMessage & m, H225_TransportAddress * ip)
        : H323PresencePDU<H460P_PresenceResponse>(m), remoteIP(ip) {}

  protected:
    virtual bool HandleSubscription(bool opt);

  private:
    H225_TransportAddress * remoteIP;
};

class H323PresenceAlive  : public H323PresencePDU<H460P_PresenceAlive>
{
  public:
    H323PresenceAlive(const H323PresenceMessage & m, H225_TransportAddress * ip)
        : H323PresencePDU<H460P_PresenceAlive>(m), remoteIP(ip) {}

  protected:
    virtual bool HandleIdentifier(bool opt);

  private:
    H225_TransportAddress * remoteIP;
};

class H323PresenceRemove  : public H323PresencePDU<H460P_PresenceRemove>
{
  public:
    H323PresenceRemove(const H323PresenceMessage & m, H225_TransportAddress * ip)
        : H323PresencePDU<H460P_PresenceRemove>(m), remoteIP(ip) {}

  protected:
    virtual bool HandleIdentifier(bool opt);

  private:
    H225_TransportAddress * remoteIP;
};

class H323PresenceAlert  : public H323PresencePDU<H460P_PresenceAlert>
{
  public:
    H323PresenceAlert(const H323PresenceMessage & m, H225_TransportAddress * ip)
        : H323PresencePDU<H460P_PresenceAlert>(m), remoteIP(ip) {}

  protected:
    virtual bool HandleNotification(bool opt);

  private:
    H225_TransportAddress * remoteIP;
};


////////////////////////////////////////////////////////////////////////////



bool H323PresenceHandler::ReceivedPDU(const PASN_OctetString & pdu, H225_TransportAddress * ip)
{
    H460P_PresenceElement element;
    PPER_Stream raw(pdu);
    if (!element.Decode(raw) || element.m_message.GetSize() == 0) {
        PTRACE(2,"PRES\tError Decoding Element. Malformed or no messages.");
        return false;
    }

    PTRACE(5,"PRES\tReceived PDU\n" << element);

    for (PINDEX i=0; i < element.m_message.GetSize(); i++) {
        H323PresenceMessage m;
        m.m_recvPDU = element.m_message[i];
        m.m_handler = this;

        H323PresenceBase handler;
        switch (m.GetTag())
        {
            case H460P_PresenceMessage::e_presenceStatus:
                return H323PresenceStatus(m).Process();

            case H460P_PresenceMessage::e_presenceInstruct:
                return H323PresenceInstruct(m).Process();

            case H460P_PresenceMessage::e_presenceAuthorize:
                return H323PresenceAuthorize(m).Process();

            case H460P_PresenceMessage::e_presenceNotify:
                return H323PresenceNotify(m).Process();

            case H460P_PresenceMessage::e_presenceRequest:
                return H323PresenceRequest(m,ip).Process();

            case H460P_PresenceMessage::e_presenceResponse:
                return H323PresenceResponse(m,ip).Process();

            case H460P_PresenceMessage::e_presenceAlive:
                return H323PresenceAlive(m,ip).Process();

            case H460P_PresenceMessage::e_presenceRemove:
                return H323PresenceRemove(m,ip).Process();

            case H460P_PresenceMessage::e_presenceAlert:
                return H323PresenceAlert(m,ip).Process();

            default:
                break;
        }
    }
    return false;
}


PBoolean H323PresenceHandler::BuildPresenceMessage(unsigned id, H323PresenceStore & store, H460P_ArrayOf_PresenceMessage & msgs)
{

    bool dataToSend = false;
    H323PresenceStore::iterator iter = store.begin();
    while (iter != store.end()) {
        H323PresenceEndpoint & ep = iter->second;
        bool ok = false;
          if ((PresenceMessage_attributes[id].notification == 1) && (ep.m_Notify.GetSize() > 0)) ok = true;
          if ((PresenceMessage_attributes[id].subscription == 1) && (ep.m_Authorize.GetSize() > 0)) ok = true;
          if ((PresenceMessage_attributes[id].instruction == 1) && (ep.m_Instruction.GetSize() > 0)) ok = true;
          if ((PresenceMessage_attributes[id].identifier == 1) && (ep.m_Identifiers.GetSize() > 0)) ok = true;

          if (ok) {
            dataToSend = true;
            int sz = msgs.GetSize();
            switch (id) {
                case H460P_PresenceMessage::e_presenceStatus:
                    BuildStatus(msgs, ep.m_Notify, ep.m_Instruction, iter->first);
                    break;
                case H460P_PresenceMessage::e_presenceInstruct:
                    BuildInstruct(msgs,ep.m_Instruction, iter->first);
                    break;
                case H460P_PresenceMessage::e_presenceAuthorize:
                    msgs.SetSize(sz+1);
                    BuildAuthorize(msgs[sz],ep.m_Authorize);
                    break;
                case H460P_PresenceMessage::e_presenceNotify:
                    msgs.SetSize(sz+1);
                    BuildNotify(msgs[sz], ep.m_Notify);
                    break;
                case H460P_PresenceMessage::e_presenceRequest:
                    msgs.SetSize(sz+1);
                    BuildRequest(msgs[sz],ep.m_Authorize);
                    break;
                case H460P_PresenceMessage::e_presenceResponse:
                    msgs.SetSize(sz+1);
                    BuildResponse(msgs[sz],ep.m_Authorize);
                    break;
                case H460P_PresenceMessage::e_presenceAlive:
                    msgs.SetSize(sz+1);
                    BuildAlive(msgs[sz], ep.m_Identifiers);
                    break;
                case H460P_PresenceMessage::e_presenceRemove:
                    msgs.SetSize(sz+1);
                    BuildRemove(msgs[sz], ep.m_Identifiers);
                    break;
                case H460P_PresenceMessage::e_presenceAlert:
                    msgs.SetSize(sz+1);
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
        ++iter;
    }

    return dataToSend;
}

PBoolean H323PresenceHandler::BuildPresenceElement(unsigned msgtag, PASN_OctetString & pdu)
{
    list<PASN_OctetString> raw;
    if (BuildPresenceElement(msgtag, raw) && raw.size() > 0) {
        pdu = raw.front();
        raw.clear();
        return true;
    }
    return false;
}

PBoolean H323PresenceHandler::BuildPresenceElement(unsigned msgtag, list<PASN_OctetString> & pdu)
{
    bool success = false;
    H460P_ArrayOf_PresenceMessage msgs;
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
        H460P_PresenceElement subElement;
        H460P_ArrayOf_PresenceMessage & subMsgs = subElement.m_message;
        PASN_OctetString subPDU;
        int sz = 0;
        subMsgs.SetSize(sz);
        for (PINDEX i=0; i < msgs.GetSize(); ++i) {
            sz = subMsgs.GetSize();
            subMsgs.SetSize(sz+1);
            subMsgs[sz] = msgs[i];
            if (subMsgs.GetSize() >= H460P_MAXPDUSIZE) {
                subPDU.EncodeSubType(subElement);
                pdu.push_back(subPDU);
                PTRACE(6,"PRES\tPDU sz=" << pdu.size() << "\n" << subElement);
                subMsgs.SetSize(0);
            }
        }
        if (subMsgs.GetSize() > 0) {
            subPDU.EncodeSubType(subElement);
            pdu.push_back(subPDU);
            PTRACE(6,"PRES\tPDU sz=" << pdu.size() << "\n" << subElement);
        }
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

PBoolean H323PresenceHandler::BuildPresenceMessage(unsigned id, const H225_EndpointIdentifier & ep, H460P_ArrayOf_PresenceMessage & msgs)
{

    bool dataToSend = false;
    int sz = 0;

    H323PresenceStore Pep;
    H323PresenceStore::iterator i;

    switch (id) {
        case H460P_PresenceMessage::e_presenceStatus:
            if (BuildNotification(ep,Pep) || BuildInstructions(ep,Pep))  {
                for(i= Pep.begin(); i != Pep.end(); ++i)
                    BuildStatus(msgs, i->second.m_Notify, i->second.m_Instruction, i->first);
                dataToSend = true;
            }
            break;
        case H460P_PresenceMessage::e_presenceInstruct:
            if (BuildInstructions(ep,Pep)) {
                for(i= Pep.begin(); i != Pep.end(); ++i)
                    BuildInstruct(msgs, i->second.m_Instruction, i->first);
                dataToSend = true;
            }
            break;
        case H460P_PresenceMessage::e_presenceAuthorize:
            if (BuildSubscription(ep,Pep)) {
                for(i= Pep.begin(); i != Pep.end(); ++i) {
                    sz = msgs.GetSize();
                    msgs.SetSize(sz+1);
                    H460P_PresenceAuthorize & xm = BuildAuthorize(msgs[sz], i->second.m_Authorize);
                    xm.m_alias = i->first;
                }
                dataToSend = true;
            }
            break;
        case H460P_PresenceMessage::e_presenceNotify:
            if (BuildNotification(ep,Pep)) {
                for(i= Pep.begin(); i != Pep.end(); ++i) {
                    sz = msgs.GetSize();
                    msgs.SetSize(sz+1);
                    H460P_PresenceNotify & xm = BuildNotify(msgs[sz], i->second.m_Notify);
                    xm.m_alias = i->first;
                }
                dataToSend = true;
            }
            break;
        default:
            break;
    }
    return dataToSend;
}

PBoolean H323PresenceHandler::BuildPresenceMessage(unsigned id, const H225_TransportAddress & ip, H460P_ArrayOf_PresenceMessage & msgs)
{
    bool dataToSend = false;
    int sz = 0;

    H323PresenceGkStore Pep;
    H323PresenceGkStore::iterator i;

    switch (id) {
        case H460P_PresenceMessage::e_presenceRequest:
            if (BuildSubscription(true,ip,Pep)) {
                for(i= Pep.begin(); i != Pep.end(); ++i) {
                    sz = msgs.GetSize();
                    msgs.SetSize(sz+1);
                    BuildRequest(msgs[sz], i->second.m_Authorize);
                }
                dataToSend = true;
            }
            break;
        case H460P_PresenceMessage::e_presenceResponse:
            if (BuildSubscription(false,ip,Pep)) {
                for(i= Pep.begin(); i != Pep.end(); ++i) {
                    sz = msgs.GetSize();
                    msgs.SetSize(sz+1);
                    BuildResponse(msgs[sz], i->second.m_Authorize);
                }
                dataToSend = true;
            }
            break;
        case H460P_PresenceMessage::e_presenceAlive:
            if (BuildIdentifiers(true,ip,Pep)) {
                for(i= Pep.begin(); i != Pep.end(); ++i) {
                    sz = msgs.GetSize();
                    msgs.SetSize(sz+1);
                    BuildAlive(msgs[sz], i->second.m_Identifiers);
                }
                dataToSend = true;
            }
            break;
        case H460P_PresenceMessage::e_presenceRemove:
            if (BuildIdentifiers(false,ip,Pep)) {
                for(i= Pep.begin(); i != Pep.end(); ++i) {
                    sz = msgs.GetSize();
                    msgs.SetSize(sz+1);
                    BuildRemove(msgs[sz], i->second.m_Identifiers);
                }
                dataToSend = true;
            }
            break;
        case H460P_PresenceMessage::e_presenceAlert:
            if (BuildNotification(ip,Pep)) {
                for(i= Pep.begin(); i != Pep.end(); ++i) {
                    sz = msgs.GetSize();
                    msgs.SetSize(sz+1);
                    BuildAlert(msgs[sz], i->second.m_Notify);
                }
                dataToSend = true;
            }
            break;
        default:
            break;
    }
    return dataToSend;
}

PBoolean H323PresenceHandler::BuildPresenceElement(unsigned msgtag,const H225_EndpointIdentifier & ep, PASN_OctetString & pdu)
{
    list<PASN_OctetString> raw;
    if (BuildPresenceElement(msgtag, ep, raw) && raw.size() > 0) {
        pdu = raw.front();
        raw.clear();
        return true;
    }
    return false;
}

PBoolean H323PresenceHandler::BuildPresenceElement(unsigned msgtag, const H225_EndpointIdentifier & ep, list<PASN_OctetString> & pdu)
{
    bool success = false;
    H460P_ArrayOf_PresenceMessage msgs;

    if (RASMessage_attributes[msgtag].preStatus >0)
        BuildPresenceMessage(H460P_PresenceMessage::e_presenceStatus,ep,msgs);
    if (RASMessage_attributes[msgtag].preInstruct >0)
        BuildPresenceMessage(H460P_PresenceMessage::e_presenceInstruct,ep,msgs);
    if (RASMessage_attributes[msgtag].preAuthorize >0)
        BuildPresenceMessage(H460P_PresenceMessage::e_presenceAuthorize,ep,msgs);
    if (RASMessage_attributes[msgtag].preNotify >0)
        BuildPresenceMessage(H460P_PresenceMessage::e_presenceNotify,ep,msgs);

    success = (msgs.GetSize() > 0);
    if (success) {
        H460P_PresenceElement subElement;
        H460P_ArrayOf_PresenceMessage & subMsgs = subElement.m_message;
        for (PINDEX i=0; i < msgs.GetSize(); ++i) {
            subMsgs.SetSize(1);
            subMsgs[0] = msgs[i];
            PASN_OctetString subPDU;
            subPDU.EncodeSubType(subElement);
            pdu.push_back(subPDU);
            PTRACE(6,"PRES\tPDU Message " << i << "\n" << subElement);
        }
    }
    return success;
}

PBoolean H323PresenceHandler::BuildPresenceElement(unsigned msgtag,const H225_TransportAddress & ip,PASN_OctetString & pdu)
{
    list<PASN_OctetString> raw;
    if (BuildPresenceElement(msgtag, ip, raw) && raw.size() > 0) {
        pdu = raw.front();
        raw.clear();
        return true;
    }
    return false;
}

PBoolean H323PresenceHandler::BuildPresenceElement(unsigned msgtag, const H225_TransportAddress & ip, list<PASN_OctetString> & pdu)
{
    bool success = false;
    H460P_ArrayOf_PresenceMessage msgs;

    if (RASMessage_attributes[msgtag].preRequest >0)
        BuildPresenceMessage(H460P_PresenceMessage::e_presenceRequest,ip,msgs);
    if (RASMessage_attributes[msgtag].preResponse >0)
        BuildPresenceMessage(H460P_PresenceMessage::e_presenceResponse,ip,msgs);
    if (RASMessage_attributes[msgtag].preAlive >0)
        BuildPresenceMessage(H460P_PresenceMessage::e_presenceAlive,ip,msgs);
    if (RASMessage_attributes[msgtag].preRemove >0)
        BuildPresenceMessage(H460P_PresenceMessage::e_presenceRemove,ip,msgs);
    if (RASMessage_attributes[msgtag].preAlert >0)
        BuildPresenceMessage(H460P_PresenceMessage::e_presenceAlert,ip,msgs);

    success = (msgs.GetSize() > 0);
    if (success) {
        H460P_PresenceElement subElement;
        H460P_ArrayOf_PresenceMessage & subMsgs = subElement.m_message;
        PASN_OctetString subPDU;
        int sz = 0;
        subMsgs.SetSize(sz);
        for (PINDEX i=0; i < msgs.GetSize(); ++i) {
            sz = subMsgs.GetSize();
            subMsgs.SetSize(sz+1);
            subMsgs[sz] = msgs[i];
            if (subMsgs.GetSize() >= H460P_MAXPDUSIZE) {
                PASN_OctetString subPDU;
                subPDU.EncodeSubType(subElement);
                pdu.push_back(subPDU);
                PTRACE(6,"PRES\tPDU sz=" << pdu.size() << " to " << H323TransportAddress(ip) << "\n" << subElement);
                subMsgs.SetSize(0);
            }
        }
        if (subMsgs.GetSize() > 0) {
            PASN_OctetString subPDU;
            subPDU.EncodeSubType(subElement);
            pdu.push_back(subPDU);
            PTRACE(6,"PRES\tPDU sz=" << pdu.size() << " to " << H323TransportAddress(ip) << "\n" << subElement);
        }
    }

    return success;
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


H460P_PresenceStatus &  H323PresenceHandler::BuildStatus(H460P_ArrayOf_PresenceMessage & msg,
                        const H323PresenceNotifications & notify,
                        const H323PresenceInstructions & inst,
                        const H225_AliasAddress & alias)
{
    H323PresenceMsg<H460P_PresenceStatus> m;
    H460P_PresenceStatus & pdu = m.Build(H460P_PresenceMessage::e_presenceStatus);
    pdu.m_notification = notify.m_notification;
    PStringList aliases;
    notify.GetAliasList(aliases);
    if (aliases.GetSize() > 0) {
        for (PINDEX i=0; i<aliases.GetSize(); ++i) {
            int sz = pdu.m_alias.GetSize();
            pdu.m_alias.SetSize(sz+1);
            H323SetAliasAddress(aliases[i],pdu.m_alias[sz]);
        }
    } else {
        pdu.m_alias.SetSize(1);
        pdu.m_alias[0] = alias;
    }
    int sz = msg.GetSize();
    msg.SetSize(sz+1);
    msg[sz] = *(H460P_PresenceMessage *)m.Clone();

    if (inst.GetSize() > 0)
        BuildInstruct(msg, inst, alias);

    return msg[sz];
}

H460P_PresenceInstruct &  H323PresenceHandler::BuildInstruct(H460P_ArrayOf_PresenceMessage & msg, const H323PresenceInstructions & inst, const H225_AliasAddress & alias)
{
    H323PresenceMsg<H460P_PresenceInstruct> m;
    H460P_PresenceInstruct & pdu = m.Build(H460P_PresenceMessage::e_presenceInstruct);
    pdu.m_alias = alias;

    for (PINDEX j=0; j < inst.GetSize(); ++j) {
       int sz = pdu.m_instruction.GetSize();
       pdu.m_instruction.SetSize(sz+1);
       pdu.m_instruction[sz] = inst[j];

       if (pdu.m_instruction.GetSize() == H460P_MAXPDUSIZE) {
           int psz = msg.GetSize();
           msg.SetSize(psz+1);
           msg[psz] = *(H460P_PresenceMessage *)m.Clone();
           pdu.m_instruction.SetSize(0);
       }
    }

    int fsz = msg.GetSize();
    if (pdu.m_instruction.GetSize() > 0) {
        msg.SetSize(fsz+1);
        msg[fsz] = *(H460P_PresenceMessage *)m.Clone();
        fsz++;
    }
    return msg[fsz-1];
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

/////////////////////////////////////////////////////////////////////////////

 H323PresenceHandler::localeInfo::localeInfo()
:  m_locale(""), m_region(""),m_country(""), m_countryCode(""),
   m_latitude(""), m_longitude(""), m_elevation("")
{
}

bool H323PresenceHandler::localeInfo::BuildLocalePDU(H460P_PresenceGeoLocation & pdu)
{
    bool contents = false;
    if (!m_locale) {
        pdu.IncludeOptionalField(H460P_PresenceGeoLocation::e_locale);
        pdu.m_locale.SetValue(m_locale);
        contents = true;
    }
    if (!m_region) {
        pdu.IncludeOptionalField(H460P_PresenceGeoLocation::e_region);
        pdu.m_region.SetValue(m_region);
        contents = true;
    }
    if (!m_country) {
        pdu.IncludeOptionalField(H460P_PresenceGeoLocation::e_country);
        pdu.m_country.SetValue(m_country);
        contents = true;
    }
    if (!m_countryCode) {
        pdu.IncludeOptionalField(H460P_PresenceGeoLocation::e_countryCode);
        pdu.m_countryCode.SetValue(m_countryCode);
        contents = true;
    }
    if (!m_latitude) {
        pdu.IncludeOptionalField(H460P_PresenceGeoLocation::e_latitude);
        pdu.m_latitude.SetValue(m_latitude);
        contents = true;
    }
    if (!m_longitude) {
        pdu.IncludeOptionalField(H460P_PresenceGeoLocation::e_longitude);
        pdu.m_longitude.SetValue(m_longitude);
        contents = true;
    }
    if (!m_elevation) {
        pdu.IncludeOptionalField(H460P_PresenceGeoLocation::e_elevation);
        pdu.m_elevation.SetValue(m_elevation);
        contents = true;
    }
    return contents;
}

////////////////////////////////////////////////////////////////////////

bool H323PresenceStatus::HandleNotification(bool opt)
{
    bool success = false;
    if (!opt) {
        for (PINDEX i=0; i<request.m_alias.GetSize(); ++i)
          success |= ReadNotification(request.m_notification,request.m_alias[i]);
    }
    return success;
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
       return ReadNotification(request.m_notification,remoteIP);
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
        return ReadSubscription(request.m_subscription,remoteIP);

    return false;
}

bool H323PresenceResponse::HandleSubscription(bool opt)
{
    if (!opt)
        return ReadSubscription(request.m_subscription,remoteIP);

    return false;
}

bool H323PresenceStatus::HandleInstruction(bool opt)
{
    bool success = false;
    if (!opt || request.HasOptionalField(H460P_PresenceStatus::e_instruction)) {
        for (PINDEX j=0; j< request.m_alias.GetSize(); ++j)
            success |= ReadInstruction(request.m_instruction,request.m_alias[j]);
    }
    return success;
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
       return ReadIdentifier(request.m_identifier,remoteIP);

   return false;
}

bool H323PresenceRemove::HandleIdentifier(bool opt)
{
   if (!opt)
       return ReadIdentifier(request.m_identifier,remoteIP);

   return false;
}

///////////////////////////////////////////////////////////////////////

H323PresenceInstruction::Instruction H323PresenceInstruction::GetInstruction()
{
    return (Instruction)GetTag();
}

H323PresenceInstruction::Category  H323PresenceInstruction::GetCategory(const H460P_PresenceAlias & alias)
{
       Category category = H323PresenceInstruction::e_UnknownCategory;

       if (!alias.HasOptionalField(H460P_PresenceAlias::e_category))
           return category;

       const H460P_PresenceFeature & cat = alias.m_category;
       switch (cat.GetTag())  {
           case H460P_PresenceFeature::e_audio:
               category = H323PresenceInstruction::e_Audio;
               break;
           case H460P_PresenceFeature::e_video:
               category = H323PresenceInstruction::e_Video;
               break;
           case H460P_PresenceFeature::e_data:
               category = H323PresenceInstruction::e_Data;
               break;
           case H460P_PresenceFeature::e_extVideo:
               category = H323PresenceInstruction::e_ExtVideo;
               break;
           case H460P_PresenceFeature::e_generic:
               {
                   const H460P_PresenceFeatureGeneric & gen = cat;
                   const H225_GenericIdentifier & id = gen.m_identifier;
                   if (id.GetTag() == H225_GenericIdentifier::e_oid) {
                       const PASN_ObjectId & val = id;
                       if (val.AsString() == PString(H460P_Meeting))
                           category = H323PresenceInstruction::e_Meeting;
                   }
               }
               break;
           default:
               break;
       }
       return category;
}

bool H323PresenceInstruction::SetCategory(H323PresenceInstruction::Category category ,H460P_PresenceAlias & alias)
{
    if (category == H323PresenceInstruction::e_UnknownCategory)
        return false;

    alias.IncludeOptionalField(H460P_PresenceAlias::e_category);
    H460P_PresenceFeature & cat = alias.m_category;
    switch (category) {
       case e_Audio:
       case e_Video:
       case e_Data:
       case e_ExtVideo:
           cat.SetTag((unsigned)category);
           break;
       case e_Teleprence:
           // Not supported yet
           break;
       case e_Meeting:
           {
               cat.SetTag(H460P_PresenceFeature::e_generic);
               H460P_PresenceFeatureGeneric & gen = cat;
               H225_GenericIdentifier & id = gen.m_identifier;
               id.SetTag(H225_GenericIdentifier::e_oid);
               PASN_ObjectId & val = id;
               val.SetValue(H460P_Meeting);
           }
           break;
       default:
           break;
    }
    return true;
}

H323PresenceInstruction::H323PresenceInstruction(Instruction instruct, const PString & alias, const PString & display, const PString & avatar, H323PresenceInstruction::Category category)
{
    SetTag((unsigned)instruct);
    H460P_PresenceAlias & palias = *this;
    H225_AliasAddress & addr = palias.m_alias;
    H323SetAliasAddress(alias, addr);

#if H323_H460P_VER >= 2
    if (!display) {
        palias.IncludeOptionalField(H460P_PresenceAlias::e_display);
        H460P_PresenceDisplay & pdisp = palias.m_display;
        pdisp.m_display.SetValue(display);
    }
    if (!avatar) {
        palias.IncludeOptionalField(H460P_PresenceAlias::e_avatar);
        PASN_IA5String & av = palias.m_avatar;
        av.SetValue(avatar);
    }

    SetCategory(category,palias);

#endif
}

PString H323PresenceInstruction::GetAlias(PString & display, PString & avatar, H323PresenceInstruction::Category & category) const
{
    const H460P_PresenceAlias & palias = *this;
#if H323_H460P_VER >= 2
    if (palias.HasOptionalField(H460P_PresenceAlias::e_display)) {
       const H460P_PresenceDisplay & pdisp = palias.m_display;
       display = pdisp.m_display.GetValue();
    }
    if (palias.HasOptionalField(H460P_PresenceAlias::e_avatar)) {
       const PASN_IA5String & pav = palias.m_avatar;
       avatar = pav.GetValue();
    }
    category = GetCategory(palias);
#endif
    const H225_AliasAddress & addr = palias.m_alias;
    return H323GetAliasAddressString(addr);
}

PString H323PresenceInstruction::GetAlias() const
{
    PString display = PString();
    PString avatar = PString();
    Category cat = e_UnknownCategory;
    return GetAlias(display,avatar,cat);
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
    "Unblock",
    "Pending",
    "Unknown"
};

PString H323PresenceInstruction::GetInstructionString(unsigned instruct)
{
	if (instruct < 5) return InstructState[instruct];

    return InstructState[5];
}

static const char *CategoryState[] = {
        "Audio",
        "Video",
        "Data",
        "ExtVideo",
        "Teleprence",
        "Meeting",
        "Unknown"
};


PString H323PresenceInstruction::GetCategoryString(unsigned cat)
{
	if (cat < 6) return CategoryState[cat];

    return CategoryState[6];
}

///////////////////////////////////////////////////////////////////////

void H323PresenceIdentifiers::Add(const OpalGloballyUniqueID & guid, PBoolean todelete)
{
    H460P_PresenceIdentifier id;
    id.m_guid.SetValue(guid);
    if (todelete) {
        id.IncludeOptionalField(H460P_PresenceIdentifier::e_remove);
        id.m_remove = true;
    }
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

void H323PresenceNotifications::SetAliasList(const PStringList & alias)
{
    for (PINDEX i=0; i<alias.GetSize(); ++i) {
        m_aliasList.AppendString(alias[i]);
    }
}

void H323PresenceNotifications::GetAliasList(PStringList & alias) const
{
    for (PINDEX i=0; i< m_aliasList.GetSize(); ++i) {
        alias.AppendString(m_aliasList[i]);
    }
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
        H460P_PresenceDisplay disp;
        disp.IncludeOptionalField(H460P_PresenceDisplay::e_language);
        disp.m_language = "en-US";
        disp.m_display = display;
        e.IncludeOptionalField(H460P_Presentity::e_display);
        e.m_display.SetSize(1);
        e.m_display[0] = disp;
    }
}

void H323PresenceNotification::AddSupportedFeature(int id)
{
    H460P_Presentity & e = m_presentity;
    e.IncludeOptionalField(H460P_Presentity::e_supportedFeatures);
    H460P_ArrayOf_PresenceFeature & f = e.m_supportedFeatures;

    H460P_PresenceFeature pf;
    pf.SetTag(id);

    int sz = f.GetSize();
    f.SetSize(sz+1);
    f[sz] = pf;
}

void H323PresenceNotification::AddSupportedFeature(const H460P_PresenceFeature & id)
{
    H460P_Presentity & e = m_presentity;
    if (!e.HasOptionalField(H460P_Presentity::e_supportedFeatures))
        e.IncludeOptionalField(H460P_Presentity::e_supportedFeatures);

    H460P_ArrayOf_PresenceFeature & f = e.m_supportedFeatures;

    int sz = f.GetSize();
    f.SetSize(sz+1);
    f[sz] = id;
}

void H323PresenceNotification::AddEndpointLocale(const H460P_PresenceGeoLocation & loc)
{
    H460P_Presentity & e = m_presentity;
    e.IncludeOptionalField(H460P_Presentity::e_geolocation);
    e.m_geolocation = loc;
}

void H323PresenceNotification::AddGenericData(const H225_ArrayOf_GenericData & data)
{
    int sz = data.GetSize();
    if (sz == 0)
        return;

    H460P_Presentity & e = m_presentity;
    e.IncludeOptionalField(H460P_Presentity::e_genericData);
    e.m_genericData.SetSize(sz);
    for (PINDEX i=0; i < sz; ++i) {
        e.m_genericData[i] = data[i];
    }
}

void H323PresenceNotification::GetPresenceState(H323PresenceNotification::States & state,
                        PString & display) const
{
    const H460P_Presentity & e = m_presentity;
    state = (H323PresenceNotification::States)e.m_state.GetTag();
    display = PString();

    if (state != e_generic) {
       if (e.HasOptionalField(H460P_Presentity::e_display) && e.m_display.GetSize() > 0)
           display = e.m_display[0].m_display;
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

H323PresenceSubscription::H323PresenceSubscription(const OpalGloballyUniqueID & guid)
{
    m_identifier.m_guid = guid;
}

void H323PresenceSubscription::SetSubscriptionDetails(const PString & subscribe, const PStringList & aliases)
{
    H323SetAliasAddress(subscribe, m_subscribe);

    for (PINDEX i=0; i< aliases.GetSize(); i++) {
        H460P_PresenceAlias pAlias;
        H225_AliasAddress & alias = pAlias.m_alias;
        H323SetAliasAddress(aliases[i],alias);
        int size = m_aliases.GetSize();
        m_aliases.SetSize(size+1);
        m_aliases[i] = pAlias;
    }
}

void H323PresenceSubscription::SetSubscriptionDetails(const H225_AliasAddress & subscribe, const H225_AliasAddress & subscriber,
                                                      const PString & display, const PString & avatar,
                                                      H323PresenceInstruction::Category category )
{
    m_subscribe = subscribe;
    H460P_PresenceAlias pAlias;
    pAlias.m_alias = subscriber;
    if (!display) {
        pAlias.IncludeOptionalField(H460P_PresenceAlias::e_display);
        H460P_PresenceDisplay & disp = pAlias.m_display;
        disp.m_display.SetValue(display);
    }
    if (!avatar) {
        pAlias.IncludeOptionalField(H460P_PresenceAlias::e_avatar);
        PASN_IA5String & avatar = pAlias.m_avatar;
        avatar.SetValue(avatar);
    }
    H323PresenceInstruction::SetCategory(category,pAlias);

    int sz = m_aliases.GetSize();
    m_aliases.SetSize(sz+1);
    m_aliases[sz] = pAlias;
}

void H323PresenceSubscription::GetSubscriberDetails(PresenceSubscriberList & aliases) const
{

    for (PINDEX i=0; i< m_aliases.GetSize(); i++) {
        H460P_PresenceAlias & pAlias = m_aliases[i];
        PresSubDetails details;
        details.m_display = PString();
        details.m_avatar = PString();
        details.m_category = 100;
        PString name = PString();
        name = H323GetAliasAddressString(pAlias.m_alias);
        if (pAlias.HasOptionalField(H460P_PresenceAlias::e_display)) {
            H460P_PresenceDisplay & disp = pAlias.m_display;
            details.m_display = disp.m_display.GetValue();
        }
        if (pAlias.HasOptionalField(H460P_PresenceAlias::e_avatar)) {
            PASN_IA5String & avatar = pAlias.m_avatar;
            details.m_avatar = avatar.GetValue();
        }
        details.m_category = H323PresenceInstruction::GetCategory(pAlias);

        aliases.insert(pair<PString, PresSubDetails>(name,details));
    }
}

void H323PresenceSubscription::GetSubscriberDetails(PStringList & aliases) const
{
    PresenceSubscriberList slist;
    GetSubscriberDetails(slist);

    PresenceSubscriberList::const_iterator i;
    for (i = slist.begin(); i != slist.end(); ++i) {
        aliases.AppendString(i->first);
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
    if (!HasOptionalField(H460P_PresenceSubscription::e_approved))
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
    m_identifier.m_guid = guid;
}

OpalGloballyUniqueID H323PresenceSubscription::GetSubscription() const
{
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

void H323PresenceSubscription::AddGenericData(const H225_ArrayOf_GenericData & data)
{
    int sz = data.GetSize();
    if (sz >0)
        return;

    IncludeOptionalField(H460P_PresenceSubscription::e_genericData);
    m_genericData.SetSize(sz);
    for (PINDEX i=0; i<sz; i++) {
        m_genericData[i] = data[i];
    }
}

#endif



