/**
 * \file GuiApplication.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author unknown
 * \author John Levon
 * \author Abdelrazak Younes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiApplication.h"

#include "ToolTipFormatter.h"
#include "ColorCache.h"
#include "GuiClipboard.h"
#include "GuiSelection.h"
#include "GuiView.h"
#include "Menus.h"
#include "qt_helpers.h"
#include "Toolbars.h"

#include "frontends/alert.h"
#include "frontends/Application.h"
#include "frontends/FontLoader.h"
#include "frontends/FontMetrics.h"

#include "Buffer.h"
#include "BufferList.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "CmdDef.h"
#include "Color.h"
#include "Converter.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "Font.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "GuiWorkArea.h"
#include "Intl.h"
#include "KeyMap.h"
#include "Language.h"
#include "LaTeXPackages.h"
#include "LyX.h"
#include "LyXAction.h"
#include "LyXRC.h"
#include "Paragraph.h"
#include "Server.h"
#include "Session.h"
#include "SpellChecker.h"
#include "Thesaurus.h"
#include "version.h"

#include "insets/InsetText.h"

#include "support/checksum.h"
#include "support/convert.h"
#include "support/debug.h"
#include "support/ExceptionMessage.h"
#include "support/environment.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/ForkedCalls.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/Lexer.h"
#include "support/lstrings.h"
#include "support/lyxalgo.h" // sorted
#include "support/mute_warning.h"
#include "support/textutils.h"
#include "support/Messages.h"
#include "support/os.h"
#include "support/Package.h"
#include "support/TempFile.h"

#ifdef Q_OS_MAC
#include "support/AppleScript.h"
#include "support/AppleSupport.h"
#include "support/linkback/LinkBackProxy.h"
#endif

#include <queue>
#include <tuple>

#include <QByteArray>
#include <QBitmap>
#include <QDateTime>
#include <QEvent>
#include <QFileOpenEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHash>
#include <QIcon>
#include <QImageReader>
#include <QKeyEvent>
#include <QLocale>
#include <QLibraryInfo>
#include <QList>
#include <QMenuBar>
#include <QMimeData>
#include <QObject>
#include <QPainter>
#include <QPixmap>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
#include <QRandomGenerator>
#endif
#include <QSessionManager>
#include <QSettings>
#include <QSocketNotifier>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#if (QT_VERSION >= QT_VERSION_CHECK(6, 8, 0))
#include <QStyle>
#include <QStyleHints>
#endif
#include <QSvgRenderer>
#include <QTimer>
#include <QTranslator>
#include <QThreadPool>
#include <QWidget>

#ifdef HAVE_XCB_XCB_H
#include <xcb/xcb.h>
#ifdef HAVE_QT5_X11_EXTRAS
#include <QtX11Extras/QX11Info>
#endif
#endif

#if defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN)
#  if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#    if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
#      include <QtGui/QWindowsMimeConverter>
#      define QWINDOWSMIME QWindowsMimeConverter
#      define QVARIANTTYPE QMetaType
#    else
#      include <QtGui/private/qguiapplication_p.h>
#      include <QtGui/private/qwindowsmime_p.h>
#      include <QtGui/qpa/qplatformintegration.h>
#      define QWINDOWSMIME QWindowsMime
#      define QVARIANTTYPE QMetaType
using QWindowsMime = QNativeInterface::Private::QWindowsMime;
using QWindowsApplication = QNativeInterface::Private::QWindowsApplication;
#    endif
#  else
#    include <QWinMime>
#    define QWINDOWSMIME QWinMime
#    define QVARIANTTYPE QVariant::Type
#  endif
#  ifdef Q_CC_GNU
#    include <wtypes.h>
#  endif
#  include <objidl.h>
#endif

#if defined(Q_OS_MAC) && (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QMacPasteboardMime>
#endif // Q_OS_MAC

#include <exception>
#include <sstream>
#include <vector>

using namespace std;
using namespace lyx::support;


namespace lyx {

frontend::Application * createApplication(int & argc, char * argv[])
{
#if defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN)
	// On Windows, allow bringing the LyX window to top
	AllowSetForegroundWindow(ASFW_ANY);
#endif


#if defined(Q_OS_MAC)
	int const cursor_time_on = NSTextInsertionPointBlinkPeriodOn();
	int const cursor_time_off = NSTextInsertionPointBlinkPeriodOff();
	if (cursor_time_on > 0 && cursor_time_off > 0) {
		QApplication::setCursorFlashTime(cursor_time_on + cursor_time_off);
	} else if (cursor_time_on <= 0 && cursor_time_off > 0) {
		// Off is set and On is undefined of zero
		QApplication::setCursorFlashTime(0);
	} else if (cursor_time_off <= 0 && cursor_time_on > 0) {
		// On is set and Off is undefined of zero
		QApplication::setCursorFlashTime(0);
	}
#endif

// Setup high DPI handling. This is a bit complicated, but will be default in Qt6.
// macOS does it by itself.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) && !defined(Q_OS_MAC)
    // Attribute Qt::AA_EnableHighDpiScaling must be set before QCoreApplication is created
    if (getEnv("QT_ENABLE_HIGHDPI_SCALING").empty()
		&& getEnv("QT_AUTO_SCREEN_SCALE_FACTOR").empty())
        QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    // HighDPI scale factor policy must be set before QGuiApplication is created
    if (getEnv("QT_SCALE_FACTOR_ROUNDING_POLICY").empty())
        QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#endif

	frontend::GuiApplication * guiApp = new frontend::GuiApplication(argc, argv);
	// I'd rather do that in the constructor, but I do not think that
	// the palette is accessible there.
	guiApp->colorCache().setPalette(guiApp->palette());
	return guiApp;
}


void setLocale()
{
	QLocale theLocale;
	string code;
	if (lyxrc.gui_language == "auto") {
		theLocale = QLocale::system();
		code = fromqstr(theLocale.name());
	} else {
		Language const * l = languages.getLanguage(lyxrc.gui_language);
		code = l ? l->code() : "C";
		theLocale = QLocale(toqstr(code));
	}
	// Qt tries to outsmart us and transforms en_US to C.
	Messages::guiLanguage((code == "C") ? "en_US" : code);
	QLocale::setDefault(theLocale);
	setlocale(LC_NUMERIC, "C");
}


namespace frontend {


/// Return the list of loadable formats.
vector<string> loadableImageFormats()
{
	vector<string> fmts;

	QList<QByteArray> qt_formats = QImageReader::supportedImageFormats();

	LYXERR(Debug::GRAPHICS,
		"\nThe image loader can load the following directly:\n");

	if (qt_formats.empty())
		LYXERR(Debug::GRAPHICS, "\nQt Problem: No Format available!");

	bool jpeg_found = false;
	bool jpg_found = false;
	for (QList<QByteArray>::const_iterator it = qt_formats.begin(); it != qt_formats.end(); ++it) {

		LYXERR(Debug::GRAPHICS, (const char *) *it << ", ");

		string ext = ascii_lowercase((const char *) *it);
		// special case
		if (ext == "jpeg") {
			jpeg_found = true;
			if (jpg_found)
				continue;
			ext = "jpg";
		}
		else if (ext == "jpg") {
			jpg_found = true;
			if (jpeg_found)
				continue;
		}
		else if (lyxrc.use_converter_cache &&
		         (ext == "svg" || ext == "svgz") &&
		          theConverters().isReachable("svg", "png"))
			// Qt only supports SVG 1.2 tiny. See #9778. We prefer displaying
			// the SVG as in the output. However we require that the converter
			// cache is enabled since this is expensive. We also require that
			// an explicit svg->png converter is defined, since the default
			// converter could produce bad quality as well. This assumes that
			// png can always be loaded.
			continue;
		fmts.push_back(ext);
	}

	return fmts;
}


////////////////////////////////////////////////////////////////////////
//
// Icon loading support code
//
////////////////////////////////////////////////////////////////////////

namespace {

struct ImgMap {
	QString key;
	QString value;
};


bool operator<(ImgMap const & lhs, ImgMap const & rhs)
{
	return lhs.key < rhs.key;
}


class CompareKey {
public:
	CompareKey(QString const & name) : name_(name) {}
	bool operator()(ImgMap const & other) const { return other.key == name_; }
private:
	QString const name_;
};


// this must be sorted alphabetically
// Upper case comes before lower case
// Please don't change the formatting, this list is parsed by
// development/tools/generate_symbols_images.py.
ImgMap sorted_img_map[] = {
	{ "Arrownot", "arrownot2"},
	{ "Arrowvert", "arrowvert2"},
	{ "Bowtie", "bowtie2" },
	{ "Box", "box2" },
	{ "Bumpeq", "bumpeq2" },
	{ "CIRCLE", "circle3" },
	{ "Cap", "cap2" },
	{ "CheckedBox", "checkedbox2" },
	{ "Circle", "circle2" },
	{ "Colonapprox", "colonapprox2" },
	{ "Coloneq", "coloneq2" },
	{ "Coloneqq", "coloneqq2" },
	{ "Colonsim", "colonsim2" },
	{ "Cup", "cup2" },
	{ "DOWNarrow", "downarrow3" },
	{ "Delta", "delta2" },
	{ "Diamond", "diamond2" },
	{ "Doteq", "doteq2" },
	{ "Downarrow", "downarrow2" },
	{ "Eqcolon", "eqcolon2" },
	{ "Eqqcolon", "eqqcolon2" },
	{ "Gamma", "gamma2" },
	{ "Join", "join2" },
	{ "LEFTCIRCLE", "leftcircle3" },
	{ "LEFTarrow", "leftarrow3" },
	{ "LEFTcircle", "leftcircle4" },
	{ "LHD", "lhd2" },
	{ "Lambda", "lambda2" },
	{ "Lbag", "lbag2"},
	{ "Leftarrow", "leftarrow2" },
	{ "Leftcircle", "leftcircle2" },
	{ "Leftrightarrow", "leftrightarrow2" },
	{ "Longarrownot", "longarrownot2"},
	{ "Longleftarrow", "longleftarrow2" },
	{ "Longleftrightarrow", "longleftrightarrow2" },
	{ "Longmapsfrom", "longmapsfrom2"},
	{ "Longmapsto", "longmapsto2"},
	{ "Longrightarrow", "longrightarrow2" },
	{ "Mapsfrom", "mapsfrom2"},
	{ "Mapsfromchar", "mapsfromchar2"},
	{ "Mapsto", "mapsto2"},
	{ "Mapstochar", "mapstochar2"},
	{ "Omega", "omega2" },
	{ "Phi", "phi2" },
	{ "Pi", "pi2" },
	{ "Psi", "psi2" },
	{ "RHD", "rhd2" },
	{ "RIGHTCIRCLE", "rightcircle3" },
	{ "RIGHTarrow", "rightarrow3" },
	{ "RIGHTcircle", "rightcircle4" },
	{ "Rbag", "rbag2"},
	{ "Rightarrow", "rightarrow2" },
	{ "Rightcircle", "rightcircle2" },
	{ "Sigma", "sigma2" },
	{ "Square", "square2" },
	{ "Subset", "subset2" },
	{ "Supset", "supset2" },
	{ "Theta", "theta2" },
	{ "Thorn", "thorn2" },
	{ "UParrow", "uparrow3" },
	{ "Uparrow", "uparrow2" },
	{ "Updownarrow", "updownarrow2" },
	{ "Upsilon", "upsilon2" },
	{ "Vdash", "vdash3" },
	{ "Vert", "vert2" },
	{ "XBox", "xbox3" },
	{ "Xbox", "xbox2" },
	{ "Xi", "xi2" },
	{ "lVert", "vert2" },
	{ "lvert", "vert" },
	{ "nLeftarrow", "nleftarrow2" },
	{ "nLeftrightarrow", "nleftrightarrow2" },
	{ "nRightarrow", "nrightarrow2" },
	{ "nVDash", "nvdash3" },
	{ "nVdash", "nvdash4" },
	{ "nvDash", "nvdash2" },
	{ "rVert", "vert2" },
	{ "rvert", "vert" },
	{ "textrm \\AA", "textrm_AA"},
	{ "textrm \\O", "textrm_O"},
	{ "vDash", "vdash2" },
	{ "varDelta", "vardelta2" },
	{ "varGamma", "vargamma2" },
	{ "varLambda", "varlambda2" },
	{ "varOmega", "varomega2" },
	{ "varPhi", "varphi2" },
	{ "varPi", "varpi2" },
	{ "varPsi", "varpsi2" },
	{ "varSigma", "varsigma2" },
	{ "varTheta", "vartheta2" },
	{ "varUpsilon", "varupsilon2" },
	{ "varXi", "varxi2" }
};


size_t const nr_sorted_img_map = sizeof(sorted_img_map) / sizeof(ImgMap);

// This list specifies which system's theme icon is related to which lyx
// command. It was based on:
// http://standards.freedesktop.org/icon-naming-spec/icon-naming-spec-latest.html
// this must be sorted alphabetically
// Upper case comes before lower case
ImgMap sorted_theme_icon_map[] = {
	{ "bookmark-goto 0", "go-jump" },
	{ "buffer-new", "document-new" },
	{ "buffer-write", "document-save" },
	{ "buffer-write-as", "document-save-as" },
	{ "buffer-zoom-in", "zoom-in" },
	{ "buffer-zoom-out", "zoom-out" },
	{ "copy", "edit-copy" },
	{ "cut", "edit-cut" },
	{ "depth-decrement", "format-indent-less" },
	{ "depth-increment", "format-indent-more" },
	{ "dialog-show spellchecker", "tools-check-spelling" },
	{ "dialog-show-new-inset graphics", "insert-image" },
	{ "dialog-toggle findreplaceadv", "edit-find-replace" },
	{ "file-open", "document-open" },
	{ "font-bold", "format-text-bold" },
	{ "font-ital", "format-text-italic" },
	{ "font-strikeout", "format-text-strikethrough" },
	{ "font-underline", "format-text-underline" },
	{ "lyx-quit", "application-exit" },
	{ "paste", "edit-paste" },
	{ "redo", "edit-redo" },
	{ "undo", "edit-undo" },
	{ "window-close", "window-close" },
	{ "window-new", "window-new" }
};

size_t const nr_sorted_theme_icon_map = sizeof(sorted_theme_icon_map) / sizeof(ImgMap);


QString findImg(QString const & name)
{
	ImgMap const * const begin = sorted_img_map;
	ImgMap const * const end = begin + nr_sorted_img_map;
	LATTEST(sorted(begin, end));

	ImgMap const * const it = find_if(begin, end, CompareKey(name));

	QString img_name;
	if (it != end) {
		img_name = it->value;
	} else {
		img_name = name;
		img_name.replace('_', "underscore");
		img_name.replace(' ', '_');

		// This way we can have "math-delim { }" on the toolbar.
		img_name.replace('(', "lparen");
		img_name.replace(')', "rparen");
		img_name.replace('[', "lbracket");
		img_name.replace(']', "rbracket");
		img_name.replace('{', "lbrace");
		img_name.replace('}', "rbrace");
		img_name.replace('|', "bars");
		img_name.replace(',', "thinspace");
		img_name.replace(':', "mediumspace");
		img_name.replace(';', "thickspace");
		img_name.replace('!', "negthinspace");
	}

	LYXERR(Debug::GUI, "findImg(" << name << ")\n"
		<< "Looking for math icon called \"" << img_name << '"');
	return img_name;
}

} // namespace


QString themeIconName(QString const & action)
{
	ImgMap const * const begin = sorted_theme_icon_map;
	ImgMap const * const end = begin + nr_sorted_theme_icon_map;
	LASSERT(sorted(begin, end), /**/);

	ImgMap const * const it = find_if(begin, end, CompareKey(action));

	if (it != end)
		return it->value;
	return QString();
}


namespace {

/* Get aliases for icon name. This allows to avoid duplication of
 * icons when new versions of functions are introduced for the
 * toolbar. A good example is the introduction of layout-toggle in
 * #9864.
 * The file is parsed by Lexer. Each line is of the form
 *     <original substring> <replacement substring>
 *
 * The return value is another icon file name that can be queried.
 */
// FIXME: consider using regular expressions.
QString getAlias(QString name) {
	static bool has_aliases = false;
	static vector <pair<QString,QString>> aliases;
	// Initialize aliases list (once).
	if (!has_aliases) {
		FileName alfile = libFileSearch("images", "icon.aliases");
		if (alfile.exists()) {
			Lexer lex;
			lex.setFile(alfile);
			while(lex.isOK()) {
				string from, to;
				lex >> from >> to;
				if (!from.empty())
					aliases.push_back({toqstr(from), toqstr(to)});
			}
		}
		has_aliases = true;
	}
	// check for the following aliases
	for (auto const & alias : aliases) {
		if (name.contains(alias.first))
			name.replace(alias.first, alias.second);
	}
	return name;
}

} // namespace


IconInfo iconInfo(FuncRequest const & f, bool unknown, bool rtl)
{
	IconInfo res;

	QStringList names;
	QString lfunname = toqstr(lyxaction.getActionName(f.action()));

	if (!f.argument().empty()) {
		// if there are arguments, first search an icon which name is the full thing
		QString name = lfunname  + ' ' + toqstr(f.argument());
		name.replace(' ', '_');
		name.replace(';', '_');
		name.replace('\\', "backslash");
		names << name;
		QString alias = getAlias(name);
		if (alias != name)
			names << alias;

		// then special default icon for some lfuns
		switch (f.action()) {
		case LFUN_MATH_INSERT:
			names <<  "math/" + findImg(toqstr(f.argument()).mid(1));
			break;
		case LFUN_MATH_DELIM:
		case LFUN_MATH_BIGDELIM:
			names << "math/" + findImg(toqstr(f.argument()));
			break;
		case LFUN_CALL:
			names << "commands/" + toqstr(f.argument());
			break;
		case LFUN_COMMAND_ALTERNATIVES: {
			// use the first of the alternative commands
			docstring firstcom;
			docstring dummy = split(f.argument(), firstcom, ';');
			QString name1 = toqstr(firstcom);
			name1.replace(' ', '_');
			name1.replace(';', '_');
			name1.replace('\\', "backslash");
			names << name1;
			break;
		}
		default:
			break;
		}
	}

	// next thing to try is function name alone
	names << lfunname;
	QString alias = getAlias(lfunname);
	if (alias != lfunname)
		names << alias;

	// and finally maybe the unknown icon
	if (unknown)
		names << "unknown";

	search_mode const mode = theGuiApp() ? theGuiApp()->imageSearchMode() : support::must_exist;
	bool const dark_mode = theGuiApp() ? theGuiApp()->isInDarkMode() : false;
	// The folders where icons are searched for
	QStringList imagedirs;
	imagedirs << "images/ipa/" << "images/";
	// This is used to search for rtl version of icons which have the +rtl suffix.
	QStringList suffixes;
	if (rtl)
		suffixes << "+rtl";
	suffixes << QString();

	for (QString const & imagedir : imagedirs)
		for (QString const & name : names)
			for (QString const & suffix : suffixes) {
				QString id = imagedir;
				FileName fname = imageLibFileSearch(id, name + suffix, "svgz,png", mode, dark_mode);
				if (fname.exists()) {
					docstring const fpath = fname.absoluteFilePath();
					res.filepath = toqstr(fname.absFileName());
					// these icons are subject to inversion in dark mode
					res.invert = (contains(fpath, from_ascii("math")) || contains(fpath, from_ascii("ert-insert"))
						      || suffixIs(fname.onlyPath().absoluteFilePath(), from_ascii("ipa")));
					res.swap = rtl && suffix.isEmpty();
					return res;
				}
			}

	LYXERR(Debug::GUI, "Cannot find icon for command \""
			   << lyxaction.getActionName(f.action())
			   << '(' << to_utf8(f.argument()) << ")\"");

	return res;
}


QPixmap GuiApplication::prepareForDarkMode(QPixmap pixmap) const
{
	// guess whether we are in dark mode
	if (!theGuiApp()->isInDarkMode())
		// not in dark mode, do nothing
		return pixmap;

	// create a layer with black text turned to QPalette::WindowText
	QPixmap black_overlay(pixmap.size());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	QColor text_color = theGuiApp()->style()->standardPalette().color(QPalette::Active, QPalette::WindowText);
#else
	QPalette palette = QPalette();
	QColor text_color = palette.color(QPalette::Active, QPalette::WindowText);
#endif
	black_overlay.fill(text_color);
	black_overlay.setMask(pixmap.createMaskFromColor(Qt::black, Qt::MaskOutColor));

	// create a layer with blue text turned to lighter blue
	QPixmap blue_overlay(pixmap.size());
	QColor math_blue(0, 0, 255);
	blue_overlay.fill(guiApp->colorCache().get(Color(Color_math)));
	blue_overlay.setMask(pixmap.createMaskFromColor(math_blue, Qt::MaskOutColor));

	// create a layer with ("latex") red text turned to lighter red
	QPixmap red_overlay(pixmap.size());
	QColor math_red(128, 0, 0);
	red_overlay.fill(guiApp->colorCache().get(Color(Color_latex)));
	red_overlay.setMask(pixmap.createMaskFromColor(math_red, Qt::MaskOutColor));

	// put layers on top of existing pixmap
	QPainter painter(&pixmap);
	painter.drawPixmap(pixmap.rect(), black_overlay);
	painter.drawPixmap(pixmap.rect(), blue_overlay);
	painter.drawPixmap(pixmap.rect(), red_overlay);

	return pixmap;
}


QPixmap getPixmap(QString const & path, QString const & name, QString const & ext)
{
	QString imagedir = path;
	FileName fname = imageLibFileSearch(imagedir, name, ext, theGuiApp()->imageSearchMode(),
					    theGuiApp()->isInDarkMode());
	QString fpath = toqstr(fname.absFileName());
	QPixmap pixmap = QPixmap();

	if (pixmap.load(fpath)) {
		if (fpath.contains("math") || fpath.contains("ipa")
		    || fpath.contains("bullets"))
			return guiApp->prepareForDarkMode(pixmap);
		return pixmap;
	}

	bool const list = ext.contains(",");
	LYXERR(Debug::GUI, "Cannot load pixmap \""
			<< path << "/" << name << "." << (list ? "{" : "") << ext
		<< (list ? "}" : "") << "\".");

	return QPixmap();
}


QIcon getIcon(FuncRequest const & f, bool unknown, bool rtl)
{
	if (lyxrc.use_system_theme_icons) {
		// use the icons from system theme that are available
		QString action = toqstr(lyxaction.getActionName(f.action()));
		if (!f.argument().empty())
			action += " " + toqstr(f.argument());
		QString const theme_icon = themeIconName(action);
		if (QIcon::hasThemeIcon(theme_icon)) {
			QIcon const thmicn = QIcon::fromTheme(theme_icon);
			if (!thmicn.isNull())
				return thmicn;
		}
	}

	IconInfo icondata = iconInfo(f, unknown, rtl);
	if (icondata.filepath.isEmpty())
		return QIcon();

	//LYXERR(Debug::GUI, "Found icon: " << icon);
	QPixmap pixmap = QPixmap();
	if (!pixmap.load(icondata.filepath)) {
		LYXERR0("Cannot load icon " << icondata.filepath << ".");
		return QIcon();
	}

	if (icondata.invert)
		pixmap = guiApp->prepareForDarkMode(pixmap);

	if (icondata.swap)
		return QIcon(pixmap.transformed(QTransform().scale(-1, 1)));
	else
		return QIcon(pixmap);
}


////////////////////////////////////////////////////////////////////////
//
// LyX server support code.
//
////////////////////////////////////////////////////////////////////////

class SocketNotifier : public QSocketNotifier
{
public:
	/// connect a connection notification from the LyXServerSocket
	SocketNotifier(QObject * parent, int fd, Application::SocketCallback const & func)
		: QSocketNotifier(fd, QSocketNotifier::Read, parent), func_(func)
	{}

public:
	/// The callback function
	Application::SocketCallback func_;
};


class GuiTranslator : public QTranslator
{
public:
	GuiTranslator(QObject * parent = nullptr)
		: QTranslator(parent)
	{}

	QString translate(const char * /* context */,
		const char *sourceText,
		const char * /* disambiguation */ = nullptr, int /* n */ = -1) const override
	{
		// Here we declare the strings that need to be translated from Qt own GUI
		// This is needed to include these strings to po files
		_("About %1");
		_("Preferences");
		_("Reconfigure");
		_("Restore Defaults");
		_("Quit %1");
		_("&OK");
		// Already in po: "Cancel", "&Cancel"
		_("Apply"); // Already in po: "&Apply"
		_("Reset"); // Already in po: "&Reset" "R&eset" "Rese&t"
		_("Open");
		_("Select Color");

		docstring s = getGuiMessages().getIfFound(sourceText);
		// This test should eventually be removed when translations are updated
		if (s.empty())
			LYXERR(Debug::LOCALE, "Missing translation for `"
			       << string(sourceText) << "'");
		return toqstr(s);
	}
};


////////////////////////////////////////////////////////////////////////
//
// Mac specific stuff goes here...
//
////////////////////////////////////////////////////////////////////////

#if defined(Q_OS_MAC) && (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
// QMacPasteboardMimeGraphics can only be compiled on Mac.

class QMacPasteboardMimeGraphics : public QMacPasteboardMime
{
public:
	QMacPasteboardMimeGraphics()
		: QMacPasteboardMime(MIME_QT_CONVERTOR|MIME_ALL)
	{}

	QString convertorName() { return "Graphics"; }

	QString flavorFor(QString const & mime)
	{
		LYXERR(Debug::ACTION, "flavorFor " << mime);
		if (mime == pdfMimeType())
			return QLatin1String("com.adobe.pdf");
		return QString();
	}

	QString mimeFor(QString flav)
	{
		LYXERR(Debug::ACTION, "mimeFor " << flav);
		if (flav == QLatin1String("com.adobe.pdf"))
			return pdfMimeType();
		return QString();
	}

	bool canConvert(QString const & mime, QString flav)
	{
		return mimeFor(flav) == mime;
	}

	QVariant convertToMime(QString const & /*mime*/, QList<QByteArray> data,
		QString /*flav*/)
	{
		if(data.count() > 1)
			qWarning("QMacPasteboardMimeGraphics: Cannot handle multiple member data");
		return data.first();
	}

	QList<QByteArray> convertFromMime(QString const & /*mime*/,
		QVariant data, QString /*flav*/)
	{
		QList<QByteArray> ret;
		ret.append(data.toByteArray());
		return ret;
	}
};
#endif

///////////////////////////////////////////////////////////////
//
// You can find more platform specific stuff at the end of this file...
//
///////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// Windows specific stuff goes here...

#if defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN)
// QWindowsMimeMetafile can only be compiled on Windows.

static FORMATETC cfFromMime(QString const & mimetype)
{
	FORMATETC formatetc;
	if (mimetype == emfMimeType()) {
		formatetc.cfFormat = CF_ENHMETAFILE;
		formatetc.tymed = TYMED_ENHMF;
	} else if (mimetype == wmfMimeType()) {
		formatetc.cfFormat = CF_METAFILEPICT;
		formatetc.tymed = TYMED_MFPICT;
	}
	formatetc.ptd = 0;
	formatetc.dwAspect = DVASPECT_CONTENT;
	formatetc.lindex = -1;
	return formatetc;
}


class QWindowsMimeMetafile : public QWINDOWSMIME {
public:
	QWindowsMimeMetafile() {}

	bool canConvertFromMime(FORMATETC const & /*formatetc*/,
		QMimeData const * /*mimedata*/) const override
	{
		return false;
	}

	bool canConvertToMime(QString const & mimetype,
		IDataObject * pDataObj) const override
	{
		if (mimetype != emfMimeType() && mimetype != wmfMimeType())
			return false;
		FORMATETC formatetc = cfFromMime(mimetype);
		return pDataObj->QueryGetData(&formatetc) == S_OK;
	}

	bool convertFromMime(FORMATETC const & /*formatetc*/,
		const QMimeData * /*mimedata*/, STGMEDIUM * /*pmedium*/) const override
	{
		return false;
	}

	QVariant convertToMime(QString const & mimetype, IDataObject * pDataObj,
		QVARIANTTYPE /*preferredType*/) const override
	{
		QByteArray data;
		if (!canConvertToMime(mimetype, pDataObj))
			return data;

		FORMATETC formatetc = cfFromMime(mimetype);
		STGMEDIUM s;
		if (pDataObj->GetData(&formatetc, &s) != S_OK)
			return data;

		int dataSize;
		if (s.tymed == TYMED_ENHMF) {
			dataSize = GetEnhMetaFileBits(s.hEnhMetaFile, 0, 0);
			data.resize(dataSize);
			dataSize = GetEnhMetaFileBits(s.hEnhMetaFile, dataSize,
				(LPBYTE)data.data());
		} else if (s.tymed == TYMED_MFPICT) {
			dataSize = GetMetaFileBitsEx((HMETAFILE)s.hMetaFilePict, 0, 0);
			data.resize(dataSize);
			dataSize = GetMetaFileBitsEx((HMETAFILE)s.hMetaFilePict, dataSize,
				(LPBYTE)data.data());
		}
		data.detach();
		ReleaseStgMedium(&s);

		return data;
	}


	QVector<FORMATETC> formatsForMime(QString const & mimetype,
		QMimeData const * /*mimedata*/) const override
	{
		QVector<FORMATETC> formats;
		if (mimetype == emfMimeType() || mimetype == wmfMimeType())
			formats += cfFromMime(mimetype);
		return formats;
	}

	QString mimeForFormat(FORMATETC const & formatetc) const override
	{
		switch (formatetc.cfFormat) {
		case CF_ENHMETAFILE:
			return emfMimeType();
		case CF_METAFILEPICT:
			return wmfMimeType();
		}
		return QString();
	}
};

#endif


/// Allows to check whether ESC was pressed during a long operation
class KeyChecker : public QObject {
private:
	bool pressed_;
	bool started_;
public:
	KeyChecker() : pressed_(false), started_(false) {}

	void start() {
		QCoreApplication::instance()->installEventFilter(this);
		pressed_ = false;
		started_ = true;
	}
	void stop() {
		QCoreApplication::instance()->removeEventFilter(this);
		started_ = false;
	}
	bool pressed() {
		QCoreApplication::processEvents();
		return pressed_;
	}
	bool started() const {
		return started_;
	}
	bool eventFilter(QObject *obj, QEvent *event) override {
		//LYXERR(Debug::ACTION, "Event Type: " << event->type());
		switch (event->type()) {
		case QEvent::Show:
		case QEvent::Hide:
		case QEvent::Resize:
		case QEvent::UpdateRequest:
		case QEvent::CursorChange:
		case QEvent::ActionChanged:
		case QEvent::EnabledChange:
		case QEvent::SockAct:
		case QEvent::Timer:
		case QEvent::Paint:
		case QEvent::ToolTipChange:
		case QEvent::LayoutRequest:
		case QEvent::MetaCall:
			return QObject::eventFilter(obj, event);
		default:
			// FIXME Blocking all these events is a bad idea.
			QKeyEvent *keyEvent = dynamic_cast<QKeyEvent*>(event);
			if (keyEvent && keyEvent->key() == Qt::Key_Escape)
				pressed_ = true;
			return true;
		}
	}
};


////////////////////////////////////////////////////////////////////////
// GuiApplication::Private definition and implementation.
////////////////////////////////////////////////////////////////////////

struct GuiApplication::Private
{
	Private(): language_model_(nullptr), meta_fake_bit(NoModifier),
		global_menubar_(nullptr), last_state_(Qt::ApplicationInactive)
	{
	#if defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN)
		/// WMF Mime handler for Windows clipboard.
		wmf_mime_ = new QWindowsMimeMetafile;
	#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) && QT_VERSION < QT_VERSION_CHECK(6, 5, 0))
		win_app_ = dynamic_cast<QWindowsApplication *>
			(QGuiApplicationPrivate::platformIntegration());
		win_app_->registerMime(wmf_mime_);
	#endif
	#endif
		initKeySequences(&theTopLevelKeymap());
	}

	#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) && QT_VERSION < QT_VERSION_CHECK(6, 5, 0))
	#if defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN)
	~Private()
	{
		win_app_->unregisterMime(wmf_mime_);
	}
	#endif
	#endif

	void initKeySequences(KeyMap * kb)
	{
		keyseq = KeySequence(kb, kb);
		cancel_meta_seq = KeySequence(kb, kb);
	}

	///
	QSortFilterProxyModel * language_model_;
	///
	GuiClipboard clipboard_;
	///
	GuiSelection selection_;
	///
	FontLoader font_loader_;
	///
	ColorCache color_cache_;
	/// the built-in Qt translation mechanism
	QTranslator qt_trans_;
	/// LyX gettext-based translation for Qt elements
	GuiTranslator gui_trans_;
	///
	QHash<int, SocketNotifier *> socket_notifiers_;
	///
	Menus menus_;
	///
	/// The global instance
	Toolbars toolbars_;

	/// this timer is used for any regular events one wants to
	/// perform. at present it is used to check if forked processes
	/// are done.
	QTimer general_timer_;

	/// delayed FuncRequests
	std::queue<FuncRequest> func_request_queue_;

	///
	KeySequence keyseq;
	///
	KeySequence cancel_meta_seq;
	///
	KeyModifier meta_fake_bit;

	/// input method uses this to preserve initial input item transform
	bool first_work_area = true;
	/// geometry of the input item of the first working area
	QRectF item_rect_base_;
	/// input item transformation of the first working area
	QTransform item_trans_base_;

	/// The result of last dispatch action
	DispatchResult dispatch_result_;

	/// Multiple views container.
	/**
	* Warning: This must not be a smart pointer as the destruction of the
	* object is handled by Qt when the view is closed
	* \sa Qt::WA_DeleteOnClose attribute.
	*/
	QHash<int, GuiView *> views_;

	/// Only used on mac.
	QMenuBar * global_menubar_;
	/// Holds previous application state on Mac
	Qt::ApplicationState last_state_;

#if defined(Q_OS_MAC) && (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	/// Linkback mime handler for macOS.
	QMacPasteboardMimeGraphics mac_pasteboard_mime_;
#endif

#if defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN)
	/// WMF Mime handler for Windows clipboard.
	QWindowsMimeMetafile * wmf_mime_;
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0) && QT_VERSION < QT_VERSION_CHECK(6, 5, 0))
	QWindowsApplication * win_app_;
#endif
#endif

	/// Allows to check whether ESC was pressed during a long operation
	KeyChecker key_checker_;
};


GuiApplication * guiApp;

GuiApplication::~GuiApplication()
{
#ifdef Q_OS_MAC
	closeAllLinkBackLinks();
#endif
	delete d;
}


GuiApplication::GuiApplication(int & argc, char ** argv)
	: QApplication(argc, argv), current_view_(nullptr),
	  d(new GuiApplication::Private)
{
	QString app_name = "LyX";
	QCoreApplication::setOrganizationName(app_name);
	QCoreApplication::setOrganizationDomain("lyx.org");
	QCoreApplication::setApplicationName(lyx_package);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

	setDesktopFileName(lyx_package);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
	QRandomGenerator(QDateTime::currentDateTime().toSecsSinceEpoch());
#else
	qsrand(QDateTime::currentDateTime().toTime_t());
#endif

	// Install LyX translator for missing Qt translations
	installTranslator(&d->gui_trans_);
	// Install Qt native translator for GUI elements.
	installTranslator(&d->qt_trans_);

	if (platformName() == "xcb") {
#if defined(HAVE_XCB_XCB_H) && defined(HAVE_LIBXCB)
		// Enable reception of XCB events.
		installNativeEventFilter(this);
#else
		LYXERR0("Warning: X11 support is incomplete in this LyX binary.");
#endif
	}

	// FIXME: quitOnLastWindowClosed is true by default. We should have a
	// lyxrc setting for this in order to let the application stay resident.
	// But then we need some kind of dock icon, at least on Windows.
	/*
	if (lyxrc.quit_on_last_window_closed)
		setQuitOnLastWindowClosed(false);
	*/
#ifdef Q_OS_MAC
	// FIXME: Do we need a lyxrc setting for this on Mac? This behaviour
	// seems to be the default case for applications like LyX.
	setQuitOnLastWindowClosed(false);
	///
	setupApplescript();
	appleCleanupEditMenu();
	appleCleanupViewMenu();
	connect(this, SIGNAL(applicationStateChanged(Qt::ApplicationState)),
			this, SLOT(onApplicationStateChanged(Qt::ApplicationState)));
#endif

	if (platformName() == "xcb") {
		// doubleClickInterval() is 400 ms on X11 which is just too long.
		// On Windows and macOS, the operating system's value is used.
		// On Microsoft Windows, calling this function sets the double
		// click interval for all applications. So we don't!
		QApplication::setDoubleClickInterval(300);
	}

	connect(this, SIGNAL(lastWindowClosed()), this, SLOT(onLastWindowClosed()));

	// needs to be done before reading lyxrc
	QWidget w;
	lyxrc.dpi = (w.logicalDpiX() + w.logicalDpiY()) / 2;

	guiApp = this;

	// Initialize RC Fonts
	if (lyxrc.roman_font_name.empty())
		lyxrc.roman_font_name = fromqstr(romanFontName());

	if (lyxrc.sans_font_name.empty())
		lyxrc.sans_font_name = fromqstr(sansFontName());

	if (lyxrc.typewriter_font_name.empty())
		lyxrc.typewriter_font_name = fromqstr(typewriterFontName());

	// Track change of keyboard
	connect(inputMethod(), SIGNAL(localeChanged()), this, SLOT(onLocaleChanged()));

	d->general_timer_.setInterval(500);
	connect(&d->general_timer_, SIGNAL(timeout()),
		this, SLOT(handleRegularEvents()));
	d->general_timer_.start();

	// maxThreadCount() defaults in general to 2 on single or dual-processor.
	// This is clearly not enough in a time where we use threads for
	// document preview and/or export. 20 should be OK.
	QThreadPool::globalInstance()->setMaxThreadCount(20);

	// make sure tooltips are formatted
	installEventFilter(new ToolTipFormatter(this));
}


GuiApplication * theGuiApp()
{
	return dynamic_cast<GuiApplication *>(theApp());
}


double GuiApplication::pixelRatio() const
{
	return devicePixelRatio();
}


void GuiApplication::syncZoomSliders(const int v)
{
	// Sync zoom in all active views
	QList<int> const ids = viewIds();
	for (int i = 0; i != ids.size(); ++i)
		view(ids[i]).setCurrentZoom(v);
}


void GuiApplication::clearSession()
{
	QSettings settings;
	settings.clear();
}


docstring Application::iconName(FuncRequest const & f, bool unknown)
{
	return qstring_to_ucs4(lyx::frontend::iconInfo(f, unknown, false).filepath);
}


docstring Application::mathIcon(docstring const & c)
{
	return qstring_to_ucs4(findImg(toqstr(c)));
}

void Application::applyPrefs()
{
	if (!guiApp)
		return;
	if (lyxrc.ui_style != "default")
		lyx::frontend::GuiApplication::setStyle(toqstr(lyxrc.ui_style));
#if (defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN) || defined(Q_OS_MAC)) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
	// Set color scheme from prefs
	if (lyxrc.color_scheme == "dark")
		guiApp->styleHints()->setColorScheme(Qt::ColorScheme::Dark);
	else if (lyxrc.color_scheme == "light")
		guiApp->styleHints()->setColorScheme(Qt::ColorScheme::Light);
#endif
}

FuncStatus GuiApplication::getStatus(FuncRequest const & cmd) const
{
	FuncStatus status;

	BufferView * bv = nullptr;
	BufferView * doc_bv = nullptr;

	if (cmd.action() == LFUN_NOACTION) {
		status.message(from_utf8(N_("Nothing to do")));
		status.setEnabled(false);
	}

	else if (cmd.action() == LFUN_UNKNOWN_ACTION) {
		status.setUnknown(true);
		status.message(from_utf8(N_("Unknown action")));
		status.setEnabled(false);
	}

	// Does the GuiApplication know something?
	else if (getStatus(cmd, status)) { }

	// If we do not have a GuiView, then other functions are disabled
	else if (!current_view_)
		status.setEnabled(false);

	// Does the GuiView know something?
	else if (current_view_->getStatus(cmd, status))	{ }

	// In LyX/Mac, when a dialog is open, the menus of the
	// application can still be accessed without giving focus to
	// the main window. In this case, we want to disable the menu
	// entries that are buffer or view-related.
	//FIXME: Abdel (09/02/10) This has very bad effect on Linux, don't know why...
	/*
	else if (cmd.origin() == FuncRequest::MENU && !current_view_->hasFocus())
		status.setEnabled(false);
	*/

	// If we do not have a BufferView, then other functions are disabled
	else if (!(bv = current_view_->currentBufferView()))
		status.setEnabled(false);

	// Does the current BufferView know something?
	else if (bv->getStatus(cmd, status)) { }

	// Does the current Buffer know something?
	else if (bv->buffer().getStatus(cmd, status)) { }

	// If we do not have a document BufferView, different from the
	// current BufferView, then other functions are disabled
	else if (!(doc_bv = current_view_->documentBufferView()) || doc_bv == bv)
		status.setEnabled(false);

	// Does the document Buffer know something?
	else if (doc_bv->buffer().getStatus(cmd, status)) { }

	else {
		LYXERR(Debug::ACTION, "LFUN not handled in getStatus(): " << cmd);
		status.message(from_utf8(N_("Command not handled")));
		status.setEnabled(false);
	}

	// the default error message if we disable the command
	if (!status.enabled() && status.message().empty())
		status.message(from_utf8(N_("Command disabled")));

	return status;
}


bool GuiApplication::getStatus(FuncRequest const & cmd, FuncStatus & flag) const
{
	// I would really like to avoid having this switch and rather try to
	// encode this in the function itself.
	// -- And I'd rather let an inset decide which LFUNs it is willing
	// to handle (Andre')
	bool enable = true;
	switch (cmd.action()) {

	// This could be used for the no-GUI version. The GUI version is handled in
	// GuiView::getStatus(). See above.
	/*
	case LFUN_BUFFER_WRITE:
	case LFUN_BUFFER_WRITE_AS:
	case LFUN_BUFFER_WRITE_AS_TEMPLATE: {
		Buffer * b = theBufferList().getBuffer(FileName(cmd.getArg(0)));
		enable = b && (b->isUnnamed() || !b->isClean());
		break;
	}
	*/

	case LFUN_BOOKMARK_GOTO: {
		const unsigned int num = convert<unsigned int>(to_utf8(cmd.argument()));
		enable = theSession().bookmarks().isValid(num);
		break;
	}

	case LFUN_BOOKMARK_CLEAR:
		enable = theSession().bookmarks().hasValid();
		break;

	// this one is difficult to get right. As a half-baked
	// solution, we consider only the first action of the sequence
	case LFUN_COMMAND_SEQUENCE: {
		// argument contains ';'-terminated commands
		string const firstcmd = token(to_utf8(cmd.argument()), ';', 0);
		FuncRequest func(lyxaction.lookupFunc(firstcmd));
		func.setOrigin(cmd.origin());
		flag = getStatus(func);
		break;
	}

	// we want to check if at least one of these is enabled
	case LFUN_COMMAND_ALTERNATIVES: {
		// argument contains ';'-terminated commands
		string arg = to_utf8(cmd.argument());
		while (!arg.empty()) {
			string first;
			arg = split(arg, first, ';');
			FuncRequest func(lyxaction.lookupFunc(first));
			func.setOrigin(cmd.origin());
			flag = getStatus(func);
			// if this one is enabled, the whole thing is
			if (flag.enabled())
				break;
		}
		break;
	}

	case LFUN_CALL: {
		FuncRequest func;
		string name = to_utf8(cmd.argument());
		if (theTopLevelCmdDef().lock(name, func)) {
			func.setOrigin(cmd.origin());
			flag = getStatus(func);
			theTopLevelCmdDef().release(name);
		} else {
			// catch recursion or unknown command
			// definition. all operations until the
			// recursion or unknown command definition
			// occurs are performed, so set the state to
			// enabled
			enable = true;
		}
		break;
	}

	case LFUN_IF_RELATIVES: {
		string const lfun = to_utf8(cmd.argument());
		BufferView const * bv =
			current_view_ ? current_view_->currentBufferView() : nullptr;
		if (!bv || (bv->buffer().parent() == nullptr && !bv->buffer().hasChildren())) {
			enable = false;
			break;
		}
		FuncRequest func(lyxaction.lookupFunc(lfun));
		func.setOrigin(cmd.origin());
		flag = getStatus(func);
		break;
	}

	case LFUN_CURSOR_FOLLOWS_SCROLLBAR_TOGGLE:
	case LFUN_REPEAT:
	case LFUN_PREFERENCES_SAVE:
	case LFUN_BUFFER_SAVE_AS_DEFAULT:
		// these are handled in our dispatch()
		break;

	case LFUN_DEBUG_LEVEL_SET: {
		string bad = Debug::badValue(to_utf8(cmd.argument()));
		enable = bad.empty();
		if (!bad.empty())
			flag.message(bformat(_("Bad debug value `%1$s'."), from_utf8(bad)));
		break;
	}

	case LFUN_WINDOW_CLOSE:
		enable = !d->views_.empty();
		break;

	case LFUN_BUFFER_NEW:
	case LFUN_BUFFER_NEW_TEMPLATE:
	case LFUN_FILE_OPEN:
	case LFUN_LYXFILES_OPEN:
	case LFUN_HELP_OPEN:
	case LFUN_SCREEN_FONT_UPDATE:
	case LFUN_SET_COLOR:
	case LFUN_WINDOW_NEW:
	case LFUN_LYX_QUIT:
	case LFUN_LYXRC_APPLY:
	case LFUN_COMMAND_PREFIX:
	case LFUN_CANCEL:
	case LFUN_META_PREFIX:
	case LFUN_RECONFIGURE:
	case LFUN_SERVER_GET_FILENAME:
	case LFUN_SERVER_NOTIFY:
		enable = true;
		break;

	case LFUN_BUFFER_FORALL: {
		if (theBufferList().empty()) {
			flag.message(from_utf8(N_("Command not allowed without a buffer open")));
			flag.setEnabled(false);
			break;
		}

		FuncRequest const cmdToPass = lyxaction.lookupFunc(cmd.getLongArg(0));
		if (cmdToPass.action() == LFUN_UNKNOWN_ACTION) {
			flag.message(from_utf8(N_("the <LFUN-COMMAND> argument of buffer-forall is not valid")));
			flag.setEnabled(false);
		}
		break;
	}

	case LFUN_DIALOG_SHOW: {
		string const name = cmd.getArg(0);
		return name == "aboutlyx"
			|| name == "lyxfiles"
			|| name == "prefs"
			|| name == "texinfo"
			|| name == "progress"
			|| name == "compare";
	}

	default:
		return false;
	}

	if (!enable)
		flag.setEnabled(false);
	return true;
}

/// make a post-dispatch status message
static docstring makeDispatchMessage(docstring const & msg,
				     FuncRequest const & cmd)
{
	const bool be_verbose = (cmd.origin() == FuncRequest::MENU
			      || cmd.origin() == FuncRequest::TOOLBAR
			      || cmd.origin() == FuncRequest::COMMANDBUFFER);

	if (cmd.action() == LFUN_SELF_INSERT || !be_verbose) {
		LYXERR(Debug::ACTION, "dispatch msg is `" << msg << "'");
		return msg;
	}

	docstring dispatch_msg = msg;
	if (!dispatch_msg.empty())
		dispatch_msg += ' ';

	docstring comname = from_utf8(lyxaction.getActionName(cmd.action()));

	bool argsadded = false;

	if (!cmd.argument().empty()) {
		if (cmd.action() != LFUN_UNKNOWN_ACTION) {
			comname += ' ' + cmd.argument();
			argsadded = true;
		}
	}
	docstring const shortcuts = theTopLevelKeymap().
		printBindings(cmd, KeySequence::ForGui);

	if (!shortcuts.empty())
		comname += ": " + shortcuts;
	else if (!argsadded && !cmd.argument().empty())
		comname += ' ' + cmd.argument();

	if (!comname.empty()) {
		comname = rtrim(comname);
		dispatch_msg += '(' + rtrim(comname) + ')';
	}
	LYXERR(Debug::ACTION, "verbose dispatch msg " << to_utf8(dispatch_msg));
	return dispatch_msg;
}


DispatchResult const & GuiApplication::dispatch(FuncRequest const & cmd)
{
	DispatchResult dr;

	Buffer * buffer = nullptr;
	if (cmd.view_origin() && current_view_ != cmd.view_origin()) {
		//setCurrentView(cmd.view_origin); //does not work
		dr.setError(true);
		dr.setMessage(_("Wrong focus!"));
		d->dispatch_result_ = dr;
		return d->dispatch_result_;
	}
	if (current_view_ && current_view_->currentBufferView()) {
		current_view_->currentBufferView()->cursor().saveBeforeDispatchPosXY();
		buffer = &current_view_->currentBufferView()->buffer();
	}

	dr.screenUpdate(Update::FitCursor);
	{
		// All the code is kept inside the undo group because
		// updateBuffer can create undo actions (see #11292)
		UndoGroupHelper ugh(buffer);
		dispatch(cmd, dr);
		if (dr.screenUpdate() & Update::ForceAll) {
			for (Buffer const * b : theBufferList())
				b->changed(true);
			dr.screenUpdate(dr.screenUpdate() & ~Update::ForceAll);
		}

		updateCurrentView(cmd, dr);
	}

	d->dispatch_result_ = dr;
	return d->dispatch_result_;
}


void GuiApplication::updateCurrentView(FuncRequest const & cmd, DispatchResult & dr)
{
	if (!current_view_)
		return;

	BufferView * bv = current_view_->currentBufferView();
	if (bv) {
		if (dr.needBufferUpdate() || bv->buffer().needUpdate()) {
			bv->cursor().clearBufferUpdate();
			bv->buffer().updateBuffer();
		}
		// BufferView::update() updates the ViewMetricsInfo and
		// also initializes the position cache for all insets in
		// (at least partially) visible top-level paragraphs.
		// We will redraw the screen only if needed.
		bv->processUpdateFlags(dr.screenUpdate());

		// Do we have a selection?
		theSelection().haveSelection(bv->cursor().selection());

		// update gui
		current_view_->restartCaret();
	}
	if (dr.needMessageUpdate()) {
		// Some messages may already be translated, so we cannot use _()
		current_view_->message(makeDispatchMessage(
				translateIfPossible(dr.message()), cmd));
	}
}


void GuiApplication::gotoBookmark(unsigned int idx, bool openFile,
	bool switchToBuffer)
{
	if (!theSession().bookmarks().isValid(idx))
		return;
	BookmarksSection::Bookmark const & bm =
		theSession().bookmarks().bookmark(idx);
	LASSERT(!bm.filename.empty(), return);
	string const file = bm.filename.absFileName();
	// if the file is not opened, open it.
	if (!theBufferList().exists(bm.filename)) {
		if (openFile)
			dispatch(FuncRequest(LFUN_FILE_OPEN, file));
		else
			return;
	}
	// open may fail, so we need to test it again
	if (!theBufferList().exists(bm.filename))
		return;

	// bm can be changed when saving
	BookmarksSection::Bookmark tmp = bm;

	// Special case idx == 0 used for back-from-back jump navigation
	if (idx == 0)
		dispatch(FuncRequest(LFUN_BOOKMARK_SAVE, "0"));

	// if the current buffer is not that one, switch to it.
	BufferView * doc_bv = current_view_ ?
		current_view_->documentBufferView() : nullptr;
	Cursor const * old = doc_bv ? &doc_bv->cursor() : nullptr;
	if (!doc_bv || doc_bv->buffer().fileName() != tmp.filename) {
		if (switchToBuffer) {
			dispatch(FuncRequest(LFUN_BUFFER_SWITCH, file));
			if (!current_view_)
				return;
			doc_bv = current_view_->documentBufferView();
		} else
			return;
	}

	// moveToPosition try paragraph id first and then paragraph (pit, pos).
	if (!doc_bv || !doc_bv->moveToPosition(
			tmp.bottom_pit, tmp.bottom_pos, tmp.top_id, tmp.top_pos))
		return;

	Cursor & cur = doc_bv->cursor();
	if (old && cur != *old)
		notifyCursorLeavesOrEnters(*old, cur);

	// bm changed
	if (idx == 0)
		return;

	// Cursor jump succeeded!
	pit_type new_pit = cur.pit();
	pos_type new_pos = cur.pos();
	int new_id = cur.paragraph().id();

	// if bottom_pit, bottom_pos or top_id has been changed, update bookmark
	// see http://www.lyx.org/trac/ticket/3092
	if (bm.bottom_pit != new_pit || bm.bottom_pos != new_pos
		|| bm.top_id != new_id) {
		const_cast<BookmarksSection::Bookmark &>(bm).updatePos(
			new_pit, new_pos, new_id);
	}
}

// This function runs "configure" and then rereads lyx.defaults to
// reconfigure the automatic settings.
void GuiApplication::reconfigure(string const & option)
{
	// emit message signal.
	if (current_view_) {
		current_view_->message(_("Running configure..."));
		current_view_->setCursor(Qt::WaitCursor);
	}

	// Run configure in user lyx directory
	string const lock_file = package().getConfigureLockName();
	int fd = fileLock(lock_file.c_str());
	int const ret = package().reconfigureUserLyXDir(option);
	// emit message signal.
	if (current_view_)
		current_view_->message(_("Reloading configuration..."));
	lyxrc.read(libFileSearch(QString(), "lyxrc.defaults"), false);
	// Re-read packages.lst
	LaTeXPackages::getAvailable();
	fileUnlock(fd, lock_file.c_str());

	if (current_view_)
		current_view_->unsetCursor();

	if (ret)
		Alert::information(_("System reconfiguration failed"),
			   _("The system reconfiguration has failed.\n"
				  "Default textclass is used but LyX may\n"
				  "not be able to work properly.\n"
				  "Please reconfigure again if needed."));
	else
		Alert::information(_("System reconfigured"),
			   _("The system has been reconfigured.\n"
			     "You need to restart LyX to make use of any\n"
			     "updated document class specifications."));
}

void GuiApplication::validateCurrentView()
{
	if (!d->views_.empty() && !current_view_) {
		// currently at least one view exists but no view has the focus.
		// choose the last view to make it current.
		// a view without any open document is preferred.
		GuiView * candidate = nullptr;
		QHash<int, GuiView *>::const_iterator it = d->views_.begin();
		QHash<int, GuiView *>::const_iterator end = d->views_.end();
		for (; it != end; ++it) {
			candidate = *it;
			if (!candidate->documentBufferView())
				break;
		}
		setCurrentView(candidate);
	}
}

void GuiApplication::dispatch(FuncRequest const & cmd, DispatchResult & dr)
{
	LYXERR(Debug::ACTION, "cmd: " << cmd);

	// we have not done anything wrong yet.
	dr.setError(false);

	FuncStatus const flag = getStatus(cmd);
	if (!flag.enabled()) {
		// We cannot use this function here
		LYXERR(Debug::ACTION, "action "
		       << lyxaction.getActionName(cmd.action())
		       << " [" << cmd.action() << "] is disabled at this location");
		dr.setMessage(flag.message());
		dr.setError(true);
		dr.dispatched(false);
		dr.screenUpdate(Update::None);
		dr.clearBufferUpdate();
		return;
	};

	if (cmd.origin() == FuncRequest::LYXSERVER) {
		if (current_view_ && current_view_->currentBufferView())
			current_view_->currentBufferView()->cursor().saveBeforeDispatchPosXY();
		// we will also need to redraw the screen at the end
		dr.screenUpdate(Update::FitCursor);
	}

	// Assumes that the action will be dispatched.
	dr.dispatched(true);

	switch (cmd.action()) {

	case LFUN_WINDOW_NEW:
		createAndShowView();
		break;

	case LFUN_WINDOW_CLOSE:
		// FIXME: this is done also in GuiView::closeBuffer()!
		// update bookmark pit of the current buffer before window close
		for (size_t i = 1; i < theSession().bookmarks().size(); ++i)
			gotoBookmark(i, false, false);
		// clear the last opened list, because
		// maybe this will end the session
		theSession().lastOpened().clear();
		// check for valid current_view_
		validateCurrentView();
		if (current_view_)
			current_view_->closeScheduled();
		break;

	case LFUN_LYX_QUIT:
		// quitting is triggered by the gui code
		// (leaving the event loop).
		if (current_view_)
			current_view_->message(from_utf8(N_("Exiting.")));
		if (closeAllViews())
			quit();
		break;

	case LFUN_SCREEN_FONT_UPDATE: {
		// handle the screen font changes.
		/* FIXME: this only updates the current document, whereas all
		 * documents should see their metrics updated.
		 */
		d->font_loader_.update();
		dr.screenUpdate(Update::Force | Update::FitCursor);
		break;
	}

	case LFUN_BUFFER_NEW:
		validateCurrentView();
		if (!current_view_
		   || (!lyxrc.open_buffers_in_tabs && current_view_->documentBufferView() != nullptr)) {
			createView(); // keep hidden
			current_view_->newDocument(to_utf8(cmd.argument()));
			current_view_->show();
			current_view_->activateWindow();
		} else {
			current_view_->newDocument(to_utf8(cmd.argument()));
		}
		break;

	case LFUN_BUFFER_NEW_TEMPLATE: {
		string const file = (cmd.getArg(0) == "newfile") ? string() : cmd.getArg(0);
		validateCurrentView();
		if (!current_view_
		   || (!lyxrc.open_buffers_in_tabs && current_view_->documentBufferView() != nullptr)) {
			createAndShowView();
			current_view_->newDocument(file, cmd.getArg(1), true);
			if (!current_view_->documentBufferView())
				current_view_->close();
		} else {
			current_view_->newDocument(file, cmd.getArg(1), true);
		}
		break;
	}

	case LFUN_FILE_OPEN: {
		// FIXME: normally the code below is not needed, since getStatus makes sure that
		//   current_view_ is not null.
		validateCurrentView();
		// FIXME: create a new method shared with LFUN_HELP_OPEN.
		string const fname = trim(to_utf8(cmd.argument()), "\"");
		bool const is_open = FileName::isAbsolute(fname)
			&& theBufferList().getBuffer(FileName(fname));
		if (!current_view_
		    || (!lyxrc.open_buffers_in_tabs
			&& current_view_->documentBufferView() != nullptr
			&& !is_open)) {
			// We want the ui session to be saved per document and not per
			// window number. The filename crc is a good enough identifier.
			createAndShowView(support::checksum(fname));
			current_view_->openDocuments(fname, cmd.origin());
			if (!current_view_->documentBufferView())
				current_view_->close();
			else if (cmd.origin() == FuncRequest::LYXSERVER) {
				current_view_->raise();
				current_view_->activateWindow();
				current_view_->showNormal();
			}
		} else {
			current_view_->openDocuments(fname, cmd.origin());
			if (cmd.origin() == FuncRequest::LYXSERVER) {
				current_view_->raise();
				current_view_->activateWindow();
				current_view_->showNormal();
			}
		}
		break;
	}

	case LFUN_HELP_OPEN: {
		// FIXME: create a new method shared with LFUN_FILE_OPEN.
		if (current_view_ == nullptr)
			createAndShowView();
		string const arg = to_utf8(cmd.argument());
		if (arg.empty()) {
			current_view_->message(_("Missing argument"));
			break;
		}
		FileName fname = i18nLibFileSearch("doc", arg, "lyx");
		if (fname.empty())
			fname = i18nLibFileSearch("examples", arg, "lyx");

		if (fname.empty()) {
			lyxerr << "LyX: unable to find documentation file `"
			       << arg << "'. Bad installation?" << endl;
			break;
		}
		current_view_->message(bformat(_("Opening help file %1$s..."),
					       makeDisplayPath(fname.absFileName())));
		Buffer * buf = current_view_->loadDocument(fname, false);
		if (buf)
			buf->setReadonly(!current_view_->develMode());
		break;
	}

	case LFUN_LYXFILES_OPEN: {
		string arg = to_utf8(cmd.argument());
		if (arg.empty())
			// set default
			arg = "templates";
		if (arg != "templates" && arg != "examples") {
			if (current_view_)
				current_view_->message(_("Wrong argument. Must be 'examples' or 'templates'."));
			break;
		}
		lyx::dispatch(FuncRequest(LFUN_DIALOG_SHOW, "lyxfiles " + arg));
		break;
	}

	case LFUN_SET_COLOR: {
		string const lyx_name = cmd.getArg(0);
		string x11_name = cmd.getArg(1);
		string x11_darkname = cmd.getArg(2);
		if (lyx_name.empty() || x11_name.empty()) {
			if (current_view_)
				current_view_->message(
					_("Syntax: set-color <lyx_name> <x11_name> <x11_darkname>"));
			break;
		}

#if 0
		// FIXME: The graphics cache no longer has a changeDisplay method.
		string const graphicsbg = lcolor.getLyXName(Color_graphicsbg);
		bool const graphicsbg_changed =
				lyx_name == graphicsbg && x11_name != graphicsbg;
		if (graphicsbg_changed)
			graphics::GCache::get().changeDisplay(true);
#endif

		if (x11_darkname.empty() && colorCache().isDarkMode()) {
			x11_darkname = x11_name;
			x11_name.clear();
		}
		if (!lcolor.setColor(lyx_name, x11_name, x11_darkname)) {
			if (current_view_)
				current_view_->message(
					bformat(_("Set-color \"%1$s\" failed "
				        "- color is undefined or "
				        "may not be redefined"),
				        from_utf8(lyx_name)));
			break;
		}
		// Make sure we don't keep old colors in cache.
		d->color_cache_.clear();
		// Update the current view
		lyx::dispatch(FuncRequest(LFUN_SCREEN_FONT_UPDATE));
		break;
	}

	case LFUN_LYXRC_APPLY: {
		// reset active key sequences, since the bindings
		// are updated (bug 6064)
		d->keyseq.reset();
		LyXRC const lyxrc_orig = lyxrc;

		istringstream ss(to_utf8(cmd.argument()));
		bool const success = lyxrc.read(ss);

		if (!success) {
			lyxerr << "Warning in LFUN_LYXRC_APPLY!\n"
					<< "Unable to read lyxrc data"
					<< endl;
			break;
		}

		actOnUpdatedPrefs(lyxrc_orig, lyxrc);

		// If the request comes from the minibuffer, then we can't reset
		// the GUI, since that would destroy the minibuffer itself and
		// cause a crash, since we are currently in one of the methods of
		// GuiCommandBuffer. See bug #8540.
		if (cmd.origin() != FuncRequest::COMMANDBUFFER)
			resetGui();
		// else
		//   FIXME Unfortunately, that leaves a bug here, since we cannot
		//   reset the GUI in this case. If the changes to lyxrc affected the
		//   UI, then, nothing would happen. This seems fairly unlikely, but
		//   it definitely is a bug.

		dr.forceBufferUpdate();
		break;
	}

	case LFUN_COMMAND_PREFIX:
		dispatch(FuncRequest(LFUN_MESSAGE, d->keyseq.printOptions(true)));
		break;

	case LFUN_CANCEL: {
		d->keyseq.reset();
		d->meta_fake_bit = NoModifier;
		GuiView * gv = currentView();
		if (gv && gv->currentBufferView())
			// cancel any selection
			processFuncRequest(FuncRequest(LFUN_MARK_OFF));
		dr.setMessage(from_ascii(N_("Cancel")));
		break;
	}
	case LFUN_META_PREFIX:
		d->meta_fake_bit = AltModifier;
		dr.setMessage(d->keyseq.print(KeySequence::ForGui));
		break;

	// --- Menus -----------------------------------------------
	case LFUN_RECONFIGURE:
		// argument is any additional parameter to the configure.py command
		reconfigure(to_utf8(cmd.argument()));
		break;

	// --- lyxserver commands ----------------------------
	case LFUN_SERVER_GET_FILENAME: {
		if (current_view_ && current_view_->documentBufferView()) {
			docstring const fname = from_utf8(
				current_view_->documentBufferView()->buffer().absFileName());
			dr.setMessage(fname);
			LYXERR(Debug::INFO, "FNAME[" << fname << ']');
		} else {
			dr.setMessage(docstring());
			LYXERR(Debug::INFO, "No current file for LFUN_SERVER_GET_FILENAME");
		}
		break;
	}

	case LFUN_SERVER_NOTIFY: {
		docstring const dispatch_buffer = d->keyseq.print(KeySequence::Portable);
		dr.setMessage(dispatch_buffer);
		theServer().notifyClient(to_utf8(dispatch_buffer));
		break;
	}

	case LFUN_CURSOR_FOLLOWS_SCROLLBAR_TOGGLE:
		lyxrc.cursor_follows_scrollbar = !lyxrc.cursor_follows_scrollbar;
		break;

	case LFUN_REPEAT: {
		// repeat command
		string countstr;
		string argument = to_utf8(cmd.argument());
		string rest = split(argument, countstr, ' ');
		int const count = convert<int>(countstr);
		// an arbitrary number to limit number of iterations
		int const max_iter = 10000;
		if (count > max_iter) {
			dr.setMessage(bformat(_("Cannot iterate more than %1$d times"), max_iter));
			dr.setError(true);
		} else {
			for (int i = 0; i < count; ++i) {
				FuncRequest lfun = lyxaction.lookupFunc(rest);
				lfun.allowAsync(false);
				dispatch(lfun);
			}
		}
		break;
	}

	case LFUN_COMMAND_SEQUENCE: {
		// argument contains ';'-terminated commands
		string arg = to_utf8(cmd.argument());
		// FIXME: this LFUN should also work without any view.
		Buffer * buffer = (current_view_ && current_view_->documentBufferView())
				  ? &(current_view_->documentBufferView()->buffer()) : nullptr;
		// This handles undo groups automagically
		UndoGroupHelper ugh(buffer);
		while (!arg.empty()) {
			string first;
			arg = split(arg, first, ';');
			FuncRequest func(lyxaction.lookupFunc(first));
			func.allowAsync(false);
			func.setOrigin(cmd.origin());
			dispatch(func);
		}
		break;
	}

	case LFUN_BUFFER_FORALL: {
		FuncRequest funcToRun = lyxaction.lookupFunc(cmd.getLongArg(0));
		funcToRun.allowAsync(false);

		map<Buffer *, GuiView *> views_lVisible;
		map<GuiView *, Buffer *> activeBuffers;

		QList<GuiView *> allViews = d->views_.values();

		// this for does not modify any buffer. It just collects info on local
		// visibility of buffers and on which buffer is active in each view.
		Buffer * const last = theBufferList().last();
		for(GuiView * view : allViews) {
			// all of the buffers might be locally hidden. That is, there is no
			// active buffer.
			if (!view || !view->currentBufferView())
				activeBuffers[view] = nullptr;
			else
				activeBuffers[view] = &view->currentBufferView()->buffer();

			// find out if each is locally visible or locally hidden.
			// we don't use a for loop as the buffer list cycles.
			Buffer * b = theBufferList().first();
			while (true) {
				bool const locallyVisible = view && view->workArea(*b);
				if (locallyVisible) {
					bool const exists_ = (views_lVisible.find(b) != views_lVisible.end());
					// only need to overwrite/add if we don't already know a buffer is globally
					// visible or we do know but we would prefer to dispatch LFUN from the
					// current view because of cursor position issues.
					if (!exists_ || (exists_ && views_lVisible[b] != current_view_))
						views_lVisible[b] = view;
				}
				if (b == last)
					break;
				b = theBufferList().next(b);
			}
		}

		GuiView * const homeView = currentView();
		Buffer * b = theBufferList().first();
		Buffer * nextBuf = nullptr;
		int numProcessed = 0;
		while (true) {
			if (b != last)
				nextBuf = theBufferList().next(b); // get next now bc LFUN might close current.

			bool const visible = (views_lVisible.find(b) != views_lVisible.end());
			if (visible) {
				// first change to a view where b is locally visible, preferably current_view_.
				GuiView * const vLv = views_lVisible[b];
				vLv->setBuffer(b);
				lyx::dispatch(funcToRun);
				numProcessed++;
			}
			if (b == last)
				break;
			b = nextBuf;
		}

		// put things back to how they were (if possible).
		for (GuiView * view : allViews) {
			Buffer * originalBuf = activeBuffers[view];
			// there might not have been an active buffer in this view or it might have been closed by the LFUN.
			if (theBufferList().isLoaded(originalBuf))
				view->setBuffer(originalBuf);
		}
		homeView->setFocus();

		dr.setMessage(bformat(_("Applied \"%1$s\" to %2$d buffer(s)"), from_utf8(cmd.getLongArg(0)), numProcessed));
		break;
	}

	case LFUN_COMMAND_ALTERNATIVES: {
		// argument contains ';'-terminated commands
		string arg = to_utf8(cmd.argument());
		while (!arg.empty()) {
			string first;
			arg = split(arg, first, ';');
			FuncRequest func(lyxaction.lookupFunc(first));
			func.setOrigin(cmd.origin());
			FuncStatus const stat = getStatus(func);
			if (stat.enabled()) {
				dispatch(func);
				break;
			}
		}
		break;
	}

	case LFUN_CALL: {
		FuncRequest func;
		string const argument = to_utf8(cmd.argument());
		if (theTopLevelCmdDef().lock(argument, func)) {
			func.setOrigin(cmd.origin());
			dispatch(func);
			theTopLevelCmdDef().release(argument);
		} else {
			if (func.action() == LFUN_UNKNOWN_ACTION) {
				// unknown command definition
				lyxerr << "Warning: unknown command definition `"
						<< argument << "'"
						<< endl;
			} else {
				// recursion detected
				lyxerr << "Warning: Recursion in the command definition `"
						<< argument << "' detected"
						<< endl;
			}
		}
		break;
	}

	case LFUN_IF_RELATIVES: {
		string const lfun = to_utf8(cmd.argument());
		FuncRequest func(lyxaction.lookupFunc(lfun));
		func.setOrigin(cmd.origin());
		FuncStatus const stat = getStatus(func);
		if (stat.enabled()) {
			dispatch(func);
			break;
		}
		break;
	}

	case LFUN_PREFERENCES_SAVE:
		lyxrc.write(support::makeAbsPath("preferences",
			package().user_support().absFileName()), false);
		break;

	case LFUN_BUFFER_SAVE_AS_DEFAULT: {
		string const fname = addName(addPath(package().user_support().absFileName(),
			"templates/"), "defaults.lyx");
		Buffer defaults(fname);

		istringstream ss(to_utf8(cmd.argument()));
		Lexer lex;
		lex.setStream(ss);

		// See #9236
		// We need to make sure that, after we recreat the DocumentClass,
		// which we do in readHeader, we apply it to the document itself.
		DocumentClassConstPtr olddc = defaults.params().documentClassPtr();
		int const unknown_tokens = defaults.readHeader(lex);
		DocumentClassConstPtr newdc = defaults.params().documentClassPtr();
		InsetText & theinset = static_cast<InsetText &>(defaults.inset());
		cap::switchBetweenClasses(olddc, newdc, theinset);

		if (unknown_tokens != 0) {
			lyxerr << "Warning in LFUN_BUFFER_SAVE_AS_DEFAULT!\n"
			       << unknown_tokens << " unknown token"
			       << (unknown_tokens == 1 ? "" : "s")
			       << endl;
		}

		if (defaults.writeFile(FileName(defaults.absFileName())))
			dr.setMessage(bformat(_("Document defaults saved in %1$s"),
					      makeDisplayPath(fname)));
		else {
			dr.setError(true);
			dr.setMessage(from_ascii(N_("Unable to save document defaults")));
		}
		break;
	}

	case LFUN_BOOKMARK_GOTO:
		// go to bookmark, open unopened file and switch to buffer if necessary
		gotoBookmark(convert<unsigned int>(to_utf8(cmd.argument())), true, true);
		dr.screenUpdate(Update::Force | Update::FitCursor);
		break;

	case LFUN_BOOKMARK_CLEAR:
		theSession().bookmarks().clear();
		dr.screenUpdate(Update::Force);
		break;

	case LFUN_DEBUG_LEVEL_SET:
		lyxerr.setLevel(Debug::value(to_utf8(cmd.argument())));
		break;

	case LFUN_DIALOG_SHOW: {
		string const name = cmd.getArg(0);
		// Workaround: on Mac OS the application
		// is not terminated when closing the last view.
		// With the following dialogs which should still
		// be usable, create a new one to be able
		// to dispatch LFUN_DIALOG_SHOW to this view.
		if (name == "aboutlyx" || name == "compare"
		    || name == "lyxfiles" || name == "prefs"
		    || name == "progress" || name == "texinfo")
		{
			if (current_view_ == nullptr)
				createAndShowView();
		}
	}
	// fall through
	default:
		// The LFUN must be for one of GuiView, BufferView, Buffer or Cursor;
		// let's try that:
		if (current_view_)
			current_view_->dispatch(cmd, dr);
		break;
	}

	if (current_view_ && current_view_->isFullScreen()) {
		if (current_view_->menuBar()->isVisible() && lyxrc.full_screen_menubar)
			current_view_->menuBar()->hide();
	}

	if (cmd.origin() == FuncRequest::LYXSERVER)
		updateCurrentView(cmd, dr);
}


docstring GuiApplication::viewStatusMessage()
{
	// When meta-fake key is pressed, show the key sequence so far + "M-".
	if (d->meta_fake_bit != NoModifier)
		return d->keyseq.print(KeySequence::ForGui) + "M-";

	// Else, when a non-complete key sequence is pressed,
	// show the available options.
	if (d->keyseq.length() > 0 && !d->keyseq.deleted())
		return d->keyseq.printOptions(true);

	return docstring();
}


bool GuiApplication::isFirstWorkArea() const
{
	return d->first_work_area;
}


void GuiApplication::firstWorkAreaDone()
{
	d->first_work_area = false;
}


QRectF GuiApplication::baseInputItemRectangle()
{
	return d->item_rect_base_;
}


void GuiApplication::setBaseInputItemRectangle(QRectF const & rect)
{
	d->item_rect_base_ = rect;
}


QTransform GuiApplication::baseInputItemTransform()
{
	return d->item_trans_base_;
}


void GuiApplication::setBaseInputItemTransform(QTransform const & trans)
{
	d->item_trans_base_ = trans;
}


string GuiApplication::inputLanguageCode() const
{
	QLocale loc = inputMethod()->locale();
	//LYXERR0("input lang = " << fromqstr(loc.name()));
	return loc.name() == "C" ? "en_US" : fromqstr(loc.name());
}


void GuiApplication::onLocaleChanged()
{
	//LYXERR0("Change language to " << inputLanguageCode());
	if (currentView() && currentView()->currentBufferView())
		currentView()->currentBufferView()->cursor().setLanguageFromInput();
}


void GuiApplication::onPaletteChanged()
{
	colorCache().setPalette(palette());
}


void GuiApplication::handleKeyFunc(FuncCode action)
{
	char_type c = 0;

	if (d->keyseq.length())
		c = 0;
	GuiView * gv = currentView();
	LASSERT(gv && gv->currentBufferView(), return);
	BufferView * bv = gv->currentBufferView();
	bv->getIntl().getTransManager().deadkey(
		c, get_accent(action).accent, bv->cursor().innerText(),
		bv->cursor());
	// Need to clear, in case the minibuffer calls these
	// actions
	d->keyseq.clear();
	// copied verbatim from do_accent_char
	bv->cursor().resetAnchor();
}


//Keep this in sync with GuiApplication::processKeySym below
bool GuiApplication::queryKeySym(KeySymbol const & keysym,
                                 KeyModifier state) const
{
	// Do nothing if we have nothing
	if (!keysym.isOK() || keysym.isModifier())
		return false;
	// Do a one-deep top-level lookup for cancel and meta-fake keys.
	KeySequence seq;
	FuncRequest func = seq.addkey(keysym, state);
	// When not cancel or meta-fake, do the normal lookup.
	if ((func.action() != LFUN_CANCEL) && (func.action() != LFUN_META_PREFIX)) {
		seq = d->keyseq;
		func = seq.addkey(keysym, (state | d->meta_fake_bit));
	}
	// Maybe user can only reach the key via holding down shift.
	// Let's see. But only if shift is the only modifier
	if (func.action() == LFUN_UNKNOWN_ACTION && state == ShiftModifier)
		// If addkey looked up a command and did not find further commands then
		// seq has been reset at this point
		func = seq.addkey(keysym, NoModifier);

	LYXERR(Debug::KEY, " Key (queried) [action="
	       << lyxaction.getActionName(func.action()) << "]["
	       << seq.print(KeySequence::Portable) << ']');
	return func.action() != LFUN_UNKNOWN_ACTION;
}


//Keep this in sync with GuiApplication::queryKeySym above
void GuiApplication::processKeySym(KeySymbol const & keysym, KeyModifier state)
{
	LYXERR(Debug::KEY, "KeySym is " << keysym.getSymbolName());

	// Do nothing if we have nothing (JMarc)
	if (!keysym.isOK() || keysym.isModifier()) {
		if (!keysym.isOK())
			LYXERR(Debug::KEY, "Empty kbd action (probably composing)");
		if (current_view_)
			current_view_->restartCaret();
		return;
	}

	char_type encoded_last_key = keysym.getUCSEncoded();

	// Do a one-deep top-level lookup for
	// cancel and meta-fake keys. RVDK_PATCH_5
	d->cancel_meta_seq.reset();

	FuncRequest func = d->cancel_meta_seq.addkey(keysym, state);
	LYXERR(Debug::KEY, "action first set to ["
	       << lyxaction.getActionName(func.action()) << ']');

	// When not cancel or meta-fake, do the normal lookup.
	// Note how the meta_fake Mod1 bit is OR-ed in and reset afterwards.
	// Mostly, meta_fake_bit = NoModifier. RVDK_PATCH_5.
	if ((func.action() != LFUN_CANCEL) && (func.action() != LFUN_META_PREFIX)) {
		// remove Caps Lock and Mod2 as a modifiers
		func = d->keyseq.addkey(keysym, (state | d->meta_fake_bit));
		LYXERR(Debug::KEY, "action now set to ["
		       << lyxaction.getActionName(func.action()) << ']');
	}

	// Don't remove this unless you know what you are doing.
	d->meta_fake_bit = NoModifier;

	// Can this happen now ?
	if (func.action() == LFUN_NOACTION)
		func = FuncRequest(LFUN_COMMAND_PREFIX);

	LYXERR(Debug::KEY, " Key [action="
	       << lyxaction.getActionName(func.action()) << "]["
	       << d->keyseq.print(KeySequence::Portable) << ']');

	// The preedit mode should avoid intervening multi-stroke commands
	command_phase_ = func.action() == LFUN_COMMAND_PREFIX;

	Q_EMIT acceptsInputMethod();

	// already here we know if it any point in going further
	// why not return already here if action == -1 and
	// num_bytes == 0? (Lgb)

	if (d->keyseq.length() > 1 && current_view_)
		current_view_->message(d->keyseq.print(KeySequence::ForGui));


	// Maybe user can only reach the key via holding down shift.
	// Let's see. But only if shift is the only modifier
	if (func.action() == LFUN_UNKNOWN_ACTION && state == ShiftModifier) {
		LYXERR(Debug::KEY, "Trying without shift");
		// If addkey looked up a command and did not find further commands then
		// seq has been reset at this point
		func = d->keyseq.addkey(keysym, NoModifier);
		LYXERR(Debug::KEY, "Action now " << lyxaction.getActionName(func.action()));
	}

	if (func.action() == LFUN_UNKNOWN_ACTION) {
		// We didn't match any of the key sequences.
		// See if it's normal insertable text not already
		// covered by a binding
		if (keysym.isText() && d->keyseq.length() == 1) {
			// Non-printable characters (such as ASCII control characters)
			// must not be inserted (#5704)
			if (!isPrintable(encoded_last_key)) {
				LYXERR(Debug::KEY, "Non-printable character! Omitting.");
				if (current_view_)
					current_view_->restartCaret();
				return;
			}
			// The following modifier check is not needed on Mac.
			// The keysym is either not text or it is different
			// from the non-modifier keysym. See #9875 for the
			// broken alt-modifier effect of having this code active.
#if !defined(Q_OS_MAC)
			// If a non-Shift Modifier is used we have a non-bound key sequence
			// (such as Alt+j = j). This should be omitted (#5575).
			// On Windows, AltModifier and ControlModifier are both
			// set when AltGr is pressed. Therefore, in order to not
			// break AltGr-bound symbols (see #5575 for details),
			// unbound Ctrl+Alt key sequences are allowed.
			if ((state & AltModifier || state & ControlModifier || state & MetaModifier)
#if defined(Q_OS_WIN) || defined(Q_CYGWIN_WIN)
			    && !(state & AltModifier && state & ControlModifier)
#endif
			    )
			{
				if (current_view_) {
					current_view_->message(_("Unknown function."));
					current_view_->restartCaret();
				}
				return;
			}
#endif
			// Since all checks above were passed, we now really have text that
			// is to be inserted (e.g., AltGr-bound symbols). Thus change the
			// func to LFUN_SELF_INSERT and thus cause the text to be inserted
			// below.
			LYXERR(Debug::KEY, "isText() is true, inserting.");
			func = FuncRequest(LFUN_SELF_INSERT, FuncRequest::KEYBOARD);
		} else {
			LYXERR(Debug::KEY, "Unknown Action and not isText() -- giving up");
			if (current_view_) {
				current_view_->message(_("Unknown function."));
				current_view_->restartCaret();
			}
			return;
		}
	}

	if (func.action() == LFUN_SELF_INSERT) {
		if (encoded_last_key != 0) {
			docstring const arg(1, encoded_last_key);
			processFuncRequest(FuncRequest(LFUN_SELF_INSERT, arg,
					     FuncRequest::KEYBOARD));
			LYXERR(Debug::KEY, "SelfInsert arg[`" << to_utf8(arg) << "']");
		}
	} else
		processFuncRequest(func);
}


void GuiApplication::processFuncRequest(FuncRequest const & func)
{
	lyx::dispatch(func);
}


void GuiApplication::processFuncRequestAsync(FuncRequest const & func)
{
	addToFuncRequestQueue(func);
	processFuncRequestQueueAsync();
}


void GuiApplication::processFuncRequestQueue()
{
	while (!d->func_request_queue_.empty()) {
		// take the item from the stack _before_ processing the
		// request in order to avoid race conditions from nested
		// or parallel requests (see #10406)
		FuncRequest const fr(d->func_request_queue_.front());
		d->func_request_queue_.pop();
		processFuncRequest(fr);
	}
}


void GuiApplication::processFuncRequestQueueAsync()
{
	QTimer::singleShot(0, this, SLOT(slotProcessFuncRequestQueue()));
}


void GuiApplication::addToFuncRequestQueue(FuncRequest const & func)
{
	d->func_request_queue_.push(func);
}


void GuiApplication::resetGui()
{
	// Set the language defined by the user.
	setGuiLanguage();

	// Read menus
	if (!readUIFile(toqstr(lyxrc.ui_file)))
		// Gives some error box here.
		return;

	if (d->global_menubar_)
		d->menus_.fillMenuBar(d->global_menubar_, nullptr, false);

	QHash<int, GuiView *>::iterator it;
	for (it = d->views_.begin(); it != d->views_.end(); ++it) {
		GuiView * gv = *it;
		setCurrentView(gv);
		gv->setLayoutDirection(layoutDirection());
		gv->resetDialogs();
	}

	processFuncRequest(FuncRequest(LFUN_SCREEN_FONT_UPDATE));
}


bool GuiApplication::rtlContext() const
{
	if (current_view_ && current_view_->currentBufferView()) {
		BufferView const * bv = current_view_->currentBufferView();
		return bv->cursor().innerParagraph().isRTL(bv->buffer().params());
	} else
		return layoutDirection() == Qt::RightToLeft;
}


void GuiApplication::createAndShowView(int view_id)
{
	createView(true, view_id);
}


void GuiApplication::createView(bool autoShow, int view_id)
{
	// release the keyboard which might have been grabbed by the global
	// menubar on Mac to catch shortcuts even without any GuiView.
	if (d->global_menubar_)
		d->global_menubar_->releaseKeyboard();

	// need to reset system input method coords with the preserved one
	// when the new view is the second one or later and a document is
	// open in the current view
	if (d->views_.size() > 0 && current_view_->currentWorkArea())
		current_view_->currentWorkArea()->resetInputItemGeometry(true);

	// create new view
	int id = view_id;
	while (d->views_.find(id) != d->views_.end())
		id++;

	LYXERR(Debug::GUI, "About to create new window with ID " << id);
	GuiView * view = new GuiView(id);
	// `view' is the new current_view_. Tell coverity that is is not 0.
	LATTEST(current_view_);
	// register view
	d->views_[id] = view;

	if (autoShow) {
		view->show();
		view->activateWindow();
	}

	view->setFocus();
}


bool GuiApplication::unhide(Buffer * buf)
{
	if (!currentView())
		return false;
	currentView()->setBuffer(buf, false);
	return true;
}


QPixmap GuiApplication::getScaledPixmap(QString imagedir, QString name) const
{
	qreal dpr = 1.0;
	// Consider device/pixel ratio (HiDPI)
	if (currentView())
		dpr = currentView()->pixelRatio();
	// We render SVG directly for HiDPI scalability
	QPixmap pm = getPixmap(imagedir, name, "svgz,png");
	search_mode const mode = theGuiApp() ? theGuiApp()->imageSearchMode() : support::must_exist;
	bool const dark_mode = theGuiApp() ? theGuiApp()->isInDarkMode() : false;
	FileName fname = imageLibFileSearch(imagedir, name, "svgz,png", mode, dark_mode);
	QString fpath = toqstr(fname.absFileName());
	if (!fpath.isEmpty() && !fpath.endsWith(".png")) {
		QSvgRenderer svgRenderer(fpath);
		if (svgRenderer.isValid()) {
			pm = QPixmap(pm.size() * dpr);
			pm.fill(Qt::transparent);
			QPainter painter(&pm);
			svgRenderer.render(&painter);
			painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
			QColor const penColor = dark_mode ? Qt::lightGray : Qt::darkGray;
			painter.fillRect(pm.rect(), penColor);
			painter.end();
			pm.setDevicePixelRatio(dpr);
		}
	}
	return pm;
}


Clipboard & GuiApplication::clipboard()
{
	return d->clipboard_;
}


Selection & GuiApplication::selection()
{
	return d->selection_;
}


FontLoader & GuiApplication::fontLoader()
{
	return d->font_loader_;
}


Toolbars const & GuiApplication::toolbars() const
{
	return d->toolbars_;
}


Toolbars & GuiApplication::toolbars()
{
	return d->toolbars_;
}


Menus const & GuiApplication::menus() const
{
	return d->menus_;
}


Menus & GuiApplication::menus()
{
	return d->menus_;
}


DrawStrategy GuiApplication::drawStrategy() const
{
	/* Qt on macOS and Wayland does not respect the
	 * Qt::WA_OpaquePaintEvent attribute and resets the widget backing
	 * store at each update. Therefore, if it not good to use
	 * "partial" draw strategy in these cases. It can also be useful
	 * to force the use of the backing store for cases like X11 with
	 * transparent WM themes.
	 */
	if (lyxrc.draw_strategy == DrawStrategy::Partial
	     && (platformName() == "cocoa" || platformName().contains("wayland")))
		return DrawStrategy::Backingstore;
	else
		return lyxrc.draw_strategy;
}


docstring GuiApplication::drawStrategyDescription() const
{
	switch(drawStrategy()) {
	case DrawStrategy::Partial:
		return _("partial draw");
		break;
	case DrawStrategy::Backingstore:
		return _("partial draw on backing store");
		break;
	case DrawStrategy::Full:
		return _("full draw");
	}
	return docstring();
}


QList<int> GuiApplication::viewIds() const
{
	return d->views_.keys();
}


ColorCache & GuiApplication::colorCache()
{
	return d->color_cache_;
}


int GuiApplication::exec()
{
	// asynchronously handle batch commands. This event will be in
	// the event queue in front of other asynchronous events. Hence,
	// we can assume in the latter that the gui is setup already.
	QTimer::singleShot(0, this, SLOT(execBatchCommands()));

	return QApplication::exec();
}


void GuiApplication::exit(int status)
{
	QApplication::exit(status);
}


void GuiApplication::setGuiLanguage()
{
	setLocale();
	QLocale theLocale;
	// install translation file for Qt built-in dialogs
	QString const language_name = QString("qt_") + theLocale.name();
	// language_name can be short (e.g. qt_zh) or long (e.g. qt_zh_CN).
	// Short-named translator can be loaded from a long name, but not the
	// opposite. Therefore, long name should be used without truncation.
	// c.f. http://doc.trolltech.com/4.1/qtranslator.html#load
	if (!d->qt_trans_.load(language_name,
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
			QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
#else
			QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
#endif
		LYXERR(Debug::LOCALE, "Could not find Qt translations for locale "
			<< language_name);
	} else {
		LYXERR(Debug::LOCALE, "Successfully installed Qt translations for locale "
			<< language_name);
	}

	switch (theLocale.language()) {
	case QLocale::Arabic :
	case QLocale::Hebrew :
	case QLocale::Persian :
	case QLocale::Urdu :
		setLayoutDirection(Qt::RightToLeft);
		break;
	default:
		setLayoutDirection(Qt::LeftToRight);
	}
}


void GuiApplication::execBatchCommands()
{
	setGuiLanguage();

	// Read menus
	if (!readUIFile(toqstr(lyxrc.ui_file)))
		// Gives some error box here.
		return;

#ifdef Q_OS_MAC
	setAttribute(Qt::AA_MacDontSwapCtrlAndMeta,lyxrc.mac_dontswap_ctrl_meta);
#  if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	setAttribute(Qt::AA_UseHighDpiPixmaps,true);
#  endif
	// Create the global default menubar which is shown for the dialogs
	// and if no GuiView is visible.
	// This must be done after the session was recovered to know the "last files".
	d->global_menubar_ = new QMenuBar(0);
	d->menus_.fillMenuBar(d->global_menubar_, 0, true);
#endif

	lyx::execBatchCommands();
}


QAbstractItemModel * GuiApplication::languageModel()
{
	if (d->language_model_)
		return d->language_model_;

	QStandardItemModel * lang_model = new QStandardItemModel(this);
	lang_model->insertColumns(0, 3);
	QIcon speller(guiApp ? guiApp->getScaledPixmap("images/", "dialog-show_spellchecker")
			  : getPixmap("images/", "dialog-show_spellchecker", "svgz,png"));
	QIcon saurus(guiApp ? guiApp->getScaledPixmap("images/", "thesaurus-entry")
			  : getPixmap("images/", "thesaurus-entry", "svgz,png"));
	Languages::const_iterator it = lyx::languages.begin();
	Languages::const_iterator end = lyx::languages.end();
	for (; it != end; ++it) {
		int current_row = lang_model->rowCount();
		lang_model->insertRows(current_row, 1);
		QModelIndex pl_item = lang_model->index(current_row, 0);
		QModelIndex sp_item = lang_model->index(current_row, 1);
		QModelIndex th_item = lang_model->index(current_row, 2);
		lang_model->setData(pl_item, qt_(it->second.display()), Qt::DisplayRole);
		lang_model->setData(pl_item, toqstr(it->second.lang()), Qt::UserRole);
		lang_model->setData(sp_item, qt_(it->second.display()), Qt::DisplayRole);
		lang_model->setData(sp_item, toqstr(it->second.lang()), Qt::UserRole);
		if (theSpellChecker() && theSpellChecker()->hasDictionary(&it->second))
			lang_model->setData(sp_item, speller, Qt::DecorationRole);
		lang_model->setData(th_item, qt_(it->second.display()), Qt::DisplayRole);
		lang_model->setData(th_item, toqstr(it->second.lang()), Qt::UserRole);
		if (thesaurus.thesaurusInstalled(from_ascii(it->second.code())))
			lang_model->setData(th_item, saurus, Qt::DecorationRole);
	}
	d->language_model_ = new QSortFilterProxyModel(this);
	d->language_model_->setSourceModel(lang_model);
	d->language_model_->setSortLocaleAware(true);
	return d->language_model_;
}


void GuiApplication::restoreGuiSession()
{
	if (!lyxrc.load_session)
		return;

	Session & session = theSession();
	LastOpenedSection::LastOpened const & lastopened =
		session.lastOpened().getfiles();

	validateCurrentView();

	FileName active_file;
	// do not add to the lastfile list since these files are restored from
	// last session, and should be already there (regular files), or should
	// not be added at all (help files).
	for (auto const & last : lastopened) {
		FileName const & file_name = last.file_name;
		if (!current_view_ || (!lyxrc.open_buffers_in_tabs
			  && current_view_->documentBufferView() != nullptr)) {
			string const & fname = file_name.absFileName();
			createAndShowView(support::checksum(fname));
		}
		current_view_->loadDocument(file_name, false);

		if (last.active)
			active_file = file_name;
	}

	// Restore last active buffer
	Buffer * buffer = theBufferList().getBuffer(active_file);
	if (buffer && current_view_)
		current_view_->setBuffer(buffer);

	// clear this list to save a few bytes of RAM
	session.lastOpened().clear();
}


QString const GuiApplication::romanFontName()
{
	QFont font;
	font.setStyleHint(QFont::Serif);
	font.setFamily("serif");

	return QFontInfo(font).family();
}


QString const GuiApplication::sansFontName()
{
	QFont font;
	font.setStyleHint(QFont::SansSerif);
	font.setFamily("sans");

	return QFontInfo(font).family();
}


QString const GuiApplication::typewriterFontName()
{
	return QFontInfo(typewriterSystemFont()).family();
}


namespace {
	// We cannot use QFont::fixedPitch() because it doesn't
	// return the fact but only if it is requested.
	static bool isFixedPitch(const QFont & font) {
		const QFontInfo fi(font);
		return fi.fixedPitch();
	}
} // namespace


QFont const GuiApplication::typewriterSystemFont()
{
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	if (!isFixedPitch(font)) {
		// try to enforce a real monospaced font
		font.setStyleHint(QFont::Monospace);
		if (!isFixedPitch(font)) {
			font.setStyleHint(QFont::TypeWriter);
			if (!isFixedPitch(font)) font.setFamily("courier");
		}
	}
#ifdef Q_OS_MAC
	// On a Mac the result is too small and it's not practical to
	// rely on Qtconfig utility to change the system settings of Qt.
	font.setPointSize(12);
#endif
	return font;
}


void GuiApplication::handleRegularEvents()
{
	ForkedCallsController::handleCompletedProcesses();
}


bool GuiApplication::event(QEvent * e)
{
	switch(e->type()) {
	case QEvent::FileOpen: {
		// Open a file; this happens only on macOS for now.
		//
		// We do this asynchronously because on startup the batch
		// commands are not executed here yet and the gui is not ready
		// therefore.
		QFileOpenEvent * foe = static_cast<QFileOpenEvent *>(e);
		FuncRequest const fr(LFUN_FILE_OPEN, qstring_to_ucs4(foe->file()));
		processFuncRequestAsync(fr);
		e->accept();
		return true;
	}
	case QEvent::ApplicationPaletteChange: {
		// runtime switch from/to dark mode
		onPaletteChanged();
		return QApplication::event(e);
	}
	default:
		return QApplication::event(e);
	}
}


bool GuiApplication::notify(QObject * receiver, QEvent * event)
{
	try {
		return QApplication::notify(receiver, event);
	}
	catch (ExceptionMessage const & e) {
		switch(e.type_) {
		case ErrorException:
			emergencyCleanup();
			setQuitOnLastWindowClosed(false);
			closeAllViews();
			Alert::error(e.title_, e.details_);
#ifndef NDEBUG
			// Properly crash in debug mode in order to get a useful backtrace.
			abort();
#endif
			// In release mode, try to exit gracefully.
			this->exit(1);
			// FIXME: GCC 7 thinks we can fall through here. Can we?
			// fall through
		case BufferException: {
			if (!current_view_ || !current_view_->documentBufferView())
				return false;
			Buffer * buf = &current_view_->documentBufferView()->buffer();
			docstring details = e.details_ + '\n';
			details += buf->emergencyWrite();
			theBufferList().release(buf);
			details += "\n" + _("The current document was closed.");
			Alert::error(e.title_, details);
			return false;
		}
		case WarningException:
			Alert::warning(e.title_, e.details_);
			return false;
		}
	}
	catch (exception const & e) {
		docstring s = _("LyX has caught an exception, it will now "
			"attempt to save all unsaved documents and exit."
			"\n\nException: ");
		s += from_ascii(e.what());
		Alert::error(_("Software exception Detected"), s);
		lyx_exit(1);
	}
	catch (...) {
		docstring s = _("LyX has caught some really weird exception, it will "
			"now attempt to save all unsaved documents and exit.");
		Alert::error(_("Software exception Detected"), s);
		lyx_exit(1);
	}

	return false;
}

bool GuiApplication::isInDarkMode()
{
	return colorCache().isDarkMode();
}


bool GuiApplication::getRgbColor(ColorCode col, RGBColor & rgbcol)
{
	QColor const & qcol = d->color_cache_.get(col);
	if (!qcol.isValid()) {
		rgbcol.r = 0;
		rgbcol.g = 0;
		rgbcol.b = 0;
		return false;
	}
	rgbcol.r = qcol.red();
	rgbcol.g = qcol.green();
	rgbcol.b = qcol.blue();
	return true;
}


bool Application::getRgbColorUncached(ColorCode col, RGBColor & rgbcol)
{
	QColor const qcol(lcolor.getX11HexName(col).c_str());
	if (!qcol.isValid()) {
		rgbcol.r = 0;
		rgbcol.g = 0;
		rgbcol.b = 0;
		return false;
	}
	rgbcol.r = qcol.red();
	rgbcol.g = qcol.green();
	rgbcol.b = qcol.blue();
	return true;
}


string const GuiApplication::hexName(ColorCode col)
{
	return ltrim(fromqstr(d->color_cache_.get(col).name()), "#");
}


void GuiApplication::registerSocketCallback(int fd, SocketCallback func)
{
	SocketNotifier * sn = new SocketNotifier(this, fd, func);
	d->socket_notifiers_[fd] = sn;
	connect(sn, SIGNAL(activated(int)), this, SLOT(socketDataReceived(int)));
}


void GuiApplication::socketDataReceived(int fd)
{
	d->socket_notifiers_[fd]->func_();
}


void GuiApplication::unregisterSocketCallback(int fd)
{
	d->socket_notifiers_.take(fd)->setEnabled(false);
}


void GuiApplication::commitData(QSessionManager & sm)
{
	/** The implementation is required to avoid an application exit
	 ** when session state save is triggered by session manager.
	 ** The default implementation sends a close event to all
	 ** visible top level widgets when session management allows
	 ** interaction.
	 ** We are changing that to check the state of each buffer in all
	 ** views and ask the users what to do if buffers are dirty.
	 ** Furthermore, we save the session state.
	 ** We do NOT close the views here since the user still can cancel
	 ** the logout process (see #9277); also, this would hide LyX from
	 ** an OSes own session handling (application restoration).
	 **/
	#ifdef QT_NO_SESSIONMANAGER
		#ifndef _MSC_VER
			#warning Qt is compiled without session manager
		#else
			#pragma message("warning: Qt is compiled without session manager")
		#endif
		(void) sm;
	#else
		if (sm.allowsInteraction() && !prepareAllViewsForLogout())
			sm.cancel();
		else
			sm.release();
	#endif
}


void GuiApplication::unregisterView(GuiView * gv)
{
	if(d->views_.contains(gv->id()) && d->views_.value(gv->id()) == gv) {
		d->views_.remove(gv->id());
		if (current_view_ == gv)
			current_view_ = nullptr;
	}
}


bool GuiApplication::closeAllViews()
{
	if (d->views_.empty())
		return true;

	// When a view/window was closed before without quitting LyX, there
	// are already entries in the lastOpened list.
	theSession().lastOpened().clear();

	QList<GuiView *> const views = d->views_.values();
	for (GuiView * view : views) {
		if (!view->closeScheduled())
			return false;
	}

	d->views_.clear();
	return true;
}


bool GuiApplication::prepareAllViewsForLogout()
{
	if (d->views_.empty())
		return true;

	QList<GuiView *> const views = d->views_.values();
	for (GuiView * view : views) {
		if (!view->prepareAllBuffersForLogout())
			return false;
	}

	return true;
}


GuiView & GuiApplication::view(int id) const
{
	LAPPERR(d->views_.contains(id));
	return *d->views_.value(id);
}


void GuiApplication::hideDialogs(string const & name, Inset * inset) const
{
	QList<GuiView *> const views = d->views_.values();
	for (GuiView * view : views)
		view->hideDialog(name, inset);
}


Buffer const * GuiApplication::updateInset(Inset const * inset) const
{
	Buffer const * buf = nullptr;
	QHash<int, GuiView *>::const_iterator end = d->views_.end();
	for (QHash<int, GuiView *>::iterator it = d->views_.begin(); it != end; ++it) {
		if (Buffer const * ptr = (*it)->updateInset(inset))
			buf = ptr;
	}
	return buf;
}


bool GuiApplication::searchMenu(FuncRequest const & func,
	docstring_list & names) const
{
	BufferView * bv = nullptr;
	if (current_view_)
		bv = current_view_->currentBufferView();
	return d->menus_.searchMenu(func, names, bv);
}


bool GuiApplication::hasBufferView() const
{
	return (current_view_ && current_view_->currentBufferView());
}


// Ensure that a file is read only once (prevents include loops)
static QStringList uifiles;
// store which ui files define Toolbars
static QStringList toolbar_uifiles;


GuiApplication::ReturnValues GuiApplication::readUIFile(FileName const & ui_path)
{
	enum {
		ui_menuset = 1,
		ui_toolbars,
		ui_toolbarset,
		ui_include,
		ui_format,
		ui_last
	};

	LexerKeyword uitags[] = {
		{ "format", ui_format },
		{ "include", ui_include },
		{ "menuset", ui_menuset },
		{ "toolbars", ui_toolbars },
		{ "toolbarset", ui_toolbarset }
	};

	Lexer lex(uitags);
	lex.setFile(ui_path);
	if (!lex.isOK()) {
		lyxerr << "Unable to set LyXLeX for ui file: " << ui_path
					 << endl;
	}

	if (lyxerr.debugging(Debug::PARSER))
		lex.printTable(lyxerr);

	bool error = false;
	// format before introduction of format tag
	unsigned int format = 0;
	while (lex.isOK()) {
		int const status = lex.lex();

		// we have to do this check here, outside the switch,
		// because otherwise we would start reading include files,
		// e.g., if the first tag we hit was an include tag.
		if (status == ui_format)
			if (lex.next()) {
				format = lex.getInteger();
				continue;
			}

		// this will trigger unless the first tag we hit is a format
		// tag, with the right format.
		if (format != LFUN_FORMAT)
			return FormatMismatch;

		switch (status) {
		case Lexer::LEX_FEOF:
			continue;

		case ui_include: {
			lex.next(true);
			QString const file = toqstr(lex.getString());
			bool const success = readUIFile(file, true);
			if (!success) {
				LYXERR0("Failed to read included file: " << fromqstr(file));
				return ReadError;
			}
			break;
		}

		case ui_menuset:
			d->menus_.read(lex);
			break;

		case ui_toolbarset:
			d->toolbars_.readToolbars(lex);
			break;

		case ui_toolbars:
			d->toolbars_.readToolbarSettings(lex);
			toolbar_uifiles.push_back(toqstr(ui_path.absFileName()));
			break;

		default:
			if (!rtrim(lex.getString()).empty())
				lex.printError("LyX::ReadUIFile: "
				               "Unknown menu tag: `$$Token'");
			else
				LYXERR0("Error with status: " << status);
			error = true;
			break;
		}

	}
	return (error ? ReadError : ReadOK);
}


bool GuiApplication::readUIFile(QString const & name, bool include)
{
	LYXERR(Debug::INIT, "About to read " << name << "...");

	FileName ui_path;
	if (include) {
		ui_path = libFileSearch("ui", name, "inc");
		if (ui_path.empty())
			ui_path = libFileSearch("ui", changeExtension(name, "inc"));
	} else {
		ui_path = libFileSearch("ui", name, "ui");
	}

	if (ui_path.empty()) {
		static const QString defaultUIFile = "default";
		LYXERR(Debug::INIT, "Could not find " << name);
		if (include) {
			Alert::warning(_("Could not find UI definition file"),
				bformat(_("Error while reading the included file\n%1$s\n"
					"Please check your installation."), qstring_to_ucs4(name)));
			return false;
		}
		if (name == defaultUIFile) {
			LYXERR(Debug::INIT, "Could not find default UI file!!");
			Alert::warning(_("Could not find default UI file"),
				_("LyX could not find the default UI file!\n"
				  "Please check your installation."));
			return false;
		}
		Alert::warning(_("Could not find UI definition file"),
		bformat(_("Error while reading the configuration file\n%1$s\n"
			"Falling back to default.\n"
			"Please look under Tools>Preferences>User Interface and\n"
			"check which User Interface file you are using."), qstring_to_ucs4(name)));
		return readUIFile(defaultUIFile, false);
	}

	QString const uifile = toqstr(ui_path.absFileName());
	if (uifiles.contains(uifile)) {
		if (!include) {
			// We are reading again the top uifile so reset the safeguard:
			uifiles.clear();
			d->menus_.reset();
			d->toolbars_.reset();
		} else {
			LYXERR(Debug::INIT, "UI file '" << name << "' has been read already. "
				<< "Is this an include loop?");
			return false;
		}
	}
	uifiles.push_back(uifile);

	LYXERR(Debug::INIT, "Found " << name << " in " << ui_path);

	ReturnValues retval = readUIFile(ui_path);

	if (retval == FormatMismatch) {
		LYXERR(Debug::FILES, "Converting ui file to format " << LFUN_FORMAT);
		TempFile tmp("convertXXXXXX.ui");
		FileName const tempfile = tmp.name();
		bool const success = prefs2prefs(ui_path, tempfile, true);
		if (!success) {
			LYXERR0("Unable to convert " << ui_path.absFileName() <<
				" to format " << LFUN_FORMAT << ".");
		} else {
			retval = readUIFile(tempfile);
		}
	}

	if (retval != ReadOK) {
		LYXERR0("Unable to read UI file: " << ui_path.absFileName());
		return false;
	}

	if (include)
		return true;

	QSettings settings;
	settings.beginGroup("ui_files");
	bool touched = false;
	for (int i = 0; i != uifiles.size(); ++i) {
		QFileInfo fi(uifiles[i]);
		QDateTime const date_value = fi.lastModified();
		QString const name_key = QString::number(i);
		// if an ui file which defines Toolbars has changed,
		// we have to reset the settings
		if (toolbar_uifiles.contains(uifiles[i])
		 && (!settings.contains(name_key)
		 || settings.value(name_key).toString() != uifiles[i]
		 || settings.value(name_key + "/date").toDateTime() != date_value)) {
			touched = true;
			settings.setValue(name_key, uifiles[i]);
			settings.setValue(name_key + "/date", date_value);
		}
	}
	settings.endGroup();
	if (touched)
		settings.remove("views");

	return true;
}


void GuiApplication::onLastWindowClosed()
{
	if (d->global_menubar_)
		d->global_menubar_->grabKeyboard();
}


void GuiApplication::onApplicationStateChanged(Qt::ApplicationState state)
{
	std::string name = "unknown";
	switch (state) {
	case Qt::ApplicationSuspended:
		name = "ApplicationSuspended";
		break;
	case Qt::ApplicationHidden:
		name = "ApplicationHidden";
		break;
	case Qt::ApplicationInactive:
		name = "ApplicationInactive";
		break;
	case Qt::ApplicationActive:
		name = "ApplicationActive";
		/// The Dock icon click produces 2 sequential QEvent::ApplicationStateChangeEvent events.
		/// cmd+tab only one QEvent::ApplicationStateChangeEvent event
		if (d->views_.empty() && d->last_state_ == state) {
			LYXERR(Debug::GUI, "Open new window...");
			createAndShowView();
		}
		break;
	}
	LYXERR(Debug::GUI, "onApplicationStateChanged..." << name);
	d->last_state_ = state;
}


void GuiApplication::startLongOperation() {
	d->key_checker_.start();
}


bool GuiApplication::longOperationCancelled() {
	return d->key_checker_.pressed();
}


void GuiApplication::stopLongOperation() {
	d->key_checker_.stop();
}


bool GuiApplication::longOperationStarted() {
	return d->key_checker_.started();
}


////////////////////////////////////////////////////////////////////////
//
// X11 specific stuff goes here...

#if defined(HAVE_XCB_XCB_H) && defined(HAVE_LIBXCB)
bool GuiApplication::nativeEventFilter(const QByteArray & eventType,
				       void * message, QINTPTR *)
{
	if (!current_view_ || eventType != "xcb_generic_event_t")
		return false;

	xcb_generic_event_t * ev = static_cast<xcb_generic_event_t *>(message);

	switch (ev->response_type) {
	case XCB_SELECTION_REQUEST: {
		xcb_selection_request_event_t * srev =
			reinterpret_cast<xcb_selection_request_event_t *>(ev);
		if (srev->selection != XCB_ATOM_PRIMARY)
			break;
		LYXERR(Debug::SELECTION, "X requested selection.");
		BufferView * bv = current_view_->currentBufferView();
		if (bv) {
			docstring const sel = bv->requestSelection();
			if (!sel.empty()) {
				d->selection_.put(sel);
#ifdef HAVE_QT5_X11_EXTRAS
				// Refresh the selection request timestamp.
				// We have to do this by ourselves as Qt seems
				// not doing that, maybe because of our
				// "persistent selection" implementation
				// (see comments in GuiSelection.cpp).
				// It is expected that every X11 event is
				// 32 bytes long, even if not all 32 bytes are
				// needed. See:
				// https://www.x.org/releases/current/doc/man/man3/xcb_send_event.3.xhtml
				struct alignas(32) padded_event
					: xcb_selection_notify_event_t {};
				padded_event nev = {};
				nev.response_type = XCB_SELECTION_NOTIFY;
				nev.requestor = srev->requestor;
				nev.selection = srev->selection;
				nev.target = srev->target;
				nev.property = XCB_NONE;
				nev.time = XCB_CURRENT_TIME;
				xcb_connection_t * con = QX11Info::connection();
				xcb_send_event(con, 0, srev->requestor,
					XCB_EVENT_MASK_NO_EVENT,
					reinterpret_cast<char const *>(&nev));
				xcb_flush(con);
#endif
				return true;
			}
		}
		break;
	}
	case XCB_SELECTION_CLEAR: {
		xcb_selection_clear_event_t * scev =
			reinterpret_cast<xcb_selection_clear_event_t *>(ev);
		if (scev->selection != XCB_ATOM_PRIMARY)
			break;
		LYXERR(Debug::SELECTION, "Lost selection.");
		BufferView * bv = current_view_->currentBufferView();
		if (bv)
			bv->clearSelection();
		break;
	}
	}
	return false;
}
#endif

} // namespace frontend


void hideDialogs(std::string const & name, Inset * inset)
{
	if (theApp())
		frontend::guiApp->hideDialogs(name, inset);
}


////////////////////////////////////////////////////////////////////
//
// Font stuff
//
////////////////////////////////////////////////////////////////////

frontend::FontLoader & theFontLoader()
{
	LAPPERR(frontend::guiApp);
	return frontend::guiApp->fontLoader();
}


frontend::FontMetrics const & theFontMetrics(Font const & f)
{
	return theFontMetrics(f.fontInfo());
}


frontend::FontMetrics const & theFontMetrics(FontInfo const & f)
{
	LAPPERR(frontend::guiApp);
	return frontend::guiApp->fontLoader().metrics(f);
}


////////////////////////////////////////////////////////////////////
//
// Misc stuff
//
////////////////////////////////////////////////////////////////////

frontend::Clipboard & theClipboard()
{
	LAPPERR(frontend::guiApp);
	return frontend::guiApp->clipboard();
}


frontend::Selection & theSelection()
{
	LAPPERR(frontend::guiApp);
	return frontend::guiApp->selection();
}


} // namespace lyx

#include "moc_GuiApplication.cpp"
