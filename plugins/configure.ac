AC_INIT(configure.ac)

if test \! -e ./aclocal.m4 ; then
  echo "ERROR: you must run 'aclocal' before 'autoconf'"
  exit
fi

PKG_PROG_PKG_CONFIG()
AC_CANONICAL_TARGET()

m4_include(../opal.m4)

OPAL_DETERMINE_DEBUG()
CFLAGS="$DEFAULT_CFLAGS $CFLAGS"
CXXFLAGS="$DEFAULT_CFLAGS $CXXFLAGS"

AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_CHECK_TOOL([NM], [nm], [:])
AC_CHECK_TOOL([LDD], [ldd], [:])
AC_CHECK_TOOL([OBJDUMP], [objdump], [:])

AC_HEADER_STDC
AC_CHECK_HEADERS([stddef.h limits.h float.h fcntl.h])

dnl Enable compiler warnings
dnl if test "x$ac_cv_c_compiler_gnu" = xyes; then
dnl for later...
dnl  CFLAGS="$CFLAGS -Wall -Wextra -Winit-self -Wswitch-default -Wswitch-enum -Wstrict-aliasing=2 -Wfloat-equal -Wshadow -Wno-comment -Wno-missing-field-initializers -Wno-unused"
dnl  CXXFLAGS="$CXXFLAGS -Wall -Wextra -Winit-self -Wswitch-default -Wswitch-enum -Wstrict-aliasing=2 -Wfloat-equal -Wshadow -Wno-comment -Wno-missing-field-initializers  -Wno-unused"
dnl fi

dnl ########################################################################
dnl Directories
dnl ########################################################################

OPAL_DETERMINE_VERSION([..])
OPAL_DETERMINE_PLUGIN_DIR()

AC_PLUGIN_DIR="${PLUGIN_DIR}/codecs/audio"
VC_PLUGIN_DIR="${PLUGIN_DIR}/codecs/video"
LID_PLUGIN_DIR="${PLUGIN_DIR}/lid"
FAX_PLUGIN_DIR="${PLUGIN_DIR}/fax"

AC_SUBST(prefix)
AC_SUBST(exec_prefix)
AC_SUBST(libdir)
AC_SUBST(target_os)
AC_SUBST(AC_PLUGIN_DIR)
AC_SUBST(VC_PLUGIN_DIR)
AC_SUBST(LID_PLUGIN_DIR)
AC_SUBST(FAX_PLUGIN_DIR)

dnl ########################################################################
dnl Platform specific section
dnl ########################################################################
WIN32=no
LDSO=
case "$target_os" in
	solaris*)
          STDCCFLAGS="$STDCCFLAGS -KPIC"
	  LDSO="-Bdynamic -G -h"
          PLUGINEXT=so
          AC_CHECK_HEADERS([sys/time.h sys/file.h netinet/in.h])
          AC_FUNC_FORK()
        ;;

        darwin*)  
	  CFLAGS="$CFLAGS -fno-common -dynamic"
          LDSO="-dynamiclib -multiply_defined suppress"
	  STDCCFLAGS="$STDCCFLAGS -fPIC -D__MACOSX__"
	  case "$target_cpu" in
	   x86 | i686 | i586 | i486 | i386 ) STDCCFLAGS="${STDCCFLAGS} -m486" ;;
          esac
	  PLUGINEXT=dylib 
          AC_CHECK_HEADERS([sys/time.h sys/file.h netinet/in.h])
          AC_FUNC_FORK()
        ;;

        cygwin*|mingw**)
	  LDSO="-module -avoid-version -shared"
	  PLUGINEXT=dll
	  WIN32=yes
          AC_CHECK_HEADERS([malloc.h])
          AC_DEFINE([PLUGIN_CODEC_DLL_EXPORTS], [1], [For win32 build])
        ;;

        *)
          STDCCFLAGS="$STDCCFLAGS -fPIC"
          LDSO='-shared -Wl,-soname,$(SONAME)'
          PLUGINEXT=so
          AC_CHECK_HEADERS([sys/time.h sys/file.h netinet/in.h])
          AC_FUNC_FORK()
        ;;
esac
AC_SUBST(LDSO)
AC_SUBST(PLUGINEXT)
AC_SUBST(CFLAGS)
AC_SUBST(STDCCFLAGS)
AC_SUBST(WIN32)


dnl ########################################################################
dnl Defaults
dnl ########################################################################

HAVE_VPB=yes
HAVE_CAPI=yes
HAVE_IXJ=no
HAVE_H264=yes
HAVE_CELT=yes
HAVE_SPANDSP=yes
DEFAULT_TO_FULL_CAPABILITIES=yes
HAVE_X264=yes
X264_LINK_STATIC=no
LIBAVCODEC_STACKALIGN_HACK=no

case "$target_os" in
        darwin*) HAVE_VPB=no 
                 HAVE_CAPI=no 
                 ;;
        cygwin*|mingw**)
	         HAVE_VPB=no 
                 HAVE_CAPI=no 
                 X264_LINK_STATIC=yes
                 ;;
esac

dnl ########################################################################
dnl Audio Codecs
dnl ########################################################################

dnl     ########################
dnl     libgsm (system/local)
dnl     ########################
AC_ARG_ENABLE([localgsm],
              [AC_HELP_STRING([--enable-localgsm],[Force use local version of GSM library rather than system version])],
              [localgsm=$enableval],
              [localgsm=])


if test "x${localgsm}" = "xyes" ; then
  AC_MSG_NOTICE(Forcing use of local GSM sources)
  GSM_SYSTEM=no

elif test "x${localgsm}" = "xno" ; then
  AC_MSG_NOTICE(Forcing use of system GSM library)
  OPAL_FIND_GSM([HAVE_GSM=yes], [HAVE_GSM=no])
  if test "x${HAVE_GSM}" != "xyes" ; then
    AC_MSG_ERROR([cannot find system GSM])
  else
    GSM_SYSTEM=yes
  fi

else
  OPAL_FIND_GSM([HAVE_GSM=yes], [HAVE_GSM=no])
  if test "x${HAVE_GSM}" = "xyes" ; then
    GSM_SYSTEM=yes
  else
    GSM_SYSTEM=no
  fi
fi

if test "x${GSM_SYSTEM}" = "xyes" ; then
  AC_SUBST(GSM_CFLAGS)
  AC_SUBST(GSM_LIBS)
  AC_MSG_RESULT(using system GSM)
fi
AC_SUBST(GSM_SYSTEM)

dnl     ########################
dnl     speex (system/local)
dnl     ########################
OPAL_SPEEX_TYPES()
AC_SUBST(SIZE16)
AC_SUBST(SIZE32)
OPAL_DETERMINE_SPEEX()
AC_SUBST(SPEEX_SYSTEM)
AC_SUBST(SPEEX_CFLAGS)
AC_SUBST(SPEEX_LIBS)

dnl     ########################
dnl     iLBC (system/local)
dnl     ########################
OPAL_DETERMINE_ILBC()
AC_SUBST(ILBC_SYSTEM)
AC_SUBST(ILBC_CFLAGS)
AC_SUBST(ILBC_LIBS)

dnl ########################################################################
dnl sbc,Bluetooth SBC for telephony with CD quality

AC_ARG_ENABLE([sbc],
              [AC_HELP_STRING([--enable-sbc],[enable use of Bluetooth SBC for CD quality telephony])],
              [has_sbc=$enableval],
              [has_sbc=])

if test "$has_sbc" = "yes" ; then
  AC_CHECK_LIB(samplerate,src_get_version,has_samplerate=yes,has_samplerate=no)
  if test "$has_samplerate" = "yes" ; then
    HAVE_SBC=yes
    AC_DEFINE(HAVE_SBC, [1], [Enable SBC codec])
    AC_MSG_NOTICE(Enabling SBC codec)
  fi
fi
AC_SUBST(HAVE_SBC)

dnl ########################################################################
dnl Video Codecs
dnl ########################################################################

dnl     ########################
dnl     Capabilities option
dnl     ########################
OPAL_SIMPLE_OPTION([default-to-full-capabilties],[DEFAULT_TO_FULL_CAPABILITIES], [default to full capabilities at empty fmtp lines])

dnl     ########################
dnl     libavcodec (ffmpeg)
dnl     ########################
AC_ARG_ENABLE([libavcodec],
              [AC_HELP_STRING([--disable-libavcodec],[disable libavcodec support])],
              [HAVE_LIBAVCODEC=$enableval])

if test "x$HAVE_LIBAVCODEC" = "xno" ; then
  AC_MSG_RESULT(Manually disabled libavcodec support)
  HAVE_H263P=no
  HAVE_MPEG4=no
  HAVE_H264_DECODER=no
else
  PKG_CHECK_MODULES([LIBAVCODEC],
                    [libavcodec >= 51.11.0],
                    [
                     AC_SUBST(LIBAVCODEC_CFLAGS)
                     AC_SUBST(LIBAVCODEC_LIBS)
                     AC_DEFINE([HAVE_LIBAVCODEC], [1], [ffmpeg])
                     AC_SUBST(LIBAVCODEC_LIB_NAME)
                     HAVE_LIBAVCODEC=yes
                     OPAL_LIBAVCODEC_HACK
                     case "$target_os" in
                        cygwin* | mingw* ) 
                                           LIBAVCODEC_LIB_NAME="avcodec.dll"
                                           HAVE_H263P=yes
                                           HAVE_MPEG4=yes
                                           HAVE_H264_DECODER=yes
                                           ;;
                                       * ) 
                                           AC_CHECK_LIB(avcodec, h263p_encoder, [HAVE_H263P=yes], [HAVE_H263P=no])
                                           AC_CHECK_LIB(avcodec, mpeg4_encoder, [HAVE_MPEG4=yes], [HAVE_MPEG4=no])
                                           AC_CHECK_LIB(avcodec, h264_decoder, [HAVE_H264_DECODER=yes], [HAVE_H264_DECODER=no])
                                           ;;
                     esac
                     AC_SUBST(HAVE_H263P)
                     AC_SUBST(HAVE_MPEG4)
                     OPAL_LIBAVCODEC_HEADER
                    ],
                    [
                     HAVE_LIBAVCODEC=no
                     HAVE_H263P=no
                     HAVE_MPEG4=no
                     HAVE_H264_DECODER=no
                    ]
                   )
fi
AC_SUBST(HAVE_LIBAVCODEC)

OPAL_LIBAVCODEC_SOURCE()
AC_SUBST(LIBAVCODEC_SOURCE_DIR)

if test "x$HAVE_LIBAVCODEC" = "xyes" ; then
  AC_MSG_CHECKING(if libavcodec has rtp_mode);
  LIBAVINCDIR=`pkg-config --variable=includedir libavcodec`/libavcodec
  AC_TRY_COMPILE([#include <libavcodec/avcodec.h>],
                  [int main(int ac,char **av) { AVCodecContext c; c.rtp_mode = 1; return 0; }], [HAVE_LIBAVCODEC_RTP_MODE=1])
  if test "x$HAVE_LIBAVCODEC_RTP_MODE" = "x1" ; then
    AC_MSG_RESULT(yes);
  else
    AC_MSG_RESULT(no)
  fi
fi
AC_SUBST(HAVE_LIBAVCODEC_RTP_MODE)

dnl     ########################
dnl     x264
dnl     ########################
AC_ARG_ENABLE([x264],
              [AC_HELP_STRING([--disable-x264],[disable x264 support])],
              [HAVE_X264=$enableval])

if test "x$HAVE_X264" = "xno" ; then
  AC_MSG_RESULT(Manually disabled X264)
else
  PKG_CHECK_MODULES([X264],
                    [x264 >= 0.48],
                    [
                     AC_SUBST(X264_CFLAGS)
                     AC_SUBST(X264_LIBS)
                     HAVE_X264=yes
                     OPAL_GET_LIBNAME([X264], [libx264], [$X264_LIBS])
                     AC_SUBST(X264_LIB_NAME)
                     OPAL_X264_LINKAGE
                    ],
                    [
                     HAVE_X264=no
                    ]
                   )
fi
AC_SUBST(X264_LINK_STATIC)
AC_SUBST(HAVE_X264)

dnl     ########################
dnl     theora
dnl     ########################
AC_ARG_ENABLE([theora],
              [AC_HELP_STRING([--disable-theora],[disable Theora support])],
              [HAVE_THEORA=$enableval])

if test "x$HAVE_THEORA" = "xno" ; then
  AC_MSG_RESULT(Manually disabled theora)
else
  PKG_CHECK_MODULES([THEORA],
                    [theora],
                    [
                     AC_SUBST(THEORA_CFLAGS)
                     AC_SUBST(THEORA_LIBS)
                     AC_DEFINE([HAVE_THEORA], [1], [theora])
                     HAVE_THEORA=yes
                    ],
                    [
                     HAVE_THEORA=no
                    ])
fi
AC_SUBST(HAVE_THEORA)

dnl     ########################
dnl     celt
dnl     ########################
AC_ARG_ENABLE([celt],
              [AC_HELP_STRING([--disable-celt],[disable CELT support])],
              [HAVE_CELT=$enableval])

if test "x$HAVE_CELT" = "xno" ; then
  AC_MSG_RESULT(Manually disabled CELT)
else
  PKG_CHECK_MODULES([CELT],
                    [celt],
                    [
                     AC_SUBST(CELT_CFLAGS)
                     AC_SUBST(CELT_LIBS)
                     AC_DEFINE([HAVE_CELT], [1], [celt])
                     HAVE_CELT=yes
                     PKG_CHECK_EXISTS([celt >= 0.5.0],
                                       [
                                        AC_DEFINE([HAVE_CELT_0_5_0_OR_LATER], [1], [celt 0.5.0 or later found])
                     ])
                    ],
                    [
                     HAVE_CELT=no
                    ])
fi
AC_SUBST(HAVE_CELT)

dnl     ########################
dnl     LIBDL
dnl     ########################
OPAL_FIND_LIBDL([HAVE_LIBDL=yes], [HAVE_LIBDL=no])
AC_SUBST(HAVE_LIBDL)
AC_SUBST(DL_LIBS)

dnl     ########################
dnl     H.264
dnl     ########################
if test "x$X264_LINK_STATIC" != "xyes"&& test "x$HAVE_LIBDL" != "xyes" ; then
  AC_MSG_NOTICE(H.264 support disabled since static linking was disabled and libdl was not found)
  HAVE_H264=no
else
  OPAL_SIMPLE_OPTION([h264],[HAVE_H264], [enable H.264 support], [HAVE_X264], [HAVE_H264_DECODER])
fi

dnl ########################################################################
dnl LID Devices
dnl ########################################################################

dnl     ########################
dnl     Quicknet xJACK cards
dnl     #######################
AC_ARG_ENABLE([ixj],
              [AC_HELP_STRING([--enable-ixj],[enable xJack cards])],
              [HAVE_IXJ=$enableval])

if test "x$HAVE_IXJ" = "xyes" ; then
OPAL_MSG_CHECK([IXJ], [$HAVE_IXJ])
  if test "x$HAVE_IXJ" = "xyes" ; then
    AC_CHECK_HEADERS([linux/telephony.h], 
		     [HAVE_IXJ=yes],
		     [AC_CHECK_HEADERS([ sys/telephony.h], [HAVE_IXJ=yes], [HAVE_IXJ=no])])
    if test "x$HAVE_IXJ" = "xyes" ; then
      AC_TRY_COMPILE([#include <linux/ixjuser.h>],
                     [int main(int ac,char **av) { return 0; }], [HAVE_IXJ=yes], [HAVE_IXJ=no])
    fi
  fi
fi
OPAL_MSG_CHECK([IXJ], [$HAVE_IXJ])

AC_SUBST(HAVE_IXJ)


dnl     ########################
dnl     VoiceTronics VPB card
dnl     ########################
OPAL_SIMPLE_OPTION([vpb],[HAVE_VPB], [enable voicetronix vpb])

dnl     ########################
dnl     CAPI
dnl     ########################
AC_ARG_ENABLE([capi],
              [AC_HELP_STRING([--enable-capi],[enable CAPI])],
              [HAVE_CAPI=$enableval])

if test "x$HAVE_CAPI" = "xyes" ; then
  AC_CHECK_HEADERS([capi20.h], [HAVE_CAPI=yes], [HAVE_CAPI=no])
fi

OPAL_MSG_CHECK([CAPI], [$HAVE_CAPI])
AC_SUBST(HAVE_CAPI)

dnl ########################################################################
dnl FAX Devices
dnl ########################################################################

dnl     ########################
dnl     spanDSP
dnl     ########################
AC_ARG_ENABLE([spandsp],
              [AC_HELP_STRING([--enable-spandsp],[enable spanDSP-based fax support])],
              [HAVE_SPANDSP=$enableval])

if test "x${HAVE_SPANDSP}" = "xyes" ; then
    OPAL_FIND_SPANDSP([HAVE_SPANDSP=yes], [HAVE_SPANDSP=no])
fi

AC_SUBST(HAVE_SPANDSP)
AC_SUBST(SPANDSP_LIBS)

dnl ########################################################################
dnl Various checks
dnl ########################################################################

AC_LANG_PUSH([C++])
AC_MSG_CHECKING(for posix_memalign)
AC_COMPILE_IFELSE([
  AC_LANG_PROGRAM([[#include <stdlib.h>]], 
                  [[
                  void * p; posix_memalign(&p, 16, 16); return 0; 
                  ]]
                  )
                  ],
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_POSIX_MEMALIGN, 1),
  AC_MSG_RESULT(no)
)
AC_LANG_POP([C++])

dnl ########################################################################

if test -d audio/G722 ; then
  AC_CONFIG_FILES(audio/G722/Makefile)
fi
if test -d audio/GSM0610 ; then
  AC_CONFIG_FILES(audio/GSM0610/Makefile)
fi
if test -d audio/gsm-amr ; then
  AC_CONFIG_FILES(audio/gsm-amr/Makefile)
fi
if test -d audio/iLBC ; then
  AC_CONFIG_FILES(audio/iLBC/Makefile)
fi
if test -d audio/Speex ; then
  AC_CONFIG_FILES(audio/Speex/Makefile)
fi
if test -d audio/LPC_10 ; then
  AC_CONFIG_FILES(audio/LPC_10/Makefile)
fi
if test -d audio/G726 ; then
  AC_CONFIG_FILES(audio/G726/Makefile)
fi
if test -d audio/IMA_ADPCM ; then
  AC_CONFIG_FILES(audio/IMA_ADPCM/Makefile)
fi
if test -d audio/SBC ; then
  AC_CONFIG_FILES(audio/SBC/Makefile)
fi
if test -d audio/celt ; then
  AC_CONFIG_FILES(audio/celt/Makefile)
fi
if test -d video/H.261-vic ; then
  AC_CONFIG_FILES(video/H.261-vic/Makefile)
fi
if test -d video/H.263-1998 ; then
  AC_CONFIG_FILES(video/H.263-1998/Makefile)
fi
if test -d video/MPEG4-ffmpeg ; then
  AC_CONFIG_FILES(video/MPEG4-ffmpeg/Makefile)
fi
if test -d video/H.264 ; then
  AC_CONFIG_FILES(video/H.264/Makefile)
fi
if test -d video/H.264/gpl; then
  AC_CONFIG_FILES(video/H.264/gpl/Makefile)
fi
if test -d video/THEORA ; then
  AC_CONFIG_FILES(video/THEORA/Makefile)
fi
if test -d LID/IxJ ; then
  AC_CONFIG_FILES(LID/IxJ/Makefile)
fi
if test -d LID/VPB ; then
  AC_CONFIG_FILES(LID/VPB/Makefile)
fi
if test -d LID/CAPI ; then
  AC_CONFIG_FILES(LID/CAPI/Makefile)
fi
if test -d fax/fax_spandsp ; then
  AC_CONFIG_FILES(fax/fax_spandsp/Makefile)
fi

AC_CONFIG_FILES(Makefile)

dnl ########################################################################
dnl Header file

dnl ########################################################################
dnl Output configured files

AC_OUTPUT(audio/Speex/libspeex/speex_config_types.h)
AC_CONFIG_HEADERS(plugin-config.h)
AC_OUTPUT()

dnl ########################################################################
dnl Summary
dnl ########################################################################
echo ""
echo "=================== Plugin configuration ======================"
echo " prefix                           : $prefix"
echo " exec_prefix                      : $exec_prefix"
echo " libdir                           : $libdir"
echo " Audio Codec plugin install dir   : "'${libdir}'"/$AC_PLUGIN_DIR"
echo " Video Codec plugin install prefix: "'${libdir}'"/$VC_PLUGIN_DIR"
echo " LID plugin install prefix        : "'${libdir}'"/$LID_PLUGIN_DIR"
echo " FAX plugin install prefix        : "'${libdir}'"/$FAX_PLUGIN_DIR"

echo ""
if test "x${GSM_SYSTEM}" = "xyes" ; then
echo "                              GSM :  yes (system)"
else
echo "                              GSM :  yes (internal)"
fi

echo "                          gsm-amr :  yes"
if test "x${ILBC_SYSTEM}" = "xyes" ; then
echo "                             iLBC :  yes (system)"
else
echo "                             iLBC :  yes (internal)"
fi

if test "x${SPEEX_SYSTEM}" = "xyes" ; then
echo "                            Speex :  yes (system, version $SPEEX_SYSTEM_VERSION)"
else
echo "                            Speex :  yes (internal, version $SPEEX_INTERNAL_VERSION)"
fi
echo "                            LPC10 :  yes"
echo "                            G.726 :  yes"
echo "                        IMA_ADPCM :  yes"

if test "x${HAVE_SBC}" = "xyes" ; then
echo "                              SBC :  yes"
else
echo "                              SBC :  no"
fi

echo ""

echo "                           H.261  :  yes"
echo "                     H.263/H.263+ :  ${HAVE_H263P}"
echo "                           H.264  :  ${HAVE_H264}"
echo "                          THEORA  :  ${HAVE_THEORA}"

if test "x${HAVE_MPEG4}" = "xyes" ; then
if test "x${LIBAVCODEC_SOURCE_DIR}" = "x" ; then
echo "                    MPEG4 Part 2  :  yes (no rate control correction)"
else
echo "                    MPEG4 Part 2  :  yes"
fi
else
echo "                    MPEG4 Part 2  :  no"
fi
echo "                            CELT  :  ${HAVE_CELT}"

echo ""
echo "                     SpanDSP FAX  :  $HAVE_SPANDSP"

echo ""

echo "                            CAPI  :  $HAVE_CAPI"
echo "           Quicknet xJACK support :  $HAVE_IXJ"
echo "         Voicetronics VPB support :  $HAVE_VPB"

echo ""

echo "                         OS Type  :  $target_os"
echo "                    Machine Type  :  $target_cpu"
echo ""
echo " If all settings are OK, type make and make install "
echo "========================================================"
