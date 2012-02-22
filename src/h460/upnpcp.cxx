/*
 * upnpcp.cxx
 *
 * UPnP NAT Traversal class.
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
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 *
 */


#include "ptlib.h"
#include "h323.h"

#ifdef H323_UPnP
#include "h460/upnpcp.h"

#ifdef H323_H46019M
#include "h460/h46018_h225.h"
#endif

#include <Natupnp.h>
#include <UPnP.h>
#include <map>

#include <ptclib/delaychan.h>

/////////////////////////////////////////////////////////////////////
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

#define UPnPUDPBasePort 55000
#define UPnPTCPBasePort 55000

// Utilities

template <class PAIR>
class deletepair { // PAIR::second_type is a pointer type
public:
    void operator()(const PAIR & p) { delete p.second; }
};

template <class M>
inline void DeleteObjectsInMap(const M & m)
{
    typedef typename M::value_type PAIR;
    std::for_each(m.begin(), m.end(), deletepair<PAIR>());
}

////////////////////////////////////////////////////////////////////

class DeviceInformationContainer
{
public:
    DeviceInformationContainer() : Children(""), Description(""),
        FriendlyName(""), HasChildren(""), IconURL(""), IsRootDevice(""), 
        ManufacturerName(""), ManufacturerURL(""), ModelName(""), 
        ModelNumber(""), ModelURL(""), ParentDevice(""), 
        PresentationURL(""), RootDevice(""), SerialNumber(""), 
        Services(""), Type(""), UniqueDeviceName(""), UPC("")   { };
    
    // see http://msdn.microsoft.com/library/en-us/upnp/upnp/iupnpdevice.asp
    
    PString Children;            // Child devices of the device. 
    PString Description;        // Human-readable form of the summary of a device's functionality. 
    PString FriendlyName;        // Device display name. 
    PString HasChildren;        // Indicates whether the device has any child devices. 
    PString IconURL;            // URL of icon
    PString IsRootDevice;        // Indicates whether the device is the top-most device in the device tree. 
    PString ManufacturerName;    // Human-readable form of the manufacturer name. 
    PString ManufacturerURL;    // URL for the manufacturer's Web site. 
    PString ModelName;            // Human-readable form of the model name. 
    PString ModelNumber;        // Human-readable form of the model number. 
    PString ModelURL;            // URL for a Web page that contains model-specific information. 
    PString ParentDevice;        // Parent of the device. 
    PString PresentationURL;    // Presentation URL for a Web page that can be used to control the device. 
    PString RootDevice;            // Top-most device in the device tree. 
    PString SerialNumber;        // Human-readable form of the serial number. 
    PString Services;            // List of services provided by the device. 
    PString Type;                // Uniform resource identifier (URI) for the device type. 
    PString UniqueDeviceName;    // Unique device name (UDN) of the device. 
    PString UPC;                // Human-readable form of the product code. 
};

//////////////////////////////////////////////////////////////////////

class PortMappingContainer : public PObject
{
public:
    PortMappingContainer() : ExternalIPAddress(""), ExternalPort(0), 
        InternalPort(0), Protocol("UDP"), InternalClient(""), 
        Enabled(true), Description("")   { };

    PObject * Clone() const;

    PString ExternalIPAddress;
    WORD ExternalPort;
    WORD InternalPort;
    PString Protocol;
    PString InternalClient;    
    bool Enabled;    
    PString Description;
};

PObject * PortMappingContainer::Clone() const
{
   return new PortMappingContainer(*this);
}

/////////////////////////////////////////////////////////////////////

class UPnPThread;
class UPnPCallbacks : public PObject  
{
public:
    UPnPCallbacks(UPnPThread * upnp);

    virtual HRESULT OnNewNumberOfEntries(long lNewNumberOfEntries);
    virtual HRESULT OnNewExternalIPAddress( const PString & extIPAddress);
    
protected:
    UPnPThread * m_upnp;
};

interface IxNATExternalIPAddressCallback : public INATExternalIPAddressCallback
{
    IxNATExternalIPAddressCallback(UPnPCallbacks* p ) : m_pointer( p ), m_dwRef( 0 ) { };

    ~IxNATExternalIPAddressCallback() { /*delete m_pointer;*/ }
    
    HRESULT STDMETHODCALLTYPE NewExternalIPAddress( BSTR bstrNewExternalIPAddress ) {
        PAssert(m_pointer != NULL,PLogicError);            
        return    m_pointer->OnNewExternalIPAddress(bstrNewExternalIPAddress);
    }
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject);
    
    ULONG STDMETHODCALLTYPE AddRef()  {    return ++m_dwRef; }
    
    ULONG STDMETHODCALLTYPE Release()  {
        if ( --m_dwRef == 0 )
            delete this;
        return m_dwRef;
    }
    DWORD        m_dwRef;
    UPnPCallbacks*    m_pointer;
};

HRESULT STDMETHODCALLTYPE IxNATExternalIPAddressCallback::QueryInterface(REFIID iid, void ** ppvObject)
{
    HRESULT hr = S_OK;
    *ppvObject = NULL;
    
    if ( iid == IID_IUnknown ||    iid == IID_INATExternalIPAddressCallback ) {
        *ppvObject = this;
        AddRef();
        hr = NOERROR;
    } else {
        hr = E_NOINTERFACE;
    }
    return hr;
}

/////////////////////////////////////////////////////////////////////////////////////////
    
interface IxNATNumberOfEntriesCallback : public INATNumberOfEntriesCallback
{
    IxNATNumberOfEntriesCallback(UPnPCallbacks* p ) : m_pointer( p ), m_dwRef( 0 ) { };

    ~IxNATNumberOfEntriesCallback() { /*delete m_pointer;*/ }
    
    HRESULT STDMETHODCALLTYPE NewNumberOfEntries( long lNewNumberOfEntries ) {
        PAssert(m_pointer != NULL,PLogicError);            
        return m_pointer->OnNewNumberOfEntries( lNewNumberOfEntries );
    }
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** ppvObject); 
    
    ULONG STDMETHODCALLTYPE AddRef()  { return ++m_dwRef; }
    
    ULONG STDMETHODCALLTYPE Release() {
        if ( --m_dwRef == 0 )
            delete this;
        return m_dwRef;
    }
    
    DWORD        m_dwRef;
    UPnPCallbacks*    m_pointer;
};

HRESULT STDMETHODCALLTYPE IxNATNumberOfEntriesCallback::QueryInterface(REFIID iid, void ** ppvObject)
{
    HRESULT hr = S_OK;
    *ppvObject = NULL;
    
    if ( iid == IID_IUnknown ||    iid == IID_INATNumberOfEntriesCallback ) {
        *ppvObject = this;
        AddRef();
        hr = NOERROR;
    } else {
        hr = E_NOINTERFACE;
    }
    return hr;
}

////////////////////////////////////////////////////////////////////

class UPnPThread : public PThread
{
 public:
    UPnPThread(PNatMethod_UPnP * nat);
    ~UPnPThread();

    void Main();

    bool CreateMap(bool pair, const PString & protocol, const PIPSocket::Address & localIP, 
                const WORD & locPort, PIPSocket::Address & extIP , WORD & extPort, PBoolean force = false);

    bool RemoveMap(WORD port, PBoolean udp = true);

    void SetExtIPAddress(const PString & newAddr);
    void SetMappingEnum();

    PBoolean IsMappingFree(WORD askPort, PBoolean udp);

    WORD GetNextFreePort(WORD askPort, bool pair, PBoolean udp = true);

    void Shutdown();

protected:
    bool Initialise();
    bool DetectDevice(PStringList & devNames);
    bool PopulateDeviceInfoContainer( IUPnPDevice* piDevice, 
                    DeviceInformationContainer& deviceInfo);

    bool AddMapping(PortMappingContainer& newMapping);  
    bool RemoveMapping(PortMappingContainer& newMapping);

    void Close();
    bool EnumMaps();

    bool TestMapping();

 private:
    IUPnPNAT*                        m_piNAT;                    
    INATEventManager*                m_piEventManager;

    map<PString,PortMappingContainer *> m_piMaps;
    map<PString,PortMappingContainer *> m_piUPnPMaps;

    PNatMethod_UPnP*                  m_piNatMethod;                        
    UPnPCallbacks*                    m_piCallbacks;

    PMutex                            m_MapMutex;
    PSyncPoint                        m_ThreadSync;

    bool m_piNewMapping;
    bool m_piShutdown;

    PAdaptiveDelay                    m_PortOpenPace;
};

UPnPThread::UPnPThread(PNatMethod_UPnP * nat)
   : PThread(1000,NoAutoDeleteThread, NormalPriority, "UPnP Thread"), m_piNatMethod(nat)
{
    m_piNAT = NULL;                    
    m_piEventManager = NULL;
    m_piCallbacks = NULL;
    m_piNewMapping = false;
    m_piShutdown = false;

    Resume();
}

bool UPnPThread::Initialise()
{
    PTRACE(4,"UPnP\tInitialising UPnP");

    m_piCallbacks =    new UPnPCallbacks(this);

    CoUninitialize();                       // Unitialise MTA
    HRESULT result = CoInitialize(NULL);   // Initialise STA
    if (FAILED(result)) {
        PTRACE(4,"UPnP\tInitialization Failure");
        return false;
    }

    result = CoCreateInstance(__uuidof(UPnPNAT), NULL, CLSCTX_ALL, __uuidof(IUPnPNAT), (void **)&m_piNAT);
    if (FAILED(result)) {
        PTRACE(4,"UPnP\tError initialising UPnP Instance");
        return false;
    }
    
    result = m_piNAT->get_NATEventManager(&m_piEventManager);
    if (FAILED(result)) {
        PTRACE(4,"UPnP\tError UPnP EventManager..");
        SAFE_RELEASE(m_piNAT);
        return false;
    }
    
    if (m_piEventManager) {
        result = m_piEventManager->put_ExternalIPAddressCallback(new IxNATExternalIPAddressCallback(m_piCallbacks));
        if (FAILED(result)) {
            SAFE_RELEASE(m_piEventManager);
            SAFE_RELEASE(m_piNAT);
            return false;
        }
        
        result = m_piEventManager->put_NumberOfEntriesCallback(new IxNATNumberOfEntriesCallback(m_piCallbacks));
        if (FAILED(result)) {
            SAFE_RELEASE(m_piEventManager);
            SAFE_RELEASE(m_piNAT);
            return false;
        }
    } else {
        PTRACE(4,"UPnP\tUPnP EventManager unavailable");
        return false;
    }

    return true;

}

UPnPThread::~UPnPThread()
{
    Shutdown();
}

void UPnPThread::Main()
{
    if (!Initialise()) {
        PTRACE(4,"UPnP\tError in Initialising Method Aborted!");
        return;
    }

    PStringList IGDdevices;
    if (DetectDevice(IGDdevices) && TestMapping()) {
        m_piNatMethod->OnUPnPAvailable(IGDdevices[0]);   
        while (!m_piShutdown) {
            if (m_piNewMapping)
                EnumMaps();

            m_ThreadSync.Wait(200);
        }
    }
    Close();
}

void UPnPThread::Shutdown() 
{
    m_piShutdown = true;
    m_ThreadSync.Signal();
}

void UPnPThread::Close()
{
    for each(pair<PString,PortMappingContainer*> c in m_piMaps) 
       RemoveMapping(*c.second);

    DeleteObjectsInMap(m_piMaps);
    DeleteObjectsInMap(m_piUPnPMaps);

    SAFE_RELEASE(m_piEventManager);
    SAFE_RELEASE(m_piNAT);

    delete m_piCallbacks;
}

bool UPnPThread::RemoveMap(WORD port, PBoolean udp)
{
    PWaitAndSignal m(m_MapMutex);

    PString key = PString((udp) ? "U" : "T") + PString(port);
        map<PString,PortMappingContainer*>::iterator it = m_piMaps.find(key);
        if (it != m_piMaps.end()) {
            RemoveMapping(*it->second);
            m_piMaps.erase(it);
        }

    return true;
}

PBoolean UPnPThread::IsMappingFree(WORD askPort, PBoolean udp)
{
   PString key = PString((udp) ? "U" : "T") + PString(askPort);
   if (m_piMaps.find(key) ==  m_piMaps.end())
       return true;
   else
       return false;
}

WORD UPnPThread::GetNextFreePort(WORD askPort, bool pair, PBoolean udp)
{
    WORD port=0;
    if (askPort > 0) 
        port = askPort;
    else
        port = (udp) ? UPnPUDPBasePort : UPnPTCPBasePort; 

    bool found = false;
    // find a free port
    while (!found) {
        PString key = PString((udp) ? "U" : "T") + PString(port);
        if ((m_piMaps.find(key) ==  m_piMaps.end()) && 
                    (m_piUPnPMaps.find(key) == m_piUPnPMaps.end())) {
            if (pair) {
                PString key2 = PString((udp) ? "U" : "T") + PString(port+1);
                if ((m_piMaps.find(key2) == m_piMaps.end()) && 
                    (m_piUPnPMaps.find(key2)== m_piUPnPMaps.end())) {
                        found = true;
                } else
                    port++;
            } else {
                found = true;
            }
        }
        if (!found)
            port++;
    }
    return port;
}

bool UPnPThread::CreateMap(bool pair, const PString & protocol, 
                           const PIPSocket::Address & localIP, const WORD & locPort, 
                           PIPSocket::Address & extIP , WORD & extPort, PBoolean force)
{
    PWaitAndSignal m(m_MapMutex);

    PINDEX size = 1;
    if (pair) size = 2;
    PBoolean udp = true;
    if (protocol == "TCP") udp = false;

    if (extPort > 0)
       RemoveMap(extPort, false);

    extPort = locPort;
    
    bool success = false;
    bool exit = false;
    int loop=0;
    int loopCount = force ? 10 : 1;

    PortMappingContainer umap;
    while (!success && !exit ) {
        for (WORD i=0; i < size; i++) {
            umap.Protocol = protocol;
            umap.InternalClient = localIP.AsString();
            umap.InternalPort = locPort + i;
            umap.Description = "h323plus";
            umap.ExternalPort = extPort + i;

            if (AddMapping(umap)) {
                PTRACE(4,"UPnP\tCreated map " << protocol << " " << umap.InternalClient << ":" << umap.InternalPort 
                                                    << " to " << umap.ExternalIPAddress << ":" << umap.ExternalPort);

                PString key = PString((protocol == "UDP") ? "U" : "T") + PString(umap.ExternalPort);
                m_piMaps.insert(std::pair<PString,PortMappingContainer*>(key,(PortMappingContainer*)umap.Clone()));
                   if (i == 0) {
                        extIP = umap.ExternalIPAddress;
                        extPort = umap.ExternalPort;
                   }
                success = true;
            }
        }
        if (success)
          continue;

        loop++;
        PTRACE(4,"UPnP\tMapping Failure: " << protocol << " " << umap.InternalClient << ":" << umap.InternalPort
                                           << " to " << umap.ExternalIPAddress << ":" << umap.ExternalPort << " retrying....");
        extPort = extPort+10;   // Always set 10 just in case there is another device on a call.
        exit = (loop == loopCount);
    }

    return success;
}
    
bool GetNextMapping(IEnumVARIANT* piEnumerator, PortMappingContainer& mappingContainer)
{
    // uses the enumerator to get the next mapping and fill in a mapping container structure

    if (!piEnumerator)
        return false;

    VARIANT varCurMapping;
    VariantInit(&varCurMapping);
    HRESULT result = piEnumerator->Next( 1, &varCurMapping, NULL);
    if (FAILED(result) || varCurMapping.vt == VT_EMPTY)
        return false;

    IStaticPortMapping* piMapping = NULL;
    IDispatch* piDispMap = V_DISPATCH(&varCurMapping);
    result = piDispMap->QueryInterface(IID_IStaticPortMapping, (void **)&piMapping);
    if (FAILED(result))
        return false;

    // get external address
    BSTR bStr = NULL;
    result = piMapping->get_ExternalIPAddress(&bStr);
    if (FAILED(result) || !bStr) {
        SAFE_RELEASE(piMapping);
        return false;
    }
    mappingContainer.ExternalIPAddress =  bStr;
    SysFreeString(bStr);

    // get external port
    long lValue = 0;
    result = piMapping->get_ExternalPort( &lValue );
    if (FAILED(result)) {    
        SAFE_RELEASE(piMapping);
        return false;
    }
    mappingContainer.ExternalPort = (WORD)lValue;
    
    // get internal port
    result = piMapping->get_InternalPort( &lValue );
    if (FAILED(result)) {    
        SAFE_RELEASE(piMapping);
        return false;
    }
    mappingContainer.InternalPort = (WORD)lValue;
    
    // get protocol
    result = piMapping->get_Protocol(&bStr);
    if (FAILED(result) || !bStr) {
        SAFE_RELEASE(piMapping);
        return false;
    }
    mappingContainer.Protocol = bStr;
    SysFreeString(bStr);

    // get internal client
    result = piMapping->get_InternalClient(&bStr);
    if (FAILED(result) || !bStr) {
        SAFE_RELEASE(piMapping);
        return false;
    }
    mappingContainer.InternalClient =  bStr;
    SysFreeString(bStr);

    // determine whether it's enabled
    VARIANT_BOOL bValue = VARIANT_FALSE;
    result = piMapping->get_Enabled(&bValue);
    if (FAILED(result)) {    
        SAFE_RELEASE(piMapping);
        return false;
    }
    mappingContainer.Enabled = (bValue==VARIANT_FALSE) ? false : true;
    
    // get description
    result = piMapping->get_Description( &bStr );
    if (FAILED(result) || !bStr) {
        SAFE_RELEASE(piMapping);
        return false;
    }
    mappingContainer.Description = bStr;
    SysFreeString(bStr);
    
    SAFE_RELEASE(piMapping);
    VariantClear(&varCurMapping);
    
    return true;
    
}

bool UPnPThread::EnumMaps()
{
    PWaitAndSignal m(m_MapMutex);

    IStaticPortMappingCollection* piPortMappingCollection = NULL;    
    if (FAILED(m_piNAT->get_StaticPortMappingCollection(&piPortMappingCollection) ) 
        || (piPortMappingCollection==NULL ) ) {
            PTRACE(4,"UPnP\tError: Could not access Static Mapping Collection!");
            return false;
    }
    
    IUnknown* piUnk = NULL;
    IEnumVARIANT* piEnumerator = NULL;
    if (FAILED(piPortMappingCollection->get__NewEnum(&piUnk)) || piUnk==NULL ) {
        PTRACE(4,"UPnP\tError: Could not access Static Mapping Collection!");
        SAFE_RELEASE(piPortMappingCollection);
        return false;
    }
    
    if (FAILED(piUnk->QueryInterface(IID_IEnumVARIANT, (void **)&piEnumerator) ) || piEnumerator==NULL ) {
        PTRACE(4,"UPnP\tError: Could not enumerate Static Mapping Collection!");
        SAFE_RELEASE(piPortMappingCollection);
        return false;
    }
 
    if (FAILED(piEnumerator->Reset())) {
        PTRACE(4,"UPnP\tError: Could not reset enumeration");
        SAFE_RELEASE(piPortMappingCollection);
        return false;
    }


    DeleteObjectsInMap(m_piUPnPMaps);

    PortMappingContainer mapCont;
    while (GetNextMapping(piEnumerator, mapCont)) {
        PString key = PString((mapCont.Protocol == "UDP") ? "U" : "T") + PString(mapCont.ExternalPort);
        m_piUPnPMaps.insert(std::pair<PString, PortMappingContainer*>(key, (PortMappingContainer*)mapCont.Clone()));
    }

    m_piNewMapping = false;
    return true;
}


bool UPnPThread::AddMapping(PortMappingContainer& newMapping)
{

    if (!m_piNAT)
        return false;

    IStaticPortMappingCollection* m_piPortMappingCollection = NULL;    
    if (FAILED(m_piNAT->get_StaticPortMappingCollection(&m_piPortMappingCollection)) || !m_piPortMappingCollection) {
        PTRACE(4,"UPnP\tError: Could not access Static Mapping Collection!");
        return false;
    }

    bool success = false;
    IStaticPortMapping* m_piStaticPortMapping = NULL;
    if (SUCCEEDED(m_piPortMappingCollection->Add(newMapping.ExternalPort, SysAllocString(newMapping.Protocol.AsUCS2()), 
        newMapping.InternalPort, SysAllocString(newMapping.InternalClient.AsUCS2()), newMapping.Enabled , SysAllocString(newMapping.Description.AsUCS2()),
        &m_piStaticPortMapping))) {
            BSTR extIP;
            m_piStaticPortMapping->get_ExternalIPAddress(&extIP);
            newMapping.ExternalIPAddress = extIP;
            PIPSocket::Address mapaddr(newMapping.ExternalIPAddress);
            success = (mapaddr.IsValid() && !mapaddr.IsLoopback() && !mapaddr.IsAny());
    } else {
        PTRACE(4,"UPnP\tError: Creating Map! " << newMapping.Protocol << " " 
                  << newMapping.InternalClient << ":" << newMapping.InternalPort  << " to " << newMapping.ExternalPort );
    }

    m_PortOpenPace.Delay(200);
    
    SAFE_RELEASE(m_piStaticPortMapping);
    SAFE_RELEASE(m_piPortMappingCollection);
        

    return success;
}

bool UPnPThread::RemoveMapping(PortMappingContainer& newMapping)
{

    if (!m_piNAT)
        return false;

    IStaticPortMappingCollection* m_piPortMappingCollection = NULL;    
    if (FAILED(m_piNAT->get_StaticPortMappingCollection(&m_piPortMappingCollection)) || !m_piPortMappingCollection) {
        PTRACE(4,"UPnP\tError: Could not access Static Mapping Collection!");
        return false;
    }

    bool success = false;
    IStaticPortMapping* m_piStaticPortMapping = NULL;
    if (SUCCEEDED(m_piPortMappingCollection->Remove(newMapping.ExternalPort, SysAllocString(newMapping.Protocol.AsUCS2())))) {
            success = true;
            PTRACE(4,"UPnP\tMap removed " << newMapping.Protocol << " " << newMapping.ExternalPort);
    }
    
    SAFE_RELEASE(m_piStaticPortMapping);
    SAFE_RELEASE(m_piPortMappingCollection);
        

    return success;
}

void UPnPThread::SetMappingEnum()
{
    m_piNewMapping = true;
    m_ThreadSync.Signal();

}


void UPnPThread::SetExtIPAddress(const PString & newAddr)
{
    m_piNatMethod->SetExtIPAddress(newAddr);
}


bool UPnPThread::PopulateDeviceInfoContainer( IUPnPDevice* piDevice, 
                DeviceInformationContainer& deviceInfo)
{

    HRESULT result=S_OK, hrReturn=S_OK;
    long lValue = 0;
    BSTR bStr = NULL;
    VARIANT_BOOL bValue = VARIANT_FALSE;
    IUPnPDevices* piDevices = NULL;
    
    result = piDevice->get_Children( &piDevices );
    hrReturn |= result;
    if ( SUCCEEDED(result) && (piDevices!=NULL) )
    {
        piDevices->get_Count( &lValue );
        if ( lValue==0 )
        {
            deviceInfo.Children = "No: Zero children";
        }
        else if ( lValue==1 )
        {
            deviceInfo.Children = "Yes: One child";
        }
        else if ( lValue>=1 )
        {
            deviceInfo.Children = "Yes: " + lValue;
        }
        SAFE_RELEASE(piDevices);
        lValue = 0;
    }
    
    // Get Description
    result = piDevice->get_Description( &bStr );
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.Description = bStr;
        SysFreeString(bStr);
    }
    
    // Get FriendlyName
    result = piDevice->get_FriendlyName( &bStr );
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.FriendlyName = bStr;    
        SysFreeString(bStr);
    }

    // Get HasChildren
    result = piDevice->get_HasChildren( &bValue );
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.HasChildren = (bValue==VARIANT_FALSE ? "No" : "Yes");
        bValue = VARIANT_FALSE;
    }

    // Get IconURL
    BSTR bStrMime = SysAllocString(L"image/gif");
    result = piDevice->IconURL( bStrMime, 32, 32, 8, &bStr );
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.IconURL = bStr;    
        SysFreeString(bStr);
    }
    
    SysFreeString(bStrMime);
    bStrMime = NULL;
    
    // Get IsRootDevice
    result = piDevice->get_IsRootDevice(&bValue);
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.IsRootDevice = (bValue==VARIANT_FALSE ? "No": "Yes");
        bValue = VARIANT_FALSE;
    }
    
    // Get ManufacturerName
    result = piDevice->get_ManufacturerName(&bStr);
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.ManufacturerName = bStr;    
        SysFreeString(bStr);
    }

    // Get ManufacturerURL
    result = piDevice->get_ManufacturerURL(&bStr);
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.ManufacturerURL = bStr;    
        SysFreeString(bStr);
    }

    // Get ModelName
    result = piDevice->get_ModelName(&bStr);
    hrReturn |= result;
    if (SUCCEEDED(result)) {
        deviceInfo.ModelName = bStr;    
        SysFreeString(bStr);
    }

    // Get ModelNumber
    result = piDevice->get_ModelNumber(&bStr);
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.ModelNumber = bStr;    
        SysFreeString(bStr);
    }

    // Get ModelURL
    result = piDevice->get_ModelURL(&bStr);
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.ModelURL = bStr;    
        SysFreeString(bStr);
    }

    IUPnPDevice* piDev = NULL;
    result = piDevice->get_ParentDevice( &piDev );
    hrReturn |= result;
    if ( SUCCEEDED(result) ){
        if (!piDev) {
            deviceInfo.ParentDevice = "This is a topmost device";
        } else {
            if (SUCCEEDED(piDev->get_FriendlyName(&bStr))) {
                deviceInfo.ParentDevice = bStr;
                SysFreeString(bStr);
            }
            SAFE_RELEASE(piDev);
        }
    }
    
    // Get PresentationURL
    result = piDevice->get_PresentationURL(&bStr);
    hrReturn |= result;
    if (SUCCEEDED(result)) {
        deviceInfo.PresentationURL =  bStr;    
        SysFreeString(bStr);
    }

    // Get RootDevice.  Actually, we will only get the FriendlyName of the root device,
    result = piDevice->get_RootDevice(&piDev);
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        if (!piDev) {
            deviceInfo.RootDevice = "This is a topmost device";
        } else {
            if (SUCCEEDED(piDev->get_FriendlyName(&bStr))) {
                deviceInfo.RootDevice = bStr;
                SysFreeString(bStr);
            }
            SAFE_RELEASE(piDev);
        }
    }

    // Get SerialNumber
    result = piDevice->get_SerialNumber(&bStr);
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.SerialNumber = bStr;    
        SysFreeString(bStr);
    }

    // Get Services.  Actually, we will NOT enumerate through all the services that are contained
    // in the IUPnPServices collection.  Rather, we will only get a count of services
    IUPnPServices* piServices = NULL;
    
    result = piDevice->get_Services(&piServices);
    hrReturn |= result;
    if (SUCCEEDED(result) && (piServices!=NULL)) {
        piServices->get_Count(&lValue);
        if (lValue==0)
            deviceInfo.Services = "No: Zero services";
        else if (lValue==1)
            deviceInfo.Services = "Yes: One service";
        else if (lValue>=1)
            deviceInfo.Services = "Yes: " + lValue;
        SAFE_RELEASE(piServices);
        lValue = 0;
    }
    
    // Get Type
    result = piDevice->get_Type(&bStr);
    hrReturn |= result;
    if (SUCCEEDED(result)) {
        deviceInfo.Type = bStr;    
        SysFreeString(bStr);
    }

    // Get UniqueDeviceName
    result = piDevice->get_UniqueDeviceName( &bStr );
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.UniqueDeviceName = bStr;    
        SysFreeString(bStr);
    }

    // Get UPC
    result = piDevice->get_UPC(&bStr);
    hrReturn |= result;
    if ( SUCCEEDED(result) ) {
        deviceInfo.UPC = bStr;    
        SysFreeString(bStr);
    }
    
    return true;
}

bool UPnPThread::DetectDevice(PStringList & devNames)
{
    
    IUPnPDeviceFinder* piDeviceFinder = NULL;
    
    if (FAILED( CoCreateInstance( CLSID_UPnPDeviceFinder, NULL, CLSCTX_ALL, 
        IID_IUPnPDeviceFinder, (void**) &piDeviceFinder ) ) || ( piDeviceFinder==NULL ) ) {
        PTRACE(4,"UPnP\tError Initialising Device Finder");
        return false;
    }
    
    BSTR bStrDev = SysAllocString( L"urn:schemas-upnp-org:device:InternetGatewayDevice:1" );
    IUPnPDevices* piFoundDevices = NULL;
    
    if (FAILED(piDeviceFinder->FindByType(bStrDev, 0, &piFoundDevices))) {
        PTRACE(4,"UPnP\tNo IGD device found!");
        SysFreeString(bStrDev);
        return false;
    }
    
    SysFreeString(bStrDev);

    if (!piFoundDevices) {
        PTRACE(4,"UPnP\tError IGD Device is NULL");
        return false;
    }
    
    HRESULT result = S_OK;    
    IUnknown * pUnk = NULL;
    DeviceInformationContainer deviceInfo;
    
    if (SUCCEEDED( piFoundDevices->get__NewEnum(&pUnk) ) && ( pUnk!=NULL ) )
    {
        IEnumVARIANT * pEnumVar = NULL;
        result = pUnk->QueryInterface(IID_IEnumVARIANT, (void **) &pEnumVar);
        if (SUCCEEDED(result))
        {
            VARIANT varCurDevice;
            VariantInit(&varCurDevice);
            pEnumVar->Reset();
            // Loop through each device in the collection
            while (S_OK == pEnumVar->Next(1, &varCurDevice, NULL))
            {
                IUPnPDevice * pDevice = NULL;
                IDispatch * pdispDevice = V_DISPATCH(&varCurDevice);
                if (SUCCEEDED(pdispDevice->QueryInterface(IID_IUPnPDevice, (void **) &pDevice)))
                {
                    // finally, post interval notification message and get all the needed information
                    result = PopulateDeviceInfoContainer(pDevice, deviceInfo);
                    PTRACE(6,"UPnP\tDevice detected " << deviceInfo.FriendlyName << "\n" 
                        << deviceInfo.ManufacturerName << " " << deviceInfo.ModelName << " " << deviceInfo.ModelNumber);
                    devNames.AppendString(deviceInfo.FriendlyName);
                }
                VariantClear(&varCurDevice);
            }
            pEnumVar->Release();
        }
        pUnk->Release();
    }
    
    SAFE_RELEASE(piDeviceFinder);

    return (devNames.GetSize() > 0);

}

PBoolean UPnPThread::TestMapping() 
{
    WORD locPort,extPort=0;
    PIPSocket::Address locAddr, extAddr;

    PTRACE(4,"UPnP\tPerforming Port Mapping Test");
 
    locAddr = PIPSocket::GetGatewayInterfaceAddress(4);
    locPort = m_piNatMethod->GetRandomPort();
    extPort = locPort;

    if (CreateMap(true,"UDP",locAddr,locPort,extAddr,extPort)) {
        m_piNatMethod->SetExtIPAddress(extAddr);
        RemoveMap(extPort,true);
        RemoveMap(extPort+1,true);
        PTRACE(4,"UPnP\tPort Mapping Test successful!");
        return true;
    }

    PTRACE(4,"UPnP\tError in Port Mapping Test ABORT!");
    return false;
}

////////////////////////////////////////////////////////////////////

UPnPCallbacks::UPnPCallbacks(UPnPThread * upnp)
: m_upnp(upnp)
{
}

HRESULT UPnPCallbacks::OnNewNumberOfEntries(long lNewNumberOfEntries)
{
    m_upnp->SetMappingEnum();
    return S_OK;
}


HRESULT UPnPCallbacks::OnNewExternalIPAddress(const PString & extIPAddress)
{
    m_upnp->SetExtIPAddress(extIPAddress);
    return S_OK;
}

////////////////////////////////////////////////////////////////////

PCREATE_NAT_PLUGIN(UPnP);

PNatMethod_UPnP::PNatMethod_UPnP()
{
    available = false;
    active = false;
    m_pShutdown = false;

    m_pExtIP = PIPSocket::GetDefaultIpAny();
    ep = NULL;
    m_pUPnP = NULL;
}

PNatMethod_UPnP::~PNatMethod_UPnP()
{
    m_pShutdown = true;

    if (m_pUPnP) { 
        m_pUPnP->Shutdown();
        m_pUPnP->WaitForTermination(2000);
        delete m_pUPnP;
    }
}

void PNatMethod_UPnP::AttachEndPoint(H323EndPoint * _ep)
{

  ep = _ep;

  SetPortRanges(ep->GetUDPPortBase(), ep->GetUDPPortMax(), 
      ep->GetRtpIpPortBase(), ep->GetRtpIpPortMax());

  m_pUPnP = new UPnPThread(this);

}

PBoolean PNatMethod_UPnP::GetExternalAddress(PIPSocket::Address & externalAddress,
                                             const PTimeInterval & /*maxAge*/)
{
    if (available)
        externalAddress = m_pExtIP;

    return available;
}

void PNatMethod_UPnP::SetConnectionSockets(PUDPSocket * data, PUDPSocket * control, 
                                             H323Connection::SessionInformation * info)
{
    H323Connection * connection = PRemoveConst(H323Connection, info->GetConnection());
    if (connection != NULL)
        connection->SetRTPNAT(info->GetSessionID(),data,control);
}

PBoolean PNatMethod_UPnP::CreateSocketPair(PUDPSocket * & socket1, PUDPSocket * & socket2,
      const PIPSocket::Address & binding, void * userData)
{

    H323Connection::SessionInformation * info = (H323Connection::SessionInformation *)userData;

#ifdef H323_H46019M
    PNatMethod_H46019 * handler = 
               (PNatMethod_H46019 *)ep->GetNatMethods().GetMethodByName("H46019");

    if (handler && info->GetRecvMultiplexID() > 0) {
        if (!handler->IsMultiplexed()) {
           H46019MultiplexSocket * & muxSocket1 = (H46019MultiplexSocket * &)handler->GetMultiplexSocket(true); 
           H46019MultiplexSocket * & muxSocket2 = (H46019MultiplexSocket * &)handler->GetMultiplexSocket(false); 
           muxSocket1 = new H46019MultiplexSocket(true);
           muxSocket2 = new H46019MultiplexSocket(false);
           muxSocket1->GetSubSocket() = new UPnPUDPSocket(this);  /// Data 
           muxSocket2->GetSubSocket() = new UPnPUDPSocket(this);  /// Signal
            pairedPortInfo.basePort    = ep->GetMultiplexPort();
            pairedPortInfo.maxPort     = pairedPortInfo.basePort+1;
            pairedPortInfo.currentPort = pairedPortInfo.basePort-1;
   
                while ((!OpenSocket(*(muxSocket1->GetSubSocket()), pairedPortInfo,binding)) ||
                       (!OpenSocket(*(muxSocket2->GetSubSocket()), pairedPortInfo,binding)) ||
                       (muxSocket2->GetSubSocket()->GetPort() != muxSocket1->GetSubSocket()->GetPort() + 1) )
                {
                        delete muxSocket1;
                        delete muxSocket2;
                        muxSocket1->GetSubSocket() = new UPnPUDPSocket(this);  /// Data 
                        muxSocket2->GetSubSocket() = new UPnPUDPSocket(this);  /// Signal
                }

                // Open UPnP mappings
                WORD locPort,extPort;
                PIPSocket::Address locAddr, extAddr;
                muxSocket1->GetLocalAddress(locAddr,locPort);
                extPort = locPort;
                
                if (m_pUPnP->CreateMap(true,"UDP",locAddr,locPort,extAddr,extPort,true)) {
                    ((UPnPUDPSocket*)muxSocket1->GetSubSocket())->SetMasqAddress(extAddr,extPort);
                    ((UPnPUDPSocket*)muxSocket2->GetSubSocket())->SetMasqAddress(extAddr,extPort+1);
                } else {
                    PTRACE(3, "UPnP\tError mapped ports. Abort Creating socket pair.");
                    return false;
                }

              handler->StartMultiplexListener();  // Start Multiplexing Listening thread;
              handler->EnableMultiplex(true); 
        }

       socket1 = new H46019UDPSocket(*handler->GetHandler(),info,true);      /// Data 
       socket2 = new H46019UDPSocket(*handler->GetHandler(),info,false);     /// Signal
       
       PNatMethod_H46019::RegisterSocket(true ,info->GetRecvMultiplexID(), socket1);
       PNatMethod_H46019::RegisterSocket(false,info->GetRecvMultiplexID(), socket2);

       SetConnectionSockets(socket1,socket2,info);

    } else 
#endif
   {
         pairedPortInfo.basePort    = UPnPUDPBasePort;
         pairedPortInfo.maxPort     = UPnPUDPBasePort + 1000;
         pairedPortInfo.currentPort = m_pUPnP->GetNextFreePort(pairedPortInfo.basePort,true)-1;

        if (pairedPortInfo.basePort == 0 || pairedPortInfo.basePort > pairedPortInfo.maxPort) {
            PTRACE(1, "UPnP\tInvalid local UDP port range "
                   << pairedPortInfo.currentPort << '-' << pairedPortInfo.maxPort);
            return FALSE;
        }

        bool ok = false;

        while (!ok) {
            socket1 = new UPnPUDPSocket(this);  /// Data 
            socket2 = new UPnPUDPSocket(this);  /// Signal

        /// Make sure we have sequential ports with matching external port
            while ((!OpenSocket(*socket1, pairedPortInfo,binding)) ||
                   (!OpenSocket(*socket2, pairedPortInfo,binding)) ||
                   (socket2->GetPort() != socket1->GetPort() + 1) )
            {
                    delete socket1;
                    delete socket2;
                    socket1 = new UPnPUDPSocket(this);  /// Data 
                    socket2 = new UPnPUDPSocket(this);  /// Signal
            }

            // Open UPnP mappings
            WORD locPort,extPort;
            PIPSocket::Address locAddr, extAddr;
            socket1->GetLocalAddress(locAddr,locPort);
            extPort = locPort;
            
            bool upnpMapOk = false;
            upnpMapOk = m_pUPnP->CreateMap(true,"UDP",locAddr,locPort,extAddr,extPort,true);

            if (upnpMapOk && socket1->GetPort() != extPort) {
                  PTRACE(3, "UPnP\tPort MisMatch " << socket1->GetPort() << " " << extPort << " Retrying!");
                    delete socket1;
                    delete socket2;
                    m_pUPnP->RemoveMap(extPort,true);
                    m_pUPnP->RemoveMap(extPort+1,true);
                    pairedPortInfo.currentPort = extPort-1;
            } else if (!upnpMapOk) {
                PTRACE(1, "UPnP\tERROR: Port Mapping Error Abort!");
                return false;
            } else {
                ok = true;
               ((UPnPUDPSocket*)socket1)->SetMasqAddress(extAddr,extPort);
                ((UPnPUDPSocket*)socket2)->SetMasqAddress(extAddr,extPort+1);
                PTRACE(3, "UPnP\tUDP mapped ports " << locAddr << " " << locPort << "-" <<  locPort+1 <<
                " to " << extAddr << " " << extPort << "-" <<  extPort+1 );
            }
        }

        if (ok)
          SetConnectionSockets(socket1,socket2,info);
    }

    return true;
}

PBoolean PNatMethod_UPnP::OpenSocket(PUDPSocket & socket, PortInfo & portInfo, const PIPSocket::Address & binding) const
{
  PWaitAndSignal mutex(portInfo.mutex);

  WORD startPort = portInfo.currentPort;

  do {
    portInfo.currentPort++;
    if (portInfo.currentPort > portInfo.maxPort)
      portInfo.currentPort = portInfo.basePort;

    if (m_pUPnP->IsMappingFree(portInfo.currentPort,true) && socket.Listen(binding,1, portInfo.currentPort)) {
      socket.SetReadTimeout(500);
      return true;
    }

  } while (portInfo.currentPort != startPort);

    PTRACE(2, "UPnP\tFailed to bind to local UDP port in range "
         << portInfo.currentPort << '-' << portInfo.maxPort);

   return false;
}

void PNatMethod_UPnP::SetExtIPAddress(const PString & newAddr)
{
    PTRACE(4,"UPnP\tDetected external IP address " <<  newAddr);
    m_pExtIP = newAddr;
}

WORD PNatMethod_UPnP::GetRandomPort()
{
   return RandomPortPair(UPnPUDPBasePort, UPnPUDPBasePort+1000);
}

PBoolean PNatMethod_UPnP::OnUPnPAvailable(const PString & devName)
{
    
    if (ep && ep->OnUPnPAvailable(devName, m_pExtIP, this))
                        Activate(true);

    SetAvailable();
    return true;
}

bool PNatMethod_UPnP::IsAvailable(const PIPSocket::Address & addr) 
{ 
    if (addr.GetVersion() == 6)
        return false;

    return (available && active); 
}


PBoolean PNatMethod_UPnP::CreateUPnPMap(bool pair, const PString & protocol, const PIPSocket::Address & localIP, 
                                        const WORD & locPort, PIPSocket::Address & extIP , WORD & extPort, PBoolean force)
{
    return m_pUPnP->CreateMap(pair,protocol,localIP,locPort,extIP,extPort,force);
}

void PNatMethod_UPnP::RemoveUPnPMap(WORD port,  PBoolean udp)
{
    if (m_pUPnP /*&& !m_pShutdown*/)
        m_pUPnP->RemoveMap(port, udp);
}

void PNatMethod_UPnP::SetAvailable(const PString & devName)
{
    if (m_pShutdown)
        return;

    PTRACE(2,"UPnP\tUPnP set " << devName);
    SetAvailable();
}

void PNatMethod_UPnP::SetAvailable() 
{ 
    if (!available) {
        available = true;
        if (ep) 
          ep->ForceGatekeeperReRegistration();
    }
}

void PNatMethod_UPnP::Activate(bool act)  
{
    active = act; 
}

PNatMethod::RTPSupportTypes PNatMethod_UPnP::GetRTPSupport(PBoolean force)
{
    if (available)
        return PNatMethod::RTPSupported;
    else
        return PNatMethod::RTPUnsupported;
};

H323EndPoint * PNatMethod_UPnP::GetEndPoint()
{
    return ep;
}


////////////////////////////////////////////////////////////////////

UPnPUDPSocket::UPnPUDPSocket(PNatMethod_UPnP * nat)
: natMethod(nat), extIP(0), extPort(0)
{

}

UPnPUDPSocket::~UPnPUDPSocket()
{
    Close();
}

PBoolean UPnPUDPSocket::Close()
{
    // Remove the UPnP Mapping
    if (natMethod && extPort > 0)
        natMethod->RemoveUPnPMap(extPort,true);

    return PUDPSocket::Close();
}

void UPnPUDPSocket::SetMasqAddress(const PIPSocket::Address & ip, WORD port)
{
    extIP = ip;
    extPort = port;
}

PBoolean UPnPUDPSocket::GetLocalAddress(Address & addr)
{
  if (extIP.IsAny() || !extIP.IsValid())
    return PUDPSocket::GetLocalAddress(addr);

  addr = extIP;
  return true;
}


PBoolean UPnPUDPSocket::GetLocalAddress(Address & addr, WORD & port)
{
  if (extIP.IsAny() || !extIP.IsValid())
    return PUDPSocket::GetLocalAddress(addr, port);

  addr = extIP;
  port = extPort;
  return true;
}

#endif

