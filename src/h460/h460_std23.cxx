/* H460_std23.cxx
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
 * The Original Code is derived from and used in conjunction with the 
 * H323Plus Project (www.h323plus.org/)
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

#ifdef H323_H46023

#include <h323.h>
#include "h460/h460_std23.h"
#include <ptclib/random.h>
#include <ptclib/pdns.h>
#ifdef H323_H46018
  #include <h460/h460_std18.h>
#endif
#ifdef H323_H46019M
  #include <h460/h46018_h225.h>
#endif
#ifdef H323_UPnP
 #include "h460/upnpcp.h"
#endif

#if _WIN32
#pragma message("H.460.23/.24 Enabled. Contact consulting@h323plus.org for licensing terms.")
#else
#warning("H.460.23/.24 Enabled. Contact consulting@h323plus.org for licensing terms.")
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4239)
#endif


// H.460.23 NAT Detection Feature
#define remoteNATOID        1       // bool if endpoint has remote NAT support
#define AnnexAOID           2       // bool if endpoint supports H.460.24 Annex A
#define localNATOID         3       // bool if endpoint is NATed
#define NATDetRASOID        4       // Detected RAS H225_TransportAddress 
#define STUNServOID         5       // H225_TransportAddress of STUN Server 
#define NATTypeOID          6       // integer 8 Endpoint NAT Type
#define AnnexBOID           7       // bool if endpoint supports H.460.24 Annex B


// H.460.24 P2Pnat Feature
#define NATProxyOID          1       // PBoolean if gatekeeper will proxy
#define remoteMastOID        2       // PBoolean if remote endpoint can assist local endpoint directly
#define mustProxyOID         3       // PBoolean if gatekeeper must proxy to reach local endpoint
#define calledIsNatOID       4       // PBoolean if local endpoint is behind a NAT/FW
#define NatRemoteTypeOID     5       // integer 8 reported NAT type
#define apparentSourceOID    6       // H225_TransportAddress of apparent source address of endpoint
#define SupAnnexAOID         7       // PBoolean if local endpoint supports H.460.24 Annex A
#define NATInstOID           8       // integer 8 Instruction on how NAT is to be Traversed
#define SupAnnexBOID         9       // bool if endpoint supports H.460.24 Annex B


//////////////////////////////////////////////////////////////////////

#if PTLIB_VER >= 2130
PCREATE_NAT_PLUGIN(H46024, "H.460.24");
#else
PCREATE_NAT_PLUGIN(H46024);
#endif

PNatMethod_H46024::PNatMethod_H46024()
: mainThread(NULL)
{
    natType = PSTUNClient::UnknownNat;
    isAvailable = false;
    isActive = false;
    feat = NULL;
}

PNatMethod_H46024::~PNatMethod_H46024()
{
    natType = PSTUNClient::UnknownNat;
    delete mainThread;
}

void PNatMethod_H46024::SetPortInformation(PortInfo & pairedPortInfo, WORD portPairBase, WORD portPairMax)
{
  pairedPortInfo.basePort = (WORD)((portPairBase+1)&0xfffe);
  if (portPairBase == 0) {
    pairedPortInfo.basePort = 0;
    pairedPortInfo.maxPort = 0;
  }
  else if (portPairMax == 0)
    pairedPortInfo.maxPort = (WORD)(pairedPortInfo.basePort+99);
  else if (portPairMax < portPairBase)
    pairedPortInfo.maxPort = portPairBase;
  else
    pairedPortInfo.maxPort = portPairMax;

  pairedPortInfo.currentPort = pairedPortInfo.basePort;

}

void PNatMethod_H46024::Start(const PString & server,H460_FeatureStd23 * _feat)
{
    feat = _feat;

   H323EndPoint * ep = feat->GetEndPoint();

   SetServer(server);
#ifdef H323_H46019M
    WORD muxBase = ep->GetMultiplexPort();
    SetPortInformation(multiplexPorts,muxBase-2, muxBase+2);
    SetPortInformation(standardPorts, ep->GetRtpIpPortBase(), ep->GetRtpIpPortMax());
    SetPortRanges(muxBase-2, muxBase+2, muxBase-2, muxBase+20);
#else
    SetPortRanges(ep->GetRtpIpPortBase(), ep->GetRtpIpPortMax(), ep->GetRtpIpPortBase(), ep->GetRtpIpPortMax());
#endif

    mainThread  = PThread::Create(PCREATE_NOTIFIER(MainMethod), 0,  
                       PThread::NoAutoDeleteThread, PThread::NormalPriority, "H.460.24");
}



PSTUNClient::NatTypes PNatMethod_H46024::NATTest()
{

    PSTUNClient::NatTypes testtype;
    WORD testport;
#ifdef H323_H46019M
    testport = (WORD)feat->GetEndPoint()->GetMultiplexPort()-1;
#else
    PRandom rand;
    testport = (WORD)rand.Generate(singlePortInfo.basePort , singlePortInfo.maxPort);   
#endif

    singlePortInfo.currentPort = testport;
    PTRACE(4,"Std23\tSTUN Test Port " << singlePortInfo.currentPort+1);

    testtype = GetNatType(true);

#ifdef H323_H46019M
    // if we have a cone NAT check the RTCP Port to see if not existing binding
    if (testtype == PSTUNClient::ConeNat && natType == PSTUNClient::UnknownNat) {
        PThread::Sleep(10);
        PTRACE(4,"Std23\tCone NAT Detected rechecking. Test Port " << singlePortInfo.currentPort+1);
        PSTUNClient::NatTypes test2 = GetNatType(true);
        if (test2 > testtype)
            testtype = test2;
    }
#endif

    return testtype;
}

int recheckTime = 300000;    // 5 minutes

void PNatMethod_H46024::MainMethod(PThread &, INT)
{

    while (natType == PSTUNClient::UnknownNat ||
                natType == PSTUNClient::ConeNat) {
        PSTUNClient::NatTypes testtype = NATTest();
        if (natType != testtype) {
            natType = testtype;
            PIPSocket::Address extIP;
            if (GetExternalAddress(extIP)) {
                feat->GetEndPoint()->NATMethodCallBack(GetName(),2,natType);
                feat->OnNATTypeDetection(natType, extIP);
            }
        }

        if (natType == PSTUNClient::ConeNat) {
            isAvailable = true;
            PThread::Sleep(recheckTime);
            continue;
        }

        isAvailable = false;
        if (natType == PSTUNClient::UnknownNat) {
            PTRACE(1,"Std24\tNAT Test failed to resolve NAT Type");
            break;
        }
    }

}

bool PNatMethod_H46024::IsAvailable(const PIPSocket::Address & /*binding*/)
{
    if (!isActive)
        return false;

    return isAvailable;
}

void PNatMethod_H46024::SetAvailable() 
{ 
    feat->GetEndPoint()->NATMethodCallBack(GetName(),1,"Available");
    isAvailable = true; 
}

void PNatMethod_H46024::Activate(bool act)
{
    if (act && !isAvailable)  // Special case where activated but not available.
       isAvailable = true;  

    isActive = act;
}

PSTUNClient::NatTypes PNatMethod_H46024::GetNATType()
{
    return natType;
}

WORD PNatMethod_H46024::CreateRandomPortPair(unsigned int start, unsigned int end)
{
    WORD num;
    PRandom rand;
    num = (WORD)rand.Generate(start,end);
    if (num %2 != 0) 
        num++;  // Make sure the number is even

    return num;
}


PBoolean PNatMethod_H46024::CreateSocketPair(PUDPSocket * & socket1,
                                             PUDPSocket * & socket2,
                                             const PIPSocket::Address & binding,
	                                         void * userData
                                             )
{
    PWaitAndSignal m(portMute);

    H323Connection::SessionInformation * info = (H323Connection::SessionInformation *)userData;
#ifdef H323_H46019M
    PNatMethod_H46019 * handler = 
               (PNatMethod_H46019 *)feat->GetEndPoint()->GetNatMethods().GetMethodByName("H46019");

    if (handler && info && (info->GetRecvMultiplexID() > 0)) {
        if (!handler->IsMultiplexed()) {
           // Set Multiplex ports here
           SetPortRanges(multiplexPorts.basePort, multiplexPorts.maxPort, multiplexPorts.basePort, multiplexPorts.maxPort);

           H46019MultiplexSocket * & muxSocket1 = (H46019MultiplexSocket * &)handler->GetMultiplexSocket(true); 
           H46019MultiplexSocket * & muxSocket2 = (H46019MultiplexSocket * &)handler->GetMultiplexSocket(false);
           muxSocket1 = new H46019MultiplexSocket(true);
           muxSocket2 = new H46019MultiplexSocket(false);
           pairedPortInfo.currentPort = feat->GetEndPoint()->GetMultiplexPort()-1;

#if PTLIB_VER >= 2130
           if (!PSTUNClient::CreateSocketPair(muxSocket1->GetSubSocket(), muxSocket2->GetSubSocket(), binding, (PObject *)1)) 
#else
           if (!PSTUNClient::CreateSocketPair(muxSocket1->GetSubSocket(), muxSocket2->GetSubSocket(), binding))
#endif
                return false;

           PIPSocket::Address stunAddress;
           muxSocket1->GetSubSocket()->GetLocalAddress(stunAddress);
           PTRACE(1,"Std24\tMux STUN Created: " << stunAddress  << " "
                        << muxSocket1->GetSubSocket()->GetPort() << "-" << muxSocket2->GetSubSocket()->GetPort());
           
           handler->StartMultiplexListener();  // Start Multiplexing Listening thread;
           handler->EnableMultiplex(true); 
        }

       socket1 = new H46019UDPSocket(*handler->GetHandler(),info,true);      /// Data 
       socket2 = new H46019UDPSocket(*handler->GetHandler(),info,false);     /// Signal
 
       PNatMethod_H46019::RegisterSocket(true ,info->GetRecvMultiplexID(), socket1);
       PNatMethod_H46019::RegisterSocket(false,info->GetRecvMultiplexID(), socket2);

    } else {
        // Set standard ports here
        SetPortRanges(standardPorts.basePort, standardPorts.maxPort, standardPorts.basePort, standardPorts.maxPort);
#else
    {
#endif

#if PTLIB_VER >= 2130
        if (!PSTUNClient::CreateSocketPair(socket1,socket2,binding, NULL))
#else
        if (!PSTUNClient::CreateSocketPair(socket1,socket2,binding))
#endif
             return false;
    }

    SetConnectionSockets(socket1,socket2,info);
    return true;
}


void PNatMethod_H46024::SetConnectionSockets(PUDPSocket * data, PUDPSocket * control, 
                                             H323Connection::SessionInformation * info)
{
    H323Connection * connection = PRemoveConst(H323Connection, info->GetConnection());
    if (connection != NULL)
        connection->SetRTPNAT(info->GetSessionID(),data,control);
}

//////////////////////////////////////////////////////////////////////

H460_FEATURE(Std23);

H460_FeatureStd23::H460_FeatureStd23()
: H460_FeatureStd(23)
{
  PTRACE(6,"Std23\tInstance Created");

  FeatureCategory = FeatureSupported;

  EP = NULL;
  alg = false;
  isavailable = true;
  isEnabled = false;
  natType = PSTUNClient::UnknownNat;
  externalIP = PIPSocket::GetDefaultIpAny();
  useAlternate = 0;
  natNotify = false;
  RegThread = NULL;
}

H460_FeatureStd23::~H460_FeatureStd23()
{
}

void H460_FeatureStd23::AttachEndPoint(H323EndPoint * _ep)
{
    EP = _ep;
    isEnabled = EP->H46023IsEnabled();

    // Ignore if already manually using STUN
    isavailable = (EP->GetSTUN() == NULL);
}

PBoolean H460_FeatureStd23::OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu) 
{ 
    if (!isEnabled)
        return false;

    if (!isavailable)
        return FALSE;

    H460_FeatureStd feat = H460_FeatureStd(23); 
    pdu = feat;
    return TRUE; 
}

PBoolean H460_FeatureStd23::OnSendRegistrationRequest(H225_FeatureDescriptor & pdu) 
{ 
    if (!isEnabled)
        return false;

    if (!isavailable)
            return FALSE;

    // Build Message
    H460_FeatureStd feat = H460_FeatureStd(23); 

    if ((EP->GetGatekeeper() == NULL) ||
        (!EP->GetGatekeeper()->IsRegistered())) {
          // First time registering
          // We always support remote
            feat.Add(remoteNATOID,H460_FeatureContent(true));
#ifdef H323_H46024A
            feat.Add(AnnexAOID,H460_FeatureContent(true));
#endif
#ifdef H323_H46024B
            feat.Add(AnnexBOID,H460_FeatureContent(true));
#endif
    } else {
        if (alg) {
              // We should be disabling H.460.23/.24 support but 
              // we will disable H.460.18/.19 instead :) and say we have no NAT..
                feat.Add(NATTypeOID,H460_FeatureContent(1,8)); 
                feat.Add(remoteNATOID,H460_FeatureContent(false)); 
                isavailable = false;
                alg = false;
        } else {
            if (natNotify || AlternateNATMethod()) {
                feat.Add(NATTypeOID,H460_FeatureContent(natType,8)); 
                natNotify = false;
            }
        }
    }

    pdu = feat;

    return true; 
}

void H460_FeatureStd23::OnReceiveGatekeeperConfirm(const H225_FeatureDescriptor & /*pdu*/) 
{
    isEnabled = true;
}

void H460_FeatureStd23::OnReceiveRegistrationConfirm(const H225_FeatureDescriptor & pdu)
{
    isEnabled = true;

   H460_FeatureStd & feat = (H460_FeatureStd &)pdu;

   // Ignore whether the gatekeeper detected as being behind NAT
   // The STUN test will confirm - SH
   //PBoolean NATdetect = false;
   //if (feat.Contains(localNATOID))
   //    NATdetect = feat.Value(localNATOID);

       if (feat.Contains(STUNServOID)) {
          H323TransportAddress addr = feat.Value(STUNServOID);
          StartSTUNTest(addr.Mid(3));
       }

       if (feat.Contains(NATDetRASOID)) {
           H323TransportAddress addr = feat.Value(NATDetRASOID);
           PIPSocket::Address ip;
           addr.GetIpAddress(ip);
           if (DetectALG(ip)) {
             // if we have an ALG then to be on the safe side
             // disable .23/.24 so that the ALG has more chance
             // of behaving properly...
               alg = true;
               DelayedReRegistration();
           }
       }
}


void H460_FeatureStd23::OnNATTypeDetection(PSTUNClient::NatTypes type, const PIPSocket::Address & ExtIP)
{
    if (natType == type)
        return;

    externalIP = ExtIP;

    if (natType == PSTUNClient::UnknownNat) {
        PTRACE(4,"Std23\tSTUN Test Result: " << type << " forcing reregistration.");
#ifdef H323_UPnP
        if (type > PSTUNClient::ConeNat) {
           PString name = PString();
           if (IsAlternateAvailable(name))
               EP->NATMethodCallBack(name,1,"Available");
           else
               EP->InitialiseUPnP();
        }
#endif
        natType = type;  // first time detection
    } else {
        PTRACE(2,"Std23\tBAD NAT Detected: Was " << natType << " Now " << type << " Disabling H.460.23/.24");
        natType = PSTUNClient::UnknownNat;  // Leopard changed it spots (disable H.460.23/.24)
    }
    
    natNotify = true;
    EP->ForceGatekeeperReRegistration();
}

bool H460_FeatureStd23::DetectALG(const PIPSocket::Address & detectAddress)
{

#if P_HAS_IPV6
  // Again horrible code should be able to get interface listing for a given protocol - SH
  PBoolean ipv6IPv4Discover = false;
  if (detectAddress.GetVersion() == 4 && PIPSocket::GetDefaultIpAddressFamily() == AF_INET6) {
      PIPSocket::SetDefaultIpAddressFamilyV4();
      ipv6IPv4Discover = true;
  }
#endif
    bool found = true;
    PIPSocket::InterfaceTable if_table;
    if (!PIPSocket::GetInterfaceTable(if_table)) {
        PTRACE(1, "Std23\tERROR: Can't get interface table");
        found = false;
    } else {  
        for (PINDEX i=0; i< if_table.GetSize(); i++) {
            if (detectAddress == if_table[i].GetAddress()) {
                PTRACE(4, "Std23\tNo Intermediary device detected between EP and GK");
                found = false;
                break;
            }
        }
    }
#if P_HAS_IPV6
  if (ipv6IPv4Discover)
      PIPSocket::SetDefaultIpAddressFamilyV6();
#endif
    if (found) {
        PTRACE(4, "Std23\tWARNING: Intermediary device detected!");
        EP->NATMethodCallBack("ALG",1,"Available");
        return true;
    }

    return false;
}

void H460_FeatureStd23::StartSTUNTest(const PString & server)
{
    PString s;
#ifdef P_DNS
    PStringList SRVs;
    PStringList x = server.Tokenise(":");
    PString number = "h323:user@" + x[0];
    if (PDNS::LookupSRV(number,"_stun._udp.",SRVs))
        s = SRVs[0];
    else
#endif
        s = server;

    // Remove any previous NAT methods.
    EP->GetNatMethods().RemoveMethod("H46024");
    natType = PSTUNClient::UnknownNat;

    PNatMethod_H46024 * xnat = (PNatMethod_H46024 *)EP->GetNatMethods().LoadNatMethod("H46024");
#ifdef H323_UPnP
    PString name = PString();
    if (IsAlternateAvailable(name)) {
          EP->NATMethodCallBack(name,1,"Available");
          EP->ForceGatekeeperReRegistration();
    } else
#endif
        xnat->Start(s,this);

    EP->GetNatMethods().AddMethod(xnat);
}

bool H460_FeatureStd23::IsAvailable()
{
    return isavailable;
}

#if H323_UPnP
bool H460_FeatureStd23::IsAlternateAvailable(PString & name)
{
    PNatMethod_UPnP * upnpMethod = (PNatMethod_UPnP *)EP->GetNatMethods().GetMethodByName("UPnP");
    if (upnpMethod && upnpMethod->IsAvailable(PIPSocket::Address::GetAny(4))) {
          PTRACE(4,"Std23\tSTUN Setting alternate: UPnP");
          name = upnpMethod->GetName();
          natType = PSTUNClient::ConeNat;
          natNotify = true;
          useAlternate = 1;
          upnpMethod->Activate(true);
          return true;
    }
    return false;
}
#endif

void H460_FeatureStd23::DelayedReRegistration()
{
     RegThread = PThread::Create(PCREATE_NOTIFIER(RegMethod), 0,
                    PThread::AutoDeleteThread,
                    PThread::NormalPriority,
                    "regmeth23");
}

void H460_FeatureStd23::RegMethod(PThread &, INT)
{
    PThread::Sleep(1000);
    EP->ForceGatekeeperReRegistration();  // We have an ALG so notify the gatekeeper   
}

bool H460_FeatureStd23::AlternateNATMethod()
{
#ifdef H323_UPnP
    if (natType <= PSTUNClient::ConeNat || useAlternate > 0)
        return false;

    H323NatList & natlist = EP->GetNatMethods().GetNATList();

    for (PINDEX i=0; i< natlist.GetSize(); i++) {
#if PTLIB_VER >= 2130
        PString methName = natlist[i].GetMethodName();
#else
        PString methName = natlist[i].GetName();
#endif
        if (methName == "UPnP" && 
            natlist[i].GetRTPSupport() == PSTUNClient::RTPSupported) {
            PIPSocket::Address extIP;
            natlist[i].GetExternalAddress(extIP);
            if (extIP.IsAny() || !extIP.IsValid() || externalIP.IsLoopback() || extIP == externalIP) {
                PTRACE(4,"H46023\tUPnP Change NAT from " << natType << " to " << PSTUNClient::ConeNat);
                natType = PSTUNClient::ConeNat;
                useAlternate = 1;
                natlist[i].Activate(true);
                EP->NATMethodCallBack(methName,1,"Available");
                return true;
            } else {
                PTRACE(4,"H46023\tUPnP Unavailable subNAT STUN: " << externalIP << " UPnP " << extIP);
                useAlternate = 2;
            }
        }
    }
#endif
    return false;
}

bool H460_FeatureStd23::UseAlternate()
{
    return (useAlternate == 1);
}

///////////////////////////////////////////////////////////////////


H460_FEATURE(Std24);

H460_FeatureStd24::H460_FeatureStd24()
: H460_FeatureStd(24)
{
 PTRACE(6,"Std24\tInstance Created");

 EP = NULL;
 CON = NULL;
 natconfig = H460_FeatureStd24::e_unknown;
 FeatureCategory = FeatureSupported;
 isEnabled = false;
 useAlternate = false;

}

H460_FeatureStd24::~H460_FeatureStd24()
{
}

void H460_FeatureStd24::AttachEndPoint(H323EndPoint * _ep)
{
   EP = _ep; 
    // We only enable IF the gatekeeper supports H.460.23
    H460_FeatureSet * gkfeat = EP->GetGatekeeperFeatures();
    if (gkfeat && gkfeat->HasFeature(23)) {
        H460_FeatureStd23 * feat = (H460_FeatureStd23 *)gkfeat->GetFeature(23);
        isEnabled = feat->IsAvailable();
        useAlternate = feat->UseAlternate();
    } else {
        PTRACE(4,"Std24\tH.460.24 disabled as H.460.23 is disabled!");
        isEnabled = false;
    }
}

void H460_FeatureStd24::AttachConnection(H323Connection * _conn)
{
   CON = _conn; 
}

PBoolean H460_FeatureStd24::OnSendAdmissionRequest(H225_FeatureDescriptor & pdu) 
{ 
    // Ignore if already not enabled or manually using STUN
    if (!isEnabled) 
        return FALSE;

    PWaitAndSignal m(h460mute);

    // Build Message
    PTRACE(6,"Std24\tSending ARQ ");
    H460_FeatureStd feat = H460_FeatureStd(24);

    if (natconfig != e_unknown) {
       feat.Add(NATInstOID,H460_FeatureContent((unsigned)natconfig,8));
    }

    pdu = feat;
    return TRUE;  
}

void H460_FeatureStd24::OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu)
{
     H460_FeatureStd & feat = (H460_FeatureStd &)pdu;

    if (feat.Contains(NATInstOID)) {
        PTRACE(6,"Std24\tReading ACF");
        unsigned NATinst = feat.Value(NATInstOID); 
        natconfig = (NatInstruct)NATinst;
        HandleNATInstruction(natconfig);
    }
}

void H460_FeatureStd24::OnReceiveAdmissionReject(const H225_FeatureDescriptor & pdu) 
{
     PTRACE(6,"Std24\tARJ Received");
     HandleNATInstruction(H460_FeatureStd24::e_natFailure);
}

PBoolean H460_FeatureStd24::OnSendSetup_UUIE(H225_FeatureDescriptor & pdu)
{ 
  // Ignore if already not enabled or manually using STUN
  if (!isEnabled) 
        return FALSE;

 PTRACE(6,"Std24\tSend Setup");
    if (natconfig == H460_FeatureStd24::e_unknown)
        return FALSE;

    H460_FeatureStd feat = H460_FeatureStd(24);
    
    int remoteconfig;
    switch (natconfig) {
        case H460_FeatureStd24::e_noassist:
            remoteconfig = H460_FeatureStd24::e_noassist;
            break;
        case H460_FeatureStd24::e_localMaster:
            remoteconfig = H460_FeatureStd24::e_remoteMaster;
            break;
        case H460_FeatureStd24::e_remoteMaster:
            remoteconfig = H460_FeatureStd24::e_localMaster;
            break;
        case H460_FeatureStd24::e_localProxy:
            remoteconfig = H460_FeatureStd24::e_remoteProxy;
            break;
        case H460_FeatureStd24::e_remoteProxy:
            remoteconfig = H460_FeatureStd24::e_localProxy;
            break;
        default:
            remoteconfig = natconfig;
    }

    feat.Add(NATInstOID,H460_FeatureContent(remoteconfig,8));
    pdu = feat;
    return TRUE; 
}
    
void H460_FeatureStd24::OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu)
{

    PWaitAndSignal m(h460mute);

     H460_FeatureStd & feat = (H460_FeatureStd &)pdu;

     if (feat.Contains(NATInstOID)) {
      PTRACE(6,"Std24\tReceive Setup");

        unsigned NATinst = feat.Value(NATInstOID); 
        natconfig = (NatInstruct)NATinst;
        HandleNATInstruction(natconfig);
    }

}

void H460_FeatureStd24::HandleNATInstruction(NatInstruct _config)
{
        
        PTRACE(4,"Std24\tNAT Instruction Received: " << _config);
        switch (_config) {
            case H460_FeatureStd24::e_localMaster:
                PTRACE(4,"Std24\tLocal NAT Support: H.460.24 ENABLED");
                CON->SetRemoteNAT(true);
                CON->H46019SetOffload();
                SetNATMethods(e_enable);
                break;

            case H460_FeatureStd24::e_remoteMaster:
                PTRACE(4,"Std24\tRemote NAT Support: ALL NAT DISABLED");
                CON->H46019SetOffload();
                if (IsNatSendAvailable()) {  // If we can use STUN do it!
                    CON->SetRemoteNAT(false);
                    SetNATMethods(e_enable);
                } else
                    SetNATMethods(e_disable);
                break;

            case H460_FeatureStd24::e_remoteProxy:
                PTRACE(4,"Std24\tRemote Proxy Support: H.460.24 DISABLED");
                SetNATMethods(e_default);
                break;

            case H460_FeatureStd24::e_localProxy:
                PTRACE(4,"Std24\tCall Local Proxy: H.460.24 DISABLED");
                SetNATMethods(e_default);
                break;

#ifdef H323_H46024A
            case H460_FeatureStd24::e_natAnnexA:
                PTRACE(4,"Std24\tSame NAT: H.460.24 AnnexA ENABLED");
                CON->H46024AEnabled();
                SetNATMethods(e_AnnexA);
                break;
#endif

#ifdef H323_H46024B
            case H460_FeatureStd24::e_natAnnexB:
                PTRACE(4,"Std24\tSame NAT: H.460.24 AnnexA ENABLED");
                CON->H46024BEnabled();
                //CON->H46024AEnabled();  // Might be on same internal network
                SetNATMethods(e_AnnexB);
                break;
#endif

            case H460_FeatureStd24::e_natFailure:
                PTRACE(4,"Std24\tCall Failure Detected");
                EP->FeatureCallBack(GetFeatureName()[0],1,"Call Failure");
                break;
            case H460_FeatureStd24::e_noassist:
                PTRACE(4,"Std24\tNAT Call direct");
            default:
                PTRACE(4,"Std24\tNo Assist: H.460.24 DISABLED.");
                CON->DisableNATSupport();
                SetNATMethods(e_default);
                break;
        }
}

void H460_FeatureStd24::SetH46019State(bool state)
{
#ifdef H323_H46018
    if (CON->GetFeatureSet()->HasFeature(19)) {
        H460_Feature * feat = CON->GetFeatureSet()->GetFeature(19);

        PTRACE(4,"H46024\t" << (state ? "En" : "Dis") << "abling H.460.19 support for call");
        ((H460_FeatureStd19 *)feat)->SetAvailable(state);
    }
#endif
}

PBoolean H460_FeatureStd24::IsNatSendAvailable()
{
   H323NatList & natlist = EP->GetNatMethods().GetNATList();

   PBoolean available = false;
   PINDEX i=0;
   for (i=0; i< natlist.GetSize(); i++) {
#if PTLIB_VER >= 2130
        if (natlist[i].GetMethodName() == "H46024") break;
#else
        if (natlist[i].GetName() == "H46024") break;
#endif
   }
   if (i < natlist.GetSize()) {
     PNatMethod_H46024 & meth = (PNatMethod_H46024 &)natlist[i];
      switch (meth.GetNatType(false)) {
        case PSTUNClient::ConeNat :
        case PSTUNClient::RestrictedNat :
        case PSTUNClient::PortRestrictedNat :
          available = true;
          break;
        case PSTUNClient::SymmetricNat :
        default :   // UnknownNet, SymmetricFirewall, BlockedNat
          break;
      }
   }
   return available;
}

void H460_FeatureStd24::SetNATMethods(H46024NAT state)
{

    H323NatList & natlist = EP->GetNatMethods().GetNATList();

    for (PINDEX i=0; i< natlist.GetSize(); i++) {
#if PTLIB_VER >= 2130
        PString name = natlist[i].GetMethodName();
#else
        PString name = natlist[i].GetName();
#endif
        switch (state) {
            case H460_FeatureStd24::e_AnnexA:   // To do Annex A Implementation.
            case H460_FeatureStd24::e_AnnexB:   // To do Annex B Implementation.
            case H460_FeatureStd24::e_default:
                if (name == "H46024" || name == "UPnP")
                    natlist[i].Activate(false);
                break;
            case H460_FeatureStd24::e_enable:
                if (name == "H46024" && !useAlternate)
                    natlist[i].Activate(true);
                else if (name == "UPnP" && useAlternate)
                    natlist[i].Activate(true);
                else
                    natlist[i].Activate(false);
                break;
            case H460_FeatureStd24::e_disable:
                if (name == "H46019" && CON->IsH46019Multiplexed())
                    natlist[i].Activate(true);
                else
                    natlist[i].Activate(false);
                break;
            default:
                break;
        }
    }

   PTRACE(6,"Std24\tNAT Methods " << GetH460NATString(state));
   for (PINDEX i=0; i< natlist.GetSize(); i++) {
#if PTLIB_VER >= 2130
       PString name = natlist[i].GetMethodName();
#else
       PString name = natlist[i].GetName();
#endif
       PTRACE(6, "H323\tNAT Method " << i << " " << name << " Ready: "
                  << (natlist[i].IsAvailable(PIPSocket::Address::GetAny(4)) ? "Yes" : "No"));  
   }
}

PString H460_FeatureStd24::GetNATStrategyString(NatInstruct method)
{
  static const char * const Names[10] = {
        "Unknown Strategy",
        "No Assistance",
        "Local Master",
        "Remote Master",
        "Local Proxy",
        "Remote Proxy",
        "Full Proxy",
        "AnnexA SameNAT",
        "AnnexB NAToffload",
        "Call Failure!"
  };

  if (method < 10)
    return Names[method];

   return psprintf("<NAT Strategy %u>", method);
}

PString H460_FeatureStd24::GetH460NATString(H46024NAT method)
{
    static const char * const Names[5] = {
        "default"
        "enable"
        "AnnexA"
        "AnnexB"
        "disable"
    };

  if (method < 5)
    return Names[method];

   return psprintf("<H460NAT %u>", method);
}



#ifdef _MSC_VER
#pragma warning(default : 4239)
#endif

#endif // Win32_WCE
