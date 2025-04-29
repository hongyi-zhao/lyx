// -*- C++ -*-
/**
 * \file MathData.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alejandro Aguilar Sierra
 * \author André Pönitz
 * \author Lars Gullik Bjønnes
 * \author Stefan Schimanski
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef MATH_DATA_H
#define MATH_DATA_H

#include "MathAtom.h"
#include "MathClass.h"

#include "OutputEnums.h"

#include <cstddef>
#include <vector>


namespace lyx {

class Buffer;
class BufferView;
class Cursor;
class Dimension;
class DocIterator;
class InsetMathMacro;
class LaTeXFeatures;
class MacroContext;
class MathRow;
class MetricsInfo;
class PainterInfo;
class ParIterator;
class ReplaceData;
class TextMetricsInfo;
class TextPainter;


class MathData : private std::vector<MathAtom> {
public:
	/// re-use inhertited stuff
	typedef std::vector<MathAtom> base_type;
	using base_type::const_iterator;
	using base_type::iterator;
	using base_type::size_type;
	using base_type::difference_type;
	using base_type::size;
	using base_type::empty;
	using base_type::clear;
	using base_type::begin;
	using base_type::end;
	using base_type::pop_back;
	using base_type::back;
	using base_type::front;
	///
	typedef size_type idx_type;
	typedef size_type pos_type;

public:
	///
	MathData() = delete;
	///
	explicit MathData(Buffer * buf) : buffer_(buf) {}
	///
	MathData(Buffer * buf, const_iterator from, const_iterator to);
	///
	Buffer * buffer() { return buffer_; }
	///
	Buffer const * buffer() const { return buffer_; }
	///
	void append(MathData const & md);

	/// inserts single atom at position pos
	void insert(size_type pos, MathAtom const & at);
	/// inserts multiple atoms at position pos
	void insert(size_type pos, MathData const & md);
	/// inserts single atom at end
	void push_back(MathAtom const & at);
	/// construct single atom at end
	void emplace_back(InsetMath * ins);

	/// erase range from pos1 to pos2
	void erase(iterator pos1, iterator pos2);
	/// erase single atom
	void erase(iterator pos);
	/// erase range from pos1 to pos2
	void erase(size_type pos1, size_type pos2);
	/// erase single atom
	void erase(size_type pos);

	///
	void dump() const;
	///
	void dump2() const;
	///
	void replace(ReplaceData &);
	///
	void substitute(MathData const & m);

	/// looks for exact match
	bool match(MathData const & md) const;
	/// looks for inclusion match starting at pos
	bool matchpart(MathData const & md, pos_type pos) const;
	/// looks for containment, return == size mean not found
	size_type find(MathData const & md) const;
	/// looks for containment, return == size mean not found
	size_type find_last(MathData const & md) const;
	///
	bool contains(MathData const & md) const;
	///
	void validate(LaTeXFeatures &) const;

	/// checked write access
	MathAtom & operator[](pos_type);
	/// checked read access
	MathAtom const & operator[](pos_type) const;

	/// Add this data to a math row. Return true if contents got added
	bool addToMathRow(MathRow &, MetricsInfo & mi) const;

	/// rebuild cached metrics information
	/** When \c tight is true, the height of the cell will be at least
	 *  the x height of the font. Otherwise, it will be the max height
	 *  of the font.
	 */
	void metrics(MetricsInfo & mi, Dimension & dim, bool tight = true) const;
	///
	Dimension const & dimension(BufferView const &) const;

	/// draw the selection over the cell
	void drawSelection(PainterInfo & pi, int x, int y) const;
	/// redraw cell using cache metrics information
	void draw(PainterInfo & pi, int x, int y) const;
	/// rebuild cached metrics information
	void metricsT(TextMetricsInfo const & mi, Dimension & dim) const;
	/// redraw cell using cache metrics information
	void drawT(TextPainter & pi, int x, int y) const;
	/// approximate mathclass of the data
	MathClass mathClass() const;
	/// math class of first interesting element
	MathClass firstMathClass() const;
	/// math class of last interesting element
	MathClass lastMathClass() const;
	/// is the cell in display style
	bool displayStyle() const { return display_style_; }

	/// access to cached x coordinate of last drawing
	int xo(BufferView const & bv) const;
	/// access to cached y coordinate of last drawing
	int yo(BufferView const & bv) const;
	/// access to cached x coordinate of mid point of last drawing
	int xm(BufferView const & bv) const;
	/// access to cached y coordinate of mid point of last drawing
	int ym(BufferView const & bv) const;
	/// write access to coordinate;
	void setXY(BufferView & bv, int x, int y) const;
	/// returns x coordinate of given position in the data
	int pos2x(BufferView const * bv, size_type pos) const;
	/// returns position of given x coordinate
	size_type x2pos(BufferView const * bv, int targetx) const;
	/// returns distance of this cell to the point given by x and y
	// assumes valid position and size cache
	int dist(BufferView const & bv, int x, int y) const;

	/// minimum ascent offset for superscript
	int minasc() const { return minasc_; }
	/// minimum descent offset for subscript
	int mindes() const { return mindes_; }
	/// level above/below which super/subscript should extend
	int slevel() const { return slevel_; }
	/// additional super/subscript shift
	int sshift() const { return sshift_; }
	/// Italic correction as described in InsetMathScript.h
	int kerning(BufferView const *) const;
	///
	void swap(MathData & md) { base_type::swap(md); }

	/// attach/detach arguments to macros, updating the cur to
	/// stay visually at the same position (cur==0 is allowed)
	void updateMacros(Cursor * cur, MacroContext const & mc, UpdateType, int nesting);
	///
	void updateBuffer(ParIterator const &, UpdateType, bool const deleted = false);
	/// Change associated buffer for this object and its contents
	void setBuffer(Buffer & b);
	/// Update assiociated buffer for the contents of the object
	void setContentsBuffer();

protected:
	/// cached values for super/subscript placement
	mutable int minasc_ = 0;
	mutable int mindes_ = 0;
	mutable int slevel_ = 0;
	mutable int sshift_ = 0;
	/// cached value for display style
	mutable bool display_style_ = false;
	Buffer * buffer_ = nullptr;

private:
	/// is this an exact match at this position?
	bool find1(MathData const & md, size_type pos) const;

	///
	void detachMacroParameters(DocIterator * dit, size_type macroPos);
	///
	void attachMacroParameters(Cursor * cur, size_type macroPos,
		size_type macroNumArgs, int macroOptionals,
		bool fromInitToNormalMode, bool interactiveInit,
		size_t appetite);
	///
	void collectOptionalParameters(Cursor * cur,
		size_type numOptionalParams, std::vector<MathData> & params,
		size_t & pos, MathAtom & scriptToPutAround,
		pos_type macroPos, pos_type thisPos, size_type thisSlice);
	///
	void collectParameters(Cursor * cur,
		size_type numParams, std::vector<MathData> & params,
		size_t & pos, MathAtom & scriptToPutAround,
		pos_type macroPos, pos_type thisPos, size_type thisSlice,
		size_t appetite);
};

///
std::ostream & operator<<(std::ostream & os, MathData const & md);
///
odocstream & operator<<(odocstream & os, MathData const & md);


} // namespace lyx

#endif
