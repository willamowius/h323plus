
/* H461_base.cxx
 *
 * Copyright (c) 2013 Spranto Int'l Pte Ltd. All Rights Reserved.
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

#include <ptlib.h>
#include <openh323buildopts.h>

#ifdef H323_H461

#include "h460/h461_base.h"
#include <h460/h460.h>
#include "h323ep.h"
#include "h323pdu.h"

#ifdef _MSC_VER
#pragma warning(disable:4127)
#endif

#define DEFAULT_ASSOCIATE_TTL 43200  // 30 days
#define DEFAULT_STATUS_INTERVAL 180  // 3 hours

// Must Declare for Factory Loader.
H460_FEATURE(X1);

#define baseOID "0.0.8.461.0" 
#define ASSETAssociate      "0" 
#define ASSETApplication    "1"

H460_FeatureX1::H460_FeatureX1()
: H460_FeatureOID(baseOID), 
  m_ep(NULL), m_con(NULL), m_handler(NULL), m_endpointmode(0),
  m_enabled(false), m_supported(false)
{
     PTRACE(6,"X1\tInstance Created");
     FeatureCategory = FeatureSupported;
}

H460_FeatureX1::~H460_FeatureX1()
{
    delete m_handler;
}


void H460_FeatureX1::AttachEndPoint(H323EndPoint * _ep)
{
   m_ep = _ep;
   m_enabled = m_ep->IsASSETEnabled();
   if (!m_enabled)
       return;

   m_endpointmode = m_ep->GetEndPointASSETMode();
   switch (m_endpointmode) {
       case H323EndPoint::e_H461EndPoint: FeatureCategory = FeatureSupported; break;
       case H323EndPoint::e_H461ASSET: FeatureCategory = FeatureNeeded; break;
   } 
}

void H460_FeatureX1::AttachConnection(H323Connection * _con)
{
   m_con = _con;
   m_handler = new H461Handler(*this);
}

PStringArray H460_FeatureX1::GetIdentifier()
{
   return PStringArray(baseOID);
}

PBoolean H460_FeatureX1::FeatureAdvertised(int mtype)
{
    if (m_con->GetH461Mode() == H323Connection::e_h461EndpointCall) {
         switch (mtype) {
            case H460_MessageType::e_setup:
            case H460_MessageType::e_connect:
                return true;
            default:
                return false;
         }
    } else
        return false;
}

PBoolean H460_FeatureX1::CommonFeature()
{
    if (!m_supported)
        m_con->SetH461Mode(H323Connection::e_h461NormalCall);

    return m_supported;
}

PBoolean H460_FeatureX1::OnSendSetup_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!m_enabled)
        return false;

    PTRACE(6,"X1\tSending Setup");
    PASN_OctetString xpdu;
    if (m_handler->BuildPDU(xpdu)) {
        H460_FeatureOID feat = H460_FeatureOID(OpalOID(baseOID));
        feat.Add(ASSETApplication,H460_FeatureContent(xpdu));
        pdu = feat;
        return true; 
    }
    return false;
}

void H460_FeatureX1::OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu)
{
    if (!m_enabled)
        return;

    m_supported = true;
    H460_FeatureOID & feat = (H460_FeatureOID &)pdu;
    if (feat.Contains(ASSETApplication)) {
        PTRACE(6,"X1\tReceive Setup");
        PASN_OctetString & xpdu = feat.Value(ASSETApplication); 
        m_handler->HandlePDU(xpdu);
    }
}

PBoolean H460_FeatureX1::OnSendCallConnect_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!m_enabled)
        return false;

    PTRACE(6,"X1\tSending Connect");
    PASN_OctetString xpdu;
    if (m_handler->BuildPDU(xpdu)) {
        H460_FeatureOID feat = H460_FeatureOID(OpalOID(baseOID));
        feat.Add(ASSETApplication,H460_FeatureContent(xpdu));
        pdu = feat;
        return true; 
    }
    return false;
}

void H460_FeatureX1::OnReceiveCallConnect_UUIE(const H225_FeatureDescriptor & pdu)
{
    if (!m_enabled)
        return;

    m_supported = true;
    H460_FeatureOID & feat = (H460_FeatureOID &)pdu;
    if (feat.Contains(ASSETApplication)) {
        PTRACE(6,"X1\tReading Connect");
        PASN_OctetString & xpdu = feat.Value(ASSETApplication); 
        m_handler->HandlePDU(xpdu);
    }
}

PBoolean H460_FeatureX1::OnSendFacility_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!m_enabled || !m_supported)
        return false;

    PTRACE(6,"X1\tSending Facility");
    PASN_OctetString xpdu;
    if (m_handler->BuildPDU(xpdu)) {
        H460_FeatureOID feat = H460_FeatureOID(OpalOID(baseOID));
        feat.Add(ASSETApplication,H460_FeatureContent(xpdu));
        pdu = feat;
        return true; 
    }
    return false;
}

void H460_FeatureX1::OnReceiveFacility_UUIE(const H225_FeatureDescriptor & pdu)
{
    if (!m_enabled || !m_supported)
        return;

    H460_FeatureOID & feat = (H460_FeatureOID &)pdu;
    if (feat.Contains(ASSETApplication)) {
        PTRACE(6,"X1\tReading facility");
        PASN_OctetString & xpdu = feat.Value(ASSETApplication); 
        m_handler->HandlePDU(xpdu);
    }
}

int H460_FeatureX1::GetMode()
{
    return m_endpointmode;
}

H323Connection * H460_FeatureX1::GetConnection()
{
    return  m_con;
}
H461DataStore * H460_FeatureX1::GetDataStore()
{
    return m_ep->GetASSETDataStore();
}

//////////////////////////////////////////////////////////////////////////////////
// Handler

H461Handler::H461Handler(H460_FeatureX1 & feature)
: m_feature(feature), m_connection(feature.GetConnection()), 
  m_mode(feature.GetMode()), m_dataStore(feature.GetDataStore())
{

}

PBoolean H461Handler::BuildPDU(PASN_OctetString & pdu)
{  
    PBoolean success = true;
    H461AssetPDU element;
    H461_ASSETPDU msg;
    msg.SetSize(1);
    msg[0] = element.BuildMessage(this, success);
    if (success)
        pdu.EncodeSubType(msg);

    return success;
}

PBoolean H461Handler::HandlePDU(const PASN_OctetString & msg)
{
    H461_ASSETPDU element;
    PPER_Stream raw(msg);
    if (!element.Decode(raw) || element.GetSize() == 0) {
        PTRACE(2,"X1\tError Decoding Element. Malformed or no messages."); 
        return false;
    }
    return ProcessPDU(element);
}

PBoolean H461Handler::ProcessPDU(H461_ASSETPDU & pdu)
{
    for (PINDEX i=0; i<pdu.GetSize(); ++i) {
        H461AssetPDU msg(pdu[i]);
        ProcessMessage(msg);
    }
    return true;
}

PBoolean H461Handler::ProcessMessage(H461AssetPDU & msg)
{
    return msg.ProcessMessage(this);
}

///////////////////////////////////////////////////////////////////////////////////
// Utilities

#define opt_NONE    0
#define opt_ASSOC   1
#define opt_CALL    2
#define opt_BOTH    3
#define opt_DEPEND  4

#define chk_associateRequest   opt_NONE
#define chk_associateResponse  opt_ASSOC
#define chk_statusRequest      opt_ASSOC
#define chk_statusResponse     opt_ASSOC
#define chk_listRequest        opt_CALL
#define chk_listResponse       opt_CALL
#define chk_callApplist        opt_BOTH
#define chk_preInvokeRequest   opt_BOTH
#define chk_preInvokeResponse  opt_BOTH    
#define chk_invokeRequest      opt_DEPEND
#define chk_invokeResponse     opt_DEPEND
#define chk_invoke             opt_BOTH
#define chk_invokeStartList    opt_CALL
#define chk_invokeNotify       opt_CALL
#define chk_stopRequest        opt_BOTH
#define chk_stopNotify         opt_CALL
#define chk_callRelease        opt_BOTH

PString H323GetGUIDString(const H225_GloballyUniqueID & guid) {
    return OpalGloballyUniqueID(guid).AsString();
}

PString H323GetCallIdentifier(const H225_CallIdentifier & callid) {
    return H323GetGUIDString(callid.m_guid);
}

void H323SetGUIDString(const PString & val, H225_GloballyUniqueID & guid) {
    OpalGloballyUniqueID id(val);
    return guid.SetValue(id);
}

void H323SetCallIdentifier(const PString & val ,H225_CallIdentifier & callid) {
    H323SetGUIDString(val, callid.m_guid);
}

void GetToken(PString & token, const H461_ASSETMessage & pdu) {
    if (pdu.HasOptionalField(H461_ASSETMessage::e_associateToken))
        token = H323GetGUIDString(pdu.m_associateToken);
}

void SetToken(const PString & val, H461_ASSETMessage & pdu) {
    pdu.IncludeOptionalField(H461_ASSETMessage::e_associateToken);
    H323SetGUIDString(val,pdu.m_associateToken);
}

void GetCallID(PString & callid, const H461_ASSETMessage & pdu) {
    if (pdu.HasOptionalField(H461_ASSETMessage::e_callIdentifier)) 
        callid = H323GetCallIdentifier(pdu.m_callIdentifier);
}

void SetCallID(const PString & val, H461_ASSETMessage & pdu) {
    pdu.IncludeOptionalField(H461_ASSETMessage::e_callIdentifier);
    H323SetCallIdentifier(val,pdu.m_callIdentifier);
}
    

#define CHECKTOKEN(name) \
if (m_token.IsEmpty()) { \
    PTRACE(2,"H461\tMessage " << #name << " missing Association Token"); \
   return false; } \

#define CHECKCALLID(name) \
if (m_callid.IsEmpty()) { \
    PTRACE(2,"H461\tMessage " << #name << " missing Call Identifier"); \
    return false; } \


#define CHECKPARAMETERS(name) \
if (chk_##name != opt_NONE) { \
    if (chk_##name == opt_ASSOC || chk_##name == opt_BOTH) { CHECKTOKEN(name) } \
    if (chk_##name == opt_CALL || chk_##name == opt_BOTH || chk_##name == opt_DEPEND) { CHECKCALLID(name) } \
}


#define BUILDPARAMETERS(name) \
if (chk_##name == opt_ASSOC || chk_##name == opt_BOTH) SetToken(m_token,*this); \
if (chk_##name == opt_CALL || chk_##name == opt_BOTH || chk_##name == opt_DEPEND) SetCallID(m_callid,*this); \
if (chk_##name == opt_DEPEND && !m_token) SetToken(m_token,*this); \
PTRACE(6,"H461\tPDU to send " << *this);

#define H461HANDLECASE(name) \
    case H461_ApplicationIE::e_##name : return Handle_##name(m_connection);

#define H461BUILDCASE(name) \
    case H461_ApplicationIE::e_##name : return Build_##name(m_connection);

#define H461HANDLE(name) \
PBoolean H461AssetPDU::Handle_##name(H323Connection* m_connection) { \
CHECKPARAMETERS(name) \
return m_dataStore->Received_##name(m_connection, m_token, m_callid, m_application); } 
    
#define H461BUILD(name) \
H461_ASSETMessage & H461AssetPDU::Build_##name(H323Connection* m_connection) { \
m_dataStore->Build_##name(m_connection, m_token, m_callid, m_appID, m_invokeToken, m_aliasAddress, m_approved, m_application); \
BUILDPARAMETERS(name) \
return *this; }


H461AssetPDU::H461AssetPDU()
: m_handler(NULL), m_connection(NULL), m_dataStore(NULL), m_token(PString()), m_callid(PString()), m_appID(-1), 
  m_invokeToken(PString()), m_aliasAddress(PString()), m_approved(false)
{

}

H461AssetPDU::H461AssetPDU(H461_ASSETMessage & msg)
: m_handler(NULL), m_connection(NULL), m_dataStore(NULL), m_token(PString()), m_callid(PString()), m_appID(-1), 
  m_invokeToken(PString()), m_aliasAddress(PString()), m_approved(false)
{
    m_application = msg.m_application;
    if (msg.HasOptionalField(H461_ASSETMessage::e_associateToken)) {
        IncludeOptionalField(H461_ASSETMessage::e_associateToken);
        m_associateToken = msg.m_associateToken;
    }
    if (msg.HasOptionalField(H461_ASSETMessage::e_callIdentifier)) {
        IncludeOptionalField(H461_ASSETMessage::e_callIdentifier);
        m_callIdentifier = msg.m_callIdentifier;
    }
}

H461AssetPDU::~H461AssetPDU()
{

}

PBoolean H461AssetPDU::BuildParameters(H323Connection* connection)
{
    H323Connection::H461MessageInfo & m_info = connection->GetH461MessageInfo();

    if (m_info.m_message < 0)
        return false;

    m_application.SetTag(m_info.m_message);
    m_token = m_info.m_assocToken;
    m_callid = m_info.m_callToken;
    m_appID = m_info.m_applicationID;
    m_invokeToken = m_info.m_invokeToken;
    m_aliasAddress = m_info.m_aliasAddress;
    m_approved = m_info.m_approved;
    return true;
}

PBoolean H461AssetPDU::ProcessMessage(H461Handler * handler)
{
    m_handler = handler;
    m_connection = handler->GetConnection();
    m_dataStore = handler->GetDataStore();

    GetToken(m_token, *this);
    GetCallID(m_callid, *this);

    PTRACE(6,"H461\tPDU Received " << *this);

    switch (m_application.GetTag()) {
        H461HANDLECASE(associateRequest)
        H461HANDLECASE(associateResponse)
        H461HANDLECASE(statusRequest)
        H461HANDLECASE(statusResponse)
        H461HANDLECASE(listRequest)
        H461HANDLECASE(listResponse)
        H461HANDLECASE(callApplist)
        H461HANDLECASE(preInvokeRequest)
        H461HANDLECASE(preInvokeResponse)
        H461HANDLECASE(invokeRequest)
        H461HANDLECASE(invokeResponse)
        H461HANDLECASE(invoke)
        H461HANDLECASE(invokeStartList)
        H461HANDLECASE(invokeNotify)
        H461HANDLECASE(stopRequest)
        H461HANDLECASE(stopNotify)
        H461HANDLECASE(callRelease)    
        default: break;
    }
    return false;
}

H461_ASSETMessage & H461AssetPDU::BuildMessage(H461Handler * handler, PBoolean & success)
{
    m_handler = handler;
    m_connection = handler->GetConnection();
    if (!BuildParameters(m_connection)) {
        success = false;
        return *this;
    }
    m_dataStore = handler->GetDataStore();

    switch (m_application.GetTag()) {
        H461BUILDCASE(associateRequest)
        H461BUILDCASE(associateResponse)
        H461BUILDCASE(statusRequest)
        H461BUILDCASE(statusResponse)
        H461BUILDCASE(listRequest)
        H461BUILDCASE(listResponse)
        H461BUILDCASE(callApplist)
        H461BUILDCASE(preInvokeRequest)
        H461BUILDCASE(preInvokeResponse)
        H461BUILDCASE(invokeRequest)
        H461BUILDCASE(invokeResponse)
        H461BUILDCASE(invoke)
        H461BUILDCASE(invokeStartList)
        H461BUILDCASE(invokeNotify)
        H461BUILDCASE(stopRequest)
        H461BUILDCASE(stopNotify)
        H461BUILDCASE(callRelease)    
        default: break;
    }
    success = true;
    return *this;
}

PBoolean H461AssetPDU::ValidateToken(const PString & token)
{
    return false;
}

H461HANDLE(associateRequest)
H461HANDLE(associateResponse)
H461HANDLE(statusRequest)
H461HANDLE(statusResponse)
H461HANDLE(listRequest)
H461HANDLE(listResponse)
H461HANDLE(callApplist)
H461HANDLE(preInvokeRequest)
H461HANDLE(preInvokeResponse)
H461HANDLE(invokeRequest)
H461HANDLE(invokeResponse)
H461HANDLE(invoke)
H461HANDLE(invokeStartList)
H461HANDLE(invokeNotify)
H461HANDLE(stopRequest)
H461HANDLE(stopNotify)
H461HANDLE(callRelease)


H461BUILD(associateRequest)
H461BUILD(associateResponse)
H461BUILD(statusRequest)
H461BUILD(statusResponse)
H461BUILD(listRequest)
H461BUILD(listResponse)
H461BUILD(callApplist)
H461BUILD(preInvokeRequest)
H461BUILD(preInvokeResponse)
H461BUILD(invokeRequest)
H461BUILD(invokeResponse)
H461BUILD(invoke)
H461BUILD(invokeStartList)
H461BUILD(invokeNotify)
H461BUILD(stopRequest)
H461BUILD(stopNotify)
H461BUILD(callRelease)


//////////////////////////////////////////////////////////////////////////////////

PString GetGenericIdentifier(const H225_GenericIdentifier & gid)
{
    if (gid.GetTag() == H225_GenericIdentifier::e_oid) {
        const PASN_ObjectId & val = gid;
        return val.AsString();
    }
    return PString();
}

void SetGenericIdentifier(const PString & id, H225_GenericIdentifier & gid)
{
    gid.SetTag(H225_GenericIdentifier::e_oid);
    PASN_ObjectId & val = gid;
    val.SetValue(id);
}

PString GetDisplay(const H461_ArrayOf_ApplicationDisplay & display)
{
    if (display.GetSize() > 0)
        return display[0].m_display.GetValue();
    return PString();
}

void SetDisplay(const PString & id, H461_ArrayOf_ApplicationDisplay & display)
{
    display.SetSize(1);
    display[0].m_display.SetValue(id);
}

void GetApplicationStatus(H461DataStore::Application & app, int associate, const H461_ApplicationStatus & application) 
{
    app.associate = associate;
    app.id = GetGenericIdentifier(application.m_applicationId);
    if (application.HasOptionalField(H461_ApplicationStatus::e_display))
        app.displayName = GetDisplay(application.m_display);
    if (application.HasOptionalField(H461_ApplicationStatus::e_avatar))
        app.avatar = application.m_avatar.GetValue();
    if (application.HasOptionalField(H461_ApplicationStatus::e_state))
        app.state = (H461DataStore::ApplicationStates)application.m_state.GetTag();
}


void SetApplicationStatus(const H461DataStore::Application & app, H461_ApplicationStatus & application) 
{
    SetGenericIdentifier(app.id, application.m_applicationId);

    if (!app.displayName) {
        application.IncludeOptionalField(H461_ApplicationStatus::e_display);
        SetDisplay(app.displayName, application.m_display);
    }

    if (!app.avatar) {
        application.IncludeOptionalField(H461_ApplicationStatus::e_avatar);
        application.m_avatar.SetValue(app.avatar);
    }

    application.IncludeOptionalField(H461_ApplicationStatus::e_state);
    application.m_state.SetTag(app.state);
}

void SetApplicationStatus(const H461DataStore::ApplicationMap & apps, H461_ArrayOf_ApplicationStatus & applications)
{
    applications.SetSize(apps.size());
    int j = 0;
    H461DataStore::ApplicationMap::const_iterator i = apps.begin();
    while (i != apps.end()) {
        SetApplicationStatus(i->second,applications[j]);
        ++j;
        ++i;
    }
}

void SetApplicationStatus(const std::list<int> & id, H461DataStore::ApplicationMap & apps, H461_ArrayOf_ApplicationStatus & applications)
{
    applications.SetSize(id.size());
    int j = 0;
    std::list<int>::const_iterator i = id.begin();
    while (i != id.end()) {
        SetApplicationStatus(apps[*i],applications[j]);
        ++j;
        ++i;
    }
}

void GetApplicationStatus(const H461_ArrayOf_ApplicationStatus & applications, H461DataStore::ApplicationList & id)
{
    for (PINDEX j=0; j < applications.GetSize(); ++j) {
        H461DataStore::Application app;
        GetApplicationStatus(app,-1,applications[j]);
        id.push_back(app);
    }
}

void BuildCallApplicationList(const H461DataStore::ApplicationMap & apps, std::list<int> & applist)
{
    H461DataStore::ApplicationMap::const_iterator i = apps.begin();
    while(i != apps.end()) {
        if (i->second.availableForCall)
            applist.push_back(i->first);
        ++i;
    }
}

void MergeCallApplicationList(const H461DataStore::ApplicationList & remote, H461DataStore::ApplicationMap & apps, std::list<int> & applist)
{
    H461DataStore::ApplicationList::const_iterator i;
    std::list<int>::iterator j = applist.begin();
    bool found = false;
    for (j = applist.begin(); j != applist.end(); ++j) {
        found = false;
        for (i = remote.begin(); i != remote.end(); ++i) {
            if ((*i).id == apps[*j].id) {
                found = true;
                break;
            }
        }
        if (!found)
            applist.erase(j);
    }
}

H461DataStore::H461DataStore()
: m_timeToLive(DEFAULT_ASSOCIATE_TTL), m_statusInterval(DEFAULT_STATUS_INTERVAL)
{

}

H461DataStore::Application::Application()
: associate(-1), id(PString()), displayName(PString()), avatar(PString()), state(e_stateAvailable),
  availableForCall(true), nextUpdate(0)
{

}

H461DataStore::CallData::CallData()
: callId(PString()), associate(-1), remote(PString())
{

}

PString H461DataStore::MessageAsString(Messages msg) {
    switch (msg) {
        case e_id_associateRequest: return "AssociateRequest";
        case e_id_associateResponse: return "AssociateResponse";
        case e_id_statusRequest: return "StatusRequest";
        case e_id_statusResponse: return "StatusResponse";
        case e_id_listRequest: return "ListRequest";
        case e_id_listResponse: return "listResponse";
        case e_id_callApplist: return "CallAppList";
        case e_id_preInvokeRequest: return "PreInvokeRequest";
        case e_id_preInvokeResponse: return "PreInvokeResponse";
        case e_id_invokeRequest: return "InvokeRequest";
        case e_id_invokeResponse: return "InvokeResponse";
        case e_id_invoke: return "Invoke";
        case e_id_invokeStartList: return "InvokeStartList";
        case e_id_invokeNotify: return "InvokeNotify";
        case e_id_stopRequest: return "StopRequest";
        case e_id_stopNotify: return "StopNotify";
        case e_id_callRelease: return "CallRelease";
    }
    return "Unknown Msg";
}
    

/////////////////////////////////////
//Utilities

int H461DataStore::FindAssociateID(const PString & token)
{
    if (token.IsEmpty()) return -1;

    AssociateMap::const_iterator i = m_associates.begin();
    while (i != m_associates.end()) {
        if (i->second.token == token)
            return i->first;
        ++i;
    }
    return -1;
}

void H461DataStore::FindAssociateAddress(int id, PString & address, PString & token)
{
    AssociateMap::const_iterator i = m_associates.begin();
    while (i != m_associates.end()) {
        if (i->first == id) {
            address = i->second.alias;
            token = i->second.token;
            break;
        }
        ++i;
    }
}

int H461DataStore::FindApplicationID(const PString & id)
{
    if (id.IsEmpty()) return -1;

    ApplicationMap::const_iterator i = m_applications.begin();
    while (i != m_applications.end()) {
        if (i->second.id == id)
            return i->first;
        ++i;
    }
    return -1;
}

int H461DataStore::FindApplicationID(const PString & token, const PString & id)
{
    if (token.IsEmpty()) return -1;

    int assoc = FindAssociateID(token);
    if (assoc < 0) return -1;

    return FindApplicationID(assoc, id);
}

int H461DataStore::FindApplicationID(int assoc, const PString & id)
{
    ApplicationMap::const_iterator i = m_applications.begin();
    while (i != m_applications.end()) {
        if (i->second.associate == assoc && i->second.id == id)
            return i->first;
        ++i;
    }
    return -1;
}

int H461DataStore::FindCallData(const PString & callIdentifier)
{
    if (callIdentifier.IsEmpty()) return -1;

    CallApplicationMap::const_iterator i = m_callapplications.begin();
    while (i != m_callapplications.end()) {
        if (i->second.callId == callIdentifier)
            return i->first;
        ++i;
    }
    return -1;
}

PString H461DataStore::FindCall(int id)
{
    CallApplicationMap::const_iterator i = m_callapplications.begin();
    while (i != m_callapplications.end()) {
        if (i->first == id)
            return i->second.callId;
        ++i;
    }
    return PString();
}

PBoolean H461DataStore::FindApplications(DataType type, const PString & key, std::list<int> & id)
{
    int idx = -1;
    if (type == H461DataStore::e_associate) {
        idx = FindAssociateID(key);
        if (idx < 0) return false;
    }
 
    ApplicationMap::const_iterator i = m_applications.begin();
    while (i != m_applications.end()) {
        if ((type == H461DataStore::e_associate && i->second.associate == idx) ||          
            (type == H461DataStore::e_application && i->second.id == key))
                id.push_back(i->first);
        ++i;
    }
    return (id.size() > 0);
}


////////////////////////////////////////

template <typename mapType>
int H461ASSIGN(const mapType & map) {
    if (map.size() == 0)
        return 0;
    return map.rbegin()->first+1;
}

#define H461BUILDPDU(name) \
void H461DataStore::Build_##name(H323Connection* connection, const PString & token, PString & callid, int appID, \
                            const PString & invokeToken, const PString & aliasAddress, bool approved, msg_##name & pdu) { \
int assocID = FindAssociateID(token); int callID = FindCallData(callid); \
OnBuildIE(e_id_##name, connection, assocID, callID, appID, invokeToken, aliasAddress, approved); \


#define H461RECEIVEDPDU_HEAD(name) \
PBoolean H461DataStore::Received_##name(H323Connection* connection, const PString & token, PString & callid, const msg_##name & pdu) { \
int assocID = FindAssociateID(token); int callID = FindCallData(callid); int appID = -1; \
PString invokeToken = PString(); PString aliasAddress = PString(); bool approved = false;

#define H461RECEIVEDPDU_TAIL(name) \
return OnReceiveIE(e_id_##name, connection, assocID, callID, appID, invokeToken, aliasAddress, approved); } 


void H461DataStore::UpdateApplication(int assoc, const H461_ApplicationStatus & application)
{
    H461DataStore::Application app;
    GetApplicationStatus(app, assoc, application);
    app.associate = assoc;
    int appID = FindApplicationID(assoc,app.id);
    bool newEntry = false;
    if (appID < 0) {
        // New Application
        appID = H461ASSIGN<ApplicationMap>(m_applications);
        m_applications.insert(ApplicationMap::value_type(appID,app));
        newEntry = true;
    } else {
        m_applications[appID].state = app.state;
        m_applications[appID].nextUpdate += m_timeToLive;
    }
    OnUpdateApplication(appID, m_applications[appID], newEntry);
}

void H461DataStore::UpdateApplications(int assoc, const H461_ArrayOf_ApplicationStatus & applications)
{
    for (PINDEX i=0; i<applications.GetSize(); ++i)
        UpdateApplication(assoc, applications[i]);
}

void H461DataStore::BuildApplicationAvailable(int assocID, int callID, const PString & display, H461_ArrayOf_ApplicationAvailable & pdu)
{
    pdu.SetSize(0);

    CallData c = m_callapplications[callID];
    std::list<int>::const_iterator i = c.applications.begin();
    while (i != c.applications.end()) {
        if (m_applications[*i].associate == assocID) {
            int sz = pdu.GetSize();
            pdu.SetSize(sz+1);
            pdu[sz].m_aliasAddress.SetSize(1);
            H323SetAliasAddress(display, pdu[sz].m_aliasAddress[0]);
            SetGenericIdentifier(m_applications[*i].id, pdu[sz].m_applicationId);

        }
        ++i;
    }
}

int H461DataStore::SetApplicationAvailable(int assocID, const PString & callidentifier, const H461_ArrayOf_ApplicationAvailable & pdu)
{
    if (pdu.GetSize() == 0)
        return -1;

    CallData cData;
    cData.callId = callidentifier;
    cData.associate = assocID;
    cData.remote = H323GetAliasAddressString(pdu[0].m_aliasAddress[0]);

    for (PINDEX i=0; i< pdu.GetSize(); ++i) {
        int appID = FindApplicationID(GetGenericIdentifier(pdu[i].m_applicationId));
        if (appID > -1)
            cData.applications.push_back(appID);
    }
    
    int callID = H461ASSIGN<CallApplicationMap>(m_callapplications);
    m_callapplications.insert(CallApplicationMap::value_type(callID,cData));
    return callID;
}

bool H461DataStore::FindCallAssociates(int callID, std::list<int> & assoc)
{
    std::list<int> & apps = m_callapplications[callID].applications;
    std::list<int>::const_iterator i = apps.begin();
    while (i != apps.end()) {
        assoc.push_back(m_applications[*i].associate);
        ++i;
    }
    assoc.unique();
    return (assoc.size() > 0);
}

void H461DataStore::GetCallApplicationList(const PString & call)
{
    int callid = FindCallData(call);
    if (callid > -1) {
       std::list<int> apps = m_callapplications[callid].applications;
       std::list<int>::const_iterator i = apps.begin();
       while (i != apps.end()) {
           Application app = m_applications[*i];
           OnCallApplicationList(*i,app.displayName, app.avatar, (app.state == e_stateAvailable));
           ++i;
       }
    }
}

void H461DataStore::GetApplicationCallList(int appid)
{
   CallApplicationMap::const_iterator i = m_callapplications.begin();
   while (i != m_callapplications.end()) {
       std::list<int> apps = i->second.applications;
       std::list<int>::const_iterator j = apps.begin();
       while (j != apps.end()) {
           if (*j == appid) {
                OnApplicationCallList(i->first, i->second.remote);
           }
           ++j;
       }
       ++i;
   }
}

////////////////////////////////////////

PBoolean H461DataStore::OnReceiveIE(Messages id, H323Connection* connection, int assoc, int callid, int appid, const PString & invokeToken, const PString & aliasAddress, bool approval)
{
    PTRACE(2,"H461\tReceived " << MessageAsString(id) <<  " message.");

    H323Connection::H461MessageInfo & info =  connection->GetH461MessageInfo();
    info.m_message = id;
    if (assoc > -1) info.m_assocToken = m_associates[assoc].token;
    if (callid > -1) info.m_callToken = m_callapplications[callid].callId;
    if (appid > -1) info.m_applicationID = appid;

    PBoolean endCall = false;
    PBoolean reply = false;
    PBoolean facilityMsg = false;

    switch (id) {
       case H461DataStore::e_id_associateRequest:
           if (OnAssociateRequest(connection->GetRemotePartyAliases()[0], connection->GetRemotePartyName())) {
               int newId = CreateNewAssociation();
               m_associates[newId].alias = connection->GetRemotePartyAliases()[0];
               m_associates[newId].displayName = connection->GetRemotePartyName();
               info.m_assocToken = m_associates[newId].token;
               reply = true;
           } else
               endCall = true;
            break;
       case H461DataStore::e_id_associateResponse: 
           facilityMsg=true;
           reply = true; 
           break;
       case H461DataStore::e_id_statusRequest: 
           reply = true; 
           break;
       case H461DataStore::e_id_listRequest: 
           reply = true; 
           break;
       case H461DataStore::e_id_preInvokeRequest: 
           reply = true; 
           break;
       case H461DataStore::e_id_invokeRequest:
           if (OnInvokeRequest(connection->GetRemotePartyAliases()[0], connection->GetRemotePartyName(), appid)) {
               if (connection->GetEndPoint().GetEndPointASSETMode() == H323EndPoint::e_H461ASSET) {
                   info.m_aliasAddress = connection->GetEndPoint().GetAliasNames()[0];
                   info.m_invokeToken = GenerateApplicationToken();
                   info.m_approved = true;
                   reply = true;
               } else {
                   StartApplication(callid,appid,true);
                   info.m_message = -1;
               }
           } else {
              info.m_approved = false;
              reply = true;
           }
           break;
       case H461DataStore::e_id_invokeResponse:
           if (assoc > -1) {
               SendFacility(info.m_callToken, H461DataStore::e_id_invokeResponse,info.m_applicationID, invokeToken, aliasAddress, approval);
               endCall = true;
           } else {
               InvokeApplication(callid,appid,invokeToken,aliasAddress,approval);
               info.m_message = -1;
           } 
           break;

       case H461DataStore::e_id_invokeStartList: 
           info.m_message = -1; 
           break;
       case H461DataStore::e_id_listResponse:
           UpdateAssociateCall(callid, true);
           info.m_message = -1;
           break;
       case H461DataStore::e_id_invoke:
           MakeApplicationCall(appid, info.m_callToken, invokeToken, aliasAddress); 
           info.m_message = -1;
           break;
       case H461DataStore::e_id_callApplist:
           OnAssociateCallStart(callid);
           info.m_message = -1;
           break;
        // End the connection
       case H461DataStore::e_id_callRelease:
           OnAssociateCallEnd(callid);
           Call_Terminate(callid);
           endCall = true;
           break;
       case H461DataStore::e_id_preInvokeResponse:
           SendFacility(info.m_callToken, H461DataStore::e_id_invokeRequest,info.m_applicationID);
           endCall = true;
           break;
       case H461DataStore::e_id_statusResponse:
       case H461DataStore::e_id_invokeNotify:
       case H461DataStore::e_id_stopNotify:
       case H461DataStore::e_id_stopRequest:
           endCall = true;
           break;
    };

    if (reply) {
        if (connection->IsConnected() || facilityMsg)
            SendInternalFacility(connection,id+1); 
        else
            info.m_message=id+1;
    }
    else if (endCall) {
        connection->ClearCall();
    }

    return true;
}

void H461DataStore::SendInternalFacility(H323Connection * connection, int id)
{
    H323Connection::H461MessageInfo & info =  connection->GetH461MessageInfo();
    info.m_message=id;

    H323SignalPDU facilityPDU;
    facilityPDU.BuildFacility(*connection, TRUE,8);
    H225_FeatureSet fs;
    if (connection->OnSendFeatureSet(H460_MessageType::e_facility, fs, false)) {
        facilityPDU.m_h323_uu_pdu.IncludeOptionalField(H225_H323_UU_PDU::e_genericData);
        H225_ArrayOf_FeatureDescriptor & fsn = fs.m_supportedFeatures;
        H225_ArrayOf_GenericData & data = facilityPDU.m_h323_uu_pdu.m_genericData;
        for (PINDEX i=0; i < fsn.GetSize(); i++) {
             PINDEX lastPos = data.GetSize();
             data.SetSize(lastPos+1);
             data[lastPos] = fsn[i];
        }
    }
    connection->WriteSignalPDU(facilityPDU);
}

void H461DataStore::OnBuildIE(Messages id, H323Connection* connection, int assoc, int callid, int appid, const PString & invokeToken, const PString & aliasAddress, bool approval)
{
    PTRACE(2,"H461\tBuild " << MessageAsString(id) <<  " message.");

   switch (id) {
       case H461DataStore::e_id_listResponse:
           UpdateAssociateCall(callid, true);
           break;
       case H461DataStore::e_id_associateRequest:
       case H461DataStore::e_id_associateResponse: 
       case H461DataStore::e_id_statusRequest:
       case H461DataStore::e_id_listRequest:
       case H461DataStore::e_id_preInvokeRequest:
       case H461DataStore::e_id_invokeRequest:
       case H461DataStore::e_id_invokeStartList:
       case H461DataStore::e_id_invokeResponse:
       case H461DataStore::e_id_invoke:
       case H461DataStore::e_id_statusResponse:
       case H461DataStore::e_id_callApplist:
       case H461DataStore::e_id_preInvokeResponse:
       case H461DataStore::e_id_invokeNotify:
       case H461DataStore::e_id_stopNotify:
       case H461DataStore::e_id_stopRequest:
       case H461DataStore::e_id_callRelease:
           break;
    };
}

void H461DataStore::LoadDataStoreAssociate(int id, const PString & alias, const PString & displayName, const PString & token, PInt64 expire, PBoolean alert)
{

}

void H461DataStore::LoadDataStoreApplication(int id, int associate, const PString & appID, const PString & displayName, const PString & avatar, int state, PBoolean availableForCall)
{
    Application cApp;
    cApp.associate = associate;
    cApp.avatar = avatar;
    cApp.displayName = displayName;
    cApp.id = appID;
    cApp.state = (H461DataStore::ApplicationStates)state;
    cApp.availableForCall = availableForCall;
    m_applications.insert(ApplicationMap::value_type(id,cApp));
}

int H461DataStore::Call_Initiate(const PString & callid, int assoc)
{ 
    int callID = FindCallData(callid);
    if (callID > -1)
        return callID;

    CallData cData;
    cData.callId = callid;
    cData.associate = assoc;
    BuildCallApplicationList(m_applications,cData.applications);
    
    callID = H461ASSIGN<CallApplicationMap>(m_callapplications);
    m_callapplications.insert(CallApplicationMap::value_type(callID,cData));
    return callID;
}
    
void H461DataStore::Call_Terminate(int callID)
{
    CallApplicationMap::iterator i = m_callapplications.find(callID);
    if (i != m_callapplications.end()) {
        m_callapplications.erase(i);
    }
}

int H461DataStore::CreateNewAssociation()
{
    Associate associate;
    OpalGloballyUniqueID guid;
    associate.token = guid.AsString();
    associate.expire = PTimer::Tick().GetSeconds() + DEFAULT_ASSOCIATE_TTL;
    int id = H461ASSIGN<AssociateMap>(m_associates);
    m_associates.insert(AssociateMap::value_type(id,associate));
    return id;
}

PString H461DataStore::GenerateApplicationToken()
{
    OpalGloballyUniqueID guid;
    return guid.AsString();
}

void H461DataStore::UpdateAssociateCall(const PString & call,  PBoolean startCall) {
    int callid = FindCallData(call);
    UpdateAssociateCall(callid, startCall);
}

void H461DataStore::UpdateAssociateCall(int callid, PBoolean startCall)
{
    std::list<int> assoc;
    if (FindCallAssociates(callid,assoc)) {
        std::list<int>::const_iterator i = assoc.begin();
        while (i != assoc.end()) {
            if (startCall)
                MakeAssociateCall(H461DataStore::e_id_callApplist, *i ,callid);
            else
                MakeAssociateCall(H461DataStore::e_id_callRelease, *i ,callid);
            ++i;
        }
    }
}

void H461DataStore::StartApplication(const PString & callToken, int i)
{
    int callid = FindCallData(callToken);
    StartApplication(callid, i);
}

void H461DataStore::StartApplication(int callid, int i, bool remote) 
{
    CallData & data = m_callapplications[callid];
    std::list<int> app = data.applications;
    std::list<int>::const_iterator j = app.begin();
    while (j != app.end()) {
        if (*j == i) {
            Application & app = m_applications[i];
            if (remote)
                MakeAssociateCall(H461DataStore::e_id_invokeRequest,app.associate,callid,i);
            else
                MakeAssociateCall(H461DataStore::e_id_preInvokeRequest,app.associate,callid,i);
            return;
        }
        ++j;
    }
}

void H461DataStore::InvokeApplication(int callid, int i, const PString & invokeToken, const PString & aliasAddress, bool approval) 
{
    CallData & data = m_callapplications[callid];
    std::list<int> app = data.applications;
    std::list<int>::const_iterator j = app.begin();
    while (j != app.end()) {
        if (*j == i) {
            Application & app = m_applications[i];
            MakeAssociateCall(H461DataStore::e_id_invoke,app.associate,callid,i,invokeToken,aliasAddress,approval);
            return;
        }
        ++j;
    }
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
H461BUILDPDU(associateRequest)
    H225_TimeToLive & ttl = pdu.m_timeToLive;
    ttl = (unsigned)m_timeToLive;
}

H461BUILDPDU(associateResponse)
    H323SetGUIDString(m_associates[assocID].token,pdu.m_associateToken);
    H225_TimeToLive & ttl = pdu.m_timeToLive;
    ttl = (unsigned)m_timeToLive;
    pdu.IncludeOptionalField(msg_associateResponse::e_statusInterval);
    H225_TimeToLive & stat = pdu.m_statusInterval;
    stat = (unsigned)m_statusInterval;
}

H461BUILDPDU(statusRequest)
    // Nothing to do.
}

H461BUILDPDU(statusResponse)
    SetApplicationStatus(m_applications,pdu);
}

H461BUILDPDU(listRequest)
    callid = connection->GetCallIdentifier().AsString();
    callID = Call_Initiate(callid);
    SetApplicationStatus(m_callapplications[callID].applications,m_applications,pdu);
}

H461BUILDPDU(listResponse)
    SetApplicationStatus(m_callapplications[callID].applications,m_applications,pdu);
}

H461BUILDPDU(callApplist)
    BuildApplicationAvailable(assocID, callID, connection->GetRemotePartyName(), pdu);
}

H461BUILDPDU(preInvokeRequest)
if (appID > -1) {
     SetGenericIdentifier(m_applications[appID].id,pdu.m_applicationId);
}
}

H461BUILDPDU(preInvokeResponse)
 if (appID > -1)
    SetApplicationStatus(m_applications[appID], pdu);
}

H461BUILDPDU(invokeRequest)
    if (appID > -1) {
        pdu.SetTag(msg_invokeRequest::e_applicationId);
        H225_GenericIdentifier & id = pdu;
        SetGenericIdentifier(m_applications[appID].id,id);
    }
}

H461BUILDPDU(invokeResponse)
    if (!approved) {
        pdu.SetTag(msg_invokeResponse::e_declined);
        H461_InvokeFailReason & reason = pdu;
        reason.SetTag(H461_InvokeFailReason::e_declined);
    } else {
        pdu.SetTag(msg_invokeResponse::e_approved);
        H461_ApplicationInvoke & invoke = pdu;
        SetGenericIdentifier(m_applications[appID].id,invoke.m_applicationId);
        H323SetGUIDString(invokeToken,invoke.m_invokeToken);
        invoke.m_aliasAddress.SetSize(1);
        H323SetAliasAddress(aliasAddress, invoke.m_aliasAddress[0]);
    }
}

H461BUILDPDU(invoke)
     SetGenericIdentifier(m_applications[appID].id,pdu.m_applicationId);
     H323SetGUIDString(invokeToken,pdu.m_invokeToken);
     pdu.m_aliasAddress.SetSize(1);
     H323SetAliasAddress(aliasAddress, pdu.m_aliasAddress[0]);
}

H461BUILDPDU(invokeStartList)
    pdu.SetSize(1);
    SetGenericIdentifier(m_applications[appID].id,pdu[0].m_applicationId);
    H323SetGUIDString(invokeToken,pdu[0].m_invokeToken);
}

H461BUILDPDU(invokeNotify)
    SetApplicationStatus(m_applications[appID],pdu);
}

H461BUILDPDU(stopRequest)
    pdu.SetSize(1);
    SetGenericIdentifier(m_applications[appID].id,pdu[0].m_applicationId);
}

H461BUILDPDU(stopNotify)
    pdu.SetSize(1);
    SetGenericIdentifier(m_applications[appID].id,pdu[0].m_applicationId);
}

H461BUILDPDU(callRelease)
    // Nothing to do
}

H461RECEIVEDPDU_HEAD(associateRequest)
    long ttl = pdu.m_timeToLive;
    invokeToken = PString(ttl);
H461RECEIVEDPDU_TAIL(associateRequest)


H461RECEIVEDPDU_HEAD(associateResponse)
    Associate associate;
    associate.token = H323GetGUIDString(pdu.m_associateToken);
    associate.expire = PTimer::Tick().GetSeconds() + pdu.m_timeToLive.GetValue();
    associate.alias = connection->GetRemotePartyAliases()[0];
    associate.displayName = connection->GetRemotePartyName();
    assocID = H461ASSIGN<AssociateMap>(m_associates);
    m_associates.insert(AssociateMap::value_type(assocID,associate));
    OnUpdateAssociate(assocID, associate, true);
H461RECEIVEDPDU_TAIL(associateResponse)


H461RECEIVEDPDU_HEAD(statusRequest)
    // nothing to do
H461RECEIVEDPDU_TAIL(statusRequest)


H461RECEIVEDPDU_HEAD(statusResponse)
    UpdateApplications(assocID, pdu);
H461RECEIVEDPDU_TAIL(statusResponse)


H461RECEIVEDPDU_HEAD(listRequest)
    callid = connection->GetCallIdentifier().AsString();
    callID = Call_Initiate(callid);
    H461DataStore::ApplicationList remote;
    GetApplicationStatus(pdu,remote);
    MergeCallApplicationList(remote, m_applications, m_callapplications[callID].applications);
    remote.clear();
H461RECEIVEDPDU_TAIL(listRequest)


H461RECEIVEDPDU_HEAD(listResponse)
    H461DataStore::ApplicationList remote;
    GetApplicationStatus(pdu,remote);
    MergeCallApplicationList(remote, m_applications, m_callapplications[callID].applications);
    remote.clear();
H461RECEIVEDPDU_TAIL(listResponse)


H461RECEIVEDPDU_HEAD(callApplist)
    callID = SetApplicationAvailable(assocID, callid, pdu);
H461RECEIVEDPDU_TAIL(callApplist)


H461RECEIVEDPDU_HEAD(preInvokeRequest)
    appID = FindApplicationID(GetGenericIdentifier(pdu.m_applicationId));
H461RECEIVEDPDU_TAIL(preInvokeRequest)


H461RECEIVEDPDU_HEAD(preInvokeResponse)
    UpdateApplication(assocID, pdu);
H461RECEIVEDPDU_TAIL(preInvokeResponse)


H461RECEIVEDPDU_HEAD(invokeRequest)
    if (pdu.GetTag() == msg_invokeRequest::e_applicationId) {
        const H225_GenericIdentifier & id = pdu;
        appID = FindApplicationID(GetGenericIdentifier(id));
    }
H461RECEIVEDPDU_TAIL(invokeRequest)


H461RECEIVEDPDU_HEAD(invokeResponse)
    if (pdu.GetTag() == msg_invokeResponse::e_declined) {
        approved = false;
        const H461_InvokeFailReason & reason = pdu;
        invokeToken = reason.GetTag();
    } else {
        approved = true;
        const H461_ApplicationInvoke & invoke = pdu;
        appID = FindApplicationID(GetGenericIdentifier(invoke.m_applicationId));
        invokeToken = H323GetGUIDString(invoke.m_invokeToken);
        if (invoke.m_aliasAddress.GetSize() > 0)
            aliasAddress = H323GetAliasAddressString(invoke.m_aliasAddress[0]);
    }
H461RECEIVEDPDU_TAIL(invokeResponse)


H461RECEIVEDPDU_HEAD(invoke)
     appID = FindApplicationID(GetGenericIdentifier(pdu.m_applicationId));
     invokeToken = H323GetGUIDString(pdu.m_invokeToken);
     if (pdu.m_aliasAddress.GetSize() > 0)
        aliasAddress = H323GetAliasAddressString(pdu.m_aliasAddress[0]);
H461RECEIVEDPDU_TAIL(invoke)


H461RECEIVEDPDU_HEAD(invokeStartList)
    if (pdu.GetSize() > 0) {
        appID = FindApplicationID(GetGenericIdentifier(pdu[0].m_applicationId));
        invokeToken = H323GetGUIDString(pdu[0].m_invokeToken);
    }
H461RECEIVEDPDU_TAIL(invokeStartList)


H461RECEIVEDPDU_HEAD(invokeNotify)
    UpdateApplication(assocID, pdu);
H461RECEIVEDPDU_TAIL(invokeNotify)


H461RECEIVEDPDU_HEAD(stopRequest)
if (pdu.GetSize() > 0)
    appID = FindApplicationID(GetGenericIdentifier(pdu[0].m_applicationId));
H461RECEIVEDPDU_TAIL(stopRequest)


H461RECEIVEDPDU_HEAD(stopNotify)
if (pdu.GetSize() > 0)
    appID = FindApplicationID(GetGenericIdentifier(pdu[0].m_applicationId));
H461RECEIVEDPDU_TAIL(stopNotify)


H461RECEIVEDPDU_HEAD(callRelease)
// TODO: Release all current calls
H461RECEIVEDPDU_TAIL(callRelease)

#endif


