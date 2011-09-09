/*
 * h235caps.cxx
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


#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H235

#include "h235/h235caps.h"
#include "h323.h"

/////////////////////////////////////////////////////////////////////////////

H235SecurityCapability::H235SecurityCapability(unsigned capabilityNo)
: m_capNumber(capabilityNo)
{
}

PObject * H235SecurityCapability::Clone() const
{
  return new H235SecurityCapability(*this);
}

H323Capability::MainTypes H235SecurityCapability::GetMainType() const
{
  return e_Security;
}

unsigned H235SecurityCapability::GetSubType()  const
{
    return 0;
}

PString H235SecurityCapability::GetFormatName() const
{
  return "Security";
}

H323Channel * H235SecurityCapability::CreateChannel(H323Connection &,
                                                      H323Channel::Directions,
                                                      unsigned,
                                                      const H245_H2250LogicalChannelParameters *) const
{
  PTRACE(1, "Codec\tCannot create Security channel");
  return NULL;
}

H323Codec * H235SecurityCapability::CreateCodec(H323Codec::Direction) const
{
  PTRACE(1, "Codec\tCannot create security codec");
  return NULL;
}

PINDEX H235SecurityCapability::GetAlgorithmCount()
{
   return m_capList.GetSize();
}

PBoolean H235SecurityCapability::OnSendingPDU(H245_EncryptionAuthenticationAndIntegrity & encAuth, H323Capability::CommandType type) const
{
  if (m_capList.GetSize() == 0)
      return false;

  encAuth.IncludeOptionalField(H245_EncryptionAuthenticationAndIntegrity::e_encryptionCapability);

  H245_EncryptionCapability & enc = encAuth.m_encryptionCapability;
    
  if (type == e_OLC) {  // only the 1st (preferred) Algorithm.
      enc.SetSize(1);
      H245_MediaEncryptionAlgorithm & alg = enc[0];
      alg.SetTag(H245_MediaEncryptionAlgorithm::e_algorithm);
      PASN_ObjectId & id = alg;
      id.SetValue(m_capList[0]);
      return true;
  } else if (type == e_TCS) {  // all the supported Algorithms
      enc.SetSize(m_capList.GetSize());
      for (PINDEX i=0; i < m_capList.GetSize(); ++i) {
          H245_MediaEncryptionAlgorithm & alg = enc[i];
          alg.SetTag(H245_MediaEncryptionAlgorithm::e_algorithm);
          PASN_ObjectId & id = alg;
          id.SetValue(m_capList[i]);
      }
      return true;
  } else
      return false;
}

PBoolean H235SecurityCapability::OnSendingPDU(H245_Capability & pdu) const
{
  if (m_capList.GetSize() == 0)
      return false;

  pdu.SetTag(H245_Capability::e_h235SecurityCapability);
  H245_H235SecurityCapability & sec = pdu;

  if (!OnSendingPDU(sec.m_encryptionAuthenticationAndIntegrity))
      return false;

  H245_CapabilityTableEntryNumber & capNo = sec.m_mediaCapability;
  capNo = m_capNumber;

  return TRUE;
}

PBoolean H235SecurityCapability::OnSendingPDU(H245_DataType &) const
{
  PTRACE(1, "Codec\tCannot have Security Capability in DataType");
  return FALSE;
}

PBoolean H235SecurityCapability::OnSendingPDU(H245_ModeElement &) const
{
  PTRACE(1, "Codec\tCannot have Security Capability in ModeElement");
  return FALSE;
}

PBoolean H235SecurityCapability::OnReceivedPDU(const H245_Capability & pdu)
{

  H323Capability::OnReceivedPDU(pdu);

  return TRUE;
}

PBoolean H235SecurityCapability::OnReceivedPDU(const H245_DataType &, PBoolean)
{
  PTRACE(1, "Codec\tCannot have Security Capability in DataType");
  return FALSE;
}

PBoolean H235SecurityCapability::OnReceivedPDU(const H245_EncryptionAuthenticationAndIntegrity & encAuth, 
                                               H323Capability::CommandType type) const
{
   return true;
}

unsigned H235SecurityCapability::GetDefaultSessionID() const
{
  return OpalMediaFormat::NonRTPSessionID;
}

PBoolean H235SecurityCapability::IsUsable(const H323Connection & connection) const
{
   const H235Capabilities & caps = (const H235Capabilities &)connection.GetLocalCapabilities();
   return caps.GetAlgorithnms(m_capList);
}

/////////////////////////////////////////////////////////////////////////////

H323SecureRealTimeCapability::H323SecureRealTimeCapability(H323Capability & childCapability, H323Capabilities * capabilities, unsigned secNo)
				: ChildCapability(*(H323Capability *)childCapability.Clone()), m_active(false), 
                  m_capabilities(capabilities), m_secNo(secNo),  nrtpqos(NULL)
{
}


H323SecureRealTimeCapability::H323SecureRealTimeCapability(RTP_QOS * _rtpqos,H323Capability & childCapability)
				: ChildCapability(*(H323Capability *)childCapability.Clone()), m_active(false), 
                  m_capabilities(NULL), m_secNo(0), nrtpqos(_rtpqos)
{
}

H323SecureRealTimeCapability::~H323SecureRealTimeCapability()
{
}


void H323SecureRealTimeCapability::AttachQoS(RTP_QOS * _rtpqos)
{
	  delete nrtpqos;  
	  nrtpqos = _rtpqos;
}

void H323SecureRealTimeCapability::SetSecurityCapabilityNumber(unsigned _secNo)
{
    m_secNo = _secNo;
}

H323Channel * H323SecureRealTimeCapability::CreateChannel(H323Connection & connection,
                                                    H323Channel::Directions dir,
                                                    unsigned sessionID,
                                 const H245_H2250LogicalChannelParameters * param) const
{
   // work to be done here.
   return ChildCapability.CreateChannel(connection,dir,sessionID,param);
}

unsigned H323SecureRealTimeCapability::GetCapabilityNumber() const 
{ 
    return ChildCapability.GetCapabilityNumber(); 
};

void H323SecureRealTimeCapability::SetCapabilityNumber(unsigned num) 
{ 
	ChildCapability.SetCapabilityNumber(num); 
}

void H323SecureRealTimeCapability::SetCapabilityList(H323Capabilities * capabilities)
{
    m_capabilities = capabilities;
}

void H323SecureRealTimeCapability::SetActive(PBoolean active)
{
    m_active = active;
}

///////////////////////////////////////////////////////////////////////////// 

H323SecureCapability::H323SecureCapability(H323Capability & childCapability,
													 H235ChType Ch,
                                                     H323Capabilities * capabilities,
                                                     unsigned secNo
													 )
   : H323SecureRealTimeCapability(childCapability, capabilities, secNo)
{
	chtype = Ch;
}

H323Capability::MainTypes H323SecureCapability::GetMainType() const
{ 
    return ChildCapability.GetMainType();
}


PObject * H323SecureCapability::Clone() const
{
	PTRACE(4, "H235RTP\tCloning Capability: " << GetFormatName());

	PBoolean IsClone = FALSE;
	H235ChType ch = H235ChNew;

	switch (chtype) {
	case H235ChNew:	
		   ch = H235ChClone;
		   IsClone = TRUE;
		break;
	case H235ChClone:
		   ch = H235Channel;
		break;
	case H235Channel:
		   ch = H235Channel;
		break;
	}

  return new H323SecureCapability(*(H323Capability *)ChildCapability.Clone(),ch, m_capabilities, m_secNo);
}

PBoolean H323SecureCapability::IsMatch(const PASN_Choice & subTypePDU) const
{
    const H245_H235Media_mediaType & dataType = (const H245_H235Media_mediaType &)subTypePDU;

    if (dataType.GetTag() == H245_H235Media_mediaType::e_audioData &&
       ChildCapability.GetMainType() == H323Capability::e_Audio) { 
          const H245_AudioCapability & audio = dataType;
          return ChildCapability.IsMatch(audio);
    }

    if (dataType.GetTag() == H245_H235Media_mediaType::e_videoData &&
       ChildCapability.GetMainType() == H323Capability::e_Video) { 
          const H245_VideoCapability & video = dataType;
          return ChildCapability.IsMatch(video);
    }

    if (dataType.GetTag() == H245_H235Media_mediaType::e_data &&
       ChildCapability.GetMainType() == H323Capability::e_Data) { 
          const H245_DataApplicationCapability & data = dataType;
          return ChildCapability.IsMatch(data.m_application);
    }
    return false;
}

PObject::Comparison H323SecureCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323SecureCapability))
    return LessThan;

  Comparison result = H323Capability::Compare(obj);
  if (result != EqualTo) 
    return result;

  const H323SecureCapability & other = (const H323SecureCapability &)obj;

  return ChildCapability.Compare(other.GetChildCapability());
}

unsigned H323SecureCapability::GetDefaultSessionID() const
{
    return ChildCapability.GetDefaultSessionID();
}

////////////////////////////////////////////////////////////////////////
// PDU Sending

PBoolean H323SecureCapability::OnSendingPDU(H245_ModeElement & mode) const
{
    switch (ChildCapability.GetMainType()) {
        case H323Capability::e_Audio:
            return ((H323AudioCapability &)ChildCapability).OnSendingPDU(mode);
        case H323Capability::e_Video:
            return ((H323VideoCapability &)ChildCapability).OnSendingPDU(mode);
        case H323Capability::e_Data:
        default:
            return false;
    }
}

PBoolean H323SecureCapability::OnSendingPDU(H245_DataType & dataType) const
{
    // find the matching H235SecurityCapability to get the agreed algorithms
    // if not found or no matching algorithm then assume no encryption.
    H235SecurityCapability * secCap = NULL;
    PBoolean secActive = false;
    if (m_capabilities) {
        secCap = (H235SecurityCapability *)m_capabilities->FindCapability(m_secNo);
        if (secCap && secCap->GetAlgorithmCount() > 0)
            secActive = true;
    }

    if (!secActive) {
        unsigned txFramesInPacket =0;
        switch (ChildCapability.GetMainType()) {
            case H323Capability::e_Audio:
                dataType.SetTag(H245_DataType::e_audioData);
                txFramesInPacket = ((H323AudioCapability &)ChildCapability).GetTxFramesInPacket();
                return ((H323AudioCapability &)ChildCapability).OnSendingPDU((H245_AudioCapability &)dataType, txFramesInPacket, e_OLC);
            case H323Capability::e_Video:
                dataType.SetTag(H245_DataType::e_videoData); 
                return ((H323VideoCapability &)ChildCapability).OnSendingPDU((H245_VideoCapability &)dataType, e_OLC);
            case H323Capability::e_Data:
                return ((H323DataCapability &)ChildCapability).OnSendingPDU(dataType, e_OLC);
            default:
                break;
        }
        return false;
    }

    dataType.SetTag(H245_DataType::e_h235Media);
    H245_H235Media & h235Media = dataType;
    // Load the algorithm
    secCap->OnSendingPDU(h235Media.m_encryptionAuthenticationAndIntegrity, e_OLC);
   
    H245_H235Media_mediaType & cType = h235Media.m_mediaType;
    unsigned txFramesInPacket =0;
    switch (ChildCapability.GetMainType()) {
        case H323Capability::e_Audio: 
            cType.SetTag(H245_H235Media_mediaType::e_audioData); 
            txFramesInPacket = ((H323AudioCapability &)ChildCapability).GetTxFramesInPacket();
            return ((H323AudioCapability &)ChildCapability).OnSendingPDU((H245_AudioCapability &)cType, txFramesInPacket, e_OLC);
        case H323Capability::e_Video: 
            cType.SetTag(H245_H235Media_mediaType::e_videoData); 
            return ((H323VideoCapability &)ChildCapability).OnSendingPDU((H245_VideoCapability &)cType, e_OLC);
        case H323Capability::e_Data: 
            cType.SetTag(H245_H235Media_mediaType::e_data);
            return ((H323DataCapability &)ChildCapability).OnSendingPDU((H245_DataApplicationCapability &)cType, e_OLC);
        default:
            break;
    }
    return false;
}

PBoolean H323SecureCapability::OnReceivedPDU(const H245_DataType & dataType,PBoolean receiver)
{
    if (dataType.GetTag() != H245_DataType::e_h235Media)
        return false;

    const H245_H235Media & h235Media = dataType;

    if (m_capabilities) {
        const H235SecurityCapability * secCap = (const H235SecurityCapability *)m_capabilities->FindCapability(m_secNo);
        if (!secCap->OnReceivedPDU(h235Media.m_encryptionAuthenticationAndIntegrity, e_OLC))
            return false;
    }


    const H245_H235Media_mediaType & mediaType = h235Media.m_mediaType;
    unsigned packetSize = 0; 


    switch (ChildCapability.GetMainType()) {
        case H323Capability::e_Audio: 
            if (mediaType.GetTag() == H245_H235Media_mediaType::e_audioData) {
               packetSize = (receiver) ?
                   ChildCapability.GetRxFramesInPacket() : ChildCapability.GetTxFramesInPacket();
               return ((H323AudioCapability &)ChildCapability).OnReceivedPDU((const H245_AudioCapability &)mediaType, packetSize, e_OLC);
            }
        case H323Capability::e_Video: 
            if (mediaType.GetTag() == H245_H235Media_mediaType::e_videoData) 
               return ((H323VideoCapability &)ChildCapability).OnReceivedPDU((const H245_VideoCapability &)mediaType, e_OLC);

        case H323Capability::e_Data:
            if (mediaType.GetTag() == H245_H235Media_mediaType::e_data) 
               return ((H323DataCapability &)ChildCapability).OnReceivedPDU((const H245_DataApplicationCapability &)mediaType, e_OLC);
 
        default:
            break;
    }

	return false;
}

//////////////////////////////////////////////////////////////////////////////
// Child Capability Intercept

PBoolean H323SecureCapability::OnSendingPDU(H245_Capability & pdu) const
{
    switch (ChildCapability.GetMainType()) {
        case H323Capability::e_Audio:
            return ((H323AudioCapability &)ChildCapability).OnSendingPDU(pdu);
        case H323Capability::e_Video:
            return ((H323VideoCapability &)ChildCapability).OnSendingPDU(pdu);
        case H323Capability::e_Data:
        case H323Capability::e_UserInput:
        case H323Capability::e_ExtendVideo:
        default:
            return false;
    }
}

PBoolean H323SecureCapability::OnReceivedPDU(const H245_Capability & pdu)
{
    switch (ChildCapability.GetMainType()) {
        case H323Capability::e_Audio:
            return ((H323AudioCapability &)ChildCapability).OnReceivedPDU(pdu);
        case H323Capability::e_Video:
            return ((H323VideoCapability &)ChildCapability).OnReceivedPDU(pdu);
        case H323Capability::e_Data:
        case H323Capability::e_UserInput:
        case H323Capability::e_ExtendVideo:
        default:
            return false;
    }
}


PString H323SecureCapability::GetFormatName() const
{
  return ChildCapability.GetFormatName() + " #";
}


unsigned H323SecureCapability::GetSubType() const
{
  return ChildCapability.GetSubType();
}


H323Codec * H323SecureCapability::CreateCodec(H323Codec::Direction direction) const
{
    return ChildCapability.CreateCodec(direction);
}


/////////////////////////////////////////////////////////////////////////////

H235Capabilities::H235Capabilities()
:  m_DHkey(NULL), m_h245Master(false)
{
    m_algorithms.SetSize(0);
}

H235Capabilities::H235Capabilities(const H323Capabilities & original)
:  m_DHkey(NULL), m_h245Master(false)
{
  m_algorithms.SetSize(0);

  for (PINDEX i = 0; i < original.GetSize(); i++)
    WrapCapability(original[i]);

  const H323CapabilitiesSet & origSet = original.GetSet();
  PINDEX outerSize = origSet.GetSize();
  set.SetSize(outerSize);
  for (PINDEX outer = 0; outer < outerSize; outer++) {
    PINDEX middleSize = origSet[outer].GetSize();
    set[outer].SetSize(middleSize);
    for (PINDEX middle = 0; middle < middleSize; middle++) {
      PINDEX innerSize = origSet[outer][middle].GetSize();
      for (PINDEX inner = 0; inner < innerSize; inner++)
        set[outer][middle].Append(FindCapability(origSet[outer][middle][inner].GetCapabilityNumber()));
    }
  }


}

H235Capabilities::H235Capabilities(const H323Connection & /*connection*/, const H245_TerminalCapabilitySet & /*pdu*/)
{

}

static unsigned SetCapabilityNumber(const H323CapabilitiesList & table,
                                      unsigned newCapabilityNumber)
{
  // Assign a unique number to the codec, check if the user wants a specific
  // value and start with that.
  if (newCapabilityNumber == 0)
    newCapabilityNumber = 1;

  PINDEX i = 0;
  while (i < table.GetSize()) {
    if (table[i].GetCapabilityNumber() != newCapabilityNumber)
      i++;
    else {
      // If it already in use, increment it
      newCapabilityNumber++;
      i = 0;
    }
  }

  return newCapabilityNumber;
}

void H235Capabilities::AddSecure(H323Capability * capability)
{
  if (capability == NULL)
    return;

  if (!PIsDescendant(capability,H323SecureCapability) &&
      !PIsDescendant(capability,H235SecurityCapability))
      return;

  // See if already added, confuses things if you add the same instance twice
  if (table.GetObjectsIndex(capability) != P_MAX_INDEX)
    return;

  capability->SetCapabilityNumber(SetCapabilityNumber(table, 1));
  ((H323SecureCapability *)capability)->SetCapabilityList(this);

  unsigned capNumber = capability->GetCapabilityNumber();
  H235SecurityCapability * secCap = new H235SecurityCapability(capNumber);
  secCap->SetCapabilityNumber(SetCapabilityNumber(table, 100+capNumber));
  ((H323SecureCapability *)capability)->SetSecurityCapabilityNumber(secCap->GetCapabilityNumber());
  table.Append(capability);
  table.Append(secCap);

  PTRACE(3, "H323\tAdded Secure Capability: " << *capability);
}

H323Capability * H235Capabilities::CopySecure(const H323Capability & capability)
{
  if (!PIsDescendant(&capability,H323SecureCapability) &&
      !PIsDescendant(&capability,H235SecurityCapability))
      return NULL;

  if (PIsDescendant(&capability,H235SecurityCapability)) {
     H235SecurityCapability * newCapability = (H235SecurityCapability *)capability.Clone();
     newCapability->SetCapabilityNumber(capability.GetCapabilityNumber());  // Do not change number - TODO -SH
     table.Append(newCapability);
     return newCapability;
  } else {
     H323SecureCapability * newCapability = (H323SecureCapability *)capability.Clone();
     newCapability->SetCapabilityNumber(SetCapabilityNumber(table, capability.GetCapabilityNumber()));
     newCapability->SetCapabilityList(this);
     table.Append(newCapability);

     PTRACE(3, "H323\tCopied Secure Capability: " << *newCapability);
     return newCapability;
  }
}

void H235Capabilities::WrapCapability(H323Capability & capability)
{

    if (PIsDescendant(&capability,H323SecureCapability) ||
        PIsDescendant(&capability,H235SecurityCapability)) {
          CopySecure(capability);
          return;
    }

    switch (capability.GetDefaultSessionID()) {
        case OpalMediaFormat::DefaultAudioSessionID:
        case OpalMediaFormat::DefaultVideoSessionID:
            AddSecure(new H323SecureCapability(capability, H235ChNew,this));
            break;
        case OpalMediaFormat::NonRTPSessionID:
        case OpalMediaFormat::DefaultDataSessionID:
        case OpalMediaFormat::DefaultH224SessionID:
        case OpalMediaFormat::DefaultExtVideoSessionID:
        default:
            Copy(capability);
            break;
    }
}

static PBoolean MatchWildcard(const PCaselessString & str, const PStringArray & wildcard)
{
  PINDEX last = 0;
  for (PINDEX i = 0; i < wildcard.GetSize(); i++) {
    if (wildcard[i].IsEmpty())
      last = str.GetLength();
    else {
      PINDEX next = str.Find(wildcard[i], last);
      if (next == P_MAX_INDEX)
        return FALSE;
      last = next + wildcard[i].GetLength();
    }
  }

  return TRUE;
}

PINDEX H235Capabilities::AddAllCapabilities(PINDEX descriptorNum,
                                            PINDEX simultaneous,
                                            const PString & name)
{
  PINDEX reply = descriptorNum == P_MAX_INDEX ? P_MAX_INDEX : simultaneous;

  PStringArray wildcard = name.Tokenise('*', FALSE);

  H323CapabilityFactory::KeyList_T stdCaps = H323CapabilityFactory::GetKeyList();

  for (unsigned session = OpalMediaFormat::FirstSessionID; session <= OpalMediaFormat::LastSessionID; session++) {
    for (H323CapabilityFactory::KeyList_T::const_iterator r = stdCaps.begin(); r != stdCaps.end(); ++r) {
      PString capName(*r);
      if (MatchWildcard(capName, wildcard) && (FindCapability(capName) == NULL)) {
        OpalMediaFormat mediaFormat(capName);
        if (!mediaFormat.IsValid() && (capName.Right(4) == "{sw}") && capName.GetLength() > 4)
          mediaFormat = OpalMediaFormat(capName.Left(capName.GetLength()-4));
        if (mediaFormat.IsValid() && mediaFormat.GetDefaultSessionID() == session) {
          // add the capability
          H323Capability * capability = H323Capability::Create(capName);
          H323SecureCapability * newCapability = NULL;
          PINDEX num=0;
            switch (session) {
                case OpalMediaFormat::DefaultAudioSessionID:
                case OpalMediaFormat::DefaultVideoSessionID:
                    newCapability = new H323SecureCapability(*capability, H235ChNew, this);
                    num = SetCapability(descriptorNum, simultaneous, newCapability);
                    num = SetCapability(descriptorNum, simultaneous, 
                                        new H235SecurityCapability(newCapability->GetCapabilityNumber()));
                    delete capability;
                    break;
                case OpalMediaFormat::DefaultDataSessionID:
                case OpalMediaFormat::DefaultH224SessionID:
                case OpalMediaFormat::DefaultExtVideoSessionID:
                default:
                    num = SetCapability(descriptorNum, simultaneous, capability);
                    break;
            }
          
            if (descriptorNum == P_MAX_INDEX) {
                reply = num;
                descriptorNum = num;
                simultaneous = P_MAX_INDEX;
            }
            else if (simultaneous == P_MAX_INDEX) {
                if (reply == P_MAX_INDEX)
                  reply = num;
                simultaneous = num;
            }
        }
      }
    }
    simultaneous = P_MAX_INDEX;
  }

  return reply;
}

void H235Capabilities::SetDHKeyPair(const PStringList & keyOIDs, H235_DiffieHellman * key, PBoolean isMaster)
{
     m_algorithms = keyOIDs;
     m_DHkey = key;
     m_h245Master = isMaster;

     PTRACE(2,"H235\tDiffieHellman selected. Key " << (isMaster ? "Master" : "Slave"));

}

PBoolean H235Capabilities::GetAlgorithnms(const PStringList & algorithms) const
{
    PStringList * m_localAlgorithms = PRemoveConst(PStringList,&algorithms);
    *m_localAlgorithms = m_algorithms;
    return (m_algorithms.GetSize() > 0);
}

#endif

