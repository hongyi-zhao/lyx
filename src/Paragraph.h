// -*- C++ -*-
/**
 * \file Paragraph.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Asger Alstrup
 * \author Lars Gullik Bjønnes
 * \author John Levon
 * \author André Pönitz
 * \author Jürgen Vigna
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef PARAGRAPH_H
#define PARAGRAPH_H

#include "LayoutEnums.h"
#include "SpellChecker.h"

#include "support/types.h"

#include "insets/InsetCode.h"
#include "insets/InsetLayout.h"

#include <set>
#include <vector>

namespace lyx {

class AuthorList;
class Buffer;
class BufferParams;
class Change;
class Cursor;
class DocIterator;
class docstring_list;
class DocumentClass;
class Inset;
class LaTeXFeatures;
class InsetList;
class Language;
class Layout;
class Font;
class OutputParams;
class ParagraphParameters;
class TocBackend;
class WordLangTuple;
class XMLStream;
class otexstream;

/// Inset identifier (above 0x10ffff, for ucs-4)
char_type const META_INSET = 0x200001;

class FontSpan {
public:
	/// Invalid font span containing no character
	FontSpan() : first(0), last(-1) {}
	/// Span including first and last
	FontSpan(pos_type f, pos_type l) : first(f), last(l) {}

public:
	/// Range including first and last.
	pos_type first, last;

	inline bool operator<(FontSpan const & s) const
	{
		return first < s.first;
	}

	inline bool operator==(FontSpan const & s) const
	{
		return first == s.first && last == s.last;
	}

	inline bool contains(pos_type p) const
	{
		return first <= p && p <= last;
	}

	inline size_t size() const
	{
		return empty() ? 0 : last - first;
	}


	inline FontSpan intersect(FontSpan const & f) const
	{
		FontSpan result = FontSpan();
		if (contains(f.first))
			result.first = f.first;
		else if (f.contains(first))
			result.first = first;
		else
			return result;
		if (contains(f.last))
			result.last = f.last;
		else if (f.contains(last))
			result.last = last;
		return result;
	}

	inline bool empty() const
	{
		return first > last;
	}
};

///
enum TextCase {
	///
	text_lowercase,
	///
	text_capitalization,
	///
	text_uppercase,
	///
	text_invertcase
};


///
enum AsStringParameter
{
	AS_STR_NONE = 0, ///< No option, only printable characters.
	AS_STR_LABEL = 1, ///< Prefix with paragraph label.
	AS_STR_INSETS = 2, ///< Go into insets.
	AS_STR_NEWLINES = 4, ///< Get also newline characters.
	AS_STR_SKIPDELETE = 8, ///< Skip deleted text in change tracking.
	AS_STR_PLAINTEXT = 16, ///< Don't export formatting when descending into insets.
	AS_STR_MATHED = 32 ///< Use a format suitable for mathed (eg. for InsetRef).
};


/// A Paragraph holds all text, attributes and insets in a text paragraph
class Paragraph
{
public:
	///
	Paragraph();
	/// Copy constructor.
	Paragraph(Paragraph const &);
	/// Partial copy constructor.
	/// Copy the Paragraph contents from \p beg to \p end (without end).
	Paragraph(Paragraph const & par, pos_type beg, pos_type end);
	///
	Paragraph & operator=(Paragraph const &);
	///
	~Paragraph();
	///
	int id() const;
	///
	void setId(int id);

	///
	void addChangesToToc(DocIterator const & cdit, Buffer const & buf,
	                     bool output_active, TocBackend & backend) const;
	///
	Language const * getParLanguage(BufferParams const &) const;
	///
	bool isRTL(BufferParams const &) const;
	///
	void changeLanguage(BufferParams const & bparams,
			    Language const * from, Language const * to);
	///
	bool isMultiLingual(BufferParams const &) const;
	///
	void getLanguages(std::set<Language const *> &) const;

	/// Convert the paragraph to a string.
	/// \param AsStringParameter options. This can contain any combination of
	/// asStringParameter values. Valid examples:
	///		asString(AS_STR_LABEL)
	///		asString(AS_STR_LABEL | AS_STR_INSETS)
	///		asString(AS_STR_INSETS)
	docstring asString(int options = AS_STR_NONE) const;

	/// Convert the paragraph to a string.
	/// \note If options includes AS_STR_PLAINTEXT, then runparams must be != 0
	docstring asString(pos_type beg, pos_type end,
			   int options = AS_STR_NONE,
			   const OutputParams *runparams = 0) const;
	///
	void forOutliner(docstring &, size_t maxlen, bool shorten = true,
	                 bool label = true) const;

	///
	void write(std::ostream &, BufferParams const &,
		depth_type & depth) const;
	///
	void validate(LaTeXFeatures &) const;

	/// \param force means: output even if layout.inpreamble is true.
	void latex(BufferParams const &, Font const & outerfont, otexstream &,
		OutputParams const &, int start_pos = 0, int end_pos = -1,
		bool force = false) const;

	/// Can we drop the standard paragraph wrapper?
	bool emptyTag() const;

	/// Get the id of the paragraph, useful for DocBook
	std::string getID(Buffer const & buf, OutputParams const & runparams) const;

	/// Return the string of the (first) \label (cross-referencing target)
	/// in this paragraph, or an empty string
	std::string getLabelForXRef() const;

	/// Output the first word of a paragraph, return the position where it left.
	pos_type firstWordDocBook(XMLStream & xs, OutputParams const & runparams) const;

	/// Output the first word of a paragraph, return the position where it left.
	pos_type firstWordLyXHTML(XMLStream & xs, OutputParams const & runparams) const;

	/// Outputs to stream the DocBook representation, one element per paragraph.
	std::tuple<std::vector<docstring>, std::vector<docstring>, std::vector<docstring>>
	simpleDocBookOnePar(Buffer const & buf,
							                   OutputParams const & runparams,
							                   Font const & outerfont,
							                   pos_type initial = 0,
							                   bool is_last_par = false,
							                   bool ignore_fonts = false) const;

	/// \return any material that has had to be deferred until after the
	/// paragraph has closed.
	docstring simpleLyXHTMLOnePar(Buffer const & buf,
								  XMLStream & xs,
								  OutputParams const & runparams,
								  Font const & outerfont,
								  bool start_paragraph = true,
								  bool close_paragraph = true,
								  pos_type initial = 0) const;

	///
	bool hasSameLayout(Paragraph const & par) const;

	///
	void makeSameLayout(Paragraph const & par);

	///
	void setInsetOwner(Inset const * inset);
	///
	Inset const & inInset() const;
	///
	bool allowParagraphCustomization() const;
	///
	bool usePlainLayout() const;
	///
	bool isPassThru() const;
	///
	bool parbreakIsNewline() const;
	///
	bool allowedInContext(Cursor const & cur, InsetLayout const & il) const;
	///
	bool isPartOfTextSequence() const;
	///
	pos_type size() const;
	///
	bool empty() const;

	///
	Layout const & layout() const;
	/// Do not pass a temporary to this!
	void setLayout(Layout const & layout);
	///
	void setPlainOrDefaultLayout(DocumentClass const & tc);
	///
	void setDefaultLayout(DocumentClass const & tc);
	///
	void setPlainLayout(DocumentClass const & tc);

	/// This is the item depth, only used by enumerate and itemize
	signed char itemdepth;

	/// look up change at given pos
	Change const & lookupChange(pos_type pos) const;

	/// is there a change within the given range (does not
	/// check contained paragraphs)
	bool isChanged(pos_type start, pos_type end) const;
	/// Are there insets containing changes in the range?
	bool hasChangedInsets(pos_type start, pos_type end) const;
	/// is there an unchanged char at the given pos ?
	bool isChanged(pos_type pos) const;
	/// is there a change in the paragraph ?
	bool isChanged() const;

	/// is there an insertion at the given pos ?
	bool isInserted(pos_type pos) const;
	/// is there a deletion at the given pos ?
	bool isDeleted(pos_type pos) const;
	/// is the whole paragraph deleted ?
	bool isDeleted(pos_type start, pos_type end) const;

	/// will the paragraph be physically merged with the next
	/// one if the imaginary end-of-par character is logically deleted?
	bool isMergedOnEndOfParDeletion(bool trackChanges) const;
	/// Return Change form of paragraph break
	Change parEndChange() const;

	/// set change for the entire par
	void setChange(Change const & change);

	/// set change at given pos
	void setChange(pos_type pos, Change const & change);

	/// accept changes within the given range
	void acceptChanges(pos_type start, pos_type end);

	/// reject changes within the given range
	void rejectChanges(pos_type start, pos_type end);

	/// Paragraphs can contain "manual labels", for example, Description
	/// environment. The text for this user-editable label is stored in
	/// the paragraph alongside the text of the rest of the paragraph
	/// (the body). This function returns the starting position of the
	/// body of the text in the paragraph.
	pos_type beginOfBody() const;
	/// recompute this value
	void setBeginOfBody();

	///
	docstring expandLabel(Layout const &, BufferParams const &) const;
	///
	docstring const & labelString() const;
	/// the next two functions are for the manual labels
	docstring const getLabelWidthString() const;
	/// Set label width string.
	void setLabelWidthString(docstring const & s);
	/// Actual paragraph alignment used
	LyXAlignment getAlign(BufferParams const &) const;
	/// Default paragraph alignment as determined by layout
	LyXAlignment getDefaultAlign(BufferParams const &) const;
	/// The nesting depth of a paragraph
	depth_type getDepth() const;
	/// The maximal possible depth of a paragraph after this one
	depth_type getMaxDepthAfter() const;
	///
	void applyLayout(Layout const & new_layout);

	/// (logically) erase the char at pos; return true if it was actually erased
	bool eraseChar(pos_type pos, bool trackChanges);
	/// (logically) erase the given range; return the number of chars actually erased
	int eraseChars(pos_type start, pos_type end, bool trackChanges);

	///
	void resetFonts(Font const & font);

	/** Get uninstantiated font setting. Returns the difference
	    between the characters font and the layoutfont.
	    This is what is stored in the fonttable
	*/
	Font const &
	getFontSettings(BufferParams const &, pos_type pos) const;
	///
	Font const & getFirstFontSettings(BufferParams const &) const;

	/** Get fully instantiated font, i.e., one that does not have any
	    attributes with values FONT_INHERIT, FONT_IGNORE or FONT_TOGGLE.
	*/
	Font const getFont(BufferParams const &, pos_type pos,
			      Font const & outerfont) const;
	Font const getLayoutFont(BufferParams const &,
				    Font const & outerfont) const;
	Font const getLabelFont(BufferParams const &,
				   Font const & outerfont) const;
	/**
	 * The font returned by the above functions is the same in a
	 * span of characters. This method will return the first and
	 * the last positions in the paragraph for which that font is
	 * the same. This can be used to avoid unnecessary calls to getFont.
	 */
	FontSpan fontSpan(pos_type pos) const;
	///
	char_type getChar(pos_type pos) const;
	/// Get the char, but mirror all bracket characters if it is right-to-left
	char_type getUChar(BufferParams const &, OutputParams const &,
			   pos_type pos) const;
	/// pos <= size() (there is a dummy font change at the end of each par)
	void setFont(pos_type pos, Font const & font);
	///
	void insert(pos_type pos, docstring const & str,
		    Font const & font, Change const & change);

	///
	void appendString(docstring const & s, Font const & font,
		Change const & change);
	///
	void appendChar(char_type c, Font const & font, Change const & change);
	///
	void insertChar(pos_type pos, char_type c, bool trackChanges);
	///
	void insertChar(pos_type pos, char_type c,
			Font const &, bool trackChanges);
	///
	void insertChar(pos_type pos, char_type c,
			Font const &, Change const & change);
	/// Insert \p inset at position \p pos with \p change traking status and
	/// \p font.
	/// \return true if successful.
	bool insertInset(pos_type pos, Inset * inset,
			 Font const & font, Change const & change);
	///
	Inset * getInset(pos_type pos);
	///
	Inset const * getInset(pos_type pos) const;

	/// Release inset at given position.
	/// \warning does not honour change tracking!
	/// Therefore, it should only be used for breaking and merging
	/// paragraphs
	Inset * releaseInset(pos_type pos);

	///
	InsetList const & insetList() const;
	///
	void setInsetBuffers(Buffer &);
	///
	void resetBuffer();

	///
	bool isHfill(pos_type pos) const;

	/// hinted by profiler
	bool isInset(pos_type pos) const;
	///
	bool isNewline(pos_type pos) const;
	///
	bool isEnvSeparator(pos_type pos) const;
	/// return true if the char is a word separator
	bool isSeparator(pos_type pos) const;
	///
	bool isLineSeparator(pos_type pos) const;
	/// True if the character/inset at this point is a word separator.
	/// Note that digits in particular are not considered as word separator.
	bool isWordSeparator(pos_type pos, bool const ignore_deleted = false) const;
	/// True if the element at this point is a character that is not a letter.
	bool isChar(pos_type pos) const;
	/// True if the element at this point is a space
	bool isSpace(pos_type pos) const;
	/// True if the element at this point is a hard hyphen or a apostrophe
	/// If it is enclosed by spaces return false
	bool isHardHyphenOrApostrophe(pos_type pos) const;
	/// Return true if this paragraph has verbatim content that needs to be
	/// protected by \cprotect
	bool needsCProtection(bool const fragile = false) const;

	/// returns true if at least one line break or line separator has been deleted
	/// at the beginning of the paragraph (either physically or logically)
	bool stripLeadingSpaces(bool trackChanges);

	/// return true if we allow multiple spaces
	bool isFreeSpacing() const;

	/// return true if we allow this par to stay empty
	bool allowEmpty() const;
	///
	ParagraphParameters & params();
	///
	ParagraphParameters const & params() const;

	/// Check whether a call to fixBiblio is needed.
	bool brokenBiblio() const;
	/// Check if we are in a Biblio environment and insert or
	/// delete InsetBibitems as necessary.
	/// \param cur : the cursor that needs to be updated accordingly
	void fixBiblio(DocIterator & dit);

	/// For each author, set 'used' to true if there is a change
	/// by this author in the paragraph.
	void checkAuthors(AuthorList const & authorList);

	///
	void changeCase(BufferParams const & bparams, pos_type pos,
		pos_type & right, TextCase action);

	/// find \param str string inside Paragraph.
	/// \return non-zero if the specified string is at the specified
	///	position; returned value is the actual match length in positions
	/// \param del specifies whether deleted strings in ct mode will be considered
	int find(
		docstring const & str, ///< string to search
		bool cs, ///<
		bool mw, ///<
		pos_type pos, ///< start from here.
		bool del = true) const;

	void locateWord(pos_type & from, pos_type & to,
		word_location const loc, bool const ignore_deleted = false) const;
	///
	void updateWords();

	/// Spellcheck word at position \p from and fill in found misspelled word
	/// and \p suggestions if \p do_suggestion is true.
	/// \return result from spell checker, SpellChecker::UNKNOWN_WORD when misspelled.
	SpellChecker::Result spellCheck(pos_type & from, pos_type & to, WordLangTuple & wl,
		docstring_list & suggestions, bool do_suggestion =  true,
		bool check_learned = false) const;

	/// Spell checker status at position \p pos.
	/// If \p check_boundary is true the status of position immediately
	/// before \p pos is tested too if it is at word boundary.
	/// \return true if one of the tested positions is misspelled.
	bool isMisspelled(pos_type pos, bool check_boundary = false) const;

	/// \return the spell range (misspelled area) around position.
	/// Range is empty if word at position is correctly spelled.
	FontSpan const & getSpellRange(pos_type pos) const;

	/// spell check of whole paragraph
	/// remember results until call of requestSpellCheck()
	void spellCheck() const;

	/// query state of spell checker results
	bool needsSpellCheck() const;
	/// mark position of text manipulation to inform the spell checker
	/// default value -1 marks the whole paragraph to be checked (again)
	void requestSpellCheck(pos_type pos = -1);

	/// an automatically generated identifying label for this paragraph.
	/// presently used only in the XHTML output routines.
	std::string magicLabel() const;

	/// anonymizes the paragraph contents (but not the paragraphs
	/// contained inside it. Does not handle undo.
	void anonymize();

private:
	/// Expand the counters for the labelstring of \c layout
	docstring expandParagraphLabel(Layout const &, BufferParams const &,
		bool process_appendix) const;
	///
	void deregisterWords();
	///
	void collectWords();
	///
	void registerWords();
	///
	int getInsetPos(InsetCode const code, int startpos,
			bool ignore_deleted=false) const;

	/// Pimpl away stuff
	class Private;
	///
	friend class Paragraph::Private;
	///
	Private * d;
};

} // namespace lyx

#endif // PARAGRAPH_H
