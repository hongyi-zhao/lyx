/**
 * \file InsetMathGrid.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>
#include <algorithm>

#include "InsetMathGrid.h"

#include "InsetMathUnknown.h"
#include "MathData.h"
#include "MathParser.h"
#include "MathStream.h"
#include "MetricsInfo.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "CoordCache.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "InsetMathIntertext.h"
#include "LaTeXFeatures.h"
#include "MathFactory.h"
#include "TexRow.h"

#include "frontends/alert.h"
#include "frontends/Clipboard.h"
#include "frontends/Painter.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/gettext.h"
#include "support/lstrings.h"
#include "support/lassert.h"

#include <sstream>

using namespace std;
using namespace lyx::support;



namespace lyx {

static docstring verboseHLine(int n)
{
	docstring res;
	for (int i = 0; i < n; ++i)
		res += "\\hline";
	if (n)
		res += ' ';
	return res;
}


// read a number to be used as an iteration count (limited arbitrary to 1000)
static int extractInt(istream & is)
{
	int num = 1;
	is >> num;
	return min(max(num, 1), 1000);
}


static void resetGrid(InsetMathGrid & grid)
{
	while (grid.ncols() > 1)
		grid.delCol(grid.ncols() - 1);
	while (grid.nrows() > 1)
		grid.delRow(grid.nrows() - 1);
	grid.cell(0).erase(0, grid.cell(0).size());
	grid.setDefaults();
}


//////////////////////////////////////////////////////////////


int InsetMathGrid::RowInfo::skipPixels(MetricsInfo const & mi) const
{
	return mi.base.inPixels(crskip);
}


//////////////////////////////////////////////////////////////


InsetMathGrid::InsetMathGrid(Buffer * buf)
	: InsetMathNest(buf, 1),
	  rowinfo_(1 + 1),
		colinfo_(1 + 1),
		cellinfo_(1),
		v_align_('c')
{
	setDefaults();
}


InsetMathGrid::InsetMathGrid(Buffer * buf, col_type m, row_type n)
	: InsetMathNest(buf, m * n),
	  rowinfo_(n + 1),
		colinfo_(m + 1),
		cellinfo_(m * n),
		v_align_('c')
{
	setDefaults();
}


InsetMathGrid::InsetMathGrid(Buffer * buf, col_type m, row_type n, char v,
	docstring const & h)
	: InsetMathNest(buf, m * n),
	  rowinfo_(n + 1),
	  colinfo_(m + 1),
		cellinfo_(m * n),
		v_align_(v)
{
	setDefaults();
	setVerticalAlignment(v);
	setHorizontalAlignments(h);
}


Inset * InsetMathGrid::clone() const
{
	return new InsetMathGrid(*this);
}


idx_type InsetMathGrid::index(row_type row, col_type col) const
{
	return col + ncols() * row;
}


void InsetMathGrid::setDefaults()
{
	if (ncols() <= 0)
		lyxerr << "positive number of columns expected" << endl;
	//if (nrows() <= 0)
	//	lyxerr << "positive number of rows expected" << endl;
	for (col_type col = 0; col < ncols(); ++col) {
		colinfo_[col].align = defaultColAlign(col);
		colinfo_[col].skip  = defaultColSpace(col);
		colinfo_[col].special.clear();
	}
	for (idx_type idx = 0; idx < nargs(); ++idx)
		cellinfo_[idx].multi = CELL_NORMAL;
}


bool InsetMathGrid::interpretString(Cursor & cur, docstring const & str)
{
	if (str == "\\hline") {
		FuncRequest fr = FuncRequest(LFUN_TABULAR_FEATURE, "add-hline-above");
		FuncStatus status;
		if (getStatus(cur, fr, status)) {
			if (status.enabled()) {
				rowinfo_[cur.row()].lines++;
				return true;
			}
		}
	}
	return InsetMathNest::interpretString(cur, str);
}


void InsetMathGrid::setHorizontalAlignments(docstring const & hh)
{
	col_type col = 0;
	for (docstring::const_iterator it = hh.begin(); it != hh.end(); ++it) {
		char_type c = *it;
		if (c == '|') {
			colinfo_[col].lines++;
		} else if ((c == 'p' || c == 'm' || c == 'b'||
		            c == '!' || c == '@' || c == '>' || c == '<') &&
		           it + 1 != hh.end() && *(it + 1) == '{') {
			// @{decl.} and p{width} are standard LaTeX, the
			// others are extensions by array.sty
			bool const newcolumn = c == 'p' || c == 'm' || c == 'b';
			if (newcolumn) {
				// this declares a new column
				if (col >= ncols())
					// Only intercolumn stuff is allowed
					// in the last dummy column
					break;
				colinfo_[col].align = 'l';
			} else {
				// this is intercolumn stuff
				if (colinfo_[col].special.empty())
					// Overtake possible lines
					colinfo_[col].special = docstring(colinfo_[col].lines, '|');
			}
			int brace_open = 0;
			int brace_close = 0;
			while (it != hh.end()) {
				c = *it;
				colinfo_[col].special += c;
				if (c == '{')
					++brace_open;
				else if (c == '}')
					++brace_close;
				++it;
				if (brace_open > 0 && brace_open == brace_close)
					break;
			}
			--it;
			if (newcolumn) {
				colinfo_[col].lines = count(
					colinfo_[col].special.begin(),
					colinfo_[col].special.end(), '|');
				LYXERR(Debug::MATHED, "special column separator: `"
					<< to_utf8(colinfo_[col].special) << '\'');
				++col;
				colinfo_[col].lines = 0;
				colinfo_[col].special.clear();
			}
		} else if (col >= ncols()) {
			// Only intercolumn stuff is allowed in the last
			// dummy column
			break;
		} else if (c == 'c' || c == 'l' || c == 'r') {
			colinfo_[col].align = static_cast<char>(c);
			if (!colinfo_[col].special.empty()) {
				colinfo_[col].special += c;
				colinfo_[col].lines = count(
						colinfo_[col].special.begin(),
						colinfo_[col].special.end(), '|');
				LYXERR(Debug::MATHED, "special column separator: `"
					<< to_utf8(colinfo_[col].special) << '\'');
			}
			++col;
			colinfo_[col].lines = 0;
			colinfo_[col].special.clear();
		} else {
			lyxerr << "unknown column separator: '" << c << "'" << endl;
		}
	}

/*
	col_type n = hh.size();
	if (n > ncols())
		n = ncols();
	for (col_type col = 0; col < n; ++col)
		colinfo_[col].align = hh[col];
*/
}


col_type InsetMathGrid::guessColumns(docstring const & hh)
{
	col_type col = 0;
	for (char_type const c : hh)
		if (c == 'c' || c == 'l' || c == 'r'||
		    c == 'p' || c == 'm' || c == 'b')
			++col;
	// let's have at least one column, even if we did not recognize its
	// alignment
	if (col == 0)
		col = 1;
	return col;
}


void InsetMathGrid::setHorizontalAlignment(char h, col_type col)
{
	colinfo_[col].align = h;
	if (!colinfo_[col].special.empty()) {
		char_type & c = colinfo_[col].special[colinfo_[col].special.size() - 1];
		if (c == 'l' || c == 'c' || c == 'r')
			c = h;
	}
	// FIXME: Change alignment of p, m and b columns, too
}


char InsetMathGrid::horizontalAlignment(col_type col) const
{
	return colinfo_[col].align;
}


docstring InsetMathGrid::horizontalAlignments() const
{
	docstring res;
	for (col_type col = 0; col < ncols(); ++col) {
		if (colinfo_[col].special.empty()) {
			res += docstring(colinfo_[col].lines, '|');
			res += colinfo_[col].align;
		} else
			res += colinfo_[col].special;
	}
	if (colinfo_[ncols()].special.empty())
		return res + docstring(colinfo_[ncols()].lines, '|');
	return res + colinfo_[ncols()].special;
}


void InsetMathGrid::setVerticalAlignment(char c)
{
	v_align_ = c;
}


char InsetMathGrid::verticalAlignment() const
{
	return v_align_;
}


col_type InsetMathGrid::ncols() const
{
	// the array is never empty and there is an extra column for last vlines.
	LATTEST(colinfo_.size() > 1);
	return colinfo_.size() - 1;
}


row_type InsetMathGrid::nrows() const
{
	// the array is never empty and there is an extra row for last hlines.
	LATTEST(rowinfo_.size() > 1);
	return rowinfo_.size() - 1;
}


col_type InsetMathGrid::col(idx_type idx) const
{
	return idx % ncols();
}


row_type InsetMathGrid::row(idx_type idx) const
{
	return idx / ncols();
}


col_type InsetMathGrid::ncellcols(idx_type idx) const
{
	col_type cols = 1;
	if (cellinfo_[idx].multi == CELL_NORMAL)
		return cols;
	// If the cell at idx is already CELL_PART_OF_MULTICOLUMN we return
	// the number of remaining columns, not the ones of the complete
	// multicolumn cell. This makes it possible to always go to the next
	// cell with idx + ncellcols(idx) - 1.
	row_type const r = row(idx);
	while (idx+cols < nargs() && row(idx+cols) == r &&
	       cellinfo_[idx+cols].multi == CELL_PART_OF_MULTICOLUMN)
		cols++;
	return cols;
}


void InsetMathGrid::vcrskip(Length const & crskip, row_type row)
{
	rowinfo_[row].crskip = crskip;
}


Length InsetMathGrid::vcrskip(row_type row) const
{
	return rowinfo_[row].crskip;
}


void InsetMathGrid::metrics(MetricsInfo & mi, Dimension & dim) const
{
	// let the cells adjust themselves
	for (idx_type i = 0; i < nargs(); ++i) {
		if (cellinfo_[i].multi != CELL_PART_OF_MULTICOLUMN) {
			Dimension dimc;
			// the 'false' is to make sure that the cell is tall enough
			cell(i).metrics(mi, dimc, false);
		}
	}

	BufferView & bv = *mi.base.bv;

	// compute absolute sizes of vertical structure
	for (row_type row = 0; row < nrows(); ++row) {
		int asc  = 0;
		int desc = 0;
		for (col_type col = 0; col < ncols(); ++col) {
			idx_type const i = index(row, col);
			if (cellinfo_[i].multi != CELL_PART_OF_MULTICOLUMN) {
				Dimension const & dimc = cell(i).dimension(bv);
				asc  = max(asc,  dimc.asc);
				desc = max(desc, dimc.des);
			}
		}
		rowinfo_[row].ascent  = asc;
		rowinfo_[row].descent = desc;
	}
	rowinfo_[nrows()].ascent  = 0;
	rowinfo_[nrows()].descent = 0;

	// compute vertical offsets
	rowinfo_[0].offset[&bv] = 0;
	for (row_type row = 1; row <= nrows(); ++row) {
		rowinfo_[row].offset[&bv] =
			rowinfo_[row - 1].offset[&bv] +
			rowinfo_[row - 1].descent +
			rowinfo_[row - 1].skipPixels(mi) +
			rowsep() +
			rowinfo_[row].lines * hlinesep() +
			rowinfo_[row].ascent;
	}

	// adjust vertical offset
	int h = 0;
	switch (v_align_) {
		case 't':
			h = 0;
			break;
		case 'b':
			h = rowinfo_[nrows() - 1].offset[&bv];
			break;
		default:
			h = rowinfo_[nrows() - 1].offset[&bv] / 2;
	}
	for (row_type row = 0; row <= nrows(); ++row)
		rowinfo_[row].offset[&bv] -= h;


	// multicolumn cell widths, as a map from first column to width in a
	// vector of last columns.
	// This is only used if the grid has more than one row, since for
	// one-row grids multicolumn cells do not need special handling
	vector<map<col_type, int> > mcolwidths(ncols());

	// compute absolute sizes of horizontal structure
	for (col_type col = 0; col < ncols(); ++col) {
		int wid = 0;
		for (row_type row = 0; row < nrows(); ++row) {
			idx_type const i = index(row, col);
			if (cellinfo_[i].multi != CELL_PART_OF_MULTICOLUMN) {
				int const w = cell(i).dimension(bv).wid;
				col_type const cols = ncellcols(i);
				if (cols > 1 && nrows() > 1) {
					col_type last = col+cols-1;
					LASSERT(last < ncols(), last = ncols()-1);
					map<col_type, int>::iterator it =
						mcolwidths[last].find(col);
					if (it == mcolwidths[last].end())
						mcolwidths[last][col] = w;
					else
						it->second = max(it->second, w);
				} else
					wid = max(wid, w);
			}
		}
		colinfo_[col].width = wid;
	}
	colinfo_[ncols()].width  = 0;

	// compute horizontal offsets
	colinfo_[0].offset = border() + colinfo_[0].lines * vlinesep();
	for (col_type col = 1; col <= ncols(); ++col) {
		colinfo_[col].offset =
			colinfo_[col - 1].offset +
			colinfo_[col - 1].width +
			displayColSpace(col - 1) +
			colsep() +
			colinfo_[col].lines * vlinesep();
	}

	// increase column widths for multicolumn cells if needed
	// FIXME: multicolumn lines are not yet considered
	for (col_type last = 0; last < ncols(); ++last) {
		map<col_type, int> const & widths = mcolwidths[last];
		// We increase the width of the last column of the multicol
		// cell (some sort of left alignment). Since we iterate through
		// the last and the first columns from left to right, we ensure
		// that increased widths of previous columns are correctly
		// taken into account for later columns, thus preventing
		// unneeded width increasing.
		for (map<col_type, int>::const_iterator it = widths.begin();
		     it != widths.end(); ++it) {
			int const wid = it->second;
			col_type const first = it->first;
			int const nextoffset =
				colinfo_[first].offset +
				wid +
				displayColSpace(last) +
				colsep() +
				colinfo_[last+1].lines * vlinesep();
			int const dx = nextoffset - colinfo_[last+1].offset;
			if (dx > 0) {
				colinfo_[last].width += dx;
				for (col_type col = last + 1; col <= ncols(); ++col)
					colinfo_[col].offset += dx;
			}
		}
	}


	dim.wid = colinfo_[ncols() - 1].offset
		+ colinfo_[ncols() - 1].width
		+ vlinesep() * colinfo_[ncols()].lines
		+ border();

	dim.asc = - rowinfo_[0].offset[&bv]
		+ rowinfo_[0].ascent
		+ hlinesep() * rowinfo_[0].lines
		+ border();

	dim.des = rowinfo_[nrows() - 1].offset[&bv]
		+ rowinfo_[nrows() - 1].descent
		+ hlinesep() * rowinfo_[nrows()].lines
		+ border() + 1;


/*
	// Increase ws_[i] for 'R' columns (except the first one)
	for (int i = 1; i < nc_; ++i)
		if (align[i] == 'R')
			ws_[i] += 10 * df_width;
	// Increase ws_[i] for 'C' column
	if (align[0] == 'C')
		if (ws_[0] < 7 * workwidth / 8)
			ws_[0] = 7 * workwidth / 8;

	// Adjust local tabs
	width = colsep();
	for (cxrow = row_.begin(); cxrow; ++cxrow) {
		int rg = COLSEP;
		int lf = 0;
		for (int i = 0; i < nc_; ++i) {
			bool isvoid = false;
			if (cxrow->getTab(i) <= 0) {
				cxrow->setTab(i, df_width);
				isvoid = true;
			}
			switch (align[i]) {
			case 'l':
				lf = 0;
				break;
			case 'c':
				lf = (ws_[i] - cxrow->getTab(i))/2;
				break;
			case 'r':
			case 'R':
				lf = ws_[i] - cxrow->getTab(i);
				break;
			case 'C':
				if (cxrow == row_.begin())
					lf = 0;
				else if (cxrow.is_last())
					lf = ws_[i] - cxrow->getTab(i);
				else
					lf = (ws_[i] - cxrow->getTab(i))/2;
				break;
			}
			int const ww = (isvoid) ? lf : lf + cxrow->getTab(i);
			cxrow->setTab(i, lf + rg);
			rg = ws_[i] - ww + colsep();
			if (cxrow == row_.begin())
				width += ws_[i] + colsep();
		}
		cxrow->setBaseline(cxrow->getBaseline() - ascent);
	}
*/
	dim.wid += leftMargin() + rightMargin();
}


int InsetMathGrid::vLineHOffset(col_type col, unsigned int line) const
{
	if (col < ncols())
		return leftMargin() + colinfo_[col].offset
			- (colinfo_[col].lines - line - 1) * vlinesep()
			- vlinesep()/2 - colsep()/2;
	else {
		LASSERT(col == ncols(), return 0);
		return leftMargin() + colinfo_[col-1].offset + colinfo_[col-1].width
			+ line * vlinesep()
			+ vlinesep()/2 + colsep()/2;
	}
}


int InsetMathGrid::hLineVOffset(BufferView const & bv, row_type row,
                                unsigned int line) const
{
	return rowinfo_[row].offset[&bv]
		- rowinfo_[row].ascent
		- line * hlinesep()
		- hlinesep()/2 - rowsep()/2;
}


void InsetMathGrid::draw(PainterInfo & pi, int x, int y) const
{
	BufferView const & bv = *pi.base.bv;

	for (idx_type idx = 0; idx < nargs(); ++idx) {
		if (cellinfo_[idx].multi != CELL_PART_OF_MULTICOLUMN) {
			cell(idx).draw(pi,
			               x + leftMargin() + cellXOffset(bv, idx),
			               y + cellYOffset(bv, idx));

			row_type r = row(idx);
			int const yy1 = y + hLineVOffset(bv, r, 0);
			int const yy2 = y + hLineVOffset(bv, r + 1, rowinfo_[r + 1].lines - 1);
			auto draw_left_borders = [&](col_type c) {
				for (unsigned int i = 0; i < colinfo_[c].lines; ++i) {
					int const xx = x + vLineHOffset(c, i);
					pi.pain.line(xx, yy1, xx, yy2, Color_foreground);
				}
			};
			col_type c = col(idx);
			// Draw inner left borders cell-by-cell because of multicolumns
			draw_left_borders(c);
			// Draw the right border (only once)
			if (c == 0)
				draw_left_borders(ncols());
		}
	}

	// Draw horizontal borders
	for (row_type r = 0; r <= nrows(); ++r) {
		int const xx1 = x + vLineHOffset(0, 0);
		int const xx2 = x + vLineHOffset(ncols(), colinfo_[ncols()].lines - 1);
		for (unsigned int i = 0; i < rowinfo_[r].lines; ++i) {
			int const yy = y + hLineVOffset(bv, r, i);
			pi.pain.line(xx1, yy, xx2, yy, Color_foreground);
		}
	}
}


void InsetMathGrid::metricsT(TextMetricsInfo const & /*mi*/, Dimension & /*dim*/) const
{
// FIXME: this does not compile anymore with offset being a map
// It is not worth fixing it at this point since the code is basically dead.
#if 0
	// let the cells adjust themselves
	for (idx_type i = 0; i < nargs(); ++i)
		if (cellinfo_[i].multi != CELL_PART_OF_MULTICOLUMN)
			cell(i).metricsT(mi, dim);

	// compute absolute sizes of vertical structure
	for (row_type row = 0; row < nrows(); ++row) {
		int asc  = 0;
		int desc = 0;
		for (col_type col = 0; col < ncols(); ++col) {
			idx_type const i = index(row, col);
			if (cellinfo_[i].multi != CELL_PART_OF_MULTICOLUMN) {
				//MathData const & c = cell(i);
				// FIXME: BROKEN!
				Dimension dimc;
				asc  = max(asc,  dimc.ascent());
				desc = max(desc, dimc.descent());
			}
		}
		rowinfo_[row].ascent  = asc;
		rowinfo_[row].descent = desc;
	}
	rowinfo_[nrows()].ascent  = 0;
	rowinfo_[nrows()].descent = 0;

	// compute vertical offsets
	rowinfo_[0].offset[&bv] = 0;
	for (row_type row = 1; row <= nrows(); ++row) {
		rowinfo_[row].offset[&bv]  =
			rowinfo_[row - 1].offset  +
			rowinfo_[row - 1].descent +
			//rowinfo_[row - 1].skipPixels(mi) +
			1 + //rowsep() +
			//rowinfo_[row].lines * hlinesep() +
			rowinfo_[row].ascent;
	}

	// adjust vertical offset
	int h = 0;
	switch (v_align_) {
		case 't':
			h = 0;
			break;
		case 'b':
			h = rowinfo_[nrows() - 1].offset;
			break;
		default:
			h = rowinfo_[nrows() - 1].offset / 2;
	}
	for (row_type row = 0; row <= nrows(); ++row)
		rowinfo_[row].offset -= h;


	// compute absolute sizes of horizontal structure
	for (col_type col = 0; col < ncols(); ++col) {
		int wid = 0;
		for (row_type row = 0; row < nrows(); ++row) {
			// FIXME: BROKEN!
			//idx_type const i = index(row, col);
			//if (cellinfo_[i].multi != CELL_PART_OF_MULTICOLUMN)
			//	wid = max(wid, cell(i).width());
		}
		colinfo_[col].width = wid;
	}
	colinfo_[ncols()].width  = 0;

	// compute horizontal offsets
	colinfo_[0].offset = border();
	for (col_type col = 1; col <= ncols(); ++col) {
		colinfo_[col].offset =
			colinfo_[col - 1].offset +
			colinfo_[col - 1].width +
			displayColSpace(col - 1) +
			1 ; //colsep() +
			//colinfo_[col].lines * vlinesep();
	}


	dim.wid  =  colinfo_[ncols() - 1].offset
		       + colinfo_[ncols() - 1].width
		 //+ vlinesep() * colinfo_[ncols()].lines
		       + 2;

	dim.asc  = -rowinfo_[0].offset
		       + rowinfo_[0].ascent
		 //+ hlinesep() * rowinfo_[0].lines
		       + 1;

	dim.des  =  rowinfo_[nrows() - 1].offset
		       + rowinfo_[nrows() - 1].descent
		 //+ hlinesep() * rowinfo_[nrows()].lines
		       + 1;
#endif
}


void InsetMathGrid::drawT(TextPainter & /*pain*/, int /*x*/, int /*y*/) const
{
//	for (idx_type idx = 0; idx < nargs(); ++idx)
//		if (cellinfo_[idx].multi != CELL_PART_OF_MULTICOLUMN)
//			cell(idx).drawT(pain, x + cellXOffset(idx), y + cellYOffset(idx));
}


void InsetMathGrid::updateBuffer(ParIterator const & it, UpdateType utype, bool const deleted)
{
	// pass down
	for (idx_type idx = 0; idx < nargs(); ++idx)
		if (cellinfo_[idx].multi != CELL_PART_OF_MULTICOLUMN)
			cell(idx).updateBuffer(it, utype, deleted);
}


void InsetMathGrid::eol(TeXMathStream & os, row_type row, bool fragile,
		bool /*latex*/, bool last_eoln) const
{
	docstring eol;

	if (!rowinfo_[row].crskip.zero())
		eol += '[' + from_utf8(rowinfo_[row].crskip.asLatexString()) + ']';
	else if(!rowinfo_[row].allow_newpage)
		eol += '*';

	// make sure an upcoming '[' does not break anything
	if (row + 1 < nrows()) {
		MathData const & c = cell(index(row + 1, 0));
		if (!c.empty() && c.front()->getChar() == '[')
			//eol += "[0pt]";
			eol += "{}";
	}

	// only add \\ if necessary
	if (eol.empty() && row + 1 == nrows() && (nrows() == 1 || !last_eoln))
		return;

	os << (fragile ? "\\protect\\\\" : "\\\\") << eol;
}


docstring InsetMathGrid::eocString(col_type col, col_type lastcol) const
{
	if (col + 1 == lastcol)
		return docstring();
	return from_ascii(" & ");
}


void InsetMathGrid::addRow(row_type row)
{
	rowinfo_.insert(rowinfo_.begin() + row + 1, RowInfo());
	cells_.insert
		(cells_.begin() + (row + 1) * ncols(), ncols(), MathData(buffer_));
	cellinfo_.insert
		(cellinfo_.begin() + (row + 1) * ncols(), ncols(), CellInfo());
}


void InsetMathGrid::delRow(row_type row)
{
	if (nrows() == 1)
		return;

	cells_type::iterator it = cells_.begin() + row * ncols();
	cells_.erase(it, it + ncols());

	vector<CellInfo>::iterator jt = cellinfo_.begin() + row * ncols();
	cellinfo_.erase(jt, jt + ncols());

	rowinfo_.erase(rowinfo_.begin() + row);
}


void InsetMathGrid::copyRow(row_type row)
{
	addRow(row);
	for (col_type col = 0; col < ncols(); ++col) {
		cells_[(row + 1) * ncols() + col] = cells_[row * ncols() + col];
		// copying the cell does not set the buffer
		cells_[(row + 1) * ncols() + col].setBuffer(*buffer_);
	}
}


void InsetMathGrid::swapRow(row_type row)
{
	if (nrows() == 1)
		return;
	if (row + 1 == nrows())
		--row;
	for (col_type col = 0; col < ncols(); ++col)
		swap(cells_[row * ncols() + col], cells_[(row + 1) * ncols() + col]);
}


void InsetMathGrid::addCol(col_type newcol)
{
	const col_type nc = ncols();
	const row_type nr = nrows();
	cells_type new_cells((nc + 1) * nr, MathData(buffer_));
	vector<CellInfo> new_cellinfo((nc + 1) * nr);

	for (row_type row = 0; row < nr; ++row)
		for (col_type col = 0; col < nc; ++col) {
			new_cells[row * (nc + 1) + col + (col >= newcol)]
				= cells_[row * nc + col];
			new_cellinfo[row * (nc + 1) + col + (col >= newcol)]
				= cellinfo_[row * nc + col];
		}
	swap(cells_, new_cells);
	// copying cells loses the buffer reference
	setBuffer(*buffer_);
	swap(cellinfo_, new_cellinfo);

	ColInfo inf;
	inf.skip  = defaultColSpace(newcol);
	inf.align = defaultColAlign(newcol);
	colinfo_.insert(colinfo_.begin() + newcol, inf);
}


void InsetMathGrid::delCol(col_type col)
{
	if (ncols() == 1)
		return;

	cells_type tmpcells;
	vector<CellInfo> tmpcellinfo;
	for (col_type i = 0; i < nargs(); ++i)
		if (i % ncols() != col) {
			tmpcells.push_back(cells_[i]);
			tmpcellinfo.push_back(cellinfo_[i]);
		}
	swap(cells_, tmpcells);
	// copying cells loses the buffer reference
	setBuffer(*buffer_);
	swap(cellinfo_, tmpcellinfo);

	colinfo_.erase(colinfo_.begin() + col);
}


void InsetMathGrid::copyCol(col_type col)
{
	addCol(col+1);
	for (row_type row = 0; row < nrows(); ++row) {
		cells_[row * ncols() + col + 1] = cells_[row * ncols() + col];
		// copying the cell does not set the buffer
		cells_[row * ncols() + col + 1].setBuffer(*buffer_);
	}
}


void InsetMathGrid::swapCol(col_type col)
{
	if (ncols() == 1)
		return;
	if (col + 1 == ncols())
		--col;
	for (row_type row = 0; row < nrows(); ++row)
		swap(cells_[row * ncols() + col], cells_[row * ncols() + col + 1]);
}


int InsetMathGrid::cellXOffset(BufferView const & bv, idx_type idx) const
{
	if (cellinfo_[idx].multi == CELL_PART_OF_MULTICOLUMN)
		return 0;
	col_type c = col(idx);
	int x = colinfo_[c].offset;
	char align = displayColAlign(idx);
	Dimension const & celldim = cell(idx).dimension(bv);
	if (align == 'r' || align == 'R')
		x += cellWidth(idx) - celldim.wid;
	if (align == 'c' || align == 'C')
		x += (cellWidth(idx) - celldim.wid) / 2;
	return x;
}


int InsetMathGrid::cellYOffset(BufferView const & bv, idx_type idx) const
{
	return rowinfo_[row(idx)].offset[&bv];
}


int InsetMathGrid::cellWidth(idx_type idx) const
{
	switch (cellinfo_[idx].multi) {
	case CELL_NORMAL:
		return colinfo_[col(idx)].width;
	case CELL_BEGIN_OF_MULTICOLUMN: {
		col_type c1 = col(idx);
		col_type c2 = c1 + ncellcols(idx);
		return colinfo_[c2].offset
			- colinfo_[c1].offset
			- displayColSpace(c2)
			- colsep()
			- colinfo_[c2].lines * vlinesep();
	}
	case CELL_PART_OF_MULTICOLUMN:
		return 0;
	}
	return 0;
}


bool InsetMathGrid::idxUpDown(Cursor & cur, bool up) const
{
	if (up) {
		if (cur.row() == 0)
			return false;
		cur.idx() -= ncols();
	} else {
		if (cur.row() + 1 >= nrows())
			return false;
		cur.idx() += ncols();
	}
	// If we are in a multicolumn cell, move to the "real" cell
	while (cellinfo_[cur.idx()].multi == CELL_PART_OF_MULTICOLUMN) {
		LASSERT(cur.idx() > 0, return false);
		--cur.idx();
	}
	// FIXME: this is only a workaround to avoid a crash if the inset
	// in not in coord cache. The best would be to force a FitCursor
	// operation.
	CoordCache::Cells const & cellsCache = cur.bv().coordCache().cells();
	if (cellsCache.has(&cur.cell()))
		cur.pos() = cur.cell().x2pos(&cur.bv(), cur.x_target() - cur.cell().xo(cur.bv()));
	else
		cur.pos() = 0;
	return true;
}


bool InsetMathGrid::idxBackward(Cursor & cur) const
{
	// leave matrix if at the front edge
	if (cur.col() == 0)
		return false;
	--cur.idx();
	// If we are in a multicolumn cell, move to the "real" cell
	while (cellinfo_[cur.idx()].multi == CELL_PART_OF_MULTICOLUMN) {
		LASSERT(cur.idx() > 0, return false);
		--cur.idx();
	}
	cur.pos() = cur.lastpos();
	return true;
}


bool InsetMathGrid::idxForward(Cursor & cur) const
{
	// leave matrix if at the back edge
	if (cur.col() + 1 == ncols())
		return false;
	++cur.idx();
	// If we are in a multicolumn cell, move to the next cell
	while (cellinfo_[cur.idx()].multi == CELL_PART_OF_MULTICOLUMN) {
		// leave matrix if at the back edge
		if (cur.col() + 1 == ncols())
			return false;
		++cur.idx();
	}
	cur.pos() = 0;
	return true;
}


idx_type InsetMathGrid::firstIdx() const
{
	size_type idx = 0;
	switch (v_align_) {
		case 't':
			//idx = 0;
			break;
		case 'b':
			idx = (nrows() - 1) * ncols();
			break;
		default:
			idx = ((nrows() - 1) / 2) * ncols();
	}
	// If we are in a multicolumn cell, move to the "real" cell
	while (cellinfo_[idx].multi == CELL_PART_OF_MULTICOLUMN) {
		LASSERT(idx > 0, return 0);
		--idx;
	}
	return idx;
}


idx_type InsetMathGrid::lastIdx() const
{
	size_type idx = 0;
	switch (v_align_) {
		case 't':
			idx = ncols() - 1;
			break;
		case 'b':
			idx = nargs() - 1;
			break;
		default:
			idx = ((nrows() - 1) / 2 + 1) * ncols() - 1;
	}
	// If we are in a multicolumn cell, move to the "real" cell
	while (cellinfo_[idx].multi == CELL_PART_OF_MULTICOLUMN) {
		LASSERT(idx > 0, return false);
		--idx;
	}
	return idx;
}


bool InsetMathGrid::idxDelete(idx_type & idx)
{
	// nothing to do if we have just one row
	if (nrows() == 1)
		return false;

	// nothing to do if we are in the middle of the last row of the inset
	if (idx + ncols() > nargs())
		return false;

	// try to delete entire sequence of ncols() empty cells if possible
	for (idx_type i = idx; i < idx + ncols(); ++i)
		if (!cell(i).empty())
			return false;

	// move cells if necessary
	for (idx_type i = index(row(idx), 0); i < idx; ++i)
		swap(cell(i), cell(i + ncols()));

	delRow(row(idx));

	if (idx >= nargs())
		idx = nargs() - 1;

	// undo effect of Ctrl-Tab (i.e. pull next cell)
	//if (idx + 1 != nargs())
	//	cell(idx).swap(cell(idx + 1));

	// we handled the event..
	return true;
}


// reimplement old behaviour when pressing Delete in the last position
// of a cell
void InsetMathGrid::idxGlue(idx_type idx)
{
	col_type c = col(idx);
	if (c + 1 == ncols()) {
		if (row(idx) + 1 != nrows()) {
			for (col_type cc = 0; cc < ncols(); ++cc)
				cell(idx).append(cell(idx + cc + 1));
			delRow(row(idx) + 1);
		}
	} else {
		idx_type idx_next = idx + 1;
		while (idx_next < nargs() &&
		       cellinfo_[idx_next].multi == CELL_PART_OF_MULTICOLUMN)
			++idx_next;
		if (idx_next < nargs())
			cell(idx).append(cell(idx_next));
		col_type oldcol = c + 1;
		for (col_type cc = c + 2; cc < ncols(); ++cc)
			cell(idx - oldcol + cc) = cell(idx - oldcol + 1 + cc);
		cell(idx - c + ncols() - 1).clear();
	}
}


InsetMathGrid::RowInfo const & InsetMathGrid::rowinfo(row_type row) const
{
	return rowinfo_[row];
}


InsetMathGrid::RowInfo & InsetMathGrid::rowinfo(row_type row)
{
	return rowinfo_[row];
}


bool InsetMathGrid::idxBetween(idx_type idx, idx_type from, idx_type to) const
{
	row_type const ri = row(idx);
	row_type const r1 = min(row(from), row(to));
	row_type const r2 = max(row(from), row(to));
	col_type const ci = col(idx);
	col_type const c1 = min(col(from), col(to));
	col_type const c2 = max(col(from), col(to));
	return r1 <= ri && ri <= r2 && c1 <= ci && ci <= c2;
}


void InsetMathGrid::normalize(NormalStream & os) const
{
	os << "[grid ";
	for (row_type row = 0; row < nrows(); ++row) {
		os << "[row ";
		for (col_type col = 0; col < ncols(); ++col) {
			idx_type const i = index(row, col);
			switch (cellinfo_[i].multi) {
			case CELL_NORMAL:
				os << "[cell " << cell(i) << ']';
				break;
			case CELL_BEGIN_OF_MULTICOLUMN:
				os << "[cell colspan="
				   << static_cast<int>(ncellcols(i)) << ' '
				   << cell(i) << ']';
				break;
			case CELL_PART_OF_MULTICOLUMN:
				break;
			}
		}
		os << ']';
	}
	os << ']';
}


void InsetMathGrid::mathmlize(MathMLStream & ms) const
{
	bool const havetable = nrows() > 1 || ncols() > 1;
	if (havetable)
		ms << MTag("mtable");
	char const * const celltag = havetable ? "mtd" : "mrow";
	for (row_type row = 0; row < nrows(); ++row) {
		if (havetable)
			ms << MTag("mtr");
		for (col_type col = 0; col < ncols(); ++col) {
			idx_type const i = index(row, col);
			if (cellinfo_[i].multi != CELL_PART_OF_MULTICOLUMN) {
				col_type const cellcols = ncellcols(i);
				ostringstream attr;
				if (havetable && cellcols > 1)
					attr << "colspan='" << cellcols << '\'';
				ms << MTag(celltag, attr.str());
				ms << cell(index(row, col));
				ms << ETag(celltag);
			}
		}
		if (havetable)
			ms << ETag("mtr");
	}
	if (havetable)
		ms << ETag("mtable");
}


// FIXME XHTML
// We need to do something about alignment here.
void InsetMathGrid::htmlize(HtmlStream & os, string const & attrib) const
{
	bool const havetable = nrows() > 1 || ncols() > 1;
	if (!havetable) {
		os << cell(index(0, 0));
		return;
	}
	os << MTag("table", attrib);
	for (row_type row = 0; row < nrows(); ++row) {
		os << MTag("tr");
		for (col_type col = 0; col < ncols(); ++col) {
			idx_type const i = index(row, col);
			if (cellinfo_[i].multi != CELL_PART_OF_MULTICOLUMN) {
				col_type const cellcols = ncellcols(i);
				ostringstream attr;
				if (cellcols > 1)
					attr << "colspan='" << cellcols << '\'';
				os << MTag("td", attr.str());
				os << cell(index(row, col));
				os << ETag("td");
			}
		}
		os << ETag("tr");
	}
	os << ETag("table");
}


void InsetMathGrid::htmlize(HtmlStream & os) const
{
	htmlize(os, "class='mathtable'");
}


void InsetMathGrid::validate(LaTeXFeatures & features) const
{
	if (features.runparams().math_flavor == OutputParams::MathAsHTML
	    && (nrows() > 1 || ncols() > 1)) {
		// CSS taken from InsetMathCases
		features.addCSSSnippet(
			"table.mathtable{display: inline-block; text-align: center; border: none;"
			"border-left: thin solid black; vertical-align: middle; padding-left: 0.5ex;}\n"
			"table.mathtable td {text-align: left; border: none;}");
	}
	InsetMathNest::validate(features);
}


void InsetMathGrid::write(TeXMathStream & os) const
{
	write(os, 0, 0, nrows(), ncols());
}

void InsetMathGrid::write(TeXMathStream & os,
                          row_type beg_row, col_type beg_col,
                          row_type end_row, col_type end_col) const
{
	MathEnsurer ensurer(os, false);
	docstring eolstr;
	// As of 2018 (with amendment in LaTeX 2021/06),
	// \\ is a robust command and its protection
	// is no longer necessary
	bool const fragile = os.fragile()
			&& !LaTeXFeatures::isAvailableAtLeastFrom("LaTeX", 2021, 06);
	for (row_type row = beg_row; row < end_row; ++row) {
		os << verboseHLine(rowinfo_[row].lines);
		// don't write & and empty cells at end of line,
		// unless there are vertical lines
		col_type lastcol = 0;
		bool emptyline = true;
		bool last_eoln = true;
		for (col_type col = beg_col; col < end_col; ++col) {
			idx_type const idx = index(row, col);
			bool const empty_cell = cell(idx).empty();
			if (last_eoln && (!empty_cell || cellinfo_[idx].multi != CELL_NORMAL))
				last_eoln = false;
			if (!empty_cell || cellinfo_[idx].multi != CELL_NORMAL ||
			    colinfo_[col + 1].lines) {
				lastcol = col + 1;
				emptyline = false;
			}
		}
		for (col_type col = beg_col; col < end_col;) {
			int nccols = 1;
			idx_type const idx = index(row, col);
			TexRow::RowEntry const entry = TexRow::mathEntry(id(),idx);
			os.texrow().start(entry);
			if (col >= lastcol) {
				++col;
				continue;
			}
			Changer dummy = os.changeRowEntry(entry);
			if (cellinfo_[idx].multi == CELL_BEGIN_OF_MULTICOLUMN) {
				size_t s = col + 1;
				while (s < ncols() &&
				       cellinfo_[index(row, s)].multi == CELL_PART_OF_MULTICOLUMN)
					s++;
				nccols = s - col;
				os << "\\multicolumn{" << nccols
				   << "}{" << cellinfo_[idx].align
				   << "}{";
			}
			os << cell(idx);
			if (os.pendingBrace())
				ModeSpecifier specifier(os, TEXT_MODE);
			if (cellinfo_[idx].multi == CELL_BEGIN_OF_MULTICOLUMN)
				os << '}';
			os << eocString(col + nccols - 1, lastcol);
			col += nccols;
		}
		eol(os, row, fragile, os.latex(), last_eoln);
		// append newline only if line wasn't completely empty
		// and the formula is not written on a single line
		bool const empty = emptyline && eolstr.empty();
		if (!empty && nrows() > 1)
			os << "\n";
	}
	// @TODO use end_row instead of nrows() ?
	docstring const s = verboseHLine(rowinfo_[nrows()].lines);
	if (!s.empty()) {
		if (eolstr.empty()) {
			if (fragile)
				os << "\\protect";
			os << "\\\\";
		}
		os << s;
	}
}


int InsetMathGrid::colsep() const
{
	return 6;
}


int InsetMathGrid::rowsep() const
{
	return 6;
}


int InsetMathGrid::hlinesep() const
{
	return 3;
}


int InsetMathGrid::vlinesep() const
{
	return 3;
}


int InsetMathGrid::border() const
{
	return 1;
}


void InsetMathGrid::splitCell(Cursor & cur)
{
	if (cur.idx() == cur.lastidx())
		return;
	MathData md = cur.cell();
	md.erase(0, cur.pos());
	cur.cell().erase(cur.pos(), cur.lastpos());
	++cur.idx();
	while (cur.idx() < nargs() &&
	       cellinfo_[cur.idx()].multi == CELL_BEGIN_OF_MULTICOLUMN)
		++cur.idx();
	cur.pos() = 0;
	cur.cell().insert(0, md);
}


char InsetMathGrid::displayColAlign(idx_type idx) const
{
	if (cellinfo_[idx].multi == CELL_BEGIN_OF_MULTICOLUMN) {
		// align may also contain lines like "||r|", so this is
		// not complete, but we catch at least the simple cases.
		if (cellinfo_[idx].align == "c")
			return 'c';
		if (cellinfo_[idx].align == "l")
			return 'l';
		if (cellinfo_[idx].align == "r")
			return 'r';
	}
	return colinfo_[col(idx)].align;
}


int InsetMathGrid::displayColSpace(col_type col) const
{
	return colinfo_[col].skip;
}

void InsetMathGrid::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	//lyxerr << "*** InsetMathGrid: request: " << cmd << endl;

	Parse::flags parseflg = Parse::QUIET | Parse::USETEXT;

	FuncCode const act = cmd.action();
	switch (act) {

	// insert file functions
	case LFUN_LINE_DELETE_FORWARD:
		cur.recordUndoInset();
		//autocorrect_ = false;
		//macroModeClose();
		//if (selection_) {
		//	selDel();
		//	break;
		//}
		if (nrows() > 1)
			delRow(cur.row());
		if (cur.idx() > cur.lastidx())
			cur.idx() = cur.lastidx();
		if (cur.pos() > cur.lastpos())
			cur.pos() = cur.lastpos();
		break;

	case LFUN_CELL_SPLIT:
		// undo needs to cover two cells
		cur.recordUndoInset();
		splitCell(cur);
		break;

	case LFUN_NEWLINE_INSERT: {
		cur.recordUndoInset();
		row_type const oldr = cur.row();
		col_type const oldc = cur.col();
		addRow(oldr);

		// split line
		for (col_type c = col(cur.idx()) + 1; c < ncols(); ++c)
			swap(cell(index(oldr, c)), cell(index(oldr + 1, c)));

		// split cell
		splitCell(cur);
		if (ncols() > 1)
			swap(cell(cur.idx()), cell(cur.idx() + ncols() - 1));

		// move cursor to next line
		cur.idx() = index(oldr + 1, oldc);
		cur.pos() = 0;
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_TABULAR_FEATURE: {
		cur.recordUndoInset();
		//lyxerr << "handling tabular-feature " << to_utf8(cmd.argument()) << endl;
		istringstream is(to_utf8(cmd.argument()));
		string s;
		is >> s;
		if (s == "valign-top")
			setVerticalAlignment('t');
		else if (s == "valign-middle")
			setVerticalAlignment('c');
		else if (s == "valign-bottom")
			setVerticalAlignment('b');
		else if (s == "align-left")
			setHorizontalAlignment('l', cur.col());
		else if (s == "align-right")
			setHorizontalAlignment('r', cur.col());
		else if (s == "align-center")
			setHorizontalAlignment('c', cur.col());
		else if (s == "append-row")
			for (int i = 0, n = extractInt(is); i < n; ++i)
				addRow(cur.row());
		else if (s == "delete-row") {
			cur.clearSelection(); // bug 4323
			for (int i = 0, n = extractInt(is); i < n; ++i) {
				delRow(cur.row());
				if (cur.idx() >= nargs())
					cur.idx() -= ncols();
			}
			cur.pos() = 0; // trick, see below
		}
		else if (s == "copy-row") {
			// Here (as later) we save the cursor col/row
			// in order to restore it after operation.
			row_type const r = cur.row();
			col_type const c = cur.col();
			for (int i = 0, n = extractInt(is); i < n; ++i)
				copyRow(cur.row());
			cur.idx() = index(r, c);
		}
		else if (s == "swap-row") {
			swapRow(cur.row());
			// Trick to suppress same-idx-means-different-cell
			// assertion crash:
			cur.pos() = 0;
		}
		else if (s == "add-hline-above")
			rowinfo_[cur.row()].lines++;
		else if (s == "add-hline-below")
			rowinfo_[cur.row()+1].lines++;
		else if (s == "delete-hline-above")
			rowinfo_[cur.row()].lines--;
		else if (s == "delete-hline-below")
			rowinfo_[cur.row()+1].lines--;
		else if (s == "append-column") {
			row_type const r = cur.row();
			col_type const c = cur.col();
			for (int i = 0, n = extractInt(is); i < n; ++i)
				addCol(cur.col() + 1);
			cur.idx() = index(r, c);
		}
		else if (s == "delete-column") {
			cur.clearSelection(); // bug 4323
			row_type const r = cur.row();
			col_type const c = cur.col();
			for (int i = 0, n = extractInt(is); i < n; ++i)
				delCol(col(cur.idx()));
			cur.idx() = index(r, min(c, cur.ncols() - 1));
			cur.pos() = 0; // trick, see above
		}
		else if (s == "copy-column") {
			row_type const r = cur.row();
			col_type const c = cur.col();
			copyCol(cur.col());
			cur.idx() = index(r, c);
		}
		else if (s == "swap-column") {
			swapCol(cur.col());
			cur.pos() = 0; // trick, see above
		}
		else if (s == "add-vline-left") {
			colinfo_[cur.col()].lines++;
			if (!colinfo_[cur.col()].special.empty())
				colinfo_[cur.col()].special += '|';
		}
		else if (s == "add-vline-right") {
			colinfo_[cur.col()+1].lines++;
			if (!colinfo_[cur.col()+1].special.empty())
				colinfo_[cur.col()+1].special.insert(0, 1, '|');
		}
		else if (s == "delete-vline-left") {
			colinfo_[cur.col()].lines--;
			docstring & special = colinfo_[cur.col()].special;
			if (!special.empty()) {
				docstring::size_type i = special.rfind('|');
				LASSERT(i != docstring::npos, break);
				special.erase(i, 1);
			}
		}
		else if (s == "delete-vline-right") {
			colinfo_[cur.col()+1].lines--;
			docstring & special = colinfo_[cur.col()+1].special;
			if (!special.empty()) {
				docstring::size_type i = special.find('|');
				LASSERT(i != docstring::npos, break);
				special.erase(i, 1);
			}
		}
		else {
			cur.undispatched();
			break;
		}
		cur.forceBufferUpdate();
		// perhaps this should be FINISHED_BACKWARD -- just for clarity?
		//lyxerr << "returning FINISHED_LEFT" << endl;
		break;
	}

	case LFUN_CLIPBOARD_PASTE:
		parseflg |= Parse::VERBATIM;
		// fall through
	case LFUN_PASTE: {
		if (cur.currentMode() != MATH_MODE)
			parseflg |= Parse::TEXTMODE;
		cur.message(_("Paste"));
		cap::replaceSelection(cur);
		docstring topaste;
		if (cmd.argument().empty() && !theClipboard().isInternal())
			topaste = theClipboard().getAsText(frontend::Clipboard::PlainTextType);
		else {
			idocstringstream is(cmd.argument());
			int n = 0;
			is >> n;
			topaste = cap::selection(n, make_pair(buffer().params().documentClassPtr(),
							      buffer().params().authors()), true);
		}
		InsetMathGrid grid(buffer_, 1, 1);
		if (!topaste.empty())
			if ((topaste.size() == 1 && isAscii(topaste))
			    || !mathed_parse_normal(grid, topaste, parseflg)) {
				resetGrid(grid);
				mathed_parse_normal(grid, topaste, parseflg | Parse::VERBATIM);
			}

		bool hline_enabled = false;
		FuncRequest fr = FuncRequest(LFUN_TABULAR_FEATURE, "add-hline-above");
		FuncStatus status;
		if (getStatus(cur, fr, status))
			hline_enabled = status.enabled();
		if (grid.nargs() == 1) {
			// single cell/part of cell
			cur.recordUndoInset();
			cur.cell().insert(cur.pos(), grid.cell(0));
			cur.pos() += grid.cell(0).size();
			if (hline_enabled)
				rowinfo_[cur.row()].lines += grid.rowinfo_[0].lines;
			else {
				for (unsigned int l = 0; l < grid.rowinfo_[0].lines; ++l) {
					 cur.cell().insert(0,
						MathAtom(new InsetMathUnknown(buffer_, from_ascii("\\hline"))));
					 cur.pos()++;
				}
			}
		} else {
			// multiple cells
			cur.recordUndoInset();
			col_type startcol = col(cur.idx());
			row_type startrow = cur.row();
			col_type oldncols = ncols();
			col_type numcols =
				min(grid.ncols(), ncols() - startcol);
			row_type const numrows =
				min(grid.nrows(), nrows() - cur.row());
			for (row_type r = 0; r < numrows; ++r) {
				for (col_type c = 0; c < numcols; ++c) {
					idx_type i = index(r + startrow, c + startcol);
					pos_type const ipos = min(cur.pos(), pos_type(cell(i).size()));
					cell(i).insert(ipos, grid.cell(grid.index(r, c)));
				}
				if (hline_enabled)
					rowinfo_[r].lines += grid.rowinfo_[r].lines;
				else {
					for (unsigned int l = 0; l < grid.rowinfo_[r].lines; ++l) {
						idx_type i = index(r + startrow, 0);
						cell(i).insert(0,
							MathAtom(new InsetMathUnknown(buffer_, from_ascii("\\hline"))));
					}
				}
				// append columns for the left over horizontal cells
				for (col_type c = numcols; c < grid.ncols(); ++c) {
					addCol(c + startcol);
					idx_type i = index(r + startrow, min(c + startcol, ncols() - 1));
					cell(i).append(grid.cell(grid.index(r, c)));
					++numcols;
				}
			}
			// amend cursor position if cols have been appended
			cur.idx() += startrow * (ncols() - oldncols);
			// append rows for the left over vertical cells
			idx_type i = nargs() - 1;
			for (row_type r = numrows; r < grid.nrows(); ++r) {
				row_type crow = startrow + r;
				addRow(crow - 1);
				for (col_type c = 0; c < grid.ncols(); ++c)
					cell(index(min(crow, nrows() - 1), min(c + startcol, ncols() - 1))).append(grid.cell(grid.index(r, c)));
				if (hline_enabled)
					rowinfo_[crow].lines += grid.rowinfo_[r].lines;
				else {
					for (unsigned int l = 0; l < grid.rowinfo_[r].lines; ++l) {
						cell(i).insert(0,
							MathAtom(new InsetMathUnknown(buffer_, from_ascii("\\hline"))));
					}
				}
			}
		}
		cur.clearSelection(); // bug 393
		// FIXME audit setBuffer calls
		cur.inset().setBuffer(*buffer_);
		cur.forceBufferUpdate();
		cur.finishUndo();
		break;
	}

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
		} else if (cur.idx() % cur.ncols() != 0) {
			cur.idx() -= cur.idx() % cur.ncols();
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
		} else if ((cur.idx() + 1) % cur.ncols() != 0) {
			cur.idx() += cur.ncols() - 1 - cur.idx() % cur.ncols();
			cur.pos() = cur.lastpos();
		} else if (cur.idx() != cur.lastidx()) {
			cur.idx() = cur.lastidx();
			cur.pos() = cur.lastpos();
		} else {
			cmd = FuncRequest(LFUN_FINISHED_FORWARD);
			cur.undispatched();
		}
		break;

	default:
		InsetMathNest::doDispatch(cur, cmd);
	}
}


bool InsetMathGrid::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & status) const
{
	switch (cmd.action()) {
	case LFUN_TABULAR_FEATURE: {
		string s = cmd.getArg(0);
		if (&cur.inset() != this) {
			// Table actions requires that the cursor is _inside_ the
			// table.
			status.setEnabled(false);
			status.message(from_utf8(N_("Cursor not in table")));
			return true;
		}
		if (nrows() <= 1 && (s == "delete-row" || s == "swap-row")) {
			status.setEnabled(false);
			status.message(from_utf8(N_("Only one row")));
			return true;
		}
		if (ncols() <= 1 &&
		    (s == "delete-column" || s == "swap-column")) {
			status.setEnabled(false);
			status.message(from_utf8(N_("Only one column")));
			return true;
		}
		if ((rowinfo_[cur.row()].lines == 0 &&
		     s == "delete-hline-above") ||
		    (rowinfo_[cur.row() + 1].lines == 0 &&
		     s == "delete-hline-below")) {
			status.setEnabled(false);
			status.message(from_utf8(N_("No hline to delete")));
			return true;
		}

		if ((colinfo_[cur.col()].lines == 0 &&
		     s == "delete-vline-left") ||
		    (colinfo_[cur.col() + 1].lines == 0 &&
		     s == "delete-vline-right")) {
			status.setEnabled(false);
			status.message(from_utf8(N_("No vline to delete")));
			return true;
		}
		if (s == "valign-top" || s == "valign-middle" ||
		    s == "valign-bottom" || s == "align-left" ||
		    s == "align-right" || s == "align-center") {
			status.setEnabled(true);
			char const ha = horizontalAlignment(cur.col());
			char const va = verticalAlignment();
			status.setOnOff((s == "align-left" && ha == 'l')
					|| (s == "align-right"   && ha == 'r')
					|| (s == "align-center"  && ha == 'c')
					|| (s == "valign-top"    && va == 't')
					|| (s == "valign-bottom" && va == 'b')
					|| (s == "valign-middle" && va == 'c'));
			return true;
		}
		if (s == "append-row" || s == "delete-row" ||
		    s == "copy-row" || s == "swap-row" ||
		    s == "add-hline-above" || s == "add-hline-below" ||
		    s == "delete-hline-above" || s == "delete-hline-below" ||
		    s == "append-column" || s == "delete-column" ||
		    s == "copy-column" || s == "swap-column" ||
		    s == "add-vline-left" || s == "add-vline-right" ||
		    s == "delete-vline-left" || s == "delete-vline-right") {
			status.setEnabled(true);
		} else {
			status.setEnabled(false);
			status.message(bformat(
			    from_utf8(N_("Unknown tabular feature '%1$s'")),
			    from_utf8(s)));
		}

		return true;
	}

	case LFUN_CELL_SPLIT:
		status.setEnabled(cur.idx() != cur.lastidx());
		return true;

	case LFUN_CELL_BACKWARD:
	case LFUN_CELL_FORWARD:
		status.setEnabled(true);
		return true;

	default:
		break;
	}
	return InsetMathNest::getStatus(cur, cmd, status);
}


char InsetMathGrid::colAlign(HullType type, col_type col) const
{
	switch (type) {
	case hullEqnArray:
		return "rcl"[col % 3];

	case hullMultline:
		return 'c';
	case hullGather:
		LASSERT(isBufferValid(),
				LYXERR0("Buffer not set correctly. Please report!");
				return 'c';);
		if (buffer().params().is_math_indent)
			return 'l';
		else
			return 'c';

	case hullAlign:
	case hullAlignAt:
	case hullXAlignAt:
	case hullXXAlignAt:
	case hullFlAlign:
		return "rl"[col & 1];

	case hullUnknown:
	case hullNone:
	case hullSimple:
	case hullEquation:
	case hullRegexp:
		return 'c';
	}
	// avoid warning
	return 'c';
}


//static
int InsetMathGrid::colSpace(HullType type, col_type col)
{
	int alignInterSpace = 0;
	switch (type) {
	case hullUnknown:
	case hullNone:
	case hullSimple:
	case hullEquation:
	case hullMultline:
	case hullGather:
	case hullRegexp:
		return 0;

	case hullEqnArray:
		return 5;

	case hullAlign:
		alignInterSpace = 20;
		break;
	case hullAlignAt:
		alignInterSpace = 0;
		break;
	case hullXAlignAt:
		alignInterSpace = 40;
		break;
	case hullXXAlignAt:
	case hullFlAlign:
		alignInterSpace = 60;
		break;
	}
	return (col % 2) ? alignInterSpace : 0;
}


} // namespace lyx
