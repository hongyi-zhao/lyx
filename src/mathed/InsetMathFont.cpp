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

#include "Dimension.h"
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
	MathFontInfo old_font = os.fontInfo().mergeWith(MathFontInfo::fromMacro(key_->name));
	const std::string span_class = os.fontInfo().toHTMLSpanClass();

	// TODO: with this implementation, variants are output several times. See mathmlize.
	if (!span_class.empty()) {
		os << MTag("span", "class='" + span_class + "'")
		   << cell(0)
		   << ETag("span");
	} else
		os << cell(0);

	os.fontInfo().replaceBy(old_font);
}


// The fonts we want to support are listed in lib/symbols
void InsetMathFont::mathmlize(MathMLStream & ms) const
{
	MathFontInfo old_font = ms.fontInfo().mergeWith(MathFontInfo::fromMacro(key_->name));
	const std::string variant = ms.fontInfo().toMathMLMathVariant(ms.version());

	// TODO: with this implementation, variants are output several times (e.g.,
	// for \textit{a\textbf{b}}, italics will be present twice in the output,
	// <mstyle mathvariant="italic">a<mstyle mathvariant="italic bold">b</mstyle></mstyle>
	// To do better, we'd need more logic for (un)toggling, like in the code for text
	// (for instance, computeDocBookFontSwitch).
	if (ms.fontInfo().shape() == MathFontInfo::MATH_UP_SHAPE) {
		SetMode textmode(ms, true);
		ms << cell(0);
	} else if (!variant.empty()) {
		ms << MTag("mstyle", "mathvariant='" + variant + "'");
		ms << cell(0);
		ms << ETag("mstyle");
	} else {
		ms << cell(0);
	}

	ms.fontInfo().replaceBy(old_font);
}


void InsetMathFont::infoize(odocstream & os) const
{
	os << bformat(_("Font: %1$s"), key_->name);
}


} // namespace lyx
