/**
 * \file InsetMathBrace.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathBrace.h"

#include "Dimension.h"
#include "MathData.h"
#include "MathStream.h"
#include "MathSupport.h"
#include "MetricsInfo.h"

#include "frontends/FontMetrics.h"
#include "frontends/Painter.h"

#include <ostream>
#include <algorithm>

using namespace std;

namespace lyx {

InsetMathBrace::InsetMathBrace(Buffer * buf)
	: InsetMathNest(buf, 1), current_mode_(UNDECIDED_MODE)
{}


InsetMathBrace::InsetMathBrace(Buffer * buf, MathData const & ar)
	: InsetMathNest(buf, 1),
	  current_mode_(UNDECIDED_MODE)
{
	cell(0) = ar;
}


Inset * InsetMathBrace::clone() const
{
	return new InsetMathBrace(*this);
}


void InsetMathBrace::metrics(MetricsInfo & mi, Dimension & dim) const
{
	current_mode_ = isTextFont(mi.base.fontname) ? TEXT_MODE : MATH_MODE;
	Dimension dim0;
	cell(0).metrics(mi, dim0);
	FontInfo font = mi.base.font;
	augmentFont(font, current_mode_ == MATH_MODE ? "mathnormal" : "text");
	Dimension t = theFontMetrics(font).dimension('{');
	dim.asc = max(dim0.asc, t.asc);
	dim.des = max(dim0.des, t.des);
	dim.wid = dim0.width() + 2 * t.wid;
}


void InsetMathBrace::draw(PainterInfo & pi, int x, int y) const
{
	current_mode_ = isTextFont(pi.base.fontname) ? TEXT_MODE : MATH_MODE;
	FontInfo font = pi.base.font;
	augmentFont(font, current_mode_ == MATH_MODE ? "mathnormal" : "text");
	font.setShape(UP_SHAPE);
	font.setColor(Color_latex);
	Dimension t = theFontMetrics(font).dimension('{');
	pi.pain.text(x, y, '{', font);
	cell(0).draw(pi, x + t.wid, y);
	Dimension const & dim0 = cell(0).dimension(*pi.base.bv);
	pi.pain.text(x + t.wid + dim0.width(), y, '}', font);
}


void InsetMathBrace::write(TeXMathStream & os) const
{
	os << '{' << cell(0) << '}';
}


void InsetMathBrace::normalize(NormalStream & os) const
{
	os << "[block " << cell(0) << ']';
}


void InsetMathBrace::maple(MapleStream & os) const
{
	os << cell(0);
}


void InsetMathBrace::octave(OctaveStream & os) const
{
	os << cell(0);
}


void InsetMathBrace::mathmlize(MathMLStream & ms) const
{
	ms << MTag("mrow")
       << cell(0)
       << ETag("mrow");
}


void InsetMathBrace::htmlize(HtmlStream & os) const
{
	os << cell(0);
}


void InsetMathBrace::mathematica(MathematicaStream & os) const
{
	os << cell(0);
}


void InsetMathBrace::infoize(odocstream & os) const
{
	os << "Nested Block: ";
}


} // namespace lyx
