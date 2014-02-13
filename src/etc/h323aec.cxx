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

H323AEC::BufferFrame::BufferFrame() 
: receiveTime(0), echoTime(0), frame(0)
{
}

H323AEC::BufferFrame::BufferFrame(PINDEX sz) 
: receiveTime(0), echoTime(0), frame(sz)
{
    memset(frame.GetPointer(),0,sz);
}

H323AEC::BufferFrame::~BufferFrame()
{
    frame.SetSize(0);
}

H323_AECBuffer::H323_AECBuffer()
: m_bufferTime(0), m_curPos(0)
{

}
    
H323_AECBuffer::~H323_AECBuffer()
{
 
}

void H323_AECBuffer::Initialise(PINDEX size, PINDEX byteSize, PINDEX clockRate)
{
    PWaitAndSignal m(m_bufferMutex);

    m_bufferTime = size * (byteSize/(clockRate/1000)/2);

    for (PINDEX i=0; i < size; ++i)
        (*this)[i] = *new H323AEC::BufferFrame(byteSize);
}

void H323_AECBuffer::ShutDown()
{
    PWaitAndSignal m(m_bufferMutex);

    if (this->empty()) 
        return;

    std::map<unsigned, H323AEC::BufferFrame>::iterator it;
    for (it= this->begin(); it!= this->end(); ++it)
        this->erase(it++);
}

PBoolean H323_AECBuffer::Send(BYTE * buffer, unsigned & length)
{
    PWaitAndSignal m(m_bufferMutex);

    if (this->empty()) {
        PTRACE(6,"AEC\tEmpty!");
        return false;
    }

    const H323AEC::BufferFrame & entry = (*this)[m_curPos];

    if (entry.receiveTime == 0) {
        PTRACE(6,"AEC\tFilling AEC Buffer");
        return false;
    }

    if (length != (unsigned)entry.frame.GetSize()) {
        PTRACE(3,"AEC\tSend buffer size " << length << " does not match receive " << entry.frame.GetSize());
        return false;
    }

    memcpy(buffer, (const void *)entry.frame, length);
    PTRACE(6,"AEC\tPlay Pos " << m_curPos << " " << entry.receiveTime << " " << PTimer::Tick().GetMilliSeconds() - entry.echoTime);
    return true;
}

void H323_AECBuffer::Receive(BYTE * buffer, unsigned & length)
{
    PWaitAndSignal m(m_bufferMutex);

    if (this->empty()) 
        return;

    H323AEC::BufferFrame & entry = (*this)[m_curPos];
    entry.receiveTime = PTimer::Tick().GetMilliSeconds();
    entry.echoTime = entry.receiveTime + m_bufferTime;
    memcpy(entry.frame.GetPointer(), buffer, length);

    m_curPos++;
    if (m_curPos >= this->size())
        m_curPos = 0;

#if 0  // debugging
    PTRACE(6,"AEC\tPosition " << m_curPos);
    std::map<unsigned, H323AEC::BufferFrame>::const_iterator r = this->begin();
    for (r = this->begin(); r != this->end(); ++r)
        PTRACE(6,"AEC\t" << r->first << " " << r->second.receiveTime);
#endif

}


///////////////////////////////////////////////////////////////////////////////
#define TAIL 5

H323Aec::H323Aec(int _clock, int _sampletime, int _buffers)
  :  m_echoState(NULL), m_preprocessState(NULL), m_clockrate(_clock), m_samplesFrame(_sampletime*(m_clockrate/1000)), m_BufferBytes(2*m_samplesFrame),
     m_echo_buf((spx_int16_t *)malloc(m_BufferBytes)), m_ref_buf((spx_int16_t *)malloc(m_BufferBytes)), m_temp_buf((spx_int16_t *)malloc(m_BufferBytes)),
     m_tail(TAIL * m_samplesFrame)
{

    m_buffer.Initialise( _buffers, m_BufferBytes, _clock);

    m_echoState = speex_echo_state_init(m_samplesFrame, m_tail);
    speex_echo_ctl(m_echoState, SPEEX_ECHO_SET_SAMPLING_RATE, &m_clockrate);

    m_preprocessState = speex_preprocess_state_init(m_samplesFrame, m_clockrate);
    speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_ECHO_STATE, m_echoState);
       int i=1;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_DENOISE, &i);
       i=0;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_AGC, &i);
       i=0;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_AGC_LEVEL, &i);
       i=0;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_DEREVERB, &i);
       float f=.0;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &f);
       f=.0;
       speex_preprocess_ctl(m_preprocessState, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &f);

    PTRACE(3, "AEC\tcreated AEC " << m_clockrate << " hz " << " buffer Size " << _buffers );
}


H323Aec::~H323Aec()
{
    m_buffer.ShutDown();

    free(m_echo_buf);
    free(m_ref_buf);
    free(m_temp_buf);

    if (m_echoState) {
        speex_echo_state_destroy(m_echoState);
        m_echoState = NULL;
    }

    if (m_preprocessState) {
        speex_preprocess_state_destroy(m_preprocessState);
        m_preprocessState = NULL;
    }
}

void H323Aec::Receive(BYTE * buffer, unsigned & length)
{
  if (length == 0)
	  return;

  m_buffer.Receive(buffer,length);

}

void H323Aec::Send(BYTE * buffer, unsigned & length)
{

  if (!m_buffer.Send((BYTE *)m_echo_buf,length))
      return;

  memcpy(m_ref_buf,buffer,length);

  speex_echo_cancellation(m_echoState, m_ref_buf,
                          m_echo_buf, m_temp_buf);

  speex_preprocess_run(m_preprocessState, m_temp_buf);

  memcpy(buffer,m_temp_buf,length);

}

#endif // H323_AEC


