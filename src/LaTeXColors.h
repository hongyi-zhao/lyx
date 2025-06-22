// -*- C++ -*-
/**
 * \file LaTeXColors.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef LATEXCOLORS_H
#define LATEXCOLORS_H

#include "support/docstring.h"

#include <map>
#include <vector>


namespace lyx {

namespace support { class Lexer; }

/// LaTeX Font definition
class LaTeXColor {
public:
	/// LaTeX color
	LaTeXColor() : svgclash_(false) {}
	/// The color name
	std::string const & name() const { return name_; }
	/// The name to appear in the dialog
	docstring const & guiname() const { return guiname_; }
	/// LaTeX name
	std::string const & latex() const { return latex_; }
	/// Hex value
	std::string const hexname() const { return "#" + hexname_; }
	/// The (xcolor) model providing this color
	std::string const & model() const { return model_; }
	/// The color category
	docstring const & category() const { return category_; }
	/// CMYK value
	std::string const & cmyk() const { return cmyk_; }
	/// Required packages
	std::vector<std::string> const & req() const { return requires_; }
	/// Is this a dvipsfont name that clashes with SVG name?
	bool svgclash() const { return svgclash_; }
	///
	bool read(support::Lexer & lex);
	///
	bool readColor(support::Lexer & lex);
private:
	///
	std::string name_;
	///
	docstring guiname_;
	///
	std::string latex_;
	///
	std::string hexname_;
	///
	std::string model_;
	///
	docstring category_;
	///
	std::string cmyk_;
	///
	std::vector<std::string> requires_;
	///
	bool svgclash_;
};


/** The list of available LaTeX fonts
 */
class LaTeXColors {
public:
	///
	typedef std::vector<std::pair<std::string, LaTeXColor>> TexColorMap;
	/// Get all LaTeXColors
	TexColorMap getLaTeXColors();
	/// Get a specific LaTeXColor \p name
	LaTeXColor getLaTeXColor(std::string const & name);
	///
	bool isLaTeXColor(std::string const & lyxname);
	///
	bool isRealLaTeXColor(std::string const & latexname);
	///
	std::string getFromLaTeXColor(std::string const & latexname);
	///
	std::map<std::string, std::string> getSVGClashColors();
private:
	///
	void readLaTeXColors();
	///
	TexColorMap texcolormap_;
};

/// Implementation is in LyX.cpp
extern LaTeXColors & theLaTeXColors();


} // namespace lyx

#endif
