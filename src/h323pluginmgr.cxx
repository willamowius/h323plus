/*
 * h323pluginmgr.cxx
 *
 * H.323 codec plugins handler
 *
 * Open H323 Library
 *
 * Copyright (C) 2004 Post Increment
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
 * The Initial Developer of the Original Code is Post Increment
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifdef __GNUC__
#pragma implementation "h323pluginmgr.h"
#endif

#include <ptlib.h>
#include <ptlib/video.h>
#include <h323.h>
#define _FACTORY_LOAD  1
#include <h323pluginmgr.h>
#include <codec/opalplugin.h>
#include <opalwavfile.h>
#include <h323caps.h>
#include <h245.h>
#include <rtp.h>
#include <mediafmt.h>
#include <openh323buildopts.h>

#define H323CAP_TAG_PREFIX    "h323"
static const char GET_CODEC_OPTIONS_CONTROL[]       = "get_codec_options";
static const char FREE_CODEC_OPTIONS_CONTROL[]      = "free_codec_options";
static const char GET_OUTPUT_DATA_SIZE_CONTROL[]    = "get_output_data_size";
static const char SET_CODEC_OPTIONS_CONTROL[]       = "set_codec_options";
static const char SET_CODEC_CUSTOMISED_OPTIONS[]    = "to_customised_options";
static const char SET_CODEC_FLOWCONTROL_OPTIONS[]   = "to_flowcontrol_options";
static const char EVENT_CODEC_CONTROL[]             = "event_codec";
static const char SET_CODEC_FORMAT_OPTIONS[]        = "set_format_options";

#ifdef H323_VIDEO

#define CIF_WIDTH         352
#define CIF_HEIGHT        288

#define CIF4_WIDTH        (CIF_WIDTH*2)
#define CIF4_HEIGHT       (CIF_HEIGHT*2)

#define CIF16_WIDTH       (CIF_WIDTH*4)
#define CIF16_HEIGHT      (CIF_HEIGHT*4)

#define QCIF_WIDTH        (CIF_WIDTH/2)
#define QCIF_HEIGHT       (CIF_HEIGHT/2)

#define SQCIF_WIDTH       128
#define SQCIF_HEIGHT      96


static const char * sqcifMPI_tag                          = "SQCIF MPI";
static const char * qcifMPI_tag                           = "QCIF MPI";
static const char * cifMPI_tag                            = "CIF MPI";
static const char * cif4MPI_tag                           = "CIF4 MPI";
static const char * cif16MPI_tag                          = "CIF16 MPI";

// H.261 only
static const char * h323_stillImageTransmission_tag            = H323CAP_TAG_PREFIX "_stillImageTransmission";

// H.261/H.263/H.264 tags
static const char * h323_qcifMPI_tag                           = H323CAP_TAG_PREFIX "_qcifMPI";
static const char * h323_cifMPI_tag                            = H323CAP_TAG_PREFIX "_cifMPI";

// H.263/H.264 tags
static const char * h323_sqcifMPI_tag                          = H323CAP_TAG_PREFIX "_sqcifMPI";
static const char * h323_cif4MPI_tag                           = H323CAP_TAG_PREFIX "_cif4MPI";
static const char * h323_cif16MPI_tag                          = H323CAP_TAG_PREFIX "_cif16MPI";
//static const char * h323_slowSqcifMPI_tag                      = H323CAP_TAG_PREFIX "_slowSqcifMPI";
//static const char * h323_slowQcifMPI_tag                       = H323CAP_TAG_PREFIX "_slowQcifMPI";
//static const char * h323_slowCifMPI_tag                        = H323CAP_TAG_PREFIX "slowCifMPI";
//static const char * h323_slowCif4MPI_tag                       = H323CAP_TAG_PREFIX "_slowCif4MPI";
//static const char * h323_slowCif16MPI_tag                      = H323CAP_TAG_PREFIX "_slowCif16MPI";

// H.263 only
static const char * h323_temporalSpatialTradeOffCapability_tag = H323CAP_TAG_PREFIX "_temporalSpatialTradeOffCapability";
static const char * h323_unrestrictedVector_tag                = H323CAP_TAG_PREFIX "_unrestrictedVector";
static const char * h323_arithmeticCoding_tag                  = H323CAP_TAG_PREFIX "_arithmeticCoding";
static const char * h323_advancedPrediction_tag                = H323CAP_TAG_PREFIX "_advancedPrediction";
static const char * h323_pbFrames_tag                          = H323CAP_TAG_PREFIX "_pbFrames";
static const char * h323_hrdB_tag                              = H323CAP_TAG_PREFIX "_hrdB";
static const char * h323_bppMaxKb_tag                          = H323CAP_TAG_PREFIX "_bppMaxKb";
static const char * h323_errorCompensation_tag                 = H323CAP_TAG_PREFIX "_errorCompensation";
static const char * h323_advancedIntra_tag                     = H323CAP_TAG_PREFIX "_advancedIntra";
static const char * h323_modifiedQuantization_tag              = H323CAP_TAG_PREFIX "_modifiedQuantization";

inline static bool IsValidMPI(int mpi) {
  return (mpi > 0) && (mpi < 5);
}

#define FASTPICTUREINTERVAL  1000

#endif // H323_VIDEO

typedef PFactory<OpalFactoryCodec, PString> OpalPluginCodecFactory;

class OpalPluginCodec : public OpalFactoryCodec {
  PCLASSINFO(OpalPluginCodec, PObject)
  public:
    OpalPluginCodec(PluginCodec_Definition * _codecDefn)
      : codecDefn(_codecDefn)
    {
      if (codecDefn->createCodec == NULL)
        context = NULL;
      else
        context = (codecDefn->createCodec)(codecDefn);
    }

    ~OpalPluginCodec()
    {
      (codecDefn->destroyCodec)(codecDefn, context);
    }

    const struct PluginCodec_Definition * GetDefinition()
    { return codecDefn; }

    PString GetInputFormat() const
    { return codecDefn->sourceFormat; }

    PString GetOutputFormat() const
    { return codecDefn->destFormat; }

    int Encode(const void * from, unsigned * fromLen, void * to,   unsigned * toLen, unsigned int * flag)
    { return (*codecDefn->codecFunction)(codecDefn, context, from, fromLen, to, toLen, flag); }

    unsigned int GetSampleRate() const
    { return codecDefn->sampleRate; }

    unsigned int GetBitsPerSec() const
    { return codecDefn->bitsPerSec; }

    unsigned int GetFrameTime() const
    { return codecDefn->usPerFrame; }

    unsigned int GetSamplesPerFrame() const
    { return codecDefn->parm.audio.samplesPerFrame; }

    unsigned int GetBytesPerFrame() const
    { return codecDefn->parm.audio.bytesPerFrame; }

    unsigned int GetRecommendedFramesPerPacket() const
    { return codecDefn->parm.audio.recommendedFramesPerPacket; }

    unsigned int GetMaxFramesPerPacket() const
    { return codecDefn->parm.audio.maxFramesPerPacket; }

    BYTE GetRTPPayload() const
    { return (BYTE)codecDefn->rtpPayload; }

    PString GetSDPFormat() const
    { return codecDefn->sampleRate; }

    bool SetCustomFormat(unsigned frameWidth, unsigned frameHeight, unsigned frameRate);

    bool SetCustomFormat(unsigned bitrate, unsigned samplerate);

#ifdef H323_VIDEO
    /** Set Media Format */
    bool SetMediaFormat(OpalMediaFormat & fmt);

    /** Update Media Options */
    bool UpdateMediaOptions(OpalMediaFormat & fmt);

    /** codec control */
    bool CodecControl(const char * name, void * parm, unsigned int * parmLen, int & retVal);
#endif // H323_VIDEO

  protected:
    PluginCodec_Definition * codecDefn;
    void * context;
};

/////////////////////////////////////////////////////////////////////////////

#ifdef H323_AUDIO_CODECS

extern "C" {
  unsigned char linear2ulaw(int pcm_val);
  int ulaw2linear(unsigned char u_val);
  unsigned char linear2alaw(int pcm_val);
  int alaw2linear(unsigned char u_val);
};

#define DECLARE_FIXED_CODEC(name, format, bps, frameTime, samples, bytes, fpp, maxfpp, payload, sdp) \
class name##_Base : public OpalFactoryCodec { \
  PCLASSINFO(name##_Base, OpalFactoryCodec) \
  public: \
    name##_Base() \
    { } \
    unsigned int GetSampleRate() const                 { return 8000; } \
    unsigned int GetBitsPerSec() const                 { return bps; } \
    unsigned int GetFrameTime() const                  { return frameTime; } \
    unsigned int GetSamplesPerFrame() const            { return samples; } \
    unsigned int GetBytesPerFrame() const              { return bytes; } \
    unsigned int GetRecommendedFramesPerPacket() const { return fpp; } \
    unsigned int GetMaxFramesPerPacket() const         { return maxfpp; } \
    BYTE GetRTPPayload() const                         { return payload; } \
    PString GetSDPFormat() const                       { return sdp; } \
}; \
class name##_Encoder : public name##_Base { \
  PCLASSINFO(name##_Encoder, name##_Base) \
  public: \
    name##_Encoder() \
    { } \
    virtual PString GetInputFormat() const \
    { return format; }  \
    virtual PString GetOutputFormat() const \
    { return "L16"; }  \
    static PString GetFactoryName() \
    { return PString("L16") + "|" + format; } \
    int Encode(const void * from, unsigned * fromLen, void * to,   unsigned * toLen, unsigned int * flag); \
}; \
class name##_Decoder : public name##_Base { \
PCLASSINFO(name##_Decoder, name##_Base) \
  public: \
    name##_Decoder() \
    { } \
    virtual PString GetInputFormat() const \
    { return "L16"; }  \
    virtual PString GetOutputFormat() const \
    { return format; } \
    static PString GetFactoryName() \
    { return PString(format) + "|" + "L16"; } \
    int Encode(const void * from, unsigned * fromLen, void * to,   unsigned * toLen, unsigned int * flag); \
}; \

DECLARE_FIXED_CODEC(OpalG711ALaw64k, OpalG711ALaw64k, 64000, 30000, 240, 240, 30, 30, RTP_DataFrame::PCMA, "PCMA")

int OpalG711ALaw64k_Encoder::Encode(const void * _from, unsigned * fromLen, void * _to,   unsigned * toLen, unsigned int * )
{
  if (*fromLen/2 > *toLen)
    return 0;

  const short * from = (short *)_from;
  BYTE * to          = (BYTE *)_to;

  unsigned count = *fromLen / 2;
  *toLen         = count;

  while (count-- > 0)
    *to++ = linear2alaw(*from++);

  return 1;
}

int OpalG711ALaw64k_Decoder::Encode(const void * _from, unsigned * fromLen, void * _to,   unsigned * toLen, unsigned int * )
{
  if (*fromLen*2 > *toLen)
    return 0;

  const BYTE * from = (BYTE *)_from;
  short * to        = (short *)_to;

  unsigned count = *fromLen;
  *toLen         = count * 2;

  while (count-- > 0)
    *to++ = (short)alaw2linear(*from++);

  return 1;
}

DECLARE_FIXED_CODEC(OpalG711ALaw64k20, OpalG711ALaw64k20, 64000, 20000, 160, 160, 20, 20, RTP_DataFrame::PCMU, "PCMA")

int OpalG711ALaw64k20_Encoder::Encode(const void * _from, unsigned * fromLen, void * _to,   unsigned * toLen, unsigned int * )
{
  if (*fromLen/2 > *toLen)
    return 0;

  const short * from = (short *)_from;
  BYTE * to          = (BYTE *)_to;

  unsigned count = *fromLen / 2;
  *toLen         = count;

  while (count-- > 0)
    *to++ = linear2alaw(*from++);

  return 1;
}

int OpalG711ALaw64k20_Decoder::Encode(const void * _from, unsigned * fromLen, void * _to,   unsigned * toLen, unsigned int * )
{
  if (*fromLen*2 > *toLen)
    return 0;

  const BYTE * from = (BYTE *)_from;
  short * to        = (short *)_to;

  unsigned count = *fromLen;
  *toLen         = count * 2;

  while (count-- > 0)
    *to++ = (short)alaw2linear(*from++);

  return 1;
}

DECLARE_FIXED_CODEC(OpalG711uLaw64k, OpalG711uLaw64k, 64000, 30000, 240, 240, 30, 30, RTP_DataFrame::PCMU, "PCMU")

int OpalG711uLaw64k_Encoder::Encode(const void * _from, unsigned * fromLen, void * _to,   unsigned * toLen, unsigned int * )
{
  if (*fromLen/2 > *toLen)
    return 0;

  const short * from = (short *)_from;
  BYTE * to          = (BYTE *)_to;

  unsigned count = *fromLen / 2;
  *toLen         = count;

  while (count-- > 0)
    *to++ = linear2ulaw(*from++);

  return 1;
}

int OpalG711uLaw64k_Decoder::Encode(const void * _from, unsigned * fromLen, void * _to,   unsigned * toLen, unsigned int * )
{
  if (*fromLen*2 > *toLen)
    return 0;

  const BYTE * from = (BYTE *)_from;
  short * to        = (short *)_to;

  unsigned count = *fromLen;
  *toLen         = count * 2;

  while (count-- > 0)
    *to++ = (short)ulaw2linear(*from++);

  return 1;
}

DECLARE_FIXED_CODEC(OpalG711uLaw64k20, OpalG711uLaw64k20, 64000, 20000, 160, 160, 20, 20, RTP_DataFrame::PCMU, "PCMU")

int OpalG711uLaw64k20_Encoder::Encode(const void * _from, unsigned * fromLen, void * _to,   unsigned * toLen, unsigned int * )
{
  if (*fromLen/2 > *toLen)
    return 0;

  const short * from = (short *)_from;
  BYTE * to          = (BYTE *)_to;

  unsigned count = *fromLen / 2;
  *toLen         = count;

  while (count-- > 0)
    *to++ = linear2ulaw(*from++);

  return 1;
}

int OpalG711uLaw64k20_Decoder::Encode(const void * _from, unsigned * fromLen, void * _to,   unsigned * toLen, unsigned int * )
{
  if (*fromLen*2 > *toLen)
    return 0;

  const BYTE * from = (BYTE *)_from;
  short * to        = (short *)_to;

  unsigned count = *fromLen;
  *toLen         = count * 2;

  while (count-- > 0)
    *to++ = (short)ulaw2linear(*from++);

  return 1;
}


#endif // NO_H323_AUDIO_CODECS

//////////////////////////////////////////////////////////////////////////////
//
// Helper functions for codec control operators
//

static PluginCodec_ControlDefn * GetCodecControl(const PluginCodec_Definition * codec, const char * name)
{
  PluginCodec_ControlDefn * codecControls = codec->codecControls;
  if (codecControls == NULL)
    return NULL;

  while (codecControls->name != NULL) {
    if (strcasecmp(codecControls->name, name) == 0)
      return codecControls;
    codecControls++;
  }

  return NULL;
}

static bool SetCustomisedOptions(const PluginCodec_Definition * codec,
                                 void * context,
                                 unsigned frameWidth,
                                 unsigned frameHeight,
                                 unsigned frameRate)
{
    if (context == NULL)
        return false;

    PStringArray list;
    list.AppendString(PLUGINCODEC_OPTION_FRAME_WIDTH);
    list.AppendString(frameWidth);
    list.AppendString(PLUGINCODEC_OPTION_FRAME_HEIGHT);
    list.AppendString(frameHeight);
    list.AppendString(PLUGINCODEC_OPTION_FRAME_TIME);
    list.AppendString(OpalMediaFormat::VideoTimeUnits * 1000 * 100 * frameRate / 2997);

    char ** _options = list.ToCharArray();
    unsigned int optionsLen = sizeof(_options);
    PluginCodec_ControlDefn * ctl = GetCodecControl(codec, SET_CODEC_CUSTOMISED_OPTIONS);
    if (ctl != NULL) {
        (*ctl->control)(codec, context ,SET_CODEC_CUSTOMISED_OPTIONS, _options, &optionsLen);
        free(_options);
        return true;
    }
    free(_options);
    return false;
}

static bool SetCustomisedOptions(const PluginCodec_Definition * codec,
                                 void * context,
                                 unsigned bitRate,
                                 unsigned sampleRate)
{
    if (context == NULL)
      return false;

    PStringArray list;
    if (bitRate > 0) {
        list.AppendString(PLUGINCODEC_OPTION_MAX_BIT_RATE);
        list.AppendString(bitRate);
    }
    if (sampleRate > 0) {
        list.AppendString(PLUGINCODEC_OPTION_MAX_FRAME_SIZE);
        list.AppendString(sampleRate);
    }

    char ** _options = list.ToCharArray();
    unsigned int optionsLen = sizeof(_options);
    PluginCodec_ControlDefn * ctl = GetCodecControl(codec, SET_CODEC_CUSTOMISED_OPTIONS);
    if (ctl != NULL) {
        (*ctl->control)(codec, context ,SET_CODEC_CUSTOMISED_OPTIONS, _options, &optionsLen);
        free(_options);
        return true;
    }
    free(_options);
    return false;
}


bool OpalPluginCodec::SetCustomFormat(unsigned frameWidth, unsigned frameHeight, unsigned frameRate)
{
    return SetCustomisedOptions(codecDefn, context, frameWidth, frameHeight, frameRate);
}

bool OpalPluginCodec::SetCustomFormat(unsigned bitrate, unsigned samplerate)
{
    return SetCustomisedOptions(codecDefn, context, bitrate, samplerate);
}

static PBoolean SetCodecControl(const PluginCodec_Definition * codec,
                                                    void * context,
                                              const char * name,
                                              const char * parm,
                                              const char * value)
{
  PluginCodec_ControlDefn * codecControls = GetCodecControl(codec, name);
  if (codecControls == NULL)
    return FALSE;

  PStringArray list;
  list += parm;
  list += value;
  char ** options = list.ToCharArray();
  unsigned int optionsLen = sizeof(options);

  bool result = (*codecControls->control)(codec, context, SET_CODEC_OPTIONS_CONTROL, options, &optionsLen);
  free(options);
  return result;
}

#if defined(H323_AUDIO_CODECS) || defined(H323_VIDEO)
static PBoolean SetCodecControl(const PluginCodec_Definition * codec,
                                                    void * context,
                                              const char * name,
                                              const char * parm,
                                                       int value)
{
  return SetCodecControl(codec, context, name, parm, PString(PString::Signed, value));
}
#endif

#ifdef H323_VIDEO

static PBoolean CallCodecControl(PluginCodec_Definition * codec,
                                               void * context,
                                         const char * name,
                                               void * parm,
                                       unsigned int * parmLen,
                                                int & retVal)
{
  PluginCodec_ControlDefn * codecControls = codec->codecControls;
  if (codecControls == NULL)
    return FALSE;

  while (codecControls->name != NULL) {
    if (strcasecmp(codecControls->name, name) == 0) {
      retVal = (*codecControls->control)(codec, context, name, parm, parmLen);
      return TRUE;
    }
    codecControls++;
  }

  return FALSE;
}

static PBoolean EventCodecControl(PluginCodec_Definition * codec,
                                               void * context,
                                         const char * name,
                                         const char * parm )
{
   PStringArray list;
   list += name;
   list += parm;

   char ** parms = list.ToCharArray();
   unsigned int parmsLen = sizeof(parms);
   int retVal = 0;

   bool result = CallCodecControl(codec,context,EVENT_CODEC_CONTROL, parms, &parmsLen, retVal);
   free(parms);
   return result;
}

static void PopulateMediaFormatOptions(PluginCodec_Definition * _encoderCodec, OpalMediaFormat & format)
{
  char ** _options = NULL;
  unsigned int optionsLen = sizeof(_options);
  int retVal;
  if (CallCodecControl(_encoderCodec, NULL, GET_CODEC_OPTIONS_CONTROL, &_options, &optionsLen, retVal) && (_options != NULL)) {
    if (_encoderCodec->version < PLUGIN_CODEC_VERSION_OPTIONS) {
      PTRACE(3, "OpalPlugin\tAdding options to OpalMediaFormat " << format << " using old style method");
      // Old scheme
      char ** options = _options;

      while (options[0] != NULL && options[1] != NULL && options[2] != NULL) {
        const char * key = options[0];
        // Backward compatibility tests
        if (strcasecmp(key, h323_qcifMPI_tag) == 0)
          key = qcifMPI_tag;
        else if (strcasecmp(key, h323_cifMPI_tag) == 0)
          key = cifMPI_tag;
        else if (strcasecmp(key, h323_sqcifMPI_tag) == 0)
          key = sqcifMPI_tag;
        else if (strcasecmp(key, h323_cif4MPI_tag) == 0)
          key = cif4MPI_tag;
        else if (strcasecmp(key, h323_cif16MPI_tag) == 0)
          key = cif16MPI_tag;
        const char * val = options[1];
        const char * type = options[2];
        OpalMediaOption::MergeType op = OpalMediaOption::NoMerge;
        if (val != NULL && val[0] != '\0' && val[1] != '\0') {
          switch (val[0]) {
            case '<':
              op = OpalMediaOption::MinMerge;
              ++val;
              break;
            case '>':
              op = OpalMediaOption::MaxMerge;
              ++val;
              break;
            case '=':
              op = OpalMediaOption::EqualMerge;
              ++val;
              break;
            case '!':
              op = OpalMediaOption::NotEqualMerge;
              ++val;
              break;
            case '*':
              op = OpalMediaOption::AlwaysMerge;
              ++val;
              break;
            default:
              break;
          }
        }
        if (type != NULL && type[0] != '\0') {
          PStringArray tokens = PString(val+1).Tokenise(':', FALSE);
          char ** array = tokens.ToCharArray();
          switch (toupper(type[0])) {
            case 'E':
              if (format.HasOption(key))
                format.SetOptionEnum(key,tokens.GetStringsIndex(val));
              else
                format.AddOption(new OpalMediaOptionEnum(key, false, array, tokens.GetSize(), op, tokens.GetStringsIndex(val)));
              break;
            case 'B':
              if (format.HasOption(key))
                  format.SetOptionBoolean(key, val != NULL && (val[0] == '1' || toupper(val[0]) == 'T'));
              else
                format.AddOption(new OpalMediaOptionBoolean(key, false, op, val != NULL && (val[0] == '1' || toupper(val[0]) == 'T')));
              break;
            case 'R':
                if (format.HasOption(key))
                format.SetOptionReal(key, PString(val).AsReal());
                else if (tokens.GetSize() < 2)
                format.AddOption(new OpalMediaOptionReal(key, false, op, PString(val).AsReal()));
                else
                format.AddOption(new OpalMediaOptionReal(key, false, op, PString(val).AsReal(), tokens[0].AsReal(), tokens[1].AsReal()));
              break;
            case 'I':
                if (format.HasOption(key))
                    format.SetOptionInteger(key,PString(val).AsInteger());
                else if (tokens.GetSize() < 2)
                    format.AddOption(new OpalMediaOptionInteger(key, false, op, PString(val).AsInteger()));
                else
                    format.AddOption(new OpalMediaOptionInteger(key, false, op, PString(val).AsInteger(), tokens[0].AsInteger(), tokens[1].AsInteger()));
              break;
            case 'S':
            default:
                if (format.HasOption(key))
                  format.SetOptionString(key, val);
                else
                  format.AddOption(new OpalMediaOptionString(key, false, val));
              break;
          }
          free(array);
        }
        options += 3;
      }
    }
    else {
      // New scheme
      struct PluginCodec_Option const * const * options = (struct PluginCodec_Option const * const *)_options;
      PTRACE_IF(5, options != NULL, "Adding options to OpalMediaFormat " << format << " using new style method");
      while (*options != NULL) {
        struct PluginCodec_Option const * option = *options++;
        OpalMediaOption * newOption;
        switch (option->m_type) {
          case PluginCodec_StringOption :
            newOption = new OpalMediaOptionString(option->m_name,
                                                  option->m_readOnly != 0,
                                                  option->m_value);
            break;
          case PluginCodec_BoolOption :
            newOption = new OpalMediaOptionBoolean(option->m_name,
                                                   option->m_readOnly != 0,
                                                   (OpalMediaOption::MergeType)option->m_merge,
                                                   option->m_value != NULL && *option->m_value == 'T');
            break;
          case PluginCodec_IntegerOption :
            newOption = new OpalMediaOptionUnsigned(option->m_name,
                                                    option->m_readOnly != 0,
                                                    (OpalMediaOption::MergeType)option->m_merge,
                                                    PString(option->m_value).AsInteger(),
                                                    PString(option->m_minimum).AsInteger(),
                                                    PString(option->m_maximum).AsInteger());
            break;
          case PluginCodec_RealOption :
            newOption = new OpalMediaOptionReal(option->m_name,
                                                option->m_readOnly != 0,
                                                (OpalMediaOption::MergeType)option->m_merge,
                                                PString(option->m_value).AsReal(),
                                                PString(option->m_minimum).AsReal(),
                                                PString(option->m_maximum).AsReal());
            break;
          case PluginCodec_EnumOption :
            {
              PStringArray valueTokens = PString(option->m_minimum).Tokenise(':');
              char ** enumValues = valueTokens.ToCharArray();
              newOption = new OpalMediaOptionEnum(option->m_name,
                                                  option->m_readOnly != 0,
                                                  enumValues,
                                                  valueTokens.GetSize(),
                                                  (OpalMediaOption::MergeType)option->m_merge,
                                                  valueTokens.GetStringsIndex(option->m_value));
              free(enumValues);
            }
            break;
          case PluginCodec_OctetsOption :
            newOption = new OpalMediaOptionOctets(option->m_name, option->m_readOnly != 0, option->m_minimum != NULL); // Use minimum to indicate Base64
            newOption->FromString(option->m_value);
            break;
          default : // Huh?
            continue;
        }

        newOption->SetFMTPName(option->m_FMTPName);
        newOption->SetFMTPDefault(option->m_FMTPDefault);

        OpalMediaOption::H245GenericInfo genericInfo;
        genericInfo.ordinal = option->m_H245Generic&PluginCodec_H245_OrdinalMask;
        if (option->m_H245Generic&PluginCodec_H245_Collapsing)
          genericInfo.mode = OpalMediaOption::H245GenericInfo::Collapsing;
        else if (option->m_H245Generic&PluginCodec_H245_NonCollapsing)
          genericInfo.mode = OpalMediaOption::H245GenericInfo::NonCollapsing;
        else
          genericInfo.mode = OpalMediaOption::H245GenericInfo::None;
        if (option->m_H245Generic&PluginCodec_H245_Unsigned32)
          genericInfo.integerType = OpalMediaOption::H245GenericInfo::Unsigned32;
        else if (option->m_H245Generic&PluginCodec_H245_BooleanArray)
          genericInfo.integerType = OpalMediaOption::H245GenericInfo::BooleanArray;
        else
          genericInfo.integerType = OpalMediaOption::H245GenericInfo::UnsignedInt;
        genericInfo.excludeTCS = (option->m_H245Generic&PluginCodec_H245_TCS) == 0;
        genericInfo.excludeOLC = (option->m_H245Generic&PluginCodec_H245_OLC) == 0;
        genericInfo.excludeReqMode = (option->m_H245Generic&PluginCodec_H245_ReqMode) == 0;
        newOption->SetH245Generic(genericInfo);

        format.AddOption(newOption, TRUE);
      }
    }

    CallCodecControl(_encoderCodec, NULL, FREE_CODEC_OPTIONS_CONTROL, _options, &optionsLen, retVal);
  } else {
      PTRACE(4,"PLUGIN\tUnable to read default options");
  }

}

static void SetDefaultVideoOptions(OpalMediaFormat & mediaFormat)
{
  mediaFormat.AddOption(new OpalMediaOptionInteger(qcifMPI_tag,  false, OpalMediaOption::MinMerge, 0));
  mediaFormat.AddOption(new OpalMediaOptionInteger(cifMPI_tag,   false, OpalMediaOption::MinMerge, 0));
  mediaFormat.AddOption(new OpalMediaOptionInteger(sqcifMPI_tag, false, OpalMediaOption::MinMerge, 0));
  mediaFormat.AddOption(new OpalMediaOptionInteger(cif4MPI_tag,  false, OpalMediaOption::MinMerge, 0));
  mediaFormat.AddOption(new OpalMediaOptionInteger(cif16MPI_tag, false, OpalMediaOption::MinMerge, 0));

  mediaFormat.AddOption(new OpalMediaOptionInteger(OpalVideoFormat::FrameWidthOption,          true,  OpalMediaOption::MinMerge, CIF_WIDTH, 11, 32767));
  mediaFormat.AddOption(new OpalMediaOptionInteger(OpalVideoFormat::FrameHeightOption,         true,  OpalMediaOption::MinMerge, CIF_HEIGHT, 9, 32767));
  mediaFormat.AddOption(new OpalMediaOptionInteger(OpalVideoFormat::EncodingQualityOption,     false, OpalMediaOption::MinMerge, 15,          1, 31));
  mediaFormat.AddOption(new OpalMediaOptionInteger(OpalVideoFormat::TargetBitRateOption,       false, OpalMediaOption::MinMerge, mediaFormat.GetBandwidth(), 1000));
  mediaFormat.AddOption(new OpalMediaOptionInteger(OpalVideoFormat::MaxBitRateOption,          false, OpalMediaOption::MinMerge, mediaFormat.GetBandwidth(), 1000));
  mediaFormat.AddOption(new OpalMediaOptionBoolean(OpalVideoFormat::DynamicVideoQualityOption, false, OpalMediaOption::NoMerge,  false));
  mediaFormat.AddOption(new OpalMediaOptionBoolean(OpalVideoFormat::AdaptivePacketDelayOption, false, OpalMediaOption::NoMerge,  false));
  mediaFormat.AddOption(new OpalMediaOptionInteger(OpalVideoFormat::FrameTimeOption,           false, OpalMediaOption::NoMerge,  9000));
  mediaFormat.AddOption(new OpalMediaOptionBoolean(OpalVideoFormat::EmphasisSpeedOption,       false, OpalMediaOption::MaxMerge,  false));
  mediaFormat.AddOption(new OpalMediaOptionInteger(OpalVideoFormat::MaxPayloadSizeOption,      false, OpalMediaOption::MaxMerge,  0));

  mediaFormat.AddOption(new OpalMediaOptionBoolean(h323_temporalSpatialTradeOffCapability_tag, false, OpalMediaOption::NoMerge,  false));
  mediaFormat.AddOption(new OpalMediaOptionBoolean(h323_stillImageTransmission_tag           , false, OpalMediaOption::NoMerge,  false));
}

#endif  // #ifdef H323_VIDEO

#if defined(H323_AUDIO_CODECS) || defined(H323_VIDEO)
static void PopulateMediaFormatFromGenericData(OpalMediaFormat & mediaFormat, const PluginCodec_H323GenericCodecData * genericData)
{
  const PluginCodec_H323GenericParameterDefinition *ptr = genericData->params;
  for (unsigned i = 0; i < genericData->nParameters; i++, ptr++) {
    OpalMediaOption::H245GenericInfo generic;
    generic.ordinal = ptr->id;
    generic.mode = ptr->collapsing ? OpalMediaOption::H245GenericInfo::Collapsing : OpalMediaOption::H245GenericInfo::NonCollapsing;
    generic.excludeTCS = ptr->excludeTCS;
    generic.excludeOLC = ptr->excludeOLC;
    generic.excludeReqMode = ptr->excludeReqMode;
    generic.integerType = OpalMediaOption::H245GenericInfo::UnsignedInt;

    PString name(PString::Printf, "Generic Parameter %u", ptr->id);

    OpalMediaOption * mediaOption;
    switch (ptr->type) {
      case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_logical :
        mediaOption = new OpalMediaOptionBoolean(name, ptr->readOnly, OpalMediaOption::NoMerge, ptr->value.integer != 0);
        break;

      case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_booleanArray :
        generic.integerType = OpalMediaOption::H245GenericInfo::BooleanArray;
        mediaOption = new OpalMediaOptionUnsigned(name, ptr->readOnly, OpalMediaOption::AndMerge, ptr->value.integer, 0, 255);
        break;

      case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsigned32Min :
        generic.integerType = OpalMediaOption::H245GenericInfo::Unsigned32;
        // Do next case

      case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMin :
        mediaOption = new OpalMediaOptionUnsigned(name, ptr->readOnly, OpalMediaOption::MinMerge, ptr->value.integer);
        break;

      case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsigned32Max :
        generic.integerType = OpalMediaOption::H245GenericInfo::Unsigned32;
        // Do next case

      case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_unsignedMax :
        mediaOption = new OpalMediaOptionUnsigned(name, ptr->readOnly, OpalMediaOption::MaxMerge, ptr->value.integer);
        break;

      case PluginCodec_H323GenericParameterDefinition::PluginCodec_GenericParameter_octetString :
        mediaOption = new OpalMediaOptionString(name, ptr->readOnly, ptr->value.octetstring);
        break;

      default :
        mediaOption = NULL;
    }

    if (mediaOption != NULL) {
      mediaOption->SetH245Generic(generic);
      mediaFormat.AddOption(mediaOption);
    }
  }
}
#endif

static PString CreateCodecName(PluginCodec_Definition * codec, PBoolean addSW)
{
  PString str;
  if (codec->destFormat != NULL)
    str = codec->destFormat;
  else
    str = PString(codec->descr);
  if (addSW)
    str += "{sw}";
  return str;
}

static PString CreateCodecName(const PString & baseName, PBoolean addSW)
{
  PString str(baseName);
  if (addSW)
    str += "{sw}";
  return str;
}

static void SetDefaultAudioOptions(OpalMediaFormat & mediaFormat)
{
#ifdef H323_VIDEO
   mediaFormat.AddOption(new OpalMediaOptionInteger(OpalVideoFormat::MaxBitRateOption, false, OpalMediaOption::MinMerge, mediaFormat.GetBandwidth()*100, 1000));
#endif
}

class OpalPluginAudioMediaFormat : public OpalMediaFormat
{
  public:
    friend class H323PluginCodecManager;

    OpalPluginAudioMediaFormat(
      PluginCodec_Definition * _encoderCodec,
      unsigned defaultSessionID,  /// Default session for codec type
      PBoolean     needsJitter,   /// Indicate format requires a jitter buffer
      unsigned frameTime,     /// Time for frame in RTP units (if applicable)
      unsigned timeUnits,     /// RTP units for frameTime (if applicable)
      time_t timeStamp        /// timestamp (for versioning)
    )
    : OpalMediaFormat(
      CreateCodecName(_encoderCodec, FALSE),
      defaultSessionID,
      (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ? RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload),
      needsJitter,
      _encoderCodec->bitsPerSec,
      _encoderCodec->parm.audio.bytesPerFrame,
      frameTime,
      timeUnits,
      timeStamp
    )
    , encoderCodec(_encoderCodec)
    {
      SetDefaultAudioOptions(*this);
      // manually register the new singleton type, as we do not have a concrete type
      OpalMediaFormatFactory::Register(*this, this);
    }
    ~OpalPluginAudioMediaFormat()
    {
      OpalMediaFormatFactory::Unregister(*this);
    }
    PluginCodec_Definition * encoderCodec;
};

#ifdef H323_AUDIO_CODECS

static H323Capability * CreateG7231Cap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateGenericAudioCap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateNonStandardAudioCap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateGSMCap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int subType
);

#endif // NO_H323_AUDIO

/////////////////////////////////////////////////////

#ifdef H323_VIDEO

class OpalPluginVideoMediaFormat : public OpalVideoFormat
{
  public:
    friend class OpalPluginCodecManager;

    OpalPluginVideoMediaFormat(
      PluginCodec_Definition * _encoderCodec,
      const char * /*rtpEncodingName*/, /// rtp encoding name. Not required
      time_t timeStamp              /// timestamp (for versioning)
    )
    : OpalVideoFormat(
      CreateCodecName(_encoderCodec, FALSE),
      (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ? RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload),
      _encoderCodec->parm.video.maxFrameWidth,
      _encoderCodec->parm.video.maxFrameHeight,
      _encoderCodec->parm.video.maxFrameRate,
      _encoderCodec->bitsPerSec,
      timeStamp
    )
    , encoderCodec(_encoderCodec)
    {
       SetDefaultVideoOptions(*this);

       rtpPayloadType = (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ? RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload);
       frameTime = (VideoTimeUnits * encoderCodec->usPerFrame) / 1000;
       timeUnits = encoderCodec->sampleRate / 1000;

      // manually register the new singleton type, as we do not have a concrete type
      OpalMediaFormatFactory::Register(*this, this);
    }
    ~OpalPluginVideoMediaFormat()
    {
      OpalMediaFormatFactory::Unregister(*this);
    }

    PObject * Clone() const
    { return new OpalPluginVideoMediaFormat(*this); }

    PluginCodec_Definition * encoderCodec;
};

static H323Capability * CreateNonStandardVideoCap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateGenericVideoCap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateH261Cap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int subType
);

static H323Capability * CreateH263Cap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int subType
);

#endif // H323_VIDEO


/*
//////////////////////////////////////////////////////////////////////////////
//
// Class to auto-register plugin capabilities
//

class H323CodecPluginCapabilityRegistration : public PObject
{
  public:
    H323CodecPluginCapabilityRegistration(
       PluginCodec_Definition * _encoderCodec,
       PluginCodec_Definition * _decoderCodec
    );

    H323Capability * Create(H323EndPoint & ep) const;

    static H323Capability * CreateG7231Cap           (H323EndPoint & ep, int subType) const;
    static H323Capability * CreateNonStandardAudioCap(H323EndPoint & ep, int subType) const;
    //H323Capability * CreateNonStandardVideoCap(H323EndPoint & ep, int subType) const;
    static H323Capability * CreateGSMCap             (H323EndPoint & ep, int subType) const;
    static H323Capability * CreateH261Cap            (H323EndPoint & ep, int subType) const;

  protected:
    PluginCodec_Definition * encoderCodec;
    PluginCodec_Definition * decoderCodec;
};

*/

class H323CodecPluginCapabilityMapEntry {
  public:
    int pluginCapType;
    int h323SubType;
    H323Capability * (* createFunc)(PluginCodec_Definition * encoderCodec, PluginCodec_Definition * decoderCodec, int subType);
};

#ifdef H323_AUDIO_CODECS

static H323CodecPluginCapabilityMapEntry audioMaps[] = {
  { PluginCodec_H323Codec_nonStandard,              H245_AudioCapability::e_nonStandard,         &CreateNonStandardAudioCap },
  { PluginCodec_H323AudioCodec_gsmFullRate,         H245_AudioCapability::e_gsmFullRate,         &CreateGSMCap },
  { PluginCodec_H323AudioCodec_gsmHalfRate,         H245_AudioCapability::e_gsmHalfRate,         &CreateGSMCap },
  { PluginCodec_H323AudioCodec_gsmEnhancedFullRate, H245_AudioCapability::e_gsmEnhancedFullRate, &CreateGSMCap },
  { PluginCodec_H323AudioCodec_g711Alaw_64k,        H245_AudioCapability::e_g711Alaw64k },
  { PluginCodec_H323AudioCodec_g711Alaw_56k,        H245_AudioCapability::e_g711Alaw56k },
  { PluginCodec_H323AudioCodec_g711Ulaw_64k,        H245_AudioCapability::e_g711Ulaw64k },
  { PluginCodec_H323AudioCodec_g711Ulaw_56k,        H245_AudioCapability::e_g711Ulaw56k },
  { PluginCodec_H323AudioCodec_g7231,               H245_AudioCapability::e_g7231,               &CreateG7231Cap },
  { PluginCodec_H323AudioCodec_g729,                H245_AudioCapability::e_g729 },
  { PluginCodec_H323AudioCodec_g729AnnexA,          H245_AudioCapability::e_g729AnnexA },
  { PluginCodec_H323AudioCodec_g728,                H245_AudioCapability::e_g728 },
  { PluginCodec_H323AudioCodec_g722_64k,            H245_AudioCapability::e_g722_64k },
  { PluginCodec_H323AudioCodec_g722_56k,            H245_AudioCapability::e_g722_56k },
  { PluginCodec_H323AudioCodec_g722_48k,            H245_AudioCapability::e_g722_48k },
  { PluginCodec_H323AudioCodec_g729wAnnexB,         H245_AudioCapability::e_g729wAnnexB },
  { PluginCodec_H323AudioCodec_g729AnnexAwAnnexB,   H245_AudioCapability::e_g729AnnexAwAnnexB },
  { PluginCodec_H323Codec_generic,                  H245_AudioCapability::e_genericAudioCapability, &CreateGenericAudioCap },

  // not implemented
  //{ PluginCodec_H323AudioCodec_g729Extensions,      H245_AudioCapability::e_g729Extensions,   0 },
  //{ PluginCodec_H323AudioCodec_g7231AnnexC,         H245_AudioCapability::e_g7231AnnexCMode   0 },
  //{ PluginCodec_H323AudioCodec_is11172,             H245_AudioCapability::e_is11172AudioMode, 0 },
  //{ PluginCodec_H323AudioCodec_is13818Audio,        H245_AudioCapability::e_is13818AudioMode, 0 },

  { -1 }
};

#endif

#ifdef H323_VIDEO

static H323CodecPluginCapabilityMapEntry videoMaps[] = {
  // video codecs
  { PluginCodec_H323Codec_nonStandard,              H245_VideoCapability::e_nonStandard, &CreateNonStandardVideoCap },
  { PluginCodec_H323VideoCodec_h261,                H245_VideoCapability::e_h261VideoCapability, &CreateH261Cap },
  { PluginCodec_H323VideoCodec_h263,                H245_VideoCapability::e_h263VideoCapability,    &CreateH263Cap },
  { PluginCodec_H323Codec_generic,                  H245_VideoCapability::e_genericVideoCapability, &CreateGenericVideoCap },
/*
  PluginCodec_H323VideoCodec_h262,                // not yet implemented
  PluginCodec_H323VideoCodec_is11172,             // not yet implemented
*/

  { -1 }
};

#endif  // H323_VIDEO

#if defined(H323_AUDIO_CODECS) || defined(H323_VIDEO)
static bool UpdatePluginOptions(const PluginCodec_Definition * codec, void * context, OpalMediaFormat & mediaFormat) {

    PluginCodec_ControlDefn * ctl = GetCodecControl(codec, SET_CODEC_OPTIONS_CONTROL);
    if (ctl != NULL) {
      PStringArray list(mediaFormat.GetOptionCount()*2);
      for (PINDEX i = 0; i < mediaFormat.GetOptionCount(); i++) {
        const OpalMediaOption & option = mediaFormat.GetOption(i);
        list[i*2] = option.GetName();
        list[(i*2)+1] = option.AsString();
      }
      char ** _options = list.ToCharArray();
      unsigned int optionsLen = sizeof(_options);
      (*ctl->control)(codec, context, SET_CODEC_OPTIONS_CONTROL, _options, &optionsLen);
      for (int i = 0; _options[i] != NULL; i += 2) {
      const char * key = _options[i];
      int val = atoi(_options[i+1]);
      if (mediaFormat.HasOption(key))
        mediaFormat.SetOptionInteger(key,val);
      }
#ifdef H323_VIDEO
      mediaFormat.SetBandwidth(mediaFormat.GetOptionInteger(OpalVideoFormat::MaxBitRateOption));
#endif

      free(_options);
      return true;
    }
    return false;
}
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Plugin framed audio codec classes
//

#ifdef H323_AUDIO_CODECS

class H323PluginFramedAudioCodec : public H323FramedAudioCodec
{
  PCLASSINFO(H323PluginFramedAudioCodec, H323FramedAudioCodec);
  public:
    H323PluginFramedAudioCodec(const OpalMediaFormat & fmtName, Direction direction, PluginCodec_Definition * _codec)
      : H323FramedAudioCodec(fmtName, direction), codec(_codec)
    {
      if (codec && codec->createCodec) {
         context = (*codec->createCodec)(codec);
         UpdatePluginOptions(codec,context,GetWritableMediaFormat());
      } else context = NULL;
    }

    ~H323PluginFramedAudioCodec()
    { if (codec != NULL && codec->destroyCodec != NULL) (*codec->destroyCodec)(codec, context); }

    PBoolean EncodeFrame(
      BYTE * buffer,        /// Buffer into which encoded bytes are placed
      unsigned int & toLen  /// Actual length of encoded data buffer
    )
    {
      if (codec == NULL || direction != Encoder)
        return FALSE;

      unsigned int fromLen = codec->parm.audio.samplesPerFrame*2;
      toLen                = codec->parm.audio.bytesPerFrame;
      unsigned flags = 0;
      return (codec->codecFunction)(codec, context,
                                 (const unsigned char *)sampleBuffer.GetPointer(), &fromLen,
                                 buffer, &toLen,
                                 &flags) != 0;
    };

    PBoolean DecodeFrame(
      const BYTE * buffer,    /// Buffer from which encoded data is found
      unsigned length,        /// Length of encoded data buffer
      unsigned & written,     /// Number of bytes used from data buffer
      unsigned & bytesDecoded /// Number of bytes output from frame
    )
    {
      if (codec == NULL || direction != Decoder)
        return FALSE;
      unsigned flags = 0;
      if ((codec->codecFunction)(codec, context,
                                 buffer, &length,
                                 (unsigned char *)sampleBuffer.GetPointer(), &bytesDecoded,
                                 &flags) == 0)
        return FALSE;

      written = length;
      return TRUE;
    }

    void DecodeSilenceFrame(
      void * buffer,        /// Buffer from which encoded data is found
      unsigned length       /// Length of encoded data buffer
    )
    {
      if ((codec->flags & PluginCodec_DecodeSilence) == 0)
        memset(buffer, 0, length);
      else {
        unsigned flags = PluginCodec_CoderSilenceFrame;
        (codec->codecFunction)(codec, context,
                                 NULL, NULL,
                                 buffer, &length,
                                 &flags);
      }
    }

    virtual void SetTxQualityLevel(int qlevel)
    { SetCodecControl(codec, context, SET_CODEC_OPTIONS_CONTROL, "set_quality", qlevel); }

  protected:
    void * context;
    PluginCodec_Definition * codec;
};

//////////////////////////////////////////////////////////////////////////////
//
// Plugin streamed audio codec classes
//

class H323StreamedPluginAudioCodec : public H323StreamedAudioCodec
{
  PCLASSINFO(H323StreamedPluginAudioCodec, H323StreamedAudioCodec);
  public:
    H323StreamedPluginAudioCodec(
      const OpalMediaFormat & fmtName,
      H323Codec::Direction direction,
      unsigned samplesPerFrame,  /// Number of samples in a frame
      unsigned bits,             /// Bits per sample
      PluginCodec_Definition * _codec
    )
      : H323StreamedAudioCodec(fmtName, direction, samplesPerFrame, bits), codec(_codec)
    { if (codec != NULL && codec->createCodec != NULL) context = (*codec->createCodec)(codec); else context = NULL; }

    ~H323StreamedPluginAudioCodec()
    { if (codec != NULL && codec->destroyCodec != NULL) (*codec->destroyCodec)(codec, context); }

    int Encode(short sample) const
    {
      if (codec == NULL || direction != Encoder)
        return 0;
      unsigned int fromLen = sizeof(sample);
      int to;
      unsigned toLen = sizeof(to);
      unsigned flags = 0;
      (codec->codecFunction)(codec, context,
                                 (const unsigned char *)&sample, &fromLen,
                                 (unsigned char *)&to, &toLen,
                                 &flags);
      return to;
    }

    short Decode(int sample) const
    {
      if (codec == NULL || direction != Decoder)
        return 0;
      unsigned fromLen = sizeof(sample);
      short to;
      unsigned toLen   = sizeof(to);
      unsigned flags = 0;
      (codec->codecFunction)(codec, context,
                                 (const unsigned char *)&sample, &fromLen,
                                 (unsigned char *)&to, &toLen,
                                 &flags);
      return to;
    }

    virtual void SetTxQualityLevel(int qlevel)
    { SetCodecControl(codec, context, SET_CODEC_OPTIONS_CONTROL, "set_quality", qlevel); }

  protected:
    void * context;
    PluginCodec_Definition * codec;
};

#endif //  NO_H323_AUDIO_CODECS

//////////////////////////////////////////////////////////////////////////////
//
// Plugin video codec class
//

#ifdef H323_PACKET_TRACE
static void CheckPacket(PBoolean encode, const RTP_DataFrame & frame)
{
    PTRACE(6, "RTP\t" << (encode ? "> " : "< ")
           << " ver=" << frame.GetVersion()
           << " pt=" << frame.GetPayloadType()
           << " psz=" << frame.GetPayloadSize()
           << " m=" << frame.GetMarker()
           << " x=" << frame.GetExtension()
           << " seq=" << frame.GetSequenceNumber()
           << " ts=" << frame.GetTimestamp()
           << " src=" << frame.GetSyncSource()
           << " ccnt=" << frame.GetContribSrcCount());
}
#endif

#ifdef H323_FRAMEBUFFER

class H323PluginFrameBuffer : public H323_FrameBuffer
{
public:
    H323PluginFrameBuffer()
        : codec(NULL), m_noError(true), m_flowControl(false) {};

    void SetCodec(H323Codec * _codec) {
        codec = _codec;
        Start();
    }

    virtual void FrameOut(PBYTEArray & frame, PInt64 receiveTime, unsigned clock , PBoolean fup, PBoolean flow)  {
        m_flowControl = flow;
        frameData.SetPayloadSize(frame.GetSize()-12);
        memmove(frameData.GetPointer(), frame.GetPointer(), frame.GetSize());
        unsigned written = 0;
        H323Codec::H323_RTPInformation  rtpInformation;
          rtpInformation.m_recvTime = receiveTime;
          rtpInformation.m_timeStamp = frameData.GetTimestamp();
          rtpInformation.m_clockRate = clock*1000;
          codec->CalculateRTPSendTime(rtpInformation.m_timeStamp, rtpInformation.m_clockRate, rtpInformation.m_sendTime);
          rtpInformation.m_frame = &frameData;
        m_noError = codec->WriteInternal(frameData.GetPointer(), frameData.GetSize(), frameData, written, rtpInformation);
        //frameData.SetPayloadSize(0);
    }

    virtual PBoolean FrameIn(unsigned seq, unsigned time, PBoolean marker, unsigned payload, const PBYTEArray & frame) {
        return ((codec) ? H323_FrameBuffer::FrameIn(seq,time,marker,payload,frame) : false);
    }

    PBoolean GetNoError() {
        return m_noError;
    }

    PBoolean GetFlowControl() {
        return m_flowControl;
    }

protected:
    RTP_DataFrame frameData;

    H323Codec * codec;
    PBoolean m_noError;
    PBoolean m_flowControl;
};
#endif

#ifdef H323_VIDEO

class H323PluginVideoCodec : public H323VideoCodec
{
  PCLASSINFO(H323PluginVideoCodec, H323VideoCodec);
  public:
    H323PluginVideoCodec(const OpalMediaFormat & fmt, Direction direction,
                         PluginCodec_Definition * _codec,
                         const H323Capability * cap = NULL
                         );

    ~H323PluginVideoCodec();

    virtual PBoolean Open(
      H323Connection & connection ///< Connection between the endpoints
    );

    virtual PBoolean Read(
      BYTE * buffer,            ///< Buffer of encoded data
      unsigned & length,        ///< Actual length of encoded data buffer
      RTP_DataFrame & dst       ///< RTP data frame
    );

    virtual PBoolean Write(
      const BYTE * buffer,        ///< Buffer of encoded data
      unsigned length,            ///< Length of encoded data buffer
      const RTP_DataFrame & src,  ///< RTP data frame
      unsigned & written          ///< Number of bytes used from data buffer
    );

    virtual PBoolean WriteInternal(
      const BYTE * buffer,             ///< Buffer of encoded data
      unsigned length,                 ///< Length of encoded data buffer
      const RTP_DataFrame & src,       ///< RTP data frame
      unsigned & written,              ///< Number of bytes used from data buffer
      H323_RTPInformation & rtp        ///< RTP Information
    );

    PBoolean RenderFrame(
      const BYTE * buffer,        ///< Buffer of data to render
      void * mark                 ///< WaterMark
    );

    PBoolean RenderInternal(
      const BYTE * buffer,        ///< Buffer of data to render
      void * mark                 ///< WaterMark
    );

    virtual unsigned GetFrameRate() const
    {  return targetFrameTimeMs ? 90000/targetFrameTimeMs : 30;  }

    PBoolean SetTargetFrameTimeMs(unsigned ms)  // Requires implementing
    {  targetFrameTimeMs = ms; return TRUE; }

    virtual PBoolean SetFrameSize(int width, int height);

    virtual PBoolean SetFrameSize(int width, int height,int sarwidth, int sarheight);

    void SetTxQualityLevel(int qlevel)
    { SetCodecControl(codec, context, SET_CODEC_OPTIONS_CONTROL, "Encoding Quality", qlevel); }

    void SetTxMinQuality(int qlevel)
    { SetCodecControl(codec, context, SET_CODEC_OPTIONS_CONTROL, "set_min_quality", qlevel); }

    void SetTxMaxQuality(int qlevel)
    { SetCodecControl(codec, context, SET_CODEC_OPTIONS_CONTROL, "set_max_quality", qlevel); }

    void SetBackgroundFill(int fillLevel)
    { SetCodecControl(codec, context, SET_CODEC_OPTIONS_CONTROL, "set_background_fill", fillLevel); }

    virtual unsigned GetMaxBitRate() const
    { return mediaFormat.GetOptionInteger(OpalVideoFormat::MaxBitRateOption); }

    virtual PBoolean SetMaxBitRate(unsigned bitRate);

    virtual void SetEmphasisSpeed(bool speed);

    virtual void SetMaxPayloadSize(int maxSize);

    void SetGeneralCodecOption(const char * opt, int val)
    { SetCodecControl(codec, context, SET_CODEC_OPTIONS_CONTROL, opt, val);}

    unsigned GetVideoMode(void);

    void SetVideoMode(int mode);

    // The following require implementation in the plugin codec
    virtual void OnFastUpdatePicture() {
      EventCodecControl(codec, context, "on_fast_update", "");
      sendIntra = true;
    }

    virtual void OnFlowControl(long bitRateRestriction);

    virtual PBoolean SetSupportedFormats(std::list<PVideoFrameInfo> & info);

    virtual void OnLostPartialPicture()
    { EventCodecControl(codec, context, "on_lost_partial", ""); }

    virtual void OnLostPicture()
    { EventCodecControl(codec, context, "on_lost_picture", ""); }

  protected:
    void *       context;
    PluginCodec_Definition * codec;
    int          bufferSize;
    RTP_DataFrame bufferRTP;
    int          maxWidth;
    int          maxHeight;

    unsigned     bytesPerFrame;
    unsigned     lastFrameTimeRTP;
    unsigned     targetFrameTimeMs;

    long         flowRequest;
    PBoolean     lastPacketSent;
    bool         sendIntra;

    PInt64  lastFrameTick;
    PInt64  nowFrameTick;
    PInt64  lastFUPTick;
    PInt64  nowFUPTick;

    // Regular used variables
    int          outputDataSize;
    unsigned int fromLen;
    unsigned int toLen;
    unsigned int flags;
    int          pluginRetVal;

#ifdef H323_FRAMEBUFFER
    H323PluginFrameBuffer  m_frameBuffer;
#endif
};

static bool SetFlowControl(const PluginCodec_Definition * codec, void * context, OpalMediaFormat & mediaFormat, long bitRate)
{
    if (context == NULL)
        return false;

    if (mediaFormat.GetOptionInteger(OpalVideoFormat::MaxBitRateOption) < (bitRate*100)) {
        PTRACE(3,"H323\tFlow Control request exceeds codec limits Ignored! Max: "
          << mediaFormat.GetOptionInteger(OpalVideoFormat::MaxBitRateOption) << " Req: " << bitRate*100);
        return false;
    }

    if (mediaFormat.GetOptionInteger(OpalVideoFormat::TargetBitRateOption) == (bitRate*100)) {
        PTRACE(3,"H323\tFlow Control request ignored already doing " << bitRate*100);
        return false;
    }

    PluginCodec_ControlDefn * ctl = GetCodecControl(codec, SET_CODEC_FLOWCONTROL_OPTIONS);
    if (ctl != NULL) {
      mediaFormat.SetOptionInteger(OpalVideoFormat::TargetBitRateOption,(int)bitRate * 100);
      PStringArray strlist(mediaFormat.GetOptionCount()*2);
      for (PINDEX i = 0; i < mediaFormat.GetOptionCount(); i++) {
        const OpalMediaOption & option = mediaFormat.GetOption(i);
        strlist[i*2] = option.GetName();
        strlist[(i*2)+1] = option.AsString();
      }
      char ** _options = strlist.ToCharArray();
      unsigned int optionsLen = sizeof(_options);
      (*ctl->control)(codec, context ,SET_CODEC_FLOWCONTROL_OPTIONS, _options, &optionsLen);
          for (int i = 0; _options[i] != NULL; i += 2) {
            const char * key = _options[i];
            int val = (_options[i+1] != NULL ? atoi(_options[i+1]) : 0);
            if (mediaFormat.HasOption(key) && val > 0)
                mediaFormat.SetOptionInteger(key,val);
          }
      free(_options);
      strlist.SetSize(0);
#if PTRACING
      PTRACE(6, "H323\tFlow Control applied: ");
      OpalMediaFormat::DebugOptionList(mediaFormat);
#endif
      return true;
    }
    PTRACE(3, "H323\tNo Flow Control supported in codec:");
    return false;
}

static bool SetCustomLevel(const PluginCodec_Definition * codec, OpalMediaFormat & mediaFormat, unsigned width, unsigned height, unsigned rate)
{

    mediaFormat.SetOptionInteger(OpalVideoFormat::FrameWidthOption,width);
    mediaFormat.SetOptionInteger(OpalVideoFormat::FrameHeightOption,height);
    mediaFormat.SetOptionInteger(OpalVideoFormat::FrameTimeOption, (int)(OpalMediaFormat::VideoTimeUnits * 1000 * 100 * rate / 2997));

    PluginCodec_ControlDefn * ctl = GetCodecControl(codec, SET_CODEC_CUSTOMISED_OPTIONS);
    if (ctl != NULL) {
      PStringArray list;
      for (PINDEX i = 0; i < mediaFormat.GetOptionCount(); i++) {
        const OpalMediaOption & option = mediaFormat.GetOption(i);
        list += option.GetName();
        list += option.AsString();
      }
      char ** _options = list.ToCharArray();
      unsigned int optionsLen = sizeof(_options);
      (*ctl->control)(codec, NULL,SET_CODEC_CUSTOMISED_OPTIONS, _options, &optionsLen);
          while (_options[0] != NULL && _options[1] != NULL) {
            const char * key = _options[0];
            int val = atoi(_options[1]);
            if (strcasecmp(key, OpalVideoFormat::TargetBitRateOption) == 0) {
                mediaFormat.SetOptionInteger(OpalVideoFormat::TargetBitRateOption,val);
                mediaFormat.SetOptionInteger(OpalVideoFormat::MaxBitRateOption,val);
            } else if (strcasecmp(key, "Generic Parameter 42") == 0)
                mediaFormat.SetOptionInteger("Generic Parameter 42",val);
            else if (strcasecmp(key, "Generic Parameter 10") == 0)
                mediaFormat.SetOptionInteger("Generic Parameter 10",13);  // 16:9 Ratio

            _options += 2;
          }
      free(_options);
#if PTRACING
      PTRACE(6, "H323\tCustom Video Format: ");
      OpalMediaFormat::DebugOptionList(mediaFormat);
#endif
      return true;
    }
    return false;
}

bool OpalPluginCodec::SetMediaFormat(OpalMediaFormat & fmt)
{
  switch (codecDefn->flags & PluginCodec_MediaTypeMask) {
      case PluginCodec_MediaTypeVideo:
          SetDefaultVideoOptions(fmt);
          break;
      case PluginCodec_MediaTypeAudio:
      case PluginCodec_MediaTypeAudioStreamed:
      default:
          return false;
  }

    PopulateMediaFormatOptions(codecDefn, fmt);
    PopulateMediaFormatFromGenericData(fmt,
          (PluginCodec_H323GenericCodecData *)codecDefn->h323CapabilityData);
    OpalMediaFormat::DebugOptionList(fmt);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////

bool OpalPluginCodec::UpdateMediaOptions(OpalMediaFormat & fmt)
{
  switch (codecDefn->flags & PluginCodec_MediaTypeMask) {
      case PluginCodec_MediaTypeVideo:
      case PluginCodec_MediaTypeAudio:
           return UpdatePluginOptions(codecDefn, context, fmt);
      case PluginCodec_MediaTypeAudioStreamed:
      default:
          return false;
  }
}

bool OpalPluginCodec::CodecControl(const char * name, void * parm, unsigned int * parmLen, int & retVal)
{
    return CallCodecControl(codecDefn, context, name, parm, parmLen, retVal);
}

///////////////////////////////////////////////////////////////////////////////////////////

#define PLUGIN_MAX_WIDTH   1920
#define PLUGIN_MAX_HEIGHT  1200
#define PLUGIN_RTP_HEADER_SIZE 12
#define MAX_MTU_SIZE 2000  //1518-14-4-8-20-16; Max Ethernet packet (1518 bytes) minus 802.3/CRC, 802.3, IP, UDP headers

H323PluginVideoCodec::H323PluginVideoCodec(const OpalMediaFormat & fmt, Direction direction, PluginCodec_Definition * _codec, const H323Capability * cap)
    : H323VideoCodec(fmt, direction), context(NULL), codec(_codec),
      bufferSize(sizeof(PluginCodec_Video_FrameHeader) + (PLUGIN_MAX_WIDTH * PLUGIN_MAX_HEIGHT * 3)/2 + PLUGIN_RTP_HEADER_SIZE), bufferRTP(bufferSize-PLUGIN_RTP_HEADER_SIZE, TRUE),
      maxWidth(fmt.GetOptionInteger(OpalVideoFormat::FrameWidthOption)), maxHeight(fmt.GetOptionInteger(OpalVideoFormat::FrameHeightOption)),
      bytesPerFrame((maxHeight * maxWidth * 3)/2), lastFrameTimeRTP(0), targetFrameTimeMs(fmt.GetOptionInteger(OpalVideoFormat::FrameTimeOption)),
      flowRequest(0), lastPacketSent(true), sendIntra(true), lastFrameTick(0), nowFrameTick(0), lastFUPTick(0), nowFUPTick(0), outputDataSize(MAX_MTU_SIZE),
      fromLen(0), toLen(0), flags(0), pluginRetVal(0)
{
    if (codec && codec->createCodec) {
        context = (*codec->createCodec)(codec);
        UpdatePluginOptions(codec,context,GetWritableMediaFormat());
    } else context = NULL;

    if (cap) {
        OpalMediaFormat & capFmt = PRemoveConst(H323Capability, cap)->GetWritableMediaFormat();
        capFmt = GetMediaFormat();
    }

    frameWidth = maxWidth;
    frameHeight = maxHeight;

#if PTRACING
   PTRACE(6,"Agreed Codec Options");
     OpalMediaFormat::DebugOptionList(mediaFormat);
#endif
}

H323PluginVideoCodec::~H323PluginVideoCodec()
{
    //PWaitAndSignal mutex(videoHandlerActive);

#ifdef H323_FRAMEBUFFER
    m_frameBuffer.Terminate();
    m_frameBuffer.WaitForTermination();
#endif
    // Set the buffer memory to zero to prevent
    // memory leak
    bufferRTP.SetSize(0);

    if (codec != NULL && codec->destroyCodec != NULL)
        (*codec->destroyCodec)(codec, context);
}

PBoolean H323PluginVideoCodec::SetMaxBitRate(unsigned bitRate)
{
    if (SetFlowControl(codec,context,mediaFormat,bitRate/100)) {
         frameWidth = mediaFormat.GetOptionInteger(OpalVideoFormat::FrameWidthOption);
         frameHeight =  mediaFormat.GetOptionInteger(OpalVideoFormat::FrameHeightOption);
         targetFrameTimeMs = mediaFormat.GetOptionInteger(OpalVideoFormat::FrameTimeOption);
         mediaFormat.SetBandwidth(bitRate);
         return true;
    }
    return false;
}

void H323PluginVideoCodec::SetEmphasisSpeed(bool speed)
{
  mediaFormat.SetOptionBoolean(OpalVideoFormat::EmphasisSpeedOption, speed);
  //UpdatePluginOptions(codec, context, mediaFormat);
}

void H323PluginVideoCodec::SetMaxPayloadSize(int maxSize)
{
  mediaFormat.SetOptionInteger(OpalVideoFormat::MaxPayloadSizeOption, (int)maxSize);
  UpdatePluginOptions(codec, context, mediaFormat);
}

PStringArray LoadInputDeviceOptions(const OpalMediaFormat & fmt)
{
     PStringArray list;
     list += OpalVideoFormat::FrameHeightOption;
     list += PString(fmt.GetOptionInteger(OpalVideoFormat::FrameHeightOption));
     list += OpalVideoFormat::FrameWidthOption;
     list += PString(fmt.GetOptionInteger(OpalVideoFormat::FrameWidthOption));
     list += OpalVideoFormat::FrameTimeOption;
     list += PString(fmt.GetOptionInteger(OpalVideoFormat::FrameTimeOption));
     return list;
}

void H323PluginVideoCodec::OnFlowControl(long bitRateRestriction)
{
  if (direction != Encoder) {
    PTRACE(1, "PLUGIN\tAttempt to flowControl the decoder!");
    return;
  }

  flowRequest = bitRateRestriction;

}

PBoolean H323PluginVideoCodec::SetSupportedFormats(std::list<PVideoFrameInfo> & info)
{
    PluginCodec_ControlDefn * ctl = GetCodecControl(codec, SET_CODEC_FORMAT_OPTIONS);
    if (ctl != NULL && !info.empty()) {
      int i = 0;
      PStringArray list(info.size()*2 + mediaFormat.GetOptionCount()*2);
      for (std::list<PVideoFrameInfo>::const_iterator r = info.begin(); r != info.end(); ++r) {
        PString option(PString(r->GetFrameWidth()) + "," + PString(r->GetFrameHeight()) + "," + PString(r->GetFrameRate()));
        list[i*2] = PString("InputFmt"+ PString(i+1));
        list[(i*2)+1] = option;
        i++;
      }

      for (PINDEX j = 0; j < mediaFormat.GetOptionCount(); j++) {
        const OpalMediaOption & option = mediaFormat.GetOption(j);
        list[i*2] = option.GetName();
        list[(i*2)+1] = option.AsString();
        i++;
      }

      int nw = frameWidth;
      int nh = frameHeight;

      char ** _options = list.ToCharArray();
      unsigned int optionsLen = sizeof(_options);
      (*ctl->control)(codec, context, SET_CODEC_FORMAT_OPTIONS, _options, &optionsLen);
      for (i = 0; _options[i] != NULL; i += 2) {
        const char * key = _options[i];
        int val = atoi(_options[i+1]);;
        if (mediaFormat.HasOption(key)) {
          mediaFormat.SetOptionInteger(key,val);
          if (strcmp(key, OpalVideoFormat::FrameWidthOption) == 0)
            nw = val;
          else if (strcmp(key, OpalVideoFormat::FrameHeightOption) == 0)
            nh = val;
         else if (strcmp(key, OpalVideoFormat::FrameTimeOption) == 0)
           targetFrameTimeMs = val;
        }
      }
      free(_options);
      SetFrameSize(nw,nh);
      return true;
    } else {
       PTRACE(4,"PLUGIN\tUnable to set format options in codec");
       return false;
    }
}

PBoolean H323PluginVideoCodec::Read(BYTE * /*buffer*/, unsigned & length, RTP_DataFrame & dst)
{
    PWaitAndSignal mutex(videoHandlerActive);

    if (direction != Encoder) {
        PTRACE(1, "Plugin\tAttempt to decode from encoder");
        return FALSE;
    }

    if (rawDataChannel == NULL) {
        PTRACE(1, "PLUGIN\tNo channel to grab from, close down video transmission thread");
        return FALSE;
    }

    PVideoChannel *videoIn = (PVideoChannel *)rawDataChannel;


#ifdef H323_MEDIAENCODED
    if (videoIn->SourceEncoded(lastPacketSent,length)) {
        int maxDataSize = 1518-14-4-8-20-16; // Max Ethernet packet (1518 bytes) minus 802.3/CRC, 802.3, IP, UDP headers
        dst.SetMinSize(maxDataSize);

        if (rawDataChannel->Read(dst.GetPayloadPtr(), length)) {
           dst.SetMarker(lastPacketSent);
           return true;
        } else
           return false;
    }
#endif

    PluginCodec_Video_FrameHeader * frameHeader = (PluginCodec_Video_FrameHeader *)bufferRTP.GetPayloadPtr();
    if (!frameHeader) {
        PTRACE(1,"PLUGIN\tCould not locate frame header, close down video transmission thread");
        return false;
    }

    frameHeader->x = 0;
    frameHeader->y = 0;
    frameHeader->width        = videoIn->GetGrabWidth();
    frameHeader->height       = videoIn->GetGrabHeight();

    if (frameHeader->width == 0 || frameHeader->height == 0) {
        PTRACE(1,"PLUGIN\tVideo grab dimension is 0, close down video transmission thread");
        return false;
    }


    if (lastPacketSent) {
        videoIn->RestrictAccess();

        if (!videoIn->IsGrabberOpen()) {
            PTRACE(1, "PLUGIN\tVideo grabber is not initialised, close down video transmission thread");
            videoIn->EnableAccess();
            return FALSE;
        }

#if PTLIB_VER >= 290
        if (flowRequest && lastFrameTimeRTP) {
            PStringArray options;
            if (videoIn->FlowControl((void *)&options)    // test if implemented with empty options
                && SetFlowControl(codec,context,mediaFormat, flowRequest)) {
                PTRACE(4, "PLUGIN\tApplying Flow Control " << flowRequest);
                options = LoadInputDeviceOptions(mediaFormat);
                if (videoIn->FlowControl((void *)&options)) {
                    frameHeader->width  = videoIn->GetGrabWidth();
                    frameHeader->height = videoIn->GetGrabHeight();
                    sendIntra = true;  // Send a FPU when setting flow control.
                }
            } else if (videoIn->GetVideoReader() && videoIn->GetVideoReader()->GetCaptureMode() == 0) {
                    frameHeader->width  = videoIn->GetGrabWidth();
                    frameHeader->height = videoIn->GetGrabHeight();
            }
            flowRequest = 0;
        }
#endif

        if (!SetFrameSize(frameHeader->width, frameHeader->height)) {
            PTRACE(1, "PLUGIN\tFailed to resize, close down video transmission thread");
            videoIn->EnableAccess();
            return FALSE;
        }

        unsigned char * data = OPAL_VIDEO_FRAME_DATA_PTR(frameHeader);
        if (!rawDataChannel->Read(data, bytesPerFrame)) {
            PTRACE(3, "PLUGIN\tFailed to read data from video grabber");
            videoIn->EnableAccess();
            length=0;
            dst.SetPayloadSize(0);
            return TRUE; // and hope the error condition will fix itself
        }

        videoIn->EnableAccess();

        RenderFrame(data, NULL);

        nowFrameTick = PTimer::Tick().GetMilliSeconds();
        lastFrameTimeRTP = (nowFrameTick - lastFrameTick)*90;
        lastFrameTick = nowFrameTick;
    }
    else
        lastFrameTimeRTP = 0;

#if 0
    // get the size of the output buffer
    if (!CallCodecControl(codec, context, GET_OUTPUT_DATA_SIZE_CONTROL, NULL, NULL, outputDataSize))
        return false;
#endif

    dst.SetMinSize(outputDataSize);
    bytesPerFrame = outputDataSize;

    fromLen = bufferSize;
    toLen = outputDataSize;
    flags = sendIntra ? PluginCodec_CoderForceIFrame : 0;

    pluginRetVal = (codec->codecFunction)(codec, context,
                                        bufferRTP.GetPointer(), &fromLen,
                                        dst.GetPointer(), &toLen,
                                        &flags);

    if (pluginRetVal == 0) {
        PTRACE(3,"PLUGIN\tError encoding frame from plugin " << codec->descr);
        length = 0;
        return FALSE;
    }

    if ((flags & PluginCodec_ReturnCoderIFrame) != 0) {
        PTRACE(sendIntra ? 3 : 5,"PLUGIN\tSent I-Frame" << (sendIntra ? ", in response to VideoFastUpdate" : ""));
        sendIntra = false;
    }

    if (toLen > 0)
        length = toLen - dst.GetHeaderSize();
    else
        length = 0;

    lastPacketSent = (flags & PluginCodec_ReturnCoderLastFrame);

    return TRUE;
}

PBoolean H323PluginVideoCodec::Open(H323Connection & connection) {

#ifdef H323_FRAMEBUFFER
    if (direction == Decoder && connection.HasVideoFrameBuffer())
        m_frameBuffer.SetCodec(this);
#endif
    return H323VideoCodec::Open(connection);
}

PBoolean H323PluginVideoCodec::Write(const BYTE * buffer, unsigned length, const RTP_DataFrame & src, unsigned & written)
{
#ifdef H323_FRAMEBUFFER
    if (m_frameBuffer.IsRunning()) {
        if (m_frameBuffer.FrameIn(src.GetSequenceNumber(), src.GetTimestamp(), src.GetMarker(), src.GetPayloadSize(), src)) {
            written = length;
            return true;
        } else
            return false;
    } else
#endif
    {
        rtpInformation.m_recvTime = PTimer::Tick().GetMilliSeconds();
        rtpInformation.m_timeStamp = src.GetTimestamp();
        rtpInformation.m_clockRate = 90000;
        CalculateRTPSendTime(src.GetTimestamp(), rtpInformation.m_clockRate, rtpInformation.m_sendTime);
        rtpInformation.m_frame = &src;

        return WriteInternal(buffer, length, src, written, rtpInformation);
    }
}

PBoolean H323PluginVideoCodec::WriteInternal(const BYTE * /*buffer*/, unsigned length, const RTP_DataFrame & src,
                                             unsigned & written, H323_RTPInformation & rtp)
{
  PWaitAndSignal mutex(videoHandlerActive);

  if (direction != Decoder) {
    PTRACE(1, "PLUGIN\tAttempt to decode from decoder");
    return FALSE;
  }

  if (rawDataChannel == NULL) {
    PTRACE(1, "PLUGIN\tNo channel to render to, close down video reception thread");
    return FALSE;
  }

  if (length == 0) {
    written = length;
    return TRUE;
  }

  rtp.m_sessionID = rtpInformation.m_sessionID;

#ifdef H323_MEDIAENCODED
  if (((PVideoChannel *)rawDataChannel)->DisableDecode()) {
      if (RenderFrame(src.GetPayloadPtr(), &rtp)) {
         written = length; // pretend we wrote the data, to avoid error message
         return TRUE;
      } else
         return FALSE;
  }
#endif

#if 0
  // get the size of the output buffer
  if (!CallCodecControl(codec, context, GET_OUTPUT_DATA_SIZE_CONTROL, NULL, NULL, outputDataSize))
    return FALSE;
#endif

  bufferRTP.SetMinSize(outputDataSize);
  bytesPerFrame = outputDataSize;

#ifdef H323_PACKET_TRACE
  CheckPacket(false,src);
#endif

  fromLen = src.GetHeaderSize() + src.GetPayloadSize();
  toLen = bufferSize;
  flags=0;

  pluginRetVal = (codec->codecFunction)(codec, context,
                              (const BYTE *)src, &fromLen,
                              bufferRTP.GetPointer(toLen), &toLen,
                              &flags);

  for(;;) {
      if (!pluginRetVal) {
        PTRACE(3,"PLUGIN\tError decoding frame from plugin " << codec->descr);
        return FALSE;
      }

      if (sendIntra || (flags & PluginCodec_ReturnCoderRequestIFrame)) {
        nowFUPTick = PTimer::Tick().GetMilliSeconds();
        if ((nowFUPTick - lastFUPTick)  > FASTPICTUREINTERVAL) {
            PTRACE(6,"PLUGIN\tIFrame Request Decoder.");
            logicalChannel->SendMiscCommand(H245_MiscellaneousCommand_type::e_videoFastUpdatePicture);
            lastFUPTick = nowFUPTick;
            sendIntra = false;
        }
      }

      if (flags & PluginCodec_ReturnCoderLastFrame) {
        PluginCodec_Video_FrameHeader * header = (PluginCodec_Video_FrameHeader *)(bufferRTP.GetPayloadPtr());
        if (!header)
            return false;

        if (!SetFrameSize(header->width,header->height))
           return false;

        if (!RenderFrame(OPAL_VIDEO_FRAME_DATA_PTR(header), &rtp))
            return false;

        if(flags & PluginCodec_ReturnCoderMoreFrame) {
           PTRACE(6,"PLUGIN\tMore Frames to decode");
           flags=0;
           pluginRetVal = (codec->codecFunction)(codec, context,
                              (const BYTE *)0, &fromLen,
                              bufferRTP.GetPointer(toLen), &toLen,
                              &flags);
        } else
            break;

     } else if (toLen < (unsigned)PLUGIN_RTP_HEADER_SIZE) {  // No Payload
         PTRACE(6,"PLUGIN\tPartial Frame received " << codec->descr << " Ignoring rendering.");
         written = length;
         return TRUE;
     } else
        break;
  }

  written = length;

  return TRUE;
}

PBoolean H323PluginVideoCodec::RenderFrame(const BYTE * buffer, void * mark)
{
#ifdef H323_PACKET_TRACE
    H323_RTPInformation * rtp = (H323_RTPInformation * )mark;
    if (rtp && rtp->m_frame) CheckPacket(false, *(rtp->m_frame));
#endif
    return RenderInternal(buffer,mark);
}

PBoolean H323PluginVideoCodec::RenderInternal(const BYTE * buffer, void * mark)
{
    PVideoChannel *videoOut = (PVideoChannel *)rawDataChannel;

    if (!videoOut || !videoOut->IsRenderOpen())
        return TRUE;

#if PTLIB_VER >= 290
    videoOut->SetRenderFrameSize(frameWidth, frameHeight,sarWidth,sarHeight);
#else
    videoOut->SetRenderFrameSize(frameWidth, frameHeight);
#endif

    PTRACE(6, "PLUGIN\tWrite data to video renderer");
#if PTLIB_VER < 290
    return videoOut->Write(buffer, 0);
#else
    return videoOut->Write(buffer, 0, mark);
#endif
}

PBoolean H323PluginVideoCodec::SetFrameSize(int _width, int _height)
{
    return SetFrameSize(_width, _height, 1, 1);
}

PBoolean H323PluginVideoCodec::SetFrameSize(int _width, int _height,int _sar_width, int _sar_height)
{
    if ((frameWidth == _width) && (frameHeight == _height))
        return TRUE;

    if ((_width == 0) || (_height == 0))
        return FALSE;

    // TODO MAN: this might not be codec for non H.264 codec
    // TODO Need to set the actual MaxWidth and MaxHeight For now qwe accept any size recev'd
    //if ((_width*_height > maxWidth * maxHeight))
    //{
    //    PTRACE(3, "PLUGIN\tERROR: Frame Size " << _width << "x" << _height  << " exceeds codec limits (" << maxWidth << "x" << maxHeight << ")");
    //    return FALSE;
    //}

    mediaFormat.SetOptionInteger(OpalVideoFormat::FrameWidthOption,_width);
    mediaFormat.SetOptionInteger(OpalVideoFormat::FrameHeightOption,_height);
    if (_width * _height > frameWidth * frameHeight)
        UpdatePluginOptions(codec,context,GetWritableMediaFormat());

    frameWidth  = _width;
    frameHeight = _height;
    sarWidth    = _sar_width;
    sarHeight   = _sar_height;

    PTRACE(3,"PLUGIN\tResize to w:" << frameWidth << " h:" << frameHeight);

    bytesPerFrame = (frameHeight * frameWidth * 3)/2;

    if (direction == Encoder) {
        bufferRTP.SetPayloadSize(sizeof(PluginCodec_Video_FrameHeader) + bytesPerFrame);
        PluginCodec_Video_FrameHeader * header =
                    (PluginCodec_Video_FrameHeader *)(bufferRTP.GetPayloadPtr());
        header->x = header->y = 0;
        header->width         = frameWidth;
        header->height        = frameHeight;
    }

    return TRUE;
}

unsigned H323PluginVideoCodec::GetVideoMode(void)
{
   if (mediaFormat.GetOptionBoolean(OpalVideoFormat::DynamicVideoQualityOption))
      return H323VideoCodec::DynamicVideoQuality;
   else if (mediaFormat.GetOptionBoolean(OpalVideoFormat::AdaptivePacketDelayOption))
      return H323VideoCodec::AdaptivePacketDelay;
   else
      return H323VideoCodec::None;
}

void H323PluginVideoCodec::SetVideoMode(int mode)
{

    switch (mode) {
      case H323VideoCodec::DynamicVideoQuality :
        SetCodecControl(codec, context, SET_CODEC_OPTIONS_CONTROL, "Dynamic Video Quality", mode);
        break;
      case H323VideoCodec::AdaptivePacketDelay :
        SetCodecControl(codec, context, SET_CODEC_OPTIONS_CONTROL, "Adaptive Packet Delay", mode);
        break;
      default:
        break;
     }
}



#endif // H323_VIDEO

//////////////////////////////////////////////////////////////////////////////
//
// Helper class for handling plugin capabilities
//

class H323PluginCapabilityInfo
{
  public:
    H323PluginCapabilityInfo(PluginCodec_Definition * _encoderCodec,
                             PluginCodec_Definition * _decoderCodec);

    H323PluginCapabilityInfo(const PString & _mediaFormat,
                             const PString & _baseName);

    const PString & GetFormatName() const
    { return capabilityFormatName; }

    H323Codec * CreateCodec(const OpalMediaFormat & mediaFormat,
                            H323Codec::Direction direction,
                            const H323Capability * cap = NULL
                            ) const;

  protected:
    PluginCodec_Definition * encoderCodec;
    PluginCodec_Definition * decoderCodec;
    PString                  capabilityFormatName;
    PString                  mediaFormatName;
};

#ifndef NO_H323_AUDIO

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling most audio plugin capabilities
//

class H323AudioPluginCapability : public H323AudioCapability,
                                  public H323PluginCapabilityInfo
{
  PCLASSINFO(H323AudioPluginCapability, H323AudioCapability);
  public:
    H323AudioPluginCapability(PluginCodec_Definition * _encoderCodec,
                         PluginCodec_Definition * _decoderCodec,
                         unsigned _pluginSubType)
      : H323AudioCapability(_decoderCodec->parm.audio.maxFramesPerPacket, _encoderCodec->parm.audio.recommendedFramesPerPacket),
        H323PluginCapabilityInfo(_encoderCodec, _decoderCodec),
        pluginSubType(_pluginSubType), h323subType(0)
      {
          rtpPayloadType = (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ?
                                                                                    RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload);
      }

    // this constructor is only used when creating a capability without a codec
    H323AudioPluginCapability(const PString & _mediaFormat, const PString & _baseName,
                         unsigned maxFramesPerPacket, unsigned recommendedFramesPerPacket,
                         unsigned _pluginSubType)
      : H323AudioCapability(maxFramesPerPacket, recommendedFramesPerPacket),
        H323PluginCapabilityInfo(_mediaFormat, _baseName),
        pluginSubType(_pluginSubType), h323subType(0)
      {
#ifdef H323_AUDIO_CODECS
        for (PINDEX i = 0; audioMaps[i].pluginCapType >= 0; i++) {
          if (audioMaps[i].pluginCapType == (int)_pluginSubType) {
            h323subType = audioMaps[i].h323SubType;
            break;
          }
        }
#endif
        rtpPayloadType = OpalMediaFormat(_mediaFormat).GetPayloadType();
      }

    virtual PObject * Clone() const
    { return new H323AudioPluginCapability(*this); }

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}

    virtual H323Codec * CreateCodec(H323Codec::Direction direction) const
    { return H323PluginCapabilityInfo::CreateCodec(GetMediaFormat(), direction); }

    virtual unsigned GetSubType() const
    { return pluginSubType; }

  protected:
    unsigned pluginSubType;
    unsigned h323subType;   // only set if using capability without codec
};

#ifdef H323_AUDIO_CODECS

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling non standard audio capabilities
//

class H323CodecPluginNonStandardAudioCapability : public H323NonStandardAudioCapability,
                                                  public H323PluginCapabilityInfo
{
  PCLASSINFO(H323CodecPluginNonStandardAudioCapability, H323NonStandardAudioCapability);
  public:
    H323CodecPluginNonStandardAudioCapability(
                                   PluginCodec_Definition * _encoderCodec,
                                   PluginCodec_Definition * _decoderCodec,
                                   H323NonStandardCapabilityInfo::CompareFuncType compareFunc,
                                   const unsigned char * data, unsigned dataLen);

    H323CodecPluginNonStandardAudioCapability(
                                   PluginCodec_Definition * _encoderCodec,
                                   PluginCodec_Definition * _decoderCodec,
                                   const unsigned char * data, unsigned dataLen);

    virtual PObject * Clone() const
    { return new H323CodecPluginNonStandardAudioCapability(*this); }

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}

    virtual H323Codec * CreateCodec(H323Codec::Direction direction) const
    { return H323PluginCapabilityInfo::CreateCodec(GetMediaFormat(), direction); }
};


//////////////////////////////////////////////////////////////////////////////
//
// Class for handling generic audio capabilities
//

class H323CodecPluginGenericAudioCapability : public H323GenericAudioCapability,
                          public H323PluginCapabilityInfo
{
  PCLASSINFO(H323CodecPluginGenericAudioCapability, H323GenericAudioCapability);
  public:
    H323CodecPluginGenericAudioCapability(
                                   const PluginCodec_Definition * _encoderCodec,
                                   const PluginCodec_Definition * _decoderCodec,
                   const PluginCodec_H323GenericCodecData * data );

    virtual PObject * Clone() const
    {
      return new H323CodecPluginGenericAudioCapability(*this);
    }

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}

    virtual H323Codec * CreateCodec(H323Codec::Direction direction) const
    { return H323PluginCapabilityInfo::CreateCodec(GetMediaFormat(), direction); }
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling G.723.1 codecs
//

class H323PluginG7231Capability : public H323AudioPluginCapability
{
  PCLASSINFO(H323PluginG7231Capability, H323AudioPluginCapability);
  public:
    H323PluginG7231Capability(PluginCodec_Definition * _encoderCodec,
                               PluginCodec_Definition * _decoderCodec,
                               PBoolean _annexA = TRUE)
      : H323AudioPluginCapability(_encoderCodec, _decoderCodec, H245_AudioCapability::e_g7231),
        annexA(_annexA)
      { }

    Comparison Compare(const PObject & obj) const
    {
      if (!PIsDescendant(&obj, H323PluginG7231Capability))
        return LessThan;

      Comparison result = H323AudioCapability::Compare(obj);
      if (result != EqualTo)
        return result;

      PBoolean otherAnnexA = ((const H323PluginG7231Capability &)obj).annexA;
      if (annexA == otherAnnexA)
        return EqualTo;

      if (annexA)
        return GreaterThan;

      return EqualTo;
    }

    virtual PObject * Clone() const
    {
      return new H323PluginG7231Capability(*this);
    }

    virtual PBoolean OnSendingPDU(H245_AudioCapability & cap, unsigned packetSize) const
    {
      cap.SetTag(H245_AudioCapability::e_g7231);
      H245_AudioCapability_g7231 & g7231 = cap;
      g7231.m_maxAl_sduAudioFrames = packetSize;
      g7231.m_silenceSuppression = annexA;
      return TRUE;
    }

    virtual PBoolean OnReceivedPDU(const H245_AudioCapability & cap,  unsigned & packetSize)
    {
      if (cap.GetTag() != H245_AudioCapability::e_g7231)
        return FALSE;
      const H245_AudioCapability_g7231 & g7231 = cap;
      packetSize = g7231.m_maxAl_sduAudioFrames;
      annexA = g7231.m_silenceSuppression;
      return TRUE;
    }

  protected:
    PBoolean annexA;
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling GSM plugin capabilities
//

class H323GSMPluginCapability : public H323AudioPluginCapability
{
  PCLASSINFO(H323GSMPluginCapability, H323AudioPluginCapability);
  public:
    H323GSMPluginCapability(PluginCodec_Definition * _encoderCodec,
                            PluginCodec_Definition * _decoderCodec,
                            int _pluginSubType, int _comfortNoise, int _scrambled)
      : H323AudioPluginCapability(_encoderCodec, _decoderCodec, _pluginSubType),
        comfortNoise(_comfortNoise), scrambled(_scrambled)
    { }

    Comparison Compare(const PObject & obj) const;

    virtual PObject * Clone() const
    {
      return new H323GSMPluginCapability(*this);
    }

    virtual PBoolean OnSendingPDU(
      H245_AudioCapability & pdu,  /// PDU to set information on
      unsigned packetSize          /// Packet size to use in capability
    ) const;

    virtual PBoolean OnReceivedPDU(
      const H245_AudioCapability & pdu,  /// PDU to get information from
      unsigned & packetSize              /// Packet size to use in capability
    );
  protected:
    int comfortNoise;
    int scrambled;
};

#endif // NO_H323_AUDIO_CODECS

#endif // NO_H323_AUDIO

#ifdef  H323_VIDEO

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling most video plugin capabilities
//

class H323VideoPluginCapability : public H323VideoCapability,
                                  public H323PluginCapabilityInfo
{
  PCLASSINFO(H323VideoPluginCapability, H323VideoCapability);
  public:
    H323VideoPluginCapability(PluginCodec_Definition * _encoderCodec,
                              PluginCodec_Definition * _decoderCodec,
                              unsigned _pluginSubType)
      : H323VideoCapability(),
           H323PluginCapabilityInfo(_encoderCodec, _decoderCodec),
        pluginSubType(_pluginSubType)
      {
        SetCommonOptions(GetWritableMediaFormat(),encoderCodec->parm.video.maxFrameWidth,encoderCodec->parm.video.maxFrameHeight, encoderCodec->parm.video.recommendedFrameRate);
        PopulateMediaFormatOptions(encoderCodec,GetWritableMediaFormat());

        rtpPayloadType = (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ? RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload);
      }

#if 0
    // this constructor is only used when creating a capability without a codec
    H323VideoPluginCapability(const PString & _mediaFormat, const PString & _baseName,
                         unsigned maxFramesPerPacket, unsigned /*recommendedFramesPerPacket*/,
                         unsigned _pluginSubType)
      : H323VideoCapability(),
        H323PluginCapabilityInfo(_mediaFormat, _baseName),
        pluginSubType(_pluginSubType)
      {
        for (PINDEX i = 0; audioMaps[i].pluginCapType >= 0; i++) {
          if (videoMaps[i].pluginCapType == (int)_pluginSubType) {
            h323subType = audioMaps[i].h323SubType;
            break;
          }
        }
        rtpPayloadType = OpalMediaFormat(_mediaFormat).GetPayloadType();
      }
#endif

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}

    virtual unsigned GetSubType() const
    { return pluginSubType; }


    static PBoolean SetCommonOptions(OpalMediaFormat & mediaFormat, int frameWidth, int frameHeight, int frameRate)
    {
        if (!mediaFormat.SetOptionInteger(OpalVideoFormat::FrameWidthOption, frameWidth)) {
          // PTRACE(3,"PLUGIN Error setting " << OpalVideoFormat::FrameWidthOption << " to " << frameWidth);   BUG in PTLIB v2.11?  SH
           return FALSE;
        }

        if (!mediaFormat.SetOptionInteger(OpalVideoFormat::FrameHeightOption, frameHeight)) {
         //  PTRACE(3,"PLUGIN Error setting " << OpalVideoFormat::FrameHeightOption << " to " << frameHeight);  BUG in PTLIB v2.11?  SH
           return FALSE;
        }

        if (!mediaFormat.SetOptionInteger(OpalVideoFormat::FrameTimeOption, (int)(OpalMediaFormat::VideoTimeUnits * 1000 * 100 * frameRate / 2997))){
         //  PTRACE(3,"PLUGIN Error setting " << OpalVideoFormat::FrameTimeOption << " to " << (int)(OpalMediaFormat::VideoTimeUnits * 100 * frameRate / 2997));  BUG in PTLIB v2.11? SH
           return FALSE;
        }

      return TRUE;
    }

     virtual PBoolean SetMaxFrameSize(CapabilityFrameSize framesize, int frameunits = 1)
     {
        const OpalMediaFormat & mediaFormat = GetMediaFormat();
        int sqcifMPI = mediaFormat.GetOptionInteger(sqcifMPI_tag);
        int qcifMPI  = mediaFormat.GetOptionInteger(qcifMPI_tag);
        int cifMPI   = mediaFormat.GetOptionInteger(cifMPI_tag);
        int cif4MPI  = mediaFormat.GetOptionInteger(cif4MPI_tag);
        int cif16MPI = mediaFormat.GetOptionInteger(cif16MPI_tag);

         PString param;
         int w=0; int h=0;
         switch (framesize) {
             case H323Capability::sqcifMPI :
                sqcifMPI =frameunits; qcifMPI=0; cifMPI=0; cif4MPI=0; cif16MPI=0;
                w = SQCIF_WIDTH; h = SQCIF_HEIGHT;
                break;
             case H323Capability::qcifMPI :
                  if (qcifMPI==0 || cifMPI==0 || cif4MPI==0 || cif16MPI==0) return true;
                 qcifMPI =frameunits; cifMPI=0; cif4MPI=0; cif16MPI=0;
                 w = QCIF_WIDTH; h = QCIF_HEIGHT;
                 break;
             case H323Capability::cifMPI :
                  if (cifMPI==0 || cif4MPI==0 || cif16MPI==0) return true;
                 cifMPI =frameunits; cif4MPI=0; cif16MPI=0;
                 w = CIF_WIDTH; h = CIF_HEIGHT;
                 break;
             case H323Capability::cif4MPI :
             case H323Capability::i480MPI :
                 if (cif4MPI==0 || cif16MPI==0) return true;
                 cif4MPI =frameunits; cif16MPI=0;
                 w = CIF4_WIDTH; h = CIF4_HEIGHT;
                 break;
             case H323Capability::cif16MPI :
             case H323Capability::p720MPI :
                  if (cif16MPI==0) return true;
                  w = CIF16_WIDTH; h = CIF16_HEIGHT;
                 break;
             default: return false;
         }

         OpalMediaFormat & fmt = GetWritableMediaFormat();
         fmt.SetOptionInteger(sqcifMPI_tag, sqcifMPI);
         fmt.SetOptionInteger(qcifMPI_tag,  qcifMPI );
         fmt.SetOptionInteger(cifMPI_tag,   cifMPI  );
         fmt.SetOptionInteger(cif4MPI_tag,  cif4MPI );
         fmt.SetOptionInteger(cif16MPI_tag, cif16MPI);

         fmt.SetOptionInteger(OpalVideoFormat::FrameWidthOption,w);
         fmt.SetOptionInteger(OpalVideoFormat::FrameHeightOption,h);
         return true;
     }

    virtual H323Codec * CreateCodec(H323Codec::Direction direction) const
    { return H323PluginCapabilityInfo::CreateCodec(GetMediaFormat(), direction, this); }

  protected:
    unsigned pluginSubType;
#if 0
    unsigned h323subType;   // only set if using capability without codec
#endif
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling non standard video capabilities
//

class H323CodecPluginNonStandardVideoCapability : public H323NonStandardVideoCapability,
                                                  public H323PluginCapabilityInfo
{
  PCLASSINFO(H323CodecPluginNonStandardVideoCapability, H323NonStandardVideoCapability);
  public:
    H323CodecPluginNonStandardVideoCapability(
                                   PluginCodec_Definition * _encoderCodec,
                                   PluginCodec_Definition * _decoderCodec,
                                   H323NonStandardCapabilityInfo::CompareFuncType compareFunc,
                                   const unsigned char * data, unsigned dataLen);

    H323CodecPluginNonStandardVideoCapability(
                                   PluginCodec_Definition * _encoderCodec,
                                   PluginCodec_Definition * _decoderCodec,
                                   const unsigned char * data, unsigned dataLen);

    virtual PObject * Clone() const
    { return new H323CodecPluginNonStandardVideoCapability(*this); }

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}

    virtual H323Codec * CreateCodec(H323Codec::Direction direction) const
    { return H323PluginCapabilityInfo::CreateCodec(GetMediaFormat(), direction); }
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling generic video capabilities ie H.264 / MPEG4 part 2
//

class H323CodecPluginGenericVideoCapability : public H323GenericVideoCapability,
                                              public H323PluginCapabilityInfo
{
  PCLASSINFO(H323CodecPluginGenericVideoCapability, H323GenericVideoCapability);
  public:
    H323CodecPluginGenericVideoCapability(
                                   const PluginCodec_Definition * _encoderCodec,
                                   const PluginCodec_Definition * _decoderCodec,
                                   const PluginCodec_H323GenericCodecData * data );

    virtual PObject * Clone() const
    { return new H323CodecPluginGenericVideoCapability(*this); }

    virtual PString GetFormatName() const
    { return H323PluginCapabilityInfo::GetFormatName();}

    virtual H323Codec * CreateCodec(H323Codec::Direction direction) const
    { return H323PluginCapabilityInfo::CreateCodec(GetMediaFormat(), direction); }

    virtual void LoadGenericData(const PluginCodec_H323GenericCodecData *ptr);

    virtual PBoolean SetMaxFrameSize(CapabilityFrameSize framesize, int frameunits = 1);

    virtual PBoolean SetCustomEncode(unsigned width, unsigned height, unsigned rate);

  protected:
     OpalMediaFormat encoderMediaFormat;
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling H.261 plugin capabilities
//

class H323H261PluginCapability : public H323VideoPluginCapability
{
  PCLASSINFO(H323H261PluginCapability, H323VideoPluginCapability);
  public:
    H323H261PluginCapability(PluginCodec_Definition * _encoderCodec,
                             PluginCodec_Definition * _decoderCodec);

    Comparison Compare(const PObject & obj) const;

    virtual PObject * Clone() const
    {
      return new H323H261PluginCapability(*this);
    }

    virtual PBoolean OnSendingPDU(
      H245_VideoCapability & pdu  /// PDU to set information on
    ) const;

    virtual PBoolean OnSendingPDU(
      H245_VideoMode & pdu
    ) const;

    virtual PBoolean OnReceivedPDU(
      const H245_VideoCapability & pdu  /// PDU to get information from
    );

    PluginCodec_Definition * enc;
};

//////////////////////////////////////////////////////////////////////////////
//
// Class for handling H.263 plugin capabilities
//

class H323H263PluginCapability : public H323VideoPluginCapability
{
  PCLASSINFO(H323H263PluginCapability, H323VideoPluginCapability);
  public:
    H323H263PluginCapability(PluginCodec_Definition * _encoderCodec,
                             PluginCodec_Definition * _decoderCodec);

    Comparison Compare(const PObject & obj) const;

    virtual PObject * Clone() const
    { return new H323H263PluginCapability(*this); }

    virtual PBoolean IsMatch(const PASN_Choice & subTypePDU) const;

    virtual PBoolean OnSendingPDU(
      H245_VideoCapability & pdu  /// PDU to set information on
    ) const;

    virtual PBoolean OnSendingPDU(
      H245_VideoMode & pdu
    ) const;

    virtual PBoolean OnReceivedPDU(
      const H245_VideoCapability & pdu  /// PDU to get information from
    );
};

#endif //  H323_VIDEO

/////////////////////////////////////////////////////////////////////////////

class H323StaticPluginCodec
{
  public:
    virtual ~H323StaticPluginCodec() { }
    virtual PluginCodec_GetAPIVersionFunction Get_GetAPIFn() = 0;
    virtual PluginCodec_GetCodecFunction Get_GetCodecFn() = 0;
};

PMutex & H323PluginCodecManager::GetMediaFormatMutex()
{
  static PMutex mutex;
  return mutex;
}

H323PluginCodecManager::H323PluginCodecManager(PPluginManager * _pluginMgr)
 : PPluginModuleManager(PLUGIN_CODEC_GET_CODEC_FN_STR, _pluginMgr), m_skipRedefinitions(false)
{
  // this code runs before the application is able to set the trace level
  char * debug_level = getenv ("PTLIB_TRACE_CODECS");
  if (debug_level != NULL) {
    PTrace::SetLevel(atoi(debug_level));
  }
  // skip plugin codecs that re-define built-in codecs (default is to overwrite)
  char * codec_redefine = getenv ("PTLIB_SKIP_CODEC_REDEFINITION");
  if (codec_redefine != NULL) {
    m_skipRedefinitions = true;
  }

  // instantiate all of the media formats
  {
    OpalMediaFormatFactory::KeyList_T keyList = OpalMediaFormatFactory::GetKeyList();
    OpalMediaFormatFactory::KeyList_T::const_iterator r;
    for (r = keyList.begin(); r != keyList.end(); ++r) {
      OpalMediaFormat * instance = OpalMediaFormatFactory::CreateInstance(*r);
      if (instance == NULL) {
        PTRACE(4, "H323PLUGIN\tCannot instantiate opal media format " << *r);
      } else {
        PTRACE(4, "H323PLUGIN\tCreating media format " << *r);
      }
    }
  }

  // instantiate all of the static codecs
  {
    PFactory<H323StaticPluginCodec>::KeyList_T keyList = PFactory<H323StaticPluginCodec>::GetKeyList();
    PFactory<H323StaticPluginCodec>::KeyList_T::const_iterator r;
    for (r = keyList.begin(); r != keyList.end(); ++r) {
      H323StaticPluginCodec * instance = PFactory<H323StaticPluginCodec>::CreateInstance(*r);
      if (instance == NULL) {
        PTRACE(4, "H323PLUGIN\tCannot instantiate static codec plugin " << *r);
      } else {
        PTRACE(4, "H323PLUGIN\tLoading static codec plugin " << *r);
        RegisterStaticCodec(r->c_str(), instance->Get_GetAPIFn(), instance->Get_GetCodecFn());
      }
    }
  }

  // cause the plugin manager to load all dynamic plugins
  pluginMgr->AddNotifier(PCREATE_NOTIFIER(OnLoadModule), TRUE);
}

H323PluginCodecManager::~H323PluginCodecManager()
{
}

void H323PluginCodecManager::OnShutdown()
{
  // unregister the plugin media formats
  OpalMediaFormatFactory::UnregisterAll();

  // Unregister the codec factory
  OpalPluginCodecFactory::UnregisterAll();

#ifdef H323_VIDEO
#ifdef H323_H239
  H323ExtendedVideoFactory::UnregisterAll();
#endif
#endif
  // unregister the plugin capabilities
  H323CapabilityFactory::UnregisterAll();
}

void H323PluginCodecManager::OnLoadPlugin(PDynaLink & dll, INT code)
{
  PluginCodec_GetCodecFunction getCodecs;
  if (!dll.GetFunction(PString(signatureFunctionName), (PDynaLink::Function &)getCodecs)) {
    PTRACE(3, "H323PLUGIN\tPlugin Codec DLL " << dll.GetName() << " is not a plugin codec");
    return;
  }

  unsigned int count;
  PluginCodec_Definition * codecs = (*getCodecs)(&count, PLUGIN_CODEC_VERSION_OPTIONS);
  if (codecs == NULL || count == 0) {
    PTRACE(3, "H323PLUGIN\tPlugin Codec DLL " << dll.GetName() << " contains no codec definitions");
    return;
  }

  PTRACE(3, "H323PLUGIN\tLoading plugin codec " << dll.GetName());

  switch (code) {

    // plugin loaded
    case 0:
      RegisterCodecs(count, codecs);
      break;

    // plugin unloaded
    case 1:
      UnregisterCodecs(count, codecs);
      break;

    default:
      break;
  }
}

void H323PluginCodecManager::RegisterStaticCodec(
      const char * name,
      PluginCodec_GetAPIVersionFunction /*getApiVerFn*/,
      PluginCodec_GetCodecFunction getCodecFn)
{
  unsigned int count;
  PluginCodec_Definition * codecs = (*getCodecFn)(&count, PLUGIN_CODEC_VERSION_OPTIONS);
  if (codecs == NULL || count == 0) {
    PTRACE(3, "H323PLUGIN\tStatic codec " << name << " contains no codec definitions");
    return;
  }

  RegisterCodecs(count, codecs);
}

void H323PluginCodecManager::RegisterCodecs(unsigned int count, void * _codecList)
{
  // make sure all non-timestamped codecs have the same concept of "now"
//  static time_t codecNow = ::time(NULL);

  PluginCodec_Definition * codecList = (PluginCodec_Definition *)_codecList;
  unsigned i, j ;
  for (i = 0; i < count; i++) {

    PluginCodec_Definition & encoder = codecList[i];

    PBoolean videoSupported = encoder.version >= PLUGIN_CODEC_VERSION_VIDEO;

    // for every encoder, we need a decoder
    PBoolean found = FALSE;
    PBoolean isEncoder = FALSE;
    if ((encoder.h323CapabilityType != PluginCodec_H323Codec_undefined) &&
         ((
           ((encoder.flags & PluginCodec_MediaTypeMask) == PluginCodec_MediaTypeAudio) &&
            (strcmp(encoder.sourceFormat, "L16") == 0)
         ) ||
         (
           ((encoder.flags & PluginCodec_MediaTypeMask) == PluginCodec_MediaTypeAudioStreamed) &&
            (strcmp(encoder.sourceFormat, "L16") == 0)
         ) ||
         (
           videoSupported &&
           (((encoder.flags & PluginCodec_MediaTypeMask) == PluginCodec_MediaTypeVideo) ||
           ((encoder.flags & PluginCodec_MediaTypeMask) == PluginCodec_MediaTypeExtended)) &&
            (strcmp(encoder.sourceFormat, "YUV420P") == 0)
         )
        )
       ) {
      isEncoder = TRUE;
      for (j = 0; j < count; j++) {

        PluginCodec_Definition & decoder = codecList[j];
        if (
            (decoder.h323CapabilityType == encoder.h323CapabilityType) &&
            ((decoder.flags & PluginCodec_MediaTypeMask) == (encoder.flags & PluginCodec_MediaTypeMask)) &&
            (strcmp(decoder.sourceFormat, encoder.destFormat) == 0) &&
            (strcmp(decoder.destFormat,   encoder.sourceFormat) == 0)
            )
          {
/*
          // deal with codec having no info, or timestamp in future
          time_t timeStamp = codecList[i].info == NULL ? codecNow : codecList[i].info->timestamp;
          if (timeStamp > codecNow)
            timeStamp = codecNow;
*/
          // create the capability and media format associated with this plugin
          CreateCapabilityAndMediaFormat(&encoder, &decoder);
          found = TRUE;

          PTRACE(5, "H323PLUGIN\tPlugin codec " << encoder.descr << " defined");
          break;
        }
      }
    }
    if (!found && isEncoder) {
      PTRACE(2, "H323PLUGIN\tCannot find decoder for plugin encoder " << encoder.descr);
    }
  }
}

void H323PluginCodecManager::UnregisterCodecs(unsigned int /*count*/, void * /*codec*/)
{
}

void H323PluginCodecManager::AddFormat(OpalMediaFormat * fmt)
{
  PWaitAndSignal m(H323PluginCodecManager::GetMediaFormatMutex());
  H323PluginCodecManager::GetMediaFormatList().Append(fmt);
}

void H323PluginCodecManager::AddFormat(const OpalMediaFormat & fmt)
{
  PWaitAndSignal m(H323PluginCodecManager::GetMediaFormatMutex());
  H323PluginCodecManager::GetMediaFormatList().Append(new OpalMediaFormat(fmt));
}

OpalMediaFormat::List H323PluginCodecManager::GetMediaFormats()
{
  return GetMediaFormatList();
}

OpalMediaFormat::List & H323PluginCodecManager::GetMediaFormatList()
{
  static OpalMediaFormat::List mediaFormatList;
  return mediaFormatList;
}


void H323PluginCodecManager::CreateCapabilityAndMediaFormat(
       PluginCodec_Definition * encoderCodec,
       PluginCodec_Definition * decoderCodec
)
{
  // make sure all non-timestamped codecs have the same concept of "now"
  static time_t mediaNow = time(NULL);

  // deal with codec having no info, or timestamp in future
  time_t timeStamp = encoderCodec->info == NULL ? mediaNow : encoderCodec->info->timestamp;
  if (timeStamp > mediaNow)
    timeStamp = mediaNow;

  unsigned defaultSessionID = 0;
  PBoolean jitter = FALSE;
#ifdef H323_VIDEO
  PBoolean extended = FALSE;
  PBoolean extendedOnly = FALSE;
#endif
  unsigned frameTime = 0;
  unsigned timeUnits = 0;
  switch (encoderCodec->flags & PluginCodec_MediaTypeMask) {
#ifdef H323_VIDEO
    case PluginCodec_MediaTypeVideo:
      defaultSessionID = OpalMediaFormat::DefaultVideoSessionID;
      switch (encoderCodec->flags & PluginCodec_MediaExtensionMask) {
         case PluginCodec_MediaTypeExtVideo:
         case PluginCodec_MediaTypeH239:
             extended = TRUE;
             break;
         default:
             break;
      }
      jitter = FALSE;
      break;

    case PluginCodec_MediaTypeExtended:
      extendedOnly = TRUE;
      defaultSessionID = OpalMediaFormat::DefaultExtVideoSessionID;
      jitter = FALSE;
      break;
#endif
#ifndef NO_H323_AUDIO
    case PluginCodec_MediaTypeAudio:
    case PluginCodec_MediaTypeAudioStreamed:
      defaultSessionID = OpalMediaFormat::DefaultAudioSessionID;
      jitter = TRUE;
      frameTime = encoderCodec->usPerFrame / 1000;
      timeUnits = encoderCodec->sampleRate / 1000;
      break;
#endif
    default:
      break;
  }

  // add the media format
  if (defaultSessionID == 0) {
    PTRACE(3, "H323PLUGIN\tCodec DLL provides unknown media format " << (int)(encoderCodec->flags & PluginCodec_MediaTypeMask));
  } else {
    PString fmtName = CreateCodecName(encoderCodec, FALSE);
    OpalMediaFormat existingFormat(fmtName, TRUE);
    if (existingFormat.IsValid()) {
      PTRACE(5, "H323PLUGIN\tMedia format " << fmtName << " already exists");
      if (m_skipRedefinitions) {
        PTRACE(3, "H323PLUGIN\tSkipping new definition of " << fmtName);
        return;
      }
      H323PluginCodecManager::AddFormat(existingFormat);
    } else {
      PTRACE(5, "H323PLUGIN\tCreating new media format " << fmtName);

      OpalMediaFormat * mediaFormat = NULL;

      // manually register the new singleton type, as we do not have a concrete type
      switch (encoderCodec->flags & PluginCodec_MediaTypeMask) {
#ifdef H323_VIDEO
        case PluginCodec_MediaTypeVideo:
        case PluginCodec_MediaTypeExtended:
          mediaFormat = new OpalPluginVideoMediaFormat(
                                  encoderCodec,
                                  encoderCodec->sdpFormat,
                                  timeStamp);
          break;
#endif
#ifndef NO_H323_AUDIO
        case PluginCodec_MediaTypeAudio:
        case PluginCodec_MediaTypeAudioStreamed:
          mediaFormat = new OpalPluginAudioMediaFormat(
                                   encoderCodec,
                                   defaultSessionID,
                                   jitter,
                                   frameTime,
                                   timeUnits,
                                   timeStamp);
          break;
#endif
        default:
          break;
      }
      // if the codec has been flagged to use a shared RTP payload type, then find a codec with the same SDP name
      // and use that RTP code rather than creating a new one. That prevents codecs (like Speex) from consuming
      // dozens of dynamic RTP types
      if ((encoderCodec->flags & PluginCodec_RTPTypeShared) != 0) {
        PWaitAndSignal m(H323PluginCodecManager::GetMediaFormatMutex());
        OpalMediaFormat::List & list = H323PluginCodecManager::GetMediaFormatList();
        for (PINDEX i = 0; i < list.GetSize(); i++) {
          OpalMediaFormat * opalFmt = &list[i];
#ifndef NO_H323_AUDIO
         {
          OpalPluginAudioMediaFormat * fmt = dynamic_cast<OpalPluginAudioMediaFormat *>(opalFmt);
          if (
               (encoderCodec->sdpFormat != NULL) &&
               (fmt != NULL) &&
               (fmt->encoderCodec->sdpFormat != NULL) &&
               (strcasecmp(encoderCodec->sdpFormat, fmt->encoderCodec->sdpFormat) == 0)
              ) {
            mediaFormat->SetPayloadType(fmt->GetPayloadType());
            break;
                }
            }
#endif
#ifdef H323_VIDEO
          {
            OpalPluginVideoMediaFormat * fmt = dynamic_cast<OpalPluginVideoMediaFormat *>(opalFmt);
            if (
                (fmt != NULL) &&
                (encoderCodec->sampleRate == fmt->encoderCodec->sampleRate) &&
                (fmt->encoderCodec->sdpFormat != NULL) && (encoderCodec->sdpFormat != NULL) &&
                (strcasecmp(encoderCodec->sdpFormat, fmt->encoderCodec->sdpFormat) == 0)
                ) {
              mediaFormat->SetPayloadType(fmt->GetPayloadType());
              break;
            }
          }
#endif
    }
  }

      // save the format
      H323PluginCodecManager::AddFormat(mediaFormat);
    }
  }

  // add the capability
  H323CodecPluginCapabilityMapEntry * map = NULL;

  switch (encoderCodec->flags & PluginCodec_MediaTypeMask) {
#ifdef H323_AUDIO_CODECS
    case PluginCodec_MediaTypeAudio:
    case PluginCodec_MediaTypeAudioStreamed:
      map = audioMaps;
      break;
#endif

#ifdef H323_VIDEO
    case PluginCodec_MediaTypeVideo:
    case PluginCodec_MediaTypeExtended:
      map = videoMaps;
      break;
#endif

    default:
      break;
  }

  if (map == NULL) {
    PTRACE(3, "H323PLUGIN\tCannot create capability for unknown plugin codec media format " << (int)(encoderCodec->flags & PluginCodec_MediaTypeMask));
  } else {
    for (PINDEX i = 0; map[i].pluginCapType >= 0; i++) {
      if (map[i].pluginCapType == encoderCodec->h323CapabilityType) {
        H323Capability * cap = NULL;
        if (map[i].createFunc != NULL)
          cap = (*map[i].createFunc)(encoderCodec, decoderCodec, map[i].h323SubType);
        else {
          switch (encoderCodec->flags & PluginCodec_MediaTypeMask) {
#ifndef NO_H323_AUDIO
            case PluginCodec_MediaTypeAudio:
            case PluginCodec_MediaTypeAudioStreamed:
              cap = new H323AudioPluginCapability(encoderCodec, decoderCodec, map[i].h323SubType);
              break;
#endif // NO_H323_AUDIO

#ifdef H323_VIDEO
            case PluginCodec_MediaTypeVideo:
            case PluginCodec_MediaTypeExtended:
              // all video caps are created using the create functions
              break;
#endif // H323_VIDEO

            default:
              break;
          }
        }

        // manually register the new singleton type, as we do not have a concrete type
        if (cap != NULL){
#ifdef H323_VIDEO
          if (!extendedOnly)
#endif
             H323CapabilityFactory::Register(CreateCodecName(encoderCodec, TRUE), cap);
#ifdef H323_H239
             if (extendedOnly || extended)
                H323ExtendedVideoFactory::Register(CreateCodecName(encoderCodec, FALSE), (H323VideoCapability *)cap);
#endif
        }
        break;
      }
    }
  }

  // create the factories for the codecs
  OpalPluginCodecFactory::Register(PString(encoderCodec->sourceFormat) + "|" + encoderCodec->destFormat, new OpalPluginCodec(encoderCodec));
  OpalPluginCodecFactory::Register(PString(decoderCodec->sourceFormat) + "|" + decoderCodec->destFormat, new OpalPluginCodec(decoderCodec));

}

/////////////////////////////////////////////////////////////////////////////



#ifdef H323_AUDIO_CODECS

H323Capability * CreateNonStandardAudioCap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int /*subType*/)
{
  PluginCodec_H323NonStandardCodecData * pluginData =  (PluginCodec_H323NonStandardCodecData *)encoderCodec->h323CapabilityData;
  if (pluginData == NULL) {
    return new H323CodecPluginNonStandardAudioCapability(
                             encoderCodec, decoderCodec,
                             (const unsigned char *)encoderCodec->descr,
                             strlen(encoderCodec->descr));
  }

  else if (pluginData->capabilityMatchFunction != NULL)
    return new H323CodecPluginNonStandardAudioCapability(encoderCodec, decoderCodec,
                             (H323NonStandardCapabilityInfo::CompareFuncType)pluginData->capabilityMatchFunction,
                             pluginData->data, pluginData->dataLength);
  else
    return new H323CodecPluginNonStandardAudioCapability(
                             encoderCodec, decoderCodec,
                             pluginData->data, pluginData->dataLength);
}

H323Capability *CreateGenericAudioCap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int /*subType*/)
{
    PluginCodec_H323GenericCodecData * pluginData = (PluginCodec_H323GenericCodecData *)encoderCodec->h323CapabilityData;

    if(pluginData == NULL ) {
    PTRACE(1, "Generic codec information for codec '"<<encoderCodec->descr<<"' has NULL data field");
    return NULL;
    }
    return new H323CodecPluginGenericAudioCapability(encoderCodec, decoderCodec, pluginData);
}

H323Capability * CreateG7231Cap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int /*subType*/)
{
  return new H323PluginG7231Capability(encoderCodec, decoderCodec, decoderCodec->h323CapabilityData != 0);
}


H323Capability * CreateGSMCap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int subType)
{
  PluginCodec_H323AudioGSMData * pluginData =  (PluginCodec_H323AudioGSMData *)encoderCodec->h323CapabilityData;
  return new H323GSMPluginCapability(encoderCodec, decoderCodec, subType, pluginData->comfortNoise, pluginData->scrambled);
}

#endif

#ifdef H323_VIDEO

H323Capability * CreateNonStandardVideoCap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int /*subType*/)
{
  PluginCodec_H323NonStandardCodecData * pluginData =  (PluginCodec_H323NonStandardCodecData *)encoderCodec->h323CapabilityData;
  if (pluginData == NULL) {
    return new H323CodecPluginNonStandardVideoCapability(
                             encoderCodec, decoderCodec,
                             (const unsigned char *)encoderCodec->descr,
                             strlen(encoderCodec->descr));
  }

  else if (pluginData->capabilityMatchFunction != NULL)
    return new H323CodecPluginNonStandardVideoCapability(encoderCodec, decoderCodec,
                             (H323NonStandardCapabilityInfo::CompareFuncType)pluginData->capabilityMatchFunction,
                             pluginData->data, pluginData->dataLength);
  else
    return new H323CodecPluginNonStandardVideoCapability(
                             encoderCodec, decoderCodec,
                             pluginData->data, pluginData->dataLength);
}

H323Capability *CreateGenericVideoCap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int /*subType*/)
{
  PluginCodec_H323GenericCodecData * pluginData = (PluginCodec_H323GenericCodecData *)encoderCodec->h323CapabilityData;

  if (pluginData == NULL ) {
      PTRACE(1, "Generic codec information for codec '"<<encoderCodec->descr<<"' has NULL data field");
      return NULL;
  }
  return new H323CodecPluginGenericVideoCapability(encoderCodec, decoderCodec, pluginData);
}


H323Capability * CreateH261Cap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int /*subType*/)
{
  return new H323H261PluginCapability(encoderCodec, decoderCodec);
}

H323Capability * CreateH263Cap(
  PluginCodec_Definition * encoderCodec,
  PluginCodec_Definition * decoderCodec,
  int /*subType*/)
{
  return new H323H263PluginCapability(encoderCodec, decoderCodec);
}

#endif // H323_VIDEO

/////////////////////////////////////////////////////////////////////////////

H323Codec * H323PluginCapabilityInfo::CreateCodec(const OpalMediaFormat & mediaFormat, H323Codec::Direction direction, const H323Capability * cap) const
{
  // allow use of this class for external codec capabilities
  if (encoderCodec == NULL || decoderCodec == NULL)
    return NULL;

  PluginCodec_Definition * codec = (direction == H323Codec::Encoder) ? encoderCodec : decoderCodec;

  switch (codec->flags & PluginCodec_MediaTypeMask) {

    case PluginCodec_MediaTypeAudio:
#ifdef H323_AUDIO_CODECS
      PTRACE(3, "H323PLUGIN\tCreating framed audio codec " << mediaFormatName << " from plugin");
      return new H323PluginFramedAudioCodec(mediaFormat, direction, codec);
#endif  // NO_H323_AUDIO_CODECS

    case PluginCodec_MediaTypeAudioStreamed:
#ifndef H323_AUDIO_CODECS
      PTRACE(3, "H323PLUGIN\tAudio plugins disabled");
      return NULL;
#else
      {
        PTRACE(3, "H323PLUGIN\tCreating audio codec " << mediaFormatName << " from plugin");
        int bitsPerSample = (codec->flags & PluginCodec_BitsPerSampleMask) >> PluginCodec_BitsPerSamplePos;
        if (bitsPerSample == 0)
          bitsPerSample = 16;
        return new H323StreamedPluginAudioCodec(
                                mediaFormat,
                                direction,
                                codec->parm.audio.samplesPerFrame,
                                bitsPerSample,
                                codec);
      }
#endif  // H323_AUDIO_CODECS

    case PluginCodec_MediaTypeVideo:
    case PluginCodec_MediaTypeExtended:
#ifndef H323_VIDEO
      PTRACE(3, "H323PLUGIN\tVideo plugins disabled");
      return NULL;
#else
      if ((((codec->flags & PluginCodec_MediaTypeMask) != PluginCodec_MediaTypeVideo) &&
          ((codec->flags & PluginCodec_MediaTypeMask) != PluginCodec_MediaTypeExtended)) ||
          (((codec->flags & PluginCodec_RTPTypeMask) != PluginCodec_RTPTypeExplicit) &&
           (codec->flags & PluginCodec_RTPTypeMask) != PluginCodec_RTPTypeDynamic)) {
             PTRACE(3, "H323PLUGIN\tVideo codec " << mediaFormatName << " has incorrect format types");
             return NULL;
      }

      PTRACE(3, "H323PLUGIN\tCreating video codec " << mediaFormatName << " from plugin");
      return new H323PluginVideoCodec(mediaFormat, direction, codec, cap);

#endif // H323_VIDEO
    default:
      break;
  }

  PTRACE(3, "H323PLUGIN\tCannot create codec for unknown plugin codec media format " << (int)(codec->flags & PluginCodec_MediaTypeMask));
  return NULL;
}

/////////////////////////////////////////////////////////////////////////////

H323PluginCapabilityInfo::H323PluginCapabilityInfo(PluginCodec_Definition * _encoderCodec,
                                                   PluginCodec_Definition * _decoderCodec)
 : encoderCodec(_encoderCodec),
   decoderCodec(_decoderCodec),
   capabilityFormatName(CreateCodecName(_encoderCodec, TRUE)),
   mediaFormatName(CreateCodecName(_encoderCodec, FALSE))
{
}

H323PluginCapabilityInfo::H323PluginCapabilityInfo(const PString & _mediaFormat, const PString & _baseName)
 : encoderCodec(NULL),
   decoderCodec(NULL),
   capabilityFormatName(CreateCodecName(_baseName, TRUE)),
   mediaFormatName(_mediaFormat)
{
}

#ifdef H323_AUDIO_CODECS

/////////////////////////////////////////////////////////////////////////////

H323CodecPluginNonStandardAudioCapability::H323CodecPluginNonStandardAudioCapability(
    PluginCodec_Definition * _encoderCodec,
    PluginCodec_Definition * _decoderCodec,
    H323NonStandardCapabilityInfo::CompareFuncType compareFunc,
    const unsigned char * data, unsigned dataLen)
 : H323NonStandardAudioCapability(_decoderCodec->parm.audio.maxFramesPerPacket,
                                  _encoderCodec->parm.audio.maxFramesPerPacket,
                                  compareFunc,
                                  data, dataLen),
   H323PluginCapabilityInfo(_encoderCodec, _decoderCodec)
{
  PluginCodec_H323NonStandardCodecData * nonStdData = (PluginCodec_H323NonStandardCodecData *)_encoderCodec->h323CapabilityData;
  if (nonStdData->objectId != NULL) {
    oid = PString(nonStdData->objectId);
  } else {
    t35CountryCode   = nonStdData->t35CountryCode;
    t35Extension     = nonStdData->t35Extension;
    manufacturerCode = nonStdData->manufacturerCode;
  }
}

H323CodecPluginNonStandardAudioCapability::H323CodecPluginNonStandardAudioCapability(
    PluginCodec_Definition * _encoderCodec,
    PluginCodec_Definition * _decoderCodec,
    const unsigned char * data, unsigned dataLen)
 : H323NonStandardAudioCapability(_decoderCodec->parm.audio.maxFramesPerPacket,
                                  _encoderCodec->parm.audio.maxFramesPerPacket,
                                  data, dataLen),
   H323PluginCapabilityInfo(_encoderCodec, _decoderCodec)
{
  PluginCodec_H323NonStandardCodecData * nonStdData = (PluginCodec_H323NonStandardCodecData *)_encoderCodec->h323CapabilityData;
  if (nonStdData->objectId != NULL) {
    oid = PString(nonStdData->objectId);
  } else {
    t35CountryCode   = nonStdData->t35CountryCode;
    t35Extension     = nonStdData->t35Extension;
    manufacturerCode = nonStdData->manufacturerCode;
  }
    rtpPayloadType = (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ?
                                                                              RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload);
}

/////////////////////////////////////////////////////////////////////////////

H323CodecPluginGenericAudioCapability::H323CodecPluginGenericAudioCapability(
    const PluginCodec_Definition * _encoderCodec,
    const PluginCodec_Definition * _decoderCodec,
    const PluginCodec_H323GenericCodecData *data )
    : H323GenericAudioCapability(_decoderCodec->parm.audio.maxFramesPerPacket,
                     _encoderCodec->parm.audio.maxFramesPerPacket,
                     data -> standardIdentifier, data -> maxBitRate),
      H323PluginCapabilityInfo((PluginCodec_Definition *)_encoderCodec,
                   (PluginCodec_Definition *) _decoderCodec)
{
  PopulateMediaFormatFromGenericData(GetWritableMediaFormat(), data);
  rtpPayloadType = (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ? RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload);
}

/////////////////////////////////////////////////////////////////////////////

PObject::Comparison H323GSMPluginCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323GSMPluginCapability))
    return LessThan;

  Comparison result = H323AudioCapability::Compare(obj);
  if (result != EqualTo)
    return result;

  const H323GSMPluginCapability& other = (const H323GSMPluginCapability&)obj;
  if (scrambled < other.scrambled)
    return LessThan;
  if (comfortNoise < other.comfortNoise)
    return LessThan;
  return EqualTo;
}


PBoolean H323GSMPluginCapability::OnSendingPDU(H245_AudioCapability & cap, unsigned packetSize) const
{
  cap.SetTag(pluginSubType);
  H245_GSMAudioCapability & gsm = cap;
  gsm.m_audioUnitSize = packetSize * encoderCodec->parm.audio.bytesPerFrame;
  gsm.m_comfortNoise  = comfortNoise;
  gsm.m_scrambled     = scrambled;

  return TRUE;
}


PBoolean H323GSMPluginCapability::OnReceivedPDU(const H245_AudioCapability & cap, unsigned & packetSize)
{
  const H245_GSMAudioCapability & gsm = cap;
  packetSize   = gsm.m_audioUnitSize / encoderCodec->parm.audio.bytesPerFrame;
  if (packetSize == 0)
    packetSize = 1;

  scrambled    = gsm.m_scrambled;
  comfortNoise = gsm.m_comfortNoise;

  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

#endif   // H323_AUDIO_CODECS

#ifdef H323_VIDEO

/////////////////////////////////////////////////////////////////////////////

H323H261PluginCapability::H323H261PluginCapability(PluginCodec_Definition * _encoderCodec,
                                                   PluginCodec_Definition * _decoderCodec)
  : H323VideoPluginCapability(_encoderCodec, _decoderCodec, H245_VideoCapability::e_h261VideoCapability),
  enc(_encoderCodec)
{
}

PObject::Comparison H323H261PluginCapability::Compare(const PObject & obj) const
{
  if (!PIsDescendant(&obj, H323H261PluginCapability))
    return LessThan;

  Comparison result = H323Capability::Compare(obj);
  if (result != EqualTo)
    return result;

  const H323H261PluginCapability & other = (const H323H261PluginCapability &)obj;

  const OpalMediaFormat & myFormat = GetMediaFormat();
  int qcifMPI  = myFormat.GetOptionInteger(qcifMPI_tag);
  int cifMPI   = myFormat.GetOptionInteger(cifMPI_tag);
  int cif4MPI  = myFormat.GetOptionInteger(cif4MPI_tag);
  int cif16MPI = myFormat.GetOptionInteger(cif16MPI_tag);

  const OpalMediaFormat & otherFormat = other.GetMediaFormat();
  int other_qcifMPI  = otherFormat.GetOptionInteger(qcifMPI_tag);
  int other_cifMPI   = otherFormat.GetOptionInteger(cifMPI_tag);
  int other_cif4MPI  = otherFormat.GetOptionInteger(cif4MPI_tag);
  int other_cif16MPI = otherFormat.GetOptionInteger(cif16MPI_tag);

  if ((IsValidMPI(qcifMPI) && IsValidMPI(other_qcifMPI)) ||
      (IsValidMPI(cifMPI) && IsValidMPI(other_cifMPI)) ||
      (IsValidMPI(cif4MPI) && IsValidMPI(other_cif4MPI)) ||
      (IsValidMPI(cif16MPI) && IsValidMPI(other_cif16MPI)))
           return EqualTo;

  if ((!IsValidMPI(cif16MPI) && IsValidMPI(other_cif16MPI)) ||
      (!IsValidMPI(cif4MPI) && IsValidMPI(other_cif4MPI)) ||
      (!IsValidMPI(cifMPI) && IsValidMPI(other_cifMPI)) ||
      (!IsValidMPI(qcifMPI) && IsValidMPI(other_qcifMPI)))
    return LessThan;

  return GreaterThan;
}


PBoolean H323H261PluginCapability::OnSendingPDU(H245_VideoCapability & cap) const
{
  cap.SetTag(H245_VideoCapability::e_h261VideoCapability);

  H245_H261VideoCapability & h261 = cap;

  const OpalMediaFormat & fmt = GetMediaFormat();

  int qcifMPI = fmt.GetOptionInteger(qcifMPI_tag, 0);

  if (qcifMPI > 0) {
    h261.IncludeOptionalField(H245_H261VideoCapability::e_qcifMPI);
    h261.m_qcifMPI = qcifMPI;
  }

  int cifMPI = fmt.GetOptionInteger(cifMPI_tag);
  if (cifMPI > 0 || qcifMPI == 0) {
    h261.IncludeOptionalField(H245_H261VideoCapability::e_cifMPI);
    h261.m_cifMPI = cifMPI;
  }

  h261.m_temporalSpatialTradeOffCapability = fmt.GetOptionBoolean(h323_temporalSpatialTradeOffCapability_tag, FALSE);
  h261.m_maxBitRate                        = (fmt.GetOptionInteger(OpalVideoFormat::MaxBitRateOption, 621700)+50)/100;
  h261.m_stillImageTransmission            = fmt.GetOptionBoolean(h323_stillImageTransmission_tag, FALSE);

  return TRUE;
}


PBoolean H323H261PluginCapability::OnSendingPDU(H245_VideoMode & pdu) const
{
  pdu.SetTag(H245_VideoMode::e_h261VideoMode);
  H245_H261VideoMode & mode = pdu;

  const OpalMediaFormat & fmt = GetMediaFormat();

  int qcifMPI = fmt.GetOptionInteger(qcifMPI_tag);

  mode.m_resolution.SetTag(qcifMPI > 0 ? H245_H261VideoMode_resolution::e_qcif
                                       : H245_H261VideoMode_resolution::e_cif);

  mode.m_bitRate                = (fmt.GetOptionInteger(OpalVideoFormat::MaxBitRateOption, 621700) + 50) / 1000;
  mode.m_stillImageTransmission = fmt.GetOptionBoolean(h323_stillImageTransmission_tag, FALSE);

  return TRUE;
}

PBoolean H323H261PluginCapability::OnReceivedPDU(const H245_VideoCapability & cap)
{
  if (cap.GetTag() != H245_VideoCapability::e_h261VideoCapability)
    return FALSE;

  OpalMediaFormat & fmt = GetWritableMediaFormat();

  const H245_H261VideoCapability & h261 = cap;

  if (h261.HasOptionalField(H245_H261VideoCapability::e_qcifMPI)) {
    if (!fmt.SetOptionInteger(qcifMPI_tag, h261.m_qcifMPI))
      return FALSE;

     if (!H323VideoPluginCapability::SetCommonOptions(fmt, QCIF_WIDTH, QCIF_HEIGHT, h261.m_qcifMPI))
         return FALSE;
  }

  if (h261.HasOptionalField(H245_H261VideoCapability::e_cifMPI)) {
    if (!fmt.SetOptionInteger(cifMPI_tag, h261.m_cifMPI))
      return FALSE;

    if (!H323VideoPluginCapability::SetCommonOptions(fmt, CIF_WIDTH, CIF_HEIGHT, h261.m_cifMPI))
      return FALSE;
  }

  fmt.SetOptionInteger(OpalVideoFormat::MaxBitRateOption,          h261.m_maxBitRate*100);
  fmt.SetOptionBoolean(h323_temporalSpatialTradeOffCapability_tag, h261.m_temporalSpatialTradeOffCapability);
  fmt.SetOptionBoolean(h323_stillImageTransmission_tag,            h261.m_stillImageTransmission);

  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

H323H263PluginCapability::H323H263PluginCapability(PluginCodec_Definition * _encoderCodec,
                                                   PluginCodec_Definition * _decoderCodec)
  : H323VideoPluginCapability(_encoderCodec, _decoderCodec, H245_VideoCapability::e_h263VideoCapability)
{
}

PObject::Comparison H323H263PluginCapability::Compare(const PObject & obj) const
{

  if (!PIsDescendant(&obj, H323H263PluginCapability))
    return LessThan;

  Comparison result = H323Capability::Compare(obj);
  if (result != EqualTo)
    return result;

  const H323H263PluginCapability & other = (const H323H263PluginCapability &)obj;

  const OpalMediaFormat & myFormat = GetMediaFormat();
  int sqcifMPI = myFormat.GetOptionInteger(sqcifMPI_tag);
  int qcifMPI  = myFormat.GetOptionInteger(qcifMPI_tag);
  int cifMPI   = myFormat.GetOptionInteger(cifMPI_tag);
  int cif4MPI  = myFormat.GetOptionInteger(cif4MPI_tag);
  int cif16MPI = myFormat.GetOptionInteger(cif16MPI_tag);

  const OpalMediaFormat & otherFormat = other.GetMediaFormat();
  int other_sqcifMPI = otherFormat.GetOptionInteger(sqcifMPI_tag);
  int other_qcifMPI  = otherFormat.GetOptionInteger(qcifMPI_tag);
  int other_cifMPI   = otherFormat.GetOptionInteger(cifMPI_tag);
  int other_cif4MPI  = otherFormat.GetOptionInteger(cif4MPI_tag);
  int other_cif16MPI = otherFormat.GetOptionInteger(cif16MPI_tag);

  if ((IsValidMPI(sqcifMPI) && IsValidMPI(other_sqcifMPI)) ||
      (IsValidMPI(qcifMPI) && IsValidMPI(other_qcifMPI)) ||
      (IsValidMPI(cifMPI) && IsValidMPI(other_cifMPI)) ||
      (IsValidMPI(cif4MPI) && IsValidMPI(other_cif4MPI)) ||
      (IsValidMPI(cif16MPI) && IsValidMPI(other_cif16MPI)))
           return EqualTo;

  if ((!IsValidMPI(cif16MPI) && IsValidMPI(other_cif16MPI)) ||
      (!IsValidMPI(cif4MPI) && IsValidMPI(other_cif4MPI)) ||
      (!IsValidMPI(cifMPI) && IsValidMPI(other_cifMPI)) ||
      (!IsValidMPI(qcifMPI) && IsValidMPI(other_qcifMPI)) ||
      (!IsValidMPI(sqcifMPI) && IsValidMPI(other_sqcifMPI)))
    return LessThan;

  return GreaterThan;
}

PBoolean H323H263PluginCapability::IsMatch(const PASN_Choice & subTypePDU) const
{
  if (!H323Capability::IsMatch(subTypePDU))
      return false;

  const H245_H263VideoCapability & cap = (const H245_H263VideoCapability &)subTypePDU.GetObject();
  const OpalMediaFormat & format = GetMediaFormat();
  PString packetization = format.GetOptionString(PLUGINCODEC_MEDIA_PACKETIZATION);
  PBoolean explicitMatch = format.GetOptionBoolean(H263_EXPLICIT_MATCH);

  // By the standard the method to distinguish H.263 and H.263+ is by the packetization element
  // however some endpoints do not include the packetization element so the common practise indicator
  // is the inclusion of the the h263Options field.
  if (packetization == "RFC2429" && cap.HasOptionalField(H245_H263VideoCapability::e_h263Options))
      return true;

  // H.263 Exact Match..
  if (packetization == "RFC2190" && !cap.HasOptionalField(H245_H263VideoCapability::e_h263Options))
      return true;

  return !explicitMatch;
}

static void SetTransmittedCap(const OpalMediaFormat & mediaFormat,
                              H245_H263VideoCapability & h263,
                              const char * mpiTag,
                              int mpiEnum,
                              PASN_Integer & mpi,
                              int slowMpiEnum,
                              PASN_Integer & slowMpi)
{
  int mpiVal = mediaFormat.GetOptionInteger(mpiTag);
  if (mpiVal > 0) {
    h263.IncludeOptionalField(mpiEnum);
    mpi = mpiVal;
  }
  else if (mpiVal < 0) {
    h263.IncludeOptionalField(slowMpiEnum);
    slowMpi = -mpiVal;
  }
}

PBoolean SetH263Options(const OpalMediaFormat & fmt, H245_H263Options & options)
{
    PString mediaPacketization = fmt.GetOptionString(PLUGINCODEC_MEDIA_PACKETIZATION);
    if (mediaPacketization.IsEmpty() || mediaPacketization != "RFC2429")
      return false;

    options.m_advancedIntraCodingMode = fmt.GetOptionBoolean(h323_advancedIntra_tag, FALSE);
    options.m_deblockingFilterMode = false;
    options.m_improvedPBFramesMode = false;
    options.m_unlimitedMotionVectors = false;
    options.m_fullPictureFreeze = false;
    options.m_partialPictureFreezeAndRelease = false;
    options.m_resizingPartPicFreezeAndRelease = false;
    options.m_fullPictureSnapshot = false;
    options.m_partialPictureSnapshot = false;
    options.m_videoSegmentTagging = false;
    options.m_progressiveRefinement = false;
    options.m_dynamicPictureResizingByFour = false;
    options.m_dynamicPictureResizingSixteenthPel = false;
    options.m_dynamicWarpingHalfPel = false;
    options.m_dynamicWarpingSixteenthPel = false;
    options.m_independentSegmentDecoding = false;
    options.m_slicesInOrder_NonRect = false;
    options.m_slicesInOrder_Rect = false;
    options.m_slicesNoOrder_NonRect = false;
    options.m_slicesNoOrder_Rect = false;
    options.m_alternateInterVLCMode = false;
    options.m_modifiedQuantizationMode = fmt.GetOptionBoolean(h323_modifiedQuantization_tag, FALSE);;
    options.m_reducedResolutionUpdate = false;
    options.m_separateVideoBackChannel = false;

    H245_ArrayOf_CustomPictureFormat & customFormats = options.m_customPictureFormat;
    customFormats.RemoveAll();

    for (PINDEX i = 0; i<fmt.GetOptionCount(); i++) {
        PString optionName = fmt.GetOption(i).GetName();
        if (optionName.NumCompare("CustomFmt") == PObject::EqualTo) {
            PStringList custom = fmt.GetOptionString(optionName).Tokenise(",");
            H245_CustomPictureFormat customFormat;
            customFormat.m_maxCustomPictureHeight = custom[0].AsInteger();
            customFormat.m_minCustomPictureHeight = custom[0].AsInteger();
            customFormat.m_maxCustomPictureWidth = custom[1].AsInteger();
            customFormat.m_minCustomPictureWidth = custom[1].AsInteger();
            H245_CustomPictureFormat_mPI & mpi = customFormat.m_mPI;
            mpi.IncludeOptionalField(H245_CustomPictureFormat_mPI::e_standardMPI);
            mpi.m_standardMPI = custom[2].AsInteger();
            int par = custom[3].AsInteger();
            if(par) {
                customFormat.m_pixelAspectInformation.SetTag(H245_CustomPictureFormat_pixelAspectInformation::e_pixelAspectCode);
                H245_CustomPictureFormat_pixelAspectInformation_pixelAspectCode & pixel =
                                                    customFormat.m_pixelAspectInformation;
                pixel.SetSize(1);
                pixel[0] = custom[3].AsInteger();
            }
            else {
                customFormat.m_pixelAspectInformation.SetTag(H245_CustomPictureFormat_pixelAspectInformation::e_anyPixelAspectRatio);
                PASN_Boolean& anyPixelAspectRatio =  (PASN_Boolean&)customFormat.m_pixelAspectInformation;
                anyPixelAspectRatio.SetValue(1);
            }

            int sz = customFormats.GetSize();
            customFormats.SetSize(sz+1);
            customFormats[sz] = customFormat;
        }
    }
    if (customFormats.GetSize() > 0)
        options.IncludeOptionalField(H245_H263Options::e_customPictureFormat);

    options.IncludeOptionalField(H245_H263Options::e_h263Version3Options);
    options.m_h263Version3Options.m_dataPartitionedSlices = false;
    options.m_h263Version3Options.m_fixedPointIDCT0 = false;
    options.m_h263Version3Options.m_interlacedFields = false;
    options.m_h263Version3Options.m_currentPictureHeaderRepetition = false;
    options.m_h263Version3Options.m_previousPictureHeaderRepetition = false;
    options.m_h263Version3Options.m_nextPictureHeaderRepetition = false;
    options.m_h263Version3Options.m_pictureNumber = false;
    options.m_h263Version3Options.m_spareReferencePictures = false;

    return true;
}

PBoolean H323H263PluginCapability::OnSendingPDU(H245_VideoCapability & cap) const
{
  cap.SetTag(H245_VideoCapability::e_h263VideoCapability);
  H245_H263VideoCapability & h263 = cap;

  const OpalMediaFormat & fmt = GetMediaFormat();

  SetTransmittedCap(fmt, cap, sqcifMPI_tag, H245_H263VideoCapability::e_sqcifMPI, h263.m_sqcifMPI, H245_H263VideoCapability::e_slowSqcifMPI, h263.m_slowSqcifMPI);
  SetTransmittedCap(fmt, cap, qcifMPI_tag,  H245_H263VideoCapability::e_qcifMPI,  h263.m_qcifMPI,  H245_H263VideoCapability::e_slowQcifMPI,  h263.m_slowQcifMPI);
  SetTransmittedCap(fmt, cap, cifMPI_tag,   H245_H263VideoCapability::e_cifMPI,   h263.m_cifMPI,   H245_H263VideoCapability::e_slowCifMPI,   h263.m_slowCifMPI);
  SetTransmittedCap(fmt, cap, cif4MPI_tag,  H245_H263VideoCapability::e_cif4MPI,  h263.m_cif4MPI,  H245_H263VideoCapability::e_slowCif4MPI,  h263.m_slowCif4MPI);
  SetTransmittedCap(fmt, cap, cif16MPI_tag, H245_H263VideoCapability::e_cif16MPI, h263.m_cif16MPI, H245_H263VideoCapability::e_slowCif16MPI, h263.m_slowCif16MPI);

  h263.m_maxBitRate                        = (fmt.GetOptionInteger(OpalVideoFormat::MaxBitRateOption, 327600) + 50) / 100;
  h263.m_temporalSpatialTradeOffCapability = fmt.GetOptionBoolean(h323_temporalSpatialTradeOffCapability_tag, FALSE);
  h263.m_unrestrictedVector                = fmt.GetOptionBoolean(h323_unrestrictedVector_tag, FALSE);
  h263.m_arithmeticCoding                  = fmt.GetOptionBoolean(h323_arithmeticCoding_tag, FALSE);
  h263.m_advancedPrediction                = fmt.GetOptionBoolean(h323_advancedPrediction_tag, FALSE);
  h263.m_pbFrames                          = fmt.GetOptionBoolean(h323_pbFrames_tag, FALSE);
  h263.m_errorCompensation                 = fmt.GetOptionBoolean(h323_errorCompensation_tag, FALSE);

  H245_H263Options & options = h263.m_h263Options;
  if(SetH263Options(fmt,options))
      h263.IncludeOptionalField(H245_H263VideoCapability::e_h263Options);

  {
    int hrdB = fmt.GetOptionInteger(h323_hrdB_tag, -1);
    if (hrdB >= 0) {
      h263.IncludeOptionalField(H245_H263VideoCapability::e_hrd_B);
        h263.m_hrd_B = hrdB;
    }
  }

  {
    int bppMaxKb = fmt.GetOptionInteger(h323_bppMaxKb_tag, -1);
    if (bppMaxKb >= 0) {
      h263.IncludeOptionalField(H245_H263VideoCapability::e_bppMaxKb);
        h263.m_bppMaxKb = bppMaxKb;
    }
  }

  return TRUE;
}

PBoolean H323H263PluginCapability::OnSendingPDU(H245_VideoMode & pdu) const
{
  pdu.SetTag(H245_VideoMode::e_h263VideoMode);
  H245_H263VideoMode & mode = pdu;

  const OpalMediaFormat & fmt = GetMediaFormat();

  int qcifMPI  = fmt.GetOptionInteger(qcifMPI_tag);
  int cifMPI   = fmt.GetOptionInteger(cifMPI_tag);
  int cif4MPI  = fmt.GetOptionInteger(cif4MPI_tag);
  int cif16MPI = fmt.GetOptionInteger(cif16MPI_tag);

  mode.m_resolution.SetTag(cif16MPI ? H245_H263VideoMode_resolution::e_cif16
              :(cif4MPI ? H245_H263VideoMode_resolution::e_cif4
               :(cifMPI ? H245_H263VideoMode_resolution::e_cif
                :(qcifMPI ? H245_H263VideoMode_resolution::e_qcif
            : H245_H263VideoMode_resolution::e_sqcif))));

  mode.m_bitRate              = (fmt.GetOptionInteger(OpalVideoFormat::MaxBitRateOption, 327600) + 50) / 100;
  mode.m_unrestrictedVector   = fmt.GetOptionBoolean(h323_unrestrictedVector_tag, FALSE);
  mode.m_arithmeticCoding     = fmt.GetOptionBoolean(h323_arithmeticCoding_tag, FALSE);
  mode.m_advancedPrediction   = fmt.GetOptionBoolean(h323_advancedPrediction_tag, FALSE);
  mode.m_pbFrames             = fmt.GetOptionBoolean(h323_pbFrames_tag, FALSE);
  mode.m_errorCompensation    = fmt.GetOptionBoolean(h323_errorCompensation_tag, FALSE);

  if (SetH263Options(fmt,mode.m_h263Options))
      mode.IncludeOptionalField(H245_H263VideoMode::e_h263Options);


  return TRUE;
}

static PBoolean SetReceivedH263Cap(OpalMediaFormat & mediaFormat,
                               const H245_H263VideoCapability & h263,
                               const char * mpiTag,
                               int mpiEnum,
                               const PASN_Integer & mpi,
                               int slowMpiEnum,
                               const PASN_Integer & slowMpi,
                               int frameWidth, int frameHeight,
                               PBoolean & formatDefined)
{
  if (h263.HasOptionalField(mpiEnum)) {
    if (!mediaFormat.SetOptionInteger(mpiTag, mpi))
      return FALSE;
    if (!H323VideoPluginCapability::SetCommonOptions(mediaFormat, frameWidth, frameHeight, mpi))
      return FALSE;
    formatDefined = TRUE;
  } else if (h263.HasOptionalField(slowMpiEnum)) {
    if (!mediaFormat.SetOptionInteger(mpiTag, -(signed)slowMpi))
      return FALSE;
    if (!H323VideoPluginCapability::SetCommonOptions(mediaFormat, frameWidth, frameHeight, -(signed)slowMpi))
      return FALSE;
    formatDefined = TRUE;
  }

  return TRUE;
}

PBoolean GetH263Options(OpalMediaFormat & fmt, const H245_H263Options & options)
{
    fmt.SetOptionBoolean(h323_advancedIntra_tag, options.m_advancedIntraCodingMode);
    fmt.SetOptionBoolean(h323_modifiedQuantization_tag, options.m_modifiedQuantizationMode);

    if (options.HasOptionalField(H245_H263Options::e_customPictureFormat)) {
        int opts[4];
        for (PINDEX j = 0; j < options.m_customPictureFormat.GetSize(); ++j) {
            opts[2] = 1;  opts[3] = 0;
            const H245_CustomPictureFormat & customFormat = options.m_customPictureFormat[j];
            opts[0] = customFormat.m_maxCustomPictureHeight;
            opts[1] = customFormat.m_maxCustomPictureWidth;

            const H245_CustomPictureFormat_mPI & mpi = customFormat.m_mPI;
            if (mpi.HasOptionalField(H245_CustomPictureFormat_mPI::e_standardMPI))
                   opts[2] = mpi.m_standardMPI;

            if (customFormat.m_pixelAspectInformation.GetTag() == H245_CustomPictureFormat_pixelAspectInformation::e_pixelAspectCode) {
               const H245_CustomPictureFormat_pixelAspectInformation_pixelAspectCode & pixel = customFormat.m_pixelAspectInformation;
                  if (pixel.GetSize() > 0) opts[3] = pixel[0];
            }

            PString val = PString(opts[0]) + ',' + PString(opts[1]) + ',' + PString(opts[2]) + ',' + PString(opts[3]);
            PString key = "CustomFmt"+ PString((j+1));
            if (fmt.HasOption(key))
              fmt.SetOptionString(key,val);
            else
              fmt.AddOption(new OpalMediaOptionString(key,false,val));
        }
    }

/*  Not Supported
    options.m_deblockingFilterMode;
    options.m_improvedPBFramesMode;
    options.m_unlimitedMotionVectors;
    options.m_fullPictureFreeze;
    options.m_partialPictureFreezeAndRelease;
    options.m_resizingPartPicFreezeAndRelease;
    options.m_fullPictureSnapshot;
    options.m_partialPictureSnapshot;
    options.m_videoSegmentTagging;
    options.m_progressiveRefinement;
    options.m_dynamicPictureResizingByFour;
    options.m_dynamicPictureResizingSixteenthPel;
    options.m_dynamicWarpingHalfPel;
    options.m_dynamicWarpingSixteenthPel;
    options.m_independentSegmentDecoding;
    options.m_slicesInOrder_NonRect;
    options.m_slicesInOrder_Rect;
    options.m_slicesNoOrder_NonRect;
    options.m_slicesNoOrder_Rect;
    options.m_alternateInterVLCMode;
    options.m_reducedResolutionUpdate;
    options.m_separateVideoBackChannel;

    if (options.HasOptionalField(H245_H263Options::e_h263Version3Options)) {
        options.m_h263Version3Options.m_dataPartitionedSlices;
        options.m_h263Version3Options.m_fixedPointIDCT0;
        options.m_h263Version3Options.m_interlacedFields;
        options.m_h263Version3Options.m_currentPictureHeaderRepetition;
        options.m_h263Version3Options.m_previousPictureHeaderRepetition;
        options.m_h263Version3Options.m_nextPictureHeaderRepetition;
        options.m_h263Version3Options.m_pictureNumber;
        options.m_h263Version3Options.m_spareReferencePictures;
    }
*/
    return true;
}



PBoolean H323H263PluginCapability::OnReceivedPDU(const H245_VideoCapability & cap)
{
  if (cap.GetTag() != H245_VideoCapability::e_h263VideoCapability)
    return FALSE;

  OpalMediaFormat & fmt = GetWritableMediaFormat();

  PBoolean formatDefined = FALSE;

  const H245_H263VideoCapability & h263 = cap;

  if (!SetReceivedH263Cap(fmt, cap, sqcifMPI_tag, H245_H263VideoCapability::e_sqcifMPI, h263.m_sqcifMPI, H245_H263VideoCapability::e_slowSqcifMPI, h263.m_slowSqcifMPI, SQCIF_WIDTH, SQCIF_HEIGHT, formatDefined))
    return FALSE;

  if (!SetReceivedH263Cap(fmt, cap, qcifMPI_tag,  H245_H263VideoCapability::e_qcifMPI,  h263.m_qcifMPI,  H245_H263VideoCapability::e_slowQcifMPI,  h263.m_slowQcifMPI,  QCIF_WIDTH,  QCIF_HEIGHT,  formatDefined))
    return FALSE;

  if (!SetReceivedH263Cap(fmt, cap, cifMPI_tag,   H245_H263VideoCapability::e_cifMPI,   h263.m_cifMPI,   H245_H263VideoCapability::e_slowCifMPI,   h263.m_slowCifMPI,   CIF_WIDTH,   CIF_HEIGHT,   formatDefined))
    return FALSE;

  if (!SetReceivedH263Cap(fmt, cap, cif4MPI_tag,  H245_H263VideoCapability::e_cif4MPI,  h263.m_cif4MPI,  H245_H263VideoCapability::e_slowCif4MPI,  h263.m_slowCif4MPI,  CIF4_WIDTH,  CIF4_HEIGHT,  formatDefined))
    return FALSE;

  if (!SetReceivedH263Cap(fmt, cap, cif16MPI_tag, H245_H263VideoCapability::e_cif16MPI, h263.m_cif16MPI, H245_H263VideoCapability::e_slowCif16MPI, h263.m_slowCif16MPI, CIF16_WIDTH, CIF16_HEIGHT, formatDefined))
    return FALSE;

  if (!fmt.SetOptionInteger(OpalVideoFormat::MaxBitRateOption, h263.m_maxBitRate*100))
    return FALSE;

  fmt.SetOptionBoolean(h323_unrestrictedVector_tag,      h263.m_unrestrictedVector);
  fmt.SetOptionBoolean(h323_arithmeticCoding_tag,        h263.m_arithmeticCoding);
  fmt.SetOptionBoolean(h323_advancedPrediction_tag,      h263.m_advancedPrediction);
  fmt.SetOptionBoolean(h323_pbFrames_tag,                h263.m_pbFrames);
  fmt.SetOptionBoolean(h323_errorCompensation_tag,       h263.m_errorCompensation);

  if (h263.HasOptionalField(H245_H263VideoCapability::e_hrd_B))
    fmt.SetOptionInteger(h323_hrdB_tag, h263.m_hrd_B);

  if (h263.HasOptionalField(H245_H263VideoCapability::e_bppMaxKb))
    fmt.SetOptionInteger(h323_bppMaxKb_tag, h263.m_bppMaxKb);

  // zero out all (if any) custom formats
  for (PINDEX i = 0; i<fmt.GetOptionCount(); i++) {
        PString optionName = fmt.GetOption(i).GetName();
        if (optionName.NumCompare("CustomFmt") == PObject::EqualTo)
            fmt.SetOptionString(optionName,"0,0,1,0");
  }

  PString packetization = fmt.GetOptionString(PLUGINCODEC_MEDIA_PACKETIZATION);
  if (packetization == "RFC2429" && h263.HasOptionalField(H245_H263VideoCapability::e_h263Options)) {
     GetH263Options(fmt, h263.m_h263Options);                           // H.263+
  } else {
     fmt.SetOptionString(PLUGINCODEC_MEDIA_PACKETIZATION, "RFC2190");   // force to H.263
  }

  return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

H323CodecPluginNonStandardVideoCapability::H323CodecPluginNonStandardVideoCapability(
    PluginCodec_Definition * _encoderCodec,
    PluginCodec_Definition * _decoderCodec,
    H323NonStandardCapabilityInfo::CompareFuncType /*compareFunc*/,
    const unsigned char * data, unsigned dataLen)
 : H323NonStandardVideoCapability(data, dataLen),
   H323PluginCapabilityInfo(_encoderCodec, _decoderCodec)
{
  PluginCodec_H323NonStandardCodecData * nonStdData = (PluginCodec_H323NonStandardCodecData *)_encoderCodec->h323CapabilityData;
  if (nonStdData->objectId != NULL) {
    oid = PString(nonStdData->objectId);
  } else {
    t35CountryCode   = nonStdData->t35CountryCode;
    t35Extension     = nonStdData->t35Extension;
    manufacturerCode = nonStdData->manufacturerCode;
  }

  PopulateMediaFormatOptions(encoderCodec,GetWritableMediaFormat());
  rtpPayloadType = (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ? RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload);
}

H323CodecPluginNonStandardVideoCapability::H323CodecPluginNonStandardVideoCapability(
    PluginCodec_Definition * _encoderCodec,
    PluginCodec_Definition * _decoderCodec,
    const unsigned char * data, unsigned dataLen)
 : H323NonStandardVideoCapability(data, dataLen),
   H323PluginCapabilityInfo(_encoderCodec, _decoderCodec)
{
  PluginCodec_H323NonStandardCodecData * nonStdData = (PluginCodec_H323NonStandardCodecData *)_encoderCodec->h323CapabilityData;
  if (nonStdData->objectId != NULL) {
    oid = PString(nonStdData->objectId);
  } else {
    t35CountryCode   = nonStdData->t35CountryCode;
    t35Extension     = nonStdData->t35Extension;
    manufacturerCode = nonStdData->manufacturerCode;
  }

  rtpPayloadType = (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ? RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload);
}

/////////////////////////////////////////////////////////////////////////////

H323CodecPluginGenericVideoCapability::H323CodecPluginGenericVideoCapability(
    const PluginCodec_Definition * _encoderCodec,
    const PluginCodec_Definition * _decoderCodec,
    const PluginCodec_H323GenericCodecData *data )
    : H323GenericVideoCapability(data -> standardIdentifier, data -> maxBitRate),
      H323PluginCapabilityInfo((PluginCodec_Definition *)_encoderCodec, (PluginCodec_Definition *) _decoderCodec)
{
  H323VideoPluginCapability::SetCommonOptions(GetWritableMediaFormat(),encoderCodec->parm.video.maxFrameWidth,
                                    encoderCodec->parm.video.maxFrameHeight, encoderCodec->parm.video.recommendedFrameRate);
  LoadGenericData(data);
  rtpPayloadType = (RTP_DataFrame::PayloadTypes)(((_encoderCodec->flags & PluginCodec_RTPTypeMask) == PluginCodec_RTPTypeDynamic) ? RTP_DataFrame::DynamicBase : _encoderCodec->rtpPayload);
}

void H323CodecPluginGenericVideoCapability::LoadGenericData(const PluginCodec_H323GenericCodecData *data)
{
  PopulateMediaFormatOptions(encoderCodec,GetWritableMediaFormat());

  PopulateMediaFormatFromGenericData(GetWritableMediaFormat(), data);
}

PBoolean H323CodecPluginGenericVideoCapability::SetMaxFrameSize(CapabilityFrameSize framesize, int frameunits)
{
/*
    PString param;
    switch (framesize) {
        case sqcifMPI  : param = sqcifMPI_tag; break;
        case  qcifMPI  : param =  qcifMPI_tag; break;
        case   cifMPI  : param =   cifMPI_tag; break;
        case   cif4MPI : param =   cif4MPI_tag; break;
        case   cif16MPI: param =   cif16MPI_tag; break;
        default: return FALSE;
    }

    SetCodecControl(encoderCodec, NULL, SET_CODEC_OPTIONS_CONTROL, param,frameunits);
    SetCodecControl(decoderCodec, NULL, SET_CODEC_OPTIONS_CONTROL, param, frameunits);
    LoadGenericData((PluginCodec_H323GenericCodecData *)encoderCodec->h323CapabilityData); */
    return TRUE;
}

PBoolean H323CodecPluginGenericVideoCapability::SetCustomEncode(unsigned width, unsigned height, unsigned rate)
{
   return SetCustomLevel(encoderCodec, GetWritableMediaFormat(), width, height, rate);
}

/////////////////////////////////////////////////////////////////////////////

#endif  // H323_VIDEO

/////////////////////////////////////////////////////////////////////////////

static PAtomicInteger bootStrapCount;

void H323PluginCodecManager::Bootstrap()
{
  if (++bootStrapCount != 1)
    return;

#if defined(H323_AUDIO_CODECS)
  OpalMediaFormat::List & mediaFormatList = H323PluginCodecManager::GetMediaFormatList();

  mediaFormatList.Append(new OpalMediaFormat(OpalG711uLaw));
  mediaFormatList.Append(new OpalMediaFormat(OpalG711ALaw));
/*
  OpalPluginCodecFactory::Register("L16|OpalG711ALaw64k", new OpalG711ALaw64k_Encoder());
  OpalPluginCodecFactory::Register("OpalG711ALaw64k|L16", new OpalG711ALaw64k_Decoder());
  OpalPluginCodecFactory::Register("L16|G.711-uLaw-64k", new OpalG711uLaw64k_Encoder());
  OpalPluginCodecFactory::Register("G.711-uLaw-64k|L16", new OpalG711uLaw64k_Decoder());
*/
  OpalPluginCodecFactory::Register("L16|OpalG711ALaw64k20", new OpalG711ALaw64k20_Encoder());
  OpalPluginCodecFactory::Register("OpalG711ALaw64k20|L16", new OpalG711ALaw64k20_Decoder());
  OpalPluginCodecFactory::Register("L16|G.711-uLaw-64k-20", new OpalG711uLaw64k20_Encoder());
  OpalPluginCodecFactory::Register("G.711-uLaw-64k-20|L16", new OpalG711uLaw64k20_Decoder());

#endif

}

void H323PluginCodecManager::Reboot()
{
      // unregister the plugin media formats
      OpalMediaFormatFactory::UnregisterAll();

      // Unregister the codec factory
      OpalPluginCodecFactory::UnregisterAll();

#ifdef H323_VIDEO
#ifdef H323_H239
      H323ExtendedVideoFactory::UnregisterAll();
#endif
#endif
      // unregister the plugin capabilities
      H323CapabilityFactory::UnregisterAll();

    bootStrapCount--;
    Bootstrap();
}

OpalFactoryCodec * H323PluginCodecManager::CreateCodec(const PString & name)
{
#ifdef H323_AUDIO_CODECS
  // OpalPluginCodecFactory is not being loaded from Bootstrap
  // This needs to be fixed - SH
  if (name =="L16|OpalG711ALaw64k") return new OpalG711ALaw64k_Encoder();
  if (name =="OpalG711ALaw64k|L16") return new OpalG711ALaw64k_Decoder();
  if (name =="L16|G.711-uLaw-64k") return new OpalG711uLaw64k_Encoder();
  if (name =="G.711-uLaw-64k|L16") return new OpalG711uLaw64k_Decoder();

  if (name =="L16|OpalG711ALaw64k20") return new OpalG711ALaw64k20_Encoder();
  if (name =="OpalG711ALaw64k20|L16") return new OpalG711ALaw64k20_Decoder();
  if (name =="L16|G.711-uLaw-64k-20") return new OpalG711uLaw64k20_Encoder();
  if (name =="G.711-uLaw-64k-20|L16") return new OpalG711uLaw64k20_Decoder();

    OpalPluginCodecFactory::KeyList_T keyList = OpalPluginCodecFactory::GetKeyList();
    OpalPluginCodecFactory::KeyList_T::const_iterator r;
    for (r = keyList.begin(); r != keyList.end(); ++r) {
        if (*r == name)
            return OpalPluginCodecFactory::CreateInstance(*r);
    }
#endif
    return NULL;
}

void H323PluginCodecManager::CodecListing(const PString & matchStr, PStringList & listing)
{
    OpalPluginCodecFactory::KeyList_T keyList = OpalPluginCodecFactory::GetKeyList();
    OpalPluginCodecFactory::KeyList_T::const_iterator r;
    for (r = keyList.begin(); r != keyList.end(); ++r) {
        int i = r->Find(matchStr);
        if (i != P_MAX_INDEX) {
            if (i == 0)
                listing.AppendString((*r).Mid(matchStr.GetLength()));
            else {
                listing.AppendString((*r).Left((*r).GetLength() -
                                                 matchStr.GetLength()));
            }
        }
    }

    PFactory<H323StaticPluginCodec>::KeyList_T staticList = PFactory<H323StaticPluginCodec>::GetKeyList();
    PFactory<H323StaticPluginCodec>::KeyList_T::const_iterator s;
    for (s = staticList.begin(); s != staticList.end(); ++s) {
        int i = PString(*s).Find(matchStr);
        if (i != P_MAX_INDEX) {
            if (i == 0)
                listing.AppendString(PString(*s).Mid(matchStr.GetLength()));
            else {
                listing.AppendString(PString(*s).Left(PString(*s).GetLength() -
                                                 matchStr.GetLength()));
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// Embedding codecs

#define INCLUDE_STATIC_CODEC(name) \
extern "C" { \
extern unsigned int Opal_StaticCodec_##name##_GetAPIVersion(); \
extern struct PluginCodec_Definition * Opal_StaticCodec_##name##_GetCodecs(unsigned *,unsigned); \
}; \
class H323StaticPluginCodec_##name : public H323StaticPluginCodec \
{ \
  public: \
    PluginCodec_GetAPIVersionFunction Get_GetAPIFn() \
    { return &Opal_StaticCodec_##name##_GetAPIVersion; } \
    PluginCodec_GetCodecFunction Get_GetCodecFn() \
    { return &Opal_StaticCodec_##name##_GetCodecs; } \
}; \
static PFactory<H323StaticPluginCodec>::Worker<H323StaticPluginCodec_##name > static##name##CodecFactory( #name ); \

/////////////////////////////////////////////////////////////////////////////////////
// Static codec definitions
// Implementor may add more...

#ifdef H323_STATIC_GSM
    #if _WIN32
        #pragma comment(lib,"GSM_0610_codec.lib")
    #endif
    INCLUDE_STATIC_CODEC(GSM_0610)
#endif

#ifdef H323_STATIC_G7221
    #if _WIN32
        #pragma comment(lib,"G7221_codec.lib")
    #endif
    INCLUDE_STATIC_CODEC(G7221)
#endif

#ifdef H323_STATIC_H261
    #if _WIN32
       #pragma comment(lib,"VIC_H261_codec.lib")
    #endif
    INCLUDE_STATIC_CODEC(VIC_H261)
#endif

#ifdef H323_STATIC_H263
    #if _WIN32
       #pragma comment(lib,"FFMPEG_H263P_codec.lib")
    #endif
    INCLUDE_STATIC_CODEC(FFMPEG_H263P)
#endif

#ifdef H323_STATIC_H264
    #if _WIN32
       #pragma comment(lib,"H264_codec.lib")
    #endif
    INCLUDE_STATIC_CODEC(H264)
#endif

