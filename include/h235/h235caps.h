/*
 * h235caps.h
 *
 * H.235 Capability wrapper class.
 *
 * h323plus library
 *
 * Copyright (c) 2011 Spranto Australia Pty Ltd.
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
 *
 * Contributor(s): ______________________________________.
 *
 * $Id $
 *
 *
 */

#include "h323ep.h"
#include "h245.h"

#include "h323caps.h"

#ifdef H323_H235

class H235SecurityCapability  : public H323Capability
{
  PCLASSINFO(H235SecurityCapability, H323Capability);

  public:
  /**@name Construction */

    /**Create the Conference capability
      */
    H235SecurityCapability(unsigned capabilityNo);
  //@}

  /**@name Overrides from class PObject */
  //@{
    /**Create a copy of the object.
      */
    virtual PObject * Clone() const;
  //@}

  /**@name Identification functions */
  //@{
    /**Get the main type of the capability.
     */
    virtual MainTypes GetMainType() const;

    /**Get the sub-type of the capability. This is a code dependent on the
       main type of the capability.
     */
    virtual unsigned GetSubType()  const;

    /**Get the name of the media data format this class represents.
     */
    virtual PString GetFormatName() const;
  //@}

  /**@name Operations */
  //@{
    /**Create the channel instance, allocating resources as required.
       This creates a logical channel object appropriate for the parameters
       provided. Not if param is NULL, sessionID must be provided, otherwise
       this is taken from the fields in param.
     */
    virtual H323Channel * CreateChannel(
      H323Connection & connection,    ///< Owner connection for channel
      H323Channel::Directions dir,    ///< Direction of channel
      unsigned sessionID,             ///< Session ID for RTP channel
      const H245_H2250LogicalChannelParameters * param
                                      ///< Parameters for channel
    ) const;

    /**Create the codec instance, allocating resources as required.
     */
    virtual H323Codec * CreateCodec(
      H323Codec::Direction direction  ///< Direction in which this instance runs
    ) const;

  //@}

  /**@name Protocol manipulation */
  //@{
    /**This function is called whenever and outgoing TerminalCapabilitySet
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour is pure.
     */
    virtual PBoolean OnSendingPDU(
      H245_Capability & pdu  ///< PDU to set information on
    ) const;

    /**This function is called whenever and outgoing OpenLogicalChannel
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour is pure.
     */
    virtual PBoolean OnSendingPDU(
      H245_DataType & pdu  ///< PDU to set information on
    ) const;

    /**This function is called whenever and outgoing RequestMode
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour calls the OnSendingPDU() function with a more
       specific PDU type.
     */
    virtual PBoolean OnSendingPDU(
      H245_ModeElement & pdu  ///< PDU to set information on
    ) const;

    /**This function is called whenever and incoming TerminalCapabilitySet
       PDU is received on the control channel, and a new H323Capability
       descendent was created. This completes reading fields from the PDU
       into the classes members.

       If the function returns FALSE then the received PDU codec description
       is not supported, so will be ignored. The default behaviour simply
       returns TRUE.
     */
    virtual PBoolean OnReceivedPDU(
      const H245_Capability & pdu  ///< PDU to get information from
    );

    /**This function is called whenever and incoming OpenLogicalChannel
       PDU has been used to construct the control channel. It allows the
       capability to set from the PDU fields, information in members specific
       to the class.

       The default behaviour is pure.
     */
    virtual PBoolean OnReceivedPDU(
      const H245_DataType & pdu,  ///< PDU to get information from
      PBoolean receiver               ///< Is receiver OLC
    );
  //@}

  protected:
      unsigned m_capNumber;
};


////////////////////////////////////////////////////////////////////////////////////////
/**This class describes the secure interface to a codec that has channels based on
   the RTP protocol.

   An application may create a descendent off this class and override
   functions as required for descibing the codec.
 */

enum PTLSChType {
	PTLSChNew,			/// New Channel (Template)
	PTLSChClone,		/// Clone Channel (Primary)
	PTLSChannel,		/// Connection Channel
};

class PAec;
class H323SecureRealTimeCapability  : public H323Capability
{
public:

  PCLASSINFO(H323SecureRealTimeCapability, H323Capability);

  /**@name Constructor/Deconstructor */
  //@{  
    /**Constructor
      */
	  H323SecureRealTimeCapability(
		  H323Capability & childCapability
		  );

	  H323SecureRealTimeCapability(
		  RTP_QOS * _rtpqos,
		  H323Capability & childCapability
		  );

    /**Deconstructor
      */
	  ~H323SecureRealTimeCapability();
  //@}

  /**@name Operations */
  //@{
    /**Create the channel instance, allocating resources as required.
     */
    virtual H323Channel * CreateChannel(
      H323Connection & connection,    /// Owner connection for channel
      H323Channel::Directions dir,    /// Direction of channel
      unsigned sessionID,             /// Session ID for RTP channel
      const H245_H2250LogicalChannelParameters * param
                                      /// Parameters for channel
    ) const;

    /// Get unique capability number.
    virtual unsigned GetCapabilityNumber() const 
		{ return ChildCapability.GetCapabilityNumber(); };

    /// Set unique capability number.
    virtual void SetCapabilityNumber(unsigned num) 
		{ 
			assignedCapabilityNumber = num;
			ChildCapability.SetCapabilityNumber(num); 
		};

	void AttachQoS(RTP_QOS * _rtpqos);

#ifdef H323_AEC
	void AttachAEC(PAec * _ARC);
#endif

  //@}

protected:
	H323Capability & ChildCapability;  /// Child Capability
	PTLSChType chtype;				  /// Channel Type
	RTP_QOS * nrtpqos;				  /// RTP QOS

#ifdef H323_AEC
	PAec * aec;						  /// Acoustic Echo Cancellation
#endif
};


/**This class describes the secure interface to an audio codec used to transfer data
   via the logical channels opened and managed by the H323 control channel.

   An application may create a descendent off this class and override
   functions as required for descibing the codec.
 */

class H323SecureAudioCapability : public H323SecureRealTimeCapability 
{

  PCLASSINFO(H323SecureAudioCapability, H323SecureRealTimeCapability);

  public:
  /**@name Construction */
  //@{

    /**Create an encrypted audio based capability 
      */
    H323SecureAudioCapability(			 
        H323Capability & childCapability,           /// ChildAudio Capability
		enum PTLSChType Ch = PTLSChNew			    /// ChannelType
    );

    /**Create an encrypted audio based capability 
		with QoS Support
      */
	H323SecureAudioCapability(
		H323Capability & childCapability,			/// ChildAudio Capability
		RTP_QOS * _rtpqos,							/// Quality of Service Setting
		enum PTLSChType Ch = PTLSChNew				/// Channel Type
	);

  //@}

  /**@name Deconstruction */
  //@{   

	~H323SecureAudioCapability();
  //@}

  /**@name Overrides from class PObject */
  //@{
    /**Create a copy of the object.
      */
    virtual PObject * Clone() const;

	/**Compare objects
	  */
//	PObject::Comparison Compare(const PObject & obj) const;
  //@}

  /**@name Identification functions */
  //@{
    /**Get the main type of the capability.
       Always returns e_Audio.
     */
    virtual MainTypes GetMainType() const;

    /**Get the sub-type of the capability. This is a code dependent on the
       main type of the capability.

       This returns one of the four possible combinations of mode and speed
       using the enum values of the protocol ASN H245_AudioCapability class.
     */
    virtual unsigned GetSubType() const;

    /**Get the name of the media data format this class represents.
     */
    virtual PString GetFormatName() const;
  
   /**Create the Codec */
    H323Codec * CreateCodec(H323Codec::Direction direction) const;
  //@}

  /**@name Operations */
  //@{
    /**Get the default RTP session.
       This function gets the default RTP session ID for the capability
       type. For example audio capabilities return the value
       RTP_Session::DefaultAudioSessionID etc.

       The default behaviour returns zero, indicating it is not an RTP
       based capability.
      */
    virtual unsigned GetDefaultSessionID() const;

    /**Set the maximum size (in frames) of data that will be transmitted in a
       single PDU.

       This will also be the desired number that will be sent by most codec
       implemetations.

       The default behaviour sets the txFramesInPacket variable.
     */
    virtual void SetTxFramesInPacket(
      unsigned frames   /// Number of frames per packet
    );

    /**Get the maximum size (in frames) of data that will be transmitted in a
       single PDU.

       The default behaviour sends the txFramesInPacket variable.
     */
    virtual unsigned GetTxFramesInPacket() const;

    /**Get the maximum size (in frames) of data that can be received in a
       single PDU.

       The default behaviour sends the rxFramesInPacket variable.
     */
    virtual unsigned GetRxFramesInPacket() const;
  //@}

  /**@name Protocol manipulation */
  //@{
    /**This function is called whenever and outgoing TerminalCapabilitySet
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour calls the OnSendingPDU() function with a more
       specific PDU type.
     */
    virtual PBoolean OnSendingPDU(
      H245_Capability & pdu  /// PDU to set information on
    ) const;

    /**This function is called whenever and outgoing OpenLogicalChannel
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour calls the OnSendingPDU() function with a more
       specific PDU type.
     */
    virtual PBoolean OnSendingPDU(
      H245_DataType & pdu  /// PDU to set information on
    ) const;

    /**This function is called whenever and outgoing RequestMode
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour calls the OnSendingPDU() function with a more
       specific PDU type.
     */
    virtual PBoolean OnSendingPDU(
      H245_ModeElement & pdu  /// PDU to set information on
    ) const;

    /**This function is called whenever and incoming TerminalCapabilitySet
       PDU is received on the control channel, and a new H323Capability
       descendent was created. This completes reading fields from the PDU
       into the classes members.

       If the function returns FALSE then the received PDU codec description
       is not supported, so will be ignored.
       
       The default behaviour calls the OnReceivedPDU() that takes a
       H245_AudioCapability and clamps the txFramesInPacket.
     */

    virtual PBoolean OnReceivedPDU(
      const H245_Capability & pdu  /// PDU to get information from
    );

    /**This function is called whenever and incoming OpenLogicalChannel
       PDU has been used to construct the control channel. It allows the
       capability to set from the PDU fields, information in members specific
       to the class.
       
       The default behaviour calls the OnReceivedPDU() that takes a
       H245_AudioCapability and clamps the txFramesInPacket or
       rxFramesInPacket.
     */
    virtual PBoolean OnReceivedPDU(
      const H245_DataType & pdu,  /// PDU to get information from
      PBoolean receiver               /// Is receiver OLC
    );

	/**Get Child Capability
	  */
	H323Capability & GetChildCapability() { return ChildCapability; };
  //@}

    /**Validate that the capability is usable given the connection.
       This checks agains the negotiated protocol version number and remote
       application to determine if this capability should be used in TCS or
       OLC pdus.

       The default behaviour returns TRUE.
      */
    virtual PBoolean IsUsable(
      const H323Connection & connection
	  ) const { return ChildCapability.IsUsable(connection); };
  

    /**Get the direction for this capability.
      */ 
    CapabilityDirection GetCapabilityDirection() const 
		{ return ChildCapability.GetCapabilityDirection(); };

    /**Set the direction for this capability.
      */
    void SetCapabilityDirection(
      CapabilityDirection dir   /// New direction code
    ) { ChildCapability.SetCapabilityDirection(dir); };


    /// Get the payload type for the capaibility
    RTP_DataFrame::PayloadTypes GetPayloadType() const 
		{ return ChildCapability.GetPayloadType(); };

  //@}

protected:

    unsigned rxFramesInPacket;
    unsigned txFramesInPacket;

};

/////////////////////////////////////////////////////////////////////////////////////////

/**This class describes the interface to a video codec used to transfer data
   via the logical channels opened and managed by the H323 control channel.

   An application may create a descendent off this class and override
   functions as required for descibing a codec.
 */
class H323SecureVideoCapability : public H323SecureRealTimeCapability
{
  PCLASSINFO(H323SecureVideoCapability, H323SecureRealTimeCapability);

  public:
  /**@name Construction */
  //@{

    /**Create an encrypted audio based capability 
      */
    H323SecureVideoCapability(				 
        H323Capability & childCapability,          /// ChildAudio Capability
		enum PTLSChType Ch = PTLSChNew			   /// ChannelType
    );

  //@}

  /**@name Overrides from class PObject */
  //@{
    /**Create a copy of the object.
      */
    virtual PObject * Clone() const;

	/**Compare
	  */
	PObject::Comparison Compare(const PObject & obj) const;
  //@}

  /**@name Identification functions */
  //@{
    /**Get the main type of the capability.
       Always returns e_Audio.
     */
    virtual MainTypes GetMainType() const;

    /**Get the sub-type of the capability. This is a code dependent on the
       main type of the capability.

       This returns one of the four possible combinations of mode and speed
       using the enum values of the protocol ASN H245_AudioCapability class.
     */
    virtual unsigned GetSubType() const;

    /**Get the name of the media data format this class represents.
     */
    virtual PString GetFormatName() const;
  
   /**Create the Codec */
    H323Codec * CreateCodec(H323Codec::Direction direction) const;
  //@}

  /**@name Operations */
  //@{
    /**Get the default RTP session.
       This function gets the default RTP session ID for the capability
       type. For example audio capabilities return the value
       RTP_Session::DefaultAudioSessionID etc.

       The default behaviour returns zero, indicating it is not an RTP
       based capability.
      */
    virtual unsigned GetDefaultSessionID() const;
  //@}

  /**@name Protocol manipulation */
  //@{
    /**This function is called whenever and outgoing TerminalCapabilitySet
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour calls the OnSendingPDU() function with a more
       specific PDU type.
     */
    virtual PBoolean OnSendingPDU(
      H245_Capability & pdu  /// PDU to set information on
    ) const;

    /**This function is called whenever and outgoing OpenLogicalChannel
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour calls the OnSendingPDU() function with a more
       specific PDU type.
     */
    virtual PBoolean OnSendingPDU(
      H245_DataType & pdu  /// PDU to set information on
    ) const;

    /**This function is called whenever and outgoing RequestMode
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour calls the OnSendingPDU() function with a more
       specific PDU type.
     */
    virtual PBoolean OnSendingPDU(
      H245_ModeElement & pdu  /// PDU to set information on
    ) const;

    /**This function is called whenever and incoming TerminalCapabilitySet
       PDU is received on the control channel, and a new H323Capability
       descendent was created. This completes reading fields from the PDU
       into the classes members.

       If the function returns FALSE then the received PDU codec description
       is not supported, so will be ignored.
       
       The default behaviour calls the OnReceivedPDU() that takes a
       H245_AudioCapability and clamps the txFramesInPacket.
     */

    virtual PBoolean OnReceivedPDU(
      const H245_Capability & pdu  /// PDU to get information from
    );

    /**This function is called whenever and incoming OpenLogicalChannel
       PDU has been used to construct the control channel. It allows the
       capability to set from the PDU fields, information in members specific
       to the class.
       
       The default behaviour calls the OnReceivedPDU() that takes a
       H245_AudioCapability and clamps the txFramesInPacket or
       rxFramesInPacket.
     */
    virtual PBoolean OnReceivedPDU(
      const H245_DataType & pdu,  /// PDU to get information from
	  PBoolean receiver				  /// is receiver OLC
    );

    /**This function is called whenever and outgoing TerminalCapabilitySet
       or OpenLogicalChannel PDU is being constructed for the control channel.
       It allows the capability to set the PDU fields from information in
       members specific to the class.

       The default behaviour is pure.
     */
    virtual PBoolean OnSendingPDU(
      H245_VideoCapability & pdu  /// PDU to set information on
    ) const;

    /**This function is called whenever and outgoing RequestMode
       PDU is being constructed for the control channel. It allows the
       capability to set the PDU fields from information in members specific
       to the class.

       The default behaviour sets the PDUs tag according to the GetSubType()
       function (translated to different enum).
     */
    virtual PBoolean OnSendingPDU(
      H245_VideoMode & pdu  /// PDU to set information on
    ) const;

    /**This function is called whenever and incoming TerminalCapabilitySet
       or OpenLogicalChannel PDU has been used to construct the control
       channel. It allows the capability to set from the PDU fields,
       information in members specific to the class.

       The default behaviour is pure.
     */
    virtual PBoolean OnReceivedPDU(
      const H245_VideoCapability & pdu  /// PDU to set information on
    );

	/**Get Child Capability
	  */
	H323Capability & GetChildCapability() const { return ChildCapability; }

    /**Validate that the capability is usable given the connection.
       This checks agains the negotiated protocol version number and remote
       application to determine if this capability should be used in TCS or
       OLC pdus.

       The default behaviour returns TRUE.
      */
    virtual PBoolean IsUsable(
      const H323Connection & connection
	  ) { return ChildCapability.IsUsable(connection); }
  //@}

    /**Get the direction for this capability.
      */ 
    CapabilityDirection GetCapabilityDirection() const 
		{ return ChildCapability.GetCapabilityDirection(); }

    /**Set the direction for this capability.
      */
    void SetCapabilityDirection(
      CapabilityDirection dir   /// New direction code
    ) { ChildCapability.SetCapabilityDirection(dir); }

    /// Get the payload type for the capaibility
    RTP_DataFrame::PayloadTypes GetPayloadType() const 
		{ return ChildCapability.GetPayloadType(); }

  //@}

};

class H235Capabilities : public H323Capabilities
{
     PCLASSINFO(H235Capabilities, H323Capabilities);

public:
    H235Capabilities();

    H235Capabilities(
      const H323Capabilities & original ///< Original capabilities to duplicate
    );

    /**Construct a capability set from the H.245 PDU provided.
      */
    H235Capabilities(
      const H323Connection & connection,      ///< Connection for capabilities
      const H245_TerminalCapabilitySet & pdu  ///< PDU to convert to a capability set.
    );

    void WrapCapability(H323Capability & Cap);

};

#endif  // H323_H235


