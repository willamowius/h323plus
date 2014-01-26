/*
 * h460_oid3.cxx
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
 * $Id$
 *
 */


#include <ptlib.h>
#include <openh323buildopts.h>

#ifdef H323_H460P

#include "h460/h460_oid3.h"
#include <h323.h>
#include <h323pdu.h>

///////////////////////////////////////////////////////////////////////
int PRETIME = 2;

H460PresenceHandler::H460PresenceHandler(H323EndPoint & _ep)
: presenceRegistration(false), pendingMessages(false), ep(_ep), feat(NULL)
{
    genericData.SetSize(0);
    PTRACE(4,"OID3\tPresence Handler created!");
}

H460PresenceHandler::~H460PresenceHandler()
{
    if (QueueTimer.IsRunning())
        QueueTimer.Stop();

    genericData.SetSize(0);
}

void H460PresenceHandler::dequeue(PTimer &,  H323_INT)
{ 
    if (pendingMessages) {
        if (ep.GetGatekeeper() && ep.GetGatekeeper()->IsRegistered()) 
             ep.GetGatekeeper()->SendServiceControlIndication();
        pendingMessages = false;
    }
}

void H460PresenceHandler::AttachFeature(H460_FeatureOID3 * _feat)
{
    feat = _feat;
    if (ep.GetGatekeeper())
        presenceRegistration = true;

    QueueTimer.SetNotifier(PCREATE_NOTIFIER(dequeue));
    QueueTimer.RunContinuous(PRETIME * 1000); 
}

void PostSubscription(H323PresenceStore & gw, const H323PresenceSubscriptions & list)
{
     H323PresenceStore::iterator inf = gw.find(list.m_alias);
     if (inf == gw.end()) {
        H323PresenceEndpoint epRecord;
          epRecord.m_Authorize.m_alias = list.m_alias;
         for (PINDEX i=0; i<list.m_subscription.GetSize(); i++) {
             epRecord.m_Authorize.Add(list[i]);
         }
        gw.insert(pair<H225_AliasAddress, H323PresenceEndpoint>(list.m_alias, epRecord));
     } else {
         H323PresenceEndpoint & epRecord = inf->second;
         epRecord.m_Authorize.m_alias = list.m_alias;
         for (PINDEX i=0; i<list.m_subscription.GetSize(); i++) {
             epRecord.m_Authorize.Add(list[i]);
         }
     }
}

void PostInstruction(H323PresenceStore & gw, const H323PresenceInstructions & list)
{
     H323PresenceStore::iterator inf = gw.find(list.m_alias);
     if (inf == gw.end()) {
        H323PresenceEndpoint epRecord;
          epRecord.m_Instruction.m_alias = list.m_alias;
         for (PINDEX i=0; i<list.m_instruction.GetSize(); i++) {
             epRecord.m_Instruction.Add(list[i]);
         }
        gw.insert(pair<H225_AliasAddress, H323PresenceEndpoint>(list.m_alias, epRecord));
     } else {
         H323PresenceEndpoint & epRecord = inf->second;
         epRecord.m_Instruction.m_alias = list.m_alias;
         for (PINDEX i=0; i<list.m_instruction.GetSize(); i++) {
             epRecord.m_Instruction.Add(list[i]);
         }
     }
}

void PostNotification(H323PresenceStore & gw, const H323PresenceNotifications & list)
{
    H323PresenceStore::iterator inf = gw.find(list.m_alias);
     if (inf == gw.end()) {
        H323PresenceEndpoint epRecord;
         epRecord.m_Notify.m_alias = list.m_alias;
         PStringList aliases;
         list.GetAliasList(aliases);
         if (aliases.GetSize() > 0)
            epRecord.m_Notify.SetAliasList(aliases);
         for (PINDEX i=0; i<list.m_notification.GetSize(); i++) {
             epRecord.m_Notify.Add(list[i]);
         }
        gw.insert(pair<H225_AliasAddress, H323PresenceEndpoint>(list.m_alias, epRecord));
     } else {
         H323PresenceEndpoint & epRecord = inf->second;
         for (PINDEX i=0; i<list.m_notification.GetSize(); i++) {
             epRecord.m_Notify.Add(list[i]);
         }
     }
}

void H460PresenceHandler::SetPresenceState(const PStringList & alias, unsigned localstate, const PString & localdisplay, PBoolean updateOnly)
{
    if (!feat || !feat->CommonFeature())
          return;

    H323PresenceNotification notification;
    notification.SetPresenceState((H323PresenceNotification::States)localstate,localdisplay);

    if (!updateOnly) {
        // Add Geoloation Information
        H460P_PresenceGeoLocation loc;
        if (EndpointLocale.BuildLocalePDU(loc))
            notification.AddEndpointLocale(loc);

        // Add the features the endpoint supports
        list<H460P_PresenceFeature>::iterator i = EndpointFeatures.begin();
        while (i != EndpointFeatures.end()) {
            notification.AddSupportedFeature(*i);
            i++;
        }

        // Add any generic data 
        notification.AddGenericData(genericData);
    }

    for (PINDEX i = 0; i< alias.GetSize(); ++i) {
        H323PresenceNotifications notify;
        notify.Add(notification);
        notify.SetAlias(alias[i]);
        notify.SetAliasList(alias);

        H323PresenceStore & store = GetPresenceStoreLocked();
        PostNotification(store,notify);
        PresenceStoreUnLock();
    }

    if (ep.GetGatekeeper() && ep.GetGatekeeper()->IsRegistered()) 
        pendingMessages = true;
}

void H460PresenceHandler::AddInstruction(const PString & epalias, 
                        H323PresenceHandler::InstType instType, 
                        const PresenceInstructList & subscribe,
                        PBoolean autoSend)
{
    H323PresenceInstructions instruct;
    instruct.SetAlias(epalias);
    PresenceInstructList::const_iterator i;
    for (i = subscribe.begin(); i != subscribe.end(); ++i) {
        unsigned t = (int)instType;
        H323PresenceInstruction inst((H323PresenceInstruction::Instruction)t, i->first,i->second);
        instruct.Add(inst);
    }

    H323PresenceStore & store = GetPresenceStoreLocked();
    PostInstruction(store,instruct);
    PresenceStoreUnLock();

    if (ep.GetGatekeeper() && ep.GetGatekeeper()->IsRegistered() && autoSend) 
        pendingMessages = true;
}

void H460PresenceHandler::AddAuthorization(const OpalGloballyUniqueID id,
                                            const PString & epalias,
                                            PBoolean approved,
                                            const PStringList & subscribe)
{
    H323PresenceSubscriptions sub;
    sub.SetAlias(epalias);
    for (PINDEX i=0; i< subscribe.GetSize(); i++) 
    {
        H323PresenceSubscription subs(id);
        subs.SetSubscriptionDetails(epalias,subscribe);
        subs.SetApproved(approved);
        sub.Add(subs);
    }

    H323PresenceStore & store = GetPresenceStoreLocked();
    PostSubscription(store,sub);
    PresenceStoreUnLock();

    if (ep.GetGatekeeper() && ep.GetGatekeeper()->IsRegistered()) 
        pendingMessages = true;
}

PStringList & H460PresenceHandler::GetSubscriptionList()
{
    return PresenceSubscriptions;
}

PStringList & H460PresenceHandler::GetBlockList()
{
    return PresenceBlockList;
}

void H460PresenceHandler::OnNotification(H323PresenceHandler::MsgType tag, const H460P_PresenceNotification & notify, const H225_AliasAddress & addr) 
{
    switch (tag) {
      case H323PresenceHandler::e_Notify:
          PresenceRcvNotification(addr,(const H323PresenceNotification &)notify);
          break;
      default:
          break;
    }
}
    
void H460PresenceHandler::OnSubscription(H323PresenceHandler::MsgType tag, const H460P_PresenceSubscription & subscription, const H225_AliasAddress & addr) 
{
    switch (tag) {
      case H323PresenceHandler::e_Authorize:
            PresenceRcvAuthorization(addr,(const H323PresenceSubscription &)subscription);
      default:
          break;
    }
}

void H460PresenceHandler::OnInstructions(H323PresenceHandler::MsgType tag, const H460P_ArrayOf_PresenceInstruction & instruction, const H225_AliasAddress & addr)
{
    PINDEX j = 0;
    switch (tag) {
      case H323PresenceHandler::e_Instruct:
          PTRACE(4,"OID3\tReceived Gatekeeper Instruction!");
          for (j=0; j < instruction.GetSize(); j++) {
             PresenceRcvInstruction(addr,(const H323PresenceInstruction &)instruction[j]);
          }
          break;
      default:
          break;
    }
}


void H460PresenceHandler::PresenceRcvNotification(const H225_AliasAddress & addr, const H323PresenceNotification & notify)
{
    PString alias = notify.GetAlias();
    H323PresenceNotification::States s;
    PString display = PString();
    notify.GetPresenceState(s,display);

    ep.PresenceNotification(H323GetAliasAddressString(addr),alias,s,display);
}

void H460PresenceHandler::PresenceRcvAuthorization(const H225_AliasAddress & addr, const H323PresenceSubscription & subscript)
{
    PresenceSubscriberList aliases;
    subscript.GetSubscriberDetails(aliases);
    OpalGloballyUniqueID id = subscript.GetSubscription();

    ep.PresenceAuthorization(id,H323GetAliasAddressString(addr),aliases);

}
    
void H460PresenceHandler::PresenceRcvInstruction(const H225_AliasAddress & addr, const H323PresenceInstruction & instruct)
{
    PString display = PString();
    PString avatar = PString();
    H323PresenceInstruction::Category cat = H323PresenceInstruction::e_UnknownCategory;
    PString alias = instruct.GetAlias(display, avatar,cat);
    unsigned i = instruct.GetTag();

    ep.PresenceInstruction(H323GetAliasAddressString(addr),i, alias, display, avatar, cat);
}

void H460PresenceHandler::AddEndpointFeature(int feat)
{
    H460P_PresenceFeature f;
    f.SetTag(feat);
    EndpointFeatures.push_back(f);
}

void H460PresenceHandler::AddEndpointH460Feature(const H225_GenericIdentifier & featid, const PString & display)
{
    H460P_PresenceFeature f;
    f.SetTag(H460P_PresenceFeature::e_generic);
    H460P_PresenceFeatureGeneric & g = f;
    g.m_identifier = featid;
    g.IncludeOptionalField(H460P_PresenceFeatureGeneric::e_display);
    g.m_display.SetValue(display);

    EndpointFeatures.push_back(f);
}

void H460PresenceHandler::AddEndpointGenericData(const H225_GenericData & data)
{
    int sz = genericData.GetSize();
    genericData.SetSize(sz+1);
    genericData[sz] = data;
}

///////////////////////////////////////////////////////////////////////

//static const char * OID_3 = "1.3.6.1.4.1.17090.0.3";            // Advertised Feature
static const char * OID_3 = "1.3.6.1.4.1.17090.0.12";            // Advertised Feature v2
static const char * OID3_ID = "1";

///////////////////////////////////////////////////////////////////////

H460_FEATURE(OID3);
PBoolean H460_FeatureOID3::isLoaded = false;
H460_FeatureOID3::H460_FeatureOID3()
: H460_FeatureOID(OID_3)
{
     PTRACE(4,"OID3\tInstance Created");

     handler = NULL;
     remoteSupport = false;
     FeatureCategory = FeatureSupported;
}

H460_FeatureOID3::~H460_FeatureOID3()
{
    isLoaded = false;
}

PStringArray H460_FeatureOID3::GetIdentifier()
{
    return PStringArray(OID_3);
}

void H460_FeatureOID3::AttachEndPoint(H323EndPoint * _ep)
{
    handler = _ep->GetPresenceHandler();

    if (handler != NULL) {
         handler->AttachFeature(this);
         isLoaded = true;
    }
}

int H460_FeatureOID3::GetPurpose()
{ 
    if (isLoaded)
        return FeatureRas; 
    else
        return FeaturePresence; 
}

PBoolean H460_FeatureOID3::OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu) 
{ 
    if (handler == NULL)
        return false;

    H460_FeatureOID feat = H460_FeatureOID(OID_3); 
    pdu = feat;
    return TRUE; 
}

PBoolean H460_FeatureOID3::OnSendRegistrationRequest(H225_FeatureDescriptor & pdu) 
{ 
    if (handler == NULL)
        return false;

    H460_FeatureOID feat = H460_FeatureOID(OID_3); 
    list<PASN_OctetString> raw;
    if (handler->BuildPresenceElement(H225_RasMessage::e_registrationRequest, raw)) {
        if (raw.size() > 0) {
            PASN_OctetString data = raw.front();
            feat.Add(OID3_ID,H460_FeatureContent(data));
            raw.pop_front();
            if (raw.size() > 0) {
                PTRACE(2,"OID3\tERROR: Too many elements for RRQ");
            }
        }
    }

    pdu = feat;
    return true;
}


void H460_FeatureOID3::OnReceiveRegistrationConfirm(const H225_FeatureDescriptor & pdu) 
{
    if (handler == NULL)
        return;

    remoteSupport = true;
    H460_FeatureOID & feat = (H460_FeatureOID &)pdu;

    if (feat.Contains(OID3_ID)) {
        PASN_OctetString & data = feat.Value(OID3_ID);
        handler->ReceivedPDU(data);
    }
}

PBoolean H460_FeatureOID3::OnSendServiceControlIndication(H225_FeatureDescriptor & pdu) 
{ 
    if (!remoteSupport)
        return false;

    H460_FeatureOID feat = H460_FeatureOID(OID_3); 

    list<PASN_OctetString> raw;
    if (handler->BuildPresenceElement(H225_RasMessage::e_serviceControlIndication, raw)) {
        if (raw.size() > 0) {
            PASN_OctetString data = raw.front();
            feat.Add(OID3_ID,H460_FeatureContent(data));
            raw.pop_front();
            if (raw.size() > 0) {
                PTRACE(2,"OID3\tERROR: Too many elements for SCI");
            }
        }
       return true;
    }

    return false; 
};

void H460_FeatureOID3::OnReceiveServiceControlIndication(const H225_FeatureDescriptor & pdu) 
{
   H460_FeatureOID & feat = (H460_FeatureOID &)pdu;

       if (feat.Contains(OID3_ID)) {
        PASN_OctetString & data = feat.Value(OID3_ID);
        handler->ReceivedPDU(data);
    }
};

#endif  // H323_H460P



