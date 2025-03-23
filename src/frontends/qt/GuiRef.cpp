/**
 * \file GuiRef.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiRef.h"

#include "GuiApplication.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferList.h"
#include "BufferView.h"
#include "Cursor.h"
#include "FancyLineEdit.h"
#include "FuncRequest.h"
#include "PDFOptions.h"

#include "qt_helpers.h"

#include "insets/InsetRef.h"

#include "support/FileName.h"
#include "support/FileNameList.h"
#include "support/filetools.h" // makeAbsPath, makeDisplayPath

#include <QLineEdit>
#include <QCheckBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QToolTip>
#include <QCloseEvent>
#include <QHeaderView>

using namespace std;
using namespace lyx::support;

namespace lyx {
namespace frontend {

GuiRef::GuiRef(GuiView & lv)
	: GuiDialog(lv, "ref", qt_("Cross-reference")),
	  params_(insetCode("ref"))
{
	setupUi(this);

	at_ref_ = false;

	// The filter bar
	filter_ = new FancyLineEdit(this);
	filter_->setClearButton(true);
	filter_->setPlaceholderText(qt_("All available labels"));
	filter_->setToolTip(qt_("Enter string to filter the list of available labels"));
	connect(filter_, &FancyLineEdit::downPressed,
	        refsTW, [this](){ focusAndHighlight(refsTW); });

	filterBarL->addWidget(filter_, 0);
	findKeysLA->setBuddy(filter_);

	sortingCO->addItem(qt_("By Occurrence"), "unsorted");
	sortingCO->addItem(qt_("Alphabetically (Case-Insensitive)"), "nocase");
	sortingCO->addItem(qt_("Alphabetically (Case-Sensitive)"), "case");

	buttonBox->button(QDialogButtonBox::Reset)->setText(qt_("&Update"));
	buttonBox->button(QDialogButtonBox::Reset)->setToolTip(qt_("Update the label list"));

	connect(this, SIGNAL(rejected()), this, SLOT(dialogRejected()));

	connect(typeCO, SIGNAL(activated(int)),
		this, SLOT(changed_adaptor()));
	connect(referenceED, SIGNAL(textChanged(QString)),
		this, SLOT(refTextChanged(QString)));
	connect(referenceED, SIGNAL(textChanged(QString)),
		this, SLOT(changed_adaptor()));
	connect(filter_, SIGNAL(textEdited(QString)),
		this, SLOT(filterLabels()));
	connect(filter_, SIGNAL(rightButtonClicked()),
		this, SLOT(resetFilter()));
	connect(csFindCB, SIGNAL(clicked()),
		this, SLOT(filterLabels()));
	connect(refsTW, SIGNAL(itemClicked(QTreeWidgetItem *, int)),
		this, SLOT(refHighlighted(QTreeWidgetItem *)));
	connect(refsTW, SIGNAL(itemSelectionChanged()),
		this, SLOT(selectionChanged()));
	connect(refsTW, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
		this, SLOT(refSelected(QTreeWidgetItem *)));
	connect(sortingCO, SIGNAL(activated(int)),
		this, SLOT(sortToggled()));
	connect(groupCB, SIGNAL(clicked()),
		this, SLOT(groupToggled()));
	connect(gotoPB, SIGNAL(clicked()),
		this, SLOT(gotoClicked()));
	connect(bufferCO, SIGNAL(activated(int)),
		this, SLOT(updateClicked()));
	connect(pluralCB, SIGNAL(clicked()),
		this, SLOT(changed_adaptor()));
	connect(capsCB, SIGNAL(clicked()),
		this, SLOT(changed_adaptor()));
	connect(noprefixCB, SIGNAL(clicked()),
		this, SLOT(changed_adaptor()));
	connect(nolinkCB, SIGNAL(clicked()),
		this, SLOT(changed_adaptor()));

	enableBoxes();

	bc().setPolicy(ButtonPolicy::NoRepeatedApplyReadOnlyPolicy);
	bc().setOK(buttonBox->button(QDialogButtonBox::Ok));
	bc().setApply(buttonBox->button(QDialogButtonBox::Apply));
	bc().setCancel(buttonBox->button(QDialogButtonBox::Cancel));
	bc().addReadOnly(typeCO);

	restored_buffer_ = -1;
	active_buffer_ = -1;

	setFocusProxy(filter_);
}


void GuiRef::enableView(bool enable)
{
	if (!enable)
		// In the opposite case, updateContents() will be called anyway.
		updateContents();
	GuiDialog::enableView(enable);
}


void GuiRef::enableBoxes()
{
	QString const reftype =
		typeCO->itemData(typeCO->currentIndex()).toString();
	bool const use_refstyle = buffer().params().xref_package == "refstyle";
	bool const use_cleveref = buffer().params().xref_package == "cleveref";
	bool const isFormatted = (reftype == "formatted");
	bool const isLabelOnly = (reftype == "labelonly");
	bool const hyper_on = buffer().params().pdfoptions().use_hyperref;
	bool const cleveref_nameref = use_cleveref && reftype == "nameref"
			&& (!hyper_on || nolinkCB->isChecked());
	bool const allow_plural = use_refstyle || cleveref_nameref;
	bool const allow_caps = use_refstyle || use_cleveref;
	bool const allow_nohyper = !isLabelOnly && (!isFormatted || use_cleveref);
	bool const intext = bufferview()->cursor().inTexted();
	pluralCB->setEnabled(intext && (isFormatted || cleveref_nameref) && allow_plural);
	capsCB->setEnabled(intext && (isFormatted || cleveref_nameref) && allow_caps);
	noprefixCB->setEnabled(intext && isLabelOnly);
	// disabling of hyperlinks not supported by formatted references
	nolinkCB->setEnabled(hyper_on && intext && allow_nohyper);
}


void GuiRef::changed_adaptor()
{
	changed();
	enableBoxes();
}


void GuiRef::gotoClicked()
{
	// By setting last_reference_, we ensure that the reference
	// to which we are going (or from which we are returning) is
	// restored in the dialog. It's a bit of a hack, but it works,
	// and no-one seems to have any better idea.
	bool const toggled =
		last_reference_.isEmpty() || last_reference_.isNull();
	if (toggled)
		last_reference_ = referenceED->text();
	gotoRef();
	if (toggled)
		last_reference_.clear();
}


void GuiRef::selectionChanged()
{
	if (isBufferReadonly())
		return;

	QList<QTreeWidgetItem *> selections = refsTW->selectedItems();
	if (selections.isEmpty())
		return;
	QTreeWidgetItem * sel = selections.first();
	refHighlighted(sel);
}


void GuiRef::refHighlighted(QTreeWidgetItem * sel)
{
	if (sel->childCount() > 0) {
		sel->setExpanded(true);
		return;
	}

/*	int const cur_item = refsTW->currentRow();
	bool const cur_item_selected = cur_item >= 0 ?
		refsLB->isSelected(cur_item) : false;*/
	bool const cur_item_selected = sel->isSelected();

	if (cur_item_selected)
		referenceED->setText(sel->data(0, Qt::UserRole).toString());

	if (at_ref_)
		gotoRef();
	gotoPB->setEnabled(true);
	if (!isBufferReadonly())
		typeCO->setEnabled(true);
}


void GuiRef::refTextChanged(QString const & str)
{
	gotoPB->setEnabled(!str.isEmpty());
	typeCO->setEnabled(!str.isEmpty());
	typeLA->setEnabled(!str.isEmpty());
}


void GuiRef::refSelected(QTreeWidgetItem * sel)
{
	if (isBufferReadonly())
		return;

	if (sel->childCount()) {
		sel->setExpanded(false);
		return;
	}

/*	int const cur_item = refsTW->currentRow();
	bool const cur_item_selected = cur_item >= 0 ?
		refsLB->isSelected(cur_item) : false;*/
	bool const cur_item_selected = sel->isSelected();

	if (cur_item_selected)
		referenceED->setText(sel->data(0, Qt::UserRole).toString());
	// <enter> or double click, inserts ref and closes dialog
	slotOK();
}


void GuiRef::sortToggled()
{
	redoRefs();
}


void GuiRef::groupToggled()
{
	redoRefs();
}


void GuiRef::on_buttonBox_clicked(QAbstractButton * button)
{
	switch (buttonBox->standardButton(button)) {
	case QDialogButtonBox::Ok:
		slotOK();
		break;
	case QDialogButtonBox::Apply:
		slotApply();
		break;
	case QDialogButtonBox::Cancel:
		slotClose();
		resetDialog();
		break;
	case QDialogButtonBox::Reset:
		updateClicked();
		break;
	default:
		break;
	}
}


void GuiRef::updateClicked()
{
	updateRefs();
}


void GuiRef::dialogRejected()
{
	resetDialog();
	// We have to do this manually, instead of calling slotClose(), because
	// the dialog has already been made invisible before rejected() triggers.
	Dialog::disconnect();
}


void GuiRef::resetDialog()
{
	at_ref_ = false;
	setGotoRef();
}


void GuiRef::closeEvent(QCloseEvent * e)
{
	slotClose();
	resetDialog();
	e->accept();
}


void GuiRef::updateContents()
{
	QString const orig_type =
		typeCO->itemData(typeCO->currentIndex()).toString();

	referenceED->clear();
	typeCO->clear();

	// FIXME Bring InsetMathRef on par with InsetRef
	// (see #11104)
	typeCO->addItem(qt_("<reference>"), "ref");
	typeCO->addItem(qt_("(<reference>)"), "eqref");
	typeCO->addItem(qt_("<page>"), "pageref");
	typeCO->addItem(qt_("on page <page>"), "vpageref");
	typeCO->addItem(qt_("<reference> on page <page>"), "vref");
	typeCO->addItem(qt_("Textual reference"), "nameref");
	typeCO->addItem(qt_("Formatted reference"), "formatted");
	typeCO->addItem(qt_("Label only"), "labelonly");

	referenceED->setText(toqstr(params_["reference"]));

	// restore type settings for new insets
	bool const new_inset = params_["reference"].empty();
	if (new_inset) {
		int index = typeCO->findData(orig_type);
		if (index == -1)
			index = 0;
		typeCO->setCurrentIndex(index);
	}
	else
		typeCO->setCurrentIndex(
			typeCO->findData(toqstr(params_.getCmdName())));
	typeCO->setEnabled(!isBufferReadonly());

	pluralCB->setChecked(params_["plural"] == "true");
	capsCB->setChecked(params_["caps"] == "true");
	noprefixCB->setChecked(params_["noprefix"] == "true");
	nolinkCB->setChecked(params_["nolink"] == "true");

	// insert buffer list
	bufferCO->clear();
	FileNameList const buffers(theBufferList().fileNames());
	for (FileNameList::const_iterator it = buffers.begin();
	     it != buffers.end(); ++it) {
		bufferCO->addItem(toqstr(makeDisplayPath(it->absFileName())));
	}

	int const thebuffer = theBufferList().bufferNum(buffer().fileName());
	// restore the buffer combo setting for new insets
	if (new_inset && restored_buffer_ != -1
	    && restored_buffer_ < bufferCO->count() && thebuffer == active_buffer_)
		bufferCO->setCurrentIndex(restored_buffer_);
	else {
		int const num = theBufferList().bufferNum(buffer().fileName());
		bufferCO->setCurrentIndex(num);
		if (thebuffer != active_buffer_)
			restored_buffer_ = num;
	}
	active_buffer_ = thebuffer;

	updateRefs();
	enableBoxes();
	// Activate OK/Apply buttons if the users inserts a new ref
	// and we have a valid pre-setting.
	bc().setValid(isValid() && new_inset);
}


void GuiRef::applyView()
{
	last_reference_ = referenceED->text();

	params_.setCmdName(fromqstr(typeCO->itemData(typeCO->currentIndex()).toString()));
	params_["reference"] = qstring_to_ucs4(last_reference_);
	params_["plural"] = pluralCB->isChecked() ?
	      from_ascii("true") : from_ascii("false");
	params_["caps"] = capsCB->isChecked() ?
	      from_ascii("true") : from_ascii("false");
	params_["noprefix"] = noprefixCB->isChecked() ?
	      from_ascii("true") : from_ascii("false");
	params_["nolink"] = nolinkCB->isChecked() ?
	      from_ascii("true") : from_ascii("false");
	restored_buffer_ = bufferCO->currentIndex();
}


void GuiRef::setGoBack()
{
	gotoPB->setText(qt_("&Go Back"));
	gotoPB->setToolTip(qt_("Jump back to the original cursor location"));
}


void GuiRef::setGotoRef()
{
	gotoPB->setText(qt_("&Go to Label"));
	gotoPB->setToolTip(qt_("Jump to the selected label"));
}


void GuiRef::gotoRef()
{
	string ref = fromqstr(referenceED->text());

	if (at_ref_) {
		// go back
		setGotoRef();
		gotoBookmark();
	} else {
		// go to the ref
		setGoBack();
		gotoRef(ref);
	}
	at_ref_ = !at_ref_;
}

inline bool caseInsensitiveLessThanVec(std::tuple<QString, QString, QString> const & s1, std::tuple<QString, QString, QString> const & s2)
{
	return std::get<0>(s1).toLower() < std::get<0>(s2).toLower();
}

inline bool caseInsensitiveLessThan(QString const & s1, QString const & s2)
{
	return s1.toLower() < s2.toLower();
}


void GuiRef::redoRefs()
{
	// Prevent these widgets from emitting any signals whilst
	// we modify their state.
	refsTW->blockSignals(true);
	referenceED->blockSignals(true);
	refsTW->setUpdatesEnabled(false);

	refsTW->clear();

	// need this because Qt will send a highlight() here for
	// the first item inserted
	QString const oldSelection(referenceED->text());

	// Plain label, GUI string, and dereferenced string.
	// This might get resorted below
	QVector<std::tuple<QString, QString,QString>> refsNames;
	// List of categories (prefixes)
	QStringList refsCategories;
	// Do we have a prefix-less label at all?
	bool noprefix = false;
	for (auto const & theref : refs_) {
		// first: plain label name, second: gui name, third: pretty name
		QString const lab = toqstr(get<0>(theref));
		refsNames.append({lab, toqstr(get<1>(theref)), toqstr(get<2>(theref))});
		if (groupCB->isChecked()) {
			if (lab.contains(":")) {
				QString const pref = lab.split(':')[0];
				if (!refsCategories.contains(pref)) {
					if (!pref.isEmpty())
						refsCategories.append(pref);
					else
						noprefix = true;
				}
			}
			else
				noprefix = true;
		}
	}
	// sort categories case-insensitively
	sort(refsCategories.begin(), refsCategories.end(),
		  caseInsensitiveLessThan /*defined above*/);
	if (noprefix)
		refsCategories.insert(0, qt_("<No prefix>"));

	QString const sort_method = sortingCO->isEnabled() ?
					sortingCO->itemData(sortingCO->currentIndex()).toString()
					: QString();
	// Sort items if so requested.
	if (sort_method == "nocase")
		sort(refsNames.begin(), refsNames.end(),
			  caseInsensitiveLessThanVec /*defined above*/);
	else if (sort_method == "case")
		sort(refsNames.begin(), refsNames.end());

	if (groupCB->isChecked()) {
		QList<QTreeWidgetItem *> refsCats;
		for (int i = 0; i < refsCategories.size(); ++i) {
			QString const & cat = refsCategories.at(i);
			QTreeWidgetItem * item = new QTreeWidgetItem(refsTW);
			item->setText(0, cat);
			for (int j = 0; j < refsNames.size(); ++j) {
				QString const ref = std::get<0>(refsNames.at(j));
				if ((ref.startsWith(cat + QString(":")))
				    || (cat == qt_("<No prefix>")
				       && (!ref.mid(1).contains(":") || ref.left(1).contains(":")))) {
						QTreeWidgetItem * child =
							new QTreeWidgetItem(item);
						QString const val = std::get<1>(refsNames.at(j));
						QString const pretty = std::get<2>(refsNames.at(j));
						child->setText(0, val);
						child->setData(0, Qt::UserRole, ref);
						child->setText(1, pretty);
						item->addChild(child);
				}
			}
			refsCats.append(item);
		}
		refsTW->addTopLevelItems(refsCats);
	} else {
		QList<QTreeWidgetItem *> refsItems;
		for (int i = 0; i < refsNames.size(); ++i) {
			QTreeWidgetItem * item = new QTreeWidgetItem(refsTW);
			QString const ref = std::get<0>(refsNames.at(i));
			QString const val = std::get<1>(refsNames.at(i));
			QString const pretty = std::get<2>(refsNames.at(i));
			item->setText(0, val);
			item->setData(0, Qt::UserRole, ref);
			item->setText(1, pretty);
			refsItems.append(item);
		}
		refsTW->addTopLevelItems(refsItems);
	}

	// restore the last selection or, for new insets, highlight
	// the previous selection
	if (!oldSelection.isEmpty() || !last_reference_.isEmpty()) {
		bool const newInset = oldSelection.isEmpty();
		QString textToFind = newInset ? last_reference_ : oldSelection;
		referenceED->setText(textToFind);
		last_reference_.clear();
		QTreeWidgetItemIterator it(refsTW);
		while (*it) {
			if ((*it)->text(0) == textToFind) {
				refsTW->setCurrentItem(*it);
				(*it)->setSelected(true);
				//Make sure selected item is visible
				refsTW->scrollToItem(*it);
				last_reference_ = textToFind;
				break;
			}
			++it;
		}
	}
	refsTW->setUpdatesEnabled(true);
	refsTW->update();

	// redo filter
	filterLabels();

	// Re-activate the emission of signals by these widgets.
	refsTW->blockSignals(false);
	referenceED->blockSignals(false);

	gotoPB->setEnabled(!referenceED->text().isEmpty());
	typeCO->setEnabled(!referenceED->text().isEmpty());
	typeLA->setEnabled(!referenceED->text().isEmpty());
}


void GuiRef::updateRefs()
{
	refs_.clear();
	int const the_buffer = bufferCO->currentIndex();
	if (the_buffer != -1) {
		FileNameList const names(theBufferList().fileNames());
		FileName const & name = names[the_buffer];
		Buffer const * buf = theBufferList().getBuffer(name);
		buf->getLabelList(refs_);
	}
	sortingCO->setEnabled(!refs_.empty());
	refsTW->setEnabled(!refs_.empty());
	groupCB->setEnabled(!refs_.empty());
	redoRefs();
}


bool GuiRef::isValid()
{
	return !referenceED->text().isEmpty();
}


void GuiRef::gotoRef(string const & ref)
{
	dispatch(FuncRequest(LFUN_BOOKMARK_SAVE, "0"));
	dispatch(FuncRequest(LFUN_LABEL_GOTO, ref));
}


void GuiRef::gotoBookmark()
{
	dispatch(FuncRequest(LFUN_BOOKMARK_GOTO, "0"));
}


void GuiRef::filterLabels()
{
	Qt::CaseSensitivity cs = csFindCB->isChecked() ?
		Qt::CaseSensitive : Qt::CaseInsensitive;
	QTreeWidgetItemIterator it(refsTW);
	while (*it) {
		(*it)->setHidden(
			(*it)->childCount() == 0
			&& !(*it)->text(0).contains(filter_->text(), cs)
			&& !(*it)->text(1).contains(filter_->text(), cs)
		);
		++it;
	}
}


void GuiRef::resetFilter()
{
	filter_->setText(QString());
	filterLabels();
}


bool GuiRef::initialiseParams(std::string const & sdata)
{
	InsetCommand::string2params(sdata, params_);
	return true;
}


void GuiRef::dispatchParams()
{
	std::string const lfun = InsetCommand::params2string(params_);
	dispatch(FuncRequest(getLfun(), lfun));
	connectToNewInset();
}


} // namespace frontend
} // namespace lyx

#include "moc_GuiRef.cpp"
