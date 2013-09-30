/* h461_base.h
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

#ifndef H_H460_FeatureX1
#define H_H460_FeatureX1

#include <h460/h4601.h>
#include <h460/h4610.h>

// Must call the following
#include <ptlib/plugin.h>
#include <map>
#include <list>

#if _MSC_VER
#pragma once
#endif 

/////////////////////////////////////////////////////////////////////////

class H323EndPoint;
class H323Connection;
class H461Handler;
class H461DataStore;
class H460_FeatureX1 : public H460_FeatureOID 
{
    PCLASSINFO(H460_FeatureX1, H460_FeatureOID);

public:

    H460_FeatureX1();
    virtual ~H460_FeatureX1();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);
    virtual void AttachConnection(H323Connection * _con);

    static PStringArray GetFeatureName() { return PStringArray("X1"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("ASSET"); };
    static int GetPurpose()    { return FeatureSignal; };
    static PStringArray GetIdentifier();

    virtual PBoolean FeatureAdvertised(int mtype);
    virtual PBoolean CommonFeature();  // Remove this feature if required.

    // Messages
    virtual PBoolean OnSendSetup_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendCallConnect_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveCallConnect_UUIE(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendFacility_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveFacility_UUIE(const H225_FeatureDescriptor & pdu);

    // internal
    int GetMode();
    H323Connection * GetConnection();
    H461DataStore * GetDataStore();
 
private:
    H323EndPoint   * m_ep;
    H323Connection * m_con;
    H461Handler    * m_handler;
    int              m_endpointmode;
    PBoolean         m_enabled;
    PBoolean         m_supported;

};

// Need to declare for Factory Loader
#ifndef _WIN32_WCE
    #if PTLIB_VER > 260
       PPLUGIN_STATIC_LOAD(X1, H460_Feature);
    #else
       PWLIB_STATIC_LOAD_PLUGIN(X1, H460_Feature);
    #endif
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Typedef
typedef H461_AssociateRequest           msg_associateRequest;
typedef H461_AssociateResponse          msg_associateResponse;
typedef PASN_Null                       msg_statusRequest;
typedef H461_ArrayOf_ApplicationStatus  msg_statusResponse;
typedef H461_ArrayOf_ApplicationStatus  msg_listRequest;
typedef H461_ArrayOf_ApplicationStatus  msg_listResponse;
typedef H461_ArrayOf_ApplicationAvailable   msg_callApplist;
typedef H461_Application                msg_preInvokeRequest;
typedef H461_ApplicationStatus          msg_preInvokeResponse;    
typedef H461_ApplicationInvokeRequest   msg_invokeRequest;
typedef H461_ApplicationInvokeResponse  msg_invokeResponse;
typedef H461_ApplicationInvoke          msg_invoke;
typedef H461_ArrayOf_ApplicationStart   msg_invokeStartList;
typedef H461_ApplicationStatus          msg_invokeNotify;
typedef H461_ArrayOf_Application        msg_stopRequest;
typedef H461_ArrayOf_Application        msg_stopNotify;
typedef PASN_Null                       msg_callRelease;


#define H461MESSAGES(name) \
void Build_##name(H323Connection* connection, const PString & token, PString & callid, int appID, \
                  const PString & invokeToken, const PString & aliasAddress, bool approved, msg_##name & pdu); \
PBoolean Received_##name(H323Connection* connection, const PString & token, PString & callid, const msg_##name & pdu);

struct mapsort
{
    bool operator()(int s1, int s2) const {
        return (s1 > s2);
    }
};


class H461DataStore : public PObject
{
public:

    H461DataStore();

    struct Associate {
        PString              alias;
        PString              displayName;
        PString              token;
        long                 expire;
    };
    typedef std::map<int, Associate, mapsort> AssociateMap;

    enum ApplicationStates {
        e_stateAvailable,
        e_stateUnAvailable,
        e_stateInUse,
        e_associated,
        e_fail
    };

    class Application {
    public:
        Application();
        int                  associate;
        PString              id;
        PString              displayName;
        PString              avatar;
        ApplicationStates    state;
        PBoolean             availableForCall;
        long                 nextUpdate;
    };
    typedef std::list<Application> ApplicationList;
    typedef std::map<int, Application, mapsort> ApplicationMap;


    class CallData {
    public:
        CallData();
        PString callId;
        int associate;  // only used by ASSETS.
        PString remote;
        std::list<int> applications;
    };
    typedef std::map<int, CallData> CallApplicationMap;
    typedef std::map<int, std::list<PString> >  AssociateCallMap;

    enum DataType {
        e_associate,
        e_application
    };

    enum Messages {
        e_id_associateRequest,
        e_id_associateResponse,
        e_id_statusRequest,
        e_id_statusResponse,
        e_id_listRequest,
        e_id_listResponse,
        e_id_callApplist,
        e_id_preInvokeRequest,
        e_id_preInvokeResponse,
        e_id_invokeRequest,
        e_id_invokeResponse,
        e_id_invoke,
        e_id_invokeStartList,
        e_id_invokeNotify,
        e_id_stopRequest,
        e_id_stopNotify,
        e_id_callRelease
    };
    PString MessageAsString(Messages msg);
    

    // UI Functions
    void SetAssociateTTL(long newTTL) { m_timeToLive = newTTL; }
    long GetAssociateTTL() { return m_timeToLive; }

    void SetStatusInterval(long interval)  { m_statusInterval = interval; }
    PInt64 GetStatusInterval()  { return m_statusInterval; }

    void LoadDataStoreAssociate(int id, const PString & alias, const PString & displayName, const PString & token, PInt64 expire, PBoolean alert = false);
    void LoadDataStoreApplication(int id, int associate, const PString & appID, const PString & displayName, const PString & avatar, int state, PBoolean availableForCall);

    virtual void StartApplication(const PString & callToken, int i);
    virtual void StartApplication(int callid, int i, bool remote = false);
    virtual void StartASSETApplication(int callid, int i);


    virtual void NewAssociateCall(const PString & address) {}
    virtual void MakeAssociateCall(Messages msg, int assocID=-1, int callID=-1, int appID=-1,
        const PString & invokeToken = PString(), const PString & aliasAddress = PString(), bool m_approved = false) {}

    virtual void MakeApplicationCall(int id, const PString & callid, const PString & invokeToken,  const PString & aliasAddress) {}

    void UpdateAssociateCall(int callid, PBoolean startCall);
    void UpdateAssociateCall(const PString & call, PBoolean startCall);

    void GetCallApplicationList(const PString & call);
    void GetApplicationCallList(int appid);

    // Process EVENTS
    virtual void SendFacility(const PString & callid, Messages id, int appid = -1, const PString & invokeToken = PString(), 
                              const PString & aliasAddress = PString(), bool approved = false) {}

    // DB EVENTS
    virtual void OnUpdateAssociate(int id, const Associate & data, bool newEntry) {}
    virtual void OnUpdateApplication(int id, const Application & data, bool newEntry = false) {}

    //////////////////////////////////////////////////////////////////////////////////////
    // UI Events

    PBoolean OnAssociateRequest(const PString & alias, const PString & display) { return true; }
    virtual void OnAssociateCallStart(int i) {}
    virtual void OnAssociateCallEnd(int i) {}   
    virtual PBoolean OnInvokeRequest(const PString & alias, const PString & display, int i) { return true; }
    virtual void OnCallApplicationList(int id, const PString & display, const PString & avatar, PBoolean available) {}
    virtual void OnApplicationCallList(int appid, int callid, const PString & remote) {}

    virtual PBoolean OnReceiveIE(Messages id, H323Connection* connection, int assoc, int callid, int appid, const PString & invokeToken, const PString & aliasAddress, bool approval);
    virtual void OnBuildIE(Messages id, H323Connection* connection, int assoc, int callid, int appid, const PString & invokeToken, const PString & aliasAddress, bool approval);


    //////////////////////////////////////////////////////////////////////////////////////
    // Messaging

    H461MESSAGES(associateRequest)
    H461MESSAGES(associateResponse)
    H461MESSAGES(statusRequest)
    H461MESSAGES(statusResponse)
    H461MESSAGES(listRequest)
    H461MESSAGES(listResponse)
    H461MESSAGES(callApplist)
    H461MESSAGES(preInvokeRequest)
    H461MESSAGES(preInvokeResponse)  
    H461MESSAGES(invokeRequest)
    H461MESSAGES(invokeResponse)
    H461MESSAGES(invoke)
    H461MESSAGES(invokeStartList)
    H461MESSAGES(invokeNotify)
    H461MESSAGES(stopRequest)
    H461MESSAGES(stopNotify)
    H461MESSAGES(callRelease)

protected:
    // 
    int Call_Initiate(const PString & callid, int assoc = -1);
    void Call_Terminate(int callID);

    // utilities
    int FindAssociateID(const PString & token);
    void FindAssociateAddress(int id, PString & address, PString & token);
    int FindApplicationID(const PString & appID);
    int FindApplicationID(const PString & token, const PString & appID);
    int FindApplicationID(int assoc, const PString & appID);
    int FindCallData(const PString & callIdentifier);
    PString FindCall(int id);
    PBoolean FindApplications(DataType type, const PString & key, std::list<int> & id);

    void BuildApplicationAvailable(int assocID, int callID, const PString & callidentifier, H461_ArrayOf_ApplicationAvailable & pdu);
    int SetApplicationAvailable(int assocID, const PString & callidentifier, const H461_ArrayOf_ApplicationAvailable & pdu);

    bool FindCallAssociates(int callID, std::list<int> & assoc);

    void UpdateApplication(int assoc, const H461_ApplicationStatus & applications);
    void UpdateApplications(int assoc, const H461_ArrayOf_ApplicationStatus & applications);

    void InvokeApplication(int callid, int i, const PString & invokeToken, const PString & aliasAddress, bool approval = true);

    void SendInternalFacility(H323Connection * connection, int id);

    int CreateNewAssociation();
    int FindValidAssociate(const PString & alias);
    PString GenerateApplicationToken();

    AssociateMap        m_associates;
    ApplicationMap      m_applications;
    CallApplicationMap  m_callapplications;
    AssociateCallMap   m_associateCalls;

private:
    long m_timeToLive;
    long m_statusInterval;
};

/////////////////////////////////////////////////////////////

class H461AssetPDU;
class H461Handler : public PObject
{
    PCLASSINFO(H461Handler,PObject)

public:
    H461Handler(H460_FeatureX1 & feature);

    // Feature Messages
    PBoolean BuildPDU(PASN_OctetString & pdu);
    PBoolean HandlePDU(const PASN_OctetString & msg);

    PBoolean ProcessPDU(H461_ASSETPDU & pdu);
    PBoolean ProcessMessage(H461AssetPDU & msg);

    // internal
    H323Connection * GetConnection() { return m_connection; }
    int              GetMode() { return m_mode; }
    H461DataStore  * GetDataStore() { return m_dataStore; }


private:
    H460_FeatureX1 &        m_feature;
    H323Connection *        m_connection;
    int                     m_mode;

    H461DataStore *         m_dataStore;
};


/////////////////////////////////////////////////////////////

// Typedef
typedef H461_AssociateRequest           msg_associateRequest;
typedef H461_AssociateResponse          msg_associateResponse;
typedef PASN_Null                       msg_statusRequest;
typedef H461_ArrayOf_ApplicationStatus  msg_statusResponse;
typedef H461_ArrayOf_ApplicationStatus  msg_listRequest;
typedef H461_ArrayOf_ApplicationStatus  msg_listResponse;
typedef H461_ArrayOf_ApplicationAvailable   msg_callApplist;
typedef H461_Application                msg_preInvokeRequest;
typedef H461_ApplicationStatus          msg_preInvokeResponse;    
typedef H461_ApplicationInvokeRequest   msg_invokeRequest;
typedef H461_ApplicationInvokeResponse  msg_invokeResponse;
typedef H461_ApplicationInvoke          msg_invoke;
typedef H461_ArrayOf_ApplicationStart   msg_invokeStartList;
typedef H461_ApplicationStatus          msg_invokeNotify;
typedef H461_ArrayOf_Application        msg_stopRequest;
typedef H461_ArrayOf_Application        msg_stopNotify;
typedef PASN_Null                       msg_callRelease;

#define H461ELEMENT(name) \
PBoolean Handle_##name(H323Connection* connection); \
H461_ASSETMessage & Build_##name(H323Connection* connection);

class H461AssetPDU : public H461_ASSETMessage
{
    PCLASSINFO(H461AssetPDU, H461_ASSETMessage)

public:
    H461AssetPDU();
    H461AssetPDU(H461_ASSETMessage & msg);
    ~H461AssetPDU();

    PBoolean ProcessMessage(H461Handler * handler);
    H461_ASSETMessage & BuildMessage(H461Handler * handler, PBoolean & success);

protected:
    PBoolean ValidateToken(const PString & token);
    PBoolean BuildParameters(H323Connection* connection);

    H461ELEMENT(associateRequest);
    H461ELEMENT(associateResponse);
    H461ELEMENT(statusRequest);
    H461ELEMENT(statusResponse);
    H461ELEMENT(listRequest);
    H461ELEMENT(listResponse);
    H461ELEMENT(callApplist);
    H461ELEMENT(preInvokeRequest);
    H461ELEMENT(preInvokeResponse);
    H461ELEMENT(invokeRequest);
    H461ELEMENT(invokeResponse);
    H461ELEMENT(invoke);
    H461ELEMENT(invokeStartList);
    H461ELEMENT(invokeNotify);
    H461ELEMENT(stopRequest);
    H461ELEMENT(stopNotify);
    H461ELEMENT(callRelease);

private:
    H461Handler   * m_handler;
    H323Connection* m_connection;
    H461DataStore * m_dataStore;

    PString         m_token; 
    PString         m_callid;
    int             m_appID;
    PString         m_invokeToken;
    PString         m_aliasAddress;
    bool            m_approved;
};

#endif
