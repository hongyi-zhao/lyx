// -*- C++ -*-
/**
 * \file Layout.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author Jean-Marc Lasgouttes
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef LAYOUT_H
#define LAYOUT_H

#include "FontInfo.h"
#include "LayoutEnums.h"
#include "Spacing.h"
#include "support/debug.h"
#include "support/docstring.h"

#include <map>
#include <set>

namespace lyx {

namespace support { class Lexer; }

class TextClass;

/* Fixed labels are printed flushright, manual labels flushleft.
 * MARGIN_MANUAL and MARGIN_FIRST_DYNAMIC are *only* for LABEL_MANUAL,
 * MARGIN_DYNAMIC and MARGIN_STATIC are *not* for LABEL_MANUAL.
 * This seems a funny restriction, but I think other combinations are
 * not needed, so I will not change it yet.
 * Correction: MARGIN_FIRST_DYNAMIC also usable with LABEL_STATIC.
 */


/* There is a parindent and a parskip. Which one is used depends on the
 * paragraph_separation-flag of the text-object.
 * BUT: parindent is only thrown away, if a parskip is defined! So if you
 * want a space between the paragraphs and a parindent at the same time,
 * you should set parskip to zero and use topsep, parsep and bottomsep.
 *
 * The standard layout is an exception: its parindent is only set, if the
 * previous paragraph is standard too. Well, this is LateX and it is good!
 */

///
class Layout {
public:
	///
	Layout();
	/// is this layout a default layout created for an unknown layout
	bool isUnknown() const { return unknown_; }
	void setUnknown(bool unknown) { unknown_ = unknown; }
	/// Reads a layout definition from file
	/// \return true on success.
	bool read(support::Lexer &, TextClass const &, bool validating = false);
	///
	void readAlign(support::Lexer &);
	///
	void readAlignPossible(support::Lexer &);
	///
	void readLabelType(support::Lexer &);
	///
	void readEndLabelType(support::Lexer &);
	///
	void readMargin(support::Lexer &);
	///
	void readLatexType(support::Lexer &);
	///
	void readSpacing(support::Lexer &);
	///
	void readArgument(support::Lexer &, bool);
	/// Write a layout definition in utf8 encoding
	void write(std::ostream &) const;
	///
	docstring const & name() const { return name_; }
	///
	void setName(docstring const & n) { name_ = n; }
	///
	docstring const & obsoleted_by() const { return obsoleted_by_; }
	///
	docstring const & depends_on() const { return depends_on_; }
	///
	std::string const & latexname() const { return latexname_; }
	///
	std::string const & itemcommand() const { return itemcommand_; }
	/// One argument of this layout
	struct latexarg {
		docstring labelstring;
		docstring menustring;
		bool mandatory = false;
		bool nodelims = false;
		docstring ldelim;
		docstring rdelim;
		docstring defaultarg;
		docstring presetarg;
		docstring tooltip;
		std::string required;
		std::string decoration;
		FontInfo font = inherit_font;
		FontInfo labelfont = inherit_font;
		bool autoinsert = false;
		bool insertcotext = false;
		bool insertonnewline = false;
		ArgPassThru passthru = PT_INHERITED;
		docstring escape_chars;
		docstring pass_thru_chars;
		bool is_toc_caption = false;
		bool free_spacing = false;
		bool inh_font = true;
		std::string newlinecmd;
		/// The DocBook tag corresponding to this argument.
		docstring docbooktag;
		docstring docbooktagtype;
		docstring docbookattr;
		/// Whether this argument should be output after the main tag (default: inside). The result if the argument
		/// should be output both before and after the main tag is undefined.
		bool docbookargumentbeforemaintag = false;
		/// Whether this argument should be output before the main tag (default: inside). The result if the argument
		/// should be output both before and after the main tag is undefined.
		bool docbookargumentaftermaintag = false;
	};
	///
	typedef std::map<std::string, latexarg> LaTeXArgMap;
	///
	LaTeXArgMap const & latexargs() const { return latexargs_; }
	///
	LaTeXArgMap const & postcommandargs() const { return postcommandargs_; }
	///
	LaTeXArgMap const & listpreamble() const { return listpreamble_; }
	///
	LaTeXArgMap const & itemargs() const { return itemargs_; }
	/// Returns true is the layout has arguments. If false, then an
	/// InsetArgument in this layout stands for the parent InsetText.
	bool hasArgs() const;
	/// Returns latexargs() + postcommandargs() + itemargs().
	/// But note that it returns a *copy*, not a reference, so do not do
	/// anything like:
	///   Layout::LaTeXArgMap::iterator it = args().begin();
	///   Layout::LaTeXArgMap::iterator en = args().end();
	/// Those are iterators for different containers.
	LaTeXArgMap args() const;
	///
	int optArgs() const;
	///
	int requiredArgs() const;
	///
	docstring const & labelstring(bool in_appendix) const
	{ return in_appendix ? labelstring_appendix_ : labelstring_; }
	///
	docstring const & endlabelstring() const { return endlabelstring_; }
	///
	docstring const & category() const { return category_; }
	///
	docstring const & preamble() const { return preamble_; }
	/// Get language dependent macro definitions needed for this layout
	/// for language \p lang
	docstring const & langpreamble() const { return langpreamble_; }
	/// Get language and babel dependent macro definitions needed for
	/// this layout for language \p lang
	docstring const & babelpreamble() const { return babelpreamble_; }
	///
	std::set<std::string> const & required() const { return required_; }
	///
	std::set<docstring> const & autonests() const { return autonests_; }
	///
	std::set<docstring> const & isAutonestedBy() const { return autonested_by_; }
	///
	std::string const & latexparam() const { return latexparam_; }
	///
	docstring leftdelim() const & { return leftdelim_; }
	///
	docstring rightdelim() const & { return rightdelim_; }
	///
	std::string const & innertag() const { return innertag_; }
	///
	std::string const & labeltag() const { return labeltag_; }
	///
	std::string const & itemtag() const { return itemtag_; }
	///
	std::string const & thmName() const { return thm_name_; }
	///
	std::string const & thmLaTeXName() const { return thm_latex_name_; }
	///
	std::string const & thmCounter() const { return thm_counter_; }
	///
	std::string const & thmParentCounter() const { return thm_parent_counter_; }
	///
	std::string const & thmStyle() const { return thm_style_; }
	///
	std::string const & thmZRefName() const { return thm_zref_name_; }
	///
	std::string const & thmXRefName() const { return thm_xref_name_; }
	///
	std::string const & thmXRefNamePl() const { return thm_xref_name_pl_; }
	///
	std::string const & htmltag() const;
	///
	std::string const & htmlattr() const { return htmlattr_; }
	///
	std::string const & htmlclass() const;
	/// Returns a complete attribute string, including class, etc.
	std::string const & htmlGetAttrString() const;
	///
	std::string const & htmlitemtag() const;
	///
	std::string const & htmlitemattr() const;
	///
	std::string const & htmllabeltag() const;
	///
	std::string const & htmllabelattr() const;
	///
	bool htmlintoc() const { return htmlintoc_; }
	///
	std::string defaultCSSClass() const;
	///
	bool htmllabelfirst() const { return htmllabelfirst_; }
	///
	docstring htmlstyle() const;
	///
	docstring const & htmlpreamble() const { return htmlpreamble_; }
	///
	bool htmltitle() const { return htmltitle_; }
	///
	std::string const & docbooktag() const;
	///
	std::string const & docbookattr() const;
	///
	std::string const & docbooktagtype() const;
	///
	std::string const & docbookinnertag() const;
	///
	std::string const & docbookinnerattr() const;
	///
	std::string const & docbookinnertagtype() const;
	///
	std::string const & docbookininfo() const;
	///
	bool docbookabstract() const { return docbookabstract_; }
	///
	std::string const & docbookwrappertag() const;
	///
	std::string const & docbookwrapperattr() const;
	///
	std::string const & docbookwrappertagtype() const;
	///
	bool docbookwrappermergewithprevious() const { return docbookwrappermergewithprevious_; }
	///
	std::string const & docbooksectiontag() const;
	///
	bool docbooksection() const { return docbooksection_; }
	///
	std::string const & docbookitemwrappertag() const;
	///
	std::string const & docbookitemwrapperattr() const;
	///
	std::string const & docbookitemwrappertagtype() const;
	///
	std::string const & docbookitemlabeltag() const;
	///
	std::string const & docbookitemlabelattr() const;
	///
	std::string const & docbookitemlabeltagtype() const;
	///
	std::string const & docbookiteminnertag() const;
	///
	std::string const & docbookiteminnerattr() const;
	///
	std::string const & docbookiteminnertagtype() const;
	///
	std::string const & docbookitemtag() const;
	///
	std::string const & docbookitemattr() const;
	///
	std::string const & docbookitemtagtype() const;
	///
	std::string const & docbookforceabstracttag() const;
	///
	bool docbooknofontinside() const { return docbooknofontinside_; }
	///
	bool docbookgeneratetitle() const { return docbookgeneratetitle_; }
	///
	bool isParagraph() const { return latextype == LATEX_PARAGRAPH; }
	///
	bool isCommand() const { return latextype == LATEX_COMMAND; }
	///
	bool isEnvironment() const {
		return latextype == LATEX_ENVIRONMENT
			|| latextype == LATEX_BIB_ENVIRONMENT
			|| latextype == LATEX_ITEM_ENVIRONMENT
			|| latextype == LATEX_LIST_ENVIRONMENT;
	}
	/// Is this the kind of layout in which adjacent paragraphs
	/// are handled as one group?
	bool isParagraphGroup() const {	return par_group_; }
	///
	bool isMultiparCommand() const { return isCommand() && multi_par_; }
	///
	bool labelIsInline() const {
		return labeltype == LABEL_STATIC
			|| labeltype == LABEL_SENSITIVE
			|| labeltype == LABEL_ENUMERATE
			|| labeltype == LABEL_ITEMIZE;
	}
	bool labelIsAbove() const {
		return labeltype == LABEL_ABOVE
			|| labeltype == LABEL_CENTERED
			|| labeltype == LABEL_BIBLIO;
	}
	bool isNumHeadingLabelType() const {
		return labeltype == LABEL_ABOVE
			|| labeltype == LABEL_CENTERED
			|| labeltype == LABEL_STATIC;
	}
	///
	bool addToToc() const { return add_to_toc_; }
	///
	std::string const & tocType() const { return toc_type_; }
	///
	bool isTocCaption() const { return is_toc_caption_; }

	///
	bool operator==(Layout const &) const;
	///
	bool operator!=(Layout const & rhs) const
		{ return !(*this == rhs); }

	////////////////////////////////////////////////////////////////
	// members
	////////////////////////////////////////////////////////////////
	/** Default font for this layout/environment.
	    The main font for this kind of environment. If an attribute has
	    INHERITED_*, it means that the value is specified by
	    the defaultfont for the entire layout. If we are nested, the
	    font is inherited from the font in the environment one level
	    up until the font is resolved. The values :IGNORE_*
	    and FONT_TOGGLE are illegal here.
	*/
	FontInfo font;

	/** Default font for labels.
	    Interpretation the same as for font above
	*/
	FontInfo labelfont;

	/** Resolved version of the font for this layout/environment.
	    This is a resolved version the default font. The font is resolved
	    against the defaultfont of the entire layout.
	*/
	FontInfo resfont;

	/** Resolved version of the font used for labels.
	    This is a resolved version the label font. The font is resolved
	    against the defaultfont of the entire layout.
	*/
	FontInfo reslabelfont;

	/// Text that dictates how wide the left margin is on the screen
	docstring leftmargin;
	/// Text that dictates how wide the right margin is on the screen
	docstring rightmargin;
	/// Text that dictates how much space to leave after a potential label
	docstring labelsep;
	/// Text that dictates how much space to leave before a potential label
	docstring labelindent;
	/// Text that dictates the width of the indentation of indented pars
	docstring parindent;
	///
	double parskip;
	///
	double itemsep;
	///
	double topsep;
	///
	double bottomsep;
	///
	double labelbottomsep;
	///
	double parsep;
	///
	Spacing spacing;
	///
	LyXAlignment align;
	///
	LyXAlignment alignpossible;
	///
	LabelType labeltype;
	///
	EndLabelType endlabeltype;
	///
	MarginType margintype;
	///
	bool newline_allowed;
	///
	bool nextnoindent;
	///
	ToggleIndentation toggle_indent;
	///
	bool free_spacing;
	///
	bool pass_thru;
	/// Individual chars to be escaped
	docstring escape_chars;
	/// Individual chars to be passed verbatim
	docstring pass_thru_chars;
	/// Individual characters that must not be
	/// passed verbatim even if normally requested
	docstring no_pass_thru_chars;
	///
	bool parbreak_is_newline;
	/// show this in toc
	int toclevel;
	/// special value of toclevel for non-section layouts
	static const int NOT_IN_TOC;

	/** true when the fragile commands in the paragraph need to be
	    \protect'ed. */
	bool needprotect;
	/** true when the verbatim stuff of this layout needs to be
	    \cprotect'ed. */
	bool needcprotect;
	/** true when the verbatim stuff of this layout never should be
	    \cprotect'ed. */
	bool nocprotect;
	/** true when specific commands in this paragraph need to be
	    protected in an \mbox. */
	bool needmboxprotect;
	/// true when empty paragraphs should be kept.
	bool keepempty;
	/// Type of LaTeX object
	LatexType latextype;
	/// Does this object belong in the title part of the document?
	bool intitle;
	/// Is the content to go in the preamble rather than the body?
	bool inpreamble;
	/// Which counter to step
	docstring counter;
	/// Resume counter?
	bool resumecounter;
	/// Step parent counter?
	bool stepparentcounter;
	/// Prefix to use when creating labels
	docstring refprefix;
	/// Depth of XML command
	int commanddepth;

	/// Is this spellchecked?
	bool spellcheck;
	/**
	 * Should this layout definition always be written to the document preamble?
	 * Possible values are:
	 *   0: Do not enforce local layout
	 * >=1: Enforce local layout with version forcelocal
	 *  -1: Enforce local layout with infinite version
	 * On reading, the forced local layout is only used if its version
	 * number is greater than the version number of the same layout in the
	 * document class. Otherwise, it is ignored.
	 */
	int forcelocal;


private:
	/// Reads a layout definition from file
	/// \return true on success.
	bool readIgnoreForcelocal(support::Lexer &, TextClass const &, bool validating);
	/// generates the default CSS for this layout
	void makeDefaultCSS() const;
	///
	std::string defaultCSSItemClass() const { return defaultCSSClass() + "_item"; }
	///
	std::string defaultCSSLabelClass() const { return defaultCSSClass() + "_label"; }

	/// Name of the layout/paragraph environment
	docstring name_;

	/// LaTeX name for environment
	std::string latexname_;

	/** Is this layout the default layout for an unknown layout? If
	 * so, its name will be displayed as xxx (unknown).
	 */
	bool unknown_;

	/** Name of an layout that has replaced this layout.
	    This is used to rename a layout, while keeping backward
	    compatibility
	*/
	docstring obsoleted_by_;

	/** Name of an layout which preamble must come before this one
	    This is used when the preamble snippet uses macros defined in
	    another preamble
	 */
	docstring depends_on_;

	/// Label string. "Abstract", "Reference", "Caption"...
	docstring labelstring_;
	///
	docstring endlabelstring_;
	/// Label string inside appendix. "Appendix", ...
	docstring labelstring_appendix_;
	/// LaTeX parameter for environment
	std::string latexparam_;
	/// Item command in lists
	std::string itemcommand_;
	/// Left delimiter of the content
	docstring leftdelim_;
	/// Right delimiter of the content
	docstring rightdelim_;
	/// Internal tag to use (e.g., <title></title> for sect header)
	std::string innertag_;
	/// Internal tag to use (e.g. to surround varentrylist label)
	std::string labeltag_;
	/// Internal tag to surround the item text in a list.
	std::string itemtag_;
	/// Tags for theorem definitions
	/// 1. name
	std::string thm_name_;
	/// 2. latex name
	std::string thm_latex_name_;
	/// 3. counter
	std::string thm_counter_;
	/// 4. parent counter (e.g., section)
	std::string thm_parent_counter_;
	/// 5. style
	std::string thm_style_;
	/// 6. zref-clever name
	std::string thm_zref_name_;
	/// 7. cross references name
	std::string thm_xref_name_;
	/// 8. cross references name (plural)
	std::string thm_xref_name_pl_;
	/// The interpretation of this tag varies depending upon the latextype.
	/// In an environment, it is the tag enclosing all content for this set of
	/// paragraphs. So for quote, e.g,. it would be: blockquote. For itemize,
	/// it would be: ul. (You get the idea.)
	///
	/// For a command, it is the tag enclosing the content of the command.
	/// So, for section, it might be: h2.
	///
	/// For the paragraph type, it is the tag that will enclose each paragraph.
	///
	/// Defaults to "div".
	mutable std::string htmltag_;
	/// Additional attributes for inclusion with the start tag.
	/// Note that the CSS class is handled separately.
	std::string htmlattr_;
	/// The CSS class to use. Calculated from the layout name if not given.
	mutable std::string htmlclass_;
	/// cached
	mutable std::string htmlfullattrs_;
	/// Tag for individual paragraphs in an environment. In lists, this
	/// would be something like "li". But it also needs to be set for
	/// quotation, e.g., since the paragraphs in a quote need to be
	/// in "p" tags. Default is "div".
	/// Note that when I said "environment", I meant it: This has no
	/// effect for LATEX_PARAGRAPH type layouts.
	mutable std::string htmlitemtag_;
	/// Attributes for htmlitemtag_. Default is: class="layoutname_item".
	mutable std::string htmlitemattr_;
	/// Tag for labels, of whatever sort. One use for this is in setting
	/// descriptions, in which case it would be: dt. Another use is to
	/// customize the display of, say, the auto-generated label for
	/// sections. Defaults to "span".
	/// If set to "NONE", this suppresses the printing of the label.
	mutable std::string htmllabeltag_;
	/// Attributes for the label. Defaults to: class="layoutname_label".
	mutable std::string htmllabelattr_;
	/// Whether to put the label before the item, or within the item.
	/// I.e., do we have (true):
	///    <label>...</label><item>...</item>
	/// or instead (false):
	///    <item><label>...</label>...</item>
	/// The latter is the default.
	bool htmllabelfirst_;
	/// Is this to be output with the toc?
	bool htmlintoc_;
	/// CSS information needed by this layout.
	docstring htmlstyle_;
	/// DocBook tag corresponding to this layout.
	mutable std::string docbooktag_;
	/// Roles to add to docbooktag_, if any (default: none).
	mutable std::string docbookattr_;
	/// DocBook tag type corresponding to this layout (block, paragraph, or inline; default: block).
	mutable std::string docbooktagtype_;
	/// DocBook inner tag corresponding to this layout.
	mutable std::string docbookinnertag_;
	/// Roles to add to docbookinnertag_, if any (default: none).
	mutable std::string docbookinnerattr_;
	/// DocBook inner-tag type corresponding to this layout (block, paragraph, or inline; default: block).
	mutable std::string docbookinnertagtype_;
	/// DocBook tag corresponding to this item (mainly for lists).
	mutable std::string docbookitemtag_;
	/// Roles to add to docbookitemtag_, if any (default: none).
	mutable std::string docbookitemattr_;
	/// DocBook tag type corresponding to this item (block, paragraph, or inline; default: block).
	mutable std::string docbookitemtagtype_;
	/// DocBook tag corresponding to the wrapper around an item (mainly for lists).
	mutable std::string docbookitemwrappertag_;
	/// Roles to add to docbookitemwrappertag_, if any (default: none).
	mutable std::string docbookitemwrapperattr_;
	/// DocBook tag type corresponding to the wrapper around an item (block, paragraph, or inline; default: block).
	mutable std::string docbookitemwrappertagtype_;
	/// DocBook tag corresponding to this label (mostly for description lists;
	/// labels in the common sense do not exist with DocBook).
	mutable std::string docbookitemlabeltag_;
	/// Roles to add to docbooklabeltag_, if any (default: none).
	mutable std::string docbookitemlabelattr_;
	/// DocBook tag corresponding to this label (block, paragraph, or inline; default: block).
	mutable std::string docbookitemlabeltagtype_;
	/// DocBook tag to add within the item, around its direct content (mainly for lists).
	mutable std::string docbookiteminnertag_;
	/// Roles to add to docbookiteminnertag_, if any (default: none).
	mutable std::string docbookiteminnerattr_;
	/// DocBook tag to add within the item, around its direct content (block, paragraph, or inline; default: block).
	mutable std::string docbookiteminnertagtype_;
	/// DocBook tag corresponding to this wrapper around the main tag.
	mutable std::string docbookwrappertag_;
	/// Roles to add to docbookwrappertag_, if any (default: none).
	mutable std::string docbookwrapperattr_;
	/// DocBook tag corresponding to this wrapper around the main tag (block, paragraph, or inline; default: block).
	mutable std::string docbookwrappertagtype_;
	/// Whether this wrapper tag may be merged with the previously opened wrapper tag.
	bool docbookwrappermergewithprevious_;
	/// Outer tag for this section, only if this layout represent a sectionning item, including chapters
	/// (default: section).
	mutable std::string docbooksectiontag_;
	/// Whether this element should be considered as a section-level element in DocBook.
	bool docbooksection_;
	/// Whether this tag must/can/can't go into an <info> tag (default: never, as it only makes sense for metadata).
	mutable std::string docbookininfo_;
	/// Whether this paragraph should be considered as abstract. Such paragraphs are output as a part of the document
	/// abstract element (corresponding to the root tag).
	bool docbookabstract_;
	/// Whether this element (root or not) does not accept text without a section (i.e. the first text that is met
	/// in LyX must be considered as the abstract if this is true); this text must be output with the specific tag
	/// held by this attribute.
	mutable std::string docbookforceabstracttag_;
	/// Whether font tags are allowed inside this tag.
	bool docbooknofontinside_ = false;
	/// Whether LyX should create a title on its own, just after the wrapper tag. Typically, this parameter is required
	/// because the wrapper tag requires a title (like a figure). The generated title will be similar to a LyXHTML label
	/// (environment type and a number).
	bool docbookgeneratetitle_ = false;
	/// Should we generate the default CSS for this layout, even if HTMLStyle
	/// has been given? Default is false.
	/// Note that the default CSS is output first, then the user CSS, so it is
	/// possible to override what one does not want.
	bool htmlforcecss_;
	/// A cache for the default style info so generated.
	mutable docstring htmldefaultstyle_;
	/// Any other info for the HTML header.
	docstring htmlpreamble_;
	/// Whether this is the <title> paragraph.
	bool htmltitle_;
	/// calculating this is expensive, so we cache it.
	mutable std::string defaultcssclass_;
	/// This is the `category' for this layout. The following are
	/// recommended basic categories: FrontMatter, BackMatter, MainText,
	/// Sectioning, Starred, List, Reasoning.
	docstring category_;
	/// Macro definitions needed for this layout
	docstring preamble_;
	/// Language dependent macro definitions needed for this layout
	docstring langpreamble_;
	/// Language and babel dependent macro definitions needed for this layout
	docstring babelpreamble_;
	/// Are adjacent paragraphs handled as one group?
	bool par_group_;
	/// Packages needed for this layout
	std::set<std::string> required_;
	/// Layouts that are by default nested after this one
	std::set<docstring> autonests_;
	/// Layouts that by auto-nest this one
	std::set<docstring> autonested_by_;
	///
	LaTeXArgMap latexargs_;
	///
	LaTeXArgMap postcommandargs_;
	///
	LaTeXArgMap listpreamble_;
	///
	LaTeXArgMap itemargs_;
	///
	bool add_to_toc_;
	///
	bool multi_par_;
	///
	std::string toc_type_;
	///
	bool is_toc_caption_;
};


} // namespace lyx

#endif
