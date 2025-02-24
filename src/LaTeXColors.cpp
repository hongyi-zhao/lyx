/**
 * \file LaTeXColors.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "LaTeXColors.h"

#include "support/debug.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/Lexer.h"
#include "support/lstrings.h"


using namespace std;
using namespace lyx::support;


namespace lyx {

LaTeXColors latexcolors;


bool LaTeXColor::readColor(Lexer & lex)
{
	enum LaTeXColorTags {
		LC_CATEGORY = 1,
		LC_CMYK,
		LC_COLOR_MODEL,
		LC_END,
		LC_GUINAME,
		LC_HEXNAME,
		LC_LATEXNAME,
		LC_REQUIRES,
		LC_SVG_CLASH
	};

	// Keep these sorted alphabetically!
	LexerKeyword latexColorTags[] = {
		{ "category",             LC_CATEGORY },
		{ "cmyk",                 LC_CMYK },
		{ "colormodel",           LC_COLOR_MODEL },
		{ "endcolor",             LC_END },
		{ "guiname",              LC_GUINAME },
		{ "hexname",              LC_HEXNAME },
		{ "latexname",            LC_LATEXNAME },
		{ "requires",             LC_REQUIRES },
		{ "svgclash",             LC_SVG_CLASH },
	};

	bool error = false;
	bool finished = false;
	lex.pushTable(latexColorTags);
	// parse style section
	while (!finished && lex.isOK() && !error) {
		int le = lex.lex();
		// See comment in LyXRC.cpp.
		switch (le) {
		case Lexer::LEX_FEOF:
			continue;

		case Lexer::LEX_UNDEF: // parse error
			lex.printError("Unknown LaTeXColor tag `$$Token'");
			error = true;
			continue;

		default:
			break;
		}
		switch (static_cast<LaTeXColorTags>(le)) {
		case LC_END: // end of structure
			finished = true;
			break;
		case LC_GUINAME: {
			lex >> guiname_;
			break;
		}
		case LC_HEXNAME: {
			lex >> hexname_;
			break;
		}
		case LC_CATEGORY: {
			lex >> category_;
			break;
		}
		case LC_CMYK: {
			lex >> cmyk_;
			break;
		}
		case LC_COLOR_MODEL: {
			lex >> model_;
			break;
		}
		case LC_LATEXNAME: {
			lex >> latex_;
			break;
		}
		case LC_SVG_CLASH: {
			lex >> svgclash_;
			break;
		}
		case LC_REQUIRES: {
			lex.eatLine();
			string features = lex.getString();
			requires_ = getVectorFromString(features);
			break;
		}
		}
	}
	if (!finished) {
		lex.printError("No EndColor tag found for LaTeXColor tag `$$Token'");
		return false;
	}
	lex.popTable();
	return finished && !error;
}


bool LaTeXColor::read(Lexer & lex)
{
	svgclash_ = false;

	if (!lex.next()) {
		lex.printError("No name given for LaTeX color: `$$Token'.");
		return false;
	}

	name_ = ascii_lowercase(lex.getString());
	LYXERR(Debug::INFO, "Reading LaTeX color " << name_);
	if (!readColor(lex)) {
		LYXERR0("Error parsing LaTeX font `" << name_ << '\'');
		return false;
	}

	return true;
}


void LaTeXColors::readLaTeXColors()
{
	// Read latexcolors file
	FileName filename = libFileSearch(string(), "latexcolors");
	if (filename.empty()) {
		LYXERR0("Error: latexcolors file not found!");
		return;
	}
	Lexer lex;
	lex.setFile(filename);
	lex.setContext("LaTeXFeatures::readLaTeXColors");
	while (lex.isOK()) {
		int le = lex.lex();
		switch (le) {
		case Lexer::LEX_FEOF:
			continue;

		default:
			break;
		}
		string const type = lex.getString();
		if (type != "Color") {
			lex.printError("Unknown LaTeXColor tag `$$Token'");
			continue;
		}
		LaTeXColor c;
		c.read(lex);
		if (!lex)
			break;

		texcolormap_.push_back(make_pair(c.name(), c));
	}
}


LaTeXColors::TexColorMap LaTeXColors::getLaTeXColors()
{
	if (texcolormap_.empty())
		readLaTeXColors();
	return texcolormap_;
}


LaTeXColor LaTeXColors::getLaTeXColor(string const & name)
{
	if (name == "default" || name == "none")
		return LaTeXColor();
	if (texcolormap_.empty())
		readLaTeXColors();
	for (auto const & lc : texcolormap_) {
		if (lc.first == name)
			return lc.second;
	}
	LYXERR0("LaTeXColors::getLaTeXColor: color '" << name << "' not found!");
	return LaTeXColor();
}

bool LaTeXColors::isLaTeXColor(string const & lyxname)
{
	if (texcolormap_.empty())
		readLaTeXColors();
	for (auto const & lc : texcolormap_) {
		if (lc.first == lyxname)
			return true;
	}
	return false;
}


bool LaTeXColors::isRealLaTeXColor(string const & latexname)
{
	if (texcolormap_.empty())
		readLaTeXColors();
	for (auto const & lc : texcolormap_) {
		if (lc.second.latex() == latexname)
			return true;
	}
	return false;
}


string LaTeXColors::getFromLaTeXColor(string const & latexname)
{
	if (texcolormap_.empty())
		readLaTeXColors();
	for (auto const & lc : texcolormap_) {
		if (lc.second.latex() == latexname)
			return lc.first;
	}
	return string();
}


map<string, string> LaTeXColors::getSVGClashColors()
{
	map<string, string> res;
	if (texcolormap_.empty())
		readLaTeXColors();
	for (auto const & lc : texcolormap_) {
		if (lc.second.svgclash())
			res[lc.first] = lc.second.cmyk();
	}
	return res;
}


} // namespace lyx
