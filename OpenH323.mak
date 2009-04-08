!ifndef PTLIBDIR
PTLIBDIR=c:\work\pwlib
!endif

!ifndef OPENH323DIR
OPENH323DIR=$(MAKEDIR)
!endif

INCLUDE=$(INCLUDE);$(PTLIBDIR)\include\ptlib\msos;$(PTLIBDIR)\include\pwlib\mswin;$(PTLIBDIR)\include;$(OPENH323DIR)/include
LIB=$(LIB);$(PTLIBDIR)\Lib;$(OPENH323DIR)/Lib

all:
	msdev OpenH323Lib.dsp /MAKE "OpenH323Lib - Win32 Release" /USEENV
	msdev OpenH323dll.dsp /MAKE "OpenH323dll - Win32 Release" /USEENV
	cd samples\simple
	msdev simple.dsp /MAKE "SimpH323 - Win32 Release" /USEENV

debug: all
	msdev OpenH323Lib.dsp /MAKE "OpenH323Lib - Win32 Debug" /USEENV
	msdev OpenH323dll.dsp /MAKE "OpenH323dll - Win32 Debug" /USEENV
	cd samples\simple
	msdev simple.dsp /MAKE "SimpH323 - Win32 Debug" /USEENV
