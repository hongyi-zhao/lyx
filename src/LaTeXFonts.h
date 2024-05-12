// -*- C++ -*-
/**
 * \file LaTeXFonts.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef LATEXFONTS_H
#define LATEXFONTS_H

#include "support/docstring.h"

#include <map>
#include <vector>


namespace lyx {

namespace support { class Lexer; }

/// LaTeX Font definition
class LaTeXFont {
public:
	/// TeX font
	// FIXME Add fontenc tag to classes which is used if no font is specified?
	LaTeXFont() : osfdefault_(false), switchdefault_(false), moreopts_(false),
		osffontonly_(false) { fontenc_.push_back("T1"); }
	/// The font name
	docstring const & name() const { return name_; }
	/// The name to appear in the document dialog
	docstring const & guiname() const { return guiname_; }
	/// Font family (rm, sf, tt)
	docstring const & family() const { return family_; }
	/// The package that provides this font
	docstring const & package() const { return package_; }
	/// Does this provide a specific font encoding?
	bool hasFontenc(std::string const &) const;
	/// The font encoding(s)
	std::vector<std::string> const & fontencs() const { return fontenc_; }
	/// Alternative font if package() is not available
	std::vector<docstring> const & altfonts() const { return altfonts_; }
	/// A font that provides all families
	docstring const & completefont() const { return completefont_; }
	/// A font specifically needed for OT1 font encoding
	docstring const & ot1font() const { return ot1font_; }
	/// A font that provides Old Style Figures for this type face
	docstring const & osffont() const { return osffont_; }
	/// A package option for Old Style Figures
	docstring const & osfoption() const { return osfoption_; }
	/// A package option for true SmallCaps
	docstring const & scoption() const { return scoption_; }
	/// A package option for both Old Style Figures and SmallCaps
	docstring const & osfscoption() const { return osfscoption_; }
	/// A package option for font scaling
	docstring const & scaleoption() const { return scaleoption_; }
	/// A macro for font scaling
	docstring const & scalecmd() const { return scalecmd_; }
	/// Does this provide additional options?
	bool providesMoreOptions(bool ot1, bool complete, bool nomath) const;
	/// Alternative requirement to test for
	docstring const & required() const { return required_; }
	/// Does this font provide a given \p feature
	bool provides(std::string const & name, bool ot1,
		      bool complete, bool nomath) const;
	/// Issue the familydefault switch
	bool switchdefault() const { return switchdefault_; }
	/// Does the font provide Old Style Figures as default?
	bool osfDefault() const { return osfdefault_; }
	/// Does OSF font replace (rather than complement) the non-OSF one?
	bool osfFontOnly() const { return osffontonly_; }
	/// Is this font available?
	bool available(bool ot1, bool nomath) const;
	/// Does this font provide an alternative without math?
	bool providesNoMath(bool ot1, bool complete) const;
	/// Does this font provide Old Style Figures?
	bool providesOSF(bool ot1, bool complete, bool nomath) const;
	/// Does this font provide optional true SmallCaps?
	bool providesSC(bool ot1, bool complete, bool nomath) const;
	/** does this font provide OSF and Small Caps only via
	 * a single, undifferentiated expert option?
	 */
	bool hasMonolithicExpertSet(bool ot1, bool complete, bool nomath) const;
	/// Does this font provide scaling?
	bool providesScale(bool ot1, bool complete, bool nomath) const;
	/// Return the LaTeX Code
	std::string const getLaTeXCode(bool dryrun, bool ot1, bool complete,
				       bool sc, bool osf, bool nomath,
				       std::string const & extraopts = std::string(),
				       int scale = 100) const;
	/// Return the actually used font
	docstring const getUsedFont(bool ot1, bool complete, bool nomath, bool osf) const;
	/// Return the actually used package
	docstring const getUsedPackage(bool ot1, bool complete, bool nomath) const;
	///
	bool read(support::Lexer & lex);
	///
	bool readFont(support::Lexer & lex);
private:
	/// Return the preferred available package
	std::string const getAvailablePackage(bool dryrun) const;
	/// Return the package options
	std::string const getPackageOptions(bool ot1,
					    bool complete,
					    bool sc,
					    bool osf,
					    int scale,
					    std::string const & extraopts,
					    bool nomath) const;
	/// Return an alternative font
	LaTeXFont altFont(docstring const & name) const;
	///
	docstring name_;
	///
	docstring guiname_;
	///
	docstring family_;
	///
	docstring package_;
	///
	std::vector<std::string> fontenc_;
	///
	std::vector<docstring> altfonts_;
	///
	docstring completefont_;
	///
	docstring nomathfont_;
	///
	docstring ot1font_;
	///
	docstring osffont_;
	///
	docstring packageoptions_;
	///
	docstring osfoption_;
	///
	docstring scoption_;
	///
	docstring osfscoption_;
	///
	docstring scaleoption_;
	///
	docstring scalecmd_;
	///
	std::vector<std::string> provides_;
	///
	docstring required_;
	///
	docstring preamble_;
	///
	bool osfdefault_;
	///
	bool switchdefault_;
	///
	bool moreopts_;
	///
	bool osffontonly_;
};


/** The list of available LaTeX fonts
 */
class LaTeXFonts {
public:
	///
	typedef std::map<docstring, LaTeXFont> TexFontMap;
	/// Get all LaTeXFonts
	TexFontMap getLaTeXFonts();
	/// Get a specific LaTeXFont \p name
	LaTeXFont getLaTeXFont(docstring const & name);
	/// Get a specific AltFont \p name
	LaTeXFont getAltFont(docstring const & name);
private:
	///
	void readLaTeXFonts();
	///
	TexFontMap texfontmap_;
	///
	TexFontMap texaltfontmap_;
};

/// Implementation is in LyX.cpp
extern LaTeXFonts & theLaTeXFonts();


} // namespace lyx

#endif
