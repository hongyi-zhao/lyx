// -*- C++ -*-
/**
 * \file MathStream.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef MATH_MATHSTREAM_H
#define MATH_MATHSTREAM_H

#include "InsetMath.h"

#include "TexRow.h"
#include "texstream.h"

#include "support/Changer.h"


namespace lyx {

class Encoding;
class MathAtom;
class MathData;

///
enum class MathMLVersion : int {
	mathml3,
	mathmlCore
};

// Similar to FontInfo and its related enums, but specifically for the math
// mode.
//
// All types have enumerations, like FontEnums.h, even though there are
// sometimes only two cases: this design ensures some future-proofness and
// ensures that you cannot inadvertently swap two values.
class MathFontInfo {
public:
	enum MathFontFamily {
		MATH_INHERIT_FAMILY = 0,
		MATH_NORMAL_FAMILY, // Default value in MathML.
		MATH_FRAKTUR_FAMILY,
		MATH_SANS_FAMILY,
		MATH_MONOSPACE_FAMILY,
		MATH_DOUBLE_STRUCK_FAMILY,
		MATH_SCRIPT_FAMILY,
		MATH_SMALL_CAPS // Not natively supported in any version of MathML.
	};

	enum MathFontSeries {
		MATH_INHERIT_SERIES = 0,
		MATH_MEDIUM_SERIES, // Default value in MathML. // Default value in MathML.
		MATH_BOLD_SERIES
	};

	enum MathFontShape {
		MATH_INHERIT_SHAPE = 0,
		MATH_UP_SHAPE,
		MATH_ITALIC_SHAPE // Default value in MathML mi, not outside.
	};

	MathFontInfo() :
		family_(MATH_INHERIT_FAMILY), series_(MATH_INHERIT_SERIES), shape_(MATH_INHERIT_SHAPE) {}
	MathFontInfo(const MathFontFamily family, const MathFontSeries series, const MathFontShape shape) :
		family_(family), series_(series), shape_(shape) {}

	/// Merges this font with another one, replacing all fields in this font
    /// by the ones in the argument if they are not "inherit".
	MathFontInfo mergeWith(const MathFontInfo& other);
	/// Replaces this font by the given one.
	void replaceBy(const MathFontInfo& other);
	/// Parses a LaTeX font macro into a MathFontInfo object (builder method).
	static MathFontInfo fromMacro(const docstring& tag);

	MathFontFamily family() const { return family_; }
	MathFontSeries series() const { return series_; }
	MathFontShape shape() const { return shape_; }

	/// Transforms this font into the mathvariant attribute for MathML.
	/// For MathML 3, all fonts are output as mathvariants; for MathML Core,
	/// almost all fonts are supposed to be output as characters.
	std::string toMathMLMathVariant(MathMLVersion mathml_version) const;
	/// Transforms this font into a class attribute for the HTML span tag.
	std::string toHTMLSpanClass() const;

	/// Converts the character into the closest Unicode character that encodes
	/// this font. If there is only a partial mapping, parts of the mapping are
    /// applied. For instance, take the character C and a bold-italic font.
    /// - If there is a bold-italic mapping for this character, it is returned.
    /// - If there is only a bold mapping for this character, a bold character
    ///   is returned. This font encoding is the closest one to the font.
    /// - If there are two mappings (one bold, one italic), one of them is
    ///   returned (arbitrary choice between the two).
    /// - If there are no mappings, the original character is returned.
    /// The mappings are defined in the global variable theMathVariantList.
    ///
    /// The character is supposed to be a single Latin letter (a-z, A-Z) or
    /// digit (0-9) or the entity encoding a Greek character (0x3b1-0x3c9
	/// for lower case, 0x3b1-0x3c9 for upper case), exactly like the
    /// `unicode_alphanum_variants` file.
	///
	/// If in_text, the default shape is up. If not in_text, the default shape
	/// is italic. This behaviour matches that of MathMLStream::in_text_.
	[[nodiscard]]
	docstring convertCharacterToUnicodeWithFont(const docstring & c, bool in_text) const;
	/// Converts the character into the closest Unicode character that encodes
	/// this font as an entity if the character is not ASCII.
	/// Also see convertCharacterToUnicodeWithFont.
	[[nodiscard]]
	docstring convertCharacterToUnicodeEntityWithFont(const docstring & c, bool in_text) const;

private:
	MathFontFamily family_;
	MathFontSeries series_;
	MathFontShape shape_;

	/// Transforms this font into the mathvariant attribute for MathML 3.
	std::string toMathVariantForMathML3() const;
	/// Transforms this font into the mathvariant attribute for MathML Core.
	std::string toMathVariantForMathMLCore() const;
};

//
// LaTeX/LyX
//

class TeXMathStream {
public:
	///
	enum OutputType {
		wsDefault,
		wsDryrun,
		wsPreview,
		wsSearchAdv
	};
	///
	enum UlemCmdType {
		NONE,
		UNDERLINE,
		STRIKEOUT
	};
	///
	explicit TeXMathStream(otexrowstream & os, bool fragile = false,
	                       bool latex = false, OutputType output = wsDefault,
	                       Encoding const * encoding = nullptr);
	///
	~TeXMathStream();
	///
	int line() const { return line_; }
	///
	bool fragile() const { return fragile_; }
	///
	bool latex() const { return latex_; }
	///
	OutputType output() const { return output_; }
	///
	otexrowstream & os() { return os_; }
	///
	TexRow & texrow() { return os_.texrow(); }
	///
	bool & firstitem() { return firstitem_; }
	///
	void addlines(unsigned int);
	/// record whether we can write an immediately following newline char
	void canBreakLine(bool breakline) { canbreakline_ = breakline; }
	/// tell whether we can write an immediately following newline char
	bool canBreakLine() const { return canbreakline_; }
	/// record whether we have to take care for striking out display math
	void strikeoutMath(bool mathsout) { mathsout_ = mathsout; }
	/// tell whether we have to take care for striking out display math
	bool strikeoutMath() const { return mathsout_; }
	/// record which ulem command type we are inside
	void ulemCmd(UlemCmdType ulemcmd) { ulemcmd_ = ulemcmd; }
	/// tell which ulem command type we are inside
	UlemCmdType ulemCmd() const { return ulemcmd_; }
	/// writes space if next thing is isalpha()
	void pendingSpace(bool space);
	/// writes space if next thing is isalpha()
	bool pendingSpace() const { return pendingspace_; }
	/// write braces if a space is pending and next char is [
	/// or when a prime immediately follows a superscript
	void useBraces(bool braces);
	/// write braces if a space is pending and next char is [
	/// or when a prime immediately follows a superscript
	bool useBraces() const { return usebraces_; }
	/// tell whether to write the closing brace of \ensuremath
	void pendingBrace(bool brace);
	/// tell whether to write the closing brace of \ensuremath
	bool pendingBrace() const { return pendingbrace_; }
	/// tell whether we are in text mode or not when producing latex code
	void textMode(bool textmode);
	/// tell whether we are in text mode or not when producing latex code
	bool textMode() const { return textmode_; }
	/// tell whether we are allowed to switch mode when producing latex code
	void lockedMode(bool locked);
	/// tell whether we are allowed to switch mode when producing latex code
	bool lockedMode() const { return locked_; }
	/// tell whether to use only ascii chars when producing latex code
	void asciiOnly(bool ascii);
	/// tell whether to use only ascii chars when producing latex code
	bool asciiOnly() const { return ascii_; }
	/// tell whether we are in a MathClass inset
	void inMathClass(bool mathclass) { mathclass_ = mathclass; }
	/// tell whether we are in a MathClass inset
	bool inMathClass() const { return mathclass_; }
	/// LaTeX encoding
	Encoding const * encoding() const { return encoding_; }

	/// Temporarily change the TexRow information about the outer row entry.
	Changer changeRowEntry(TexRow::RowEntry const & entry);
	/// TexRow::starts the innermost outer math inset
	/// returns true if the outer row entry will appear at this line
	bool startOuterRow();
private:
	///
	otexrowstream & os_;
	/// do we have to write \\protect sometimes
	bool fragile_ = false;
	/// are we at the beginning of an MathData?
	bool firstitem_ = false;
	/// are we writing to .tex?
	int latex_ = false;
	/// output type (default, source preview, instant preview)?
	OutputType output_ = wsDefault;
	/// do we have a space pending?
	bool pendingspace_ = false;
	/// do we have to write braces when a space is pending and [ follows,
	/// or when a prime immediately follows a superscript?
	bool usebraces_ = false;
	/// do we have a brace pending?
	bool pendingbrace_ = false;
	/// are we in text mode when producing latex code?
	bool textmode_ = false;
	/// are we allowed to switch mode when producing latex code?
	bool locked_ = false;
	/// should we use only ascii chars when producing latex code?
	bool ascii_ = false;
	/// are we allowed to output an immediately following newline?
	bool canbreakline_ = true;
	/// should we take care for striking out display math?
	bool mathsout_ = false;
	/// what ulem command are we inside (none, underline, strikeout)?
	UlemCmdType ulemcmd_ = NONE;
	///
	int line_ = 0;
	///
	Encoding const * encoding_ = nullptr;
	/// Row entry we are in
	TexRow::RowEntry row_entry_ = TexRow::row_none;
	/// whether we are in a MathClass inset
	bool mathclass_ = false;
};

///
TeXMathStream & operator<<(TeXMathStream &, MathAtom const &);
///
TeXMathStream & operator<<(TeXMathStream &, MathData const &);
///
TeXMathStream & operator<<(TeXMathStream &, docstring const &);
///
TeXMathStream & operator<<(TeXMathStream &, char const * const);
///
TeXMathStream & operator<<(TeXMathStream &, char);
///
TeXMathStream & operator<<(TeXMathStream &, int);
///
TeXMathStream & operator<<(TeXMathStream &, unsigned int);

/// ensure correct mode, possibly by opening \ensuremath or \lyxmathsym
bool ensureMath(TeXMathStream & os, bool needs_mathmode = true,
                bool macro = false, bool textmode_macro = false);

/// ensure the requested mode, possibly by closing \ensuremath or \lyxmathsym
int ensureMode(TeXMathStream & os, InsetMath::mode_type mode, bool locked, bool ascii);


/**
 * MathEnsurer - utility class for ensuring math mode
 *
 * A local variable of this type can be used to either ensure math mode
 * or delay the writing of a pending brace when outputting LaTeX.
 * A LyX InsetMathMacro is always assumed needing a math mode environment, while
 * no assumption is made for macros defined through \newcommand or \def.
 *
 * Example 1:
 *
 *      MathEnsurer ensurer(os);
 *
 * If not already in math mode, inserts an \ensuremath command followed
 * by an open brace. This brace will be automatically closed when exiting
 * math mode. Math mode is automatically exited when writing something
 * that doesn't explicitly require math mode.
 *
 * Example 2:
 *
 *      MathEnsurer ensurer(os, false);
 *
 * Simply suspend writing a closing brace until the end of ensurer's scope.
 *
 * Example 3:
 *
 *      MathEnsurer ensurer(os, needs_mathmode, true, textmode_macro);
 *
 * This form is mainly used for math macros as they are treated specially.
 * In essence, the macros defined in the lib/symbols file and tagged as
 * textmode will be enclosed in \lyxmathsym if they appear in a math mode
 * environment, while macros defined in the preamble or ERT are left as is.
 * The third parameter must be set to true and the fourth parameter has also
 * to be specified. Only the following 3 different cases are handled.
 *
 * When the needs_mathmode parameter is true the behavior is as in Example 1.
 * This is the case for a LyX InsetMathMacro or a macro not tagged as textmode.
 *
 * When the needs_mathmode and textmode_macro parameters are both false the
 * macro is left in the same (text or math mode) environment it was entered.
 * This is because it is assumed that the macro was designed for that mode
 * and we have no way to tell the contrary.
 * This is the case for macros defined by using \newcommand or \def in ERT.
 *
 * When the needs_mathmode parameter is false while textmode_macro is true the
 * macro will be enclosed in \lyxmathsym if it appears in a math mode environment.
 * This is the case for the macros tagged as textmode in lib/symbols.
 */
class MathEnsurer
{
public:
	///
	explicit MathEnsurer(TeXMathStream & os, bool needs_mathmode = true,
	                     bool macro = false, bool textmode_macro = false)
		: os_(os), brace_(ensureMath(os, needs_mathmode, macro, textmode_macro)) {}
	///
	~MathEnsurer() { os_.pendingBrace(brace_); }
private:
	///
	TeXMathStream & os_;
	///
	bool brace_;
};


/**
 * ModeSpecifier - utility class for specifying a given mode (math or text)
 *
 * A local variable of this type can be used to specify that a command or
 * environment works in a given mode. For example, \mbox works in text
 * mode, but \boxed works in math mode. Note that no mode changing commands
 * are needed, but we have to track the current mode, hence this class.
 * This is only used when exporting to latex and helps determining whether
 * the mode needs being temporarily switched when a command would not work
 * in the current mode. As there are cases where this switching is to be
 * avoided, the optional third parameter can be used to lock the mode.
 * When the mode is locked, the optional fourth parameter specifies whether
 * strings are to be output by using a suitable ascii representation.
 *
 * Example 1:
 *
 *      ModeSpecifier specifier(os, TEXT_MODE);
 *
 * Sets the current mode to text mode and allows mode switching.
 *
 * Example 2:
 *
 *      ModeSpecifier specifier(os, TEXT_MODE, true);
 *
 * Sets the current mode to text mode and disallows mode switching.
 *
 * Example 3:
 *
 *      ModeSpecifier specifier(os, TEXT_MODE, true, true);
 *
 * Sets the current mode to text mode, disallows mode switching, and outputs
 * strings as ascii only.
 *
 * At the end of specifier's scope the mode is reset to its previous value.
 */
class ModeSpecifier
{
public:
	///
	explicit ModeSpecifier(TeXMathStream & os, InsetMath::mode_type mode,
	                       bool locked = false, bool ascii = false)
		: os_(os), oldmodes_(ensureMode(os, mode, locked, ascii)) {}
	///
	~ModeSpecifier()
	{
		os_.textMode(oldmodes_ & 0x01);
		os_.lockedMode(oldmodes_ & 0x02);
		os_.asciiOnly(oldmodes_ & 0x04);
	}
private:
	///
	TeXMathStream & os_;
	///
	int oldmodes_;
};



//
//  MathML
//


/// Start tag.
class MTag {
public:
	///
	MTag(const std::string & tag, const std::string & attr = std::string())
		: tag_(tag), attr_(attr) {}
	///
	std::string tag_;
	///
	std::string attr_;
};

/// Start inline tag.
class MTagInline {
public:
	///
	MTagInline(const std::string & tag, const std::string & attr = std::string())
	        : tag_(tag), attr_(attr) {}
	///
	std::string tag_;
	///
	std::string attr_;
};


/// End tag.
class ETag {
public:
	///
	explicit ETag(const std::string & tag) : tag_(tag) {}
	///
	std::string tag_;
};


/// End inlinetag.
class ETagInline {
public:
	///
	explicit ETagInline(const std::string & tag) : tag_(tag) {}
	///
	std::string tag_;
};


/// Compound tag (no content, directly closed).
class CTag {
public:
	///
	CTag(const std::string & tag, std::string const & attr = "")
            : tag_(tag), attr_(attr) {}
	///
	std::string tag_;
    ///
    std::string attr_;
};


/// Signalling elements for font handling. They do not output anything per se,
/// they alter the state of the stream to either start or stop respecting
/// fonts (i.e. output Unicode entities encoding the font, such as
/// "Mathematical Italic Small A" &#1d44e;).
struct StartRespectFont{};
struct StopRespectFont{};


/// Throw MathExportException to signal that the attempt to export
/// some math in the current format did not succeed. E.g., we can't
/// export xymatrix as MathML, so that will throw, and we'll fall back
/// to images.
class MathExportException : public std::exception {};


class MathMLStream {
public:
	/// Builds a stream proxy for os; the MathML namespace is given by xmlns
	/// (supposed to be already defined elsewhere in the document).
	explicit MathMLStream(odocstream & os, std::string const & xmlns = "",
	                      MathMLVersion version = MathMLVersion::mathml3);
	///
	void cr();
	/// Indentation when nesting tags
	int & tab() { return tab_; }
	///
	void defer(docstring const &);
	///
	void defer(std::string const &);
	///
	docstring deferred() const;
	///
	bool inText() const { return text_level_ != nlevel; }
	///
	std::string xmlns() const { return xmlns_; }
	///
	MathMLVersion version() const { return version_; }
	/// Returns the tag name prefixed by the name space if needed.
	std::string namespacedTag(std::string const & tag) const {
		return (xmlns().empty() ? "" : xmlns() + ":") + tag;
	}
	/// Returns the current math style in the stream.
	const MathStyle & getFontMathStyle() const { return font_math_style_; }
	/// Sets the current math style in the stream.
	void setFontMathStyle(const MathStyle style) { font_math_style_ = style; }
	/// Gets a mutable reference to the stream's current font.
	MathFontInfo& fontInfo() { return current_font_; }
private:
	/// Check whether it makes sense to start a <mtext>
	void beforeText();
	/// Check whether there is a <mtext> to close here
	void beforeTag();
	/// Determines whether characters should be mapped for font variations.
	/// (True if characters must respect font, in particular.)
	bool needsFontMapping() const;
	///
	odocstream & os_;
	///
	int tab_ = 0;
	///
	int nesting_level_ = 0;
	static const int nlevel = -1000;
	///
	int text_level_ = nlevel;
	///
	bool in_mtext_ = false;
	///
	odocstringstream deferred_;
	///
	std::string xmlns_;
	///
	MathMLVersion version_;
	/// The only important part of a FontInfo object.
	MathStyle font_math_style_;
	/// Current font (which might be nested).
	MathFontInfo current_font_;
	/// whether the output shall respect the current font
	bool respect_font_ = false;
	///
	friend class SetMode;
	friend MathMLStream & operator<<(MathMLStream &, MathAtom const &);
	friend MathMLStream & operator<<(MathMLStream &, MathData const &);
	friend MathMLStream & operator<<(MathMLStream &, docstring const &);
	friend MathMLStream & operator<<(MathMLStream &, MTag const &);
	friend MathMLStream & operator<<(MathMLStream &, MTagInline const &);
	friend MathMLStream & operator<<(MathMLStream &, ETag const &);
	friend MathMLStream & operator<<(MathMLStream &, ETagInline const &);
	friend MathMLStream & operator<<(MathMLStream &, CTag const &);
	friend MathMLStream & operator<<(MathMLStream &, StartRespectFont);
	friend MathMLStream & operator<<(MathMLStream &, StopRespectFont);
};

///
MathMLStream & operator<<(MathMLStream &, MathAtom const &);
///
MathMLStream & operator<<(MathMLStream &, MathData const &);
///
MathMLStream & operator<<(MathMLStream &, docstring const &);
///
MathMLStream & operator<<(MathMLStream &, char const *);
///
MathMLStream & operator<<(MathMLStream &, char);
///
MathMLStream & operator<<(MathMLStream &, char_type);
///
MathMLStream & operator<<(MathMLStream &, MTag const &);
///
MathMLStream & operator<<(MathMLStream &, MTagInline const &);
///
MathMLStream & operator<<(MathMLStream &, ETag const &);
///
MathMLStream & operator<<(MathMLStream &, ETagInline const &);
///
MathMLStream & operator<<(MathMLStream &, CTag const &);
/// Starts respecting fonts until meeting StopRespectFont.
MathMLStream & operator<<(MathMLStream &, StartRespectFont);
/// Stops respecting fonts.
MathMLStream & operator<<(MathMLStream &, StopRespectFont);


/// A simpler version of ModeSpecifier, for MathML
class SetMode {
public:
	///
	explicit SetMode(MathMLStream & ms, bool text);
	///
	~SetMode();
private:
	///
	MathMLStream & ms_;
	///
	int old_text_level_;
};


class HtmlStream {
public:
	///
	explicit HtmlStream(odocstream & os);
	///
	void cr();
	///
	odocstream & os() { return os_; }
	///
	int line() const { return line_; }
	///
	int & tab() { return tab_; }
	///
	friend HtmlStream & operator<<(HtmlStream &, char const *);
	///
	void defer(docstring const &);
	///
	void defer(std::string const &);
	///
	docstring deferred() const;
	///
	bool inText() const { return in_text_; }
	/// Gets a mutable reference to the stream's current font.
	MathFontInfo& fontInfo() { return current_font_; }
private:
	///
	void setTextMode(bool t) { in_text_ = t; }
	///
	odocstream & os_;
	///
	int tab_;
	///
	int line_;
	///
	bool in_text_;
	///
	odocstringstream deferred_;
	/// Current font (which might be nested).
	MathFontInfo current_font_;
	///
	friend class SetHTMLMode;
};

///
HtmlStream & operator<<(HtmlStream &, MathAtom const &);
///
HtmlStream & operator<<(HtmlStream &, MathData const &);
///
HtmlStream & operator<<(HtmlStream &, docstring const &);
///
HtmlStream & operator<<(HtmlStream &, char const *);
///
HtmlStream & operator<<(HtmlStream &, char);
///
HtmlStream & operator<<(HtmlStream &, char_type);
///
HtmlStream & operator<<(HtmlStream &, MTag const &);
///
HtmlStream & operator<<(HtmlStream &, ETag const &);


class SetHTMLMode {
public:
	///
	explicit SetHTMLMode(HtmlStream & os, bool text);
	///
	~SetHTMLMode();
private:
	///
	HtmlStream & os_;
	///
	bool was_text_;
};


//
// Debugging
//

class NormalStream {
public:
	///
	explicit NormalStream(odocstream & os) : os_(os) {}
	///
	odocstream & os() { return os_; }
private:
	///
	odocstream & os_;
};

///
NormalStream & operator<<(NormalStream &, MathAtom const &);
///
NormalStream & operator<<(NormalStream &, MathData const &);
///
NormalStream & operator<<(NormalStream &, docstring const &);
///
NormalStream & operator<<(NormalStream &, char const *);
///
NormalStream & operator<<(NormalStream &, char);
///
NormalStream & operator<<(NormalStream &, int);


//
// Maple
//


class MapleStream {
public:
	///
	explicit MapleStream(odocstream & os, bool maxima = false) : os_(os), maxima_(maxima) {}
	///
	odocstream & os() { return os_; }
	bool maxima() { return maxima_; }
private:
	///
	odocstream & os_;
	///
	bool maxima_;
};


///
MapleStream & operator<<(MapleStream &, MathAtom const &);
///
MapleStream & operator<<(MapleStream &, MathData const &);
///
MapleStream & operator<<(MapleStream &, docstring const &);
///
MapleStream & operator<<(MapleStream &, char_type);
///
MapleStream & operator<<(MapleStream &, char const *);
///
MapleStream & operator<<(MapleStream &, char);
///
MapleStream & operator<<(MapleStream &, int);


//
// Maxima
//


class MaximaStream {
public:
	///
	explicit MaximaStream(odocstream & os) : os_(os) {}
	///
	odocstream & os() { return os_; }
private:
	///
	odocstream & os_;
};


///
MaximaStream & operator<<(MaximaStream &, MathAtom const &);
///
MaximaStream & operator<<(MaximaStream &, MathData const &);
///
MaximaStream & operator<<(MaximaStream &, docstring const &);
///
MaximaStream & operator<<(MaximaStream &, char_type);
///
MaximaStream & operator<<(MaximaStream &, char const *);
///
MaximaStream & operator<<(MaximaStream &, char);
///
MaximaStream & operator<<(MaximaStream &, int);


//
// Mathematica
//


class MathematicaStream {
public:
	///
	explicit MathematicaStream(odocstream & os) : os_(os) {}
	///
	odocstream & os() { return os_; }
private:
	///
	odocstream & os_;
};


///
MathematicaStream & operator<<(MathematicaStream &, MathAtom const &);
///
MathematicaStream & operator<<(MathematicaStream &, MathData const &);
///
MathematicaStream & operator<<(MathematicaStream &, docstring const &);
///
MathematicaStream & operator<<(MathematicaStream &, char const *);
///
MathematicaStream & operator<<(MathematicaStream &, char);
///
MathematicaStream & operator<<(MathematicaStream &, int);


//
// Octave
//


class OctaveStream {
public:
	///
	explicit OctaveStream(odocstream & os) : os_(os) {}
	///
	odocstream & os() { return os_; }
private:
	///
	odocstream & os_;
};

///
OctaveStream & operator<<(OctaveStream &, MathAtom const &);
///
OctaveStream & operator<<(OctaveStream &, MathData const &);
///
OctaveStream & operator<<(OctaveStream &, docstring const &);
///
OctaveStream & operator<<(OctaveStream &, char_type);
///
OctaveStream & operator<<(OctaveStream &, char const *);
///
OctaveStream & operator<<(OctaveStream &, char);
///
OctaveStream & operator<<(OctaveStream &, int);


docstring convertDelimToXMLEscape(docstring const & name);

} // namespace lyx

#endif
