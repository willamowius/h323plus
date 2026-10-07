// Unit tests for H264Frame (h264frame.h/.cxx) - the RTP packetisation /
// depacketisation logic shared by the encoder and decoder sides of the
// plugin. This links against the real, unmodified h264frame.cxx; no fake
// OpenH264 SDK is needed here since H264Frame has no dependency on it.
#include "framework.h"
#include "../h264frame.h"

#ifdef _MSC_VER
#include "../../common/rtpframe.h"
#else
#include "rtpframe.h"
#endif

#include <vector>
#include <cstring>

namespace {

// Builds one Annex-B NAL (4-byte start code + header byte + payload).
std::vector<uint8_t> MakeNal(uint8_t nalHeaderByte, const std::vector<uint8_t> & payload) {
  std::vector<uint8_t> nal = {0, 0, 0, 1, nalHeaderByte};
  nal.insert(nal.end(), payload.begin(), payload.end());
  return nal;
}

// Drives H264Frame::SetFromFrame() with a set of NALs already built via
// MakeNal(), mirroring what openh264.cxx's flattening loop hands it.
void LoadFrame(H264Frame & frame, const std::vector<std::vector<uint8_t>> & nals) {
  std::vector<H264Frame::NALSource> sources;
  sources.reserve(nals.size());
  for (auto & n : nals)
    sources.push_back({n.data(), (unsigned)n.size()});
  frame.SetFromFrame(sources.data(), (unsigned)sources.size());
}

// Pulls every RTP packet GetRTPFrame() will produce for the currently
// loaded picture into a list of raw (header+payload) buffers, using the
// 2-argument RTPFrame constructor to match how EncodeFrames() itself uses
// it (a pre-existing header is expected to already be present - here we
// just zero-init 12 bytes ourselves to act as that pre-existing header).
struct Packet {
  std::vector<uint8_t> raw; // full RTP packet: 12-byte header + payload
  bool marker;
};

std::vector<Packet> DrainPackets(H264Frame & frame, unsigned maxPayloadSize) {
  frame.SetMaxPayloadSize((uint16_t)maxPayloadSize);
  std::vector<Packet> out;
  while (frame.HasRTPFrames()) {
    std::vector<uint8_t> buf(12 + maxPayloadSize + 16, 0); // headroom for FU/STAP overhead
    buf[0] = 0x80; // version 2, no padding/extension/CSRC - as a real RTP session would set up
    RTPFrame rtp(buf.data(), (int)buf.size());
    unsigned flags = 0;
    bool ok = frame.GetRTPFrame(rtp, flags);
    ASSERT_TRUE(ok);
    buf.resize(rtp.GetFrameLen());
    out.push_back({buf, rtp.GetMarker() != 0});
  }
  return out;
}

} // namespace

TEST(H264Frame, SingleSmallNal_ProducesOneRegularPacket) {
  H264Frame frame;
  auto sps = MakeNal(0x67, {0x42, 0xC0, 0x1E, 0x01, 0x02, 0x03});
  LoadFrame(frame, {sps});

  auto packets = DrainPackets(frame, 1400);
  ASSERT_EQ(packets.size(), (size_t)1);
  ASSERT_TRUE(packets[0].marker);

  RTPFrame rtp(packets[0].raw.data(), (int)packets[0].raw.size());
  ASSERT_EQ(rtp.GetPayloadSize(), (unsigned)(sps.size() - 4)); // minus start code
  ASSERT_EQ(memcmp(rtp.GetPayloadPtr(), sps.data() + 4, sps.size() - 4), 0);
}

TEST(H264Frame, MultipleNals_AllEncapsulated_MarkerOnlyOnLast) {
  H264Frame frame;
  auto sps = MakeNal(0x67, {0x42, 0xC0, 0x1E});
  auto pps = MakeNal(0x68, {0xCE, 0x3C, 0x80});
  auto idr = MakeNal(0x65, std::vector<uint8_t>(50, 0xAB));
  LoadFrame(frame, {sps, pps, idr});

  auto packets = DrainPackets(frame, 1400);
  ASSERT_EQ(packets.size(), (size_t)3);
  ASSERT_FALSE(packets[0].marker);
  ASSERT_FALSE(packets[1].marker);
  ASSERT_TRUE(packets[2].marker);
}

TEST(H264Frame, LargeNal_FragmentsViaFUA_ReassemblesExactly) {
  H264Frame frame;
  // NRI bits (0x60) preserved through FU-A per RFC 3984; nal_unit_type = 1
  const uint8_t nalHeader = 0x60 | 0x01; // non-IDR slice, nal_ref_idc=3
  std::vector<uint8_t> payload(3050);
  for (size_t i = 0; i < payload.size(); i++)
    payload[i] = (uint8_t)(i & 0xff);
  auto nal = MakeNal(nalHeader, payload);
  LoadFrame(frame, {nal});

  const unsigned maxPayload = 1000;
  auto packets = DrainPackets(frame, maxPayload);
  ASSERT_TRUE(packets.size() > 1); // must have actually fragmented

  std::vector<uint8_t> reassembled;
  uint8_t reconstructedNalType = 0, reconstructedNri = 0;
  for (size_t i = 0; i < packets.size(); i++) {
    RTPFrame rtp(packets[i].raw.data(), (int)packets[i].raw.size());
    uint8_t * p = rtp.GetPayloadPtr();
    unsigned len = rtp.GetPayloadSize();
    ASSERT_TRUE(len >= 2); // FU indicator + FU header
    bool startBit = (p[1] & 0x80) != 0;
    bool endBit   = (p[1] & 0x40) != 0;

    if (i == 0) {
      ASSERT_TRUE(startBit);
      ASSERT_FALSE(endBit);
      reconstructedNri = p[0] & 0x60;
      reconstructedNalType = p[1] & 0x1f;
    }
    else if (i + 1 == packets.size()) {
      ASSERT_FALSE(startBit);
      ASSERT_TRUE(endBit);
      ASSERT_TRUE(packets[i].marker);
    }
    else {
      ASSERT_FALSE(startBit);
      ASSERT_FALSE(endBit);
      ASSERT_FALSE(packets[i].marker);
      ASSERT_EQ(len, maxPayload); // every middle fragment fully packed
    }

    reassembled.insert(reassembled.end(), p + 2, p + len);
  }

  ASSERT_EQ(reconstructedNri, (uint8_t)(nalHeader & 0x60));
  ASSERT_EQ(reconstructedNalType, (uint8_t)(nalHeader & 0x1f));
  ASSERT_EQ(reassembled.size(), payload.size());
  ASSERT_EQ(memcmp(reassembled.data(), payload.data(), payload.size()), 0);
}

TEST(H264Frame, EncodeThenDecode_RoundTripsExactBytes) {
  H264Frame enc;
  auto sps = MakeNal(0x67, {0x42, 0xC0, 0x1E});
  auto pps = MakeNal(0x68, {0xCE, 0x3C, 0x80});
  std::vector<uint8_t> slicePayload(2500);
  for (size_t i = 0; i < slicePayload.size(); i++)
    slicePayload[i] = (uint8_t)((i * 7) & 0xff);
  auto idr = MakeNal(0x65, slicePayload);
  LoadFrame(enc, {sps, pps, idr});

  auto packets = DrainPackets(enc, 1000);
  ASSERT_TRUE(packets.size() > 3); // SPS + PPS + fragmented IDR

  H264Frame dec;
  unsigned flags = 0;
  for (auto & pkt : packets) {
    std::vector<uint8_t> buf = pkt.raw; // SetFromRTPFrame doesn't need a const buffer
    RTPFrame rtp(buf.data(), (int)buf.size());
    bool ok = dec.SetFromRTPFrame(rtp, flags);
    ASSERT_TRUE(ok);
  }

  ASSERT_TRUE(dec.IsSync());

  // Reconstruct what the original encoded picture "should" look like: each
  // NAL as 00 00 00 01 + header + payload, concatenated in order - which is
  // exactly what AddDataToEncodedFrame() normalises everything to,
  // regardless of whether the encoder side used 3- or 4-byte start codes.
  std::vector<uint8_t> expected;
  for (auto & nal : {sps, pps, idr})
    expected.insert(expected.end(), nal.begin(), nal.end());

  ASSERT_EQ(dec.GetFrameSize(), (uint32_t)expected.size());
  ASSERT_EQ(memcmp(dec.GetFramePtr(), expected.data(), expected.size()), 0);
}

TEST(H264Frame, IsSync_TrueForIDR_FalseForPSliceOnly) {
  H264Frame syncFrame;
  auto sps = MakeNal(0x67, {0x42, 0xC0, 0x1E});
  LoadFrame(syncFrame, {sps});
  ASSERT_TRUE(syncFrame.IsSync());

  H264Frame nonSyncFrame;
  auto pSlice = MakeNal(0x41, {0x01, 0x02, 0x03}); // nal_unit_type 1, non-IDR
  LoadFrame(nonSyncFrame, {pSlice});
  ASSERT_FALSE(nonSyncFrame.IsSync());
}

TEST(H264Frame, TimestampPropagatesToEveryFragment) {
  H264Frame frame;
  const uint8_t nalHeader = 0x61;
  std::vector<uint8_t> payload(2500, 0x42);
  auto nal = MakeNal(nalHeader, payload);
  LoadFrame(frame, {nal});
  frame.SetTimestamp(123456789);

  auto packets = DrainPackets(frame, 1000);
  ASSERT_TRUE(packets.size() > 1);
  for (auto & pkt : packets) {
    RTPFrame rtp(pkt.raw.data(), (int)pkt.raw.size());
    ASSERT_EQ(rtp.GetTimestamp(), (unsigned long)123456789);
  }
}

TEST(H264Frame, TruncatedSTAP_DoesNotCrash_ReturnsFalse) {
  H264Frame dec;
  // STAP-A (type 24) header, claims a NAL of length 2000 but the packet
  // is far shorter than that - must be rejected, not overrun the buffer.
  std::vector<uint8_t> buf(12 + 6, 0);
  buf[0] = 0x80;
  buf[1] = 0x80; // marker set, payload type irrelevant here
  RTPFrame rtp(buf.data(), (int)buf.size());
  rtp.SetPayloadSize(6);
  uint8_t * p = rtp.GetPayloadPtr();
  p[0] = 24;          // STAP-A NAL type
  p[1] = 0x07; p[2] = 0xD0; // claimed length 2000 (0x07D0), way bigger than available
  p[3] = 0x67; p[4] = 0xAA; p[5] = 0xBB;

  unsigned flags = 0;
  bool ok = dec.SetFromRTPFrame(rtp, flags);
  ASSERT_FALSE(ok);
}

int main() {
  return testfw::RunAll();
}
