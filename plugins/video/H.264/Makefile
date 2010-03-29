#
# Makefile for H.264
#
# Copyright (C) 2010 Post Increment, All Rights Reserved
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
#
# The Original Code is Open H323 library.
#
# The Initial Developer of the Original Code is Post Increment
#
# Contributor(s): ______________________________________.
#
# $Log$
# Revision 1.1  2010/02/24 02:19:43  shorne
# First commit of h323plus mirror
#

VC_PLUGIN_DIR=h323plus/plugins/video
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
target_os=mingw32
DL_LIBS =

SHAREDDIR	= ./shared
COMMONDIR	= ../common
PLUGINDIR	= ../../
OBJDIR	= ./obj
WIN32=yes
SUBDIRS		+= gpl

BASENAME=h264
SONAME	=$(BASENAME)

CC	 	=gcc
CFLAGS	 	= -Os 
CXX	 	=cl.exe
LDSO	 	=-module -avoid-version -shared
PLUGINEXT	=dll
STDCCFLAGS	=
LDFLAGS		=
LIBAVCODEC_CFLAGS=

EXTRACCFLAGS    +=  $(LIBAVCODEC_CFLAGS) -I$(COMMONDIR) -I$(PLUGINDIR) -DLIB_DIR='"$(libdir)"' -DVC_PLUGIN_DIR='"$(VC_PLUGIN_DIR)"' -DLICENCE_MPL

vpath	%.cxx $(SHAREDDIR)
vpath	%.cxx $(COMMONDIR)
vpath	%.o   $(OBJDIR)

SRCS    += $(SHAREDDIR)/h264frame.cxx \
           h264-x264.cxx \
           $(COMMONDIR)/dyna.cxx \
           $(COMMONDIR)/trace.cxx

ifeq ($(WIN32),yes)
SRCS    += h264pipe_win32.cxx
else
SRCS    += h264pipe_unix.cxx
endif


$(OBJDIR)/%.o : %.cxx
	@mkdir -p $(OBJDIR) >/dev/null 2>&1
	$(CXX) -I../../../include $(EXTRACCFLAGS) $(STDCCFLAGS) $(OPTCCFLAGS) $(CFLAGS) -c $< -o $@

PLUGIN	= ./$(BASENAME)_video_pwplugin.$(PLUGINEXT)
STATIC	= ./lib$(BASENAME)_video_s.a

OBJECTS = $(addprefix $(OBJDIR)/,$(patsubst %.c,%.o,$(patsubst %.cxx,%.o,$(notdir $(SRCS)))))

$(PLUGIN): $(OBJECTS)
	@set -e; $(foreach dir,$(SUBDIRS),if test -d ${dir} ; then $(MAKE) -C $(dir); fi ; )
ifeq (,$(findstring solaris,$(target_os)))
	$(CXX) $(LDSO) $(DL_LIBS) -o $@ $^
else
	$(CXX) $(LDSO) $(DL_LIBS) $@ -o $@ $^
endif


install:
	@set -e; $(foreach dir,$(SUBDIRS),if test -d ${dir} ; then $(MAKE) -C $(dir) install; fi ; )
	mkdir -p $(DESTDIR)$(libdir)/$(VC_PLUGIN_DIR)
	install $(PLUGIN) $(DESTDIR)$(libdir)/$(VC_PLUGIN_DIR)

uninstall:
	@set -e; $(foreach dir,$(SUBDIRS),if test -d ${dir} ; then $(MAKE) -C $(dir) uninstall; fi ; )
	rm -f $(DESTDIR)$(libdir)/$(VC_PLUGIN_DIR)/$(PLUGIN)

clean:
	rm -f $(OBJECTS) $(PLUGIN)
	@set -e; $(foreach dir,$(SUBDIRS),if test -d ${dir} ; then $(MAKE) -C $(dir) clean; fi ; )

###########################################
                        