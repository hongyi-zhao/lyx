// -*- C++ -*-
/**
 * \file ParagraphMetrics.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Asger Alstrup
 * \author Lars Gullik Bjønnes
 * \author John Levon
 * \author André Pönitz
 * \author Jürgen Vigna
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef PARAGRAPH_METRICS_H
#define PARAGRAPH_METRICS_H

#include "Dimension.h"
#include "Row.h"

#include <vector>

namespace lyx {

class BufferView;
class Paragraph;

/**
 * Each paragraph is broken up into a number of rows on the screen.
 * This is a list of such on-screen rows, ordered from the top row
 * downwards.
 */
typedef std::vector<Row> Rows;

/// Helper class for paragraph metrics.
class ParagraphMetrics {
public:
	/// Default constructor (only here for STL containers).
	ParagraphMetrics() {}
	/// The only useful constructor.
	explicit ParagraphMetrics(Paragraph const & par);

	void reset(Paragraph const & par);

	///
	Row const & getRow(pos_type pos, bool boundary) const;
	///
	size_t getRowIndex(pos_type pos, bool boundary) const;

	/// TextMetrics::redoParagraph updates this
	Dimension const & dim() const { return dim_; }
	Dimension & dim() { return dim_; }
	/// total height of paragraph
	int height() const { return dim_.height(); }
	/// total width of paragraph, may differ from workwidth
	int width() const { return dim_.width(); }
	/// ascend of paragraph above baseline
	int ascent() const { return dim_.ascent(); }
	/// descend of paragraph below baseline
	int descent() const { return dim_.descent(); }
	/// Text updates the rows using this access point
	Rows & rows() { return rows_; }
	/// The painter and others use this
	Rows const & rows() const { return rows_; }
	///
	int rightMargin(BufferView const & bv) const;
	///
	Paragraph const & par() const { return *par_; }

	/// dump some information to lyxerr
	void dump() const;

	///
	bool hfillExpansion(Row const & row, pos_type pos) const;

	/// The vertical position of the baseline of the first line of the paragraph
	int position() const;
	void setPosition(int position);
	/// Set position to unknown
	void resetPosition();
	/// Return true when the position of the paragraph is known
	bool hasPosition() const;
	/// The vertical position of the top of the paragraph
	int top() const { return position() - dim_.ascent(); }
	/// The vertical position of the bottom of the paragraph
	int bottom() const { return position() + dim_.descent(); }
	///
	int id() const { return id_; }

private:
	///
	int position_ = 0;
	/// This is stored separately from the paragraph to work around
	/// InsetInfo messiness in ticket #12639.
	int id_ = -1;
	///
	mutable Rows rows_;
	/// cached dimensions of paragraph
	Dimension dim_;
	///
	Paragraph const * par_ = nullptr;
};

} // namespace lyx

#endif // PARAGRAPH_METRICS_H
