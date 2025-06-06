/**
 * \file FontLoader.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Asger Alstrup
 * \author John Levon
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiFontLoader.h"

#include "FontLoader.h"
#include "Font.h"
#include "GuiFontMetrics.h"
#include "qt_helpers.h"

#include "LyXRC.h"

#include "support/debug.h"
#include "support/filetools.h"
#include "support/lassert.h"
#include "support/Package.h"
#include "support/os.h"

#include "GuiApplication.h"

#include <QFontInfo>
#include <QFontDatabase>

using namespace std;
using namespace lyx::support;

QString const math_fonts[] = {"cmex10", "cmmi10", "cmr10", "cmsy10",
	"dsrom10", "esint10", "eufm10", "msam10", "msbm10", "rsfs10",
	"stmary10", "wasy10"};
int const num_math_fonts = sizeof(math_fonts) / sizeof(*math_fonts);

namespace lyx {

bool isMathFamily(QString const & name)
{
	for (int i = 0; i < num_math_fonts; ++i) {
		if (math_fonts[i] == name)
			return true;
	}
	return false;
}

namespace frontend {

/**
 * Matches Fonts against
 * actual QFont instances, and also caches metrics.
 */
class GuiFontInfo
{
public:
	GuiFontInfo(FontInfo const & f);

	/// The font instance
	QFont font;
	/// Metrics on the font
	GuiFontMetrics metrics;
};

namespace {

struct SymbolFont {
	FontFamily lyx_family;
	QString family;
};

SymbolFont symbol_fonts[] = {
	{ SYMBOL_FAMILY,"symbol"},
	{ CMR_FAMILY,   "cmr10"},
	{ CMSY_FAMILY,  "cmsy10"},
	{ CMM_FAMILY,   "cmmi10"},
	{ CMEX_FAMILY,  "cmex10"},
	{ MSA_FAMILY,   "msam10"},
	{ MSB_FAMILY,   "msbm10"},
	{ DS_FAMILY,    "dsrom10"},
	{ EUFRAK_FAMILY,"eufm10"},
	{ RSFS_FAMILY,  "rsfs10"},
	{ STMARY_FAMILY,"stmary10"},
	{ WASY_FAMILY,  "wasy10"},
	{ ESINT_FAMILY, "esint10"}
};

size_t const nr_symbol_fonts = sizeof(symbol_fonts) / sizeof(symbol_fonts[0]);

/// BUTT ugly !
static GuiFontInfo *
fontinfo_[NUM_FAMILIES][NUM_SERIES][NUM_SHAPE][NUM_SIZE][NUM_STYLE];


// returns a reference to the pointer type (GuiFontInfo *) in the
// fontinfo_ table.
GuiFontInfo * & fontinfo_ptr(FontInfo const & f)
{
	// The display font and the text font are the same
	size_t const style = (f.style() == DISPLAY_STYLE) ? TEXT_STYLE : f.style();
	return fontinfo_[f.family()][f.series()][f.realShape()][f.size()][style];
}


// Get font info (font + metrics) for the given LyX font.
// if not cached, create it.
GuiFontInfo & fontinfo(FontInfo const & f)
{
    bool const fontIsRealized =
            (f.family() < NUM_FAMILIES) &&
            (f.series() < NUM_SERIES) &&
            (f.realShape() < NUM_SHAPE) &&
            (f.size() < NUM_SIZE);
    if (!fontIsRealized) {
        // We can reset the font to something sensible in release mode.
        LYXERR0("Unrealized font!" << f);
        LATTEST(false);
        FontInfo f2 = f;
        f2.realize(sane_font);
        GuiFontInfo * & fi = fontinfo_ptr(f2);
        if (!fi)
            fi = new GuiFontInfo(f2);
        return *fi;
    }
    GuiFontInfo * & fi = fontinfo_ptr(f);
	if (!fi)
		fi = new GuiFontInfo(f);
	return *fi;
}


QString symbolFamily(FontFamily family)
{
	for (size_t i = 0; i < nr_symbol_fonts; ++i) {
		if (family == symbol_fonts[i].lyx_family)
			return symbol_fonts[i].family;
	}
	return QString();
}


#if 0
bool isSymbolFamily(FontFamily family)
{
	return family >= SYMBOL_FAMILY && family <= ESINT_FAMILY;
}
#endif


static bool isChosenFont(QFont & font, QString const & family,
	                 QString const & style)
{
	// QFontInfo won't find a font that has only a few glyphs at unusual
	// positions, e.g. the original esint10 font.
	// The workaround is to add dummy glyphs at least at all ASCII
	// positions.
	QFontInfo fi(font);

	LYXERR_NOPOS(Debug::FONT, "got: " << fi.family()
		     << " (style: " << fi.styleName() << ")");

	if (fi.family().contains(family)
	    && (style.isEmpty() || fi.styleName().contains(style))) {
		LYXERR_NOENDL(Debug::FONT, " got it ");
		return true;
	}

	return false;
}


QFont symbolFont(QString const & family, bool * ok)
{
	LYXERR_NOENDL(Debug::FONT, "Looking for font family " << family << " ... ");
	QString upper = family;
	upper[0] = family[0].toUpper();

	QFont font;
	if (guiApp->platformName() == "xcb"
	    || guiApp->platformName().contains("wayland")) {
		// On *nix we have to also specify the foundry to be able to
		// discriminate our fonts when the texlive fonts are managed by
		// fontconfig. Unfortunately, doing the same on Windows breaks things.
		font.setFamily(family + QLatin1String(" [LyEd]"));
	} else {
		font.setFamily(family);
	}
	font.setStyleStrategy(QFont::NoFontMerging);
	font.setStyleName("LyX");

	if (isChosenFont(font, family, "LyX")) {
		LYXERR_NOPOS(Debug::FONT, "lyx!");
		*ok = true;
		return font;
	}

	LYXERR_NOENDL(Debug::FONT, "Trying normal " << family << " ... ");
	font.setStyleName(QString());

	if (isChosenFont(font, family, QString())) {
		LYXERR_NOPOS(Debug::FONT, "normal!");
		*ok = true;
		return font;
	}

	LYXERR_NOENDL(Debug::FONT, "Trying " << upper << " ... ");
	font.setFamily(upper);

	if (isChosenFont(font, upper, QString())) {
		LYXERR_NOPOS(Debug::FONT, "upper!");
		*ok = true;
		return font;
	}

	LYXERR_NOPOS(Debug::FONT, " FAILED :-(");
	*ok = false;
	return font;
}

} // namespace


FontLoader::FontLoader()
{
	QString const fonts_dir =
		toqstr(addPath(package().system_support().absFileName(), "fonts"));

	for (int i = 0 ; i < num_math_fonts; ++i) {
		QString const font_file = fonts_dir + math_fonts[i] + ".ttf";
		int fontID = QFontDatabase::addApplicationFont(font_file);

		LYXERR(Debug::FONT, "Adding font " << font_file
				    << (fontID < 0 ? " FAIL" : " OK"));
	}

	for (int i1 = 0; i1 < NUM_FAMILIES; ++i1)
		for (int i2 = 0; i2 < NUM_SERIES; ++i2)
			for (int i3 = 0; i3 < NUM_SHAPE; ++i3)
				for (int i4 = 0; i4 < NUM_SIZE; ++i4)
					for (int i5 = 0; i5 < NUM_STYLE; ++i5)
						fontinfo_[i1][i2][i3][i4][i5] = 0;
}


void FontLoader::update()
{
	for (int i1 = 0; i1 < NUM_FAMILIES; ++i1)
		for (int i2 = 0; i2 < NUM_SERIES; ++i2)
			for (int i3 = 0; i3 < NUM_SHAPE; ++i3)
				for (int i4 = 0; i4 < NUM_SIZE; ++i4)
					for (int i5 = 0; i5 < NUM_STYLE; ++i5) {
					delete fontinfo_[i1][i2][i3][i4][i5];
					fontinfo_[i1][i2][i3][i4][i5] = 0;
				}
}


FontLoader::~FontLoader()
{
	update();
}

/////////////////////////////////////////////////

namespace {

QString makeFontName(QString const & family, QString const & foundry)
{
	QString res = family;
	if (!foundry.isEmpty())
		res += " [" + foundry + ']';
	return res;
}


QFont makeQFont(FontInfo const & f)
{
	QFont font;
	QString const pat = symbolFamily(f.family());
	if (!pat.isEmpty()) {
		bool ok;
		font = symbolFont(pat, &ok);
	} else {
		switch (f.family()) {
		case ROMAN_FAMILY: {
			QString family = makeFontName(toqstr(lyxrc.roman_font_name),
				toqstr(lyxrc.roman_font_foundry));
			font.setFamily(family);
			break;
		}
		case SANS_FAMILY:
			font.setFamily(makeFontName(toqstr(lyxrc.sans_font_name),
						    toqstr(lyxrc.sans_font_foundry)));
			break;
		case TYPEWRITER_FAMILY:
			font.setFamily(makeFontName(toqstr(lyxrc.typewriter_font_name),
						    toqstr(lyxrc.typewriter_font_foundry)));
			break;
		default:
			break;
		}
	}

	switch (f.series()) {
		case MEDIUM_SERIES:
			font.setWeight(QFont::Normal);
			break;
		case BOLD_SERIES:
			font.setWeight(QFont::Bold);
			break;
		default:
			break;
	}

	switch (f.realShape()) {
		case ITALIC_SHAPE:
			font.setStyle(QFont::StyleItalic);
			break;
		case SLANTED_SHAPE:
			font.setStyle(QFont::StyleOblique);
			break;
		case SMALLCAPS_SHAPE:
			font.setCapitalization(QFont::SmallCaps);
			break;
		default:
			break;
	}

	LYXERR(Debug::FONT, "Font '" << f.stateText()
		<< "' matched by\n" << font.family());

	// Is this an exact match?
	if (font.exactMatch())
		LYXERR(Debug::FONT, "This font is an exact match");
	else
		LYXERR(Debug::FONT, "This font is NOT an exact match");

	font.setPointSizeF(f.realSize() * lyxrc.currentZoom / 100.0);

	LYXERR(Debug::FONT, "The font has size: " << font.pointSizeF());

	return font;
}

} // namespace


GuiFontInfo::GuiFontInfo(FontInfo const & f)
	: font(makeQFont(f)), metrics(font)
{}


bool FontLoader::available(FontInfo const & f)
{
	// FIXME THREAD
	static vector<int> cache_set(NUM_FAMILIES, false);
	static vector<int> cache(NUM_FAMILIES, false);

	FontFamily family = f.family();
#ifdef Q_OS_MAC
	// Apple ships a font name "Symbol", which has more or less the same
	// glyphs as the original PostScript Symbol font, but it uses a different
	// encoding (see https://en.wikipedia.org/wiki/Symbol_(typeface)#cite_note-2).
	// Since we expect the font specific encoding of the original
	// PostScript Symbol font, we can't use the one provided on macOS.
	// See also the discussion in bug 7954.
	if (f.family() == SYMBOL_FAMILY)
		return false;
#endif
	if (cache_set[family])
		return cache[family];
	cache_set[family] = true;

	QString const pat = symbolFamily(family);
	if (pat.isEmpty())
		// We don't care about non-symbol fonts
		return false;

	bool ok;
	symbolFont(pat, &ok);
	if (!ok)
		return false;

	cache[family] = true;
	return true;
}


bool FontLoader::canBeDisplayed(char_type c)
{
	// bug 8493: with Qt5 and Qt6, characters with codepoints 9, 10, 12 and
	// 13 are rendered as spaces. We handle that in our TeX fonts by making
	// copies of the glyphs elsewhere, but this may trigger if we are
	// unlucky.
	if (c == 0x0009 || c == 0x000a || c == 0x000c || c == 0x000d)
		return false;
	return true;
}


FontMetrics const & FontLoader::metrics(FontInfo const & f)
{
	return fontinfo(f).metrics;
}


GuiFontMetrics const & getFontMetrics(FontInfo const & f)
{
	return fontinfo(f).metrics;
}


QFont const & getFont(FontInfo const & f)
{
	return fontinfo(f).font;
}

} // namespace frontend
} // namespace lyx
