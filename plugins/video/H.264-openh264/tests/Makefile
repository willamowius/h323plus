# Standalone test-suite Makefile for the OpenH264 H323Plus plugin.
#
# This is intentionally self-contained: it does not depend on the plugin
# having been built or configured via ../configure.ac, and it does not
# link against the real libopenh264 - it links the plugin's own real,
# unmodified openh264.cxx/h264frame.cxx against a small fake OpenH264 SDK
# (fake_wels/) that stands in for libopenh264, so the tests are fast,
# deterministic, and need nothing installed beyond a C++11 compiler.
#
# Set the H323PLUSDIR environment variable to your H323Plus checkout
# before building - it is not guessed from a relative path:
#
#   export H323PLUSDIR=/path/to/h323plus
#   make check
#
# or inline:
#
#   H323PLUSDIR=/path/to/h323plus make check
#
#   make          # build all three test binaries
#   make check    # build and run all three, fail the build if any fail
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -g -pthread

# H323PLUSDIR is picked up from the environment automatically (make
# imports the environment as variables); ?= here only supplies it if
# something set it on the command line but not the environment.
H323PLUSDIR ?=

ifeq ($(strip $(H323PLUSDIR)),)
$(error H323PLUSDIR is not set - export H323PLUSDIR=/path/to/h323plus, or pass H323PLUSDIR=... on the make command line)
endif

COMMON_DIR = $(H323PLUSDIR)/plugins/video/common
INCLUDE_DIR = $(H323PLUSDIR)/include

CPPFLAGS = -I. -I.. -I$(INCLUDE_DIR) -I$(COMMON_DIR) -Ifake_wels

PLUGIN_DIR = ..

.PHONY: all check check-real valgrind valgrind-real clean

all: test_h264frame test_negotiation test_plugin_abi test_invalid_input

# --- test_h264frame: pure H264Frame packetiser tests, no fake SDK needed ---
test_h264frame: test_h264frame.cxx $(PLUGIN_DIR)/h264frame.cxx $(PLUGIN_DIR)/h264frame.h $(COMMON_DIR)/trace.cxx framework.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) test_h264frame.cxx $(PLUGIN_DIR)/h264frame.cxx $(COMMON_DIR)/trace.cxx -o $@

# --- test_negotiation: white-box #include of openh264.cxx (static helpers) ---
test_negotiation: test_negotiation.cxx $(PLUGIN_DIR)/openh264.cxx $(PLUGIN_DIR)/openh264.h $(PLUGIN_DIR)/h264frame.cxx $(COMMON_DIR)/trace.cxx fake_wels/fake_openh264.cxx framework.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) test_negotiation.cxx $(PLUGIN_DIR)/h264frame.cxx $(COMMON_DIR)/trace.cxx fake_wels/fake_openh264.cxx -o $@

# --- test_plugin_abi: drives the real plugin ABI through the fake SDK ---
test_plugin_abi: test_plugin_abi.cxx $(PLUGIN_DIR)/openh264.cxx $(PLUGIN_DIR)/openh264.h $(PLUGIN_DIR)/h264frame.cxx $(COMMON_DIR)/trace.cxx fake_wels/fake_openh264.cxx framework.h fake_wels/fake_control.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) test_plugin_abi.cxx $(PLUGIN_DIR)/openh264.cxx $(PLUGIN_DIR)/h264frame.cxx $(COMMON_DIR)/trace.cxx fake_wels/fake_openh264.cxx -o $@

# --- test_invalid_input: malformed/malicious input rejection, both at
# the H264Frame level and through the full plugin ABI (fake SDK) ---
test_invalid_input: test_invalid_input.cxx $(PLUGIN_DIR)/openh264.cxx $(PLUGIN_DIR)/openh264.h $(PLUGIN_DIR)/h264frame.cxx $(PLUGIN_DIR)/h264frame.h $(COMMON_DIR)/trace.cxx fake_wels/fake_openh264.cxx framework.h fake_wels/fake_control.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) test_invalid_input.cxx $(PLUGIN_DIR)/openh264.cxx $(PLUGIN_DIR)/h264frame.cxx $(COMMON_DIR)/trace.cxx fake_wels/fake_openh264.cxx -o $@

check: all
	@echo "=== test_h264frame ===";     ./test_h264frame
	@echo "=== test_negotiation ===";   ./test_negotiation
	@echo "=== test_plugin_abi ===";    ./test_plugin_abi
	@echo "=== test_invalid_input ==="; ./test_invalid_input

# --- test_real_openh264: integration tests against the REAL OpenH264 ---
# library (not fake_wels/) - includes feeding the plugin encoder's own
# output straight into the plugin decoder. Requires libopenh264 headers/
# library at build time (Debian/Ubuntu: apt install libopenh264-dev; or
# a local build of https://github.com/cisco/openh264 with
# REAL_OPENH264_CFLAGS/REAL_OPENH264_LIBS set manually below). Not part
# of `all`/`check`/the default build - a machine without the real SDK
# installed shouldn't fail the fast, deterministic fake-SDK-based suite.
REAL_OPENH264_CFLAGS := $(shell pkg-config --cflags openh264 2>/dev/null)
REAL_OPENH264_LIBS   := $(shell pkg-config --libs openh264 2>/dev/null || echo -lopenh264)

test_real_openh264: test_real_openh264.cxx $(PLUGIN_DIR)/openh264.cxx $(PLUGIN_DIR)/openh264.h $(PLUGIN_DIR)/h264frame.cxx $(PLUGIN_DIR)/h264frame.h $(COMMON_DIR)/trace.cxx framework.h
	$(CXX) $(CXXFLAGS) -I. -I.. -I$(INCLUDE_DIR) -I$(COMMON_DIR) $(REAL_OPENH264_CFLAGS) \
	  test_real_openh264.cxx $(PLUGIN_DIR)/openh264.cxx $(PLUGIN_DIR)/h264frame.cxx $(COMMON_DIR)/trace.cxx \
	  $(REAL_OPENH264_LIBS) -o $@

check-real: test_real_openh264
	@echo "=== test_real_openh264 (real libopenh264) ==="; ./test_real_openh264

# --- valgrind / valgrind-real: run the same suites under Valgrind's
# memcheck. Reads suppressions from $(H323PLUSDIR)/ptlib.supp, which
# this Makefile does not ship - add it to your H323Plus checkout before
# running these targets, the same way H323PLUSDIR itself is supplied.
# valgrind is split from valgrind-real the same way check/check-real
# are: memcheck typically adds a large constant overhead per process,
# and test_real_openh264 now covers resolutions up to 4K (see
# kCommonFormats), so valgrind-real can take a long time - run it
# deliberately, not as part of routine `make valgrind`.
VALGRIND ?= valgrind
VALGRIND_SUPP = $(H323PLUSDIR)/ptlib.supp
VALGRIND_FLAGS ?= --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=97

define CHECK_VALGRIND_SUPP
	@if [ ! -f "$(VALGRIND_SUPP)" ]; then \
	  echo "error: suppression file not found: $(VALGRIND_SUPP)"; \
	  echo "Add ptlib.supp to $(H323PLUSDIR) before running this target."; \
	  exit 1; \
	fi
endef

valgrind: all
	$(CHECK_VALGRIND_SUPP)
	@for t in test_h264frame test_negotiation test_plugin_abi test_invalid_input; do \
	  echo "=== valgrind: $$t ==="; \
	  $(VALGRIND) $(VALGRIND_FLAGS) --suppressions=$(VALGRIND_SUPP) ./$$t || exit 1; \
	done

valgrind-real: test_real_openh264
	$(CHECK_VALGRIND_SUPP)
	@echo "=== valgrind: test_real_openh264 (real libopenh264 - this will be slow) ==="
	$(VALGRIND) $(VALGRIND_FLAGS) --suppressions=$(VALGRIND_SUPP) ./test_real_openh264

clean:
	rm -f test_h264frame test_negotiation test_plugin_abi test_invalid_input test_real_openh264
