/**
 * \file InsetMathIntertext.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathIntertext.h"

#include "Buffer.h"
#include "BufferView.h"
#include "Dimension.h"
#include "InsetMathChar.h"
#include "LaTeXFeatures.h"
#include "MathData.h"
#include "MathFactory.h"
#include "MathStream.h"
#include "MathSupport.h"
#include "MetricsInfo.h"
#include "support/docstream.h"
#include "support/lstrings.h"

using lyx::support::escape;


namespace lyx {

InsetMathIntertext::InsetMathIntertext(Buffer * buf, docstring const & name)
	: InsetMathNest(buf, 1), name_(name)
{}


void InsetMathIntertext::write(TeXMathStream & os) const
{
	ModeSpecifier specifier(os, TEXT_MODE);
	os << '\\' << name_ << '{' << cell(0) << '}';
}


Inset * InsetMathIntertext::clone() const
{
	return new InsetMathIntertext(*this);
}


void InsetMathIntertext::metrics(MetricsInfo & mi, Dimension & dim) const
{
	Changer dummy = mi.base.changeFontSet("text");
	cell(0).metrics(mi, dim);
	if (name_ == "shortintertext")
		dim.asc += 2 * dim.asc + dim.des + spacer_;
	else
		dim.asc += 3 * dim.asc + 2 * (dim.des + spacer_);
}


void InsetMathIntertext::draw(PainterInfo & pi, int x, int y) const
{
	Changer dummy = pi.base.changeFontSet("text");
	Dimension const dim = dimension(*pi.base.bv);
	int text_y;
	// note that dim.asc is already updated in metrics()
	if (name_ == "shortintertext")
		text_y = y - (dim.asc + dim.des + spacer_)/2;
	else
		text_y = y - (dim.asc - dim.des + spacer_)/2;
	cell(0).draw(pi, x, text_y);
}


docstring InsetMathIntertext::name() const
{
	return name_;
}


void InsetMathIntertext::mathmlize(MathMLStream & ms) const
{
	ms << MTagInline("mtext", "style=\"position:absolute;left:0px\"");
	for (size_type i=0; i<cell(0).size(); ++i)
		ms << cell(0)[i]->asCharInset()->getChar();
	ms << ETagInline("mtext");
	// make a new line
	ms << ETag("mtd") << ETag("mtr")
	   << MTag("mtr", "style=\"height:3ex\"") << ETag("mtr")
	   << MTag("mtr") << MTag("mtd");
}


void InsetMathIntertext::infoize(odocstream & os) const
{
	os << "Intertext ";
}


void InsetMathIntertext::validate(LaTeXFeatures & features) const
{
	if (name_ == "shortintertext" && features.runparams().isLaTeX())
		features.require("mathtools");
	InsetMathNest::validate(features);
}

} // namespace lyx
