/*
 * codecs.cxx
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
 * $ Id $
 *
 */

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "codecs.h"
#endif

#ifdef _MSC_VER
#include "../include/codecs.h"
#else
#include "codecs.h"
#endif

#include "channels.h"
#include "h323pdu.h"
#include "h323con.h"

#ifdef H323_AEC
#include <etc/h323aec.h>
#endif // H323_AEC

extern "C" {
#include "g711.h"
};

#define new PNEW

/////////////////////////////////////////////////////////////////////////////

H323Codec::H323Codec(const OpalMediaFormat & fmt, Direction dir)
  : mediaFormat(fmt)
{
  logicalChannel = NULL;
  direction = dir;

  lastSequenceNumber = 1;
  rawDataChannel = NULL;
  deleteChannel  = FALSE;

  rtpInformation.m_sessionID=0;
  rtpInformation.m_sendTime=0;
  rtpInformation.m_timeStamp=0;
  rtpInformation.m_clockRate=0;
  rtpInformation.m_frameLost=0;
  rtpInformation.m_recvTime=0;
  rtpInformation.m_frame=NULL;

  rtpSync.m_realTimeStamp = 0;
  rtpSync.m_rtpTimeStamp = 0;
}


PBoolean H323Codec::Open(H323Connection & /*connection*/)
{
  return TRUE;
}

unsigned H323Codec::GetFrameRate() const
{
  return mediaFormat.GetFrameTime();
}


void H323Codec::OnFlowControl(long PTRACE_PARAM(bitRateRestriction))
{
  PTRACE(3, "Codec\tOnFlowControl: " << bitRateRestriction);
}


void H323Codec::OnMiscellaneousCommand(const H245_MiscellaneousCommand_type & PTRACE_PARAM(type))
{
  PTRACE(3, "Codec\tOnMiscellaneousCommand: " << type.GetTagName());
}


void H323Codec::OnMiscellaneousIndication(const H245_MiscellaneousIndication_type & PTRACE_PARAM(type))
{
  PTRACE(3, "Codec\tOnMiscellaneousIndication: " << type.GetTagName());
}


PBoolean H323Codec::AttachChannel(PChannel * channel, PBoolean autoDelete)
{
  PWaitAndSignal mutex(rawChannelMutex);

  CloseRawDataChannel();

  rawDataChannel = channel;
  deleteChannel = autoDelete;

  if (channel == NULL){
	PTRACE(3, "Codec\tError attaching channel. channel is NULL");
    return FALSE;
  }

  return channel->IsOpen();
}

PChannel * H323Codec::SwapChannel(PChannel * newChannel, PBoolean autoDelete)
{
  PWaitAndSignal mutex(rawChannelMutex);

  PChannel * oldChannel = rawDataChannel;

  rawDataChannel = newChannel;
  deleteChannel = autoDelete;

  return oldChannel;
}


PBoolean H323Codec::CloseRawDataChannel()
{
  if (rawDataChannel == NULL)
    return FALSE;
  
  PBoolean closeOK = rawDataChannel->Close();
  
  if (deleteChannel) {
     delete rawDataChannel;
     rawDataChannel = NULL;
  }
  
  return closeOK;
}  


PBoolean H323Codec::IsRawDataChannelNative() const
{
  return FALSE;
}


PBoolean H323Codec::ReadRaw(void * data, PINDEX size, PINDEX & length)
{
  if (rawDataChannel == NULL) {
    PTRACE(1, "Codec\tNo audio channel for read");
    return FALSE;
  }

  if (!rawDataChannel->Read(data, size)) {
    PTRACE(1, "Codec\tAudio read failed: " << rawDataChannel->GetErrorText(PChannel::LastReadError));
    return FALSE;
  }

  length = rawDataChannel->GetLastReadCount();
  for (PINDEX i = 0; i < filters.GetSize(); i++) {
      length = filters[i].ProcessFilter(data, size, length);
  }

  return TRUE;
}

PBoolean H323Codec::WriteRaw(void * data, PINDEX length, void * mark)
{
    return WriteInternal(data, length, mark);
}

PBoolean H323Codec::WriteInternal(void * data, PINDEX length, void * mark)
{
  if (rawDataChannel == NULL) {
    PTRACE(1, "Codec\tNo audio channel for write");
    return FALSE;
  }

  for (PINDEX i = 0; i < filters.GetSize(); i++) {
      length = filters[i].ProcessFilter(data, length, length);
  }

#if PTLIB_VER < 290
  if (rawDataChannel->Write(data, length))
#else
  if (rawDataChannel->Write(data, length, mark))
#endif
    return TRUE;

  PTRACE(1, "Codec\tWrite failed: " << rawDataChannel->GetErrorText(PChannel::LastWriteError));
  return FALSE;
}


PBoolean H323Codec::AttachLogicalChannel(H323Channel *channel)
{
  logicalChannel = channel;
  rtpInformation.m_sessionID=logicalChannel->GetSessionID();

  return TRUE;
}


void H323Codec::AddFilter(const PNotifier & notifier)
{
  rawChannelMutex.Wait();
  filters.Append(new FilterData(*this,notifier));
  rawChannelMutex.Signal();
}

PBoolean H323Codec::SetRawDataHeld(PBoolean /*hold*/) 
{
	return FALSE;
}

PBoolean H323Codec::OnRxSenderReport(DWORD rtpTimeStamp, const PInt64 & realTimeStamp)
{
    rtpSync.m_rtpTimeStamp = rtpTimeStamp;
    rtpSync.m_realTimeStamp = realTimeStamp;
    return true;
}

PTime H323Codec::CalculateRTPSendTime(DWORD timeStamp, unsigned rate) const
{
    if (rtpSync.m_rtpTimeStamp == 0)
        return 0;

    DWORD timeDiff = (timeStamp - rtpSync.m_rtpTimeStamp)/rate;

    return rtpSync.m_realTimeStamp + timeDiff;

}

void H323Codec::CalculateRTPSendTime(DWORD timeStamp, unsigned rate, PInt64 & sendTime) const
{
    if (rtpSync.m_rtpTimeStamp == 0) {
        sendTime = 0;
        return;
    }

    DWORD timeDiff = (timeStamp - rtpSync.m_rtpTimeStamp)/rate;

    sendTime = rtpSync.m_realTimeStamp + timeDiff;
}

/////////////////////////////////////////////////////////////////////////////

#ifdef H323_VIDEO

H323VideoCodec::H323VideoCodec(const OpalMediaFormat & fmt, Direction dir)
  : H323Codec(fmt, dir),
    frameWidth(0), frameHeight(0), fillLevel(0), sarWidth(1), sarHeight(1),
    videoBitRateControlModes(None), bitRateHighLimit(0), oldLength(0), oldTime(0), newTime(0),
    targetFrameTimeMs(0), frameBytes(0), sumFrameTimeMs(0), sumAdjFrameTimeMs(0), sumFrameBytes(0),
    videoQMax(0), videoQMin(0), videoQuality(0), frameStartTime(0), grabInterval(0), frameNum(0), 
    packetNum(0), oldPacketNum(0), framesPerSec(0)
{

}


H323VideoCodec::~H323VideoCodec()
{
  Close();    //The close operation may delete the rawDataChannel.

  //mediaFormat.RemoveAllOptions();
}


PBoolean H323VideoCodec::Open(H323Connection & connection)
{
#ifdef H323_H239
  if (rtpInformation.m_sessionID != OpalMediaFormat::DefaultVideoSessionID)
    return connection.OpenExtendedVideoChannel(direction == Encoder, *this);
  else 
#endif
    return connection.OpenVideoChannel(direction == Encoder, *this);
}


void H323VideoCodec::OnMiscellaneousCommand(const H245_MiscellaneousCommand_type & type)
{
  switch (type.GetTag()) {
    case H245_MiscellaneousCommand_type::e_videoFreezePicture :
      OnFreezePicture();
      break;

    case H245_MiscellaneousCommand_type::e_videoFastUpdatePicture :
      OnFastUpdatePicture();
      break;

    case H245_MiscellaneousCommand_type::e_videoFastUpdateGOB :
    {
      const H245_MiscellaneousCommand_type_videoFastUpdateGOB & fuGOB = type;
      OnFastUpdateGOB(fuGOB.m_firstGOB, fuGOB.m_numberOfGOBs);
      break;
    }

    case H245_MiscellaneousCommand_type::e_videoFastUpdateMB :
    {
      const H245_MiscellaneousCommand_type_videoFastUpdateMB & fuMB = type;
      OnFastUpdateMB(fuMB.HasOptionalField(H245_MiscellaneousCommand_type_videoFastUpdateMB::e_firstGOB) ? (int)fuMB.m_firstGOB : -1,
                     fuMB.HasOptionalField(H245_MiscellaneousCommand_type_videoFastUpdateMB::e_firstMB)  ? (int)fuMB.m_firstMB  : -1,
                     fuMB.m_numberOfMBs);
      break;
    }

    case H245_MiscellaneousCommand_type::e_lostPartialPicture :
      OnLostPartialPicture();
      break;

    case H245_MiscellaneousCommand_type::e_lostPicture :
      OnLostPicture();
      break;

    case H245_MiscellaneousCommand_type::e_videoTemporalSpatialTradeOff :
    {
      const PASN_Integer & newQuality = type;
      OnVideoTemporalSpatialTradeOffCommand(newQuality);
      break;
    }
  }

  H323Codec::OnMiscellaneousCommand(type);
}


void H323VideoCodec::OnFreezePicture()
{
  PTRACE(3, "Codec\tOnFreezePicture()");
}


void H323VideoCodec::OnFastUpdatePicture()
{
  PTRACE(3, "Codec\tOnFastUpdatePicture()");
}


void H323VideoCodec::OnFastUpdateGOB(unsigned PTRACE_PARAM(firstGOB),
                                     unsigned PTRACE_PARAM(numberOfGOBs))
{
  PTRACE(3, "Codecs\tOnFastUpdateGOB(" << firstGOB << ',' << numberOfGOBs << ')');
}


void H323VideoCodec::OnFastUpdateMB(int PTRACE_PARAM(firstGOB),
                                    int PTRACE_PARAM(firstMB),
                                    unsigned PTRACE_PARAM(numberOfMBs))
{
  PTRACE(3, "Codecs\tOnFastUpdateMB(" << firstGOB << ',' << firstMB << ',' << numberOfMBs << ')');
}


void H323VideoCodec::OnLostPartialPicture()
{
  PTRACE(3, "Codec\tOnLostPartialPicture()");
}


void H323VideoCodec::OnLostPicture()
{
  PTRACE(3, "Codec\tOnLostPicture()");
}


void H323VideoCodec::OnMiscellaneousIndication(const H245_MiscellaneousIndication_type & type)
{
  switch (type.GetTag()) {
    case H245_MiscellaneousIndication_type::e_videoIndicateReadyToActivate :
      OnVideoIndicateReadyToActivate();
      break;

    case H245_MiscellaneousIndication_type::e_videoTemporalSpatialTradeOff :
    {
      const PASN_Integer & newQuality = type;
      OnVideoTemporalSpatialTradeOffIndication(newQuality);
      break;
    }

    case H245_MiscellaneousIndication_type::e_videoNotDecodedMBs :
    {
      const H245_MiscellaneousIndication_type_videoNotDecodedMBs & vndMB = type;
      OnVideoNotDecodedMBs(vndMB.m_firstMB, vndMB.m_numberOfMBs, vndMB.m_temporalReference);
      break;
    }
  }

  H323Codec::OnMiscellaneousIndication(type);
}


void H323VideoCodec::OnVideoIndicateReadyToActivate()
{
  PTRACE(3, "Codec\tOnVideoIndicateReadyToActivate()");
}


void H323VideoCodec::OnVideoTemporalSpatialTradeOffCommand(int PTRACE_PARAM(newQuality))
{
  PTRACE(3, "Codecs\tOnVideoTemporalSpatialTradeOffCommand(" << newQuality << ')');
}


void H323VideoCodec::OnVideoTemporalSpatialTradeOffIndication(int PTRACE_PARAM(newQuality))
{
  PTRACE(3, "Codecs\tOnVideoTemporalSpatialTradeOffIndication(" << newQuality << ')');
}


void H323VideoCodec::OnVideoNotDecodedMBs(unsigned PTRACE_PARAM(firstMB),
                                          unsigned PTRACE_PARAM(numberOfMBs),
                                          unsigned PTRACE_PARAM(temporalReference))
{
  PTRACE(3, "Codecs\tOnVideoNotDecodedMBs(" << firstMB << ',' << numberOfMBs << ',' << temporalReference << ')');
}


void H323VideoCodec::Close()
{
  PWaitAndSignal mutex1(videoHandlerActive);  

  CloseRawDataChannel();
}


PBoolean H323VideoCodec::SetMaxBitRate(unsigned bitRate)
{
  PTRACE(1,"Set bitRateHighLimit for video to " << bitRate << " bps");
        
  bitRateHighLimit = bitRate;

  if (0 == bitRateHighLimit) // disable bitrate control
    videoBitRateControlModes &= ~AdaptivePacketDelay;

  // Set the maximum bit rate for capability exchange
  GetWritableMediaFormat().SetBandwidth(bitRate);
  return TRUE;
}

PBoolean H323VideoCodec::SetTargetFrameTimeMs(unsigned ms)
{
  PTRACE(1,"Set targetFrameTimeMs for video to " << ms << " milliseconds");

  targetFrameTimeMs = ms;

  if (0 == targetFrameTimeMs)
    videoBitRateControlModes &= ~DynamicVideoQuality;
  return TRUE;
}

void H323VideoCodec::SetEmphasisSpeed(bool /*speed*/)
{

}

void H323VideoCodec::SetMaxPayloadSize(int /*maxSize*/) 
{ 

}

void H323VideoCodec::SetGeneralCodecOption(const char * /*opt*/,  int /*val*/)
{
}


void H323VideoCodec::SendMiscCommand(unsigned command)
{
  if (logicalChannel != NULL)
    logicalChannel->SendMiscCommand(command);
}

PBoolean H323VideoCodec::SetSupportedFormats(std::list<PVideoFrameInfo> & /*info*/)
{
    return false;
}

#endif // H323_VIDEO


/////////////////////////////////////////////////////////////////////////////

#ifdef H323_AUDIO_CODECS

H323AudioCodec::H323AudioCodec(const OpalMediaFormat & fmt, Direction dir)
  : H323Codec(fmt, dir)
{
  framesReceived = 0;
  samplesPerFrame = mediaFormat.GetFrameTime() * mediaFormat.GetTimeUnits();
  if (samplesPerFrame == 0)
    samplesPerFrame = 8; // Default for non-frame based codecs.

  // Start off in silent mode
  inTalkBurst = FALSE;

  IsRawDataHeld = FALSE;

  // Initialise the adaptive threshold variables.
  SetSilenceDetectionMode(AdaptiveSilenceDetection);
}


H323AudioCodec::~H323AudioCodec()
{
  Close();

  CloseRawDataChannel();

  //mediaFormat.RemoveAllOptions();
}


PBoolean H323AudioCodec::Open(H323Connection & connection)
{
  return connection.OpenAudioChannel(direction == Encoder, samplesPerFrame*2, *this);
}


void H323AudioCodec::Close()
{
  //PWaitAndSignal mutex(rawChannelMutex); - TODO: This causes a lockup. Is it needed? -SH

  if (rawDataChannel != NULL)
    rawDataChannel->Close();

}


unsigned H323AudioCodec::GetFrameRate() const
{
  return samplesPerFrame;
}

unsigned H323AudioCodec::GetFrameTime() const
{
    return mediaFormat.GetFrameTime();
}

H323AudioCodec::SilenceDetectionMode H323AudioCodec::GetSilenceDetectionMode(
                                PBoolean * isInTalkBurst, unsigned * currentThreshold) const
{
  if (isInTalkBurst != NULL)
    *isInTalkBurst = inTalkBurst;

  if (currentThreshold != NULL)
    *currentThreshold = ulaw2linear((BYTE)(levelThreshold ^ 0xff));

  return silenceDetectMode;
}


void H323AudioCodec::SetSilenceDetectionMode(SilenceDetectionMode mode,
                                             unsigned threshold,
                                             unsigned signalDeadband,
                                             unsigned silenceDeadband,
                                             unsigned adaptivePeriod)
{
  silenceDetectMode = mode;

  // The silence deadband is the number of frames of low energy that have to
  // occur before the system stops sending data over the RTP link.
  signalDeadbandFrames = (signalDeadband+samplesPerFrame-1)/samplesPerFrame;
  silenceDeadbandFrames = (silenceDeadband+samplesPerFrame-1)/samplesPerFrame;

  // This is the period over which the adaptive algorithm operates
  adaptiveThresholdFrames = (adaptivePeriod+samplesPerFrame-1)/samplesPerFrame;

  if (mode != AdaptiveSilenceDetection) {
    levelThreshold = threshold;
    return;
  }

  // Initials threshold levels
  levelThreshold = 0;

  // Initialise the adaptive threshold variables.
  signalMinimum = UINT_MAX;
  silenceMaximum = 0;
  signalFramesReceived = 0;
  silenceFramesReceived = 0;

  // Restart in silent mode
  inTalkBurst = FALSE;
}


PBoolean H323AudioCodec::DetectSilence()
{
  // Can never have silence if NoSilenceDetection
  if (silenceDetectMode == NoSilenceDetection)
    return FALSE;

  // Can never have average signal level that high, this indicates that the
  // hardware cannot do silence detection.
  unsigned level = GetAverageSignalLevel();
  if (level == UINT_MAX)
    return FALSE;

  // Convert to a logarithmic scale - use uLaw which is complemented
  level = linear2ulaw(level) ^ 0xff;

  // Now if signal level above threshold we are "talking"
  PBoolean haveSignal = level > levelThreshold;

  // If no change ie still talking or still silent, resent frame counter
  if (inTalkBurst == haveSignal)
    framesReceived = 0;
  else {
    framesReceived++;
    // If have had enough consecutive frames talking/silent, swap modes.
    if (framesReceived >= (inTalkBurst ? silenceDeadbandFrames : signalDeadbandFrames)) {
      inTalkBurst = !inTalkBurst;
      PTRACE(4, "Codec\tSilence detection transition: "
             << (inTalkBurst ? "Talk" : "Silent")
             << " level=" << level << " threshold=" << levelThreshold);

      // If we had talk/silence transition restart adaptive threshold measurements
      signalMinimum = UINT_MAX;
      silenceMaximum = 0;
      signalFramesReceived = 0;
      silenceFramesReceived = 0;
    }
  }

  if (silenceDetectMode == FixedSilenceDetection)
    return !inTalkBurst;

  if (levelThreshold == 0) {
    if (level > 1) {
      // Bootstrap condition, use first frame level as silence level
      levelThreshold = level/2;
      PTRACE(4, "Codec\tSilence detection threshold initialised to: " << levelThreshold);
    }
    return TRUE; // inTalkBurst always FALSE here, so return silent
  }

  // Count the number of silent and signal frames and calculate min/max
  if (haveSignal) {
    if (level < signalMinimum)
      signalMinimum = level;
    signalFramesReceived++;
  }
  else {
    if (level > silenceMaximum)
      silenceMaximum = level;
    silenceFramesReceived++;
  }

  // See if we have had enough frames to look at proportions of silence/signal
  if ((signalFramesReceived + silenceFramesReceived) > adaptiveThresholdFrames) {

    /* Now we have had a period of time to look at some average values we can
       make some adjustments to the threshold. There are four cases:
     */
    if (signalFramesReceived >= adaptiveThresholdFrames) {
      /* If every frame was noisy, move threshold up. Don't want to move too
         fast so only go a quarter of the way to minimum signal value over the
         period. This avoids oscillations, and time will continue to make the
         level go up if there really is a lot of background noise.
       */
      int delta = (signalMinimum - levelThreshold)/4;
      if (delta != 0) {
        levelThreshold += delta;
        PTRACE(4, "Codec\tSilence detection threshold increased to: " << levelThreshold);
      }
    }
    else if (silenceFramesReceived >= adaptiveThresholdFrames) {
      /* If every frame was silent, move threshold down. Again do not want to
         move too quickly, but we do want it to move faster down than up, so
         move to halfway to maximum value of the quiet period. As a rule the
         lower the threshold the better as it would improve response time to
         the start of a talk burst.
       */
      unsigned newThreshold = (levelThreshold + silenceMaximum)/2 + 1;
      if (levelThreshold != newThreshold) {
        levelThreshold = newThreshold;
        PTRACE(4, "Codec\tSilence detection threshold decreased to: " << levelThreshold);
      }
    }
    else if (signalFramesReceived > silenceFramesReceived) {
      /* We haven't got a definitive silent or signal period, but if we are
         constantly hovering at the threshold and have more signal than
         silence we should creep up a bit.
       */
      levelThreshold++;
      PTRACE(4, "Codec\tSilence detection threshold incremented to: " << levelThreshold
             << " signal=" << signalFramesReceived << ' ' << signalMinimum
             << " silence=" << silenceFramesReceived << ' ' << silenceMaximum);
    }

    signalMinimum = UINT_MAX;
    silenceMaximum = 0;
    signalFramesReceived = 0;
    silenceFramesReceived = 0;
  }

  return !inTalkBurst;
}


unsigned H323AudioCodec::GetAverageSignalLevel()
{
  return UINT_MAX;
}

PBoolean H323AudioCodec::SetRawDataHeld(PBoolean hold) { 
	
  PTimedMutex m;
	m.Wait(50);    // wait for 50ms to avoid current locks
	IsRawDataHeld = hold; 
	m.Wait(50); 	// wait for 50ms to avoid any further locks
	return TRUE;
} 

/////////////////////////////////////////////////////////////////////////////

H323FramedAudioCodec::H323FramedAudioCodec(const OpalMediaFormat & fmt, Direction dir)
  : H323AudioCodec(fmt, dir), 
#ifdef H323_AEC
    aec(NULL),
#endif
    sampleBuffer(samplesPerFrame), bytesPerFrame(mediaFormat.GetFrameSize()), 
    readBytes(samplesPerFrame*2), writeBytes(samplesPerFrame*2), cntBytes(0)
{

}


PBoolean H323FramedAudioCodec::Read(BYTE * buffer, unsigned & length, RTP_DataFrame &)
{
  PWaitAndSignal mutex(rawChannelMutex);

  if (direction != Encoder) {
    PTRACE(1, "Codec\tAttempt to decode from encoder");
    return FALSE;
  }

  if (IsRawDataHeld) {	 // If connection is onHold
    PThread::Sleep(5);  // Sleep to avoid CPU overload. <--Should be a better method but it works :)
    length = 0;
    return TRUE;
  }

#if 0 //PTLIB_VER >= 2110
    bool lastPacket = true;
    if (rawDataChannel->SourceEncoded(lastPacket,length))
        return rawDataChannel->Read(buffer, length);
#endif

  if (!ReadRaw(sampleBuffer.GetPointer(samplesPerFrame), readBytes, cntBytes))
    return FALSE;

#ifdef H323_AEC
    if (aec != NULL) {
       PTRACE(6,"AEC\tSend " << readBytes);
       aec->Send((BYTE*)sampleBuffer.GetPointer(samplesPerFrame),(unsigned &)readBytes);
    }
#endif

  if (IsRawDataHeld) {
    length = 0;
    return TRUE;
  }

  if (cntBytes != readBytes) {
    PTRACE(1, "Codec\tRead truncated frame of raw data. Wanted " << readBytes << " and got "<< cntBytes);
    return FALSE;
  }
  cntBytes = 0;

  if (DetectSilence()) {
    length = 0;
    return TRUE;
  }

  // Default length is the frame size
  length = bytesPerFrame;
  return EncodeFrame(buffer, length);
}

WORD lastSequence=0;
PBoolean H323FramedAudioCodec::Write(const BYTE * buffer,
                                 unsigned length,
                                 const RTP_DataFrame & rtpFrame,
                                 unsigned & written
                                 )
{
  PWaitAndSignal mutex(rawChannelMutex);

  if (direction != Decoder) {
    PTRACE(1, "Codec\tAttempt to encode from decoder");
    return FALSE;
  }

  // If length is zero then it indicates silence, do nothing.
  written = 0;

  // Prepare AVSync Information
  rtpInformation.m_frameLost = (lastSequence > 0) ? rtpFrame.GetSequenceNumber() -lastSequence-1 : 0; 
        lastSequence = rtpFrame.GetSequenceNumber();
        rtpInformation.m_recvTime = PTimer::Tick().GetMilliSeconds();
        rtpInformation.m_timeStamp = rtpFrame.GetTimestamp();
        rtpInformation.m_clockRate = GetFrameRate();
        CalculateRTPSendTime(rtpInformation.m_timeStamp, rtpInformation.m_clockRate, rtpInformation.m_sendTime);
        rtpInformation.m_frame = &rtpFrame;


#if 0 //PTLIB_VER >= 290
  if (rawDataChannel->DisableDecode()) {
      if (WriteRaw(rtpFrame.GetPayloadPtr(), rtpFrame.GetPayloadSize(), &rtpInformation))  {
         written = length; // pretend we wrote the data, to avoid error message
         return TRUE;
      } else
         return FALSE;
  }
#endif

  if (length != 0) {
    if (length > bytesPerFrame)
      length = bytesPerFrame;
    written = bytesPerFrame;

    // Decode the data
    if (!DecodeFrame(buffer, length, written, writeBytes)) {
      written = length;
      length = 0;
    }
  }

  // was memset(sampleBuffer.GetPointer(samplesPerFrame), 0, );
  if (length == 0)
    DecodeSilenceFrame(sampleBuffer.GetPointer(writeBytes), writeBytes);

  // Write as 16bit PCM to sound channel
  if (IsRawDataHeld) {		// If Connection om Hold 
    PThread::Sleep(5);	// Sleep to avoid CPU Overload <--- Must be a better way but need it to work.
    return TRUE;
  } else {
#ifdef H323_AEC
      if (aec != NULL) {
         PTRACE(6,"AEC\tReceive " << writeBytes);
         aec->Receive((BYTE *)sampleBuffer.GetPointer(), writeBytes);
      }
#endif
      if (!WriteRaw(sampleBuffer.GetPointer(), writeBytes, &rtpInformation)) 
          return FALSE;
  }
      return TRUE;


}


unsigned H323FramedAudioCodec::GetAverageSignalLevel()
{
  // Calculate the average signal level of this frame
  int sum = 0;

  const short * pcm = sampleBuffer;
  const short * end = pcm + samplesPerFrame;
  while (pcm != end) {
    if (*pcm < 0)
      sum -= *pcm++;
    else
      sum += *pcm++;
  }

  return sum/samplesPerFrame;
}


PBoolean H323FramedAudioCodec::DecodeFrame(const BYTE * buffer,
                                       unsigned length,
                                       unsigned & written,
                                       unsigned & /*decodedBytes*/)
{
  return DecodeFrame(buffer, length, written);
}


PBoolean H323FramedAudioCodec::DecodeFrame(const BYTE * /*buffer*/,
                                       unsigned /*length*/,
                                       unsigned & /*written*/)
{
  PAssertAlways(PUnimplementedFunction);
  return FALSE;
}

#ifdef H323_AEC 
void H323FramedAudioCodec::AttachAEC(H323Aec * _aec)
{
  aec = _aec;
}
#endif

/////////////////////////////////////////////////////////////////////////////

H323StreamedAudioCodec::H323StreamedAudioCodec(const OpalMediaFormat & fmt,
                                               Direction dir,
                                               unsigned samples,
                                               unsigned bits)
  : H323FramedAudioCodec(fmt, dir)
{
  samplesPerFrame = samples;
  bytesPerFrame = (samples*bits+7)/8;
  bitsPerSample = bits;
}


PBoolean H323StreamedAudioCodec::EncodeFrame(BYTE * buffer, unsigned &)
{
  PINDEX i;
  unsigned short position = 0;
  BYTE encoded;
  switch (bitsPerSample) {
    case 8 :
      for (i = 0; i < (PINDEX)samplesPerFrame; i++)
        *buffer++ = (BYTE)Encode(sampleBuffer[i]);
      break;
    case 5 : // g.726-40 payload encoding....
      for (i = 0; i < (PINDEX)samplesPerFrame;i++)
      {
        // based on a 40 bits encoding, we have 8 words of 5 bits each
        encoded = (BYTE)Encode(sampleBuffer[i]);
        switch(position)
        {
          case 0: // 0 bits overflow
            *buffer = encoded;
            position++;
            break;
          case 1: // 2 bits overflow
            *buffer++ |= (encoded << 5);
            *buffer = (BYTE)(encoded >> 3);
            position++;
            break;
          case 2: 
            *buffer |= (encoded << 2);
            position++;
            break;
          case 3: // one bit left for word 4
            *buffer++ |= (encoded << 7);
            *buffer = (BYTE)(encoded >> 1);
            position++;
            break;
          case 4:
            *buffer++ |= (encoded << 4);
            *buffer = (BYTE)(encoded >> 4);
            position++;
            break;
          case 5:
            *buffer |= (encoded << 1);
            position++;
            break;
          case 6: //two bits left for the new encoded word
            *buffer++ |= (encoded << 6);
            *buffer =  (BYTE)(encoded >> 2);
            position++;
            break;
          case 7: // now five bits left for the last word
            *buffer++ |= (encoded << 3);
            position = 0;
            break;
        }
      }
      break;

    case 4 :
      for (i = 0; i < (PINDEX)samplesPerFrame; i++) {
        if ((i&1) == 0)
          *buffer = (BYTE)Encode(sampleBuffer[i]);
        else
          *buffer++ |= (BYTE)(Encode(sampleBuffer[i]) << 4);
      }
      break;

    case 3 :
      for (i = 0;i < (PINDEX)samplesPerFrame;i++)
      {
        encoded = (BYTE)Encode(sampleBuffer[i]);
        switch(position)
        {
          case 0: // 0 bits overflow
            *buffer = encoded;
            position++;
            break;
          case 1: // 2 bits overflow
            *buffer |= (encoded << 3);
            position++;
            break;
          case 2: 
            *buffer++ |= (encoded << 6);
            *buffer = (BYTE)(encoded >> 2);
            position++;
            break;
          case 3: // one bit left for word 4
            *buffer |= (encoded << 1);
            position++;
            break;
          case 4:
            *buffer |= (encoded << 4);
            position++;
            break;
          case 5:
            *buffer++ |= (encoded << 7);
            *buffer = (BYTE)(encoded >> 1);
            position++;
            break;
          case 6: //two bits left for the new encoded word
            *buffer |= (encoded << 2);
            position++;
            break;
          case 7: // now five bits left for the last word
            *buffer++ |= (encoded << 5);
            position = 0;
            break;
        }
      }
      break;

    case 2:
      for (i = 0; i < (PINDEX)samplesPerFrame; i++) 
      {
        switch(position)
        {
          case 0:
            *buffer = (BYTE)Encode(sampleBuffer[i]);
            position++;
            break;
          case 1:
            *buffer |= (BYTE)(Encode(sampleBuffer[i]) << 2);
            position++;
            break;
          case 2:
            *buffer |= (BYTE)(Encode(sampleBuffer[i]) << 4);
            position++;
            break;
          case 3:
            *buffer++ |= (BYTE)(Encode(sampleBuffer[i]) << 6);
            position = 0;
            break;
        }
      }
      break;

    default :
      PAssertAlways("Unsupported bit size");
      return FALSE;
  }
  
  return TRUE;
}


PBoolean H323StreamedAudioCodec::DecodeFrame(const BYTE * buffer,
                                         unsigned length,
                                         unsigned & written,
                                         unsigned & decodedBytes)
{
  unsigned i;
  
  short * sampleBufferPtr = sampleBuffer.GetPointer(samplesPerFrame);
  short * out = sampleBufferPtr;
  unsigned short position = 0;
  unsigned remaining = 0;
  
  switch (bitsPerSample) {
    case 8 :
      for (i = 0; i < length; i++)
        *out++ = Decode(*buffer++);
      break;

    // those case are for ADPCM G.726
    case 5 :
      for (i = 0; i < length; i++) {
        switch(position)
        {
          case 0:
            *out++ = Decode(*buffer & 31);
            remaining = *buffer >> 5; // get the three remaining bytes for the next word
            buffer++;
            position++;
            break;
          case 1: // we can decode more than one word in second buffer
            *out++ = Decode (((*buffer&3) << 3) | remaining);
            *out++ = Decode( (*buffer >> 2) & 31);
            remaining = *buffer >> 7;
            buffer++;
            position++;
            break;
          case 2:
            *out++ = Decode( remaining | ((*buffer&15) << 1));
            remaining = *buffer >> 4;
            buffer++;
            position++;
            break;
          case 3:
            *out++ = Decode( remaining | ((*buffer&1) << 4));
            *out++ = Decode( (*buffer >> 1) & 31);
            remaining = *buffer >> 6;
            buffer++;
            position++;
            break;
          case 4 :
            *out++ = Decode( remaining | ((*buffer&7) << 2));
            *out++ = Decode(*buffer >> 3);
            buffer++;
            position = 0;
            break;
        }
      }
      break;

    case 4 :
      for (i = 0; i < length; i++) {
        *out++ = Decode(*buffer & 15);
        *out++ = Decode(*buffer >> 4);
        buffer++;
      }
      break;

    case 3:
      for (i = 0; i < length; i++) {
        switch(position)
        {
        case 0:
          *out++ = Decode(*buffer & 7);
          *out++ = Decode((*buffer>>3)&7);
          remaining = *buffer >> 6;
          buffer++;
          position++;
          break;
        case 1:
          *out++ = Decode(remaining | ((*buffer&1) << 2));
          *out++ = Decode((*buffer >> 1) & 7);
          *out++ = Decode((*buffer >> 4)&7);
          remaining = *buffer >> 7;
          buffer++;
          position++;
          break;
        case 2:
          *out++ = Decode(remaining | ((*buffer&3) << 1));
          *out++ = Decode((*buffer >> 2) & 7);
          *out++ = Decode((*buffer >> 5) & 7);
          buffer++;
          position = 0;
          break;
        }
      }
      break;

    case 2:
      for (i = 0; i < length; i++) 
      {
        *out++ = Decode(*buffer & 3);
        *out++ = Decode((*buffer >> 2) & 3);
        *out++ = Decode((*buffer >> 4) & 3);
        *out++ = Decode((*buffer >> 6) & 3);
        buffer++;
      
      }
      break;

    default :
      PAssertAlways("Unsupported bit size");
      return FALSE;
  }

  written = length;
  decodedBytes = (out - sampleBufferPtr)*2;
  
  return TRUE;
}


/////////////////////////////////////////////////////////////////////////////

H323_ALawCodec::H323_ALawCodec(Direction dir,
                               PBoolean at56kbps,
                               unsigned frameSize)
  : H323StreamedAudioCodec(OpalG711ALaw, dir, frameSize, 8)
{
  sevenBit = at56kbps;

  PTRACE(3, "Codec\tG711 ALaw " << (dir == Encoder ? "en" : "de")
         << "coder created for at "
         << (sevenBit ? "56k" : "64k") << ", " << frameSize << " samples");
}



int H323_ALawCodec::EncodeSample(short sample)
{
  return linear2alaw(sample);
}


short H323_ALawCodec::DecodeSample(int sample)
{
  return (short)alaw2linear((unsigned char)sample);
}


/////////////////////////////////////////////////////////////////////////////

H323_muLawCodec::H323_muLawCodec(Direction dir,
                                 PBoolean at56kbps,
                                 unsigned frameSize)
  : H323StreamedAudioCodec(OpalG711uLaw, dir, frameSize, 8)
{
  sevenBit = at56kbps;

  PTRACE(3, "Codec\tG711 uLaw " << (dir == Encoder ? "en" : "de")
         << "coder created for at "
         << (sevenBit ? "56k" : "64k") << ", frame of " << frameSize << " samples");
}


int H323_muLawCodec::EncodeSample(short sample)
{
  return linear2ulaw(sample);
}


short H323_muLawCodec::DecodeSample(int sample)
{
  return (short)ulaw2linear((unsigned char)sample);
}


/////////////////////////////////////////////////////////////////////////////

#endif // NO_H323_AUDIO_CODECS
