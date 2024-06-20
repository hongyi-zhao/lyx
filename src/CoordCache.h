// -*- C++ -*-
/* \file CoordCache.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef COORDCACHE_H
#define COORDCACHE_H

// It seems that MacOSX define the check macro.
#undef check

#include "Dimension.h"

#include <unordered_map>

namespace lyx {

class Inset;
class MathData;

void lyxbreaker(void const * data, const char * hint, int size);

struct Geometry {
	Point pos = {-10000, -10000 };
	Dimension dim;

	bool covers(int x, int y) const
	{
		return x >= pos.x
			&& x <= pos.x + dim.wid
			&& y >= pos.y - dim.asc
			&& y <= pos.y + dim.des;
	}

	int squareDistance(int x, int y) const
	{
		int xx = 0;
		int yy = 0;

		if (x < pos.x)
			xx = pos.x - x;
		else if (x > pos.x + dim.wid)
			xx = x - pos.x - dim.wid;

		if (y < pos.y - dim.asc)
			yy = pos.y - dim.asc - y;
		else if (y > pos.y + dim.des)
			yy = y - pos.y - dim.des;

		// Optimisation: We avoid to compute the sqrt on purpose.
		return xx*xx + yy*yy;
	}
};


template <class T> class CoordCacheBase {
public:
	void clear()
	{
		data_.clear();
	}

	bool empty() const
	{
		return data_.empty();
	}

	void add(T const * thing, int x, int y)
	{
		data_[thing].pos = Point(x, y);
	}

	void add(T const * thing, Dimension const & dim)
	{
		data_[thing].dim = dim;
	}

	Geometry & geometry(T const * thing)
	{
		checkDim(thing, "geometry");
		return data_.find(thing)->second;
	}

	Geometry const & geometry(T const * thing) const
	{
		checkDim(thing, "geometry");
		return data_.find(thing)->second;
	}

	Dimension const & dim(T const * thing) const
	{
		checkDim(thing, "dim");
		return data_.find(thing)->second.dim;
	}

	int x(T const * thing) const
	{
		check(thing, "x");
		return data_.find(thing)->second.pos.x;
	}

	int y(T const * thing) const
	{
		check(thing, "y");
		return data_.find(thing)->second.pos.y;
	}

	Point xy(T const * thing) const
	{
		check(thing, "xy");
		return data_.find(thing)->second.pos;
	}

	bool has(T const * thing) const
	{
		typename cache_type::const_iterator it = data_.find(thing);

		if (it == data_.end())
			return false;
		return it->second.pos.x != -10000;
	}

	bool hasDim(T const * thing) const
	{
		return data_.find(thing) != data_.end();
	}

	bool covers(T const * thing, int x, int y) const
	{
		typename cache_type::const_iterator it = data_.find(thing);
		return it != data_.end() && it->second.covers(x, y);
	}

	int squareDistance(T const * thing, int x, int y) const
	{
		typename cache_type::const_iterator it = data_.find(thing);
		if (it == data_.end())
			return 1000000;
		return it->second.squareDistance(x, y);
	}

private:
	friend class CoordCache;

#ifdef ENABLE_ASSERTIONS
	void checkDim(T const * thing, char const * hint) const
	{
		if (!hasDim(thing))
			lyxbreaker(thing, hint, data_.size());
	}

	void check(T const * thing, char const * hint) const
	{
		if (!has(thing))
			lyxbreaker(thing, hint, data_.size());
	}
#else
	void checkDim(T const *, char const * const ) const {}

	void check(T const *, char const *) const {}
#endif

	typedef std::unordered_map<T const *, Geometry> cache_type;
	cache_type data_;
};

/**
 * A BufferView dependent cache that allows us to come from an inset in
 * a document to a position point and dimension on the screen.
 * All points cached in this cache are only valid between subsequent
 * updates. (x,y) == (0,0) is the upper left screen corner, x increases
 * to the right, y increases downwords.
 * The dimension part is built in BufferView::updateMetrics() and the
 * diverse Inset::metrics() calls.
 * The individual points are added at drawing time in
 * BufferView::draw(). The math inset position are cached in
 * the diverse InsetMathXXX::draw() calls and the in-text inset position
 * are cached in RowPainter::paintInset().
 * FIXME: For mathed, it would be nice if the insets did not saves their
 * position themselves. That should be the duty of the containing math
 * array.
 */
class CoordCache {
public:
	void clear();

	/// A map from MathData to position on the screen
	typedef CoordCacheBase<MathData> Arrays;
	Arrays & arrays() { return arrays_; }
	Arrays const & getArrays() const { return arrays_; }
	/// A map from insets to positions on the screen
	typedef CoordCacheBase<Inset> Insets;
	Insets & insets() { return insets_; }
	Insets const & getInsets() const { return insets_; }

	/// Dump the contents of the cache to lyxerr in debugging form
	void dump() const;
private:
	/// MathDatas
	Arrays arrays_;
	// All insets
	Insets insets_;
};

} // namespace lyx

#endif
