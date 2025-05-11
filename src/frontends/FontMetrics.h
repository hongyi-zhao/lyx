// -*- C++ -*-
/**
 * \file FontMetrics.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author unknown
 * \author John Levon
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef FONT_METRICS_H
#define FONT_METRICS_H

#include "Dimension.h"

#include "support/mute_warning.h"
#include "support/docstring.h"

#include <vector>

/**
 * A class holding helper functions for determining
 * the screen dimensions of fonts.
 *
 * The geometry is the standard typographical geometry,
 * as follows :
 *
 * --------------+------------------<maxAscent
 *               |          |
 *               <-------> (right bearing)
 *               <-> (left bearing)
 * char ascent>___          |
 *               ^   oooo   |  oooo
 *   origin>____ |  oo  oo  | oo  oo
 *              \|  oo  oo  | oo  oo
 * --------------+---ooooo--|--oooo-<baseline
 *               |      oo  |
 * char          |  oo  oo  |
 * descent>______|   oooo   |
 *               <-  width ->
 * --------------+----------+-------<maxDescent
 *
 * Caution: All char_type and docstring arguments of any method of this class
 * are no UCS4 chars or strings if the font is a symbol font. They simply
 * denote the code points of the font instead. You have to keep this in mind
 * when you implement the methods in a frontend. You must not pass these
 * parameters to a unicode conversion function in particular.
 */

namespace lyx {

class Dimension;

namespace frontend {

class FontMetrics
{
public:
	virtual ~FontMetrics() {}

	/// return the maximum ascent of the font
	virtual int maxAscent() const = 0;
	/// return the maximum descent of the font
	virtual int maxDescent() const = 0;
	/// return default dimension of the font.
	/// \warning \c width is set to zero.
	virtual Dimension const defaultDimension() const = 0;
	/// return the em size
	virtual int em() const = 0;
	/// return the x height
	virtual int xHeight() const = 0;
	/// return the width of a line for underlining
	virtual int lineWidth() const = 0;
	/// return the distance from the base line to where an underline
	/// should be drawn.
	virtual int underlinePos() const = 0;
	/// return the distance from the base line to where the strike out line
	/// should be drawn.
	virtual int strikeoutPos() const = 0;
	/// return true if font is not upright (italic or oblique)
	virtual bool italic() const = 0;
	/// return slope for italic font
	virtual double italicSlope() const = 0;

	/// return the ascent of the char in the font
	virtual int ascent(char_type c) const = 0;
	/// return the descent of the char in the font
	virtual int descent(char_type c) const = 0;
	/// return the maximum height of the font
	inline int maxHeight() const { return maxAscent() + maxDescent(); }
	/// return the height of the char in the font
	inline int height(char_type c) const { return ascent(c) + descent(c); }

	/// return the left bearing of the char in the font
	virtual int lbearing(char_type c) const = 0;
	/// return the right bearing of the char in the font
	virtual int rbearing(char_type c) const = 0;
	/// return the width of the char in the font
	virtual int width(char_type c) const = 0;
	/// return the width of the string in the font
	virtual int width(docstring const & s) const = 0;
	/// FIXME ??
	virtual int signedWidth(docstring const & s) const = 0;
	/// return the inner width of the char in the font
	inline int center(char_type c) const {
		return (rbearing(c) - lbearing(c)) / 2;
	}

	/**
	 * return the x offset of a position in the string. The
	 * direction of the string is forced, and the returned value
	 * is from the left edge of the word, not from the start of the string.
	 * \param rtl is true for right-to-left layout
	 * \param ws is the amount of extra inter-word space applied text justification.
	 */
	virtual int pos2x(docstring const & s, int pos, bool rtl, double ws) const = 0;
	/**
	 * return the position in the string for a given x offset. The
	 * direction of the string is forced, and the returned value
	 * is from the left edge of the word, not from the start of the string.
	 * the offset x is updated to match the closest position in the string.
	 * \param rtl is true for right-to-left layout
	 * \param ws is the amount of extra inter-word space applied text justification.
	 */
	virtual int x2pos(docstring const & s, int & x, bool rtl, double ws) const = 0;

	// The places where to break a string and the width of the resulting lines.
	struct Break {
		Break(int l, Dimension const & d, int nsw) : len(l), dim(d), nspc_wid(nsw) {}
		// Number of characters
		int len = 0;
		// text dimension
		Dimension dim;
		// text width when trailing spaces are removed; only makes a
		// difference for the last break.
		int nspc_wid = 0;
	};
	typedef std::vector<Break> Breaks;
	/**
	 * Break a string in multiple fragments according to width limits.
	 * \return a sequence of Break elements.
	 * \param s is the string to break.
	 * \param first_wid is the available width for first line.
	 * \param wid is the available width for the next lines.
	 * \param rtl is true for right-to-left layout.
	 * \param force is false for breaking at word separator, true for
	 *   arbitrary position.
	 */
	virtual Breaks
	breakString(docstring const & s, int first_wid, int wid, bool rtl, bool force) const = 0;

	// return string dimension for the font.
	virtual Dimension const dimension(docstring const & str) const = 0;

	/// return char dimension for the font.
	virtual Dimension const dimension(char_type c) const = 0;
	/**
	 * fill in width,ascent,descent with the values for the
	 * given string in the font.
	 */
	virtual void rectText(docstring const & str,
		int & width,
		int & ascent,
		int & descent) const = 0;
	/**
	 * fill in width,ascent,descent with the values for the
	 * given string in the font for a button with given offset.
	 */
	virtual void buttonText(docstring const & str,
		const int offset,
		int & width,
		int & ascent,
		int & descent) const = 0;
};


} // namespace frontend

class Font;
class FontInfo;

/// Implementation is in Application.cpp

LYX_BEGIN_MUTE_GCC_WARNING(dangling-reference)
frontend::FontMetrics const & theFontMetrics(Font const & f);
frontend::FontMetrics const & theFontMetrics(FontInfo const & fi);
LYX_END_MUTE_GCC_WARNING


} // namespace lyx

#endif // FONT_METRICS_H
