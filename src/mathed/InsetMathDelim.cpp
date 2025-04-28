/**
 * \file InsetMathDelim.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alejandro Aguilar Sierra
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathDelim.h"

#include "MathData.h"
#include "MathFactory.h"
#include "MathStream.h"
#include "MathSupport.h"
#include "MetricsInfo.h"

#include "Dimension.h"
#include "LaTeXFeatures.h"

#include "support/docstring.h"

#include "frontends/FontMetrics.h"

#include <algorithm>

using namespace std;

namespace lyx {

static docstring convertDelimToLatexName(docstring const & name)
{
	if (name.size() == 1) {
		char_type const c = name[0];
		if (c == '<' || c == '(' || c == '[' || c == '.'
		    || c == '>' || c == ')' || c == ']' || c == '/' || c == '|')
			return name;
	}
	return '\\' + name + ' ';
}


InsetMathDelim::InsetMathDelim(Buffer * buf, docstring const & l,
		docstring const & r)
	: InsetMathNest(buf, 1), left_(l), right_(r), dw_(0), is_extracted_(false)
{}


InsetMathDelim::InsetMathDelim(Buffer * buf, docstring const & l, docstring const & r,
	MathData const & md)
	: InsetMathNest(buf, 1), left_(l), right_(r), dw_(0), is_extracted_(false)
{
	cell(0) = md;
}


InsetMathDelim::InsetMathDelim(Buffer * buf, docstring const & l, docstring const & r,
                               MathData const & md, bool const is_extracted)
		: InsetMathNest(buf, 1), left_(l), right_(r), dw_(0), is_extracted_(is_extracted)
{
	cell(0) = md;
}


Inset * InsetMathDelim::clone() const
{
	return new InsetMathDelim(*this);
}


void InsetMathDelim::validate(LaTeXFeatures & features) const
{
	InsetMathNest::validate(features);
	// The delimiters may be used without \left or \right as well.
	// Therefore they are listed in lib/symbols, and if they have
	// requirements, we need to add them here.
	validate_math_word(features, left_);
	validate_math_word(features, right_);
}


void InsetMathDelim::write(TeXMathStream & os) const
{
	MathEnsurer ensurer(os);
	os << "\\left" << convertDelimToLatexName(left_) << cell(0)
	   << "\\right" << convertDelimToLatexName(right_);
}


void InsetMathDelim::normalize(NormalStream & os) const
{
	os << "[delim " << convertDelimToLatexName(left_) << ' '
	   << convertDelimToLatexName(right_) << ' ' << cell(0) << ']';
}


void InsetMathDelim::metrics(MetricsInfo & mi, Dimension & dim) const
{
	Changer dummy = mi.base.changeEnsureMath();
	Dimension dim0;
	cell(0).metrics(mi, dim0, false);
	Dimension t = theFontMetrics(mi.base.font).dimension('I');
	int h0 = (t.asc + t.des) / 2;
	int a0 = max(dim0.asc, t.asc)   - h0;
	int d0 = max(dim0.des, t.des)  + h0;
	dw_ = dim0.height() / 5;
	if (dw_ > 8)
		dw_ = 8;
	if (dw_ < 4)
		dw_ = 4;
	dim.wid = dim0.width() + 2 * dw_ + 2 * mathed_thinmuskip(mi.base.font);
	dim.asc = max(a0, d0) + h0;
	dim.des = max(a0, d0) - h0;
}


void InsetMathDelim::draw(PainterInfo & pi, int x, int y) const
{
	Changer dummy = pi.base.changeEnsureMath();
	Dimension const dim = dimension(*pi.base.bv);
	int const b = y - dim.asc;
	int const skip = mathed_thinmuskip(pi.base.font);
	cell(0).draw(pi, x + dw_ + skip, y);
	mathed_draw_deco(pi, x + skip / 2, b, dw_, dim.height(), left_);
	mathed_draw_deco(pi, x + dim.width() - dw_ - skip / 2,
		b, dw_, dim.height(), right_);
}


bool InsetMathDelim::isParenthesis() const
{
	return left_ == "(" && right_ == ")";
}


bool InsetMathDelim::isBrackets() const
{
	return left_ == "[" && right_ == "]";
}


bool InsetMathDelim::isAbs() const
{
	return left_ == "|" && right_ == "|";
}


void InsetMathDelim::maple(MapleStream & os) const
{
	if (isAbs()) {
		if (cell(0).size() == 1 && cell(0).front()->asMatrixInset())
			os << "linalg[det](" << cell(0) << ')';
		else
			os << "abs(" << cell(0) << ')';
	}
	else
		os << left_ << cell(0) << right_;
}


void InsetMathDelim::maxima(MaximaStream & os) const
{
	if (isAbs()) {
		if (cell(0).size() == 1 && cell(0).front()->asMatrixInset())
			os << "determinant(" << cell(0) << ')';
		else
			os << "abs(" << cell(0) << ')';
	}
	else
		os << left_ << cell(0) << right_;
}


void InsetMathDelim::mathematica(MathematicaStream & os) const
{
	if (isAbs()) {
		if (cell(0).size() == 1 && cell(0).front()->asMatrixInset())
			os << "Det[" << cell(0) << ']';
		else
			os << "Abs[" << cell(0) << ']';
	}
	else
		os << left_ << cell(0) << right_;
}


void InsetMathDelim::mathmlize(MathMLStream & ms) const
{
	// Ignore the delimiter if: it is empty or only a space (one character).
	const std::string attr = is_extracted_ ? "stretchy='false'" : "";
	if (!left_.empty() && ((left_.size() == 1 && left_[0] != ' ') || left_.size() > 1)) {
	    ms << MTag("mrow")
	       << MTagInline("mo", attr)
	       << convertDelimToXMLEscape(left_)
	       << ETagInline("mo");
	}
	ms << cell(0);
	if (!right_.empty() && ((right_.size() == 1 && right_[0] != ' ') || right_.size() > 1)) {
	    ms << MTagInline("mo", attr)
	       << convertDelimToXMLEscape(right_)
	       << ETagInline("mo")
	       << ETag("mrow");
	}
}


void InsetMathDelim::htmlize(HtmlStream & os) const
{
	os << convertDelimToXMLEscape(left_)
	   << cell(0)
	   << convertDelimToXMLEscape(right_);
}


void InsetMathDelim::octave(OctaveStream & os) const
{
	if (isAbs())
		os << "det(" << cell(0) << ')';
	else
		os << left_ << cell(0) << right_;
}


} // namespace lyx
