#
# openh323u.mak
#
# Make symbols include file for Open H323 library
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
# The Original Code is Open H323 library.
#
# The Initial Developer of the Original Code is Equivalence Pty. Ltd.
#
# Contributor(s): ______________________________________.
#
# $Id$
#

PTLIBDIR	= @PTLIBDIR@
ifndef OPENH323DIR
OPENH323DIR	= @OPENH323DIR@
endif
STDCCFLAGS	+= @STDCCFLAGS@
LDFLAGS		+= @LDFLAGS@
LDLIBS		+= @LDLIBS@
ENDLDLIBS	:= @ENDLDLIBS@ $(ENDLDLIBS)

HAS_PTLIB_LIB_MAK=@HAS_PTLIB_LIB_MAK@

NOAUDIOCODECS        = @NOAUDIOCODECS@
NOVIDEO	             = @NOVIDEO@
NOTRACE	             = @NOTRACE@
H323_H224	     = @H323_H224@
H323_H230	     = @H323_H230@
H323_H235	     = @H323_H235@
H323_H239	     = @H323_H239@
H323_H248	     = @H323_H248@
H323_H249	     = @H323_H249@
H323_H341	     = @H323_H341@
H323_H350	     = @H323_H350@
H323_H450	     = @H323_H450@
H323_H460        = @H323_H460@
H323_H46018      = @H323_H46018@
H323_H46019M	 = @H323_H46019M@
H323_H46023      = @H323_H46023@
H323_H46025      = @H323_H46025@
H323_H46026      = @H323_H46026@
H323_H501	     = @H323_H501@
H323_T38	     = @H323_T38@
H323_T120	     = @H323_T120@
H323_AEC	     = @H323_AEC@
H323_GNUGK	     = @H323_GNUGK@
H323_FILE	     = @H323_FILE@
H323_TLS	     = @H323_TLS@


###### new ptlib
ifndef HAS_PTLIB_LIB_MAK

##### grab ptlib build settings
include $(PTLIBDIR)/make/ptbuildopts.mak

# compatibility with new ptlib - it's not has OPENSSL flag which cause undefined reference to `PPlugin_H235Authenticator_Std1_link()' in library
ifdef HAS_SASL2
HAS_OPENSSL = 1
endif

MAJOR_VERSION =@MAJOR_VERSION@
MINOR_VERSION =@MINOR_VERSION@
#BUILD_TYPE    =.
BUILD_NUMBER  =@BUILD_NUMBER@

#export to GK
export target_os=@target_os@
export target_cpu=@target_cpu@
export OSTYPE=$(target_os)
export PLATFORM_TYPE=$(target_cpu)
export PTLIB_CXXFLAGS=@PTLIB_CXXFLAGS@
export PT_LIBDIR=@PT_LIBDIR@/lib_@target_os@_@target_cpu@
export PW_LIBDIR=@PT_LIBDIR@/lib_@target_os@_@target_cpu@

endif #HAS_PTLIB_LIB_MAK

ifdef LIBRARY_MAKEFILE
include $(PTLIBDIR)/make/unix.mak
else
ifeq ($(NOTRACE), 1)
OBJDIR_SUFFIX := n
endif # NOTRACE
include $(PTLIBDIR)/make/ptlib.mak
endif # LIBRARY_MAKEFILE

LIBDIRS += $(OPENH323DIR)

#OH323_SUPPRESS_H235	= 1

OH323_SRCDIR = $(OPENH323DIR)/src
ifdef PREFIX
OH323_INCDIR = $(PREFIX)/include/openh323
else
OH323_INCDIR = $(OPENH323DIR)/include
endif # PREFIX

ifndef OH323_LIBDIR
OH323_LIBDIR = $(OPENH323DIR)/lib
endif # OH323_LIBDIR

ifeq ($(NOTRACE), 1)
STDCCFLAGS += -DPASN_NOPRINTON -DPASN_LEANANDMEAN
OH323_SUFFIX = n
else
ifeq (,$(findstring PTRACING,$(STDCCFLAGS)))
ifeq ($(HAS_PTLIB_LIB_MAK),1)
STDCCFLAGS += -DPTRACING
RCFLAGS	   += -DPTRACING
endif # HAS_PTLIB_LIB_MAK
endif
OH323_SUFFIX = $(OBJ_SUFFIX)
endif # NOTRACE

###### new ptlib
ifneq ($(HAS_PTLIB_LIB_MAK),1)
PT_OBJBASE=$(PTLIB_OBJBASE)
ifndef OH323_SUFFIX
OH323_SUFFIX = $(OBJ_SUFFIX)
endif # OH323_SUFFIX
endif # HAS_PTLIB_LIB_MAK

OH323_BASE  = h323_$(PLATFORM_TYPE)_$(OH323_SUFFIX)$(LIB_TYPE)
OH323_FILE  = lib$(OH323_BASE).$(LIB_SUFFIX)

LDFLAGS	    += -L$(OH323_LIBDIR)
LDLIBS	    := -l$(OH323_BASE) $(LDLIBS)

STDCCFLAGS  += -I$(OH323_INCDIR)

ifdef	OH323_SUPPRESS_H235
STDCCFLAGS  += -DOH323_SUPPRESS_H235
endif

ifneq ($(HAS_PTLIB_LIB_MAK),1)
CFLAGS += $(STDCCFLAGS)
endif # HAS_PTLIB_LIB_MAK

$(TARGET) :	$(OH323_LIBDIR)/$(OH323_FILE)

ifndef LIBRARY_MAKEFILE
ifdef DEBUG
$(OH323_LIBDIR)/$(OH323_FILE):
	$(MAKE) -C $(OH323_SRCDIR) debug
else
$(OH323_LIBDIR)/$(OH323_FILE):
	$(MAKE) -C $(OH323_SRCDIR) opt
endif # DEBUG
endif # LIBRARY_MAKEFILE

# End of file


