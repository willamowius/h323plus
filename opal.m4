dnl ########################################################################
dnl Generic OPAL Macros
dnl ########################################################################

dnl OPAL_MSG_CHECK
dnl Print out a line of text and a result
dnl Arguments: $1 Message to print out
dnl            $2 Result to print out
dnl Return:    none
AC_DEFUN([OPAL_MSG_CHECK],
         [
          AC_MSG_CHECKING([$1])
          AC_MSG_RESULT([$2])
         ])

dnl OPAL_SIMPLE
dnl Change a given variable according to arguments and subst and define it
dnl Arguments: $1 name of configure option
dnl            $2 the variable to change, subst and define
dnl            $3 the configure argument description
dnl            $4 dependency variable #1
dnl            $5 dependency variable #2 
dnl Return:    $$2 The (possibly) changed variable
AC_DEFUN([OPAL_SIMPLE_OPTION],
         [
          if test "x$$2" = "x"; then
            AC_MSG_ERROR([No default specified for $2, please correct configure.ac])
	  fi          
          AC_ARG_ENABLE([$1],
                        [AC_HELP_STRING([--enable-$1],[$3])],
                        [$2=$enableval])

          if test "x$4" != "x"; then
            if test "x$$4" != "xyes"; then
              AC_MSG_NOTICE([$1 support disabled due to disabled dependency $4])
	      $2=no
	    fi
	  fi

          if test "x$5" != "x"; then
            if test "x$$5" != "xyes"; then
              AC_MSG_NOTICE([$1 support disabled due to disabled dependency $5])
	      $2=no
	    fi
	  fi

          OPAL_MSG_CHECK([$3], [$$2])
          AC_SUBST($2)
          if test "x$$2" = "xyes"; then
            AC_DEFINE([$2], [1], [$3])
          fi
         ])

dnl OPAL_ADD_CFLAGS_LIBS
dnl Add options to LIBS and CFLAGS. 
dnl Also add them to PKG_LIBS and PKG_REQUIRES accordingly.
dnl Arguments: $1 CFLAGS to add
dnl            $2 LIBS to add
dnl            $3 PKG library to add
dnl Return:    none
AC_DEFUN([OPAL_ADD_CFLAGS_LIBS],
         [
          CFLAGS="$CFLAGS $1"
          CXXFLAGS="$CXXFLAGS $1"
          LIBS="$LIBS $2"

          if test "x$3" = x; then
            PKG_CFLAGS="$PKG_CFLAGS $1"
            PKG_LIBS="$PKG_LIBS $2"
          else
            PKG_REQUIRES="$PKG_REQUIRES $3"
          fi
         ])

dnl OPAL_GET_LIBNAME
dnl Find out the real name of a library file
dnl Arguments: $1 The prefix for variables referring to the library (e.g. LIBAVCODEC)
dnl            $2 The name of the library (e.g. libavcodec)
dnl            $3 The -l argument(s) of the library (e.g. -lavcodec)
dnl Return:    $$1_LIB_NAME The actual name of the library file
dnl Define:    $1_LIB_NAME The actual name of the library file
AC_DEFUN([OPAL_GET_LIBNAME],
         [
          AC_MSG_CHECKING(filename of $2 library)
          AC_LANG_CONFTEST([int main () {}])
          $CC -o conftest$ac_exeext $CFLAGS $CPPFLAGS $LDFLAGS conftest.$ac_ext $LIBS $3>&AS_MESSAGE_LOG_FD
          if test \! -x conftest$ac_exeext ; then
            AC_MSG_RESULT(cannot determine - using defaults)
          else
            $1_LIB_NAME=`$OBJDUMP -x ./conftest$ac_exeext | grep 'NEEDED.*$2' | awk '{print @S|@2; }'`
            AC_MSG_RESULT($$1_LIB_NAME)
            AC_DEFINE_UNQUOTED([$1_LIB_NAME], ["$$1_LIB_NAME"], [Filename of the $2 library])
          fi
         ])

dnl OPAL_DETERMINE_DEBUG
dnl Determine desired debug level, default is -g -O2
dnl Arguments: 
dnl Return:    $DEFAULT_CFLAGS
dnl            $DEBUG_BUILD

AC_DEFUN([OPAL_DETERMINE_DEBUG],
         [
          AC_ARG_ENABLE([debug],
                        [AC_HELP_STRING([--enable-debug],[Enable debug build])],
                        [DEBUG_BUILD=$enableval],
                        [DEBUG_BUILD=no])
           case "$target_os" in
               solaris*)
                 opal_release_flags="-O3 -DSOLARIS"
                 opal_debug_flags="-g -D_DEBUG -DSOLARIS"
               ;;
               *)
                 opal_release_flags="-Os"
                 opal_debug_flags="-g3 -ggdb -O0 -D_DEBUG"
               ;;
           esac

          DEBUG_CFLAGS="$DEBUG_CFLAGS $opal_debug_flags"
          RELEASE_CFLAGS="$RELEASE_CFLAGS $opal_release_flags"

          if test "x${DEBUG_BUILD}" = xyes; then
            DEFAULT_CFLAGS="$DEFAULT_CFLAGS $opal_debug_flags"
          else
            DEFAULT_CFLAGS="$DEFAULT_CFLAGS $opal_release_flags"
          fi

          OPAL_MSG_CHECK([Debugging support], [$DEBUG_BUILD])
         ])

dnl OPAL_DETERMINE_VERSION
dnl Determine OPAL's version number
dnl Arguments: $OPALDIR
dnl Return:    $MAJOR_VERSION
dnl            $MINOR_VERSION
dnl            $BUILD_NUMBER
dnl            $OPAL_VERSION
AC_DEFUN([OPAL_DETERMINE_VERSION],
         [
          MAJOR_VERSION=`cat $1/version.h | grep MAJOR_VERSION | cut -f3 -d' '`
          MINOR_VERSION=`cat $1/version.h | grep MINOR_VERSION | cut -f3 -d' '`
          BUILD_NUMBER=`cat $1/version.h | grep BUILD_NUMBER | cut -f3 -d' '`
          OPAL_VERSION="${MAJOR_VERSION}.${MINOR_VERSION}.${BUILD_NUMBER}"

          OPAL_MSG_CHECK([OPAL Version], [$OPAL_VERSION])
         ])

dnl OPAL_DETERMINE_PLUGIN_DIR
dnl Determine plugin install directory
dnl Arguments: $OPAL_VERSION
dnl Return:    $PLUGIN_DIR
dnl            $EXPANDED_PLUGIN_DIR
AC_DEFUN([OPAL_DETERMINE_PLUGIN_DIR],
         [
          AC_ARG_WITH([plugin-installdir],
                      AS_HELP_STRING([--with-plugin-installdir=DIR],[Location where plugins are installed, starting at the lib dir]),
                      [PLUGIN_DIR="$withval"],
                      [PLUGIN_DIR="opal-${OPAL_VERSION}"]
                     )

          EXPANDED_PLUGIN_DIR="${libdir}/${PLUGIN_DIR}"
          if test "x${exec_prefix}" = "xNONE" ; then
            if test "x${prefix}" = "xNONE" ; then
              EXPANDED_PLUGIN_DIR=`echo ${EXPANDED_PLUGIN_DIR} | sed s#\\${exec_prefix}#/usr/local#`
            else
              EXPANDED_PLUGIN_DIR=`echo ${EXPANDED_PLUGIN_DIR} | sed s#\\${exec_prefix}#${prefix}#`
            fi
          else
            EXPANDED_PLUGIN_DIR=`echo ${EXPANDED_PLUGIN_DIR} | sed s#\\${exec_prefix}#${exec_prefix}#`
          fi

          OPAL_MSG_CHECK([Plugin install directory], [${EXPANDED_PLUGIN_DIR}])
         ])


dnl OPAL_DETERMINE_LIBNAMES
dnl Determine opal library and symlink names
dnl Arguments: $target_os
dnl            $MACHTYPE
dnl            $SHAREDLIBEXT
dnl            BUILD_TYPE
dnl            BUILD_NUMBER
dnl            $DEBUG_LEVEL
dnl Return:    $OBJ_SUFFIX
dnl            $OPAL_OBJDIR
dnl            $LIB_FILENAME_STATIC
dnl            $LIB_FILENAME_SHARED
dnl            $LIB_FILENAME_SHARED_PAT
AC_DEFUN([OPAL_DETERMINE_LIBNAMES],
         [
          if test "x$1" = "xDEBUG" ; then
            OBJ_SUFFIX="_d"
          else
            OBJ_SUFFIX=""
          fi
          $1_OPAL_OBJDIR="\${OPALDIR}/lib_${OSTYPE}_${MACHTYPE}/obj${OBJ_SUFFIX}"
          OPAL_LIBDIR="\${OPALDIR}/lib_${OSTYPE}_${MACHTYPE}"
          $1_LIB_NAME="libopal${OBJ_SUFFIX}"
          $1_LIB_FILENAME_SHARED="libopal${OBJ_SUFFIX}.${SHAREDLIBEXT}"
          $1_LIB_FILENAME_STATIC="libopal${OBJ_SUFFIX}_s.a"

          if test "x${BUILD_TYPE}" = "x." ; then
            build_suffix=".${BUILD_NUMBER}"
          else
            build_suffix="${BUILD_TYPE}${BUILD_NUMBER}"
          fi

          case "$target_os" in
                  cygwin*|mingw*|darwin*)  
                    $1_LIB_FILENAME_SHARED_PAT="libopal${OBJ_SUFFIX}.${MAJOR_VERSION}.${MINOR_VERSION}${build_suffix}.${SHAREDLIBEXT}" 
                    ;;
                  *)
                    $1_LIB_FILENAME_SHARED_PAT="libopal${OBJ_SUFFIX}.${SHAREDLIBEXT}.${MAJOR_VERSION}.${MINOR_VERSION}${build_suffix}"
                    ;;
          esac

          AC_SUBST(OPAL_LIBDIR)
          AC_SUBST($1_OPAL_OBJDIR)
          AC_SUBST($1_LIB_NAME)
          AC_SUBST($1_LIB_FILENAME_SHARED)
          AC_SUBST($1_LIB_FILENAME_STATIC)
          AC_SUBST($1_LIB_FILENAME_SHARED_PAT)
         ])

dnl OPAL_GCC_VERSION
dnl Verify that GCC version is > 3
dnl Arguments: 
dnl Return:
AC_DEFUN([OPAL_GCC_VERSION],
         [
          if test "x$GXX" = "xyes" ; then
            gcc_version=`$CXX -dumpversion`
            AC_MSG_NOTICE(gcc version is $gcc_version);
            GXX_MAJOR=`echo $gcc_version | sed 's/\..*$//'`
            GXX_MINOR=[`echo $gcc_version | sed -e 's/[0-9][0-9]*\.//' -e 's/\..*$//'`]
            GXX_PATCH=[`echo $gcc_version | sed -e 's/[0-9][0-9]*\.[0-9][0-9]*\.//' -e 's/\..*$//'`]

            dnl cannot compile for less than gcc 3
            AC_MSG_CHECKING(if gcc version is valid)
            if test ${GXX_MAJOR} -lt 3 ; then
              AC_MSG_RESULT(no)
              AC_MSG_ERROR([OPAL requires gcc version 3 or later])
            else
              AC_MSG_RESULT(yes)
            fi
          fi
         ])

dnl OPAL_CHECK_BSR
dnl Check for bit scan intrinsic
dnl Arguments: 
dnl Return:    $1 action if-found
dnl            $2 action if-not-found
AC_DEFUN([OPAL_CHECK_BSR],
         [
          BSR_TEST_SRC="
            #include <stdlib.h>
            int main(int argc, char* argv[]) {
            #ifdef __GNUC__
                    unsigned int val = 0x00000FF0;
                    if (__builtin_clz(val) == 20)
                            exit(0);
            #endif
                    exit(1);
            }"
          AC_RUN_IFELSE([AC_LANG_SOURCE([[$BSR_TEST_SRC]])],[opal_gcc_clz=yes],[opal_gcc_clz=no],[opal_gcc_clz=yes])
          OPAL_MSG_CHECK([for working bit scan intrinsic], [$opal_gcc_clz])
          AS_IF([test AS_VAR_GET([opal_gcc_clz]) = yes], [$1], [$2])[]
         ])


dnl ########################################################################
dnl PTLIB
dnl ########################################################################

dnl OPAL_FIND_PTLIB
dnl Find ptlib, either in PTLIBDIR or whereever on the system
dnl Arguments: 
dnl Return:    $PTLIB_VERSION
dnl            $PTLIB_CFLAGS
dnl            $PTLIB_CXXFLAGS
dnl            $PTLIB_LIBS
dnl            $PTLIB_MACHTYPE
dnl            $PTLIB_OSTYPE
dnl            $PTLIB_LIBS
dnl            $DEBUG_LIBS
dnl            $RELEASE_LIBS
dnl            $DEFAULT_LIBS

AC_DEFUN([OPAL_FIND_PTLIB],
         [
          m4_pattern_allow([PKG_CONFIG_LIBDIR])
	  AC_ARG_VAR([PTLIBDIR], [path to ptlib directory if installed ptlib shall not be used])
          AC_ARG_ENABLE([versioncheck],
                        [AC_HELP_STRING([--enable-versioncheck], [enable ptlib versioncheck])],
                        [PTLIB_VERSION_CHECK=$enableval],
                        [PTLIB_VERSION_CHECK=yes])

          dnl This segment looks for PTLIB in PTLIBDIR
          if test "x${PTLIBDIR}" != "x" ; then

            old_PKG_CONFIG_LIBDIR="${PKG_CONFIG_LIBDIR}"
            export PKG_CONFIG_LIBDIR="${PTLIBDIR}"

            # Should not need to set PKG_CONFIG_PATH, but some systems find the
            # installed versions before the explicit version even though that
            # is what PKG_CONFIG_LIBDIR is supposed to override.
            old_PKG_CONFIG_PATH="${PKG_CONFIG_PATH}"
            export PKG_CONFIG_PATH="${PTLIBDIR}"

            if ! AC_RUN_LOG([$PKG_CONFIG ptlib --exists]); then
              AC_MSG_ERROR([No PTLIB library found in ${PTLIBDIR}])
            fi

            echo "Found PTLIB in PTLIBDIR ${PTLIBDIR}"

            if test "x${PTLIB_VERSION_CHECK}" = "xyes" ; then
              if ! AC_RUN_LOG([$PKG_CONFIG ptlib --atleast-version=${PTLIB_REC_VERSION}]); then
                AC_MSG_ERROR([PTLIB Version check failed, recommended version is at least ${PTLIB_REC_VERSION}])
              fi
            fi

            PTLIB_VERSION=`$PKG_CONFIG ptlib --modversion`
            PTLIB_CFLAGS=`$PKG_CONFIG ptlib --cflags`
            PTLIB_CXXFLAGS=`$PKG_CONFIG ptlib --variable=cxxflags`
            PTLIB_LIBS=`$PKG_CONFIG ptlib --libs`

            RELEASE_LIBS=`$PKG_CONFIG ptlib --libs`
            DEBUG_LIBS=`$PKG_CONFIG ptlib --define-variable=suffix=_d --libs`
            	    
            export PKG_CONFIG_LIBDIR="${old_PKG_CONFIG_LIBDIR}"
            export PKG_CONFIG_PATH="${old_PKG_CONFIG_PATH}"

          dnl This segment looks for PTLIB on the system
          else
            if test "x${PTLIB_VERSION_CHECK}" = "xyes" ; then
              PKG_CHECK_MODULES(PTLIB, ptlib >= ${PTLIB_REC_VERSION})
            else
              PKG_CHECK_MODULES(PTLIB, ptlib)
            fi            

            PTLIB_VERSION=`$PKG_CONFIG ptlib --modversion`
            PTLIB_CXXFLAGS=`$PKG_CONFIG ptlib --variable=cxxflags` 
            DEBUG_LIBS=`$PKG_CONFIG ptlib --define-variable=suffix=_d --libs`
            RELEASE_LIBS="$PTLIB_LIBS"
          fi
          
          if test "x${DEBUG_BUILD}" = xyes; then
            DEFAULT_LIBS="$DEBUG_LIBS"
          else
            DEFAULT_LIBS="$RELEASE_LIBS"
          fi

          echo "Version:  ${PTLIB_VERSION}"
          echo "CFLAGS:   ${PTLIB_CFLAGS}"
          echo "CXXFLAGS: ${PTLIB_CXXFLAGS}"
          echo "DEBUG:    ${DEBUG_LIBS}"
          echo "RELEASE:  ${RELEASE_LIBS}"
         ])


dnl OPAL_CHECK_PTLIB
dnl Check if ptlib was compiled with a specific optional feature
dnl Arguments: $1 Name of feature
dnl            $2 ptlib/pasn.h Header file to include
dnl            $3 Code to test the feature
dnl            $4 Variable to set/define
dnl Return:    $$4
dnl Define:    $4
AC_DEFUN([OPAL_CHECK_PTLIB],
         [
          old_CXXFLAGS="$CXXFLAGS"
          old_LIBS="$LIBS"
          old_LIBS="$LIBS"

          CXXFLAGS="$CXXFLAGS $PTLIB_CFLAGS $PTLIB_CXXFLAGS"
          if test "x${DEBUG_BUILD}" = xyes; then
            LIBS="$LIBS $DEBUG_LIBS"
          else
            LIBS="$LIBS $RELEASE_LIBS"
          fi

          AC_LANG(C++)
          AC_LINK_IFELSE([
                          #include <ptbuildopts.h>
                          #include <ptlib.h>
                          #include <$2>

                          int main()
                          {
                            $3
                          }
                         ], 
                         [opal_ptlib_option=yes],
                         [opal_ptlib_option=no])

          CXXFLAGS="$old_CXXFLAGS"
          LIBS="$old_LIBS"

          OPAL_MSG_CHECK([PTLIB has $1], [$opal_ptlib_option])
					$4="$opal_ptlib_option"
	    		AC_SUBST($4)
	    		if test "x$opal_ptlib_option" = "xyes" ; then
	      		AC_DEFINE([$4], [1], [$1])
	    		fi
	  
         ])

dnl OPAL_CHECK_PTLIB_MANDATORY
dnl Check if ptlib was compiled with a specific mandatory feature
dnl Arguments: $1 Name of feature
dnl            $2 ptlib/pasn.h Header file to include
dnl            $3 Code to test the feature
AC_DEFUN([OPAL_CHECK_PTLIB_MANDATORY],
         [
          old_CXXFLAGS="$CXXFLAGS"
          old_LIBS="$LIBS"

          CXXFLAGS="$CXXFLAGS $PTLIB_CFLAGS $PTLIB_CXXFLAGS"
          if test "x${DEBUG_BUILD}" = xyes; then
            LIBS="$LIBS $DEBUG_LIBS"
          else
            LIBS="$LIBS $RELEASE_LIBS"
          fi

          AC_LANG(C++)
          AC_LINK_IFELSE([
                          #include <ptbuildopts.h>
                          #include <ptlib.h>
                          #include <$2>

                          int main()
                          {
                            $3
                          }
                         ], 
                         [opal_ptlib_option=yes],
                         [opal_ptlib_option=no])

          CXXFLAGS="$old_CXXFLAGS"
          LIBS="$old_LIBS"

          OPAL_MSG_CHECK([PTLIB has $1], [$opal_ptlib_option])
          if test "x$opal_ptlib_option" = "xno" ; then
              echo "  ERROR: compulsory feature from PTLib disabled.";
              exit 1; 
          fi

					])

AC_DEFUN([OPAL_CHECK_PTLIB_EXISTS],
         [
          old_CXXFLAGS="$CXXFLAGS"
          old_LIBS="$LIBS"

          CXXFLAGS="$CXXFLAGS $PTLIB_CFLAGS $PTLIB_CXXFLAGS"
          if test "x${DEBUG_BUILD}" = xyes; then
            LIBS="$LIBS $DEBUG_LIBS"
          else
            LIBS="$LIBS $RELEASE_LIBS"
          fi

          AC_LANG(C++)
          AC_LINK_IFELSE([int main() {}
                         ], 
                         [opal_ptlib_exists=yes],
                         [opal_ptlib_exists=no])

          CXXFLAGS="$old_CXXFLAGS"
          LIBS="$old_LIBS"


	  if test "x$opal_ptlib_exists" != "xyes" ; then
            AC_MSG_ERROR([Could not find a linkable ptlib in specified environment to verify symbols (debug ptlib: ${DEBUG_BUILD})])
	  fi

         ])

dnl OPAL_CHECK_PTLIB_DEFINE
dnl Verify if a specific #define in ptlib is defined
dnl Arguments: $1 define name / description 
dnl            $2 PTLIB define
dnl            $3 OPAL define to set
dnl Return:    
dnl Define:    $3
AC_DEFUN([OPAL_CHECK_PTLIB_DEFINE],
         [
          old_CXXFLAGS="$CXXFLAGS"
          CXXFLAGS="$CXXFLAGS $PTLIB_CFLAGS $PTLIB_CXXFLAGS"
          AC_LANG(C++)
	  AC_TRY_COMPILE([
			  #include <ptbuildopts.h>
			  #include <ptlib.h>
			  #include <iostream>
			 ],
                         [
                          #ifndef $2
		          #error "$2 not defined"
		          #endif
                         ], 
                         [opal_ptlib_option=yes],
                         [opal_ptlib_option=no])

          CXXFLAGS="$old_CXXFLAGS"

          OPAL_MSG_CHECK([PTLIB has option $1], [$opal_ptlib_option])
	  $3="$opal_ptlib_option"
	  AC_SUBST($3)
	  if test "x$opal_ptlib_option" = "xyes" ; then
	    AC_DEFINE([$3], [1], [$1])
	  fi
	  
         ])

dnl ########################################################################
dnl LIBAVCODEC
dnl ########################################################################


dnl OPAL_LIBAVCODEC_HACK
dnl Whether to activate or deactivate the memory alignment hack for libavcodec
dnl Arguments: $LIBAVCODEC_STACKALIGN_HACK The default value
dnl Return:    $LIBAVCODEC_STACKALIGN_HACK The possibly user-mandated value
dnl Define:    LIBAVCODEC_STACKALIGN_HACK The possibly user-mandated value

AC_DEFUN([OPAL_LIBAVCODEC_HACK],
         [
          AC_ARG_ENABLE([libavcodec-stackalign-hack],
                        [AC_HELP_STRING([--enable-libavcodec-stackalign-hack], [Stack alignment hack for libavcodec library])],
                        [LIBAVCODEC_STACKALIGN_HACK=$enableval])
          if test x$LIBAVCODEC_STACKALIGN_HACK = xyes; then
            AC_MSG_NOTICE(libavcodec stack align hack enabled)
            AC_DEFINE([LIBAVCODEC_STACKALIGN_HACK], [1], [Stack alignment hack for libavcodec library])
          else
            AC_MSG_NOTICE(libavcodec stack align hack disabled)
          fi
         ])

dnl OPAL_LIBAVCODEC_SOURCE
dnl Allow the user to specify the libavcodec source dir for full MPEG4 rate control
dnl Arguments: none
dnl Return:    $LIBAVCODEC_SOURCE_DIR The directory
dnl Define:    LIBAVCODEC_SOURCE_DIR The directory
AC_DEFUN([OPAL_LIBAVCODEC_SOURCE],
         [
          AC_MSG_CHECKING(libavcodec source)
          LIBAVCODEC_SOURCE_DIR=
          AC_ARG_WITH([libavcodec-source-dir],
                      [AC_HELP_STRING([--with-libavcodec-source-dir],[Directory with libavcodec source code, for MPEG4 rate control correction])])
          if test -f "$with_libavcodec_source_dir/libavcodec/avcodec.h"
          then
            AC_MSG_RESULT(enabled)
            LIBAVCODEC_SOURCE_DIR="$with_libavcodec_source_dir"
            AC_DEFINE([LIBAVCODEC_HAVE_SOURCE_DIR], [1], [Directory with libavcodec source code, for MPEG4 rate control correction])
          else
            LIBAVCODEC_SOURCE_DIR=
            AC_MSG_RESULT(disabled)
          fi
         ])

dnl OPAL_LIBAVCODEC_HEADER
dnl Find out whether libavcodec headers reside in ffmpeg/ (old) or libavcodec/ (new)
dnl Arguments: $LIBAVCODEC_CFLAGS The cflags for compiling apps with libavcodec
dnl Return:    none
dnl Define:    LIBAVCODEC_HEADER The libavcodec header (e.g. libavcodec/avcodec.h)
AC_DEFUN([OPAL_LIBAVCODEC_HEADER],
         [LIBAVCODEC_HEADER=
          old_CFLAGS="$CFLAGS"
          CFLAGS="$CFLAGS $LIBAVCODEC_CFLAGS"
          AC_CHECK_HEADER([libavcodec/avcodec.h], 
                          [
                           AC_DEFINE([LIBAVCODEC_HEADER], 
                                     ["libavcodec/avcodec.h"],
                                     [The libavcodec header file])
                           LIBAVCODEC_HEADER="libavcodec/avcodec.h"
                          ],
                          [])
          if test x$LIBAVCODEC_HEADER = x; then
            AC_CHECK_HEADER([ffmpeg/avcodec.h], 
                            [
                             AC_DEFINE([LIBAVCODEC_HEADER], 
                                       ["ffmpeg/avcodec.h"],
                                       [The libavcodec header file])
                             LIBAVCODEC_HEADER="ffmpeg/avcodec.h"
                            ])
          fi
          if test x$LIBAVCODEC_HEADER = x; then
            AC_MSG_ERROR([Cannot find libavcodec header file])
          fi
          CFLAGS="$old_CFLAGS"
         ])
         
dnl OPAL_CHECK_LIBAVCODEC
dnl Check if libavcodec has a specific C symbol
dnl Arguments: $LIBAVCODEC_LIBS The libs needed to link to libavcodec
dnl            $1 symbol name
dnl            $2 action if-found
dnl            $3 action if-not-found
dnl Return:    
AC_DEFUN([OPAL_CHECK_LIBAVCODEC],
         [
          AC_MSG_CHECKING(if libavcodec has $1)
  	  got_symbol=no
          AC_LANG_CONFTEST([int main () {}])
          $CC -o conftest$ac_exeext $CFLAGS $CPPFLAGS $LDFLAGS conftest.$ac_ext $LIBS $LIBAVCODEC_LIBS>&AS_MESSAGE_LOG_FD
          if test -x conftest$ac_exeext ; then
            libavcodec_libdir=`$LDD ./conftest$ac_exeext | grep libavcodec | awk '{print @S|@3; }'`
            if test "$libavcodec_libdir" = "not" ; then
              AC_MSG_RESULT([no, please check LD_LIBRARY_PATH])
            else
              symbol=`@S|@NM -D @S|@libavcodec_libdir | grep $1`
              if test "x$symbol" != "x"; then
                got_symbol=yes
	      fi
              AC_MSG_RESULT([$got_symbol])
            fi
          fi
          AS_IF([test AS_VAR_GET([got_symbol]) = yes], [$2], [$3])[]
         ])


dnl ########################################################################
dnl x264
dnl ########################################################################

dnl OPAL_X264_LINKAGE
dnl Whether to statically link the H.264 helper to x264 or to load libx264 dynamically when the helper is executed
dnl Arguments: $X264_LINK_STATIC The default value
dnl Return:    $X264_LINK_STATIC The possibly user-mandated value
dnl Define:    X264_LINK_STATIC The possibly user-mandated value
AC_DEFUN([OPAL_X264_LINKAGE],
         [
          AC_ARG_ENABLE([x264-link-static],
                        [AC_HELP_STRING([--enable-x264-link-static], [Statically link x264 to the plugin. Default for win32.])],
                        [X264_LINK_STATIC=$enableval])
          if test x$X264_LINK_STATIC = xyes; then
            AC_MSG_NOTICE(x264 static linking enabled)
            AC_DEFINE([X264_LINK_STATIC], 
                      [1],
                      [Statically link x264 to the plugin. Default for win32.])
          else
            AC_MSG_NOTICE(x264 static linking disabled)
          fi
         ])

dnl ########################################################################
dnl speex
dnl ########################################################################

dnl OPAL_SPEEX_TYPES
dnl Define necessary typedefs for Speex
dnl Arguments: none
dnl Return:    SIZE16 short or int
dnl            SIZE32 short, int or long
AC_DEFUN([OPAL_SPEEX_TYPES],
         [
          old_CFLAGS="$CFLAGS"
          old_LIBS="$LIBS"
          CFLAGS=
          LIBS=

          AC_CHECK_SIZEOF(short)
          AC_CHECK_SIZEOF(int)
          AC_CHECK_SIZEOF(long)
          AC_CHECK_SIZEOF(long long)

          case 2 in
                  $ac_cv_sizeof_short) SIZE16="short";;
                  $ac_cv_sizeof_int) SIZE16="int";;
          esac

          case 4 in
                  $ac_cv_sizeof_int) SIZE32="int";;
                  $ac_cv_sizeof_long) SIZE32="long";;
                  $ac_cv_sizeof_short) SIZE32="short";;
          esac
          CFLAGS="$old_CFLAGS"
          LIBS="$old_LIBS"

         ])


dnl OPAL_SPEEX_TYPES
dnl Determine whether to use the system or internal speex (can be forced), uses pkg-config
dnl Arguments: none
dnl Return:    $SPEEX_SYSTEM whether system or interal speex shall be used
dnl            $SPEEX_INTERNAL_VERSION Internal speex version
dnl            $SPEEX_SYSTEM_VERSION System speex version (if found)
dnl            $SPEEX_CFLAGS System speex cflags (if using system speex)
dnl            $SPEEX_LIBS System speex libs (if using system speex)
AC_DEFUN([OPAL_DETERMINE_SPEEX],
         [AC_ARG_ENABLE([localspeex],
                        [AC_HELP_STRING([--enable-localspeex],[Force use local version of Speex library rather than system version])],
                        [localspeex=$enableval],
                        [localspeex=])

          AC_MSG_CHECKING(internal Speex version)
          SPEEX_CFLAGS=
          SPEEX_LIBS=
          if test -f "audio/Speex/libspeex/misc.h"; then
            SPEEX_INTERNAL_VERSION=`grep "#define SPEEX_VERSION" audio/Speex/libspeex/misc.h | sed -e 's/^.*speex\-//' -e 's/\".*//'`
    	  elif test -f "src/codec/speex/libspeex/misc.h"; then
            SPEEX_INTERNAL_VERSION=`grep "#define SPEEX_VERSION" src/codec/speex/libspeex/misc.h | sed -e 's/^.*speex\-//' -e 's/\".*//'`
      	  else
      	    AC_MSG_ERROR([Could not find internal speex library])
      	  fi
          AC_MSG_RESULT($SPEEX_INTERNAL_VERSION)

          if test "x${localspeex}" = "xyes" ; then
            AC_MSG_NOTICE(forcing use of local Speex sources)
            SPEEX_SYSTEM=no

          elif test "x${localspeex}" = "xno" ; then
            AC_MSG_NOTICE(forcing use of system Speex library)
            PKG_CHECK_MODULES([SPEEX],
                              [speex],
                              [SPEEX_SYSTEM=yes],
                              [
                              AC_MSG_ERROR([cannot find system speex])
                              ])

          else

            AC_MSG_NOTICE(checking whether system Speex or internal Speex is more recent)
            PKG_CHECK_MODULES([SPEEX],
                              [speex >= $SPEEX_INTERNAL_VERSION],
                              [SPEEX_SYSTEM=yes],
                              [
                              SPEEX_SYSTEM=no
                              AC_MSG_RESULT(internal Speex version is more recent than system Speex or system Speex not found)
                              ])
          fi

          if test "x${SPEEX_SYSTEM}" = "xyes" ; then
            SPEEX_SYSTEM_VERSION=`$PKG_CONFIG speex --modversion`
            AC_MSG_RESULT(using system Speex version $SPEEX_SYSTEM_VERSION)
          fi
         ])


dnl OPAL_DETERMINE_SPEEXDSP
dnl Determine whether to use the system or internal speex dsp lib (can be forced), uses pkg-config
dnl Arguments: none
dnl Return:    $SPEEXDSP_SYSTEM whether system or interal speex dsp lib shall be used
dnl            $SPEEXDSP_CFLAGS System speex cflags (if using system speex)
dnl            $SPEEXDSP_LIBS System speex libs (if using system speex)
AC_DEFUN([OPAL_DETERMINE_SPEEXDSP],
         [SPEEXDSP_SYSTEM=no
          AC_ARG_ENABLE([localspeexdsp],
                        [AC_HELP_STRING([--enable-localspeexdsp],[Force use local version of Speex DSP library for echo cancellation rather than system version])],
                        [localspeexdsp=$enableval],
                        [localspeexdsp=no])

          if test "x$localspeexdsp" = "xno" ; then
            PKG_CHECK_MODULES([SPEEXDSP],
                              [speexdsp],
                              [SPEEXDSP_SYSTEM=yes],
                              [
                               AC_MSG_RESULT(System Speex DSP library not found)
                              ])

            if test "x$SPEEXDSP_SYSTEM" = "xyes" ; then
              old_CFLAGS="$CFLAGS"
              CFLAGS="$CFLAGS $SPEEXDSP_CFLAGS"
              AC_CHECK_HEADERS([speex/speex.h], [AC_DEFINE(OPAL_HAVE_SPEEX_SPEEX_H, [1], [speex/speex.h available])])
              CFLAGS="$old_CFLAGS"
            fi
          fi
         ])

dnl OPAL_SPEEX_FLOAT
dnl Determine whether to use the system or internal speex dsp lib (can be forced), uses pkg-config
dnl Arguments: $SPEEXDSP_CFLAGS
dnl            $SPEEXDSP_LIBS
dnl Return:    $SPEEXDSP_SYSTEM whether system or interal speex dsp lib shall be used
AC_DEFUN([OPAL_SPEEX_FLOAT],
         [
          old_CFLAGS="$CFLAGS"
          old_LIBS="$LIBS"
          CFLAGS="$CFLAGS $SPEEXDSP_CFLAGS -Werror"
          LIBS="$LIBS $SPEEXDSP_LIBS"
          AC_CHECK_HEADERS([speex/speex.h], [speex_inc_dir="speex/"], [speex_inc_dir=])
          AC_LINK_IFELSE([
                          #include <${speex_inc_dir}speex.h>
                          #include <${speex_inc_dir}speex_preprocess.h>
                          #include <stdio.h>
                          int main()
                          {
                            SpeexPreprocessState *st;
                            spx_int16_t *x;
                            float *echo;
                            speex_preprocess(st, x, echo);
                            return 0;
                          }
                          ], [opal_speexdsp_float=yes], [opal_speexdsp_float=no])
          CFLAGS="$old_CFLAGS"
          LIBS="$old_LIBS"
          OPAL_MSG_CHECK([Speex has float], [$opal_speexdsp_float])

          AS_IF([test AS_VAR_GET([opal_speexdsp_float]) = yes], [$1], [$2])[]
         ])

dnl ########################################################################
dnl libdl
dnl ########################################################################

dnl OPAL_FIND_LBDL
dnl Try to find a library containing dlopen()
dnl Arguments: $1 action if-found
dnl            $2 action if-not-found
dnl Return:    $DL_LIBS The libs for dlopen()
AC_DEFUN([OPAL_FIND_LIBDL],
         [
          opal_libdl=no
          AC_CHECK_HEADERS([dlfcn.h], [opal_dlfcn=yes], [opal_dlfcn=no])
          if test "$opal_dlfcn" = yes ; then
            AC_MSG_CHECKING(if dlopen is available)
            AC_LANG(C)
            AC_TRY_COMPILE([#include <dlfcn.h>],
                            [void * p = dlopen("lib", 0);], [opal_dlopen=yes], [opal_dlopen=no])
            if test "$opal_dlopen" = no ; then
              AC_MSG_RESULT(no)
            else
              AC_MSG_RESULT(yes)
              case "$target_os" in
                freebsd*|openbsd*|netbsd*|darwin*)  
                  AC_CHECK_LIB([c],[dlopen],
                              [
                                opal_libdl=yes
                                DL_LIBS="-lc"
                              ],
                              [opal_libdl=no])
                ;;
                *)
                  AC_CHECK_LIB([dl],[dlopen],
                              [
                                opal_libdl=yes
                                DL_LIBS="-ldl"
                              ],
                              [opal_libdl=no])
                ;;
               esac
            fi
          fi
          AS_IF([test AS_VAR_GET([opal_libdl]) = yes], [$1], [$2])[]
         ])


dnl ########################################################################
dnl GSM
dnl ########################################################################

dnl OPAL_FIND_GSM
dnl Try to find an installed libgsm that is compiled with WAV49
dnl Arguments: $1 action if-found
dnl            $2 action if-not-found
dnl Return:    $GSM_CFLAGS
dnl            $GSM_LIBS
AC_DEFUN([OPAL_FIND_GSM],
         [
          opal_gsm=no
          AC_CHECK_LIB(gsm, gsm_create, opal_gsm=yes)
          if test "x$opal_gsm" = "xyes"; then
            AC_MSG_CHECKING(if system GSM library has WAV49)
            old_LIBS=$LIBS
            opal_gsm=no

            LIBS="$LIBS -lgsm"
            AC_RUN_IFELSE(
            [AC_LANG_PROGRAM([[
            #include <gsm/gsm.h>
 ]],[[
            int option = 0;
            gsm handle = gsm_create();
            return (gsm_option(handle, GSM_OPT_WAV49, &option) == -1) ? 1 : 0;
 ]])], opal_gsm=yes) 
            LIBS=$old_LIBS
            AC_MSG_RESULT($opal_gsm)

            if test "x${opal_gsm}" = "xyes" ; then
              GSM_CFLAGS="-I/usr/include/gsm -I/usr/local/include/gsm"
              GSM_LIBS="-lgsm"
            fi
            OPAL_MSG_CHECK([System GSM], [$opal_gsm])
          fi
          AS_IF([test AS_VAR_GET([opal_gsm]) = yes], [$1], [$2])[]
         ])


dnl ########################################################################
dnl SPANDSP
dnl ########################################################################

dnl OPAL_FIND_SPANDSP
dnl Find spandsp
dnl Arguments: $1 action if-found
dnl            $2 action if-not-found
dnl Return:    $SPANDSP_LIBS
AC_DEFUN([OPAL_FIND_SPANDSP],
         [
          saved_LIBS="$LIBS"
          LIBS="$LIBS -lspandsp"
          AC_CHECK_LIB(spandsp, fax_free, [opal_spandsp=yes], [opal_spandsp=no])
          LIBS=$saved_LIBS
          if test "x${opal_spandsp}" = "xyes"; then
              SPANDSP_LIBS="-lspandsp"
              AC_CHECK_HEADERS([spandsp.h], [opal_spandsp=yes], [opal_spandsp=no])
          fi
          AS_IF([test AS_VAR_GET([opal_spandsp]) = yes], [$1], [$2])[]
         ])


dnl ########################################################################
dnl LIBZRTP
dnl ########################################################################

dnl OPAL_FIND_LIBZRTP
dnl Try to find zrtp library
dnl Arguments: 
dnl Return:    $1 action if-found
dnl            $2 action if-not-found
dnl            $ZRTP_INCDIR
dnl            $ZRTP_LIBDIR
dnl            $ZRTP_LIBS
dnl            $ZRTP_CFLAGS
dnl            $BN_INCDIR
dnl            $BN_LIBDIR
AC_DEFUN([OPAL_FIND_LIBZRTP],
         [
          opal_libzrtp=no
          ZRTP_LIBS=
          ZRTP_CFLAGS=
          AC_ARG_WITH([zrtp_includedir],
                      AS_HELP_STRING([--with-zrtp-includedir=DIR],[ZRTP includes dir (default /usr/local/include/zrtp)])
                      ,[AC_SUBST(ZRTP_INCDIR, $withval)]
                      ,[AC_SUBST(ZRTP_INCDIR, "/usr/local/include/zrtp")]
          )
          AC_ARG_WITH([zrtp_libdir],
              AS_HELP_STRING([--with-zrtp-libdir=DIR],[ZRTP3 library dir (deafult /usr/local/lib)])
              ,[AC_SUBST(ZRTP_LIBDIR, $withval)]
              ,[AC_SUBST(ZRTP_LIBDIR, "/usr/local/lib")]
          )
          AC_ARG_WITH([bn_includedir],
              AS_HELP_STRING([--with-bn-includedir=DIR],[bn includes dir (deafult /usr/local/include/zrtp)])
              ,[AC_SUBST(BN_INCDIR, $withval)]
              ,[AC_SUBST(BN_INCDIR, "/usr/local/include/zrtp")]
          )
          AC_ARG_WITH([bn_libdir],
              AS_HELP_STRING([--with-bn-libdir=DIR],[bn library dir (deafult /usr/local/lib)])
              ,[AC_SUBST(BN_LIBDIR, $withval)]
              ,[AC_SUBST(BN_LIBDIR, "/usr/local/lib")]
          )

          dnl Check for the includes presence
          AC_MSG_CHECKING(for zrtp library includes in ${ZRTP_INCDIR})
          if test -f ${ZRTP_INCDIR}/zrtp.h; then
            opal_libzrtp=yes
            ZRTP_LIBS="-lzrtp -lbn"
            ZRTP_CFLAGS="-DBUILD_ZRTP_MUTEXES -DHAS_LIBZRTP -I${ZRTP_INCDIR} -I${BN_INCDIR}"
          fi
          OPAL_MSG_CHECK([libzrtp], [$opal_libzrtp])

          AS_IF([test AS_VAR_GET([opal_libzrtp]) = yes], [$1], [$2])[]
         ])


dnl ########################################################################
dnl LIBSRTP
dnl ########################################################################

AC_DEFUN([OPAL_FIND_LIBSRTP],
         [
          AC_LANG(C)
          AC_COMPILE_IFELSE([
                             AC_LANG_PROGRAM([[#include "srtp/srtp.h"]], 
                                             [[
                                               crypto_policy_t p; p.cipher_key_len = SRTP_MASTER_KEY_LEN; return 0; 
                                             ]]
                                            )
                            ],
                            [opal_libsrtp=yes],
                            [opal_libsrtp=no])

          if test "x${opal_libsrtp}" = "xyes" ; then
            SRTP_LIBS="-lsrtp"
          fi
           AS_IF([test AS_VAR_GET([opal_libsrtp]) = yes], [$1], [$2])[]
         ])

dnl ########################################################################
dnl JAVA
dnl ########################################################################

dnl OPAL_FIND_JAVA
dnl Try to find java headers
dnl Arguments: $1 action if-found
dnl            $2 action if-not-found
dnl Return:    $JAVA_CFLAGS The cflags for java
AC_DEFUN([OPAL_FIND_JAVA],
         [
          opal_java=no
          JAVA_CFLAGS=
          AC_CHECK_HEADERS([jni.h], [opal_java=yes])

          if test "x$opal_java" = "xno" ; then
            if test "x${JDK_ROOT}" = "x" ; then
              JDK_ROOT=${JDK_HOME}
            fi

            if test "x${JDK_ROOT}" != "x" ; then
              AC_CHECK_FILE([${JDK_ROOT}/include/jni.h], 
                            [
                             opal_java=yes
                             JAVA_CFLAGS="-I${JDK_ROOT}/include -I${JDK_ROOT}/include/linux"
                            ])
             fi
           fi
           AS_IF([test AS_VAR_GET([opal_java]) = yes], [$1], [$2])[]
          ])

dnl OPAL_DETERMINE_ILBC
dnl Determine whether to use the system or internal iLBC (can be forced)
dnl Arguments: none
dnl Return:    $ILBC_SYSTEM whether system or internal iLBC shall be used
dnl            $ILBC_CFLAGS system iLBC CFLAGS if using system iLBC
dnl            $ILBC_LIBS   system iLBC LIBS if using system iLBC
AC_DEFUN([OPAL_DETERMINE_ILBC],
         [AC_ARG_ENABLE([localilbc],
                        [AC_HELP_STRING([--enable-localilbc],[Force use local version of iLBC library rather than system version])],
                        [localilbc=$enableval],
                        [localilbc=])

				if test "x${localilbc}" = "xyes" ; then
					AC_MSG_NOTICE(forcing use of local iLBC sources)
					ILBC_SYSTEM=no
				else
					AC_MSG_NOTICE(checking if iLBC is installed)

          saved_LIBS="$LIBS"
          LIBS="$LIBS -lilbc"
          AC_CHECK_LIB(ilbc, iLBC_encode, [has_ilbc=yes], [has_ilbc=no])
          LIBS=$saved_LIBS

          if test "x${has_ilbc}" = "xyes"; then
              AC_CHECK_HEADERS([ilbc/iLBC_decode.h ilbc/iLBC_define.h ilbc/iLBC_encode.h], [has_ilbc=yes], [has_ilbc=no])
          fi

					if test "x${has_ilbc}" = "xyes"; then
						ILBC_CFLAGS=""
						ILBC_LIBS="-lilbc"
						ILBC_SYSTEM=yes
					else
						ILBC_SYSTEM=no
					fi
					OPAL_MSG_CHECK([System iLBC], [$has_ilbc])
				fi
      ])
