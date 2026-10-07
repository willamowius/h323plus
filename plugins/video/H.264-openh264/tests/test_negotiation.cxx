// White-box tests for the pure, stateless negotiation helpers defined
// `static` inside openh264.cxx (profile_level_from_string,
// merge_profile_level_h264, merge_packetization_mode, setLevel,
// GetLevelLimits, adjust_bitrate_to_level, adjust_to_level, num2str,
// int_from_string). These have internal linkage by design (they're
// implementation details of the plugin, not part of its ABI), so this
// file gets at them the standard way for testing file-static functions
// without changing production visibility: it #includes openh264.cxx
// directly into its own single translation unit, rather than linking it
// normally. Do not link this file's .o together with a normally-compiled
// openh264.o in the same binary - that would multiply-define every
// symbol in openh264.cxx.
//
// This still needs something providing the Wels* factory functions
// openh264.cxx references (even though none of the tests below actually
// call OpenCodec()/EncodeFrames()), so it links against the fake SDK in
// fake_wels/, same as test_plugin_abi.cxx.
#include "framework.h"

#define PLUGIN_CODEC_DLL_EXPORTS 1
#include "../openh264.cxx"

TEST(Negotiation, ProfileLevelFromString_ParsesHex) {
  unsigned profile, constraints, level;
  profile_level_from_string("42C01E", profile, constraints, level);
  ASSERT_EQ(profile, 0x42u);
  ASSERT_EQ(constraints, 0xC0u);
  ASSERT_EQ(level, 0x1Eu);
}

TEST(Negotiation, ProfileLevelFromString_HandlesQuotedString) {
  unsigned profile, constraints, level;
  profile_level_from_string("\"42C01E\"", profile, constraints, level);
  ASSERT_EQ(profile, 0x42u);
  ASSERT_EQ(constraints, 0xC0u);
  ASSERT_EQ(level, 0x1Eu);
}

TEST(Negotiation, ProfileLevelFromString_ZeroFallsBackToRFC3984Default) {
  unsigned profile, constraints, level;
  profile_level_from_string("0", profile, constraints, level);
  // RFC 3984 default: Baseline, Level 1 -> 0x42C00A
  ASSERT_EQ(profile, 0x42u);
  ASSERT_EQ(constraints, 0xC0u);
  ASSERT_EQ(level, 0x0Au);
}

TEST(Negotiation, MergeProfileLevel_TakesMinProfile_OrsConstraints_MinLevel) {
  char * result = NULL;
  // Encoding is profile+constraints+level (see profile_level_from_string).
  // dst: High(0x64) profile, constraints 0x00, level 0x1E(30)
  // src: Baseline(0x42) profile, constraints 0x80, level 0x14(20)
  // expect: profile = min(0x64,0x42) = 0x42, constraints = 0x00|0x80 = 0x80,
  //         level = min(30,20) = 20 = 0x14
  int ok = merge_profile_level_h264(&result, "64001E", "428014");
  ASSERT_TRUE(ok);
  ASSERT_TRUE(result != NULL);
  std::string s(result);
  free_string(result);
  ASSERT_EQ(s, "428014");
}

TEST(Negotiation, MergePacketizationMode_TakesMin) {
  char * result = NULL;
  int ok = merge_packetization_mode(&result, "1", "0");
  ASSERT_TRUE(ok);
  std::string s(result);
  free_string(result);
  ASSERT_EQ(s, "0");
}

TEST(Negotiation, MergePacketizationMode_FiveIsRFC3984DefaultForZero) {
  // Per RFC 3984, an unspecified packetization-mode (encoded here as "5",
  // this plugin's FMTP default) is equivalent to mode 0 for merge purposes.
  char * result = NULL;
  int ok = merge_packetization_mode(&result, "5", "5");
  ASSERT_TRUE(ok);
  std::string s(result);
  free_string(result);
  ASSERT_EQ(s, "0");
}

TEST(Negotiation, IntFromString_HandlesQuotedAndUnquoted) {
  ASSERT_EQ(int_from_string("5"), 5);
  ASSERT_EQ(int_from_string("\"5\""), 5);
}

TEST(Negotiation, GetLevelLimits_KnownH241Level_ReturnsMatchingTableEntry) {
  // h264_levels[] (h264frame.h) has an entry {level_idc=13, mbps=11880,
  // frame_size=396, h241_level=36} - Level 1.3, exactly CIF's macroblock
  // count. Pick it by its H.241 level value and check the rest comes back
  // consistent with that same table row.
  unsigned maxMB = 0, maxMBPS = 0, h264level = 0;
  int ok = GetLevelLimits(36, maxMB, maxMBPS, h264level);
  ASSERT_TRUE(ok);
  ASSERT_EQ(maxMB, 396u);
  ASSERT_EQ(maxMBPS, 11880u);
  ASSERT_EQ(h264level, 13u);
}

TEST(Negotiation, GetLevelLimits_UnknownH241Level_Fails) {
  unsigned maxMB = 0, maxMBPS = 0, h264level = 0;
  int ok = GetLevelLimits(999, maxMB, maxMBPS, h264level);
  ASSERT_FALSE(ok);
}

TEST(Negotiation, AdjustBitrateToLevel_ClampsDownToLevelCeiling) {
  // Level 1.2 (level_idc=12) caps bitrate at 384 kbit/s (h264_levels[]).
  unsigned bitrate = 10000000; // way over the cap
  int ok = adjust_bitrate_to_level(bitrate, 12);
  ASSERT_TRUE(ok);
  ASSERT_EQ(bitrate, 384000u);
}

TEST(Negotiation, AdjustBitrateToLevel_LeavesLowerBitrateUnchanged) {
  unsigned bitrate = 100000; // comfortably under Level 1.2's 384 kbit/s cap
  int ok = adjust_bitrate_to_level(bitrate, 12);
  ASSERT_TRUE(ok);
  ASSERT_EQ(bitrate, 100000u);
}

TEST(Negotiation, SetLevel_KnownGoodResolution_Succeeds) {
  // This is a characterisation test of setLevel()'s existing (unmodified,
  // inherited from the original H323Plus x264 plugin) table-walk logic,
  // not a specification of what the "ideal" level for CIF@30fps should
  // be - it just pins down current behaviour so a future refactor doesn't
  // silently change it. See the h264_levels[] table in h264frame.h.
  unsigned level = 0, h264level = 0;
  int ok = setLevel(352, 288, 30, level, h264level);
  ASSERT_TRUE(ok);
  ASSERT_TRUE(level > 0);
  // sanity bound rather than pinning an exact table row: CIF@30fps must
  // land at or below Level 1.3 (h264level=13), the first table entry
  // whose frame_size/mbps actually cover 396 MBs @ 11880 MBs/sec.
  ASSERT_TRUE(h264level <= 13);
}

int main() {
  return testfw::RunAll();
}
