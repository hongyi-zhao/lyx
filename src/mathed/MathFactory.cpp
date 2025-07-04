/**
 * \file MathFactory.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "MathFactory.h"

#include "InsetMathAMSArray.h"
#include "InsetMathArray.h"
#include "InsetMathBoldSymbol.h"
#include "InsetMathBox.h"
#include "InsetMathCancel.h"
#include "InsetMathCancelto.h"
#include "InsetMathCases.h"
#include "InsetMathClass.h"
#include "InsetMathColor.h"
#include "InsetMathDecoration.h"
#include "InsetMathDots.h"
#include "InsetMathEnsureMath.h"
#include "InsetMathFont.h"
#include "InsetMathFontOld.h"
#include "InsetMathFrac.h"
#include "InsetMathIntertext.h"
#include "InsetMathLefteqn.h"
#include "InsetMathOverset.h"
#include "InsetMathPhantom.h"
#include "InsetMathRef.h"
#include "InsetMathRoot.h"
#include "InsetMathSideset.h"
#include "InsetMathSize.h"
#include "InsetMathSpace.h"
#include "InsetMathSpecialChar.h"
#include "InsetMathSplit.h"
#include "InsetMathSqrt.h"
#include "InsetMathStackrel.h"
#include "InsetMathSubstack.h"
#include "InsetMathSymbol.h"
#include "InsetMathTabular.h"
#include "InsetMathTextsize.h"
#include "InsetMathUnderset.h"
#include "InsetMathUnknown.h"
#include "InsetMathHull.h"
#include "InsetMathXArrow.h"
#include "InsetMathXYMatrix.h"
#include "InsetMathDiagram.h"
#include "MacroTable.h"
#include "InsetMathMacro.h"
#include "InsetMathMacroArgument.h"
#include "MathParser.h"
#include "MathStream.h"
#include "MathSupport.h"

#include "insets/InsetCommand.h"
#include "insets/InsetSpace.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/FileName.h"
#include "support/filetools.h" // LibFileSearch
#include "support/lstrings.h"

#include "frontends/FontLoader.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "Encoding.h"
#include "FontInfo.h"
#include "LyX.h" // use_gui

#include <iomanip>

using namespace std;
using namespace lyx::support;

namespace lyx {

bool has_math_fonts;


namespace {

MathWordList theMathWordList;
MathVariantList theMathVariantList;
MathConflictList theMathConflictList;
std::vector<HullType> theMathConflictEnvs;


bool isMathFontAvailable(string & name)
{
	if (!use_gui)
		return false;

	FontInfo f;
	augmentFont(f, name);

	// Do we have the font proper?
	if (theFontLoader().available(f))
		return true;

	// can we fake it?
	if (name == "eufrak") {
		name = "lyxfakefrak";
		return true;
	}

	LYXERR(Debug::MATHED,
		"font " << name << " not available and I can't fake it");
	return false;
}


bool canBeDisplayed(char_type c)
{
	if (!use_gui)
		return true;
	return theFontLoader().canBeDisplayed(c);
}


bool isUnicodeSymbolAvailable(docstring const & name, char_type & c)
{
	docstring cmd(from_ascii("\\") + name);
	bool is_combining;
	bool termination;
	c = Encodings::fromLaTeXCommand(cmd, Encodings::MATH_CMD,
	                                is_combining, termination);
	if (c == 0 && name == "varOmega") {
		// fallback for bug 7954, unicodesymbols does not list
		// \\varOmega because of requirements, but this might change
		cmd = from_ascii("\\mathit{\\Omega}");
		c = Encodings::fromLaTeXCommand(cmd, Encodings::MATH_CMD,
		                                is_combining, termination);
	}
	return c != 0 && !is_combining;
}


void initSymbols()
{
	FileName const filename = libFileSearch(string(), "symbols");
	LYXERR(Debug::MATHED, "read symbols from " << filename);
	if (filename.empty()) {
		lyxerr << "Could not find symbols file" << endl;
		return;
	}

	ifstream fs(filename.toFilesystemEncoding().c_str());
	// limit the size of strings we read to avoid memory problems
	fs >> setw(65636);
	string line;
	bool skip = false;
	while (getline(fs, line)) {
		int charid     = 0;
		int dsp_charid = 0;
		int fallbackid = 0;
		if (line.empty() || line[0] == '#')
			continue;

		// special case of iffont/else/endif
		if (line.size() >= 7 && line.substr(0, 6) == "iffont") {
			istringstream is(line);
			// limit the size of strings we read to avoid memory problems
			is >> setw(65636);
			string tmp;
			is >> tmp;
			is >> tmp;
			skip = !isMathFontAvailable(tmp);
			continue;
		} else if (line.size() >= 4 && line.substr(0, 4) == "else") {
			skip = !skip;
			continue;
		} else if (line.size() >= 5 && line.substr(0, 5) == "endif") {
			skip = false;
			continue;
		} else if (skip)
			continue;

		// special case of pre-defined macros
		if (line.size() > 8 && line.substr(0, 5) == "\\def\\") {
			//lyxerr << "macro definition: '" << line << '\'' << endl;
			// syntax: Either
			// \def\macroname{definition}
			// or
			// \def\macroname{definition} requires
			// or
			// \def\macroname{definition} extra xmlname requires
			istringstream is(line);
			string macro;
			string required;
			string extra;
			string xmlname;
			bool hidden = false;
			is >> setw(65536) >> macro >> required;
			if ((is >> xmlname)) {
				extra = required;
				if (!(is >> required))
					required = "";
			} else
				xmlname = "";
			MacroTable::iterator it = MacroTable::globalMacros().insert(
					nullptr, from_utf8(macro));
			if (!extra.empty() || !xmlname.empty() || !required.empty()) {
				MathWordList::iterator wit = theMathWordList.find(it->first);
				if (wit != theMathWordList.end())
					LYXERR(Debug::MATHED, "readSymbols: inset "
						<< to_utf8(it->first) << " already exists.");
				else {
					latexkeys tmp;
					tmp.inset = "macro";
					tmp.name = it->first;
					tmp.extra = from_utf8(extra);
					tmp.xmlname = from_utf8(xmlname);
					if (required == "hiddensymbol") {
						required = "";
						tmp.hidden = hidden = true;
					} else
						tmp.required = required;
					theMathWordList[it->first] = tmp;
					wit = theMathWordList.find(it->first);
					it->second.setSymbol(&(wit->second));
				}
			}
			// If you change the following output, please adjust
			// development/tools/generate_symbols_images.py.
			LYXERR(Debug::MATHED, "read symbol '" << to_utf8(it->first)
			    << "  inset: macro"
			    << "  draw: 0"
			    << "  extra: " << extra
			    << "  xml: " << xmlname
				<< "  requires: " << required
				<< "  hidden: " << hidden << '\'');
			continue;
		}

		idocstringstream is(from_utf8(line));
		latexkeys tmp;
		docstring help;
		is >> tmp.name >> help;
		tmp.inset = to_ascii(help);
		if (isFontName(tmp.inset)) {
			is >> help >> fallbackid >> tmp.extra >> tmp.xmlname;
			docstring cid, dsp_cid;
			idocstringstream is2(subst(help, '|', ' '));
			is2 >> charid >> dsp_charid;
		} else
			is >> tmp.extra;
		// requires is optional
		if (is) {
			if ((is >> help)) {
				// backward compatibility
				if (help == "esintoramsmath")
					tmp.required = "esint|amsmath";
				else
					tmp.required = to_ascii(help);
			}
		} else {
			LYXERR(Debug::MATHED, "skipping line '" << line << "'\n"
				<< to_utf8(tmp.name) << ' ' << tmp.inset << ' '
				<< to_utf8(tmp.extra));
			continue;
		}

		if (isFontName(tmp.inset)) {
			// tmp.inset _is_ the fontname here.
			// create fallbacks if necessary

			// store requirements as long as we can
			if (tmp.required.empty()) {
				if (tmp.inset == "msa" || tmp.inset == "msb")
					tmp.required = "amssymb";
				else if (tmp.inset == "wasy")
					tmp.required = "wasysym";
				else if (tmp.inset == "mathscr")
					tmp.required = "mathrsfs";
				else if (tmp.inset == "mathds")
					tmp.required = "dsfont";
			}

			// symbol font is not available sometimes
			string symbol_font = "lyxsymbol";
			char_type unicodesymbol = 0;

			if (tmp.extra == "func" || tmp.extra == "funclim" || tmp.extra == "special") {
				LYXERR(Debug::MATHED, "symbol abuse for " << to_utf8(tmp.name));
				tmp.draw = tmp.name;
			} else if (isMathFontAvailable(tmp.inset) && canBeDisplayed(charid)) {
				LYXERR(Debug::MATHED, "symbol available for " << to_utf8(tmp.name));
				tmp.draw.push_back(char_type(charid));
				if (dsp_charid && canBeDisplayed(dsp_charid)) {
					LYXERR(Debug::MATHED, "large symbol available for " << to_utf8(tmp.name));
					tmp.dsp_draw.push_back(char_type(dsp_charid));
				}
			} else if (fallbackid && isMathFontAvailable(symbol_font) &&
			           canBeDisplayed(fallbackid)) {
				if (tmp.inset == "cmex")
					tmp.inset = "lyxsymbol";
				else
					tmp.inset = "lyxboldsymbol";
				LYXERR(Debug::MATHED, "symbol fallback for " << to_utf8(tmp.name));
				tmp.draw.push_back(char_type(fallbackid));
			} else if (isUnicodeSymbolAvailable(tmp.name, unicodesymbol)) {
				LYXERR(Debug::MATHED, "unicode fallback for " << to_utf8(tmp.name));
				tmp.inset = "mathnormal";
				tmp.draw.push_back(unicodesymbol);
			} else {
				LYXERR(Debug::MATHED, "faking " << to_utf8(tmp.name));
				tmp.draw = tmp.name;
				tmp.inset = "lyxtex";
			}
		} else {
			// it's a proper inset
			LYXERR(Debug::MATHED, "inset " << tmp.inset
					      << " used for " << to_utf8(tmp.name));
		}

		if (tmp.required == "hiddensymbol")
		{
			tmp.required = "";
			tmp.hidden = true;
		}

		if (theMathWordList.find(tmp.name) != theMathWordList.end())
			LYXERR(Debug::MATHED, "readSymbols: inset " << to_utf8(tmp.name)
				<< " already exists.");
		else
			theMathWordList[tmp.name] = tmp;

		// If you change the following output, please adjust
		// development/tools/generate_symbols_images.py.
		LYXERR(Debug::MATHED, "read symbol '" << to_utf8(tmp.name)
			<< "  inset: " << tmp.inset
			<< "  draw: " << int(tmp.draw.empty() ? 0 : tmp.draw[0])
			<< "  extra: " << to_utf8(tmp.extra)
			<< "  xml: " << to_utf8(tmp.xmlname)
			<< "  requires: " << tmp.required
			<< "  hidden: " << tmp.hidden << '\'');
	}
	string tmp = "cmm";
	string tmp2 = "cmsy";
	has_math_fonts = isMathFontAvailable(tmp) && isMathFontAvailable(tmp2);
}


void initVariantSymbols()
{
	FileName const filename = libFileSearch(string(), "unicode_alphanum_variants");
	LYXERR(Debug::MATHED, "read variant symbols from " << filename);
	if (filename.empty()) {
		lyxerr << "Could not find variant symbols file" << endl;
		return;
	}

	ifstream fs(filename.toFilesystemEncoding().c_str());
	// limit the size of strings we read to avoid memory problems
	fs >> setw(65636);
	string line;
	while (getline(fs, line)) {
		if (line.empty() || line[0] == '#')
			continue;

		// Split the line along spaces.
		std::string character, bold, italic, bold_italic, script, bold_script,
			fraktur, bold_fraktur, double_struck, sans, bold_sans, italic_sans,
			bold_italic_sans, monospace;
		line = split(line, character, ' ');
		line = split(line, bold, ' ');
		line = split(line, italic, ' ');
		line = split(line, bold_italic, ' ');
		line = split(line, script, ' ');
		line = split(line, bold_script, ' ');
		line = split(line, fraktur, ' ');
		line = split(line, bold_fraktur, ' ');
		line = split(line, double_struck, ' ');
		line = split(line, sans, ' ');
		line = split(line, bold_sans, ' ');
		line = split(line, italic_sans, ' ');
		line = split(line, bold_italic_sans, ' ');
		line = split(line, monospace, ' ');

		// Deal with the special case of "": it means that there is no mapping.
		if (character == "\"\"") continue;
		if (bold == "\"\"") bold = "";
		if (italic == "\"\"") italic = "";
		if (bold_italic == "\"\"") bold_italic = "";
		if (script == "\"\"") script = "";
		if (bold_script == "\"\"") bold_script = "";
		if (fraktur == "\"\"") fraktur = "";
		if (bold_fraktur == "\"\"") bold_fraktur = "";
		if (double_struck == "\"\"") double_struck = "";
		if (sans == "\"\"") sans = "";
		if (bold_sans == "\"\"") bold_sans = "";
		if (italic_sans == "\"\"") italic_sans = "";
		if (bold_italic_sans == "\"\"") bold_italic_sans = "";
		if (monospace == "\"\"") monospace = "";

		// Build the object, converting from ASCII std::string to actual docstring.
		UnicodeVariants tmp;
		tmp.character = from_ascii(character);
		tmp.bold = from_ascii(bold);
		tmp.italic = from_ascii(italic);
		tmp.bold_italic = from_ascii(bold_italic);
		tmp.script = from_ascii(script);
		tmp.bold_script = from_ascii(bold_script);
		tmp.fraktur = from_ascii(fraktur);
		tmp.bold_fraktur = from_ascii(bold_fraktur);
		tmp.double_struck = from_ascii(double_struck);
		tmp.sans = from_ascii(sans);
		tmp.bold_sans = from_ascii(bold_sans);
		tmp.italic_sans = from_ascii(italic_sans);
		tmp.bold_italic_sans = from_ascii(bold_italic_sans);
		tmp.monospace = from_ascii(monospace);

		// Insert the new mappings if the character hasn't had previous mappings.
		if (theMathVariantList.find(tmp.character) != theMathVariantList.end())
			LYXERR(Debug::MATHED, "readVariantSymbols: symbol " << to_utf8(tmp.character)
				<< " already exists.");
		else
			theMathVariantList[tmp.character] = tmp;
	}
}


void initConflictList() {
	FileName const filename = libFileSearch(string(), "math_conflicts");
	LYXERR(Debug::MATHED, "read conflict list from " << filename);
	if (filename.empty()) {
		lyxerr << "Could not find conflict list file" << endl;
		return;
	}

	ifstream fs(filename.toFilesystemEncoding().c_str());
	// limit the size of strings we read to avoid memory problems
	fs >> setw(65636);
	string line;

	HullType currentMathHull;
	while (getline(fs, line)) {

		if (line.empty() || line[0] == '#')
			continue;

		if (line[0] == '[') {
			currentMathHull = hullType(from_utf8(trim(line, "[]")));
			theMathConflictEnvs.push_back(currentMathHull);
		} else {
			theMathConflictList[currentMathHull].push_back(from_utf8(line));
		}
	}
}


bool isSpecialChar(docstring const & name)
{
	if (name.size() != 1)
		return  name == "textasciicircum" || name == "mathcircumflex" ||
			name == "textasciitilde"  || name == "textbackslash";

	char_type const c = name.at(0);
	return  c == '{' || c == '}' || c == '&' || c == '$' ||
		c == '#' || c == '%' || c == '_';
}


} // namespace

MathWordList const & mathedWordList()
{
	return theMathWordList;
}


MathVariantList const & mathedVariantList()
{
	return theMathVariantList;
}


MathConflictList const & mathedConflictList()
{
	return theMathConflictList;
}


std::vector<docstring> const & mathedConflictList(HullType hull)
{
	return theMathConflictList[hull];
}


std::vector<HullType> const & mathedConflictEnvs()
{
	return theMathConflictEnvs;
}


void initMath()
{
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		initParser();
		initSymbols();
		initVariantSymbols();
		initConflictList();
	}
}


bool ensureMath(TeXMathStream & os, bool needs_mathmode, bool macro,
                bool textmode_macro)
{
	bool brace = os.pendingBrace();
	os.pendingBrace(false);
	if (!os.latex())
		return brace;
	if (os.textMode() && needs_mathmode) {
		if (brace) {
			// close \lyxmathsym
			os << '}';
			brace = false;
		} else {
			os << "\\ensuremath{";
			brace = true;
		}
		os.textMode(false);
	} else if (macro && textmode_macro && !os.textMode()) {
		if (brace) {
			// close \ensuremath
			os << '}';
			brace = false;
		} else {
			os << "\\lyxmathsym{";
			brace = true;
		}
		os.textMode(true);
	} else if (macro && brace && !needs_mathmode && !textmode_macro) {
		// This is a user defined macro, not a InsetMathMacro, so we
		// cannot be sure what mode is needed. We leave it in the
		// same environment it was entered by closing either \lyxmathsym
		// or \ensuremath, whichever was opened.
		os << '}';
		brace = false;
		os.textMode(!os.textMode());
	}
	return brace;
}


int ensureMode(TeXMathStream & os, InsetMath::mode_type mode,
               bool locked, bool ascii)
{
	bool textmode = mode == InsetMath::TEXT_MODE;
	if (os.latex() && textmode && os.pendingBrace()) {
		os.os() << '}';
		os.pendingBrace(false);
		os.pendingSpace(false);
		os.textMode(true);
	}
	int oldmodes = os.textMode() ? 0x01 : 0;
	os.textMode(textmode);
	oldmodes |= os.lockedMode() ? 0x02 : 0;
	os.lockedMode(locked);
	oldmodes |= os.asciiOnly() ? 0x04 : 0;
	os.asciiOnly(ascii);
	return oldmodes;
}


latexkeys const * in_word_set(docstring const & str)
{
	MathWordList::iterator it = theMathWordList.find(str);
	if (it == theMathWordList.end())
		return nullptr;
	if (it->second.inset == "macro")
		return nullptr;
	return &(it->second);
}


MathAtom createInsetMath(char const * const s, Buffer * buf)
{
	return createInsetMath(from_utf8(s), buf);
}


MathAtom createInsetMath(docstring const & s, Buffer * buf)
{
	//lyxerr << "creating inset with name: '" << to_utf8(s) << '\'' << endl;
	if ((s == "ce" || s == "cf") && buf
	    && buf->params().use_package("mhchem") == BufferParams::package_off)
		return MathAtom(new InsetMathMacro(buf, s));

	latexkeys const * l = in_word_set(s);
	if (l) {
		string const & inset = l->inset;
		//lyxerr << " found inset: '" << inset << '\'' << endl;
		if (inset == "ref")
			return MathAtom(new InsetMathRef(buf, l->name));
		if (inset == "overset")
			return MathAtom(new InsetMathOverset(buf));
		if (inset == "underset")
			return MathAtom(new InsetMathUnderset(buf));
		if (inset == "decoration")
			return MathAtom(new InsetMathDecoration(buf, l));
		if (inset == "space")
			return MathAtom(new InsetMathSpace(buf, to_ascii(l->name), ""));
		if (inset == "class")
			return MathAtom(new InsetMathClass(buf, string_to_class(s)));
		if (inset == "dots")
			return MathAtom(new InsetMathDots(buf, l));
		if (inset == "mbox")
			return MathAtom(new InsetMathBox(buf, l->name));
//		if (inset == "fbox")
//			return MathAtom(new InsetMathFBox(l));
		if (inset == "intertext")
			return MathAtom(new InsetMathIntertext(buf, l->name));
		if (inset == "style")
			return MathAtom(new InsetMathSize(buf, l));
		if (inset == "font")
			return MathAtom(new InsetMathFont(buf, l));
		if (inset == "oldfont")
			return MathAtom(new InsetMathFontOld(buf, l));
		if (inset == "textsize")
			return MathAtom(new InsetMathTextsize(buf, l));
		if (inset == "matrix")
			return MathAtom(new InsetMathAMSArray(buf, s));
		if (inset == "split")
			return MathAtom(new InsetMathSplit(buf, s));
		if (inset == "big")
			// we can't create a InsetMathBig, since the argument
			// is missing.
			return MathAtom(new InsetMathUnknown(buf, s));
		return MathAtom(new InsetMathSymbol(buf, l));
	}

	if (s.size() == 2 && s[0] == '#' && s[1] >= '1' && s[1] <= '9')
		return MathAtom(new InsetMathMacroArgument(buf, s[1] - '0'));
	if (s.size() == 3 && s[0] == '\\' && s[1] == '#'
			&& s[2] >= '1' && s[2] <= '9')
		return MathAtom(new InsetMathMacroArgument(buf, s[2] - '0'));
	if (s == "boxed")
		return MathAtom(new InsetMathBoxed(buf));
	if (s == "fbox")
		return MathAtom(new InsetMathFBox(buf));
	if (s == "framebox")
		return MathAtom(new InsetMathMakebox(buf, true));
	if (s == "makebox")
		return MathAtom(new InsetMathMakebox(buf, false));
	if (s.substr(0, 8) == "xymatrix") {
		char spacing_code = '\0';
		Length spacing;
		bool equal_spacing = false;
		size_t const len = s.length();
		size_t i = 8;
		if (i < len && s[i] == '@') {
			++i;
			if (i < len && s[i] == '!') {
				equal_spacing = true;
				++i;
				if (i < len) {
					switch (s[i]) {
					case '0':
					case 'R':
					case 'C':
						spacing_code = static_cast<char>(s[i]);
					}
				}
			} else if (i < len) {
				switch (s[i]) {
				case 'R':
				case 'C':
				case 'M':
				case 'W':
				case 'H':
				case 'L':
					spacing_code = static_cast<char>(s[i]);
					++i;
					break;
				}
				if (i < len && s[i] == '=') {
					++i;
					spacing = Length(to_ascii(s.substr(i)));
				}
			}
		}
		return MathAtom(new InsetMathXYMatrix(buf, spacing, spacing_code,
			equal_spacing));
	}

	if (s == "Diagram")
		return MathAtom(new InsetMathDiagram(buf));
	if (s == "xrightarrow" || s == "xleftarrow" ||
		s == "xhookrightarrow" || s == "xhookleftarrow" ||
		s == "xRightarrow" || s == "xLeftarrow" ||
		s == "xleftrightarrow" || s == "xLeftrightarrow" ||
		s == "xrightharpoondown" || s == "xrightharpoonup" ||
		s == "xleftharpoondown" || s == "xleftharpoonup" ||
		s == "xleftrightharpoons" || s == "xrightleftharpoons" ||
		s == "xmapsto")
		return MathAtom(new InsetMathXArrow(buf, s));
	if (s == "split" || s == "alignedat")
		return MathAtom(new InsetMathSplit(buf, s));
	if (s == "cases")
		return MathAtom(new InsetMathCases(buf));
	if (s == "substack")
		return MathAtom(new InsetMathSubstack(buf));
	if (s == "subarray" || s == "array")
		return MathAtom(new InsetMathArray(buf, s, 1, 1));
	if (s == "sqrt")
		return MathAtom(new InsetMathSqrt(buf));
	if (s == "root")
		return MathAtom(new InsetMathRoot(buf));
	if (s == "tabular")
		return MathAtom(new InsetMathTabular(buf, s, 1, 1));
	if (s == "stackrel")
		return MathAtom(new InsetMathStackrel(buf, false));
	// This string value is only for math toolbar use, no LaTeX name
	if (s == "stackrelthree")
		return MathAtom(new InsetMathStackrel(buf, true));
	if (s == "binom")
		return MathAtom(new InsetMathBinom(buf, InsetMathBinom::BINOM));
	if (s == "dbinom")
		return MathAtom(new InsetMathBinom(buf, InsetMathBinom::DBINOM));
	if (s == "tbinom")
		return MathAtom(new InsetMathBinom(buf, InsetMathBinom::TBINOM));
	if (s == "choose")
		return MathAtom(new InsetMathBinom(buf, InsetMathBinom::CHOOSE));
	if (s == "brace")
		return MathAtom(new InsetMathBinom(buf, InsetMathBinom::BRACE));
	if (s == "brack")
		return MathAtom(new InsetMathBinom(buf, InsetMathBinom::BRACK));
	if (s == "frac")
		return MathAtom(new InsetMathFrac(buf));
	if (s == "cfrac")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::CFRAC));
	if (s == "dfrac")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::DFRAC));
	if (s == "tfrac")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::TFRAC));
	if (s == "over")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::OVER));
	if (s == "nicefrac")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::NICEFRAC));
	if (s == "unitfrac")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::UNITFRAC));
	// These string values are only for math toolbar use, no LaTeX names
	if (s == "unitfracthree")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::UNITFRAC, 3));
	if (s == "unitone")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::UNIT, 1));
	if (s == "unittwo")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::UNIT));
	if (s == "cfracleft")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::CFRACLEFT));
	if (s == "cfracright")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::CFRACRIGHT));
	if (s == "case") // TODO: only if class is aastex(6|62)
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::AASTEX_CASE));
	//if (s == "infer")
	//	return MathAtom(new MathInferInset);
	if (s == "atop")
		return MathAtom(new InsetMathFrac(buf, InsetMathFrac::ATOP));
	if (s == "lefteqn")
		return MathAtom(new InsetMathLefteqn(buf));
	if (s == "boldsymbol")
		return MathAtom(new InsetMathBoldSymbol(buf, InsetMathBoldSymbol::AMS_BOLD));
	if (s == "bm")
		return MathAtom(new InsetMathBoldSymbol(buf, InsetMathBoldSymbol::BM_BOLD));
	if (s == "heavysymbol" || s == "hm")
		return MathAtom(new InsetMathBoldSymbol(buf, InsetMathBoldSymbol::BM_HEAVY));
	if (s == "color" || s == "normalcolor")
		return MathAtom(new InsetMathColor(buf, true));
	if (s == "textcolor")
		return MathAtom(new InsetMathColor(buf, false));
	if (s == "hphantom")
		return MathAtom(new InsetMathPhantom(buf, InsetMathPhantom::hphantom));
	if (s == "phantom")
		return MathAtom(new InsetMathPhantom(buf, InsetMathPhantom::phantom));
	if (s == "vphantom")
		return MathAtom(new InsetMathPhantom(buf, InsetMathPhantom::vphantom));
	if (s == "cancel")
		return MathAtom(new InsetMathCancel(buf, InsetMathCancel::cancel));
	if (s == "bcancel")
		return MathAtom(new InsetMathCancel(buf, InsetMathCancel::bcancel));
	if (s == "xcancel")
		return MathAtom(new InsetMathCancel(buf, InsetMathCancel::xcancel));
	if (s == "cancelto")
		return MathAtom(new InsetMathCancelto(buf));
	if (s == "smash")
		return MathAtom(new InsetMathPhantom(buf, InsetMathPhantom::smash));
	// The following 2 string values are only for math toolbar use, no LaTeX names
	if (s == "smashb")
		return MathAtom(new InsetMathPhantom(buf, InsetMathPhantom::smashb));
	if (s == "smasht")
		return MathAtom(new InsetMathPhantom(buf, InsetMathPhantom::smasht));
	if (s == "mathclap")
		return MathAtom(new InsetMathPhantom(buf, InsetMathPhantom::mathclap));
	if (s == "mathllap")
		return MathAtom(new InsetMathPhantom(buf, InsetMathPhantom::mathllap));
	if (s == "mathrlap")
		return MathAtom(new InsetMathPhantom(buf, InsetMathPhantom::mathrlap));
	if (s == "ensuremath")
		return MathAtom(new InsetMathEnsureMath(buf));
	if (s == "sideset")
		return MathAtom(new InsetMathSideset(buf, true, true));
	// The following 3 string values are only for math toolbar use, no LaTeX names
	if (s == "sidesetr")
		return MathAtom(new InsetMathSideset(buf, false, true));
	if (s == "sidesetl")
		return MathAtom(new InsetMathSideset(buf, true, false));
	if (s == "sidesetn")
		return MathAtom(new InsetMathSideset(buf, false, false));
	if (isSpecialChar(s))
		return MathAtom(new InsetMathSpecialChar(buf, s));
	if (s == " ")
		return MathAtom(new InsetMathSpace(buf, " ", ""));
	if (s == "regexp")
		return MathAtom(new InsetMathHull(buf, hullRegexp));

	return MathAtom(new InsetMathMacro(buf, s));
}


bool createInsetMath_fromDialogStr(docstring const & str, MathData & md)
{
	// An example str:
	// "ref LatexCommand ref\nreference \"sec:Title\"\n\\end_inset\n\n";
	docstring name;
	docstring body = split(str, name, ' ');

	if (name == "ref") {
		InsetCommandParams icp(REF_CODE);
		// FIXME UNICODE
		InsetCommand::string2params(to_utf8(str), icp);
		Encoding const * const utf8 = encodings.fromLyXName("utf8");
		OutputParams op(utf8);
		mathed_parse_cell(md, icp.getCommand(op, false, true));
	} else if (name == "mathspace") {
		InsetSpaceParams isp(true);
		InsetSpace::string2params(to_utf8(str), isp);
		InsetSpace is(isp);
		odocstringstream ods;
		otexstream os(ods);
		Encoding const * const ascii = encodings.fromLyXName("ascii");
		OutputParams op(ascii);
		is.latex(os, op);
		mathed_parse_cell(md, ods.str());
		if (md.size() == 2) {
			// remove "{}"
			if (md[1].nucleus()->asBraceInset())
				md.pop_back();
		}
	} else
		return false;

	if (md.size() != 1)
		return false;

	return md[0].nucleus();
}


} // namespace lyx
