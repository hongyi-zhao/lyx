include $(top_srcdir)/config/common.am

############################### LyX ###############################

AM_CPPFLAGS += -I$(top_srcdir)/src
AM_CPPFLAGS += $(BOOST_INCLUDES) $(ICONV_INCLUDES) $(ZLIB_INCLUDES)
AM_CPPFLAGS += $(ENCHANT_CFLAGS) $(HUNSPELL_CFLAGS) $(MYTHES_INCLUDES)
AM_CPPFLAGS += $(NOD_INCLUDES) $(QT_CPPFLAGS) $(QT_CORE_INCLUDES)

if BUILD_CLIENT_SUBDIR
CLIENT = client
endif

SUBDIRS = support frontends . $(CLIENT) tex2lyx convert

EXTRA_DIST = lyx_commit_hash.h.in \
	CMakeLists.txt \
	graphics/CMakeLists.txt \
	insets/CMakeLists.txt \
	mathed/CMakeLists.txt \
	tests/CMakeLists.txt

OTHERLIBS = $(MYTHES_LIBS) $(ENCHANT_LIBS) $(HUNSPELL_LIBS) \
            @LIBS@ $(ICONV_LIBS) $(ZLIB_LIBS) $(SOCKET_LIBS) \
	    $(LIBSHLWAPI) $(LIBPSAPI)

bin_PROGRAMS = lyx

lyx_LDFLAGS = $(AM_LDFLAGS) $(QT_LDFLAGS)

lyx_LDADD = \
	liblyxcore.a \
	liblyxmathed.a \
	liblyxinsets.a \
	frontends/liblyxfrontends.a \
	frontends/qt/liblyxqt.a \
	liblyxgraphics.a \
	support/liblyxsupport.a \
	$(OTHERLIBS) \
	$(QT_LIB)

if LYX_WIN_RESOURCE
.rc.o:
	cp $(top_srcdir)/development/Win32/packaging/icons/lyx.ico .
	cp $(top_srcdir)/development/Win32/packaging/icons/lyx_doc.ico .
	$(RC) -I$(top_builddir) --preprocessor "$(CPP) -xc-header -DRC_INVOKED" $< -o $@
endif

if INSTALL_MACOSX
lyx_LDFLAGS += -framework AppKit -framework ApplicationServices \
	-Wl,-rpath,@loader_path/../Frameworks \
	-Wl,-rpath,@executable_path/../Frameworks \
	-Wl,-headerpad_max_install_names
endif

if INSTALL_MACOSX
APPLESPELL = AppleSpellChecker.cpp AppleSpellChecker.h
endif

if USE_ASPELL
ASPELL = AspellChecker.cpp AspellChecker.h
PWL = PersonalWordList.cpp PersonalWordList.h
endif

if USE_ENCHANT
ENCHANT = EnchantChecker.cpp EnchantChecker.h
PWL = PersonalWordList.cpp PersonalWordList.h
endif

if USE_HUNSPELL
HUNSPELL = HunspellChecker.cpp HunspellChecker.h
PWL = PersonalWordList.cpp PersonalWordList.h
endif

lyx_SOURCES = \
	$(APPLESPELL) \
	$(ASPELL) \
	$(ENCHANT) \
	$(HUNSPELL) \
	$(PWL) \
	Compare.cpp \
	Compare.h \
	main.cpp \
	version.cpp \
	version.h

if LYX_WIN_RESOURCE
lyx_SOURCES += lyxwinres.rc
endif

.PHONY: update_commit_hash

LCH_V_CHK = $(lch__v_CHK_@AM_V@)
lch__v_CHK_ = $(lch__v_CHK_@AM_DEFAULT_V@)
lch__v_CHK_0 = @echo "  CHK      lyx_commit_hash.h";
lch__v_CHK_1 =

update_commit_hash:
	$(LCH_V_CHK)rm -f hash-temp ; \
	hash=`cd "$(top_srcdir)" && git log -1 --pretty=format:%H 2>/dev/null || echo none` ; \
	sed s/@LYX_GIT_COMMIT_HASH@/$$hash/ "$(srcdir)"/lyx_commit_hash.h.in >hash-temp ; \
	cmp -s lyx_commit_hash.h hash-temp || cp hash-temp lyx_commit_hash.h ; \
	rm -f hash-temp

lyx_commit_hash.h: update_commit_hash

BUILT_SOURCES = lyx_commit_hash.h
CLEANFILES = lyx_commit_hash.h

############################### Core ##############################

noinst_LIBRARIES = liblyxcore.a

liblyxcore_a_SOURCES = \
	Author.cpp \
	Author.h \
	BiblioInfo.cpp \
	BiblioInfo.h \
	boost.cpp \
	Box.h \
	BranchList.cpp \
	BranchList.h \
	Buffer.cpp \
	Buffer.h \
	buffer_funcs.cpp \
	buffer_funcs.h \
	BufferEncodings.cpp \
	BufferEncodings.h \
	BufferList.cpp \
	BufferList.h \
	BufferParams.cpp \
	BufferParams.h \
	BufferView.cpp \
	BufferView.h \
	Bullet.cpp \
	Bullet.h \
	Changes.cpp \
	Changes.h \
	Chktex.cpp \
	Chktex.h \
	Citation.h \
	CiteEnginesList.cpp \
	CiteEnginesList.h \
	CmdDef.cpp \
	CmdDef.h \
	Color.cpp \
	Color.h \
	ColorCode.h \
	ColorSet.h \
	CompletionList.h \
	Converter.cpp \
	Converter.h \
	ConverterCache.cpp \
	ConverterCache.h \
	CoordCache.cpp \
	CoordCache.h \
	Counters.cpp \
	Counters.h \
	Cursor.cpp \
	Cursor.h \
	CursorSlice.cpp \
	CursorSlice.h \
	CutAndPaste.cpp \
	CutAndPaste.h \
	DepTable.cpp \
	DepTable.h \
	Dimension.cpp \
	Dimension.h \
	DispatchResult.h \
	DocIterator.cpp \
	DocIterator.h \
	DocumentClassPtr.h \
	Encoding.cpp \
	Encoding.h \
	ErrorList.cpp \
	ErrorList.h \
	Exporter.cpp \
	Exporter.h \
	factory.cpp \
	factory.h \
	Floating.cpp \
	Floating.h \
	FloatList.cpp \
	FloatList.h \
	Font.cpp \
	Font.h \
	FontEnums.h \
	FontInfo.cpp \
	FontInfo.h \
	FontList.cpp \
	FontList.h \
	Format.cpp \
	Format.h \
	FuncCode.h \
	FuncRequest.cpp \
	FuncRequest.h \
	FuncStatus.cpp \
	FuncStatus.h \
	Graph.cpp \
	Graph.h \
	IndicesList.cpp \
	IndicesList.h \
	InsetIterator.cpp \
	InsetIterator.h \
	InsetList.cpp \
	InsetList.h \
	Intl.cpp \
	Intl.h \
	KeyMap.cpp \
	KeyMap.h \
	KeySequence.cpp \
	KeySequence.h \
	Language.cpp \
	Language.h \
	LaTeX.cpp \
	LaTeX.h \
	LaTeXColors.cpp \
	LaTeXColors.h \
	LaTeXFeatures.cpp \
	LaTeXFeatures.h \
	LaTeXFonts.cpp \
	LaTeXFonts.h \
	LaTeXPackages.cpp \
	LaTeXPackages.h \
	Layout.cpp \
	Layout.h \
	LayoutEnums.h \
	LayoutFile.cpp \
	LayoutFile.h \
	LayoutModuleList.cpp \
	LayoutModuleList.h \
	LyX.cpp \
	LyX.h \
	LyXAction.cpp \
	LyXAction.h \
	lyxfind.cpp \
	lyxfind.h \
	LyXRC.cpp \
	LyXRC.h \
	LyXVC.cpp \
	LyXVC.h \
	MetricsInfo.cpp \
	MetricsInfo.h \
	ModuleList.cpp \
	ModuleList.h \
	Mover.cpp \
	Mover.h \
	output.cpp \
	output.h \
	output_docbook.cpp \
	output_docbook.h \
	output_latex.cpp \
	output_latex.h \
	output_plaintext.cpp \
	output_plaintext.h \
	output_xhtml.cpp \
	output_xhtml.h \
	OutputEnums.h \
	OutputParams.cpp \
	OutputParams.h \
	paper.h \
	Paragraph.cpp \
	Paragraph.h \
	ParagraphList.h \
	ParagraphMetrics.cpp \
	ParagraphMetrics.h \
	ParagraphParameters.cpp \
	ParagraphParameters.h \
	ParIterator.cpp \
	ParIterator.h \
	PDFOptions.cpp \
	PDFOptions.h \
	Row.cpp \
	Row.h \
	RowFlags.h \
	RowPainter.cpp \
	RowPainter.h \
	Server.cpp \
	Server.h \
	ServerSocket.cpp \
	ServerSocket.h \
	Session.cpp \
	Session.h \
	Spacing.cpp \
	Spacing.h \
	SpellChecker.h \
	Statistics.cpp \
	Statistics.h \
	TexRow.cpp \
	TexRow.h \
	texstream.cpp \
	texstream.h \
	Text.cpp \
	Text.h \
	TextClass.cpp \
	TextClass.h \
	TextMetrics.cpp \
	TextMetrics.h \
	Thesaurus.cpp \
	Thesaurus.h \
	Toc.h \
	TocBackend.cpp \
	TocBackend.h \
	TocBuilder.cpp \
	TocBuilder.h \
	Trans.cpp \
	Trans.h \
	Undo.cpp \
	Undo.h \
	update_flags.h \
	VCBackend.cpp \
	VCBackend.h \
	VSpace.cpp \
	VSpace.h \
	WordLangTuple.h \
	WordList.cpp \
	WordList.h \
	xml.cpp \
	xml.h

#########################  Qt stuff  ##############################

MOCHEADER = Compare.h PreviewLoader.h

if INSTALL_WINDOWS

MOCHEADER += Server.h
MOCFLAG = -D_WIN32

endif

MOCEDFILES = $(MOCHEADER:%.h=moc_%.cpp)

BUILT_SOURCES += $(MOCEDFILES)
CLEANFILES += $(MOCEDFILES)

moc_PreviewLoader.cpp: graphics/PreviewLoader.h
	$(AM_V_GEN)$(QT_MOC) $(MOCFLAG) -o $@ $<

moc_%.cpp: %.h
	$(AM_V_GEN)$(QT_MOC) $(MOCFLAG) -o $@ $<

liblyxcore_a_DEPENDENCIES = $(MOCEDFILES)

############################### Graphics ##############################

noinst_LIBRARIES += liblyxgraphics.a

liblyxgraphics_a_SOURCES = \
	graphics/epstools.h \
	graphics/epstools.cpp \
	graphics/GraphicsCache.h \
	graphics/GraphicsCache.cpp \
	graphics/GraphicsCacheItem.h \
	graphics/GraphicsCacheItem.cpp \
	graphics/GraphicsConverter.h \
	graphics/GraphicsConverter.cpp \
	graphics/GraphicsImage.h \
	graphics/GraphicsLoader.h \
	graphics/GraphicsLoader.cpp \
	graphics/GraphicsParams.cpp \
	graphics/GraphicsParams.h \
	graphics/GraphicsTypes.h \
	graphics/PreviewImage.h \
	graphics/PreviewImage.cpp \
	graphics/PreviewLoader.h \
	graphics/PreviewLoader.cpp


############################### Mathed  ##############################

noinst_LIBRARIES += liblyxmathed.a

liblyxmathed_a_SOURCES = \
	mathed/InsetMath.cpp \
	mathed/InsetMath.h \
	mathed/InsetMathAMSArray.cpp \
	mathed/InsetMathAMSArray.h \
	mathed/InsetMathArray.cpp \
	mathed/InsetMathArray.h \
	mathed/InsetMathBig.cpp \
	mathed/InsetMathBig.h \
	mathed/InsetMathBoldSymbol.cpp \
	mathed/InsetMathBoldSymbol.h \
	mathed/InsetMathBox.cpp \
	mathed/InsetMathBox.h \
	mathed/InsetMathBrace.cpp \
	mathed/InsetMathBrace.h \
	mathed/InsetMathCancel.cpp \
	mathed/InsetMathCancel.h \
	mathed/InsetMathCancelto.cpp \
	mathed/InsetMathCancelto.h \
	mathed/InsetMathCases.cpp \
	mathed/InsetMathCases.h \
	mathed/InsetMathChar.cpp \
	mathed/InsetMathChar.h \
	mathed/InsetMathClass.cpp \
	mathed/InsetMathClass.h \
	mathed/InsetMathColor.cpp \
	mathed/InsetMathColor.h \
	mathed/InsetMathCommand.cpp \
	mathed/InsetMathCommand.h \
	mathed/InsetMathComment.cpp \
	mathed/InsetMathComment.h \
	mathed/InsetMathDecoration.cpp \
	mathed/InsetMathDecoration.h \
	mathed/InsetMathDelim.cpp \
	mathed/InsetMathDelim.h \
	mathed/InsetMathDiagram.cpp \
	mathed/InsetMathDiagram.h \
	mathed/InsetMathDiff.cpp \
	mathed/InsetMathDiff.h \
	mathed/InsetMathDots.cpp \
	mathed/InsetMathDots.h \
	mathed/InsetMathEnsureMath.cpp \
	mathed/InsetMathEnsureMath.h \
	mathed/InsetMathEnv.cpp \
	mathed/InsetMathEnv.h \
	mathed/InsetMathExFunc.cpp \
	mathed/InsetMathExFunc.h \
	mathed/InsetMathExInt.cpp \
	mathed/InsetMathExInt.h \
	mathed/InsetMathFont.cpp \
	mathed/InsetMathFont.h \
	mathed/InsetMathFontOld.cpp \
	mathed/InsetMathFontOld.h \
	mathed/InsetMathFrac.cpp \
	mathed/InsetMathFrac.h \
	mathed/InsetMathGrid.cpp \
	mathed/InsetMathGrid.h \
	mathed/InsetMathHull.cpp \
	mathed/InsetMathHull.h \
	mathed/InsetMathKern.cpp \
	mathed/InsetMathKern.h \
	mathed/InsetMathLefteqn.cpp \
	mathed/InsetMathLefteqn.h \
	mathed/InsetMathLim.cpp \
	mathed/InsetMathLim.h \
	mathed/InsetMathMacro.cpp \
	mathed/InsetMathMacro.h \
	mathed/InsetMathMacroArgument.cpp \
	mathed/InsetMathMacroArgument.h \
	mathed/InsetMathMacroTemplate.cpp \
	mathed/InsetMathMacroTemplate.h \
	mathed/InsetMathMatrix.cpp \
	mathed/InsetMathMatrix.h \
	mathed/InsetMathNest.cpp \
	mathed/InsetMathNest.h \
	mathed/InsetMathNumber.cpp \
	mathed/InsetMathNumber.h \
	mathed/InsetMathOverset.cpp \
	mathed/InsetMathOverset.h \
	mathed/InsetMathPar.cpp \
	mathed/InsetMathPar.h \
	mathed/InsetMathPhantom.cpp \
	mathed/InsetMathPhantom.h \
	mathed/InsetMathRef.cpp \
	mathed/InsetMathRef.h \
	mathed/InsetMathRoot.cpp \
	mathed/InsetMathRoot.h \
	mathed/InsetMathScript.cpp \
	mathed/InsetMathScript.h \
	mathed/InsetMathSideset.cpp \
	mathed/InsetMathSideset.h \
	mathed/InsetMathSize.cpp \
	mathed/InsetMathSize.h \
	mathed/InsetMathSpace.cpp \
	mathed/InsetMathSpace.h \
	mathed/InsetMathSpecialChar.cpp \
	mathed/InsetMathSpecialChar.h \
	mathed/InsetMathSplit.cpp \
	mathed/InsetMathSplit.h \
	mathed/InsetMathSqrt.cpp \
	mathed/InsetMathSqrt.h \
	mathed/InsetMathStackrel.cpp \
	mathed/InsetMathStackrel.h \
	mathed/InsetMathString.cpp \
	mathed/InsetMathString.h \
	mathed/InsetMathSubstack.cpp \
	mathed/InsetMathSubstack.h \
	mathed/InsetMathSymbol.cpp \
	mathed/InsetMathSymbol.h \
	mathed/InsetMathTabular.cpp \
	mathed/InsetMathTabular.h \
	mathed/InsetMathTextsize.cpp \
	mathed/InsetMathTextsize.h \
	mathed/InsetMathUnderset.cpp \
	mathed/InsetMathUnderset.h \
	mathed/InsetMathUnknown.cpp \
	mathed/InsetMathUnknown.h \
	mathed/InsetMathXArrow.cpp \
	mathed/InsetMathXArrow.h \
	mathed/InsetMathXYMatrix.cpp \
	mathed/InsetMathXYMatrix.h \
	mathed/MacroTable.cpp \
	mathed/MacroTable.h \
	mathed/MathAtom.cpp \
	mathed/MathAtom.h \
	mathed/MathAutoCorrect.cpp \
	mathed/MathAutoCorrect.h \
	mathed/MathClass.cpp \
	mathed/MathClass.h \
	mathed/MathCompletionList.h \
	mathed/MathData.cpp \
	mathed/MathData.h \
	mathed/MathExtern.cpp \
	mathed/MathExtern.h \
	mathed/MathFactory.cpp \
	mathed/MathFactory.h \
	mathed/MathParser.cpp \
	mathed/MathParser.h \
	mathed/MathParser_flags.h \
	mathed/MathRow.cpp \
	mathed/MathRow.h \
	mathed/MathStream.cpp \
	mathed/MathStream.h \
	mathed/MathSupport.cpp \
	mathed/MathSupport.h \
	mathed/ReplaceData.h \
	mathed/TextPainter.cpp \
	mathed/TextPainter.h


############################### Insets  ##############################

noinst_LIBRARIES += liblyxinsets.a

liblyxinsets_a_SOURCES = \
	insets/ExternalSupport.cpp \
	insets/ExternalSupport.h \
	insets/ExternalTemplate.cpp \
	insets/ExternalTemplate.h \
	insets/ExternalTransforms.cpp \
	insets/ExternalTransforms.h \
	insets/Inset.cpp \
	insets/Inset.h \
	insets/InsetArgument.cpp \
	insets/InsetArgument.h \
	insets/InsetBibitem.cpp \
	insets/InsetBibitem.h \
	insets/InsetBibtex.cpp \
	insets/InsetBibtex.h \
	insets/InsetBox.cpp \
	insets/InsetBox.h \
	insets/InsetBranch.cpp \
	insets/InsetBranch.h \
	insets/InsetCaption.cpp \
	insets/InsetCaption.h \
	insets/InsetCaptionable.cpp \
	insets/InsetCaptionable.h \
	insets/InsetCitation.cpp \
	insets/InsetCitation.h \
	insets/InsetCode.h \
	insets/InsetCollapsible.cpp \
	insets/InsetCollapsible.h \
	insets/InsetCommand.cpp \
	insets/InsetCommand.h \
	insets/InsetCommandParams.cpp \
	insets/InsetCommandParams.h \
	insets/InsetCounter.cpp \
	insets/InsetCounter.h \
	insets/InsetERT.cpp \
	insets/InsetERT.h \
	insets/InsetExternal.cpp \
	insets/InsetExternal.h \
	insets/InsetFlex.cpp \
	insets/InsetFlex.h \
	insets/InsetFloat.cpp \
	insets/InsetFloat.h \
	insets/InsetFloatList.cpp \
	insets/InsetFloatList.h \
	insets/InsetFoot.cpp \
	insets/InsetFoot.h \
	insets/InsetFootlike.cpp \
	insets/InsetFootlike.h \
	insets/InsetGraphics.cpp \
	insets/InsetGraphics.h \
	insets/InsetGraphicsParams.cpp \
	insets/InsetGraphicsParams.h \
	insets/InsetHyperlink.cpp \
	insets/InsetHyperlink.h \
	insets/InsetInclude.cpp \
	insets/InsetInclude.h \
	insets/InsetIndex.cpp \
	insets/InsetIndex.h \
	insets/InsetIndexMacro.cpp \
	insets/InsetIndexMacro.h \
	insets/InsetInfo.cpp \
	insets/InsetInfo.h \
	insets/InsetIPA.cpp \
	insets/InsetIPA.h \
	insets/InsetIPAMacro.cpp \
	insets/InsetIPAMacro.h \
	insets/InsetLabel.cpp \
	insets/InsetLabel.h \
	insets/InsetLayout.cpp \
	insets/InsetLayout.h \
	insets/InsetLine.cpp \
	insets/InsetLine.h \
	insets/InsetListings.cpp \
	insets/InsetListings.h \
	insets/InsetListingsParams.cpp \
	insets/InsetListingsParams.h \
	insets/InsetMarginal.cpp \
	insets/InsetMarginal.h \
	insets/InsetNewline.cpp \
	insets/InsetNewline.h \
	insets/InsetNewpage.cpp \
	insets/InsetNewpage.h \
	insets/InsetNomencl.cpp \
	insets/InsetNomencl.h \
	insets/InsetNote.cpp \
	insets/InsetNote.h \
	insets/InsetPhantom.cpp \
	insets/InsetPhantom.h \
	insets/InsetPreview.cpp \
	insets/InsetPreview.h \
	insets/InsetQuotes.cpp \
	insets/InsetQuotes.h \
	insets/InsetRef.cpp \
	insets/InsetRef.h \
	insets/InsetScript.cpp \
	insets/InsetScript.h \
	insets/InsetSeparator.cpp \
	insets/InsetSeparator.h \
	insets/InsetSpace.cpp \
	insets/InsetSpace.h \
	insets/InsetSpecialChar.cpp \
	insets/InsetSpecialChar.h \
	insets/InsetTabular.cpp \
	insets/InsetTabular.h \
	insets/InsetText.cpp \
	insets/InsetText.h \
	insets/InsetTOC.cpp \
	insets/InsetTOC.h \
	insets/InsetVSpace.cpp \
	insets/InsetVSpace.h \
	insets/InsetWrap.cpp \
	insets/InsetWrap.h \
	insets/RenderBase.h \
	insets/RenderButton.cpp \
	insets/RenderButton.h \
	insets/RenderGraphic.cpp \
	insets/RenderGraphic.h \
	insets/RenderPreview.cpp \
	insets/RenderPreview.h


############################## Tests ##################################

EXTRA_DIST += \
	tests/boost.cpp \
	tests/dummy_functions.cpp \
	tests/regfiles/ExternalTransforms \
	tests/regfiles/Length \
	tests/regfiles/ListingsCaption \
	tests/test_ExternalTransforms \
	tests/test_layout \
	tests/test_Length \
	tests/test_ListingsCaption

TESTS = tests/test_ExternalTransforms tests/test_ListingsCaption \
	tests/test_layout tests/test_Length

alltests: check alltests-recursive

alltests-recursive: check_layout
	@srcdir=$(srcdir) $(srcdir)/tests/test_layout; \
	if test $$? -eq 0; then \
		echo -e "====================\nlayout tests passed.\n===================="; \
	else \
		echo -e "====================\nlayout tests failed.\n===================="; \
	fi
	cd tex2lyx; $(MAKE) alltests-recursive

updatetests:
	cd tex2lyx; $(MAKE) updatetests

check_PROGRAMS = \
	check_ExternalTransforms \
	check_Length \
	check_ListingsCaption \
	check_layout

if INSTALL_MACOSX
ADD_FRAMEWORKS = -framework QtGui -framework QtCore -framework AppKit -framework ApplicationServices
endif

TESTS_LIBS = support/liblyxsupport.a \
	$(LIBICONV) @LIBS@ \
	$(ICONV_LIBS) $(ZLIB_LIBS) $(QT_LIB) $(LIBSHLWAPI)

check_layout_CPPFLAGS = $(AM_CPPFLAGS)
check_layout_LDADD = $(check_layout_LYX_OBJS) $(TESTS_LIBS)
check_layout_LDFLAGS = $(QT_LDFLAGS) $(ADD_FRAMEWORKS)
check_layout_SOURCES = \
	tests/boost.cpp \
	tests/check_layout.cpp \
	tests/dummy_functions.cpp
check_layout_LYX_OBJS = \
	insets/InsetLayout.o \
	support/Lexer.o \
	CiteEnginesList.o \
	Color.o \
	Counters.o \
	Floating.o \
	FloatList.o \
	FontInfo.o \
	Layout.o \
	LayoutFile.o \
	ModuleList.o \
	Spacing.o \
	TextClass.o

check_ExternalTransforms_CPPFLAGS = $(AM_CPPFLAGS)
check_ExternalTransforms_LDADD = $(check_ExternalTransforms_LYX_OBJS) $(TESTS_LIBS)
check_ExternalTransforms_LDFLAGS = $(QT_LDFLAGS) $(ADD_FRAMEWORKS)
check_ExternalTransforms_SOURCES = \
	tests/boost.cpp \
	tests/check_ExternalTransforms.cpp \
	tests/dummy_functions.cpp
check_ExternalTransforms_LYX_OBJS = \
	graphics/GraphicsParams.o \
	insets/ExternalTransforms.o

check_Length_CPPFLAGS = $(AM_CPPFLAGS)
check_Length_LDADD = $(TESTS_LIBS)
check_Length_LDFLAGS = $(QT_LDFLAGS) $(ADD_FRAMEWORKS)
check_Length_SOURCES = \
	tests/boost.cpp \
	tests/check_Length.cpp \
	tests/dummy_functions.cpp

check_ListingsCaption_CPPFLAGS = $(AM_CPPFLAGS)
check_ListingsCaption_LDADD = $(check_ListingsCaption_LYX_OBJS) $(TESTS_LIBS)
check_ListingsCaption_LDFLAGS = $(QT_LDFLAGS) $(ADD_FRAMEWORKS)
check_ListingsCaption_SOURCES = \
	tests/boost.cpp \
	tests/check_ListingsCaption.cpp \
	tests/dummy_functions.cpp
check_ListingsCaption_LYX_OBJS =

.PHONY: alltests alltests-recursive updatetests
