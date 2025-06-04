// -*- C++ -*-
/**
 * \file InsetMathIntertext.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef MATH_INTERTEXTINSET_H
#define MATH_INTERTEXTINSET_H

#include "InsetMathNest.h"

#include "support/docstring.h"


namespace lyx {


/// Support for LaTeX's \\intertext command

class InsetMathIntertext : public InsetMathNest {
public:
	///
	explicit InsetMathIntertext(Buffer * buf, docstring const & name);
	///
	mode_type currentMode() const override { return TEXT_MODE; }
	///
	docstring name() const override;
	///
	void metrics(MetricsInfo & mi, Dimension & dim) const override;
	///
	void draw(PainterInfo & pi, int x, int y) const override;
	///
	void write(TeXMathStream & os) const override;
	///
	void mathmlize(MathMLStream & ms) const override;
	///
	void infoize(odocstream & os) const override;
	///
	InsetCode lyxCode() const override { return MATH_INTERTEXT_CODE; }
	///
	void validate(LaTeXFeatures &) const override;

private:
	Inset * clone() const override;
	/// the string
	docstring name_;
	int spacer_ = 10;
};


} // namespace lyx
#endif
