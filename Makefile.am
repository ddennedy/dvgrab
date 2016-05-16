AM_CXXFLAGS = -D_REENTRANT -D_FILE_OFFSET_BITS=64
AM_CFLAGS   = -D_REENTRANT -D_FILE_OFFSET_BITS=64

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST       = ChangeLog TODO dvgrab.dox dvgrab.spec dvgrab.1 NEWS
man_MANS         = dvgrab.1
bin_PROGRAMS     = dvgrab
#noinst_PROGRAMS  = riffdump rawdump

dvgrab_SOURCES = affine.h avi.cc avi.h dvframe.cc dvframe.h dvgrab.cc dvgrab.h \
	endian_types.h error.cc error.h filehandler.cc filehandler.h frame.cc frame.h \
	hdvframe.cc hdvframe.h iec13818-1.cc iec13818-1.h iec13818-2.cc iec13818-2.h \
	ieee1394io.cc ieee1394io.h io.c io.h main.cc raw1394util.c raw1394util.h riff.cc \
	riff.h smiltime.cc smiltime.h stringutils.cc stringutils.h v4l2reader.h v4l2reader.cc \
	srt.h srt.cc

AM_CPPFLAGS =	\
	@LIBRAW1394_CFLAGS@ \
	@LIBAVC1394_CFLAGS@ \
	@LIBIEC61883_CFLAGS@ \
	@LIBDV_CFLAGS@ \
	@LIBQUICKTIME_CFLAGS@

dvgrab_LDADD = \
	@LIBRAW1394_LIBS@ \
	@LIBAVC1394_LIBS@ \
	@LIBIEC61883_LIBS@ \
	@LIBDV_LIBS@ \
	@LIBQUICKTIME_LIBS@

#riffdump_SOURCES = error.cc error.h riffdump.cc avi.h riff.h avi.cc riff.cc dvframe.h dvframe.cc frame.h

#rawdump_SOURCES  = rawdump.c

# a C++ formatter

ASTYLEFLAGS = -c -t -b -P

pretty:
	astyle $(ASTYLEFLAGS) $(dvgrab_SOURCES)

# a documentation generator

dox:
	doxygen dvgrab.dox
