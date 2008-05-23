/*
 * h323annexg.h
 *
 * Implementation of H.323 Annex G using H.501
 *
 * Open H323 Library
 *
 * Copyright (c) 2003 Equivalence Pty. Ltd.
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
 * Contributor(s): ______________________________________.
 */

#ifndef __OPAL_H323ANNEXG_H
#define __OPAL_H323ANNEXG_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include "h323trans.h"
#include "h501pdu.h"

class PASN_Sequence;
class PASN_Choice;

class H323EndPoint;

///////////////////////////////////////////////////////////////////////////////

/**This class embodies the H.323 Annex G using the H.501 protocol 
  */
class H323_AnnexG : public H323Transactor
{
  PCLASSINFO(H323_AnnexG, H323Transactor);
  public:
  /**@name Construction */
  //@{
    enum {
      DefaultUdpPort = 2099,
      DefaultTcpPort = 2099
    };

    /**Create a new protocol handler.
     */
    H323_AnnexG(
      H323EndPoint & endpoint,  ///< Endpoint gatekeeper is associated with.
      H323Transport * transport ///< Transport over which gatekeepers communicates.
    );
    H323_AnnexG(
      H323EndPoint & endpoint,           ///< Endpoint gatekeeper is associated with.
      const H323TransportAddress & addr ///< Transport over which gatekeepers communicates.
    );

    /**Destroy protocol handler.
     */
    ~H323_AnnexG();
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
    virtual PBoolean OnReceiveServiceRequest              (const H501PDU & pdu, const H501_ServiceRequest & pduBody);
    virtual PBoolean OnReceiveServiceConfirmation         (const H501PDU & pdu, const H501_ServiceConfirmation & pduBody);
    virtual PBoolean OnReceiveServiceRejection            (const H501PDU & pdu, const H501_ServiceRejection & pduBody);
    virtual PBoolean OnReceiveServiceRelease              (const H501PDU & pdu, const H501_ServiceRelease & pduBody);
    virtual PBoolean OnReceiveDescriptorRequest           (const H501PDU & pdu, const H501_DescriptorRequest & pduBody);
    virtual PBoolean OnReceiveDescriptorConfirmation      (const H501PDU & pdu, const H501_DescriptorConfirmation & pduBody);
    virtual PBoolean OnReceiveDescriptorRejection         (const H501PDU & pdu, const H501_DescriptorRejection & pduBody);
    virtual PBoolean OnReceiveDescriptorIDRequest         (const H501PDU & pdu, const H501_DescriptorIDRequest & pduBody);
    virtual PBoolean OnReceiveDescriptorIDConfirmation    (const H501PDU & pdu, const H501_DescriptorIDConfirmation & pduBody);
    virtual PBoolean OnReceiveDescriptorIDRejection       (const H501PDU & pdu, const H501_DescriptorIDRejection & pduBody);
    virtual PBoolean OnReceiveDescriptorUpdate            (const H501PDU & pdu, const H501_DescriptorUpdate & pduBody);
    virtual PBoolean OnReceiveDescriptorUpdateACK         (const H501PDU & pdu, const H501_DescriptorUpdateAck & pduBody);
    virtual PBoolean OnReceiveAccessRequest               (const H501PDU & pdu, const H501_AccessRequest & pduBody);
    virtual PBoolean OnReceiveAccessConfirmation          (const H501PDU & pdu, const H501_AccessConfirmation & pduBody);
    virtual PBoolean OnReceiveAccessRejection             (const H501PDU & pdu, const H501_AccessRejection & pduBody);
    virtual PBoolean OnReceiveRequestInProgress           (const H501PDU & pdu, const H501_RequestInProgress & pduBody);
    virtual PBoolean OnReceiveNonStandardRequest          (const H501PDU & pdu, const H501_NonStandardRequest & pduBody);
    virtual PBoolean OnReceiveNonStandardConfirmation     (const H501PDU & pdu, const H501_NonStandardConfirmation & pduBody);
    virtual PBoolean OnReceiveNonStandardRejection        (const H501PDU & pdu, const H501_NonStandardRejection & pduBody);
    virtual PBoolean OnReceiveUnknownMessageResponse      (const H501PDU & pdu, const H501_UnknownMessageResponse & pduBody);
    virtual PBoolean OnReceiveUsageRequest                (const H501PDU & pdu, const H501_UsageRequest & pduBody);
    virtual PBoolean OnReceiveUsageConfirmation           (const H501PDU & pdu, const H501_UsageConfirmation & pduBody);
    virtual PBoolean OnReceiveUsageIndicationConfirmation (const H501PDU & pdu, const H501_UsageIndicationConfirmation & pduBody);
    virtual PBoolean OnReceiveUsageIndicationRejection    (const H501PDU & pdu, const H501_UsageIndicationRejection & pduBody);
    virtual PBoolean OnReceiveUsageRejection              (const H501PDU & pdu, const H501_UsageRejection & pduBody);
    virtual PBoolean OnReceiveValidationRequest           (const H501PDU & pdu, const H501_ValidationRequest & pduBody);
    virtual PBoolean OnReceiveValidationConfirmation      (const H501PDU & pdu, const H501_ValidationConfirmation & pduBody);
    virtual PBoolean OnReceiveValidationRejection         (const H501PDU & pdu, const H501_ValidationRejection & pduBody);
    virtual PBoolean OnReceiveAuthenticationRequest       (const H501PDU & pdu, const H501_AuthenticationRequest & pduBody);
    virtual PBoolean OnReceiveAuthenticationConfirmation  (const H501PDU & pdu, const H501_AuthenticationConfirmation & pduBody);
    virtual PBoolean OnReceiveAuthenticationRejection     (const H501PDU & pdu, const H501_AuthenticationRejection & pduBody);
    virtual PBoolean OnReceiveUnknown(const H501PDU &);

  protected:
    void Construct();
};


#endif // __OPAL_H323ANNEXG_H


/////////////////////////////////////////////////////////////////////////////
