#
# Makefile for GSM 06.10
#
# Copyright (C) 2004 Post Increment, All Rights Reserved
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is Open H323 library.
#
# The Initial Developer of the Original Code is Post Increment
#
# Contributor(s): ______________________________________.
#
# $Log$
# Revision 1.4  2007/09/02 12:03:19  dominance
# * dynamically load libx264 at runtime on *nix
# * Improved error handling
#
# Patch received from and thus thanks goes to Matthias Schneider.
#
# Revision 1.3  2007/08/23 19:20:15  dominance
# fix compilation of H.264 if CFLAGS is present from make
#
# Revision 1.2  2007/08/22 21:51:26  dsandras
# Applied patch from Matthias Schneider <ma30002000 yahoo de> to share
# things between codecs. Huge thanks!
#
# Revision 1.1  2007/06/16 21:37:00  dsandras
# Added H.264 support thanks to Matthias Schneider <ma30002000 yahoo de>.
# Thanks a lot !
#
# Baseline Profile:
# no B-frames
# We make use of the baseline profile (which is the designated profile for interactive vide) ,
# that means:
# no B-Frames (too much latency in interactive video)
# CBR (we want to get the max. quality making use of all the bitrate that is available)
# We allow one exeption: configuring a bitrate of > 786 kbit/s
#
# This plugin implements
# - Single Time Aggregation Packets A
# - Single NAL units
# - Fragmentation Units
# like described in RFC3984
#
# It requires x264 and ffmpeg.
#
# Revision 1.5  2007/04/19 06:09:04  csoutheren
# Fixed plugin build system to use better detection for OSX
# Resolved 1636262 - Fix for audio plugins for OSX
#
# Revision 1.4  2006/10/10 01:42:55  csoutheren
# Applied patches from Ekiga bug 360869 - fix OpenSolaris build problems
# Thanks to Boying Lu
#
# Revision 1.3  2006/08/20 00:32:25  csoutheren
# Added missing files to Makefile
#
# Revision 1.2  2006/08/12 10:59:14  rjongbloed
# Added Linux build for H.261 plug-in.
#

VC_PLUGIN_DIR=opal-3.5.2/codecs/video
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
target_os=mingw32
WIN32=yes

SHAREDDIR	= ../shared
COMMONDIR       = ../../common
PLUGINDIR       = ../../../
OBJDIR	= ./obj

BASENAME=h264
SONAME	=$(BASENAME)

CC	 	=gcc
CFLAGS	 	= -Os 
CXX	 	=cl.exe
PLUGINEXT	=dll
STDCCFLAGS	=
LDFLAGS		=
X264_CFLAGS     =
X264_LIBS	=
X264_LINK_STATIC=yes
DL_LIBS	=

EXTRACCFLAGS	+= $(X264_CFLAGS) -I$(COMMONDIR) -I$(PLUGINDIR)

vpath	%.cxx $(SHAREDDIR)
vpath	%.cxx $(COMMONDIR)
vpath	%.o   $(OBJDIR)

SRCS   += $(SHAREDDIR)/h264frame.cxx \
	  $(COMMONDIR)/trace.cxx \
	  enc-ctx.cxx  \


ifeq ($(WIN32),yes)
SRCS    += h264helper_win32.cxx
PLUGIN	= ./$(BASENAME)_video_pwplugin_helper.exe

else
SRCS    += h264helper_unix.cxx
PLUGIN	= ./$(BASENAME)_video_pwplugin_helper

ifneq ($(X264_LINK_STATIC),yes)
SRCS   += x264loader_unix.cxx
endif

X264_LIBS	+= $(DL_LIBS)

endif




$(OBJDIR)/%.o : %.cxx
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(CXX) -I../../../include -I.. $(EXTRACCFLAGS) $(STDCCFLAGS) $(OPTCCFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o : %.c
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(CC) -I../../../include  -I.. $(EXTRACCFLAGS) $(STDCCFLAGS) $(OPTCCFLAGS) $(CFLAGS) -c $< -o $@


OBJECTS = $(addprefix $(OBJDIR)/,$(patsubst %.c,%.o,$(patsubst %.cxx,%.o,$(notdir $(SRCS)))))

$(PLUGIN): $(OBJECTS)
ifeq (,$(findstring solaris,$(target_os)))
	$(CXX) -o $@ $^ $(X264_LIBS)
else
	$(CXX) $@ -o $@ $^ $(X264_LIBS)
endif

install:
	mkdir -p $(DESTDIR)$(libdir)/$(VC_PLUGIN_DIR)
	install $(PLUGIN) $(DESTDIR)$(libdir)/$(VC_PLUGIN_DIR)

uninstall:
	rm -f $(DESTDIR)$(libdir)/$(VC_PLUGIN_DIR)/$(PLUGIN)

clean:
	rm -f $(OBJECTS) $(PLUGIN)

###########################################
