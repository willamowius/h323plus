/* h46026mgr.h
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

#ifndef H_H460_Featurestd26
#define H_H460_Featurestd26

#include <h460/h46026.h>
#include <queue>
#include <vector>
#include <map>

class H46026UDPBuffer {
public:
    H46026UDPBuffer(int sessionId, PBoolean rtp);

    H46026_ArrayOf_FrameData & GetData();

    void SetFrame(const PBYTEArray & data);

    H46026_UDPFrame & GetBuffer();
    void ClearBuffer();

    PINDEX GetSize();
    PINDEX GetPacketCount();

protected:
    PINDEX m_size;
    H46026_UDPFrame m_data;
    PBoolean m_rtp;
};


class socketOrder {
public:
     enum priority {
        Priority_Critical=1,    // Audio RTP
        Priority_Discretion,    // Video RTP
        Priority_High,          // Signaling   
        Priority_Low            // RTCP
     };

     struct MessageHeader {
        unsigned crv;
        int sessionId;
        int priority;
        int delay;
        PInt64 packTime;
     };

     int operator() ( const std::pair<PBYTEArray, MessageHeader>& p1,
                      const std::pair<PBYTEArray, MessageHeader>& p2 ) {
           return ((p1.second.priority >= p2.second.priority) && 
                     (p1.second.packTime > p2.second.packTime));
     }
};

typedef std::priority_queue< std::pair<PBYTEArray, socketOrder::MessageHeader >, 
        std::vector< std::pair<PBYTEArray, socketOrder::MessageHeader> >, socketOrder > H46026SocketQueue;

typedef std::map<int,H46026UDPBuffer*> H46026CallMap;
typedef std::map<unsigned, H46026CallMap >  H46026RTPBuffer;

//-------------------------------------------------------------------------------

class H46026_MediaFrame  : public PBYTEArray
{
public:
    H46026_MediaFrame(const BYTE * data, PINDEX len);
    PBoolean GetMarker();
};

//-------------------------------------------------------------------------------

class Q931;
class H225_H323_UserInformation;
class H46026ChannelManager : public PObject
{  
  PCLASSINFO(H46026ChannelManager, PObject);

public:
    H46026ChannelManager();
    ~H46026ChannelManager();

    enum PacketTypes {
      e_Audio,   /// Audio packet
      e_Video,   /// Video packet
      e_Data,    /// data packet
      e_extVideo /// extended video
    };

    /* Set the pipe bandwidth Default is 384k */
    void SetPipeBandwidth(unsigned bps);

    /** Clear Buffers */
    /* Call this is clear the channel buffers at the end of a call */
    /* This MUST be called at the end of a call */
    void BufferRelease(unsigned crv);

    /** Events */
    /* Retrieve a Signal Message. Note RAS messages are included.
     */
    virtual void SignalMsgIn(const Q931 & /*pdu*/);
    virtual void SignalMsgIn(unsigned /*crv*/, const Q931 & /*pdu*/) {};

    /* Retrieve an RTP/RTCP Frame. The sessionID is the id of the session
     */
    virtual void RTPFrameIn(unsigned /*crv*/, PINDEX /*sessionId*/, PBoolean /*rtp*/, const PBYTEArray & /*data*/);
    virtual void RTPFrameIn(unsigned /*crv*/, PINDEX /*sessionId*/, PBoolean /*rtp*/, const BYTE * /*data*/, PINDEX /*len*/) {};

    /* Fast Picture Update Required on the non-tunneled side.
     */
    virtual void FastUpdatePictureRequired(unsigned /*crv*/, PINDEX /*sessionId*/) {};


    /* Methods */
    /* Sending to the socket */
    /* Post a Signal Message. Note RAS messages must be included.
     */
    PBoolean SignalMsgOut(const Q931 & pdu);
    PBoolean SignalMsgOut(const BYTE * data, PINDEX len);

    /* Post an RTP/RTCP Frame. Need to specify the basic packet type, sessionID and whether RTP or RTCP packet
     */
    PBoolean RTPFrameOut(unsigned crv, PacketTypes id, PINDEX sessionId, PBoolean rtp, const BYTE * data, PINDEX len);
    PBoolean RTPFrameOut(unsigned crv, PacketTypes id, PINDEX sessionId, PBoolean rtp, PBYTEArray & data);

    /* Collect an incoming to send to the socket.
        if the function return 
            True: data has been copied and ready to send on the wire.
            False: there is no data or data is not ready.
        WARNING: This function does not block and returns immediately if false.
        Best practise would be to call from a threaded loop and use PThread::Sleep
     */
    PBoolean SocketOut(BYTE * data, PINDEX & len);
    PBoolean SocketOut(PBYTEArray & data, PINDEX & len);

    /* Receiving from the socket */
    /* Process an incoming message from the socket.
        Returns false if message could not be handled or decoded into Q931.
     */
    PBoolean SocketIn(const BYTE * data, PINDEX len);
    PBoolean SocketIn(const PBYTEArray & data);
    PBoolean SocketIn(const Q931 & q931);

protected:
    PBoolean WriteQueue(const Q931 & msg, const socketOrder::MessageHeader & prior);
    PBoolean WriteQueue(const PBYTEArray & data, const socketOrder::MessageHeader & prior);

    PBoolean PackageFrame(PBoolean rtp, unsigned crv, PacketTypes id, PINDEX sessionId, H46026_UDPFrame & data);
    H46026UDPBuffer * GetRTPBuffer(unsigned crv, int sessionId);

    PBoolean ProcessQueue();

private:
    double                     m_mbps;
    PBoolean                   m_socketPacketReady;
    PInt64                     m_currentPacketTime;

    H225_H323_UserInformation  m_uuie;
    H46026RTPBuffer            m_rtpBuffer;
    PMutex                     m_writeMutex;
    H46026SocketQueue          m_socketQueue;
    PMutex                     m_queueMutex;
};


#endif  // H_H460_Featurestd26