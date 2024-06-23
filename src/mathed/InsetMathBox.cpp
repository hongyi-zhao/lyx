/**
 * \file InsetMathBox.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 * \author Ling Li (InsetMathMakebox)
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathBox.h"

#include "LaTeXFeatures.h"
#include "MathData.h"
#include "MathStream.h"
#include "MathSupport.h"
#include "MetricsInfo.h"

#include "support/gettext.h"
#include "support/lstrings.h"

#include "frontends/Painter.h"

#include <algorithm>
#include <iostream>
#include <ostream>

#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>
#include "support/qstring_helpers.h"

using namespace lyx::support;

namespace lyx {

/////////////////////////////////////////////////////////////////////
//
// InsetMathBox
//
/////////////////////////////////////////////////////////////////////

InsetMathBox::InsetMathBox(Buffer * buf, docstring const & name)
	: InsetMathNest(buf, 1), name_(name)
{}


void InsetMathBox::write(TeXMathStream & os) const
{
	ModeSpecifier specifier(os, TEXT_MODE);
	os << '\\' << name_ << '{' << cell(0) << '}';
}


void InsetMathBox::normalize(NormalStream & os) const
{
	os << '[' << name_ << ' ';
	//text_->write(buffer(), os);
	os << "] ";
}


namespace {
void splitAndWrapInMText(MathMLStream & ms, MathData const & cell,
						 const std::string & attributes)
{
        // The goal of this function is to take an XML fragment and wrap
        // anything that is outside of any tag in <mtext></mtext> tags,
        // then wrap the whole thing in an <mrow></mrow> tag with attributes

	// First, generate the inset into a string of its own.
	docstring inset_contents;
	{
		odocstringstream ostmp;
		MathMLStream mstmp(ostmp, ms.xmlns());

		SetMode textmode(mstmp, true);
		mstmp << cell;

		inset_contents = ostmp.str();
	}

        // We use the QT XML library. It's easier if we deal with an XML "document"
        // rather than "fragment", so we wrap it in a fake root node (which we will
        // remove at the end).
        docstring inset_contents_xml = "<root>" + inset_contents + "</root>";

        // Parse the XML into a DOM
        QDomDocument xml;
        xml.setContent(toqstr(inset_contents_xml));

        QDomElement docElem = xml.documentElement();

        // Iterate through the children of our fake root element.
        QDomNode n = docElem.firstChild();
        while (!n.isNull()) {
			// try to convert the child into a text element
			// (i.e. some text that is outside of an XML tag)
            QDomText text = n.toText();
            if (!text.isNull()) {
				// if the result is not null, then the child was indeed
				// bare text, so we need to wrap it in mtext tags
				// make an mtext element
                QDomElement wrapper = xml.createElement(toqstr(ms.namespacedTag("mtext")));
				// put the mtext element in the document right before the text
                docElem.insertBefore(wrapper,n);
                // move the text node inside the mtext element (this has the side
                // effect of removing it from where it was as a child of the root)
				wrapper.appendChild(n);
                n = wrapper.nextSibling();
            } else {
				// if the text is null, then we have something besides a text
				// element (i.e. a tag), so we don't need to do anything and just
				// move onto the next child
                n = n.nextSibling();
            }
        }

        ms << MTag("mrow", attributes);
        docstring interior = qstring_to_ucs4(xml.toString(-1));
        ms << interior.substr(6,interior.length()-13); // chop off the initial <root> and final </root> tags
        ms << ETag("mrow");
}
}


void InsetMathBox::mathmlize(MathMLStream & ms) const
{
	// FIXME XHTML
	// Need to do something special for tags here.
	// Probably will have to involve deferring them, which
	// means returning something from this routine.
	splitAndWrapInMText(ms, cell(0), "class='mathbox'");
}


void InsetMathBox::htmlize(HtmlStream & ms) const
{
	SetHTMLMode textmode(ms, true);
	ms << MTag("span", "class='mathbox'")
	   << cell(0)
	   << ETag("span");
}


void InsetMathBox::metrics(MetricsInfo & mi, Dimension & dim) const
{
	Changer dummy = mi.base.changeFontSet("text");
	cell(0).metrics(mi, dim);
}


void InsetMathBox::draw(PainterInfo & pi, int x, int y) const
{
	Changer dummy = pi.base.changeFontSet("text");
	cell(0).draw(pi, x, y);
}


void InsetMathBox::infoize(odocstream & os) const
{
	os << bformat(_("Box: %1$s"), name_);
}


void InsetMathBox::validate(LaTeXFeatures & features) const
{
	// FIXME XHTML
	// It'd be better to be able to get this from an InsetLayout, but at present
	// InsetLayouts do not seem really to work for things that aren't InsetTexts.
	if (features.runparams().math_flavor == OutputParams::MathAsMathML)
		features.addCSSSnippet("mtext.mathbox { font-style: normal; }");
	else if (features.runparams().math_flavor == OutputParams::MathAsHTML)
		features.addCSSSnippet("span.mathbox { font-style: normal; }");

	if (name_ == "tag" || name_ == "tag*")
		features.require("amsmath");

	InsetMathNest::validate(features);
}



/////////////////////////////////////////////////////////////////////
//
// InsetMathFBox
//
/////////////////////////////////////////////////////////////////////


InsetMathFBox::InsetMathFBox(Buffer * buf)
	: InsetMathNest(buf, 1)
{}


void InsetMathFBox::metrics(MetricsInfo & mi, Dimension & dim) const
{
	Changer dummy = mi.base.changeFontSet("text");
	cell(0).metrics(mi, dim);
	// 1 pixel space, 1 frame, 1 space
	dim.wid += 2 * 3;
	dim.asc += 3;
	dim.des += 3;
}


void InsetMathFBox::draw(PainterInfo & pi, int x, int y) const
{
	Dimension const dim = dimension(*pi.base.bv);
	pi.pain.rectangle(x + 1, y - dim.ascent() + 1,
		dim.width() - 2, dim.height() - 2, Color_foreground);
	Changer dummy = pi.base.changeFontSet("text");
	cell(0).draw(pi, x + 3, y);
}


void InsetMathFBox::write(TeXMathStream & os) const
{
	ModeSpecifier specifier(os, TEXT_MODE);
	os << "\\fbox{" << cell(0) << '}';
}


void InsetMathFBox::normalize(NormalStream & os) const
{
	os << "[fbox " << cell(0) << ']';
}


void InsetMathFBox::mathmlize(MathMLStream & ms) const
{
	splitAndWrapInMText(ms, cell(0), "class='fbox'");
}


void InsetMathFBox::htmlize(HtmlStream & ms) const
{
	SetHTMLMode textmode(ms, true);
	ms << MTag("span", "class='fbox'")
	   << cell(0)
	   << ETag("span");
}


void InsetMathFBox::infoize(odocstream & os) const
{
	os << "FBox: ";
}


void InsetMathFBox::validate(LaTeXFeatures & features) const
{
	// FIXME XHTML
	// It'd be better to be able to get this from an InsetLayout, but at present
	// InsetLayouts do not seem really to work for things that aren't InsetTexts.
	if (features.runparams().math_flavor == OutputParams::MathAsMathML)
		features.addCSSSnippet(
			"mtext.fbox { border: 1px solid black; font-style: normal; padding: 0.5ex; }");
	else if (features.runparams().math_flavor == OutputParams::MathAsHTML)
		features.addCSSSnippet(
			"span.fbox { border: 1px solid black; font-style: normal; padding: 0.5ex; }");

	cell(0).validate(features);
	InsetMathNest::validate(features);
}



/////////////////////////////////////////////////////////////////////
//
// InsetMathMakebox
//
/////////////////////////////////////////////////////////////////////


InsetMathMakebox::InsetMathMakebox(Buffer * buf, bool framebox)
	: InsetMathNest(buf, 3), framebox_(framebox)
{}


void InsetMathMakebox::metrics(MetricsInfo & mi, Dimension & dim) const
{
	Changer dummy = mi.base.changeFontSet("text");

	Dimension wdim;
	static docstring bracket = from_ascii("[");
	metricsStrRedBlack(mi, wdim, bracket);
	int w = wdim.wid;

	Dimension dim0;
	Dimension dim1;
	Dimension dim2;
	cell(0).metrics(mi, dim0);
	cell(1).metrics(mi, dim1);
	cell(2).metrics(mi, dim2);

	dim.wid = w + dim0.wid + w + w + dim1.wid + w + 2 + dim2.wid;
	dim.asc = std::max(std::max(wdim.asc, dim0.asc), std::max(dim1.asc, dim2.asc));
	dim.des = std::max(std::max(wdim.des, dim0.des), std::max(dim1.des, dim2.des));

	if (framebox_) {
		dim.wid += 4;
		dim.asc += 3;
		dim.des += 2;
	} else {
		dim.asc += 1;
		dim.des += 1;
	}
}


void InsetMathMakebox::draw(PainterInfo & pi, int x, int y) const
{
	Changer dummy = pi.base.changeFontSet("text");
	BufferView const & bv = *pi.base.bv;
	int w = mathed_char_width(pi.base.font, '[');

	if (framebox_) {
		Dimension const dim = dimension(*pi.base.bv);
		pi.pain.rectangle(x + 1, y - dim.ascent() + 1,
				  dim.width() - 2, dim.height() - 2, Color_foreground);
		x += 2;
	}

	drawStrBlack(pi, x, y, from_ascii("["));
	x += w;
	cell(0).draw(pi, x, y);
	x += cell(0).dimension(bv).wid;
	drawStrBlack(pi, x, y, from_ascii("]"));
	x += w;

	drawStrBlack(pi, x, y, from_ascii("["));
	x += w;
	cell(1).draw(pi, x, y);
	x += cell(1).dimension(bv).wid;
	drawStrBlack(pi, x, y, from_ascii("]"));
	x += w + 2;

	cell(2).draw(pi, x, y);
}


void InsetMathMakebox::write(TeXMathStream & os) const
{
	ModeSpecifier specifier(os, TEXT_MODE);
	os << (framebox_ ? "\\framebox" : "\\makebox");
	if (!cell(0).empty() || !os.latex()) {
		os << '[' << cell(0) << ']';
		if (!cell(1).empty() || !os.latex())
			os << '[' << cell(1) << ']';
	}
	os << '{' << cell(2) << '}';
}


void InsetMathMakebox::normalize(NormalStream & os) const
{
	os << (framebox_ ? "[framebox " : "[makebox ")
	   << cell(0) << ' ' << cell(1) << ' ' << cell(2) << ']';
}


void InsetMathMakebox::infoize(odocstream & os) const
{
	os << (framebox_ ? "Framebox" : "Makebox")
	   << " (width: " << cell(0)
	   << " pos: " << cell(1) << ")";
}


void InsetMathMakebox::mathmlize(MathMLStream & ms) const
{
	// FIXME We could do something with the other arguments.
	std::string const cssclass = framebox_ ? "framebox" : "makebox";
	splitAndWrapInMText(ms, cell(2), "class='" + cssclass + "'");
}


void InsetMathMakebox::htmlize(HtmlStream & ms) const
{
	// FIXME We could do something with the other arguments.
	SetHTMLMode textmode(ms, true);
	std::string const cssclass = framebox_ ? "framebox" : "makebox";
	ms << MTag("span", "class='" + cssclass + "'")
	   << cell(2)
	   << ETag("span");
}


void InsetMathMakebox::validate(LaTeXFeatures & features) const
{
	// FIXME XHTML
	// It'd be better to be able to get this from an InsetLayout, but at present
	// InsetLayouts do not seem really to work for things that aren't InsetTexts.
	if (features.runparams().math_flavor == OutputParams::MathAsMathML)
		features.addCSSSnippet("mtext.framebox { border: 1px solid black; }");
	else if (features.runparams().math_flavor == OutputParams::MathAsHTML)
		features.addCSSSnippet("span.framebox { border: 1px solid black; }");
	InsetMathNest::validate(features);
}


/////////////////////////////////////////////////////////////////////
//
// InsetMathBoxed
//
/////////////////////////////////////////////////////////////////////

InsetMathBoxed::InsetMathBoxed(Buffer * buf)
	: InsetMathNest(buf, 1)
{}


void InsetMathBoxed::metrics(MetricsInfo & mi, Dimension & dim) const
{
	cell(0).metrics(mi, dim);
	// 1 pixel space, 1 frame, 1 space
	dim.wid += 2 * 3;
	dim.asc += 3;
	dim.des += 3;
}


void InsetMathBoxed::draw(PainterInfo & pi, int x, int y) const
{
	Dimension const dim = dimension(*pi.base.bv);
	pi.pain.rectangle(x + 1, y - dim.ascent() + 1,
		dim.width() - 2, dim.height() - 2, Color_foreground);
	cell(0).draw(pi, x + 3, y);
}


void InsetMathBoxed::write(TeXMathStream & os) const
{
	ModeSpecifier specifier(os, MATH_MODE);
	os << "\\boxed{" << cell(0) << '}';
}


void InsetMathBoxed::normalize(NormalStream & os) const
{
	os << "[boxed " << cell(0) << ']';
}


void InsetMathBoxed::infoize(odocstream & os) const
{
	os << "Boxed: ";
}


void InsetMathBoxed::mathmlize(MathMLStream & ms) const
{
	splitAndWrapInMText(ms, cell(0), "class='boxed'");
}


void InsetMathBoxed::htmlize(HtmlStream & ms) const
{
	ms << MTag("span", "class='boxed'")
	   << cell(0)
	   << ETag("span");
}


void InsetMathBoxed::validate(LaTeXFeatures & features) const
{
	features.require("amsmath");

	// FIXME XHTML
	// It'd be better to be able to get this from an InsetLayout, but at present
	// InsetLayouts do not seem really to work for things that aren't InsetTexts.
	if (features.runparams().math_flavor == OutputParams::MathAsMathML)
		features.addCSSSnippet("mtext.boxed { border: 1px solid black; }");
	else if (features.runparams().math_flavor == OutputParams::MathAsHTML)
		features.addCSSSnippet("span.boxed { border: 1px solid black; }");

	InsetMathNest::validate(features);
}


} // namespace lyx
