/*
 * channels.cxx
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

#include <ptlib.h>

#ifdef __GNUC__
#pragma implementation "channels.h"
#endif

#include "channels.h"

#include "h323pdu.h"
#include "h323ep.h"
#include "h323rtp.h"
#include <ptclib/random.h>
#include <ptclib/delaychan.h>


#define MAX_PAYLOAD_TYPE_MISMATCHES 8
#define RTP_TRACE_DISPLAY_RATE 16000 // 2 seconds


class H323LogicalChannelThread : public PThread
{
    PCLASSINFO(H323LogicalChannelThread, PThread)
  public:
    H323LogicalChannelThread(H323EndPoint & endpoint, H323Channel & channel, PBoolean rx);
    void Main();
  private:
    H323Channel & channel;
    PBoolean receiver;
};


#define new PNEW


#if PTRACING

ostream & operator<<(ostream & out, H323Channel::Directions dir)
{
  static const char * const DirNames[H323Channel::NumDirections] = {
    "IsBidirectional", "IsTransmitter", "IsReceiver"
  };

  if (dir < H323Channel::NumDirections && DirNames[dir] != NULL)
    out << DirNames[dir];
  else
    out << "Direction<" << (unsigned)dir << '>';

  return out;
}

#endif


/////////////////////////////////////////////////////////////////////////////

H323LogicalChannelThread::H323LogicalChannelThread(H323EndPoint & endpoint,
                                                   H323Channel & c,
                                                   PBoolean rx)
  : PThread(endpoint.GetChannelThreadStackSize(),
            NoAutoDeleteThread,
            endpoint.GetChannelThreadPriority(),
            rx ? "LogChanRx:%0x" : "LogChanTx:%0x"),
    channel(c)
{
  PTRACE(4, "LogChan\tStarting logical channel thread " << this);
  receiver = rx;
  Resume();
}


void H323LogicalChannelThread::Main()
{
  PTRACE(4, "LogChan\tStarted logical channel thread " << this);
  if (receiver)
    channel.Receive();
  else
    channel.Transmit();

#ifdef _WIN32_WCE
    Sleep(0); // Relinquish control to other thread
#endif
}


/////////////////////////////////////////////////////////////////////////////

H323ChannelNumber::H323ChannelNumber(unsigned num, PBoolean fromRem)
{
  PAssert(num < 0x10000, PInvalidParameter);
  number = num;
  fromRemote = fromRem;
}


PObject * H323ChannelNumber::Clone() const
{
  return new H323ChannelNumber(number, fromRemote);
}


PINDEX H323ChannelNumber::HashFunction() const
{
  PINDEX hash = (number%17) << 1;
  if (fromRemote)
    hash++;
  return hash;
}


void H323ChannelNumber::PrintOn(ostream & strm) const
{
  strm << (fromRemote ? 'R' : 'T') << '-' << number;
}


PObject::Comparison H323ChannelNumber::Compare(const PObject & obj) const
{
#ifndef PASN_LEANANDMEAN
  PAssert(PIsDescendant(this, H323ChannelNumber), PInvalidCast);
#endif
  const H323ChannelNumber & other = (const H323ChannelNumber &)obj;
  if (number < other.number)
    return LessThan;
  if (number > other.number)
    return GreaterThan;
  if (fromRemote && !other.fromRemote)
    return LessThan;
  if (!fromRemote && other.fromRemote)
    return GreaterThan;
  return EqualTo;
}


H323ChannelNumber & H323ChannelNumber::operator++(int)
{
  number++;
  return *this;
}


/////////////////////////////////////////////////////////////////////////////

H323Channel::H323Channel(H323Connection & conn, const H323Capability & cap)
  : endpoint(conn.GetEndPoint()),
    connection(conn)
{
  capability = (H323Capability *)cap.Clone();
  codec = NULL;
  bandwidthUsed = 0;
  receiveThread = NULL;
  transmitThread = NULL;
  terminating = FALSE;
  opened = FALSE;
  paused = FALSE;

#ifdef H323_H46026
  mediaTunneled = conn.H46026IsMediaTunneled();
#else
  mediaTunneled = FALSE;
#endif
}


H323Channel::~H323Channel()
{
  connection.UseBandwidth(bandwidthUsed, TRUE);

  delete codec;
  delete capability;
}


void H323Channel::PrintOn(ostream & strm) const
{
  strm << number;
}


unsigned H323Channel::GetSessionID() const
{
  return 0;
}

void H323Channel::SetSessionID(unsigned /*id*/) 
{
}

void H323Channel::CleanUpOnTermination()
{
  if (!opened || terminating)
    return;

  PTRACE(3, "LogChan\tCleaning up " << number);

  terminating = TRUE;

  // If we have a codec, then close it, this allows the transmitThread to be
  // broken out of any I/O block on reading the codec.
  if (codec != NULL)
    codec->Close();

  // If we have a receiver thread, wait for it to die.
  if (receiveThread != NULL) {
    PTRACE(4, "LogChan\tAwaiting termination of " << receiveThread << ' ' << receiveThread->GetThreadName());
    receiveThread->WaitForTermination(5000);
   // PAssert(receiveThread->WaitForTermination(10000), "Receive media thread did not terminate");
    delete receiveThread;
    receiveThread = NULL;
  }

  // If we have a transmitter thread, wait for it to die.
  if (transmitThread != NULL) {
    PTRACE(4, "LogChan\tAwaiting termination of " << transmitThread << ' ' << transmitThread->GetThreadName());
    transmitThread->WaitForTermination(5000);
   // PAssert(transmitThread->WaitForTermination(10000), "Transmit media thread did not terminate");
    delete transmitThread;
    transmitThread = NULL;
  }

  // Signal to the connection that this channel is on the way out
  connection.OnClosedLogicalChannel(*this);

  PTRACE(3, "LogChan\tCleaned up " << number);
}


PBoolean H323Channel::IsRunning() const
{
  if (receiveThread  != NULL && !receiveThread ->IsTerminated())
    return TRUE;

  if (transmitThread != NULL && !transmitThread->IsTerminated())
    return TRUE;

  return FALSE;
}


PBoolean H323Channel::OnReceivedPDU(const H245_OpenLogicalChannel & /*pdu*/,
                                unsigned & /*errorCode*/)
{
  return TRUE;
}


PBoolean H323Channel::OnReceivedAckPDU(const H245_OpenLogicalChannelAck & /*pdu*/)
{
  return TRUE;
}


void H323Channel::OnSendOpenAck(const H245_OpenLogicalChannel & /*pdu*/,
                                H245_OpenLogicalChannelAck & /* pdu*/) const
{
}


void H323Channel::OnFlowControl(long bitRateRestriction)
{
  if (GetCodec() != NULL)
    codec->OnFlowControl(bitRateRestriction);
  else
    PTRACE(3, "LogChan\tOnFlowControl: " << bitRateRestriction);
}


void H323Channel::OnMiscellaneousCommand(const H245_MiscellaneousCommand_type & type)
{
  if (GetCodec() != NULL)
    codec->OnMiscellaneousCommand(type);
  else
    PTRACE(3, "LogChan\tOnMiscellaneousCommand: chan=" << number
           << ", type=" << type.GetTagName());
}


void H323Channel::OnMiscellaneousIndication(const H245_MiscellaneousIndication_type & type)
{
  if (GetCodec() != NULL)
    codec->OnMiscellaneousIndication(type);
  else
    PTRACE(3, "LogChan\tOnMiscellaneousIndication: chan=" << number
           << ", type=" << type.GetTagName());
}


void H323Channel::OnJitterIndication(DWORD PTRACE_PARAM(jitter),
                                     int   PTRACE_PARAM(skippedFrameCount),
                                     int   PTRACE_PARAM(additionalBuffer))
{
  PTRACE(3, "LogChan\tOnJitterIndication:"
            " jitter=" << jitter <<
            " skippedFrameCount=" << skippedFrameCount <<
            " additionalBuffer=" << additionalBuffer);
}


PBoolean H323Channel::SetInitialBandwidth()
{
  if (GetCodec() == NULL)
    return TRUE;

#ifdef H323_VIDEO
  if (GetSessionID() == OpalMediaFormat::DefaultVideoSessionID) { 
     if (GetDirection() == H323Channel::IsTransmitter)
        connection.OnSetInitialBandwidth((H323VideoCodec *)codec);
  }
#endif
  return SetBandwidthUsed(codec->GetMediaFormat().GetBandwidth()/100);
}


PBoolean H323Channel::SetBandwidthUsed(unsigned bandwidth)
{
  PTRACE(3, "LogChan\tBandwidth requested/used = "
         << bandwidth/10 << '.' << bandwidth%10 << '/'
         << bandwidthUsed/10 << '.' << bandwidthUsed%10
         << " kb/s");
  connection.UseBandwidth(bandwidthUsed, TRUE);
  bandwidthUsed = 0;

  if (!connection.UseBandwidth(bandwidth, FALSE))
    return FALSE;

  bandwidthUsed = bandwidth;
  return TRUE;
}


PBoolean H323Channel::Open()
{
  if (opened)
    return TRUE;

  // Give the connection (or endpoint) a chance to do something with
  // the opening of the codec. Default sets up various filters.
  if (!connection.OnStartLogicalChannel(*this)) {
    PTRACE(1, "LogChan\tOnStartLogicalChannel failed");
    return FALSE;
  }
  
  opened = TRUE;
  return TRUE;
}


H323Codec * H323Channel::GetCodec() const
{
  if (codec == NULL) {
    ((H323Channel*)this)->codec = capability->CreateCodec(
                  GetDirection() == IsReceiver ? H323Codec::Decoder : H323Codec::Encoder);
#ifdef H323_AUDIO_CODECS
    if (codec && PIsDescendant(codec, H323AudioCodec))
      ((H323AudioCodec*)codec)->SetSilenceDetectionMode(endpoint.GetSilenceDetectionMode()); 
#endif
  }

  return codec;
}


void H323Channel::SendMiscCommand(unsigned command)
{ 
  connection.SendLogicalChannelMiscCommand(*this, command); 
}

void H323Channel::SendFlowControlRequest(long restriction)
{ 
  connection.SendLogicalChannelFlowControl(*this, restriction); 
}


/////////////////////////////////////////////////////////////////////////////

H323UnidirectionalChannel::H323UnidirectionalChannel(H323Connection & conn,
                                                     const H323Capability & cap,
                                                     Directions direction)
  : H323Channel(conn, cap),
    receiver(direction == IsReceiver)
{
}


H323Channel::Directions H323UnidirectionalChannel::GetDirection() const
{
  return receiver ? IsReceiver : IsTransmitter;
}


PBoolean H323UnidirectionalChannel::Start()
{
  if (!Open())
    return FALSE;

  PThread * thread = new H323LogicalChannelThread(endpoint, *this, receiver);

  if (receiver)
    receiveThread  = thread;
  else
    transmitThread = thread;
  return TRUE;
}


/////////////////////////////////////////////////////////////////////////////

H323BidirectionalChannel::H323BidirectionalChannel(H323Connection & conn,
                                                   const H323Capability & cap)
  : H323Channel(conn, cap)
{
}


H323Channel::Directions H323BidirectionalChannel::GetDirection() const
{
  return IsBidirectional;
}


PBoolean H323BidirectionalChannel::Start()
{
  receiveThread  = new H323LogicalChannelThread(endpoint, *this, TRUE);
  transmitThread = new H323LogicalChannelThread(endpoint, *this, FALSE);
  return TRUE;
}


/////////////////////////////////////////////////////////////////////////////

H323_RealTimeChannel::H323_RealTimeChannel(H323Connection & connection,
                                           const H323Capability & capability,
                                           Directions direction)
  : H323UnidirectionalChannel(connection, capability, direction)

{
  rtpPayloadType = RTP_DataFrame::IllegalPayloadType;
}


PBoolean H323_RealTimeChannel::OnSendingPDU(H245_OpenLogicalChannel & open) const
{
  PTRACE(3, "H323RTP\tOnSendingPDU");

  open.m_forwardLogicalChannelNumber = (unsigned)number;

  if (open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
    open.m_reverseLogicalChannelParameters.IncludeOptionalField(
            H245_OpenLogicalChannel_reverseLogicalChannelParameters::e_multiplexParameters);
    // Set the communications information for unicast IPv4
    open.m_reverseLogicalChannelParameters.m_multiplexParameters.SetTag(
                H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters
                    ::e_h2250LogicalChannelParameters);

    connection.OnSendH245_OpenLogicalChannel(open, PFalse);
    return OnSendingPDU(open.m_reverseLogicalChannelParameters.m_dataType,
                        open.m_reverseLogicalChannelParameters.m_multiplexParameters);
  }
  else {
    // Set the communications information for unicast IPv4
    open.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(
                H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters
                    ::e_h2250LogicalChannelParameters);

    if (OnSendingAltPDU(open.m_genericInformation))
        open.IncludeOptionalField(H245_OpenLogicalChannel::e_genericInformation);

    connection.OnSendH245_OpenLogicalChannel(open, PTrue);
    return OnSendingPDU(open.m_forwardLogicalChannelParameters.m_dataType,
                        open.m_forwardLogicalChannelParameters.m_multiplexParameters);
  }
}


void H323_RealTimeChannel::OnSendOpenAck(const H245_OpenLogicalChannel & open,
                                         H245_OpenLogicalChannelAck & ack) const
{
  PTRACE(3, "H323RTP\tOnSendOpenAck");

  // set forwardMultiplexAckParameters option
  ack.IncludeOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters);

  // select H225 choice
  ack.m_forwardMultiplexAckParameters.SetTag(
      H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters);

  // get H225 parms
  H245_H2250LogicalChannelAckParameters & param = ack.m_forwardMultiplexAckParameters;

  // set session ID
  param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID);
  const H245_H2250LogicalChannelParameters & openparam =
                          open.m_forwardLogicalChannelParameters.m_multiplexParameters;
  unsigned sessionID = openparam.m_sessionID;
  param.m_sessionID = sessionID;


  OnSendOpenAck(param);

  PTRACE(2, "H323RTP\tSending open logical channel ACK: sessionID=" << sessionID);
}


PBoolean H323_RealTimeChannel::OnReceivedPDU(const H245_OpenLogicalChannel & open,
                                         unsigned & errorCode)
{
  if (receiver)
    number = H323ChannelNumber(open.m_forwardLogicalChannelNumber, TRUE);

  PTRACE(3, "H323RTP\tOnReceivedPDU for channel: " << number);

  unsigned prevTxFrames = capability->GetTxFramesInPacket();
  unsigned prevRxFrames = capability->GetRxFramesInPacket();
  PString  prevFormat   = capability->GetFormatName();

  PBoolean reverse = open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
  const H245_DataType & dataType = reverse ? open.m_reverseLogicalChannelParameters.m_dataType
                                           : open.m_forwardLogicalChannelParameters.m_dataType;

  if (!capability->OnReceivedPDU(dataType, receiver)) {
    PTRACE(1, "H323RTP\tData type not supported");
    errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeNotSupported;
    return FALSE;
  }

  // If we have already created a codec, and the new parameters indicate that
  // the capability limits have changed, then kill off the old codec it will
  // be wrongly constructed.
  if (codec != NULL &&
      (prevTxFrames != capability->GetTxFramesInPacket() ||
       prevRxFrames != capability->GetRxFramesInPacket() ||
       prevFormat   != capability->GetFormatName())) {
    delete codec;
    codec = NULL;
  }

  if (reverse) {
    if (open.m_reverseLogicalChannelParameters.m_multiplexParameters.GetTag() ==
             H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
      return OnReceivedPDU(dataType, open.m_reverseLogicalChannelParameters.m_multiplexParameters, errorCode);
  }
  else {
    if (open.m_forwardLogicalChannelParameters.m_multiplexParameters.GetTag() ==
             H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters::e_h2250LogicalChannelParameters)
      return OnReceivedPDU(dataType, open.m_forwardLogicalChannelParameters.m_multiplexParameters, errorCode);
  }

  PTRACE(1, "H323RTP\tOnly H.225.0 multiplex supported");
  errorCode = H245_OpenLogicalChannelReject_cause::e_unsuitableReverseParameters;
  return FALSE;
}


PBoolean H323_RealTimeChannel::OnReceivedAckPDU(const H245_OpenLogicalChannelAck & ack)
{
  PTRACE(3, "H323RTP\tOnReceiveOpenAck");

  if (!ack.HasOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters)) {
    PTRACE(1, "H323RTP\tNo forwardMultiplexAckParameters");
    return FALSE;
  }

  if (ack.m_forwardMultiplexAckParameters.GetTag() !=
            H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters) {
    PTRACE(1, "H323RTP\tOnly H.225.0 multiplex supported");
    return FALSE;
  }

  if (ack.HasOptionalField(H245_OpenLogicalChannelAck::e_genericInformation))
                       OnReceivedAckAltPDU(ack.m_genericInformation);

  return OnReceivedAckPDU(ack.m_forwardMultiplexAckParameters);
}


RTP_DataFrame::PayloadTypes H323_RealTimeChannel::GetRTPPayloadType() const
{
  RTP_DataFrame::PayloadTypes pt = rtpPayloadType;

  if (pt == RTP_DataFrame::IllegalPayloadType) {
    pt = capability->GetPayloadType();
    if (pt == RTP_DataFrame::IllegalPayloadType) {
      PAssertNULL(codec);
      pt = codec->GetMediaFormat().GetPayloadType();
    }
  }

  return pt;
}


PBoolean H323_RealTimeChannel::SetDynamicRTPPayloadType(int newType)
{
  PTRACE(1, "H323RTP\tSetting dynamic RTP payload type: " << newType);

  // This is "no change"
  if (newType == -1)
    return TRUE;

  // Check for illegal type
  if (newType < RTP_DataFrame::DynamicBase || newType > RTP_DataFrame::MaxPayloadType)
    return FALSE;

  // Check for overwriting "known" type
  if (rtpPayloadType < RTP_DataFrame::DynamicBase)
    return FALSE;

  rtpPayloadType = (RTP_DataFrame::PayloadTypes)newType;
  PTRACE(3, "H323RTP\tSetting dynamic payload type to " << rtpPayloadType);
  return TRUE;
}


/////////////////////////////////////////////////////////////////////////////

H323_RTPChannel::H323_RTPChannel(H323Connection & conn,
                                 const H323Capability & cap,
                                 Directions direction,
                                 RTP_Session & r)
  : H323_RealTimeChannel(conn, cap, direction),
    rtpSession(r),
    rtpCallbacks(*(H323_RTP_Session *)r.GetUserData()),
    rec_written(0), rec_ok(false)
{
  PTRACE(3, "H323RTP\t" << (receiver ? "Receiver" : "Transmitter")
         << " created using session " << GetSessionID());
}


H323_RTPChannel::~H323_RTPChannel()
{
  // Finished with the RTP session, this will delete the session if it is no
  // longer referenced by any logical channels.
  connection.ReleaseSession(GetSessionID());
}


void H323_RTPChannel::CleanUpOnTermination()
{
  if (terminating)
    return;

  PTRACE(3, "H323RTP\tCleaning up RTP " << number);

  // Break any I/O blocks and wait for the thread that uses this object to
  // terminate before we allow it to be deleted.
  if ((receiver ? receiveThread : transmitThread) != NULL)
    rtpSession.Close(receiver);

  H323Channel::CleanUpOnTermination();
}


unsigned H323_RTPChannel::GetSessionID() const
{
  return rtpSession.GetSessionID();
}


void H323_RTPChannel::SetSessionID(unsigned id)
{
  rtpSession.SetSessionID(id);
}


PBoolean H323_RTPChannel::Open()
{
  if (opened)
    return TRUE;

  if (GetCodec() == NULL) {
    PTRACE(1, "LogChan\t" << (GetDirection() == IsReceiver ? "Receive" : "Transmit")
           << " thread aborted (could not create codec)");
    return FALSE;
  }

  if (!codec->GetMediaFormat().IsValid()) {
    PTRACE(1, "LogChan\t" << (GetDirection() == IsReceiver ? "Receive" : "Transmit")
           << " thread aborted (invalid media format)");
    return FALSE;
  }

  codec->AttachLogicalChannel((H323Channel*)this);

  // Open the codec
  if (!codec->Open(connection)) {
    PTRACE(1, "LogChan\t" << (GetDirection() == IsReceiver ? "Receive" : "Transmit")
           << " thread aborted (open fail) for "<< *capability);
    return FALSE;
  }

  // Give the connection (or endpoint) a chance to do something with
  // the opening of the codec. Default sets up various filters.
  if (!connection.OnStartLogicalChannel(*this)) {
    PTRACE(1, "LogChan\t" << (GetDirection() == IsReceiver ? "Receive" : "Transmit")
           << " thread aborted (OnStartLogicalChannel fail)");
    return FALSE;
  }

  PTRACE(3, "LogChan\tOpened using capability " << *capability);

  opened = TRUE;

  return TRUE;
}


PBoolean H323_RTPChannel::OnSendingPDU(H245_H2250LogicalChannelParameters & param) const
{
  return rtpCallbacks.OnSendingPDU(*this, param);
}

PBoolean H323_RTPChannel::OnSendingAltPDU(H245_ArrayOf_GenericInformation & alternate) const
{
  return rtpCallbacks.OnSendingAltPDU(*this, alternate);
}

void H323_RTPChannel::OnSendOpenAck(H245_H2250LogicalChannelAckParameters & param) const
{
  rtpCallbacks.OnSendingAckPDU(*this, param);
}

PBoolean H323_RTPChannel::OnReceivedPDU(const H245_H2250LogicalChannelParameters & param,
                                    unsigned & errorCode)
{
  return rtpCallbacks.OnReceivedPDU(*this, param, errorCode);
}

PBoolean H323_RTPChannel::OnReceivedAckPDU(const H245_H2250LogicalChannelAckParameters & param)
{
  return rtpCallbacks.OnReceivedAckPDU(*this, param);
}

PBoolean H323_RTPChannel::OnReceivedAckAltPDU(const H245_ArrayOf_GenericInformation & alternate)
{ 
  return rtpCallbacks.OnReceivedAckAltPDU(*this, alternate);
}

PBoolean H323_RTPChannel::ReadFrame(DWORD & rtpTimestamp, RTP_DataFrame & frame)
{
  return rtpSession.ReadBufferedData(rtpTimestamp, frame);
}

PBoolean H323_RTPChannel::WriteFrame(RTP_DataFrame & frame)
{
  return rtpSession.PreWriteData(frame) && rtpSession.WriteData(frame);
}

#if PTRACING
class CodecReadAnalyser
{
  enum { MaxSamples = 1000 };
  public:
    CodecReadAnalyser() { count = 0; }
    void AddSample(DWORD timestamp)
      {
        if (count < MaxSamples) {
          tick[count] = PTimer::Tick();
          rtp[count] = timestamp;
          count++;
        }
      }
    friend ostream & operator<<(ostream & strm, const CodecReadAnalyser & analysis)
      {
        PTimeInterval minimum = PMaxTimeInterval;
        PTimeInterval maximum;
        for (PINDEX i = 1; i < analysis.count; i++) {
          PTimeInterval delta = analysis.tick[i] - analysis.tick[i-1];
          strm << setw(6) << analysis.rtp[i] << ' '
               << setw(6) << (analysis.tick[i] - analysis.tick[0]) << ' '
               << setw(6) << delta
               << '\n';
          if (delta > maximum)
            maximum = delta;
          if (delta < minimum)
            minimum = delta;
        }
        strm << "Maximum delta time: " << maximum << "\n"
                "Minimum delta time: " << minimum << '\n';
        return strm;
      }
  private:
    PTimeInterval tick[MaxSamples];
    DWORD rtp[MaxSamples];
    PINDEX count;
};
#endif


void H323_RTPChannel::Transmit()
{
  if (terminating) {
    PTRACE(3, "H323RTP\tTransmit thread terminated on start up");
    return;
  }

  const OpalMediaFormat & mediaFormat = codec->GetMediaFormat();

  // Get parameters from the codec on time and data sizes
  PBoolean isAudio = mediaFormat.NeedsJitterBuffer();
  unsigned framesInPacket = capability->GetTxFramesInPacket();
  if (framesInPacket > 8) framesInPacket = 1;  // TODO: Resolve issue with G.711 20ms
  unsigned maxSampleSize = mediaFormat.GetFrameSize();
  unsigned maxSampleTime = mediaFormat.GetFrameTime();

  unsigned maxFrameSize = isAudio ? maxSampleSize*maxSampleTime : 2000;
  RTP_DataFrame frame(framesInPacket*maxFrameSize);

  rtpPayloadType = GetRTPPayloadType();
  if (rtpPayloadType == RTP_DataFrame::IllegalPayloadType) {
     PTRACE(1, "H323RTP\tReceive " << mediaFormat << " thread ended (illegal payload type)");
     return;
  }
  frame.SetPayloadType(rtpPayloadType); 

  PTRACE(2, "H323RTP\tTransmit " << mediaFormat << " thread started:"
            " rate=" << codec->GetFrameRate() <<
            " time=" << (codec->GetFrameRate()/(mediaFormat.GetTimeUnits() > 0 ? mediaFormat.GetTimeUnits() : 1)) << "ms" <<
            " size=" << framesInPacket << '*' << maxFrameSize << '='
                    << (framesInPacket*maxFrameSize) );

  // This is real time so need to keep track of elapsed milliseconds
  PBoolean silent = TRUE;
  unsigned length;
  unsigned frameOffset = 0;
  unsigned frameCount = 0;
  DWORD rtpTimestamp = PRandom();
  DWORD nextTimestamp = 0;
#ifndef H323_FIXED_VIDEOCLOCK
  PInt64 lastFrameTime = 0;
#endif

#if PTRACING
  DWORD lastDisplayedTimestamp = 0;
  CodecReadAnalyser * codecReadAnalysis = NULL;
  if (PTrace::GetLevel() >= 5)
    codecReadAnalysis = new CodecReadAnalyser;
#endif


  /* Now keep getting encoded frames from the codec, it is expected that the
     Read() function will maintain the Real Time aspects of the transmission.
     That is for GSM codec say with a single frame, this function will take
     20 milliseconds to complete.
   */
  while (codec->Read(frame.GetPayloadPtr()+frameOffset, length, frame)) {
    // Calculate the timestamp and real time to take in processing
    if(isAudio)
    {
        rtpTimestamp += codec->GetFrameRate();
    } 
    else
    { 
       if(frame.GetMarker()) {
          // Video uses a 90khz clock. Note that framerate should really be a float.
#ifdef H323_FIXED_VIDEOCLOCK
           nextTimestamp = rtpTimestamp + 90000/codec->GetFrameRate(); 
#else
           PInt64 nowTime = PTimer::Tick().GetMilliSeconds();
           if (lastFrameTime == 0) {
              nextTimestamp = rtpTimestamp + 90000/codec->GetFrameRate(); 
           } else {
              nextTimestamp = rtpTimestamp + (DWORD)(90000.0*((float)(nowTime-lastFrameTime))/1000.0);
           }
           lastFrameTime = nowTime;
#endif
       }
    }

#if PTRACING
    if (rtpTimestamp - lastDisplayedTimestamp > RTP_TRACE_DISPLAY_RATE) {
      PTRACE(3, "H323RTP\tTransmitter sent timestamp " << rtpTimestamp);
      lastDisplayedTimestamp = rtpTimestamp;
    }

    if (codecReadAnalysis != NULL)
      codecReadAnalysis->AddSample(rtpTimestamp);
#endif

    if (paused)
      length = 0; // Act as though silent/no video

    // Handle marker bit for audio codec
    if (isAudio) {
      // If switching from silence to signal
      if (silent && length > 0) {
        silent = FALSE;
        frame.SetMarker(TRUE);  // Set flag for start of sound
        PTRACE(3, "H323RTP\tTransmit start of talk burst: " << rtpTimestamp);
      }
      // If switching from signal to silence
      else if (!silent && length == 0) {
        silent = TRUE;
        // If had some data waiting to go out
        if (frameOffset > 0)
          frameCount = framesInPacket;  // Force the RTP write
        PTRACE(3, "H323RTP\tTransmit  end  of talk burst: " << rtpTimestamp);
      }
    }

    // See if is silence or have some audio data to stuff in the RTP packet
    if (length == 0)
      frame.SetTimestamp(rtpTimestamp);
    else {
      silenceStartTick = PTimer::Tick();

      // If first read frame in packet, set timestamp for it
      if (frameOffset == 0)
        frame.SetTimestamp(rtpTimestamp);
      frameOffset += length;

      // Look for special cases
      if (rtpPayloadType == RTP_DataFrame::G729 && length == 2) {
        /* If we have a G729 sid frame (ie 2 bytes instead of 10) then we must
           not send any more frames in the RTP packet.
         */
        frameCount = framesInPacket;
      }
      else {
        /* Increment by number of frames that were read in one hit Note a
           codec that does variable length frames should never return more
           than one frame per Read() call or confusion will result.
         */
        frameCount += (length + maxFrameSize - 1)/maxFrameSize;
      }
    }

    PBoolean sendPacket = FALSE;

    // Have read number of frames for packet (or just went silent)
    if (frameCount >= framesInPacket) {
      // Set payload size to frame offset, now length of frame.
      frame.SetPayloadSize(frameOffset);
      frame.SetPayloadType(rtpPayloadType);

      frameOffset = 0;
      frameCount = 0;

      sendPacket = TRUE;
    }

    if (isAudio) {
        filterMutex.Wait();
        for (PINDEX i = 0; i < filters.GetSize(); i++)
          filters[i](frame, (INT)&sendPacket);
        filterMutex.Signal();
    }

    if (sendPacket || (silent && frame.GetPayloadSize() > 0)) {
      // Send the frame of coded data we have so far to RTP transport
      if (!WriteFrame(frame))
         break;

      // video frames produce many packets per frame especially at
      // higher resolutions and can easily overload the link if sent
      // without delay
      if (!isAudio) {
         PThread::Sleep(5);
         if (frame.GetMarker()) 
             rtpTimestamp = nextTimestamp;
      }

      // Reset flag for in talk burst
      if (isAudio)
        frame.SetMarker(FALSE); 

      frame.SetPayloadSize(maxFrameSize);
      frameOffset = 0;
      frameCount = 0;
    }

    if (terminating)
      break;
  }

#if PTRACING
  if (PTrace::GetLevel() >= 5) {
      PTRACE_IF(5, codecReadAnalysis != NULL, "Codec read timing:\n" << *codecReadAnalysis);
      delete codecReadAnalysis;
  }
#endif

  if (!terminating)
    connection.CloseLogicalChannelNumber(number);

  PTRACE(2, "H323RTP\tTransmit " << mediaFormat << " thread ended");
}

void H323_RTPChannel::SendUniChannelBackProbe()
{
  // When we are receiving media on a unidirectional Channel
  // we need to send media on the backchannel to ensure that
  // we open any necessary pinholes in NAT. 
   if (capability->GetCapabilityDirection() != H323Capability::e_Transmit)
        return;
   
    RTP_DataFrame frame;
    frame.SetPayloadSize(0);
    frame.SetPayloadType(rtpPayloadType);
    frame.SetTimestamp(PRandom());
    frame.SetMarker(false);

    WORD sequenceNumber = (WORD)PRandom::Number();
    int packetCount = 4;
  
    for (PINDEX i= 0; i < packetCount; ++i) {
      frame.SetSequenceNumber(++sequenceNumber);
      if (i == packetCount-1) frame.SetMarker(true);

      if (!WriteFrame(frame)) {
         PTRACE(2, "H323RTP\tERROR: BackChannel Probe Failed.");
         return;
      }
    }
    PTRACE(4, "H323RTP\tReceiving Unidirectional Channel: NAT Support Packets sent.");
}


void H323_RTPChannel::Receive()
{
  if (terminating) {
    PTRACE(3, "H323RTP\tReceive thread terminated on start up");
    return;
  }

  const OpalMediaFormat & mediaFormat = codec->GetMediaFormat();

  PTRACE(2, "H323RTP\tReceive " << mediaFormat << " thread started.");

  // if jitter buffer required, start the thread that is on the other end of it
  if (mediaFormat.NeedsJitterBuffer() && endpoint.UseJitterBuffer())
    rtpSession.SetJitterBufferSize(connection.GetMinAudioJitterDelay()*mediaFormat.GetTimeUnits(),
                                   connection.GetMaxAudioJitterDelay()*mediaFormat.GetTimeUnits(),
                                   endpoint.GetJitterThreadStackSize());

  rtpPayloadType = GetRTPPayloadType();
  if (rtpPayloadType == RTP_DataFrame::IllegalPayloadType) {
     PTRACE(1, "H323RTP\tTransmit " << mediaFormat << " thread ended (illegal payload type)");
     return;
  }

  // Keep time using th RTP timestamps.
  DWORD codecFrameRate = codec->GetFrameRate();
  DWORD rtpTimestamp = 0;
#if PTRACING
  DWORD lastDisplayedTimestamp = 0;
#endif

  // keep track of consecutive payload type mismatches
  int consecutiveMismatches = 0;
  int payloadSize = 0;
  PBoolean isAudio = codec->GetMediaFormat().NeedsJitterBuffer();
  PBoolean allowRtpPayloadChange = isAudio;

  RTP_Session::SenderReport avData;

  // UniDirectional Channel NAT support
  SendUniChannelBackProbe();

  RTP_DataFrame frame;
  while (ReadFrame(rtpTimestamp, frame)) {

    if (isAudio) {
      filterMutex.Wait();
      for (PINDEX i = 0; i < filters.GetSize(); i++)
        filters[i](frame, 0);
      filterMutex.Signal();
    }

    payloadSize = frame.GetPayloadSize();
    rtpTimestamp = frame.GetTimestamp();

    if (rtpSession.AVSyncData(avData))
        codec->OnRxSenderReport(avData.rtpTimestamp, avData.realTimestamp1970);

#if PTRACING
    if (rtpTimestamp - lastDisplayedTimestamp > RTP_TRACE_DISPLAY_RATE) {
      PTRACE(3, "H323RTP\tReceiver written timestamp " << rtpTimestamp);
      lastDisplayedTimestamp = rtpTimestamp;
    }
#endif

    rec_written=0;
    rec_ok=TRUE;
    if (payloadSize == 0) {
      rec_ok = codec->Write(NULL, 0, frame, rec_written);
      rtpTimestamp += codecFrameRate;
    } else {
      silenceStartTick = PTimer::Tick();

      if (frame.GetPayloadType() == rtpPayloadType) {
        PTRACE_IF(2, consecutiveMismatches > 0,
                  "H323RTP\tPayload type matched again " << rtpPayloadType);
        consecutiveMismatches = 0;
      }
      else {
        consecutiveMismatches++;
        if (allowRtpPayloadChange && consecutiveMismatches >= MAX_PAYLOAD_TYPE_MISMATCHES) {
          rtpPayloadType = frame.GetPayloadType();
          consecutiveMismatches = 0;
          PTRACE(1, "H323RTP\tResetting expected payload type to " << rtpPayloadType);
        }
        PTRACE_IF(2, consecutiveMismatches < MAX_PAYLOAD_TYPE_MISMATCHES, "H323RTP\tPayload type mismatch: expected "
              << rtpPayloadType << ", got " << frame.GetPayloadType()
              << ". Ignoring packet.");
      }

      if (consecutiveMismatches == 0) {
        const BYTE * ptr = frame.GetPayloadPtr();
        while (rec_ok && payloadSize > 0) {
          /* Now write data to the codec, it is expected that the Write()
             function will maintain the Real Time aspects of the system. That
             is for GSM codec, say with a single frame, this function will take
             20 milliseconds to complete. It is very important that this occurs
             for audio codecs or the jitter buffer will not operate correctly.
           */
          rec_ok = codec->Write(ptr, paused ? 0 : payloadSize, frame, rec_written);
          rtpTimestamp += codecFrameRate;
          payloadSize -= rec_written != 0 ? rec_written : payloadSize;
          ptr += rec_written;
        }
        PTRACE_IF(1, payloadSize < 0, "H323RTP\tPayload size too small, short " << -payloadSize << " bytes.");
      }
    }

    if (terminating)
      break;

    if (!rec_ok) {
      connection.CloseLogicalChannelNumber(number);
      break;
    }
  }

  PTRACE(2, "H323RTP\tReceive " << mediaFormat << " thread ended");
}


void H323_RTPChannel::AddFilter(const PNotifier & filterFunction)
{
  filterMutex.Wait();
  filters.Append(new PNotifier(filterFunction));
  filterMutex.Signal();
}


void H323_RTPChannel::RemoveFilter(const PNotifier & filterFunction)
{
  filterMutex.Wait();
  PINDEX idx = filters.GetValuesIndex(filterFunction);
  if (idx != P_MAX_INDEX)
    filters.RemoveAt(idx);
  filterMutex.Signal();
}


PTimeInterval H323_RTPChannel::GetSilenceDuration() const
{
  if (silenceStartTick == 0)
    return silenceStartTick;

  return PTimer::Tick() - silenceStartTick;
}


/////////////////////////////////////////////////////////////////////////////

H323_ExternalRTPChannel::H323_ExternalRTPChannel(H323Connection & connection,
                                                 const H323Capability & capability,
                                                 Directions direction,
                                                 unsigned id)
  : H323_RealTimeChannel(connection, capability, direction)
{
  sessionID = id;
  isRunning = FALSE;
}


H323_ExternalRTPChannel::H323_ExternalRTPChannel(H323Connection & connection,
                                                 const H323Capability & capability,
                                                 Directions direction,
                                                 unsigned id,
                                                 const H323TransportAddress & data,
                                                 const H323TransportAddress & control)
  : H323_RealTimeChannel(connection, capability, direction),
    externalMediaAddress(data),
    externalMediaControlAddress(control)
{
  sessionID = id;
  isRunning = FALSE;
}


H323_ExternalRTPChannel::H323_ExternalRTPChannel(H323Connection & connection,
                                                 const H323Capability & capability,
                                                 Directions direction,
                                                 unsigned id,
                                                 const PIPSocket::Address & ip,
                                                 WORD dataPort)
  : H323_RealTimeChannel(connection, capability, direction),
    externalMediaAddress(ip, dataPort),
    externalMediaControlAddress(ip, (WORD)(dataPort+1))
{
  sessionID = id;
  isRunning = FALSE;
}


unsigned H323_ExternalRTPChannel::GetSessionID() const
{
  return sessionID;
}


PBoolean H323_ExternalRTPChannel::Start()
{
  isRunning = TRUE;
  return Open();
}


PBoolean H323_ExternalRTPChannel::IsRunning() const
{
  return opened && isRunning;
}


void H323_ExternalRTPChannel::Receive()
{
  // Do nothing
}


void H323_ExternalRTPChannel::Transmit()
{
  // Do nothing
}


PBoolean H323_ExternalRTPChannel::OnSendingPDU(H245_H2250LogicalChannelParameters & param) const
{
  param.m_sessionID = sessionID;

  param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaGuaranteedDelivery);
  param.m_mediaGuaranteedDelivery = FALSE;

  param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_silenceSuppression);
  param.m_silenceSuppression = FALSE;

  // unicast must have mediaControlChannel unless tunneled
  if (!IsMediaTunneled()) {
      param.IncludeOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel);
      externalMediaControlAddress.SetPDU(param.m_mediaControlChannel);

      if (receiver) {
        // set mediaChannel
        param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
        externalMediaAddress.SetPDU(param.m_mediaChannel);
      }
  }

  return TRUE;
}


void H323_ExternalRTPChannel::OnSendOpenAck(H245_H2250LogicalChannelAckParameters & param) const
{
  if (!IsMediaTunneled()) {
      // set mediaControlChannel
      param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaControlChannel);
      externalMediaControlAddress.SetPDU(param.m_mediaControlChannel);

      // set mediaChannel
      param.IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
      externalMediaAddress.SetPDU(param.m_mediaChannel);
  }
}


PBoolean H323_ExternalRTPChannel::OnReceivedPDU(const H245_H2250LogicalChannelParameters & param,
                                          unsigned & errorCode)
{
  // Only support a single audio session
  if (param.m_sessionID != sessionID) {
    PTRACE(1, "LogChan\tOpen for invalid session: " << param.m_sessionID);
    errorCode = H245_OpenLogicalChannelReject_cause::e_invalidSessionID;
    return FALSE;
  }

  if (!IsMediaTunneled() && !param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaControlChannel)) {
    PTRACE(1, "LogChan\tNo mediaControlChannel specified");
    errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
    return FALSE;
  }

  remoteMediaControlAddress = param.m_mediaControlChannel;
  if (remoteMediaControlAddress.IsEmpty())
    return FALSE;

  if (param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaChannel)) {
    remoteMediaAddress = param.m_mediaChannel;
    if (remoteMediaAddress.IsEmpty())
      return FALSE;
  }

  return TRUE;
}


PBoolean H323_ExternalRTPChannel::OnReceivedAckPDU(const H245_H2250LogicalChannelAckParameters & param)
{
  if (param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID) && (param.m_sessionID != sessionID)) {
    PTRACE(1, "LogChan\twarning: Ack for invalid session: " << param.m_sessionID);
  }

  if (!IsMediaTunneled()) {
      if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaControlChannel)) {
        PTRACE(1, "LogChan\tNo mediaControlChannel specified");
        return FALSE;
      }
      remoteMediaControlAddress = param.m_mediaControlChannel;
      if (remoteMediaControlAddress.IsEmpty())
        return FALSE;

      if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel)) {
        PTRACE(1, "LogChan\tNo mediaChannel specified");
        return FALSE;
      }
      remoteMediaAddress = param.m_mediaChannel;
      if (remoteMediaAddress.IsEmpty())
        return FALSE;
  }

  return TRUE;
}


void H323_ExternalRTPChannel::SetExternalAddress(const H323TransportAddress & data,
                                                 const H323TransportAddress & control)
{
  externalMediaAddress = data;
  externalMediaControlAddress = control;

  if (data.IsEmpty() || control.IsEmpty()) {
    PIPSocket::Address ip;
    WORD port = 0;
    if (data.GetIpAndPort(ip, port))
      externalMediaControlAddress = H323TransportAddress(ip, (WORD)(port+1));
    else if (control.GetIpAndPort(ip, port))
      externalMediaAddress = H323TransportAddress(ip, (WORD)(port-1));
  }
}


PBoolean H323_ExternalRTPChannel::GetRemoteAddress(PIPSocket::Address & ip,
                                               WORD & dataPort) const
{
  if (!remoteMediaControlAddress) {
    if (remoteMediaControlAddress.GetIpAndPort(ip, dataPort)) {
      dataPort--;
      return TRUE;
    }
  }

  if (!remoteMediaAddress)
    return remoteMediaAddress.GetIpAndPort(ip, dataPort);

  return FALSE;
}


/////////////////////////////////////////////////////////////////////////////

H323DataChannel::H323DataChannel(H323Connection & conn,
                                 const H323Capability & cap,
                                 Directions dir,
                                 unsigned id)
  : H323UnidirectionalChannel(conn, cap, dir)
{
  sessionID = id;
  listener = NULL;
  autoDeleteListener = TRUE;
  transport = NULL;
  autoDeleteTransport = TRUE;
  separateReverseChannel = FALSE;
}


H323DataChannel::~H323DataChannel()
{
  if (autoDeleteListener)
    delete listener;
  if (autoDeleteTransport)
    delete transport;
}


void H323DataChannel::CleanUpOnTermination()
{
  if (terminating)
    return;

  PTRACE(3, "LogChan\tCleaning up data channel " << number);

  // Break any I/O blocks and wait for the thread that uses this object to
  // terminate before we allow it to be deleted.
  if (listener != NULL)
    listener->Close();
  if (transport != NULL)
    transport->Close();

  H323UnidirectionalChannel::CleanUpOnTermination();
}


unsigned H323DataChannel::GetSessionID() const
{
  return sessionID;
}


PBoolean H323DataChannel::OnSendingPDU(H245_OpenLogicalChannel & open) const
{
  PTRACE(3, "LogChan\tOnSendingPDU for channel: " << number);

  open.m_forwardLogicalChannelNumber = (unsigned)number;

  open.m_forwardLogicalChannelParameters.m_multiplexParameters.SetTag(
              H245_OpenLogicalChannel_forwardLogicalChannelParameters_multiplexParameters
                  ::e_h2250LogicalChannelParameters);
  H245_H2250LogicalChannelParameters & fparam = open.m_forwardLogicalChannelParameters.m_multiplexParameters;
  fparam.m_sessionID = GetSessionID();

  if (connection.OnSendingOLCGenericInformation(GetSessionID(),open.m_genericInformation,false))
        open.IncludeOptionalField(H245_OpenLogicalChannel::e_genericInformation);

  if (separateReverseChannel)
    return TRUE;

  open.IncludeOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters);
  open.m_reverseLogicalChannelParameters.IncludeOptionalField(
              H245_OpenLogicalChannel_reverseLogicalChannelParameters::e_multiplexParameters);
  open.m_reverseLogicalChannelParameters.m_multiplexParameters.SetTag(
              H245_OpenLogicalChannel_reverseLogicalChannelParameters_multiplexParameters
                  ::e_h2250LogicalChannelParameters);
  H245_H2250LogicalChannelParameters & rparam = open.m_reverseLogicalChannelParameters.m_multiplexParameters;
  rparam.m_sessionID = GetSessionID();

  return capability->OnSendingPDU(open.m_reverseLogicalChannelParameters.m_dataType);
}


void H323DataChannel::OnSendOpenAck(const H245_OpenLogicalChannel & /*open*/,
                                    H245_OpenLogicalChannelAck & ack) const
{
  if (listener == NULL && transport == NULL) {
    PTRACE(2, "LogChan\tOnSendOpenAck without a listener or transport");
    return;
  }

  PTRACE(3, "LogChan\tOnSendOpenAck for channel: " << number);

  H245_H2250LogicalChannelAckParameters * param;

  if (separateReverseChannel) {
    ack.IncludeOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters);
    ack.m_forwardMultiplexAckParameters.SetTag(
              H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters);
    param = (H245_H2250LogicalChannelAckParameters*)&ack.m_forwardMultiplexAckParameters.GetObject();
  }
  else {
    ack.IncludeOptionalField(H245_OpenLogicalChannelAck::e_reverseLogicalChannelParameters);
    ack.m_reverseLogicalChannelParameters.m_multiplexParameters.SetTag(
              H245_OpenLogicalChannelAck_reverseLogicalChannelParameters_multiplexParameters
                  ::e_h2250LogicalChannelParameters);
    param = (H245_H2250LogicalChannelAckParameters*)
                &ack.m_reverseLogicalChannelParameters.m_multiplexParameters.GetObject();
  }

  unsigned session = GetSessionID();
  if (session != 0) {
    param->IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_sessionID);
    param->m_sessionID = GetSessionID();

    if (connection.OnSendingOLCGenericInformation(session,ack.m_genericInformation,true))
       ack.IncludeOptionalField(H245_OpenLogicalChannel::e_genericInformation);
  }

  if (!IsMediaTunneled()) {
      param->IncludeOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel);
      if (listener != NULL)
        listener->SetUpTransportPDU(param->m_mediaChannel, connection.GetControlChannel());
      else
        transport->SetUpTransportPDU(param->m_mediaChannel, H323Transport::UseLocalTSAP);
  }
}


PBoolean H323DataChannel::OnReceivedPDU(const H245_OpenLogicalChannel & open,
                                    unsigned & errorCode)
{
  number = H323ChannelNumber(open.m_forwardLogicalChannelNumber, TRUE);

  PTRACE(3, "LogChan\tOnReceivedPDU for data channel: " << number);

  if (!CreateListener()) {
    PTRACE(1, "LogChan\tCould not create listener");
    errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
    return FALSE;
  }

  if (separateReverseChannel &&
      open.HasOptionalField(H245_OpenLogicalChannel::e_reverseLogicalChannelParameters)) {
    errorCode = H245_OpenLogicalChannelReject_cause::e_unsuitableReverseParameters;
    PTRACE(2, "LogChan\tOnReceivedPDU has unexpected reverse parameters");
    return FALSE;
  }

  if (open.HasOptionalField(H245_OpenLogicalChannel::e_genericInformation) && 
      !connection.OnReceiveOLCGenericInformation(GetSessionID(),open.m_genericInformation, false)) {
        errorCode = H245_OpenLogicalChannelReject_cause::e_unspecified;
        PTRACE(2, "LogChan\tOnReceivedPDU Invalid Generic Parameters");
        return FALSE;
  }

  if (!capability->OnReceivedPDU(open.m_forwardLogicalChannelParameters.m_dataType, receiver)) {
    PTRACE(1, "H323RTP\tData type not supported");
    errorCode = H245_OpenLogicalChannelReject_cause::e_dataTypeNotSupported;
    return FALSE;
  }

  return TRUE;
}


PBoolean H323DataChannel::OnReceivedAckPDU(const H245_OpenLogicalChannelAck & ack)
{
  PTRACE(3, "LogChan\tOnReceivedAckPDU");

  const H245_TransportAddress * address = NULL;

  if (separateReverseChannel) {
      PTRACE(3, "LogChan\tseparateReverseChannels");
    if (!ack.HasOptionalField(H245_OpenLogicalChannelAck::e_forwardMultiplexAckParameters)) {
      PTRACE(1, "LogChan\tNo forwardMultiplexAckParameters");
      return FALSE;
    }

    if (ack.m_forwardMultiplexAckParameters.GetTag() !=
              H245_OpenLogicalChannelAck_forwardMultiplexAckParameters::e_h2250LogicalChannelAckParameters) {
      PTRACE(1, "LogChan\tOnly H.225.0 multiplex supported");
      return FALSE;
    }

    const H245_H2250LogicalChannelAckParameters & param = ack.m_forwardMultiplexAckParameters;

    if (!IsMediaTunneled()) {
        if (!param.HasOptionalField(H245_H2250LogicalChannelAckParameters::e_mediaChannel)) {
          PTRACE(1, "LogChan\tNo media channel address provided");
          return FALSE;
        }
        address = &param.m_mediaChannel;
    }

    if (ack.HasOptionalField(H245_OpenLogicalChannelAck::e_reverseLogicalChannelParameters)) {
      PTRACE(3, "LogChan\treverseLogicalChannelParameters set");
      reverseChannel = H323ChannelNumber(ack.m_reverseLogicalChannelParameters.m_reverseLogicalChannelNumber, TRUE);
    }
  }
  else {
    if (!ack.HasOptionalField(H245_OpenLogicalChannelAck::e_reverseLogicalChannelParameters)) {
      PTRACE(1, "LogChan\tNo reverseLogicalChannelParameters");
      return FALSE;
    }

    if (ack.m_reverseLogicalChannelParameters.m_multiplexParameters.GetTag() !=
              H245_OpenLogicalChannelAck_reverseLogicalChannelParameters_multiplexParameters
                              ::e_h2250LogicalChannelParameters) {
      PTRACE(1, "LogChan\tOnly H.225.0 multiplex supported");
      return FALSE;
    }

    const H245_H2250LogicalChannelParameters & param = ack.m_reverseLogicalChannelParameters.m_multiplexParameters;

    if (!IsMediaTunneled()) {
        if (!param.HasOptionalField(H245_H2250LogicalChannelParameters::e_mediaChannel)) {
          PTRACE(1, "LogChan\tNo media channel address provided");
          return FALSE;
        }
        address = &param.m_mediaChannel;
    }

    if (ack.HasOptionalField(H245_OpenLogicalChannelAck::e_genericInformation) && 
      !connection.OnReceiveOLCGenericInformation(GetSessionID(), ack.m_genericInformation, true)) {
        PTRACE(1, "LogChan\tOnReceivedPDUAck Invalid Generic Parameters");
        return FALSE;
    }
  }

  if (!CreateTransport()) {
    PTRACE(1, "LogChan\tCould not create transport");
    return FALSE;
  }

  if (!address || !transport->ConnectTo(*address)) {
    PTRACE(1, "LogChan\tCould not connect to remote transport address: ");
    return FALSE;
  }

  return TRUE;
}


PBoolean H323DataChannel::CreateListener()
{
  if (listener == NULL) {
    listener = connection.GetControlChannel().GetLocalAddress().CreateCompatibleListener(connection.GetEndPoint());
    if (listener == NULL)
      return FALSE;

    PTRACE(3, "LogChan\tCreated listener for data channel: " << *listener);
  }

  return listener->Open();
}


PBoolean H323DataChannel::CreateTransport()
{
  if (transport == NULL) {
    transport = connection.GetControlChannel().GetLocalAddress().CreateTransport(connection.GetEndPoint());
    if (transport == NULL)
      return FALSE;

    PTRACE(3, "LogChan\tCreated transport for data channel: " << *transport);
  }

  return transport != NULL;
}


/////////////////////////////////////////////////////////////////////////////
