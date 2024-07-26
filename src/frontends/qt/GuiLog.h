// -*- C++ -*-
/**
 * \file GuiLog.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Angus Leeming
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUILOG_H
#define GUILOG_H

#include "GuiDialog.h"
#include "ui_LogUi.h"

#include "support/FileName.h"


namespace lyx {
namespace frontend {

class LogHighlighter;

class GuiLog : public GuiDialog, public Ui::LogUi
{
	Q_OBJECT

public:
	GuiLog(GuiView & lv);

private Q_SLOTS:
	void updateContents() override;
	/// find content
	void find();
	/// jump to next error message
	void on_nextErrorPB_clicked();
	/// jump to next warning
	void on_nextWarningPB_clicked();
	/// open containing directory
	void on_openDirPB_clicked();
	/// Log type changed
	void typeChanged(int);

private:
	/// Apply changes
	void applyView() override {}

	/// log syntax highlighter
	LogHighlighter * highlighter;

	/** \param data should contain "<logtype> <logfile>"
	 *  where <logtype> is one of "latex", "literate", "lyx2lyx", "vc".
	 */
	bool initialiseParams(std::string const & data) override;
	///
	void clearParams() override;
	///
	void dispatchParams() override {}
	///
	bool isBufferDependent() const override { return true; }

	/// The title displayed by the dialog reflects the \c LogType
	docstring title() const;
	/// put the log file into the ostream
	void getContents(std::ostream & ss) const;
	/// go to the next occurrence of the RegExp
	void goTo(QRegularExpression const & exp) const;
	/// does the document after cursor position contain the RegExp?
	bool contains(QRegularExpression const & exp) const;

private:
	/// Recognized log file-types
	enum LogType {
		LatexLog,
		LiterateLog,
		Lyx2lyxLog,
		VCLog
	};

	LogType type_;
	/// The log as input by params
	support::FileName logfile_;
	/// The log file to display
	support::FileName real_logfile_;
};


} // namespace frontend
} // namespace lyx

#endif // GUILOG_H
