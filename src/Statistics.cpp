// -*- C++ -*-
/**
 * \file Statistics.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jean-Marc Lasgouttes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "Statistics.h"

#include "Paragraph.h"
#include "Text.h"
#include "Cursor.h"

#include "support/lassert.h"
#include "support/lstrings.h"
#include "support/textutils.h"


namespace lyx {

using namespace support;


void Statistics::update(CursorData const & cur)
{
	// reset counts
	*this = Statistics();
	if (cur.selection()) {
		if (cur.inMathed())
			return;
		CursorSlice from, to;
		from = cur.selBegin();
		to = cur.selEnd();
		update(from, to);
	} else
		update(*cur.bottom().text());
}


void Statistics::update(docstring const & s)
{
	// FIXME: use a stripped-down version of the paragraph code.
	// This is the original code from InsetCitation::isWords()
	char_count += s.size();
	// FIXME: this does not count words properly
	word_count += wordCount(s);
	// FIXME: spaces are not counted
}


void Statistics::update(Text const & text)
{
	for (Paragraph const & par : text.paragraphs())
		update(par);
}


void Statistics::update(CursorSlice const & from, CursorSlice & to)
{
	LASSERT(from.text() == to.text(), return);
	if (from.idx() == to.idx()) {
		if (from.pit() == to.pit()) {
			update(from.paragraph(), from.pos(), to.pos());
		} else {
			pos_type frompos = from.pos();
			for (pit_type pit = from.pit() ; pit < to.pit() ; ++pit) {
				update(from.text()->getPar(pit), frompos);
				frompos = 0;
			}
			update(to.paragraph(), 0, to.pos());
		}
	} else
		for (idx_type idx = from.idx() ; idx <= to.idx(); ++idx)
			update(*from.inset().getText(idx));
}


void Statistics::update(Paragraph const & par, pos_type from, pos_type to)
{
	if (to == -1)
		to = par.size();

	for (pos_type pos = from ; pos < to ; ++pos) {
		Inset const * ins = par.isInset(pos) ? par.getInset(pos) : nullptr;
		// Stuff that we skip
		if (par.isDeleted(pos))
			continue;
		if (ins && skip_no_output && !ins->producesOutput())
			continue;

		// words
		if (par.isWordSeparator(pos))
			inword = false;
		else if (!inword) {
			++word_count;
			inword = true;
		}

		if (ins)
			ins->updateStatistics(*this);
		else {
			char_type const c = par.getChar(pos);
			if (isPrintableNonspace(c))
				++char_count;
			else if (lyx::isSpace(c))
				++blank_count;
		}
	}
	inword = false;
}


} // namespace lyx

