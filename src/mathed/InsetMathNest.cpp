/**
 * \file InsetMathNest.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetMathNest.h"

#include "InsetMathArray.h"
#include "InsetMathAMSArray.h"
#include "InsetMathBig.h"
#include "InsetMathBrace.h"
#include "InsetMathChar.h"
#include "InsetMathColor.h"
#include "InsetMathComment.h"
#include "InsetMathDecoration.h"
#include "InsetMathDelim.h"
#include "InsetMathEnsureMath.h"
#include "InsetMathFont.h"
#include "InsetMathHull.h"
#include "InsetMathRef.h"
#include "InsetMathScript.h"
#include "InsetMathSpace.h"
#include "InsetMathUnknown.h"
#include "MathAutoCorrect.h"
#include "MathCompletionList.h"
#include "MathFactory.h"
#include "InsetMathMacro.h"
#include "InsetMathMacroArgument.h"
#include "MathParser.h"
#include "MathStream.h"
#include "MathSupport.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "CoordCache.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "DispatchResult.h"
#include "Encoding.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "LaTeXFeatures.h"
#include "LyX.h"
#include "LyXRC.h"
#include "MetricsInfo.h"
#include "TexRow.h"
#include "Text.h"

#include "frontends/Application.h"
#include "frontends/Clipboard.h"
#include "frontends/InputMethod.h"
#include "frontends/Selection.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/lstrings.h"
#include "support/textutils.h"

#include <algorithm>
#include <sstream>

using namespace std;
using namespace lyx::support;

namespace lyx {

using cap::copySelection;
using cap::grabAndEraseSelection;
using cap::cutSelection;
using cap::replaceSelection;
using cap::selClearOrDel;


InsetMathNest::InsetMathNest(Buffer * buf, idx_type nargs)
	: InsetMath(buf), cells_(nargs, MathData(buffer_)), lock_(false)
{
	// FIXME This should not really be necessary, but when we are
	// initializing the table of global macros, we create macros
	// with no associated Buffer.
	if (buf)
		setBuffer(*buf);
}


InsetMathNest::InsetMathNest(InsetMathNest const & inset)
	: InsetMath(inset), cells_(inset.cells_), lock_(inset.lock_)
{}


InsetMathNest::~InsetMathNest()
{
	map<BufferView const *, bool>::iterator it = mouse_hover_.begin();
	map<BufferView const *, bool>::iterator end = mouse_hover_.end();
	for (; it != end; ++it)
		if (it->second)
			it->first->clearLastInset(this);
}


InsetMathNest & InsetMathNest::operator=(InsetMathNest const & inset)
{
	cells_ = inset.cells_;
	lock_ = inset.lock_;
	mouse_hover_.clear();
	InsetMath::operator=(inset);
	return *this;
}


void InsetMathNest::setBuffer(Buffer & buffer)
{
	InsetMath::setBuffer(buffer);
	for (MathData & data : cells_)
		data.setBuffer(buffer);
}


idx_type InsetMathNest::nargs() const
{
	return cells_.size();
}


void InsetMathNest::cursorPos(BufferView const & bv,
		CursorSlice const & sl, bool /*boundary*/,
		int & x, int & y) const
{
// FIXME: This is a hack. Ideally, the coord cache should not store
// absolute positions, but relative ones. This would mean to call
// setXY() not in MathData::draw(), but in the parent insets' draw()
// with the correctly adjusted x,y values. But this means that we'd have
// to touch all (math)inset's draw() methods. Right now, we'll store
// absolute value, and make them here relative, only to make them
// absolute again when actually drawing the cursor. What a mess.
	LASSERT(&sl.inset() == this, return);
	MathData const & md = sl.cell();
	CoordCache const & coord_cache = bv.coordCache();
	if (!coord_cache.cells().has(&md)) {
		// this can (semi-)legally happen if we just created this cell
		// and it never has been drawn before. So don't ASSERT.
		//lyxerr << "no cached data for cell " << &md << endl;
		x = 0;
		y = 0;
		return;
	}
	Point const pt = coord_cache.cells().xy(&md);
	if (!coord_cache.insets().has(this)) {
		// same as above
		//lyxerr << "no cached data for inset " << this << endl;
		x = 0;
		y = 0;
		return;
	}
	Point const pt2 = coord_cache.insets().xy(this);
	//lyxerr << "retrieving position cache for MathData "
	//	<< pt.x << ' ' << pt.y << endl;
	x = pt.x - pt2.x + md.pos2x(&bv, sl.pos());
	y = pt.y - pt2.y;
//	lyxerr << "pt.y : " << pt.y << " pt2_.y : " << pt2.y
//		<< " asc: " << ascent() << "  des: " << descent()
//		<< " md.asc: " << md.ascent() << " md.des: " << md.descent() << endl;
	// move cursor visually into empty cells ("blue rectangles");
	if (md.empty() && bv.inputMethod()->preeditString().empty()) {
		Dimension const dim = coord_cache.cells().dim(&md);
		x += dim.wid / 3;
	}
}


void InsetMathNest::cellsMetrics(MetricsInfo const & mi) const
{
	MetricsInfo m = mi;
	for (auto const & cell : cells_) {
		Dimension dim;
		cell.metrics(m, dim);
	}
}


void InsetMathNest::updateBuffer(ParIterator const & it, UpdateType utype, bool const deleted)
{
	for (idx_type i = 0, n = nargs(); i != n; ++i)
		cell(i).updateBuffer(it, utype, deleted);
}



bool InsetMathNest::idxNext(Cursor & cur) const
{
	LASSERT(&cur.inset() == this, return false);
	if (cur.idx() == cur.lastidx())
		return false;
	++cur.idx();
	cur.pos() = 0;
	return true;
}


bool InsetMathNest::idxForward(Cursor & cur) const
{
	return idxNext(cur);
}


bool InsetMathNest::idxPrev(Cursor & cur) const
{
	LASSERT(&cur.inset() == this, return false);
	if (cur.idx() == 0)
		return false;
	--cur.idx();
	cur.pos() = lyxrc.mac_like_cursor_movement ? cur.lastpos() : 0;
	return true;
}


bool InsetMathNest::idxBackward(Cursor & cur) const
{
	return idxPrev(cur);
}


bool InsetMathNest::idxFirst(Cursor & cur) const
{
	LASSERT(&cur.inset() == this, return false);
	if (!isActive())
		return false;
	cur.idx() = firstIdx();
	cur.pos() = 0;
	return true;
}


bool InsetMathNest::idxLast(Cursor & cur) const
{
	LASSERT(&cur.inset() == this, return false);
	if (!isActive())
		return false;
	cur.idx() = lastIdx();
	cur.pos() = cur.lastpos();
	return true;
}


void InsetMathNest::dump() const
{
	odocstringstream oss;
	otexrowstream ots(oss);
	TeXMathStream os(ots);
	os << "---------------------------------------------\n";
	write(os);
	os << "\n";
	for (idx_type i = 0, n = nargs(); i != n; ++i)
		os << cell(i) << "\n";
	os << "---------------------------------------------\n";
	lyxerr << to_utf8(oss.str());
}


void InsetMathNest::draw(PainterInfo &, int, int) const
{
#if 0
	if (lock_)
		pi.pain.fillRectangle(x, y - ascent(), width(), height(),
					Color_mathlockbg);
#endif
}


void InsetMathNest::validate(LaTeXFeatures & features) const
{
	for (idx_type i = 0; i < nargs(); ++i)
		cell(i).validate(features);
}


void InsetMathNest::replace(ReplaceData & rep)
{
	for (idx_type i = 0; i < nargs(); ++i)
		cell(i).replace(rep);
}


bool InsetMathNest::contains(MathData const & md) const
{
	for (idx_type i = 0; i < nargs(); ++i)
		if (cell(i).contains(md))
			return true;
	return false;
}


bool InsetMathNest::lock() const
{
	return lock_;
}


void InsetMathNest::lock(bool l)
{
	lock_ = l;
}


bool InsetMathNest::isActive() const
{
	return nargs() > 0;
}


MathData InsetMathNest::glue() const
{
	MathData md(buffer_);
	for (size_t i = 0; i < nargs(); ++i)
		md.append(cell(i));
	return md;
}


void InsetMathNest::write(TeXMathStream & os) const
{
	MathEnsurer ensurer(os, currentMode() == MATH_MODE);
	ModeSpecifier specifier(os, currentMode(), lockedMode());
	docstring const latex_name = name();
	os << '\\' << latex_name;
	os.inMathClass(asClassInset());
	for (size_t i = 0; i < nargs(); ++i) {
		Changer dummy = os.changeRowEntry(TexRow::mathEntry(id(),i));
		os << '{' << cell(i) << '}';
	}
	// FIXME: can this ever happen?
	if (nargs() == 0)
		os.pendingSpace(true);
	if (lock_ && !os.latex()) {
		os << "\\lyxlock";
		os.pendingSpace(true);
	}
	os.inMathClass(false);
}


void InsetMathNest::normalize(NormalStream & os) const
{
	os << '[' << name();
	for (size_t i = 0; i < nargs(); ++i)
		os << ' ' << cell(i);
	os << ']';
}


void InsetMathNest::latex(otexstream & os, OutputParams const & runparams) const
{
	TeXMathStream::OutputType ot;
	if (runparams.find_effective())
		ot = TeXMathStream::wsSearchAdv;
	else if (runparams.dryrun)
		ot = TeXMathStream::wsDryrun;
	else
		ot = TeXMathStream::wsDefault;
	TeXMathStream wi(os, runparams.moving_arg, true, ot,
	                 runparams.encoding);
	wi.strikeoutMath(runparams.inDeletedInset);
	if (runparams.inulemcmd) {
		wi.ulemCmd(TeXMathStream::UNDERLINE);
		if (runparams.local_font) {
			FontInfo f = runparams.local_font->fontInfo();
			if (f.strikeout() == FONT_ON)
				wi.ulemCmd(TeXMathStream::STRIKEOUT);
		}
	}
	wi.canBreakLine(os.canBreakLine());
	Changer dummy = wi.changeRowEntry(TexRow::textEntry(runparams.lastid,
	                                                    runparams.lastpos));
	write(wi);
	// Reset parbreak and command termination status after a math inset.
	os.lastChar(0);
	os.canBreakLine(wi.canBreakLine());
	os.terminateCommand(false);
}


bool InsetMathNest::setMouseHover(BufferView const * bv, bool mouse_hover)
	const
{
	mouse_hover_[bv] = mouse_hover;
	return true;
}


bool InsetMathNest::notifyCursorLeaves(Cursor const & /*old*/, Cursor & /*cur*/)
{
	// FIXME: look here
#if 0
	MathData & md = cur.cell();
	// remove base-only "scripts"
	for (pos_type i = 0; i + 1 < md.size(); ++i) {
		InsetMathScript * p = operator[](i).nucleus()->asScriptInset();
		if (p && p->nargs() == 1) {
			MathData md = p->nuc();
			erase(i);
			insert(i, md);
			cur.adjust(i, md.size() - 1);
		}
	}

	// glue adjacent font insets of the same kind
	for (pos_type i = 0; i + 1 < size(); ++i) {
		InsetMathFont * p = operator[](i).nucleus()->asFontInset();
		InsetMathFont const * q = operator[](i + 1)->asFontInset();
		if (p && q && p->name() == q->name()) {
			p->cell(0).append(q->cell(0));
			erase(i + 1);
			cur.adjust(i, -1);
		}
	}
#endif
	return false;
}


void InsetMathNest::handleFont
	(Cursor & cur, docstring const & arg, char const * const font)
{
	handleFont(cur, arg, from_ascii(font));
}


void InsetMathNest::handleFont(Cursor & cur, docstring const & arg,
	docstring const & font)
{
	cur.recordUndoSelection();

	// this whole function is a hack and won't work for incremental font
	// changes...
	if (cur.inset().asInsetMath()->name() == font)
		cur.handleFont(to_utf8(font));
	else
		handleNest(cur, createInsetMath(font, cur.buffer()), arg);
}


void InsetMathNest::handleNest(Cursor & cur, MathAtom const & nest)
{
	handleNest(cur, nest, docstring());
}


void InsetMathNest::handleNest(Cursor & cur, MathAtom const & nest,
	docstring const & arg)
{
	DocIterator const i1 = cur.selectionBegin();
	DocIterator const i2 = cur.selectionEnd();
	if (!i1.inset().asInsetMath())
		return;
	if (i1.idx() == i2.idx()) {
		// the easy case where only one cell is selected
		cur.handleNest(nest);
		cur.insert(arg);
		return;
	}

	// multiple selected cells in a simple non-grid inset
	if (i1.inset().nrows() == 0 || i1.inset().ncols() == 0) {
		for (idx_type i = i1.idx(); i <= i2.idx(); ++i) {
			cur.setCursor(i1);
			// select cell
			cur.idx() = i;
			cur.pos() = 0;
			cur.resetAnchor();
			cur.pos() = cur.lastpos();
			cur.setSelection();

			// do the real job
			cur.handleNest(nest);
			cur.insert(arg);
		}
		return;
	}

	// the complicated case with multiple selected cells in a grid
	row_type r1, r2;
	col_type c1, c2;
	cap::region(i1.top(), i2.top(), r1, r2, c1, c2);
	for (row_type row = r1; row <= r2; ++row) {
		for (col_type col = c1; col <= c2; ++col) {
			cur.setCursor(i1);
			// select cell
			cur.idx() = i1.inset().index(row, col);
			cur.pos() = 0;
			cur.resetAnchor();
			cur.pos() = cur.lastpos();
			cur.setSelection();

			// do the real job
			cur.handleNest(nest);
			cur.insert(arg);
		}
	}
}


void InsetMathNest::handleFont2(Cursor & cur, docstring const & arg)
{
	cur.recordUndoSelection();
	bool include_previous_change = false;
	bool selection = cur.selection();
	DocIterator sel_begin = cur.selectionBegin();
	DocIterator sel_end = cur.selectionEnd();
	bool multiple_cells = sel_begin.idx() != sel_end.idx();
	Font font;
	docstring im;
	InsetMathFont const * f = asFontInset();
	bool b;

	// Return if nothing has been set
	if (!font.fromString(to_utf8(arg), b))
		return;

	if (support::contains(arg, from_ascii("family"))) {
		switch(font.fontInfo().family()) {
		case ROMAN_FAMILY:
			if (!f || (f->name() != "textrm" && f->name() != "mathrm"))
				im = currentMode() != MATH_MODE ? from_ascii("textrm")
								: from_ascii("mathrm");
			break;
		case SANS_FAMILY:
			if (!f || (f->name() != "textsf" && f->name() != "mathsf"))
				im = currentMode() != MATH_MODE ? from_ascii("textsf")
								: from_ascii("mathsf");
			break;
		case TYPEWRITER_FAMILY:
			if (!f || (f->name() != "texttt" && f->name() != "mathtt"))
				im = currentMode() != MATH_MODE ? from_ascii("texttt")
								: from_ascii("mathtt");
			break;
		case SYMBOL_FAMILY:
		case CMR_FAMILY:
		case CMSY_FAMILY:
		case CMM_FAMILY:
		case CMEX_FAMILY:
		case MSA_FAMILY:
		case MSB_FAMILY:
		case DS_FAMILY:
		case EUFRAK_FAMILY:
		case RSFS_FAMILY:
		case STMARY_FAMILY:
		case WASY_FAMILY:
		case ESINT_FAMILY:
		case INHERIT_FAMILY:
		case IGNORE_FAMILY:
			break;
		}
		if (!im.empty()) {
			handleNest(cur, createInsetMath(im, cur.buffer()));
			im.clear();
			include_previous_change = true;
		}
	}

	if (support::contains(arg, from_ascii("series"))) {
		switch(font.fontInfo().series()) {
		case MEDIUM_SERIES:
			if (!f || (f->name() != "textmd" && f->name() != "mathrm"))
				im = currentMode() != MATH_MODE ? from_ascii("textmd")
								: from_ascii("mathrm");
			break;
		case BOLD_SERIES:
			if (!f || (f->name() != "textbf" && f->name() != "mathbf"))
				im = currentMode() != MATH_MODE ? from_ascii("textbf")
								: from_ascii("boldsymbol");
			break;
		case INHERIT_SERIES:
		case IGNORE_SERIES:
			break;
		}
		if (!im.empty()) {
			if (multiple_cells) {
				cur.setCursor(sel_begin);
				cur.idx() = 0;
				cur.pos() = 0;
				cur.resetAnchor();
				cur.setCursor(sel_end);
				cur.pos() = cur.lastpos();
				cur.setSelection();
			} else if (include_previous_change && selection) {
				cur.setSelection();
			}
			handleNest(cur, createInsetMath(im, cur.buffer()));
			im.clear();
			include_previous_change = true;
		}
	}

	if (support::contains(arg, from_ascii("shape"))) {
		switch(font.fontInfo().shape()) {
		case UP_SHAPE:
			if (!f || (f->name() != "textup" && f->name() != "mathrm"))
				im = currentMode() != MATH_MODE ? from_ascii("textup")
								: from_ascii("mathrm");
			break;
		case ITALIC_SHAPE:
			if (!f || (f->name() != "textit" && f->name() != "mathit"))
				im = currentMode() != MATH_MODE ? from_ascii("textit")
								: from_ascii("mathit");
			break;
		case SLANTED_SHAPE:
			if (!f || f->name() != "textsl")
				im = currentMode() != MATH_MODE ? from_ascii("textsl")
								: docstring();
			break;
		case SMALLCAPS_SHAPE:
			if (!f || f->name() != "textsc")
				im = currentMode() != MATH_MODE ? from_ascii("textsc")
								: docstring();
			break;
		case INHERIT_SHAPE:
		case IGNORE_SHAPE:
			break;
		}
		if (!im.empty()) {
			if (multiple_cells) {
				cur.setCursor(sel_begin);
				cur.idx() = 0;
				cur.pos() = 0;
				cur.resetAnchor();
				cur.setCursor(sel_end);
				cur.pos() = cur.lastpos();
				cur.setSelection();
			} else if (include_previous_change && selection) {
				cur.setSelection();
			}
			handleNest(cur, createInsetMath(im, cur.buffer()));
			im.clear();
			include_previous_change = true;
		}
	}

	if (support::contains(arg, from_ascii("size"))) {
		switch(font.fontInfo().size()) {
		case TINY_SIZE:
			im = from_ascii("tiny");
			break;
		case SCRIPT_SIZE:
			im = from_ascii("scriptsize");
			break;
		case FOOTNOTE_SIZE:
			im = from_ascii("footnotesize");
			break;
		case SMALL_SIZE:
			im = from_ascii("small");
			break;
		case NORMAL_SIZE:
			im = from_ascii("normalsize");
			break;
		case LARGE_SIZE:
			im = from_ascii("large");
			break;
		case LARGER_SIZE:
			im = from_ascii("Large");
			break;
		case LARGEST_SIZE:
			im = from_ascii("LARGE");
			break;
		case HUGE_SIZE:
			im = from_ascii("huge");
			break;
		case HUGER_SIZE:
			im = from_ascii("Huge");
			break;
		case INCREASE_SIZE:
		case DECREASE_SIZE:
		case INHERIT_SIZE:
		case IGNORE_SIZE:
			break;
		}
		if (!im.empty()) {
			if (multiple_cells) {
				cur.setCursor(sel_begin);
				cur.idx() = 0;
				cur.pos() = 0;
				cur.resetAnchor();
				cur.setCursor(sel_end);
				cur.pos() = cur.lastpos();
				cur.setSelection();
			} else if (include_previous_change && selection) {
				cur.setSelection();
			}
			handleNest(cur, createInsetMath(im, cur.buffer()));
			im.clear();
			include_previous_change = true;
		}
	}

	InsetMathDecoration const * d = multiple_cells
					? nullptr : asDecorationInset();
	docstring const name = d ? d->name() : docstring();

	if ((font.fontInfo().underbar() == FONT_OFF && name == "uline") ||
	    (font.fontInfo().uuline() == FONT_OFF && name == "uuline") ||
	    (font.fontInfo().uwave() == FONT_OFF && name == "uwave")) {
		if (include_previous_change) {
			if (!selection)
				cur.popForward();
			cur.setSelection();
		}
		docstring const beg = '\\' + name + '{';
		docstring const end = from_ascii("}");
		docstring const sel2 = cur.selectionAsString(false);
		cutSelection(cur, false);
		cur.pos() = 0;
		cur.setSelection();
		docstring const sel1 = cur.selectionAsString(false);
		cur.pos() = cur.lastpos();
		cur.setSelection();
		docstring const sel3 = cur.selectionAsString(false);
		cur.mathForward(false);
		cur.setSelection();
		cutSelection(cur, false);
		MathData md(buffer_);
		if (!sel1.empty()) {
			mathed_parse_cell(md, beg + sel1 + end);
			cur.insert(md);
		}
		cur.resetAnchor();
		if (!sel2.empty()) {
			mathed_parse_cell(md, sel2);
			cur.insert(md);
		}
		if (!sel3.empty()) {
			pos_type pos = cur.pos();
			mathed_parse_cell(md, beg + sel3 + end);
			cur.insert(md);
			cur.pos() = pos;
		}
		cur.setSelection();
		sel_begin = cur.selectionBegin();
		sel_end = cur.selectionEnd();
		include_previous_change = false;
	}

	if (font.fontInfo().underbar() == FONT_ON) {
		if (!d || name != "uline")
			im = from_ascii("uline");
	} else if (font.fontInfo().uuline() == FONT_ON) {
		if (!d || name != "uuline")
			im = from_ascii("uuline");
	} else if (font.fontInfo().uwave() == FONT_ON) {
		if (!d || name != "uwave")
			im = from_ascii("uwave");
	}

	if (!im.empty()) {
		if (multiple_cells) {
			cur.setCursor(sel_begin);
			cur.idx() = 0;
			cur.pos() = 0;
			cur.resetAnchor();
			cur.setCursor(sel_end);
			cur.pos() = cur.lastpos();
			cur.setSelection();
		} else if (include_previous_change && selection) {
			cur.setSelection();
		}
		handleNest(cur, createInsetMath(im, cur.buffer()));
		im.clear();
		include_previous_change = true;
	}

	if (support::contains(arg, from_ascii("color"))) {
		if (font.fontInfo().color() != Color_inherit &&
		    font.fontInfo().color() != Color_ignore) {
			if (multiple_cells) {
				cur.setCursor(sel_begin);
				cur.idx() = 0;
				cur.pos() = 0;
				cur.resetAnchor();
				cur.setCursor(sel_end);
				cur.pos() = cur.lastpos();
				cur.setSelection();
			} else if (include_previous_change && selection) {
				cur.setSelection();
			}
			handleNest(cur, MathAtom(new InsetMathColor(buffer_, true, font.fontInfo().color())));
			include_previous_change = true;
		}
	}

	if (selection) {
		if (multiple_cells) {
			cur.setCursor(sel_begin);
			cur.idx() = 0;
			cur.pos() = 0;
			cur.resetAnchor();
			cur.setCursor(sel_end);
			cur.pos() = cur.lastpos();
			cur.setSelection();
		} else {
			if (include_previous_change) {
				sel_end = cur;
				cur.backwardInset();
			} else {
				cur.setCursor(sel_begin);
			}
			cur.resetAnchor();
			cur.setCursor(sel_end);
			cur.setSelection();
		}
	}

	// FIXME: support other font changes here as well?
}

void InsetMathNest::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	//LYXERR0("InsetMathNest: request: " << cmd);

	Parse::flags parseflg = Parse::QUIET | Parse::USETEXT;

	FuncCode const act = cmd.action();
	switch (act) {

	case LFUN_CLIPBOARD_PASTE:
		parseflg |= Parse::VERBATIM;
		// fall through
	case LFUN_PASTE: {
		if (cur.currentMode() != MATH_MODE)
			parseflg |= Parse::TEXTMODE;
		cur.recordUndoSelection();
		cur.message(_("Paste"));
		replaceSelection(cur);
		docstring topaste;
		if (cmd.argument().empty() && !theClipboard().isInternal())
			topaste = theClipboard().getAsText(frontend::Clipboard::PlainTextType);
		else {
			size_t n = 0;
			idocstringstream is(cmd.argument());
			is >> n;
			topaste = cap::selection(n, make_pair(buffer().params().documentClassPtr(),
							      buffer().params().authors()));
		}
		InsetMath const * im = cur.inset().asInsetMath();
		InsetMathMacro const * macro = im ? im->asMacroInset() : nullptr;
		// do not allow pasting a backslash in the name of a macro
		if (macro
		    && macro->displayMode() == InsetMathMacro::DISPLAY_UNFOLDED
		    && support::contains(topaste, char_type('\\'))) {
			LYXERR0("Removing backslash from pasted string");
			topaste = subst(topaste, from_ascii("\\"), docstring());
		}
		cur.niceInsert(topaste, parseflg, false);
		cur.clearSelection(); // bug 393
		cur.forceBufferUpdate();
		cur.finishUndo();
		break;
	}

	case LFUN_CUT:
		cur.recordUndo();
		cutSelection(cur, true);
		cur.message(_("Cut"));
		// Prevent stale position >= size crash
		// Probably not necessary anymore, see eraseSelection (gb 2005-10-09)
		cur.normalize();
		cur.forceBufferUpdate();
		break;

	case LFUN_MOUSE_PRESS:
		lfunMousePress(cur, cmd);
		break;

	case LFUN_MOUSE_MOTION:
		lfunMouseMotion(cur, cmd);
		break;

	case LFUN_MOUSE_RELEASE:
		lfunMouseRelease(cur, cmd);
		break;

	case LFUN_FINISHED_LEFT: // in math, left is backwards
	case LFUN_FINISHED_BACKWARD:
		cur.bv().cursor() = cur;
		break;

	case LFUN_FINISHED_RIGHT: // in math, right is forward
	case LFUN_FINISHED_FORWARD:
		++cur.pos();
		cur.bv().cursor() = cur;
		break;

	case LFUN_WORD_RIGHT:
	case LFUN_WORD_LEFT:
	case LFUN_WORD_BACKWARD:
	case LFUN_WORD_FORWARD:
	case LFUN_CHAR_RIGHT:
	case LFUN_CHAR_LEFT:
	case LFUN_CHAR_BACKWARD:
	case LFUN_CHAR_FORWARD:
		cur.screenUpdateFlags(Update::Decoration | Update::FitCursor);
		// fall through
	case LFUN_WORD_RIGHT_SELECT:
	case LFUN_WORD_LEFT_SELECT:
	case LFUN_WORD_BACKWARD_SELECT:
	case LFUN_WORD_FORWARD_SELECT:
	case LFUN_CHAR_RIGHT_SELECT:
	case LFUN_CHAR_LEFT_SELECT:
	case LFUN_CHAR_BACKWARD_SELECT:
	case LFUN_CHAR_FORWARD_SELECT: {
		// are we in a selection?
		bool select = (act == LFUN_WORD_RIGHT_SELECT
					   || act == LFUN_WORD_LEFT_SELECT
					   || act == LFUN_WORD_BACKWARD_SELECT
					   || act == LFUN_WORD_FORWARD_SELECT
		               || act == LFUN_CHAR_RIGHT_SELECT
					   || act == LFUN_CHAR_LEFT_SELECT
					   || act == LFUN_CHAR_BACKWARD_SELECT
					   || act == LFUN_CHAR_FORWARD_SELECT);
		// select words
		bool word = (act == LFUN_WORD_RIGHT_SELECT
		             || act == LFUN_WORD_LEFT_SELECT
		             || act == LFUN_WORD_BACKWARD_SELECT
		             || act == LFUN_WORD_FORWARD_SELECT
		             || act == LFUN_WORD_RIGHT
		             || act == LFUN_WORD_LEFT
		             || act == LFUN_WORD_BACKWARD
		             || act == LFUN_WORD_FORWARD);
		// are we moving forward or backwards?
		// If the command was RIGHT or LEFT, then whether we're moving forward
		// or backwards depends on the cursor movement mode (logical or visual):
		//  * in visual mode, since math is always LTR, right -> forward,
		//    left -> backwards
		//  * in logical mode, the mapping is determined by the
		//    reverseDirectionNeeded() function

		bool forward;
		FuncCode finish_lfun;

		if (act == LFUN_CHAR_FORWARD
		    || act == LFUN_CHAR_FORWARD_SELECT
		    || act == LFUN_WORD_FORWARD
		    || act == LFUN_WORD_FORWARD_SELECT) {
			forward = true;
			finish_lfun = LFUN_FINISHED_FORWARD;
		}
		else if (act == LFUN_CHAR_BACKWARD
		         || act == LFUN_CHAR_BACKWARD_SELECT
		         || act == LFUN_WORD_BACKWARD
		         || act == LFUN_WORD_BACKWARD_SELECT) {
			forward = false;
			finish_lfun = LFUN_FINISHED_BACKWARD;
		}
		else {
			bool right = (act == LFUN_CHAR_RIGHT_SELECT
						  || act == LFUN_CHAR_RIGHT
			              || act == LFUN_WORD_RIGHT_SELECT
			              || act == LFUN_WORD_RIGHT);
			if (lyxrc.visual_cursor || !cur.reverseDirectionNeeded())
				forward = right;
			else
				forward = !right;

			if (right)
				finish_lfun = LFUN_FINISHED_RIGHT;
			else
				finish_lfun = LFUN_FINISHED_LEFT;
		}
		// Now that we know exactly what we want to do, let's do it!
		cur.selHandle(select);
		cur.clearTargetX();
		cur.macroModeClose();
		// try moving forward or backwards as necessary...
		if (!(forward ? cur.mathForward(word) : cur.mathBackward(word))) {
			// ... and if movement failed, then finish forward or backwards
			// as necessary
			cmd = FuncRequest(finish_lfun);
			cur.undispatched();
		}
		break;
	}

	case LFUN_DOWN:
	case LFUN_UP:
	case LFUN_PARAGRAPH_UP:
	case LFUN_PARAGRAPH_DOWN:
		cur.screenUpdateFlags(Update::Decoration | Update::FitCursor);
		// fall through
	case LFUN_DOWN_SELECT:
	case LFUN_UP_SELECT:
	case LFUN_PARAGRAPH_UP_SELECT:
	case LFUN_PARAGRAPH_DOWN_SELECT: {
		// close active macro
		if (cur.inMacroMode()) {
			cur.macroModeClose();
			break;
		}

		// stop/start the selection
		bool select = act == LFUN_DOWN_SELECT
			|| act == LFUN_UP_SELECT
			|| act == LFUN_PARAGRAPH_DOWN_SELECT
			|| act == LFUN_PARAGRAPH_UP_SELECT;
		cur.selHandle(select);

		// go up/down
		bool up = act == LFUN_UP || act == LFUN_UP_SELECT
			|| act == LFUN_PARAGRAPH_UP || act == LFUN_PARAGRAPH_UP_SELECT;
		bool successful = cur.upDownInMath(up);
		if (successful)
			break;

		if (cur.fixIfBroken())
			// FIXME: Something bad happened. We pass the corrected Cursor
			// instead of letting things go worse.
			break;

		// We did not manage to move the cursor.
		cur.undispatched();
		break;
	}

	case LFUN_MOUSE_DOUBLE:
	case LFUN_WORD_SELECT:
		cur.pos() = 0;
		cur.bv().mouseSetCursor(cur);
		cur.pos() = cur.lastpos();
		cur.bv().mouseSetCursor(cur, true);
		break;

	case LFUN_MOUSE_TRIPLE:
		cur.idx() = 0;
		cur.pos() = 0;
		cur.bv().mouseSetCursor(cur);
		cur.idx() = cur.lastidx();
		cur.pos() = cur.lastpos();
		cur.bv().mouseSetCursor(cur, true);
		break;

	case LFUN_LINE_BEGIN:
		cur.screenUpdateFlags(Update::Decoration | Update::FitCursor);
		// fall through
	case LFUN_LINE_BEGIN_SELECT:
		cur.selHandle(act == LFUN_WORD_BACKWARD_SELECT ||
				act == LFUN_WORD_LEFT_SELECT ||
				act == LFUN_LINE_BEGIN_SELECT);
		cur.macroModeClose();
		if (cur.pos() != 0) {
			cur.pos() = 0;
		} else if (cur.col() != 0) {
			cur.idx() -= cur.col();
			cur.pos() = 0;
		} else if (cur.idx() != 0) {
			cur.idx() = 0;
			cur.pos() = 0;
		} else {
			cmd = FuncRequest(LFUN_FINISHED_BACKWARD);
			cur.undispatched();
		}
		break;

	case LFUN_LINE_END:
		cur.screenUpdateFlags(Update::Decoration | Update::FitCursor);
		// fall through
	case LFUN_LINE_END_SELECT:
		cur.selHandle(act == LFUN_WORD_FORWARD_SELECT ||
				act == LFUN_WORD_RIGHT_SELECT ||
				act == LFUN_LINE_END_SELECT);
		cur.macroModeClose();
		cur.clearTargetX();
		if (cur.pos() != cur.lastpos()) {
			cur.pos() = cur.lastpos();
		} else if (ncols() && (cur.col() != cur.lastcol())) {
			cur.idx() = cur.idx() - cur.col() + cur.lastcol();
			cur.pos() = cur.lastpos();
		} else if (cur.idx() != cur.lastidx()) {
			cur.idx() = cur.lastidx();
			cur.pos() = cur.lastpos();
		} else {
			cmd = FuncRequest(LFUN_FINISHED_FORWARD);
			cur.undispatched();
		}
		break;

	case LFUN_CELL_FORWARD:
		cur.screenUpdateFlags(Update::Decoration | Update::FitCursor);
		cur.selHandle(false);
		cur.clearTargetX();
		cur.macroModeClose();
		if (!cur.inset().idxNext(cur)) {
			if (cur.lastidx() == 0)
				cur.popForward();
			else {
				cur.idx() = firstIdx();
				cur.pos() = 0;
			}
		}
		break;

	case LFUN_CELL_BACKWARD:
		cur.screenUpdateFlags(Update::Decoration | Update::FitCursor);
		cur.selHandle(false);
		cur.clearTargetX();
		cur.macroModeClose();
		if (!cur.inset().idxPrev(cur)) {
			if (cur.lastidx() == 0)
				cur.popBackward();
			else {
				cur.idx() = cur.lastidx();
				cur.pos() = lyxrc.mac_like_cursor_movement ? cur.lastpos() : 0;
			}
		}
		break;

	case LFUN_WORD_DELETE_BACKWARD:
	case LFUN_CHAR_DELETE_BACKWARD:
		if (cur.pos() == 0)
			// May affect external cell:
			cur.recordUndoInset();
		else if (!cur.inMacroMode())
			cur.recordUndoSelection();
		// if the inset can not be removed from within, delete it
		if (!cur.backspace(cmd.getArg(0) != "confirm")) {
			FuncRequest newcmd = FuncRequest(LFUN_CHAR_DELETE_FORWARD);
			cur.innerText()->dispatch(cur, newcmd);
		}
		break;

	case LFUN_WORD_DELETE_FORWARD:
	case LFUN_CHAR_DELETE_FORWARD:
		if (cur.pos() == cur.lastpos())
			// May affect external cell:
			cur.recordUndoInset();
		else
			cur.recordUndoSelection();
		// if the inset can not be removed from within, delete it
		if (!cur.erase(cmd.getArg(0) != "confirm")) {
			FuncRequest newcmd = FuncRequest(LFUN_CHAR_DELETE_FORWARD);
			cur.innerText()->dispatch(cur, newcmd);
		}
		break;

	case LFUN_ESCAPE:
		if (cur.selection())
			cur.clearSelection();
		else  {
			if (cur.inMacroMode())
				cur.macroModeClose(true);
			else {
				cmd = FuncRequest(LFUN_FINISHED_FORWARD);
				cur.undispatched();
			}
		}
		break;

#if 0
// FIXME: resurrect this later
	// 'Locks' the math inset. A 'locked' math inset behaves as a unit
	// that is traversed by a single <CursorLeft>/<CursorRight>.
	case LFUN_INSET_TOGGLE:
		cur.recordUndo();
		lock(!lock());
		cur.popForward();
		break;
#endif

	case LFUN_SELF_INSERT:
		// special case first for big delimiters
		if (cmd.argument().size() != 1 && interpretString(cur, cmd.argument()))
			break;

		for (char_type c : cmd.argument()) {
			// Don't record undo steps if we are in macro mode and thus
			// cmd.argument is the next character of the macro name.
			// Otherwise we'll get an invalid cursor if we undo after
			// the macro was finished and the macro is a known command,
			// e.g. sqrt. Cursor::macroModeClose replaces in this case
			// the InsetMathUnknown with name "frac" by an empty
			// InsetMathFrac -> a pos value > 0 is invalid.
			// A side effect is that an undo before the macro is finished
			// undoes the complete macro, not only the last character.
			// At the time we hit '\' we are not in macro mode, still.
			if (!cur.inMacroMode())
				cur.recordUndoSelection();

			// special handling of space. If we insert an inset
			// via macro mode, we want to put the cursor inside it
			// if relevant. Think typing "\frac<space>".
			if (c == ' '
			    && cur.inMacroMode() && cur.macroName() != "\\"
			    && cur.macroModeClose() && cur.pos() > 0)
				cur.editInsertedInset();
			else if (!interpretChar(cur, c)) {
				cmd = FuncRequest(LFUN_FINISHED_FORWARD);
				cur.undispatched();
				// FIXME: can we avoid skipping the end of the string?
				break;
			}
		}
		break;

	//case LFUN_SERVER_GET_XY:
	//	break;

	case LFUN_SERVER_SET_XY: {
		lyxerr << "LFUN_SERVER_SET_XY broken!" << endl;
		int x = 0;
		int y = 0;
		istringstream is(to_utf8(cmd.argument()));
		is >> x >> y;
		cur.setTargetX(x);
		break;
	}

	// Special casing for superscript in case of LyX handling
	// dead-keys:
	case LFUN_ACCENT_CIRCUMFLEX:
		if (cmd.argument().empty()) {
			// do superscript if LyX handles
			// deadkeys
			cur.recordUndoInset();
			script(cur, true, grabAndEraseSelection(cur));
		}
		break;

	case LFUN_ACCENT_UMLAUT:
	case LFUN_ACCENT_ACUTE:
	case LFUN_ACCENT_GRAVE:
	case LFUN_ACCENT_BREVE:
	case LFUN_ACCENT_DOT:
	case LFUN_ACCENT_MACRON:
	case LFUN_ACCENT_CARON:
	case LFUN_ACCENT_TILDE:
	case LFUN_ACCENT_CEDILLA:
	case LFUN_ACCENT_CIRCLE:
	case LFUN_ACCENT_UNDERDOT:
	case LFUN_ACCENT_TIE:
	case LFUN_ACCENT_OGONEK:
	case LFUN_ACCENT_HUNGARIAN_UMLAUT:
		break;

	//  Math fonts
	case LFUN_TEXTSTYLE_APPLY:
	case LFUN_TEXTSTYLE_UPDATE:
		handleFont2(cur, cmd.argument());
		break;

	case LFUN_FONT_BOLD:
		if (currentMode() != MATH_MODE)
			handleFont(cur, cmd.argument(), "textbf");
		else
			handleFont(cur, cmd.argument(), "mathbf");
		break;
	case LFUN_FONT_BOLDSYMBOL:
		if (currentMode() != MATH_MODE)
			handleFont(cur, cmd.argument(), "textbf");
		else
			handleFont(cur, cmd.argument(), "boldsymbol");
		break;
	case LFUN_FONT_SANS:
		if (currentMode() != MATH_MODE)
			handleFont(cur, cmd.argument(), "textsf");
		else
			handleFont(cur, cmd.argument(), "mathsf");
		break;
	case LFUN_FONT_EMPH:
		if (currentMode() != MATH_MODE)
			handleFont(cur, cmd.argument(), "emph");
		else
			handleFont(cur, cmd.argument(), "mathcal");
		break;
	case LFUN_FONT_ROMAN:
		if (currentMode() != MATH_MODE)
			handleFont(cur, cmd.argument(), "textrm");
		else
			handleFont(cur, cmd.argument(), "mathrm");
		break;
	case LFUN_FONT_TYPEWRITER:
		if (currentMode() != MATH_MODE)
			handleFont(cur, cmd.argument(), "texttt");
		else
			handleFont(cur, cmd.argument(), "mathtt");
		break;
	case LFUN_FONT_FRAK:
		handleFont(cur, cmd.argument(), "mathfrak");
		break;
	case LFUN_FONT_ITAL:
		if (currentMode() != MATH_MODE)
			handleFont(cur, cmd.argument(), "textit");
		else
			handleFont(cur, cmd.argument(), "mathit");
		break;
	case LFUN_FONT_NOUN:
		if (currentMode() != MATH_MODE)
			// FIXME: should be "noun"
			handleFont(cur, cmd.argument(), "textsc");
		else
			handleFont(cur, cmd.argument(), "mathbb");
		break;
	case LFUN_FONT_DEFAULT:
		handleFont(cur, cmd.argument(), "textnormal");
		break;
	case LFUN_FONT_UNDERLINE:
		cur.recordUndo();
		cur.handleNest(createInsetMath("underline", cur.buffer()));
		break;

	case LFUN_MATH_MODE: {
#if 1
		// ignore math-mode on when already in math mode
		if (currentMode() == Inset::MATH_MODE && cmd.argument() == "on")
			break;
		cur.recordUndoSelection();
		cur.macroModeClose();
		docstring const save_selection = grabAndEraseSelection(cur);
		selClearOrDel(cur);
		if (currentMode() != Inset::MATH_MODE)
			cur.plainInsert(MathAtom(new InsetMathEnsureMath(buffer_)));
		else
			cur.plainInsert(createInsetMath("text", buffer_));
		cur.posBackward();
		cur.pushBackward(*cur.nextInset());
		cur.niceInsert(save_selection);
		cur.forceBufferUpdate();
#else
		if (currentMode() == Inset::TEXT_MODE) {
			cur.recordUndoSelection();
			cur.niceInsert(MathAtom(new InsetMathHull("simple", cur.buffer())));
			cur.message(_("create new math text environment ($...$)"));
		} else {
			handleFont(cur, cmd.argument(), "textrm");
			cur.message(_("entered math text mode (textrm)"));
		}
#endif
		cur.bv().inputMethod()->toggleInputMethodAcceptance();
		break;
	}

	case LFUN_REGEXP_MODE: {
		InsetMath * im = cur.inset().asInsetMath();
		if (im) {
			InsetMathHull * i = im->asHullInset();
			if (i && i->getType() == hullRegexp) {
				cur.message(_("Already in regular expression mode"));
				break;
			}
		}
		cur.macroModeClose();
		docstring const save_selection = grabAndEraseSelection(cur);
		selClearOrDel(cur);
		cur.plainInsert(MathAtom(new InsetMathHull(buffer_, hullRegexp)));
		cur.posBackward();
		cur.pushBackward(*cur.nextInset());
		cur.niceInsert(save_selection);
		cur.message(_("Regular expression editor mode"));
		break;
	}

	case LFUN_MATH_FONT_STYLE: {
		FuncRequest fr = FuncRequest(LFUN_MATH_INSERT, '\\' + cmd.argument());
		doDispatch(cur, fr);
		break;
	}

	case LFUN_MATH_SIZE: {
		FuncRequest fr = FuncRequest(LFUN_MATH_INSERT, cmd.argument());
		doDispatch(cur, fr);
		break;
	}

	case LFUN_MATH_MATRIX: {
		cur.recordUndo();
		unsigned int m = 1;
		unsigned int n = 1;
		docstring v_align;
		docstring h_align;
		idocstringstream is(cmd.argument());
		is >> m >> n >> v_align >> h_align;
		if (m < 1)
			m = 1;
		if (n < 1)
			n = 1;
		v_align += 'c';
		cur.niceInsert(MathAtom(new InsetMathArray(buffer_,
			from_ascii("array"), m, n, (char)v_align[0], h_align)));
		break;
	}

	case LFUN_MATH_AMS_MATRIX: {
		cur.recordUndo();
		unsigned int m = 1;
		unsigned int n = 1;
		docstring name = from_ascii("matrix");
		idocstringstream is(cmd.argument());
		is >> m >> n >> name;
		if (m < 1)
			m = 1;
		if (n < 1)
			n = 1;
		// check if we have a valid decoration
		if (name != "pmatrix" && name != "bmatrix"
			&& name != "Bmatrix" && name != "vmatrix"
			&& name != "Vmatrix" && name != "matrix"
			&& name != "smallmatrix")
			name = from_ascii("matrix");

		cur.niceInsert(
			MathAtom(new InsetMathAMSArray(buffer_, name, m, n)));
		break;
	}

	case LFUN_MATH_DELIM: {
		docstring ls;
		docstring rs = split(cmd.argument(), ls, ' ');
		// Reasonable default values
		if (ls.empty())
			ls = '(';
		if (rs.empty())
			rs = ')';
		cur.recordUndo();
		cur.handleNest(MathAtom(new InsetMathDelim(buffer_, ls, rs)));
		break;
	}

	case LFUN_MATH_BIGDELIM: {
		docstring const lname  = from_utf8(cmd.getArg(0));
		docstring const ldelim = from_utf8(cmd.getArg(1));
		docstring const rname  = from_utf8(cmd.getArg(2));
		docstring const rdelim = from_utf8(cmd.getArg(3));
		latexkeys const * l = in_word_set(lname);
		bool const have_l = l && l->inset == "big" &&
				    InsetMathBig::isBigInsetDelim(ldelim);
		l = in_word_set(rname);
		bool const have_r = l && l->inset == "big" &&
				    InsetMathBig::isBigInsetDelim(rdelim);
		// We mimic LFUN_MATH_DELIM in case we have an empty left
		// or right delimiter.
		if (have_l || have_r) {
			cur.recordUndo();
			docstring const selection = grabAndEraseSelection(cur);
			selClearOrDel(cur);
			if (have_l)
				cur.insert(MathAtom(new InsetMathBig(buffer_, lname, ldelim)));
			// first insert the right delimiter and then go back
			// and re-insert the selection (bug 7088)
			if (have_r) {
				cur.insert(MathAtom(new InsetMathBig(buffer_, rname, rdelim)));
				cur.posBackward();
			}
			cur.niceInsert(selection);
		}
		// Don't call cur.undispatched() if we did nothing, this would
		// lead to infinite recursion via Text::dispatch().
		break;
	}

	case LFUN_SPACE_INSERT: {
		cur.recordUndoSelection();
		string const name = cmd.getArg(0);
		if (name == "normal")
			cur.insert(MathAtom(new InsetMathSpace(buffer_, " ", "")));
		else if (name == "protected")
			cur.insert(MathAtom(new InsetMathSpace(buffer_, "~", "")));
		else if (name == "thin" || name == "med" || name == "thick")
			cur.insert(MathAtom(new InsetMathSpace(buffer_, name + "space", "")));
		else if (name == "hfill*")
			cur.insert(MathAtom(new InsetMathSpace(buffer_, "hspace*{\\fill}", "")));
		else if (name == "quad" || name == "qquad" ||
		         name == "enspace" || name == "enskip" ||
		         name == "negthinspace" || name == "negmedspace" ||
		         name == "negthickspace" || name == "hfill")
			cur.insert(MathAtom(new InsetMathSpace(buffer_, name, "")));
		else if (name == "hspace" || name == "hspace*") {
			string const len = cmd.getArg(1);
			if (len.empty() || !isValidLength(len)) {
				lyxerr << "LyX function 'space-insert " << name << "' "
				          "needs a valid length argument." << endl;
				break;
			}
			cur.insert(MathAtom(new InsetMathSpace(buffer_, name, len)));
		} else
			cur.insert(MathAtom(new InsetMathSpace(buffer_)));
		break;
	}

	case LFUN_MATH_SPACE:
		cur.recordUndoSelection();
		if (cmd.argument().empty())
			cur.insert(MathAtom(new InsetMathSpace(buffer_)));
		else {
			string const name = cmd.getArg(0);
			string const len = cmd.getArg(1);
			cur.insert(MathAtom(new InsetMathSpace(buffer_, name, len)));
		}
		break;

	case LFUN_ERT_INSERT:
		// interpret this as if a backslash was typed
		cur.recordUndo();
		interpretChar(cur, '\\');
		break;

	case LFUN_MATH_SUBSCRIPT:
		// interpret this as if a _ was typed
		cur.recordUndoSelection();
		interpretChar(cur, '_');
		break;

	case LFUN_MATH_SUPERSCRIPT:
		// interpret this as if a ^ was typed
		cur.recordUndoSelection();
		interpretChar(cur, '^');
		break;

	case LFUN_MATH_MACRO_FOLD:
	case LFUN_MATH_MACRO_UNFOLD: {
		Cursor it = cur;
		bool fold = act == LFUN_MATH_MACRO_FOLD;
		bool found = findMacroToFoldUnfold(it, fold);
		if (found) {
			InsetMathMacro * macro = it.nextInset()->asInsetMath()->asMacroInset();
			cur.recordUndoInset();
			if (fold)
				macro->fold(cur);
			else
				macro->unfold(cur);
		}
		break;
	}

	case LFUN_QUOTE_INSERT:
		// interpret this as if a straight " was typed
		cur.recordUndoSelection();
		interpretChar(cur, '\"');
		break;

// FIXME: We probably should swap parts of "math-insert" and "self-insert"
// handling such that "self-insert" works on "arbitrary stuff" too, and
// math-insert only handles special math things like "matrix".
	case LFUN_MATH_INSERT: {
		cur.recordUndoSelection();
		if (cmd.argument() == "^" || cmd.argument() == "_")
			interpretChar(cur, cmd.argument()[0]);
		else if (!cur.selection())
			cur.niceInsert(cmd.argument());
		else {
			MathData md(cur.buffer());
			asMathData(cmd.argument(), md);
			if (md.size() == 1
			    && md[0]->asNestInset()
			    && md[0]->asNestInset()->nargs() > 1)
				handleNest(cur, md[0]);
			else
				cur.niceInsert(cmd.argument());
		}
		break;
	}

	case LFUN_DIALOG_SHOW_NEW_INSET: {
		docstring const & name = cmd.argument();
		string data;
		if (name == "ref") {
			InsetMathRef tmp(buffer_, name);
			data = tmp.createDialogStr();
			cur.bv().showDialog(to_utf8(name), data);
		} else if (name == "mathspace") {
			cur.bv().showDialog(to_utf8(name));
		}
		break;
	}

	case LFUN_INSET_INSERT: {
		MathData md(buffer_);
		if (createInsetMath_fromDialogStr(cmd.argument(), md)) {
			cur.recordUndoSelection();
			cur.insert(md);
			cur.forceBufferUpdate();
		} else
			cur.undispatched();
		break;
	}
	case LFUN_INSET_DISSOLVE:
		if (cmd.argument().empty() && !asHullInset() && !asMacroTemplateInset()) {
			// we have been triggered via the AtPoint mechanism
			if (cur.nextInset() == this)
				cur.push(*this);
			cur.recordUndoInset();
			cur.pullArg();
		}
		break;

	case LFUN_MATH_LIMITS: {
		InsetMath * in = 0;
		if (cur.pos() < cur.lastpos() && cur.nextMath().allowsLimitsChange())
			in = &cur.nextMath();
		else if (cur.pos() > 0 && cur.prevMath().allowsLimitsChange())
			in = &cur.prevMath();
		else if (cur.lastpos() > 0 && cur.cell().back()->allowsLimitsChange())
			in = cur.cell().back().nucleus();
		// only when nucleus allows this
		if (!in)
			return;
		cur.recordUndoInset();
		if (!cmd.argument().empty()) {
			if (cmd.argument() == "limits")
				in->limits(LIMITS);
			else if (cmd.argument() == "nolimits")
				in->limits(NO_LIMITS);
			else
				in->limits(AUTO_LIMITS);
		} else if (in->limits() != AUTO_LIMITS)
			in->limits(AUTO_LIMITS);
		else if (in->defaultLimits(cur.cell().displayStyle()) == LIMITS)
			in->limits(NO_LIMITS);
		else
			in->limits(LIMITS);
		return;
	}

	case LFUN_PHANTOM_INSERT: {
		docstring const & arg = cmd.argument();
		docstring newarg;
		if (arg == "Phantom")
			newarg = from_ascii("\\phantom");
		else if (arg == "HPhantom")
			newarg = from_ascii("\\hphantom");
		else if (arg == "VPhantom")
			newarg = from_ascii("\\vphantom");
		if (newarg.empty())
			LYXERR0("Unknown phantom type " + newarg);
		else {
			FuncRequest const newfunc(LFUN_MATH_INSERT, newarg);
			lyx::dispatch(newfunc);
		}
		break;
	}

	default:
		InsetMath::doDispatch(cur, cmd);
		break;
	}
}


bool InsetMathNest::findMacroToFoldUnfold(Cursor & it, bool fold) const {
	// look for macro to open/close, but stay in mathed
	for (; !it.empty(); it.pop_back()) {

		// go backward through the current cell
		Inset * inset = it.nextInset();
		while (inset && inset->asInsetMath()) {
			InsetMathMacro * macro = inset->asInsetMath()->asMacroInset();
			if (macro) {
				// found the an macro to open/close?
				if (macro->folded() != fold)
					return true;

				// Wrong folding state.
				// If this was the first we see in this slice, look further left,
				// otherwise go up.
				if (inset != it.nextInset())
					break;
			}

			// go up if this was the left most position
			if (it.pos() == 0)
				break;

			// go left
			it.pos()--;
			inset = it.nextInset();
		}
	}

	return false;
}


bool InsetMathNest::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	// the font related toggles
	//string tc = "mathnormal";
	bool ret = true;
	string const arg = to_utf8(cmd.argument());
	switch (cmd.action()) {
#if 0
	case LFUN_INSET_MODIFY:
		// FIXME: check temporarily disabled
		// valign code
		char align = mathcursor::valign();
		if (align == '\0') {
			enable = false;
			break;
		}
		if (cmd.argument().empty()) {
			flag.clear();
			break;
		}
		if (!contains("tcb", cmd.argument()[0])) {
			enable = false;
			break;
		}
		flag.setOnOff(cmd.argument()[0] == align);
		break;
#endif
	/// We have to handle them since 1.4 blocks all unhandled actions
	case LFUN_FONT_ITAL:
	case LFUN_FONT_BOLD:
	case LFUN_FONT_BOLDSYMBOL:
	case LFUN_FONT_SANS:
	case LFUN_FONT_EMPH:
	case LFUN_FONT_TYPEWRITER:
	case LFUN_FONT_NOUN:
	case LFUN_FONT_ROMAN:
	case LFUN_FONT_DEFAULT:
		flag.setEnabled(true);
		break;

	// we just need to be in math mode to enable that
	case LFUN_MATH_SIZE:
	case LFUN_MATH_SPACE:
	case LFUN_MATH_EXTERN:
		flag.setEnabled(true);
		break;

	case LFUN_FONT_UNDERLINE:
	case LFUN_FONT_FRAK:
		flag.setEnabled(currentMode() != TEXT_MODE);
		break;

	case LFUN_MATH_FONT_STYLE: {
		bool const textarg =
			arg == "textbf"   || arg == "textsf" ||
			arg == "textrm"   || arg == "textmd" ||
			arg == "textit"   || arg == "textsc" ||
			arg == "textsl"   || arg == "textup" ||
			arg == "texttt"   || arg == "textbb" ||
			arg == "textnormal";
		flag.setEnabled(currentMode() != TEXT_MODE || textarg);
		break;
	}

	case LFUN_MATH_MODE:
		// forbid "math-mode on" in math mode to prevent irritating
		// behaviour of menu entries (bug 6709)
		flag.setEnabled(currentMode() == TEXT_MODE || arg != "on");
		break;

	case LFUN_MATH_INSERT:
		flag.setEnabled(currentMode() != TEXT_MODE);
		break;

	case LFUN_MATH_AMS_MATRIX:
	case LFUN_MATH_MATRIX:
		flag.setEnabled(currentMode() == MATH_MODE);
		break;

	case LFUN_INSET_INSERT: {
		// Don't test createMathInset_fromDialogStr(), since
		// getStatus is not called with a valid reference and the
		// dialog would not be applicable.
		string const name = cmd.getArg(0);
		flag.setEnabled(name == "ref" || name == "mathspace");
		break;
	}

	case LFUN_DIALOG_SHOW_NEW_INSET: {
		docstring const & name = cmd.argument();
		flag.setEnabled(name == "ref" || name == "mathspace");
		break;
	}


	case LFUN_MATH_DELIM:
	case LFUN_MATH_BIGDELIM:
		// Don't do this with multi-cell selections
		flag.setEnabled(cur.selBegin().idx() == cur.selEnd().idx());
		break;

	case LFUN_MATH_MACRO_FOLD:
	case LFUN_MATH_MACRO_UNFOLD: {
		Cursor it = cur;
		bool found = findMacroToFoldUnfold(it, cmd.action() == LFUN_MATH_MACRO_FOLD);
		flag.setEnabled(found);
		break;
	}

	case LFUN_SPECIALCHAR_INSERT:
	case LFUN_SCRIPT_INSERT:
		// FIXME: These would probably make sense in math-text mode
		flag.setEnabled(false);
		break;

	// these are nonfunctional in math
	case LFUN_BOX_INSERT:
	case LFUN_BRANCH_INSERT:
	case LFUN_BRANCH_ADD_INSERT:
	case LFUN_CAPTION_INSERT:
	case LFUN_FLEX_INSERT:
	case LFUN_FLOAT_INSERT:
	case LFUN_FLOAT_LIST_INSERT:
	case LFUN_FOOTNOTE_INSERT:
	case LFUN_HREF_INSERT:
	case LFUN_INDEX_INSERT:
	case LFUN_INDEX_PRINT:
	case LFUN_INFO_INSERT:
	case LFUN_IPA_INSERT:
	case LFUN_LISTING_INSERT:
	case LFUN_MARGINALNOTE_INSERT:
	case LFUN_NEWPAGE_INSERT:
	case LFUN_NOMENCL_INSERT:
	case LFUN_NOMENCL_PRINT:
	case LFUN_NOTE_INSERT:
	case LFUN_PREVIEW_INSERT:
	case LFUN_TABULAR_INSERT:
	case LFUN_WRAP_INSERT:
		flag.setEnabled(false);
		break;

	case LFUN_SPACE_INSERT: {
		docstring const & name = cmd.argument();
		if (name == "visible")
			flag.setEnabled(false);
		break;
	}

	case LFUN_INSET_DISSOLVE:
		flag.setEnabled(cmd.argument().empty() && !asHullInset() && !asMacroTemplateInset());
		break;

	case LFUN_PASTE: {
		docstring const & name = cmd.argument();
		if (name == "html" || name == "latex")
			flag.setEnabled(false);
		break;
	}

	case LFUN_MATH_LIMITS: {
		InsetMath * in = 0;
		if (cur.pos() < cur.lastpos() && cur.nextMath().allowsLimitsChange())
			in = &cur.nextMath();
		else if (cur.pos() > 0 && cur.prevMath().allowsLimitsChange())
			in = &cur.prevMath();
		else if (cur.lastpos() > 0 && cur.cell().back()->allowsLimitsChange())
			in = cur.cell().back().nucleus();
		if (in) {
			if (!cmd.argument().empty()) {
				if (cmd.argument() == "limits")
					flag.setOnOff(in->limits() == LIMITS);
				else if (cmd.argument() == "nolimits")
					flag.setOnOff(in->limits() == NO_LIMITS);
				else
					flag.setOnOff(in->limits() == AUTO_LIMITS);
			}
			flag.setEnabled(true);
		} else
			flag.setEnabled(false);
		return true;
	}

	default:
		ret = false;
		break;
	}
	return ret;
}


void InsetMathNest::edit(Cursor & cur, bool front, EntryDirection entry_from)
{
	cur.push(*this);
	bool enter_front = (entry_from == Inset::ENTRY_DIRECTION_LEFT ||
		(entry_from == Inset::ENTRY_DIRECTION_IGNORE && front));
	enter_front ? idxFirst(cur) : idxLast(cur);
	cur.resetAnchor();
	//lyxerr << "InsetMathNest::edit, cur:\n" << cur << endl;
}


Inset * InsetMathNest::editXY(Cursor & cur, int x, int y)
{
	int idx_min = -1;
	int dist_min = 1000000;
	for (idx_type i = 0, n = nargs(); i != n; ++i) {
		int const d = cell(i).dist(cur.bv(), x, y);
		if (d < dist_min) {
			dist_min = d;
			idx_min = i;
		}
	}
	if (idx_min == -1)
		return this;

	MathData & md = cell(idx_min);
	cur.push(*this);
	cur.idx() = idx_min;
	cur.pos() = md.x2pos(&cur.bv(), x - md.xo(cur.bv()));

	//lyxerr << "found cell : " << idx_min << " pos: " << cur.pos() << endl;
	if (dist_min == 0) {
		// hit inside cell
		for (pos_type i = 0, n = md.size(); i < n; ++i)
			if (md[i]->covers(cur.bv(), x, y))
				return md[i].nucleus()->editXY(cur, x, y);
	}
	return this;
}


void InsetMathNest::lfunMousePress(Cursor & cur, FuncRequest & cmd)
{
	//lyxerr << "## lfunMousePress: buttons: " << cmd.button() << endl;
	BufferView & bv = cur.bv();
	if (cmd.button() == mouse_button::button3) {
		// Don't do anything if we right-click a
		// selection, a context menu will popup.
		if (bv.cursor().selection() && cur >= bv.cursor().selectionBegin()
		      && cur < bv.cursor().selectionEnd()) {
	 		cur.noScreenUpdate();
			return;
		}
	}

	// set cursor after the inset if x is nearer to that position (bug 9748)
	cur.moveToClosestEdge(cmd.x(), true);

	bool do_selection = cmd.button() == mouse_button::button1
		&& cmd.modifier() == ShiftModifier;
	bv.mouseSetCursor(cur, do_selection);
	if (cmd.button() == mouse_button::button1) {
		//lyxerr << "## lfunMousePress: setting cursor to: " << cur << endl;
		// Update the cursor update flags as needed:
		//
		// Update::Decoration: tells to update the decoration
		//                     (visual box corners that define
		//                     the inset)/
		// Update::FitCursor: adjust the screen to the cursor
		//                    position if needed
		// cur.result().update(): don't overwrite previously set flags.
		cur.screenUpdateFlags(Update::Decoration | Update::FitCursor
				| cur.result().screenUpdate());
	} else if (cmd.button() == mouse_button::button2 && lyxrc.mouse_middlebutton_paste) {
		if (cap::selection()) {
			// See comment in Text::dispatch why we do this
			cap::copySelectionToStack();
			cmd = FuncRequest(LFUN_PASTE, "0");
			doDispatch(bv.cursor(), cmd);
		} else {
			MathData md(buffer_);
			asMathData(theSelection().get(), md);
			bv.cursor().insert(md);
		}
	}
}


void InsetMathNest::lfunMouseMotion(Cursor & cur, FuncRequest & cmd)
{
	// only select with button 1
	if (cmd.button() != mouse_button::button1)
		return;

	Cursor & bvcur = cur.bv().cursor();

	// ignore motions deeper nested than the real anchor
	if (!bvcur.realAnchor().hasPart(cur)) {
		cur.undispatched();
		return;
	}

	// set cursor after the inset if x is nearer to that position (bug 9748)
	cur.moveToClosestEdge(cmd.x());

	CursorSlice old = bvcur.top();

	// We continue with our existing selection or start a new one, so don't
	// reset the anchor.
	bvcur.setCursor(cur);
	// Did we actually move?
	if (cur.top() == old)
		// We didn't move one iota, so no need to change selection status
		// or update the screen.
		cur.screenUpdateFlags(Update::SinglePar | Update::FitCursor);
	else
		bvcur.setSelection();
}


void InsetMathNest::lfunMouseRelease(Cursor & cur, FuncRequest & cmd)
{
	//lyxerr << "## lfunMouseRelease: buttons: " << cmd.button() << endl;

	if (cmd.button() == mouse_button::button1) {
		if (!cur.selection())
			cur.noScreenUpdate();
		else {
			Cursor & bvcur = cur.bv().cursor();
			bvcur.selection(true);
		}
		return;
	}

	cur.undispatched();
}


bool InsetMathNest::interpretChar(Cursor & cur, char_type const c)
{
	// try auto-correction
	if (lyxrc.autocorrection_math && cur.pos() != 0 && !cur.selection()
	     && math_autocorrect(cur, c))
		return true;

	//lyxerr << "interpret 2: '" << c << "'" << endl;
	docstring save_selection;
	if (c == '^' || c == '_')
		save_selection = grabAndEraseSelection(cur);

	cur.clearTargetX();
	Buffer * buf = cur.buffer();

	// handle macroMode
	if (cur.inMacroMode()) {
		docstring name = cur.macroName();

		/// are we currently typing '#1' or '#2' or...?
		if (name == "\\#") {
			cur.backspace();
			int n = c - '0';
			if (n >= 1 && n <= 9)
				cur.insert(new InsetMathMacroArgument(buffer_, n));
			return true;
		}

		// do not finish macro for known * commands
		bool star_macro = c == '*'
			&& (in_word_set(name.substr(1) + '*')
			    || cur.buffer()->getMacro(name.substr(1) + "*", cur, true));
		if (isAlphaASCII(c) || star_macro) {
			cur.activeMacro()->setName(name + docstring(1, c));
			return true;
		}

		// handle 'special char' macros
		if (name == "\\") {
			// remove the '\\'
			if (c == '\\') {
				cur.backspace();
				if (currentMode() != InsetMath::MATH_MODE)
					cur.niceInsert(createInsetMath("textbackslash", buf));
				else
					cur.niceInsert(createInsetMath("backslash", buf));
			} else if (c == '^' && currentMode() == InsetMath::MATH_MODE) {
				cur.backspace();
				cur.niceInsert(createInsetMath("mathcircumflex", buf));
			} else if (c == '{' || c == '%') {
				//using the saved selection as argument
				InsetMathUnknown * p = cur.activeMacro();
				p->finalize();
				MathData sel(cur.buffer());
				asMathData(p->selection(), sel);
				cur.backspace();
				if (c == '{')
					cur.niceInsert(MathAtom(new InsetMathBrace(buffer_, sel)));
				else
					cur.niceInsert(MathAtom(new InsetMathComment(sel)));
			} else if (c == '#') {
				LASSERT(cur.activeMacro(), return false);
				cur.activeMacro()->setName(name + docstring(1, c));
			} else {
				cur.backspace();
				cur.niceInsert(createInsetMath(docstring(1, c), buf));
			}
			return true;
		}

		// One character big delimiters. The others are handled in
		// interpretString().
		latexkeys const * l = in_word_set(name.substr(1));
		if (name[0] == '\\' && l && l->inset == "big") {
			docstring delim;
			switch (c) {
			case '{':
				delim = from_ascii("\\{");
				break;
			case '}':
				delim = from_ascii("\\}");
				break;
			default:
				delim = docstring(1, c);
				break;
			}
			if (InsetMathBig::isBigInsetDelim(delim)) {
				// name + delim ared a valid InsetMathBig.
				// We can't use cur.macroModeClose() because
				// it does not handle delim.
				InsetMathUnknown * p = cur.activeMacro();
				p->finalize();
				--cur.pos();
				cur.cell().erase(cur.pos());
				cur.plainInsert(MathAtom(
					new InsetMathBig(buffer_, name.substr(1), delim)));
				return true;
			}
		} else if (name == "\\smash" && c == '[') {
			// We can't use cur.macroModeClose() because
			// it would create an InsetMathPhantom
			InsetMathUnknown * p = cur.activeMacro();
			p->finalize();
			interpretChar(cur, c);
			return true;
		}

		// leave macro mode and try again if necessary
		if (cur.macroModeClose()) {
			MathAtom const atom = cur.prevAtom();
			if (atom->asNestInset() && atom->isActive()) {
				cur.posBackward();
				cur.nextInset()->edit(cur, true);
			}
		}
		if (c == '{')
			cur.niceInsert(MathAtom(new InsetMathBrace(buf)));
		else if (c != ' ')
			interpretChar(cur, c);
		return true;
	}


	// just clear selection on pressing the space bar
	if (cur.selection() && c == ' ') {
		cur.selection(false);
		return true;
	}

	if (c == '\\') {
		//lyxerr << "starting with macro" << endl;
		bool reduced = cap::reduceSelectionToOneCell(cur);
		if (reduced || !cur.selection()) {
			InsetMath const * im = cur.inset().asInsetMath();
			InsetMathMacro const * macro = im ? im->asMacroInset()
			                                  : nullptr;
			bool in_macro_name = macro
				&& macro->displayMode() ==
					InsetMathMacro::DISPLAY_UNFOLDED;
			cur.recordUndoInset();
			docstring const safe = cap::grabAndEraseSelection(cur);
			if (!cur.inRegexped() && !in_macro_name)
				cur.insert(MathAtom(new InsetMathUnknown(buffer_, from_ascii("\\"), safe, false)));
			else
				cur.niceInsert(createInsetMath("backslash", buf));
		}
		return true;
	}

	selClearOrDel(cur);

	if (c == '\n') {
		if (currentMode() != InsetMath::MATH_MODE)
			cur.insert(c);
		return true;
	}

	if (c == ' ') {
		if (currentMode() != InsetMath::MATH_MODE) {
			// insert spaces in text or undecided mode,
			// but suppress direct insertion of two spaces in a row
			// the still allows typing  '<space>a<space>' and deleting the 'a', but
			// it is better than nothing...
			pos_type const pos = cur.pos();
			pos_type const lastpos = cur.lastpos();
			if ((pos == 0 && lastpos == 0)
			    || (pos == 0 && cur.nextAtom()->getChar() != ' ')
			    || (pos == lastpos && cur.prevAtom()->getChar() != ' ')
			    || (pos > 0 && cur.prevAtom()->getChar() != ' '
					&& cur.nextAtom()->getChar() != ' ')) {
				cur.insert(c);
				// FIXME: we have to enable full redraw here because of the
				// visual box corners that define the inset. If we know for
				// sure that we stay within the same cell we can optimize for
				// that using:
				//cur.screenUpdateFlags(Update::SinglePar | Update::FitCursor);
			}
			return true;
		}
		if (cur.pos() != 0 && cur.prevAtom()->asSpaceInset()) {
			cur.prevAtom().nucleus()->asSpaceInset()->incSpace();
			// FIXME: we have to enable full redraw here because of the
			// visual box corners that define the inset. If we know for
			// sure that we stay within the same cell we can optimize for
			// that using:
			//cur.screenUpdateFlags(Update::SinglePar | Update::FitCursor);
			return true;
		}

		if (cur.popForward()) {
			// FIXME: we have to enable full redraw here because of the
			// visual box corners that define the inset. If we know for
			// sure that we stay within the same cell we can optimize for
			// that using:
			//cur.screenUpdateFlags(Update::FitCursor);
			return true;
		}

		// if we are at the very end, leave the formula
		return cur.pos() != cur.lastpos();
	}

	// These should be treated differently when not in text mode:
	if (cur.inRegexped()) {
		switch (c) {
		case '^':
			cur.niceInsert(createInsetMath("mathcircumflex", buf));
			break;
		case '{':
		case '}':
		case '#':
		case '%':
		case '_':
			cur.niceInsert(createInsetMath(docstring(1, c), buf));
			break;
		case '~':
			cur.niceInsert(createInsetMath("sim", buf));
			break;
		default:
			cur.insert(c);
		}
		return true;
	} else if (currentMode() != InsetMath::TEXT_MODE) {
		if (c == '_') {
			cur.recordUndoInset();
			script(cur, false, save_selection);
			return true;
		}
		if (c == '^') {
			cur.recordUndoInset();
			script(cur, true, save_selection);
			return true;
		}
		if (c == '~') {
			cur.niceInsert(createInsetMath("sim", buf));
			return true;
		}
		if (currentMode() == InsetMath::MATH_MODE && Encodings::isUnicodeTextOnly(c)) {
			MathAtom const atom(new InsetMathChar(buffer_, c));
			if (cur.prevInset() && cur.prevInset()->asInsetMath()->name() == "text") {
				// reuse existing \text inset
				cur.prevInset()->asInsetMath()->cell(0).push_back(atom);
			} else {
				MathAtom at = createInsetMath("text", buf);
				at.nucleus()->cell(0).push_back(atom);
				cur.insert(at);
				cur.posForward();
			}
			return true;
		}
	} else {
		if (c == '^') {
			cur.niceInsert(createInsetMath("textasciicircum", buf));
			return true;
		}
		if (c == '~') {
			cur.niceInsert(createInsetMath("textasciitilde", buf));
			return true;
		}
	}

	if (c == '{' || c == '}' || c == '&' || c == '$' || c == '#' ||
	    c == '%' || c == '_') {
		cur.niceInsert(createInsetMath(docstring(1, c), buf));
		return true;
	}

	// no special circumstances, so insert the character without any fuss
	cur.insert(c);
	return true;
}


bool InsetMathNest::interpretString(Cursor & cur, docstring const & str)
{
	if (str == "\\limits" || str == "\\nolimits") {
		if (cur.pos() > 0 && cur.prevMath().allowsLimitsChange()) {
			cur.prevMath().limits(str == "\\limits" ? LIMITS : NO_LIMITS);
			return true;
		} else {
			cur.message(bformat(_("Cannot apply %1$s here."), str));
			return false;
		}
	}
	// Create a InsetMathBig from cur.cell()[cur.pos() - 1] and t if
	// possible
	if (!cur.empty() && cur.pos() > 0 &&
	    cur.cell()[cur.pos() - 1]->asUnknownInset()) {
		if (InsetMathBig::isBigInsetDelim(str)) {
			docstring prev = asString(cur.cell()[cur.pos() - 1]);
			if (prev[0] == '\\') {
				prev = prev.substr(1);
				latexkeys const * l = in_word_set(prev);
				if (l && l->inset == "big") {
					cur.recordUndoSelection();
					cur.cell()[cur.pos() - 1] =
						MathAtom(new InsetMathBig(buffer_, prev, str));
					return true;
				}
			}
		}
	}
	return false;
}


bool InsetMathNest::script(Cursor & cur, bool up)
{
	return script(cur, up, docstring());
}


bool InsetMathNest::script(Cursor & cur, bool up,
		docstring const & save_selection)
{
	// Hack to get \^ and \_ working
	//lyxerr << "handling script: up: " << up << endl;
	if (cur.inMacroMode() && cur.macroName() == "\\") {
		if (up)
			cur.niceInsert(createInsetMath("mathcircumflex", cur.buffer()));
		else
			interpretChar(cur, '_');
		return true;
	}

	cur.macroModeClose();
	if (asScriptInset() && cur.idx() == 0) {
		// we are in a nucleus of a script inset, move to _our_ script
		InsetMathScript * inset = asScriptInset();
		//lyxerr << " going to cell " << inset->idxOfScript(up) << endl;
		inset->ensure(up);
		cur.idx() = inset->idxOfScript(up);
		cur.pos() = 0;
	} else if (cur.pos() != 0 && cur.prevAtom()->asScriptInset()) {
		--cur.pos();
		InsetMathScript * inset = cur.nextAtom().nucleus()->asScriptInset();
		cur.push(*inset);
		inset->ensure(up);
		cur.idx() = inset->idxOfScript(up);
		cur.pos() = cur.lastpos();
	} else {
		// convert the thing to our left to a scriptinset or create a new
		// one if in the very first position of the data
		if (cur.pos() == 0) {
			//lyxerr << "new scriptinset" << endl;
			cur.insert(new InsetMathScript(buffer_, up));
		} else {
			//lyxerr << "converting prev atom " << endl;
			cur.prevAtom() = MathAtom(new InsetMathScript(buffer_, cur.prevAtom(), up));
		}
		--cur.pos();
		InsetMathScript * inset = cur.nextAtom().nucleus()->asScriptInset();
		// See comment in MathParser.cpp for special handling of {}-bases

		cur.push(*inset);
		cur.idx() = 1;
		cur.pos() = 0;
	}
	//lyxerr << "inserting selection 1:\n" << save_selection << endl;
	cur.niceInsert(save_selection);
	cur.resetAnchor();
	//lyxerr << "inserting selection 2:\n" << save_selection << endl;
	return true;
}


bool InsetMathNest::completionSupported(Cursor const & cur) const
{
	return cur.inMacroMode();
}


bool InsetMathNest::inlineCompletionSupported(Cursor const & cur) const
{
	return cur.inMacroMode();
}


bool InsetMathNest::automaticInlineCompletion() const
{
	return lyxrc.completion_inline_math;
}


bool InsetMathNest::automaticPopupCompletion() const
{
	return lyxrc.completion_popup_math;
}


CompletionList const *
InsetMathNest::createCompletionList(Cursor const & cur) const
{
	if (!cur.inMacroMode())
		return nullptr;

	return new MathCompletionList(cur);
}


docstring InsetMathNest::completionPrefix(Cursor const & cur) const
{
	if (!cur.inMacroMode())
		return docstring();

	return cur.activeMacro()->name();
}


bool InsetMathNest::insertCompletion(Cursor & cur, docstring const & s, bool finished)
{
	if (cur.buffer()->isReadonly() || !cur.inMacroMode())
		return false;

	// Contrary to Text, the whole inset should be recorded (#12581).
	cur.recordUndoInset();

	// append completion to active macro
	InsetMathUnknown * inset = cur.activeMacro();
	inset->setName(inset->name() + s);

	// finish macro
	if (finished) {
#if 0
		// FIXME: this creates duplicates in the completion popup
		// which looks ugly. Moreover the changes the list lengths
		// which seems to confuse the popup as well.
		MathCompletionList::addToFavorites(inset->name());
#endif
		lyx::dispatch(FuncRequest(LFUN_SELF_INSERT, " "));
	}
	cur.screenUpdateFlags(Update::SinglePar | Update::FitCursor);

	return true;
}


void InsetMathNest::completionPosAndDim(Cursor const & cur, int & x, int & y,
					Dimension & dim) const
{
	Inset const * inset = cur.activeMacro();
	if (!inset)
		return;

	// get inset dimensions
	dim = cur.bv().coordCache().insets().dim(inset);
	// FIXME: these 3 are no accurate, but should depend on the font.
	// Now the popup jumps down if you enter a char with descent > 0.
	dim.des += 3;
	dim.asc += 3;

	// and position
	Point xy = cur.bv().coordCache().insets().xy(inset);
	x = xy.x;
	y = xy.y;
}


////////////////////////////////////////////////////////////////////

MathCompletionList::MathCompletionList(Cursor const & cur)
{
	// fill it with macros from the buffer
	MacroNameSet macros;
	cur.buffer()->listMacroNames(macros);
	MacroNameSet::const_iterator it;
	for (it = macros.begin(); it != macros.end(); ++it) {
		if (cur.buffer()->getMacro(*it, cur, false))
			locals.push_back("\\" + *it);
	}
	sort(locals.begin(), locals.end());

	if (!globals.empty())
		return;

	// fill in global macros
	macros.clear();
	MacroTable::globalMacros().getMacroNames(macros, false);
	//lyxerr << "Globals completion macros: ";
	for (it = macros.begin(); it != macros.end(); ++it) {
		//lyxerr << "\\" + *it << " ";
		globals.push_back("\\" + *it);
	}
	//lyxerr << std::endl;

	// fill in global commands
	globals.push_back(from_ascii("\\boxed"));
	globals.push_back(from_ascii("\\fbox"));
	globals.push_back(from_ascii("\\framebox"));
	globals.push_back(from_ascii("\\makebox"));
	globals.push_back(from_ascii("\\kern"));
	globals.push_back(from_ascii("\\xhookrightarrow"));
	globals.push_back(from_ascii("\\xhookleftarrow"));
	globals.push_back(from_ascii("\\xrightarrow"));
	globals.push_back(from_ascii("\\xRightarrow"));
	globals.push_back(from_ascii("\\xrightharpoondown"));
	globals.push_back(from_ascii("\\xrightharpoonup"));
	globals.push_back(from_ascii("\\xrightleftharpoons"));
	globals.push_back(from_ascii("\\xleftarrow"));
	globals.push_back(from_ascii("\\xLeftarrow"));
	globals.push_back(from_ascii("\\xleftharpoondown"));
	globals.push_back(from_ascii("\\xleftharpoonup"));
	globals.push_back(from_ascii("\\xleftrightarrow"));
	globals.push_back(from_ascii("\\xLeftrightarrow"));
	globals.push_back(from_ascii("\\xleftrightharpoons"));
	globals.push_back(from_ascii("\\xmapsto"));
	globals.push_back(from_ascii("\\split"));
	globals.push_back(from_ascii("\\gathered"));
	globals.push_back(from_ascii("\\aligned"));
	globals.push_back(from_ascii("\\alignedat"));
	globals.push_back(from_ascii("\\cases"));
	globals.push_back(from_ascii("\\substack"));
	globals.push_back(from_ascii("\\xymatrix"));
	globals.push_back(from_ascii("\\Diagram"));
	globals.push_back(from_ascii("\\subarray"));
	globals.push_back(from_ascii("\\array"));
	globals.push_back(from_ascii("\\sqrt"));
	globals.push_back(from_ascii("\\root"));
	globals.push_back(from_ascii("\\tabular"));
	globals.push_back(from_ascii("\\sideset"));
	globals.push_back(from_ascii("\\stackrel"));
	globals.push_back(from_ascii("\\stackrelthree"));
	globals.push_back(from_ascii("\\binom"));
	globals.push_back(from_ascii("\\choose"));
	globals.push_back(from_ascii("\\brace"));
	globals.push_back(from_ascii("\\brack"));
	globals.push_back(from_ascii("\\frac"));
	globals.push_back(from_ascii("\\over"));
	globals.push_back(from_ascii("\\nicefrac"));
	globals.push_back(from_ascii("\\unitfrac"));
	globals.push_back(from_ascii("\\unitfracthree"));
	globals.push_back(from_ascii("\\unitone"));
	globals.push_back(from_ascii("\\unittwo"));
	globals.push_back(from_ascii("\\infer"));
	globals.push_back(from_ascii("\\atop"));
	globals.push_back(from_ascii("\\lefteqn"));
	globals.push_back(from_ascii("\\boldsymbol"));
	globals.push_back(from_ascii("\\bm"));
	globals.push_back(from_ascii("\\color"));
	globals.push_back(from_ascii("\\normalcolor"));
	globals.push_back(from_ascii("\\textcolor"));
	globals.push_back(from_ascii("\\cfrac"));
	globals.push_back(from_ascii("\\cfracleft"));
	globals.push_back(from_ascii("\\cfracright"));
	globals.push_back(from_ascii("\\dfrac"));
	globals.push_back(from_ascii("\\tfrac"));
	globals.push_back(from_ascii("\\dbinom"));
	globals.push_back(from_ascii("\\tbinom"));
	globals.push_back(from_ascii("\\hphantom"));
	globals.push_back(from_ascii("\\phantom"));
	globals.push_back(from_ascii("\\vphantom"));
	globals.push_back(from_ascii("\\cancel"));
	globals.push_back(from_ascii("\\bcancel"));
	globals.push_back(from_ascii("\\xcancel"));
	globals.push_back(from_ascii("\\cancelto"));
	globals.push_back(from_ascii("\\smash"));
	globals.push_back(from_ascii("\\mathclap"));
	globals.push_back(from_ascii("\\mathllap"));
	globals.push_back(from_ascii("\\mathrlap"));
	globals.push_back(from_ascii("\\ensuremath"));
	MathWordList const & words = mathedWordList();
	MathWordList::const_iterator it2;
	//lyxerr << "Globals completion commands: ";
	for (it2 = words.begin(); it2 != words.end(); ++it2) {
		if (it2->second.inset != "macro" && !it2->second.hidden) {
			// macros are already read from MacroTable::globalMacros()
			globals.push_back('\\' + it2->first);
			//lyxerr << '\\' + it2->first << ' ';
		}
	}
	//lyxerr << std::endl;
	sort(globals.begin(), globals.end());
}


MathCompletionList::~MathCompletionList()
{
}


size_type MathCompletionList::size() const
{
	return locals.size() + globals.size();
}


docstring const & MathCompletionList::data(size_t idx) const
{
	size_t lsize = locals.size();
	if (idx >= lsize)
		return globals[idx - lsize];
	else
		return locals[idx];
}


std::string MathCompletionList::icon(size_t idx) const
{
	// get the latex command
	docstring cmd;
	size_t lsize = locals.size();
	if (idx >= lsize)
		cmd = globals[idx - lsize];
	else
		cmd = locals[idx];

	// get the icon name by stripping the backslash
	docstring icon_name = frontend::Application::mathIcon(cmd.substr(1));
	if (icon_name.empty())
		return std::string();
	return "math/" + to_utf8(icon_name);
}

std::vector<docstring> MathCompletionList::globals;

} // namespace lyx
