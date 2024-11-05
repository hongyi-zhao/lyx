// -*- C++ -*-
/**
 * \file Box.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef BOX_H
#define BOX_H

#include <ostream>


namespace lyx {

/**
 * A simple class representing rectangular regions.
 * It is expected that the box be constructed in
 * normalised form, that is to say : x1,y1 is top-left,
 * x2,y2 is bottom-right.
 *
 * Negative values are allowed.
 */
class Box {
public:
	int x1 = 0;
	int x2 = 0;
	int y1 = 0;
	int y2 = 0;

	/// Zero-initialise the member variables.
	Box() {}
	/// Initialise the member variables.
	Box(int x1_, int x2_, int y1_, int y2_) : x1(x1_), x2(x2_), y1(y1_), y2(y2_) {}

	/**
	 * Returns true if the given co-ordinates are within
	 * the box. Check is exclusive (point on a border
	 * returns false).
	 */
	bool contains(int x, int y) const { return (x1 < x && x2 > x && y1 < y && y2 > y); }

};


inline std::ostream & operator<<(std::ostream & os, Box const & b)
{
	return os << "x1,y1: " << b.x1 << ',' << b.y1
	          << " x2,y2: " << b.x2 << ',' << b.y2
	          << std::endl;
}



} // namespace lyx

#endif // BOX_H
