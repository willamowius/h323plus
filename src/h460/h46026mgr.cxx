/* h46026mgr.cxx
 *
 * Copyright (c) 2013 Spranto Australia Pty Ltd. All Rights Reserved.
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
 * H323Plus Project (www.h323plus.org/)
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
#include <openh323buildopts.h>

#ifdef H323_H46026

#include <h323pdu.h>
#include "h460/h46026mgr.h"

//-------------------------------------------
#define MAX_AUDIO_FRAMES     3
#define MAX_VIDEO_PAYLOAD    4000         // 4k Frame
#define REC_FRAME_RATE       15.0
#define REC_FRAME_TIME       (1.0/REC_FRAME_RATE) * 1000
#define MAX_STACK_DESCRETION  REC_FRAME_TIME * 2
#define FAST_UPDATE_INTERVAL  REC_FRAME_TIME * 3
#define MAX_VIDEO_KBPS       384000.0


#define PACKETDELAY(sz,mbps) \
      int((double(sz) / (mbps *0.5)) * 1000.0);

//-------------------------------------------

const unsigned H46026_ProtocolID[] = { 0,0,8,2250,0,H225_PROTOCOL_VERSION };

H225_CallIdentifier StringToCallId(PString CallId)
{
	H225_CallIdentifier result;
	CallId.Replace("-", "", true);
	CallId.Replace(" ", "", true);
	OpalGloballyUniqueID tmp_guid(CallId);
	result.m_guid = tmp_guid;
	return result;
}

PString CallIdToString(const H225_GloballyUniqueID& id)
{
	if (id.GetSize() < 16)
		return "Invalid";
		
	PString idstr;
					
	for (int j = 0, i = 0; j < 4; j++) {
		const unsigned hex = ((unsigned)(id[i])<<24) | ((unsigned)(id[i+1])<<16) 
			| ((unsigned)(id[i+2])<<8) | ((unsigned)(id[i+3]));
		i += 4;

		idstr += PString(PString::Unsigned, (long)hex, 16);
		if (j < 3)
			idstr += ' ';
	}

	return idstr;
}

bool GetInfoUUIE(const Q931 & q931, H225_H323_UserInformation & uuie)
{
	if (q931.HasIE(Q931::UserUserIE)) {
		PPER_Stream strm(q931.GetIE(Q931::UserUserIE));
		if (uuie.Decode(strm))
			return true;
	}
	return false;
}

void SetInfoUUIE(Q931 & q931, const H225_H323_UserInformation & uuie)
{
	PPER_Stream strm;
	uuie.Encode(strm);
	strm.CompleteEncoding();
	q931.SetIE(Q931::UserUserIE, strm);
}

void BuildRTPFrame(Q931 & q931, H225_H323_UserInformation & uuie, const PString & callId, H46026_UDPFrame & data)
{
    // Set the RTP Payload
    PASN_OctetString & val = uuie.m_h323_uu_pdu.m_genericData[0].m_parameters[0].m_content;
    val.SetSize(0); // remove any existing contents
    val.EncodeSubType(data);

    // Set the Call Identifier
    H225_Information_UUIE & info = uuie.m_h323_uu_pdu.m_h323_message_body;
    info.m_callIdentifier = StringToCallId(callId);

    // Build the Q931 Message
    q931.BuildInformation(0, true);
    q931.SetCallState(Q931::CallState_CallInitiated);
    SetInfoUUIE(q931,uuie);
    data.m_frame.SetSize(0);
}

PBoolean ReadRTPFrame(const Q931 & q931, PString & callId, H46026_UDPFrame & data)
{
    H225_H323_UserInformation uuie;
    if (!GetInfoUUIE(q931, uuie)) {
        PTRACE(2,"H46026\tError decoding Media PDU");
        return false;
    }

    // sanity checks.
    if ((!uuie.m_h323_uu_pdu.HasOptionalField(H225_H323_UU_PDU::e_genericData)) || 
        (uuie.m_h323_uu_pdu.m_genericData.GetSize() == 0) || 
        (!uuie.m_h323_uu_pdu.m_genericData[0].HasOptionalField(H225_GenericData::e_parameters)) || 
        (uuie.m_h323_uu_pdu.m_genericData[0].m_parameters.GetSize() == 0) ||
        (!uuie.m_h323_uu_pdu.m_genericData[0].m_parameters[0].HasOptionalField(H225_EnumeratedParameter::e_content))
        ) {
            PTRACE(2,"H46026\tERROR BAD Media Frame structure");
            return false;
    }
    H225_GenericIdentifier & id = uuie.m_h323_uu_pdu.m_genericData[0].m_parameters[0].m_id;
    if (id.GetTag() != H225_GenericIdentifier::e_standard) {
        PTRACE(2,"H46026\tERROR BAD Media Frame ID");
        return false;
    }
	PASN_Integer & asnInt = id;
    if (asnInt.GetValue() != 26) {
        PTRACE(2,"H46026\tERROR Wrong Media Frame ID " << asnInt.GetValue());
        return false;
    }
    H225_GenericIdentifier & pid = uuie.m_h323_uu_pdu.m_genericData[0].m_parameters[0].m_id;
    if (id.GetTag() != H225_GenericIdentifier::e_standard) {
        PTRACE(2,"H46026\tERROR BAD Media Parameter ID");
        return false;
    }
	PASN_Integer & pInt = pid;
    if (pInt.GetValue() != 1) {
        PTRACE(2,"H46026\tERROR Wrong Media Parameter ID " << pInt.GetValue());
        return false;
    }

    // Get the RTP Payload
    PASN_OctetString & val = uuie.m_h323_uu_pdu.m_genericData[0].m_parameters[0].m_content;
    if (!val.DecodeSubType(data)) {
        PTRACE(2,"H46026\tERROR Decoding Media Frame");
        return false;
    }

    // Set the Call Identifier
    H225_Information_UUIE & info = uuie.m_h323_uu_pdu.m_h323_message_body;
    callId = CallIdToString(info.m_callIdentifier.m_guid);

    return true;
}

void ClearBufferEntries(H46026RTPBuffer & rtpBuffer, const PString callId)
{
    if (callId.IsEmpty()) {
        for (H46026RTPBuffer::iterator i = rtpBuffer.begin(); i != rtpBuffer.end(); ++i) {
            for (H46026CallMap::iterator j = i->second.begin(); j != i->second.end(); ++j) {
                delete j->second;
            }
            i->second.clear();
        }
        rtpBuffer.clear();
    } else {
        H46026RTPBuffer::iterator r = rtpBuffer.find(callId);
        if (r != rtpBuffer.end()) {
            for (H46026CallMap::iterator j = r->second.begin(); j != r->second.end(); ++j) {
                delete j->second;
            }
            r->second.clear();
            rtpBuffer.erase(r);
        }
    }
}

//-------------------------------------------

H46026UDPBuffer::H46026UDPBuffer(int sessionId, PBoolean rtp) 
{ 
    m_size = 0;
    m_data.m_sessionId.SetValue(sessionId);
    m_data.m_dataFrame = true;
    m_data.m_frame.SetSize(0);
    m_rtp = rtp;
}

H46026_ArrayOf_FrameData & H46026UDPBuffer::GetData()
{
    return m_data.m_frame;
}

void H46026UDPBuffer::SetFrame(const PBYTEArray & data)
{
    m_size += data.GetSize();
    int sz = m_data.GetSize();
    m_data.m_frame.SetSize(sz+1);
    m_data.m_frame[sz].SetTag(m_rtp ? H46026_FrameData::e_rtp : H46026_FrameData::e_rtcp);
    PASN_OctetString & raw = m_data.m_frame[sz];
    raw = data;
}

H46026_UDPFrame & H46026UDPBuffer::GetBuffer()
{
    return m_data;
}

void H46026UDPBuffer::ClearBuffer()
{
    m_size = 0;
    m_data.SetSize(0);
}

PINDEX H46026UDPBuffer::GetSize()
{
    return m_size;
}

PINDEX H46026UDPBuffer::GetPacketCount()
{
    return m_data.GetSize();
}

//-------------------------------------------

H46026_MediaFrame::H46026_MediaFrame(const BYTE * data, PINDEX len)
   :PBYTEArray(data,len)
{

}
    
PBoolean H46026_MediaFrame::GetMarker()
{
    return (theArray[1]&0x80) != 0;
}

//-------------------------------------------

H46026ChannelManager::H46026ChannelManager()
:  m_mbps(MAX_VIDEO_KBPS), m_socketPacketReady(false), m_currentPacketTime(0)
{

// Initialise the Information PDU RTP Message structure.
    H225_H323_UU_PDU & info = m_uuie.m_h323_uu_pdu;
    info.IncludeOptionalField(H225_H323_UU_PDU::e_genericData);
    info.m_genericData.SetSize(1);
    H225_GenericData & gen = m_uuie.m_h323_uu_pdu.m_genericData[0];
    H225_GenericIdentifier & id = gen.m_id;
        id.SetTag(H225_GenericIdentifier::e_standard);
		PASN_Integer & asnInt = id;
		asnInt.SetValue(26); 
    gen.IncludeOptionalField(H225_GenericData::e_parameters);
    gen.m_parameters.SetSize(1);
    H225_EnumeratedParameter & param = gen.m_parameters[0];
        H225_GenericIdentifier & pid = param.m_id;
        pid.SetTag(H225_GenericIdentifier::e_standard);
		PASN_Integer & pInt = pid;
		pInt.SetValue(1);
    param.IncludeOptionalField(H225_EnumeratedParameter::e_content);
    param.m_content.SetTag(H225_Content::e_raw);
    // param.m_content is set in BuildRTPFrame
    
    m_uuie.m_h323_uu_pdu.m_h323_message_body.SetTag(H225_H323_UU_PDU_h323_message_body::e_information);
	m_uuie.m_h323_uu_pdu.m_h245Tunneling = TRUE;
    H225_Information_UUIE & infoUUIE = m_uuie.m_h323_uu_pdu.m_h323_message_body;
    infoUUIE.m_protocolIdentifier.SetValue(H46026_ProtocolID, PARRAYSIZE(H46026_ProtocolID));
    // infoUUIE.m_callIdentifier is set in BuildRTPFrame
}

H46026ChannelManager::~H46026ChannelManager()
{
    PWaitAndSignal m(m_queueMutex);

    ClearBufferEntries(m_rtpBuffer, PString());
    while (!m_socketQueue.empty())
        m_socketQueue.pop();
}

 void H46026ChannelManager::RTPFrameIn(const PString & callId, PINDEX sessionId, PBoolean rtp, PBYTEArray data) 
 {
     return RTPFrameIn(callId, sessionId, rtp, data.GetPointer(), data.GetSize());
 }

 void H46026ChannelManager::SetPipeBandwidth(unsigned bps)
 {
     m_mbps = double(bps);
 }

void H46026ChannelManager::BufferRelease(const PString & callId)
{
    ClearBufferEntries(m_rtpBuffer, callId);
}

PBoolean H46026ChannelManager::SignalMsgOut(const Q931 & pdu)
{
    socketOrder::MessageHeader prior;
    prior.sessionId = 0;
    prior.callId = PString();
    prior.priority = socketOrder::Priority_High;
    prior.packTime = PTimer::Tick().GetMilliSeconds();
    prior.delay = PACKETDELAY(pdu.GetIE(Q931::UserUserIE).GetSize(),m_mbps);
    //prior.delay = int((pdu.GetIE(Q931::UserUserIE).GetSize() / MAX_PIPE_SHAPING) * 1000.0);
    return WriteQueue(pdu, prior);
}

PBoolean H46026ChannelManager::SignalMsgOut(const BYTE * data, PINDEX len)
{
    PBYTEArray msg(data,len);
    socketOrder::MessageHeader prior;
    prior.sessionId = 0;
    prior.callId = PString();
    prior.priority = socketOrder::Priority_High;
    prior.packTime = PTimer::Tick().GetMilliSeconds();
    prior.delay = PACKETDELAY(len,m_mbps);
    return WriteQueue(msg, prior);
}


H46026UDPBuffer * H46026ChannelManager::GetRTPBuffer(const PString & callId, int sessionId)
{
    H46026RTPBuffer::iterator r = m_rtpBuffer.find(callId);
    if (r != m_rtpBuffer.end()) {
        H46026CallMap::iterator c = r->second.find(sessionId);
        return c->second;
    }
    m_rtpBuffer[callId][sessionId] = new H46026UDPBuffer(sessionId,true);
    return m_rtpBuffer[callId][sessionId];
}

PBoolean H46026ChannelManager::PackageFrame(PBoolean rtp, const PString & callId, PacketTypes id, PINDEX sessionId, H46026_UDPFrame & data)
{
    // Build Packet
    Q931 mediaPDU;
    BuildRTPFrame(mediaPDU, m_uuie, callId, data);

    // Set metadata
    socketOrder::MessageHeader prior;
    prior.callId = callId;
    prior.sessionId = sessionId;
    if (rtp) {
        switch (id) {
            case e_Audio: 
                prior.priority = socketOrder::Priority_Critical; 
                break;
            case e_Video:
            case e_extVideo:
                prior.priority = socketOrder::Priority_Discretion; 
                break;
            case e_Data:
            default:
                prior.priority = socketOrder::Priority_High;
        }
    } else
        prior.priority = socketOrder::Priority_Low;
    prior.packTime = PTimer::Tick().GetMilliSeconds();
    prior.delay = PACKETDELAY(mediaPDU.GetIE(Q931::UserUserIE).GetSize(),m_mbps);   
    // int((mediaPDU.GetIE(Q931::UserUserIE).GetSize() / MAX_PIPE_SHAPING) * 1000.0);

    // Write to the output Queue
    return WriteQueue(mediaPDU, prior);
}

PBoolean H46026ChannelManager::RTPFrameOut(const PString & callId, PacketTypes id, PINDEX sessionId, PBoolean rtp, PBYTEArray & data)
{
    return RTPFrameOut(callId, id, sessionId, rtp, data.GetPointer() , data.GetSize());
}

PBoolean H46026ChannelManager::RTPFrameOut(const PString & callId, PacketTypes id, PINDEX sessionId, PBoolean rtp, BYTE * data, PINDEX len)
{
    PWaitAndSignal m(m_writeMutex);

    H46026_MediaFrame msg(data,len);
    if (rtp) {
        H46026UDPBuffer * m_buffer = GetRTPBuffer(callId,sessionId);
        if (!m_buffer) return false;

        PBoolean toSend = false;
        switch (id) {
            case e_Audio: 
                m_buffer->SetFrame(msg);
                if (m_buffer->GetPacketCount() >= MAX_AUDIO_FRAMES)
                    toSend = true;
                break;
            case e_Video:
            case e_extVideo:
                if (m_buffer->GetSize() + len > MAX_VIDEO_PAYLOAD) {
                    PackageFrame(rtp,callId,id,sessionId,m_buffer->GetBuffer());
                }
                m_buffer->SetFrame(msg);
                toSend = msg.GetMarker();
                break;
            case e_Data:
            default:
                m_buffer->SetFrame(msg);
                toSend = true;
        }
        if (toSend)
            return PackageFrame(rtp,callId,id,sessionId,m_buffer->GetBuffer());
        else
            return ProcessQueue();
    } else {
       H46026UDPBuffer m_data(sessionId,rtp);
       m_data.SetFrame(msg);
       return PackageFrame(rtp,callId,id,sessionId,m_data.GetBuffer());
    }
    return true; 
}

PBoolean H46026ChannelManager::ProcessQueue()
{
    PWaitAndSignal m(m_queueMutex);

    if (m_socketQueue.size() == 0) {
        m_socketPacketReady = false;
        return true;
    }

    PInt64 nowTime = PTimer::Tick().GetMilliSeconds();
    PInt64 stackTime = nowTime - m_socketQueue.top().second.packTime;

    PBoolean dropPacket = ((m_socketQueue.top().second.priority == socketOrder::Priority_Discretion) && 
            (stackTime > MAX_STACK_DESCRETION));

    if (dropPacket) {

        PTRACE(5,"H46026\tPossible pipe blockage detected. Delay " << stackTime << " Dropping video frames...");
        PString callId = m_socketQueue.top().second.callId;
        PINDEX sessionId = m_socketQueue.top().second.sessionId;
        while (m_socketQueue.size()>0 && m_socketQueue.top().second.priority == socketOrder::Priority_Discretion) {
            m_socketQueue.pop();
        }
        FastUpdatePictureRequired(callId, sessionId);
        // TODO: Flow Control based on the Time in the queue (stackTime) - SH
    }

    m_socketPacketReady = (m_socketQueue.size() > 0);

    PTRACE(6,"H46026\tP: " << m_socketQueue.top().second.priority << " pack "  << m_socketQueue.top().second.packTime  << " delay " << stackTime);

    return true;
}

PBoolean H46026ChannelManager::SocketIn(const BYTE * data, PINDEX len)
{
    PBYTEArray buffer(data,len);
    return SocketIn(buffer);
}

PBoolean H46026ChannelManager::SocketIn(const PBYTEArray & data)
{
	Q931 * q931pdu = new Q931();
	if (!q931pdu->Decode(data)) {
		PTRACE(1, "H46026\tERROR DECODING Q.931!");
		delete q931pdu;
		q931pdu = NULL;
		return false;
	} else
        return SocketIn(q931pdu);
}

PBoolean H46026ChannelManager::SocketIn(Q931 * q931)
{
    PString callId = PString();
    H46026_UDPFrame frameData;

    if ((q931->GetMessageType() == Q931::InformationMsg) && ReadRTPFrame(*q931, callId, frameData)) {
        for (PINDEX i=0; i < frameData.m_frame.GetSize(); ++i) {
            PASN_OctetString & data = frameData.m_frame[i];
            RTPFrameIn(callId, frameData.m_sessionId.GetValue(), frameData.m_dataFrame, data.GetValue());
        }
    } else
        SignalMsgIn(q931);

    if (q931)
       delete q931;

    return true;
}

PBoolean H46026ChannelManager::SocketOut(PBYTEArray & data)
{
    return SocketOut(data.GetPointer(), data.GetSize());
}

PBoolean H46026ChannelManager::SocketOut(BYTE * data, PINDEX len)
{
    if (!m_socketPacketReady)
        return false;

    PInt64 nowTime = PTimer::Tick().GetMilliSeconds();
    if (m_currentPacketTime > nowTime)
        return false;

    PBoolean gotPacket = false;
    m_queueMutex.Wait();
        if (m_socketQueue.size() > 0) {
            m_currentPacketTime = nowTime + m_socketQueue.top().second.delay;
            len = m_socketQueue.top().first.GetSize();
            memcpy(data, m_socketQueue.top().first, len);
            m_socketQueue.pop();
            gotPacket = true;
        } else {
            m_currentPacketTime = nowTime;
        }
    m_queueMutex.Signal();

    return gotPacket;
}

PBoolean H46026ChannelManager::WriteQueue(const Q931 & msg, const socketOrder::MessageHeader & prior)
{
    PBYTEArray data;
    msg.Encode(data);
    return WriteQueue(data, prior);
}

PBoolean H46026ChannelManager::WriteQueue(const PBYTEArray & data, const socketOrder::MessageHeader & prior)
{
    m_queueMutex.Wait();
     m_socketQueue.push(pair<PBYTEArray, socketOrder::MessageHeader>(data, prior) );
    m_queueMutex.Signal();

    return ProcessQueue();
}

#endif // H323_H46026
