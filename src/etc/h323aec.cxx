/*
 * h323_aec.cxx
 *
 * Acoustic Echo Cancellation for the h323plus Library.
 *
 * Copyright (c) 2012 Spranto Australia Pty Ltd. All Rights Reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the General Public License (the  "GNU License"), in which case the
 * provisions of GNU License are applicable instead of those
 * above. If you wish to allow use of your version of this file only
 * under the terms of the GNU License and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GNU License. If you do not delete
 * the provisions above, a recipient may use your version of this file
 * under either the MPL or the GNU License."
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is derived from and used in conjunction with the 
 * H323plus Project (www.h323plus.org/)
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_AEC
#include "etc/h323aec.h"

#if _WIN32
#pragma comment(lib, H323_AEC_LIB)
#endif

///////////////////////////////////////////////////////////////////////////////
#define TAIL 5

H323Aec::H323Aec(int _clock, int _sampletime, int _buffers)
  :  m_echoState(NULL), m_preprocessState(NULL), m_clockrate(_clock),
     m_bufferTime(m_sampleTime*_buffers-1), m_bufferSize(_buffers),
     m_sampleTime(_sampletime), m_BufferBytes(m_sampleTime*(m_clockrate/1000)),
     m_tail(TAIL * m_BufferBytes), m_playRecDiff(0)
{
     PTRACE(3, "AEC\tcreated AEC " << m_clockrate << " hz " << " buffer Size " << m_bufferTime << " ms." );
}


H323Aec::~H323Aec()
{
  PWaitAndSignal m(readwritemute);

  if (m_echoState) {
    speex_echo_state_destroy(m_echoState);
    m_echoState = NULL;
  }
  
  if (m_preprocessState) {
    speex_preprocess_state_destroy(m_preprocessState);
    m_preprocessState = NULL;
  }

  m_echoBuffer.empty();
}


void H323Aec::Receive(BYTE * buffer, unsigned & length)
{
  if (length == 0)
	  return;

  // prepare the BufferEntry
  BufferFrame entry;
  entry.receiveTime = PTimer::Tick().GetMilliSeconds();
  entry.echoTime = entry.receiveTime + m_bufferTime;
  entry.frame.SetSize(length);
  memcpy(entry.frame.GetPointer(),buffer,length);

readwritemute.Wait();
  m_echoBuffer.push(entry);

  if (m_echoBuffer.size() > m_bufferSize) {
      PTRACE(5, "AEC\tRead Buffer full dropping frame!");
      m_echoBuffer.pop();
  }
readwritemute.Signal();

}

void H323Aec::Send(BYTE * buffer, unsigned & length)
{
  short echo_buf[320], ref_buf[320], e_buf[320];

// Inialise the Echo Canceller
  if (m_echoState == NULL) {
    m_echoState = speex_echo_state_init(m_BufferBytes, m_tail);
    m_preprocessState = speex_preprocess_state_init(m_BufferBytes, m_clockrate);
    speex_echo_ctl(m_echoState, SPEEX_ECHO_SET_SAMPLING_RATE, &m_clockrate);
    speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_ECHO_STATE, m_echoState);

       int i=1;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_DENOISE, &i);
       i=0;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_AGC, &i);
       i=m_clockrate;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_AGC_LEVEL, &i);
       i=0;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_DEREVERB, &i);
       float f=.0;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &f);
       f=.0;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &f);
  
  }

  if (m_echoBuffer.size() == 0)
	  return;

  PInt64 m_recTimeMax = PTimer::Tick().GetMilliSeconds();
  PInt64 m_recTimeMin = m_recTimeMax - m_sampleTime;
  
  BufferFrame l_frame;
  readwritemute.Wait();
      l_frame = m_echoBuffer.front();
      m_playRecDiff = m_recTimeMax - l_frame.echoTime;

      while (m_playRecDiff > m_sampleTime && m_echoBuffer.size() > 0) {
           m_echoBuffer.pop();
           PTRACE(5, "AEC\tBuffer dropped too old " << m_playRecDiff << "Ms");
           if (m_echoBuffer.size() > 0) {
                l_frame = m_echoBuffer.front();
                m_playRecDiff = l_frame.echoTime - m_recTimeMax;
           }
      }

      if (l_frame.echoTime < m_recTimeMin || m_echoBuffer.size() == 0) {
            PTRACE(5, "AEC\tFilling Buffer " << m_echoBuffer.size() << " Delta " << m_playRecDiff);
            readwritemute.Signal();
            return;
      }
   
      PTRACE(6,"AEC\tPlay Delta " << m_playRecDiff << " sz: " << m_echoBuffer.size());
      memcpy(echo_buf, l_frame.frame.GetPointer(), l_frame.frame.GetSize());
      m_echoBuffer.pop();

  readwritemute.Signal();

  memcpy(ref_buf, buffer, length);

  // Cancel the Echo/Noise in this frame 
  speex_echo_playback(m_echoState,echo_buf);
  speex_echo_capture(m_echoState,ref_buf, e_buf);
  speex_preprocess_run(m_preprocessState, e_buf);

  // Use the result of the echo cancelation as capture frame 
  memcpy(buffer, e_buf, length);

}
#endif // H323_AEC


