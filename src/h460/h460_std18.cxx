/* H460_std18.cxx
 *
 * h323plus library
 *
 * Copyright (c) 2008 ISVO (Asia) Pte. Ltd.
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
 * Portions of this code were written with the assisance of funding from
 * triple-IT. http://www.triple-it.nl.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 *
 */

#include "ptlib.h"
#include "openh323buildopts.h"

#ifdef H323_H46018

#include <h323ep.h>
#include <h323pdu.h>
#include <h460/h460_std18.h>
#include <h460/h46018_h225.h>

#if _WIN32
#pragma message("H.460.18/.19 Enabled. See Tandberg Patent License. http://www.tandberg.com/collateral/tandberg-ITU-license.pdf")
#else
#warning("H.460.18/.19 Enabled. See Tandberg Patent License. http://www.tandberg.com/collateral/tandberg-ITU-license.pdf")
#endif

///////////////////////////////////////////////////////
// H.460.18
//
// Must Declare for Factory Loader.
H460_FEATURE(Std18);

H460_FeatureStd18::H460_FeatureStd18()
: H460_FeatureStd(18)
{
    PTRACE(6,"Std18\tInstance Created");

    EP = NULL;
    handler = NULL;
    isEnabled = FALSE;
    FeatureCategory = FeatureSupported;

}

H460_FeatureStd18::~H460_FeatureStd18()
{
    if (handler != NULL)
        delete handler;
}

void H460_FeatureStd18::AttachEndPoint(H323EndPoint * _ep)
{
    EP =_ep; 
    handler = NULL;

    isEnabled = EP->H46018IsEnabled();

    if (isEnabled) {
        PTRACE(6,"Std18\tEnabling and Initialising H.460.18 Handler");
        handler = new H46018Handler(*EP);
    }
}

PBoolean H460_FeatureStd18::OnSendGatekeeperRequest(H225_FeatureDescriptor & pdu) 
{ 
    if (!isEnabled)
        return false;

    // Ignore if already manually using STUN
    PNatStrategy & natMethods = EP->GetNatMethods();
    const PNatList & list = natMethods.GetNATList();
    if (list.GetSize() > 0) {
      for (PINDEX i=0; i < list.GetSize(); i++) {  
          if (list[i].GetName() == "STUN" && list[i].IsAvailable()) {
             return false;
        }
      }
    }

    H460_FeatureStd feat = H460_FeatureStd(18); 
    pdu = feat;
    return TRUE; 
}
    
void H460_FeatureStd18::OnReceiveGatekeeperConfirm(const H225_FeatureDescriptor & pdu) 
{
    isEnabled = true;
}

PBoolean H460_FeatureStd18::OnSendRegistrationRequest(H225_FeatureDescriptor & pdu) 
{
    if (!isEnabled)
        return FALSE;

    H460_FeatureStd feat = H460_FeatureStd(18); 
    pdu = feat;
    return TRUE; 
}

void H460_FeatureStd18::OnReceiveRegistrationConfirm(const H225_FeatureDescriptor & pdu) 
{
    isEnabled = true;
    handler->Enable();
    EP->H46018Received();
}

void H460_FeatureStd18::OnReceiveServiceControlIndication(const H225_FeatureDescriptor & pdu) 
{
    if (handler == NULL)
        return;

    H460_FeatureStd & feat = (H460_FeatureStd &)pdu;

    if (!feat.Contains(H460_FeatureID(1))) {
        PTRACE(4,"Std18\tERROR: Received SCI without Call Indication!");
        return;
    }
 
    PTRACE(4,"Std18\tSCI: Processing Incoming call request.");
    PASN_OctetString raw = feat.Value(H460_FeatureID(1));
    handler->CreateH225Transport(raw);

}


///////////////////////////////////////////////////////
// H.460.19
//
// Must Declare for Factory Loader.
H460_FEATURE(Std19);

#define H46019_Multiplex      1
#define H46019_MultiServer    2

H460_FeatureStd19::H460_FeatureStd19()
: H460_FeatureStd(19), EP(NULL), CON(NULL), isEnabled(false),
    isAvailable(true), remoteSupport(false), multiSupport(false)
{
    PTRACE(6,"Std19\tInstance Created");

    FeatureCategory = FeatureSupported;
}

H460_FeatureStd19::~H460_FeatureStd19()
{
}

void H460_FeatureStd19::AttachEndPoint(H323EndPoint * _ep)
{
    PTRACE(6,"Std19\tEndPoint Attached");
    EP = _ep; 
    // We only enable IF the gatekeeper supports H.460.18
    H460_FeatureSet * gkfeat = EP->GetGatekeeperFeatures();
    if (gkfeat && gkfeat->HasFeature(18)) {
        isEnabled = true;
    } else {
        PTRACE(4,"Std19\tH.460.19 disabled as GK does not support H.460.18");
        isEnabled = false;
    }
}

void H460_FeatureStd19::AttachConnection(H323Connection * _con)
{
    CON = _con;
}

PBoolean H460_FeatureStd19::OnSendSetup_UUIE(H225_FeatureDescriptor & pdu) 
{ 
    if (!isEnabled || !isAvailable)
        return FALSE;

    CON->H46019Enabled();
    H460_FeatureStd feat = H460_FeatureStd(19); 
#ifdef H323_H46019M
    feat.Add(H46019_Multiplex);
#endif
    pdu = feat;
    return TRUE; 
}

void H460_FeatureStd19::OnReceiveSetup_UUIE(const H225_FeatureDescriptor & pdu) 
{
    if (isEnabled && isAvailable) {
        remoteSupport = TRUE;
        CON->H46019Enabled();
        CON->H46019SetCallReceiver();
#ifdef H323_H46019M
        H460_FeatureStd & feat = (H460_FeatureStd &)pdu;
        if (feat.Contains(H46019_Multiplex)) { 
             EnableMultiplex();
             multiSupport = true;
        }
        
        /// MultiServer?
        // if (feat.Contains(H46019_MultiServer))
        //    Do we need to do anything? - SH
#endif
    }
}

PBoolean H460_FeatureStd19::OnSendCallProceeding_UUIE(H225_FeatureDescriptor & pdu) 
{ 
    if (!isEnabled || !isAvailable || !remoteSupport)
        return FALSE;

    H460_FeatureStd feat = H460_FeatureStd(19); 
#ifdef H323_H46019M
    if (multiSupport)
         feat.Add(H46019_Multiplex);
#endif
    pdu = feat;
    return TRUE; 
}

void H460_FeatureStd19::OnReceiveCallProceeding_UUIE(const H225_FeatureDescriptor & pdu) 
{
//    if (isEnabled && isAvailable) {
//        remoteSupport = TRUE;
//        CON->H46019Enabled();
//#ifdef H323_H46019M
//        H460_FeatureStd & feat = (H460_FeatureStd &)pdu;
//        if (feat.Contains(H46019_Multiplex)) {
//             EnableMultiplex();
//             multiSupport = true;
//        }
//#endif
//    }
}

PBoolean H460_FeatureStd19::OnSendAlerting_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!isEnabled || !isAvailable || !remoteSupport)
        return FALSE;

    H460_FeatureStd feat = H460_FeatureStd(19); 
#ifdef H323_H46019M
    if (multiSupport)
         feat.Add(H46019_Multiplex);
#endif
    pdu = feat;
    return TRUE; 
}

void H460_FeatureStd19::OnReceiveAlerting_UUIE(const H225_FeatureDescriptor & pdu) 
{ 
    if (!remoteSupport) {
        remoteSupport = TRUE;
        CON->H46019Enabled();
#ifdef H323_H46019M
        H460_FeatureStd & feat = (H460_FeatureStd &)pdu;
        if (feat.Contains(H46019_Multiplex)) {
             EnableMultiplex();
             multiSupport = true;
        }
#endif
    } 
}

PBoolean H460_FeatureStd19::OnSendCallConnect_UUIE(H225_FeatureDescriptor & pdu)
{
    if (!isEnabled || !isAvailable || !remoteSupport)
        return FALSE;

    H460_FeatureStd feat = H460_FeatureStd(19); 
#ifdef H323_H46019M
    if (multiSupport)
         feat.Add(H46019_Multiplex);
#endif
    pdu = feat;
    return TRUE; 
}

void H460_FeatureStd19::OnReceiveCallConnect_UUIE(const H225_FeatureDescriptor & pdu) 
{
    if (!remoteSupport) {
        remoteSupport = TRUE;
        CON->H46019Enabled();
#ifdef H323_H46019M
        H460_FeatureStd & feat = (H460_FeatureStd &)pdu;
        if (feat.Contains(H46019_Multiplex)) {
             EnableMultiplex();
             multiSupport = true;
        }
#endif
    }
}

void H460_FeatureStd19::SetAvailable(bool avail)
{
    isAvailable = avail;
}

void H460_FeatureStd19::EnableMultiplex()
{
    CON->H46019MultiEnabled();
    const PNatList & list = EP->GetNatMethods().GetNATList();
    if (list.GetSize() > 0) {
      bool h24Active= false;
      for (PINDEX i=0; i < list.GetSize(); i++) {  
          if (((list[i].GetName() == "H46024") || (list[i].GetName() == "UPnP")) &&
              list[i].IsAvailable(PIPSocket::Address::GetAny(4))) {
                  h24Active= true;
                  break;
          }
      }
      if (h24Active) 
          return;

      for (PINDEX i=0; i < list.GetSize(); i++) {  
          if (list[i].GetName() == "H46019" &&
              !list[i].IsAvailable(PIPSocket::Address::GetAny(4))) {
                  PTRACE(4,"Std19\tActivating Multiplexing for call");
                  list[i].Activate(true);
                  break;
          }
      }
    }
}

#endif
