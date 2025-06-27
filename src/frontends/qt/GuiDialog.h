// -*- C++ -*-
/**
 * \file GuiDialog.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUIDIALOG_H
#define GUIDIALOG_H

#include "Dialog.h"
#include "ButtonController.h"

#include <QAbstractButton>
#include <QDialog>


namespace lyx {
namespace frontend {

/// Base class for historical LyX dialogs.
/**
  * \warning New dialogs should use the leaner classes \c DialogView or
  * \c DockView depending on the intent. Eventually, old dialogs should be
  * converted to \c DialogView too.
  */
class GuiDialog : public QDialog, public Dialog
{
	Q_OBJECT

public:
	/// \param lv is the access point for the dialog to the LyX kernel.
	/// \param name is the identifier given to the dialog by its parent
	/// container.
	/// \param title is the window title used for decoration.
	GuiDialog(GuiView & lv, QString const & name, QString const & title);

	QWidget * asQWidget() override { return this; }
	QWidget const * asQWidget() const override { return this; }

public Q_SLOTS:
	/** \name Buttons
	 *  These methods are publicly accessible because they are invoked
	 *  by the View when the user presses... guess what ;-)
	 */
	// Restore button clicked
	void slotRestore();
	// Restore Defaults button clicked
	virtual void slotRestoreDefaults() {}
	// OK button clicked
	void slotOK();
	// Apply button clicked
	void slotApply();
	// AutoApply checkbox clicked
	void slotAutoApply();
	// Close button clicked or closed from WindowManager
	void slotClose();
	// A collectiong slot for QDialogButtonBox
	void slotButtonBox(QAbstractButton *);
	///
	void closeEvent(QCloseEvent * e) override;

protected Q_SLOTS:
	void onBufferViewChanged() override {}
	void onClosing(int) override {}

public:
	/** Check whether we may apply our data.
	 *
	 *  The buttons are disabled if not and (re-)enabled if yes.
	 */
	void setButtonsValid(bool valid);

	// Set whether to stop the apply process
	void setApplyStopped(bool stop) { apply_stopped_ = stop; }

	/** \name Dialog Components
	 *  Methods to access the various components making up a dialog.
	 */
	//@{
	ButtonController const & bc() const { return bc_; }
	ButtonController & bc() { return bc_; }
	//@}

	/// the dialog has changed contents
	virtual void changed();

	void enableView(bool enable) override;

	/// default: do nothing
	void applyView() override {}
	/// default: do nothing
	virtual void updateContents() {}

public:
	/// is the dialog currently valid ?
	virtual bool isValid() { return true; }

public:

	/** When applying, it's useful to know whether the dialog is about
	 *  to close or not (no point refreshing the display for example).
	 */
	bool isClosing() const override { return is_closing_; }

	///
	bool needBufferOpen() const override { return isBufferDependent(); }

	/// Update the display of the dialog whilst it is still visible.
	void updateView() override;


	/** Launch a file dialog and return the chosen file.
		filename: a suggested filename.
		title: the title of the dialog.
		filters: *.ps etc.
		dir1 = (name, dir), dir2 = (name, dir): extra buttons on the dialog.
	 */
	QString browseFile(QString const & filename,
		QString const & title,
		QStringList const & filters,
		bool save = false,
		QString const & label1 = QString(),
		QString const & dir1 = QString(),
		QString const & label2 = QString(),
		QString const & dir2 = QString(),
		QString const & fallback_dir = QString());
	/** Launch a file dialog and return the chosen directory.
		pathname: a suggested pathname.
		title: the title of the dialog.
		dir1 = (name, dir), dir2 = (name, dir): extra buttons on the dialog.
	*/
	QString browseDir(QString const & pathname,
		QString const & title,
		QString const & label1 = QString(),
		QString const & dir1 = QString(),
		QString const & label2 = QString(),
		QString const & dir2 = QString());
	/** Wrappers around browseFile which try to provide a filename relative to relpath.

	\param title: title for dialog

	\param filters: *.ps, etc

	\param save: whether to save dialog info (current path, etc) for next use.

	The \param labelN and \param dirN arguments provide for extra buttons
	in the dialog (e.g., "Templates" and a path to that directory).

	The difference between the functions concerns when we think we have a
	relative path.

	In \c browseRelToParent, we return a relative path only if it IS NOT of
		the form "../../foo.txt".

	In \c browseRelToSub, we return a relative path only if it IS of the
	 form "../../foo.txt".
	 */
	QString browseRelToParent(QString const & filename,
		QString const & relpath,
		QString const & title,
		QStringList const & filters,
		bool save = false,
		QString const & label1 = QString(),
		QString const & dir1 = QString(),
		QString const & label2 = QString(),
		QString const & dir2 = QString());
	QString browseRelToSub(QString const & filename,
		QString const & relpath,
		QString const & title,
		QStringList const & filters,
		bool save = false,
		QString const & label1 = QString(),
		QString const & dir1 = QString(),
		QString const & label2 = QString(),
		QString const & dir2 = QString());

	static QColor getColor(const QColor &initial, QWidget *parent);
	QColor getColor(const QColor &initial);

private:
	ButtonController bc_;
	/// are we updating ?
	bool updating_;

	bool is_closing_;

	/// stop the apply process?
	bool applyStopped() { return apply_stopped_; }
	bool apply_stopped_;
};


} // namespace frontend
} // namespace lyx

#endif // GUIDIALOG_H
