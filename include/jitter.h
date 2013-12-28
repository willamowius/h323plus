/*
 * jitter.h
 *
 * Jitter buffer support
 *
 * Open H323 Library
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
 * Portions of this code were written with the assisance of funding from
 * Vovida Networks, Inc. http://www.vovida.com.
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#ifndef __OPAL_JITTER_H
#define __OPAL_JITTER_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif


#include "rtp.h"

class RTP_JitterBufferAnalyser;
class RTP_AggregatedHandle;

///////////////////////////////////////////////////////////////////////////////

class RTP_JitterBuffer : public PObject
{
  PCLASSINFO(RTP_JitterBuffer, PObject);

  public:
    friend class RTP_AggregatedHandle;

    RTP_JitterBuffer(
      RTP_Session & session,   ///<  Associated RTP session tor ead data from
      unsigned minJitterDelay, ///<  Minimum delay in RTP timestamp units
      unsigned maxJitterDelay, ///<  Maximum delay in RTP timestamp units
      PINDEX stackSize = 30000 ///<  Stack size for jitter thread
    );
    ~RTP_JitterBuffer();

//    PINDEX GetSize() const { return bufferSize; }
    /**Set the maximum delay the jitter buffer will operate to.
      */
    void SetDelay(
      unsigned minJitterDelay, ///<  Minimum delay in RTP timestamp units
      unsigned maxJitterDelay  ///<  Maximum delay in RTP timestamp units
    );

    void UseImmediateReduction(PBoolean state) { doJitterReductionImmediately = state; }

    /**Reset Firt write
		This is used when redirecting media flows to ensure Jitter buffer is not exceeded.
      */
	void ResetFirstWrite();

    /**Read a data frame from the RTP channel.
       Any control frames received are dispatched to callbacks and are not
       returned by this function. It will block until a data frame is
       available or an error occurs.
      */
    virtual PBoolean ReadData(
      DWORD timestamp,        ///<  Timestamp to read from buffer.
      RTP_DataFrame & frame   ///<  Frame read from the RTP session
    );

    /**Get current delay for jitter buffer.
      */
    DWORD GetJitterTime() const { return currentJitterTime; }

    /**Get total number received packets too late to go into jitter buffer.
      */
    DWORD GetPacketsTooLate() const { return packetsTooLate; }

    /**Get total number received packets that overran the jitter buffer.
      */
    DWORD GetBufferOverruns() const { return bufferOverruns; }

    /**Get maximum consecutive marker bits before buffer starts to ignore them.
      */
    DWORD GetMaxConsecutiveMarkerBits() const { return maxConsecutiveMarkerBits; }

    /**Set maximum consecutive marker bits before buffer starts to ignore them.
      */
    void SetMaxConsecutiveMarkerBits(DWORD max) { maxConsecutiveMarkerBits = max; }

    /**Start seperate jitter thread
      */
    void Resume(
#ifdef H323_RTP_AGGREGATE
      PHandleAggregator * aggregator
#endif
      );

    PDECLARE_NOTIFIER(PThread, RTP_JitterBuffer, JitterThreadMain);

  protected:
    //virtual void Main();

    class Entry : public RTP_DataFrame
    {
      public:
        Entry * next;
        Entry * prev;
        PTimeInterval tick;
    };

    RTP_Session & session;
    PINDEX        bufferSize;
    DWORD         minJitterTime;
    DWORD         maxJitterTime;
    DWORD         maxConsecutiveMarkerBits;

    unsigned currentDepth;
    DWORD    currentJitterTime;
    DWORD    packetsTooLate;
    unsigned bufferOverruns;
    unsigned consecutiveBufferOverruns;
    DWORD    consecutiveMarkerBits;
    PTimeInterval    consecutiveEarlyPacketStartTime;
    DWORD    lastWriteTimestamp;
    PTimeInterval lastWriteTick;
    DWORD    jitterCalc;
    DWORD    targetJitterTime;
    unsigned jitterCalcPacketCount;
    PBoolean     doJitterReductionImmediately;
    PBoolean     doneFreeTrash;

    Entry * oldestFrame;
    Entry * newestFrame;
    Entry * freeFrames;
    Entry * currentWriteFrame;

    PMutex bufferMutex;
    PBoolean   shuttingDown;
    PBoolean   preBuffering;
    PBoolean   doneFirstWrite;

    RTP_JitterBufferAnalyser * analyser;

    PThread * jitterThread;
    PINDEX    jitterStackSize;

#ifdef H323_RTP_AGGREGATE
    RTP_AggregatedHandle * aggregratedHandle;
#endif

    PBoolean Init(Entry * & currentReadFrame, PBoolean & markerWarning);
    PBoolean PreRead(Entry * & currentReadFrame, PBoolean & markerWarning);
    PBoolean OnRead(Entry * & currentReadFrame, PBoolean & markerWarning, PBoolean loop);
    void DeInit(Entry * & currentReadFrame, PBoolean & markerWarning);
};

#endif // __OPAL_JITTER_H


/////////////////////////////////////////////////////////////////////////////
