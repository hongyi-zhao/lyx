/**
 * \file qt/FileDialog.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Jean-Marc Lasgouttes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "FileDialog.h"

#include "LyXFileDialog.h"
#include "qt_helpers.h"

#include "LyXRC.h"

#include "support/debug.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/os.h"

#include <string>

#include <QApplication>

/** when LyXRC::use_native_filedialog is true, we use
 * QFileDialog::getOpenFileName and friends to create filedialogs.
 * Effects:
 * - the dialog does not use the quick directory buttons (Button
 *   parameters);
 * - with Qt/Mac or Qt/Win, the dialogs native to the environment are used.
 * - with Qt/Win and Qt <= 4.3.0, there was a number of bugs with our own
 *   file dialog (http://www.lyx.org/trac/ticket/3907).
 *
 * Therefore there is a tradeoff in enabling or disabling this (JMarc)
 */

namespace lyx {

using namespace support;


class FileDialog::Private {
public:
	Button b1;
	Button b2;
};


FileDialog::FileDialog(QString const & t)
	: private_(new FileDialog::Private), title_(t)
{}


FileDialog::~FileDialog()
{
	delete private_;
}


void FileDialog::setButton1(QString const & label, QString const & dir)
{
	private_->b1.first = label;
	private_->b1.second = dir;
}


void FileDialog::setButton2(QString const & label, QString const & dir)
{
	private_->b2.first = label;
	private_->b2.second = dir;
}


FileDialog::Result FileDialog::save(QString const & path,
	QStringList const & filters, QString const & suggested,
	QString * selectedFilter)
{
	LYXERR(Debug::GUI, "Select with path \"" << path
			   << "\", mask \"" << filters.join(";;")
			   << "\", suggested \"" << suggested << '"');

	FileDialog::Result result;
	result.first = FileDialog::Chosen;

	if (lyxrc.use_native_filedialog) {
		QString const startsWith = makeAbsPath(suggested, path);
		QString const name =
			QFileDialog::getSaveFileName(qApp->focusWidget(),
					title_, startsWith, filters.join(";;"),
					selectedFilter, QFileDialog::DontConfirmOverwrite);
		if (name.isNull())
			result.first = FileDialog::Later;
		else
			result.second = toqstr(os::internal_path(fromqstr(name)));
	} else {
		LyXFileDialog dlg(title_, path, filters, private_->b1, private_->b2);
		dlg.setFileMode(QFileDialog::AnyFile);
		dlg.setAcceptMode(QFileDialog::AcceptSave);
		dlg.setOption(QFileDialog::DontConfirmOverwrite, true);
		if (selectedFilter != 0 && !selectedFilter->isEmpty())
			dlg.selectNameFilter(*selectedFilter);

		if (!suggested.isEmpty())
			dlg.selectFile(suggested);

		LYXERR(Debug::GUI, "Synchronous FileDialog: ");
		int res = dlg.exec();
		LYXERR(Debug::GUI, "result " << res);
		if (res == QDialog::Accepted)
			result.second = internalPath(dlg.selectedFiles()[0]);
		else
			result.first = FileDialog::Later;
		if (selectedFilter != 0)
			*selectedFilter = dlg.selectedNameFilter();
		dlg.hide();
	}
	return result;
}


FileDialog::Result FileDialog::save(QString const & path,
	QStringList const & filters, QString const & suggested)
{
	return save(path, filters, suggested, 0);
}


FileDialog::Result FileDialog::open(QString const & path,
	QStringList const & filters, QString const & suggested)
{
	FileDialog::Result result;
	FileDialog::Results results = openMulti(path, filters, suggested, false);
	result.first = results.first;
	if (result.first != FileDialog::Later)
		result.second = results.second.at(0);
	return result;
}


FileDialog::Results FileDialog::openMulti(QString const & path,
	QStringList const & filters, QString const & suggested, bool multi)
{
	LYXERR(Debug::GUI, "Select with path \"" << path
			   << "\", mask \"" << filters.join(";;")
			   << "\", suggested \"" << suggested << '"');
	FileDialog::Results results;
	results.first = FileDialog::Chosen;

	if (lyxrc.use_native_filedialog) {
		QString const startsWith = makeAbsPath(suggested, path);
		QStringList files;
		if (multi)
			files = QFileDialog::getOpenFileNames(qApp->focusWidget(),
					title_, startsWith, filters.join(";;"));
		else
			files << QFileDialog::getOpenFileName(qApp->focusWidget(),
					title_, startsWith, filters.join(";;"));
		if (files.isEmpty())
			results.first = FileDialog::Later;
		else {
			for (const auto& file : files)
				results.second << internalPath(file);
		}
	} else {
		LyXFileDialog dlg(title_, path, filters, private_->b1, private_->b2);

		if (!suggested.isEmpty())
			dlg.selectFile(suggested);

		LYXERR(Debug::GUI, "Synchronous FileDialog: ");
		int res = dlg.exec();
		LYXERR(Debug::GUI, "result " << res);
		if (res == QDialog::Accepted)
			results.second << internalPath(dlg.selectedFiles()[0]);
		else
			results.first = FileDialog::Later;
		dlg.hide();
	}
	return results;
}


FileDialog::Result FileDialog::opendir(QString const & path,
	QString const & suggested)
{
	LYXERR(Debug::GUI, "Select with path \"" << path
			   << "\", suggested \"" << suggested << '"');
	FileDialog::Result result;
	result.first = FileDialog::Chosen;

	if (lyxrc.use_native_filedialog) {
		QString const startsWith =
			toqstr(makeAbsPath(fromqstr(suggested), fromqstr(path)).absFileName());
		QString const dir =
			QFileDialog::getExistingDirectory(qApp->focusWidget(), title_, startsWith);
		if (dir.isNull())
			result.first = FileDialog::Later;
		else
			result.second = toqstr(os::internal_path(fromqstr(dir)));
	} else {
		LyXFileDialog dlg(title_, path, QStringList(qt_("Directories")),
						  private_->b1, private_->b2);

		dlg.setFileMode(QFileDialog::Directory);
		dlg.setOption(QFileDialog::ShowDirsOnly, true);

		if (!suggested.isEmpty())
			dlg.selectFile(suggested);

		LYXERR(Debug::GUI, "Synchronous FileDialog: ");
		int res = dlg.exec();
		LYXERR(Debug::GUI, "result " << res);
		if (res == QDialog::Accepted)
			result.second = internalPath(dlg.selectedFiles()[0]);
		else
			result.first = FileDialog::Later;
		dlg.hide();
	}
	return result;
}


} // namespace lyx
