/*
 * h323caps.cxx
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

#ifdef __GNUC__
#pragma implementation "h323caps.h"
#endif

#include <ptlib.h>

#include "h323caps.h"
#include "h323ep.h"
#include "h323pdu.h"
#include "h323neg.h"
#include "codec/opalplugin.h"
#include "mediafmt.h"

#include <algorithm>

#define DEFINE_G711_CAPABILITY(cls, code, capName) \
class cls : public H323_G711Capability { \
  public: \
    cls() : H323_G711Capability(code) { } \
}; \
H323_REGISTER_CAPABILITY(cls, capName) \

#ifdef H323_AUDIO_CODECS

DEFINE_G711_CAPABILITY(H323_G711ALaw64Capability, H323_G711Capability::ALaw, "G.711-ALaw-64k{sw}")
DEFINE_G711_CAPABILITY(H323_G711uLaw64Capability, H323_G711Capability::muLaw, "G.711-uLaw-64k{sw}")

#endif

#define new PNEW


#if PTRACING
ostream & operator<<(ostream & o , H323Capability::MainTypes t)
{
  const char * const names[] = {
    "Audio", "Video", "Data", "UserInput"
  };
  return o << names[t];
}

ostream & operator<<(ostream & o , H323Capability::CapabilityDirection d)
{
  const char * const names[] = {
    "Unknown", "Receive", "Transmit", "ReceiveAndTransmit", "NoDirection"
  };
  return o << names[d];
}
#endif


/////////////////////////////////////////////////////////////////////////////

H323Capability::H323Capability()
{
  assignedCapabilityNumber = 0; // Unassigned
  capabilityDirection = e_Unknown;
  rtpPayloadType = RTP_DataFrame::IllegalPayloadType;
}


PObject::Comparison H323Capability::Compare(const PObject & obj) const
{
  PAssert(PIsDescendant(&obj, H323Capability), PInvalidCast);
  const H323Capability & other = (const H323Capability &)obj;

  int mt = GetMainType();
  int omt = other.GetMainType();
  if (mt < omt)
    return LessThan;
  if (mt > omt)
    return GreaterThan;

  int st = GetSubType();
  int ost = other.GetSubType();
  if (st < ost)
    return LessThan;
  if (st > ost)
    return GreaterThan;

  PString id = GetIdentifier();
  if (!id && id != other.GetIdentifier())
	  return LessThan;

  return EqualTo;
}


void H323Capability::PrintOn(ostream & strm) const
{
  strm << GetFormatName();
  if (assignedCapabilityNumber != 0)
    strm << " <" << assignedCapabilityNumber << '>';
}

PString H323Capability::GetIdentifier() const
{
	return PString();
}

H323Capability * H323Capability::Create(const PString & name)
{
  H323Capability * cap = H323CapabilityFactory::CreateInstance(name);
  if (cap == NULL)
    return NULL;

  return (H323Capability *)cap->Clone();
}

OpalFactoryCodec * H323Capability::CreateCodec(MainTypes ctype, PBoolean isEncoder, const PString & name)
{
	// Build the conversion
	PString base;
	switch (ctype) {
		case e_Audio: base = "L16"; break;
		case e_Video: base = "YUV420P"; break;
		default: base = PString();
	}

	PString conv;
	if (isEncoder) conv = base + "|" + name;
	else conv = name + "|" + base;

	return H323PluginCodecManager::CreateCodec(conv);
}

void H323Capability::CodecListing(MainTypes ctype, PBoolean isEncoder, PStringList & listing)
{
	// Build the conversion
	PString base;
	switch (ctype) {
		case e_Audio: base = "L16"; break;
		case e_Video: base = "YUV420P"; break;
		default: base = PString();
	}

	PString match;
	if (isEncoder) match = base + "|";
	else match = "|" + base;

    H323PluginCodecManager::CodecListing(match,listing);
}

unsigned H323Capability::GetDefaultSessionID() const
{
  return 0;
}


void H323Capability::SetTxFramesInPacket(unsigned /*frames*/)
{
}


unsigned H323Capability::GetTxFramesInPacket() const
{
  return 1;
}


unsigned H323Capability::GetRxFramesInPacket() const
{
  return 1;
}


PBoolean H323Capability::IsMatch(const PASN_Choice & subTypePDU) const
{
  return subTypePDU.GetTag() == GetSubType();
}


PBoolean H323Capability::OnReceivedPDU(const H245_Capability & cap)
{
  switch (cap.GetTag()) {
    case H245_Capability::e_receiveVideoCapability:
    case H245_Capability::e_receiveAudioCapability:
    case H245_Capability::e_receiveDataApplicationCapability:
    case H245_Capability::e_h233EncryptionReceiveCapability:
    case H245_Capability::e_receiveUserInputCapability:
      capabilityDirection = e_Receive;
      break;

    case H245_Capability::e_transmitVideoCapability:
    case H245_Capability::e_transmitAudioCapability:
    case H245_Capability::e_transmitDataApplicationCapability:
    case H245_Capability::e_h233EncryptionTransmitCapability:
    case H245_Capability::e_transmitUserInputCapability:
      capabilityDirection = e_Transmit;
      break;

    case H245_Capability::e_receiveAndTransmitVideoCapability:
    case H245_Capability::e_receiveAndTransmitAudioCapability:
    case H245_Capability::e_receiveAndTransmitDataApplicationCapability:
    case H245_Capability::e_receiveAndTransmitUserInputCapability:
      capabilityDirection = e_ReceiveAndTransmit;
      break;

    case H245_Capability::e_conferenceCapability:
	case H245_Capability::e_genericControlCapability:
    case H245_Capability::e_h235SecurityCapability:
    case H245_Capability::e_maxPendingReplacementFor:
      capabilityDirection = e_NoDirection;
  }

  return TRUE;
}


PBoolean H323Capability::IsUsable(const H323Connection &) const
{
  return TRUE;
}


const OpalMediaFormat & H323Capability::GetMediaFormat() const
{
  return PRemoveConst(H323Capability, this)->GetWritableMediaFormat();
}


OpalMediaFormat & H323Capability::GetWritableMediaFormat()
{
  if (mediaFormat.IsEmpty()) {
    PString name = GetFormatName();
    name.Delete(name.FindLast('{'), 4);
    mediaFormat = OpalMediaFormat(name);
  }
  return mediaFormat;
}


/////////////////////////////////////////////////////////////////////////////

H323RealTimeCapability::H323RealTimeCapability()
{
    rtpqos = NULL;
}

H323RealTimeCapability::H323RealTimeCapability(const H323RealTimeCapability & rtc)
  : H323Capability(rtc)
{
#if P_QOS
  if (rtc.rtpqos != NULL) 
    rtpqos  = new RTP_QOS(*rtc.rtpqos);
  else
#endif
    rtpqos = NULL;
}

H323RealTimeCapability::~H323RealTimeCapability()
{
#if P_QOS
  delete rtpqos;
#endif
}

void H323RealTimeCapability::AttachQoS(RTP_QOS * _rtpqos)
{
#if P_QOS
  delete rtpqos;
#endif
    
  rtpqos = _rtpqos;
}

H323Channel * H323RealTimeCapability::CreateChannel(H323Connection & connection,
                                                    H323Channel::Directions dir,
                                                    unsigned sessionID,
                                 const H245_H2250LogicalChannelParameters * param) const
{
  return connection.CreateRealTimeLogicalChannel(*this, dir, sessionID, param, rtpqos);
}


/////////////////////////////////////////////////////////////////////////////

H323NonStandardCapabilityInfo::H323NonStandardCapabilityInfo(CompareFuncType _compareFunc,
                                                             const BYTE * dataPtr,
                                                             PINDEX dataSize)
  :
    t35CountryCode(0),
    t35Extension(0),
    manufacturerCode(0),
    nonStandardData(dataPtr, dataSize == 0 && dataPtr != NULL
                                 ? strlen((const char *)dataPtr) : dataSize),
    comparisonOffset(0),
    comparisonLength(0),
    compareFunc(_compareFunc)
{
}

H323NonStandardCapabilityInfo::H323NonStandardCapabilityInfo(const BYTE * dataPtr,
                                                             PINDEX dataSize,
                                                             PINDEX _offset,
                                                             PINDEX _len)
  : t35CountryCode(H323EndPoint::defaultT35CountryCode),
    t35Extension(H323EndPoint::defaultT35Extension),
    manufacturerCode(H323EndPoint::defaultManufacturerCode),
    nonStandardData(dataPtr, dataSize == 0 && dataPtr != NULL
                                 ? strlen((const char *)dataPtr) : dataSize),
    comparisonOffset(_offset),
    comparisonLength(_len),
    compareFunc(NULL)
{
}


H323NonStandardCapabilityInfo::H323NonStandardCapabilityInfo(const PString & _oid,
                                                             const BYTE * dataPtr,
                                                             PINDEX dataSize,
                                                             PINDEX _offset,
                                                             PINDEX _len)
  : oid(_oid),
    nonStandardData(dataPtr, dataSize == 0 && dataPtr != NULL
                                 ? strlen((const char *)dataPtr) : dataSize),
    comparisonOffset(_offset),
    comparisonLength(_len),
    compareFunc(NULL)
{
}


H323NonStandardCapabilityInfo::H323NonStandardCapabilityInfo(BYTE country,
                                                             BYTE extension,
                                                             WORD maufacturer,
                                                             const BYTE * dataPtr,
                                                             PINDEX dataSize,
                                                             PINDEX _offset,
                                                             PINDEX _len)
  : t35CountryCode(country),
    t35Extension(extension),
    manufacturerCode(maufacturer),
    nonStandardData(dataPtr, dataSize == 0 && dataPtr != NULL
                                 ? strlen((const char *)dataPtr) : dataSize),
    comparisonOffset(_offset),
    comparisonLength(_len),
    compareFunc(NULL)
{
}


H323NonStandardCapabilityInfo::~H323NonStandardCapabilityInfo()
{
}


PBoolean H323NonStandardCapabilityInfo::OnSendingPDU(PBYTEArray & data) const
{
  data = nonStandardData;
  return data.GetSize() > 0;
}


PBoolean H323NonStandardCapabilityInfo::OnReceivedPDU(const PBYTEArray & data)
{
  if (CompareData(data) != PObject::EqualTo)
    return FALSE;

  nonStandardData = data;
  return TRUE;
}


PBoolean H323NonStandardCapabilityInfo::OnSendingNonStandardPDU(PASN_Choice & pdu,
                                                            unsigned nonStandardTag) const
{
  PBYTEArray data;
  if (!OnSendingPDU(data)) 
    return FALSE;

  pdu.SetTag(nonStandardTag);
  H245_NonStandardParameter & param = (H245_NonStandardParameter &)pdu.GetObject();

  if (!oid) {
    param.m_nonStandardIdentifier.SetTag(H245_NonStandardIdentifier::e_object);
    PASN_ObjectId & nonStandardIdentifier = param.m_nonStandardIdentifier;
    nonStandardIdentifier = oid;
  }
  else {
    param.m_nonStandardIdentifier.SetTag(H245_NonStandardIdentifier::e_h221NonStandard);
    H245_NonStandardIdentifier_h221NonStandard & h221 = param.m_nonStandardIdentifier;
    h221.m_t35CountryCode = (unsigned)t35CountryCode;
    h221.m_t35Extension = (unsigned)t35Extension;
    h221.m_manufacturerCode = (unsigned)manufacturerCode;
  }

  param.m_data = data;
  return data.GetSize() > 0;
}


PBoolean H323NonStandardCapabilityInfo::OnReceivedNonStandardPDU(const PASN_Choice & pdu,
                                                             unsigned nonStandardTag)
{
  if (pdu.GetTag() != nonStandardTag)
    return FALSE;

  const H245_NonStandardParameter & param = (const H245_NonStandardParameter &)pdu.GetObject();

  if (CompareParam(param) != PObject::EqualTo)
    return FALSE;

  return OnReceivedPDU(param.m_data);
}


PBoolean H323NonStandardCapabilityInfo::IsMatch(const H245_NonStandardParameter & param) const
{
  return CompareParam(param) == PObject::EqualTo && CompareData(param.m_data) == PObject::EqualTo;
}


PObject::Comparison H323NonStandardCapabilityInfo::CompareParam(const H245_NonStandardParameter & param) const
{
  if (compareFunc != NULL) {

    PluginCodec_H323NonStandardCodecData compareData;

    PString objectId;
    if (param.m_nonStandardIdentifier.GetTag() == H245_NonStandardIdentifier::e_object) {
      const PASN_ObjectId & nonStandardIdentifier = param.m_nonStandardIdentifier;
      objectId = nonStandardIdentifier.AsString();
      compareData.objectId = objectId;
    } else {
      const H245_NonStandardIdentifier_h221NonStandard & h221 = param.m_nonStandardIdentifier;
      compareData.objectId         = NULL;
      compareData.t35CountryCode   = (unsigned char)h221.m_t35CountryCode;
      compareData.t35Extension     = (unsigned char)h221.m_t35Extension;
      compareData.manufacturerCode = (unsigned short)h221.m_manufacturerCode;
    }
    const PBYTEArray & data = param.m_data;
    compareData.data       = (const unsigned char *)data;
    compareData.dataLength = data.GetSize();
    return (PObject::Comparison)(*compareFunc)(&compareData);
  }

  if (!oid) {
    if (param.m_nonStandardIdentifier.GetTag() != H245_NonStandardIdentifier::e_object)
      return PObject::LessThan;

    const PASN_ObjectId & nonStandardIdentifier = param.m_nonStandardIdentifier;
    PObject::Comparison cmp = oid.Compare(nonStandardIdentifier.AsString());
    if (cmp != PObject::EqualTo)
      return cmp;
  }
  else {
    if (param.m_nonStandardIdentifier.GetTag() != H245_NonStandardIdentifier::e_h221NonStandard)
      return PObject::LessThan;

    const H245_NonStandardIdentifier_h221NonStandard & h221 = param.m_nonStandardIdentifier;

    if (h221.m_t35CountryCode < (unsigned)t35CountryCode)
      return PObject::LessThan;
    if (h221.m_t35CountryCode > (unsigned)t35CountryCode)
      return PObject::GreaterThan;

    if (h221.m_t35Extension < (unsigned)t35Extension)
      return PObject::LessThan;
    if (h221.m_t35Extension > (unsigned)t35Extension)
      return PObject::GreaterThan;

    if (h221.m_manufacturerCode < (unsigned)manufacturerCode)
      return PObject::LessThan;
    if (h221.m_manufacturerCode > (unsigned)manufacturerCode)
      return PObject::GreaterThan;
  }

  return CompareData(param.m_data);
}


PObject::Comparison H323NonStandardCapabilityInfo::CompareInfo(const H323NonStandardCapabilityInfo & other) const
{
  if (compareFunc != NULL) {

    PluginCodec_H323NonStandardCodecData compareData;

    PString objectId;
    if (!other.oid.IsEmpty())
      compareData.objectId = other.oid;
    else {
      compareData.objectId         = NULL;
      compareData.t35CountryCode   = other.t35CountryCode;
      compareData.t35Extension     = other.t35Extension;
      compareData.manufacturerCode = other.manufacturerCode;
    }
    compareData.data       = (const unsigned char *)other.nonStandardData;
    compareData.dataLength = other.nonStandardData.GetSize();

    return (*compareFunc)(&compareData);
  }

  if (!oid) {
    if (other.oid.IsEmpty())
      return PObject::LessThan;

    PObject::Comparison cmp = oid.Compare(other.oid);
    if (cmp != PObject::EqualTo)
      return cmp;
  }
  else {
    if (other.t35CountryCode < t35CountryCode)
      return PObject::LessThan;
    if (other.t35CountryCode > t35CountryCode)
      return PObject::GreaterThan;

    if (other.t35Extension < t35Extension)
      return PObject::LessThan;
    if (other.t35Extension > t35Extension)
      return PObject::GreaterThan;

    if (other.manufacturerCode < manufacturerCode)
      return PObject::LessThan;
    if (other.manufacturerCode > manufacturerCode)
      return PObject::GreaterThan;
  }

  return CompareData(other.nonStandardData);
}


PObject::Comparison H323NonStandardCapabilityInfo::CompareData(const PBYTEArray & data) const
{
  if (comparisonOffset >= nonStandardData.GetSize())
    return PObject::LessThan;
  if (comparisonOffset >= data.GetSize())
    return PObject::GreaterThan;

  PINDEX len = comparisonLength;
  if (comparisonOffset+len > nonStandardData.GetSize())
    len = nonStandardData.GetSize() - comparisonOffset;

  if (comparisonOffset+len > data.GetSize())
    return PObject::GreaterThan;

  int cmp = memcmp((const BYTE *)nonStandardData + comparisonOffset,
                   (const BYTE *)data + comparisonOffset,
                   len);
  if (cmp < 0)
    return PObject::LessThan;
  if (cmp > 0)
    return PObject::GreaterThan;
  return PObject::EqualTo;
}


/////////////////////////////////////////////////////////////////////////////

struct GenericOptionOrder {
	PString name;
	PString order;
} OptionOrder[] = {
   { "h.264", "41,42,3,6,4,5,7,10" },
   { "", "" }
};

static void capabilityReorder(const PString & capName, H245_ArrayOf_GenericParameter & gen)
{
	PStringArray list;
	list.SetSize(0);
	H245_ArrayOf_GenericParameter localGen;
    localGen.SetSize(0);
    int j = 0, k = 0;
	
	int i = -1;
    while (!OptionOrder[++i].name) {
        if (capName.Find(OptionOrder[i].name) == P_MAX_INDEX)
            continue;

        list = OptionOrder[i].order.Tokenise(",");
        localGen.SetSize(gen.GetSize());
	    if (list.GetSize() > 0) {
	        for (i=0; i < list.GetSize(); i++) {
                for (j=0; j < gen.GetSize(); j++) {
                    H245_ParameterIdentifier & id = gen[j].m_parameterIdentifier;
                    if (id.GetTag() == H245_ParameterIdentifier::e_standard) {
                        PASN_Integer & val = id;
                        unsigned x = val.GetValue();
                        if ((list[i].AsInteger() == (int)x)) {
                            localGen[k++] = gen[j];
                        }
                    }
                }
	        }
            break;
	    }
    }
    if (localGen.GetSize() > 0) {
	    for (j=0; j < gen.GetSize(); j++) {
	            gen[j] = localGen[j];
	    }
    }
}

H323GenericCapabilityInfo::H323GenericCapabilityInfo(const PString & standardId, PINDEX bitRate)
	: maxBitRate(bitRate)
{
  identifier = new H245_CapabilityIdentifier(H245_CapabilityIdentifier::e_standard);
  PASN_ObjectId & object_id = *identifier;
    object_id = standardId;
}


H323GenericCapabilityInfo::H323GenericCapabilityInfo(const H323GenericCapabilityInfo & obj)
  : maxBitRate(obj.maxBitRate)
{
  identifier = new H245_CapabilityIdentifier(*obj.identifier);
}


H323GenericCapabilityInfo::~H323GenericCapabilityInfo()
{
  delete identifier;
}


PBoolean H323GenericCapabilityInfo::OnSendingGenericPDU(H245_GenericCapability & pdu,
                                                    const OpalMediaFormat & mediaFormat,
                                                    H323Capability::CommandType type) const
{
  pdu.m_capabilityIdentifier = *identifier;

#ifdef H323_VIDEO
  unsigned pbitRate = mediaFormat.GetOptionInteger(OpalVideoFormat::MaxBitRateOption)/100;
  unsigned bitRate = maxBitRate != 0 ? maxBitRate : pbitRate;
  if (pbitRate < bitRate)
        bitRate = pbitRate;
#else
  unsigned bitRate = maxBitRate;
#endif
  if (bitRate != 0) {
    pdu.IncludeOptionalField(H245_GenericCapability::e_maxBitRate);
    pdu.m_maxBitRate = bitRate;
  }

  for (PINDEX i = 0; i < mediaFormat.GetOptionCount(); i++) {
    const OpalMediaOption & option = mediaFormat.GetOption(i);
    OpalMediaOption::H245GenericInfo genericInfo = option.GetH245Generic();
    if (genericInfo.mode == OpalMediaOption::H245GenericInfo::None)
      continue;
    switch (type) {
      case H323Capability::e_TCS :
        if (genericInfo.excludeTCS)
          continue;
        break;
      case H323Capability::e_OLC :
        if (genericInfo.excludeOLC)
          continue;
        break;
      case H323Capability::e_ReqMode :
        if (genericInfo.excludeReqMode)
          continue;
        break;
    }
    
    H245_GenericParameter * param = new H245_GenericParameter;

    param->m_parameterIdentifier.SetTag(H245_ParameterIdentifier::e_standard);
    (PASN_Integer &)param->m_parameterIdentifier = genericInfo.ordinal;
	unsigned parameterValue = ((const OpalMediaOptionUnsigned &)option).GetValue();

	if (PIsDescendant(&option, OpalMediaOptionUnsigned) && parameterValue == 0)
		continue;

    if (PIsDescendant(&option, OpalMediaOptionBoolean)) {
      if (!((const OpalMediaOptionBoolean &)option).GetValue()) {
        delete param;
        continue; // Do not include a logical at all if it is false
      }
      param->m_parameterValue.SetTag(H245_ParameterValue::e_logical);
    }
    else if (PIsDescendant(&option, OpalMediaOptionUnsigned)) {
      unsigned tag;
      switch (genericInfo.integerType) {
        default :
        case OpalMediaOption::H245GenericInfo::UnsignedInt :
          tag = option.GetMerge() == OpalMediaOption::MinMerge ? H245_ParameterValue::e_unsignedMin : H245_ParameterValue::e_unsignedMax;
          break;

        case OpalMediaOption::H245GenericInfo::Unsigned32 :
          tag = option.GetMerge() == OpalMediaOption::MinMerge ? H245_ParameterValue::e_unsigned32Min : H245_ParameterValue::e_unsigned32Max;
          break;

        case OpalMediaOption::H245GenericInfo::BooleanArray :
          tag = H245_ParameterValue::e_booleanArray;
          break;
      }

      param->m_parameterValue.SetTag(tag);
      (PASN_Integer &)param->m_parameterValue = parameterValue;
    }
    else {
      param->m_parameterValue.SetTag(H245_ParameterValue::e_octetString);
      PASN_OctetString & octetString = param->m_parameterValue;
      if (PIsDescendant(&option, OpalMediaOptionOctets))
        octetString = ((const OpalMediaOptionOctets &)option).GetValue();
      else
        octetString = option.AsString();
    }

    if (genericInfo.mode == OpalMediaOption::H245GenericInfo::Collapsing) {
      pdu.IncludeOptionalField(H245_GenericCapability::e_collapsing);
      pdu.m_collapsing.Append(param);
    }
    else {
      pdu.IncludeOptionalField(H245_GenericCapability::e_nonCollapsing);
      pdu.m_nonCollapsing.Append(param);
    }
  }
    if (pdu.m_collapsing.GetSize() > 0) 
        capabilityReorder(mediaFormat,pdu.m_collapsing);

    return TRUE;
}

PBoolean H323GenericCapabilityInfo::OnReceivedGenericPDU(OpalMediaFormat & mediaFormat,
                                                     const H245_GenericCapability & pdu,
                                                     H323Capability::CommandType type)
{
  if (pdu.m_capabilityIdentifier != *identifier)
    return FALSE;

  if (pdu.HasOptionalField(H245_GenericCapability::e_maxBitRate)) {
    maxBitRate = pdu.m_maxBitRate;
#ifdef H323_VIDEO
    mediaFormat.SetOptionInteger(OpalVideoFormat::MaxBitRateOption, maxBitRate*100);
#else
    mediaFormat.SetOptionInteger(maxBitRate, maxBitRate*100);
#endif
  }

  for (PINDEX i = 0; i < mediaFormat.GetOptionCount(); i++) {
    const OpalMediaOption & option = mediaFormat.GetOption(i);
    OpalMediaOption::H245GenericInfo genericInfo = option.GetH245Generic();
    if (genericInfo.mode == OpalMediaOption::H245GenericInfo::None)
      continue;
    switch (type) {
      case H323Capability::e_TCS :
        if (genericInfo.excludeTCS)
          continue;
        break;
      case H323Capability::e_OLC :
        if (genericInfo.excludeOLC)
          continue;
        break;
      case H323Capability::e_ReqMode :
        if (genericInfo.excludeReqMode)
          continue;
        break;
    }

    const H245_ArrayOf_GenericParameter * params;
    if (genericInfo.mode == OpalMediaOption::H245GenericInfo::Collapsing) {
      if (!pdu.HasOptionalField(H245_GenericCapability::e_collapsing))
        continue;
      params = &pdu.m_collapsing;
    }
    else {
      if (!pdu.HasOptionalField(H245_GenericCapability::e_nonCollapsing))
        continue;
      params = &pdu.m_nonCollapsing;
    }

    if (PIsDescendant(&option, OpalMediaOptionBoolean))
      ((OpalMediaOptionBoolean &)option).SetValue(false); 
    else if (PIsDescendant(&option, OpalMediaOptionUnsigned) && option.GetMerge() == OpalMediaOption::MinMerge)
      ((OpalMediaOptionUnsigned &)option).SetValue(0);

    for (PINDEX j = 0; j < params->GetSize(); j++) {
      const H245_GenericParameter & param = (*params)[j];
      if (param.m_parameterIdentifier.GetTag() == H245_ParameterIdentifier::e_standard &&
                         (const PASN_Integer &)param.m_parameterIdentifier == genericInfo.ordinal) {
        if (PIsDescendant(&option, OpalMediaOptionBoolean)) {
          if (param.m_parameterValue.GetTag() == H245_ParameterValue::e_logical) {
            ((OpalMediaOptionBoolean &)option).SetValue(true);
            break;
          }
        }
        else if (PIsDescendant(&option, OpalMediaOptionUnsigned)) {
          unsigned tag;
          switch (genericInfo.integerType) {
            default :
            case OpalMediaOption::H245GenericInfo::UnsignedInt :
              tag = option.GetMerge() == OpalMediaOption::MinMerge ? H245_ParameterValue::e_unsignedMin : H245_ParameterValue::e_unsignedMax;
              break;
 
            case OpalMediaOption::H245GenericInfo::Unsigned32 :
              tag = option.GetMerge() == OpalMediaOption::MinMerge ? H245_ParameterValue::e_unsigned32Min : H245_ParameterValue::e_unsigned32Max;
              break;

            case OpalMediaOption::H245GenericInfo::BooleanArray :
              tag = H245_ParameterValue::e_booleanArray;
              break;
          }

          if (param.m_parameterValue.GetTag() == tag) {
            ((OpalMediaOptionUnsigned &)option).SetValue((const PASN_Integer &)param.m_parameterValue);
            break;
          }
        }
        else {
          if (param.m_parameterValue.GetTag() == H245_ParameterValue::e_octetString) {
            const PASN_OctetString & octetString = param.m_parameterValue;
            if (PIsDescendant(&option, OpalMediaOptionOctets))
              ((OpalMediaOptionOctets &)option).SetValue(octetString);
            else
              ((OpalMediaOption &)option).FromString(octetString.AsString());
            break;
    }
    }

        PTRACE(2, "Invalid generic parameter type (" << param.m_parameterValue.GetTagName()
               << ") for option \"" << option.GetName() << "\" (" << option.GetClass() << ')');
      }
    }
    }

    return TRUE;
}

PBoolean H323GenericCapabilityInfo::IsMatch(const H245_GenericCapability & param) const
{
  return param.m_capabilityIdentifier == *identifier;
}

PObject::Comparison H323GenericCapabilityInfo::CompareInfo(const H323GenericCapabilityInfo & obj) const
{
  return identifier->Compare(*obj.identifier);
}


/////////////////////////////////////////////////////////////////////////////

#ifdef H323_AUDIO_CODECS
int H323AudioCapability::DSCPvalue = PQoS::guaranteedDSCP;
H323AudioCapability::H323AudioCapability(unsigned rx, unsigned tx)
{
  rxFramesInPacket = rx;
  txFramesInPacket = tx;

#if P_QOS
// Set to G.729 Settings Avg 56kb/s Peek 110 kb/s
	rtpqos = new RTP_QOS;
	rtpqos->dataQoS.SetWinServiceType(SERVICETYPE_GUARANTEED);
	rtpqos->dataQoS.SetAvgBytesPerSec(7000);
	rtpqos->dataQoS.SetMaxFrameBytes(680);
	rtpqos->dataQoS.SetPeakBytesPerSec(13750);
	rtpqos->dataQoS.SetDSCP(DSCPvalue);

	rtpqos->ctrlQoS.SetWinServiceType(SERVICETYPE_CONTROLLEDLOAD);
	rtpqos->ctrlQoS.SetDSCP(PQoS::controlledLoadDSCP); 
#endif
}


H323Capability::MainTypes H323AudioCapability::GetMainType() const
{
  return e_Audio;
}


unsigned H323AudioCapability::GetDefaultSessionID() const
{
  return RTP_Session::DefaultAudioSessionID;
}


void H323AudioCapability::SetTxFramesInPacket(unsigned frames)
{
  PAssert(frames > 0, PInvalidParameter);
  if (frames > 256)
    txFramesInPacket = 256;
  else
    txFramesInPacket = frames;
}


unsigned H323AudioCapability::GetTxFramesInPacket() const
{
  return txFramesInPacket;
}


unsigned H323AudioCapability::GetRxFramesInPacket() const
{
  return rxFramesInPacket;
}

void H323AudioCapability::SetDSCPvalue(int newValue) 
{ 
	if (newValue < 64)
		DSCPvalue = newValue;
}

int H323AudioCapability::GetDSCPvalue() 
{ 
	return DSCPvalue; 
} 

PBoolean H323AudioCapability::OnSendingPDU(H245_Capability & cap) const
{
  switch (capabilityDirection) {
    case e_Transmit:
      cap.SetTag(H245_Capability::e_transmitAudioCapability);
      break;
    case e_ReceiveAndTransmit:
      cap.SetTag(H245_Capability::e_receiveAndTransmitAudioCapability);
      break;
    case e_Receive :
    default:
      cap.SetTag(H245_Capability::e_receiveAudioCapability);
  }
  return OnSendingPDU((H245_AudioCapability &)cap, rxFramesInPacket, e_TCS);
}


PBoolean H323AudioCapability::OnSendingPDU(H245_DataType & dataType) const
{
  dataType.SetTag(H245_DataType::e_audioData);
  return OnSendingPDU((H245_AudioCapability &)dataType, txFramesInPacket, e_OLC);
}


PBoolean H323AudioCapability::OnSendingPDU(H245_ModeElement & mode) const
{
  mode.m_type.SetTag(H245_ModeElementType::e_audioMode);
  return OnSendingPDU((H245_AudioMode &)mode.m_type);
}


PBoolean H323AudioCapability::OnSendingPDU(H245_AudioCapability & pdu,
                                       unsigned packetSize) const
{
  pdu.SetTag(GetSubType());

  // Set the maximum number of frames
  PASN_Integer & value = pdu;
  value = packetSize;
  return TRUE;
}


PBoolean H323AudioCapability::OnSendingPDU(H245_AudioCapability & pdu,
                                       unsigned packetSize,
                                       CommandType) const
{
  return OnSendingPDU(pdu, packetSize);
}


PBoolean H323AudioCapability::OnSendingPDU(H245_AudioMode & pdu) const
{
  static const H245_AudioMode::Choices AudioTable[] = {
    H245_AudioMode::e_nonStandard,
    H245_AudioMode::e_g711Alaw64k,
    H245_AudioMode::e_g711Alaw56k,
    H245_AudioMode::e_g711Ulaw64k,
    H245_AudioMode::e_g711Ulaw56k,
    H245_AudioMode::e_g722_64k,
    H245_AudioMode::e_g722_56k,
    H245_AudioMode::e_g722_48k,
    H245_AudioMode::e_g7231,
    H245_AudioMode::e_g728,
    H245_AudioMode::e_g729,
    H245_AudioMode::e_g729AnnexA,
    H245_AudioMode::e_is11172AudioMode,
    H245_AudioMode::e_is13818AudioMode,
    H245_AudioMode::e_g729wAnnexB,
    H245_AudioMode::e_g729AnnexAwAnnexB,
    H245_AudioMode::e_g7231AnnexCMode,
    H245_AudioMode::e_gsmFullRate,
    H245_AudioMode::e_gsmHalfRate,
    H245_AudioMode::e_gsmEnhancedFullRate,
    H245_AudioMode::e_genericAudioMode,
    H245_AudioMode::e_g729Extensions
  };

  unsigned subType = GetSubType();
  if (subType >= PARRAYSIZE(AudioTable))
    return FALSE;

  pdu.SetTag(AudioTable[subType]);
  return TRUE;
}


PBoolean H323AudioCapability::OnReceivedPDU(const H245_Capability & cap)
{
  H323Capability::OnReceivedPDU(cap);

  if (cap.GetTag() != H245_Capability::e_receiveAudioCapability &&
      cap.GetTag() != H245_Capability::e_receiveAndTransmitAudioCapability)
    return FALSE;

  unsigned packetSize = txFramesInPacket;
  if (!OnReceivedPDU((const H245_AudioCapability &)cap, packetSize, e_TCS))
    return FALSE;

  // Clamp our transmit size to maximum allowed
  if (txFramesInPacket > packetSize) {
    PTRACE(4, "H323\tCapability tx frames reduced from "
           << txFramesInPacket << " to " << packetSize);
    txFramesInPacket = packetSize;
  }
  else {
    PTRACE(4, "H323\tCapability tx frames left at "
           << txFramesInPacket << " as remote allows " << packetSize);
  }

  return TRUE;
}


PBoolean H323AudioCapability::OnReceivedPDU(const H245_DataType & dataType, PBoolean receiver)
{
  if (dataType.GetTag() != H245_DataType::e_audioData)
    return FALSE;

  unsigned & xFramesInPacket = receiver ? rxFramesInPacket : txFramesInPacket;
  unsigned packetSize = xFramesInPacket;
  if (!OnReceivedPDU((const H245_AudioCapability &)dataType, packetSize, e_OLC))
    return FALSE;

  // Clamp our transmit size to that of the remote
  if (xFramesInPacket > packetSize) {
    PTRACE(4, "H323\tCapability " << (receiver ? 'r' : 't') << "x frames reduced from "
           << xFramesInPacket << " to " << packetSize);
    xFramesInPacket = packetSize;
  }
  else if (xFramesInPacket < packetSize) {
    PTRACE(4, "H323\tCapability " << (receiver ? 'r' : 't') << "x frames increased from "
           << xFramesInPacket << " to " << packetSize);
    xFramesInPacket = packetSize;
  }

  return TRUE;
}


PBoolean H323AudioCapability::OnReceivedPDU(const H245_AudioCapability & pdu,
                                        unsigned & packetSize)
{
  if (pdu.GetTag() != GetSubType())
    return FALSE;

  const PASN_Integer & value = pdu;

  // Get the maximum number of frames
  packetSize = value;
  return TRUE;
}


PBoolean H323AudioCapability::OnReceivedPDU(const H245_AudioCapability & pdu,
                                        unsigned & packetSize,
                                        CommandType)
{
  return OnReceivedPDU(pdu, packetSize);
}


/////////////////////////////////////////////////////////////////////////////

H323GenericAudioCapability::H323GenericAudioCapability(
      unsigned max,
      unsigned desired,
      const PString &standardId,
      PINDEX maxBitRate)
  : H323AudioCapability(max, desired),
    H323GenericCapabilityInfo(standardId, maxBitRate)
{
}

PObject::Comparison H323GenericAudioCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323GenericAudioCapability))
    return LessThan;

  return CompareInfo((const H323GenericAudioCapability &)obj);
}


unsigned H323GenericAudioCapability::GetSubType() const
{
  return H245_AudioCapability::e_genericAudioCapability;
}

PString H323GenericAudioCapability::GetIdentifier() const
{
  PASN_ObjectId & oid = *identifier;
  return oid.AsString();
}

PBoolean H323GenericAudioCapability::OnSendingPDU(H245_AudioCapability & pdu, unsigned, CommandType type) const
{
  pdu.SetTag(H245_AudioCapability::e_genericAudioCapability);
  return OnSendingGenericPDU(pdu, GetMediaFormat(), type);
}

PBoolean H323GenericAudioCapability::OnSendingPDU(H245_AudioMode & pdu) const
{
  pdu.SetTag(H245_VideoMode::e_genericVideoMode);
  return OnSendingGenericPDU(pdu, GetMediaFormat(), e_ReqMode);
}

PBoolean H323GenericAudioCapability::OnReceivedPDU(const H245_AudioCapability & pdu, unsigned &, CommandType type)
{
  if( pdu.GetTag() != H245_AudioCapability::e_genericAudioCapability)
    return FALSE;
  return OnReceivedGenericPDU(GetWritableMediaFormat(), pdu, type);
}

PBoolean H323GenericAudioCapability::IsMatch(const PASN_Choice & subTypePDU) const
{
  return H323Capability::IsMatch(subTypePDU) &&
         H323GenericCapabilityInfo::IsMatch((const H245_GenericCapability &)subTypePDU.GetObject());
}



/////////////////////////////////////////////////////////////////////////////

H323NonStandardAudioCapability::H323NonStandardAudioCapability(
      unsigned max,
      unsigned desired,
      H323NonStandardCapabilityInfo::CompareFuncType compareFunc,
      const BYTE * fixedData,
      PINDEX dataSize
    )
  : H323AudioCapability(max, desired),
    H323NonStandardCapabilityInfo(compareFunc, fixedData, dataSize)
{
}

H323NonStandardAudioCapability::H323NonStandardAudioCapability(unsigned max,
                                                               unsigned desired,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323AudioCapability(max, desired),
    H323NonStandardCapabilityInfo(fixedData, dataSize, offset, length)
{
}

H323NonStandardAudioCapability::H323NonStandardAudioCapability(
      unsigned max,
      unsigned desired,
      H323EndPoint &,
      H323NonStandardCapabilityInfo::CompareFuncType compareFunc,
      const BYTE * fixedData,
      PINDEX dataSize)
  : H323AudioCapability(max, desired),
    H323NonStandardCapabilityInfo(compareFunc, fixedData, dataSize)
{
}

H323NonStandardAudioCapability::H323NonStandardAudioCapability(unsigned max,
                                                               unsigned desired,
                                                               H323EndPoint &,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323AudioCapability(max, desired),
    H323NonStandardCapabilityInfo(fixedData, dataSize, offset, length)
{
}

H323NonStandardAudioCapability::H323NonStandardAudioCapability(unsigned max,
                                                               unsigned desired,
                                                               const PString & oid,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323AudioCapability(max, desired),
    H323NonStandardCapabilityInfo(oid, fixedData, dataSize, offset, length)
{
}


H323NonStandardAudioCapability::H323NonStandardAudioCapability(unsigned max,
                                                               unsigned desired,
                                                               BYTE country,
                                                               BYTE extension,
                                                               WORD maufacturer,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323AudioCapability(max, desired),
    H323NonStandardCapabilityInfo(country, extension, maufacturer, fixedData, dataSize, offset, length)
{
}


PObject::Comparison H323NonStandardAudioCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323NonStandardAudioCapability))
    return LessThan;

  return CompareInfo((const H323NonStandardAudioCapability &)obj);
}


unsigned H323NonStandardAudioCapability::GetSubType() const
{
  return H245_AudioCapability::e_nonStandard;
}


PBoolean H323NonStandardAudioCapability::OnSendingPDU(H245_AudioCapability & pdu,
                                                  unsigned) const
{
  return OnSendingNonStandardPDU(pdu, H245_AudioCapability::e_nonStandard);
}


PBoolean H323NonStandardAudioCapability::OnSendingPDU(H245_AudioMode & pdu) const
{
  return OnSendingNonStandardPDU(pdu, H245_AudioMode::e_nonStandard);
}


PBoolean H323NonStandardAudioCapability::OnReceivedPDU(const H245_AudioCapability & pdu,
                                                   unsigned &)
{
  return OnReceivedNonStandardPDU(pdu, H245_AudioCapability::e_nonStandard);
}


PBoolean H323NonStandardAudioCapability::IsMatch(const PASN_Choice & subTypePDU) const
{
  return H323Capability::IsMatch(subTypePDU) &&
         H323NonStandardCapabilityInfo::IsMatch((const H245_NonStandardParameter &)subTypePDU.GetObject());
}

#endif // H323_AUDIO_CODECS


/////////////////////////////////////////////////////////////////////////////

#ifdef H323_VIDEO
int H323VideoCapability::DSCPvalue = PQoS::controlledLoadDSCP;
H323VideoCapability::H323VideoCapability()
{
#if P_QOS
// Set to H263CIF Settings
	rtpqos = new RTP_QOS;
	rtpqos->dataQoS.SetWinServiceType(SERVICETYPE_CONTROLLEDLOAD);
	rtpqos->dataQoS.SetAvgBytesPerSec(16000);
	rtpqos->dataQoS.SetMaxFrameBytes(8192);
	rtpqos->dataQoS.SetDSCP(DSCPvalue);

	rtpqos->ctrlQoS.SetWinServiceType(SERVICETYPE_CONTROLLEDLOAD);
	rtpqos->ctrlQoS.SetDSCP(PQoS::controlledLoadDSCP); 
#endif
}

H323Capability::MainTypes H323VideoCapability::GetMainType() const
{
  return e_Video;
}


PBoolean H323VideoCapability::OnSendingPDU(H245_Capability & cap) const
{
  switch (capabilityDirection) {
    case e_Transmit:
      cap.SetTag(H245_Capability::e_transmitVideoCapability);
      break;
    case e_ReceiveAndTransmit:
      cap.SetTag(H245_Capability::e_receiveAndTransmitVideoCapability);
      break;
    case e_Receive :
    default:
      cap.SetTag(H245_Capability::e_receiveVideoCapability);
  }
  return OnSendingPDU((H245_VideoCapability &)cap, e_TCS);
}


PBoolean H323VideoCapability::OnSendingPDU(H245_DataType & dataType) const
{
  dataType.SetTag(H245_DataType::e_videoData);
  return OnSendingPDU((H245_VideoCapability &)dataType, e_OLC);
}


PBoolean H323VideoCapability::OnSendingPDU(H245_VideoCapability & /*pdu*/) const
{
  return FALSE;
}


PBoolean H323VideoCapability::OnSendingPDU(H245_VideoCapability & pdu, CommandType) const
{
  return OnSendingPDU(pdu);
}


PBoolean H323VideoCapability::OnSendingPDU(H245_ModeElement & mode) const
{
  mode.m_type.SetTag(H245_ModeElementType::e_videoMode);
  return OnSendingPDU((H245_VideoMode &)mode.m_type);
}


PBoolean H323VideoCapability::OnReceivedPDU(const H245_Capability & cap)
{
  H323Capability::OnReceivedPDU(cap);

  if (cap.GetTag() != H245_Capability::e_receiveVideoCapability &&
      cap.GetTag() != H245_Capability::e_receiveAndTransmitVideoCapability)
    return FALSE;

  return OnReceivedPDU((const H245_VideoCapability &)cap, e_TCS);
}


PBoolean H323VideoCapability::OnReceivedPDU(const H245_DataType & dataType, PBoolean)
{
  if (dataType.GetTag() != H245_DataType::e_videoData)
    return FALSE;

  return OnReceivedPDU((const H245_VideoCapability &)dataType, e_OLC);
}


PBoolean H323VideoCapability::OnReceivedPDU(const H245_VideoCapability &)
{
  return FALSE;
}


PBoolean H323VideoCapability::OnReceivedPDU(const H245_VideoCapability & pdu, CommandType)
{
  return OnReceivedPDU(pdu);
}


unsigned H323VideoCapability::GetDefaultSessionID() const
{
  return RTP_Session::DefaultVideoSessionID;
}

void H323VideoCapability::SetDSCPvalue(int newValue) 
{ 
	if (newValue < 64)
		DSCPvalue = newValue;
}

int H323VideoCapability::GetDSCPvalue() 
{ 
	return DSCPvalue; 
}

/////////////////////////////////////////////////////////////////////////////

H323NonStandardVideoCapability::H323NonStandardVideoCapability(const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323NonStandardCapabilityInfo(fixedData, dataSize, offset, length)
{
}

H323NonStandardVideoCapability::H323NonStandardVideoCapability(H323EndPoint &,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323NonStandardCapabilityInfo(fixedData, dataSize, offset, length)
{
}

H323NonStandardVideoCapability::H323NonStandardVideoCapability(const PString & oid,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323NonStandardCapabilityInfo(oid, fixedData, dataSize, offset, length)
{
}


H323NonStandardVideoCapability::H323NonStandardVideoCapability(BYTE country,
                                                               BYTE extension,
                                                               WORD maufacturer,
                                                               const BYTE * fixedData,
                                                               PINDEX dataSize,
                                                               PINDEX offset,
                                                               PINDEX length)
  : H323NonStandardCapabilityInfo(country, extension, maufacturer, fixedData, dataSize, offset, length)
{
}


PObject::Comparison H323NonStandardVideoCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323NonStandardVideoCapability))
    return LessThan;

  return CompareInfo((const H323NonStandardVideoCapability &)obj);
}


unsigned H323NonStandardVideoCapability::GetSubType() const
{
  return H245_VideoCapability::e_nonStandard;
}


PBoolean H323NonStandardVideoCapability::OnSendingPDU(H245_VideoCapability & pdu) const
{
  return OnSendingNonStandardPDU(pdu, H245_VideoCapability::e_nonStandard);
}


PBoolean H323NonStandardVideoCapability::OnSendingPDU(H245_VideoMode & pdu) const
{
  return OnSendingNonStandardPDU(pdu, H245_VideoMode::e_nonStandard);
}


PBoolean H323NonStandardVideoCapability::OnReceivedPDU(const H245_VideoCapability & pdu)
{
  return OnReceivedNonStandardPDU(pdu, H245_VideoCapability::e_nonStandard);
}


PBoolean H323NonStandardVideoCapability::IsMatch(const PASN_Choice & subTypePDU) const
{
  return H323Capability::IsMatch(subTypePDU) &&
         H323NonStandardCapabilityInfo::IsMatch((const H245_NonStandardParameter &)subTypePDU.GetObject());
}

/////////////////////////////////////////////////////////////////////////////

H323GenericVideoCapability::H323GenericVideoCapability(
      const PString &capabilityId,
      PINDEX maxBitRate)
        : H323VideoCapability(),
          H323GenericCapabilityInfo(capabilityId, maxBitRate)
{
}

PObject::Comparison H323GenericVideoCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323GenericVideoCapability))
    return LessThan;

  return CompareInfo((const H323GenericVideoCapability &)obj);
}


unsigned H323GenericVideoCapability::GetSubType() const
{
  return H245_VideoCapability::e_genericVideoCapability;
}

PString H323GenericVideoCapability::GetIdentifier() const
{
  PASN_ObjectId & oid = *identifier;
  return oid.AsString();
}

PBoolean H323GenericVideoCapability::OnSendingPDU(H245_VideoCapability & pdu, CommandType type) const
{
  pdu.SetTag(H245_VideoCapability::e_genericVideoCapability);
  return OnSendingGenericPDU(pdu, GetMediaFormat(), type);
}

PBoolean H323GenericVideoCapability::OnSendingPDU(H245_VideoMode & pdu) const
{
  pdu.SetTag(H245_VideoMode::e_genericVideoMode);
  return OnSendingGenericPDU(pdu, GetMediaFormat(), e_ReqMode);
}

PBoolean H323GenericVideoCapability::OnReceivedPDU(const H245_VideoCapability & pdu, CommandType type)
{
  if (pdu.GetTag() != H245_VideoCapability::e_genericVideoCapability)
	return FALSE;
  return OnReceivedGenericPDU(GetWritableMediaFormat(), pdu, type);
}

PBoolean H323GenericVideoCapability::IsMatch(const PASN_Choice & subTypePDU) const
{
  return H323Capability::IsMatch(subTypePDU) &&
         H323GenericCapabilityInfo::IsMatch((const H245_GenericCapability &)subTypePDU.GetObject());
}

/////////////////////////////////////////////////////////////////////////////

#ifdef H323_H239

H323ExtendedVideoCapability::H323ExtendedVideoCapability(
      const PString &capabilityId )
        : H323Capability(),
          H323GenericCapabilityInfo(capabilityId, 0)
{
	table.SetSize(0);
	SetCapabilityDirection(H323Capability::e_NoDirection);
}

PBoolean H323ExtendedVideoCapability::OnSendingPDU(H245_Capability & cap) const
{
	  cap.SetTag(H245_Capability::e_genericControlCapability);
	  return OnSendingPDU((H245_GenericCapability &)cap, e_TCS);
}

PBoolean H323ExtendedVideoCapability::OnReceivedPDU(const H245_Capability & cap)
{
  H323Capability::OnReceivedPDU(cap);

  if( cap.GetTag()!= H245_Capability::e_genericControlCapability)
    return FALSE;

  return OnReceivedPDU((const H245_GenericCapability &)cap, e_TCS);
} 

PBoolean H323ExtendedVideoCapability::OnReceivedPDU(const H245_GenericCapability & pdu, CommandType type)
{
  OpalMediaFormat mediaFormat = GetMediaFormat();
  return OnReceivedGenericPDU(mediaFormat, pdu, type);
}

PBoolean H323ExtendedVideoCapability::OnSendingPDU(H245_GenericCapability & pdu, CommandType type) const
{
  return OnSendingGenericPDU(pdu, GetMediaFormat(), type);
}

PObject::Comparison H323ExtendedVideoCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323ExtendedVideoCapability))
    return LessThan;

  return CompareInfo((const H323ExtendedVideoCapability &)obj);
}

H323Capability::MainTypes H323ExtendedVideoCapability::GetMainType() const
{
	return H323Capability::e_GenericControl;
}

unsigned H323ExtendedVideoCapability::GetSubType() const
{
	return 0; // Not used
}

unsigned H323ExtendedVideoCapability::GetDefaultSessionID() const
{
	return OpalMediaFormat::DefaultExtVideoSessionID;
}

PBoolean H323ExtendedVideoCapability::OnSendingPDU(H245_DataType & /*pdu*/) const
{
	 return FALSE;
}

PBoolean H323ExtendedVideoCapability::OnSendingPDU(H245_ModeElement & pdu) const
{
   if (table.GetSize() > 0)
	 return table[0].OnSendingPDU(pdu);
   else
	 return FALSE;
}

PBoolean H323ExtendedVideoCapability::OnReceivedPDU(const H245_DataType & /*pdu*/, PBoolean /*receiver*/)
{
	 return FALSE;
}

PBoolean H323ExtendedVideoCapability::IsMatch(const PASN_Choice & subTypePDU) const
{
  return H323Capability::IsMatch(subTypePDU) &&
         H323GenericCapabilityInfo::IsMatch((const H245_GenericCapability &)subTypePDU.GetObject());
}

void H323ExtendedVideoCapability::AddAllCapabilities(
      H323Capabilities & basecapabilities, PINDEX descriptorNum,PINDEX simultaneous)
{
  H323ExtendedVideoFactory::KeyList_T extCaps = H323ExtendedVideoFactory::GetKeyList();
  if (extCaps.size() > 0) {
    H323CodecExtendedVideoCapability * capability = new H323CodecExtendedVideoCapability();
    H323ExtendedVideoFactory::KeyList_T::const_iterator r;
        PINDEX num = P_MAX_INDEX;
		for (r = extCaps.begin(); r != extCaps.end(); ++r) {
           H323CodecExtendedVideoCapability * extCapability = (H323CodecExtendedVideoCapability *)capability->Clone();
		   extCapability->AddCapability(*r);
           num = basecapabilities.SetCapability(descriptorNum, simultaneous,extCapability);
           simultaneous = num;
		}
    simultaneous = P_MAX_INDEX;
	basecapabilities.SetCapability(descriptorNum, simultaneous,new H323ControlExtendedVideoCapability());
    delete capability;
  } 
}

H323Capability & H323ExtendedVideoCapability::operator[](PINDEX i) 
{
  return table[i];
}

H323Capability * H323ExtendedVideoCapability::GetAt(PINDEX i) const
{
    if (extCapabilities.GetSize() > 0) 
      return &extCapabilities[i];
	
	if (table.GetSize() > 0) 
      return &table[i];

    return NULL;
}


PINDEX H323ExtendedVideoCapability::GetSize() const
{ 
    if (extCapabilities.GetSize() > 0) 
         return extCapabilities.GetSize();

    if (table.GetSize() > 0) 
         return table.GetSize(); 

    return 0;
}


H323Channel * H323ExtendedVideoCapability::CreateChannel(
					  H323Connection & /*connection*/,    
					  H323Channel::Directions /*dir*/,    
					  unsigned /*sessionID*/,             
					  const H245_H2250LogicalChannelParameters * /*param*/
					) const
{
	return NULL;
}

H323Codec * H323ExtendedVideoCapability::CreateCodec(
							H323Codec::Direction /*direction*/
						) const
{
	return NULL;
}

const H323Capabilities & H323ExtendedVideoCapability::GetCapabilities() const
{
	return extCapabilities;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// H.239 Control

// These H.245 functions need to be spun off into a common utilities class

H245_GenericParameter & buildGenericInteger(H245_GenericParameter & param, unsigned id, unsigned val)
{
	 H245_ParameterIdentifier & idm = param.m_parameterIdentifier; 
	     idm.SetTag(H245_ParameterIdentifier::e_standard);
		 PASN_Integer & idx = idm;
		 idx = id;
		 H245_ParameterValue & genvalue = param.m_parameterValue;
		 genvalue.SetTag(H245_ParameterValue::e_unsignedMin);
		 PASN_Integer & xval = genvalue;
		 xval = val;
	return param;
}

H245_GenericParameter & buildGenericLogical(H245_GenericParameter & param, unsigned id)
{
	H245_ParameterIdentifier & paramId = param.m_parameterIdentifier;
	paramId.SetTag(H245_ParameterIdentifier::e_standard);
	PASN_Integer & idx = paramId;
	idx = id;

	H245_ParameterValue & val = param.m_parameterValue;
	val.SetTag(H245_ParameterValue::e_logical);

	return param;
}

////////////////////////////////////////////////////////////////////

void BuildH239GenericMessageIndication(H239Control & ctrl, H323Connection & connection, H323ControlPDU& pdu, H239Control::H239SubMessages submesId)
{       
        PTRACE(4,"H239\tSending Generic Message Indication.");
        
	H245_GenericMessage & cap = pdu.Build(H245_IndicationMessage::e_genericIndication);

	H245_CapabilityIdentifier & id = cap.m_messageIdentifier;
	id.SetTag(H245_CapabilityIdentifier::e_standard);
	PASN_ObjectId & gid = id;
        gid.SetValue(OpalPluginCodec_Identifer_H239_GenericMessage);

	cap.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
	PASN_Integer & subMesID = cap.m_subMessageIdentifier;
	subMesID = submesId;

	cap.IncludeOptionalField(H245_GenericMessage::e_messageContent);
	H245_ArrayOf_GenericParameter & msg = cap.m_messageContent;
	msg.SetSize(2);
	buildGenericInteger(msg[0], H239Control::h239gpTerminalLabel, 0);
	buildGenericInteger(msg[1], H239Control::h239gpChannelId, connection.GetLogicalChannels()->GetLastChannelNumber());
}

void BuildH239GenericMessageResponse(H239Control & ctrl, H323Connection & connection, 
                                     H323ControlPDU& pdu, H239Control::H239SubMessages submesId,
                                     PBoolean approved)
{
	H245_GenericMessage & cap = pdu.Build(H245_ResponseMessage::e_genericResponse);

	H245_CapabilityIdentifier & id = cap.m_messageIdentifier;
	id.SetTag(H245_CapabilityIdentifier::e_standard);
	PASN_ObjectId & gid = id;
	gid.SetValue(OpalPluginCodec_Identifer_H239_GenericMessage);

	cap.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
	PASN_Integer & subMesID = cap.m_subMessageIdentifier;
	subMesID = submesId;

	cap.IncludeOptionalField(H245_GenericMessage::e_messageContent);
	H245_ArrayOf_GenericParameter & msg = cap.m_messageContent;

   if (!approved) {
    msg.SetSize(1);
    buildGenericLogical(msg[0], H239Control::h239gpReject);
   } else {
    msg.SetSize(3);
	buildGenericLogical(msg[0], H239Control::h239gpAcknowledge);
	buildGenericInteger(msg[1], H239Control::h239gpTerminalLabel, 0);
	buildGenericInteger(msg[2], H239Control::h239gpChannelId, ctrl.GetChannelNum(H323Capability::e_Receive));
   }
}

void BuildH239GenericMessageRequest(H239Control & ctrl, H323Connection & connection, H323ControlPDU& pdu, H239Control::H239SubMessages submesId)
{
	H245_GenericMessage & cap = pdu.Build(H245_RequestMessage::e_genericRequest);

	H245_CapabilityIdentifier & id = cap.m_messageIdentifier;
	id.SetTag(H245_CapabilityIdentifier::e_standard);
	PASN_ObjectId & gid = id;
	gid.SetValue(OpalPluginCodec_Identifer_H239_GenericMessage);

	cap.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
	PASN_Integer & subMesID = cap.m_subMessageIdentifier;
	subMesID = submesId;

	cap.IncludeOptionalField(H245_GenericMessage::e_messageContent);
        H245_ArrayOf_GenericParameter & msg = cap.m_messageContent;
	msg.SetSize(3);
	buildGenericInteger(msg[0], H239Control::h239gpTerminalLabel, 0);
    int channelID = connection.GetLogicalChannels()->GetLastChannelNumber()+1;
    ctrl.SetRequestedChanNum(channelID);
	buildGenericInteger(msg[1], H239Control::h239gpChannelId,channelID); 
	buildGenericInteger(msg[2], H239Control::h239gpSymmetryBreaking, 4);
}

void BuildH239GenericMessageCommand(H239Control & ctrl, H323Connection & connection, H323ControlPDU& pdu, H239Control::H239SubMessages submesId, PBoolean option)
{
    H245_GenericMessage & cap = pdu.Build(H245_CommandMessage::e_genericCommand);

    H245_CapabilityIdentifier & id = cap.m_messageIdentifier;
    id.SetTag(H245_CapabilityIdentifier::e_standard);
    PASN_ObjectId & gid = id;
	gid.SetValue(OpalPluginCodec_Identifer_H239_GenericMessage);

    cap.IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);
    PASN_Integer & num = cap.m_subMessageIdentifier;
    num = submesId;

    cap.IncludeOptionalField(H245_GenericMessage::e_messageContent);
    H245_ArrayOf_GenericParameter & msg = cap.m_messageContent;
    msg.SetSize(2);
	buildGenericInteger(msg[0], H239Control::h239gpTerminalLabel, 0);
    buildGenericInteger(msg[1], H239Control::h239gpChannelId, ctrl.GetChannelNum(option ? H323Capability::e_Transmit : H323Capability::e_Receive));
}


bool OnH239GenericMessageRequest(H239Control & ctrl, H323Connection & connection, const H245_ArrayOf_GenericParameter & content)
 {
	PTRACE(4,"H239\tReceived Generic Request.");
	for (int i = 0; i < content.GetSize(); ++i)
	{
		H245_GenericParameter& param = content[i];
		switch ((PASN_Integer)param.m_parameterIdentifier) {
		  case H239Control::h239gpBitRate:
			break;
		  case H239Control::h239gpChannelId:					
            ctrl.SetChannelNum((PASN_Integer)param.m_parameterValue, H323Capability::e_Receive);
			break;
		  case H239Control::h239gpSymmetryBreaking:
		  case H239Control::h239gpTerminalLabel:
		  default:
			break;
		}
	}

    // We send back to the connection to allow the implementor to delay the reply message till it is ready.-SH
    return connection.OnH239ControlRequest(&ctrl);
}

PBoolean OnH239GenericMessageResponse(H239Control & ctrl, H323Connection & connection, const H245_ArrayOf_GenericParameter & content)
{
	PTRACE(4,"H239\tReceived Generic Response.");

    bool m_allowOutgoingExtVideo=true;
    unsigned channelID=0;
    int defaultSession=0;
	for (int i = 0; i < content.GetSize(); ++i)
	{
		H245_GenericParameter& param = content[i];
		switch ((PASN_Integer)param.m_parameterIdentifier)
		{
		case H239Control::h239gpChannelId:
            channelID = (PASN_Integer)param.m_parameterValue;
            if (channelID == ctrl.GetChannelNum(H323Capability::e_Receive)) {
               PTRACE(4,"H239\tRec'd Response for Receive side. Close Receive Channel!");
               ctrl.SendGenericMessage(H239Control::e_h245command, &connection, false);
               defaultSession = ctrl.GetRequestedChanNum();
            } 
			break;
		case H239Control::h239gpAcknowledge:
			break;
		case H239Control::h239gpReject:
            connection.OpenExtendedVideoSessionDenied();
            m_allowOutgoingExtVideo = false;
			break;
		case H239Control::h239gpBitRate:
		case H239Control::h239gpSymmetryBreaking:
		case H239Control::h239gpTerminalLabel:
            break;
		default:
            m_allowOutgoingExtVideo = false;
			break;
		}
	}

    if (channelID > 0 && channelID == ctrl.GetChannelNum(H323Capability::e_Transmit)) {
       PTRACE(4,"H239\tLate Acknowledge IGNORE");
       m_allowOutgoingExtVideo = false;
    }

    if (m_allowOutgoingExtVideo)
        return connection.OpenExtendedVideoSession(ctrl.GetChannelNum(H323Capability::e_Transmit), defaultSession);

    return true;
}


PBoolean OnH239GenericMessageCommand(H239Control & ctrl, H323Connection & connection, const H245_ArrayOf_GenericParameter & content)
{
	PTRACE(4,"H239\tReceived Generic Command.");

    return connection.OnH239ControlCommand(&ctrl);   
}

///////////////////////////////////////////

H323ControlExtendedVideoCapability::H323ControlExtendedVideoCapability()
  : H323ExtendedVideoCapability(OpalPluginCodec_Identifer_H239)
  , m_outgoingChanNum(0, false), m_incomingChanNum(0,false), m_requestedChanNum(0)
{ 
}

PBoolean H323ControlExtendedVideoCapability::CloseChannel(H323Connection * connection, H323Capability::CapabilityDirection dir)
{
   SendGenericMessage(H239Control::e_h245command, connection, dir == H323Capability::e_Transmit);
   return connection->CloseExtendedVideoSession(GetChannelNum(dir));
}

PBoolean H323ControlExtendedVideoCapability::SendGenericMessage(h245MessageType msgtype, H323Connection * connection, PBoolean option)
{
   H323ControlPDU pdu;
     switch (msgtype) {
	    case e_h245request:
		    BuildH239GenericMessageRequest(*this,*connection,pdu,H239Control::e_presentationTokenRequest);
            break;
	    case e_h245response:
		    BuildH239GenericMessageResponse(*this,*connection,pdu,H239Control::e_presentationTokenResponse,option);
            break;
	    case e_h245command:
            BuildH239GenericMessageCommand(*this, *connection, pdu, H239Control::e_presentationTokenRelease,option);
            break;
	    case e_h245indication:
        default:
            return true;
     }
   return connection->WriteControlPDU(pdu);
}

PBoolean H323ControlExtendedVideoCapability::HandleGenericMessage(h245MessageType type,
                                              H323Connection * con,
                                              const H245_ArrayOf_GenericParameter * pdu)
{
	 switch (type) {
		case e_h245request:
			return OnH239GenericMessageRequest(*this,*con,*pdu);
		case e_h245response:
			return OnH239GenericMessageResponse(*this,*con,*pdu);
		case e_h245command:  
            return OnH239GenericMessageCommand(*this,*con,*pdu);
		case e_h245indication:
        default:
            return true;
	 }
}

H323ChannelNumber & H323ControlExtendedVideoCapability::GetChannelNum(H323Capability::CapabilityDirection dir) 
{ 
    switch (dir) {   
      case e_Transmit:
          return m_outgoingChanNum;
      case e_Receive:
      default:
          return m_incomingChanNum;
    }
}

void H323ControlExtendedVideoCapability::SetChannelNum(unsigned num, H323Capability::CapabilityDirection dir) 
{ 
    switch (dir) {
      case e_Transmit:
          m_outgoingChanNum = H323ChannelNumber(num, false);
          break;
      case e_Receive:
      default:
          m_incomingChanNum = H323ChannelNumber(num, true);
          break;
    } 
}

void H323ControlExtendedVideoCapability::SetRequestedChanNum(int num) 
{
    m_requestedChanNum = num; 
}

int H323ControlExtendedVideoCapability::GetRequestedChanNum() 
{ 
    return m_requestedChanNum; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////

H323CodecExtendedVideoCapability::H323CodecExtendedVideoCapability()
   : H323ExtendedVideoCapability(OpalPluginCodec_Identifer_H239_Video)
{
	
   SetCapabilityDirection(H323Capability::e_Transmit);
}

H323CodecExtendedVideoCapability::~H323CodecExtendedVideoCapability()
{
}

void H323CodecExtendedVideoCapability::AddCapability(const PString & cap)
{
	extCapabilities.Add(H323ExtendedVideoFactory::CreateInstance(cap));
}

PString H323CodecExtendedVideoCapability::GetFormatName() const
{ 
  PStringStream strm;
  strm << "H.239";
  if (extCapabilities.GetSize() > 0) {
    for (PINDEX i=0; i< extCapabilities.GetSize(); i++) {
        strm << '(' << extCapabilities[i] << ')';
	}
  }
  return strm;
}

H323Capability::MainTypes H323CodecExtendedVideoCapability::GetMainType() const
{  
	return H323Capability::e_Video; 
}

unsigned H323CodecExtendedVideoCapability::GetSubType() const
{  
	return H245_VideoCapability::e_extendedVideoCapability;  
}

PObject * H323CodecExtendedVideoCapability::Clone() const
{ 
    return new H323CodecExtendedVideoCapability(*this); 
}

H323Channel * H323CodecExtendedVideoCapability::CreateChannel(H323Connection & connection,   
      H323Channel::Directions dir,unsigned sessionID,const H245_H2250LogicalChannelParameters * param
) const
{
   if (table.GetSize() == 0)
 	   return NULL;
   
   return connection.CreateRealTimeLogicalChannel(*this, dir, sessionID, param);
}

H323Codec * H323CodecExtendedVideoCapability::CreateCodec(H323Codec::Direction direction ) const
{ 
   if (extCapabilities.GetSize() == 0)
	   return NULL;

	return extCapabilities[0].CreateCodec(direction); 
}

PBoolean H323CodecExtendedVideoCapability::OnSendingPDU(H245_Capability & cap) const
{
   cap.SetTag(H245_Capability::e_receiveVideoCapability);
   return OnSendingPDU((H245_VideoCapability &)cap, e_TCS);
}

PBoolean H323CodecExtendedVideoCapability::OnReceivedPDU(const H245_Capability & cap)
{
  H323Capability::OnReceivedPDU(cap);

  if (extCapabilities.GetSize() == 0)
		return FALSE;

  const H245_VideoCapability & vidcap = (const H245_VideoCapability &)cap;
  if (vidcap.GetTag() != H245_VideoCapability::e_extendedVideoCapability)
	  return FALSE;

  return OnReceivedPDU(vidcap);
} 


PBoolean H323CodecExtendedVideoCapability::OnSendingPDU(H245_DataType & pdu) const
{
	if (table.GetSize() > 0) {
	  pdu.SetTag(H245_DataType::e_videoData);
	  return OnSendingPDU((H245_VideoCapability &)pdu, e_OLC);
	} else
	 return FALSE;
}

PBoolean H323CodecExtendedVideoCapability::OnReceivedPDU(const H245_DataType & pdu, PBoolean /*receiver*/)
{
   if (pdu.GetTag() == H245_DataType::e_videoData)
	 return OnReceivedPDU((const H245_VideoCapability &)pdu);
   else
	 return FALSE;
}

PBoolean H323CodecExtendedVideoCapability::OnSendingPDU(H245_VideoCapability & pdu, CommandType ctype) const 
{ 
	if (extCapabilities.GetSize() == 0)
		return FALSE;

	pdu.SetTag(H245_VideoCapability::e_extendedVideoCapability);
	H245_ExtendedVideoCapability & extend = (H245_ExtendedVideoCapability &)pdu;
	extend.IncludeOptionalField(H245_ExtendedVideoCapability::e_videoCapabilityExtension);
	H245_ArrayOf_GenericCapability & cape = extend.m_videoCapabilityExtension;

	H245_GenericCapability gcap;
	 gcap.m_capabilityIdentifier = *(new H245_CapabilityIdentifier(H245_CapabilityIdentifier::e_standard));
	 PASN_ObjectId &object_id = gcap.m_capabilityIdentifier;
     object_id = OpalPluginCodec_Identifer_H239_Video;

	 // Add role
      H245_GenericParameter * param = new H245_GenericParameter;
      param->m_parameterIdentifier.SetTag(H245_ParameterIdentifier::e_standard);
      (PASN_Integer &)param->m_parameterIdentifier = 1;
	  param->m_parameterValue.SetTag(H245_ParameterValue::e_booleanArray);
      (PASN_Integer &)param->m_parameterValue = 1;  // Live presentation

      gcap.IncludeOptionalField(H245_GenericCapability::e_collapsing);
	  gcap.m_collapsing.SetSize(1);
      gcap.m_collapsing[0] = *param;
      cape.SetSize(1);
      cape[0] = gcap;


    H245_ArrayOf_VideoCapability & caps = extend.m_videoCapability;
    
	if (extCapabilities.GetSize() > 0) {
	  caps.SetSize(extCapabilities.GetSize());
      for (PINDEX i=0; i< extCapabilities.GetSize(); i++) {
	     H245_VideoCapability vidcap;
         ((H323VideoCapability &)extCapabilities[i]).OnSendingPDU(vidcap,ctype);
         caps[i] = vidcap;
      }
	} else {
	  caps.SetSize(table.GetSize());
      for (PINDEX i=0; i< table.GetSize(); i++) {
	     H245_VideoCapability vidcap;
        ((H323VideoCapability &)table[i]).OnSendingPDU(vidcap,ctype);
        caps[i] = vidcap;
	  }
	}

  return TRUE;
}

PBoolean H323CodecExtendedVideoCapability::OnReceivedPDU(const H245_VideoCapability & pdu )
{ 
   if (pdu.GetTag() != H245_VideoCapability::e_extendedVideoCapability)
		return FALSE;

   const H245_ExtendedVideoCapability & extend = (const H245_ExtendedVideoCapability &)pdu;

   if (!extend.HasOptionalField(H245_ExtendedVideoCapability::e_videoCapabilityExtension))
		return FALSE;

   // Role Information
   const H245_ArrayOf_GenericCapability & cape = extend.m_videoCapabilityExtension;
   if (cape.GetSize() == 0) {
	   PTRACE(2,"H239\tERROR: Missing Capability Extension!");
	   return FALSE;
   }

   for (PINDEX b =0; b < cape.GetSize(); b++) {
    const H245_GenericCapability & cap = cape[b];
	if (cap.m_capabilityIdentifier.GetTag() != H245_CapabilityIdentifier::e_standard) {
		PTRACE(4,"H239\tERROR: Wrong Capability type!");
		return FALSE;
	}
	
	const PASN_ObjectId & id = cap.m_capabilityIdentifier;
	if (id != OpalPluginCodec_Identifer_H239_Video) {
		PTRACE(4,"H239\tERROR: Wrong Capability Identifer " << id);
		return FALSE;
	}

	if (!cap.HasOptionalField(H245_GenericCapability::e_collapsing)) {
		PTRACE(4,"H239\tERROR: No collapsing field");
		return FALSE;
	}

	for (PINDEX c =0; c < cap.m_collapsing.GetSize(); c++) {
		const H245_GenericParameter & param = cap.m_collapsing[c];
		const PASN_Integer & id = param.m_parameterIdentifier;
		if (id.GetValue() != 1) {
	        PTRACE(4,"H239\tERROR: Unknown Role Identifer");
			return FALSE;
		}
		const PASN_Integer & role = param.m_parameterValue;
		if (role.GetValue() != 1) {
	        PTRACE(4,"H239\tERROR: Unsupported Role mode " << param.m_parameterValue );
			return FALSE;
		}
	}
  }

   if (table.GetSize() == 0) {
	  // Get a Common Video Capability list
	  const H245_ArrayOf_VideoCapability & caps = extend.m_videoCapability;
	  H323Capabilities allCapabilities;
	  for (PINDEX c = 0; c < extCapabilities.GetSize(); c++)
		allCapabilities.Add(allCapabilities.Copy(extCapabilities[c]));

	  // Decode out of the PDU, the list of known codecs.
		for (PINDEX i = 0; i < caps.GetSize(); i++) {
			H323Capability * capability = allCapabilities.FindCapability(H323Capability::e_Video, caps[i], NULL);
			if (capability != NULL) {
			  H323VideoCapability * copy = (H323VideoCapability *)capability->Clone();
			  if (copy->OnReceivedPDU(caps[i],e_TCS))
				table.Append(copy);
			  else
				delete copy;
			}
		}
   }
   return TRUE; 
}

PObject::Comparison H323CodecExtendedVideoCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323CodecExtendedVideoCapability))
    return LessThan;

  const H323CodecExtendedVideoCapability & cap = (const H323CodecExtendedVideoCapability &)obj;

  for (PINDEX i= 0; i< GetSize(); ++i) {
      for (PINDEX j=0; j< GetSize(); ++j) {
         H323Capability * local = GetAt(i);
         H323Capability * remote = cap.GetAt(j);

         if (!local || !remote) continue;

         PObject::Comparison equal = local->Compare(*remote);
         if (equal == EqualTo) 
             return EqualTo;
      }
  }
  return LessThan;
}

PBoolean H323CodecExtendedVideoCapability::IsMatch(const PASN_Choice & subTypePDU) const
{
   if (subTypePDU.GetTag() != GetSubType())
        return false;

   const H245_ExtendedVideoCapability & gen = (const H245_ExtendedVideoCapability &)subTypePDU.GetObject();
   const H245_VideoCapability & vid = (const H245_VideoCapability &)gen.m_videoCapability[0];
   return extCapabilities[0].IsMatch(vid);

}

PBoolean H323CodecExtendedVideoCapability::OnReceivedGenericPDU(const H245_GenericCapability & /*pdu*/)
{
	return TRUE;
}

const OpalMediaFormat & H323CodecExtendedVideoCapability::GetMediaFormat() const
{ 
  if (table.GetSize() > 0)
    return ((H323VideoCapability &)table[0]).GetMediaFormat();
  else if (extCapabilities.GetSize() > 0)
    return ((H323VideoCapability &)extCapabilities[0]).GetMediaFormat();
  else 
	return H323Capability::GetMediaFormat();
}

OpalMediaFormat & H323CodecExtendedVideoCapability::GetWritableMediaFormat()
{
  if (table.GetSize() > 0)
    return ((H323VideoCapability &)table[0]).GetWritableMediaFormat();  
  else if (extCapabilities.GetSize() > 0)
    return ((H323VideoCapability &)extCapabilities[0]).GetWritableMediaFormat();
  else
    return H323Capability::GetWritableMediaFormat();
}

#endif // H323_H239
#endif // NO_H323_VIDEO

/////////////////////////////////////////////////////////////////////////////

#ifdef H323_H230
H323_ConferenceControlCapability::H323_ConferenceControlCapability()
{
   chairControlCapability = FALSE;
   nonStandardExtension = FALSE;
}

H323_ConferenceControlCapability::H323_ConferenceControlCapability(PBoolean chairControls, PBoolean T124Extension)
{
   chairControlCapability = chairControls;
   nonStandardExtension = T124Extension;
}

PObject * H323_ConferenceControlCapability::Clone() const
{
  return new H323_ConferenceControlCapability(*this);
}

H323Capability::MainTypes H323_ConferenceControlCapability::GetMainType() const
{
  return e_ConferenceControl;
}

unsigned H323_ConferenceControlCapability::GetSubType()  const
{
    return 0;
}

PString H323_ConferenceControlCapability::GetFormatName() const
{
  return "H.230 Conference Controls";
}

H323Channel * H323_ConferenceControlCapability::CreateChannel(H323Connection &,
                                                      H323Channel::Directions,
                                                      unsigned,
                                                      const H245_H2250LogicalChannelParameters *) const
{
  PTRACE(1, "Codec\tCannot create ConferenceControlCapability channel");
  return NULL;
}

H323Codec * H323_ConferenceControlCapability::CreateCodec(H323Codec::Direction) const
{
  PTRACE(1, "Codec\tCannot create ConferenceControlCapability codec");
  return NULL;
}

static const char * ExtConferenceControlOID = "0.0.20.124.2";  // Tunnel T.124
PBoolean H323_ConferenceControlCapability::OnSendingPDU(H245_Capability & pdu) const
{
   
  pdu.SetTag(H245_Capability::e_conferenceCapability);
  H245_ConferenceCapability & conf = pdu;
  // Supports Chair control
  conf.m_chairControlCapability = chairControlCapability;

  // Include Extended Custom Controls such as INVITE/EJECT etc.
  if (nonStandardExtension) {
	conf.IncludeOptionalField(H245_ConferenceCapability::e_nonStandardData);
	H245_ArrayOf_NonStandardParameter & nsParam = conf.m_nonStandardData;

	H245_NonStandardParameter param;
	H245_NonStandardIdentifier & id = param.m_nonStandardIdentifier;
	id.SetTag(H245_NonStandardIdentifier::e_object);
	PASN_ObjectId & oid = id;
	oid.SetValue(ExtConferenceControlOID);
	PASN_OctetString & data = param.m_data;
	data.SetValue("");

	nsParam.SetSize(1);
	nsParam[0] = param;
  }

  return TRUE;
}

PBoolean H323_ConferenceControlCapability::OnSendingPDU(H245_DataType &) const
{
  PTRACE(1, "Codec\tCannot have ConferenceControlCapability in DataType");
  return FALSE;
}

PBoolean H323_ConferenceControlCapability::OnSendingPDU(H245_ModeElement &) const
{
  PTRACE(1, "Codec\tCannot have ConferenceControlCapability in ModeElement");
  return FALSE;
}

PBoolean H323_ConferenceControlCapability::OnReceivedPDU(const H245_Capability & pdu)
{

  H323Capability::OnReceivedPDU(pdu);

  if (pdu.GetTag() != H245_Capability::e_conferenceCapability)
    return FALSE;

  const H245_ConferenceCapability & conf = pdu;
  // Supports Chair control
  chairControlCapability = conf.m_chairControlCapability;

  // Include Extended Custom Control support.
  if (conf.HasOptionalField(H245_ConferenceCapability::e_nonStandardData)) {
	const H245_ArrayOf_NonStandardParameter & nsParam = conf.m_nonStandardData;

	for (PINDEX i=0; i < nsParam.GetSize(); i++) {
		const H245_NonStandardParameter & param = nsParam[i];
	    const H245_NonStandardIdentifier & id = param.m_nonStandardIdentifier;
		if (id.GetTag() == H245_NonStandardIdentifier::e_object) {
             const PASN_ObjectId & oid = id;
			 if (oid.AsString() == ExtConferenceControlOID)
				    nonStandardExtension = TRUE;
		}
	}
  }
  return TRUE;
}

PBoolean H323_ConferenceControlCapability::OnReceivedPDU(const H245_DataType &, PBoolean)
{
  PTRACE(1, "Codec\tCannot have ConferenceControlCapability in DataType");
  return FALSE;
}

#endif  // H323_H230

/////////////////////////////////////////////////////////////////////////////

H323DataCapability::H323DataCapability(unsigned rate)
  : maxBitRate(rate)
{
}


H323Capability::MainTypes H323DataCapability::GetMainType() const
{
  return e_Data;
}


unsigned H323DataCapability::GetDefaultSessionID() const
{
  return 3;
}


H323Codec * H323DataCapability::CreateCodec(H323Codec::Direction) const
{
  return NULL;
}


PBoolean H323DataCapability::OnSendingPDU(H245_Capability & cap) const
{
  switch (capabilityDirection) {
    case e_Transmit:
      cap.SetTag(H245_Capability::e_transmitDataApplicationCapability);
      break;
    case e_Receive :
      cap.SetTag(H245_Capability::e_receiveDataApplicationCapability);
      break;
    case e_ReceiveAndTransmit:
    default:
      cap.SetTag(H245_Capability::e_receiveAndTransmitDataApplicationCapability);
  }
  H245_DataApplicationCapability & app = cap;
  app.m_maxBitRate = maxBitRate;
  return OnSendingPDU(app, e_TCS);
}


PBoolean H323DataCapability::OnSendingPDU(H245_DataType & dataType) const
{
  dataType.SetTag(H245_DataType::e_data);
  H245_DataApplicationCapability & app = dataType;
  app.m_maxBitRate = maxBitRate;
  return OnSendingPDU(app, e_OLC);
}


PBoolean H323DataCapability::OnSendingPDU(H245_ModeElement & mode) const
{
  mode.m_type.SetTag(H245_ModeElementType::e_dataMode);
  H245_DataMode & type = mode.m_type;
  type.m_bitRate = maxBitRate;
  return OnSendingPDU(type);
}


PBoolean H323DataCapability::OnSendingPDU(H245_DataApplicationCapability &) const
{
  return FALSE;
}


PBoolean H323DataCapability::OnSendingPDU(H245_DataApplicationCapability & pdu, CommandType) const
{
  return OnSendingPDU(pdu);
}


PBoolean H323DataCapability::OnReceivedPDU(const H245_Capability & cap)
{
  H323Capability::OnReceivedPDU(cap);

  if (cap.GetTag() != H245_Capability::e_receiveDataApplicationCapability &&
      cap.GetTag() != H245_Capability::e_receiveAndTransmitDataApplicationCapability)
    return FALSE;

  const H245_DataApplicationCapability & app = cap;
  maxBitRate = app.m_maxBitRate;
  return OnReceivedPDU(app, e_TCS);
}


PBoolean H323DataCapability::OnReceivedPDU(const H245_DataType & dataType, PBoolean)
{
  if (dataType.GetTag() != H245_DataType::e_data)
    return FALSE;

  const H245_DataApplicationCapability & app = dataType;
  maxBitRate = app.m_maxBitRate;
  return OnReceivedPDU(app, e_OLC);
}


PBoolean H323DataCapability::OnReceivedPDU(const H245_DataApplicationCapability &)
{
  return FALSE;
}


PBoolean H323DataCapability::OnReceivedPDU(const H245_DataApplicationCapability & pdu, CommandType)
{
  return OnReceivedPDU(pdu);
}


/////////////////////////////////////////////////////////////////////////////

H323NonStandardDataCapability::H323NonStandardDataCapability(unsigned maxBitRate,
                                                             const BYTE * fixedData,
                                                             PINDEX dataSize,
                                                             PINDEX offset,
                                                             PINDEX length)
  : H323DataCapability(maxBitRate),
    H323NonStandardCapabilityInfo(fixedData, dataSize, offset, length)
{
}


H323NonStandardDataCapability::H323NonStandardDataCapability(unsigned maxBitRate,
                                                             const PString & oid,
                                                             const BYTE * fixedData,
                                                             PINDEX dataSize,
                                                             PINDEX offset,
                                                             PINDEX length)
  : H323DataCapability(maxBitRate),
    H323NonStandardCapabilityInfo(oid, fixedData, dataSize, offset, length)
{
}


H323NonStandardDataCapability::H323NonStandardDataCapability(unsigned maxBitRate,
                                                             BYTE country,
                                                             BYTE extension,
                                                             WORD maufacturer,
                                                             const BYTE * fixedData,
                                                             PINDEX dataSize,
                                                             PINDEX offset,
                                                             PINDEX length)
  : H323DataCapability(maxBitRate),
    H323NonStandardCapabilityInfo(country, extension, maufacturer, fixedData, dataSize, offset, length)
{
}


PObject::Comparison H323NonStandardDataCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323NonStandardDataCapability))
    return LessThan;

  return CompareInfo((const H323NonStandardDataCapability &)obj);
}


unsigned H323NonStandardDataCapability::GetSubType() const
{
  return H245_DataApplicationCapability_application::e_nonStandard;
}


PBoolean H323NonStandardDataCapability::OnSendingPDU(H245_DataApplicationCapability & pdu) const
{
  return OnSendingNonStandardPDU(pdu.m_application, H245_DataApplicationCapability_application::e_nonStandard);
}


PBoolean H323NonStandardDataCapability::OnSendingPDU(H245_DataMode & pdu) const
{
  return OnSendingNonStandardPDU(pdu.m_application, H245_DataMode_application::e_nonStandard);
}


PBoolean H323NonStandardDataCapability::OnReceivedPDU(const H245_DataApplicationCapability & pdu)
{
  return OnReceivedNonStandardPDU(pdu.m_application, H245_DataApplicationCapability_application::e_nonStandard);
}


PBoolean H323NonStandardDataCapability::IsMatch(const PASN_Choice & subTypePDU) const
{
  return H323Capability::IsMatch(subTypePDU) &&
         H323NonStandardCapabilityInfo::IsMatch((const H245_NonStandardParameter &)subTypePDU.GetObject());
}


/////////////////////////////////////////////////////////////////////////////

#ifdef H323_AUDIO_CODECS

H323_G711Capability::H323_G711Capability(Mode m, Speed s)
    : H323AudioCapability(20, 20) // 20ms recv, 20ms transmit
{
  mode = m;
  speed = s;
}


PObject * H323_G711Capability::Clone() const
{
  return new H323_G711Capability(*this);
}


unsigned H323_G711Capability::GetSubType() const
{
  static const unsigned G711SubType[2][2] = {
    { H245_AudioCapability::e_g711Alaw64k, H245_AudioCapability::e_g711Alaw56k },
    { H245_AudioCapability::e_g711Ulaw64k, H245_AudioCapability::e_g711Ulaw56k }
  };
  return G711SubType[mode][speed];
}


PString H323_G711Capability::GetFormatName() const
{
  static const char * const G711Name[4][2] = {
    { OPAL_G711_ALAW_64K, OPAL_G711_ALAW_56K },
    { OPAL_G711_ULAW_64K, OPAL_G711_ULAW_56K },
    { OPAL_G711_ALAW_64K_20, OPAL_G711_ALAW_56K_20 },
    { OPAL_G711_ULAW_64K_20, OPAL_G711_ULAW_56K_20 }
  };
  return G711Name[mode][speed];
}


H323Codec * H323_G711Capability::CreateCodec(H323Codec::Direction direction) const
{
  unsigned packetSize = 8*(direction == H323Codec::Encoder ? txFramesInPacket : rxFramesInPacket);

  if (mode == muLaw)
    return new H323_muLawCodec(direction, speed, packetSize);
  else
    return new H323_ALawCodec(direction, speed, packetSize);
}

#endif // H323AudioCodec


/////////////////////////////////////////////////////////////////////////////

char OpalUserInputRFC2833[] = "UserInput/RFC2833";

#ifdef H323_H249
// H.249 Identifiers
const char * const H323_UserInputCapability::SubTypeOID[4] = {
    "0.0.8.249.1",  // H.249 Annex A
    "0.0.8.249.2",  // H.249 Annex B
    "0.0.8.249.3",  // H.249 Annex C
    "0.0.8.249.4"   // H.249 Annex D
};
#endif

const char * const H323_UserInputCapability::SubTypeNames[NumSubTypes] = {
  "UserInput/basicString",
  "UserInput/iA5String",
  "UserInput/generalString",
  "UserInput/dtmf",
  "UserInput/hookflash",
  OpalUserInputRFC2833      // "UserInput/RFC2833"
#ifdef H323_H249
  ,"UserInput/Navigation",
  "UserInput/Softkey",
  "UserInput/PointDevice",
  "UserInput/Modal"
#endif
};

OPAL_MEDIA_FORMAT_DECLARE(OpalUserInputRFC2833Format,
        OpalUserInputRFC2833,
        OpalMediaFormat::DefaultAudioSessionID,
        (RTP_DataFrame::PayloadTypes)101, // Choose this for Cisco IOS compatibility
        TRUE,   // Needs jitter
        100,    // bits/sec
        4,      // bytes/frame
        8*150,  // 150 millisecond
        OpalMediaFormat::AudioTimeUnits,
        0)

H323_UserInputCapability::H323_UserInputCapability(SubTypes _subType)
{
  subType = _subType;
  
#ifdef H323_H249
  if (subType > 5)
       subTypeOID = SubTypeOID[subType-6];
  else {
#endif
  
    OpalMediaFormat * fmt = OpalMediaFormatFactory::CreateInstance(OpalUserInputRFC2833);
    if (fmt != NULL)
      rtpPayloadType = fmt->GetPayloadType();
      
#ifdef H323_H249
	subTypeOID = PString();
  }
#endif
}


PObject * H323_UserInputCapability::Clone() const
{
  return new H323_UserInputCapability(*this);
}


H323Capability::MainTypes H323_UserInputCapability::GetMainType() const
{
  return e_UserInput;
}


#define SignalToneRFC2833_SubType 10000

static unsigned UserInputCapabilitySubTypeCodes[] = {
  H245_UserInputCapability::e_basicString,
  H245_UserInputCapability::e_iA5String,
  H245_UserInputCapability::e_generalString,
  H245_UserInputCapability::e_dtmf,
  H245_UserInputCapability::e_hookflash,
  SignalToneRFC2833_SubType
#ifdef H323_H249
  ,H245_UserInputCapability::e_genericUserInputCapability,  // H.249 Annex A
  H245_UserInputCapability::e_genericUserInputCapability,  // H.249 Annex B
  H245_UserInputCapability::e_genericUserInputCapability,  // H.249 Annex C
  H245_UserInputCapability::e_genericUserInputCapability   // H.249 Annex D
#endif
};

unsigned  H323_UserInputCapability::GetSubType()  const
{
	return UserInputCapabilitySubTypeCodes[subType];
}

#ifdef H323_H249
PString H323_UserInputCapability::GetIdentifier() const
{
	return subTypeOID;
}
#endif

PString H323_UserInputCapability::GetFormatName() const
{
  return SubTypeNames[subType];
}


H323Channel * H323_UserInputCapability::CreateChannel(H323Connection &,
                                                      H323Channel::Directions,
                                                      unsigned,
                                                      const H245_H2250LogicalChannelParameters *) const
{
  PTRACE(1, "Codec\tCannot create UserInputCapability channel");
  return NULL;
}


H323Codec * H323_UserInputCapability::CreateCodec(H323Codec::Direction) const
{
  PTRACE(1, "Codec\tCannot create UserInputCapability codec");
  return NULL;
}

#ifdef H323_H249
H245_GenericInformation * H323_UserInputCapability::BuildGenericIndication(const char * oid)
{
  H245_GenericInformation * cap = new H245_GenericInformation();
  cap->IncludeOptionalField(H245_GenericMessage::e_subMessageIdentifier);

  H245_CapabilityIdentifier & id = cap->m_messageIdentifier;
  id.SetTag(H245_CapabilityIdentifier::e_standard);
  PASN_ObjectId & gid = id;
  gid.SetValue(oid);
  return cap;
}

H245_GenericParameter * H323_UserInputCapability::BuildGenericParameter(unsigned id,unsigned type, const PString & value)
{  

 H245_GenericParameter * content = new H245_GenericParameter();
      H245_ParameterIdentifier & paramid = content->m_parameterIdentifier;
        paramid.SetTag(H245_ParameterIdentifier::e_standard);
        PASN_Integer & pid = paramid;
        pid.SetValue(id);
	  H245_ParameterValue & paramval = content->m_parameterValue;
	    paramval.SetTag(type);
		 if ((type == H245_ParameterValue::e_unsignedMin) ||
			(type == H245_ParameterValue::e_unsignedMax) ||
			(type == H245_ParameterValue::e_unsigned32Min) ||
			(type == H245_ParameterValue::e_unsigned32Max)) {
				PASN_Integer & val = paramval;
				val.SetValue(value.AsUnsigned());
		 } else if (type == H245_ParameterValue::e_octetString) {
				PASN_OctetString & val = paramval;
				val.SetValue(value);
		 }		
//			H245_ParameterValue::e_logical,
//			H245_ParameterValue::e_booleanArray,
//			H245_ParameterValue::e_genericParameter

     return content;
}
#endif

PBoolean H323_UserInputCapability::OnSendingPDU(H245_Capability & pdu) const
{
  if (subType == SignalToneRFC2833) {
    pdu.SetTag(H245_Capability::e_receiveRTPAudioTelephonyEventCapability);
    H245_AudioTelephonyEventCapability & atec = pdu;
    atec.m_dynamicRTPPayloadType = rtpPayloadType;
    atec.m_audioTelephoneEvent = "0-16"; // Support DTMF 0-9,*,#,A-D & hookflash
  }
  else {
    pdu.SetTag(H245_Capability::e_receiveUserInputCapability);
    H245_UserInputCapability & ui = pdu;
    ui.SetTag(UserInputCapabilitySubTypeCodes[subType]);

#ifdef H323_H249
	// H.249 Capabilities
	if (subType > 5) {
		H245_GenericCapability & generic = ui;
		H245_CapabilityIdentifier & id = generic.m_capabilityIdentifier;
        id.SetTag(H245_CapabilityIdentifier::e_standard);
		PASN_ObjectId & oid = id;
		oid.SetValue(subTypeOID);

		if (subType == H249B_Softkey) {
			H245_ArrayOf_GenericParameter & col = generic.m_collapsing;
			// Set this to 10 so to support either 2 or 5
		    H245_GenericParameter * param = 
			       BuildGenericParameter(1,H245_ParameterValue::e_unsignedMin,10); 
		  col.Append(param);  
		  col.SetSize(col.GetSize()+1);
	    }
	}
#endif
  }
  return TRUE;
}


PBoolean H323_UserInputCapability::OnSendingPDU(H245_DataType &) const
{
  PTRACE(1, "Codec\tCannot have UserInputCapability in DataType");
  return FALSE;
}


PBoolean H323_UserInputCapability::OnSendingPDU(H245_ModeElement &) const
{
  PTRACE(1, "Codec\tCannot have UserInputCapability in ModeElement");
  return FALSE;
}


PBoolean H323_UserInputCapability::OnReceivedPDU(const H245_Capability & pdu)
{
  H323Capability::OnReceivedPDU(pdu);

  if (pdu.GetTag() == H245_Capability::e_receiveRTPAudioTelephonyEventCapability) {
    subType = SignalToneRFC2833;
    const H245_AudioTelephonyEventCapability & atec = pdu;
    rtpPayloadType = (RTP_DataFrame::PayloadTypes)(int)atec.m_dynamicRTPPayloadType;
    // Really should verify atec.m_audioTelephoneEvent here
    return TRUE;
  }

  const H245_UserInputCapability & ui = pdu;
  if (ui.GetTag() != UserInputCapabilitySubTypeCodes[subType])
	  return FALSE;

#ifdef H323_H249
  if (ui.GetTag() == H245_UserInputCapability::e_genericUserInputCapability) {
	  const H245_GenericCapability & generic = ui;
	  const H245_CapabilityIdentifier & id = generic.m_capabilityIdentifier;
      if (id.GetTag() != H245_CapabilityIdentifier::e_standard)
		  return FALSE;
	  
	  const PASN_ObjectId & oid = id;
	  return (subTypeOID == oid.AsString());
  }
#endif

  return TRUE;

}


PBoolean H323_UserInputCapability::OnReceivedPDU(const H245_DataType &, PBoolean)
{
  PTRACE(1, "Codec\tCannot have UserInputCapability in DataType");
  return FALSE;
}


PBoolean H323_UserInputCapability::IsUsable(const H323Connection & connection) const
{
  if (connection.GetControlVersion() >= 7)
    return TRUE;

  if (connection.GetRemoteApplication().Find("AltiServ-ITG") != P_MAX_INDEX)
    return FALSE;

  return subType != SignalToneRFC2833;
}


void H323_UserInputCapability::AddAllCapabilities(H323Capabilities & capabilities,
                                                  PINDEX descriptorNum,
                                                  PINDEX simultaneous)
{
  PINDEX num = capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(HookFlashH245));
  if (descriptorNum == P_MAX_INDEX) {
    descriptorNum = num;
    simultaneous = P_MAX_INDEX;
  }
  else if (simultaneous == P_MAX_INDEX)
    simultaneous = num+1;

  num = capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(BasicString));
  if (simultaneous == P_MAX_INDEX)
    simultaneous = num;

  capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(SignalToneH245));
  capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(SignalToneRFC2833));

#ifdef H323_H249
//// H.249 Capabilities
  capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(H249A_Navigation));
  capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(H249B_Softkey));
  capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(H249C_PointDevice));
  capabilities.SetCapability(descriptorNum, simultaneous, new H323_UserInputCapability(H249D_Modal));
#endif
}


/////////////////////////////////////////////////////////////////////////////

PBoolean H323SimultaneousCapabilities::SetSize(PINDEX newSize)
{
  PINDEX oldSize = GetSize();

  if (!H323CapabilitiesListArray::SetSize(newSize))
    return FALSE;

  while (oldSize < newSize) {
    H323CapabilitiesList * list = new H323CapabilitiesList;
    // The lowest level list should not delete codecs on destruction
    list->DisallowDeleteObjects();
    SetAt(oldSize++, list);
  }

  return TRUE;
}


PBoolean H323CapabilitiesSet::SetSize(PINDEX newSize)
{
  PINDEX oldSize = GetSize();

  if (!H323CapabilitiesSetArray::SetSize(newSize))
    return FALSE;

  while (oldSize < newSize)
    SetAt(oldSize++, new H323SimultaneousCapabilities);

  return TRUE;
}


H323Capabilities::H323Capabilities()
{
}


H323Capabilities::H323Capabilities(const H323Connection & connection,
                                   const H245_TerminalCapabilitySet & pdu)
{
  H323Capabilities allCapabilities;
  const H323Capabilities & localCapabilities = connection.GetLocalCapabilities();
  for (PINDEX c = 0; c < localCapabilities.GetSize(); c++)
    allCapabilities.Add(allCapabilities.Copy(localCapabilities[c]));
//  allCapabilities.AddAllCapabilities(0, 0, "*");
//  H323_UserInputCapability::AddAllCapabilities(allCapabilities, P_MAX_INDEX, P_MAX_INDEX);

  // Decode out of the PDU, the list of known codecs.
  if (pdu.HasOptionalField(H245_TerminalCapabilitySet::e_capabilityTable)) {
    for (PINDEX i = 0; i < pdu.m_capabilityTable.GetSize(); i++) {
      if (pdu.m_capabilityTable[i].HasOptionalField(H245_CapabilityTableEntry::e_capability)) {
        H323Capability * capability = allCapabilities.FindCapability(pdu.m_capabilityTable[i].m_capability);
        if (capability != NULL) {
          H323Capability * copy = (H323Capability *)capability->Clone();
          copy->SetCapabilityNumber(pdu.m_capabilityTable[i].m_capabilityTableEntryNumber);
          if (copy->OnReceivedPDU(pdu.m_capabilityTable[i].m_capability))
            table.Append(copy);
          else
            delete copy;
        }
      }
    }
  }

  PINDEX outerSize = pdu.m_capabilityDescriptors.GetSize();
  set.SetSize(outerSize);
  for (PINDEX outer = 0; outer < outerSize; outer++) {
    H245_CapabilityDescriptor & desc = pdu.m_capabilityDescriptors[outer];
    if (desc.HasOptionalField(H245_CapabilityDescriptor::e_simultaneousCapabilities)) {
      PINDEX middleSize = desc.m_simultaneousCapabilities.GetSize();
      set[outer].SetSize(middleSize);
      for (PINDEX middle = 0; middle < middleSize; middle++) {
        H245_AlternativeCapabilitySet & alt = desc.m_simultaneousCapabilities[middle];
        for (PINDEX inner = 0; inner < alt.GetSize(); inner++) {
          for (PINDEX cap = 0; cap < table.GetSize(); cap++) {
            if (table[cap].GetCapabilityNumber() == alt[inner]) {
              set[outer][middle].Append(&table[cap]);
              break;
            }
          }
        }
      }
    }
  }
}


H323Capabilities::H323Capabilities(const H323Capabilities & original)
{
  operator=(original);
}


H323Capabilities & H323Capabilities::operator=(const H323Capabilities & original)
{
  RemoveAll();

  for (PINDEX i = 0; i < original.GetSize(); i++)
    Copy(original[i]);

  PINDEX outerSize = original.set.GetSize();
  set.SetSize(outerSize);
  for (PINDEX outer = 0; outer < outerSize; outer++) {
    PINDEX middleSize = original.set[outer].GetSize();
    set[outer].SetSize(middleSize);
    for (PINDEX middle = 0; middle < middleSize; middle++) {
      PINDEX innerSize = original.set[outer][middle].GetSize();
      for (PINDEX inner = 0; inner < innerSize; inner++)
        set[outer][middle].Append(FindCapability(original.set[outer][middle][inner].GetCapabilityNumber()));
    }
  }

  return *this;
}


void H323Capabilities::PrintOn(ostream & strm) const
{
  int indent = (int)strm.precision()-1;
  strm << setw(indent) << " " << "Table:\n";
  for (PINDEX i = 0; i < table.GetSize(); i++)
    strm << setw(indent+2) << " " << table[i] << '\n';

  strm << setw(indent) << " " << "Set:\n";
  for (PINDEX outer = 0; outer < set.GetSize(); outer++) {
    strm << setw(indent+2) << " " << outer << ":\n";
    for (PINDEX middle = 0; middle < set[outer].GetSize(); middle++) {
      strm << setw(indent+4) << " " << middle << ":\n";
      for (PINDEX inner = 0; inner < set[outer][middle].GetSize(); inner++)
        strm << setw(indent+6) << " " << set[outer][middle][inner] << '\n';
    }
  }
}


PINDEX H323Capabilities::SetCapability(PINDEX descriptorNum,
                                       PINDEX simultaneousNum,
                                       H323Capability * capability)
{
  if (capability == NULL)
    return P_MAX_INDEX;

  // Make sure capability has been added to table.
  Add(capability);

  PBoolean newDescriptor = descriptorNum == P_MAX_INDEX;
  if (newDescriptor)
    descriptorNum = set.GetSize();

  // Make sure the outer array is big enough
  set.SetMinSize(descriptorNum+1);

  if (simultaneousNum == P_MAX_INDEX)
    simultaneousNum = set[descriptorNum].GetSize();

  // Make sure the middle array is big enough
  set[descriptorNum].SetMinSize(simultaneousNum+1);

  // Now we can put the new entry in.
  set[descriptorNum][simultaneousNum].Append(capability);
  return newDescriptor ? descriptorNum : simultaneousNum;
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
      // Hack to avoid accidentally deleting H.239 cap as the codec is after position 5  -SH
      if (next > 5 && str.Left(5) == "H.239")
        return FALSE;

      last = next + wildcard[i].GetLength();
    }
  }

  return TRUE;
}


PINDEX H323Capabilities::AddAllCapabilities(PINDEX descriptorNum,
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
          PINDEX num = SetCapability(descriptorNum, simultaneous, capability);
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


static unsigned MergeCapabilityNumber(const H323CapabilitiesList & table,
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


void H323Capabilities::Add(H323Capability * capability)
{
  if (capability == NULL)
    return;

  // See if already added, confuses things if you add the same instance twice
  if (table.GetObjectsIndex(capability) != P_MAX_INDEX)
    return;

  capability->SetCapabilityNumber(MergeCapabilityNumber(table, capability->GetCapabilityNumber()));
  table.Append(capability);

  PTRACE(3, "H323\tAdded capability: " << *capability);
}


H323Capability * H323Capabilities::Copy(const H323Capability & capability)
{
  H323Capability * newCapability = (H323Capability *)capability.Clone();
  newCapability->SetCapabilityNumber(MergeCapabilityNumber(table, capability.GetCapabilityNumber()));
  table.Append(newCapability);

  PTRACE(3, "H323\tAdded capability: " << *newCapability);
  return newCapability;
}


void H323Capabilities::Remove(H323Capability * capability)
{
  if (capability == NULL)
    return;

  PTRACE(3, "H323\tRemoving capability: " << *capability);

  unsigned capabilityNumber = capability->GetCapabilityNumber();

  for (PINDEX outer = 0; outer < set.GetSize(); outer++) {
    for (PINDEX middle = 0; middle < set[outer].GetSize(); middle++) {
      for (PINDEX inner = 0; inner < set[outer][middle].GetSize(); inner++) {
        if (set[outer][middle][inner].GetCapabilityNumber() == capabilityNumber) {
          set[outer][middle].RemoveAt(inner);
          break;
        }
      }
      if (set[outer][middle].GetSize() == 0)
        set[outer].RemoveAt(middle);
    }
    if (set[outer].GetSize() == 0)
      set.RemoveAt(outer);
  }

  table.Remove(capability);
}


void H323Capabilities::Remove(const PString & codecName)
{
  if (codecName.IsEmpty())
    return;

  H323Capability * cap = FindCapability(codecName);
  while (cap != NULL) {
    Remove(cap);
    cap = FindCapability(codecName);
  }
}


void H323Capabilities::Remove(const PStringArray & codecNames)
{
  for (PINDEX i = 0; i < codecNames.GetSize(); i++)
    Remove(codecNames[i]);
}


void H323Capabilities::RemoveAll()
{
  table.RemoveAll();
  set.RemoveAll();
}


H323Capability * H323Capabilities::FindCapability(unsigned capabilityNumber) const
{
  PTRACE(4, "H323\tFindCapability: " << capabilityNumber);

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    if (table[i].GetCapabilityNumber() == capabilityNumber) {
      PTRACE(3, "H323\tFound capability: " << table[i]);
      return &table[i];
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(const PString & formatName,
                              H323Capability::CapabilityDirection direction) const
{
  PTRACE(4, "H323\tFindCapability: \"" << formatName << '"');

  PStringArray wildcard = formatName.Tokenise('*', FALSE);

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    PCaselessString str = table[i].GetFormatName();
    if (MatchWildcard(str, wildcard) &&
          (direction == H323Capability::e_Unknown ||
           table[i].GetCapabilityDirection() == direction)) {
      PTRACE(3, "H323\tFound capability: " << table[i]);
      return &table[i];
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(
                              H323Capability::CapabilityDirection direction) const
{
  PTRACE(4, "H323\tFindCapability: \"" << direction << '"');

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    if (table[i].GetCapabilityDirection() == direction) {
      PTRACE(3, "H323\tFound capability: " << table[i]);
      return &table[i];
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(const H323Capability & capability) const
{
  PTRACE(4, "H323\tFindCapability: " << capability);

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    if (table[i] == capability) {
      PTRACE(3, "H323\tFound capability: " << table[i]);
      return &table[i];
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(const H245_Capability & cap) const
{
  PTRACE(4, "H323\tFindCapability: " << cap.GetTagName());

  switch (cap.GetTag()) {
    case H245_Capability::e_receiveAudioCapability :
    case H245_Capability::e_transmitAudioCapability :
    case H245_Capability::e_receiveAndTransmitAudioCapability :
    {
      const H245_AudioCapability & audio = cap;
      if (audio.GetTag() == H245_AudioCapability::e_genericAudioCapability)
          return FindCapability(H323Capability::e_Audio, audio, audio);
      else
          return FindCapability(H323Capability::e_Audio, audio, NULL);
    }

    case H245_Capability::e_receiveVideoCapability :
    case H245_Capability::e_transmitVideoCapability :
    case H245_Capability::e_receiveAndTransmitVideoCapability :
    {
      const H245_VideoCapability & video = cap;
      if (video.GetTag() == H245_VideoCapability::e_genericVideoCapability)
        return FindCapability(H323Capability::e_Video, video, video);
      else if (video.GetTag() == H245_VideoCapability::e_extendedVideoCapability)
        return FindCapability(true,video);
      else
        return FindCapability(H323Capability::e_Video, video, NULL);
    }

    case H245_Capability::e_receiveDataApplicationCapability :
    case H245_Capability::e_transmitDataApplicationCapability :
    case H245_Capability::e_receiveAndTransmitDataApplicationCapability :
    {
      const H245_DataApplicationCapability & data = cap;
      return FindCapability(H323Capability::e_Data, data.m_application, NULL);
    }

    case H245_Capability::e_receiveUserInputCapability :
    case H245_Capability::e_transmitUserInputCapability :
    case H245_Capability::e_receiveAndTransmitUserInputCapability :
    {
      const H245_UserInputCapability & ui = cap;
	  if (ui.GetTag() == H245_UserInputCapability::e_genericUserInputCapability)
		return FindCapability(H323Capability::e_UserInput, ui, ui);
	  else
		return FindCapability(H323Capability::e_UserInput, ui, NULL);
    }

    case H245_Capability::e_receiveRTPAudioTelephonyEventCapability :
      return FindCapability(H323Capability::e_UserInput, SignalToneRFC2833_SubType);

	case H245_Capability::e_genericControlCapability :
	  return FindCapability(H323Capability::e_GenericControl);

	case H245_Capability::e_conferenceCapability :
	  return FindCapability(H323Capability::e_ConferenceControl);

    default :
      break;
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(const H245_DataType & dataType) const
{
  PTRACE(4, "H323\tFindCapability: " << dataType.GetTagName());

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    H323Capability & capability = table[i];
    PBoolean checkExact=false;
    switch (dataType.GetTag()) {
      case H245_DataType::e_audioData :
      {
  	   if (capability.GetMainType() == H323Capability::e_Audio) { 
        const H245_AudioCapability & audio = dataType;
        checkExact = capability.IsMatch(audio);
	   }
        break;
      }

      case H245_DataType::e_videoData :
      {
	   if (capability.GetMainType() == H323Capability::e_Video) { 
        const H245_VideoCapability & video = dataType;
        checkExact = capability.IsMatch(video);
	   }
        break;
      }

      case H245_DataType::e_data :
      {
	   if (capability.GetMainType() == H323Capability::e_Data) { 
        const H245_DataApplicationCapability & data = dataType;
        checkExact = capability.IsMatch(data.m_application);
	   }
        break;
      }

#ifdef H323_H235
      case H245_DataType::e_h235Media :
      {
       if (PIsDescendant(&capability, H323SecureCapability)) { 
         const H245_H235Media & data = dataType;
         checkExact = capability.IsMatch(data.m_mediaType);
	   }
        break;
      }
#endif

      default :
        checkExact = FALSE;
    }

    if (checkExact) {
      H323Capability * compare = (H323Capability *)capability.Clone();
      if (compare->OnReceivedPDU(dataType, FALSE) && *compare == capability) {
        delete compare;
        PTRACE(3, "H323\tFound capability: " << capability);
        return &capability;
      }
      delete compare;
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(const H245_ModeElement & modeElement) const
{
  PTRACE(4, "H323\tFindCapability: " << modeElement.m_type.GetTagName());

  switch (modeElement.m_type.GetTag()) {
    case H245_ModeElementType::e_audioMode :
      {
        const H245_AudioMode & audio = modeElement.m_type;
        static unsigned const AudioSubTypes[] = {
	  H245_AudioCapability::e_nonStandard,
	  H245_AudioCapability::e_g711Alaw64k,
	  H245_AudioCapability::e_g711Alaw56k,
	  H245_AudioCapability::e_g711Ulaw64k,
	  H245_AudioCapability::e_g711Ulaw56k,
	  H245_AudioCapability::e_g722_64k,
	  H245_AudioCapability::e_g722_56k,
	  H245_AudioCapability::e_g722_48k,
	  H245_AudioCapability::e_g728,
	  H245_AudioCapability::e_g729,
	  H245_AudioCapability::e_g729AnnexA,
	  H245_AudioCapability::e_g7231,
	  H245_AudioCapability::e_is11172AudioCapability,
	  H245_AudioCapability::e_is13818AudioCapability,
	  H245_AudioCapability::e_g729wAnnexB,
	  H245_AudioCapability::e_g729AnnexAwAnnexB,
	  H245_AudioCapability::e_g7231AnnexCCapability,
	  H245_AudioCapability::e_gsmFullRate,
	  H245_AudioCapability::e_gsmHalfRate,
	  H245_AudioCapability::e_gsmEnhancedFullRate,
	  H245_AudioCapability::e_genericAudioCapability,
	  H245_AudioCapability::e_g729Extensions
        };
        return FindCapability(H323Capability::e_Audio, audio, AudioSubTypes);
      }

    case H245_ModeElementType::e_videoMode :
      {
        const H245_VideoMode & video = modeElement.m_type;
        static unsigned const VideoSubTypes[] = {
	  H245_VideoCapability::e_nonStandard,
	  H245_VideoCapability::e_h261VideoCapability,
	  H245_VideoCapability::e_h262VideoCapability,
	  H245_VideoCapability::e_h263VideoCapability,
	  H245_VideoCapability::e_is11172VideoCapability,
	  H245_VideoCapability::e_genericVideoCapability
        };
        return FindCapability(H323Capability::e_Video, video, VideoSubTypes);
      }

    case H245_ModeElementType::e_dataMode :
      {
        const H245_DataMode & data = modeElement.m_type;
        static unsigned const DataSubTypes[] = {
	  H245_DataApplicationCapability_application::e_nonStandard,
	  H245_DataApplicationCapability_application::e_t120,
	  H245_DataApplicationCapability_application::e_dsm_cc,
	  H245_DataApplicationCapability_application::e_userData,
	  H245_DataApplicationCapability_application::e_t84,
	  H245_DataApplicationCapability_application::e_t434,
	  H245_DataApplicationCapability_application::e_h224,
	  H245_DataApplicationCapability_application::e_nlpid,
	  H245_DataApplicationCapability_application::e_dsvdControl,
	  H245_DataApplicationCapability_application::e_h222DataPartitioning,
	  H245_DataApplicationCapability_application::e_t30fax,
	  H245_DataApplicationCapability_application::e_t140,
	  H245_DataApplicationCapability_application::e_t38fax,
	  H245_DataApplicationCapability_application::e_genericDataCapability
        };
        return FindCapability(H323Capability::e_Data, data.m_application, DataSubTypes);
      }

    default :
      break;
  }

  return NULL;
}

H323Capability * H323Capabilities::FindCapability(H323Capability::MainTypes mainType,
												  const PASN_Choice & subTypePDU,
                                                  const H245_GenericCapability & gen) const
{
   const H245_CapabilityIdentifier & id = gen.m_capabilityIdentifier;
	if (id.GetTag() != H245_CapabilityIdentifier::e_standard)
		return NULL;

	const PASN_ObjectId & idx = gen.m_capabilityIdentifier;
	PString oid = idx.AsString();

	PTRACE(4, "H323\tFindCapability: " << mainType << " Generic " << oid);

	unsigned int subType = subTypePDU.GetTag();
	for (PINDEX i = 0; i < table.GetSize(); i++) {
		H323Capability & capability = table[i];
		if (capability.GetMainType() == mainType &&
			(subType == UINT_MAX || capability.GetSubType() == subType) &&
			(capability.GetIdentifier() == oid)) {
				PTRACE(3, "H323\tFound capability: " << capability);
				return &capability;
		}
	}

	return NULL;
}

H323Capability * H323Capabilities::FindCapability(bool, const H245_ExtendedVideoCapability & gen) const
{
#ifdef H323_VIDEO
  H323Capability * newCap = NULL;
  for (PINDEX j=0; j < gen.m_videoCapability.GetSize(); ++j) {
    const H245_VideoCapability & vidCap = gen.m_videoCapability[j];
	for (PINDEX i = 0; i < table.GetSize(); i++) {
		H323Capability & capability = table[i];
		if (capability.GetMainType() == H323Capability::e_Video &&
            capability.GetSubType() == H245_VideoCapability::e_extendedVideoCapability) {
            if (vidCap.GetTag() == H245_VideoCapability::e_genericVideoCapability)
               newCap = ((H323ExtendedVideoCapability &)capability).GetCapabilities().FindCapability(H323Capability::e_Video, vidCap, vidCap);
            else  
               newCap = ((H323ExtendedVideoCapability &)capability).GetCapabilities().FindCapability(H323Capability::e_Video, vidCap, NULL);

            if (newCap)
                return &capability;
        }
    }
  }
#endif
  return NULL;
}

H323Capability * H323Capabilities::FindCapability(H323Capability::MainTypes mainType,
                                                  const PASN_Choice & subTypePDU,
                                                  const unsigned * translationTable) const
{
    unsigned int subTypeID = subTypePDU.GetTag();
  if (subTypePDU.GetTag() != 0) {
    if (translationTable != NULL)
      subTypeID = translationTable[subTypeID];
    return FindCapability(mainType, subTypeID);
  }

  PTRACE(4, "H323\tFindCapability: " << mainType << " nonStandard");

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    H323Capability & capability = table[i];
    if (capability.IsMatch(subTypePDU)) {
      PTRACE(3, "H323\tFound capability: " << capability);
      return &capability;
    }
  }

  return NULL;
}


H323Capability * H323Capabilities::FindCapability(H323Capability::MainTypes mainType,
                                                  unsigned subType) const
{
  if (subType != UINT_MAX) {
     PTRACE(4, "H323\tFindCapability: " << mainType << " subtype=" << subType);
  }

  for (PINDEX i = 0; i < table.GetSize(); i++) {
    H323Capability & capability = table[i];
    if (capability.GetMainType() == mainType &&
                        (subType == UINT_MAX || capability.GetSubType() == subType)) {
      PTRACE(3, "H323\tFound capability: " << capability);
      return &capability;
    }
  }

  return NULL;
}

PBoolean H323Capabilities::RemoveCapability(H323Capability::MainTypes capabilityType)
{
	// List of codecs
	PStringList codecsToRemove;
	for (PINDEX i = 0; i < table.GetSize(); i++) {
	    H323Capability & capability = table[i];
		if (capabilityType == H323Capability::e_Video) {
		    if ((capability.GetMainType() == H323Capability::e_Video) &&
			    (capability.GetSubType() != H245_VideoCapability::e_extendedVideoCapability))
			      codecsToRemove.AppendString(capability.GetFormatName()); 
		} else if ((capabilityType == H323Capability::e_ExtendVideo) &&
             (capability.GetMainType() == H323Capability::e_Video) &&
			 (capability.GetSubType() == H245_VideoCapability::e_extendedVideoCapability)) {
                  codecsToRemove.AppendString(capability.GetFormatName());
		} else 
			 if (capability.GetMainType() == capabilityType)
			   codecsToRemove.AppendString(capability.GetFormatName());  
	}

	for (PINDEX i=0; i < codecsToRemove.GetSize(); i++) 
		      Remove(codecsToRemove[i]);

	return TRUE;
}

#ifdef H323_VIDEO
PBoolean H323Capabilities::SetVideoEncoder(unsigned frameWidth, unsigned frameHeight, unsigned frameRate)
{
   	for (PINDEX i = 0; i < table.GetSize(); i++) {
     H323Capability & capability = table[i];
	  if (capability.GetMainType() == H323Capability::e_Video) 
		 capability.SetCustomEncode(frameWidth,frameHeight,frameRate);
    }
	return true;
}

PBoolean H323Capabilities::SetVideoFrameSize(H323Capability::CapabilityFrameSize frameSize, int frameUnits) 
{ 
    // Remove the unmatching capabilities
    if (frameSize != H323Capability::cif16MPI) Remove("*-16CIF*");
    if (frameSize != H323Capability::cif4MPI) Remove("*-4CIF*");
    if (frameSize != H323Capability::cifMPI) Remove("*-CIF*");
    if (frameSize != H323Capability::qcifMPI) Remove("*-QCIF*");
    if (frameSize != H323Capability::sqcifMPI) Remove("*-SQCIF*");
    if (frameSize != H323Capability::p720MPI) Remove("*-720*");
	if (frameSize != H323Capability::i1080MPI) Remove("*-1080*");

    // Remove Generic size Capabilities
    PStringList genericCaps;
    if ((frameSize != H323Capability::i1080MPI) &&
        (frameSize != H323Capability::p720MPI) &&
        (frameSize != H323Capability::i480MPI) &&
        (frameSize != H323Capability::cif16MPI) &&
        (frameSize != H323Capability::cif4MPI)) {
            for (PINDEX i = 0; i < table.GetSize(); i++) {
                H323Capability & capability = table[i];
                if ((capability.GetMainType() == H323Capability::e_Video) &&
                    (capability.GetSubType() != H245_VideoCapability::e_extendedVideoCapability)) {
                        PCaselessString str = table[i].GetFormatName();
                        PString formatName = "*-*";
                        PStringArray wildcard = formatName.Tokenise('*', FALSE);
                        if (!MatchWildcard(str, wildcard))
                            genericCaps.AppendString(str);
                }
            }
            Remove(genericCaps);
    }

	// Instruct remaining Video Capabilities to set Frame Size to new Value
	for (PINDEX i = 0; i < table.GetSize(); i++) {
	    H323Capability & capability = table[i];
	    if (capability.GetMainType() == H323Capability::e_Video)
			     capability.SetMaxFrameSize(frameSize,frameUnits);
	}
	return TRUE; 
}
#endif

void H323Capabilities::BuildPDU(const H323Connection & connection,
                                H245_TerminalCapabilitySet & pdu) const
{
  PINDEX tableSize = table.GetSize();
  PINDEX setSize = set.GetSize();
  //PAssert((tableSize > 0) == (setSize > 0), PLogicError);
  if (tableSize == 0 || setSize == 0)
    return;

  // Set the table of capabilities
  pdu.IncludeOptionalField(H245_TerminalCapabilitySet::e_capabilityTable);

  H245_H2250Capability & h225_0 = pdu.m_multiplexCapability;
  PINDEX rtpPacketizationCount = 0;

  PINDEX count = 0;
  for (PINDEX i = 0; i < tableSize; i++) {
    H323Capability & capability = table[i];
    if (capability.IsUsable(connection)) {
      pdu.m_capabilityTable.SetSize(count+1);
      H245_CapabilityTableEntry & entry = pdu.m_capabilityTable[count++];
      entry.m_capabilityTableEntryNumber = capability.GetCapabilityNumber();
      entry.IncludeOptionalField(H245_CapabilityTableEntry::e_capability);
      capability.OnSendingPDU(entry.m_capability);

      h225_0.m_mediaPacketizationCapability.m_rtpPayloadType.SetSize(rtpPacketizationCount+1);
      if (H323SetRTPPacketization(h225_0.m_mediaPacketizationCapability.m_rtpPayloadType[rtpPacketizationCount],
                                                    capability.GetMediaFormat(), RTP_DataFrame::MaxPayloadType)) {
        // Check if already in list
        PINDEX test;
        for (test = 0; test < rtpPacketizationCount; test++) {
          if (h225_0.m_mediaPacketizationCapability.m_rtpPayloadType[test] == h225_0.m_mediaPacketizationCapability.m_rtpPayloadType[rtpPacketizationCount])
            break;
        }
        if (test == rtpPacketizationCount)
          rtpPacketizationCount++;
      }
    }
  }

  // Have some mediaPacketizations to include.
  if (rtpPacketizationCount > 0) {
    h225_0.m_mediaPacketizationCapability.m_rtpPayloadType.SetSize(rtpPacketizationCount);
    h225_0.m_mediaPacketizationCapability.IncludeOptionalField(H245_MediaPacketizationCapability::e_rtpPayloadType);
  }

  // Set the sets of compatible capabilities
  pdu.IncludeOptionalField(H245_TerminalCapabilitySet::e_capabilityDescriptors);

  pdu.m_capabilityDescriptors.SetSize(setSize);
  for (PINDEX outer = 0; outer < setSize; outer++) {
    H245_CapabilityDescriptor & desc = pdu.m_capabilityDescriptors[outer];
    desc.m_capabilityDescriptorNumber = (unsigned)(outer + 1);
    desc.IncludeOptionalField(H245_CapabilityDescriptor::e_simultaneousCapabilities);
    PINDEX middleSize = set[outer].GetSize();
    desc.m_simultaneousCapabilities.SetSize(middleSize);
    for (PINDEX middle = 0; middle < middleSize; middle++) {
      H245_AlternativeCapabilitySet & alt = desc.m_simultaneousCapabilities[middle];
      PINDEX innerSize = set[outer][middle].GetSize();
      alt.SetSize(innerSize);
      count = 0;
      for (PINDEX inner = 0; inner < innerSize; inner++) {
        H323Capability & capability = set[outer][middle][inner];
        if (capability.IsUsable(connection)) {
          alt.SetSize(count+1);
          alt[count++] = capability.GetCapabilityNumber();
        }
      }
    }
  }
}


PBoolean H323Capabilities::Merge(const H323Capabilities & newCaps)
{
  PTRACE_IF(4, !table.IsEmpty(), "H245\tCapability merge of:\n" << newCaps
            << "\nInto:\n" << *this);

  // Add any new capabilities not already in set.
  PINDEX i;
  for (i = 0; i < newCaps.GetSize(); i++) {
    if (FindCapability(newCaps[i]) == NULL)
      Copy(newCaps[i]);
  }

  // This should merge instead of just adding to it.
  PINDEX outerSize = newCaps.set.GetSize();
  PINDEX outerBase = set.GetSize();
  set.SetSize(outerBase+outerSize);
  for (PINDEX outer = 0; outer < outerSize; outer++) {
    PINDEX middleSize = newCaps.set[outer].GetSize();
    set[outerBase+outer].SetSize(middleSize);
    for (PINDEX middle = 0; middle < middleSize; middle++) {
      PINDEX innerSize = newCaps.set[outer][middle].GetSize();
      for (PINDEX inner = 0; inner < innerSize; inner++) {
        H323Capability * cap = FindCapability(newCaps.set[outer][middle][inner].GetCapabilityNumber());
        if (cap != NULL)
          set[outerBase+outer][middle].Append(cap);
      }
    }
  }

  PTRACE_IF(4, !table.IsEmpty(), "H245\tCapability merge result:\n" << *this);
  PTRACE(3, "H245\tReceived capability set, is "
                 << (table.IsEmpty() ? "rejected" : "accepted"));
  return !table.IsEmpty();
}

const H323CapabilitiesSet & H323Capabilities::GetCapabilitySet()
{
	return set;
}

void H323Capabilities::Reorder(const PStringArray & preferenceOrder)
{
  if (preferenceOrder.IsEmpty())
    return;

  table.DisallowDeleteObjects();

  PINDEX preference = 0;
  PINDEX base = 0;

  for (preference = 0; preference < preferenceOrder.GetSize(); preference++) {
    PStringArray wildcard = preferenceOrder[preference].Tokenise('*', FALSE);
    for (PINDEX idx = base; idx < table.GetSize(); idx++) {
      PCaselessString str = table[idx].GetFormatName();
      if (MatchWildcard(str, wildcard)) {
        if (idx != base)
          table.InsertAt(base, table.RemoveAt(idx));
        base++;
      }
    }
  }

  for (PINDEX outer = 0; outer < set.GetSize(); outer++) {
    for (PINDEX middle = 0; middle < set[outer].GetSize(); middle++) {
      H323CapabilitiesList & list = set[outer][middle];
      for (PINDEX idx = 0; idx < table.GetSize(); idx++) {
        for (PINDEX inner = 0; inner < list.GetSize(); inner++) {
          if (&table[idx] == &list[inner]) {
            list.Append(list.RemoveAt(inner));
            break;
          }
        }
      }
    }
  }

  table.AllowDeleteObjects();
}


PBoolean H323Capabilities::IsAllowed(const H323Capability & capability)
{
  return IsAllowed(capability.GetCapabilityNumber());
}


PBoolean H323Capabilities::IsAllowed(const unsigned a_capno)
{
  // Check that capno is actually in the set
  PINDEX outerSize = set.GetSize();
  for (PINDEX outer = 0; outer < outerSize; outer++) {
    PINDEX middleSize = set[outer].GetSize();
    for (PINDEX middle = 0; middle < middleSize; middle++) {
      PINDEX innerSize = set[outer][middle].GetSize();
      for (PINDEX inner = 0; inner < innerSize; inner++) {
        if (a_capno == set[outer][middle][inner].GetCapabilityNumber()) {
          return TRUE;
        }
      }
    }
  }
  return FALSE;
}


PBoolean H323Capabilities::IsAllowed(const H323Capability & capability1,
                                 const H323Capability & capability2)
{
  return IsAllowed(capability1.GetCapabilityNumber(),
                   capability2.GetCapabilityNumber());
}


PBoolean H323Capabilities::IsAllowed(const unsigned a_capno1, const unsigned a_capno2)
{
  if (a_capno1 == a_capno2) {
    PTRACE(1, "H323\tH323Capabilities::IsAllowed() capabilities are the same.");
    return TRUE;
  }

  PINDEX outerSize = set.GetSize();
  for (PINDEX outer = 0; outer < outerSize; outer++) {
    PINDEX middleSize = set[outer].GetSize();
    for (PINDEX middle = 0; middle < middleSize; middle++) {
      PINDEX innerSize = set[outer][middle].GetSize();
      for (PINDEX inner = 0; inner < innerSize; inner++) {
        if (a_capno1 == set[outer][middle][inner].GetCapabilityNumber()) {
          /* Now go searching for the other half... */
          for (PINDEX middle2 = 0; middle2 < middleSize; ++middle2) {
            if (middle != middle2) {
              PINDEX innerSize2 = set[outer][middle2].GetSize();
              for (PINDEX inner2 = 0; inner2 < innerSize2; ++inner2) {
                if (a_capno2 == set[outer][middle2][inner2].GetCapabilityNumber()) {
                  return TRUE;
                }
              }
            }
          }
        }
      }
    }
  }
  return FALSE;
}

const H323CapabilitiesSet & H323Capabilities::GetSet() const
{
    return this->set;
}

/////////////////////////////////////////////////////////////////////////////

#ifndef PASN_NOPRINTON


struct msNonStandardCodecDef {
  const char * name;
  BYTE sig[2];
};


static msNonStandardCodecDef msNonStandardCodec[] = {
  { "L&H CELP 4.8k", { 0x01, 0x11 } },
  { "ADPCM",         { 0x02, 0x00 } },
  { "L&H CELP 8k",   { 0x02, 0x11 } },
  { "L&H CELP 12k",  { 0x03, 0x11 } },
  { "L&H CELP 16k",  { 0x04, 0x11 } },
  { "IMA-ADPCM",     { 0x11, 0x00 } },
  { "GSM",           { 0x31, 0x00 } },
  { NULL,            { 0,    0    } }
};

void H245_AudioCapability::PrintOn(ostream & strm) const
{
  strm << GetTagName();

  // tag 0 is nonstandard
  if (tag == 0) {

    H245_NonStandardParameter & param = (H245_NonStandardParameter &)GetObject();
    const PBYTEArray & data = param.m_data;

    switch (param.m_nonStandardIdentifier.GetTag()) {
      case H245_NonStandardIdentifier::e_h221NonStandard:
        {
          H245_NonStandardIdentifier_h221NonStandard & h221 = param.m_nonStandardIdentifier;

          // Microsoft is 181/0/21324
          if ((h221.m_t35CountryCode   == 181) &&
              (h221.m_t35Extension     == 0) &&
              (h221.m_manufacturerCode == 21324)
            ) {
            PString name = "Unknown";
            PINDEX i;
            if (data.GetSize() >= 21) {
              for (i = 0; msNonStandardCodec[i].name != NULL; i++) {
                if ((data[20] == msNonStandardCodec[i].sig[0]) && 
                    (data[21] == msNonStandardCodec[i].sig[1])) {
                  name = msNonStandardCodec[i].name;
                  break;
                }
              }
            }
            strm << (PString(" [Microsoft") & name) << "]";
          }

          // Equivalence is 9/0/61
          else if ((h221.m_t35CountryCode   == 9) &&
                   (h221.m_t35Extension     == 0) &&
                   (h221.m_manufacturerCode == 61)
                  ) {
            PString name;
            if (data.GetSize() > 0)
              name = PString((const char *)(const BYTE *)data, data.GetSize());
            strm << " [Equivalence " << name << "]";
          }

          // Xiph is 181/0/38
          else if ((h221.m_t35CountryCode   == 181) &&
                   (h221.m_t35Extension     == 0) &&
                   (h221.m_manufacturerCode == 38)
                  ) {
            PString name;
            if (data.GetSize() > 0)
              name = PString((const char *)(const BYTE *)data, data.GetSize());
            strm << " [Xiph " << name << "]";
          }

          // Cisco is 181/0/18
          else if ((h221.m_t35CountryCode   == 181) &&
                   (h221.m_t35Extension     == 0) &&
                   (h221.m_manufacturerCode == 18)
                  ) {
            PString name;
            if (data.GetSize() > 0)
              name = PString((const char *)(const BYTE *)data, data.GetSize());
            strm << " [Cisco " << name << "]";
          }

        }
        break;
      default:
        break;
    }
  }

  if (choice == NULL)
    strm << " (NULL)";
  else {
    strm << ' ' << *choice;
  }

  //PASN_Choice::PrintOn(strm);
}
#endif
