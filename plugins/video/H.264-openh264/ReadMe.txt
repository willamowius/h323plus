OpenH264 H.264 video plugin for H323Plus
=========================================

This is based on OPAL's OpenH264 plugin and H323Plus H.264
(x264/ffmpeg) plugin.

To compile, you need the OpenH264 SDK version 2.4.x (libopenh264-dev package
on Ubuntu 24.04 or install from https://github.com/cisco/openh264).
OpenH264 2.6.x should also work.

Unit tests
----------
tests/ holds a number of mostly self-contained unit tests.
Build and run all self-contained tests:
    cd tests && make check

Individual test targets:
	make test_h264frame     # RTP (de)packetisation
	make test_negotiation   # profile/level/bitrate/packetization-mode parsing
	make test_plugin_abi    # createCodec()/codecFunction()/codecControls[]
	make test_invalid_input # checking of malformed input
    make check-real         # run checks against the real OpenH264 library, feeding the plugin output as input to the plugin

Running the unit tests under Valgrind:
    make valgrind           # test_h264frame/test_negotiation/test_plugin_abi/test_invalid_input
    make valgrind-real      # test_real_openh264 (needs libopenh264-dev; slow)

Implementation notes:

* All NAL units from every OpenH264 SLayerInfo entry are collected for
  each encoded picture, not just entry 0. OpenH264 commonly splits one
  picture's output across multiple layers even with SVC scalability fully
  disabled - typically a pseudo-layer for SPS/PPS and a separate one for
  the coded slice - so EncodeFrames() flattens bitstream.sLayerInfo[0..
  iLayerNum-1] into one NAL list before handing it to H264Frame. (An
  earlier version of this conversion only read layer 0, which happened to
  work when everything landed in one layer but silently sent SPS/PPS-only
  "pictures" - no actual slice data - whenever the encoder split layers,
  which is common at lower resolutions/bitrates. Any future change here
  should keep iterating every layer rather than assuming iLayerNum == 1.)

* Profile/level *is* pushed into OpenH264's SEncParamExt
  (uiProfileIdc/uiLevelIdc), via a direct cast from H264EncoderContext's
  _profile/_level. These already sit on the same numeric scale OpenH264's
  EProfileIdc/ELevelIdc enums use (both are just literal H.264
  profile_idc/level_idc values - H264_BASE_IDC=66 == PRO_BASELINE,
  h264_levels[].h264level e.g. 30 == LEVEL_3_0), so no real mapping table
  is needed. Getting this wrong (i.e. leaving it at the encoder's default)
  can produce an SPS whose profile/level doesn't match what was actually
  negotiated in H.245/SDP, which some receivers will refuse to decode at
  all - if you extend the capability set (e.g. add Main/High profile
  entries to openh264.h), double check the negotiated value is what
  actually reaches ApplyOptions() via encoder_set_options().

* RTP timestamps on the encode side are generated internally: a running
  90kHz counter (H264EncoderContext::_timestamp) advances by one frame
  period each time a picture is encoded (or skipped), and is written into
  H264Frame before packetisation, matching what the codec is expected to
  supply. Sequence number, SSRC and payload type are NOT touched by this
  plugin - dst arrives from the RTP session with those already populated.

