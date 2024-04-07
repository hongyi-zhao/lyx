// -*- C++ -*-
/**
 * \file GuiFontExample.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUIFONTEXAMPLE_H
#define GUIFONTEXAMPLE_H

#include <QWidget>
#include <QFont>
#include <QString>

class QPaintEvent;

//namespace lyx {

class GuiFontExample : public QWidget
{
public:
	GuiFontExample(QWidget * parent) : QWidget(parent) {}

	void set(QFont const & font, QString const & text);

	QSize sizeHint() const override;
	
	int minWidth() const;

protected:
	void paintEvent(QPaintEvent * p) override;

private:
	QFont font_;
	QString text_;
	int string_ascent_ = 0;
	int string_descent_ = 0;
	int string_width_ = 0;
};


//} // namespace lyx

#endif // GUIFONTEXAMPLE_H
