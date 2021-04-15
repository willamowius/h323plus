/*
 * vxml.cxx
 *
 * VXML control for for Opal
 *
 * A H.323 IVR application.
 *
 * Copyright (C) 2002 Equivalence Pty. Ltd.
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
 * The Original Code is Portable Windows Library.
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
#pragma implementation "opalvxml.h"
#endif

#include "opalvxml.h"

#ifdef P_VXML

#include <ptclib/delaychan.h>
#include <ptclib/pwavfile.h>
#include <ptclib/memfile.h>

#endif

#ifdef _MSC_VER
#include "../include/codecs.h"
#else
#include "codecs.h"
#endif

#define	G7231_SAMPLES_PER_BLOCK	240
#define	G7231_BANDWIDTH		      (6300/100)

///////////////////////////////////////////////////////////////

#ifdef H323_AUDIO_CODECS

G7231_File_Capability::G7231_File_Capability()
  : H323AudioCapability(8, 4)
{
}

unsigned G7231_File_Capability::GetSubType() const
{
  return H245_AudioCapability::e_g7231;
}

PString G7231_File_Capability::GetFormatName() const
{
  return "G.723.1{file}";
}

PBoolean G7231_File_Capability::OnSendingPDU(H245_AudioCapability & cap, unsigned packetSize) const
{
  // set the choice to the correct type
  cap.SetTag(GetSubType());

  // get choice data
  H245_AudioCapability_g7231 & g7231 = cap;

  // max number of audio frames per PDU we want to send
  g7231.m_maxAl_sduAudioFrames = packetSize;

  // enable silence suppression
  g7231.m_silenceSuppression = TRUE;

  return TRUE;
}

PBoolean G7231_File_Capability::OnReceivedPDU(const H245_AudioCapability & cap, unsigned & packetSize)
{
  const H245_AudioCapability_g7231 & g7231 = cap;
  packetSize = g7231.m_maxAl_sduAudioFrames;
  return TRUE;
}

PObject * G7231_File_Capability::Clone() const
{
  return new G7231_File_Capability(*this);
}

H323Codec * G7231_File_Capability::CreateCodec(H323Codec::Direction direction) const
{
  return new G7231_File_Codec(direction);
}

///////////////////////////////////////////////////////////////

G7231_File_Codec::G7231_File_Codec(Direction dir)
  : H323AudioCodec(OPAL_G7231_6k3, dir)
{
  lastFrameLen = 4;
}


int G7231_File_Codec::GetFrameLen(int val)
{
  static const int frameLen[] = { 24, 20, 4, 1 };
  return frameLen[val & 3];
}

PBoolean G7231_File_Codec::Read(BYTE * buffer, unsigned & length, RTP_DataFrame &)
{
  if (rawDataChannel == NULL)
    return FALSE;

  if (!rawDataChannel->Read(buffer, 24)) {
    PTRACE(1, "G7231WAV\tRead failed");
    return FALSE;
  }

  lastFrameLen = length = G7231_File_Codec::GetFrameLen(buffer[0]);

  return TRUE;
}


PBoolean G7231_File_Codec::Write(const BYTE * buffer,
                             unsigned length,
                             const RTP_DataFrame & /* rtp */,
                             unsigned & writtenLength)
{
  if (rawDataChannel == NULL)
    return TRUE;

  static const BYTE silence[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  0, 0, 0, 0};

  // If the length is zero, output silence to the file..
  if (length == 0) {
    PTRACE(6,"G7231WAV\tZero length frame");
    writtenLength = 0;
    return rawDataChannel->Write(silence, 24);
  }

  int writeLen;
  switch (buffer[0]&3) {
    case 0:
      writeLen = 24;
      break;
    case 1:
      writeLen = 20;
      break;
    case 2:
      // Windows Media Player cannot play 4 byte SID (silence) frames.
      // So write out a 24 byte frame of silence instead.
      PTRACE(5, "G7231WAV\tReplacing SID with 24 byte frame");
      writtenLength = 4;
      return rawDataChannel->Write(silence, 24);
    default :
      writeLen = 1;
      break;
  }

  PTRACE(6, "G7231WAV\tFrame length = " <<writeLen);

  writtenLength = writeLen;

  return rawDataChannel->Write(buffer, writeLen);
}


unsigned G7231_File_Codec::GetBandwidth() const
{
  return G7231_BANDWIDTH;
}


PBoolean G7231_File_Codec::IsRawDataChannelNative() const
{
  return TRUE;
}

unsigned G7231_File_Codec::GetAverageSignalLevel()
{
  if (lastFrameLen == 4)
    return 0;
  else
    return UINT_MAX;
}

#endif // H323_AUDIO_CODECS

///////////////////////////////////////////////////////////////

#ifdef P_VXML

OpalVXMLSession::OpalVXMLSession(H323Connection * _conn, PTextToSpeech * tts, PBoolean autoDelete)
  : PVXMLSession(tts, autoDelete), conn(_conn)
{
}


PBoolean OpalVXMLSession::Close()
{
  PBoolean ok = PVXMLSession::Close();
  conn->ClearCall();
  return ok;
}




#endif


// End of File /////////////////////////////////////////////////////////////
