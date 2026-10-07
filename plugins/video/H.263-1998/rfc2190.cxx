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

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "rfc2190.h"

using namespace std;

// Mode A uses a 4 byte payload header, Mode B an 8 byte one.  The fragment
// search always budgets for the larger of the two, as FFmpeg's own RFC 2190
// packetiser does - a Mode A packet then simply ends up 4 bytes shorter
// than it strictly needs to be.
// size of one AV_PKT_DATA_H263_MB_INFO entry
#define MB_INFO_ENTRY_SIZE 12

// the mb_info side data is little endian regardless of host byte order
static inline uint32_t ReadLE32(const unsigned char * p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint16_t ReadLE16(const unsigned char * p)
{
  return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

const unsigned char PSC[3]      = { 0x00, 0x00, 0x80 };
const unsigned char PSC_Mask[3] = { 0xff, 0xff, 0xfc };

static int MacroblocksPerGOBTable[] = {
    -1,  // forbidden
    -1,  // sub-QCIF
     (144 / 16) *  (176 / 16),  //  QCIF = 99
     (288 / 16) *  (352 / 16),  //  CIF  = 396
     (576 / 32) *  (704 / 32),  //  4CIF = 396
    (1408 / 64) * (1152 / 64),  //  16CIF = 396
    -1,  // Reserved
    -1   // extended
};


static int FindByteAlignedCode(const unsigned char * base, int len, const unsigned char * code, const unsigned char * mask, int codeLen)
{
  const unsigned char * ptr = base;
  while (len > codeLen) {
    int i;
    for (i = 0; i < codeLen; ++i) {
      if ((ptr[i] & mask[i]) != code[i])
        break;
    }
    if (i == codeLen)
      return (int)(ptr - base);
    ++ptr;
    --len;
  }
  return -1;
}


static int FindPSC(const unsigned char * base, int len)
{ return FindByteAlignedCode(base, len, PSC, PSC_Mask, sizeof(PSC)); }


/* Scan backwards for the last byte aligned GOB/picture start code strictly
   inside (start, end).  A start code is the 17 bit pattern
   0000 0000 0000 0000 1, i.e. the bytes 00 00 1xxxxxxx once byte aligned.
   Returns end if there is none, so the caller can fall back to mb_info. */
static const unsigned char * FindResyncMarkerReverse(const unsigned char * start,
                                                     const unsigned char * end,
                                                     const unsigned char * bufferEnd)
{
  const unsigned char * p = end - 1;
  if (p > bufferEnd - 3)
    p = bufferEnd - 3;

  for (; p > start; --p) {
    if (p[0] == 0 && p[1] == 0 && (p[2] & 0x80) != 0)
      return p;
  }

  return end;
}


///////////////////////////////////////////////////////////////////////////////////////

RFC2190Packetizer::RFC2190Packetizer()
  : m_currentFragment(0)
  , m_timestamp(0)
  , m_TR(0)
  , m_srcFormat(0)
  , m_iFrame(true)
  , m_annexD(false)
  , m_annexE(false)
  , m_annexF(false)
  , m_annexG(false)
  , m_pQuant(0)
  , m_cpm(false)
{
}


RFC2190Packetizer::~RFC2190Packetizer()
{
}


int RFC2190Packetizer::ParsePictureHeader()
{
  const unsigned char * data = &m_buffer[0];
  size_t dataLen = m_buffer.size();

  // must be long enough to hold PSC, TR and the mandatory part of PTYPE,
  // PQUANT and CPM
  if (dataLen < 7)
    return -1;

  // must start with a picture start code
  //     0         1         2
  // 0000 0000 0000 0000 1000 00..
  if (FindPSC(data, (int)dataLen) != 0)
    return -2;

  // TR
  //     2         3
  // .... ..XX XXXX XX..
  m_TR = ((data[2] << 6) & 0xfc) | (data[3] >> 2);

  // mandatory part of PTYPE
  //     3
  // .... ..10
  if ((data[3] & 0x03) != 2)
    return -3;

  // split screen, document camera and freeze picture release are not supported
  //     4
  // XXX. ....
  if ((data[4] & 0xe0) != 0)
    return -4;

  // source format
  //     4
  // ...X XX..
  m_srcFormat = (data[4] >> 2) & 0x7;
  if (MacroblocksPerGOBTable[m_srcFormat] == -1)
    return -6;

  // picture coding type
  //     4
  // .... ..X.
  m_iFrame = (data[4] & 0x02) == 0;

  // annex bits:
  //   Annex D - unrestricted motion vectors
  //   Annex E - syntax based arithmetic coding
  //   Annex F - advanced prediction
  //   Annex G - PB frames
  //
  //     4         5
  // .... ...X XXX. ....
  m_annexD = (data[4] & 0x01) != 0;
  m_annexE = (data[5] & 0x80) != 0;
  m_annexF = (data[5] & 0x40) != 0;
  m_annexG = (data[5] & 0x20) != 0;

  // PB frames are not supported
  if (m_annexG)
    return -5;

  // PQUANT
  //     5
  // ...X XXXX
  m_pQuant = data[5] & 0x1f;

  // CPM
  //     6
  // X... ....
  m_cpm = (data[6] & 0x80) != 0;

  // PEI must be 0
  //     6
  // .X.. ....
  if (!m_cpm && (data[6] & 0x40) != 0)
    return -6;

  return 0;
}


void RFC2190Packetizer::BuildFragments(const unsigned char * mbInfo,
                                       size_t mbInfoLen,
                                       size_t maxPayloadSize)
{
  m_fragments.clear();
  m_currentFragment = 0;

  const unsigned char * base = &m_buffer[0];
  const unsigned char * bufferEnd = base + m_buffer.size();
  const unsigned char * buf = base;

  size_t size = m_buffer.size();
  size_t mbInfoCount = mbInfoLen / MB_INFO_ENTRY_SIZE;
  size_t mbInfoPos = 0;

  // usable payload per packet, assuming the larger Mode B header
  size_t maxData = (maxPayloadSize > RFC2190_MODE_B_HEADER_SIZE)
                        ? maxPayloadSize - RFC2190_MODE_B_HEADER_SIZE : 1;

  unsigned sBits = 0;
  unsigned eBits = 0;

  // state carried from one split point to the next, used for the Mode B
  // header of the packet that *starts* at that point
  Fragment state;
  memset(&state, 0, sizeof(state));

  while (size > 0) {
    Fragment frag = state;
    size_t len = (maxData < size) ? maxData : size;

    eBits = 0;

    // look for a nicer place to split the frame
    if (len < size) {
      const unsigned char * end = FindResyncMarkerReverse(buf, buf + len, bufferEnd);
      len = (size_t)(end - buf);

      if (len < maxData) {
        /* A real GOB header starts the next fragment.  Read its GOB number
           (GBSC is 16 zero bits then a 1, so end[2]'s top bit is that 1 and
           the next 5 bits - the rest of end[2] - are GN) directly out of
           the bitstream, the same source of truth the encoder itself used,
           rather than trying to track it by counting splits.  This is what
           lets the RARE fallback below still report a correct GOB number
           even on a picture with no mb_info at all - which is the normal
           case now, see ApplyCodecOptions() in h263-1998.cxx for why. */
        if (end + 2 < bufferEnd)
          state.gobn = (unsigned)((end[2] >> 2) & 0x1F);
      }
      else {
        // no GOB header close enough to split on, fall back to the
        // macroblock info

        // skip entries before the current position
        while (mbInfoPos < mbInfoCount) {
          uint32_t pos = ReadLE32(&mbInfo[MB_INFO_ENTRY_SIZE*mbInfoPos]) / 8;
          if (pos >= (uint32_t)(buf - base))
            break;
          mbInfoPos++;
        }

        // advance to the last entry that still fits
        while (mbInfoPos + 1 < mbInfoCount) {
          uint32_t pos = ReadLE32(&mbInfo[MB_INFO_ENTRY_SIZE*(mbInfoPos + 1)]) / 8;
          if (pos >= (uint32_t)(end - base))
            break;
          mbInfoPos++;
        }

        if (mbInfoPos < mbInfoCount) {
          const unsigned char * ptr = &mbInfo[MB_INFO_ENTRY_SIZE*mbInfoPos];
          uint32_t bitPos = ReadLE32(ptr);
          uint32_t bytePos = (bitPos + 7) / 8;
          if (bytePos <= (uint32_t)(end - base)) {
            state.quant = ptr[4];
            state.gobn  = ptr[5];
            state.mba   = (unsigned)ReadLE16(&ptr[6]);
            state.hmv1  = (int)(int8_t)ptr[8];
            state.vmv1  = (int)(int8_t)ptr[9];
            state.hmv2  = (int)(int8_t)ptr[10];
            state.vmv2  = (int)(int8_t)ptr[11];
            eBits = 8*bytePos - bitPos;
            len   = bytePos - (size_t)(buf - base);
            mbInfoPos++;
          }
        }
        // if there is no usable macroblock info we just split on a byte
        // boundary and hope the far end copes; that is what happens when
        // neither "mb_info" nor "ps" were accepted by the encoder
      }
    }

    frag.offset = (size_t)(buf - base);
    frag.length = len;
    frag.sBit   = sBits;
    frag.eBit   = eBits;
    // Mode A is only legal when the fragment starts on a start code
    frag.modeB  = !(size > 2 && buf[0] == 0 && buf[1] == 0);

    m_fragments.push_back(frag);

    if (eBits != 0) {
      sBits = 8 - eBits;
      len--;            // the shared byte is repeated in the next packet
    }
    else {
      sBits = 0;
    }

    buf  += len;
    size -= len;
  }
}


int RFC2190Packetizer::Open(unsigned long timeStamp,
                            const unsigned char * encoded,
                            size_t encodedLen,
                            const unsigned char * mbInfo,
                            size_t mbInfoLen,
                            size_t maxPayloadSize)
{
  m_timestamp = timeStamp;
  m_fragments.clear();
  m_currentFragment = 0;

  m_buffer.assign(encoded, encoded + encodedLen);

  int result = ParsePictureHeader();
  if (result < 0)
    return result;

  if (mbInfo == NULL)
    mbInfoLen = 0;

  BuildFragments(mbInfo, mbInfoLen, maxPayloadSize);

  return 0;
}


int RFC2190Packetizer::GetPacket(RTPFrame & outputFrame, unsigned int & flags)
{
  while (m_currentFragment < m_fragments.size()) {

    const Fragment & frag = m_fragments[m_currentFragment++];

    size_t hdrSize = frag.modeB ? RFC2190_MODE_B_HEADER_SIZE : RFC2190_MODE_A_HEADER_SIZE;
    size_t payloadRemaining = outputFrame.GetFrameLen() - outputFrame.GetHeaderSize();

    if ((frag.length + hdrSize) > payloadRemaining)
      continue;                 // will not fit, drop it

    outputFrame.SetTimestamp(m_timestamp);
    outputFrame.SetPayloadSize((int)(hdrSize + frag.length));

    unsigned char * ptr = outputFrame.GetPayloadPtr();

    if (!frag.modeB) {
      // Mode A
      //  0                   1                   2                   3
      // |F|P|SBIT |EBIT | SRC |I|U|S|A|R      |DBQ| TRB |    TR         |
      ptr[0] = (unsigned char)(((frag.sBit & 7) << 3) | (frag.eBit & 7));
      ptr[1] = (unsigned char)((m_srcFormat << 5)
                             | (m_iFrame  ? 0 : 0x10)
                             | (m_annexD  ? 0x08 : 0)
                             | (m_annexE  ? 0x04 : 0)
                             | (m_annexF  ? 0x02 : 0));
      ptr[2] = 0;
      ptr[3] = (unsigned char)m_TR;
    }
    else {
      // Mode B
      //  0                   1                   2                   3
      // |F|P|SBIT |EBIT | SRC | QUANT   |  GOBN   |   MBA          |R  |
      // |I|U|S|A| HMV1        | VMV1        | HMV2        | VMV2        |
      int hmv1 = frag.hmv1 & 0x7f;
      int vmv1 = frag.vmv1 & 0x7f;
      int hmv2 = frag.hmv2 & 0x7f;
      int vmv2 = frag.vmv2 & 0x7f;
      ptr[0] = (unsigned char)(0x80 | ((frag.sBit & 7) << 3) | (frag.eBit & 7));
      ptr[1] = (unsigned char)((m_srcFormat << 5) | (frag.quant & 0x1f));
      ptr[2] = (unsigned char)(((frag.gobn << 3) & 0xf8) | ((frag.mba >> 6) & 0x07));
      ptr[3] = (unsigned char)((frag.mba << 2) & 0xfc);
      ptr[4] = (unsigned char)((m_iFrame ? 0 : 0x80)
                             | (m_annexD ? 0x40 : 0)
                             | (m_annexE ? 0x20 : 0)
                             | (m_annexF ? 0x10 : 0)
                             | ((hmv1 >> 3) & 0x0f));
      ptr[5] = (unsigned char)(((hmv1 << 5) & 0xe0) | ((vmv1 >> 2) & 0x1f));
      ptr[6] = (unsigned char)(((vmv1 << 6) & 0xc0) | ((hmv2 >> 1) & 0x3f));
      ptr[7] = (unsigned char)(((hmv2 << 7) & 0x80) | (vmv2 & 0x7f));
    }

    memcpy(ptr + hdrSize, &m_buffer[frag.offset], frag.length);

    flags = 0;
    if (m_currentFragment == m_fragments.size()) {
      flags |= 1;
      outputFrame.SetMarker(1);
    }
    else {
      outputFrame.SetMarker(0);
    }
    if (m_iFrame)
      flags |= 2;

    return 1;
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////3Y

RFC2190Depacketizer::RFC2190Depacketizer()
{
  NewFrame();
  lastSequence = 0;
}

void RFC2190Depacketizer::NewFrame()
{
  frame.resize(0);
  first               = true;
  skipUntilEndOfFrame = false;
  lastEbit            = 8;
}

int RFC2190Depacketizer::LostSync(bool & requestIframe, const char * /*reason*/)
{
  skipUntilEndOfFrame = true;
  requestIframe = true;
  return 0;
}

int RFC2190Depacketizer::SetPacket(const RTPFrame & inputFrame, bool & requestIFrame, bool & isIFrame)
{
  requestIFrame = false;
  isIFrame      = false;

  // ignore packets if required
  if (skipUntilEndOfFrame) {
    if (inputFrame.GetMarker()) 
      NewFrame();
    return 0;
  }

  // check if packet is in sequence. If not, skip til end of frame 
  if (first) {
    NewFrame();    // make sure this is called before "first = false"
    first = false;
    lastSequence = inputFrame.GetSequenceNumber();
  }
  else {
    ++lastSequence;
    if (inputFrame.GetSequenceNumber() != lastSequence) {
      return LostSync(requestIFrame, "missed frame");
    }
  }

  unsigned payloadLen = inputFrame.GetPayloadSize();

  // payload must be at least as long as mode A header + 1 byte
  if (payloadLen < 5) 
    return LostSync(requestIFrame, "payload too small");

  unsigned char * payload = inputFrame.GetPayloadPtr();
  unsigned int sbit = (payload[0] >> 3) & 0x07;
  unsigned hdrLen;

  // handle mode A frames
  if ((payload[0] & 0x80) == 0) {
    isIFrame = (payload[1] & 0x10) == 0;
    hdrLen = 4;

#if 0
    // sanity check data
    if (payloadLen < (hdrLen+3) ||
        (payload[hdrLen+0] != 0x00) ||
        (payload[hdrLen+1] != 0x00) ||
        ((payload[hdrLen+2] & 0x80) != 0x80)
       ) {
      return LostSync(requestIFrame, "Mode A packet not starting with GBSC");
    }
#endif
  }

  // handle mode B frames
  else if ((payload[0] & 0x40) == 0) {
    if (payloadLen < 9)
      return LostSync(requestIFrame, "mode B payload too small");
    isIFrame = (payload[4] & 0x80) == 0;
    hdrLen = 8;
  }

  // handle mode C frames
  else {
    if (payloadLen < 13)
      return LostSync(requestIFrame, "mode C payload too small");
    isIFrame = (payload[4] & 0x80) == 0;
    hdrLen = 12;
  }

  // if ebit and sbit do not add up, then we have lost sync
  if (((sbit + lastEbit) & 0x7) != 0) {
    return LostSync(requestIFrame, "mismatched ebit and sbit");
  }

  unsigned char * src = payload + hdrLen;
  size_t cpyLen = payloadLen - hdrLen;

  // handle first partial byte
  if ((sbit != 0) && (frame.size() > 0)) {
    
    static unsigned char smasks[7] = { 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01 };
    unsigned smask = smasks[sbit-1];
    frame[frame.size()-1] |= (*src & smask);
    --cpyLen;
    ++src;
  }

  // copy whole bytes
  if (cpyLen > 0) {
    size_t frameSize = frame.size();
    frame.resize(frameSize + cpyLen);
    memcpy(&frame[0] + frameSize, src, cpyLen);
  }

  // keep ebit for next time
  lastEbit = payload[0] & 0x07;

  /* Return 0 while the picture is still being reassembled and 1 once the
     marker bit says it is complete. */
  if (!inputFrame.GetMarker())
    return 0;

  return 1;
}

