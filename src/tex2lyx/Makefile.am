include $(top_srcdir)/config/common.am

EXTRA_DIST = TODO.txt tex2lyx.1in $(TEST_FILES) $(TEST_RESULTS) \
	CMakeLists.txt test/CMakeLists.txt

#noinst_LIBRARIES = libtexparser.a
#
#libtexparser_a_SOURCES = \
#	Parser.cpp \
#	Parser.h
#
#tex2lyx_LDADD = libtexparser.a

man_MANS = tex2lyx.1

bin_PROGRAMS = tex2lyx

AM_CPPFLAGS += -I$(top_srcdir)/src/tex2lyx \
	-I$(top_srcdir)/src -I$(top_builddir) -I$(top_builddir)/src \
	$(ICONV_INCLUDES) $(ZLIB_INCLUDES) $(NOD_INCLUDES)

TEST_FILES = \
	test/runtests.cmake \
	test/runtests.py \
	test/algo2e.tex \
	test/beamer.tex \
	test/box-color-size-space-align.tex \
	test/CJK.tex \
	test/CJKutf8.tex \
	test/DummyDocument.tex \
	test/Dummy~Document.tex \
	test/foo.eps \
	test/foo.png \
	test/listpreamble.tex \
	test/test-insets.tex \
	test/test-insets-basic.tex \
	test/test.ltx \
	test/test-memoir.tex \
	test/test-minted.tex \
	test/test-modules.tex \
	test/test-refstyle-theorems.tex \
	test/test-scr.tex \
	test/test-structure.tex \
	test/verbatim.tex \
	test/XeTeX-polyglossia.tex \
	test/xfigtest.fig \
	test/xfigtest.pstex \
	test/xfigtest.pstex_t

TEST_RESULTS = \
	test/algo2e.lyx.lyx \
	test/beamer.lyx.lyx \
	test/box-color-size-space-align.lyx.lyx \
	test/CJK.lyx.lyx \
	test/CJKutf8.lyx.lyx \
	test/DummyDocument.lyx.lyx \
	test/Dummy~Document.lyx.lyx \
	test/listpreamble.lyx.lyx \
	test/test-insets.lyx.lyx \
	test/test-insets-basic.lyx.lyx \
	test/test.lyx.lyx \
	test/test-memoir.lyx.lyx \
	test/test-minted.lyx.lyx \
	test/test-modules.lyx.lyx \
	test/test-refstyle-theorems.lyx.lyx \
	test/test-scr.lyx.lyx \
	test/test-structure.lyx.lyx \
	test/verbatim.lyx.lyx \
	test/XeTeX-polyglossia.lyx.lyx

alltests: check alltests-recursive

alltests-recursive: tex2lyx
	if test "$(srcdir)" = "$(builddir)"; then \
		echo "tex2lyx tests require a separate build directory"; \
		exit 1; \
	fi
	@$(PYTHON) "$(srcdir)/test/runtests.py"; \
	if test $$? -eq 0; then \
		echo -e "=====================\ntex2lyx tests passed.\n====================="; \
	else \
		echo -e "=====================\ntex2lyx tests failed.\n====================="; \
	fi

updatetests: tex2lyx
	$(PYTHON) "$(srcdir)/test/runtests.py" ./tex2lyx "$(top_srcdir)/lib/scripts" "$(srcdir)/test"

LYX_OBJS = \
	../graphics/GraphicsParams.o \
	../insets/ExternalTemplate.o \
	../insets/ExternalTransforms.o \
	../insets/InsetLayout.o \
	../support/Lexer.o \
	../Author.o \
	../CiteEnginesList.o \
	../Color.o \
	../Counters.o \
	../Encoding.o \
	../FloatList.o \
	../Floating.o \
	../FontInfo.o \
	../LaTeXPackages.o \
	../LaTeXColors.o \
	../Layout.o \
	../LayoutFile.o \
	../LayoutModuleList.o \
	../ModuleList.o \
	../Spacing.o \
	../TextClass.o \
	../version.o

tex2lyx_SOURCES = \
	Context.cpp \
	Context.h \
	dummy_impl.cpp \
	math.cpp \
	Parser.cpp \
	Parser.h \
	Preamble.cpp \
	Preamble.h \
	table.cpp \
	tex2lyx.cpp \
	tex2lyx.h \
	text.cpp

tex2lyx_LDADD = \
	$(LYX_OBJS) \
	$(top_builddir)/src/support/liblyxsupport.a \
	$(LIBICONV) \
	$(QT_LIB) $(QT_LDFLAGS) \
	@LIBS@ $(ICONV_LIBS) $(ZLIB_LIBS) $(LIBSHLWAPI) $(LIBPSAPI)

if INSTALL_MACOSX
tex2lyx_LDFLAGS = -framework AppKit \
	-Wl,-rpath,@loader_path/../Frameworks \
	-Wl,-rpath,@executable_path/../Frameworks \
	-Wl,-headerpad_max_install_names
endif

.PHONY: alltests alltests-recursive updatetests
