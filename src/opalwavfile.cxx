/*
 * OpalWavFile.cxx
 *
 * WAV file class with auto-PCM conversion
 *
 * OpenH323 Library
 *
 * Copyright (c) 2002 Equivalence Pty. Ltd.
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
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "opalwavfile.h"
#endif

#include "openh323buildopts.h"
#include "opalwavfile.h"

#ifdef _MSC_VER
#include "../include/codecs.h"
#else
#include "codecs.h"
#endif



#define new PNEW


OpalWAVFile::OpalWAVFile(unsigned fmt)
  : PWAVFile(fmt)
{
  SetAutoconvert();
}


OpalWAVFile::OpalWAVFile(OpenMode mode,
                     #if PTLIB_VER >= 2112
                         OpenOptions opts,
                     #else
                         int opts, 
                     #endif
                         unsigned fmt)
: PWAVFile(mode, opts, fmt)
{
  SetAutoconvert();
}

OpalWAVFile::OpalWAVFile(const PFilePath & name, 
                         OpenMode mode,  
                     #if PTLIB_VER >= 2112
                         OpenOptions opts,
                     #else
                         int opts, 
                     #endif
                         unsigned fmt)  
  : PWAVFile(name, mode, opts, fmt)
{
  SetAutoconvert();
}

/////////////////////////////////////////////////////////////////////////////////

class PWAVFileConverterXLaw : public PWAVFileConverter
{
  public:
    off_t GetPosition     (const PWAVFile & file) const;
    PBoolean SetPosition      (PWAVFile & file, off_t pos, PFile::FilePositionOrigin origin);
    unsigned GetSampleSize(const PWAVFile & file) const;
    off_t GetDataLength   (PWAVFile & file);
    PBoolean Read             (PWAVFile & file, void * buf, PINDEX len);
    PBoolean Write            (PWAVFile & file, const void * buf, PINDEX len);

    virtual short DecodeSample(int sample) = 0;
};

off_t PWAVFileConverterXLaw::GetPosition(const PWAVFile & file) const
{
  off_t pos = file.RawGetPosition();
  return pos * 2;
}

PBoolean PWAVFileConverterXLaw::SetPosition(PWAVFile & file, off_t pos, PFile::FilePositionOrigin origin)
{
  pos /= 2;
  return file.SetPosition(pos, origin);
}

unsigned PWAVFileConverterXLaw::GetSampleSize(const PWAVFile &) const
{
  return 16;
}

off_t PWAVFileConverterXLaw::GetDataLength(PWAVFile & file)
{
  return file.RawGetDataLength() * 2;
}

PBoolean PWAVFileConverterXLaw::Read(PWAVFile & file, void * buf, PINDEX len)
{
  // read the xLaw data
  PINDEX samples = (len / 2);
  PBYTEArray xlaw;
  if (!file.PFile::Read(xlaw.GetPointer(samples), samples))
    return FALSE;

  // convert to PCM
  PINDEX i;
  short * pcmPtr = (short *)buf;
  for (i = 0; i < samples; i++)
    *pcmPtr++ = DecodeSample(xlaw[i]);

  // fake the lastReadCount
  file.SetLastReadCount(len);

  return TRUE;
}


PBoolean PWAVFileConverterXLaw::Write(PWAVFile & /*file*/, const void * /*buf*/, PINDEX /*len*/)
{
  return FALSE;
}

//////////////////////////////////////////////////////////////////////

#ifdef H323_AUDIO_CODECS

class PWAVFileConverterULaw : public PWAVFileConverterXLaw
{
  public:
    unsigned GetFormat(const PWAVFile & /*file*/) const
    { return PWAVFile::fmt_uLaw; }

    short DecodeSample(int sample)
    { return H323_muLawCodec::DecodeSample(sample);}
};

class PWAVFileConverterALaw : public PWAVFileConverterXLaw
{
  public:
    unsigned GetFormat(const PWAVFile & /*file*/) const
    { return PWAVFile::fmt_ALaw; }

    short DecodeSample(int sample)
    { return H323_ALawCodec::DecodeSample(sample);}
};

PWAVFileConverterFactory::Worker<PWAVFileConverterULaw> uLawConverter(PWAVFile::fmt_uLaw, true);
PWAVFileConverterFactory::Worker<PWAVFileConverterALaw> ALawConverter(PWAVFile::fmt_ALaw, true);

#endif

///////////////////////////////////////////////////////////////////////

