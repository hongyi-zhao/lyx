// -*- C++ -*-
/**
 * \file ColorListWidget.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef COLORLISTWIDGET_H
#define COLORLISTWIDGET_H

#include <QListWidget>

#include <QObject>

namespace lyx {
namespace frontend {

class ColorListWidget : public QListWidget
{
	Q_OBJECT
public:
	explicit ColorListWidget(QWidget *parent = nullptr);
	~ColorListWidget(){};

	void focusInEvent(QFocusEvent* ev) override;
	void focusOutEvent(QFocusEvent* ev) override;

	QPalette::ColorGroup currentColorGroup() { return color_group_; }

Q_SIGNALS:
	void focusChanged();
private:
	QPalette::ColorGroup color_group_ = QPalette::Inactive;
};

} // namespace frontend
} // namespace lyx

#endif // COLORLISTWIDGET_H
