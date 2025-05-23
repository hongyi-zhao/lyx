// -*- C++ -*-
/**
 * \file GuiPainter.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUIPAINTER_H
#define GUIPAINTER_H

#include "Color.h"

#include "frontends/Painter.h"

#include <QPainter>
#include <stack>

namespace lyx {

class FontInfo;

namespace frontend {

/**
 * GuiPainter - a painter implementation for Qt
 */
class GuiPainter : public QPainter, public Painter {
public:
	GuiPainter(QPaintDevice *, double pixel_ratio, bool devel_mode);
	virtual ~GuiPainter();

	/// This painter paints
	bool isNull() const override { return false; }

	/// draw a line from point to point
	void line(
		int x1, int y1,
		int x2, int y2,
		Color,
		line_style ls = line_solid,
		int lw = thin_line) override;

	/**
	 * lines -  draw a set of lines
	 * @param xp array of points' x co-ords
	 * @param yp array of points' y co-ords
	 * @param np size of the points array
	 */
	void lines(
		int const * xp,
		int const * yp,
		int np,
		Color,
		fill_style fs = fill_none,
		line_style ls = line_solid,
		int lw = thin_line) override;

	/**
	 * path -  draw a path with bezier curves
	 * @param xp array of points' x co-ords
	 * @param yp array of points' y co-ords
	 * @param c1x array of first control points' x co-ords
	 * @param c1y array of first control points' y co-ords
	 * @param c2x array of second control points' x co-ords
	 * @param c2y array of second control points' y co-ords
	 * @param np size of the points array
	 */
	void path(int const * xp, int const * yp,
		int const * c1x, int const * c1y,
		int const * c2x, int const * c2y,
		int np, Color,
		fill_style = fill_none, line_style = line_solid,
		int line_width = thin_line) override;

	/// draw a rectangle
	void rectangle(
		int x, int y,
		int w, int h,
		Color,
		line_style = line_solid,
		int lw = thin_line) override;

	/// draw a filled rectangle
	void fillRectangle(
		int x, int y,
		int w, int h,
		Color) override;

	/// draw an arc
	void arc(
		int x, int y,
		unsigned int w, unsigned int h,
		int a1, int a2,
		Color) override;

	/// draw an ellipse
	void ellipse(
		double x, double y,
		double rx, double ry,
		Color,
		fill_style fs = fill_none,
		line_style ls = line_solid,
		int lw = thin_line) override;

	/// draw a pixel
	void point(int x, int y, Color) override;

	/// draw an image from the image cache
	void image(int x, int y, int w, int h,
		lyx::graphics::Image const & image, bool const darkmode = false) override;

	/// draw a string at position x, y (y is the baseline).
	void text(int x, int y, docstring const & str, FontInfo const & f,
	          Direction const dir = Auto) override;
	/// draw a string at position x, y (y is the baseline) using input method.
	void text(int x, int y, docstring const & str, InputMethod const * im,
	          pos_type const char_format_index,
	          Direction const dir = Auto) override;

	/// draw a char at position x, y (y is the baseline)
	void text(int x, int y, char_type c, FontInfo const & f,
	          Direction const dir = Auto) override;
	/// draw a char at position x, y (y is the baseline) using input method
	void text(int x, int y, char_type c, InputMethod const * im,
	          pos_type const char_format_index,
	          Direction const dir = Auto) override;

	/** draw a string at position x, y (y is the baseline). The
	 * text direction is enforced by the \c Font.
	 */
	void text(int x, int y, docstring const & str, Font const & f,
                      double wordspacing, double textwidth) override;

	/** draw a string at position x, y (y is the baseline), but
	 * make sure that the part between \c from and \c to is in
	 * \c other color. The text direction is enforced by the \c Font.
	 */
	void text(int x, int y, docstring const & str, Font const & f,
	                  Color other, size_type from, size_type to,
                      double wordspacing, double textwidth) override;

	///
	void textDecoration(FontInfo const & f, int x, int y, int width) override;

	/// draw a string and enclose it inside a button frame
	void buttonText(int x, int baseline, docstring const & s,
		FontInfo const & font, Color back, Color frame, int offset) override;

	/// start monochrome painting mode, i.e. map every color a shade of \c blend.
	void enterMonochromeMode(Color const & blend) override;
	/// leave monochrome painting mode
	void leaveMonochromeMode() override;

	/**
	 * Draw a string and enclose it inside a rectangle. If
	 * back color is specified, the background is cleared with
	 * the given color. If frame is specified, a thin frame is drawn
	 * around the text with the given color.
	 */
	void rectText(int x, int baseline, docstring const & str,
		FontInfo const & font, Color back, Color frame) override;

	void wavyHorizontalLine(FontInfo const & f, int x, int y, int width, ColorCode col) override;

private:
	/// check the font, and if set, draw an underline
	void underline(FontInfo const & f,
	               int x, int y, int width, line_style ls = line_solid);

	/// check the font, and if set, draw an dashed underline
	void dashedUnderline(FontInfo const & f,
		int x, int y, int width);

	/// check the font, and if set, draw an strike-through line
	void strikeoutLine(FontInfo const & f,
		int x, int y, int width);

	/// check the font, and if set, draw cross-through lines
	void crossoutLines(FontInfo const & f,
		int x, int y, int width);

	/// check the font, and if set, draw double underline
	void doubleUnderline(FontInfo const & f,
		int x, int y, int width);

	/// set pen parameters
	void setQPainterPen(QColor const & col,
		line_style ls = line_solid, int lw = thin_line,
		Qt::PenJoinStyle js = Qt::BevelJoin);

	// Real text() method
	void text(int x, int y, docstring const & s,
              FontInfo const & f, Direction const dir,
              double const wordspacing, double tw);

	QColor current_color_;
	Painter::line_style current_ls_;
	int current_lw_;
	///
	std::stack<QColor> monochrome_blend_;
	/// convert into Qt color, possibly applying the monochrome mode
	QColor computeColor(Color col);
	/// possibly apply monochrome mode
	QColor filterColor(QColor const & col);
};

} // namespace frontend
} // namespace lyx

#endif // GUIPAINTER_H
