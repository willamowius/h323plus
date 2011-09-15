/*
 * mediafmt.cxx
 *
 * Media Format descriptions
 *
 * H323Plus Library
 *
 * Copyright (c) 1999-2000 Equivalence Pty. Ltd.
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
 *
 * $Id$
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "mediafmt.h"
#endif

#include "mediafmt.h"
#include "rtp.h"
#include "h323pluginmgr.h"

#include <ptclib/cypher.h>

#ifdef _WIN32
#ifndef _Ios_Fmtflags
  #define _Ios_Fmtflags ios::fmtflags
#endif
#endif

/////////////////////////////////////////////////////////////////////////////

char OpalPCM16[] = OPAL_PCM16;

OPAL_MEDIA_FORMAT_DECLARE(OpalPCM16Format, 
          OpalPCM16,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::L16_Mono,
          TRUE,   // Needs jitter
          128000, // bits/sec
          16, // bytes/frame
          8, // 1 millisecond
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG711uLaw64k[] = OPAL_G711_ULAW_64K;
char OpalG711uLaw64k20[] = OPAL_G711_ULAW_64K_20;

OPAL_MEDIA_FORMAT_DECLARE(OpalG711uLaw64kFormat20,
          OpalG711uLaw64k,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::PCMU,
          TRUE,   // Needs jitter
          64000, // bits/sec
          8, // bytes/frame
          20, // 1 millisecond/frame
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG711ALaw64k[] = OPAL_G711_ALAW_64K;
char OpalG711ALaw64k20[] = OPAL_G711_ALAW_64K_20;

OPAL_MEDIA_FORMAT_DECLARE(OpalG711ALaw64kFormat20,
          OpalG711ALaw64k,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::PCMA,
          TRUE,   // Needs jitter
          64000, // bits/sec
          8, // bytes/frame
          20, // 1 millisecond/frame
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG728[] = OPAL_G728;

OPAL_MEDIA_FORMAT_DECLARE(OpalG728Format,
          OpalG728,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::G728,
          TRUE, // Needs jitter
          16000,// bits/sec
          5,    // bytes
          20,   // 2.5 milliseconds
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG729[] = OPAL_G729;

OPAL_MEDIA_FORMAT_DECLARE(OpalG729Format,
          OpalG729,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::G729,
          TRUE, // Needs jitter
          8000, // bits/sec
          10,   // bytes
          10,   // 10 milliseconds
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG729A[] = OPAL_G729A;

OPAL_MEDIA_FORMAT_DECLARE(OpalG729AFormat,
          OpalG729A,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::G729,
          TRUE, // Needs jitter
          8000, // bits/sec
          10,   // bytes
          10,   // 10 milliseconds
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG729B[] = OPAL_G729B;

OPAL_MEDIA_FORMAT_DECLARE(OpalG729BFormat,
          OpalG729B,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::G729,
          TRUE, // Needs jitter
          8000, // bits/sec
          10,   // bytes
          10,   // 10 milliseconds
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG729AB[] = OPAL_G729AB;

OPAL_MEDIA_FORMAT_DECLARE(OpalG729ABFormat,
          OpalG729AB,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::G729,
          TRUE, // Needs jitter
          8000, // bits/sec
          10,   // bytes
          10,   // 10 milliseconds
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG7231_6k3[] = OPAL_G7231_6k3;

OPAL_MEDIA_FORMAT_DECLARE(OpalG7231_6k3Format,
          OpalG7231_6k3,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::G7231,
          TRUE, // Needs jitter
          6400, // bits/sec
          24,   // bytes
          30,  // 30 milliseconds
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG7231_5k3[] = OPAL_G7231_5k3;

OPAL_MEDIA_FORMAT_DECLARE(OpalG7231_5k3Format,
          OpalG7231_5k3,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::G7231,
          TRUE, // Needs jitter
          5300, // bits/sec
          24,   // bytes
          30,  // 30 milliseconds
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG7231A_6k3[] = OPAL_G7231A_6k3;

OPAL_MEDIA_FORMAT_DECLARE(OpalG7231A_6k3Format,
          OpalG7231A_6k3,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::G7231,
          TRUE, // Needs jitter
          6400, // bits/sec
          24,   // bytes
          30,  // 30 milliseconds
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalG7231A_5k3[] = OPAL_G7231A_5k3;

OPAL_MEDIA_FORMAT_DECLARE(OpalG7231A_5k3Format,
          OpalG7231A_5k3,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::G7231,
          TRUE, // Needs jitter
          5300, // bits/sec
          24,   // bytes
          30,  // 30 milliseconds
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalGSM0610[] = OPAL_GSM0610;

OPAL_MEDIA_FORMAT_DECLARE(OpalGSM0610Format,
          OpalGSM0610,
          OpalMediaFormat::DefaultAudioSessionID,
          RTP_DataFrame::GSM,
          TRUE,  // Needs jitter
          13200, // bits/sec
          33,    // bytes
          20,   // 20 milliseconds
          OpalMediaFormat::AudioTimeUnits,
          0)

/////////////////////////////////////////////////////////////////////////////

char OpalT120[] = "T.120";

OPAL_MEDIA_FORMAT_DECLARE(OpalT120Format,
          OpalT120,
          OpalMediaFormat::DefaultDataSessionID,
          RTP_DataFrame::IllegalPayloadType,
          FALSE,   // No jitter for data
          825000,   // 100's bits/sec
          0,0,0,0)

/////////////////////////////////////////////////////////////////////////////

OpalMediaOption::OpalMediaOption(const char * name, bool readOnly, MergeType merge)
  : m_name(name),
    m_readOnly(readOnly),
    m_merge(merge)
{
  m_name.Replace("=", "_", TRUE);
  memset(&m_H245Generic, 0, sizeof(m_H245Generic));
}


PObject::Comparison OpalMediaOption::Compare(const PObject & obj) const
{
  const OpalMediaOption * otherOption = PDownCast(const OpalMediaOption, &obj);
  if (otherOption == NULL)
    return GreaterThan;
  return m_name.Compare(otherOption->m_name);
}


bool OpalMediaOption::Merge(const OpalMediaOption & option)
{
  // Local Generic Parameters that are default zero are place holders and you merge
  // to the value of the other party (option is local, this is remote)
  if ((option.GetH245Generic().mode != OpalMediaOption::H245GenericInfo::None) &&
      PIsDescendant(&option, OpalMediaOptionUnsigned)) {
      if (((const OpalMediaOptionUnsigned &)option).GetValue() == 0) 
             return true;
  }

  switch (m_merge) {
    case MinMerge :
      if (CompareValue(option) == GreaterThan)
        Assign(option);
      break;

    case MaxMerge :
      if (CompareValue(option) == LessThan)
        Assign(option);
      break;

    case EqualMerge :
      return CompareValue(option) == EqualTo;

    case NotEqualMerge :
      return CompareValue(option) != EqualTo;
      
    case AlwaysMerge :
      Assign(option);
      break;

    default :
      break;
  }

  return true;
}


PString OpalMediaOption::AsString() const
{
  PStringStream strm;
  PrintOn(strm);
  return strm;
}


bool OpalMediaOption::FromString(const PString & value)
{
  PStringStream strm;
  strm = value;
  ReadFrom(strm);
  return !strm.fail();
}


OpalMediaOptionEnum::OpalMediaOptionEnum(const char * name,
                                         bool readOnly,
                                         const char * const * enumerations,
                                         PINDEX count,
                                         MergeType merge,
                                         PINDEX value)
  : OpalMediaOption(name, readOnly, merge),
    m_enumerations(count, enumerations),
    m_value(value)
{
  if (m_value >= count)
    m_value = count;
}


PObject * OpalMediaOptionEnum::Clone() const
{
  return new OpalMediaOptionEnum(*this);
}


void OpalMediaOptionEnum::PrintOn(ostream & strm) const
{
  if (m_value < m_enumerations.GetSize())
    strm << m_enumerations[m_value];
  else
    strm << m_value;
}


void OpalMediaOptionEnum::ReadFrom(istream & strm)
{
  PCaselessString str;
  while (strm.good()) {
    char ch;
    strm.get(ch);
    str += ch;
    for (PINDEX i = 0; i < m_enumerations.GetSize(); i++) {
      if (str == m_enumerations[i]) {
        m_value = i;
        return;
      }
    }
  }

  m_value = m_enumerations.GetSize();

#if PTLIB_VER >= 2110 || defined(__USE_STL__)
   strm.setstate(ios::badbit);
#else
   strm.setf(ios::badbit , ios::badbit);
#endif
}


PObject::Comparison OpalMediaOptionEnum::CompareValue(const OpalMediaOption & option) const
{
  const OpalMediaOptionEnum * otherOption = PDownCast(const OpalMediaOptionEnum, &option);
  if (otherOption == NULL)
    return GreaterThan;

  if (m_value > otherOption->m_value)
    return GreaterThan;

  if (m_value < otherOption->m_value)
    return LessThan;

  return EqualTo;
}


void OpalMediaOptionEnum::Assign(const OpalMediaOption & option)
{
  const OpalMediaOptionEnum * otherOption = PDownCast(const OpalMediaOptionEnum, &option);
  if (otherOption != NULL)
    m_value = otherOption->m_value;
}


void OpalMediaOptionEnum::SetValue(PINDEX value)
{
  if (value < m_enumerations.GetSize())
    m_value = value;
  else
    m_value = m_enumerations.GetSize();
}


OpalMediaOptionString::OpalMediaOptionString(const char * name, bool readOnly)
  : OpalMediaOption(name, readOnly, MinMerge)
{
}


OpalMediaOptionString::OpalMediaOptionString(const char * name, bool readOnly, const PString & value)
  : OpalMediaOption(name, readOnly, MinMerge),
    m_value(value)
{
}


PObject * OpalMediaOptionString::Clone() const
{
  OpalMediaOptionString * newObj = new OpalMediaOptionString(*this);
  newObj->m_value.MakeUnique();
  return newObj;
}


void OpalMediaOptionString::PrintOn(ostream & strm) const
{
  strm << m_value.ToLiteral();
}


void OpalMediaOptionString::ReadFrom(istream & strm)
{
  char c;
  strm >> c; // Skip whitespace

  if (c != '"') {
    strm.putback(c);
    strm >> m_value; // If no " then read to end of line.
  }
  else {
    // If there was a '"' then assume it is a C style literal string with \ escapes etc

    PINDEX count = 0;
    PStringStream str;
    str << '"';

    while (strm.get(c).good()) {
      str << c;

      // Keep reading till get a '"' that is not preceded by a '\' that is not itself preceded by a '\'
      if (c == '"' && count > 0 && (str[count] != '\\' || !(count > 1 && str[count-1] == '\\')))
        break;

      count++;
    }

    m_value = PString(PString::Literal, (const char *)str);
  }
}


PObject::Comparison OpalMediaOptionString::CompareValue(const OpalMediaOption & option) const
{
  const OpalMediaOptionString * otherOption = PDownCast(const OpalMediaOptionString, &option);
  if (otherOption == NULL)
    return GreaterThan;

  return m_value.Compare(otherOption->m_value);
}


void OpalMediaOptionString::Assign(const OpalMediaOption & option)
{
  const OpalMediaOptionString * otherOption = PDownCast(const OpalMediaOptionString, &option);
  if (otherOption != NULL) {
    m_value = otherOption->m_value;
    m_value.MakeUnique();
  }
}


void OpalMediaOptionString::SetValue(const PString & value)
{
  m_value = value;
  m_value.MakeUnique();
}


OpalMediaOptionOctets::OpalMediaOptionOctets(const char * name, bool readOnly, bool base64)
  : OpalMediaOption(name, readOnly, NoMerge)
  , m_base64(base64)
{
}


OpalMediaOptionOctets::OpalMediaOptionOctets(const char * name, bool readOnly, bool base64, const PBYTEArray & value)
  : OpalMediaOption(name, readOnly, NoMerge)
  , m_value(value)
  , m_base64(base64)
{
}


OpalMediaOptionOctets::OpalMediaOptionOctets(const char * name, bool readOnly, bool base64, const BYTE * data, PINDEX length)
  : OpalMediaOption(name, readOnly, NoMerge)
  , m_value(data, length)
  , m_base64(base64)
{
}


PObject * OpalMediaOptionOctets::Clone() const
{
  OpalMediaOptionOctets * newObj = new OpalMediaOptionOctets(*this);
  newObj->m_value.MakeUnique();
  return newObj;
}


void OpalMediaOptionOctets::PrintOn(ostream & strm) const
{
  if (m_base64)
    strm << PBase64::Encode(m_value);
  else {
    _Ios_Fmtflags flags = strm.flags();
    char fill = strm.fill();

    strm << hex << setfill('0');
    for (PINDEX i = 0; i < m_value.GetSize(); i++)
      strm << setw(2) << (unsigned)m_value[i];

    strm.fill(fill);
    strm.flags(flags);
  }
}


void OpalMediaOptionOctets::ReadFrom(istream & strm)
{
  if (m_base64) {
    PString str;
    strm >> str;
    PBase64::Decode(str, m_value);
  }
  else {
    char pair[3];
    pair[2] = '\0';

    PINDEX count = 0;

    while (isxdigit(strm.peek())) {
      pair[0] = (char)strm.get();
      if (!isxdigit(strm.peek())) {
        strm.putback(pair[0]);
        break;
      }
      pair[1] = (char)strm.get();
      if (!m_value.SetMinSize((count+1+99)%100))
        break;
      m_value[count++] = (BYTE)strtoul(pair, NULL, 16);
    }

    m_value.SetSize(count);
  }
}


PObject::Comparison OpalMediaOptionOctets::CompareValue(const OpalMediaOption & option) const
{
  const OpalMediaOptionOctets * otherOption = PDownCast(const OpalMediaOptionOctets, &option);
  if (otherOption == NULL)
    return GreaterThan;

  return m_value.Compare(otherOption->m_value);
}


void OpalMediaOptionOctets::Assign(const OpalMediaOption & option)
{
  const OpalMediaOptionOctets * otherOption = PDownCast(const OpalMediaOptionOctets, &option);
  if (otherOption != NULL) {
    m_value = otherOption->m_value;
    m_value.MakeUnique();
  }
}


void OpalMediaOptionOctets::SetValue(const PBYTEArray & value)
{
  m_value = value;
  m_value.MakeUnique();
}


void OpalMediaOptionOctets::SetValue(const BYTE * data, PINDEX length)
{
  m_value = PBYTEArray(data, length);
}


/////////////////////////////////////////////////////////////////////////////

OpalMediaFormat::OpalMediaFormat()
{
  rtpPayloadType = RTP_DataFrame::IllegalPayloadType;

  needsJitter = FALSE;
  bandwidth = 0;
  frameSize = 0;
  frameTime = 0;
  timeUnits = 0;
  codecBaseTime = 0;
  defaultSessionID = NonRTPSessionID;
}


OpalMediaFormat::OpalMediaFormat(const char * search, PBoolean exact)
{
  rtpPayloadType = RTP_DataFrame::IllegalPayloadType;

  needsJitter = FALSE;
  bandwidth = 0;
  frameSize = 0;
  frameTime = 0;
  timeUnits = 0;
  codecBaseTime = 0;
  defaultSessionID = NonRTPSessionID; 

  // look for the media type in the factory. 
  // Don't make a copy of the list - lock the list and use the raw data
  if (exact) {
    OpalMediaFormat * registeredFormat = OpalMediaFormatFactory::CreateInstance(search);
    if (registeredFormat != NULL)
      *this = *registeredFormat;
  }
  else {
    PWaitAndSignal m(OpalMediaFormatFactory::GetMutex());
    OpalMediaFormatFactory::KeyMap_T & keyMap = OpalMediaFormatFactory::GetKeyMap();
    OpalMediaFormatFactory::KeyMap_T::const_iterator r;
    for (r = keyMap.begin(); r != keyMap.end(); ++r) {
      if (r->first.find(search) != std::string::npos) {
        *this = *OpalMediaFormatFactory::CreateInstance(r->first);
        break;
      }
    }
  }
}


OpalMediaFormat::OpalMediaFormat(const char * fullName,
                                 unsigned dsid,
                                 RTP_DataFrame::PayloadTypes pt,
                                 PBoolean     nj,
                                 unsigned bw,
                                 PINDEX   fs,
                                 unsigned ft,
                                 unsigned tu,
                                 time_t ts)
  : PCaselessString(fullName)
{
  rtpPayloadType = pt;
  defaultSessionID = dsid;
  needsJitter = nj;
  bandwidth = bw;
  frameSize = fs;
  frameTime = ft;
  timeUnits = tu;
  codecBaseTime = ts;

  // assume non-dynamic payload types are correct and do not need deconflicting
  if (rtpPayloadType < RTP_DataFrame::DynamicBase || rtpPayloadType == RTP_DataFrame::IllegalPayloadType)
    return;

  // find the next unused dynamic number, and find anything with the new 
  // rtp payload type if it is explicitly required
  PWaitAndSignal m(OpalMediaFormatFactory::GetMutex());
  OpalMediaFormatFactory::KeyMap_T & keyMap = OpalMediaFormatFactory::GetKeyMap();

  OpalMediaFormat * match = NULL;
  RTP_DataFrame::PayloadTypes nextUnused = RTP_DataFrame::DynamicBase;
  OpalMediaFormatFactory::KeyMap_T::iterator r;

  do {
    for (r = keyMap.begin(); r != keyMap.end(); ++r) {
      if (r->first == fullName)
        continue;
      OpalMediaFormat & fmt = *OpalMediaFormatFactory::CreateInstance(r->first);
      if (fmt.GetPayloadType() == nextUnused) {
        nextUnused = (RTP_DataFrame::PayloadTypes)(nextUnused + 1);
        break; // restart the search
      }
      if (fmt.GetPayloadType() == rtpPayloadType)
        match = &fmt;
    }
  } while (r != keyMap.end());

  // If we found a match to the payload type, then it needs to be deconflicted
  // If the new format is just requesting any dynamic payload number, then give it the next unused one
  // If it is requesting a specific code (like RFC 2833) then renumber the old format. This could be dangerous
  // as media formats could be created when there is a session in use with the old media format and payload type
  // For this reason, any media formats that require specific dynamic codes should be created before any calls are made
  if (match != NULL) {
    if (rtpPayloadType == RTP_DataFrame::DynamicBase)
      rtpPayloadType = nextUnused;
    else 
      match->rtpPayloadType = nextUnused;
  }
}

OpalMediaFormat & OpalMediaFormat::operator=(const OpalMediaFormat &format)
{
  PWaitAndSignal m1(media_format_mutex);
  PWaitAndSignal m2(format.media_format_mutex);
  *static_cast<PCaselessString *>(this) = *static_cast<const PCaselessString *>(&format);
  options = format.options;
  options.MakeUnique();
  rtpPayloadType = format.rtpPayloadType;
  defaultSessionID = format.defaultSessionID;
  needsJitter = format.NeedsJitterBuffer();
  bandwidth = format.GetBandwidth();
  frameSize = format.GetFrameSize();
  frameTime = format.GetFrameTime();
  timeUnits = format.GetTimeUnits();
  return *this;  
}


void OpalMediaFormat::GetRegisteredMediaFormats(OpalMediaFormat::List & list)
{
  list.DisallowDeleteObjects();
  PWaitAndSignal m(OpalMediaFormatFactory::GetMutex());
  OpalMediaFormatFactory::KeyMap_T & keyMap = OpalMediaFormatFactory::GetKeyMap();
  OpalMediaFormatFactory::KeyMap_T::const_iterator r;
  for (r = keyMap.begin(); r != keyMap.end(); ++r)
    list.Append(OpalMediaFormatFactory::CreateInstance(r->first));
}


OpalMediaFormat::List OpalMediaFormat::GetRegisteredMediaFormats()
{
  OpalMediaFormat::List list;
  GetRegisteredMediaFormats(list);
  return list;
}


bool OpalMediaFormat::Merge(const OpalMediaFormat & mediaFormat)
{
  PWaitAndSignal m1(media_format_mutex);
  PWaitAndSignal m2(mediaFormat.media_format_mutex);
  for (PINDEX i = 0; i < options.GetSize(); i++) {
    OpalMediaOption * option = mediaFormat.FindOption(options[i].GetName());
    if (option != NULL && !options[i].Merge(*option))
      return false;
  }

  return true;
}


bool OpalMediaFormat::GetOptionValue(const PString & name, PString & value) const
{
  PWaitAndSignal m(media_format_mutex);
  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return false;

  value = option->AsString();
  return true;
}


bool OpalMediaFormat::SetOptionValue(const PString & name, const PString & value)
{
  PWaitAndSignal m(media_format_mutex);
  options.MakeUnique();

  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return false;

  return option->FromString(value);
}


bool OpalMediaFormat::GetOptionBoolean(const PString & name, bool dflt) const
{
  PWaitAndSignal m(media_format_mutex);
  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return dflt;

  return PDownCast(OpalMediaOptionBoolean, option)->GetValue();
}


bool OpalMediaFormat::SetOptionBoolean(const PString & name, bool value)
{
  PWaitAndSignal m(media_format_mutex);
  options.MakeUnique();

  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return false;

  PDownCast(OpalMediaOptionBoolean, option)->SetValue(value);
  return true;
}


int OpalMediaFormat::GetOptionInteger(const PString & name, int dflt) const
{
  PWaitAndSignal m(media_format_mutex);
  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return dflt;

  OpalMediaOptionUnsigned * optUnsigned = dynamic_cast<OpalMediaOptionUnsigned *>(option);
  if (optUnsigned != NULL)
    return optUnsigned->GetValue();

  OpalMediaOptionInteger * optInteger = dynamic_cast<OpalMediaOptionInteger *>(option);
  if (optInteger != NULL)
    return optInteger->GetValue();

  return 0;
}


bool OpalMediaFormat::SetOptionInteger(const PString & name, int value)
{
  PWaitAndSignal m(media_format_mutex);
  options.MakeUnique();

  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return false;

  OpalMediaOptionUnsigned * optUnsigned = dynamic_cast<OpalMediaOptionUnsigned *>(option);
  if (optUnsigned != NULL) {
    optUnsigned->SetValue(value);
    return true;
  }

  OpalMediaOptionInteger * optInteger = dynamic_cast<OpalMediaOptionInteger *>(option);
  if (optInteger != NULL) {
    optInteger->SetValue(value);
    return true;
  }

  return false;
}


double OpalMediaFormat::GetOptionReal(const PString & name, double dflt) const
{
  PWaitAndSignal m(media_format_mutex);
  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return dflt;

  return PDownCast(OpalMediaOptionReal, option)->GetValue();
}


bool OpalMediaFormat::SetOptionReal(const PString & name, double value)
{
  PWaitAndSignal m(media_format_mutex);
  options.MakeUnique();

  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return false;

  PDownCast(OpalMediaOptionReal, option)->SetValue(value);
  return true;
}


PINDEX OpalMediaFormat::GetOptionEnum(const PString & name, PINDEX dflt) const
{
  PWaitAndSignal m(media_format_mutex);
  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return dflt;

  return PDownCast(OpalMediaOptionEnum, option)->GetValue();
}


bool OpalMediaFormat::SetOptionEnum(const PString & name, PINDEX value)
{
  PWaitAndSignal m(media_format_mutex);
  options.MakeUnique();

  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return false;

  PDownCast(OpalMediaOptionEnum, option)->SetValue(value);
  return true;
}


PString OpalMediaFormat::GetOptionString(const PString & name, const PString & dflt) const
{
  PWaitAndSignal m(media_format_mutex);
  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return dflt;

  return PDownCast(OpalMediaOptionString, option)->GetValue();
}


bool OpalMediaFormat::SetOptionString(const PString & name, const PString & value)
{
  PWaitAndSignal m(media_format_mutex);
  options.MakeUnique();

  OpalMediaOption * option = FindOption(name);
  if (option == NULL)
    return false;

  PDownCast(OpalMediaOptionString, option)->SetValue(value);
  return true;
}


bool OpalMediaFormat::AddOption(OpalMediaOption * option, PBoolean overwrite)
{
  PWaitAndSignal m(media_format_mutex);
  if (PAssertNULL(option) == NULL)
    return false;

  PINDEX index = options.GetValuesIndex(*option);
  if (index != P_MAX_INDEX) {
    if (!overwrite) {
      delete option;
      return false;
    }

    options.RemoveAt(index);
  }

  options.MakeUnique();
  options.Append(option);
  return true;
}


OpalMediaOption * OpalMediaFormat::FindOption(const PString & name) const
{
  PWaitAndSignal m(media_format_mutex);
  OpalMediaOptionString search(name, false);
  PINDEX index = options.GetValuesIndex(search);
  if (index == P_MAX_INDEX)
    return NULL;

  return &options[index];
}


bool OpalMediaFormat::SetRegisteredMediaFormat(const OpalMediaFormat & mediaFormat)
{
  PWaitAndSignal mutex(OpalMediaFormatFactory::GetMutex());
  OpalMediaFormat * registeredFormat = OpalMediaFormatFactory::CreateInstance(mediaFormat);
  if (registeredFormat == NULL)
    return false;

  *registeredFormat = mediaFormat;
  return true;
}


#if PTRACING
void OpalMediaFormat::DebugOptionList(const OpalMediaFormat & fmt)
{

  PStringStream traceStream;
  traceStream << "         " << fmt.GetOptionCount() << " options found:\n";
  for (PINDEX i = 0; i < fmt.GetOptionCount(); i++) {
    const OpalMediaOption & option = fmt.GetOption(i);
    traceStream << "         " << option.GetName() << " = " << option.AsString() << '\n';
  }
#if PTLIB_VER < 2110
  PTRACE(6,traceStream);
#else
  PTRACE2(6,NULL,traceStream);
#endif
}
#endif

#ifdef H323_VIDEO

const char * const OpalVideoFormat::FrameWidthOption = "Frame Width";
const char * const OpalVideoFormat::FrameHeightOption = "Frame Height";
const char * const OpalVideoFormat::EncodingQualityOption = "Encoding Quality";
const char * const OpalVideoFormat::TargetBitRateOption = "Target Bit Rate";
const char * const OpalVideoFormat::DynamicVideoQualityOption = "Dynamic Video Quality";
const char * const OpalVideoFormat::AdaptivePacketDelayOption = "Adaptive Packet Delay";

const char * const OpalVideoFormat::NeedsJitterOption = "Needs Jitter";
const char * const OpalVideoFormat::MaxBitRateOption = "Max Bit Rate";
const char * const OpalVideoFormat::MaxFrameSizeOption = "Max Frame Size";
const char * const OpalVideoFormat::FrameTimeOption = "Frame Time";
const char * const OpalVideoFormat::ClockRateOption = "Clock Rate";

OpalVideoFormat::OpalVideoFormat(const char * fullName,
                                 RTP_DataFrame::PayloadTypes rtpPayloadType,
                                 unsigned /*frameWidth*/,
                                 unsigned /*frameHeight*/,
                                 unsigned frameRate,
                                 unsigned bitRate,
                                   time_t timeStamp)
  : OpalMediaFormat(fullName,
                    OpalMediaFormat::DefaultVideoSessionID,
                    rtpPayloadType,
                    FALSE,
                    bitRate,
                    0,
                    OpalMediaFormat::VideoTimeUnits/frameRate,
                    OpalMediaFormat::VideoTimeUnits,
                    timeStamp)
{

}

PObject * OpalVideoFormat::Clone() const
{
  return new OpalVideoFormat(*this);
}


bool OpalVideoFormat::Merge(const OpalMediaFormat & mediaFormat)
{
    return OpalMediaFormat::Merge(mediaFormat);
}

#endif // H323_VIDEO


// End of File ///////////////////////////////////////////////////////////////
