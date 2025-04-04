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
#include "Paragraph.h"
#include "TextClass.h"

#include "FancyLineEdit.h"
#include "FuncRequest.h"
#include "GuiView.h"
#include "PDFOptions.h"

#include "TocModel.h"
#include "TocBackend.h"
#include "qt_helpers.h"

#include "insets/InsetRef.h"

#include "support/FileName.h"
#include "support/FileNameList.h"
#include "support/filetools.h" // makeAbsPath, makeDisplayPath
#include "support/gettext.h"
#include "support/lstrings.h"

#include "frontends/alert.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QToolTip>
#include <QCloseEvent>
#include <QHeaderView>
#include <QAbstractItemModel>
#include <QTableWidget>

using namespace std;
using namespace lyx::support;

namespace lyx {
namespace frontend {

GuiRef::GuiRef(GuiView & lv)
	: GuiDialog(lv, "ref", qt_("Cross-reference")),
	  params_(insetCode("ref")), view_(&lv)
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

	rangeListCO->addItem(qt_("list (1 and 2)"), "list");
	rangeListCO->addItem(qt_("range (1 to 2)"), "range");

	buttonBox->button(QDialogButtonBox::Reset)->setText(qt_("&Update"));
	buttonBox->button(QDialogButtonBox::Reset)->setToolTip(qt_("Update the label list"));

	connect(this, SIGNAL(rejected()), this, SLOT(dialogRejected()));

	connect(typeCO, SIGNAL(activated(int)),
		this, SLOT(typeChanged()));
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
	connect(selectedLV, SIGNAL(itemSelectionChanged()),
		this, SLOT(updateButtons()));
	connect(selectedLV, SIGNAL(itemSelectionChanged()),
		this, SLOT(refTextChanged()));
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
	connect(targetCO, SIGNAL(activated(int)),
		this, SLOT(updateClicked()));
	connect(pluralCB, SIGNAL(clicked()),
		this, SLOT(changed_adaptor()));
	connect(capsCB, SIGNAL(clicked()),
		this, SLOT(changed_adaptor()));
	connect(noprefixCB, SIGNAL(clicked()),
		this, SLOT(changed_adaptor()));
	connect(nolinkCB, SIGNAL(clicked()),
		this, SLOT(changed_adaptor()));
	connect(refOptionsLE, SIGNAL(textChanged(QString)),
		this, SLOT(changed_adaptor()));
	connect(addPB, SIGNAL(clicked()),
		this, SLOT(addClicked()));
	connect(deletePB, SIGNAL(clicked()),
		this, SLOT(deleteClicked()));
	connect(upPB, SIGNAL(clicked()),
		this, SLOT(upClicked()));
	connect(downPB, SIGNAL(clicked()),
		this, SLOT(downClicked()));
	connect(rangeListCO, SIGNAL(activated(int)),
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
	refsTW->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
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
	bool const use_prettyref = buffer().params().xref_package == "prettyref";
	bool const use_refstyle = buffer().params().xref_package == "refstyle";
	bool const use_cleveref = buffer().params().xref_package == "cleveref";
	bool const use_zref = buffer().params().xref_package == "zref";
	bool const isFormatted = (reftype == "formatted");
	bool const isLabelOnly = (reftype == "labelonly");
	bool const hyper_on = buffer().params().pdfoptions().use_hyperref;
	bool const cleveref_nameref = use_cleveref && reftype == "nameref"
			&& (!hyper_on || nolinkCB->isChecked());
	bool const zref_clever = use_zref && (reftype == "vref" || reftype == "vpageref");
	bool const allow_plural = use_refstyle || cleveref_nameref;
	bool const allow_caps = use_refstyle || use_cleveref || use_zref;
	bool const allow_nohyper = !isLabelOnly && (!isFormatted || use_cleveref || use_zref)
			&& (reftype != "cpageref" || use_zref);
	bool const intext = bufferview()->cursor().inTexted();
	pluralCB->setEnabled(intext && (isFormatted || cleveref_nameref) && allow_plural);
	capsCB->setEnabled(intext && (isFormatted || cleveref_nameref || zref_clever || reftype == "cpageref")
			   && allow_caps);
	noprefixCB->setEnabled(intext && isLabelOnly);
	// disabling of hyperlinks not supported by formatted references
	nolinkCB->setEnabled(hyper_on && intext && allow_nohyper);
	// options only supported by zref currently
	refOptionsLE->setEnabled(use_zref && (isFormatted || zref_clever));
	refOptionsLA->setEnabled(use_zref && (isFormatted || zref_clever));
	bool const allow_range_list_switch = selectedLV->topLevelItemCount() == 2
		&& (isFormatted || reftype == "cpageref") && !use_prettyref;
	if (reftype == "vref" || reftype == "vpageref")
		rangeListCO->setCurrentIndex(rangeListCO->findData("range"));
	rangeListCO->setEnabled(allow_range_list_switch);
	rangeListLA->setEnabled(allow_range_list_switch);
}


bool GuiRef::isSelected(const QModelIndex & idx)
{
	if (!selectedLV->model() || selectedLV->model()->rowCount() == 0)
		return false;
	QVariant str = refsTW->model()->data(idx, Qt::DisplayRole);
	if (targetCO->itemData(targetCO->currentIndex()).toString() != "labels") {
		// for outliner-based items, we need to check whether the paragraph
		// has a label and see if this is already selected
		int const id = refsTW->currentItem()->data(0, Qt::UserRole).toInt();
		if (id < 0)
			return false;
		int const the_buffer = bufferCO->currentIndex();
		if (the_buffer == -1)
			return false;
		FileNameList const names(theBufferList().fileNames());
		FileName const & name = names[the_buffer];
		Buffer const * buf = theBufferList().getBuffer(name);
		if (!buf)
			return false;
		DocIterator dit = buf->getParFromID(id);
		if (dit.empty())
			return false;
		string label = dit.innerParagraph().getLabelForXRef();
		if (label.empty())
			return false;
		str = toqstr(label);
	}
	
	QModelIndexList qmil =
			selectedLV->model()->match(selectedLV->model()->index(0, 0),
			                     Qt::DisplayRole, str, 1,
			                     Qt::MatchFlags(Qt::MatchExactly | Qt::MatchWrap));
	return !qmil.empty();
}


void GuiRef::typeChanged()
{
	QString const reftype =
		typeCO->itemData(typeCO->currentIndex()).toString();
	bool const threshold = (reftype == "vref" || reftype == "vpageref")
		&& selectedLV->topLevelItemCount() > 1;
	if (threshold)
		Alert::warning(_("Unsupported setting!"),
			_("The reference type you selected allows for maximally two target labels."));
	changed_adaptor();
}


void GuiRef::updateAddPB()
{
	QString const reftype =
		typeCO->itemData(typeCO->currentIndex()).toString();
	bool const threshold = (reftype == "vref" || reftype == "vpageref")
		&& selectedLV->topLevelItemCount() > 1;
	int const arows = refsTW->model()->rowCount();
	QModelIndexList const availSels =
		refsTW->selectionModel()->selectedIndexes();
	addPB->setEnabled(arows > 0
		 && !availSels.isEmpty()
		 && !isSelected(availSels.first())
		 && !threshold);
}


void GuiRef::updateDelPB()
{
	if (!selectedLV->model()) {
		deletePB->setEnabled(false);
		return;
	}
	int const srows = selectedLV->model()->rowCount();
	if (srows == 0) {
		deletePB->setEnabled(false);
		return;
	}
	QModelIndexList const selSels =
		selectedLV->selectionModel()->selectedIndexes();
	int const sel_nr = selSels.empty() ? -1 : selSels.first().row();
	deletePB->setEnabled(sel_nr >= 0);
}


void GuiRef::updateUpPB()
{
	if (!selectedLV->model()) {
		upPB->setEnabled(false);
		return;
	}
	int const srows = selectedLV->model()->rowCount();
	if (srows == 0) {
		upPB->setEnabled(false);
		return;
	}
	QModelIndexList const selSels =
			selectedLV->selectionModel()->selectedIndexes();
	int const sel_nr = selSels.empty() ? -1 : selSels.first().row();
	upPB->setEnabled(sel_nr > 0);
}


void GuiRef::updateDownPB()
{
	if (!selectedLV->model()) {
		downPB->setEnabled(false);
		return;
	}
	int const srows = selectedLV->model()->rowCount();
	if (srows == 0) {
		downPB->setEnabled(false);
		return;
	}
	QModelIndexList const selSels =
			selectedLV->selectionModel()->selectedIndexes();
	int const sel_nr = selSels.empty() ? -1 : selSels.first().row();
	downPB->setEnabled(sel_nr >= 0 && sel_nr < srows - 1);
}


void GuiRef::updateButtons()
{
	updateAddPB();
	updateDelPB();
	updateDownPB();
	updateUpPB();
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
	if (toggled && refsTW->currentItem())
		last_reference_ = refsTW->currentItem()->data(0, Qt::UserRole).toString();
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
	updateButtons();
}


void GuiRef::refHighlighted(QTreeWidgetItem * sel)
{
	if (sel->childCount() > 0) {
		sel->setExpanded(true);
		return;
	}

	if (at_ref_)
		gotoRef();
	gotoPB->setEnabled(true);
	if (!isBufferReadonly())
		typeCO->setEnabled(true);
}


void GuiRef::refTextChanged()
{
	bool const sel = selectedLV->currentItem()
			&& selectedLV->currentItem()->isSelected();
	gotoPB->setEnabled(sel);
	typeCO->setEnabled(sel);
	typeLA->setEnabled(sel);
}


void GuiRef::refSelected(QTreeWidgetItem * sel)
{
	if (isBufferReadonly())
		return;

	if (sel->childCount()) {
		sel->setExpanded(false);
		return;
	}

	// <enter> or double click, inserts ref
	addClicked();
}


void GuiRef::sortToggled()
{
	updateAvailableLabels();
}


void GuiRef::groupToggled()
{
	updateAvailableLabels();
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


void GuiRef::addClicked()
{
	QString text = refsTW->currentItem()->data(0, Qt::UserRole).toString();
	if (targetCO->itemData(targetCO->currentIndex()).toString() != "labels") {
		dispatch(FuncRequest(LFUN_REFERENCE_TO_PARAGRAPH,
				qstring_to_ucs4(text) + " " + "forrefdialog"));
		if (bufferview()->insertedLabel().empty()) {
			frontend::Alert::error(_("Label creation error!"),
					       _("Could not auto-generate label for this target.\n"
						 "Please insert a label manually."));
			return;
		}
		text = toqstr(bufferview()->insertedLabel());
	}
	QTreeWidgetItem * item = new QTreeWidgetItem(selectedLV);
	item->setText(0, text);
	item->setData(0, Qt::UserRole, text);

	selectedLV->addTopLevelItem(item);
	selectedLV->setCurrentItem(item);

	changed_adaptor();
}


void GuiRef::deleteClicked()
{
	int const i = selectedLV->indexOfTopLevelItem(selectedLV->currentItem());
	selectedLV->takeTopLevelItem(i);
	changed_adaptor();
}


void GuiRef::upClicked()
{
	int const i = selectedLV->indexOfTopLevelItem(selectedLV->currentItem());
	if (i < 1)
		return;
	QTreeWidgetItem * item = selectedLV->takeTopLevelItem(i);
	if (item) {
		selectedLV->insertTopLevelItem(i - 1, item);
		selectedLV->setCurrentItem(item);
	}
	changed_adaptor();
}


void GuiRef::downClicked()
{
	int const i = selectedLV->indexOfTopLevelItem(selectedLV->currentItem());
	if (i == selectedLV->topLevelItemCount())
		return;
	QTreeWidgetItem * item = selectedLV->takeTopLevelItem(i);
	if (item) {
		selectedLV->insertTopLevelItem(i + 1, item);
		selectedLV->setCurrentItem(item);
	}
	changed_adaptor();
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


void GuiRef::updateTargets()
{
	QString const target = targetCO->itemData(targetCO->currentIndex()).toString();
	targetCO->clear();
	targetCO->addItem(qt_("Existing Labels"), "labels");
	if (isTargetAvailable("tableofcontents"))
		targetCO->addItem(qt_("Table of Contents"), "tableofcontents");
	for (auto const & name : buffer().params().documentClass().outlinerNames()) {
		// Use only items that make sense in this context
		// FIXME: avoid hardcoding
		if (name.first != "branch" && name.first != "index"
		    && name.first != "marginalnote" && name.first != "note") {
			if (isTargetAvailable(toqstr(name.first)))
				targetCO->addItem(toqstr(translateIfPossible(name.second)), toqstr(name.first));
		}
	}
	if (isTargetAvailable("equation"))
		targetCO->addItem(qt_("Equations"), "equation");
	// restore previous setting
	int const i = targetCO->findData(target);
	if (i != -1)
		targetCO->setCurrentIndex(i);
}


void GuiRef::updateContents()
{
	QString const orig_type =
		typeCO->itemData(typeCO->currentIndex()).toString();

	typeCO->clear();

	// FIXME Bring InsetMathRef on par with InsetRef
	// (see #11104)
	bool const have_cpageref =
			buffer().params().xref_package == "cleveref"
			|| buffer().params().xref_package == "zref";
	typeCO->addItem(qt_("<reference>"), "ref");
	typeCO->addItem(qt_("(<reference>)"), "eqref");
	typeCO->addItem(qt_("<page>"), "pageref");
	if (have_cpageref)
		typeCO->addItem(qt_("page <page>"), "cpageref");
	typeCO->addItem(qt_("on page <page>"), "vpageref");
	typeCO->addItem(qt_("<reference> on page <page>"), "vref");
	typeCO->addItem(qt_("Textual reference"), "nameref");
	typeCO->addItem(qt_("Formatted reference"), "formatted");
	typeCO->addItem(qt_("Label only"), "labelonly");

	// restore type settings for new insets
	bool const new_inset = params_["reference"].empty();
	if (new_inset) {
		int index = typeCO->findData(orig_type);
		if (index == -1)
			index = 0;
		typeCO->setCurrentIndex(index);
	} else if (params_.getCmdName() == "cpageref" && !have_cpageref)
		typeCO->setCurrentIndex(typeCO->findData("pageref"));
	else
		typeCO->setCurrentIndex(
			typeCO->findData(toqstr(params_.getCmdName())));
	typeCO->setEnabled(!isBufferReadonly());

	pluralCB->setChecked(params_["plural"] == "true");
	capsCB->setChecked(params_["caps"] == "true");
	noprefixCB->setChecked(params_["noprefix"] == "true");
	nolinkCB->setChecked(params_["nolink"] == "true");
	refOptionsLE->setText(toqstr(params_["options"]));
	if (!params_["tuple"].empty())
		rangeListCO->setCurrentIndex(rangeListCO->findData(toqstr(params_["tuple"])));

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

	updateTargets();

	updateRefs();
	enableBoxes();
	// Activate OK/Apply buttons if the users inserts a new ref
	// and we have a valid pre-setting.
	bc().setValid(isValid() && new_inset);
}


void GuiRef::applyView()
{
	QList<QTreeWidgetItem *> selRefs = selectedLV->findItems("*", Qt::MatchWildcard);
	vector<docstring> labels;
	for (int i = 0; i < selRefs.size(); ++i)
		labels.push_back(qstring_to_ucs4(selRefs.at(i)->data(0, Qt::UserRole).toString()));

	params_.setCmdName(fromqstr(typeCO->itemData(typeCO->currentIndex()).toString()));
	params_["reference"] = getStringFromVector(labels);
	params_["plural"] = pluralCB->isChecked() ?
	      from_ascii("true") : from_ascii("false");
	params_["caps"] = capsCB->isChecked() ?
	      from_ascii("true") : from_ascii("false");
	params_["noprefix"] = noprefixCB->isChecked() ?
	      from_ascii("true") : from_ascii("false");
	params_["nolink"] = nolinkCB->isChecked() ?
	      from_ascii("true") : from_ascii("false");
	params_["options"] = qstring_to_ucs4(refOptionsLE->text());
	params_["tuple"] = qstring_to_ucs4(rangeListCO->itemData(rangeListCO->currentIndex()).toString());
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
	if (at_ref_) {
		// go back
		setGotoRef();
		gotoBookmark();
	} else {
		// go to the ref
		setGoBack();
		goToRef(fromqstr(last_reference_));
		// restore the last selection
		if (!last_reference_.isEmpty()) {
			QTreeWidgetItemIterator it(selectedLV);
			while (*it) {
				if ((*it)->text(0) == last_reference_) {
					selectedLV->setCurrentItem(*it);
					(*it)->setSelected(true);
					//Make sure selected item is visible
					selectedLV->scrollToItem(*it);
					break;
				}
				++it;
			}
			last_reference_.clear();
		}
	}
	at_ref_ = !at_ref_;
}

inline bool caseInsensitiveLessThanVec(std::tuple<QString, QString, QString> const & s1,
				       std::tuple<QString, QString, QString> const & s2)
{
	return std::get<0>(s1).toLower() < std::get<0>(s2).toLower();
}

inline bool caseInsensitiveLessThan(QString const & s1, QString const & s2)
{
	return s1.toLower() < s2.toLower();
}


void GuiRef::updateAvailableLabels()
{
	// Prevent these widgets from emitting any signals whilst
	// we modify their state.
	refsTW->blockSignals(true);
	refsTW->setUpdatesEnabled(false);

	refsTW->clear();

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

	refsTW->setUpdatesEnabled(true);
	refsTW->update();
	updateButtons();

	// redo filter
	filterLabels();

	// Re-activate the emission of signals by these widgets.
	refsTW->blockSignals(false);

	bool const sel = selectedLV->currentItem()
			&& selectedLV->currentItem()->isSelected();

	gotoPB->setEnabled(sel);
	typeCO->setEnabled(sel);
	typeLA->setEnabled(sel);

	if (groupCB->isChecked())
		refsTW->setIndentation(10);
	else
		refsTW->setIndentation(0);
}


void GuiRef::getTargetChildren(QModelIndex & index, QAbstractItemModel * model,
			       QTreeWidgetItem * pitem, QString const & target)
{
	for (int r = 0; r != model->rowCount(index); ++r) {
		QModelIndex mi = model->index(r, 0, index);
		if (mi == index)
			continue;
		TocItem const & ti = view_->tocModels().currentItem(target, mi);
		docstring const id = (ti.parIDs().empty())
				? ti.dit().paragraphGotoArgument(true)
				: ti.parIDs();
		QString const ref = toqstr(id);
		QString const val = model->data(mi, Qt::DisplayRole).toString();
		QTreeWidgetItem * child = new QTreeWidgetItem(pitem);
		child->setText(0, val);
		child->setData(0, Qt::UserRole, ref);
		// recursive call to get grandchildren
		if (model->hasChildren(mi))
			getTargetChildren(mi, model, child, target);
		pitem->addChild(child);
	}
}


void GuiRef::updateAvailableTargets()
{
	// Prevent these widgets from emitting any signals whilst
	// we modify their state.
	refsTW->blockSignals(true);
	refsTW->setUpdatesEnabled(false);

	refsTW->clear();

	QString const target = targetCO->itemData(targetCO->currentIndex()).toString();
	QAbstractItemModel * toc_model = view_->tocModels().model(target);
	if (!toc_model)
		return;

	bool has_children = false;
	QList<QTreeWidgetItem *> refsItems;
	for (int r = 0; r < toc_model->rowCount(); ++r) {
		QModelIndex mi = toc_model->index(r, 0);
		QTreeWidgetItem * item = new QTreeWidgetItem(refsTW);
		TocItem const & ti = view_->tocModels().currentItem(target, mi);
		docstring const id = (ti.parIDs().empty())
				? ti.dit().paragraphGotoArgument(true)
				: ti.parIDs();
		QString const ref = toqstr(id);
		QString const val = toc_model->data(mi, Qt::DisplayRole).toString();
		item->setText(0, val);
		item->setData(0, Qt::UserRole, ref);
		if (toc_model->hasChildren(mi)) {
			getTargetChildren(mi, toc_model, item, target);
			has_children = true;
		}
		refsItems.append(item);
	}
	refsTW->addTopLevelItems(refsItems);

	refsTW->setUpdatesEnabled(true);
	refsTW->update();
	updateButtons();

	// redo filter
	filterLabels();

	// Re-activate the emission of signals by these widgets.
	refsTW->blockSignals(false);

	bool const sel = selectedLV->currentItem()
		&& selectedLV->currentItem()->isSelected();

	gotoPB->setEnabled(sel);
	typeCO->setEnabled(sel);
	typeLA->setEnabled(sel);

	if (has_children)
		refsTW->setIndentation(10);
	else
		refsTW->setIndentation(0);
}


bool GuiRef::isTargetAvailable(QString const & target)
{
	if (!view_->tocModels().hasModel(target))
		return false;

	QAbstractItemModel * toc_model = view_->tocModels().model(target);
	return toc_model && toc_model->rowCount() > 0;
}


void GuiRef::updateRefs()
{
	QString const target = targetCO->itemData(targetCO->currentIndex()).toString();
	bool const show_labels = target == "labels";
	if (show_labels) {
		refs_.clear();
		int const the_buffer = bufferCO->currentIndex();
		if (the_buffer != -1) {
			FileNameList const names(theBufferList().fileNames());
			FileName const & name = names[the_buffer];
			Buffer const * buf = theBufferList().getBuffer(name);
			buf->getLabelList(refs_);
		}
	}
	bool const enable_tw = (show_labels) ? !refs_.empty()
					     : isTargetAvailable(target);
	refsTW->setEnabled(enable_tw);
	sortingCO->setEnabled(show_labels && !refs_.empty());
	groupCB->setEnabled(show_labels && !refs_.empty());

	if (show_labels) {
		refsTW->header()->setVisible(true);
		availableLA->setText(qt_("Available &Labels:"));
		filter_->setPlaceholderText(qt_("All available labels"));
		filter_->setToolTip(qt_("Enter string to filter the list of available labels"));
		updateAvailableLabels();
	} else {
		refsTW->header()->setVisible(false);
		availableLA->setText(qt_("Available &Targets:"));
		filter_->setPlaceholderText(qt_("All available targets"));
		filter_->setToolTip(qt_("Enter string to filter the list of available targets"));
		updateAvailableTargets();
	}
}


bool GuiRef::isValid()
{
	QString const reftype =
		typeCO->itemData(typeCO->currentIndex()).toString();
	bool const threshold = (reftype == "vref" || reftype == "vpageref")
		&& selectedLV->topLevelItemCount() > 1;
	return selectedLV->currentItem() && !threshold;
}


void GuiRef::goToRef(string const & ref)
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
	bool const show_labels = targetCO->itemData(targetCO->currentIndex()).toString() == "labels";
	Qt::CaseSensitivity cs = csFindCB->isChecked() ?
		Qt::CaseSensitive : Qt::CaseInsensitive;
	QTreeWidgetItemIterator it(refsTW);
	if (show_labels) {
		while (*it) {
			(*it)->setHidden(
				(*it)->childCount() == 0
				&& !(*it)->text(0).contains(filter_->text(), cs)
				&& !(*it)->text(1).contains(filter_->text(), cs)
			);
			++it;
		}
	} else {
		QTreeWidgetItemIterator itt(refsTW);
		while (*it) {
			(*it)->setHidden(!(*it)->text(0).contains(filter_->text(), cs));
			itt = it;
			++it;
		}
		// recursively unhide parents of unhidden children
		while (*itt) {
			if (!(*itt)->isHidden() && (*itt)->parent())
				(*itt)->parent()->setHidden(false);
			--itt;
		}
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

	selectedLV->clear();
	vector<docstring> selRefs = getVectorFromString(params_["reference"]);
	QList<QTreeWidgetItem *> selRefsItems;
	for (auto const & sr : selRefs) {
		QTreeWidgetItem * item = new QTreeWidgetItem(selectedLV);
		item->setText(0, toqstr(sr));
		item->setData(0, Qt::UserRole, toqstr(sr));
		selRefsItems.append(item);
	}
	if (!selRefsItems.empty()) {
		selectedLV->addTopLevelItems(selRefsItems);
		selectedLV->setCurrentItem(selRefsItems.front());
	}

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
