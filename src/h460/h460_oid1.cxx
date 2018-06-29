/*
* h460_oid1.cxx
*
* Copyright (c) 2015 Spranto International Pte Ltd. All Rights Reserved.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, you can obtain one at http://mozilla.org/MPL/2.0/
*
* Software distributed under the License is distributed on an "AS IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
* the License for the specific language governing rights and limitations
* under the License.
*
* Contributor(s): ______________________________________.
*
*
*/


#include <ptlib.h>
#include <openh323buildopts.h>

#ifdef H323_H460IM

#include <h323.h>
#include <h323pdu.h>

#include "h460/h460_oid1.h"

#include "h460/h460tm.h"

static const char * baseOID  = "1.3.6.1.4.1.17090.0.1";      // Advertised Feature
static const char * typeOID  = "1";                          // Type 1-IM session
//static const char * encOID   = "3";                          // Support Encryption
static const char * OpenOID  = "4";                          // Message Session open/close
static const char * MsgOID   = "5";                          // Message contents
//static const char * WriteOID  = "6";                         // Message write event
//static const char * MultiModeOID  = "7";                     // MultipointMode Message
//static const char * MultiMessageOID  = "8";					 // Multipoint Message
//static const char * EncMsgOID  = "10";                       // Encrypt Message


//static const char * RegOID     = "10";
//static const char * RegIDOID   = "10.1";
//static const char * RegPwdOID  = "10.2";

#ifdef _MSC_VER
#pragma warning(disable : 4239)
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// Must Declare for Factory Loader.
H460_FEATURE(OID1);

H460_FeatureOID1::H460_FeatureOID1()
: H460_FeatureOID(baseOID)
{
 PTRACE(6,"OID1\tInstance Created");

 remoteSupport = false;
 callToken = PString();
 sessionOpen = false;
 m_ep = NULL;
 m_con = NULL;

 FeatureCategory = FeatureSupported;

}

H460_FeatureOID1::~H460_FeatureOID1()
{
}

PStringArray H460_FeatureOID1::GetIdentifier()
{
	return PStringArray(baseOID);
}

void H460_FeatureOID1::AttachEndPoint(H323EndPoint * _ep)
{
    PTRACE(6,"OID1\tEndPoint Attached");

    m_ep = _ep;
}

void H460_FeatureOID1::AttachConnection(H323Connection * _con)
{
   m_con = _con;
   callToken = _con->GetCallToken();
   _con->DisableH245inSETUP();     // We don't have H.245 in Setup with Text Messaging
}

PBoolean H460_FeatureOID1::SupportNonCallService() const
{
	return true;
}

PBoolean H460_FeatureOID1::OnSendSetup_UUIE(H225_FeatureDescriptor & pdu)
{

  if (m_ep->IMisDisabled())
	  return false;

  // Set Call Token
  callToken = m_con->GetCallToken();
  PBoolean imcall = m_ep->IsIMCall();

  // Build Message
  H460_FeatureOID feat = H460_FeatureOID(baseOID);

  // Is a IM session call
  if (imcall) {
      m_con->SetIMCall(true);
      sessionOpen = !m_con->IMSession();            // set flag to open session
      feat.Add(typeOID,H460_FeatureContent(1,8));   // 1 specify Instant Message Call
      m_ep->SetIMCall(false);
  }

  // Attach PDU
  pdu = feat;

  return true;
}

void H460_FeatureOID1::OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu)
{

  if (m_ep->IMisDisabled())
	  return;

   callToken = m_con->GetCallToken();

   H460_FeatureOID & feat = (H460_FeatureOID &)pdu;

   if (feat.Contains(typeOID)) {  // This is a Non Call Service
	      m_con->DisableH245inSETUP();    // Turn off H245 Tunnelling in Setup

     unsigned calltype = feat.Value(typeOID);
	 if (calltype == 1) {  // IM Call
         m_con->SetIMCall(true);
	     m_con->SetNonCallConnection();
	 }
   }

   m_con->SetIMSupport(true);
   m_ep->IMSupport(callToken);
   remoteSupport = true;

}

PBoolean H460_FeatureOID1::OnSendCallProceeding_UUIE(H225_FeatureDescriptor & pdu)
{
    if (remoteSupport) {
      H460_FeatureOID feat = H460_FeatureOID(baseOID);
      pdu = feat;
    }

	return remoteSupport;
}


void H460_FeatureOID1::OnReceiveCallProceeding_UUIE(const H225_FeatureDescriptor & pdu)
{
   remoteSupport = true;
}

PBoolean H460_FeatureOID1::OnSendAlerting_UUIE(H225_FeatureDescriptor & pdu)
{
    if (remoteSupport) {
      // Build Message
      H460_FeatureOID feat = H460_FeatureOID(baseOID);

      // Signal to say ready for message
      if (m_con->IMCall())
          feat.Add(typeOID,H460_FeatureContent(1,8));   // Notify ready for message

       // Attach PDU
       pdu = feat;
    }

	return remoteSupport;
}

void H460_FeatureOID1::OnReceiveAlerting_UUIE(const H225_FeatureDescriptor & pdu)
{
   remoteSupport = true;
   m_ep->IMSupport(callToken);
   m_con->SetIMSession(true);

   H460_FeatureOID & feat = (H460_FeatureOID &)pdu;

    //   if Remote Ready for Non-Call
    if (feat.Contains(typeOID)) {
       unsigned calltype = feat.Value(typeOID);

       //    if IM session send facility msg
       if (calltype == 1) {
         H323SignalPDU facilityPDU;
         facilityPDU.BuildFacility(*m_con, false,H225_FacilityReason::e_featureSetUpdate);
         m_con->WriteSignalPDU(facilityPDU);
       }
    }
}


// Send Message
PBoolean H460_FeatureOID1::OnSendFacility_UUIE(H225_FeatureDescriptor & pdu)
{

    if (!remoteSupport)
        return false;

    // Build Message
    H460_FeatureOID feat = H460_FeatureOID(baseOID);
    PBoolean contents = false;

    // Open and Closing Session
    if ((sessionOpen != m_con->IMSession())) {
        sessionOpen = m_con->IMSession();
        feat.Add(OpenOID,H460_FeatureContent(sessionOpen));

        contents = true;
    }

    // If Message send as Unicode String
    PString msg = m_con->IMMsg();
    if (msg.GetLength() > 0) {
        PASN_BMPString str;
        str.SetValue(msg);
        feat.Add(MsgOID,H460_FeatureContent(str));
        m_con->SetIMMsg(PString());
        contents = true;
    }

    if (contents) {
        pdu = feat;

    if (m_con->IMCall() && !sessionOpen) {
        if (feat.Contains(MsgOID))
 		    m_ep->IMSent(callToken,true);
	    else
		    m_ep->IMSessionClosed(callToken);
	    }
        return true;
    }

    return false;
};

// Receive Message
void H460_FeatureOID1::OnReceiveFacility_UUIE(const H225_FeatureDescriptor & pdu)
{
   H460_FeatureOID & feat = (H460_FeatureOID &)pdu;
   PBoolean open = false;

   if (feat.Contains(OpenOID)) {
	   open = feat.Value(OpenOID);
	   if (open) {
             m_ep->IMSessionOpen(callToken);

             m_con->SetIMSession(true);
             m_con->SetCallAnswered();   // Set Flag to specify call is answered
	   } else {
		   if (m_con->IMSession()) {
                    m_ep->IMSessionClosed(callToken);
		   }

             m_con->SetIMSession(false);
	   }
	   sessionOpen = open;
   }

   if (feat.Contains(MsgOID)) {

	  PASN_BMPString & str = feat.Value(MsgOID);
	  PString msg = str.GetValue();
      PTRACE(6,"TLS\tReceived " << msg << " " << msg.GetLength());

      m_ep->IMReceived(callToken,msg,m_con->IMSession());
   }

   if (!m_con->IMCall())   // Message in an existing connection
	   return;

   if (open) {
       H323SignalPDU connectPDU;
       connectPDU.BuildConnect(*m_con);
       m_con->WriteSignalPDU(connectPDU); // Send H323 Connect PDU
   } else if (!m_con->IMSession()) {
       m_con->ClearCall();    // Send Release Complete
   }

};

// You end connection
PBoolean H460_FeatureOID1::OnSendReleaseComplete_UUIE(H225_FeatureDescriptor & pdu)
{
	if (sessionOpen) {
		if (m_con->IMSession())
           m_ep->IMSessionClosed(callToken);
	    sessionOpen = false;

        // Build Message
        H460_FeatureOID feat = H460_FeatureOID(baseOID);
        feat.Add(OpenOID,H460_FeatureContent(sessionOpen));
        pdu = feat;
        return true;
	}

   return false;
};

// Other person ends connection
void H460_FeatureOID1::OnReceiveReleaseComplete_UUIE(const H225_FeatureDescriptor & pdu)
{
   H460_FeatureOID & feat = (H460_FeatureOID &)pdu;

    if (sessionOpen && feat.Contains(OpenOID)) {
      PBoolean open = feat.Value(OpenOID);
	if (!open) {
          sessionOpen = false;
          m_ep->IMSessionClosed(callToken);
        }
    }
};

PBoolean H460_FeatureOID1::OnSendAdmissionRequest(H225_FeatureDescriptor & pdu)
{
   if (m_con->IMCall())   // Message in an IM Call
   {
       H460_FeatureOID feat = H460_FeatureOID(baseOID);
       feat.Add(typeOID,H460_FeatureContent(1,8));   // 1 specify Instant Message Call
       pdu = feat;
	   return true;
   }

	return false;
}

#ifdef _MSC_VER
#pragma warning(default : 4239)
#endif

#endif

