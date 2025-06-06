/**
 * \file tex2lyx/text.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 * \author Jean-Marc Lasgouttes
 * \author Uwe Stöhr
 *
 * Full author contact details are available in file CREDITS.
 */

// {[(

#include <config.h>

#include "tex2lyx.h"

#include "Context.h"
#include "Encoding.h"
#include "FloatList.h"
#include "LaTeXPackages.h"
#include "Layout.h"
#include "Preamble.h"

#include "insets/ExternalTemplate.h"

#include "support/lassert.h"
#include "support/convert.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/Length.h"
#include "support/lstrings.h"
#include "support/lyxtime.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

using namespace std;
using namespace lyx::support;

namespace lyx {


namespace {

void output_arguments(ostream &, Parser &, bool, bool, const string &, Context &,
                      Layout::LaTeXArgMap const &);

}


void parse_text_in_inset(Parser & p, ostream & os, unsigned flags, bool outer,
		Context & context, InsetLayout const * layout,
		string const & rdelim, string const & rdelimesc)
{
	bool const forcePlainLayout =
		layout ? layout->forcePlainLayout() : false;
	Context newcontext(true, context.textclass);
	if (forcePlainLayout)
		newcontext.layout = &context.textclass.plainLayout();
	else
		newcontext.font = context.font;
	// Inherit commands to pass through
	newcontext.pass_thru_cmds = context.pass_thru_cmds;
	// and table cell
	newcontext.in_table_cell = context.in_table_cell;
	if (layout)
		output_arguments(os, p, outer, false, string(), newcontext,
		                 layout->latexargs());
	// If we have a latex param, we eat it here.
	if (!context.latexparam.empty()) {
		ostringstream oss;
		Context dummy(true, context.textclass);
		parse_text(p, oss, FLAG_RDELIM, outer, dummy,
			   string(1, context.latexparam.back()));
	}
	parse_text(p, os, flags, outer, newcontext, rdelim, rdelimesc);
	if (layout)
		output_arguments(os, p, outer, false, "post", newcontext,
		                 layout->postcommandargs());
	newcontext.check_end_layout(os);
	context.cell_align = newcontext.cell_align;
}


namespace {

void parse_text_in_inset(Parser & p, ostream & os, unsigned flags, bool outer,
		Context const & context, string const & name,
		string const & rdelim = string(),
		string const & rdelimesc = string())
{
	InsetLayout const * layout = 0;
	DocumentClass::InsetLayouts::const_iterator it =
		context.textclass.insetLayouts().find(from_ascii(name));
	if (it != context.textclass.insetLayouts().end())
		layout = &(it->second);
	Context newcontext = context;
	parse_text_in_inset(p, os, flags, outer, newcontext, layout, rdelim, rdelimesc);
}

/// parses a paragraph snippet, useful for example for \\emph{...}
void parse_text_snippet(Parser & p, ostream & os, unsigned flags, bool outer,
			Context & context, string const & rdelim = string(),
			string const & rdelimesc = string())
{
	Context newcontext(context);
	// Don't inherit the paragraph-level extra stuff
	newcontext.par_extra_stuff.clear();
	parse_text(p, os, flags, outer, newcontext, rdelim, rdelimesc);
	// Make sure that we don't create invalid .lyx files
	context.need_layout = newcontext.need_layout;
	context.need_end_layout = newcontext.need_end_layout;
}


/*!
 * Thin wrapper around parse_text_snippet() using a string.
 *
 * We completely ignore \c context.need_layout and \c context.need_end_layout,
 * because our return value is not used directly (otherwise the stream version
 * of parse_text_snippet() could be used). That means that the caller needs
 * to do layout management manually.
 * This is intended to parse text that does not create any layout changes.
 */
string parse_text_snippet(Parser & p, unsigned flags, const bool outer,
		  Context & context)
{
	Context newcontext(context);
	newcontext.need_layout = false;
	newcontext.need_end_layout = false;
	newcontext.new_layout_allowed = false;
	// Avoid warning by Context::~Context()
	newcontext.par_extra_stuff.clear();
	ostringstream os;
	parse_text_snippet(p, os, flags, outer, newcontext);
	return os.str();
}

string fboxrule = "";
string fboxsep = "";
string shadow_size = "";
string printnomencl_textwidth = "";

char const * const known_babel_shorthands[] = { "\"", "|", "-", "~", "=", "/",
 "~", "*", ":", "_", "x", "'", "`", "<", ">", 0 };

char const * const known_ref_commands[] = { "ref", "pageref", "vref",
 "vpageref", "prettyref", "nameref", "eqref", "cref", "Cref",
 "cpageref", "labelcref", "labelcpageref",
 "vrefrange", "vpagerefrange", "crefrange", "Crefrange", 0 };

char const * const known_coded_ref_commands[] = { "ref", "pageref", "vref",
 "vpageref", "formatted", "nameref", "eqref", "formatted", "formatted",
 "cpageref", "ref", "pageref",
 "vref", "vpageref", "formatted", "formatted", 0 };

char const * const known_starref_commands[] = { "ref", "pageref", "vref",
 "vpageref", "nameref", "eqref", "cref", "Cref", "cpageref", 0 };

char const * const known_refstyle_commands[] = { "algref", "chapref", "corref",
 "eqref", "enuref", "figref", "fnref", "lemref", "parref", "partref", "propref",
 "secref", "subsecref", "tabref", "thmref",
"algrangeref", "chaprangeref", "corrangeref",
 "eqrangeref", "enurangeref", "figrangeref", "fnrangeref", "lemrangeref", "parrangeref", "partrangeref", "proprangeref",
 "secrangeref", "subsecrangeref", "tabrangeref", "thmrangeref", 0 };

char const * const known_refstyle_prefixes[] = { "alg", "chap", "cor",
 "eq", "enu", "fig", "fn", "lem", "par", "part", "prop",
 "sec", "subsec", "tab", "thm",
 "alg", "chap", "cor",
  "eq", "enu", "fig", "fn", "lem", "par", "part", "prop",
  "sec", "subsec", "tab", "thm", 0 };

char const * const known_zref_commands[] = { "zcref", "zcpageref", "zvref",
 "zvpageref", "zvrefrange", "zvpagerefrange", 0 };

char const * const known_coded_zref_commands[] = { "formatted", "cpageref", "vref",
 "vpageref", "vref", "vpageref", 0 };


/**
 * supported CJK encodings
 * JIS does not work with LyX's encoding conversion
 */
const char * const supported_CJK_encodings[] = {
"EUC-JP", "KS", "GB", "UTF8",
"Bg5", /*"JIS",*/ "SJIS", 0};

/**
 * the same as supported_CJK_encodings with their corresponding LyX language name
 * FIXME: The mapping "UTF8" => "chinese-traditional" is only correct for files
 *        created by LyX.
 * NOTE: "Bg5", "JIS" and "SJIS" are not supported by LyX, on re-export the
 *       encodings "UTF8", "EUC-JP" and "EUC-JP" will be used.
 * please keep this in sync with supported_CJK_encodings line by line!
 */
const char * const supported_CJK_languages[] = {
"japanese-cjk", "korean", "chinese-simplified", "chinese-traditional",
"chinese-traditional", /*"japanese-cjk",*/ "japanese-cjk", 0};

/*!
 * natbib commands.
 * The starred forms are also known except for "citefullauthor",
 * "citeyear" and "citeyearpar".
 */
char const * const known_natbib_commands[] = { "cite", "citet", "citep",
"citealt", "citealp", "citeauthor", "citeyear", "citeyearpar",
"citefullauthor", "Citet", "Citep", "Citealt", "Citealp", "Citeauthor", 0 };

/*!
 * jurabib commands.
 * No starred form other than "cite*" known.
 */
char const * const known_jurabib_commands[] = { "cite", "citet", "citep",
"citealt", "citealp", "citeauthor", "citeyear", "citeyearpar",
// jurabib commands not (yet) supported by LyX:
// "fullcite",
// "footcite", "footcitet", "footcitep", "footcitealt", "footcitealp",
// "footciteauthor", "footciteyear", "footciteyearpar",
"citefield", "citetitle", 0 };

/*!
 * biblatex commands.
 */
char const * const known_biblatex_commands[] = { "cite", "Cite", "textcite", "Textcite",
"parencite", "Parencite", "citeauthor", "Citeauthor", "citeyear", "smartcite", "Smartcite",
"footcite", "Footcite", "autocite", "Autocite", "citetitle", "fullcite", "footfullcite",
"supercite", "cites", "Cites", "textcites", "Textcites", "parencites", "Parencites",
"smartcites", "Smartcites", "autocites", "Autocites", 0 };
/*!
 * Known starred biblatex commands.
 */
char const * const known_biblatex_star_commands[] = { "cite", "citeauthor",
"Citeauthor", "parencite", "citetitle", 0 };

/*!
 * biblatex-chicago commands.
 * Known starred forms: \cite*, \citeauthor*, \Citeauthor*, \parencite*, \citetitle*.
 */
char const * const known_biblatex_chicago_commands[] = { "atcite", "atpcite", "Citetitle", "gentextcite",
"Gentextcite", "shortcite", "shortrefcite", "shorthandcite", "shorthandrefcite",
"citejournal", "headlesscite", "Headlesscite", "headlessfullcite", "surnamecite", 0 };

// Whether we need to insert a bibtex inset in a comment
bool need_commentbib = false;

/// LaTeX names for quotes
char const * const known_quotes[] = { "dq", "guillemotleft", "flqq", "og",
"guillemotright", "frqq", "fg", "glq", "glqq", "textquoteleft", "grq", "grqq",
"quotedblbase", "textquotedblleft", "quotesinglbase", "textquoteright", "flq",
"guilsinglleft", "frq", "guilsinglright", "textquotedblright", "textquotesingle",
"textquotedbl", 0};

/// the same as known_quotes with .lyx names
char const * const known_coded_quotes[] = { "qrd", "ard", "ard", "ard",
"ald", "ald", "ald", "gls", "gld", "els", "els", "eld",
"gld", "eld", "gls", "ers", "ars",
"ars", "als", "als", "erd", "qrs", "qrd", 0};

/// LaTeX names for font sizes
char const * const known_sizes[] = { "tiny", "scriptsize", "footnotesize",
"small", "normalsize", "large", "Large", "LARGE", "huge", "Huge", 0};

/// the same as known_sizes with .lyx names
char const * const known_coded_sizes[] = { "tiny", "scriptsize", "footnotesize",
"small", "normal", "large", "larger", "largest", "huge", "giant", 0};

/// LaTeX 2.09 names for font families
char const * const known_old_font_families[] = { "rm", "sf", "tt", 0};

/// LaTeX names for font families
char const * const known_font_families[] = { "rmfamily", "sffamily",
"ttfamily", 0};

/// LaTeX names for font family changing commands
char const * const known_text_font_families[] = { "textrm", "textsf",
"texttt", 0};

/// The same as known_old_font_families, known_font_families and
/// known_text_font_families with .lyx names
char const * const known_coded_font_families[] = { "roman", "sans",
"typewriter", 0};

/// LaTeX 2.09 names for font series
char const * const known_old_font_series[] = { "bf", 0};

/// LaTeX names for font series
char const * const known_font_series[] = { "bfseries", "mdseries", 0};

/// LaTeX names for font series changing commands
char const * const known_text_font_series[] = { "textbf", "textmd", 0};

/// The same as known_old_font_series, known_font_series and
/// known_text_font_series with .lyx names
char const * const known_coded_font_series[] = { "bold", "medium", 0};

/// LaTeX 2.09 names for font shapes
char const * const known_old_font_shapes[] = { "it", "sl", "sc", 0};

/// LaTeX names for font shapes
char const * const known_font_shapes[] = { "itshape", "slshape", "scshape",
"upshape", 0};

/// LaTeX names for font shape changing commands
char const * const known_text_font_shapes[] = { "textit", "textsl", "textsc",
"textup", 0};

/// The same as known_old_font_shapes, known_font_shapes and
/// known_text_font_shapes with .lyx names
char const * const known_coded_font_shapes[] = { "italic", "slanted",
"smallcaps", "up", 0};

/// Known special characters which need skip_spaces_braces() afterwards
char const * const known_special_chars[] = {"ldots",
"lyxarrow", "textcompwordmark",
"slash", "textasciitilde", "textasciicircum", "textbackslash",
"LyX", "TeX", "LaTeXe",
"LaTeX", 0};

/// special characters from known_special_chars which may have a \\protect before
char const * const known_special_protect_chars[] = {"LyX", "TeX",
"LaTeXe", "LaTeX", 0};

/// the same as known_special_chars with .lyx names
char const * const known_coded_special_chars[] = {"\\SpecialChar ldots\n",
"\\SpecialChar menuseparator\n", "\\SpecialChar ligaturebreak\n",
"\\SpecialChar breakableslash\n", "~", "^", "\n\\backslash\n",
"\\SpecialChar LyX\n", "\\SpecialChar TeX\n", "\\SpecialChar LaTeX2e\n",
"\\SpecialChar LaTeX\n", 0};

/*!
 * Graphics file extensions known by the dvips driver of the graphics package.
 * These extensions are used to complete the filename of an included
 * graphics file if it does not contain an extension.
 * The order must be the same that latex uses to find a file, because we
 * will use the first extension that matches.
 * This is only an approximation for the common cases. If we would want to
 * do it right in all cases, we would need to know which graphics driver is
 * used and know the extensions of every driver of the graphics package.
 */
char const * const known_dvips_graphics_formats[] = {"eps", "ps", "eps.gz",
"ps.gz", "eps.Z", "ps.Z", 0};

/*!
 * Graphics file extensions known by the pdftex driver of the graphics package.
 * \sa known_dvips_graphics_formats
 */
char const * const known_pdftex_graphics_formats[] = {"png", "pdf", "jpg",
"mps", "tif", 0};

/*!
 * Known file extensions for TeX files as used by \\include.
 */
char const * const known_tex_extensions[] = {"tex", 0};

/// spaces known by InsetSpace
char const * const known_spaces[] = { " ", "space", 
",", "thinspace",//                                   \\, = \\thinspace
"quad", "qquad", "enspace", "enskip",
";", ">", "medspace",//                               \\; = \\> = \\medspace
":", "thickspace",//                                  \\: = \\thickspace
"!", "negthinspace",//                                \\! = \\negthinspace
"negmedspace", "negthickspace",
"textvisiblespace", "hfill", "dotfill", "hrulefill", "leftarrowfill",
"rightarrowfill", "upbracefill", "downbracefill", 0};

/// the same as known_spaces with .lyx names
char const * const known_coded_spaces[] = { "space{}", "space{}",
"thinspace{}", "thinspace{}",
"quad{}", "qquad{}", "enspace{}", "enskip{}",
"medspace{}", "medspace{}", "medspace{}",
"thickspace{}", "thickspace{}",
"negthinspace{}", "negthinspace{}",
"negmedspace{}", "negthickspace{}",
"textvisiblespace{}", "hfill{}", "dotfill{}", "hrulefill{}", "leftarrowfill{}",
"rightarrowfill{}", "upbracefill{}", "downbracefill{}", 0};

/// known TIPA combining diacritical marks
char const * const known_tipa_marks[] = {"textsubwedge", "textsubumlaut",
"textsubtilde", "textseagull", "textsubbridge", "textinvsubbridge",
"textsubsquare", "textsubrhalfring", "textsublhalfring", "textsubplus",
"textovercross", "textsubarch", "textsuperimposetilde", "textraising",
"textlowering", "textadvancing", "textretracting", "textdoublegrave",
"texthighrise", "textlowrise", "textrisefall", "textsyllabic",
"textsubring", "textsubbar", 0};

/// TIPA tones that need special handling
char const * const known_tones[] = {"15", "51", "45", "12", "454", 0};

// string to store the float type to be able to determine the type of subfloats
string float_type = "";

// string to store the float status of minted listings
string minted_float = "";

// whether a caption has been parsed for a floating minted listing
bool minted_float_has_caption = false;

// The caption for non-floating minted listings
string minted_nonfloat_caption = "";

// Characters that have to be escaped by \\ in LaTeX
char const * const known_escaped_chars[] = {
		"&", "_", "$", "%", "#", "^", "{", "}", 0};


/// splits "x=z, y=b" into a map and an ordered keyword vector
void split_map(string const & s, map<string, string> & res, vector<string> & keys)
{
	vector<string> v;
	split(s, v);
	res.clear();
	keys.resize(v.size());
	for (size_t i = 0; i < v.size(); ++i) {
		size_t const pos   = v[i].find('=');
		string const index = trimSpaceAndEol(v[i].substr(0, pos));
		string const value = trimSpaceAndEol(v[i].substr(pos + 1, string::npos));
		res[index] = value;
		keys[i] = index;
	}
}


/*!
 * Split a LaTeX length into value and unit.
 * The latter can be a real unit like "pt", or a latex length variable
 * like "\textwidth". The unit may contain additional stuff like glue
 * lengths, but we don't care, because such lengths are ERT anyway.
 * \returns true if \p value and \p unit are valid.
 */
bool splitLatexLength(string const & len, string & value, string & unit)
{
	if (len.empty())
		return false;
	const string::size_type i = len.find_first_not_of(" -+0123456789.,");
	//'4,5' is a valid LaTeX length number. Change it to '4.5'
	string const length = subst(len, ',', '.');
	if (i == string::npos)
		return false;
	if (i == 0) {
		if (len[0] == '\\') {
			// We had something like \textwidth without a factor
			value = "1.0";
		} else {
			return false;
		}
	} else {
		value = trimSpaceAndEol(string(length, 0, i));
	}
	if (value == "-")
		value = "-1.0";
	// 'cM' is a valid LaTeX length unit. Change it to 'cm'
	if (contains(len, '\\'))
		unit = trimSpaceAndEol(string(len, i));
	else
		unit = ascii_lowercase(trimSpaceAndEol(string(len, i)));
	return true;
}


/// A simple function to translate a latex length to something LyX can
/// understand. Not perfect, but rather best-effort.
bool translate_len(string const & length, string & valstring, string & unit)
{
	if (!splitLatexLength(length, valstring, unit))
		return false;
	// LyX uses percent values
	double value;
	istringstream iss(valstring);
	iss >> value;
	value *= 100;
	ostringstream oss;
	oss << value;
	string const percentval = oss.str();
	// a normal length
	if (unit.empty() || unit[0] != '\\')
		return true;
	string::size_type const i = unit.find(' ');
	string const endlen = (i == string::npos) ? string() : string(unit, i);
	if (unit == "\\textwidth") {
		valstring = percentval;
		unit = "text%" + endlen;
	} else if (unit == "\\columnwidth") {
		valstring = percentval;
		unit = "col%" + endlen;
	} else if (unit == "\\paperwidth") {
		valstring = percentval;
		unit = "page%" + endlen;
	} else if (unit == "\\linewidth") {
		valstring = percentval;
		unit = "line%" + endlen;
	} else if (unit == "\\paperheight") {
		valstring = percentval;
		unit = "pheight%" + endlen;
	} else if (unit == "\\textheight") {
		valstring = percentval;
		unit = "theight%" + endlen;
	} else if (unit == "\\baselineskip") {
		valstring = percentval;
		unit = "baselineskip%" + endlen;
	}
	return true;
}


/// If we have ambiguous quotation marks, make a smart guess
/// based on main quote style
string guessQuoteStyle(string const & in, bool const opening)
{
	string res = in;
	if (prefixIs(in, "qr")) {// straight quote
		if (!opening)
			res = subst(res, "r", "l");
	} else if (in == "eld") {// ``
		if (preamble.quotesStyle() == "german")
			res = "grd";
		else if (preamble.quotesStyle() == "british")
			res = "bls";
		else if (preamble.quotesStyle() == "french")
			res = "fls";
		else if (preamble.quotesStyle() == "russian")
			res = "rrs";
	} else if (in == "erd") {// ''
		if (preamble.quotesStyle() == "polish")
			res = "prd";
		else if (preamble.quotesStyle() == "british")
			res = "brs";
		else if (preamble.quotesStyle() == "french")
			res = "frs";
		else if (preamble.quotesStyle() == "hungarian")
			res = "hrd";
		else if (preamble.quotesStyle() == "swedish")
			res = opening ? "sld" : "srd";
		else if (preamble.quotesStyle() == "hebrew")
			res = "dld";
	} else if (in == "els") {// `
		if (preamble.quotesStyle() == "german")
			res = "grs";
		else if (preamble.quotesStyle() == "british")
			res = "bld";
	} else if (in == "ers") {// '
		if (preamble.quotesStyle() == "polish")
			res = "prs";
		else if (preamble.quotesStyle() == "british")
			res = "brd";
		else if (preamble.quotesStyle() == "swedish")
			res = opening ? "sls" : "srs";
		else if (preamble.quotesStyle() == "hebrew")
			res = "dls";
	} else if (in == "ard") {// >>
		if (preamble.quotesStyle() == "swiss")
			res = "cld";
		else if (preamble.quotesStyle() == "french")
			res = "fld";
		else if (preamble.quotesStyle() == "russian")
			res = "rld";
		else if (preamble.quotesStyle() == "hungarian")
			res = "hrs";
	} else if (in == "ald") {// <<
		if (preamble.quotesStyle() == "swiss")
			res = "crd";
		else if (preamble.quotesStyle() == "french")
			res = "frd";
		else if (preamble.quotesStyle() == "russian")
			res = "rrd";
		else if (preamble.quotesStyle() == "hungarian")
			res = "hls";
	} else if (in == "ars") {// >
		if (preamble.quotesStyle() == "swiss")
			res = "cls";
	} else if (in == "als") {// <
		if (preamble.quotesStyle() == "swiss")
			res = "crs";
	} else if (in == "gld") {// ,,
		if (preamble.quotesStyle() == "polish")
			res = "pld";
		else if (preamble.quotesStyle() == "hungarian")
			res = "hld";
		else if (preamble.quotesStyle() == "russian")
			res = "rls";
		else if (preamble.quotesStyle() == "hebrew")
			res = "drd";
	} else if (in == "gls") {// ,
		if (preamble.quotesStyle() == "polish")
			res = "pls";
		else if (preamble.quotesStyle() == "hebrew")
			res = "drs";
	}
	return res;
}


string const fromPolyglossiaEnvironment(string const & s)
{
	// Since \arabic is taken by the LaTeX kernel,
	// the Arabic polyglossia environment is upcased
	if (s == "Arabic")
		return "arabic";
	else
		return s;
}


string uncapitalize(string const & s)
{
	docstring in = from_ascii(s);
	char_type t = lowercase(s[0]);
	in[0] = t;
	return to_ascii(in);
}


bool isCapitalized(string const & s)
{
	docstring in = from_ascii(s);
	char_type t = uppercase(s[0]);
	in[0] = t;
	return to_ascii(in) == s;
}


} // namespace


string translate_len(string const & length)
{
	string unit;
	string value;
	if (translate_len(length, value, unit))
		return value + unit;
	// If the input is invalid, return what we have.
	return length;
}

bool is_glue_length(string & length)
{
	// check for glue lengths
	bool is_gluelength = false;
	string gluelength = length;
	string::size_type i = length.find(" minus");
	if (i == string::npos) {
		i = length.find(" plus");
		if (i != string::npos)
			is_gluelength = true;
	} else
		is_gluelength = true;
	// if yes transform "9xx minus 8yy plus 7zz"
	// to "9xx-8yy+7zz"
	if (is_gluelength) {
		i = gluelength.find(" minus");
		if (i != string::npos)
			gluelength.replace(i, 7, "-");
		i = gluelength.find(" plus");
		if (i != string::npos)
			gluelength.replace(i, 6, "+");
		length = gluelength;
	}
	return is_gluelength;
}


namespace {

/*!
 * Translates a LaTeX length into \p value, \p unit and
 * \p special parts suitable for a box inset.
 * The difference from translate_len() is that a box inset knows about
 * some special "units" that are stored in \p special.
 */
void translate_box_len(string const & length, string & value, string & unit, string & special)
{
	if (translate_len(length, value, unit)) {
		if (unit == "\\height" || unit == "\\depth" ||
		    unit == "\\totalheight" || unit == "\\width") {
			special = unit.substr(1);
			// The unit is not used, but LyX requires a dummy setting
			unit = "in";
		} else
			special = "none";
	} else {
		value.clear();
		unit = length;
		special = "none";
	}
}


void begin_inset(ostream & os, string const & name)
{
	os << "\n\\begin_inset " << name;
}


void begin_command_inset(ostream & os, string const & name,
                         string const & latexname)
{
	begin_inset(os, "CommandInset ");
	os << name << "\nLatexCommand " << latexname << '\n';
}


void end_inset(ostream & os)
{
	os << "\n\\end_inset\n\n";
}


bool skip_braces(Parser & p)
{
	if (p.next_token().cat() != catBegin)
		return false;
	p.get_token();
	if (p.next_token().cat() == catEnd) {
		p.get_token();
		return true;
	}
	p.putback();
	return false;
}


/// replace LaTeX commands in \p s from the unicodesymbols file with their
/// unicode points
pair<bool, docstring> convert_unicodesymbols(docstring s)
{
	bool res = true;
	odocstringstream os;
	for (size_t i = 0; i < s.size();) {
		if (s[i] != '\\') {
			os.put(s[i++]);
			continue;
		}
		s = s.substr(i);
		bool termination;
		docstring rem;
		set<string> req;
		docstring parsed = normalize_c(Encodings::fromLaTeXCommand(s,
				Encodings::TEXT_CMD, termination, rem, &req));
		set<string>::const_iterator it = req.begin();
		set<string>::const_iterator en = req.end();
		for (; it != en; ++it)
			preamble.registerAutomaticallyLoadedPackage(*it);
		os << parsed;
		s = rem;
		if (s.empty() || s[0] != '\\')
			i = 0;
		else {
			res = false;
			for (auto const & c : known_escaped_chars)
				if (c != 0 && prefixIs(s, from_ascii("\\") + c))
					res = true;
			i = 1;
		}
	}
	return make_pair(res, os.str());
}


/// try to convert \p s to a valid InsetCommand argument
/// return whether this succeeded. If not, these command insets
/// get the "literate" flag.
pair<bool, string> convert_latexed_command_inset_arg(string s)
{
	bool success = false;
	if (isAscii(s)) {
		// since we don't know the input encoding we can't use from_utf8
		pair<bool, docstring> res = convert_unicodesymbols(from_ascii(s));
		success = res.first;
		s = to_utf8(res.second);
	}
	// LyX cannot handle newlines in a latex command
	return make_pair(success, subst(s, "\n", " "));
}

/// try to convert \p s to a valid InsetCommand argument
/// without trying to recode macros.
string convert_literate_command_inset_arg(string const & s)
{
	// LyX cannot handle newlines in a latex command
	return subst(s, "\n", " ");
}

void output_ert(ostream & os, string const & s, Context & context)
{
	context.check_layout(os);
	for (char const c : s) {
		if (c == '\\')
			os << "\n\\backslash\n";
		else if (c == '\n') {
			context.new_paragraph(os);
			context.check_layout(os);
		} else
			os << c;
	}
	context.check_end_layout(os);
}


void output_ert_inset(ostream & os, string const & s, Context & context)
{
	// We must have a valid layout before outputting the ERT inset.
	context.check_layout(os);
	Context newcontext(true, context.textclass);
	InsetLayout const & layout = context.textclass.insetLayout(from_ascii("ERT"));
	if (layout.forcePlainLayout())
		newcontext.layout = &context.textclass.plainLayout();
	begin_inset(os, "ERT");
	os << "\nstatus collapsed\n";
	output_ert(os, s, newcontext);
	end_inset(os);
}


void output_comment(Parser & p, ostream & os, string const & s,
                    Context & context)
{
	if (p.next_token().cat() == catNewline)
		output_ert_inset(os, '%' + s, context);
	else
		output_ert_inset(os, '%' + s + '\n', context);
}


Layout const * findLayout(TextClass const & textclass, string const & name, bool command,
			  string const & latexparam = string())
{
	Layout const * layout = findLayoutWithoutModule(textclass, name, command, latexparam);
	if (layout)
		return layout;
	if (checkModule(name, command))
		return findLayoutWithoutModule(textclass, name, command, latexparam);
	return layout;
}


InsetLayout const * findInsetLayout(TextClass const & textclass, string const & name, bool command,
				    string const & latexparam = string())
{
	InsetLayout const * insetlayout =
		findInsetLayoutWithoutModule(textclass, name, command, latexparam);
	if (insetlayout)
		return insetlayout;
	if (checkModule(name, command))
		return findInsetLayoutWithoutModule(textclass, name, command, latexparam);
	return insetlayout;
}


void eat_whitespace(Parser &, ostream &, Context &, bool eatParagraph,
		    bool eatNewline = true);


/*!
 * Skips whitespace and braces.
 * This should be called after a command has been parsed that is not put into
 * ERT, and where LyX adds "{}" if needed.
 */
void skip_spaces_braces(Parser & p, bool keepws = false)
{
	/* The following four examples produce the same typeset output and
	   should be handled by this function:
	   - abc \j{} xyz
	   - abc \j {} xyz
	   - abc \j
	     {} xyz
	   - abc \j %comment
	     {} xyz
	 */
	// Unfortunately we need to skip comments, too.
	// We can't use eat_whitespace since writing them after the {}
	// results in different output in some cases.
	bool const skipped_spaces = p.skip_spaces(true);
	bool const skipped_braces = skip_braces(p);
	if (keepws && skipped_spaces && !skipped_braces)
		// put back the space (it is better handled by check_space)
		p.unskip_spaces(true);
}


void output_arguments(ostream & os, Parser & p, bool outer, bool need_layout, string const & prefix,
                      Context & context, Layout::LaTeXArgMap const & latexargs)
{
	if (context.layout->latextype != LATEX_ITEM_ENVIRONMENT || !prefix.empty()) {
		if (need_layout) {
			context.check_layout(os);
			need_layout = false;
		} else
			need_layout = true;
	}
	int i = 0;
	Layout::LaTeXArgMap::const_iterator lait = latexargs.begin();
	Layout::LaTeXArgMap::const_iterator const laend = latexargs.end();
	for (; lait != laend; ++lait) {
		++i;
		eat_whitespace(p, os, context, false);
		if (lait->second.mandatory) {
			if (p.next_token().cat() != catBegin)
				break;
			string ldelim = to_utf8(lait->second.ldelim);
			string rdelim = to_utf8(lait->second.rdelim);
			if (ldelim.empty())
				ldelim = "{";
			if (rdelim.empty())
				rdelim = "}";
			p.get_token(); // eat ldelim
			if (ldelim.size() > 1)
				p.get_token(); // eat ldelim
			if (need_layout) {
				context.check_layout(os);
				need_layout = false;
			}
			begin_inset(os, "Argument ");
			if (!prefix.empty())
				os << prefix << ':';
			os << i << "\nstatus collapsed\n\n";
			parse_text_in_inset(p, os, FLAG_RDELIM, outer, context, 0, rdelim);
			end_inset(os);
		} else {
			string ldelim = to_utf8(lait->second.ldelim);
			string rdelim = to_utf8(lait->second.rdelim);
			if (ldelim.empty())
				ldelim = "[";
			if (rdelim.empty())
				rdelim = "]";
			string tok = p.next_token().asInput();
			// we only support delimiters with max 2 chars for now.
			if (ldelim.size() > 1)
				tok += p.next_next_token().asInput();
			if (p.next_token().cat() == catEscape || tok != ldelim)
				continue;
			p.get_token(); // eat ldelim
			if (ldelim.size() > 1)
				p.get_token(); // eat ldelim
			if (need_layout) {
				context.check_layout(os);
				need_layout = false;
			}
			begin_inset(os, "Argument ");
			if (!prefix.empty())
				os << prefix << ':';
			os << i << "\nstatus collapsed\n\n";
			parse_text_in_inset(p, os, FLAG_RDELIM, outer, context, 0, rdelim);
			end_inset(os);
		}
		eat_whitespace(p, os, context, false);
	}
}


void output_command_layout(ostream & os, Parser & p, bool outer,
			   Context & parent_context,
			   Layout const * newlayout)
{
	TeXFont const oldFont = parent_context.font;
	// save the current font size
	string const size = oldFont.size;
	// reset the font size to default, because the font size switches
	// don't affect section headings and the like
	parent_context.font.size = Context::normalfont.size;
	// we only need to write the font change if we have an open layout
	if (!parent_context.atParagraphStart())
		output_font_change(os, oldFont, parent_context.font);
	parent_context.check_end_layout(os);
	Context context(true, parent_context.textclass, newlayout,
			parent_context.layout, parent_context.font);
	if (parent_context.deeper_paragraph) {
		// We are beginning a nested environment after a
		// deeper paragraph inside the outer list environment.
		// Therefore we don't need to output a "begin deeper".
		context.need_end_deeper = true;
	}
	context.check_deeper(os);
	output_arguments(os, p, outer, true, string(), context,
	                 context.layout->latexargs());
	// If we have a latex param, we eat it here.
	if (!parent_context.latexparam.empty()) {
		ostringstream oss;
		Context dummy(true, parent_context.textclass);
		parse_text(p, oss, FLAG_RDELIM, outer, dummy,
			   string(1, parent_context.latexparam.back()));
	}
	parse_text(p, os, FLAG_ITEM, outer, context);
	output_arguments(os, p, outer, false, "post", context,
	                 context.layout->postcommandargs());
	context.check_end_layout(os);
	if (parent_context.deeper_paragraph) {
		// We must suppress the "end deeper" because we
		// suppressed the "begin deeper" above.
		context.need_end_deeper = false;
	}
	context.check_end_deeper(os);
	// We don't need really a new paragraph, but
	// we must make sure that the next item gets a \begin_layout.
	parent_context.new_paragraph(os);
	// Set the font size to the original value. No need to output it here
	// (Context::begin_layout() will do that if needed)
	parent_context.font.size = size;
}


/*!
 * Output a space if necessary.
 * This function gets called for every whitespace token.
 *
 * We have three cases here:
 * 1. A space must be suppressed. Example: The lyxcode case below
 * 2. A space may be suppressed. Example: Spaces before "\par"
 * 3. A space must not be suppressed. Example: A space between two words
 *
 * We currently handle only 1. and 3 and from 2. only the case of
 * spaces before newlines as a side effect.
 *
 * 2. could be used to suppress as many spaces as possible. This has two effects:
 * - Reimporting LyX generated LaTeX files changes almost no whitespace
 * - Superfluous whitespace from non LyX generated LaTeX files is removed.
 * The drawback is that the logic inside the function becomes
 * complicated, and that is the reason why it is not implemented.
 */
void check_space(Parser & p, ostream & os, Context & context)
{
	Token const next = p.next_token();
	Token const curr = p.curr_token();
	// A space before a single newline and vice versa must be ignored
	// LyX emits a newline before \end{lyxcode}.
	// This newline must be ignored,
	// otherwise LyX will add an additional protected space.
	if (next.cat() == catSpace ||
	    next.cat() == catNewline ||
	    (next.cs() == "end" && context.layout->free_spacing && curr.cat() == catNewline)) {
		return;
	}
	context.check_layout(os);
	os << ' ';
}


/*!
 * Parse all arguments of \p command
 */
void parse_arguments(string const & command,
		     vector<ArgumentType> const & template_arguments,
		     Parser & p, ostream & os, bool outer, Context & context)
{
	string ert = command;
	size_t no_arguments = template_arguments.size();
	for (size_t i = 0; i < no_arguments; ++i) {
		switch (template_arguments[i]) {
		case required:
		case req_group:
			// This argument contains regular LaTeX
			output_ert_inset(os, ert + '{', context);
			eat_whitespace(p, os, context, false);
			if (template_arguments[i] == required)
				parse_text(p, os, FLAG_ITEM, outer, context);
			else
				parse_text_snippet(p, os, FLAG_ITEM, outer, context);
			ert = "}";
			break;
		case item:
			// This argument consists only of a single item.
			// The presence of '{' or not must be preserved.
			p.skip_spaces();
			if (p.next_token().cat() == catBegin)
				ert += '{' + p.verbatim_item() + '}';
			else
				ert += p.verbatim_item();
			break;
		case displaymath:
		case verbatim:
			// This argument may contain special characters
			ert += '{' + p.verbatim_item() + '}';
			break;
		case optional:
		case opt_group:
			// true because we must not eat whitespace
			// if an optional arg follows we must not strip the
			// brackets from this one
			if (i < no_arguments - 1 &&
			    template_arguments[i+1] == optional)
				ert += p.getFullOpt(true);
			else
				ert += p.getOpt(true);
			break;
		}
	}
	output_ert_inset(os, ert, context);
}


/*!
 * Check whether \p command is a known command. If yes,
 * handle the command with all arguments.
 * \return true if the command was parsed, false otherwise.
 */
bool parse_command(string const & command, Parser & p, ostream & os,
		   bool outer, Context & context)
{
	if (known_commands.find(command) != known_commands.end()) {
		parse_arguments(command, known_commands[command], p, os,
				outer, context);
		return true;
	}
	return false;
}


/// Parses a minipage or parbox
void parse_box(Parser & p, ostream & os, unsigned outer_flags,
               unsigned inner_flags, bool outer, Context & parent_context,
               string const & outer_type, string const & special,
               string inner_type, string const & frame_color,
               string const & background_color)
{
	string position;
	string inner_pos;
	string hor_pos = "l";
	// We need to set the height to the LaTeX default of 1\\totalheight
	// for the case when no height argument is given
	string height_value = "1";
	string height_unit = "in";
	string height_special = "totalheight";
	string latex_height;
	string width_value;
	string width_unit;
	string latex_width;
	string width_special = "none";
	string thickness = "0.4pt";
	if (!fboxrule.empty())
		thickness = fboxrule;
	else
		thickness = "0.4pt";
	string separation;
	if (!fboxsep.empty())
		separation = fboxsep;
	else
		separation = "3pt";
	string shadowsize;
	if (!shadow_size.empty())
		shadowsize = shadow_size;
	else
		shadowsize = "4pt";

	string framecolor = "black";
	string backgroundcolor = "none";
	if (!frame_color.empty())
		// check the case that a standard color is used
		framecolor = preamble.getLyXColor(frame_color, true);
	if (!background_color.empty())
		// check the case that a standard color is used
		backgroundcolor = preamble.getLyXColor(background_color, true);

	// if there is a color box around the \begin statements have not yet been parsed
	// so do this now
	if (!frame_color.empty() || !background_color.empty()) {
		eat_whitespace(p, os, parent_context, false);
		p.get_token().asInput(); // the '{'
		// parse minipage
		if (p.next_token().asInput() == "\\begin") {
			p.get_token().asInput();
			p.getArg('{', '}');
			inner_type = "minipage";
			inner_flags = FLAG_END;
			active_environments.push_back("minipage");
		}
		// parse parbox
		else if (p.next_token().asInput() == "\\parbox") {
			p.get_token().asInput();
			inner_type = "parbox";
			inner_flags = FLAG_ITEM;
		}
		// parse makebox
		else if (p.next_token().asInput() == "\\makebox") {
			p.get_token().asInput();
			inner_type = "makebox";
			inner_flags = FLAG_ITEM;
		}
		// in case there is just \colorbox{color}{text}
		else {
			latex_width = "";
			inner_type = "makebox";
			inner_flags = FLAG_BRACE_LAST;
			position = "t";
			inner_pos = "t";
		}
	}
	if (!p.hasOpt() && (inner_type == "makebox" || outer_type == "mbox"))
		hor_pos = "c";
	if (!inner_type.empty() && p.hasOpt()) {
		if (inner_type != "makebox")
			position = p.getArg('[', ']');
		else {
			latex_width = p.getArg('[', ']');
			translate_box_len(latex_width, width_value, width_unit, width_special);
			position = "t";
		}
		if (position != "t" && position != "c" && position != "b") {
			warning_message("invalid position " + position
					+ " for " + inner_type);
			position = "c";
		}
		if (p.hasOpt()) {
			if (inner_type != "makebox") {
				latex_height = p.getArg('[', ']');
				translate_box_len(latex_height, height_value, height_unit, height_special);
			} else {
				string const opt = p.getArg('[', ']');
				if (!opt.empty()) {
					hor_pos = opt;
					if (hor_pos != "l" && hor_pos != "c" &&
					    hor_pos != "r" && hor_pos != "s") {
						warning_message("invalid hor_pos "
								+ hor_pos + " for "
								+ inner_type);
						hor_pos = "c";
					}
				}
			}

			if (p.hasOpt()) {
				inner_pos = p.getArg('[', ']');
				if (inner_pos != "c" && inner_pos != "t" &&
				    inner_pos != "b" && inner_pos != "s") {
					warning_message("invalid inner_pos "
							+ inner_pos + " for "
							+ inner_type);
					inner_pos = position;
				}
			}
		} else {
			if (inner_type == "makebox")
				hor_pos = "c";
		}
	}
	if (inner_type.empty()) {
		if (special.empty() && outer_type != "framebox")
			latex_width = "1\\columnwidth";
		else {
			Parser p2(special);
			latex_width = p2.getArg('[', ']');
			string const opt = p2.getArg('[', ']');
			if (!opt.empty()) {
				hor_pos = opt;
				if (hor_pos != "l" && hor_pos != "c" &&
				    hor_pos != "r" && hor_pos != "s") {
					warning_message("invalid hor_pos "
							+ hor_pos + " for "
							+ outer_type);
					hor_pos = "c";
				}
			} else {
				if (outer_type == "framebox")
					hor_pos = "c";
			}
		}
	} else if (inner_type != "makebox")
		latex_width = p.verbatim_item();
	// if e.g. only \ovalbox{content} was used, set the width to 1\columnwidth
	// as this is LyX's standard for such cases (except for makebox)
	// \framebox is more special and handled below
	if (latex_width.empty() && inner_type != "makebox"
		&& outer_type != "framebox")
		latex_width = "1\\columnwidth";

	translate_len(latex_width, width_value, width_unit);

	bool shadedparbox = false;
	if (inner_type == "shaded") {
		eat_whitespace(p, os, parent_context, false);
		if (outer_type == "parbox") {
			// Eat '{'
			if (p.next_token().cat() == catBegin)
				p.get_token();
			eat_whitespace(p, os, parent_context, false);
			shadedparbox = true;
		}
		p.get_token();
		p.getArg('{', '}');
	}
	// If we already read the inner box we have to push the inner env
	if (!outer_type.empty() && !inner_type.empty() &&
	    (inner_flags & FLAG_END))
		active_environments.push_back(inner_type);
	bool use_ert = false;
	if (!outer_type.empty() && !inner_type.empty()) {
		// Look whether there is some content after the end of the
		// inner box, but before the end of the outer box.
		// If yes, we need to output ERT.
		p.pushPosition();
		if (inner_flags & FLAG_END)
			p.ertEnvironment(inner_type);
		else
			p.verbatim_item();
		p.skip_spaces(true);
		bool const outer_env(outer_type == "framed" || outer_type == "minipage");
		if ((outer_env && p.next_token().asInput() != "\\end") ||
		    (!outer_env && p.next_token().cat() != catEnd)) {
			// something is between the end of the inner box and
			// the end of the outer box, so we need to use ERT.
			use_ert = true;
		}
		p.popPosition();
	}

	if (use_ert) {
		ostringstream ss;
		if (!outer_type.empty()) {
			if (outer_flags & FLAG_END)
				ss << "\\begin{" << outer_type << '}';
			else {
				ss << '\\' << outer_type << '{';
				if (!special.empty())
					ss << special;
			}
		}
		if (!inner_type.empty()) {
			if (inner_type != "shaded") {
				if (inner_flags & FLAG_END)
					ss << "\\begin{" << inner_type << '}';
				else
					ss << '\\' << inner_type;
			}
			if (!position.empty())
				ss << '[' << position << ']';
			if (!latex_height.empty())
				ss << '[' << latex_height << ']';
			if (!inner_pos.empty())
				ss << '[' << inner_pos << ']';
			ss << '{' << latex_width << '}';
			if (!(inner_flags & FLAG_END))
				ss << '{';
		}
		if (inner_type == "shaded")
			ss << "\\begin{shaded}";
		output_ert_inset(os, ss.str(), parent_context);
		if (!inner_type.empty()) {
			parse_text(p, os, inner_flags, outer, parent_context);
			if (inner_flags & FLAG_END)
				output_ert_inset(os, "\\end{" + inner_type + '}',
				           parent_context);
			else
				output_ert_inset(os, "}", parent_context);
		}
		if (!outer_type.empty()) {
			// If we already read the inner box we have to pop
			// the inner env
			if (!inner_type.empty() && (inner_flags & FLAG_END))
				active_environments.pop_back();

			// Ensure that the end of the outer box is parsed correctly:
			// The opening brace has been eaten by parse_outer_box()
			if (!outer_type.empty() && (outer_flags & FLAG_ITEM)) {
				outer_flags &= ~FLAG_ITEM;
				outer_flags |= FLAG_BRACE_LAST;
			}
			parse_text(p, os, outer_flags, outer, parent_context);
			if (outer_flags & FLAG_END)
				output_ert_inset(os, "\\end{" + outer_type + '}',
				           parent_context);
			else
				output_ert_inset(os, "}", parent_context);
		}
	} else {
		// LyX does not like empty positions, so we have
		// to set them to the LaTeX default values here.
		if (position.empty())
			position = "c";
		if (inner_pos.empty())
			inner_pos = position;
		parent_context.check_layout(os);
		begin_inset(os, "Box ");
		if (outer_type == "framed")
			os << "Framed\n";
		else if (outer_type == "framebox" || outer_type == "fbox" || !frame_color.empty())
			os << "Boxed\n";
		else if (outer_type == "shadowbox")
			os << "Shadowbox\n";
		else if ((outer_type == "shaded" && inner_type.empty()) ||
			     (outer_type == "minipage" && inner_type == "shaded") ||
			     (outer_type == "parbox" && inner_type == "shaded")) {
			os << "Shaded\n";
			preamble.registerAutomaticallyLoadedPackage("color");
		} else if (outer_type == "doublebox")
			os << "Doublebox\n";
		else if (outer_type.empty() || outer_type == "mbox")
			os << "Frameless\n";
		else
			os << outer_type << '\n';
		os << "position \"" << position << "\"\n";
		os << "hor_pos \"" << hor_pos << "\"\n";
		if (outer_type == "mbox")
			os << "has_inner_box 1\n";
		else if (!frame_color.empty() && inner_type == "makebox")
			os << "has_inner_box 0\n";
		else
			os << "has_inner_box " << !inner_type.empty() << "\n";
		os << "inner_pos \"" << inner_pos << "\"\n";
		os << "use_parbox " << (inner_type == "parbox" || shadedparbox)
		   << '\n';
		if (outer_type == "mbox")
			os << "use_makebox 1\n";
		else if (!frame_color.empty())
			os << "use_makebox 0\n";
		else
			os << "use_makebox " << (inner_type == "makebox") << '\n';
		if (outer_type == "mbox" || (outer_type == "fbox" && inner_type.empty()))
			os << "width \"\"\n";
		// for values like "1.5\width" LyX uses "1.5in" as width and sets "width" as special
		else if (contains(width_unit, '\\'))
			os << "width \"" << width_value << "in" << "\"\n";
		else
			os << "width \"" << width_value << width_unit << "\"\n";
		if (contains(width_unit, '\\')) {
			width_unit.erase (0,1); // remove the leading '\'
			os << "special \"" << width_unit << "\"\n";
		} else
			os << "special \"" << width_special << "\"\n";
		if (contains(height_unit, '\\'))
			os << "height \"" << height_value << "in" << "\"\n";
		else
			os << "height \"" << height_value << height_unit << "\"\n";
		os << "height_special \"" << height_special << "\"\n";
		os << "thickness \"" << thickness << "\"\n";
		os << "separation \"" << separation << "\"\n";
		os << "shadowsize \"" << shadowsize << "\"\n";
		os << "framecolor \"" << framecolor << "\"\n";
		os << "backgroundcolor \"" << backgroundcolor << "\"\n";
		os << "status open\n\n";

		// Unfortunately we can't use parse_text_in_inset:
		// InsetBox::forcePlainLayout() is hard coded and does not
		// use the inset layout. Apart from that do we call parse_text
		// up to two times, but need only one check_end_layout.
		bool const forcePlainLayout =
			(!inner_type.empty() || inner_type == "makebox") &&
			outer_type != "shaded" && outer_type != "framed";
		Context context(true, parent_context.textclass);
		if (forcePlainLayout)
			context.layout = &context.textclass.plainLayout();
		else
			context.font = parent_context.font;

		// If we have no inner box the contents will be read with the outer box
		if (!inner_type.empty())
			parse_text(p, os, inner_flags, outer, context);

		// Ensure that the end of the outer box is parsed correctly:
		// The opening brace has been eaten by parse_outer_box()
		if (!outer_type.empty() && (outer_flags & FLAG_ITEM)) {
			outer_flags &= ~FLAG_ITEM;
			outer_flags |= FLAG_BRACE_LAST;
		}

		// Find end of outer box, output contents if inner_type is
		// empty and output possible comments
		if (!outer_type.empty()) {
			// If we already read the inner box we have to pop
			// the inner env
			if (!inner_type.empty() && (inner_flags & FLAG_END))
				active_environments.pop_back();
			// This does not output anything but comments if
			// inner_type is not empty (see use_ert)
			parse_text(p, os, outer_flags, outer, context);
		}

		context.check_end_layout(os);
		end_inset(os);
#ifdef PRESERVE_LAYOUT
		// LyX puts a % after the end of the minipage
		if (p.next_token().cat() == catNewline && p.next_token().cs().size() > 1) {
			// new paragraph
			//output_comment(p, os, "dummy", parent_context);
			p.get_token();
			p.skip_spaces();
			parent_context.new_paragraph(os);
		}
		else if (p.next_token().cat() == catSpace || p.next_token().cat() == catNewline) {
			//output_comment(p, os, "dummy", parent_context);
			p.get_token();
			p.skip_spaces();
			// We add a protected space if something real follows
			if (p.good() && p.next_token().cat() != catComment) {
				begin_inset(os, "space ~\n");
				end_inset(os);
			}
		}
#endif
	}
	if (inner_type == "minipage" && (!frame_color.empty() || !background_color.empty()))
		active_environments.pop_back();
	if (inner_flags != FLAG_BRACE_LAST && (!frame_color.empty() || !background_color.empty())) {
		// in this case we have to eat the the closing brace of the color box
		p.get_token().asInput(); // the '}'
	}
	if (p.next_token().asInput() == "}") {
		// in this case we assume that the closing brace is from the box settings
		// therefore reset these values for the next box
		fboxrule = "";
		fboxsep = "";
		shadow_size = "";
	}

	// all boxes except of Frameless and Shaded require calc
	if (!(outer_type.empty() || outer_type == "mbox") &&
		!((outer_type == "shaded" && inner_type.empty()) ||
			     (outer_type == "minipage" && inner_type == "shaded") ||
			     (outer_type == "parbox" && inner_type == "shaded")))
		preamble.registerAutomaticallyLoadedPackage("calc");
}


void parse_outer_box(Parser & p, ostream & os, unsigned flags, bool outer,
                     Context & parent_context, string const & outer_type,
                     string const & special)
{
	eat_whitespace(p, os, parent_context, false);
	if (flags & FLAG_ITEM) {
		// Eat '{'
		if (p.next_token().cat() == catBegin)
			p.get_token();
		else
			warning_message("Ignoring missing '{' after \\"
					+ outer_type + '.');
		eat_whitespace(p, os, parent_context, false);
	}
	string inner;
	unsigned int inner_flags = 0;
	p.pushPosition();
	if (outer_type == "minipage" || outer_type == "parbox") {
		p.skip_spaces(true);
		while (p.hasOpt()) {
			p.getArg('[', ']');
			p.skip_spaces(true);
		}
		p.getArg('{', '}');
		p.skip_spaces(true);
		if (outer_type == "parbox") {
			// Eat '{'
			if (p.next_token().cat() == catBegin)
				p.get_token();
			p.skip_spaces(true);
		}
	}
	if (outer_type == "shaded" || outer_type == "mbox") {
		// These boxes never have an inner box
		;
	} else if (p.next_token().asInput() == "\\parbox") {
		inner = p.get_token().cs();
		inner_flags = FLAG_ITEM;
	} else if (p.next_token().asInput() == "\\begin") {
		// Is this a minipage or shaded box?
		p.pushPosition();
		p.get_token();
		inner = p.getArg('{', '}');
		p.popPosition();
		if (inner == "minipage" || inner == "shaded")
			inner_flags = FLAG_END;
		else
			inner = "";
	}
	p.popPosition();
	if (inner_flags == FLAG_END) {
		if (inner != "shaded")
		{
			p.get_token();
			p.getArg('{', '}');
			eat_whitespace(p, os, parent_context, false);
		}
		parse_box(p, os, flags, FLAG_END, outer, parent_context,
		          outer_type, special, inner, "", "");
	} else {
		if (inner_flags == FLAG_ITEM) {
			p.get_token();
			eat_whitespace(p, os, parent_context, false);
		}
		parse_box(p, os, flags, inner_flags, outer, parent_context,
		          outer_type, special, inner, "", "");
	}
}


void parse_index_entry(Parser & p, ostream & os, Context & context, string const & kind)
{
	// write inset header
	begin_inset(os, "Index ");
	os << kind;

	// Parse for post argument (|...)
	p.pushPosition();
	string const marg = p.getArg('{', '}');
	p.popPosition();
	char lc = char();
	bool inpost = false;
	bool startrange = false;
	bool endrange = false;
	string post;
	for (string::const_iterator it = marg.begin(), et = marg.end(); it != et; ++it) {
		char c = *it;
		if (inpost) {
			if (post.empty() && c == '(')
				startrange = true;
			else if (post.empty() && c == ')')
				endrange = true;
			else
				post += c;
		}
		if (!inpost && (c == '|' && lc != '"'))
			inpost = true;
		lc = c;
	}
	if (startrange)
		os << "\nrange start";
	else if (endrange)
		os << "\nrange end";
	else
		os << "\nrange none";
	bool const see = prefixIs(post, "see{");
	bool const seealso = prefixIs(post, "seealso{");
	if (!post.empty() && !see && !seealso)
		os << "\npageformat " << post;
	else
		os << "\npageformat default";
	os << "\nstatus collapsed\n";

	bool main = true;
	// save position
	p.pushPosition();
	// Check for levels
	if (p.hasIdxMacros("!")) {
		// Index entry with levels
		while (p.hasIdxMacros("!")) {
			if (main) {
				// swallow brace
				p.get_token();
				os << "\\begin_layout Plain Layout\n";
			} else {
				begin_inset(os, "IndexMacro subentry");
				os << "\nstatus collapsed\n";
			}
			// Check for (level-specific) sortkey
			if (p.hasIdxMacros("@", "!")) {
				if (!main)
					os << "\\begin_layout Plain Layout\n";
				begin_inset(os, "IndexMacro sortkey");
				os << "\nstatus collapsed\n";
				parse_text_in_inset(p, os, FLAG_RDELIM, false, context, "IndexMacro sortkey", "@", "\"");
				end_inset(os);
			}
			parse_text_snippet(p, os, FLAG_RDELIM, false, context, "!", "\"");
			if (!main) {
				os << "\n\\end_layout\n";
				end_inset(os);
			}
			main = false;
		}
		if (!main) {
			begin_inset(os, "IndexMacro subentry");
			os << "\nstatus collapsed\n";
		}
		// Final level
		// Check for (level-specific) sortkey
		if (p.hasIdxMacros("@", "!")) {
			if (main) {
				// swallow brace
				p.get_token();
			}
			os << "\\begin_layout Plain Layout\n";
			begin_inset(os, "IndexMacro sortkey");
			os << "\nstatus collapsed\n";
			parse_text_in_inset(p, os, FLAG_RDELIM, false, context, "IndexMacro sortkey", "@", "\"");
			end_inset(os);
			if (post.empty() && !startrange && !endrange) {
				parse_text_snippet(p, os, FLAG_BRACE_LAST, false, context);
				p.dropPosition();
			} else {
				// Handle post-argument
				parse_text_snippet(p, os, FLAG_RDELIM, false, context, "|", "\"");
				if (see || seealso) {
					while (p.next_token().character() != '{' && p.good())
						p.get_token();
					// this ends the subinset, as the see[also] insets
					// must come at main index inset
					os << "\n\\end_layout\n";
					end_inset(os);
					if (see)
						begin_inset(os, "IndexMacro see");
					else
						begin_inset(os, "IndexMacro seealso");
					os << "\nstatus collapsed\n";
					os << "\\begin_layout Plain Layout\n";
					parse_text_snippet(p, os, FLAG_ITEM, false, context);
				}
				p.popPosition();
				// swallow argument
				p.getArg('{', '}');
			}
			os << "\n\\end_layout\n";
		} else {
			if (post.empty() && !startrange && !endrange) {
				parse_text_in_inset(p, os, FLAG_BRACE_LAST, false, context, "IndexMacro subentry");
				p.dropPosition();
			} else {
				// Handle post-argument
				if (see || seealso) {
					os << "\\begin_layout Plain Layout\n";
					parse_text_snippet(p, os, FLAG_RDELIM, false, context, "|", "\"");
					while (p.next_token().character() != '{' && p.good())
						p.get_token();
					// this ends the subinset, as the see[also] insets
					// must come at main index inset
					os << "\n\\end_layout\n";
					end_inset(os);
					if (see)
						begin_inset(os, "IndexMacro see");
					else
						begin_inset(os, "IndexMacro seealso");
					os << "\nstatus collapsed\n";
					parse_text_in_inset(p, os, FLAG_ITEM, false, context, "IndexMacro see");
				} else
					parse_text_in_inset(p, os, FLAG_RDELIM, false, context, "Index", "|", "\"");
				p.popPosition();
				// swallow argument
				p.getArg('{', '}');
			}
		}
		if (!main)
			end_inset(os);
		os << "\n\\end_layout\n";
	} else {
		// Index without any levels
		// Check for sortkey
		if (p.hasIdxMacros("@", "!")) {
			// swallow brace
			p.get_token();
			os << "\\begin_layout Plain Layout\n";
			begin_inset(os, "IndexMacro sortkey");
			os << "\nstatus collapsed\n";
			parse_text_in_inset(p, os, FLAG_RDELIM, false, context, "IndexMacro sortkey", "@", "\"");
			end_inset(os);
			if (post.empty() && !startrange && !endrange) {
				parse_text_snippet(p, os, FLAG_BRACE_LAST, false, context);
				p.dropPosition();
			} else {
				parse_text_snippet(p, os, FLAG_RDELIM, false, context, "|", "\"");
				if (see || seealso) {
					while (p.next_token().character() != '{' && p.good())
						p.get_token();
					if (see)
						begin_inset(os, "IndexMacro see");
					else
						begin_inset(os, "IndexMacro seealso");
					os << "\nstatus collapsed\n";
					parse_text_in_inset(p, os, FLAG_ITEM, false, context, "IndexMacro see");
					end_inset(os);
				}
				p.popPosition();
				// swallow argument
				p.getArg('{', '}');
			}
			os << "\n\\end_layout\n";
		} else {
			if (post.empty() && !startrange && !endrange) {
				parse_text_in_inset(p, os, FLAG_ITEM, false, context, "Index");
				p.dropPosition();
			} else {
				// Handle post-argument
				// swallow brace
				p.get_token();
				if (see || seealso) {
					os << "\\begin_layout Plain Layout\n";
					parse_text_snippet(p, os, FLAG_RDELIM, false, context, "|", "\"");
					while (p.next_token().character() != '{' && p.good())
						p.get_token();
					if (see)
						begin_inset(os, "IndexMacro see");
					else
						begin_inset(os, "IndexMacro seealso");
					os << "\nstatus collapsed\n";
					parse_text_in_inset(p, os, FLAG_ITEM, false, context, "IndexMacro see");
					end_inset(os);
					os << "\n\\end_layout\n";
				} else
					parse_text_in_inset(p, os, FLAG_RDELIM, false, context, "Index", "|", "\"");
				p.popPosition();
				// swallow argument
				p.getArg('{', '}');
			}
		}
	}
	end_inset(os);
}


void parse_listings(Parser & p, ostream & os, Context & parent_context,
		    bool in_line, bool use_minted)
{
	parent_context.check_layout(os);
	begin_inset(os, "listings\n");
	string arg = p.hasOpt() ? subst(p.verbatimOption(), "\n", "") : string();
	size_t i;
	while ((i = arg.find(", ")) != string::npos
			|| (i = arg.find(",\t")) != string::npos)
		arg.erase(i + 1, 1);

	if (use_minted) {
		string const language = p.getArg('{', '}');
		p.skip_spaces(true);
		arg += string(arg.empty() ? "" : ",") + "language=" + language;
		if (!minted_float.empty()) {
			arg += string(arg.empty() ? "" : ",") + minted_float;
			minted_nonfloat_caption.clear();
		}
	}
	if (!arg.empty()) {
		os << "lstparams " << '"' << arg << '"' << '\n';
		if (arg.find("\\color") != string::npos)
	                preamble.registerAutomaticallyLoadedPackage("color");
	}
	if (in_line)
		os << "inline true\n";
	else
		os << "inline false\n";
	os << "status open\n";
	Context context(true, parent_context.textclass);
	context.layout = &parent_context.textclass.plainLayout();
	if (use_minted && prefixIs(minted_nonfloat_caption, "[t]")) {
		minted_nonfloat_caption.erase(0,3);
		os << "\n\\begin_layout Plain Layout\n";
		begin_inset(os, "Caption Standard\n");
		Context newcontext(true, context.textclass,
				   context.layout, 0, context.font);
		newcontext.check_layout(os);
		os << minted_nonfloat_caption << "\n";
		newcontext.check_end_layout(os);
		end_inset(os);
		os << "\n\\end_layout\n";
		minted_nonfloat_caption.clear();
	}
	string s;
	if (in_line) {
		// set catcodes to verbatim early, just in case.
		p.setCatcodes(VERBATIM_CATCODES);
		string delim = p.get_token().asInput();
		//FIXME: handler error condition
		s = p.verbatimStuff(delim).second;
//		context.new_paragraph(os);
	} else if (use_minted) {
		s = p.verbatimEnvironment("minted");
	} else {
		s = p.verbatimEnvironment("lstlisting");
	}
	output_ert(os, s, context);
	if (use_minted && prefixIs(minted_nonfloat_caption, "[b]")) {
		minted_nonfloat_caption.erase(0,3);
		os << "\n\\begin_layout Plain Layout\n";
		begin_inset(os, "Caption Standard\n");
		Context newcontext(true, context.textclass,
				   context.layout, 0, context.font);
		newcontext.check_layout(os);
		os << minted_nonfloat_caption << "\n";
		newcontext.check_end_layout(os);
		end_inset(os);
		os << "\n\\end_layout\n";
		minted_nonfloat_caption.clear();
	}
	// Don't close the inset here for floating minted listings.
	// It will be closed at the end of the listing environment.
	if (!use_minted || minted_float.empty())
		end_inset(os);
	else {
		eat_whitespace(p, os, parent_context, true);
		Token t = p.get_token();
		if (t.asInput() != "\\end") {
			// If anything follows, collect it into a caption.
			minted_float_has_caption = true;
			os << "\n\\begin_layout Plain Layout\n"; // outer layout
			begin_inset(os, "Caption Standard\n");
			os << "\n\\begin_layout Plain Layout\n"; // inner layout
		}
		p.putback();
	}
}


/// parse an unknown environment
void parse_unknown_environment(Parser & p, string const & name, ostream & os,
			       unsigned flags, bool outer,
			       Context & parent_context)
{
	if (name == "tabbing")
		// We need to remember that we have to handle '\=' specially
		flags |= FLAG_TABBING;

	// We need to translate font changes and paragraphs inside the
	// environment to ERT if we have a non standard font.
	// Otherwise things like
	// \large\begin{foo}\huge bar\end{foo}
	// will not work.
	bool const specialfont =
		(parent_context.font != parent_context.normalfont);
	bool const new_layout_allowed = parent_context.new_layout_allowed;
	if (specialfont)
		parent_context.new_layout_allowed = false;
	output_ert_inset(os, "\\begin{" + name + "}", parent_context);
	// Try to handle options: Look if we have optional arguments,
	// and if so, put the brackets in ERT.
	while (p.hasOpt()) {
		p.get_token(); // eat '['
		output_ert_inset(os, "[", parent_context);
		os << parse_text_snippet(p, FLAG_BRACK_LAST, outer, parent_context);
		output_ert_inset(os, "]", parent_context);
	}
	parse_text_snippet(p, os, flags, outer, parent_context);
	output_ert_inset(os, "\\end{" + name + "}", parent_context);
	if (specialfont)
		parent_context.new_layout_allowed = new_layout_allowed;
}


void parse_environment(Parser & p, ostream & os, bool outer,
                       string & last_env, Context & parent_context)
{
	Layout const * newlayout;
	InsetLayout const * newinsetlayout = 0;
	string const name = p.getArg('{', '}');
	const bool is_starred = suffixIs(name, '*');
	string const unstarred_name = rtrim(name, "*");
	active_environments.push_back(name);

	// We use this loop and break out after a condition is met
	// rather than a huge else-if-chain.
	while (true) {
		if (is_math_env(name)) {
			parent_context.check_layout(os);
			begin_inset(os, "Formula ");
			os << "\\begin{" << name << "}";
			parse_math(p, os, FLAG_END, MATH_MODE);
			os << "\\end{" << name << "}";
			end_inset(os);
			if (is_display_math_env(name)) {
				// Prevent the conversion of a line break to a space
				// (bug 7668). This does not change the output, but
				// looks ugly in LyX.
				eat_whitespace(p, os, parent_context, false);
			}
			break;
		}

		// We need to use fromPolyglossiaEnvironment due to Arabic > arabic
		if (is_known(fromPolyglossiaEnvironment(name), preamble.polyglossia_languages)) {
			// We must begin a new paragraph if not already done
			if (!parent_context.atParagraphStart()) {
				parent_context.check_end_layout(os);
				parent_context.new_paragraph(os);
			}
			// store previous language because we must reset it at the end
			string const lang_old = parent_context.font.language;
			// save new language in context so that it is
			// handled by parse_text
			parent_context.font.language =
				preamble.polyglossia2lyx(fromPolyglossiaEnvironment(name));
			parse_text(p, os, FLAG_END, outer, parent_context);
			// reset previous language
			parent_context.font.language = lang_old;
			// Just in case the environment is empty
			parent_context.extra_stuff.erase();
			// We must begin a new paragraph to reset the language
			parent_context.new_paragraph(os);
			p.skip_spaces();
			break;
		}

		if (unstarred_name == "tabular" || name == "longtable"
			 || name == "tabularx" || name == "xltabular") {
			eat_whitespace(p, os, parent_context, false);
			string width = "0pt";
			string halign;
			if ((name == "longtable" || name == "xltabular") && p.hasOpt()) {
				string const opt = p.getArg('[', ']');
				if (opt == "c")
					halign = "center";
				else if (opt == "l")
					halign = "left";
				else if (opt == "r")
					halign = "right";
			}
			if (name == "tabular*" || name == "tabularx" || name == "xltabular") {
				width = lyx::translate_len(p.getArg('{', '}'));
				eat_whitespace(p, os, parent_context, false);
			}
			parent_context.check_layout(os);
			begin_inset(os, "Tabular ");
			handle_tabular(p, os, name, width, halign, parent_context);
			end_inset(os);
			p.skip_spaces();
			break;
		}

		if (parent_context.textclass.floats().typeExist(unstarred_name)) {
			eat_whitespace(p, os, parent_context, false);
			string const opt = p.hasOpt() ? p.getArg('[', ']') : string();
			eat_whitespace(p, os, parent_context, false);
			parent_context.check_layout(os);
			begin_inset(os, "Float " + unstarred_name + "\n");
			// store the float type for subfloats
			// subfloats only work with figures and tables
			if (unstarred_name == "figure")
				float_type = unstarred_name;
			else if (unstarred_name == "table")
				float_type = unstarred_name;
			else
				float_type = "";
			if (!opt.empty())
				os << "placement " << opt << '\n';
			if (contains(opt, "H"))
				preamble.registerAutomaticallyLoadedPackage("float");
			else {
				Floating const & fl = parent_context.textclass.floats()
						      .getType(unstarred_name);
				if (!fl.floattype().empty() && fl.usesFloatPkg())
					preamble.registerAutomaticallyLoadedPackage("float");
			}

			os << "wide " << convert<string>(is_starred)
			   << "\nsideways false"
			   << "\nstatus open\n\n";
			set<string> pass_thru_cmds = parent_context.pass_thru_cmds;
			if (unstarred_name == "algorithm")
				// in algorithm, \; has special meaning
				parent_context.pass_thru_cmds.insert(";");
			parse_text_in_inset(p, os, FLAG_END, outer, parent_context);
			if (unstarred_name == "algorithm")
				parent_context.pass_thru_cmds = pass_thru_cmds;
			end_inset(os);
			// We don't need really a new paragraph, but
			// we must make sure that the next item gets a \begin_layout.
			parent_context.new_paragraph(os);
			p.skip_spaces();
			// the float is parsed thus delete the type
			float_type = "";
			break;
		}

		if (unstarred_name == "sidewaysfigure"
		    || unstarred_name == "sidewaystable"
		    || unstarred_name == "sidewaysalgorithm") {
			string const opt = p.hasOpt() ? p.getArg('[', ']') : string();
			eat_whitespace(p, os, parent_context, false);
			parent_context.check_layout(os);
			if (unstarred_name == "sidewaysfigure")
				begin_inset(os, "Float figure\n");
			else if (unstarred_name == "sidewaystable")
				begin_inset(os, "Float table\n");
			else if (unstarred_name == "sidewaysalgorithm")
				begin_inset(os, "Float algorithm\n");
			if (!opt.empty())
				os << "placement " << opt << '\n';
			if (contains(opt, "H"))
				preamble.registerAutomaticallyLoadedPackage("float");
			os << "wide " << convert<string>(is_starred)
			   << "\nsideways true"
			   << "\nstatus open\n\n";
			parse_text_in_inset(p, os, FLAG_END, outer, parent_context);
			end_inset(os);
			// We don't need really a new paragraph, but
			// we must make sure that the next item gets a \begin_layout.
			parent_context.new_paragraph(os);
			p.skip_spaces();
			preamble.registerAutomaticallyLoadedPackage("rotfloat");
			break;
		}

		if (name == "wrapfigure" || name == "wraptable") {
			// syntax is \begin{wrapfigure}[lines]{placement}[overhang]{width}
			eat_whitespace(p, os, parent_context, false);
			parent_context.check_layout(os);
			// default values
			string lines = "0";
			string overhang = "0col%";
			// parse
			if (p.hasOpt())
				lines = p.getArg('[', ']');
			string const placement = p.getArg('{', '}');
			if (p.hasOpt())
				overhang = p.getArg('[', ']');
			string const width = p.getArg('{', '}');
			// write
			if (name == "wrapfigure")
				begin_inset(os, "Wrap figure\n");
			else
				begin_inset(os, "Wrap table\n");
			os << "lines " << lines
			   << "\nplacement " << placement
			   << "\noverhang " << lyx::translate_len(overhang)
			   << "\nwidth " << lyx::translate_len(width)
			   << "\nstatus open\n\n";
			parse_text_in_inset(p, os, FLAG_END, outer, parent_context);
			end_inset(os);
			// We don't need really a new paragraph, but
			// we must make sure that the next item gets a \begin_layout.
			parent_context.new_paragraph(os);
			p.skip_spaces();
			preamble.registerAutomaticallyLoadedPackage("wrapfig");
			break;
		}

		if (name == "minipage") {
			eat_whitespace(p, os, parent_context, false);
			// Test whether this is an outer box of a shaded box
			p.pushPosition();
			// swallow arguments
			while (p.hasOpt()) {
				p.getArg('[', ']');
				p.skip_spaces(true);
			}
			p.getArg('{', '}');
			p.skip_spaces(true);
			Token t = p.get_token();
			bool shaded = false;
			if (t.asInput() == "\\begin") {
				p.skip_spaces(true);
				if (p.getArg('{', '}') == "shaded")
					shaded = true;
			}
			p.popPosition();
			if (shaded)
				parse_outer_box(p, os, FLAG_END, outer,
						parent_context, name, "shaded");
			else
				parse_box(p, os, 0, FLAG_END, outer, parent_context,
					  "", "", name, "", "");
			p.skip_spaces();
			break;
		}

		if (name == "comment") {
			eat_whitespace(p, os, parent_context, false);
			parent_context.check_layout(os);
			begin_inset(os, "Note Comment\n");
			os << "status open\n";
			parse_text_in_inset(p, os, FLAG_END, outer, parent_context);
			end_inset(os);
			p.skip_spaces();
			skip_braces(p); // eat {} that might by set by LyX behind comments
			preamble.registerAutomaticallyLoadedPackage("verbatim");
			break;
		}

		if (unstarred_name == "verbatim") {
			// FIXME: this should go in the generic code that
			// handles environments defined in layout file that
			// have "PassThru 1". However, the code over there is
			// already too complicated for my taste.
			string const ascii_name =
				(name == "verbatim*") ? "Verbatim*" : "Verbatim";
			parent_context.new_paragraph(os);
			Context context(true, parent_context.textclass,
					&parent_context.textclass[from_ascii(ascii_name)]);
			string s = p.verbatimEnvironment(name);
			output_ert(os, s, context);
			p.skip_spaces();
			break;
		}

		if (name == "IPA") {
			eat_whitespace(p, os, parent_context, false);
			parent_context.check_layout(os);
			begin_inset(os, "IPA\n");
			set<string> pass_thru_cmds = parent_context.pass_thru_cmds;
			// These commands have special meanings in IPA
			parent_context.pass_thru_cmds.insert("!");
			parent_context.pass_thru_cmds.insert(";");
			parent_context.pass_thru_cmds.insert(":");
			parse_text_in_inset(p, os, FLAG_END, outer, parent_context);
			parent_context.pass_thru_cmds = pass_thru_cmds;
			end_inset(os);
			p.skip_spaces();
			preamble.registerAutomaticallyLoadedPackage("tipa");
			preamble.registerAutomaticallyLoadedPackage("tipx");
			break;
		}

		if (name == parent_context.textclass.titlename()
		    && parent_context.textclass.titletype() == TITLE_ENVIRONMENT) {
			parse_text(p, os, FLAG_END, outer, parent_context);
			// Just in case the environment is empty
			parent_context.extra_stuff.erase();
			// We must begin a new paragraph
			parent_context.new_paragraph(os);
			p.skip_spaces();
			break;
		}

		if (name == "CJK") {
			// the scheme is \begin{CJK}{encoding}{mapping}text\end{CJK}
			// It is impossible to decide if a CJK environment was in its own paragraph or within
			// a line. We therefore always assume a paragraph since the latter is a rare case.
			eat_whitespace(p, os, parent_context, false);
			parent_context.check_end_layout(os);
			// store the encoding to be able to reset it
			string const encoding_old = p.getEncoding();
			string const encoding = p.getArg('{', '}');
			// FIXME: For some reason JIS does not work. Although the text
			// in tests/CJK.tex is identical with the SJIS version if you
			// convert both snippets using the recode command line utility,
			// the resulting .lyx file contains some extra characters if
			// you set buggy_encoding to false for JIS.
			bool const buggy_encoding = encoding == "JIS";
			if (!buggy_encoding)
				p.setEncoding(encoding, Encoding::CJK);
			else {
				// FIXME: This will read garbage, since the data is not encoded in utf8.
				p.setEncoding("UTF-8");
			}
			// LyX only supports the same mapping for all CJK
			// environments, so we might need to output everything as ERT
			string const mapping = trim(p.getArg('{', '}'));
			char const * const * const where =
				is_known(encoding, supported_CJK_encodings);
			if (!buggy_encoding && !preamble.fontCJKSet())
				preamble.fontCJK(mapping);
			bool knownMapping = mapping == preamble.fontCJK();
			if (buggy_encoding || !knownMapping || !where) {
				parent_context.check_layout(os);
				output_ert_inset(os, "\\begin{" + name + "}{" + encoding + "}{" + mapping + "}",
					       parent_context);
				// we must parse the content as verbatim because e.g. JIS can contain
				// normally invalid characters
				// FIXME: This works only for the most simple cases.
				//        Since TeX control characters are not parsed,
				//        things like comments are completely wrong.
				string const s = p.plainEnvironment("CJK");
				for (string::const_iterator it = s.begin(), et = s.end(); it != et; ++it) {
					string snip;
					snip += *it;
					if (snip == "\\" || is_known(snip, known_escaped_chars))
						output_ert_inset(os, snip, parent_context);
					else if (*it == '\n' && it + 1 != et && s.begin() + 1 != it)
						os << "\n ";
					else
						os << *it;
				}
				output_ert_inset(os, "\\end{" + name + "}",
					       parent_context);
			} else {
				string const lang =
					supported_CJK_languages[where - supported_CJK_encodings];
				// store the language because we must reset it at the end
				string const lang_old = parent_context.font.language;
				parent_context.font.language = lang;
				parse_text_in_inset(p, os, FLAG_END, outer, parent_context);
				parent_context.font.language = lang_old;
				parent_context.new_paragraph(os);
			}
			p.setEncoding(encoding_old);
			p.skip_spaces();
			break;
		}

		if (name == "lyxgreyedout") {
			eat_whitespace(p, os, parent_context, false);
			parent_context.check_layout(os);
			begin_inset(os, "Note Greyedout\n");
			os << "status open\n";
			parse_text_in_inset(p, os, FLAG_END, outer, parent_context);
			end_inset(os);
			p.skip_spaces();
			if (!preamble.notefontcolor().empty())
				preamble.registerAutomaticallyLoadedPackage("color");
			break;
		}

		if (name == "btSect") {
			eat_whitespace(p, os, parent_context, false);
			parent_context.check_layout(os);
			begin_command_inset(os, "bibtex", "bibtex");
			string bibstyle = "plain";
			if (p.hasOpt()) {
				bibstyle = p.getArg('[', ']');
				p.skip_spaces(true);
			}
			string const bibfile = p.getArg('{', '}');
			eat_whitespace(p, os, parent_context, false);
			Token t = p.get_token();
			if (t.asInput() == "\\btPrintCited") {
				p.skip_spaces(true);
				os << "btprint " << '"' << "btPrintCited" << '"' << "\n";
			}
			if (t.asInput() == "\\btPrintNotCited") {
				p.skip_spaces(true);
				os << "btprint " << '"' << "btPrintNotCited" << '"' << "\n";
			}
			if (t.asInput() == "\\btPrintAll") {
				p.skip_spaces(true);
				os << "btprint " << '"' << "btPrintAll" << '"' << "\n";
			}
			os << "bibfiles " << '"' << bibfile << "\"\n"
			   << "options " << '"' << bibstyle << "\"\n";
			parse_text_in_inset(p, os, FLAG_END, outer, parent_context);
			end_inset(os);
			p.skip_spaces();
			break;
		}

		if (name == "btUnit") {
			string const nt = p.next_next_token().cs();
			// Do not attempt to overwrite a former diverging multibib.
			// Those are output as ERT instead.
			if ((nt == "part" || nt == "chapter"
			     || nt == "section" || nt == "subsection")
			   && (preamble.multibib().empty() || preamble.multibib() == nt)) {
				parse_text(p, os, FLAG_END, outer, parent_context);
				preamble.multibib(nt);
			} else
				parse_unknown_environment(p, name, os, FLAG_END, outer,
							  parent_context);
			break;
		}

		// This is only attempted at turn environments that consist only
		// of a tabular (this is how tables in LyX, modulo longtables, are rotated).
		// Thus we will fall through in other cases.
		if (name == "turn") {
			// We check if the next thing is a tabular[*|x]
			p.pushPosition();
			p.getArg('{', '}');// eat turn argument
			bool found_end = false;
			bool only_table = false;
			bool end_table = false;
			p.get_token();
			p.get_token();
			string envname = p.getArg('{', '}');
			if (rtrim(envname, "*") == "tabular" || envname == "tabularx") {
				// Now we check if the table is the only content
				// of the turn environment
				string const tenv = envname;
				while (!found_end && !end_table && p.good()) {
					envname = p.next_token().cat() == catBegin
							? p.getArg('{', '}') : string();
					Token const & t = p.get_token();
					p.skip_spaces();
					end_table = t.asInput() != "\\end"
							&& envname == tenv;
					found_end = t.asInput() == "\\end"
							&& envname == "turn";
				}
				if (end_table) {
					p.get_token();
					envname = p.getArg('{', '}');
					only_table = p.next_next_token().asInput() == "\\end"
							&& envname == "turn";
				}
				if (only_table) {
					p.popPosition();
					string const angle = p.getArg('{', '}');
					p.skip_spaces();
					int const save_tablerotation = parent_context.tablerotation;
					parent_context.tablerotation = convert<int>(angle);
					parse_text(p, os, FLAG_END, outer, parent_context);
					parent_context.tablerotation = save_tablerotation;
					p.skip_spaces();
					break;
				}
				// fall through
			}
			// fall through
			p.popPosition();
		}

		// This is only attempted at landscape environments that consist only
		// of a longtable (this is how longtables in LyX are rotated by 90 degs).
		// Other landscape environment is handled via the landscape module, thus
		// we will fall through in that case.
		if (name == "landscape") {
			// We check if the next thing is a longtable
			p.pushPosition();
			bool found_end = false;
			bool only_longtable = false;
			bool end_longtable = false;
			p.get_token();
			p.get_token();
			string envname = p.getArg('{', '}');
			if (envname == "longtable" || envname == "xltabular") {
				// Now we check if the longtable is the only content
				// of the landscape environment
				string const ltenv = envname;
				while (!found_end && !end_longtable && p.good()) {
					envname = p.next_token().cat() == catBegin
							? p.getArg('{', '}') : string();
					Token const & t = p.get_token();
					p.skip_spaces();
					end_longtable = t.asInput() != "\\end"
							&& envname == ltenv;
					found_end = t.asInput() == "\\end"
							&& envname == "landscape";
				}
				if (end_longtable) {
					p.get_token();
					envname = p.getArg('{', '}');
					only_longtable = p.next_next_token().asInput() == "\\end"
							&& envname == "landscape";
				}
				if (only_longtable) {
					p.popPosition();
					p.skip_spaces();
					int const save_tablerotation = parent_context.tablerotation;
					parent_context.tablerotation = 90;
					parse_text(p, os, FLAG_END, outer, parent_context);
					parent_context.tablerotation = save_tablerotation;
					p.skip_spaces();
					break;
				}
				// fall through
			}
			// fall through
			p.popPosition();
		}

		if (name == "framed" || name == "shaded") {
			eat_whitespace(p, os, parent_context, false);
			parse_outer_box(p, os, FLAG_END, outer, parent_context, name, "");
			p.skip_spaces();
			preamble.registerAutomaticallyLoadedPackage("framed");
			break;
		}

		if (name == "listing") {
			minted_float = "float";
			eat_whitespace(p, os, parent_context, false);
			string const opt = p.hasOpt() ? p.getArg('[', ']') : string();
			if (!opt.empty())
				minted_float += "=" + opt;
			// If something precedes \begin{minted}, we output it at the end
			// as a caption, in order to keep it inside the listings inset.
			eat_whitespace(p, os, parent_context, true);
			p.pushPosition();
			Token const & t = p.get_token();
			p.skip_spaces(true);
			string const envname = p.next_token().cat() == catBegin
							? p.getArg('{', '}') : string();
			bool prologue = t.asInput() != "\\begin" || envname != "minted";
			p.popPosition();
			minted_float_has_caption = false;
			string content = parse_text_snippet(p, FLAG_END, outer,
							    parent_context);
			size_t i = content.find("\\begin_inset listings");
			bool minted_env = i != string::npos;
			string caption;
			if (prologue) {
				caption = content.substr(0, i);
				content.erase(0, i);
			}
			parent_context.check_layout(os);
			if (minted_env && minted_float_has_caption) {
				eat_whitespace(p, os, parent_context, true);
				os << content << "\n";
				if (!caption.empty())
					os << caption << "\n";
				os << "\n\\end_layout\n"; // close inner layout
				end_inset(os);            // close caption inset
				os << "\n\\end_layout\n"; // close outer layout
			} else if (!caption.empty()) {
				if (!minted_env) {
					begin_inset(os, "listings\n");
					os << "lstparams " << '"' << minted_float << '"' << '\n';
					os << "inline false\n";
					os << "status collapsed\n";
				}
				os << "\n\\begin_layout Plain Layout\n";
				begin_inset(os, "Caption Standard\n");
				Context newcontext(true, parent_context.textclass,
						   0, 0, parent_context.font);
				newcontext.check_layout(os);
				os << caption << "\n";
				newcontext.check_end_layout(os);
				end_inset(os);
				os << "\n\\end_layout\n";
			} else if (content.empty()) {
				begin_inset(os, "listings\n");
				os << "lstparams " << '"' << minted_float << '"' << '\n';
				os << "inline false\n";
				os << "status collapsed\n";
			} else {
				os << content << "\n";
			}
			end_inset(os); // close listings inset
			parent_context.check_end_layout(os);
			parent_context.new_paragraph(os);
			p.skip_spaces();
			minted_float.clear();
			minted_float_has_caption = false;
			break;
		}

		if (name == "lstlisting" || name == "minted") {
			bool use_minted = name == "minted";
			// with listings, we do not eat newlines here since
			// \begin{lstlistings}
			// [foo]
			// and
			// // \begin{lstlistings}%
			//
			// [foo]
			// reads [foo] as content, whereas
			// // \begin{lstlistings}%
			// [foo]
			// or
			// \begin{lstlistings}[foo,
			// bar]
			// reads [foo...] as argument.
			eat_whitespace(p, os, parent_context, false, use_minted);
			if (use_minted && minted_float.empty()) {
				// look ahead for a bottom caption
				p.pushPosition();
				bool found_end_minted = false;
				while (!found_end_minted && p.good()) {
					Token const & t = p.get_token();
					p.skip_spaces();
					string const envname =
						p.next_token().cat() == catBegin
							? p.getArg('{', '}') : string();
					found_end_minted = t.asInput() == "\\end"
								&& envname == "minted";
				}
				eat_whitespace(p, os, parent_context, true);
				Token const & t = p.get_token();
				p.skip_spaces(true);
				if (t.asInput() == "\\lyxmintcaption") {
					string const pos = p.getArg('[', ']');
					if (pos == "b") {
						string const caption =
							parse_text_snippet(p, FLAG_ITEM,
								false, parent_context);
						minted_nonfloat_caption = "[b]" + caption;
						eat_whitespace(p, os, parent_context, true);
					}
				}
				p.popPosition();
			}
			parse_listings(p, os, parent_context, false, use_minted);
			p.skip_spaces();
			break;
		}

		if (!parent_context.new_layout_allowed) {
			parse_unknown_environment(p, name, os, FLAG_END, outer,
						  parent_context);
			break;
		}

		// Alignment and spacing settings
		// FIXME (bug xxxx): These settings can span multiple paragraphs and
		//					 therefore are totally broken!
		// Note that \centering, \raggedright, and \raggedleft cannot be handled, as
		// they are commands not environments. They are furthermore switches that
		// can be ended by another switches, but also by commands like \footnote or
		// \parbox. So the only safe way is to leave them untouched.
		// However, we support the pseudo-environments
		// \begin{centering} ... \end{centering}
		// \begin{raggedright} ... \end{raggedright}
		// \begin{raggedleft} ... \end{raggedleft}
		// since they are used by LyX in floats (for spacing reasons)
		if (name == "center" || name == "centering"
		    || name == "flushleft" || name == "raggedright"
		    || name == "flushright" || name == "raggedleft"
		    || name == "singlespace" || name == "onehalfspace"
		    || name == "doublespace" || name == "spacing") {
			eat_whitespace(p, os, parent_context, false);
			// We must begin a new paragraph if not already done
			if (! parent_context.atParagraphStart()) {
				parent_context.check_end_layout(os);
				parent_context.new_paragraph(os);
			}
			if (name == "flushleft" || name == "raggedright")
				parent_context.add_extra_stuff("\\align left\n");
			else if (name == "flushright" || name == "raggedleft")
				parent_context.add_extra_stuff("\\align right\n");
			else if (name == "center" || name == "centering")
				parent_context.add_extra_stuff("\\align center\n");
			else if (name == "singlespace")
				parent_context.add_extra_stuff("\\paragraph_spacing single\n");
			else if (name == "onehalfspace") {
				parent_context.add_extra_stuff("\\paragraph_spacing onehalf\n");
				preamble.registerAutomaticallyLoadedPackage("setspace");
			} else if (name == "doublespace") {
				parent_context.add_extra_stuff("\\paragraph_spacing double\n");
				preamble.registerAutomaticallyLoadedPackage("setspace");
			} else if (name == "spacing") {
				parent_context.add_extra_stuff("\\paragraph_spacing other " + p.verbatim_item() + "\n");
				preamble.registerAutomaticallyLoadedPackage("setspace");
			}
			parse_text(p, os, FLAG_END, outer, parent_context);
			// Just in case the environment is empty
			parent_context.extra_stuff.erase();
			// We must begin a new paragraph to reset the alignment
			parent_context.new_paragraph(os);
			p.skip_spaces();
			break;
		}

		// The single '=' is meant here.
		if ((newlayout = findLayout(parent_context.textclass, name, false))) {
			eat_whitespace(p, os, parent_context, false);
			Context context(true, parent_context.textclass, newlayout,
					parent_context.layout, parent_context.font);
			// If we have options we don't know of in the layout,
			// output ERT
			if (p.hasOpt() && context.layout->optArgs() == 0) {
				parse_unknown_environment(p, name, os, FLAG_END, outer, parent_context);
				break;
			}
			if (parent_context.deeper_paragraph) {
				// We are beginning a nested environment after a
				// deeper paragraph inside the outer list environment.
				// Therefore we don't need to output a "begin deeper".
				context.need_end_deeper = true;
			}
			parent_context.check_end_layout(os);
			if (last_env == name) {
				// we need to output a separator since LyX would export
				// the two environments as one otherwise (bug 5716)
				TeX2LyXDocClass const & textclass(parent_context.textclass);
				Context newcontext(true, textclass,
						&(textclass.defaultLayout()));
				newcontext.check_layout(os);
				begin_inset(os, "Separator plain\n");
				end_inset(os);
				newcontext.check_end_layout(os);
			}
			switch (context.layout->latextype) {
			case LATEX_LIST_ENVIRONMENT:
				context.in_list_preamble =
					!context.layout->listpreamble().empty()
					&& p.hasListPreamble(context.layout->itemcommand());
				context.add_par_extra_stuff("\\labelwidthstring "
							    + p.verbatim_item() + '\n');
				p.skip_spaces();
				break;
			case LATEX_BIB_ENVIRONMENT:
				p.verbatim_item(); // swallow next arg
				p.skip_spaces();
				break;
			default:
				break;
			}
			context.check_deeper(os);
			if (newlayout->keepempty) {
				// We need to start a new paragraph
				// even if it is empty.
				context.new_paragraph(os);
				context.check_layout(os);
			}
			// handle known optional and required arguments
			if (context.layout->latextype == LATEX_ENVIRONMENT)
				output_arguments(os, p, outer, false, string(), context,
						 context.layout->latexargs());
			else if (context.layout->latextype == LATEX_ITEM_ENVIRONMENT) {
				context.in_list_preamble =
					!context.layout->listpreamble().empty()
					&& p.hasListPreamble(context.layout->itemcommand());
				ostringstream oss;
				output_arguments(oss, p, outer, false, string(), context,
						 context.layout->latexargs());
				context.list_extra_stuff = oss.str();
			}
			if (context.in_list_preamble) {
				// Collect the stuff between \begin and first \item
				context.list_preamble =
					parse_text_snippet(p, FLAG_END, outer, context);
				context.in_list_preamble = false;
			}
			parse_text(p, os, FLAG_END, outer, context);
			if (context.layout->latextype == LATEX_ENVIRONMENT)
				output_arguments(os, p, outer, false, "post", context,
						 context.layout->postcommandargs());
			context.check_end_layout(os);
			if (parent_context.deeper_paragraph) {
				// We must suppress the "end deeper" because we
				// suppressed the "begin deeper" above.
				context.need_end_deeper = false;
			}
			context.check_end_deeper(os);
			parent_context.new_paragraph(os);
			p.skip_spaces();
			if (!preamble.titleLayoutFound())
				preamble.titleLayoutFound(newlayout->intitle);
			set<string> const & req = newlayout->required();
			set<string>::const_iterator it = req.begin();
			set<string>::const_iterator en = req.end();
			for (; it != en; ++it)
				preamble.registerAutomaticallyLoadedPackage(*it);
			break;
		}

		// The single '=' is meant here.
		if ((newinsetlayout = findInsetLayout(parent_context.textclass, name, false))) {
			eat_whitespace(p, os, parent_context, false);
			parent_context.check_layout(os);
			begin_inset(os, "Flex ");
			docstring flex_name = newinsetlayout->name();
			// FIXME: what do we do if the prefix is not Flex: ?
			if (prefixIs(flex_name, from_ascii("Flex:")))
				flex_name.erase(0, 5);
			os << to_utf8(flex_name) << '\n'
			   << "status collapsed\n";
			if (newinsetlayout->isPassThru()) {
				string const arg = p.verbatimEnvironment(name);
				Context context(true, parent_context.textclass,
						&parent_context.textclass.plainLayout(),
						parent_context.layout);
				output_ert(os, arg, parent_context);
			} else
				parse_text_in_inset(p, os, FLAG_END, false, parent_context, newinsetlayout);
			end_inset(os);
			break;
		}

		if (name == "appendix") {
			// This is no good latex style, but it works and is used in some documents...
			eat_whitespace(p, os, parent_context, false);
			parent_context.check_end_layout(os);
			Context context(true, parent_context.textclass, parent_context.layout,
					parent_context.layout, parent_context.font);
			context.check_layout(os);
			os << "\\start_of_appendix\n";
			parse_text(p, os, FLAG_END, outer, context);
			context.check_end_layout(os);
			p.skip_spaces();
			break;
		}

		if (known_environments.find(name) != known_environments.end()) {
			vector<ArgumentType> arguments = known_environments[name];
			// The last "argument" denotes whether we may translate the
			// environment contents to LyX
			// The default required if no argument is given makes us
			// compatible with the reLyXre environment.
			ArgumentType contents = arguments.empty() ?
				required :
				arguments.back();
			if (!arguments.empty())
				arguments.pop_back();
			// See comment in parse_unknown_environment()
			bool const specialfont =
				(parent_context.font != parent_context.normalfont);
			bool const new_layout_allowed =
				parent_context.new_layout_allowed;
			if (specialfont)
				parent_context.new_layout_allowed = false;
			parse_arguments("\\begin{" + name + "}", arguments, p, os,
					outer, parent_context);
			if (contents == verbatim)
				output_ert_inset(os, p.ertEnvironment(name),
					   parent_context);
			else
				parse_text_snippet(p, os, FLAG_END, outer,
						   parent_context);
			output_ert_inset(os, "\\end{" + name + "}", parent_context);
			if (specialfont)
				parent_context.new_layout_allowed = new_layout_allowed;
			break;
		}

		parse_unknown_environment(p, name, os, FLAG_END, outer, parent_context);
		break;
	}// end of loop

	last_env = name;
	active_environments.pop_back();
}


/// parses a comment and outputs it to \p os.
void parse_comment(Parser & p, ostream & os, Token const & t, Context & context,
		   bool skipNewlines = false)
{
	LASSERT(t.cat() == catComment, return);
	string comment = t.cs();
	// Join multiple consecutive comment lines into one ERT inset
	while (p.next_token().cat() == catComment)
		comment += "\n%" + p.get_token().cs();

	if (!comment.empty()) {
		context.check_layout(os);
		output_comment(p, os, comment, context);
		if (p.next_token().cat() == catNewline) {
			// A newline after a comment line starts a new
			// paragraph
			if (context.new_layout_allowed) {
				if(!context.atParagraphStart())
					// Only start a new paragraph if not already
					// done (we might get called recursively)
					context.new_paragraph(os);
			} else
				output_ert_inset(os, "\n", context);
			eat_whitespace(p, os, context, true);
		}
	} else if (!skipNewlines) {
		// "%\n" combination
		p.skip_spaces();
	}
}


/*!
 * Reads spaces and comments until the first non-space, non-comment token.
 * New paragraphs (double newlines or \\par) are handled like simple spaces
 * if \p eatParagraph is true.
 * If \p eatNewline is false, newlines won't be treated as whitespace.
 * Spaces are skipped, but comments are written to \p os.
 */
void eat_whitespace(Parser & p, ostream & os, Context & context,
		    bool eatParagraph, bool eatNewline)
{
	while (p.good()) {
		Token const & t = p.get_token();
		if (t.cat() == catComment)
			parse_comment(p, os, t, context, !eatNewline);
		else if ((!eatParagraph && p.isParagraph()) ||
			 (t.cat() != catSpace && (t.cat() != catNewline || !eatNewline))) {
			p.putback();
			return;
		}
	}
}


/*!
 * Set a font attribute, parse text and reset the font attribute.
 * \param attribute Attribute name (e.g. \\family, \\shape etc.)
 * \param currentvalue Current value of the attribute. Is set to the new
 * value during parsing.
 * \param newvalue New value of the attribute
 */
void parse_text_attributes(Parser & p, ostream & os, unsigned flags, bool outer,
			   Context & context, string const & attribute,
			   string & currentvalue, string const & newvalue)
{
	context.check_layout(os);
	string const oldvalue = currentvalue;
	currentvalue = newvalue;
	os << '\n' << attribute << ' ' << newvalue << "\n";
	parse_text_snippet(p, os, flags, outer, context);
	context.check_layout(os);
	os << '\n' << attribute << ' ' << oldvalue << "\n";
	currentvalue = oldvalue;
}


/// get the arguments of a natbib or jurabib citation command
void get_cite_arguments(Parser & p, bool natbibOrder,
	string & before, string & after, bool const qualified = false)
{
	// We need to distinguish "" and "[]", so we can't use p.getOpt().

	// text before the citation
	before.clear();
	// text after the citation
	after = qualified ? p.getFullOpt(false, '(', ')') : p.getFullOpt();

	if (!after.empty()) {
		before = qualified ? p.getFullOpt(false, '(', ')') : p.getFullOpt();
		if (natbibOrder && !before.empty())
			swap(before, after);
	}
}


void copy_file(FileName const & src, string const & dstname)
{
	if (!copyFiles())
		return;
	string const absParent = getParentFilePath(false);
	FileName dst;
	if (FileName::isAbsolute(dstname))
		dst = FileName(dstname);
	else
		dst = makeAbsPath(dstname, absParent);
	FileName const srcpath = src.onlyPath();
	FileName const dstpath = dst.onlyPath();
	if (equivalent(srcpath, dstpath))
		return;
	if (!dstpath.isDirectory()) {
		if (!dstpath.createPath()) {
			warning_message("Could not create directory for file `"
					+ dst.absFileName() + "´.");
			return;
		}
	}
	if (dst.isReadableFile()) {
		if (overwriteFiles())
			warning_message("Overwriting existing file `"
					+ dst.absFileName() + "´.");
		else {
			warning_message("Not overwriting existing file `"
					+ dst.absFileName() + "´.");
			return;
		}
	}
	if (!src.copyTo(dst))
		warning_message("Could not copy file `" + src.absFileName()
				+ "´ to `" + dst.absFileName() + "´.");
}


/// Parse a literate Chunk section. The initial "<<" is already parsed.
bool parse_chunk(Parser & p, ostream & os, Context & context)
{
	// check whether a chunk is possible here.
	if (!context.textclass.hasInsetLayout(from_ascii("Flex:Chunk"))) {
		return false;
	}

	p.pushPosition();

	// read the parameters
	Parser::Arg const params = p.verbatimStuff(">>=\n", false);
	if (!params.first) {
		p.popPosition();
		return false;
	}

	Parser::Arg const code = p.verbatimStuff("\n@");
	if (!code.first) {
		p.popPosition();
		return false;
	}
	string const post_chunk = p.verbatimStuff("\n").second + '\n';
	if (post_chunk[0] != ' ' && post_chunk[0] != '\n') {
		p.popPosition();
		return false;
	}
	// The last newline read is important for paragraph handling
	p.putback();
	p.deparse();

	//warning_message("params=[" + params.second + "], code=[" + code.second + "]");
	// We must have a valid layout before outputting the Chunk inset.
	context.check_layout(os);
	Context chunkcontext(true, context.textclass);
	chunkcontext.layout = &context.textclass.plainLayout();
	begin_inset(os, "Flex Chunk");
	os << "\nstatus open\n";
	if (!params.second.empty()) {
		chunkcontext.check_layout(os);
		Context paramscontext(true, context.textclass);
		paramscontext.layout = &context.textclass.plainLayout();
		begin_inset(os, "Argument 1");
		os << "\nstatus open\n";
		output_ert(os, params.second, paramscontext);
		end_inset(os);
	}
	output_ert(os, code.second, chunkcontext);
	end_inset(os);

	p.dropPosition();
	return true;
}


/// detects \\def, \\long\\def and \\global\\long\\def with ws and comments
bool is_macro(Parser & p)
{
	Token first = p.curr_token();
	if (first.cat() != catEscape || !p.good())
		return false;
	if (first.cs() == "def")
		return true;
	if (first.cs() != "global" && first.cs() != "long")
		return false;
	Token second = p.get_token();
	int pos = 1;
	while (p.good() && !p.isParagraph() && (second.cat() == catSpace ||
	       second.cat() == catNewline || second.cat() == catComment)) {
		second = p.get_token();
		pos++;
	}
	bool secondvalid = second.cat() == catEscape;
	Token third;
	bool thirdvalid = false;
	if (p.good() && first.cs() == "global" && secondvalid &&
	    second.cs() == "long") {
		third = p.get_token();
		pos++;
		while (p.good() && !p.isParagraph() &&
		       (third.cat() == catSpace ||
		        third.cat() == catNewline ||
		        third.cat() == catComment)) {
			third = p.get_token();
			pos++;
		}
		thirdvalid = third.cat() == catEscape;
	}
	for (int i = 0; i < pos; ++i)
		p.putback();
	if (!secondvalid)
		return false;
	if (!thirdvalid)
		return (first.cs() == "global" || first.cs() == "long") &&
		       second.cs() == "def";
	return first.cs() == "global" && second.cs() == "long" &&
	       third.cs() == "def";
}


/// Parse a macro definition (assumes that is_macro() returned true)
void parse_macro(Parser & p, ostream & os, Context & context)
{
	context.check_layout(os);
	Token first = p.curr_token();
	Token second;
	Token third;
	string command = first.asInput();
	if (first.cs() != "def") {
		p.get_token();
		eat_whitespace(p, os, context, false);
		second = p.curr_token();
		command += second.asInput();
		if (second.cs() != "def") {
			p.get_token();
			eat_whitespace(p, os, context, false);
			third = p.curr_token();
			command += third.asInput();
		}
	}
	eat_whitespace(p, os, context, false);
	string const name = p.get_token().cs();
	eat_whitespace(p, os, context, false);

	// parameter text
	bool simple = true;
	string paramtext;
	int arity = 0;
	while (p.next_token().cat() != catBegin) {
		if (p.next_token().cat() == catParameter) {
			// # found
			p.get_token();
			paramtext += "#";

			// followed by number?
			if (p.next_token().cat() == catOther) {
				string s = p.get_token().asInput();
				paramtext += s;
				// number = current arity + 1?
				if (s.size() == 1 && s[0] == arity + '0' + 1)
					++arity;
				else
					simple = false;
			} else
				paramtext += p.get_token().cs();
		} else {
			paramtext += p.get_token().cs();
			simple = false;
		}
	}

	// only output simple (i.e. compatible) macro as FormulaMacros
	string ert = '\\' + name + ' ' + paramtext + '{' + p.verbatim_item() + '}';
	if (simple) {
		context.check_layout(os);
		begin_inset(os, "FormulaMacro");
		os << "\n\\def" << ert;
		end_inset(os);
	} else
		output_ert_inset(os, command + ert, context);
}


void registerExternalTemplatePackages(string const & name)
{
	external::TemplateManager const & etm = external::TemplateManager::get();
	external::Template const * const et = etm.getTemplateByName(name);
	if (!et)
		return;
	external::Template::Formats::const_iterator cit = et->formats.end();
	if (pdflatex)
	        cit = et->formats.find("PDFLaTeX");
	if (cit == et->formats.end())
		// If the template has not specified a PDFLaTeX output,
		// we try the LaTeX format.
		cit = et->formats.find("LaTeX");
	if (cit == et->formats.end())
		return;
	vector<string>::const_iterator qit = cit->second.requirements.begin();
	vector<string>::const_iterator qend = cit->second.requirements.end();
	for (; qit != qend; ++qit)
		preamble.registerAutomaticallyLoadedPackage(*qit);
}

} // anonymous namespace


/*!
 * Find a file with basename \p name in path \p path and an extension
 * in \p extensions.
 */
string find_file(string const & name, string const & path,
		 char const * const * extensions)
{
	for (char const * const * what = extensions; *what; ++what) {
		string const trial = addExtension(name, *what);
		if (makeAbsPath(trial, path).exists())
			return trial;
	}
	return string();
}


/// Convert filenames with TeX macros and/or quotes to something LyX
/// can understand
string const normalize_filename(string const & name)
{
	Parser p(name);
	ostringstream os;
	while (p.good()) {
		Token const & t = p.get_token();
		if (t.cat() != catEscape)
			os << t.asInput();
		else if (t.cs() == "lyxdot") {
			// This is used by LyX for simple dots in relative
			// names
			os << '.';
			p.skip_spaces();
		} else if (t.cs() == "space") {
			os << ' ';
			p.skip_spaces();
		} else if (t.cs() == "string") {
			// Convert \string" to " and \string~ to ~
			Token const & n = p.next_token();
			if (n.asInput() != "\"" && n.asInput() != "~")
				os << t.asInput();
		} else
			os << t.asInput();
	}
	// Strip quotes. This is a bit complicated (see latex_path()).
	string full = os.str();
	if (!full.empty() && full[0] == '"') {
		string base = removeExtension(full);
		string ext = getExtension(full);
		if (!base.empty() && base[base.length()-1] == '"')
			// "a b"
			// "a b".tex
			return addExtension(trim(base, "\""), ext);
		if (full[full.length()-1] == '"')
			// "a b.c"
			// "a b.c".tex
			return trim(full, "\"");
	}
	return full;
}


/// Convert \p name from TeX convention (relative to master file) to LyX
/// convention (relative to .lyx file) if it is relative
void fix_child_filename(string & name)
{
	string const absMasterTeX = getMasterFilePath(true);
	bool const isabs = FileName::isAbsolute(name);
	// convert from "relative to .tex master" to absolute original path
	if (!isabs)
		name = makeAbsPath(name, absMasterTeX).absFileName();
	bool copyfile = copyFiles();
	string const absParentLyX = getParentFilePath(false);
	string abs = name;
	if (copyfile) {
		// convert from absolute original path to "relative to master file"
		string const rel = to_utf8(makeRelPath(from_utf8(name),
						       from_utf8(absMasterTeX)));
		// re-interpret "relative to .tex file" as "relative to .lyx file"
		// (is different if the master .lyx file resides in a
		// different path than the master .tex file)
		string const absMasterLyX = getMasterFilePath(false);
		abs = makeAbsPath(rel, absMasterLyX).absFileName();
		// Do not copy if the new path is impossible to create. Example:
		// absMasterTeX = "/foo/bar/"
		// absMasterLyX = "/bar/"
		// name = "/baz.eps" => new absolute name would be "/../baz.eps"
		if (contains(name, "/../"))
			copyfile = false;
	}
	if (copyfile) {
		if (isabs)
			name = abs;
		else {
			// convert from absolute original path to
			// "relative to .lyx file"
			name = to_utf8(makeRelPath(from_utf8(abs),
						   from_utf8(absParentLyX)));
		}
	}
	else if (!isabs) {
		// convert from absolute original path to "relative to .lyx file"
		name = to_utf8(makeRelPath(from_utf8(name),
					   from_utf8(absParentLyX)));
	}
}


void parse_text(Parser & p, ostream & os, unsigned flags, bool outer,
		Context & context, string const & rdelim, string const & rdelimesc)
{
	Layout const * newlayout = 0;
	InsetLayout const * newinsetlayout = 0;
	char const * const * where = 0;
	// Store the latest bibliographystyle, addcontentslineContent and
	// nocite{*} option (needed for bibtex inset)
	string btprint;
	string contentslineContent;
	// Some classes provide a \bibliographystyle, so do not output
	// any if none is explicitly set.
	string bibliographystyle;
	bool const use_natbib = isProvided("natbib");
	bool const use_jurabib = isProvided("jurabib");
	bool const use_biblatex = isProvided("biblatex")
			&& preamble.citeEngine() != "biblatex-natbib";
	bool const use_biblatex_natbib = isProvided("biblatex-natbib")
			|| (isProvided("biblatex") && preamble.citeEngine() == "biblatex-natbib");
	bool const use_biblatex_chicago = isProvided("biblatex-chicago")
			|| (isProvided("biblatex") && preamble.citeEngine() == "biblatex-chicago");
	need_commentbib = use_biblatex || use_biblatex_natbib || use_biblatex_chicago;
	string last_env;

	// it is impossible to determine the correct encoding for non-CJK Japanese.
	// Therefore write a note at the beginning of the document
	if (is_nonCJKJapanese) {
		context.check_layout(os);
		begin_inset(os, "Note Note\n");
		os << "status open\n\\begin_layout Plain Layout\n"
		   << "\\series bold\n"
		   << "Important information:\n"
		   << "\\end_layout\n\n"
		   << "\\begin_layout Plain Layout\n"
		   << "The original LaTeX source for this document is in Japanese (pLaTeX).\n"
		   << " It was therefore impossible for tex2lyx to determine the correct encoding.\n"
		   << " The iconv encoding " << p.getEncoding() << " was used.\n"
		   << " If this is incorrect, you must run the tex2lyx program on the command line\n"
		   << " and specify the encoding using the -e command-line switch.\n"
		   << " In addition, you might want to double check that the desired output encoding\n"
		   << " is correctly selected in Document > Settings > Language.\n"
		   << "\\end_layout\n";
		end_inset(os);
		is_nonCJKJapanese = false;
	}

	bool have_cycled = false;
	while (p.good()) {
		// Leave here only after at least one cycle
		if (have_cycled && flags & FLAG_LEAVE) {
			flags &= ~FLAG_LEAVE;
			break;
		}

		Token const & t = p.get_token();
#ifdef FILEDEBUG
		debugToken(cerr, t, flags);
#endif

		if (context.in_list_preamble
		    && p.next_token().cs() == context.layout->itemcommand()) {
			// We are parsing a list preamble. End before first \item.
			flags |= FLAG_LEAVE;
			context.in_list_preamble = false;
		}

		if (flags & FLAG_ITEM) {
			if (t.cat() == catSpace)
				continue;

			flags &= ~FLAG_ITEM;
			if (t.cat() == catBegin) {
				// skip the brace and collect everything to the next matching
				// closing brace
				flags |= FLAG_BRACE_LAST;
				continue;
			}

			// handle only this single token, leave the loop if done
			flags |= FLAG_LEAVE;
		}

		if (t.cat() != catEscape && t.character() == ']' &&
		    (flags & FLAG_BRACK_LAST))
			return;
		if (t.cat() == catEnd && (flags & FLAG_BRACE_LAST))
			return;
		string tok = t.asInput();
		// we only support delimiters with max 2 chars for now.
		if (rdelim.size() > 1)
			tok += p.next_token().asInput();
		if (t.cat() != catEscape && !rdelim.empty()
		    && tok == rdelim && (flags & FLAG_RDELIM)
		    && (rdelimesc.empty() || p.prev_token().asInput() != rdelimesc)) {
		    	if (rdelim.size() > 1)
		    		p.get_token(); // eat rdelim
			return;
		}

		// If there is anything between \end{env} and \begin{env} we
		// don't need to output a separator.
		if (t.cat() != catSpace && t.cat() != catNewline &&
		    t.asInput() != "\\begin")
			last_env = "";

		//
		// cat codes
		//
		have_cycled = true;
		bool const starred = p.next_token().asInput() == "*";
		string const starredname(starred ? (t.cs() + '*') : t.cs());
		if (t.cat() == catMath) {
			// we are inside some text mode thingy, so opening new math is allowed
			context.check_layout(os);
			begin_inset(os, "Formula ");
			Token const & n = p.get_token();
			bool const display(n.cat() == catMath && outer);
			if (display) {
				// TeX's $$...$$ syntax for displayed math
				os << "\\[";
				parse_math(p, os, FLAG_SIMPLE, MATH_MODE);
				os << "\\]";
				p.get_token(); // skip the second '$' token
			} else {
				// simple $...$  stuff
				p.putback();
				os << '$';
				parse_math(p, os, FLAG_SIMPLE, MATH_MODE);
				os << '$';
			}
			end_inset(os);
			if (display) {
				// Prevent the conversion of a line break to a
				// space (bug 7668). This does not change the
				// output, but looks ugly in LyX.
				eat_whitespace(p, os, context, false);
			}
			continue;
		}

		if (t.cat() == catSuper || t.cat() == catSub) {
			string const cc = (t.cat() == catSuper) ? "catSuper" : "catSub";
			warning_message("catcode " + cc + " illegal in text mode");
			continue;
		}

		// Basic support for quotes. We try to disambiguate
		// quotes from the context (e.g., a left english quote is
		// the same as a right german quote...).
		// Try to make a smart guess about the side
		Token const prev = p.prev_token();
		bool const opening = (prev.cat() != catSpace && prev.character() != 0
				&& prev.character() != '\n' && prev.character() != '~');
		if (t.asInput() == "`" && p.next_token().asInput() == "`") {
			context.check_layout(os);
			begin_inset(os, "Quotes ");
			os << guessQuoteStyle("eld", opening);
			end_inset(os);
			p.get_token();
			skip_braces(p);
			continue;
		}
		if (t.asInput() == "'" && p.next_token().asInput() == "'") {
			context.check_layout(os);
			begin_inset(os, "Quotes ");
			os << guessQuoteStyle("erd", opening);
			end_inset(os);
			p.get_token();
			skip_braces(p);
			continue;
		}

		if (t.asInput() == ">" && p.next_token().asInput() == ">") {
			context.check_layout(os);
			begin_inset(os, "Quotes ");
			os << guessQuoteStyle("ald", opening);
			end_inset(os);
			p.get_token();
			skip_braces(p);
			continue;
		}

		if (t.asInput() == "<"
			 && p.next_token().asInput() == "<") {
			bool has_chunk = false;
			if (noweb_mode) {
				p.pushPosition();
				p.get_token();
				has_chunk = parse_chunk(p, os, context);
				if (!has_chunk)
					p.popPosition();
			}

			if (!has_chunk) {
				context.check_layout(os);
				begin_inset(os, "Quotes ");
				os << guessQuoteStyle("ard", opening);
				end_inset(os);
				p.get_token();
				skip_braces(p);
			}
			continue;
		}

		if (t.cat() == catSpace || (t.cat() == catNewline && ! p.isParagraph())) {
			check_space(p, os, context);
			continue;
		}

		// babel shorthands (also used by polyglossia)
		// Since these can have different meanings for different languages
		// we import them as ERT (but they must be put in ERT to get output
		// verbatim).
		if (t.asInput() == "\"") {
			string s = "\"";
			// We put the known shorthand pairs together in
			// one ERT inset. In other cases (such as "a), only
			// the quotation mark is ERTed.
			if (is_known(p.next_token().asInput(), known_babel_shorthands)) {
				s += p.next_token().asInput();
				p.get_token();
			}
			output_ert_inset(os, s, context);
			continue;
		}

		if (t.character() == '[' && noweb_mode &&
			 p.next_token().character() == '[') {
			// These can contain underscores
			p.putback();
			string const s = p.getFullOpt() + ']';
			if (p.next_token().character() == ']')
				p.get_token();
			else
				warning_message("Inserting missing ']' in '" + s + "'.");
			output_ert_inset(os, s, context);
			continue;
		}

		if (t.cat() == catLetter) {
			context.check_layout(os);
			os << t.cs();
			continue;
		}

		if (t.cat() == catOther ||
			       t.cat() == catAlign ||
			       t.cat() == catParameter) {
			context.check_layout(os);
			if (t.asInput() == "-" && p.next_token().asInput() == "-" &&
			    context.merging_hyphens_allowed &&
			    context.font.family != "ttfamily" &&
			    !context.layout->pass_thru) {
				if (p.next_next_token().asInput() == "-") {
					// --- is emdash
					os << to_utf8(docstring(1, 0x2014));
					p.get_token();
				} else
					// -- is endash
					os << to_utf8(docstring(1, 0x2013));
				p.get_token();
			} else
				// This translates "&" to "\\&" which may be wrong...
				os << t.cs();
			continue;
		}

		if (p.isParagraph()) {
			// In minted floating listings we will collect
			// everything into the caption, where multiple
			// paragraphs are forbidden.
			if (minted_float.empty()) {
				if (context.new_layout_allowed)
					context.new_paragraph(os);
				else
					output_ert_inset(os, "\\par ", context);
			} else
				os << ' ';
			eat_whitespace(p, os, context, true);
			continue;
		}

		if (t.cat() == catActive) {
			context.check_layout(os);
			if (t.character() == '~') {
				if (context.layout->free_spacing)
					os << ' ';
				else {
					begin_inset(os, "space ~\n");
					end_inset(os);
				}
			} else
				os << t.cs();
			continue;
		}

		if (t.cat() == catBegin) {
			Token const next = p.next_token();
			Token const end = p.next_next_token();
			if (next.cat() == catEnd) {
				// {}
				Token const prev = p.prev_token();
				p.get_token();
				if (p.next_token().character() == '`')
					; // ignore it in {}``
				else
					output_ert_inset(os, "{}", context);
			} else if (next.cat() == catEscape &&
			           is_known(next.cs(), known_quotes) &&
			           end.cat() == catEnd) {
				// Something like {\textquoteright} (e.g.
				// from writer2latex). We may skip the
				// braces here for better readability.
				parse_text_snippet(p, os, FLAG_BRACE_LAST,
				                   outer, context);
			} else if (p.next_token().asInput() == "\\ascii") {
				// handle the \ascii characters
				// (the case without braces is handled later)
				// the code is "{\ascii\xxx}"
				p.get_token(); // eat \ascii
				string name2 = p.get_token().asInput();
				p.get_token(); // eat the final '}'
				string const name = "{\\ascii" + name2 + "}";
				bool termination;
				docstring rem;
				set<string> req;
				// get the character from unicodesymbols
				docstring s = encodings.fromLaTeXCommand(from_utf8(name),
					Encodings::TEXT_CMD, termination, rem, &req);
				if (!s.empty()) {
					context.check_layout(os);
					os << to_utf8(s);
					if (!rem.empty())
						output_ert_inset(os,
							to_utf8(rem), context);
					for (set<string>::const_iterator it = req.begin();
					     it != req.end(); ++it)
						preamble.registerAutomaticallyLoadedPackage(*it);
				} else
					// we did not find a non-ert version
					output_ert_inset(os, name, context);
			} else {
			context.check_layout(os);
			// special handling of font attribute changes
			Token const prev = p.prev_token();
			TeXFont const oldFont = context.font;
			if (next.character() == '[' ||
			    next.character() == ']' ||
			    next.character() == '*') {
				p.get_token();
				if (p.next_token().cat() == catEnd) {
					os << next.cs();
					p.get_token();
				} else {
					p.putback();
					output_ert_inset(os, "{", context);
					parse_text_snippet(p, os,
							FLAG_BRACE_LAST,
							outer, context);
					output_ert_inset(os, "}", context);
				}
			} else if (! context.new_layout_allowed) {
				output_ert_inset(os, "{", context);
				parse_text_snippet(p, os, FLAG_BRACE_LAST,
						   outer, context);
				output_ert_inset(os, "}", context);
			} else if (is_known(next.cs(), known_sizes)) {
				// next will change the size, so we must
				// reset it here
				parse_text_snippet(p, os, FLAG_BRACE_LAST,
						   outer, context);
				if (!context.atParagraphStart())
					os << "\n\\size "
					   << context.font.size << "\n";
			} else if (is_known(next.cs(), known_font_families)) {
				// next will change the font family, so we
				// must reset it here
				parse_text_snippet(p, os, FLAG_BRACE_LAST,
						   outer, context);
				if (!context.atParagraphStart())
					os << "\n\\family "
					   << context.font.family << "\n";
			} else if (is_known(next.cs(), known_font_series)) {
				// next will change the font series, so we
				// must reset it here
				parse_text_snippet(p, os, FLAG_BRACE_LAST,
						   outer, context);
				if (!context.atParagraphStart())
					os << "\n\\series "
					   << context.font.series << "\n";
			} else if (is_known(next.cs(), known_font_shapes)) {
				// next will change the font shape, so we
				// must reset it here
				parse_text_snippet(p, os, FLAG_BRACE_LAST,
						   outer, context);
				if (!context.atParagraphStart())
					os << "\n\\shape "
					   << context.font.shape << "\n";
			} else if (is_known(next.cs(), known_old_font_families) ||
				   is_known(next.cs(), known_old_font_series) ||
				   is_known(next.cs(), known_old_font_shapes)) {
				// next will change the font family, series
				// and shape, so we must reset it here
				parse_text_snippet(p, os, FLAG_BRACE_LAST,
						   outer, context);
				if (!context.atParagraphStart())
					os <<  "\n\\family "
					   << context.font.family
					   << "\n\\series "
					   << context.font.series
					   << "\n\\shape "
					   << context.font.shape << "\n";
			} else {
				output_ert_inset(os, "{", context);
				parse_text_snippet(p, os, FLAG_BRACE_LAST,
						   outer, context);
				output_ert_inset(os, "}", context);
				}
			}
			continue;
		}

		if (t.cat() == catEnd) {
			if (flags & FLAG_BRACE_LAST) {
				return;
			}
			warning_message("stray '}' in text");
			output_ert_inset(os, "}", context);
			continue;
		}

		if (t.cat() == catComment) {
			parse_comment(p, os, t, context);
			continue;
		}

		//
		// control sequences
		//

		if (t.cs() == "(" || t.cs() == "[") {
			bool const simple = t.cs() == "(";
			context.check_layout(os);
			begin_inset(os, "Formula");
			os << " \\" << t.cs();
			parse_math(p, os, simple ? FLAG_SIMPLE2 : FLAG_EQUATION, MATH_MODE);
			os << '\\' << (simple ? ')' : ']');
			end_inset(os);
			if (!simple) {
				// Prevent the conversion of a line break to a
				// space (bug 7668). This does not change the
				// output, but looks ugly in LyX.
				eat_whitespace(p, os, context, false);
			}
			continue;
		}

		if (t.cs() == "begin") {
			parse_environment(p, os, outer, last_env,
			                  context);
			continue;
		}

		if (t.cs() == "end") {
			if (flags & FLAG_END) {
				// eat environment name
				string const name = p.getArg('{', '}');
				if (name != active_environment())
					warning_message("\\end{" + name + "} does not match \\begin{"
						+ active_environment() + "}");
				return;
			}
			p.error("found 'end' unexpectedly");
			continue;
		}

		// "item" by default, but could be something else
		if (t.cs() == context.layout->itemcommand()) {
			string s;
			if (context.layout->labeltype == LABEL_MANUAL) {
				// FIXME: This swallows comments, but we cannot use
				//        eat_whitespace() since we must not output
				//        anything before the item.
				p.skip_spaces(true);
				s = p.verbatimOption();
			} else
				p.skip_spaces(false);
			context.set_item();
			context.check_layout(os);
			if (context.has_item) {
				// An item in an unknown list-like environment
				// FIXME: Do this in check_layout()!
				context.has_item = false;
				string item = "\\" + context.layout->itemcommand();
				if (!p.hasOpt())
					item += " ";
				output_ert_inset(os, item, context);
			}
			if (context.layout->labeltype != LABEL_MANUAL)
				output_arguments(os, p, outer, false, "item", context,
					         context.layout->itemargs());
			if (!context.list_preamble.empty()) {
				// We have a list preamble. Output it here.
				begin_inset(os, "Argument listpreamble:1");
				os << "\nstatus collapsed\n\n"
				   << "\\begin_layout Plain Layout\n\n"
				   << rtrim(context.list_preamble)
				   << "\n\\end_layout";
				end_inset(os);
				context.list_preamble.clear();
			}
			if (!context.list_extra_stuff.empty()) {
				os << context.list_extra_stuff;
				context.list_extra_stuff.clear();
			}
			else if (!s.empty()) {
					// LyX adds braces around the argument,
					// so we need to remove them here.
					if (s.size() > 2 && s[0] == '{' &&
					    s[s.size()-1] == '}')
						s = s.substr(1, s.size()-2);
					// If the argument contains a space we
					// must put it into ERT: Otherwise LyX
					// would misinterpret the space as
					// item delimiter (bug 7663)
					if (contains(s, ' ')) {
						output_ert_inset(os, s, context);
					} else {
						Parser p2(s + ']');
						os << parse_text_snippet(p2,
							FLAG_BRACK_LAST, outer, context);
					}
					// The space is needed to separate the
					// item from the rest of the sentence.
					os << ' ';
					eat_whitespace(p, os, context, false);
				}
			continue;
		}

		if (t.cs() == "bibitem") {
			context.set_item();
			context.check_layout(os);
			eat_whitespace(p, os, context, false);
			string label = p.verbatimOption();
			pair<bool, string> lbl = convert_latexed_command_inset_arg(label);
			bool const literal = !lbl.first;
			label = literal ? subst(label, "\n", " ") : lbl.second;
			string lit = literal ? "\"true\"" : "\"false\"";
			string key = convert_literate_command_inset_arg(p.verbatim_item());
			begin_command_inset(os, "bibitem", "bibitem");
			os << "label \"" << label << "\"\n"
			   << "key \"" << key << "\"\n"
			   << "literal " << lit << "\n";
			end_inset(os);
			continue;
		}

		if (is_macro(p)) {
			// catch the case of \def\inputGnumericTable
			bool macro = true;
			if (t.cs() == "def") {
				Token second = p.next_token();
				if (second.cs() == "inputGnumericTable") {
					p.pushPosition();
					p.get_token();
					skip_braces(p);
					Token third = p.get_token();
					p.popPosition();
					if (third.cs() == "input") {
						p.get_token();
						skip_braces(p);
						p.get_token();
						string name = normalize_filename(p.verbatim_item());
						string const path = getMasterFilePath(true);
						// We want to preserve relative / absolute filenames,
						// therefore path is only used for testing
						// The file extension is in every case ".tex".
						// So we need to remove this extension and check for
						// the original one.
						name = removeExtension(name);
						if (!makeAbsPath(name, path).exists()) {
							char const * const Gnumeric_formats[] = {"gnumeric",
								"ods", "xls", 0};
							string const Gnumeric_name =
								find_file(name, path, Gnumeric_formats);
							if (!Gnumeric_name.empty())
								name = Gnumeric_name;
						}
						FileName const absname = makeAbsPath(name, path);
						if (absname.exists()) {
							fix_child_filename(name);
							copy_file(absname, name);
						} else
							warning_message("Could not find file '" + name + "'.");
						context.check_layout(os);
						begin_inset(os, "External\n\ttemplate ");
						os << "GnumericSpreadsheet\n\tfilename "
						   << name << "\n";
						end_inset(os);
						context.check_layout(os);
						macro = false;
						// register the packages that are automatically loaded
						// by the Gnumeric template
						registerExternalTemplatePackages("GnumericSpreadsheet");
					}
				}
			}
			if (macro)
				parse_macro(p, os, context);
			continue;
		}

		if (t.cs() == "noindent") {
			p.skip_spaces();
			context.add_par_extra_stuff("\\noindent\n");
			continue;
		}

		if (t.cs() == "appendix" && !context.in_list_preamble) {
			context.add_par_extra_stuff("\\start_of_appendix\n");
			// We need to start a new paragraph. Otherwise the
			// appendix in 'bla\appendix\chapter{' would start
			// too late.
			context.new_paragraph(os);
			// We need to make sure that the paragraph is
			// generated even if it is empty. Otherwise the
			// appendix in '\par\appendix\par\chapter{' would
			// start too late.
			context.check_layout(os);
			// FIXME: This is a hack to prevent paragraph
			// deletion if it is empty. Handle this better!
			output_comment(p, os,
				"dummy comment inserted by tex2lyx to "
				"ensure that this paragraph is not empty",
				context);
			// Both measures above may generate an additional
			// empty paragraph, but that does not hurt, because
			// whitespace does not matter here.
			eat_whitespace(p, os, context, true);
			continue;
		}

		// Must catch empty dates before findLayout is called below
		if (t.cs() == "date") {
			eat_whitespace(p, os, context, false);
			p.pushPosition();
			string const date = p.verbatim_item();
			p.popPosition();
			if (date.empty()) {
				preamble.suppressDate(true);
				p.verbatim_item();
			} else {
				preamble.suppressDate(false);
				if (context.new_layout_allowed &&
				    (newlayout = findLayout(context.textclass,
				                            t.cs(), true))) {
					// write the layout
					output_command_layout(os, p, outer,
							context, newlayout);
					parse_text_snippet(p, os, FLAG_ITEM, outer, context);
					if (!preamble.titleLayoutFound())
						preamble.titleLayoutFound(newlayout->intitle);
					set<string> const & req = newlayout->required();
					set<string>::const_iterator it = req.begin();
					set<string>::const_iterator en = req.end();
					for (; it != en; ++it)
						preamble.registerAutomaticallyLoadedPackage(*it);
				} else
					output_ert_inset(os,
						"\\date{" + p.verbatim_item() + '}',
						context);
			}
			continue;
		}

		// Before we look for the layout name with star and alone below, we check the layouts including
		// the LateXParam, which might be one or several options or a star.
		// The single '=' is meant here.
		if (context.new_layout_allowed &&
		   (newlayout = findLayout(context.textclass, t.cs(), true, p.getCommandLatexParam()))) {
			// store the latexparam here. This is eaten in output_command_layout
			context.latexparam = newlayout->latexparam();
			// write the layout
			output_command_layout(os, p, outer, context, newlayout);
			context.latexparam.clear();
			p.skip_spaces();
			if (!preamble.titleLayoutFound())
				preamble.titleLayoutFound(newlayout->intitle);
			set<string> const & req = newlayout->required();
			for (set<string>::const_iterator it = req.begin(); it != req.end(); ++it)
				preamble.registerAutomaticallyLoadedPackage(*it);
			continue;
		}

		// Starred section headings
		// Must attempt to parse "Section*" before "Section".
		if ((p.next_token().asInput() == "*") &&
			 context.new_layout_allowed &&
			 (newlayout = findLayout(context.textclass, t.cs() + '*', true))) {
			// write the layout
			p.get_token();
			output_command_layout(os, p, outer, context, newlayout);
			p.skip_spaces();
			if (!preamble.titleLayoutFound())
				preamble.titleLayoutFound(newlayout->intitle);
			set<string> const & req = newlayout->required();
			for (set<string>::const_iterator it = req.begin(); it != req.end(); ++it)
				preamble.registerAutomaticallyLoadedPackage(*it);
			continue;
		}

		// Section headings and the like
		if (context.new_layout_allowed &&
			 (newlayout = findLayout(context.textclass, t.cs(), true))) {
			// write the layout
			output_command_layout(os, p, outer, context, newlayout);
			p.skip_spaces();
			if (!preamble.titleLayoutFound())
				preamble.titleLayoutFound(newlayout->intitle);
			set<string> const & req = newlayout->required();
			for (set<string>::const_iterator it = req.begin(); it != req.end(); ++it)
				preamble.registerAutomaticallyLoadedPackage(*it);
			continue;
		}

		if (t.cs() == "subfloat") {
			// the syntax is \subfloat[list entry][sub caption]{content}
			// if it is a table of figure depends on the surrounding float
			p.skip_spaces();
			// do nothing if there is no outer float
			if (!float_type.empty()) {
				context.check_layout(os);
				p.skip_spaces();
				begin_inset(os, "Float " + float_type + "\n");
				os << "wide false"
				   << "\nsideways false"
				   << "\nstatus collapsed\n\n";
				// test for caption
				string caption;
				bool has_caption = false;
				if (p.next_token().cat() != catEscape &&
						p.next_token().character() == '[') {
							p.get_token(); // eat '['
							caption = parse_text_snippet(p, FLAG_BRACK_LAST, outer, context);
							has_caption = true;
				}
				// In case we have two optional args, the second is the caption.
				if (p.next_token().cat() != catEscape &&
						p.next_token().character() == '[') {
							p.get_token(); // eat '['
							caption = parse_text_snippet(p, FLAG_BRACK_LAST, outer, context);
				}
				// the content
				parse_text_in_inset(p, os, FLAG_ITEM, outer, context);
				// the caption comes always as the last
				if (has_caption) {
					// we must make sure that the caption gets a \begin_layout
					os << "\n\\begin_layout Plain Layout";
					p.skip_spaces();
					begin_inset(os, "Caption Standard\n");
					Context newcontext(true, context.textclass,
					                   0, 0, context.font);
					newcontext.check_layout(os);
					os << caption << "\n";
					newcontext.check_end_layout(os);
					end_inset(os);
					p.skip_spaces();
					// close the layout we opened
					os << "\n\\end_layout";
				}
				end_inset(os);
				p.skip_spaces();
			} else {
				// if the float type is not supported or there is no surrounding float
				// output it as ERT
				string opt_arg1;
				string opt_arg2;
				if (p.hasOpt()) {
					opt_arg1 = convert_literate_command_inset_arg(p.getFullOpt());
					if (p.hasOpt())
						opt_arg2 = convert_literate_command_inset_arg(p.getFullOpt());
				}
				output_ert_inset(os, t.asInput() + opt_arg1 + opt_arg2
						 + "{" + p.verbatim_item() + '}', context);
			}
			continue;
		}

		if (t.cs() == "xymatrix") {
			// we must open a new math because LyX's xy support is in math
			context.check_layout(os);
			begin_inset(os, "Formula ");
			os << '$';
			os << "\\" << t.cs() << '{';
			parse_math(p, os, FLAG_ITEM, MATH_MODE);
			os << '}' << '$';
			end_inset(os);
			preamble.registerAutomaticallyLoadedPackage("xy");
			continue;
		}

		if (t.cs() == "includegraphics") {
			bool const clip = p.next_token().asInput() == "*";
			if (clip)
				p.get_token();
			string const arg = p.getArg('[', ']');
			map<string, string> opts;
			vector<string> keys;
			split_map(arg, opts, keys);
			if (clip)
				opts["clip"] = string();
			string name = normalize_filename(p.verbatim_item());

			string const path = getMasterFilePath(true);
			// We want to preserve relative / absolute filenames,
			// therefore path is only used for testing
			if (!makeAbsPath(name, path).exists()) {
				// The file extension is probably missing.
				// Now try to find it out.
				string const dvips_name =
					find_file(name, path,
						  known_dvips_graphics_formats);
				string const pdftex_name =
					find_file(name, path,
						  known_pdftex_graphics_formats);
				if (!dvips_name.empty()) {
					if (!pdftex_name.empty()) {
						warning_message("This file contains the "
								"latex snippet\n"
								"\"\\includegraphics{" + name + "}\".\n"
								"However, files\n\""
								+ dvips_name + "\" and\n\""
								+ pdftex_name + "\"\n"
								"both exist, so I had to make a "
								"choice and took the first one.\n"
								"Please move the unwanted one "
								"someplace else and try again\n"
								"if my choice was wrong.");
					}
					name = dvips_name;
				} else if (!pdftex_name.empty()) {
					name = pdftex_name;
					pdflatex = true;
				}
			}

			FileName const absname = makeAbsPath(name, path);
			if (absname.exists()) {
				fix_child_filename(name);
				copy_file(absname, name);
			} else
				warning_message("Could not find graphics file '" + name + "'.");

			context.check_layout(os);
			begin_inset(os, "Graphics ");
			os << "\n\tfilename " << name << '\n';
			if (opts.find("width") != opts.end())
				os << "\twidth "
				   << translate_len(opts["width"]) << '\n';
			if (opts.find("totalheight") != opts.end())
				os << "\theight "
				   << translate_len(opts["totalheight"]) << '\n';
			if (opts.find("scale") != opts.end()) {
				istringstream iss(opts["scale"]);
				double val;
				iss >> val;
				val = val*100;
				os << "\tscale " << val << '\n';
			}
			if (opts.find("angle") != opts.end()) {
				os << "\trotateAngle "
				   << opts["angle"] << '\n';
				vector<string>::const_iterator a =
					find(keys.begin(), keys.end(), "angle");
				vector<string>::const_iterator s =
					find(keys.begin(), keys.end(), "width");
				if (s == keys.end())
					s = find(keys.begin(), keys.end(), "totalheight");
				if (s == keys.end())
					s = find(keys.begin(), keys.end(), "scale");
				if (s != keys.end() && distance(s, a) > 0)
					os << "\tscaleBeforeRotation\n";
			}
			if (opts.find("origin") != opts.end()) {
				ostringstream ss;
				string const opt = opts["origin"];
				if (opt.find('l') != string::npos) ss << "left";
				if (opt.find('r') != string::npos) ss << "right";
				if (opt.find('c') != string::npos) ss << "center";
				if (opt.find('t') != string::npos) ss << "Top";
				if (opt.find('b') != string::npos) ss << "Bottom";
				if (opt.find('B') != string::npos) ss << "Baseline";
				if (!ss.str().empty())
					os << "\trotateOrigin " << ss.str() << '\n';
				else
					warning_message("Ignoring unknown includegraphics origin argument '" + opt + "'");
			}
			if (opts.find("keepaspectratio") != opts.end())
				os << "\tkeepAspectRatio\n";
			if (opts.find("clip") != opts.end())
				os << "\tclip\n";
			if (opts.find("draft") != opts.end())
				os << "\tdraft\n";
			if (opts.find("bb") != opts.end())
				os << "\tBoundingBox "
				   << opts["bb"] << '\n';
			int numberOfbbOptions = 0;
			if (opts.find("bbllx") != opts.end())
				numberOfbbOptions++;
			if (opts.find("bblly") != opts.end())
				numberOfbbOptions++;
			if (opts.find("bburx") != opts.end())
				numberOfbbOptions++;
			if (opts.find("bbury") != opts.end())
				numberOfbbOptions++;
			if (numberOfbbOptions == 4)
				os << "\tBoundingBox "
				   << opts["bbllx"] << " " << opts["bblly"] << " "
				   << opts["bburx"] << " " << opts["bbury"] << '\n';
			else if (numberOfbbOptions > 0)
				warning_message("Ignoring incomplete includegraphics boundingbox arguments.");
			numberOfbbOptions = 0;
			if (opts.find("natwidth") != opts.end())
				numberOfbbOptions++;
			if (opts.find("natheight") != opts.end())
				numberOfbbOptions++;
			if (numberOfbbOptions == 2)
				os << "\tBoundingBox 0bp 0bp "
				   << opts["natwidth"] << " " << opts["natheight"] << '\n';
			else if (numberOfbbOptions > 0)
				warning_message("Ignoring incomplete includegraphics boundingbox arguments.");
			ostringstream special;
			if (opts.find("hiresbb") != opts.end())
				special << "hiresbb,";
			if (opts.find("trim") != opts.end())
				special << "trim,";
			if (opts.find("viewport") != opts.end())
				special << "viewport=" << opts["viewport"] << ',';
			if (opts.find("height") != opts.end())
				special << "height=" << opts["height"] << ',';
			if (opts.find("type") != opts.end())
				special << "type=" << opts["type"] << ',';
			if (opts.find("ext") != opts.end())
				special << "ext=" << opts["ext"] << ',';
			if (opts.find("read") != opts.end())
				special << "read=" << opts["read"] << ',';
			if (opts.find("command") != opts.end())
				special << "command=" << opts["command"] << ',';
			string s_special = special.str();
			if (!s_special.empty()) {
				// We had special arguments. Remove the trailing ','.
				os << "\tspecial " << s_special.substr(0, s_special.size() - 1) << '\n';
			}
			// TODO: Handle the unknown settings better.
			// Warn about invalid options.
			// Check whether some option was given twice.
			end_inset(os);
			preamble.registerAutomaticallyLoadedPackage("graphicx");
			continue;
		}

		if (t.cs() == "footnote" ||
			 (t.cs() == "thanks" && context.layout->intitle)) {
			p.skip_spaces();
			context.check_layout(os);
			begin_inset(os, "Foot\n");
			os << "status collapsed\n\n";
			parse_text_in_inset(p, os, FLAG_ITEM, false, context);
			end_inset(os);
			continue;
		}

		if (t.cs() == "marginpar") {
			p.skip_spaces();
			context.check_layout(os);
			begin_inset(os, "Marginal\n");
			os << "status collapsed\n\n";
			parse_text_in_inset(p, os, FLAG_ITEM, false, context);
			end_inset(os);
			continue;
		}

		if (t.cs() == "lstinline" || t.cs() == "mintinline") {
			bool const use_minted = t.cs() == "mintinline";
			p.skip_spaces();
			parse_listings(p, os, context, true, use_minted);
			continue;
		}

		if (t.cs() == "ensuremath") {
			p.skip_spaces();
			context.check_layout(os);
			string const s = p.verbatim_item();
			//FIXME: this never triggers in UTF8
			if (s == "\xb1" || s == "\xb3" || s == "\xb2" || s == "\xb5")
				os << s;
			else
				output_ert_inset(os, "\\ensuremath{" + s + "}",
					   context);
			continue;
		}

		else if (t.cs() == "makeindex"
			 || ((t.cs() == "maketitle" || t.cs() == context.textclass.titlename())
			     && context.textclass.titletype() == TITLE_COMMAND_AFTER)) {
			if (preamble.titleLayoutFound()) {
				// swallow this
				skip_spaces_braces(p);
			} else
				output_ert_inset(os, t.asInput(), context);
			continue;
		}

		if (t.cs() == "tableofcontents"
				|| t.cs() == "lstlistoflistings"
				|| t.cs() == "listoflistings") {
			string name = t.cs();
			if (preamble.minted() && name == "listoflistings")
				name.insert(0, "lst");
			context.check_layout(os);
			begin_command_inset(os, "toc", name);
			end_inset(os);
			skip_spaces_braces(p);
			if (name == "lstlistoflistings") {
				if (preamble.minted())
					preamble.registerAutomaticallyLoadedPackage("minted");
				else
					preamble.registerAutomaticallyLoadedPackage("listings");
			}
			continue;
		}

		if (t.cs() == "listoffigures" || t.cs() == "listoftables") {
			context.check_layout(os);
			if (t.cs() == "listoffigures")
				begin_inset(os, "FloatList figure\n");
			else
				begin_inset(os, "FloatList table\n");
			end_inset(os);
			skip_spaces_braces(p);
			continue;
		}

		if (t.cs() == "listof") {
			p.skip_spaces(true);
			string const name = p.verbatim_item();
			if (context.textclass.floats().typeExist(name)) {
				context.check_layout(os);
				begin_inset(os, "FloatList ");
				os << name << "\n";
				end_inset(os);
				p.verbatim_item(); // swallow second arg
			} else
				output_ert_inset(os, "\\listof{" + name + "}", context);
			continue;
		}

		if (t.cs() == "theendnotes"
		   || (t.cs() == "printendnotes"
		       && p.next_token().asInput() != "*"
		       && !p.hasOpt())) {
			context.check_layout(os);
			begin_inset(os, "FloatList endnote\n");
			end_inset(os);
			skip_spaces_braces(p);
			continue;
		}

		if ((where = is_known(t.cs(), known_text_font_families))) {
			parse_text_attributes(p, os, FLAG_ITEM, outer,
				context, "\\family", context.font.family,
				known_coded_font_families[where - known_text_font_families]);
			continue;
		}

		// beamer has a \textbf<overlay>{} inset
		if (!p.hasOpt("<") && (where = is_known(t.cs(), known_text_font_series))) {
			parse_text_attributes(p, os, FLAG_ITEM, outer,
				context, "\\series", context.font.series,
				known_coded_font_series[where - known_text_font_series]);
			continue;
		}

		// beamer has a \textit<overlay>{} inset
		if (!p.hasOpt("<") && (where = is_known(t.cs(), known_text_font_shapes))) {
			parse_text_attributes(p, os, FLAG_ITEM, outer,
				context, "\\shape", context.font.shape,
				known_coded_font_shapes[where - known_text_font_shapes]);
			continue;
		}

		if (t.cs() == "textnormal" || t.cs() == "normalfont") {
			context.check_layout(os);
			TeXFont oldFont = context.font;
			context.font.init();
			context.font.size = oldFont.size;
			os << "\n\\family " << context.font.family << "\n";
			os << "\n\\series " << context.font.series << "\n";
			os << "\n\\shape " << context.font.shape << "\n";
			if (t.cs() == "textnormal") {
				parse_text_snippet(p, os, FLAG_ITEM, outer, context);
				output_font_change(os, context.font, oldFont);
				context.font = oldFont;
			} else
				eat_whitespace(p, os, context, false);
			continue;
		}

		if (t.cs() == "textcolor") {
			// scheme is \textcolor{color name}{text}
			string const color = p.verbatim_item();
			// we support the predefined colors of the color and the xcolor package
			string const lyxcolor = preamble.getLyXColor(color, true);
			if (!lyxcolor.empty()) {
				context.check_layout(os);
				os << "\n\\color " << lyxcolor << "\n";
				parse_text_snippet(p, os, FLAG_ITEM, outer, context);
				context.check_layout(os);
				os << "\n\\color inherit\n";
			} else
				output_ert_inset(os, t.asInput() + "{" + color + "}", context);
			continue;
		}

		if (t.cs() == "underbar" || t.cs() == "uline") {
			// \underbar is not 100% correct (LyX outputs \uline
			// of ulem.sty). The difference is that \ulem allows
			// line breaks, and \underbar does not.
			// Do NOT handle \underline.
			// \underbar cuts through y, g, q, p etc.,
			// \underline does not.
			context.check_layout(os);
			os << "\n\\bar under\n";
			parse_text_snippet(p, os, FLAG_ITEM, outer, context);
			context.check_layout(os);
			os << "\n\\bar default\n";
			preamble.registerAutomaticallyLoadedPackage("ulem");
			continue;
		}

		if (t.cs() == "sout") {
			context.check_layout(os);
			os << "\n\\strikeout on\n";
			parse_text_snippet(p, os, FLAG_ITEM, outer, context);
			context.check_layout(os);
			os << "\n\\strikeout default\n";
			preamble.registerAutomaticallyLoadedPackage("ulem");
			continue;
		}

		// beamer has an \emph<overlay>{} inset
		if ((t.cs() == "uuline" || t.cs() == "uwave"
		        || t.cs() == "emph" || t.cs() == "noun"
		        || t.cs() == "xout") && !p.hasOpt("<")) {
			context.check_layout(os);
			os << "\n\\" << t.cs() << " on\n";
			parse_text_snippet(p, os, FLAG_ITEM, outer, context);
			context.check_layout(os);
			os << "\n\\" << t.cs() << " default\n";
			if (t.cs() == "uuline" || t.cs() == "uwave" || t.cs() == "xout")
				preamble.registerAutomaticallyLoadedPackage("ulem");
			continue;
		}

		if (t.cs() == "lyxadded" || t.cs() == "lyxdeleted" || t.cs() == "lyxobjdeleted"
		    || t.cs() == "lyxdisplayobjdeleted" || t.cs() == "lyxudisplayobjdeleted") {
			context.check_layout(os);
			string initials;
			if (p.hasOpt()) {
				initials = p.getArg('[', ']');
			}
			string name = p.getArg('{', '}');
			string localtime = p.getArg('{', '}');
			preamble.registerAuthor(name, initials);
			Author const & author = preamble.getAuthor(name);
			// from_asctime_utc() will fail if LyX decides to output the
			// time in the text language.
			time_t ptime = from_asctime_utc(localtime);
			if (ptime == static_cast<time_t>(-1)) {
				warning_message("Could not parse time `" + localtime
				     + "´ for change tracking, using current time instead.");
				ptime = current_time();
			}
			if (t.cs() == "lyxadded")
				os << "\n\\change_inserted ";
			else
				os << "\n\\change_deleted ";
			os << author.bufferId() << ' ' << ptime << '\n';
			parse_text_snippet(p, os, FLAG_ITEM, outer, context);
			if (LaTeXPackages::isAvailable("ulem")
			    && LaTeXPackages::isAvailable("xcolor")) {
				preamble.registerAutomaticallyLoadedPackage("ulem");
				preamble.registerAutomaticallyLoadedPackage("xcolor");
			}
			continue;
		}

		if (t.cs() == "textipa") {
			context.check_layout(os);
			begin_inset(os, "IPA\n");
			bool merging_hyphens_allowed = context.merging_hyphens_allowed;
			context.merging_hyphens_allowed = false;
			set<string> pass_thru_cmds = context.pass_thru_cmds;
			// These commands have special meanings in IPA
			context.pass_thru_cmds.insert("!");
			context.pass_thru_cmds.insert(";");
			context.pass_thru_cmds.insert(":");
			parse_text_in_inset(p, os, FLAG_ITEM, outer, context);
			context.pass_thru_cmds = pass_thru_cmds;
			context.merging_hyphens_allowed = merging_hyphens_allowed;
			end_inset(os);
			preamble.registerAutomaticallyLoadedPackage("tipa");
			preamble.registerAutomaticallyLoadedPackage("tipx");
			continue;
		}

		if ((preamble.isPackageUsed("tipa") && t.cs() == "t" && p.next_token().asInput() == "*")
		    || t.cs() == "texttoptiebar" || t.cs() == "textbottomtiebar") {
			context.check_layout(os);
			if (t.cs() == "t")
				// swallow star
				p.get_token();
			string const type = (t.cs() == "t") ? "bottomtiebar" : t.cs().substr(4);
			begin_inset(os, "IPADeco " + type + "\n");
			os << "status open\n";
			parse_text_in_inset(p, os, FLAG_ITEM, outer, context);
			end_inset(os);
			p.skip_spaces();
			continue;
		}

		if (t.cs() == "textvertline") {
			// FIXME: This is not correct, \textvertline is higher than |
			os << "|";
			skip_braces(p);
			continue;
		}

		if (t.cs() == "tone" ) {
			context.check_layout(os);
			// register the tone package
			preamble.registerAutomaticallyLoadedPackage("tone");
			string content = trimSpaceAndEol(p.verbatim_item());
			string command = t.asInput() + "{" + content + "}";
			// some tones can be detected by unicodesymbols, some need special code
			if (is_known(content, known_tones)) {
				os << "\\IPAChar " << command << "\n";
				continue;
			}
			// try to see whether the string is in unicodesymbols
			bool termination;
			docstring rem;
			set<string> req;
			docstring s = encodings.fromLaTeXCommand(from_utf8(command),
				Encodings::TEXT_CMD | Encodings::MATH_CMD,
				termination, rem, &req);
			if (!s.empty()) {
				os << to_utf8(s);
				if (!rem.empty())
					output_ert_inset(os, to_utf8(rem), context);
				for (set<string>::const_iterator it = req.begin();
				     it != req.end(); ++it)
					preamble.registerAutomaticallyLoadedPackage(*it);
			} else
				// we did not find a non-ert version
				output_ert_inset(os, command, context);
			continue;
		}

		if (t.cs() == "phantom" || t.cs() == "hphantom" ||
			     t.cs() == "vphantom") {
			context.check_layout(os);
			if (t.cs() == "phantom")
				begin_inset(os, "Phantom Phantom\n");
			if (t.cs() == "hphantom")
				begin_inset(os, "Phantom HPhantom\n");
			if (t.cs() == "vphantom")
				begin_inset(os, "Phantom VPhantom\n");
			os << "status open\n";
			parse_text_in_inset(p, os, FLAG_ITEM, outer, context,
			                    "Phantom");
			end_inset(os);
			continue;
		}

		if (t.cs() == "href") {
			context.check_layout(os);
			string target = convert_literate_command_inset_arg(p.verbatim_item());
			string name = p.verbatim_item();
			pair<bool, string> nm = convert_latexed_command_inset_arg(name);
			bool const literal = !nm.first;
			name = literal ? subst(name, "\n", " ") : nm.second;
			string lit = literal ? "\"true\"" : "\"false\"";
			string type;
			size_t i = target.find(':');
			if (i != string::npos) {
				type = target.substr(0, i + 1);
				if (type == "mailto:" || type == "file:")
					target = target.substr(i + 1);
				else if (target.find("://") == string::npos)
					type = "other";
				// handle the case that name is equal to target, except of "http(s)://"
				else if (target.substr(i + 3) == name && (type == "http:" || type == "https:"))
					target = name;
			}
			begin_command_inset(os, "href", "href");
			if (name != target)
				os << "name \"" << name << "\"\n";
			os << "target \"" << target << "\"\n";
			if (type == "mailto:" || type == "file:" || type == "other")
				os << "type \"" << type << "\"\n";
			os << "literal " << lit << "\n";
			end_inset(os);
			skip_spaces_braces(p);
			continue;
		}

		if (t.cs() == "lyxline") {
			// swallow size argument (it is not used anyway)
			p.getArg('{', '}');
			if (!context.atParagraphStart()) {
				// so our line is in the middle of a paragraph
				// we need to add a new line, lest this line
				// follow the other content on that line and
				// run off the side of the page
				// FIXME: This may create an empty paragraph,
				//        but without that it would not be
				//        possible to set noindent below.
				//        Fortunately LaTeX does not care
				//        about the empty paragraph.
				context.new_paragraph(os);
			}
			if (preamble.indentParagraphs()) {
				// we need to unindent, lest the line be too long
				context.add_par_extra_stuff("\\noindent\n");
			}
			context.check_layout(os);
			begin_command_inset(os, "line", "rule");
			os << "offset \"0.5ex\"\n"
			      "width \"100line%\"\n"
			      "height \"1pt\"\n";
			end_inset(os);
			continue;
		}

		if (t.cs() == "rule") {
			string const offset = (p.hasOpt() ? p.getArg('[', ']') : string());
			string const width = p.getArg('{', '}');
			string const thickness = p.getArg('{', '}');
			context.check_layout(os);
			begin_command_inset(os, "line", "rule");
			if (!offset.empty())
				os << "offset \"" << translate_len(offset) << "\"\n";
			os << "width \"" << translate_len(width) << "\"\n"
				  "height \"" << translate_len(thickness) << "\"\n";
			end_inset(os);
			continue;
		}

		// Handle refstyle first in order to to catch \eqref, because this
		// can also occur without refstyle. Only recognize these commands if
		// refstyle.sty was found in the preamble (otherwise \eqref
		// and user defined ref commands could be misdetected).
		// We uncapitalize the input in order to catch capitalized commands
		// such as \Eqref.
		if ((where = is_known(uncapitalize(t.cs()), known_refstyle_commands))
		     && preamble.crossrefPackage() == "refstyle") {
			string const cap = isCapitalized(t.cs()) ? "true" : "false";
			string plural = "false";
			// Catch the plural option [s]
			if (p.hasOpt()) {
				string const opt = p.getOpt();
				if (opt == "[s]")
					plural = "true";
				else {
					// LyX does not yet support other optional arguments of ref commands
					output_ert_inset(os, t.asInput() + opt + "{" +
					       p.verbatim_item() + '}', context);
					continue;
				}
			}
			context.check_layout(os);
			begin_command_inset(os, "ref", "formatted");
			os << "reference \"";
			os << known_refstyle_prefixes[where - known_refstyle_commands]
			   << ":";
			string arg = p.getArg('{', '}');
			// with refstyle, labels containing blanks are grouped
			// remove the grouping
			if (contains(arg, ' '))
				arg = ltrim(rtrim(arg, "}"), "{");
			if (contains(t.cs(), "range")) {
				string arg2 = p.getArg('{', '}');
				if (contains(arg2, ' '))
					arg2 = ltrim(rtrim(arg2, "}"), "{");
				if (!arg2.empty())
					arg += "," + arg2;
			}
			os << convert_literate_command_inset_arg(arg)
			   << "\"\n";
			os << "plural \"" << plural << "\"\n";
			os << "caps \"" << cap << "\"\n";
			os << "noprefix \"false\"\n";
			if (contains(t.cs(), "range"))
				os << "tuple \"range\"\n";
			else
				os << "tuple \"list\"\n";
			end_inset(os);
			preamble.registerAutomaticallyLoadedPackage("refstyle");
			continue;
		}

		// if refstyle is used, we must not convert \prettyref to a
		// formatted reference, since that would result in a refstyle command.
		if ((where = is_known(t.cs(), known_ref_commands))
		     && (t.cs() != "prettyref" || preamble.crossrefPackage() == "prettyref")
		     && (p.next_token().asInput() != "*" || is_known(t.cs(), known_starref_commands))) {
			bool starred = false;
			bool const caps = contains(t.cs(), 'C');
			bool const plural = suffixIs(t.cs(), "refs");
			if (p.next_token().asInput() == "*") {
				starred = true;
				p.get_token();
			}
			string const opt = p.getOpt();
			if (opt.empty()) {
				context.check_layout(os);
				begin_command_inset(os, "ref",
					known_coded_ref_commands[where - known_ref_commands]);
				string arg = p.getArg('{', '}');
				if (contains(t.cs(), "range")) {
					string arg2 = p.getArg('{', '}');
					if (!arg2.empty())
						arg += "," + arg2;
				}
				os << "reference \""
				   << convert_literate_command_inset_arg(arg)
				   << "\"\n";
				if (plural)
					os << "plural \"true\"\n";
				else
					os << "plural \"false\"\n";
				if (caps)
					os << "caps \"true\"\n";
				else
					os << "caps \"false\"\n";
				os << "noprefix \"false\"\n";
				if (starred)
					os << "nolink \"true\"\n";
				else
					os << "nolink \"false\"\n";
				if (contains(t.cs(), "range"))
					os << "tuple \"range\"\n";
				else
					os << "tuple \"list\"\n";
				end_inset(os);
				if (t.cs() == "vref" || t.cs() == "vpageref"
				    || t.cs() == "vrefrange" || t.cs() == "vpagerefrange")
					preamble.registerAutomaticallyLoadedPackage("varioref");
				else if (t.cs() == "prettyref")
					preamble.registerAutomaticallyLoadedPackage("prettyref");
				else if (contains(t.cs(), "cref") || contains(t.cs(), "Cref"))
					preamble.registerAutomaticallyLoadedPackage("cleveref");
			} else {
				// LyX does not yet support optional arguments of ref commands
				output_ert_inset(os, t.asInput() + opt + "{" +
						 p.verbatim_item() + '}', context);
			}
			continue;
		}

		if ((where = is_known(t.cs(), known_zref_commands))
		     && preamble.crossrefPackage() == "zref") {
			bool starred = false;
			bool caps = false;
			bool range = false;
			bool noname = false;
			bool page = false;
			if (p.next_token().asInput() == "*") {
				starred = true;
				p.get_token();
			}
			vector<string> const opts = getVectorFromString(p.getOpt());
			string options;
			bool first = true;
			for (auto const & opt : opts) {
				if (opt == "S")
					caps = true;
				if (opt == "range")
					range = true;
				if (opt == "noname" && t.cs() == "zcref")
					noname = true;
				if (opt == "page" && t.cs() == "zcref")
					page = true;
				else {
					if (!first)
						options += ",";
					options += opt;
					first = false;
				}
			}
			context.check_layout(os);
			string lyxname = known_coded_zref_commands[where - known_zref_commands];
			if (noname) 
				lyxname = (page) ? "pageref" : "ref";
			begin_command_inset(os, "ref", lyxname);
			string arg = p.getArg('{', '}');
			if (contains(t.cs(), "range")) {
				range = true;
				string arg2 = p.getArg('{', '}');
				if (!arg2.empty())
					arg += "," + arg2;
			}
			os << "reference \""
			   << convert_literate_command_inset_arg(arg)
			   << "\"\n";
			os << "plural \"false\"\n";
			if (caps)
				os << "caps \"true\"\n";
			else
				os << "caps \"false\"\n";
			os << "noprefix \"false\"\n";
			if (starred)
				os << "nolink \"true\"\n";
			else
				os << "nolink \"false\"\n";
			if (range)
				os << "tuple \"range\"\n";
			else
				os << "tuple \"list\"\n";
			end_inset(os);
			if (t.cs() == "zvref" || t.cs() == "zvpageref")
				preamble.registerAutomaticallyLoadedPackage("zref-vario");
			else
				preamble.registerAutomaticallyLoadedPackage("zref-clever");
			continue;
		}

		if (use_natbib &&
			 is_known(t.cs(), known_natbib_commands) &&
			 ((t.cs() != "citefullauthor" &&
			   t.cs() != "citeyear" &&
			   t.cs() != "citeyearpar") ||
			  p.next_token().asInput() != "*")) {
			context.check_layout(os);
			string command = t.cs();
			if (p.next_token().asInput() == "*") {
				command += '*';
				p.get_token();
			}
			if (command == "citefullauthor")
				// alternative name for "\\citeauthor*"
				command = "citeauthor*";

			// text before the citation
			string before;
			// text after the citation
			string after;
			get_cite_arguments(p, true, before, after);

			if (command == "cite") {
				// \cite without optional argument means
				// \citet, \cite with at least one optional
				// argument means \citep.
				if (before.empty() && after.empty())
					command = "citet";
				else
					command = "citep";
			}
			if (before.empty() && after == "[]")
				// avoid \citet[]{a}
				after.erase();
			else if (before == "[]" && after == "[]") {
				// avoid \citet[][]{a}
				before.erase();
				after.erase();
			}
			bool literal = false;
			pair<bool, string> aft;
			pair<bool, string> bef;
			// remove the brackets around after and before
			if (!after.empty()) {
				after.erase(0, 1);
				after.erase(after.length() - 1, 1);
				aft = convert_latexed_command_inset_arg(after);
				literal = !aft.first;
				after = literal ? subst(after, "\n", " ") : aft.second;
			}
			if (!before.empty()) {
				before.erase(0, 1);
				before.erase(before.length() - 1, 1);
				bef = convert_latexed_command_inset_arg(before);
				literal |= !bef.first;
				before = literal ? subst(before, "\n", " ") : bef.second;
				if (literal && !after.empty())
					after = subst(after, "\n", " ");
			}
			string lit = literal ? "\"true\"" : "\"false\"";
			begin_command_inset(os, "citation", command);
			os << "after " << '"' << after << '"' << "\n";
			os << "before " << '"' << before << '"' << "\n";
			os << "key \""
			   << convert_literate_command_inset_arg(p.verbatim_item())
			   << "\"\n"
			   << "literal " << lit << "\n";
			end_inset(os);
			// Need to set the cite engine if natbib is loaded by
			// the document class directly
			if (preamble.citeEngine() == "basic")
				preamble.citeEngine("natbib");
			continue;
		}

		if ((use_biblatex// normal biblatex
		     && is_known(t.cs(), known_biblatex_commands)
		     && (is_known(t.cs(), known_biblatex_star_commands)
			 || p.next_token().asInput() != "*"))
		     || (use_biblatex_natbib// biblatex-natbib
			 && (is_known(t.cs(), known_biblatex_commands)
			     || is_known(t.cs(), known_natbib_commands))
			 && (is_known(t.cs(), known_biblatex_star_commands)
			     || (t.cs() == "citet" || t.cs() == "Citet"
				 || t.cs() == "Citealt" || t.cs() == "citealp"
				 || t.cs() == "Citealp")
			     || p.next_token().asInput() != "*"))
		     || (use_biblatex_chicago// biblatex-chicago
			 && (is_known(t.cs(), known_biblatex_commands)
			     || is_known(t.cs(), known_biblatex_chicago_commands))
			 && (is_known(t.cs(), known_biblatex_star_commands)
			     || p.next_token().asInput() != "*"))
		     || ((use_biblatex || use_biblatex_natbib)// specific styles: apa, mla
			 && ((prefixIs(preamble.biblatexCiteStyle(), "apa")
			      && (t.cs() == "nptextcite" || t.cs() == "nptextcites"))
			     || (prefixIs(preamble.biblatexCiteStyle(), "mla")
				 && ((t.cs() == "headlesscite" && p.next_token().asInput() != "*")
				     || t.cs() == "autocite" || t.cs() == "Autocite"
				     || t.cs() == "autocites" || t.cs() == "Autocites"))))){

			context.check_layout(os);
			string command = t.cs();
			if (p.next_token().asInput() == "*") {
				command += '*';
				p.get_token();
			}

			bool const qualified = suffixIs(command, "s");
			if (qualified)
				command = rtrim(command, "s");

			// text before the citation
			string before;
			// text after the citation
			string after;
			get_cite_arguments(p, true, before, after, qualified);

			// These use natbib cmd names in LyX
			// for inter-citeengine compatibility
			if (command == "citeyear")
				command = "citebyear";
			else if (command == "cite*")
				command = "citeyear";
			else if (command == "textcite")
				command = "citet";
			else if (command == "Textcite")
				command = "Citet";
			else if (command == "parencite")
				command = "citep";
			else if (command == "Parencite")
				command = "Citep";
			else if (command == "parencite*")
				command = "citeyearpar";
			else if (command == "smartcite")
				command = "footcite";
			else if (command == "Smartcite")
				command = "Footcite";

			// MLA-specific commands
			if (prefixIs(preamble.biblatexCiteStyle(), "mla")) {
				if (prefixIs(command, "autocite"))
					command = "mla" + command;
				else if (prefixIs(command, "Autocite"))
					command = subst(command, "Auto", "Mlaauto");
				else if (command == "headlesscite")
					command = "autocite*";
			}

			string const emptyarg = qualified ? "()" : "[]";
			if (before.empty() && after == emptyarg)
				// avoid \cite[]{a}
				after.erase();
			else if (before == emptyarg && after == emptyarg) {
				// avoid \cite[][]{a}
				before.erase();
				after.erase();
			}
			bool literal = false;
			pair<bool, string> aft;
			pair<bool, string> bef;
			// remove the brackets around after and before
			if (!after.empty()) {
				after.erase(0, 1);
				after.erase(after.length() - 1, 1);
				aft = convert_latexed_command_inset_arg(after);
				literal = !aft.first;
				after = literal ? subst(after, "\n", " ") : aft.second;
			}
			if (!before.empty()) {
				before.erase(0, 1);
				before.erase(before.length() - 1, 1);
				bef = convert_latexed_command_inset_arg(before);
				literal |= !bef.first;
				before = literal ? subst(before, "\n", " ") : bef.second;
			}
			string keys, pretextlist, posttextlist;
			if (qualified) {
				vector<pair<string, string>> pres, posts, preslit, postslit;
				vector<string> lkeys;
				// text before the citation
				string lbefore, lbeforelit;
				// text after the citation
				string lafter, lafterlit;
				string lkey;
				pair<bool, string> laft, lbef;
				while (true) {
					get_cite_arguments(p, true, lbefore, lafter);
					// remove the brackets around after and before
					if (!lafter.empty()) {
						lafter.erase(0, 1);
						lafter.erase(lafter.length() - 1, 1);
						laft = convert_latexed_command_inset_arg(lafter);
						literal |= !laft.first;
						lafter = laft.second;
						lafterlit = subst(lafter, "\n", " ");
					}
					if (!lbefore.empty()) {
						lbefore.erase(0, 1);
						lbefore.erase(lbefore.length() - 1, 1);
						lbef = convert_latexed_command_inset_arg(lbefore);
						literal |= !lbef.first;
						lbefore = lbef.second;
						lbeforelit = subst(lbefore, "\n", " ");
					}
					if (lbefore.empty() && lafter == "[]") {
						// avoid \cite[]{a}
						lafter.erase();
						lafterlit.erase();
					}
					else if (lbefore == "[]" && lafter == "[]") {
						// avoid \cite[][]{a}
						lbefore.erase();
						lafter.erase();
						lbeforelit.erase();
						lafterlit.erase();
					}
					lkey = p.getArg('{', '}');
					if (lkey.empty())
						break;
					pres.push_back(make_pair(lkey, lbefore));
					preslit.push_back(make_pair(lkey, lbeforelit));
					posts.push_back(make_pair(lkey, lafter));
					postslit.push_back(make_pair(lkey, lafterlit));
					lkeys.push_back(lkey);
				}
				keys = convert_literate_command_inset_arg(getStringFromVector(lkeys));
				if (literal) {
					pres = preslit;
					posts = postslit;
				}
				for (auto const & ptl : pres) {
					if (!pretextlist.empty())
						pretextlist += '\t';
					pretextlist += ptl.first;
					if (!ptl.second.empty())
						pretextlist += " " + ptl.second;
				}
				for (auto const & potl : posts) {
					if (!posttextlist.empty())
						posttextlist += '\t';
					posttextlist += potl.first;
					if (!potl.second.empty())
						posttextlist += " " + potl.second;
				}
			} else
				keys = convert_literate_command_inset_arg(p.verbatim_item());
			if (literal) {
				if (!after.empty())
					after = subst(after, "\n", " ");
				if (!before.empty())
					before = subst(after, "\n", " ");
			}
			string lit = literal ? "\"true\"" : "\"false\"";
			begin_command_inset(os, "citation", command);
			os << "after " << '"' << after << '"' << "\n";
			os << "before " << '"' << before << '"' << "\n";
			os << "key \""
			   << keys
			   << "\"\n";
			if (!pretextlist.empty())
				os << "pretextlist " << '"'  << pretextlist << '"' << "\n";
			if (!posttextlist.empty())
				os << "posttextlist " << '"'  << posttextlist << '"' << "\n";
			os << "literal " << lit << "\n";
			end_inset(os);
			// Need to set the cite engine if biblatex is loaded by
			// the document class directly
			if (preamble.citeEngine() == "basic") {
				if (use_biblatex_natbib)
					preamble.citeEngine("biblatex-natbib");
				else if (use_biblatex_chicago)
					preamble.citeEngine("biblatex-chicago");
				else
					preamble.citeEngine("biblatex");
			}
			continue;
		}

		if (use_jurabib &&
			 is_known(t.cs(), known_jurabib_commands) &&
		         (t.cs() == "cite" || p.next_token().asInput() != "*")) {
			context.check_layout(os);
			string command = t.cs();
			if (p.next_token().asInput() == "*") {
				command += '*';
				p.get_token();
			}
			char argumentOrder = '\0';
			vector<string> const options =
				preamble.getPackageOptions("jurabib");
			if (find(options.begin(), options.end(),
				      "natbiborder") != options.end())
				argumentOrder = 'n';
			else if (find(options.begin(), options.end(),
					   "jurabiborder") != options.end())
				argumentOrder = 'j';

			// text before the citation
			string before;
			// text after the citation
			string after;
			get_cite_arguments(p, argumentOrder != 'j', before, after);

			string const citation = p.verbatim_item();
			if (!before.empty() && argumentOrder == '\0') {
				warning_message("Assuming argument order "
						"of jurabib version 0.6 for\n'"
						+ command + before + after + '{'
						+ citation + "}'.\n"
						"Add 'jurabiborder' to the jurabib "
						"package options if you used an\n"
						"earlier jurabib version.");
			}
			bool literal = false;
			pair<bool, string> aft;
			pair<bool, string> bef;
			// remove the brackets around after and before
			if (!after.empty()) {
				after.erase(0, 1);
				after.erase(after.length() - 1, 1);
				aft = convert_latexed_command_inset_arg(after);
				literal = !aft.first;
				after = literal ? subst(after, "\n", " ") : aft.second;
			}
			if (!before.empty()) {
				before.erase(0, 1);
				before.erase(before.length() - 1, 1);
				bef = convert_latexed_command_inset_arg(before);
				literal |= !bef.first;
				before = literal ? subst(before, "\n", " ") : bef.second;
				if (literal && !after.empty())
					after = subst(after, "\n", " ");
			}
			string lit = literal ? "\"true\"" : "\"false\"";
			begin_command_inset(os, "citation", command);
			os << "after " << '"' << after << "\"\n"
			   << "before " << '"' << before << "\"\n"
			   << "key " << '"' << citation << "\"\n"
			   << "literal " << lit << "\n";
			end_inset(os);
			// Need to set the cite engine if jurabib is loaded by
			// the document class directly
			if (preamble.citeEngine() == "basic")
				preamble.citeEngine("jurabib");
			continue;
		}

		if (t.cs() == "cite"
			|| t.cs() == "nocite") {
			context.check_layout(os);
			string after = p.getArg('[', ']');
			pair<bool, string> aft = convert_latexed_command_inset_arg(after);
			bool const literal = !aft.first;
			after = literal ? subst(after, "\n", " ") : aft.second;
			string lit = literal ? "\"true\"" : "\"false\"";
			string key = convert_literate_command_inset_arg(p.verbatim_item());
			// store the case that it is "\nocite{*}" to use it later for
			// the BibTeX inset
			if (key != "*") {
				begin_command_inset(os, "citation", t.cs());
				os << "after " << '"' << after << "\"\n"
				   << "key " << '"' << key << "\"\n"
				   << "literal " << lit << "\n";
				end_inset(os);
			} else if (t.cs() == "nocite")
				btprint = key;
			continue;
		}

		if (t.cs() == "index" ||
		    (t.cs() == "sindex" && preamble.use_indices() == "true")) {
			context.check_layout(os);
			string const arg = (t.cs() == "sindex" && p.hasOpt()) ?
				p.getArg('[', ']') : "";
			string const kind = arg.empty() ? "idx" : arg;
			parse_index_entry(p, os, context, kind);
			if (kind != "idx")
				preamble.registerAutomaticallyLoadedPackage("splitidx");
			continue;
		}

		if (t.cs() == "nomenclature") {
			context.check_layout(os);
			begin_inset(os, "Nomenclature\n");
			os << "status open\n";
			set<string> pass_thru_cmds = context.pass_thru_cmds;
			// These commands have special meanings in Nomenclature
			context.pass_thru_cmds.insert("%");
			if (contains(preamble.nomenclOpts(), "nomentbl"))
				parse_text_in_inset(p, os, FLAG_ITEM, outer, context, "Nomenclature:nomentbl");
			else
				parse_text_in_inset(p, os, FLAG_ITEM, outer, context, "Nomenclature");
			context.pass_thru_cmds = pass_thru_cmds;
			end_inset(os);
			preamble.registerAutomaticallyLoadedPackage("nomencl");
			continue;
		}

		if (t.cs() == "label") {
			context.check_layout(os);
			begin_command_inset(os, "label", "label");
			os << "name \""
			   << convert_literate_command_inset_arg(p.verbatim_item())
			   << "\"\n";
			end_inset(os);
			continue;
		}

		if (t.cs() == "lyxmintcaption") {
			string const pos = p.getArg('[', ']');
			if (pos == "t") {
				string const caption =
					parse_text_snippet(p, FLAG_ITEM, false,
							   context);
				minted_nonfloat_caption = "[t]" + caption;
			} else {
				// We already got the caption at the bottom,
				// so simply skip it.
				parse_text_snippet(p, FLAG_ITEM, false, context);
			}
			eat_whitespace(p, os, context, true);
			continue;
		}

		if (t.cs() == "printindex" || t.cs() == "printsubindex") {
			context.check_layout(os);
			string commandname = t.cs();
			bool star = false;
			if (p.next_token().asInput() == "*") {
				commandname += "*";
				star = true;
				p.get_token();
			}
			begin_command_inset(os, "index_print", commandname);
			string const indexname = p.getArg('[', ']');
			if (!star) {
				if (indexname.empty())
					os << "type \"idx\"\n";
				else
					os << "type \"" << indexname << "\"\n";
				os << "literal \"true\"\n";
			}
			end_inset(os);
			skip_spaces_braces(p);
			preamble.registerAutomaticallyLoadedPackage("makeidx");
			if (preamble.use_indices() == "true")
				preamble.registerAutomaticallyLoadedPackage("splitidx");
			continue;
		}

		if (t.cs() == "settowidth") {
			context.check_layout(os);
			string arg = p.getArg('{', '}');
			// we are only interested in this:
			if (arg == "\\nomlabelwidth") {
				printnomencl_textwidth = p.getArg('{', '}');
				eat_whitespace(p, os, context, false);
				continue;
			}
			// for any other length, do ERT
			string arg2 = p.getArg('{', '}');
			string const ert = t.asInput()
					+ '{' + arg + '}'
					+ '{' + arg2 + '}';
			output_ert_inset(os, ert, context);
			continue;
		}

		if (t.cs() == "printnomenclature") {
			string width = "";
			string width_type = "";
			context.check_layout(os);
			begin_command_inset(os, "nomencl_print", "printnomenclature");
			// case of a custom width
			if (p.hasOpt()) {
				width = p.getArg('[', ']');
				width = translate_len(width);
				width_type = "custom";
			}
			string label = convert_literate_command_inset_arg(p.getArg('{', '}'));
			if (!printnomencl_textwidth.empty()) {
				width_type = "textwidth";
				width = printnomencl_textwidth;
			}
			else if (label.empty() && width_type.empty())
				width_type = "none";
			os << "set_width \"" << width_type << "\"\n";
			if (width_type == "custom" || width_type == "textwidth")
				os << "width \"" << width << '\"';
			end_inset(os);
			skip_spaces_braces(p);
			preamble.registerAutomaticallyLoadedPackage("nomencl");
			continue;
		}

		if ((t.cs() == "textsuperscript" || t.cs() == "textsubscript")) {
			context.check_layout(os);
			begin_inset(os, "script ");
			os << t.cs().substr(4) << '\n';
			newinsetlayout = findInsetLayout(context.textclass, t.cs(), true);
			parse_text_in_inset(p, os, FLAG_ITEM, false, context, newinsetlayout);
			end_inset(os);
			if (t.cs() == "textsubscript")
				preamble.registerAutomaticallyLoadedPackage("subscript");
			continue;
		}

		if ((where = is_known(t.cs(), known_quotes))) {
			context.check_layout(os);
			begin_inset(os, "Quotes ");
			string quotetype = known_coded_quotes[where - known_quotes];
			// try to make a smart guess about the side
			Token const prev = p.prev_token();
			bool const opening = (prev.cat() != catSpace && prev.character() != 0
					&& prev.character() != '\n' && prev.character() != '~');
			quotetype = guessQuoteStyle(quotetype, opening);
			os << quotetype;
			end_inset(os);
			// LyX adds {} after the quote, so we have to eat
			// spaces here if there are any before a possible
			// {} pair.
			eat_whitespace(p, os, context, false);
			skip_braces(p);
			continue;
		}

		if ((where = is_known(t.cs(), known_sizes)) &&
			context.new_layout_allowed) {
			context.check_layout(os);
			TeXFont const oldFont = context.font;
			context.font.size = known_coded_sizes[where - known_sizes];
			output_font_change(os, oldFont, context.font);
			eat_whitespace(p, os, context, false);
			continue;
		}

		if ((where = is_known(t.cs(), known_font_families)) &&
			 context.new_layout_allowed) {
			context.check_layout(os);
			TeXFont const oldFont = context.font;
			context.font.family =
				known_coded_font_families[where - known_font_families];
			output_font_change(os, oldFont, context.font);
			eat_whitespace(p, os, context, false);
			continue;
		}

		if ((where = is_known(t.cs(), known_font_series)) &&
			 context.new_layout_allowed) {
			context.check_layout(os);
			TeXFont const oldFont = context.font;
			context.font.series =
				known_coded_font_series[where - known_font_series];
			output_font_change(os, oldFont, context.font);
			eat_whitespace(p, os, context, false);
			continue;
		}

		if ((where = is_known(t.cs(), known_font_shapes)) &&
			 context.new_layout_allowed) {
			context.check_layout(os);
			TeXFont const oldFont = context.font;
			context.font.shape =
				known_coded_font_shapes[where - known_font_shapes];
			output_font_change(os, oldFont, context.font);
			eat_whitespace(p, os, context, false);
			continue;
		}
		if ((where = is_known(t.cs(), known_old_font_families)) &&
			 context.new_layout_allowed) {
			context.check_layout(os);
			TeXFont const oldFont = context.font;
			context.font.init();
			context.font.size = oldFont.size;
			context.font.family =
				known_coded_font_families[where - known_old_font_families];
			output_font_change(os, oldFont, context.font);
			eat_whitespace(p, os, context, false);
			continue;
		}

		if ((where = is_known(t.cs(), known_old_font_series)) &&
			 context.new_layout_allowed) {
			context.check_layout(os);
			TeXFont const oldFont = context.font;
			context.font.init();
			context.font.size = oldFont.size;
			context.font.series =
				known_coded_font_series[where - known_old_font_series];
			output_font_change(os, oldFont, context.font);
			eat_whitespace(p, os, context, false);
			continue;
		}

		if ((where = is_known(t.cs(), known_old_font_shapes)) &&
			 context.new_layout_allowed) {
			context.check_layout(os);
			TeXFont const oldFont = context.font;
			context.font.init();
			context.font.size = oldFont.size;
			context.font.shape =
				known_coded_font_shapes[where - known_old_font_shapes];
			output_font_change(os, oldFont, context.font);
			eat_whitespace(p, os, context, false);
			continue;
		}

		if (t.cs() == "selectlanguage") {
			context.check_layout(os);
			// save the language for the case that a
			// \foreignlanguage is used
			context.font.language = babel2lyx(p.verbatim_item());
			os << "\n\\lang " << context.font.language << "\n";
			continue;
		}

		if (t.cs() == "foreignlanguage") {
			string const lang = babel2lyx(p.verbatim_item());
			parse_text_attributes(p, os, FLAG_ITEM, outer,
			                      context, "\\lang",
			                      context.font.language, lang);
			continue;
		}

		if (prefixIs(t.cs(), "text") && preamble.usePolyglossia()
			 && is_known(t.cs().substr(4), preamble.polyglossia_languages)) {
			// scheme is \textLANGUAGE{text} where LANGUAGE is in polyglossia_languages[]
			string lang;
			// We have to output the whole command if it has an option
			// because LyX doesn't support this yet, see bug #8214,
			// only if there is a single option specifying a variant, we can handle it.
			if (p.hasOpt()) {
				string langopts = p.getOpt();
				// check if the option contains a variant, if yes, extract it
				string::size_type pos_var = langopts.find("variant");
				string::size_type i = langopts.find(',');
				string::size_type k = langopts.find('=', pos_var);
				if (pos_var != string::npos && i == string::npos) {
					string variant;
					variant = langopts.substr(k + 1, langopts.length() - k - 2);
					lang = preamble.polyglossia2lyx(variant);
					parse_text_attributes(p, os, FLAG_ITEM, outer,
						                  context, "\\lang",
						                  context.font.language, lang);
				} else
					output_ert_inset(os, t.asInput() + langopts, context);
			} else {
				lang = preamble.polyglossia2lyx(t.cs().substr(4, string::npos));
				parse_text_attributes(p, os, FLAG_ITEM, outer,
					                  context, "\\lang",
					                  context.font.language, lang);
			}
			continue;
		}

		if (t.cs() == "inputencoding") {
			// nothing to write here
			string const enc = subst(p.verbatim_item(), "\n", " ");
			p.setEncoding(enc, Encoding::inputenc);
			continue;
		}

		if (is_known(t.cs(), known_special_chars) ||
		    (t.cs() == "protect" &&
		     p.next_token().cat() == catEscape &&
		     is_known(p.next_token().cs(), known_special_protect_chars))) {
			// LyX sometimes puts a \protect in front, so we have to ignore it
			where = is_known(
				t.cs() == "protect" ? p.get_token().cs() : t.cs(),
				known_special_chars);
			context.check_layout(os);
			os << known_coded_special_chars[where - known_special_chars];
			skip_spaces_braces(p);
			continue;
		}

		if ((t.cs() == "nobreakdash" && p.next_token().asInput() == "-") ||
		         (t.cs() == "protect" && p.next_token().asInput() == "\\nobreakdash" &&
		          p.next_next_token().asInput() == "-") ||
		         (t.cs() == "@" && p.next_token().asInput() == ".")) {
			// LyX sometimes puts a \protect in front, so we have to ignore it
			if (t.cs() == "protect")
				p.get_token();
			context.check_layout(os);
			if (t.cs() == "nobreakdash")
				os << "\\SpecialChar nobreakdash\n";
			else
				os << "\\SpecialChar endofsentence\n";
			p.get_token();
			continue;
		}

		if (t.cs() == "_" || t.cs() == "&" || t.cs() == "#"
			    || t.cs() == "$" || t.cs() == "{" || t.cs() == "}"
			    || t.cs() == "%" || t.cs() == "-") {
			context.check_layout(os);
			if (t.cs() == "-")
				os << "\\SpecialChar softhyphen\n";
			else
				os << t.cs();
			continue;
		}

		if (t.cs() == "char") {
			context.check_layout(os);
			if (p.next_token().character() == '`') {
				p.get_token();
				if (p.next_token().cs() == "\"") {
					p.get_token();
					os << '"';
					skip_braces(p);
				} else {
					output_ert_inset(os, "\\char`", context);
				}
			} else {
				output_ert_inset(os, "\\char", context);
			}
			continue;
		}

		if (t.cs() == "verb") {
			context.check_layout(os);
			// set catcodes to verbatim early, just in case.
			p.setCatcodes(VERBATIM_CATCODES);
			string delim = p.get_token().asInput();
			Parser::Arg arg = p.verbatimStuff(delim);
			if (arg.first)
				output_ert_inset(os, "\\verb" + delim
						 + arg.second + delim, context);
			else
				warning_message("invalid \\verb command. Skipping");
			continue;
		}

		// Problem: \= creates a tabstop inside the tabbing environment
		// and else an accent. In the latter case we really would want
		// \={o} instead of \= o.
		if (t.cs() == "=" && (flags & FLAG_TABBING)) {
			output_ert_inset(os, t.asInput(), context);
			continue;
		}

		if (t.cs() == "\\") {
			context.check_layout(os);
			if (p.hasOpt())
				output_ert_inset(os, "\\\\" + p.getOpt(), context);
			else if (p.next_token().asInput() == "*") {
				p.get_token();
				// getOpt() eats the following space if there
				// is no optional argument, but that is OK
				// here since it has no effect in the output.
				output_ert_inset(os, "\\\\*" + p.getOpt(), context);
			}
			else {
				begin_inset(os, "Newline newline");
				end_inset(os);
			}
			continue;
		}

		if (t.cs() == "newline" ||
		    (t.cs() == "linebreak" && !p.hasOpt())) {
			context.check_layout(os);
			begin_inset(os, "Newline ");
			os << t.cs();
			end_inset(os);
			skip_spaces_braces(p);
			continue;
		}

		if (t.cs() == "endgraf" && context.in_table_cell) {
			context.new_paragraph(os);
			context.check_layout(os);
			skip_spaces_braces(p);
			continue;
		}

		if (t.cs() == "input" || t.cs() == "include"
		    || t.cs() == "verbatiminput"
		    || t.cs() == "lstinputlisting"
		    || t.cs() == "inputminted") {
			string name = t.cs();
			if (name == "verbatiminput"
			    && p.next_token().asInput() == "*")
				name += p.get_token().asInput();
			context.check_layout(os);
			string lstparams;
			bool literal = false;
			if (name == "lstinputlisting" && p.hasOpt()) {
				lstparams = p.getArg('[', ']');
				pair<bool, string> oa = convert_latexed_command_inset_arg(lstparams);
				literal = !oa.first;
				if (literal)
					lstparams = subst(lstparams, "\n", " ");
				else
					lstparams = oa.second;
			} else if (name == "inputminted") {
				name = "lstinputlisting";
				string const lang = p.getArg('{', '}');
				if (lang != "tex") {
					string cmd = "\\inputminted{" + lang + "}{";
					cmd += p.getArg('{', '}') + "}";
					output_ert_inset(os, cmd, context);
					continue;
				}
				if (prefixIs(minted_nonfloat_caption, "[t]")) {
					minted_nonfloat_caption.erase(0,3);
					// extract label and caption from the already produced LyX code
					vector<string> nfc = getVectorFromString(minted_nonfloat_caption, "\n");
					string const caption = nfc.front();
					string label;
					vector<string>::iterator it =
						find(nfc.begin(), nfc.end(), "LatexCommand label");
					if (it != nfc.end()) {
						++it;
						if (it != nfc.end())
							label = *it;
						label = support::split(label, '"');
						label.pop_back();
					}
					minted_nonfloat_caption.clear();
					lstparams = "caption=" + caption;
					if (!label.empty())
						lstparams += ",label=" + label;
					pair<bool, string> oa = convert_latexed_command_inset_arg(lstparams);
					literal = !oa.first;
					if (literal)
						lstparams = subst(lstparams, "\n", " ");
					else
						lstparams = oa.second;
				}
			}
			string lit = literal ? "\"true\"" : "\"false\"";
			string filename(normalize_filename(p.getArg('{', '}')));
			string const path = getMasterFilePath(true);
			// We want to preserve relative / absolute filenames,
			// therefore path is only used for testing
			if ((t.cs() == "include" || t.cs() == "input") &&
			    !makeAbsPath(filename, path).exists()) {
				// The file extension is probably missing.
				// Now try to find it out.
				string const tex_name =
					find_file(filename, path,
						  known_tex_extensions);
				if (!tex_name.empty())
					filename = tex_name;
			}
			bool external = false;
			string outname;
			if (makeAbsPath(filename, path).exists()) {
				string const abstexname =
					makeAbsPath(filename, path).absFileName();
				string const absfigname =
					changeExtension(abstexname, ".fig");
				fix_child_filename(filename);
				string const lyxname = changeExtension(filename,
					roundtripMode() ? ".lyx.lyx" : ".lyx");
				string const abslyxname = makeAbsPath(
					lyxname, getParentFilePath(false)).absFileName();
				bool xfig = false;
				if (!skipChildren())
					external = FileName(absfigname).exists();
				if (t.cs() == "input" && !skipChildren()) {
					string const ext = getExtension(abstexname);

					// Combined PS/LaTeX:
					// x.eps, x.pstex_t (old xfig)
					// x.pstex, x.pstex_t (new xfig, e.g. 3.2.5)
					FileName const absepsname(
						changeExtension(abstexname, ".eps"));
					FileName const abspstexname(
						changeExtension(abstexname, ".pstex"));
					bool const xfigeps =
						(absepsname.exists() ||
						 abspstexname.exists()) &&
						ext == "pstex_t";

					// Combined PDF/LaTeX:
					// x.pdf, x.pdftex_t (old xfig)
					// x.pdf, x.pdf_t (new xfig, e.g. 3.2.5)
					FileName const abspdfname(
						changeExtension(abstexname, ".pdf"));
					bool const xfigpdf =
						abspdfname.exists() &&
						(ext == "pdftex_t" || ext == "pdf_t");
					if (xfigpdf)
						pdflatex = true;

					// Combined PS/PDF/LaTeX:
					// x_pspdftex.eps, x_pspdftex.pdf, x.pspdftex
					string const absbase2(
						removeExtension(abstexname) + "_pspdftex");
					FileName const abseps2name(
						addExtension(absbase2, ".eps"));
					FileName const abspdf2name(
						addExtension(absbase2, ".pdf"));
					bool const xfigboth =
						abspdf2name.exists() &&
						abseps2name.exists() && ext == "pspdftex";

					xfig = xfigpdf || xfigeps || xfigboth;
					external = external && xfig;
				}
				if (external) {
					outname = changeExtension(filename, ".fig");
					FileName abssrc(changeExtension(abstexname, ".fig"));
					copy_file(abssrc, outname);
				} else if (xfig) {
					// Don't try to convert, the result
					// would be full of ERT.
					outname = filename;
					FileName abssrc(abstexname);
					copy_file(abssrc, outname);
				} else if (t.cs() != "verbatiminput" &&
				           !skipChildren() &&
				    tex2lyx(abstexname, FileName(abslyxname),
					    p.getEncoding())) {
					outname = lyxname;
					// no need to call copy_file
					// tex2lyx creates the file
				} else {
					outname = filename;
					FileName abssrc(abstexname);
					copy_file(abssrc, outname);
				}
			} else {
				warning_message("Could not find included file '" + filename + "'.");
				outname = filename;
			}
			if (external) {
				begin_inset(os, "External\n");
				os << "\ttemplate XFig\n"
				   << "\tfilename " << outname << '\n';
				registerExternalTemplatePackages("XFig");
			} else {
				begin_command_inset(os, "include", name);
				outname = subst(outname, "\"", "\\\"");
				os << "preview false\n"
				      "filename \"" << outname << "\"\n";
				if (!lstparams.empty())
					os << "lstparams \"" << lstparams << "\"\n";
				os << "literal " << lit << "\n";
				if (t.cs() == "verbatiminput")
					preamble.registerAutomaticallyLoadedPackage("verbatim");
			}
			end_inset(os);
			continue;
		}

		if (t.cs() == "bibliographystyle") {
			// store new bibliographystyle
			bibliographystyle = p.verbatim_item();
			// If any other command than \bibliography, \addcontentsline
			// and \nocite{*} follows, we need to output the style
			// (because it might be used by that command).
			// Otherwise, it will automatically be output by LyX.
			p.pushPosition();
			bool output = true;
			for (Token t2 = p.get_token(); p.good(); t2 = p.get_token()) {
				if (t2.cat() == catBegin)
					break;
				if (t2.cat() != catEscape)
					continue;
				if (t2.cs() == "nocite") {
					if (p.getArg('{', '}') == "*")
						continue;
				} else if (t2.cs() == "bibliography")
					output = false;
				else if (t2.cs() == "phantomsection") {
					output = false;
					continue;
				}
				else if (t2.cs() == "addcontentsline") {
					// get the 3 arguments of \addcontentsline
					p.getArg('{', '}');
					p.getArg('{', '}');
					contentslineContent = p.getArg('{', '}');
					// if the last argument is not \refname we must output
					if (contentslineContent == "\\refname")
						output = false;
				}
				break;
			}
			p.popPosition();
			if (output) {
				output_ert_inset(os,
					"\\bibliographystyle{" + bibliographystyle + '}',
					context);
			}
			continue;
		}

		if (t.cs() == "phantomsection") {
			// we only support this if it occurs between
			// \bibliographystyle and \bibliography
			if (bibliographystyle.empty())
				output_ert_inset(os, "\\phantomsection", context);
			continue;
		}

		if (t.cs() == "addcontentsline") {
			context.check_layout(os);
			// get the 3 arguments of \addcontentsline
			string const one = p.getArg('{', '}');
			string const two = p.getArg('{', '}');
			string const three = p.getArg('{', '}');
			// only if it is a \refname, we support if for the bibtex inset
			if (contentslineContent != "\\refname") {
				output_ert_inset(os,
					"\\addcontentsline{" + one + "}{" + two + "}{"+ three + '}',
					context);
			}
			continue;
		}

		if (t.cs() == "tikzset") {
			context.check_layout(os);
			// we catch the argument and output it verbatim in an ERT
			string const arg = p.getArg('{', '}');
			output_ert_inset(os,
				"\\tikzset{" + arg + "}",
				context);
			continue;
		}

		else if (t.cs() == "bibliography") {
			context.check_layout(os);
			string BibOpts;
			begin_command_inset(os, "bibtex", "bibtex");
			if (!btprint.empty()) {
				os << "btprint " << '"' << "btPrintAll" << '"' << "\n";
				// clear the string because the next BibTeX inset can be without the
				// \nocite{*} option
				btprint.clear();
			}
			os << "bibfiles " << '"' << normalize_filename(p.verbatim_item()) << '"' << "\n";
			// Do we have addcontentsline?
			if (contentslineContent == "\\refname") {
				BibOpts = "bibtotoc";
				// clear string because next BibTeX inset can be without addcontentsline
				contentslineContent.clear();
			}
			// Do we have a bibliographystyle set?
			if (!bibliographystyle.empty()) {
				if (BibOpts.empty())
					BibOpts = normalize_filename(bibliographystyle);
				else
					BibOpts = BibOpts + ',' + normalize_filename(bibliographystyle);
				// clear it because each bibtex entry has its style
				// and we need an empty string to handle \phantomsection
				bibliographystyle.clear();
			}
			os << "options " << '"' << BibOpts << '"' << "\n";
			if (p.getEncoding() != preamble.docencoding) {
				Encoding const * const enc = encodings.fromIconvName(
					p.getEncoding(), Encoding::inputenc, true);
				if (!enc) {
					warning_message("Unknown bib encoding " + p.getEncoding()
					     + ". Ignoring.");
				} else
					os << "encoding " << '"' << enc->name() << '"' << "\n";
			}
			end_inset(os);
			continue;
		}

		if (t.cs() == "printbibliography") {
			context.check_layout(os);
			string BibOpts;
			string bbloptions = p.hasOpt() ? p.getArg('[', ']') : string();
			vector<string> opts = getVectorFromString(bbloptions);
			vector<string>::iterator it =
				find(opts.begin(), opts.end(), "heading=bibintoc");
			if (it != opts.end()) {
				opts.erase(it);
				BibOpts = "bibtotoc";
			}
			bbloptions = getStringFromVector(opts);
			begin_command_inset(os, "bibtex", "bibtex");
			if (!btprint.empty()) {
				os << "btprint " << '"' << "btPrintAll" << '"' << "\n";
				// clear the string because the next BibTeX inset can be without the
				// \nocite{*} option
				btprint.clear();
			}
			string bibfiles;
			for (auto const & bf : preamble.biblatex_bibliographies) {
				if (!bibfiles.empty())
					bibfiles += ",";
				bibfiles += normalize_filename(bf);
			}
			if (!bibfiles.empty())
				os << "bibfiles " << '"' << bibfiles << '"' << "\n";
			// Do we have addcontentsline?
			if (contentslineContent == "\\refname") {
				BibOpts = "bibtotoc";
				// clear string because next BibTeX inset can be without addcontentsline
				contentslineContent.clear();
			}
			os << "options " << '"' << BibOpts << '"' << "\n";
			if (!bbloptions.empty())
				os << "biblatexopts " << '"' << bbloptions << '"' << "\n";
			if (!preamble.bibencoding.empty()) {
				Encoding const * const enc = encodings.fromLaTeXName(
					preamble.bibencoding, Encoding::inputenc, true);
				if (!enc) {
					warning_message("Unknown bib encoding " + preamble.bibencoding
					     + ". Ignoring.");
				} else
					os << "encoding " << '"' << enc->name() << '"' << "\n";
			}
			string bibfileencs;
			for (auto const & bf : preamble.biblatex_encodings) {
				if (!bibfileencs.empty())
					bibfileencs += "\t";
				bibfileencs += bf;
			}
			if (!bibfileencs.empty())
				os << "file_encodings " << '"' << bibfileencs << '"' << "\n";
			end_inset(os);
			need_commentbib = false;
			continue;
		}

		if (t.cs() == "bibbysection") {
			context.check_layout(os);
			string BibOpts;
			string bbloptions = p.hasOpt() ? p.getArg('[', ']') : string();
			vector<string> opts = getVectorFromString(bbloptions);
			vector<string>::iterator it =
				find(opts.begin(), opts.end(), "heading=bibintoc");
			if (it != opts.end()) {
				opts.erase(it);
				BibOpts = "bibtotoc";
			}
			bbloptions = getStringFromVector(opts);
			begin_command_inset(os, "bibtex", "bibtex");
			os << "btprint " << '"' << "bibbysection" << '"' << "\n";
			string bibfiles;
			for (auto const & bf : preamble.biblatex_bibliographies) {
				if (!bibfiles.empty())
					bibfiles += ",";
				bibfiles += normalize_filename(bf);
			}
			if (!bibfiles.empty())
				os << "bibfiles " << '"' << bibfiles << '"' << "\n";
			os << "options " << '"' << BibOpts << '"' << "\n";
			if (!bbloptions.empty())
				os << "biblatexopts " << '"' << bbloptions << '"' << "\n";
			end_inset(os);
			need_commentbib = false;
			continue;
		}

		if (t.cs() == "parbox") {
			// Test whether this is an outer box of a shaded box
			p.pushPosition();
			// swallow arguments
			while (p.hasOpt()) {
				p.getArg('[', ']');
				p.skip_spaces(true);
			}
			p.getArg('{', '}');
			p.skip_spaces(true);
			// eat the '{'
			if (p.next_token().cat() == catBegin)
				p.get_token();
			p.skip_spaces(true);
			Token to = p.get_token();
			bool shaded = false;
			if (to.asInput() == "\\begin") {
				p.skip_spaces(true);
				if (p.getArg('{', '}') == "shaded")
					shaded = true;
			}
			p.popPosition();
			if (shaded) {
				parse_outer_box(p, os, FLAG_ITEM, outer,
				                context, "parbox", "shaded");
			} else
				parse_box(p, os, 0, FLAG_ITEM, outer, context,
				          "", "", t.cs(), "", "");
			continue;
		}

		if (t.cs() == "fbox" || t.cs() == "mbox" ||
		    t.cs() == "ovalbox" || t.cs() == "Ovalbox" ||
		    t.cs() == "shadowbox" || t.cs() == "doublebox") {
			parse_outer_box(p, os, FLAG_ITEM, outer, context, t.cs(), "");
			continue;
		}

		if (t.cs() == "fcolorbox" || t.cs() == "colorbox") {
			string backgroundcolor;
			preamble.registerAutomaticallyLoadedPackage("xcolor");
			if (t.cs() == "fcolorbox") {
				string const framecolor = p.getArg('{', '}');
				backgroundcolor = p.getArg('{', '}');
				parse_box(p, os, 0, 0, outer, context, "", "", "", framecolor, backgroundcolor);
			} else {
				backgroundcolor = p.getArg('{', '}');
				parse_box(p, os, 0, 0, outer, context, "", "", "", "", backgroundcolor);
			}
			continue;
		}

		// FIXME: due to the compiler limit of "if" nestings
		// the code for the alignment was put here
		// put them in their own if if this is fixed
		if (t.cs() == "fboxrule" || t.cs() == "fboxsep"
		    || t.cs() == "shadowsize") {
			if (t.cs() == "fboxrule")
				fboxrule = "";
			if (t.cs() == "fboxsep")
				fboxsep = "";
			if (t.cs() == "shadowsize")
				shadow_size = "";
			p.skip_spaces(true);
			while (p.good() && p.next_token().cat() != catSpace
			       && p.next_token().cat() != catNewline
			       && p.next_token().cat() != catEscape) {
				if (t.cs() == "fboxrule")
					fboxrule = fboxrule + p.get_token().asInput();
				if (t.cs() == "fboxsep")
					fboxsep = fboxsep + p.get_token().asInput();
				if (t.cs() == "shadowsize")
					shadow_size = shadow_size + p.get_token().asInput();
			}
			continue;
		}

		if (t.cs() == "raggedleft" || t.cs() == "centering" || t.cs() == "raggedright") {
			if (context.in_table_cell) {
				if (t.cs() == "raggedleft")
					context.cell_align = 'r';
				else if (t.cs() == "centering")
					context.cell_align = 'c';
				else if (t.cs() == "raggedright")
					context.cell_align = 'l';
				p.skip_spaces(true);
			} else {
				output_ert_inset(os, t.asInput(), context);
			}
			continue;
		}

		//\framebox() is part of the picture environment and different from \framebox{}
		//\framebox{} will be parsed by parse_outer_box
		if (t.cs() == "framebox") {
			if (p.next_token().character() == '(') {
				//the syntax is: \framebox(x,y)[position]{content}
				string arg = t.asInput();
				arg += p.getFullParentheseArg();
				arg += p.getFullOpt();
				eat_whitespace(p, os, context, false);
				output_ert_inset(os, arg + '{', context);
				parse_text(p, os, FLAG_ITEM, outer, context);
				output_ert_inset(os, "}", context);
			} else {
				//the syntax is: \framebox[width][position]{content}
				string special = p.getFullOpt();
				special += p.getOpt();
				parse_outer_box(p, os, FLAG_ITEM, outer,
					            context, t.cs(), special);
			}
			continue;
		}

		//\makebox() is part of the picture environment and different from \makebox{}
		//\makebox{} will be parsed by parse_box
		if (t.cs() == "makebox") {
			if (p.next_token().character() == '(') {
				//the syntax is: \makebox(x,y)[position]{content}
				string arg = t.asInput();
				arg += p.getFullParentheseArg();
				arg += p.getFullOpt();
				eat_whitespace(p, os, context, false);
				output_ert_inset(os, arg + '{', context);
				parse_text(p, os, FLAG_ITEM, outer, context);
				output_ert_inset(os, "}", context);
			} else
				//the syntax is: \makebox[width][position]{content}
				parse_box(p, os, 0, FLAG_ITEM, outer, context,
				          "", "", t.cs(), "", "");
			continue;
		}

		if (t.cs() == "smallskip" ||
		    t.cs() == "medskip" ||
		    t.cs() == "bigskip" ||
		    t.cs() == "vfill") {
			context.check_layout(os);
			begin_inset(os, "VSpace ");
			os << t.cs();
			end_inset(os);
			skip_spaces_braces(p);
			continue;
		}

		if ((where = is_known(t.cs(), known_spaces))
		    && (context.pass_thru_cmds.find(t.cs()) == context.pass_thru_cmds.end())) {
			context.check_layout(os);
			begin_inset(os, "space ");
			os << '\\' << known_coded_spaces[where - known_spaces]
			   << '\n';
			end_inset(os);
			// LaTeX swallows whitespace after all spaces except
			// "\\,", "\\>", "\\!", "\\;", and "\\:".
			// We have to do that here, too, because LyX
			// adds "{}" which would make the spaces significant.
			if (!contains(",>!;:", t.cs()))
				eat_whitespace(p, os, context, false);
			// LyX adds "{}" after all spaces except "\\ " and
			// "\\,", so we have to remove "{}".
			// "\\,{}" is equivalent to "\\," in LaTeX, so we
			// remove the braces after "\\,", too.
			if (t.cs() != " ")
				skip_braces(p);
			continue;
		}

		if (t.cs() == "newpage" ||
		    (t.cs() == "pagebreak" && !p.hasOpt()) ||
		    t.cs() == "clearpage" ||
		    t.cs() == "cleardoublepage" ||
		    t.cs() == "nopagebreak") {
			context.check_layout(os);
			begin_inset(os, "Newpage ");
			os << t.cs();
			end_inset(os);
			skip_spaces_braces(p);
			continue;
		}

		// This accounts for the following table(s)
		if (t.cs() == "arrayrulecolor") {
			string const color = p.getArg('{', '}');
			context.current_table_bordercolor = preamble.getLyXColor(color);
			if (context.current_table_bordercolor.empty())
				output_ert_inset(os, t.asInput(), context);
		}

		// This accounts for the following table(s)
		if (t.cs() == "rowcolors") {
			string const startrow = p.getArg('{', '}');
			string const oddrowcolor = p.getArg('{', '}');
			string const evenrowcolor = p.getArg('{', '}');
			if (isStrInt(startrow))
				context.current_table_alt_row_colors_start = convert<int>(startrow);
			else
				context.current_table_alt_row_colors_start = 1;
			context.current_table_odd_row_color = preamble.getLyXColor(oddrowcolor);
			context.current_table_even_row_color = preamble.getLyXColor(evenrowcolor);
			if (context.current_table_odd_row_color.empty() || context.current_table_even_row_color.empty())
				output_ert_inset(os, t.asInput(), context);
		}
		

		if (t.cs() == "DeclareRobustCommand" ||
		         t.cs() == "DeclareRobustCommandx" ||
		         t.cs() == "newcommand" ||
		         t.cs() == "newcommandx" ||
		         t.cs() == "providecommand" ||
		         t.cs() == "providecommandx" ||
		         t.cs() == "renewcommand" ||
		         t.cs() == "renewcommandx") {
			// DeclareRobustCommand, DeclareRobustCommandx,
			// providecommand and providecommandx could be handled
			// by parse_command(), but we need to call
			// add_known_command() here.
			string name = t.asInput();
			if (p.next_token().asInput() == "*") {
				// Starred form. Eat '*'
				p.get_token();
				name += '*';
			}
			string const command = p.verbatim_item();
			string const opt1 = p.getFullOpt();
			string const opt2 = p.getFullOpt();
			add_known_command(command, opt1, !opt2.empty());
			string const ert = name + '{' + command + '}' +
					   opt1 + opt2 +
					   '{' + p.verbatim_item() + '}';

			if (t.cs() == "DeclareRobustCommand" ||
			    t.cs() == "DeclareRobustCommandx" ||
			    t.cs() == "providecommand" ||
			    t.cs() == "providecommandx" ||
			    name[name.length()-1] == '*')
				output_ert_inset(os, ert, context);
			else {
				context.check_layout(os);
				begin_inset(os, "FormulaMacro");
				os << "\n" << ert;
				end_inset(os);
			}
			continue;
		}

		if (t.cs() == "let" && p.next_token().asInput() != "*") {
			// let could be handled by parse_command(),
			// but we need to call add_known_command() here.
			string ert = t.asInput();
			string name;
			p.skip_spaces();
			if (p.next_token().cat() == catBegin) {
				name = p.verbatim_item();
				ert += '{' + name + '}';
			} else {
				name = p.verbatim_item();
				ert += name;
			}
			string command;
			p.skip_spaces();
			if (p.next_token().cat() == catBegin) {
				command = p.verbatim_item();
				ert += '{' + command + '}';
			} else {
				command = p.verbatim_item();
				ert += command;
			}
			// If command is known, make name known too, to parse
			// its arguments correctly. For this reason we also
			// have commands in syntax.default that are hardcoded.
			CommandMap::iterator it = known_commands.find(command);
			if (it != known_commands.end())
				known_commands[t.asInput()] = it->second;
			output_ert_inset(os, ert, context);
			continue;
		}

		if (t.cs() == "hspace" || t.cs() == "vspace") {
			if (starred)
				p.get_token();
			string name = t.asInput();
			string const length = p.verbatim_item();
			string unit;
			string valstring;
			bool valid = splitLatexLength(length, valstring, unit);
			bool known_hspace = false;
			bool known_vspace = false;
			bool known_unit = false;
			double value = 0.0;
			if (valid) {
				istringstream iss(valstring);
				iss >> value;
				if (value == 1.0) {
					if (t.cs()[0] == 'h') {
						if (unit == "\\fill") {
							if (!starred) {
								unit = "";
								name = "\\hfill";
							}
							known_hspace = true;
						}
					} else {
						if (unit == "\\smallskipamount") {
							unit = "smallskip";
							known_vspace = true;
						} else if (unit == "\\medskipamount") {
							unit = "medskip";
							known_vspace = true;
						} else if (unit == "\\bigskipamount") {
							unit = "bigskip";
							known_vspace = true;
						} else if (length == "\\baselineskip") {
							unit = "fullline";
							known_vspace = true;
						} else if (unit == "\\fill") {
							unit = "vfill";
							known_vspace = true;
						}
					}
				}
				if (value == 0.5 && t.cs()[0] != 'h' && unit == "\\baselineskip") {
					unit = "halfline";
					known_vspace = true;
				}
				if (!known_hspace && !known_vspace) {
					switch (unitFromString(unit)) {
					case Length::SP:
					case Length::PT:
					case Length::BP:
					case Length::DD:
					case Length::MM:
					case Length::PC:
					case Length::CC:
					case Length::CM:
					case Length::IN:
					case Length::EX:
					case Length::EM:
					case Length::MU:
						known_unit = true;
						break;
					default: {
						//unitFromString(unit) fails for relative units like Length::PCW
						// therefore handle them separately
						if (unit == "\\paperwidth" || unit == "\\columnwidth"
							|| unit == "\\textwidth" || unit == "\\linewidth"
							|| unit == "\\textheight" || unit == "\\paperheight"
							|| unit == "\\baselineskip")
							known_unit = true;
						break;
							 }
					}
				}
			}

			// check for glue lengths
			string gluelength = length;
			bool is_gluelength = is_glue_length(gluelength);

			if (t.cs()[0] == 'h' && (known_unit || known_hspace || is_gluelength)) {
				// Literal horizontal length or known variable
				context.check_layout(os);
				begin_inset(os, "space ");
				os << name;
				if (starred)
					os << '*';
				os << '{';
				if (known_hspace)
					os << unit;
				os << "}";
				if (known_unit && !known_hspace)
					os << "\n\\length " << translate_len(length);
				if (is_gluelength)
					os << "\n\\length " << gluelength;
				end_inset(os);
			} else if (known_unit || known_vspace || is_gluelength) {
				// Literal vertical length or known variable
				context.check_layout(os);
				begin_inset(os, "VSpace ");
				if (known_vspace)
					os << unit;
				if (known_unit && !known_vspace)
					os << translate_len(length);
				if (is_gluelength)
					os << gluelength;
				if (starred)
					os << '*';
				end_inset(os);
			} else {
				// LyX can't handle other length variables in Inset VSpace/space
				if (starred)
					name += '*';
				if (valid) {
					if (value == 1.0)
						output_ert_inset(os, name + '{' + unit + '}', context);
					else if (value == -1.0)
						output_ert_inset(os, name + "{-" + unit + '}', context);
					else
						output_ert_inset(os, name + '{' + valstring + unit + '}', context);
				} else
					output_ert_inset(os, name + '{' + length + '}', context);
			}
			continue;
		}

		// Before we look for the layout name alone below, we check the layouts including the LateXParam, which
		// might be one or several options or a star.
		// The single '=' is meant here.
		if ((newinsetlayout = findInsetLayout(context.textclass, starredname, true, p.getCommandLatexParam()))) {
			if (starred)
				p.get_token();
			p.skip_spaces();
			context.check_layout(os);
			// store the latexparam here. This is eaten in parse_text_in_inset
			context.latexparam = newinsetlayout->latexparam();
			docstring name = newinsetlayout->name();
			bool const caption = name.find(from_ascii("Caption:")) == 0;
			if (caption) {
				// Already done for floating minted listings.
				if (minted_float.empty()) {
					begin_inset(os, "Caption ");
					os << to_utf8(name.substr(8)) << '\n';
				}
			} else {
				// FIXME: what do we do if the prefix is not Flex: ?
				if (prefixIs(name, from_ascii("Flex:")))
					name.erase(0, 5);
				begin_inset(os, "Flex ");
				os << to_utf8(name) << '\n'
				   << "status collapsed\n";
			}
			if (!minted_float.empty()) {
				parse_text_snippet(p, os, FLAG_ITEM, false, context);
			} else if (newinsetlayout->isPassThru()) {
				// set catcodes to verbatim early, just in case.
				p.setCatcodes(VERBATIM_CATCODES);
				string delim = p.get_token().asInput();
				if (delim != "{")
					warning_message("bad delimiter for command " + t.asInput());
				//FIXME: handle error condition
				string const arg = p.verbatimStuff("}").second;
				Context newcontext(true, context.textclass);
				if (newinsetlayout->forcePlainLayout())
					newcontext.layout = &context.textclass.plainLayout();
				output_ert(os, arg, newcontext);
			} else
				parse_text_in_inset(p, os, FLAG_ITEM, false, context, newinsetlayout);
			context.latexparam.clear();
			if (caption)
				p.skip_spaces();
			// Minted caption insets are not closed here because
			// we collect everything into the caption.
			if (minted_float.empty())
				end_inset(os);
			continue;
		}

		// The single '=' is meant here.
		if ((newinsetlayout = findInsetLayout(context.textclass, starredname, true))) {
			if (starred)
				p.get_token();
			p.skip_spaces();
			context.check_layout(os);
			docstring name = newinsetlayout->name();
			bool const caption = name.find(from_ascii("Caption:")) == 0;
			if (caption) {
				// Already done for floating minted listings.
				if (minted_float.empty()) {
					begin_inset(os, "Caption ");
					os << to_utf8(name.substr(8)) << '\n';
				}
			} else {
				// FIXME: what do we do if the prefix is not Flex: ?
				if (prefixIs(name, from_ascii("Flex:")))
					name.erase(0, 5);
				begin_inset(os, "Flex ");
				os << to_utf8(name) << '\n'
				   << "status collapsed\n";
			}
			if (!minted_float.empty()) {
				parse_text_snippet(p, os, FLAG_ITEM, false, context);
			} else if (newinsetlayout->isPassThru()) {
				// set catcodes to verbatim early, just in case.
				p.setCatcodes(VERBATIM_CATCODES);
				string delim = p.get_token().asInput();
				if (delim != "{")
					warning_message("bad delimiter for command " + t.asInput());
				//FIXME: handle error condition
				string const arg = p.verbatimStuff("}").second;
				Context newcontext(true, context.textclass);
				if (newinsetlayout->forcePlainLayout())
					newcontext.layout = &context.textclass.plainLayout();
				output_ert(os, arg, newcontext);
			} else
				parse_text_in_inset(p, os, FLAG_ITEM, false, context, newinsetlayout);
			if (caption)
				p.skip_spaces();
			// Minted caption insets are not closed here because
			// we collect everything into the caption.
			if (minted_float.empty())
				end_inset(os);
			continue;
		}

		if (t.cs() == "includepdf") {
			p.skip_spaces();
			string const arg = p.getArg('[', ']');
			map<string, string> opts;
			vector<string> keys;
			split_map(arg, opts, keys);
			string name = normalize_filename(p.verbatim_item());
			string const path = getMasterFilePath(true);
			// We want to preserve relative / absolute filenames,
			// therefore path is only used for testing
			if (!makeAbsPath(name, path).exists()) {
				// The file extension is probably missing.
				// Now try to find it out.
				char const * const pdfpages_format[] = {"pdf", 0};
				string const pdftex_name =
					find_file(name, path, pdfpages_format);
				if (!pdftex_name.empty()) {
					name = pdftex_name;
					pdflatex = true;
				}
			}
			FileName const absname = makeAbsPath(name, path);
			if (absname.exists())
			{
				fix_child_filename(name);
				copy_file(absname, name);
			} else
				warning_message("Could not find file '" + name + "'.");
			// write output
			context.check_layout(os);
			begin_inset(os, "External\n\ttemplate ");
			os << "PDFPages\n\tfilename "
			   << name << "\n";
			// parse the options
			if (opts.find("pages") != opts.end())
				os << "\textra LaTeX \"pages="
				   << opts["pages"] << "\"\n";
			if (opts.find("angle") != opts.end())
				os << "\trotateAngle "
				   << opts["angle"] << '\n';
			if (opts.find("origin") != opts.end()) {
				ostringstream ss;
				string const opt = opts["origin"];
				if (opt == "tl") ss << "topleft";
				if (opt == "bl") ss << "bottomleft";
				if (opt == "Bl") ss << "baselineleft";
				if (opt == "c") ss << "center";
				if (opt == "tc") ss << "topcenter";
				if (opt == "bc") ss << "bottomcenter";
				if (opt == "Bc") ss << "baselinecenter";
				if (opt == "tr") ss << "topright";
				if (opt == "br") ss << "bottomright";
				if (opt == "Br") ss << "baselineright";
				if (!ss.str().empty())
					os << "\trotateOrigin " << ss.str() << '\n';
				else
					warning_message("Ignoring unknown includegraphics origin argument '" + opt + "'");
			}
			if (opts.find("width") != opts.end())
				os << "\twidth "
				   << translate_len(opts["width"]) << '\n';
			if (opts.find("height") != opts.end())
				os << "\theight "
				   << translate_len(opts["height"]) << '\n';
			if (opts.find("keepaspectratio") != opts.end())
				os << "\tkeepAspectRatio\n";
			end_inset(os);
			context.check_layout(os);
			registerExternalTemplatePackages("PDFPages");
			continue;
		}

		if (t.cs() == "loadgame") {
			p.skip_spaces();
			string name = normalize_filename(p.verbatim_item());
			string const path = getMasterFilePath(true);
			// We want to preserve relative / absolute filenames,
			// therefore path is only used for testing
			if (!makeAbsPath(name, path).exists()) {
				// The file extension is probably missing.
				// Now try to find it out.
				char const * const lyxskak_format[] = {"fen", 0};
				string const lyxskak_name =
					find_file(name, path, lyxskak_format);
				if (!lyxskak_name.empty())
					name = lyxskak_name;
			}
			FileName const absname = makeAbsPath(name, path);
			if (absname.exists())
			{
				fix_child_filename(name);
				copy_file(absname, name);
			} else
				warning_message("Could not find file '" + name + "'.");
			context.check_layout(os);
			begin_inset(os, "External\n\ttemplate ");
			os << "ChessDiagram\n\tfilename "
			   << name << "\n";
			end_inset(os);
			context.check_layout(os);
			// after a \loadgame follows a \showboard
			if (p.get_token().asInput() == "showboard")
				p.get_token();
			registerExternalTemplatePackages("ChessDiagram");
			continue;
		}

		// try to see whether the string is in unicodesymbols
		// Only use text mode commands, since we are in text mode here,
		// and math commands may be invalid (bug 6797)
		string name = t.asInput();
		// handle the dingbats, cyrillic and greek
		if (name == "\\textcyr")
			name = "\\textcyrillic";
		if (name == "\\ding" || name == "\\textcyrillic" ||
		    (name == "\\textgreek" && !preamble.usePolyglossia()))
			name = name + '{' + p.getArg('{', '}') + '}';
		// handle the ifsym characters
		else if (name == "\\textifsymbol") {
			string const optif = p.getFullOpt();
			string const argif = p.getArg('{', '}');
			name = name + optif + '{' + argif + '}';
		}
		// handle the \ascii characters
		// the case of \ascii within braces, as LyX outputs it, is already
		// handled for t.cat() == catBegin
		else if (name == "\\ascii") {
			// the code is "\asci\xxx"
			name = "{" + name + p.get_token().asInput() + "}";
			skip_braces(p);
		}
		// handle some TIPA special characters
		else if (preamble.isPackageUsed("tipa")) {
			if (name == "\\s") {
				// fromLaTeXCommand() does not yet
				// recognize tipa short cuts
				name = "\\textsyllabic";
			} else if (name == "\\=" &&
				   p.next_token().asInput() == "*") {
				// fromLaTeXCommand() does not yet
				// recognize tipa short cuts
				p.get_token();
				name = "\\textsubbar";
			} else if (name == "\\textdoublevertline") {
				// FIXME: This is not correct,
				// \textvertline is higher than \textbardbl
				name = "\\textbardbl";
				skip_braces(p);
			} else if (name == "\\!" ) {
				if (p.next_token().asInput() == "b") {
					p.get_token();	// eat 'b'
					name = "\\texthtb";
					skip_braces(p);
				} else if (p.next_token().asInput() == "d") {
					p.get_token();
					name = "\\texthtd";
					skip_braces(p);
				} else if (p.next_token().asInput() == "g") {
					p.get_token();
					name = "\\texthtg";
					skip_braces(p);
				} else if (p.next_token().asInput() == "G") {
					p.get_token();
					name = "\\texthtscg";
					skip_braces(p);
				} else if (p.next_token().asInput() == "j") {
					p.get_token();
					name = "\\texthtbardotlessj";
					skip_braces(p);
				} else if (p.next_token().asInput() == "o") {
					p.get_token();
					name = "\\textbullseye";
					skip_braces(p);
				}
			} else if (name == "\\*" ) {
				if (p.next_token().asInput() == "k") {
					p.get_token();
					name = "\\textturnk";
					skip_braces(p);
				} else if (p.next_token().asInput() == "r") {
					p.get_token();	// eat 'b'
					name = "\\textturnr";
					skip_braces(p);
				} else if (p.next_token().asInput() == "t") {
					p.get_token();
					name = "\\textturnt";
					skip_braces(p);
				} else if (p.next_token().asInput() == "w") {
					p.get_token();
					name = "\\textturnw";
					skip_braces(p);
				}
			}
		}
		if ((name.size() == 2 &&
		     contains("\"'.=^`bcdHkrtuv~", name[1]) &&
		     p.next_token().asInput() != "*") ||
		    is_known(name.substr(1), known_tipa_marks)) {
			// name is a command that corresponds to a
			// combining character in unicodesymbols.
			// Append the argument, fromLaTeXCommand()
			// will either convert it to a single
			// character or a combining sequence.
			name += '{' + p.verbatim_item() + '}';
		}
		// now get the character from unicodesymbols
		bool termination;
		docstring rem;
		set<string> req;
		docstring s = normalize_c(encodings.fromLaTeXCommand(from_utf8(name),
				Encodings::TEXT_CMD, termination, rem, &req));
		if (!s.empty()) {
			context.check_layout(os);
			os << to_utf8(s);
			if (!rem.empty())
				output_ert_inset(os, to_utf8(rem), context);
			if (termination)
				skip_spaces_braces(p);
			for (set<string>::const_iterator it = req.begin(); it != req.end(); ++it)
				preamble.registerAutomaticallyLoadedPackage(*it);
		}
		//warning_message("#: " + t + " mode: " + mode);
		// heuristic: read up to next non-nested space
		/*
		string s = t.asInput();
		string z = p.verbatim_item();
		while (p.good() && z != " " && !z.empty()) {
			//warning_message("read: " + z);
			s += z;
			z = p.verbatim_item();
		}
		warning_message("found ERT: " + s);
		output_ert_inset(os, s + ' ', context);
		*/
		else {
			if (t.asInput() == name &&
			    p.next_token().asInput() == "*") {
				// Starred commands like \vspace*{}
				p.get_token();	// Eat '*'
				name += '*';
			}
			if (!parse_command(name, p, os, outer, context)) {
				output_ert_inset(os, name, context);
				// Try to handle options of unknown commands:
				// Look if we have optional arguments,
				// and if so, put the brackets in ERT.
				while (p.hasOpt()) {
					p.get_token(); // eat '['
					output_ert_inset(os, "[", context);
					os << parse_text_snippet(p, FLAG_BRACK_LAST, outer, context);
					output_ert_inset(os, "]", context);
				}
			}
		}
	}
}


string guessLanguage(Parser & p, string const & lang)
{
	typedef std::map<std::string, size_t> LangMap;
	// map from language names to number of characters
	LangMap used;
	used[lang] = 0;
	for (char const * const * i = supported_CJK_languages; *i; i++)
		used[string(*i)] = 0;

	while (p.good()) {
		Token const t = p.get_token();
		// comments are not counted for any language
		if (t.cat() == catComment)
			continue;
		// commands are not counted as well, but we need to detect
		// \begin{CJK} and switch encoding if needed
		if (t.cat() == catEscape) {
			if (t.cs() == "inputencoding") {
				string const enc = subst(p.verbatim_item(), "\n", " ");
				p.setEncoding(enc, Encoding::inputenc);
				continue;
			}
			if (t.cs() != "begin")
				continue;
		} else {
			// Non-CJK content is counted for lang.
			// We do not care about the real language here:
			// If we have more non-CJK contents than CJK contents,
			// we simply use the language that was specified as
			// babel main language.
			used[lang] += t.asInput().length();
			continue;
		}
		// Now we are starting an environment
		p.pushPosition();
		string const name = p.getArg('{', '}');
		if (name != "CJK") {
			p.popPosition();
			continue;
		}
		// It is a CJK environment
		p.popPosition();
		/* name = */ p.getArg('{', '}');
		string const encoding = p.getArg('{', '}');
		/* mapping = */ p.getArg('{', '}');
		string const encoding_old = p.getEncoding();
		char const * const * const where =
			is_known(encoding, supported_CJK_encodings);
		if (where)
			p.setEncoding(encoding, Encoding::CJK);
		else
			p.setEncoding("UTF-8");
		string const text = p.ertEnvironment("CJK");
		p.setEncoding(encoding_old);
		p.skip_spaces();
		if (!where) {
			// ignore contents in unknown CJK encoding
			continue;
		}
		// the language of the text
		string const cjk =
			supported_CJK_languages[where - supported_CJK_encodings];
		used[cjk] += text.length();
	}
	LangMap::const_iterator use = used.begin();
	for (LangMap::const_iterator it = used.begin(); it != used.end(); ++it) {
		if (it->second > use->second)
			use = it;
	}
	return use->first;
}


void check_comment_bib(ostream & os, Context & context)
{
	if (!need_commentbib)
		return;
	// We have a bibliography database, but no bibliography with biblatex
	// which is completely valid. Insert a bibtex inset in a note.
	context.check_layout(os);
	begin_inset(os, "Note Note\n");
	os << "status open\n";
	os << "\\begin_layout Plain Layout\n";
	begin_command_inset(os, "bibtex", "bibtex");
	string bibfiles;
	for (auto const & bf : preamble.biblatex_bibliographies) {
		if (!bibfiles.empty())
			bibfiles += ",";
		bibfiles += normalize_filename(bf);
	}
	if (!bibfiles.empty())
		os << "bibfiles " << '"' << bibfiles << '"' << "\n";
	string bibfileencs;
	for (auto const & bf : preamble.biblatex_encodings) {
		if (!bibfileencs.empty())
			bibfileencs += "\t";
		bibfileencs += bf;
	}
	if (!bibfileencs.empty())
		os << "file_encodings " << '"' << bibfileencs << '"' << "\n";
	end_inset(os);// Bibtex
	os << "\\end_layout\n";
	end_inset(os);// Note
}

// }])


} // namespace lyx
