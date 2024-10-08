// -*- C++ -*-
/**
 * \file GuiDelimiter.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUIDELIMITERDIALOG_H
#define GUIDELIMITERDIALOG_H

#include "support/docstring.h"

#include "GuiDialog.h"

#include "ui_DelimiterUi.h"

class QListWidgetItem;

namespace lyx {
namespace frontend {

class GuiDelimiter : public GuiDialog, public Ui::DelimiterUi
{
	Q_OBJECT

public:
	GuiDelimiter(GuiView & lv);

	bool initialiseParams(std::string const &) override { return true; }
	void clearParams() override {}
	void dispatchParams() override {}
	bool isBufferDependent() const override { return true; }

public Q_SLOTS:
	void on_buttonBox_clicked(QAbstractButton * button);
	void on_leftLW_itemActivated(QListWidgetItem *);
	void on_rightLW_itemActivated(QListWidgetItem *);
	void on_leftLW_currentRowChanged(int);
	void on_rightLW_currentRowChanged(int);
	void on_matchCB_stateChanged(int);
	void insert();
	void on_swapPB_clicked();
	void on_sizeCO_activated(int);

private:
	///
	char_type doMatch(char_type const symbol);
	///
	void updateTeXCode(int size);

	/// TeX code that will be inserted.
	docstring tex_code_;
};

} // namespace frontend
} // namespace lyx

#endif // GUIDELIMITERDIALOG_H
