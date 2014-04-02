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

#include "h323caps.h"

#ifdef H323_H235

#pragma once

class H245_EncryptionAuthenticationAndIntegrity;
class H235SecurityCapability  : public H323Capability
{
  PCLASSINFO(H235SecurityCapability, H323Capability);

  public:

  /**@name Construction */

    /**Create the Security capability
      */
    H235SecurityCapability(H323Capabilities * capabilities, unsigned capabilityNo);
    ~H235SecurityCapability();

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

    /**Validate that the capability is usable given the connection.
       This checks agains the negotiated protocol version number and remote
       application to determine if this capability should be used in TCS or
       OLC pdus.

       The default behaviour returns TRUE.
      */
    virtual PBoolean IsUsable(
      const H323Connection & connection) const;
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

    /**Get the default RTP session.
       This function gets the default RTP session ID for the capability
       type. For example audio capabilities return the value
       RTP_Session::DefaultAudioSessionID etc.

       The default behaviour returns zero, indicating it is not an RTP
       based capability.
      */
    unsigned GetDefaultSessionID() const;

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

    /**This function is called whenever and incoming OpenLogicalChannel
       PDU has been used to construct the control channel. It allows the
       capability to set from the PDU fields, information in members specific
       to the class.
     */
    PBoolean OnSendingPDU(
       H245_EncryptionAuthenticationAndIntegrity & encAuth,  ///< Encryption Algorithms
       H323Capability::CommandType type = e_TCS              ///< Message Type
    ) const;


    /**This function is called whenever and incoming OpenLogicalChannel
       PDU has been used to construct the control channel. It allows the
       capability to set from the PDU fields, information in members specific
       to the class.
     */
    PBoolean OnReceivedPDU(
       const H245_EncryptionAuthenticationAndIntegrity & encAuth,  ///< Encryption Algorithms
       H323Capability::CommandType type = e_TCS                    ///< Message Type
    ) const;


    /**Set the Associated Capability Number
      */
    virtual void SetAssociatedCapability(unsigned capNumber);

    /**Merge the Algorithms
      */
    PBoolean MergeAlgorithms(
        const PStringArray & remote   ///< List of remote algorithms
    );

    /**Get the number of Algorithms in the list
      */
    PINDEX GetAlgorithmCount();

    /**Get the current Algorithms
      */
    PString GetAlgorithm();

  //@}

  protected:
      H323Capabilities * m_capabilities;
      unsigned m_capNumber;
      PStringList m_capList;
};


////////////////////////////////////////////////////////////////////////////////////////
/**This class describes the secure interface to a codec that has channels based on
   the RTP protocol.

   An application may create a descendent off this class and override
   functions as required for descibing the codec.
 */

enum H235ChType {
    H235ChNew,          /// New Channel (Template)
    H235ChClone,        /// Clone Channel (Primary)
    H235Channel,        /// Connection Channel
};

class H323SecureRealTimeCapability  : public H323Capability
{
public:

  PCLASSINFO(H323SecureRealTimeCapability, H323Capability);

  /**@name Constructor/Deconstructor */
  //@{  
    /**Constructor
      */
         H323SecureRealTimeCapability(
          H323Capability * childCapability,          ///< Child Capability
          H323Capabilities * capabilities = NULL,    ///< Capabilities reference
          unsigned secNo = 0,                        ///< Security Capability No
          PBoolean active = false                    ///< Whether encryption Activated
          );

      H323SecureRealTimeCapability(
          RTP_QOS * _rtpqos,
          H323Capability * childCapability
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
    virtual unsigned GetCapabilityNumber() const;

    /// Set unique capability number.
    virtual void SetCapabilityNumber(unsigned num);

    /// Attach QoS
    void AttachQoS(RTP_QOS * _rtpqos);

    /// Set the Associated Capability 
    virtual void SetAssociatedCapability(unsigned  _secNo);

    /// Set the Capability List
    virtual void SetCapabilityList(H323Capabilities * capabilities);

    /// Set the encryption active
    virtual void SetEncryptionActive(PBoolean active);

    /// Is encryption active
    virtual PBoolean IsEncryptionActive() const;

    /// Set Algorithm
    virtual void SetEncryptionAlgorithm(const PString & alg);

    /// Get Algorithm
    virtual const PString & GetEncryptionAlgorithm() const;

    /// Get the MediaFormat for this capability.
    virtual const OpalMediaFormat & GetMediaFormat() const;

    virtual OpalMediaFormat & GetWritableMediaFormat();
  //@}

protected:
    H323Capability * ChildCapability;    /// Child Capability
    H235ChType chtype;                   /// Channel Type
    PBoolean  m_active;                  /// Whether encryption is active
    H323Capabilities * m_capabilities;   /// Capabilities list
    unsigned  m_secNo;                   /// Security Capability
    RTP_QOS * nrtpqos;                   /// RTP QOS
    PString   m_algorithm;               /// Algorithm for encryption

};


/////////////////////////////////////////////////////////////////////////////////////////

/**This class describes the interface to a secure codec used to transfer data
   via the logical channels opened and managed by the H323 control channel.

   An application may create a descendent off this class and override
   functions as required for descibing a codec.
 */
class H323SecureCapability : public H323SecureRealTimeCapability
{
  PCLASSINFO(H323SecureCapability, H323SecureRealTimeCapability);

  public:
  /**@name Construction */
  //@{

    /**Create an encrypted audio based capability 
      */
    H323SecureCapability(
        H323Capability & childCapability,          ///< Child Capability
        enum H235ChType Ch = H235ChNew,            ///< ChannelType
        H323Capabilities * capabilities = NULL,    ///< Capabilities reference
        unsigned secNo = 0,                        ///< Security Capability No
        PBoolean active = false                    ///< Whether encryption is active or not
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

    /**Get Generic Identifier 
        Default returns PString::Empty
     */
    virtual PString GetIdentifier() const;

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
      PBoolean receiver                  /// is receiver OLC
    );

    /**Compare the sub capability.
      */
    virtual PBoolean IsMatch(
      const PASN_Choice & subTypePDU  ///<  sub-type PDU of H323Capability
    ) const;

    /**Compare the security part of the capability, if applicable.
      */
    virtual PBoolean IsSubMatch(
      const PASN_Choice & subTypePDU  ///<  sub-type PDU of H323Capability
    ) const;

    /**Get Child Capability
      */
    H323Capability * GetChildCapability() const { return ChildCapability; }

    /**Validate that the capability is usable given the connection.
       This checks agains the negotiated protocol version number and remote
       application to determine if this capability should be used in TCS or
       OLC pdus.

       The default behaviour returns TRUE.
      */
    virtual PBoolean IsUsable(
      const H323Connection & connection
      ) const { return ChildCapability->IsUsable(connection); }
  //@}

    /**Get the direction for this capability.
      */ 
    CapabilityDirection GetCapabilityDirection() const 
        { return ChildCapability->GetCapabilityDirection(); }

    /**Set the direction for this capability.
      */
    void SetCapabilityDirection(
      CapabilityDirection dir   /// New direction code
    ) { ChildCapability->SetCapabilityDirection(dir); }

    /// Get the payload type for the capaibility
    RTP_DataFrame::PayloadTypes GetPayloadType() const 
        { return ChildCapability->GetPayloadType(); }

  //@}

};


////////////////////////////////////////////////////////////////////////////////////////
/**This class describes the secure interface to a data codec that has channels based on
   the RTP protocol.

   An application may create a descendent off this class and override
   functions as required for descibing the codec.
 */

class H323SecureDataCapability : public H323DataCapability
{
  PCLASSINFO(H323SecureDataCapability, H323DataCapability);
	
public:
	
    H323SecureDataCapability(H323Capability & childCapability,          ///< Child Capability
                           enum H235ChType Ch = H235ChNew,               ///< ChannelType
                           H323Capabilities * capabilities = NULL,    ///< Capabilities reference
                           unsigned secNo = 0,                        ///< Security Capability No
                           PBoolean active = false                    ///< Whether encryption is active or not
                           );
    ~H323SecureDataCapability();

    Comparison Compare(const PObject & obj) const;

    virtual PObject * Clone() const;

    virtual unsigned GetSubType() const;

    virtual PString GetFormatName() const;

    virtual H323Channel * CreateChannel(H323Connection & connection,
                                      H323Channel::Directions dir,
                                      unsigned sesionID,
                                      const H245_H2250LogicalChannelParameters * param) const;

    virtual PBoolean IsMatch(const PASN_Choice & subTypePDU) const;
    virtual PBoolean IsSubMatch(const PASN_Choice & subTypePDU) const;

    virtual PBoolean IsUsable(const H323Connection & connection) const { return ChildCapability->IsUsable(connection); }

    CapabilityDirection GetCapabilityDirection() const  { return ChildCapability->GetCapabilityDirection(); }

    void SetCapabilityDirection(CapabilityDirection dir ) { ChildCapability->SetCapabilityDirection(dir); }

    RTP_DataFrame::PayloadTypes GetPayloadType() const { return ChildCapability->GetPayloadType(); }

    H323Capability * GetChildCapability() const { return ChildCapability; }

    virtual PBoolean OnSendingPDU(H245_Capability & pdu) const;
    virtual PBoolean OnReceivedPDU(const H245_Capability & pdu);

    virtual PBoolean OnSendingPDU(H245_ModeElement & mode) const;

    virtual PBoolean OnSendingPDU(H245_DataType & dataType) const;
    virtual PBoolean OnReceivedPDU(const H245_DataType & dataType,PBoolean receiver);

    virtual PBoolean OnSendingPDU(H245_DataMode & pdu) const;


    virtual void SetEncryptionActive(PBoolean active);
    virtual PBoolean IsEncryptionActive() const;

    virtual void SetEncryptionAlgorithm(const PString & alg);
    virtual const PString & GetEncryptionAlgorithm() const;

    virtual void SetAssociatedCapability(unsigned _secNo);

protected:
    H323Capability * ChildCapability;    /// Child Capability
    H235ChType chtype;                   /// Channel Type
    PBoolean  m_active;                  /// Whether encryption is active
    H323Capabilities * m_capabilities;   /// Capabilities list
    unsigned  m_secNo;                   /// Security Capability
    PString   m_algorithm;               /// Algorithm for encryption
	
};

//////////////////////////////////////////////////////////////////////////////////////////

class H235_DiffieHellman;
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
      const H323Connection & connection,             ///< Connection for capabilities
      const H245_TerminalCapabilitySet & pdu         ///< PDU to convert to a capability set.
    );

    void WrapCapability(PINDEX descriptorNum,        ///< The member of the capabilityDescriptor to add
                        PINDEX simultaneous,         ///< The member of the SimultaneousCapabilitySet to add
                        H323Capability & capability  ///< capability to wrap
                       );

    void AddSecure(PINDEX descriptorNum,             ///< The member of the capabilityDescriptor to add
                   PINDEX simultaneous,              ///< The member of the SimultaneousCapabilitySet to add
                   H323Capability * capability       ///< capability to add
                   );

    H323Capability * CopySecure(PINDEX descriptorNum,             ///< The member of the capabilityDescriptor to add 
                                PINDEX simultaneous,              ///< The member of the SimultaneousCapabilitySet to add
                                const H323Capability & capability ///< capability to copy
                                );

    /**Add all matching capabilities to descriptor lists.
       All capabilities that match the specified name are added as in the other
       form of the SetCapability() function.
      */
    virtual PINDEX AddAllCapabilities(
      PINDEX descriptorNum, ///< The member of the capabilityDescriptor to add
      PINDEX simultaneous,  ///< The member of the SimultaneousCapabilitySet to add
      const PString & name  ///< New capabilities name, if using "known" one.
    );

    /**Add the DH KeyPair
      */
    void SetDHKeyPair(const PStringList & keyOIDs, H235_DiffieHellman * key, PBoolean isMaster);

    /**Get the DH KeyPair
      */
   void GetDHKeyPair(PStringList & keyOIDs, H235_DiffieHellman * & key, PBoolean & isMaster);

    /**Get the Algorithms
         return false if no algorithms.
      */
    PBoolean GetAlgorithms(const PStringList & algorithms) const;

    H235_DiffieHellman * GetDiffieHellMan() { return m_DHkey; }

    /**Filter codecs
      */
    static void SetH235Codecs(const PStringArray & servers);
    PBoolean IsH235Codec(const PString & name);

protected:
    H235_DiffieHellman * m_DHkey; 
    PStringList          m_algorithms;
    PBoolean             m_h245Master;

};

#endif  // H323_H235


