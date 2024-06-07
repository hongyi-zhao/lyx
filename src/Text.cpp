/**
 * \file src/Text.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alfredo Braunstein
 * \author Allan Rae
 * \author André Pönitz
 * \author Angus Leeming
 * \author Asger Alstrup
 * \author Dekel Tsur
 * \author Dov Feldstern
 * \author Jean-Marc Lasgouttes
 * \author John Levon
 * \author Jürgen Vigna
 * \author Lars Gullik Bjønnes
 * \author Stefan Schimanski
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "Text.h"

#include "Author.h"
#include "BranchList.h"
#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Changes.h"
#include "CompletionList.h"
#include "Cursor.h"
#include "CursorSlice.h"
#include "CutAndPaste.h"
#include "DispatchResult.h"
#include "Encoding.h"
#include "ErrorList.h"
#include "factory.h"
#include "FloatList.h"
#include "Font.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "InsetList.h"
#include "Intl.h"
#include "Language.h"
#include "Layout.h"
#include "LyX.h"
#include "LyXAction.h"
#include "lyxfind.h"
#include "LyXRC.h"
#include "Paragraph.h"
#include "ParagraphParameters.h"
#include "SpellChecker.h"
#include "TextClass.h"
#include "TextMetrics.h"
#include "Thesaurus.h"
#include "WordLangTuple.h"
#include "WordList.h"

#include "frontends/alert.h"
#include "frontends/Application.h"
#include "frontends/Clipboard.h"
#include "frontends/Selection.h"

#include "mathed/InsetMathHull.h"
#include "mathed/InsetMathMacroTemplate.h"

#include "insets/Inset.h"
#include "insets/InsetArgument.h"
#include "insets/InsetCaption.h"
#include "insets/InsetCollapsible.h"
#include "insets/InsetCommand.h"
#include "insets/InsetExternal.h"
#include "insets/InsetFloat.h"
#include "insets/InsetFloatList.h"
#include "insets/InsetGraphics.h"
#include "insets/InsetGraphicsParams.h"
#include "insets/InsetIndexMacro.h"
#include "insets/InsetInfo.h"
#include "insets/InsetIPAMacro.h"
#include "insets/InsetNewline.h"
#include "insets/InsetQuotes.h"
#include "insets/InsetSpecialChar.h"
#include "insets/InsetTabular.h"
#include "insets/InsetText.h"
#include "insets/InsetWrap.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/docstream.h"
#include "support/docstring.h"
#include "support/docstring_list.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/Lexer.h"
#include "support/limited_stack.h"
#include "support/lstrings.h"
#include "support/lyxtime.h"
#include "support/textutils.h"
#include "support/unique_ptr.h"

#include <clocale>
#include <regex>
#include <sstream>

using namespace std;

namespace lyx {
using namespace support;
using namespace cap;
using frontend::Clipboard;


namespace {

bool moveItem(Paragraph & fromPar, pos_type fromPos,
	Paragraph & toPar, pos_type toPos, BufferParams const & params)
{
	// Note: moveItem() does not honour change tracking!
	// Therefore, it should only be used for breaking and merging paragraphs

	// We need a copy here because the character at fromPos is going to be erased.
	Font const tmpFont = fromPar.getFontSettings(params, fromPos);
	Change const tmpChange = fromPar.lookupChange(fromPos);

	if (Inset * tmpInset = fromPar.getInset(fromPos)) {
		fromPar.releaseInset(fromPos);
		// The inset is not in fromPar any more.
		if (!toPar.insertInset(toPos, tmpInset, tmpFont, tmpChange)) {
			delete tmpInset;
			return false;
		}
		return true;
	}

	char_type const tmpChar = fromPar.getChar(fromPos);
	fromPar.eraseChar(fromPos, false);
	toPar.insertChar(toPos, tmpChar, tmpFont, tmpChange);
	return true;
}


} //namespace


void breakParagraphConservative(BufferParams const & bparams,
	ParagraphList & pars, pit_type pit, pos_type pos)
{
	// create a new paragraph
	Paragraph & tmp = *pars.insert(pars.iterator_at(pit + 1), Paragraph());
	Paragraph & par = pars[pit];

	tmp.setInsetOwner(&par.inInset());
	tmp.makeSameLayout(par);

	LASSERT(pos <= par.size(), return);

	if (pos < par.size()) {
		// move everything behind the break position to the new paragraph
		pos_type pos_end = par.size() - 1;

		for (pos_type i = pos, j = 0; i <= pos_end; ++i) {
			if (moveItem(par, pos, tmp, j, bparams)) {
				++j;
			}
		}
		// Move over the end-of-par change information
		tmp.setChange(tmp.size(), par.lookupChange(par.size()));
		par.setChange(par.size(), Change(bparams.track_changes ?
					   Change::INSERTED : Change::UNCHANGED));
	}
}


void mergeParagraph(BufferParams const & bparams,
	ParagraphList & pars, pit_type par_offset)
{
	Paragraph & next = pars[par_offset + 1];
	Paragraph & par = pars[par_offset];

	pos_type pos_end = next.size() - 1;
	pos_type pos_insert = par.size();

	// the imaginary end-of-paragraph character (at par.size()) has to be
	// marked as unmodified. Otherwise, its change is adopted by the first
	// character of the next paragraph.
	if (par.isChanged(par.size())) {
		LYXERR(Debug::CHANGES,
		   "merging par with inserted/deleted end-of-par character");
		par.setChange(par.size(), Change(Change::UNCHANGED));
	}

	Change change = next.lookupChange(next.size());

	// move the content of the second paragraph to the end of the first one
	for (pos_type i = 0, j = pos_insert; i <= pos_end; ++i) {
		if (moveItem(next, 0, par, j, bparams)) {
			++j;
		}
	}

	// move the change of the end-of-paragraph character
	par.setChange(par.size(), change);

	pars.erase(pars.iterator_at(par_offset + 1));
}


Text::Text(InsetText * owner, bool use_default_layout)
	: owner_(owner)
{
	pars_.push_back(Paragraph());
	Paragraph & par = pars_.back();
	par.setInsetOwner(owner);
	DocumentClass const & dc = owner->buffer().params().documentClass();
	if (use_default_layout)
		par.setDefaultLayout(dc);
	else
		par.setPlainLayout(dc);
}


Text::Text(InsetText * owner, Text const & text)
	: owner_(owner), pars_(text.pars_)
{
	for (auto & p : pars_)
		p.setInsetOwner(owner);
}


pit_type Text::depthHook(pit_type pit, depth_type depth) const
{
	pit_type newpit = pit;

	if (newpit != 0)
		--newpit;

	while (newpit != 0 && pars_[newpit].getDepth() > depth)
		--newpit;

	if (pars_[newpit].getDepth() > depth)
		return pit;

	return newpit;
}


pit_type Text::outerHook(pit_type par_offset) const
{
	Paragraph const & par = pars_[par_offset];

	if (par.getDepth() == 0)
		return pars_.size();
	return depthHook(par_offset, par.getDepth() - 1);
}


bool Text::isFirstInSequence(pit_type par_offset) const
{
	Paragraph const & par = pars_[par_offset];

	pit_type dhook_offset = depthHook(par_offset, par.getDepth());

	if (dhook_offset == par_offset)
		return true;

	Paragraph const & dhook = pars_[dhook_offset];

	return dhook.layout() != par.layout()
		|| dhook.getDepth() != par.getDepth();
}


pit_type Text::lastInSequence(pit_type pit) const
{
	depth_type const depth = pars_[pit].getDepth();
	pit_type newpit = pit;

	while (size_t(newpit + 1) < pars_.size() &&
	       (pars_[newpit + 1].getDepth() > depth ||
	        (pars_[newpit + 1].getDepth() == depth &&
	         pars_[newpit + 1].layout() == pars_[pit].layout())))
		++newpit;

	return newpit;
}


int Text::getTocLevel(pit_type par_offset) const
{
	Paragraph const & par = pars_[par_offset];

	if (par.layout().isEnvironment() && !isFirstInSequence(par_offset))
		return Layout::NOT_IN_TOC;

	return par.layout().toclevel;
}


Font const Text::outerFont(pit_type par_offset) const
{
	depth_type par_depth = pars_[par_offset].getDepth();
	FontInfo tmpfont = inherit_font;
	depth_type prev_par_depth = 0;
	// Resolve against environment font information
	while (par_offset != pit_type(pars_.size())
	       && par_depth != prev_par_depth
	       && par_depth
	       && !tmpfont.resolved()) {
		prev_par_depth = par_depth;
		par_offset = outerHook(par_offset);
		if (par_offset != pit_type(pars_.size())) {
			tmpfont.realize(pars_[par_offset].layout().font);
			par_depth = pars_[par_offset].getDepth();
		}
	}

	return Font(tmpfont);
}


int Text::getEndLabel(pit_type p) const
{
	pit_type pit = p;
	depth_type par_depth = pars_[p].getDepth();
	while (pit != pit_type(pars_.size())) {
		Layout const & layout = pars_[pit].layout();
		int const endlabeltype = layout.endlabeltype;

		if (endlabeltype != END_LABEL_NO_LABEL) {
			if (p + 1 == pit_type(pars_.size()))
				return endlabeltype;

			depth_type const next_depth =
				pars_[p + 1].getDepth();
			if (par_depth > next_depth ||
			    (par_depth == next_depth && layout != pars_[p + 1].layout()))
				return endlabeltype;
			break;
		}
		if (par_depth == 0)
			break;
		pit = outerHook(pit);
		if (pit != pit_type(pars_.size()))
			par_depth = pars_[pit].getDepth();
	}
	return END_LABEL_NO_LABEL;
}


static void acceptOrRejectChanges(ParagraphList & pars,
	BufferParams const & bparams, Text::ChangeOp op)
{
	pit_type pars_size = static_cast<pit_type>(pars.size());

	// first, accept or reject changes within each individual
	// paragraph (do not consider end-of-par)
	for (pit_type pit = 0; pit < pars_size; ++pit) {
		// prevent assertion failure
		if (!pars[pit].empty()) {
			if (op == Text::ACCEPT)
				pars[pit].acceptChanges(0, pars[pit].size());
			else
				pars[pit].rejectChanges(0, pars[pit].size());
		}
	}

	// next, accept or reject imaginary end-of-par characters
	for (pit_type pit = 0; pit < pars_size; ++pit) {
		pos_type pos = pars[pit].size();
		if (pars[pit].isChanged(pos)) {
			// keep the end-of-par char if it is inserted and accepted
			// or when it is deleted and rejected.
			if (pars[pit].isInserted(pos) == (op == Text::ACCEPT)) {
				pars[pit].setChange(pos, Change(Change::UNCHANGED));
			} else {
				if (pit == pars_size - 1) {
					// we cannot remove a par break at the end of the last
					// paragraph; instead, we mark it unchanged
					pars[pit].setChange(pos, Change(Change::UNCHANGED));
				} else {
					mergeParagraph(bparams, pars, pit);
					--pit;
					--pars_size;
				}
			}
		}
	}
}


void acceptChanges(ParagraphList & pars, BufferParams const & bparams)
{
	acceptOrRejectChanges(pars, bparams, Text::ACCEPT);
}


void rejectChanges(ParagraphList & pars, BufferParams const & bparams)
{
	acceptOrRejectChanges(pars, bparams, Text::REJECT);
}


InsetText const & Text::inset() const
{
	return *owner_;
}



void Text::readParToken(Paragraph & par, Lexer & lex,
	string const & token, Font & font, Change & change, ErrorList & errorList)
{
	Buffer * buf = &owner_->buffer();
	BufferParams & bp = buf->params();

	if (token[0] != '\\') {
		docstring dstr = lex.getDocString();
		par.appendString(dstr, font, change);

	} else if (token == "\\begin_layout") {
		lex.eatLine();
		docstring layoutname = lex.getDocString();

		font = Font(inherit_font, bp.language);
		change = Change(Change::UNCHANGED);

		DocumentClass const & tclass = bp.documentClass();

		if (layoutname.empty())
			layoutname = tclass.defaultLayoutName();

		if (owner_->forcePlainLayout()) {
			// in this case only the empty layout is allowed
			layoutname = tclass.plainLayoutName();
		} else if (par.usePlainLayout()) {
			// in this case, default layout maps to empty layout
			if (layoutname == tclass.defaultLayoutName())
				layoutname = tclass.plainLayoutName();
		} else {
			// otherwise, the empty layout maps to the default
			if (layoutname == tclass.plainLayoutName())
				layoutname = tclass.defaultLayoutName();
		}

		// When we apply an unknown layout to a document, we add this layout to the textclass
		// of this document. For example, when you apply class article to a beamer document,
		// all unknown layouts such as frame will be added to document class article so that
		// these layouts can keep their original names.
		bool const added_one = tclass.addLayoutIfNeeded(layoutname);
		if (added_one) {
			// Warn the user.
			docstring const s = bformat(_("Layout `%1$s' was not found."), layoutname);
			errorList.push_back(ErrorItem(_("Layout Not Found"), s,
			                              {par.id(), 0}, {par.id(), -1}));
		}

		par.setLayout(bp.documentClass()[layoutname]);

		// Test whether the layout is obsolete.
		Layout const & layout = par.layout();
		if (!layout.obsoleted_by().empty())
			par.setLayout(bp.documentClass()[layout.obsoleted_by()]);

		par.params().read(lex);

	} else if (token == "\\end_layout") {
		LYXERR0("Solitary \\end_layout in line " << lex.lineNumber() << "\n"
		       << "Missing \\begin_layout ?");
	} else if (token == "\\end_inset") {
		LYXERR0("Solitary \\end_inset in line " << lex.lineNumber() << "\n"
		       << "Missing \\begin_inset ?");
	} else if (token == "\\begin_inset") {
		Inset * inset = readInset(lex, buf);
		if (inset)
			par.insertInset(par.size(), inset, font, change);
		else {
			lex.eatLine();
			docstring line = lex.getDocString();
			errorList.push_back(ErrorItem(_("Unknown Inset"), line,
			                              {par.id(), 0}, {par.id(), -1}));
		}
	} else if (token == "\\family") {
		lex.next();
		setLyXFamily(lex.getString(), font.fontInfo());
	} else if (token == "\\series") {
		lex.next();
		setLyXSeries(lex.getString(), font.fontInfo());
	} else if (token == "\\shape") {
		lex.next();
		setLyXShape(lex.getString(), font.fontInfo());
	} else if (token == "\\size") {
		lex.next();
		setLyXSize(lex.getString(), font.fontInfo());
	} else if (token == "\\lang") {
		lex.next();
		string const tok = lex.getString();
		Language const * lang = languages.getLanguage(tok);
		if (lang) {
			font.setLanguage(lang);
		} else {
			font.setLanguage(bp.language);
			lex.printError("Unknown language `$$Token'");
		}
	} else if (token == "\\numeric") {
		lex.next();
		font.fontInfo().setNumber(setLyXMisc(lex.getString()));
	} else if (token == "\\nospellcheck") {
		lex.next();
		font.fontInfo().setNoSpellcheck(setLyXMisc(lex.getString()));
	} else if (token == "\\emph") {
		lex.next();
		font.fontInfo().setEmph(setLyXMisc(lex.getString()));
	} else if (token == "\\bar") {
		lex.next();
		string const tok = lex.getString();

		if (tok == "under")
			font.fontInfo().setUnderbar(FONT_ON);
		else if (tok == "no")
			font.fontInfo().setUnderbar(FONT_OFF);
		else if (tok == "default")
			font.fontInfo().setUnderbar(FONT_INHERIT);
		else
			lex.printError("Unknown bar font flag "
				       "`$$Token'");
	} else if (token == "\\strikeout") {
		lex.next();
		font.fontInfo().setStrikeout(setLyXMisc(lex.getString()));
	} else if (token == "\\xout") {
		lex.next();
		font.fontInfo().setXout(setLyXMisc(lex.getString()));
	} else if (token == "\\uuline") {
		lex.next();
		font.fontInfo().setUuline(setLyXMisc(lex.getString()));
	} else if (token == "\\uwave") {
		lex.next();
		font.fontInfo().setUwave(setLyXMisc(lex.getString()));
	} else if (token == "\\noun") {
		lex.next();
		font.fontInfo().setNoun(setLyXMisc(lex.getString()));
	} else if (token == "\\color") {
		lex.next();
		setLyXColor(lex.getString(), font.fontInfo());
	} else if (token == "\\SpecialChar" ||
	           (token == "\\SpecialCharNoPassThru" &&
	            !par.layout().pass_thru && !inset().isPassThru())) {
		auto inset = make_unique<InsetSpecialChar>();
		inset->read(lex);
		inset->setBuffer(*buf);
		par.insertInset(par.size(), inset.release(), font, change);
	} else if (token == "\\SpecialCharNoPassThru") {
		lex.next();
		docstring const s = ltrim(lex.getDocString(), "\\");
		par.insert(par.size(), s, font, change);
	} else if (token == "\\IPAChar") {
		auto inset = make_unique<InsetIPAChar>();
		inset->read(lex);
		inset->setBuffer(*buf);
		par.insertInset(par.size(), inset.release(), font, change);
	} else if (token == "\\twohyphens" || token == "\\threehyphens") {
		// Ideally, this should be done by lyx2lyx, but lyx2lyx does not know the
		// running font and does not know anything about layouts (and CopyStyle).
		Layout const & layout(par.layout());
		FontInfo info = font.fontInfo();
		info.realize(layout.resfont);
		if (layout.pass_thru || inset().isPassThru() ||
		    info.family() == TYPEWRITER_FAMILY) {
			if (token == "\\twohyphens")
				par.insert(par.size(), from_ascii("--"), font, change);
			else
				par.insert(par.size(), from_ascii("---"), font, change);
		} else {
			if (token == "\\twohyphens")
				par.insertChar(par.size(), 0x2013, font, change);
			else
				par.insertChar(par.size(), 0x2014, font, change);
		}
	} else if (token == "\\backslash") {
		par.appendChar('\\', font, change);
	} else if (token == "\\LyXTable") {
		auto inset = make_unique<InsetTabular>(buf);
		inset->read(lex);
		par.insertInset(par.size(), inset.release(), font, change);
	} else if (token == "\\change_unchanged") {
		change = Change(Change::UNCHANGED);
	} else if (token == "\\change_inserted" || token == "\\change_deleted") {
		lex.eatLine();
		istringstream is(lex.getString());
		int aid;
		time_t ct;
		is >> aid >> ct;
		BufferParams::AuthorMap const & am = bp.author_map_;
		if (am.find(aid) == am.end()) {
			errorList.push_back(ErrorItem(
				_("Change tracking author index missing"),
				bformat(_("A change tracking author information for index "
				          "%1$d is missing. This can happen after a wrong "
				          "merge by a version control system. In this case, "
				          "either fix the merge, or have this information "
				          "missing until the corresponding tracked changes "
				          "are merged or this user edits the file again.\n"),
				        aid),
				{par.id(), par.size()}, {par.id(), par.size() + 1}));
			bp.addAuthor(Author(aid));
		}
		if (token == "\\change_inserted")
			change = Change(Change::INSERTED, am.find(aid)->second, ct);
		else
			change = Change(Change::DELETED, am.find(aid)->second, ct);
	} else {
		lex.eatLine();
		errorList.push_back(ErrorItem(_("Unknown token"),
		                              bformat(_("Unknown token: %1$s %2$s\n"),
		                                      from_utf8(token),
		                                      lex.getDocString()),
		                              {par.id(), 0}, {par.id(), -1}));
	}
}


void Text::readParagraph(Paragraph & par, Lexer & lex,
	ErrorList & errorList)
{
	lex.nextToken();
	string token = lex.getString();
	Font font;
	Change change(Change::UNCHANGED);

	while (lex.isOK()) {
		readParToken(par, lex, token, font, change, errorList);

		lex.nextToken();
		token = lex.getString();

		if (token.empty())
			continue;

		if (token == "\\end_layout") {
			//Ok, paragraph finished
			break;
		}

		LYXERR(Debug::PARSER, "Handling paragraph token: `" << token << '\'');
		if (token == "\\begin_layout" || token == "\\end_document"
		    || token == "\\end_inset" || token == "\\begin_deeper"
		    || token == "\\end_deeper") {
			lex.pushToken(token);
			lyxerr << "Paragraph ended in line "
			       << lex.lineNumber() << "\n"
			       << "Missing \\end_layout.\n";
			break;
		}
	}
	// Final change goes to paragraph break:
	if (inset().allowMultiPar())
		par.setChange(par.size(), change);

	// Initialize begin_of_body_ on load; redoParagraph maintains
	par.setBeginOfBody();

	// mark paragraph for spell checking on load
	// par.requestSpellCheck();
}


class TextCompletionList : public CompletionList
{
public:
	///
	TextCompletionList(Cursor const & cur, WordList const & list)
		: buffer_(cur.buffer()), list_(list)
	{}
	///
	virtual ~TextCompletionList() {}

	///
	bool sorted() const override { return true; }
	///
	size_t size() const override
	{
		return list_.size();
	}
	///
	docstring const & data(size_t idx) const override
	{
		return list_.word(idx);
	}

private:
	///
	Buffer const * buffer_;
	///
	WordList const & list_;
};


bool Text::empty() const
{
	return pars_.empty() || (pars_.size() == 1 && pars_[0].empty()
		// FIXME: Should we consider the labeled type as empty too?
		&& pars_[0].layout().labeltype == LABEL_NO_LABEL);
}


double Text::spacing(Paragraph const & par) const
{
	if (par.params().spacing().isDefault())
		return owner_->buffer().params().spacing().getValue();
	return par.params().spacing().getValue();
}


/**
 * This breaks a paragraph at the specified position.
 * The new paragraph will:
 * - change layout to default layout when keep_layout == false
 * - keep layout when keep_layout == true
 */
static void breakParagraph(Text & text, pit_type par_offset, pos_type pos,
		    bool keep_layout)
{
	BufferParams const & bparams = text.inset().buffer().params();
	ParagraphList & pars = text.paragraphs();
	// create a new paragraph, and insert into the list
	ParagraphList::iterator tmp =
		pars.insert(pars.iterator_at(par_offset + 1), Paragraph());

	Paragraph & par = pars[par_offset];

	// remember to set the inset_owner
	tmp->setInsetOwner(&par.inInset());
	tmp->params().depth(par.params().depth());

	if (keep_layout) {
		tmp->setLayout(par.layout());
		tmp->setLabelWidthString(par.params().labelWidthString());
	} else
		tmp->setPlainOrDefaultLayout(bparams.documentClass());

	bool const isempty = (par.allowEmpty() && par.empty());

	if (!isempty && (par.size() > pos || par.empty())) {
		tmp->setLayout(par.layout());
		tmp->params().align(par.params().align());
		tmp->setLabelWidthString(par.params().labelWidthString());

		tmp->params().depth(par.params().depth());
		tmp->params().noindent(par.params().noindent());
		tmp->params().spacing(par.params().spacing());

		// move everything behind the break position
		// to the new paragraph

		/* Note: if !keepempty, empty() == true, then we reach
		 * here with size() == 0. So pos_end becomes - 1. This
		 * doesn't cause problems because both loops below
		 * enforce pos <= pos_end and 0 <= pos
		 */
		pos_type pos_end = par.size() - 1;

		for (pos_type i = pos, j = 0; i <= pos_end; ++i) {
			if (moveItem(par, pos, *tmp, j, bparams)) {
				++j;
			}
		}
	}

	// Move over the end-of-par change information
	tmp->setChange(tmp->size(), par.lookupChange(par.size()));
	par.setChange(par.size(), Change(bparams.track_changes ?
					   Change::INSERTED : Change::UNCHANGED));

	if (pos) {
		// Make sure that we keep the language when
		// breaking paragraph.
		if (tmp->empty()) {
			Font changed = tmp->getFirstFontSettings(bparams);
			Font const & old = par.getFontSettings(bparams, par.size());
			changed.setLanguage(old.language());
			tmp->setFont(0, changed);
		}

		return;
	}

	if (!isempty) {
		bool const soa = par.params().startOfAppendix();
		par.params().clear();
		// do not lose start of appendix marker (bug 4212)
		par.params().startOfAppendix(soa);
		par.setPlainOrDefaultLayout(bparams.documentClass());
	}

	if (keep_layout) {
		par.setLayout(tmp->layout());
		par.setLabelWidthString(tmp->params().labelWidthString());
		par.params().depth(tmp->params().depth());
	}
}


void Text::breakParagraph(Cursor & cur, bool inverse_logic)
{
	LBUFERR(this == cur.text());

	Paragraph & cpar = cur.paragraph();
	pit_type cpit = cur.pit();

	DocumentClass const & tclass = cur.buffer()->params().documentClass();
	Layout const & layout = cpar.layout();

	if (cur.lastpos() == 0 && !cpar.allowEmpty()) {
		if (changeDepthAllowed(cur, DEC_DEPTH)) {
			changeDepth(cur, DEC_DEPTH);
			pit_type const prev = depthHook(cpit, cpar.getDepth());
			docstring const & lay = pars_[prev].layout().name();
			if (lay != layout.name())
				setLayout(cur, lay);
		} else {
			docstring const & lay = cur.paragraph().usePlainLayout()
			    ? tclass.plainLayoutName() : tclass.defaultLayoutName();
			if (lay != layout.name())
				setLayout(cur, lay);
		}
		return;
	}

	cur.recordUndo();

	// Always break behind a space
	// It is better to erase the space (Dekel)
	if (cur.pos() != cur.lastpos() && cpar.isLineSeparator(cur.pos()))
		cpar.eraseChar(cur.pos(), cur.buffer()->params().track_changes);

	// What should the layout for the new paragraph be?
	bool keep_layout = layout.isEnvironment()
		|| (layout.isParagraph() && layout.parbreak_is_newline);
	if (inverse_logic)
		keep_layout = !keep_layout;

	// We need to remember this before we break the paragraph, because
	// that invalidates the layout variable
	bool sensitive = layout.labeltype == LABEL_SENSITIVE;

	// we need to set this before we insert the paragraph.
	bool const isempty = cpar.allowEmpty() && cpar.empty();

	lyx::breakParagraph(*this, cpit, cur.pos(), keep_layout);

	// After this, neither paragraph contains any rows!

	cpit = cur.pit();
	pit_type next_par = cpit + 1;

	// well this is the caption hack since one caption is really enough
	if (sensitive) {
		if (cur.pos() == 0)
			// set to standard-layout
		//FIXME Check if this should be plainLayout() in some cases
			pars_[cpit].applyLayout(tclass.defaultLayout());
		else
			// set to standard-layout
			//FIXME Check if this should be plainLayout() in some cases
			pars_[next_par].applyLayout(tclass.defaultLayout());
	}

	while (!pars_[next_par].empty() && pars_[next_par].isNewline(0)) {
		if (!pars_[next_par].eraseChar(0, cur.buffer()->params().track_changes))
			break; // the character couldn't be deleted physically due to change tracking
	}

	// A singlePar update is not enough in this case.
	cur.screenUpdateFlags(Update::Force);
	cur.forceBufferUpdate();

	// This check is necessary. Otherwise the new empty paragraph will
	// be deleted automatically. And it is more friendly for the user!
	if (cur.pos() != 0 || isempty)
		setCursor(cur, cur.pit() + 1, 0);
	else
		setCursor(cur, cur.pit(), 0);
}


// needed to insert the selection
void Text::insertStringAsLines(Cursor & cur, docstring const & str,
		Font const & font)
{
	BufferParams const & bparams = owner_->buffer().params();
	pit_type pit = cur.pit();
	pos_type pos = cur.pos();

	// The special chars we handle
	static map<wchar_t, InsetSpecialChar::Kind> specialchars = {
		{ 0x200c, InsetSpecialChar::LIGATURE_BREAK },
		{ 0x200b, InsetSpecialChar::ALLOWBREAK },
		{ 0x2026, InsetSpecialChar::LDOTS },
		{ 0x2011, InsetSpecialChar::NOBREAKDASH }
	};

	// insert the string, don't insert doublespace
	bool space_inserted = true;
	for (auto const & ch : str) {
		Paragraph & par = pars_[pit];
		if (ch == '\n') {
			if (inset().allowMultiPar() && (!par.empty() || par.allowEmpty())) {
				lyx::breakParagraph(*this, pit, pos,
					par.layout().isEnvironment());
				++pit;
				pos = 0;
				space_inserted = true;
			} else {
				continue;
			}
		// do not insert consecutive spaces if !free_spacing
		} else if ((ch == ' ' || ch == '\t') &&
			   space_inserted && !par.isFreeSpacing()) {
			continue;
		} else if (ch == '\t') {
			if (!par.isFreeSpacing()) {
				// tabs are like spaces here
				par.insertChar(pos, ' ', font, bparams.track_changes);
				++pos;
				space_inserted = true;
			} else {
				par.insertChar(pos, ch, font, bparams.track_changes);
				++pos;
				space_inserted = true;
			}
		} else if (specialchars.find(ch) != specialchars.end()
			   && (par.insertInset(pos, new InsetSpecialChar(specialchars.find(ch)->second),
					       font, bparams.track_changes
					       ? Change(Change::INSERTED)
					       : Change(Change::UNCHANGED)))) {
			++pos;
			space_inserted = false;
		} else if (!isPrintable(ch)) {
			// Ignore (other) unprintables
			continue;
		} else {
			// just insert the character
			par.insertChar(pos, ch, font, bparams.track_changes);
			++pos;
			space_inserted = (ch == ' ');
		}
	}
	setCursor(cur, pit, pos);
}


// turn double CR to single CR, others are converted into one
// blank. Then insertStringAsLines is called
void Text::insertStringAsParagraphs(Cursor & cur, docstring const & str,
		Font const & font)
{
	docstring linestr = str;
	bool newline_inserted = false;

	for (string::size_type i = 0, siz = linestr.size(); i < siz; ++i) {
		if (linestr[i] == '\n') {
			if (newline_inserted) {
				// we know that \r will be ignored by
				// insertStringAsLines. Of course, it is a dirty
				// trick, but it works...
				linestr[i - 1] = '\r';
				linestr[i] = '\n';
			} else {
				linestr[i] = ' ';
				newline_inserted = true;
			}
		} else if (isPrintable(linestr[i])) {
			newline_inserted = false;
		}
	}
	insertStringAsLines(cur, linestr, font);
}


namespace {

bool canInsertChar(Cursor const & cur, char_type c)
{
	Paragraph const & par = cur.paragraph();
	// If not in free spacing mode, check if there will be two blanks together or a blank at
	// the beginning of a paragraph.
	if (!par.isFreeSpacing() && isLineSeparatorChar(c)) {
		if (cur.pos() == 0) {
			cur.message(_(
					"You cannot insert a space at the "
					"beginning of a paragraph. Please read the Tutorial."));
			return false;
		}
		// If something is wrong, ignore this character.
		LASSERT(cur.pos() > 0, return false);
		if ((par.isLineSeparator(cur.pos() - 1) || par.isNewline(cur.pos() - 1))
				&& !par.isDeleted(cur.pos() - 1)) {
			cur.message(_(
					"You cannot type two spaces this way. "
					"Please read the Tutorial."));
			return false;
		}
	}

	// Prevent to insert uncodable characters in verbatim and ERT.
	// The encoding is inherited from the context here.
	if (par.isPassThru() && cur.getEncoding()) {
		Encoding const * e = cur.getEncoding();
		if (!e->encodable(c)) {
			cur.message(_("Character is uncodable in this verbatim context."));
			return false;
		}
	}
	return true;
}

} // namespace


// insert a character, moves all the following breaks in the
// same Paragraph one to the right and make a rebreak
void Text::insertChar(Cursor & cur, char_type c)
{
	LBUFERR(this == cur.text());

	if (!canInsertChar(cur,c))
		return;

	cur.recordUndo(INSERT_UNDO);

	TextMetrics const & tm = cur.bv().textMetrics(this);
	Buffer const & buffer = *cur.buffer();
	Paragraph & par = cur.paragraph();
	// try to remove this
	pit_type const pit = cur.pit();

	if (lyxrc.auto_number) {
		static docstring const number_operators = from_ascii("+-/*");
		static docstring const number_unary_operators = from_ascii("+-");

		// Common Number Separators: comma, dot etc.
		// European Number Terminators: percent, permille, degree, euro etc.
		if (cur.current_font.fontInfo().number() == FONT_ON) {
			if (!isDigitASCII(c) && !contains(number_operators, c) &&
			    !(isCommonNumberSeparator(c) &&
			      cur.pos() != 0 &&
			      cur.pos() != cur.lastpos() &&
			      tm.displayFont(pit, cur.pos()).fontInfo().number() == FONT_ON &&
			      tm.displayFont(pit, cur.pos() - 1).fontInfo().number() == FONT_ON) &&
			    !(isEuropeanNumberTerminator(c) &&
			      cur.pos() != 0 &&
			      tm.displayFont(pit, cur.pos()).fontInfo().number() == FONT_ON &&
			      tm.displayFont(pit, cur.pos() - 1).fontInfo().number() == FONT_ON)
			   )
				number(cur); // Set current_font.number to OFF
		} else if (isDigitASCII(c) &&
			   cur.real_current_font.isVisibleRightToLeft()) {
			number(cur); // Set current_font.number to ON

			if (cur.pos() != 0) {
				char_type const ch = par.getChar(cur.pos() - 1);
				if (contains(number_unary_operators, ch) &&
				    (cur.pos() == 1
				     || par.isSeparator(cur.pos() - 2)
				     || par.isEnvSeparator(cur.pos() - 2)
				     || par.isNewline(cur.pos() - 2))
				  ) {
					setCharFont(pit, cur.pos() - 1, cur.current_font,
						tm.font_);
				} else if (isCommonNumberSeparator(ch)
				     && cur.pos() >= 2
				     && tm.displayFont(pit, cur.pos() - 2).fontInfo().number() == FONT_ON) {
					setCharFont(pit, cur.pos() - 1, cur.current_font,
						tm.font_);
				}
			}
		}
	}

	// In Bidi text, we want spaces to be treated in a special way: spaces
	// which are between words in different languages should get the
	// paragraph's language; otherwise, spaces should keep the language
	// they were originally typed in. This is only in effect while typing;
	// after the text is already typed in, the user can always go back and
	// explicitly set the language of a space as desired. But 99.9% of the
	// time, what we're doing here is what the user actually meant.
	//
	// The following cases are the ones in which the language of the space
	// should be changed to match that of the containing paragraph. In the
	// depictions, lowercase is LTR, uppercase is RTL, underscore (_)
	// represents a space, pipe (|) represents the cursor position (so the
	// character before it is the one just typed in). The different cases
	// are depicted logically (not visually), from left to right:
	//
	// 1. A_a|
	// 2. a_A|
	//
	// Theoretically, there are other situations that we should, perhaps, deal
	// with (e.g.: a|_A, A|_a). In practice, though, there really isn't any
	// point (to understand why, just try to create this situation...).

	if ((cur.pos() >= 2) && (par.isLineSeparator(cur.pos() - 1))) {
		// get font in front and behind the space in question. But do NOT
		// use getFont(cur.pos()) because the character c is not inserted yet
		Font const pre_space_font  = tm.displayFont(cur.pit(), cur.pos() - 2);
		Font const & post_space_font = cur.real_current_font;
		bool pre_space_rtl  = pre_space_font.isVisibleRightToLeft();
		bool post_space_rtl = post_space_font.isVisibleRightToLeft();

		if (pre_space_rtl != post_space_rtl) {
			// Set the space's language to match the language of the
			// adjacent character whose direction is the paragraph's
			// direction; don't touch other properties of the font
			Language const * lang =
				(pre_space_rtl == par.isRTL(buffer.params())) ?
				pre_space_font.language() : post_space_font.language();

			Font space_font = tm.displayFont(cur.pit(), cur.pos() - 1);
			space_font.setLanguage(lang);
			par.setFont(cur.pos() - 1, space_font);
		}
	}

	pos_type pos = cur.pos();
	if (!cur.paragraph().isPassThru() && owner_->lyxCode() != IPA_CODE &&
	    cur.real_current_font.fontInfo().family() != TYPEWRITER_FAMILY &&
	    c == '-' && pos > 0) {
		pos_type prev_pos = pos - 1;
		while (prev_pos > 0 && par.isDeleted(prev_pos))
			--prev_pos;
		if (!par.isDeleted(prev_pos) && par.getChar(prev_pos) == '-') {
			// convert "--" to endash
			par.eraseChar(prev_pos, cur.buffer()->params().track_changes);
			c = 0x2013;
			pos--;
		} else if (!par.isDeleted(prev_pos) && par.getChar(prev_pos) == 0x2013) {
			// convert "---" to emdash
			par.eraseChar(pos - 1, cur.buffer()->params().track_changes);
			c = 0x2014;
			pos--;
		}
	}

	par.insertChar(pos, c, cur.current_font,
		cur.buffer()->params().track_changes);
	cur.checkBufferStructure();

//		cur.screenUpdateFlags(Update::Force);
	bool boundary = cur.boundary()
		|| tm.isRTLBoundary(cur.pit(), pos + 1);
	setCursor(cur, cur.pit(), pos + 1, false, boundary);
	charInserted(cur);
}


void Text::charInserted(Cursor & cur)
{
	Paragraph & par = cur.paragraph();

	// register word if a non-letter was entered
	if (cur.pos() > 1
	    && !par.isWordSeparator(cur.pos() - 2)
	    && par.isWordSeparator(cur.pos() - 1)) {
		// get the word in front of cursor
		LBUFERR(this == cur.text());
		par.updateWords();
	}
}


// the cursor set functions have a special mechanism. When they
// realize, that you left an empty paragraph, they will delete it.

bool Text::cursorForwardOneWord(Cursor & cur)
{
	LBUFERR(this == cur.text());

	if (lyxrc.mac_like_cursor_movement) {
		DocIterator dit(cur);
		DocIterator prv(cur);
		bool inword = false;
		bool intext = dit.inTexted();
		while (!dit.atEnd()) {
			if (dit.inTexted()) { // no paragraphs in mathed
				Paragraph const & par = dit.paragraph();
				pos_type const pos = dit.pos();

				if (!par.isDeleted(pos)) {
					bool wordsep = par.isWordSeparator(pos);
					if (inword && wordsep)
						break; // stop at word end
					else if (!inword && !wordsep)
						inword = true;
				}
				intext = true;
			} else if (intext) {
				// move to end of math
				while (!dit.inTexted() && !dit.atEnd()) dit.forwardPos();
				break;
			}
			prv = dit;
			dit.forwardPosIgnoreCollapsed();
		}
		if (dit.atEnd()) dit = prv;
		if (dit == cur) return false; // we didn't move
		Cursor orig(cur);
		cur.setCursor(dit);
		// see comment above
		cur.bv().checkDepm(cur, orig);
		return true;
	} else {
		pos_type const lastpos = cur.lastpos();
		pit_type pit = cur.pit();
		pos_type pos = cur.pos();
		Paragraph const & par = cur.paragraph();

		// Paragraph boundary is a word boundary
		if (pos == lastpos || (pos + 1 == lastpos && par.isEnvSeparator(pos))) {
			if (pit != cur.lastpit())
				return setCursor(cur, pit + 1, 0);
			else
				return false;
		}

		LASSERT(pos < lastpos, return false); // see above
		if (!par.isWordSeparator(pos))
			while (pos != lastpos && !par.isWordSeparator(pos))
				++pos;
		else if (par.isChar(pos))
			while (pos != lastpos && par.isChar(pos))
				++pos;
		else if (!par.isSpace(pos)) // non-char inset
			++pos;

		// Skip over white space
		while (pos != lastpos && par.isSpace(pos))
			     ++pos;

		// Don't skip a separator inset at the end of a paragraph
		if (pos == lastpos && pos && par.isEnvSeparator(pos - 1))
			--pos;

		return setCursor(cur, pit, pos);
	}
}


bool Text::cursorBackwardOneWord(Cursor & cur)
{
	LBUFERR(this == cur.text());

	if (lyxrc.mac_like_cursor_movement) {
		DocIterator dit(cur);
		bool inword = false;
		bool intext = dit.inTexted();
		while (!dit.atBegin()) {
			DocIterator prv(dit);
			dit.backwardPosIgnoreCollapsed();
			if (dit.inTexted()) { // no paragraphs in mathed
				Paragraph const & par = dit.paragraph();
				pos_type pos = dit.pos();

				if (!par.isDeleted(pos)) {
					bool wordsep = par.isWordSeparator(pos);
					if (inword && wordsep) {
						dit = prv;
						break; // stop at word begin
					} else if (!inword && !wordsep)
						inword = true;
				}
				intext = true;
			} else if (intext) {
				// move to begin of math
				while (!dit.inTexted() && !dit.atBegin()) dit.backwardPos();
				break;
			}
		}
		if (dit == cur) return false; // we didn't move
		Cursor orig(cur);
		cur.setCursor(dit);
		// see comment above cursorForwardOneWord
		cur.bv().checkDepm(cur, orig);
		return true;
	} else {
		Paragraph const & par = cur.paragraph();
		pit_type const pit = cur.pit();
		pos_type pos = cur.pos();

		// Paragraph boundary is a word boundary
		if (pos == 0 && pit != 0) {
			Paragraph & prevpar = getPar(pit - 1);
			pos = prevpar.size();
			// Don't stop after an environment separator
			if (pos && prevpar.isEnvSeparator(pos - 1))
				--pos;
			return setCursor(cur, pit - 1, pos);
		}
		// Skip over white space
		while (pos != 0 && par.isSpace(pos - 1))
			--pos;

		if (pos != 0 && !par.isWordSeparator(pos - 1))
			while (pos != 0 && !par.isWordSeparator(pos - 1))
				--pos;
		else if (pos != 0 && par.isChar(pos - 1))
			while (pos != 0 && par.isChar(pos - 1))
				--pos;
		else if (pos != 0 && !par.isSpace(pos - 1)) // non-char inset
			--pos;

		return setCursor(cur, pit, pos);
	}
}


bool Text::cursorVisLeftOneWord(Cursor & cur)
{
	LBUFERR(this == cur.text());

	pos_type left_pos, right_pos;

	Cursor temp_cur = cur;

	// always try to move at least once...
	while (temp_cur.posVisLeft(true /* skip_inset */)) {

		// collect some information about current cursor position
		temp_cur.getSurroundingPos(left_pos, right_pos);
		bool left_is_letter =
			(left_pos > -1 ? !temp_cur.paragraph().isWordSeparator(left_pos) : false);
		bool right_is_letter =
			(right_pos > -1 ? !temp_cur.paragraph().isWordSeparator(right_pos) : false);

		// if we're not at a letter/non-letter boundary, continue moving
		if (left_is_letter == right_is_letter)
			continue;

		// we should stop when we have an LTR word on our right or an RTL word
		// on our left
		if ((left_is_letter && temp_cur.paragraph().getFontSettings(
				temp_cur.buffer()->params(), left_pos).isRightToLeft())
			|| (right_is_letter && !temp_cur.paragraph().getFontSettings(
				temp_cur.buffer()->params(), right_pos).isRightToLeft()))
			break;
	}

	return setCursor(cur, temp_cur.pit(), temp_cur.pos(),
					 true, temp_cur.boundary());
}


bool Text::cursorVisRightOneWord(Cursor & cur)
{
	LBUFERR(this == cur.text());

	pos_type left_pos, right_pos;

	Cursor temp_cur = cur;

	// always try to move at least once...
	while (temp_cur.posVisRight(true /* skip_inset */)) {

		// collect some information about current cursor position
		temp_cur.getSurroundingPos(left_pos, right_pos);
		bool left_is_letter =
			(left_pos > -1 ? !temp_cur.paragraph().isWordSeparator(left_pos) : false);
		bool right_is_letter =
			(right_pos > -1 ? !temp_cur.paragraph().isWordSeparator(right_pos) : false);

		// if we're not at a letter/non-letter boundary, continue moving
		if (left_is_letter == right_is_letter)
			continue;

		// we should stop when we have an LTR word on our right or an RTL word
		// on our left
		if ((left_is_letter && temp_cur.paragraph().getFontSettings(
				temp_cur.buffer()->params(),
				left_pos).isRightToLeft())
			|| (right_is_letter && !temp_cur.paragraph().getFontSettings(
				temp_cur.buffer()->params(),
				right_pos).isRightToLeft()))
			break;
	}

	return setCursor(cur, temp_cur.pit(), temp_cur.pos(),
					 true, temp_cur.boundary());
}


void Text::selectWord(Cursor & cur, word_location loc)
{
	LBUFERR(this == cur.text());
	CursorSlice from = cur.top();
	CursorSlice to;
	getWord(from, to, loc);
	if (cur.top() != from)
		setCursor(cur, from.pit(), from.pos());
	if (to == from)
		return;
	if (!cur.selection())
		cur.resetAnchor();
	setCursor(cur, to.pit(), to.pos());
	cur.setSelection();
	cur.setWordSelection(true);
}


void Text::expandWordSel(Cursor & cur)
{
	// get selection of word around cur
	Cursor c = cur;
	c.selection(false);
	c.text()->selectWord(c, WHOLE_WORD);
	// get selection around anchor too.
	// FIXME: this cursor is not a proper one. normalAnchor() should
	// return a DocIterator.
	Cursor a(cur.bv());
	a.push_back(cur.normalAnchor());
	a.text()->selectWord(a, WHOLE_WORD);
	// use the correct word boundary, depending on selection direction
	if (cur.top() > cur.normalAnchor()) {
		cur.top() = a.selBegin();
		cur.resetAnchor();
		cur.top() = c.selEnd();
	} else {
		cur.top() = a.selEnd();
		cur.resetAnchor();
		cur.top() = c.selBegin();
	}
}


void Text::selectAll(Cursor & cur)
{
	LBUFERR(this == cur.text());
	if (cur.lastpos() == 0 && cur.lastpit() == 0)
		return;
	// If the cursor is at the beginning, make sure the cursor ends there
	if (cur.pit() == 0 && cur.pos() == 0) {
		setCursor(cur, cur.lastpit(), getPar(cur.lastpit()).size());
		cur.resetAnchor();
		setCursor(cur, 0, 0);
	} else {
		setCursor(cur, 0, 0);
		cur.resetAnchor();
		setCursor(cur, cur.lastpit(), getPar(cur.lastpit()).size());
	}
	cur.setSelection();
}


// Select the word currently under the cursor when no
// selection is currently set
bool Text::selectWordWhenUnderCursor(Cursor & cur, word_location loc)
{
	LBUFERR(this == cur.text());
	if (cur.selection())
		return false;
	selectWord(cur, loc);
	return cur.selection();
}


void Text::acceptOrRejectChanges(Cursor & cur, ChangeOp op)
{
	LBUFERR(this == cur.text());

	if (!cur.selection()) {
		if (!selectChange(cur))
			return;
	}

	cur.recordUndoSelection();

	pit_type begPit = cur.selectionBegin().pit();
	pit_type endPit = cur.selectionEnd().pit();

	pos_type begPos = cur.selectionBegin().pos();
	pos_type endPos = cur.selectionEnd().pos();

	// keep selection info, because endPos becomes invalid after the first loop
	bool const endsBeforeEndOfPar = (endPos < pars_[endPit].size());

	// first, accept/reject changes within each individual paragraph (do not consider end-of-par)
	for (pit_type pit = begPit; pit <= endPit; ++pit) {
		pos_type parSize = pars_[pit].size();

		// ignore empty paragraphs; otherwise, an assertion will fail for
		// acceptChanges(bparams, 0, 0) or rejectChanges(bparams, 0, 0)
		if (parSize == 0)
			continue;

		// do not consider first paragraph if the cursor starts at pos size()
		if (pit == begPit && begPos == parSize)
			continue;

		// do not consider last paragraph if the cursor ends at pos 0
		if (pit == endPit && endPos == 0)
			break; // last iteration anyway

		pos_type const left  = (pit == begPit ? begPos : 0);
		pos_type const right = (pit == endPit ? endPos : parSize);

		if (left == right)
			// there is no change here
			continue;

		if (op == ACCEPT) {
			pars_[pit].acceptChanges(left, right);
		} else {
			pars_[pit].rejectChanges(left, right);
		}
	}

	// next, accept/reject imaginary end-of-par characters

	for (pit_type pit = begPit; pit <= endPit; ++pit) {
		pos_type pos = pars_[pit].size();

		// skip if the selection ends before the end-of-par
		if (pit == endPit && endsBeforeEndOfPar)
			break; // last iteration anyway

		// skip if this is not the last paragraph of the document
		// note: the user should be able to accept/reject the par break of the last par!
		if (pit == endPit && pit + 1 != int(pars_.size()))
			break; // last iteration anway

		if (op == ACCEPT) {
			if (pars_[pit].isInserted(pos)) {
				pars_[pit].setChange(pos, Change(Change::UNCHANGED));
			} else if (pars_[pit].isDeleted(pos)) {
				if (pit + 1 == int(pars_.size())) {
					// we cannot remove a par break at the end of the last paragraph;
					// instead, we mark it unchanged
					pars_[pit].setChange(pos, Change(Change::UNCHANGED));
				} else {
					mergeParagraph(cur.buffer()->params(), pars_, pit);
					--endPit;
					--pit;
				}
			}
		} else {
			if (pars_[pit].isDeleted(pos)) {
				pars_[pit].setChange(pos, Change(Change::UNCHANGED));
			} else if (pars_[pit].isInserted(pos)) {
				if (pit + 1 == int(pars_.size())) {
					// we mark the par break at the end of the last paragraph unchanged
					pars_[pit].setChange(pos, Change(Change::UNCHANGED));
				} else {
					mergeParagraph(cur.buffer()->params(), pars_, pit);
					--endPit;
					--pit;
				}
			}
		}
	}

	// finally, invoke the DEPM
	deleteEmptyParagraphMechanism(begPit, endPit, begPos, endPos,
				      cur.buffer()->params().track_changes);

	cur.finishUndo();
	cur.clearSelection();
	setCursorIntern(cur, begPit, begPos);
	cur.screenUpdateFlags(Update::Force);
	cur.forceBufferUpdate();
}


void Text::acceptChanges()
{
	BufferParams const & bparams = owner_->buffer().params();
	lyx::acceptChanges(pars_, bparams);
	deleteEmptyParagraphMechanism(0, pars_.size() - 1, bparams.track_changes);
}


void Text::rejectChanges()
{
	BufferParams const & bparams = owner_->buffer().params();
	pit_type pars_size = static_cast<pit_type>(pars_.size());

	// first, reject changes within each individual paragraph
	// (do not consider end-of-par)
	for (pit_type pit = 0; pit < pars_size; ++pit) {
		if (!pars_[pit].empty())   // prevent assertion failure
			pars_[pit].rejectChanges(0, pars_[pit].size());
	}

	// next, reject imaginary end-of-par characters
	for (pit_type pit = 0; pit < pars_size; ++pit) {
		pos_type pos = pars_[pit].size();

		if (pars_[pit].isDeleted(pos)) {
			pars_[pit].setChange(pos, Change(Change::UNCHANGED));
		} else if (pars_[pit].isInserted(pos)) {
			if (pit == pars_size - 1) {
				// we mark the par break at the end of the last
				// paragraph unchanged
				pars_[pit].setChange(pos, Change(Change::UNCHANGED));
			} else {
				mergeParagraph(bparams, pars_, pit);
				--pit;
				--pars_size;
			}
		}
	}

	// finally, invoke the DEPM
	deleteEmptyParagraphMechanism(0, pars_size - 1, bparams.track_changes);
}


void Text::deleteWordForward(Cursor & cur, bool const force)
{
	LBUFERR(this == cur.text());
	if (cur.lastpos() == 0)
		cursorForward(cur);
	else {
		cur.resetAnchor();
		cur.selection(true);
		cursorForwardOneWord(cur);
		cur.setSelection();
		if (force || !cur.confirmDeletion()) {
			cutSelection(cur, false);
			cur.checkBufferStructure();
		}
	}
}


void Text::deleteWordBackward(Cursor & cur, bool const force)
{
	LBUFERR(this == cur.text());
	if (cur.lastpos() == 0)
		cursorBackward(cur);
	else {
		cur.resetAnchor();
		cur.selection(true);
		cursorBackwardOneWord(cur);
		cur.setSelection();
		if (force || !cur.confirmDeletion()) {
			cutSelection(cur, false);
			cur.checkBufferStructure();
		}
	}
}


// Kill to end of line.
void Text::changeCase(Cursor & cur, TextCase action, bool partial)
{
	LBUFERR(this == cur.text());
	CursorSlice from;
	CursorSlice to;

	bool const gotsel = cur.selection();
	if (gotsel) {
		from = cur.selBegin();
		to = cur.selEnd();
	} else {
		from = cur.top();
		getWord(from, to, partial ? PARTIAL_WORD : WHOLE_WORD);
		cursorForwardOneWord(cur);
	}

	cur.recordUndoSelection();

	pit_type begPit = from.pit();
	pit_type endPit = to.pit();

	pos_type begPos = from.pos();
	pos_type endPos = to.pos();

	pos_type right = 0; // needed after the for loop

	for (pit_type pit = begPit; pit <= endPit; ++pit) {
		Paragraph & par = pars_[pit];
		pos_type const pos = (pit == begPit ? begPos : 0);
		right = (pit == endPit ? endPos : par.size());
		par.changeCase(cur.buffer()->params(), pos, right, action);
	}

	// the selection may have changed due to logically-only deleted chars
	if (gotsel) {
		setCursor(cur, begPit, begPos);
		cur.resetAnchor();
		setCursor(cur, endPit, right);
		cur.setSelection();
	} else
		setCursor(cur, endPit, right);

	cur.checkBufferStructure();
}


bool Text::handleBibitems(Cursor & cur)
{
	if (cur.paragraph().layout().labeltype != LABEL_BIBLIO)
		return false;

	if (cur.pos() != 0)
		return false;

	BufferParams const & bufparams = cur.buffer()->params();
	Paragraph const & par = cur.paragraph();
	Cursor prevcur = cur;
	if (cur.pit() > 0) {
		--prevcur.pit();
		prevcur.pos() = prevcur.lastpos();
	}
	Paragraph const & prevpar = prevcur.paragraph();

	// if a bibitem is deleted, merge with previous paragraph
	// if this is a bibliography item as well
	if (cur.pit() > 0 && par.layout() == prevpar.layout()) {
		cur.recordUndo(prevcur.pit());
		mergeParagraph(bufparams, cur.text()->paragraphs(),
							prevcur.pit());
		cur.forceBufferUpdate();
		setCursorIntern(cur, prevcur.pit(), prevcur.pos());
		cur.screenUpdateFlags(Update::Force);
		return true;
	}

	// otherwise reset to default
	cur.paragraph().setPlainOrDefaultLayout(bufparams.documentClass());
	return true;
}


bool Text::erase(Cursor & cur)
{
	LASSERT(this == cur.text(), return false);
	bool needsUpdate = false;
	Paragraph & par = cur.paragraph();

	if (cur.pos() != cur.lastpos()) {
		// this is the code for a normal delete, not pasting
		// any paragraphs
		cur.recordUndo(DELETE_UNDO);
		bool const was_inset = cur.paragraph().isInset(cur.pos());
		if(!par.eraseChar(cur.pos(), cur.buffer()->params().track_changes))
			// the character has been logically deleted only => skip it
			cur.top().forwardPos();

		if (was_inset)
			cur.forceBufferUpdate();
		else
			cur.checkBufferStructure();
		needsUpdate = true;
	} else {
		if (cur.pit() == cur.lastpit())
			return dissolveInset(cur);

		if (!par.isMergedOnEndOfParDeletion(cur.buffer()->params().track_changes)) {
			cur.recordUndo(DELETE_UNDO);
			par.setChange(cur.pos(), Change(Change::DELETED));
			cur.forwardPos();
			needsUpdate = true;
		} else {
			setCursorIntern(cur, cur.pit() + 1, 0);
			needsUpdate = backspacePos0(cur);
		}
	}

	needsUpdate |= handleBibitems(cur);

	if (needsUpdate) {
		// Make sure the cursor is correct. Is this really needed?
		// No, not really... at least not here!
		cur.top().setPitPos(cur.pit(), cur.pos());
		cur.checkBufferStructure();
	}

	return needsUpdate;
}


bool Text::backspacePos0(Cursor & cur)
{
	LBUFERR(this == cur.text());
	if (cur.pit() == 0)
		return false;

	BufferParams const & bufparams = cur.buffer()->params();
	ParagraphList & plist = cur.text()->paragraphs();
	Paragraph const & par = cur.paragraph();
	Cursor prevcur = cur;
	--prevcur.pit();
	prevcur.pos() = prevcur.lastpos();
	Paragraph const & prevpar = prevcur.paragraph();

	// is it an empty paragraph?
	if (cur.lastpos() == 0
	    || (cur.lastpos() == 1 && par.isSeparator(0))) {
		cur.recordUndo(prevcur.pit());
		plist.erase(plist.iterator_at(cur.pit()));
	}
	// is previous par empty?
	else if (prevcur.lastpos() == 0
		 || (prevcur.lastpos() == 1 && prevpar.isSeparator(0))) {
		cur.recordUndo(prevcur.pit());
		plist.erase(plist.iterator_at(prevcur.pit()));
	}
	// FIXME: Do we really not want to allow this???
	// Pasting is not allowed, if the paragraphs have different
	// layouts. I think it is a real bug of all other
	// word processors to allow it. It confuses the user.
	// Correction: Pasting is always allowed with standard-layout
	// or the empty layout.
	else {
		cur.recordUndo(prevcur.pit());
		mergeParagraph(bufparams, plist, prevcur.pit());
	}

	cur.forceBufferUpdate();
	setCursorIntern(cur, prevcur.pit(), prevcur.pos());

	return true;
}


bool Text::backspace(Cursor & cur)
{
	LBUFERR(this == cur.text());
	bool needsUpdate = false;
	if (cur.pos() == 0) {
		if (cur.pit() == 0)
			return dissolveInset(cur);

		Cursor prev_cur = cur;
		--prev_cur.pit();

		if (!cur.paragraph().empty()
		    && !prev_cur.paragraph().isMergedOnEndOfParDeletion(cur.buffer()->params().track_changes)) {
			cur.recordUndo(prev_cur.pit(), prev_cur.pit());
			prev_cur.paragraph().setChange(prev_cur.lastpos(), Change(Change::DELETED));
			setCursorIntern(cur, prev_cur.pit(), prev_cur.lastpos());
			return true;
		}
		// The cursor is at the beginning of a paragraph, so
		// the backspace will collapse two paragraphs into one.
		needsUpdate = backspacePos0(cur);

	} else {
		// this is the code for a normal backspace, not pasting
		// any paragraphs
		cur.recordUndo(DELETE_UNDO);
		// We used to do cursorBackwardIntern() here, but it is
		// not a good idea since it triggers the auto-delete
		// mechanism. So we do a cursorBackwardIntern()-lite,
		// without the dreaded mechanism. (JMarc)
		setCursorIntern(cur, cur.pit(), cur.pos() - 1,
				false, cur.boundary());
		bool const was_inset = cur.paragraph().isInset(cur.pos());
		cur.paragraph().eraseChar(cur.pos(), cur.buffer()->params().track_changes);
		if (was_inset)
			cur.forceBufferUpdate();
		else
			cur.checkBufferStructure();
	}

	if (cur.pos() == cur.lastpos())
		cur.setCurrentFont();

	needsUpdate |= handleBibitems(cur);

	// A singlePar update is not enough in this case.
	// cur.screenUpdateFlags(Update::Force);
	cur.top().setPitPos(cur.pit(), cur.pos());

	return needsUpdate;
}


bool Text::dissolveInset(Cursor & cur)
{
	LASSERT(this == cur.text(), return false);

	if (isMainText() || cur.inset().nargs() != 1)
		return false;

	cur.recordUndoInset();
	cur.setMark(false);
	cur.selHandle(false);
	// save position inside inset
	pos_type spos = cur.pos();
	pit_type spit = cur.pit();
	bool const inset_non_empty = cur.lastpit() != 0 || cur.lastpos() != 0;
	cur.popBackward();
	// update cursor offset
	if (spit == 0)
		spos += cur.pos();
	spit += cur.pit();
	// remember position outside inset to delete inset later
	// we do not do it now to avoid memory reuse issues (see #10667).
	DocIterator inset_it = cur;
	// jump over inset
	++cur.pos();

	Buffer & b = *cur.buffer();
	// Is there anything in this text?
	if (inset_non_empty) {
		// see bug 7319
		// we clear the cache so that we won't get conflicts with labels
		// that get pasted into the buffer. we should update this before
		// its being empty matters. if not (i.e., if we encounter bugs),
		// then this should instead be:
		//	  cur.buffer().updateBuffer();
		// but we'll try the cheaper solution here.
		cur.buffer()->clearReferenceCache();

		ParagraphList & plist = paragraphs();
		if (!lyxrc.ct_markup_copied)
			// Do not revive deleted text
			lyx::acceptChanges(plist, b.params());

		// ERT paragraphs have the Language latex_language.
		// This is invalid outside of ERT, so we need to
		// change it to the buffer language.
		for (auto & p : plist)
			p.changeLanguage(b.params(), latex_language, b.language());

		/* If the inset is the only thing in paragraph and the layout
		 * is not plain, then the layout of the first paragraph of
		 * inset should be remembered.
		 * FIXME: this does not work as expected when change tracking
		 *   is on However, we do not really know what to do in this
		 *   case.
		 */
		DocumentClass const & tclass = cur.buffer()->params().documentClass();
		if (inset_it.lastpos() == 1
		    && !tclass.isPlainLayout(plist[0].layout())
		    && !tclass.isDefaultLayout(plist[0].layout())) {
			// Copy all parameters except depth.
			Paragraph & par = cur.paragraph();
			par.setLayout(plist[0].layout());
			depth_type const dpth = par.getDepth();
			par.params() = plist[0].params();
			par.params().depth(dpth);
		}

		pasteParagraphList(cur, plist, b.params().documentClassPtr(),
				   b.params().authors(),
				   b.errorList("Paste"));
	}

	// delete the inset now
	inset_it.paragraph().eraseChar(inset_it.pos(), b.params().track_changes);

	// restore position
	cur.pit() = min(cur.lastpit(), spit);
	cur.pos() = min(cur.lastpos(), spos);
	// Ensure the current language is set correctly (bug 6292)
	cur.text()->setCursor(cur, cur.pit(), cur.pos());
	cur.clearSelection();
	cur.resetAnchor();
	cur.forceBufferUpdate();

	return true;
}


bool Text::splitInset(Cursor & cur)
{
	LASSERT(this == cur.text(), return false);

	if (isMainText() || cur.inset().nargs() != 1)
		return false;

	cur.recordUndo();
	if (cur.selection()) {
		// start from selection begin
		setCursor(cur, cur.selBegin().pit(), cur.selBegin().pos());
		cur.clearSelection();
	}
	// save split position inside inset
	// (we need to copy the whole inset first)
	pos_type spos = cur.pos();
	pit_type spit = cur.pit();
	// some things only need to be done if the inset has content
	bool const inset_non_empty = cur.lastpit() != 0 || cur.lastpos() != 0;

	// move right before the inset
	cur.popBackward();
	cur.resetAnchor();
	// remember position outside inset
	pos_type ipos = cur.pos();
	pit_type ipit = cur.pit();
	// select inset ...
	++cur.pos();
	cur.setSelection();
	// ... and copy
	cap::copySelectionToTemp(cur);
	cur.clearSelection();
	cur.resetAnchor();
	// paste copied inset
	cap::pasteFromTemp(cur, cur.buffer()->errorList("Paste"));
	cur.forceBufferUpdate();

	// if the inset has text, cut after split position
	// and paste to new inset
	if (inset_non_empty) {
		// go back to first inset
		cur.text()->setCursor(cur, ipit, ipos);
		cur.forwardPos();
		setCursor(cur, spit, spos);
		cur.resetAnchor();
		setCursor(cur, cur.lastpit(), getPar(cur.lastpit()).size());
		cur.setSelection();
		// Remember whether there was something cut that has to be pasted below
		// (bug #12747)
		bool const hasCut = cur.selection();
		cap::cutSelectionToTemp(cur);
		cur.setMark(false);
		cur.selHandle(false);
		cur.resetAnchor();
		bool atlastpos = false;
		if (cur.pos() == 0 && cur.pit() > 0) {
			// if we are at par start, remove this par
			cur.text()->backspace(cur);
			cur.forceBufferUpdate();
		} else if (cur.pos() == cur.lastpos())
			atlastpos = true;
		// Move out of and jump over inset
		cur.popBackward();
		++cur.pos();

		// enter new inset
		cur.forwardPos();
		cur.setCursor(cur);
		cur.resetAnchor();
		cur.text()->selectAll(cur);
		cutSelection(cur, false);
		// If there was something cut paste it
		if (hasCut)
			cap::pasteFromTemp(cur, cur.buffer()->errorList("Paste"));
		cur.text()->setCursor(cur, 0, 0);
		if (atlastpos && cur.paragraph().isFreeSpacing() && cur.paragraph().empty()) {
			// We started from par end, remove extra empty par in free spacing insets
			cur.text()->erase(cur);
			cur.forceBufferUpdate();
		}
	}

	cur.finishUndo();
	return true;
}


void Text::getWord(CursorSlice & from, CursorSlice & to,
	word_location const loc) const
{
	to = from;
	pars_[to.pit()].locateWord(from.pos(), to.pos(), loc);
}


void Text::write(ostream & os) const
{
	Buffer const & buf = owner_->buffer();
	ParagraphList::const_iterator pit = paragraphs().begin();
	ParagraphList::const_iterator end = paragraphs().end();
	depth_type dth = 0;
	for (; pit != end; ++pit)
		pit->write(os, buf.params(), dth);

	// Close begin_deeper
	for(; dth > 0; --dth)
		os << "\n\\end_deeper";
}


bool Text::read(Lexer & lex,
		ErrorList & errorList, InsetText * insetPtr)
{
	Buffer const & buf = owner_->buffer();
	depth_type depth = 0;
	bool res = true;

	while (lex.isOK()) {
		lex.nextToken();
		string const token = lex.getString();

		if (token.empty())
			continue;

		if (token == "\\end_inset")
			break;

		if (token == "\\end_body")
			continue;

		if (token == "\\begin_body")
			continue;

		if (token == "\\end_document") {
			res = false;
			break;
		}

		if (token == "\\begin_layout") {
			lex.pushToken(token);

			Paragraph par;
			par.setInsetOwner(insetPtr);
			par.params().depth(depth);
			par.setFont(0, Font(inherit_font, buf.params().language));
			pars_.push_back(par);
			readParagraph(pars_.back(), lex, errorList);

			// register the words in the global word list
			pars_.back().updateWords();
		} else if (token == "\\begin_deeper") {
			++depth;
		} else if (token == "\\end_deeper") {
			if (!depth)
				lex.printError("\\end_deeper: " "depth is already null");
			else
				--depth;
		} else {
			LYXERR0("Handling unknown body token: `" << token << '\'');
		}
	}

	// avoid a crash on weird documents (bug 4859)
	if (pars_.empty()) {
		Paragraph par;
		par.setInsetOwner(insetPtr);
		par.params().depth(depth);
		par.setFont(0, Font(inherit_font,
				    buf.params().language));
		par.setPlainOrDefaultLayout(buf.params().documentClass());
		pars_.push_back(par);
	}

	return res;
}


// Returns the current state (font, depth etc.) as a message for status bar.
docstring Text::currentState(CursorData const & cur, bool devel_mode) const
{
	LBUFERR(this == cur.text());
	Buffer & buf = *cur.buffer();
	Paragraph const & par = cur.paragraph();
	odocstringstream os;

	if (buf.params().track_changes)
		os << _("[Change Tracking] ");

	Change change = par.lookupChange(cur.pos());

	if (change.changed()) {
		docstring const author =
			buf.params().authors().get(change.author).nameAndEmail();
		docstring const date = formatted_datetime(change.changetime);
		os << bformat(_("Changed by %1$s[[author]] on %2$s[[date]]. "),
		              author, date);
	}

	// I think we should only show changes from the default
	// font. (Asger)
	// No, from the document font (MV)
	Font font = cur.real_current_font;
	font.fontInfo().reduce(buf.params().getFont().fontInfo());

	os << bformat(_("Font: %1$s"), font.stateText(&buf.params()));

	// The paragraph depth
	int depth = par.getDepth();
	if (depth > 0)
		os << bformat(_(", Depth: %1$d"), depth);

	// The paragraph spacing, but only if different from
	// buffer spacing.
	Spacing const & spacing = par.params().spacing();
	if (!spacing.isDefault()) {
		os << _(", Spacing: ");
		switch (spacing.getSpace()) {
		case Spacing::Single:
			os << _("Single");
			break;
		case Spacing::Onehalf:
			os << _("OneHalf");
			break;
		case Spacing::Double:
			os << _("Double");
			break;
		case Spacing::Other:
			os << _("Other (") << from_ascii(spacing.getValueAsString()) << ')';
			break;
		case Spacing::Default:
			// should never happen, do nothing
			break;
		}
	}

	// Custom text style
	InsetLayout const & layout = cur.inset().getLayout();
	if (layout.lyxtype() == InsetLyXType::CHARSTYLE)
		os << _(", Style: ") << translateIfPossible(layout.labelstring());

	if (devel_mode) {
		os << _(", Inset: ") << &cur.inset();
		if (cur.lastidx() > 0)
			os << _(", Cell: ") << cur.idx();
		os << _(", Paragraph: ") << cur.pit();
		os << _(", Id: ") << par.id();
		os << _(", Position: ") << cur.pos();
		// FIXME: Why is the check for par.size() needed?
		// We are called with cur.pos() == par.size() quite often.
		if (!par.empty() && cur.pos() < par.size()) {
			// Force output of code point, not character
			size_t const c = par.getChar(cur.pos());
			if (c == META_INSET)
				os << ", Char: INSET";
			else
				os << _(", Char: 0x") << hex << c;
		}
		os << _(", Boundary: ") << cur.boundary();
//		Row & row = cur.textRow();
//		os << bformat(_(", Row b:%1$d e:%2$d"), row.pos(), row.endpos());
	}
	return os.str();
}


docstring Text::getPossibleLabel(DocIterator const & cur) const
{
	pit_type textpit = cur.pit();
	Layout const * layout = &(pars_[textpit].layout());

	// Will contain the label prefix.
	docstring name;

	// For captions, we just take the caption type
	Inset * caption_inset = cur.innerInsetOfType(CAPTION_CODE);
	if (caption_inset) {
		string const & ftype = static_cast<InsetCaption *>(caption_inset)->floattype();
		FloatList const & fl = cur.buffer()->params().documentClass().floats();
		if (fl.typeExist(ftype)) {
			Floating const & flt = fl.getType(ftype);
			name = from_utf8(flt.refPrefix());
		}
		if (name.empty())
			name = from_utf8(ftype.substr(0,3));
	} else {
		// For section, subsection, etc...
		if (layout->latextype == LATEX_PARAGRAPH && textpit != 0) {
			Layout const * layout2 = &(pars_[textpit - 1].layout());
			if (layout2->latextype != LATEX_PARAGRAPH) {
				--textpit;
				layout = layout2;
			}
		}
		if (layout->latextype != LATEX_PARAGRAPH)
			name = layout->refprefix;

		// If none of the above worked, see if the inset knows.
		if (name.empty()) {
			InsetLayout const & il = cur.inset().getLayout();
			name = il.refprefix();
		}
	}

	docstring text;
	docstring par_text = pars_[textpit].asString(AS_STR_SKIPDELETE);

	// The return string of math matrices might contain linebreaks
	par_text = subst(par_text, '\n', '-');
	int const numwords = 3;
	for (int i = 0; i < numwords; ++i) {
		if (par_text.empty())
			break;
		docstring head;
		par_text = split(par_text, head, ' ');
		// Is it legal to use spaces in labels ?
		if (i > 0)
			text += '-';
		text += head;
	}

	// Make sure it isn't too long
	unsigned int const max_label_length = 32;
	if (text.size() > max_label_length)
		text.resize(max_label_length);

	if (!name.empty())
		text = name + ':' + text;

	// We need a unique label
	docstring label = text;
	int i = 1;
	while (cur.buffer()->activeLabel(label)) {
			label = text + '-' + convert<docstring>(i);
			++i;
		}

	return label;
}


docstring Text::asString(int options) const
{
	return asString(0, pars_.size(), options);
}


docstring Text::asString(pit_type beg, pit_type end, int options) const
{
	size_t i = size_t(beg);
	docstring str = pars_[i].asString(options);
	for (++i; i != size_t(end); ++i) {
		str += '\n';
		str += pars_[i].asString(options);
	}
	return str;
}


void Text::shortenForOutliner(docstring & str, size_t const maxlen)
{
	support::truncateWithEllipsis(str, maxlen);
	for (char_type & c : str)
		if (c == L'\n' || c == L'\t')
			c = L' ';
}


void Text::forOutliner(docstring & os, size_t const maxlen,
                       bool const shorten) const
{
	pit_type end = pars_.size() - 1;
	if (0 <= end && !pars_[0].labelString().empty())
		os += pars_[0].labelString() + ' ';
	forOutliner(os, maxlen, 0, end, shorten);
}


void Text::forOutliner(docstring & os, size_t const maxlen,
                       pit_type pit_start, pit_type pit_end,
                       bool const shorten) const
{
	size_t tmplen = shorten ? maxlen + 1 : maxlen;
	pit_type end = min(size_t(pit_end), pars_.size() - 1);
	bool first = true;
	for (pit_type i = pit_start; i <= end && os.length() < tmplen; ++i) {
		if (!first)
			os += ' ';
		// This function lets the first label be treated separately
		pars_[i].forOutliner(os, tmplen, false, !first);
		first = false;
	}
	if (shorten)
		shortenForOutliner(os, maxlen);
}


void Text::charsTranspose(Cursor & cur)
{
	LBUFERR(this == cur.text());

	pos_type pos = cur.pos();

	// If cursor is at beginning or end of paragraph, do nothing.
	if (pos == cur.lastpos() || pos == 0)
		return;

	Paragraph & par = cur.paragraph();

	// Get the positions of the characters to be transposed.
	pos_type pos1 = pos - 1;
	pos_type pos2 = pos;

	// In change tracking mode, ignore deleted characters.
	while (pos2 < cur.lastpos() && par.isDeleted(pos2))
		++pos2;
	if (pos2 == cur.lastpos())
		return;

	while (pos1 >= 0 && par.isDeleted(pos1))
		--pos1;
	if (pos1 < 0)
		return;

	// Don't do anything if one of the "characters" is not regular text.
	if (par.isInset(pos1) || par.isInset(pos2))
		return;

	// Store the characters to be transposed (including font information).
	char_type const char1 = par.getChar(pos1);
	Font const font1 =
		par.getFontSettings(cur.buffer()->params(), pos1);

	char_type const char2 = par.getChar(pos2);
	Font const font2 =
		par.getFontSettings(cur.buffer()->params(), pos2);

	// And finally, we are ready to perform the transposition.
	// Track the changes if Change Tracking is enabled.
	bool const trackChanges = cur.buffer()->params().track_changes;

	cur.recordUndo();

	par.eraseChar(pos2, trackChanges);
	par.eraseChar(pos1, trackChanges);
	par.insertChar(pos1, char2, font2, trackChanges);
	par.insertChar(pos2, char1, font1, trackChanges);

	cur.checkBufferStructure();

	// After the transposition, move cursor to after the transposition.
	setCursor(cur, cur.pit(), pos2);
	cur.forwardPos();
}


DocIterator Text::macrocontextPosition() const
{
	return macrocontext_position_;
}


void Text::setMacrocontextPosition(DocIterator const & pos)
{
	macrocontext_position_ = pos;
}


bool Text::completionSupported(Cursor const & cur) const
{
	Paragraph const & par = cur.paragraph();
	return !cur.buffer()->isReadonly()
		&& !cur.selection()
		&& cur.pos() > 0
		&& (cur.pos() >= par.size() || par.isWordSeparator(cur.pos()))
		&& !par.isWordSeparator(cur.pos() - 1);
}


CompletionList const * Text::createCompletionList(Cursor const & cur) const
{
	WordList const & list = theWordList(cur.getFont().language()->lang());
	return new TextCompletionList(cur, list);
}


bool Text::insertCompletion(Cursor & cur, docstring const & s)
{
	LBUFERR(cur.bv().cursor() == cur);
	if (cur.buffer()->isReadonly())
		return false;
	cur.recordUndo();
	cur.insert(s);
	cur.bv().cursor() = cur;
	if (!(cur.result().screenUpdate() & Update::Force))
		cur.screenUpdateFlags(cur.result().screenUpdate() | Update::SinglePar);
	return true;
}


docstring Text::completionPrefix(Cursor const & cur) const
{
	CursorSlice from = cur.top();
	CursorSlice to = from;
	getWord(from, to, PREVIOUS_WORD);

	return cur.paragraph().asString(from.pos(), to.pos());
}

bool Text::isMainText() const
{
	return &owner_->buffer().text() == this;
}


// Note that this is supposed to return a fully realized font.
FontInfo Text::layoutFont(pit_type const pit) const
{
	Layout const & layout = pars_[pit].layout();

	if (!pars_[pit].getDepth())  {
		FontInfo lf = layout.resfont;
		// In case the default family has been customized
		if (layout.font.family() == INHERIT_FAMILY)
			lf.setFamily(owner_->buffer().params().getFont().fontInfo().family());
		FontInfo icf = (!isMainText())
				  // inside insets, we call the getFont() method
				? owner_->getFont()
				  // outside, we access the layout font directly
				: owner_->getLayout().font();
		icf.realize(lf);
		return icf;
	}

	FontInfo font = layout.font;
	// Realize with the fonts of lesser depth.
	//font.realize(outerFont(pit));
	font.realize(owner_->buffer().params().getFont().fontInfo());

	return font;
}


// Note that this is supposed to return a fully realized font.
FontInfo Text::labelFont(Paragraph const & par) const
{
	Buffer const & buffer = owner_->buffer();
	Layout const & layout = par.layout();

	if (!par.getDepth()) {
		FontInfo lf = layout.reslabelfont;
		// In case the default family has been customized
		if (layout.labelfont.family() == INHERIT_FAMILY)
			lf.setFamily(buffer.params().getFont().fontInfo().family());
		return lf;
	}

	FontInfo font = layout.labelfont;
	// Realize with the fonts of lesser depth.
	font.realize(buffer.params().getFont().fontInfo());

	return font;
}


void Text::setCharFont(pit_type pit,
		pos_type pos, Font const & fnt, Font const & display_font)
{
	Buffer const & buffer = owner_->buffer();
	Font font = fnt;
	Layout const & layout = pars_[pit].layout();

	// Get concrete layout font to reduce against
	FontInfo layoutfont;

	if (pos < pars_[pit].beginOfBody())
		layoutfont = layout.labelfont;
	else
		layoutfont = layout.font;

	// Realize against environment font information
	if (pars_[pit].getDepth()) {
		pit_type tp = pit;
		while (!layoutfont.resolved() &&
		       tp != pit_type(paragraphs().size()) &&
		       pars_[tp].getDepth()) {
			tp = outerHook(tp);
			if (tp != pit_type(paragraphs().size()))
				layoutfont.realize(pars_[tp].layout().font);
		}
	}

	// Inside inset, apply the inset's font attributes if any
	// (charstyle!)
	if (!isMainText())
		layoutfont.realize(display_font.fontInfo());

	layoutfont.realize(buffer.params().getFont().fontInfo());

	// Now, reduce font against full layout font
	font.fontInfo().reduce(layoutfont);

	pars_[pit].setFont(pos, font);
}


void Text::setInsetFont(BufferView const & bv, pit_type pit,
		pos_type pos, Font const & font)
{
	Inset * const inset = pars_[pit].getInset(pos);
	LASSERT(inset && inset->resetFontEdit(), return);

	idx_type endidx = inset->nargs();
	for (CursorSlice cs(*inset); cs.idx() != endidx; ++cs.idx()) {
		Text * text = cs.text();
		if (text) {
			// last position of the cell
			CursorSlice cellend = cs;
			cellend.pit() = cellend.lastpit();
			cellend.pos() = cellend.lastpos();
			text->setFont(bv, cs, cellend, font);
		}
	}
}


void Text::setLayout(pit_type start, pit_type end,
		     docstring const & layout)
{
	// FIXME: make this work in multicell selection case
	LASSERT(start != end, return);

	Buffer const & buffer = owner_->buffer();
	BufferParams const & bp = buffer.params();
	Layout const & lyxlayout = bp.documentClass()[layout];

	for (pit_type pit = start; pit != end; ++pit) {
		Paragraph & par = pars_[pit];
		// Is this a separating paragraph? If so,
		// this needs to be standard layout
		bool const is_separator = par.size() == 1
				&& par.isEnvSeparator(0);
		par.applyLayout(is_separator ? bp.documentClass().defaultLayout() : lyxlayout);
		if (lyxlayout.margintype == MARGIN_MANUAL)
			par.setLabelWidthString(par.expandLabel(lyxlayout, bp));
	}

	deleteEmptyParagraphMechanism(start, end - 1, bp.track_changes);
}


// set layout over selection and make a total rebreak of those paragraphs
void Text::setLayout(Cursor & cur, docstring const & layout)
{
	LBUFERR(this == cur.text());

	pit_type start = cur.selBegin().pit();
	pit_type end = cur.selEnd().pit() + 1;
	cur.recordUndoSelection();
	setLayout(start, end, layout);
	cur.fixIfBroken();
	cur.setCurrentFont();
	cur.forceBufferUpdate();
}


static bool changeDepthAllowed(Text::DEPTH_CHANGE type,
			Paragraph const & par, int max_depth)
{
	int const depth = par.params().depth();
	if (type == Text::INC_DEPTH && depth < max_depth)
		return true;
	if (type == Text::DEC_DEPTH && depth > 0)
		return true;
	return false;
}


bool Text::changeDepthAllowed(Cursor const & cur, DEPTH_CHANGE type) const
{
	LBUFERR(this == cur.text());
	// this happens when selecting several cells in tabular (bug 2630)
	if (cur.selBegin().idx() != cur.selEnd().idx())
		return false;

	pit_type const beg = cur.selBegin().pit();
	pit_type const end = cur.selEnd().pit() + 1;
	int max_depth = (beg != 0 ? pars_[beg - 1].getMaxDepthAfter() : 0);

	for (pit_type pit = beg; pit != end; ++pit) {
		if (lyx::changeDepthAllowed(type, pars_[pit], max_depth))
			return true;
		max_depth = pars_[pit].getMaxDepthAfter();
	}
	return false;
}


void Text::changeDepth(Cursor & cur, DEPTH_CHANGE type)
{
	LBUFERR(this == cur.text());
	pit_type const beg = cur.selBegin().pit();
	pit_type const end = cur.selEnd().pit() + 1;
	cur.recordUndoSelection();
	int max_depth = (beg != 0 ? pars_[beg - 1].getMaxDepthAfter() : 0);

	for (pit_type pit = beg; pit != end; ++pit) {
		Paragraph & par = pars_[pit];
		if (lyx::changeDepthAllowed(type, par, max_depth)) {
			int const depth = par.params().depth();
			if (type == INC_DEPTH)
				par.params().depth(depth + 1);
			else
				par.params().depth(depth - 1);
		}
		max_depth = par.getMaxDepthAfter();
	}
	cur.setCurrentFont();
	// this handles the counter labels, and also fixes up
	// depth values for follow-on (child) paragraphs
	cur.forceBufferUpdate();
}


void Text::setFont(Cursor & cur, Font const & font, bool toggleall)
{
	LASSERT(this == cur.text(), return);

	// If there is a selection, record undo before the cursor font is changed.
	if (cur.selection())
		cur.recordUndoSelection();

	// Set the current_font
	// Determine basis font
	FontInfo layoutfont;
	pit_type pit = cur.pit();
	if (cur.pos() < pars_[pit].beginOfBody())
		layoutfont = labelFont(pars_[pit]);
	else
		layoutfont = layoutFont(pit);

	// Update current font
	cur.real_current_font.update(font,
					cur.buffer()->params().language,
					toggleall);

	// Reduce to implicit settings
	cur.current_font = cur.real_current_font;
	cur.current_font.fontInfo().reduce(layoutfont);
	// And resolve it completely
	cur.real_current_font.fontInfo().realize(layoutfont);

	// if there is no selection that's all we need to do
	if (!cur.selection())
		return;

	// Ok, we have a selection.
	Font newfont = font;

	if (toggleall) {
		// Toggling behaves as follows: We check the first character of the
		// selection. If it's (say) got EMPH on, then we set to off; if off,
		// then to on. With families and the like, we set it to INHERIT, if
		// we already have it.
		CursorSlice const & sl = cur.selBegin();
		Text const & text = *sl.text();
		Paragraph const & par = text.getPar(sl.pit());

		// get font at the position
		Font oldfont = par.getFont(cur.bv().buffer().params(), sl.pos(),
			text.outerFont(sl.pit()));
		FontInfo const & oldfi = oldfont.fontInfo();

		FontInfo & newfi = newfont.fontInfo();

		FontFamily newfam = newfi.family();
		if (newfam !=	INHERIT_FAMILY && newfam != IGNORE_FAMILY &&
				newfam == oldfi.family())
			newfi.setFamily(INHERIT_FAMILY);

		FontSeries newser = newfi.series();
		if (newser == BOLD_SERIES && oldfi.series() == BOLD_SERIES)
			newfi.setSeries(INHERIT_SERIES);

		FontShape newshp = newfi.shape();
		if (newshp != INHERIT_SHAPE && newshp != IGNORE_SHAPE &&
				newshp == oldfi.shape())
			newfi.setShape(INHERIT_SHAPE);

		ColorCode newcol = newfi.color();
		if (newcol != Color_none && newcol != Color_inherit
		    && newcol != Color_ignore && newcol == oldfi.color())
			newfi.setColor(Color_none);

		// ON/OFF ones
		if (newfi.emph() == FONT_TOGGLE)
			newfi.setEmph(oldfi.emph() == FONT_OFF ? FONT_ON : FONT_OFF);
		if (newfi.underbar() == FONT_TOGGLE)
			newfi.setUnderbar(oldfi.underbar() == FONT_OFF ? FONT_ON : FONT_OFF);
		if (newfi.strikeout() == FONT_TOGGLE)
			newfi.setStrikeout(oldfi.strikeout() == FONT_OFF ? FONT_ON : FONT_OFF);
		if (newfi.xout() == FONT_TOGGLE)
			newfi.setXout(oldfi.xout() == FONT_OFF ? FONT_ON : FONT_OFF);
		if (newfi.uuline() == FONT_TOGGLE)
			newfi.setUuline(oldfi.uuline() == FONT_OFF ? FONT_ON : FONT_OFF);
		if (newfi.uwave() == FONT_TOGGLE)
			newfi.setUwave(oldfi.uwave() == FONT_OFF ? FONT_ON : FONT_OFF);
		if (newfi.noun() == FONT_TOGGLE)
			newfi.setNoun(oldfi.noun() == FONT_OFF ? FONT_ON : FONT_OFF);
		if (newfi.number() == FONT_TOGGLE)
			newfi.setNumber(oldfi.number() == FONT_OFF ? FONT_ON : FONT_OFF);
		if (newfi.nospellcheck() == FONT_TOGGLE)
			newfi.setNoSpellcheck(oldfi.nospellcheck() == FONT_OFF ? FONT_ON : FONT_OFF);
	}

	setFont(cur.bv(), cur.selectionBegin().top(),
		cur.selectionEnd().top(), newfont);
}


void Text::setFont(BufferView const & bv, CursorSlice const & begin,
		CursorSlice const & end, Font const & font)
{
	Buffer const & buffer = bv.buffer();

	// Don't use forwardChar here as ditend might have
	// pos() == lastpos() and forwardChar would miss it.
	// Can't use forwardPos either as this descends into
	// nested insets.
	Language const * language = buffer.params().language;
	for (CursorSlice dit = begin; dit != end; dit.forwardPos()) {
		if (dit.pos() == dit.lastpos())
			continue;
		pit_type const pit = dit.pit();
		pos_type const pos = dit.pos();
		Inset * inset = pars_[pit].getInset(pos);
		if (inset && inset->resetFontEdit()) {
			// We need to propagate the font change to all
			// text cells of the inset (bugs 1973, 6919).
			setInsetFont(bv, pit, pos, font);
		}
		TextMetrics const & tm = bv.textMetrics(this);
		Font f = tm.displayFont(pit, pos);
		f.update(font, language);
		setCharFont(pit, pos, f, tm.font_);
		// font change may change language...
		// spell checker has to know that
		pars_[pit].requestSpellCheck(pos);
	}
}


bool Text::cursorTop(Cursor & cur)
{
	LBUFERR(this == cur.text());
	return setCursor(cur, 0, 0);
}


bool Text::cursorBottom(Cursor & cur)
{
	LBUFERR(this == cur.text());
	return setCursor(cur, cur.lastpit(), prev(paragraphs().end(), 1)->size());
}


void Text::toggleFree(Cursor & cur, Font const & font, bool toggleall)
{
	LBUFERR(this == cur.text());
	// If the mask is completely neutral, tell user
	if (font.fontInfo() == ignore_font && font.language() == ignore_language) {
		// Could only happen with user style
		cur.message(_("No font change defined."));
		return;
	}

	// Try implicit word selection
	// If there is a change in the language the implicit word selection
	// is disabled.
	CursorSlice const resetCursor = cur.top();
	bool const implicitSelection =
		font.language() == ignore_language
		&& font.fontInfo().number() == FONT_IGNORE
		&& selectWordWhenUnderCursor(cur, WHOLE_WORD_STRICT);

	// Set font
	setFont(cur, font, toggleall);

	// Implicit selections are cleared afterwards
	// and cursor is set to the original position.
	if (implicitSelection) {
		cur.clearSelection();
		cur.top() = resetCursor;
		cur.resetAnchor();
	}

	// if there was no selection at all, the point was to change cursor font.
	// Otherwise, we want to reset it to local text font.
	if (cur.selection() || implicitSelection)
		cur.setCurrentFont();
}


docstring Text::getStringForDialog(Cursor & cur)
{
	LBUFERR(this == cur.text());

	if (cur.selection())
		return cur.selectionAsString(false);

	// Try implicit word selection. If there is a change
	// in the language the implicit word selection is
	// disabled.
	selectWordWhenUnderCursor(cur, WHOLE_WORD);
	docstring const & retval = cur.selectionAsString(false);
	cur.clearSelection();
	return retval;
}


void Text::setLabelWidthStringToSequence(Cursor const & cur,
		docstring const & s)
{
	Cursor c = cur;
	// Find first of same layout in sequence
	while (!isFirstInSequence(c.pit())) {
		c.pit() = depthHook(c.pit(), c.paragraph().getDepth());
	}

	// now apply label width string to every par
	// in sequence
	depth_type const depth = c.paragraph().getDepth();
	Layout const & layout = c.paragraph().layout();
	for ( ; c.pit() <= c.lastpit() ; ++c.pit()) {
		while (c.paragraph().getDepth() > depth) {
			++c.pit();
			if (c.pit() > c.lastpit())
				return;
		}
		if (c.paragraph().getDepth() < depth)
			return;
		if (c.paragraph().layout() != layout)
			return;
		c.recordUndo();
		c.paragraph().setLabelWidthString(s);
	}
}


void Text::setParagraphs(Cursor const & cur, docstring const & arg, bool merge)
{
	LBUFERR(cur.text());

	//FIXME UNICODE
	string const argument = to_utf8(arg);
	depth_type priordepth = -1;
	Layout priorlayout;
	Cursor c(cur.bv());
	c.setCursor(cur.selectionBegin());
	pit_type const last_pit = cur.selectionEnd().pit();
	for ( ; c.pit() <= last_pit ; ++c.pit()) {
		Paragraph & par = c.paragraph();
		ParagraphParameters params = par.params();
		params.read(argument, merge);
		// Changes to label width string apply to all paragraphs
		// with same layout in a sequence.
		// Do this only once for a selected range of paragraphs
		// of the same layout and depth.
		c.recordUndo();
		par.params().apply(params, par.layout());
		if (par.getDepth() != priordepth || par.layout() != priorlayout)
			setLabelWidthStringToSequence(c, params.labelWidthString());
		priordepth = par.getDepth();
		priorlayout = par.layout();
	}
}


void Text::setParagraphs(Cursor const & cur, ParagraphParameters const & p)
{
	LBUFERR(cur.text());

	depth_type priordepth = -1;
	Layout priorlayout;
	Cursor c(cur.bv());
	c.setCursor(cur.selectionBegin());
	pit_type const last_pit = cur.selectionEnd().pit();
	for ( ; c.pit() <= last_pit ; ++c.pit()) {
		Paragraph & par = c.paragraph();
		// Changes to label width string apply to all paragraphs
		// with same layout in a sequence.
		// Do this only once for a selected range of paragraphs
		// of the same layout and depth.
		cur.recordUndo();
		par.params().apply(p, par.layout());
		if (par.getDepth() != priordepth || par.layout() != priorlayout)
			setLabelWidthStringToSequence(c,
				par.params().labelWidthString());
		priordepth = par.getDepth();
		priorlayout = par.layout();
	}
}


// just insert the inset and not move the cursor.
bool Text::insertInset(Cursor & cur, Inset * inset)
{
	LBUFERR(this == cur.text());
	LBUFERR(inset);
	return cur.paragraph().insertInset(cur.pos(), inset, cur.current_font,
		Change(cur.buffer()->params().track_changes
		? Change::INSERTED : Change::UNCHANGED));
}


bool Text::setCursor(Cursor & cur, pit_type pit, pos_type pos,
			bool setfont, bool boundary)
{
	TextMetrics const & tm = cur.bv().textMetrics(this);
	bool const update_needed = !tm.contains(pit);
	Cursor old = cur;
	setCursorIntern(cur, pit, pos, setfont, boundary);
	return cur.bv().checkDepm(cur, old) || update_needed;
}


void Text::setCursorIntern(Cursor & cur, pit_type pit, pos_type pos,
                           bool setfont, bool boundary)
{
	LBUFERR(this == cur.text());
	cur.boundary(boundary);
	cur.top().setPitPos(pit, pos);
	if (setfont)
		cur.setCurrentFont();
}


bool Text::checkAndActivateInset(Cursor & cur, bool front)
{
	if (front && cur.pos() == cur.lastpos())
		return false;
	if (!front && cur.pos() == 0)
		return false;
	Inset * inset = front ? cur.nextInset() : cur.prevInset();
	if (!inset || !inset->editable())
		return false;
	if (cur.selection() && cur.realAnchor().find(inset) == -1)
		return false;
	/*
	 * Apparently, when entering an inset we are expected to be positioned
	 * *before* it in the containing paragraph, regardless of the direction
	 * from which we are entering. Otherwise, cursor placement goes awry,
	 * and when we exit from the beginning, we'll be placed *after* the
	 * inset.
	 */
	if (!front)
		--cur.pos();
	inset->edit(cur, front);
	cur.setCurrentFont();
	cur.boundary(false);
	return true;
}


bool Text::checkAndActivateInsetVisual(Cursor & cur, bool movingForward, bool movingLeft)
{
	if (cur.pos() == -1)
		return false;
	if (cur.pos() == cur.lastpos())
		return false;
	Paragraph & par = cur.paragraph();
	Inset * inset = par.isInset(cur.pos()) ? par.getInset(cur.pos()) : nullptr;
	if (!inset || !inset->editable())
		return false;
	if (cur.selection() && cur.realAnchor().find(inset) == -1)
		return false;
	inset->edit(cur, movingForward,
		movingLeft ? Inset::ENTRY_DIRECTION_RIGHT : Inset::ENTRY_DIRECTION_LEFT);
	cur.setCurrentFont();
	cur.boundary(false);
	return true;
}


bool Text::cursorBackward(Cursor & cur)
{
	// Tell BufferView to test for FitCursor in any case!
	cur.screenUpdateFlags(Update::FitCursor);

	// not at paragraph start?
	if (cur.pos() > 0) {
		// if on right side of boundary (i.e. not at paragraph end, but line end)
		// -> skip it, i.e. set boundary to true, i.e. go only logically left
		// there are some exceptions to ignore this: lineseps, newlines, spaces
#if 0
		// some effectless debug code to see the values in the debugger
		bool bound = cur.boundary();
		int rowpos = cur.textRow().pos();
		int pos = cur.pos();
		bool sep = cur.paragraph().isSeparator(cur.pos() - 1);
		bool newline = cur.paragraph().isNewline(cur.pos() - 1);
		bool linesep = cur.paragraph().isLineSeparator(cur.pos() - 1);
#endif
		if (!cur.boundary() &&
				cur.textRow().pos() == cur.pos() &&
				!cur.paragraph().isLineSeparator(cur.pos() - 1) &&
				!cur.paragraph().isNewline(cur.pos() - 1) &&
				!cur.paragraph().isEnvSeparator(cur.pos() - 1) &&
				!cur.paragraph().isSeparator(cur.pos() - 1)) {
			return setCursor(cur, cur.pit(), cur.pos(), true, true);
		}

		// go left and try to enter inset
		if (checkAndActivateInset(cur, false))
			return false;

		// normal character left
		return setCursor(cur, cur.pit(), cur.pos() - 1, true, false);
	}

	// move to the previous paragraph or do nothing
	if (cur.pit() > 0) {
		Paragraph & par = getPar(cur.pit() - 1);
		pos_type lastpos = par.size();
		if (lastpos > 0 && par.isEnvSeparator(lastpos - 1))
			return setCursor(cur, cur.pit() - 1, lastpos - 1, true, false);
		else
			return setCursor(cur, cur.pit() - 1, lastpos, true, false);
	}
	return false;
}


bool Text::cursorVisLeft(Cursor & cur, bool skip_inset)
{
	Cursor temp_cur = cur;
	temp_cur.posVisLeft(skip_inset);
	if (temp_cur.depth() > cur.depth()) {
		cur = temp_cur;
		return false;
	}
	return setCursor(cur, temp_cur.pit(), temp_cur.pos(),
		true, temp_cur.boundary());
}


bool Text::cursorVisRight(Cursor & cur, bool skip_inset)
{
	Cursor temp_cur = cur;
	temp_cur.posVisRight(skip_inset);
	if (temp_cur.depth() > cur.depth()) {
		cur = temp_cur;
		return false;
	}
	return setCursor(cur, temp_cur.pit(), temp_cur.pos(),
		true, temp_cur.boundary());
}


bool Text::cursorForward(Cursor & cur)
{
	// Tell BufferView to test for FitCursor in any case!
	cur.screenUpdateFlags(Update::FitCursor);

	// not at paragraph end?
	if (cur.pos() != cur.lastpos()) {
		// in front of editable inset, i.e. jump into it?
		if (checkAndActivateInset(cur, true))
			return false;

		TextMetrics const & tm = cur.bv().textMetrics(this);
		// if left of boundary -> just jump to right side
		// but for RTL boundaries don't, because: abc|DDEEFFghi -> abcDDEEF|Fghi
		if (cur.boundary() && !tm.isRTLBoundary(cur.pit(), cur.pos()))
			return setCursor(cur, cur.pit(), cur.pos(), true, false);

		// next position is left of boundary,
		// but go to next line for special cases like space, newline, linesep
#if 0
		// some effectless debug code to see the values in the debugger
		int endpos = cur.textRow().endpos();
		int lastpos = cur.lastpos();
		int pos = cur.pos();
		bool linesep = cur.paragraph().isLineSeparator(cur.pos());
		bool newline = cur.paragraph().isNewline(cur.pos());
		bool sep = cur.paragraph().isSeparator(cur.pos());
		if (cur.pos() != cur.lastpos()) {
			bool linesep2 = cur.paragraph().isLineSeparator(cur.pos()+1);
			bool newline2 = cur.paragraph().isNewline(cur.pos()+1);
			bool sep2 = cur.paragraph().isSeparator(cur.pos()+1);
		}
#endif
		if (cur.textRow().endpos() == cur.pos() + 1) {
			if (cur.paragraph().isEnvSeparator(cur.pos()) &&
			    cur.pos() + 1 == cur.lastpos() &&
			    cur.pit() != cur.lastpit()) {
				// move to next paragraph
				return setCursor(cur, cur.pit() + 1, 0, true, false);
			} else if (cur.textRow().endpos() != cur.lastpos() &&
				   !cur.paragraph().isNewline(cur.pos()) &&
				   !cur.paragraph().isEnvSeparator(cur.pos()) &&
				   !cur.paragraph().isLineSeparator(cur.pos()) &&
				   !cur.paragraph().isSeparator(cur.pos())) {
				return setCursor(cur, cur.pit(), cur.pos() + 1, true, true);
			}
		}

		// in front of RTL boundary? Stay on this side of the boundary because:
		//   ab|cDDEEFFghi -> abc|DDEEFFghi
		if (tm.isRTLBoundary(cur.pit(), cur.pos() + 1))
			return setCursor(cur, cur.pit(), cur.pos() + 1, true, true);

		// move right
		return setCursor(cur, cur.pit(), cur.pos() + 1, true, false);
	}

	// move to next paragraph
	if (cur.pit() != cur.lastpit())
		return setCursor(cur, cur.pit() + 1, 0, true, false);
	return false;
}


bool Text::cursorUpParagraph(Cursor & cur)
{
	bool updated = false;
	if (cur.pos() > 0)
		updated = setCursor(cur, cur.pit(), 0);
	else if (cur.pit() != 0)
		updated = setCursor(cur, cur.pit() - 1, 0);
	return updated;
}


bool Text::cursorDownParagraph(Cursor & cur)
{
	bool updated = false;
	if (cur.pit() != cur.lastpit())
		if (lyxrc.mac_like_cursor_movement)
			if (cur.pos() == cur.lastpos())
				updated = setCursor(cur, cur.pit() + 1, getPar(cur.pit() + 1).size());
			else
				updated = setCursor(cur, cur.pit(), cur.lastpos());
		else
			updated = setCursor(cur, cur.pit() + 1, 0);
	else
		updated = setCursor(cur, cur.pit(), cur.lastpos());
	return updated;
}

namespace {

/** delete num_spaces characters between from and to. Return the
 * number of spaces that got physically deleted (not marked as
 * deleted) */
int deleteSpaces(Paragraph & par, pos_type const from, pos_type to,
				  int num_spaces, bool const trackChanges)
{
	if (num_spaces <= 0)
		return 0;

	// First, delete spaces marked as inserted
	int pos = from;
	while (pos < to && num_spaces > 0) {
		Change const & change = par.lookupChange(pos);
		if (change.inserted() && !change.currentAuthor()) {
			par.eraseChar(pos, trackChanges);
			--num_spaces;
			--to;
		} else
			++pos;
	}

	// Then remove remaining spaces
	int const psize = par.size();
	par.eraseChars(from, from + num_spaces, trackChanges);
	return psize - par.size();
}

}


bool Text::deleteEmptyParagraphMechanism(Cursor & cur,
		Cursor & old, bool & need_anchor_change)
{
	//LYXERR(Debug::DEBUG, "DEPM: cur:\n" << cur << "old:\n" << old);

	Paragraph & oldpar = old.paragraph();
	bool const trackChanges = cur.buffer()->params().track_changes;
	bool result = false;

	// We do nothing if cursor did not move
	if (cur.top() == old.top())
		return false;

	// We do not do anything on read-only documents
	if (cur.buffer()->isReadonly())
		return false;

	// Whether a common inset is found and whether the cursor is still in
	// the same paragraph (possibly nested).
	int const depth = cur.find(&old.inset());
	bool const same_par = depth != -1 && old.idx() == cur[depth].idx()
		&& old.pit() == cur[depth].pit();

	/*
	 * (1) If the chars around the old cursor were spaces and the
	 * paragraph is not in free spacing mode, delete some of them, but
	 * only if the cursor has really moved.
	 */

	/* There are still some small problems that can lead to
	   double spaces stored in the document file or space at
	   the beginning of paragraphs(). This happens if you have
	   the cursor between two spaces and then save. Or if you
	   cut and paste and the selection has a space at the
	   beginning and then save right after the paste. (Lgb)
	*/
	if (!oldpar.isFreeSpacing()) {
		// find range of spaces around cursors
		pos_type from = old.pos();
		while (from > 0
			   && oldpar.isLineSeparator(from - 1)
			   && !oldpar.isDeleted(from - 1))
			--from;
		pos_type to = old.pos();
		while (to < old.lastpos()
			   && oldpar.isLineSeparator(to)
			   && !oldpar.isDeleted(to))
			++to;

		int num_spaces = to - from;
		// If we are not at the start of the paragraph, keep one space
		if (from != to && from > 0)
			--num_spaces;

		// If cursor is inside range, keep one additional space
		if (same_par && cur.pos() > from && cur.pos() < to)
			--num_spaces;

		// Remove spaces and adapt cursor.
		if (num_spaces > 0) {
			old.recordUndo();
			int const deleted =
				deleteSpaces(oldpar, from, to, num_spaces, trackChanges);
			// correct cur position
			// FIXME: there can be other cursors pointing there, we should update them
			if (same_par) {
				if (cur[depth].pos() >= to)
					cur[depth].pos() -= deleted;
				else if (cur[depth].pos() > from)
					cur[depth].pos() = min(from + 1, old.lastpos());
				need_anchor_change = true;
			}
			result = true;
		}
	}

	/*
	 * (2) If the paragraph where the cursor was is empty, delete it
	 */

	// only do our other magic if we changed paragraph
	if (same_par)
		return result;

	// only do our magic if the paragraph is empty
	if (!oldpar.empty())
		return result;

	// don't delete anything if this is the ONLY paragraph!
	if (old.lastpit() == 0)
		return result;

	// Do not delete empty paragraphs with keepempty set.
	if (oldpar.allowEmpty())
		return result;

	// Delete old par.
	old.recordUndo(max(old.pit() - 1, pit_type(0)),
	               min(old.pit() + 1, old.lastpit()));
	ParagraphList & plist = old.text()->paragraphs();
	bool const soa = oldpar.params().startOfAppendix();
	plist.erase(plist.iterator_at(old.pit()));
	// do not lose start of appendix marker (bug 4212)
	if (soa && old.pit() < pit_type(plist.size()))
		plist[old.pit()].params().startOfAppendix(true);

	// see #warning (FIXME?) above
	if (cur.depth() >= old.depth()) {
		CursorSlice & curslice = cur[old.depth() - 1];
		if (&curslice.inset() == &old.inset()
		    && curslice.idx() == old.idx()
		    && curslice.pit() > old.pit()) {
			--curslice.pit();
			// since a paragraph has been deleted, all the
			// insets after `old' have been copied and
			// their address has changed. Therefore we
			// need to `regenerate' cur. (JMarc)
			cur.updateInsets(&(cur.bottom().inset()));
			need_anchor_change = true;
		}
	}

	return true;
}


void Text::deleteEmptyParagraphMechanism(pit_type first, pit_type last, bool trackChanges)
{
	pos_type last_pos = pars_[last].size() - 1;
	deleteEmptyParagraphMechanism(first, last, 0, last_pos, trackChanges);
}


void Text::deleteEmptyParagraphMechanism(pit_type first, pit_type last,
					 pos_type first_pos, pos_type last_pos,
					 bool trackChanges)
{
	LASSERT(first >= 0 && first <= last && last < (int) pars_.size(), return);

	for (pit_type pit = first; pit <= last; ++pit) {
		Paragraph & par = pars_[pit];

		/*
		 * (1) Delete consecutive spaces
		 */
		if (!par.isFreeSpacing()) {
			pos_type from = (pit == first) ? first_pos : 0;
			pos_type to_pos = (pit == last) ? last_pos + 1 : par.size();
			while (from < to_pos) {
				// skip non-spaces
				while (from < par.size()
					   && (!par.isLineSeparator(from) || par.isDeleted(from)))
					++from;
				// find string of spaces
				pos_type to = from;
				while (to < par.size()
					   && par.isLineSeparator(to) && !par.isDeleted(to))
					++to;
				// empty? We are done
				if (from == to)
					break;

				int num_spaces = to - from;

				// If we are not at the extremity of the paragraph, keep one space
				if (from != to && from > 0 && to < par.size())
					--num_spaces;

				// Remove spaces if needed
				int const deleted = deleteSpaces(par, from , to, num_spaces, trackChanges);
				from = to - deleted;
			}
		}

		/*
		 * (2) Delete empty pragraphs
		 */

		// don't delete anything if this is the only remaining paragraph
		// within the given range. Note: Text::acceptOrRejectChanges()
		// sets the cursor to 'first' after calling DEPM
		if (first == last)
			continue;

		// don't delete empty paragraphs with keepempty set
		if (par.allowEmpty())
			continue;

		if (par.empty() || (par.size() == 1 && par.isLineSeparator(0))) {
			pars_.erase(pars_.iterator_at(pit));
			--pit;
			--last;
			continue;
		}
	}
}


namespace {

// globals...
typedef limited_stack<pair<docstring, Font>> FontStack;
static FontStack freeFonts(15);
static bool toggleall = false;

void toggleAndShow(Cursor & cur, Text * text,
	Font const & font, bool togall = true)
{
	text->toggleFree(cur, font, togall);

	if (font.language() != ignore_language ||
	    font.fontInfo().number() != FONT_IGNORE) {
		TextMetrics const & tm = cur.bv().textMetrics(text);
		if (cur.boundary() != tm.isRTLBoundary(cur.pit(), cur.pos(),
						       cur.real_current_font))
			text->setCursor(cur, cur.pit(), cur.pos(),
					false, !cur.boundary());
		if (font.language() != ignore_language)
			// We need a buffer update if we change the language
			// (e.g., with info insets or if the selection contains
			// a par label)
			cur.forceBufferUpdate();
	}
}


void moveCursor(Cursor & cur, bool selecting)
{
	if (selecting || cur.mark())
		cur.setSelection();
}


void finishChange(Cursor & cur, bool selecting)
{
	cur.finishUndo();
	moveCursor(cur, selecting);
}


void mathDispatch(Cursor & cur, FuncRequest const & cmd)
{
	cur.recordUndo();
	docstring sel = cur.selectionAsString(false);

	// It may happen that sel is empty but there is a selection
	replaceSelection(cur);

	// Is this a valid formula?
	bool valid = true;

	if (sel.empty()) {
#ifdef ENABLE_ASSERTIONS
		const int old_pos = cur.pos();
#endif
		cur.insert(new InsetMathHull(cur.buffer(), hullSimple));
#ifdef ENABLE_ASSERTIONS
		LATTEST(old_pos == cur.pos());
#endif
		cur.nextInset()->edit(cur, true);
		if (cmd.action() != LFUN_MATH_MODE)
			// LFUN_MATH_MODE has a different meaning in math mode
			cur.dispatch(cmd);
	} else {
		InsetMathHull * formula = new InsetMathHull(cur.buffer());
		string const selstr = to_utf8(sel);
		istringstream is(selstr);
		Lexer lex;
		lex.setStream(is);
		if (!formula->readQuiet(lex)) {
			// No valid formula, let's try with delims
			is.str("$" + selstr + "$");
			lex.setStream(is);
			if (!formula->readQuiet(lex)) {
				// Still not valid, leave it as is
				valid = false;
				delete formula;
				cur.insert(sel);
			}
		}
		if (valid) {
			cur.insert(formula);
			cur.nextInset()->edit(cur, true);
			LASSERT(cur.inMathed(), return);
			cur.pos() = 0;
			cur.resetAnchor();
			cur.selection(true);
			cur.pos() = cur.lastpos();
			if (cmd.action() != LFUN_MATH_MODE)
				// LFUN_MATH_MODE has a different meaning in math mode
				cur.dispatch(cmd);
			cur.clearSelection();
			cur.pos() = cur.lastpos();
		}
	}
	if (valid)
		cur.message(from_utf8(N_("Math editor mode")));
	else
		cur.message(from_utf8(N_("No valid math formula")));
}


void regexpDispatch(Cursor & cur, FuncRequest const & cmd)
{
	LASSERT(cmd.action() == LFUN_REGEXP_MODE, return);
	if (cur.inRegexped()) {
		cur.message(_("Already in regular expression mode"));
		return;
	}
	cur.recordUndo();
	docstring sel = cur.selectionAsString(false);

	// It may happen that sel is empty but there is a selection
	replaceSelection(cur);

	cur.insert(new InsetMathHull(cur.buffer(), hullRegexp));
	cur.nextInset()->edit(cur, true);
	cur.niceInsert(sel);

	cur.message(_("Regexp editor mode"));
}


void specialChar(Cursor & cur, InsetSpecialChar::Kind kind)
{
	cur.recordUndo();
	cap::replaceSelection(cur);
	cur.insert(new InsetSpecialChar(kind));
	cur.posForward();
}


void ipaChar(Cursor & cur, InsetIPAChar::Kind kind)
{
	cur.recordUndo();
	cap::replaceSelection(cur);
	cur.insert(new InsetIPAChar(kind));
	cur.posForward();
}


bool doInsertInset(Cursor & cur, Text * text,
			  FuncRequest const & cmd, bool edit,
			  bool pastesel, bool resetfont = false)
{
	Buffer & buffer = cur.bv().buffer();
	BufferParams const & bparams = buffer.params();
	Inset * inset = createInset(&buffer, cmd);
	if (!inset)
		return false;

	if (InsetCollapsible * ci = inset->asInsetCollapsible())
		ci->setButtonLabel();

	cur.recordUndo();
	if (cmd.action() == LFUN_ARGUMENT_INSERT) {
		bool cotextinsert = false;
		InsetArgument * const ia = static_cast<InsetArgument *>(inset);
		Layout const & lay = cur.paragraph().layout();
		Layout::LaTeXArgMap args = lay.args();
		Layout::LaTeXArgMap::const_iterator const lait = args.find(ia->name());
		if (lait != args.end())
			cotextinsert = (*lait).second.insertcotext;
		else {
			InsetLayout const & il = cur.inset().getLayout();
			args = il.args();
			Layout::LaTeXArgMap::const_iterator const ilait = args.find(ia->name());
			if (ilait != args.end())
				cotextinsert = (*ilait).second.insertcotext;
		}
		// The argument requests to insert a copy of the co-text to the inset
		if (cotextinsert) {
			docstring ds;
			// If we have a selection within a paragraph, use this
			if (cur.selection() && cur.selBegin().pit() == cur.selEnd().pit())
				ds = cur.selectionAsString(false);
			// else use the whole paragraph
			else
				ds = cur.paragraph().asString();
			text->insertInset(cur, inset);
			ia->init(cur.paragraph());
			if (edit)
				inset->edit(cur, true);
			// Now put co-text into inset
			Font const f(inherit_font, cur.current_font.language());
			if (!ds.empty()) {
				cur.text()->insertStringAsLines(cur, ds, f);
				cur.leaveInset(*inset);
			}
			return true;
		}
	}

	bool gotsel = false;
	bool move_layout = false;
	if (cur.selection()) {
		if (cmd.action() == LFUN_INDEX_INSERT)
			copySelectionToTemp(cur);
		else {
			cutSelectionToTemp(cur, pastesel);
			/* Move layout information inside the inset if the whole
			 * paragraph and the inset allows setting layout
			 * FIXME: figure out a good test in the environment case (see #12251).
			 */
			if (cur.paragraph().layout().isCommand()
			     && (cur.paragraph().empty()
				 || cur.paragraph().isDeleted(0, cur.paragraph().size()))
			     && !inset->forcePlainLayout()) {
				cur.paragraph().setPlainOrDefaultLayout(bparams.documentClass());
				move_layout = true;
			}
		}
		cur.clearSelection();
		gotsel = true;
	} else if (cmd.action() == LFUN_INDEX_INSERT) {
		gotsel = text->selectWordWhenUnderCursor(cur, WHOLE_WORD);
		copySelectionToTemp(cur);
		cur.clearSelection();
	}
	text->insertInset(cur, inset);

	InsetText * inset_text = inset->asInsetText();
	if (inset_text) {
		Font const & font = inset->inheritFont()
			? cur.bv().textMetrics(text).displayFont(cur.pit(), cur.pos())
			: bparams.getFont();
		inset_text->setOuterFont(cur.bv(), font.fontInfo());
	}

	if (cmd.action() == LFUN_ARGUMENT_INSERT) {
		InsetArgument * const ia = static_cast<InsetArgument *>(inset);
		ia->init(cur.paragraph());
	}

	if (edit)
		inset->edit(cur, true);

	if (!gotsel || !pastesel)
		return true;

	pasteFromTemp(cur, cur.buffer()->errorList("Paste"));
	cur.buffer()->errors("Paste");
	cur.clearSelection(); // bug 393
	cur.finishUndo();
	if (inset_text) {
		if (resetfont) {
			// Reset of font (not language) is requested.
			// Used by InsetIndex (#11961).
			Language const * lang = cur.getFont().language();
			Font font(bparams.getFont().fontInfo(), lang);
			cur.paragraph().resetFonts(font);
		}
		inset_text->fixParagraphsFont();
		cur.pos() = 0;
		cur.pit() = 0;
		/* If the containing paragraph has kept its layout, reset the
		 * layout of the first paragraph of the inset.
		 */
		if (!move_layout)
			cur.paragraph().setPlainOrDefaultLayout(bparams.documentClass());
		// FIXME: what does this do?
		if (cmd.action() == LFUN_FLEX_INSERT)
			return true;
		Cursor old = cur;
		cur.leaveInset(*inset);
		if (cmd.action() == LFUN_PREVIEW_INSERT
			|| cmd.action() == LFUN_IPA_INSERT)
			// trigger preview
			notifyCursorLeavesOrEnters(old, cur);
	} else {
		cur.leaveInset(*inset);
		// reset surrounding par to default
		DocumentClass const & dc = bparams.documentClass();
		docstring const layoutname = inset->usePlainLayout()
			? dc.plainLayoutName()
			: dc.defaultLayoutName();
		text->setLayout(cur, layoutname);
	}
	return true;
}


/// the type of outline operation
enum OutlineOp {
	OutlineUp, // Move this header with text down
	OutlineDown,   // Move this header with text up
	OutlineIn, // Make this header deeper
	OutlineOut // Make this header shallower
};


void insertSeparator(Cursor const & cur, depth_type const depth)
{
	Buffer & buf = *cur.buffer();
	lyx::dispatch(FuncRequest(LFUN_PARAGRAPH_BREAK));
	DocumentClass const & tc = buf.params().documentClass();
	lyx::dispatch(FuncRequest(LFUN_LAYOUT, from_ascii("\"") + tc.plainLayout().name()
				  + from_ascii("\" ignoreautonests")));
	// FIXME: Bibitem mess!
	if (cur.prevInset() && cur.prevInset()->lyxCode() == BIBITEM_CODE)
		lyx::dispatch(FuncRequest(LFUN_CHAR_DELETE_BACKWARD));
	lyx::dispatch(FuncRequest(LFUN_SEPARATOR_INSERT, "plain"));
	while (cur.paragraph().params().depth() > depth)
		lyx::dispatch(FuncRequest(LFUN_DEPTH_DECREMENT));
}


void outline(OutlineOp mode, Cursor & cur, bool local)
{
	Buffer & buf = *cur.buffer();
	Text & text = *cur.text();
	pit_type & pit = cur.pit();
	ParagraphList & pars = text.paragraphs();
	ParagraphList::iterator const bgn = pars.begin();
	// The first paragraph of the area to be copied:
	ParagraphList::iterator start = pars.iterator_at(pit);
	// The final paragraph of area to be copied:
	ParagraphList::iterator finish = start;
	ParagraphList::iterator const end = pars.end();
	depth_type const current_depth = cur.paragraph().params().depth();

	int const thistoclevel = text.getTocLevel(distance(bgn, start));
	int toclevel;

	// Move out (down) from this section header
	if (finish != end)
		++finish;

	if (!local || (mode != OutlineIn && mode != OutlineOut)) {
		// Seek the one (on same level) below
		for (; finish != end; ++finish) {
			toclevel = text.getTocLevel(distance(bgn, finish));
			if (toclevel != Layout::NOT_IN_TOC && toclevel <= thistoclevel)
				break;
		}
	}

	switch (mode) {
		case OutlineUp: {
			if (start == pars.begin())
				// Nothing to move.
				return;
			ParagraphList::iterator dest = start;
			// Move out (up) from this header
			if (dest == bgn)
				return;
			// Search previous same-level header above
			do {
				--dest;
				toclevel = text.getTocLevel(distance(bgn, dest));
			} while(dest != bgn
				&& (toclevel == Layout::NOT_IN_TOC
				    || toclevel > thistoclevel));
			// Not found; do nothing
			if (toclevel == Layout::NOT_IN_TOC || toclevel > thistoclevel)
				return;
			pit_type newpit = distance(bgn, dest);
			pit_type const len = distance(start, finish);
			pit_type const deletepit = pit + len;
			buf.undo().recordUndo(cur, newpit, deletepit - 1);
			// If we move an environment upwards, make sure it is
			// separated from its new neighbour below:
			// If an environment of the same layout follows, and the moved
			// paragraph sequence does not end with a separator, insert one.
			ParagraphList::iterator lastmoved = finish;
			--lastmoved;
			if (start->layout().isEnvironment()
			    && dest->layout() == start->layout()
			    && !lastmoved->isEnvSeparator(lastmoved->beginOfBody())) {
				cur.pit() = distance(bgn, lastmoved);
				cur.pos() = cur.lastpos();
				insertSeparator(cur, current_depth);
				cur.pit() = pit;
			}
			// Likewise, if we moved an environment upwards, make sure it
			// is separated from its new neighbour above.
			// The paragraph before the target of movement
			if (dest != bgn) {
				ParagraphList::iterator before = dest;
				--before;
				// Get the parent paragraph (outer in nested context)
				pit_type const parent =
					before->params().depth() > current_depth
						? text.depthHook(distance(bgn, before), current_depth)
						: distance(bgn, before);
				// If a environment with same layout preceeds the moved one in the new
				// position, and there is no separator yet, insert one.
				if (start->layout().isEnvironment()
				    && pars[parent].layout() == start->layout()
				    && !before->isEnvSeparator(before->beginOfBody())) {
					cur.pit() = distance(bgn, before);
					cur.pos() = cur.lastpos();
					insertSeparator(cur, current_depth);
					cur.pit() = pit;
				}
			}
			newpit = distance(bgn, dest);
			pars.splice(dest, start, finish);
			cur.pit() = newpit;
			break;
		}
		case OutlineDown: {
			if (finish == end)
				// Nothing to move.
				return;
			// Go one down from *this* header:
			ParagraphList::iterator dest = next(finish, 1);
			// Go further down to find header to insert in front of:
			for (; dest != end; ++dest) {
				toclevel = text.getTocLevel(distance(bgn, dest));
				if (toclevel != Layout::NOT_IN_TOC
				      && toclevel <= thistoclevel)
					break;
			}
			// One such was found, so go on...
			// If we move an environment downwards, make sure it is
			// separated from its new neighbour above.
			pit_type newpit = distance(bgn, dest);
			buf.undo().recordUndo(cur, pit, newpit - 1);
			// The paragraph before the target of movement
			ParagraphList::iterator before = dest;
			--before;
			// Get the parent paragraph (outer in nested context)
			pit_type const parent =
				before->params().depth() > current_depth
					? text.depthHook(distance(bgn, before), current_depth)
					: distance(bgn, before);
			// If a environment with same layout preceeds the moved one in the new
			// position, and there is no separator yet, insert one.
			if (start->layout().isEnvironment()
			    && pars[parent].layout() == start->layout()
			    && !before->isEnvSeparator(before->beginOfBody())) {
				cur.pit() = distance(bgn, before);
				cur.pos() = cur.lastpos();
				insertSeparator(cur, current_depth);
				cur.pit() = pit;
			}
			// Likewise, make sure moved environments are separated
			// from their new neighbour below:
			// If an environment of the same layout follows, and the moved
			// paragraph sequence does not end with a separator, insert one.
			ParagraphList::iterator lastmoved = finish;
			--lastmoved;
			if (dest != end
			    && start->layout().isEnvironment()
			    && dest->layout() == start->layout()
			    && !lastmoved->isEnvSeparator(lastmoved->beginOfBody())) {
				cur.pit() = distance(bgn, lastmoved);
				cur.pos() = cur.lastpos();
				insertSeparator(cur, current_depth);
				cur.pit() = pit;
			}
			newpit = distance(bgn, dest);
			pit_type const len = distance(start, finish);
			pars.splice(dest, start, finish);
			cur.pit() = newpit - len;
			break;
		}
		case OutlineIn:
		case OutlineOut: {
			// We first iterate without actually doing something
			// in order to check whether the action flattens the structure.
			// If so, warn (#11178).
			ParagraphList::iterator cstart = start;
			bool strucchange = false;
			for (; cstart != finish; ++cstart) {
				toclevel = text.getTocLevel(distance(bgn, cstart));
				if (toclevel == Layout::NOT_IN_TOC)
					continue;

				DocumentClass const & tc = buf.params().documentClass();
				int newtoclevel = -1;
				if (mode == OutlineIn) {
					if (toclevel == -1 && tc.getTOCLayout().toclevel > 0)
						// we are at part but don't have a chapter
						newtoclevel = tc.getTOCLayout().toclevel;
					else
						newtoclevel = toclevel + 1;
				} else {
					if (tc.getTOCLayout().toclevel == toclevel && tc.min_toclevel() < toclevel)
						// we are at highest level, but there is still part
						newtoclevel = tc.min_toclevel();
					else
						newtoclevel = toclevel - 1;
				}

				bool found = false;
				for (auto const & lay : tc) {
					if (lay.toclevel == newtoclevel
					    && lay.isNumHeadingLabelType()
					    && cstart->layout().isNumHeadingLabelType()) {
						found = true;
						break;
					}
				}
				if (!found) {
					strucchange = true;
					break;
				}
			}
			if (strucchange
			    && frontend::Alert::prompt(_("Action flattens document structure"),
						       _("This action will cause some headings that have been "
							 "on different level before to be on the same level "
							 "since there is no more lower or higher heading level. "
							 "Continue still?"),
						       1, 1,
						       _("&Yes, continue nonetheless"),
						       _("&No, quit operation")) == 1)
				break;

			pit_type const len = distance(start, finish);
			buf.undo().recordUndo(cur, pit, pit + len - 1);
			for (; start != finish; ++start) {
				toclevel = text.getTocLevel(distance(bgn, start));
				if (toclevel == Layout::NOT_IN_TOC)
					continue;

				DocumentClass const & tc = buf.params().documentClass();
				int newtoclevel = -1;
				if (mode == OutlineIn) {
					if (toclevel == -1 && tc.getTOCLayout().toclevel > 0)
						// we are at part but don't have a chapter
						newtoclevel = tc.getTOCLayout().toclevel;
					else
						newtoclevel = toclevel + 1;
				} else {
					if (tc.getTOCLayout().toclevel == toclevel && tc.min_toclevel() < toclevel)
						// we are at highest level, but there is still part
						newtoclevel = tc.min_toclevel();
					else
						newtoclevel = toclevel - 1;
				}

				for (auto const & lay : tc) {
					if (lay.toclevel == newtoclevel
					    && lay.isNumHeadingLabelType()
					    && start->layout().isNumHeadingLabelType()) {
						start->setLayout(lay);
						break;
					}
				}
			}
			break;
		}
	}
}


} // namespace


void Text::number(Cursor & cur)
{
	FontInfo font = ignore_font;
	font.setNumber(FONT_TOGGLE);
	toggleAndShow(cur, this, Font(font, ignore_language));
}


bool Text::isRTL(pit_type const pit) const
{
	Buffer const & buffer = owner_->buffer();
	return pars_[pit].isRTL(buffer.params());
}


namespace {

Language const * getLanguage(Cursor const & cur, string const & lang)
{
	return lang.empty() ? cur.getFont().language() : languages.getLanguage(lang);
}


docstring resolveLayout(docstring layout, DocIterator const & dit)
{
	Paragraph const & par = dit.paragraph();
	DocumentClass const & tclass = dit.buffer()->params().documentClass();

	if (layout.empty())
		layout = tclass.defaultLayoutName();

	if (dit.inset().forcePlainLayout(dit.idx()))
		// in this case only the empty layout is allowed
		layout = tclass.plainLayoutName();
	else if (par.usePlainLayout()) {
		// in this case, default layout maps to empty layout
		if (layout == tclass.defaultLayoutName())
			layout = tclass.plainLayoutName();
	} else {
		// otherwise, the empty layout maps to the default
		if (layout == tclass.plainLayoutName())
			layout = tclass.defaultLayoutName();
	}

	// If the entry is obsolete, use the new one instead.
	if (tclass.hasLayout(layout)) {
		docstring const & obs = tclass[layout].obsoleted_by();
		if (!obs.empty())
			layout = obs;
	}
	if (!tclass.hasLayout(layout))
		layout.clear();
	return layout;
}


bool isAlreadyLayout(docstring const & layout, CursorData const & cur)
{
	ParagraphList const & pars = cur.text()->paragraphs();

	pit_type pit = cur.selBegin().pit();
	pit_type const epit = cur.selEnd().pit() + 1;
	for ( ; pit != epit; ++pit)
		if (pars[pit].layout().name() != layout)
			return false;

	return true;
}


} // namespace


void Text::dispatch(Cursor & cur, FuncRequest & cmd)
{
	LYXERR(Debug::ACTION, "Text::dispatch: cmd: " << cmd);

	// Dispatch if the cursor is inside the text. It is not the
	// case for context menus (bug 5797).
	if (cur.text() != this) {
		cur.undispatched();
		return;
	}

	BufferView * bv = &cur.bv();
	TextMetrics * tm = &bv->textMetrics(this);
	if (!tm->contains(cur.pit())) {
		lyx::dispatch(FuncRequest(LFUN_SCREEN_SHOW_CURSOR));
		tm = &bv->textMetrics(this);
	}

	// FIXME: We use the update flag to indicates wether a singlePar or a
	// full screen update is needed. We reset it here but shall we restore it
	// at the end?
	cur.noScreenUpdate();

	LBUFERR(this == cur.text());

	// NOTE: This should NOT be a reference. See commit 94a5481a.
	CursorSlice const oldTopSlice = cur.top();
	bool const oldBoundary = cur.boundary();
	bool const oldSelection = cur.selection();
	// Signals that, even if needsUpdate == false, an update of the
	// cursor paragraph is required
	bool singleParUpdate = lyxaction.funcHasFlag(cmd.action(),
		LyXAction::SingleParUpdate);
	// Signals that a full-screen update is required
	bool needsUpdate = !(lyxaction.funcHasFlag(cmd.action(),
		LyXAction::NoUpdate) || singleParUpdate);
	bool const last_misspelled = lyxrc.spellcheck_continuously
		&& cur.paragraph().isMisspelled(cur.pos(), true);

	FuncCode const act = cmd.action();
	switch (act) {

	case LFUN_PARAGRAPH_MOVE_DOWN: {
		pit_type const pit = cur.pit();
		cur.recordUndo(pit, pit + 1);
		pars_.swap(pit, pit + 1);
		needsUpdate = true;
		cur.forceBufferUpdate();
		++cur.pit();
		break;
	}

	case LFUN_PARAGRAPH_MOVE_UP: {
		pit_type const pit = cur.pit();
		cur.recordUndo(pit - 1, pit);
		cur.finishUndo();
		pars_.swap(pit, pit - 1);
		--cur.pit();
		needsUpdate = true;
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_APPENDIX: {
		Paragraph & par = cur.paragraph();
		bool start = !par.params().startOfAppendix();

// FIXME: The code below only makes sense at top level.
// Should LFUN_APPENDIX be restricted to top-level paragraphs?
		// ensure that we have only one start_of_appendix in this document
		// FIXME: this don't work for multipart document!
		for (pit_type tmp = 0, end = pars_.size(); tmp != end; ++tmp) {
			if (pars_[tmp].params().startOfAppendix()) {
				cur.recordUndo(tmp, tmp);
				pars_[tmp].params().startOfAppendix(false);
				break;
			}
		}

		cur.recordUndo();
		par.params().startOfAppendix(start);

		// we can set the refreshing parameters now
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_WORD_DELETE_FORWARD:
		if (cur.selection())
			cutSelection(cur, false);
		else
			deleteWordForward(cur, cmd.getArg(0) != "confirm");
		finishChange(cur, false);
		break;

	case LFUN_WORD_DELETE_BACKWARD:
		if (cur.selection())
			cutSelection(cur, false);
		else
			deleteWordBackward(cur, cmd.getArg(0) != "confirm");
		finishChange(cur, false);
		break;

	case LFUN_LINE_DELETE_FORWARD:
		if (cur.selection())
			cutSelection(cur, false);
		else
			tm->deleteLineForward(cur);
		finishChange(cur, false);
		break;

	case LFUN_BUFFER_BEGIN:
	case LFUN_BUFFER_BEGIN_SELECT:
		needsUpdate |= cur.selHandle(act == LFUN_BUFFER_BEGIN_SELECT);
		if (cur.depth() == 1)
			needsUpdate |= cursorTop(cur);
		else
			cur.undispatched();
		cur.screenUpdateFlags(Update::FitCursor);
		break;

	case LFUN_BUFFER_END:
	case LFUN_BUFFER_END_SELECT:
		needsUpdate |= cur.selHandle(act == LFUN_BUFFER_END_SELECT);
		if (cur.depth() == 1)
			needsUpdate |= cursorBottom(cur);
		else
			cur.undispatched();
		cur.screenUpdateFlags(Update::FitCursor);
		break;

	case LFUN_INSET_BEGIN:
	case LFUN_INSET_BEGIN_SELECT:
		needsUpdate |= cur.selHandle(act == LFUN_INSET_BEGIN_SELECT);
		if (cur.depth() == 1 || !cur.top().at_begin())
			needsUpdate |= cursorTop(cur);
		else
			cur.undispatched();
		cur.screenUpdateFlags(Update::FitCursor);
		break;

	case LFUN_INSET_END:
	case LFUN_INSET_END_SELECT:
		needsUpdate |= cur.selHandle(act == LFUN_INSET_END_SELECT);
		if (cur.depth() == 1 || !cur.top().at_end())
			needsUpdate |= cursorBottom(cur);
		else
			cur.undispatched();
		cur.screenUpdateFlags(Update::FitCursor);
		break;

	case LFUN_CHAR_FORWARD:
	case LFUN_CHAR_FORWARD_SELECT: {
		//LYXERR0(" LFUN_CHAR_FORWARD[SEL]:\n" << cur);
		needsUpdate |= cur.selHandle(act == LFUN_CHAR_FORWARD_SELECT);
		bool const cur_moved = cursorForward(cur);
		needsUpdate |= cur_moved;

		if (!cur_moved && cur.depth() > 1
		     && oldTopSlice == cur.top() && cur.boundary() == oldBoundary) {
			cur.undispatched();
			cmd = FuncRequest(LFUN_FINISHED_FORWARD);

			// we will be moving out the inset, so we should execute
			// the depm-mechanism.
			// The cursor hasn't changed yet. To give the DEPM the
			// possibility of doing something we must provide it with
			// two different cursors.
			Cursor dummy = cur;
			dummy.pos() = dummy.pit() = 0;
			if (cur.bv().checkDepm(dummy, cur))
				cur.forceBufferUpdate();
		}
		break;
	}

	case LFUN_CHAR_BACKWARD:
	case LFUN_CHAR_BACKWARD_SELECT: {
		//lyxerr << "handle LFUN_CHAR_BACKWARD[_SELECT]:\n" << cur << endl;
		needsUpdate |= cur.selHandle(act == LFUN_CHAR_BACKWARD_SELECT);
		bool const cur_moved = cursorBackward(cur);
		needsUpdate |= cur_moved;

		if (!cur_moved && cur.depth() > 1
		     && oldTopSlice == cur.top() && cur.boundary() == oldBoundary) {
			cur.undispatched();
			cmd = FuncRequest(LFUN_FINISHED_BACKWARD);

			// we will be moving out the inset, so we should execute
			// the depm-mechanism.
			// The cursor hasn't changed yet. To give the DEPM the
			// possibility of doing something we must provide it with
			// two different cursors.
			Cursor dummy = cur;
			dummy.pos() = cur.lastpos();
			dummy.pit() = cur.lastpit();
			if (cur.bv().checkDepm(dummy, cur))
				cur.forceBufferUpdate();
		}
		break;
	}

	case LFUN_CHAR_LEFT:
	case LFUN_CHAR_LEFT_SELECT:
		if (lyxrc.visual_cursor) {
			needsUpdate |= cur.selHandle(act == LFUN_CHAR_LEFT_SELECT);
			bool const cur_moved = cursorVisLeft(cur);
			needsUpdate |= cur_moved;
			if (!cur_moved && cur.depth() > 1
			     && oldTopSlice == cur.top() && cur.boundary() == oldBoundary) {
				cur.undispatched();
				cmd = FuncRequest(LFUN_FINISHED_LEFT);
			}
		} else {
			if (cur.reverseDirectionNeeded()) {
				cmd.setAction(cmd.action() == LFUN_CHAR_LEFT_SELECT ?
					LFUN_CHAR_FORWARD_SELECT : LFUN_CHAR_FORWARD);
			} else {
				cmd.setAction(cmd.action() == LFUN_CHAR_LEFT_SELECT ?
					LFUN_CHAR_BACKWARD_SELECT : LFUN_CHAR_BACKWARD);
			}
			dispatch(cur, cmd);
			return;
		}
		break;

	case LFUN_CHAR_RIGHT:
	case LFUN_CHAR_RIGHT_SELECT:
		if (lyxrc.visual_cursor) {
			needsUpdate |= cur.selHandle(cmd.action() == LFUN_CHAR_RIGHT_SELECT);
			bool const cur_moved = cursorVisRight(cur);
			needsUpdate |= cur_moved;
			if (!cur_moved && cur.depth() > 1
			     && oldTopSlice == cur.top() && cur.boundary() == oldBoundary) {
				cur.undispatched();
				cmd = FuncRequest(LFUN_FINISHED_RIGHT);
			}
		} else {
			if (cur.reverseDirectionNeeded()) {
				cmd.setAction(cmd.action() == LFUN_CHAR_RIGHT_SELECT ?
					LFUN_CHAR_BACKWARD_SELECT : LFUN_CHAR_BACKWARD);
			} else {
				cmd.setAction(cmd.action() == LFUN_CHAR_RIGHT_SELECT ?
					LFUN_CHAR_FORWARD_SELECT : LFUN_CHAR_FORWARD);
			}
			dispatch(cur, cmd);
			return;
		}
		break;


	case LFUN_UP_SELECT:
	case LFUN_DOWN_SELECT:
	case LFUN_UP:
	case LFUN_DOWN: {
		// stop/start the selection
		bool select = cmd.action() == LFUN_DOWN_SELECT ||
			cmd.action() == LFUN_UP_SELECT;

		// move cursor up/down
		bool up = cmd.action() == LFUN_UP_SELECT || cmd.action() == LFUN_UP;
		bool const atFirstOrLastRow = cur.atFirstOrLastRow(up);

		if (!atFirstOrLastRow) {
			needsUpdate |= cur.selHandle(select);
			needsUpdate |= cur.upDownInText(up);
			needsUpdate |= cur.beforeDispatchCursor().inMathed();
		} else {
			pos_type newpos = up ? 0 : cur.lastpos();
			if (lyxrc.mac_like_cursor_movement && cur.pos() != newpos) {
				needsUpdate |= cur.selHandle(select);
				// we do not reset the targetx of the cursor
				cur.pos() = newpos;
				if (bv->checkDepm(cur, bv->cursor())) {
					needsUpdate = true;
					cur.forceBufferUpdate();
				}
				cur.updateTextTargetOffset();
				break;
			}

			// if the cursor cannot be moved up or down do not remove
			// the selection right now, but wait for the next dispatch.
			if (select)
				needsUpdate |= cur.selHandle(select);
			needsUpdate |= cur.upDownInText(up);
			cur.undispatched();
		}

		break;
	}

	case LFUN_PARAGRAPH_SELECT:
		if (cur.pos() > 0)
			needsUpdate |= setCursor(cur, cur.pit(), 0);
		needsUpdate |= cur.selHandle(true);
		if (cur.pos() < cur.lastpos())
			needsUpdate |= setCursor(cur, cur.pit(), cur.lastpos());
		break;

	case LFUN_PARAGRAPH_UP:
	case LFUN_PARAGRAPH_UP_SELECT:
		needsUpdate |= cur.selHandle(cmd.action() == LFUN_PARAGRAPH_UP_SELECT);
		needsUpdate |= cursorUpParagraph(cur);
		break;

	case LFUN_PARAGRAPH_DOWN:
	case LFUN_PARAGRAPH_DOWN_SELECT:
		needsUpdate |= cur.selHandle(cmd.action() == LFUN_PARAGRAPH_DOWN_SELECT);
		needsUpdate |= cursorDownParagraph(cur);
		break;

	case LFUN_LINE_BEGIN:
	case LFUN_LINE_BEGIN_SELECT:
		needsUpdate |= cur.selHandle(cmd.action() == LFUN_LINE_BEGIN_SELECT);
		needsUpdate |= tm->cursorHome(cur);
		break;

	case LFUN_LINE_END:
	case LFUN_LINE_END_SELECT:
		needsUpdate |= cur.selHandle(cmd.action() == LFUN_LINE_END_SELECT);
		needsUpdate |= tm->cursorEnd(cur);
		break;

	case LFUN_SECTION_SELECT: {
		Buffer const & buf = *cur.buffer();
		pit_type const pit = cur.pit();
		ParagraphList & pars = buf.text().paragraphs();
		ParagraphList::iterator bgn = pars.begin();
		// The first paragraph of the area to be selected:
		ParagraphList::iterator start = pars.iterator_at(pit);
		// The final paragraph of area to be selected:
		ParagraphList::iterator finish = start;
		ParagraphList::iterator end = pars.end();

		int const thistoclevel = buf.text().getTocLevel(distance(bgn, start));
		if (thistoclevel == Layout::NOT_IN_TOC)
			break;

		cur.pos() = 0;
		Cursor const old_cur = cur;
		needsUpdate |= cur.selHandle(true);

		// Move out (down) from this section header
		if (finish != end)
			++finish;

		// Seek the one (on same level) below
		for (; finish != end; ++finish, ++cur.pit()) {
			int const toclevel = buf.text().getTocLevel(distance(bgn, finish));
			if (toclevel != Layout::NOT_IN_TOC && toclevel <= thistoclevel)
				break;
		}
		cur.pos() = cur.lastpos();
		cur.boundary(false);
		cur.setCurrentFont();

		needsUpdate |= cur != old_cur;
		break;
	}

	case LFUN_WORD_RIGHT:
	case LFUN_WORD_RIGHT_SELECT:
		if (lyxrc.visual_cursor) {
			needsUpdate |= cur.selHandle(cmd.action() == LFUN_WORD_RIGHT_SELECT);
			bool const cur_moved = cursorVisRightOneWord(cur);
			needsUpdate |= cur_moved;
			if (!cur_moved && cur.depth() > 1
			     && oldTopSlice == cur.top() && cur.boundary() == oldBoundary) {
				cur.undispatched();
				cmd = FuncRequest(LFUN_FINISHED_RIGHT);
			}
		} else {
			if (cur.reverseDirectionNeeded()) {
				cmd.setAction(cmd.action() == LFUN_WORD_RIGHT_SELECT ?
						LFUN_WORD_BACKWARD_SELECT : LFUN_WORD_BACKWARD);
			} else {
				cmd.setAction(cmd.action() == LFUN_WORD_RIGHT_SELECT ?
						LFUN_WORD_FORWARD_SELECT : LFUN_WORD_FORWARD);
			}
			dispatch(cur, cmd);
			return;
		}
		break;

	case LFUN_WORD_FORWARD:
	case LFUN_WORD_FORWARD_SELECT: {
		needsUpdate |= cur.selHandle(cmd.action() == LFUN_WORD_FORWARD_SELECT);
		bool const cur_moved = cursorForwardOneWord(cur);
		needsUpdate |= cur_moved;

		if (!cur_moved && cur.depth() > 1
		     && oldTopSlice == cur.top() && cur.boundary() == oldBoundary) {
			cur.undispatched();
			cmd = FuncRequest(LFUN_FINISHED_FORWARD);

			// we will be moving out the inset, so we should execute
			// the depm-mechanism.
			// The cursor hasn't changed yet. To give the DEPM the
			// possibility of doing something we must provide it with
			// two different cursors.
			Cursor dummy = cur;
			dummy.pos() = dummy.pit() = 0;
			if (cur.bv().checkDepm(dummy, cur))
				cur.forceBufferUpdate();
		}
		break;
	}

	case LFUN_WORD_LEFT:
	case LFUN_WORD_LEFT_SELECT:
		if (lyxrc.visual_cursor) {
			needsUpdate |= cur.selHandle(cmd.action() == LFUN_WORD_LEFT_SELECT);
			bool const cur_moved = cursorVisLeftOneWord(cur);
			needsUpdate |= cur_moved;
			if (!cur_moved && cur.depth() > 1
			     && oldTopSlice == cur.top() && cur.boundary() == oldBoundary) {
				cur.undispatched();
				cmd = FuncRequest(LFUN_FINISHED_LEFT);
			}
		} else {
			if (cur.reverseDirectionNeeded()) {
				cmd.setAction(cmd.action() == LFUN_WORD_LEFT_SELECT ?
						LFUN_WORD_FORWARD_SELECT : LFUN_WORD_FORWARD);
			} else {
				cmd.setAction(cmd.action() == LFUN_WORD_LEFT_SELECT ?
						LFUN_WORD_BACKWARD_SELECT : LFUN_WORD_BACKWARD);
			}
			dispatch(cur, cmd);
			return;
		}
		break;

	case LFUN_WORD_BACKWARD:
	case LFUN_WORD_BACKWARD_SELECT: {
		needsUpdate |= cur.selHandle(cmd.action() == LFUN_WORD_BACKWARD_SELECT);
		bool const cur_moved = cursorBackwardOneWord(cur);
		needsUpdate |= cur_moved;

		if (!cur_moved && cur.depth() > 1
		     && oldTopSlice == cur.top() && cur.boundary() == oldBoundary) {
			cur.undispatched();
			cmd = FuncRequest(LFUN_FINISHED_BACKWARD);

			// we will be moving out the inset, so we should execute
			// the depm-mechanism.
			// The cursor hasn't changed yet. To give the DEPM the
			// possibility of doing something we must provide it with
			// two different cursors.
			Cursor dummy = cur;
			dummy.pos() = cur.lastpos();
			dummy.pit() = cur.lastpit();
			if (cur.bv().checkDepm(dummy, cur))
				cur.forceBufferUpdate();
		}
		break;
	}

	case LFUN_WORD_SELECT: {
		selectWord(cur, WHOLE_WORD);
		finishChange(cur, true);
		break;
	}

	case LFUN_NEWLINE_INSERT: {
		InsetNewlineParams inp;
		docstring const & arg = cmd.argument();
		if (arg == "linebreak")
			inp.kind = InsetNewlineParams::LINEBREAK;
		else
			inp.kind = InsetNewlineParams::NEWLINE;
		cap::replaceSelection(cur);
		cur.recordUndo();
		cur.insert(new InsetNewline(inp));
		cur.posForward();
		moveCursor(cur, false);
		break;
	}

	case LFUN_TAB_INSERT: {
		bool const multi_par_selection = cur.selection() &&
			cur.selBegin().pit() != cur.selEnd().pit();
		if (multi_par_selection) {
			// If there is a multi-paragraph selection, a tab is inserted
			// at the beginning of each paragraph.
			cur.recordUndoSelection();
			pit_type const pit_end = cur.selEnd().pit();
			for (pit_type pit = cur.selBegin().pit(); pit <= pit_end; pit++) {
				pars_[pit].insertChar(0, '\t',
						      bv->buffer().params().track_changes);
				// Update the selection pos to make sure the selection does not
				// change as the inserted tab will increase the logical pos.
				if (cur.realAnchor().pit() == pit)
					cur.realAnchor().forwardPos();
				if (cur.pit() == pit)
					cur.forwardPos();
			}
			cur.finishUndo();
		} else {
			// Maybe we shouldn't allow tabs within a line, because they
			// are not (yet) aligned as one might do expect.
			FuncRequest ncmd(LFUN_SELF_INSERT, from_ascii("\t"));
			dispatch(cur, ncmd);
		}
		break;
	}

	case LFUN_TAB_DELETE: {
		bool const tc = bv->buffer().params().track_changes;
		if (cur.selection()) {
			// If there is a selection, a tab (if present) is removed from
			// the beginning of each paragraph.
			cur.recordUndoSelection();
			pit_type const pit_end = cur.selEnd().pit();
			for (pit_type pit = cur.selBegin().pit(); pit <= pit_end; pit++) {
				Paragraph & par = paragraphs()[pit];
				if (par.empty())
					continue;
				char_type const c = par.getChar(0);
				if (c == '\t' || c == ' ') {
					// remove either 1 tab or 4 spaces.
					int const n = (c == ' ' ? 4 : 1);
					for (int i = 0; i < n
						  && !par.empty() && par.getChar(0) == c; ++i) {
						if (cur.pit() == pit)
							cur.posBackward();
						if (cur.realAnchor().pit() == pit
							  && cur.realAnchor().pos() > 0 )
							cur.realAnchor().backwardPos();
						par.eraseChar(0, tc);
					}
				}
			}
			cur.finishUndo();
		} else {
			// If there is no selection, try to remove a tab or some spaces
			// before the position of the cursor.
			Paragraph & par = paragraphs()[cur.pit()];
			pos_type const pos = cur.pos();

			if (pos == 0)
				break;

			char_type const c = par.getChar(pos - 1);
			cur.recordUndo();
			if (c == '\t') {
				cur.posBackward();
				par.eraseChar(cur.pos(), tc);
			} else
				for (int n_spaces = 0;
				     cur.pos() > 0
					     && par.getChar(cur.pos() - 1) == ' '
					     && n_spaces < 4;
				     ++n_spaces) {
					cur.posBackward();
					par.eraseChar(cur.pos(), tc);
				}
			cur.finishUndo();
		}
		break;
	}

	case LFUN_CHAR_DELETE_FORWARD:
		if (!cur.selection()) {
			if (cur.pos() == cur.paragraph().size())
				// Par boundary, force full-screen update
				singleParUpdate = false;
			else if (cmd.getArg(0) == "confirm" && cur.confirmDeletion()) {
				cur.resetAnchor();
				cur.selection(true);
				cur.posForward();
				cur.setSelection();
				break;
			}
			needsUpdate |= erase(cur);
			cur.resetAnchor();
		} else {
			cutSelection(cur, false);
			cur.setCurrentFont();
			singleParUpdate = false;
		}
		moveCursor(cur, false);
		break;

	case LFUN_CHAR_DELETE_BACKWARD:
		if (!cur.selection()) {
			if (bv->getIntl().getTransManager().backspace()) {
				bool par_boundary = cur.pos() == 0;
				bool first_par = cur.pit() == 0;
				// Par boundary, full-screen update
				if (par_boundary)
					singleParUpdate = false;
				else if (cmd.getArg(0) == "confirm" && cur.confirmDeletion(true)) {
					cur.resetAnchor();
					cur.selection(true);
					cur.posBackward();
					cur.setSelection();
					break;
				}
				needsUpdate |= backspace(cur);
				cur.resetAnchor();
				if (par_boundary && !first_par && cur.pos() > 0
				    && cur.paragraph().isEnvSeparator(cur.pos() - 1)) {
					needsUpdate |= backspace(cur);
					cur.resetAnchor();
				}
			}
		} else {
			DocIterator const dit = cur.selectionBegin();
			cutSelection(cur, false);
			if (cur.buffer()->params().track_changes)
				// since we're doing backwards deletion,
				// and the selection is not really cut,
				// move cursor before selection (#11630)
				cur.setCursor(dit);
			cur.setCurrentFont();
			singleParUpdate = false;
		}
		break;

	case LFUN_PARAGRAPH_BREAK: {
		cap::replaceSelection(cur);
		pit_type pit = cur.pit();
		Paragraph const & par = pars_[pit];
		bool lastpar = (pit == pit_type(pars_.size() - 1));
		Paragraph const & nextpar = lastpar ? par : pars_[pit + 1];
		pit_type prev = pit > 0 ? depthHook(pit, par.getDepth()) : pit;
		if (prev < pit && cur.pos() == par.beginOfBody()
		    && par.empty() && !par.isEnvSeparator(cur.pos())
		    && !par.layout().keepempty
		    && !par.layout().isCommand()
		    && pars_[prev].layout() != par.layout()
		    && pars_[prev].layout().isEnvironment()
		    && !nextpar.isEnvSeparator(nextpar.beginOfBody())) {
			if (par.layout().isEnvironment()
			    && pars_[prev].getDepth() == par.getDepth()) {
				docstring const layout = par.layout().name();
				DocumentClass const & tc = bv->buffer().params().documentClass();
				lyx::dispatch(FuncRequest(LFUN_LAYOUT, tc.plainLayout().name()));
				lyx::dispatch(FuncRequest(LFUN_SEPARATOR_INSERT, "plain"));
				lyx::dispatch(FuncRequest(LFUN_PARAGRAPH_BREAK, "inverse"));
				lyx::dispatch(FuncRequest(LFUN_LAYOUT, layout));
			} else {
				lyx::dispatch(FuncRequest(LFUN_SEPARATOR_INSERT, "plain"));
				breakParagraph(cur);
			}
			Font const f(inherit_font, cur.current_font.language());
			pars_[cur.pit() - 1].resetFonts(f);
		} else {
			if (par.isEnvSeparator(cur.pos()) && cmd.getArg(1) != "ignoresep")
				cur.posForward();
			breakParagraph(cur, cmd.getArg(0) == "inverse");
		}
		cur.resetAnchor();
		// If we have a list and autoinsert item insets,
		// insert them now.
		Layout::LaTeXArgMap args = par.layout().args();
		for (auto const & thearg : args) {
			Layout::latexarg arg = thearg.second;
			if (arg.autoinsert && prefixIs(thearg.first, "item:")) {
				FuncRequest cmd2(LFUN_ARGUMENT_INSERT, thearg.first);
				lyx::dispatch(cmd2);
			}
		}
		break;
	}

	case LFUN_INSET_INSERT: {
		cur.recordUndo();

		// We have to avoid triggering InstantPreview loading
		// before inserting into the document. See bug #5626.
		bool loaded = bv->buffer().isFullyLoaded();
		bv->buffer().setFullyLoaded(false);
		Inset * inset = createInset(&bv->buffer(), cmd);
		bv->buffer().setFullyLoaded(loaded);

		if (inset) {
			// FIXME (Abdel 01/02/2006):
			// What follows would be a partial fix for bug 2154:
			//   http://www.lyx.org/trac/ticket/2154
			// This automatically put the label inset _after_ a
			// numbered section. It should be possible to extend the mechanism
			// to any kind of LateX environement.
			// The correct way to fix that bug would be at LateX generation.
			// I'll let the code here for reference as it could be used for some
			// other feature like "automatic labelling".
			/*
			Paragraph & par = pars_[cur.pit()];
			if (inset->lyxCode() == LABEL_CODE
				&& !par.layout().counter.empty()) {
				// Go to the end of the paragraph
				// Warning: Because of Change-Tracking, the last
				// position is 'size()' and not 'size()-1':
				cur.pos() = par.size();
				// Insert a new paragraph
				FuncRequest fr(LFUN_PARAGRAPH_BREAK);
				dispatch(cur, fr);
			}
			*/
			if (cur.selection())
				cutSelection(cur, false);
			cur.insert(inset);
			cur.forceBufferUpdate();
			if (inset->editable() && inset->asInsetText())
				inset->edit(cur, true);
			else
				cur.posForward();

			// trigger InstantPreview now
			if (inset->lyxCode() == EXTERNAL_CODE) {
				InsetExternal & ins =
					static_cast<InsetExternal &>(*inset);
				ins.updatePreview();
			}
		}

		break;
	}

	case LFUN_INSET_DISSOLVE: {
		if (dissolveInset(cur)) {
			needsUpdate = true;
			cur.forceBufferUpdate();
		}
		break;
	}

	case LFUN_INSET_SPLIT: {
		if (splitInset(cur)) {
			needsUpdate = true;
			cur.forceBufferUpdate();
		}
		break;
	}

	case LFUN_GRAPHICS_SET_GROUP: {
		InsetGraphics * ins = graphics::getCurrentGraphicsInset(cur);
		if (!ins)
			break;

		cur.recordUndo();

		string id = to_utf8(cmd.argument());
		string grp = graphics::getGroupParams(bv->buffer(), id);
		InsetGraphicsParams tmp, inspar = ins->getParams();

		if (id.empty())
			inspar.groupId = to_utf8(cmd.argument());
		else {
			InsetGraphics::string2params(grp, bv->buffer(), tmp);
			tmp.filename = inspar.filename;
			inspar = tmp;
		}

		ins->setParams(inspar);
		break;
	}

	case LFUN_SPACE_INSERT:
		if (cur.paragraph().layout().free_spacing)
			insertChar(cur, ' ');
		else {
			doInsertInset(cur, this, cmd, false, false);
			cur.posForward();
		}
		moveCursor(cur, false);
		break;

	case LFUN_SPECIALCHAR_INSERT: {
		string const name = to_utf8(cmd.argument());
		if (name == "hyphenation")
			specialChar(cur, InsetSpecialChar::HYPHENATION);
		else if (name == "allowbreak")
			specialChar(cur, InsetSpecialChar::ALLOWBREAK);
		else if (name == "ligature-break")
			specialChar(cur, InsetSpecialChar::LIGATURE_BREAK);
		else if (name == "slash")
			specialChar(cur, InsetSpecialChar::SLASH);
		else if (name == "nobreakdash")
			specialChar(cur, InsetSpecialChar::NOBREAKDASH);
		else if (name == "dots")
			specialChar(cur, InsetSpecialChar::LDOTS);
		else if (name == "end-of-sentence")
			specialChar(cur, InsetSpecialChar::END_OF_SENTENCE);
		else if (name == "menu-separator")
			specialChar(cur, InsetSpecialChar::MENU_SEPARATOR);
		else if (name == "lyx")
			specialChar(cur, InsetSpecialChar::PHRASE_LYX);
		else if (name == "tex")
			specialChar(cur, InsetSpecialChar::PHRASE_TEX);
		else if (name == "latex")
			specialChar(cur, InsetSpecialChar::PHRASE_LATEX);
		else if (name == "latex2e")
			specialChar(cur, InsetSpecialChar::PHRASE_LATEX2E);
		else if (name.empty())
			lyxerr << "LyX function 'specialchar-insert' needs an argument." << endl;
		else
			lyxerr << "Wrong argument for LyX function 'specialchar-insert'." << endl;
		break;
	}

	case LFUN_IPAMACRO_INSERT: {
		string const arg = cmd.getArg(0);
		if (arg == "deco") {
			// Open the inset, and move the current selection
			// inside it.
			doInsertInset(cur, this, cmd, true, true);
			cur.posForward();
			// Some insets are numbered, others are shown in the outline pane so
			// let's update the labels and the toc backend.
			cur.forceBufferUpdate();
			break;
		}
		if (arg == "tone-falling")
			ipaChar(cur, InsetIPAChar::TONE_FALLING);
		else if (arg == "tone-rising")
			ipaChar(cur, InsetIPAChar::TONE_RISING);
		else if (arg == "tone-high-rising")
			ipaChar(cur, InsetIPAChar::TONE_HIGH_RISING);
		else if (arg == "tone-low-rising")
			ipaChar(cur, InsetIPAChar::TONE_LOW_RISING);
		else if (arg == "tone-high-rising-falling")
			ipaChar(cur, InsetIPAChar::TONE_HIGH_RISING_FALLING);
		else if (arg.empty())
			lyxerr << "LyX function 'ipamacro-insert' needs an argument." << endl;
		else
			lyxerr << "Wrong argument for LyX function 'ipamacro-insert'." << endl;
		break;
	}

	case LFUN_WORD_UPCASE:
		changeCase(cur, text_uppercase, cmd.getArg(0) == "partial");
		break;

	case LFUN_WORD_LOWCASE:
		changeCase(cur, text_lowercase, cmd.getArg(0) == "partial");
		break;

	case LFUN_WORD_CAPITALIZE:
		changeCase(cur, text_capitalization, cmd.getArg(0) == "partial");
		break;

	case LFUN_CHARS_TRANSPOSE:
		charsTranspose(cur);
		break;

	case LFUN_PASTE: {
		cur.message(_("Paste"));
		LASSERT(cur.selBegin().idx() == cur.selEnd().idx(), break);
		cap::replaceSelection(cur);

		// without argument?
		string const arg = to_utf8(cmd.argument());
		if (arg.empty()) {
			bool tryGraphics = true;
			if (theClipboard().isInternal())
				pasteFromStack(cur, bv->buffer().errorList("Paste"), 0);
			else if (theClipboard().hasTextContents()) {
				if (pasteClipboardText(cur, bv->buffer().errorList("Paste"), 0,
					                   Clipboard::AnyTextType))
					tryGraphics = false;
			}
			if (tryGraphics && theClipboard().hasGraphicsContents())
				pasteClipboardGraphics(cur, bv->buffer().errorList("Paste"));
		} else if (isStrUnsignedInt(arg)) {
			// we have a numerical argument
			pasteFromStack(cur, bv->buffer().errorList("Paste"),
				       convert<unsigned int>(arg));
		} else if (arg == "html" || arg == "latex") {
			Clipboard::TextType type = (arg == "html") ?
				Clipboard::HtmlTextType : Clipboard::LaTeXTextType;
			pasteClipboardText(cur, bv->buffer().errorList("Paste"), true, type);
		} else {
			Clipboard::GraphicsType type = Clipboard::AnyGraphicsType;
			if (arg == "pdf")
				type = Clipboard::PdfGraphicsType;
			else if (arg == "png")
				type = Clipboard::PngGraphicsType;
			else if (arg == "jpeg")
				type = Clipboard::JpegGraphicsType;
			else if (arg == "linkback")
				type = Clipboard::LinkBackGraphicsType;
			else if (arg == "emf")
				type = Clipboard::EmfGraphicsType;
			else if (arg == "wmf")
				type = Clipboard::WmfGraphicsType;
			else
				// we also check in getStatus()
				LYXERR0("Unrecognized graphics type: " << arg);

			pasteClipboardGraphics(cur, bv->buffer().errorList("Paste"), type);
		}

		bv->buffer().errors("Paste");
		bv->buffer().updatePreviews(); // bug 11619
		cur.clearSelection(); // bug 393
		cur.finishUndo();
		break;
	}

	case LFUN_CUT:
		cutSelection(cur, true);
		cur.message(_("Cut"));
		break;

	case LFUN_SERVER_GET_XY:
		cur.message(from_utf8(
			convert<string>(tm->cursorX(cur.top(), cur.boundary()))
			+ ' ' + convert<string>(tm->cursorY(cur.top(), cur.boundary()))));
		break;

	case LFUN_SERVER_SET_XY: {
		int x = 0;
		int y = 0;
		istringstream is(to_utf8(cmd.argument()));
		is >> x >> y;
		if (!is)
			lyxerr << "SETXY: Could not parse coordinates in '"
			       << to_utf8(cmd.argument()) << endl;
		else
			tm->setCursorFromCoordinates(cur, x, y);
		break;
	}

	case LFUN_SERVER_GET_LAYOUT:
		cur.message(cur.paragraph().layout().name());
		break;

	case LFUN_LAYOUT:
	case LFUN_LAYOUT_TOGGLE: {
		bool const ignoreautonests = cmd.getArg(1) == "ignoreautonests";
		docstring req_layout = ignoreautonests ? from_utf8(cmd.getArg(0)) : cmd.argument();
		LYXERR(Debug::INFO, "LFUN_LAYOUT: (arg) " << to_utf8(req_layout));

		docstring layout = resolveLayout(req_layout, cur);
		if (layout.empty()) {
			cur.errorMessage(from_utf8(N_("Layout ")) + req_layout +
				from_utf8(N_(" not known")));
			break;
		}

		docstring const old_layout = cur.paragraph().layout().name();
		bool change_layout = !isAlreadyLayout(layout, cur);

		if (cmd.action() == LFUN_LAYOUT_TOGGLE && !change_layout) {
			change_layout = true;
			layout = resolveLayout(docstring(), cur);
		}

		if (change_layout) {
			setLayout(cur, layout);
			if (cur.pit() > 0 && !ignoreautonests) {
				pit_type prev_pit = cur.pit() - 1;
				depth_type const cur_depth = pars_[cur.pit()].getDepth();
				// Scan for the previous par on same nesting level
				while (prev_pit > 0 && pars_[prev_pit].getDepth() > cur_depth)
					--prev_pit;
				set<docstring> const & autonests =
						pars_[prev_pit].layout().autonests();
				set<docstring> const & autonested =
						pars_[cur.pit()].layout().isAutonestedBy();
				if (autonests.find(layout) != autonests.end()
						|| autonested.find(old_layout) != autonested.end())
					lyx::dispatch(FuncRequest(LFUN_DEPTH_INCREMENT));
			}
		}

		DocumentClass const & tclass = bv->buffer().params().documentClass();
		bool inautoarg = false;
		for (auto const & la_pair : tclass[layout].args()) {
			Layout::latexarg const & arg = la_pair.second;
			if (arg.autoinsert) {
				// If we had already inserted an arg automatically,
				// leave this now in order to insert the next one.
				if (inautoarg) {
					cur.leaveInset(cur.inset());
					cur.posForward();
				}
				FuncRequest const cmd2(LFUN_ARGUMENT_INSERT, la_pair.first);
				lyx::dispatch(cmd2);
				inautoarg = true;
			}
		}

		break;
	}

	case LFUN_ENVIRONMENT_SPLIT: {
		bool const outer = cmd.argument() == "outer";
		bool const previous = cmd.argument() == "previous";
		bool const before = cmd.argument() == "before";
		bool const normal = cmd.argument().empty();
		Paragraph const & para = cur.paragraph();
		docstring layout;
		if (para.layout().isEnvironment())
			layout = para.layout().name();
		depth_type split_depth = cur.paragraph().params().depth();
		vector<depth_type> nextpars_depth;
		if (outer || previous) {
			// check if we have an environment in our scope
			pit_type pit = cur.pit();
			Paragraph cpar = pars_[pit];
			while (true) {
				if (pit == 0)
					break;
				--pit;
				cpar = pars_[pit];
				if (layout.empty() && previous
				    && cpar.layout().isEnvironment()
				    && cpar.params().depth() <= split_depth)
					layout = cpar.layout().name();
				if (cpar.params().depth() < split_depth
				    && cpar.layout().isEnvironment()) {
						if (!previous)
							layout = cpar.layout().name();
						split_depth = cpar.params().depth();
				}
				if (cpar.params().depth() == 0)
					break;
			}
		}
		if ((outer || normal) && cur.pit() < cur.lastpit()) {
			// save nesting of following paragraphs if they are deeper
			// or same depth
			pit_type offset = 1;
			depth_type cur_depth = pars_[cur.pit()].params().depth();
			while (cur.pit() + offset <= cur.lastpit()) {
				Paragraph cpar = pars_[cur.pit() + offset];
				depth_type nextpar_depth = cpar.params().depth();
				if (cur_depth <= nextpar_depth && nextpar_depth > 0) {
					nextpars_depth.push_back(nextpar_depth);
					cur_depth = nextpar_depth;
					++offset;
				} else
					break;
			}
		}
		if (before)
			cur.top().setPitPos(cur.pit(), 0);
		if (before || cur.pos() > 0)
			lyx::dispatch(FuncRequest(LFUN_PARAGRAPH_BREAK));
		else if (previous && cur.nextInset() && cur.nextInset()->lyxCode() == SEPARATOR_CODE)
			lyx::dispatch(FuncRequest(LFUN_PARAGRAPH_BREAK, "inverse ignoresep"));
		if (outer) {
			while (cur.paragraph().params().depth() > split_depth)
				lyx::dispatch(FuncRequest(LFUN_DEPTH_DECREMENT));
		}
		DocumentClass const & tc = bv->buffer().params().documentClass();
		lyx::dispatch(FuncRequest(LFUN_LAYOUT, from_ascii("\"") + tc.plainLayout().name()
					  + from_ascii("\" ignoreautonests")));
		// FIXME: Bibitem mess!
		if (cur.prevInset() && cur.prevInset()->lyxCode() == BIBITEM_CODE)
			lyx::dispatch(FuncRequest(LFUN_CHAR_DELETE_BACKWARD));
		lyx::dispatch(FuncRequest(LFUN_SEPARATOR_INSERT, "plain"));
		if (before) {
			cur.backwardPos();
			lyx::dispatch(FuncRequest(LFUN_PARAGRAPH_BREAK, "inverse ignoresep"));
			while (cur.paragraph().params().depth() < split_depth)
				lyx::dispatch(FuncRequest(LFUN_DEPTH_INCREMENT));
		}
		else
			lyx::dispatch(FuncRequest(LFUN_PARAGRAPH_BREAK, "inverse"));
		lyx::dispatch(FuncRequest(LFUN_LAYOUT, layout));
		if ((outer || normal) && !nextpars_depth.empty()) {
			// restore nesting of following paragraphs
			DocIterator scur = cur;
			depth_type max_depth = cur.paragraph().params().depth() + 1;
			for (auto nextpar_depth : nextpars_depth) {
				cur.forwardPar();
				while (cur.paragraph().params().depth() < min(nextpar_depth, max_depth)) {
					depth_type const olddepth = cur.paragraph().params().depth();
					lyx::dispatch(FuncRequest(LFUN_DEPTH_INCREMENT));
					if (olddepth == cur.paragraph().params().depth())
						// leave loop if no incrementation happens
						break;
				}
				max_depth = cur.paragraph().params().depth() + 1;
			}
			cur.setCursor(scur);
		}

		break;
	}

	case LFUN_CLIPBOARD_PASTE:
		cap::replaceSelection(cur);
		pasteClipboardText(cur, bv->buffer().errorList("Paste"),
			       cmd.argument() == "paragraph");
		bv->buffer().errors("Paste");
		break;

	case LFUN_CLIPBOARD_PASTE_SIMPLE:
		cap::replaceSelection(cur);
		pasteSimpleText(cur, cmd.argument() == "paragraph");
		break;

	case LFUN_PRIMARY_SELECTION_PASTE:
		cap::replaceSelection(cur);
		pasteString(cur, theSelection().get(),
			    cmd.argument() == "paragraph");
		break;

	case LFUN_SELECTION_PASTE:
		// Copy the selection buffer to the clipboard stack,
		// because we want it to appear in the "Edit->Paste
		// recent" menu.
		cap::replaceSelection(cur);
		cap::copySelectionToStack();
		cap::pasteSelection(bv->cursor(), bv->buffer().errorList("Paste"));
		bv->buffer().errors("Paste");
		break;

	case LFUN_QUOTE_INSERT: {
		cap::replaceSelection(cur);
		cur.recordUndo();

		Paragraph const & par = cur.paragraph();
		pos_type pos = cur.pos();
		// Ignore deleted text before cursor
		while (pos > 0 && par.isDeleted(pos - 1))
			--pos;

		bool const inner = (cmd.getArg(0) == "single" || cmd.getArg(0) == "inner");

		// Guess quote side.
		// A space triggers an opening quote. This is passed if the preceding
		// char/inset is a space or at paragraph start.
		char_type c = ' ';
		if (pos > 0 && !par.isSpace(pos - 1)) {
			if (cur.prevInset() && cur.prevInset()->lyxCode() == QUOTE_CODE) {
				// If an opening double quotation mark precedes, and this
				// is a single quote, make it opening as well
				InsetQuotes & ins =
					static_cast<InsetQuotes &>(*cur.prevInset());
				string const type = ins.getType();
				if (!suffixIs(type, "ld") || !inner)
					c = par.getChar(pos - 1);
			}
			else if (!cur.prevInset()
			    || (cur.prevInset() && cur.prevInset()->isChar()))
				// If a char precedes, pass that and let InsetQuote decide
				c = par.getChar(pos - 1);
			else {
				while (pos > 0) {
					if (par.getInset(pos - 1)
					    && !par.getInset(pos - 1)->isPartOfTextSequence()) {
						// skip "invisible" insets
						--pos;
						continue;
					}
					c = par.getChar(pos - 1);
					break;
				}
			}
		}
		QuoteLevel const quote_level = inner
				? QuoteLevel::Secondary : QuoteLevel::Primary;
		cur.insert(new InsetQuotes(cur.buffer(), c, quote_level, cmd.getArg(1), cmd.getArg(2)));
		cur.buffer()->updateBuffer();
		cur.posForward();
		break;
	}

	case LFUN_MOUSE_TRIPLE:
		if (cmd.button() == mouse_button::button1) {
			if (cur.pos() > 0)
				setCursor(cur, cur.pit(), 0);
			bv->cursor() = cur;
			cur.resetAnchor();
			if (cur.pos() < cur.lastpos())
				setCursor(cur, cur.pit(), cur.lastpos());
			cur.setSelection();
			bv->cursor() = cur;
		}
		break;

	case LFUN_MOUSE_DOUBLE:
		if (cmd.button() == mouse_button::button1) {
			selectWord(cur, WHOLE_WORD);
			bv->cursor() = cur;
		}
		break;

	// Single-click on work area
	case LFUN_MOUSE_PRESS: {
		// We are not marking a selection with the keyboard in any case.
		Cursor & bvcur = cur.bv().cursor();
		bvcur.setMark(false);
		switch (cmd.button()) {
		case mouse_button::button1:
			bvcur.setClickPos(cmd.x(), cmd.y());
			if (!bvcur.selection())
				// Set the cursor
				bvcur.resetAnchor();
			if (!bv->mouseSetCursor(cur, cmd.modifier() == ShiftModifier))
				cur.screenUpdateFlags(Update::SinglePar | Update::FitCursor);
			// FIXME: move this to mouseSetCursor?
			if (bvcur.wordSelection() && bvcur.inTexted())
				expandWordSel(bvcur);
			break;

		case mouse_button::button2:
			if (lyxrc.mouse_middlebutton_paste) {
				// Middle mouse pasting.
				bv->mouseSetCursor(cur);
				lyx::dispatch(
					FuncRequest(LFUN_COMMAND_ALTERNATIVES,
						    "selection-paste ; primary-selection-paste paragraph"));
			}
			cur.noScreenUpdate();
			break;

		case mouse_button::button3: {
			// Don't do anything if we right-click a
			// selection, a context menu will popup.
			if (bvcur.selection() && cur >= bvcur.selectionBegin()
			    && cur <= bvcur.selectionEnd()) {
				cur.noScreenUpdate();
				return;
			}
			if (!bv->mouseSetCursor(cur, false))
				cur.screenUpdateFlags(Update::FitCursor);
			break;
		}

		default:
			break;
		} // switch (cmd.button())
		break;
	}
	case LFUN_MOUSE_MOTION: {
		// Mouse motion with right or middle mouse do nothing for now.
		if (cmd.button() != mouse_button::button1) {
			cur.noScreenUpdate();
			return;
		}
		// ignore motions deeper nested than the real anchor
		Cursor & bvcur = cur.bv().cursor();
		if (!bvcur.realAnchor().hasPart(cur)) {
			cur.undispatched();
			break;
		}
		CursorSlice old = bvcur.top();

		int const wh = bv->workHeight();
		int const y = max(0, min(wh - 1, cmd.y()));

		tm->setCursorFromCoordinates(cur, cmd.x(), y);
		cur.setTargetX(cmd.x());
		// Don't allow selecting a separator inset
		if (cur.pos() && cur.paragraph().isEnvSeparator(cur.pos() - 1))
			cur.posBackward();
		if (cmd.y() >= wh)
			lyx::dispatch(FuncRequest(LFUN_DOWN_SELECT));
		else if (cmd.y() < 0)
			lyx::dispatch(FuncRequest(LFUN_UP_SELECT));
		// This is to allow jumping over large insets
		if (cur.top() == old) {
			if (cmd.y() >= wh)
				lyx::dispatch(FuncRequest(LFUN_DOWN_SELECT));
			else if (cmd.y() < 0)
				lyx::dispatch(FuncRequest(LFUN_UP_SELECT));
		}
		// We continue with our existing selection or start a new one, so don't
		// reset the anchor.
		bvcur.setCursor(cur);
		if (bvcur.wordSelection() && bvcur.inTexted())
			expandWordSel(bvcur);
		bvcur.selection(true);
		bvcur.setCurrentFont();
		if (cur.top() == old) {
			// We didn't move one iota, so no need to update the screen.
			cur.screenUpdateFlags(Update::SinglePar | Update::FitCursor);
			//cur.noScreenUpdate();
			return;
		}
		break;
	}

	case LFUN_MOUSE_RELEASE:
		switch (cmd.button()) {
		case mouse_button::button1:
			// unregister last mouse press position
			cur.bv().cursor().setClickPos(-1, -1);
			// Cursor was set at LFUN_MOUSE_PRESS or LFUN_MOUSE_MOTION time.
			// If there is a new selection, update persistent selection;
			// otherwise, single click does not clear persistent selection
			// buffer.
			if (cur.selection()) {
				// Finish selection. If double click,
				// cur is moved to the end of word by
				// selectWord but bvcur is current
				// mouse position.
				cur.bv().cursor().setSelection();
				// We might have removed an empty but drawn selection
				// (probably a margin)
				cur.screenUpdateFlags(Update::SinglePar | Update::FitCursor);
			} else
				cur.noScreenUpdate();
			// FIXME: We could try to handle drag and drop of selection here.
			return;

		case mouse_button::button2:
			// Middle mouse pasting is handled at mouse press time,
			// see LFUN_MOUSE_PRESS.
			cur.noScreenUpdate();
			return;

		case mouse_button::button3:
			// Cursor was set at LFUN_MOUSE_PRESS time.
			// FIXME: If there is a selection we could try to handle a special
			// drag & drop context menu.
			cur.noScreenUpdate();
			return;

		case mouse_button::none:
		case mouse_button::button4:
		case mouse_button::button5:
			break;
		} // switch (cmd.button())

		break;

	case LFUN_SELF_INSERT: {
		if (cmd.argument().empty())
			break;

		// Automatically delete the currently selected
		// text and replace it with what is being
		// typed in now. Depends on lyxrc settings
		// "auto_region_delete", which defaults to
		// true (on).

		if (lyxrc.auto_region_delete && cur.selection()) {
			cutSelection(cur, false);
			cur.setCurrentFont();
		}
		cur.clearSelection();

		for (char_type c : cmd.argument())
			bv->translateAndInsert(c, this, cur);

		cur.resetAnchor();
		moveCursor(cur, false);
		cur.markNewWordPosition();
		bv->bookmarkEditPosition();
		break;
	}

	case LFUN_HREF_INSERT: {
		docstring content = cmd.argument();
		if (content.empty() && cur.selection())
			content = cur.selectionAsString(false);

		InsetCommandParams p(HYPERLINK_CODE);
		if (!content.empty()){
			// if it looks like a link, we'll put it as target,
			// otherwise as name (bug #8792).

			// We can't do:
			//   regex_match(to_utf8(content), matches, link_re)
			// because smatch stores pointers to the substrings rather
			// than making copies of them. And those pointers become
			// invalid after regex_match returns, since it is then
			// being given a temporary object. (Thanks to Georg for
			// figuring that out.)
			regex const link_re("^(([a-z]+):|www\\.).*");
			smatch matches;
			string const c = to_utf8(lowercase(content));

			if (c.substr(0,7) == "mailto:") {
				p["target"] = content;
				p["type"] = from_ascii("mailto:");
			} else if (regex_match(c, matches, link_re)) {
				p["target"] = content;
				string protocol = matches.str(1);
				if (protocol == "file")
					p["type"] = from_ascii("file:");
			} else
				p["name"] = content;
		}
		string const data = InsetCommand::params2string(p);

		// we need to have a target. if we already have one, then
		// that gets used at the default for the name, too, which
		// is probably what is wanted.
		if (p["target"].empty()) {
			bv->showDialog("href", data);
		} else {
			FuncRequest fr(LFUN_INSET_INSERT, data);
			dispatch(cur, fr);
		}
		break;
	}

	case LFUN_LABEL_INSERT: {
		InsetCommandParams p(LABEL_CODE);
		// Try to generate a valid label
		p["name"] = (cmd.argument().empty()) ?
			cur.getPossibleLabel() :
			cmd.argument();
		string const data = InsetCommand::params2string(p);

		if (cmd.argument().empty()) {
			bv->showDialog("label", data);
		} else {
			FuncRequest fr(LFUN_INSET_INSERT, data);
			dispatch(cur, fr);
		}
		break;
	}

	case LFUN_INFO_INSERT: {
		if (cmd.argument().empty()) {
			bv->showDialog("info", cur.current_font.language()->lang());
		} else {
			Inset * inset;
			inset = createInset(cur.buffer(), cmd);
			if (!inset)
				break;
			cur.recordUndo();
			insertInset(cur, inset);
			cur.forceBufferUpdate();
			cur.posForward();
		}
		break;
	}
	case LFUN_CAPTION_INSERT:
	case LFUN_FOOTNOTE_INSERT:
	case LFUN_NOTE_INSERT:
	case LFUN_BOX_INSERT:
	case LFUN_BRANCH_INSERT:
	case LFUN_PHANTOM_INSERT:
	case LFUN_ERT_INSERT:
	case LFUN_INDEXMACRO_INSERT:
	case LFUN_LISTING_INSERT:
	case LFUN_MARGINALNOTE_INSERT:
	case LFUN_ARGUMENT_INSERT:
	case LFUN_INDEX_INSERT:
	case LFUN_PREVIEW_INSERT:
	case LFUN_SCRIPT_INSERT:
	case LFUN_IPA_INSERT: {
		// Indexes reset font formatting (#11961)
		bool const resetfont = cmd.action() == LFUN_INDEX_INSERT;
		// Open the inset, and move the current selection
		// inside it.
		doInsertInset(cur, this, cmd, true, true, resetfont);
		cur.posForward();
		cur.setCurrentFont();
		// Some insets are numbered, others are shown in the outline pane so
		// let's update the labels and the toc backend.
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_FLEX_INSERT: {
		// Open the inset, and move the current selection
		// inside it.
		bool const sel = cur.selection();
		doInsertInset(cur, this, cmd, true, true);
		// Insert auto-insert arguments
		bool autoargs = false, inautoarg = false;
		Layout::LaTeXArgMap args = cur.inset().getLayout().args();
		for (auto const & argt : args) {
			Layout::latexarg arg = argt.second;
			if (!inautoarg && arg.insertonnewline && cur.pos() > 0) {
				FuncRequest cmd2(LFUN_PARAGRAPH_BREAK);
				lyx::dispatch(cmd2);
			}
			if (arg.autoinsert) {
				// The cursor might have been invalidated by the replaceSelection.
				cur.buffer()->changed(true);
				// If we had already inserted an arg automatically,
				// leave this now in order to insert the next one.
				if (inautoarg) {
					cur.leaveInset(cur.inset());
					cur.setCurrentFont();
					cur.posForward();
					if (arg.insertonnewline && cur.pos() > 0) {
						FuncRequest cmd2(LFUN_PARAGRAPH_BREAK);
						lyx::dispatch(cmd2);
					}
				}
				FuncRequest cmd2(LFUN_ARGUMENT_INSERT, argt.first);
				lyx::dispatch(cmd2);
				autoargs = true;
				inautoarg = true;
			}
		}
		if (!autoargs) {
			if (sel)
				cur.leaveInset(cur.inset());
			cur.posForward();
		}
		// Some insets are numbered, others are shown in the outline pane so
		// let's update the labels and the toc backend.
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_TABULAR_INSERT: {
		// if there were no arguments, just open the dialog
		if (cmd.argument().empty()) {
			bv->showDialog("tabularcreate");
			break;
		} else if (cur.buffer()->masterParams().tablestyle != "default"
			   || bv->buffer().params().documentClass().tablestyle() != "default") {
			string tabstyle = cur.buffer()->masterParams().tablestyle;
			if (tabstyle == "default")
				tabstyle = bv->buffer().params().documentClass().tablestyle();
			if (!libFileSearch("tabletemplates", tabstyle + ".lyx").empty()) {
				FuncRequest fr(LFUN_TABULAR_STYLE_INSERT,
					       tabstyle + " " + to_ascii(cmd.argument()));
				lyx::dispatch(fr);
				break;
			} else
				// Unknown style. Report and fall back to default.
				cur.errorMessage(from_utf8(N_("Table Style ")) + from_utf8(tabstyle) +
						     from_utf8(N_(" not known")));
		}
		if (doInsertInset(cur, this, cmd, false, true))
			// move inside
			(void) checkAndActivateInset(cur, true);
		break;
	}

	case LFUN_TABULAR_STYLE_INSERT: {
		string const style = cmd.getArg(0);
		string const rows = cmd.getArg(1);
		string const cols = cmd.getArg(2);
		if (cols.empty() || !isStrInt(cols)
		    || rows.empty() || !isStrInt(rows))
			break;
		int const r = convert<int>(rows);
		int const c = convert<int>(cols);

		string suffix;
		if (r == 1)
			suffix = "_1x1";
		else if (r == 2)
			suffix = "_1x2";
		FileName const tabstyle = libFileSearch("tabletemplates",
							style + suffix + ".lyx", "lyx");
		if (tabstyle.empty())
			    break;
		UndoGroupHelper ugh(cur.buffer());
		cur.recordUndo();
		FuncRequest cmd2(LFUN_FILE_INSERT, tabstyle.absFileName() + " ignorelang");
		lyx::dispatch(cmd2);
		// go into table
		cur.backwardPos();
		if (r > 2) {
			// move one cell up to middle cell
			cur.up();
			// add the missing rows
			int const addrows = r - 3;
			for (int i = 0 ; i < addrows ; ++i) {
				FuncRequest fr(LFUN_TABULAR_FEATURE, "append-row");
				lyx::dispatch(fr);
			}
		}
		// add the missing columns
		int const addcols = c - 1;
		for (int i = 0 ; i < addcols ; ++i) {
			FuncRequest fr(LFUN_TABULAR_FEATURE, "append-column");
			lyx::dispatch(fr);
		}
		if (r > 1)
			// go to first cell
			cur.up();
		break;
	}

	case LFUN_FLOAT_INSERT:
	case LFUN_FLOAT_WIDE_INSERT:
	case LFUN_WRAP_INSERT: {
		// will some content be moved into the inset?
		bool const content = cur.selection();
		// does the content consist of multiple paragraphs?
		bool const singlepar = (cur.selBegin().pit() == cur.selEnd().pit());

		doInsertInset(cur, this, cmd, true, true);
		cur.posForward();

		// If some single-par content is moved into the inset,
		// doInsertInset puts the cursor outside the inset.
		// To insert the caption we put it back into the inset.
		// FIXME cleanup doInsertInset to avoid such dances!
		if (content && singlepar)
			cur.backwardPos();

		ParagraphList & pars = cur.text()->paragraphs();

		DocumentClass const & tclass = bv->buffer().params().documentClass();

		// add a separate paragraph for the caption inset
		pars.push_back(Paragraph());
		pars.back().setInsetOwner(&cur.text()->inset());
		pars.back().setPlainOrDefaultLayout(tclass);
		int cap_pit = pars.size() - 1;

		// if an empty inset was created, we create an additional empty
		// paragraph at the bottom so that the user can choose where to put
		// the graphics (or table).
		if (!content) {
			pars.push_back(Paragraph());
			pars.back().setInsetOwner(&cur.text()->inset());
			pars.back().setPlainOrDefaultLayout(tclass);
		}

		// reposition the cursor to the caption
		cur.pit() = cap_pit;
		cur.pos() = 0;
		// FIXME: This Text/Cursor dispatch handling is a mess!
		// We cannot use Cursor::dispatch here it needs access to up to
		// date metrics.
		FuncRequest cmd_caption(LFUN_CAPTION_INSERT);
		doInsertInset(cur, cur.text(), cmd_caption, true, false);
		cur.forceBufferUpdate();
		cur.screenUpdateFlags(Update::Force);
		// FIXME: When leaving the Float (or Wrap) inset we should
		// delete any empty paragraph left above or below the
		// caption.
		break;
	}

	case LFUN_NOMENCL_INSERT: {
		InsetCommandParams p(NOMENCL_CODE);
		if (cmd.argument().empty()) {
			p["symbol"] =
				bv->cursor().innerText()->getStringForDialog(bv->cursor());
			cur.clearSelection();
		} else
			p["symbol"] = cmd.argument();
		string const data = InsetCommand::params2string(p);
		bv->showDialog("nomenclature", data);
		break;
	}

	case LFUN_INDEX_PRINT: {
		InsetCommandParams p(INDEX_PRINT_CODE);
		if (cmd.argument().empty())
			p["type"] = from_ascii("idx");
		else
			p["type"] = cmd.argument();
		string const data = InsetCommand::params2string(p);
		FuncRequest fr(LFUN_INSET_INSERT, data);
		dispatch(cur, fr);
		break;
	}

	case LFUN_NOMENCL_PRINT:
		// do nothing fancy
		doInsertInset(cur, this, cmd, false, false);
		cur.posForward();
		break;

	case LFUN_NEWPAGE_INSERT: {
		// When we are in a heading, put the page break in a standard
		// paragraph before the heading (if cur.pos() == 0) or after
		// (if cur.pos() == cur.lastpos())
		if (cur.text()->getTocLevel(cur.pit()) != Layout::NOT_IN_TOC) {
			lyx::dispatch(FuncRequest(LFUN_PARAGRAPH_BREAK));
			DocumentClass const & tc = bv->buffer().params().documentClass();
			lyx::dispatch(FuncRequest(LFUN_LAYOUT, from_ascii("\"") + tc.plainLayout().name()
						  + from_ascii("\" ignoreautonests")));
		}
		// do nothing fancy
		doInsertInset(cur, this, cmd, false, false);
		cur.posForward();
		break;
	}

	case LFUN_SEPARATOR_INSERT: {
		doInsertInset(cur, this, cmd, false, false);
		cur.posForward();
		// remove a following space
		Paragraph & par = cur.paragraph();
		if (cur.pos() != cur.lastpos() && par.isLineSeparator(cur.pos()))
		    par.eraseChar(cur.pos(), cur.buffer()->params().track_changes);
		break;
	}

	case LFUN_DEPTH_DECREMENT:
		changeDepth(cur, DEC_DEPTH);
		break;

	case LFUN_DEPTH_INCREMENT:
		changeDepth(cur, INC_DEPTH);
		break;

	case LFUN_REGEXP_MODE:
		regexpDispatch(cur, cmd);
		break;

	case LFUN_MATH_MODE: {
		if (cmd.argument() == "on" || cmd.argument() == "") {
			// don't pass "on" as argument
			// (it would appear literally in the first cell)
			docstring sel = cur.selectionAsString(false);
			InsetMathMacroTemplate * macro = new InsetMathMacroTemplate(cur.buffer());
			// create a macro template if we see "\\newcommand" somewhere, and
			// an ordinary formula otherwise
			if (!sel.empty()
				&& (sel.find(from_ascii("\\newcommand")) != string::npos
					|| sel.find(from_ascii("\\newlyxcommand")) != string::npos
					|| sel.find(from_ascii("\\def")) != string::npos)
				&& macro->fromString(sel)) {
				cur.recordUndo();
				replaceSelection(cur);
				cur.insert(macro);
			} else {
				// no meaningful macro template was found
				delete macro;
				mathDispatch(cur,FuncRequest(LFUN_MATH_MODE));
			}
		} else
			// The argument is meaningful
			// We replace cmd with LFUN_MATH_INSERT because LFUN_MATH_MODE
			// has a different meaning in math mode
			mathDispatch(cur, FuncRequest(LFUN_MATH_INSERT,cmd.argument()));
		break;
	}

	case LFUN_MATH_MACRO:
		if (cmd.argument().empty())
			cur.errorMessage(from_utf8(N_("Missing argument")));
		else {
			cur.recordUndo();
			string s = to_utf8(cmd.argument());
			string const s1 = token(s, ' ', 1);
			int const nargs = s1.empty() ? 0 : convert<int>(s1);
			string const s2 = token(s, ' ', 2);
			MacroType type = MacroTypeNewcommand;
			if (s2 == "def")
				type = MacroTypeDef;
			InsetMathMacroTemplate * inset = new InsetMathMacroTemplate(cur.buffer(),
				from_utf8(token(s, ' ', 0)), nargs, false, type);
			inset->setBuffer(bv->buffer());
			if (insertInset(cur, inset)) {
				// If insertion is successful, enter macro inset and select the name
				cur.push(*inset);
				cur.top().pos() = cur.top().lastpos();
				cur.resetAnchor();
				cur.selection(true);
				cur.top().pos() = 0;
			} else
				delete inset;
		}
		break;

	case LFUN_MATH_DISPLAY:
	case LFUN_MATH_SUBSCRIPT:
	case LFUN_MATH_SUPERSCRIPT:
	case LFUN_MATH_INSERT:
	case LFUN_MATH_AMS_MATRIX:
	case LFUN_MATH_MATRIX:
	case LFUN_MATH_DELIM:
	case LFUN_MATH_BIGDELIM:
		mathDispatch(cur, cmd);
		break;

	case LFUN_FONT_EMPH: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setEmph(FONT_TOGGLE);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_ITAL: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setShape(ITALIC_SHAPE);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_BOLD:
	case LFUN_FONT_BOLDSYMBOL: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setSeries(BOLD_SERIES);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_NOUN: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setNoun(FONT_TOGGLE);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_TYPEWRITER: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setFamily(TYPEWRITER_FAMILY); // no good
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_SANS: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setFamily(SANS_FAMILY);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_ROMAN: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setFamily(ROMAN_FAMILY);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_DEFAULT: {
		Font font(inherit_font, ignore_language);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_STRIKEOUT: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setStrikeout(FONT_TOGGLE);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_CROSSOUT: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setXout(FONT_TOGGLE);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_UNDERUNDERLINE: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setUuline(FONT_TOGGLE);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_UNDERWAVE: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setUwave(FONT_TOGGLE);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_UNDERLINE: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setUnderbar(FONT_TOGGLE);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_NO_SPELLCHECK: {
		Font font(ignore_font, ignore_language);
		font.fontInfo().setNoSpellcheck(FONT_TOGGLE);
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_FONT_SIZE: {
		Font font(ignore_font, ignore_language);
		setLyXSize(to_utf8(cmd.argument()), font.fontInfo());
		toggleAndShow(cur, this, font);
		break;
	}

	case LFUN_LANGUAGE: {
		string const lang_arg = cmd.getArg(0);
		bool const reset = (lang_arg.empty() || lang_arg == "reset");
		Language const * lang =
			reset ? cur.bv().buffer().params().language
			      : languages.getLanguage(lang_arg);
		// we allow reset_language, which is 0, but only if it
		// was requested via empty or "reset" arg.
		if (!lang && !reset)
			break;
		bool const toggle = (cmd.getArg(1) != "set");
		selectWordWhenUnderCursor(cur, WHOLE_WORD_STRICT);
		Font font(ignore_font, lang);
		toggleAndShow(cur, this, font, toggle);
		break;
	}

	case LFUN_TEXTSTYLE_APPLY: {
		unsigned int num = 0;
		string const arg = to_utf8(cmd.argument());
		// Argument?
		if (!arg.empty()) {
			if (isStrUnsignedInt(arg)) {
				num = convert<unsigned int>(arg);
				if (num >= freeFonts.size()) {
					cur.message(_("Invalid argument (number exceeds stack size)!"));
					break;
				}
			} else {
				cur.message(_("Invalid argument (must be a non-negative number)!"));
				break;
			}
		}
		toggleAndShow(cur, this, freeFonts[num].second, toggleall);
		cur.message(bformat(_("Text properties applied: %1$s"), freeFonts[num].first));
		break;
	}

	// Set the freefont using the contents of \param data dispatched from
	// the frontends and apply it at the current cursor location.
	case LFUN_TEXTSTYLE_UPDATE: {
		Font font(ignore_font, ignore_language);
		bool toggle;
		if (font.fromString(to_utf8(cmd.argument()), toggle)) {
			docstring const props = font.stateText(&bv->buffer().params(), true);
			freeFonts.push(make_pair(props, font));
			toggleall = toggle;
			toggleAndShow(cur, this, font, toggleall);
			cur.message(bformat(_("Text properties applied: %1$s"), props));
		} else
			LYXERR0("Invalid argument of textstyle-update");
		break;
	}

	case LFUN_FINISHED_LEFT:
		LYXERR(Debug::DEBUG, "handle LFUN_FINISHED_LEFT:\n" << cur);
		// We're leaving an inset, going left. If the inset is LTR, we're
		// leaving from the front, so we should not move (remain at --- but
		// not in --- the inset). If the inset is RTL, move left, without
		// entering the inset itself; i.e., move to after the inset.
		if (cur.paragraph().getFontSettings(
				cur.bv().buffer().params(), cur.pos()).isRightToLeft())
			cursorVisLeft(cur, true);
		break;

	case LFUN_FINISHED_RIGHT:
		LYXERR(Debug::DEBUG, "handle LFUN_FINISHED_RIGHT:\n" << cur);
		// We're leaving an inset, going right. If the inset is RTL, we're
		// leaving from the front, so we should not move (remain at --- but
		// not in --- the inset). If the inset is LTR, move right, without
		// entering the inset itself; i.e., move to after the inset.
		if (!cur.paragraph().getFontSettings(
				cur.bv().buffer().params(), cur.pos()).isRightToLeft())
			cursorVisRight(cur, true);
		break;

	case LFUN_FINISHED_BACKWARD:
		LYXERR(Debug::DEBUG, "handle LFUN_FINISHED_BACKWARD:\n" << cur);
		cur.setCurrentFont();
		break;

	case LFUN_FINISHED_FORWARD:
		LYXERR(Debug::DEBUG, "handle LFUN_FINISHED_FORWARD:\n" << cur);
		++cur.pos();
		cur.setCurrentFont();
		break;

	case LFUN_LAYOUT_PARAGRAPH: {
		string data;
		params2string(cur.paragraph(), data);
		data = "show\n" + data;
		bv->showDialog("paragraph", data);
		break;
	}

	case LFUN_PARAGRAPH_UPDATE: {
		string data;
		params2string(cur.paragraph(), data);

		// Will the paragraph accept changes from the dialog?
		bool const accept =
			cur.inset().allowParagraphCustomization(cur.idx());

		data = "update " + convert<string>(accept) + '\n' + data;
		bv->updateDialog("paragraph", data);
		break;
	}

	case LFUN_ACCENT_UMLAUT:
	case LFUN_ACCENT_CIRCUMFLEX:
	case LFUN_ACCENT_GRAVE:
	case LFUN_ACCENT_ACUTE:
	case LFUN_ACCENT_TILDE:
	case LFUN_ACCENT_PERISPOMENI:
	case LFUN_ACCENT_CEDILLA:
	case LFUN_ACCENT_MACRON:
	case LFUN_ACCENT_DOT:
	case LFUN_ACCENT_UNDERDOT:
	case LFUN_ACCENT_UNDERBAR:
	case LFUN_ACCENT_CARON:
	case LFUN_ACCENT_BREVE:
	case LFUN_ACCENT_TIE:
	case LFUN_ACCENT_HUNGARIAN_UMLAUT:
	case LFUN_ACCENT_CIRCLE:
	case LFUN_ACCENT_OGONEK:
		theApp()->handleKeyFunc(cmd.action());
		if (!cmd.argument().empty())
			// FIXME: Are all these characters encoded in one byte in utf8?
			bv->translateAndInsert(cmd.argument()[0], this, cur);
		cur.screenUpdateFlags(Update::FitCursor);
		break;

	case LFUN_FLOAT_LIST_INSERT: {
		DocumentClass const & tclass = bv->buffer().params().documentClass();
		if (tclass.floats().typeExist(to_utf8(cmd.argument()))) {
			cur.recordUndo();
			if (cur.selection())
				cutSelection(cur, false);
			breakParagraph(cur);

			if (cur.lastpos() != 0) {
				cursorBackward(cur);
				breakParagraph(cur);
			}

			docstring const laystr = cur.inset().usePlainLayout() ?
				tclass.plainLayoutName() :
				tclass.defaultLayoutName();
			setLayout(cur, laystr);
			ParagraphParameters p;
			// FIXME If this call were replaced with one to clearParagraphParams(),
			// then we could get rid of this method altogether.
			setParagraphs(cur, p);
			// FIXME This should be simplified when InsetFloatList takes a
			// Buffer in its constructor.
			InsetFloatList * ifl = new InsetFloatList(cur.buffer(), to_utf8(cmd.argument()));
			ifl->setBuffer(bv->buffer());
			insertInset(cur, ifl);
			cur.posForward();
		} else {
			lyxerr << "Non-existent float type: "
			       << to_utf8(cmd.argument()) << endl;
		}
		break;
	}

	case LFUN_CHANGE_ACCEPT: {
		acceptOrRejectChanges(cur, ACCEPT);
		break;
	}

	case LFUN_CHANGE_REJECT: {
		acceptOrRejectChanges(cur, REJECT);
		break;
	}

	case LFUN_THESAURUS_ENTRY: {
		Language const * language = cur.getFont().language();
		docstring arg = cmd.argument();
		if (arg.empty()) {
			arg = cur.selectionAsString(false);
			// Too large. We unselect if needed and try to get
			// the first word in selection or under cursor
			if (arg.size() > 100 || arg.empty()) {
				if (cur.selection()) {
					DocIterator selbeg = cur.selectionBegin();
					cur.clearSelection();
					setCursorIntern(cur, selbeg.pit(), selbeg.pos());
					cur.screenUpdateFlags(Update::Force);
				}
				// Get word or selection
				selectWordWhenUnderCursor(cur, WHOLE_WORD);
				arg = cur.selectionAsString(false);
				arg += " lang=" + from_ascii(language->lang());
			}
		} else {
			string lang = cmd.getArg(1);
			// This duplicates the code in GuiThesaurus::initialiseParams
			if (prefixIs(lang, "lang=")) {
				language = languages.getLanguage(lang.substr(5));
				if (!language)
					language = cur.getFont().language();
			}
		}
		string lang = language->code();
		if (lyxrc.thesaurusdir_path.empty() && !thesaurus.thesaurusInstalled(from_ascii(lang))) {
			LYXERR(Debug::ACTION, "Command " << cmd << ". Thesaurus not found for language " << lang);
			frontend::Alert::warning(_("Path to thesaurus directory not set!"),
					_("The path to the thesaurus directory has not been specified.\n"
					  "The thesaurus is not functional.\n"
					  "Please refer to sec. 6.15.1 of the User's Guide for setup\n"
					  "instructions."));
		}
		bv->showDialog("thesaurus", to_utf8(arg));
		break;
	}

	case LFUN_SPELLING_ADD: {
		Language const * language = getLanguage(cur, cmd.getArg(1));
		docstring word = from_utf8(cmd.getArg(0));
		if (word.empty()) {
			word = cur.selectionAsString(false);
			// FIXME
			if (word.size() > 100 || word.empty()) {
				// Get word or selection
				selectWordWhenUnderCursor(cur, WHOLE_WORD);
				word = cur.selectionAsString(false);
			}
		}
		WordLangTuple wl(word, language);
		theSpellChecker()->insert(wl);
		break;
	}

	case LFUN_SPELLING_ADD_LOCAL: {
		Language const * language = getLanguage(cur, cmd.getArg(1));
		docstring word = from_utf8(cmd.getArg(0));
		if (word.empty()) {
			word = cur.selectionAsString(false);
			if (word.size() > 100)
				break;
			if (word.empty()) {
				// Get word or selection
				selectWordWhenUnderCursor(cur, WHOLE_WORD);
				word = cur.selectionAsString(false);
			}
		}
		WordLangTuple wl(word, language);
		if (!bv->buffer().params().spellignored(wl)) {
			cur.recordUndoBufferParams();
			bv->buffer().params().spellignore().push_back(wl);
			cur.recordUndo();
			// trigger re-check of whole buffer
			bv->buffer().requestSpellcheck();
		}
		break;
	}

	case LFUN_SPELLING_REMOVE_LOCAL: {
		Language const * language = getLanguage(cur, cmd.getArg(1));
		docstring word = from_utf8(cmd.getArg(0));
		if (word.empty()) {
			word = cur.selectionAsString(false);
			if (word.size() > 100)
				break;
			if (word.empty()) {
				// Get word or selection
				selectWordWhenUnderCursor(cur, WHOLE_WORD);
				word = cur.selectionAsString(false);
			}
		}
		WordLangTuple wl(word, language);
		bool has_item = false;
		vector<WordLangTuple>::const_iterator it = bv->buffer().params().spellignore().begin();
		for (; it != bv->buffer().params().spellignore().end(); ++it) {
			if (it->lang()->code() != wl.lang()->code())
				continue;
			if (it->word() == wl.word()) {
				has_item = true;
				break;
			}
		}
		if (has_item) {
			cur.recordUndoBufferParams();
			bv->buffer().params().spellignore().erase(it);
			cur.recordUndo();
			// trigger re-check of whole buffer
			bv->buffer().requestSpellcheck();
		}
		break;
	}


	case LFUN_SPELLING_IGNORE: {
		Language const * language = getLanguage(cur, cmd.getArg(1));
		docstring word = from_utf8(cmd.getArg(0));
		if (word.empty()) {
			word = cur.selectionAsString(false);
			// FIXME
			if (word.size() > 100 || word.empty()) {
				// Get word or selection
				selectWordWhenUnderCursor(cur, WHOLE_WORD);
				word = cur.selectionAsString(false);
			}
		}
		WordLangTuple wl(word, language);
		theSpellChecker()->accept(wl);
		break;
	}

	case LFUN_SPELLING_REMOVE: {
		Language const * language = getLanguage(cur, cmd.getArg(1));
		docstring word = from_utf8(cmd.getArg(0));
		if (word.empty()) {
			word = cur.selectionAsString(false);
			// FIXME
			if (word.size() > 100 || word.empty()) {
				// Get word or selection
				selectWordWhenUnderCursor(cur, WHOLE_WORD);
				word = cur.selectionAsString(false);
			}
		}
		WordLangTuple wl(word, language);
		theSpellChecker()->remove(wl);
		break;
	}

	case LFUN_PARAGRAPH_PARAMS_APPLY: {
		// Given data, an encoding of the ParagraphParameters
		// generated in the Paragraph dialog, this function sets
		// the current paragraph, or currently selected paragraphs,
		// appropriately.
		// NOTE: This function overrides all existing settings.
		setParagraphs(cur, cmd.argument());
		cur.message(_("Paragraph layout set"));
		break;
	}

	case LFUN_PARAGRAPH_PARAMS: {
		// Given data, an encoding of the ParagraphParameters as we'd
		// find them in a LyX file, this function modifies the current paragraph,
		// or currently selected paragraphs.
		// NOTE: This function only modifies, and does not override, existing
		// settings.
		setParagraphs(cur, cmd.argument(), true);
		cur.message(_("Paragraph layout set"));
		break;
	}

	case LFUN_ESCAPE:
		if (cur.selection()) {
			cur.selection(false);
		} else {
			cur.undispatched();
			// This used to be LFUN_FINISHED_RIGHT, I think FORWARD is more
			// correct, but I'm not 100% sure -- dov, 071019
			cmd = FuncRequest(LFUN_FINISHED_FORWARD);
		}
		break;

	case LFUN_OUTLINE_UP: {
		pos_type const opos = cur.pos();
		outline(OutlineUp, cur, false);
		setCursor(cur, cur.pit(), opos);
		cur.forceBufferUpdate();
		needsUpdate = true;
		break;
	}

	case LFUN_OUTLINE_DOWN: {
		pos_type const opos = cur.pos();
		outline(OutlineDown, cur, false);
		setCursor(cur, cur.pit(), opos);
		cur.forceBufferUpdate();
		needsUpdate = true;
		break;
	}

	case LFUN_OUTLINE_IN:
		outline(OutlineIn, cur, cmd.getArg(0) == "local");
		cur.forceBufferUpdate();
		needsUpdate = true;
		break;

	case LFUN_OUTLINE_OUT:
		outline(OutlineOut, cur, cmd.getArg(0) == "local");
		cur.forceBufferUpdate();
		needsUpdate = true;
		break;

	case LFUN_SERVER_GET_STATISTICS: {
		DocIterator from, to;
		if (cur.selection()) {
			from = cur.selectionBegin();
			to = cur.selectionEnd();
		} else {
			from = doc_iterator_begin(cur.buffer());
			to = doc_iterator_end(cur.buffer());
		}

		cur.buffer()->updateStatistics(from, to);
		string const arg0 = cmd.getArg(0);
		if (arg0 == "words") {
			cur.message(convert<docstring>(cur.buffer()->wordCount()));
		} else if (arg0 == "chars") {
			cur.message(convert<docstring>(cur.buffer()->charCount(false)));
		} else if (arg0 == "chars-space") {
			cur.message(convert<docstring>(cur.buffer()->charCount(true)));
		} else {
			cur.message(convert<docstring>(cur.buffer()->wordCount()) + " "
			+ convert<docstring>(cur.buffer()->charCount(false)) + " "
			+ convert<docstring>(cur.buffer()->charCount(true)));
		}
		break;
	}

	default:
		LYXERR(Debug::ACTION, "Command " << cmd << " not DISPATCHED by Text");
		cur.undispatched();
		break;
	}

	needsUpdate |= (cur.pos() != cur.lastpos()) && cur.selection();

	if (lyxrc.spellcheck_continuously && !needsUpdate) {
		// Check for misspelled text
		// The redraw is useful because of the painting of
		// misspelled markers depends on the cursor position.
		// Trigger a redraw for cursor moves inside misspelled text.
		if (!cur.inTexted()) {
			// move from regular text to math
			needsUpdate = last_misspelled;
		} else if (oldTopSlice != cur.top() || oldBoundary != cur.boundary()) {
			// move inside regular text
			needsUpdate = last_misspelled
				|| cur.paragraph().isMisspelled(cur.pos(), true);
		}
	}

	// FIXME: the following code should go in favor of fine grained
	// update flag treatment.
	if (needsUpdate || cur.result().screenUpdate() & Update::Force)
		cur.screenUpdateFlags(Update::Force | Update::FitCursor);
	else if (singleParUpdate || cur.result().screenUpdate() & Update::SinglePar) {
		// Inserting characters does not change par height in general. So, try
		// to update _only_ this paragraph. BufferView will detect if a full
		// metrics update is needed anyway.
		cur.screenUpdateFlags(Update::SinglePar | Update::FitCursor);
		return;
	}
	else {
		// oldSelection is a backup of cur.selection() at the beginning of the function.
	    if (!oldSelection && !cur.selection())
			// FIXME: it would be better if we could just do this
			//
			//if (cur.result().update() != Update::FitCursor)
			//	cur.noScreenUpdate();
			//
			// But some LFUNs do not set Update::FitCursor when needed, so we
			// do it for all. This is not very harmfull as FitCursor will provoke
			// a full redraw only if needed but still, a proper review of all LFUN
			// should be done and this needsUpdate boolean can then be removed.
			cur.screenUpdateFlags(Update::FitCursor);
		else
			cur.screenUpdateFlags(Update::ForceDraw | Update::FitCursor);
	}
}


bool Text::getStatus(Cursor & cur, FuncRequest const & cmd,
			FuncStatus & status) const
{
	LBUFERR(this == cur.text());

	FontInfo const & fontinfo = cur.real_current_font.fontInfo();
	bool enable = true;
	bool allow_in_passthru = false;
	InsetCode code = NO_CODE;

	switch (cmd.action()) {

	case LFUN_DEPTH_DECREMENT:
		enable = changeDepthAllowed(cur, DEC_DEPTH);
		break;

	case LFUN_DEPTH_INCREMENT:
		enable = changeDepthAllowed(cur, INC_DEPTH);
		break;

	case LFUN_APPENDIX:
		// FIXME We really should not allow this to be put, e.g.,
		// in a footnote, or in ERT. But it would make sense in a
		// branch, so I'm not sure what to do.
		status.setOnOff(cur.paragraph().params().startOfAppendix());
		break;

	case LFUN_DIALOG_SHOW_NEW_INSET:
		if (cmd.argument() == "bibitem")
			code = BIBITEM_CODE;
		else if (cmd.argument() == "bibtex") {
			code = BIBTEX_CODE;
			// not allowed in description items
			enable = !inDescriptionItem(cur);
		}
		else if (cmd.argument() == "box")
			code = BOX_CODE;
		else if (cmd.argument() == "branch")
			code = BRANCH_CODE;
		else if (cmd.argument() == "citation")
			code = CITE_CODE;
		else if (cmd.argument() == "counter")
			code = COUNTER_CODE;
		else if (cmd.argument() == "ert")
			code = ERT_CODE;
		else if (cmd.argument() == "external")
			code = EXTERNAL_CODE;
		else if (cmd.argument() == "float")
			code = FLOAT_CODE;
		else if (cmd.argument() == "graphics")
			code = GRAPHICS_CODE;
		else if (cmd.argument() == "href")
			code = HYPERLINK_CODE;
		else if (cmd.argument() == "include")
			code = INCLUDE_CODE;
		else if (cmd.argument() == "index")
			code = INDEX_CODE;
		else if (cmd.argument() == "index_print")
			code = INDEX_PRINT_CODE;
		else if (cmd.argument() == "listings")
			code = LISTINGS_CODE;
		else if (cmd.argument() == "mathspace")
			code = MATH_HULL_CODE;
		else if (cmd.argument() == "nomenclature")
			code = NOMENCL_CODE;
		else if (cmd.argument() == "nomencl_print")
			code = NOMENCL_PRINT_CODE;
		else if (cmd.argument() == "label")
			code = LABEL_CODE;
		else if (cmd.argument() == "line")
			code = LINE_CODE;
		else if (cmd.argument() == "note")
			code = NOTE_CODE;
		else if (cmd.argument() == "phantom")
			code = PHANTOM_CODE;
		else if (cmd.argument() == "ref")
			code = REF_CODE;
		else if (cmd.argument() == "space")
			code = SPACE_CODE;
		else if (cmd.argument() == "toc")
			code = TOC_CODE;
		else if (cmd.argument() == "vspace")
			code = VSPACE_CODE;
		else if (cmd.argument() == "wrap")
			code = WRAP_CODE;
		break;

	case LFUN_ERT_INSERT:
		code = ERT_CODE;
		break;
	case LFUN_LISTING_INSERT:
		code = LISTINGS_CODE;
		// not allowed in description items
		enable = !inDescriptionItem(cur);
		break;
	case LFUN_FOOTNOTE_INSERT:
		code = FOOT_CODE;
		break;
	case LFUN_TABULAR_INSERT:
		code = TABULAR_CODE;
		break;
	case LFUN_TABULAR_STYLE_INSERT:
		code = TABULAR_CODE;
		break;
	case LFUN_MARGINALNOTE_INSERT:
		code = MARGIN_CODE;
		break;
	case LFUN_FLOAT_INSERT:
	case LFUN_FLOAT_WIDE_INSERT:
		// FIXME: If there is a selection, we should check whether there
		// are floats in the selection, but this has performance issues, see
		// LFUN_CHANGE_ACCEPT/REJECT.
		code = FLOAT_CODE;
		if (inDescriptionItem(cur))
			// not allowed in description items
			enable = false;
		else {
			InsetCode const inset_code = cur.inset().lyxCode();

			// algorithm floats cannot be put in another float
			if (to_utf8(cmd.argument()) == "algorithm") {
				enable = inset_code != WRAP_CODE && inset_code != FLOAT_CODE;
				break;
			}

			// for figures and tables: only allow in another
			// float or wrap if it is of the same type and
			// not a subfloat already
			if(cur.inset().lyxCode() == code) {
				InsetFloat const & ins =
					static_cast<InsetFloat const &>(cur.inset());
				enable = ins.params().type == to_utf8(cmd.argument())
					&& !ins.params().subfloat;
			} else if(cur.inset().lyxCode() == WRAP_CODE) {
				InsetWrap const & ins =
					static_cast<InsetWrap const &>(cur.inset());
				enable = ins.params().type == to_utf8(cmd.argument());
			}
		}
		break;
	case LFUN_WRAP_INSERT:
		code = WRAP_CODE;
		// not allowed in description items
		enable = !inDescriptionItem(cur);
		break;
	case LFUN_FLOAT_LIST_INSERT: {
		code = FLOAT_LIST_CODE;
		// not allowed in description items
		enable = !inDescriptionItem(cur);
		if (enable) {
			FloatList const & floats = cur.buffer()->params().documentClass().floats();
			FloatList::const_iterator cit = floats[to_ascii(cmd.argument())];
			// make sure we know about such floats
			if (cit == floats.end() ||
					// and that we know how to generate a list of them
			    (!cit->second.usesFloatPkg() && cit->second.listCommand().empty())) {
				status.setUnknown(true);
				// probably not necessary, but...
				enable = false;
			}
		}
		break;
	}
	case LFUN_CAPTION_INSERT: {
		code = CAPTION_CODE;
		string arg = cmd.getArg(0);
		bool varia = arg != "Unnumbered"
			&& cur.inset().allowsCaptionVariation(arg);
		// not allowed in description items,
		// and in specific insets
		enable = !inDescriptionItem(cur)
			&& (varia || arg.empty() || arg == "Standard");
		break;
	}
	case LFUN_NOTE_INSERT:
		code = NOTE_CODE;
		break;
	case LFUN_FLEX_INSERT: {
		code = FLEX_CODE;
		docstring s = from_utf8(cmd.getArg(0));
		// Prepend "Flex:" prefix if not there
		if (!prefixIs(s, from_ascii("Flex:")))
			s = from_ascii("Flex:") + s;
		if (!cur.buffer()->params().documentClass().hasInsetLayout(s))
			enable = false;
		else if (!cur.paragraph().allowedInContext(cur, cur.buffer()->params().documentClass().insetLayout(s)))
			enable = false;
		else {
			InsetLyXType ilt =
				cur.buffer()->params().documentClass().insetLayout(s).lyxtype();
			if (ilt != InsetLyXType::CHARSTYLE
			    && ilt != InsetLyXType::CUSTOM
			    && ilt != InsetLyXType::STANDARD)
				enable = false;
		}
		break;
	}
	case LFUN_BOX_INSERT:
		code = BOX_CODE;
		break;
	case LFUN_BRANCH_INSERT:
		code = BRANCH_CODE;
		if (cur.buffer()->masterBuffer()->params().branchlist().empty()
		    && cur.buffer()->params().branchlist().empty())
			enable = false;
		break;
	case LFUN_IPA_INSERT:
		code = IPA_CODE;
		break;
	case LFUN_PHANTOM_INSERT:
		code = PHANTOM_CODE;
		break;
	case LFUN_LABEL_INSERT:
		code = LABEL_CODE;
		break;
	case LFUN_INFO_INSERT:
		code = INFO_CODE;
		enable = cmd.argument().empty()
			|| infoparams.validateArgument(cur.buffer(), cmd.argument(), true);
		break;
	case LFUN_ARGUMENT_INSERT: {
		code = ARG_CODE;
		allow_in_passthru = true;
		string const arg = cmd.getArg(0);
		if (arg.empty()) {
			enable = false;
			break;
		}
		Layout const & lay = cur.paragraph().layout();
		Layout::LaTeXArgMap args = lay.args();
		Layout::LaTeXArgMap::const_iterator const lait =
				args.find(arg);
		if (lait != args.end()) {
			enable = true;
			pit_type pit = cur.pit();
			pit_type lastpit = cur.pit();
			if (lay.isEnvironment() && !prefixIs(arg, "item:")) {
				// In a sequence of "merged" environment layouts, we only allow
				// non-item arguments once.
				lastpit = cur.lastpit();
				// get the first paragraph in sequence with this layout
				depth_type const current_depth = cur.paragraph().params().depth();
				while (true) {
					if (pit == 0)
						break;
					Paragraph cpar = pars_[pit - 1];
					if (cpar.layout() == lay && cpar.params().depth() == current_depth)
						--pit;
					else
						break;
				}
			}
			for (; pit <= lastpit; ++pit) {
				if (pars_[pit].layout() != lay)
					break;
				for (auto const & table : pars_[pit].insetList())
					if (InsetArgument const * ins = table.inset->asInsetArgument())
						if (ins->name() == arg) {
							// we have this already
							enable = false;
							break;
						}
			}
		} else
			enable = false;
		break;
	}
	case LFUN_INDEX_INSERT:
		code = INDEX_CODE;
		break;
	case LFUN_INDEX_PRINT:
		code = INDEX_PRINT_CODE;
		// not allowed in description items
		enable = !inDescriptionItem(cur);
		break;
	case LFUN_NOMENCL_INSERT:
		if (cur.selIsMultiCell() || cur.selIsMultiLine()) {
			enable = false;
			break;
		}
		code = NOMENCL_CODE;
		break;
	case LFUN_NOMENCL_PRINT:
		code = NOMENCL_PRINT_CODE;
		// not allowed in description items
		enable = !inDescriptionItem(cur);
		break;
	case LFUN_HREF_INSERT:
		if (cur.selIsMultiCell() || cur.selIsMultiLine()) {
			enable = false;
			break;
		}
		code = HYPERLINK_CODE;
		break;
	case LFUN_INDEXMACRO_INSERT: {
		string const arg = cmd.getArg(0);
		if (arg == "sortkey")
			code = INDEXMACRO_SORTKEY_CODE;
		else
			code = INDEXMACRO_CODE;
		break;
	}
	case LFUN_IPAMACRO_INSERT: {
		string const arg = cmd.getArg(0);
		if (arg == "deco")
			code = IPADECO_CODE;
		else
			code = IPACHAR_CODE;
		break;
	}
	case LFUN_QUOTE_INSERT:
		// always allow this, since we will inset a raw quote
		// if an inset is not allowed.
		allow_in_passthru = true;
		break;
	case LFUN_SPECIALCHAR_INSERT:
		code = SPECIALCHAR_CODE;
		break;
	case LFUN_SPACE_INSERT:
		// slight hack: we know this is allowed in math mode
		if (cur.inTexted())
			code = SPACE_CODE;
		break;
	case LFUN_PREVIEW_INSERT:
		code = PREVIEW_CODE;
		break;
	case LFUN_SCRIPT_INSERT:
		code = SCRIPT_CODE;
		break;

	case LFUN_MATH_INSERT:
	case LFUN_MATH_AMS_MATRIX:
	case LFUN_MATH_MATRIX:
	case LFUN_MATH_DELIM:
	case LFUN_MATH_BIGDELIM:
	case LFUN_MATH_DISPLAY:
	case LFUN_MATH_MODE:
	case LFUN_MATH_SUBSCRIPT:
	case LFUN_MATH_SUPERSCRIPT:
		code = MATH_HULL_CODE;
		break;

	case LFUN_MATH_MACRO:
		code = MATHMACRO_CODE;
		break;

	case LFUN_REGEXP_MODE:
		code = MATH_HULL_CODE;
		enable = cur.buffer()->isInternal() && !cur.inRegexped();
		break;

	case LFUN_INSET_MODIFY:
		// We need to disable this, because we may get called for a
		// tabular cell via
		// InsetTabular::getStatus() -> InsetText::getStatus()
		// and we don't handle LFUN_INSET_MODIFY.
		enable = false;
		break;

	case LFUN_FONT_EMPH:
		status.setOnOff(fontinfo.emph() == FONT_ON);
		enable = !cur.paragraph().isPassThru();
		break;

	case LFUN_FONT_ITAL:
		status.setOnOff(fontinfo.shape() == ITALIC_SHAPE);
		enable = !cur.paragraph().isPassThru();
		break;

	case LFUN_FONT_NOUN:
		status.setOnOff(fontinfo.noun() == FONT_ON);
		enable = !cur.paragraph().isPassThru();
		break;

	case LFUN_FONT_BOLD:
	case LFUN_FONT_BOLDSYMBOL:
		status.setOnOff(fontinfo.series() == BOLD_SERIES);
		enable = !cur.paragraph().isPassThru();
		break;

	case LFUN_FONT_SANS:
		status.setOnOff(fontinfo.family() == SANS_FAMILY);
		enable = !cur.paragraph().isPassThru();
		break;

	case LFUN_FONT_ROMAN:
		status.setOnOff(fontinfo.family() == ROMAN_FAMILY);
		enable = !cur.paragraph().isPassThru();
		break;

	case LFUN_FONT_TYPEWRITER:
		status.setOnOff(fontinfo.family() == TYPEWRITER_FAMILY);
		enable = !cur.paragraph().isPassThru();
		break;

	case LFUN_CUT:
		enable = cur.selection();
		break;

	case LFUN_PASTE: {
		if (cmd.argument().empty()) {
			if (theClipboard().isInternal())
				enable = cap::numberOfSelections() > 0;
			else
				enable = !theClipboard().empty();
			break;
		}

		// we have an argument
		string const arg = to_utf8(cmd.argument());
		if (isStrUnsignedInt(arg)) {
			// it's a number and therefore means the internal stack
			unsigned int n = convert<unsigned int>(arg);
			enable = cap::numberOfSelections() > n;
			break;
		}

		// explicit text type?
		if (arg == "html") {
			// Do not enable for PlainTextType, since some tidying in the
			// frontend is needed for HTML, which is too unsafe for plain text.
			enable = theClipboard().hasTextContents(Clipboard::HtmlTextType);
			break;
		} else if (arg == "latex") {
			// LaTeX is usually not available on the clipboard with
			// the correct MIME type, but in plain text.
			enable = theClipboard().hasTextContents(Clipboard::PlainTextType) ||
			         theClipboard().hasTextContents(Clipboard::LaTeXTextType);
			break;
		}

		Clipboard::GraphicsType type = Clipboard::AnyGraphicsType;
		if (arg == "pdf")
			type = Clipboard::PdfGraphicsType;
		else if (arg == "png")
			type = Clipboard::PngGraphicsType;
		else if (arg == "jpeg")
			type = Clipboard::JpegGraphicsType;
		else if (arg == "linkback")
			type = Clipboard::LinkBackGraphicsType;
		else if (arg == "emf")
			type = Clipboard::EmfGraphicsType;
		else if (arg == "wmf")
			type = Clipboard::WmfGraphicsType;
		else {
			// unknown argument
			LYXERR0("Unrecognized graphics type: " << arg);
			// we don't want to assert if the user just mistyped the LFUN
			LATTEST(cmd.origin() != FuncRequest::INTERNAL);
			enable = false;
			break;
		}
		enable = theClipboard().hasGraphicsContents(type);
		break;
	}

	case LFUN_CLIPBOARD_PASTE:
	case LFUN_CLIPBOARD_PASTE_SIMPLE:
		enable = !theClipboard().empty();
		break;

	case LFUN_PRIMARY_SELECTION_PASTE:
		status.setUnknown(!theSelection().supported());
		enable = cur.selection() || !theSelection().empty();
		break;

	case LFUN_SELECTION_PASTE:
		enable = cap::selection();
		break;

	case LFUN_PARAGRAPH_MOVE_UP:
		enable = cur.pit() > 0 && !cur.selection();
		break;

	case LFUN_PARAGRAPH_MOVE_DOWN:
		enable = cur.pit() < cur.lastpit() && !cur.selection();
		break;

	case LFUN_CHANGE_ACCEPT:
	case LFUN_CHANGE_REJECT:
		if (!cur.selection())
			enable = cur.paragraph().isChanged(cur.pos());
		else {
			// will enable if there is a change in the selection
			enable = false;

			// cheap improvement for efficiency: using cached
			// buffer variable, if there is no change in the
			// document, no need to check further.
			if (!cur.buffer()->areChangesPresent())
				break;

			for (DocIterator it = cur.selectionBegin(); ; it.forwardPar()) {
				pos_type const beg = it.pos();
				pos_type end;
				bool const in_last_par = (it.pit() == cur.selectionEnd().pit() &&
							  it.idx() == cur.selectionEnd().idx());
				if (in_last_par)
					end = cur.selectionEnd().pos();
				else
					// the +1 is needed for cases, e.g., where there is a
					// paragraph break. See #11629.
					end = it.lastpos() + 1;
				if (beg != end && it.paragraph().isChanged(beg, end)) {
					enable = true;
					break;
				}
				if (beg != end && it.paragraph().hasChangedInsets(beg, end)) {
					enable = true;
					break;
				}
				if (in_last_par)
					break;
			}
		}
		break;

	case LFUN_OUTLINE_UP:
	case LFUN_OUTLINE_DOWN:
		enable = cur.text()->getTocLevel(cur.pit()) != Layout::NOT_IN_TOC;
		break;
	case LFUN_OUTLINE_IN:
		enable = cur.text()->getTocLevel(cur.pit()) != Layout::NOT_IN_TOC
			  && cur.text()->getTocLevel(cur.pit()) !=
				cur.buffer()->params().documentClass().max_toclevel();
		break;
	case LFUN_OUTLINE_OUT:
		enable = cur.text()->getTocLevel(cur.pit()) != Layout::NOT_IN_TOC
			 && cur.text()->getTocLevel(cur.pit()) !=
				cur.buffer()->params().documentClass().min_toclevel();
		break;

	case LFUN_NEWLINE_INSERT:
		// LaTeX restrictions (labels or empty par)
		enable = !cur.paragraph().isPassThru()
			&& cur.pos() > cur.paragraph().beginOfBody();
		break;

	case LFUN_SEPARATOR_INSERT:
		// Always enabled for now
		enable = true;
		break;

	case LFUN_TAB_INSERT:
	case LFUN_TAB_DELETE:
		enable = cur.paragraph().isPassThru();
		break;

	case LFUN_GRAPHICS_SET_GROUP: {
		InsetGraphics * ins = graphics::getCurrentGraphicsInset(cur);
		if (!ins)
			enable = false;
		else
			status.setOnOff(to_utf8(cmd.argument()) == ins->getParams().groupId);
		break;
	}

	case LFUN_NEWPAGE_INSERT:
		// not allowed in description items and in the midst of sections
		code = NEWPAGE_CODE;
		enable = !inDescriptionItem(cur)
			&& (cur.text()->getTocLevel(cur.pit()) == Layout::NOT_IN_TOC
			    || cur.pos() == 0 || cur.pos() == cur.lastpos());
		break;

	case LFUN_LANGUAGE:
		enable = !cur.paragraph().isPassThru();
		status.setOnOff(cmd.getArg(0) == cur.real_current_font.language()->lang());
		break;

	case LFUN_PARAGRAPH_BREAK:
		enable = inset().allowMultiPar();
		break;

	case LFUN_SPELLING_ADD:
	case LFUN_SPELLING_ADD_LOCAL:
	case LFUN_SPELLING_REMOVE_LOCAL:
	case LFUN_SPELLING_IGNORE:
	case LFUN_SPELLING_REMOVE:
		enable = theSpellChecker() != nullptr;
		if (enable && !cmd.getArg(1).empty()) {
			// validate explicitly given language
			Language const * const lang = const_cast<Language *>(languages.getLanguage(cmd.getArg(1)));
			enable &= lang != nullptr;
		}
		break;

	case LFUN_LAYOUT:
	case LFUN_LAYOUT_TOGGLE: {
		bool const ignoreautonests = cmd.getArg(1) == "ignoreautonests";
		docstring const req_layout = ignoreautonests ? from_utf8(cmd.getArg(0)) : cmd.argument();
		docstring const layout = resolveLayout(req_layout, cur);

		// FIXME: make this work in multicell selection case
		enable = !owner_->forcePlainLayout() && !layout.empty() && !cur.selIsMultiCell();
		status.setOnOff(!owner_->forcePlainLayout() && !cur.selIsMultiCell()
		                && isAlreadyLayout(layout, cur));
		break;
	}

	case LFUN_ENVIRONMENT_SPLIT: {
		if (cmd.argument() == "outer") {
			// check if we have an environment in our nesting hierarchy
			bool res = false;
			depth_type const current_depth = cur.paragraph().params().depth();
			pit_type pit = cur.pit();
			Paragraph cpar = pars_[pit];
			while (true) {
				if (pit == 0 || cpar.params().depth() == 0)
					break;
				--pit;
				cpar = pars_[pit];
				if (cpar.params().depth() < current_depth)
					res = cpar.layout().isEnvironment();
			}
			enable = res;
			break;
		}
		else if (cmd.argument() == "previous") {
			// look if we have an environment in the previous par
			pit_type pit = cur.pit();
			Paragraph cpar = pars_[pit];
			if (pit > 0) {
				--pit;
				cpar = pars_[pit];
				enable = cpar.layout().isEnvironment();
				break;
			}
			enable = false;
			break;
		}
		else if (cur.paragraph().layout().isEnvironment()) {
			enable = cmd.argument() == "before"
				|| cur.pos() > 0 || !isFirstInSequence(cur.pit());
			break;
		}
		enable = false;
		break;
	}

	case LFUN_LAYOUT_PARAGRAPH:
	case LFUN_PARAGRAPH_PARAMS:
	case LFUN_PARAGRAPH_PARAMS_APPLY:
	case LFUN_PARAGRAPH_UPDATE:
		enable = owner_->allowParagraphCustomization();
		break;

	// FIXME: why are accent lfuns forbidden with pass_thru layouts?
	//  Because they insert COMBINING DIACRITICAL Unicode characters,
	//  that cannot be handled by LaTeX but must be converted according
	//  to the definition in lib/unicodesymbols?
	case LFUN_ACCENT_ACUTE:
	case LFUN_ACCENT_BREVE:
	case LFUN_ACCENT_CARON:
	case LFUN_ACCENT_CEDILLA:
	case LFUN_ACCENT_CIRCLE:
	case LFUN_ACCENT_CIRCUMFLEX:
	case LFUN_ACCENT_DOT:
	case LFUN_ACCENT_GRAVE:
	case LFUN_ACCENT_HUNGARIAN_UMLAUT:
	case LFUN_ACCENT_MACRON:
	case LFUN_ACCENT_OGONEK:
	case LFUN_ACCENT_TIE:
	case LFUN_ACCENT_TILDE:
	case LFUN_ACCENT_PERISPOMENI:
	case LFUN_ACCENT_UMLAUT:
	case LFUN_ACCENT_UNDERBAR:
	case LFUN_ACCENT_UNDERDOT:
	case LFUN_FONT_FRAK:
	case LFUN_FONT_SIZE:
	case LFUN_FONT_STATE:
	case LFUN_FONT_UNDERLINE:
	case LFUN_FONT_STRIKEOUT:
	case LFUN_FONT_CROSSOUT:
	case LFUN_FONT_UNDERUNDERLINE:
	case LFUN_FONT_UNDERWAVE:
	case LFUN_FONT_NO_SPELLCHECK:
	case LFUN_TEXTSTYLE_UPDATE:
		enable = !cur.paragraph().isPassThru();
		break;

	case LFUN_FONT_DEFAULT: {
		Font font(inherit_font, ignore_language);
		BufferParams const & bp = cur.buffer()->masterParams();
		if (cur.selection()) {
			enable = false;
			// Check if we have a non-default font attribute
			// in the selection range.
			DocIterator const from = cur.selectionBegin();
			DocIterator const to = cur.selectionEnd();
			for (DocIterator dit = from ; dit != to && !dit.atEnd(); ) {
				if (!dit.inTexted()) {
					dit.forwardPos();
					continue;
				}
				Paragraph const & par = dit.paragraph();
				pos_type const pos = dit.pos();
				Font tmp = par.getFontSettings(bp, pos);
				if (tmp.fontInfo() != font.fontInfo()
				    || tmp.language() != bp.language) {
					enable = true;
					break;
				}
				dit.forwardPos();
			}
			break;
		}
		// Disable if all is default already.
		enable = (cur.current_font.fontInfo() != font.fontInfo()
			  || cur.current_font.language() != bp.language);
		break;
	}

	case LFUN_TEXTSTYLE_APPLY:
		enable = !freeFonts.empty();
		break;

	case LFUN_WORD_DELETE_FORWARD:
	case LFUN_WORD_DELETE_BACKWARD:
	case LFUN_LINE_DELETE_FORWARD:
	case LFUN_WORD_FORWARD:
	case LFUN_WORD_BACKWARD:
	case LFUN_WORD_RIGHT:
	case LFUN_WORD_LEFT:
	case LFUN_CHAR_FORWARD:
	case LFUN_CHAR_FORWARD_SELECT:
	case LFUN_CHAR_BACKWARD:
	case LFUN_CHAR_BACKWARD_SELECT:
	case LFUN_CHAR_LEFT:
	case LFUN_CHAR_LEFT_SELECT:
	case LFUN_CHAR_RIGHT:
	case LFUN_CHAR_RIGHT_SELECT:
	case LFUN_UP:
	case LFUN_UP_SELECT:
	case LFUN_DOWN:
	case LFUN_DOWN_SELECT:
	case LFUN_PARAGRAPH_SELECT:
	case LFUN_PARAGRAPH_UP_SELECT:
	case LFUN_PARAGRAPH_DOWN_SELECT:
	case LFUN_LINE_BEGIN_SELECT:
	case LFUN_LINE_END_SELECT:
	case LFUN_WORD_FORWARD_SELECT:
	case LFUN_WORD_BACKWARD_SELECT:
	case LFUN_WORD_RIGHT_SELECT:
	case LFUN_WORD_LEFT_SELECT:
	case LFUN_WORD_SELECT:
	case LFUN_SECTION_SELECT:
	case LFUN_BUFFER_BEGIN:
	case LFUN_BUFFER_END:
	case LFUN_BUFFER_BEGIN_SELECT:
	case LFUN_BUFFER_END_SELECT:
	case LFUN_INSET_BEGIN:
	case LFUN_INSET_END:
	case LFUN_INSET_BEGIN_SELECT:
	case LFUN_INSET_END_SELECT:
	case LFUN_PARAGRAPH_UP:
	case LFUN_PARAGRAPH_DOWN:
	case LFUN_LINE_BEGIN:
	case LFUN_LINE_END:
	case LFUN_CHAR_DELETE_FORWARD:
	case LFUN_CHAR_DELETE_BACKWARD:
	case LFUN_WORD_UPCASE:
	case LFUN_WORD_LOWCASE:
	case LFUN_WORD_CAPITALIZE:
	case LFUN_CHARS_TRANSPOSE:
	case LFUN_SERVER_GET_XY:
	case LFUN_SERVER_SET_XY:
	case LFUN_SERVER_GET_LAYOUT:
	case LFUN_SELF_INSERT:
	case LFUN_UNICODE_INSERT:
	case LFUN_THESAURUS_ENTRY:
	case LFUN_ESCAPE:
	case LFUN_SERVER_GET_STATISTICS:
		// these are handled in our dispatch()
		enable = true;
		break;

	case LFUN_INSET_INSERT: {
		string const type = cmd.getArg(0);
		if (type == "toc") {
			code = TOC_CODE;
			// not allowed in description items
			//FIXME: couldn't this be merged in Inset::insetAllowed()?
			enable = !inDescriptionItem(cur);
		} else {
			enable = true;
		}
		break;
	}

	case LFUN_SEARCH_IGNORE: {
		bool const value = cmd.getArg(1) == "true";
		setIgnoreFormat(cmd.getArg(0), value);
		break;
	}

	default:
		return false;
	}

	if (code != NO_CODE
	    && (cur.empty()
		|| !cur.inset().insetAllowed(code)
		|| (cur.paragraph().layout().pass_thru && !allow_in_passthru)))
		enable = false;

	status.setEnabled(enable);
	return true;
}


void Text::pasteString(Cursor & cur, docstring const & clip,
		bool asParagraphs)
{
	if (!clip.empty()) {
		cur.recordUndo();
		if (asParagraphs)
			insertStringAsParagraphs(cur, clip, cur.current_font);
		else
			insertStringAsLines(cur, clip, cur.current_font);
	}
}


// FIXME: an item inset would make things much easier.
bool Text::inDescriptionItem(Cursor const & cur) const
{
	Paragraph const & par = cur.paragraph();
	pos_type const pos = cur.pos();
	pos_type const body_pos = par.beginOfBody();

	if (par.layout().latextype != LATEX_LIST_ENVIRONMENT
	    && (par.layout().latextype != LATEX_ITEM_ENVIRONMENT
		|| par.layout().margintype != MARGIN_FIRST_DYNAMIC))
		return false;

	return (pos < body_pos
		|| (pos == body_pos
		    && (pos == 0 || par.getChar(pos - 1) != ' ')));
}


std::vector<docstring> Text::getFreeFonts() const
{
	vector<docstring> ffList;

	for (auto const & f : freeFonts)
		ffList.push_back(f.first);

	return ffList;
}

} // namespace lyx
