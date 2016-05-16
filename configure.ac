dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.69)
AC_INIT(dvgrab, 3.5)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

dnl Checks for programs.
AC_PROG_CC
AC_ISC_POSIX
AC_PROG_CXX
AM_PROG_CC_STDC
AC_C_BIGENDIAN
AC_PROG_INSTALL

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h unistd.h stdio.h)

dnl Checks for libraries.

PKG_CHECK_MODULES(LIBRAW1394, libraw1394 >= 1.1.0)
AC_SUBST(LIBRAW1394_CFLAGS)
AC_SUBST(LIBRAW1394_LIBS)

PKG_CHECK_MODULES(LIBAVC1394, libavc1394 >= 0.5.1)
AC_SUBST(LIBAVC1394_CFLAGS)
AC_SUBST(LIBAVC1394_LIBS)

PKG_CHECK_MODULES(LIBIEC61883, libiec61883 >= 1.0.0)
AC_SUBST(LIBIEC61883_CFLAGS)
AC_SUBST(LIBIEC61883_LIBS)

AC_CHECK_LIB(pthread, pthread_create,,
[
	AC_ERROR(You need the pthread library to compile dvgrab)
])

# LIBDV
AC_ARG_WITH(libdv, [  --with-libdv  Enables libdv support for JPEG output], [enable_libdv=$withval], [enable_libdv=yes])
if test "$enable_libdv" = yes; then
	PKG_CHECK_MODULES(LIBDV, libdv >= 0.103,
			[
					AC_DEFINE(HAVE_LIBDV, 1,[Define to 1 if you have libdv.])
					AC_SUBST(LIBDV_CFLAGS)
					AC_SUBST(LIBDV_LIBS)
			],
			[AC_MSG_RESULT([libdv not installed; I make better dv2 AVI files with libdv 0.103 or newer.])]
	)
fi

# LIBQUICKTIME
AC_ARG_WITH(libquicktime, [  --with-libquicktime  Enables Quicktime support], [enable_libquicktime=$withval], [enable_libquicktime=yes])
if test "$enable_libquicktime" = yes; then
	PKG_CHECK_MODULES(LIBQUICKTIME, [libquicktime >= 0.9.5],
	[
		AC_DEFINE(HAVE_LIBQUICKTIME, 1, [libquicktime.sourceforge.net present])
		AC_SUBST(LIBQUICKTIME_CFLAGS)
		AC_SUBST(LIBQUICKTIME_LIBS)
	],[
		AC_CHECK_HEADERS(quicktime/quicktime.h,
		[
			LIBQUICKTIME_CFLAGS="-I$prefix/include/quicktime"
			AC_CHECK_LIB(quicktime, quicktime_open,
			[
				LIBQUICKTIME_LIBS="-lquicktime"
				AC_DEFINE(HAVE_LIBQUICKTIME, 1, [Heroine Virtual Quicktime4Linux present])
			],[
				AC_CHECK_LIB(quicktimehv, quicktime_open,
				[
					LIBQUICKTIME_LIBS="-lquicktimehv"
					AC_DEFINE(HAVE_LIBQUICKTIME, 1, [cvs.cinelerra.org Quicktime4Linux present])
				],[
					AC_WARN([libquicktime missing; install libquicktime or quicktime4linux to support Quicktime files.])
				])
			])
		],[
			AC_WARN($LIBQUICKTIME_PKG_ERRORS)
			AC_WARN([libquicktime missing; install libquicktime or quicktime4linux to support Quicktime files.])
		])
	])
fi
AC_SUBST(LIBQUICKTIME_CFLAGS)
AC_SUBST(LIBQUICKTIME_LIBS)

# LIBJPEG
AC_ARG_WITH(libjpeg, [  --with-libjpeg  Enables JPEG support], [enable_libjpeg=$withval], [enable_libjpeg=yes])
if test "$enable_libjpeg" = yes; then
	AC_CHECK_HEADERS(jpeglib.h,,
	[
		AC_WARN(jpeglib headers missing; install jpeglib to save to JPEG files.)
	])
	AC_CHECK_LIB(jpeg, jpeg_CreateCompress,,
	[
		AC_WARN(jpeglib missing; install jpeglib to save to JPEG files.)
	])
fi

# V4L
AC_CHECK_HEADERS(linux/videodev2.h,,
[
	AC_WARN(V4L2 headers missing; install linux 2.6 headers to use USB.)
])


# EFENCE
AC_ARG_WITH(efence,[  --with-efence        Use ElectricFence for debugging support.],
[
	AC_CHECK_LIB(efence,free,,
	[
		AC_ERROR(efence not found)
	])
])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(mktime)

AC_OUTPUT(Makefile)
