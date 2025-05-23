/**
 * \file GuiThesaurus.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiThesaurus.h"
#include "GuiApplication.h"

#include "qt_helpers.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "FuncRequest.h"
#include "Language.h"
#include "lyxfind.h"
#include "WordLangTuple.h"

#include "support/debug.h"
#include "support/lstrings.h"

#include <QAbstractItemModel>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>


using namespace lyx::support;
using namespace std;

namespace lyx {
namespace frontend {

GuiThesaurus::GuiThesaurus(GuiView & lv)
	: GuiDialog(lv, "thesaurus", qt_("Thesaurus"))
{
	setupUi(this);

	meaningsTV->setColumnCount(1);
	meaningsTV->header()->hide();

	connect(buttonBox, SIGNAL(clicked(QAbstractButton *)),
		this, SLOT(slotButtonBox(QAbstractButton *)));
	connect(replaceED, SIGNAL(returnPressed()),
		this, SLOT(replaceClicked()));
	connect(replaceED, SIGNAL(textChanged(QString)),
		this, SLOT(change_adaptor()));
	connect(entryCO, SIGNAL(editTextChanged(const QString &)),
		this, SLOT(entryChanged()));
	connect(entryCO, SIGNAL(activated(int)),
		this, SLOT(entryChanged()));
	connect(lookupPB, SIGNAL(clicked()),
		this, SLOT(entryChanged()));
	connect(replacePB, SIGNAL(clicked()),
		this, SLOT(replaceClicked()));
	connect(languageCO, SIGNAL(activated(int)),
		this, SLOT(entryChanged()));
	connect(meaningsTV, SIGNAL(itemClicked(QTreeWidgetItem *, int)),
		this, SLOT(itemClicked(QTreeWidgetItem *, int)));
	connect(meaningsTV, SIGNAL(itemSelectionChanged()),
		this, SLOT(selectionChanged()));
	connect(meaningsTV, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
		this, SLOT(selectionClicked(QTreeWidgetItem *, int)));

	// language
	QAbstractItemModel * language_model = guiApp->languageModel();
	// FIXME: it would be nice if sorting was enabled/disabled via a checkbox.
	language_model->sort(0);
	languageCO->setModel(language_model);
	languageCO->setModelColumn(2);

	//bug #8138
	if (entryCO->completer())
		entryCO->completer()->setCompletionMode(QCompleter::PopupCompletion);

	bc().setCancel(buttonBox->button(QDialogButtonBox::Close));
	bc().setApply(replacePB, true);
	bc().setPolicy(ButtonPolicy::OkApplyCancelReadOnlyPolicy);

	setFocusProxy(entryCO);
}

void GuiThesaurus::checkStatus()
{
	if (!isBufferAvailable()) {
		// deactivate the thesaurus if we have no buffer
		enableView(false);
		return;
	}
	updateView();
}

void GuiThesaurus::change_adaptor()
{
	changed();
}


void GuiThesaurus::entryChanged()
{
	updateLists();
}


void GuiThesaurus::selectionChanged()
{
	int const col = meaningsTV->currentColumn();
	if (col < 0 || isBufferReadonly())
		return;

	QString item = meaningsTV->currentItem()->text(col);
	// cut out the classification in brackets:
	// "hominid (generic term)" -> "hominid"
	QRegularExpression re("^([^\\(\\)]+)\\b\\(?.*\\)?.*$");
	// This is for items with classifications at the beginning:
	// "(noun) man" -> "man"; "(noun) male (generic term)" -> "male"
	QRegularExpression rex("^(\\(.+\\))\\s*([^\\(\\)]+)\\s*\\(?.*\\)?.*$");
	QRegularExpressionMatch match = re.match(item);
	if (match.hasMatch())
		item = match.captured(1).trimmed();
	match = rex.match(item);
	if (match.hasMatch())
		item = match.captured(2).trimmed();
	replaceED->setText(item);
	replacePB->setEnabled(!isBufferReadonly());
	changed();
}


void GuiThesaurus::itemClicked(QTreeWidgetItem * /*item*/, int /*col*/)
{
	selectionChanged();
}


void GuiThesaurus::selectionClicked(QTreeWidgetItem * item, int col)
{
	QString str = item->text(col);
	// cut out the classification in brackets:
	// "hominid (generic term)" -> "hominid"
	QRegularExpression re("^([^\\(\\)]+)\\b\\(?.*\\)?.*$");
	// This is for items with classifications at the beginning:
	// "(noun) man" -> "man"; "(noun) male (generic term)" -> "male"
	QRegularExpression rex("^(\\(.+\\))\\s*([^\\(\\)]+)\\s*\\(?.*\\)?.*$");
	QRegularExpressionMatch match = re.match(str);
	if (match.hasMatch())
		str = match.captured(1).trimmed();
	match = rex.match(str);
	if (match.hasMatch())
		str = match.captured(2).trimmed();
	entryCO->insertItem(0, str);
	entryCO->setCurrentIndex(0);

	selectionChanged();
	updateLists();
}


void GuiThesaurus::updateLists()
{
	meaningsTV->clear();

	if (entryCO->currentText().isEmpty())
		return;

	meaningsTV->setUpdatesEnabled(false);

	QString const lang = languageCO->itemData(
		languageCO->currentIndex()).toString();
	Language * language = const_cast<Language*>(lyx::languages.getLanguage(fromqstr(lang)));
	docstring const lang_code = from_ascii(language->code());

	Thesaurus::Meanings meanings =
		getMeanings(WordLangTuple(qstring_to_ucs4(entryCO->currentText()), language));

	for (auto const & meaning_p : meanings) {
		QTreeWidgetItem * i = new QTreeWidgetItem(meaningsTV);
		i->setText(0, toqstr(meaning_p.first));
		meaningsTV->expandItem(i);
		for (docstring const & word : meaning_p.second) {
			QTreeWidgetItem * i2 = new QTreeWidgetItem(i);
			i2->setText(0, toqstr(word));
		}
	}

	meaningsTV->setEnabled(!meanings.empty());
	lookupPB->setEnabled(!meanings.empty());
	selectionLA->setEnabled(!meanings.empty() && !isBufferReadonly());
	replaceED->setEnabled(!meanings.empty() && !isBufferReadonly());
	replacePB->setEnabled(!meanings.empty() && !isBufferReadonly());

	if (meanings.empty() && !thesaurus.thesaurusAvailable(lang_code)) {
		QTreeWidgetItem * i = new QTreeWidgetItem(meaningsTV);
		i->setText(0, qt_("No thesaurus available for this language!"));
	}

	meaningsTV->setUpdatesEnabled(true);
	meaningsTV->update();
}


void GuiThesaurus::updateContents()
{
	entryCO->clear();
	entryCO->addItem(toqstr(text_));
	entryCO->setCurrentIndex(0);
	replaceED->setText("");
	int const pos = languageCO->findData(toqstr(lang_));
	if (pos != -1)
		languageCO->setCurrentIndex(pos);
	updateLists();
}


void GuiThesaurus::replaceClicked()
{
	replace(qstring_to_ucs4(replaceED->text()));
}


bool GuiThesaurus::initialiseParams(string const & sdata)
{
	string arg;
	string const lang = rsplit(sdata, arg, ' ');
	if (prefixIs(lang, "lang=")) {
		lang_ = from_utf8(split(lang, '='));
		text_ = from_utf8(arg);
	} else {
		text_ = from_utf8(sdata);
		if (bufferview())
			lang_ = from_ascii(
				bufferview()->buffer().params().language->lang());
	}
	return true;
}


void GuiThesaurus::clearParams()
{
	text_.erase();
	lang_.erase();
}


void GuiThesaurus::replace(docstring const & newstr)
{
	/* FIXME: this is not suitable ! We need to have a "lock"
	 * on a particular charpos in a paragraph that is broken on
	 * deletion/change !
	 */
	docstring const sdata =
		replace2string(newstr, text_,
				     true,  // case sensitive
				     true,  // match word
				     false, // all words
				     true,  // forward
				     false,  // find next
				     false,  // auto-wrap
				     false); // only selection
	dispatch(FuncRequest(LFUN_WORD_REPLACE, sdata));
}


Thesaurus::Meanings const & GuiThesaurus::getMeanings(WordLangTuple const & wl)
{
	if (wl.word() != laststr_)
		meanings_ = thesaurus.lookup(wl);
	return meanings_;
}


} // namespace frontend
} // namespace lyx


#include "moc_GuiThesaurus.cpp"
