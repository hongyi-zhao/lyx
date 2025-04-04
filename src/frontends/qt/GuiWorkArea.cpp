/**
 * \file GuiWorkArea.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiInputMethod.h"
#include "GuiWorkArea.h"
#include "GuiWorkArea_Private.h"

#include "ColorCache.h"
#include "GuiApplication.h"
#include "GuiCompleter.h"
#include "GuiKeySymbol.h"
#include "GuiPainter.h"
#include "GuiView.h"
#include "Menus.h"
#include "qt_helpers.h"

#include "Buffer.h"
#include "BufferList.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "CoordCache.h"
#include "Cursor.h"
#include "Font.h"
#include "FuncRequest.h"
#include "KeyMap.h"
#include "KeySymbol.h"
#include "KeySequence.h"
#include "LyX.h"
#include "LyXRC.h"
#include "LyXVC.h"
#include "Text.h"
#include "TextMetrics.h"
#include "version.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/lassert.h"
#include "support/TempFile.h"

#include "frontends/Application.h"
#include "frontends/CaretGeometry.h"
#include "frontends/FontMetrics.h"
#include "frontends/WorkAreaManager.h"

#include <QContextMenuEvent>
#include <QDrag>
#include <QHelpEvent>
#include <QInputMethod>
#ifdef Q_OS_MAC
#include <QProxyStyle>
#endif
#include <QMainWindow>
#include <QMimeData>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QScrollBar>
#include <QStyleOption>
#include <QStylePainter>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QMenuBar>

#include <array> //Needed for Windows (preeditCaretOffset())
#include <cmath>
#include <iostream>

#undef KeyPress
#undef NoModifier

using namespace std;
using namespace lyx::support;

namespace lyx {


/// return the LyX mouse button state from Qt's
static mouse_button::state q_button_state(Qt::MouseButton button)
{
	mouse_button::state b = mouse_button::none;
	switch (button) {
		case Qt::LeftButton:
			b = mouse_button::button1;
			break;
		case Qt::MiddleButton:
			b = mouse_button::button2;
			break;
		case Qt::RightButton:
			b = mouse_button::button3;
			break;
		default:
			break;
	}
	return b;
}


/// return the LyX mouse button state from Qt's
mouse_button::state q_motion_state(Qt::MouseButtons state)
{
	mouse_button::state b = mouse_button::none;
	if (state & Qt::LeftButton)
		b |= mouse_button::button1;
	if (state & Qt::MiddleButton)
		b |= mouse_button::button2;
	if (state & Qt::RightButton)
		b |= mouse_button::button3;
	return b;
}


namespace frontend {

// This is a 'heartbeat' generating synthetic mouse move events when the
// cursor is at the top or bottom edge of the viewport. One scroll per 0.2 s
SyntheticMouseEvent::SyntheticMouseEvent()
	: timeout(200), restart_timeout(true)
{}


GuiWorkArea::Private::Private(GuiWorkArea * parent)
	: p(parent), completer_(new GuiCompleter(p, p))
{
	use_backingstore_ = guiApp->drawStrategy() == DrawStrategy::Backingstore;
	LYXERR(Debug::WORKAREA, "Drawing strategy: " << guiApp->drawStrategyDescription());

	int const time = QApplication::cursorFlashTime() / 2;
	if (time > 0) {
		caret_timeout_.setInterval(time);
		caret_timeout_.start();
	} else {
		// let's initialize this just to be safe
		caret_timeout_.setInterval(500);
	}
}


GuiWorkArea::Private::~Private()
{
	// If something is wrong with the buffer, we can ignore it safely
	try {
		buffer_view_->buffer().workAreaManager().remove(p);
	} catch(...) {}
	delete buffer_view_;
	// Completer has a QObject parent and is thus automatically destroyed.
	// See #4758.
	// delete completer_;
}


GuiWorkArea::GuiWorkArea(QWidget * /* w */)
: d(new Private(this))
{
	new CompressorProxy(this); // not a leak
}


GuiWorkArea::GuiWorkArea(Buffer & buffer, GuiView & gv)
: d(new Private(this))
{
	new CompressorProxy(this); // not a leak
	setGuiView(gv);
	buffer.params().display_pixel_ratio = theGuiApp()->pixelRatio();
	setBuffer(buffer);
	init();
}


double GuiWorkArea::pixelRatio() const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	return devicePixelRatioF();
#else
	return devicePixelRatio();
#endif
}


void GuiWorkArea::init()
{
	// Setup the signals
	connect(&d->caret_timeout_, SIGNAL(timeout()), this, SLOT(toggleCaret()));
	connect(this, &GuiWorkArea::preeditChanged,
	        d->im_, &GuiInputMethod::processPreedit);
	connect(d->im_, &GuiInputMethod::preeditProcessed,
	        this, &GuiWorkArea::flagPreedit);
	connect(d->im_, &GuiInputMethod::queryProcessed,
	        this, &GuiWorkArea::receiveIMQueryResponse);

	// This connection is closed at the same time as this is destroyed.
	d->synthetic_mouse_event_.timeout.timeout.connect([this](){
			generateSyntheticMouseEvent();
		});

	d->resetScreen();

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setAcceptDrops(true);
	setMouseTracking(true);
	setMinimumSize(100, 70);
	setFrameStyle(QFrame::NoFrame);
	setInputMethodHints(Qt::ImhNone);
	updateWindowTitle();

	d->updateCursorShape();

	// we paint our own background
	viewport()->setAttribute(Qt::WA_OpaquePaintEvent);

	setFocusPolicy(Qt::StrongFocus);

	LYXERR(Debug::GUI, "viewport width: " << viewport()->width()
		<< "  viewport height: " << viewport()->height());

	// Enables input methods for asian languages.
	// Must be set when creating custom text editing widgets.
	setAttribute(Qt::WA_InputMethodEnabled, true);
}


GuiWorkArea::~GuiWorkArea()
{
	delete d;
}


void GuiWorkArea::Private::updateCursorShape()
{
	bool const clickable = buffer_view_ && buffer_view_->clickableInset();
	p->viewport()->setCursor(clickable ? Qt::PointingHandCursor
	                                   : Qt::IBeamCursor);
}


void GuiWorkArea::setGuiView(GuiView & gv)
{
	d->lyx_view_ = &gv;
}


void GuiWorkArea::setBuffer(Buffer & buffer)
{
	delete d->buffer_view_;
	d->buffer_view_ = new BufferView(buffer);
	setInputMethod();
	buffer.workAreaManager().add(this);

	// HACK: Prevents an additional redraw when the scrollbar pops up
	// which regularly happens on documents with more than one page.
	// The policy  should be set to "Qt::ScrollBarAsNeeded" soon.
	// Since we have no geometry information yet, we assume that
	// a document needs a scrollbar if there is more then four
	// paragraph in the outermost text.
	if (buffer.text().paragraphs().size() > 4)
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	QTimer::singleShot(50, this, SLOT(fixVerticalScrollBar()));
	Q_EMIT bufferViewChanged();
}


void GuiWorkArea::setInputMethod()
{
	d->im_ = new GuiInputMethod(this);

	// Notify BufferView about the input method so that TextMetics can know it
	d->buffer_view_->setInputMethod(d->im_);
}


GuiInputMethod * GuiWorkArea::inputMethod()
{
	return d->im_;
}


void GuiWorkArea::fixVerticalScrollBar()
{
	if (!isFullScreen())
		setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}


void GuiWorkArea::close()
{
	d->lyx_view_->removeWorkArea(this);
}


void GuiWorkArea::setFullScreen(bool full_screen)
{
	d->buffer_view_->setFullScreen(full_screen);

	queryInputItemGeometry();

	if (full_screen && lyxrc.full_screen_scrollbar)
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	else
		setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}


BufferView & GuiWorkArea::bufferView()
{
	return *d->buffer_view_;
}


BufferView const & GuiWorkArea::bufferView() const
{
	return *d->buffer_view_;
}


void GuiWorkArea::stopBlinkingCaret()
{
	d->caret_timeout_.stop();
	d->hideCaret();
}


void GuiWorkArea::startBlinkingCaret()
{
	// do not show the cursor if the view is busy
	if (view().busy())
		return;

	// Don't start blinking if the cursor isn't on screen, unless we
	// are not ready to know whether the cursor is on screen.
	if (!d->buffer_view_->busy() && !d->buffer_view_->caretInView())
		return;

	d->showCaret();

	// Avoid blinking when debugging PAINTING, since it creates too much noise
	if (!lyxerr.debugging(Debug::PAINTING)) {
		// we are not supposed to cache this value.
		int const time = QApplication::cursorFlashTime() / 2;
		if (time <= 0)
			return;
		d->caret_timeout_.setInterval(time);
		d->caret_timeout_.start();
	}
}


void GuiWorkArea::toggleCaret()
{
	if (d->caret_visible_)
		d->hideCaret();
	else
		d->showCaret();
}


void GuiWorkArea::scheduleRedraw(bool update_metrics)
{
	if (!isVisible() || view().busy())
		// No need to redraw in this case.
		return;

	// No need to do anything if this is the current view. The BufferView
	// metrics are already up to date.
	if (update_metrics || d->lyx_view_ != guiApp->currentView()
		|| d->lyx_view_->currentWorkArea() != this) {
		// FIXME: it would be nice to optimize for the off-screen case.
		d->buffer_view_->cursor().fixIfBroken();
		d->buffer_view_->updateMetrics();
		d->buffer_view_->cursor().fixIfBroken();
	}

	// update caret position, because otherwise it has to wait until
	// the blinking interval is over
	d->resetCaret();

	LYXERR(Debug::WORKAREA, "WorkArea::redraw screen");
	viewport()->update();

	/// FIXME: is this still true now that paintEvent does the actual painting?
	/// \warning: scrollbar updating *must* be done after the BufferView is drawn
	/// because \c BufferView::updateScrollbar() is called in \c BufferView::draw().
	d->updateScrollbar();
	d->lyx_view_->updateStatusBar();

	if (lyxerr.debugging(Debug::WORKAREA))
		d->buffer_view_->coordCache().dump();

	updateWindowTitle();

	d->updateCursorShape();
}


// Keep in sync with GuiWorkArea::processKeySym below
bool GuiWorkArea::queryKeySym(KeySymbol const & key, KeyModifier mod) const
{
	return guiApp->queryKeySym(key, mod);
}


// Keep in sync with GuiWorkArea::queryKeySym above
void GuiWorkArea::processKeySym(KeySymbol const & key, KeyModifier mod)
{
	if (d->lyx_view_->isFullScreen() && d->lyx_view_->menuBar()->isVisible()
		&& lyxrc.full_screen_menubar) {
		// FIXME HACK: we should not have to do this here. See related comment
		// in GuiView::event() (QEvent::ShortcutOverride)
		d->lyx_view_->menuBar()->hide();
	}

	// In order to avoid bad surprise in the middle of an operation,
	// we better stop the blinking caret...
	// the caret gets restarted in GuiView::restartCaret()
	stopBlinkingCaret();
	guiApp->processKeySym(key, mod);
}


void GuiWorkArea::Private::dispatch(FuncRequest const & cmd)
{
	// Handle drag&drop
	if (cmd.action() == LFUN_FILE_OPEN) {
		DispatchResult dr;
		lyx_view_->dispatch(cmd, dr);
		return;
	}

	bool const notJustMovingTheMouse =
		cmd.action() != LFUN_MOUSE_MOTION || cmd.button() != mouse_button::none;

	// In order to avoid bad surprise in the middle of an operation, we better stop
	// the blinking caret.
	if (notJustMovingTheMouse)
		p->stopBlinkingCaret();

	buffer_view_->mouseEventDispatch(cmd);

	// Skip these when selecting
	// FIXME: let GuiView take care of those.
	if (notJustMovingTheMouse && !buffer_view_->mouseSelecting()) {
		completer_->updateVisibility(false, false);
		lyx_view_->updateDialogs();
		lyx_view_->updateStatusBar();
		p->inputMethod()->toggleInputMethodAcceptance();
	}

	// GUI tweaks except with mouse motion with no button pressed.
	if (notJustMovingTheMouse) {
		// Slight hack: this is only called currently when we
		// clicked somewhere, so we force through the display
		// of the new status here.
		// FIXME: let GuiView take care of those.
		lyx_view_->clearMessage();

		// Show the caret immediately after any operation
		p->startBlinkingCaret();
	}

	updateCursorShape();
}


void GuiWorkArea::resizeBufferView()
{
	// WARNING: Please don't put any code that will trigger a repaint here!
	// We are already inside a paint event.
	stopBlinkingCaret();
	// Warn our container (GuiView).
	busy(true);

	bool const caret_in_view = d->buffer_view_->caretInView();
	// Change backing store size if needed
	d->resetScreen();
	// Do the actual work: recompute metrics
	d->buffer_view_->resize(viewport()->width(), viewport()->height());
	if (caret_in_view)
		d->buffer_view_->showCursor();
	d->resetCaret();

	// Update scrollbars which might have changed due different
	// BufferView dimension. This is especially important when the
	// BufferView goes from zero-size to the real-size for the first time,
	// as the scrollbar parameters are then set for the first time.
	d->updateScrollbar();

	d->need_resize_ = false;
	busy(false);
	// Eventually, restart the caret after the resize event.
	// We might be resizing even if the focus is on another widget so we only
	// restart the caret if we have the focus.
	if (hasFocus())
		QTimer::singleShot(50, this, SLOT(startBlinkingCaret()));
}


void GuiWorkArea::Private::resetCaret()
{
	// Don't start blinking if the cursor isn't on screen or the window
	// does not have focus
	if (!buffer_view_->caretInView() || !p->hasFocus())
		return;

	needs_caret_geometry_update_ = true;
	caret_visible_ = true;
}


void GuiWorkArea::Private::updateCaretGeometry()
{
	// we cannot update geometry if not ready and we do not need to if
	// caret is not in view.
	if (buffer_view_->busy() || !buffer_view_->caretInView())
		return;

	// completion indicator
	Cursor const & cur = buffer_view_->cursor();
	bool const completable = cur.inset().showCompletionCursor()
		&& completer_->completionAvailable()
		&& !completer_->popupVisible()
		&& !completer_->inlineVisible();

	buffer_view_->buildCaretGeometry(completable);

	needs_caret_geometry_update_ = false;
}


void GuiWorkArea::Private::showCaret()
{
	if (caret_visible_)
		return;

	resetCaret();
	p->viewport()->update();
}


void GuiWorkArea::Private::hideCaret()
{
	if (!caret_visible_)
		return;

	caret_visible_ = false;
	//if (!qApp->focusWidget())
		p->viewport()->update();
}


/* Draw the caret. Parameter \c horiz_offset is not 0 when there
 * has been horizontal scrolling in current row
 */
void GuiWorkArea::Private::drawCaret(QPainter & painter, int horiz_offset) const
{
	drawCaret(painter, horiz_offset, 0);
}

void GuiWorkArea::Private::drawCaret(QPainter & painter, int horiz_offset,
                                     int vert_offset) const
{
	if (buffer_view_->caretGeometry().shapes.empty())
		return;

	QColor const color = guiApp->colorCache().get(Color_cursor);
	painter.setPen(color);
	painter.setRenderHint(QPainter::Antialiasing, true);
	for (auto const & shape : buffer_view_->caretGeometry().shapes) {
		bool first = true;
		QPainterPath path;
		for (Point const & p : shape) {
			if (first) {
				path.moveTo(p.x - horiz_offset, p.y - vert_offset);
				first = false;
			} else
				path.lineTo(p.x - horiz_offset, p.y - vert_offset);
		}
		painter.fillPath(path, color);
	}
	painter.setRenderHint(QPainter::Antialiasing, false);
}


void GuiWorkArea::Private::updateScrollbar()
{
	// Prevent setRange() and setSliderPosition from causing recursive calls via
	// the signal valueChanged. (#10311)
	QObject::disconnect(p->verticalScrollBar(), SIGNAL(valueChanged(int)),
	                    p, SLOT(scrollTo(int)));
	ScrollbarParameters const & scroll = buffer_view_->scrollbarParameters();
	p->verticalScrollBar()->setRange(scroll.min, scroll.max);
	p->verticalScrollBar()->setPageStep(scroll.page_step);
	p->verticalScrollBar()->setSingleStep(scroll.single_step);
	p->verticalScrollBar()->setSliderPosition(0);
	// Connect to the vertical scroll bar
	QObject::connect(p->verticalScrollBar(), SIGNAL(valueChanged(int)),
	                 p, SLOT(scrollTo(int)));
}


void GuiWorkArea::scrollTo(int value)
{
	stopBlinkingCaret();
	d->buffer_view_->scrollDocView(value);

	if (lyxrc.cursor_follows_scrollbar) {
		d->buffer_view_->setCursorFromScrollbar();
		// FIXME: let GuiView take care of those.
		d->lyx_view_->updateLayoutList();
	}
	// Show the caret immediately after any operation.
	startBlinkingCaret();
}


bool GuiWorkArea::event(QEvent * e)
{
	switch (e->type()) {
	case QEvent::ToolTip: {
		QHelpEvent * helpEvent = static_cast<QHelpEvent *>(e);
		if (lyxrc.use_tooltip) {
			QPoint pos = helpEvent->pos();
			if (pos.x() < viewport()->width()) {
				QString s = toqstr(d->buffer_view_->toolTip(pos.x(), pos.y()));
				QToolTip::showText(helpEvent->globalPos(), formatToolTip(s,35));
			}
			else
				QToolTip::hideText();
		}
		// Don't forget to accept the event!
		e->accept();
		return true;
	}

	case QEvent::ShortcutOverride:
		// keyPressEvent is ShortcutOverride-aware and only accepts the event in
		// this case
		keyPressEvent(static_cast<QKeyEvent *>(e));
		return e->isAccepted();

	case QEvent::KeyPress: {
		// We catch this event in order to catch the Tab or Shift+Tab key press
		// which are otherwise reserved to focus switching between controls
		// within a dialog.
		QKeyEvent * ke = static_cast<QKeyEvent*>(e);
		if ((ke->key() == Qt::Key_Tab && ke->modifiers() == Qt::NoModifier)
			|| (ke->key() == Qt::Key_Backtab && (
				ke->modifiers() == Qt::ShiftModifier
				|| ke->modifiers() == Qt::NoModifier))) {
			keyPressEvent(ke);
			return true;
		}
		return QAbstractScrollArea::event(e);
	}

	default:
		return QAbstractScrollArea::event(e);
	}
	return false;
}


void GuiWorkArea::contextMenuEvent(QContextMenuEvent * e)
{
	string name;
	if (e->reason() == QContextMenuEvent::Mouse)
		// the menu name is set on mouse press
		name = d->context_menu_name_;
	else {
		QPoint pos = e->pos();
		Cursor const & cur = d->buffer_view_->cursor();
		if (e->reason() == QContextMenuEvent::Keyboard && cur.inTexted()) {
			// Do not access the context menu of math right in front of before
			// the cursor. This does not work when the cursor is in text.
			Inset * inset = cur.paragraph().getInset(cur.pos());
			if (inset && inset->asInsetMath())
				--pos.rx();
			else if (cur.pos() > 0) {
				inset = cur.paragraph().getInset(cur.pos() - 1);
				if (inset)
					++pos.rx();
			}
		}
		if (e->reason() == QContextMenuEvent::Keyboard)
			// Subtract the top margin, see #12811
			pos.setY(pos.y() - d->buffer_view_->topMargin());

		name = d->buffer_view_->contextMenu(pos.x(), pos.y());
	}

	if (name.empty()) {
		e->accept();
		return;
	}
	// always show mnemonics when the keyboard is used to show the context menu
	// FIXME: This should be fixed in Qt itself
	bool const keyboard = (e->reason() == QContextMenuEvent::Keyboard);
	QMenu * menu = guiApp->menus().menu(toqstr(name), *d->lyx_view_, keyboard);
	if (!menu) {
		e->accept();
		return;
	}
	// Position the menu to the right.
	// FIXME: menu position should be different for RTL text.
	menu->exec(e->globalPos());
	e->accept();
}


void GuiWorkArea::focusInEvent(QFocusEvent * e)
{
	LYXERR(Debug::DEBUG, "GuiWorkArea::focusInEvent(): " << this << " reason() = " << e->reason() << endl);
	if (d->lyx_view_->currentWorkArea() != this) {
		d->lyx_view_->setCurrentWorkArea(this);
		d->lyx_view_->currentWorkArea()->bufferView().buffer().updateBuffer();
	}

	// needs to reset IM item coordinates when focus in from dialogs or other apps
	if ((e->reason() == Qt::PopupFocusReason || e->reason() == Qt::ActiveWindowFocusReason) &&
		!(this->inDialogMode())) {
		// Switched from most of dialogs or other apps, and not on a dialog (e.g. findreplaceadv)
		d->item_geom_needs_reset_ = true;
	} else {
		// Switched from advanced search dialog or else (e.g. mouse event)
		d->item_geom_needs_reset_ = false;
	}

	startBlinkingCaret();
	QAbstractScrollArea::focusInEvent(e);
}


void GuiWorkArea::focusOutEvent(QFocusEvent * e)
{
	LYXERR(Debug::DEBUG, "GuiWorkArea::focusOutEvent(): " << this << endl);
	stopBlinkingCaret();
	QAbstractScrollArea::focusOutEvent(e);
}


void GuiWorkArea::mousePressEvent(QMouseEvent * e)
{
	if (d->dc_event_.active && d->dc_event_ == *e) {
		d->dc_event_.active = false;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		FuncRequest cmd(LFUN_MOUSE_TRIPLE, e->position().x(), e->position().y(),
#else
		FuncRequest cmd(LFUN_MOUSE_TRIPLE, e->x(), e->y(),
#endif
			q_button_state(e->button()), q_key_state(e->modifiers()));
		d->dispatch(cmd);
		e->accept();
		return;
	}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	FuncRequest const cmd(LFUN_MOUSE_PRESS, e->position().x(), e->position().y(),
#else
	FuncRequest const cmd(LFUN_MOUSE_PRESS, e->x(), e->y(),
#endif
			q_button_state(e->button()), q_key_state(e->modifiers()));
	d->dispatch(cmd);

	// Save the context menu on mouse press, because also the mouse
	// cursor is set on mouse press. Afterwards, we can either release
	// the mousebutton somewhere else, or the cursor might have moved
	// due to the DEPM. We need to do this after the mouse has been
	// set in dispatch(), because the selection state might change.
	if (e->button() == Qt::RightButton)
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		d->context_menu_name_ = d->buffer_view_->contextMenu(e->position().x(), e->position().y());
#else
		d->context_menu_name_ = d->buffer_view_->contextMenu(e->x(), e->y());
#endif

	e->accept();
}


void GuiWorkArea::mouseReleaseEvent(QMouseEvent * e)
{
	if (d->synthetic_mouse_event_.timeout.running())
		d->synthetic_mouse_event_.timeout.stop();

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	FuncRequest const cmd(LFUN_MOUSE_RELEASE, e->position().x(), e->position().y(),
#else
	FuncRequest const cmd(LFUN_MOUSE_RELEASE, e->x(), e->y(),
#endif
			q_button_state(e->button()), q_key_state(e->modifiers()));
#if (QT_VERSION > QT_VERSION_CHECK(5, 10, 1) && \
	QT_VERSION < QT_VERSION_CHECK(5, 15, 1))
	d->synthetic_mouse_event_.cmd = cmd; // QtBug QAbstractScrollArea::mouseMoveEvent
#endif
	d->dispatch(cmd);
	e->accept();
}


void GuiWorkArea::mouseMoveEvent(QMouseEvent * e)
{
#if (QT_VERSION > QT_VERSION_CHECK(5, 10, 1) && \
	QT_VERSION < QT_VERSION_CHECK(5, 15, 1))
	// cancel the event if the coordinates didn't change, this is due to QtBug
	// QAbstractScrollArea::mouseMoveEvent, the event is triggered falsely when quickly
	// double tapping a touchpad. To test: try to select a word by quickly double tapping
	// on a touchpad while hovering the cursor over that word in the work area.
	// Only Windows seems to be affected.
	// ML thread: https://www.mail-archive.com/lyx-devel@lists.lyx.org/msg211699.html
	// Qt bugtracker: https://bugreports.qt.io/browse/QTBUG-85431
	// Bug was fixed in Qt 5.15.1
	if (e->x() == d->synthetic_mouse_event_.cmd.x() && // QtBug QAbstractScrollArea::mouseMoveEvent
			e->y() == d->synthetic_mouse_event_.cmd.y()) // QtBug QAbstractScrollArea::mouseMoveEvent
		return; // QtBug QAbstractScrollArea::mouseMoveEvent
#endif

	// we kill the triple click if we move
	doubleClickTimeout();
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	FuncRequest cmd(LFUN_MOUSE_MOTION, e->position().x(), e->position().y(),
#else
	FuncRequest cmd(LFUN_MOUSE_MOTION, e->x(), e->y(),
#endif
			q_motion_state(e->buttons()), q_key_state(e->modifiers()));

	e->accept();

	// If we're above or below the work area...
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	if ((e->position().y() <= 20 || e->position().y() >= viewport()->height() - 20)
#else
	if ((e->y() <= 20 || e->y() >= viewport()->height() - 20)
#endif
			&& e->buttons() == mouse_button::button1) {
		// Make sure only a synthetic event can cause a page scroll,
		// so they come at a steady rate:
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
		if (e->position().y() <= 20)
			// _Force_ a scroll up:
			cmd.set_y(e->position().y() - 21);
		else
			cmd.set_y(e->position().y() + 21);
#else
		if (e->y() <= 20)
			// _Force_ a scroll up:
			cmd.set_y(e->y() - 21);
		else
			cmd.set_y(e->y() + 21);
#endif
		// Store the event, to be handled when the timeout expires.
		d->synthetic_mouse_event_.cmd = cmd;

		if (d->synthetic_mouse_event_.timeout.running()) {
			// Discard the event. Note that it _may_ be handled
			// when the timeout expires if
			// synthetic_mouse_event_.cmd has not been overwritten.
			// Ie, when the timeout expires, we handle the
			// most recent event but discard all others that
			// occurred after the one used to start the timeout
			// in the first place.
			return;
		}

		d->synthetic_mouse_event_.restart_timeout = true;
		d->synthetic_mouse_event_.timeout.start();
		// Fall through to handle this event...

	} else if (d->synthetic_mouse_event_.timeout.running()) {
		// Store the event, to be possibly handled when the timeout
		// expires.
		// Once the timeout has expired, normal control is returned
		// to mouseMoveEvent (restart_timeout = false).
		// This results in a much smoother 'feel' when moving the
		// mouse back into the work area.
		d->synthetic_mouse_event_.cmd = cmd;
		d->synthetic_mouse_event_.restart_timeout = false;
		return;
	}
	d->dispatch(cmd);
}


void GuiWorkArea::wheelEvent(QWheelEvent * ev)
{
	// Wheel rotation by one notch results in a delta() of 120 (see
	// documentation of QWheelEvent)
	// But first we have to ignore horizontal scroll events.
	QPoint const aDelta = ev->angleDelta();
	// skip horizontal wheel event
	if (abs(aDelta.x()) > abs(aDelta.y())) {
		ev->accept();
		return;
	}
	double const delta = aDelta.y() / 120.0;

	bool zoom = false;
	switch (lyxrc.scroll_wheel_zoom) {
	case LyXRC::SCROLL_WHEEL_ZOOM_CTRL:
		zoom = ev->modifiers() & Qt::ControlModifier;
		zoom &= !(ev->modifiers() & (Qt::ShiftModifier | Qt::AltModifier));
		break;
	case LyXRC::SCROLL_WHEEL_ZOOM_SHIFT:
		zoom = ev->modifiers() & Qt::ShiftModifier;
		zoom &= !(ev->modifiers() & (Qt::ControlModifier | Qt::AltModifier));
		break;
	case LyXRC::SCROLL_WHEEL_ZOOM_ALT:
		zoom = ev->modifiers() & Qt::AltModifier;
		zoom &= !(ev->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier));
		break;
	case LyXRC::SCROLL_WHEEL_ZOOM_OFF:
		break;
	}
	if (zoom) {
		docstring arg = convert<docstring>(int(5 * delta));
		lyx::dispatch(FuncRequest(LFUN_BUFFER_ZOOM_IN, arg));
		return;
	}

	// Take into account the desktop wide settings.
	int const lines = qApp->wheelScrollLines();
	int const page_step = verticalScrollBar()->pageStep();
	// Test if the wheel mouse is set to one screen at a time.
	// This is according to
	// https://doc.qt.io/qt-5/qapplication.html#wheelScrollLines-prop
	int scroll_value =
		min(lines * verticalScrollBar()->singleStep(), page_step);

	// Take into account the rotation and the user preferences.
	scroll_value = int(scroll_value * delta * lyxrc.mouse_wheel_speed);
	LYXERR(Debug::SCROLLING, "wheelScrollLines = " << lines
			<< " delta = " << delta << " scroll_value = " << scroll_value
			<< " page_step = " << page_step);
	// Now scroll.
	verticalScrollBar()->setValue(verticalScrollBar()->value() - scroll_value);

	ev->accept();
}


void GuiWorkArea::generateSyntheticMouseEvent()
{
	int const e_y = d->synthetic_mouse_event_.cmd.y();
	int const wh = d->buffer_view_->workHeight();
	bool const up = e_y < 0;
	bool const down = e_y > wh;

	// Set things off to generate the _next_ 'pseudo' event.
	int step = 50;
	if (d->synthetic_mouse_event_.restart_timeout) {
		// This is some magic formulae to determine the speed
		// of scrolling related to the position of the mouse.
		int time = 200;
		if (up || down) {
			int dist = up ? -e_y : e_y - wh;
			time = max(min(200, 250000 / (dist * dist)), 1) ;

			if (time < 40) {
				step = 80000 / (time * time);
				time = 40;
			}
			step /= 8;
			time /= 8;
		}
		d->synthetic_mouse_event_.timeout.setTimeout(time);
		d->synthetic_mouse_event_.timeout.start();
	}

	// Can we scroll further ?
	int const value = verticalScrollBar()->value();
	if (value == verticalScrollBar()->maximum()
		  || value == verticalScrollBar()->minimum()) {
		d->synthetic_mouse_event_.timeout.stop();
		return;
	}

	// Scroll
	if (step <= 2 * wh) {
		d->buffer_view_->scroll(up ? -step : step);
		d->buffer_view_->processUpdateFlags(Update::ForceDraw);
	} else {
		d->buffer_view_->scrollDocView(value + (up ? -step : step));
	}

	// In which paragraph do we have to set the cursor ?
	Cursor & cur = d->buffer_view_->cursor();
	// FIXME: we don't know how to handle math.
	Text * text = cur.text();
	if (!text)
		return;
	TextMetrics & tm = d->buffer_view_->textMetrics(text);

	// Quit gracefully if there are no metrics, since otherwise next
	// line would crash (bug #10324).
	// This situation seems related to a (not yet understood) timing problem.
	if (tm.empty())
		return;

	const int y = up ? 0 : wh - defaultRowHeight();
	tm.setCursorFromCoordinates(cur, d->synthetic_mouse_event_.cmd.x(), y);

	d->buffer_view_->buffer().changed(false);
}


// CompressorProxy adapted from Kuba Ober https://stackoverflow.com/a/21006207
CompressorProxy::CompressorProxy(GuiWorkArea * wa) : QObject(wa), flag_(false)
{
	qRegisterMetaType<KeySymbol>("KeySymbol");
	qRegisterMetaType<KeyModifier>("KeyModifier");
	connect(wa, SIGNAL(compressKeySym(KeySymbol, KeyModifier, bool)),
		this, SLOT(slot(KeySymbol, KeyModifier, bool)),
	        Qt::QueuedConnection);
	connect(this, SIGNAL(signal(KeySymbol, KeyModifier)),
		wa, SLOT(processKeySym(KeySymbol, KeyModifier)));
}


bool CompressorProxy::emitCheck(bool isAutoRepeat)
{
	flag_ = true;
	if (isAutoRepeat)
		QCoreApplication::sendPostedEvents(this, QEvent::MetaCall); // recurse
	bool result = flag_;
	flag_ = false;
	return result;
}


void CompressorProxy::slot(KeySymbol const & sym, KeyModifier mod, bool isAutoRepeat)
{
	if (emitCheck(isAutoRepeat))
		Q_EMIT signal(sym, mod);
	else
		LYXERR(Debug::KEY, "system is busy: autoRepeat key event ignored");
}


void GuiWorkArea::keyPressEvent(QKeyEvent * ev)
{
	// this is also called for ShortcutOverride events. In this case, one must
	// not act but simply accept the event explicitly.
	bool const act = (ev->type() != QEvent::ShortcutOverride);

	// Do not process here some keys if dialog_mode_ is set
	bool const for_dialog_mode = d->dialog_mode_
		&& (ev->modifiers() == Qt::NoModifier
		    || ev->modifiers() == Qt::ShiftModifier)
		&& (ev->key() == Qt::Key_Escape
		    || ev->key() == Qt::Key_Enter
		    || ev->key() == Qt::Key_Return);
	// also do not use autoRepeat to input shortcuts
	bool const autoRepeat = ev->isAutoRepeat();

	if (for_dialog_mode || (!act && autoRepeat)) {
		ev->ignore();
		return;
	}

	// intercept some keys if completion popup is visible
	if (d->completer_->popupVisible()) {
		switch (ev->key()) {
		case Qt::Key_Enter:
		case Qt::Key_Return:
			if (act)
				d->completer_->activate();
			ev->accept();
			return;
		}
	}

	KeyModifier const m = q_key_state(ev->modifiers());

	if (act && lyxerr.debugging(Debug::KEY)) {
		std::string str;
		if (m & ShiftModifier)
			str += "Shift-";
		if (m & ControlModifier)
			str += "Control-";
		if (m & AltModifier)
			str += "Alt-";
		if (m & MetaModifier)
			str += "Meta-";
		LYXERR(Debug::KEY, " count: " << ev->count() << " text: " << ev->text()
		       << " isAutoRepeat: " << ev->isAutoRepeat() << " key: " << ev->key()
		       << " keyState: " << str);
	}

	KeySymbol sym;
	setKeySymbol(&sym, ev);
	if (sym.isOK()) {
		if (act) {
			Q_EMIT compressKeySym(sym, m, autoRepeat);
			ev->accept();
		} else
			// here, !autoRepeat, as determined at the beginning
			ev->setAccepted(queryKeySym(sym, m));
	} else {
		ev->ignore();
	}
}


void GuiWorkArea::doubleClickTimeout()
{
	d->dc_event_.active = false;
}


void GuiWorkArea::mouseDoubleClickEvent(QMouseEvent * ev)
{
	d->dc_event_ = DoubleClick(ev);
	QTimer::singleShot(QApplication::doubleClickInterval(), this,
			SLOT(doubleClickTimeout()));
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	FuncRequest cmd(LFUN_MOUSE_DOUBLE, ev->position().x(), ev->position().y(),
#else
	FuncRequest cmd(LFUN_MOUSE_DOUBLE, ev->x(), ev->y(),
#endif
			q_button_state(ev->button()), q_key_state(ev->modifiers()));
	d->dispatch(cmd);
	ev->accept();
}


void GuiWorkArea::resizeEvent(QResizeEvent * ev)
{
	QAbstractScrollArea::resizeEvent(ev);
	d->need_resize_ = true;
	ev->accept();
}


void GuiWorkArea::queryInputItemGeometry()
{
	LYXERR(
		Debug::DEBUG,
		"item_rect_  is aquired:  x() = " << d->item_rect_.x() <<
			" -> " << d->sys_im_->inputItemRectangle().x() <<
			",  y() = " << d->item_rect_.y() <<
			" -> " << d->sys_im_->inputItemRectangle().y() <<
			", width() = " << d->item_rect_.width() <<
			" -> " << d->sys_im_->inputItemRectangle().width() <<
			", height() = " << d->item_rect_.height() <<
			" -> " << d->sys_im_->inputItemRectangle().height()
	);

	d->item_rect_  = d->sys_im_->inputItemRectangle();

#if defined(Q_OS_LINUX) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	const QPointF origin(0,0);
	QPointF global_coord = mapToGlobal(origin);
	QTransform qt;
	qt.translate(global_coord.x(), global_coord.y());
	LYXERR(
	   Debug::DEBUG,
	   "item_trans_ is aquired: dx() = " <<
	            d->item_trans_.dx() << " -> " << qt.dx() <<
	            ", dy() = " << d->item_trans_.dy() << " -> " << qt.dy()
	);
	d->item_trans_ = qt;
#else
	LYXERR(
	   Debug::DEBUG,
	   "item_trans_ is aquired: dx() = " << d->item_trans_.dx() <<
	   " -> " << d->sys_im_->inputItemTransform().dx() <<
	   ", dy() = " << d->item_trans_.dy() <<
	   " -> " << d->sys_im_->inputItemTransform().dy()
	);
	d->item_trans_ = d->sys_im_->inputItemTransform();
#endif

	// save coordinates of the base working area for later use necessary to
	// creat new GuiViews
	if (guiApp->isFirstWorkArea()) {
		guiApp->setBaseInputItemRectangle(d->item_rect_);
		guiApp->setBaseInputItemTransform(d->item_trans_);
		guiApp->firstWorkAreaDone();

		LYXERR(
			Debug::DEBUG,
			"base inputItemRectangle x = " <<
					guiApp->baseInputItemRectangle().x() <<
			",  y = " << guiApp->baseInputItemRectangle().y() <<
			", width = " << guiApp->baseInputItemRectangle().width() <<
			", height = " << guiApp->baseInputItemRectangle().height()
		);
		LYXERR(
			Debug::DEBUG,
			"base inputItemTransform dx = " <<
					guiApp->baseInputItemTransform().dx() <<
			",  dy = " << guiApp->baseInputItemTransform().dy()
		);
	}
}


void GuiWorkArea::resetInputItemGeometry()
{
	resetInputItemGeometry(false);
}

void GuiWorkArea::resetInputItemGeometry(bool is_new_view)
{
	if (d->item_geom_needs_reset_) {
		if (is_new_view) {
			LYXERR(
				Debug::DEBUG,
				"QInputMethod::inputItemRectangle is reset: x = " <<
					d->sys_im_->inputItemRectangle().x() <<
					" -> " << guiApp->baseInputItemRectangle().x() <<
					",  y = " << d->sys_im_->inputItemRectangle().y() <<
					" -> " << guiApp->baseInputItemRectangle().y() <<
					", width = " << d->sys_im_->inputItemRectangle().width() <<
					" -> " << guiApp->baseInputItemRectangle().width() <<
					", height = " << d->sys_im_->inputItemRectangle().height() <<
					" -> " << guiApp->baseInputItemRectangle().height()
			);
			LYXERR(
				Debug::DEBUG,
				"QInputMethod::inputItemTransform is reset: dx = " <<
					d->sys_im_->inputItemTransform().dx() <<
					" -> " << guiApp->baseInputItemTransform().dx() <<
					", dy = " << d->sys_im_->inputItemTransform().dy() <<
					" -> " << guiApp->baseInputItemTransform().dy()
			);
			d->sys_im_->setInputItemRectangle(guiApp->baseInputItemRectangle());
			d->sys_im_->setInputItemTransform(guiApp->baseInputItemTransform());
		} else {
			LYXERR(
				Debug::DEBUG,
				"QInputMethod::inputItemRectangle is reset:  x = " <<
					d->sys_im_->inputItemRectangle().x() <<
					" -> " << d->item_rect_.x() <<
					",  y = " << d->sys_im_->inputItemRectangle().y() <<
					" -> " << d->item_rect_.y() <<
					", width = " << d->sys_im_->inputItemRectangle().width() <<
					" -> " << d->item_rect_.width() <<
					", height = " << d->sys_im_->inputItemRectangle().height() <<
					" -> " << d->item_rect_.height()
			);
			LYXERR(
				Debug::DEBUG,
				"QInputMethod::inputItemTransform is reset: dx = " <<
					d->sys_im_->inputItemTransform().dx() <<
					" -> " << d->item_trans_.dx() <<
					", dy = " << d->sys_im_->inputItemTransform().dy() <<
					" -> " << d->item_trans_.dy()
			);
			d->sys_im_->setInputItemRectangle(d->item_rect_);
			d->sys_im_->setInputItemTransform(d->item_trans_);
		}
		d->item_geom_needs_reset_ = false;
	}
}


void GuiWorkArea::Private::resetScreen()
{
	if (use_backingstore_) {
		double const pr = p->pixelRatio();
		screen_ = QImage(int(pr * p->viewport()->width()),
		                 int(pr * p->viewport()->height()),
		                 QImage::Format_ARGB32_Premultiplied);
		screen_.setDevicePixelRatio(pr);
	}
}


QPaintDevice * GuiWorkArea::Private::screenDevice()
{
	if (use_backingstore_)
		return &screen_;
	else
		return p->viewport();
}


void GuiWorkArea::Private::updateScreen(QRectF const & rc)
{
	if (use_backingstore_) {
		QPainter qpain(p->viewport());
		double const pr = p->pixelRatio();
		QRectF const rcs = QRectF(rc.x() * pr, rc.y() * pr,
		                          rc.width() * pr, rc.height() * pr);
		qpain.drawImage(rc, screen_, rcs);
	}
}


void GuiWorkArea::paintEvent(QPaintEvent * ev)
{
	// Do not trigger the painting machinery if we are not ready (see
	// bug #10989). The second test triggers when in the middle of a
	// dispatch operation.
	if (view().busy() || d->buffer_view_->busy()) {
		// Since the screen may have turned black at this point, our
		// backing store has to be copied to screen. This is a no-op
		// except when our drawing strategy is "backingstore" (macOS,
		// Wayland, or set in prefs).
		d->updateScreen(ev->rect());
		// Ignore this paint event, but request a new one for later.
		viewport()->update(ev->rect());
		ev->accept();
		return;
	}

	// LYXERR(Debug::PAINTING, "paintEvent begin: x: " << rc.x()
	//	<< " y: " << rc.y() << " w: " << rc.width() << " h: " << rc.height());

	if (d->need_resize_ || pixelRatio() != d->last_pixel_ratio_)
		resizeBufferView();

	d->last_pixel_ratio_ = pixelRatio();

	GuiPainter pain(d->screenDevice(), pixelRatio(), d->lyx_view_->develMode());

	d->buffer_view_->draw(pain, d->caret_visible_);

	// draw the caret
	// FIXME: the code would be a little bit simpler if caret geometry
	// was updated unconditionally. Some profiling is required to see
	// how expensive this is (especially when idle).
	if ((d->im_->preeditString().empty() && d->caret_visible_) ||
	        (!d->im_->preeditString().empty() && d->im_->isCaretVisible())) {
		if (d->needs_caret_geometry_update_)
			d->updateCaretGeometry();
		d->drawCaret(pain,
		             d->buffer_view_->horizScrollOffset()
		             - d->im_->preeditCaretOffset()[0],
		             - d->im_->preeditCaretOffset()[1]);
	}

	d->updateScreen(ev->rect());

	ev->accept();
}


void GuiWorkArea::inputMethodEvent(QInputMethodEvent * ev)
{
    Q_EMIT preeditChanged(ev);
}


QVariant GuiWorkArea::inputMethodQuery(Qt::InputMethodQuery query) const
{
	// ask a query
	d->im_->processQuery(query);

	// wait for the response and return
	clock_t start = clock();
	while (!d->im_query_responded_) {
		// time out in one second
		if ((clock() - start)/CLOCKS_PER_SEC >= 1) break;
	}
	d->im_query_responded_ = false;

	return d->im_query_response_;
}


void GuiWorkArea::updateWindowTitle()
{
	Buffer const & buf = bufferView().buffer();
	if (buf.fileName() != d->file_name_
	    || buf.params().shell_escape != d->shell_escape_
	    || buf.hasReadonlyFlag() != d->read_only_
	    || buf.lyxvc().vcstatus() != d->vc_status_
	    || buf.isClean() != d->clean_
	    || buf.notifiesExternalModification() != d->externally_modified_) {
		d->file_name_ = buf.fileName();
		d->shell_escape_ = buf.params().shell_escape;
		d->read_only_ = buf.hasReadonlyFlag();
		d->vc_status_ = buf.lyxvc().vcstatus();
		d->clean_ = buf.isClean();
		d->externally_modified_ = buf.notifiesExternalModification();
		Q_EMIT titleChanged(this);
	}
}


bool GuiWorkArea::isFullScreen() const
{
	return d->lyx_view_ && d->lyx_view_->isFullScreen();
}


bool GuiWorkArea::inDialogMode() const
{
	return d->dialog_mode_;
}


void GuiWorkArea::setDialogMode(bool mode)
{
	d->dialog_mode_ = mode;
}


void GuiWorkArea::flagPreedit(QInputMethodEvent* ev)
{
	d->buffer_view_->processUpdateFlags(Update::Force);
	ev->accept();
}


void GuiWorkArea::receiveIMQueryResponse(QVariant response) {
	d->im_query_response_ = response;
	// notify the response
	d->im_query_responded_ = true;
}

GuiCompleter & GuiWorkArea::completer()
{
	return *d->completer_;
}

GuiView const & GuiWorkArea::view() const
{
	return *d->lyx_view_;
}


GuiView & GuiWorkArea::view()
{
	return *d->lyx_view_;
}

////////////////////////////////////////////////////////////////////
//
// EmbeddedWorkArea
//
////////////////////////////////////////////////////////////////////


EmbeddedWorkArea::EmbeddedWorkArea(QWidget * w): GuiWorkArea(w)
{
	support::TempFile tempfile("embedded.internal");
	tempfile.setAutoRemove(false);
	buffer_ = theBufferList().newInternalBuffer(tempfile.name().absFileName());
	buffer_->setUnnamed(true);
	buffer_->setFullyLoaded(true);
	setBuffer(*buffer_);
	setDialogMode(true);
}


EmbeddedWorkArea::~EmbeddedWorkArea()
{
	// No need to destroy buffer and bufferview here, because it is done
	// in theBufferList() destruction loop at application exit
}


void EmbeddedWorkArea::closeEvent(QCloseEvent * ev)
{
	disable();
	GuiWorkArea::closeEvent(ev);
}


void EmbeddedWorkArea::hideEvent(QHideEvent * ev)
{
	disable();
	GuiWorkArea::hideEvent(ev);
}


QSize EmbeddedWorkArea::sizeHint () const
{
	// FIXME(?):
	// GuiWorkArea sets the size to the screen's viewport
	// by returning a value this gets overridden
	// EmbeddedWorkArea is now sized to fit in the layout
	// of the parent, and has a minimum size set in GuiWorkArea
	// which is what we return here
	return QSize(100, 70);
}


void EmbeddedWorkArea::disable()
{
	stopBlinkingCaret();
	if (view().currentWorkArea() != this)
		return;
	// No problem if currentMainWorkArea() is 0 (setCurrentWorkArea()
	// tolerates it and shows the background logo), what happens if
	// an EmbeddedWorkArea is closed after closing all document WAs
	view().setCurrentWorkArea(view().currentMainWorkArea());
}

////////////////////////////////////////////////////////////////////
//
// TabWorkArea
//
////////////////////////////////////////////////////////////////////

TabWorkArea::TabWorkArea(QWidget * parent)
	: QTabWidget(parent), clicked_tab_(-1), midpressed_tab_(-1)
{
	QPalette pal = palette();
	pal.setColor(QPalette::Active, QPalette::Button,
		pal.color(QPalette::Active, QPalette::Window));
	pal.setColor(QPalette::Disabled, QPalette::Button,
		pal.color(QPalette::Disabled, QPalette::Window));
	pal.setColor(QPalette::Inactive, QPalette::Button,
		pal.color(QPalette::Inactive, QPalette::Window));

	QObject::connect(this, SIGNAL(currentChanged(int)),
		this, SLOT(on_currentTabChanged(int)));
	// Fix for #11835
	QObject::connect(this, SIGNAL(tabBarClicked(int)),
		this, SLOT(on_tabBarClicked(int)));

	closeBufferButton = new QToolButton(this);
	closeBufferButton->setPalette(pal);
	// FIXME: rename the icon to closebuffer.png
	closeBufferButton->setIcon(QIcon(getPixmap("images/", "closetab", "svgz,png")));
	closeBufferButton->setText("Close File");
	closeBufferButton->setAutoRaise(true);
	closeBufferButton->setCursor(Qt::ArrowCursor);
	closeBufferButton->setToolTip(qt_("Close File"));
	closeBufferButton->setEnabled(true);
	QObject::connect(closeBufferButton, SIGNAL(clicked()),
		this, SLOT(closeCurrentBuffer()));
	setCornerWidget(closeBufferButton, Qt::TopRightCorner);

	// set TabBar behaviour
	QTabBar * tb = tabBar();
	tb->setTabsClosable(!lyxrc.single_close_tab_button);
	tb->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
	tb->setElideMode(Qt::ElideNone);
	// allow dragging tabs
	tb->setMovable(true);
	// make us responsible for the context menu of the tabbar
	tb->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(tb, SIGNAL(customContextMenuRequested(const QPoint &)),
	        this, SLOT(showContextMenu(const QPoint &)));
	connect(tb, SIGNAL(tabCloseRequested(int)),
	        this, SLOT(closeTab(int)));

	setUsesScrollButtons(true);
#ifdef Q_OS_MAC
	// use document mode tabs
	setDocumentMode(true);
#endif
}


void TabWorkArea::mousePressEvent(QMouseEvent *me)
{
	if (me->button() == Qt::MiddleButton)
		midpressed_tab_ = tabBar()->tabAt(me->pos());
	else
		QTabWidget::mousePressEvent(me);
}


void TabWorkArea::mouseReleaseEvent(QMouseEvent *me)
{
	if (me->button() == Qt::MiddleButton) {
		int const midreleased_tab = tabBar()->tabAt(me->pos());
		if (midpressed_tab_ == midreleased_tab && posIsTab(me->pos()))
			closeTab(midreleased_tab);
	} else
		QTabWidget::mouseReleaseEvent(me);
}


void TabWorkArea::paintEvent(QPaintEvent * event)
{
	if (tabBar()->isVisible()) {
		QTabWidget::paintEvent(event);
	} else {
		// Prevent the selected tab to influence the
		// painting of the frame of the tab widget.
		// This is needed for gtk style in Qt.
		QStylePainter p(this);
		QStyleOptionTabWidgetFrame opt;
		initStyleOption(&opt);
		opt.rect = style()->subElementRect(QStyle::SE_TabWidgetTabPane,
			&opt, this);
		opt.selectedTabRect = QRect();
		p.drawPrimitive(QStyle::PE_FrameTabWidget, opt);
	}
}


bool TabWorkArea::posIsTab(QPoint position)
{
	// tabAt returns -1 if tab does not covers position
	return tabBar()->tabAt(position) > -1;
}


void TabWorkArea::mouseDoubleClickEvent(QMouseEvent * event)
{
	if (event->button() != Qt::LeftButton)
		return;

	// this code chunk is unnecessary because it seems the event only makes
	// it this far if it is not on a tab. I'm not sure why this is (maybe
	// it is handled and ended in DragTabBar?), and thus I'm not sure if
	// this is true in all cases and if it will be true in the future so I
	// leave this code for now. (skostysh, 2016-07-21)
	//
	// return early if double click on existing tabs
	if (posIsTab(event->pos()))
		return;

	dispatch(FuncRequest(LFUN_BUFFER_NEW));
}


void TabWorkArea::setFullScreen(bool full_screen)
{
	for (int i = 0; i != count(); ++i) {
		if (GuiWorkArea * wa = workArea(i))
			wa->setFullScreen(full_screen);
	}

	if (lyxrc.full_screen_tabbar)
		showBar(!full_screen && count() > 1);
	else
		showBar(count() > 1);
}


void TabWorkArea::showBar(bool show)
{
	tabBar()->setEnabled(show);
	tabBar()->setVisible(show);
	if (documentMode()) {
		// avoid blank corner widget when documentMode(true) is used
		if (show && lyxrc.single_close_tab_button) {
			setCornerWidget(closeBufferButton, Qt::TopRightCorner);
			closeBufferButton->setVisible(true);
		} else
			// remove corner widget
			setCornerWidget(nullptr);
	} else
		closeBufferButton->setVisible(show && lyxrc.single_close_tab_button);
	setTabsClosable(!lyxrc.single_close_tab_button);
}


GuiWorkAreaContainer * TabWorkArea::widget(int index) const
{
	QWidget * w = QTabWidget::widget(index);
	if (!w)
		return nullptr;
	GuiWorkAreaContainer * wac = dynamic_cast<GuiWorkAreaContainer *>(w);
	LATTEST(wac);
	return wac;
}


GuiWorkAreaContainer * TabWorkArea::currentWidget() const
{
	return widget(currentIndex());
}


GuiWorkArea * TabWorkArea::workArea(int index) const
{
	GuiWorkAreaContainer * w = widget(index);
	if (!w)
		return nullptr;
	return w->workArea();
}


GuiWorkArea * TabWorkArea::currentWorkArea() const
{
	return workArea(currentIndex());
}


GuiWorkArea * TabWorkArea::workArea(Buffer & buffer) const
{
	// FIXME: this method doesn't work if we have more than one work area
	// showing the same buffer.
	for (int i = 0; i != count(); ++i) {
		GuiWorkArea * wa = workArea(i);
		LASSERT(wa, return nullptr);
		if (&wa->bufferView().buffer() == &buffer)
			return wa;
	}
	return nullptr;
}


void TabWorkArea::closeAll()
{
	while (count()) {
		QWidget * wac = widget(0);
		LASSERT(wac, return);
		removeTab(0);
		delete wac;
	}
}


int TabWorkArea::indexOfWorkArea(GuiWorkArea * w) const
{
	for (int index = 0; index < count(); ++index)
		if (workArea(index) == w)
			return index;
	return -1;
}


bool TabWorkArea::setCurrentWorkArea(GuiWorkArea * work_area)
{
	LASSERT(work_area, return false);
	int index = indexOfWorkArea(work_area);
	if (index == -1)
		return false;

	if (index == currentIndex())
		// Make sure the work area is up to date.
		on_currentTabChanged(index);
	else
		// Switch to the work area.
		setCurrentIndex(index);
	work_area->setFocus(Qt::OtherFocusReason);

	return true;
}


GuiWorkArea * TabWorkArea::addWorkArea(Buffer & buffer, GuiView & view)
{
	view.setBusy(true);
	GuiWorkArea * wa = new GuiWorkArea(buffer, view);
	GuiWorkAreaContainer * wac = new GuiWorkAreaContainer(wa);
	wa->setUpdatesEnabled(false);
	// Hide tabbar if there's no tab (avoid a resize and a flashing tabbar
	// when hiding it again below).
	if (!(currentWorkArea() && currentWorkArea()->isFullScreen()))
		showBar(count() > 0);
	addTab(wac, wa->windowTitle());
	QObject::connect(wa, SIGNAL(titleChanged(GuiWorkArea *)),
		this, SLOT(updateTabTexts()));
	if (currentWorkArea() && currentWorkArea()->isFullScreen())
		setFullScreen(true);
	else
		// Hide tabbar if there's only one tab.
		showBar(count() > 1);

	updateTabTexts();

	// obtain new input item coordinates in the new and old work areas
	wa->queryInputItemGeometry();
	if (currentWorkArea())
		currentWorkArea()->queryInputItemGeometry();

	view.setBusy(false);

	return wa;
}


bool TabWorkArea::removeWorkArea(GuiWorkArea * work_area)
{
	LASSERT(work_area, return false);
	int index = indexOfWorkArea(work_area);
	if (index == -1)
		return false;

	work_area->setUpdatesEnabled(false);
	QWidget * wac = widget(index);
	removeTab(index);
	delete wac;

	if (count()) {
		// make sure the next work area is enabled.
		currentWidget()->setUpdatesEnabled(true);
		if (currentWorkArea() && currentWorkArea()->isFullScreen())
			setFullScreen(true);
		else
			// Show tabbar only if there's more than one tab.
			showBar(count() > 1);
		currentWorkArea()->queryInputItemGeometry();
	} else
		lastWorkAreaRemoved();

	updateTabTexts();

	return true;
}


void TabWorkArea::on_tabBarClicked(int i)
{
	// returns e.g. on application destruction
	if (i == -1)
		return;

	// if we click on a tab in a different TabWorkArea,
	// focus needs to be set (#11835)
	if (currentWorkArea() && currentWorkArea()->view().currentTabWorkArea() != this) {
		GuiWorkArea * wa = workArea(i);
		LASSERT(wa, return);
		wa->setFocus();
	}
}


void TabWorkArea::on_currentTabChanged(int i)
{
	// returns e.g. on application destruction
	if (i == -1)
		return;
	GuiWorkArea * wa = workArea(i);
	LASSERT(wa, return);
	wa->setUpdatesEnabled(true);
	wa->scheduleRedraw(true);
	wa->setFocus();
	// if the work area did change,
	// inform the view and dialogs
	if (wa == currentWorkArea())
		currentWorkAreaChanged(wa);

	LYXERR(Debug::GUI, "currentTabChanged " << i
		<< " File: " << wa->bufferView().buffer().absFileName());
}


void TabWorkArea::closeCurrentBuffer()
{
	GuiWorkArea * wa;
	if (clicked_tab_ == -1)
		wa = currentWorkArea();
	else {
		wa = workArea(clicked_tab_);
		LASSERT(wa, return);
	}
	wa->view().closeWorkArea(wa);
}


bool TabWorkArea::closeTabsToRight()
{
	if (clicked_tab_ == -1)
		return false;

	int const initialCurrentIndex = currentIndex();

	while (count() - 1 > clicked_tab_) {
		GuiWorkArea * wa = workArea(count() - 1);
		LASSERT(wa, return false);
		if (!wa->view().closeWorkArea(wa)) {
			// closing cancelled, if possible, reset initial current tab index
			if (initialCurrentIndex < count())
				setCurrentIndex(initialCurrentIndex);
			else
				setCurrentIndex(clicked_tab_);
			return false;
		}
	}
	return true;
}


bool TabWorkArea::openEnclosingDirectory()
{

	if (clicked_tab_ == -1)
		return false;

	Buffer const & buf = workArea(clicked_tab_)->bufferView().buffer();
	showDirectory(buf.fileName().onlyPath());
	return true;
}


bool TabWorkArea::closeTabsToLeft()
{
	if (clicked_tab_ == -1)
		return false;

	int const initialCurrentIndex = currentIndex();

	int n = clicked_tab_;

	for (int i = 0; i < n; ++i) {
		GuiWorkArea * wa = workArea(0);
		LASSERT(wa, return false);
		if (!wa->view().closeWorkArea(wa)) {
			// closing cancelled, if possible, reset initial current tab index
			if (initialCurrentIndex - i >= 0)
				setCurrentIndex(initialCurrentIndex - i);
			else
				setCurrentIndex(clicked_tab_ - i);
			return false;
		}
	}
	return true;
}


void TabWorkArea::closeOtherTabs()
{
	if (clicked_tab_ == -1)
		return;

	closeTabsToRight() && closeTabsToLeft();
}


void TabWorkArea::hideCurrentTab()
{
	GuiWorkArea * wa;
	if (clicked_tab_ == -1)
		wa = currentWorkArea();
	else {
		wa = workArea(clicked_tab_);
		LASSERT(wa, return);
	}
	wa->view().hideWorkArea(wa);
}


void TabWorkArea::closeTab(int index)
{
	on_currentTabChanged(index);
	GuiWorkArea * wa;
	if (index == -1)
		wa = currentWorkArea();
	else {
		wa = workArea(index);
		LASSERT(wa, return);
	}
	wa->view().closeWorkArea(wa);
}


void TabWorkArea::moveToStartCurrentTab()
{
	if (clicked_tab_ == -1)
		return;
	tabBar()->moveTab(clicked_tab_, 0);
}


void TabWorkArea::moveToEndCurrentTab()
{
	if (clicked_tab_ == -1)
		return;
	tabBar()->moveTab(clicked_tab_, count() - 1);
}


///
class DisplayPath {
public:
	/// make vector happy
	DisplayPath() : tab_(-1), dottedPrefix_(false) {}
	///
	DisplayPath(int tab, FileName const & filename)
		: tab_(tab)
	{
		// Recode URL encoded chars via fromPercentEncoding()
		string const fn = (filename.extension() == "lyx")
			? filename.onlyFileNameWithoutExt() : filename.onlyFileName();
		filename_ = QString::fromUtf8(QByteArray::fromPercentEncoding(fn.c_str()));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
		postfix_ = toqstr(filename.absoluteFilePath()).
			split("/", Qt::SkipEmptyParts);
#else
		postfix_ = toqstr(filename.absoluteFilePath()).
			split("/", QString::SkipEmptyParts);
#endif
		postfix_.pop_back();
		abs_ = toqstr(filename.absoluteFilePath());
		dottedPrefix_ = false;
	}

	/// Absolute path for debugging.
	QString abs() const
	{
		return abs_;
	}
	/// Add the first segment from the postfix or three dots to the prefix.
	/// Merge multiple dot tripples. In fact dots are added lazily, i.e. only
	/// when really needed.
	void shiftPathSegment(bool dotted)
	{
		if (postfix_.count() <= 0)
			return;

		if (!dotted) {
			if (dottedPrefix_ && !prefix_.isEmpty())
				prefix_ += ellipsisSlash_;
			prefix_ += postfix_.front() + "/";
		}
		dottedPrefix_ = dotted && !prefix_.isEmpty();
		postfix_.pop_front();
	}
	///
	QString displayString() const
	{
		if (prefix_.isEmpty())
			return filename_;

		bool dots = dottedPrefix_ || !postfix_.isEmpty();
		return prefix_ + (dots ? ellipsisSlash_ : "") + filename_;
	}
	///
	QString forecastPathString() const
	{
		if (postfix_.count() == 0)
			return displayString();

		return prefix_
			+ (dottedPrefix_ ? ellipsisSlash_ : "")
			+ postfix_.front() + "/";
	}
	///
	bool final() const { return postfix_.empty(); }
	///
	int tab() const { return tab_; }

private:
	/// ".../"
	static QString const ellipsisSlash_;
	///
	QString prefix_;
	///
	QStringList postfix_;
	///
	QString filename_;
	///
	QString abs_;
	///
	int tab_;
	///
	bool dottedPrefix_;
};


QString const DisplayPath::ellipsisSlash_ = QString(QChar(0x2026)) + "/";


///
bool operator<(DisplayPath const & a, DisplayPath const & b)
{
	return a.displayString() < b.displayString();
}

///
bool operator==(DisplayPath const & a, DisplayPath const & b)
{
	return a.displayString() == b.displayString();
}


void TabWorkArea::updateTabTexts()
{
	int const n = count();
	if (n == 0)
		return;
	std::list<DisplayPath> paths;
	typedef std::list<DisplayPath>::iterator It;

	// collect full names first: path into postfix, empty prefix and
	// filename without extension
	for (int i = 0; i < n; ++i) {
		GuiWorkArea * i_wa = workArea(i);
		FileName const fn = i_wa->bufferView().buffer().fileName();
		paths.push_back(DisplayPath(i, fn));
	}

	// go through path segments and see if it helps to make the path more unique
	bool somethingChanged = true;
	bool allFinal = false;
	while (somethingChanged && !allFinal) {
		// adding path segments changes order
		paths.sort();

		LYXERR(Debug::GUI, "updateTabTexts() iteration start");
		somethingChanged = false;
		allFinal = true;

		// find segments which are not unique (i.e. non-atomic)
		It it = paths.begin();
		It segStart = it;
		QString segString = it->displayString();
		for (; it != paths.end(); ++it) {
			// look to the next item
			It next = it;
			++next;

			// final?
			allFinal = allFinal && it->final();

			LYXERR(Debug::GUI, "it = " << it->abs()
			       << " => " << it->displayString());

			// still the same segment?
			QString nextString;
			if ((next != paths.end()
			     && (nextString = next->displayString()) == segString))
				continue;
			LYXERR(Debug::GUI, "segment ended");

			// only a trivial one with one element?
			if (it == segStart) {
				// start new segment
				segStart = next;
				segString = nextString;
				continue;
			}

			// We found a non-atomic segment
			// We know that segStart <= it < next <= paths.end().
			// The assertion below tells coverity about it.
			LATTEST(segStart != paths.end());
			QString dspString = segStart->forecastPathString();
			LYXERR(Debug::GUI, "first forecast found for "
			       << segStart->abs() << " => " << dspString);
			It sit = segStart;
			++sit;
			// Shift path segments and hope for the best
			// that it makes the path more unique.
			somethingChanged = true;
			bool moreUnique = false;
			for (; sit != next; ++sit) {
				if (sit->forecastPathString() != dspString) {
					LYXERR(Debug::GUI, "different forecast found for "
						<< sit->abs() << " => " << sit->forecastPathString());
					moreUnique = true;
					break;
				}
				LYXERR(Debug::GUI, "same forecast found for "
					<< sit->abs() << " => " << dspString);
			}

			// if the path segment helped, add it. Otherwise add dots
			bool dots = !moreUnique;
			LYXERR(Debug::GUI, "using dots = " << dots);
			for (sit = segStart; sit != next; ++sit) {
				sit->shiftPathSegment(dots);
				LYXERR(Debug::GUI, "shifting "
					<< sit->abs() << " => " << sit->displayString());
			}

			// start new segment
			segStart = next;
			segString = nextString;
		}
	}

	// set new tab titles
	for (It it = paths.begin(); it != paths.end(); ++it) {
		int const tab_index = it->tab();
		Buffer const & buf = workArea(tab_index)->bufferView().buffer();
		QString tab_text = it->displayString().replace("&", "&&");
		if (!buf.fileName().empty() && !buf.isClean())
			tab_text += "*";
		QString tab_tooltip = it->abs();
		if (buf.hasReadonlyFlag()) {
#ifdef Q_OS_MAC
			QLabel * readOnlyButton = new QLabel();
			QIcon icon = QIcon(getPixmap("images/", "emblem-readonly", "svgz,png"));
			readOnlyButton->setPixmap(icon.pixmap(QSize(16, 16)));
			tabBar()->setTabButton(tab_index, QTabBar::RightSide, readOnlyButton);
#else
			setTabIcon(tab_index, QIcon(getPixmap("images/", "emblem-readonly", "svgz,png")));
#endif
			tab_tooltip = qt_("%1 (read only)").arg(tab_tooltip);
		} else
#ifdef Q_OS_MAC
			tabBar()->setTabButton(tab_index, QTabBar::RightSide, 0);
#else
			setTabIcon(tab_index, QIcon());
#endif
		if (buf.notifiesExternalModification()) {
			QString const warn = qt_("%1 (modified externally)");
			tab_tooltip = warn.arg(tab_tooltip);
			tab_text += QChar(0x26a0);
		}
		setTabText(tab_index, tab_text);
		setTabToolTip(tab_index, tab_tooltip);
	}
}


void TabWorkArea::showContextMenu(const QPoint & pos)
{
	// which tab?
	clicked_tab_ = tabBar()->tabAt(pos);
	if (clicked_tab_ == -1)
		return;

	GuiWorkArea * wa = workArea(clicked_tab_);
	LASSERT(wa, return);

	// show tab popup
	QMenu popup;
	popup.addAction(qt_("&Hide Tab"), this, SLOT(hideCurrentTab()));

	// we want to show the 'close' option only if this is not a child buffer.
	Buffer const & buf = wa->bufferView().buffer();
	if (!buf.parent())
		popup.addAction(qt_("&Close Tab"), this, SLOT(closeCurrentBuffer()));

	popup.addSeparator();

	QAction * closeOther = popup.addAction(qt_("Close &Other Tabs"), this, SLOT(closeOtherTabs()));
	closeOther->setEnabled(clicked_tab_ != 0 || hasTabsToRight(clicked_tab_));
	QAction * closeRight = popup.addAction(qt_("Close Tabs to the &Right"), this, SLOT(closeTabsToRight()));
	closeRight->setEnabled(hasTabsToRight(clicked_tab_));
	QAction * closeLeft = popup.addAction(qt_("Close Tabs to the &Left"), this, SLOT(closeTabsToLeft()));
	closeLeft->setEnabled(clicked_tab_ != 0);

	popup.addSeparator();

	QAction * moveStart = popup.addAction(qt_("Move Tab to &Start"), this, SLOT(moveToStartCurrentTab()));
	moveStart->setEnabled(closeLeft->isEnabled());
	QAction * moveEnd = popup.addAction(qt_("Move Tab to &End"), this, SLOT(moveToEndCurrentTab()));
	moveEnd->setEnabled(closeRight->isEnabled());

	popup.addSeparator();

	popup.addAction(qt_("Open Enclosing &Directory"), this, SLOT(openEnclosingDirectory()));

	popup.exec(tabBar()->mapToGlobal(pos));

	clicked_tab_ = -1;
}

bool TabWorkArea::hasTabsToRight(int index) {
	return count() - 1 > index;
}


void TabWorkArea::moveTab(int fromIndex, int toIndex)
{
	QWidget * w = widget(fromIndex);
	QIcon icon = tabIcon(fromIndex);
	QString text = tabText(fromIndex);

	setCurrentIndex(fromIndex);
	removeTab(fromIndex);
	insertTab(toIndex, w, icon, text);
	setCurrentIndex(toIndex);
}


GuiWorkAreaContainer::GuiWorkAreaContainer(GuiWorkArea * wa, QWidget * parent)
	: QWidget(parent), wa_(wa)
{
	LASSERT(wa, return);
	Ui::WorkAreaUi::setupUi(this);
	layout()->addWidget(wa);
	connect(wa, SIGNAL(titleChanged(GuiWorkArea *)),
	        this, SLOT(updateDisplay()));
	connect(reloadPB, SIGNAL(clicked()), this, SLOT(reload()));
	connect(ignorePB, SIGNAL(clicked()), this, SLOT(ignore()));
	setMessageColour({notificationFrame, externalModificationLabel},
	                 {reloadPB, ignorePB});
	updateDisplay();
}


void GuiWorkAreaContainer::updateDisplay()
{
	Buffer const & buf = wa_->bufferView().buffer();
	notificationFrame->setHidden(!buf.notifiesExternalModification());
	QString const label = qt_("<b>The file %1 changed on disk.</b>")
		.arg(toqstr(buf.fileName().displayName()));
	externalModificationLabel->setText(label);
}


void GuiWorkAreaContainer::dispatch(FuncRequest const & f) const
{
	lyx::dispatch(FuncRequest(LFUN_BUFFER_SWITCH,
	                          wa_->bufferView().buffer().absFileName()));
	lyx::dispatch(f);
}


void GuiWorkAreaContainer::reload() const
{
	dispatch(FuncRequest(LFUN_BUFFER_RELOAD));
}


void GuiWorkAreaContainer::ignore() const
{
	dispatch(FuncRequest(LFUN_BUFFER_EXTERNAL_MODIFICATION_CLEAR));
}


void GuiWorkAreaContainer::mouseDoubleClickEvent(QMouseEvent * event)
{
	// prevent TabWorkArea from opening a new buffer on double click
	event->accept();
}


} // namespace frontend
} // namespace lyx

#include "moc_GuiWorkArea.cpp"
