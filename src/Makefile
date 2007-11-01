#
# Makefile
#
# Make file for Open H323 library
#
# Copyright (c) 1998-2000 Equivalence Pty. Ltd.
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
# The Original Code is Open H323 Library.
#
# The Initial Developer of the Original Code is Equivalence Pty. Ltd.
#
# Contributor(s): ______________________________________.
#
# $Log$
# Revision 1.3  2007/10/31 09:52:35  shorne
# added missing reference to H460.9
#
# Revision 1.2  2007/10/31 08:37:01  shorne
# removed unused reference
#
# Revision 1.1  2007/08/06 20:51:02  shorne
# First commit of h323plus
#
# Revision 1.136.2.5  2007/08/02 20:11:57  shorne
# Added H.341 Support
#
# Revision 1.136.2.4  2007/08/01 18:46:35  shorne
# fixed typo
#
# Revision 1.136.2.3  2007/07/20 22:03:26  shorne
# Initial H.350 Support
#
# Revision 1.136.2.2  2007/01/30 09:03:31  shorne
# Fixed H450 error and removed missed VIC files
#
# Revision 1.136.2.1  2006/12/23 19:08:02  shorne
# Plugin video codecs & sundry
#
# Revision 1.136  2006/06/27 12:35:03  csoutheren
# Patch 1366328 - Support for H.450.3 divertingLegInformation2
# Thanks to Norbert Bartalsky
#
# Revision 1.135  2006/06/23 07:06:04  csoutheren
# Fixed linux compile
#
# Revision 1.134  2006/06/23 03:18:23  shorne
# Backport FECC (H.224) from Opal
#
# Revision 1.133  2006/05/30 11:15:54  hfriederich
# Enable to build H.460
#
# Revision 1.132  2005/06/07 07:59:11  csoutheren
# Applied patch 1176459 for PocketPC. Thanks to Matthias Weber
#
# Revision 1.131  2005/01/03 14:03:21  csoutheren
# Added new configure options and ability to disable/enable modules
#
# Revision 1.130  2005/01/03 06:25:53  csoutheren
# Added extensive support for disabling code modules at compile time
#
# Revision 1.129  2004/12/08 05:16:14  csoutheren
# Fixed OSP compilation on Linux
#
# Revision 1.128  2004/06/10 13:16:31  csoutheren
# Added configure option to remove ASN tracing
#
# Revision 1.127  2004/05/18 22:27:35  csoutheren
# Initial support for embedded codecs
#
# Revision 1.126  2004/05/10 12:44:43  csoutheren
# Removed G.726 and IMA ADPCM codecs
#
# Revision 1.125  2004/05/04 12:50:55  csoutheren
# Having a bad day with endifs
#
# Revision 1.124  2004/05/04 12:46:42  csoutheren
# Moved LPC-10 to plugins
#
# Revision 1.123  2004/05/04 00:06:57  csoutheren
# Restored deleted endif
#
# Revision 1.122  2004/05/04 00:05:53  csoutheren
# Removed usage of g729codec.cxx
#
# Revision 1.121  2004/05/02 13:36:31  csoutheren
# Removed GSM as this is now a plugin
#
# Revision 1.120  2004/04/25 01:52:47  rjongbloed
# Fixed GCC 3.4 warnings
#
# Revision 1.119  2004/04/22 22:35:00  csoutheren
# Fixed mispelling of Guilhem Tardy - my apologies to him
#
# Revision 1.118  2004/04/22 14:22:20  csoutheren
# Added RFC 2190 H.263 code as created by Guilhem Tardy and AliceStreet
# Many thanks to them for their contributions.
#
# Revision 1.117  2004/04/18 21:49:11  ykiryanov
# Ifdef'd inclusion of ixjunix for BeOS platform
#
# Revision 1.116  2004/04/14 11:40:05  csoutheren
# Updated to detect system GSM library
#
# Revision 1.115  2004/04/11 13:27:25  csoutheren
# Added plugin manager to Makefile
#
# Revision 1.114  2004/04/04 13:21:22  csoutheren
# Remove requirement for ASN parser at compile time by adding in
# pre-processed versions of all ASN files
# Removed iLBC and Speex as these are now plugin codecs
#
# Revision 1.113  2004/04/03 10:38:25  csoutheren
# Added in initial cut at codec plugin code. Branches are for wimps :)
#
# Revision 1.112.2.1  2004/03/31 11:50:25  csoutheren
# Removed references to builtin iLBC as this has been moved to plugins
#
# Revision 1.112  2004/03/22 22:11:11  csoutheren
# Fixed typo preventing compliltion on x86-64 thanks to Klaus Kaempf
#
# Revision 1.111  2004/02/26 11:27:36  csoutheren
# Added extra flag for compiled C files, thanks to Klaus Kaempf
#
# Revision 1.110  2004/01/31 13:13:22  csoutheren
# Fixed problem with HAS_IXJ being tested but not included
#
# Revision 1.109  2003/12/29 12:13:26  csoutheren
# configure now checks for libspeex in system libraries and compares
# version against version in local sources. Also use --enable-localspeex to
# force use of local Speex or system Speex
#
# Revision 1.108  2003/08/04 00:03:29  dereksmithies
# Reorganise tests for H263 codecs
#
# Revision 1.107  2003/07/29 12:34:18  csoutheren
# Changed to ensure dynamic libraries dynamically link pwlib
#
# Revision 1.106  2003/07/26 03:54:30  dereksmithies
# Fixes for New H263 support from ffmpeg and Vic H263
#
# Revision 1.105  2003/06/06 03:55:52  rjongbloed
# Added missing iLBC file.
#
# Revision 1.104  2003/06/06 02:37:05  rjongbloed
# Fixed iLBC build
#
# Revision 1.103  2003/06/06 02:19:04  rjongbloed
# Added iLBC codec
#
# Revision 1.102  2003/05/14 20:37:29  csoutheren
# Changed for new AVC
#
# Revision 1.101  2003/05/05 12:14:03  robertj
# Removed internal ldap as now use OpenLDAP
#

ifndef OPENH323DIR
OPENH323DIR=$(CURDIR)/..
endif

LIBRARY_MAKEFILE:=1

include $(OPENH323DIR)/openh323u.mak


OH323_OBJDIR = $(OH323_LIBDIR)/$(PT_OBJBASE)
ifdef NOTRACE
OH323_OBJDIR := $(OH323_OBJDIR)n
endif


OBJDIR	=	$(OH323_OBJDIR)
LIBDIR	=	$(OH323_LIBDIR)
TARGET	=	$(LIBDIR)/$(OH323_FILE)
VERSION_FILE =	$(OPENH323DIR)/version.h

########################################

SOURCES		+= \
		   $(OH323_SRCDIR)/h225_1.cxx \
		   $(OH323_SRCDIR)/h225_2.cxx \
		   $(OH323_SRCDIR)/h245_1.cxx \
		   $(OH323_SRCDIR)/h245_2.cxx \
		   $(OH323_SRCDIR)/h245_3.cxx 

HEADER_FILES	+= $(OH323_INCDIR)/h323.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323.cxx $(OH323_SRCDIR)/h323ep.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h323neg.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323neg.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h323pdu.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323pdu.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h323rtp.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323rtp.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h323caps.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323caps.cxx
HEADER_FILES	+= $(OH323_INCDIR)/q931.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/q931.cxx
HEADER_FILES	+= $(OH323_INCDIR)/codecs.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/codecs.cxx
HEADER_FILES	+= $(OH323_INCDIR)/channels.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/channels.cxx
HEADER_FILES	+= $(OH323_INCDIR)/transports.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/transports.cxx
HEADER_FILES	+= $(OH323_INCDIR)/rtp.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/rtp.cxx
HEADER_FILES	+= $(OH323_INCDIR)/gkclient.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/gkclient.cxx
HEADER_FILES	+= $(OH323_INCDIR)/gkserver.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/gkserver.cxx
HEADER_FILES	+= $(OH323_INCDIR)/mediafmt.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/mediafmt.cxx
HEADER_FILES	+= $(OH323_INCDIR)/guid.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/guid.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h225ras.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h225ras.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h323trans.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323trans.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h235auth.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h235auth.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h235pluginmgr.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h235pluginmgr.cxx
HEADER_FILES	+= $(OH323_INCDIR)/rfc2833.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/rfc2833.cxx

ifdef H323_H224
HEADER_FILES	+= $(OH323_INCDIR)/h224.h
HEADER_FILES	+= $(OH323_INCDIR)/h224handler.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h224.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h281.h
HEADER_FILES	+= $(OH323_INCDIR)/h281handler.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h281.cxx
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323h224.cxx
COMMON_SOURCES	+= $(OH323_SRCDIR)/q922.cxx
endif


ifdef HAS_PLUGINS
HEADER_FILES	+= $(OH323_INCDIR)/h323pluginmgr.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323pluginmgr.cxx
endif

ifdef HAS_WAVFILE
HEADER_FILES	+= $(OH323_INCDIR)/opalwavfile.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/opalwavfile.cxx
HEADER_FILES	+= $(OH323_INCDIR)/rtp2wav.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/rtp2wav.cxx
endif

ifdef HAS_VXML
HEADER_FILES	+= $(OH323_INCDIR)/opalvxml.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/opalvxml.cxx
endif

########### H.230

ifdef H323_H230
HEADER_FILES	+= $(OH323_INCDIR))/h230/h230.h \
                     $(OH323_INCDIR))/h230/OID2.h
COMMON_SOURCES	+= $(OH323_SRCDIR))/h230/h230.cxx \
                     $(OH323_INCDIR))/h230/OID2.cxx
endif

########### H.248

ifdef H323_H248
SOURCES    	+= $(OH323_SRCDIR)/h248.cxx 
HEADER_FILES	+= $(OH323_INCDIR)/svcctrl.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/svcctrl.cxx
endif

########### H.341

ifdef H323_H341
HEADER_FILES    += $(OH323_INCDIR)/h341/h341.h 
COMMON_SOURCES  += $(OH323_SRCDIR)/h341/h341.cxx 
endif

########### H.350

ifdef H323_H350
HEADER_FILES    += $(OH323_INCDIR)/h350/h350.h \
                   $(OH323_INCDIR)/h350/h350_service.h

COMMON_SOURCES  += $(OH323_SRCDIR)/h350/h350.cxx \
                   $(OH323_INCDIR)/h350/h350_service.cxx
endif

########### H.450

ifdef H323_H450
SOURCES    += \
             $(OH323_SRCDIR)/x880.cxx \
             $(OH323_SRCDIR)/h450/h4501.cxx \
             $(OH323_SRCDIR)/h450/h4502.cxx \
             $(OH323_SRCDIR)/h450/h4503.cxx \
             $(OH323_SRCDIR)/h450/h4504.cxx \
             $(OH323_SRCDIR)/h450/h4505.cxx \
             $(OH323_SRCDIR)/h450/h4506.cxx \
             $(OH323_SRCDIR)/h450/h4507.cxx \
             $(OH323_SRCDIR)/h450/h4508.cxx \
             $(OH323_SRCDIR)/h450/h4509.cxx \
             $(OH323_SRCDIR)/h450/h45010.cxx \
             $(OH323_SRCDIR)/h450/h45011.cxx 

HEADER_FILES	+= $(OH323_INCDIR)/h450/h450pdu.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h450/h450pdu.cxx
endif

########### H.460

ifdef H323_H460
HEADER_FILES    += $(OH323_INCDIR)/h460/h460.h \
                   $(OH323_INCDIR)/h460/h4601.h \
                   $(OH323_INCDIR)/h460/h4609.h

COMMON_SOURCES  += $(OH323_SRCDIR)/h460/h4601.cxx \
                   $(OH323_SRCDIR)/h460/h4609.cxx
endif 

########### H.501

ifdef H323_H501
SOURCES    	+= $(OH323_SRCDIR)/h501.cxx 
HEADER_FILES	+= $(OH323_INCDIR)/peclient.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/peclient.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h323annexg.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323annexg.cxx
HEADER_FILES	+= $(OH323_INCDIR)/h501pdu.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h501pdu.cxx
endif

########### T.120

ifdef H323_T120
HEADER_FILES	+= $(OH323_INCDIR)/h323t120.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323t120.cxx
HEADER_FILES	+= $(OH323_INCDIR)/t120proto.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/t120proto.cxx
endif

########## H.235

ifndef OH323_SUPPRESS_H235
SOURCES		+= $(OH323_SRCDIR)/h235.cxx 
endif

ifdef HAS_OPENSSL
COMMON_SOURCES	+= $(OH323_SRCDIR)/h235auth1.cxx
endif

########### FAX

ifdef H323_T38
SOURCES		+= $(OH323_SRCDIR)/t38.cxx 
HEADER_FILES	+= $(OH323_INCDIR)/h323t38.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/h323t38.cxx
HEADER_FILES	+= $(OH323_INCDIR)/t38proto.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/t38proto.cxx
endif

########### AUDIO

ifndef NOAUDIOCODECS

HEADER_FILES	+= $(OH323_INCDIR)/jitter.h
COMMON_SOURCES	+= $(OH323_SRCDIR)/jitter.cxx

COMMON_SOURCES	+= $(OH323_SRCDIR)/g711.c

endif # NOAUDIOCODECS

########### VIDEO

ifndef NOVIDEO
##### NOW PLUGINS
endif # NOVIDEO

CLEAN_FILES	= $(OH323_LIB) 

ifeq ($(NOTRACE), 0)
STDCCFLAGS += -DPTRACING
endif


VPATH_CXX := $(OH323_SRCDIR) \
             $(OH323_SRCDIR)/h341 \
             $(OH323_SRCDIR)/h350 \
             $(OH323_SRCDIR)/h450 \
             $(OH323_SRCDIR)/h460 

VPATH_C := $(OH323_SRCDIR)

SOURCES		+= $(COMMON_SOURCES)


####################################################

include $(PWLIBDIR)/make/common.mak

####################################################

# make sure pwlib is in the list of external libraries
# this ensures pwlib is not statically linked
LDFLAGS        += -L$(PW_LIBDIR)
SYSLIBS        += -l$(PTLIB_BASE)$(LIB_TYPE)

LIB_BASENAME=$(OH323_BASE)
LIB_FILENAME=$(OH323_FILE)

include $(PWLIBDIR)/make/lib.mak

####################################################


# Make sure the asnparser is built and if new version force recompiles

# Use a different variable here to support cross compiling
ifndef HOSTPWLIBDIR
HOSTPWLIBDIR=$(PWLIBDIR)
endif

ifndef HOST_PLATFORM_TYPE
HOST_PLATFORM_TYPE=$(PLATFORM_TYPE)
endif


# Set library path so asnparser will run

ifdef LD_LIBRARY_PATH
LD_LIBRARY_PATH:=$(LD_LIBRARY_PATH):$(HOSTPWLIBDIR)/lib
else
LD_LIBRARY_PATH:=$(HOSTPWLIBDIR)/lib
endif
export LD_LIBRARY_PATH


# If we're cross compiling, we want the host's asnparser
# otherwise use the one for the current platform
ASNPARSE_DIR = $(HOSTPWLIBDIR)/tools/asnparser
ASNPARSER = $(ASNPARSE_DIR)/obj_$(HOST_PLATFORM_TYPE)_d/asnparser


# If not cross compiling then make sure asnparser is built
ifeq ($(PLATFORM_TYPE),$(HOST_PLATFORM_TYPE))
$(ASNPARSER):
	$(MAKE) -C $(ASNPARSE_DIR) opt
endif


asnparser.version: $(ASNPARSER)
	$(ASNPARSER) --version | awk '{print $$1,$$2,$$3}' > asnparser.version.new
	if test -f asnparser.version && diff asnparser.version.new asnparser.version >/dev/null 2>&1 ; \
		then rm asnparser.version.new ; \
		else mv asnparser.version.new asnparser.version ; \
	fi

asnfiles: $(ASNPARSER)
	$(ASNPARSER) -s3 -m H245 -c --classheader "H245_AudioCapability=#ifndef PASN_NOPRINTON\nvoid PrintOn(ostream & strm) const;\n#endif" h245.asn
	mv $(OH323_SRCDIR)/h245.h $(OH323_INCDIR)/h245.h
	$(ASNPARSER) -m H235 -c h235.asn
	mv $(OH323_SRCDIR)/h235.h $(OH323_INCDIR)/h235.h
	$(ASNPARSER) -s2 -m H225 -r MULTIMEDIA-SYSTEM-CONTROL=H245 -c h225.asn
	mv $(OH323_SRCDIR)/h225.h $(OH323_INCDIR)/h225.h
	$(ASNPARSER) -m H248 -c h248.asn
	mv $(OH323_SRCDIR)/h248.h $(OH323_INCDIR)/h248.h
	$(ASNPARSER) -m H4501 -c h4501.asn
	mv $(OH323_SRCDIR)/h4501.h $(OH323_INCDIR)/h4501.h
	$(ASNPARSER) -m H4502 -c h4502.asn
	mv $(OH323_SRCDIR)/h4502.h $(OH323_INCDIR)/h4502.h
	$(ASNPARSER) -m H4503 -c h4503.asn
	mv $(OH323_SRCDIR)/h4503.h $(OH323_INCDIR)/h4503.h
	$(ASNPARSER) -m H4504 -c h4504.asn
	mv $(OH323_SRCDIR)/h4504.h $(OH323_INCDIR)/h4504.h
	$(ASNPARSER) -m H4505 -c h4505.asn
	mv $(OH323_SRCDIR)/h4505.h $(OH323_INCDIR)/h4505.h
	$(ASNPARSER) -m H4506 -c h4506.asn
	mv $(OH323_SRCDIR)/h4506.h $(OH323_INCDIR)/h4506.h
	$(ASNPARSER) -m H4507 -c h4507.asn
	mv $(OH323_SRCDIR)/h4507.h $(OH323_INCDIR)/h4507.h
	$(ASNPARSER) -m H4508 -c h4508.asn
	mv $(OH323_SRCDIR)/h4508.h $(OH323_INCDIR)/h4508.h
	$(ASNPARSER) -m H4509 -c h4509.asn
	mv $(OH323_SRCDIR)/h4509.h $(OH323_INCDIR)/h4509.h
	$(ASNPARSER) -m H45010 -c h45010.asn
	mv $(OH323_SRCDIR)/h45010.h $(OH323_INCDIR)/h45010.h
	$(ASNPARSER) -m H45011 -c h45011.asn
	mv $(OH323_SRCDIR)/h45011.h $(OH323_INCDIR)/h45011.h
	$(ASNPARSER) -m X880 -c x880.asn
	mv $(OH323_SRCDIR)/x880.h $(OH323_INCDIR)/x880.h
	$(ASNPARSER) -m H501 -c h501.asn
	mv $(OH323_SRCDIR)/h501.h $(OH323_INCDIR)/h501.h
	$(ASNPARSER) -m T38 -c t38.asn
	mv $(OH323_SRCDIR)/t38.h $(OH323_INCDIR)/t38.h
	$(ASNPARSER) -m MCS -c mcspdu.asn
	mv $(OH323_SRCDIR)/mcspdu.h $(OH323_INCDIR)/mcspdu.h
	$(ASNPARSER) -m GCC -c gccpdu.asn
	mv $(OH323_SRCDIR)/gccpdu.h $(OH323_INCDIR)/gccpdu.h

notrace::
	$(MAKE) NOTRACE=1 opt

