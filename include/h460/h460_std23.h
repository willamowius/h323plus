/* H460_std23.h
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
 * $Id $
 *
 */

#ifndef H_H460_Featurestd23
#define H_H460_Featurestd23


#include <h460/h4601.h>
#include <ptlib/plugin.h>
#include <ptclib/pstun.h>

#if _MSC_VER
#pragma once
#endif

class H323EndPoint;
class H460_FeatureStd23;
class PNatMethod_H46024  : public PSTUNClient,
						   public PThread
{
	PCLASSINFO(PNatMethod_H46024, PNatMethod);

	public:
		PNatMethod_H46024();

		~PNatMethod_H46024();

		static PStringList GetNatMethodName() {  return PStringArray("H46024"); };

		virtual PString GetName() const
				{ return GetNatMethodName()[0]; }

		// Start the Nat Method testing
		void Start(const PString & server,H460_FeatureStd23 * _feat);

		// Main thread testing
		void Main();

		// Whether the NAT method is Available
		virtual bool IsAvailable(
				const PIPSocket::Address & binding = PIPSocket::GetDefaultIpAny()  ///< Interface to see if NAT is available on
		);

		// Create the socket pair
		virtual PBoolean CreateSocketPair(
		  PUDPSocket * & socket1,
		  PUDPSocket * & socket2,
		  const PIPSocket::Address & binding = PIPSocket::GetDefaultIpAny()
		);

		// Whether the NAT Method is available
		void SetAvailable();

		// Whether the NAT method is activated for this call
		virtual void Activate(bool act);

		// Reportable NAT Type
		PSTUNClient::NatTypes GetNATType();

#if PTLIB_VER >= 2110
    virtual PString GetServer() const { return PString(); }
    virtual bool GetServerAddress(PIPSocketAddressAndPort & ) const { return false; }
    virtual NatTypes GetNatType(bool) { return UnknownNat; }
    virtual NatTypes GetNatType(const PTimeInterval &) { return UnknownNat; }
    virtual bool SetServer(const PString &) { return false; }
    virtual bool Open(const PIPSocket::Address &) { return false; }
    virtual bool CreateSocket(BYTE,PUDPSocket * &, const PIPSocket::Address,WORD)  { return false; }
    virtual void SetCredentials(const PString &, const PString &, const PString &) {}
#endif


protected:
		// Do a NAT test
        PSTUNClient::NatTypes NATTest();

    private:
		bool					isActive;
		bool					isAvailable;
		PSTUNClient::NatTypes	natType;
		H460_FeatureStd23 *		feat;
};

///////////////////////////////////////////////////////////////////////////////

class H460_FeatureStd23 : public H460_FeatureStd 
{
    PCLASSINFO(H460_FeatureStd23, H460_FeatureStd);

public:

    H460_FeatureStd23();
    virtual ~H460_FeatureStd23();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);

    static PStringArray GetFeatureName() { return PStringArray("Std23"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("P2Pnat Detect-H.460.23"); };
    static int GetPurpose()	{ return FeatureRas; };
	static PStringArray GetIdentifier() { return PStringArray("23"); };

	virtual PBoolean CommonFeature() { return isEnabled; }

	// Messages
	// GK -> EP
    virtual PBoolean OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveGatekeeperConfirm(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendRegistrationRequest(H225_FeatureDescriptor & pdu);
	virtual void OnReceiveRegistrationConfirm(const H225_FeatureDescriptor & pdu);

	H323EndPoint * GetEndPoint() { return (H323EndPoint *)EP; }

	// Reporting the NAT Type
	void OnNATTypeDetection(PSTUNClient::NatTypes type, const PIPSocket::Address & ExtIP);

	bool IsAvailable();

	bool AlternateNATMethod();
	bool UseAlternate();

#ifdef H323_UPnP
	void InitialiseUPnP();
#endif

protected:
	bool DetectALG(const PIPSocket::Address & detectAddress);
	void StartSTUNTest(const PString & server);
	
	void DelayedReRegistration();
 
private:
    H323EndPoint *			EP;
	PSTUNClient::NatTypes	natType;
	PIPSocket::Address		externalIP;
    PBoolean				natNotify;
	PBoolean				alg;
	PBoolean				isavailable;
	PBoolean				isEnabled; 
	int						useAlternate;

	// Delayed Reregistration
    PThread  *  RegThread;
    PDECLARE_NOTIFIER(PThread, H460_FeatureStd23, RegMethod);

};

// Need to declare for Factory Loader
#if !defined(_WIN32_WCE)
	#if PTLIB_VER > 260
	   PPLUGIN_STATIC_LOAD(Std23, H460_Feature);
	#else
	   PWLIB_STATIC_LOAD_PLUGIN(Std23, H460_Feature);
	#endif
#endif


////////////////////////////////////////////////////////

class H323EndPoint;
class H323Connection;
class H460_FeatureStd24 : public H460_FeatureStd 
{
    PCLASSINFO(H460_FeatureStd24, H460_FeatureStd);

public:

    H460_FeatureStd24();
    virtual ~H460_FeatureStd24();

    // Universal Declarations Every H460 Feature should have the following
    virtual void AttachEndPoint(H323EndPoint * _ep);
    virtual void AttachConnection(H323Connection * _ep);

    static PStringArray GetFeatureName() { return PStringArray("Std24"); };
    static PStringArray GetFeatureFriendlyName() { return PStringArray("P2Pnat Media-H.460.24"); };
    static int GetPurpose()	{ return FeatureSignal; };
	static PStringArray GetIdentifier() { return PStringArray("24"); };

	virtual PBoolean CommonFeature() { return isEnabled; }

	enum NatInstruct {
		e_unknown,
		e_noassist,
		e_localMaster,
	    e_remoteMaster,
		e_localProxy,
	    e_remoteProxy,
        e_natFullProxy,
		e_natAnnexA,				// Same NAT
		e_natAnnexB,				// NAT Offload
		e_natFailure = 100
	};

    static PString GetNATStrategyString(NatInstruct method);

	enum H46024NAT {
		e_default,		// This will use the underlying NAT Method
		e_enable,		// Use H.460.24 method (STUN)
		e_AnnexA,       // Disable H.460.24 method but initiate AnnexA
		e_AnnexB,		// Disable H.460.24 method but initiate AnnexB
		e_disable		// Disable all and remote will do the NAT help		
	};

	// Messages
    virtual PBoolean OnSendAdmissionRequest(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu);
	virtual void OnReceiveAdmissionReject(const H225_FeatureDescriptor & pdu);

    virtual PBoolean OnSendSetup_UUIE(H225_FeatureDescriptor & pdu);
    virtual void OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu);

protected:
	void HandleNATInstruction(NatInstruct natconfig);
	void SetNATMethods(H46024NAT state);
	void SetH46019State(bool state);
 
private:
    H323EndPoint * EP;
	H323Connection * CON;
	NatInstruct natconfig;
	PMutex h460mute;
	int nattype;
	bool isEnabled;
	bool useAlternate;


};

inline ostream & operator<<(ostream & strm, H460_FeatureStd24::NatInstruct method) { return strm << H460_FeatureStd24::GetNATStrategyString(method); }

// Need to declare for Factory Loader
#if !defined(_WIN32_WCE)
	#if PTLIB_VER > 260
	   PPLUGIN_STATIC_LOAD(Std24, H460_Feature);
	#else
	   PWLIB_STATIC_LOAD_PLUGIN(Std24, H460_Feature);
	#endif
#endif

#endif 

