/**
 * \file Dialog.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "FileDialog.h"
#include "GuiApplication.h"
#include "GuiDialog.h"
#include "GuiView.h"
#include "qt_helpers.h"

#include "support/debug.h"
#include "support/filetools.h"

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QRegularExpression>

using namespace std;

namespace lyx {
namespace frontend {

GuiDialog::GuiDialog(GuiView & lv, QString const & name, QString const & title)
	: QDialog(&lv), Dialog(lv, name, title), updating_(false),
      is_closing_(false), apply_stopped_(false)
{
	connect(&lv, SIGNAL(bufferViewChanged()),
	        this, SLOT(onBufferViewChanged()));
	connect(&lv, SIGNAL(closing(int)),
		this, SLOT(onClosing(int)));

	// remove question marks from Windows dialogs
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}


void GuiDialog::closeEvent(QCloseEvent * ev)
{
	slotClose();
	ev->accept();
}


void GuiDialog::setButtonsValid(bool valid)
{
	bc().setValid(valid);
}


void GuiDialog::slotApply()
{
	setApplyStopped(false);
	apply();
	if (applyStopped())
		return;
	bc().apply();
}


void GuiDialog::slotAutoApply()
{
	apply();
	bc().autoApply();
}


void GuiDialog::slotOK()
{
	is_closing_ = true;
	setApplyStopped(false);
	apply();
	if (applyStopped())
		return;
	is_closing_ = false;
	hideView();
	bc().ok();
}


void GuiDialog::slotClose()
{
	hideView();
	bc().cancel();
}


void GuiDialog::slotRestore()
{
	// Tell the controller that a request to refresh the dialog's contents
	// has been received. It's up to the controller to supply the necessary
	// info by calling GuiDialog::updateView().
	updateDialog();
	bc().restore();
}


void GuiDialog::slotButtonBox(QAbstractButton * button)
{
	QDialogButtonBox * bbox = qobject_cast<QDialogButtonBox*>(sender());
	switch (bbox->standardButton(button)) {
	case QDialogButtonBox::Ok:
		slotOK();
		break;
	case QDialogButtonBox::Apply:
		slotApply();
		break;
	case QDialogButtonBox::Cancel:
	case QDialogButtonBox::Close:
		slotClose();
		break;
	case QDialogButtonBox::Reset:
		slotRestore();
		break;
	case QDialogButtonBox::RestoreDefaults:
		slotRestoreDefaults();
		break;
	default:
		break;
	}
}


void GuiDialog::changed()
{
	if (updating_)
		return;
	bc().setValid(isValid());
}


void GuiDialog::enableView(bool enable)
{
	if (!enable) {
		bc().setReadOnly(true);
		bc().setValid(false);
	}
	Dialog::enableView(enable);
}


void GuiDialog::updateView()
{
	setUpdatesEnabled(false);

	bc().setReadOnly(isBufferReadonly());
	// protect the BC from unwarranted state transitions
	updating_ = true;
	updateContents();
	updating_ = false;
	// The widgets may not be valid, so refresh the button controller
	bc().refresh();

	setUpdatesEnabled(true);
}

QString GuiDialog::browseFile(QString const & filename,
	QString const & title,
	QStringList const & filters,
	bool save,
	QString const & label1,
	QString const & dir1,
	QString const & label2,
	QString const & dir2,
	QString const & fallback_dir)
{
	QString lastPath = ".";
	if (!filename.isEmpty())
		lastPath = onlyPath(filename);
	else if(!fallback_dir.isEmpty())
		lastPath = fallback_dir;

	FileDialog dlg(title);
	dlg.setButton1(label1, dir1);
	dlg.setButton2(label2, dir2);

	FileDialog::Result result;

	if (save)
		result = dlg.save(lastPath, filters, onlyFileName(filename));
	else
		result = dlg.open(lastPath, filters, onlyFileName(filename));

	if (guiApp->platformName() == "cocoa") {
		QWidget * dialog = asQWidget();
		dialog->raise();
		dialog->activateWindow();
	}

	return result.second;
}


/** Launch a file dialog and return the chosen directory.
	pathname: a suggested pathname.
	title: the title of the dialog.
	dir1 = (name, dir), dir2 = (name, dir): extra buttons on the dialog.
*/
QString GuiDialog::browseDir(QString const & pathname,
	QString const & title,
	QString const & label1,
	QString const & dir1,
	QString const & label2,
	QString const & dir2)
{
	QString lastPath = ".";
	if (!pathname.isEmpty())
		lastPath = onlyPath(pathname);

	FileDialog dlg(title);
	dlg.setButton1(label1, dir1);
	dlg.setButton2(label2, dir2);

	FileDialog::Result const result =
		dlg.opendir(lastPath, onlyFileName(pathname));
	
	if (guiApp->platformName() == "cocoa") {
		QWidget * dialog = asQWidget();
		dialog->raise();
		dialog->activateWindow();
	}

	return result.second;
}

QString GuiDialog::browseRelToParent(
	QString const & filename,
	QString const & relpath,
	QString const & title,
	QStringList const & filters,
	bool save,
	QString const & label1,
	QString const & dir1,
	QString const & label2,
	QString const & dir2)
{
	QString const fname = makeAbsPath(filename, relpath);

	QString const outname =
		browseFile(fname, title, filters, save, label1, dir1, label2, dir2);

	QString const reloutname =
		toqstr(support::makeRelPath(qstring_to_ucs4(outname), qstring_to_ucs4(relpath)));

	if (reloutname.startsWith("../"))
		return outname;
	else
		return reloutname;
}


QString GuiDialog::browseRelToSub(
	QString const & filename,
	QString const & relpath,
	QString const & title,
	QStringList const & filters,
	bool save,
	QString const & label1,
	QString const & dir1,
	QString const & label2,
	QString const & dir2)
{
	QString const fname = makeAbsPath(filename, relpath);

	QString const outname =
		browseFile(fname, title, filters, save, label1, dir1, label2, dir2);

	QString const reloutname =
		toqstr(support::makeRelPath(qstring_to_ucs4(outname), qstring_to_ucs4(relpath)));

	QString testname = reloutname;
	testname.remove(QRegularExpression("^(\\.\\./)+"));

	if (testname.contains("/"))
		return outname;
	else
		return reloutname;
}


QColor GuiDialog::getColor(const QColor &initial, QWidget *parent)
{
	const QColor color = QColorDialog::getColor(initial, parent);
	if (guiApp->platformName() == "cocoa") {
		QWidget * dialog = parent->window();
		// On Mac explicitly activate the parents top-level widget
		// See #10740
		dialog->raise();
		dialog->activateWindow();
	}
	return color;
}

QColor GuiDialog::getColor(const QColor &initial)
{
	return getColor(initial, asQWidget());
}

} // namespace frontend
} // namespace lyx

#include "moc_GuiDialog.cpp"
