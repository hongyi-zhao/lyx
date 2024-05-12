/**
 * \file GuiLog.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Angus Leeming
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiLog.h"

#include "GuiApplication.h"
#include "qt_helpers.h"

#include "frontends/Clipboard.h"

#include "support/docstring.h"
#include "support/gettext.h"
#include "support/Lexer.h"
#include "support/lstrings.h"

#include <QTextBrowser>
#include <QSyntaxHighlighter>
#include <QClipboard>

#include <fstream>
#include <sstream>

using namespace std;
using namespace lyx::support;

namespace lyx {
namespace frontend {


// Regular expressions needed at several places
// FIXME: These regexes are incomplete. It would be good if we could collect those used in LaTeX::scanLogFile
//        and LaTeX::scanBlgFile and re-use them here!(spitz, 2013-05-27)
// Information
QRegularExpression exprInfo("^(Document Class:|LaTeX Font Info:|File:|Package:|Language:|.*> INFO - |\\(|\\\\).*$");
// Warnings
QRegularExpression exprWarning("^(## Warning|LaTeX Warning|LaTeX Font Warning|Package [\\w\\-\\.]+ Warning|Class \\w+ Warning|Warning--|Underfull|Overfull|.*> WARN - ).*$");
// Errors
QRegularExpression exprError("^(ERROR: |!|.*---line [0-9]+ of file|.*> FATAL - |.*> ERROR - |Missing character: There is no ).*$");


/////////////////////////////////////////////////////////////////////
//
// LogHighlighter
//
////////////////////////////////////////////////////////////////////

class LogHighlighter : public QSyntaxHighlighter
{
public:
	LogHighlighter(QTextDocument * parent);

private:
	void highlightBlock(QString const & text) override;

private:
	QTextCharFormat infoFormat;
	QTextCharFormat warningFormat;
	QTextCharFormat errorFormat;
};



LogHighlighter::LogHighlighter(QTextDocument * parent)
	: QSyntaxHighlighter(parent)
{
	infoFormat.setForeground(Qt::darkGray);
	warningFormat.setForeground(Qt::darkBlue);
	errorFormat.setForeground(Qt::red);
}


void LogHighlighter::highlightBlock(QString const & text)
{
	// Info
	QRegularExpressionMatch match = exprInfo.match(text);
	int index = match.capturedStart(1);
	while (index >= 0) {
		int length = match.capturedEnd(1) - index;
		setFormat(index, length, infoFormat);
		match = exprInfo.match(text, index + length);
		index = match.capturedStart(1);
	}
	// LaTeX Warning:
	match = exprWarning.match(text);
	index = match.capturedStart(1);
	while (index >= 0) {
		int length = match.capturedEnd(1) - index;
		setFormat(index, length, warningFormat);
		match = exprWarning.match(text, index + length);
		index = match.capturedStart(1);
	}
	// ! error
	match = exprError.match(text);
	index = match.capturedStart(1);
	while (index >= 0) {
		int length = match.capturedEnd(1) - index;
		setFormat(index, length, errorFormat);
		match = exprError.match(text, index + length);
		index = match.capturedStart(1);
	}
}


/////////////////////////////////////////////////////////////////////
//
// GuiLog
//
/////////////////////////////////////////////////////////////////////

GuiLog::GuiLog(GuiView & lv)
	: GuiDialog(lv, "log", qt_("LaTeX Log")), type_(LatexLog)
{
	setupUi(this);

	connect(buttonBox, SIGNAL(clicked(QAbstractButton *)),
		this, SLOT(slotButtonBox(QAbstractButton *)));
	connect(updatePB, SIGNAL(clicked()), this, SLOT(updateContents()));
	connect(findPB, SIGNAL(clicked()), this, SLOT(find()));
	connect(findLE, SIGNAL(returnPressed()), this, SLOT(find()));
	connect(logTypeCO, SIGNAL(activated(int)),
		this, SLOT(typeChanged(int)));

	bc().setPolicy(ButtonPolicy::OkCancelPolicy);

	// set syntax highlighting
	highlighter = new LogHighlighter(logTB->document());

	logTB->setReadOnly(true);
	logTB->setFont(guiApp->typewriterSystemFont());

	QPushButton * closePB = buttonBox->button(QDialogButtonBox::Close);
	closePB->setAutoDefault(false);
}


void GuiLog::updateContents()
{
	setTitle(toqstr(title()));

	ostringstream ss;
	getContents(ss);

	logTB->setPlainText(toqstr(ss.str()));

	nextErrorPB->setEnabled(contains(exprError));
	nextWarningPB->setEnabled(contains(exprWarning));
}


void GuiLog::typeChanged(int i)
{
	string const type =
		fromqstr(logTypeCO->itemData(i).toString());
	string ext;
	if (type == "latex")
		ext = "log";
	else if (type == "bibtex")
		ext = "blg";
	else if (type == "index")
		ext = "ilg";

	if (!ext.empty())
		logfile_.changeExtension(ext);

	updateContents();
}


void GuiLog::find()
{
	logTB->find(findLE->text());
}


void GuiLog::on_nextErrorPB_clicked()
{
	goTo(exprError);
}


void GuiLog::on_nextWarningPB_clicked()
{
	goTo(exprWarning);
}


void GuiLog::on_openDirPB_clicked()
{	
	showDirectory(logfile_.onlyPath());
}


void GuiLog::goTo(QRegularExpression const & exp) const
{
	QTextCursor const newc =
		logTB->document()->find(exp, logTB->textCursor());
	logTB->setTextCursor(newc);
}


bool GuiLog::contains(QRegularExpression const & exp) const
{
	return !logTB->document()->find(exp, logTB->textCursor()).isNull();
}


bool GuiLog::initialiseParams(string const & sdata)
{
	istringstream is(sdata);
	Lexer lex;
	lex.setStream(is);

	string logtype, logfile;
	lex >> logtype;
	if (lex) {
		lex.next(true);
		logfile = lex.getString();
	}
	if (!lex)
		// Parsing of the data failed.
		return false;

	logTypeCO->setEnabled(logtype == "latex");
	logTypeCO->clear();

	FileName log(logfile);

	if (logtype == "latex") {
		type_ = LatexLog;
		logTypeCO->addItem(qt_("LaTeX"), toqstr(logtype));
		FileName tmp = log;
		tmp.changeExtension("blg");
		if (tmp.exists()) {
			if (support::contains(tmp.fileContents("UTF-8"), from_ascii("This is Biber")))
				logTypeCO->addItem(qt_("Biber"), QString("bibtex"));
			else
				logTypeCO->addItem(qt_("BibTeX"), QString("bibtex"));
		}
		tmp.changeExtension("ilg");
		if (tmp.exists())
			logTypeCO->addItem(qt_("Index"), QString("index"));
	// FIXME: not sure "literate" still works.
	} else if (logtype == "literate") {
		type_ = LiterateLog;
		logTypeCO->addItem(qt_("Literate"), toqstr(logtype));
	} else if (logtype == "lyx2lyx") {
		type_ = Lyx2lyxLog;
		logTypeCO->addItem(qt_("LyX2LyX"), toqstr(logtype));
	} else if (logtype == "vc") {
		type_ = VCLog;
		logTypeCO->addItem(qt_("Version Control"), toqstr(logtype));
	} else
		return false;

	logfile_ = log;

	updateContents();

	return true;
}


void GuiLog::clearParams()
{
	logfile_.erase();
}


docstring GuiLog::title() const
{
	switch (type_) {
	case LatexLog:
		return _("LaTeX Log");
	case LiterateLog:
		return _("Literate Programming Build Log");
	case Lyx2lyxLog:
		return _("lyx2lyx Error Log");
	case VCLog:
		return _("Version Control Log");
	default:
		return docstring();
	}
}


void GuiLog::getContents(ostream & ss) const
{
	ifstream in(logfile_.toFilesystemEncoding().c_str());

	bool success = false;

	// FIXME UNICODE
	// Our caller interprets the file contents as UTF8, but is that
	// correct?
	// spitz: No it isn't (generally). The log file encoding depends on the TeX
	// _output_ encoding (T1 etc.). We should account for that. See #10728.
	if (in) {
		ss << in.rdbuf();
		success = ss.good();
	}

	if (success)
		return;

	switch (type_) {
	case LatexLog:
		ss << to_utf8(_("Log file not found."));
		break;
	case LiterateLog:
		ss << to_utf8(_("No literate programming build log file found."));
		break;
	case Lyx2lyxLog:
		ss << to_utf8(_("No lyx2lyx error log file found."));
		break;
	case VCLog:
		ss << to_utf8(_("No version control log file found."));
		break;
	}
}


} // namespace frontend
} // namespace lyx

#include "moc_GuiLog.cpp"
