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
 * $Log$
 * Revision 1.1  2009/07/25 10:35:51  shorne
 * First cut of H.460.23/.24 support
 *
 *
 *
 *
 */
#include <ptlib.h>
#include <openh323buildopts.h>

#ifdef H323_H46023

#include "h460/h460_std23.h"
#include <h323.h>
#include <ptclib/random.h>
#include <h460/h460_std18.h>

#if _WIN32
#pragma message("H.460.23/.24 Enabled. Contact consulting@h323plus.org for licensing terms.")
#else
#warning("H.460.23/.24 Enabled. Contact consulting@h323plus.org for licensing terms.")
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4239)
#endif


// H.460.23 NAT Detection Feature
#define remoteNATOID		1     	// bool if endpoint has remote NAT support
#define sameNATOID			2		// bool if endpoint supports H.460.24 Annex A
#define localNATOID			3       // PBoolean if endpoint is NATed
#define NATDetRASOID		4       // Detected RAS H225_TransportAddress 
#define STUNServOID			5       // H225_TransportAddress of STUN Server 
#define NATTypeOID			6       // integer 8 Endpoint NAT Type


// H.460.24 P2Pnat Feature
#define NATProxyOID			1       // PBoolean if gatekeeper will proxy
#define remoteMastOID		2       // PBoolean if remote endpoint can assist local endpoint directly
#define mustProxyOID		3       // PBoolean if gatekeeper must proxy to reach local endpoint
#define calledIsNatOID		4       // PBoolean if local endpoint is behind a NAT/FW
#define NatRemoteTypeOID	5		// integer 8 reported NAT type
#define apparentSourceOID	6       // H225_TransportAddress of apparent source address of endpoint
#define SupSameNATOID		7   	// PBoolean if local endpoint supports same NAT probe
#define NATInstOID    		8   	// integer 8 Instruction on how NAT is to be Traversed


//////////////////////////////////////////////////////////////////////

PCREATE_NAT_PLUGIN(H46024);


PNatMethod_H46024::PNatMethod_H46024()
 : PThread(1000, NoAutoDeleteThread, LowPriority ,"H.460.24")
{
	natType = PSTUNClient::UnknownNat;
	isAvailable = false;
	isActive = false;
	feat = NULL;
}

PNatMethod_H46024::~PNatMethod_H46024()
{
	natType = PSTUNClient::UnknownNat;
}

void PNatMethod_H46024::Start(const PString & server,H460_FeatureStd23 * _feat)
{
	feat = _feat;
	H323EndPoint * ep = feat->GetEndPoint();
    Initialise(server,ep->GetRtpIpPortBase(), ep->GetRtpIpPortMax(), ep->GetRtpIpPortBase(), ep->GetRtpIpPortMax());

	Resume();
}



PSTUNClient::NatTypes PNatMethod_H46024::NATTest()
{

	PRandom rand;
	WORD testport = (WORD)rand.Generate(singlePortInfo.basePort , singlePortInfo.maxPort);
	PSTUNClient::NatTypes testtype;

	singlePortInfo.currentPort = testport;
	testtype = GetNatType(true);

    PTRACE(4,"Std23\tSTUN Test Port " << singlePortInfo.currentPort << " result: " << testtype);

	return testtype;
}

int recheckTime = 300000;    // 5 minutes

void PNatMethod_H46024::Main()
{

	while (natType == PSTUNClient::UnknownNat ||
				natType == PSTUNClient::ConeNat) {
		PSTUNClient::NatTypes testtype = NATTest();
		if (natType != testtype) {
			natType = testtype;
			feat->OnNATTypeDetection(natType);
		}

		if (natType == PSTUNClient::ConeNat) {
			isAvailable = true;
			PProcess::Sleep(recheckTime);
		} else {
			isAvailable = false;
		}
	}

}

bool PNatMethod_H46024::IsAvailable(const PIPSocket::Address & binding)
{
	if (!isActive)
		return false;

	return isAvailable;
}

void PNatMethod_H46024::Activate(bool act)
{
	isActive = act;
}

PSTUNClient::NatTypes PNatMethod_H46024::GetNATType()
{
	return natType;
}

PBoolean PNatMethod_H46024::CreateSocketPair(PUDPSocket * & socket1,
											 PUDPSocket * & socket2,
											const PIPSocket::Address & binding)
{
#if (PTLIB_VER > 260)
	pairedPortInfo.currentPort = 
			RandomPortPair(pairedPortInfo.basePort-1,pairedPortInfo.maxPort-2);
#endif

	return PSTUNClient::CreateSocketPair(socket1,socket2,binding);
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
 natType = PSTUNClient::UnknownNat;
 natNotify = false;
}

H460_FeatureStd23::~H460_FeatureStd23()
{
}

void H460_FeatureStd23::AttachEndPoint(H323EndPoint * _ep)
{
    EP = _ep; 
}

PBoolean H460_FeatureStd23::OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu) 
{ 
	// Ignore if already manually using STUN
	isavailable = (EP->GetSTUN() == NULL);	
	if (!isavailable)
		return FALSE;

	H460_FeatureStd feat = H460_FeatureStd(23); 
	pdu = feat;
	return TRUE; 
}

PBoolean H460_FeatureStd23::OnSendRegistrationRequest(H225_FeatureDescriptor & pdu) 
{ 

	// Ignore if already manually using STUN
	if (isavailable) 
		isavailable = (EP->GetSTUN() == NULL);

	if (!isavailable)
			return FALSE;

    // Build Message
    H460_FeatureStd & feat = H460_FeatureStd(23); 

    if ((EP->GetGatekeeper() == NULL) ||
		(!EP->GetGatekeeper()->IsRegistered())) {
		  // First time registering
		  // We always support remote
	        feat.Add(remoteNATOID,H460_FeatureContent(true));
#ifdef H323_H46024A
			feat.Add(sameNATOID,H460_FeatureContent(true));
#endif
	} else {
		if (alg) {
			  // We are disabling H.460.23/.24 support
				feat.Add(NATTypeOID,H460_FeatureContent(0,8)); 
				feat.Add(remoteNATOID,H460_FeatureContent(false)); 
				isavailable = false;
				alg = false;
		} else {
			if (natNotify) {
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

}

void H460_FeatureStd23::OnReceiveRegistrationConfirm(const H225_FeatureDescriptor & pdu)
{

   H460_FeatureStd & feat = (H460_FeatureStd &)pdu;

   PBoolean NATdetect = false;

   if (feat.Contains(localNATOID))
       NATdetect = feat.Value(localNATOID);

   if (NATdetect) {  // If we detected as being behind NAT then do test
	   if (feat.Contains(STUNServOID)) {
          H323TransportAddress addr = feat.Value(STUNServOID);
          StartSTUNTest(addr.Mid(3));
	   }
   } else {  // If not being detected see if an ALG is present
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
}

void H460_FeatureStd23::OnNATTypeDetection(PSTUNClient::NatTypes type)
{
	if (natType == type)
		return;

	if (natType == PSTUNClient::UnknownNat) {
		PTRACE(4,"Std23\tSTUN Test Result: " << type << " forcing reregistration.");
		natType = type;  // first time detection
	} else {
		PTRACE(2,"Std23\tBAD NAT Detected: Was " << natType << " Now " << type);
		natType = type;  // Leopard changed it spots (change this to PSTUNClient::UnknownNat if disabling)
	}
	
	natNotify = true;
	EP->ForceGatekeeperReRegistration();
}

bool H460_FeatureStd23::DetectALG(const PIPSocket::Address & detectAddress)
{
	PIPSocket::InterfaceTable if_table;
	if (!PIPSocket::GetInterfaceTable(if_table)) {
		PTRACE(1, "Std23\tERROR: Can't get interface table");
		return false;
	}
	
	for (PINDEX i=0; i< if_table.GetSize(); i++) {
		if (detectAddress == if_table[i].GetAddress()) {
			PTRACE(4, "Std23\tNo Intermediary device detected between EP and GK");
			return false;
		}
	}
	PTRACE(4, "Std23\tWARNING: Intermediary device detected!");
	return true;
}

void H460_FeatureStd23::StartSTUNTest(const PString & server)
{
	PNatMethod_H46024 * xnat = (PNatMethod_H46024 *)EP->GetNatMethods().LoadNatMethod("H46024");
	xnat->Start(server,this);
	EP->GetNatMethods().AddMethod(xnat);
}

bool H460_FeatureStd23::IsAvailable()
{
	return isavailable;
}

void H460_FeatureStd23::DelayedReRegistration()
{
	 RegThread = PThread::Create(PCREATE_NOTIFIER(RegMethod), 0,
					PThread::AutoDeleteThread,
					PThread::NormalPriority,
					"regmeth23");
}

void H460_FeatureStd23::RegMethod(PThread &, INT)
{
	PProcess::Sleep(1000);
	EP->ForceGatekeeperReRegistration();  // We have an ALG so notify the gatekeeper   
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

}

H460_FeatureStd24::~H460_FeatureStd24()
{
}

void H460_FeatureStd24::AttachEndPoint(H323EndPoint * _ep)
{
   EP = _ep; 
	// We only enable IF the gatekeeper supports H.460.23
	H460_FeatureSet * gkfeat = EP->GetGatekeeperFeatures();
	if (gkfeat && gkfeat->HasFeature(23) && 
			((H460_FeatureStd23 *)gkfeat->GetFeature(23))->IsAvailable()) {
		isEnabled = true;
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
    H460_FeatureStd & feat = H460_FeatureStd(24);

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

PBoolean H460_FeatureStd24::OnSendSetup_UUIE(H225_FeatureDescriptor & pdu)
{ 
  // Ignore if already not enabled or manually using STUN
  if (!isEnabled) 
		return FALSE;

 PTRACE(6,"Std24\tSend Setup");
	if (natconfig == H460_FeatureStd24::e_unknown)
		return FALSE;

    H460_FeatureStd & feat = H460_FeatureStd(24);
    
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
				CON->SetRemoteNAT();
				SetNATMethods(e_enable);
				break;

			case H460_FeatureStd24::e_remoteMaster:
				PTRACE(4,"Std24\tRemote NAT Support: ALL NAT DISABLED");
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

			case H460_FeatureStd24::e_natSameNAT:
				PTRACE(4,"Std24\tSame NAT: H.460.24 AnnexA ENABLED");
#ifdef H323_H46024A
				CON->H46024AEnabled();
#endif
				SetNATMethods(e_AnnexA);
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

	if (CON->GetFeatureSet()->HasFeature(19)) {
		H460_Feature * feat = CON->GetFeatureSet()->GetFeature(19);

		PTRACE(4,"H46024\t" << (state ? "En" : "Dis") << "abling H.460.19 support for call");
		((H460_FeatureStd19 *)feat)->SetAvailable(state);
	}
}

void H460_FeatureStd24::SetNATMethods(H46024NAT state)
{

	PNatList & natlist = EP->GetNatMethods().GetNATList();

	if ((state == H460_FeatureStd24::e_default) ||
         (state == H460_FeatureStd24::e_AnnexA))
			SetH46019State(true);
	else
			SetH46019State(false);

	for (PINDEX i=0; i< natlist.GetSize(); i++)
	{
		switch (state) {
			case H460_FeatureStd24::e_AnnexA:   // To do Annex A Implementation.
			case H460_FeatureStd24::e_default:
				if (natlist[i].GetName() == "H46024")
					natlist[i].Activate(false);
				else
					natlist[i].Activate(true);

				break;
			case H460_FeatureStd24::e_enable:
				if (natlist[i].GetName() == "H46024")
					natlist[i].Activate(true);
				else
					natlist[i].Activate(false);
				break;
			case H460_FeatureStd24::e_disable:
				natlist[i].Activate(false);
				break;
			default:
				break;
		}
	}
}

PString H460_FeatureStd24::GetNATStrategyString(NatInstruct method)
{
  static const char * const Names[9] = {
		"Unknown Strategy",
		"No Assistance",
		"Local Master",
		"Remote Master",
		"Local Proxy",
		"Remote Proxy",
		"Full Proxy",
		"Same NAT/FW",
		"Call Failure!"
  };

  if (method < 9)
    return Names[method];

   return psprintf("<NAT Strategy %u>", method);
}

#ifdef _MSC_VER
#pragma warning(default : 4239)
#endif

#endif // Win32_WCE