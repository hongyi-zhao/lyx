/**
 * \file GuiSpellchecker.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Edwin Leuven
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiSpellchecker.h"
#include "GuiApplication.h"

#include "qt_helpers.h"

#include "ui_SpellcheckerUi.h"

#include "Buffer.h"
#include "BufferView.h"
#include "Cursor.h"
#include "Text.h"
#include "FuncRequest.h"
#include "GuiView.h"
#include "Language.h"
#include "LyX.h"
#include "lyxfind.h"
#include "Paragraph.h"
#include "WordLangTuple.h"

#include "support/debug.h"
#include "support/docstring.h"
#include "support/docstring_list.h"
#include "support/ExceptionMessage.h"

#include <QKeyEvent>
#include <QListWidgetItem>
#include <QMessageBox>

#include "SpellChecker.h"

#include "frontends/alert.h"

using namespace std;
using namespace lyx::support;

namespace lyx {
namespace frontend {


struct SpellcheckerWidget::Private
{
	Private(SpellcheckerWidget * parent, DockView * dv, GuiView * gv)
		: p(parent), dv_(dv), gv_(gv), incheck_(false), wrap_around_(false) {}
	///
	void updateView();
	/// update from controller
	void updateSuggestions(docstring_list & words);
	/// move to next position after current word
	void forward();
	/// check text until next misspelled/unknown word
	void check();
	/// close the spell checker dialog
	void hide() const;
	/// if no selection was checked:
	/// ask the user if the check should start over
	bool continueFromBeginning();
	/// set the given language in language chooser
	void setLanguage(Language const * lang);
	/// test and set guard flag
	bool inCheck() {
		if (incheck_)
			return true;
		incheck_ = true;
		return false;
	}
	void canCheck() { incheck_ = false; }
	/// set if checking already started from the beginning
	void wrapAround(bool flag) {
		wrap_around_ = flag;
		if (flag) {
			end_ = start_;
		}
	}
	/// test for existing association with a document buffer
	/// and test for already active check
	bool disabled() {
		return gv_->documentBufferView() == nullptr || inCheck();
	}
	/// the cursor position of the buffer view
	DocIterator const cursor() const;
	/// status checks
	bool isCurrentBuffer(DocIterator const & cursor) const;
	/// return true if we ended a complete cycle
	bool isWrapAround(DocIterator const & cursor) const;
	/// returns true if we did already start from the beginning
	bool isWrapAround() const { return wrap_around_; }
	bool atLastPos(DocIterator const & cursor) const;
	/// validate the cached doc iterators
	/// The spell checker dialog is not modal.
	/// The user may change the buffer being checked and break the iterators.
	void fixPositionsIfBroken();
	///
	Ui::SpellcheckerUi ui;
	///
	SpellcheckerWidget * p;
	///
	DockView * dv_;
	///
	GuiView * gv_;
	/// current word being checked and lang code
	WordLangTuple word_;
	/// cursor position where spell checking starts
	DocIterator start_;
	/// range to spell check
	/// for selection both are non-empty
	/// after wrap around the start position becomes the end
	DocIterator begin_;
	DocIterator end_;
	///
	bool incheck_;
	/// Did we already start from the beginning?
	bool wrap_around_;
};


SpellcheckerWidget::SpellcheckerWidget(GuiView * gv, DockView * dv, QWidget * parent)
	: QTabWidget(parent), d(new Private(this, dv, gv))
{
	d->ui.setupUi(this);

	connect(d->ui.suggestionsLW, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
		this, SLOT(on_replacePB_clicked()));

	// language
	QAbstractItemModel * language_model = guiApp->languageModel();
	// FIXME: it would be nice if sorting was enabled/disabled via a checkbox.
	language_model->sort(0);
	d->ui.languageCO->setModel(language_model);
	d->ui.languageCO->setModelColumn(1);
	d->ui.suggestionsLW->installEventFilter(this);
}


SpellcheckerWidget::~SpellcheckerWidget()
{
	delete d;
}


bool SpellcheckerWidget::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == d->ui.suggestionsLW && event->type() == QEvent::KeyPress) {
		QKeyEvent *e = static_cast<QKeyEvent *> (event);
		if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
			if (d->ui.suggestionsLW->currentItem()) {
				on_suggestionsLW_itemClicked(d->ui.suggestionsLW->currentItem());
				on_replacePB_clicked();
			}
			return true;
		} else if (e->key() == Qt::Key_Right) {
			if (d->ui.suggestionsLW->currentItem())
				on_suggestionsLW_itemClicked(d->ui.suggestionsLW->currentItem());
			return true;
		}
	}
	// standard event processing
	return QWidget::eventFilter(obj, event);
}


void SpellcheckerWidget::on_suggestionsLW_itemClicked(QListWidgetItem * item)
{
	if (d->ui.replaceCO->count() != 0)
		d->ui.replaceCO->setItemText(0, item->text());
	else
		d->ui.replaceCO->addItem(item->text());

	d->ui.replaceCO->setCurrentIndex(0);
}


void SpellcheckerWidget::on_replaceCO_highlighted(int index)
{
	QString const str = d->ui.replaceCO->itemText(index);
	QListWidget * lw = d->ui.suggestionsLW;
	if (lw->currentItem() && lw->currentItem()->text() == str)
		return;

	for (int i = 0; i != lw->count(); ++i) {
		if (lw->item(i)->text() == str) {
			lw->setCurrentRow(i);
			break;
		}
	}
}


void SpellcheckerWidget::updateView()
{
	d->updateView();
}


void SpellcheckerWidget::Private::updateView()
{
	BufferView * bv = gv_->documentBufferView();
	bool const enabled = bv != nullptr;
	// Check cursor position
	if (enabled && p->hasFocus()) {
		Cursor const & cur = bv->cursor();
		if (start_.empty() || !isCurrentBuffer(cur)) {
			if (cur.selection()) {
				begin_ = cur.selectionBegin();
				end_   = cur.selectionEnd();
				start_ = begin_;
				bv->cursor().setCursor(start_);
			} else {
				begin_ = DocIterator();
				end_   = DocIterator();
				start_ = cur;
			}
			wrapAround(false);
			check();
		}
	}

	// Enable widgets as needed.
	bool const has_word = enabled && !ui.wordED->text().isEmpty();
	bool const can_replace = has_word && !bv->buffer().isReadonly();
	ui.TextLabel3->setEnabled(enabled);
	ui.wordED->setEnabled(enabled);
	ui.ignorePB->setEnabled(has_word);
	ui.ignoreAllPB->setEnabled(has_word);
	ui.addPB->setEnabled(has_word);
	ui.TextLabel1->setEnabled(can_replace);
	ui.replaceCO->setEnabled(can_replace);
	ui.TextLabel2->setEnabled(has_word);
	ui.suggestionsLW->setEnabled(has_word);
	ui.replacePB->setEnabled(can_replace);
	ui.replaceAllPB->setEnabled(can_replace);
}


DocIterator const SpellcheckerWidget::Private::cursor() const
{
	BufferView * bv = gv_->documentBufferView();
	return bv ? bv->cursor() : DocIterator();
}


bool SpellcheckerWidget::Private::continueFromBeginning()
{
	DocIterator const current_ = cursor();
	if (isCurrentBuffer(current_) && !begin_.empty()) {
		// selection was checked
		// start over from beginning makes no sense
		fixPositionsIfBroken();
		hide();
		if (current_ == start_) {
			// no errors found... tell the user the good news
			// so there is some feedback
			QMessageBox::information(p,
				qt_("Spell Checker"),
				qt_("Spell check of the selection done, "
					"did not find any errors."));
		}
		return false;
	}
	QMessageBox::StandardButton const answer = QMessageBox::question(p,
		qt_("Spell Checker"),
		qt_("We reached the end of the document, would you like to "
			"continue from the beginning?"),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (answer == QMessageBox::No) {
		fixPositionsIfBroken();
		hide();
		return false;
	}
	// there is no selection, start over from the beginning now
	wrapAround(true);
	dispatch(FuncRequest(LFUN_BUFFER_BEGIN));
	return true;
}


bool SpellcheckerWidget::Private::isCurrentBuffer(DocIterator const & cursor) const
{
	return start_.buffer() == cursor.buffer();
}


bool SpellcheckerWidget::Private::atLastPos(DocIterator const & cursor) const
{
	bool const valid_end = !end_.empty();
	return cursor.depth() <= 1 && (
		cursor.atEnd() ||
		(valid_end && isCurrentBuffer(cursor) && cursor >= end_));
}


bool SpellcheckerWidget::Private::isWrapAround(DocIterator const & cursor) const
{
	return wrap_around_ && isCurrentBuffer(cursor) && start_ < cursor;
}


void SpellcheckerWidget::Private::fixPositionsIfBroken()
{
	if (!isCurrentBuffer(cursor())) {
		LYXERR(Debug::GUI, "wrong document of current cursor position " << start_);
		start_ = cursor();
		begin_ = DocIterator();
		end_   = DocIterator();
	}
	if (start_.fixIfBroken())
		LYXERR(Debug::GUI, "broken start position fixed " << start_);
	if (begin_.fixIfBroken()) {
		LYXERR(Debug::GUI, "broken selection begin position fixed " << begin_);
		begin_ = DocIterator();
		end_   = DocIterator();
	}
	if (end_.fixIfBroken())
		LYXERR(Debug::GUI, "broken selection end position fixed " << end_);
}


void SpellcheckerWidget::Private::hide() const
{
	BufferView * bv = gv_->documentBufferView();
	Cursor & bvcur = bv->cursor();
	dv_->hide();
	if (isCurrentBuffer(bvcur)) {
		if (!begin_.empty() && !end_.empty()) {
			// restore previous selection
			bv->setSelection(begin_, end_);
		} else {
			// restore cursor position
			bvcur.setCursor(start_);
			bvcur.clearSelection();
			bv->processUpdateFlags(Update::Force | Update::FitCursor);
		}
	}
}


void SpellcheckerWidget::Private::forward()
{
	DocIterator const from = cursor();

	dispatch(FuncRequest(LFUN_ESCAPE));
	fixPositionsIfBroken();
	if (!atLastPos(cursor())) {
		dispatch(FuncRequest(LFUN_CHAR_FORWARD));
	}
	if (atLastPos(cursor())) {
		return;
	}
	if (from == cursor()) {
		//FIXME we must be at the end of a cell
		dispatch(FuncRequest(LFUN_CHAR_FORWARD));
 	}
	if (isWrapAround(cursor())) {
		hide();
	}
	// If we reached the end of the document
	// and have not yet wrapped (which was
	// checked above), continue from beginning
	if (cursor().pit() == cursor().lastpit()
	    && cursor().pos() == cursor().lastpos()
	    && cursor().text()->isMainText())
		continueFromBeginning();
}


void SpellcheckerWidget::on_languageCO_activated(int index)
{
	string const lang =
		fromqstr(d->ui.languageCO->itemData(index).toString());
	if (!d->word_.lang() || d->word_.lang()->lang() == lang)
		// nothing changed
		return;
	dispatch(FuncRequest(LFUN_LANGUAGE, lang));
	d->check();
}


bool SpellcheckerWidget::initialiseParams(std::string const &)
{
	BufferView * bv = d->gv_->documentBufferView();
	if (bv == nullptr)
		return false;
	std::set<Language const *> langs =
		bv->buffer().masterBuffer()->getLanguages();
	if (!langs.empty())
		d->setLanguage(*langs.begin());
	d->start_ = DocIterator();
	d->wrapAround(false);
	d->canCheck();
	return true;
}


void SpellcheckerWidget::on_skipAllPB_clicked()
{
	/// ignore all occurrences of word
	if (d->disabled())
		return;
	LYXERR(Debug::GUI, "Spellchecker: skip all button");
	if (d->word_.lang() && !d->word_.word().empty())
		theSpellChecker()->accept(d->word_);
	d->forward();
	d->check();
	d->canCheck();
}


void SpellcheckerWidget::on_ignoreAllPB_clicked()
{
	/// ignore all occurrences of word
	if (d->disabled())
		return;
	LYXERR(Debug::GUI, "Spellchecker: ignore all button");
	if (d->word_.lang() && !d->word_.word().empty())
		dispatch(FuncRequest(LFUN_SPELLING_ADD_LOCAL,
				     d->word_.word() + " " + from_ascii(d->word_.lang()->lang())));
	d->forward();
	d->check();
	d->canCheck();
}


void SpellcheckerWidget::on_addPB_clicked()
{
	/// insert word in personal dictionary
	if (d->disabled())
		return;
	LYXERR(Debug::GUI, "Spellchecker: add word button");
	theSpellChecker()->insert(d->word_);
	d->forward();
	d->check();
	d->canCheck();
}


void SpellcheckerWidget::on_skipPB_clicked()
{
	/// ignore this occurrence of word
	if (d->disabled())
		return;
	LYXERR(Debug::GUI, "Spellchecker: skip button");
	d->forward();
	d->check();
	d->canCheck();
}


void SpellcheckerWidget::on_ignorePB_clicked()
{
	/// ignore this occurrence of word
	if (d->disabled())
		return;
	LYXERR(Debug::GUI, "Spellchecker: ignore button");
	dispatch(FuncRequest(LFUN_FONT_NO_SPELLCHECK));
	d->forward();
	d->check();
	d->canCheck();
}


void SpellcheckerWidget::on_replacePB_clicked()
{
	if (d->disabled())
		return;
	docstring const textfield = qstring_to_ucs4(d->ui.wordED->text());
	docstring const replacement = qstring_to_ucs4(d->ui.replaceCO->currentText());
	docstring const datastring =
		replace2string(replacement, textfield,
			true,   // case sensitive
			true,   // match word
			false,  // all words
			true,   // forward
			false,  // find next
			false,  // auto-wrap
			false); // only selection

	LYXERR(Debug::GUI, "Replace (" << replacement << ")");
	dispatch(FuncRequest(LFUN_WORD_REPLACE, datastring));
	d->forward();
	d->check();
	d->canCheck();
}


void SpellcheckerWidget::on_replaceAllPB_clicked()
{
	if (d->disabled())
		return;
	docstring const textfield = qstring_to_ucs4(d->ui.wordED->text());
	docstring const replacement = qstring_to_ucs4(d->ui.replaceCO->currentText());
	docstring const datastring =
		replace2string(replacement, textfield,
			true,   // case sensitive
			true,   // match word
			true,   // all words
			true,   // forward
			false,  // find next
			false,  // auto-wrap
			false); // only selection

	LYXERR(Debug::GUI, "Replace all (" << replacement << ")");
	dispatch(FuncRequest(LFUN_WORD_REPLACE, datastring));
	d->forward();
	d->check(); // continue spellchecking
	d->canCheck();
}


void SpellcheckerWidget::Private::updateSuggestions(docstring_list & words)
{
	QString const suggestion = toqstr(word_.word());
	ui.wordED->setText(suggestion);
	QListWidget * lw = ui.suggestionsLW;
	lw->clear();

	if (words.empty()) {
		p->on_suggestionsLW_itemClicked(new QListWidgetItem(suggestion));
		return;
	}
	for (size_t i = 0; i != words.size(); ++i)
		lw->addItem(toqstr(words[i]));

	p->on_suggestionsLW_itemClicked(lw->item(0));
	lw->setCurrentRow(0);
}


void SpellcheckerWidget::Private::setLanguage(Language const * lang)
{
	int const pos = ui.languageCO->findData(toqstr(lang->lang()));
	if (pos != -1)
		ui.languageCO->setCurrentIndex(pos);
}


void SpellcheckerWidget::Private::check()
{
	BufferView * bv = gv_->documentBufferView();
	if (!bv || bv->buffer().text().empty())
		return;

	fixPositionsIfBroken();

	SpellChecker * speller = theSpellChecker();
	if (speller && !speller->hasDictionary(bv->buffer().language())) {
		int dsize = speller->numDictionaries();
		if (0 == dsize) {
			hide();
			QMessageBox::information(p,
				qt_("Spell Checker"),
				qt_("Spell checker has no dictionaries."));
			return;
		}
	}

	DocIterator from = bv->cursor();
	DocIterator to = isCurrentBuffer(from) ? end_ : doc_iterator_end(&bv->buffer());
	WordLangTuple word_lang;
	docstring_list suggestions;

	LYXERR(Debug::GUI, "Spellchecker: start check at " << from);
	try {
		bv->buffer().spellCheck(from, to, word_lang, suggestions);
	} catch (ExceptionMessage const & message) {
		if (message.type_ == WarningException) {
			Alert::warning(message.title_, message.details_);
			return;
		}
		throw;
	}

	// end of document or selection?
	if (atLastPos(from)) {
		if (isWrapAround()) {
			hide();
			return;
		}
		if (continueFromBeginning())
			check();
		return;
	}

	if (isWrapAround(from)) {
		hide();
		return;
	}

	word_ = word_lang;

	// set suggestions
	updateSuggestions(suggestions);
	// set language
	if (!word_lang.lang())
		return;
	setLanguage(word_lang.lang());
	// mark misspelled word
	bv->setSelection(from, to);
	// enable relevant widgets
	updateView();
}


GuiSpellchecker::GuiSpellchecker(GuiView & parent,
		Qt::DockWidgetArea area, Qt::WindowFlags flags)
	: DockView(parent, "spellchecker", qt_("Spellchecker"),
		   area, flags)
{
	widget_ = new SpellcheckerWidget(&parent, this);
	setWidget(widget_);
	setFocusProxy(widget_);
#ifdef Q_OS_MAC
	// On Mac show and floating
	setFloating(true);
#endif
}


GuiSpellchecker::~GuiSpellchecker()
{
	setFocusProxy(nullptr);
	delete widget_;
}


void GuiSpellchecker::updateView()
{
	widget_->updateView();
}


} // namespace frontend
} // namespace lyx

#include "moc_GuiSpellchecker.cpp"
