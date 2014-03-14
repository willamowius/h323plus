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
 * $Id$
 *
 *
 */


#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H235

#include "h235/h235caps.h"
#include "h235/h235chan.h"
#include "h235/h235support.h"
#include "h323.h"

/////////////////////////////////////////////////////////////////////////////

H235SecurityCapability::H235SecurityCapability(H323Capabilities * capabilities, unsigned capabilityNo)
: m_capabilities(capabilities), m_capNumber(capabilityNo)
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
    return m_capNumber;
}

PString H235SecurityCapability::GetFormatName() const
{   
    PStringStream name;
    name << "SecCapability [" << m_capNumber << "]";
    return name;
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

PString H235SecurityCapability::GetAlgorithm()
{
   if (m_capList.GetSize() > 0)
      return m_capList[0];
   else
      return PString();
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
  PTRACE(1, "Codec\tCannot have Security Capability in DataType. Capability " << m_capNumber);
  return FALSE;
}

PBoolean H235SecurityCapability::OnSendingPDU(H245_ModeElement &) const
{
  PTRACE(1, "Codec\tCannot have Security Capability in ModeElement");
  return FALSE;
}

PBoolean H235SecurityCapability::OnReceivedPDU(const H245_Capability & pdu)
{

  if (pdu.GetTag() != H245_Capability::e_h235SecurityCapability)
      return false;

  const H245_H235SecurityCapability & sec = pdu;
  if (!OnReceivedPDU(sec.m_encryptionAuthenticationAndIntegrity,e_TCS))
      return false;

  const H245_CapabilityTableEntryNumber & capNo = sec.m_mediaCapability;
  PRemoveConst(H235SecurityCapability, this)->SetAssociatedCapability(capNo);
  return true;
}

PBoolean H235SecurityCapability::OnReceivedPDU(const H245_DataType &, PBoolean)
{
  PTRACE(1, "Codec\tCannot have Security Capability in DataType. Capability " << m_capNumber);
  return FALSE;
}

void H235SecurityCapability::SetAssociatedCapability(unsigned capNumber)
{
    m_capNumber = capNumber;
}

PBoolean H235SecurityCapability::MergeAlgorithms(const PStringArray & remote)
{
    PStringArray toKeep;
    for (PINDEX i=0; i< m_capList.GetSize(); ++i) {
       for (PINDEX j=0; j< remote.GetSize(); ++j) {
           if (m_capList[i] == remote[j]) {
               toKeep.AppendString(m_capList[i]);
               break;
           }
       }
    }
    m_capList.SetSize(0);
    m_capList = toKeep;

    if (m_capList.GetSize() > 0) {
        if (m_capabilities) {
            H323Capability * cap = m_capabilities->FindCapability(m_capNumber);
            if (cap) 
                cap->SetEncryptionAlgorithm(m_capList[0]);
        }
        return true;
    }
    return false;
}

PBoolean H235SecurityCapability::OnReceivedPDU(const H245_EncryptionAuthenticationAndIntegrity & encAuth, 
                                               H323Capability::CommandType type) const
{
    if (!encAuth.HasOptionalField(H245_EncryptionAuthenticationAndIntegrity::e_encryptionCapability))
            return false;

    const H245_EncryptionCapability & enc = encAuth.m_encryptionCapability;
    if (type == e_OLC) {
        if (m_capList.GetSize()==0 && enc.GetSize() > 0) {
            PTRACE(4,"H235\tLOGIC ERROR No Agreed algorithms loaded!");
        }
        return true;
    }

    PStringArray other;
      for (PINDEX i=0; i < enc.GetSize(); ++i) {
          const H245_MediaEncryptionAlgorithm & alg = enc[i];
          if (alg.GetTag() == H245_MediaEncryptionAlgorithm::e_algorithm) {
                  const PASN_ObjectId & id = alg;
                  other.AppendString(id.AsString());
          }
      }

    return PRemoveConst(H235SecurityCapability, this)->MergeAlgorithms(other);
}

unsigned H235SecurityCapability::GetDefaultSessionID() const
{
  return OpalMediaFormat::NonRTPSessionID;
}

PBoolean H235SecurityCapability::IsUsable(const H323Connection & connection) const
{
   const H235Capabilities & caps = (const H235Capabilities &)connection.GetLocalCapabilities();
   return caps.GetAlgorithms(m_capList);
}

/////////////////////////////////////////////////////////////////////////////

H323SecureRealTimeCapability::H323SecureRealTimeCapability(H323Capability * childCapability, H323Capabilities * capabilities, unsigned secNo, PBoolean active)
                : ChildCapability(childCapability), chtype(H235ChNew), m_active(active), 
                  m_capabilities(capabilities), m_secNo(secNo),  nrtpqos(NULL)
{
    assignedCapabilityNumber = ChildCapability->GetCapabilityNumber();
}


H323SecureRealTimeCapability::H323SecureRealTimeCapability(RTP_QOS * _rtpqos,H323Capability * childCapability)
                : ChildCapability(childCapability), chtype(H235ChNew), m_active(false), 
                  m_capabilities(NULL), m_secNo(0), nrtpqos(_rtpqos)
{
}

H323SecureRealTimeCapability::~H323SecureRealTimeCapability()
{
    delete ChildCapability;
}


void H323SecureRealTimeCapability::AttachQoS(RTP_QOS * _rtpqos)
{
#ifdef P_QOS
      delete nrtpqos;  
      nrtpqos = _rtpqos;
#endif
}

void H323SecureRealTimeCapability::SetAssociatedCapability(unsigned _secNo)
{
    m_secNo = _secNo;
}

H323Channel * H323SecureRealTimeCapability::CreateChannel(H323Connection & connection,
                                                    H323Channel::Directions dir,
                                                    unsigned sessionID,
                                 const H245_H2250LogicalChannelParameters * param) const
{

  // create a standard RTP channel if we don't have a DH token
  H235Capabilities * caps = dynamic_cast<H235Capabilities*>(connection.GetLocalCapabilitiesRef());
  if (!caps || !caps->GetDiffieHellMan())
    return connection.CreateRealTimeLogicalChannel(*ChildCapability, dir, sessionID, param, nrtpqos);

  // Support for encrypted external RTP Channel
  H323Channel * extRTPChannel = connection.CreateRealTimeLogicalChannel(*this, dir, sessionID, param, nrtpqos);
  if (extRTPChannel)
      return extRTPChannel;

  RTP_Session * session = NULL;              // Session
  if (
#ifdef H323_H46026
     connection.H46026IsMediaTunneled() ||
#endif
     !param || !param->HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel)) {
    // Make a fake transport address from the connection so gets initialised with
    // the transport type (IP, IPX, multicast etc).
    H245_TransportAddress addr;
    connection.GetControlChannel().SetUpTransportPDU(addr, H323Transport::UseLocalTSAP);
    session = connection.UseSession(sessionID, addr, dir, nrtpqos);
  } else {
    session = connection.UseSession(param->m_sessionID, param->m_mediaControlChannel, dir, nrtpqos);
  }

  if (!session)
    return NULL;

  return new H323SecureRTPChannel(connection, *this, dir, *session); 
}

unsigned H323SecureRealTimeCapability::GetCapabilityNumber() const 
{ 
    return ChildCapability->GetCapabilityNumber(); 
};

void H323SecureRealTimeCapability::SetCapabilityNumber(unsigned num) 
{ 
    assignedCapabilityNumber = num;
    ChildCapability->SetCapabilityNumber(num); 
}

void H323SecureRealTimeCapability::SetCapabilityList(H323Capabilities * capabilities)
{
    m_capabilities = capabilities;
}

void H323SecureRealTimeCapability::SetEncryptionActive(PBoolean active)
{
    m_active = active;
}

PBoolean H323SecureRealTimeCapability::IsEncryptionActive() const
{
    return m_active;
}

void H323SecureRealTimeCapability::SetEncryptionAlgorithm(const PString & alg)
{
    m_algorithm = alg;
}

const PString & H323SecureRealTimeCapability::GetEncryptionAlgorithm() const
{
   return m_algorithm;
}

const OpalMediaFormat & H323SecureRealTimeCapability::GetMediaFormat() const
{
   return ChildCapability->GetMediaFormat();
}

OpalMediaFormat & H323SecureRealTimeCapability::GetWritableMediaFormat()
{
   return ChildCapability->GetWritableMediaFormat();
}

///////////////////////////////////////////////////////////////////////////// 

H323SecureCapability::H323SecureCapability(H323Capability & childCapability,
                                                     H235ChType Ch,
                                                     H323Capabilities * capabilities,
                                                     unsigned secNo,
                                                     PBoolean active
                                                     )
   : H323SecureRealTimeCapability((H323Capability *)childCapability.Clone(), capabilities, secNo, active)
{
    chtype = Ch;
}

H323Capability::MainTypes H323SecureCapability::GetMainType() const
{ 
    return ChildCapability->GetMainType();
}


PObject * H323SecureCapability::Clone() const
{
    PTRACE(4, "H235RTP\tCloning Capability: " << GetFormatName());

    H235ChType ch = H235ChNew;

    switch (chtype) {
    case H235ChNew:    
           ch = H235ChClone;
        break;
    case H235ChClone:
           ch = H235Channel;
        break;
    case H235Channel:
           ch = H235Channel;
        break;
    }

  return new H323SecureCapability(*ChildCapability, ch, m_capabilities, m_secNo, m_active);
}

PBoolean H323SecureCapability::IsMatch(const PASN_Choice & subTypePDU) const
{

    if (PIsDescendant(&subTypePDU, H245_AudioCapability) &&
       ChildCapability->GetMainType() == H323Capability::e_Audio) { 
          const H245_AudioCapability & audio = (const H245_AudioCapability &)subTypePDU;
          return ChildCapability->IsMatch(audio);
    }

    if (PIsDescendant(&subTypePDU, H245_VideoCapability) &&
       ChildCapability->GetMainType() == H323Capability::e_Video) { 
          const H245_VideoCapability & video = (const H245_VideoCapability &)subTypePDU;
          return ChildCapability->IsMatch(video);
    }

    if (PIsDescendant(&subTypePDU, H245_DataApplicationCapability_application) &&
       ChildCapability->GetMainType() == H323Capability::e_Data) { 
          const H245_DataApplicationCapability_application & data = 
                         (const H245_DataApplicationCapability_application &)subTypePDU;
          return ChildCapability->IsMatch(data);
    }

    if (PIsDescendant(&subTypePDU, H245_H235Media_mediaType)) { 
          const H245_H235Media_mediaType & data = 
                          (const H245_H235Media_mediaType &)subTypePDU;
          return IsSubMatch(data);
    }
    return false;
}


PBoolean H323SecureCapability::IsSubMatch(const PASN_Choice & subTypePDU) const
{
    const H245_H235Media_mediaType & dataType = (const H245_H235Media_mediaType &)subTypePDU;
    if (dataType.GetTag() == H245_H235Media_mediaType::e_audioData &&
       ChildCapability->GetMainType() == H323Capability::e_Audio) { 
          const H245_AudioCapability & audio = dataType;
          return ChildCapability->IsMatch(audio);
    }

    if (dataType.GetTag() == H245_H235Media_mediaType::e_videoData &&
       ChildCapability->GetMainType() == H323Capability::e_Video) { 
          const H245_VideoCapability & video = dataType;
          return ChildCapability->IsMatch(video);
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

  return ChildCapability->Compare(*(other.GetChildCapability()));
}

unsigned H323SecureCapability::GetDefaultSessionID() const
{
    return ChildCapability->GetDefaultSessionID();
}

////////////////////////////////////////////////////////////////////////
// PDU Sending

PBoolean H323SecureCapability::OnSendingPDU(H245_ModeElement & mode) const
{
    switch (ChildCapability->GetMainType()) {
        case H323Capability::e_Audio:
            return ((H323AudioCapability *)ChildCapability)->OnSendingPDU(mode);
        case H323Capability::e_Video:
            return ((H323VideoCapability *)ChildCapability)->OnSendingPDU(mode);
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
    if (m_capabilities) {
        secCap = (H235SecurityCapability *)m_capabilities->FindCapability(m_secNo);
        if (secCap && secCap->GetAlgorithmCount() > 0) {
           (PRemoveConst(H323SecureCapability,this))->SetEncryptionActive(true);
           (PRemoveConst(H323SecureCapability,this))->SetEncryptionAlgorithm(secCap->GetAlgorithm());
        }
    }

    if (!IsEncryptionActive()) {
        unsigned txFramesInPacket =0;
        switch (ChildCapability->GetMainType()) {
            case H323Capability::e_Audio:
                dataType.SetTag(H245_DataType::e_audioData);
                txFramesInPacket = ((H323AudioCapability *)ChildCapability)->GetTxFramesInPacket();
                return ((H323AudioCapability *)ChildCapability)->OnSendingPDU((H245_AudioCapability &)dataType, txFramesInPacket, e_OLC);
            case H323Capability::e_Video:
                dataType.SetTag(H245_DataType::e_videoData); 
                return ((H323VideoCapability *)ChildCapability)->OnSendingPDU((H245_VideoCapability &)dataType, e_OLC);
            case H323Capability::e_Data:
            default:
                break;
        }
        return false;
    }

    dataType.SetTag(H245_DataType::e_h235Media);
    H245_H235Media & h235Media = dataType;
    // Load the algorithm
    if (secCap)
      secCap->OnSendingPDU(h235Media.m_encryptionAuthenticationAndIntegrity, e_OLC);
   
    H245_H235Media_mediaType & cType = h235Media.m_mediaType;
    unsigned txFramesInPacket =0;
    switch (ChildCapability->GetMainType()) {
        case H323Capability::e_Audio: 
            cType.SetTag(H245_H235Media_mediaType::e_audioData); 
            txFramesInPacket = ((H323AudioCapability *)ChildCapability)->GetTxFramesInPacket();
            return ((H323AudioCapability *)ChildCapability)->OnSendingPDU((H245_AudioCapability &)cType, txFramesInPacket, e_OLC);
        case H323Capability::e_Video: 
            cType.SetTag(H245_H235Media_mediaType::e_videoData); 
            return ((H323VideoCapability *)ChildCapability)->OnSendingPDU((H245_VideoCapability &)cType, e_OLC);
        case H323Capability::e_Data: 
        default:
            break;
    }
    return false;
}

PBoolean H323SecureCapability::OnReceivedPDU(const H245_DataType & dataType,PBoolean receiver)
{
    if (dataType.GetTag() != H245_DataType::e_h235Media)
        return ChildCapability->OnReceivedPDU(dataType, receiver);

    const H245_H235Media & h235Media = dataType;

    if (m_capabilities) {
        H235SecurityCapability * secCap = (H235SecurityCapability *)m_capabilities->FindCapability(m_secNo);
        if (!secCap || !secCap->OnReceivedPDU(h235Media.m_encryptionAuthenticationAndIntegrity, e_OLC)) {
            PTRACE(4,"H235\tFailed to locate security capability " << m_secNo);
            return false;
        }
        if (secCap && secCap->GetAlgorithmCount() > 0) {
            SetEncryptionAlgorithm(secCap->GetAlgorithm());
            SetEncryptionActive(true);
        }
    }

    const H245_H235Media_mediaType & mediaType = h235Media.m_mediaType;
    unsigned packetSize = 0; 

    switch (ChildCapability->GetMainType()) {
        case H323Capability::e_Audio: 
            if (mediaType.GetTag() == H245_H235Media_mediaType::e_audioData) {
               packetSize = (receiver) ?
                   ChildCapability->GetRxFramesInPacket() : ChildCapability->GetTxFramesInPacket();
               return ((H323AudioCapability *)ChildCapability)->OnReceivedPDU((const H245_AudioCapability &)mediaType, packetSize, e_OLC);
            }
            break;

        case H323Capability::e_Video: 
            if (mediaType.GetTag() == H245_H235Media_mediaType::e_videoData) 
               return ((H323VideoCapability *)ChildCapability)->OnReceivedPDU((const H245_VideoCapability &)mediaType, e_OLC);
            break;

        case H323Capability::e_Data:
        default:
            break;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
// Child Capability Intercept

PBoolean H323SecureCapability::OnSendingPDU(H245_Capability & pdu) const
{
    switch (ChildCapability->GetMainType()) {
        case H323Capability::e_Audio:
            return ((H323AudioCapability *)ChildCapability)->OnSendingPDU(pdu);
        case H323Capability::e_Video:
            return ((H323VideoCapability *)ChildCapability)->OnSendingPDU(pdu);
        case H323Capability::e_Data:
        case H323Capability::e_UserInput:
        case H323Capability::e_ExtendVideo:
        default:
            return false;
    }
}

PBoolean H323SecureCapability::OnReceivedPDU(const H245_Capability & pdu)
{
    switch (ChildCapability->GetMainType()) {
        case H323Capability::e_Audio:
            return ((H323AudioCapability *)ChildCapability)->OnReceivedPDU(pdu);
        case H323Capability::e_Video:
            return ((H323VideoCapability *)ChildCapability)->OnReceivedPDU(pdu);
        case H323Capability::e_Data:
        case H323Capability::e_UserInput:
        case H323Capability::e_ExtendVideo:
        default:
            return false;
    }
}


PString H323SecureCapability::GetFormatName() const
{
  return ChildCapability->GetFormatName() + (m_active ? " #" : "");
}

unsigned H323SecureCapability::GetSubType() const
{
  return ChildCapability->GetSubType();
}

PString H323SecureCapability::GetIdentifier() const
{
  return ChildCapability->GetIdentifier();
}

H323Codec * H323SecureCapability::CreateCodec(H323Codec::Direction direction) const
{
    return ChildCapability->CreateCodec(direction);
}

/////////////////////////////////////////////////////////////////////////////


H323SecureDataCapability::H323SecureDataCapability(H323Capability & childCapability, enum H235ChType Ch, H323Capabilities * capabilities, 
                           unsigned secNo, PBoolean active )
: ChildCapability((H323Capability *)childCapability.Clone()), chtype(Ch), m_active(active), m_capabilities(capabilities), 
  m_secNo(secNo), m_algorithm(PString())
{
    assignedCapabilityNumber = childCapability.GetCapabilityNumber();
}

H323SecureDataCapability::~H323SecureDataCapability()
{
    delete ChildCapability;
}

void H323SecureDataCapability::SetAssociatedCapability(unsigned _secNo)
{
    m_secNo = _secNo;
}

	 
PObject::Comparison H323SecureDataCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323SecureDataCapability))
    return LessThan;

  Comparison result = H323Capability::Compare(obj);
  if (result != EqualTo) 
    return result;

  const H323SecureDataCapability & other = (const H323SecureDataCapability &)obj;

  return ChildCapability->Compare(*(other.GetChildCapability()));
}
	
PObject * H323SecureDataCapability::Clone() const
{
    PTRACE(4, "H235Data\tCloning Capability: " << GetFormatName());

    H235ChType ch = H235ChNew;
    switch (chtype) {
    case H235ChNew:    
           ch = H235ChClone;
        break;
    case H235ChClone:
           ch = H235Channel;
        break;
    case H235Channel:
           ch = H235Channel;
        break;
    }
    return new H323SecureDataCapability(*ChildCapability, ch, m_capabilities, m_secNo, m_active);
}


unsigned H323SecureDataCapability::GetSubType() const
{
  return ChildCapability->GetSubType();
}
	

PString H323SecureDataCapability::GetFormatName() const
{
  return ChildCapability->GetFormatName() + (m_active ? " #" : "");
}


PBoolean H323SecureDataCapability::IsMatch(const PASN_Choice & subTypePDU) const
{
    if (PIsDescendant(&subTypePDU, H245_DataApplicationCapability_application) &&
       ChildCapability->GetMainType() == H323Capability::e_Data) { 
          const H245_DataApplicationCapability_application & data = 
                         (const H245_DataApplicationCapability_application &)subTypePDU;
          return ChildCapability->IsMatch(data);
    }

    if (PIsDescendant(&subTypePDU, H245_H235Media_mediaType)) { 
          const H245_H235Media_mediaType & data = 
                          (const H245_H235Media_mediaType &)subTypePDU;
          return IsSubMatch(data);
    }
    return false;
}

PBoolean H323SecureDataCapability::IsSubMatch(const PASN_Choice & subTypePDU) const
{
    const H245_H235Media_mediaType & dataType = (const H245_H235Media_mediaType &)subTypePDU;

    if (dataType.GetTag() == H245_H235Media_mediaType::e_data &&
       ChildCapability->GetMainType() == H323Capability::e_Data) { 
          const H245_DataApplicationCapability & data = dataType;
          return ChildCapability->IsMatch(data.m_application);
    }
    return false;
}

void H323SecureDataCapability::SetEncryptionActive(PBoolean active)
{
    m_active = active;
}

PBoolean H323SecureDataCapability::IsEncryptionActive() const
{
    return m_active;
}

void H323SecureDataCapability::SetEncryptionAlgorithm(const PString & alg)
{
    m_algorithm = alg;
}

const PString & H323SecureDataCapability::GetEncryptionAlgorithm() const
{
   return m_algorithm;
}

H323Channel * H323SecureDataCapability::CreateChannel(H323Connection & connection,
                                      H323Channel::Directions dir,
                                      unsigned sessionID,
                                      const H245_H2250LogicalChannelParameters * param) const
{

    H235Capabilities * caps = dynamic_cast<H235Capabilities*>(connection.GetLocalCapabilitiesRef());
    if (!caps || !caps->GetDiffieHellMan())
        return ChildCapability->CreateChannel(connection, dir, sessionID, param);

    return new H323SecureChannel(connection, *this, ChildCapability->CreateChannel(connection, dir, sessionID, param));
}

PBoolean H323SecureDataCapability::OnSendingPDU(H245_Capability & pdu) const
{
   return ((H323DataCapability *)ChildCapability)->OnSendingPDU(pdu);
}

PBoolean H323SecureDataCapability::OnReceivedPDU(const H245_Capability & pdu)
{
   return ((H323DataCapability *)ChildCapability)->OnReceivedPDU(pdu);
}

PBoolean H323SecureDataCapability::OnSendingPDU(H245_ModeElement & mode) const
{
   return ((H323DataCapability *)ChildCapability)->OnSendingPDU(mode);
}

PBoolean H323SecureDataCapability::OnSendingPDU(H245_DataType & dataType) const
{
    // find the matching H235SecurityCapability to get the agreed algorithms
    // if not found or no matching algorithm then assume no encryption.
    H235SecurityCapability * secCap = NULL;
    if (m_capabilities) {
        secCap = (H235SecurityCapability *)m_capabilities->FindCapability(m_secNo);
        if (secCap && secCap->GetAlgorithmCount() > 0) {
           (PRemoveConst(H323SecureDataCapability,this))->SetEncryptionActive(true);
           (PRemoveConst(H323SecureDataCapability,this))->SetEncryptionAlgorithm(secCap->GetAlgorithm());
        }
    }

    if (!IsEncryptionActive())
        return ((H323DataCapability *)ChildCapability)->OnSendingPDU(dataType);

    dataType.SetTag(H245_DataType::e_h235Media);
    H245_H235Media & h235Media = dataType;
    // Load the algorithm
    if (secCap)
      secCap->OnSendingPDU(h235Media.m_encryptionAuthenticationAndIntegrity, e_OLC);
   
    H245_H235Media_mediaType & cType = h235Media.m_mediaType;
    cType.SetTag(H245_H235Media_mediaType::e_data);
    return ((H323DataCapability *)ChildCapability)->OnSendingPDU((H245_DataApplicationCapability &)cType, e_OLC);
}


PBoolean H323SecureDataCapability::OnReceivedPDU(const H245_DataType & dataType, PBoolean receiver)
{
    if (dataType.GetTag() != H245_DataType::e_h235Media)
        return ChildCapability->OnReceivedPDU(dataType, receiver);

    const H245_H235Media & h235Media = dataType;
    if (m_capabilities) {
        H235SecurityCapability * secCap = (H235SecurityCapability *)m_capabilities->FindCapability(m_secNo);
        if (!secCap || !secCap->OnReceivedPDU(h235Media.m_encryptionAuthenticationAndIntegrity, e_OLC)) {
            PTRACE(4,"H235\tFailed to locate security capability " << m_secNo);
            return false;
        }
        if (secCap && secCap->GetAlgorithmCount() > 0) {
            SetEncryptionAlgorithm(secCap->GetAlgorithm());
            SetEncryptionActive(true);
        }
    }
    const H245_H235Media_mediaType & mediaType = h235Media.m_mediaType;
    if (mediaType.GetTag() == H245_H235Media_mediaType::e_data) 
        return ((H323DataCapability *)ChildCapability)->OnReceivedPDU((const H245_DataApplicationCapability &)mediaType, e_OLC);
    else
        return false;
}


PBoolean H323SecureDataCapability::OnSendingPDU(H245_DataMode & pdu) const
{
    return ((H323DataCapability *)ChildCapability)->OnSendingPDU(pdu);
}


/////////////////////////////////////////////////////////////////////////////

static PStringArray & GetH235Codecs()
{
  static const char * defaultCodecs[] = { "all" };
  static PStringArray codecs(
          sizeof(defaultCodecs)/sizeof(defaultCodecs[0]),
          defaultCodecs
  );
  return codecs;
}

static PMutex & GetH235CodecsMutex()
{
  static PMutex mutex;
  return mutex;
}

/////////////////////////////////////////////////////////////////////////////

H235Capabilities::H235Capabilities()
: m_DHkey(NULL), m_h245Master(false)
{
    m_algorithms.SetSize(0);
}

H235Capabilities::H235Capabilities(const H323Capabilities & original)
: m_DHkey(NULL), m_h245Master(false)
{
  m_algorithms.SetSize(0);
  const H323CapabilitiesSet rset = original.GetSet();

  for (PINDEX i = 0; i < original.GetSize(); i++) {
    unsigned capabilityNumber = original[i].GetCapabilityNumber();
    PINDEX outer=0,middle=0,inner=0;
    for (outer = 0; outer < rset.GetSize(); outer++) {
        for (middle = 0; middle < rset[outer].GetSize(); middle++) {
          for (inner = 0; inner < rset[outer][middle].GetSize(); inner++) {
              if (rset[outer][middle][inner].GetCapabilityNumber() == capabilityNumber) {
                 WrapCapability(outer, middle, original[i]);
                 break;
              }
          }
          if (rset[outer][middle].GetSize() == 0) {
             WrapCapability(outer, middle, original[i]);
             break;
          }
        }
        if (rset[outer].GetSize() == 0) {
           WrapCapability(outer, middle, original[i]);
           break;
        }
    }
  }
}

H235Capabilities::H235Capabilities(const H323Connection & connection, const H245_TerminalCapabilitySet & pdu)
 : H323Capabilities(connection, pdu), m_DHkey(NULL), m_h245Master(false)
{
   const H235Capabilities & localCapabilities = (const H235Capabilities &)connection.GetLocalCapabilities();
   PRemoveConst(H235Capabilities,&localCapabilities)->GetDHKeyPair(m_algorithms, m_DHkey, m_h245Master);
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

void H235Capabilities::AddSecure(PINDEX descriptorNum, PINDEX simultaneous, H323Capability * capability)
{
    if (capability == NULL)
        return;

    if (!PIsDescendant(capability,H323SecureCapability) &&
        !PIsDescendant(capability,H323SecureDataCapability) &&
        !PIsDescendant(capability,H235SecurityCapability))
            return;

    // See if already added, confuses things if you add the same instance twice
    if (table.GetObjectsIndex(capability) != P_MAX_INDEX)
        return;

    // Create the secure capability wrapper
    unsigned capNumber = SetCapabilityNumber(table, capability->GetCapabilityNumber());
    unsigned secNumber = 100+capNumber;
 
    capability->SetCapabilityNumber(capNumber);
    SetCapability(descriptorNum, simultaneous, capability);

    // Create the security capability
    H235SecurityCapability * secCap = new H235SecurityCapability(this, capNumber);
    secCap->SetCapabilityNumber(secNumber); 
    SetCapability(descriptorNum, simultaneous, secCap);

    capability->SetCapabilityList(this);
    capability->SetAssociatedCapability(secNumber);

    PTRACE(3, "H323\tAdded Secure Capability: " << *capability);
}

H323Capability * H235Capabilities::CopySecure(PINDEX descriptorNum, PINDEX simultaneous, const H323Capability & capability)
{
  if (!PIsDescendant(&capability,H323SecureCapability) &&
      !PIsDescendant(&capability,H323SecureDataCapability) &&
      !PIsDescendant(&capability,H235SecurityCapability))
      return NULL;

  if (PIsDescendant(&capability,H235SecurityCapability)) {
     H235SecurityCapability * newCapability = (H235SecurityCapability *)capability.Clone();
     newCapability->SetCapabilityNumber(capability.GetCapabilityNumber());
     table.Append(newCapability);
     SetCapability(descriptorNum, simultaneous, newCapability);
     return newCapability;
  } else {
     H323Capability * newCapability = (H323Capability *)capability.Clone();
     newCapability->SetCapabilityNumber(capability.GetCapabilityNumber());
     newCapability->SetCapabilityList(this);
     SetCapability(descriptorNum, simultaneous, newCapability);
     PTRACE(3, "H323\tCopied Secure Capability: " << *newCapability);
     return newCapability;
  }
}

void H235Capabilities::WrapCapability(PINDEX descriptorNum, PINDEX simultaneous, H323Capability & capability)
{

    if (PIsDescendant(&capability,H323SecureCapability) ||
        PIsDescendant(&capability,H323SecureDataCapability) ||
        PIsDescendant(&capability,H235SecurityCapability)) {
          CopySecure(descriptorNum, simultaneous, capability);
          return;
    }

    if (!IsH235Codec(capability.GetFormatName())) {
        SetCapability(descriptorNum, simultaneous, (H323Capability *)capability.Clone());
        return;
    }

    switch (capability.GetDefaultSessionID()) {
        case OpalMediaFormat::DefaultAudioSessionID:
        case OpalMediaFormat::DefaultVideoSessionID:
            AddSecure(descriptorNum, simultaneous, new H323SecureCapability(capability, H235ChNew,this));                    
            break;
        case OpalMediaFormat::DefaultDataSessionID:
            AddSecure(descriptorNum, simultaneous, new H323SecureDataCapability(capability, H235ChNew,this));   
            break;
        case OpalMediaFormat::NonRTPSessionID:
        //case OpalMediaFormat::DefaultDataSessionID:
        case OpalMediaFormat::DefaultExtVideoSessionID:
        case OpalMediaFormat::DefaultFileSessionID:
        default:
            SetCapability(descriptorNum, simultaneous, (H323Capability *)capability.Clone());
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
          H323Capability * newCapability = NULL;
          PINDEX num=0;
            switch (session) {
                case OpalMediaFormat::DefaultAudioSessionID:
                case OpalMediaFormat::DefaultVideoSessionID:
                    newCapability = new H323SecureCapability(*capability, H235ChNew, this);
                    num = SetCapability(descriptorNum, simultaneous, newCapability);
                    num = SetCapability(descriptorNum, simultaneous, 
                                        new H235SecurityCapability(this, newCapability->GetCapabilityNumber()));
                    break;
                case OpalMediaFormat::DefaultDataSessionID:
                    newCapability = new H323SecureDataCapability(*capability, H235ChNew, this);
                    num = SetCapability(descriptorNum, simultaneous, newCapability);
                    num = SetCapability(descriptorNum, simultaneous, 
                                        new H235SecurityCapability(this, newCapability->GetCapabilityNumber()));
                    break;
                case OpalMediaFormat::DefaultExtVideoSessionID:
                case OpalMediaFormat::DefaultFileSessionID:
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
    m_algorithms.SetSize(0);
    for (PINDEX i=0; i < keyOIDs.GetSize(); ++i)
         m_algorithms.AppendString(keyOIDs[i]);

     m_DHkey = key;
     m_h245Master = isMaster;

     PTRACE(2,"H235\tDiffieHellman selected. Key " << (isMaster ? "Master" : "Slave"));

}

void H235Capabilities::GetDHKeyPair(PStringList & keyOIDs, H235_DiffieHellman * & key, PBoolean & isMaster)
{
    for (PINDEX i=0; i < m_algorithms.GetSize(); ++i)
         keyOIDs.AppendString(m_algorithms[i]);

    if (m_DHkey)
         key = m_DHkey;

     isMaster = m_h245Master;
}

PBoolean H235Capabilities::GetAlgorithms(const PStringList & algorithms) const
{
    PStringList * m_localAlgorithms = PRemoveConst(PStringList,&algorithms);
    m_localAlgorithms->SetSize(0);
    for (PINDEX i=0; i < m_algorithms.GetSize(); ++i)
         m_localAlgorithms->AppendString(m_algorithms[i]);

    return (algorithms.GetSize() > 0);
}

void H235Capabilities::SetH235Codecs(const PStringArray & servers)
{
     PWaitAndSignal m(GetH235CodecsMutex());
     GetH235Codecs() = servers;
}

PBoolean H235Capabilities::IsH235Codec(const PString & name)
{
    PStringArray codecs = GetH235Codecs();

    if ((codecs.GetSize() == 0) || (codecs[0] *= "all"))
        return true;
    
    for (PINDEX i=0; i<codecs.GetSize(); ++i) {
        if (name.Find(codecs[i]) != P_MAX_INDEX)
            return true;
    }
    return false;
}

#endif

