/*
 * rtp.cxx
 *
 * RTP protocol handler
 *
 * H323plus Library
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

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "rtp.h"
#endif

#if defined(_WIN32) && (_MSC_VER > 1300)
  #pragma warning(disable:4244) // warning about possible loss of data
#endif

#include "openh323buildopts.h"

#include "rtp.h"
#include "h323con.h"

#ifdef H323_AUDIO_CODECS
#include "jitter.h"
#endif

#include <ptclib/random.h>

#ifdef P_STUN
#include <ptclib/pnat.h>
#endif

#if defined(H323_RTP_AGGREGATE) || defined(H323_SIGNAL_AGGREGATE)
#include <ptclib/sockagg.h>
#endif

#define new PNEW


const unsigned SecondsFrom1900to1970 = (70*365+17)*24*60*60U;

#define UDP_BUFFER_SIZE 32768

#define MIN_HEADER_SIZE 12


/////////////////////////////////////////////////////////////////////////////

RTP_DataFrame::RTP_DataFrame(PINDEX sz, PBoolean dynamicAllocation)
  : PBYTEArray(MIN_HEADER_SIZE+sz)
{
  payloadSize = sz;
  allocatedDynamically = dynamicAllocation;
  theArray[0] = '\x80';
}


void RTP_DataFrame::SetExtension(PBoolean ext)
{
  if (ext)
    theArray[0] |= 0x10;
  else
    theArray[0] &= 0xef;
}

void RTP_DataFrame::SetPadding(PBoolean padding)
{
  if (padding)
    theArray[0] |= 0x20;
  else
    theArray[0] &= 0xdf;
}


void RTP_DataFrame::SetMarker(PBoolean m)
{
  if (m)
    theArray[1] |= 0x80;
  else
    theArray[1] &= 0x7f;
}

void RTP_DataFrame::SetPayloadType(PayloadTypes t)
{
  PAssert(t <= 0x7f, PInvalidParameter);

  theArray[1] &= 0x80;
  theArray[1] |= t;
}

BYTE * RTP_DataFrame::GetSequenceNumberPtr() const
{
    return (BYTE *)&theArray[2];
}


DWORD RTP_DataFrame::GetContribSource(PINDEX idx) const
{
  PAssert(idx < GetContribSrcCount(), PInvalidParameter);
  return ((PUInt32b *)&theArray[MIN_HEADER_SIZE])[idx];
}


void RTP_DataFrame::SetContribSource(PINDEX idx, DWORD src)
{
  PAssert(idx <= 15, PInvalidParameter);

  if (idx >= GetContribSrcCount()) {
    BYTE * oldPayload = GetPayloadPtr();
    theArray[0] &= 0xf0;
    theArray[0] |= idx+1;
    SetSize(GetHeaderSize()+payloadSize);
    memmove(GetPayloadPtr(), oldPayload, payloadSize);
  }

  ((PUInt32b *)&theArray[MIN_HEADER_SIZE])[idx] = src;
}


PINDEX RTP_DataFrame::GetHeaderSize() const
{
  PINDEX sz = MIN_HEADER_SIZE + 4*GetContribSrcCount();

  if (GetExtension())
    sz += 4 + GetExtensionSize();

  return sz;
}


int RTP_DataFrame::GetExtensionType() const
{
  if (GetExtension())
    return *(PUInt16b *)&theArray[MIN_HEADER_SIZE + 4*GetContribSrcCount()];

  return -1;
}


void RTP_DataFrame::SetExtensionType(int type)
{
  if (type < 0)
    SetExtension(FALSE);
  else {
    if (!GetExtension())
      SetExtensionSize(0);
    *(PUInt16b *)&theArray[MIN_HEADER_SIZE + 4*GetContribSrcCount()] = (WORD)type;
  }
}


PINDEX RTP_DataFrame::GetExtensionSize() const
{
  if (GetExtension())
    return *(PUInt16b *)&theArray[MIN_HEADER_SIZE + 4*GetContribSrcCount() + 2];

  return 0;
}


PBoolean RTP_DataFrame::SetExtensionSize(PINDEX sz)
{
  if (!SetMinSize(MIN_HEADER_SIZE + 4*GetContribSrcCount() + 4+4*sz + payloadSize))
    return FALSE;

  SetExtension(TRUE);
  *(PUInt16b *)&theArray[MIN_HEADER_SIZE + 4*GetContribSrcCount() + 2] = (WORD)sz;
  return TRUE;
}


BYTE * RTP_DataFrame::GetExtensionPtr() const
{
  if (GetExtension())
    return (BYTE *)&theArray[MIN_HEADER_SIZE + 4*GetContribSrcCount() + 4];

  return NULL;
}


PBoolean RTP_DataFrame::SetPayloadSize(PINDEX sz)
{
  payloadSize = sz;
  return SetMinSize(GetHeaderSize()+payloadSize);
}


#if PTRACING
static const char * const PayloadTypesNames[RTP_DataFrame::LastKnownPayloadType] = {
  "PCMU",
  "FS1016",
  "G721",
  "GSM",
  "G7231",
  "DVI4_8k",
  "DVI4_16k",
  "LPC",
  "PCMA",
  "G722",
  "L16_Stereo",
  "L16_Mono",
  "G723",
  "CN",
  "MPA",
  "G728",
  "DVI4_11k",
  "DVI4_22k",
  "G729",
  "CiscoCN",
  NULL, NULL, NULL, NULL, NULL,
  "CelB",
  "JPEG",
  NULL, NULL, NULL, NULL,
  "H261",
  "MPV",
  "MP2T",
  "H263"
};

ostream & operator<<(ostream & o, RTP_DataFrame::PayloadTypes t)
{
  if ((PINDEX)t < PARRAYSIZE(PayloadTypesNames) && PayloadTypesNames[t] != NULL)
    o << PayloadTypesNames[t];
  else
    o << "[pt=" << (int)t << ']';
  return o;
}

#endif

/////////////////////////////////////////////////////////////////////////////

RTP_MultiDataFrame::RTP_MultiDataFrame(const BYTE * buffer, PINDEX length)
: PBYTEArray(buffer,length)
{
}

RTP_MultiDataFrame::RTP_MultiDataFrame(DWORD id, const BYTE * buffer, PINDEX rtplen)
: PBYTEArray(rtplen+4)
{
   memcpy(theArray+4,buffer,rtplen);
   *(PUInt32b *)&theArray[0] = id;
}

RTP_MultiDataFrame::RTP_MultiDataFrame(PINDEX rtplen)
: PBYTEArray(rtplen+4)
{
}

int  RTP_MultiDataFrame::GetMultiHeaderSize() const
{
    return 4;
}

DWORD RTP_MultiDataFrame::GetMultiplexID() const
{
   return *(PUInt32b *)&theArray[0];
}

void RTP_MultiDataFrame::SetMulitplexID(DWORD id)
{
   *(PUInt32b *)&theArray[0] = id;
}

void RTP_MultiDataFrame::GetRTPPayload(RTP_DataFrame & frame) const
{
     int sz = GetSize()- GetMultiHeaderSize();
     frame.SetPayloadSize(sz - frame.GetHeaderSize());
     memcpy(theArray+GetMultiHeaderSize(), frame.GetPointer(), sz);
}

void RTP_MultiDataFrame::SetRTPPayload(RTP_DataFrame & frame)
{
    int sz = frame.GetPayloadSize() + frame.GetHeaderSize();
    SetSize(sz+ GetMultiHeaderSize());
    memcpy(frame.GetPointer(),theArray+GetMultiHeaderSize(), sz);
}

/////////////////////////////////////////////////////////////////////////////

RTP_ControlFrame::RTP_ControlFrame(PINDEX sz)
  : PBYTEArray(sz)
{
  compoundOffset = 0;
  compoundSize = 0;
  theArray[0] = '\x80'; // Set version 2
}


void RTP_ControlFrame::SetCount(unsigned count)
{
  PAssert(count < 32, PInvalidParameter);
  theArray[compoundOffset] &= 0xe0;
  theArray[compoundOffset] |= count;
}


void RTP_ControlFrame::SetPayloadType(unsigned t)
{
  PAssert(t < 256, PInvalidParameter);
  theArray[compoundOffset+1] = (BYTE)t;
}


void RTP_ControlFrame::SetPayloadSize(PINDEX sz)
{
  sz = (sz+3)/4;
  PAssert(sz <= 0xffff, PInvalidParameter);

  compoundSize = compoundOffset+4*(sz+1);
  SetMinSize(compoundSize+1);
  *(PUInt16b *)&theArray[compoundOffset+2] = (WORD)sz;
}


PBoolean RTP_ControlFrame::ReadNextCompound()
{
  compoundOffset += GetPayloadSize()+4;
  if (compoundOffset+4 > GetSize())
    return FALSE;
  return compoundOffset+GetPayloadSize()+4 <= GetSize();
}


PBoolean RTP_ControlFrame::WriteNextCompound()
{
  compoundOffset += GetPayloadSize()+4;
  if (!SetMinSize(compoundOffset+4))
    return FALSE;

  theArray[compoundOffset] = '\x80'; // Set version 2
  theArray[compoundOffset+1] = 0;    // Set payload type to illegal
  theArray[compoundOffset+2] = 0;    // Set payload size to zero
  theArray[compoundOffset+3] = 0;
  return TRUE;
}


RTP_ControlFrame::SourceDescription & RTP_ControlFrame::AddSourceDescription(DWORD src)
{
  SetPayloadType(RTP_ControlFrame::e_SourceDescription);

  PINDEX index = GetCount();
  SetCount(index+1);

  PINDEX originalPayloadSize = index != 0 ? GetPayloadSize() : 0;
  SetPayloadSize(originalPayloadSize+sizeof(SourceDescription));
  SourceDescription & sdes = *(SourceDescription *)(GetPayloadPtr()+originalPayloadSize);
  sdes.src = src;
  sdes.item[0].type = e_END;
  return sdes;
}


RTP_ControlFrame::SourceDescription::Item &
        RTP_ControlFrame::AddSourceDescriptionItem(SourceDescription & sdes,
                                                   unsigned type,
                                                   const PString & data)
{
  PINDEX dataLength = data.GetLength();
  SetPayloadSize(GetPayloadSize()+sizeof(SourceDescription::Item)+dataLength-1);

  SourceDescription::Item * item = sdes.item;
  while (item->type != e_END)
    item = item->GetNextItem();

  item->type = (BYTE)type;
  item->length = (BYTE)dataLength;
  memcpy(item->data, (const char *)data, item->length);

  item->GetNextItem()->type = e_END;
  return *item;
}


void RTP_ControlFrame::ReceiverReport::SetLostPackets(unsigned packets)
{
  lost[0] = (BYTE)(packets >> 16);
  lost[1] = (BYTE)(packets >> 8);
  lost[2] = (BYTE)packets;
}

/////////////////////////////////////////////////////////////////////////////

RTP_MultiControlFrame::RTP_MultiControlFrame(BYTE const * buffer, PINDEX length)
: PBYTEArray(buffer,length)
{
}

RTP_MultiControlFrame::RTP_MultiControlFrame(PINDEX rtplen)
: PBYTEArray(rtplen+4)
{
}

int  RTP_MultiControlFrame::GetMultiHeaderSize() const
{
    return 4;
}

WORD RTP_MultiControlFrame::GetMultiplexID() const
{
   return *(PUInt16b *)&theArray[0];
}

void RTP_MultiControlFrame::SetMulitplexID(WORD id)
{
   *(PUInt16b *)&theArray[0] = id;
}

void RTP_MultiControlFrame::GetRTCPPayload(RTP_ControlFrame & frame) const
{
     int sz = GetSize()- GetMultiHeaderSize();
     frame.SetPayloadSize(sz);
     memcpy(theArray+GetMultiHeaderSize(), frame.GetPointer(), sz);
}

void RTP_MultiControlFrame::SetRTCPPayload(RTP_ControlFrame & frame)
{
    int sz = frame.GetSize();
    SetSize(sz+ GetMultiHeaderSize());
    memcpy(frame.GetPointer(), theArray+GetMultiHeaderSize(), sz);
}


/////////////////////////////////////////////////////////////////////////////

void RTP_UserData::OnTxStatistics(const RTP_Session & /*session*/) const
{
}


void RTP_UserData::OnRxStatistics(const RTP_Session & /*session*/) const
{
}

void RTP_UserData::OnFinalStatistics(const RTP_Session & /*session*/ ) const
{
}

void RTP_UserData::OnRxSenderReport(unsigned /*sessionID*/,
                                    const RTP_Session::SenderReport & /*send*/,
                                    const RTP_Session::ReceiverReportArray & /*recv*/
                                    ) const
{
}

/////////////////////////////////////////////////////////////////////////////

RTP_Session::RTP_Session(
#ifdef H323_RTP_AGGREGATE

                         PHandleAggregator * _aggregator, 
#endif
                         unsigned id, RTP_UserData * data)
  : canonicalName(PProcess::Current().GetUserName()),
    toolName(PProcess::Current().GetName()),
    reportTimeInterval(0, 12),  // Seconds
    firstDataReceivedTime(0),
    reportTimer(reportTimeInterval)
#ifdef H323_RTP_AGGREGATE
    ,aggregator(_aggregator)
#endif
{
  sessionID = (BYTE)id;
  if (sessionID <= 0) {
      PTRACE(2,"RTP\tWARNING: Session ID <= 0 Invalid SessionID.");
  } else if (sessionID > 256) {
      PTRACE(2,"RTP\tWARNING: Session ID " << sessionID << " Invalid SessionID.");
  }

  referenceCount = 1;
  userData = data;

#ifdef H323_AUDIO_CODECS
  jitter = NULL;
#endif

  ignoreOtherSources = TRUE;
  ignoreOtherSourcesCount = 0;
  ignoreOtherSourceMaximum = 10;
  ignoreOutOfOrderPackets = TRUE;
  syncSourceOut = PRandom::Number();
  syncSourceIn = 0;
  txStatisticsInterval = 50; //100;  // Number of data packets between tx reports
  rxStatisticsInterval = 50; //100;  // Number of data packets between rx reports
  lastSentSequenceNumber = (WORD)PRandom::Number();
  expectedSequenceNumber = 0;
  lastRRSequenceNumber = 0;
  consecutiveOutOfOrderPackets = 0;

  packetsSent = 0;
  octetsSent = 0;
  packetsReceived = 0;
  octetsReceived = 0;
  packetsLost = 0;
  packetsOutOfOrder = 0;
  averageSendTime = 0;
  maximumSendTime = 0;
  minimumSendTime = 0;
  averageReceiveTime = 0;
  maximumReceiveTime = 0;
  minimumReceiveTime = 0;
  jitterLevel = 0;
  maximumJitterLevel = 0;

  txStatisticsCount = 0;
  rxStatisticsCount = 0;
  averageSendTimeAccum = 0;
  maximumSendTimeAccum = 0;
  minimumSendTimeAccum = 0xffffffff;
  averageReceiveTimeAccum = 0;
  maximumReceiveTimeAccum = 0;
  minimumReceiveTimeAccum = 0xffffffff;
  packetsLostSinceLastRR = 0;
  lastTransitTime = 0;

  localAddress = PString();
  remoteAddress = PString(); 

  avSyncData = false;
}


RTP_Session::~RTP_Session()
{
  userData->OnFinalStatistics(*this);

  PTRACE_IF(2, packetsSent != 0 || packetsReceived != 0,
            "RTP\tFinal statistics: Session " << sessionID << "\n"
            "    packetsSent       = " << packetsSent << "\n"
            "    octetsSent        = " << octetsSent << "\n"
            "    averageSendTime   = " << averageSendTime << "\n"
            "    maximumSendTime   = " << maximumSendTime << "\n"
            "    minimumSendTime   = " << minimumSendTime << "\n"
            "    packetsReceived   = " << packetsReceived << "\n"
            "    octetsReceived    = " << octetsReceived << "\n"
            "    packetsLost       = " << packetsLost << "\n"
            "    packetsTooLate    = " << GetPacketsTooLate() << "\n"
            "    packetsOutOfOrder = " << packetsOutOfOrder << "\n"
            "    averageReceiveTime= " << averageReceiveTime << "\n"
            "    maximumReceiveTime= " << maximumReceiveTime << "\n"
            "    minimumReceiveTime= " << minimumReceiveTime << "\n"
            "    averageJitter     = " << (jitterLevel >> 7) << "\n"
            "    maximumJitter     = " << (maximumJitterLevel >> 7)
            );
  delete userData;

#ifdef H323_AUDIO_CODECS
  delete jitter;
#endif
}

void RTP_Session::SetSessionID(unsigned id)
{
    sessionID = id;
}

PString RTP_Session::GetCanonicalName() const
{
  PWaitAndSignal mutex(reportMutex);
  PString s = canonicalName;
  s.MakeUnique();
  return s;
}


void RTP_Session::SetCanonicalName(const PString & name)
{
  PWaitAndSignal mutex(reportMutex);
  canonicalName = name;
}


PString RTP_Session::GetToolName() const
{
  PWaitAndSignal mutex(reportMutex);
  PString s = toolName;
  s.MakeUnique();
  return s;
}


void RTP_Session::SetToolName(const PString & name)
{
  PWaitAndSignal mutex(reportMutex);
  toolName = name;
}


void RTP_Session::SetUserData(RTP_UserData * data)
{
  delete userData;
  userData = data;
}


void RTP_Session::SetJitterBufferSize(unsigned minJitterDelay,
                                      unsigned maxJitterDelay,
                                      PINDEX stackSize)
{
  if (minJitterDelay == 0 && maxJitterDelay == 0) {
#ifdef H323_AUDIO_CODECS
    delete jitter;
    jitter = NULL;
#endif
  }
#ifdef H323_AUDIO_CODECS
  else if (jitter != NULL) {
    jitter->SetDelay(minJitterDelay, maxJitterDelay);
  }
#endif
  else {
    SetIgnoreOutOfOrderPackets(FALSE);
#ifdef H323_AUDIO_CODECS
    jitter = new RTP_JitterBuffer(*this, minJitterDelay, maxJitterDelay, stackSize);
    jitter->Resume(
#ifdef H323_RTP_AGGREGATE
      aggregator
#endif
      );
#endif
  }
}


unsigned RTP_Session::GetJitterBufferSize() const
{
  return
#ifdef H323_AUDIO_CODECS
  jitter != NULL ? jitter->GetJitterTime() :
#endif
  0;
}


PBoolean RTP_Session::ReadBufferedData(DWORD timestamp, RTP_DataFrame & frame)
{
#ifdef H323_AUDIO_CODECS
  if (jitter != NULL)
    return jitter->ReadData(timestamp, frame);
  else
#endif
    return ReadData(frame, TRUE);
}

PBoolean RTP_Session::PseudoRead(int & /*selectStatus*/)
{
    return false;
}

bool RTP_Session::AVSyncData(SenderReport & sender)
{
    if (avSyncData) {
        sender = rtpSync;
        avSyncData = false;
        return true;
    }
    return false;
}

void RTP_Session::SetTxStatisticsInterval(unsigned packets)
{
  txStatisticsInterval = PMAX(packets, 2);
  txStatisticsCount = 0;
  averageSendTimeAccum = 0;
  maximumSendTimeAccum = 0;
  minimumSendTimeAccum = 0xffffffff;
}


void RTP_Session::SetRxStatisticsInterval(unsigned packets)
{
  rxStatisticsInterval = PMAX(packets, 2);
  rxStatisticsCount = 0;
  averageReceiveTimeAccum = 0;
  maximumReceiveTimeAccum = 0;
  minimumReceiveTimeAccum = 0xffffffff;
}


void RTP_Session::AddReceiverReport(RTP_ControlFrame::ReceiverReport & receiver)
{
  receiver.ssrc = syncSourceIn;
  receiver.SetLostPackets(packetsLost);

  if (expectedSequenceNumber > lastRRSequenceNumber)
    receiver.fraction = (BYTE)((packetsLostSinceLastRR<<8)/(expectedSequenceNumber - lastRRSequenceNumber));
  else
    receiver.fraction = 0;
  packetsLostSinceLastRR = 0;

  receiver.last_seq = lastRRSequenceNumber;
  lastRRSequenceNumber = expectedSequenceNumber;

  receiver.jitter = jitterLevel >> 4; // Allow for rounding protection bits

  // The following have not been calculated yet.
  receiver.lsr = 0;
  receiver.dlsr = 0;

  PTRACE(3, "RTP\tSentReceiverReport:"
            " ssrc=" << receiver.ssrc
         << " fraction=" << (unsigned)receiver.fraction
         << " lost=" << receiver.GetLostPackets()
         << " last_seq=" << receiver.last_seq
         << " jitter=" << receiver.jitter
         << " lsr=" << receiver.lsr
         << " dlsr=" << receiver.dlsr);
}


RTP_Session::SendReceiveStatus RTP_Session::OnSendData(RTP_DataFrame & frame)
{
  PTimeInterval tick = PTimer::Tick();  // Timestamp set now

  frame.SetSequenceNumber(++lastSentSequenceNumber);
  frame.SetSyncSource(syncSourceOut);

  if (packetsSent != 0 && !frame.GetMarker()) {
    // Only do statistics on subsequent packets
    DWORD diff = (tick - lastSentPacketTime).GetInterval();

    averageSendTimeAccum += diff;
    if (diff > maximumSendTimeAccum)
      maximumSendTimeAccum = diff;
    if (diff < minimumSendTimeAccum)
      minimumSendTimeAccum = diff;
    txStatisticsCount++;
  }

  lastSentTimestamp = frame.GetTimestamp();
  lastSentPacketTime = tick;

  octetsSent += frame.GetPayloadSize();
  packetsSent++;

  // Call the statistics call-back on the first PDU with total count == 1
  if (packetsSent == 1 && userData != NULL)
    userData->OnTxStatistics(*this);

  if (!SendReport())
    return e_AbortTransport;

  if (txStatisticsCount < txStatisticsInterval)
    return e_ProcessPacket;

  txStatisticsCount = 0;

  averageSendTime = averageSendTimeAccum/txStatisticsInterval;
  maximumSendTime = maximumSendTimeAccum;
  minimumSendTime = minimumSendTimeAccum;

  averageSendTimeAccum = 0;
  maximumSendTimeAccum = 0;
  minimumSendTimeAccum = 0xffffffff;

  PTRACE(2, "RTP\tTransmit statistics: "
            " packets=" << packetsSent <<
            " octets=" << octetsSent <<
            " avgTime=" << averageSendTime <<
            " maxTime=" << maximumSendTime <<
            " minTime=" << minimumSendTime
            );

  if (userData != NULL)
    userData->OnTxStatistics(*this);

  return e_ProcessPacket;
}


RTP_Session::SendReceiveStatus RTP_Session::OnReceiveData(const RTP_DataFrame & frame, const RTP_UDP & rtp)
{
  // Check that the PDU is the right version
  if (frame.GetVersion() != RTP_DataFrame::ProtocolVersion)
    return e_IgnorePacket; // Non fatal error, just ignore

  // Check for if a control packet rather than data packet.
  if (frame.GetPayloadType() > RTP_DataFrame::MaxPayloadType)
    return e_IgnorePacket; // Non fatal error, just ignore

  PTimeInterval tick = PTimer::Tick();  // Get timestamp now

  // Have not got SSRC yet, so grab it now
  if (syncSourceIn == 0)
    syncSourceIn = frame.GetSyncSource();

  // Check packet sequence numbers
  if (packetsReceived == 0) {
    expectedSequenceNumber = (WORD)(frame.GetSequenceNumber() + 1);
    firstDataReceivedTime = PTime();
    PTRACE(2, "RTP\tFirst data:"
              " ver=" << frame.GetVersion()
           << " pt=" << frame.GetPayloadType()
           << " psz=" << frame.GetPayloadSize()
           << " m=" << frame.GetMarker()
           << " x=" << frame.GetExtension()
           << " seq=" << frame.GetSequenceNumber()
           << " ts=" << frame.GetTimestamp()
           << " src=" << frame.GetSyncSource()
           << " ccnt=" << frame.GetContribSrcCount());
  }
  else {
    if (ignoreOtherSources && frame.GetSyncSource() != syncSourceIn) {
      PTRACE(2, "RTP\tPacket from SSRC=" << frame.GetSyncSource()
             << " ignored, expecting SSRC=" << syncSourceIn);

      if (ignoreOtherSourcesCount < ignoreOtherSourceMaximum) {
         ignoreOtherSourcesCount++;
         return e_IgnorePacket; // Non fatal error, just ignore
      }

      PTRACE(2, "RTP\tPacket from SSRC=" << frame.GetSyncSource()
             << " " << ignoreOtherSourcesCount << " Consecutive Received. Auto switching to SSRC " << frame.GetSyncSource());
      syncSourceIn = ((RTP_DataFrame &)frame).GetSyncSource();
      expectedSequenceNumber = ((RTP_DataFrame &)frame).GetSequenceNumber();
    }

    if (!ignoreOtherSourcesCount)
        ignoreOtherSourcesCount=0;

    WORD sequenceNumber = frame.GetSequenceNumber();
    if (sequenceNumber == expectedSequenceNumber) {
      expectedSequenceNumber++;
      consecutiveOutOfOrderPackets = 0;
      // Only do statistics on packets after first received in talk burst
      if (!frame.GetMarker()) {
        DWORD diff = (tick - lastReceivedPacketTime).GetInterval();

        averageReceiveTimeAccum += diff;
        if (diff > maximumReceiveTimeAccum)
          maximumReceiveTimeAccum = diff;
        if (diff < minimumReceiveTimeAccum)
          minimumReceiveTimeAccum = diff;
        rxStatisticsCount++;

        // The following has the implicit assumption that something that has jitter
        // is an audio codec and thus is in 8kHz timestamp units.
        diff *= 8;
        long variance = (long)diff - (long)lastTransitTime;
        lastTransitTime = diff;
        if (variance < 0)
          variance = -variance;
        jitterLevel += variance - ((jitterLevel+8) >> 4);
        if (jitterLevel > maximumJitterLevel)
          maximumJitterLevel = jitterLevel;
      }
    }
    else if (sequenceNumber < expectedSequenceNumber) {
      PTRACE(3, "RTP\tOut of order packet, received "
             << sequenceNumber << " expected " << expectedSequenceNumber
             << " ssrc=" << syncSourceIn);
      packetsOutOfOrder++;

      // Check for Cisco bug where sequence numbers suddenly start incrementing
      // from a different base.
      if (++consecutiveOutOfOrderPackets > 10) {
        expectedSequenceNumber = (WORD)(sequenceNumber + 1);
        PTRACE(1, "RTP\tAbnormal change of sequence numbers, adjusting to expect "
               << expectedSequenceNumber << " ssrc=" << syncSourceIn);
      }

      if (ignoreOutOfOrderPackets)
        return e_IgnorePacket; // Non fatal error, just ignore
    }
    else {
      unsigned dropped = sequenceNumber - expectedSequenceNumber;
      packetsLost += dropped;
      packetsLostSinceLastRR += dropped;
      PTRACE(3, "RTP\tDropped " << dropped << " packet(s) at " << sequenceNumber
             << ", ssrc=" << syncSourceIn);
      expectedSequenceNumber = (WORD)(sequenceNumber + 1);
      consecutiveOutOfOrderPackets = 0;
    }
  }

  lastReceivedPacketTime = tick;

  octetsReceived += frame.GetPayloadSize();
  packetsReceived++;

  if (rtp.GetRemoteDataPort() > 0 && localAddress.IsEmpty()) {
      localAddress = rtp.GetLocalAddress().AsString() + ":" + PString(rtp.GetLocalDataPort());
      remoteAddress = rtp.GetRemoteAddress().AsString() + ":" + PString(rtp.GetRemoteDataPort());
  }

  // Call the statistics call-back on the first PDU with total count == 1
  if (packetsReceived == 1 && userData != NULL)
    userData->OnRxStatistics(*this);

  if (!SendReport())
    return e_AbortTransport;

  if (rxStatisticsCount < rxStatisticsInterval)
    return e_ProcessPacket;

  rxStatisticsCount = 0;

  averageReceiveTime = averageReceiveTimeAccum/rxStatisticsInterval;
  maximumReceiveTime = maximumReceiveTimeAccum;
  minimumReceiveTime = minimumReceiveTimeAccum;

  averageReceiveTimeAccum = 0;
  maximumReceiveTimeAccum = 0;
  minimumReceiveTimeAccum = 0xffffffff;
  
  PTRACE(2, "RTP\tReceive statistics: "
            " packets=" << packetsReceived <<
            " octets=" << octetsReceived <<
            " lost=" << packetsLost <<
            " tooLate=" << GetPacketsTooLate() <<
            " order=" << packetsOutOfOrder <<
            " avgTime=" << averageReceiveTime <<
            " maxTime=" << maximumReceiveTime <<
            " minTime=" << minimumReceiveTime <<
            " jitter=" << (jitterLevel >> 7) <<
            " maxJitter=" << (maximumJitterLevel >> 7)
            );

  if (userData != NULL)
    userData->OnRxStatistics(*this);

  return e_ProcessPacket;
}


PBoolean RTP_Session::SendReport()
{
  PWaitAndSignal mutex(reportMutex);

  if (reportTimer.IsRunning())
    return TRUE;

  // Have not got anything yet, do nothing
  if (packetsSent == 0 && packetsReceived == 0) {
    reportTimer = reportTimeInterval;
    return TRUE;
  }

  RTP_ControlFrame report;

  // No packets sent yet, so only send RR
  if (packetsSent == 0) {
    // Send RR as we are not transmitting
    report.SetPayloadType(RTP_ControlFrame::e_ReceiverReport);
    report.SetPayloadSize(4+sizeof(RTP_ControlFrame::ReceiverReport));
    report.SetCount(1);

    PUInt32b * payload = (PUInt32b *)report.GetPayloadPtr();
    *payload = syncSourceOut;
    AddReceiverReport(*(RTP_ControlFrame::ReceiverReport *)&payload[1]);
  }
  else {
    report.SetPayloadType(RTP_ControlFrame::e_SenderReport);
    report.SetPayloadSize(sizeof(RTP_ControlFrame::SenderReport));

    RTP_ControlFrame::SenderReport * sender =
                              (RTP_ControlFrame::SenderReport *)report.GetPayloadPtr();
    sender->ssrc = syncSourceOut;
    PTime now;
    sender->ntp_sec = now.GetTimeInSeconds()+SecondsFrom1900to1970; // Convert from 1970 to 1900
    sender->ntp_frac = now.GetMicrosecond()*4294; // Scale microseconds to "fraction" from 0 to 2^32
    sender->rtp_ts = lastSentTimestamp;
    sender->psent = packetsSent;
    sender->osent = octetsSent;

    PTRACE(3, "RTP\tSentSenderReport: "
                " ssrc=" << sender->ssrc
             << " ntp=" << sender->ntp_sec << '.' << sender->ntp_frac
             << " rtp=" << sender->rtp_ts
             << " psent=" << sender->psent
             << " osent=" << sender->osent);

    if (syncSourceIn != 0) {
      report.SetPayloadSize(sizeof(RTP_ControlFrame::SenderReport) +
                            sizeof(RTP_ControlFrame::ReceiverReport));
      report.SetCount(1);
      AddReceiverReport(*(RTP_ControlFrame::ReceiverReport *)&sender[1]);
    }
  }

  // Add the SDES part to compound RTCP packet
  PTRACE(2, "RTP\tSending SDES: " << canonicalName);
  report.WriteNextCompound();

  RTP_ControlFrame::SourceDescription & sdes = report.AddSourceDescription(syncSourceOut);
  report.AddSourceDescriptionItem(sdes, RTP_ControlFrame::e_CNAME, canonicalName);
  report.AddSourceDescriptionItem(sdes, RTP_ControlFrame::e_TOOL, toolName);

  // Wait a fuzzy amount of time so things don't get into lock step
  int interval = (int)reportTimeInterval.GetMilliSeconds();
  int third = interval/3;
  interval += PRandom::Number()%(2*third);
  interval -= third;
  reportTimer = interval;

  return WriteControl(report);
}


static RTP_Session::ReceiverReportArray
                BuildReceiverReportArray(const RTP_ControlFrame & frame, PINDEX offset)
{
  RTP_Session::ReceiverReportArray reports;

  const RTP_ControlFrame::ReceiverReport * rr = (const RTP_ControlFrame::ReceiverReport *)(frame.GetPayloadPtr()+offset);
  for (PINDEX repIdx = 0; repIdx < (PINDEX)frame.GetCount(); repIdx++) {
    RTP_Session::ReceiverReport * report = new RTP_Session::ReceiverReport;
    report->sourceIdentifier = rr->ssrc;
    report->fractionLost = rr->fraction;
    report->totalLost = rr->GetLostPackets();
    report->lastSequenceNumber = rr->last_seq;
    report->jitter = rr->jitter;
    report->lastTimestamp = (PInt64)(DWORD)rr->lsr;
    report->delay = ((PInt64)rr->dlsr << 16)/1000;
    reports.SetAt(repIdx, report);
    rr++;
  }

  return reports;
}


RTP_Session::SendReceiveStatus RTP_Session::OnReceiveControl(RTP_ControlFrame & frame)
{
  do {
    BYTE * payload = frame.GetPayloadPtr();
    unsigned size = frame.GetPayloadSize();

    switch (frame.GetPayloadType()) {
      case RTP_ControlFrame::e_SenderReport :
        if (size >= (sizeof(RTP_ControlFrame::SenderReport)+frame.GetCount()*sizeof(RTP_ControlFrame::ReceiverReport))) {
          SenderReport sender;
          const RTP_ControlFrame::SenderReport & sr = *(const RTP_ControlFrame::SenderReport *)payload;
          sender.sourceIdentifier = sr.ssrc;
          sender.realTimestamp = PTime(sr.ntp_sec-SecondsFrom1900to1970, sr.ntp_frac/4294);
          sender.realTimestamp1970 = sr.ntp_sec-SecondsFrom1900to1970;
          sender.rtpTimestamp = sr.rtp_ts;
          sender.packetsSent = sr.psent;
          sender.octetsSent = sr.osent;
          // trace the report
          ReceiverReportArray RRs = BuildReceiverReportArray(frame, sizeof(RTP_ControlFrame::SenderReport));
          OnRxSenderReport(sender, RRs);
        }
        else {
          PTRACE(2, "RTP\tSenderReport packet truncated");
        }
        break;

      case RTP_ControlFrame::e_ReceiverReport :
        if (size >= (frame.GetCount()*sizeof(RTP_ControlFrame::ReceiverReport)))
          OnRxReceiverReport(*(const PUInt32b *)payload,
                                      BuildReceiverReportArray(frame, sizeof(PUInt32b)));
        else {
          PTRACE(2, "RTP\tReceiverReport packet truncated");
        }
        break;

      case RTP_ControlFrame::e_SourceDescription :
        if (size >= frame.GetCount()*sizeof(RTP_ControlFrame::SourceDescription)) {
          SourceDescriptionArray descriptions;
          const RTP_ControlFrame::SourceDescription * sdes = (const RTP_ControlFrame::SourceDescription *)payload;
          for (PINDEX srcIdx = 0; srcIdx < (PINDEX)frame.GetCount(); srcIdx++) {
            descriptions.SetAt(srcIdx, new SourceDescription(sdes->src));
            const RTP_ControlFrame::SourceDescription::Item * item = sdes->item;
            while (item->type != RTP_ControlFrame::e_END) {
              descriptions[srcIdx].items.SetAt(item->type, PString(item->data, item->length));
              item = item->GetNextItem();
            }
            sdes = (const RTP_ControlFrame::SourceDescription *)item->GetNextItem();
          }
          OnRxSourceDescription(descriptions);
        }
        else {
          PTRACE(2, "RTP\tSourceDescription packet truncated");
        }
        break;

      case RTP_ControlFrame::e_Goodbye :
        if (size >= 4) {
          PString str;
          unsigned count = frame.GetCount()*4;
          if (size > count)
            str = PString((const char *)(payload+count+1), payload[count]);
          PDWORDArray sources(count);
          for (PINDEX i = 0; i < (PINDEX)count; i++)
            sources[i] = ((const PUInt32b *)payload)[i];
          OnRxGoodbye(sources, str);
        }
        else {
          PTRACE(2, "RTP\tGoodbye packet truncated");
        }
        break;

      case RTP_ControlFrame::e_ApplDefined :
        if (size >= 4) {
          PString str((const char *)(payload+4), 4);
          OnRxApplDefined(str, frame.GetCount(), *(const PUInt32b *)payload,
                          payload+8, frame.GetPayloadSize()-8);
        }
        else {
          PTRACE(2, "RTP\tApplDefined packet truncated");
        }
        break;

      default :
        PTRACE(2, "RTP\tUnknown control payload type: " << frame.GetPayloadType());
    }
  } while (frame.ReadNextCompound());

  return e_ProcessPacket;
}


void RTP_Session::OnRxSenderReport(const SenderReport & PTRACE_PARAM(sender),
                                   const ReceiverReportArray & PTRACE_PARAM(reports))
{
   userData->OnRxSenderReport(sessionID,sender,reports);

#if PTRACING
  PTRACE(3, "RTP\tOnRxSenderReport: " << sender);
  for (PINDEX i = 0; i < reports.GetSize(); i++)
    PTRACE(3, "RTP\tOnRxSenderReport RR: " << reports[i]);
#endif
}


void RTP_Session::OnRxReceiverReport(DWORD PTRACE_PARAM(src),
                                     const ReceiverReportArray & PTRACE_PARAM(reports))
{
#if PTRACING
  PTRACE(3, "RTP\tOnReceiverReport: ssrc=" << src);
  for (PINDEX i = 0; i < reports.GetSize(); i++)
    PTRACE(3, "RTP\tOnReceiverReport RR: " << reports[i]);
#endif
}


void RTP_Session::OnRxSourceDescription(const SourceDescriptionArray & PTRACE_PARAM(description))
{
#if PTRACING
  for (PINDEX i = 0; i < description.GetSize(); i++)
    PTRACE(3, "RTP\tOnSourceDescription: " << description[i]);
#endif
}


void RTP_Session::OnRxGoodbye(const PDWORDArray & PTRACE_PARAM(src),
                              const PString & PTRACE_PARAM(reason))
{
  PTRACE(3, "RTP\tOnGoodbye: \"" << reason << "\" srcs=" << src);
}


void RTP_Session::OnRxApplDefined(const PString & PTRACE_PARAM(type),
                                  unsigned PTRACE_PARAM(subtype),
                                  DWORD PTRACE_PARAM(src),
                                  const BYTE * /*data*/, PINDEX PTRACE_PARAM(size))
{
  PTRACE(3, "RTP\tOnApplDefined: \"" << type << "\"-" << subtype
                          << " " << src << " [" << size << ']');
}


void RTP_Session::ReceiverReport::PrintOn(ostream & strm) const
{
  strm << "ssrc=" << sourceIdentifier
       << " fraction=" << fractionLost
       << " lost=" << totalLost
       << " last_seq=" << lastSequenceNumber
       << " jitter=" << jitter
       << " lsr=" << lastTimestamp
       << " dlsr=" << delay;
}


void RTP_Session::SenderReport::PrintOn(ostream & strm) const
{
  strm << "ssrc=" << sourceIdentifier
       << " ntp=" << realTimestamp.AsString("yyyy/M/d-h:m:s.uuuu")
       << " rtp=" << rtpTimestamp
       << " psent=" << packetsSent
       << " osent=" << octetsSent;
}


void RTP_Session::SourceDescription::PrintOn(ostream & strm) const
{
  static const char * const DescriptionNames[RTP_ControlFrame::NumDescriptionTypes] = {
    "END", "CNAME", "NAME", "EMAIL", "PHONE", "LOC", "TOOL", "NOTE", "PRIV"
  };

  strm << "ssrc=" << sourceIdentifier;
  for (PINDEX i = 0; i < items.GetSize(); i++) {
    strm << "\n  item[" << i << "]: type=";
    unsigned typeNum = items.GetKeyAt(i);
    if (typeNum < PARRAYSIZE(DescriptionNames))
      strm << DescriptionNames[typeNum];
    else
      strm << typeNum;
    strm << " data=\""
         << items.GetDataAt(i)
         << '"';
  }
}


DWORD RTP_Session::GetPacketsTooLate() const
{
  return
#ifdef H323_AUDIO_CODECS
    jitter != NULL ? jitter->GetPacketsTooLate() :
#endif
  0;
}


/////////////////////////////////////////////////////////////////////////////

RTP_SessionManager::RTP_SessionManager()
{
  enumerationIndex = P_MAX_INDEX;
}


RTP_SessionManager::RTP_SessionManager(const RTP_SessionManager & sm)
  : sessions(sm.sessions)
{
  enumerationIndex = P_MAX_INDEX;
}


RTP_SessionManager & RTP_SessionManager::operator=(const RTP_SessionManager & sm)
{
  PWaitAndSignal m1(mutex);
  PWaitAndSignal m2(sm.mutex);
  sessions   = sm.sessions;
  return *this;
}


RTP_Session * RTP_SessionManager::UseSession(unsigned sessionID)
{
  mutex.Wait();

  RTP_Session * session = sessions.GetAt(sessionID);
  if (session == NULL)
    return NULL;  // Deliberately have not release mutex here! See AddSession.

  PTRACE(3, "RTP\tFound existing session " << sessionID);
  session->IncrementReference();

  mutex.Signal();
  return session;
}


void RTP_SessionManager::AddSession(RTP_Session * session)
{
  if (PAssertNULL(session) != NULL) {
    PTRACE(2, "RTP\tAdding session " << *session);
    sessions.SetAt(session->GetSessionID(), session);
  }

  // The following is the mutex.Signal() that was not done in the UseSession()
  mutex.Signal();
}

void RTP_SessionManager::MoveSession(unsigned oldSessionID, unsigned newSessionID)
{
  PTRACE(2, "RTP\tMoving session " << oldSessionID << " to " << newSessionID);

  mutex.Wait();

  if (sessions.Contains(oldSessionID)) {
      RTP_Session & session = sessions[oldSessionID];
      sessions.SetAt(newSessionID, &session);
      //sessions.RemoveAt(oldSessionID);
  }

  mutex.Signal();
}

void RTP_SessionManager::ReleaseSession(unsigned sessionID)
{
  PTRACE(2, "RTP\tReleasing session " << sessionID);

  mutex.Wait();

  if (sessions.Contains(sessionID)) {
    if (sessions[sessionID].DecrementReference()) {
      PTRACE(3, "RTP\tDeleting session " << sessionID);
      sessions[sessionID].SetJitterBufferSize(0, 0);
      sessions.SetAt(sessionID, NULL);
    }
  }

  mutex.Signal();
}


RTP_Session * RTP_SessionManager::GetSession(unsigned sessionID) const
{
  PWaitAndSignal wait(mutex);
  if (!sessions.Contains(sessionID))
    return NULL;

  PTRACE(3, "RTP\tFound existing session " << sessionID);
#if PTLIB_VER >= 2110
  return const_cast<RTP_Session *>(&sessions[sessionID]);
#else
  return &sessions[sessionID];
#endif
}


RTP_Session * RTP_SessionManager::First()
{
  mutex.Wait();

  enumerationIndex = 0;
  return Next();
}


RTP_Session * RTP_SessionManager::Next()
{
  if (enumerationIndex < sessions.GetSize())
    return &sessions.GetDataAt(enumerationIndex++);

  Exit();
  return NULL;
}


void RTP_SessionManager::Exit()
{
  enumerationIndex = P_MAX_INDEX;
  mutex.Signal();
}


/////////////////////////////////////////////////////////////////////////////

static void SetMinBufferSize(PUDPSocket & sock, int buftype)
{
  int sz = 0;
  if (sock.GetOption(buftype, sz)) {
    if (sz >= UDP_BUFFER_SIZE)
      return;
  }

  if (!sock.SetOption(buftype, UDP_BUFFER_SIZE)) {
    PTRACE(1, "RTP_UDP\tSetOption(" << buftype << ") failed: " << sock.GetErrorText());
  }
}


RTP_UDP::RTP_UDP(
#ifdef H323_RTP_AGGREGATE
                 PHandleAggregator * _aggregator, 
#endif
                 unsigned id, PBoolean _remoteIsNAT)
  : RTP_Session(
#ifdef H323_RTP_AGGREGATE
  _aggregator, 
#endif
  id),
    remoteAddress(0),
    remoteTransmitAddress(0),
    remoteIsNAT(_remoteIsNAT)
{
  remoteDataPort = 0;
  remoteControlPort = 0;
  shutdownRead = FALSE;
  shutdownWrite = FALSE;
  dataSocket = NULL;
  controlSocket = NULL;
  appliedQOS = FALSE;
  enableGQOS = FALSE;
  successiveWrongAddresses = 0;
}


RTP_UDP::~RTP_UDP()
{
  Close(TRUE);
  Close(FALSE);

  delete dataSocket;
  delete controlSocket;
}


void RTP_UDP::ApplyQOS(const PIPSocket::Address & addr)
{
  if (dataSocket != NULL)
    dataSocket->SetSendAddress(addr,GetRemoteDataPort());
  else if (controlSocket != NULL)
    controlSocket->SetSendAddress(addr,GetRemoteControlPort());

  appliedQOS = TRUE;
}


PBoolean RTP_UDP::ModifyQOS(RTP_QOS * rtpqos)
{
  PBoolean retval = FALSE;

  if (rtpqos == NULL)
    return retval;

#if P_QOS
  if (dataSocket != NULL)
    retval &= dataSocket->ModifyQoSSpec(&(rtpqos->dataQoS));
  else if (controlSocket != NULL)
    retval = controlSocket->ModifyQoSSpec(&(rtpqos->ctrlQoS));
#endif

  appliedQOS = FALSE;
  return retval;
}

void RTP_UDP::EnableGQoS(PBoolean success)
{
    enableGQOS = success;
}

#if P_QOS
PQoS & RTP_UDP::GetQOS()
{
    if (dataSocket != NULL) 
       return dataSocket->GetQoSSpec();
    else if (controlSocket != NULL) 
       return controlSocket->GetQoSSpec();
    else
       return *new PQoS(); 
}
#endif

PBoolean RTP_UDP::Open(PIPSocket::Address _localAddress,
                   WORD portBase, WORD portMax,
                   BYTE tos,
#ifdef P_STUN
                   const H323Connection & connection,
                   PNatMethod * meth,
#else
                   const H323Connection &,
                   void *,
#endif
                   RTP_QOS * rtpQos)
{
  // save local address 
  localAddress = _localAddress;

  localDataPort    = (WORD)(portBase&0xfffe);
  localControlPort = (WORD)(localDataPort + 1);

  delete dataSocket;
  delete controlSocket;
  dataSocket = NULL;
  controlSocket = NULL;

  PQoS * dataQos = NULL;
  PQoS * ctrlQos = NULL;
  if (rtpQos != NULL) {
#if P_QOS
    dataQos = &(rtpQos->dataQoS);
    ctrlQos = &(rtpQos->ctrlQoS);
#endif
  }

#ifdef P_STUN
  if (meth != NULL) {
    H323Connection::SessionInformation * info = 
         connection.BuildSessionInformation(GetSessionID());

#if PTLIB_VER > 260
    if (meth->CreateSocketPair(dataSocket, controlSocket, localAddress,(void *)info)) {
#else
    if (meth->CreateSocketPair(dataSocket, controlSocket, localAddress)) {
#endif
      dataSocket->GetLocalAddress(localAddress, localDataPort);
      controlSocket->GetLocalAddress(localAddress, localControlPort);
      PTRACE(4, "RTP\tNAT Method " << meth->GetName() << " created NAT ports " << localDataPort << " " << localControlPort);
    }
    else
      PTRACE(1, "RTP\tNAT could not create socket pair!");

    delete info;
  }
#endif

  if (dataSocket == NULL || controlSocket == NULL) {
    dataSocket = new PUDPSocket(dataQos);
    controlSocket = new PUDPSocket(ctrlQos);
    while (!dataSocket->Listen(localAddress,    1, localDataPort) ||
           !controlSocket->Listen(localAddress, 1, localControlPort)) {
      dataSocket->Close();
      controlSocket->Close();
      if ((localDataPort > portMax) || (localDataPort > 0xfffd))
        return FALSE; // If it ever gets to here the OS has some SERIOUS problems!
      localDataPort    += 2;
      localControlPort += 2;
    }
  }

  // Set the IP Type Of Service field for prioritisation of media UDP packets
  // through some Cisco routers and Linux boxes
  if (!dataSocket->SetOption(IP_TOS, tos, IPPROTO_IP)) {
    PTRACE(1, "RTP_UDP\tCould not set TOS field in IP header: " << dataSocket->GetErrorText());
  }

  // Increase internal buffer size on media UDP sockets
  SetMinBufferSize(*dataSocket,    SO_RCVBUF);
  SetMinBufferSize(*dataSocket,    SO_SNDBUF);
  SetMinBufferSize(*controlSocket, SO_RCVBUF);
  SetMinBufferSize(*controlSocket, SO_SNDBUF);

  shutdownRead = FALSE;
  shutdownWrite = FALSE;

  if (canonicalName.Find('@') == P_MAX_INDEX)
    canonicalName += '@' + GetLocalHostName();

  PTRACE(2, "RTP_UDP\tSession " << sessionID << " created: "
         << localAddress << ':' << localDataPort << '-' << localControlPort
         << " ssrc=" << syncSourceOut);
  
  return TRUE;
}


void RTP_UDP::Reopen(PBoolean reading)
{
  if (reading)
    shutdownRead = FALSE;
  else
    shutdownWrite = FALSE;
}


void RTP_UDP::Close(PBoolean reading)
{
  if (reading) {
    if (!shutdownRead) {
      PTRACE(3, "RTP_UDP\tSession " << sessionID << ", Shutting down read.");
      syncSourceIn = 0;
      shutdownRead = TRUE;
      if (dataSocket != NULL && controlSocket != NULL) {
        PIPSocket::Address addr;
        controlSocket->GetLocalAddress(addr);
        if (addr.IsAny())
          PIPSocket::GetHostAddress(addr);
        dataSocket->WriteTo("", 1, addr, controlSocket->GetPort());
      }
    }
  }
  else {
    PTRACE(3, "RTP_UDP\tSession " << sessionID << ", Shutting down write.");
    shutdownWrite = TRUE;
  }
}


PString RTP_UDP::GetLocalHostName()
{
  return PIPSocket::GetHostName();
}


PBoolean RTP_UDP::SetRemoteSocketInfo(PIPSocket::Address address, WORD port, PBoolean isDataPort)
{
  if (remoteIsNAT) {
    PTRACE(3, "RTP_UDP\tIgnoring remote socket info as remote is behind NAT");
    return TRUE;
  }

  PTRACE(3, "RTP_UDP\tSetRemoteSocketInfo: session=" << sessionID << ' '
         << (isDataPort ? "data" : "control") << " channel, "
            "new=" << address << ':' << port << ", "
            "local=" << localAddress << ':' << localDataPort << '-' << localControlPort << ", "
            "remote=" << remoteAddress << ':' << remoteDataPort << '-' << remoteControlPort);

  if (localAddress == address && (isDataPort ? localDataPort : localControlPort) == port)
    return TRUE;

  remoteAddress = address;

  if (isDataPort) {
    remoteDataPort = port;
    if (remoteControlPort == 0)
      remoteControlPort = (WORD)(port + 1);
  }
  else {
    remoteControlPort = port;
    if (remoteDataPort == 0)
      remoteDataPort = (WORD)(port - 1);
  }

  if (!appliedQOS)
      ApplyQOS(remoteAddress);

  return remoteAddress != 0 && port != 0;
}

PBoolean RTP_UDP::PseudoRead(int & selectStatus)
{
#if PTLIB_VER >= 290
    return (controlSocket->DoPseudoRead(selectStatus) || 
            dataSocket->DoPseudoRead(selectStatus));
#else
    return false;
#endif
}

PBoolean RTP_UDP::ReadData(RTP_DataFrame & frame, PBoolean loop)
{
  do {
#ifdef H323_RTP_AGGREGATE
    PTime start;
#endif
    int selectStatus = 0;

    if (!PseudoRead(selectStatus))
       selectStatus = PSocket::Select(*dataSocket, *controlSocket, reportTimer);
#ifdef H323_RTP_AGGREGATE
    unsigned duration = (unsigned)(PTime() - start).GetMilliSeconds();
    if (duration > 50) {
      PTRACE(4, "Warning: aggregator read routine was of extended duration = " << duration << " msecs");
    }
#endif

    if (shutdownRead) {
      PTRACE(3, "RTP_UDP\tSession " << sessionID << ", Read shutdown.");
      shutdownRead = FALSE;
      return FALSE;
    }

    switch (selectStatus) {
      case -2 :
        if (ReadControlPDU() == e_AbortTransport)
          return FALSE;
        break;

      case -3 :
        if (ReadControlPDU() == e_AbortTransport)
          return FALSE;
        // Then do -1 case

      case -1 :
        switch (ReadDataPDU(frame)) {
          case e_ProcessPacket :
            if (!shutdownRead)
              return TRUE;
          case e_IgnorePacket :
            break;
          case e_AbortTransport :
            return FALSE;
        }
        break;

      case 0 :
        PTRACE(5, "RTP_UDP\tSession " << sessionID << ", check for sending report.");
        if (!SendReport())
          return FALSE;
        break;

      case PSocket::Interrupted:
        PTRACE(3, "RTP_UDP\tSession " << sessionID << ", Interrupted.");
        return FALSE;

      default :
        PTRACE(1, "RTP_UDP\tSession " << sessionID << ", Select error: "
                << PChannel::GetErrorText((PChannel::Errors)selectStatus));
        return FALSE;
    }
  } while (loop);

  return TRUE;
}


RTP_Session::SendReceiveStatus RTP_UDP::ReadDataOrControlPDU(PUDPSocket & socket,
                                                             PBYTEArray & frame,
                                                             PBoolean fromDataChannel)
{
#if PTRACING
  const char * channelName = fromDataChannel ? "Data" : "Control";
#endif
  PIPSocket::Address addr;
  WORD port;

  if (socket.ReadFrom(frame.GetPointer(), frame.GetSize(), addr, port)) {
    if (ignoreOtherSources) {

      // If remote address never set from higher levels, then try and figure
      // it out from the first packet received.
      if (remoteAddress.IsAny() || !remoteAddress.IsValid()) {
        remoteAddress = addr;
        PTRACE(4, "RTP\tSet remote address from first " << channelName
               << " PDU from " << addr << ':' << port);
      }
      if (fromDataChannel) {
        if (remoteDataPort == 0)
          remoteDataPort = port;
      }
      else {
        if (remoteControlPort == 0)
          remoteControlPort = port;
      }

      if (remoteTransmitAddress.IsAny() || !remoteTransmitAddress.IsValid())
             remoteTransmitAddress = addr;

      else if (remoteTransmitAddress != addr) {
#ifdef H323_H46024A
          if (socket.IsAlternateAddress(addr,port)) {
                remoteTransmitAddress = addr;
                remoteAddress = addr;
                appliedQOS = false;
                if (fromDataChannel) {
                    remoteDataPort = port;
                    // Fixes to makes sure sync,stats and jitter don't get screwed up 
                    syncSourceIn = ((RTP_DataFrame &)frame).GetSyncSource();
                    expectedSequenceNumber = ((RTP_DataFrame &)frame).GetSequenceNumber();
#ifdef H323_AUDIO_CODECS
                    if (jitter != NULL)  jitter->ResetFirstWrite();
#endif
                } else
                    remoteControlPort = port;
          } else
#endif
          {
            successiveWrongAddresses++;
            if (successiveWrongAddresses < 5) {
                PTRACE(1, "RTP_UDP\tSession " << sessionID << ", "
                       << channelName << " PDU from incorrect host, "
                          " is " << addr << " should be " << remoteTransmitAddress);
                return RTP_Session::e_IgnorePacket;
            }

            PTRACE(1, "RTP_UDP\tSession " << sessionID << ", "
                       << channelName << " PDU from incorrect host limit switching to " << addr);

            remoteTransmitAddress = addr;
            remoteAddress = addr;
            appliedQOS = false;
               if (fromDataChannel) {
                    remoteDataPort = port;
                    // Fixes to makes sure sync,stats and jitter don't get screwed up 
                    syncSourceIn = ((RTP_DataFrame &)frame).GetSyncSource();
                    expectedSequenceNumber = ((RTP_DataFrame &)frame).GetSequenceNumber();
#ifdef H323_AUDIO_CODECS
                    if (jitter != NULL)  jitter->ResetFirstWrite();
#endif                 
               } else
                 remoteControlPort = port;
          }
      }
    }
    successiveWrongAddresses=0;

    if (!remoteAddress.IsAny() && remoteAddress.IsValid() && !appliedQOS) 
      ApplyQOS(remoteAddress);

    return RTP_Session::e_ProcessPacket;
  }

  switch (socket.GetErrorNumber()) {
    case ECONNRESET :
    case ECONNREFUSED :
      PTRACE(2, "RTP_UDP\tSession " << sessionID << ", "
             << channelName << " port on remote not ready.");
      return RTP_Session::e_IgnorePacket;

    case EMSGSIZE :
      PTRACE(2, "RTP_UDP\tSession " << sessionID << ", " << channelName
             << " read packet too large");
      return RTP_Session::e_IgnorePacket;

    case EAGAIN :
      // Shouldn't happen, but it does.
      return RTP_Session::e_IgnorePacket;

    default:
      PTRACE(1, "RTP_UDP\t" << channelName << " read error ("
             << socket.GetErrorNumber(PChannel::LastReadError) << "): "
             << socket.GetErrorText(PChannel::LastReadError));
      return RTP_Session::e_AbortTransport;
  }
}


RTP_Session::SendReceiveStatus RTP_UDP::ReadDataPDU(RTP_DataFrame & frame)
{
  SendReceiveStatus status = ReadDataOrControlPDU(*dataSocket, frame, TRUE);
  if (status != e_ProcessPacket)
    return status;

  // Check received PDU is big enough
  PINDEX pduSize = dataSocket->GetLastReadCount();
  if (pduSize < RTP_DataFrame::MinHeaderSize || pduSize < frame.GetHeaderSize()) {
    PTRACE(2, "RTP_UDP\tSession " << sessionID
           << ", Received data packet too small: " << pduSize << " bytes");
    return e_IgnorePacket;
  }

  frame.SetPayloadSize(pduSize - frame.GetHeaderSize());
  return OnReceiveData(frame,*this);
}


RTP_Session::SendReceiveStatus RTP_UDP::ReadControlPDU()
{
  RTP_ControlFrame frame(2048);

  SendReceiveStatus status = ReadDataOrControlPDU(*controlSocket, frame, FALSE);
  if (status != e_ProcessPacket)
    return status;

  PINDEX pduSize = controlSocket->GetLastReadCount();
  if (pduSize < 4 || pduSize < 4+frame.GetPayloadSize()) {
    PTRACE(2, "RTP_UDP\tSession " << sessionID
           << ", Received control packet too small: " << pduSize << " bytes");
    return e_IgnorePacket;
  }

  frame.SetSize(pduSize);
  return OnReceiveControl(frame);
}


PBoolean RTP_UDP::PreWriteData(RTP_DataFrame & frame)
{
  if (shutdownWrite) {
    PTRACE(3, "RTP_UDP\tSession " << sessionID << ", Write shutdown.");
    shutdownWrite = FALSE;
    return FALSE;
  }

  // Trying to send a PDU before we are set up!
  if (remoteAddress.IsAny() || !remoteAddress.IsValid() || remoteDataPort == 0)
    return TRUE;

  switch (OnSendData(frame)) {
    case e_ProcessPacket :
      break;
    case e_IgnorePacket :
      return TRUE;
    case e_AbortTransport :
      return FALSE;
  }
 
  return TRUE;
}

PBoolean RTP_UDP::WriteData(RTP_DataFrame & frame)
{

  while (!dataSocket->WriteTo(frame.GetPointer(),
                             frame.GetHeaderSize()+frame.GetPayloadSize(),
                             remoteAddress, remoteDataPort)) {
    switch (dataSocket->GetErrorNumber()) {
      case ECONNRESET :
      case ECONNREFUSED :
        PTRACE(2, "RTP_UDP\tSession " << sessionID << ", data port on remote not ready.");
        break;

      default:
        PTRACE(1, "RTP_UDP\tSession " << sessionID
               << ", Write error on data port ("
               << dataSocket->GetErrorNumber(PChannel::LastWriteError) << "): "
               << dataSocket->GetErrorText(PChannel::LastWriteError));
        return FALSE;
    }
  }

  return TRUE;
}


PBoolean RTP_UDP::WriteControl(RTP_ControlFrame & frame)
{
  // Trying to send a PDU before we are set up!
  if (remoteAddress.IsAny() || !remoteAddress.IsValid() || remoteControlPort == 0)
    return true;

  while (!controlSocket->WriteTo(frame.GetPointer(), frame.GetCompoundSize(),
                                remoteAddress, remoteControlPort)) {
    switch (controlSocket->GetErrorNumber()) {
      case ECONNRESET :
      case ECONNREFUSED :
        PTRACE(2, "RTP_UDP\tSession " << sessionID << ", control port on remote not ready.");
        break;

      default:
        PTRACE(1, "RTP_UDP\tSession " << sessionID
               << ", Write error on control port ("
               << controlSocket->GetErrorNumber(PChannel::LastWriteError) << "): "
               << controlSocket->GetErrorText(PChannel::LastWriteError));
        return false;
    }
  }

  return true;
}


/////////////////////////////////////////////////////////////////////////////
