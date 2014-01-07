/*
 * codecs.h
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

#ifndef __CODECS_H
#define __CODECS_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include <mediafmt.h>
#include <rtp.h>
#include <channels.h>
#include "openh323buildopts.h"
#include <ptlib/video.h>

/* The following classes have forward references to avoid including the VERY
   large header files for H225 and H245. If an application requires access
   to the protocol classes they can include them, but for simple usage their
   inclusion can be avoided.
 */
class H245_MiscellaneousCommand_type;
class H245_MiscellaneousIndication_type;
class H323Connection;

///////////////////////////////////////////////////////////////////////////////

/**This class embodies the implementation of a specific codec instance
   used to transfer data via the logical channels opened and managed by the
   H323 control channel.

   An application may create a descendent off this class and override
   functions as required for descibing a codec.
 */
class H323Codec : public PObject
{
  PCLASSINFO(H323Codec, PObject);

  public:

    enum Direction {
      Encoder,
      Decoder
    };

	struct H323_RTPInformation
	{
		int                   m_sessionID;
        unsigned              m_timeStamp;
        unsigned              m_clockRate;
        int                   m_frameLost;
        PInt64                m_sendTime;
        PInt64                m_recvTime;
		const RTP_DataFrame * m_frame;
	};

    struct AVSync
    {
        DWORD  m_rtpTimeStamp;
        PInt64 m_realTimeStamp;
    };

    H323Codec(
      const OpalMediaFormat & mediaFormat, ///< Media format for codec
      Direction direction       ///< Direction in which this instance runs
    );


    /**Open the codec.
       This will open the codec for encoding or decoding, it is called after
       the logical channel have been established and the background threads to
       drive them have been started. This is primarily used to delay allocation
       of resources until the last millisecond.

       A descendent class may be created by the application and it may cast
       the connection parameter to the application defined descendent of 
       H323Connection to obtain information needed to open the codec.

       The default behaviour does nothing.
      */
    virtual PBoolean Open(
      H323Connection & connection ///< Connection between the endpoints
    );

    /**Close the codec.
      */
    virtual void Close() = 0;

    /**Encode the data from the appropriate device.
       This will encode data for transmission. The exact size and description
       of the data placed in the buffer is codec dependent but should be less
       than OpalMediaFormat::GetFrameSize() in length.

       The length parameter is filled with the actual length of the encoded
       data, often this will be the same as OpalMediaFormat::GetFrameSize().

       This function is called every GetFrameRate() timestamp units, so MUST
       take less than (or equal to) that amount of time to complete. It
       should always return the amount of data that corresponds to the
       GetFrameRate() timestamp units as well.

       A returned length of zero indicates that time has passed but there is
       no data encoded. This is typically used for silence detection in an
       audio codec.
     */
    virtual PBoolean Read(
      BYTE * buffer,            ///< Buffer of encoded data
      unsigned & length,        ///< Actual length of encoded data buffer
      RTP_DataFrame & rtpFrame  ///< RTP data frame
    ) = 0;

    /**Decode the data and output it to appropriate device.
       This will decode a single frame of received data. The exact size and
       description of the data required in the buffer is codec dependent but
       should be at least than OpalMediaFormat::GetFrameSize() in length.

       It is expected this function anunciates the data. That is, for example
       with audio data, the sound is output on a speaker.

       This function is called every GetFrameRate() timestamp units, so MUST
       take less than that amount of time to complete!

       This may call write Internal
     */
    virtual PBoolean Write(
      const BYTE * buffer,          ///< Buffer of encoded data
      unsigned length,              ///< Length of encoded data buffer
      const RTP_DataFrame & frame,  ///< Entire RTP frame
      unsigned & written            ///< Number of bytes used from data buffer
    ) = 0;

    /**Decode the data and output it to appropriate device.
       This will decode a single frame of received data. The exact size and
       description of the data required in the buffer is codec dependent but
       should be at least than OpalMediaFormat::GetFrameSize() in length.

       It is expected this function anunciates the data. That is, for example
       with audio data, the sound is output on a speaker.

       This function is called every GetFrameRate() timestamp units, so MUST
       take less than that amount of time to complete!
     */
    virtual PBoolean WriteInternal(
      const BYTE * /*buffer*/,              ///< Buffer of encoded data
      unsigned /*length*/,                  ///< Length of encoded data buffer
      const RTP_DataFrame & /*frame*/,      ///< Entire RTP frame
      unsigned & /*written*/,               ///< Number of bytes used from data buffer
      H323_RTPInformation &  /*rtp*/        ///< RTP Information
    ) { return false; }

    /**Get the frame rate in RTP timestamp units.
      */
    virtual unsigned GetFrameRate() const;

    /**Limit bit flow for the logical channel.
       The default behaviour does nothing.
     */
    virtual void OnFlowControl(
      long bitRateRestriction   ///< Bit rate limitation
    );

    /**Process a miscellaneous command on the logical channel.
       The default behaviour does nothing.
     */
    virtual void OnMiscellaneousCommand(
      const H245_MiscellaneousCommand_type & type  ///< Command to process
    );

    /**Process a miscellaneous indication on the logical channel.
       The default behaviour does nothing.
     */
    virtual void OnMiscellaneousIndication(
      const H245_MiscellaneousIndication_type & type  ///< Indication to process
    );

    Direction GetDirection()   const { return direction; }

    virtual const OpalMediaFormat & GetMediaFormat() const { return mediaFormat; }
    OpalMediaFormat & GetWritableMediaFormat() { return mediaFormat; }

    virtual PBoolean SetFrameSize(int /*frameWidth*/, int /*frameHeight*/) { return FALSE; };

    /**Attach the raw data channel for use by codec.
       Note the channel provided will be deleted on destruction of the codec.
       
       The channel connects the codec (audio or video) with hardware to read/write data.
       Thus, the video codec provides a pointer to the data, which the renderer/grabber
       then accesses to display/grab the image from/to.
      */
    virtual PBoolean AttachChannel(
      PChannel * channel,     ///< Channel to read/write raw codec data
      PBoolean autoDelete = TRUE  ///< Channel is to be automatically deleted
    );

    /**Attach a new channel and returns the previous one, without neither
       deleting it nor closing it. This method is used to, for example,
       when swapping to or from a hold media channel
     */
    virtual PChannel * SwapChannel(
      PChannel * newChannel,  ///< Channel to read/write raw codec data
      PBoolean autoDelete = TRUE  ///< Channel is to be automatically deleted
    );

    /**Close the raw data channel, described in H323Codec::AttachChannel
      */
    virtual PBoolean CloseRawDataChannel();

    /**Return a pointer to the raw data channel, which can then be used to
       access the recording/playing device. (or testing if channel is attached).
    */
    PChannel *GetRawDataChannel()
      {	return rawDataChannel; }

    /**Return flag indicating raw channel is native.
       For audio codecs, FALSE typically means that the format is PCM-16.
       For video codecs, FALSE typically means that the format is YUV411P.

       The default behaviour returns FALSE.
      */
    virtual PBoolean IsRawDataChannelNative() const;

    /**Read from the raw data channel.
      */
    PBoolean ReadRaw(
      void * data,
      PINDEX size,
      PINDEX & length
    );

    /**Write from the raw data channel.
      */
    PBoolean WriteRaw(
      void * data,
      PINDEX length,
	  void * mark
    );

    /**Write from the raw data channel.
      */
    PBoolean WriteInternal(
      void * data,
      PINDEX length,
	  void * mark
    );

    /**Attach the logical channel, for use by the codec.
       The channel provided is not deleted on destruction, it is just used.

       The logical channel provides a means for the codec to send control messages.
       E.G. the receive video codec wants to receive a frame update.
    */
    PBoolean AttachLogicalChannel(H323Channel *channel);

    class FilterInfo : public PObject {
        PCLASSINFO(FilterInfo, PObject);
      public:
        FilterInfo(H323Codec & c) 
          : codec(c), buffer(NULL), bufferSize(0), bufferLength(0) {}

        FilterInfo(H323Codec & c, void * b, PINDEX s, PINDEX l)
          : codec(c), buffer(b), bufferSize(s), bufferLength(l) { }

        H323Codec & codec;
        void      * buffer;
        PINDEX      bufferSize;
        PINDEX      bufferLength;
    };

    class FilterData : public PObject {
        PCLASSINFO(FilterData, PObject);
      public:
        FilterData(H323Codec & c, const PNotifier & n)
         : m_filterInfo(c), m_notifier(new PNotifier(n)) {}

        ~FilterData() { delete m_notifier; }

        PINDEX ProcessFilter(void * b, PINDEX s, PINDEX l) {
            m_filterInfo.buffer = b;
            m_filterInfo.bufferSize = s;
            m_filterInfo.bufferLength = l;
            (*m_notifier)(m_filterInfo, 0);
            return m_filterInfo.bufferLength;
        }

        FilterInfo   m_filterInfo;
        PNotifier  * m_notifier;
    };

    /**Add a filter to the codec.
       The call back function is executed just after reading from, or just
       before writing to, the raw data channel. The callback is passed the
       H323Codec::FilterInfo structure containing the data that is being
       read or written.

       To use define:
         PDECLARE_NOTIFIER(H323Codec::FilterInfo, YourClass, YourFunction);
       and
         void YourClass::YourFunction(H323Codec::FilterInfo & info, INT)
         {
           // DO something with data
         }
       and to connect to a codec:
         PBoolean YourClass::OnStartLogicalChannel(H323Channel & channel)
         {
           H323Codec * codec = channel.GetCodec();
           codec->AddFilter(PCREATE_NOTIFIER(YourFunction));
         }
       for example. Other places can be used to set the filter.
      */
    void AddFilter(
      const PNotifier & notifier
    );

   /**SetRawDataHeld is called when the cuurent call has been held and the raw 
         data channel has been swapped out and released.
    */
    virtual PBoolean SetRawDataHeld(PBoolean hold );

   /**On Receive sender report.
        Use this for AV Synchronisation
    */
    virtual PBoolean OnRxSenderReport(DWORD rtpTimeStamp, const PInt64 & realTimeStamp);

   /**Calculate the remote send time
    */
    virtual PTime CalculateRTPSendTime(DWORD timeStamp, unsigned rate) const;

   /**Calculate the remote send time
    */
    void CalculateRTPSendTime(DWORD timeStamp, unsigned rate, PInt64 & sendTime) const;

  protected:
    Direction direction;
    OpalMediaFormat mediaFormat;
    
    H323Channel * logicalChannel; // sends messages from receive codec to tx codec.

    PChannel * rawDataChannel;  // connection to the hardware for reading/writing data.
    PBoolean       deleteChannel;
    PMutex     rawChannelMutex;

    PINDEX     lastSequenceNumber;  // Detects lost RTP packets in the video codec.

	H323_RTPInformation  rtpInformation;
    AVSync  rtpSync;

    H323LIST(FilterList, FilterData);
    FilterList filters;
};

#ifdef H323_AUDIO_CODECS


/**This class defines a codec class that will use the standard platform PCM
   output device.

   An application may create a descendent off this class and override
   functions as required for descibing a specific codec.
 */
class H323Aec;
class H323AudioCodec : public H323Codec
{
  PCLASSINFO(H323AudioCodec, H323Codec);

  public:
    /** Create a new audio codec.
        This opens the standard PCM audio output device, for input and output
        and allows descendent codec classes to do audio I/O after
        decoding/encoding.
      */
    H323AudioCodec(
      const OpalMediaFormat & mediaFormat, ///< Media format for codec
      Direction direction       ///< Direction in which this instance runs
    );

    ~H323AudioCodec();

    /**Open the codec.
       This will open the codec for encoding or decoding. This is primarily
       used to delay allocation of resources until the last minute.

       The default behaviour calls the H323EndPoint::OpenAudioChannel()
       function and assigns the result of that function to the raw data
       channel in the H323Codec class.
      */
    virtual PBoolean Open(
      H323Connection & connection ///< Connection between the endpoints
    );

    /**Close down the codec.
       This will close the codec breaking any block on the Read() or Write()
       functions.

       The default behaviour will close the rawDataChannel if it is not NULL
       and thene delete it if delteChannel is TRUE.
      */
    virtual void Close();

    /**Get the frame rate in RTP timestamp units.
      */
    virtual unsigned GetFrameRate() const;

    /**Get the frame time.
      */
    virtual unsigned GetFrameTime() const;

    enum SilenceDetectionMode {
      NoSilenceDetection,
      FixedSilenceDetection,
      AdaptiveSilenceDetection
    };

    /**Enable/Disable silence detection.
       The deadband periods are in audio samples of 8kHz.
      */
    void SetSilenceDetectionMode(
      SilenceDetectionMode mode,   ///< New silence detection mode
      unsigned threshold = 0,      ///< Threshold value if FixedSilenceDetection
      unsigned signalDeadband = 80,    ///< 10 milliseconds of signal needed
      unsigned silenceDeadband = 3200, ///< 400 milliseconds of silence needed
      unsigned adaptivePeriod = 4800   ///< 600 millisecond window for adaptive threshold
    );

    /**Get silence detection mode

       The inTalkBurst value is TRUE if packet transmission is enabled and
       FALSE if it is being suppressed due to silence.

       The currentThreshold value is the value from 0 to 32767 which is used
       as the threshold value for 16 bit PCM data.
      */
    SilenceDetectionMode GetSilenceDetectionMode(
      PBoolean * isInTalkBurst = NULL,        ///< Current silence detct state.
      unsigned * currentThreshold = NULL  ///< Current signal/silence threshold
    ) const;

    
    /** for codecs which support it, this sets the quality level of the
        transmitted audio.
        In order to have consistency between different codecs, the qlevel
        parameter is defined to range from 1 (good) to 31 (poor), even
        if the individual codec defines fewer levels than this.
     */
    virtual void SetTxQualityLevel(int /*qlevel*/) {}

    /** for codecs which support it, this gets the quality level of the
     * transmitted audio.
     */
    virtual int GetTxQualityLevel(int /*qlevel*/) { return 1; }
    
    /**Check frame for a talk burst.
       This does the deadband calculations on the average signal levels
       returned by the GetAverageSignalLevel() function and based on the
       levelThreshold, signalDeadbandFrames and silenceDeadbandFrames
       member variables.
      */
    virtual PBoolean DetectSilence();

    /**Get the average signal level in the audio stream.
       This is called from within DetectSilence() to calculate the average
       signal level since the last call to DetectSilence().

       The default behaviour returns UINT_MAX which disables the silence
       detection algorithm.
      */
    virtual unsigned GetAverageSignalLevel();

   /**SetRawDataHeld is called when the call has been held and the raw 
      data channel has been swapped out and released for another connection.
      */
    virtual PBoolean SetRawDataHeld(PBoolean hold);

#ifdef H323_AEC	
	/** Attach Acoustic Echo Cancellation.
	*/
	virtual void AttachAEC(
       H323Aec * /*_ARC*/   ///* Acoustic Echo Cancellation Instance
    ) {};
#endif

  protected:
    unsigned samplesPerFrame;

    SilenceDetectionMode silenceDetectMode;

    unsigned signalDeadbandFrames;  // Frames of signal before talk burst starts
    unsigned silenceDeadbandFrames; // Frames of silence before talk burst ends
    unsigned adaptiveThresholdFrames; // Frames to min/max over for adaptive threshold

    PBoolean     inTalkBurst;           // Currently sending RTP data
    unsigned framesReceived;        // Signal/Silence frames received so far.
    unsigned levelThreshold;        // Threshold level for silence/signal
    unsigned signalMinimum;         // Minimum of frames above threshold
    unsigned silenceMaximum;        // Maximum of frames below threshold
    unsigned signalFramesReceived;  // Frames of signal received
    unsigned silenceFramesReceived; // Frames of silence received
    PBoolean	 IsRawDataHeld;
};


/**This class defines a codec class that will use the standard platform PCM
   output device, and the encoding/decoding has fixed blocks. That is each
   input block of n samples is encoded to exactly the same sized compressed
   data, eg G.711, GSM etc.

   An application may create a descendent off this class and override
   functions as required for descibing a specific codec.
 */
class H323Aec;
class H323FramedAudioCodec : public H323AudioCodec
{
  PCLASSINFO(H323FramedAudioCodec, H323AudioCodec);

  public:
    /** Create a new audio codec.
        This opens the standard PCM audio output device, for input and output
        and allows descendent codec classes to do audio I/O after
        decoding/encoding.
      */
    H323FramedAudioCodec(
      const OpalMediaFormat & mediaFormat, ///< Media format for codec
      Direction direction       ///< Direction in which this instance runs
    );

    /**Encode the data from the appropriate device.
       This will encode data for transmission. The exact size and description
       of the data placed in the buffer is codec dependent but should be less
       than OpalMediaFormat::GetFrameSize() in length.

       The length parameter is filled with the actual length of the encoded
       data, often this will be the same as OpalMediaFormat::GetFrameSize().

       This function is called every GetFrameRate() timestamp units, so MUST
       take less than (or equal to) that amount of time to complete. It
       should always return the amount of data that corresponds to the
       GetFrameRate() timestamp units as well.

       A returned length of zero indicates that time has passed but there is
       no data encoded. This is typically used for silence detection in an
       audio codec.
     */
    virtual PBoolean Read(
      BYTE * buffer,            ///< Buffer of encoded data
      unsigned & length,        ///< Actual length of encoded data buffer
      RTP_DataFrame & rtpFrame  ///< RTP data frame
    );

    /**Decode the data and output it to appropriate device.
       This will decode a single frame of received data. The exact size and
       description of the data required in the buffer is codec dependent but
       should be less than H323Capability::GetRxFramesInPacket() *
       OpalMediaFormat::GetFrameSize()  in length.

       It is expected this function anunciates the data. That is, for example
       with audio data, the sound is output on a speaker.

       This function is called every GetFrameRate() timestamp units, so MUST
       take less than that amount of time to complete!
     */
    virtual PBoolean Write(
      const BYTE * buffer,            ///< Buffer of encoded data
      unsigned length,                ///< Length of encoded data buffer
      const RTP_DataFrame & rtpFrame, ///< RTP data frame
      unsigned & written              ///< Number of bytes used from data buffer
    );


    /**Get the average signal level in the audio stream.
       This is called from within DetectSilence() to calculate the average
       signal level since the last call to DetectSilence().
      */
    virtual unsigned GetAverageSignalLevel();


    /**Encode a sample block into the buffer specified.
       The samples have been read and are waiting in the readBuffer member
       variable. it is expected this function will encode exactly
       bytesPerFrame bytes.
     */
    virtual PBoolean EncodeFrame(
      BYTE * buffer,    ///< Buffer into which encoded bytes are placed
      unsigned & length ///< Actual length of encoded data buffer
    ) = 0;

    /**Decode a sample block from the buffer specified.
       The samples must be placed into the writeBuffer member variable. It is
       expected that exactly samplesPerFrame samples is decoded.
     */
    virtual PBoolean DecodeFrame(
      const BYTE * buffer,   ///< Buffer from which encoded data is found
      unsigned length,       ///< Length of encoded data buffer
      unsigned & written,    ///< Number of bytes used from data buffer
      unsigned & bytesOutput ///< Number of bytes in decoded data
    );
    virtual PBoolean DecodeFrame(
      const BYTE * buffer,  ///< Buffer from which encoded data is found
      unsigned length,      ///< Length of encoded data buffer
      unsigned & written    ///< Number of bytes used from data buffer
    );

    /**
      Called when a frame is missed due to late arrival or other reasons
      By default, this function fills the buffer with silence
      */
    virtual void DecodeSilenceFrame(
      void * buffer,  ///< Buffer from which encoded data is found
      unsigned length       ///< Length of encoded data buffer
    )
    { memset(buffer, 0, length); }

#ifdef H323_AEC	
    /** Attach Acoustic Echo Cancellation.
    */
    virtual void AttachAEC(
       H323Aec * _ARC   ///* Acoustic Echo Cancellation Instance
    );
#endif

  protected:
#ifdef H323_AEC	
    H323Aec * aec;     // Acoustic Echo Canceller
#endif
    PShortArray sampleBuffer;
    unsigned    bytesPerFrame;

    PINDEX      readBytes;
    unsigned    writeBytes;
    PINDEX      cntBytes;
};


/**This class defines a codec class that will use the standard platform PCM
   output device, and the encoding/decoding is streamed. That is each input
   16 bit PCM sample is encoded to 8 bits or less of encoded data and no
   blocking of PCM data is required, eg G.711, G.721 etc.

   An application may create a descendent off this class and override
   functions as required for descibing a specific codec.
 */
class H323StreamedAudioCodec : public H323FramedAudioCodec
{
  PCLASSINFO(H323StreamedAudioCodec, H323FramedAudioCodec);

  public:
    /** Create a new audio codec.
        This opens the standard PCM audio output device, for input and output
        and allows descendent codec classes to do audio I/O after
        decoding/encoding.
      */
    H323StreamedAudioCodec(
      const OpalMediaFormat & mediaFormat, ///< Media format for codec
      Direction direction,      ///< Direction in which this instance runs
      unsigned samplesPerFrame, ///< Number of samples in a frame
      unsigned bits             ///< Bits per sample
    );

    /**Encode a sample block into the buffer specified.
       The samples have been read and are waiting in the readBuffer member
       variable. it is expected this function will encode exactly
       encodedBlockSize bytes.
     */
    virtual PBoolean EncodeFrame(
      BYTE * buffer,    ///< Buffer into which encoded bytes are placed
      unsigned & length ///< Actual length of encoded data buffer
    );

    /**Decode a sample block from the buffer specified.
       The samples must be placed into the writeBuffer member variable. It is
       expected that no more than frameSamples is decoded. The return value
       is the number of samples decoded. Zero indicates an error.
     */
    virtual PBoolean DecodeFrame(
      const BYTE * buffer,  ///< Buffer from which encoded data is found
      unsigned length,      ///< Length of encoded data buffer
      unsigned & written,   ///< Number of bytes used from data buffer
      unsigned & samples    ///< Number of sample output from frame
    );

    /**Encode a single sample value.
     */
    virtual int Encode(short sample) const = 0;

    /**Decode a single sample value.
     */
    virtual short Decode(int sample) const = 0;

  protected:
    unsigned bitsPerSample;
};

#endif // NO_H323_AUDIO_CODECS


#ifdef H323_VIDEO

/**This class defines a codec class that will use the standard platform image
   output device.

   An application may create a descendent off this class and override
   functions as required for descibing a specific codec.
 */
class H323VideoCodec : public H323Codec
{
  PCLASSINFO(H323VideoCodec, H323Codec);

  public:
    /** Create a new video codec.
        This opens the standard image output device, for input and output
        and allows descendent codec classes to do video I/O after
        decoding/encoding.
      */
    H323VideoCodec(
      const OpalMediaFormat & mediaFormat, ///< Media format for codec
      Direction direction      ///< Direction in which this instance runs
    );

    ~H323VideoCodec();

    /**Open the codec.
       This will open the codec for encoding or decoding. This is primarily
       used to delay allocation of resources until the last minute.

       The default behaviour calls the H323EndPoint::OpenVideoDevice()
       function and assigns the result of that function to the raw data
       channel in the H323Codec class.
      */
    virtual PBoolean Open(
      H323Connection & connection ///< Connection between the endpoints
    );

    /**Close down the codec.
       This will close the codec breaking any block on the Read() or Write()
       functions.

       The default behaviour will close the rawDataChannel if it is not NULL
       and thene delete it if delteChannel is TRUE.
      */
    virtual void Close();


    /**Process a miscellaneous command on the logical channel.
       The default behaviour does nothing.
     */
    virtual void OnMiscellaneousCommand(
      const H245_MiscellaneousCommand_type & type  ///< Command to process
    );

    /**Process a miscellaneous indication on the logical channel.
       The default behaviour does nothing.
     */
    virtual void OnMiscellaneousIndication(
      const H245_MiscellaneousIndication_type & type  ///< Indication to process
    );

    //    /**Attach the raw data device for use by codec.
    //   Note the device provided will be deleted on destruction of the codec.
    //   */
    // virtual PBoolean AttachDevice(
    //  H323VideoDevice * device, ///< Device to read/write data
    //  PBoolean autoDelete = TRUE    ///< Device is to be automatically deleted
    // );

    /**Process a FreezePicture command from remote endpoint.
       The default behaviour does nothing.
     */
    virtual void OnFreezePicture();

    /**Process a FastUpdatePicture command from remote endpoint.
       The default behaviour does nothing.
     */
    virtual void OnFastUpdatePicture();

    /**Process a FastUpdateGOB command from remote endpoint.
       The default behaviour does nothing.
     */
    virtual void OnFastUpdateGOB(unsigned firstGOB, unsigned numberOfGOBs);

    /**Process a FastUpdateMB command from remote endpoint.
       The default behaviour does nothing.
     */
    virtual void OnFastUpdateMB(int firstGOB, int firstMB, unsigned numberOfMBs);

    /**Process a H.245 videoIndicateReadyToActivate indication from remote endpoint.
       The default behaviour does nothing.
     */
    virtual void OnVideoIndicateReadyToActivate();

    /**Process a H.245 ideoTemporalSpatialTradeOff command from remote endpoint.
       The default behaviour does nothing.
     */
    virtual void OnVideoTemporalSpatialTradeOffCommand(int newQuality);

    /**Process a H.245 videoTemporalSpatialTradeOff indication from remote endpoint.
       The default behaviour does nothing.
     */
    virtual void OnVideoTemporalSpatialTradeOffIndication(int newQuality);

    /**Process a H.245 videoNotDecodedMBs indication from remote endpoint.
       The default behaviour does nothing.
     */
    virtual void OnVideoNotDecodedMBs(
      unsigned firstMB,
      unsigned numberOfMBs,
      unsigned temporalReference
    );

    /**Process a request for a new frame, 
       as part of the picture has been lost.
    */
    virtual void OnLostPartialPicture();

    /**Process a request for a new frame, 
       as the entire picture has been lost.
    */
    virtual void OnLostPicture();

    /** Get width of video
     */ 
    virtual unsigned GetWidth() const { return frameWidth; }

    /** Get height of video
     */ 
    virtual unsigned GetHeight() const { return frameHeight; }

    /** Get width of sample aspect ratio
     */
    virtual unsigned GetSarWidth() const { return sarWidth; }

    /** Get height of sample aspect ratio
     */
    virtual unsigned GetSarHeight() const { return sarHeight; }

    /**Quality of the transmitted video. 1 is good, 31 is poor.
     */
    virtual void SetTxQualityLevel(int qlevel) {videoQuality = qlevel; }

    /**Minimum quality limit for the transmitted video.
     * Default is 1.  Encode quality will not be set below this value.
     */
    virtual void SetTxMinQuality(int qlevel) {videoQMin = qlevel; }

    /**Maximum quality limit for the transmitted video.
     * Default is 24.  Encode quality will not be set above this value.
     */
    virtual void SetTxMaxQuality(int qlevel) {videoQMax = qlevel; }

    /**number of blocks (that haven't changed) transmitted with each 
       frame. These blocks fill in the background */
    virtual void SetBackgroundFill(int idle) {fillLevel= idle; }
 
    enum BitRateModeBits {
      None                = 0x00,
      DynamicVideoQuality = 0x01,
      AdaptivePacketDelay = 0x02
    };

    /**Get the current value for video control mode
     */
    virtual unsigned GetVideoMode(void) {return videoBitRateControlModes;}

    /**Set the current value for video control mode
     * return the resulting value video control mode
     */
    virtual void SetVideoMode(int mode) {videoBitRateControlModes = mode;}

    /**Set maximum bitrate when transmitting video, in bps. A value of 0
       disables bit rate control. The average bitrate will be less depending
       on channel dead time, i.e. time that the channel could be transmitting
       bits but is not.

       @return TRUE if success
    */
    virtual PBoolean SetMaxBitRate(
      unsigned bitRate ///< New bit rate
    );

    /**Get the current value of the maximum bitrate, in bps. If SetMaxBitRate
       was never called, the return value depends on the derived class
       implementation.
    */
    virtual unsigned GetMaxBitRate() const { return bitRateHighLimit; }

    /**Set target time in milliseconds between video frames going through
       the channel.  This sets the video frame rate through the channel,
       which is <= grabber frame rate.  Encoder quality will be adjusted
       dynamically by the codec to find a frame size that allows sending
       at this rate.  Default = 167 ms = 6 frames per second.  A value of 0
       means the channel will attempt to run at the video grabber frame rate
       Sometimes the channel cannot transmit as fast as the video grabber.
    */
    virtual PBoolean SetTargetFrameTimeMs(
      unsigned ms ///< new time between frames
    );

    /**
       Set whether is preference frame rate over video quality.
       This will emphasize speed over quality for a given bitrate.
       With Speed the video frame size may be smaller with a higher frame rate (>20)
       With Quality the video frame size is larger with a lower frame rate (>10)
    */
    virtual void SetEmphasisSpeed(bool speed);

    /**
       Set the maximum allowable MTU Size of the RTP Frame
    */
    virtual void SetMaxPayloadSize(int maxSize);

    /**
       Set a miscellaneous option setting in the video codec.
       This message is used for Video Plugin Codecs.
    */
    virtual void SetGeneralCodecOption(
		const char * opt,    ///< Option string to set
		int val              ///< New Value to set to
	);

    /**
       Send a miscellaneous command to the remote transmitting video codec.
       This message is sent via the H245 Logical Channel.
    */
    void SendMiscCommand(unsigned command);
 
   /** 
       Returns the number of frames transmitted or received so far. 
   */
    virtual int GetFrameNum() { return frameNum; }

   /**
      Set the supported Formats the codec is to support
      Return whether codec supports setting Supported Formats.
    */
    virtual PBoolean SetSupportedFormats(std::list<PVideoFrameInfo> & info);

  protected:

    int frameWidth;
    int frameHeight;
    int fillLevel;
    int sarWidth;
    int sarHeight;

    // used in h261codec.cxx
    unsigned videoBitRateControlModes;
    // variables used for video bit rate control
    int bitRateHighLimit; // maximum instantaneous bit rate allowed
    unsigned oldLength;
    PTimeInterval oldTime;
    PTimeInterval newTime;
    // variables used for dynamic video quality control
    int targetFrameTimeMs; //targetFrameTimeMs = 1000 / videoSendFPS
    int frameBytes; // accumulate count of bytes per frame
    int sumFrameTimeMs, sumAdjFrameTimeMs, sumFrameBytes; // accumulate running average
    int videoQMax, videoQMin; // dynamic video quality min/max limits
    int videoQuality; // current video encode quality setting, 1..31
    PTimeInterval frameStartTime;
    PTimeInterval grabInterval;
    
    int frameNum, packetNum, oldPacketNum;
    int framesPerSec;

    PMutex  videoHandlerActive;    
};

#endif // NO_H323_VIDEO

#ifdef H323_AUDIO_CODECS

///////////////////////////////////////////////////////////////////////////////
// The simplest codec is the G.711 PCM codec.

/**This class is a G711 ALaw codec.
 */
class H323_ALawCodec : public H323StreamedAudioCodec
{
  PCLASSINFO(H323_ALawCodec, H323StreamedAudioCodec)

  public:
  /**@name Construction */
  //@{
    /**Create a new G.711 codec for ALaw.
     */
    H323_ALawCodec(
      Direction direction,  ///< Direction in which this instance runs
      PBoolean at56kbps,        ///< Encoding bit rate.
      unsigned frameSize    ///< Size of frame in bytes
    );
  //@}

    virtual int   Encode(short sample) const { return EncodeSample(sample); }
    virtual short Decode(int   sample) const { return DecodeSample(sample); }

    static int   EncodeSample(short sample);
    static short DecodeSample(int   sample);

  protected:
    PBoolean sevenBit;
};


/**This class is a G711 uLaw codec.
 */
class H323_muLawCodec : public H323StreamedAudioCodec
{
  PCLASSINFO(H323_muLawCodec, H323StreamedAudioCodec)

  public:
  /**@name Construction */
  //@{
    /**Create a new G.711 codec for muLaw.
     */
    H323_muLawCodec(
      Direction direction,  ///< Direction in which this instance runs
      PBoolean at56kbps,        ///< Encoding bit rate.
      unsigned frameSize    ///< Size of frame in bytes
    );
  //@}

    virtual int   Encode(short sample) const { return EncodeSample(sample); }
    virtual short Decode(int   sample) const { return DecodeSample(sample); }

    static int   EncodeSample(short sample);
    static short DecodeSample(int   sample);

  protected:
    PBoolean sevenBit;
};

#endif // NO_H323_AUDIO_CODECS


#endif // __CODECS_H


/////////////////////////////////////////////////////////////////////////////
