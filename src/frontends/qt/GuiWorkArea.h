// -*- C++ -*-
/**
 * \file GuiWorkArea.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef WORKAREA_H
#define WORKAREA_H

#include "ui_WorkAreaUi.h"

#include "frontends/WorkArea.h"

#include <QAbstractScrollArea>
#include <QTabWidget>

class QToolButton;
class QWidget;

namespace lyx {

class Buffer;
class BufferView;
class FuncRequest;
class KeySymbol;

namespace frontend {

class GuiCompleter;
class GuiInputMethod;
class GuiView;

class GuiWorkArea : public QAbstractScrollArea, public WorkArea
{
	Q_OBJECT

public:
	///
	GuiWorkArea(QWidget *);
	///
	GuiWorkArea(Buffer & buffer, GuiView & gv);
	///
	~GuiWorkArea();

	///
	void init();
	///
	void setBuffer(Buffer &);
	///
	void setGuiView(GuiView &);
	///
	void setInputMethod();
	///
	GuiInputMethod * inputMethod();
	///
	void setFullScreen(bool full_screen);
	/// is GuiView in fullscreen mode?
	bool isFullScreen() const;
	///
	BufferView & bufferView() override;
	///
	BufferView const & bufferView() const override;
	///
	void resizeBufferView();
	///
	void scheduleRedraw(bool update_metrics) override;

	/// return true if the key is part of a shortcut
	bool queryKeySym(KeySymbol const & key, KeyModifier mod) const;

	/// Ask relative position of the input item coordinates against the main
	/// coordinates to the system input method
	void queryInputItemGeometry();
	/// Restore coordinate transformation information
	void resetInputItemGeometry();
	/// Restore coordinate transformation information
	void resetInputItemGeometry(bool is_new_view);

	bool inDialogMode() const;
	void setDialogMode(bool mode);

	///
	GuiCompleter & completer();

	/// Return the GuiView this workArea belongs to
	GuiView const & view() const;
	GuiView & view();

	/// Current ratio between physical pixels and device-independent pixels
	double pixelRatio() const;

public Q_SLOTS:
	/// Process Key pressed event.
	/// This needs to be public because it is accessed externally by GuiView.
	void processKeySym(KeySymbol const & key, KeyModifier mod);
	///
	void stopBlinkingCaret();
	///
	void startBlinkingCaret();

Q_SIGNALS:
	///
	void titleChanged(GuiWorkArea *);
	///
	void busy(bool);
	///
	void bufferViewChanged();
	/// send key event to CompressorProxy
	void compressKeySym(KeySymbol const & sym, KeyModifier mod, bool isAutoRepeat);
	///
	void preeditChanged(QInputMethodEvent * ev);

private Q_SLOTS:
	/// Scroll the BufferView.
	/**
	  * This is a slot for the valueChanged() signal of the vertical scrollbar.
	  * \p value value of the scrollbar.
	*/
	void scrollTo(int value);
	/// timer to limit triple clicks
	void doubleClickTimeout();
	/// toggle the caret's visibility
	void toggleCaret();
	/// close this work area.
	/// Slot for Buffer::closing signal.
	void close() override;
	/// Slot to restore proper scrollbar behaviour.
	void fixVerticalScrollBar();

	void flagPreedit(QInputMethodEvent* ev);
	void receiveIMQueryResponse(QVariant);

private:
	/// Update window titles of all users.
	void updateWindowTitle() override;
	///
	bool event(QEvent *) override;
	///
	void contextMenuEvent(QContextMenuEvent *) override;
	///
	void focusInEvent(QFocusEvent *) override;
	///
	void focusOutEvent(QFocusEvent *) override;
	/// repaint part of the widget
	void paintEvent(QPaintEvent * ev) override;
	/// widget has been resized
	void resizeEvent(QResizeEvent * ev) override;
	/// mouse button press
	void mousePressEvent(QMouseEvent * ev) override;
	/// mouse button release
	void mouseReleaseEvent(QMouseEvent * ev) override;
	/// mouse double click of button
	void mouseDoubleClickEvent(QMouseEvent * ev) override;
	/// mouse motion
	void mouseMoveEvent(QMouseEvent * ev) override;
	/// wheel event
	void wheelEvent(QWheelEvent * ev) override;
	/// key press event. It also knows how to handle ShortcutOverride events to
	/// avoid code duplication.
	void keyPressEvent(QKeyEvent * ev) override;
	/// IM events
	void inputMethodEvent(QInputMethodEvent * ev) override;
	/// IM query
	QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

	/// The slot connected to SyntheticMouseEvent::timeout.
	void generateSyntheticMouseEvent();

	friend class GuiCompleter;
	struct Private;
	Private * const d;
}; // GuiWorkArea


/// CompressorProxy adapted from Kuba Ober https://stackoverflow.com/a/21006207
class CompressorProxy : public QObject
{
    Q_OBJECT
	bool emitCheck(bool isAutoRepeat);
	bool flag_;
	// input: event to compress
	Q_SLOT void slot(KeySymbol const & sym, KeyModifier mod, bool isAutoRepeat);
	// output: compressed event
    Q_SIGNAL void signal(KeySymbol const & sym, KeyModifier mod);
public:
    // No default constructor, since the proxy must be a child of the
    // target object.
	explicit CompressorProxy(GuiWorkArea * wa);
};


class EmbeddedWorkArea : public GuiWorkArea
{
	Q_OBJECT
public:
	///
	EmbeddedWorkArea(QWidget *);
	~EmbeddedWorkArea();

	/// Dummy methods for Designer.
	void setWidgetResizable(bool) {}
	void setWidget(QWidget *) {}

	QSize sizeHint () const override;
	///
	void disable();

protected:
	///
	void closeEvent(QCloseEvent * ev) override;
	///
	void hideEvent(QHideEvent *ev) override;

private:
	/// Embedded Buffer.
	Buffer * buffer_;
}; // EmbeddedWorkArea


class GuiWorkAreaContainer;

/// A tabbed set of GuiWorkAreas.
class TabWorkArea : public QTabWidget
{
	Q_OBJECT
public:
	TabWorkArea(QWidget * parent = nullptr);

	/// hide QTabWidget methods
	GuiWorkAreaContainer * currentWidget() const;
	GuiWorkAreaContainer * widget(int index) const;

	///
	void setFullScreen(bool full_screen);
	void showBar(bool show);
	void closeAll();
	bool setCurrentWorkArea(GuiWorkArea *);
	GuiWorkArea * addWorkArea(Buffer & buffer, GuiView & view);
	bool removeWorkArea(GuiWorkArea *);
	GuiWorkArea * currentWorkArea() const;
	GuiWorkArea * workArea(Buffer & buffer) const;
	GuiWorkArea * workArea(int index) const;
	void paintEvent(QPaintEvent *) override;

Q_SIGNALS:
	///
	void currentWorkAreaChanged(GuiWorkArea *);
	///
	void lastWorkAreaRemoved();

public Q_SLOTS:
	/// close current buffer, or the one given by \c clicked_tab_
	void closeCurrentBuffer();
	/// hide current tab, or the one given by \c clicked_tab_
	void hideCurrentTab();
	///
	bool closeTabsToRight();
	///
	bool closeTabsToLeft();
	///
	void closeOtherTabs();
	///
	void moveToStartCurrentTab();
	///
	void moveToEndCurrentTab();
	///
	bool openEnclosingDirectory();
	/// close the tab given by \c index
	void closeTab(int index);
	///
	void moveTab(int fromIndex, int toIndex);
	///
	void updateTabTexts();

private Q_SLOTS:
	///
	void on_currentTabChanged(int index);
	///
	void on_tabBarClicked(int index);
	///
	void showContextMenu(const QPoint & pos);
	/// enable closing tab on middle-click
	void mousePressEvent(QMouseEvent * me) override;
	void mouseReleaseEvent(QMouseEvent * me) override;
	///
	void mouseDoubleClickEvent(QMouseEvent * event) override;
	///
	int indexOfWorkArea(GuiWorkArea * w) const;

private:
	using QTabWidget::addTab;
	using QTabWidget::insertTab;

	/// true if position is a tab (rather than the blank space in tab bar)
	bool posIsTab(QPoint position);

	// true if there are tabs to the right of the tab at index
	bool hasTabsToRight(int index);

	int clicked_tab_;
	///
	int midpressed_tab_;
	///
	QToolButton * closeBufferButton;
}; // TabWorkArea


class GuiWorkAreaContainer : public QWidget, public Ui::WorkAreaUi
{
	Q_OBJECT
	// non-null
	GuiWorkArea * const wa_;
	void dispatch(FuncRequest const & f) const;

private Q_SLOTS:
	void updateDisplay();
	void reload() const;
	void ignore() const;

protected:
	void mouseDoubleClickEvent(QMouseEvent * event) override;

public:
	/// wa != 0
	GuiWorkAreaContainer(GuiWorkArea * wa, QWidget * parent = nullptr);
	/// non-null
	GuiWorkArea * workArea() const { return wa_; }
};



} // namespace frontend
} // namespace lyx

#endif // WORKAREA_H
