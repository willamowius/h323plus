/*
 * h225ras.h
 *
 * H.225 RAS protocol handler
 *
 * Open H323 Library
 *
 * Copyright (c) 2001 Equivalence Pty. Ltd.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Open H323 Library.
 *
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Portions of this code were written with the assisance of funding from
 * iFace, Inc. http://www.iface.com
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef __OPAL_H225RAS_H
#define __OPAL_H225RAS_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include "openh323buildopts.h"
#include "transports.h"
#include "h235auth.h"
#include "h323trans.h"
#include "svcctrl.h"

class PASN_Sequence;
class PASN_Choice;

class H225_GatekeeperRequest;
class H225_GatekeeperConfirm;
class H225_GatekeeperReject;
class H225_RegistrationRequest;
class H225_RegistrationConfirm;
class H225_RegistrationReject;
class H225_UnregistrationRequest;
class H225_UnregistrationConfirm;
class H225_UnregistrationReject;
class H225_AdmissionRequest;
class H225_AdmissionConfirm;
class H225_AdmissionReject;
class H225_BandwidthRequest;
class H225_BandwidthConfirm;
class H225_BandwidthReject;
class H225_DisengageRequest;
class H225_DisengageConfirm;
class H225_DisengageReject;
class H225_LocationRequest;
class H225_LocationConfirm;
class H225_LocationReject;
class H225_InfoRequest;
class H225_InfoRequestResponse;
class H225_NonStandardMessage;
class H225_UnknownMessageResponse;
class H225_RequestInProgress;
class H225_ResourcesAvailableIndicate;
class H225_ResourcesAvailableConfirm;
class H225_InfoRequestAck;
class H225_InfoRequestNak;
class H225_ArrayOf_CryptoH323Token;
class H225_FeatureSet;

class H323EndPoint;
class H323RasPDU;



///////////////////////////////////////////////////////////////////////////////

/**This class embodies the H.225.0 RAS protocol to/from gatekeepers.
  */
class H225_RAS : public H323Transactor
{
  PCLASSINFO(H225_RAS, H323Transactor);
  public:
  /**@name Construction */
  //@{
    enum {
      DefaultRasMulticastPort = 1718,
      DefaultRasUdpPort = 1719
    };

    /**Create a new protocol handler.
     */
    H225_RAS(
      H323EndPoint & endpoint,  ///< Endpoint gatekeeper is associated with.
      H323Transport * transport ///< Transport over which gatekeepers communicates.
    );

    /**Destroy protocol handler.
     */
    ~H225_RAS();
  //@}

  /**@name Overrides from PObject */
  //@{
    /**Print the name of the gatekeeper.
      */
    void PrintOn(
      ostream & strm    ///< Stream to print to.
    ) const;
  //@}

  /**@name Overrides from H323Transactor */
  //@{
    /**Create the transaction PDU for reading.
      */
    virtual H323TransactionPDU * CreateTransactionPDU() const;

    /**Handle and dispatch a transaction PDU
      */
    virtual PBoolean HandleTransaction(
      const PASN_Object & rawPDU
    );

    /**Allow for modifications to PDU on send.
      */
    virtual void OnSendingPDU(
      PASN_Object & rawPDU
    );
  //@}

  /**@name Protocol callbacks */
  //@{
    virtual void OnSendGatekeeperRequest(H323RasPDU &, H225_GatekeeperRequest &);
    virtual void OnSendGatekeeperConfirm(H323RasPDU &, H225_GatekeeperConfirm &);
    virtual void OnSendGatekeeperReject(H323RasPDU &, H225_GatekeeperReject &);
    virtual void OnSendGatekeeperRequest(H225_GatekeeperRequest &);
    virtual void OnSendGatekeeperConfirm(H225_GatekeeperConfirm &);
    virtual void OnSendGatekeeperReject(H225_GatekeeperReject &);
    virtual PBoolean OnReceiveGatekeeperRequest(const H323RasPDU &, const H225_GatekeeperRequest &);
    virtual PBoolean OnReceiveGatekeeperConfirm(const H323RasPDU &, const H225_GatekeeperConfirm &);
    virtual PBoolean OnReceiveGatekeeperReject(const H323RasPDU &, const H225_GatekeeperReject &);
    virtual PBoolean OnReceiveGatekeeperRequest(const H225_GatekeeperRequest &);
    virtual PBoolean OnReceiveGatekeeperConfirm(const H225_GatekeeperConfirm &);
    virtual PBoolean OnReceiveGatekeeperReject(const H225_GatekeeperReject &);

    virtual void OnSendRegistrationRequest(H323RasPDU &, H225_RegistrationRequest &);
    virtual void OnSendRegistrationConfirm(H323RasPDU &, H225_RegistrationConfirm &);
    virtual void OnSendRegistrationReject(H323RasPDU &, H225_RegistrationReject &);
    virtual void OnSendRegistrationRequest(H225_RegistrationRequest &);
    virtual void OnSendRegistrationConfirm(H225_RegistrationConfirm &);
    virtual void OnSendRegistrationReject(H225_RegistrationReject &);
    virtual PBoolean OnReceiveRegistrationRequest(const H323RasPDU &, const H225_RegistrationRequest &);
    virtual PBoolean OnReceiveRegistrationConfirm(const H323RasPDU &, const H225_RegistrationConfirm &);
    virtual PBoolean OnReceiveRegistrationReject(const H323RasPDU &, const H225_RegistrationReject &);
    virtual PBoolean OnReceiveRegistrationRequest(const H225_RegistrationRequest &);
    virtual PBoolean OnReceiveRegistrationConfirm(const H225_RegistrationConfirm &);
    virtual PBoolean OnReceiveRegistrationReject(const H225_RegistrationReject &);

    virtual void OnSendUnregistrationRequest(H323RasPDU &, H225_UnregistrationRequest &);
    virtual void OnSendUnregistrationConfirm(H323RasPDU &, H225_UnregistrationConfirm &);
    virtual void OnSendUnregistrationReject(H323RasPDU &, H225_UnregistrationReject &);
    virtual void OnSendUnregistrationRequest(H225_UnregistrationRequest &);
    virtual void OnSendUnregistrationConfirm(H225_UnregistrationConfirm &);
    virtual void OnSendUnregistrationReject(H225_UnregistrationReject &);
    virtual PBoolean OnReceiveUnregistrationRequest(const H323RasPDU &, const H225_UnregistrationRequest &);
    virtual PBoolean OnReceiveUnregistrationConfirm(const H323RasPDU &, const H225_UnregistrationConfirm &);
    virtual PBoolean OnReceiveUnregistrationReject(const H323RasPDU &, const H225_UnregistrationReject &);
    virtual PBoolean OnReceiveUnregistrationRequest(const H225_UnregistrationRequest &);
    virtual PBoolean OnReceiveUnregistrationConfirm(const H225_UnregistrationConfirm &);
    virtual PBoolean OnReceiveUnregistrationReject(const H225_UnregistrationReject &);

    virtual void OnSendAdmissionRequest(H323RasPDU &, H225_AdmissionRequest &);
    virtual void OnSendAdmissionConfirm(H323RasPDU &, H225_AdmissionConfirm &);
    virtual void OnSendAdmissionReject(H323RasPDU &, H225_AdmissionReject &);
    virtual void OnSendAdmissionRequest(H225_AdmissionRequest &);
    virtual void OnSendAdmissionConfirm(H225_AdmissionConfirm &);
    virtual void OnSendAdmissionReject(H225_AdmissionReject &);
    virtual PBoolean OnReceiveAdmissionRequest(const H323RasPDU &, const H225_AdmissionRequest &);
    virtual PBoolean OnReceiveAdmissionConfirm(const H323RasPDU &, const H225_AdmissionConfirm &);
    virtual PBoolean OnReceiveAdmissionReject(const H323RasPDU &, const H225_AdmissionReject &);
    virtual PBoolean OnReceiveAdmissionRequest(const H225_AdmissionRequest &);
    virtual PBoolean OnReceiveAdmissionConfirm(const H225_AdmissionConfirm &);
    virtual PBoolean OnReceiveAdmissionReject(const H225_AdmissionReject &);

    virtual void OnSendBandwidthRequest(H323RasPDU &, H225_BandwidthRequest &);
    virtual void OnSendBandwidthConfirm(H323RasPDU &, H225_BandwidthConfirm &);
    virtual void OnSendBandwidthReject(H323RasPDU &, H225_BandwidthReject &);
    virtual void OnSendBandwidthRequest(H225_BandwidthRequest &);
    virtual void OnSendBandwidthConfirm(H225_BandwidthConfirm &);
    virtual void OnSendBandwidthReject(H225_BandwidthReject &);
    virtual PBoolean OnReceiveBandwidthRequest(const H323RasPDU &, const H225_BandwidthRequest &);
    virtual PBoolean OnReceiveBandwidthConfirm(const H323RasPDU &, const H225_BandwidthConfirm &);
    virtual PBoolean OnReceiveBandwidthReject(const H323RasPDU &, const H225_BandwidthReject &);
    virtual PBoolean OnReceiveBandwidthRequest(const H225_BandwidthRequest &);
    virtual PBoolean OnReceiveBandwidthConfirm(const H225_BandwidthConfirm &);
    virtual PBoolean OnReceiveBandwidthReject(const H225_BandwidthReject &);

    virtual void OnSendDisengageRequest(H323RasPDU &, H225_DisengageRequest &);
    virtual void OnSendDisengageConfirm(H323RasPDU &, H225_DisengageConfirm &);
    virtual void OnSendDisengageReject(H323RasPDU &, H225_DisengageReject &);
    virtual void OnSendDisengageRequest(H225_DisengageRequest &);
    virtual void OnSendDisengageConfirm(H225_DisengageConfirm &);
    virtual void OnSendDisengageReject(H225_DisengageReject &);
    virtual PBoolean OnReceiveDisengageRequest(const H323RasPDU &, const H225_DisengageRequest &);
    virtual PBoolean OnReceiveDisengageConfirm(const H323RasPDU &, const H225_DisengageConfirm &);
    virtual PBoolean OnReceiveDisengageReject(const H323RasPDU &, const H225_DisengageReject &);
    virtual PBoolean OnReceiveDisengageRequest(const H225_DisengageRequest &);
    virtual PBoolean OnReceiveDisengageConfirm(const H225_DisengageConfirm &);
    virtual PBoolean OnReceiveDisengageReject(const H225_DisengageReject &);

    virtual void OnSendLocationRequest(H323RasPDU &, H225_LocationRequest &);
    virtual void OnSendLocationConfirm(H323RasPDU &, H225_LocationConfirm &);
    virtual void OnSendLocationReject(H323RasPDU &, H225_LocationReject &);
    virtual void OnSendLocationRequest(H225_LocationRequest &);
    virtual void OnSendLocationConfirm(H225_LocationConfirm &);
    virtual void OnSendLocationReject(H225_LocationReject &);
    virtual PBoolean OnReceiveLocationRequest(const H323RasPDU &, const H225_LocationRequest &);
    virtual PBoolean OnReceiveLocationConfirm(const H323RasPDU &, const H225_LocationConfirm &);
    virtual PBoolean OnReceiveLocationReject(const H323RasPDU &, const H225_LocationReject &);
    virtual PBoolean OnReceiveLocationRequest(const H225_LocationRequest &);
    virtual PBoolean OnReceiveLocationConfirm(const H225_LocationConfirm &);
    virtual PBoolean OnReceiveLocationReject(const H225_LocationReject &);

    virtual void OnSendInfoRequest(H323RasPDU &, H225_InfoRequest &);
    virtual void OnSendInfoRequestAck(H323RasPDU &, H225_InfoRequestAck &);
    virtual void OnSendInfoRequestNak(H323RasPDU &, H225_InfoRequestNak &);
    virtual void OnSendInfoRequestResponse(H323RasPDU &, H225_InfoRequestResponse &);
    virtual void OnSendInfoRequest(H225_InfoRequest &);
    virtual void OnSendInfoRequestAck(H225_InfoRequestAck &);
    virtual void OnSendInfoRequestNak(H225_InfoRequestNak &);
    virtual void OnSendInfoRequestResponse(H225_InfoRequestResponse &);
    virtual PBoolean OnReceiveInfoRequest(const H323RasPDU &, const H225_InfoRequest &);
    virtual PBoolean OnReceiveInfoRequestAck(const H323RasPDU &, const H225_InfoRequestAck &);
    virtual PBoolean OnReceiveInfoRequestNak(const H323RasPDU &, const H225_InfoRequestNak &);
    virtual PBoolean OnReceiveInfoRequestResponse(const H323RasPDU &, const H225_InfoRequestResponse &);
    virtual PBoolean OnReceiveInfoRequest(const H225_InfoRequest &);
    virtual PBoolean OnReceiveInfoRequestAck(const H225_InfoRequestAck &);
    virtual PBoolean OnReceiveInfoRequestNak(const H225_InfoRequestNak &);
    virtual PBoolean OnReceiveInfoRequestResponse(const H225_InfoRequestResponse &);

    virtual void OnSendResourcesAvailableIndicate(H323RasPDU &, H225_ResourcesAvailableIndicate &);
    virtual void OnSendResourcesAvailableConfirm(H323RasPDU &, H225_ResourcesAvailableConfirm &);
    virtual void OnSendResourcesAvailableIndicate(H225_ResourcesAvailableIndicate &);
    virtual void OnSendResourcesAvailableConfirm(H225_ResourcesAvailableConfirm &);
    virtual PBoolean OnReceiveResourcesAvailableIndicate(const H323RasPDU &, const H225_ResourcesAvailableIndicate &);
    virtual PBoolean OnReceiveResourcesAvailableConfirm(const H323RasPDU &, const H225_ResourcesAvailableConfirm &);
    virtual PBoolean OnReceiveResourcesAvailableIndicate(const H225_ResourcesAvailableIndicate &);
    virtual PBoolean OnReceiveResourcesAvailableConfirm(const H225_ResourcesAvailableConfirm &);

    virtual void OnSendServiceControlIndication(H323RasPDU &, H225_ServiceControlIndication &);
    virtual void OnSendServiceControlResponse(H323RasPDU &, H225_ServiceControlResponse &);
    virtual void OnSendServiceControlIndication(H225_ServiceControlIndication &);
    virtual void OnSendServiceControlResponse(H225_ServiceControlResponse &);
    virtual PBoolean OnReceiveServiceControlIndication(const H323RasPDU &, const H225_ServiceControlIndication &);
    virtual PBoolean OnReceiveServiceControlResponse(const H323RasPDU &, const H225_ServiceControlResponse &);
    virtual PBoolean OnReceiveServiceControlIndication(const H225_ServiceControlIndication &);
    virtual PBoolean OnReceiveServiceControlResponse(const H225_ServiceControlResponse &);

    virtual void OnSendNonStandardMessage(H323RasPDU &, H225_NonStandardMessage &);
    virtual void OnSendNonStandardMessage(H225_NonStandardMessage &);
    virtual PBoolean OnReceiveNonStandardMessage(const H323RasPDU &, const H225_NonStandardMessage &);
    virtual PBoolean OnReceiveNonStandardMessage(const H225_NonStandardMessage &);

    virtual void OnSendUnknownMessageResponse(H323RasPDU &, H225_UnknownMessageResponse &);
    virtual void OnSendUnknownMessageResponse(H225_UnknownMessageResponse &);
    virtual PBoolean OnReceiveUnknownMessageResponse(const H323RasPDU &, const H225_UnknownMessageResponse &);
    virtual PBoolean OnReceiveUnknownMessageResponse(const H225_UnknownMessageResponse &);

    virtual void OnSendRequestInProgress(H323RasPDU &, H225_RequestInProgress &);
    virtual void OnSendRequestInProgress(H225_RequestInProgress &);
    virtual PBoolean OnReceiveRequestInProgress(const H323RasPDU &, const H225_RequestInProgress &);
    virtual PBoolean OnReceiveRequestInProgress(const H225_RequestInProgress &);

    virtual PBoolean OnSendFeatureSet(unsigned, H225_FeatureSet &, PBoolean) const
    { return FALSE; }

    virtual void OnReceiveFeatureSet(unsigned, const H225_FeatureSet &) const
    { }

    virtual void DisableFeatureSet(int) const
    { }

    /**Handle unknown PDU type.
      */
    virtual PBoolean OnReceiveUnknown(
      const H323RasPDU & pdu  ///< PDU that was not handled.
    );
  //@}

  /**@name Member variable access */
  //@{
    /**Get the gatekeeper identifer.
       For clients at least one successful registration must have been
       achieved for this field to be filling in.
      */
    const PString & GetIdentifier() const { return gatekeeperIdentifier; }

    /**Set the gatekeeper identifer.
       For servers this allows the identifier to be set and provided to all
       remote clients.
      */
    void SetIdentifier(const PString & id) { gatekeeperIdentifier = id; }
  //@}

  protected:
    // Option variables
    PString gatekeeperIdentifier;
};


#endif // __OPAL_H225RAS_H


/////////////////////////////////////////////////////////////////////////////
