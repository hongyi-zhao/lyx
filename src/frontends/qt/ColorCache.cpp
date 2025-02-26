/**
 * \file ColorCache.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "ColorCache.h"

#include "LyXRC.h"

#include "Color.h"
#include "ColorSet.h"

namespace lyx {

namespace{

QPalette::ColorRole role(ColorCode col)
{
	switch (col) {
	case Color_background:
	case Color_commentbg:
	case Color_greyedoutbg:
	case Color_mathbg:
	case Color_graphicsbg:
	case Color_mathmacrobg:
	case Color_mathcorners:
		return QPalette::Base;
		break;

	case Color_foreground:
	case Color_cursor:
	case Color_preview:
	case Color_tabularline:
	case Color_previewframe:
		return QPalette::Text;
		break;

	case Color_selection:
		return QPalette::Highlight;
		break;
	case Color_selectionmath:
	case Color_selectiontext:
		return QPalette::HighlightedText;
		break;
	case Color_urllabel:
	case Color_urltext:
		return QPalette::Link;
	default:
		return QPalette::NoRole;
	}
}

} // namespace


void ColorCache::init()
{
	for (int col = 0; col <= Color_ignore; ++col) {
		// light-mode color
		lcolors_[col].first =
		    QColor(lcolor.getX11HexName(ColorCode(col), false).c_str());
		// dark-mode color
		lcolors_[col].second =
		     QColor(lcolor.getX11HexName(ColorCode(col), true).c_str());
	}

	initialized_ = true;
}


/// get the given color
QColor ColorCache::get(Color const & color) const
{
	return get(color, lyxrc.use_system_colors);
}


/// get the given color
QColor ColorCache::get(Color const & color, bool syscolors) const
{
	bool const dark_mode = isDarkMode();

	if (dark_mode)
		return getAll(color, syscolors).second;
	else
		return getAll(color, syscolors).first;
}


/// get the given color
std::pair<QColor, QColor> ColorCache::getAll(Color const & color, bool syscolors) const
{
	if (!initialized_)
		const_cast<ColorCache *>(this)->init();
	if (color <= Color_ignore && color.mergeColor == Color_ignore) {
		QPalette::ColorRole const cr = role(color.baseColor);
		if (syscolors && cr != QPalette::NoRole) {
			static QColor const white = Qt::white;
			QColor c = pal_.brush(QPalette::Active, cr).color();
			// Change to fully opaque color
			c.setAlpha(255);
			if (cr == QPalette::Base && c == white)
				return lcolors_[color.baseColor];
			else
				return {c, c};
		} else
			return lcolors_[color.baseColor];
	}
	if (color.mergeColor != Color_ignore) {
		// FIXME: This would ideally be done in the Color class, but
		// that means that we'd have to use the Qt code in the core.
		std::pair<QColor, QColor> base_colors = getAll(color.baseColor, syscolors);
		std::pair<QColor, QColor> merge_colors = getAll(color.mergeColor, syscolors);
		return {QColor(
		            (base_colors.first.toRgb().red() + merge_colors.first.toRgb().red()) / 2,
		            (base_colors.first.toRgb().green() + merge_colors.first.toRgb().green()) / 2,
		            (base_colors.first.toRgb().blue() + merge_colors.first.toRgb().blue()) / 2),
		    QColor(
		            (base_colors.first.toRgb().red() + merge_colors.first.toRgb().red()) / 2,
		            (base_colors.first.toRgb().green() + merge_colors.first.toRgb().green()) / 2,
		            (base_colors.first.toRgb().blue() + merge_colors.first.toRgb().blue()) / 2),
		};
	}
	// used by branches
	return {QColor(lcolor.getX11HexName(color.baseColor, false).c_str()),
		    QColor(lcolor.getX11HexName(color.baseColor, true).c_str())};
}


bool ColorCache::isSystem(ColorCode const color) const
{
	QPalette::ColorRole const cr = role(color);
	if (cr == QPalette::Base) {
		static QColor const white = Qt::white;
		return pal_.brush(QPalette::Active, cr).color() != white;
	} else
		return cr != QPalette::NoRole;
}


bool ColorCache::isDarkMode() const
{
	QColor text_color = pal_.color(QPalette::Active, QPalette::WindowText);
	QColor bg_color = pal_.color(QPalette::Active, QPalette::Window);

	return (text_color.black() < bg_color.black());
}


QColor const rgb2qcolor(RGBColor const & rgb)
{
	return QColor(rgb.r, rgb.g, rgb.b);
}


} // namespace lyx
