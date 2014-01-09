/*
 * rtp.h
 *
 * RTP protocol handler
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

#ifndef __OPAL_RTP_H
#define __OPAL_RTP_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <openh323buildopts.h>
#include <ptlib/sockets.h>

#include "ptlib_extras.h"

class RTP_JitterBuffer;
class PHandleAggregator;

#ifdef P_STUN
class PNatMethod;
#endif

///////////////////////////////////////////////////////////////////////////////
// 
// class to hold the QoS definitions for an RTP channel

#if P_QOS

class RTP_QOS : public PObject
{
  PCLASSINFO(RTP_QOS,PObject);
  public:
    PQoS dataQoS;
    PQoS ctrlQoS;
};

#else

class RTP_QOS;

#endif

///////////////////////////////////////////////////////////////////////////////
// Real Time Protocol - IETF RFC1889 and RFC1890

/**An RTP data frame encapsulation.
  */
class RTP_DataFrame : public PBYTEArray
{
  PCLASSINFO(RTP_DataFrame, PBYTEArray);

  public:
    RTP_DataFrame(PINDEX payloadSize = 2048, PBoolean dynamicAllocation = TRUE);

    enum {
      ProtocolVersion = 2,
      MinHeaderSize = 12
    };

    enum PayloadTypes {
      PCMU,         // G.711 u-Law
      FS1016,       // Federal Standard 1016 CELP
      G721,         // ADPCM - Subsumed by G.726
      G726 = G721,
      GSM,          // GSM 06.10
      G7231,        // G.723.1 at 6.3kbps or 5.3 kbps
      DVI4_8k,      // DVI4 at 8kHz sample rate
      DVI4_16k,     // DVI4 at 16kHz sample rate
      LPC,          // LPC-10 Linear Predictive CELP
      PCMA,         // G.711 A-Law
      G722,         // G.722
      L16_Stereo,   // 16 bit linear PCM
      L16_Mono,     // 16 bit linear PCM
      G723,         // G.723
      CN,           // Confort Noise
      MPA,          // MPEG1 or MPEG2 audio
      G728,         // G.728 16kbps CELP
      DVI4_11k,     // DVI4 at 11kHz sample rate
      DVI4_22k,     // DVI4 at 22kHz sample rate
      G729,         // G.729 8kbps
      Cisco_CN,     // Cisco systems comfort noise (unofficial)

      CelB = 25,    // Sun Systems Cell-B video
      JPEG,         // Motion JPEG
      H261 = 31,    // H.261
      MPV,          // MPEG1 or MPEG2 video
      MP2T,         // MPEG2 transport system
      H263,         // H.263

      LastKnownPayloadType,

      DynamicBase = 96,
      MaxPayloadType = 127,
      IllegalPayloadType
    };

    unsigned GetVersion() const { return (theArray[0]>>6)&3; }

    PBoolean GetExtension() const   { return (theArray[0]&0x10) != 0; }
    void SetExtension(PBoolean ext);

    PBoolean GetPadding()  { return (theArray[0]&0x20) != 0; }
    void SetPadding(PBoolean padding);

    PBoolean GetMarker() const { return (theArray[1]&0x80) != 0; }
    void SetMarker(PBoolean m);

    PayloadTypes GetPayloadType() const { return (PayloadTypes)(theArray[1]&0x7f); }
    void         SetPayloadType(PayloadTypes t);

    WORD GetSequenceNumber() const { return *(PUInt16b *)&theArray[2]; }
    BYTE * GetSequenceNumberPtr() const;
    void SetSequenceNumber(WORD n) { *(PUInt16b *)&theArray[2] = n; }

    DWORD GetTimestamp() const  { return *(PUInt32b *)&theArray[4]; }
    void  SetTimestamp(DWORD t) { *(PUInt32b *)&theArray[4] = t; }

    DWORD GetSyncSource() const  { return *(PUInt32b *)&theArray[8]; }
    void  SetSyncSource(DWORD s) { *(PUInt32b *)&theArray[8] = s; }

    PINDEX GetContribSrcCount() const { return theArray[0]&0xf; }
    DWORD  GetContribSource(PINDEX idx) const;
    void   SetContribSource(PINDEX idx, DWORD src);

    PINDEX GetHeaderSize() const;

    int GetExtensionType() const; // -1 is no extension
    void   SetExtensionType(int type);
    PINDEX GetExtensionSize() const;
    PBoolean   SetExtensionSize(PINDEX sz);
    BYTE * GetExtensionPtr() const;

    PINDEX GetPayloadSize() const { return payloadSize; }
    PBoolean   SetPayloadSize(PINDEX sz);
    BYTE * GetPayloadPtr()     const { return (BYTE *)(theArray+GetHeaderSize()); }

    PBoolean IsValid() const;

  protected:
    PINDEX payloadSize;

#if PTRACING
    friend ostream & operator<<(ostream & o, PayloadTypes t);
#endif
};

H323LIST(RTP_DataFrameList, RTP_DataFrame);


/**This class is for encapsulating the Multiplexing of RTP Data.
 */

class RTP_MultiDataFrame : public PBYTEArray
{
  PCLASSINFO(RTP_MultiDataFrame, PBYTEArray);

 public:

  RTP_MultiDataFrame(
      BYTE const * buffer,   ///< Pointer to an array of BYTEs.
      PINDEX length          ///< Number of elements pointed to by \p buffer.
  );

  RTP_MultiDataFrame(
      DWORD id,              ///< Mulitplex ID
      const BYTE * buffer,   ///< Pointer to an array of BYTEs.
      PINDEX rtplen          ///< Number of elements pointed to by \p buffer.
  );

  RTP_MultiDataFrame(
      PINDEX rtplen          ///< Length of RTP Frame
  );

  int   GetMultiHeaderSize() const;
  DWORD GetMultiplexID() const;
  void SetMultiplexID(DWORD id);
  void GetRTPPayload(RTP_DataFrame & frame) const;
  void SetRTPPayload(RTP_DataFrame & frame);

  PBoolean IsValidRTPPayload(PBoolean muxed = true) const;
  PBoolean IsNotMultiplexed() const;

};

H323LIST(RTP_MultiDataFrameList, RTP_MultiDataFrame);


/**An RTP control frame encapsulation.
  */
class RTP_ControlFrame : public PBYTEArray
{
  PCLASSINFO(RTP_ControlFrame, PBYTEArray);

  public:
    RTP_ControlFrame(PINDEX compoundSize = 2048);

    unsigned GetVersion() const { return (BYTE)theArray[compoundOffset]>>6; }

    unsigned GetCount() const { return (BYTE)theArray[compoundOffset]&0x1f; }
    void     SetCount(unsigned count);

    enum PayloadTypes {
      e_SenderReport = 200,
      e_ReceiverReport,
      e_SourceDescription,
      e_Goodbye,
      e_ApplDefined
    };

    unsigned GetPayloadType() const { return (BYTE)theArray[compoundOffset+1]; }
    void     SetPayloadType(unsigned t);

    PINDEX GetPayloadSize() const { return 4*(*(PUInt16b *)&theArray[compoundOffset+2]); }
    void   SetPayloadSize(PINDEX sz);

    BYTE * GetPayloadPtr() const { return (BYTE *)(theArray+compoundOffset+4); }

    PBoolean ReadNextCompound();
    PBoolean WriteNextCompound();

    PINDEX GetCompoundSize() const { return compoundSize; }

#pragma pack(1)
    struct ReceiverReport {
      PUInt32b ssrc;      /* data source being reported */
      BYTE fraction;      /* fraction lost since last SR/RR */
      BYTE lost[3];	  /* cumulative number of packets lost (signed!) */
      PUInt32b last_seq;  /* extended last sequence number received */
      PUInt32b jitter;    /* interarrival jitter */
      PUInt32b lsr;       /* last SR packet from this source */
      PUInt32b dlsr;      /* delay since last SR packet */

      unsigned GetLostPackets() const { return (lost[0]<<16U)+(lost[1]<<8U)+lost[2]; }
      void SetLostPackets(unsigned lost);
    };

    struct SenderReport {
      PUInt32b ssrc;      /* source this RTCP packet refers to */
      PUInt32b ntp_sec;   /* NTP timestamp */
      PUInt32b ntp_frac;
      PUInt32b rtp_ts;    /* RTP timestamp */
      PUInt32b psent;     /* packets sent */
      PUInt32b osent;     /* octets sent */ 
    };

    enum DescriptionTypes {
      e_END,
      e_CNAME,
      e_NAME,
      e_EMAIL,
      e_PHONE,
      e_LOC,
      e_TOOL,
      e_NOTE,
      e_PRIV,
      NumDescriptionTypes
    };

    struct SourceDescription {
      PUInt32b src;       /* first SSRC/CSRC */
      struct Item {
        BYTE type;        /* type of SDES item (enum DescriptionTypes) */
        BYTE length;      /* length of SDES item (in octets) */
        char data[1];     /* text, not zero-terminated */

        const Item * GetNextItem() const { return (const Item *)((char *)this + length + 2); }
        Item * GetNextItem() { return (Item *)((char *)this + length + 2); }
      } item[1];          /* list of SDES items */
    };

    SourceDescription & AddSourceDescription(
      DWORD src   ///<  SSRC/CSRC identifier
    );

    SourceDescription::Item & AddSourceDescriptionItem(
      SourceDescription & sdes, ///<  SDES record to add item to
      unsigned type,            ///<  Description type
      const PString & data      ///<  Data for description
    );
#pragma pack()

  protected:
    PINDEX compoundOffset;
    PINDEX compoundSize;
};

/**This class is for encapsulating the Multiplexing of RTCP.
 */

class RTP_MultiControlFrame : public PBYTEArray
{
  PCLASSINFO(RTP_MultiControlFrame, PBYTEArray);

 public:

  RTP_MultiControlFrame(
      BYTE const * buffer,   ///< Pointer to an array of BYTEs.
      PINDEX length          ///< Number of elements pointed to by \p buffer.
  );

  RTP_MultiControlFrame(
      PINDEX rtplen          ///< Length of RTP Frame
  );

  int  GetMultiHeaderSize() const;
  WORD GetMultiplexID() const;
  void SetMultiplexID(WORD id);
  void GetRTCPPayload(RTP_ControlFrame & frame) const;
  void SetRTCPPayload(RTP_ControlFrame & frame);

};


/**This class is for encpsulating the IETF Real Time Protocol interface.
 */
class RTP_UDP;
class RTP_UserData;
class RTP_Session : public PObject
{
  PCLASSINFO(RTP_Session, PObject);

  public:
    enum {
      DefaultAudioSessionID    = 1,
      DefaultVideoSessionID    = 2,
      DefaultFaxSessionID      = 3,
      DefaultH224SessionID     = 3,
      DefaultExtVideoSessionID = 4,
      DefaultFileSessionID     = 5
    };

  /**@name Construction */
  //@{
    /**Create a new RTP session.
     */
    RTP_Session(
#ifdef H323_RTP_AGGREGATE
      PHandleAggregator * aggregator, ///<  RTP aggregator
#endif
      unsigned id,                    ///<  Session ID for RTP channel
      RTP_UserData * userData = NULL  ///<  Optional data for session.
    );

    /**Delete a session.
       This deletes the userData field.
     */
    ~RTP_Session();
  //@}

  /**@name Operations */
  //@{
    /**Sets the size of the jitter buffer to be used by this RTP session.
       A session default to not having any jitter buffer enabled for reading
       and the ReadBufferedData() function simply calls ReadData(). Once a
       jitter buffer has been created it cannot be removed, though its size
       may be adjusted.
       
       If the jitterDelay paramter is zero, it destroys the jitter buffer
       attached to this RTP session.
      */
    void SetJitterBufferSize(
      unsigned minJitterDelay, ///<  Minimum jitter buffer delay in RTP timestamp units
      unsigned maxJitterDelay, ///<  Maximum jitter buffer delay in RTP timestamp units
      PINDEX stackSize = 30000 ///<  Stack size for jitter thread
    );

    /**Get current size of the jitter buffer.
       This returns the currently used jitter buffer delay in RTP timestamp
       units. It will be some value between the minimum and maximum set in
       the SetJitterBufferSize() function.
      */
    unsigned GetJitterBufferSize() const;

    /**Modifies the QOS specifications for this RTP session*/
    virtual PBoolean ModifyQOS(RTP_QOS * )
    { return FALSE; }

    /**Read a data frame from the RTP channel.
       This function will conditionally read data from eth jitter buffer or
       directly if there is no jitter buffer enabled. An application should
       generally use this in preference to directly calling ReadData().
      */
    PBoolean ReadBufferedData(
      DWORD timestamp,        ///<  Timestamp to read from buffer.
      RTP_DataFrame & frame   ///<  Frame read from the RTP session
    );

    /**Read a data frame from the RTP channel.
       Any control frames received are dispatched to callbacks and are not
       returned by this function. It will block until a data frame is
       available or an error occurs.
      */
    virtual PBoolean ReadData(
      RTP_DataFrame & frame,   ///<  Frame read from the RTP session
      PBoolean loop                ///<  If TRUE, loop as long as data is available, if FALSE, only process once
    ) = 0;

    /**Read a data frame from the RTP channel that did not orginate
       from the UDPSocket
      */
    virtual PBoolean PseudoRead(
        int & selectStatus
     );

    /**Check the data before sending to the RTP Channel
      */
    virtual PBoolean PreWriteData(
        RTP_DataFrame & frame   ///<  Frame to write to the RTP session
    ) = 0;

    /**Write a data frame from the RTP channel.
      */
    virtual PBoolean WriteData(
      RTP_DataFrame & frame   ///<  Frame to write to the RTP session
    ) = 0;

    /**Write a control frame from the RTP channel.
      */
    virtual PBoolean WriteControl(
      RTP_ControlFrame & frame    ///<  Frame to write to the RTP session
    ) = 0;

    /**Write the RTCP reports.
      */
    virtual PBoolean SendReport();

    /**Close down the RTP session.
      */
    virtual void Close(
      PBoolean reading    ///<  Closing the read side of the session
    ) = 0;

    /**Get the local host name as used in SDES packes.
      */
    virtual PString GetLocalHostName() = 0;
  //@}

  /**@name Call back functions */
  //@{
    enum SendReceiveStatus {
      e_ProcessPacket,
      e_IgnorePacket,
      e_AbortTransport
    };
    virtual SendReceiveStatus OnSendData(RTP_DataFrame & frame);
    virtual SendReceiveStatus OnReceiveData(const RTP_DataFrame & frame, const RTP_UDP & rtp);
    virtual SendReceiveStatus OnReceiveControl(RTP_ControlFrame & frame);

    class ReceiverReport : public PObject  {
        PCLASSINFO(ReceiverReport, PObject);
      public:
        void PrintOn(ostream &) const;

        DWORD sourceIdentifier;
        DWORD fractionLost;         /* fraction lost since last SR/RR */
        DWORD totalLost;	    /* cumulative number of packets lost (signed!) */
        DWORD lastSequenceNumber;   /* extended last sequence number received */
        DWORD jitter;               /* interarrival jitter */
        PTimeInterval lastTimestamp;/* last SR packet from this source */
        PTimeInterval delay;        /* delay since last SR packet */
    };
    PARRAY(ReceiverReportArray, ReceiverReport);

    class SenderReport : public PObject  {
        PCLASSINFO(SenderReport, PObject);
      public:
        SenderReport() 
        : sourceIdentifier(0), realTimestamp1970(0), 
          rtpTimestamp(0), packetsSent(0), octetsSent(0) {}

        void PrintOn(ostream &) const;

        DWORD sourceIdentifier;
        PTime  realTimestamp;
        PInt64 realTimestamp1970;
        DWORD  rtpTimestamp;
        DWORD  packetsSent;
        DWORD  octetsSent;
    };
    virtual void OnRxSenderReport(const SenderReport & sender,
                                  const ReceiverReportArray & reports);
    virtual void OnRxReceiverReport(DWORD src,
                                    const ReceiverReportArray & reports);
    virtual bool AVSyncData(SenderReport & sender);

    class SourceDescription : public PObject  {
        PCLASSINFO(SourceDescription, PObject);
      public:
        SourceDescription(DWORD src) { sourceIdentifier = src; }
        void PrintOn(ostream &) const;

        DWORD            sourceIdentifier;
        POrdinalToString items;
    };
    PARRAY(SourceDescriptionArray, SourceDescription);
    virtual void OnRxSourceDescription(const SourceDescriptionArray & descriptions);

    virtual void OnRxGoodbye(const PDWORDArray & sources,
                             const PString & reason);

    virtual void OnRxApplDefined(const PString & type, unsigned subtype, DWORD src,
                                 const BYTE * data, PINDEX size);
  //@}

  /**@name Member variable access */
  //@{
    /**Get the ID for the RTP session.
      */
    unsigned GetSessionID() const { return sessionID; }

    /**Set the ID for the RTP session.
      */
    void SetSessionID(unsigned id);

    /**Get the canonical name for the RTP session.
      */
    PString GetCanonicalName() const;

    /**Set the canonical name for the RTP session.
      */
    void SetCanonicalName(const PString & name);

    /**Get the tool name for the RTP session.
      */
    PString GetToolName() const;

    /**Set the tool name for the RTP session.
      */
    void SetToolName(const PString & name);

    /**Get the user data for the session.
      */
    RTP_UserData * GetUserData() const { return userData; }

    /**Set the user data for the session.
      */
    void SetUserData(
      RTP_UserData * data   ///<  New user data to be used
    );

    /**Get the source output identifier.
      */
    DWORD GetSyncSourceOut() const { return syncSourceOut; }

    /**Increment reference count for RTP session.
      */
    void IncrementReference() { referenceCount++; }

    /**Decrement reference count for RTP session.
      */
    PBoolean DecrementReference() { return --referenceCount == 0; }

	/**Get Reference Count
	  */
	unsigned GetReferenceCount() const { return referenceCount; }

    /**Indicate if will ignore all but first received SSRC value.
      */
    PBoolean WillIgnoreOtherSources() const { return ignoreOtherSources; }

    /**Indicate if will ignore all but first received SSRC value.
      */
    void SetIgnoreOtherSources(
      PBoolean ignore   ///<  Flag for ignore other SSRC values
    ) { ignoreOtherSources = ignore; }

    /**Indicate if will ignore out of order packets.
      */
    PBoolean WillIgnoreOutOfOrderPackets() const { return ignoreOutOfOrderPackets; }

    /**Indicate if will ignore out of order packets.
      */
    void SetIgnoreOutOfOrderPackets(
      PBoolean ignore   ///<  Flag for ignore out of order packets
    ) { ignoreOutOfOrderPackets = ignore; }

    /**Get the time interval for sending RTCP reports in the session.
      */
    const PTimeInterval & GetReportTimeInterval() { return reportTimeInterval; }

    /**Set the time interval for sending RTCP reports in the session.
      */
    void SetReportTimeInterval(
      const PTimeInterval & interval ///<  New time interval for reports.
    )  { reportTimeInterval = interval; }

    /**Get the current report timer
     */
    PTimeInterval GetReportTimer()
    { return reportTimer; }

    /**Get the interval for transmitter statistics in the session.
      */
    unsigned GetTxStatisticsInterval() { return txStatisticsInterval; }

    /**Set the interval for transmitter statistics in the session.
      */
    void SetTxStatisticsInterval(
      unsigned packets   ///<  Number of packets between callbacks
    );

    /**Get the interval for receiver statistics in the session.
      */
    unsigned GetRxStatisticsInterval() { return rxStatisticsInterval; }

    /**Set the interval for receiver statistics in the session.
      */
    void SetRxStatisticsInterval(
      unsigned packets   ///<  Number of packets between callbacks
    );

    /**Get total number of packets sent in session.
      */
    DWORD GetPacketsSent() const { return packetsSent; }

    /**Get total number of octets sent in session.
      */
    DWORD GetOctetsSent() const { return octetsSent; }

    /**Get total number of packets received in session.
      */
    DWORD GetPacketsReceived() const { return packetsReceived; }

    /**Get total number of octets received in session.
      */
    DWORD GetOctetsReceived() const { return octetsReceived; }

    /**Get total number received packets lost in session.
      */
    DWORD GetPacketsLost() const { return packetsLost; }

    /**Get total number of packets received out of order in session.
      */
    DWORD GetPacketsOutOfOrder() const { return packetsOutOfOrder; }

    /**Get total number received packets too late to go into jitter buffer.
      */
    DWORD GetPacketsTooLate() const;

    /**Get average time between sent packets.
       This is averaged over the last txStatisticsInterval packets and is in
       milliseconds.
      */
    DWORD GetAverageSendTime() const { return averageSendTime; }

    /**Get maximum time between sent packets.
       This is over the last txStatisticsInterval packets and is in
       milliseconds.
      */
    DWORD GetMaximumSendTime() const { return maximumSendTime; }

    /**Get minimum time between sent packets.
       This is over the last txStatisticsInterval packets and is in
       milliseconds.
      */
    DWORD GetMinimumSendTime() const { return minimumSendTime; }

    /**Get average time between received packets.
       This is averaged over the last rxStatisticsInterval packets and is in
       milliseconds.
      */
    DWORD GetAverageReceiveTime() const { return averageReceiveTime; }

    /**Get maximum time between received packets.
       This is over the last rxStatisticsInterval packets and is in
       milliseconds.
      */
    DWORD GetMaximumReceiveTime() const { return maximumReceiveTime; }

    /**Get minimum time between received packets.
       This is over the last rxStatisticsInterval packets and is in
       milliseconds.
      */
    DWORD GetMinimumReceiveTime() const { return minimumReceiveTime; }

    /**Get averaged jitter time for received packets.
       This is the calculated statistical variance of the interarrival
       time of received packets in milliseconds.
      */
    DWORD GetAvgJitterTime() const { return jitterLevel>>7; }

    /**Get averaged jitter time for received packets.
       This is the maximum value of jitterLevel for the session.
      */
    DWORD GetMaxJitterTime() const { return maximumJitterLevel>>7; }

    /**
      * return the timestamp at which the first packet of RTP data was received
      */
    PTime GetFirstDataReceivedTime() const { return firstDataReceivedTime; }

	/**
	  * return the local Transport Address
	  */
	PString GetLocalTransportAddress() const { return locAddress; };

	/**
	  * return the remote Transport Address
	  */
	PString GetRemoteTransportAddress() const { return remAddress; };

  //@}

  /**@name Functions added to RTP aggregator */
  //@{
    virtual PINDEX GetDataSocketHandle() const
    { return PINDEX(-1); }
    virtual PINDEX GetControlSocketHandle() const
    { return PINDEX(-1); }
  //@}

  protected:
    void AddReceiverReport(RTP_ControlFrame::ReceiverReport & receiver);

    unsigned           sessionID;
    PString            canonicalName;
    PString            toolName;
    unsigned           referenceCount;
    RTP_UserData     * userData;

#ifdef H323_AUDIO_CODECS
    RTP_JitterBuffer * jitter;
#endif

    PBoolean          ignoreOtherSources;
    unsigned          ignoreOtherSourcesCount;
    unsigned          ignoreOtherSourceMaximum;
    PBoolean          ignoreOutOfOrderPackets;
    DWORD         syncSourceOut;
    DWORD         syncSourceIn;
    PTimeInterval reportTimeInterval;
    unsigned      txStatisticsInterval;
    unsigned      rxStatisticsInterval;
    WORD          lastSentSequenceNumber;
    WORD          expectedSequenceNumber;
    DWORD         lastSentTimestamp;
    PInt64        lastSentPacketTime;
    PInt64        lastReceivedPacketTime;
    WORD          lastRRSequenceNumber;
    PINDEX        consecutiveOutOfOrderPackets;

    // Statistics
    DWORD packetsSent;
    DWORD octetsSent;
    DWORD packetsReceived;
    DWORD octetsReceived;
    DWORD packetsLost;
    DWORD packetsOutOfOrder;
    DWORD averageSendTime;
    DWORD maximumSendTime;
    DWORD minimumSendTime;
    DWORD averageReceiveTime;
    DWORD maximumReceiveTime;
    DWORD minimumReceiveTime;
    DWORD jitterLevel;
    DWORD maximumJitterLevel;

	// Socket information
    PString locAddress;
    PString remAddress;

    unsigned txStatisticsCount;
    unsigned rxStatisticsCount;

    DWORD    averageSendTimeAccum;
    DWORD    maximumSendTimeAccum;
    DWORD    minimumSendTimeAccum;
    DWORD    averageReceiveTimeAccum;
    DWORD    maximumReceiveTimeAccum;
    DWORD    minimumReceiveTimeAccum;
    DWORD    packetsLostSinceLastRR;
    DWORD    lastTransitTime;
    PTime    firstDataReceivedTime;

    PMutex reportMutex;
    PTimer reportTimer;

    // Sync Information
    PBoolean avSyncData;
    SenderReport  rtpSync;

#ifdef H323_RTP_AGGREGATE
    PHandleAggregator * aggregator;
#endif
};


/**This class is the base for user data that may be attached to the RTP_session
   allowing callbacks for statistics and progress monitoring to be passed to an
   arbitrary object that an RTP consumer may require.
  */
class RTP_UserData : public PObject
{
  PCLASSINFO(RTP_UserData, PObject);

  public:
    /**Callback from the RTP session for transmit statistics monitoring.
       This is called every RTP_Session::txStatisticsInterval packets on the
       transmitter indicating that the statistics have been updated.

       The default behaviour does nothing.
      */
    virtual void OnTxStatistics(
      const RTP_Session & session   ///<  Session with statistics
    ) const;

    /**Callback from the RTP session for receive statistics monitoring.
       This is called every RTP_Session::receiverReportInterval packets on the
       receiver indicating that the statistics have been updated.

       The default behaviour does nothing.
      */
    virtual void OnRxStatistics(
      const RTP_Session & session   ///<  Session with statistics
    ) const;

    /**Callback from the RTP session for statistics monitoring.
       This is called at the end of a call to indicating 
	   that the statistics of the call.

       The default behaviour does nothing.
      */
    virtual void OnFinalStatistics(
      const RTP_Session & session   ///<  Session with statistics
    ) const;

    /**Callback from the RTP session for statistics monitoring.
       This is called when a RTPSenderReport is received

       The default behaviour does nothing.
      */
    virtual void OnRxSenderReport(
        unsigned sessionID,
        const RTP_Session::SenderReport & send,
        const RTP_Session::ReceiverReportArray & recv
        ) const;

};



/**This class is for encpsulating the IETF Real Time Protocol interface.
 */
class RTP_SessionManager : public PObject
{
  PCLASSINFO(RTP_SessionManager, PObject);

  public:
  /**@name Construction */
  //@{
    /**Construct new session manager database.
      */
    RTP_SessionManager();
    RTP_SessionManager(const RTP_SessionManager & sm);
    RTP_SessionManager & operator=(const RTP_SessionManager & sm);
  //@}


  /**@name Operations */
  //@{
    /**Use an RTP session for the specified ID.

       If this function returns a non-null value, then the ReleaseSession()
       function MUST be called or the session is never deleted for the
       lifetime of the session manager.

       If there is no session of the specified ID, then you MUST call the
       AddSession() function with a new RTP_Session. The mutex flag is left
       locked in this case. The AddSession() expects the mutex to be locked
       and unlocks it automatically.
      */
    RTP_Session * UseSession(
      unsigned sessionID    ///<  Session ID to use.
    );

    /**Add an RTP session for the specified ID.

       This function MUST be called only after the UseSession() function has
       returned NULL. The mutex flag is left locked in that case. This
       function expects the mutex to be locked and unlocks it automatically.
      */
    void AddSession(
      RTP_Session * session    ///<  Session to add.
    );

    /**Release the session. If the session ID is not being used any more any
       clients via the UseSession() function, then the session is deleted.
     */
    void ReleaseSession(
      unsigned sessionID    ///<  Session ID to release.
    );

    /**Move the session. 
     */
    void MoveSession(
      unsigned oldSessionID,    ///<  Session ID from.
      unsigned newSessionID     ///<  Session ID to.
    );

    /**Get a session for the specified ID.
       Unlike UseSession, this does not increment the usage count on the
       session so may be used to just gain a pointer to an RTP session.
     */
    RTP_Session * GetSession(
      unsigned sessionID    /// Session ID to get.
    ) const;

    /**Begin an enumeration of the RTP sessions.
       This function acquires the mutex for the sessions database and returns
       the first element in it.

         eg:
         RTP_Session * session;
         for (session = rtpSessions.First(); session != NULL; session = rtpSessions.Next()) {
           if (session->Something()) {
             rtpSessions.Exit();
             break;
           }
         }

       Note that the Exit() function must be called if the enumeration is
       stopped prior to Next() returning NULL.
      */
    RTP_Session * First();

    /**Get the next session in the enumeration.
       The session database remains locked until this function returns NULL.

       Note that the Exit() function must be called if the enumeration is
       stopped prior to Next() returning NULL.
      */
    RTP_Session * Next();

    /**Exit the enumeration of RTP sessions.
       If the enumeration is desired to be exited before Next() returns NULL
       this this must be called to unlock the session database.

       Note that you should NOT call Exit() if Next() HAS returned NULL, or
       race conditions can result.
      */
    void Exit();
  //@}


  protected:
    H323DICTIONARY(SessionDict, POrdinalKey, RTP_Session);
    SessionDict sessions;
    PMutex      mutex;
    PINDEX      enumerationIndex;
};



/**This class is for the IETF Real Time Protocol interface on UDP/IP.
 */
class H323Connection;
class RTP_UDP : public RTP_Session
{
  PCLASSINFO(RTP_UDP, RTP_Session);

  public:
  /**@name Construction */
  //@{
    /**Create a new RTP channel.
     */
    RTP_UDP(
#ifdef H323_RTP_AGGREGATE
      PHandleAggregator * aggregator, ///< RTP aggregator
#endif
      unsigned id,                    ///<  Session ID for RTP channel
      PBoolean remoteIsNat = FALSE,   ///<  If TRUE, we have hints the remote endpoint is behind a NAT
      PBoolean mediaTunneled = FALSE  ///<  Whether media is tunneled.
    );

    /// Destroy the RTP
    ~RTP_UDP();
  //@}

  /**@name Overrides from class RTP_Session */
  //@{
    /**Read a data frame from the RTP channel.
       Any control frames received are dispatched to callbacks and are not
       returned by this function. It will block until a data frame is
       available or an error occurs.
      */
    virtual PBoolean ReadData(RTP_DataFrame & frame, PBoolean loop);

    /**Read a data frame from the RTP channel that did not orginate
       from the UDPSocket
      */
    virtual PBoolean PseudoRead(int & selectStatus);

    /**Check the data before sending to the RTP Channel
      */
    PBoolean PreWriteData(RTP_DataFrame & frame);

    /**Write a data frame from the RTP channel.
      */
    virtual PBoolean WriteData(RTP_DataFrame & frame);

    /**Write a control frame from the RTP channel.
      */
    virtual PBoolean WriteControl(RTP_ControlFrame & frame);

    /**Close down the RTP session.
      */
    virtual void Close(
      PBoolean reading    ///<  Closing the read side of the session
    );

    /**Get the session description name.
      */
    virtual PString GetLocalHostName();
  //@}

  /**@name QoS Settings */
  //@{
    /**Change the QoS settings
      */
    virtual PBoolean ModifyQOS(RTP_QOS * rtpqos);

    /** Enable QOS on Call Basis
      */
    virtual void EnableGQoS(PBoolean success = TRUE);

#if P_QOS
     /** Get the QOS settings
       */
      PQoS & GetQOS();
#endif
  //@}

  /**@name New functions for class */
  //@{
    /**Open the UDP ports for the RTP session.
      */
    PBoolean Open(
      PIPSocket::Address localAddress,  ///<  Local interface to bind to
      WORD portBase,                    ///<  Base of ports to search
      WORD portMax,                     ///<  end of ports to search (inclusive)
      BYTE ipTypeOfService,             ///<  Type of Service byte
	  const H323Connection & connection, ///< Connection
#ifdef P_STUN
      PNatMethod * meth = NULL,         ///< Nat Method to use to create sockets (or NULL if no Method)
#else
      void * = NULL,                    ///<  STUN server to use createing sockets (or NULL if no STUN)
#endif
      RTP_QOS * rtpqos = NULL           ///<  QOS spec (or NULL if no QoS)
    );
  //@}

   /**Reopens an existing session in the given direction.
      */
    void Reopen(PBoolean isReading);
  //@}

  /**@name Member variable access */
  //@{
    /**Get local address of session.
      */
    PIPSocket::Address GetLocalAddress() const { return localAddress; }

    /**Set local address of session.
      */
    void SetLocalAddress(
      const PIPSocket::Address & addr
    ) { localAddress = addr; }

    /**Get remote address of session.
      */
    PIPSocket::Address GetRemoteAddress() const { return remoteAddress; }

    /**Get local data port of session.
      */
    WORD GetLocalDataPort() const { return localDataPort; }

    /**Get local control port of session.
      */
    WORD GetLocalControlPort() const { return localControlPort; }

    /**Get remote data port of session.
      */
    WORD GetRemoteDataPort() const { return remoteDataPort; }

    /**Get remote control port of session.
      */
    WORD GetRemoteControlPort() const { return remoteControlPort; }

    /**Get data UDP socket of session.
      */
    PUDPSocket & GetDataSocket() { return *dataSocket; }

    /**Get control UDP socket of session.
      */
    PUDPSocket & GetControlSocket() { return *controlSocket; }

    /**Set the remote address and port information for session.
      */
    PBoolean SetRemoteSocketInfo(
      PIPSocket::Address address,   ///<  Address of remote
      WORD port,                    ///<  Port on remote
      PBoolean isDataPort               ///<  Flag for data or control channel
    );

    /**Apply QOS - requires address to connect the socket on Windows platforms
     */
    void ApplyQOS(
      const PIPSocket::Address & addr
    );

    /**Whether the remote has been detected as being behind NAT.
      */
    PBoolean IsRemoteNAT()  { return remoteIsNAT; }

    /**Whether the remote has been detected as being behind NAT.
      */
    PBoolean IsMediaTunneled()  { return mediaIsTunneled; }
  //@}

    PINDEX GetDataSocketHandle() const
    { return dataSocket != NULL ? dataSocket->GetHandle() : -1; }

    PINDEX GetControlSocketHandle() const
    { return controlSocket != NULL ? controlSocket->GetHandle() : -1; }

  protected:
    SendReceiveStatus ReadDataPDU(RTP_DataFrame & frame);
    SendReceiveStatus ReadControlPDU();
    SendReceiveStatus ReadDataOrControlPDU(
      PUDPSocket & socket,
      PBYTEArray & frame,
      PBoolean fromDataChannel
    );

    PIPSocket::Address localAddress;
    WORD               localDataPort;
    WORD               localControlPort;

    PIPSocket::Address remoteAddress;
    WORD               remoteDataPort;
    WORD               remoteControlPort;

    PIPSocket::Address remoteTransmitAddress;

    PBoolean shutdownRead;
    PBoolean shutdownWrite;

    PUDPSocket * dataSocket;
    PUDPSocket * controlSocket;

    PBoolean appliedQOS;
    PBoolean enableGQOS;

    PBoolean remoteIsNAT;
    unsigned successiveWrongAddresses;

    PBoolean mediaIsTunneled;
};


#endif // __OPAL_RTP_H


/////////////////////////////////////////////////////////////////////////////
