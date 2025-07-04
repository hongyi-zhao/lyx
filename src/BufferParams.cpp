/**
 * \file BufferParams.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alfredo Braunstein
 * \author Lars Gullik Bjønnes
 * \author Jean-Marc Lasgouttes
 * \author John Levon
 * \author André Pönitz
 * \author Martin Vermeer
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "BufferParams.h"

#include "Author.h"
#include "LayoutFile.h"
#include "BranchList.h"
#include "Buffer.h"
#include "Bullet.h"
#include "CiteEnginesList.h"
#include "Color.h"
#include "ColorSet.h"
#include "Converter.h"
#include "Encoding.h"
#include "Format.h"
#include "IndicesList.h"
#include "Language.h"
#include "LaTeXColors.h"
#include "LaTeXFeatures.h"
#include "LaTeXFonts.h"
#include "Font.h"
#include "LyXRC.h"
#include "OutputParams.h"
#include "Spacing.h"
#include "buffer_funcs.h"
#include "texstream.h"
#include "TexRow.h"
#include "VSpace.h"
#include "PDFOptions.h"

#include "frontends/alert.h"

#include "insets/InsetListingsParams.h"
#include "insets/InsetQuotes.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/Length.h"
#include "support/Lexer.h"
#include "support/Messages.h"
#include "support/mutex.h"
#include "support/Package.h"
#include "support/Translator.h"
#include "support/lstrings.h"

#include <algorithm>
#include <sstream>

using namespace std;
using namespace lyx::support;


static char const * const string_paragraph_separation[] = {
	"indent", "skip", ""
};


static char const * const string_quotes_style[] = {
	"english", "swedish", "german", "polish", "swiss", "danish", "plain",
	"british", "swedishg", "french", "frenchin", "russian", "cjk", "cjkangle",
	"hungarian", "hebrew", ""
};


static char const * const string_papersize[] = {
	"default", "custom", "letter", "legal", "executive",
	"a0", "a1", "a2", "a3", "a4", "a5", "a6",
	"b0", "b1", "b2", "b3", "b4", "b5", "b6",
	"c0", "c1", "c2", "c3", "c4", "c5", "c6",
	"b0j", "b1j", "b2j", "b3j", "b4j", "b5j", "b6j", ""
};


static char const * const string_papersize_geometry[] = {
	"default", "custom", "letterpaper", "legalpaper", "executivepaper",
	"a0paper", "a1paper", "a2paper", "a3paper", "a4paper", "a5paper", "a6paper",
	"b0paper", "b1paper", "b2paper", "b3paper", "b4paper", "b5paper", "b6paper",
	"c0paper", "c1paper", "c2paper", "c3paper", "c4paper", "c5paper", "c6paper",
	"b0j", "b1j", "b2j", "b3j", "b4j", "b5j", "b6j", ""
};


static char const * const string_orientation[] = {
	"portrait", "landscape", ""
};


static char const * const tex_graphics[] = {
	"default", "dvialw", "dvilaser", "dvipdf", "dvipdfm", "dvipdfmx",
	"dvips", "dvipsone", "dvitops", "dviwin", "dviwindo", "dvi2ps", "emtex",
	"ln", "oztex", "pctexhp", "pctexps", "pctexwin", "pctex32", "pdftex",
	"psprint", "pubps", "tcidvi", "textures", "truetex", "vtex", "xdvi",
	"xetex", "none", ""
};


namespace lyx {

// Local translators
namespace {

// Paragraph separation
typedef Translator<string, BufferParams::ParagraphSeparation> ParSepTranslator;


ParSepTranslator const init_parseptranslator()
{
	ParSepTranslator translator
		(string_paragraph_separation[0], BufferParams::ParagraphIndentSeparation);
	translator.addPair(string_paragraph_separation[1], BufferParams::ParagraphSkipSeparation);
	return translator;
}


ParSepTranslator const & parseptranslator()
{
	static ParSepTranslator const translator =
		init_parseptranslator();
	return translator;
}


// Quotes style
typedef Translator<string, QuoteStyle> QuotesStyleTranslator;


QuotesStyleTranslator const init_quotesstyletranslator()
{
	QuotesStyleTranslator translator
		(string_quotes_style[0], QuoteStyle::English);
	translator.addPair(string_quotes_style[1], QuoteStyle::Swedish);
	translator.addPair(string_quotes_style[2], QuoteStyle::German);
	translator.addPair(string_quotes_style[3], QuoteStyle::Polish);
	translator.addPair(string_quotes_style[4], QuoteStyle::Swiss);
	translator.addPair(string_quotes_style[5], QuoteStyle::Danish);
	translator.addPair(string_quotes_style[6], QuoteStyle::Plain);
	translator.addPair(string_quotes_style[7], QuoteStyle::British);
	translator.addPair(string_quotes_style[8], QuoteStyle::SwedishG);
	translator.addPair(string_quotes_style[9], QuoteStyle::French);
	translator.addPair(string_quotes_style[10], QuoteStyle::FrenchIN);
	translator.addPair(string_quotes_style[11], QuoteStyle::Russian);
	translator.addPair(string_quotes_style[12], QuoteStyle::CJK);
	translator.addPair(string_quotes_style[13], QuoteStyle::CJKAngle);
	translator.addPair(string_quotes_style[14], QuoteStyle::Hungarian);
	translator.addPair(string_quotes_style[15], QuoteStyle::Hebrew);
	return translator;
}


QuotesStyleTranslator const & quotesstyletranslator()
{
	static QuotesStyleTranslator const translator =
		init_quotesstyletranslator();
	return translator;
}


// Paper size
typedef Translator<string, PAPER_SIZE> PaperSizeTranslator;


static PaperSizeTranslator initPaperSizeTranslator()
{
	PaperSizeTranslator translator(string_papersize[0], PAPER_DEFAULT);
	translator.addPair(string_papersize[1], PAPER_CUSTOM);
	translator.addPair(string_papersize[2], PAPER_USLETTER);
	translator.addPair(string_papersize[3], PAPER_USLEGAL);
	translator.addPair(string_papersize[4], PAPER_USEXECUTIVE);
	translator.addPair(string_papersize[5], PAPER_A0);
	translator.addPair(string_papersize[6], PAPER_A1);
	translator.addPair(string_papersize[7], PAPER_A2);
	translator.addPair(string_papersize[8], PAPER_A3);
	translator.addPair(string_papersize[9], PAPER_A4);
	translator.addPair(string_papersize[10], PAPER_A5);
	translator.addPair(string_papersize[11], PAPER_A6);
	translator.addPair(string_papersize[12], PAPER_B0);
	translator.addPair(string_papersize[13], PAPER_B1);
	translator.addPair(string_papersize[14], PAPER_B2);
	translator.addPair(string_papersize[15], PAPER_B3);
	translator.addPair(string_papersize[16], PAPER_B4);
	translator.addPair(string_papersize[17], PAPER_B5);
	translator.addPair(string_papersize[18], PAPER_B6);
	translator.addPair(string_papersize[19], PAPER_C0);
	translator.addPair(string_papersize[20], PAPER_C1);
	translator.addPair(string_papersize[21], PAPER_C2);
	translator.addPair(string_papersize[22], PAPER_C3);
	translator.addPair(string_papersize[23], PAPER_C4);
	translator.addPair(string_papersize[24], PAPER_C5);
	translator.addPair(string_papersize[25], PAPER_C6);
	translator.addPair(string_papersize[26], PAPER_JISB0);
	translator.addPair(string_papersize[27], PAPER_JISB1);
	translator.addPair(string_papersize[28], PAPER_JISB2);
	translator.addPair(string_papersize[29], PAPER_JISB3);
	translator.addPair(string_papersize[30], PAPER_JISB4);
	translator.addPair(string_papersize[31], PAPER_JISB5);
	translator.addPair(string_papersize[32], PAPER_JISB6);
	return translator;
}


PaperSizeTranslator const & papersizetranslator()
{
	static PaperSizeTranslator const translator =
		initPaperSizeTranslator();
	return translator;
}


// Paper orientation
typedef Translator<string, PAPER_ORIENTATION> PaperOrientationTranslator;


PaperOrientationTranslator const init_paperorientationtranslator()
{
	PaperOrientationTranslator translator(string_orientation[0], ORIENTATION_PORTRAIT);
	translator.addPair(string_orientation[1], ORIENTATION_LANDSCAPE);
	return translator;
}


PaperOrientationTranslator const & paperorientationtranslator()
{
	static PaperOrientationTranslator const translator =
	    init_paperorientationtranslator();
	return translator;
}


// Page sides
typedef Translator<int, PageSides> SidesTranslator;


SidesTranslator const init_sidestranslator()
{
	SidesTranslator translator(1, OneSide);
	translator.addPair(2, TwoSides);
	return translator;
}


SidesTranslator const & sidestranslator()
{
	static SidesTranslator const translator = init_sidestranslator();
	return translator;
}


// LaTeX packages
typedef Translator<int, BufferParams::Package> PackageTranslator;


PackageTranslator const init_packagetranslator()
{
	PackageTranslator translator(0, BufferParams::package_off);
	translator.addPair(1, BufferParams::package_auto);
	translator.addPair(2, BufferParams::package_on);
	return translator;
}


PackageTranslator const & packagetranslator()
{
	static PackageTranslator const translator =
		init_packagetranslator();
	return translator;
}


// Spacing
typedef Translator<string, Spacing::Space> SpaceTranslator;


SpaceTranslator const init_spacetranslator()
{
	SpaceTranslator translator("default", Spacing::Default);
	translator.addPair("single", Spacing::Single);
	translator.addPair("onehalf", Spacing::Onehalf);
	translator.addPair("double", Spacing::Double);
	translator.addPair("other", Spacing::Other);
	return translator;
}


SpaceTranslator const & spacetranslator()
{
	static SpaceTranslator const translator = init_spacetranslator();
	return translator;
}


bool inSystemDir(FileName const & document_dir, string & system_dir)
{
	// A document is assumed to be in a system LyX directory (not
	// necessarily the system directory of the running instance)
	// if both "configure.py" and "chkconfig.ltx" are found in
	// either document_dir/../ or document_dir/../../.
	// If true, the system directory path is returned in system_dir
	// with a trailing path separator.

	string const msg = "Checking whether document is in a system dir...";

	string dir = document_dir.absFileName();

	for (int i = 0; i < 3; ++i) {
		dir = addPath(dir, "..");
		if (!fileSearch(dir, "configure.py").empty() &&
		    !fileSearch(dir, "chkconfig.ltx").empty()) {
			LYXERR(Debug::FILES, msg << " yes");
			system_dir = addPath(FileName(dir).realPath(), "");
			return true;
		}
	}

	LYXERR(Debug::FILES, msg << " no");
	system_dir = string();
	return false;
}

} // namespace


class BufferParams::Impl
{
public:
	Impl();

	AuthorList authorlist;
	AuthorMap authormap;
	BranchList branchlist;
	WordLangTable spellignore;
	Bullet temp_bullets[4];
	Bullet user_defined_bullets[4];
	IndicesList indiceslist;
	Spacing spacing;
	Length parindent;
	Length mathindent;
	/** This is the amount of space used for paragraph_separation "skip",
	 * and for detached paragraphs in "indented" documents.
	 */
	VSpace defskip;
	PDFOptions pdfoptions;
	LayoutFileIndex baseClass_;
	FormatList exportableFormatList;
	FormatList viewableFormatList;
	bool isViewCacheValid;
	bool isExportCacheValid;
};


BufferParams::Impl::Impl()
	: defskip(VSpace::MEDSKIP), baseClass_(string("")),
	  isViewCacheValid(false), isExportCacheValid(false)
{
	// set initial author
	// FIXME UNICODE
	authorlist.record(Author(from_utf8(lyxrc.user_name),
				 from_utf8(lyxrc.user_email),
				 from_utf8(lyxrc.user_initials)));
	// set comparison author
	authorlist.record(Author(from_utf8("Document Comparison"),
				 docstring(), docstring()));
}


BufferParams::Impl *
BufferParams::MemoryTraits::clone(BufferParams::Impl const * ptr)
{
	LBUFERR(ptr);
	return new BufferParams::Impl(*ptr);
}


void BufferParams::MemoryTraits::destroy(BufferParams::Impl * ptr)
{
	delete ptr;
}


BufferParams::BufferParams()
	: pimpl_(new Impl)
{
	setBaseClass(defaultBaseclass());
	cite_engine_ = "basic";
	cite_engine_type_ = ENGINE_TYPE_DEFAULT;
	makeDocumentClass();
	paragraph_separation = ParagraphIndentSeparation;
	is_math_indent = false;
	math_numbering_side = DEFAULT;
	quotes_style = QuoteStyle::English;
	dynamic_quotes = false;
	fontsize = "default";

	/*  PaperLayout */
	papersize = PAPER_DEFAULT;
	orientation = ORIENTATION_PORTRAIT;
	use_geometry = false;
	biblio_style = string();
	use_bibtopic = false;
	multibib = string();
	use_indices = false;
	use_memindex = false;
	save_transient_properties = true;
	track_changes = false;
	output_changes = false;
	change_bars = false;
	postpone_fragile_content = true;
	use_default_options = true;
	maintain_unincluded_children = CM_None;
	secnumdepth = 3;
	tocdepth = 3;
	language = default_language;
	fontenc = "auto";
	fonts_roman[0] = "default";
	fonts_roman[1] = "default";
	fonts_sans[0] = "default";
	fonts_sans[1] = "default";
	fonts_typewriter[0] = "default";
	fonts_typewriter[1] = "default";
	fonts_math[0] = "auto";
	fonts_math[1] = "auto";
	fonts_default_family = "default";
	useNonTeXFonts = false;
	use_microtype = false;
	use_dash_ligatures = true;
	fonts_expert_sc = false;
	fonts_roman_osf = false;
	fonts_sans_osf = false;
	fonts_typewriter_osf = false;
	fonts_sans_scale[0] = 100;
	fonts_sans_scale[1] = 100;
	fonts_typewriter_scale[0] = 100;
	fonts_typewriter_scale[1] = 100;
	inputenc = "utf8";
	lang_package = "default";
	graphics_driver = "default";
	default_output_format = "default";
	bibtex_command = "default";
	index_command = "default";
	sides = OneSide;
	columns = 1;
	listings_params = string();
	pagestyle = "default";
	tablestyle = "default";
	float_alignment = "class";
	float_placement = "class";
	suppress_date = false;
	justification = true;
	// no color is the default (white)
	backgroundcolor = "none";
	// no color is the default (black)
	fontcolor = "none";
	// light gray is the default font color for greyed-out notes
	notefontcolor = "lightgray";
	registerLyXColor("notefontcolor", notefontcolor);
	boxbgcolor = "red";
	registerLyXColor("boxbgcolor", boxbgcolor);
	table_border_color = "default";
	table_odd_row_color = "default";
	table_even_row_color = "default";
	table_alt_row_colors_start = 1;
	compressed = lyxrc.save_compressed;
	for (int iter = 0; iter < 4; ++iter) {
		user_defined_bullet(iter) = ITEMIZE_DEFAULTS[iter];
		temp_bullet(iter) = ITEMIZE_DEFAULTS[iter];
	}
	// default index
	indiceslist().addDefault(B_("Index"));
	html_be_strict = false;
	html_math_output = MathMLCore;
	html_math_img_scale = 1.0;
	html_css_as_file = false;
	docbook_table_output = HTMLTable;
	docbook_mathml_prefix = MPrefix;
	docbook_mathml_version = MathMLVersion::mathml3;
	display_pixel_ratio = 1.0;

	shell_escape = false;
	output_sync = false;
	xref_package = "refstyle";
	use_formatted_ref = false;
	use_minted = false;
	use_lineno = false;

	// map current author
	pimpl_->authormap[pimpl_->authorlist.get(0).bufferId()] = 0;
}


docstring BufferParams::B_(string const & l10n) const
{
	LASSERT(language, return from_utf8(l10n));
	return getMessages(language->code()).get(l10n);
}


BufferParams::Package BufferParams::use_package(std::string const & p) const
{
	PackageMap::const_iterator it = use_packages.find(p);
	if (it == use_packages.end())
		return package_auto;
	return it->second;
}


void BufferParams::use_package(std::string const & p, BufferParams::Package u)
{
	use_packages[p] = u;
}


map<string, string> const & BufferParams::auto_packages()
{
	static map<string, string> packages;
	if (packages.empty()) {
		// We could have a race condition here that two threads
		// discover an empty map at the same time and want to fill
		// it, but that is no problem, since the same contents is
		// filled in twice then. Having the locker inside the
		// packages.empty() condition has the advantage that we
		// don't need the mutex overhead for simple reading.
		static Mutex mutex;
		Mutex::Locker locker(&mutex);
		// adding a package here implies a file format change!
		packages["amsmath"] =
			N_("The LaTeX package amsmath is only used if AMS formula types or symbols from the AMS math toolbars are inserted into formulas");
		packages["amssymb"] =
			N_("The LaTeX package amssymb is only used if symbols from the AMS math toolbars are inserted into formulas");
		packages["cancel"] =
			N_("The LaTeX package cancel is only used if \\cancel commands are used in formulas");
		packages["esint"] =
			N_("The LaTeX package esint is only used if special integral symbols are inserted into formulas");
		packages["mathdots"] =
			N_("The LaTeX package mathdots is only used if the command \\iddots is inserted into formulas");
		packages["mathtools"] =
			N_("The LaTeX package mathtools is only used if some mathematical relations are inserted into formulas");
		packages["mhchem"] =
			N_("The LaTeX package mhchem is only used if either the command \\ce or \\cf is inserted into formulas");
		packages["stackrel"] =
			N_("The LaTeX package stackrel is only used if the command \\stackrel with subscript is inserted into formulas");
		packages["stmaryrd"] =
			N_("The LaTeX package stmaryrd is only used if symbols from the St Mary's Road symbol font for theoretical computer science are inserted into formulas");
		packages["undertilde"] =
			N_("The LaTeX package undertilde is only used if you use the math frame decoration 'utilde'");
	}
	return packages;
}


bool BufferParams::useBibtopic() const
{
	if (useBiblatex())
		return false;
	return (use_bibtopic || (!multibib.empty() && multibib != "child"));
}


AuthorList & BufferParams::authors()
{
	return pimpl_->authorlist;
}


AuthorList const & BufferParams::authors() const
{
	return pimpl_->authorlist;
}


BufferParams::AuthorMap & BufferParams::authormap()
{
	return pimpl_->authormap;
}


BufferParams::AuthorMap const & BufferParams::authormap() const
{
	return pimpl_->authormap;
}


void BufferParams::addAuthor(Author const & a)
{
	pimpl_->authormap[a.bufferId()] = pimpl_->authorlist.record(a);
}


BranchList & BufferParams::branchlist()
{
	return pimpl_->branchlist;
}


BranchList const & BufferParams::branchlist() const
{
	return pimpl_->branchlist;
}


IndicesList & BufferParams::indiceslist()
{
	return pimpl_->indiceslist;
}


IndicesList const & BufferParams::indiceslist() const
{
	return pimpl_->indiceslist;
}


WordLangTable & BufferParams::spellignore()
{
	return pimpl_->spellignore;
}


WordLangTable const & BufferParams::spellignore() const
{
	return pimpl_->spellignore;
}


bool BufferParams::spellignored(WordLangTuple const & wl) const
{
	bool has_item = false;
	vector<WordLangTuple> il = spellignore();
	vector<WordLangTuple>::const_iterator it = il.begin();
	for (; it != il.end(); ++it) {
		if (it->lang()->code() != wl.lang()->code())
			continue;
		if (it->word() == wl.word()) {
			has_item = true;
			break;
		}
	}
	return has_item;
}


Bullet & BufferParams::temp_bullet(lyx::size_type const index)
{
	LASSERT(index < 4, return pimpl_->temp_bullets[0]);
	return pimpl_->temp_bullets[index];
}


Bullet const & BufferParams::temp_bullet(lyx::size_type const index) const
{
	LASSERT(index < 4, return pimpl_->temp_bullets[0]);
	return pimpl_->temp_bullets[index];
}


Bullet & BufferParams::user_defined_bullet(lyx::size_type const index)
{
	LASSERT(index < 4, return pimpl_->temp_bullets[0]);
	return pimpl_->user_defined_bullets[index];
}


Bullet const & BufferParams::user_defined_bullet(lyx::size_type const index) const
{
	LASSERT(index < 4, return pimpl_->temp_bullets[0]);
	return pimpl_->user_defined_bullets[index];
}


Spacing & BufferParams::spacing()
{
	return pimpl_->spacing;
}


Spacing const & BufferParams::spacing() const
{
	return pimpl_->spacing;
}


PDFOptions & BufferParams::pdfoptions()
{
	return pimpl_->pdfoptions;
}


PDFOptions const & BufferParams::pdfoptions() const
{
	return pimpl_->pdfoptions;
}


Length const & BufferParams::getMathIndent() const
{
	return pimpl_->mathindent;
}


void BufferParams::setMathIndent(Length const & indent)
{
	pimpl_->mathindent = indent;
}


Length const & BufferParams::getParIndent() const
{
	return pimpl_->parindent;
}


void BufferParams::setParIndent(Length const & indent)
{
	pimpl_->parindent = indent;
}


VSpace const & BufferParams::getDefSkip() const
{
	return pimpl_->defskip;
}


void BufferParams::setDefSkip(VSpace const & vs)
{
	// DEFSKIP will cause an infinite loop
	LASSERT(vs.kind() != VSpace::DEFSKIP, return);
	pimpl_->defskip = vs;
}


BufferParams::MathNumber BufferParams::getMathNumber() const
{
	if (math_numbering_side != DEFAULT)
		return math_numbering_side;
	// FIXME: do not hardcode language here
	else if (language->lang() == "arabic_arabi"
	         || documentClass().provides("leqno"))
		return LEFT;
	else
		return RIGHT;
}

void BufferParams::registerLyXColor(string const & col, string const & value)
{
	if (!lcolor.isKnownLyXName(value)) {
		if (theLaTeXColors().isLaTeXColor(value)) {
			LaTeXColor const lc = theLaTeXColors().getLaTeXColor(value);
			string const lyxname = lc.name();
			lcolor.setColor(lyxname, lc.hexname());
			lcolor.setLaTeXName(lyxname, lc.latex());
			lcolor.setGUIName(lyxname, to_ascii(lc.guiname()));
		} else if (custom_colors.find(value) != custom_colors.end()) {
			lcolor.setColor(value, custom_colors[value]);
			lcolor.setLaTeXName(value, value);
		}
	}
	if (!lcolor.isKnownLyXName(col))
		lcolor.setColor(col, lcolor.getX11HexName(value));
}


string BufferParams::readToken(Lexer & lex, string const & token,
	FileName const & filename)
{
	string result;
	FileName const & filepath = filename.onlyPath();

	while (true) {
		if (token == "\\textclass") {
			lex.next();
			string const classname = lex.getString();
			// if there exists a local layout file, ignore the system one
			// NOTE: in this case, the textclass (.cls file) is assumed to
			// be available.
			string tcp;
			LayoutFileList & bcl = LayoutFileList::get();
			if (!filepath.empty()) {
				// If classname is an absolute path, the document is
				// using a local layout file which could not be accessed
				// by a relative path. In this case the path is correct
				// even if the document was moved to a different
				// location. However, we will have a problem if the
				// document was generated on a different platform.
				bool isabsolute = FileName::isAbsolute(classname);
				string const classpath = onlyPath(classname);
				string const path = isabsolute ? classpath
					: FileName(addPath(filepath.absFileName(),
							classpath)).realPath();
				string const oldpath = isabsolute ? string()
					: FileName(addPath(origin, classpath)).realPath();
				tcp = bcl.addLocalLayout(onlyFileName(classname), path, oldpath);
			}
			// that returns non-empty if a "local" layout file is found.
			if (!tcp.empty()) {
				result = to_utf8(makeRelPath(from_utf8(onlyPath(tcp)),
							from_utf8(filepath.absFileName())));
				if (result.empty())
					result = ".";
				setBaseClass(onlyFileName(tcp));
			} else
				setBaseClass(onlyFileName(classname));
			// We assume that a tex class exists for local or unknown
			// layouts so this warning, will only be given for system layouts.
			if (!baseClass()->isTeXClassAvailable()) {
				docstring const desc =
					translateIfPossible(from_utf8(baseClass()->description()));
				docstring const prereqs =
					from_utf8(baseClass()->prerequisites());
				docstring const msg =
					bformat(_("The selected document class\n"
							 "\t%1$s\n"
							 "requires external files that are not available.\n"
							 "The document class can still be used, but the\n"
							 "document cannot be compiled until the following\n"
							 "prerequisites are installed:\n"
							 "\t%2$s\n"
							 "See section 3.1.2.2 (Class Availability) of the\n"
							 "User's Guide for more information."), desc, prereqs);
				frontend::Alert::warning(_("Document class not available"),
					       msg, true);
			}
			break;
		}
		if (token == "\\save_transient_properties") {
			lex >> save_transient_properties;
			break;
		}
		if (token == "\\origin") {
			lex.eatLine();
			origin = lex.getString();
			string const sysdirprefix = "/systemlyxdir/";
			if (prefixIs(origin, sysdirprefix)) {
				string docsys;
				if (inSystemDir(filepath, docsys))
					origin.replace(0, sysdirprefix.length() - 1, docsys);
				else
					origin.replace(0, sysdirprefix.length() - 1,
						package().system_support().absFileName());
			}
			break;
		}
		if (token == "\\begin_metadata") {
			readDocumentMetadata(lex);
			break;
		}
		if (token == "\\begin_preamble") {
			readPreamble(lex);
			break;
		}
		if (token == "\\begin_local_layout") {
			readLocalLayout(lex, false);
			break;
		}
		if (token == "\\begin_forced_local_layout") {
			readLocalLayout(lex, true);
			break;
		}
		if (token == "\\begin_modules") {
			readModules(lex);
			break;
		}
		if (token == "\\begin_removed_modules") {
			readRemovedModules(lex);
			break;
		}
		if (token == "\\begin_includeonly") {
			readIncludeonly(lex);
			break;
		}
		if (token == "\\maintain_unincluded_children") {
			string tmp;
			lex >> tmp;
			if (tmp == "no")
				maintain_unincluded_children = CM_None;
			else if (tmp == "mostly")
				maintain_unincluded_children = CM_Mostly;
			else if (tmp == "strict")
				maintain_unincluded_children = CM_Strict;
			break;
		}
		if (token == "\\options") {
			lex.eatLine();
			options = lex.getString();
			break;
		}
		if (token == "\\use_default_options") {
			lex >> use_default_options;
			break;
		}
		if (token == "\\master") {
			lex.eatLine();
			master = lex.getString();
			if (!filepath.empty() && FileName::isAbsolute(origin)) {
				bool const isabs = FileName::isAbsolute(master);
				FileName const abspath(isabs ? master : origin + master);
				bool const moved = filepath != FileName(origin);
				if (moved && abspath.exists()) {
					docstring const path = isabs
						? from_utf8(master)
						: from_utf8(abspath.realPath());
					docstring const refpath =
						from_utf8(filepath.absFileName());
					master = to_utf8(makeRelPath(path, refpath));
				}
			}
			break;
		}
		if (token == "\\suppress_date") {
			lex >> suppress_date;
			break;
		}
		if (token == "\\justification") {
			lex >> justification;
			break;
		}
		if (token == "\\language") {
			readLanguage(lex);
			break;
		}
		if (token == "\\language_package") {
			lex.eatLine();
			lang_package = lex.getString();
			break;
		}
		if (token == "\\language_options_babel") {
			string lang;
			lex >> lang;
			lex.eatLine();
			string const opts = lex.getString();
			lang_options_babel_[lang] = trim(opts, "\"");
			break;
		}
		if (token == "\\language_options_polyglossia") {
			string lang;
			lex >> lang;
			lex.eatLine();
			string const opts = lex.getString();
			lang_options_polyglossia_[lang] = trim(opts, "\"");
			break;
		}
		if (token == "\\inputencoding") {
			lex >> inputenc;
			break;
		}
		if (token == "\\graphics") {
			readGraphicsDriver(lex);
			break;
		}
		if (token == "\\default_output_format") {
			lex >> default_output_format;
			break;
		}
		if (token == "\\bibtex_command") {
			lex.eatLine();
			bibtex_command = lex.getString();
			break;
		}
		if (token == "\\index_command") {
			lex.eatLine();
			index_command = lex.getString();
			break;
		}
		if (token == "\\fontencoding") {
			lex.eatLine();
			fontenc = lex.getString();
			break;
		}
		if (token == "\\font_roman") {
			lex >> fonts_roman[0];
			lex >> fonts_roman[1];
			break;
		}
		if (token == "\\font_sans") {
			lex >> fonts_sans[0];
			lex >> fonts_sans[1];
			break;
		}
		if (token == "\\font_typewriter") {
			lex >> fonts_typewriter[0];
			lex >> fonts_typewriter[1];
			break;
		}
		if (token == "\\font_math") {
			lex >> fonts_math[0];
			lex >> fonts_math[1];
			break;
		}
		if (token == "\\font_default_family") {
			lex >> fonts_default_family;
			break;
		}
		if (token == "\\use_non_tex_fonts") {
			lex >> useNonTeXFonts;
			break;
		}
		if (token == "\\font_sc") {
			lex >> fonts_expert_sc;
			break;
		}
		if (token == "\\font_roman_osf") {
			lex >> fonts_roman_osf;
			break;
		}
		if (token == "\\font_sans_osf") {
			lex >> fonts_sans_osf;
			break;
		}
		if (token == "\\font_typewriter_osf") {
			lex >> fonts_typewriter_osf;
			break;
		}
		if (token == "\\font_roman_opts") {
			lex >> font_roman_opts;
			break;
		}
		if (token == "\\font_sf_scale") {
			lex >> fonts_sans_scale[0];
			lex >> fonts_sans_scale[1];
			break;
		}
		if (token == "\\font_sans_opts") {
			lex >> font_sans_opts;
			break;
		}
		if (token == "\\font_tt_scale") {
			lex >> fonts_typewriter_scale[0];
			lex >> fonts_typewriter_scale[1];
			break;
		}
		if (token == "\\font_typewriter_opts") {
			lex >> font_typewriter_opts;
			break;
		}
		if (token == "\\font_cjk") {
			lex >> fonts_cjk;
			break;
		}
		if (token == "\\use_microtype") {
			lex >> use_microtype;
			break;
		}
		if (token == "\\use_dash_ligatures") {
			lex >> use_dash_ligatures;
			break;
		}
		if (token == "\\paragraph_separation") {
			string parsep;
			lex >> parsep;
			paragraph_separation = parseptranslator().find(parsep);
			break;
		}
		if (token == "\\paragraph_indentation") {
			lex.next();
			string parindent = lex.getString();
			if (parindent == "default")
				pimpl_->parindent = Length();
			else
				pimpl_->parindent = Length(parindent);
			break;
		}
		if (token == "\\defskip") {
			lex.next();
			string const defskip = lex.getString();
			pimpl_->defskip = VSpace(defskip);
			if (pimpl_->defskip.kind() == VSpace::DEFSKIP)
				// that is invalid
				pimpl_->defskip = VSpace(VSpace::MEDSKIP);
			break;
		}
		if (token == "\\is_math_indent") {
			lex >> is_math_indent;
			break;
		}
		if (token == "\\math_indentation") {
			lex.next();
			string mathindent = lex.getString();
			if (mathindent == "default")
				pimpl_->mathindent = Length();
			else
				pimpl_->mathindent = Length(mathindent);
			break;
		}
		if (token == "\\math_numbering_side") {
			string tmp;
			lex >> tmp;
			if (tmp == "left")
				math_numbering_side = LEFT;
			else if (tmp == "right")
				math_numbering_side = RIGHT;
			else
				math_numbering_side = DEFAULT;
			break;
		}
		if (token == "\\quotes_style") {
			string qstyle;
			lex >> qstyle;
			quotes_style = quotesstyletranslator().find(qstyle);
			break;
		}
		if (token == "\\dynamic_quotes") {
			lex >> dynamic_quotes;
			break;
		}
		if (token == "\\papersize") {
			string ppsize;
			lex >> ppsize;
			papersize = papersizetranslator().find(ppsize);
			break;
		}
		if (token == "\\use_geometry") {
			lex >> use_geometry;
			break;
		}
		if (token == "\\use_package") {
			string package;
			int use;
			lex >> package;
			lex >> use;
			use_package(package, packagetranslator().find(use));
			break;
		}
		if (token == "\\cite_engine") {
			lex.eatLine();
			cite_engine_ = lex.getString();
			break;
		}
		if (token == "\\cite_engine_type") {
			string engine_type;
			lex >> engine_type;
			cite_engine_type_ = theCiteEnginesList.getType(engine_type);
			break;
		}
		if (token == "\\biblio_style") {
			lex.eatLine();
			biblio_style = lex.getString();
			break;
		}
		if (token == "\\biblio_options") {
			lex.eatLine();
			biblio_opts = trim(lex.getString());
			break;
		}
		if (token == "\\biblatex_bibstyle") {
			lex.eatLine();
			biblatex_bibstyle = trim(lex.getString());
			break;
		}
		if (token == "\\biblatex_citestyle") {
			lex.eatLine();
			biblatex_citestyle = trim(lex.getString());
			break;
		}
		if (token == "\\use_bibtopic") {
			lex >> use_bibtopic;
			break;
		}
		if (token == "\\multibib") {
			lex >> multibib;
			break;
		}
		if (token == "\\use_indices") {
			lex >> use_indices;
			break;
		}
		if (token == "\\tracking_changes") {
			lex >> track_changes;
			break;
		}
		if (token == "\\output_changes") {
			lex >> output_changes;
			break;
		}
		if (token == "\\change_bars") {
			lex >> change_bars;
			break;
		}
		if (token == "\\postpone_fragile_content") {
			lex >> postpone_fragile_content;
			break;
		}
		if (token == "\\branch") {
			lex.eatLine();
			docstring branch = lex.getDocString();
			branchlist().add(branch);
			while (true) {
				lex.next();
				string const tok = lex.getString();
				if (tok == "\\end_branch")
					break;
				Branch * branch_ptr = branchlist().find(branch);
				if (tok == "\\selected") {
					lex.next();
					if (branch_ptr)
						branch_ptr->setSelected(lex.getInteger());
				}
				if (tok == "\\filename_suffix") {
					lex.next();
					if (branch_ptr)
						branch_ptr->setFileNameSuffix(lex.getInteger());
				}
				if (tok == "\\color") {
					lex.eatLine();
					vector<string> const colors = getVectorFromString(lex.getString(), " ");
					string const lmcolor = colors.front();
					string dmcolor;
					if (colors.size() > 1)
						dmcolor = colors.back();
					if (branch_ptr)
						branch_ptr->setColors(lmcolor, dmcolor);
				}
			}
			break;
		}
		if (token == "\\index") {
			lex.eatLine();
			docstring index = lex.getDocString();
			docstring shortcut;
			indiceslist().add(index);
			while (true) {
				lex.next();
				string const tok = lex.getString();
				if (tok == "\\end_index")
					break;
				Index * index_ptr = indiceslist().find(index);
				if (tok == "\\shortcut") {
					lex.next();
					shortcut = lex.getDocString();
					if (index_ptr)
						index_ptr->setShortcut(shortcut);
				}
				if (tok == "\\color") {
					lex.eatLine();
					string color = lex.getString();
					if (index_ptr)
						index_ptr->setColor(color);
					// Update also the Color table:
					if (color == "none")
						color = lcolor.getX11HexName(Color_background);
					// FIXME UNICODE
					if (!shortcut.empty())
						lcolor.setColor(to_utf8(shortcut)+ "@" + filename.absFileName(), color);
				}
			}
			break;
		}
		if (token == "\\spellchecker_ignore") {
			lex.eatLine();
			docstring wl = lex.getDocString();
			docstring language;
			docstring word = split(wl, language, ' ');
			Language const * lang = languages.getLanguage(to_ascii(language));
			if (lang)
				spellignore().push_back(WordLangTuple(word, lang));
			break;
		}
		if (token == "\\author") {
			lex.eatLine();
			istringstream ss(lex.getString());
			Author a;
			ss >> a;
			addAuthor(a);
			break;
		}
		if (token == "\\paperorientation") {
			string orient;
			lex >> orient;
			orientation = paperorientationtranslator().find(orient);
			break;
		}
		if (token == "\\backgroundcolor") {
			lex.eatLine();
			backgroundcolor = lex.getString();
			registerLyXColor("backgroundcolor", backgroundcolor);
			break;
		}
		if (token == "\\fontcolor") {
			lex.eatLine();
			fontcolor = lex.getString();
			registerLyXColor("fontcolor", fontcolor);
			break;
		}
		if (token == "\\notefontcolor") {
			lex.eatLine();
			notefontcolor = lex.getString();
			// set a local name for the painter
			lcolor.setColor("notefontcolor@" + filename.absFileName(),
					lcolor.getX11HexName(notefontcolor));
			break;
		}
		if (token == "\\boxbgcolor") {
			lex.eatLine();
			boxbgcolor = lex.getString();
			// set a local name for the painter
			lcolor.setColor("boxbgcolor@" + filename.absFileName(),
					lcolor.getX11HexName(boxbgcolor));
			break;
		}
		if (token == "\\table_border_color") {
			lex.eatLine();
			table_border_color = lex.getString();
			if (table_border_color != "default")
				registerLyXColor("table_border_color", table_border_color);
			break;
		}
		if (token == "\\table_odd_row_color") {
			lex.eatLine();
			table_odd_row_color = lex.getString();
			if (table_odd_row_color != "default")
				registerLyXColor("table_odd_row_color", table_odd_row_color);
			break;
		}
		if (token == "\\table_even_row_color") {
			lex.eatLine();
			table_even_row_color = lex.getString();
			if (table_even_row_color != "default")
				registerLyXColor("table_even_row_color", table_even_row_color);
			break;
		}
		if (token == "\\table_alt_row_colors_start") {
			lex >> table_alt_row_colors_start;
			break;
		}
		if (token == "\\customcolor") {
			string name;
			lex >> name;;
			string value;
			lex >> value;
			custom_colors[name] = "#" + value;
			lcolor.setColor(name, "#" + value);
			lcolor.setLaTeXName(name, name);
			lcolor.setGUIName(name, name);
			break;
		}
		if (token == "\\paperwidth") {
			lex >> paperwidth;
			break;
		}
		if (token == "\\paperheight") {
			lex >> paperheight;
			break;
		}
		if (token == "\\leftmargin") {
			lex >> leftmargin;
			break;
		}
		if (token == "\\topmargin") {
			lex >> topmargin;
			break;
		}
		if (token == "\\rightmargin") {
			lex >> rightmargin;
			break;
		}
		if (token == "\\bottommargin") {
			lex >> bottommargin;
			break;
		}
		if (token == "\\headheight") {
			lex >> headheight;
			break;
		}
		if (token == "\\headsep") {
			lex >> headsep;
			break;
		}
		if (token == "\\footskip") {
			lex >> footskip;
			break;
		}
		if (token == "\\columnsep") {
			lex >> columnsep;
			break;
		}
		if (token == "\\paperfontsize") {
			lex >> fontsize;
			break;
		}
		if (token == "\\papercolumns") {
			lex >> columns;
			break;
		}
		if (token == "\\listings_params") {
			string par;
			lex >> par;
			listings_params = InsetListingsParams(par).params();
			break;
		}
		if (token == "\\papersides") {
			int psides;
			lex >> psides;
			sides = sidestranslator().find(psides);
			break;
		}
		if (token == "\\paperpagestyle") {
			lex >> pagestyle;
			break;
		}
		if (token == "\\tablestyle") {
			lex >> tablestyle;
			break;
		}
		if (token == "\\bullet") {
			readBullets(lex);
			break;
		}
		if (token == "\\bulletLaTeX") {
			readBulletsLaTeX(lex);
			break;
		}
		if (token == "\\secnumdepth") {
			lex >> secnumdepth;
			break;
		}
		if (token == "\\tocdepth") {
			lex >> tocdepth;
			break;
		}
		if (token == "\\spacing") {
			string nspacing;
			lex >> nspacing;
			string tmp_val;
			if (nspacing == "other") {
				lex >> tmp_val;
			}
			spacing().set(spacetranslator().find(nspacing), tmp_val);
			break;
		}
		if (token == "\\float_placement") {
			lex >> float_placement;
			break;
		}
		if (token == "\\float_alignment") {
			lex >> float_alignment;
			break;
	
		}
		if (prefixIs(token, "\\pdf_") || token == "\\use_hyperref") {
			string toktmp = pdfoptions().readToken(lex, token);
			if (!toktmp.empty()) {
				lyxerr << "PDFOptions::readToken(): Unknown token: " <<
					toktmp << endl;
				return toktmp;
			}
			break;
		}
		if (token == "\\html_math_output") {
			int temp;
			lex >> temp;
			html_math_output = static_cast<MathOutput>(temp);
			break;
		}
		if (token == "\\html_be_strict") {
			lex >> html_be_strict;
			break;
		}
		if (token == "\\html_css_as_file") {
			lex >> html_css_as_file;
			break;
		}
		if (token == "\\html_math_img_scale") {
			lex >> html_math_img_scale;
			break;
		}
		if (token == "\\html_latex_start") {
			lex.eatLine();
			html_latex_start = lex.getString();
			break;
		}
		if (token == "\\html_latex_end") {
			lex.eatLine();
			html_latex_end = lex.getString();
			break;
		}
		if (token == "\\docbook_table_output") {
			int temp;
			lex >> temp;
			docbook_table_output = static_cast<TableOutput>(temp);
			break;
		}
		if (token == "\\docbook_mathml_prefix") {
			int temp;
			lex >> temp;
			docbook_mathml_prefix = static_cast<MathMLNameSpacePrefix>(temp);
			break;
		}
		if (token == "\\docbook_mathml_version") {
			int temp;
			lex >> temp;
			docbook_mathml_version = static_cast<MathMLVersion>(temp);
			break;
		}
		if (token == "\\output_sync") {
			lex >> output_sync;
			break;
		}
		if (token == "\\output_sync_macro") {
			lex >> output_sync_macro;
			break;
		}
		if (token == "\\crossref_package") {
			lex >> xref_package;
			break;
		}
		if (token == "\\use_formatted_ref") {
			lex >> use_formatted_ref;
			break;
		}
		if (token == "\\use_minted") {
			lex >> use_minted;
			break;
		}
		if (token == "\\nomencl_options") {
			lex.eatLine();
			nomencl_opts = trim(lex.getString());
			break;
		}
		if (token == "\\use_lineno") {
			lex >> use_lineno;
			break;
		}
		if (token == "\\lineno_options") {
			lex.eatLine();
			lineno_opts = trim(lex.getString());
			break;
		}
		lyxerr << "BufferParams::readToken(): Unknown token: " <<
			token << endl;
		return token;
	}
	return result;
}


namespace {
	// Quote argument if it contains spaces
	string quoteIfNeeded(string const & str) {
		if (contains(str, ' '))
			return "\"" + str + "\"";
		return str;
	}
} // namespace


void BufferParams::writeFile(ostream & os, Buffer const * buf) const
{
	// The top of the file is written by the buffer.
	// Prints out the buffer info into the .lyx file given by file

	os << "\\save_transient_properties "
	   << convert<string>(save_transient_properties) << '\n';

	// the document directory (must end with a path separator)
	// realPath() is used to resolve symlinks, while addPath(..., "")
	// ensures a trailing path separator.
	string docsys;
	string filepath = addPath(buf->fileName().onlyPath().realPath(), "");
	string const sysdir = inSystemDir(FileName(filepath), docsys) ? docsys
			: addPath(package().system_support().realPath(), "");
	string const relpath =
		to_utf8(makeRelPath(from_utf8(filepath), from_utf8(sysdir)));
	if (!prefixIs(relpath, "../") && !FileName::isAbsolute(relpath))
		filepath = addPath("/systemlyxdir", relpath);
	else if (!save_transient_properties || !lyxrc.save_origin)
		filepath = "unavailable";
	os << "\\origin " << quoteIfNeeded(filepath) << '\n';

	// the textclass
	os << "\\textclass "
	   << quoteIfNeeded(buf->includedFilePath(addName(buf->layoutPos(),
						baseClass()->name()), "layout"))
	   << '\n';

	// then document metadata
	if (!document_metadata.empty()) {
		// remove '\n' from the end of document_metadata
		docstring const tmpmd = rtrim(document_metadata, "\n");
		os << "\\begin_metadata\n"
		   << to_utf8(tmpmd)
		   << "\n\\end_metadata\n";
	}

	// then the preamble
	if (!preamble.empty()) {
		// remove '\n' from the end of preamble
		docstring const tmppreamble = rtrim(preamble, "\n");
		os << "\\begin_preamble\n"
		   << to_utf8(tmppreamble)
		   << "\n\\end_preamble\n";
	}

	// the options
	if (!options.empty()) {
		os << "\\options " << options << '\n';
	}

	// use the class options defined in the layout?
	os << "\\use_default_options "
	   << convert<string>(use_default_options) << "\n";

	// the master document
	if (!master.empty()) {
		os << "\\master " << master << '\n';
	}

	// removed modules
	if (!removed_modules_.empty()) {
		os << "\\begin_removed_modules" << '\n';
		for (auto const & mod : removed_modules_)
			os << mod << '\n';
		os << "\\end_removed_modules" << '\n';
	}

	// the modules
	if (!layout_modules_.empty()) {
		os << "\\begin_modules" << '\n';
		for (auto const & mod : layout_modules_)
			os << mod << '\n';
		os << "\\end_modules" << '\n';
	}

	// includeonly
	if (!included_children_.empty()) {
		os << "\\begin_includeonly" << '\n';
		for (auto const & c : included_children_)
			os << c << '\n';
		os << "\\end_includeonly" << '\n';
	}
	string muc = "no";
	switch (maintain_unincluded_children) {
	case CM_Mostly:
		muc = "mostly";
		break;
	case CM_Strict:
		muc = "strict";
		break;
	case CM_None:
	default:
		break;
	}
	os << "\\maintain_unincluded_children " << muc << '\n';

	// local layout information
	docstring const local_layout = getLocalLayout(false);
	if (!local_layout.empty()) {
		// remove '\n' from the end
		docstring const tmplocal = rtrim(local_layout, "\n");
		os << "\\begin_local_layout\n"
		   << to_utf8(tmplocal)
		   << "\n\\end_local_layout\n";
	}
	docstring const forced_local_layout = getLocalLayout(true);
	if (!forced_local_layout.empty()) {
		// remove '\n' from the end
		docstring const tmplocal = rtrim(forced_local_layout, "\n");
		os << "\\begin_forced_local_layout\n"
		   << to_utf8(tmplocal)
		   << "\n\\end_forced_local_layout\n";
	}

	// then the text parameters
	if (language != ignore_language)
		os << "\\language " << language->lang() << '\n';
	for (auto const & s : lang_options_babel_) {
		Language const * l = languages.getLanguage(s.first);
		if (l && l->babelOpts() != s.second)
			// babel options can be empty
			os << "\\language_options_babel " << s.first << " \"" << s.second << "\"\n";
	}
	for (auto const & s : lang_options_polyglossia_) {
		Language const * l = languages.getLanguage(s.first);
		if (l && l->polyglossiaOpts() != s.second)
			// polyglossia options can be empty
			os << "\\language_options_polyglossia " << s.first << " \"" << s.second << "\"\n";
	}
	os << "\\language_package " << lang_package
	   << "\n\\inputencoding " << inputenc
	   << "\n\\fontencoding " << fontenc
	   << "\n\\font_roman \"" << fonts_roman[0]
	   << "\" \"" << fonts_roman[1] << '"'
	   << "\n\\font_sans \"" << fonts_sans[0]
	   << "\" \"" << fonts_sans[1] << '"'
	   << "\n\\font_typewriter \"" << fonts_typewriter[0]
	   << "\" \"" << fonts_typewriter[1] << '"'
	   << "\n\\font_math \"" << fonts_math[0]
	   << "\" \"" << fonts_math[1] << '"'
	   << "\n\\font_default_family " << fonts_default_family
	   << "\n\\use_non_tex_fonts " << convert<string>(useNonTeXFonts)
	   << "\n\\font_sc " << convert<string>(fonts_expert_sc)
	   << "\n\\font_roman_osf " << convert<string>(fonts_roman_osf)
	   << "\n\\font_sans_osf " << convert<string>(fonts_sans_osf)
	   << "\n\\font_typewriter_osf " << convert<string>(fonts_typewriter_osf);
	if (!font_roman_opts.empty())
		os << "\n\\font_roman_opts \"" << font_roman_opts << "\"";
	os << "\n\\font_sf_scale " << fonts_sans_scale[0]
	   << ' ' << fonts_sans_scale[1];
	if (!font_sans_opts.empty())
		os << "\n\\font_sans_opts \"" << font_sans_opts << "\"";
	os << "\n\\font_tt_scale " << fonts_typewriter_scale[0]
	   << ' ' << fonts_typewriter_scale[1];
	if (!font_typewriter_opts.empty())
		os << "\n\\font_typewriter_opts \"" << font_typewriter_opts << "\"";
	os << '\n';
	if (!fonts_cjk.empty())
		os << "\\font_cjk " << fonts_cjk << '\n';
	os << "\\use_microtype " << convert<string>(use_microtype) << '\n';
	os << "\\use_dash_ligatures " << convert<string>(use_dash_ligatures) << '\n';
	os << "\\graphics " << graphics_driver << '\n';
	os << "\\default_output_format " << default_output_format << '\n';
	os << "\\output_sync " << output_sync << '\n';
	if (!output_sync_macro.empty())
		os << "\\output_sync_macro \"" << output_sync_macro << "\"\n";
	os << "\\bibtex_command " << bibtex_command << '\n';
	os << "\\index_command " << index_command << '\n';

	if (!float_placement.empty())
		os << "\\float_placement " << float_placement << '\n';
	if (!float_alignment.empty())
		os << "\\float_alignment " << float_alignment << '\n';
	os << "\\paperfontsize " << fontsize << '\n';

	spacing().writeFile(os);
	pdfoptions().writeFile(os);

	os << "\\papersize " << string_papersize[papersize]
	   << "\n\\use_geometry " << convert<string>(use_geometry);
	map<string, string> const & packages = auto_packages();
	for (auto const & pack : packages)
		os << "\n\\use_package " << pack.first << ' '
		   << use_package(pack.first);

	os << "\n\\cite_engine ";

	if (!cite_engine_.empty())
		os << cite_engine_;
	else
		os << "basic";

	os << "\n\\cite_engine_type " << theCiteEnginesList.getTypeAsString(cite_engine_type_);

	if (!biblio_style.empty())
		os << "\n\\biblio_style " << biblio_style;
	if (!biblio_opts.empty())
		os << "\n\\biblio_options " << biblio_opts;
	if (!biblatex_bibstyle.empty())
		os << "\n\\biblatex_bibstyle " << biblatex_bibstyle;
	if (!biblatex_citestyle.empty())
		os << "\n\\biblatex_citestyle " << biblatex_citestyle;
	if (!multibib.empty())
		os << "\n\\multibib " << multibib;

	os << "\n\\use_bibtopic " << convert<string>(use_bibtopic)
	   << "\n\\use_indices " << convert<string>(use_indices)
	   << "\n\\paperorientation " << string_orientation[orientation]
	   << "\n\\suppress_date " << convert<string>(suppress_date)
	   << "\n\\justification " << convert<string>(justification)
	   << "\n\\crossref_package " << xref_package
	   << "\n\\use_formatted_ref " << use_formatted_ref
	   << "\n\\use_minted " << use_minted
	   << "\n\\use_lineno " << use_lineno
	   << '\n';

	if (!lineno_opts.empty())
		os << "\\lineno_options " << lineno_opts << '\n';

	if (!nomencl_opts.empty())
		os << "\\nomencl_options " << nomencl_opts << '\n';

	for (auto const & cc : custom_colors) {
		os << "\\customcolor "
		   << cc.first
		   << " " << ltrim(cc.second, "#")
		   << "\n";
	}
	os << "\\backgroundcolor " << backgroundcolor << '\n';
	os << "\\fontcolor " << fontcolor << '\n';
	os << "\\notefontcolor " << notefontcolor << '\n';
	os << "\\boxbgcolor " << boxbgcolor << '\n';
	os << "\\table_border_color " << table_border_color << '\n';
	os << "\\table_odd_row_color " << table_odd_row_color << '\n';
	os << "\\table_even_row_color " << table_even_row_color << '\n';
	os << "\\table_alt_row_colors_start " << table_alt_row_colors_start << '\n';

	for (auto const & br : branchlist()) {
		os << "\\branch " << to_utf8(br.branch())
		   << "\n\\selected " << br.isSelected()
		   << "\n\\filename_suffix " << br.hasFileNameSuffix()
		   << "\n\\color " << br.lightModeColor() << " " << br.darkModeColor()
		   << "\n\\end_branch"
		   << "\n";
	}

	for (auto const & id : indiceslist()) {
		os << "\\index " << to_utf8(id.index())
		   << "\n\\shortcut " << to_utf8(id.shortcut())
		   << "\n\\color " << lyx::X11hexname(id.color())
		   << "\n\\end_index"
		   << "\n";
	}

	for (auto const & si : spellignore()) {
		os << "\\spellchecker_ignore " << si.lang()->lang()
		   << " " << to_utf8(si.word())
		   << "\n";
	}

	if (!paperwidth.empty())
		os << "\\paperwidth "
		   << VSpace(paperwidth).asLyXCommand() << '\n';
	if (!paperheight.empty())
		os << "\\paperheight "
		   << VSpace(paperheight).asLyXCommand() << '\n';
	if (!leftmargin.empty())
		os << "\\leftmargin "
		   << VSpace(leftmargin).asLyXCommand() << '\n';
	if (!topmargin.empty())
		os << "\\topmargin "
		   << VSpace(topmargin).asLyXCommand() << '\n';
	if (!rightmargin.empty())
		os << "\\rightmargin "
		   << VSpace(rightmargin).asLyXCommand() << '\n';
	if (!bottommargin.empty())
		os << "\\bottommargin "
		   << VSpace(bottommargin).asLyXCommand() << '\n';
	if (!headheight.empty())
		os << "\\headheight "
		   << VSpace(headheight).asLyXCommand() << '\n';
	if (!headsep.empty())
		os << "\\headsep "
		   << VSpace(headsep).asLyXCommand() << '\n';
	if (!footskip.empty())
		os << "\\footskip "
		   << VSpace(footskip).asLyXCommand() << '\n';
	if (!columnsep.empty())
		os << "\\columnsep "
			 << VSpace(columnsep).asLyXCommand() << '\n';
	os << "\\secnumdepth " << secnumdepth
	   << "\n\\tocdepth " << tocdepth
	   << "\n\\paragraph_separation "
	   << string_paragraph_separation[paragraph_separation];
	if (!paragraph_separation)
		os << "\n\\paragraph_indentation "
		   << (getParIndent().empty() ? "default" : getParIndent().asString());
	else
		os << "\n\\defskip " << getDefSkip().asLyXCommand();
	os << "\n\\is_math_indent " << is_math_indent;
	if (is_math_indent)
		os << "\n\\math_indentation "
		   << (getMathIndent().empty() ? "default" : getMathIndent().asString());
	os << "\n\\math_numbering_side ";
	switch(math_numbering_side) {
	case LEFT:
		os << "left";
		break;
	case RIGHT:
		os << "right";
		break;
	case DEFAULT:
		os << "default";
	}
	os << "\n\\quotes_style "
	   << string_quotes_style[static_cast<int>(quotes_style)]
	   << "\n\\dynamic_quotes " << dynamic_quotes
	   << "\n\\papercolumns " << columns
	   << "\n\\papersides " << sides
	   << "\n\\paperpagestyle " << pagestyle
	   << "\n\\tablestyle " << tablestyle << '\n';
	if (!listings_params.empty())
		os << "\\listings_params \"" <<
			InsetListingsParams(listings_params).encodedString() << "\"\n";
	for (int i = 0; i < 4; ++i) {
		if (user_defined_bullet(i) != ITEMIZE_DEFAULTS[i]) {
			if (user_defined_bullet(i).getFont() != -1) {
				os << "\\bullet " << i << " "
				   << user_defined_bullet(i).getFont() << " "
				   << user_defined_bullet(i).getCharacter() << " "
				   << user_defined_bullet(i).getSize() << "\n";
			}
			else {
				// FIXME UNICODE
				os << "\\bulletLaTeX " << i << " \""
				   << lyx::to_ascii(user_defined_bullet(i).getText())
				   << "\"\n";
			}
		}
	}

	os << "\\tracking_changes "
	   << (save_transient_properties ? convert<string>(track_changes) : "false")
	   << '\n';

	os << "\\output_changes "
	   << (save_transient_properties ? convert<string>(output_changes) : "false")
	   << '\n';

	os << "\\change_bars "
	   << (save_transient_properties ? convert<string>(change_bars) : "false")
	   << '\n';

	os << "\\postpone_fragile_content " << convert<string>(postpone_fragile_content) << '\n';

	os << "\\html_math_output " << html_math_output << '\n'
	   << "\\html_css_as_file " << html_css_as_file << '\n'
	   << "\\html_be_strict " << convert<string>(html_be_strict) << '\n';

	os << "\\docbook_table_output " << docbook_table_output << '\n';
	os << "\\docbook_mathml_prefix " << docbook_mathml_prefix << '\n';
	os << "\\docbook_mathml_version " << static_cast<int>(docbook_mathml_version) << '\n';

	if (html_math_img_scale != 1.0)
		os << "\\html_math_img_scale " << convert<string>(html_math_img_scale) << '\n';
	if (!html_latex_start.empty())
		os << "\\html_latex_start " << html_latex_start << '\n';
	if (!html_latex_end.empty())
		 os << "\\html_latex_end " << html_latex_end << '\n';

	os << pimpl_->authorlist;
}


void BufferParams::validate(LaTeXFeatures & features) const
{
	features.require(documentClass().required());

	if (columns > 1 && language->rightToLeft()
	    && !features.runparams().isFullUnicode()
	    && language->babel() != "hebrew")
		features.require("rtloutputdblcol");

	if (output_changes) {
		bool xcolorulem = LaTeXFeatures::isAvailable("ulem") &&
				  LaTeXFeatures::isAvailable("xcolor");

		switch (features.runparams().flavor) {
		case Flavor::LaTeX:
		case Flavor::DviLuaTeX:
			if (xcolorulem) {
				features.require("ct-xcolor-ulem");
				features.require("ulem");
				features.require("xcolor");
			} else {
				features.require("ct-none");
			}
			break;
		case Flavor::LuaTeX:
		case Flavor::PdfLaTeX:
		case Flavor::XeTeX:
			if (xcolorulem) {
				features.require("ct-xcolor-ulem");
				features.require("ulem");
				features.require("xcolor");
				// improves color handling in PDF output
			} else {
				features.require("ct-none");
			}
			break;
		default:
			break;
		}
		if (change_bars)
			features.require("changebar");
	}

	// Floats with 'Here definitely' as default setting.
	if (float_placement.find('H') != string::npos)
		features.require("float");

	for (auto const & pm : use_packages) {
		if (pm.first == "amsmath") {
			// AMS Style is at document level
			if (pm.second == package_on ||
			    features.isProvided("amsmath"))
				features.require(pm.first);
		} else if (pm.second == package_on)
			features.require(pm.first);
	}

	// Document-level line spacing
	if (spacing().getSpace() != Spacing::Single && !spacing().isDefault())
		features.require("setspace");

	// the bullet shapes are buffer level not paragraph level
	// so they are tested here
	for (int i = 0; i < 4; ++i) {
		if (user_defined_bullet(i) == ITEMIZE_DEFAULTS[i])
			continue;
		int const font = user_defined_bullet(i).getFont();
		if (font == 0) {
			int const c = user_defined_bullet(i).getCharacter();
			if (c == 16
			    || c == 17
			    || c == 25
			    || c == 26
			    || c == 31) {
				features.require("latexsym");
			}
		} else if (font == 1) {
			features.require("amssymb");
		} else if (font >= 2 && font <= 5) {
			features.require("pifont");
		}
	}

	if (pdfoptions().use_hyperref) {
		features.require("hyperref");
		// due to interferences with babel and hyperref, the color package has to
		// be loaded after hyperref when hyperref is used with the colorlinks
		// option, see http://www.lyx.org/trac/ticket/5291
		if (pdfoptions().colorlinks)
			features.require("color");
		pdfoptions().validate(features);
	}
	if (!listings_params.empty()) {
		// do not test validity because listings_params is
		// supposed to be valid
		string par =
			InsetListingsParams(listings_params).separatedParams(true);
		// we can't support all packages, but we should load the color package
		if (par.find("\\color", 0) != string::npos)
			features.require("color");
	}

	// some languages are only available via polyglossia
	if (features.hasPolyglossiaExclusiveLanguages())
		features.require("polyglossia");

	if (useNonTeXFonts && fontsMath() != "auto")
		features.require("unicode-math");

	if (use_microtype)
		features.require("microtype");

	if (!language->required().empty())
		features.require(language->required());

	if (!custom_colors.empty())
		features.require("color");
}


bool BufferParams::writeLaTeX(otexstream & os, LaTeXFeatures & features,
			      FileName const & filepath) const
{
	// DocumentMetadata must come before anything else
	if (features.isAvailableAtLeastFrom("LaTeX", 2022, 6)
	    && !containsOnly(document_metadata, " \n\t")) {
		// Check if the user preamble contains uncodable glyphs
		odocstringstream doc_metadata;
		docstring uncodable_glyphs;
		Encoding const * const enc = features.runparams().encoding;
		if (enc) {
			for (char_type c : document_metadata) {
				if (!enc->encodable(c)) {
					docstring const glyph(1, c);
					LYXERR0("Uncodable character '"
						<< glyph
						<< "' in document metadata!");
					uncodable_glyphs += glyph;
					if (features.runparams().dryrun) {
						doc_metadata << "<" << _("LyX Warning: ")
						   << _("uncodable character") << " '";
						doc_metadata.put(c);
						doc_metadata << "'>";
					}
				} else
					doc_metadata.put(c);
			}
		} else
			doc_metadata << document_metadata;

		// On BUFFER_VIEW|UPDATE, warn user if we found uncodable glyphs
		if (!features.runparams().dryrun && !uncodable_glyphs.empty()) {
			frontend::Alert::warning(
				_("Uncodable character in document metadata"),
				support::bformat(
				  _("The metadata of your document contains glyphs "
				    "that are unknown in the current document encoding "
				    "(namely %1$s).\nThese glyphs are omitted "
				    " from the output, which may result in "
				    "incomplete output."
				    "\n\nPlease select an appropriate "
				    "document encoding\n"
				    "(such as utf8) or change the "
				    "metadata accordingly."),
				  uncodable_glyphs));
		}
		if (!doc_metadata.str().empty()) {
			os << "\\DocumentMetadata{\n"
			   << doc_metadata.str()
			   << "}\n";
		}
	}

	// http://www.tug.org/texmf-dist/doc/latex/base/fixltx2e.pdf
	// !! To use the Fix-cm package, load it before \documentclass, and use the command
	// \RequirePackage to do so, rather than the normal \usepackage
	// Do not try to load any other package before the document class, unless you
	// have a thorough understanding of the LATEX internals and know exactly what you
	// are doing!
	if (features.mustProvide("fix-cm"))
		os << "\\RequirePackage{fix-cm}\n";
	// Likewise for fixltx2e. If other packages conflict with this policy,
	// treat it as a package bug (and report it!)
	// See http://www.latex-project.org/cgi-bin/ltxbugs2html?pr=latex/4407
	if (features.mustProvide("fixltx2e"))
		os << "\\RequirePackage{fixltx2e}\n";

	os << "\\documentclass";

	DocumentClass const & tclass = documentClass();

	ostringstream clsoptions; // the document class options.

	if (tokenPos(tclass.opt_fontsize(),
		     '|', fontsize) >= 0) {
		// only write if existing in list (and not default)
		clsoptions << subst(tclass.fontsizeformat(), "$$s", fontsize) << ",";
	}

	// paper sizes not supported by the class itself need the
	// geometry package
	vector<string> classpsizes = getVectorFromString(tclass.opt_pagesize(), "|");
	bool class_supported_papersize = papersize == PAPER_DEFAULT
		|| find(classpsizes.begin(), classpsizes.end(), string_papersize[papersize]) != classpsizes.end();

	if ((!use_geometry || features.isProvided("geometry-light"))
	    && class_supported_papersize && papersize != PAPER_DEFAULT)
		clsoptions << subst(tclass.pagesizeformat(), "$$s", string_papersize[papersize]) << ",";

	// if needed
	if (sides != tclass.sides()) {
		switch (sides) {
		case OneSide:
			clsoptions << "oneside,";
			break;
		case TwoSides:
			clsoptions << "twoside,";
			break;
		}
	}

	// if needed
	if (columns != tclass.columns()) {
		if (columns == 2)
			clsoptions << "twocolumn,";
		else
			clsoptions << "onecolumn,";
	}

	if (!use_geometry
	    && orientation == ORIENTATION_LANDSCAPE)
		clsoptions << "landscape,";

	if (is_math_indent)
		clsoptions << "fleqn,";

	switch(math_numbering_side) {
	case LEFT:
		clsoptions << "leqno,";
		break;
	case RIGHT:
		clsoptions << "reqno,";
		features.require("amsmath");
		break;
	case DEFAULT:
		break;
	}

	if (paragraph_separation) {
		if (!tclass.halfparskip().empty() && getDefSkip().kind() == VSpace::HALFLINE)
			clsoptions << tclass.halfparskip() << ",";
		if (!tclass.fullparskip().empty() && getDefSkip().kind() == VSpace::FULLLINE)
			clsoptions << tclass.fullparskip() << ",";
	}

	// language should be a parameter to \documentclass
	if (language->babel() == "hebrew"
	    && default_language->babel() != "hebrew")
		// This seems necessary
		features.useLanguage(default_language);

	ostringstream language_options;
	bool const use_babel = features.useBabel() && !features.isProvided("babel");
	bool const use_polyglossia = features.usePolyglossia();
	bool const global = lyxrc.language_global_options;
	if (features.useBabel() || (use_polyglossia && global)) {
		language_options << features.getBabelLanguages();
		if (!language->babel().empty()) {
			if (!language_options.str().empty())
				language_options << ',';
			language_options << language->babel();
		}
		if (global && !language_options.str().empty())
			clsoptions << language_options.str() << ',';
	}

	// the predefined options from the layout
	if (use_default_options && !tclass.options().empty())
		clsoptions << tclass.options() << ',';

	// the user-defined options
	if (!options.empty()) {
		clsoptions << options << ',';
	}
	
	docstring const strOptions = from_utf8(clsoptions.str());
	if (!strOptions.empty()) {
		// Check if class options contain uncodable glyphs
		docstring uncodable_glyphs;
		docstring options_encodable;
		Encoding const * const enc = features.runparams().encoding;
		if (enc) {
			for (char_type c : strOptions) {
				if (!enc->encodable(c)) {
					docstring const glyph(1, c);
					LYXERR0("Uncodable character '"
						<< glyph
						<< "' in class options!");
					uncodable_glyphs += glyph;
					if (features.runparams().dryrun) {
						options_encodable += "<" + _("LyX Warning: ")
						   + _("uncodable character") + " '";
						options_encodable += c;
						options_encodable += "'>";
					}
				} else
					options_encodable += c;
			}
		} else
			options_encodable = strOptions;
	
		// On BUFFER_VIEW|UPDATE, warn user if we found uncodable glyphs
		if (!features.runparams().dryrun && !uncodable_glyphs.empty()) {
			frontend::Alert::warning(
				_("Uncodable character in class options"),
				support::bformat(
				  _("The class options of your document contain glyphs "
				    "that are unknown in the current document encoding "
				    "(namely %1$s).\nThese glyphs are omitted "
				    " from the output, which may result in "
				    "incomplete output."
				    "\n\nPlease select an appropriate "
				    "document encoding\n"
				    "(such as utf8) or change the "
				    "class options accordingly."),
				  uncodable_glyphs));
		}
		options_encodable = rtrim(options_encodable, ",");
		os << '[' << options_encodable << ']';
	}

	os << '{' << from_ascii(tclass.latexname()) << "}\n";
	// end of \documentclass defs

	// The package options (via \PassOptionsToPackage)
	os << from_ascii(features.getPackageOptions());

	// if we use fontspec or newtxmath, we have to load the AMS packages here
	string const ams = features.loadAMSPackages();
	string const main_font_enc = features.runparams().main_fontenc;
	bool const ot1 = (main_font_enc == "default" || main_font_enc == "OT1");
	bool const use_newtxmath =
		theLaTeXFonts().getLaTeXFont(from_ascii(fontsMath())).getUsedPackage(
			ot1, false, false) == "newtxmath";
	if ((useNonTeXFonts || use_newtxmath) && !ams.empty())
		os << from_ascii(ams);

	if (useNonTeXFonts) {
		// Babel (as of 2017/11/03) loads fontspec itself
		// However, it does so only if a non-default font is requested via \babelfont
		// Thus load fontspec if this is not the case and we need fontspec features
		bool const babel_needfontspec =
				!features.isAvailableAtLeastFrom("babel", 2017, 11, 3)
				|| (fontsRoman() == "default"
				    && fontsSans() == "default"
				    && fontsTypewriter() == "default"
				    // these need fontspec features
				    && (features.isRequired("textquotesinglep")
					|| features.isRequired("textquotedblp")));
		if (!features.isProvided("fontspec")
		    && (!features.useBabel() || babel_needfontspec))
			os << "\\usepackage{fontspec}\n";
		if (features.mustProvide("unicode-math")
		    && features.isAvailable("unicode-math"))
			os << "\\usepackage{unicode-math}\n";
	}

	// load CJK support package before font selection
	// (see autotests/export/latex/CJK/micro-sign_utf8-cjk-libertine.lyx)
	if (!useNonTeXFonts && encoding().package() != Encoding::none && inputenc != "utf8x"
		&& (encoding().package() == Encoding::CJK || features.mustProvide("CJK"))) {
		if (inputenc == "utf8-cjk" || inputenc == "utf8")
			os << "\\usepackage{CJKutf8}\n";
		else
			os << "\\usepackage[encapsulated]{CJK}\n";
	}

	// font selection must be done before loading fontenc.sty
	// but after babel with non-TeX fonts
	string const fonts = loadFonts(features);
	if (!fonts.empty() && (!features.useBabel() || !useNonTeXFonts))
		os << from_utf8(fonts);

	if (fonts_default_family != "default")
		os << "\\renewcommand{\\familydefault}{\\"
		   << from_ascii(fonts_default_family) << "}\n";

	// set font encoding
	// non-TeX fonts use font encoding TU (set by fontspec)
	if (!useNonTeXFonts && !features.isProvided("fontenc")
	    && main_font_enc != "default") {
		// get main font encodings
		vector<string> fontencs = font_encodings();
		// get font encodings of secondary languages
		// FIXME: some languages (hebrew, ...) assume a standard font encoding as last
		// option (for text in other languages).
		features.getFontEncodings(fontencs);
		if (!fontencs.empty()) {
			os << "\\usepackage["
			   << from_ascii(getStringFromVector(fontencs))
			   << "]{fontenc}\n";
		}
	}

	// Load textcomp and pmboxdraw before (lua)inputenc (#11454)
	if (features.mustProvide("textcomp"))
		os << "\\usepackage{textcomp}\n";
	if (features.mustProvide("pmboxdraw"))
		os << "\\usepackage{pmboxdraw}\n";
	
	// handle inputenc etc.
	// (In documents containing text in Thai language, 
	// we must load inputenc after babel, see lib/languages).
	if (!contains(features.getBabelPostsettings(), from_ascii("thai.ldf")))
		writeEncodingPreamble(os, features);

	// includeonly
	if (!features.runparams().includeall && !included_children_.empty()) {
		os << "\\includeonly{";
		bool first = true;
		// we do not use "auto const &" here, because incfile is modified later
		// coverity[auto_causes_copy]
		for (auto incfile : included_children_) {
			FileName inc = makeAbsPath(incfile, filepath.absFileName());
			string mangled = DocFileName(changeExtension(inc.absFileName(), ".tex")).
					mangledFileName();
			if (!features.runparams().nice)
				incfile = mangled;
			// \includeonly doesn't want an extension
			incfile = changeExtension(incfile, string());
			incfile = support::latex_path(incfile);
			if (!incfile.empty()) {
				if (!first)
					os << ",";
				os << from_utf8(incfile);
			}
			first = false;
		}
		os << "}\n";
	}

	if (!features.isProvided("geometry")
	    && (use_geometry || !class_supported_papersize)) {
		odocstringstream ods;
		if (!getGraphicsDriver("geometry").empty())
			ods << getGraphicsDriver("geometry");
		if (orientation == ORIENTATION_LANDSCAPE)
			ods << ",landscape";
		switch (papersize) {
		case PAPER_CUSTOM:
			if (!paperwidth.empty())
				ods << ",paperwidth="
				   << from_ascii(paperwidth);
			if (!paperheight.empty())
				ods << ",paperheight="
				   << from_ascii(paperheight);
			break;
		case PAPER_USLETTER:
		case PAPER_USLEGAL:
		case PAPER_USEXECUTIVE:
		case PAPER_A0:
		case PAPER_A1:
		case PAPER_A2:
		case PAPER_A3:
		case PAPER_A4:
		case PAPER_A5:
		case PAPER_A6:
		case PAPER_B0:
		case PAPER_B1:
		case PAPER_B2:
		case PAPER_B3:
		case PAPER_B4:
		case PAPER_B5:
		case PAPER_B6:
		case PAPER_C0:
		case PAPER_C1:
		case PAPER_C2:
		case PAPER_C3:
		case PAPER_C4:
		case PAPER_C5:
		case PAPER_C6:
		case PAPER_JISB0:
		case PAPER_JISB1:
		case PAPER_JISB2:
		case PAPER_JISB3:
		case PAPER_JISB4:
		case PAPER_JISB5:
		case PAPER_JISB6:
			ods << "," << from_ascii(string_papersize_geometry[papersize]);
			break;
		case PAPER_DEFAULT:
			break;
		}
		string g_options = to_ascii(trim(ods.str(), ","));
		// geometry must be loaded after graphics nowadays, since
		// graphic drivers might overwrite some settings
		// see https://tex.stackexchange.com/a/384952/19291
		// Hence we store this and output it later
		ostringstream gs;
		gs << "\\usepackage";
		// geometry-light means that the class works with geometry, but overwrites
		// the package options and paper sizes (memoir does this).
		// In this case, all options need to go to \geometry
		// and the standard paper sizes need to go to the class options.
		if (!g_options.empty() && !features.isProvided("geometry-light")) {
			gs << '[' << g_options << ']';
			g_options.clear();
		}
		gs << "{geometry}\n";
		if (use_geometry || features.isProvided("geometry-light")) {
			gs << "\\geometry{verbose";
			if (!g_options.empty())
				// Output general options here with "geometry light".
				gs << "," << g_options;
			// output this only if use_geometry is true
			if (use_geometry) {
				if (!topmargin.empty())
					gs << ",tmargin=" << Length(topmargin).asLatexString();
				if (!bottommargin.empty())
					gs << ",bmargin=" << Length(bottommargin).asLatexString();
				if (!leftmargin.empty())
					gs << ",lmargin=" << Length(leftmargin).asLatexString();
				if (!rightmargin.empty())
					gs << ",rmargin=" << Length(rightmargin).asLatexString();
				if (!headheight.empty())
					gs << ",headheight=" << Length(headheight).asLatexString();
				if (!headsep.empty())
					gs << ",headsep=" << Length(headsep).asLatexString();
				if (!footskip.empty())
					gs << ",footskip=" << Length(footskip).asLatexString();
				if (!columnsep.empty())
					gs << ",columnsep=" << Length(columnsep).asLatexString();
			}
			gs << "}\n";
		}
		set_geometry = gs.str();
	} else if (orientation == ORIENTATION_LANDSCAPE
		   || papersize != PAPER_DEFAULT) {
		features.require("papersize");
	}

	// only output when the background color is not default
	if (backgroundcolor != "none") {
		// only require color here, the background color will be defined
		// in LaTeXFeatures.cpp to avoid interferences with the LaTeX
		// package pdfpages
		if (theLaTeXColors().isLaTeXColor(backgroundcolor)) {
			LaTeXColor const lc = theLaTeXColors().getLaTeXColor(backgroundcolor);
			for (auto const & r : lc.req())
				features.require(r);
			features.require("xcolor");
			if (!lc.model().empty())
				features.require("xcolor:" + lc.model());
		}
		features.require("pagecolor");
	}

	// only output when the font color is not default
	if (fontcolor != "none") {
		// only require color here, the font color will be defined
		// in LaTeXFeatures.cpp to avoid interferences with the LaTeX
		// package pdfpages
		if (theLaTeXColors().isLaTeXColor(fontcolor)) {
			LaTeXColor const lc = theLaTeXColors().getLaTeXColor(fontcolor);
			for (auto const & r : lc.req())
				features.require(r);
			features.require("xcolor");
			if (!lc.model().empty())
				features.require("xcolor:" + lc.model());
		}
		features.require("fontcolor");
	}

	// Only if class has a ToC hierarchy
	if (tclass.hasTocLevels()) {
		if (secnumdepth != tclass.secnumdepth()) {
			os << "\\setcounter{secnumdepth}{"
			   << secnumdepth
			   << "}\n";
		}
		if (tocdepth != tclass.tocdepth()) {
			os << "\\setcounter{tocdepth}{"
			   << tocdepth
			   << "}\n";
		}
	}

	if (paragraph_separation) {
		// when skip separation
		string psopt;
		bool default_skip = false;
		bool by_class_option = false;
		switch (getDefSkip().kind()) {
		case VSpace::SMALLSKIP:
			psopt = "\\smallskipamount";
			break;
		case VSpace::MEDSKIP:
			psopt = "\\medskipamount";
			break;
		case VSpace::BIGSKIP:
			psopt = "\\bigskipamount";
			break;
		case VSpace::HALFLINE:
			// default (no option)
			default_skip = true;
			by_class_option = !tclass.halfparskip().empty();
			break;
		case VSpace::FULLLINE:
			psopt = "\\baselineskip";
			by_class_option = !tclass.fullparskip().empty();
			break;
		case VSpace::LENGTH:
			psopt = getDefSkip().length().asLatexString();
			break;
		default:
			break;
		}
		if (features.isProvided("parskip")) {
			// package already loaded (with arbitrary options)
			// change parskip value only
			if (!psopt.empty())
				os << "\\setlength{\\parskip}{" + psopt + "}\n";
			else if (default_skip)
				// explicitly reset default (might have been changed
				// in a class or package)
				os << "\\parskip=.5\\baselineskip plus 2pt\\relax\n";
		} else if (!by_class_option) {
			// load parskip package with required options
			string psopts;
			if (!psopt.empty()) {
				if (contains(psopt, ' '))
					// glue length has spaces: embrace
					psopts = "skip={" + psopt + "}";
				else
					psopts = "skip=" + psopt;
			}
			string const xpsopts = getPackageOptions("parskip");
			if (!xpsopts.empty()) {
				if (!psopts.empty())
					psopts += ",";
				psopts += xpsopts;
			}
			os << "\\usepackage";
			if (!psopts.empty())
				os << "[" << psopts << "]";
			os << "{parskip}\n";
		}
	} else {
		// when separation by indentation
		// only output something when a width is given
		if (!getParIndent().empty()) {
			os << "\\setlength{\\parindent}{"
			   << from_utf8(getParIndent().asLatexString())
			   << "}\n";
		}
	}

	if (is_math_indent) {
		// when formula indentation
		// only output something when it is not the default
		if (!getMathIndent().empty()) {
			os << "\\setlength{\\mathindent}{"
			   << from_utf8(getMathIndent().asString())
			   << "}\n";
		}
	}

	// Now insert the LyX specific LaTeX commands...
	features.resolveAlternatives();
	features.expandMultiples();

	if (output_sync) {
		if (!output_sync_macro.empty())
			os << from_utf8(output_sync_macro) +"\n";
		else if (features.runparams().flavor == Flavor::LaTeX)
			os << "\\usepackage[active]{srcltx}\n";
		else
			os << "\\synctex=-1\n";
	}

	// due to interferences with babel and hyperref, the color package has to
	// be loaded (when it is not already loaded) before babel when hyperref
	// is used with the colorlinks option, see
	// http://www.lyx.org/trac/ticket/5291
	// we decided therefore to load color always before babel, see
	// http://www.mail-archive.com/lyx-devel@lists.lyx.org/msg144349.html
	os << from_ascii(features.getColorOptions());

	// colortbl afterwards
	if (features.mustProvide("colortbl"))
		os << "\\usepackage{colortbl}\n";
	// and colortbl features:
	// table border color
	if (table_border_color != "default")
		os << "\\arrayrulecolor{"
			  << lcolor.getLaTeXName(lcolor.getFromLyXName(table_border_color)) << "}\n";

	// alternating row colors
	if (table_odd_row_color != "default" || table_even_row_color != "default") {
		string const odd_rc = (table_odd_row_color == "default")
				? string()
				: lcolor.getLaTeXName(lcolor.getFromLyXName(table_odd_row_color));
		string const even_rc = (table_even_row_color == "default")
				? string()
				: lcolor.getLaTeXName(lcolor.getFromLyXName(table_even_row_color));
		os << "\\rowcolors{"
			  << table_alt_row_colors_start
			  << "}{" << odd_rc << "}{" << even_rc << "}\n";
	}

	// If we use hyperref, jurabib, japanese or varioref,
	// we have to call babel before
	if (use_babel
	    && (features.isRequired("jurabib")
		|| features.isRequired("hyperref")
		|| features.isRequired("varioref")
		|| features.isRequired("japanese"))) {
			os << features.getBabelPresettings();
			// FIXME UNICODE
			os << from_utf8(babelCall(features, language_options.str(),
						  !lyxrc.language_global_options)) + '\n';
			os << features.getBabelPostsettings();
	}

	// The optional packages;
	os << from_ascii(features.getPackages());

	// Additional Indices
	if (features.isRequired("splitidx")) {
		for (auto const & idx : indiceslist()) {
			if (features.isProvided("memoir-idx")) {
				if (idx.shortcut() == "idx")
					continue;
				os << "\\makeindex[";
			} else
				os << "\\newindex{";
			os << escape(idx.shortcut());
			if (features.isProvided("memoir-idx"))
				os << "]\n";
			else
				os << "}\n";
		}
	}

	// with external references (xr), load and register the
	// externally referred files
	if (features.isRequired("xr")) {
		for (FileName & fn : features.buffer().externalRefFiles()) {
			Buffer const * buf = checkAndLoadLyXFile(fn, true);
			if (buf) {
				fn.changeExtension(string());
				if (features.runparams().nice) {
					os << "\\externaldocument{" << fn.relPath(features.buffer().filePath()) << "}\n";
				} else {
					FileName const in_file(addName(buf->temppath(), fn.onlyFileName()));
					os << "\\externaldocument{" << in_file.absFileName() << "}\n";
				}
			} else
				LYXERR0("File " << fn.absFileName() << " not could not be loaded. External reference to it won't work.");
		}
	}

	// Line spacing
	os << from_utf8(spacing().writePreamble(features.isProvided("SetSpace")));

	// PDF support.
	// * Hyperref manual: "Make sure it comes last of your loaded
	//   packages, to give it a fighting chance of not being over-written,
	//   since its job is to redefine many LaTeX commands."
	// * Email from Heiko Oberdiek: "It is usually better to load babel
	//   before hyperref. Then hyperref has a chance to detect babel.
	// * Has to be loaded before the "LyX specific LaTeX commands" to
	//   avoid errors with algorithm floats.
	// use hyperref explicitly if it is required
	if (features.isRequired("hyperref")) {
		OutputParams tmp_params = features.runparams();
		pdfoptions().writeLaTeX(tmp_params, os,
					features.isProvided("hyperref"));
		// correctly break URLs with hyperref and dvi/ps output
		if (features.runparams().hyperref_driver == "dvips"
		    && features.isAvailable("breakurl"))
			os << "\\usepackage{breakurl}\n";
	} else if (features.isRequired("nameref"))
		// hyperref loads this automatically
		os << "\\usepackage{nameref}\n";

	if (use_lineno){
		os << "\\usepackage";
		if (!lineno_opts.empty())
			os << "[" << lineno_opts << "]";
		os << "{lineno}\n";
		os << "\\linenumbers\n";
	}

	// bibtopic needs to be loaded after hyperref.
	// the dot provides the aux file naming which LyX can detect.
	if (features.mustProvide("bibtopic"))
		os << "\\usepackage[dot]{bibtopic}\n";

	// Will be surrounded by \makeatletter and \makeatother when not empty
	otexstringstream atlyxpreamble;

	// Some macros LyX will need
	{
		TexString tmppreamble = features.getMacros();
		if (!tmppreamble.str.empty())
			atlyxpreamble << "\n%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% "
			                 "LyX specific LaTeX commands.\n"
			              << std::move(tmppreamble)
			              << '\n';
	}
	// the text class specific preamble
	{
		docstring tmppreamble = features.getTClassPreamble();
		if (!tmppreamble.empty())
			atlyxpreamble << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% "
			                 "Textclass specific LaTeX commands.\n"
			              << tmppreamble
			              << '\n';
	}
	// suppress date if selected
	// use \@ifundefined because we cannot be sure that every document class
	// has a \date command
	if (suppress_date)
		atlyxpreamble << "\\@ifundefined{date}{}{\\date{}}\n";

	/* the user-defined preamble */
	if (!containsOnly(preamble, " \n\t")) {
		// FIXME UNICODE
		atlyxpreamble << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% "
		                 "User specified LaTeX commands.\n";

		// Check if the user preamble contains uncodable glyphs
		odocstringstream user_preamble;
		docstring uncodable_glyphs;
		Encoding const * const enc = features.runparams().encoding;
		if (enc) {
			for (char_type c : preamble) {
				if (!enc->encodable(c)) {
					docstring const glyph(1, c);
					LYXERR0("Uncodable character '"
						<< glyph
						<< "' in user preamble!");
					uncodable_glyphs += glyph;
					if (features.runparams().dryrun) {
						user_preamble << "<" << _("LyX Warning: ")
						   << _("uncodable character") << " '";
						user_preamble.put(c);
						user_preamble << "'>";
					}
				} else
					user_preamble.put(c);
			}
		} else
			user_preamble << preamble;

		// On BUFFER_VIEW|UPDATE, warn user if we found uncodable glyphs
		if (!features.runparams().dryrun && !uncodable_glyphs.empty()) {
			frontend::Alert::warning(
				_("Uncodable character in user preamble"),
				support::bformat(
				  _("The user preamble of your document contains glyphs "
				    "that are unknown in the current document encoding "
				    "(namely %1$s).\nThese glyphs are omitted "
				    " from the output, which may result in "
				    "incomplete output."
				    "\n\nPlease select an appropriate "
				    "document encoding\n"
				    "(such as utf8) or change the "
				    "preamble code accordingly."),
				  uncodable_glyphs));
		}
		atlyxpreamble << user_preamble.str() << '\n';
	}

	// footmisc must be loaded after setspace
	// Load it here to avoid clashes with footmisc loaded in the user
	// preamble. For that reason we also pass the options via
	// \PassOptionsToPackage in getPreamble() and not here.
	if (features.mustProvide("footmisc"))
		atlyxpreamble << "\\usepackage{footmisc}\n";

	// subfig loads internally the LaTeX package "caption". As
	// caption is a very popular package, users will load it in
	// the preamble. Therefore we must load subfig behind the
	// user-defined preamble and check if the caption package was
	// loaded or not. For the case that caption is loaded before
	// subfig, there is the subfig option "caption=false". This
	// option also works when a koma-script class is used and
	// koma's own caption commands are used instead of caption. We
	// use \PassOptionsToPackage here because the user could have
	// already loaded subfig in the preamble.
	if (features.mustProvide("subfig"))
		atlyxpreamble << "\\ifdefined\\showcaptionsetup\n"
		                 " % Caption package is used. Advise subfig not to load it again.\n"
		                 " \\PassOptionsToPackage{caption=false}{subfig}\n"
		                 "\\fi\n"
		                 "\\usepackage{subfig}\n";

	// Itemize bullet settings need to be last in case the user
	// defines their own bullets that use a package included
	// in the user-defined preamble -- ARRae
	// Actually it has to be done much later than that
	// since some packages like frenchb make modifications
	// at \begin{document} time -- JMarc
	docstring bullets_def;
	for (int i = 0; i < 4; ++i) {
		if (user_defined_bullet(i) != ITEMIZE_DEFAULTS[i]) {
			if (bullets_def.empty())
				bullets_def += "\\AtBeginDocument{\n";
			bullets_def += "  \\def\\labelitemi";
			switch (i) {
				// `i' is one less than the item to modify
			case 0:
				break;
			case 1:
				bullets_def += 'i';
				break;
			case 2:
				bullets_def += "ii";
				break;
			case 3:
				bullets_def += 'v';
				break;
			}
			bullets_def += '{' +
				user_defined_bullet(i).getText()
				+ "}\n";
		}
	}

	if (!bullets_def.empty())
		atlyxpreamble << bullets_def << "}\n\n";

	if (!atlyxpreamble.empty())
		os << "\n\\makeatletter\n"
		   << atlyxpreamble.release()
		   << "\\makeatother\n\n";

	// theorem definitions: Must be loaded after user preamble
	// but before babel
	os << features.getThmDefinitions();

	// We try to load babel late, in case it interferes with other packages.
	// Jurabib, hyperref, varioref, bicaption, menukeys and listings (bug 8995)
	// have to be called after babel, though.
	if (use_babel && !features.isRequired("jurabib")
	    && !features.isRequired("hyperref")
	    && !features.isRequired("varioref")
	    && !features.isRequired("japanese")) {
		os << features.getBabelPresettings();
		// FIXME UNICODE
		os << from_utf8(babelCall(features, language_options.str(),
		                          !lyxrc.language_global_options)) + '\n';
		os << features.getBabelPostsettings();
	}
	// In documents containing text in Thai language, 
	// we must load inputenc after babel (see lib/languages).
	if (contains(features.getBabelPostsettings(), from_ascii("thai.ldf")))
		writeEncodingPreamble(os, features);

	// font selection must be done after babel with non-TeX fonts
	if (!fonts.empty() && features.useBabel() && useNonTeXFonts)
		os << from_utf8(fonts);

	if (features.isRequired("bicaption"))
		os << "\\usepackage{bicaption}\n";
	if (!listings_params.empty()
	    || features.mustProvide("listings")
	    || features.mustProvide("minted")) {
		if (use_minted)
			os << "\\usepackage{minted}\n";
		else
			os << "\\usepackage{listings}\n";
	}
	string lst_params = listings_params;
	// If minted, do not output the language option (bug 11203)
	if (use_minted && contains(lst_params, "language=")) {
		vector<string> opts =
			getVectorFromString(lst_params, ",", false);
		for (size_t i = 0; i < opts.size(); ++i) {
			if (prefixIs(opts[i], "language="))
				opts.erase(opts.begin() + i--);
		}
		lst_params = getStringFromVector(opts, ",");
	}
	if (!lst_params.empty()) {
		if (use_minted)
			os << "\\setminted{";
		else
			os << "\\lstset{";
		// do not test validity because listings_params is
		// supposed to be valid
		string par =
			InsetListingsParams(lst_params).separatedParams(true);
		os << from_utf8(par);
		os << "}\n";
	}

	// xunicode only needs to be loaded if tipa is used
	// (the rest is obsoleted by the new TU encoding).
	// It needs to be loaded at least after amsmath, amssymb,
	// esint and the other packages that provide special glyphs
	if (features.mustProvide("tipa") && useNonTeXFonts
	    && !features.isProvided("xunicode")) {
		// The `xunicode` package officially only supports XeTeX,
		//  but also works with LuaTeX. We work around its XeTeX test.
		if (features.runparams().flavor != Flavor::XeTeX) {
			os << "% Pretend to xunicode that we are XeTeX\n"
			   << "\\def\\XeTeXpicfile{}\n";
		}
		os << "\\usepackage{xunicode}\n";
	}

	// covington must be loaded after beamerarticle
	if (features.isRequired("covington"))
	    os << "\\usepackage{covington}\n";

	// Polyglossia must be loaded last ...
	if (use_polyglossia) {
		// call the package
		os << "\\usepackage{polyglossia}\n";
		// set the main language
		os << "\\setdefaultlanguage";
		if (!polyglossiaLangOptions(language->lang()).empty())
			os << "[" << from_ascii(polyglossiaLangOptions(language->lang())) << "]";
		os << "{" << from_ascii(language->polyglossia()) << "}\n";
		// now setup the other languages
		set<string> const polylangs =
			features.getPolyglossiaLanguages();
		for (auto const & pl : polylangs) {
			// We do not output the options here; they are output in
			// the language switch commands. This is safer if multiple
			// varieties are used.
			if (pl == language->polyglossia())
				continue;
			os << "\\setotherlanguage";
			os << "{" << from_ascii(pl) << "}\n";
		}
	}

	// ... but before biblatex (see #7065)
	if ((features.mustProvide("biblatex")
	     || features.isRequired("biblatex-chicago"))
	    && !features.isProvided("biblatex-chicago")
	    && !features.isProvided("biblatex-natbib")
	    && !features.isProvided("natbib-internal")
	    && !features.isProvided("natbib")
	    && !features.isProvided("jurabib")) {
		// The biblatex-chicago package has a differing interface
		// it uses a wrapper package and loads styles via fixed options
		bool const chicago = features.isRequired("biblatex-chicago");
		string delim = "";
		string opts;
		os << "\\usepackage";
		if (!biblatex_bibstyle.empty()
		    && (biblatex_bibstyle == biblatex_citestyle)
		    && !chicago) {
			opts = "style=" + biblatex_bibstyle;
			delim = ",";
		} else if (!chicago) {
			if (!biblatex_bibstyle.empty()) {
				opts = "bibstyle=" + biblatex_bibstyle;
				delim = ",";
			}
			if (!biblatex_citestyle.empty()) {
				opts += delim + "citestyle=" + biblatex_citestyle;
				delim = ",";
			}
		}
		if (!multibib.empty() && multibib != "child") {
			opts += delim + "refsection=" + multibib;
			delim = ",";
		}
		if (bibtexCommand() == "bibtex8"
		    || prefixIs(bibtexCommand(), "bibtex8 ")) {
			opts += delim + "backend=bibtex8";
			delim = ",";
		} else if (bibtexCommand() == "bibtex"
			   || prefixIs(bibtexCommand(), "bibtex ")) {
			opts += delim + "backend=bibtex";
			delim = ",";
		}
		if (!bib_encoding.empty() && encodings.fromLyXName(bib_encoding)) {
			opts += delim + "bibencoding="
				+ encodings.fromLyXName(bib_encoding)->latexName();
			delim = ",";
		}
		// biblatex-chicago offers the style options "authordate"
		// or "authordate-trad". We use "authordate" if none
		// is given via the options field.
		if (chicago && citeEngineType() == ENGINE_TYPE_AUTHORYEAR
		    && !contains(biblio_opts, "authordate")) {
			opts += delim + "authordate";
			delim = ",";
			
		}
		if (!biblio_opts.empty())
			opts += delim + biblio_opts;
		if (!opts.empty())
			os << "[" << opts << "]";
		if (chicago)
			os << "{biblatex-chicago}\n";
		else
			os << "{biblatex}\n";
	}


	// Load custom language package here
	if (features.langPackage() == LaTeXFeatures::LANG_PACK_CUSTOM) {
		if (lang_package == "default")
			os << from_utf8(lyxrc.language_custom_package);
		else
			os << from_utf8(lang_package);
		os << '\n';
	}

	// cleveref needs to be loaded very late as well
	if (features.mustProvide("cleveref")) {
		os << "\\usepackage";
		if (!lyxrc.language_global_options && !language_options.str().empty())
			os << "[" << language_options.str() << "]";
		os << "{cleveref}\n";
	}

	// package-specific theorem definitions
	os << features.getThmExtraDefinitions();

	// Since menukeys uses catoptions, which does some heavy changes on key-value options,
	// it is recommended to load menukeys as the last package (even after hyperref)
	if (features.isRequired("menukeys"))
		os << "\\usepackage{menukeys}\n";

	docstring const i18npreamble =
		features.getTClassI18nPreamble(use_babel, use_polyglossia,
					       use_minted);
	if (!i18npreamble.empty())
		os << i18npreamble + '\n';

	return use_babel;
}


void BufferParams::useClassDefaults()
{
	DocumentClass const & tclass = documentClass();

	sides = tclass.sides();
	columns = tclass.columns();
	pagestyle = tclass.pagestyle();
	tablestyle = tclass.tablestyle();
	use_default_options = true;
	// Only if class has a ToC hierarchy
	if (tclass.hasTocLevels()) {
		secnumdepth = tclass.secnumdepth();
		tocdepth = tclass.tocdepth();
	}
}


bool BufferParams::hasClassDefaults() const
{
	DocumentClass const & tclass = documentClass();

	return sides == tclass.sides()
		&& columns == tclass.columns()
		&& pagestyle == tclass.pagestyle()
		&& tablestyle == tclass.tablestyle()
		&& use_default_options
		&& secnumdepth == tclass.secnumdepth()
		&& tocdepth == tclass.tocdepth();
}


DocumentClass const & BufferParams::documentClass() const
{
	return *doc_class_;
}


DocumentClassConstPtr BufferParams::documentClassPtr() const
{
	return doc_class_;
}


void BufferParams::setDocumentClass(DocumentClassConstPtr const & tc)
{
	// evil, but this function is evil
	doc_class_ = const_pointer_cast<DocumentClass>(tc);
	invalidateConverterCache();
}


bool BufferParams::setBaseClass(string const & classname, string const & path)
{
	LYXERR(Debug::TCLASS, "setBaseClass: " << classname);
	LayoutFileList & bcl = LayoutFileList::get();
	if (!bcl.haveClass(classname)) {
		docstring s =
			bformat(_("The layout file:\n"
				"%1$s\n"
				"could not be found. A default textclass with default\n"
				"layouts will be used. LyX will not be able to produce\n"
				"correct output."),
			from_utf8(classname));
		frontend::Alert::error(_("Document class not found"), s);
		bcl.addEmptyClass(classname);
	}

	bool const success = bcl[classname].load(path);
	if (!success) {
		docstring s =
			bformat(_("Due to some error in it, the layout file:\n"
				"%1$s\n"
				"could not be loaded. A default textclass with default\n"
				"layouts will be used. LyX will not be able to produce\n"
				"correct output."),
			from_utf8(classname));
		frontend::Alert::error(_("Could not load class"), s);
		bcl.addEmptyClass(classname);
	}

	pimpl_->baseClass_ = classname;
	layout_modules_.adaptToBaseClass(baseClass(), removed_modules_);
	return true;
}


LayoutFile const * BufferParams::baseClass() const
{
	if (LayoutFileList::get().haveClass(pimpl_->baseClass_))
		return &(LayoutFileList::get()[pimpl_->baseClass_]);

	return nullptr;
}


LayoutFileIndex const & BufferParams::baseClassID() const
{
	return pimpl_->baseClass_;
}


void BufferParams::makeDocumentClass(bool clone, bool internal)
{
	if (!baseClass())
		return;

	invalidateConverterCache();
	LayoutModuleList mods;
	for (auto const & mod : layout_modules_)
		mods.push_back(mod);

	doc_class_ = getDocumentClass(*baseClass(), mods, cite_engine_, clone, internal);

	TextClass::ReturnValues success = TextClass::OK;
	if (!forced_local_layout_.empty())
		success = doc_class_->read(to_utf8(forced_local_layout_),
		                           TextClass::MODULE);
	if (!local_layout_.empty() &&
	    (success == TextClass::OK || success == TextClass::OK_OLDFORMAT))
		success = doc_class_->read(to_utf8(local_layout_), TextClass::MODULE);
	if (success != TextClass::OK && success != TextClass::OK_OLDFORMAT) {
		docstring const msg = _("Error reading internal layout information");
		frontend::Alert::warning(_("Read Error"), msg);
	}
}


bool BufferParams::layoutModuleCanBeAdded(string const & modName) const
{
	return layout_modules_.moduleCanBeAdded(modName, baseClass());
}


docstring BufferParams::getLocalLayout(bool forced) const
{
	if (forced)
		return from_utf8(doc_class_->forcedLayouts());
	else
		return local_layout_;
}


void BufferParams::setLocalLayout(docstring const & layout, bool forced)
{
	if (forced)
		forced_local_layout_ = layout;
	else
		local_layout_ = layout;
}


bool BufferParams::addLayoutModule(string const & modName)
{
	for (auto const & mod : layout_modules_)
		if (mod == modName)
			return false;
	layout_modules_.push_back(modName);
	return true;
}


string BufferParams::bufferFormat() const
{
	return documentClass().outputFormat();
}


bool BufferParams::isExportable(string const & format, bool need_viewable) const
{
	FormatList const & formats = exportableFormats(need_viewable);
	for (auto const & fmt : formats) {
		if (fmt->name() == format)
			return true;
	}
	return false;
}


FormatList const & BufferParams::exportableFormats(bool only_viewable) const
{
	FormatList & cached = only_viewable ?
			pimpl_->viewableFormatList : pimpl_->exportableFormatList;
	bool & valid = only_viewable ?
			pimpl_->isViewCacheValid : pimpl_->isExportCacheValid;
	if (valid)
		return cached;

	vector<string> const backs = backends();
	set<string> excludes;
	if (useNonTeXFonts) {
		excludes.insert("latex");
		excludes.insert("pdflatex");
	} else if (inputenc != "ascii" && inputenc != "utf8-plain") {
		  // XeTeX with TeX fonts requires input encoding ascii (#10600).
		  excludes.insert("xetex");
	}

	FormatList result =
		theConverters().getReachable(backs[0], only_viewable, true, excludes);
	vector<string>::const_iterator it = backs.begin() + 1;
	for (; it != backs.end(); ++it) {
		FormatList r = theConverters().getReachable(*it, only_viewable,
													false, excludes);
		result.insert(result.end(), r.begin(), r.end());
	}
	sort(result.begin(), result.end(), Format::formatSorter);
	cached = result;
	valid = true;
	return cached;
}


vector<string> BufferParams::backends() const
{
	vector<string> v;
	string const buffmt = bufferFormat();

	// FIXME: Don't hardcode format names here, but use a flag
	if (buffmt == "latex") {
		if (encoding().package() == Encoding::japanese)
			v.push_back("platex");
		else {
			if (!useNonTeXFonts) {
				v.push_back("pdflatex");
				v.push_back("latex");
			}
			if (useNonTeXFonts 
				|| inputenc == "ascii" || inputenc == "utf8-plain")
				v.push_back("xetex");
			v.push_back("luatex");
			v.push_back("dviluatex");
		}
	} else {
		string rbuffmt = buffmt;
		// If we use an OutputFormat in Japanese docs,
		// we need special format in order to get the path
		// via pLaTeX (#8823)
		if (documentClass().hasOutputFormat()
		    && encoding().package() == Encoding::japanese)
			rbuffmt += "-ja";
		v.push_back(rbuffmt);
	}

	v.push_back("xhtml");
	v.push_back("docbook5");
	v.push_back("text");
	v.push_back("lyx");
	return v;
}


Flavor BufferParams::getOutputFlavor(string const & format) const
{
	string const dformat = (format.empty() || format == "default") ?
		getDefaultOutputFormat() : format;
	DefaultFlavorCache::const_iterator it =
		default_flavors_.find(dformat);

	if (it != default_flavors_.end())
		return it->second;

	Flavor result = Flavor::LaTeX;

	// FIXME It'd be better not to hardcode this, but to do
	//       something with formats.
	if (dformat == "xhtml")
		result = Flavor::Html;
	else if (dformat == "docbook5")
		result = Flavor::DocBook5;
	else if (dformat == "text")
		result = Flavor::Text;
	else if (dformat == "lyx")
		result = Flavor::LyX;
	else if (dformat == "pdflatex")
		result = Flavor::PdfLaTeX;
	else if (dformat == "xetex")
		result = Flavor::XeTeX;
	else if (dformat == "luatex")
		result = Flavor::LuaTeX;
	else if (dformat == "dviluatex")
		result = Flavor::DviLuaTeX;
	else {
		// Try to determine flavor of default output format
		vector<string> backs = backends();
		if (find(backs.begin(), backs.end(), dformat) == backs.end()) {
			// Get shortest path to format
			Graph::EdgePath path;
			for (auto const & bvar : backs) {
				Graph::EdgePath p = theConverters().getPath(bvar, dformat);
				if (!p.empty() && (path.empty() || p.size() < path.size())) {
					path = p;
				}
			}
			if (!path.empty())
				result = theConverters().getFlavor(path);
		}
	}
	// cache this flavor
	default_flavors_[dformat] = result;
	return result;
}


string BufferParams::getDefaultOutputFormat() const
{
	if (!default_output_format.empty()
	    && default_output_format != "default")
		return default_output_format;
	if (encoding().package() == Encoding::japanese)
		return lyxrc.default_platex_view_format;
	if (useNonTeXFonts)
		return lyxrc.default_otf_view_format;
	return lyxrc.default_view_format;
}

Font const BufferParams::getFont() const
{
	FontInfo f = documentClass().defaultfont();
	if (fonts_default_family == "rmdefault")
		f.setFamily(ROMAN_FAMILY);
	else if (fonts_default_family == "sfdefault")
		f.setFamily(SANS_FAMILY);
	else if (fonts_default_family == "ttdefault")
		f.setFamily(TYPEWRITER_FAMILY);
	return Font(f, language);
}


QuoteStyle BufferParams::getQuoteStyle(string const & qs) const
{
	return quotesstyletranslator().find(qs);
}


bool BufferParams::isLatex() const
{
	return documentClass().outputType() == LATEX;
}


bool BufferParams::isLiterate() const
{
	return documentClass().outputType() == LITERATE;
}


bool BufferParams::hasPackageOption(string const & package, string const & opt) const
{
	for (auto const & p : documentClass().packageOptions())
		if (package == p.first && opt == p.second)
			return true;
	return false;
}


string BufferParams::getPackageOptions(string const & package) const
{
	for (auto const & p : documentClass().packageOptions())
		if (package == p.first)
			return p.second;
	return string();
}


bool BufferParams::useBidiPackage(OutputParams const & rp) const
{
	return (rp.use_polyglossia
		// as of babel 3.29, bidi=bidi-* is supported by babel
		// So we check whether we use a respective version and
		// whethert bidi-r or bidi-l have been requested either via class
		// or package options
		|| (rp.use_babel
		    && LaTeXFeatures::isAvailableAtLeastFrom("babel", 2019, 4, 3)
		    && useNonTeXFonts)
		)
		&& rp.flavor == Flavor::XeTeX;
}


void BufferParams::readPreamble(Lexer & lex)
{
	if (lex.getString() != "\\begin_preamble")
		lyxerr << "Error (BufferParams::readPreamble):"
			"consistency check failed." << endl;

	preamble = lex.getLongString(from_ascii("\\end_preamble"));
}


void BufferParams::readDocumentMetadata(Lexer & lex)
{
	if (lex.getString() != "\\begin_metadata")
		lyxerr << "Error (BufferParams::readDocumentMetadata):"
			"consistency check failed." << endl;

	document_metadata = lex.getLongString(from_ascii("\\end_metadata"));
}


void BufferParams::readLocalLayout(Lexer & lex, bool forced)
{
	string const expected = forced ? "\\begin_forced_local_layout" :
	                                 "\\begin_local_layout";
	if (lex.getString() != expected)
		lyxerr << "Error (BufferParams::readLocalLayout):"
			"consistency check failed." << endl;

	if (forced)
		forced_local_layout_ =
			lex.getLongString(from_ascii("\\end_forced_local_layout"));
	else
		local_layout_ = lex.getLongString(from_ascii("\\end_local_layout"));
}


bool BufferParams::setLanguage(string const & lang)
{
	Language const *new_language = languages.getLanguage(lang);
	if (!new_language) {
		// Language lang was not found
		return false;
	}
	language = new_language;
	return true;
}


void BufferParams::readLanguage(Lexer & lex)
{
	if (!lex.next()) return;

	string const tmptok = lex.getString();

	// check if tmptok is part of tex_babel in tex-defs.h
	if (!setLanguage(tmptok)) {
		// Language tmptok was not found
		language = default_language;
		lyxerr << "Warning: Setting language `"
		       << tmptok << "' to `" << language->lang()
		       << "'." << endl;
	}
}


void BufferParams::readGraphicsDriver(Lexer & lex)
{
	if (!lex.next())
		return;

	string const tmptok = lex.getString();
	// check if tmptok is part of tex_graphics in tex_defs.h
	int n = 0;
	while (true) {
		string const test = tex_graphics[n++];

		if (test == tmptok) {
			graphics_driver = tmptok;
			break;
		}
		if (test.empty()) {
			lex.printError(
				"Warning: graphics driver `$$Token' not recognized!\n"
				"         Setting graphics driver to `default'.\n");
			graphics_driver = "default";
			break;
		}
	}
}


void BufferParams::readBullets(Lexer & lex)
{
	if (!lex.next())
		return;

	int const index = lex.getInteger();
	lex.next();
	int temp_int = lex.getInteger();
	user_defined_bullet(index).setFont(temp_int);
	temp_bullet(index).setFont(temp_int);
	lex >> temp_int;
	user_defined_bullet(index).setCharacter(temp_int);
	temp_bullet(index).setCharacter(temp_int);
	lex >> temp_int;
	user_defined_bullet(index).setSize(temp_int);
	temp_bullet(index).setSize(temp_int);
}


void BufferParams::readBulletsLaTeX(Lexer & lex)
{
	// The bullet class should be able to read this.
	if (!lex.next())
		return;
	int const index = lex.getInteger();
	lex.next(true);
	docstring const temp_str = lex.getDocString();

	user_defined_bullet(index).setText(temp_str);
	temp_bullet(index).setText(temp_str);
}


void BufferParams::readModules(Lexer & lex)
{
	if (!lex.eatLine()) {
		lyxerr << "Error (BufferParams::readModules):"
				"Unexpected end of input." << endl;
		return;
	}
	while (true) {
		string mod = lex.getString();
		if (mod == "\\end_modules")
			break;
		addLayoutModule(mod);
		lex.eatLine();
	}
}


void BufferParams::readRemovedModules(Lexer & lex)
{
	if (!lex.eatLine()) {
		lyxerr << "Error (BufferParams::readRemovedModules):"
				"Unexpected end of input." << endl;
		return;
	}
	while (true) {
		string mod = lex.getString();
		if (mod == "\\end_removed_modules")
			break;
		removed_modules_.push_back(mod);
		lex.eatLine();
	}
	// now we want to remove any removed modules that were previously
	// added. normally, that will be because default modules were added in
	// setBaseClass(), which gets called when \textclass is read at the
	// start of the read.
	for (auto const & rm : removed_modules_) {
		LayoutModuleList::iterator const mit = layout_modules_.begin();
		LayoutModuleList::iterator const men = layout_modules_.end();
		LayoutModuleList::iterator found = find(mit, men, rm);
		if (found == men)
			continue;
		layout_modules_.erase(found);
	}
}


void BufferParams::readIncludeonly(Lexer & lex)
{
	if (!lex.eatLine()) {
		lyxerr << "Error (BufferParams::readIncludeonly):"
				"Unexpected end of input." << endl;
		return;
	}
	while (true) {
		string child = lex.getString();
		if (child == "\\end_includeonly")
			break;
		included_children_.push_back(child);
		lex.eatLine();
	}
}


string BufferParams::paperSizeName(PapersizePurpose purpose, string const & psize) const
{
	PAPER_SIZE ppsize = psize.empty() ? papersize : papersizetranslator().find(psize);
	switch (ppsize) {
	case PAPER_DEFAULT:
		if (documentClass().pagesize() == "default")
			// could be anything, so don't guess
			return string();
		return paperSizeName(purpose, documentClass().pagesize());
	case PAPER_CUSTOM: {
		if (purpose == XDVI && !paperwidth.empty() &&
		    !paperheight.empty()) {
			// heightxwidth<unit>
			string first = paperwidth;
			string second = paperheight;
			if (orientation == ORIENTATION_LANDSCAPE)
				first.swap(second);
			// cut off unit.
			return first.erase(first.length() - 2)
				+ "x" + second;
		}
		return string();
	}
	case PAPER_A0:
		// dvips and dvipdfm do not know this
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "a0";
	case PAPER_A1:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "a1";
	case PAPER_A2:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "a2";
	case PAPER_A3:
		return "a3";
	case PAPER_A4:
		return "a4";
	case PAPER_A5:
		return "a5";
	case PAPER_A6:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "a6";
	case PAPER_B0:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "b0";
	case PAPER_B1:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "b1";
	case PAPER_B2:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "b2";
	case PAPER_B3:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "b3";
	case PAPER_B4:
		// dvipdfm does not know this
		if (purpose == DVIPDFM)
			return string();
		return "b4";
	case PAPER_B5:
		if (purpose == DVIPDFM)
			return string();
		return "b5";
	case PAPER_B6:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "b6";
	case PAPER_C0:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "c0";
	case PAPER_C1:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "c1";
	case PAPER_C2:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "c2";
	case PAPER_C3:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "c3";
	case PAPER_C4:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "c4";
	case PAPER_C5:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "c5";
	case PAPER_C6:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "c6";
	case PAPER_JISB0:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "jisb0";
	case PAPER_JISB1:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "jisb1";
	case PAPER_JISB2:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "jisb2";
	case PAPER_JISB3:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "jisb3";
	case PAPER_JISB4:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "jisb4";
	case PAPER_JISB5:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "jisb5";
	case PAPER_JISB6:
		if (purpose == DVIPS || purpose == DVIPDFM)
			return string();
		return "jisb6";
	case PAPER_USEXECUTIVE:
		// dvipdfm does not know this
		if (purpose == DVIPDFM)
			return string();
		return "foolscap";
	case PAPER_USLEGAL:
		return "legal";
	case PAPER_USLETTER:
	default:
		if (purpose == XDVI)
			return "us";
		return "letter";
	}
}


string const BufferParams::dvips_options() const
{
	string result;

	// If the class loads the geometry package, we do not know which
	// paper size is used, since we do not set it (bug 7013).
	// Therefore we must not specify any argument here.
	// dvips gets the correct paper size via DVI specials in this case
	// (if the class uses the geometry package correctly).
	if (documentClass().provides("geometry"))
		return result;

	if (use_geometry
	    && papersize == PAPER_CUSTOM
	    && !lyxrc.print_paper_dimension_flag.empty()
	    && !paperwidth.empty()
	    && !paperheight.empty()) {
		// using a custom papersize
		result = lyxrc.print_paper_dimension_flag;
		result += ' ' + paperwidth;
		result += ',' + paperheight;
	} else {
		string const paper_option = paperSizeName(DVIPS);
		if (!paper_option.empty() && (paper_option != "letter" ||
		    orientation != ORIENTATION_LANDSCAPE)) {
			// dvips won't accept -t letter -t landscape.
			// In all other cases, include the paper size
			// explicitly.
			result = lyxrc.print_paper_flag;
			result += ' ' + paper_option;
		}
	}
	if (orientation == ORIENTATION_LANDSCAPE &&
	    papersize != PAPER_CUSTOM)
		result += ' ' + lyxrc.print_landscape_flag;
	return result;
}


string const BufferParams::main_font_encoding() const
{
	vector<string> const fencs = font_encodings();
	if (fencs.empty()) {
		if (ascii_lowercase(language->fontenc(*this)) == "none")
			return "none";
		return "default";
	}
	return fencs.back();
}


vector<string> const BufferParams::font_encodings() const
{
	string doc_fontenc = (fontenc == "auto") ? string() : fontenc;

	vector<string> fontencs;

	// "default" means "no explicit font encoding"
	if (doc_fontenc != "default") {
		if (!doc_fontenc.empty())
			// If we have a custom setting, we use only that!
			return getVectorFromString(doc_fontenc);
		string const lfe = language->fontenc(*this);
		if (!lfe.empty()
		    && ascii_lowercase(language->fontenc(*this)) != "none") {
			vector<string> fencs = getVectorFromString(lfe);
			for (auto & fe : fencs) {
				if (find(fontencs.begin(), fontencs.end(), fe) == fontencs.end())
					fontencs.push_back(fe);
			}
		}
	}

	return fontencs;
}


string BufferParams::babelCall(LaTeXFeatures const & features, string lang_opts,
			       bool const langoptions) const
{
	// suppress the babel call if there is no BabelName defined
	// for the document language in the lib/languages file and if no
	// other languages are used (lang_opts is then empty)
	if (lang_opts.empty())
		return string();
	// get language options with modifiers
	bool have_mods = false;
	vector<string> blangs;
	std::set<Language const *> langs = features.getLanguages();
	// add main language
	langs.insert(language);
	ostringstream os;
	string force_provide;
	bool have_main_forceprovide = false;
	bool have_other_forceprovide = useNonTeXFonts
			? languages.haveOtherForceProvide()
			: false;
	for (auto const & l : langs) {
		string blang = l->babel();
		bool use_opt = langoptions;
		if (blang.empty())
			continue;
		int const bp = l->useBabelProvide();
		// pass option as modifier if apt
		if (bp != 2 && !have_other_forceprovide && l->babelOptFormat() == "modifier") {
			vector<string> opts = getVectorFromString(babelLangOptions(l->lang()));
			bool have_one = false;
			for (string const & s : opts) {
				have_mods = true;
				if (!features.isAvailableAtLeastFrom("babel", 2023, 05, 11)
				    || langoptions || have_one)
					blang += "." + s;
				else {
					// modifiers.<lang>=opts is provided as of
					// babel 3.89 (2023-05-11)
					blang = "modifiers." + blang + "=" + s;
					have_one = true;
				}
				use_opt = true;
			}
		}
		// language that always requires \babelprovide
		if (bp == 1 && !have_other_forceprovide) {
			os << "\n\\babelprovide[import";
			if (l == language)
				os << ", main";
			if (!babelLangOptions(l->lang(), true).empty())
				os << ", " << babelLangOptions(l->lang());
			os << "]{" << blang << "}";
			have_mods = true;
		}
		// language that only requires \babelprovide with nonTeXFonts
		if (bp == 2 && useNonTeXFonts) {
			// here we need to tell babel to use the *.ini
			// even though an *.ldf exists.
			// This imports the *ini, so no "import" needed.
			if (l == language) {
				force_provide = force_provide.empty()
						? "provide=*"
						: "provide*=*";
				have_main_forceprovide = true;
			} else
				force_provide = have_main_forceprovide
						? "provide*=*"
						: "provide+=*";
			have_mods = true;
		}
		if ((bp == 2 && useNonTeXFonts) || have_other_forceprovide) {
			// Options need to go to \babeprovide
			// but only those set in document settings
			if (!babelLangOptions(l->lang(), true).empty())
				os << "\n\\babelprovide["
				   << babelLangOptions(l->lang())
				   << "]{" << blang << "}";
		}
		if (bp != 1 && use_opt)
			blangs.push_back(blang);
	}
	if (have_mods) {
		lang_opts = getStringFromVector(blangs);
		if (!force_provide.empty()) {
			if (!lang_opts.empty())
				lang_opts += ", ";
			lang_opts += force_provide;
		}
	}
	if (useNonTeXFonts && features.hasRTLLanguage()) {
		if (!lang_opts.empty())
			lang_opts += ", ";
		if (features.runparams().flavor == Flavor::XeTeX) {
			// main language RTL?
			if (language->rightToLeft())
				lang_opts += "bidi=bidi-r";
			else
				lang_opts += "bidi=bidi-l";
		} else
			lang_opts += "bidi=basic";
	}
	// The prefs may require the languages to
	// be submitted to babel itself (not the class).
	if ((langoptions || have_mods) && !lang_opts.empty())
		return "\\usepackage[" + lang_opts + "]{babel}" + os.str();
	return "\\usepackage{babel}" + os.str();
}


docstring BufferParams::getGraphicsDriver(string const & package) const
{
	docstring result;

	if (package == "geometry") {
		if (graphics_driver == "dvips"
		    || graphics_driver == "dvipdfm"
		    || graphics_driver == "pdftex"
		    || graphics_driver == "vtex")
			result = from_ascii(graphics_driver);
		else if (graphics_driver == "dvipdfmx")
			result = from_ascii("dvipdfm");
	}

	return result;
}


void BufferParams::writeEncodingPreamble(otexstream & os,
					 LaTeXFeatures & features) const
{
	// With no-TeX fonts we use utf8-plain without encoding package.
	if (useNonTeXFonts)
		return;

	string const doc_encoding = encoding().latexName();
	Encoding::Package const package = encoding().package();
	// (dvi)lualatex uses luainputenc rather than inputenc
	string const inputenc_package = 
		(features.runparams().flavor == Flavor::LuaTeX
		 || features.runparams().flavor == Flavor::DviLuaTeX)
		? "luainputenc" : "inputenc";

	if (inputenc == "auto-legacy") {
		// The "japanese" babel language requires the pLaTeX engine
		// which conflicts with "inputenc".
		// See http://www.mail-archive.com/lyx-devel@lists.lyx.org/msg129680.html
		if (!features.isRequired("japanese")
		    && !features.isProvided("inputenc")) {
			if (package == Encoding::inputenc) {
				// Main language requires (lua)inputenc
				os << "\\usepackage[" << doc_encoding << "]{"
				   << inputenc_package << "}\n";
			} else {
				// We might have an additional language that requires inputenc
				set<string> encoding_set = features.getEncodingSet(doc_encoding);
				bool inputenc = false;
				for (auto const & enc : encoding_set) {
					if (encodings.fromLaTeXName(enc)
					    && encodings.fromLaTeXName(enc)->package() == Encoding::inputenc) {
						inputenc = true;
						break;
					}
				}
				if (inputenc)
					// load (lua)inputenc without options
					// (the encoding is loaded later)
					os << "\\usepackage{" << inputenc_package << "}\n";
			}
		}
	} else if (inputenc != "auto-legacy-plain") {
		switch (package) {
		case Encoding::none:
		case Encoding::CJK:
		case Encoding::japanese:
			if (encoding().iconvName() != "UTF-8"
			    && !features.runparams().isFullUnicode()
			    && features.isAvailableAtLeastFrom("LaTeX", 2018, 4))
				// don't default to [utf8]{inputenc} with LaTeX >= 2018/04
				os << "\\UseRawInputEncoding\n";
			break;
		case Encoding::inputenc:
			// do not load inputenc if japanese is used
			// or if the class provides inputenc
			if (features.isRequired("japanese")
			    || features.isProvided("inputenc"))
				break;
			// The 2022 release of ucs.sty uses the default utf8
			// inputenc encoding with 'utf8x' inputenc if the ucs
			// package is not loaded before inputenc.
			// This breaks existing documents that use utf8x
			// and also makes utf8x redundant.
			// Thus we load ucs.sty in order to keep functionality
			// that would otherwise be silently dropped.
			if (doc_encoding == "utf8x"
			    && features.isAvailableAtLeastFrom("ucs", 2022, 8, 7)
			    && !features.isProvided("ucs"))
				os << "\\usepackage{ucs}\n";
			os << "\\usepackage[" << doc_encoding << "]{"
			   << inputenc_package << "}\n";
			break;
		}
	}
	if ((inputenc == "auto-legacy-plain" || features.isRequired("japanese"))
	    && features.isAvailableAtLeastFrom("LaTeX", 2018, 4))
		// don't default to [utf8]{inputenc} with LaTeX >= 2018/04
		os << "\\UseRawInputEncoding\n";
}


string const BufferParams::parseFontName(string const & name) const
{
	string mangled = name;
	size_t const idx = mangled.find('[');
	if (idx == string::npos || idx == 0)
		return mangled;
	else
		return mangled.substr(0, idx - 1);
}


string const BufferParams::loadFonts(LaTeXFeatures & features) const
{
	if (fontsRoman() == "default" && fontsSans() == "default"
	    && fontsTypewriter() == "default"
	    && (fontsMath() == "default" || fontsMath() == "auto"))
		//nothing to do
		return string();

	ostringstream os;

	/* Fontspec (XeTeX, LuaTeX): we provide GUI support for oldstyle
	 * numbers (Numbers=OldStyle) and sf/tt scaling. The Ligatures=TeX/
	 * Mapping=tex-text option assures TeX ligatures (such as "--")
	 * are resolved. Note that tt does not use these ligatures.
	 * TODO:
	 *    -- add more GUI options?
	 *    -- add more fonts (fonts for other scripts)
	 *    -- if there's a way to find out if a font really supports
	 *       OldStyle, enable/disable the widget accordingly.
	*/
	if (useNonTeXFonts && features.isAvailable("fontspec")) {
		// "Mapping=tex-text" and "Ligatures=TeX" are equivalent.
		// However, until v.2 (2010/07/11) fontspec only knew
		// Mapping=tex-text (for XeTeX only); then "Ligatures=TeX"
		// was introduced for both XeTeX and LuaTeX (LuaTeX
		// didn't understand "Mapping=tex-text", while XeTeX
		// understood both. With most recent versions, both
		// variants are understood by both engines. However,
		// we want to provide support for at least TeXLive 2009
		// (for XeTeX; LuaTeX is only supported as of v.2)
		// As of 2017/11/03, Babel has its own higher-level
		// interface on top of fontspec that is to be used.
		bool const babelfonts = features.useBabel()
				&& features.isAvailableAtLeastFrom("babel", 2017, 11, 3);
		string const texmapping =
			(features.runparams().flavor == Flavor::XeTeX) ?
			"Mapping=tex-text" : "Ligatures=TeX";
		if (fontsRoman() != "default") {
			if (babelfonts)
				os << "\\babelfont{rm}[";
			else
				os << "\\setmainfont[";
			os << texmapping;
			if (fonts_roman_osf)
				os << ",Numbers=OldStyle";
			if (!font_roman_opts.empty())
				os << ',' << font_roman_opts;
			os << "]{" << parseFontName(fontsRoman()) << "}\n";
		}
		if (fontsSans() != "default") {
			string const sans = parseFontName(fontsSans());
			if (fontsSansScale() != 100) {
				if (babelfonts)
					os << "\\babelfont{sf}";
				else
					os << "\\setsansfont";
				os << "[Scale="
				   << float(fontsSansScale()) / 100 << ',';
				if (fonts_sans_osf)
					os << "Numbers=OldStyle,";
				os << texmapping;
				if (!font_sans_opts.empty())
					os << ',' << font_sans_opts;
				os << "]{" << sans << "}\n";
			} else {
				if (babelfonts)
					os << "\\babelfont{sf}[";
				else
					os << "\\setsansfont[";
				if (fonts_sans_osf)
					os << "Numbers=OldStyle,";
				os << texmapping;
				if (!font_sans_opts.empty())
					os << ',' << font_sans_opts;
				os << "]{" << sans << "}\n";
			}
		}
		if (fontsTypewriter() != "default") {
			string const mono = parseFontName(fontsTypewriter());
			if (fontsTypewriterScale() != 100) {
				if (babelfonts)
					os << "\\babelfont{tt}";
				else
					os << "\\setmonofont";
				os << "[Scale="
				   << float(fontsTypewriterScale()) / 100;
				if (fonts_typewriter_osf)
					os << ",Numbers=OldStyle";
				if (!font_typewriter_opts.empty())
					os << ',' << font_typewriter_opts;
				os << "]{"
				   << mono << "}\n";
			} else {
				if (babelfonts)
					os << "\\babelfont{tt}";
				else
					os << "\\setmonofont";
				if (!font_typewriter_opts.empty() || fonts_typewriter_osf) {
					os << '[';
					if (fonts_typewriter_osf)
						os << "Numbers=OldStyle";
					if (!font_typewriter_opts.empty()) {
						if (fonts_typewriter_osf)
							os << ',';
						os << font_typewriter_opts;
					}
					os << ']';
				}
				os << '{' << mono << "}\n";
			}
		}
		return os.str();
	}

	// Tex Fonts
	bool const ot1 = (features.runparams().main_fontenc == "default"
			  || features.runparams().main_fontenc == "OT1");
	bool const dryrun = features.runparams().dryrun;
	bool const complete = (fontsSans() == "default" && fontsTypewriter() == "default");
	bool const nomath = (fontsMath() != "auto");

	// ROMAN FONTS
	os << theLaTeXFonts().getLaTeXFont(from_ascii(fontsRoman())).getLaTeXCode(
		dryrun, ot1, complete, fonts_expert_sc, fonts_roman_osf,
		nomath, font_roman_opts);

	// SANS SERIF
	os << theLaTeXFonts().getLaTeXFont(from_ascii(fontsSans())).getLaTeXCode(
		dryrun, ot1, complete, fonts_expert_sc, fonts_sans_osf,
		nomath, font_sans_opts, fontsSansScale());

	// MONOSPACED/TYPEWRITER
	os << theLaTeXFonts().getLaTeXFont(from_ascii(fontsTypewriter())).getLaTeXCode(
		dryrun, ot1, complete, fonts_expert_sc, fonts_typewriter_osf,
		nomath, font_typewriter_opts, fontsTypewriterScale());

	// MATH
	os << theLaTeXFonts().getLaTeXFont(from_ascii(fontsMath())).getLaTeXCode(
		dryrun, ot1, complete, fonts_expert_sc, fonts_roman_osf,
		nomath);

	return os.str();
}


Encoding const & BufferParams::encoding() const
{
	// Main encoding for LaTeX output.
	if (useNonTeXFonts)
		return *(encodings.fromLyXName("utf8-plain"));
	if (inputenc == "auto-legacy" || inputenc == "auto-legacy-plain")
		return *language->encoding();
	if (inputenc == "utf8" && language->lang() == "japanese")
		return *(encodings.fromLyXName("utf8-platex"));
	Encoding const * const enc = encodings.fromLyXName(inputenc);
	if (enc)
		return *enc;
	LYXERR0("Unknown inputenc value `" << inputenc
	       << "'. Using `auto' instead.");
	return *language->encoding();
}


string const & BufferParams::defaultBiblioStyle() const
{
	if (!biblio_style.empty())
		return biblio_style;

	map<string, string> const & bs = documentClass().defaultBiblioStyle();
	auto cit = bs.find(theCiteEnginesList.getTypeAsString(citeEngineType()));
	if (cit != bs.end())
		return cit->second;
	else
		return empty_string();
}


bool BufferParams::fullAuthorList() const
{
	return documentClass().fullAuthorList();
}


string BufferParams::getCiteAlias(string const & s) const
{
	bool realcmd = false;
	vector<CitationStyle> const styles = citeStyles();
	for (size_t i = 0; i != styles.size(); ++i) {
		// only include variants that are supported in the current style
		if (styles[i].name == s && isActiveCiteStyle(styles[i])) {
			realcmd = true;
			break;
		}
	}
	// If it is a real command, don't treat it as an alias
	if (realcmd)
		return string();
	map<string,string> aliases = documentClass().citeCommandAliases();
	if (aliases.find(s) != aliases.end())
		return aliases[s];
	return string();
}


vector<string> BufferParams::citeCommands() const
{
	static CitationStyle const default_style;
	vector<string> commands =
		documentClass().citeCommands(citeEngineType());
	if (commands.empty())
		commands.push_back(default_style.name);
	return commands;
}


vector<CitationStyle> BufferParams::citeStyles() const
{
	static CitationStyle const default_style;
	vector<CitationStyle> styles =
		documentClass().citeStyles(citeEngineType());
	if (styles.empty())
		styles.push_back(default_style);
	return styles;
}


bool BufferParams::isActiveCiteStyle(CitationStyle const & cs) const
{
	if (!useBiblatex())
		// outside biblatex, all cite styles are active
		return true;

	if (cs.styles.empty() && cs.nostyles.empty())
		// no restrictions
		return true;

	// exclude variants that are excluded in the current style
	for (string const & s: cs.nostyles) {
		if (s == biblatex_citestyle)
			// explicitly excluded style
			return false;
	}
	if (cs.styles.empty())
		// not excluded
		return true;

	// only include variants that are supported in the current style
	for (string const & s: cs.styles) {
		if (s == biblatex_citestyle)
			return true;
	}
	return false;
}

string const BufferParams::getBibtexCommand(string const & cmd, bool const warn) const
{
	// split from options
	string command_in;
	split(cmd, command_in, ' ');

	// Look if the requested command is available. If so, use that.
	for (auto const & alts : lyxrc.bibtex_alternatives) {
		string command_prov;
		split(alts, command_prov, ' ');
		if (command_in == command_prov)
			return cmd;
	}

	// If not, find the most suitable fallback for the current cite framework,
	// and warn. Note that we omit options in any such case.
	string fallback;
	if (useBiblatex()) {
		// For Biblatex, we prefer biber (also for Japanese)
		// and try to fall back to bibtex8
		if (lyxrc.bibtex_alternatives.find("biber") != lyxrc.bibtex_alternatives.end())
			fallback = "biber";
		else if (lyxrc.bibtex_alternatives.find("bibtex8") != lyxrc.bibtex_alternatives.end())
			fallback = "bibtex8";
	}
	// For classic BibTeX and as last resort for biblatex, try bibtex
	if (fallback.empty()) {
		if (lyxrc.bibtex_alternatives.find("bibtex") != lyxrc.bibtex_alternatives.end())
			fallback = "bibtex";
	}

	if (!warn)
		return fallback;

	if (fallback.empty()) {
		frontend::Alert::warning(
			_("No bibliography processor found!"),
			support::bformat(
			  _("The bibliography processor requested by this document "
			    "(%1$s) is not available and no appropriate "
			    "alternative has been found. "
			    "No bibliography and references will be generated.\n"
			    "Please fix your installation!"),
			  from_utf8(cmd)));
	} else {
		frontend::Alert::warning(
			_("Requested bibliography processor not found!"),
			support::bformat(
			  _("The bibliography processor requested by this document "
			    "(%1$s) is not available. "
			    "As a fallback, '%2$s' will be used, options are omitted. "
			    "This might result in errors or unwanted changes in "
			    "the bibliography. Please check carefully!\n"
			    "It is suggested to install the missing processor."),
			  from_utf8(cmd), from_utf8(fallback)));
	}
	return fallback;
}


string const BufferParams::bibtexCommand(bool const warn) const
{
	// Return document-specific setting if available
	if (bibtex_command != "default")
		return getBibtexCommand(bibtex_command, warn);

	// If we have "default" in document settings, consult the prefs
	// 1. Japanese (uses a specific processor)
	if (encoding().package() == Encoding::japanese) {
		if (lyxrc.jbibtex_command != "automatic")
			// Return the specified program, if "automatic" is not set
			return lyxrc.jbibtex_command;
		else if (!useBiblatex()) {
			// With classic BibTeX, return pbibtex, jbibtex, bibtex
			if (lyxrc.jbibtex_alternatives.find("pbibtex") != lyxrc.jbibtex_alternatives.end())
				return "pbibtex";
			if (lyxrc.jbibtex_alternatives.find("jbibtex") != lyxrc.jbibtex_alternatives.end())
				return "jbibtex";
			return "bibtex";
		}
	}
	// 2. All other languages
	else if (lyxrc.bibtex_command != "automatic")
		// Return the specified program, if "automatic" is not set
		return getBibtexCommand(lyxrc.bibtex_command, warn);

	// 3. Automatic: find the most suitable for the current cite framework
	if (useBiblatex()) {
		// For Biblatex, we prefer biber (also for Japanese)
		// and fall back to bibtex8 and, as last resort, bibtex
		if (lyxrc.bibtex_alternatives.find("biber") != lyxrc.bibtex_alternatives.end())
			return "biber";
		else if (lyxrc.bibtex_alternatives.find("bibtex8") != lyxrc.bibtex_alternatives.end())
			return "bibtex8";
	}
	return "bibtex";
}


bool BufferParams::useBiblatex() const
{
	return theCiteEnginesList[citeEngine()]->getCiteFramework() == "biblatex";
}


void BufferParams::invalidateConverterCache() const
{
	pimpl_->isExportCacheValid = false;
	pimpl_->isViewCacheValid = false;
}


// We shouldn't need to reset the params here, since anything
// we need will be recopied.
void BufferParams::copyForAdvFR(const BufferParams & bp)
{
	string const & lang = bp.language->lang();
	setLanguage(lang);
	quotes_style = bp.quotes_style;
	layout_modules_ = bp.layout_modules_;
	string const & doc_class = bp.documentClass().name();
	setBaseClass(doc_class);
}


void BufferParams::setBibFileEncoding(string const & file, string const & enc)
{
	bib_encodings[file] = enc;
}


string const BufferParams::bibFileEncoding(string const & file) const
{
	if (bib_encodings.find(file) == bib_encodings.end())
		return string();
	return bib_encodings.find(file)->second;
}


string const BufferParams::babelLangOptions(string const & lang, bool const onlycust) const
{
	if (lang_options_babel_.find(lang) == lang_options_babel_.end()) {
		Language const * l = languages.getLanguage(lang);
		return (l && !onlycust) ? l->babelOpts() : string();
	}
	return lang_options_babel_.find(lang)->second;
}


string const BufferParams::polyglossiaLangOptions(string const & lang) const
{
	if (lang_options_polyglossia_.find(lang) == lang_options_polyglossia_.end()) {
		Language const * l = languages.getLanguage(lang);
		return l ? l->polyglossiaOpts() : string();
	}
	return lang_options_polyglossia_.find(lang)->second;
}


BufferParams const & defaultBufferParams()
{
	static BufferParams default_params;
	return default_params;
}


} // namespace lyx
