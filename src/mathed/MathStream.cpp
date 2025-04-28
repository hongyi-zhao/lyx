/**
 * \file MathStream.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "MathStream.h"

#include "MathFactory.h"
#include "MathData.h"
#include "MathExtern.h"

#include "TexRow.h"


#include "support/debug.h"
#include "support/docstring.h"
#include "support/textutils.h"

#include <algorithm>
#include <cstring>
#include <FontInfo.h>

#include "support/lstrings.h"

using namespace std;

namespace lyx {


//////////////////////////////////////////////////////////////////////


MathFontInfo MathFontInfo::mergeWith(const MathFontInfo& other)
{
	MathFontInfo old = *this;

	if (other.family_ != family_ && other.family_ != MATH_INHERIT_FAMILY) {
		family_ = other.family_;
	}
	if (other.series_ != series_ && other.series_ != MATH_INHERIT_SERIES) {
		series_ = other.series_;
	}
	if (other.shape_ != shape_ && other.shape_ != MATH_INHERIT_SHAPE) {
		shape_ = other.shape_;
	}

	return old;
}


void MathFontInfo::replaceBy(const MathFontInfo& other)
{
	*this = other;
}


MathFontInfo MathFontInfo::fromMacro(const docstring& tag)
{
	MathFontInfo font;
	if (tag == "mathnormal" || tag == "mathrm"
			|| tag == "text" || tag == "textnormal"
			|| tag == "textrm" || tag == "textup"
			|| tag == "textmd")
		font.shape_ = MATH_UP_SHAPE;
	else if (tag == "frak" || tag == "mathfrak")
		font.family_ = MATH_FRAKTUR_FAMILY;
	else if (tag == "mathbf" || tag == "textbf"
			|| tag == "boldsymbol" || tag == "bm" || tag == "hm")
		font.series_ = MATH_BOLD_SERIES;
	else if (tag == "mathbb" || tag == "mathbbm"
			 || tag == "mathds")
		font.family_ = MATH_DOUBLE_STRUCK_FAMILY;
	else if (tag == "mathcal")
		font.family_ = MATH_SCRIPT_FAMILY;
	else if (tag == "mathit" || tag == "textsl"
			 || tag == "emph" || tag == "textit")
		font.shape_ = MATH_ITALIC_SHAPE;
	else if (tag == "mathsf" || tag == "textsf")
		font.family_ = MATH_SANS_FAMILY;
	else if (tag == "mathtt" || tag == "texttt")
		font.family_ = MATH_MONOSPACE_FAMILY;
	else if (tag == "textipa" || tag == "textsc" || tag == "noun")
		font.family_ = MATH_SMALL_CAPS;
	// Otherwise, the tag is not recognised, use the default font.

	return font;
}


std::string MathFontInfo::toMathMLMathVariant(MathMLVersion mathml_version) const
{
	return mathml_version == MathMLVersion::mathml3 ?
		toMathVariantForMathML3() : toMathVariantForMathMLCore();
}


std::string MathFontInfo::toMathVariantForMathML3() const
{
	// mathvariant is the way MathML 3 encodes fonts.
	// Not all combinations are supported. Official list:
	// https://www.w3.org/TR/MathML3/chapter3.html#presm.commatt
	// "initial", "tailed", "looped", and "stretched" are not implemented,
	// as they are only useful for Arabic characters (for which LyX has no
	// support right now).
	switch (family_) {
	case MATH_MONOSPACE_FAMILY:
		return "monospace";
	case MATH_DOUBLE_STRUCK_FAMILY:
		return "double-struck";
	case MATH_FRAKTUR_FAMILY:
		return series_ == MATH_BOLD_SERIES ? "bold-fraktur" : "fraktur";
	case MATH_SCRIPT_FAMILY:
		return series_ == MATH_BOLD_SERIES ? "bold-script" : "script";
	case MATH_SANS_FAMILY:
		if (series_ == MATH_BOLD_SERIES) {
			return shape_ == MATH_UP_SHAPE ? "bold-sans-serif" : "sans-serif-bold-italic";
		}
		return shape_ == MATH_UP_SHAPE ? "sans-serif" : "sans-serif-italic";
	case MATH_NORMAL_FAMILY:
	case MATH_INHERIT_FAMILY: // Only consider the other two attributes.
		if (series_ == MATH_BOLD_SERIES) {
			return shape_ == MATH_UP_SHAPE ? "bold" : "bold-italic";
		}
		return shape_ == MATH_UP_SHAPE ? "normal" : "italic";
	case MATH_SMALL_CAPS:
		// No valid value to return.
		return "";
	}

	// Better safe than sorry.
	LYXERR(Debug::MATHED,
		"Unexpected case in MathFontInfo::toMathVariantForMathML3: family_ = " << family_
			<< ", series = " << series_ << ", shape = " << shape_);
	return "";
}


std::string MathFontInfo::toMathVariantForMathMLCore() const
{
	return shape_ == MATH_UP_SHAPE ? "normal" : "";
}


std::string MathFontInfo::toHTMLSpanClass() const
{
	std::string span_class;
	switch (family_) {
	case MATH_INHERIT_FAMILY:
	case MATH_NORMAL_FAMILY:
		break;
	case MATH_FRAKTUR_FAMILY:
		span_class = "fraktur";
		break;
	case MATH_SANS_FAMILY:
		span_class = "sans";
		break;
	case MATH_MONOSPACE_FAMILY:
		span_class = "monospace";
		break;
	case MATH_DOUBLE_STRUCK_FAMILY:
		// This style does not exist in HTML and cannot be implemented in CSS.
		break;
	case MATH_SCRIPT_FAMILY:
		span_class = "script";
		break;
	case MATH_SMALL_CAPS:
		span_class = "noun";
		break;
	}
	// Explicitly match the cases with an empty output. This ensures that we catch at runtime
	// invalid values for the enum while keeping compile-time warnings.
	if (span_class.empty() && (family_ == MATH_INHERIT_FAMILY || family_ == MATH_NORMAL_FAMILY || family_ == MATH_DOUBLE_STRUCK_FAMILY)) {
		LYXERR(Debug::MATHED,
			"Unexpected case in MathFontInfo::toHTMLSpanClass: family_ = " << family_
				<< ", series = " << series_ << ", shape = " << shape_);
	}

	if (series_ == MATH_BOLD_SERIES) {
		if (!span_class.empty()) span_class += "-";
		span_class += "bold";
	}

	if (shape_ == MATH_ITALIC_SHAPE) {
		if (!span_class.empty()) span_class += "-";
		span_class += "italic";
	}

	return span_class;
}


docstring MathFontInfo::convertCharacterToUnicodeEntityWithFont(const docstring & c, bool in_text) const
{
	docstring mapped = convertCharacterToUnicodeWithFont(c, in_text);
	if (mapped == c || mapped.length() == 1) {
		return mapped;
	}
	// Otherwise, it's an entity, like 0x1d44e (as a hexadecimal number).
	return from_ascii("&#") + mapped + from_ascii(";");
}


docstring MathFontInfo::convertCharacterToUnicodeWithFont(const docstring & c, bool in_text) const
{
	// Not much to do if the query is empty.
	if (c.empty()) {
		return c;
	}

	MathVariantList const & mvl = mathedVariantList();

	// If this character is unknown, exit early.
	auto it = mvl.find(support::ascii_lowercase(c));
	if (it == mvl.end() && c.size() >= 2 && c[0] == '0' && c[1] == 'x') {
		// If the character starts with "0x", it might be a hex encoding,
		// like "0x1d73d": also look for the variant that browsers support
		// best, "x1d73d".
		it = mvl.find(support::ascii_lowercase(c.substr(1)));
	}
	if (it == mvl.end()) {
		return c;
	}

	// Check for the best variant. Heuristically:
	// - First check the font type: normal, script, fraktur, etc. This is the
    //   most constraining factor.
	// - Second, check for shape and series.
	// If the variant for one factor does not exist, ignore it and continue
    // the search. Hence, we store the copies of family, shape, and series.
	UnicodeVariants const & variants = it->second;

	MathFontFamily family = family_;
	MathFontSeries series = series_;
	MathFontShape shape = shape_;

	if (family == MATH_INHERIT_FAMILY) {
		family = MATH_NORMAL_FAMILY;
	}
	if (series == MATH_INHERIT_SERIES) {
		series = MATH_MEDIUM_SERIES;
	}
	if (shape == MATH_INHERIT_SHAPE) {
		shape = in_text ? MATH_UP_SHAPE : MATH_ITALIC_SHAPE;
	}

	if (family == MATH_MONOSPACE_FAMILY) {
		if (!variants.monospace.empty()) return variants.monospace;
		family = MATH_NORMAL_FAMILY;
	}

	if (family == MATH_DOUBLE_STRUCK_FAMILY) {
		if (!variants.double_struck.empty()) return variants.double_struck;
		family = MATH_NORMAL_FAMILY;
	}

	if (family == MATH_FRAKTUR_FAMILY) {
		if (series == MATH_BOLD_SERIES) {
			if (!variants.bold_fraktur.empty()) return variants.bold_fraktur;
			series = MATH_MEDIUM_SERIES;
		}

		if (series == MATH_MEDIUM_SERIES) {
			if (!variants.fraktur.empty()) return variants.fraktur;
		}

		family = MATH_NORMAL_FAMILY;
	}

	if (family == MATH_SCRIPT_FAMILY) {
		if (series == MATH_BOLD_SERIES) {
			if (!variants.bold_script.empty()) return variants.bold_script;
			series = MATH_MEDIUM_SERIES;
		}

		if (series == MATH_MEDIUM_SERIES) {
			if (!variants.script.empty()) return variants.script;
		}

		family = MATH_NORMAL_FAMILY;
	}

	if (family == MATH_SANS_FAMILY) {
		if (series == MATH_BOLD_SERIES) {
			if (shape == MATH_UP_SHAPE) {
				if (!variants.bold_sans.empty()) return variants.bold_sans;
			} else {
				if (!variants.bold_italic_sans.empty()) return variants.bold_italic_sans;
			}
			series = MATH_MEDIUM_SERIES;
		}

		if (series == MATH_MEDIUM_SERIES) {
			if (shape == MATH_UP_SHAPE) {
				if (!variants.sans.empty()) return variants.sans;
			} else {
				if (!variants.italic_sans.empty()) return variants.italic_sans;
			}
		}

		family = MATH_NORMAL_FAMILY;
	}

	if (family != MATH_NORMAL_FAMILY) {
		LYXERR(Debug::MATHED,
				"Unexpected case in MathFontInfo::convertCharacterToUnicodeWithFont"
				<<"(c = " << to_ascii(c) << ", in_text = " << in_text << "), unrecognised family: "
				<< "family_ = " << family_ << ", series = " << series_ << ", shape = " << shape_);
		// Continue processing to return a value that matches the other constraints.
	}

	if (series == MATH_BOLD_SERIES) {
		if (shape == MATH_UP_SHAPE) {
			if (!variants.bold.empty()) return variants.bold;
		} else {
			if (!variants.bold_italic.empty()) return variants.bold_italic;
		}
		series = MATH_MEDIUM_SERIES;
	}

	if (series == MATH_MEDIUM_SERIES) {
		if (shape == MATH_UP_SHAPE) {
			if (!variants.character.empty()) return variants.character;
		} else {
			if (!variants.italic.empty()) return variants.italic;
		}
	}

	// The previous cases should have matched, unless this code is not up to date.
	LYXERR(Debug::MATHED,
			"Unexpected case in MathFontInfo::convertCharacterToUnicodeWithFont"
			<<"(c = " << c << ", in_text = " << in_text << "), unrecognised series/shape: "
			<< "family_ = " << family_ << ", series = " << series_ << ", shape = " << shape_);
	return variants.character;
}


NormalStream & operator<<(NormalStream & ns, MathAtom const & at)
{
	at->normalize(ns);
	return ns;
}


NormalStream & operator<<(NormalStream & ns, MathData const & md)
{
	normalize(md, ns);
	return ns;
}


NormalStream & operator<<(NormalStream & ns, docstring const & s)
{
	ns.os() << s;
	return ns;
}


NormalStream & operator<<(NormalStream & ns, const string & s)
{
	ns.os() << from_utf8(s);
	return ns;
}


NormalStream & operator<<(NormalStream & ns, char const * s)
{
	ns.os() << s;
	return ns;
}


NormalStream & operator<<(NormalStream & ns, char c)
{
	ns.os() << c;
	return ns;
}


NormalStream & operator<<(NormalStream & ns, int i)
{
	ns.os() << i;
	return ns;
}



/////////////////////////////////////////////////////////////////


TeXMathStream & operator<<(TeXMathStream & ws, docstring const & s)
{
	// Skip leading '\n' if we had already output a newline char
	size_t const first =
		(s.length() > 0 && (s[0] != '\n' || ws.canBreakLine())) ? 0 : 1;

	// Check whether there's something to output
	if (s.length() <= first)
		return ws;

	if (ws.pendingBrace()) {
		ws.os() << '}';
		ws.pendingBrace(false);
		ws.pendingSpace(false);
		ws.textMode(true);
	} else if (ws.pendingSpace()) {
		if (isAlphaASCII(s[first]))
			ws.os() << ' ';
		else if (s[first] == '[' && ws.useBraces())
			ws.os() << "{}";
		else if (s[first] == ' ' && ws.textMode())
			ws.os() << '\\';
		ws.pendingSpace(false);
	} else if (ws.useBraces()) {
		if (s[first] == '\'')
			ws.os() << "{}";
		ws.useBraces(false);
	}
	ws.os() << s.substr(first);
	int lf = 0;
	char_type lastchar = 0;
	docstring::const_iterator dit = s.begin() + first;
	docstring::const_iterator end = s.end();
	for (; dit != end; ++dit) {
		lastchar = *dit;
		if (lastchar == '\n')
			++lf;
	}
	ws.addlines(lf);
	ws.canBreakLine(lastchar != '\n');
	return ws;
}


TeXMathStream::TeXMathStream(otexrowstream & os, bool fragile, bool latex,
                             OutputType output, Encoding const * encoding)
	: os_(os), fragile_(fragile), latex_(latex),
	  output_(output), encoding_(encoding)
{}


TeXMathStream::~TeXMathStream()
{
	if (pendingbrace_)
		os_ << '}';
	else if (pendingspace_)
		os_ << ' ';
}


void TeXMathStream::addlines(unsigned int n)
{
	line_ += n;
}


void TeXMathStream::pendingSpace(bool space)
{
	pendingspace_ = space;
	if (!space)
		usebraces_ = false;
}


void TeXMathStream::useBraces(bool braces)
{
	usebraces_ = braces;
}


void TeXMathStream::pendingBrace(bool brace)
{
	pendingbrace_ = brace;
}


void TeXMathStream::textMode(bool textmode)
{
	textmode_ = textmode;
}


void TeXMathStream::lockedMode(bool locked)
{
	locked_ = locked;
}


void TeXMathStream::asciiOnly(bool ascii)
{
	ascii_ = ascii;
}


Changer TeXMathStream::changeRowEntry(TexRow::RowEntry entry)
{
	return changeVar(row_entry_, entry);
}


bool TeXMathStream::startOuterRow()
{
	if (TexRow::isNone(row_entry_))
		return false;
	return texrow().start(row_entry_);
}


TeXMathStream & operator<<(TeXMathStream & ws, MathAtom const & at)
{
	at->write(ws);
	return ws;
}


TeXMathStream & operator<<(TeXMathStream & ws, MathData const & md)
{
	write(md, ws);
	return ws;
}


TeXMathStream & operator<<(TeXMathStream & ws, char const * s)
{
	ws << from_utf8(s);
	return ws;
}


TeXMathStream & operator<<(TeXMathStream & ws, char c)
{
	if (c == '\n' && !ws.canBreakLine())
		return ws;

	if (ws.pendingBrace()) {
		ws.os() << '}';
		ws.pendingBrace(false);
		ws.pendingSpace(false);
		ws.textMode(true);
	} else if (ws.pendingSpace()) {
		if (isAlphaASCII(c))
			ws.os() << ' ';
		else if (c == '[' && ws.useBraces())
			ws.os() << "{}";
		else if (c == ' ' && ws.textMode())
			ws.os() << '\\';
		ws.pendingSpace(false);
	} else if (ws.useBraces()) {
		if (c == '\'')
			ws.os() << "{}";
		ws.useBraces(false);
	}
	ws.os() << c;
	if (c == '\n')
		ws.addlines(1);
	ws.canBreakLine(c != '\n');
	return ws;
}


TeXMathStream & operator<<(TeXMathStream & ws, int i)
{
	if (ws.pendingBrace()) {
		ws.os() << '}';
		ws.pendingBrace(false);
		ws.textMode(true);
	}
	ws.os() << i;
	ws.canBreakLine(true);
	return ws;
}


TeXMathStream & operator<<(TeXMathStream & ws, unsigned int i)
{
	if (ws.pendingBrace()) {
		ws.os() << '}';
		ws.pendingBrace(false);
		ws.textMode(true);
	}
	ws.os() << i;
	ws.canBreakLine(true);
	return ws;
}


//////////////////////////////////////////////////////////////////////


MathMLStream::MathMLStream(odocstream & os, std::string const & xmlns, MathMLVersion version)
	: os_(os), xmlns_(xmlns), version_(version)
{
	if (inText())
		font_math_style_ = TEXT_STYLE;
	else
		font_math_style_ = DISPLAY_STYLE;
}


void MathMLStream::cr()
{
	os_ << '\n';
	for (int i = 0; i < tab(); ++i)
		os_ << ' ';
}


void MathMLStream::defer(docstring const & s)
{
	deferred_ << s;
}


void MathMLStream::defer(string const & s)
{
	deferred_ << from_utf8(s);
}


docstring MathMLStream::deferred() const
{
	return deferred_.str();
}

void MathMLStream::beforeText()
{
	if (!in_mtext_ && nesting_level_ == text_level_) {
		*this << MTagInline("mtext");
		in_mtext_ = true;
	}
}


void MathMLStream::beforeTag()
{
	if (in_mtext_ && nesting_level_ == text_level_ + 1) {
		in_mtext_ = false;
		*this << ETagInline("mtext");
	}
}


MathMLStream & operator<<(MathMLStream & ms, MathAtom const & at)
{
	at->mathmlize(ms);
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, MathData const & md)
{
	mathmlize(md, ms);
	return ms;
}


bool MathMLStream::needsFontMapping() const {
	// Condition written for *not* needing mapping, because it's easier
	// (every field has a default value). The function returns the
	// opposite because it's easier to understand.
	return !(
		(current_font_.family() == MathFontInfo::MathFontFamily::MATH_INHERIT_FAMILY ||
					current_font_.family() == MathFontInfo::MathFontFamily::MATH_NORMAL_FAMILY) &&
		(current_font_.series() == MathFontInfo::MathFontSeries::MATH_INHERIT_SERIES ||
		   			current_font_.series() == MathFontInfo::MathFontSeries::MATH_MEDIUM_SERIES) &&
		(current_font_.shape() == MathFontInfo::MathFontShape::MATH_INHERIT_SHAPE ||
					(in_mtext_ && current_font_.shape() == MathFontInfo::MathFontShape::MATH_UP_SHAPE) ||
					(!in_mtext_ && current_font_.shape() == MathFontInfo::MathFontShape::MATH_ITALIC_SHAPE))
	);
}


MathMLStream & operator<<(MathMLStream & ms, docstring const & s)
{
	ms.beforeText();
	if (!ms.respect_font_) {
		// Ignore fonts for now. This is especially useful for tags.
		ms.os_ << s;
	} else {
		// Only care about fonts if they are currently enabled.
		if (ms.version() == MathMLVersion::mathmlCore) {
			// New case: MathML uses Unicode characters to indicate fonts.
			// If possible, avoid doing the mapping: it involves looking up a hash
			// table and doing a lot of conditions *per character*.
			if (!ms.needsFontMapping()) {
				ms.os_ << s;
			} else {
				// Perform the conversion character per character (which might
				// mean consume a complete Greek entity!).
				docstring buf;
				bool within_entity = false;
				for (const char_type c : s) {
					if (!within_entity && c == '&') { // New entity.
						within_entity = true;
					} else if (within_entity && c == '#') { // Still new entity.
						// Nothing to do: unicode_alphanum_variants only has
						// the code point, not the full XML/HTML entity.
					} else if (within_entity && c == ';') { // End of entity.
						if (support::prefixIs(buf, 'x')) {
							// An HTML entity is typically &#x3B1;, but
							// unicode_alpha_num_variants has 0x3B1.
							buf.insert(0, from_ascii("0"));
						}
						ms.os_ << ms.current_font_.convertCharacterToUnicodeEntityWithFont(buf, ms.inText());
						buf.clear();
						within_entity = false;
					} else if (within_entity) { // Within new entity.
						buf += c;
					} else {
						if (!buf.empty()) {
							lyxerr << "Assertion failed in MathLMStream::operator<<(docstring): the buffer is not empty (" << buf << ") when processing a single character";
						}

						buf = docstring(1, c);
						ms.os_ << ms.current_font_.convertCharacterToUnicodeEntityWithFont(buf, ms.inText());
						buf.clear();
					}

					if (!within_entity && !buf.empty()) {
						lyxerr << "Assertion failed in MathLMStream::operator<<(docstring): not reading an entity "
						       << "while the buffer is not empty (" << buf << ")";
					}
				}
				if (!buf.empty()) {
					lyxerr << "Assertion failed in MathLMStream::operator<<(docstring): the buffer is not empty (" << buf << ") after processing the whole string";
					ms.os_ << ms.current_font_.convertCharacterToUnicodeEntityWithFont(buf, ms.inText());
				}
			}
		} else {
			// Old case (MathML3): MathML uses mathvariant to indicate fonts.
			ms.os_ << s;
		}
	}
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, char const * s)
{
	ms << from_utf8(s);
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, char c)
{
	ms << docstring(1, c);
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, char_type c)
{
	ms << docstring(1, c);
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, MTag const & t)
{
	ms.beforeTag();
	SetMode rawmode(ms, false);
	ms.cr();
	++ms.tab();
	ms.os_ << '<' << from_ascii(ms.namespacedTag(t.tag_));
	if (!t.attr_.empty())
		ms.os_ << " " << from_ascii(t.attr_);
	ms << ">";
	++ms.nesting_level_;
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, MTagInline const & t)
{
	ms.beforeTag();
	SetMode rawmode(ms, false);
	ms.cr();
	ms.os_ << '<' << from_ascii(ms.namespacedTag(t.tag_));
	if (!t.attr_.empty())
		ms.os_ << " " << from_ascii(t.attr_);
	ms << ">";
	++ms.nesting_level_;
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, ETag const & t)
{
	ms.beforeTag();
	SetMode rawmode(ms, false);
	if (ms.tab() > 0)
		--ms.tab();
	ms.cr();
	ms.os_ << "</" << from_ascii(ms.namespacedTag(t.tag_)) << ">";
	--ms.nesting_level_;
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, ETagInline const & t)
{
	ms.beforeTag();
	SetMode rawmode(ms, false);
	ms.os_ << "</" << from_ascii(ms.namespacedTag(t.tag_)) << ">";
	--ms.nesting_level_;
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, CTag const & t)
{
	ms.beforeTag();
	SetMode rawmode(ms, false);
	ms.cr();
	ms.os_ << "<" << from_ascii(ms.namespacedTag(t.tag_));
    if (!t.attr_.empty())
        ms.os_ << " " << from_utf8(t.attr_);
    ms.os_ << "/>";
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, StartRespectFont)
{
	ms.respect_font_ = true;
	return ms;
}


MathMLStream & operator<<(MathMLStream & ms, StopRespectFont)
{
	ms.respect_font_ = false;
	return ms;
}


//////////////////////////////////////////////////////////////////////


HtmlStream::HtmlStream(odocstream & os)
	: os_(os), tab_(0), line_(0), in_text_(false)
{}


void HtmlStream::defer(docstring const & s)
{
	deferred_ << s;
}


void HtmlStream::defer(string const & s)
{
	deferred_ << from_utf8(s);
}


docstring HtmlStream::deferred() const
{
	return deferred_.str();
}


HtmlStream & operator<<(HtmlStream & ms, MathAtom const & at)
{
	at->htmlize(ms);
	return ms;
}


HtmlStream & operator<<(HtmlStream & ms, MathData const & md)
{
	htmlize(md, ms);
	return ms;
}


HtmlStream & operator<<(HtmlStream & ms, char const * s)
{
	ms.os() << s;
	return ms;
}


HtmlStream & operator<<(HtmlStream & ms, char c)
{
	ms.os() << c;
	return ms;
}


HtmlStream & operator<<(HtmlStream & ms, char_type c)
{
	ms.os().put(c);
	return ms;
}


HtmlStream & operator<<(HtmlStream & ms, MTag const & t)
{
	ms.os() << '<' << from_ascii(t.tag_);
	if (!t.attr_.empty())
		ms.os() << " " << from_ascii(t.attr_);
	ms << '>';
	return ms;
}


HtmlStream & operator<<(HtmlStream & ms, ETag const & t)
{
	ms.os() << "</" << from_ascii(t.tag_) << '>';
	return ms;
}


HtmlStream & operator<<(HtmlStream & ms, docstring const & s)
{
	ms.os() << s;
	return ms;
}


//////////////////////////////////////////////////////////////////////


SetMode::SetMode(MathMLStream & ms, bool text)
	: ms_(ms)
{
	old_text_level_ = ms_.text_level_;
	ms_.text_level_ = text ? ms_.nesting_level_ : MathMLStream::nlevel;
}


SetMode::~SetMode()
{
	ms_.beforeTag();
	ms_.text_level_ = old_text_level_;
}


//////////////////////////////////////////////////////////////////////


SetHTMLMode::SetHTMLMode(HtmlStream & os, bool text)
	: os_(os)
{
	was_text_ = os_.inText();
	os_.setTextMode(text);
}


SetHTMLMode::~SetHTMLMode()
{
	os_.setTextMode(was_text_);
}


//////////////////////////////////////////////////////////////////////


MapleStream & operator<<(MapleStream & ms, MathAtom const & at)
{
	at->maple(ms);
	return ms;
}


MapleStream & operator<<(MapleStream & ms, MathData const & md)
{
	maple(md, ms);
	return ms;
}


MapleStream & operator<<(MapleStream & ms, char const * s)
{
	ms.os() << s;
	return ms;
}


MapleStream & operator<<(MapleStream & ms, char c)
{
	ms.os() << c;
	return ms;
}


MapleStream & operator<<(MapleStream & ms, int i)
{
	ms.os() << i;
	return ms;
}


MapleStream & operator<<(MapleStream & ms, char_type c)
{
	ms.os().put(c);
	return ms;
}


MapleStream & operator<<(MapleStream & ms, docstring const & s)
{
	ms.os() << s;
	return ms;
}


//////////////////////////////////////////////////////////////////////


MaximaStream & operator<<(MaximaStream & ms, MathAtom const & at)
{
	at->maxima(ms);
	return ms;
}


MaximaStream & operator<<(MaximaStream & ms, MathData const & md)
{
	maxima(md, ms);
	return ms;
}


MaximaStream & operator<<(MaximaStream & ms, char const * s)
{
	ms.os() << s;
	return ms;
}


MaximaStream & operator<<(MaximaStream & ms, char c)
{
	ms.os() << c;
	return ms;
}


MaximaStream & operator<<(MaximaStream & ms, int i)
{
	ms.os() << i;
	return ms;
}


MaximaStream & operator<<(MaximaStream & ms, docstring const & s)
{
	ms.os() << s;
	return ms;
}


MaximaStream & operator<<(MaximaStream & ms, char_type c)
{
	ms.os().put(c);
	return ms;
}


//////////////////////////////////////////////////////////////////////


MathematicaStream & operator<<(MathematicaStream & ms, MathAtom const & at)
{
	at->mathematica(ms);
	return ms;
}


MathematicaStream & operator<<(MathematicaStream & ms, MathData const & md)
{
	mathematica(md, ms);
	return ms;
}


MathematicaStream & operator<<(MathematicaStream & ms, char const * s)
{
	ms.os() << s;
	return ms;
}


MathematicaStream & operator<<(MathematicaStream & ms, char c)
{
	ms.os() << c;
	return ms;
}


MathematicaStream & operator<<(MathematicaStream & ms, int i)
{
	ms.os() << i;
	return ms;
}


MathematicaStream & operator<<(MathematicaStream & ms, docstring const & s)
{
	ms.os() << s;
	return ms;
}


MathematicaStream & operator<<(MathematicaStream & ms, char_type c)
{
	ms.os().put(c);
	return ms;
}


//////////////////////////////////////////////////////////////////////


OctaveStream & operator<<(OctaveStream & ns, MathAtom const & at)
{
	at->octave(ns);
	return ns;
}


OctaveStream & operator<<(OctaveStream & ns, MathData const & md)
{
	octave(md, ns);
	return ns;
}


OctaveStream & operator<<(OctaveStream & ns, char const * s)
{
	ns.os() << s;
	return ns;
}


OctaveStream & operator<<(OctaveStream & ns, char c)
{
	ns.os() << c;
	return ns;
}


OctaveStream & operator<<(OctaveStream & ns, int i)
{
	ns.os() << i;
	return ns;
}


OctaveStream & operator<<(OctaveStream & ns, docstring const & s)
{
	ns.os() << s;
	return ns;
}


OctaveStream & operator<<(OctaveStream & ns, char_type c)
{
	ns.os().put(c);
	return ns;
}


OctaveStream & operator<<(OctaveStream & os, string const & s)
{
	os.os() << from_utf8(s);
	return os;
}


docstring convertDelimToXMLEscape(docstring const & name)
{
	if (name.size() == 1) {
		char_type const c = name[0];
		if (c == '<')
			return from_ascii("&lt;");
		else if (c == '>')
			return from_ascii("&gt;");
		else
			return name;
	} else if (name.size() == 2 && name[0] == '\\') {
		char_type const c = name[1];
		if (c == '{')
			return from_ascii("&#123;");
		else if (c == '}')
			return from_ascii("&#125;");
	}

	MathWordList const & words = mathedWordList();
	MathWordList::const_iterator it = words.find(name);
	if (it != words.end())
		return it->second.xmlname;

	LYXERR0("Unable to find `" << name <<"' in the mathWordList.");
	return name;
}

} // namespace lyx
