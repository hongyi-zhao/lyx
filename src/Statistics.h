// -*- C++ -*-
/**
 * \file Statistics.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jean-Marc Lasgouttes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef STATISTICS_H
#define STATISTICS_H

#include "support/strfwd.h"
#include "support/types.h"

namespace lyx {

class CursorData;
class CursorSlice;
class Text;
class Paragraph;

// Class used to compute letters/words statistics on buffer or selection
class Statistics {
public:
	/// Count characters in the whole document, or in the selection if
	/// there is one. This is the main entry point.
	void update(CursorData const & cur, bool skip = true);

	/// Helper: count chars and words in this string
	void update(docstring const & s);
	/// Helper: count chars and words in the paragraphs of \c text
	void update(Text const & text);

	// Number of words
	int word_count = 0;
	// Number of non blank characters
	int char_count = 0;
	// Number of blank characters
	int blank_count = 0;

private:

	/// Count chars and words between two positions
	void update(CursorSlice const & from, CursorSlice & to);

	/** Count chars and words in a paragraph
	 * \param par: the paragraph
	 * \param from: starting position
	 * \param to: end position. If it is equal to -1, then the end is
	 *    the end of the paragraph.
	 */
	void update(Paragraph const & par, pos_type from = 0, pos_type to = -1);

	// Indicate whether parts that produce no output should be counted.
	bool skip_no_output_ = false;
	// Used in the code to track status
	bool inword_ = false;
	// The buffer id at last statistics computation.
	int stats_id_ = -1;
};

}

#endif // STATISTICS_H
