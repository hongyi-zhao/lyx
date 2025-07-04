// -*- C++ -*-
/**
 * \file InsetMathNumber.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef MATH_NUMBERINSET_H
#define MATH_NUMBERINSET_H

#include "InsetMath.h"

#include "support/docstring.h"


namespace lyx {

/** Some inset that "is" a number mainly for math-extern
 */
class InsetMathNumber : public InsetMath {
public:
	///
	explicit InsetMathNumber(Buffer * buf, docstring const & s);
	///
	void metrics(MetricsInfo & mi, Dimension & dim) const override;
	///
	void draw(PainterInfo &, int x, int y) const override;
	///
	docstring const & str() const { return str_; }
	///
	InsetMathNumber * asNumberInset() { return this; }

	///
	void normalize(NormalStream &) const override;
	///
	void octave(OctaveStream &) const override;
	///
	void maple(MapleStream &) const override;
	///
	void mathematica(MathematicaStream &) const override;
	///
	void mathmlize(MathMLStream &) const override;
	///
	void htmlize(HtmlStream &) const override;
	///
	void write(TeXMathStream & os) const override;
	///
	InsetCode lyxCode() const override { return MATH_NUMBER_CODE; }

private:
	Inset * clone() const override;
	/// the number as string
	docstring str_;
};


} // namespace lyx

#endif
