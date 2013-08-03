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
#include "h235/h235crypto.h"
#include <h323rtp.h>
#include <h323con.h>

#ifdef H323_H235_AES256
const char * STR_AES256 = "AES256";
#endif
const char * STR_AES192 = "AES192";
const char * STR_AES128 = "AES128";

PString CipherString(const PString & m_algorithmOID) 
{
    if (m_algorithmOID == "2.16.840.1.101.3.4.1.2") {
        return STR_AES128;
    } else if (m_algorithmOID == "2.16.840.1.101.3.4.1.22") {
        return STR_AES192;
#ifdef H323_H235_AES256
    } else if (m_algorithmOID == "2.16.840.1.101.3.4.1.42") {
        return STR_AES256;
#endif
    }
    return "Unknown";
}

H323SecureRTPChannel::H323SecureRTPChannel(H323Connection & conn,
                                 const H323SecureRealTimeCapability & cap,
                                 Directions direction,
                                 RTP_Session & r
								 )
    : H323_RTPChannel(conn,cap,direction, r), m_algorithm(cap.GetAlgorithm()),
      m_encryption(new H235Session((H235Capabilities*)conn.GetLocalCapabilitiesRef(), cap.GetAlgorithm())),
      m_payload(RTP_DataFrame::IllegalPayloadType)
{	
}

H323SecureRTPChannel::~H323SecureRTPChannel()
{
    delete m_encryption;
}

void H323SecureRTPChannel::CleanUpOnTermination()
{
  if (terminating)
    return;

  return H323_RTPChannel::CleanUpOnTermination();
}

void BuildEncryptionSync(H245_EncryptionSync & sync, const H323SecureRTPChannel & chan, H235Session & session)
{     
    sync.m_synchFlag = chan.GetRTPPayloadType();

    PBYTEArray encryptedMediaKey;
    session.EncodeMediaKey(encryptedMediaKey);
    H235_H235Key h235key;
    h235key.SetTag(H235_H235Key::e_secureSharedSecret);
    H235_V3KeySyncMaterial & v3data = h235key;
    v3data.IncludeOptionalField(H235_V3KeySyncMaterial::e_algorithmOID);
    v3data.m_algorithmOID = session.GetAlgorithmOID();
    v3data.IncludeOptionalField(H235_V3KeySyncMaterial::e_encryptedSessionKey);
    v3data.m_encryptedSessionKey = encryptedMediaKey;

    sync.m_h235Key.EncodeSubType(h235key);
}


PBoolean ReadEncryptionSync(const H245_EncryptionSync & sync, H323SecureRTPChannel & chan, H235Session & session)
{
    H235_H235Key h235key;
    sync.m_h235Key.DecodeSubType(h235key);

    chan.SetDynamicRTPPayloadType(sync.m_synchFlag);

    switch (h235key.GetTag()) {
        case H235_H235Key::e_secureChannel:
            PTRACE(4,"H235Key\tSecureChannel not supported");
            return false;
        case H235_H235Key::e_secureChannelExt:
            PTRACE(4,"H235Key\tSecureChannelExt not supported");
            return false;
        case H235_H235Key::e_sharedSecret:
            PTRACE(4,"H235Key\tShared Secret not supported");
            return false;
        case H235_H235Key::e_certProtectedKey:
            PTRACE(4,"H235Key\tProtected Key not supported");
            return false;
        case H235_H235Key::e_secureSharedSecret:
            {
                const H235_V3KeySyncMaterial & v3data = h235key;
                if (!v3data.HasOptionalField(H235_V3KeySyncMaterial::e_algorithmOID)) {
                    // the algo is required, but is really redundant
                    PTRACE(3, "H235\tWarning: No algo set in encryptionSync");
                }
                if (v3data.HasOptionalField(H235_V3KeySyncMaterial::e_encryptedSessionKey)) {
                  PBYTEArray mediaKey = v3data.m_encryptedSessionKey;
                  return session.DecodeMediaKey(mediaKey);
                }
            }
    } 
    return false;
}


PBoolean H323SecureRTPChannel::OnSendingPDU(H245_OpenLogicalChannel & open) const
{
  PTRACE(4, "H235RTP\tOnSendingPDU");

  if (H323_RealTimeChannel::OnSendingPDU(open)) {
       if (connection.IsH245Master() && m_encryption) {
            if (m_encryption->CreateSession(true)) {
                open.IncludeOptionalField(H245_OpenLogicalChannel::e_encryptionSync);
                BuildEncryptionSync(open.m_encryptionSync,*this,*m_encryption);
            }
        }
        connection.OnMediaEncryption(GetSessionID(), GetDirection(), CipherString(m_algorithm));
       return true;
  }
  return false;
}


void H323SecureRTPChannel::OnSendOpenAck(const H245_OpenLogicalChannel & open,
                                         H245_OpenLogicalChannelAck & ack) const
{
  PTRACE(4, "H235RTP\tOnSendOpenAck");

  H323_RealTimeChannel::OnSendOpenAck(open,ack);

  if (connection.IsH245Master() && m_encryption) {
        if (m_encryption->CreateSession(true)) {
            ack.IncludeOptionalField(H245_OpenLogicalChannelAck::e_encryptionSync);
            BuildEncryptionSync(ack.m_encryptionSync,*this,*m_encryption);
            connection.OnMediaEncryption(GetSessionID(), GetDirection(), CipherString(m_algorithm));
        }
  }

}


PBoolean H323SecureRTPChannel::OnReceivedPDU(const H245_OpenLogicalChannel & open,
                                         unsigned & errorCode)
{
   PTRACE(4, "H235RTP\tOnRecievedPDU");

   if (!H323_RealTimeChannel::OnReceivedPDU(open,errorCode)) 
       return false;

   if (open.HasOptionalField(H245_OpenLogicalChannel::e_encryptionSync)) {
       if (m_encryption->CreateSession(false)) {
           connection.OnMediaEncryption(GetSessionID(), GetDirection(), CipherString(m_algorithm));
           return ReadEncryptionSync(open.m_encryptionSync,*this,*m_encryption);
       }
   } 
   return true;
}


PBoolean H323SecureRTPChannel::OnReceivedAckPDU(const H245_OpenLogicalChannelAck & ack)
{
  PTRACE(3, "H235RTP\tOnReceiveOpenAck");

  if (!H323_RealTimeChannel::OnReceivedAckPDU(ack))
      return false;

  if (ack.HasOptionalField(H245_OpenLogicalChannelAck::e_encryptionSync)) {
      if (m_encryption->CreateSession(false)) {
            connection.OnMediaEncryption(GetSessionID(), GetDirection(), CipherString(m_algorithm));
            return ReadEncryptionSync(ack.m_encryptionSync,*this,*m_encryption);
      }
  }
  return true;
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
	if (rtpSession.ReadBufferedData(rtpTimestamp, frame)) {
		if (m_encryption && m_encryption->IsInitialised() && frame.GetPayloadSize() > 0)
           return m_encryption->ReadFrame(rtpTimestamp,frame);
        else
		   return true;
	} else 
		return false;
}


PBoolean H323SecureRTPChannel::WriteFrame(RTP_DataFrame & frame) 
{
    if (!rtpSession.PreWriteData(frame))
        return false;

    if (m_encryption && m_encryption->IsInitialised()) {
        if (m_encryption->WriteFrame(frame))
            return rtpSession.WriteData(frame);
        else
            return true;
    }
    return rtpSession.WriteData(frame);
}

RTP_DataFrame::PayloadTypes H323SecureRTPChannel::GetRTPPayloadType() const
{
    int tempPayload=0;
    if (m_payload == RTP_DataFrame::IllegalPayloadType) {
        int baseType = H323_RealTimeChannel::GetRTPPayloadType();
        if (baseType >= RTP_DataFrame::DynamicBase) 
            tempPayload = baseType;
        else if (m_encryption)
            tempPayload = 120 + capability->GetMainType();
        else
            tempPayload = baseType;
      
      PRemoveConst(H323SecureRTPChannel, this)->SetDynamicRTPPayloadType(tempPayload);
    }
    return (RTP_DataFrame::PayloadTypes)m_payload;
}

PBoolean H323SecureRTPChannel::SetDynamicRTPPayloadType(int newType)
{
    if (m_payload == newType)
        return true;

    if (m_payload != RTP_DataFrame::IllegalPayloadType) {
        PTRACE(1,"WARNING: Change Payload " << GetSessionID() << " " << 
                 (GetDirection() == IsReceiver ? "Receive" : "Transmit") << 
                  " to " << newType << " from " << m_payload);
    }

    m_payload = newType;
    return true;
}


#endif   // H323_H235


