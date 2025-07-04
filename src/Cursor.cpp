/**
 * \file Cursor.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alejandro Aguilar Sierra
 * \author Alfredo Braunstein
 * \author Dov Feldstern
 * \author André Pönitz
 * \author Stefan Schimanski
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "Cursor.h"

#include "Buffer.h"
#include "BufferView.h"
#include "CoordCache.h"
#include "CutAndPaste.h"
#include "FuncCode.h"
#include "FuncRequest.h"
#include "Language.h"
#include "Layout.h"
#include "LyXAction.h"
#include "LyXRC.h"
#include "Paragraph.h"
#include "Row.h"
#include "texstream.h"
#include "Text.h"
#include "TextMetrics.h"
#include "TocBackend.h"
#include "FuncStatus.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/gettext.h"
#include "support/lassert.h"

#include "insets/InsetLayout.h"
#include "insets/InsetTabular.h"

#include "mathed/InsetMath.h"
#include "mathed/InsetMathBrace.h"
#include "mathed/InsetMathEnsureMath.h"
#include "mathed/InsetMathScript.h"
#include "mathed/MacroTable.h"
#include "mathed/MathData.h"
#include "mathed/MathFactory.h"
#include "mathed/InsetMathMacro.h"

#include "frontends/Application.h"

#include <sstream>
#include <limits>
#include <map>
#include <algorithm>

using namespace std;

namespace lyx {

namespace {

// Find position closest to (x, y) in cell given by iter.
// Used only in mathed
DocIterator bruteFind(Cursor const & c, int x, int y)
{
	double best_dist = numeric_limits<double>::max();

	DocIterator result;

	DocIterator it = c;
	it.pos() = 0;
	DocIterator et = c;
	et.pos() = et.lastpos();
	for (size_t i = 0;; ++i) {
		int xo;
		int yo;
		Inset const * inset = &it.inset();
		CoordCache::Insets const & insetCache = c.bv().coordCache().insets();

		// FIXME: in the case where the inset is not in the cache, this
		// means that no part of it is visible on screen. In this case
		// we don't do elaborate search and we just return the forwarded
		// DocIterator at its beginning.
		if (!insetCache.has(inset)) {
			it.top().pos() = 0;
			return it;
		}

		Point const o = insetCache.xy(inset);
		inset->cursorPos(c.bv(), it.top(), c.boundary(), xo, yo);
		// Convert to absolute
		xo += o.x;
		yo += o.y;
		double d = (x - xo) * (x - xo) + (y - yo) * (y - yo);
		// '<=' in order to take the last possible position
		// this is important for clicking behind \sum in e.g. '\sum_i a'
		LYXERR(Debug::DEBUG, "i: " << i << " d: " << d
			<< " best: " << best_dist);
		if (d <= best_dist) {
			best_dist = d;
			result = it;
		}
		if (it == et)
			break;
		it.forwardPos();
	}
	return result;
}


} // namespace


//
// CursorData
//


CursorData::CursorData()
	: DocIterator(), anchor_(), selection_(false), mark_(false),
	  word_selection_(false), current_font(inherit_font)
{}


CursorData::CursorData(Buffer * buffer)
	: DocIterator(buffer), anchor_(), selection_(false), mark_(false),
	  word_selection_(false), current_font(inherit_font)
{}


CursorData::CursorData(DocIterator const & dit)
	: DocIterator(dit), anchor_(), selection_(false), mark_(false),
	  word_selection_(false), current_font(inherit_font)
{}




ostream & operator<<(ostream & os, CursorData const & cur)
{
	os << "\n cursor:                                | anchor:\n";
	for (size_t i = 0, n = cur.depth(); i != n; ++i) {
		os << " " << cur[i] << " | ";
		if (i < cur.anchor_.depth())
			os << cur.anchor_[i];
		else
			os << "-------------------------------";
		os << "\n";
	}
	for (size_t i = cur.depth(), n = cur.anchor_.depth(); i < n; ++i) {
		os << "------------------------------- | " << cur.anchor_[i] << "\n";
	}
	os << " selection: " << cur.selection_
//	   << " x_target: " << cur.x_target_
	   << " boundary: " << cur.boundary() << endl;
	return os;
}


LyXErr & operator<<(LyXErr & os, CursorData const & cur)
{
	os.stream() << cur;
	return os;
}


void CursorData::reset()
{
	clear();
	push_back(CursorSlice(buffer()->inset()));
	anchor_ = doc_iterator_begin(buffer());
	anchor_.clear();
	new_word_ = doc_iterator_begin(buffer());
	new_word_.clear();
	selection_ = false;
	mark_ = false;
}


void CursorData::setCursor(DocIterator const & cur)
{
	DocIterator::operator=(cur);
}


void CursorData::setCursorSelectionTo(DocIterator dit)
{
	size_t i = 0;
	// normalise dit
	while (i < dit.depth() && i < anchor_.depth() && dit[i] == anchor_[i])
		++i;
	if (i != dit.depth()) {
		// otherwise the cursor is already normal
		if (i == anchor_.depth())
			// dit is a proper extension of the anchor_
			dit.resize(i);
		else if (i + 1 < dit.depth()) {
			// one has dit[i] != anchor_[i] but either dit[i-1] == anchor_[i-1]
			// or i == 0. Remove excess.
			dit.resize(i + 1);
			if (dit[i] > anchor_[i])
				// place dit after the inset it was in
				++dit.pos();
		}
	}
	setCursor(dit);
	setSelection();
}


void CursorData::setCursorToAnchor()
{
	if (selection()) {
		DocIterator normal = anchor_;
		while (depth() < normal.depth())
			normal.pop_back();
		if (depth() < anchor_.depth() && top() <= anchor_[depth() - 1])
			++normal.pos();
		setCursor(normal);
	}
}


CursorSlice CursorData::normalAnchor() const
{
	if (!selection())
		return top();
	// LASSERT: There have been several bugs around this code, that seem
	// to involve failures to reset the anchor. We can at least not crash
	// in release mode by resetting it ourselves.
	if (anchor_.depth() < depth()) {
		LYXERR0("Cursor is deeper than anchor. PLEASE REPORT.\nCursor is"
		        << *this);
		const_cast<DocIterator &>(anchor_) = *this;
	}

	CursorSlice normal = anchor_[depth() - 1];
	if (depth() < anchor_.depth() && top() <= normal) {
		// anchor is behind cursor -> move anchor behind the inset
		++normal.pos();
	}
	return normal;
}


void CursorData::setSelection()
{
	selection(true);
	if (idx() == normalAnchor().idx() &&
	    pit() == normalAnchor().pit() &&
	    pos() == normalAnchor().pos())
		selection(false);
}


void CursorData::setSelection(DocIterator const & where, int n)
{
	setCursor(where);
	selection(true);
	anchor_ = where;
	pos() += n;
}


void CursorData::resetAnchor()
{
	anchor_ = *this;
	checkNewWordPosition();
}


CursorSlice CursorData::selBegin() const
{
	if (!selection())
		return top();
	return normalAnchor() < top() ? normalAnchor() : top();
}


CursorSlice CursorData::selEnd() const
{
	if (!selection())
		return top();
	return normalAnchor() > top() ? normalAnchor() : top();
}


DocIterator CursorData::selectionBegin() const
{
	if (!selection())
		return *this;

	DocIterator di;
	// FIXME: This is a work-around for the problem that
	// CursorSlice doesn't keep track of the boundary.
	if (normalAnchor() == top())
		di = anchor_.boundary() > boundary() ? anchor_ : *this;
	else
		di = normalAnchor() < top() ? anchor_ : *this;
	di.resize(depth());
	return di;
}


DocIterator CursorData::selectionEnd() const
{
	if (!selection())
		return *this;

	DocIterator di;
	// FIXME: This is a work-around for the problem that
	// CursorSlice doesn't keep track of the boundary.
	if (normalAnchor() == top())
		di = anchor_.boundary() < boundary() ? anchor_ : *this;
	else
		di = normalAnchor() > top() ? anchor_ : *this;

	if (di.depth() > depth()) {
		di.resize(depth());
		++di.pos();
		di.boundary(true);
	}
	return di;
}


namespace {

docstring parbreak(CursorData const * cur)
{
	if (cur->inset().getLayout().parbreakIgnored())
		return docstring();
	odocstringstream os;
	os << '\n';
	// only add blank line if we're not in a ParbreakIsNewline situation
	if (!cur->inset().getLayout().parbreakIsNewline()
	    && !cur->paragraph().layout().parbreak_is_newline)
		os << '\n';
	return os.str();
}

}


docstring CursorData::selectionAsString(bool const with_label, bool const skipdelete) const
{
	if (!selection())
		return docstring();

	if (inMathed())
		return cap::grabSelection(*this);

	int label = with_label
		? AS_STR_LABEL | AS_STR_INSETS : AS_STR_INSETS;
	if (skipdelete)
		label = with_label
				? AS_STR_LABEL | AS_STR_INSETS | AS_STR_SKIPDELETE
				: AS_STR_INSETS | AS_STR_SKIPDELETE;

	idx_type const startidx = selBegin().idx();
	idx_type const endidx = selEnd().idx();
	if (startidx != endidx) {
		// multicell selection
		InsetTabular * table = inset().asInsetTabular();
		LASSERT(table, return docstring());
		return table->asString(startidx, endidx);
	}

	ParagraphList const & pars = text()->paragraphs();

	pit_type const startpit = selBegin().pit();
	pit_type const endpit = selEnd().pit();
	size_t const startpos = selBegin().pos();
	size_t const endpos = selEnd().pos();

	if (startpit == endpit)
		return pars[startpit].asString(startpos, endpos, label);

	// First paragraph in selection
	docstring result = pars[startpit].
		asString(startpos, pars[startpit].size(), label)
		+ parbreak(this);

	// The paragraphs in between (if any)
	for (pit_type pit = startpit + 1; pit != endpit; ++pit) {
		Paragraph const & par = pars[pit];
		result += par.asString(0, par.size(), label)
			+ parbreak(this);
	}

	// Last paragraph in selection
	result += pars[endpit].asString(0, endpos, label);

	return result;
}


void CursorData::info(odocstream & os, bool devel_mode) const
{
	for (int i = 1, n = depth(); i < n; ++i) {
		operator[](i).inset().infoize(os);
		os << "  ";
	}
	if (pos() != 0) {
		Inset const * inset = prevInset();
		// prevInset() can return 0 in certain case.
		if (inset)
			prevInset()->infoize2(os);
	}
	if (devel_mode) {
		InsetMath * math = inset().asInsetMath();
		if (math)
			os << _(", Inset: ") << math->id();
		os << _(", Cell: ") << idx();
		os << _(", Position: ") << pos();
	}

}

docstring CursorData::currentState(bool devel_mode) const
{
	if (inMathed()) {
		odocstringstream os;
		info(os, devel_mode);
		return os.str();
	}

	if (inTexted())
		return text()->currentState(*this, devel_mode);

	return docstring();
}


void CursorData::markNewWordPosition()
{
	if (lyxrc.spellcheck_continuously && inTexted() && new_word_.empty()) {
		FontSpan nw = locateWord(WHOLE_WORD);
		if (nw.size() == 1) {
			LYXERR(Debug::DEBUG, "start new word: "
				<< " par: " << pit()
				<< " pos: " << nw.first);
			new_word_ = *this;
		}
	}
}


void CursorData::clearNewWordPosition()
{
	if (!new_word_.empty()) {
		LYXERR(Debug::DEBUG, "clear new word: "
			<< " par: " << pit()
			<< " pos: " << pos());
		new_word_.resize(0);
	}
}


void CursorData::checkNewWordPosition()
{
	if (!lyxrc.spellcheck_continuously || new_word_.empty())
		return ;
	// forget the position of the current new word if
	// 1) or the remembered position was "broken"
	// 2) or the count of nested insets changed
	// 3) the top-level inset is not the same anymore
	// 4) the cell index changed
	// 5) or the paragraph changed
	// 6) or the cursor pos is out of paragraph bound
	if (new_word_.fixIfBroken()
	    || depth() != new_word_.depth()
	    || &inset() != &new_word_.inset()
	    || pit() != new_word_.pit()
	    || idx() != new_word_.idx()
	    || new_word_.pos() > new_word_.lastpos())
			clearNewWordPosition();
	else {
		FontSpan nw = locateWord(WHOLE_WORD);
		if (!nw.empty()) {
			FontSpan ow = new_word_.locateWord(WHOLE_WORD);
			if (nw.intersect(ow).empty())
				clearNewWordPosition();
			else
				LYXERR(Debug::DEBUG, "new word: "
					   << " par: " << pit()
					   << " pos: " << nw.first << ".." << nw.last);
		} else
			clearNewWordPosition();
	}
}


void CursorData::clearSelection()
{
	selection(false);
	setWordSelection(false);
	setMark(false);
	resetAnchor();
}


int CursorData::countInsetsInSelection(InsetCode const & inset_code) const
{
	if (!selection_)
		return 0;

	DocIterator from, to;
	from = selectionBegin();
	to = selectionEnd();

	int count = 0;

	if (!from.nextInset())      //move to closest inset
		from.forwardInset();

	while (!from.empty() && from < to) {
		Inset * inset = from.nextInset();
		if (!inset)
			break;
		if (inset->lyxCode() == inset_code)
			count ++;
		from.forwardInset();
	}
	return count;
}


bool CursorData::insetInSelection(InsetCode const & inset_code) const
{
	if (!selection_)
		return false;

	DocIterator from, to;
	from = selectionBegin();
	to = selectionEnd();

	if (!from.nextInset())      //move to closest inset
		from.forwardInset();

	while (!from.empty() && from < to) {
		Inset * inset = from.nextInset();
		if (!inset)
			break;
		if (inset->lyxCode() == inset_code)
			return true;
		from.forwardInset();
	}
	return false;
}


bool CursorData::fixIfBroken()
{
	bool const broken_cursor = DocIterator::fixIfBroken();
	bool const broken_anchor = anchor_.fixIfBroken();

	if (broken_cursor || broken_anchor) {
		clearNewWordPosition();
		clearSelection();
		return true;
	}
	return false;
}


void CursorData::sanitize()
{
	DocIterator::sanitize();
	new_word_.sanitize();
	if (selection())
		anchor_.sanitize();
	else
		resetAnchor();
}


bool CursorData::undoAction()
{
	if (!buffer()->undo().undoAction(*this))
		return false;
	sanitize();
	return true;
}


bool CursorData::redoAction()
{
	if (!buffer()->undo().redoAction(*this))
		return false;
	sanitize();
	return true;
}


void CursorData::finishUndo() const
{
	buffer()->undo().finishUndo();
}


void CursorData::beginUndoGroup() const
{
	buffer()->undo().beginUndoGroup(*this);
}


void CursorData::endUndoGroup() const
{
	buffer()->undo().endUndoGroup(*this);
}


void CursorData::splitUndoGroup() const
{
	buffer()->undo().splitUndoGroup(*this);
}


void CursorData::recordUndo(pit_type from, pit_type to) const
{
	buffer()->undo().recordUndo(*this, from, to);
}


void CursorData::recordUndo(pit_type from) const
{
	buffer()->undo().recordUndo(*this, from, pit());
}


void CursorData::recordUndo(UndoKind kind) const
{
	buffer()->undo().recordUndo(*this, kind);
}


void CursorData::recordUndoInset(Inset const * inset) const
{
	buffer()->undo().recordUndoInset(*this, inset);
}


void CursorData::recordUndoFullBuffer() const
{
	buffer()->undo().recordUndoFullBuffer();
}


void CursorData::recordUndoBufferParams() const
{
	buffer()->undo().recordUndoBufferParams(*this);
}


void CursorData::recordUndoSelection() const
{
	if (inMathed()) {
		if (cap::multipleCellsSelected(*this))
			recordUndoInset();
		else
			recordUndo();
	} else {
		buffer()->undo().recordUndo(*this,
			selBegin().pit(), selEnd().pit());
	}
}


int CursorData::currentMode() const
{
	LASSERT(!empty(), return Inset::UNDECIDED_MODE);
	for (int i = depth() - 1; i >= 0; --i) {
		int res = operator[](i).inset().currentMode();
		bool locked_mode = operator[](i).inset().lockedMode();
		// Also return UNDECIDED_MODE when the mode is locked,
		// as in this case it is treated the same as TEXT_MODE
		if (res != Inset::UNDECIDED_MODE || locked_mode)
			return res;
	}
	return Inset::TEXT_MODE;
}


bool CursorData::confirmDeletion(bool const before) const
{
	if (!selection()) {
		if (Inset const * inset = before ? prevInset() : nextInset())
			return inset->confirmDeletion();
	} else {
		DocIterator dit = selectionBegin();
		CursorSlice const end = selectionEnd().top();
		for (; dit.top() < end; dit.top().forwardPos())
			if (Inset const * inset = dit.nextInset())
				if (inset->confirmDeletion())
					return true;
	}
	return false;
}



//
// Cursor
//


// be careful: this is called from the bv's constructor, too, so
// bv functions are not yet available!
Cursor::Cursor(BufferView & bv)
	: CursorData(&bv.buffer()), bv_(&bv),
	  x_target_(-1), textTargetOffset_(0),
	  x_clickpos_(-1), y_clickpos_(-1),
	  beforeDispatchPosX_(0), beforeDispatchPosY_(0)
{}


void Cursor::reset()
{
	CursorData::reset();
	clearTargetX();
}


void Cursor::setCursorData(CursorData const & data)
{
	CursorData::operator=(data);
}


bool Cursor::getStatus(FuncRequest const & cmd, FuncStatus & status) const
{
	Cursor cur = *this;

	// Try to fix cursor in case it is broken.
	cur.fixIfBroken();

	// Is this a function that acts on inset at point?
	Inset * inset = cur.nextInset();
	if (lyxaction.funcHasFlag(cmd.action(), LyXAction::AtPoint)
	    && inset && inset->getStatus(cur, cmd, status))
		return true;

	// This is, of course, a mess. Better create a new doc iterator and use
	// this in Inset::getStatus. This might require an additional
	// BufferView * arg, though (which should be avoided)
	//Cursor safe = *this;
	bool res = false;
	for ( ; cur.depth(); cur.pop()) {
		//lyxerr << "\nCursor::getStatus: cmd: " << cmd << endl << *this << endl;
		// LASSERT: Is it safe to continue here, or should we return?
		LASSERT(cur.idx() <= cur.lastidx(), /**/);
		LASSERT(cur.pit() <= cur.lastpit(), /**/);
		LASSERT(cur.pos() <= cur.lastpos(), /**/);

		// The inset's getStatus() will return 'true' if it made
		// a definitive decision on whether it want to handle the
		// request or not. The result of this decision is put into
		// the 'status' parameter.
		if (cur.inset().getStatus(cur, cmd, status)) {
			res = true;
			break;
		}
	}
	return res;
}


void Cursor::saveBeforeDispatchPosXY()
{
	// FIXME: it is known that the case where the position is not
	// known exist, in particular with previews. The question is
	// whether not saving the cursor position is bad in these cases.
	if (bv().hasPosition(*this))
		getPos(beforeDispatchPosX_, beforeDispatchPosY_);
}


void Cursor::dispatch(FuncRequest const & cmd0)
{
	LYXERR(Debug::ACTION, "Cursor::dispatch: cmd: " << cmd0 << '\n' << *this);
	if (empty())
		return;

	fixIfBroken();
	FuncRequest cmd = cmd0;
	Cursor safe = *this;
	Cursor old = *this;
	disp_ = DispatchResult();

	beginUndoGroup();

	Inset * nextins = nextInset();
	// Is this a function that acts on inset at point?
	if (lyxaction.funcHasFlag(cmd.action(), LyXAction::AtPoint) && nextins) {
		disp_.dispatched(true);
		disp_.screenUpdate(Update::FitCursor | Update::Force);
		FuncRequest tmpcmd = cmd;
		LYXERR(Debug::ACTION, "Cursor::dispatch: (AtPoint) cmd: "
			<< cmd0 << endl << *this);
		nextins->dispatch(*this, tmpcmd);
		if (disp_.dispatched()) {
			endUndoGroup();
			return;
		}
	}

	// store some values to be used inside of the handlers
	beforeDispatchCursor_ = *this;
	for (; depth(); pop(), boundary(false)) {
		LYXERR(Debug::ACTION, "Cursor::dispatch: cmd: "
			<< cmd0 << endl << *this);

		// In any of these cases, the cursor is invalid, and we should
		// try to save this document rather than crash.
		LBUFERR(pos() <= lastpos());
		LBUFERR(idx() <= lastidx());
		LBUFERR(pit() <= lastpit());

		// The common case is 'LFUN handled, need update', so make the
		// LFUN handler's life easier by assuming this as default value.
		// The handler can reset the update and val flags if necessary.
		disp_.screenUpdate(Update::FitCursor | Update::Force);
		disp_.dispatched(true);
		inset().dispatch(*this, cmd);
		if (disp_.dispatched())
			break;
	}

	// it completely to get a 'bomb early' behaviour in case this
	// object will be used again.
	if (!disp_.dispatched()) {
		LYXERR(Debug::ACTION, "RESTORING OLD CURSOR!");
		// We might have invalidated the cursor when removing an empty
		// paragraph while the cursor could not be moved out the inset
		// while we initially thought we could. This might happen when
		// a multiline inset becomes an inline inset when the second
		// paragraph is removed.
		if (safe.pit() > safe.lastpit()) {
			safe.pit() = safe.lastpit();
			safe.pos() = safe.lastpos();
		}
		operator=(safe);
		disp_.screenUpdate(Update::None);
		disp_.dispatched(false);
	} else {
		// restore the previous one because nested Cursor::dispatch calls
		// are possible which would change it
		beforeDispatchCursor_ = safe.beforeDispatchCursor_;
	}
	endUndoGroup();

	// NOTE: The code below has been copied to BufferView::dispatch.
	// If you need to modify this, please update the other one too.

	// notify insets we just left
	if (*this != old) {
		old.beginUndoGroup();
		old.fixIfBroken();
		bool badcursor = notifyCursorLeavesOrEnters(old, *this);
		if (badcursor) {
			fixIfBroken();
			bv().resetInlineCompletionPos();
		}
		old.endUndoGroup();
	}
}


DispatchResult const & Cursor::result() const
{
	return disp_;
}


void Cursor::message(docstring const & msg) const
{
	disp_.setMessage(msg);
}


void Cursor::errorMessage(docstring const & msg) const
{
	disp_.setMessage(msg);
	disp_.setError(true);
}


BufferView & Cursor::bv() const
{
	LBUFERR(bv_);
	return *bv_;
}


void Cursor::pop()
{
	LBUFERR(depth() >= 1);
	pop_back();
}


void Cursor::push(Inset & inset)
{
	push_back(CursorSlice(inset));
	// See bug #13050
	// inset.setBuffer(*buffer());
}


void Cursor::pushBackward(Inset & inset)
{
	LASSERT(!empty(), return);
	//lyxerr << "Entering inset " << t << " front" << endl;
	push(inset);
	inset.idxFirst(*this);
}


void Cursor::editInsertedInset()
{
	LASSERT(!empty(), return);
	if (pos() == 0)
		return;

	InsetMath &p = prevMath();
	if (!p.isActive())
		return;

	posBackward();
	push(p);
	p.idxFirst(*this);
	// this could be a while() loop, but only one cell is not empty in
	// cases we are interested in. The cell is not empty because we
	// have inserted the selection in there.
	if (!cell().empty()) {
		// if it is not empty, move to the next one.
		if (!inset().idxNext(*this))
			// If there is no next one, exit the inset.
			popForward();
	}
}


bool Cursor::popBackward()
{
	LASSERT(!empty(), return false);
	if (depth() == 1)
		return false;
	pop();
	return true;
}


bool Cursor::popForward()
{
	LASSERT(!empty(), return false);
	//lyxerr << "Leaving inset from in back" << endl;
	const pos_type lp = (depth() > 1) ? (*this)[depth() - 2].lastpos() : 0;
	if (depth() == 1)
		return false;
	pop();
	pos() += lastpos() - lp + 1;
	return true;
}


void Cursor::getPos(int & x, int & y) const
{
	Point p = bv().getPos(*this);
	x = p.x;
	y = p.y;
}


Row const & Cursor::textRow() const
{
	CursorSlice const & cs = innerTextSlice();
	ParagraphMetrics const & pm = bv().textMetrics(cs.text()).parMetrics(cs.pit());
	return pm.getRow(pos(), boundary());
}


bool Cursor::posVisRight(bool skip_inset)
{
	Cursor new_cur = *this; // where we will move to
	pos_type left_pos; // position visually left of current cursor
	pos_type right_pos; // position visually right of current cursor

	getSurroundingPos(left_pos, right_pos);

	LYXERR(Debug::RTL, left_pos <<"|"<< right_pos << " (pos: "<< pos() <<")");

	// Are we at an inset?
	new_cur.pos() = right_pos;
	new_cur.boundary(false);
	if (!skip_inset &&
		text()->checkAndActivateInsetVisual(new_cur, right_pos >= pos(), false)) {
		// we actually move the cursor at the end of this
		// function, for now we just keep track of the new
		// position in new_cur...
		LYXERR(Debug::RTL, "entering inset at: " << new_cur.pos());
	}

	// Are we already at rightmost pos in row?
	else if (text()->empty() || right_pos == -1) {

		new_cur = *this;
		if (!new_cur.posVisToNewRow(false)) {
			LYXERR(Debug::RTL, "not moving!");
			return false;
		}

		// we actually move the cursor at the end of this
		// function, for now just keep track of the new
		// position in new_cur...
		LYXERR(Debug::RTL, "right edge, moving: " << int(new_cur.pit()) << ","
			<< int(new_cur.pos()) << "," << (new_cur.boundary() ? 1 : 0));

	}
	// normal movement to the right
	else {
		new_cur = *this;
		// Recall, if the cursor is at position 'x', that
		// means *before* the character at position 'x'. In
		// RTL, "before" means "to the right of", in LTR, "to
		// the left of". So currently our situation is this:
		// the position to our right is 'right_pos' (i.e.,
		// we're currently to the left of 'right_pos'). In
		// order to move to the right, it depends whether or
		// not the character at 'right_pos' is RTL.
		bool const new_pos_is_RTL = paragraph().getFontSettings(
			buffer()->params(), right_pos).isVisibleRightToLeft();
		// If the character at 'right_pos' *is* LTR, then in
		// order to move to the right of it, we need to be
		// *after* 'right_pos', i.e., move to position
		// 'right_pos' + 1.
		if (!new_pos_is_RTL) {
			new_cur.pos() = right_pos + 1;
			// set the boundary to true in two situations:
			if (
			// 1. if new_pos is now lastpos, and we're in
			// an RTL paragraph (this means that we're
			// moving right to the end of an LTR chunk
			// which is at the end of an RTL paragraph);
				(new_cur.pos() == lastpos()
				 && paragraph().isRTL(buffer()->params()))
			// 2. if the position *after* right_pos is RTL
			// (we want to be *after* right_pos, not
			// before right_pos + 1!)
				|| paragraph().getFontSettings(buffer()->params(),
						new_cur.pos()).isVisibleRightToLeft()
			)
				new_cur.boundary(true);
			else // set the boundary to false
				new_cur.boundary(false);
		}
		// Otherwise (if the character at position 'right_pos'
		// is RTL), then moving to the right of it is as easy
		// as setting the new position to 'right_pos'.
		else {
			new_cur.pos() = right_pos;
			new_cur.boundary(false);
		}

	}

	bool const moved = new_cur != *this || new_cur.boundary() != boundary();

	if (moved) {
		LYXERR(Debug::RTL, "moving to: " << new_cur.pos()
			<< (new_cur.boundary() ? " (boundary)" : ""));
		*this = new_cur;
	}

	return moved;
}


bool Cursor::posVisLeft(bool skip_inset)
{
	Cursor new_cur = *this; // where we will move to
	pos_type left_pos; // position visually left of current cursor
	pos_type right_pos; // position visually right of current cursor

	getSurroundingPos(left_pos, right_pos);

	LYXERR(Debug::RTL, left_pos <<"|"<< right_pos << " (pos: "<< pos() <<")");

	// Are we at an inset?
	new_cur.pos() = left_pos;
	new_cur.boundary(false);
	if (!skip_inset &&
		text()->checkAndActivateInsetVisual(new_cur, left_pos >= pos(), true)) {
		// we actually move the cursor at the end of this
		// function, for now we just keep track of the new
		// position in new_cur...
		LYXERR(Debug::RTL, "entering inset at: " << new_cur.pos());
	}

	// Are we already at leftmost pos in row?
	else if (text()->empty() || left_pos == -1) {

		new_cur = *this;
		if (!new_cur.posVisToNewRow(true)) {
			LYXERR(Debug::RTL, "not moving!");
			return false;
		}

		// we actually move the cursor at the end of this
		// function, for now just keep track of the new
		// position in new_cur...
		LYXERR(Debug::RTL, "left edge, moving: " << int(new_cur.pit()) << ","
			<< int(new_cur.pos()) << "," << (new_cur.boundary() ? 1 : 0));

	}
	// normal movement to the left
	else {
		new_cur = *this;
		// Recall, if the cursor is at position 'x', that
		// means *before* the character at position 'x'. In
		// RTL, "before" means "to the right of", in LTR, "to
		// the left of". So currently our situation is this:
		// the position to our left is 'left_pos' (i.e., we're
		// currently to the right of 'left_pos'). In order to
		// move to the left, it depends whether or not the
		// character at 'left_pos' is RTL.
		bool const new_pos_is_RTL = paragraph().getFontSettings(
			buffer()->params(), left_pos).isVisibleRightToLeft();
		// If the character at 'left_pos' *is* RTL, then in
		// order to move to the left of it, we need to be
		// *after* 'left_pos', i.e., move to position
		// 'left_pos' + 1.
		if (new_pos_is_RTL) {
			new_cur.pos() = left_pos + 1;
			// set the boundary to true in two situations:
			if (
			// 1. if new_pos is now lastpos and we're in
			// an LTR paragraph (this means that we're
			// moving left to the end of an RTL chunk
			// which is at the end of an LTR paragraph);
				(new_cur.pos() == lastpos()
				 && !paragraph().isRTL(buffer()->params()))
			// 2. if the position *after* left_pos is not
			// RTL (we want to be *after* left_pos, not
			// before left_pos + 1!)
				|| !paragraph().getFontSettings(buffer()->params(),
						new_cur.pos()).isVisibleRightToLeft()
			)
				new_cur.boundary(true);
			else // set the boundary to false
				new_cur.boundary(false);
		}
		// Otherwise (if the character at position 'left_pos'
		// is LTR), then moving to the left of it is as easy
		// as setting the new position to 'left_pos'.
		else {
			new_cur.pos() = left_pos;
			new_cur.boundary(false);
		}

	}

	bool const moved = new_cur != *this || new_cur.boundary() != boundary();

	if (moved) {
		LYXERR(Debug::RTL, "moving to: " << new_cur.pos()
			<< (new_cur.boundary() ? " (boundary)" : ""));
		*this = new_cur;
	}

	return moved;
}


namespace {

// Return true on success
bool findNonVirtual(Row const & row, Row::const_iterator & cit, bool onleft)
{
	if (onleft) {
		while (cit != row.begin() && cit->isVirtual())
			--cit;
	} else {
		while (cit != row.end() && cit->isVirtual())
			++cit;
	}
	return cit != row.end() && !cit->isVirtual();
}

} // namespace

void Cursor::getSurroundingPos(pos_type & left_pos, pos_type & right_pos) const
{
	// by default, we know nothing.
	left_pos = -1;
	right_pos = -1;

	Row const & row = textRow();
	Row::const_iterator cit = row.findElement(pos(), boundary());
	// Handle the case of empty row
	if (cit == row.end()) {
		if (row.isRTL())
			right_pos = row.pos();
		else
			left_pos = row.pos() - 1;
		return;
	}

	// skip virtual elements and exit if no non-virtual one exists
	if (!findNonVirtual(row, cit, !cit->isRTL()))
		return;

	// if the position is at the left side of the element, we have to
	// look at the previous element
	if (pos() == cit->left_pos()) {
		LYXERR(Debug::RTL, "getSurroundingPos(" << pos() << (boundary() ? "b" : "")
			   << "), AT LEFT of *cit=" << *cit);
		// this one is easy (see common case below)
		right_pos = pos() - (cit->isRTL() ? 1 : 0);
		// at the left of the row
		if (cit == row.begin())
			return;
		--cit;
		if (!findNonVirtual(row, cit, true))
			return;
		// [...[ is the row element, | is cursor position (! with boundary)
		// [ 1 2 [ is a ltr row element with pos=1 and endpos=3
		// ] 2 1] is an rtl row element with pos=1 and endpos=3
		//    [ 1 2 [  [|3 4 [ => (2, 3)
		// or [ 1 2 [  ]!4 3 ] => (2, 4)
		// or ] 2 1 ]  [|3 4 [ => (1, 3)
		// or ] 4 3 ]  ]!2 1 ] => (3, 2)
		left_pos = cit->right_pos() - (cit->isRTL() ? 0 : 1);
		// happens with consecutive row of same direction
		if (left_pos == right_pos) {
			left_pos += cit->isRTL() ? 1 : -1;
		}
	}
	// same code but with the element at the right
	else if (pos() == cit->right_pos()) {
		LYXERR(Debug::RTL, "getSurroundingPos(" << pos() << (boundary() ? "b" : "")
			   << "), AT RIGHT of *cit=" << *cit);
		// this one is easy (see common case below)
		left_pos = pos() - (cit->isRTL() ? 0 : 1);
		// at the right of the row
		if (cit + 1 == row.end())
			return;
		++cit;
		if (!findNonVirtual(row, cit, false))
			return;
		//    [ 1 2![  [ 3 4 [ => (2, 3)
		// or [ 1 2![  ] 4 3 ] => (2, 4)
		// or ] 2 1|]  [ 3 4 [ => (1, 3)
		// or ] 4 3|]  ] 2 1 ] => (3, 2)
		right_pos = cit->left_pos() - (cit->isRTL() ? 1 : 0);
		// happens with consecutive row of same direction
		if (right_pos == left_pos)
			right_pos += cit->isRTL() ? -1 : 1;
	}
	// common case: both positions are inside the row element
	else {
		//    [ 1 2|3 [ => (2, 3)
		// or ] 3|2 1 ] => (3, 2)
		left_pos = pos() - (cit->isRTL() ? 0 : 1);
		right_pos = pos() - (cit->isRTL() ? 1 : 0);
	}

	// Note that debug message does not catch all early returns above
	LYXERR(Debug::RTL,"getSurroundingPos(" << pos() << (boundary() ? "b" : "")
		   << ") => (" << left_pos << ", " << right_pos <<")");
}


bool Cursor::posVisToNewRow(bool movingLeft)
{
	Row const & row = textRow();
	bool par_is_LTR = !row.isRTL();

	// Inside a table, determining whether to move to the next or
	// previous row should be done based on the table's direction.
	if (inset().asInsetTabular()) {
		par_is_LTR = !inset().asInsetTabular()->isRightToLeft(*this);
		LYXERR(Debug::RTL, "Inside table! par_is_LTR=" << (par_is_LTR ? 1 : 0));
	}

	// if moving left in an LTR paragraph or moving right in an
	// RTL one, move to previous row
	if (par_is_LTR == movingLeft) {
		if (row.start_boundary())
			boundary(true);
		else if (row.pos() == 0) { // we're at first row in paragraph
			if (pit() == 0) // no previous paragraph! don't move
				return false;
			// move to last pos in previous par
			--pit();
			pos() = lastpos();
			boundary(false);
		} else { // move to previous row in this par
			pos() = row.pos() - 1; // this is guaranteed to be in previous row
			boundary(false);
		}
	}
	// if moving left in an RTL paragraph or moving right in an
	// LTR one, move to next row
	else {
		if (row.endpos() == lastpos()) { // we're at last row in paragraph
			if (pit() == lastpit()) // last paragraph! don't move
				return false;
			// move to first row in next par
			++pit();
			pos() = 0;
			boundary(false);
		} else { // move to next row in this par
			pos() = row.endpos();
			boundary(false);
		}
	}

	// make sure we're at left-/right-most pos in new row
	posVisToRowExtremity(!movingLeft);

	return true;
}


void Cursor::posVisToRowExtremity(bool left)
{
	LYXERR(Debug::RTL, "entering extremity: " << pit() << "," << pos() << ","
		<< (boundary() ? 1 : 0));

	// FIXME: no need for metrics here!
	TextMetrics const & tm = bv_->textMetrics(text());
	// Looking for extremities is like clicking on the left or the
	// right of the row.
	int x = tm.origin().x + (left ? 0 : textRow().width());
	auto [p, b] = tm.getPosNearX(textRow(), x);
	pos() = p;
	boundary(b);

	LYXERR(Debug::RTL, "leaving extremity: " << pit() << "," << pos() << ","
		<< (boundary() ? 1 : 0));
}


bool Cursor::reverseDirectionNeeded() const
{
	/*
	 * We determine the directions based on the direction of the
	 * bottom() --- i.e., outermost --- paragraph, because that is
	 * the only way to achieve consistency of the arrow's movements
	 * within a paragraph, and thus avoid situations in which the
	 * cursor gets stuck.
	 */
	return bottom().paragraph().isRTL(bv().buffer().params());
}


void Cursor::setTargetX(int x)
{
	x_target_ = x;
	textTargetOffset_ = 0;
}


int Cursor::x_target() const
{
	return x_target_;
}


void Cursor::clearTargetX()
{
	x_target_ = -1;
	textTargetOffset_ = 0;
}


void Cursor::updateTextTargetOffset()
{
	int x;
	int y;
	getPos(x, y);
	textTargetOffset_ = x - x_target_;
}


void Cursor::setClickPos(int x, int y)
{
	x_clickpos_ = x;
	y_clickpos_ = y;
}


bool Cursor::selHandle(bool selecting)
{
	//lyxerr << "Cursor::selHandle" << endl;
	if (mark())
		selecting = true;
	if (selecting == selection())
		return false;

	if (!selecting)
		cap::saveSelection(*this);

	resetAnchor();
	selection(selecting);
	return true;
}


bool Cursor::atFirstOrLastRow(bool up)
{
	TextMetrics const & tm = bv_->textMetrics(text());
	ParagraphMetrics const & pm = tm.parMetrics(pit());

	int const row = pm.getRowIndex(pos(), boundary());

	if (up) {
		if (pit() == 0 && row == 0)
			return true;
	} else {
		if (pit() + 1 >= int(text()->paragraphs().size()) &&
			row + 1 >= int(pm.rows().size()))
			return true;
	}
	return false;
}


} // namespace lyx


///////////////////////////////////////////////////////////////////
//
// FIXME: Look here
// The part below is the non-integrated rest of the original math
// cursor. This should be either generalized for texted or moved
// back to mathed (in most cases to InsetMathNest).
//
///////////////////////////////////////////////////////////////////

#include "mathed/InsetMathChar.h"
#include "mathed/InsetMathUnknown.h"
#include "mathed/MathStream.h"
#include "mathed/MathSupport.h"


namespace lyx {

bool Cursor::openable(MathAtom const & t) const
{
	if (!t->isActive())
		return false;

	if (t->lock())
		return false;

	if (!selection())
		return true;

	// we can't move into anything new during selection
	if (depth() >= realAnchor().depth())
		return false;
	if (t.nucleus() != &realAnchor()[depth()].inset())
		return false;

	return true;
}


void Cursor::plainErase()
{
	cell().erase(pos());
}


void Cursor::plainInsert(MathAtom const & t)
{
	cell().insert(pos(), t);
	++pos();
	inset().setBuffer(bv_->buffer());
	inset().initView();
	checkBufferStructure();
}


void Cursor::insert(char_type c)
{
	//lyxerr << "Cursor::insert char '" << c << "'" << endl;
	LASSERT(!empty(), return);
	if (inMathed()) {
		cap::selClearOrDel(*this);
		insert(new InsetMathChar(buffer(), c));
	} else {
		text()->insertChar(*this, c);
	}
}


void Cursor::insert(docstring const & str)
{
	for (char_type c : str)
		insert(c);
}


void Cursor::insert(Inset * inset0)
{
	LASSERT(inset0, return);
	if (inMathed())
		insert(MathAtom(inset0->asInsetMath()));
	else {
		text()->insertInset(*this, inset0);
		inset0->setBuffer(bv_->buffer());
		inset0->initView();
		if (inset0->isLabeled())
			forceBufferUpdate();
	}
}


void Cursor::insert(MathAtom const & t)
{
	LATTEST(inMathed());
	//lyxerr << "Cursor::insert MathAtom '" << t << "'" << endl;
	macroModeClose();
	cap::selClearOrDel(*this);
	plainInsert(t);
}


void Cursor::insert(MathData const & md)
{
	LATTEST(inMathed());
	macroModeClose();
	if (selection())
		cap::eraseSelection(*this);
	cell().insert(pos(), md);
	pos() += md.size();
	// FIXME audit setBuffer calls
	inset().setBuffer(bv_->buffer());
}


int Cursor::niceInsert(docstring const & t, Parse::flags f, bool enter)
{
	LATTEST(inMathed());
	MathData md(buffer());
	asMathData(t, md, f);
	if (md.size() == 1 && (enter || selection()))
		niceInsert(md[0]);
	else
		insert(md);
	return md.size();
}


void Cursor::niceInsert(MathAtom const & t)
{
	LATTEST(inMathed());
	macroModeClose();
	docstring const safe = cap::grabAndEraseSelection(*this);
	plainInsert(t);
	// If possible, enter the new inset and move the contents of the selection
	if (t->isActive()) {
		idx_type const idx = prevMath().asNestInset()->firstIdx();
		MathData md(buffer());
		asMathData(safe, md);
		prevMath().cell(idx).insert(0, md);
		editInsertedInset();
	} else if (t->asMacroInset() && !safe.empty()) {
		MathData md(buffer());
		asMathData(safe, md);
		docstring const name = t->asMacroInset()->name();
		MacroData const * data = buffer()->getMacro(name);
		if (data && data->numargs() - data->optionals() > 0) {
			plainInsert(MathAtom(new InsetMathBrace(buffer(), md)));
			posBackward();
		}
	}
}


bool Cursor::backspace(bool const force)
{
	if (selection()) {
		cap::eraseSelection(*this);
		return true;
	}

	if (pos() == 0) {
		// If empty cell, and not part of a big cell
		if (lastpos() == 0 && inset().nargs() == 1) {
			popBackward();
			// Directly delete empty cell: [|[]] => [|]
			if (inMathed()) {
				plainErase();
				resetAnchor();
				return true;
			}
			// [|], can not delete from inside
			return false;
		} else {
			if (inMathed()) {
				switch (inset().asInsetMath()->getType()) {
				case hullEqnArray:
				case hullAlign:
				case hullFlAlign: {
					FuncRequest cmd(LFUN_CHAR_BACKWARD);
					this->dispatch(cmd);
					break;
				}
				default:
					pullArg();
					break;
				}
			} else
				popBackward();
			return true;
		}
	}

	if (inMacroMode()) {
		InsetMathUnknown * p = activeMacro();
		if (p->name().size() > 1) {
			p->setName(p->name().substr(0, p->name().size() - 1));
			return true;
		}
	}

	if (pos() != 0 && !force && prevAtom()->confirmDeletion()) {
		// let's require two backspaces for 'big stuff' and
		// highlight on the first
		resetAnchor();
		selection(true);
		--pos();
	} else {
		--pos();
		plainErase();
	}
	return true;
}


bool Cursor::erase(bool const force)
{
	if (inMacroMode())
		return true;

	if (selection()) {
		cap::eraseSelection(*this);
		return true;
	}

	// delete empty cells if possible
	if (pos() == lastpos() && inset().idxDelete(idx()))
		return true;

	// special behaviour when in last position of cell
	if (pos() == lastpos()) {
		bool one_cell = inset().nargs() == 1;
		if (one_cell && lastpos() == 0) {
			popBackward();
			// Directly delete empty cell: [|[]] => [|]
			if (inMathed()) {
				plainErase();
				resetAnchor();
				return true;
			}
			// [|], can not delete from inside
			return false;
		}
		// remove markup
		if (!one_cell)
			inset().idxGlue(idx());
		return true;
	}

	// 'clever' UI hack: only erase large items if previously slected
	if (pos() != lastpos() && !force && nextAtom()->confirmDeletion()) {
		resetAnchor();
		selection(true);
		++pos();
	} else {
		plainErase();
	}

	return true;
}


bool Cursor::up()
{
	macroModeClose();
	DocIterator save = *this;
	FuncRequest cmd(selection() ? LFUN_UP_SELECT : LFUN_UP, docstring());
	this->dispatch(cmd);
	if (disp_.dispatched())
		return true;
	setCursor(save);
	return false;
}


bool Cursor::down()
{
	macroModeClose();
	DocIterator save = *this;
	FuncRequest cmd(selection() ? LFUN_DOWN_SELECT : LFUN_DOWN, docstring());
	this->dispatch(cmd);
	if (disp_.dispatched())
		return true;
	setCursor(save);
	return false;
}


void Cursor::handleNest(MathAtom const & a)
{
	idx_type const idx = a.nucleus()->asNestInset()->firstIdx();
	//lyxerr << "Cursor::handleNest: " << idx << endl;
	InsetMath const * im = selectionBegin().inset().asInsetMath();
	Parse::flags const f = im && im->currentMode() != InsetMath::MATH_MODE
		? Parse::TEXTMODE : Parse::NORMAL;
	MathAtom t = a;
	asMathData(cap::grabAndEraseSelection(*this), t.nucleus()->cell(idx), f);
	insert(t);
	editInsertedInset();
}


int Cursor::targetX() const
{
	if (x_target() != -1)
		return x_target();
	int x = 0;
	int y = 0;
	getPos(x, y);
	return x;
}


int Cursor::textTargetOffset() const
{
	return textTargetOffset_;
}


void Cursor::setTargetX()
{
	int x;
	int y;
	getPos(x, y);
	setTargetX(x);
}


bool Cursor::macroModeClose(bool cancel)
{
	if (!inMacroMode())
		return false;
	InsetMathUnknown * p = activeMacro();
	p->finalize();
	MathData selection(buffer());
	asMathData(p->selection(), selection);
	docstring const s = p->name();
	--pos();
	cell().erase(pos());

	// trigger updates of macros, at least, if no full
	// updates take place anyway
	screenUpdateFlags(Update::Force);

	// do nothing if the macro name is empty
	if (s == "\\" || cancel) {
		return false;
	}

	docstring const name = s.substr(1);
	InsetMathNest * const in = inset().asInsetMath()->asNestInset();
	if (in && in->interpretString(*this, s))
		return true;
	bool const user_macro = buffer()->getMacro(name, *this, false);

	MathAtom atom;
	FuncRequest fr(LFUN_MATH_INSERT, "\\" + name);
	FuncStatus status;
	getStatus(fr, status);
	// exit if the inset cannot be used e.g. in the current math hull
	if (!status.enabled())
		return false;
	else
		atom = user_macro ? MathAtom(new InsetMathMacro(buffer(), name))
					   : createInsetMath(name, buffer());

	// try to put argument into macro, if we just inserted a macro
	bool macroArg = false;
	InsetMathMacro * atomAsMacro = atom.nucleus()->asMacroInset();
	InsetMathNest * atomAsNest = atom.nucleus()->asNestInset();
	if (atomAsMacro) {
		// macros here are still unfolded (in init mode in fact). So
		// we have to resolve the macro here manually and check its arity
		// to put the selection behind it if arity > 0.
		MacroData const * data = buffer()->getMacro(atomAsMacro->name());
		if (!selection.empty() && data && data->numargs()) {
			macroArg = true;
			atomAsMacro->setDisplayMode(InsetMathMacro::DISPLAY_INTERACTIVE_INIT, 1);
		} else
			// non-greedy case. Do not touch the arguments behind
			atomAsMacro->setDisplayMode(InsetMathMacro::DISPLAY_INTERACTIVE_INIT, 0);
	}

	// insert remembered selection into first argument of a non-macro
	else if (atomAsNest && atomAsNest->isActive())
		atomAsNest->cell(atomAsNest->firstIdx()).append(selection);

	MathWordList const & words = mathedWordList();
	MathWordList::const_iterator it = words.find(name);
	bool keep_mathmode = user_macro
		|| (it != words.end() && (it->second.inset == "font"
		                          || it->second.inset == "oldfont"
		                          || it->second.inset == "textsize"
		                          || it->second.inset == "mbox"
		                          || it->second.inset == "intertext"
		                          || it->second.name == "underbar"));
	bool ert_macro = !user_macro && it == words.end() && atomAsMacro;

	if (in && in->currentMode() == Inset::TEXT_MODE
	    && atom.nucleus()->currentMode() == Inset::MATH_MODE
	    && name != from_ascii("ensuremath") && !ert_macro) {
		MathAtom at(new InsetMathEnsureMath(buffer()));
		at.nucleus()->cell(0).push_back(atom);
		niceInsert(at);
		posForward();
	} else if (in && in->currentMode() == Inset::MATH_MODE
		   && atom.nucleus()->currentMode() == Inset::TEXT_MODE
		   && !keep_mathmode) {
		MathAtom at = createInsetMath("text", buffer());
		at.nucleus()->cell(0).push_back(atom);
		niceInsert(at);
		posForward();
	} else
		plainInsert(atom);

	// finally put the macro argument behind, if needed
	if (macroArg) {
		if (selection.size() > 1 || selection[0]->asScriptInset())
			plainInsert(MathAtom(new InsetMathBrace(buffer(), selection)));
		else
			insert(selection);
	}

	return true;
}


bool Cursor::inMacroMode() const
{
	if (!inMathed())
		return false;
	if (pos() == 0 || cell().empty())
		return false;
	InsetMathUnknown const * p = prevAtom()->asUnknownInset();
	return p && !p->final();
}


InsetMathUnknown * Cursor::activeMacro()
{
	return inMacroMode() ? prevAtom().nucleus()->asUnknownInset() : nullptr;
}


InsetMathUnknown const * Cursor::activeMacro() const
{
	return inMacroMode() ? prevAtom().nucleus()->asUnknownInset() : nullptr;
}


docstring Cursor::macroName()
{
	return inMacroMode() ? activeMacro()->name() : docstring();
}


void Cursor::pullArg()
{
	// FIXME: Look here
	MathData md = cell();
	if (popBackward() && inMathed()) {
		plainErase();
		cell().insert(pos(), md);
		resetAnchor();
	}
}


void Cursor::normalize()
{
	if (idx() > lastidx()) {
		lyxerr << "this should not really happen - 1: "
		       << idx() << ' ' << nargs()
		       << " in: " << &inset() << endl;
		idx() = lastidx();
	}

	if (pos() > lastpos()) {
		lyxerr << "this should not really happen - 2: "
			<< pos() << ' ' << lastpos() <<  " in idx: " << idx()
		       << " in atom: '";
		odocstringstream os;
		otexrowstream ots(os);
		TeXMathStream wi(ots, false, true, TeXMathStream::wsDefault);
		inset().asInsetMath()->write(wi);
		lyxerr << to_utf8(os.str()) << endl;
		pos() = lastpos();
	}
}


bool Cursor::upDownInMath(bool up)
{
	// Be warned: The 'logic' implemented in this function is highly
	// fragile. A distance of one pixel or a '<' vs '<=' _really
	// matters. So fiddle around with it only if you think you know
	// what you are doing!
	int xo = 0;
	int yo = 0;
	getPos(xo, yo);
	xo = beforeDispatchPosX_;

	// check if we had something else in mind, if not, this is the future
	// target
	if (x_target_ == -1)
		setTargetX(xo);
	else if (inset().asInsetText() && xo - textTargetOffset() != x_target()) {
		// In text mode inside the line (not left or right) possibly set a new target_x,
		// but only if we are somewhere else than the previous target-offset.

		// We want to keep the x-target on subsequent up/down movements
		// that cross beyond the end of short lines. Thus a special
		// handling when the cursor is at the end of line: Use the new
		// x-target only if the old one was before the end of line
		// or the old one was after the beginning of the line
		bool inRTL = innerParagraph().isRTL(bv().buffer().params());
		bool left;
		bool right;
		if (inRTL) {
			left = pos() == textRow().endpos();
			right = pos() == textRow().pos();
		} else {
			left = pos() == textRow().pos();
			right = pos() == textRow().endpos();
		}
		if ((!left && !right) ||
				(left && !right && xo < x_target_) ||
				(!left && right && x_target_ < xo))
			setTargetX(xo);
		else
			xo = targetX();
	} else
		xo = targetX();

	// try neigbouring script insets
	Cursor old = *this;
	if (inMathed() && !selection()) {
		// try left
		if (pos() != 0) {
			InsetMathScript const * p = prevAtom()->asScriptInset();
			if (p && p->has(up)) {
				--pos();
				push(*const_cast<InsetMathScript*>(p));
				idx() = p->idxOfScript(up);
				pos() = lastpos();

				// we went in the right direction? Otherwise don't jump into the script
				int x;
				int y;
				getPos(x, y);
				int oy = beforeDispatchPosY_;
				if ((!up && y <= oy) ||
						(up && y >= oy))
					operator=(old);
				else
					return true;
			}
		}

		// try right
		if (pos() != lastpos()) {
			InsetMathScript const * p = nextAtom()->asScriptInset();
			if (p && p->has(up)) {
				push(*const_cast<InsetMathScript*>(p));
				idx() = p->idxOfScript(up);
				pos() = 0;

				// we went in the right direction? Otherwise don't jump into the script
				int x;
				int y;
				getPos(x, y);
				int oy = beforeDispatchPosY_;
				if ((!up && y <= oy) ||
						(up && y >= oy))
					operator=(old);
				else
					return true;
			}
		}
	}

	// try to find an inset that knows better than we do
	if (inset().idxUpDown(*this, up)) {
		//lyxerr << "idxUpDown triggered" << endl;
		// try to find best position within this inset
		if (!selection())
			setCursor(bruteFind(*this, xo, yo));
		// FIXME : this is actually only needed for InsetMathMacro (bug #12952).
		screenUpdateFlags(Update::SinglePar);
		return true;
	}

	// any improvement going just out of inset?
	if (popBackward() && inMathed()) {
		//lyxerr << "updown: popBackward succeeded" << endl;
		int xnew;
		int ynew;
		int yold = beforeDispatchPosY_;
		getPos(xnew, ynew);
		if (up ? ynew < yold : ynew > yold)
			return true;
	}

	// no success, we are probably at the document top or bottom
	operator=(old);
	return false;
}


bool Cursor::mathForward(bool word)
{
	LASSERT(inMathed(), return false);
	if (pos() < lastpos()) {
		if (word) {
			// word: skip a group of insets of the form X*(B*|R*|P*) (greedy
			// match) where X is any math class, B is mathbin, R is mathrel, and
			// P is mathpunct. Make sure that the following remains true:
			//   mathForward(true); mathBackward(true); mathForward(true)
			// is the same as mathForward(true) and
			//   mathBackward(true); mathForward(true); mathBackward(true)
			// is the same as mathBackward(true).
			MathClass mc = nextMath().mathClass();
			do
				posForward();
			while (pos() < lastpos() && mc == nextMath().mathClass());
			if (pos() < lastpos() &&
			    ((mc = nextMath().mathClass()) == MC_BIN ||
			     mc == MC_REL || mc == MC_PUNCT))
				do
					posForward();
				while (pos() < lastpos() && mc == nextMath().mathClass());
		} else if (openable(nextAtom())) {
			InsetMathScript const * n = nextMath().asScriptInset();
			bool to_brace_deco = n && !n->nuc().empty()
				&& n->nuc().back()->lyxCode() == MATH_DECORATION_CODE
				&& n->nuc().back()->mathClass() == MC_OP;
			// single step: try to enter the next inset
			pushBackward(nextMath());
			inset().idxFirst(*this);
			// Make sure the cursor moves directly to an
			// \overbrace or \underbrace inset (bug 2264)
			if (to_brace_deco) {
				pushBackward(nextMath());
				inset().idxFirst(*this);
			}
		} else
			posForward();
		return true;
	}
	if (inset().idxForward(*this))
		return true;
	InsetMath const * m = inset().asInsetMath();
	bool from_brace_deco = m
		&& m->lyxCode() == MATH_DECORATION_CODE
		&& m->mathClass() == MC_OP;
	// try to pop forwards --- but don't pop out of math! leave that to
	// the FINISH lfuns
	int s = depth() - 2;
	if (s >= 0 && operator[](s).inset().asInsetMath() && popForward()) {
		// Make sure the cursor moves directly to an
		// \overbrace or \underbrace inset (bug 2264)
		bool to_script = inset().asInsetMath()
			&& inset().asInsetMath()->asScriptInset();
		return from_brace_deco && to_script ? mathForward(word) : true;
	}
	return false;
}


bool Cursor::mathBackward(bool word)
{
	LASSERT(inMathed(), return false);
	if (pos() > 0) {
		if (word) {
			// word: skip a group of insets. See the comment in mathForward.
			MathClass mc = prevMath().mathClass();
			do
				posBackward();
			while (pos() > 0 && mc == prevMath().mathClass());
			if (pos() > 0 && (mc == MC_BIN || mc == MC_REL || mc == MC_PUNCT)) {
				mc = prevMath().mathClass();
				do
					posBackward();
				while (pos() > 0 && mc == prevMath().mathClass());
			}
		} else if (openable(prevAtom())) {
			InsetMathScript const * p = prevMath().asScriptInset();
			bool to_brace_deco = p && !p->nuc().empty()
				&& p->nuc().back()->lyxCode() == MATH_DECORATION_CODE
				&& p->nuc().back()->mathClass() == MC_OP;
			// single step: try to enter the preceding inset
			posBackward();
			push(nextMath());
			inset().idxLast(*this);
			// Make sure the cursor moves directly to an
			// \overbrace or \underbrace inset (bug 2264)
			if (to_brace_deco) {
				posBackward();
				push(nextMath());
				inset().idxLast(*this);
			}
		} else
			posBackward();
		return true;
	}
	if (inset().idxBackward(*this))
		return true;
	InsetMath const * m = inset().asInsetMath();
	bool from_brace_deco = m
		&& m->lyxCode() == MATH_DECORATION_CODE
		&& m->mathClass() == MC_OP;
	// try to pop backwards --- but don't pop out of math! leave that to
	// the FINISH lfuns
	int s = depth() - 2;
	if (s >= 0 && operator[](s).inset().asInsetMath() && popBackward()) {
		// Make sure the cursor moves directly to an
		// \overbrace or \underbrace inset (bug 2264)
		bool to_script = inset().asInsetMath()
			&& inset().asInsetMath()->asScriptInset();
		return from_brace_deco && to_script ? mathBackward(word) : true;
	}
	return false;
}


bool Cursor::upDownInText(bool up)
{
	LASSERT(text(), return false);

	// where are we?
	int xo = 0;
	int yo = 0;
	getPos(xo, yo);
	xo = beforeDispatchPosX_;

	// Is a full repaint necessary?
	bool updateNeeded = false;

	// update the targetX - this is here before the "return false"
	// to set a new target which can be used by InsetTexts above
	// if we cannot move up/down inside this inset anymore
	if (x_target_ == -1)
		setTargetX(xo);
	else if (xo - textTargetOffset() != x_target() &&
					 depth() == beforeDispatchCursor_.depth()) {
		// In text mode inside the line (not left or right)
		// possibly set a new target_x, but only if we are
		// somewhere else than the previous target-offset.

		// We want to keep the x-target on subsequent up/down
		// movements that cross beyond the end of short lines.
		// Thus a special handling when the cursor is at the
		// end of line: Use the new x-target only if the old
		// one was before the end of line or the old one was
		// after the beginning of the line
		bool inRTL = innerParagraph().isRTL(bv().buffer().params());
		bool left;
		bool right;
		if (inRTL) {
			left = pos() == textRow().endpos();
			right = pos() == textRow().pos();
		} else {
			left = pos() == textRow().pos();
			right = pos() == textRow().endpos();
		}
		if ((!left && !right) ||
				(left && !right && xo < x_target_) ||
				(!left && right && x_target_ < xo))
			setTargetX(xo);
		else
			xo = targetX();
	} else
		xo = targetX();

	// first get the current line
	TextMetrics & tm = bv_->textMetrics(text());
	ParagraphMetrics const & pm = tm.parMetrics(pit());
	int const row = pm.getRowIndex(pos(), boundary());

	if (atFirstOrLastRow(up)) {
		// Is there a place for the cursor to go ? If yes, we
		// can execute the DEPM, otherwise we should keep the
		// paragraph to host the cursor.
		Cursor dummy = *this;
		bool valid_destination = false;
		for(; dummy.depth(); dummy.pop())
			if (!dummy.atFirstOrLastRow(up)) {
				valid_destination = true;
				break;
			}

		// will a next dispatch follow and if there is a new
		// dispatch will it move the cursor out ?
		if (depth() > 1 && valid_destination) {
			// The cursor hasn't changed yet. This happens when
			// you e.g. move out of an inset. And to give the
			// DEPM the possibility of doing something we must
			// provide it with two different cursors. (Lgb, vfr)
			dummy = *this;
			dummy.pos() = dummy.pos() == 0 ? dummy.lastpos() : 0;
			dummy.pit() = dummy.pit() == 0 ? dummy.lastpit() : 0;

			if (bv().checkDepm(dummy, *this)) {
				updateNeeded = true;
				forceBufferUpdate();
			}
			updateTextTargetOffset();
		}
		return updateNeeded;
	}

	// with and without selection are handled differently
	if (!selection()) {
		int yo1 = bv().getPos(*this).y;
		Cursor old = *this;
		// To next/previous row
		// FIXME: the y position is often guessed wrongly across styles and
		// insets, which leads to weird behaviour.
		if (up)
			tm.editXY(*this, xo, yo1 - textRow().ascent() - 1);
		else
			tm.editXY(*this, xo, yo1 + textRow().descent() + 1);
		x_target_ = old.x_target_;
		clearSelection();

		// This happens when you move out of an inset.
		// And to give the DEPM the possibility of doing
		// something we must provide it with two different
		// cursors. (Lgb)
		Cursor dummy = *this;
		if (dummy == old)
			++dummy.pos();
		if (bv().checkDepm(dummy, old)) {
			updateNeeded = true;
			forceBufferUpdate();
			// Make sure that cur gets back whatever happened to dummy (Lgb)
			operator=(dummy);
		}
		if (inTexted() && pos() && paragraph().isEnvSeparator(pos() - 1))
			posBackward();
	} else {
		// if there is a selection, we stay out of any inset,
		// and just jump to the right position:
		Cursor old = *this;
		int next_row = row;
		if (up) {
			if (row > 0) {
				--next_row;
			} else if (pit() > 0) {
				--pit();
				TextMetrics & tm2 = bv_->textMetrics(text());
				if (!tm2.contains(pit()))
					tm2.newParMetricsUp();
				ParagraphMetrics const & pmcur = tm2.parMetrics(pit());
				next_row = pmcur.rows().size() - 1;
			}
		} else {
			if (row + 1 < int(pm.rows().size())) {
				++next_row;
			} else if (pit() + 1 < int(text()->paragraphs().size())) {
				++pit();
				TextMetrics & tm2 = bv_->textMetrics(text());
				if (!tm2.contains(pit()))
					tm2.newParMetricsDown();
				next_row = 0;
			}
		}

		Row const & real_next_row = tm.parMetrics(pit()).rows()[next_row];
		tie(pos(), boundary()) = tm.getPosNearX(real_next_row, xo);
		// When selection==false, this is done by TextMetrics::editXY
		setCurrentFont();

		if (bv().checkDepm(*this, old)) {
			updateNeeded = true;
			forceBufferUpdate();
		}
	}

	updateTextTargetOffset();
	return updateNeeded;
}


void Cursor::handleFont(string const & font)
{
	LYXERR(Debug::DEBUG, font);
	docstring safe;
	if (selection()) {
		macroModeClose();
		safe = cap::grabAndEraseSelection(*this);
	}

	recordUndoInset();

	if (lastpos() != 0) {
		// something left in the cell
		if (pos() == 0) {
			// cursor in first position
			popBackward();
		} else if (pos() == lastpos()) {
			// cursor in last position
			popForward();
		} else {
			// cursor in between. split cell
			MathData::iterator bt = cell().begin();
			MathAtom at = createInsetMath(from_utf8(font), buffer());
			at.nucleus()->cell(0) = MathData(buffer(), bt, bt + pos());
			cell().erase(bt, bt + pos());
			popBackward();
			plainInsert(at);
		}
	} else {
		// nothing left in the cell
		popBackward();
		plainErase();
		resetAnchor();
	}
	insert(safe);
}


void Cursor::undispatched() const
{
	disp_.dispatched(false);
}


void Cursor::dispatched() const
{
	disp_.dispatched(true);
}


void Cursor::screenUpdateFlags(Update::flags f) const
{
	disp_.screenUpdate(f);
}


void Cursor::noScreenUpdate() const
{
	disp_.screenUpdate(Update::None);
}


void Cursor::forceBufferUpdate() const
{
	disp_.forceBufferUpdate();
}


void Cursor::clearBufferUpdate() const
{
	disp_.clearBufferUpdate();
}


bool Cursor::needBufferUpdate() const
{
	return disp_.needBufferUpdate();
}


Font Cursor::getFont() const
{
	// The logic here should more or less match to the
	// Cursor::setCurrentFont logic, i.e. the cursor height should
	// give a hint what will happen if a character is entered.
	// FIXME: this is not the case, what about removing this method ? (see #10478).

	// HACK. far from being perfect...

	CursorSlice const & sl = innerTextSlice();
	Text const & text = *sl.text();
	Paragraph const & par = text.getPar(sl.pit());

	// on boundary, so we are really at the character before
	pos_type pos = sl.pos();
	if (pos > 0 && boundary())
		--pos;

	// on space? Take the font before (only for RTL boundary stay)
	if (pos > 0) {
		TextMetrics const & tm = bv().textMetrics(&text);
		if (pos == sl.lastpos()
			|| (par.isSeparator(pos)
			&& !tm.isRTLBoundary(sl.pit(), pos)))
			--pos;
	}

	// get font at the position
	Font font = par.getFont(buffer()->params(), pos,
		text.outerFont(sl.pit()));

	return font;
}


void Cursor::sanitize()
{
	setBuffer(&bv_->buffer());
	CursorData::sanitize();
}


bool notifyCursorLeavesOrEnters(Cursor const & old, Cursor & cur)
{
	// find inset in common
	size_type i;
	for (i = 0; i < old.depth() && i < cur.depth(); ++i) {
		if (&old[i].inset() != &cur[i].inset())
			break;
	}

	// update words if we just moved to another paragraph
	if (i == old.depth() && i == cur.depth()
	    && !cur.buffer()->isClean()
	    && cur.inTexted() && old.inTexted()
	    && cur.pit() != old.pit()) {
		old.paragraph().updateWords();
	}

	// notify everything on top of the common part in old cursor,
	// but stop if the inset claims the cursor to be invalid now
	for (size_type j = i; j < old.depth(); ++j) {
		Cursor inset_pos = old;
		inset_pos.resize(j + 1);
		if (old[j].inset().notifyCursorLeaves(inset_pos, cur))
			return true;
	}

	// notify everything on top of the common part in new cursor,
	// but stop if the inset claims the cursor to be invalid now
	for (; i < cur.depth(); ++i) {
		if (cur[i].inset().notifyCursorEnters(cur))
			return true;
	}

	return false;
}


void Cursor::setLanguageFromInput()
{
	if (!lyxrc.respect_os_kbd_language
	    || !inTexted()
	    || paragraph().isPassThru())
		return;
	string const & code = theApp()->inputLanguageCode();
	Language const * lang = languages.getFromCode(code, buffer()->getLanguages());
	if (lang) {
		current_font.setLanguage(lang);
		real_current_font.setLanguage(lang);
	} else
		LYXERR0("setLanguageFromCode: unknown language code " << code);
}


void Cursor::setCurrentFont()
{
	CursorSlice const & cs = innerTextSlice();
	Paragraph const & par = cs.paragraph();
	pos_type cpit = cs.pit();
	pos_type cpos = cs.pos();
	Text const & ctext = *cs.text();
	TextMetrics const & tm = bv().textMetrics(&ctext);

	// are we behind previous char in fact? -> go to that char
	if (cpos > 0 && boundary())
		--cpos;

	// find position to take the font from
	if (cpos != 0) {
		// paragraph end? -> font of last char
		if (cpos == lastpos())
			--cpos;
		// on space? -> look at the words in front of space
		else if (cpos > 0 && par.isSeparator(cpos))	{
			// abc| def -> font of c
			// abc |[WERBEH], i.e. boundary==true -> font of c
			// abc [WERBEH]| def, font of the space
			if (!tm.isRTLBoundary(cpit, cpos))
				--cpos;
		}
	}

	// get font
	BufferParams const & bufparams = buffer()->params();
	current_font = par.getFontSettings(bufparams, cpos);
	real_current_font = tm.displayFont(cpit, cpos);

	// set language to input language
	setLanguageFromInput();

	// special case for paragraph end
	if (cs.pos() == lastpos()
	    && tm.isRTLBoundary(cpit, cs.pos())
	    && !boundary()) {
		Language const * lang = par.getParLanguage(bufparams);
		current_font.setLanguage(lang);
		current_font.fontInfo().setNumber(FONT_OFF);
		real_current_font.setLanguage(lang);
		real_current_font.fontInfo().setNumber(FONT_OFF);
	}

	// No language in pass thru situations
	if (cs.paragraph().isPassThru()) {
		current_font.setLanguage(latex_language);
		real_current_font.setLanguage(latex_language);
	}
}


void Cursor::checkBufferStructure()
{
	if (buffer()->isInternal())
		return;

	Buffer const * master = buffer()->masterBuffer();
	master->tocBackend().updateItem(*this);
	if (master != buffer() && !master->hasGuiDelegate())
		// In case the master has no gui associated with it,
		// the TocItem is not updated (part of bug 5699).
		buffer()->tocBackend().updateItem(*this);
}


void Cursor::moveToClosestEdge(int const x, bool const edit)
{
	if (Inset const * inset = nextInset()) {
		// This is only used in mathed.
		LATTEST(inset->asInsetMath());
		// stay in front of insets for which we want to open the dialog
		// (e.g. InsetMathSpace).
		if (edit && (inset->hasSettings() || !inset->contextMenuName().empty()))
			return;
		CoordCache::Insets const & insetCache = bv().coordCache().insets();
		if (!insetCache.has(inset))
			return;
		int const wid = insetCache.dim(inset).wid;
		Point p = insetCache.xy(inset);
		if (x > p.x + (wid + 1) / 2)
			posForward();
	}
}


} // namespace lyx
