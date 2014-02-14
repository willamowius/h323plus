/*
 * opalvxml.h
 *
 * Header file for IVR code
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

#ifndef _OpenIVR_OPALVXML_H
#define _OpenIVR_OPALVXML_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <ptclib/vxml.h>
#include <opalwavfile.h>
#include <ptclib/delaychan.h>
#include <h323caps.h>
#include <h245.h>
#include <h323con.h>

//////////////////////////////////////////////////////////////////

class G7231_File_Codec : public H323AudioCodec
{
  PCLASSINFO(G7231_File_Codec, H323AudioCodec);

  public:
    G7231_File_Codec(Direction dir);

    unsigned GetBandwidth() const;
    static int GetFrameLen(int val);
      
    PBoolean Read(BYTE * buffer, unsigned & length, RTP_DataFrame &);
    PBoolean Write(const BYTE * buffer, unsigned length, const RTP_DataFrame & rtp, unsigned & frames);

    PBoolean IsRawDataChannelNative() const;

    unsigned GetAverageSignalLevel();

  protected:
    int lastFrameLen;
};  


class G7231_File_Capability : public H323AudioCapability
{
  PCLASSINFO(G7231_File_Capability, H323AudioCapability)

  public:
    G7231_File_Capability();

    unsigned GetSubType() const;
    PString GetFormatName() const;

    H323Codec * CreateCodec(H323Codec::Direction direction) const;

    PBoolean OnSendingPDU(H245_AudioCapability & cap, unsigned packetSize) const;
    PBoolean OnReceivedPDU(const H245_AudioCapability & pdu, unsigned & packetSize);
    PObject * Clone() const;
};


//////////////////////////////////////////////////////////////////


#ifdef P_VXML

class PTextToSpeech;

class OpalVXMLSession : public PVXMLSession 
{
  PCLASSINFO(OpalVXMLSession, PVXMLSession);
  public:
    OpalVXMLSession(H323Connection * _conn, PTextToSpeech * tts = NULL, PBoolean autoDelete = FALSE);
    PBoolean Close();

  protected:
    H323Connection * conn;
};

#endif

#endif

