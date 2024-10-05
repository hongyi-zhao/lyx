// -*- C++ -*-
/**
 * \file LayoutBox.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author John Levon
 * \author Jean-Marc Lasgouttes
 * \author Angus Leeming
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef LYX_LAYOUT_BOX_H
#define LYX_LAYOUT_BOX_H

#include "support/docstring.h"

#include <QComboBox>

namespace lyx {

namespace frontend {

class GuiView;
class LayoutItemDelegate;

class LayoutBox : public QComboBox
{
	Q_OBJECT
public:
	LayoutBox(GuiView &);
	~LayoutBox();

	/// select the right layout in the combobox.
	void set(docstring const & layout);
	/// Populate the layout combobox.
	void updateContents(bool reset);
	/// Add Item to Layout box according to sorting settings from preferences
	void addItemSort(docstring const & item, docstring const & category,
		bool sorted, bool sortedByCat, bool unknown);

	///
	void showPopup() override;

	///
	bool eventFilter(QObject * o, QEvent * e) override;
	///
	QString const & filter() const;

private Q_SLOTS:
	///
	void selected(int index);
	///
	void setIconSize(QSize size);

private:
	friend class LayoutItemDelegate;
	class Private;
	Private * const d;
	int lastCurrentIndex_;
};

} // namespace frontend
} // namespace lyx

#endif // LYX_LAYOUT_BOX_H
