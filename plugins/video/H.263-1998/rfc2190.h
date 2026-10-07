/*
 * RFC 2190 packetiser and unpacketiser
 *
 * Copyright (C) 2008 Post Increment
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
 * The Original Code is Opal
 *
 * Contributor(s): Craig Southeren <craigs@postincrement.com>
 *
 */

#ifndef _RFC2190_H_
#define _RFC2190_H_

#include <vector>
#include <list>
#include <stddef.h>

#include "../common/rtpframe.h"

// Mode A uses a 4 byte payload header, Mode B an 8 byte one
#define RFC2190_MODE_A_HEADER_SIZE  4
#define RFC2190_MODE_B_HEADER_SIZE  8

class RFC2190Depacketizer {
  public:
    RFC2190Depacketizer();
    void NewFrame();
    int SetPacket(const RTPFrame & outputFrame, bool & requestIFrame, bool & isIFrame);

    std::vector<unsigned char> frame;

  protected:
    unsigned lastSequence;
    int LostSync(bool & requestIFrame, const char * reason);
    bool first;
    bool skipUntilEndOfFrame;
    unsigned lastEbit;
};


/*
  The packetiser used to be driven by AVCodecContext::rtp_callback, which
  libavcodec called once per RTP sized chunk while encoding.  That callback
  was removed from the public API in FFmpeg 5.0.

  The supported replacement is the "mb_info" private option of the H.263
  encoder: when it is set, every encoded AVPacket carries an
  AV_PKT_DATA_H263_MB_INFO side data block describing macroblock boundaries
  the frame may be split on.  Each entry is 12 bytes:

      u32le  bit offset from the start of the packet
      u8     quantiser in effect at the start of the macroblock
      u8     GOB number
      u16le  macroblock address within the GOB
      u8     horizontal MV predictor
      u8     vertical MV predictor
      u8     horizontal MV predictor for block 3
      u8     vertical MV predictor for block 3

  Open() takes the encoded frame plus that side data and precomputes the
  fragment list; GetPacket() then emits one RTP packet per fragment.  Unlike
  the callback based version this also yields correct SBIT/EBIT values and
  real MV predictors in the Mode B headers.
 */
class RFC2190Packetizer
{
  public:
    RFC2190Packetizer();
    ~RFC2190Packetizer();

    /* Prepare a freshly encoded frame for transmission.  maxPayloadSize is
       the number of payload bytes available in one RTP packet, i.e. the MTU
       less the RTP header.  Returns 0 on success, negative if the picture
       header could not be parsed. */
    int Open(unsigned long timeStamp,
             const unsigned char * encoded,
             size_t encodedLen,
             const unsigned char * mbInfo,
             size_t mbInfoLen,
             size_t maxPayloadSize);

    /* Fill outputFrame with the next fragment.  Returns 1 if a packet was
       produced, 0 when the frame is exhausted. */
    int GetPacket(RTPFrame & outputFrame, unsigned int & flags);

    bool IsIFrame() const { return m_iFrame; }
    size_t GetFragmentCount() const { return m_fragments.size(); }

    struct Fragment {
      size_t   offset;      // byte offset into m_buffer
      size_t   length;      // bytes in this fragment
      unsigned sBit;        // bits to ignore at the start of the first byte
      unsigned eBit;        // bits to ignore at the end of the last byte
      bool     modeB;       // false: Mode A (4 byte header), true: Mode B (8 bytes)
      unsigned quant;       // Mode B: quantiser at the start of the fragment
      unsigned gobn;        // Mode B: GOB number
      unsigned mba;         // Mode B: macroblock address within the GOB
      int      hmv1, vmv1;  // Mode B: MV predictors
      int      hmv2, vmv2;
    };

  protected:
    int  ParsePictureHeader();
    void BuildFragments(const unsigned char * mbInfo, size_t mbInfoLen, size_t maxPayloadSize);

    std::vector<unsigned char> m_buffer;
    std::vector<Fragment>      m_fragments;
    size_t                     m_currentFragment;
    unsigned long              m_timestamp;

    // picture header fields shared by every packet of the frame
    unsigned m_TR;
    unsigned m_srcFormat;
    bool     m_iFrame;
    bool     m_annexD;  // U - unrestricted motion vectors
    bool     m_annexE;  // S - syntax based arithmetic coding
    bool     m_annexF;  // A - advanced prediction
    bool     m_annexG;  // PB frames (not supported)
    unsigned m_pQuant;
    bool     m_cpm;
};

#endif // _RFC2190_H_
