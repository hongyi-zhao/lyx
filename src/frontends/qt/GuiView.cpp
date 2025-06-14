/**
 * \file GuiView.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author John Levon
 * \author Abdelrazak Younes
 * \author Peter Kümmel
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiView.h"

#include "DialogFactory.h"
#include "DispatchResult.h"
#include "FileDialog.h"
#include "FindAndReplace.h"
#include "FontLoader.h"
#include "GuiApplication.h"
#include "GuiClickableLabel.h"
#include "GuiCompleter.h"
#include "GuiFontMetrics.h"
#include "GuiInputMethod.h"
#include "GuiKeySymbol.h"
#include "GuiToc.h"
#include "GuiToolbar.h"
#include "GuiWorkArea.h"
#include "GuiProgress.h"
#include "LayoutBox.h"
#include "Menus.h"
#include "TocModel.h"

#include "qt_helpers.h"

#include "frontends/alert.h"
#include "frontends/KeySymbol.h"

#include "buffer_funcs.h"
#include "Buffer.h"
#include "BufferList.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Converter.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "Encoding.h"
#include "ErrorList.h"
#include "Format.h"
#include "FuncStatus.h"
#include "FuncRequest.h"
#include "KeySymbol.h"
#include "Language.h"
#include "LaTeXColors.h"
#include "LayoutFile.h"
#include "LyXAction.h"
#include "LyX.h"
#include "LyXRC.h"
#include "LyXVC.h"
#include "Paragraph.h"
#include "Session.h"
#include "SpellChecker.h"
#include "Statistics.h"
#include "TexRow.h"
#include "Text.h"
#include "Toolbars.h"
#include "version.h"

#include "graphics/PreviewLoader.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/ExceptionMessage.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/ForkedCalls.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/Lexer.h"
#include "support/lstrings.h"
#include "support/os.h"
#include "support/Package.h"
#include "support/PathChanger.h"
#include "support/Systemcall.h"
#include "support/Timeout.h"
#include "support/ProgressInterface.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFuture>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QSettings>
#include <QShowEvent>
#include <QSlider>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSvgRenderer>
#include <QtConcurrentRun>
#include <QTimer>
#include <QUrl>
#include <QWindowStateChangeEvent>
#include <QGestureEvent>
#include <QPinchGesture>


// sync with GuiAlert.cpp
#define EXPORT_in_THREAD 1


#include <sstream>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif


using namespace std;
using namespace lyx::support;

namespace lyx {

using support::addExtension;
using support::changeExtension;
using support::removeExtension;
using std::bind;
using std::placeholders::_1;

namespace frontend {

namespace {

class BackgroundWidget : public QWidget
{
public:
	BackgroundWidget(int width, int height)
		: width_(width), height_(height)
	{
		LYXERR(Debug::GUI, "show banner: " << lyxrc.show_banner);
		if (!lyxrc.show_banner)
			return;
		/// The text to be written on top of the pixmap
		QString const htext = qt_("The Document\nProcessor[[welcome banner]]");
		QString const htextsize = qt_("1.0[[translating this to different value scales the welcome banner text size for your language]]");
		/// The text to be written on top of the pixmap
		QString const text = lyx_version ?
			qt_("version ") + lyx_version : qt_("unknown version");
		QString imagedir = "images/";
		FileName fname = imageLibFileSearch(imagedir, "banner", "svgz");
		QSvgRenderer svgRenderer(toqstr(fname.absFileName()));
		if (svgRenderer.isValid()) {
			splash_ = QPixmap(splashSize());
			QPainter painter(&splash_);
			svgRenderer.render(&painter);
			splash_.setDevicePixelRatio(pixelRatio());
		} else {
			splash_ = getPixmap("images/", "banner", "png");
		}

		QPainter pain(&splash_);
		pain.setPen(QColor(0, 0, 0));
		qreal const fsize = fontSize();
		bool ok;
		int hfsize = 20;
		qreal locscale = htextsize.toFloat(&ok);
		if (!ok)
			locscale = 1.0;
		QPointF const position = textPosition(false);
		QPointF const hposition = textPosition(true);
		QRectF const hrect(hposition, splashSize());
		LYXERR(Debug::GUI,
			"widget pixel ratio: " << pixelRatio() <<
			" splash pixel ratio: " << splashPixelRatio() <<
			" version text size,position: " << fsize << "@" << position.x() << "+" << position.y());
		QFont font;
		// The font used to display the version info
		font.setStyleHint(QFont::SansSerif);
		font.setWeight(QFont::Bold);
		font.setPointSizeF(fsize);
		pain.setFont(font);
		pain.drawText(position, text);
		// The font used to display the version info
		font.setStyleHint(QFont::SansSerif);
		font.setWeight(QFont::Normal);
		font.setPointSizeF(hfsize);
		// Check how long the logo gets with the current font
		// and adapt if the font is running wider than what
		// we assume
		GuiFontMetrics fm(font);
		// Split the title into lines to measure the longest line
		// in the current l7n.
		QStringList titlesegs = htext.split('\n');
		int wline = 0;
		int hline = fm.maxHeight();
		for (QString const & seg : titlesegs) {
			if (fm.width(seg) > wline)
				wline = fm.width(seg);
		}
		// The longest line in the reference font (for English)
		// is 180. Calculate scale factor from that.
		double const wscale = wline > 0 ? (180.0 / wline) : 1;
		// Now do the same for the height (necessary for condensed fonts)
		double const hscale = (34.0 / hline);
		// take the lower of the two scale factors.
		double const scale = min(wscale, hscale);
		// Now rescale. Also consider l7n's offset factor.
		font.setPointSizeF(hfsize * scale * locscale);

		pain.setFont(font);
		pain.drawText(hrect, Qt::AlignLeft, htext);
		setFocusPolicy(Qt::StrongFocus);
	}

	void paintEvent(QPaintEvent *) override
	{
		int const w = width_;
		int const h = height_;
		int const x = (width() - w) / 2;
		int const y = (height() - h) / 2;
		LYXERR(Debug::GUI,
			"widget pixel ratio: " << pixelRatio() <<
			" splash pixel ratio: " << splashPixelRatio() <<
			" paint pixmap: " << w << "x" << h << "@" << x << "+" << y);
		QPainter pain(this);
		pain.drawPixmap(x, y, w, h, splash_);
	}

	void keyPressEvent(QKeyEvent * ev) override
	{
		KeySymbol sym;
		setKeySymbol(&sym, ev);
		if (sym.isOK()) {
			guiApp->processKeySym(sym, q_key_state(ev->modifiers()));
			ev->accept();
		} else {
			ev->ignore();
		}
	}

private:
	QPixmap splash_;
	int const width_;
	int const height_;

	/// Current ratio between physical pixels and device-independent pixels
	double pixelRatio() const {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		return devicePixelRatioF();
#else
		return devicePixelRatio();
#endif
	}

	qreal fontSize() const {
		return toqstr(lyxrc.font_sizes[NORMAL_SIZE]).toDouble();
	}

	QPointF textPosition(bool const heading) const {
		return heading ? QPointF(width_/2 - 18, height_/2 - 45)
			       : QPointF(width_/2 - 18, height_/2 + 45);
	}

	QSize splashSize() const {
		return QSize(
			static_cast<unsigned int>(width_ * pixelRatio()),
			static_cast<unsigned int>(height_ * pixelRatio()));
	}

	/// Ratio between physical pixels and device-independent pixels of splash image
	double splashPixelRatio() const {
		return splash_.devicePixelRatio();
	}
};


/// Toolbar store providing access to individual toolbars by name.
typedef map<string, GuiToolbar *> ToolbarMap;

typedef shared_ptr<Dialog> DialogPtr;

} // namespace


class GuiView::GuiViewPrivate
{
	/// noncopyable
	GuiViewPrivate(GuiViewPrivate const &);
	void operator=(GuiViewPrivate const &);
public:
	GuiViewPrivate(GuiView * gv)
		: gv_(gv), current_work_area_(nullptr), current_main_work_area_(nullptr),
		layout_(nullptr), autosave_timeout_(5000),
		in_show_(false)
	{
		// hardcode here the platform specific icon size
		smallIconSize = 16;  // scaling problems
		normalIconSize = 20; // ok, default if iconsize.png is missing
		bigIconSize = 26;	// better for some math icons
		hugeIconSize = 32;	// better for hires displays
		giantIconSize = 48;

		// if it exists, use width of iconsize.png as normal size
		QString const dir = toqstr(addPath("images", lyxrc.icon_set));
		FileName const fn = lyx::libFileSearch(dir, "iconsize.png");
		if (!fn.empty()) {
			QImage image(toqstr(fn.absFileName()));
			if (image.width() < int(smallIconSize))
				normalIconSize = smallIconSize;
			else if (image.width() > int(giantIconSize))
				normalIconSize = giantIconSize;
			else
				normalIconSize = image.width();
		}

		splitter_ = new QSplitter;
		bg_widget_ = new BackgroundWidget(400, 250);
		stack_widget_ = new QStackedWidget;
		stack_widget_->addWidget(bg_widget_);
		stack_widget_->addWidget(splitter_);
		setBackground();

		// TODO cleanup, remove the singleton, handle multiple Windows?
		progress_ = ProgressInterface::instance();
		if (!dynamic_cast<GuiProgress*>(progress_)) {
			progress_ = new GuiProgress;  // TODO who deletes it
			ProgressInterface::setInstance(progress_);
		}
		QObject::connect(
				dynamic_cast<GuiProgress*>(progress_),
				SIGNAL(updateStatusBarMessage(QString const&)),
				gv, SLOT(updateStatusBarMessage(QString const&)));
		QObject::connect(
				dynamic_cast<GuiProgress*>(progress_),
				SIGNAL(clearMessageText()),
				gv, SLOT(clearMessageText()));
	}

	~GuiViewPrivate()
	{
		delete splitter_;
		delete bg_widget_;
		delete stack_widget_;
	}

	void setBackground()
	{
		stack_widget_->setCurrentWidget(bg_widget_);
		bg_widget_->setUpdatesEnabled(true);
		bg_widget_->setFocus();
	}

	int tabWorkAreaCount()
	{
		return splitter_->count();
	}

	TabWorkArea * tabWorkArea(int i)
	{
		return dynamic_cast<TabWorkArea *>(splitter_->widget(i));
	}

	TabWorkArea * currentTabWorkArea()
	{
		int areas = tabWorkAreaCount();
		if (areas == 1)
			// The first TabWorkArea is always the first one, if any.
			return tabWorkArea(0);

		for (int i = 0; i != areas;  ++i) {
			TabWorkArea * twa = tabWorkArea(i);
			if (current_main_work_area_ == twa->currentWorkArea())
				return twa;
		}

		// None has the focus so we just take the first one.
		return tabWorkArea(0);
	}

	int countWorkAreasOf(Buffer & buf)
	{
		int areas = tabWorkAreaCount();
		int count = 0;
		for (int i = 0; i != areas;  ++i) {
			TabWorkArea * twa = tabWorkArea(i);
			if (twa->workArea(buf))
				++count;
		}
		return count;
	}

	void setPreviewFuture(QFuture<Buffer::ExportStatus> const & f)
	{
		if (processing_thread_watcher_.isRunning()) {
			// we prefer to cancel this preview in order to keep a snappy
			// interface.
			return;
		}
		processing_thread_watcher_.setFuture(f);
	}

	QSize iconSize(docstring const & icon_size)
	{
		unsigned int size;
		if (icon_size == "small")
			size = smallIconSize;
		else if (icon_size == "normal")
			size = normalIconSize;
		else if (icon_size == "big")
			size = bigIconSize;
		else if (icon_size == "huge")
			size = hugeIconSize;
		else if (icon_size == "giant")
			size = giantIconSize;
		else
			size = icon_size.empty() ? normalIconSize : convert<int>(icon_size);

		if (size < smallIconSize)
			size = smallIconSize;

		return QSize(size, size);
	}

	QSize iconSize(QString const & icon_size)
	{
		return iconSize(qstring_to_ucs4(icon_size));
	}

	string & iconSize(QSize const & qsize)
	{
		LATTEST(qsize.width() == qsize.height());

		static string icon_size;

		unsigned int size = qsize.width();

		if (size < smallIconSize)
			size = smallIconSize;

		if (size == smallIconSize)
			icon_size = "small";
		else if (size == normalIconSize)
			icon_size = "normal";
		else if (size == bigIconSize)
			icon_size = "big";
		else if (size == hugeIconSize)
			icon_size = "huge";
		else if (size == giantIconSize)
			icon_size = "giant";
		else
			icon_size = convert<string>(size);

		return icon_size;
	}

	static Buffer::ExportStatus previewAndDestroy(Buffer const * orig,
			Buffer * buffer, string const & format);
	static Buffer::ExportStatus exportAndDestroy(Buffer const * orig,
			Buffer * buffer, string const & format);
	static Buffer::ExportStatus compileAndDestroy(Buffer const * orig,
			Buffer * buffer, string const & format);
	static docstring autosaveAndDestroy(Buffer const * orig, Buffer * buffer);

	template<class T>
	static Buffer::ExportStatus runAndDestroy(const T& func,
			Buffer const * orig, Buffer * buffer, string const & format);

	// TODO syncFunc/previewFunc: use bind
	bool asyncBufferProcessing(string const & argument,
				   Buffer const * used_buffer,
				   docstring const & msg,
				   Buffer::ExportStatus (*asyncFunc)(Buffer const *, Buffer *, string const &),
				   Buffer::ExportStatus (Buffer::*syncFunc)(string const &, bool) const,
				   Buffer::ExportStatus (Buffer::*previewFunc)(string const &) const,
				   bool allow_async, bool use_tmpdir = false);

	QVector<GuiWorkArea*> guiWorkAreas();

public:
	GuiView * gv_;
	GuiWorkArea * current_work_area_;
	GuiWorkArea * current_main_work_area_;
	QSplitter * splitter_;
	QStackedWidget * stack_widget_;
	BackgroundWidget * bg_widget_;
	/// view's toolbars
	ToolbarMap toolbars_;
	ProgressInterface* progress_;
	/// The main layout box.
	/**
	 * \warning Don't Delete! The layout box is actually owned by
	 * whichever toolbar contains it. All the GuiView class needs is a
	 * means of accessing it.
	 *
	 * FIXME: replace that with a proper model so that we are not limited
	 * to only one dialog.
	 */
	LayoutBox * layout_;

	///
	map<string, DialogPtr> dialogs_;

	///
	QTimer statusbar_timer_;
	QTimer statusbar_stats_timer_;
	/// auto-saving of buffers
	Timeout autosave_timeout_;

	///
	TocModels toc_models_;

	///
	QFutureWatcher<docstring> autosave_watcher_;
	QFutureWatcher<Buffer::ExportStatus> processing_thread_watcher_;
	///
	string last_export_format;
	string processing_format;

	// Buffers that are being exported
	static QSet<Buffer const *> busyBuffers;

	unsigned int smallIconSize;
	unsigned int normalIconSize;
	unsigned int bigIconSize;
	unsigned int hugeIconSize;
	unsigned int giantIconSize;

	/// flag against a race condition due to multiclicks, see bug #1119
	bool in_show_;

	// Timers for statistic updates in buffer
	/// Current time left to the nearest info update
	int time_to_update = 1000;
	///Basic step for timer in ms. Basically reaction time for short selections
	int const timer_rate = 500;
	/// Real stats updates infrequently. First they take long time for big buffers, second
	/// they are visible for fast-repeat keyboards even for mid documents.
	int const default_stats_rate = 5000;
	/// Detection of new selection, so we can react fast
	bool already_in_selection_ = false;
	/// Maximum size of "short" selection for which we can update with faster timer_rate
	int const max_sel_chars = 5000;
	/// equivalent time_to_update = 0; but better to see it the code
	bool stats_update_trigger_ = false;

};

QSet<Buffer const *> GuiView::GuiViewPrivate::busyBuffers;


GuiView::GuiView(int id)
	: d(*new GuiViewPrivate(this)), id_(id), closing_(false), busy_(0),
	  command_execute_(false), minibuffer_focus_(false), word_count_enabled_(true),
	  char_count_enabled_(true),  char_nb_count_enabled_(false),
	  toolbarsMovable_(false), devel_mode_(false),
	  colors_model_(new QStandardItemModel(0, 4, this))
{
	connect(this, SIGNAL(bufferViewChanged()),
	        this, SLOT(onBufferViewChanged()));

	// GuiToolbars *must* be initialised before the menu bar.
	setIconSize(QSize(d.normalIconSize, d.normalIconSize)); // at least on Mac the default is 32 otherwise, which is huge
	constructToolbars();

	// set ourself as the current view. This is needed for the menu bar
	// filling, at least for the static special menu item on Mac. Otherwise
	// they are greyed out.
	guiApp->setCurrentView(this);

	// Fill up the menu bar.
	guiApp->menus().fillMenuBar(menuBar(), this, true);

	setCentralWidget(d.stack_widget_);

	// Start autosave timer
	if (lyxrc.autosave) {
		// The connection is closed when this is destroyed.
		d.autosave_timeout_.timeout.connect([this](){ autoSave();});
		d.autosave_timeout_.setTimeout(lyxrc.autosave * 1000);
		d.autosave_timeout_.start();
	}
	connect(&d.statusbar_timer_, SIGNAL(timeout()),
		this, SLOT(clearMessage()));
	connect(&d.statusbar_stats_timer_, SIGNAL(timeout()),
		this, SLOT(showStats()));
	d.statusbar_stats_timer_.start(d.timer_rate);

	// We don't want to keep the window in memory if it is closed.
	setAttribute(Qt::WA_DeleteOnClose, true);

#if !(defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN)) && !defined(Q_OS_MAC)
	// assign an icon to main form. We do not do it under Qt/Win or Qt/Mac,
	// since the icon is provided in the application bundle. We use a themed
	// version when available and use the bundled one as fallback.
	setWindowIcon(QIcon::fromTheme("lyx", getPixmap("images/", "lyx", "svg,png")));

#endif
	resetWindowTitle();

	// use tabbed dock area for multiple docks
	// (such as "source" and "messages")
	setDockOptions(QMainWindow::ForceTabbedDocks);

#ifdef Q_OS_MAC
	// use document mode tabs on docks
	setDocumentMode(true);
#endif

	// For Drag&Drop.
	setAcceptDrops(true);

	QFontMetrics const fm(statusBar()->fontMetrics());
	int const iconheight = max(int(d.normalIconSize), fm.height());
	QSize const iconsize(iconheight, iconheight);

	// add busy indicator to statusbar
	search_mode mode = theGuiApp()->imageSearchMode();
	QString fn = toqstr(lyx::libFileSearch("images", "busy", "svgz", mode).absFileName());
	PressableSvgWidget * busySVG = new PressableSvgWidget(fn);
	statusBar()->addPermanentWidget(busySVG);
	// make busy indicator square with 5px margins
	busySVG->setMaximumSize(busySVG->height() - 5, busySVG->height() - 5);
	busySVG->hide();
	// Add cancel button
	QPixmap ps = QIcon(getPixmap("images/", "process-stop", "svgz")).pixmap(iconsize);
	GuiClickableLabel * processStop = new GuiClickableLabel(statusBar());
	processStop->setPixmap(ps);
	processStop->setToolTip(qt_("Click here to stop export/output process"));
	processStop->hide();
	statusBar()->addPermanentWidget(processStop);

	connect(&d.processing_thread_watcher_, SIGNAL(started()),
		busySVG, SLOT(show()));
	connect(&d.processing_thread_watcher_, SIGNAL(finished()),
		busySVG, SLOT(hide()));
	connect(&d.processing_thread_watcher_, SIGNAL(started()),
		processStop, SLOT(show()));
	connect(&d.processing_thread_watcher_, SIGNAL(finished()),
		processStop, SLOT(hide()));
	connect(processStop, SIGNAL(clicked()), this, SLOT(checkCancelBackground()));

	connect(this, SIGNAL(scriptKilled()), busySVG, SLOT(hide()));
	connect(this, SIGNAL(scriptKilled()), processStop, SLOT(hide()));

	stat_counts_ = new GuiClickableLabel(statusBar());
	stat_counts_->setAlignment(Qt::AlignCenter);
	stat_counts_->hide();
	statusBar()->addPermanentWidget(stat_counts_);

	connect(stat_counts_, SIGNAL(clicked()), this, SLOT(statsPressed()));

	zoom_slider_ = new QSlider(Qt::Horizontal, statusBar());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
	zoom_slider_->setFixedWidth(fm.horizontalAdvance('x') * 15);
#else
	zoom_slider_->setFixedWidth(fm.width('x') * 15);
#endif
	// Make the defaultZoom center
	zoom_slider_->setRange(10, (lyxrc.defaultZoom * 2) - 10);
	// Initialize proper zoom value
	QSettings settings;
	zoom_ratio_ = settings.value("zoom_ratio", 1.0).toDouble();
	// Actual zoom value: default zoom + fractional offset
	int zoom = (int)(lyxrc.defaultZoom * zoom_ratio_);
	zoom = min(max(zoom, zoom_min_), zoom_max_);
	zoom_slider_->setValue(zoom);
	zoom_slider_->setToolTip(qt_("Workarea zoom level. Drag, use Ctrl-+/- or Shift-Mousewheel to adjust."));

	// Buttons to change zoom stepwise
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
	QSize s(fm.horizontalAdvance('+'), fm.height());
#else
	QSize s(fm.width('+'), fm.height());
#endif
	zoom_in_ = new GuiClickableLabel(statusBar());
	zoom_in_->setText("+");
	zoom_in_->setFixedSize(s);
	zoom_in_->setAlignment(Qt::AlignCenter);
	zoom_out_ = new GuiClickableLabel(statusBar());
	zoom_out_->setText(QString(QChar(0x2212)));
	zoom_out_->setFixedSize(s);
	zoom_out_->setAlignment(Qt::AlignCenter);


	zoom_widget_ = new QWidget(statusBar());
	zoom_widget_->setAttribute(Qt::WA_MacSmallSize);
	zoom_widget_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	zoom_widget_->setLayout(new QHBoxLayout());
	zoom_widget_->layout()->setSpacing(5);
	zoom_widget_->layout()->setContentsMargins(0,0,0,0);
	zoom_widget_->layout()->addWidget(zoom_out_);
	zoom_widget_->layout()->addWidget(zoom_slider_);
	zoom_widget_->layout()->addWidget(zoom_in_);
	statusBar()->addPermanentWidget(zoom_widget_);
	zoom_out_->setEnabled(currentBufferView()
			      && zoom_slider_->value() > zoom_slider_->minimum());
	zoom_slider_->setEnabled(currentBufferView());
	zoom_in_->setEnabled(currentBufferView()
			     && zoom_slider_->value() < zoom_slider_->maximum());

	connect(zoom_slider_, SIGNAL(sliderMoved(int)), this, SLOT(zoomSliderMoved(int)));
	connect(zoom_slider_, SIGNAL(valueChanged(int)), this, SLOT(zoomValueChanged(int)));
	connect(this, SIGNAL(currentZoomChanged(int)), zoom_slider_, SLOT(setValue(int)));
	connect(zoom_in_, SIGNAL(clicked()), this, SLOT(zoomInPressed()));
	connect(zoom_out_, SIGNAL(clicked()), this, SLOT(zoomOutPressed()));

	// QPalette palette = statusBar()->palette();

	zoom_value_ = new GuiClickableLabel(statusBar());
	connect(zoom_value_, SIGNAL(pressed()), this, SLOT(showZoomContextMenu()));
	// zoom_value_->setPalette(palette);
	zoom_value_->setForegroundRole(statusBar()->foregroundRole());
	zoom_value_->setFixedHeight(fm.height());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
	zoom_value_->setMinimumWidth(fm.horizontalAdvance("444M"));
#else
	zoom_value_->setMinimumWidth(fm.width("444M"));
#endif
	zoom_value_->setAlignment(Qt::AlignCenter);
	zoom_value_->setText(toqstr(bformat(_("[[ZOOM]]%1$d%"), zoom)));
	statusBar()->addPermanentWidget(zoom_value_);
	zoom_value_->setEnabled(currentBufferView());

	statusBar()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(statusBar(), SIGNAL(customContextMenuRequested(QPoint)),
		this, SLOT(showStatusBarContextMenu()));

	// enable pinch to zoom
	grabGesture(Qt::PinchGesture);

	QPixmap shellescape = QIcon(getPixmap("images/", "emblem-shellescape", "svgz,png")).pixmap(iconsize);
	shell_escape_ = new QLabel(statusBar());
	shell_escape_->setPixmap(shellescape);
	shell_escape_->setAlignment(Qt::AlignCenter);
	shell_escape_->setContextMenuPolicy(Qt::CustomContextMenu);
	shell_escape_->setToolTip(qt_("WARNING: LaTeX is allowed to execute "
	                              "external commands for this document. "
	                              "Right click to change."));
	SEMenu * menu = new SEMenu(this);
	connect(shell_escape_, SIGNAL(customContextMenuRequested(QPoint)),
		menu, SLOT(showMenu(QPoint)));
	shell_escape_->hide();
	statusBar()->addPermanentWidget(shell_escape_);

	QPixmap readonly = QIcon(getPixmap("images/", "emblem-readonly", "svgz,png")).pixmap(iconsize);
	read_only_ = new QLabel(statusBar());
	read_only_->setPixmap(readonly);
	read_only_->setAlignment(Qt::AlignCenter);
	read_only_->hide();
	statusBar()->addPermanentWidget(read_only_);

	version_control_ = new QLabel(statusBar());
	version_control_->setAlignment(Qt::AlignCenter);
	version_control_->setFrameStyle(QFrame::StyledPanel);
	version_control_->hide();
	statusBar()->addPermanentWidget(version_control_);

	statusBar()->setSizeGripEnabled(true);
	updateStatusBar();

	connect(&d.autosave_watcher_, SIGNAL(finished()), this,
		SLOT(autoSaveThreadFinished()));

	connect(&d.processing_thread_watcher_, SIGNAL(started()), this,
		SLOT(processingThreadStarted()));
	connect(&d.processing_thread_watcher_, SIGNAL(finished()), this,
		SLOT(processingThreadFinished()));

	connect(this, SIGNAL(triggerShowDialog(QString const &, QString const &, Inset *)),
		SLOT(doShowDialog(QString const &, QString const &, Inset *)));

	// Forbid too small unresizable window because it can happen
	// with some window manager under X11.
	setMinimumSize(300, 200);

	if (lyxrc.allow_geometry_session) {
		// Now take care of session management.
		if (restoreLayout())
			return;
	}

	// no session handling, default to a sane size.
	setGeometry(50, 50, 690, 510);
	initToolbars();

	// clear session data if any.
	settings.remove("views");
}


GuiView::~GuiView()
{
	delete &d;
}


void GuiView::disableShellEscape()
{
	BufferView * bv = documentBufferView();
	if (!bv)
		return;
	theSession().shellescapeFiles().remove(bv->buffer().absFileName());
	bv->buffer().params().shell_escape = false;
	bv->processUpdateFlags(Update::Force);
}


void GuiView::checkCancelBackground()
{
	docstring const ttl = _("Cancel Export?");
	docstring const msg = _("Do you want to cancel the background export process?");
	int const ret =
		Alert::prompt(ttl, msg, 1, 1,
			_("&Cancel export"), _("Co&ntinue"));
	if (ret == 0) {
		cancelExport();
	}
}

void GuiView::statsPressed()
{
	lyx::dispatch(FuncRequest(LFUN_STATISTICS));
}

void GuiView::zoomSliderMoved(int value)
{
	lyx::dispatch(FuncRequest(LFUN_BUFFER_ZOOM, convert<string>(value)));
	zoom_value_->setText(toqstr(bformat(_("[[ZOOM]]%1$d%"), value)));
	zoom_in_->setEnabled(currentBufferView()
			     && value < zoom_slider_->maximum());
	zoom_out_->setEnabled(currentBufferView()
			      && value > zoom_slider_->minimum());
}


void GuiView::zoomValueChanged(int value)
{
	if (value != lyxrc.currentZoom)
		zoomSliderMoved(value);
}


void GuiView::zoomInPressed()
{
	lyx::dispatch(FuncRequest(LFUN_BUFFER_ZOOM_IN));
}


void GuiView::zoomOutPressed()
{
	lyx::dispatch(FuncRequest(LFUN_BUFFER_ZOOM_OUT));
}


void GuiView::showZoomContextMenu()
{
	QMenu * menu = guiApp->menus().menu(toqstr("context-zoom"), * this);
	if (!menu)
		return;
	menu->exec(QCursor::pos());
}


void GuiView::showStatusBarContextMenu()
{
	QMenu * menu = guiApp->menus().menu(toqstr("context-statusbar"), * this);
	if (!menu)
		return;
	menu->exec(QCursor::pos());
}


void GuiView::scheduleRedrawWorkAreas()
{
	for (int i = 0; i < d.tabWorkAreaCount(); i++) {
		TabWorkArea* ta = d.tabWorkArea(i);
		for (int u = 0; u < ta->count(); u++) {
			ta->workArea(u)->scheduleRedraw(true);
		}
	}
}


QVector<GuiWorkArea*> GuiView::GuiViewPrivate::guiWorkAreas()
{
	QVector<GuiWorkArea*> areas;
	for (int i = 0; i < tabWorkAreaCount(); i++) {
		TabWorkArea* ta = tabWorkArea(i);
		for (int u = 0; u < ta->count(); u++) {
			areas << ta->workArea(u);
		}
	}
	return areas;
}

static void handleExportStatus(GuiView * view, Buffer::ExportStatus status,
	string const & format)
{
	docstring const fmt = translateIfPossible(theFormats().prettyName(format));
	docstring msg;
	switch (status) {
	case Buffer::ExportSuccess:
		msg = bformat(_("Successful export to format: %1$s"), fmt);
		break;
	case Buffer::ExportCancel:
		msg = _("Document export cancelled.");
		break;
	case Buffer::ExportError:
	case Buffer::ExportNoPathToFormat:
	case Buffer::ExportTexPathHasSpaces:
	case Buffer::ExportConverterError:
		msg = bformat(_("Error while exporting format: %1$s"), fmt);
		break;
	case Buffer::PreviewSuccess:
		msg = bformat(_("Successful preview of format: %1$s"), fmt);
		break;
	case Buffer::PreviewError:
		msg = bformat(_("Error while previewing format: %1$s"), fmt);
		break;
	case Buffer::ExportKilled:
		msg = bformat(_("Conversion cancelled while previewing format: %1$s"), fmt);
		break;
	}
	view->message(msg);
}


void GuiView::processingThreadStarted()
{
}


void GuiView::processingThreadFinished()
{
	QFutureWatcher<Buffer::ExportStatus> const * watcher =
		static_cast<QFutureWatcher<Buffer::ExportStatus> const *>(sender());

	Buffer::ExportStatus const status = watcher->result();
	handleExportStatus(this, status, d.processing_format);

	updateToolbars();
	BufferView const * const bv = currentBufferView();
	if (bv && !bv->buffer().errorList("Export").empty()) {
		errors("Export");
		return;
	}

	bool const error = (status != Buffer::ExportSuccess &&
			    status != Buffer::PreviewSuccess &&
			    status != Buffer::ExportCancel);
	if (error && bv) {
		ErrorList & el = bv->buffer().errorList(d.last_export_format);
		// at this point, we do not know if buffer-view or
		// master-buffer-view was called. If there was an export error,
		// and the current buffer's error log is empty, we guess that
		// it must be master-buffer-view that was called so we set
		// from_master=true.
		errors(d.last_export_format, el.empty());
	}
}


void GuiView::autoSaveThreadFinished()
{
	QFutureWatcher<docstring> const * watcher =
		static_cast<QFutureWatcher<docstring> const *>(sender());
	message(watcher->result());
	updateToolbars();
}


void GuiView::saveLayout() const
{
	QSettings settings;
	settings.setValue("zoom_ratio", zoom_ratio_);
	settings.setValue("devel_mode", devel_mode_);
	settings.beginGroup("views");
	settings.beginGroup(QString::number(id_));
	if (guiApp->platformName() == "xcb") {
		settings.setValue("pos", pos());
		settings.setValue("size", size());
	} else
		settings.setValue("geometry", saveGeometry());
	settings.setValue("layout", saveState(0));
	settings.setValue("icon_size", toqstr(d.iconSize(iconSize())));
	settings.setValue("zoom_value_visible", zoom_value_->isVisible());
	settings.setValue("zoom_slider_visible", zoom_widget_->isVisible());
	settings.setValue("word_count_enabled", word_count_enabled_);
	settings.setValue("char_count_enabled", char_count_enabled_);
	settings.setValue("char_nb_count_enabled", char_nb_count_enabled_);
}


void GuiView::saveUISettings() const
{
	QSettings settings;

	// Save the toolbar private states
	for (auto const & tb_p : d.toolbars_)
		tb_p.second->saveSession(settings);
	// Now take care of all other dialogs
	for (auto const & dlg_p : d.dialogs_)
		dlg_p.second->saveSession(settings);
}


void GuiView::setCurrentZoom(const int v)
{
	// Avoid (only theoretical) recursive call
	if (zoom_slider_->value() != v)
		Q_EMIT currentZoomChanged(v);
	lyxrc.currentZoom = v;
	zoom_value_->setText(toqstr(bformat(_("[[ZOOM]]%1$d%"), v)));
	zoom_in_->setEnabled(currentBufferView() && v < zoom_slider_->maximum());
	zoom_out_->setEnabled(currentBufferView() && v > zoom_slider_->minimum());
}


bool GuiView::restoreLayout()
{
	QSettings settings;
	zoom_ratio_ = settings.value("zoom_ratio", 1.0).toDouble();
	// Actual zoom value: default zoom + fractional offset
	int zoom = (int)(lyxrc.defaultZoom * zoom_ratio_);
	zoom = min(max(zoom, zoom_min_), zoom_max_);
	setCurrentZoom(zoom);
	devel_mode_ = settings.value("devel_mode", devel_mode_).toBool();
	settings.beginGroup("views");
	settings.beginGroup(QString::number(id_));
	QString const icon_key = "icon_size";
	if (!settings.contains(icon_key))
		return false;

	//code below is skipped when when ~/.config/LyX is (re)created
	setIconSize(d.iconSize(settings.value(icon_key).toString()));

	zoom_value_->setVisible(settings.value("zoom_value_visible", true).toBool());

	bool const show_zoom_slider = settings.value("zoom_slider_visible", true).toBool();
	zoom_widget_->setVisible(show_zoom_slider);

	word_count_enabled_ = settings.value("word_count_enabled", true).toBool();
	char_count_enabled_ = settings.value("char_count_enabled", true).toBool();
	char_nb_count_enabled_ = settings.value("char_nb_count_enabled", true).toBool();
	stat_counts_->setVisible(statsEnabled());

	if (guiApp->platformName() == "xcb") {
		QPoint pos = settings.value("pos", QPoint(50, 50)).toPoint();
		QSize size = settings.value("size", QSize(690, 510)).toSize();
		resize(size);
		move(pos);
	} else {
		// Work-around for bug #6034: the window ends up in an undetermined
		// state when trying to restore a maximized window when it is
		// already maximized.
		if (!(windowState() & Qt::WindowMaximized))
			if (!restoreGeometry(settings.value("geometry").toByteArray()))
				setGeometry(50, 50, 690, 510);
	}

	// Make sure layout is correctly oriented.
	setLayoutDirection(qApp->layoutDirection());

	// Allow the toc and view-source dock widget to be restored if needed.
	Dialog * dialog;
	if ((dialog = findOrBuild("toc", true)))
		// see bug 5082. At least setup title and enabled state.
		// Visibility will be adjusted by restoreState below.
		dialog->prepareView();
	if ((dialog = findOrBuild("view-source", true)))
		dialog->prepareView();
	if ((dialog = findOrBuild("progress", true)))
		dialog->prepareView();

	if (!restoreState(settings.value("layout").toByteArray(), 0))
		initToolbars();

	// init the toolbars that have not been restored
	for (auto const & tb_p : guiApp->toolbars()) {
		GuiToolbar * tb = toolbar(tb_p.name);
		if (tb && !tb->isRestored())
			initToolbar(tb_p.name);
	}

	// update lock (all) toolbars positions
	updateLockToolbars();

	updateDialogs();
	return true;
}


GuiToolbar * GuiView::toolbar(string const & name)
{
	ToolbarMap::iterator it = d.toolbars_.find(name);
	if (it != d.toolbars_.end())
		return it->second;

	LYXERR(Debug::GUI, "Toolbar::display: no toolbar named " << name);
	return nullptr;
}


void GuiView::updateLockToolbars()
{
	toolbarsMovable_ = false;
	for (ToolbarInfo const & info : guiApp->toolbars()) {
		GuiToolbar * tb = toolbar(info.name);
		if (tb && tb->isMovable())
			toolbarsMovable_ = true;
	}
	// set unified mac toolbars only when not movable as recommended:
	// https://doc.qt.io/qt-5/qmainwindow.html#unifiedTitleAndToolBarOnMac-prop
	setUnifiedTitleAndToolBarOnMac(!toolbarsMovable_);
}


void GuiView::constructToolbars()
{
	for (auto const & tb_p : d.toolbars_)
		delete tb_p.second;
	d.toolbars_.clear();

	// I don't like doing this here, but the standard toolbar
	// destroys this object when it's destroyed itself (vfr)
	d.layout_ = new LayoutBox(*this);
	d.stack_widget_->addWidget(d.layout_);
	d.layout_->move(0,0);

	// extracts the toolbars from the backend
	for (ToolbarInfo const & inf : guiApp->toolbars())
		d.toolbars_[inf.name] =  new GuiToolbar(inf, *this);

	DynamicMenuButton::resetIconCache();
}


void GuiView::initToolbars()
{
	// extracts the toolbars from the backend
	for (ToolbarInfo const & inf : guiApp->toolbars())
		initToolbar(inf.name);
}


void GuiView::initToolbar(string const & name)
{
	GuiToolbar * tb = toolbar(name);
	if (!tb)
		return;
	int const visibility = guiApp->toolbars().defaultVisibility(name);
	bool newline = !(visibility & Toolbars::SAMEROW);
	tb->setVisible(false);
	tb->setVisibility(visibility);

	if (visibility & Toolbars::TOP) {
		if (newline)
			addToolBarBreak(Qt::TopToolBarArea);
		addToolBar(Qt::TopToolBarArea, tb);
	}

	if (visibility & Toolbars::BOTTOM) {
		if (newline)
			addToolBarBreak(Qt::BottomToolBarArea);
		addToolBar(Qt::BottomToolBarArea, tb);
	}

	if (visibility & Toolbars::LEFT) {
		if (newline)
			addToolBarBreak(Qt::LeftToolBarArea);
		addToolBar(Qt::LeftToolBarArea, tb);
	}

	if (visibility & Toolbars::RIGHT) {
		if (newline)
			addToolBarBreak(Qt::RightToolBarArea);
		addToolBar(Qt::RightToolBarArea, tb);
	}

	if (visibility & Toolbars::ON)
		tb->setVisible(true);

	tb->setMovable(false);
}


TocModels & GuiView::tocModels()
{
	return d.toc_models_;
}


void GuiView::addColorItem(QString const & item, QString const & guiname,
			   QString const & category, QString const color,
			   bool const custom, int const r) const
{
	QList<QStandardItem *> row;
	QStandardItem * gui = new QStandardItem(guiname);
	// add color icon if applicable
	if (!color.isEmpty()) {
		QPixmap pixmap(32, 20);
		QColor col(color);
		pixmap.fill(col);
		QPainter painter(&pixmap);
		QRect pixmapRect = QRect(0, 0, 31, 19);
		painter.drawPixmap(pixmapRect.x(), pixmapRect.y(), pixmap);
		painter.drawRect(pixmapRect);
		gui->setIcon(QIcon(pixmap));
	}
	row.append(gui);
	row.append(new QStandardItem(item));
	row.append(new QStandardItem(category));
	row.append(new QStandardItem(custom ? QString("custom") : QString()));
	if (!color.isEmpty())
		row.append(new QStandardItem(color));

	// specified row
	if (r != -1) {
		colors_model_->insertRow(r, row);
		return;
	}

	int const end = colors_model_->rowCount();
	// first entry
	if (end == 0) {
		colors_model_->appendRow(row);
		return;
	}

	// find category
	int i = 0;
	// sort categories alphabetically
	while (i < end && colors_model_->item(i, 2)->text() != category)
		++i;

	// jump to the end of the category group to sort in order
	// specified in input
	while (i < end && colors_model_->item(i, 2)->text() == category)
		++i;

	colors_model_->insertRow(i, row);
}


QStandardItemModel * GuiView::viewColorsModel()
{
	if (colors_model_->rowCount() > 0)
		return colors_model_;

	// at first add the general values as required
	addColorItem("ignore", qt_("No change"));
	addColorItem("default", qt_("Default"));
	addColorItem("none", qt_("None[[color]]"));
	addColorItem("inherit", qt_("(Without)[[color]]"));
	// then custom colors
	if (currentBufferView()) {
		for (auto const & lc : currentBufferView()->buffer().masterParams().custom_colors) {
			addColorItem(toqstr(lc.first),
				     toqstr(lc.first),
				     qt_("Custom Colors"),
				     toqstr(lc.second),
				     true);
		}
	}
	// finally the latex colors
	for (auto const & lc : theLaTeXColors().getLaTeXColors()) {
		addColorItem(toqstr(lc.first),
			     toqstr(translateIfPossible(lc.second.guiname())),
			     toqstr(translateIfPossible(lc.second.category())),
			     toqstr(lc.second.hexname()));
	}

	return colors_model_;
}


void GuiView::updateColorsModel() const
{
	bool changed = false;
	// remove custom colors
	QList<QStandardItem *> cis = colors_model_->findItems("custom", Qt::MatchExactly, 3);
	for (int i = 0; i < cis.size(); ++i) {
		colors_model_->removeRow(cis.at(i)->row());
		changed = true;
	}
	// and add anew
	if (currentBufferView()) {
		int r = 4;
		for (auto const & lc : currentBufferView()->buffer().masterParams().custom_colors) {
			addColorItem(toqstr(lc.first),
				     toqstr(lc.first),
				     qt_("Custom Colors"),
				     toqstr(lc.second),
				     true, r);
			++r;
			changed = true;
		}
	}
	if (changed)
		Q_EMIT colorsModelChanged();
}


void GuiView::setFocus()
{
	LYXERR(Debug::DEBUG, "GuiView::setFocus()" << this);
	QMainWindow::setFocus();
}


void GuiView::setFocus(Qt::FocusReason reason)
{
	LYXERR(Debug::DEBUG, "GuiView::setFocus(): " << this << " reason = " << reason);
	QMainWindow::setFocus(reason);
}


bool GuiView::hasFocus() const
{
	if (currentWorkArea())
		return currentWorkArea()->hasFocus();
	if (currentMainWorkArea())
		return currentMainWorkArea()->hasFocus();
	return d.bg_widget_->hasFocus();
}


void GuiView::focusInEvent(QFocusEvent * e)
{
	LYXERR(Debug::DEBUG, "GuiView::focusInEvent(): " << this << " reason = " << e->reason());
	QMainWindow::focusInEvent(e);
	// Make sure guiApp points to the correct view.
	guiApp->setCurrentView(this);
	if (currentWorkArea())
		currentWorkArea()->setFocus(e->reason());
	else if (currentMainWorkArea())
		currentMainWorkArea()->setFocus(e->reason());
	else
		d.bg_widget_->setFocus(e->reason());
}


void GuiView::showEvent(QShowEvent * e)
{
	LYXERR(Debug::GUI, "Passed Geometry "
		<< size().height() << "x" << size().width()
		<< "+" << pos().x() << "+" << pos().y());

	if (d.splitter_->count() == 0)
		// No work area, switch to the background widget.
		d.setBackground();

	updateToolbars();
	QMainWindow::showEvent(e);
}


bool GuiView::closeScheduled()
{
	closing_ = true;
	return close();
}


bool GuiView::prepareAllBuffersForLogout()
{
	Buffer * first = theBufferList().first();
	if (!first)
		return true;

	// First, iterate over all buffers and ask the users if unsaved
	// changes should be saved.
	// We cannot use a for loop as the buffer list cycles.
	Buffer * b = first;
	do {
		if (!saveBufferIfNeeded(*b, false))
			return false;
		b = theBufferList().next(b);
	} while (b != first);

	// Next, save session state
	// When a view/window was closed before without quitting LyX, there
	// are already entries in the lastOpened list.
	theSession().lastOpened().clear();
	writeSession();

	return true;
}


/** Destroy only all tabbed WorkAreas. Destruction of other WorkAreas
 ** is responsibility of the container (e.g., dialog)
 **/
void GuiView::closeEvent(QCloseEvent * close_event)
{
	LYXERR(Debug::DEBUG, "GuiView::closeEvent()");

	// FIXME Bug #12828 bites here. If there is some other View open, then
	// we really should only refuse to close if one of the Buffers open here
	// is being processed.
	if (!GuiViewPrivate::busyBuffers.isEmpty()) {
		Alert::warning(_("Exit LyX"),
			_("LyX could not be closed because documents are being processed by LyX."));
		close_event->setAccepted(false);
		return;
	}

	// If the user pressed the x (so we didn't call closeView
	// programmatically), we want to clear all existing entries.
	if (!closing_)
		theSession().lastOpened().clear();
	closing_ = true;

	writeSession();

	// it can happen that this event arrives without selecting the view,
	// e.g. when clicking the close button on a background window.
	setFocus();
	Q_EMIT closing(id_);
	if (!closeWorkAreaAll()) {
		closing_ = false;
		close_event->ignore();
		return;
	}

	// Make sure that nothing will use this to be closed View.
	guiApp->unregisterView(this);

	if (isFullScreen()) {
		// Switch off fullscreen before closing.
		toggleFullScreen();
		updateDialogs();
	}

	// Make sure the timer time out will not trigger a statusbar update.
	d.statusbar_timer_.stop();
	d.statusbar_stats_timer_.stop();

	// Saving fullscreen requires additional tweaks in the toolbar code.
	// It wouldn't also work under linux natively.
	if (lyxrc.allow_geometry_session) {
		saveLayout();
		saveUISettings();
	}

	close_event->accept();
}


void GuiView::dragEnterEvent(QDragEnterEvent * event)
{
	if (event->mimeData()->hasUrls())
		event->accept();
	/// \todo Ask lyx-devel is this is enough:
	/// if (event->mimeData()->hasFormat("text/plain"))
	///	event->acceptProposedAction();
}


void GuiView::dropEvent(QDropEvent * event)
{
	QList<QUrl> files = event->mimeData()->urls();
	if (files.isEmpty())
		return;

	LYXERR(Debug::GUI, "GuiView::dropEvent: got URLs!");
	for (int i = 0; i != files.size(); ++i) {
		string const file = os::internal_path(fromqstr(
			files.at(i).toLocalFile()));
		if (file.empty())
			continue;

		string const ext = support::getExtension(file);
		vector<const Format *> found_formats;

		// Find all formats that have the correct extension.
		for (const Format * fmt : theConverters().importableFormats())
			if (fmt->hasExtension(ext))
				found_formats.push_back(fmt);

		FuncRequest cmd;
		if (!found_formats.empty()) {
			if (found_formats.size() > 1) {
				//FIXME: show a dialog to choose the correct importable format
				LYXERR(Debug::FILES,
					"Multiple importable formats found, selecting first");
			}
			string const arg = found_formats[0]->name() + " " + file;
			cmd = FuncRequest(LFUN_BUFFER_IMPORT, arg);
		}
		else {
			//FIXME: do we have to explicitly check whether it's a lyx file?
			LYXERR(Debug::FILES,
				"No formats found, trying to open it as a lyx file");
			cmd = FuncRequest(LFUN_FILE_OPEN, file);
		}
		// add the functions to the queue
		guiApp->addToFuncRequestQueue(cmd);
		event->accept();
	}
	// now process the collected functions. We perform the events
	// asynchronously. This prevents potential problems in case the
	// BufferView is closed within an event.
	guiApp->processFuncRequestQueueAsync();
}


void GuiView::message(docstring const & str)
{
	if (ForkedProcess::iAmAChild())
		return;

	// call is moved to GUI-thread by GuiProgress
	d.progress_->appendMessage(toqstr(str));
}


void GuiView::clearMessageText()
{
	message(docstring());
}


void GuiView::updateStatusBarMessage(QString const & str)
{
	statusBar()->showMessage(str);
	d.statusbar_timer_.stop();
	d.statusbar_timer_.start(3000);
}


void GuiView::clearMessage()
{
	// FIXME: This code was introduced in r19643 to fix bug #4123. However,
	// the hasFocus function mostly returns false, even if the focus is on
	// a workarea in this view.
	//if (!hasFocus())
	//	return;
	showMessage();
	d.statusbar_timer_.stop();
}

void GuiView::showStats()
{
	if (!statsEnabled())
		return;

	d.time_to_update -= d.timer_rate;

	BufferView * bv = currentBufferView();
	Buffer * buf = bv ? &bv->buffer() : nullptr;
	if (!buf) {
		stat_counts_->hide();
		return;
	}

	// UI toggle, buffer change, etc
	if (bv->stats_update_trigger() || d.stats_update_trigger_) {
		d.stats_update_trigger_ = false;
		d.time_to_update = 0;
	}

	Cursor const & cur = bv->cursor();

	// we start new selection and need faster update
	if (!d.already_in_selection_ && cur.selection())
		d.time_to_update = 0;

	if (d.time_to_update > 0)
		return;

	// Don't attempt to calculate stats if
	// the buffer is busy as this might crash (#12935)
	Statistics & statistics = buf->statistics();
	if (!busy() && !bv->busy())
		statistics.update(cur);

	QStringList stats;
	if (word_count_enabled_) {
		int const words = statistics.word_count - bv->stats_ref_value_w();
		if (words == 1)
			stats << toqstr(bformat(_("%1$d Word"), words));
		else
			stats << toqstr(bformat(_("%1$d Words"), words));
	}
	int const chars_with_blanks = statistics.char_count + statistics.blank_count;
	if (char_count_enabled_) {
		int const chars_with_blanks_disp = chars_with_blanks - bv->stats_ref_value_c();
		if (chars_with_blanks == 1)
			stats << toqstr(bformat(_("%1$d Character"), chars_with_blanks_disp));
		else
			stats << toqstr(bformat(_("%1$d Characters"), chars_with_blanks_disp));
	}
	if (char_nb_count_enabled_) {
		int const chars = statistics.char_count - bv->stats_ref_value_nb();
		if (chars == 1)
			stats << toqstr(bformat(_("%1$d Character (no Blanks)"), chars));
		else
			stats << toqstr(bformat(_("%1$d Characters (no Blanks)"), chars));
	}
	// we need to add space before and after manually, using stylesheet
	// would break with dark mode on Qt >= 6.8.
	stat_counts_->setText(" " + stats.join(qt_(", [[stats separator]]")) + " ");
	stat_counts_->show();

	d.time_to_update = d.default_stats_rate;
	// fast updates for small selections
	if (chars_with_blanks < d.max_sel_chars && cur.selection())
		d.time_to_update = d.timer_rate;
}


void GuiView::updateWindowTitle(GuiWorkArea * wa)
{
	if (wa != d.current_work_area_
		|| wa->bufferView().buffer().isInternal())
		return;
	Buffer const & buf = wa->bufferView().buffer();
	// Set the windows title
	docstring title = buf.fileName().displayName(130) + from_ascii("[*]");
	if (buf.notifiesExternalModification()) {
		title = bformat(_("%1$s (modified externally)"), title);
		// If the external modification status has changed, then maybe the status of
		// buffer-save has changed too.
		updateToolbars();
	}
	title += from_ascii(" - LyX");
	setWindowTitle(toqstr(title));
	// Sets the path for the window: this is used by macOS to
	// allow a context click on the title bar showing a menu
	// with the path up to the file
	setWindowFilePath(toqstr(buf.absFileName()));
	// Tell Qt whether the current document is changed
	setWindowModified(!buf.isClean());

	if (buf.params().shell_escape)
		shell_escape_->show();
	else
		shell_escape_->hide();

	if (buf.hasReadonlyFlag())
		read_only_->show();
	else
		read_only_->hide();

	if (buf.lyxvc().inUse()) {
		version_control_->show();
		version_control_->setText(toqstr(buf.lyxvc().vcstatus()));
	} else
		version_control_->hide();
}


void GuiView::on_currentWorkAreaChanged(GuiWorkArea * wa)
{
	if (d.current_work_area_) {
		// disconnect the current work area from all slots
		QObject::disconnect(d.current_work_area_, nullptr, this, nullptr);
	}
	disconnectBuffer();
	disconnectBufferView();
	connectBufferView(wa->bufferView());
	connectBuffer(wa->bufferView().buffer());
	d.current_work_area_ = wa;
	// The below specifies that the input method item transformation will
	// not reset
	wa->setFocus(Qt::OtherFocusReason);
	QObject::connect(wa, SIGNAL(titleChanged(GuiWorkArea *)),
	                 this, SLOT(updateWindowTitle(GuiWorkArea *)));
	QObject::connect(wa, SIGNAL(busy(bool)),
	                 this, SLOT(setBusy(bool)));
	// connection of a signal to a signal
	QObject::connect(wa, SIGNAL(bufferViewChanged()),
	                 this, SIGNAL(bufferViewChanged()));
	Q_EMIT updateWindowTitle(wa);
	Q_EMIT bufferViewChanged();
}


void GuiView::onBufferViewChanged()
{
	structureChanged();
	// Buffer-dependent dialogs must be updated. This is done here because
	// some dialogs require buffer()->text.
	updateDialogs();
	zoom_slider_->setEnabled(currentBufferView());
	zoom_value_->setEnabled(currentBufferView());
	zoom_in_->setEnabled(currentBufferView()
			     && zoom_slider_->value() < zoom_slider_->maximum());
	zoom_out_->setEnabled(currentBufferView()
			      && zoom_slider_->value() > zoom_slider_->minimum());
	d.stats_update_trigger_ = true;
	if (!closing_)
		updateColorsModel();
}


void GuiView::on_lastWorkAreaRemoved()
{
	if (closing_)
		// We already are in a close event. Nothing more to do.
		return;

	if (d.splitter_->count() > 1)
		// We have a splitter so don't close anything.
		return;

	// Reset and updates the dialogs.
	Q_EMIT bufferViewChanged();

	resetWindowTitle();
	updateStatusBar();

	if (lyxrc.open_buffers_in_tabs)
		// Nothing more to do, the window should stay open.
		return;

	if (guiApp->viewIds().size() > 1) {
		close();
		return;
	}

#ifdef Q_OS_MAC
	// On Mac we also close the last window because the application stay
	// resident in memory. On other platforms we don't close the last
	// window because this would quit the application.
	close();
#endif
}


void GuiView::updateStatusBar()
{
	// let the user see the explicit message
	if (d.statusbar_timer_.isActive())
		return;

	showMessage();
}


void GuiView::showMessage()
{
	if (busy_)
		return;
	QString msg = toqstr(theGuiApp()->viewStatusMessage());
	if (msg.isEmpty()) {
		BufferView const * bv = currentBufferView();
		if (bv)
			msg = toqstr(bv->cursor().currentState(devel_mode_));
		else
			msg = qt_("Welcome to LyX!");
	}
	statusBar()->showMessage(msg);
}


bool GuiView::statsEnabled() const
{
	return word_count_enabled_ || char_count_enabled_ || char_nb_count_enabled_;
}


bool GuiView::event(QEvent * e)
{
	switch (e->type())
	{
	// Useful debug code:
	//case QEvent::ActivationChange:
	//case QEvent::WindowDeactivate:
	//case QEvent::Paint:
	//case QEvent::Enter:
	//case QEvent::Leave:
	//case QEvent::HoverEnter:
	//case QEvent::HoverLeave:
	//case QEvent::HoverMove:
	//case QEvent::StatusTip:
	//case QEvent::DragEnter:
	//case QEvent::DragLeave:
	//case QEvent::Drop:
	//	break;

	case QEvent::WindowStateChange: {
		QWindowStateChangeEvent * ev = (QWindowStateChangeEvent*)e;
		bool ofstate = (ev->oldState() & Qt::WindowFullScreen);
		bool result = QMainWindow::event(e);
		bool nfstate = (windowState() & Qt::WindowFullScreen);
		if (!ofstate && nfstate) {
			LYXERR(Debug::DEBUG, "GuiView: WindowStateChange(): full-screen " << nfstate);
			// switch to full-screen state
			if (lyxrc.full_screen_statusbar)
				statusBar()->hide();
			if (lyxrc.full_screen_menubar)
				menuBar()->hide();
			if (lyxrc.full_screen_toolbars) {
				for (auto const & tb_p  : d.toolbars_)
					if (tb_p.second->isVisibilityOn() && tb_p.second->isVisible())
						tb_p.second->hide();
			}
			for (int i = 0; i != d.splitter_->count(); ++i)
				d.tabWorkArea(i)->setFullScreen(true);
			// Safe area measures won't allow us to go negative in margins
			setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
			setContentsMargins(-2, -2, -2, -2);
			// bug 5274
			hideDialogs("prefs", nullptr);
		} else if (ofstate && !nfstate) {
			LYXERR(Debug::DEBUG, "GuiView: WindowStateChange(): full-screen " << nfstate);
			// switch back from full-screen state
			if (lyxrc.full_screen_statusbar && !statusBar()->isVisible())
				statusBar()->show();
			if (lyxrc.full_screen_menubar && !menuBar()->isVisible())
				menuBar()->show();
			if (lyxrc.full_screen_toolbars) {
				for (auto const & tb_p  : d.toolbars_)
					if (tb_p.second->isVisibilityOn() && !tb_p.second->isVisible())
						tb_p.second->show();
				//updateToolbars();
			}
			for (int i = 0; i != d.splitter_->count(); ++i)
				d.tabWorkArea(i)->setFullScreen(false);
			setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, true);
			setContentsMargins(0, 0, 0, 0);
		}
		return result;
	}

	case QEvent::WindowActivate: {
		GuiView * old_view = guiApp->currentView();
		if (this == old_view) {
			setFocus(Qt::ActiveWindowFocusReason);
			return QMainWindow::event(e);
		}
		if (old_view && old_view->currentBufferView()) {
			// save current selection to the selection buffer to allow
			// middle-button paste in this window.
			cap::saveSelection(old_view->currentBufferView()->cursor());
		}
		guiApp->setCurrentView(this);
		if (d.current_work_area_)
			on_currentWorkAreaChanged(d.current_work_area_);
		else
			resetWindowTitle();
		setFocus(Qt::ActiveWindowFocusReason);
		return QMainWindow::event(e);
	}

	case QEvent::ShortcutOverride: {
		// See bug 4888
		if (isFullScreen() && menuBar()->isHidden()) {
			QKeyEvent * ke = static_cast<QKeyEvent*>(e);
			// FIXME: we should also try to detect special LyX shortcut such as
			// Alt-P and Alt-M. Right now there is a hack in
			// GuiWorkArea::processKeySym() that hides again the menubar for
			// those cases.
			if (ke->modifiers() & Qt::AltModifier && ke->key() != Qt::Key_Alt) {
				menuBar()->show();
				return QMainWindow::event(e);
			}
		}
		return QMainWindow::event(e);
	}

	case QEvent::Gesture: {
		QGestureEvent *ge = static_cast<QGestureEvent*>(e);
		QGesture *gp = ge->gesture(Qt::PinchGesture);
		if (gp) {
			QPinchGesture *pinch = static_cast<QPinchGesture *>(gp);
			QPinchGesture::ChangeFlags changeFlags = pinch->changeFlags();
			qreal totalScaleFactor = pinch->totalScaleFactor();
			LYXERR(Debug::GUI, "totalScaleFactor: " << totalScaleFactor);
			if (pinch->state() == Qt::GestureStarted) {
				initialZoom_ = lyxrc.currentZoom;
				LYXERR(Debug::GUI, "initialZoom_: " << initialZoom_);
			}
			if (changeFlags & QPinchGesture::ScaleFactorChanged) {
				qreal factor = initialZoom_ * totalScaleFactor;
				LYXERR(Debug::GUI, "scaleFactor: " << factor);
				zoomValueChanged(factor);
			}
		}
		return QMainWindow::event(e);
	}

	// dark/light mode runtime switch support
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	case QEvent::ThemeChange: {
		if (lyxrc.color_scheme != "dark" && lyxrc.color_scheme != "light") {
			guiApp->setPalette(guiApp->style()->standardPalette());
			// We need to update metrics here to avoid a crash (#12786)
			theBufferList().changed(true);
			refillToolbars();
			return QMainWindow::event(e);
		}
		return true;
	}
#else
	// Pre 6.8: OS-dependent
	// 1. macOS
	// Limit to Q_OS_MAC as this unnecessarily would also
	// trigger on Linux with grave performance issues
#ifdef Q_OS_MAC
	case QEvent::ApplicationPaletteChange: {
		// We need to update metrics here to avoid a crash (#12786)
		theBufferList().changed(true);
		refillToolbars();
		return QMainWindow::event(e);
	}
#endif
	// 2. Linux
	case QEvent::StyleChange: {
		// We need to update metrics here to avoid a crash (#12786)
		theBufferList().changed(true);
		refillToolbars();
		return QMainWindow::event(e);
	}
#endif

	default:
		return QMainWindow::event(e);
	}
}

void GuiView::resetWindowTitle()
{
	setWindowTitle(qt_("LyX"));
}

bool GuiView::focusNextPrevChild(bool /*next*/)
{
	setFocus();
	return true;
}


bool GuiView::busy() const
{
	return busy_ > 0;
}


void GuiView::setBusy(bool busy)
{
	bool const busy_before = busy_ > 0;
	busy ? ++busy_ : --busy_;
	if ((busy_ > 0) == busy_before)
		// busy state didn't change
		return;

	if (busy) {
		QApplication::setOverrideCursor(Qt::WaitCursor);
		return;
	}
	QApplication::restoreOverrideCursor();
	updateLayoutList();
}


void GuiView::resetCommandExecute()
{
	command_execute_ = false;
	updateToolbars();
}


double GuiView::pixelRatio() const
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	return devicePixelRatioF();
#else
	return devicePixelRatio();
#endif
}


GuiWorkArea * GuiView::workArea(int index)
{
	if (TabWorkArea * twa = d.currentTabWorkArea())
		if (index < twa->count())
			return twa->workArea(index);
	return nullptr;
}


GuiWorkArea * GuiView::workArea(Buffer & buffer)
{
	if (currentWorkArea()
		&& &currentWorkArea()->bufferView().buffer() == &buffer)
		return currentWorkArea();
	if (TabWorkArea * twa = d.currentTabWorkArea())
		return twa->workArea(buffer);
	return nullptr;
}


GuiWorkArea * GuiView::addWorkArea(Buffer & buffer)
{
	// Automatically create a TabWorkArea if there are none yet.
	TabWorkArea * tab_widget = d.splitter_->count()
		? d.currentTabWorkArea() : addTabWorkArea();
	return tab_widget->addWorkArea(buffer, *this);
}


TabWorkArea * GuiView::addTabWorkArea()
{
	TabWorkArea * twa = new TabWorkArea;
	QObject::connect(twa, SIGNAL(currentWorkAreaChanged(GuiWorkArea *)),
		this, SLOT(on_currentWorkAreaChanged(GuiWorkArea *)));
	QObject::connect(twa, SIGNAL(lastWorkAreaRemoved()),
			 this, SLOT(on_lastWorkAreaRemoved()));

	d.splitter_->insertWidget(d.splitter_->indexOf(d.currentTabWorkArea()) + 1,
	                          twa);
	d.stack_widget_->setCurrentWidget(d.splitter_);
	return twa;
}


GuiWorkArea const * GuiView::currentWorkArea() const
{
	return d.current_work_area_;
}


TabWorkArea * GuiView::currentTabWorkArea() const
{
	return d.currentTabWorkArea();
}


GuiWorkArea * GuiView::currentWorkArea()
{
	return d.current_work_area_;
}


GuiWorkArea const * GuiView::currentMainWorkArea() const
{
	if (!d.currentTabWorkArea())
		return nullptr;
	return d.currentTabWorkArea()->currentWorkArea();
}


GuiWorkArea * GuiView::currentMainWorkArea()
{
	if (!d.currentTabWorkArea())
		return nullptr;
	return d.currentTabWorkArea()->currentWorkArea();
}


void GuiView::setCurrentWorkArea(GuiWorkArea * wa)
{
	LYXERR(Debug::DEBUG, "Setting current wa: " << wa << endl);
	if (!wa) {
		d.current_work_area_ = nullptr;
		d.setBackground();
		Q_EMIT bufferViewChanged();
		return;
	}

	// FIXME: I've no clue why this is here and why it accesses
	//  theGuiApp()->currentView, which might be 0 (bug 6464).
	//  See also 27525 (vfr).
	if (theGuiApp()->currentView() == this
		  && theGuiApp()->currentView()->currentWorkArea() == wa)
		return;

	if (currentBufferView())
		cap::saveSelection(currentBufferView()->cursor());

	theGuiApp()->setCurrentView(this);
	d.current_work_area_ = wa;

	// We need to reset this now, because it will need to be
	// right if the tabWorkArea gets reset in the for loop. We
	// will change it back if we aren't in that case.
	GuiWorkArea * const old_cmwa = d.current_main_work_area_;
	d.current_main_work_area_ = wa;

	for (int i = 0; i != d.splitter_->count(); ++i) {
		if (d.tabWorkArea(i)->setCurrentWorkArea(wa)) {
			LYXERR(Debug::DEBUG, "Current wa: " << currentWorkArea()
				<< ", Current main wa: " << currentMainWorkArea());
			return;
		}
	}

	d.current_main_work_area_ = old_cmwa;

	LYXERR(Debug::DEBUG, "This is not a tabbed wa");
	on_currentWorkAreaChanged(wa);
	BufferView & bv = wa->bufferView();
	bv.cursor().fixIfBroken();
	bv.updateMetrics();
	wa->setUpdatesEnabled(true);
	LYXERR(Debug::DEBUG, "Current wa: " << currentWorkArea() << ", Current main wa: " << currentMainWorkArea());
}


void GuiView::removeWorkArea(GuiWorkArea * wa)
{
	LASSERT(wa, return);
	if (wa == d.current_work_area_) {
		disconnectBuffer();
		disconnectBufferView();
		d.current_work_area_ = nullptr;
		d.current_main_work_area_ = nullptr;
	}

	bool found_twa = false;
	for (int i = 0; i != d.splitter_->count(); ++i) {
		TabWorkArea * twa = d.tabWorkArea(i);
		if (twa->removeWorkArea(wa)) {
			// Found in this tab group, and deleted the GuiWorkArea.
			found_twa = true;
			if (twa->count() != 0) {
				if (d.current_work_area_ == nullptr)
					// This means that we are closing the current GuiWorkArea, so
					// switch to the next GuiWorkArea in the found TabWorkArea.
					setCurrentWorkArea(twa->currentWorkArea());
			} else {
				// No more WorkAreas in this tab group, so delete it.
				delete twa;
			}
			break;
		}
	}

	// It is not a tabbed work area (i.e., the search work area), so it
	// should be deleted by other means.
	LASSERT(found_twa, return);

	if (d.current_work_area_ == nullptr) {
		if (d.splitter_->count() != 0) {
			TabWorkArea * twa = d.currentTabWorkArea();
			setCurrentWorkArea(twa->currentWorkArea());
		} else {
			// No more work areas, switch to the background widget.
			setCurrentWorkArea(nullptr);
		}
	}
}


bool GuiView::hasVisibleWorkArea(GuiWorkArea * wa) const
{
	for (int i = 0; i < d.splitter_->count(); ++i)
		if (d.tabWorkArea(i)->currentWorkArea() == wa)
			return true;

	FindAndReplace * fr = static_cast<FindAndReplace*>(find("findreplaceadv", false));
	return fr->isVisible() && fr->hasWorkArea(wa);
}


LayoutBox * GuiView::getLayoutDialog() const
{
	return d.layout_;
}


void GuiView::updateLayoutList()
{
	if (d.layout_)
		d.layout_->updateContents(false);
}


void GuiView::updateToolbars()
{
	if (d.current_work_area_) {
		int context = 0;
		if (d.current_work_area_->bufferView().cursor().inMathed()
			&& !d.current_work_area_->bufferView().cursor().inRegexped())
			context |= Toolbars::MATH;
		if (lyx::getStatus(FuncRequest(LFUN_LAYOUT_TABULAR)).enabled())
			context |= Toolbars::TABLE;
		if (currentBufferView()->buffer().areChangesPresent()
		    || (lyx::getStatus(FuncRequest(LFUN_CHANGES_TRACK)).enabled()
		        && lyx::getStatus(FuncRequest(LFUN_CHANGES_TRACK)).onOff(true))
		    || (lyx::getStatus(FuncRequest(LFUN_CHANGES_OUTPUT)).enabled()
		        && lyx::getStatus(FuncRequest(LFUN_CHANGES_OUTPUT)).onOff(true)))
			context |= Toolbars::REVIEW;
		if (lyx::getStatus(FuncRequest(LFUN_IN_MATHMACROTEMPLATE)).enabled())
			context |= Toolbars::MATHMACROTEMPLATE;
		if (lyx::getStatus(FuncRequest(LFUN_IN_IPA)).enabled())
			context |= Toolbars::IPA;
		if (command_execute_)
			context |= Toolbars::MINIBUFFER;
		if (minibuffer_focus_) {
			context |= Toolbars::MINIBUFFER_FOCUS;
			minibuffer_focus_ = false;
		}

		for (auto const & tb_p : d.toolbars_)
			tb_p.second->update(context);
	} else
		for (auto const & tb_p : d.toolbars_)
			tb_p.second->update();
}


void GuiView::refillToolbars()
{
	DynamicMenuButton::resetIconCache();
	for (auto const & tb_p : d.toolbars_)
		tb_p.second->refill();
}


void GuiView::setBuffer(Buffer * newBuffer, bool switch_to)
{
	LYXERR(Debug::DEBUG, "Setting buffer: " << newBuffer << endl);
	LASSERT(newBuffer, return);

	GuiWorkArea * wa = workArea(*newBuffer);
	if (wa == nullptr) {
		setBusy(true);
		newBuffer->masterBuffer()->updateBuffer();
		setBusy(false);
		wa = addWorkArea(*newBuffer);
		wa->resizeBufferView();
		// scroll to the position when the BufferView was last closed
		if (lyxrc.use_lastfilepos) {
			LastFilePosSection::FilePos filepos =
				theSession().lastFilePos().load(newBuffer->fileName());
			wa->bufferView().moveToPosition(filepos.pit, filepos.pos, 0, 0);
		}
	} else {
		//Disconnect the old buffer...there's no new one.
		disconnectBuffer();
	}
	connectBuffer(*newBuffer);
	connectBufferView(wa->bufferView());
	if (switch_to)
		setCurrentWorkArea(wa);
}


void GuiView::connectBuffer(Buffer & buf)
{
	buf.setGuiDelegate(this);
}


void GuiView::disconnectBuffer()
{
	if (d.current_work_area_)
		d.current_work_area_->bufferView().buffer().setGuiDelegate(nullptr);
}


void GuiView::connectBufferView(BufferView & bv)
{
	bv.setGuiDelegate(this);
}


void GuiView::disconnectBufferView()
{
	if (d.current_work_area_)
		d.current_work_area_->bufferView().setGuiDelegate(nullptr);
}


void GuiView::errors(string const & error_type, bool from_master, int const item)
{
	BufferView const * const bv = currentBufferView();
	if (!bv)
		return;

	ErrorList const & el = from_master ?
		bv->buffer().masterBuffer()->errorList(error_type) :
		bv->buffer().errorList(error_type);

	if (el.empty())
		return;

	string err = error_type;
	if (from_master)
		err = "from_master|" + error_type;
	if (item != -1)
		err += "@" + convert<string>(item);
	showDialog("errorlist", err);
}


int GuiView::nextError(string const & error_type, bool from_master,
		       bool navigateto, bool atcursor)
{
	BufferView const * const bv = currentBufferView();
	if (!bv)
		return -1;

	Buffer const & buf = from_master
			? *(bv->buffer().masterBuffer())
			: bv->buffer();

	ErrorList const & el = buf.errorList(error_type);

	if (el.empty())
		return -1;

	int item = 0;
	for (auto const & err : el) {
		if (TexRow::isNone(err.start)
		    || (atcursor && TexRow::getDocIteratorsFromEntries(err.start, err.end, buf).first < bv->cursor())
		    || (!atcursor && TexRow::getDocIteratorsFromEntries(err.start, err.end, buf).first <= bv->cursor())) {
			++item;
			continue;
		}
		if (navigateto) {
			lyx::dispatch(TexRow::goToFunc(err.start, err.end));
		}
		return item;
	}

	return -1;
}


void GuiView::updateTocItem(string const & type, DocIterator const & dit)
{
	d.toc_models_.updateItem(toqstr(type), dit);
}


void GuiView::structureChanged()
{
	// This is called from the Buffer, which has no way to ensure that cursors
	// in BufferView remain valid.
	if (documentBufferView())
		documentBufferView()->cursor().sanitize();
	// FIXME: This is slightly expensive, though less than the tocBackend update
	// (#9880). This also resets the view in the Toc Widget (#6675).
	d.toc_models_.reset(documentBufferView());
	// Navigator needs more than a simple update in this case. It needs to be
	// rebuilt.
	updateDialog("toc", "");
}


void GuiView::updateDialog(string const & name, string const & sdata)
{
	if (!isDialogVisible(name))
		return;

	map<string, DialogPtr>::const_iterator it = d.dialogs_.find(name);
	if (it == d.dialogs_.end())
		return;

	Dialog * const dialog = it->second.get();
	if (dialog->isVisibleView())
		dialog->initialiseParams(sdata);
}


BufferView * GuiView::documentBufferView()
{
	return currentMainWorkArea()
		? &currentMainWorkArea()->bufferView()
		: nullptr;
}


BufferView const * GuiView::documentBufferView() const
{
	return currentMainWorkArea()
		? &currentMainWorkArea()->bufferView()
		: nullptr;
}


BufferView * GuiView::currentBufferView()
{
	return d.current_work_area_ ? &d.current_work_area_->bufferView() : nullptr;
}


BufferView const * GuiView::currentBufferView() const
{
	return d.current_work_area_ ? &d.current_work_area_->bufferView() : nullptr;
}


docstring GuiView::GuiViewPrivate::autosaveAndDestroy(
	Buffer const * orig, Buffer * clone)
{
	bool const success = clone->autoSave();
	delete clone;
	busyBuffers.remove(orig);
	return success
		? _("Automatic save done.")
		: _("Automatic save failed!");
}


void GuiView::autoSave()
{
	LYXERR(Debug::INFO, "Running autoSave()");

	Buffer * buffer = documentBufferView()
		? &documentBufferView()->buffer() : nullptr;
	if (!buffer) {
		resetAutosaveTimers();
		return;
	}

	GuiViewPrivate::busyBuffers.insert(buffer);
	QFuture<docstring> f = QtConcurrent::run(GuiViewPrivate::autosaveAndDestroy,
		buffer, buffer->cloneBufferOnly());
	d.autosave_watcher_.setFuture(f);
	resetAutosaveTimers();
}


void GuiView::resetAutosaveTimers()
{
	if (lyxrc.autosave)
		d.autosave_timeout_.restart();
}


namespace {

double zoomRatio(FuncRequest const & cmd, double const zr)
{
	if (cmd.argument().empty()) {
		if (cmd.action() == LFUN_BUFFER_ZOOM)
			return 1.0;
		else if (cmd.action() == LFUN_BUFFER_ZOOM_IN)
			return zr + 0.1;
		else // cmd.action() == LFUN_BUFFER_ZOOM_OUT
			return zr - 0.1;
	} else {
		if (cmd.action() == LFUN_BUFFER_ZOOM)
			return convert<int>(cmd.argument()) / double(lyxrc.defaultZoom);
		else if (cmd.action() == LFUN_BUFFER_ZOOM_IN)
			return zr + convert<int>(cmd.argument()) / 100.0;
		else // cmd.action() == LFUN_BUFFER_ZOOM_OUT
			return zr - convert<int>(cmd.argument()) / 100.0;
	}
}

}


bool GuiView::getStatus(FuncRequest const & cmd, FuncStatus & flag)
{
	bool enable = true;
	Buffer * buf = currentBufferView()
		? &currentBufferView()->buffer() : nullptr;
	Buffer * doc_buffer = documentBufferView()
		? &(documentBufferView()->buffer()) : nullptr;

#ifdef Q_OS_MAC
	/* In LyX/Mac, when a dialog is open, the menus of the
	   application can still be accessed without giving focus to
	   the main window. In this case, we want to disable the menu
	   entries that are buffer-related.
	   This code must not be used on Linux and Windows, since it
	   would disable buffer-related entries when hovering over the
	   menu (see bug #9574).
	 */
	if (cmd.origin() == FuncRequest::MENU && !hasFocus()) {
		buf = 0;
		doc_buffer = 0;
	}
#endif

	// Check whether we need a buffer
	if (!lyxaction.funcHasFlag(cmd.action(), LyXAction::NoBuffer) && !buf) {
		// no, exit directly
		flag.message(from_utf8(N_("Command not allowed with"
					"out any document open")));
		flag.setEnabled(false);
		return true;
	}

	if (cmd.origin() == FuncRequest::TOC) {
		GuiToc * toc = static_cast<GuiToc*>(findOrBuild("toc", false));
		if (!toc || !toc->getStatus(documentBufferView()->cursor(), cmd, flag))
			flag.setEnabled(false);
		return true;
	}

	switch(cmd.action()) {
	case LFUN_BUFFER_IMPORT:
		break;

	case LFUN_MASTER_BUFFER_EXPORT:
		enable = doc_buffer
			&& (doc_buffer->parent() != nullptr
			    || doc_buffer->hasChildren())
			&& !d.processing_thread_watcher_.isRunning()
			// this launches a dialog, which would be in the wrong Buffer
			&& !(::lyx::operator==(cmd.argument(), "custom"));
		break;

	case LFUN_MASTER_BUFFER_UPDATE:
	case LFUN_MASTER_BUFFER_VIEW:
		enable = doc_buffer
			&& (doc_buffer->parent() != nullptr
			    || doc_buffer->hasChildren())
			&& !d.processing_thread_watcher_.isRunning();
		break;

	case LFUN_BUFFER_UPDATE:
	case LFUN_BUFFER_VIEW: {
		if (!doc_buffer || d.processing_thread_watcher_.isRunning()) {
			enable = false;
			break;
		}
		string format = to_utf8(cmd.argument());
		if (cmd.argument().empty())
			format = doc_buffer->params().getDefaultOutputFormat();
		enable = doc_buffer->params().isExportable(format, true);
		break;
	}

	case LFUN_BUFFER_UPDATE_EXTERNAL: {
		if (!doc_buffer || d.processing_thread_watcher_.isRunning()) {
			enable = false;
			break;
		}
		enable = !doc_buffer->externalRefFiles().empty();
		break;
	}

	case LFUN_BUFFER_RELOAD:
		enable = doc_buffer && !doc_buffer->isUnnamed()
			&& doc_buffer->fileName().exists()
			&& (!doc_buffer->isClean() || doc_buffer->notifiesExternalModification());
		break;

	case LFUN_BUFFER_RESET_EXPORT:
		enable = doc_buffer != nullptr;
		break;

	case LFUN_BUFFER_CHILD_OPEN:
		enable = doc_buffer != nullptr;
		break;

	case LFUN_MASTER_BUFFER_FORALL: {
		if (doc_buffer == nullptr) {
			flag.message(from_utf8(N_("Command not allowed without a buffer open")));
			enable = false;
			break;
		}
		FuncRequest const cmdToPass = lyxaction.lookupFunc(cmd.getLongArg(0));
		if (cmdToPass.action() == LFUN_UNKNOWN_ACTION) {
			flag.message(from_utf8(N_("Invalid argument of master-buffer-forall")));
			enable = false;
			break;
		}
		enable = false;
		for (Buffer * buf : doc_buffer->allRelatives()) {
			GuiWorkArea * wa = workArea(*buf);
			if (!wa)
				continue;
			if (wa->bufferView().getStatus(cmdToPass, flag)) {
				enable = flag.enabled();
				break;
			}
		}
		break;
	}

	case LFUN_BUFFER_WRITE:
		enable = doc_buffer && (doc_buffer->isUnnamed()
					|| (!doc_buffer->isClean()
					    || cmd.argument() == "force"));
		break;

	//FIXME: This LFUN should be moved to GuiApplication.
	case LFUN_BUFFER_WRITE_ALL: {
		// We enable the command only if there are some modified buffers
		Buffer * first = theBufferList().first();
		enable = false;
		if (!first)
			break;
		Buffer * b = first;
		// We cannot use a for loop as the buffer list is a cycle.
		do {
			if (!b->isClean()) {
				enable = true;
				break;
			}
			b = theBufferList().next(b);
		} while (b != first);
		break;
	}

	case LFUN_BUFFER_EXTERNAL_MODIFICATION_CLEAR:
		enable = doc_buffer && doc_buffer->notifiesExternalModification();
		break;

	case LFUN_BUFFER_EXPORT: {
		if (!doc_buffer || d.processing_thread_watcher_.isRunning()) {
			enable = false;
			break;
		}
		return doc_buffer->getStatus(cmd, flag);
	}

	case LFUN_BUFFER_EXPORT_AS:
		if (!doc_buffer || d.processing_thread_watcher_.isRunning()) {
			enable = false;
			break;
		}
		// fall through
	case LFUN_BUFFER_WRITE_AS:
	case LFUN_BUFFER_WRITE_AS_TEMPLATE:
		enable = doc_buffer != nullptr;
		break;

	case LFUN_EXPORT_CANCEL:
		enable = d.processing_thread_watcher_.isRunning();
		break;

	case LFUN_BUFFER_CLOSE:
	case LFUN_VIEW_CLOSE:
		enable = doc_buffer != nullptr;
		break;

	case LFUN_BUFFER_CLOSE_ALL:
		enable = theBufferList().last() != theBufferList().first();
		break;

	case LFUN_BUFFER_CHKTEX: {
		// hide if we have no checktex command
		if (lyxrc.chktex_command.empty()) {
			flag.setUnknown(true);
			enable = false;
			break;
		}
		if (!doc_buffer || !doc_buffer->params().isLatex()
		    || d.processing_thread_watcher_.isRunning()) {
			// grey out, don't hide
			enable = false;
			break;
		}
		enable = true;
		break;
	}

	case LFUN_CHANGES_TRACK: {
		if (!doc_buffer) {
			enable = false;
			break;
		}
		return doc_buffer->getStatus(cmd, flag);
	}

	case LFUN_VIEW_SPLIT:
		if (cmd.getArg(0) == "vertical")
			enable = doc_buffer && (d.splitter_->count() == 1 ||
					 d.splitter_->orientation() == Qt::Vertical);
		else
			enable = doc_buffer && (d.splitter_->count() == 1 ||
					 d.splitter_->orientation() == Qt::Horizontal);
		break;

	case LFUN_TAB_GROUP_NEXT:
	case LFUN_TAB_GROUP_PREVIOUS:
		enable = (d.splitter_->count() > 1);
		break;

	case LFUN_TAB_GROUP_CLOSE:
		enable = d.tabWorkAreaCount() > 1;
		break;

	case LFUN_DEVEL_MODE_TOGGLE:
		flag.setOnOff(devel_mode_);
		break;

	case LFUN_TOOLBAR_SET: {
		string const name = cmd.getArg(0);
		string const state = cmd.getArg(1);
		if (name.empty() || state.empty()) {
			enable = false;
			docstring const msg =
				_("Function toolbar-set requires two arguments!");
			flag.message(msg);
			break;
		}
		if (state != "on" && state != "off" && state != "auto") {
			enable = false;
			docstring const msg =
				bformat(_("Invalid argument \"%1$s\" to function toolbar-set!"),
					from_utf8(state));
			flag.message(msg);
			break;
		}
		if (GuiToolbar * t = toolbar(name)) {
			bool const autovis = t->visibility() & Toolbars::AUTO;
			if (state == "on")
				flag.setOnOff(t->isVisible() && !autovis);
			else if (state == "off")
				flag.setOnOff(!t->isVisible() && !autovis);
			else if (state == "auto")
				flag.setOnOff(autovis);
		} else {
			enable = false;
			docstring const msg =
				bformat(_("Unknown toolbar \"%1$s\""), from_utf8(name));
			flag.message(msg);
		}
		break;
	}

	case LFUN_TOOLBAR_TOGGLE: {
		string const name = cmd.getArg(0);
		if (GuiToolbar * t = toolbar(name))
			flag.setOnOff(t->isVisible());
		else {
			enable = false;
			docstring const msg =
				bformat(_("Unknown toolbar \"%1$s\""), from_utf8(name));
			flag.message(msg);
		}
		break;
	}

	case LFUN_TOOLBAR_MOVABLE: {
		string const name = cmd.getArg(0);
		// use negation since locked == !movable
		if (name == "*")
			// toolbar name * locks all toolbars
			flag.setOnOff(!toolbarsMovable_);
		else if (GuiToolbar * t = toolbar(name))
			flag.setOnOff(!(t->isMovable()));
		else {
			enable = false;
			docstring const msg =
				bformat(_("Unknown toolbar \"%1$s\""), from_utf8(name));
			flag.message(msg);
		}
		break;
	}

	case LFUN_ICON_SIZE:
		flag.setOnOff(d.iconSize(cmd.argument()) == iconSize());
		break;

	case LFUN_DROP_LAYOUTS_CHOICE:
		enable = buf != nullptr;
		break;

	case LFUN_UI_TOGGLE:
		if (cmd.argument() == "zoomlevel") {
			flag.setOnOff(zoom_value_ ? zoom_value_->isVisible() : false);
		} else if (cmd.argument() == "zoomslider") {
			flag.setOnOff(zoom_widget_ ? zoom_widget_->isVisible() : false);
		} else if (cmd.argument() == "statistics-w") {
			flag.setOnOff(word_count_enabled_);
		} else if (cmd.argument() == "statistics-cb") {
			flag.setOnOff(char_count_enabled_);
		} else if (cmd.argument() == "statistics-c") {
			flag.setOnOff(char_nb_count_enabled_);
		} else
			flag.setOnOff(isFullScreen());
		break;

	case LFUN_DIALOG_DISCONNECT_INSET:
		break;

	case LFUN_DIALOG_HIDE:
		// FIXME: should we check if the dialog is shown?
		break;

	case LFUN_DIALOG_TOGGLE:
		flag.setOnOff(isDialogVisible(cmd.getArg(0)));
		// to set "enable"
		// fall through
	case LFUN_DIALOG_SHOW: {
		string const name = cmd.getArg(0);
		if (!doc_buffer)
			enable = name == "aboutlyx"
				|| name == "file" //FIXME: should be removed.
				|| name == "lyxfiles"
				|| name == "prefs"
				|| name == "texinfo"
				|| name == "progress"
				|| name == "compare";
		else if (name == "character" || name == "symbols"
			|| name == "mathdelimiter" || name == "mathmatrix") {
			if (!buf || buf->isReadonly())
				enable = false;
			else {
				Cursor const & cur = currentBufferView()->cursor();
				enable = !(cur.inTexted() && cur.paragraph().isPassThru());
			}
		}
		else if (name == "latexlog")
			enable = FileName(doc_buffer->logName()).isReadableFile();
		else if (name == "spellchecker")
			enable = theSpellChecker()
				&& !doc_buffer->text().empty();
		else if (name == "vclog")
			enable = doc_buffer->lyxvc().inUse();
		break;
	}

	case LFUN_DIALOG_UPDATE: {
		string const name = cmd.getArg(0);
		if (!buf)
			enable = name == "prefs";
		break;
	}

	case LFUN_ERRORS_SHOW: {
		enable = buf && (!buf->errorList(d.last_export_format).empty()
				 || !buf->masterBuffer()->errorList(d.last_export_format).empty());
		break;
	}

	case LFUN_ERROR_NEXT: {
		if (!buf || (buf->errorList(d.last_export_format).empty()
			     && buf->masterBuffer()->errorList(d.last_export_format).empty())) {
			enable = false;
			break;
		}
		// We guess it's from master if the single buffer list is empty
		bool const from_master = currentBufferView()->buffer().errorList(d.last_export_format).empty();
		enable = nextError(d.last_export_format, from_master) != -1;
		break;
	}

	case LFUN_COMMAND_EXECUTE:
	case LFUN_MESSAGE:
	case LFUN_MENU_OPEN:
		// Nothing to check.
		break;

	case LFUN_COMPLETION_INLINE:
		if (!d.current_work_area_
			|| !d.current_work_area_->completer().inlinePossible(
			currentBufferView()->cursor()))
			enable = false;
		break;

	case LFUN_COMPLETION_POPUP:
		if (!d.current_work_area_
			|| !d.current_work_area_->completer().popupPossible(
			currentBufferView()->cursor()))
			enable = false;
		break;

	case LFUN_COMPLETE:
		if (!d.current_work_area_
			|| !d.current_work_area_->completer().inlinePossible(
			currentBufferView()->cursor()))
			enable = false;
		break;

	case LFUN_COMPLETION_ACCEPT:
		if (!d.current_work_area_
			|| (!d.current_work_area_->completer().popupVisible()
			&& !d.current_work_area_->completer().inlineVisible()
			&& !d.current_work_area_->completer().completionAvailable()))
			enable = false;
		break;

	case LFUN_COMPLETION_CANCEL:
		if (!d.current_work_area_
			|| (!d.current_work_area_->completer().popupVisible()
			&& !d.current_work_area_->completer().inlineVisible()))
			enable = false;
		break;

	case LFUN_BUFFER_ZOOM_OUT:
	case LFUN_BUFFER_ZOOM_IN:
	case LFUN_BUFFER_ZOOM: {
		int const zoom = (int)(lyxrc.defaultZoom * zoomRatio(cmd, zoom_ratio_));
		if (zoom < zoom_min_) {
			docstring const msg =
				bformat(_("Zoom level cannot be less than %1$d%."), zoom_min_);
			flag.message(msg);
			enable = false;
		} else if (zoom > zoom_max_) {
			docstring const msg =
				bformat(_("Zoom level cannot be more than %1$d%."), zoom_max_);
			flag.message(msg);
			enable = false;
		} else
			enable = doc_buffer;
		break;
	}


	case LFUN_BUFFER_MOVE_NEXT:
	case LFUN_BUFFER_MOVE_PREVIOUS:
		// we do not cycle when moving
	case LFUN_BUFFER_NEXT:
	case LFUN_BUFFER_PREVIOUS:
		// because we cycle, it doesn't matter whether on first or last
		enable = (d.currentTabWorkArea()->count() > 1);
		break;
	case LFUN_BUFFER_SWITCH:
		// toggle on the current buffer, but do not toggle off
		// the other ones (is that a good idea?)
		if (doc_buffer
			&& to_utf8(cmd.argument()) == doc_buffer->absFileName())
			flag.setOnOff(true);
		break;

	case LFUN_VC_REGISTER:
		enable = doc_buffer && !doc_buffer->lyxvc().inUse();
		break;
	case LFUN_VC_RENAME:
		enable = doc_buffer && doc_buffer->lyxvc().renameEnabled();
		break;
	case LFUN_VC_COPY:
		enable = doc_buffer && doc_buffer->lyxvc().copyEnabled();
		break;
	case LFUN_VC_CHECK_IN:
		enable = doc_buffer && doc_buffer->lyxvc().checkInEnabled();
		break;
	case LFUN_VC_CHECK_OUT:
		enable = doc_buffer && doc_buffer->lyxvc().checkOutEnabled();
		break;
	case LFUN_VC_LOCKING_TOGGLE:
		enable = doc_buffer && !doc_buffer->hasReadonlyFlag()
			&& doc_buffer->lyxvc().lockingToggleEnabled();
		flag.setOnOff(enable && doc_buffer->lyxvc().locking());
		break;
	case LFUN_VC_REVERT:
		enable = doc_buffer && doc_buffer->lyxvc().inUse()
			&& !doc_buffer->hasReadonlyFlag();
		break;
	case LFUN_VC_UNDO_LAST:
		enable = doc_buffer && doc_buffer->lyxvc().undoLastEnabled();
		break;
	case LFUN_VC_REPO_UPDATE:
		enable = doc_buffer && doc_buffer->lyxvc().repoUpdateEnabled();
		break;
	case LFUN_VC_COMMAND: {
		if (cmd.argument().empty())
			enable = false;
		if (!doc_buffer && contains(cmd.getArg(0), 'D'))
			enable = false;
		break;
	}
	case LFUN_VC_COMPARE:
		enable = doc_buffer && doc_buffer->lyxvc().prepareFileRevisionEnabled();
		break;

	case LFUN_SERVER_GOTO_FILE_ROW:
	case LFUN_LYX_ACTIVATE:
	case LFUN_WINDOW_RAISE:
		break;
	case LFUN_FORWARD_SEARCH:
		enable = !(lyxrc.forward_search_dvi.empty() && lyxrc.forward_search_pdf.empty()) &&
			doc_buffer && doc_buffer->isSyncTeXenabled();
		break;

	case LFUN_FILE_INSERT_PLAINTEXT:
	case LFUN_FILE_INSERT_PLAINTEXT_PARA:
		enable = documentBufferView() && documentBufferView()->cursor().inTexted();
		break;

	case LFUN_SPELLING_CONTINUOUSLY:
		flag.setOnOff(lyxrc.spellcheck_continuously);
		break;

	case LFUN_CITATION_OPEN:
		enable = true;
		break;

	default:
		return false;
	}

	if (!enable)
		flag.setEnabled(false);

	return true;
}


static FileName selectTemplateFile()
{
	FileDialog dlg(qt_("Select template file"));
	dlg.setButton1(qt_("D&ocuments"), toqstr(lyxrc.document_path));
	dlg.setButton2(qt_("&Templates"), toqstr(lyxrc.template_path));

	FileDialog::Result result = dlg.open(toqstr(lyxrc.template_path),
				 QStringList(qt_("LyX Documents (*.lyx)")));

	if (result.first == FileDialog::Later)
		return FileName();
	if (result.second.isEmpty())
		return FileName();
	return FileName(fromqstr(result.second));
}


Buffer * GuiView::loadDocument(FileName const & filename, bool tolastfiles)
{
	setBusy(true);

	Buffer * newBuffer = nullptr;
	try {
		newBuffer = checkAndLoadLyXFile(filename);
	} catch (ExceptionMessage const &) {
		setBusy(false);
		throw;
	}
	setBusy(false);

	if (!newBuffer) {
		message(_("Document not loaded."));
		return nullptr;
	}

	setBuffer(newBuffer);
	newBuffer->errors("Parse");

	if (tolastfiles) {
		theSession().lastFiles().add(filename);
		theSession().writeFile();
  }

	return newBuffer;
}


void GuiView::openDocuments(string const & fname, int origin)
{
	string initpath = lyxrc.document_path;

	if (documentBufferView()) {
		string const trypath = documentBufferView()->buffer().filePath();
		// If directory is writeable, use this as default.
		if (FileName(trypath).isDirWritable())
			initpath = trypath;
	}

	QStringList files;

	if (fname.empty()) {
		FileDialog dlg(qt_("Select documents to open"));
		dlg.setButton1(qt_("D&ocuments"), toqstr(lyxrc.document_path));
		dlg.setButton2(qt_("&Examples"), toqstr(lyxrc.example_path));

		QStringList const filter({
				qt_("LyX Documents (*.lyx)"),
				qt_("LyX Document Backups (*.lyx~)"),
				qt_("All Files") + " " + wildcardAllFiles()
		});
		FileDialog::Results results =
			dlg.openMulti(toqstr(initpath), filter);

		if (results.first == FileDialog::Later)
			return;

		files = results.second;

		// check selected filename
		if (files.isEmpty()) {
			message(_("Canceled."));
			return;
		}
	} else
		files << toqstr(fname);

	// iterate over all selected files
	for (auto const & file : files) {
		string filename = fromqstr(file);

		// get absolute path of file and add ".lyx" to the filename if
		// necessary.
		FileName const fullname =
				fileSearch(string(), filename, "lyx", support::may_not_exist);
		if (!fullname.empty())
			filename = fullname.absFileName();

		if (!fullname.onlyPath().isDirectory()) {
			Alert::warning(_("Invalid filename"),
					bformat(_("The directory in the given path\n%1$s\ndoes not exist."),
					from_utf8(fullname.absFileName())));
			continue;
		}

		// if the file doesn't exist and isn't already open (bug 6645),
		// let the user create one
		if (!fullname.exists() && !theBufferList().exists(fullname) &&
			!LyXVC::file_not_found_hook(fullname)) {
			// see bug #12609
			if (origin == FuncRequest::MENU) {
				docstring const & msg =
					bformat(_("File\n"
						"%1$s\n"
						"does not exist. Create empty file?"),
							from_utf8(filename));
				int ret = Alert::prompt(_("File does not exist"),
							msg, 0, 1,
							_("Create &File"),
							_("&Cancel"));
				if (ret == 1)
					continue;
			}
			Buffer * const b = newFile(filename, string(), true);
			if (b)
				setBuffer(b);
			continue;
		}

		docstring const disp_fn = makeDisplayPath(filename);
		message(bformat(_("Opening document %1$s..."), disp_fn));

		docstring str2;
		Buffer * buf = loadDocument(fullname);
		if (buf) {
			str2 = bformat(_("Document %1$s opened."), disp_fn);
			if (buf->lyxvc().inUse())
				str2 += " " + from_utf8(buf->lyxvc().versionString()) +
					" " + _("Version control detected.");
		} else {
			str2 = bformat(_("Could not open document %1$s"), disp_fn);
		}
		message(str2);
	}
}

// FIXME: clean that
static bool import(GuiView * lv, FileName const & filename,
	string const & format, ErrorList & errorList)
{
	FileName const lyxfile(support::changeExtension(filename.absFileName(), ".lyx"));

	string loader_format;
	vector<string> loaders = theConverters().loaders();
	if (find(loaders.begin(), loaders.end(), format) == loaders.end()) {
		for (string const & loader : loaders) {
			if (!theConverters().isReachable(format, loader))
				continue;

			string const tofile =
				support::changeExtension(filename.absFileName(),
				theFormats().extension(loader));
			if (theConverters().convert(nullptr, filename, FileName(tofile),
				filename, format, loader, errorList) != Converters::SUCCESS)
				return false;
			loader_format = loader;
			break;
		}
		if (loader_format.empty()) {
			frontend::Alert::error(_("Couldn't import file"),
					 bformat(_("No information for importing the format %1$s."),
					 translateIfPossible(theFormats().prettyName(format))));
			return false;
		}
	} else
		loader_format = format;

	if (loader_format == "lyx") {
		Buffer * buf = lv->loadDocument(lyxfile);
		if (!buf)
			return false;
	} else {
		Buffer * const b = newFile(lyxfile.absFileName(), string(), true);
		if (!b)
			return false;
		lv->setBuffer(b);
		bool as_paragraphs = loader_format == "textparagraph";
		string filename2 = (loader_format == format) ? filename.absFileName()
			: support::changeExtension(filename.absFileName(),
					  theFormats().extension(loader_format));
		lv->currentBufferView()->insertPlaintextFile(FileName(filename2),
			as_paragraphs);
		guiApp->setCurrentView(lv);
		lyx::dispatch(FuncRequest(LFUN_MARK_OFF));
	}

	return true;
}


void GuiView::importDocument(string const & argument)
{
	string format;
	string filename = split(argument, format, ' ');

	LYXERR(Debug::INFO, format << " file: " << filename);

	// need user interaction
	if (filename.empty()) {
		string initpath = lyxrc.document_path;
		if (documentBufferView()) {
			string const trypath = documentBufferView()->buffer().filePath();
			// If directory is writeable, use this as default.
			if (FileName(trypath).isDirWritable())
				initpath = trypath;
		}

		docstring const text = bformat(_("Select %1$s file to import"),
			translateIfPossible(theFormats().prettyName(format)));

		FileDialog dlg(toqstr(text));
		dlg.setButton1(qt_("D&ocuments"), toqstr(lyxrc.document_path));
		dlg.setButton2(qt_("&Examples"), toqstr(lyxrc.example_path));

		docstring filter = translateIfPossible(theFormats().prettyName(format));
		filter += " (*.{";
		// FIXME UNICODE
		filter += from_utf8(theFormats().extensions(format));
		filter += "})";

		FileDialog::Result result =
			dlg.open(toqstr(initpath), fileFilters(toqstr(filter)));

		if (result.first == FileDialog::Later)
			return;

		filename = fromqstr(result.second);

		// check selected filename
		if (filename.empty())
			message(_("Canceled."));
	}

	if (filename.empty())
		return;

	// get absolute path of file
	FileName const fullname(support::makeAbsPath(filename));

	// Can happen if the user entered a path into the dialog
	// (see bug #7437)
	if (fullname.onlyFileName().empty()) {
		docstring msg = bformat(_("The file name '%1$s' is invalid!\n"
					  "Aborting import."),
					from_utf8(fullname.absFileName()));
		frontend::Alert::error(_("File name error"), msg);
		message(_("Canceled."));
		return;
	}


	FileName const lyxfile(support::changeExtension(fullname.absFileName(), ".lyx"));

	// Check if the document already is open
	Buffer * buf = theBufferList().getBuffer(lyxfile);
	if (buf) {
		setBuffer(buf);
		if (!closeBuffer()) {
			message(_("Canceled."));
			return;
		}
	}

	docstring const displaypath = makeDisplayPath(lyxfile.absFileName(), 30);

	// if the file exists already, and we didn't do
	// -i lyx thefile.lyx, warn
	if (lyxfile.exists() && fullname != lyxfile) {

		docstring text = bformat(_("The document %1$s already exists.\n\n"
			"Do you want to overwrite that document?"), displaypath);
		int const ret = Alert::prompt(_("Overwrite document?"),
			text, 0, 1, _("&Overwrite"), _("&Cancel"));

		if (ret == 1) {
			message(_("Canceled."));
			return;
		}
	}

	message(bformat(_("Importing %1$s..."), displaypath));
	ErrorList errorList;
	if (import(this, fullname, format, errorList))
		message(_("imported."));
	else
		message(_("file not imported!"));

	// FIXME (Abdel 12/08/06): Is there a need to display the error list here?
}


void GuiView::newDocument(string const & filename, string templatefile,
			  bool from_template)
{
	FileName initpath(lyxrc.document_path);
	if (documentBufferView()) {
		FileName const trypath(documentBufferView()->buffer().filePath());
		// If directory is writeable, use this as default.
		if (trypath.isDirWritable())
			initpath = trypath;
	}

	if (from_template) {
		if (templatefile.empty())
			templatefile =  selectTemplateFile().absFileName();
		if (templatefile.empty())
			return;
	}

	Buffer * b;
	if (filename.empty())
		b = newUnnamedFile(initpath, to_utf8(_("newfile")), templatefile);
	else
		b = newFile(filename, templatefile, true);

	if (b)
		setBuffer(b);

	// If no new document could be created, it is unsure
	// whether there is a valid BufferView.
	if (currentBufferView())
		// Ensure the cursor is correctly positioned on screen.
		currentBufferView()->showCursor();
}


bool GuiView::insertLyXFile(docstring const & fname, bool ignorelang)
{
	BufferView * bv = documentBufferView();
	if (!bv)
		return false;

	// FIXME UNICODE
	FileName filename(to_utf8(fname));
	if (filename.empty()) {
		// Launch a file browser
		// FIXME UNICODE
		string initpath = lyxrc.document_path;
		string const trypath = bv->buffer().filePath();
		// If directory is writeable, use this as default.
		if (FileName(trypath).isDirWritable())
			initpath = trypath;

		// FIXME UNICODE
		FileDialog dlg(qt_("Select LyX document to insert"));
		dlg.setButton1(qt_("D&ocuments"), toqstr(lyxrc.document_path));
		dlg.setButton2(qt_("&Examples"), toqstr(lyxrc.example_path));

		FileDialog::Result result = dlg.open(toqstr(initpath),
					 QStringList(qt_("LyX Documents (*.lyx)")));

		if (result.first == FileDialog::Later)
			return false;

		// FIXME UNICODE
		filename.set(fromqstr(result.second));

		// check selected filename
		if (filename.empty()) {
			// emit message signal.
			message(_("Canceled."));
			return false;
		}
	}

	bv->insertLyXFile(filename, ignorelang);
	bv->buffer().errors("Parse");
	return true;
}


string const GuiView::getTemplatesPath(Buffer & b)
{
	// We start off with the user's templates path
	string result = addPath(package().user_support().absFileName(), "templates");
	// Check for the document language
	string const langcode = b.params().language->code();
	string const shortcode = langcode.substr(0, 2);
	if (!langcode.empty() && shortcode != "en") {
		string subpath = addPath(result, shortcode);
		string subpath_long = addPath(result, langcode);
		// If we have a subdirectory for the language already,
		// navigate there
		FileName sp = FileName(subpath);
		if (sp.isDirectory())
			result = subpath;
		else if (FileName(subpath_long).isDirectory())
			result = subpath_long;
		else {
			// Ask whether we should create such a subdirectory
			docstring const text =
				bformat(_("It is suggested to save the template in a subdirectory\n"
					  "appropriate to the document language (%1$s).\n"
					  "This subdirectory does not exists yet.\n"
					  "Do you want to create it?"),
					_(b.params().language->display()));
			if (Alert::prompt(_("Create Language Directory?"),
					  text, 0, 1, _("&Yes, Create"), _("&No, Save Template in Parent Directory")) == 0) {
				// If the user agreed, we try to create it and report if this failed.
				if (!sp.createDirectory(0777))
					Alert::error(_("Subdirectory creation failed!"),
						     _("Could not create subdirectory.\n"
						       "The template will be saved in the parent directory."));
				else
					result = subpath;
			}
		}
	}
	// Do we have a layout category?
	string const cat = b.params().baseClass() ?
				b.params().baseClass()->category()
			      : string();
	if (!cat.empty()) {
		string subpath = addPath(result, cat);
		// If we have a subdirectory for the category already,
		// navigate there
		FileName sp = FileName(subpath);
		if (sp.isDirectory())
			result = subpath;
		else {
			// Ask whether we should create such a subdirectory
			docstring const text =
				bformat(_("It is suggested to save the template in a subdirectory\n"
					  "appropriate to the layout category (%1$s).\n"
					  "This subdirectory does not exists yet.\n"
					  "Do you want to create it?"),
					_(cat));
			if (Alert::prompt(_("Create Category Directory?"),
					  text, 0, 1, _("&Yes, Create"), _("&No, Save Template in Parent Directory")) == 0) {
				// If the user agreed, we try to create it and report if this failed.
				if (!sp.createDirectory(0777))
					Alert::error(_("Subdirectory creation failed!"),
						     _("Could not create subdirectory.\n"
						       "The template will be saved in the parent directory."));
				else
					result = subpath;
			}
		}
	}
	return result;
}


bool GuiView::renameBuffer(Buffer & b, docstring const & newname, RenameKind kind)
{
	FileName fname = b.fileName();
	FileName const oldname = fname;
	bool const as_template = (kind == LV_WRITE_AS_TEMPLATE);

	if (!newname.empty()) {
		// FIXME UNICODE
		if (as_template)
			fname = support::makeAbsPath(to_utf8(newname), getTemplatesPath(b));
		else
			fname = support::makeAbsPath(to_utf8(newname),
						     oldname.onlyPath().absFileName());
	} else {
		// Switch to this Buffer.
		setBuffer(&b);

		// No argument? Ask user through dialog.
		// FIXME UNICODE
		QString const title = as_template ? qt_("Choose a filename to save template as")
						  : qt_("Choose a filename to save document as");
		FileDialog dlg(title);
		dlg.setButton1(qt_("D&ocuments"), toqstr(lyxrc.document_path));
		dlg.setButton2(qt_("&Templates"), toqstr(lyxrc.template_path));

		fname.ensureExtension(".lyx");

		string const path = as_template ?
					getTemplatesPath(b)
				      : fname.onlyPath().absFileName();
		FileDialog::Result result =
			dlg.save(toqstr(path),
				   QStringList(qt_("LyX Documents (*.lyx)")),
					 toqstr(fname.onlyFileName()));

		if (result.first == FileDialog::Later)
			return false;

		fname.set(fromqstr(result.second));

		if (fname.empty())
			return false;

		fname.ensureExtension(".lyx");
	}

	// fname is now the new Buffer location.

	// if there is already a Buffer open with this name, we do not want
	// to have another one. (the second test makes sure we're not just
	// trying to overwrite ourselves, which is fine.)
	if (theBufferList().exists(fname) && fname != oldname
		  && theBufferList().getBuffer(fname) != &b) {
		docstring const text =
			bformat(_("The file\n%1$s\nis already open in your current session.\n"
		            "Please close it before attempting to overwrite it.\n"
		            "Do you want to choose a new filename?"),
			        from_utf8(fname.absFileName()));
		int const ret = Alert::prompt(_("Chosen File Already Open"),
			text, 0, 1, _("&Rename"), _("&Cancel"));
		switch (ret) {
		case 0: return renameBuffer(b, docstring(), kind);
		case 1: return false;
		}
		//return false;
	}

	bool const existsLocal = fname.exists();
	bool const existsInVC = LyXVC::fileInVC(fname);
	if (existsLocal || existsInVC) {
		docstring const file = makeDisplayPath(fname.absFileName(), 30);
		if (kind != LV_WRITE_AS && existsInVC) {
			// renaming to a name that is already in VC
			// would not work
			docstring text = bformat(_("The document %1$s "
					"is already registered.\n\n"
					"Do you want to choose a new name?"),
				file);
			docstring const title = (kind == LV_VC_RENAME) ?
				_("Rename document?") : _("Copy document?");
			docstring const button = (kind == LV_VC_RENAME) ?
				_("&Rename") : _("&Copy");
			int const ret = Alert::prompt(title, text, 0, 1,
				button, _("&Cancel"));
			switch (ret) {
			case 0: return renameBuffer(b, docstring(), kind);
			case 1: return false;
			}
		}

		if (existsLocal) {
			docstring text = bformat(_("The document %1$s "
					"already exists.\n\n"
					"Do you want to overwrite that document?"),
				file);
			int const ret = Alert::prompt(_("Overwrite document?"),
					text, 0, 2, _("&Overwrite"),
					_("&Rename"), _("&Cancel"));
			switch (ret) {
			case 0: break;
			case 1: return renameBuffer(b, docstring(), kind);
			case 2: return false;
			}
		}
	}

	switch (kind) {
	case LV_VC_RENAME: {
		string msg = b.lyxvc().rename(fname);
		if (msg.empty())
			return false;
		message(from_utf8(msg));
		break;
	}
	case LV_VC_COPY: {
		string msg = b.lyxvc().copy(fname);
		if (msg.empty())
			return false;
		message(from_utf8(msg));
		break;
	}
	case LV_WRITE_AS:
	case LV_WRITE_AS_TEMPLATE:
		break;
	}
	// LyXVC created the file already in case of LV_VC_RENAME or
	// LV_VC_COPY, but call saveBuffer() nevertheless to get
	// relative paths of included stuff right if we moved e.g. from
	// /a/b.lyx to /a/c/b.lyx.

	bool const saved = saveBuffer(b, fname);
	if (saved)
		b.reload();
	return saved;
}


bool GuiView::exportBufferAs(Buffer & b, docstring const & iformat)
{
	FileName fname = b.fileName();

	FileDialog dlg(qt_("Choose a filename to export the document as"));
	dlg.setButton1(qt_("D&ocuments"), toqstr(lyxrc.document_path));

	QStringList types;
	QString const anyformat = qt_("Guess from extension (*.*)");
	types << anyformat;

	vector<Format const *> export_formats;
	for (Format const & f : theFormats())
		if (f.documentFormat())
			export_formats.push_back(&f);
	sort(export_formats.begin(), export_formats.end(), Format::formatSorter);
	map<QString, string> fmap;
	QString filter;
	string ext;
	for (Format const * f : export_formats) {
		docstring const loc_prettyname = translateIfPossible(f->prettyname());
		QString const loc_filter = toqstr(bformat(from_ascii("%1$s (*.%2$s)"),
						     loc_prettyname,
						     from_ascii(f->extension())));
		types << loc_filter;
		fmap[loc_filter] = f->name();
		if (from_ascii(f->name()) == iformat) {
			filter = loc_filter;
			ext = f->extension();
		}
	}
	string ofname = fname.onlyFileName();
	if (!ext.empty())
		ofname = support::changeExtension(ofname, ext);
	FileDialog::Result result =
		dlg.save(toqstr(fname.onlyPath().absFileName()),
			 types,
			 toqstr(ofname),
			 &filter);
	if (result.first != FileDialog::Chosen)
		return false;

	string fmt_name;
	fname.set(fromqstr(result.second));
	if (filter == anyformat)
		fmt_name = theFormats().getFormatFromExtension(fname.extension());
	else
		fmt_name = fmap[filter];
	LYXERR(Debug::FILES, "filter=" << fromqstr(filter)
	       << ", fmt_name=" << fmt_name << ", fname=" << fname.absFileName());

	if (fmt_name.empty() || fname.empty())
		return false;

	fname.ensureExtension(theFormats().extension(fmt_name));

	// fname is now the new Buffer location.
	if (fname.exists()) {
		docstring const file = makeDisplayPath(fname.absFileName(), 30);
		docstring text = bformat(_("The document %1$s already "
					   "exists.\n\nDo you want to "
					   "overwrite that document?"),
					 file);
		int const ret = Alert::prompt(_("Overwrite document?"),
			text, 0, 2, _("&Overwrite"), _("&Rename"), _("&Cancel"));
		switch (ret) {
		case 0: break;
		case 1: return exportBufferAs(b, from_ascii(fmt_name));
		case 2: return false;
		}
	}

	FuncRequest cmd(LFUN_BUFFER_EXPORT, fmt_name + " " + fname.absFileName());
	DispatchResult dr = lyx::dispatch(cmd);
	return dr.dispatched();
}


bool GuiView::saveBuffer(Buffer & b)
{
	return saveBuffer(b, FileName());
}


bool GuiView::saveBuffer(Buffer & b, FileName const & fn)
{
	if (workArea(b) && workArea(b)->inDialogMode())
		return true;

	if (fn.empty() && b.isUnnamed())
		return renameBuffer(b, docstring());

	bool const success = (fn.empty() ? b.save() : b.saveAs(fn));
	if (success) {
		theSession().lastFiles().add(b.fileName());
		theSession().writeFile();
		return true;
	}

	// Switch to this Buffer.
	setBuffer(&b);

	// FIXME: we don't tell the user *WHY* the save failed !!
	docstring const file = makeDisplayPath(b.absFileName(), 30);
	docstring text = bformat(_("The document %1$s could not be saved.\n\n"
				   "Do you want to rename the document and "
				   "try again?"), file);
	int const ret = Alert::prompt(_("Rename and save?"),
		text, 0, 2, _("&Rename"), _("&Retry"), _("&Cancel"));
	switch (ret) {
	case 0:
		if (!renameBuffer(b, docstring()))
			return false;
		break;
	case 1:
		break;
	case 2:
		return false;
	}

	return saveBuffer(b, fn);
}


bool GuiView::hideWorkArea(GuiWorkArea * wa)
{
	return closeWorkArea(wa, false);
}


// We only want to close the buffer if it is not visible in other workareas
// of the same view, nor in other views, and if this is not a child
bool GuiView::closeWorkArea(GuiWorkArea * wa)
{
	Buffer & buf = wa->bufferView().buffer();

	bool last_wa = d.countWorkAreasOf(buf) == 1
		&& !inOtherView(buf) && !buf.parent();

	bool close_buffer = last_wa;

	if (last_wa) {
		if (lyxrc.close_buffer_with_last_view == "yes")
			; // Nothing to do
		else if (lyxrc.close_buffer_with_last_view == "no")
			close_buffer = false;
		else {
			docstring file;
			if (buf.isUnnamed())
				file = from_utf8(buf.fileName().onlyFileName());
			else
				file = buf.fileName().displayName(30);
			docstring const text = bformat(
				_("Last view on document %1$s is being closed.\n"
				  "Would you like to close or hide the document?\n"
				  "\n"
				  "Hidden documents can be displayed back through\n"
				  "the menu: View->Hidden->...\n"
				  "\n"
				  "To remove this question, set your preference in:\n"
				  "  Tools->Preferences->Look&Feel->UserInterface\n"
				), file);
			int ret = Alert::prompt(_("Close or hide document?"),
				text, 0, 2, _("&Close"), _("&Hide"), _("&Cancel"));
			if (ret == 2)
				return false;
			close_buffer = (ret == 0);
		}
	}

	return closeWorkArea(wa, close_buffer);
}


bool GuiView::closeBuffer()
{
	GuiWorkArea * wa = currentMainWorkArea();
	// coverity complained about this
	// it seems unnecessary, but perhaps is worth the check
	LASSERT(wa, return false);

	setCurrentWorkArea(wa);
	Buffer & buf = wa->bufferView().buffer();
	return closeWorkArea(wa, !buf.parent());
}


void GuiView::writeSession() const {
	GuiWorkArea const * active_wa = currentMainWorkArea();
	for (int i = 0; i < d.splitter_->count(); ++i) {
		TabWorkArea * twa = d.tabWorkArea(i);
		for (int j = 0; j < twa->count(); ++j) {
			GuiWorkArea * wa = twa->workArea(j);
			Buffer & buf = wa->bufferView().buffer();
			theSession().lastOpened().add(buf.fileName(), wa == active_wa);
		}
	}
}


bool GuiView::closeBufferAll()
{

	for (auto & buf : theBufferList()) {
		if (!saveBufferIfNeeded(*buf, false)) {
			// Closing has been cancelled, so abort.
			return false;
		}
	}

	// Close the workareas in all other views
	QList<int> const ids = guiApp->viewIds();
	for (int i = 0; i != ids.size(); ++i) {
		if (id_ != ids[i] && !guiApp->view(ids[i]).closeWorkAreaAll())
			return false;
	}

	// Close our own workareas
	if (!closeWorkAreaAll())
		return false;

	return true;
}


bool GuiView::closeWorkAreaAll()
{
	setCurrentWorkArea(currentMainWorkArea());

	// We might be in a situation that there is still a tabWorkArea, but
	// there are no tabs anymore. This can happen when we get here after a
	// TabWorkArea::lastWorkAreaRemoved() signal. Therefore we count how
	// many TabWorkArea's have no documents anymore.
	int empty_twa = 0;

	// We have to call count() each time, because it can happen that
	// more than one splitter will disappear in one iteration (bug 5998).
	while (d.splitter_->count() > empty_twa) {
		TabWorkArea * twa = d.tabWorkArea(empty_twa);

		if (twa->count() == 0)
			++empty_twa;
		else {
			setCurrentWorkArea(twa->currentWorkArea());
			if (!closeTabWorkArea(twa))
				return false;
		}
	}
	return true;
}


bool GuiView::closeWorkArea(GuiWorkArea * wa, bool close_buffer)
{
	if (!wa)
		return false;

	Buffer & buf = wa->bufferView().buffer();

	if (GuiViewPrivate::busyBuffers.contains(&buf)) {
		Alert::warning(_("Close document"),
			_("Document could not be closed because it is being processed by LyX."));
		return false;
	}

	if (close_buffer)
		return closeBuffer(buf);
	else {
		if (!inMultiTabs(wa))
			if (!saveBufferIfNeeded(buf, true))
				return false;
		removeWorkArea(wa);
		return true;
	}
}


bool GuiView::closeBuffer(Buffer & buf)
{
	buf.setClosing(true);
	bool success = true;
	for (Buffer * child_buf : buf.getChildren()) {
		if (theBufferList().isOthersChild(&buf, child_buf)) {
			child_buf->setParent(nullptr);
			continue;
		}

		// FIXME: should we look in other tabworkareas?
		// ANSWER: I don't think so. I've tested, and if the child is
		// open in some other window, it closes without a problem.
		GuiWorkArea * child_wa = workArea(*child_buf);
		if (child_wa) {
			if (closing_)
				// If we are in a close_event all children will be closed in some time,
				// so no need to do it here. This will ensure that the children end up
				// in the session file in the correct order. If we close the master
				// buffer, we can close or release the child buffers here too.
				continue;
			success = closeWorkArea(child_wa, true);
			if (!success)
				break;
		} else {
			// In this case the child buffer is open but hidden.
			// Even in this case, children can be dirty (e.g.,
			// after a label change in the master, see #11405).
			// Therefore, check this
			if (closing_ && (child_buf->isClean() || child_buf->paragraphs().empty())) {
				// If we are in a close_event all children will be closed in some time,
				// so no need to do it here. This will ensure that the children end up
				// in the session file in the correct order. If we close the master
				// buffer, we can close or release the child buffers here too.
				continue;
			}
			// Save dirty buffers also if closing_!
			if (saveBufferIfNeeded(*child_buf, false)) {
				child_buf->removeAutosaveFile();
				theBufferList().release(child_buf);
			} else {
				// Saving of dirty children has been cancelled.
				// Cancel the whole process.
				success = false;
				break;
			}
		}
	}
	if (success) {
		// goto bookmark to update bookmark pit.
		// FIXME: we should update only the bookmarks related to this buffer!
		// FIXME: this is done also in LFUN_WINDOW_CLOSE!
		LYXERR(Debug::DEBUG, "GuiView::closeBuffer()");
		for (unsigned int i = 1; i < theSession().bookmarks().size(); ++i)
			guiApp->gotoBookmark(i, false, false);

		if (saveBufferIfNeeded(buf, false)) {
			buf.removeAutosaveFile();
			theBufferList().release(&buf);
			return true;
		}
	}
	buf.setClosing(false);
	// open all children again to avoid a crash because of dangling
	// pointers (bug 6603)
	buf.updateBuffer();
	return false;
}


bool GuiView::closeTabWorkArea(TabWorkArea * twa)
{
	while (twa == d.currentTabWorkArea()) {
		twa->setCurrentIndex(twa->count() - 1);

		GuiWorkArea * wa = twa->currentWorkArea();
		Buffer & b = wa->bufferView().buffer();

		// We only want to close the buffer if the same buffer is not visible
		// in another view, and if this is not a child and if we are closing
		// a view (not a tabgroup).
		bool const close_buffer =
			!inOtherView(b) && !b.parent() && closing_;

		if (!closeWorkArea(wa, close_buffer))
			return false;
	}
	return true;
}


bool GuiView::saveBufferIfNeeded(Buffer & buf, bool hiding)
{
	if (buf.isClean() || buf.paragraphs().empty())
		return true;

	// Switch to this Buffer.
	setBuffer(&buf);

	docstring file;
	bool exists;
	// FIXME: Unicode?
	if (buf.isUnnamed()) {
		file = from_utf8(buf.fileName().onlyFileName());
		exists = false;
	} else {
		FileName filename = buf.fileName();
		filename.refresh();
		file = filename.displayName(30);
		exists = filename.exists();
	}

	// Bring this window to top before asking questions.
	raise();
	activateWindow();

	int ret;
	if (hiding && buf.isUnnamed()) {
		docstring const text = bformat(_("The document %1$s has not been "
						 "saved yet.\n\nDo you want to save "
						 "the document?"), file);
		ret = Alert::prompt(_("Save new document?"),
			text, 0, 1, _("&Save"), _("&Cancel"));
		if (ret == 1)
			++ret;
	} else {
		docstring const text = exists ?
			bformat(_("The document %1$s has unsaved changes."
			          "\n\nDo you want to save the document or "
			          "discard the changes?"), file) :
			bformat(_("The document %1$s has not been saved yet."
			          "\n\nDo you want to save the document or "
			          "discard it entirely?"), file);
		docstring const title = exists ?
			_("Save changed document?") : _("Save document?");
		ret = Alert::prompt(title, text, 0, 2,
		                    _("&Save"), _("&Discard"), _("&Cancel"));
	}

	switch (ret) {
	case 0:
		if (!saveBuffer(buf))
			return false;
		break;
	case 1:
		// If we crash after this we could have no autosave file
		// but I guess this is really improbable (Jug).
		// Sometimes improbable things happen:
		// - see bug http://www.lyx.org/trac/ticket/6587 (ps)
		// buf.removeAutosaveFile();
		if (hiding)
			// revert all changes
			reloadBuffer(buf);
		buf.markClean();
		break;
	case 2:
		return false;
	}
	return true;
}


bool GuiView::inMultiTabs(GuiWorkArea * wa)
{
	Buffer & buf = wa->bufferView().buffer();

	for (int i = 0; i != d.splitter_->count(); ++i) {
		GuiWorkArea * wa_ = d.tabWorkArea(i)->workArea(buf);
		if (wa_ && wa_ != wa)
			return true;
	}
	return inOtherView(buf);
}


bool GuiView::inOtherView(Buffer & buf)
{
	QList<int> const ids = guiApp->viewIds();

	for (int i = 0; i != ids.size(); ++i) {
		if (id_ == ids[i])
			continue;

		if (guiApp->view(ids[i]).workArea(buf))
			return true;
	}
	return false;
}


void GuiView::gotoNextOrPreviousBuffer(NextOrPrevious np, bool const move)
{
	if (!documentBufferView())
		return;

	if (TabWorkArea * twa = d.currentTabWorkArea()) {
		Buffer * const curbuf = &documentBufferView()->buffer();
		int nwa = twa->count();
		for (int i = 0; i < nwa; ++i) {
			if (&workArea(i)->bufferView().buffer() == curbuf) {
				int next_index;
				if (np == NEXT)
					next_index = (i == nwa - 1 ? 0 : i + 1);
				else
					next_index = (i == 0 ? nwa - 1 : i - 1);
				if (move)
					twa->moveTab(i, next_index);
				else
					setBuffer(&workArea(next_index)->bufferView().buffer());
				break;
			}
		}
	}
}


void GuiView::gotoNextTabWorkArea(NextOrPrevious np)
{
	int count = d.splitter_->count();
	for (int i = 0; i < count; ++i) {
		if (d.tabWorkArea(i) == d.currentTabWorkArea()) {
			int new_index;
			if (np == NEXT)
				new_index = (i == count - 1 ? 0 : i + 1);
			else
				new_index = (i == 0 ? count - 1 : i - 1);
			setCurrentWorkArea(d.tabWorkArea(new_index)->currentWorkArea());
			break;
		}
	}
}


/// make sure the document is saved
static bool ensureBufferClean(Buffer * buffer)
{
	LASSERT(buffer, return false);
	if (buffer->isClean() && !buffer->isUnnamed())
		return true;

	docstring const file = buffer->fileName().displayName(30);
	docstring title;
	docstring text;
	if (!buffer->isUnnamed()) {
		text = bformat(_("The document %1$s has unsaved "
						 "changes.\n\nDo you want to save "
						 "the document?"), file);
		title = _("Save changed document?");

	} else {
		text = bformat(_("The document %1$s has not been "
						 "saved yet.\n\nDo you want to save "
						 "the document?"), file);
		title = _("Save new document?");
	}
	int const ret = Alert::prompt(title, text, 0, 1, _("&Save"), _("&Cancel"));

	if (ret == 0)
		dispatch(FuncRequest(LFUN_BUFFER_WRITE));

	return buffer->isClean() && !buffer->isUnnamed();
}


bool GuiView::reloadBuffer(Buffer & buf)
{
	currentBufferView()->cursor().reset();
	Buffer::ReadStatus status = buf.reload();
	return status == Buffer::ReadSuccess;
}


void GuiView::checkExternallyModifiedBuffers()
{
	for (Buffer * buf : theBufferList()) {
		if (buf->fileName().exists() && buf->isChecksumModified()) {
			docstring text = bformat(_("Document \n%1$s\n has been externally modified."
					" Reload now? Any local changes will be lost."),
					from_utf8(buf->absFileName()));
			int const ret = Alert::prompt(_("Reload externally changed document?"),
						text, 0, 1, _("&Reload"), _("&Cancel"));
			if (!ret)
				reloadBuffer(*buf);
		}
	}
}


void GuiView::dispatchVC(FuncRequest const & cmd, DispatchResult & dr)
{
	Buffer * buffer = documentBufferView()
		? &(documentBufferView()->buffer()) : nullptr;

	switch (cmd.action()) {
	case LFUN_VC_REGISTER:
		if (!buffer || !ensureBufferClean(buffer))
			break;
		if (!buffer->lyxvc().inUse()) {
			if (buffer->lyxvc().registrer()) {
				reloadBuffer(*buffer);
				dr.clearMessageUpdate();
			}
		}
		break;

	case LFUN_VC_RENAME:
	case LFUN_VC_COPY: {
		if (!buffer || !ensureBufferClean(buffer))
			break;
		if (buffer->lyxvc().inUse() && !buffer->hasReadonlyFlag()) {
			if (buffer->lyxvc().isCheckInWithConfirmation()) {
				// Some changes are not yet committed.
				// We test here and not in getStatus(), since
				// this test is expensive.
				string log;
				LyXVC::CommandResult ret =
					buffer->lyxvc().checkIn(log);
				dr.setMessage(log);
				if (ret == LyXVC::ErrorCommand ||
				    ret == LyXVC::VCSuccess)
					reloadBuffer(*buffer);
				if (buffer->lyxvc().isCheckInWithConfirmation()) {
					frontend::Alert::error(
						_("Revision control error."),
						_("Document could not be checked in."));
					break;
				}
			}
			RenameKind const kind = (cmd.action() == LFUN_VC_RENAME) ?
				LV_VC_RENAME : LV_VC_COPY;
			renameBuffer(*buffer, cmd.argument(), kind);
		}
		break;
	}

	case LFUN_VC_CHECK_IN:
		if (!buffer || !ensureBufferClean(buffer))
			break;
		if (buffer->lyxvc().inUse() && !buffer->hasReadonlyFlag()) {
			string log;
			LyXVC::CommandResult ret = buffer->lyxvc().checkIn(log);
			dr.setMessage(log);
			// Only skip reloading if the checkin was cancelled or
			// an error occurred before the real checkin VCS command
			// was executed, since the VCS might have changed the
			// file even if it could not checkin successfully.
			if (ret == LyXVC::ErrorCommand || ret == LyXVC::VCSuccess)
				reloadBuffer(*buffer);
		}
		break;

	case LFUN_VC_CHECK_OUT:
		if (!buffer || !ensureBufferClean(buffer))
			break;
		if (buffer->lyxvc().inUse()) {
			dr.setMessage(buffer->lyxvc().checkOut());
			reloadBuffer(*buffer);
		}
		break;

	case LFUN_VC_LOCKING_TOGGLE:
		if (!buffer || !ensureBufferClean(buffer) || buffer->hasReadonlyFlag())
			break;
		if (buffer->lyxvc().inUse()) {
			string res = buffer->lyxvc().lockingToggle();
			if (res.empty()) {
				frontend::Alert::error(_("Revision control error."),
				_("Error when setting the locking property."));
			} else {
				dr.setMessage(res);
				reloadBuffer(*buffer);
			}
		}
		break;

	case LFUN_VC_REVERT:
		if (!buffer)
			break;
		if (buffer->lyxvc().revert()) {
			reloadBuffer(*buffer);
			dr.clearMessageUpdate();
		}
		break;

	case LFUN_VC_UNDO_LAST:
		if (!buffer)
			break;
		buffer->lyxvc().undoLast();
		reloadBuffer(*buffer);
		dr.clearMessageUpdate();
		break;

	case LFUN_VC_REPO_UPDATE:
		if (!buffer)
			break;
		if (ensureBufferClean(buffer)) {
			dr.setMessage(buffer->lyxvc().repoUpdate());
			checkExternallyModifiedBuffers();
		}
		break;

	case LFUN_VC_COMMAND: {
		string flag = cmd.getArg(0);
		if (buffer && contains(flag, 'R') && !ensureBufferClean(buffer))
			break;
		docstring message;
		if (contains(flag, 'M')) {
			if (!Alert::askForText(message, _("LyX VC: Log Message")))
				break;
		}
		string path = cmd.getArg(1);
		if (contains(path, "$$p") && buffer)
			path = subst(path, "$$p", buffer->filePath());
		LYXERR(Debug::LYXVC, "Directory: " << path);
		FileName pp(path);
		if (!pp.isReadableDirectory()) {
			lyxerr << _("Directory is not accessible.") << endl;
			break;
		}
		support::PathChanger p(pp);

		string command = cmd.getArg(2);
		if (command.empty())
			break;
		if (buffer) {
			command = subst(command, "$$i", buffer->absFileName());
			command = subst(command, "$$p", buffer->filePath());
		}
		command = subst(command, "$$m", to_utf8(message));
		LYXERR(Debug::LYXVC, "Command: " << command);
		Systemcall one;
		one.startscript(Systemcall::Wait, command);

		if (!buffer)
			break;
		if (contains(flag, 'I'))
			buffer->markDirty();
		if (contains(flag, 'R'))
			reloadBuffer(*buffer);

		break;
		}

	case LFUN_VC_COMPARE: {
		if (cmd.argument().empty()) {
			lyx::dispatch(FuncRequest(LFUN_DIALOG_SHOW, "comparehistory"));
			break;
		}
		if (!buffer)
			break;

		string rev1 = cmd.getArg(0);
		string f1, f2;

		// f1
		if (!buffer->lyxvc().prepareFileRevision(rev1, f1))
			break;

		if (isStrInt(rev1) && convert<int>(rev1) <= 0) {
			f2 = buffer->absFileName();
		} else {
			string rev2 = cmd.getArg(1);
			if (rev2.empty())
				break;
			// f2
			if (!buffer->lyxvc().prepareFileRevision(rev2, f2))
				break;
		}

		LYXERR(Debug::LYXVC, "Launching comparison for fetched revisions:\n" <<
					f1 << "\n"  << f2 << "\n" );
		string par = "compare run " + quoteName(f1) + " " + quoteName(f2);
		lyx::dispatch(FuncRequest(LFUN_DIALOG_SHOW, par));
		break;
	}

	default:
		break;
	}
}


void GuiView::openChildDocument(string const & fname)
{
	LASSERT(documentBufferView(), return);
	Buffer & buffer = documentBufferView()->buffer();
	FileName const filename = support::makeAbsPath(fname, buffer.filePath());
	documentBufferView()->saveBookmark(false);
	Buffer * child = nullptr;
	if (theBufferList().exists(filename)) {
		child = theBufferList().getBuffer(filename);
		setBuffer(child);
	} else {
		message(bformat(_("Opening child document %1$s..."),
			makeDisplayPath(filename.absFileName())));
		child = loadDocument(filename, false);
	}
	// Set the parent name of the child document.
	// This makes insertion of citations and references in the child work,
	// when the target is in the parent or another child document.
	if (child)
		child->setParent(&buffer);
}


bool GuiView::goToFileRow(string const & argument)
{
	string file_name;
	int row = -1;
	size_t i = argument.find_last_of(' ');
	if (i != string::npos) {
		file_name = os::internal_path(FileName(trim(argument.substr(0, i))).realPath());
		istringstream is(argument.substr(i + 1));
		is >> row;
		if (is.fail())
			i = string::npos;
	}
	if (i == string::npos) {
		LYXERR0("Wrong argument: " << argument);
		return false;
	}
	Buffer * buf = nullptr;
	string const realtmp = package().temp_dir().realPath();
	// We have to use os::path_prefix_is() here, instead of
	// simply prefixIs(), because the file name comes from
	// an external application and may need case adjustment.
	if (os::path_prefix_is(file_name, realtmp, os::CASE_ADJUSTED)) {
		buf = theBufferList().getBufferFromTmp(file_name, true);
		LYXERR(Debug::FILES, "goToFileRow: buffer lookup for " << file_name
			   << (buf ? " success" : " failed"));
	} else {
		// Must replace extension of the file to be .lyx
		// and get full path
		FileName const s = fileSearch(string(),
						  support::changeExtension(file_name, ".lyx"), "lyx");
		// Either change buffer or load the file
		if (theBufferList().exists(s))
			buf = theBufferList().getBuffer(s);
		else if (s.exists()) {
			buf = loadDocument(s);
			if (!buf)
				return false;
		} else {
			message(bformat(
					_("File does not exist: %1$s"),
					makeDisplayPath(file_name)));
			return false;
		}
	}
	if (!buf) {
		message(bformat(
			_("No buffer for file: %1$s."),
			makeDisplayPath(file_name))
		);
		return false;
	}
	setBuffer(buf);
	bool success = documentBufferView()->setCursorFromRow(row);
	if (!success) {
		LYXERR(Debug::OUTFILE,
		       "setCursorFromRow: invalid position for row " << row);
		frontend::Alert::error(_("Inverse Search Failed"),
		                       _("Invalid position requested by inverse search.\n"
		                         "You may need to update the viewed document."));
	}
	return success;
}


template<class T>
Buffer::ExportStatus GuiView::GuiViewPrivate::runAndDestroy(const T& func,
		Buffer const * orig, Buffer * clone, string const & format)
{
	Buffer::ExportStatus const status = func(format);

	// the cloning operation will have produced a clone of the entire set of
	// documents, starting from the master. so we must delete those.
	Buffer * mbuf = const_cast<Buffer *>(clone->masterBuffer());
	delete mbuf;
	busyBuffers.remove(orig);
	return status;
}


Buffer::ExportStatus GuiView::GuiViewPrivate::compileAndDestroy(
		Buffer const * orig, Buffer * clone, string const & format)
{
	Buffer::ExportStatus (Buffer::* mem_func)(std::string const &, bool) const =
			&Buffer::doExport;
	return runAndDestroy(lyx::bind(mem_func, clone, _1, true), orig, clone, format);
}


Buffer::ExportStatus GuiView::GuiViewPrivate::exportAndDestroy(
		Buffer const * orig, Buffer * clone, string const & format)
{
	Buffer::ExportStatus (Buffer::* mem_func)(std::string const &, bool) const =
			&Buffer::doExport;
	return runAndDestroy(lyx::bind(mem_func, clone, _1, false), orig, clone, format);
}


Buffer::ExportStatus GuiView::GuiViewPrivate::previewAndDestroy(
		Buffer const * orig, Buffer * clone, string const & format)
{
	Buffer::ExportStatus (Buffer::* mem_func)(std::string const &) const =
			&Buffer::preview;
	return runAndDestroy(lyx::bind(mem_func, clone, _1), orig, clone, format);
}


bool GuiView::GuiViewPrivate::asyncBufferProcessing(string const & argument,
			   Buffer const * used_buffer,
			   docstring const & msg,
			   Buffer::ExportStatus (*asyncFunc)(Buffer const *, Buffer *, string const &),
			   Buffer::ExportStatus (Buffer::*syncFunc)(string const &, bool) const,
			   Buffer::ExportStatus (Buffer::*previewFunc)(string const &) const,
			   bool allow_async, bool use_tmpdir)
{
	if (!used_buffer)
		return false;

	string format = argument;
	if (format.empty())
		format = used_buffer->params().getDefaultOutputFormat();
	processing_format = format;
	if (!msg.empty()) {
		progress_->clearMessages();
		gv_->message(msg);
	}
#if EXPORT_in_THREAD
	if (allow_async) {
		GuiViewPrivate::busyBuffers.insert(used_buffer);
		Buffer * cloned_buffer = used_buffer->cloneWithChildren();
		if (!cloned_buffer) {
			Alert::error(_("Export Error"),
				     _("Error cloning the Buffer."));
			return false;
		}
		QFuture<Buffer::ExportStatus> f = QtConcurrent::run(
					asyncFunc,
					used_buffer,
					cloned_buffer,
					format);
		setPreviewFuture(f);
		last_export_format = used_buffer->params().bufferFormat();
		(void) syncFunc;
		(void) previewFunc;
		// We are asynchronous, so we don't know here anything about the success
		return true;
	} else {
		Buffer::ExportStatus status;
		if (syncFunc) {
			status = (used_buffer->*syncFunc)(format, use_tmpdir);
		} else if (previewFunc) {
			status = (used_buffer->*previewFunc)(format);
		} else
			return false;
		handleExportStatus(gv_, status, format);
		(void) asyncFunc;
		return (status == Buffer::ExportSuccess
				|| status == Buffer::PreviewSuccess);
	}
#else
	(void) allow_async;
	Buffer::ExportStatus status;
	if (syncFunc) {
		status = (used_buffer->*syncFunc)(format, true);
	} else if (previewFunc) {
		status = (used_buffer->*previewFunc)(format);
	} else
		return false;
	handleExportStatus(gv_, status, format);
	(void) asyncFunc;
	return (status == Buffer::ExportSuccess
			|| status == Buffer::PreviewSuccess);
#endif
}

void GuiView::dispatchToBufferView(FuncRequest const & cmd, DispatchResult & dr)
{
	BufferView * bv = currentBufferView();
	LASSERT(bv, return);

	// Let the current BufferView dispatch its own actions.
	bv->dispatch(cmd, dr);
	if (dr.dispatched()) {
		if (cmd.action() == LFUN_REDO || cmd.action() == LFUN_UNDO)
			updateDialog("document", "");
		return;
	}

	// Try with the document BufferView dispatch if any.
	BufferView * doc_bv = documentBufferView();
	if (doc_bv && doc_bv != bv) {
		doc_bv->dispatch(cmd, dr);
		if (dr.dispatched()) {
			if (cmd.action() == LFUN_REDO || cmd.action() == LFUN_UNDO)
				updateDialog("document", "");
			return;
		}
	}

	// Then let the current Cursor dispatch its own actions.
	bv->cursor().dispatch(cmd);

	// update completion. We do it here and not in
	// processKeySym to avoid another redraw just for a
	// changed inline completion
	if (cmd.origin() == FuncRequest::KEYBOARD) {
		if (cmd.action() == LFUN_SELF_INSERT
			|| (cmd.action() == LFUN_ERT_INSERT && bv->cursor().inMathed()))
			updateCompletion(bv->cursor(), true, true);
		else if (cmd.action() == LFUN_CHAR_DELETE_BACKWARD)
			updateCompletion(bv->cursor(), false, true);
		else
			updateCompletion(bv->cursor(), false, false);
		bv->inputMethod()->toggleInputMethodAcceptance();
	}

	dr = bv->cursor().result();
}


void GuiView::dispatch(FuncRequest const & cmd, DispatchResult & dr)
{
	BufferView * bv = currentBufferView();
	// By default we won't need any update.
	dr.screenUpdate(Update::None);
	// assume cmd will be dispatched
	dr.dispatched(true);

	Buffer * doc_buffer = documentBufferView()
		? &(documentBufferView()->buffer()) : nullptr;

	if (cmd.origin() == FuncRequest::TOC) {
		GuiToc * toc = static_cast<GuiToc*>(findOrBuild("toc", false));
		toc->doDispatch(bv->cursor(), cmd, dr);
		return;
	}

	string const argument = to_utf8(cmd.argument());

	switch(cmd.action()) {
		case LFUN_BUFFER_CHILD_OPEN:
			openChildDocument(to_utf8(cmd.argument()));
			break;

		case LFUN_BUFFER_IMPORT:
			importDocument(to_utf8(cmd.argument()));
			break;

		case LFUN_MASTER_BUFFER_EXPORT:
			if (doc_buffer)
				doc_buffer = const_cast<Buffer *>(doc_buffer->masterBuffer());
			// fall through
		case LFUN_BUFFER_EXPORT: {
			if (!doc_buffer)
				break;
			if (cmd.argument() == "custom") {
				// LFUN_MASTER_BUFFER_EXPORT is not enabled for this case,
				// so the following test should not be needed.
				// In principle, we could try to switch to such a view...
				// if (cmd.action() == LFUN_BUFFER_EXPORT)
				dispatch(FuncRequest(LFUN_DIALOG_SHOW, "sendto"), dr);
				break;
			}

			string const dest = cmd.getArg(1);
			FileName target_dir;
			if (!dest.empty() && FileName::isAbsolute(dest))
				target_dir = FileName(support::onlyPath(dest));
			else
				target_dir = doc_buffer->fileName().onlyPath();

			string const format = (argument.empty() || argument == "default") ?
				doc_buffer->params().getDefaultOutputFormat() : argument;

			if ((dest.empty() && doc_buffer->isUnnamed())
			    || !target_dir.isDirWritable()) {
				exportBufferAs(*doc_buffer, from_utf8(format));
				break;
			}
			/* TODO/Review: Is it a problem to also export the children?
					See the update_unincluded flag */
			d.asyncBufferProcessing(format,
						doc_buffer,
						_("Exporting ..."),
						&GuiViewPrivate::exportAndDestroy,
						&Buffer::doExport,
						nullptr, cmd.allowAsync());
			// TODO Inform user about success
			break;
		}

		case LFUN_BUFFER_EXPORT_AS: {
			LASSERT(doc_buffer, break);
			docstring f = cmd.argument();
			if (f.empty())
				f = from_ascii(doc_buffer->params().getDefaultOutputFormat());
			exportBufferAs(*doc_buffer, f);
			break;
		}

		case LFUN_BUFFER_UPDATE: {
			if (doc_buffer && !doc_buffer->externalRefFiles().empty()) {
				// compile externally referred files first
				FuncRequest fr = FuncRequest(LFUN_BUFFER_UPDATE_EXTERNAL);
				fr.allowAsync(false);
				lyx::dispatch(fr);
			}
			d.asyncBufferProcessing(argument,
						doc_buffer,
						_("Exporting ..."),
						&GuiViewPrivate::compileAndDestroy,
						&Buffer::doExport,
						nullptr, cmd.allowAsync(), true);
			// If fresh start had been required, reset to false here
			// otherwise this would be done on each subsequent call
			if (doc_buffer)
				doc_buffer->requireFreshStart(false);
			break;
		}

		case LFUN_BUFFER_UPDATE_EXTERNAL: {
			if (!doc_buffer)
				break;
			bool const master = cmd.argument() == "master";
			for (FileName const & fn : master
			     ? doc_buffer->masterBuffer()->externalRefFiles()
			     : doc_buffer->externalRefFiles()) {
				Buffer const * buf = checkAndLoadLyXFile(fn, true);
				if (buf) {
					d.asyncBufferProcessing(string(),
								buf,
								_("Preparing externally referenced file ..."),
								&GuiViewPrivate::compileAndDestroy,
								&Buffer::doExport,
								nullptr, cmd.allowAsync(), true);
					// If fresh start had been required, reset to false here
					// otherwise this would be done on each subsequent call
					buf->requireFreshStart(false);
				} else
					LYXERR0("File " << fn.absFileName() << " not could not be compiled. External reference to it won't work.");
			}
			break;
		}

		case LFUN_BUFFER_VIEW: {
			if (doc_buffer && !doc_buffer->externalRefFiles().empty()) {
				// compile externally referred files first
				FuncRequest fr = FuncRequest(LFUN_BUFFER_UPDATE_EXTERNAL);
				fr.allowAsync(false);
				lyx::dispatch(fr);
			}
			d.asyncBufferProcessing(argument,
						doc_buffer,
						_("Previewing ..."),
						&GuiViewPrivate::previewAndDestroy,
						nullptr,
						&Buffer::preview, cmd.allowAsync());
			// If fresh start had been required, reset to false here
			// otherwise this would be done on each subsequent call
			if (doc_buffer)
				doc_buffer->requireFreshStart(false);
			break;
		}
		case LFUN_MASTER_BUFFER_UPDATE: {
			if (doc_buffer && !doc_buffer->masterBuffer()->externalRefFiles().empty()) {
				// compile externally referred files first
				FuncRequest fr = FuncRequest(LFUN_BUFFER_UPDATE_EXTERNAL, "master");
				fr.allowAsync(false);
				lyx::dispatch(fr);
			}
			d.asyncBufferProcessing(argument,
						(doc_buffer ? doc_buffer->masterBuffer() : nullptr),
						docstring(),
						&GuiViewPrivate::compileAndDestroy,
						&Buffer::doExport,
						nullptr, cmd.allowAsync(), true);
			// If fresh start had been required, reset to false here
			// otherwise this would be done on each subsequent call
			if (doc_buffer && doc_buffer->masterBuffer())
				doc_buffer->masterBuffer()->requireFreshStart(false);
			break;
		}
		case LFUN_MASTER_BUFFER_VIEW: {
			if (doc_buffer && !doc_buffer->masterBuffer()->externalRefFiles().empty()) {
				// compile externally referred files first
				FuncRequest fr = FuncRequest(LFUN_BUFFER_UPDATE_EXTERNAL, "master");
				fr.allowAsync(false);
				lyx::dispatch(fr);
			}
			d.asyncBufferProcessing(argument,
						(doc_buffer ? doc_buffer->masterBuffer() : nullptr),
						docstring(),
						&GuiViewPrivate::previewAndDestroy,
						nullptr, &Buffer::preview, cmd.allowAsync());
			// If fresh start had been required, reset to false here
			// otherwise this would be done on each subsequent call
			if (doc_buffer && doc_buffer->masterBuffer())
				doc_buffer->masterBuffer()->requireFreshStart(false);
			break;
		}
		case LFUN_EXPORT_CANCEL: {
			cancelExport();
			break;
		}
		case LFUN_BUFFER_SWITCH: {
			string const file_name = to_utf8(cmd.argument());
			if (!FileName::isAbsolute(file_name)) {
				dr.setError(true);
				dr.setMessage(_("Absolute filename expected."));
				break;
			}

			Buffer * buffer = theBufferList().getBuffer(FileName(file_name));
			if (!buffer) {
				dr.setError(true);
				dr.setMessage(_("Document not loaded"));
				break;
			}

			// Do we open or switch to the buffer in this view ?
			if (workArea(*buffer)
				  || lyxrc.open_buffers_in_tabs || !documentBufferView()) {
				setBuffer(buffer);
				break;
			}

			// Look for the buffer in other views
			QList<int> const ids = guiApp->viewIds();
			int i = 0;
			for (; i != ids.size(); ++i) {
				GuiView & gv = guiApp->view(ids[i]);
				if (gv.workArea(*buffer)) {
					gv.raise();
					gv.activateWindow();
					gv.setFocus();
					gv.setBuffer(buffer);
					break;
				}
			}

			// If necessary, open a new window as a last resort
			if (i == ids.size()) {
				lyx::dispatch(FuncRequest(LFUN_WINDOW_NEW));
				lyx::dispatch(cmd);
			}
			break;
		}

		case LFUN_BUFFER_NEXT:
			gotoNextOrPreviousBuffer(NEXT, false);
			break;

		case LFUN_BUFFER_MOVE_NEXT:
			gotoNextOrPreviousBuffer(NEXT, true);
			break;

		case LFUN_BUFFER_PREVIOUS:
			gotoNextOrPreviousBuffer(PREV, false);
			break;

		case LFUN_BUFFER_MOVE_PREVIOUS:
			gotoNextOrPreviousBuffer(PREV, true);
			break;

		case LFUN_BUFFER_CHKTEX:
			LASSERT(doc_buffer, break);
			doc_buffer->runChktex();
			break;

		case LFUN_CHANGES_TRACK: {
			// the actual dispatch is done in Buffer
			dispatchToBufferView(cmd, dr);
			// but we inform the GUI (document settings) if this is toggled
			LASSERT(doc_buffer, break);
			Q_EMIT changeTrackingToggled(doc_buffer->params().track_changes);
			break;
		}

		case LFUN_COMMAND_EXECUTE: {
			command_execute_ = true;
			minibuffer_focus_ = true;
			break;
		}
		case LFUN_DROP_LAYOUTS_CHOICE:
			d.layout_->showPopup();
			break;

		case LFUN_MENU_OPEN:
			if (QMenu * menu = guiApp->menus().menu(toqstr(cmd.argument()), *this))
				menu->exec(QCursor::pos());
			break;

		case LFUN_FILE_INSERT: {
			bool const ignore_lang = cmd.getArg(1) == "ignorelang";
			if (insertLyXFile(from_utf8(cmd.getArg(0)), ignore_lang)) {
				dr.forceBufferUpdate();
				dr.screenUpdate(Update::Force);
			}
			break;
		}

		case LFUN_FILE_INSERT_PLAINTEXT:
		case LFUN_FILE_INSERT_PLAINTEXT_PARA: {
			string const fname = to_utf8(cmd.argument());
			if (!fname.empty() && !FileName::isAbsolute(fname)) {
				dr.setMessage(_("Absolute filename expected."));
				break;
			}

			FileName filename(fname);
			if (fname.empty()) {
				FileDialog dlg(qt_("Select file to insert"));

				FileDialog::Result result = dlg.open(toqstr(bv->buffer().filePath()),
					QStringList(qt_("All Files")+ " " + wildcardAllFiles()));

				if (result.first == FileDialog::Later || result.second.isEmpty()) {
					dr.setMessage(_("Canceled."));
					break;
				}

				filename.set(fromqstr(result.second));
			}

			if (bv) {
				FuncRequest const new_cmd(cmd, filename.absoluteFilePath());
				bv->dispatch(new_cmd, dr);
			}
			break;
		}

		case LFUN_BUFFER_RELOAD: {
			LASSERT(doc_buffer, break);

			// drop changes?
			bool drop = (cmd.argument() == "dump");

			int ret = 0;
			if (!drop && !doc_buffer->isClean()) {
				docstring const file =
					makeDisplayPath(doc_buffer->absFileName(), 20);
				if (doc_buffer->notifiesExternalModification()) {
					docstring text = _("The current version will be lost. "
					    "Are you sure you want to load the version on disk "
					    "of the document %1$s?");
					ret = Alert::prompt(_("Reload saved document?"),
					                    bformat(text, file), 1, 1,
					                    _("&Reload"), _("&Cancel"));
				} else {
					docstring text = _("Any changes will be lost. "
					    "Are you sure you want to revert to the saved version "
					    "of the document %1$s?");
					ret = Alert::prompt(_("Revert to saved document?"),
					                    bformat(text, file), 1, 1,
					                    _("&Revert"), _("&Cancel"));
				}
			}

			if (ret == 0) {
				doc_buffer->markClean();
				reloadBuffer(*doc_buffer);
				dr.forceBufferUpdate();
			}
			break;
		}

		case LFUN_BUFFER_RESET_EXPORT:
			LASSERT(doc_buffer, break);
			doc_buffer->requireFreshStart(true);
			dr.setMessage(_("Buffer export reset."));
			break;

		case LFUN_BUFFER_WRITE:
			LASSERT(doc_buffer, break);
			saveBuffer(*doc_buffer);
			break;

		case LFUN_BUFFER_WRITE_AS:
			LASSERT(doc_buffer, break);
			renameBuffer(*doc_buffer, cmd.argument());
			break;

		case LFUN_BUFFER_WRITE_AS_TEMPLATE:
			LASSERT(doc_buffer, break);
			renameBuffer(*doc_buffer, cmd.argument(),
				     LV_WRITE_AS_TEMPLATE);
			break;

		case LFUN_BUFFER_WRITE_ALL: {
			Buffer * first = theBufferList().first();
			if (!first)
				break;
			message(_("Saving all documents..."));
			// We cannot use a for loop as the buffer list cycles.
			Buffer * b = first;
			do {
				if (!b->isClean()) {
					saveBuffer(*b);
					LYXERR(Debug::ACTION, "Saved " << b->absFileName());
				}
				b = theBufferList().next(b);
			} while (b != first);
			dr.setMessage(_("All documents saved."));
			break;
		}

		case LFUN_MASTER_BUFFER_FORALL: {
			if (!doc_buffer)
				break;

			FuncRequest funcToRun = lyxaction.lookupFunc(cmd.getLongArg(0));
			funcToRun.allowAsync(false);

			for (Buffer const * buf : doc_buffer->allRelatives()) {
				// Switch to other buffer view and resend cmd
				lyx::dispatch(FuncRequest(
					LFUN_BUFFER_SWITCH, buf->absFileName()));
				lyx::dispatch(funcToRun);
			}
			// switch back
			lyx::dispatch(FuncRequest(
				LFUN_BUFFER_SWITCH, doc_buffer->absFileName()));
			break;
		}

		case LFUN_BUFFER_EXTERNAL_MODIFICATION_CLEAR:
			LASSERT(doc_buffer, break);
			doc_buffer->clearExternalModification();
			break;

		case LFUN_BUFFER_CLOSE:
			closeBuffer();
			break;

		case LFUN_BUFFER_CLOSE_ALL:
			closeBufferAll();
			break;

		case LFUN_DEVEL_MODE_TOGGLE:
			devel_mode_ = !devel_mode_;
			if (devel_mode_)
				dr.setMessage(_("Developer mode is now enabled."));
			else
				dr.setMessage(_("Developer mode is now disabled."));
			break;

		case LFUN_TOOLBAR_SET: {
			string const name = cmd.getArg(0);
			string const state = cmd.getArg(1);
			if (GuiToolbar * t = toolbar(name))
				t->setState(state);
			break;
		}

		case LFUN_TOOLBAR_TOGGLE: {
			string const name = cmd.getArg(0);
			if (GuiToolbar * t = toolbar(name))
				t->toggle();
			break;
		}

		case LFUN_TOOLBAR_MOVABLE: {
			string const name = cmd.getArg(0);
			if (name == "*") {
				// toggle (all) toolbars movablility
				toolbarsMovable_ = !toolbarsMovable_;
				for (ToolbarInfo const & ti : guiApp->toolbars()) {
					GuiToolbar * tb = toolbar(ti.name);
					if (tb && tb->isMovable() != toolbarsMovable_)
						// toggle toolbar movablity if it does not fit lock
						// (all) toolbars positions state silent = true, since
						// status bar notifications are slow
						tb->movable(true);
				}
				if (toolbarsMovable_)
					dr.setMessage(_("Toolbars unlocked."));
				else
					dr.setMessage(_("Toolbars locked."));
			} else if (GuiToolbar * tb = toolbar(name))
				// toggle current toolbar movablity
				tb->movable();
			// update lock (all) toolbars positions
			updateLockToolbars();
			break;
		}

		case LFUN_ICON_SIZE: {
			QSize size = d.iconSize(cmd.argument());
			setIconSize(size);
			dr.setMessage(bformat(_("Icon size set to %1$dx%2$d."),
						size.width(), size.height()));
			break;
		}

		case LFUN_DIALOG_UPDATE: {
			string const name = to_utf8(cmd.argument());
			if (name == "prefs" || name == "document")
				updateDialog(name, string());
			else if (name == "paragraph")
				lyx::dispatch(FuncRequest(LFUN_PARAGRAPH_UPDATE));
			else if (currentBufferView()) {
				Inset * inset = currentBufferView()->editedInset(name);
				// Can only update a dialog connected to an existing inset
				if (inset) {
					// FIXME: get rid of this indirection; GuiView ask the inset
					// if he is kind enough to update itself...
					FuncRequest fr(LFUN_INSET_DIALOG_UPDATE, cmd.argument());
					//FIXME: pass DispatchResult here?
					inset->dispatch(currentBufferView()->cursor(), fr);
				}
			}
			break;
		}

		case LFUN_DIALOG_TOGGLE: {
			FuncCode const func_code = isDialogVisible(cmd.getArg(0))
				? LFUN_DIALOG_HIDE : LFUN_DIALOG_SHOW;
			dispatch(FuncRequest(func_code, cmd.argument()), dr);
			break;
		}

		case LFUN_DIALOG_DISCONNECT_INSET:
			disconnectDialog(to_utf8(cmd.argument()));
			break;

		case LFUN_DIALOG_HIDE: {
			guiApp->hideDialogs(to_utf8(cmd.argument()), nullptr);
			setFocus(Qt::PopupFocusReason);
			break;
		}

		case LFUN_DIALOG_SHOW: {
			string const name = cmd.getArg(0);
			string sdata = trim(to_utf8(cmd.argument()).substr(name.size()));

			if (name == "latexlog") {
				// getStatus checks that
				LASSERT(doc_buffer, break);
				Buffer::LogType type;
				string const logfile = doc_buffer->logName(&type);
				switch (type) {
				case Buffer::latexlog:
					sdata = "latex ";
					break;
				case Buffer::buildlog:
					sdata = "literate ";
					break;
				}
				sdata += Lexer::quoteString(logfile);
				showDialog("log", sdata);
			} else if (name == "vclog") {
				// getStatus checks that
				LASSERT(doc_buffer, break);
				string const sdata2 = "vc " +
					Lexer::quoteString(doc_buffer->lyxvc().getLogFile());
				showDialog("log", sdata2);
			} else if (name == "symbols") {
				sdata = bv->cursor().getEncoding()->name();
				if (!sdata.empty())
					showDialog("symbols", sdata);
			} else if (name == "findreplace") {
				sdata = to_utf8(bv->cursor().selectionAsString(false));
				showDialog(name, sdata);
			// bug 5274
			} else if (name == "prefs" && isFullScreen()) {
				lfunUiToggle("fullscreen");
				showDialog("prefs", sdata);
			} else
				showDialog(name, sdata);
			break;
		}

		case LFUN_ERRORS_SHOW: {
			// We guess it's from master if the single buffer list is empty
			bool const from_master = bv->buffer().errorList(d.last_export_format).empty();
			errors(d.last_export_format, from_master, nextError(d.last_export_format, from_master, false, true));
			break;
		}

		case LFUN_ERROR_NEXT: {
			// We guess it's from master if the single buffer list is empty
			bool const from_master = bv->buffer().errorList(d.last_export_format).empty();
			if (nextError(d.last_export_format, from_master, true) != -1) {
				dr.forceBufferUpdate();
				dr.screenUpdate(Update::Force);
			}
			break;
		}

		case LFUN_MESSAGE:
			dr.setMessage(cmd.argument());
			break;

		case LFUN_UI_TOGGLE: {
			string arg = cmd.getArg(0);
			if (!lfunUiToggle(arg)) {
				docstring const msg = "ui-toggle " + _("%1$s unknown command!");
				dr.setMessage(bformat(msg, from_utf8(arg)));
			}
			// Make sure the keyboard focus stays in the work area.
			setFocus();
			break;
		}

		case LFUN_VIEW_SPLIT: {
			LASSERT(doc_buffer, break);
			string const orientation = cmd.getArg(0);
			d.splitter_->setOrientation(orientation == "vertical"
				? Qt::Vertical : Qt::Horizontal);
			TabWorkArea * twa = addTabWorkArea();
			GuiWorkArea * wa = twa->addWorkArea(*doc_buffer, *this);

			wa->bufferView().copySettingsFrom(*bv);
			dr.screenUpdate(Update::ForceAll);
			setCurrentWorkArea(wa);
			break;
		}

		case LFUN_TAB_GROUP_NEXT:
			gotoNextTabWorkArea(NEXT);
			break;

		case LFUN_TAB_GROUP_PREVIOUS:
			gotoNextTabWorkArea(PREV);
			break;

		case LFUN_TAB_GROUP_CLOSE:
			if (TabWorkArea * twa = d.currentTabWorkArea()) {
				closeTabWorkArea(twa);
				d.current_work_area_ = nullptr;
				twa = d.currentTabWorkArea();
				// Switch to the next GuiWorkArea in the found TabWorkArea.
				if (twa) {
					// Make sure the work area is up to date.
					setCurrentWorkArea(twa->currentWorkArea());
				} else {
					setCurrentWorkArea(nullptr);
				}
			}
			break;

		case LFUN_VIEW_CLOSE:
			if (TabWorkArea * twa = d.currentTabWorkArea()) {
				closeWorkArea(twa->currentWorkArea());
				d.current_work_area_ = nullptr;
				twa = d.currentTabWorkArea();
				// Switch to the next GuiWorkArea in the found TabWorkArea.
				if (twa) {
					// Make sure the work area is up to date.
					setCurrentWorkArea(twa->currentWorkArea());
				} else {
					setCurrentWorkArea(nullptr);
				}
			}
			break;

		case LFUN_COMPLETION_INLINE:
			if (d.current_work_area_)
				d.current_work_area_->completer().showInline();
			break;

		case LFUN_COMPLETION_POPUP:
			if (d.current_work_area_)
				d.current_work_area_->completer().showPopup();
			break;


		case LFUN_COMPLETE:
			if (d.current_work_area_)
				d.current_work_area_->completer().tab();
			break;

		case LFUN_COMPLETION_CANCEL:
			if (d.current_work_area_) {
				if (d.current_work_area_->completer().popupVisible())
					d.current_work_area_->completer().hidePopup();
				else
					d.current_work_area_->completer().hideInline();
			}
			break;

		case LFUN_COMPLETION_ACCEPT:
			if (d.current_work_area_)
				d.current_work_area_->completer().activate();
			break;

		case LFUN_BUFFER_ZOOM_IN:
		case LFUN_BUFFER_ZOOM_OUT:
		case LFUN_BUFFER_ZOOM: {
			zoom_ratio_ = zoomRatio(cmd, zoom_ratio_);

			// Actual zoom value: default zoom + fractional extra value
			int zoom = (int)(lyxrc.defaultZoom * zoom_ratio_);
			zoom = min(max(zoom, zoom_min_), zoom_max_);

			// Sync zoom sliders in all active views
			guiApp->syncZoomSliders(zoom);

			dr.setMessage(bformat(_("Zoom level is now %1$d% (default value: %2$d%)"),
					      lyxrc.currentZoom, lyxrc.defaultZoom));

			guiApp->fontLoader().update();
			// Regenerate instant previews
			if (lyxrc.preview != LyXRC::PREVIEW_OFF
			    && doc_buffer && doc_buffer->loader())
				doc_buffer->loader()->refreshPreviews();
			dr.screenUpdate(Update::ForceAll | Update::FitCursor);
			break;
		}

		case LFUN_VC_REGISTER:
		case LFUN_VC_RENAME:
		case LFUN_VC_COPY:
		case LFUN_VC_CHECK_IN:
		case LFUN_VC_CHECK_OUT:
		case LFUN_VC_REPO_UPDATE:
		case LFUN_VC_LOCKING_TOGGLE:
		case LFUN_VC_REVERT:
		case LFUN_VC_UNDO_LAST:
		case LFUN_VC_COMMAND:
		case LFUN_VC_COMPARE:
			dispatchVC(cmd, dr);
			break;

		case LFUN_SERVER_GOTO_FILE_ROW:
			if(goToFileRow(to_utf8(cmd.argument())))
				dr.screenUpdate(Update::Force | Update::FitCursor);
			break;

		case LFUN_LYX_ACTIVATE:
			activateWindow();
			break;

		case LFUN_WINDOW_RAISE:
			raise();
			activateWindow();
			showNormal();
			break;

		case LFUN_FORWARD_SEARCH: {
			// it seems safe to assume we have a document buffer, since
			// getStatus wants one.
			LASSERT(doc_buffer, break);
			Buffer const * doc_master = doc_buffer->masterBuffer();
			FileName const path(doc_master->temppath());
			string const texname = doc_master->isChild(doc_buffer)
				? DocFileName(changeExtension(
					doc_buffer->absFileName(),
						"tex")).mangledFileName()
				: doc_buffer->latexName();
			string const fulltexname =
				support::makeAbsPath(texname, doc_master->temppath()).absFileName();
			string const mastername =
				removeExtension(doc_master->latexName());
			FileName const dviname(addName(path.absFileName(),
					addExtension(mastername, "dvi")));
			FileName const pdfname(addName(path.absFileName(),
					addExtension(mastername, "pdf")));
			bool const have_dvi = dviname.exists();
			bool const have_pdf = pdfname.exists();
			if (!have_dvi && !have_pdf) {
				dr.setMessage(_("Please, preview the document first."));
				break;
			}
			bool const goto_dvi = have_dvi && !lyxrc.forward_search_dvi.empty();
			bool const goto_pdf = have_pdf && !lyxrc.forward_search_pdf.empty();
			string outname = dviname.onlyFileName();
			string command = lyxrc.forward_search_dvi;
			if ((!goto_dvi || goto_pdf) &&
			    pdfname.lastModified() > dviname.lastModified()) {
				outname = pdfname.onlyFileName();
				command = lyxrc.forward_search_pdf;
			}

			DocIterator cur = bv->cursor();
			int row = doc_buffer->texrow().rowFromDocIterator(cur).first;
			LYXERR(Debug::ACTION, "Forward search: row:" << row
				   << " cur:" << cur);
			if (row == -1 || command.empty()) {
				dr.setMessage(_("Couldn't proceed."));
				break;
			}
			string texrow = convert<string>(row);

			command = subst(command, "$$n", texrow);
			command = subst(command, "$$f", fulltexname);
			command = subst(command, "$$t", texname);
			command = subst(command, "$$o", outname);

			volatile PathChanger p(path);
			Systemcall one;
			one.startscript(Systemcall::DontWait, command);
			break;
		}

		case LFUN_SPELLING_CONTINUOUSLY:
			lyxrc.spellcheck_continuously = !lyxrc.spellcheck_continuously;
			dr.screenUpdate(Update::Force);
			break;

		case LFUN_CITATION_OPEN: {
			LASSERT(doc_buffer, break);
			frontend::showTarget(argument, *doc_buffer);
			break;
		}

		default:
			// The LFUN must be for one of BufferView, Buffer or Cursor;
			// let's try that:
			dispatchToBufferView(cmd, dr);
			break;
	}

	// Need to update bv because many LFUNs here might have destroyed it
	bv = currentBufferView();

	// Clear non-empty selections
	// (e.g. from a "char-forward-select" followed by "char-backward-select")
	if (bv) {
		Cursor & cur = bv->cursor();
		if ((cur.selection() && cur.selBegin() == cur.selEnd())) {
			cur.clearSelection();
		}
	}
}


bool GuiView::lfunUiToggle(string const & ui_component)
{
	if (ui_component == "scrollbar") {
		// hide() is of no help
		if (d.current_work_area_->verticalScrollBarPolicy() ==
			Qt::ScrollBarAlwaysOff)

			d.current_work_area_->setVerticalScrollBarPolicy(
				Qt::ScrollBarAsNeeded);
		else
			d.current_work_area_->setVerticalScrollBarPolicy(
				Qt::ScrollBarAlwaysOff);
	} else if (ui_component == "statusbar") {
		statusBar()->setVisible(!statusBar()->isVisible());
	} else if (ui_component == "menubar") {
		menuBar()->setVisible(!menuBar()->isVisible());
	} else if (ui_component == "zoomlevel") {
		zoom_value_->setVisible(!zoom_value_->isVisible());
	} else if (ui_component == "zoomslider") {
		zoom_widget_->setVisible(!zoom_widget_->isVisible());
	} else if (ui_component == "statistics-w") {
		word_count_enabled_ = !word_count_enabled_;
		showStats();
	} else if (ui_component == "statistics-cb") {
		char_count_enabled_ = !char_count_enabled_;
		showStats();
	} else if (ui_component == "statistics-c") {
		char_nb_count_enabled_ = !char_nb_count_enabled_;
		showStats();
	} else if (ui_component == "frame") {
		int const l = contentsMargins().left();

		//are the frames in default state?
		d.current_work_area_->setFrameStyle(QFrame::NoFrame);
		if (l == 0) {
			setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
			setContentsMargins(-2, -2, -2, -2);
		} else {
			setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, true);
			setContentsMargins(0, 0, 0, 0);
		}
	} else
	if (ui_component == "fullscreen") {
		toggleFullScreen();
	} else
		return false;
	stat_counts_->setVisible(statsEnabled());
	d.stats_update_trigger_ = true;
	return true;
}


void GuiView::cancelExport()
{
	Systemcall::killscript();
	// stop busy signal immediately so that in the subsequent
	// "Export canceled" prompt the status bar icons are accurate.
	Q_EMIT scriptKilled();
}


void GuiView::toggleFullScreen()
{
	setWindowState(windowState() ^ Qt::WindowFullScreen);
}


Buffer const * GuiView::updateInset(Inset const * inset)
{
	if (!inset)
		return nullptr;

	Buffer const * inset_buffer = &(inset->buffer());

	for (int i = 0; i != d.splitter_->count(); ++i) {
		GuiWorkArea * wa = d.tabWorkArea(i)->currentWorkArea();
		if (!wa)
			continue;
		Buffer const * buffer = &(wa->bufferView().buffer());
		if (inset_buffer == buffer)
			wa->scheduleRedraw(true);
	}
	return inset_buffer;
}


void GuiView::restartCaret()
{
	/* When we move around, or type, it's nice to be able to see
	 * the caret immediately after the keypress.
	 */
	if (d.current_work_area_)
		d.current_work_area_->startBlinkingCaret();

	// Take this occasion to update the other GUI elements.
	updateDialogs();
	updateStatusBar();
}


void GuiView::updateCompletion(Cursor & cur, bool start, bool keep)
{
	if (d.current_work_area_)
		d.current_work_area_->completer().updateVisibility(cur, start, keep);
}

namespace {

// This list should be kept in sync with the list of insets in
// src/insets/Inset.cpp.  I.e., if a dialog goes with an inset, the
// dialog should have the same name as the inset.
// Changes should be also recorded in LFUN_DIALOG_SHOW doxygen
// docs in LyXAction.cpp.

char const * const dialognames[] = {

"aboutlyx", "bibitem", "bibtex", "box", "branch", "changes", "character",
"citation", "compare", "comparehistory", "counter", "document", "errorlist", "ert",
"external", "file", "findreplace", "findreplaceadv", "float", "graphics",
"href", "include", "index", "index_print", "info", "listings", "label", "line",
"log", "lyxfiles", "mathdelimiter", "mathmatrix", "mathspace", "nomenclature",
"nomencl_print", "note", "paragraph", "phantom", "prefs", "ref",
"sendto", "space", "spellchecker", "symbols", "tabular", "tabularcreate",
"thesaurus", "texinfo", "toc", "view-source", "vspace", "wrap", "progress"};

char const * const * const end_dialognames =
	dialognames + (sizeof(dialognames) / sizeof(char *));

class cmpCStr {
public:
	cmpCStr(char const * name) : name_(name) {}
	bool operator()(char const * other) {
		return strcmp(other, name_) == 0;
	}
private:
	char const * name_;
};


bool isValidName(string const & name)
{
	return find_if(dialognames, end_dialognames,
		cmpCStr(name.c_str())) != end_dialognames;
}

} // namespace


void GuiView::resetDialogs()
{
	// Make sure that no LFUN uses any GuiView.
	guiApp->setCurrentView(nullptr);
	saveLayout();
	saveUISettings();
	menuBar()->clear();
	constructToolbars();
	guiApp->menus().fillMenuBar(menuBar(), this, false);
	d.layout_->updateContents(true);
	// Now update controls with current buffer.
	guiApp->setCurrentView(this);
	restoreLayout();
	restartCaret();
}


void GuiView::flatGroupBoxes(const QObject * widget, bool flag)
{
	for (QObject * child: widget->children()) {
		if (child->inherits("QGroupBox")) {
			QGroupBox * box = (QGroupBox*) child;
			box->setFlat(flag);
		} else {
			flatGroupBoxes(child, flag);
		}
	}
}


Dialog * GuiView::find(string const & name, bool hide_it) const
{
	if (!isValidName(name))
		return nullptr;

	map<string, DialogPtr>::iterator it = d.dialogs_.find(name);

	if (it != d.dialogs_.end()) {
		if (hide_it)
			it->second->hideView();
		return it->second.get();
	}
	return nullptr;
}


Dialog * GuiView::findOrBuild(string const & name, bool hide_it)
{
	Dialog * dialog = find(name, hide_it);
	if (dialog != nullptr)
		return dialog;

	dialog = build(name);
	if (dialog) {

		d.dialogs_[name].reset(dialog);
		// Force a uniform style for group boxes
		// On Mac non-flat works better, on Linux flat is standard
		flatGroupBoxes(dialog->asQWidget(), guiApp->platformName() != "cocoa");
		if (lyxrc.allow_geometry_session)
			dialog->restoreSession();
		if (hide_it)
			dialog->hideView();
		}
	return dialog;
}


void GuiView::showDialog(string const & name, string const & sdata,
	Inset * inset)
{
	triggerShowDialog(toqstr(name), toqstr(sdata), inset);
}


void GuiView::doShowDialog(QString const & qname, QString const & qdata,
	Inset * inset)
{
	if (d.in_show_)
		return;

	const string name = fromqstr(qname);
	const string sdata = fromqstr(qdata);

	d.in_show_ = true;
	try {
		Dialog * dialog = findOrBuild(name, false);
		if (dialog) {
			bool const visible = dialog->isVisibleView();
			if (name == "findreplaceadv")
				dialog->showData(sdata, Qt::OtherFocusReason);
			else
				dialog->showData(sdata);
			if (currentBufferView())
				currentBufferView()->editInset(name, inset);
			// We only set the focus to the new dialog if it was not yet
			// visible in order not to change the existing previous behaviour
			if (visible) {
				// activateWindow is needed for floating dockviews
				dialog->asQWidget()->raise();
				dialog->asQWidget()->activateWindow();
				if (dialog->wantInitialFocus())
					dialog->asQWidget()->setFocus();
			}
		}
	}
	catch (ExceptionMessage const &) {
		d.in_show_ = false;
		throw;
	}
	d.in_show_ = false;
}


bool GuiView::isDialogVisible(string const & name) const
{
	map<string, DialogPtr>::const_iterator it = d.dialogs_.find(name);
	if (it == d.dialogs_.end())
		return false;
	return it->second.get()->isVisibleView() && !it->second.get()->isClosing();
}


void GuiView::hideDialog(string const & name, Inset * inset)
{
	map<string, DialogPtr>::const_iterator it = d.dialogs_.find(name);
	if (it == d.dialogs_.end())
		return;

	if (inset) {
		if (!currentBufferView())
			return;
		if (inset != currentBufferView()->editedInset(name))
			return;
	}

	Dialog * const dialog = it->second.get();
	if (dialog->isVisibleView())
		dialog->hideView();
	if (currentBufferView())
		currentBufferView()->editInset(name, nullptr);
}


void GuiView::disconnectDialog(string const & name)
{
	if (!isValidName(name))
		return;
	if (currentBufferView())
		currentBufferView()->editInset(name, nullptr);
}


void GuiView::hideAll() const
{
	for(auto const & dlg_p : d.dialogs_)
		dlg_p.second->hideView();
}


void GuiView::updateDialogs()
{
	for(auto const & dlg_p : d.dialogs_) {
		Dialog * dialog = dlg_p.second.get();
		if (dialog) {
			if (dialog->needBufferOpen() && !documentBufferView())
				hideDialog(fromqstr(dialog->name()), nullptr);
			else if (dialog->isVisibleView())
				dialog->checkStatus();
		}
	}
	updateToolbars();
	updateLayoutList();
}


Dialog * GuiView::build(string const & name)
{
	return createDialog(*this, name);
}


SEMenu::SEMenu(QWidget * parent)
{
	QAction * action = addAction(qt_("Disable Shell Escape"));
	connect(action, SIGNAL(triggered()),
		parent, SLOT(disableShellEscape()));
}


void PressableSvgWidget::mousePressEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
        Q_EMIT pressed();
    }
}

} // namespace frontend
} // namespace lyx

#include "moc_GuiView.cpp"
