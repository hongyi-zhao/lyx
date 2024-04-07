/**
 * \file GuiFontExample.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "support/qstring_helpers.h"

#include "GuiFontExample.h"
#include "GuiFontMetrics.h"

#include <QPainter>
#include <QPaintEvent>


//namespace lyx {

void GuiFontExample::set(QFont const & font, QString const & text)
{
	font_ = font;
	text_ = text;
	lyx::frontend::GuiFontMetrics m(font_);
	// store width, ascent and descent of the font name
	string_width_ = m.width(text_);
	for (auto const c : lyx::fromqstr(text)) {
		string_ascent_ = std::max(string_ascent_, m.ascent(c));
		string_descent_ = std::max(string_ascent_, m.descent(c));
	}
	update();
}


QSize GuiFontExample::sizeHint() const
{
	return QSize(string_width_ + 10,
		     string_ascent_ + string_descent_ + 6);
}


void GuiFontExample::paintEvent(QPaintEvent *)
{
	QPainter p;

	p.begin(this);
	p.setFont(font_);
	int const h = height() - 1;
	p.drawRect(0, 0, width() - 1, h);
	p.drawText(5, (h / 2) + (string_descent_ / 2), text_);
	p.end();
}


int GuiFontExample::minWidth() const
{
	return string_width_;
}


//} // namespace lyx
