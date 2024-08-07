include $(top_srcdir)/config/common.am

############################### Core  ##############################

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

noinst_LIBRARIES = liblyxcore.a
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
	main.cpp \
	$(APPLESPELL) \
	$(ASPELL) \
	BiblioInfo.h \
	BiblioInfo.cpp \
	Box.cpp \
	Box.h \
	Compare.cpp \
	Compare.h \
	Dimension.cpp \
	Dimension.h \
	$(ENCHANT) \
	$(HUNSPELL) \
	$(PWL) \
	LaTeXFonts.cpp \
	LaTeXFonts.h \
	Thesaurus.cpp \
	Thesaurus.h

if LYX_WIN_RESOURCE
lyx_SOURCES += lyxwinres.rc
endif

SOURCEFILESCORE = \
	Author.cpp \
	boost.cpp \
	BranchList.cpp \
	Buffer.cpp \
	buffer_funcs.cpp \
	BufferEncodings.cpp \
	BufferList.cpp \
	BufferParams.cpp \
	BufferView.cpp \
	Bullet.cpp \
	Changes.cpp \
	Chktex.cpp \
	CiteEnginesList.cpp \
	CmdDef.cpp \
	Color.cpp \
	Converter.cpp \
	ConverterCache.cpp \
	CoordCache.cpp \
	Counters.cpp \
	Cursor.cpp \
	CursorSlice.cpp \
	CutAndPaste.cpp \
	DepTable.cpp \
	DocIterator.cpp \
	Encoding.cpp \
	ErrorList.cpp \
	Exporter.cpp \
	factory.cpp \
	Floating.cpp \
	FloatList.cpp \
	Font.cpp \
	FontInfo.cpp \
	FontList.cpp \
	Format.cpp \
	FuncRequest.cpp \
	FuncStatus.cpp \
	Graph.cpp \
	IndicesList.cpp \
	InsetIterator.cpp \
	InsetList.cpp \
	Intl.cpp \
	KeyMap.cpp \
	KeySequence.cpp \
	Language.cpp \
	LaTeX.cpp \
	LaTeXFeatures.cpp \
	LaTeXPackages.cpp \
	Layout.cpp \
	LayoutFile.cpp \
	LayoutModuleList.cpp \
	LyX.cpp \
	LyXAction.cpp \
	lyxfind.cpp \
	LyXRC.cpp \
	LyXVC.cpp \
	MetricsInfo.cpp \
	ModuleList.cpp \
	Mover.cpp \
	output.cpp \
	output_docbook.cpp \
	output_latex.cpp \
	output_plaintext.cpp \
	output_xhtml.cpp \
	OutputParams.cpp \
	Paragraph.cpp \
	ParagraphMetrics.cpp \
	ParagraphParameters.cpp \
	ParIterator.cpp \
	PDFOptions.cpp \
	Row.cpp \
	RowPainter.cpp \
	Server.cpp \
	ServerSocket.cpp \
	Session.cpp \
	Spacing.cpp \
	Statistics.cpp \
	TexRow.cpp \
	texstream.cpp \
	Text.cpp \
	TextClass.cpp \
	TextMetrics.cpp \
	TocBackend.cpp \
	TocBuilder.cpp \
	Trans.cpp \
	Undo.cpp \
	VCBackend.cpp \
	version.cpp \
	VSpace.cpp \
	WordList.cpp \
	xml.cpp

HEADERFILESCORE = \
	Author.h \
	BranchList.h \
	Buffer.h \
	buffer_funcs.h \
	BufferEncodings.h \
	BufferList.h \
	BufferParams.h \
	BufferView.h \
	Bullet.h \
	Changes.h \
	Chktex.h \
	Citation.h \
	CiteEnginesList.h \
	CmdDef.h \
	Color.h \
	ColorCode.h \
	ColorSet.h \
	CompletionList.h \
	Converter.h \
	ConverterCache.h \
	CoordCache.h \
	Counters.h \
	Cursor.h \
	CursorSlice.h \
	CutAndPaste.h \
	DepTable.h \
	DispatchResult.h \
	DocIterator.h \
	DocumentClassPtr.h \
	Encoding.h \
	ErrorList.h \
	Exporter.h \
	factory.h \
	Floating.h \
	FloatList.h \
	Font.h \
	FontEnums.h \
	FontInfo.h \
	FontList.h \
	Format.h \
	FuncCode.h \
	FuncRequest.h \
	FuncStatus.h \
	Graph.h \
	IndicesList.h \
	InsetIterator.h \
	InsetList.h \
	Intl.h \
	KeyMap.h \
	KeySequence.h \
	Language.h \
	LaTeX.h \
	LaTeXFeatures.h \
	LaTeXPackages.h \
	Layout.h \
	LayoutEnums.h \
	LayoutFile.h \
	LayoutModuleList.h \
	LyX.h \
	LyXAction.h \
	lyxfind.h \
	LyXRC.h \
	LyXVC.h \
	MetricsInfo.h \
	ModuleList.h \
	Mover.h \
	output.h \
	output_docbook.h \
	output_latex.h \
	output_plaintext.h \
	output_xhtml.h \
	OutputEnums.h \
	OutputParams.h \
	paper.h \
	Paragraph.h \
	ParagraphList.h \
	ParagraphMetrics.h \
	ParagraphParameters.h \
	ParIterator.h \
	PDFOptions.h \
	Row.h \
	RowFlags.h \
	RowPainter.h \
	Server.h \
	ServerSocket.h \
	Session.h \
	Spacing.h \
	SpellChecker.h \
	Statistics.h \
	TexRow.h \
	texstream.h \
	Text.h \
	TextClass.h \
	TextMetrics.h \
	Toc.h \
	TocBackend.h \
	TocBuilder.h \
	Trans.h \
	Undo.h \
	update_flags.h \
	VCBackend.h \
	version.h \
	VSpace.h \
	WordLangTuple.h \
	WordList.h \
	xml.h

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

liblyxcore_a_SOURCES = $(SOURCEFILESCORE) $(HEADERFILESCORE)


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

SOURCEFILESMATHED = \
	mathed/InsetMath.cpp \
	mathed/InsetMathAMSArray.cpp \
	mathed/InsetMathArray.cpp \
	mathed/InsetMathBig.cpp \
	mathed/InsetMathBoldSymbol.cpp \
	mathed/InsetMathBox.cpp \
	mathed/InsetMathBrace.cpp \
	mathed/InsetMathCancel.cpp \
	mathed/InsetMathCancelto.cpp \
	mathed/InsetMathCases.cpp \
	mathed/InsetMathChar.cpp \
	mathed/InsetMathClass.cpp \
	mathed/InsetMathColor.cpp \
	mathed/InsetMathCommand.cpp \
	mathed/InsetMathComment.cpp \
	mathed/InsetMathDecoration.cpp \
	mathed/InsetMathDecoration.h \
	mathed/InsetMathDelim.cpp \
	mathed/InsetMathDiagram.cpp \
	mathed/InsetMathDiff.cpp \
	mathed/InsetMathDots.cpp \
	mathed/InsetMathEnsureMath.cpp \
	mathed/InsetMathEnv.cpp \
	mathed/InsetMathExFunc.cpp \
	mathed/InsetMathExInt.cpp \
	mathed/InsetMathFont.cpp \
	mathed/InsetMathFontOld.cpp \
	mathed/InsetMathFrac.cpp \
	mathed/InsetMathGrid.cpp \
	mathed/InsetMathHull.cpp \
	mathed/InsetMathKern.cpp \
	mathed/InsetMathLefteqn.cpp \
	mathed/InsetMathLim.cpp \
	mathed/InsetMathMacro.cpp \
	mathed/InsetMathMacroArgument.cpp \
	mathed/InsetMathMacroTemplate.cpp \
	mathed/InsetMathMatrix.cpp \
	mathed/InsetMathNest.cpp \
	mathed/InsetMathNumber.cpp \
	mathed/InsetMathOverset.cpp \
	mathed/InsetMathPar.cpp \
	mathed/InsetMathPhantom.cpp \
	mathed/InsetMathRef.cpp \
	mathed/InsetMathRoot.cpp \
	mathed/InsetMathScript.cpp \
	mathed/InsetMathSideset.cpp \
	mathed/InsetMathSize.cpp \
	mathed/InsetMathSpace.cpp \
	mathed/InsetMathSpecialChar.cpp \
	mathed/InsetMathSplit.cpp \
	mathed/InsetMathSqrt.cpp \
	mathed/InsetMathStackrel.cpp \
	mathed/InsetMathString.cpp \
	mathed/InsetMathSubstack.cpp \
	mathed/InsetMathSymbol.cpp \
	mathed/InsetMathTabular.cpp \
	mathed/InsetMathTextsize.cpp \
	mathed/InsetMathUnderset.cpp \
	mathed/InsetMathUnknown.cpp \
	mathed/InsetMathXArrow.cpp \
	mathed/InsetMathXYMatrix.cpp \
	mathed/MacroTable.cpp \
	mathed/MathAtom.cpp \
	mathed/MathAutoCorrect.cpp \
	mathed/MathClass.cpp \
	mathed/MathData.cpp \
	mathed/MathExtern.cpp \
	mathed/MathFactory.cpp \
	mathed/MathParser.cpp \
	mathed/MathRow.cpp \
	mathed/MathStream.cpp \
	mathed/MathSupport.cpp \
	mathed/TextPainter.cpp

HEADERFILESMATHED = \
	mathed/InsetMath.h \
	mathed/InsetMathAMSArray.h \
	mathed/InsetMathArray.h \
	mathed/InsetMathBig.h \
	mathed/InsetMathBoldSymbol.h \
	mathed/InsetMathBox.h \
	mathed/InsetMathBrace.h \
	mathed/InsetMathCancel.h \
	mathed/InsetMathCancelto.h \
	mathed/InsetMathCases.h \
	mathed/InsetMathChar.h \
	mathed/InsetMathClass.h \
	mathed/InsetMathColor.h \
	mathed/InsetMathCommand.h \
	mathed/InsetMathComment.h \
	mathed/InsetMathDelim.h \
	mathed/InsetMathDiagram.h \
	mathed/InsetMathDiff.h \
	mathed/InsetMathDots.h \
	mathed/InsetMathEnsureMath.h \
	mathed/InsetMathEnv.h \
	mathed/InsetMathExFunc.h \
	mathed/InsetMathExInt.h \
	mathed/InsetMathFont.h \
	mathed/InsetMathFontOld.h \
	mathed/InsetMathFrac.h \
	mathed/InsetMathGrid.h \
	mathed/InsetMathHull.h \
	mathed/InsetMathKern.h \
	mathed/InsetMathLefteqn.h \
	mathed/InsetMathLim.h \
	mathed/InsetMathMacro.h \
	mathed/InsetMathMacroArgument.h \
	mathed/InsetMathMacroTemplate.h \
	mathed/InsetMathMatrix.h \
	mathed/InsetMathNest.h \
	mathed/InsetMathNumber.h \
	mathed/InsetMathOverset.h \
	mathed/InsetMathPar.h \
	mathed/InsetMathPhantom.h \
	mathed/InsetMathRef.h \
	mathed/InsetMathRoot.h \
	mathed/InsetMathScript.h \
	mathed/InsetMathSideset.h \
	mathed/InsetMathSize.h \
	mathed/InsetMathSpace.h \
	mathed/InsetMathSpecialChar.h \
	mathed/InsetMathSplit.h \
	mathed/InsetMathSqrt.h \
	mathed/InsetMathStackrel.h \
	mathed/InsetMathString.h \
	mathed/InsetMathSubstack.h \
	mathed/InsetMathSymbol.h \
	mathed/InsetMathTabular.h \
	mathed/InsetMathTextsize.h \
	mathed/InsetMathUnderset.h \
	mathed/InsetMathUnknown.h \
	mathed/InsetMathXArrow.h \
	mathed/InsetMathXYMatrix.h \
	mathed/MacroTable.h \
	mathed/MathAtom.h \
	mathed/MathAutoCorrect.h \
	mathed/MathClass.h \
	mathed/MathCompletionList.h \
	mathed/MathData.h \
	mathed/MathExtern.h \
	mathed/MathFactory.h \
	mathed/MathParser.h \
	mathed/MathParser_flags.h \
	mathed/MathRow.h \
	mathed/MathStream.h \
	mathed/MathSupport.h \
	mathed/ReplaceData.h \
	mathed/TextPainter.h

liblyxmathed_a_SOURCES = $(SOURCEFILESMATHED) $(HEADERFILESMATHED)


############################### Insets  ##############################

noinst_LIBRARIES += liblyxinsets.a

SOURCEFILESINSETS = \
	insets/ExternalSupport.cpp \
	insets/ExternalTemplate.cpp \
	insets/ExternalTransforms.cpp \
	insets/Inset.cpp \
	insets/InsetArgument.cpp \
	insets/InsetBibitem.cpp \
	insets/InsetBibtex.cpp \
	insets/InsetBox.cpp \
	insets/InsetBranch.cpp \
	insets/InsetCaption.cpp \
	insets/InsetCaptionable.cpp \
	insets/InsetCitation.cpp \
	insets/InsetCollapsible.cpp \
	insets/InsetCommand.cpp \
	insets/InsetCommandParams.cpp \
	insets/InsetCounter.cpp \
	insets/InsetERT.cpp \
	insets/InsetExternal.cpp \
	insets/InsetFlex.cpp \
	insets/InsetFloat.cpp \
	insets/InsetFloatList.cpp \
	insets/InsetFoot.cpp \
	insets/InsetFootlike.cpp \
	insets/InsetGraphics.cpp \
	insets/InsetGraphicsParams.cpp \
	insets/InsetHyperlink.cpp \
	insets/InsetInclude.cpp \
	insets/InsetIndex.cpp \
	insets/InsetIndexMacro.cpp \
	insets/InsetInfo.cpp \
	insets/InsetIPA.cpp \
	insets/InsetIPAMacro.cpp \
	insets/InsetLabel.cpp \
	insets/InsetLayout.cpp \
	insets/InsetLine.cpp \
	insets/InsetListings.cpp \
	insets/InsetListingsParams.cpp \
	insets/InsetMarginal.cpp \
	insets/InsetNewline.cpp \
	insets/InsetNewpage.cpp \
	insets/InsetNomencl.cpp \
	insets/InsetNote.cpp \
	insets/InsetPhantom.cpp \
	insets/InsetPreview.cpp \
	insets/InsetQuotes.cpp \
	insets/InsetRef.cpp \
	insets/InsetScript.cpp \
	insets/InsetSeparator.cpp \
	insets/InsetSpace.cpp \
	insets/InsetSpecialChar.cpp \
	insets/InsetTabular.cpp \
	insets/InsetText.cpp \
	insets/InsetTOC.cpp \
	insets/InsetVSpace.cpp \
	insets/InsetWrap.cpp \
	insets/RenderButton.cpp \
	insets/RenderGraphic.cpp \
	insets/RenderPreview.cpp

HEADERFILESINSETS = \
	insets/ExternalSupport.h \
	insets/ExternalTemplate.h \
	insets/ExternalTransforms.h \
	insets/Inset.h \
	insets/InsetArgument.h \
	insets/InsetBibitem.h \
	insets/InsetBibtex.h \
	insets/InsetBox.h \
	insets/InsetBranch.h \
	insets/InsetCaption.h \
	insets/InsetCaptionable.h \
	insets/InsetCitation.h \
	insets/InsetCode.h \
	insets/InsetCollapsible.h \
	insets/InsetCommand.h \
	insets/InsetCommandParams.h \
	insets/InsetCounter.h \
	insets/InsetERT.h \
	insets/InsetExternal.h \
	insets/InsetFlex.h \
	insets/InsetFloat.h \
	insets/InsetFloatList.h \
	insets/InsetFoot.h \
	insets/InsetFootlike.h \
	insets/InsetGraphics.h \
	insets/InsetGraphicsParams.h \
	insets/InsetHyperlink.h \
	insets/InsetInclude.h \
	insets/InsetIndex.h \
	insets/InsetIndexMacro.h \
	insets/InsetInfo.h \
	insets/InsetIPA.h \
	insets/InsetIPAMacro.h \
	insets/InsetLabel.h \
	insets/InsetLayout.h \
	insets/InsetLine.h \
	insets/InsetListings.h \
	insets/InsetListingsParams.h \
	insets/InsetMarginal.h \
	insets/InsetNewline.h \
	insets/InsetNewpage.h \
	insets/InsetNomencl.h \
	insets/InsetNote.h \
	insets/InsetPhantom.h \
	insets/InsetPreview.h \
	insets/InsetQuotes.h \
	insets/InsetRef.h \
	insets/InsetScript.h \
	insets/InsetSeparator.h \
	insets/InsetSpace.h \
	insets/InsetSpecialChar.h \
	insets/InsetTabular.h \
	insets/InsetText.h \
	insets/InsetTOC.h \
	insets/InsetVSpace.h \
	insets/InsetWrap.h \
	insets/RenderBase.h \
	insets/RenderButton.h \
	insets/RenderGraphic.h \
	insets/RenderPreview.h

liblyxinsets_a_SOURCES = $(SOURCEFILESINSETS) $(HEADERFILESINSETS)


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
