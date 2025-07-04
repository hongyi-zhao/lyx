/**
 * \file FindAndReplace.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Tommaso Cucinotta
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "FindAndReplace.h"

#include "GuiApplication.h"
#include "GuiView.h"

#include "Buffer.h"
#include "BufferList.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Cursor.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "KeyMap.h"
#include "KeySequence.h"
#include "Language.h"
#include "LyX.h"
#include "lyxfind.h"
#include "Text.h"

#include "frontends/alert.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/filetools.h"
#include "support/FileName.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/lstrings.h"

#include <QCloseEvent>

using namespace std;
using namespace lyx::support;

namespace lyx {
namespace frontend {


FindAndReplaceWidget::FindAndReplaceWidget(GuiView & view)
	: QTabWidget(&view), view_(view)
{
	setupUi(this);
	find_work_area_->setGuiView(view_);
	find_work_area_->init();
	find_work_area_->setFrameStyle(QFrame::StyledPanel);

	setFocusProxy(find_work_area_);
	replace_work_area_->setGuiView(view_);
	replace_work_area_->init();
	replace_work_area_->setFrameStyle(QFrame::StyledPanel);

	// We don't want two cursors blinking.
	find_work_area_->stopBlinkingCaret();
	replace_work_area_->stopBlinkingCaret();
	old_buffer_ = view_.documentBufferView() ?
	    &(view_.documentBufferView()->buffer()) : 0;

	// align items on top
	cbVerticalLayout->setAlignment(Qt::AlignTop);
	pbVerticalLayout->setAlignment(Qt::AlignTop);
}


void FindAndReplaceWidget::dockLocationChanged(Qt::DockWidgetArea area)
{
	if (area == Qt::RightDockWidgetArea || area == Qt::LeftDockWidgetArea) {
		dynamicLayoutBasic_->setDirection(QBoxLayout::TopToBottom);
		dynamicLayoutAdvanced_->setDirection(QBoxLayout::TopToBottom);
	} else {
		dynamicLayoutBasic_->setDirection(QBoxLayout::LeftToRight);
		dynamicLayoutAdvanced_->setDirection(QBoxLayout::LeftToRight);
	}
}


bool FindAndReplaceWidget::eventFilter(QObject * obj, QEvent * event)
{
	updateButtons();
	if (event->type() != QEvent::KeyPress
		  || (obj != find_work_area_ && obj != replace_work_area_))
		return QWidget::eventFilter(obj, event);

	QKeyEvent * e = static_cast<QKeyEvent *> (event);
	switch (e->key()) {
	case Qt::Key_Escape:
		if (e->modifiers() == Qt::NoModifier) {
			hideDialog();
			return true;
		}
		break;

	case Qt::Key_Enter:
	case Qt::Key_Return: {
		bool const searchback = (e->modifiers() == Qt::ShiftModifier);
		bool const replace = (obj == replace_work_area_);
		findAndReplace(searchback, replace);
		if (replace)
			replace_work_area_->setFocus();
		else
			find_work_area_->setFocus();
		return true;
	}

	case Qt::Key_Tab:
		if (e->modifiers() == Qt::NoModifier && obj == find_work_area_){
			KeySequence seq;
			seq.parse("Tab");
			FuncRequest func = theTopLevelKeymap().getBinding(seq);
			if (!getStatus(func).enabled()) {
				LYXERR(Debug::FINDVERBOSE, "Focusing replace WA");
				replace_work_area_->setFocus(Qt::TabFocusReason);
				LYXERR(Debug::FINDVERBOSE, "Selecting entire replace buffer");
				dispatch(FuncRequest(LFUN_BUFFER_BEGIN));
				dispatch(FuncRequest(LFUN_BUFFER_END_SELECT));
				return true;
			}
		}
		break;

	case Qt::Key_Backtab:
		if (obj == replace_work_area_) {
			KeySequence seq;
			seq.parse("~S-BackTab");
			FuncRequest func = theTopLevelKeymap().getBinding(seq);
			if (!getStatus(func).enabled()) {
				LYXERR(Debug::FINDVERBOSE, "Focusing find WA");
				find_work_area_->setFocus(Qt::BacktabFocusReason);
				LYXERR(Debug::FINDVERBOSE, "Selecting entire find buffer");
				dispatch(FuncRequest(LFUN_BUFFER_BEGIN));
				dispatch(FuncRequest(LFUN_BUFFER_END_SELECT));
				return true;
			}
		}
		break;

	default:
		break;
	}
	// standard event processing
	return QWidget::eventFilter(obj, event);
}


static vector<string> const & allManualsFiles()
{
	static const char * files[] = {
		"Intro", "UserGuide", "Tutorial", "Additional",
		"EmbeddedObjects", "Math", "Customization", "Shortcuts",
		"LFUNs", "LaTeXConfig"
	};

	static vector<string> v;
	if (v.empty()) {
		FileName fname;
		for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
			fname = i18nLibFileSearch("doc", files[i], "lyx");
			v.push_back(fname.absFileName());
		}
	}

	return v;
}


/** Switch buf to point to next document buffer.
 **
 ** Return true if restarted from master-document buffer.
 **/
static bool nextDocumentBuffer(Buffer * & buf)
{
	ListOfBuffers const children = buf->allRelatives();
	LYXERR(Debug::FINDVERBOSE, "children.size()=" << children.size());
	ListOfBuffers::const_iterator it =
		find(children.begin(), children.end(), buf);
	LASSERT(it != children.end(), return false);
	++it;
	if (it == children.end()) {
		buf = *children.begin();
		return true;
	}
	buf = *it;
	return false;
}


/** Switch p_buf to point to previous document buffer.
 **
 ** Return true if restarted from last child buffer.
 **/
static bool prevDocumentBuffer(Buffer * & buf)
{
	ListOfBuffers const children = buf->allRelatives();
	LYXERR(Debug::FINDVERBOSE, "children.size()=" << children.size());
	ListOfBuffers::const_iterator it =
		find(children.begin(), children.end(), buf);
	LASSERT(it != children.end(), return false)
	if (it == children.begin()) {
		it = children.end();
		--it;
		buf = *it;
		return true;
	}
	--it;
	buf = *it;
	return false;
}


/** Switch buf to point to next or previous buffer in search scope.
 **
 ** Return true if restarted from scratch.
 **/
static bool nextPrevBuffer(Buffer * & buf,
			     FindAndReplaceOptions const & opt)
{
	bool restarted = false;
	switch (opt.scope) {
	case FindAndReplaceOptions::S_BUFFER:
		restarted = true;
		break;
	case FindAndReplaceOptions::S_DOCUMENT:
		if (opt.forward)
			restarted = nextDocumentBuffer(buf);
		else
			restarted = prevDocumentBuffer(buf);
		break;
	case FindAndReplaceOptions::S_OPEN_BUFFERS:
		if (opt.forward) {
			buf = theBufferList().next(buf);
			restarted = (buf == *theBufferList().begin());
		} else {
			buf = theBufferList().previous(buf);
			restarted = (buf == *(theBufferList().end() - 1));
		}
		break;
	case FindAndReplaceOptions::S_ALL_MANUALS:
		vector<string> const & manuals = allManualsFiles();
		vector<string>::const_iterator it =
			find(manuals.begin(), manuals.end(), buf->absFileName());
		if (it == manuals.end())
			it = manuals.begin();
		else if (opt.forward) {
			++it;
			if (it == manuals.end()) {
				it = manuals.begin();
				restarted = true;
			}
		} else {
			if (it == manuals.begin()) {
				it = manuals.end();
				restarted = true;
			}
			--it;
		}
		FileName const & fname = FileName(*it);
		if (!theBufferList().exists(fname)) {
			guiApp->currentView()->setBusy(false);
			guiApp->currentView()->loadDocument(fname, false);
			guiApp->currentView()->setBusy(true);
		}
		buf = theBufferList().getBuffer(fname);
		break;
	}
	return restarted;
}


/** Find the finest question message to post to the user */
docstring getQuestionString(FindAndReplaceOptions const & opt)
{
	docstring scope;
	switch (opt.scope) {
	case FindAndReplaceOptions::S_BUFFER:
		scope = _("File");
		break;
	case FindAndReplaceOptions::S_DOCUMENT:
		scope = _("Master document");
		break;
	case FindAndReplaceOptions::S_OPEN_BUFFERS:
		scope = _("Open files");
		break;
	case FindAndReplaceOptions::S_ALL_MANUALS:
		scope = _("Manuals");
		break;
	}
	docstring message = opt.forward ?
		bformat(_("The end was reached while searching forward.\n"
			  "Continue searching from the beginning? (Scope:%1$s)"),
			scope) :
		bformat(_("The beginning was reached while searching backward.\n"
			  "Continue searching from the end? (Scope:%1$s)"),
			scope);

	return message;
}


/// Return true if a match was found
bool FindAndReplaceWidget::findAndReplaceScope(FindAndReplaceOptions & opt, bool replace_all)
{
	BufferView * bv = view_.documentBufferView();
	if (!bv)
		return false;
	Buffer * buf = &bv->buffer();
	Buffer * buf_orig = &bv->buffer();
	DocIterator cur_orig(bv->cursor());
	int wrap_answer = -1;
	opt.replace_all = replace_all;
	ostringstream oss;
	oss << opt;
	FuncRequest cmd(LFUN_WORD_FINDADV, from_utf8(oss.str()));

	view_.message(_("Advanced search in progress (press ESC to cancel) . . ."));
	theApp()->startLongOperation();
	view_.setBusy(true);
	if (opt.scope == FindAndReplaceOptions::S_ALL_MANUALS) {
		vector<string> const & v = allManualsFiles();
		if (std::find(v.begin(), v.end(), buf->absFileName()) == v.end()) {
			FileName const & fname = FileName(*v.begin());
			if (!theBufferList().exists(fname)) {
				guiApp->currentView()->setBusy(false);
				theApp()->stopLongOperation();
				guiApp->currentView()->loadDocument(fname, false);
				theApp()->startLongOperation();
				guiApp->currentView()->setBusy(true);
			}
			buf = theBufferList().getBuffer(fname);
			if (!buf) {
				view_.setBusy(false);
				return false;
			}

			lyx::dispatch(FuncRequest(LFUN_BUFFER_SWITCH,
						  buf->absFileName()));
			bv = view_.documentBufferView();
			bv->cursor().clear();
			bv->cursor().push_back(CursorSlice(buf->inset()));
		}
	}

	UndoGroupHelper helper(buf);

	do {
		LYXERR(Debug::FINDVERBOSE, "Dispatching LFUN_WORD_FINDADV");
		dispatch(cmd);
		LYXERR(Debug::FINDVERBOSE, "dispatched");
		if (bv->cursor().result().dispatched()) {
			// New match found and selected (old selection replaced if needed)
			if (replace_all)
				continue;
			view_.setBusy(false);
			theApp()->stopLongOperation();
			return true;
		} else if (replace_all)
			bv->clearSelection();

		if (theApp()->longOperationCancelled()) {
			// Search aborted by user
			view_.message(_("Advanced search cancelled by user"));
			view_.setBusy(false);
			theApp()->stopLongOperation();
			return false;
		}

		// No match found in current buffer (however old selection might have been replaced)
		// select next buffer in scope, if any
		bool const prompt = nextPrevBuffer(buf, opt);
		if (!buf)
			break;
		if (prompt) {
			if (wrap_answer != -1)
				break;
			docstring q = getQuestionString(opt);
			view_.setBusy(false);
			theApp()->stopLongOperation();
			wrap_answer = frontend::Alert::prompt(
				_("Wrap search?"), q,
				0, 1, _("&Yes"), _("&No"));
			theApp()->startLongOperation();
			view_.setBusy(true);
			if (wrap_answer == 1)
				break;
		}
		if (buf != &view_.documentBufferView()->buffer())
			lyx::dispatch(FuncRequest(LFUN_BUFFER_SWITCH,
						  buf->absFileName()));

		helper.resetBuffer(buf);

		bv = view_.documentBufferView();
		if (opt.forward) {
			bv->cursor().clear();
			bv->cursor().push_back(CursorSlice(buf->inset()));
		} else {
			//lyx::dispatch(FuncRequest(LFUN_BUFFER_END));
			bv->cursor().setCursor(doc_iterator_end(buf));
			bv->cursor().backwardPos();
			LYXERR(Debug::FINDVERBOSE, "findBackAdv5: cur: "
				<< bv->cursor());
		}
		bv->clearSelection();
	} while (wrap_answer != 1);

	if (buf_orig != &view_.documentBufferView()->buffer())
		lyx::dispatch(FuncRequest(LFUN_BUFFER_SWITCH,
					  buf_orig->absFileName()));
	bv = view_.documentBufferView();
	// This may happen after a replace occurred
	if (cur_orig.pos() > cur_orig.lastpos())
		cur_orig.pos() = cur_orig.lastpos();
	bv->cursor().setCursor(cur_orig);
	view_.setBusy(false);
	theApp()->stopLongOperation();
	return false;
}


/// Return true if a match was found
bool FindAndReplaceWidget::findAndReplace(
	bool casesensitive, bool matchword, bool backwards,
	bool expandmacros, bool adhereformat, bool replace,
	bool keep_case, bool replace_all)
{
	Buffer & find_buf = find_work_area_->bufferView().buffer();
	docstring const & find_buf_name = find_buf.fileName().absoluteFilePath();

	if (find_buf.text().empty()) {
		view_.message(_("Nothing to search"));
		return false;
	}

	Buffer & repl_buf = replace_work_area_->bufferView().buffer();
	docstring const & repl_buf_name = replace ?
		repl_buf.fileName().absoluteFilePath() : docstring();

	FindAndReplaceOptions::SearchScope scope =
		FindAndReplaceOptions::S_BUFFER;
	if (CurrentDocument->isChecked())
		scope = FindAndReplaceOptions::S_BUFFER;
	else if (MasterDocument->isChecked())
		scope = FindAndReplaceOptions::S_DOCUMENT;
	else if (OpenDocuments->isChecked())
		scope = FindAndReplaceOptions::S_OPEN_BUFFERS;
	else if (AllManualsRB->isChecked())
		scope = FindAndReplaceOptions::S_ALL_MANUALS;
	else
		LATTEST(false);

	FindAndReplaceOptions::SearchRestriction restr =
		OnlyMaths->isChecked()
			? FindAndReplaceOptions::R_ONLY_MATHS
			: FindAndReplaceOptions::R_EVERYTHING;

	LYXERR(Debug::FINDVERBOSE, "FindAndReplaceOptions: "
	       << "find_buf_name=" << find_buf_name
	       << ", casesensitiv=" << casesensitive
	       << ", matchword=" << matchword
	       << ", backwards=" << backwards
	       << ", expandmacros=" << expandmacros
	       << ", adhereformat=" << adhereformat
	       << ", repl_buf_name" << repl_buf_name
	       << ", keep_case=" << keep_case
	       << ", scope=" << scope
	       << ", restr=" << restr);

	FindAndReplaceOptions opt(find_buf_name, casesensitive, matchword,
				  !backwards, expandmacros, !adhereformat,
				  repl_buf_name, keep_case, scope, restr);
	
	if (adhereformat) {
		// Formats to adhere
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("language",
					  !adhereLanguageCB->isChecked())));
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("color",
					  !adhereColorCB->isChecked())));
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("family",
					  !adhereFFamilyCB->isChecked())));
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("series",
					  !adhereFSeriesCB->isChecked())));
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("shape",
					  !adhereFShapeCB->isChecked())));
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("size",
					  !adhereFSizeCB->isChecked())));
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("markup",
					  !adhereMarkupCB->isChecked())));
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("underline",
					  !adhereUnderlineCB->isChecked())));
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("strike",
					  !adhereStrikeCB->isChecked())));
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("deleted",
					  !adhereDeletedCB->isChecked())));
		lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("sectioning",
					  !adhereSectioningCB->isChecked())));
	}
	lyx::dispatch(FuncRequest(LFUN_SEARCH_IGNORE, checkState("non-output-content",
				  ignoreNonOutputCB->isChecked())));
	
	return findAndReplaceScope(opt, replace_all);
}


docstring const FindAndReplaceWidget::checkState(string const & s, bool const b)
{
	docstring res = from_ascii(s) + from_ascii(" ");
	if (b)
		res += from_ascii("true");
	else
		res += from_ascii("false");
	return res;
}


bool FindAndReplaceWidget::findAndReplace(bool backwards, bool replace, bool replace_all)
{
	if (! view_.currentMainWorkArea()) {
		view_.message(_("No open document(s) in which to search"));
		return false;
	}
	// Finalize macros that are being typed, both in main document and in search or replacement WAs
	if (view_.currentWorkArea()->bufferView().cursor().macroModeClose())
		view_.currentWorkArea()->bufferView().processUpdateFlags(Update::Force);
	if (view_.currentMainWorkArea()->bufferView().cursor().macroModeClose())
		view_.currentMainWorkArea()->bufferView().processUpdateFlags(Update::Force);

	// FIXME: create a Dialog::returnFocus()
	// or something instead of this:
	view_.setCurrentWorkArea(view_.currentMainWorkArea());
	return findAndReplace(caseCB->isChecked(),
		wordsCB->isChecked(),
		backwards,
		expandMacrosCB->isChecked(),
		adhereFormatGB->isChecked(),
		replace,
		keepCaseCB->isChecked(),
		replace_all);
}


void FindAndReplaceWidget::hideDialog()
{
	dispatch(FuncRequest(LFUN_DIALOG_TOGGLE, "findreplaceadv"));
}


void FindAndReplaceWidget::on_findNextPB_clicked()
{
	findAndReplace(searchbackCB->isChecked(), false);
	find_work_area_->setFocus();
}


void FindAndReplaceWidget::on_replacePB_clicked()
{
	findAndReplace(searchbackCB->isChecked(), true);
	replace_work_area_->setFocus();
}


void FindAndReplaceWidget::on_replaceallPB_clicked()
{
	findAndReplace(searchbackCB->isChecked(), true, true);
	replace_work_area_->setFocus();
}


void FindAndReplaceWidget::on_searchbackCB_clicked()
{
	updateButtons();
}


void FindAndReplaceWidget::setFormatIgnores(bool const b)
{
	adhereLanguageCB->setChecked(b);
	adhereColorCB->setChecked(b);
	adhereFFamilyCB->setChecked(b);
	adhereFSeriesCB->setChecked(b);
	adhereFShapeCB->setChecked(b);
	adhereFSizeCB->setChecked(b);
	adhereMarkupCB->setChecked(b);
	adhereUnderlineCB->setChecked(b);
	adhereStrikeCB->setChecked(b);
	adhereDeletedCB->setChecked(b);
	adhereSectioningCB->setChecked(b);
}


void FindAndReplaceWidget::on_selectAllPB_clicked()
{
	setFormatIgnores(true);
}


void FindAndReplaceWidget::on_deselectAllPB_clicked()
{
	setFormatIgnores(false);
}


// Copy selected elements from bv's BufferParams to the dest_bv's
static void copy_params(BufferView const & bv, BufferView & dest_bv) {
	Buffer const & doc_buf = bv.buffer();
	BufferParams const & doc_bp = doc_buf.params();
	Buffer & dest_buf = dest_bv.buffer();
	dest_buf.params().copyForAdvFR(doc_bp);
	dest_bv.makeDocumentClass();
	dest_bv.cursor().current_font.setLanguage(doc_bp.language);
}


void FindAndReplaceWidget::showEvent(QShowEvent * /* ev */)
{
	LYXERR(Debug::DEBUG, "showEvent()" << endl);
	BufferView * bv = view_.documentBufferView();
	if (bv) {
		copy_params(*bv, find_work_area_->bufferView());
		copy_params(*bv, replace_work_area_->bufferView());
	}
	// no paragraph indentation for more space
	find_work_area_->bufferView().buffer().params().setParIndent(Length(0, Length::IN));
	replace_work_area_->bufferView().buffer().params().setParIndent(Length(0, Length::IN));

	find_work_area_->installEventFilter(this);
	replace_work_area_->installEventFilter(this);

	view_.setCurrentWorkArea(find_work_area_);
	LYXERR(Debug::FINDVERBOSE, "Selecting entire find buffer");
	dispatch(FuncRequest(LFUN_BUFFER_BEGIN));
	dispatch(FuncRequest(LFUN_BUFFER_END_SELECT));
}


void FindAndReplaceWidget::hideEvent(QHideEvent *ev)
{
	replace_work_area_->removeEventFilter(this);
	find_work_area_->removeEventFilter(this);
	this->QWidget::hideEvent(ev);
}


bool FindAndReplaceWidget::initialiseParams(std::string const & /*params*/)
{
	return true;
}


FindAndReplace::FindAndReplace(GuiView & parent,
			       Qt::DockWidgetArea area,
			       Qt::WindowFlags flags)
    : DockView(parent, "findreplaceadv", qt_("Advanced Find and Replace"),
		   area, flags)
{
	widget_ = new FindAndReplaceWidget(parent);
	setWidget(widget_);
	setFocusProxy(widget_);
	// FIXME: Allow all areas once the dialog re-orientation is fixed
	setAllowedAreas(Qt::DockWidgetAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea));
#ifdef Q_OS_MAC
	// On Mac show and floating
	setFloating(true);
#endif

//  FIXME: uncomment once the dialog re-orientation is fixed
//	connect(this, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
//	        widget_, SLOT(dockLocationChanged(Qt::DockWidgetArea)));
}


FindAndReplace::~FindAndReplace()
{
	setFocusProxy(nullptr);
	delete widget_;
}


bool FindAndReplace::initialiseParams(std::string const & params)
{
	return widget_->initialiseParams(params);
}


void FindAndReplaceWidget::updateWorkAreas()
{
	BufferView * bv = view_.documentBufferView();
	if (bv) {
		if (old_buffer_ != &bv->buffer()) {
				old_buffer_ = &bv->buffer();
				copy_params(*bv, find_work_area_->bufferView());
				copy_params(*bv, replace_work_area_->bufferView());
		}
	} else
		old_buffer_ = nullptr;
}


void FindAndReplaceWidget::updateButtons()
{
	if (searchbackCB->isChecked()) {
		findNextPB->setText(qt_("&< Find"));
		findNextPB->setToolTip(qt_("Find previous occurrence (Shift+Enter, forwards: Enter)"));
		replacePB->setText(qt_("< Rep&lace"));
		replacePB->setToolTip(qt_("Replace and find previous occurrence (Shift+Enter, forwards: Enter)"));
	} else {
		findNextPB->setText(qt_("Find &>"));
		findNextPB->setToolTip(qt_("Find next occurrence (Enter, backwards: Shift+Enter)"));
		replacePB->setText(qt_("Rep&lace >"));
		replacePB->setToolTip(qt_("Replace and find next occurrence (Enter, backwards: Shift+Enter)"));
	}

	BufferView * bv = view_.documentBufferView();
	bool const find_enabled = !find_work_area_->bufferView().buffer().empty();
	findNextPB->setEnabled(find_enabled);
	bool const replace_enabled = find_enabled && bv && !bv->buffer().isReadonly();
	replaceLabel->setEnabled(replace_enabled);
	replace_work_area_->setEnabled(replace_enabled);
	replacePB->setEnabled(replace_enabled);
	replaceallPB->setEnabled(replace_enabled);
}


void FindAndReplace::updateView()
{
	widget_->updateWorkAreas();
	widget_->updateButtons();
}


bool FindAndReplaceWidget::hasWorkArea(GuiWorkArea * wa) const
{
	return wa == find_work_area_ || wa == replace_work_area_;
}

} // namespace frontend
} // namespace lyx


#include "moc_FindAndReplace.cpp"
