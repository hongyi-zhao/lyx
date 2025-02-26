/**
 * \file ColorListWidget.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "ColorListWidget.h"
#include <QtGui/qevent.h>

namespace lyx {
namespace frontend {

ColorListWidget::ColorListWidget(QWidget *parent) : QListWidget(parent)
{}


void ColorListWidget::focusInEvent(QFocusEvent* ev)
{
	color_group_ = QPalette::Active;

	Q_EMIT focusChanged();
	ev->accept();
}


void ColorListWidget::focusOutEvent(QFocusEvent* ev)
{
	color_group_ = QPalette::Inactive;

	Q_EMIT focusChanged();
	ev->accept();
}

} // namespace frontend
} // namespace lyx

#include "moc_ColorListWidget.cpp"
