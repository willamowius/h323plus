/* h460_std25.cxx
 *
 * Copyright (c) 2014 Spranto International Pte Ltd. All Rights Reserved.
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
 * The Initial Developer of the Original Code is ISVO (Asia) Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include "ptlib.h"
#include "openh323buildopts.h"

#ifdef H323_H46025

#include <h323ep.h>
#include <h323con.h>
#include <h460/h460_std25.h>
#include <h460/h460_std25_pidf_lo.h>
#include <h460/h460.h>


#ifdef _MSC_VER
#pragma warning(disable : 4239)
#endif

// Must Declare for Factory Loader.
H460_FEATURE(Std25);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
H460_FeatureStd25::H460_FeatureStd25()
: H460_FeatureStd(25)
{
 PTRACE(6,"Std25\tInstance Created");

 enabled = false;
 dataToSend = false;
 EP = NULL;
 CON = NULL;
 FeatureCategory = FeatureSupported;
}

H460_FeatureStd25::~H460_FeatureStd25()
{
}

void H460_FeatureStd25::AttachEndPoint(H323EndPoint * _ep)
{
  PTRACE(6,"Std25\tEndpoint Attached");
   EP = _ep; 
   enabled = EP->H46025IsEnabled();
}

void H460_FeatureStd25::AttachConnection(H323Connection * _con)
{
   CON = _con;
}

PBoolean H460_FeatureStd25::FeatureAdvertised(int mtype)
{
     switch (mtype) {
        case H460_MessageType::e_admissionRequest:
        case H460_MessageType::e_admissionConfirm:
        case H460_MessageType::e_admissionReject:
            return true;
        default:
            return false;
     }
}

PBoolean H460_FeatureStd25::OnSendAdmissionRequest(H225_FeatureDescriptor & pdu)
{
    if (!enabled)
        return false;

	// Build Message
    H460_FeatureStd feat = H460_FeatureStd(25); 
    
    pdu = feat;
	return true;
}

void H460_FeatureStd25::OnReceiveAdmissionConfirm(const H225_FeatureDescriptor & pdu)
{
    dataToSend = true;
}

PBoolean H460_FeatureStd25::OnSendSetup_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!enabled)
        return false;

    if (!dataToSend)
        return false;

    H460_FeatureStd feat = H460_FeatureStd(25); 

    PString pidf_lo;
    if (Build_PIDF_LO_Message(pidf_lo)) {
        PASN_OctetString data;
        data.SetValue(pidf_lo);
        feat.Add(1,H460_FeatureContent(data));

        feat.Add(2,H460_FeatureContent(0,8));
        feat.Add(3,H460_FeatureContent(1,8));
        pdu = feat;
	    return true;
    }
    return false;
}

void H460_FeatureStd25::OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu)
{
    if (!enabled)
        return;

    dataToSend = true;
}

PBoolean H460_FeatureStd25::OnSendAlerting_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!dataToSend)
        return false;

    H460_FeatureStd feat = H460_FeatureStd(25);

    PString pidf_lo;
    if (Build_PIDF_LO_Message(pidf_lo)) {
        PASN_OctetString data;
        data.SetValue(pidf_lo);
        feat.Add(1,H460_FeatureContent(data));

        feat.Add(2,H460_FeatureContent(0,8));
        feat.Add(3,H460_FeatureContent(1,8));
        pdu = feat;
	    return true;
    }
  
    pdu = feat;
    dataToSend = false;
	return true;
}

void H460_FeatureStd25::OnReceiveAlerting_UUIE(const H225_FeatureDescriptor & pdu)
{

}

PBoolean H460_FeatureStd25::OnSendCallConnect_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!dataToSend)
        return false;

    H460_FeatureStd feat = H460_FeatureStd(25); 

    PString pidf_lo;
    if (Build_PIDF_LO_Message(pidf_lo)) {
        PASN_OctetString data;
        data.SetValue(pidf_lo);
        feat.Add(1,H460_FeatureContent(data));

        feat.Add(2,H460_FeatureContent(0,8));
        feat.Add(3,H460_FeatureContent(1,8));
        pdu = feat;
	    return true;
    }
  
    pdu = feat;
    dataToSend = false;
	return true;
}

void H460_FeatureStd25::OnReceiveCallConnect_UUIE(const H225_FeatureDescriptor & pdu)
{

}

PBoolean H460_FeatureStd25::Build_PIDF_LO_Message(const PString & message)
{
    H323_H46025_Message::Device device;
    if (EP && EP->GetDeviceInformation(device)) {
        H323_H46025_Message myDevice(device);

        H323_H46025_Message::Civic civic;
        if (EP->GetCivicInformation(civic)) 
            myDevice.AddCivic(civic);

        H323_H46025_Message::Geodetic gps;
        if (EP->GetGPSInformation(gps))
            myDevice.AddGeodetic(gps);

        return true;
    }
    return false;
}


#ifdef _MSC_VER
#pragma warning(default : 4239)
#endif

#endif

