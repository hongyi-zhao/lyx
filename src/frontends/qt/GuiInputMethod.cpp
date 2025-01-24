/**
 * \file GuiInputMethod.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>
#include <array>
#include <ios>

#include "GuiInputMethod.h"

#include "Buffer.h"
#include "ColorCache.h"
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "ColorSet.h"
#endif // QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "Cursor.h"
#include "CutAndPaste.h"
#include "ErrorList.h"
#include "GuiApplication.h"
#include "GuiCompleter.h"
#include "GuiFontLoader.h"
#include "GuiWorkArea.h"
#include "KeyMap.h"
#include "LyX.h"
#include "Row.h"
#include "Text.h"
#include "TextMetrics.h"

#include "support/debug.h"
#include "support/qstring_helpers.h"

using namespace std;

namespace lyx {

namespace frontend {

struct GuiInputMethod::Private
{
	BufferView * buffer_view_;
	GuiWorkArea * work_area_;
	/// the current preedit text of the input method
	docstring preedit_str_ = from_utf8("");
	/// the non-virtual cursor the position of which is the starting point of
	/// the preedits
	Cursor * cur_;
	/// if undergoing preedit input is overriding a selection
	bool has_selection_ = false;
	///
	bool has_multiple_rows_;

	QInputMethod * sys_im_ = guiApp->inputMethod();
	QLocale locale_;
	ColorCache & color_cache_ = guiApp->colorCache();
	QColor font_color_;
	QBrush font_brush_;

	ParagraphMetrics * pm_ptr_ = nullptr;

	PreeditStyle style_;

	InputMethodState im_state_;

	RowList::iterator rows_;
	size_type rows_size_;

	pos_type * cur_pos_ptr_ = nullptr;
	pos_type cur_row_idx_;
	pos_type caret_pos_;

	pos_type anchor_pos_;
	pos_type abs_pos_;

	bool real_boundary_    = false;
	bool virtual_boundary_ = false;

	Point init_point_;
	Dimension cur_dim_;
	/// pixel offset of the caret within preedit text
	std::array<int, 2> caret_offset_ = {0, 0};
};


GuiInputMethod::GuiInputMethod(GuiWorkArea *parent)
	: QObject(parent), d(new Private)
{
	d->work_area_ = parent;
	d->im_state_.enabled_ = true;
	d->buffer_view_ = &d->work_area_->bufferView();
	d->cur_ = &d->buffer_view_->cursor();

	LYXERR(Debug::DEBUG, "GuiInputMethod: Address of parent: " << parent);
	LYXERR(Debug::DEBUG, "GuiInputMethod: Address of buffer_view_: " <<
	       &d->work_area_->bufferView());

	connect(guiApp, &GuiApplication::acceptsInputMethod,
	        this, &GuiInputMethod::toggleInputMethodAcceptance);
	connect(this, &GuiInputMethod::inputMethodStateChanged,
	        d->sys_im_, &QInputMethod::update);
}

GuiInputMethod::~GuiInputMethod()
{
	delete d;
}


void GuiInputMethod::toggleInputMethodAcceptance(){
	// note that d->cur_->inset() is a cache so it lags from actual key moves
	if (d->cur_->inset().currentMode() == Inset::MATH_MODE ||
	        guiApp->isInCommandMode())
		d->im_state_.enabled_ = false;
	else
		d->im_state_.enabled_ = true;

	Q_EMIT inputMethodStateChanged(Qt::ImEnabled);
}


#ifdef Q_DEBUG
// this is experimental (2024/10/28)
// don't input methods implement ImHints except for Qt::ImhDigitsOnly yet?
void GuiInputMethod::setHint(Hint hint){
	d->work_area_->setInputMethodHints(
	            d->work_area_->inputMethodHints() | (Qt::InputMethodHints)hint);
	Q_EMIT inputMethodStateChanged(Qt::ImHints);
}
#endif // Q_DEBUG

/* *
 * *        Preedit
 * */

void GuiInputMethod::processPreedit(QInputMethodEvent* ev)
{
	if (!d->buffer_view_->inlineCompletion().empty())
		d->work_area_->completer().hideInline();

	// prepare for dark mode
	Color fg(Color_foreground);
	d->font_color_ = d->color_cache_.get(fg);
	d->font_brush_.setColor(d->font_color_);
	LYXERR(Debug::DEBUG,
	       "Preedit font color is set to " << d->font_color_.name());

	d->locale_ = d->sys_im_->locale();
	d->style_.lang_ = d->locale_;

	LYXERR(Debug::DEBUG,
	       "IM locale: " << d->locale_.bcp47Name() <<
	       " preeditString: " << ev->preeditString() <<
	       " commitString: " << ev->commitString());

	// output the commit string to the text
	if (!ev->commitString().isEmpty()) {
		// take care of commit string assigned to a shortcut
		// e.g. quotation mark on international keyboard
		KeySequence keyseq;
		for (QChar const & ch : ev->commitString()) {
			KeySymbol keysym;
			keysym.init(ch.unicode());
			keyseq.addkey(keysym, NoModifier);
		}
		FuncRequest cmd = theTopLevelKeymap().getBinding(keyseq);

		if (cmd == FuncRequest::noaction || cmd == FuncRequest::unknown
		        || cmd.action() == LFUN_SELF_INSERT)
			// insert the processed text in the document (handles undo)
			cmd = FuncRequest(LFUN_SELF_INSERT,
			                  qstring_to_ucs4(ev->commitString()));

		cmd.setOrigin(FuncRequest::KEYBOARD);
		lyx::dispatch(cmd);
	}

	// if selected
	if (d->cur_->selection()) {
		d->has_selection_ = true;
		d->cur_->beginUndoGroup();
		d->cur_->recordUndoSelection();
		cap::cutSelection(*d->cur_, false);
		d->cur_->endUndoGroup();
	}

		// if preedit is cancelled
	if (ev->preeditString().isEmpty() && ev->commitString().isEmpty() &&
	        d->has_selection_) {
		d->cur_->undoAction();
		d->has_selection_ = false;
	}

	d->preedit_str_ = qstring_to_ucs4(ev->preeditString());

	// parse the input method event attributes
	setPreeditStyle(ev->attributes());

	// initialize related position, par, rows and boundaries
	d->cur_row_idx_ = initializePositions(d->cur_);
	// initialize virtual caret to the anchor (real cursor) position
	d->init_point_ =
	        initializeCaretCoords(d->cur_row_idx_,
	                              d->real_boundary_ && !d->im_state_.edit_mode_);

	// Push preedit texts into row elements, which can shift the anchor
	// point of the preedit texts in a centered or right-flushed row.
	// Check such a shift immediately below.
	updateMetrics(d->cur_);

	/*
	 *          Draw caret
	 */

	// set offset of the virtual preedit caret from the real position
	d->caret_offset_ = setCaretOffset(d->caret_pos_);

	// set graphical geometry of the caret
	d->im_state_.cursor_rect_.setCoords(
	            d->init_point_.x + d->caret_offset_[0],
	            d->init_point_.y + d->caret_offset_[1],
	            d->init_point_.x + d->caret_offset_[0] + d->cur_dim_.width(),
	            d->init_point_.y + d->caret_offset_[1] + d->cur_dim_.height());
	d->im_state_.anchor_rect_ = d->im_state_.cursor_rect_;

	/*
	 *          Get surrounding text
	 */
	// take this opportunity to obtain the surrounding text to report back to
	// the input method
	// FIXME: is there a benefit to cache this?
	setSurroundingText(*d->cur_);

	// notify the completion to both im and app itself
	Q_EMIT inputMethodStateChanged(Qt::ImQueryInput);
	Q_EMIT preeditProcessed(ev);
}


void GuiInputMethod::setPreeditStyle(
        const QList<QInputMethodEvent::Attribute> & attr)
{
	d->style_.segments_.clear();

	const QInputMethodEvent::Attribute * focus_style = nullptr;

	// Since Qt6 and on MacOS, the initial entry seems to deliver information
	// about the focused segment (undocumented). We formulate the code to
	// utilize this fact keeping fail-safe against its failure.

#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	bool initial_tf_entry = true;
#else
	bool initial_tf_entry = false;
#endif
	// max segment position whose information we already have
	pos_type max_start = -1;

	// obtain attributes of input method
	for (const QInputMethodEvent::Attribute & it : attr) {

		switch (it.type) {
		case QInputMethodEvent::TextFormat:
			// We adapt to the protocol on MacOS Qt6 that the first entry is the
			// style for the focused segment and the rest is the baseline styles
			// for all segments including a duplicate of the focused one.
			// Since Qt documentation says there should be at most one format
			// for every part of the preedit string, we ignore possibility of
			// other duplicate cases (at least they are not observed).

			if (initial_tf_entry) {
				// most likely the style for the focused segment
				focus_style = &it;
				initial_tf_entry = false;
			} else
				setTextFormat(it, focus_style, max_start);

			break;

		case QInputMethodEvent::Cursor:
			d->caret_pos_ = setCaretPos(it.start);
			d->style_.caret_visible_ = it.length != 0;

			// colorization of caret is not implemented yet:
			// it will require modification of GuiWorkArea::Private::drawCaret
			// however we haven't encountered use cases in QInputMethodEvent
			d->style_.caret_color_ = it.value.value<QColor>();

			// we only prepare for finding such incoming event
			LYXERR(Debug::DEBUG,
			       "QInputMethodEvent: virtual cursor position: " <<
			       std::dec << d->caret_pos_ << " caret color: " <<
			       d->style_.caret_color_.name(QColor::HexRgb));
			break;

		// again not implemented yet?
		case QInputMethodEvent::Language:
			d->style_.lang_ = it.value.value<QLocale>();
			LYXERR(Debug::DEBUG,
			       "QInputMethodEvent::Language: " <<
			       d->style_.lang_.bcp47Name());
			break;

		// this also seems not implemented by any input methods
		case QInputMethodEvent::Ruby:
			d->style_.ruby_ = it.value.value<QString>();
			LYXERR(Debug::DEBUG,
			       "QInputMethodEvent::Ruby: " << d->style_.ruby_);
			break;

		case QInputMethodEvent::Selection:
			d->anchor_pos_ = it.start;
			d->caret_pos_ = it.start + it.length;
			d->cur_->setSelection(d->cur_->realAnchor(), it.length);
			LYXERR(Debug::DEBUG,
			       "QInputMethodEvent::Selection start: " << it.start <<
			       " length: " << it.length);
			break;

		} // end switch
	} // end for

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	// set background color for a focused segment
	if (!d->style_.segments_.empty()) {
		QTextCharFormat & focus_char_format =
		        d->style_.segments_[focusedSegmentIndex()].char_format_;
		if (d->style_.segments_.size() > 1) {
			QColor background_color(
			            lcolor.getX11HexName(Color_selection, false).c_str());
			QBrush brush(background_color);
			focus_char_format.setBackground(brush);
		}
	}
#endif // QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

	// edit mode: it has no focused segments
	// edit mode in Qt version 6 or later gives focus_style->length == 0 and
	// focus_style->start > d->preedit_str_.length() on MacOS (undocumented)
	d->im_state_.edit_mode_ =
	        (d->style_.segments_.size() == 0 && focus_style != nullptr)
	        || (focus_style != nullptr && focus_style->length == 0)
	        || (focus_style == nullptr && d->style_.segments_.size() == 1);

	if (d->im_state_.edit_mode_) {
		LYXERR(Debug::DEBUG, "preedit is in the edit mode");
		if (focus_style != nullptr && focus_style->length != 0) {
			QTextCharFormat char_format =
			        focus_style->value.value<QTextCharFormat>();
			conformToSurroundingFont(char_format);
			char_format.setFontUnderline(true);

			PreeditSegment seg = {focus_style->start,
			                      (size_type)focus_style->length, char_format};
			d->style_.segments_.push_back(seg);
		}
	} else {
		LYXERR(Debug::DEBUG, "preedit is in the completing mode");
		d->style_.caret_visible_ = false;
	}
}

void GuiInputMethod::setTextFormat(const QInputMethodEvent::Attribute & it,
                                   const QInputMethodEvent::Attribute * focus_style,
                                   pos_type & max_start)
{
	QTextCharFormat char_format = it.value.value<QTextCharFormat>();

	LYXERR(Debug::GUI,
	       "QInputMethodEvent::TextFormat start: " << it.start <<
	       " length: " << it.length <<
	       " underline? " << char_format.font().underline() <<
	       " UnderlineStyle: " << char_format.underlineStyle() <<
	       " fg: " << char_format.foreground().color().name() <<
	       " bg: " << char_format.background().color().name());

	// take union of the current style and the focus style
	// the last condition clears background color in the edit mode
	// this is simply from a (subjective) aethetic consideration
	if (focus_style != nullptr &&
	        (it.start == focus_style->start || focus_style->length == 0) &&
	        it.length < (int)d->preedit_str_.length() /* completing mode*/)
		char_format.merge(focus_style->value.value<QTextCharFormat>());

	// color adjustment for dark mode
	if (guiApp->isInDarkMode()) {
		if (char_format.foreground().color().black() >=
		        char_format.background().color().black())
			char_format.setForeground(
			            guiApp->palette().color(
			                QPalette::Active, QPalette::WindowText));
		// make background color 10% lighter
		char_format.setBackground(char_format.background().color().lighter(110));
	}

	conformToSurroundingFont(char_format);

#if (! defined(Q_OS_MACOS)) || QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	char_format.setFontUnderline(true);
#endif

	// QLocale is used for wrapping words
	char_format.setProperty(QMetaType::QLocale, d->style_.lang_);

	// Do we already have some information about the incoming segment?
	// On Linux, same information arrives repeatedly from IM (a bug? 2025/1/10)
	if (it.start <= max_start) {
		for (auto & seg : d->style_.segments_) {
			if (it.start == seg.start_ &&
			        (it.length == (int)seg.length_ || it.length == 0))
				// merge old and new information on style
				seg.char_format_.merge(char_format);
		}
	} else {
		// push the constructed char format together with start and length
		// to the list
		PreeditSegment seg = {it.start, (size_type)it.length, char_format};
		d->style_.segments_.push_back(seg);
	}

	if (it.start > max_start)
		max_start = it.start;
}


void GuiInputMethod::conformToSurroundingFont(QTextCharFormat & char_format) {
	// Surrounding font at the cursor position
	d->cur_->setCurrentFont();
	const QFont & surrounding_font =
	        getFont(d->cur_->real_current_font.fontInfo());

	// set font families
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
	if (char_format.fontFamilies().isNull()) {
#else
	if (char_format.fontFamily().isNull()) {
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 1, 0)
		char_format.setFontFamilies(surrounding_font.families());
#else
		// setFontFamily and fontFamily() are deprecated at Qt-6.1 or later
		// they are introduced at Qt 5.13 (and)
		char_format.setFontFamily(surrounding_font.family());
#endif
	}
	// set font size
	if (char_format.fontPointSize() == 0)
		char_format.setFontPointSize(surrounding_font.pointSizeF());

	// set font weight to fit surrounding text
	char_format.setFontWeight(surrounding_font.weight());

	// set font italic to fit surrounding text
	if(!char_format.fontItalic())
		char_format.setFontItalic(surrounding_font.italic());
	else
		char_format.setFontItalic(!surrounding_font.italic());

}


/* *
 * *         Drawing
 * */
void GuiInputMethod::setParagraphMetrics(ParagraphMetrics & pm){
	d->pm_ptr_ = &pm;
}

std::array<int,2> GuiInputMethod::setCaretOffset(pos_type caret_pos){

	// Note that preedit elements are virtual and not counted in pos().
	// pos: 0 1 2 3 4 5 6 7 8 | 8 8 8 8 8 8 8 8 8 8 | 9 10 11 ...
	//      <-  non-virtual ->|<- preedit element ->|<- non-virtual
	//
	// This is also true for next_row_pos.
	// On the other hand d->caret_pos_ counts preedit elements.

	if (d->preedit_str_.empty()) {
		// reset shift of the virtual caret as the preedit string is cancelled
		// this part is also visited right before starting preedit input
		return {0, 0};
	}
	// when preedit cursor is available, string fonts are all common
	// so pick up the last one
	QTextCharFormat & qtcf = d->style_.segments_.back().char_format_;
	QFontMetrics qfm(qtcf.font());
	QString str_before_caret = "";

#if defined(Q_OS_MACOS) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	if (d->im_state_.edit_mode_)
		str_before_caret =
		    toqstr(d->preedit_str_.substr(0, caret_pos - *d->cur_pos_ptr_));
	else
		// adjust for the reported caret position in the completion mode in Qt5
		str_before_caret = toqstr(
		            d->preedit_str_.substr(0, caret_pos - *d->cur_pos_ptr_ -
		                                   shiftFromCaretToSegmentHead()));
#else
	str_before_caret =
		toqstr(d->preedit_str_.substr(0, caret_pos - *d->cur_pos_ptr_));
#endif

	// process line wrapping
	//
	// NOTE:
	// preedits added on boundary goes to the beginning of the next line
	//         -> real_boundary = true   virtual_boundary = false
	// preedits added on boundary is appended to the same line
	//         -> real_boundary = false   virtual_boundary = true

	PreeditRow caret_row = getCaretInfo(d->real_boundary_, d->virtual_boundary_);
	std::array<int,2> caret_offset {};

	// caret_row.index doesn't decrease with virtual_boundary
	// has multiple preedit rows
	if (caret_row.index > d->cur_row_idx_ || d->real_boundary_) {
		QString lastline_str;

		if (d->real_boundary_ && caret_row.index == d->cur_row_idx_)
			lastline_str = str_before_caret;
		else
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			lastline_str = str_before_caret.sliced(
			    caret_row.pos - *d->cur_pos_ptr_,
			    *d->cur_pos_ptr_ + str_before_caret.length() - caret_row.pos);
#else
			lastline_str = str_before_caret.mid(
			    caret_row.pos - *d->cur_pos_ptr_,
			    *d->cur_pos_ptr_ + str_before_caret.length() - caret_row.pos);
#endif
		//
		// calculate left margin
		//
		int left_margin;
		int inset_offset;
		if (d->cur_->depth() > 1) {
			pit_type & outer_pit = d->cur_->bottom().pit();
			TextMetrics & tm =
			        d->buffer_view_->textMetrics(d->cur_->bottom().text());
			left_margin = tm.leftMargin(outer_pit);
			inset_offset = d->cur_->inset().leftOffset(d->buffer_view_);
		} else {
			left_margin = d->rows_[caret_row.index].left_margin;
			inset_offset = 0;
		}
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		if (d->real_boundary_ && !d->im_state_.edit_mode_)
			caret_offset[0] = qfm.horizontalAdvance(lastline_str);
		else
			caret_offset[0] = - d->init_point_.x + left_margin
			        + inset_offset + qfm.horizontalAdvance(lastline_str);
#else
		if (d->real_boundary_ && !d->im_state_.edit_mode_)
			caret_offset[0] = qfm.width(lastline_str);
		else
			caret_offset[0] = - d->init_point_.x + left_margin
			        + inset_offset + qfm.width(lastline_str);
#endif
	} else {
		int left_margin_diff = d->rows_[caret_row.index].left_margin -
		        d->rows_[d->cur_row_idx_].left_margin;
		// the case in which the preedit caret is in the first row of the preedit
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		caret_offset[0] =
		        left_margin_diff + qfm.horizontalAdvance(str_before_caret);
#else
		caret_offset[0] =
		        left_margin_diff + qfm.width(str_before_caret);
#endif
	}

	// vertical offset only applicable to main text
	caret_offset[1] = 0;
	for (pos_type i = d->cur_row_idx_ -
	     (d->real_boundary_ && d->im_state_.edit_mode_);
	     i < caret_row.index -
	     (d->real_boundary_ && !d->im_state_.edit_mode_); ++i)
		caret_offset[1] += d->rows_[i].descent() + d->rows_[i+1].ascent();

	return caret_offset;
}


// returns (x_offset, y_offset) array
std::array<int, 2> GuiInputMethod::preeditCaretOffset() const {
	return d->caret_offset_;
}


// returns whether the caret is visible
bool GuiInputMethod::isCaretVisible() const {
	return d->style_.caret_visible_;
}


#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
// Returns index of the focused segment (just in front of the virtual caret)
pos_type GuiInputMethod::focusedSegmentIndex() {
	pos_type idx = 0;
	// position within the preedit string
	pos_type pos = d->caret_pos_ - d->cur_->pos();

#ifdef Q_OS_MACOS
	// on MacOS, cursor position is the end of the preedit segment
	bool entry_found = false;
	if (!d->style_.segments_.empty() &&
	        d->style_.segments_[0].start_ != pos) {
		for (size_type i = 1; i < d->style_.segments_.size(); i++) {
			if (d->style_.segments_[i].start_ == pos) {
				entry_found = true;
				idx = --i;
				break;
			}
		}
		if (!entry_found) idx = d->style_.segments_.size() - 1;
	}
#else // Q_OS_MACOS
	// on other systems(?), cursor pos is the beginning of the preedit segment
	if (!d->style_.segments_.empty()) {
		for (size_type i = 0; i < d->style_.segments_.size(); i++) {
			if (d->style_.segments_[i].start_ == pos) {
				idx = i;
				break;
			}
		}
	}
#endif // Q_OS_MACOS

	return idx;
}


// pos shift factor from the caret to the selection segment's head
int GuiInputMethod::shiftFromCaretToSegmentHead() {
	if (d->style_.segments_.empty()) {
		return 0;
	} else {
		pos_type focused_segment_idx = focusedSegmentIndex();
		QString segment_string =
		        toqstr(d->preedit_str_.substr(
		                   d->style_.segments_[focused_segment_idx].start_,
		                   d->style_.segments_[focused_segment_idx].length_));
		return segment_string.length();
	}
}
#endif // QT_VERSION < QT_VERSION_CHECK(6, 0, 0)


/* *
 * *        Input Method Query
 * */
void GuiInputMethod::processQuery(Qt::InputMethodQuery query)
{
	// input method is not ready yet
	if (d->buffer_view_->inputMethod() == nullptr) {
		QVariant null_answer;
		Q_EMIT queryProcessed(null_answer);
		return;
	}

	docstring msg = "Responded to query " +
	        inputMethodQueryFlagsAsString(query) + " = ";

	switch (query) {

	// The widget accepts input method input
	case Qt::ImEnabled: {
		if (d->im_state_.enabled_)
			LYXERR(Debug::DEBUG, msg << "true");
		else
			LYXERR(Debug::DEBUG, msg << "false");
		Q_EMIT queryProcessed(d->im_state_.enabled_);
		break;
	}
	// this is the CJK-specific composition window position and
	// the context menu position when the menu key is pressed.
	case Qt::ImCursorRectangle: {
		QRectF * rect;
		if (d->im_state_.edit_mode_) {
			// in the editing mode, cursor_rect_ follows the position of the
			// virtual caret, but the drop down of predicted candidates wants
			// the starting point of the preedit, so respond with anchor_rect_
			// that points the starting point during the editing mode
			rect = &d->im_state_.anchor_rect_;
			LYXERR(Debug::DEBUG,
			       "     (Edit mode: use anchor_rect_ for ImCursorRectangle)");
		} else
			rect = &d->im_state_.cursor_rect_;

		LYXERR(Debug::DEBUG, msg << " x:" << rect->x() << " y:" << rect->y()
		       << " w:" << rect->width() << " h:" << rect->height());
		Q_EMIT queryProcessed(*rect);
		break;
	}
	case Qt::ImCurrentSelection: {
		LYXERR(Debug::DEBUG, msg << d->cur_->selectionAsString(false));
		Q_EMIT queryProcessed(toqstr(d->cur_->selectionAsString(false)));
		break;
	}
	// plain text around the input area, for example the current paragraph
	case Qt::ImSurroundingText: {
		if (d->im_state_.surrounding_text_.empty())
			LYXERR(Debug::DEBUG, msg << "\"\"");
		else
			LYXERR(Debug::DEBUG, msg <<
			       d->im_state_.surrounding_text_.substr(0, 20) << "...");
		Q_EMIT queryProcessed(toqstr(d->im_state_.surrounding_text_));
		break;
	}
	// logical position of the cursor within the entire document
	case Qt::ImAbsolutePosition: {
		// FIXME: position should be set only when there was a non-virtual
		// change in the document
		setAbsolutePosition(*d->cur_);
		LYXERR(Debug::DEBUG, msg << std::dec << d->abs_pos_);
		Q_EMIT queryProcessed((qlonglong)d->abs_pos_);
		break;
	}
	// plain text before the cursor
	case Qt::ImTextBeforeCursor: {
		if (d->im_state_.text_before_.empty())
			LYXERR(Debug::DEBUG, msg << "\"\"");
		else
			LYXERR(Debug::DEBUG, msg << "…" << d->im_state_.text_before_);
		QVariant str(toqstr(d->im_state_.text_before_));
		Q_EMIT queryProcessed(str);
		break;
	}
	// plain text after the cursor
	case Qt::ImTextAfterCursor: {
		if (d->im_state_.text_after_.empty())
			LYXERR(Debug::DEBUG, msg << "\"\"");
		else
			LYXERR(Debug::DEBUG, msg << d->im_state_.text_after_);
		QVariant str(toqstr(d->im_state_.text_after_));
		Q_EMIT queryProcessed(str);
		break;
	}
	// logical position of the cursor within the text surrounding the input area
	case Qt::ImCursorPosition: {
		LYXERR(Debug::DEBUG, msg << std::dec << d->cur_->pos());
		Q_EMIT queryProcessed((qlonglong)d->cur_->pos());
		break;
	}
	// position of the selection anchor
	case Qt::ImAnchorPosition: {
		LYXERR(Debug::DEBUG, msg << std::dec << (unsigned int)d->anchor_pos_);
		Q_EMIT queryProcessed(QVariant((unsigned int)d->anchor_pos_));
		break;
	}
	case Qt::ImInputItemClipRectangle: {
		QRectF viewport(d->work_area_->viewport()->x(),
		                d->work_area_->viewport()->y(),
		                d->work_area_->viewport()->width(),
		                d->work_area_->viewport()->height());
		LYXERR(Debug::DEBUG, msg << "(x,y,w,h) = " <<
		       viewport.x() << ", " << viewport.y() << ", " <<
		       viewport.width() << ", " << viewport.height() << ")");
		Q_EMIT queryProcessed(viewport);
		break;
	}
	// hints for input method on expected input
	case Qt::ImHints: {
		LYXERR(Debug::DEBUG, msg << "0x" << std::hex <<
		       d->work_area_->inputMethodHints());
		Q_EMIT queryProcessed((qlonglong)d->work_area_->inputMethodHints());
		break;
	}
	// Qt::ImAnchorRectangle holds the selection rectangle in preedit.
	// it seems this property is not yet used by most of input methods
	// as of August 2024.
	case Qt::ImAnchorRectangle: {
		LYXERR(Debug::DEBUG, msg << " x:" << d->im_state_.anchor_rect_.x()
		       << " y:" << d->im_state_.anchor_rect_.y()
		       << " w:" << d->im_state_.anchor_rect_.width()
		       << " h:" << d->im_state_.anchor_rect_.height());
		Q_EMIT queryProcessed(d->im_state_.anchor_rect_);
		break;
	}
	default: {
		QVariant null;
		Q_EMIT queryProcessed(null);
	}
	}
}

// Returns enum Qt::InputMethodQuery constant from its value.
// This is for debugging purpose only.
docstring GuiInputMethod::inputMethodQueryFlagsAsString(long int query) const
{
	docstring str;
	switch (query) {
	case 0x1:       str += "Qt::ImEnabled";                 break;
	case 0x2:       str += "Qt::ImCursorRectangle";         break;
	case 0x4:       str += "Qt::ImFont";                    break;
	case 0x8:       str += "Qt::ImCursorPosition";          break;
	case 0x10:      str += "Qt::ImSurroundingText";         break;
	case 0x20:      str += "Qt::ImCurrentSelection";        break;
	case 0x40:      str += "Qt::ImMaximumTextLength";       break;
	case 0x80:      str += "Qt::ImAnchorPosition";          break;
	case 0x100:     str += "Qt::ImHints";                   break;
	case 0x200:     str += "Qt::ImPreferredLanguage";       break;
	case 0x80000000:str += "Qt::ImPlatformData";            break;
	case 0x400:     str += "Qt::ImAbsolutePosition";        break;
	case 0x800:     str += "Qt::ImTextBeforeCursor";        break;
	case 0x1000:    str += "Qt::ImTextAfterCursor";         break;
	case 0x2000:    str += "Qt::ImEnterKeyType";            break;
	case 0x4000:    str += "Qt::ImAnchorRectangle";         break;
	case 0x8000:    str += "Qt::ImInputItemClipRectangle";  break;
	case 0x10000:   str += "Qt::ImReadOnly";                break;
	}
	return str;
}


//
//     helper functions
//

pos_type GuiInputMethod::initializePositions(Cursor * cur) {
	// the function sets the following variables:
	//     d->cur_pos_ptr_ = pointer to the starting pos of preedits
	//     d->real_boundary_ = if the starting point is boundary
	//     d->virtual_boundary_ = if the preedits hit the boundary
	// and returns the row index of the starting point of preedits

	// position of the real cursor (also the start of the preedit)
	d->cur_pos_ptr_ = &cur->top().pos();
	d->pm_ptr_ = resetParagraphMetrics(cur);
	d->anchor_pos_ = *d->cur_pos_ptr_;
	// Note that getRowIndex(., false) gives the row index *after* preedit
	// strings since they are virtual, so it increases as preedit strings go
	// over multiple rows. To fix it at the starting point, getRowIndex(., true)
	// is used here. As a result, cur_row_idx points the row before the boundary
	// in the case it is true.
	pos_type cur_row_idx = d->pm_ptr_->getRowIndex(*d->cur_pos_ptr_, true);

	//
	// boundary check
	//
	// the real cursor is on boundary
	bool & real_boundary    = d->real_boundary_;
	// the real cursor is followed by virtual chars on boundary
	bool & virtual_boundary = d->virtual_boundary_;
	// cursor is after real boundary
	bool post_real_boundary = false;

	// NOTE:
	// preedits added on boundary goes to the beginning of the next line
	//         -> real_boundary = true   virtual_boundary = false
	// preedits added on boundary is appended to the same line
	//         -> real_boundary = false  virtual_boundary = true

	// width of first-row preedit string in the case of virtual boundary
	int orphan_width = 0;
	if (!(d->rows_->empty() || d->rows_[cur_row_idx].empty()) &&
	        d->rows_[cur_row_idx].back().isPreedit())
		orphan_width =
		        horizontalAdvance(d->rows_[cur_row_idx].back().str);

	int const max_width =
	        d->buffer_view_->textMetrics(cur->innerText()).maxWidth()
	        - d->rows_[cur_row_idx].right_margin;

	bool const has_room_to_insert = d->preedit_str_.empty() ?
	            // if preedit is empty, examine the room with a typical letter
	            max_width - d->rows_[cur_row_idx].width()
	            + orphan_width > horizontalAdvance(from_utf8("あ")) :
	            max_width - d->rows_[cur_row_idx].width() + orphan_width
	            > horizontalAdvance(d->preedit_str_.substr(0,1));

	if (cur_row_idx > 0) {
		// the second conditions below are to guarantee they are not the
		// continued lines of the virtual_boundary case
		real_boundary = d->buffer_view_->cursor().boundary()
		        && !d->rows_[cur_row_idx - 1].back().isPreedit()
		        && !has_room_to_insert;
		virtual_boundary = d->buffer_view_->cursor().boundary()
		        && !d->rows_[cur_row_idx - 1].back().isPreedit()
		        && has_room_to_insert;
	} else {
		real_boundary = d->buffer_view_->cursor().boundary()
		        && !has_room_to_insert;
		virtual_boundary = d->buffer_view_->cursor().boundary()
		        && has_room_to_insert;
	}
	// cursor is at the head of the next row after boundary
	post_real_boundary =
	        cur_row_idx + 1 < (pos_type)d->rows_size_ ?
	            *d->cur_pos_ptr_ == d->rows_[cur_row_idx+1].pos() &&
	            !has_room_to_insert : false;

	// Preedit string and the caret starts from the new line when either
	// real_boundary or post_real_boundary is true, so set the cursor row there.
	// As far as caret position is concerned, real_boundary and
	// post_real_boundary can be treated interchangeably, so merge them into
	// real_boundary, i.e. d->real_boundary_.
	real_boundary |= post_real_boundary;
	if (real_boundary)
		++cur_row_idx;

	LYXERR(Debug::DEBUG, "========== BEGIN: initializePositions ==========");
	LYXERR(Debug::DEBUG, "cur_row_idx   = " << cur_row_idx <<
	        "\treal_boundary    = " << real_boundary);
	LYXERR(Debug::DEBUG, "                      " <<
	        "\tvirtual_boundary = " << virtual_boundary);
	LYXERR(Debug::DEBUG, "max width     = " << std::dec <<
	        d->buffer_view_->textMetrics(
	            cur->innerText()).maxWidth()
	             - d->rows_[cur_row_idx].right_margin);
	if (real_boundary)
		LYXERR(Debug::DEBUG, "row width     = " << std::dec <<
		        d->rows_[cur_row_idx-1].width());
	else
		LYXERR(Debug::DEBUG, "row width     = " << std::dec <<
		        d->rows_[cur_row_idx].width());
	if (d->preedit_str_.empty())
		LYXERR(Debug::DEBUG, "wchar width   = " <<
		       horizontalAdvance(from_utf8("あ")));
	else
		LYXERR(Debug::DEBUG, "wchar width = " <<
		        horizontalAdvance(d->preedit_str_.substr(0,1)));
	LYXERR(Debug::DEBUG, "========== END:   initializePositions ==========");

	return cur_row_idx;
}


Point GuiInputMethod::initializeCaretCoords(pos_type const cur_row_idx,
                                            bool const boundary)
{
	// returns the real caret position adjusting boundary case
	// so that it points to the head of the next row

	// reset item transformation since it gets wrong after the item get
	// lost and regain focus.
	d->work_area_->resetInputItemGeometry();

	Point point;
	// get dimension of the non-virtual caret
	d->buffer_view_->caretPosAndDim(point, d->cur_dim_);

	// the position of the completion candidate window for the first selected
	// segment seems to be determined by the position of cursor_rect *before*
	// it pops up
	// so the virtual caret must be reported as of the start of the next row
	// if it is on the boundary of a row
	// it only relates to the completion mode
	if (boundary) {
		Row & cur_row = d->rows_[cur_row_idx];
		// the initial position of the preedit is on boundary
		point = {cur_row.left_margin,
		         point.y + cur_row.contents_dim().height()};
	}
	return point;
}


void GuiInputMethod::updateMetrics(Cursor * cur) {
	d->buffer_view_->updateMetrics();
	resetParagraphMetrics(cur);
}


ParagraphMetrics * GuiInputMethod::resetParagraphMetrics(Cursor * cur) {
	// paragraph metrics of the par we are in
	ParagraphMetrics * pm_ptr =
	    &cur->bv().textMetrics(cur->innerText()).parMetrics(cur->top().pit());
	d->rows_ = pm_ptr->rows().begin();
	d->rows_size_ = pm_ptr->rows().size();
	// ID of the row in which the current real cursor resides
	return pm_ptr;
}


pos_type GuiInputMethod::setCaretPos(size_type preedit_length) {
	return d->cur_->top().pos() + preedit_length;
}


GuiInputMethod::PreeditRow GuiInputMethod::getCaretInfo(
        const bool real_boundary, const bool virtual_boundary){

	const pos_type second_row_idx = d->cur_row_idx_ + 1 - virtual_boundary;

	// accumulate the length of preedit elements within d->rows_[d->cur_row_idx_]
	// the length of str is used since preedits has zero widths (pos == endpos)
	// second_row_pos is only useful when preedit string goes over two rows
	Row::const_iterator begin =
	        d->rows_[d->cur_row_idx_].findElement(*d->cur_pos_ptr_, false);
	pos_type second_row_pos = *d->cur_pos_ptr_;
	for (Row::const_iterator eit = begin;
	     eit < d->rows_[d->cur_row_idx_].end(); ++eit)
		second_row_pos += eit->str.length();

	PreeditRow caret_row{};

	// when real_boundary is true, cursor position is at the beginning of the
	// new line, while the caret on screen stays at the end of one line above
	// below is the starting point to calculate caret_row.pos
	caret_row.pos = (real_boundary && !d->im_state_.edit_mode_) ?
	            *d->cur_pos_ptr_ : second_row_pos;

	// if the preedit caret is on the second row or later, count the second row
	caret_row.index = d->caret_pos_ > second_row_pos ?
	            second_row_idx + virtual_boundary : d->cur_row_idx_;

	// the second row exists and begins with the preedit
	if (second_row_idx < (pos_type)d->rows_size_ &&
	        d->rows_[second_row_idx + virtual_boundary].begin()->isPreedit()) {

		for (pos_type i = second_row_idx + virtual_boundary;
			 i < (pos_type)d->rows_size_; i++)
		{
			if (d->rows_[i].front().isPreedit()) {
				int row_length = 0;
				for (const Row::Element & elm : d->rows_[i])
					row_length += elm.str.length();
				if (d->caret_pos_ <= caret_row.pos + row_length)
					break;
				caret_row.pos += row_length;
				++caret_row.index;
			}
		}
	} else
		caret_row.pos = *d->cur_pos_ptr_;

	LYXERR(Debug::DEBUG, "============= BEGIN: getCaretInfo ===============");
	LYXERR(Debug::DEBUG, "*d->cur_pos_ptr   = " << *d->cur_pos_ptr_);
	LYXERR(Debug::DEBUG, "second_row_pos    = " << second_row_pos);
	LYXERR(Debug::DEBUG, "d->caret_pos_     = " << std::dec << d->caret_pos_ <<
	        "\tcaret_row.index   = " << std::dec << caret_row.index);
	LYXERR(Debug::DEBUG, "caret_row.pos_    = " << std::dec << caret_row.pos <<
	        "\td->cur_row_idx_   = " << std::dec << d->cur_row_idx_);
	LYXERR(Debug::DEBUG, "real_boundary     = " << real_boundary <<
	        "\tvirtual_boundary  = " << virtual_boundary);
	LYXERR(Debug::DEBUG, "=============== END: getCaretInfo ===============");

	return caret_row;
}

// Get the text before and after the cursor, only those representable as
// a string, which is used to report back to the input method
void GuiInputMethod::setSurroundingText(const Cursor & cur) {
	if (cur.top().inset().asInsetText() == nullptr)
		return;

	docstring partext = cur.top().paragraph().asString();

	// count insets that can affect the cur position in the above string
	InsetList const & inset_list = cur.paragraph().insetList();
	pos_type insets_before_cur = 0;
	for (const auto & inset : inset_list) {
		if (inset.pos < *d->cur_pos_ptr_)
			++insets_before_cur;
	}

	d->im_state_.text_before_ =
	        partext.substr(0, *d->cur_pos_ptr_ - insets_before_cur);
	d->im_state_.text_after_  =
	        partext.substr(*d->cur_pos_ptr_ - insets_before_cur);
	d->im_state_.surrounding_text_ =
	        d->im_state_.text_before_ + d->im_state_.text_after_;

	LYXERR(Debug::DEBUG, "surrounding text before cursor = " <<
	       d->im_state_.text_before_);
	LYXERR(Debug::DEBUG, "surrounding text after  cursor = " <<
	       d->im_state_.text_after_);

	// notify the input method about the update
#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
	// work around Qt bug
	// see: https://code.qt.io/cgit/qt/qtbase.git/commit/?id=e759d38d
	using ::operator|;
#endif
	Q_EMIT inputMethodStateChanged(Qt::ImSurroundingText |
	                               Qt::ImTextBeforeCursor |
	                               Qt::ImTextAfterCursor);
	return;
}

docstring & GuiInputMethod::preeditString() const
{
	return d->preedit_str_;
}
pos_type & GuiInputMethod::segmentStart(size_type seg_id) const
{
	return d->style_.segments_[seg_id].start_;
}
size_type & GuiInputMethod::segmentLength(size_type seg_id) const
{
	return d->style_.segments_[seg_id].length_;
}
size_type GuiInputMethod::segmentSize() const
{
	return d->style_.segments_.size();
}
QTextCharFormat & GuiInputMethod::charFormat(pos_type index) const {
	return d->style_.segments_[index].char_format_;
}
pos_type GuiInputMethod::charFormatIndex(pos_type pos) const {
	// find out format index at preedit_pos
	// note that vector preeditStarts may not be sorted
	size_t fmt_index;
	std::vector<pair<size_type, size_type>> fmt_candidates;
	for (size_t j = 0 ; j < d->style_.segments_.size() ; ++j)
		if ((pos_type)d->style_.segments_[j].start_ <= pos)
			fmt_candidates.push_back(make_pair(d->style_.segments_[j].start_,j));
	if (fmt_candidates.size() > 0) {
		fmt_index = (*max_element(std::begin(fmt_candidates),
		                          std::end(fmt_candidates))).second;
	} else
		fmt_index = 0;

	return fmt_index;
}

QLocale & GuiInputMethod::locale() const
{
	return d->locale_;
}

// set absolute position of the cursor in the entire document
void GuiInputMethod::setAbsolutePosition(Cursor & cur) const
{
	d->abs_pos_ = 0;
	ParagraphList paras = cur.buffer()->paragraphs();
	for (int i=0; i < (int)paras.size(); ++i) {
		if (i >= cur.pit()) break;
		d->abs_pos_ += paras[i].size();
	}
	d->abs_pos_ += cur.pos();
}

int GuiInputMethod::horizontalAdvance(docstring const & s,
                                      pos_type const char_format_index) {
	QFontMetrics qfm(charFormat(char_format_index).font());
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return qfm.horizontalAdvance(toqstr(s));
#else
	return qfm.width(toqstr(s));
#endif
}

int GuiInputMethod::horizontalAdvance(docstring const & s) {
	QTextCharFormat qtcf;
	conformToSurroundingFont(qtcf);
	QFontMetrics qfm(qtcf.font());
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return qfm.horizontalAdvance(toqstr(s));
#else
	return qfm.width(toqstr(s));
#endif
}

bool GuiInputMethod::canWrapAnywhere(pos_type const char_format_index) {
	QLocale locale =
		charFormat(char_format_index).property(QMetaType::QLocale).toLocale();
	// Languages wrappable at any place that use the input method
	// CJK is listed here but anything else?
	if (locale == QLocale::Japanese || locale == QLocale::Chinese ||
		locale == QLocale::Korean || locale == QLocale::LiteraryChinese)
		return true;
	else
		return false;
}

} // namespace frontend

} // namespace lyx

#include "moc_GuiInputMethod.cpp"
