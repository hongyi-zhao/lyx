/**
 * \file InsetMathFont.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathFont.h"

#include "LaTeXFeatures.h"
#include "MathData.h"
#include "MathStream.h"
#include "MathParser.h"
#include "MetricsInfo.h"

#include "support/gettext.h"
#include "support/lassert.h"
#include "support/lstrings.h"

#include <ostream>

using namespace lyx::support;

namespace lyx {

namespace {
// Similar to FontInfo and its related enums, but specifically for the math
// mode.
//
// All types have enumerations, like FontEnums.h, even though there are
// sometimes only two cases: this design ensures some future-proofness and
// ensures that you cannot inadvertently swap two values.
class MathFontInfo {
public:
	enum MathFontFamily {
		MATH_NORMAL_FAMILY = 0, // Default value in MathML.
		MATH_FRAKTUR_FAMILY,
		MATH_SANS_FAMILY,
		MATH_MONOSPACE_FAMILY,
		MATH_DOUBLE_STRUCK_FAMILY,
		MATH_SCRIPT_FAMILY,
		MATH_SMALL_CAPS // Not natively supported in any version of MathML.
	};

	enum MathFontSeries {
		MATH_MEDIUM_SERIES = 0, // Default value in MathML. // Default value in MathML.
		MATH_BOLD_SERIES
	};

	enum MathFontShape {
		MATH_UP_SHAPE = 0,
		MATH_ITALIC_SHAPE // Default value in MathML mi, not outside.
	};

	MathFontInfo() :
		family_(MATH_NORMAL_FAMILY), series_(MATH_MEDIUM_SERIES), shape_(MATH_UP_SHAPE) {}
	MathFontInfo(const MathFontFamily family, const MathFontSeries series, const MathFontShape shape) :
		family_(family), series_(series), shape_(shape) {}

	static MathFontInfo fromMacro(const docstring& tag)
	{
		MathFontInfo font;
		if (tag == "mathnormal" || tag == "mathrm"
				|| tag == "text" || tag == "textnormal"
				|| tag == "textrm" || tag == "textup"
				|| tag == "textmd")
			font.shape_ = MATH_UP_SHAPE;
		else if (tag == "frak" || tag == "mathfrak")
			font.family_ = MATH_FRAKTUR_FAMILY;
		else if (tag == "mathbf" || tag == "textbf")
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

	MathFontFamily family() const { return family_; }
	MathFontSeries series() const { return series_; }
	MathFontShape shape() const { return shape_; }

	std::string toMathMLMathVariant(MathMLStream::MathMLVersion mathml_version) const
	{
		return mathml_version == MathMLStream::MathMLVersion::mathml3 ?
			toMathVariantForMathML3() : toMathVariantForMathMLCore();
	}

	std::string toHTMLSpanClass() const
	{
		std::string span_class;
		switch (family_) {
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
		if (span_class.empty() && (family_ == MATH_NORMAL_FAMILY || family_ == MATH_DOUBLE_STRUCK_FAMILY)) {
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

private:
	MathFontFamily family_;
	MathFontSeries series_;
	MathFontShape shape_;

	std::string toMathVariantForMathML3() const
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
			if (series_ == MATH_MEDIUM_SERIES) {
				return shape_ == MATH_UP_SHAPE ? "sans-serif" : "sans-serif-italic";
			}
			return shape_ == MATH_UP_SHAPE ? "bold-sans-serif" : "sans-serif-bold-italic";
		case MATH_NORMAL_FAMILY:
			if (series_ == MATH_MEDIUM_SERIES) {
				return shape_ == MATH_UP_SHAPE ? "normal" : "italic";
			}
			return shape_ == MATH_UP_SHAPE ? "bold" : "bold-italic";
		case MATH_SMALL_CAPS:
			// No valid value...
			return "";
		}

		// Better safe than sorry.
		LYXERR(Debug::MATHED,
			"Unexpected case in MathFontInfo::toMathVariantForMathML3: family_ = " << family_
				<< ", series = " << series_ << ", shape = " << shape_);
		return "";
	}

	std::string toMathVariantForMathMLCore() const
	{
		return shape_ == MATH_UP_SHAPE ? "normal" : "";
	}
};
}

InsetMathFont::InsetMathFont(Buffer * buf, latexkeys const * key)
	: InsetMathNest(buf, 1), key_(key)
{}


Inset * InsetMathFont::clone() const
{
	return new InsetMathFont(*this);
}


std::string InsetMathFont::font() const
{
	LASSERT(isAscii(key_->name), return "mathnormal");
	return to_ascii(key_->name);
}


InsetMath::mode_type InsetMathFont::currentMode() const
{
	if (key_->extra == "mathmode")
		return MATH_MODE;
	if (key_->extra == "textmode")
		return TEXT_MODE;
	return UNDECIDED_MODE;
}


bool InsetMathFont::lockedMode() const
{
	return key_->extra == "forcetext";
}


void InsetMathFont::write(TeXMathStream & os) const
{
	// Close the mode changing command inserted during export if
	// we are going to output another mode changing command that
	// actually doesn't change mode. This avoids exporting things
	// such as \ensuremath{a\mathit{b}} or \textit{a\text{b}} and
	// produce instead \ensuremath{a}\mathit{b} and \textit{a}\text{b}.
	if (os.pendingBrace()
	    && ((currentMode() == TEXT_MODE && os.textMode())
		    || (currentMode() == MATH_MODE && !os.textMode()))) {
		os.os() << '}';
		os.pendingBrace(false);
		os.textMode(!os.textMode());
	}
	InsetMathNest::write(os);
}


void InsetMathFont::metrics(MetricsInfo & mi, Dimension & dim) const
{
	Changer dummy = mi.base.changeFontSet(font());
	cell(0).metrics(mi, dim);
}


void InsetMathFont::draw(PainterInfo & pi, int x, int y) const
{
	Changer dummy = pi.base.changeFontSet(font());
	cell(0).draw(pi, x, y);
}


void InsetMathFont::metricsT(TextMetricsInfo const & mi, Dimension &) const
{
	// FIXME: BROKEN!
	Dimension dim;
	cell(0).metricsT(mi, dim);
}


void InsetMathFont::drawT(TextPainter & pain, int x, int y) const
{
	cell(0).drawT(pain, x, y);
}


docstring InsetMathFont::name() const
{
	return key_->name;
}


void InsetMathFont::validate(LaTeXFeatures & features) const
{
	InsetMathNest::validate(features);
	std::string fontname = font();
	if (features.runparams().isLaTeX()) {
		// Make sure amssymb is put in preamble if Blackboard Bold or
		// Fraktur used:
		if (fontname == "mathfrak" || fontname == "mathbb")
			features.require("amssymb");
		if (fontname == "text" || fontname == "textnormal"
		    || (fontname.length() == 6 && fontname.substr(0, 4) == "text"))
			features.require("amstext");
		if (fontname == "mathscr" && !features.isRequired("unicode-math"))
			features.require("mathrsfs");
		if (fontname == "textipa")
			features.require("tipa");
		if (fontname == "ce" || fontname == "cf")
			features.require("mhchem");
		if (fontname == "mathds")
			features.require("dsfont");
	} else if (features.runparams().math_flavor == OutputParams::MathAsHTML) {
		features.addCSSSnippet(
			"span.normal{font: normal normal normal inherit serif;}\n"
			"span.bold{font: normal normal bold inherit serif;}\n"
			"span.italic{font: italic normal normal inherit serif;}\n"
			"span.bold-italic{font: italic normal bold inherit serif;}\n"
			"span.fraktur{font: normal normal normal inherit cursive;}\n"
			"span.fraktur-bold{font: normal normal bold inherit cursive;}\n"
			"span.fraktur-italic{font: italic normal normal inherit cursive;}\n"
			"span.fraktur-bold-italic{font: italic normal bold inherit cursive;}\n"
			"span.script{font: normal normal normal inherit cursive;}\n"
			"span.script-bold{font: normal normal bold inherit cursive;}\n"
			"span.script-italic{font: italic normal normal inherit cursive;}\n"
			"span.script-bold-italic{font: italic normal bold inherit cursive;}\n"
			"span.sans{font: normal normal normal inherit sans-serif;}\n"
			"span.sans-bold{font: normal normal normal inherit bold-serif;}\n"
			"span.sans-italic{font: italic normal normal inherit sans-serif;}\n"
			"span.sans-bold-italic{font: italic normal normal inherit bold-serif;}\n"
			"span.monospace{font: normal normal normal inherit monospace;}\n"
			"span.monospace-bold{font: normal normal bold inherit monospace;}\n"
			"span.monospace-italic{font: italic normal normal inherit monospace;}\n"
			"span.monospace-bold-italic{font: italic normal bold inherit monospace;}\n"
			"span.noun{font: normal small-caps normal inherit normal;}\n"
			"span.noun-bold{font: normal small-caps bold inherit normal;}\n"
			"span.noun-italic{font: italic small-caps normal inherit normal;}\n"
			"span.noun-bold-italic{font: italic small-caps bold inherit normal;}");
	}
}


// The fonts we want to support are listed in lib/symbols
void InsetMathFont::htmlize(HtmlStream & os) const
{
	// FIXME These are not quite right, because they do not nest
	// correctly. A proper fix would presumably involve tracking
	// the fonts already in effect.
	const MathFontInfo font = MathFontInfo::fromMacro(key_->name);
	const std::string span_class = font.toHTMLSpanClass();

	if (!span_class.empty()) {
		os << MTag("span", "class='" + span_class + "'")
		   << cell(0)
		   << ETag("span");
	} else
		os << cell(0);
}


// The fonts we want to support are listed in lib/symbols
void InsetMathFont::mathmlize(MathMLStream & ms) const
{
	// FIXME These are not quite right, because they do not nest
	// correctly. A proper fix would presumably involve tracking
	// the fonts already in effect.
	const MathFontInfo font = MathFontInfo::fromMacro(key_->name);
	const std::string variant = font.toMathMLMathVariant(ms.version());

	if (font.shape() == MathFontInfo::MATH_UP_SHAPE) {
		SetMode textmode(ms, true);
		ms << cell(0);
	} else if (!variant.empty()) {
		ms << MTag("mstyle", "mathvariant='" + variant + "'");
		ms << cell(0);
		ms << ETag("mstyle");
	} else {
		ms << cell(0);
	}
}


void InsetMathFont::infoize(odocstream & os) const
{
	os << bformat(_("Font: %1$s"), key_->name);
}


} // namespace lyx
