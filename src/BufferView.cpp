/**
 * \file BufferView.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alfredo Braunstein
 * \author Lars Gullik Bjønnes
 * \author John Levon
 * \author André Pönitz
 * \author Jürgen Vigna
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "BufferView.h"

#include "BranchList.h"
#include "Buffer.h"
#include "BufferList.h"
#include "BufferParams.h"
#include "BiblioInfo.h"
#include "CoordCache.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "DispatchResult.h"
#include "ErrorList.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "Intl.h"
#include "Language.h"
#include "LayoutFile.h"
#include "LyX.h"
#include "LyXAction.h"
#include "lyxfind.h"
#include "LyXRC.h"
#include "MetricsInfo.h"
#include "Paragraph.h"
#include "Session.h"
#include "Statistics.h"
#include "texstream.h"
#include "Text.h"
#include "TextMetrics.h"
#include "TexRow.h"
#include "TocBackend.h"

#include "insets/InsetBibtex.h"
#include "insets/InsetCitation.h"
#include "insets/InsetCommand.h" // ChangeRefs
#include "insets/InsetGraphics.h"
#include "insets/InsetIndex.h"
#include "insets/InsetRef.h"
#include "insets/InsetText.h"

#include "mathed/InsetMathNest.h"
#include "mathed/InsetMathRef.h"
#include "mathed/MathData.h"
#include "mathed/MathRow.h"

#include "frontends/alert.h"
#include "frontends/Application.h"
#include "frontends/CaretGeometry.h"
#include "frontends/Delegates.h"
#include "frontends/FontMetrics.h"
#include "frontends/InputMethod.h"
#include "frontends/NullPainter.h"
#include "frontends/Painter.h"
#include "frontends/Selection.h"
#include "frontends/Clipboard.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/docstring.h"
#include "support/docstring_list.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/Length.h"
#include "support/Lexer.h"
#include "support/lstrings.h"
#include "support/lyxlib.h"
#include "support/types.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <vector>

using namespace std;
using namespace lyx::support;

namespace lyx {

namespace Alert = frontend::Alert;

namespace {

/// Return an inset of this class if it exists at the current cursor position
template <class T>
T * getInsetByCode(Cursor const & cur, InsetCode code)
{
	DocIterator it = cur;
	Inset * inset = it.nextInset();
	if (inset && inset->lyxCode() == code)
		return static_cast<T*>(inset);
	return nullptr;
}


/// Note that comparing contents can only be used for InsetCommand
bool findNextInset(DocIterator & dit, vector<InsetCode> const & codes,
	docstring const & contents)
{
	DocIterator tmpdit = dit;

	while (tmpdit) {
		Inset const * inset = tmpdit.nextInset();
		if (inset) {
			bool const valid_code = std::find(codes.begin(), codes.end(),
				inset->lyxCode()) != codes.end();
			InsetCommand const * ic = inset->asInsetCommand();
			bool const same_or_no_contents =  contents.empty()
				|| (ic && (ic->getFirstNonOptParam() == contents));

			if (valid_code && same_or_no_contents) {
				dit = tmpdit;
				return true;
			}
		}
		tmpdit.forwardInset();
	}

	return false;
}


/// Looks for next inset with one of the given codes.
/// Note that same_content can only be used for InsetCommand
bool findInset(DocIterator & dit, vector<InsetCode> const & codes,
	bool same_content)
{
	docstring contents;
	DocIterator tmpdit = dit;
	if (same_content && tmpdit.nextInset()) {
		// if we are in front of an inset, its content matters
		InsetCommand const * ic = tmpdit.nextInset()->asInsetCommand();
		if (ic) {
			bool const valid_code = std::find(codes.begin(), codes.end(),
				ic->lyxCode()) != codes.end();
			if (valid_code)
				contents = ic->getFirstNonOptParam();
		}
	}
	tmpdit.forwardInset();
	if (!tmpdit)
		return false;

	Inset const * inset = tmpdit.nextInset();
	if (same_content && contents.empty() && inset) {
		InsetCommand const * ic = inset->asInsetCommand();
		if (ic) {
			bool const valid_code = std::find(codes.begin(), codes.end(),
				ic->lyxCode()) != codes.end();
			if (valid_code)
				contents = ic->getFirstNonOptParam();
		}
	}

	if (!findNextInset(tmpdit, codes, contents)) {
		if (dit.depth() != 1 || dit.pit() != 0 || dit.pos() != 0) {
			inset = &tmpdit.bottom().inset();
			tmpdit = doc_iterator_begin(&inset->buffer(), inset);
			if (!findNextInset(tmpdit, codes, contents))
				return false;
		} else {
			return false;
		}
	}

	dit = tmpdit;
	return true;
}


/// Moves cursor to the next inset with one of the given codes.
bool gotoInset(BufferView * bv, vector<InsetCode> const & codes,
	       bool same_content)
{
	Cursor tmpcur = bv->cursor();
	if (!findInset(tmpcur, codes, same_content)) {
		bv->cursor().message(_("No more insets"));
		return false;
	}

	tmpcur.clearSelection();
	bv->setCursor(tmpcur);
	return bv->scrollToCursor(bv->cursor(), SCROLL_TOP);
}


/// A map from a Text to the associated text metrics
typedef map<Text const *, TextMetrics> TextMetricsCache;

enum ScreenUpdateStrategy {
	NoScreenUpdate,
	SingleParUpdate,
	FullScreenUpdate,
	DecorationUpdate
};

} // namespace


/////////////////////////////////////////////////////////////////////
//
// BufferView
//
/////////////////////////////////////////////////////////////////////

struct BufferView::Private
{
	Private(BufferView & bv) :
		update_strategy_(FullScreenUpdate),
		update_flags_(Update::Force),
		cursor_(bv), anchor_pit_(0), anchor_ypos_(10000),
		wh_(0), inlineCompletionUniqueChars_(0),
		last_inset_(nullptr), mouse_position_cache_(),
		gui_(nullptr), bookmark_edit_position_(-1),
		horiz_scroll_offset_(0), clickable_inset_(false)
	{
		xsel_cache_.set = false;
	}

	///
	ScrollbarParameters scrollbarParameters_;
	///
	ScreenUpdateStrategy update_strategy_;
	///
	Update::flags update_flags_;
	///
	CoordCache coord_cache_;
	///
	typedef unordered_map<MathData const *, MathRow> MathRows;
	MathRows math_rows_;

	/// this is used to handle XSelection events in the right manner.
	struct {
		CursorSlice cursor;
		CursorSlice anchor;
		bool set;
	} xsel_cache_;
	///
	Cursor cursor_;
	///
	pit_type anchor_pit_;
	///
	int anchor_ypos_;
	/// Estimated average par height for scrollbar.
	int wh_;
	///
	vector<int> par_height_;

	///
	DocIterator inlineCompletionPos_;
	///
	docstring inlineCompletion_;
	///
	size_t inlineCompletionUniqueChars_;

	/// an instance of the input method
	frontend::InputMethod * im_ = nullptr;

	/// keyboard mapping object.
	Intl intl_;

	/// last visited inset.
	/** kept to send setMouseHover(false).
	  * Not owned, so don't delete.
	  */
	Inset const * last_inset_;

	/// position of the mouse at the time of the last mouse move
	/// This is used to update the hovering status of inset in
	/// cases where the buffer is scrolled, but the mouse didn't move.
	Point mouse_position_cache_;

	mutable TextMetricsCache text_metrics_;

	/// Whom to notify.
	/** Not owned, so don't delete.
	  */
	frontend::GuiBufferViewDelegate * gui_;

	///
	map<string, Inset *> edited_insets_;

	// cache for id of the paragraph which was edited the last time
	int bookmark_edit_position_;

	/// When the row where the cursor lies is scrolled, this
	/// contains the scroll offset
	int horiz_scroll_offset_;
	/// a slice pointing to the start of the row where the cursor
	/// is (at last draw time)
	CursorSlice current_row_slice_;
	/// are we hovering something that we can click
	bool clickable_inset_;
	/// shape of the caret
	frontend::CaretGeometry caret_geometry_;
	///
	bool mouse_selecting_ = false;
	/// Reference value for statistics (essentially subtract this from the actual value to see relative counts)
	/// (words/chars/chars no blanks)
	int stats_ref_value_w_ = 0;
	int stats_ref_value_c_ = 0;
	int stats_ref_value_nb_ = 0;
	bool stats_update_trigger_ = false;

};


BufferView::BufferView(Buffer & buf)
	: width_(0), height_(0), full_screen_(false), buffer_(buf),
      d(new Private(*this))
{
	d->xsel_cache_.set = false;
	d->intl_.initKeyMapper(lyxrc.use_kbmap);

	d->cursor_.setBuffer(&buf);
	d->cursor_.push(buffer_.inset());
	d->cursor_.resetAnchor();
	d->cursor_.setCurrentFont();

	buffer_.updatePreviews();
}


BufferView::~BufferView()
{
	// current buffer is going to be switched-off, save cursor pos
	// Ideally, the whole cursor stack should be saved, but session
	// currently can only handle bottom (whole document) level pit and pos.
	// That is to say, if a cursor is in a nested inset, it will be
	// restore to the left of the top level inset.
	LastFilePosSection::FilePos fp;
	fp.file = buffer_.fileName();
	fp.pit = d->cursor_.bottom().pit();
	fp.pos = d->cursor_.bottom().pos();
	theSession().lastFilePos().save(fp);

	if (d->last_inset_)
		d->last_inset_->setMouseHover(this, false);

	delete d;
}


void BufferView::copySettingsFrom(BufferView const & bv)
{
	setCursor(bv.cursor());
	d->anchor_pit_ = bv.d->anchor_pit_;
	d->anchor_ypos_ = bv.d->anchor_ypos_;
}


int BufferView::defaultMargin() const
{
	// The value used to be hardcoded to 10
	return zoomedPixels(20);
}


int BufferView::rightMargin() const
{
	const int screen_width = inPixels(lyxrc.screen_width);

	// The additional test for the case the outliner is opened.
	if (!lyxrc.screen_limit || width_ < screen_width + 2 * defaultMargin()) {
		return defaultMargin();
	} else {
		return (width_ - screen_width) / 2;
	}
}


int BufferView::leftMargin() const
{
	return rightMargin();
}


int BufferView::topMargin() const
{
	// Original value was 20px at 100dpi. For internal buffers like in
	// advanced search and replace, a value of 5px is enough.
	return zoomedPixels(buffer().isInternal() ? 5 : 20);
}


int BufferView::bottomMargin() const
{
	return topMargin();
}


int BufferView::inPixels(Length const & len) const
{
	Font const font = buffer().params().getFont();
	return len.inPixels(workWidth(), theFontMetrics(font).em());
}


int BufferView::zoomedPixels(int pix) const
{
	// FIXME: the dpi setting should really depend on the BufferView
	// (think different monitors).

	// Zoom factor specified by user in percent
	double const zoom = lyxrc.currentZoom / 100.0; // [percent]

	// DPI setting for monitor relative to 100dpi
	double const dpizoom = lyxrc.dpi / 100.0; // [per 100dpi]

	return support::iround(pix * zoom * dpizoom);
}


bool BufferView::isTopScreen() const
{
	return 0 == d->scrollbarParameters_.min;
}


bool BufferView::isBottomScreen() const
{
	return 0 == d->scrollbarParameters_.max;
}


Intl & BufferView::getIntl()
{
	return d->intl_;
}


Intl const & BufferView::getIntl() const
{
	return d->intl_;
}


CoordCache & BufferView::coordCache()
{
	return d->coord_cache_;
}


CoordCache const & BufferView::coordCache() const
{
	return d->coord_cache_;
}


bool BufferView::hasMathRow(MathData const * cell) const
{
	return d->math_rows_.find(cell) != d->math_rows_.end();
}


MathRow const & BufferView::mathRow(MathData const * cell) const
{
	auto it = d->math_rows_.find(cell);
	LATTEST(it != d->math_rows_.end());
	return it->second;
}


void BufferView::setMathRow(MathData const * cell, MathRow const & mrow)
{
	d->math_rows_[cell] = mrow;
}


Buffer & BufferView::buffer()
{
	return buffer_;
}


Buffer const & BufferView::buffer() const
{
	return buffer_;
}


docstring const & BufferView::searchRequestCache() const
{
	return theClipboard().getFindBuffer();
}


void BufferView::setSearchRequestCache(docstring const & text)
{
	bool casesensitive;
	bool matchword;
	bool forward;
	bool wrap;
	bool instant;
	bool onlysel;
	docstring const search = string2find(text, casesensitive, matchword,
					     forward, wrap, instant, onlysel);
	theClipboard().setFindBuffer(search);
}


bool BufferView::needsFitCursor() const
{
	if (cursorStatus(d->cursor_) == CUR_INSIDE) {
		frontend::FontMetrics const & fm =
			theFontMetrics(d->cursor_.getFont().fontInfo());
		int const asc = fm.maxAscent();
		int const des = fm.maxDescent();
		Point const p = getPos(d->cursor_);
		if (p.y - asc >= 0 && p.y + des < height_)
			return false;
	}
	return true;
}


namespace {

// this is for debugging only.
string flagsAsString(Update::flags flags)
{
	if (flags == Update::None)
		return "None ";
	return string((flags & Update::FitCursor) ? "FitCursor " : "")
		+ ((flags & Update::Force) ? "Force " : "")
		+ ((flags & Update::ForceDraw) ? "ForceDraw " : "")
		+ ((flags & Update::SinglePar) ? "SinglePar " : "")
		+ ((flags & Update::Decoration) ? "Decoration " : "");
}

}

void BufferView::processUpdateFlags(Update::flags flags)
{
	LYXERR(Debug::PAINTING, "BufferView::processUpdateFlags( "
		   << flagsAsString(flags) << ")  buffer: " << &buffer_);

	// Case when no explicit update is requested.
	if (flags == Update::None || !ready())
		return;

	/* FIXME We would like to avoid doing this here, since it is very
	 * expensive and is called in updateBuffer already. However, even
	 * inserting a plain character can invalidate the overly fragile
	 * tables of child documents built by updateMacros. Some work is
	 * needed to avoid doing that when not necessary.
	 */
	buffer_.updateMacros();

	// First check whether the metrics and inset positions should be updated
	if (flags & Update::Force) {
		// This will compute all metrics and positions.
		updateMetrics(true);
		// metrics is done, full drawing is necessary now
		flags = (flags & ~Update::Force) | Update::ForceDraw;
	}
	/* If a single paragraph update has been requested and we are not
	 * already repainting all, check whether this update changes the
	 * paragraph metrics. If it does, then compute all metrics (in
	 * case the paragraph is in an inset)
	 *
	 * We handle this before FitCursor because the later will require
	 * correct metrics at cursor position.
	 */
	else if ((flags & Update::SinglePar) && !(flags & Update::ForceDraw)) {
		if (singleParUpdate())
			updateMetrics(false);
		else
			updateMetrics(true);
	}
	else if (flags & Update::ForceDraw)
		// This will compute only the needed metrics and update positions.
		updateMetrics(false);

	// Then make sure that the screen contains the cursor if needed
	if (flags & Update::FitCursor) {
		if (needsFitCursor()) {
			// First try to make the selection start visible
			// (which is just the cursor when there is no selection)
			scrollToCursor(d->cursor_.selectionBegin(), SCROLL_VISIBLE);
			// Metrics have to be updated
			updateMetrics(false);
			// Is the cursor visible? (only useful if cursor is at end of selection)
			if (needsFitCursor()) {
				// then try to make cursor visible instead
				scrollToCursor(d->cursor_, SCROLL_VISIBLE);
				// Metrics have to be recomputed (maybe again)
				updateMetrics(false);
			}
		}
		flags = (flags & ~Update::FitCursor) | Update::ForceDraw;
	}

	if (theApp()->drawStrategy() == DrawStrategy::Full)
		flags = flags | Update::ForceDraw;

	// Add flags to the the update flags. These will be reset to None
	// after the redraw is actually done
	d->update_flags_ = d->update_flags_ | flags;
	LYXERR(Debug::PAINTING, "Cumulative flags: " << flagsAsString(flags));

	// Now compute the update strategy
	// Possibly values in flag are None, SinglePar, Decoration, ForceDraw
	LATTEST((d->update_flags_ & ~(Update::None | Update::SinglePar
	                              | Update::Decoration | Update::ForceDraw)) == 0);

	if (d->update_flags_ & Update::ForceDraw)
		d->update_strategy_ = FullScreenUpdate;
	else if (d->update_flags_ & Update::Decoration)
		d->update_strategy_ = DecorationUpdate;
	else if (d->update_flags_ & Update::SinglePar)
		d->update_strategy_ = SingleParUpdate;
	else {
		// no need to redraw anything.
		d->update_strategy_ = NoScreenUpdate;
	}

	updateHoveredInset();

	// Trigger a redraw.
	buffer_.changed(false);
}


void BufferView::updateScrollbarParameters()
{
	if (!ready())
		return;

	// We prefer fixed size line scrolling.
	d->scrollbarParameters_.single_step = defaultRowHeight();
	// We prefer full screen page scrolling.
	d->scrollbarParameters_.page_step = height_;

	Text & t = buffer_.text();
	TextMetrics & tm = d->text_metrics_[&t];

	LYXERR(Debug::SCROLLING, " Updating scrollbar: height: "
		<< t.paragraphs().size()
		<< " curr par: " << d->cursor_.bottom().pit()
		<< " default height " << defaultRowHeight());

	size_t const parsize = t.paragraphs().size();
	if (d->par_height_.size() != parsize) {
		d->par_height_.clear();
		// FIXME: We assume a default paragraph height of 2 rows. This
		// should probably be pondered with the screen width.
		d->par_height_.resize(parsize, defaultRowHeight() * 2);
	}

	// Look at paragraph heights on-screen
	pair<pit_type, ParagraphMetrics const *> first = tm.first();
	pair<pit_type, ParagraphMetrics const *> last = tm.last();
	for (pit_type pit = first.first; pit <= last.first; ++pit) {
		d->par_height_[pit] = tm.parMetrics(pit).height();
		LYXERR(Debug::SCROLLING, "storing height for pit " << pit << " : "
			<< d->par_height_[pit]);
	}

	int top_pos = first.second->top();
	int bottom_pos = last.second->bottom();
	bool first_visible = first.first == 0 && top_pos >= 0;
	bool last_visible = last.first + 1 == int(parsize) && bottom_pos <= height_;
	if (first_visible && last_visible) {
		d->scrollbarParameters_.min = 0;
		d->scrollbarParameters_.max = 0;
		return;
	}

	d->scrollbarParameters_.min = top_pos;
	for (size_t i = 0; i != size_t(first.first); ++i)
		d->scrollbarParameters_.min -= d->par_height_[i];
	d->scrollbarParameters_.max = bottom_pos;
	for (size_t i = last.first + 1; i != parsize; ++i)
		d->scrollbarParameters_.max += d->par_height_[i];

	// The reference is the top position so we remove one page.
	if (lyxrc.scroll_below_document)
		d->scrollbarParameters_.max -= minVisiblePart();
	else
		d->scrollbarParameters_.max -= d->scrollbarParameters_.page_step;

	// 0 must be inside the range as it denotes the current position
	if (d->scrollbarParameters_.max < 0)
		d->scrollbarParameters_.max = 0;
	if (d->scrollbarParameters_.min > 0)
		d->scrollbarParameters_.min = 0;
}


ScrollbarParameters const & BufferView::scrollbarParameters() const
{
	return d->scrollbarParameters_;
}


docstring BufferView::toolTip(int x, int y) const
{
	// Get inset under mouse, if there is one.
	Inset const * covering_inset = getCoveringInset(buffer_.text(), x, y);
	if (!covering_inset)
		// No inset, no tooltip...
		return docstring();
	return covering_inset->toolTip(*this, x, y);
}


string BufferView::contextMenu(int x, int y) const
{
	//If there is a selection, return the containing inset menu
	if (d->cursor_.selection())
		return d->cursor_.inset().contextMenu(*this, x, y);

	// Get inset under mouse, if there is one.
	Inset const * covering_inset = getCoveringInset(buffer_.text(), x, y);
	if (covering_inset) {
		if (covering_inset->asInsetMath()) {
			CoordCache::Insets const & inset_cache =
				coordCache().insets();
			Inset const * inner_inset = mathContextMenu(
				covering_inset->asInsetMath()->asNestInset(),
				inset_cache, x, y);
			if (inner_inset)
				return inner_inset->contextMenu(*this, x, y);
		}
		return covering_inset->contextMenu(*this, x, y);
	}

	return buffer_.inset().contextMenu(*this, x, y);
}


Inset const * BufferView::mathContextMenu(InsetMathNest const * inset,
		CoordCache::Insets const & inset_cache, int x, int y) const
{
	for (size_t i = 0; i < inset->nargs(); ++i) {
		MathData const & md = inset->cell(i);
		for (size_t j = 0; j < md.size(); ++j) {
			string const name = lyxerr.debugging(Debug::MATHED)
				? insetName(md[j].nucleus()->lyxCode())
				: string();
			LYXERR(Debug::MATHED, "Examining inset: " << name);
			if (!md[j].nucleus()->contextMenuName().empty()) {
				if (inset_cache.covers(md[j].nucleus(), x, y)) {
					LYXERR(Debug::MATHED, "Hit inset: "
					       << name);
					return md[j].nucleus();
				}
			}
			InsetMathNest const * imn =
				md[j].nucleus()->asNestInset();
			if (imn) {
				Inset const * inner =
					mathContextMenu(imn, inset_cache, x, y);
				if (inner)
					return inner;
			}
		}
	}
	return nullptr;
}


void BufferView::scrollDocView(int const pixels)
{
	// The scrollbar values are relative to the top of the screen, therefore the
	// offset is equal to the target value.

	// No scrolling at all? No need to redraw anything
	if (pixels == 0)
		return;

	// If part of the existing paragraphs will remain visible, prefer to
	// scroll
	TextMetrics const & tm = textMetrics(&buffer_.text());
	if (tm.first().second->top() - pixels <= height_
	     &&  tm.last().second->bottom() - pixels >= 0) {
		LYXERR(Debug::SCROLLING, "small skip");
		d->anchor_ypos_ -= pixels;
		processUpdateFlags(Update::ForceDraw);
		return;
	}

	// cut off at the top
	if (pixels <= d->scrollbarParameters_.min) {
		DocIterator dit = doc_iterator_begin(&buffer_);
		showCursor(dit, SCROLL_VISIBLE);
		LYXERR(Debug::SCROLLING, "scroll to top");
		return;
	}

	// cut off at the bottom
	if (pixels >= d->scrollbarParameters_.max) {
		DocIterator dit = doc_iterator_end(&buffer_);
		dit.backwardPos();
		showCursor(dit, SCROLL_VISIBLE);
		LYXERR(Debug::SCROLLING, "scroll to bottom");
		return;
	}

	LYXERR(Debug::SCROLLING, "search paragraph");
	// find paragraph at target position
	int par_pos = d->scrollbarParameters_.min;
	pit_type i = 0;
	for (; i != int(d->par_height_.size()); ++i) {
		par_pos += d->par_height_[i];
		if (par_pos >= pixels)
			break;
	}

	if (par_pos < pixels) {
		// It seems we didn't find the correct pit so stay on the safe side and
		// scroll to bottom.
		LYXERR0("scrolling position not found!");
		scrollDocView(d->scrollbarParameters_.max);
		return;
	}

	DocIterator dit = doc_iterator_begin(&buffer_);
	dit.pit() = i;
	LYXERR(Debug::SCROLLING, "pixels = " << pixels << " -> scroll to pit " << i);
	showCursor(dit, SCROLL_VISIBLE);
}


// FIXME: this method is not working well.
void BufferView::setCursorFromScrollbar()
{
	TextMetrics & tm = d->text_metrics_[&buffer_.text()];

	int const height = 2 * defaultRowHeight();
	int const first = height;
	int const last = height_ - height;
	int newy = 0;
	Cursor const & oldcur = d->cursor_;

	switch (cursorStatus(oldcur)) {
	case CUR_ABOVE:
		newy = first;
		break;
	case CUR_BELOW:
		newy = last;
		break;
	case CUR_INSIDE:
		int const y = getPos(oldcur).y;
		newy = min(last, max(y, first));
		if (y == newy)
			return;
	}
	// We reset the cursor because cursorStatus() does not
	// work when the cursor is within mathed.
	Cursor cur(*this);
	cur.reset();
	tm.setCursorFromCoordinates(cur, 0, newy);

	// update the bufferview cursor and notify insets
	// FIXME: Care about the d->cursor_ flags to redraw if needed
	Cursor old = d->cursor_;
	mouseSetCursor(cur);
	// the DEPM call in mouseSetCursor() might have destroyed the
	// paragraph the cursor is in.
	bool badcursor = old.fixIfBroken();
	badcursor |= notifyCursorLeavesOrEnters(old, d->cursor_);
	if (badcursor)
		d->cursor_.fixIfBroken();
}


Change const BufferView::getCurrentChange() const
{
	if (!d->cursor_.selection())
		return Change(Change::UNCHANGED);

	DocIterator dit = d->cursor_.selectionBegin();
	// The selected content might have been changed (see #7685)
	dit = dit.getInnerText();
	return dit.paragraph().lookupChange(dit.pos());
}


// this could be used elsewhere as well?
// FIXME: This does not work within mathed!
CursorStatus BufferView::cursorStatus(DocIterator const & dit) const
{
	Point const p = getPos(dit);
	if (p.y < 0)
		return CUR_ABOVE;
	if (p.y > workHeight())
		return CUR_BELOW;
	return CUR_INSIDE;
}


void BufferView::bookmarkEditPosition()
{
	// Don't eat cpu time for each keystroke
	if (d->cursor_.paragraph().id() == d->bookmark_edit_position_)
		return;
	saveBookmark(0);
	d->bookmark_edit_position_ = d->cursor_.paragraph().id();
}


void BufferView::saveBookmark(unsigned int idx)
{
	if (buffer().isInternal())
		return;

	// tentatively save bookmark, id and pos will be used to
	// acturately locate a bookmark in a 'live' lyx session.
	// pit and pos will be updated with bottom level pit/pos
	// when lyx exits.
	theSession().bookmarks().save(
		buffer_.fileName(),
		d->cursor_.bottom().pit(),
		d->cursor_.bottom().pos(),
		d->cursor_.paragraph().id(),
		d->cursor_.pos(),
		idx
	);
	if (idx)
		// emit message signal.
		message(_("Save bookmark"));
}


bool BufferView::moveToPosition(pit_type bottom_pit, pos_type bottom_pos,
	int top_id, pos_type top_pos)
{
	bool success = false;
	DocIterator dit;

	d->cursor_.clearSelection();

	// if a valid par_id is given, try it first
	// This is the case for a 'live' bookmark when unique paragraph ID
	// is used to track bookmarks.
	if (top_id > 0) {
		dit = buffer_.getParFromID(top_id);
		if (!dit.atEnd()) {
			dit.pos() = min(dit.paragraph().size(), top_pos);
			// Some slices of the iterator may not be
			// reachable (e.g. closed collapsible inset)
			// so the dociterator may need to be
			// shortened. Otherwise, setCursor may crash
			// lyx when the cursor can not be set to these
			// insets.
			size_t const n = dit.depth();
			for (size_t i = 0; i < n; ++i)
				if (!dit[i].inset().editable()) {
					dit.resize(i);
					break;
				}
			success = true;
		}
	}

	// if top_id == 0, or searching through top_id failed
	// This is the case for a 'restored' bookmark when only bottom
	// (document level) pit was saved. Because of this, bookmark
	// restoration is inaccurate. If a bookmark was within an inset,
	// it will be restored to the left of the outmost inset that contains
	// the bookmark.
	if (!success && bottom_pit < int(buffer_.paragraphs().size())) {
		dit = doc_iterator_begin(&buffer_);

		dit.pit() = bottom_pit;
		dit.pos() = min(bottom_pos, dit.paragraph().size());
		success = true;
	}

	if (success) {
		// Note: only bottom (document) level pit is set.
		setCursor(dit);
		// set the current font.
		d->cursor_.setCurrentFont();
		// Do not forget to reset the anchor (see #9912)
		d->cursor_.resetAnchor();
		processUpdateFlags(Update::Force | Update::FitCursor);
	}

	return success;
}


void BufferView::translateAndInsert(char_type c, Text * t, Cursor & cur)
{
	if (d->cursor_.real_current_font.isRightToLeft()) {
		if (d->intl_.keymap == Intl::PRIMARY)
			d->intl_.keyMapSec();
	} else {
		if (d->intl_.keymap == Intl::SECONDARY)
			d->intl_.keyMapPrim();
	}

	d->intl_.getTransManager().translateAndInsert(c, t, cur);
}


int BufferView::workWidth() const
{
	return width_;
}


void BufferView::recenter()
{
	showCursor(d->cursor_, SCROLL_CENTER);
}


void BufferView::showCursor(ScrollType how)
{
	showCursor(d->cursor_, how);
}


void BufferView::showCursor(DocIterator const & dit, ScrollType how)
{
	if (scrollToCursor(dit, how))
		processUpdateFlags(Update::ForceDraw);
}


bool BufferView::scrollToCursor(DocIterator const & dit, ScrollType how)
{
	// We are not properly started yet, delay until resizing is done.
	if (height_ == 0)
		return false;

	if (how == SCROLL_CENTER)
		LYXERR(Debug::SCROLLING, "Centering cursor in workarea");
	else if (how == SCROLL_TOP)
		LYXERR(Debug::SCROLLING, "Setting cursor to top of workarea");
	else if (how == SCROLL_BOTTOM)
		LYXERR(Debug::SCROLLING, "Setting cursor to bottom of workarea");
	else if (how == SCROLL_TOGGLE)
		LYXERR(Debug::SCROLLING, "Alternate cursor position between center, top and bottom");
	else
		LYXERR(Debug::SCROLLING, "Making sure cursor is visible in workarea");

	CursorSlice const & bot = dit.bottom();
	TextMetrics & tm = textMetrics(bot.text());

	pos_type const max_pit = pos_type(bot.text()->paragraphs().size() - 1);
	pos_type bot_pit = bot.pit();
	if (bot_pit > max_pit) {
		// FIXME: Why does this happen?
		LYXERR0("bottom pit is greater that max pit: "
			<< bot_pit << " > " << max_pit);
		bot_pit = max_pit;
	}

	// Add surrounding paragraph metrics to scroll properly with cursor up/down
	if (bot_pit == tm.first().first - 1)
		tm.newParMetricsUp();
	else if (bot_pit == tm.last().first + 1)
		tm.newParMetricsDown();

	if (tm.contains(bot_pit) && how == SCROLL_VISIBLE) {
		ParagraphMetrics const & pm = tm.parMetrics(bot_pit);
		LBUFERR(!pm.rows().empty());
		// FIXME: smooth scrolling doesn't work in mathed.
		CursorSlice const & cs = dit.innerTextSlice();
		int const ypos = pm.position() + coordOffset(dit).y;
		ParagraphMetrics const & inner_pm =
			textMetrics(cs.text()).parMetrics(cs.pit());
		Dimension const & row_dim =
			inner_pm.getRow(cs.pos(), dit.boundary()).dim();
		// Assume first that we do not have to scroll anything
		int ynew = ypos;

		// We try to visualize the whole row, if the row height is larger than
		// the screen height, we scroll to a heuristic value of height_ / 4.
		// FIXME: This heuristic value should be replaced by a recursive search
		// for a row in the inset that can be visualized completely.
		if (row_dim.height() > height_) {
			if (ypos < defaultRowHeight())
				ynew = height_ / 4;
			else if (ypos > height_ - defaultRowHeight())
				ynew = 3 * height_ / 4;
		}
		// If the top part of the row falls of the screen, we scroll
		// up to align the top of the row with the top of the screen.
		else if (ypos - row_dim.ascent() < 0 && ypos < height_)
			ynew = row_dim.ascent();
		// If the bottom of the row falls of the screen, we scroll down.
		else if (ypos + row_dim.descent() > height_ && ypos > 0)
			ynew = height_ - row_dim.descent();

		d->anchor_ypos_ += ynew - ypos;
		return ynew != ypos;
	}

	// fix inline completion position
	if (d->inlineCompletionPos_.fixIfBroken())
		d->inlineCompletionPos_ = DocIterator();

	pit_type const old_pit = d->anchor_pit_;
	int const old_ypos = d->anchor_ypos_;

	if (!tm.contains(bot_pit))
		tm.redoParagraph(bot_pit);
	int const offset = coordOffset(dit).y;

	CursorSlice const & cs = dit.innerTextSlice();
	ParagraphMetrics const & inner_pm =
		textMetrics(cs.text()).parMetrics(cs.pit());
	// dimension of the contents of the text row that holds dit
	Dimension const & row_dim = inner_pm.getRow(cs.pos(), dit.boundary()).contents_dim();

	// Compute typical ypos values.
	int const ypos_center = height_/2 - row_dim.height() / 2 + row_dim.ascent() - offset;
	int const ypos_top = (offset > height_) ? height_ - offset - defaultRowHeight()
	                                        : defaultRowHeight() * 2;
	int const ypos_bottom = height_ - offset - defaultRowHeight();

	// Select the right one.
	d->anchor_pit_ = bot_pit;
	switch(how) {
	case SCROLL_CENTER:
		d->anchor_ypos_ = ypos_center;
		break;
	case SCROLL_TOP:
	case SCROLL_VISIBLE:
		d->anchor_ypos_ = ypos_top;
		break;
	case SCROLL_BOTTOM:
		d->anchor_ypos_ = ypos_bottom;
		break;
	case SCROLL_TOGGLE: {
		ParagraphMetrics const & bot_pm = tm.parMetrics(bot_pit);
		if (!bot_pm.hasPosition()) {
			d->anchor_ypos_ = ypos_center;
			break;
		}
		int const ypos = bot_pm.position();
		if (ypos == ypos_center)
			d->anchor_ypos_ = ypos_top;
		else if (ypos == ypos_top)
			d->anchor_ypos_ = ypos_bottom;
		else
			d->anchor_ypos_ = ypos_center;
	}
	}

	return d->anchor_ypos_ != old_ypos || d->anchor_pit_ != old_pit;
}


void BufferView::makeDocumentClass()
{
	DocumentClassConstPtr olddc = buffer_.params().documentClassPtr();
	buffer_.params().makeDocumentClass(buffer_.isClone(), buffer_.isInternal());
	updateDocumentClass(olddc);
}


void BufferView::updateDocumentClass(DocumentClassConstPtr const & olddc)
{
	StableDocIterator backcur(d->cursor_);
	ErrorList & el = buffer_.errorList("Class Switch");
	cap::switchBetweenClasses(
			olddc, buffer_.params().documentClassPtr(),
			static_cast<InsetText &>(buffer_.inset()), el);

	setCursor(backcur.asDocIterator(&buffer_));

	buffer_.errors("Class Switch");
}


/** Return the change status at cursor position, taking into account the
 * status at each level of the document iterator (a table in a deleted
 * footnote is deleted).
 * When \param outer is true, the top slice is not looked at.
 */
static Change::Type lookupChangeType(DocIterator const & dit, bool outer = false)
{
	size_t const depth = dit.depth() - (outer ? 1 : 0);

	for (size_t i = 0 ; i < depth ; ++i) {
		CursorSlice const & slice = dit[i];
		if (!slice.inset().inMathed()
		    && slice.pos() < slice.paragraph().size()) {
			Change::Type const ch = slice.paragraph().lookupChange(slice.pos()).type;
			if (ch != Change::UNCHANGED)
				return ch;
		}
	}
	return Change::UNCHANGED;
}


bool BufferView::getStatus(FuncRequest const & cmd, FuncStatus & flag)
{
	FuncCode const act = cmd.action();

	// Can we use a readonly buffer?
	if (buffer_.isReadonly()
	    && !lyxaction.funcHasFlag(act, LyXAction::ReadOnly)
	    && !lyxaction.funcHasFlag(act, LyXAction::NoBuffer)) {
		if (buffer_.hasReadonlyFlag())
			flag.message(from_utf8(N_("Document is read-only")));
		else
			flag.message(from_utf8(N_("Document has been modified externally")));
		flag.setEnabled(false);
		return true;
	}

	// Are we in a DELETED change-tracking region?
	if (lookupChangeType(d->cursor_, true) == Change::DELETED
	    && !lyxaction.funcHasFlag(act, LyXAction::ReadOnly)
	    && !lyxaction.funcHasFlag(act, LyXAction::NoBuffer)) {
		flag.message(from_utf8(N_("This portion of the document is deleted.")));
		flag.setEnabled(false);
		return true;
	}

	Cursor & cur = d->cursor_;

	if (cur.getStatus(cmd, flag))
		return true;

	switch (act) {

	// FIXME: This is a bit problematic because we don't check if this is
	// a document BufferView or not for these LFUNs. We probably have to
	// dispatch both to currentBufferView() and, if that fails,
	// to documentBufferView(); same as we do now for current Buffer and
	// document Buffer. Ideally those LFUN should go to Buffer as they
	// operate on the full Buffer and the cursor is only needed either for
	// an Undo record or to restore a cursor position. But we don't know
	// how to do that inside Buffer of course.
	case LFUN_BUFFER_PARAMS_APPLY:
	case LFUN_LAYOUT_MODULES_CLEAR:
	case LFUN_LAYOUT_MODULE_ADD:
	case LFUN_LAYOUT_RELOAD:
	case LFUN_TEXTCLASS_APPLY:
	case LFUN_TEXTCLASS_LOAD:
		flag.setEnabled(!buffer_.isReadonly());
		break;

	case LFUN_UNDO:
		// We do not use the LyXAction flag for readonly because Undo sets the
		// buffer clean/dirty status by itself.
		flag.setEnabled(!buffer_.isReadonly() && buffer_.undo().hasUndoStack());
		break;
	case LFUN_REDO:
		// We do not use the LyXAction flag for readonly because Redo sets the
		// buffer clean/dirty status by itself.
		flag.setEnabled(!buffer_.isReadonly() && buffer_.undo().hasRedoStack());
		break;
	case LFUN_FILE_INSERT_PLAINTEXT_PARA:
	case LFUN_FILE_INSERT_PLAINTEXT: {
		docstring const & fname = cmd.argument();
		if (!FileName::isAbsolute(to_utf8(fname))) {
			flag.message(_("Absolute filename expected."));
			return false;
		}
		flag.setEnabled(cur.inTexted());
		break;
	}
	case LFUN_FILE_INSERT:
	case LFUN_BOOKMARK_SAVE:
		// FIXME: Actually, these LFUNS should be moved to Text
		flag.setEnabled(cur.inTexted());
		break;

	case LFUN_FONT_STATE:
	case LFUN_LABEL_INSERT:
	case LFUN_INFO_INSERT:
	case LFUN_PARAGRAPH_GOTO:
	case LFUN_NOTE_NEXT:
	case LFUN_WORD_FIND:
	case LFUN_WORD_FIND_FORWARD:
	case LFUN_WORD_FIND_BACKWARD:
	case LFUN_WORD_REPLACE:
	case LFUN_MARK_OFF:
	case LFUN_MARK_ON:
	case LFUN_MARK_TOGGLE:
	case LFUN_SEARCH_STRING_SET:
	case LFUN_SCREEN_RECENTER:
	case LFUN_SCREEN_SHOW_CURSOR:
	case LFUN_BIBTEX_DATABASE_ADD:
	case LFUN_BIBTEX_DATABASE_DEL:
	case LFUN_BIBTEX_DATABASE_LIST:
	case LFUN_STATISTICS:
	case LFUN_KEYMAP_OFF:
	case LFUN_KEYMAP_PRIMARY:
	case LFUN_KEYMAP_SECONDARY:
	case LFUN_KEYMAP_TOGGLE:
	case LFUN_INSET_SELECT_ALL:
		flag.setEnabled(true);
		break;

	case LFUN_GRAPHICS_UNIFY:
		flag.setEnabled(cur.countInsetsInSelection(GRAPHICS_CODE)>1);
		break;

	case LFUN_WORD_FINDADV: {
		FindAndReplaceOptions opt;
		istringstream iss(to_utf8(cmd.argument()));
		iss >> opt;
		flag.setEnabled(opt.repl_buf_name.empty()
				|| !buffer_.isReadonly());
		break;
	}
	
	case LFUN_REFERENCE_NEXT: {
		if (cmd.argument() == "anyref")
			flag.setEnabled(true);
		else
			flag.setEnabled(getInsetByCode<InsetRef>(cur, REF_CODE)
					|| getInsetByCode<InsetMathRef>(cur, MATH_REF_CODE)
					|| getInsetByCode<InsetMathRef>(cur, LABEL_CODE));
		break;
	}

	case LFUN_LABEL_GOTO:
		flag.setEnabled(!cmd.argument().empty()
		    || getInsetByCode<InsetRef>(cur, REF_CODE)
		    || getInsetByCode<InsetMathRef>(cur, MATH_REF_CODE));
		break;

	case LFUN_REFERENCE_TO_PARAGRAPH: {
		if (contains(cmd.argument(), from_ascii("cpageref"))
		     && buffer().params().xref_package != "cleveref"
		     && buffer().params().xref_package != "zref") {
			flag.setEnabled(false);
			break;
		}
		vector<string> const pids = getVectorFromString(cmd.getArg(0));
		for (string const & s : pids) {
			int const id = convert<int>(s);
			if (id < 0) {
				flag.setEnabled(false);
				break;
			}
		}
		flag.setEnabled(true);
		break;
	}

	case LFUN_CHANGES_MERGE:
	case LFUN_CHANGE_NEXT:
	case LFUN_CHANGE_PREVIOUS:
	case LFUN_ALL_CHANGES_ACCEPT:
	case LFUN_ALL_CHANGES_REJECT:
		flag.setEnabled(buffer_.areChangesPresent());
		break;

	case LFUN_SCREEN_UP:
	case LFUN_SCREEN_DOWN:
	case LFUN_SCROLL:
	case LFUN_SCREEN_UP_SELECT:
	case LFUN_SCREEN_DOWN_SELECT:
	case LFUN_INSET_FORALL:
		flag.setEnabled(true);
		break;

	case LFUN_LAYOUT_TABULAR:
		flag.setEnabled(cur.innerInsetOfType(TABULAR_CODE));
		break;

	case LFUN_LAYOUT:
		flag.setEnabled(!cur.inset().forcePlainLayout(cur.idx()));
		break;

	case LFUN_LAYOUT_PARAGRAPH:
		flag.setEnabled(cur.inset().allowParagraphCustomization(cur.idx()));
		break;

	case LFUN_BRANCH_ADD_INSERT:
		flag.setEnabled(!(cur.inTexted() && cur.paragraph().isPassThru()));
		break;

	case LFUN_DIALOG_SHOW_NEW_INSET:
		// FIXME: this is wrong, but I do not understand the
		// intent (JMarc)
		if (cur.inset().lyxCode() == CAPTION_CODE)
			return cur.inset().getStatus(cur, cmd, flag);
		// FIXME we should consider passthru paragraphs too.
		flag.setEnabled(!(cur.inTexted() && cur.paragraph().isPassThru()));
		break;

	case LFUN_CITATION_INSERT: {
		FuncRequest fr(LFUN_INSET_INSERT, "citation");
		// FIXME: This could turn in a recursive hell.
		// Shouldn't we use Buffer::getStatus() instead?
		flag.setEnabled(lyx::getStatus(fr).enabled());
		break;
	}
	case LFUN_INSET_APPLY: {
		string const name = cmd.getArg(0);
		Inset * inset = editedInset(name);
		if (inset) {
			FuncRequest fr(LFUN_INSET_MODIFY, cmd.argument());
			if (!inset->getStatus(cur, fr, flag)) {
				// Every inset is supposed to handle this
				LASSERT(false, break);
			}
		} else {
			FuncRequest fr(LFUN_INSET_INSERT, cmd.argument());
			flag = lyx::getStatus(fr);
		}
		break;
	}

	case LFUN_COPY:
		flag.setEnabled(cur.selection());
		break;

	case LFUN_STATISTICS_REFERENCE_CLAMP: {
		// disable optitem reset if clamp not used
		if  (cmd.argument() == "reset" && d->stats_ref_value_c_ == 0) {
				flag.setEnabled(false);
				break;
		}
		flag.setEnabled(true);
		break;

	}

	default:
		return false;
	}

	return true;
}


Inset * BufferView::editedInset(string const & name) const
{
	map<string, Inset *>::const_iterator it = d->edited_insets_.find(name);
	return it == d->edited_insets_.end() ? nullptr : it->second;
}


void BufferView::editInset(string const & name, Inset * inset)
{
	if (inset)
		d->edited_insets_[name] = inset;
	else
		d->edited_insets_.erase(name);
}


void BufferView::dispatch(FuncRequest const & cmd, DispatchResult & dr)
{
	LYXERR(Debug::ACTION, "BufferView::dispatch: cmd: " << cmd);

	string const argument = to_utf8(cmd.argument());
	Cursor & cur = d->cursor_;
	Cursor old = cur;

	// Don't dispatch function that does not apply to internal buffers.
	if (buffer_.isInternal()
	    && lyxaction.funcHasFlag(cmd.action(), LyXAction::NoInternal))
		return;

	// We'll set this back to false if need be.
	bool dispatched = true;
	buffer_.undo().beginUndoGroup();

	FuncCode const act = cmd.action();
	switch (act) {

	case LFUN_BUFFER_PARAMS_APPLY: {
		DocumentClassConstPtr olddc = buffer_.params().documentClassPtr();
		cur.recordUndoBufferParams();
		istringstream ss(to_utf8(cmd.argument()));
		Lexer lex;
		lex.setStream(ss);
		int const unknown_tokens = buffer_.readHeader(lex);
		if (unknown_tokens != 0) {
			LYXERR0("Warning in LFUN_BUFFER_PARAMS_APPLY!\n"
						<< unknown_tokens << " unknown token"
						<< (unknown_tokens == 1 ? "" : "s"));
		}
		updateDocumentClass(olddc);

		// We are most certainly here because of a change in the document
		// It is then better to make sure that all dialogs are in sync with
		// current document settings.
		dr.screenUpdate(Update::Force | Update::FitCursor);
		dr.forceBufferUpdate();
		break;
	}

	case LFUN_LAYOUT_MODULES_CLEAR: {
		// FIXME: this modifies the document in cap::switchBetweenClasses
		//  without calling recordUndo. Fix this before using
		//  recordUndoBufferParams().
		cur.recordUndoFullBuffer();
		buffer_.params().clearLayoutModules();
		makeDocumentClass();
		dr.screenUpdate(Update::Force);
		dr.forceBufferUpdate();
		break;
	}

	case LFUN_LAYOUT_MODULE_ADD: {
		BufferParams const & params = buffer_.params();
		if (!params.layoutModuleCanBeAdded(argument)) {
			LYXERR0("Module `" << argument <<
				"' cannot be added due to failed requirements or "
				"conflicts with installed modules.");
			break;
		}
		// FIXME: this modifies the document in cap::switchBetweenClasses
		//  without calling recordUndo. Fix this before using
		//  recordUndoBufferParams().
		cur.recordUndoFullBuffer();
		buffer_.params().addLayoutModule(argument);
		makeDocumentClass();
		dr.screenUpdate(Update::Force);
		dr.forceBufferUpdate();
		break;
	}

	case LFUN_TEXTCLASS_APPLY: {
		// since this shortcircuits, the second call is made only if
		// the first fails
		bool const success =
			LayoutFileList::get().load(argument, buffer_.temppath()) ||
			LayoutFileList::get().load(argument, buffer_.filePath());
		if (!success) {
			docstring s = bformat(_("The document class `%1$s' "
						 "could not be loaded."), from_utf8(argument));
			frontend::Alert::error(_("Could not load class"), s);
			break;
		}

		LayoutFile const * old_layout = buffer_.params().baseClass();
		LayoutFile const * new_layout = &(LayoutFileList::get()[argument]);

		if (old_layout == new_layout)
			// nothing to do
			break;

		// Save the old, possibly modular, layout for use in conversion.
		// FIXME: this modifies the document in cap::switchBetweenClasses
		//  without calling recordUndo. Fix this before using
		//  recordUndoBufferParams().
		cur.recordUndoFullBuffer();
		buffer_.params().setBaseClass(argument, buffer_.layoutPos());
		makeDocumentClass();
		dr.screenUpdate(Update::Force);
		dr.forceBufferUpdate();
		break;
	}

	case LFUN_TEXTCLASS_LOAD: {
		// since this shortcircuits, the second call is made only if
		// the first fails
		bool const success =
			LayoutFileList::get().load(argument, buffer_.temppath()) ||
			LayoutFileList::get().load(argument, buffer_.filePath());
		if (!success) {
			docstring s = bformat(_("The document class `%1$s' "
						 "could not be loaded."), from_utf8(argument));
			frontend::Alert::error(_("Could not load class"), s);
		}
		break;
	}

	case LFUN_LAYOUT_RELOAD: {
		LayoutFileIndex bc = buffer_.params().baseClassID();
		LayoutFileList::get().reset(bc);
		buffer_.params().setBaseClass(bc, buffer_.layoutPos());
		makeDocumentClass();
		dr.screenUpdate(Update::Force);
		dr.forceBufferUpdate();
		break;
	}

	case LFUN_UNDO: {
		dr.setMessage(_("Undo"));
		cur.clearSelection();
		// We need to find out if the bibliography information
		// has changed. See bug #11055.
		// So these should not be references...
		string const engine = buffer().params().citeEngine();
		CiteEngineType const enginetype = buffer().params().citeEngineType();
		if (!cur.undoAction())
			dr.setMessage(_("No further undo information"));
		else {
			dr.screenUpdate(Update::Force | Update::FitCursor);
			dr.forceBufferUpdate();
			resetInlineCompletionPos();
			if (buffer().params().citeEngine() != engine ||
			    buffer().params().citeEngineType() != enginetype)
				buffer().invalidateCiteLabels();
		}
		break;
	}

	case LFUN_REDO: {
		dr.setMessage(_("Redo"));
		cur.clearSelection();
		// We need to find out if the bibliography information
		// has changed. See bug #11055.
		// So these should not be references...
		string const engine = buffer().params().citeEngine();
		CiteEngineType const enginetype = buffer().params().citeEngineType();
		if (!cur.redoAction())
			dr.setMessage(_("No further redo information"));
		else {
			dr.screenUpdate(Update::Force | Update::FitCursor);
			dr.forceBufferUpdate();
			resetInlineCompletionPos();
			if (buffer().params().citeEngine() != engine ||
			    buffer().params().citeEngineType() != enginetype)
				buffer().invalidateCiteLabels();
		}
		break;
	}

	case LFUN_FONT_STATE:
		dr.setMessage(cur.currentState(false));
		break;

	case LFUN_BOOKMARK_SAVE:
		dr.screenUpdate(Update::Force);
		saveBookmark(convert<unsigned int>(to_utf8(cmd.argument())));
		break;

	case LFUN_LABEL_GOTO: {
		docstring label = cmd.argument();
		if (label.empty()) {
			InsetRef * inset =
				getInsetByCode<InsetRef>(cur, REF_CODE);
			if (inset) {
				label = inset->getParam("reference");
				// persistent=false: use temp_bookmark
				saveBookmark(0);
			}
		}
		if (!label.empty()) {
			gotoLabel(label);
			// at the moment, this is redundant, since gotoLabel will
			// eventually call LFUN_PARAGRAPH_GOTO, but it seems best
			// to have it here.
			dr.screenUpdate(Update::Force | Update::FitCursor);
		} else {
			InsetMathRef * minset =
				getInsetByCode<InsetMathRef>(cur, MATH_REF_CODE);
			if (minset)
				lyx::dispatch(FuncRequest(LFUN_LABEL_GOTO, minset->getTarget()));
		}
		break;
	}

	case LFUN_PARAGRAPH_GOTO: {
		int const id = convert<int>(cmd.getArg(0));
		pos_type const pos = convert<int>(cmd.getArg(1));
		if (id < 0)
			break;
		string const str_id_end = cmd.getArg(2);
		string const str_pos_end = cmd.getArg(3);
		int i = 0;
		for (Buffer * b = &buffer_; i == 0 || b != &buffer_;
			b = theBufferList().next(b)) {

			Cursor curs(*this);
			curs.setCursor(b->getParFromID(id));
			if (curs.atEnd()) {
				LYXERR(Debug::INFO, "No matching paragraph found! [" << id << "].");
				++i;
				continue;
			}
			LYXERR(Debug::INFO, "Paragraph " << curs.paragraph().id()
				<< " found in buffer `"
				<< b->absFileName() << "'.");

			if (b == &buffer_) {
				bool success;
				if (str_id_end.empty() || str_pos_end.empty()) {
					// Set the cursor
					curs.pos() = pos;
					mouseSetCursor(curs);
					success = true;
				} else {
					int const id_end = convert<int>(str_id_end);
					pos_type const pos_end = convert<int>(str_pos_end);
					success = setCursorFromEntries({id, pos},
					                               {id_end, pos_end});
				}
				if (success && scrollToCursor(d->cursor_, SCROLL_TOP))
						dr.screenUpdate(Update::Force);
			} else {
				// Switch to other buffer view and resend cmd
				lyx::dispatch(FuncRequest(
					LFUN_BUFFER_SWITCH, b->absFileName()));
				lyx::dispatch(cmd);
			}
			break;
		}
		break;
	}

	case LFUN_REFERENCE_TO_PARAGRAPH: {
		vector<string> const pids = getVectorFromString(cmd.getArg(0));
		string const type = cmd.getArg(1);
		int id = convert<int>(pids.back());
		inserted_label_.clear();
		if (id < 0)
			break;
		int i = 0;
		for (Buffer * b = &buffer_; i == 0 || b != &buffer_;
			b = theBufferList().next(b)) {
			DocIterator dit = b->getParFromID(id);
			if (dit.empty()) {
				LYXERR(Debug::INFO, "No matching paragraph found! [" << id << "].");
				++i;
				continue;
			}
			string label = dit.innerParagraph().getLabelForXRef();
			if (!label.empty()) {
				// if the paragraph has a label, we use this
				if (type == "forrefdialog")
					inserted_label_ = label;
				else {
					string const arg = (type.empty()) ? label : label + " " + type;
					lyx::dispatch(FuncRequest(LFUN_REFERENCE_INSERT, arg));
					cur.forceBufferUpdate();
				}
				break;
			} else {
				// if there is not a label yet
				// go to the paragraph (including nested insets) ...
				lyx::dispatch(FuncRequest(LFUN_BOOKMARK_SAVE, "0"));
				for (string const & s : pids) {
					id = convert<int>(s);
					if (id < 0)
						break;
					dit = b->getParFromID(id);
					lyx::dispatch(FuncRequest(LFUN_PARAGRAPH_GOTO, s));
				}
				// ... if not, insert a new label
				// we do not want to open the dialog, hence we
				// do not employ LFUN_LABEL_INSERT
				InsetCommandParams p(LABEL_CODE);
				docstring const new_label = dit.getPossibleLabel();
				p["name"] = new_label;
				string const data = InsetCommand::params2string(p);
				lyx::dispatch(FuncRequest(LFUN_INSET_INSERT, data));
				// ... go back to the original position
				lyx::dispatch(FuncRequest(LFUN_BOOKMARK_GOTO, "0"));
				if (type == "forrefdialog")
					// ... and save for the ref dialog to insert
					inserted_label_ = to_utf8(new_label);
				else {
					// ... or insert the ref directly (from outliner)
					string const arg = (type.empty()) ? to_utf8(new_label)
									  : to_utf8(new_label) + " " + type;
					lyx::dispatch(FuncRequest(LFUN_REFERENCE_INSERT, arg));
				}
				cur.forceBufferUpdate();
				break;
			}
		}
		break;
	}

	case LFUN_NOTE_NEXT:
		if (gotoInset(this, { NOTE_CODE }, false))
			dr.screenUpdate(Update::Force);
		break;

	case LFUN_REFERENCE_NEXT: {
		if (cmd.argument() == "anyref") {
			if (gotoInset(this, { REF_CODE }, false))
				dr.screenUpdate(Update::Force);
		} else {
			if (gotoInset(this, { LABEL_CODE, REF_CODE }, true))
				dr.screenUpdate(Update::Force);
		}
		break;
	}

	case LFUN_CHANGE_NEXT:
		findNextChange(this);
		if (cur.inset().isTable())
			// In tables, there might be whole changed rows or columns
			cur.dispatch(cmd);
		// FIXME: Move this LFUN to Buffer so that we don't have to do this:
		dr.screenUpdate(Update::Force | Update::FitCursor);
		break;

	case LFUN_CHANGE_PREVIOUS:
		findPreviousChange(this);
		if (cur.inset().isTable())
			// In tables, there might be whole changed rows or columns
			cur.dispatch(cmd);
		// FIXME: Move this LFUN to Buffer so that we don't have to do this:
		dr.screenUpdate(Update::Force | Update::FitCursor);
		break;

	case LFUN_CHANGES_MERGE:
		if (findNextChange(this) || findPreviousChange(this)) {
			dr.screenUpdate(Update::Force | Update::FitCursor);
			dr.forceBufferUpdate();
			showDialog("changes");
		}
		break;

	case LFUN_ALL_CHANGES_ACCEPT: {
		UndoGroupHelper helper(cur);
		// select complete document
		cur.reset();
		cur.selHandle(true);
		buffer_.text().cursorBottom(cur);
		// accept everything in a single step to support atomic undo
		// temporarily disable track changes in order to end with really
		// no new (e.g., DPSM-caused) changes (see #7487)
		bool const track = buffer_.params().track_changes;
		buffer_.params().track_changes = false;
		buffer_.text().acceptOrRejectChanges(cur, Text::ACCEPT);
		buffer_.params().track_changes = track;
		cur.resetAnchor();
		// FIXME: Move this LFUN to Buffer so that we don't have to do this:
		dr.screenUpdate(Update::Force | Update::FitCursor);
		dr.forceBufferUpdate();
		break;
	}

	case LFUN_ALL_CHANGES_REJECT: {
		UndoGroupHelper helper(cur);
		// select complete document
		cur.reset();
		cur.selHandle(true);
		buffer_.text().cursorBottom(cur);
		// reject everything in a single step to support atomic undo
		// temporarily disable track changes in order to end with really
		// no new (e.g., DPSM-caused) changes (see #7487)
		bool const track = buffer_.params().track_changes;
		buffer_.params().track_changes = false;
		buffer_.text().acceptOrRejectChanges(cur, Text::REJECT);
		buffer_.params().track_changes = track;
		cur.resetAnchor();
		// FIXME: Move this LFUN to Buffer so that we don't have to do this:
		dr.screenUpdate(Update::Force | Update::FitCursor);
		dr.forceBufferUpdate();
		break;
	}

	case LFUN_WORD_FIND_FORWARD:
	case LFUN_WORD_FIND_BACKWARD: {
		docstring searched_string;

		if (!cmd.argument().empty()) {
			setSearchRequestCache(cmd.argument());
			searched_string = cmd.argument();
		} else {
			searched_string = searchRequestCache();
		}

		if (searched_string.empty())
			break;

		docstring const data =
			find2string(searched_string, false, false,
				    act == LFUN_WORD_FIND_FORWARD, false, false, false);
		bool found = lyxfind(this, FuncRequest(LFUN_WORD_FIND, data));
		if (found)
			dr.screenUpdate(Update::Force | Update::FitCursor);
		else
			dr.setMessage(_("Search string not found!"));
		break;
	}

	case LFUN_WORD_FIND: {
		docstring arg = cmd.argument();
		if (arg.empty())
			arg = searchRequestCache();
		if (arg.empty()) {
			lyx::dispatch(FuncRequest(LFUN_DIALOG_SHOW, "findreplace"));
			break;
		}
		if (lyxfind(this, FuncRequest(act, arg)))
			dr.screenUpdate(Update::Force | Update::FitCursor);
		else
			dr.setMessage(_("Search string not found!"));

		setSearchRequestCache(arg);
		break;
	}

	case LFUN_SEARCH_STRING_SET: {
		docstring pattern = cmd.argument();
		if (!pattern.empty()) {
			setSearchRequestCache(pattern);
			break;
		}
		if (cur.selection())
			pattern = cur.selectionAsString(false);
		else if (!cur.inTexted())
			break; // not suitable for selectWord at cursor
		else {
			pos_type spos = cur.pos();
			cur.innerText()->selectWord(cur, WHOLE_WORD);
			pattern = cur.selectionAsString(false);
			cur.selection(false);
			cur.pos() = spos;
		}
		setSearchRequestCache(pattern);
		break;
	}

	case LFUN_WORD_REPLACE: {
		if (lyxreplace(this, cmd)) {
			dr.forceBufferUpdate();
			dr.screenUpdate(Update::Force | Update::FitCursor);
		}
		else
			dr.setMessage(_("Search string not found!"));
		break;
	}

	case LFUN_WORD_FINDADV: {
		FindAndReplaceOptions opt;
		istringstream iss(to_utf8(cmd.argument()));
		iss >> opt;
		if (findAdv(this, opt)) {
			dr.screenUpdate(Update::Force | Update::FitCursor);
			cur.dispatched();
			dispatched = true;
		} else {
			cur.undispatched();
			dispatched = false;
		}
		break;
	}

	case LFUN_INDEX_TAG_ALL: {
		if (cur.pos() == 0)
			// nothing precedes
			break;

		Inset * ins = cur.nextInset();
		if (!ins || ins->lyxCode() != INDEX_CODE)
			// not at index inset
			break;

		// clone the index inset
		InsetIndex * cins =
			new InsetIndex(static_cast<InsetIndex &>(*cur.nextInset()));
		// In order to avoid duplication, we compare the
		// LaTeX output if we find another index inset after
		// the word
		odocstringstream oilatex;
		otexstream oits(oilatex);
		OutputParams rp(&cur.buffer()->params().encoding());
		ins->latex(oits, rp);
		cap::copyInsetToTemp(cur, cins);

		// move backwards into preceding word
		// skip over other index insets
		cur.backwardPosIgnoreCollapsed();
		while (true) {
			if (cur.inset().lyxCode() == INDEX_CODE)
				cur.pop_back();
			else if (cur.prevInset() && cur.prevInset()->lyxCode() == INDEX_CODE)
				cur.backwardPosIgnoreCollapsed();
			else
				break;
		}
		if (!cur.inTexted()) {
			// Nothing to do here.
			setCursorFromInset(ins);
			break;
		}
		// Get word or selection
		cur.text()->selectWord(cur, WHOLE_WORD);
		docstring const searched_string = cur.selectionAsString(false);
		if (searched_string.empty())
			break;
		// Start from the beginning
		lyx::dispatch(FuncRequest(LFUN_BUFFER_BEGIN));
		while (findOne(this, searched_string,
			       false,// case sensitive
			       true,// match whole word only
			       true,// forward
			       false,//find deleted
			       false,//check wrap
			       false,// auto-wrap
			       false,// instant
			       false// only selection
			       )) {
			cur.clearSelection();
			Inset * ains = cur.nextInset();
			if (ains && ains->lyxCode() == INDEX_CODE) {
				// We have an index inset.
				// Check whether it has the same
				// LaTeX content and move on if so.
				odocstringstream filatex;
				otexstream fits(filatex);
				ains->latex(fits, rp);
				if (oilatex.str() == filatex.str())
					continue;
			}
			// Paste the inset and possibly continue
			cap::pasteFromTemp(cursor(), cursor().buffer()->errorList("Paste"));
		}
		// Go back to start position.
		setCursorFromInset(ins);
		dr.screenUpdate(cur.result().screenUpdate());
		if (cur.result().needBufferUpdate())
			dr.forceBufferUpdate();
		break;
	}

	case LFUN_MARK_OFF:
		cur.clearSelection();
		dr.setMessage(from_utf8(N_("Mark off")));
		break;

	case LFUN_MARK_ON:
		cur.clearSelection();
		cur.setMark(true);
		dr.setMessage(from_utf8(N_("Mark on")));
		break;

	case LFUN_MARK_TOGGLE:
		cur.selection(false);
		if (cur.mark()) {
			cur.setMark(false);
			dr.setMessage(from_utf8(N_("Mark removed")));
		} else {
			cur.setMark(true);
			dr.setMessage(from_utf8(N_("Mark set")));
		}
		cur.resetAnchor();
		break;

	case LFUN_SCREEN_SHOW_CURSOR:
		showCursor();
		break;

	case LFUN_SCREEN_RECENTER:
		recenter();
		break;

	case LFUN_BIBTEX_DATABASE_ADD: {
		Cursor tmpcur = cur;
		findInset(tmpcur, { BIBTEX_CODE }, false);
		InsetBibtex * inset = getInsetByCode<InsetBibtex>(tmpcur,
						BIBTEX_CODE);
		if (inset) {
			if (inset->addDatabase(cmd.argument()))
				dr.forceBufferUpdate();
		}
		break;
	}

	case LFUN_BIBTEX_DATABASE_DEL: {
		Cursor tmpcur = cur;
		findInset(tmpcur, { BIBTEX_CODE }, false);
		InsetBibtex * inset = getInsetByCode<InsetBibtex>(tmpcur,
						BIBTEX_CODE);
		if (inset) {
			if (inset->delDatabase(cmd.argument()))
				dr.forceBufferUpdate();
		}
		break;
	}

	case LFUN_GRAPHICS_UNIFY: {

		cur.recordUndoFullBuffer();

		DocIterator from, to;
		from = cur.selectionBegin();
		to = cur.selectionEnd();

		string const newId = cmd.getArg(0);
		bool fetchId = newId.empty(); //if we wait for groupId from first graphics inset

		InsetGraphicsParams grp_par;
		if (!fetchId)
			InsetGraphics::string2params(graphics::getGroupParams(buffer_, newId), buffer_, grp_par);

		if (!from.nextInset())	//move to closest inset
			from.forwardInset();

		while (!from.empty() && from < to) {
			Inset * inset = from.nextInset();
			if (!inset)
				break;
			InsetGraphics * ig = inset->asInsetGraphics();
			if (ig) {
				InsetGraphicsParams inspar = ig->getParams();
				if (fetchId) {
				        grp_par = inspar;
					fetchId = false;
				} else {
					grp_par.filename = inspar.filename;
					ig->setParams(grp_par);
				}
			}
			from.forwardInset();
		}
		dr.screenUpdate(Update::Force); //needed if triggered from context menu
		break;
	}

	case LFUN_BIBTEX_DATABASE_LIST: {
		docstring_list const & files = buffer_.getBibfiles();
		bool first = true;
		docstring result;
		char const separator(os::path_separator());
		for (auto const & file : files) {
			if (first)
				first = false;
			else
				result += separator;

			FileName const fn = buffer_.getBibfilePath(file);
			string const path = fn.realPath();
			result += from_utf8(os::external_path(path));
		}
		dr.setMessage(result);
		break;
	}

	case LFUN_STATISTICS: {
		Statistics & stats = buffer_.statistics();
		stats.update(cur);
		int const words = stats.word_count;
		int const chars = stats.char_count;
		int const chars_blanks = chars + stats.blank_count;
		docstring message;
		if (cur.selection())
			message = _("Statistics for the selection:");
		else
			message = _("Statistics for the document:");
		message += "\n\n";
		if (words != 1)
			message += bformat(_("%1$d words"), words);
		else
			message += _("One word");
		message += "\n";
		if (chars_blanks != 1)
			message += bformat(_("%1$d characters"), chars_blanks);
		else
			message += _("One character");
		message += "\n";
		if (chars != 1)
			message += bformat(_("%1$d characters (no blanks)"), chars);
		else
			message += _("One character (no blanks)");

		Alert::information(_("Statistics"), message);
		break;
	}

	case LFUN_STATISTICS_REFERENCE_CLAMP: {
		d->stats_update_trigger_ = true;
		if  (cmd.argument() == "reset") {
			d->stats_ref_value_w_ = d->stats_ref_value_c_ = d->stats_ref_value_nb_ = 0;
			break;
		}

		Statistics & stats = buffer_.statistics();
		stats.update(cur);
		d->stats_ref_value_w_ = stats.word_count;
		d->stats_ref_value_c_ = stats.char_count + stats.blank_count;
		d->stats_ref_value_nb_ = stats.char_count;
		break;
	}


	case LFUN_SCREEN_UP:
	case LFUN_SCREEN_DOWN: {
		Point p = getPos(cur);
		// replace x by the cursor target
		p.x = cur.targetX();
		// This code has been commented out to enable to scroll down a
		// document, even if there are large insets in it (see bug #5465).
		/*if (p.y < 0 || p.y > height_) {
			// The cursor is off-screen so recenter before proceeding.
			showCursor();
			p = getPos(cur);
		}*/
		scroll(act == LFUN_SCREEN_UP ? -height_ : height_);
		// update metrics
		int const correction = updateMetrics(false);
		if (act == LFUN_SCREEN_UP && correction < 0)
			p = Point(lyxrc.mac_like_cursor_movement ? 0 : p.x, 0);
		if (act == LFUN_SCREEN_DOWN && correction > 0)
			p = Point(lyxrc.mac_like_cursor_movement ? width_ : p.x, height_);
		bool const in_texted = cur.inTexted();
		cur.setCursor(doc_iterator_begin(cur.buffer()));
		cur.selHandle(false);

		d->text_metrics_[&buffer_.text()].editXY(cur, p.x, p.y);
		//FIXME: what to do with cur.x_target()?
		bool update = in_texted && cur.bv().checkDepm(cur, old);
		cur.finishUndo();

		if (update) {
			dr.screenUpdate(Update::Force | Update::FitCursor);
			dr.forceBufferUpdate();
		} else
			dr.screenUpdate(Update::ForceDraw | Update::FitCursor);
		break;
	}

	case LFUN_SCROLL: {
		string const scroll_type = cmd.getArg(0);
		if (scroll_type == "caret") {
			string const where = cmd.getArg(1);
			ScrollType how;
			if (where == "top")
				how = SCROLL_TOP;
			else if (where == "center")
				how = SCROLL_CENTER;
			else if (where == "bottom")
				how = SCROLL_BOTTOM;
			else if (where == "toggle")
				how = SCROLL_TOGGLE;
			else if (where == "visible")
				how = SCROLL_VISIBLE;
			else {
				dispatched = false;
				break;
			}
			showCursor(how);
			break;
		}

		int scroll_step = 0;
		if (scroll_type == "line")
			scroll_step = d->scrollbarParameters_.single_step;
		else if (scroll_type == "page")
			scroll_step = d->scrollbarParameters_.page_step;
		else {
			dispatched = false;
			break;
		}

		string const scroll_quantity = cmd.getArg(1);

		if (scroll_quantity == "up")
			scroll(-scroll_step);
		else if (scroll_quantity == "down")
			scroll(scroll_step);
		else if (isStrInt(scroll_quantity))
			scroll(scroll_step * convert<int>(scroll_quantity));
		else {
			dispatched = false;
			break;
		}

		dr.screenUpdate(Update::ForceDraw);
		dr.forceBufferUpdate();
		break;
	}

	case LFUN_SCREEN_UP_SELECT: {
		// FIXME: why is the algorithm different from LFUN_SCREEN_UP?
		cur.selHandle(true);
		if (isTopScreen()) {
			lyx::dispatch(FuncRequest(LFUN_BUFFER_BEGIN_SELECT));
			cur.finishUndo();
			break;
		}
		int y = getPos(cur).y;
		int const ymin = y - height_ + defaultRowHeight();
		while (y > ymin && cur.up())
			y = getPos(cur).y;

		cur.finishUndo();
		dr.screenUpdate(Update::SinglePar | Update::FitCursor);
		break;
	}

	case LFUN_SCREEN_DOWN_SELECT: {
		// FIXME: why is the algorithm different from LFUN_SCREEN_DOWN?
		cur.selHandle(true);
		if (isBottomScreen()) {
			lyx::dispatch(FuncRequest(LFUN_BUFFER_END_SELECT));
			cur.finishUndo();
			break;
		}
		int y = getPos(cur).y;
		int const ymax = y + height_ - defaultRowHeight();
		while (y < ymax && cur.down())
			y = getPos(cur).y;

		cur.finishUndo();
		dr.screenUpdate(Update::SinglePar | Update::FitCursor);
		break;
	}


	case LFUN_INSET_SELECT_ALL: {
		// true if all cells are selected
		bool const all_selected = cur.depth() > 1
		    && cur.selBegin().at_begin()
		    && cur.selEnd().at_end();
		// true if some cells are selected
		bool const cells_selected = cur.depth() > 1
		    && cur.selBegin().at_cell_begin()
			&& cur.selEnd().at_cell_end();
		if (all_selected || (cells_selected && !cur.inset().isTable())) {
			// All the contents of the inset if selected, or only at
			// least one cell but inset is not a table.
			// Select the inset from outside.
			cur.pop();
			cur.resetAnchor();
			cur.selection(true);
			cur.posForward();
		} else if (cells_selected) {
			// At least one complete cell is selected and inset is a table.
			// Select all cells
			cur.idx() = 0;
			cur.pit() = 0;
			cur.pos() = 0;
			cur.resetAnchor();
			cur.selection(true);
			cur.idx() = cur.lastidx();
			cur.pit() = cur.lastpit();
			cur.pos() = cur.lastpos();
		} else {
			// select current cell
			cur.pit() = 0;
			cur.pos() = 0;
			cur.resetAnchor();
			cur.selection(true);
			cur.pit() = cur.lastpit();
			cur.pos() = cur.lastpos();
		}
		cur.setCurrentFont();
		dr.screenUpdate(Update::Force);
		break;
	}


	case LFUN_UNICODE_INSERT: {
		if (cmd.argument().empty())
			break;

		FuncCode code = cur.inset().currentMode() == Inset::MATH_MODE ?
			LFUN_MATH_INSERT : LFUN_SELF_INSERT;
		int i = 0;
		while (true) {
			docstring const arg = from_utf8(cmd.getArg(i));
			if (arg.empty())
				break;
			if (!isHex(arg)) {
				LYXERR0("Not a hexstring: " << arg);
				++i;
				continue;
			}
			char_type c = hexToInt(arg);
			if (c >= 32 && c < 0x10ffff) {
				LYXERR(Debug::KEY, "Inserting c: " << c);
				lyx::dispatch(FuncRequest(code, docstring(1, c)));
			}
			++i;
		}
		break;
	}


	// This would be in Buffer class if only Cursor did not
	// require a bufferview
	case LFUN_INSET_FORALL: {
		docstring const name = from_utf8(cmd.getArg(0));
		string const commandstr = cmd.getLongArg(1);
		FuncRequest const fr = lyxaction.lookupFunc(commandstr);

		// an arbitrary number to limit number of iterations
		const int max_iter = 100000;
		int iterations = 0;
		Cursor & bvcur = d->cursor_;
		Cursor const savecur = bvcur;
		bvcur.reset();
		if (!bvcur.nextInset())
			bvcur.forwardInset();
		bvcur.beginUndoGroup();
		while(bvcur && iterations < max_iter) {
			Inset * const ins = bvcur.nextInset();
			if (!ins)
				break;
			docstring insname = ins->layoutName();
			while (!insname.empty()) {
				if (insname == name || name == from_utf8("*")) {
					lyx::dispatch(fr, dr);
					// we do not want to remember selection here
					bvcur.clearSelection();
					++iterations;
					break;
				}
				size_t const i = insname.rfind(':');
				if (i == string::npos)
					break;
				insname = insname.substr(0, i);
			}
			// if we did not delete the inset, skip it
			if (!bvcur.nextInset() || bvcur.nextInset() == ins)
				bvcur.forwardInset();
		}
		bvcur = savecur;
		bvcur.fixIfBroken();
		/** This is a dummy undo record only to remember the cursor
		 * that has just been set; this will be used on a redo action
		 * (see ticket #10097)

		 * FIXME: a better fix would be to have a way to set the
		 * cursor value directly, but I am not sure it is worth it.
		 */
		bvcur.recordUndo();
		bvcur.endUndoGroup();
		dr.screenUpdate(Update::Force);
		dr.forceBufferUpdate();

		if (iterations >= max_iter) {
			dr.setError(true);
			dr.setMessage(bformat(_("`inset-forall' interrupted because number of actions is larger than %1$d"), max_iter));
		} else
			dr.setMessage(bformat(_("Applied \"%1$s\" to %2$d insets"), from_utf8(commandstr), iterations));
		break;
	}


	case LFUN_BRANCH_ADD_INSERT: {
		docstring branch_name = from_utf8(cmd.getArg(0));
		if (branch_name.empty())
			if (!Alert::askForText(branch_name, _("Branch name")) ||
						branch_name.empty())
				break;

		DispatchResult drtmp;
		buffer_.dispatch(FuncRequest(LFUN_BRANCH_ADD, branch_name), drtmp);
		if (drtmp.error()) {
			Alert::warning(_("Branch already exists"), drtmp.message());
			break;
		}
		docstring const sep = buffer_.params().branchlist().separator();
		for (docstring const & branch : getVectorFromString(branch_name, sep))
			lyx::dispatch(FuncRequest(LFUN_BRANCH_INSERT, branch));
		break;
	}

	case LFUN_KEYMAP_OFF:
		getIntl().keyMapOn(false);
		break;

	case LFUN_KEYMAP_PRIMARY:
		getIntl().keyMapPrim();
		break;

	case LFUN_KEYMAP_SECONDARY:
		getIntl().keyMapSec();
		break;

	case LFUN_KEYMAP_TOGGLE:
		getIntl().toggleKeyMap();
		break;

	case LFUN_DIALOG_SHOW_NEW_INSET: {
		string const name = cmd.getArg(0);
		string data = trim(to_utf8(cmd.argument()).substr(name.size()));
		if (decodeInsetParam(name, data, buffer_))
			lyx::dispatch(FuncRequest(LFUN_DIALOG_SHOW, name + " " + data));
		else
			lyxerr << "Inset type '" << name <<
			"' not recognized in LFUN_DIALOG_SHOW_NEW_INSET" <<  endl;
		break;
	}

	case LFUN_CITATION_INSERT: {
		if (argument.empty()) {
			lyx::dispatch(FuncRequest(LFUN_DIALOG_SHOW_NEW_INSET, "citation"));
			break;
		}
		// we can have one optional argument, delimited by '|'
		// citation-insert <key>|<text_before>
		// this should be enhanced to also support text_after
		// and citation style
		string arg = argument;
		string opt1;
		if (contains(argument, "|")) {
			arg = token(argument, '|', 0);
			opt1 = token(argument, '|', 1);
		}

		// if our cursor is directly in front of or behind a citation inset,
		// we will instead add the new key to it.
		Inset * inset = cur.nextInset();
		if (!inset || inset->lyxCode() != CITE_CODE)
			inset = cur.prevInset();
		if (inset && inset->lyxCode() == CITE_CODE) {
			InsetCitation * icite = static_cast<InsetCitation *>(inset);
			if (icite->addKey(arg)) {
				dr.forceBufferUpdate();
				dr.screenUpdate(Update::FitCursor | Update::SinglePar);
				if (!opt1.empty())
					LYXERR0("Discarding optional argument to citation-insert.");
			}
			dispatched = true;
			break;
		}
		InsetCommandParams icp(CITE_CODE);
		icp["key"] = from_utf8(arg);
		if (!opt1.empty())
			icp["before"] = from_utf8(opt1);
		icp["literal"] =
			from_ascii(InsetCitation::last_literal ? "true" : "false");
		string icstr = InsetCommand::params2string(icp);
		FuncRequest fr(LFUN_INSET_INSERT, icstr);
		lyx::dispatch(fr);

		// if the request comes from the LyX server, then we
		// return a list of the undefined keys, in case some
		// action could be taken.
		if (cmd.origin() != FuncRequest::LYXSERVER)
			break;

		vector<docstring> keys = getVectorFromString(from_utf8(arg));
		vector<docstring>::iterator it = keys.begin();
		vector<docstring>::const_iterator end = keys.end();

		BiblioInfo const & bibInfo = buffer_.masterBibInfo();
		const BiblioInfo::const_iterator bibEnd = bibInfo.end();
		while (it != end) {
			if (bibInfo.find(*it) != bibEnd) {
				it = keys.erase(it);
				end = keys.end();
			} else
				++it;
		}
		dr.setMessage(getStringFromVector(keys));

		break;
	}

	case LFUN_INSET_APPLY: {
		string const name = cmd.getArg(0);
		Inset * inset = editedInset(name);
		if (!inset) {
			FuncRequest fr(LFUN_INSET_INSERT, cmd.argument());
			lyx::dispatch(fr);
			break;
		}
		// put cursor in front of inset.
		if (!setCursorFromInset(inset)) {
			LASSERT(false, break);
		}
		cur.recordUndo();
		FuncRequest fr(LFUN_INSET_MODIFY, cmd.argument());
		inset->dispatch(cur, fr);
		dr.screenUpdate(cur.result().screenUpdate());
		if (cur.result().needBufferUpdate())
			dr.forceBufferUpdate();
		break;
	}

	// FIXME:
	// The change of language of buffer belongs to the Buffer class.
	// We have to do it here because we need a cursor for Undo.
	// When Undo::recordUndoBufferParams() is implemented someday
	// LFUN_BUFFER_LANGUAGE should be handled by the Buffer class.
	case LFUN_BUFFER_LANGUAGE: {
		Language const * oldL = buffer_.params().language;
		Language const * newL = languages.getLanguage(argument);
		if (!newL || oldL == newL)
			break;
		if (oldL->rightToLeft() == newL->rightToLeft()) {
			cur.recordUndoFullBuffer();
			buffer_.changeLanguage(oldL, newL);
			cur.setCurrentFont();
			dr.forceBufferUpdate();
		}
		break;
	}

	case LFUN_FILE_INSERT_PLAINTEXT_PARA:
	case LFUN_FILE_INSERT_PLAINTEXT: {
		bool const as_paragraph = (act == LFUN_FILE_INSERT_PLAINTEXT_PARA);
		string const fname = to_utf8(cmd.argument());
		if (!FileName::isAbsolute(fname))
			dr.setMessage(_("Absolute filename expected."));
		else
			insertPlaintextFile(FileName(fname), as_paragraph);
		break;
	}

	case LFUN_COPY:
		// With multi-cell table content, we pass down to the inset
		if (cur.inTexted() && cur.selection()
		    && cur.selectionBegin().idx() != cur.selectionEnd().idx()) {
			buffer_.dispatch(cmd, dr);
			dispatched = dr.dispatched();
			break;
		}
		cap::copySelection(cur);
		cur.message(_("Copy"));
		break;

	default:
		// OK, so try the Buffer itself...
		buffer_.dispatch(cmd, dr);
		dispatched = dr.dispatched();
		break;
	}

	buffer_.undo().endUndoGroup();
	dr.dispatched(dispatched);

	// NOTE: The code below is copied from Cursor::dispatch. If you
	// need to modify this, please update the other one too.

	// notify insets we just entered/left
	if (cursor() != old) {
		old.beginUndoGroup();
		old.fixIfBroken();
		bool badcursor = notifyCursorLeavesOrEnters(old, cursor());
		if (badcursor) {
			cursor().fixIfBroken();
			resetInlineCompletionPos();
		}
		old.endUndoGroup();
	}
}


docstring BufferView::requestSelection()
{
	Cursor & cur = d->cursor_;

	LYXERR(Debug::SELECTION, "requestSelection: cur.selection: " << cur.selection());
	if (!cur.selection()) {
		d->xsel_cache_.set = false;
		return docstring();
	}

	LYXERR(Debug::SELECTION, "requestSelection: xsel_cache.set: " << d->xsel_cache_.set);
	if (!d->xsel_cache_.set ||
	    cur.top() != d->xsel_cache_.cursor ||
	    cur.realAnchor().top() != d->xsel_cache_.anchor)
	{
		d->xsel_cache_.cursor = cur.top();
		d->xsel_cache_.anchor = cur.realAnchor().top();
		d->xsel_cache_.set = cur.selection();
		return cur.selectionAsString(false);
	}
	return docstring();
}


void BufferView::clearSelection()
{
	d->cursor_.clearSelection();
	// Clear the selection buffer. Otherwise a subsequent
	// middle-mouse-button paste would use the selection buffer,
	// not the more current external selection.
	cap::clearSelection();
	d->xsel_cache_.set = false;
	// The buffer did not really change, but this causes the
	// redraw we need because we cleared the selection above.
	buffer_.changed(false);
}


void BufferView::resize(int width, int height)
{
	height_ = height;
	// Update metrics only if width has changed
	if (width != width_) {
		width_ = width;

		// Clear the paragraph height cache.
		d->par_height_.clear();
		// Redo the metrics.
		updateMetrics(true);
	}
	// metrics is OK, full drawing is necessary now
	d->update_flags_ = (d->update_flags_ & ~Update::Force) | Update::ForceDraw;
	d->update_strategy_ = FullScreenUpdate;
}


Inset const * BufferView::getCoveringInset(Text const & text,
		int x, int y) const
{
	TextMetrics & tm = d->text_metrics_[&text];
	Inset * inset = tm.checkInsetHit(x, y);
	if (!inset)
		return nullptr;

	if (!inset->descendable(*this))
		// No need to go further down if the inset is not
		// descendable.
		return inset;

	size_t cell_number = inset->nargs();
	// Check all the inner cell.
	for (size_t i = 0; i != cell_number; ++i) {
		Text const * inner_text = inset->getText(i);
		if (inner_text) {
			// Try deeper.
			Inset const * inset_deeper =
				getCoveringInset(*inner_text, x, y);
			if (inset_deeper)
				return inset_deeper;
		}
	}

	return inset;
}


Inset const * BufferView::clickableMathInset(InsetMathNest const * inset,
		CoordCache::Insets const & inset_cache, int x, int y) const
{
	for (size_t i = 0; i < inset->nargs(); ++i) {
		MathData const & md = inset->cell(i);
		for (size_t j = 0; j < md.size(); ++j) {
			string const name = lyxerr.debugging(Debug::MATHED)
				? insetName(md[j].nucleus()->lyxCode())
				: string();
			LYXERR(Debug::MATHED, "Checking inset: " << name);
			if (md[j].nucleus()->clickable(*this, x, y)) {
				if (inset_cache.covers(md[j].nucleus(), x, y)) {
					LYXERR(Debug::MATHED, "Clickable inset: "
					       << name);
					return md[j].nucleus();
				}
			}
			InsetMathNest const * imn =
				md[j].nucleus()->asNestInset();
			if (imn) {
				Inset const * inner =
					clickableMathInset(imn, inset_cache, x, y);
				if (inner)
					return inner;
			}
		}
	}
	return nullptr;
}


void BufferView::updateHoveredInset() const
{
	// Get inset under mouse, if there is one.
	int const x = d->mouse_position_cache_.x;
	int const y = d->mouse_position_cache_.y;
	Inset const * covering_inset = getCoveringInset(buffer_.text(), x, y);
	if (covering_inset && covering_inset->asInsetMath()) {
		Inset const * inner_inset = clickableMathInset(
				covering_inset->asInsetMath()->asNestInset(),
				coordCache().insets(), x, y);
		if (inner_inset)
			covering_inset = inner_inset;
	}

	d->clickable_inset_ = covering_inset && covering_inset->clickable(*this, x, y);

	if (covering_inset == d->last_inset_)
		// Same inset, no need to do anything...
		return;

	bool need_redraw = false;
	if (d->last_inset_) {
		// Remove the hint on the last hovered inset (if any).
		need_redraw |= d->last_inset_->setMouseHover(this, false);
		d->last_inset_ = nullptr;
	}

	if (covering_inset && covering_inset->setMouseHover(this, true)) {
		need_redraw = true;
		// Only the insets that accept the hover state, do
		// clear the last_inset_, so only set the last_inset_
		// member if the hovered setting is accepted.
		d->last_inset_ = covering_inset;
	}

	if (need_redraw) {
		LYXERR(Debug::PAINTING, "Mouse hover detected at: ("
				<< d->mouse_position_cache_.x << ", "
				<< d->mouse_position_cache_.y << ")");

		d->update_strategy_ = DecorationUpdate;

		// This event (moving without mouse click) is not passed further.
		// This should be changed if it is further utilized.
		buffer_.changed(false);
	}
}


void BufferView::clearLastInset(Inset * inset) const
{
	if (d->last_inset_ != inset) {
		LYXERR0("Wrong last_inset!");
		LATTEST(false);
	}
	d->last_inset_ = nullptr;
}


bool BufferView::mouseSelecting() const
{
	return d->mouse_selecting_;
}


int BufferView::stats_ref_value_w() const
{
	return d->stats_ref_value_w_;
}


int BufferView::stats_ref_value_c() const
{
	return d->stats_ref_value_c_;
}


int BufferView::stats_ref_value_nb() const
{
	return d->stats_ref_value_nb_;
}


void BufferView::mouseEventDispatch(FuncRequest const & cmd0)
{
	//lyxerr << "[ cmd0 " << cmd0 << "]" << endl;

	if (!ready())
		return;

	// This is only called for mouse related events including
	// LFUN_FILE_OPEN generated by drag-and-drop.
	FuncRequest cmd = cmd0;

	// Either the inset under the cursor or the
	// surrounding Text will handle this event.

	// make sure we stay within the screen...
	cmd.set_y(min(max(cmd.y(), -1), height_));

	d->mouse_position_cache_.x = cmd.x();
	d->mouse_position_cache_.y = cmd.y();

	d->mouse_selecting_ =
		cmd.action() == LFUN_MOUSE_MOTION && cmd.button() == mouse_button::button1;

	if (cmd.action() == LFUN_MOUSE_MOTION && cmd.button() == mouse_button::none) {
		updateHoveredInset();
		return;
	}

	Cursor old = cursor();
	Cursor cur(*this);
	cur.push(buffer_.inset());
	cur.selection(d->cursor_.selection());

	// Build temporary cursor.
	Inset * inset = d->text_metrics_[&buffer_.text()].editXY(cur, cmd.x(), cmd.y());
	if (inset) {
		// If inset is not editable, cur.pos() might point behind the
		// inset (depending on cmd.x(), cmd.y()). This is needed for
		// editing to fix bug 9628, but e.g. the context menu needs a
		// cursor in front of the inset.
		if ((inset->hasSettings() || !inset->contextMenuName().empty()
		     || inset->lyxCode() == SEPARATOR_CODE) &&
		    cur.nextInset() != inset && cur.prevInset() == inset)
			cur.posBackward();
	} else if (cur.inTexted() && cur.pos()
			&& cur.paragraph().isEnvSeparator(cur.pos() - 1)) {
		// Always place cursor in front of a separator inset.
		cur.posBackward();
	}

	// Put anchor at the same position.
	cur.resetAnchor();

	old.beginUndoGroup();

	// Try to dispatch to an non-editable inset near this position
	// via the temp cursor. If the inset wishes to change the real
	// cursor it has to do so explicitly by using
	//  cur.bv().cursor() = cur;  (or similar)
	if (inset)
		inset->dispatch(cur, cmd);

	// Now dispatch to the temporary cursor. If the real cursor should
	// be modified, the inset's dispatch has to do so explicitly.
	if (!inset || !cur.result().dispatched())
		cur.dispatch(cmd);

	// Notify left insets
	if (cur != old) {
		bool badcursor = old.fixIfBroken();
		badcursor |= cur.fixIfBroken();
		badcursor |= notifyCursorLeavesOrEnters(old, cur);
		if (badcursor)
			cursor().fixIfBroken();
	}

	old.endUndoGroup();

	// Do we have a selection?
	theSelection().haveSelection(cursor().selection());

	if (cur.needBufferUpdate() || buffer().needUpdate()) {
		cur.clearBufferUpdate();
		buffer().updateBuffer();
	}

	// If the command has been dispatched,
	if (cur.result().dispatched() || cur.result().screenUpdate())
		processUpdateFlags(cur.result().screenUpdate());
}


int BufferView::minVisiblePart()
{
	return 2 * defaultRowHeight();
}


void BufferView::scroll(int pixels)
{
	d->anchor_ypos_ -= pixels;
}


bool BufferView::setCursorFromRow(int row)
{
	TexRow::TextEntry start, end;
	tie(start,end) = buffer_.texrow().getEntriesFromRow(row);
	LYXERR(Debug::OUTFILE,
	       "setCursorFromRow: for row " << row << ", TexRow has found "
	       "start (id=" << start.id << ",pos=" << start.pos << "), "
	       "end (id=" << end.id << ",pos=" << end.pos << ")");
	return setCursorFromEntries(start, end);
}


bool BufferView::setCursorFromEntries(TexRow::TextEntry start,
                                      TexRow::TextEntry end)
{
	DocIterator dit_start, dit_end;
	tie(dit_start,dit_end) =
		TexRow::getDocIteratorsFromEntries(start, end, buffer_);
	if (!dit_start)
		return false;
	// Setting selection start
	d->cursor_.clearSelection();
	setCursor(dit_start);
	// Setting selection end
	if (dit_end) {
		d->cursor_.resetAnchor();
		setCursorSelectionTo(dit_end);
	}
	return true;
}


bool BufferView::setCursorFromInset(Inset const * inset)
{
	// are we already there?
	if (cursor().nextInset() == inset)
		return true;

	// Inset is not at cursor position. Find it in the document.
	Cursor cur(*this);
	cur.reset();
	while (cur && cur.nextInset() != inset)
		cur.forwardInset();

	if (cur) {
		setCursor(cur);
		return true;
	}
	return false;
}


void BufferView::gotoLabel(docstring const & label)
{
	FuncRequest action;
	bool have_inactive = false;
	for (Buffer const * buf : buffer().allRelatives()) {
		// find label
		for (TocItem const & item : *buf->tocBackend().toc("label")) {
			if (label == item.str() && item.isOutput()) {
				lyx::dispatch(item.action());
				return;
			}
			// If we find an inactive label, save it for the case
			// that no active one is there
			if (label == item.str() && !have_inactive) {
				have_inactive = true;
				action = item.action();
			}
		}
	}
	// We only found an inactive label. Go there.
	if (have_inactive)
		lyx::dispatch(action);
}


TextMetrics const & BufferView::textMetrics(Text const * t) const
{
	return const_cast<BufferView *>(this)->textMetrics(t);
}


TextMetrics & BufferView::textMetrics(Text const * t)
{
	LBUFERR(t);
	TextMetricsCache::iterator tmc_it  = d->text_metrics_.find(t);
	if (tmc_it == d->text_metrics_.end()) {
		tmc_it = d->text_metrics_.emplace(std::piecewise_construct,
		            std::forward_as_tuple(t),
		            std::forward_as_tuple(this, t)).first;
	}
	return tmc_it->second;
}


int BufferView::workHeight() const
{
	return height_;
}


void BufferView::setCursor(DocIterator const & dit)
{
	d->cursor_.reset();
	size_t const n = dit.depth();
	for (size_t i = 0; i < n; ++i)
		dit[i].inset().edit(d->cursor_, true);

	d->cursor_.setCursor(dit);
	d->cursor_.selection(false);
	d->cursor_.setCurrentFont();
	// FIXME
	// It seems on general grounds as if this is probably needed, but
	// it is not yet clear.
	// See bug #7394 and r38388.
	// d->cursor.resetAnchor();
}


void BufferView::setCursorSelectionTo(DocIterator const & dit)
{
	size_t const n = dit.depth();
	for (size_t i = 0; i < n; ++i)
		dit[i].inset().edit(d->cursor_, true);

	d->cursor_.selection(true);
	d->cursor_.setCursorSelectionTo(dit);
	d->cursor_.setCurrentFont();
}


bool BufferView::checkDepm(Cursor & cur, Cursor & old)
{
	// Would be wrong to delete anything if we have a selection.
	if (cur.selection())
		return false;

	bool need_anchor_change = false;
	bool changed = Text::deleteEmptyParagraphMechanism(cur, old,
		need_anchor_change);

	if (need_anchor_change)
		cur.resetAnchor();

	if (!changed)
		return false;

	d->cursor_ = cur;

	// we would rather not do this here, but it needs to be done before
	// the changed() signal is sent.
	buffer_.updateBuffer();

	buffer_.changed(true);
	return true;
}


bool BufferView::mouseSetCursor(Cursor & cur, bool const select)
{
	LASSERT(&cur.bv() == this, return false);

	if (!select)
		// this event will clear selection so we save selection for
		// persistent selection
		cap::saveSelection(cursor());

	d->cursor_.macroModeClose();
	// If a macro has been finalized, the cursor might have been broken
	cur.fixIfBroken();

	// Has the cursor just left the inset?
	bool const leftinset = (&d->cursor_.inset() != &cur.inset());
	if (leftinset)
		d->cursor_.fixIfBroken();

	// do the dEPM magic if needed
	// FIXME: (1) move this to InsetText::notifyCursorLeaves?
	// FIXME: (2) if we had a working InsetText::notifyCursorLeaves,
	// the leftinset bool would not be necessary (badcursor instead).
	bool update = leftinset;

	if (select) {
		d->cursor_.setSelection();
		d->cursor_.setCursorSelectionTo(cur);
	} else {
		if (d->cursor_.inTexted())
			update |= checkDepm(cur, d->cursor_);
		d->cursor_.resetAnchor();
		d->cursor_.setCursor(cur);
		d->cursor_.clearSelection();
	}
	d->cursor_.boundary(cur.boundary());
	d->cursor_.finishUndo();
	d->cursor_.setCurrentFont();
	if (update)
		cur.forceBufferUpdate();
	return update;
}


void BufferView::putSelectionAt(DocIterator const & cur,
				int length, bool backwards)
{
	d->cursor_.clearSelection();

	setCursor(cur);

	if (length) {
		if (backwards) {
			d->cursor_.pos() += length;
			d->cursor_.setSelection(d->cursor_, -length);
		} else
			d->cursor_.setSelection(d->cursor_, length);
	}
}


void BufferView::setSelection(DocIterator const & from,
			      DocIterator const & to)
{
	if (from.pit() != to.pit()) {
		// there are multiple paragraphs in selection
		cursor().setCursor(from);
		cursor().clearSelection();
		cursor().selection(true);
		cursor().setCursor(to);
		cursor().selection(true);
	} else {
		// only single paragraph
		int const size = to.pos() - from.pos();
		putSelectionAt(from, size, false);
	}
	processUpdateFlags(Update::Force | Update::FitCursor);
}


bool BufferView::selectIfEmpty(DocIterator & cur)
{
	if ((cur.inTexted() && !cur.paragraph().empty())
	    || (cur.inMathed() && !cur.cell().empty()))
		return false;

	pit_type const beg_pit = cur.pit();
	if (beg_pit > 0) {
		// The paragraph associated to this item isn't
		// the first one, so it can be selected
		cur.backwardPos();
	} else {
		// We have to resort to select the space between the
		// end of this item and the begin of the next one
		cur.forwardPos();
	}
	if (cur.empty()) {
		// If it is the only item in the document,
		// nothing can be selected
		return false;
	}
	pit_type const end_pit = cur.pit();
	pos_type const end_pos = cur.pos();
	d->cursor_.clearSelection();
	d->cursor_.reset();
	d->cursor_.setCursor(cur);
	d->cursor_.pit() = beg_pit;
	d->cursor_.pos() = 0;
	d->cursor_.selection(false);
	d->cursor_.resetAnchor();
	d->cursor_.pit() = end_pit;
	d->cursor_.pos() = end_pos;
	d->cursor_.setSelection();
	return true;
}


Cursor & BufferView::cursor()
{
	return d->cursor_;
}


Cursor const & BufferView::cursor() const
{
	return d->cursor_;
}


bool BufferView::singleParUpdate()
{
	CursorSlice const & its = d->cursor_.innerTextSlice();
	pit_type const pit = its.pit();
	TextMetrics & tm = textMetrics(its.text());
	Dimension const old_dim = tm.parMetrics(pit).dim();

	// make sure inline completion pointer is ok
	if (d->inlineCompletionPos_.fixIfBroken())
		d->inlineCompletionPos_ = DocIterator();

	if (!tm.contains(pit)) {
		LYXERR(Debug::PAINTING, "SinglePar optimization failed: no known metrics");
		return false;
	}
	/* Try to rebreak only the paragraph containing the cursor (if
	 * this paragraph contains insets etc., rebreaking will
	 * recursively descend). We need a full redraw if either
	 * 1/ the height has changed
	 * or
	 * 2/ the width has changed and it was equal to the textmetrics
	 *    width; the goal is to catch the case of a one-row inset that
	 *    grows with its contents, but optimize the case of typing at
	 *    the end of a mmultiple-row paragraph.
	 *
	 * NOTE: if only the height has changed, then it should be
	 *   possible to update all metrics at minimal cost. However,
	 *   since this is risky, we do not try that right now.
	 */
	tm.redoParagraph(pit);
	ParagraphMetrics & pm = tm.parMetrics(pit);
	if (pm.height() != old_dim.height()
	     || (pm.width() != old_dim.width() && old_dim.width() == tm.width())) {
		// Paragraph height or width has changed so we cannot proceed
		// to the singlePar optimisation.
		LYXERR(Debug::PAINTING, "SinglePar optimization failed: paragraph metrics changed");
		return false;
	}
	// Since position() points to the baseline of the first row, we
	// may have to update it. See ticket #11601 for an example where
	// the height does not change but the ascent does.
	if (pm.hasPosition())
		pm.setPosition(pm.position() - old_dim.ascent() + pm.ascent());
	else
		LYXERR0("SinglePar optimization succeeded, but no position to update");

	LYXERR(Debug::PAINTING, "\ny1: " << pm.top() << " y2: " << pm.bottom()
		<< " pit: " << pit << " singlepar: 1");
	return true;
}


void BufferView::updateMetrics()
{
	updateMetrics(true);
	// metrics is done, full drawing is necessary now
	d->update_flags_ = (d->update_flags_ & ~Update::Force) | Update::ForceDraw;
	d->update_strategy_ = FullScreenUpdate;
}


int BufferView::updateMetrics(bool force)
{
	if (!ready())
		return 0;

	//LYXERR0("updateMetrics " << _v_(force));

	Text & buftext = buffer_.text();
	pit_type const lastpit = int(buftext.paragraphs().size()) - 1;

	if (force) {
		// Clear out the position cache in case of full screen redraw,
		d->coord_cache_.clear();
		d->math_rows_.clear();

		// Clear out paragraph metrics to avoid having invalid metrics
		// in the cache from paragraphs not relayouted below. The
		// complete text metrics will be redone.
		d->text_metrics_.clear();
	}

	// This should not be moved earlier
	TextMetrics & tm = textMetrics(&buftext);

	// make sure inline completion pointer is ok
	if (d->inlineCompletionPos_.fixIfBroken())
		d->inlineCompletionPos_ = DocIterator();

	if (d->anchor_pit_ > lastpit)
		// The anchor pit must have been deleted...
		d->anchor_pit_ = lastpit;

	// Update metrics around the anchor
	tm.updateMetrics(d->anchor_pit_, d->anchor_ypos_, height_);

	// Check that the end of the document is not too high
	int const old_ypos = d->anchor_ypos_;
	int const min_visible = lyxrc.scroll_below_document ? minVisiblePart() : height_;
	if (tm.last().first == lastpit && tm.last().second->hasPosition()
	     && tm.last().second->bottom() < min_visible) {
		d->anchor_ypos_ += min_visible - tm.last().second->bottom();
		LYXERR(Debug::SCROLLING, "Too high, adjusting anchor ypos to " << d->anchor_ypos_);
		tm.updateMetrics(d->anchor_pit_, d->anchor_ypos_, height_);
	}

	// Check that the start of the document is not too low
	if (tm.first().first == 0 && tm.first().second->hasPosition()
	     && tm.first().second->top() > 0) {
		d->anchor_ypos_ -= tm.first().second->top();
		LYXERR(Debug::SCROLLING, "Too low, adjusting anchor ypos to " << d->anchor_ypos_);
		tm.updateMetrics(d->anchor_pit_, d->anchor_ypos_, height_);
	}

	// The global adjustment that have been made above
	int const correction = d->anchor_ypos_ - old_ypos;

	/* FIXME: do we want that? It avoids potential issues with old
	 * paragraphs that should have been recomputed but have not, at
	 * the price of potential extra metrics computation. I do not
	 * think that the performance gain is high, so that for now the
	 * extra paragraphs are removed
	 */
	// Remove paragraphs that are outside of screen
	while(!tm.first().second->hasPosition() || tm.first().second->bottom() <= 0) {
		//LYXERR0("Forget pit: " << tm.first().first);
		tm.forget(tm.first().first);
	}
	while(!tm.last().second->hasPosition() || tm.last().second->top() > height_) {
		//LYXERR0("Forget pit: " << tm.first().first);
		tm.forget(tm.last().first);
	}

	/* FIXME: if paragraphs outside of the screen are not removed
	 * above, one has to search for the first visible one here */
	// Normalize anchor for next time
	if (d->anchor_pit_ != tm.first().first
	    || d->anchor_ypos_ != tm.first().second->position()) {
		LYXERR(Debug::PAINTING, __func__ << ": Found new anchor pit = " << tm.first().first
				<< "  anchor ypos = " << tm.first().second->position()
				<< " (was " << d->anchor_pit_ << ", " << d->anchor_ypos_ << ")");
		d->anchor_pit_ = tm.first().first;
		d->anchor_ypos_ = tm.first().second->position();
	}

	// Now update the positions of insets in the cache.
	updatePosCache();

	if (lyxerr.debugging(Debug::WORKAREA)) {
		LYXERR(Debug::WORKAREA, "BufferView::updateMetrics");
		d->coord_cache_.dump();
	}
	return correction;
}


void BufferView::updatePosCache()
{
	// this is the "nodraw" drawing stage: only set the positions of the
	// insets in metrics cache.
	frontend::NullPainter np;
	draw(np, false);
}


void BufferView::insertLyXFile(FileName const & fname, bool const ignorelang)
{
	LASSERT(d->cursor_.inTexted(), return);

	// Get absolute path of file and add ".lyx"
	// to the filename if necessary
	FileName filename = fileSearch(string(), fname.absFileName(), "lyx");

	docstring const disp_fn = makeDisplayPath(filename.absFileName());
	// emit message signal.
	message(bformat(_("Inserting document %1$s..."), disp_fn));

	docstring res;
	Buffer buf(filename.absFileName(), false);
	if (buf.loadLyXFile() == Buffer::ReadSuccess) {
		ErrorList & el = buffer_.errorList("Parse");
		// Copy the inserted document error list into the current buffer one.
		el = buf.errorList("Parse");
		ParagraphList & pars = buf.paragraphs();
		if (ignorelang)
			// set main language of imported file to context language
			buf.changeLanguage(buf.language(), d->cursor_.getFont().language());
		buffer_.undo().recordUndo(d->cursor_);
		cap::replaceSelection(d->cursor_);
		cap::pasteParagraphList(d->cursor_, pars,
					buf.params().documentClassPtr(),
					buf.params().authors(), el);
		res = _("Document %1$s inserted.");
	} else {
		res = _("Could not insert document %1$s");
	}

	buffer_.changed(true);
	// emit message signal.
	message(bformat(res, disp_fn));
}


Point BufferView::coordOffset(DocIterator const & dit) const
{
	int x = 0;
	int y = 0;
	int lastw = 0;

	// Addup contribution of nested insets, from inside to outside,
	// keeping the outer paragraph for a special handling below
	for (size_t i = dit.depth() - 1; i >= 1; --i) {
		CursorSlice const & sl = dit[i];
		int xx = 0;
		int yy = 0;

		// get relative position inside sl.inset()
		sl.inset().cursorPos(*this, sl, (dit.boundary() || !d->im_->preeditString().empty()) && (i + 1 == dit.depth()), xx, yy);

		// Make relative position inside of the edited inset relative to sl.inset()
		x += xx;
		y += yy;

		// In case of an RTL inset, the edited inset will be positioned to the left
		// of xx:yy
		if (sl.text()) {
			bool boundary_i = dit.boundary() && i + 1 == dit.depth();
			bool rtl = textMetrics(sl.text()).isRTL(sl, boundary_i);
			if (rtl)
				x -= lastw;
		}

		// remember width for the case that sl.inset() is positioned in an RTL inset
		lastw = sl.inset().dimension(*this).wid;

		//lyxerr << "Cursor::getPos, i: "
		// << i << " x: " << xx << " y: " << y << endl;
	}

	// Add contribution of initial rows of outermost paragraph
	CursorSlice const & sl = dit[0];
	TextMetrics const & tm = textMetrics(sl.text());
	ParagraphMetrics const & pm = tm.parMetrics(sl.pit());

	LBUFERR(!pm.rows().empty());
	y -= pm.rows()[0].ascent();

	// Take boundary into account if cursor is in main text.
	// To set coords of preedit strings at the starting point in the first row,
	// they are treated as being on boundary at any time here. Note that preedit
	// strings are virtual with zero width but can occupy several rows on screen.
	bool const has_boundary = dit.depth() == 1 &&
	        (dit.boundary() || !d->im_->preeditString().empty());
	size_t const rend = pm.getRowIndex(sl.pos(), has_boundary);
	for (size_t rit = 0; rit != rend; ++rit)
		y += pm.rows()[rit].height();
	y += pm.rows()[rend].ascent();

	TextMetrics const & bottom_tm = textMetrics(dit.bottom().text());

	// Make relative position from the nested inset now bufferview absolute.
	int xx = bottom_tm.cursorX(dit.bottom(), has_boundary);
	x += xx;

	// In the RTL case place the nested inset at the left of the cursor in
	// the outer paragraph
	bool rtl = bottom_tm.isRTL(dit.bottom(), has_boundary);
	if (rtl)
		x -= lastw;

	return Point(x, y);
}


Point BufferView::getPos(DocIterator const & dit) const
{
	if (!hasPosition(dit))
		return Point(-1, -1);

	CursorSlice const & bot = dit.bottom();
	TextMetrics const & tm = textMetrics(bot.text());

	// offset from outer paragraph
	Point p = coordOffset(dit);
	p.y += tm.parMetrics(bot.pit()).position();
	return p;
}


bool BufferView::hasPosition(DocIterator const & dit) const
{
	CursorSlice const & bot = dit.bottom();
	TextMetrics const & tm = textMetrics(bot.text());

	return tm.contains(bot.pit()) && tm.parMetrics(bot.pit()).hasPosition();
}


void BufferView::caretPosAndDim(Point & p, Dimension & dim) const
{
	Cursor const & cur = cursor();
	if (cur.inMathed() && hasMathRow(&cur.cell())) {
		MathRow const & mrow = mathRow(&cur.cell());
		dim = mrow.caret_dim;
	} else {
		Font const font = cur.real_current_font;
		frontend::FontMetrics const & fm = theFontMetrics(font);
		// lineWidth() can be 0 to mean 'thin line' on HiDpi, but the
		// caret drawing code is not prepared for that.
		dim.wid = max(fm.lineWidth(), 1);
		dim.asc = fm.maxAscent();
		dim.des = fm.maxDescent();
	}
	if (lyxrc.cursor_width > 0)
		dim.wid = lyxrc.cursor_width;

	p = getPos(cur);
	// center fat carets horizontally
	p.x -= dim.wid / 2;
	// p is top-left
	p.y -= dim.asc;
}


void BufferView::buildCaretGeometry(bool complet, Point shift)
{
	Point p, origP;
	Dimension dim;
	caretPosAndDim(origP, dim);

	p.x = origP.x + shift.x;
	p.y = origP.y + shift.y;

	Cursor const & cur = d->cursor_;
	Font const & realfont = cur.real_current_font;
	frontend::FontMetrics const & fm = theFontMetrics(realfont.fontInfo());
	bool const isrtl = realfont.isVisibleRightToLeft();
	int const dir = isrtl ? -1 : 1;

	frontend::CaretGeometry & cg = d->caret_geometry_;
	cg.shapes.clear();

	// The caret itself, slanted for italics in text edit mode except
	// for selections because the selection rect does not slant
	bool const slant = fm.italic() && cur.inTexted() && !cur.selection();
	double const slope = slant ? fm.italicSlope() : 0;
	cg.shapes.push_back(
		{{iround(p.x + dim.asc * slope),                 p.y},
		 {iround(p.x - dim.des * slope),                 p.y + dim.height()},
		 {iround(p.x + dir * dim.wid - dim.des * slope), p.y + dim.height()},
		 {iround(p.x + dir * dim.wid + dim.asc * slope), p.y}}
		);

	// The language indicator _| (if needed)
	Language const * doclang = buffer().params().language;
	if (!((realfont.language() == doclang && isrtl == doclang->rightToLeft())
		  || realfont.language() == latex_language)) {
		int const lx = dim.height() / 3;
		int const xx = iround(p.x - dim.des * slope);
		int const yy = p.y + dim.height();
		cg.shapes.push_back(
			{{xx,                            yy - dim.wid},
			 {xx + dir * (dim.wid + lx - 1), yy - dim.wid},
			 {xx + dir * (dim.wid + lx - 1), yy},
			 {xx,                            yy}}
			);
	}

	// The completion triangle |> (if needed)
	if (complet) {
		int const m = p.y + dim.height() / 2;
		int const d = dim.height() / 8;
		// offset for slanted carret
		int const sx = iround((dim.asc - (dim.height() / 2 - d)) * slope);
		// starting position x
		int const xx = p.x + dir * dim.wid + sx;
		cg.shapes.push_back(
			{{xx,                     m - d},
			 {xx + dir * d,           m},
			 {xx,                     m + d},
			 {xx,                     m + d - dim.wid},
			 {xx + dir * d - dim.wid, m},
			 {xx,                     m - d + dim.wid}}
			);
	}

	// compute extremal x values
	cg.left = 1000000;
	cg.right = -1000000;
	cg.top = 1000000;
	cg.bottom = -1000000;
	for (auto const & shape : cg.shapes)
		for (Point const & p : shape) {
			cg.left = min(cg.left, p.x);
			cg.right = max(cg.right, p.x);
			cg.top = min(cg.top, p.y);
			cg.bottom = max(cg.bottom, p.y);
		}
}


frontend::CaretGeometry const &  BufferView::caretGeometry() const
{
	return d->caret_geometry_;
}


bool BufferView::caretInView() const
{
	if (!hasPosition(cursor()))
		return false;
	Point p;
	Dimension dim;
	caretPosAndDim(p, dim);

	// does the cursor touch the screen ?
	if (p.y + dim.height() < 0 || p.y >= workHeight())
		return false;
	return true;
}


int BufferView::horizScrollOffset() const
{
	return d->horiz_scroll_offset_;
}


int BufferView::horizScrollOffset(Text const * text,
                                  pit_type pit, pos_type pos) const
{
	// Is this a row that is currently scrolled?
	if (!d->current_row_slice_.empty()
	    && &text->inset() == d->current_row_slice_.inset().asInsetText()
	    && pit ==  d->current_row_slice_.pit()
	    && pos ==  d->current_row_slice_.pos())
		return d->horiz_scroll_offset_;
	return 0;
}


void BufferView::setCurrentRowSlice(CursorSlice const & rowSlice)
{
	// nothing to do if the cursor was already on this row
	if (d->current_row_slice_ == rowSlice)
		return;

	// if the (previous) current row was scrolled, we have to
	// remember it in order to repaint it next time.
	if (d->horiz_scroll_offset_ != 0) {
		// search the old row in cache and mark it changed
		for (auto & tm_pair : d->text_metrics_) {
			if (&tm_pair.first->inset() == rowSlice.inset().asInsetText()) {
				tm_pair.second.setRowChanged(rowSlice.pit(), rowSlice.pos());
				// We found it, no need to continue.
				break;
			}
		}
	}

	// Since we changed row, the scroll offset is not valid anymore
	d->horiz_scroll_offset_ = 0;
	d->current_row_slice_ = rowSlice;
}


void BufferView::checkCursorScrollOffset()
{
	CursorSlice rowSlice = d->cursor_.bottom();
	TextMetrics const & tm = textMetrics(rowSlice.text());

	// Stop if metrics have not been computed yet, since it means
	// that there is nothing to do.
	if (!tm.contains(rowSlice.pit()))
		return;
	ParagraphMetrics const & pm = tm.parMetrics(rowSlice.pit());
	Row const & row = pm.getRow(rowSlice.pos(),
	                            d->cursor_.boundary() && rowSlice == d->cursor_.top());
	rowSlice.pos() = row.pos();

	// Set the row on which the cursor lives.
	setCurrentRowSlice(rowSlice);

	// Current x position of the cursor in pixels
	int cur_x = getPos(d->cursor_).x;

	// Horizontal scroll offset of the cursor row in pixels
	int offset = d->horiz_scroll_offset_;
	int const MARGIN = 2 * theFontMetrics(d->cursor_.real_current_font).em()
	                   + row.right_margin;
	if (row.right_x() <= workWidth() - row.right_margin) {
		// Row is narrower than the work area, no offset needed.
		offset = 0;
	} else {
		if (cur_x - offset < MARGIN) {
			// cursor would be too far right
			offset = cur_x - MARGIN;
		} else if (cur_x - offset > workWidth() - MARGIN) {
			// cursor would be too far left
			offset = cur_x - workWidth() + MARGIN;
		}
		// Correct the offset to make sure that we do not scroll too much
		if (offset < 0)
			offset = 0;
		if (row.right_x() - offset < workWidth() - row.right_margin)
			offset = row.right_x() - workWidth() + row.right_margin;
	}

	//lyxerr << "cur_x=" << cur_x << ", offset=" << offset << ", row.wid=" << row.width() << ", margin=" << MARGIN << endl;

	if (offset != d->horiz_scroll_offset_) {
		LYXERR(Debug::PAINTING, "Horiz. scroll offset changed from "
		       << d->horiz_scroll_offset_ << " to " << offset);
		row.changed(true);
		if (d->update_strategy_ == NoScreenUpdate)
			d->update_strategy_ = SingleParUpdate;
	}

	d->horiz_scroll_offset_ = offset;
}


bool BufferView::busy() const
{
	return buffer().undo().activeUndoGroup();
}


void BufferView::draw(frontend::Painter & pain, bool paint_caret)
{
	if (!ready())
		return;
	LYXERR(Debug::PAINTING, (pain.isNull() ? "\t\t--- START NODRAW ---"
	                         : "\t\t*** START DRAWING ***"));
	Text & text = buffer_.text();
	TextMetrics const & tm = d->text_metrics_[&text];
	int const y = tm.first().second->position();
	PainterInfo pi(this, pain);

	// Check whether the row where the cursor lives needs to be scrolled.
	// Update the drawing strategy if needed.
	checkCursorScrollOffset();

	switch (d->update_strategy_) {

	case NoScreenUpdate:
		// no screen painting is actually needed. In nodraw stage
		// however, the different coordinates of insets and paragraphs
		// needs to be updated.
		LYXERR(Debug::PAINTING, "Strategy: NoScreenUpdate");
		if (pain.isNull()) {
			pi.full_repaint = true;
			tm.draw(pi, 0, y);
		} else {
			pi.full_repaint = false;
			tm.draw(pi, 0, y);
		}
		break;

	case SingleParUpdate:
		pi.full_repaint = false;
		LYXERR(Debug::PAINTING, "Strategy: SingleParUpdate");
		// In general, only the current row of the outermost paragraph
		// will be redrawn. Particular cases where selection spans
		// multiple paragraph are correctly detected in TextMetrics.
		tm.draw(pi, 0, y);
		break;

	case DecorationUpdate:
		// FIXME: We should also distinguish DecorationUpdate to avoid text
		// drawing if possible. This is not possible to do easily right now
		// because of the single backing pixmap.

	case FullScreenUpdate:

		LYXERR(Debug::PAINTING,
		       ((d->update_strategy_ == FullScreenUpdate)
			? "Strategy: FullScreenUpdate"
			: "Strategy: DecorationUpdate"));

		// The whole screen, including insets, will be refreshed.
		pi.full_repaint = true;

		// Clear background.
		pain.fillRectangle(0, 0, width_, height_,
			pi.backgroundColor(&buffer_.inset()));

		// Draw everything.
		tm.draw(pi, 0, y);

		break;
	}

	// Possibly grey out below
	if (d->update_strategy_ != NoScreenUpdate) {
		pair<pit_type, ParagraphMetrics const *> lastpm = tm.last();
		int const y2 = lastpm.second->bottom();

		if (y2 < height_) {
			Color color = buffer().isInternal()
				? Color_background : Color_bottomarea;
			pain.fillRectangle(0, y2, width_, height_ - y2, color);
		}
	}

	LYXERR(Debug::PAINTING, (pain.isNull() ? "\t\t --- END NODRAW ---"
	                        : "\t\t *** END DRAWING ***"));

	// The scrollbar needs an update.
	// FIXME: does it always? see ticket #11947.
	updateScrollbarParameters();

	// Normalize anchor for next time (in case updateMetrics did not do it yet)
	// FIXME: is this useful?
	for (auto const & [pit, pm] : tm) {
		if (pm.bottom() > 0) {
			if (d->anchor_pit_ != pit
			    || d->anchor_ypos_ != pm.position())
				LYXERR0(__func__ << ": Found new anchor pit = " << pit
						<< "  anchor ypos = " << pm.position()
						<< " (was " << d->anchor_pit_ << ", " << d->anchor_ypos_ << ")"
						   "\nIf you see this message, please report.");
			d->anchor_pit_ = pit;
			d->anchor_ypos_ = pm.position();
			break;
		}
	}

	if (!pain.isNull()) {
		// reset the update flags, everything has been done
		d->update_flags_ = Update::None;
	}

	// If a caret has to be painted, mark its text row as dirty to
	// make sure that it will be repainted on next redraw.
	/* FIXME: investigate whether this can be avoided when the cursor did not
	 * move at all
	 */
	if (paint_caret) {
		Cursor cur(d->cursor_);
		while (cur.depth() > 1) {
			if (cur.inTexted()) {
				TextMetrics const & tm = textMetrics(cur.text());
				if (d->caret_geometry_.left >= tm.origin().x
				    && d->caret_geometry_.right <= tm.origin().x + tm.dim().width())
					break;
			}
			cur.pop();
		}
		TextMetrics const & tm = textMetrics(cur.text());
		if (tm.contains(cur.pit())) {
			ParagraphMetrics const & pm = tm.parMetrics(cur.pit());
			pm.getRow(cur.pos(), cur.boundary()).changed(true);
		}
	}
}


void BufferView::message(docstring const & msg)
{
	if (d->gui_)
		d->gui_->message(msg);
}


void BufferView::showDialog(string const & name)
{
	if (d->gui_)
		d->gui_->showDialog(name, string());
}


void BufferView::showDialog(string const & name,
	string const & data, Inset * inset)
{
	if (d->gui_)
		d->gui_->showDialog(name, data, inset);
}


void BufferView::updateDialog(string const & name, string const & data)
{
	if (d->gui_)
		d->gui_->updateDialog(name, data);
}


void BufferView::setGuiDelegate(frontend::GuiBufferViewDelegate * gui)
{
	d->gui_ = gui;
}


// FIXME: Move this out of BufferView again
docstring BufferView::contentsOfPlaintextFile(FileName const & fname)
{
	if (!fname.isReadableFile()) {
		docstring const error = from_ascii(strerror(errno));
		docstring const file = makeDisplayPath(fname.absFileName(), 50);
		docstring const text =
		  bformat(_("Could not read the specified document\n"
			    "%1$s\ndue to the error: %2$s"), file, error);
		Alert::error(_("Could not read file"), text);
		return docstring();
	}

	if (!fname.isReadableFile()) {
		docstring const file = makeDisplayPath(fname.absFileName(), 50);
		docstring const text =
		  bformat(_("%1$s\n is not readable."), file);
		Alert::error(_("Could not open file"), text);
		return docstring();
	}

	// FIXME UNICODE: We don't know the encoding of the file
	docstring file_content = fname.fileContents("UTF-8");
	if (file_content.empty()) {
		Alert::error(_("Reading not UTF-8 encoded file"),
			     _("The file is not UTF-8 encoded.\n"
			       "It will be read as local 8Bit-encoded.\n"
			       "If this does not give the correct result\n"
			       "then please change the encoding of the file\n"
			       "to UTF-8 with a program other than LyX.\n"));
		file_content = fname.fileContents("local8bit");
	}

	return normalize_c(file_content);
}


void BufferView::insertPlaintextFile(FileName const & f, bool asParagraph)
{
	docstring const tmpstr = contentsOfPlaintextFile(f);

	if (tmpstr.empty())
		return;

	Cursor & cur = cursor();
	cap::replaceSelection(cur);
	buffer_.undo().recordUndo(cur);
	if (asParagraph)
		cur.innerText()->insertStringAsParagraphs(cur, tmpstr, cur.current_font);
	else
		cur.innerText()->insertStringAsLines(cur, tmpstr, cur.current_font);

	buffer_.changed(true);
}


docstring const & BufferView::inlineCompletion() const
{
	return d->inlineCompletion_;
}


size_t BufferView::inlineCompletionUniqueChars() const
{
	return d->inlineCompletionUniqueChars_;
}


DocIterator const & BufferView::inlineCompletionPos() const
{
	return d->inlineCompletionPos_;
}


void BufferView::resetInlineCompletionPos()
{
	d->inlineCompletionPos_ = DocIterator();
}


bool samePar(DocIterator const & a, DocIterator const & b)
{
	if (a.empty() && b.empty())
		return true;
	if (a.empty() || b.empty())
		return false;
	if (a.depth() != b.depth())
		return false;
	return &a.innerParagraph() == &b.innerParagraph();
}


void BufferView::setInlineCompletion(Cursor const & cur, DocIterator const & pos,
	docstring const & completion, size_t uniqueChars)
{
	uniqueChars = min(completion.size(), uniqueChars);
	bool changed = d->inlineCompletion_ != completion
		|| d->inlineCompletionUniqueChars_ != uniqueChars;
	bool singlePar = true;
	d->inlineCompletion_ = completion;
	d->inlineCompletionUniqueChars_ = min(completion.size(), uniqueChars);

	//lyxerr << "setInlineCompletion pos=" << pos << " completion=" << completion << " uniqueChars=" << uniqueChars << std::endl;

	// at new position?
	DocIterator const & old = d->inlineCompletionPos_;
	if (old != pos) {
		//lyxerr << "inlineCompletionPos changed" << std::endl;
		// old or pos are in another paragraph?
		if ((!samePar(cur, pos) && !pos.empty())
		    || (!samePar(cur, old) && !old.empty())) {
			singlePar = false;
			//lyxerr << "different paragraph" << std::endl;
		}
		d->inlineCompletionPos_ = pos;
	}

	// set update flags
	if (changed) {
		if (singlePar && !(cur.result().screenUpdate() & Update::Force))
			cur.screenUpdateFlags(cur.result().screenUpdate() | Update::SinglePar);
		else
			cur.screenUpdateFlags(cur.result().screenUpdate() | Update::Force);
	}
}


void BufferView::setInputMethod(frontend::InputMethod * im)
{
	LYXERR(Debug::DEBUG, "setInputMethod: Address of im_: " << im);
	d->im_ = im;
}


frontend::InputMethod * BufferView::inputMethod() const
{
	return d->im_;
}


bool BufferView::clickableInset() const
{
	return d->clickable_inset_;
}


bool BufferView::stats_update_trigger()
{
	if (d->stats_update_trigger_) {
		d->stats_update_trigger_ = false;
		return true;
	}
	return false;
}

} // namespace lyx
