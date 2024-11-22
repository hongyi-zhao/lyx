// -*- C++ -*-
/**
 * \file RowFlags.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jean-Marc Lasgouttes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef ROWFLAGS_H
#define ROWFLAGS_H

// Do not include anything here

namespace lyx {

/* The list of possible flags, that can be combined. Some flags that
 * should logically be here (e.g., AlwaysBreakBefore), do not exist.
 * This is because the need has not been identitfied yet.
 *
 * Priorities when before/after disagree:
 *      AlwaysBreak* > NoBreak* > Break* or CanBreak*.
 */
enum RowFlags {
	// Do not break before or after this element, except if really
	// needed (between NoBreak* and CanBreak*).
	Inline = 0,
	// force (maybe empty) row before this element
	AlwaysBreakBefore = 1 << 0,
	// break row before this element if the row is not empty
	BreakBefore = 1 << 1,
	// break row whenever needed before this element
	CanBreakBefore = 1 << 2,
	// Avoid breaking row before this element
	NoBreakBefore = 1 << 3,
	// flush the row before this element (useful with BreakBefore)
	FlushBefore = 1 << 4,
	// force new (maybe empty) row after this element
	AlwaysBreakAfter = 1 << 5,
	// break row after this element if there are more elements
	BreakAfter = 1 << 6,
	// break row whenever needed after this element
	CanBreakAfter = 1 << 7,
	// Avoid breaking row after this element
	NoBreakAfter = 1 << 8,
	// The contents of the row may be broken in two (e.g. string)
	CanBreakInside = 1 << 9,
	// Flush the row that ends with this element
	Flush = 1 << 10,
	// specify an alignment (left, right) for a display element
	// (default is center)
	AlignLeft = 1 << 11,
	AlignRight = 1 << 12,
	// Forbid boundary after this element
	NoEndBoundary = 1 << 13,
	// A display element breaks row at both ends
	Display = FlushBefore | BreakBefore | BreakAfter,
	// Flags that concern breaking after element
	AfterFlags = AlwaysBreakAfter | BreakAfter | CanBreakAfter | NoBreakAfter
};

} // namespace lyx

#endif
