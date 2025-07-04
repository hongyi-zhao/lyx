include $(top_srcdir)/config/common.am

EXTRA_DIST = os_cygwin.cpp os_unix.cpp os_win32.cpp os_win32.h \
	CMakeLists.txt tests/CMakeLists.txt tests/supporttest.cmake

noinst_LIBRARIES = liblyxsupport.a


#########################  Qt stuff  #############################
#

MOCHEADER = \
	ConsoleApplicationPrivate.h \
	FileMonitor.h \
	SystemcallPrivate.h

MOCEDFILES = $(MOCHEADER:%.h=moc_%.cpp)

CLEANFILES = $(MOCEDFILES)
BUILT_SOURCES = $(MOCEDFILES)

moc_%.cpp: %.h
	$(AM_V_GEN)$(QT_MOC) -o $@ $<

liblyxsupport_a_DEPENDENCIES = $(MOCEDFILES)

#
##################################################################

AM_CPPFLAGS += -I$(srcdir)/.. $(ICONV_INCLUDES) $(ZLIB_INCLUDES) \
	$(NOD_INCLUDES) $(QT_CPPFLAGS) $(QT_INCLUDES)

liblyxsupport_a_SOURCES = \
	FileMonitor.h \
	FileMonitor.cpp \
	RandomAccessList.h \
	Cache.h \
	Changer.h \
	checksum.cpp \
	checksum.h \
	ConsoleApplication.cpp \
	ConsoleApplication.h \
	ConsoleApplicationPrivate.h \
	convert.cpp \
	convert.h \
	copied_ptr.h \
	counter_reps.cpp \
	counter_reps.h \
	debug.cpp \
	debug.h \
	docstream.cpp \
	docstream.h \
	docstring.cpp \
	docstring.h \
	docstring_list.h \
	environment.h \
	environment.cpp \
	ExceptionMessage.h \
	FileName.cpp \
	FileName.h \
	FileNameList.h \
	filetools.cpp \
	filetools.h \
	ForkedCalls.cpp \
	ForkedCalls.h \
	gettext.cpp \
	gettext.h \
	gzstream.cpp \
	gzstream.h \
	kill.cpp \
	lassert.h \
	lassert.cpp \
	Length.cpp \
	Length.h \
	lengthcommon.cpp \
	Lexer.cpp \
	Lexer.h \
	limited_stack.h \
	lstrings.cpp \
	lstrings.h \
	lyxalgo.h \
	lyxlib.h \
	lyxmagic.h \
	lyxtime.cpp \
	lyxtime.h \
	mute_warning.h \
	mutex.h \
	mutex.cpp \
	Messages.cpp \
	Messages.h \
	numpunct_lyx_char_type.h \
	os.cpp \
	os.h \
	PathChanger.cpp \
	PathChanger.h \
	Package.cpp \
	Package.h \
	ProgressInterface.h \
	pmprof.h \
	qstring_helpers.cpp \
	qstring_helpers.h \
	signals.h \
	socktools.cpp \
	socktools.h \
	Systemcall.cpp \
	Systemcall.h \
	SystemcallPrivate.h \
	TempFile.cpp \
	TempFile.h \
	textutils.h \
	Translator.h \
	Timeout.cpp \
	Timeout.h \
	types.h \
	userinfo.cpp \
	userinfo.h \
	unicode.cpp \
	unicode.h \
	weighted_btree.h

if INSTALL_MACOSX
liblyxsupport_a_SOURCES += \
	AppleSpeller.h \
	AppleSpeller.m \
	AppleScript.h \
	AppleScript.m \
	AppleScriptProxy.cpp \
	AppleSupport.h \
	AppleSupport.m \
	linkback/LinkBack.h \
	linkback/LinkBack.m \
	linkback/LinkBackProxy.h \
	linkback/LinkBackProxy.m \
	linkback/LinkBackServer.h \
	linkback/LinkBackServer.m
endif

############################## Tests ##################################

EXTRA_DIST += \
	tests/test_convert \
	tests/test_filetools \
	tests/test_lstrings \
	tests/test_trivstring \
	tests/regfiles/convert \
	tests/regfiles/filetools \
	tests/regfiles/lstrings \
	tests/regfiles/trivstring


TESTS = \
	tests/test_convert \
	tests/test_filetools \
	tests/test_lstrings \
	tests/test_trivstring

check_PROGRAMS = \
	check_convert \
	check_filetools \
	check_lstrings \
	check_trivstring

if INSTALL_MACOSX
ADD_FRAMEWORKS = \
	-framework QtCore -framework AppKit -framework ApplicationServices \
	-Wl,-rpath,@loader_path/../Frameworks \
	-Wl,-rpath,@executable_path/../Frameworks \
	-Wl,-headerpad_max_install_names
endif

check_convert_LDADD = liblyxsupport.a $(LIBICONV) $(ZLIB_LIBS) $(QT_LIB) $(LIBSHLWAPI) @LIBS@
check_convert_LDFLAGS = $(QT_CORE_LDFLAGS) $(ADD_FRAMEWORKS)
check_convert_SOURCES = \
	tests/check_convert.cpp \
	tests/dummy_functions.cpp

check_filetools_LDADD = liblyxsupport.a $(LIBICONV) $(ZLIB_LIBS) $(QT_LIB) $(LIBSHLWAPI) @LIBS@
check_filetools_LDFLAGS = $(QT_CORE_LDFLAGS) $(ADD_FRAMEWORKS)
check_filetools_SOURCES = \
	tests/check_filetools.cpp \
	tests/dummy_functions.cpp

check_lstrings_LDADD = liblyxsupport.a $(LIBICONV) $(ZLIB_LIBS) $(QT_LIB) $(LIBSHLWAPI) @LIBS@
check_lstrings_LDFLAGS = $(QT_CORE_LDFLAGS) $(ADD_FRAMEWORKS)
check_lstrings_SOURCES = \
	tests/check_lstrings.cpp \
	tests/dummy_functions.cpp

check_trivstring_LDADD = liblyxsupport.a $(LIBICONV) $(ZLIB_LIBS) $(QT_LIB) $(LIBSHLWAPI) @LIBS@
check_trivstring_LDFLAGS = $(QT_CORE_LDFLAGS) $(ADD_FRAMEWORKS)
check_trivstring_SOURCES = \
	tests/check_trivstring.cpp \
	tests/dummy_functions.cpp

makeregfiles: ${check_PROGRAMS}
	for all in ${check_PROGRAMS} ; do \
		./$$all > ${srcdir}/tests/regfiles/$$all ; \
	done
