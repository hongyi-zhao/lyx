include $(top_srcdir)/config/common.am

SUBDIRS = $(FRONTENDS_SUBDIRS) .

DIST_SUBDIRS = qt .

noinst_LIBRARIES = liblyxfrontends.a

AM_CPPFLAGS += -I$(srcdir)/.. $(ICONV_INCLUDES) $(ZLIB_INCLUDES) $(NOD_INCLUDES)

liblyxfrontends_a_SOURCES = \
	alert.h \
	Application.h \
	CaretGeometry.h \
	FontLoader.h \
	FontMetrics.h \
	Delegates.h \
	InputMethod.h \
	KeyModifier.h \
	KeySymbol.h \
	NullPainter.h \
	Painter.h \
	Clipboard.h \
	Selection.h \
	WorkArea.h \
	WorkAreaManager.cpp \
	WorkAreaManager.h \
	mouse_state.h

################################# Tests ################################

EXTRA_DIST = \
	CMakeLists.txt \
	tests/CMakeLists.txt \
	tests/test_biblio.cmake \
	tests/test_biblio \
	tests/regfiles/biblio

TESTS = \
	tests/test_biblio

check_PROGRAMS = \
	biblio

biblio_LDADD = $(ICONV_LIBS) $(ZLIB_LIBS)
biblio_SOURCES = \
	tests/biblio.cpp

makeregfiles: ${check_PROGRAMS}
	for all in ${check_PROGRAMS} ; do \
		./$$all > ${srcdir}/tests/regfiles/$$all ; \
	done
