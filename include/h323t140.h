/* h323t140.h
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
 * The Initial Developer of the Original Code is Spranto International Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef __H323T140_H
#define __H323T140_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include "h323caps.h"
#include "channels.h"


///////////////////////////////////////////////////////////////////////////////

/**This class describes the T.140 logical channel.
 */
class H323_RFC4103Capability : public H323DataCapability
{
    PCLASSINFO(H323_RFC4103Capability, H323DataCapability);
  public:
  /**@name Construction */
  //@{
    /**Create capability.
      */
    H323_RFC4103Capability();
  //@}

  /**@name Overrides from class PObject */
  //@{
    /**Create a copy of the object.
      */
    virtual PObject * Clone() const;
  //@}

  /**@name Identification functions */
  //@{
    /**Get the sub-type of the capability. This is a code dependent on the
       main type of the capability.

       This returns the e_T140 enum value from the protocol ASN
       H245_DataApplicationCapability_application class.
     */
    virtual unsigned GetSubType() const;

    /**Get the name of the media data format this class represents.
     */
    virtual PString GetFormatName() const;
  //@}

  /**@name Operations */
  //@{
    /**Create the channel instance, allocating resources as required.
     */
    virtual H323Channel * CreateChannel(
      H323Connection & connection,    ///<  Owner connection for channel
      H323Channel::Directions dir,    ///<  Direction of channel
      unsigned sessionID,             ///<  Session ID for RTP channel
      const H245_H2250LogicalChannelParameters * param
                                      ///<  Parameters for channel
    ) const;
  //@}

  /**@name Protocol manipulation */
  //@{
    /**This function is called whenever and outgoing TerminalCapabilitySet
       or OpenLogicalChannel PDU is being constructed for the control channel.
       It allows the capability to set the PDU fields from information in
       members specific to the class.

       The default behaviour sets the pdu and calls OnSendingPDU with a
       H245_DataProtocolCapability parameter.
     */
    virtual PBoolean OnSendingPDU(
      H245_DataApplicationCapability & pdu
    ) const;

    /**This function is called whenever and outgoing RequestMode
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour sets the pdu and calls OnSendingPDU with a
       H245_DataProtocolCapability parameter.
     */
    virtual PBoolean OnSendingPDU(
      H245_DataMode & pdu  ///<  PDU to set information on
    ) const;



    /**This function is called whenever and incoming TerminalCapabilitySet
       or OpenLogicalChannel PDU has been used to construct the control
       channel. It allows the capability to set from the PDU fields,
       information in members specific to the class.

       The default behaviour gets the data rate field from the PDU.
     */
    virtual PBoolean OnReceivedPDU(
      const H245_DataApplicationCapability & pdu  ///<  PDU to set information on
    );

    /**This function is called whenever and outgoing TerminalCapabilitySet
       or OpenLogicalChannel PDU is being constructed for the control channel.
       It allows the capability to set the PDU fields from information in
       members specific to the class.

       The default behaviour sets the pdu and calls OnSendingPDU with a
       H245_DataProtocolCapability parameter.
     */
    PBoolean OnReceivedPDU(const H245_GenericCapability & pdu);

    /**This function is called whenever and outgoing TerminalCapabilitySet
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour calls the OnSendingPDU() function with a more
       specific PDU type.
     */
    PBoolean OnSendingPDU(H245_GenericCapability & pdu) const;
  //@}

  protected:

};


/**This class describes the RFC4103 flavour of T.140 logical channel.
 */
class H323_RFC4103Channel : public H323DataChannel
{
    PCLASSINFO(H323_RFC4103Channel, H323DataChannel);
  public:
  /**@name Construction */
  //@{
    /**Create a new channel.
     */
    H323_RFC4103Channel(
      H323Connection & connection,        ///<  Connection to endpoint for channel
      const H323Capability & capability,  ///<  Capability channel is using
      Directions direction,               ///<  Direction of channel
	  RTP_UDP & rtp,                      ///<  RTP Session for channel
      unsigned sessionID                  ///<  Session ID for channel
    );
  //@}

  /**@name Overrides from class H323Channel */
  //@{
    /**Handle channel data reception.

       This is called by the thread started by the Start() function and is
       typically a loop reading  from the transport and handling PDU's.

       The default behaviour here is to call HandleChannel()
      */
    virtual void Receive();

    /**Handle channel data transmission.

       This is called by the thread started by the Start() function and is
       typically a loop reading from the codec and writing to the transport
       (eg an RTP_session).

       The default behaviour here is to call HandleChannel()
      */
    virtual void Transmit();

    /**Fill out the OpenLogicalChannel PDU for the particular channel type.
     */
    virtual PBoolean OnSendingPDU(
      H245_OpenLogicalChannel & openPDU  ///<  Open PDU to send. 
    ) const;

    virtual PBoolean OnSendingPDU(H245_H2250LogicalChannelParameters & param) const;

    virtual void OnSendOpenAck(H245_H2250LogicalChannelAckParameters & param) const;

    /**This is called when request to create a channel is received from a
       remote machine and is about to be acknowledged.
     */
    virtual void OnSendOpenAck(
      const H245_OpenLogicalChannel & open,   ///<  Open PDU
      H245_OpenLogicalChannelAck & ack        ///<  Acknowledgement PDU
    ) const;

    /**This is called after a request to create a channel occurs from the
       local machine via the H245LogicalChannelDict::Open() function, and
       the request has been acknowledged by the remote endpoint.

       The default makes sure the parameters are compatible and passes on
       the PDU to the rtp session.
     */
    virtual PBoolean OnReceivedPDU(
      const H245_OpenLogicalChannel & pdu,    ///<  Open PDU
      unsigned & errorCode                    ///<  Error code on failure
    );

    virtual PBoolean OnReceivedPDU(const H245_H2250LogicalChannelParameters & param,
							 unsigned & errorCode);

    /**This is called after a request to create a channel occurs from the
       local machine via the H245LogicalChannelDict::Open() function, and
       the request has been acknowledged by the remote endpoint.

       The default makes sure the parameters are compatible and passes on
       the PDU to the rtp session.
     */
    virtual PBoolean OnReceivedAckPDU(
      const H245_OpenLogicalChannelAck & pdu ///<  Acknowledgement PDU
    );

    virtual PBoolean OnReceivedAckPDU(
      const H245_H2250LogicalChannelAckParameters & param
    );
  //@}

    virtual PBoolean SetDynamicRTPPayloadType(int newType);
    RTP_DataFrame::PayloadTypes GetDynamicRTPPayloadType() const { return rtpPayloadType; }

    virtual void HandleChannel();

  protected:

    virtual PBoolean ExtractTransport(const H245_TransportAddress & pdu,
                                      PBoolean isDataPort,
                                      unsigned & errorCode);

    RTP_UDP & rtpSession;
    Directions direction;
    unsigned sessionID;

    RTP_DataFrame::PayloadTypes rtpPayloadType;

};

#endif  // __H323T140_H
