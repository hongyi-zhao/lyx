// -*- C++ -*-
/**
 * \file GuiRef.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUIREF_H
#define GUIREF_H

#include "GuiDialog.h"
#include "ui_RefUi.h"

#include "insets/InsetCommandParams.h"

#include <vector>

class QTreeWidgetItem;

namespace lyx {
namespace frontend {

class FancyLineEdit;

class GuiRef : public GuiDialog, public Ui::RefUi
{
	Q_OBJECT

public:
	GuiRef(GuiView & lv);
	/// Dialog inherited methods
	//@{
	void enableView(bool enable) override;
	//@}

private Q_SLOTS:
	void changed_adaptor();
	void gotoClicked();
	void filterLabels();
	void resetFilter();
	void refHighlighted(QTreeWidgetItem *);
	void selectionChanged();
	void refTextChanged();
	void refSelected(QTreeWidgetItem *);
	void sortToggled();
	void groupToggled();
	void on_buttonBox_clicked(QAbstractButton *);
	void updateClicked();
	void resetDialog();
	void dialogRejected();
	void updateButtons();
	void addClicked();
	void deleteClicked();
	void upClicked();
	void downClicked();
	void typeChanged();

private:
	///
	bool isBufferDependent() const override { return true; }
	/** disconnect from the inset when the Apply button is pressed.
	 Allows easy insertion of multiple references. */
	bool disconnectOnApply() const override { return true; }
	///
	void goToRef(std::string const &);
	///
	void gotoBookmark();
	///
	void closeEvent(QCloseEvent * e) override;
	///
	bool isValid() override;
	/// apply changes
	void applyView() override;
	/// update dialog
	void updateContents() override;
	///
	void enableBoxes();

	/// go to current reference
	void gotoRef();
	/// set go back button
	void setGoBack();
	/// set goto ref button
	void setGotoRef();
	/// re-enter references
	void redoRefs();
	/// update references
	void updateRefs();
	///
	bool initialiseParams(std::string const & data) override;
	/// clean-up on hide.
	void clearParams() override { params_.clear(); }
	/// clean-up on hide.
	void dispatchParams() override;

private:
	///
	InsetCommandParams params_;
	///
	virtual void updateAddPB();
	///
	virtual void updateDelPB();
	///
	virtual void updateDownPB();
	///
	virtual void updateUpPB();
	///
	bool isSelected(const QModelIndex & idx);

	/// contains the search box
	FancyLineEdit * filter_;

	/// went to a reference ?
	bool at_ref_;
	/// the last reference entered or examined
	QString last_reference_;
	/// store the buffer settings
	int restored_buffer_;
	/// store the last active buffer
	int active_buffer_;
	/// the references as three strings: plain label name, label as gui
	/// string, and pretty dereferenced name ("Lemma 3")
	/// FIXME: might be a good idea to use a custom struct
	std::vector<std::tuple<docstring, docstring, docstring>> refs_;
};

} // namespace frontend
} // namespace lyx

#endif // GUIREF_H
