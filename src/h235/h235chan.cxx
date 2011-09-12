/*
 * h235chan.cxx
 *
 * H.235 Secure RTP channel class.
 *
 * h323plus library
 *
 * Copyright (c) 2011 Spranto Australia Pty Ltd.
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
 *
 * The Initial Developer of the Original Code is ISVO (Asia) Pte. Ltd.
 *
 *
 * Contributor(s): ______________________________________.
 *
 * $Id$
 *
 */

#include <ptlib.h>
#include "openh323buildopts.h"

#ifdef H323_H235

#include "h235/h235chan.h"
#include <h323rtp.h>
#include <h323con.h>

H323SecureRTPChannel::H323SecureRTPChannel(H323Connection & conn,
                                 const H323SecureRealTimeCapability & cap,
                                 Directions direction,
                                 RTP_Session & r
								 )
    : H323_RTPChannel(conn,cap,direction, r) 
{	
}

H323SecureRTPChannel::~H323SecureRTPChannel()
{
}

void H323SecureRTPChannel::CleanUpOnTermination()
{
  if (terminating)
    return;

//****<- Put Encryption Stuff Here

//****<-

  return H323_RTPChannel::CleanUpOnTermination();
}

PBoolean H323SecureRTPChannel::OnSendingPDU(H245_OpenLogicalChannel & open) const
{

  PTRACE(4, "H235RTP\tOnSendingPDU");

    if (connection.IsH245Master()) {
    /*
         open.IncludeOptionalField(H245_OpenLogicalChannel::e_encryptionSync);
           H245_EncryptionSync & sync = open;
              sync.m_synchFlag;
              sync.m_encryptionSync;
    */
    }

  return H323_RealTimeChannel::OnSendingPDU(open);
}


void H323SecureRTPChannel::OnSendOpenAck(const H245_OpenLogicalChannel & open,
                                         H245_OpenLogicalChannelAck & ack) const
{
  PTRACE(4, "H235RTP\tOnSendOpenAck");

    if (connection.IsH245Master()) {
    /*
         open.IncludeOptionalField(H245_OpenLogicalChannel::e_encryptionSync);
           H245_EncryptionSync & sync = open;
              sync.m_synchFlag;
              sync.m_encryptionSync;
    */
    }

  return H323_RealTimeChannel::OnSendOpenAck(open,ack);
}


PBoolean H323SecureRTPChannel::OnReceivedPDU(const H245_OpenLogicalChannel & open,
                                         unsigned & errorCode)
{
   PTRACE(4, "H235RTP\tOnRecievedPDU");

/*
  if (open.HasOptionalField(H245_OpenLogicalChannel::e_encryptionSync)) {
      const H245_EncryptionSync & sync = open;
          sync.m_synchFlag;
          sync.m_encryptionSync;
  }
*/
   return H323_RealTimeChannel::OnReceivedPDU(open,errorCode);
}


PBoolean H323SecureRTPChannel::OnReceivedAckPDU(const H245_OpenLogicalChannelAck & ack)
{
  PTRACE(3, "H235RTP\tOnReceiveOpenAck");

/*
  if (open.HasOptionalField(H245_OpenLogicalChannel::e_encryptionSync)) {
      const H245_EncryptionSync & sync = open;
          sync.m_synchFlag;
          sync.m_encryptionSync;
  }
*/
  return H323_RealTimeChannel::OnReceivedAckPDU(ack);
}

PBoolean H323SecureRTPChannel::OnSendingPDU(H245_H2250LogicalChannelParameters & param) const
{
  return rtpCallbacks.OnSendingPDU(*this, param);
}


void H323SecureRTPChannel::OnSendOpenAck(H245_H2250LogicalChannelAckParameters & param) const
{
  rtpCallbacks.OnSendingAckPDU(*this, param);
}


PBoolean H323SecureRTPChannel::OnReceivedPDU(const H245_H2250LogicalChannelParameters & param,
                                             unsigned & errorCode)
{
  return rtpCallbacks.OnReceivedPDU(*this, param, errorCode);
}


PBoolean H323SecureRTPChannel::OnReceivedAckPDU(const H245_H2250LogicalChannelAckParameters & param)
{
  return rtpCallbacks.OnReceivedAckPDU(*this, param);
}


PBoolean H323SecureRTPChannel::ReadFrame(DWORD & rtpTimestamp, RTP_DataFrame & frame)
{
	if (H323_RTPChannel::ReadFrame(rtpTimestamp, frame)) {

//****<- Put Encryption Stuff Here

//****<-
		return TRUE;
	} else 
		return FALSE;
}

PBoolean H323SecureRTPChannel::WriteFrame(RTP_DataFrame & frame) 
{

//****<- Put Encryption Stuff Here

//****<-
    return H323_RTPChannel::WriteFrame(frame);
}


#endif   // H323_H235


