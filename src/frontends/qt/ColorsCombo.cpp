/**
 * \file qt/ColorsCombo.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "ColorsCombo.h"
#include "LaTeXColors.h"

#include "support/gettext.h"
#include "qt_helpers.h"

#include "support/qstring_helpers.h"

#include <QPainter>

using namespace lyx::support;

namespace lyx {
namespace frontend {


ColorsCombo::ColorsCombo(QWidget * parent)
	: CategorizedCombo(parent),
	  params_(BufferParams()),
	  has_ignore_(false), has_inherit_(false)
{
	setLeftMargin(32);
	fillComboColor();
}


ColorsCombo::~ColorsCombo()
{}


void ColorsCombo::setColorIcon(int const i, QString const color)
{
	if (color.isEmpty())
		return;

	QPixmap pixmap(32, 20);
	QColor col(color);
	pixmap.fill(col);
	QPainter painter(&pixmap);
	QRect pixmapRect = QRect(0, 0, 31, 19);
	painter.drawPixmap(pixmapRect.x(), pixmapRect.y(), pixmap);
	painter.drawRect(pixmapRect);

	setItemIcon(i, QIcon(pixmap));
}


void ColorsCombo::addItemSort(QString const & item, QString const & guiname,
			      QString const & category, QString const color)
{
	QString titem = guiname;

	QList<QStandardItem *> row;
	QStandardItem * gui = new QStandardItem(titem);
	row.append(gui);
	row.append(new QStandardItem(item));
	row.append(new QStandardItem(category));
	row.append(new QStandardItem(true));
	if (!color.isEmpty())
		row.append(new QStandardItem(color));

	// the first entry is easy
	int const end = model()->rowCount();
	if (end == 0) {
		model()->appendRow(row);
		setColorIcon(0, color);
		return;
	}

	// find category
	int i = 0;
	// Sort categories alphabetically, uncategorized at the end.
	while (i < end && model()->item(i, 2)->text() != category)
		++i;

	// find row to insert the item, after the separator if it exists
	if (i < end) {
		// find alphabetic position, unavailable at the end
		while (i != end
		       && (model()->item(i, 0)->text().localeAwareCompare(titem) < 0)
		       && (model()->item(i, 2)->text() == category))
			++i;
	}

	model()->insertRow(i, row);
	setColorIcon(i, color);
}


void ColorsCombo::setBufferParams(BufferParams const params)
{
	params_ = params;
	fillComboColor();
}


void ColorsCombo::fillComboColor()
{
	reset();
	// at first add the general values as required
	if (has_ignore_)
		addItemSort(QString("ignore"), qt_("No change"),
			    QString());
	addItemSort(QString("none"), qt_("Default"),
		    QString());
	if (has_inherit_)
		addItemSort(QString("inherit"), qt_("(Without)[[color]]"),
			    QString());
	// now custom colors
	for (auto const & lc : params_.custom_colors) {
		QString const lyxname = toqstr(lc.first);
		QString const guiname = toqstr(lc.first);
		QString const category = qt_("Custom Colors");
		QString const plaincategory = toqstr("custom");
		QString const color = toqstr(lc.second);
		addItemSort(lyxname,
			    guiname,
			    category,
			    color);
	}
	// then real colors
	for (auto const & lc : theLaTeXColors().getLaTeXColors()) {
		QString const lyxname = toqstr(lc.first);
		QString const guiname = toqstr(translateIfPossible(lc.second.guiname()));
		QString const category = toqstr(translateIfPossible(lc.second.category()));
		QString const plaincategory = toqstr(lc.second.category());
		QString const color = toqstr(lc.second.hexname());
		addItemSort(lyxname,
			    guiname,
			    category,
			    color);
	}
}


} // namespace frontend
} // namespace lyx


#include "moc_ColorsCombo.cpp"
