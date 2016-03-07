AC_INIT(include/h323.h)

AC_PROG_CXX
AC_HEADER_STDC

dnl this must be updated for every minor revision, and for
dnl builds when and as necessary
REQUIRED_PTLIB_MAJOR=2
REQUIRED_PTLIB_MINOR=6
REQUIRED_PTLIB_BUILD=4

dnl ########################################################################
dnl set the OpenH323 directory to the current directory

OPENH323DIR=`pwd`
AC_SUBST(OPENH323DIR)

dnl ########################################################################
dnl set the PREFIX accordingly
if test "x$prefix" = "xNONE"; then
   INSTALLPREFIX="/usr/local"
else
   INSTALLPREFIX="${prefix}"
fi

AC_SUBST(INSTALLPREFIX)


dnl ########################################################################
dnl set LIBDIR accordingly
LIBDIR="${libdir}"

AC_SUBST(LIBDIR)


dnl ########################################################################
dnl extract the OpenH323 version

dnl MSWIN_DISPLAY version,Version
dnl MSWIN_VERSION version,OPENH323_MAJOR=MAJOR_VERSION
dnl MSWIN_VERSION version,OPENH323_MINOR=MINOR_VERSION
dnl MSWIN_VERSION version,OPENH323_BUILD=BUILD_NUMBER
dnl MSWIN_VERSION version,OPENH323_VERSION=VERSION

MAJOR_VERSION=`cat ${OPENH323DIR}/version.h | grep MAJOR_VERSION | cut -f3 -d' '`
MINOR_VERSION=`cat ${OPENH323DIR}/version.h | grep MINOR_VERSION | cut -f3 -d' '`
BUILD_NUMBER=`cat ${OPENH323DIR}/version.h | grep BUILD_NUMBER | cut -f3 -d' '`
OPENH323_VERSION="${MAJOR_VERSION}.${MINOR_VERSION}.${BUILD_NUMBER}"
AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(BUILD_NUMBER)
AC_SUBST(OPENH323_VERSION)
AC_SUBST(MAJOR_VERSION, $MAJOR_VERSION)
AC_SUBST(MINOR_VERSION, $MINOR_VERSION)
AC_SUBST(BUILD_NUMBER, $BUILD_NUMBER)

echo "OpenH323 version is ${OPENH323_VERSION}"

AC_DEFINE_UNQUOTED(OPENH323_MAJOR,   ${MAJOR_VERSION}, [Major version])
AC_DEFINE_UNQUOTED(OPENH323_MINOR,   ${MINOR_VERSION}, [Minor version])
AC_DEFINE_UNQUOTED(OPENH323_BUILD,   ${BUILD_NUMBER},  [Build number])
AC_DEFINE_UNQUOTED(OPENH323_VERSION, "$OPENH323_VERSION", [Full version number])

dnl ########################################################################
dnl look for ptlib, use a preference order of explicit PTLIBDIR, directory
dnl at same level, home directory, /usr/local or /usr, pkgconfig.

if test "${PTLIBDIR:-unset}" != "unset" ; then 	 
  AC_CHECK_HEADER(${PTLIBDIR}/version.h, HAS_PTLIB=1) 	 
fi

if test "${HAS_PTLIB:-unset}" = "unset" ; then
  AC_CHECK_HEADER(${OPENH323DIR}/../ptlib/version.h, HAS_PTLIB=1) 	 
  if test "${HAS_PTLIB:-unset}" != "unset" ; then
    PTLIBDIR="${OPENH323DIR}/../ptlib"
  else
    AC_CHECK_HEADER(${HOME}/ptlib/version.h, HAS_PTLIB=1) 	 
    if test "${HAS_PTLIB:-unset}" != "unset" ; then
      PTLIBDIR="${HOME}/ptlib"
    else
      if test "${HAS_PTLIB:-unset}" = "unset" ; then
        AC_PATH_PROG(PTLIB_CONFIG, ptlib-config, , /usr/local/bin:/usr/bin)
      else
        if test "${PTLIB_CONFIG:-unset}" = "unset" ; then
          AC_PATH_PROG(PTLIB_CONFIG, ptlib-config, , /usr/share/ptlib/make/)
        else
          if test "${PTLIB_CONFIG:-unset}" = "unset" ; then
            AC_PATH_PROG(PTLIB_CONFIG, ptlib-config, , /usr/share/ptlib/make/)
          fi
        fi
      fi
    fi
  fi
fi
if test "${PTLIB_CONFIG:-unset}" != "unset" ; then
  PTLIBDIR=`$PTLIB_CONFIG --prefix`
  HAS_PTLIB=1
fi

if test "${HAS_PTLIB:-unset}" = "unset" ; then
  echo "Cannot find ptlib - please install or set PTLIBDIR and try again"
  exit
fi

if test "x$PTLIBDIR" = "x/usr" -o "x$PTLIBDIR" = "x/usr/"; then
  PTLIBDIR="/usr/share/ptlib/"
fi
if test "x$PTLIBDIR" = "x/usr/local" -o "x$PTLIBDIR" = "x/usr/local/"; then
  PTLIBDIR="/usr/local/share/ptlib/"
fi

AC_CHECK_FILE(${PTLIBDIR}/make/lib.mak, HAS_PTLIB_LIB_MAK=1)

if test "${HAS_PTLIB_LIB_MAK:-unset}" = "unset" ; then
  PKG_PROG_PKG_CONFIG
  PKG_CHECK_MODULES([PTLIB], [ptlib], HAS_PTLIB=1)
  if test ${HAS_PTLIB}x = 1x; then
    PTLIBDIR=`${PKG_CONFIG} --variable=prefix ptlib`
    PTVERSION=`${PKG_CONFIG} --modversion ptlib`
    target_os=`${PKG_CONFIG} --variable=target_os ptlib`
    target_cpu=`${PKG_CONFIG} --variable=target_cpu ptlib`
    PTLIB_CXXFLAGS=`${PKG_CONFIG} --cflags ptlib`
    PT_LIBDIR=`${PKG_CONFIG} --variable=libdir ptlib`
    PT_LIBDIR=${PTLIBDIR}
    PT_MAJOR_VERSION=`echo $PTVERSION | cut -d'.' -f1`
    PT_MINOR_VERSION=`echo $PTVERSION | cut -d'.' -f2`
    PT_BUILD_NUMBER=`echo $PTVERSION | cut -d'.' -f3`
    AC_SUBST(PT_LIBDIR)
    AC_SUBST(target_os)
    AC_SUBST(target_cpu)
    AC_SUBST(PTLIB_CXXFLAGS)
    AC_SUBST(PT_LIBDIR)
  fi
fi

echo "PTLib prefix set to.... $PTLIBDIR"

if test "${PTLIB_CONFIG:-unset}" != "unset" ; then
  PTVERSION=`$PTLIB_CONFIG --version`
  PT_MAJOR_VERSION=`echo $PTVERSION | cut -d'.' -f1`
  PT_MINOR_VERSION=`echo $PTVERSION | cut -d'.' -f2`
  PT_BUILD_NUMBER=`echo $PTVERSION | cut -d'.' -f3`
  
elif test "${PTVERSION:-unset}" = "unset" ; then
  PT_MAJOR_VERSION=`cat ${PTLIBDIR}/version.h | grep MAJOR_VERSION | cut -f3 -d' '`
  PT_MINOR_VERSION=`cat ${PTLIBDIR}/version.h | grep MINOR_VERSION | cut -f3 -d' '`
  PT_BUILD_NUMBER=`cat ${PTLIBDIR}/version.h | grep BUILD_NUMBER | cut -f3 -d' '`
  PTVERSION="${PT_MAJOR_VERSION}.${PT_MINOR_VERSION}.${PT_BUILD_NUMBER}"
fi


AC_ARG_ENABLE(versioncheck,
		[  --disable-versioncheck  disable PTLib version check],
		PTLIB_VERSION_CHECK=$enableval)

AC_MSG_CHECKING(Checking PTLib version ${PTVERSION})
if test "${PTLIB_VERSION_CHECK}x" = "nox" ; then
  AC_MSG_RESULT(check disabled)
else
  if test $PT_MAJOR_VERSION -lt $REQUIRED_PTLIB_MAJOR ; then
    AC_MSG_RESULT(Major version less than required $REQUIRED_PTLIB_MAJOR)
    exit 1
  fi
  if test $PT_MAJOR_VERSION -eq $REQUIRED_PTLIB_MAJOR ; then
    if test $PT_MINOR_VERSION -lt $REQUIRED_PTLIB_MINOR ; then
      AC_MSG_RESULT(Minor version less than required $REQUIRED_PTLIB_MINOR)
      exit 1
  	fi
  fi
  if test $PT_MAJOR_VERSION -eq $REQUIRED_PTLIB_MAJOR ; then
    if test $PT_MINOR_VERSION -eq $REQUIRED_PTLIB_MINOR ; then
      if test $PT_BUILD_NUMBER -lt $REQUIRED_PTLIB_BUILD ; then
        AC_MSG_RESULT(Build version less than required $REQUIRED_PTLIB_BUILD)
        exit 1
      fi
    fi
  fi
  AC_MSG_RESULT(ok)
fi

AC_SUBST(PTLIBDIR)
AC_SUBST(HAS_PTLIB_LIB_MAK)
AC_DEFINE_UNQUOTED(PT_MAJOR_VERSION, ${PT_MAJOR_VERSION}, [PTLib Major version])
AC_DEFINE_UNQUOTED(PT_MINOR_VERSION, ${PT_MINOR_VERSION}, [PTLib Minor version])
AC_DEFINE_UNQUOTED(PT_BUILD_NUMBER, ${PT_BUILD_NUMBER}, [PTLib Build number])

dnl ########################################################################
dnl check for setting minimum size

AC_ARG_ENABLE(minsize,
       [  --enable-minsize        minimise the size of the lib by disabling least used options])

dnl these are the default configure settings
default_audio=yes
default_video=yes
default_h450=yes
default_h501=yes
default_t38=yes
default_h224=yes
default_h230=yes
default_h235=no
default_h235_256=no
default_h239=yes
default_h248=yes
default_h249=no
default_h341=yes
default_h350=yes
default_h460=yes
default_h4609=yes
default_h46017=no
default_h46018=yes
default_h46019m=no
default_h46023=yes
default_h46025=no
default_h46026=no
default_h461=no
default_h460p=no
default_h460pre=no
default_h460com=no
default_t120=no
default_t140=no
default_gnugk=yes
default_file=yes
default_aec=no
default_tls=yes

if test "${enable_minsize}x" = "yesx" ; then
  default_audio=yes
  default_video=yes
  default_h450=no
  default_h501=no
  default_t38=no
  default_h224=no
  default_h230=no
  default_h235=no
  default_h235_256=no
  default_h239=no
  default_h248=no
  default_h249=no
  default_h341=no
  default_h350=no
  default_h460=no
  default_h4609=no
  default_h46017=no
  default_h46018=no
  default_h46019m=no
  default_h46023=no
  default_h46025=no
  default_h46026=no
  default_h461=no
  default_hh460p=no
  default_h460pre=no
  default_h460com=no
  default_t120=no
  default_t140=no
  default_gnugk=no
  default_file=no
  default_aec=no
  default_tls=no
fi


dnl ########################################################################
dnl check for disbling ASN tracing

AC_ARG_ENABLE(asntracing,
       [  --disable-asntracing    disable ASN tracing])

if test "${enable_asntracing}z" = "z" ; then
  enable_asntracing=$default_asntracing
fi

if test "$enable_asntracing" = "no" ; then
  AC_MSG_NOTICE(Disabling ASN tracing)
  AC_SUBST(NOTRACE, 1)
fi

dnl ########################################################################
dnl Enable audio subsystems

dnl MSWIN_DISPLAY audio,Audio Software Codecs
dnl MSWIN_DEFINE  audio,H323_AUDIO_CODECS

AC_ARG_ENABLE(audio,
       [  --disable-audio         disable audio codecs])

if test "${enable_audio}z" = "z" ; then
  enable_audio=yes
fi

if test "$enable_audio" = "no" ; then
  AC_MSG_NOTICE(Disabling audio functions)
  AC_SUBST(NOAUDIOCODECS, 1)
else
  AC_DEFINE(H323_AUDIO_CODECS, 1, [Include audio codecs])

  AC_ARG_ENABLE(embeddedgsm,
         [  --enable-embeddedgsm    embed GSM codec via static linking],
         EMBEDDED_GSM=$enableval)
  if test "${EMBEDDED_GSM}x" != "x" ; then
    AC_MSG_NOTICE(Embedding GSM codec)
    LDFLAGS="$LDFLAGS -L${OPENH323DIR}/plugins/audio/GSM0610"
    ENDLDLIBS="$ENDLDLIBS -lgsm0610_audio_s"
    AC_CHECK_LIB(gsm, gsm_create, GSM=1)
    if test "x$GSM" != "x"; then
      ENDLDLIBS="$ENDLDLIBS -lgsm"
    fi
    AC_DEFINE(H323_EMBEDDED_GSM, , [Use embdedded GSM codec])
  fi
fi

dnl ########################################################################
dnl Enable video subsystems

dnl MSWIN_DISPLAY video,Video Software Codecs
dnl MSWIN_DEFINE  video,H323_VIDEO

AC_ARG_ENABLE(video,
       [  --disable-video         disable video functions])

if test "${enable_video}z" = "z" ; then
  enable_video=$default_video
fi

if test "$enable_video" = "no" ; then
  AC_MSG_NOTICE(Disabling video functions)
  AC_SUBST(NOVIDEO, 1)
else
  AC_DEFINE(H323_VIDEO, 1, [Enable H.323 video support])
fi

dnl ########################################################################
dnl check for disabling H.224

dnl MSWIN_DISPLAY h224,H.224
dnl MSWIN_DEFINE  h224,H323_H224

AC_ARG_ENABLE(h224,
       [  --disable-h224          disable H.224])
if test "${enable_h224}x" = "x" ; then
  enable_h224=$default_h224
fi
if test "$enable_h224" = "yes" ; then
  H323_H224=1
  AC_DEFINE(H323_H224, 1, [Enable H.244 support])
else
  H323_H224=
  AC_MSG_NOTICE(Disabling H.224)
fi
AC_SUBST(H323_H224)

dnl ########################################################################
dnl check for disabling H.230

dnl MSWIN_DISPLAY h230,H.230
dnl MSWIN_DEFINE  h230,H323_H230

AC_ARG_ENABLE(h230,
       [  --disable-h230          disable H.230])
if test "${enable_h230}x" = "x" ; then
  enable_h230=$default_h230
fi
if test "$enable_h230" = "yes" ; then
  H323_H230=1
  AC_DEFINE(H323_H230, 1, [Enable H.230 support])
else
  H323_H230=
  AC_MSG_NOTICE(Disabling H.230)
fi
AC_SUBST(H323_H230)

dnl ########################################################################
dnl check for en/disabling H.235

dnl MSWIN_DISPLAY h235,H.235.6 (128 bit)
dnl MSWIN_DEFINE  h235,H323_H235

AC_ARG_ENABLE(h235,
       [  --disable-h235          disable H.235.6 (128 bit)])
if test "${enable_h235}x" = "x" ; then
  enable_h235=$default_h235
fi
if test "$enable_h235" = "yes" ; then
  H323_H235=1
  AC_DEFINE(H323_H235, 1, [Enable H.235 support])
else
  H323_H235=
  AC_MSG_NOTICE(Disabling H.235.6)
fi
AC_SUBST(H323_H235)

dnl ########################################################################
dnl check for en/disabling H.235 with 256 bits

dnl MSWIN_DISPLAY h235_256,H.235.6 (256 bit)
dnl MSWIN_DEFAULT h235_256,Disabled
dnl MSWIN_DEFINE  h235_256,H323_H235_AES256

AC_ARG_ENABLE(h235_256,
       [  --disable-h235-256      disable H.235.6 (256 bit)])
if test "${enable_h235_256}x" = "x" ; then
  enable_h235_256=$default_h235_256
fi
if test "$enable_h235" != "yes" ; then
  enable_h235_256="no"
fi
if test "$enable_h235_256" = "yes" ; then
  H323_H235_AES256=1
  AC_DEFINE(H323_H235_AES256, 1, [Enable H.235 AES256 support])
else
  H323_H235_AES256=
  AC_MSG_NOTICE(Disabling H.235.6 (256 bit))
fi
AC_SUBST(H323_H235_AES256)

dnl ########################################################################
dnl check for disabling H.239

dnl MSWIN_DISPLAY h239,H.239
dnl MSWIN_DEFINE  h239,H323_H239

AC_ARG_ENABLE(h239,
       [  --disable-h239          disable H.239])
if test "${enable_h239}x" = "x" ; then
  enable_h239=$default_h239
fi
if test "$enable_h239" = "yes" ; then
  H323_H239=1
  AC_DEFINE(H323_H239, 1, [Enable H.239 support])
else
  H323_H239=
  AC_MSG_NOTICE(Disabling H.239)
fi
AC_SUBST(H323_H239)

dnl ########################################################################
dnl check for disabling H.248

dnl MSWIN_DISPLAY h248,H.248
dnl MSWIN_DEFINE  h248,H323_H248

AC_ARG_ENABLE(h248,
       [  --disable-h248          enable H.248])
if test "${enable_h248}x" = "x" ; then
  enable_h248=$default_h248
fi
if test "$enable_h248" = "yes" ; then
  H323_H248=1
  AC_DEFINE(H323_H248, 1, [Enable H.248 support])
else
  H323_H248=
  AC_MSG_NOTICE(Disabling H.248)
fi
AC_SUBST(H323_H248)

dnl ########################################################################
dnl check for disabling H.249

dnl MSWIN_DISPLAY h249,H.249
dnl MSWIN_DEFINE  h249,H323_H249

AC_ARG_ENABLE(h249,
       [  --disable-h249          disable H.249])
if test "${enable_h249}x" = "x" ; then
  enable_h249=$default_h249
fi
if test "$enable_h249" = "yes" ; then
  H323_H249=1
  AC_DEFINE(H323_H249, 1, [Enable H.249 support])
else
  H323_H249=
  AC_MSG_NOTICE(Disabling H.249)
fi
AC_SUBST(H323_H249)

dnl ########################################################################
dnl check for disabling H.341

dnl MSWIN_DISPLAY h341,H.341
dnl MSWIN_DEFINE  h341,H323_H341

AC_ARG_ENABLE(h341,
       [  --disable-h341          disable H.341])
if test "${enable_h341}x" = "x" ; then
  enable_h341=$default_h341
fi
if test "$enable_h341" = "yes" ; then
  H323_H341=1
  AC_DEFINE(H323_H341, 1, [Enable H.341 support])
else
  H323_H341=
  AC_MSG_NOTICE(Disabling H.341)
fi
AC_SUBST(H323_H341)

dnl ########################################################################
dnl check for disabling H.350

dnl MSWIN_DISPLAY h350,H.350
dnl MSWIN_DEFINE  h350,H323_H350

AC_ARG_ENABLE(h350,
       [  --disable-h350          disable H.350])
if test "${enable_h350}x" = "x" ; then
  enable_h350=$default_h350
fi
if test "$enable_h350" = "yes" ; then
  H323_H350=1
  AC_DEFINE(H323_H350, 1, [Enable H.350 support])
else
  H323_H350=
  AC_MSG_NOTICE(Disabling H.350)
fi
AC_SUBST(H323_H350)

dnl ########################################################################
dnl check for disabling H.450

dnl MSWIN_DISPLAY h450,H.450
dnl MSWIN_DEFINE  h450,H323_H450

AC_ARG_ENABLE(h450,
       [  --disable-h450          disable H.450])
if test "${enable_h450}x" = "x" ; then
  enable_h450=$default_h450
fi
if test "$enable_h450" = "yes" ; then
  H323_H450=1
  AC_DEFINE(H323_H450, 1, [Enable H.450 support])
else
  H323_H450=
  AC_MSG_NOTICE(Disabling H.450)
fi
AC_SUBST(H323_H450)

dnl ########################################################################
dnl check for disabling H.460

dnl MSWIN_DISPLAY h460,H.460
dnl MSWIN_DEFINE  h460,H323_H460

AC_ARG_ENABLE(h460,
       [  --disable-h460          disable H.460])
if test "${enable_h460}x" = "x" ; then
  enable_h460=$default_h460
fi
if test "$enable_h460" = "yes" ; then
  H323_H460=1
  AC_DEFINE(H323_H460, 1, [Enable H.460 support])
else
  H323_H460=
  AC_MSG_NOTICE(Disabling H.460)
fi
AC_SUBST(H323_H460)

dnl ########################################################################
dnl check for enabling H.460.9

dnl MSWIN_DISPLAY h4609,H.460.9
dnl MSWIN_DEFINE  h4609,H323_H4609

AC_ARG_ENABLE(h4609,
       [  --disable-h4609         disable H.460.9])
if test "${enable_h4609}x" = "x" ; then
  enable_h4609=$default_h4609
fi
if test "$enable_h4609" = "yes" ; then
  H323_h4609=1
  AC_DEFINE(H323_h4609, 1, [Enable H.490 support])
else
  H323_h4609=
  AC_MSG_NOTICE(Disabling H.460.9)
fi
AC_SUBST(H323_H4609)

dnl ########################################################################
dnl check for enabling H.460.17

dnl MSWIN_DISPLAY h46017,H.460.17
dnl MSWIN_DEFINE  h46017,H323_H46017

AC_ARG_ENABLE(h46017,
       [  --disable-h46017        disable H.460.17])
if test "${enable_h46017}x" = "x" ; then
  enable_h46017=$default_h46017
fi
if test "$enable_h46017" = "yes" ; then
  H323_H46017=1
  AC_DEFINE(H323_H46017, 1, [Enable H.460.17 support])
else
  H323_H46017=
  AC_MSG_NOTICE(Disabling H.460.17)
fi
AC_SUBST(H323_H46017)

dnl ########################################################################
dnl check for enabling H.460.18/.19

dnl MSWIN_DISPLAY h46018,H.460.18/.19
dnl MSWIN_DEFINE  h46018,H323_H46018

AC_ARG_ENABLE(h46018,
       [  --disable-h46018        disable H.460.18/.19])
if test "${enable_h46018}x" = "x" ; then
  enable_h46018=$default_h46018
fi
if test "$enable_h46018" = "yes" ; then
  H323_H46018=1
  AC_DEFINE(H323_H46018, 1, [Enable H.460.18/.19 support])
else
  H323_H46018=
  AC_MSG_NOTICE(Disabling H.460.18/.19)
fi
AC_SUBST(H323_H46018)

dnl ########################################################################
dnl check for enabling H.460.19 RTP multiplexing

dnl MSWIN_DISPLAY h46019m,H.460.19 RTP multiplexing
dnl MSWIN_DEFINE  h46019m,H323_H46019M

AC_ARG_ENABLE(h46019m,
       [  --disable-h46019m       disable H.460.19 RTP multiplexing])
if test "${enable_h46019m}x" = "x" ; then
  enable_h46019m=$default_h46019m
fi
if test "$enable_h46019m" = "yes" ; then
  H323_H46019M=1
  AC_DEFINE(H323_H46019M, 1, [Enable H.460.19 support])
else
  H323_H46019M=
  AC_MSG_NOTICE(Disabling H.460.19 RTP multiplexing)
fi
AC_SUBST(H323_H46019M)

dnl ########################################################################
dnl check for disabling H.460.23/.24

dnl MSWIN_DISPLAY h46023,H.460.23/.24
dnl MSWIN_DEFINE  h46023,H323_H46023

AC_ARG_ENABLE(h46023,
       [  --disable-h46023        disable H.460.23/.24])
if test "${enable_h46023}x" = "x" ; then
  enable_h46023=$default_h46023
fi
if test "$enable_h46023" = "yes" ; then
  H323_H46023=1
  AC_DEFINE(H323_H46023, 1, [Enable H.460.23 support])
else
  H323_H46023=
  AC_MSG_NOTICE(Disabling H.460.23/.24)
fi
AC_SUBST(H323_H46023)

dnl ########################################################################
dnl check for disabling H.460.25

dnl MSWIN_DISPLAY h46025,H.460.25
dnl MSWIN_DEFINE  h46025,H323_H46025

AC_ARG_ENABLE(h46025,
       [  --disable-h46025        disable H.460.25])
if test "${enable_h46025}x" = "x" ; then
  enable_h46023=$default_h46025
fi
if test "$enable_h46025" = "yes" ; then
  H323_H46025=1
  AC_DEFINE(H323_H46025, 1, [Enable H.460.25 support])
else
  H323_H46025=
  AC_MSG_NOTICE(Disabling H.460.25)
fi

dnl ########################################################################
dnl check for disabling H.460.26

dnl MSWIN_DISPLAY h46026,H.460.26
dnl MSWIN_DEFINE  h46026,H323_H46026

AC_ARG_ENABLE(h46026,
       [  --disable-h46026        disable H.460.26])
if test "${enable_h46026}x" = "x" ; then
  enable_h46023=$default_h46026
fi
if test "$enable_h46026" = "yes" ; then
  H323_H46026=1
  AC_DEFINE(H323_H46026, 1, [Enable H.460.26 support])
else
  H323_H46026=
  AC_MSG_NOTICE(Disabling H.460.26)
fi
AC_SUBST(H323_H46026)

dnl ########################################################################
dnl check for disabling H.460 Presence

dnl MSWIN_DISPLAY h460p,H.460 Presence
dnl MSWIN_DEFINE  h460p,H323_H460P

AC_ARG_ENABLE(h460p,
       [  --enable-h460p          enable H.460 Presence])
if test "${enable_h460p}x" = "x" ; then
  enable_h460p=$default_h460p
fi
if test "$enable_h460p" = "yes" ; then
  H323_H460P=1
  AC_DEFINE(H323_H460P, 1, [Enable H.460P support])
else
  H323_H460P=
  AC_MSG_NOTICE(Disabling H.460 Presence)
fi
AC_SUBST(H323_H460P)

dnl ########################################################################
dnl check for Enable H.460 PreEmption

dnl MSWIN_DISPLAY h460pre,H.460 PreEmption
dnl MSWIN_DEFINE  h460pre,H323_H460PRE

AC_ARG_ENABLE(h460pre,
       [  --enable-h460pre        enable H.460 PreEmption])
if test "${enable_h460pre}x" = "x" ; then
  enable_h460pre=$default_h460pre
fi
if test "$enable_h460pre" = "yes" ; then
  H323_H460PRE=1
  AC_DEFINE(H323_H460PRE, 1, [Enable H.460PRE support])
else
  H323_H460PRE=
  AC_MSG_NOTICE(Disabling H.460 PreEmption)
fi
AC_SUBST(H323_H460PRE)

dnl ########################################################################
dnl check for Enable H.460 Compatibility

dnl MSWIN_DISPLAY h460com,H.460 Compat Feature
dnl MSWIN_DEFINE  h460com,H323_H460COM

AC_ARG_ENABLE(h460com,
       [  --enable-h460com        enable H.460 Compat Feature])
if test "${enable_h460com}x" = "x" ; then
  enable_h460com=$default_h460com
fi
if test "$enable_h460com" = "yes" ; then
  H323_H460COM=1
  AC_DEFINE(H323_H460COM, 1, [Enable H.460COM support])
else
  H323_H460COM=
  AC_MSG_NOTICE(Disabling H.460 Compat Feature)
fi
AC_SUBST(H323_H460COM)

dnl ########################################################################
dnl check for Enable H.460 Text Message

dnl MSWIN_DISPLAY h460im,H.460 Text Message
dnl MSWIN_DEFINE  h460im,H323_H460IM

AC_ARG_ENABLE(h460im,
       [  --enable-h460im        enable H.460 Text Message])
if test "${enable_h460im}x" = "x" ; then
  enable_h460im=$default_h460im
fi
if test "$enable_h460im" = "yes" ; then
  H323_H460IM=1
  AC_DEFINE(H323_H460IM, 1, [Enable H.460IM support])
else
  H323_H460IM=
  AC_MSG_NOTICE(Disabling H.460 Text Message)
fi
AC_SUBST(H323_H460IM)

dnl ########################################################################
dnl check for Enable H.461 ASSET Feature

dnl MSWIN_DISPLAY h461,H.461 ASSET Feature
dnl MSWIN_DEFINE  h461,H323_H461

AC_ARG_ENABLE(h461,
       [  --enable-h461           enable H.461 ASSET Feature])
if test "${enable_h461}x" = "x" ; then
  enable_h461=$default_h461
fi
if test "$enable_h461" = "yes" ; then
  H323_H461=1
  AC_DEFINE(H323_H461, 1, [Enabling H.461 ASSET Feature])
else
  H323_H461=
  AC_MSG_NOTICE(Disabling H.461 ASSET Feature)
fi
AC_SUBST(H323_H461)

dnl ########################################################################
dnl check for disabling H.501

dnl MSWIN_DISPLAY h501,H.501
dnl MSWIN_DEFINE  h501,H323_H501

AC_ARG_ENABLE(h501,
       [  --disable-h501          disable H.501])
if test "${enable_h501}x" = "x" ; then
  enable_h501=$default_h501
fi
if test "$enable_h501" = "yes" ; then
  H323_H501=1
  AC_DEFINE(H323_H501, 1, [Enable H.501 support])
else
  H323_H501=
  AC_MSG_NOTICE(Disabling H.501)
fi
AC_SUBST(H323_H501)

dnl ########################################################################
dnl check for disabling T.38

dnl MSWIN_DISPLAY t38,T.38
dnl MSWIN_DEFINE  t38,H323_T38

AC_ARG_ENABLE(t38,
       [  --disable-t38           disable T.38])
if test "${enable_t38}x" = "x" ; then
  enable_t38=$default_t38
fi
if test "$enable_t38" = "yes" ; then
  AC_MSG_NOTICE(Enabling T.38)
  H323_T38=1
  AC_DEFINE(H323_T38, 1, [Enable T.38 support])
else
  H323_T38=
  AC_MSG_NOTICE(Disabling T.38)
fi
AC_SUBST(H323_T38)

dnl ########################################################################
dnl check for enabling T.120

dnl MSWIN_DISPLAY t120,T.120
dnl MSWIN_DEFINE  t120,H323_T120

AC_ARG_ENABLE(t120,
       [  --disable-t120          disable T.120])
if test "${enable_t120}x" = "x" ; then
  enable_t120=$default_t120
fi
if test "$enable_t120" = "no" ; then
  H323_T120=
  AC_MSG_NOTICE(Disabling T.120)
else
  H323_T20=1
  AC_DEFINE(H323_T120, 1, [Enable T.120 support])
fi
AC_SUBST(H323_T120)

dnl ########################################################################
dnl check for enabling T.140

dnl MSWIN_DISPLAY t140,T.140
dnl MSWIN_DEFINE  t140,H323_T140

AC_ARG_ENABLE(t140,
       [  --disable-t140          disable T.140])
if test "${enable_t140}x" = "x" ; then
  enable_t140=$default_t140
fi
if test "$enable_t140" = "no" ; then
  H323_T140=
  AC_MSG_NOTICE(Disabling T.140)
else
  H323_T140=1
  AC_DEFINE(H323_T140, 1, [Enable T.140 support])
fi
AC_SUBST(H323_T140)

dnl ########################################################################
dnl check for disabling GNUGK Support

dnl MSWIN_DISPLAY gnugk,GNUGKnat
dnl MSWIN_DEFINE  gnugk,H323_GNUGK

AC_ARG_ENABLE(gnugk,
       [  --disable-gnugk         disable GnuGk NAT])
if test "${enable_gnugk}x" = "x" ; then
  enable_gnugk=$default_gnugk
fi
if test "$enable_gnugk" = "yes" ; then
  H323_GNUGK=1
  AC_DEFINE(H323_GNUGK, 1, [Enable GNU GateKeeper NAT support])
else
  H323_GNUGK=
  AC_MSG_NOTICE(Disabling GnuGk NAT)
fi
AC_SUBST(H323_GNUGK)

dnl ########################################################################
dnl check for disabling File Transfer Support

dnl MSWIN_DISPLAY file,FileTransfer
dnl MSWIN_DEFINE  file,H323_FILE

AC_ARG_ENABLE(file,
       [  --disable-file          disable File Transfer])
if test "${enable_file}x" = "x" ; then
  enable_file=$default_file
fi
if test "$enable_file" = "yes" ; then
  H323_FILE=1
  AC_DEFINE(H323_FILE, 1, [Enable H323 file support])
else
  H323_FILE=
  AC_MSG_NOTICE(Disabling File Transfer)
fi
AC_SUBST(H323_FILE)

dnl ########################################################################
dnl look for Speex AEC

dnl MSWIN_DISPLAY    aec,AEC (Acoustic Echo Cancellation)
dnl MSWIN_CHECK_FILE aec,include\speex\speex_echo.h,H323_AEC=1
dnl MSWIN_DIR_SYMBOL aec,AEC_DIR
dnl MSWIN_CHECK_DIR  aec,c:\speex-1.2rc1\

AC_ARG_ENABLE([aec],
	      AS_HELP_STRING([--enable-aec],[enable Acoustic Echo Cancellation]),
	      [enable_aec=$enableval],
	      [enable_aec=$default_aec]
	     )
		 
if test "$enable_aec" = "no" ; then
  AC_MSG_NOTICE(AEC disabled)
else
  if test "x${H323_AEC}" = "x1" ; then
    AC_DEFINE(H323_AEC, 1, "AEC found")
    STDCCFLAGS="$STDCCFLAGS $AEC_CFLAGS"
    ENDLDLIBS="$ENDLDLIBS $AEC_LIBS"
  fi
fi
AC_SUBST(H323_AEC)

dnl ########################################################################
dnl check for disabling TLS support

dnl MSWIN_DISPLAY tls,TLS
dnl MSWIN_DEFINE  tls,H323_TLS

AC_ARG_ENABLE(tls,
       [  --disable-tls           disable TLS support])
if test "${enable_tls}x" = "x" ; then
  enable_tls=$default_tls
fi
if test "$enable_tls" = "yes" ; then
  H323_TLS=1
  AC_DEFINE(H323_TLS, 1, [Enable TLS support])
else
  H323_TLS=
  AC_MSG_NOTICE(Disabling TLS support)
fi
AC_SUBST(H323_TLS)

dnl ########################################################################
dnl make directives

AC_SUBST(STDCCFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(LDLIBS)
AC_SUBST(ENDLDLIBS)
AC_PROG_INSTALL

AC_CONFIG_FILES(openh323u.mak)
AC_CONFIG_FILES(Makefile)


dnl ########################################################################
dnl Header file

AC_CONFIG_HEADERS(include/openh323buildopts.h)


dnl ########################################################################
dnl Output configured files

AC_CONFIG_SUBDIRS(plugins)

AC_OUTPUT()

