/**
 * \file MathData.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 * \author Stefan Schimanski
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "MathData.h"

#include "InsetMathBrace.h"
#include "InsetMathMacro.h"
#include "InsetMathScript.h"
#include "MacroTable.h"
#include "MathRow.h"
#include "MathStream.h"
#include "MathSupport.h"
#include "MetricsInfo.h"
#include "ReplaceData.h"

#include "Buffer.h"
#include "BufferView.h"
#include "CoordCache.h"
#include "Cursor.h"
#include "Dimension.h"

#include "mathed/InsetMathChar.h"

#include "frontends/FontMetrics.h"
#include "frontends/InputMethod.h"
#include "frontends/Painter.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/lassert.h"

#include <cstdlib>

using namespace std;

namespace lyx {


MathData::MathData(Buffer * buf, const_iterator from, const_iterator to)
	: base_type(from, to), buffer_(buf)
{
	setContentsBuffer();
}


void MathData::setContentsBuffer()
{
	if (buffer_)
		for (MathAtom & at : *this)
			at.nucleus()->setBuffer(*buffer_);
}


void MathData::setBuffer(Buffer & b)
{
	buffer_ = &b;
	setContentsBuffer();
}


MathAtom & MathData::operator[](pos_type pos)
{
	LBUFERR(pos < size());
	return base_type::operator[](pos);
}


MathAtom const & MathData::operator[](pos_type pos) const
{
	LBUFERR(pos < size());
	return base_type::operator[](pos);
}


void MathData::insert(size_type pos, MathAtom const & t)
{
	LBUFERR(pos <= size());
	base_type::insert(begin() + pos, t);
	if (buffer_)
		operator[](pos)->setBuffer(*buffer_);
}


void MathData::insert(size_type pos, MathData const & md)
{
	LBUFERR(pos <= size());
	base_type::insert(begin() + pos, md.begin(), md.end());
	if (buffer_)
		for (size_type i = 0 ; i < md.size() ; ++i)
			operator[](pos + i)->setBuffer(*buffer_);
}


void MathData::push_back(MathAtom const & t)
{
	base_type::push_back(t);
	if (buffer_)
		back()->setBuffer(*buffer_);
}


void MathData::emplace_back(InsetMath * ins)
{
	base_type::emplace_back(ins);
	if (buffer_)
		back()->setBuffer(*buffer_);
}


void MathData::append(MathData const & md)
{
	insert(size(), md);
}


void MathData::erase(size_type pos)
{
	if (pos < size())
		erase(pos, pos + 1);
}


void MathData::erase(iterator pos1, iterator pos2)
{
	base_type::erase(pos1, pos2);
}


void MathData::erase(iterator pos)
{
	base_type::erase(pos);
}


void MathData::erase(size_type pos1, size_type pos2)
{
	base_type::erase(begin() + pos1, begin() + pos2);
}


void MathData::dump2() const
{
	odocstringstream os;
	NormalStream ns(os);
	for (const_iterator it = begin(); it != end(); ++it)
		ns << *it << ' ';
	lyxerr << to_utf8(os.str());
}


void MathData::dump() const
{
	odocstringstream os;
	NormalStream ns(os);
	for (const_iterator it = begin(); it != end(); ++it)
		ns << '<' << *it << '>';
	lyxerr << to_utf8(os.str());
}


void MathData::validate(LaTeXFeatures & features) const
{
	for (const_iterator it = begin(); it != end(); ++it)
		(*it)->validate(features);
}


bool MathData::match(MathData const & md) const
{
	return size() == md.size() && matchpart(md, 0);
}


bool MathData::matchpart(MathData const & md, pos_type pos) const
{
	if (size() < md.size() + pos)
		return false;
	const_iterator it = begin() + pos;
	for (const_iterator jt = md.begin(); jt != md.end(); ++jt, ++it)
		if (asString(*it) != asString(*jt))
			return false;
	return true;
}


void MathData::replace(ReplaceData & rep)
{
	for (size_type i = 0; i < size(); ++i) {
		if (find1(rep.from, i)) {
			// match found
			lyxerr << "match found!" << endl;
			erase(i, i + rep.from.size());
			insert(i, rep.to);
		}
	}

	// FIXME: temporarily disabled
	// for (const_iterator it = begin(); it != end(); ++it)
	//	it->nucleus()->replace(rep);
}


bool MathData::find1(MathData const & md, size_type pos) const
{
	lyxerr << "finding '" << md << "' in '" << *this << "'" << endl;
	for (size_type i = 0, n = md.size(); i < n; ++i)
		if (asString(operator[](pos + i)) != asString(md[i]))
			return false;
	return true;
}


MathData::size_type MathData::find(MathData const & md) const
{
	for (int i = 0, last = size() - md.size(); i < last; ++i)
		if (find1(md, i))
			return i;
	return size();
}


MathData::size_type MathData::find_last(MathData const & md) const
{
	for (int i = size() - md.size(); i >= 0; --i)
		if (find1(md, i))
			return i;
	return size();
}


bool MathData::contains(MathData const & md) const
{
	if (find(md) != size())
		return true;
	for (const_iterator it = begin(); it != end(); ++it)
		if ((*it)->contains(md))
			return true;
	return false;
}


bool MathData::addToMathRow(MathRow & mrow, MetricsInfo & mi) const
{
	bool has_contents = false;
	BufferView * bv = mi.base.bv;
	display_style_ = mi.base.font.style() == DISPLAY_STYLE;
	MathData * md = const_cast<MathData*>(this);
	md->updateMacros(&bv->cursor(), mi.macrocontext,
	                 InternalUpdate, mi.base.macro_nesting);

	pos_type bspos = -1, espos = -1;
	Cursor const & cur = bv->cursor();
	InsetMath const * inset = cur.inset().asInsetMath();
	if (cur.selection() && inset) {
		CursorSlice const s1 = cur.selBegin();
		CursorSlice const s2 = cur.selEnd();
		// Detect inner selection in this math data.
		if (s1.idx() == s2.idx() && &inset->cell(s1.idx()) == this) {
			bspos = s1.pos();
			espos = s2.pos();
		} else if (s1.idx() != s2.idx()) {
			// search for this math data and check whether it is selected
			for (idx_type idx = 0; idx < inset->nargs(); ++idx) {
				MathData const & c = inset->cell(idx);
				if (&c == this && inset->idxBetween(idx, s1.idx(), s2.idx())) {
					// whole cell is selected
					bspos = 0;
					espos = size();
					// no need to continue searchning
					break;
				}
			}
		}
	}

	// FIXME: for completion, try to insert the relevant data in the
	// mathrow (like is done for text rows). We could add a pair of
	// InsetMathColor inset, but these come with extra spacing of
	// their own.
	DocIterator const & inlineCompletionPos = bv->inlineCompletionPos();
	bool const has_completion = inlineCompletionPos.inMathed()
		&& &inlineCompletionPos.cell() == this;
	size_t const compl_pos = has_completion ? inlineCompletionPos.pos() : 0;

	for (size_t i = 0 ; i < size() ; ++i) {
		if (i == bspos)
			mrow.push_back(MathRow::Element(mi, MathRow::BEGIN_SEL));
		has_contents |= (*this)[i]->addToMathRow(mrow, mi);
		if (i + 1 == compl_pos) {
			mrow.back().compl_text = bv->inlineCompletion();
			mrow.back().compl_unique_to = bv->inlineCompletionUniqueChars();
		}
		if (i + 1 == espos)
			mrow.push_back(MathRow::Element(mi, MathRow::END_SEL));
	}

	// insert virtual preedit
	bool const has_preedit = !bv->inputMethod()->preeditString().empty() &&
	        cur.inMathed() && &cur.cell() == this &&
	        cur.inset().currentMode() == Inset::TEXT_MODE;
	docstring const preedit_str = bv->inputMethod()->preeditString();

	if (has_preedit) {
		for (size_t i = 0 ; i < preedit_str.size(); ++i) {
			MathRow::Element e(mi, MathRow::INSET, MC_ORD);
			e.inset = new InsetMathChar(buffer_, *preedit_str.substr(i, 1).c_str());
			e.im = bv->inputMethod();
			e.char_format_index = bv->inputMethod()->charFormatIndex(i);

			mrow.insert(cur.pos() + i + 1, e);
			has_contents = true;
		}
	}
	return has_contents;
}


#if 0
namespace {

bool isInside(DocIterator const & it, MathData const & md,
	pos_type p1, pos_type p2)
{
	for (size_t i = 0; i != it.depth(); ++i) {
		CursorSlice const & sl = it[i];
		if (sl.inset().inMathed() && &sl.cell() == &md)
			return p1 <= sl.pos() && sl.pos() < p2;
	}
	return false;
}

}
#endif


void MathData::metrics(MetricsInfo & mi, Dimension & dim, bool tight) const
{
	frontend::FontMetrics const & fm = theFontMetrics(mi.base.font);
	BufferView * bv = mi.base.bv;
	int const Iascent = fm.dimension('I').ascent();
	int xascent = fm.xHeight();
	if (xascent >= Iascent)
		xascent = (2 * Iascent) / 3;
	minasc_ = xascent;
	mindes_ = (3 * xascent) / 4;
	slevel_ = (4 * xascent) / 5;
	sshift_ = xascent / 4;

	MathRow mrow(mi, this);
	mrow.metrics(mi, dim);

	// Set a minimal ascent/descent for the cell
	if (tight)
		// FIXME: this is the minimal ascent seen empirically, check
		// what the TeXbook says.
		dim.asc = max(dim.asc, fm.xHeight());
	else {
		dim.asc = max(dim.asc, fm.maxAscent());
		dim.des = max(dim.des, fm.maxDescent());
	}

	// This is one of the the few points where the drawing font is known,
	// so that we can set the caret vertical dimensions.
	mrow.caret_dim.asc = min(dim.asc, fm.maxAscent());
	mrow.caret_dim.des = min(dim.des, fm.maxDescent());
	mrow.caret_dim.wid = max(fm.lineWidth(), 1);

	/// do the same for math cells linearized in the row
	MathRow caret_row = MathRow(mrow.caret_dim);
	for (auto const & e : mrow)
		if (e.type == MathRow::BEGIN && e.md)
			bv->setMathRow(e.md, caret_row);

	// Cache row and dimension.
	bv->setMathRow(this, mrow);
	bv->coordCache().cells().add(this, dim);
}


void MathData::drawSelection(PainterInfo & pi, int const x, int const y) const
{
	BufferView const * bv = pi.base.bv;
	Cursor const & cur = bv->cursor();
	InsetMath const * inset = cur.inset().asInsetMath();
	if (!cur.selection() || !inset || !inset->isActive())
		return;

	CursorSlice const s1 = cur.selBegin();
	CursorSlice const s2 = cur.selEnd();
	MathData const & c1 = inset->cell(s1.idx());

	if (s1.idx() == s2.idx() && &c1 == this) {
		// selection inside cell
		Dimension const dim = bv->coordCache().cells().dim(&c1);
		int const beg = c1.pos2x(bv, s1.pos());
		int const end = c1.pos2x(bv, s2.pos());
		pi.pain.fillRectangle(x + beg, y - dim.ascent(),
		                      end - beg, dim.height(), Color_selection);
	} else {
		for (idx_type i = 0; i < inset->nargs(); ++i) {
			MathData const & c = inset->cell(i);
			if (&c == this && inset->idxBetween(i, s1.idx(), s2.idx())) {
				// The whole cell is selected
				Dimension const dim = bv->coordCache().cells().dim(&c);
				pi.pain.fillRectangle(x, y - dim.ascent(),
				                      dim.width(), dim.height(),
				                      Color_selection);
			}
		}
	}
}


void MathData::draw(PainterInfo & pi, int const x, int const y) const
{
	//lyxerr << "MathData::draw: x: " << x << " y: " << y << endl;
	setXY(*pi.base.bv, x, y);

	drawSelection(pi, x, y);
	MathRow const & mrow = pi.base.bv->mathRow(this);
	mrow.draw(pi, x, y);
}


void MathData::metricsT(TextMetricsInfo const & mi, Dimension & dim) const
{
	dim.clear();
	Dimension d;
	for (const_iterator it = begin(); it != end(); ++it) {
		(*it)->metricsT(mi, d);
		dim += d;
	}
}


void MathData::drawT(TextPainter & pain, int x, int y) const
{
	//lyxerr << "x: " << x << " y: " << y << ' ' << pain.workAreaHeight() << endl;

	// FIXME: Abdel 16/10/2006
	// This drawT() method is never used, this is dead code.

	for (auto const & it : *this) {
		it->drawT(pain, x, y);
		//x += it->width_;
		x += 2;
	}
}


int MathData::kerning(BufferView const * bv) const
{
	return 	bv->mathRow(this).kerning(bv);
}


void MathData::updateBuffer(ParIterator const & it, UpdateType utype, bool const deleted)
{
	// pass down
	for (size_t i = 0, n = size(); i != n; ++i) {
		MathAtom & at = operator[](i);
		at.nucleus()->updateBuffer(it, utype, deleted);
	}
}


void MathData::updateMacros(Cursor * cur, MacroContext const & mc,
		UpdateType utype, int nesting)
{
	// If we are editing a macro, we cannot update it immediately,
	// otherwise wrong undo steps will be recorded (bug 6208).
	InsetMath const * inmath = cur ? cur->inset().asInsetMath() : 0;
	InsetMathMacro const * inmacro = inmath ? inmath->asMacroInset() : 0;
	docstring const edited_name = inmacro ? inmacro->name() : docstring();

	// go over the data and look for macros
	for (size_t i = 0; i < size(); ++i) {
		InsetMathMacro * macroInset = operator[](i).nucleus()->asMacroInset();
		if (!macroInset || macroInset->macroName().empty()
				|| macroInset->macroName()[0] == '^'
				|| macroInset->macroName()[0] == '_'
				|| (macroInset->name() == edited_name
				    && macroInset->displayMode() ==
						InsetMathMacro::DISPLAY_UNFOLDED))
			continue;

		// get macro
		macroInset->updateMacro(mc);
		size_t macroNumArgs = 0;
		size_t macroOptionals = 0;
		MacroData const * macro = macroInset->macro();
		if (macro) {
			macroNumArgs = macro->numargs();
			macroOptionals = macro->optionals();
		}

		// store old and compute new display mode
		InsetMathMacro::DisplayMode newDisplayMode;
		InsetMathMacro::DisplayMode oldDisplayMode = macroInset->displayMode();
		newDisplayMode = macroInset->computeDisplayMode();

		// arity changed or other reason to detach?
		if (oldDisplayMode == InsetMathMacro::DISPLAY_NORMAL
		    && (macroInset->arity() != macroNumArgs
			|| macroInset->optionals() != macroOptionals
			|| newDisplayMode == InsetMathMacro::DISPLAY_UNFOLDED))
			detachMacroParameters(cur, i);

		// the macro could have been copied while resizing this
		macroInset = operator[](i).nucleus()->asMacroInset();

		// Cursor in \label?
		if (newDisplayMode != InsetMathMacro::DISPLAY_UNFOLDED
		    && oldDisplayMode == InsetMathMacro::DISPLAY_UNFOLDED) {
			// put cursor in front of macro
			if (cur) {
				size_type macroSlice = cur->find(macroInset);
				if (macroSlice != lyx::npos)
					cur->resize(macroSlice);
			}
		}

		// update the display mode
		size_t appetite = macroInset->appetite();
		macroInset->setDisplayMode(newDisplayMode);

		// arity changed?
		if (newDisplayMode == InsetMathMacro::DISPLAY_NORMAL
		    && (macroInset->arity() != macroNumArgs
			|| macroInset->optionals() != macroOptionals)) {
			// is it a virgin macro which was never attached to parameters?
			bool fromInitToNormalMode
			= (oldDisplayMode == InsetMathMacro::DISPLAY_INIT
			   || oldDisplayMode == InsetMathMacro::DISPLAY_INTERACTIVE_INIT)
			  && newDisplayMode == InsetMathMacro::DISPLAY_NORMAL;

			// if the macro was entered interactively (i.e. not by paste or during
			// loading), it should not be greedy, but the cursor should
			// automatically jump into the macro when behind
			bool interactive = (oldDisplayMode == InsetMathMacro::DISPLAY_INTERACTIVE_INIT);

			// attach parameters
			attachMacroParameters(cur, i, macroNumArgs, macroOptionals,
				fromInitToNormalMode, interactive, appetite);

			if (cur)
				cur->updateInsets(&cur->bottom().inset());
		}

		// Give macro the chance to adapt to new situation.
		// The macroInset could be invalid now because it was put into a script
		// inset and therefore "deep" copied. So get it again from the MathData.
		InsetMath * inset = operator[](i).nucleus();
		if (inset->asScriptInset())
			inset = inset->asScriptInset()->nuc()[0].nucleus();
		LASSERT(inset->asMacroInset(), continue);
		inset->asMacroInset()->updateRepresentation(cur, mc, utype, nesting + 1);
	}
}


void MathData::detachMacroParameters(DocIterator * cur, const size_type macroPos)
{
	InsetMathMacro * macroInset = operator[](macroPos).nucleus()->asMacroInset();
	// We store this now, because the inset pointer will be invalidated in the scond loop below
	size_t const optionals = macroInset->optionals();

	// detach all arguments
	vector<MathData> detachedArgs;
	if (macroPos + 1 == size())
		// strip arguments if we are at the MathData end
		macroInset->detachArguments(detachedArgs, true);
	else
		macroInset->detachArguments(detachedArgs, false);

	// find cursor slice
	size_type curMacroSlice = lyx::npos;
	if (cur)
		curMacroSlice = cur->find(macroInset);
	idx_type curMacroIdx = lyx::npos;
	pos_type curMacroPos = lyx::npos;
	vector<CursorSlice> argSlices;
	if (curMacroSlice != lyx::npos) {
		curMacroPos = (*cur)[curMacroSlice].pos();
		curMacroIdx = (*cur)[curMacroSlice].idx();
		cur->resize(curMacroSlice + 1, argSlices);
		cur->pop_back();
	}

	// only [] after the last non-empty argument can be dropped later
	size_t lastNonEmptyOptional = 0;
	for (size_t l = 0; l < detachedArgs.size() && l < optionals; ++l) {
		if (!detachedArgs[l].empty())
			lastNonEmptyOptional = l;
	}

	// optional arguments to be put back?
	pos_type p = macroPos + 1;
	size_t j = 0;
	// We do not want to use macroInset below, the insert() call in
	// the loop will invalidate it.
	macroInset = 0;
	for (; j < detachedArgs.size() && j < optionals; ++j) {
		// another non-empty parameter follows?
		bool canDropEmptyOptional = j >= lastNonEmptyOptional;

		// then we can drop empty optional parameters
		if (detachedArgs[j].empty() && canDropEmptyOptional) {
			if (curMacroIdx == j)
				(*cur)[curMacroSlice - 1].pos() = macroPos + 1;
			continue;
		}

		// Otherwise we don't drop an empty optional, put it back normally
		MathData optarg(buffer_);
		asMathData(from_ascii("[]"), optarg);
		MathData & arg = detachedArgs[j];

		// look for "]", i.e. put a brace around?
		InsetMathBrace * brace = 0;
		for (size_t q = 0; q < arg.size(); ++q) {
			if (arg[q]->getChar() == ']') {
				// put brace
				brace = new InsetMathBrace(buffer_);
				break;
			}
		}

		// put arg between []
		if (brace) {
			brace->cell(0) = arg;
			optarg.insert(1, MathAtom(brace));
		} else
			optarg.insert(1, arg);

		// insert it
		insert(p, optarg);
		p += optarg.size();

		// cursor in macro?
		if (curMacroSlice == lyx::npos)
			continue;

		// cursor in optional argument of macro?
		if (curMacroIdx == j) {
			if (brace) {
				cur->append(0, curMacroPos);
				(*cur)[curMacroSlice - 1].pos() = macroPos + 2;
			} else
				(*cur)[curMacroSlice - 1].pos() = macroPos + 2 + curMacroPos;
			cur->append(argSlices);
		} else if ((*cur)[curMacroSlice - 1].pos() >= int(p))
			// cursor right of macro
			(*cur)[curMacroSlice - 1].pos() += optarg.size();
	}

	// put them back into the MathData
	for (; j < detachedArgs.size(); ++j, ++p) {
		MathData const & arg = detachedArgs[j];
		if (arg.size() == 1
		    && !arg[0]->asScriptInset()
		    && !(arg[0]->asMacroInset() && arg[0]->asMacroInset()->arity() > 0))
			insert(p, arg[0]);
		else
			insert(p, MathAtom(new InsetMathBrace(buffer_, arg)));

		// cursor in macro?
		if (curMacroSlice == lyx::npos)
			continue;

		// cursor in j-th argument of macro?
		if (curMacroIdx == j) {
			if (operator[](p).nucleus()->asBraceInset()) {
				(*cur)[curMacroSlice - 1].pos() = p;
				cur->append(0, curMacroPos);
				cur->append(argSlices);
			} else {
				(*cur)[curMacroSlice - 1].pos() = p; // + macroPos;
				cur->append(argSlices);
			}
		} else if ((*cur)[curMacroSlice - 1].pos() >= int(p))
			++(*cur)[curMacroSlice - 1].pos();
	}

	if (cur)
		cur->updateInsets(&cur->bottom().inset());
}


void MathData::attachMacroParameters(Cursor * cur,
	const pos_type macroPos, const size_type macroNumArgs,
	const int macroOptionals, const bool fromInitToNormalMode,
	const bool interactiveInit, const size_t appetite)
{
	// Coverity Scan believes that this can happen
	LATTEST(macroPos != lyx::npos);

	InsetMathMacro * macroInset = operator[](macroPos).nucleus()->asMacroInset();

	// start at atom behind the macro again, maybe with some new arguments
	// from the detach phase above, to add them back into the macro inset
	size_t p = macroPos + 1;
	vector<MathData> detachedArgs;
	MathAtom scriptToPutAround;

	// find cursor slice again of this MathData
	size_type thisSlice = lyx::npos;
	if (cur)
		thisSlice = cur->find(*this);
	pos_type thisPos = lyx::npos;
	if (thisSlice != lyx::npos)
		thisPos = (*cur)[thisSlice].pos();

	// find arguments behind the macro
	if (!interactiveInit) {
		collectOptionalParameters(cur, macroOptionals, detachedArgs, p,
			scriptToPutAround, macroPos, thisPos, thisSlice);
	}
	collectParameters(cur, macroNumArgs, detachedArgs, p,
		scriptToPutAround, macroPos, thisPos, thisSlice, appetite);

	// attach arguments back to macro inset
	macroInset->attachArguments(detachedArgs, macroNumArgs, macroOptionals);

	// found tail script? E.g. \foo{a}b^x
	if (scriptToPutAround.nucleus()) {
		InsetMathScript * scriptInset =
			scriptToPutAround.nucleus()->asScriptInset();
		// In the math parser we remove empty braces in the base
		// of a script inset, but we have to restore them here.
		if (scriptInset->nuc().empty()) {
			MathData md(buffer_);
			scriptInset->nuc().push_back(
					MathAtom(new InsetMathBrace(buffer_, md)));
		}
		// put macro into a script inset
		scriptInset->nuc()[0] = operator[](macroPos);
		operator[](macroPos) = scriptToPutAround;

		// go into the script inset nucleus
		if (cur && thisPos == macroPos)
			cur->append(0, 0);

		// get pointer to "deep" copied macro inset
		scriptInset = operator[](macroPos).nucleus()->asScriptInset();
		macroInset = scriptInset->nuc()[0].nucleus()->asMacroInset();
	}

	// remove them from the MathData
	erase(macroPos + 1, p);

	// cursor outside this MathData?
	if (thisSlice == lyx::npos)
		return;

	// fix cursor if right of p
	if (thisPos >= p)
		(*cur)[thisSlice].pos() -= p - (macroPos + 1);

	// was the macro inset just inserted interactively and was now folded
	// and the cursor is just behind?
	// FIXME: MathData::pos_type is unsigned, whereas lyx::pos_type is signed.
	//        This is not good.
	if ((*cur)[thisSlice].pos() == lyx::pos_type(macroPos + 1)
	    && interactiveInit
	    && fromInitToNormalMode
	    && macroInset->arity() > 0
	    && thisSlice + 1 == cur->depth()) {
		// then enter it if the cursor was just behind
		(*cur)[thisSlice].pos() = macroPos;
		cur->push_back(CursorSlice(*macroInset));
		macroInset->idxFirst(*cur);
	}
}


void MathData::collectOptionalParameters(Cursor * cur,
	const size_type numOptionalParams, vector<MathData> & params,
	size_t & pos, MathAtom & scriptToPutAround,
	const pos_type macroPos, const pos_type thisPos, const size_type thisSlice)
{
	Buffer * buf = cur ? cur->buffer() : 0;
	// insert optional arguments?
	while (params.size() < numOptionalParams
	       && pos < size()
	       && !scriptToPutAround.nucleus()) {
		// is a [] block following which could be an optional parameter?
		if (operator[](pos)->getChar() != '[')
			break;

		// found possible optional argument, look for pairing "]"
		int count = 1;
		size_t right = pos + 1;
		for (; right < size(); ++right) {
			MathAtom & cell = operator[](right);

			if (cell->getChar() == '[')
				++count;
			else if (cell->getChar() == ']' && --count == 0)
				// found right end
				break;

			// maybe "]" with a script around?
			InsetMathScript * script = cell.nucleus()->asScriptInset();
			if (!script)
				continue;
			if (script->nuc().size() != 1)
				continue;
			if (script->nuc()[0]->getChar() == ']') {
				// script will be put around the macro later
				scriptToPutAround = cell;
				break;
			}
		}

		// found?
		if (right >= size()) {
			// no ] found, so it's not an optional argument
			break;
		}

		// add everything between [ and ] as optional argument
		MathData optarg(buf, begin() + pos + 1, begin() + right);

		// a brace?
		bool brace = false;
		if (optarg.size() == 1 && optarg[0]->asBraceInset()) {
			brace = true;
			params.push_back(optarg[0]->asBraceInset()->cell(0));
		} else
			params.push_back(optarg);

		// place cursor in optional argument of macro
		// Note: The two expressions on the first line are equivalent
		// (see caller), but making this explicit pleases coverity.
		if (cur && thisSlice != lyx::npos
		    && thisPos >= pos && thisPos <= right) {
			int paramPos = max(0, int(thisPos - pos - 1));
			vector<CursorSlice> x;
			cur->resize(thisSlice + 1, x);
			(*cur)[thisSlice].pos() = macroPos;
			if (brace) {
				paramPos = x[0].pos();
				x.erase(x.begin());
			}
			cur->append(0, paramPos);
			cur->append(x);
		}
		pos = right + 1;
	}

	// fill up empty optional parameters
	while (params.size() < numOptionalParams)
		params.push_back(MathData(buffer_));
}


void MathData::collectParameters(Cursor * cur,
	const size_type numParams, vector<MathData> & params,
	size_t & pos, MathAtom & scriptToPutAround,
	const pos_type macroPos, const pos_type thisPos, const size_type thisSlice,
	const size_t appetite)
{
	size_t startSize = params.size();

	// insert normal arguments
	while (params.size() < numParams
	       && params.size() - startSize < appetite
	       && pos < size()
	       && !scriptToPutAround.nucleus()) {
		MathAtom & cell = operator[](pos);

		// fix cursor
		vector<CursorSlice> argSlices;
		int argPos = 0;
		// Note: The two expressions on the first line are equivalent
		// (see caller), but making this explicit pleases coverity.
		if (cur && thisSlice != lyx::npos && thisPos == pos)
			cur->resize(thisSlice + 1, argSlices);

		// which kind of parameter is it? In {}? With index x^n?
		InsetMathBrace const * brace = cell->asBraceInset();
		if (brace) {
			// found brace, convert into argument
			params.push_back(brace->cell(0));

			// cursor inside of the brace or just in front of?
			if (thisPos == pos && !argSlices.empty()) {
				argPos = argSlices[0].pos();
				argSlices.erase(argSlices.begin());
			}
		} else if (cell->asScriptInset() && params.size() + 1 == numParams) {
			// last inset with scripts without braces
			// -> they belong to the macro, not the argument
			InsetMathScript * script = cell.nucleus()->asScriptInset();
			if (script->nuc().size() == 1 && script->nuc()[0]->asBraceInset())
				// nucleus in brace? Unpack!
				params.push_back(script->nuc()[0]->asBraceInset()->cell(0));
			else
				params.push_back(script->nuc());

			// script will be put around below
			scriptToPutAround = cell;

			// this should only happen after loading, so make cursor handling simple
			if (thisPos >= macroPos && thisPos <= macroPos + numParams) {
				argSlices.clear();
				if (cur)
					cur->append(0, 0);
			}
		} else {
			// the simplest case: plain inset
			MathData md(buffer_);
			md.insert(0, cell);
			params.push_back(md);
		}

		// put cursor in argument again
		// Note: The first two expressions on the first line are
		// equivalent (see caller), but making this explicit pleases
		// coverity.
		if (cur && thisSlice != lyx::npos && thisPos == pos) {
			cur->append(params.size() - 1, argPos);
			cur->append(argSlices);
			(*cur)[thisSlice].pos() = macroPos;
		}

		++pos;
	}
}


int MathData::pos2x(BufferView const * bv, size_type pos) const
{
	int x = 0;
	size_type target = min(pos, size());
	CoordCache::Insets const & coords = bv->coordCache().insets();
	for (size_type i = 0; i < target; ++i) {
		const_iterator it = begin() + i;
		//lyxerr << "char: " << (*it)->getChar()
		//	<< "width: " << (*it)->width() << endl;
		x += coords.dim((*it).nucleus()).wid;
	}
	return x;
}


MathData::size_type MathData::x2pos(BufferView const * bv, int targetx) const
{
	const_iterator it = begin();
	int lastx = 0;
	int currx = 0;
	CoordCache::Insets const & coords = bv->coordCache().insets();
	// find first position after targetx
	for (; currx < targetx && it != end(); ++it) {
		lastx = currx;
		currx += coords.dim((*it).nucleus()).wid;
	}

	/**
	 * If we are not at the beginning of the data, go to the left
	 * of the inset if one of the following two condition holds:
	 * - the current inset is editable (so that the cursor tip is
	 *   deeper than us): in this case, we want all intermediate
	 *   cursor slices to be before insets;
	 * - the mouse is closer to the left side of the inset than to
	 *   the right one.
	 * See bug 1918 for details.
	 **/
	if (it != begin() && currx >= targetx
	    && ((*prev(it, 1))->asNestInset()
		|| abs(lastx - targetx) < abs(currx - targetx))) {
		--it;
	}

	return it - begin();
}


int MathData::dist(BufferView const & bv, int x, int y) const
{
	return bv.coordCache().cells().squareDistance(this, x, y);
}


void MathData::setXY(BufferView & bv, int x, int y) const
{
	//lyxerr << "setting position cache for MathData " << this << endl;
	bv.coordCache().cells().add(this, x, y);
}


Dimension const & MathData::dimension(BufferView const & bv) const
{
	return bv.coordCache().cells().dim(this);
}


int MathData::xo(BufferView const & bv) const
{
	return bv.coordCache().cells().x(this);
}


int MathData::yo(BufferView const & bv) const
{
	return bv.coordCache().cells().y(this);
}


MathClass MathData::mathClass() const
{
	MathClass res = MC_UNKNOWN;
	for (MathAtom const & at : *this) {
		MathClass mc = at->mathClass();
		if (res == MC_UNKNOWN)
			res = mc;
		else if (mc != MC_UNKNOWN && res != mc)
			return MC_ORD;
	}
	return res == MC_UNKNOWN ? MC_ORD : res;
}


MathClass MathData::firstMathClass() const
{
	for (MathAtom const & at : *this) {
		MathClass mc = at->mathClass();
		if (mc != MC_UNKNOWN)
			return mc;
	}
	return MC_ORD;
}


MathClass MathData::lastMathClass() const
{
	MathClass res = MC_ORD;
	for (MathAtom const & at : *this) {
		MathClass mc = at->mathClass();
		if (mc != MC_UNKNOWN)
			res = mc;
	}
	return res;
}


ostream & operator<<(ostream & os, MathData const & md)
{
	odocstringstream oss;
	NormalStream ns(oss);
	ns << md;
	return os << to_utf8(oss.str());
}


odocstream & operator<<(odocstream & os, MathData const & md)
{
	NormalStream ns(os);
	ns << md;
	return os;
}


} // namespace lyx
