/*
 * h323pdu.h
 *
 * H.323 protocol handler
 *
 * Open H323 Library
 *
 * Copyright (c) 1998-2000 Equivalence Pty. Ltd.
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
 * Vovida Networks, Inc. http://www.vovida.com.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef __OPAL_H323PDU_H
#define __OPAL_H323PDU_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <ptlib/sockets.h>
#include "h323con.h"
#include "transports.h"
#include "q931.h"
#include "h225.h"
#include "h245.h"
#include "h323trans.h"


class H225_RAS;


#define H225_PROTOCOL_VERSION 7
#define H245_PROTOCOL_VERSION 15


///////////////////////////////////////////////////////////////////////////////

/**Wrapper class for the H323 signalling channel.
 */
class H323SignalPDU : public H225_H323_UserInformation
{
  PCLASSINFO(H323SignalPDU, H225_H323_UserInformation);

  public:
  /**@name Construction */
  //@{
    /**Create a new H.323 signalling channel (H.225/Q.931) PDU.
     */
    H323SignalPDU();

    /**Build a SETUP message.
      */
    H225_Setup_UUIE & BuildSetup(
      const H323Connection & connection,    ///< Connection PDU is generated for
      const H323TransportAddress & destAddr ///< Destination address for packet
    );

    /**Build a CALL-PROCEEDING message.
      */
    H225_CallProceeding_UUIE & BuildCallProceeding(
      const H323Connection & connection    ///< Connection PDU is generated for
    );

    /**Build a CONNECT message.
      */
    H225_Connect_UUIE & BuildConnect(
      const H323Connection & connection    ///< Connection PDU is generated for
    );

    /**Build a CONNECT message with H.245 address.
      */
    H225_Connect_UUIE & BuildConnect(
      const H323Connection & connection,    ///< Connection PDU is generated for
      const PIPSocket::Address & h245Address, ///< H.245 IP address
      WORD port                               ///< H.245 TCP port
    );

    /**Build an ALERTING message.
      */
    H225_Alerting_UUIE & BuildAlerting(
      const H323Connection & connection    ///< Connection PDU is generated for
    );

    /**Build a INFORMATION message.
      */
    H225_Information_UUIE & BuildInformation(
      const H323Connection & connection    ///< Connection PDU is generated for
    );

    /**Build a RELEASE-COMPLETE message.
      */
    H225_ReleaseComplete_UUIE & BuildReleaseComplete(
      const H323Connection & connection    ///< Connection PDU is generated for
    );

    /**Build a FACILITY message.
      */
    H225_Facility_UUIE * BuildFacility(
      const H323Connection & connection,  ///< Connection PDU is generated for
      PBoolean empty,                         ///< Flag for empty facility message
	  unsigned reason = H225_FacilityReason::e_undefinedReason ///< Reason for Facility
    );

    /**Build a FACILITY message for Tunneling RAS Messages.
      */
    H225_Facility_UUIE * BuildRasFacility(
        unsigned reason = H225_FacilityReason::e_featureSetUpdate    ///< Reason for Facility
    );

    /**Build a PROGRESS message.
      */
    H225_Progress_UUIE & BuildProgress(
      const H323Connection & connection    ///< Connection PDU is generated for
    );

    /**Build a STATUS message.
      */
    H225_Status_UUIE & BuildStatus(
      const H323Connection & connection    ///< Connection PDU is generated for
    );

    /**Build a STATUS-INQUIRY message.
      */
    H225_StatusInquiry_UUIE & BuildStatusInquiry(
      const H323Connection & connection    ///< Connection PDU is generated for
    );

    /**Build a SETUP-ACKNOWLEDGE message.
      */
    H225_SetupAcknowledge_UUIE & BuildSetupAcknowledge(
      const H323Connection & connection    ///< Connection PDU is generated for
    );

    /**Build a NOTIFY message.
      */
    H225_Notify_UUIE & BuildNotify(
      const H323Connection & connection    ///< Connection PDU is generated for
    );
  //@}


  /**@name Operations */
  //@{
    /**Print PDU to stream.
      */
    void PrintOn(
      ostream & strm
    ) const;

    /**Read PDU from the specified transport.
      */
    PBoolean Read(
      H323Transport & transport   ///< Transport to read from
    );

    /** Process a block of raw read as a PDU
      */
    PBoolean ProcessReadData(
      H323Transport & transport,   ///< Transport to read from
      const PBYTEArray & rawData
    );

    /**Write the PDU to the transport.
      */
    PBoolean Write(
      H323Transport & transport,          ///* Transport to write to
      H323Connection * connection = NULL  ///* Connection for CallBack
    );

    /**Get the Q.931 wrapper PDU for H.225 signalling PDU.
      */
    const Q931 & GetQ931() const { return q931pdu; }

    /**Get the Q.931 wrapper PDU for H.225 signalling PDU.
      */
    Q931 & GetQ931() { return q931pdu; }

    /**Set the Q.931 wrapper PDU for H.225 signalling PDU
     */
    void SetQ931(const Q931 & _q931pdu) { q931pdu = _q931pdu; }

    /**Build the Q.931 wrapper PDU for H.225 signalling PDU.
       This must be called after altering fields in the H.225 part of the PDU.
       If it has never been done, then the Write() functions will do so.
      */
    void BuildQ931();

    /**Get the source alias names for the remote endpoint.
       This returns a human readable set of names that was provided by the
       remote endpoint to identify it, eg phone number, display name etc etc
      */
    PString GetSourceAliases(
      const H323Transport * transport = NULL  ///< Transport PDU was read from.
    ) const;


    /**Get the source alias names for the remote endpoint.
       This returns an array of names of the remote endpoint to identify it.
      */
    PStringArray GetSourceAliasNames() const;

	/**Get the Source H323 URL
	   This return the URL of the caller if Present. This can be used for Inter-Domain
	   Name caller resolution 
	  */
    PString GetSourceURL() const;

    /**Get the destination alias name(s) for the local endpoint.
       The alias returned here can be used to determine the routing of an
       incoming connection.
      */
    PString GetDestinationAlias(
      PBoolean firstAliasOnly = FALSE   ///< Only return the first possible alias
    ) const;

    /**Get the source endpoints identification as a phone number.
       This returns FALSE if the remote never provided any alias or Q.931
       field that indicated a valid e.164 telephone number.
      */
    PBoolean GetSourceE164(
      PString & number    ///< String to receive number
    ) const;

    /**Get the destiation  phone number.
       This returns FALSE if the remote never provided any alias or Q.931
       field that indicated a valid e.164 telephone number.
      */
    PBoolean GetDestinationE164(
      PString & number    ///< String to receive number
    ) const;

    /**Get the distinctive ring code if present.
       This returns zero if no distinctive ring information is provided.
      */
    unsigned GetDistinctiveRing() const;

    /**Set the Q.931 fields in the PDU.
       This sets the default values for various fields, eg caller party number
       into the Q.931 from the supplied connection.
      */
    void SetQ931Fields(
      const H323Connection & connection,
      PBoolean insertPartyNumbers = FALSE,
      unsigned plan = 1,
      unsigned type = 0,
      int presentation = -1,
      int screening = -1
    );

#ifdef H323_H460
    /** When sending the H460 message in the Setup PDU you have to ensure
        the ARQ is received first then add the Fields to the Setup PDU
       so we require a call back
      */
    PBoolean InsertH460Generic(const H323Connection & connection);

    /** When sending the H460 message in the Setup PDU you have to ensure
        the ARQ is received first then add the Fields to the Setup PDU
        so we require a call back
      */
    void InsertH460Setup(const H323Connection & connection, H225_Setup_UUIE & setup);
#endif

    /** When sending the Setup PDU you have to ensure
        the ARQ is received first then add the cryptoFields to the Setup PDU
        so we require a call back
     */
    void InsertCryptoTokensSetup(const H323Connection & connection, H225_Setup_UUIE & setup);

#ifdef H323_H46017
    void LoadTunneledQ931(const Q931 & q931);
#endif

  protected:
    // Even though we generally deal with the H323 protocol (H225) it is
    // actually contained within a field of the Q931 protocol.
    Q931 q931pdu;
};


/////////////////////////////////////////////////////////////////////////////

/**Wrapper class for the H323 control channel.
 */
class H323ControlPDU : public H245_MultimediaSystemControlMessage
{
  PCLASSINFO(H323ControlPDU, H245_MultimediaSystemControlMessage);

  public:
    H245_RequestMessage    & Build(H245_RequestMessage   ::Choices request);
    H245_ResponseMessage   & Build(H245_ResponseMessage  ::Choices response);
    H245_CommandMessage    & Build(H245_CommandMessage   ::Choices command);
    H245_IndicationMessage & Build(H245_IndicationMessage::Choices indication);

    H245_MasterSlaveDetermination & BuildMasterSlaveDetermination(
      unsigned terminalType,
      unsigned statusDeterminationNumber
    );
    H245_MasterSlaveDeterminationAck & BuildMasterSlaveDeterminationAck(
      PBoolean isMaster
    );
    H245_MasterSlaveDeterminationReject & BuildMasterSlaveDeterminationReject(
      unsigned cause
    );

    H245_TerminalCapabilitySet & BuildTerminalCapabilitySet(
      const H323Connection & connection,
      unsigned sequenceNumber,
      PBoolean empty
    );
    H245_TerminalCapabilitySetAck & BuildTerminalCapabilitySetAck(
      unsigned sequenceNumber
    );
    H245_TerminalCapabilitySetReject & BuildTerminalCapabilitySetReject(
      unsigned sequenceNumber,
      unsigned cause
    );

    H245_OpenLogicalChannel & BuildOpenLogicalChannel(
      unsigned forwardLogicalChannelNumber
    );
    H245_RequestChannelClose & BuildRequestChannelClose(
      unsigned channelNumber,
      unsigned reason
    );
    H245_CloseLogicalChannel & BuildCloseLogicalChannel(
      unsigned channelNumber
    );
    H245_OpenLogicalChannelAck & BuildOpenLogicalChannelAck(
      unsigned channelNumber
    );
    H245_OpenLogicalChannelReject & BuildOpenLogicalChannelReject(
      unsigned channelNumber,
      unsigned cause
    );
    H245_OpenLogicalChannelConfirm & BuildOpenLogicalChannelConfirm(
      unsigned channelNumber
    );
    H245_IndicationMessage & BuildLogicalChannelActive(
      unsigned channelNumber
    );
    H245_CloseLogicalChannelAck & BuildCloseLogicalChannelAck(
      unsigned channelNumber
    );
    H245_RequestChannelCloseAck & BuildRequestChannelCloseAck(
      unsigned channelNumber
    );
    H245_RequestChannelCloseReject & BuildRequestChannelCloseReject(
      unsigned channelNumber
    );
    H245_RequestChannelCloseRelease & BuildRequestChannelCloseRelease(
      unsigned channelNumber
    );

    H245_RequestMode & BuildRequestMode(
      unsigned sequenceNumber
    );
    H245_RequestModeAck & BuildRequestModeAck(
      unsigned sequenceNumber,
      unsigned response
    );
    H245_RequestModeReject & BuildRequestModeReject(
      unsigned sequenceNumber,
      unsigned cause
    );

    H245_RoundTripDelayRequest & BuildRoundTripDelayRequest(
      unsigned sequenceNumber
    );
    H245_RoundTripDelayResponse & BuildRoundTripDelayResponse(
      unsigned sequenceNumber
    );

    H245_UserInputIndication & BuildUserInputIndication(
      const PString & value
    );
    H245_UserInputIndication & BuildUserInputIndication(
      char tone,               ///< DTMF tone code
      unsigned duration,       ///< Duration of tone in milliseconds
      unsigned logicalChannel, ///< Logical channel number for RTP sync.
      unsigned rtpTimestamp    ///< RTP timestamp in logical channel sync.
    );

    H245_FunctionNotUnderstood & BuildFunctionNotUnderstood(
      const H323ControlPDU & pdu
    );

    H245_EndSessionCommand & BuildEndSessionCommand(
      unsigned reason
    );
};


/////////////////////////////////////////////////////////////////////////////

/**Wrapper class for the H323 gatekeeper RAS channel.
 */
class H323RasPDU : public H225_RasMessage, public H323TransactionPDU
{
  PCLASSINFO(H323RasPDU, H225_RasMessage);

  public:
    H323RasPDU();
    H323RasPDU(
      const H235Authenticators & authenticators
    );

    // overrides from PObject
    virtual PObject * Clone() const;

    // overrides from H323TransactionPDU
    virtual PASN_Object & GetPDU();
    virtual PASN_Choice & GetChoice();
    virtual const PASN_Object & GetPDU() const;
    virtual const PASN_Choice & GetChoice() const;
    virtual unsigned GetSequenceNumber() const;
    virtual unsigned GetRequestInProgressDelay() const;
#if PTRACING
    virtual const char * GetProtocolName() const;
#endif
    virtual H323TransactionPDU * ClonePDU() const;
    virtual void DeletePDU();

    // new functions
    H225_GatekeeperRequest       & BuildGatekeeperRequest(unsigned seqNum);
    H225_GatekeeperConfirm       & BuildGatekeeperConfirm(unsigned seqNum);
    H225_GatekeeperReject        & BuildGatekeeperReject(unsigned seqNum, unsigned reason = H225_GatekeeperRejectReason::e_undefinedReason);
    H225_RegistrationRequest     & BuildRegistrationRequest(unsigned seqNum);
    H225_RegistrationConfirm     & BuildRegistrationConfirm(unsigned seqNum);
    H225_RegistrationReject      & BuildRegistrationReject(unsigned seqNum, unsigned reason = H225_RegistrationRejectReason::e_undefinedReason);
    H225_UnregistrationRequest   & BuildUnregistrationRequest(unsigned seqNum);
    H225_UnregistrationConfirm   & BuildUnregistrationConfirm(unsigned seqNum);
    H225_UnregistrationReject    & BuildUnregistrationReject(unsigned seqNum, unsigned reason = H225_UnregRejectReason::e_undefinedReason);
    H225_LocationRequest         & BuildLocationRequest(unsigned seqNum);
    H225_LocationConfirm         & BuildLocationConfirm(unsigned seqNum);
    H225_LocationReject          & BuildLocationReject(unsigned seqNum, unsigned reason = H225_LocationRejectReason::e_undefinedReason);
    H225_AdmissionRequest        & BuildAdmissionRequest(unsigned seqNum);
    H225_AdmissionConfirm        & BuildAdmissionConfirm(unsigned seqNum);
    H225_AdmissionReject         & BuildAdmissionReject(unsigned seqNum, unsigned reason = H225_AdmissionRejectReason::e_undefinedReason);
    H225_DisengageRequest        & BuildDisengageRequest(unsigned seqNum);
    H225_DisengageConfirm        & BuildDisengageConfirm(unsigned seqNum);
    H225_DisengageReject         & BuildDisengageReject(unsigned seqNum, unsigned reason = H225_DisengageRejectReason::e_securityDenial);
    H225_BandwidthRequest        & BuildBandwidthRequest(unsigned seqNum);
    H225_BandwidthConfirm        & BuildBandwidthConfirm(unsigned seqNum, unsigned bandwidth = 0);
    H225_BandwidthReject         & BuildBandwidthReject(unsigned seqNum, unsigned reason = H225_BandRejectReason::e_undefinedReason);
    H225_InfoRequest             & BuildInfoRequest(unsigned seqNum, unsigned callRef = 0, const OpalGloballyUniqueID * id = NULL);
    H225_InfoRequestResponse     & BuildInfoRequestResponse(unsigned seqNum);
    H225_InfoRequestAck          & BuildInfoRequestAck(unsigned seqNum);
    H225_InfoRequestNak          & BuildInfoRequestNak(unsigned seqNum, unsigned reason = H225_InfoRequestNakReason::e_undefinedReason);
    H225_ServiceControlIndication& BuildServiceControlIndication(unsigned seqNum, const OpalGloballyUniqueID * id = NULL);
    H225_ServiceControlResponse  & BuildServiceControlResponse(unsigned seqNum);
    H225_UnknownMessageResponse  & BuildUnknownMessageResponse(unsigned seqNum);
    H225_RequestInProgress       & BuildRequestInProgress(unsigned seqNum, unsigned delay);
};


/////////////////////////////////////////////////////////////////////////////
// Language support

void H323SetLanguage(const PString & lang, PASN_IA5String & asn);
void H323GetLanguage(PStringList & lang, const PASN_IA5String & asn);

#define H323LANGUAGEPDU(pdu) \
    PBoolean H323SetLanguages(const PStringList & lang, pdu##_language & asn); \
    PBoolean H323GetLanguages(PStringList & lang, const pdu##_language & asn);

H323LANGUAGEPDU(H225_Setup_UUIE);
H323LANGUAGEPDU(H225_Connect_UUIE);
H323LANGUAGEPDU(H225_RegistrationRequest);
H323LANGUAGEPDU(H225_RegistrationConfirm);
H323LANGUAGEPDU(H225_AdmissionConfirm);
H323LANGUAGEPDU(H225_LocationRequest);
H323LANGUAGEPDU(H225_LocationConfirm);


PString H323GetDisplayName(const H225_ArrayOf_DisplayName & asn);
PBoolean H323SetDisplayName(const PStringList & alias, const PStringList & lang, H225_ArrayOf_DisplayName & asn);

/////////////////////////////////////////////////////////////////////////////

void H323SetAliasAddresses(const H323TransportAddressArray & addresses, H225_ArrayOf_AliasAddress & aliases);
void H323SetAliasAddresses(const PStringArray & names, H225_ArrayOf_AliasAddress & aliases, int tag = -1);
void H323SetAliasAddresses(const PStringList & names, H225_ArrayOf_AliasAddress & aliases, int tag = -1);
void H323SetAliasAddress(const H323TransportAddress & address, H225_AliasAddress & alias);
void H323SetAliasAddress(const PString & name, H225_AliasAddress & alias, int tag = -1);
PStringArray H323GetAliasAddressStrings(const H225_ArrayOf_AliasAddress & aliases);
PString H323GetAliasAddressString(const H225_AliasAddress & alias);
PString H323GetAliasAddressE164(const H225_AliasAddress & alias);
PString H323GetAliasAddressE164(const H225_ArrayOf_AliasAddress & aliases);

H323Connection::CallEndReason H323TranslateToCallEndReason(
  Q931::CauseValues cause,
  const H225_ReleaseCompleteReason & reason
);
Q931::CauseValues H323TranslateFromCallEndReason(
  H323Connection::CallEndReason endReason,
  H225_ReleaseCompleteReason & reason
);
Q931::CauseValues H323TranslateFromCallEndReason(
  const H323Connection & connection,
  H225_ReleaseCompleteReason & rcReason
);

PString H323GetApplicationInfo(const H225_VendorIdentifier & vendor);

PBoolean H323SetRTPPacketization(
  H245_RTPPayloadType & rtpPacketization,
  const OpalMediaFormat & mediaFormat,
  RTP_DataFrame::PayloadTypes payloadType
);
PBoolean H323SetRTPPacketization(
  const PString & mediaPacketization,
  H245_RTPPayloadType & rtpPacketization,
  RTP_DataFrame::PayloadTypes payloadType
);
PBoolean H323GetRTPPacketization(
  OpalMediaFormat & mediaFormat,
  const H245_RTPPayloadType & rtpPacketization
);

#if PTRACING
void H323TraceDumpPDU(
  const char * proto,
  PBoolean writing,
  const PBYTEArray & rawData,
  const PASN_Object & pdu,
  const PASN_Choice & tag1,
  unsigned seqNum,
  const H323TransportAddress & locAddr,
  const H323TransportAddress & remAddr
);
#else
#define H323TraceDumpPDU(proto, writing, rawData, pdu, tag1, seqNum, locAddr, remAddr)
#endif


#endif // __OPAL_H323PDU_H


/////////////////////////////////////////////////////////////////////////////
