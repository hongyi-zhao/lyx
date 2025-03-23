/**
 * \file Preamble.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 * \author Uwe Stöhr
 *
 * Full author contact details are available in file CREDITS.
 */

// {[(

#include <config.h>

#include "Preamble.h"
#include "tex2lyx.h"

#include "Encoding.h"
#include "LaTeXPackages.h"
#include "LayoutFile.h"
#include "TextClass.h"
#include "version.h"

#include "support/convert.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/Lexer.h"
#include "support/lstrings.h"

#include <algorithm>
#include <iostream>
#include <regex>

using namespace std;
using namespace lyx::support;


namespace lyx {

Preamble preamble;

namespace {

// CJK languages are handled in text.cpp, polyglossia languages are listed
// further down.
/**
 * known babel language names (including synonyms)
 * not in standard babel: arabic, arabtex, armenian, belarusian, serbian-latin, thai
 * please keep this in sync with known_coded_languages line by line!
 */
const char * const known_languages[] = {"acadian", "afrikaans", "albanian",
"american", "amharic", "ancientgreek", "arabic", "arabtex", "armenian", "asturian",
"australian", "austrian", "azerbaijani", "bahasa", "bahasai", "bahasam", "basque",
"belarusian", "bengali", "bosnian", "brazil", "brazilian", "breton", "british",
"bulgarian", "canadian", "canadien", "catalan", "churchslavic", "classiclatin", "coptic",
"croatian", "czech", "danish", "divehi", "dutch", "ecclesiasticlatin", "english", "esperanto",
"estonian", "farsi", "finnish", "francais", "french", "frenchb", "frenchle", "frenchpro", "friulan",
"galician", "german", "germanb", "georgian", "greek", "hebrew", "hindi", "hungarian", "icelandic",
"indon", "indonesian", "interlingua", "irish", "italian", "japanese", "kannada", "kazakh", "khmer",
"kurmanji", "lao", "latin", "latvian", "lithuanian", "lowersorbian", "lsorbian", "macedonian", "magyar",
"malay", "malayalam", "marathi", "medievallatin", "meyalu", "mexican", "mongolian", "naustrian", "newzealand",
"ngerman", "ngermanb", "nko", "norsk", "nswissgerman", "nynorsk", "occitan", "odia", "piedmontese",
"polutonikogreek", "polish", "portuges", "portuguese", "romanian", "romansh", "russian", "russianb",
"samin", "scottish", "serbian", "serbian-latin", "punjabi", "sanskrit", "slovak", "slovene", "sorani",
"spanish", "swedish", "swissgerman", "syriac", "tamil", "telugu", "thai", "tibetan", "turkish", "turkmen",
"ukraineb", "ukrainian", "uppersorbian", "UKenglish", "urdu", "USenglish", "usorbian", "uyghur",
"vietnam", "welsh",
0};

/**
 * the same as known_languages with .lyx names
 * please keep this in sync with known_languages line by line!
 */
const char * const known_coded_languages[] = {"french", "afrikaans", "albanian",
"american", "amharic", "ancientgreek", "arabic_arabi", "arabic_arabtex", "armenian", "asturian",
"australian", "austrian", "azerbaijani", "bahasa", "bahasa", "bahasam", "basque",
"belarusian", "bengali", "bosnian", "brazilian", "brazilian", "breton", "british",
"bulgarian", "canadian", "canadien", "catalan", "churchslavonic", "latin-classic", "coptic",
"croatian", "czech", "danish", "divehi", "dutch", "latin-ecclesiastic", "english", "esperanto",
"estonian", "farsi", "finnish", "french", "french", "french", "french", "french", "friulan",
"galician", "german", "german", "georgian", "greek", "hebrew", "hindi", "magyar", "icelandic",
"bahasa", "bahasa", "interlingua", "irish", "italian", "japanese", "kannada", "kazakh", "khmer",
"kurmanji", "lao", "latin", "latvian", "lithuanian", "lowersorbian", "lowersorbian", "macedonian", "magyar",
"bahasam", "malayalam", "marathi", "latin-medieval", "bahasam", "spanish-mexico", "mongolian", "naustrian", "newzealand",
"ngerman", "ngerman", "nko", "norsk", "german-ch", "nynorsk", "occitan", "odia", "piedmontese",
"polutonikogreek", "polish", "portuguese", "portuguese", "romanian", "romansh", "russian", "russian",
"samin", "scottish", "serbian", "serbian-latin", "punjabi", "sanskrit", "slovak", "slovene", "sorani",
"spanish", "swedish", "german-ch-old", "syriac", "tamil", "telugu", "thai", "tibetan", "turkish", "turkmen",
"ukrainian", "ukrainian", "uppersorbian", "english", "urdu", "english", "uppersorbian", "uyghur",
"vietnamese", "welsh",
0};

/// languages with british quotes (.lyx names)
const char * const known_british_quotes_languages[] = {"british", "welsh", 0};

/// languages with cjk quotes (.lyx names)
const char * const known_cjk_quotes_languages[] = {"chinese-traditional",
"japanese", "japanese-cjk", 0};

/// languages with cjk-angle quotes (.lyx names)
const char * const known_cjkangle_quotes_languages[] = {"korean", 0};

/// languages with danish quotes (.lyx names)
const char * const known_danish_quotes_languages[] = {"danish", 0};

/// languages with english quotes (.lyx names)
const char * const known_english_quotes_languages[] = {"american", "australian",
"bahasa", "bahasam", "bengali", "brazilian", "canadian", "chinese-simplified", "english",
"esperanto", "farsi", "interlingua", "irish", "newzealand", "scottish",
"thai", "turkish", "vietnamese", 0};

/// languages with french quotes (.lyx names)
const char * const known_french_quotes_languages[] = {"ancientgreek",
"arabic_arabi", "arabic_arabtex", "asturian", "belarusian", "breton",
"canadien", "catalan", "french", "friulan", "galician", "italian", "occitan",
"piedmontese", "portuguese", "spanish", "spanish-mexico", 0};

/// languages with german quotes (.lyx names)
const char * const known_german_quotes_languages[] = {"austrian", "bulgarian",
"czech", "estonian", "georgian", "german", "icelandic", "latvian", "lithuanian",
"lowersorbian", "macedonian", "naustrian", "ngerman", "romansh", "slovak", "slovene",
"uppersorbian", 0};

/// languages with polish quotes (.lyx names)
const char * const known_polish_quotes_languages[] = {"afrikaans", "bosnian", "croatian",
"dutch", "magyar", "polish", "romanian", "serbian", "serbian-latin", 0};

/// languages with hungarian quotes (.lyx names)
const char * const known_hungarian_quotes_languages[] = {"magyar", 0};

/// languages with russian quotes (.lyx names)
const char * const known_russian_quotes_languages[] = {"azerbaijani", "oldrussian",
"russian", "ukrainian", 0};

/// languages with swedish quotes (.lyx names)
const char * const known_swedish_quotes_languages[] = {"finnish", "swedish", 0};

/// languages with swiss quotes (.lyx names)
const char * const known_swiss_quotes_languages[] = {"albanian",
"armenian", "basque", "churchslavonic", "german-ch", "german-ch-old",
"norsk", "nynorsk", "turkmen", "ukrainian", "vietnamese", 0};

/// languages with hebrew quotes (.lyx names)
const char * const known_hebrew_quotes_languages[] = {"hebrew", 0};

/// known language packages from the times before babel
const char * const known_old_language_packages[] = {"french", "frenchle",
"frenchpro", "german", "ngerman", "pmfrench", 0};

char const * const known_fontsizes[] = { "10pt", "11pt", "12pt", 0 };

const char * const known_roman_font_packages[] = { "ae", "beraserif", "bookman",
"ccfonts", "chancery", "charter", "cmr", "cochineal", "crimson", "CrimsonPro", "DejaVuSerif",
"DejaVuSerifCondensed", "fourier", "garamondx", "libertine", "libertineRoman", "libertine-type1",
"lmodern", "mathdesign", "mathpazo", "mathptmx", "MinionPro", "newcent", "noto", "noto-serif",
"PTSerif", "tgbonum", "tgchorus", "tgpagella", "tgschola", "tgtermes", "utopia", "xcharter", 0 };

const char * const known_sans_font_packages[] = { "avant", "berasans", "biolinum",
"biolinum-type1", "cantarell", "Chivo", "cmbr", "cmss", "DejaVuSans", "DejaVuSansCondensed", "FiraSans", "helvet", "iwona",
"iwonac", "iwonal", "iwonalc", "kurier", "kurierc", "kurierl", "kurierlc", "LibertinusSans-LF", "lmss", "noto-sans", "PTSans",
"tgadventor", "tgheros", "uop", 0 };

const char * const known_typewriter_font_packages[] = { "beramono", "cmtl", "cmtt", "courier", "DejaVuSansMono",
"FiraMono", "lmtt", "luximono", "libertineMono", "libertineMono-type1", "LibertinusMono-TLF", "lmodern",
"mathpazo", "mathptmx", "newcent", "noto-mono", "PTMono", "tgcursor", "txtt", 0 };

const char * const known_math_font_packages[] = { "eulervm", "newtxmath", 0};

const char * const known_latex_paper_sizes[] = { "a0paper", "b0paper", "c0paper",
"a1paper", "b1paper", "c1paper", "a2paper", "b2paper", "c2paper", "a3paper",
"b3paper", "c3paper", "a4paper", "b4paper", "c4paper", "a5paper", "b5paper",
"c5paper", "a6paper", "b6paper", "c6paper", "executivepaper", "legalpaper",
"letterpaper", "b0j", "b1j", "b2j", "b3j", "b4j", "b5j", "b6j", 0};

const char * const known_paper_margins[] = { "lmargin", "tmargin", "rmargin",
"bmargin", "headheight", "headsep", "footskip", "columnsep", 0};

const char * const known_coded_paper_margins[] = { "leftmargin", "topmargin",
"rightmargin", "bottommargin", "headheight", "headsep", "footskip",
"columnsep", 0};

/// commands that can start an \if...\else...\endif sequence
const char * const known_if_commands[] = {"if", "ifarydshln", "ifbraket",
"ifcancel", "ifcolortbl", "ifeurosym", "ifmarginnote", "ifmmode", "ifpdf",
"ifsidecap", "ifupgreek", 0};

/// LaTeX names for basic colors
const char * const known_basic_colors[] = {"black", "blue", "cyan",
	"green", "magenta", "red", "white", "yellow", 0};

/// LaTeX names for additional basic colors provided by xcolor
const char * const known_basic_xcolors[] = {"brown", "darkgray", "gray",
	"lightgray", "lime", "orange", "olive", "pink", "purple",
	"teal", "violet", 0};

/// LaTeX names for (xcolor) colors beyond the base ones
char const * const known_textcolors[] = { "Apricot", "Aquamarine", "Bittersweet", "Black",
"Blue", "BlueGreen", "BlueViolet", "BrickRed", "Brown", "BurntOrange", "CadetBlue", "CarnationPink",
"Cerulean", "CornflowerBlue", "Cyan", "Dandelion", "DarkOrchid", "Emerald", "ForestGreen", "Fuchsia",
"Goldenrod", "Gray", "Green", "GreenYellow", "JungleGreen", "Lavender", "LimeGreen", "Magenta",
"Mahogany", "Maroon", "Melon", "MidnightBlue", "Mulberry", "NavyBlue", "OliveGreen", "Orange", "OrangeRed",
"Orchid", "Peach", "Periwinkle", "PineGreen", "Plum", "ProcessBlue", "Purple", "RawSienna", "Red", "RedOrange",
"RedViolet", "Rhodamine", "RoyalBlue", "RoyalPurple", "RubineRed", "Salmon", "SeaGreen", "Sepia", "SkyBlue",
"SpringGreen", "Tan", "TealBlue", "Thistle", "Turquoise", "Violet", "VioletRed", "White", "WildStrawberry",
"Yellow", "YellowGreen", "YellowOrange", "AntiqueWhite1", "AntiqueWhite2", "AntiqueWhite3", "AntiqueWhite4", "Aquamarine1",
"Aquamarine2", "Aquamarine3", "Aquamarine4", "Azure1", "Azure2", "Azure3", "Azure4", "Bisque1", "Bisque2", "Bisque3", "Bisque4",
"Blue1", "Blue2", "Blue3", "Blue4", "Brown1", "Brown2", "Brown3", "Brown4", "Burlywood1", "Burlywood2", "Burlywood3", "Burlywood4",
"CadetBlue1", "CadetBlue2", "CadetBlue3", "CadetBlue4", "Chartreuse1", "Chartreuse2", "Chartreuse3", "Chartreuse4", "Chocolate1",
"Chocolate2", "Chocolate3", "Chocolate4", "Coral1", "Coral2", "Coral3", "Coral4", "Cornsilk1", "Cornsilk2", "Cornsilk3", "Cornsilk4",
"Cyan1", "Cyan2", "Cyan3", "Cyan4", "DarkGoldenrod1", "DarkGoldenrod2", "DarkGoldenrod3", "DarkGoldenrod4", "DarkOliveGreen1",
"DarkOliveGreen2", "DarkOliveGreen3", "DarkOliveGreen4", "DarkOrange1", "DarkOrange2", "DarkOrange3", "DarkOrange4", "DarkOrchid1",
"DarkOrchid2", "DarkOrchid3", "DarkOrchid4", "DarkSeaGreen1", "DarkSeaGreen2", "DarkSeaGreen3", "DarkSeaGreen4", "DarkSlateGray1",
"DarkSlateGray2", "DarkSlateGray3", "DarkSlateGray4", "DeepPink1", "DeepPink2", "DeepPink3", "DeepPink4", "DeepSkyBlue1", "DeepSkyBlue2",
"DeepSkyBlue3", "DeepSkyBlue4", "DodgerBlue1", "DodgerBlue2", "DodgerBlue3", "DodgerBlue4", "Firebrick1", "Firebrick2", "Firebrick3",
"Firebrick4", "Gold1", "Gold2", "Gold3", "Gold4", "Goldenrod1", "Goldenrod2", "Goldenrod3", "Goldenrod4", "Green1", "Green2", "Green3",
"Green4", "Honeydew1", "Honeydew2", "Honeydew3", "Honeydew4", "HotPink1", "HotPink2", "HotPink3", "HotPink4", "IndianRed1", "IndianRed2",
"IndianRed3", "IndianRed4", "Ivory1", "Ivory2", "Ivory3", "Ivory4", "Khaki1", "Khaki2", "Khaki3", "Khaki4", "LavenderBlush1", "LavenderBlush2",
"LavenderBlush3", "LavenderBlush4", "LemonChiffon1", "LemonChiffon2", "LemonChiffon3", "LemonChiffon4", "LightBlue1", "LightBlue2", "LightBlue3",
"LightBlue4", "LightCyan1", "LightCyan2", "LightCyan3", "LightCyan4", "LightGoldenrod1", "LightGoldenrod2", "LightGoldenrod3", "LightGoldenrod4",
"LightPink1", "LightPink2", "LightPink3", "LightPink4", "LightSalmon1", "LightSalmon2", "LightSalmon3", "LightSalmon4", "LightSkyBlue1",
"LightSkyBlue2", "LightSkyBlue3", "LightSkyBlue4", "LightSteelBlue1", "LightSteelBlue2", "LightSteelBlue3", "LightSteelBlue4", "LightYellow1",
"LightYellow2", "LightYellow3", "LightYellow4", "Magenta1", "Magenta2", "Magenta3", "Magenta4", "Maroon1", "Maroon2", "Maroon3", "Maroon4",
"MediumOrchid1", "MediumOrchid2", "MediumOrchid3", "MediumOrchid4", "MediumPurple1", "MediumPurple2", "MediumPurple3", "MediumPurple4", "MistyRose1",
"MistyRose2", "MistyRose3", "MistyRose4", "NavajoWhite1", "NavajoWhite2", "NavajoWhite3", "NavajoWhite4", "OliveDrab1", "OliveDrab2", "OliveDrab3",
"OliveDrab4", "Orange1", "Orange2", "Orange3", "Orange4", "OrangeRed1", "OrangeRed2", "OrangeRed3", "OrangeRed4", "Orchid1", "Orchid2", "Orchid3",
"Orchid4", "PaleGreen1", "PaleGreen2", "PaleGreen3", "PaleGreen4", "PaleTurquoise1", "PaleTurquoise2", "PaleTurquoise3", "PaleTurquoise4",
"PaleVioletRed1", "PaleVioletRed2", "PaleVioletRed3", "PaleVioletRed4", "PeachPuff1", "PeachPuff2", "PeachPuff3", "PeachPuff4", "Pink1",
"Pink2", "Pink3", "Pink4", "Plum1", "Plum2", "Plum3", "Plum4", "Purple1", "Purple2", "Purple3", "Purple4", "Red1", "Red2", "Red3", "Red4",
"RosyBrown1", "RosyBrown2", "RosyBrown3", "RosyBrown4", "RoyalBlue1", "RoyalBlue2", "RoyalBlue3", "RoyalBlue4", "Salmon1", "Salmon2",
"Salmon3", "Salmon4", "SeaGreen1", "SeaGreen2", "SeaGreen3", "SeaGreen4", "Seashell1", "Seashell2", "Seashell3", "Seashell4", "Sienna1",
"Sienna2", "Sienna3", "Sienna4", "SkyBlue1", "SkyBlue2", "SkyBlue3", "SkyBlue4", "SlateBlue1", "SlateBlue2", "SlateBlue3", "SlateBlue4",
"SlateGray1", "SlateGray2", "SlateGray3", "SlateGray4", "Snow1", "Snow2", "Snow3", "Snow4", "SpringGreen1", "SpringGreen2", "SpringGreen3",
"SpringGreen4", "SteelBlue1", "SteelBlue2", "SteelBlue3", "SteelBlue4", "Tan1", "Tan2", "Tan3", "Tan4", "Thistle1", "Thistle2", "Thistle3",
"Thistle4", "Tomato1", "Tomato2", "Tomato3", "Tomato4", "Turquoise1", "Turquoise2", "Turquoise3", "Turquoise4", "VioletRed1", "VioletRed2",
"VioletRed3", "VioletRed4", "Wheat1", "Wheat2", "Wheat3", "Wheat4", "Yellow1", "Yellow2", "Yellow3", "Yellow4", "Gray0", "Grey0", "Green0", "Maroon0",
"Purple0", 0};

/// LyX names for (xcolor) colors beyond the base ones (except svgnames)
char const * const known_coded_textcolors[] = { "dvips:apricot", "dvips:aquamarine",
"dvips:bittersweet", "dvips:black", "dvips:blue", "dvips:bluegreen", "dvips:blueviolet",
"dvips:brickred", "dvips:brown", "dvips:burntorange", "dvips:cadetblue", "dvips:carnationpink",
"dvips:cerulean", "dvips:cornflowerblue", "dvips:cyan", "dvips:dandelion", "dvips:darkorchid",
"dvips:emerald", "dvips:forestgreen", "dvips:fuchsia", "dvips:goldenrod", "dvips:gray", "dvips:green",
"dvips:greenyellow", "dvips:junglegreen", "dvips:lavender", "dvips:limegreen", "dvips:magenta",
"dvips:mahogany", "dvips:maroon", "dvips:melon", "dvips:midnightblue", "dvips:mulberry", "dvips:navyblue",
"dvips:olivegreen", "dvips:orange", "dvips:orangered", "dvips:orchid", "dvips:peach", "dvips:periwinkle",
"dvips:pinegreen", "dvips:plum", "dvips:processblue", "dvips:purple", "dvips:rawsienna", "dvips:red",
"dvips:redorange", "dvips:redviolet", "dvips:rhodamine", "dvips:royalblue", "dvips:royalpurple",
"dvips:rubinered", "dvips:salmon", "dvips:seagreen", "dvips:sepia", "dvips:skyblue", "dvips:springgreen",
"dvips:tan", "dvips:tealblue", "dvips:thistle", "dvips:turquoise", "dvips:violet", "dvips:violetred",
"dvips:white", "dvips:wildstrawberry", "dvips:yellow", "dvips:yellowgreen", "dvips:yelloworange",
"x11:antiquewhite1", "x11:antiquewhite2", "x11:antiquewhite3", "x11:antiquewhite4", "x11:aquamarine1",
"x11:aquamarine2", "x11:aquamarine3", "x11:aquamarine4", "x11:azure1", "x11:azure2", "x11:azure3", "x11:azure4", "x11:bisque1",
"x11:bisque2", "x11:bisque3", "x11:bisque4", "x11:blue1", "x11:blue2", "x11:blue3", "x11:blue4", "x11:brown1", "x11:brown2",
"x11:brown3", "x11:brown4", "x11:burlywood1", "x11:burlywood2", "x11:burlywood3", "x11:burlywood4", "x11:cadetblue1", "x11:cadetblue2",
"x11:cadetblue3", "x11:cadetblue4", "x11:chartreuse1", "x11:chartreuse2", "x11:chartreuse3", "x11:chartreuse4", "x11:chocolate1",
"x11:chocolate2", "x11:chocolate3", "x11:chocolate4", "x11:coral1", "x11:coral2", "x11:coral3", "x11:coral4", "x11:cornsilk1",
"x11:cornsilk2", "x11:cornsilk3", "x11:cornsilk4", "x11:cyan1", "x11:cyan2", "x11:cyan3", "x11:cyan4", "x11:darkgoldenrod1",
"x11:darkgoldenrod2", "x11:darkgoldenrod3", "x11:darkgoldenrod4", "x11:darkolivegreen1", "x11:darkolivegreen2", "x11:darkolivegreen3",
"x11:darkolivegreen4", "x11:darkorange1", "x11:darkorange2", "x11:darkorange3", "x11:darkorange4", "x11:darkorchid1", "x11:darkorchid2",
"x11:darkorchid3", "x11:darkorchid4", "x11:darkseagreen1", "x11:darkseagreen2", "x11:darkseagreen3", "x11:darkseagreen4",
"x11:darkslategray1", "x11:darkslategray2", "x11:darkslategray3", "x11:darkslategray4", "x11:deeppink1", "x11:deeppink2", "x11:deeppink3",
"x11:deeppink4", "x11:deepskyblue1", "x11:deepskyblue2", "x11:deepskyblue3", "x11:deepskyblue4", "x11:dodgerblue1", "x11:dodgerblue2",
"x11:dodgerblue3", "x11:dodgerblue4", "x11:firebrick1", "x11:firebrick2", "x11:firebrick3", "x11:firebrick4", "x11:gold1", "x11:gold2",
"x11:gold3", "x11:gold4", "x11:goldenrod1", "x11:goldenrod2", "x11:goldenrod3", "x11:goldenrod4", "x11:green1", "x11:green2", "x11:green3",
"x11:green4", "x11:honeydew1", "x11:honeydew2", "x11:honeydew3", "x11:honeydew4", "x11:hotpink1", "x11:hotpink2", "x11:hotpink3", "x11:hotpink4",
"x11:indianred1", "x11:indianred2", "x11:indianred3", "x11:indianred4", "x11:ivory1", "x11:ivory2", "x11:ivory3", "x11:ivory4", "x11:khaki1",
"x11:khaki2", "x11:khaki3", "x11:khaki4", "x11:lavenderblush1", "x11:lavenderblush2", "x11:lavenderblush3", "x11:lavenderblush4", "x11:lemonchiffon1",
"x11:lemonchiffon2", "x11:lemonchiffon3", "x11:lemonchiffon4", "x11:lightblue1", "x11:lightblue2", "x11:lightblue3", "x11:lightblue4",
"x11:lightcyan1", "x11:lightcyan2", "x11:lightcyan3", "x11:lightcyan4", "x11:lightgoldenrod1", "x11:lightgoldenrod2", "x11:lightgoldenrod3",
"x11:lightgoldenrod4", "x11:lightpink1", "x11:lightpink2", "x11:lightpink3", "x11:lightpink4", "x11:lightsalmon1", "x11:lightsalmon2",
"x11:lightsalmon3", "x11:lightsalmon4", "x11:lightskyblue1", "x11:lightskyblue2", "x11:lightskyblue3", "x11:lightskyblue4", "x11:lightsteelblue1",
"x11:lightsteelblue2", "x11:lightsteelblue3", "x11:lightsteelblue4", "x11:lightyellow1", "x11:lightyellow2", "x11:lightyellow3",
"x11:lightyellow4", "x11:magenta1", "x11:magenta2", "x11:magenta3", "x11:magenta4", "x11:maroon1", "x11:maroon2", "x11:maroon3", "x11:maroon4",
"x11:mediumorchid1", "x11:mediumorchid2", "x11:mediumorchid3", "x11:mediumorchid4", "x11:mediumpurple1", "x11:mediumpurple2", "x11:mediumpurple3",
"x11:mediumpurple4", "x11:mistyrose1", "x11:mistyrose2", "x11:mistyrose3", "x11:mistyrose4", "x11:navajowhite1", "x11:navajowhite2",
"x11:navajowhite3", "x11:navajowhite4", "x11:olivedrab1", "x11:olivedrab2", "x11:olivedrab3", "x11:olivedrab4", "x11:orange1", "x11:orange2",
"x11:orange3", "x11:orange4", "x11:orangered1", "x11:orangered2", "x11:orangered3", "x11:orangered4", "x11:orchid1", "x11:orchid2",
"x11:orchid3", "x11:orchid4", "x11:palegreen1", "x11:palegreen2", "x11:palegreen3", "x11:palegreen4", "x11:paleturquoise1", "x11:paleturquoise2",
"x11:paleturquoise3", "x11:paleturquoise4", "x11:palevioletred1", "x11:palevioletred2", "x11:palevioletred3", "x11:palevioletred4",
"x11:peachpuff1", "x11:peachpuff2", "x11:peachpuff3", "x11:peachpuff4", "x11:pink1", "x11:pink2", "x11:pink3", "x11:pink4", "x11:plum1",
"x11:plum2", "x11:plum3", "x11:plum4", "x11:purple1", "x11:purple2", "x11:purple3", "x11:purple4", "x11:red1", "x11:red2", "x11:red3",
"x11:red4", "x11:rosybrown1", "x11:rosybrown2", "x11:rosybrown3", "x11:rosybrown4", "x11:royalblue1", "x11:royalblue2", "x11:royalblue3",
"x11:royalblue4", "x11:salmon1", "x11:salmon2", "x11:salmon3", "x11:salmon4", "x11:seagreen1", "x11:seagreen2", "x11:seagreen3", "x11:seagreen4",
"x11:seashell1", "x11:seashell2", "x11:seashell3", "x11:seashell4", "x11:sienna1", "x11:sienna2", "x11:sienna3", "x11:sienna4", "x11:skyblue1",
"x11:skyblue2", "x11:skyblue3", "x11:skyblue4", "x11:slateblue1", "x11:slateblue2", "x11:slateblue3", "x11:slateblue4", "x11:slategray1",
"x11:slategray2", "x11:slategray3", "x11:slategray4", "x11:snow1", "x11:snow2", "x11:snow3", "x11:snow4", "x11:springgreen1", "x11:springgreen2",
"x11:springgreen3", "x11:springgreen4", "x11:steelblue1", "x11:steelblue2", "x11:steelblue3", "x11:steelblue4", "x11:tan1", "x11:tan2", "x11:tan3",
"x11:tan4", "x11:thistle1", "x11:thistle2", "x11:thistle3", "x11:thistle4", "x11:tomato1", "x11:tomato2", "x11:tomato3", "x11:tomato4",
"x11:turquoise1", "x11:turquoise2", "x11:turquoise3", "x11:turquoise4", "x11:violetred1", "x11:violetred2", "x11:violetred3", "x11:violetred4",
"x11:wheat1", "x11:wheat2", "x11:wheat3", "x11:wheat4", "x11:yellow1", "x11:yellow2", "x11:yellow3", "x11:yellow4", "x11:gray0", "x11:gray0", "x11:green0",
"x11:maroon0", "x11:purple0", 0};

/// LaTeX names for (xcolor) colors beyond the base ones (svgnames)
char const * const known_svgnames_textcolors[] = { "AliceBlue", "AntiqueWhite", "Aqua", "Aquamarine", "Azure", "Beige",
"Bisque", "Black", "BlanchedAlmond", "Blue", "BlueViolet", "Brown", "BurlyWood", "CadetBlue", "Chartreuse",
"Chocolate", "Coral", "CornflowerBlue", "Cornsilk", "Crimson", "Cyan", "DarkBlue", "DarkCyan", "DarkGoldenrod",
"DarkGray", "DarkGrey", "DarkGreen", "DarkKhaki", "DarkMagenta", "DarkOliveGreen", "DarkOrange", "DarkOrchid", "DarkRed",
"DarkSalmon", "DarkSeaGreen", "DarkSlateBlue", "DarkSlateGray", "DarkSlateGrey", "DarkTurquoise", "DarkViolet", "DeepPink", "DeepSkyBlue",
"DimGray", "DimGrey", "DodgerBlue", "FireBrick", "FloralWhite", "ForestGreen", "Fuchsia", "Gainsboro", "GhostWhite", "Gold",
"Goldenrod", "Gray", "Grey", "Green", "GreenYellow", "Honeydew", "HotPink", "IndianRed", "Indigo", "Ivory", "Khaki", "Lavender",
"LavenderBlush", "LawnGreen", "LemonChiffon", "LightBlue", "LightCoral", "LightCyan", "LightGoldenrod",
"LightGoldenrodYellow", "LightGray", "LightGray", "LightGreen", "LightPink", "LightSalmon", "LightSeaGreen", "LightSkyBlue",
"LightSlateBlue", "LightSlateGray", "LightSlateGrey", "LightSteelBlue", "LightYellow", "Lime", "LimeGreen", "Linen", "Magenta", "Maroon",
"MediumAquamarine", "MediumBlue", "MediumOrchid", "MediumPurple", "MediumSeaGreen", "MediumSlateBlue", "MediumSpringGreen",
"MediumTurquoise", "MediumVioletRed", "MidnightBlue", "MintCream", "MistyRose", "Moccasin", "NavajoWhite", "Navy", "NavyBlue",
"OldLace", "Olive", "OliveDrab", "Orange", "OrangeRed", "Orchid", "PaleGoldenrod", "PaleGreen", "PaleTurquoise",
"PaleVioletRed", "PapayaWhip", "PeachPuff", "Peru", "Pink", "Plum", "PowderBlue", "Purple", "Red", "RosyBrown", "RoyalBlue",
"SaddleBrown", "Salmon", "SandyBrown", "SeaGreen", "Seashell", "Sienna", "Silver", "SkyBlue", "SlateBlue", "SlateGray", "SlateGrey", "Snow",
"SpringGreen", "SteelBlue", "Tan", "Teal", "Thistle", "Tomato", "Turquoise", "Violet", "VioletRed", "Wheat", "White",
"WhiteSmoke", "Yellow", "YellowGreen", 0 };

/// LyX names for (xcolor) colors beyond the base ones (svgnames)
char const * const known_coded_svgnames_textcolors[] = { "svg:aliceblue",
"svg:antiquewhite", "svg:aqua", "svg:aquamarine", "svg:azure", "svg:beige", "svg:bisque", "svg:black",
"svg:blanchedalmond", "svg:blue", "svg:blueviolet", "svg:brown", "svg:burlywood", "svg:cadetblue", "svg:chartreuse",
"svg:chocolate", "svg:coral", "svg:cornflowerblue", "svg:cornsilk", "svg:crimson", "svg:cyan", "svg:darkblue",
"svg:darkcyan", "svg:darkgoldenrod", "svg:darkgray", "svg:darkgray", "svg:darkgreen", "svg:darkkhaki", "svg:darkmagenta",
"svg:darkolivegreen", "svg:darkorange", "svg:darkorchid", "svg:darkred", "svg:darksalmon", "svg:darkseagreen",
"svg:darkslateblue", "svg:darkslategray", "svg:darkslategray", "svg:darkturquoise", "svg:darkviolet", "svg:deeppink", "svg:deepskyblue",
"svg:dimgray", "svg:dimgray", "svg:dodgerblue", "svg:firebrick", "svg:floralwhite", "svg:forestgreen", "svg:fuchsia", "svg:gainsboro",
"svg:ghostwhite", "svg:gold", "svg:goldenrod", "svg:gray", "svg:gray", "svg:green", "svg:greenyellow", "svg:honeydew", "svg:hotpink",
"svg:indianred", "svg:indigo", "svg:ivory", "svg:khaki", "svg:lavender", "svg:lavenderblush", "svg:lawngreen",
"svg:lemonchiffon", "svg:lightblue", "svg:lightcoral", "svg:lightcyan", "svg:lightgoldenrod", "svg:lightgoldenrodyellow",
"svg:lightgray", "svg:lightgray", "svg:lightgreen", "svg:lightpink", "svg:lightsalmon", "svg:lightseagreen", "svg:lightskyblue",
"svg:lightslateblue", "svg:lightslategray", "svg:lightslategray", "svg:lightsteelblue", "svg:lightyellow", "svg:lime", "svg:limegreen",
"svg:linen", "svg:magenta", "svg:maroon", "svg:mediumaquamarine", "svg:mediumblue", "svg:mediumorchid", "svg:mediumpurple",
"svg:mediumseagreen", "svg:mediumslateblue", "svg:mediumspringgreen", "svg:mediumturquoise", "svg:mediumvioletred", "svg:midnightblue",
"svg:mintcream", "svg:mistyrose", "svg:moccasin", "svg:navajowhite", "svg:navyblue", "svg:navyblue", "svg:oldlace", "svg:olive", "svg:olivedrab",
"svg:orange", "svg:orangered", "svg:orchid", "svg:palegoldenrod", "svg:palegreen", "svg:paleturquoise", "svg:palevioletred",
"svg:papayawhip", "svg:peachpuff", "svg:peru", "svg:pink", "svg:plum", "svg:powderblue", "svg:purple", "svg:red", "svg:rosybrown",
"svg:royalblue", "svg:saddlebrown", "svg:salmon", "svg:sandybrown", "svg:seagreen", "svg:seashell", "svg:sienna", "svg:silver",
"svg:skyblue", "svg:slateblue", "svg:slategray", "svg:slategray", "svg:snow", "svg:springgreen", "svg:steelblue", "svg:tan", "svg:teal", "svg:thistle",
"svg:tomato", "svg:turquoise", "svg:violet", "svg:violetred", "svg:wheat", "svg:white", "svg:whitesmoke", "svg:yellow",
"svg:yellowgreen", 0 };

/// conditional commands with three arguments like \@ifundefined{}{}{}
const char * const known_if_3arg_commands[] = {"@ifundefined", "IfFileExists",
0};

/*!
 * Known file extensions for TeX files as used by \\includeonly
 */
char const * const known_tex_extensions[] = {"tex", 0};

/// packages that work only in xetex
/// polyglossia is handled separately
const char * const known_xetex_packages[] = {"arabxetex", "fixlatvian",
"fontbook", "fontwrap", "mathspec", "philokalia", "unisugar",
"xeCJK", "xecolor", "xecyr", "xeindex", "xepersian", "xunicode", 0};

/// packages that are automatically skipped if loaded by LyX
const char * const known_lyx_packages[] = {"amsbsy", "amsmath", "amssymb",
"amstext", "amsthm", "array", "babel", "booktabs", "calc", "CJK", "cleveref", "color", "colortbl",
"float", "fontspec", "framed", "graphicx", "hhline", "ifthen", "longtable",
"makeidx", "minted", "multirow", "nomencl", "parskip", "pdfpages", "prettyref", "refstyle",
"rotating", "rotfloat", "splitidx", "setspace", "subscript", "tabularx","textcomp", "tipa",
"tipx", "tone", "ulem", "url", "varioref", "verbatim", "wrapfig", "xcolor", "xltabular",
"xunicode", "zref-clever", "zref-vario", 0};

// codes used to remove packages that are loaded automatically by LyX.
// Syntax: package_beg_sep<name>package_mid_sep<package loading code>package_end_sep
const char package_beg_sep = '\001';
const char package_mid_sep = '\002';
const char package_end_sep = '\003';


// returns true if at least one of the options in what has been found
bool handle_opt(vector<string> & opts, char const * const * what, string & target)
{
	if (opts.empty())
		return false;

	bool found = false;
	// the last language option is the document language (for babel and LyX)
	// the last size option is the document font size
	vector<string>::iterator it;
	vector<string>::iterator position = opts.begin();
	for (; *what; ++what) {
		it = find(opts.begin(), opts.end(), *what);
		if (it != opts.end()) {
			if (it >= position) {
				found = true;
				target = *what;
				position = it;
			}
		}
	}
	return found;
}


void delete_opt(vector<string> & opts, char const * const * what)
{
	if (opts.empty())
		return;

	// remove found options from the list
	// do this after handle_opt to avoid potential memory leaks
	vector<string>::iterator it;
	for (; *what; ++what) {
		it = find(opts.begin(), opts.end(), *what);
		if (it != opts.end())
			opts.erase(it);
	}
}


/*!
 * Split a package options string (keyval format) into a vector.
 * Example input:
 *   authorformat=smallcaps,
 *   commabeforerest,
 *   titleformat=colonsep,
 *   bibformat={tabular,ibidem,numbered}
 */
vector<string> split_options(string const & input)
{
	vector<string> options;
	string option;
	Parser p(input);
	while (p.good()) {
		Token const & t = p.get_token();
		if (t.asInput() == ",") {
			options.push_back(trimSpaceAndEol(option));
			option.erase();
		} else if (t.asInput() == "=") {
			option += '=';
			p.skip_spaces(true);
			if (p.next_token().asInput() == "{")
				option += '{' + p.getArg('{', '}') + '}';
		} else if (t.cat() != catSpace && t.cat() != catComment)
			option += t.asInput();
	}

	if (!option.empty())
		options.push_back(trimSpaceAndEol(option));

	return options;
}


/*!
 * Retrieve a keyval option "name={value with=sign}" named \p name from
 * \p options and return the value.
 * The found option is also removed from \p options.
 */
string process_keyval_opt(vector<string> & options, string const & name)
{
	for (size_t i = 0; i < options.size(); ++i) {
		vector<string> option;
		split(options[i], option, '=');
		if (option.size() < 2)
			continue;
		if (option[0] == name) {
			options.erase(options.begin() + i);
			option.erase(option.begin());
			return join(option, "=");
		}
	}
	return "";
}


string const tripleToString(double const a, double const b, double const c)
{
	return convert<string>(a) + ", " + convert<string>(b) + ", " + convert<string>(c);
}


string convert_color_value(string const & model, string const & value) 
{
	// we attempt to convert color values to rgb
	// for the formulae, cf. the xcolor manual
	if (model == "rgb")
		// already have it
		return value;
	vector<string> invals = getVectorFromString(value);
	if (model == "RGB") {
		if (invals.size() != 3 || !isStrInt(invals[0])
		    || !isStrInt(invals[1]) || !isStrInt(invals[2])) {
			LYXERR0("Ignoring wrong RGB color value: " << value);
			return string();
		}
		// r = R / 255
		// g = G / 255
		// b = B / 255
		double const r = convert<double>(invals[0]) / 255.0;
		double const g = convert<double>(invals[1]) / 255.0;
		double const b =  convert<double>(invals[2]) / 255.0;
		return tripleToString(r, g, b);
	}
	if (model == "cmyk") {
		if (invals.size() != 4 || !isStrDbl(invals[0]) || !isStrDbl(invals[1])
		    || !isStrDbl(invals[2]) || !isStrDbl(invals[3])) {
			LYXERR0("Ignoring wrong cmyk color value: " << value);
			return string();
		}
		// r = (1 - c) * (1 - k)
		// g = (1 - m) * (1 - k)
		// b = (1 - y) * (1 - k)
		double const c = convert<double>(invals[0]);
		double const m = convert<double>(invals[1]);
		double const y = convert<double>(invals[2]);
		double const k = convert<double>(invals[3]);
		double const r = (1.0-c) * (1.0-k);
		double const g = (1.0-m) * (1.0-k);
		double const b = (1.0-y) * (1.0-k);
		return tripleToString(r, g, b);
	}
	if (model == "cmy") {
		if (invals.size() != 3 || !isStrDbl(invals[0]) || !isStrDbl(invals[1])
		    || !isStrDbl(invals[2])) {
			LYXERR0("Ignoring wrong cmy color value: " << value);
			return string();
		}
		// r = (1 - c)
		// g = (1 - m)
		// b = (1 - y)
		double const c = convert<double>(invals[0]);
		double const m = convert<double>(invals[1]);
		double const y = convert<double>(invals[2]);
		return tripleToString(1 - c, 1 - m, 1 - y);
	}
	if (model == "gray") {
		// we only have a single value, which is
		// always r = g = b
		if (!isStrDbl(value)) {
			LYXERR0("Ignoring wrong gray color value: " << value);
			return string();
		}
		ostringstream os;
		os << value
		   << ", "
		   << value
		   << ", "
		   << value;
		return os.str();
	}
	if (model == "Gray") {
		// r = g = b = (gray / 15)
		if (!isStrDbl(value)) {
			LYXERR0("Ignoring wrong Gray color value: " << value);
			return string();
		}
		double const gv = convert<double>(value);
		double const r = gv / 15.0;
		double const g = gv / 15.0;
		double const b = gv / 15.0;
		return tripleToString(r, g, b);
	}
	if (model == "hsb") {
		// more complex formula
		// see xcolor manual
		if (invals.size() != 3 || !isStrDbl(invals[0]) || !isStrDbl(invals[1])
		    || !isStrDbl(invals[2])) {
			LYXERR0("Ignoring wrong hsb color value: " << value);
			return string();
		}
		double const h = convert<double>(invals[0]);
		double const s = convert<double>(invals[1]);
		double const b = convert<double>(invals[2]);
		int i = (int)(h * 6);
		double f = h * 6 - i;

		double f1 = 0.0, f2 = 0.0, f3 = 0.0;
		switch (i) {
			case 0:
				f1 = 0.0;
				f2 = 1.0 - f;
				f3 = 1.0;
				break;
			case 1:
				f1 = f;
				f2 = 0.0;
				f3 = 1.0;
				break;
			case 2:
				f1 = 1.0;
				f2 = 0.0;
				f3 = 1.0 - f;
				break;
			case 3:
				f1 = 1.0;
				f2 = f;
				f3 = 0.0;
				break;
			case 4:
				f1 = 1.0 - f;
				f2 = 1.0;
				f3 = 0.0;
				break;
			case 5:
				f1 = 0.0;
				f2 = 1.0;
				f3 = f;
				break;
			case 6:
				f1 = 0.0;
				f2 = 1.0;
				f3 = 1.0;
				break;
			default:
				LYXERR0("Something went wrong when converting from hsb to rgb. Input was: " << value);
				return string();
		}
		return tripleToString((b * (h - s * f1)), (b * (s - s * f2)), (b * (b - s * f3)));
	}
	if (model == "HSB") {
		// same as hsb * 240
		if (invals.size() != 3 || !isStrInt(invals[0]) || !isStrInt(invals[1])
		    || !isStrInt(invals[2])) {
			LYXERR0("Ignoring wrong HSB color value: " << value);
			return string();
		}
		double const h = convert<double>(invals[0]) / 240;
		double const s = convert<double>(invals[1]) / 240;
		double const b = convert<double>(invals[2]) / 240;
		int i = (int)(h * 6);
		double f = h * 6 - i;

		double f1 = 0.0, f2 = 0.0, f3 = 0.0;
		switch (i) {
			case 0:
				f1 = 0.0;
				f2 = 1.0 - f;
				f3 = 1.0;
				break;
			case 1:
				f1 = f;
				f2 = 0.0;
				f3 = 1.0;
				break;
			case 2:
				f1 = 1.0;
				f2 = 0.0;
				f3 = 1.0 - f;
				break;
			case 3:
				f1 = 1.0;
				f2 = f;
				f3 = 0.0;
				break;
			case 4:
				f1 = 1.0 - f;
				f2 = 1.0;
				f3 = 0.0;
				break;
			case 5:
				f1 = 0.0;
				f2 = 1.0;
				f3 = f;
				break;
			case 6:
				f1 = 0.0;
				f2 = 1.0;
				f3 = 1.0;
				break;
			default:
				LYXERR0("Something went wrong when converting from HSB to rgb. Input was: " << value);
				return string();
		}
		return tripleToString((b * (h - s * f1)), (b * (s - s * f2)), (b * (b - s * f3)));
	}
	// TODO: Hsb, tHsb, wave
	return string();
}

} // anonymous namespace


/**
 * known polyglossia language names (including variants)
 * FIXME: support spelling=old for german variants (german vs. ngerman LyX names etc)
 */
const char * const Preamble::polyglossia_languages[] = {
"albanian", "american", "amharic", "ancient", "arabic", "armenian", "asturian", "australian",
"bahasai", "bahasam", "basque", "bengali", "brazil", "brazilian", "breton", "british", "bulgarian",
"catalan", "chinese", "chinese", "churchslavonic", "coptic", "croatian", "czech", "danish", "divehi", "dutch",
"english", "esperanto", "estonian", "farsi", "finnish", "french", "friulan",
"galician", "greek", "monotonic", "hebrew", "hindi",
"icelandic", "interlingua", "irish", "italian", "japanese", "kannada", "khmer", "korean",
"kurdish", "kurmanji", "lao", "latin", "latvian", "lithuanian", "lsorbian", "magyar", "malayalam", "marathi",
"austrian", "newzealand", "german", "nko", "norsk", "nynorsk", "occitan", "odia", "oldrussian",
"piedmontese", "polish", "polytonic", "portuguese", "punjabi", "romanian", "romansh", "russian",
"samin", "sanskrit", "scottish", "serbian", "slovak", "slovenian", "spanish", "swedish", "syriac",
"tamil", "telugu", "thai", "tibetan", "turkish", "turkmen",
"ukrainian", "urdu", "usorbian", "uyghur", "vietnamese", "welsh", 0};

/**
 * the same as polyglossia_languages with .lyx names
 * please keep this in sync with polyglossia_languages line by line!
 */
const char * const Preamble::coded_polyglossia_languages[] = {
"albanian", "american", "amharic", "ancientgreek", "arabic_arabi", "armenian", "asturian", "australian",
"bahasa", "bahasam", "basque", "bengali", "brazilian", "brazilian", "breton", "british", "bulgarian",
"catalan", "chinese-simplified", "chinese-traditional", "churchslavonic", "coptic", "croatian", "czech",
"danish", "divehi", "dutch",
"english", "esperanto", "estonian", "farsi", "finnish", "french", "friulan",
"galician", "greek", "greek", "hebrew", "hindi",
"icelandic", "interlingua", "irish", "italian", "japanese", "kannada", "khmer", "korean",
"sorani", "kurmanji", "lao", "latin", "latvian", "lithuanian", "lowersorbian", "magyar", "malayalam", "marathi",
"naustrian","newzealand", "ngerman", "nko", "norsk", "nynorsk", "occitan", "odia", "oldrussian",
"piedmontese", "polish", "polutonikogreek", "portuges", "punjabi", "romanian", "romansh", "russian",
"samin", "sanskrit", "scottish", "serbian", "slovak", "slovene", "spanish", "swedish", "syriac",
"tamil", "telugu", "thai", "tibetan", "turkish", "turkmen",
"ukrainian", "urdu", "uppersorbian", "uyghur", "vietnamese", "welsh", 0};


bool Preamble::usePolyglossia() const
{
	return h_use_non_tex_fonts && h_language_package == "default";
}


bool Preamble::indentParagraphs() const
{
	return h_paragraph_separation == "indent";
}


bool Preamble::isPackageUsed(string const & package) const
{
	return used_packages.find(package) != used_packages.end();
}


bool Preamble::isPackageAutoLoaded(string const & package) const
{
	return auto_packages.find(package) != auto_packages.end();
}


vector<string> Preamble::getPackageOptions(string const & package) const
{
	map<string, vector<string> >::const_iterator it = used_packages.find(package);
	if (it != used_packages.end())
		return it->second;
	return vector<string>();
}


void Preamble::registerAutomaticallyLoadedPackage(std::string const & package)
{
	auto_packages.insert(package);
}


void Preamble::addModule(string const & module)
{
	for (auto const & m : used_modules) {
		if (m == module)
			return;
	}
	used_modules.push_back(module);
}


void Preamble::suppressDate(bool suppress)
{
	if (suppress)
		h_suppress_date = "true";
	else
		h_suppress_date = "false";
}


void Preamble::registerAuthor(std::string const & name, string const & initials)
{
	Author author(from_utf8(name), empty_docstring(), from_utf8(initials));
	author.setUsed(true);
	authors_.record(author);
	h_tracking_changes = "true";
	h_output_changes = "true";
}


Author const & Preamble::getAuthor(std::string const & name) const
{
	Author author(from_utf8(name), empty_docstring(), empty_docstring());
	for (AuthorList::Authors::const_iterator it = authors_.begin();
	     it != authors_.end(); ++it)
		if (*it == author)
			return *it;
	static Author const dummy;
	return dummy;
}


int Preamble::getSpecialTableColumnArguments(char c) const
{
	map<char, int>::const_iterator it = special_columns_.find(c);
	if (it == special_columns_.end())
		return -1;
	return it->second;
}


void Preamble::add_package(string const & name, vector<string> & options)
{
	// every package inherits the global options
	used_packages.insert({name, split_options(h_options)});

	// Insert options passed via PassOptionsToPackage
	for (auto const & p : extra_package_options_) {
		if (p.first == name) {
			vector<string> eo = getVectorFromString(p.second);
			for (auto const & eoi : eo)
				options.push_back(eoi);
		}

	}
	vector<string> & v = used_packages[name];
	v.insert(v.end(), options.begin(), options.end());
	if (name == "jurabib") {
		// Don't output the order argument (see the cite command
		// handling code in text.cpp).
		vector<string>::iterator end =
			remove(options.begin(), options.end(), "natbiborder");
		end = remove(options.begin(), end, "jurabiborder");
		options.erase(end, options.end());
	}
}

void Preamble::setTextClass(string const & tclass, TeX2LyXDocClass & tc)
{
	h_textclass = tclass;
	tc.setName(h_textclass);
	if (!LayoutFileList::get().haveClass(h_textclass) || !tc.load()) {
		error_message("Could not read layout file for textclass \"" + h_textclass + "\".");
		exit(EXIT_FAILURE);
	}
}


string Preamble::getLyXColor(string const & col, bool const reg)
{
	// Is it a base color (color package)?
	if (is_known(col, known_basic_colors)) {
		if (reg)
			registerAutomaticallyLoadedPackage("color");
		return col;
	}
	// Or one of the additional basic xcolor colors?
	if (is_known(col, known_basic_xcolors)) {
		if (reg)
			registerAutomaticallyLoadedPackage("xcolor");
		return col;
	}
	// No? So try the other xcolor types
	char const * const * where = 0;
	// svgnames colors get priority with clashing names
	if (svgnames() && (where = is_known(col, known_svgnames_textcolors))) {
		if (reg)
			registerAutomaticallyLoadedPackage("xcolor");
		return known_coded_svgnames_textcolors[where - known_svgnames_textcolors];
	}
	// Consider clashing names as well
	if (svgnames() && prefixIs(col, "DVIPS") && (where = is_known(col.substr(5), known_textcolors))) {
		string clashname = known_coded_textcolors[where - known_textcolors];
		if (prefixIs(clashname, "dvips:")) {
			if (reg)
				registerAutomaticallyLoadedPackage("xcolor");
			return clashname;
		}
		return string();
	}
	// check the other xcolor types (dvipsnames, x11names)
	if ((where = is_known(col, known_textcolors))) {
		if (reg)
			registerAutomaticallyLoadedPackage("xcolor");
		return known_coded_textcolors[where - known_textcolors];
	}
	// If none of the above is true, check if it is a known custom color
	if (h_custom_colors.find(col) != h_custom_colors.end())
		return col;

	// Nothing known, return empty string (will cause ERT to be used)
	return string();
}


namespace {

// Given is a string like "scaled=0.9" or "scale=0.9", return 0.9 * 100
bool scale_as_percentage(string const & scale, string & percentage)
{
	if (contains(scale, '=')) {
		string const value = support::split(scale, '=');
		if (isStrDbl(value)) {
			percentage = convert<string>(
				static_cast<int>(100 * convert<double>(value)));
			return true;
		}
	}
	return false;
}


string remove_braces(string const & value)
{
	if (value.empty())
		return value;
	if (value[0] == '{' && value[value.length()-1] == '}')
		return value.substr(1, value.length()-2);
	return value;
}

} // anonymous namespace


Preamble::Preamble() : one_language(true), explicit_babel(false),
	title_layout_found(false), index_number(0), h_font_cjk_set(false)
{
	//h_backgroundcolor;
	//h_boxbgcolor;
	h_biblio_style            = "plain";
	h_bibtex_command          = "default";
	h_cite_engine             = "basic";
	h_cite_engine_type        = "default";
	h_color                   = "#008000";
	h_defskip                 = "medskip";
	h_dynamic_quotes          = false;
	//h_float_placement;
	//h_fontcolor;
	h_fontencoding            = "default";
	h_font_roman[0]           = "default";
	h_font_roman[1]           = "default";
	h_font_sans[0]            = "default";
	h_font_sans[1]            = "default";
	h_font_typewriter[0]      = "default";
	h_font_typewriter[1]      = "default";
	h_font_math[0]            = "auto";
	h_font_math[1]            = "auto";
	h_font_default_family     = "default";
	h_use_non_tex_fonts       = false;
	h_font_sc                 = "false";
	h_font_roman_osf          = "false";
	h_font_sans_osf           = "false";
	h_font_typewriter_osf     = "false";
	h_font_sf_scale[0]        = "100";
	h_font_sf_scale[1]        = "100";
	h_font_tt_scale[0]        = "100";
	h_font_tt_scale[1]        = "100";
	// h_font_roman_opts;
	// h_font_sans_opts;
	// h_font_typewriter_opts;
	// h_font_cjk
	h_is_mathindent           = "0";
	h_math_numbering_side     = "default";
	h_graphics                = "default";
	h_default_output_format   = "default";
	h_html_be_strict          = "false";
	h_html_css_as_file        = "0";
	h_html_math_output        = "0";
	h_docbook_table_output    = "0";
	h_docbook_mathml_prefix   = "1";
	h_docbook_mathml_version  = "0";
	h_index[0]                = "Index";
	h_index_command           = "default";
	h_inputencoding           = "auto-legacy";
	h_justification           = "true";
	h_language                = "english";
	h_language_package        = "none";
	//h_listings_params;
	h_maintain_unincluded_children = "no";
	//h_margins;
	//h_notefontcolor;
	//h_options;
	h_output_changes          = "false";
	h_change_bars             = "false";
	h_output_sync             = "0";
	//h_output_sync_macro
	h_papercolumns            = "1";
	h_paperfontsize           = "default";
	h_paperorientation        = "portrait";
	h_paperpagestyle          = "default";
	//h_papersides;
	h_papersize               = "default";
	h_paragraph_indentation   = "default";
	h_paragraph_separation    = "indent";
	//h_pdf_title;
	//h_pdf_author;
	//h_pdf_subject;
	//h_pdf_keywords;
	h_pdf_bookmarks           = "0";
	h_pdf_bookmarksnumbered   = "0";
	h_pdf_bookmarksopen       = "0";
	h_pdf_bookmarksopenlevel  = "1";
	h_pdf_breaklinks          = "0";
	h_pdf_pdfborder           = "0";
	h_pdf_colorlinks          = "0";
	h_pdf_backref             = "section";
	h_pdf_pdfusetitle         = "0";
	//h_pdf_pagemode;
	//h_pdf_quoted_options;
	h_quotes_style         = "english";
	h_secnumdepth             = "3";
	h_shortcut[0]             = "idx";
	h_spacing                 = "single";
	h_save_transient_properties = "true";
	h_suppress_date           = "false";
	h_table_alt_row_colors_start = 0;
	h_textclass               = "article";
	h_tocdepth                = "3";
	h_tracking_changes        = "false";
	h_use_bibtopic            = "false";
	h_use_dash_ligatures      = "true";
	h_use_indices             = "false";
	h_use_geometry            = "false";
	h_use_default_options     = "false";
	h_use_hyperref            = "false";
	h_use_microtype           = "false";
	h_use_lineno              = "false";
	h_crossref_package        = "prettyref";
	h_use_minted              = false;
	h_use_xcolor_svgnames     = false;
	h_use_packages["amsmath"]    = "1";
	h_use_packages["amssymb"]    = "0";
	h_use_packages["cancel"]     = "0";
	h_use_packages["esint"]      = "1";
	h_use_packages["mhchem"]     = "0";
	h_use_packages["mathdots"]   = "0";
	h_use_packages["mathtools"]  = "0";
	h_use_packages["stackrel"]   = "0";
	h_use_packages["stmaryrd"]   = "0";
	h_use_packages["undertilde"] = "0";
}


void Preamble::handle_hyperref(vector<string> & options)
{
	// FIXME swallow inputencoding changes that might surround the
	//       hyperref setup if it was written by LyX
	h_use_hyperref = "true";
	// swallow "unicode=true", since LyX does always write that
	vector<string>::iterator it =
		find(options.begin(), options.end(), "unicode=true");
	if (it != options.end())
		options.erase(it);
	it = find(options.begin(), options.end(), "pdfusetitle");
	if (it != options.end()) {
		h_pdf_pdfusetitle = "1";
		options.erase(it);
	}
	string bookmarks = process_keyval_opt(options, "bookmarks");
	if (bookmarks == "true")
		h_pdf_bookmarks = "1";
	else if (bookmarks == "false")
		h_pdf_bookmarks = "0";
	if (h_pdf_bookmarks == "1") {
		string bookmarksnumbered =
			process_keyval_opt(options, "bookmarksnumbered");
		if (bookmarksnumbered == "true")
			h_pdf_bookmarksnumbered = "1";
		else if (bookmarksnumbered == "false")
			h_pdf_bookmarksnumbered = "0";
		string bookmarksopen =
			process_keyval_opt(options, "bookmarksopen");
		if (bookmarksopen == "true")
			h_pdf_bookmarksopen = "1";
		else if (bookmarksopen == "false")
			h_pdf_bookmarksopen = "0";
		if (h_pdf_bookmarksopen == "1") {
			string bookmarksopenlevel =
				process_keyval_opt(options, "bookmarksopenlevel");
			if (!bookmarksopenlevel.empty())
				h_pdf_bookmarksopenlevel = bookmarksopenlevel;
		}
	}
	string breaklinks = process_keyval_opt(options, "breaklinks");
	if (breaklinks == "true")
		h_pdf_breaklinks = "1";
	else if (breaklinks == "false")
		h_pdf_breaklinks = "0";
	string pdfborder = process_keyval_opt(options, "pdfborder");
	if (pdfborder == "{0 0 0}")
		h_pdf_pdfborder = "1";
	else if (pdfborder == "{0 0 1}")
		h_pdf_pdfborder = "0";
	string backref = process_keyval_opt(options, "backref");
	if (!backref.empty())
		h_pdf_backref = backref;
	string colorlinks = process_keyval_opt(options, "colorlinks");
	if (colorlinks == "true")
		h_pdf_colorlinks = "1";
	else if (colorlinks == "false")
		h_pdf_colorlinks = "0";
	string pdfpagemode = process_keyval_opt(options, "pdfpagemode");
	if (!pdfpagemode.empty())
		h_pdf_pagemode = pdfpagemode;
	string pdftitle = process_keyval_opt(options, "pdftitle");
	if (!pdftitle.empty()) {
		h_pdf_title = remove_braces(pdftitle);
	}
	string pdfauthor = process_keyval_opt(options, "pdfauthor");
	if (!pdfauthor.empty()) {
		h_pdf_author = remove_braces(pdfauthor);
	}
	string pdfsubject = process_keyval_opt(options, "pdfsubject");
	if (!pdfsubject.empty())
		h_pdf_subject = remove_braces(pdfsubject);
	string pdfkeywords = process_keyval_opt(options, "pdfkeywords");
	if (!pdfkeywords.empty())
		h_pdf_keywords = remove_braces(pdfkeywords);
	if (!options.empty()) {
		if (!h_pdf_quoted_options.empty())
			h_pdf_quoted_options += ',';
		h_pdf_quoted_options += join(options, ",");
		options.clear();
	}
}


void Preamble::handle_geometry(vector<string> & options)
{
	h_use_geometry = "true";
	vector<string>::iterator it;
	// paper orientation
	if ((it = find(options.begin(), options.end(), "landscape")) != options.end()) {
		h_paperorientation = "landscape";
		options.erase(it);
	}
	// paper size
	// keyval version: "paper=letter" or "paper=letterpaper"
	string paper = process_keyval_opt(options, "paper");
	if (!paper.empty())
		if (suffixIs(paper, "paper"))
			paper = subst(paper, "paper", "");
	// alternative version: "letterpaper"
	handle_opt(options, known_latex_paper_sizes, paper);
	if (suffixIs(paper, "paper"))
		paper = subst(paper, "paper", "");
	delete_opt(options, known_latex_paper_sizes);
	if (!paper.empty())
		h_papersize = paper;
	// page margins
	char const * const * margin = known_paper_margins;
	for (; *margin; ++margin) {
		string value = process_keyval_opt(options, *margin);
		if (!value.empty()) {
			int k = margin - known_paper_margins;
			string name = known_coded_paper_margins[k];
			h_margins += '\\' + name + ' ' + value + '\n';
		}
	}
}


void Preamble::handle_package(Parser &p, string const & name,
                              string const & opts, bool in_lyx_preamble,
                              bool detectEncoding)
{
	vector<string> options = split_options(opts);
	add_package(name, options);

	if (is_known(name, known_xetex_packages)) {
		xetex = true;
		h_use_non_tex_fonts = true;
		registerAutomaticallyLoadedPackage("fontspec");
		if (h_inputencoding == "auto-legacy")
			p.setEncoding("UTF-8");
	} else if (h_inputencoding == "auto-legacy"
		   && LaTeXPackages::isAvailableAtLeastFrom("LaTeX", 2018, 04))
		// As of LaTeX 2018/04/01, utf8 is the default input encoding
		// So use that if no inputencoding is set
		h_inputencoding = "utf8";

	// vector of all options for easier parsing and
	// skipping
	vector<string> allopts = getVectorFromString(opts);
	// this stores the potential extra options
	string xopts;

	//
	// roman fonts
	//

	// By default, we use the package name as LyX font name,
	// so this only needs to be reset if these names differ
	if (is_known(name, known_roman_font_packages))
		h_font_roman[0] = name;

	if (name == "ccfonts") {
		for (auto const & opt : allopts) {
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "lmodern") {
		for (auto const & opt : allopts) {
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "fourier") {
		h_font_roman[0] = "utopia";
		for (auto const & opt : allopts) {
			if (opt == "osf") {
				h_font_roman_osf = "true";
				continue;
			}
			if (opt == "expert") {
				h_font_sc = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "garamondx") {
		for (auto const & opt : allopts) {
			if (opt == "osfI") {
				h_font_roman_osf = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "libertine") {
		// this automatically invokes biolinum
		h_font_sans[0] = "biolinum";
		// as well as libertineMono
		h_font_typewriter[0] = "libertine-mono";
		for (auto const & opt : allopts) {
			if (opt == "osf") {
				h_font_roman_osf = "true";
				continue;
			}
			if (opt == "lining") {
				h_font_roman_osf = "false";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "libertineRoman" || name == "libertine-type1") {
		h_font_roman[0] = "libertine";
		// NOTE: contrary to libertine.sty, libertineRoman
		// and libertine-type1 do not automatically invoke
		// biolinum and libertineMono
		if (opts == "lining")
			h_font_roman_osf = "false";
		else if (opts == "osf")
			h_font_roman_osf = "true";
	}

	if (name == "libertinus" || name == "libertinus-type1") {
		bool sf = true;
		bool tt = true;
		bool rm = true;
		bool osf = false;
		string scalesf;
		string scalett;
		for (auto const & opt : allopts) {
			if (opt == "rm" || opt == "serif") {
				tt = false;
				sf = false;
				continue;
			}
			if (opt == "sf" || opt == "sans") {
				tt = false;
				rm = false;
				continue;
			}
			if (opt == "tt=false" || opt == "mono=false") {
				tt = false;
				continue;
			}
			if (opt == "osf") {
				osf = true;
				continue;
			}
			if (opt == "scaleSF") {
				scalesf = opt;
				continue;
			}
			if (opt == "scaleTT") {
				scalett = opt;
				continue;
			}
			if (opt == "lining") {
				h_font_roman_osf = "false";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (rm) {
			h_font_roman[0] = "libertinus";
			if (osf)
				h_font_roman_osf = "true";
			else
				h_font_roman_osf = "false";
		}
		if (sf) {
			h_font_sans[0] = "LibertinusSans-LF";
			if (osf)
				h_font_sans_osf = "true";
			else
				h_font_sans_osf = "false";
			if (!scalesf.empty())
				scale_as_percentage(scalesf, h_font_sf_scale[0]);
		}
		if (tt) {
			h_font_typewriter[0] = "LibertinusMono-TLF";
			if (!scalett.empty())
				scale_as_percentage(scalett, h_font_tt_scale[0]);
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "MinionPro") {
		h_font_roman[0] = "minionpro";
		h_font_roman_osf = "true";
		h_font_math[0] = "auto";
		for (auto const & opt : allopts) {
			if (opt == "lf") {
				h_font_roman_osf = "false";
				continue;
			}
			if (opt == "onlytext") {
				h_font_math[0] = "default";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "mathdesign") {
		for (auto const & opt : allopts) {
			if (opt == "charter") {
				h_font_roman[0] = "md-charter";
				continue;
			}
			if (opt == "garamond") {
				h_font_roman[0] = "md-garamond";
				continue;
			}
			if (opt == "utopia") {
				h_font_roman[0] = "md-utopia";
				continue;
			}
			if (opt == "expert") {
				h_font_sc = "true";
				h_font_roman_osf = "true";
				continue;
			}
		}
	}

	else if (name == "mathpazo") {
		h_font_roman[0] = "palatino";
		for (auto const & opt : allopts) {
			if (opt == "osf") {
				h_font_roman_osf = "true";
				continue;
			}
			if (opt == "sc") {
				h_font_sc = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	else if (name == "mathptmx") {
		h_font_roman[0] = "times";
		for (auto const & opt : allopts) {
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "crimson")
		h_font_roman[0] = "cochineal";

	if (name == "cochineal") {
		for (auto const & opt : allopts) {
			if (opt == "osf" || opt == "oldstyle") {
				h_font_roman_osf = "true";
				continue;
			}
			if (opt == "proportional" || opt == "p")
				continue;
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "CrimsonPro") {
		h_font_roman_osf = "true";
		for (auto const & opt : allopts) {
			if (opt == "lf" || opt == "lining") {
				h_font_roman_osf = "false";
				continue;
			}
			if (opt == "proportional" || opt == "p")
				continue;
			if (opt == "medium") {
				h_font_roman[0] = "CrimsonProMedium";
				continue;
			}
			if (opt == "extralight") {
				h_font_roman[0] = "CrimsonProExtraLight";
				continue;
			}
			if (opt == "light") {
				h_font_roman[0] = "CrimsonProLight";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}


	if (name == "eco")
		// font uses old-style figure
		h_font_roman_osf = "true";

	if (name == "paratype") {
		// in this case all fonts are ParaType
		h_font_roman[0] = "PTSerif-TLF";
		h_font_sans[0] = "default";
		h_font_typewriter[0] = "default";
	}

	if (name == "PTSerif")
		h_font_roman[0] = "PTSerif-TLF";

	if (name == "XCharter") {
		h_font_roman[0] = "xcharter";
		for (auto const & opt : allopts) {
			if (opt == "osf") {
				h_font_roman_osf = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "plex-serif") {
		h_font_roman[0] = "IBMPlexSerif";
		for (auto const & opt : allopts) {
			if (opt == "thin") {
				h_font_roman[0] = "IBMPlexSerifThin";
				continue;
			}
			if (opt == "extralight") {
				h_font_roman[0] = "IBMPlexSerifExtraLight";
				continue;
			}
			if (opt == "light") {
				h_font_roman[0] = "IBMPlexSerifLight";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	if (name == "noto-serif" || name == "noto") {
		bool rm = false;
		bool rmx = false;
		bool sf = false;
		bool sfx = false;
		bool tt = false;
		bool thin = false;
		bool extralight = false;
		bool light = false;
		bool medium = false;
		bool osf = false;
		string scl;
		if (name == "noto") {
			rm = true;
			sf = true;
			tt = true;
		}
		// Since the options might apply to different shapes,
		// we need to parse all options first and then handle them.
		for (auto const & opt : allopts) {
			if (opt == "regular")
				// skip
				continue;
			if (opt == "rm") {
				rm = true;
				rmx = true;
				sf = sfx;
				tt = false;
				continue;
			}
			if (opt == "thin") {
				thin = true;
				continue;
			}
			if (opt == "extralight") {
				extralight = true;
				continue;
			}
			if (opt == "light") {
				light = true;
				continue;
			}
			if (opt == "medium") {
				medium = true;
				continue;
			}
			if (opt == "sf") {
				sfx = true;
				sf = true;
				rm = rmx;
				tt = false;
				continue;
			}
			if (opt == "nott") {
				tt = false;
				continue;
			}
			if (opt == "osf") {
				osf = true;
				continue;
			}
			if (prefixIs(opt, "scaled=")) {
				scl = opt;
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		options.clear();
		// handle options that might affect different shapes
		if (name == "noto-serif" || rm) {
			if (thin)
				h_font_roman[0] = "NotoSerifThin";
			else if (extralight)
				h_font_roman[0] = "NotoSerifExtralight";
			else if (light)
				h_font_roman[0] = "NotoSerifLight";
			else if (medium)
				h_font_roman[0] = "NotoSerifMedium";
			else
				h_font_roman[0] = "NotoSerifRegular";
			if (osf)
				h_font_roman_osf = "true";
			if (!xopts.empty())
				h_font_roman_opts = xopts;
		}
		if (name == "noto" && sf) {
			if (thin)
				h_font_sans[0] = "NotoSansThin";
			else if (extralight)
				h_font_sans[0] = "NotoSansExtralight";
			else if (light)
				h_font_sans[0] = "NotoSansLight";
			else if (medium)
				h_font_sans[0] = "NotoSansMedium";
			else
				h_font_sans[0] = "NotoSansRegular";
			if (osf)
				h_font_sans_osf = "true";
			if (!scl.empty())
				scale_as_percentage(scl, h_font_sf_scale[0]);
			if (!xopts.empty())
				h_font_sans_opts = xopts;
		}
		if (name == "noto" && tt) {
			h_font_typewriter[0] = "NotoMonoRegular";
			if (osf)
				h_font_typewriter_osf = "true";
			if (!scl.empty())
				scale_as_percentage(scl, h_font_tt_scale[0]);
			if (!xopts.empty())
				h_font_typewriter_opts = xopts;
		}
	}

	if (name == "sourceserifpro") {
		h_font_roman[0] = "ADOBESourceSerifPro";
		for (auto const & opt : allopts) {
			if (opt == "osf") {
				h_font_roman_osf = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_roman_opts = xopts;
		options.clear();
	}

	//
	// sansserif fonts
	//

	// By default, we use the package name as LyX font name,
	// so this only needs to be reset if these names differ.
	// Also, we handle the scaling option here generally.
	if (is_known(name, known_sans_font_packages)) {
		h_font_sans[0] = name;
		if (contains(opts, "scale")) {
			vector<string>::iterator it = allopts.begin();
			for (; it != allopts.end() ; ++it) {
				string const opt = *it;
				if (prefixIs(opt, "scaled=") || prefixIs(opt, "scale=")) {
					if (scale_as_percentage(opt, h_font_sf_scale[0])) {
						allopts.erase(it);
						break;
					}
				}
			}
		}
	}

	if (name == "biolinum" || name == "biolinum-type1") {
		h_font_sans[0] = "biolinum";
		for (auto const & opt : allopts) {
			if (prefixIs(opt, "osf")) {
				h_font_sans_osf = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_sans_opts = xopts;
		options.clear();
	}

	if (name == "cantarell") {
		for (auto const & opt : allopts) {
			if (opt == "defaultsans")
				continue;
			if (prefixIs(opt, "oldstyle")) {
				h_font_sans_osf = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_sans_opts = xopts;
		options.clear();
	}

	if (name == "Chivo") {
		for (auto const & opt : allopts) {
			if (opt == "thin") {
				h_font_roman[0] = "ChivoThin";
				continue;
			}
			if (opt == "light") {
				h_font_roman[0] = "ChivoLight";
				continue;
			}
			if (opt == "regular") {
				h_font_roman[0] = "Chivo";
				continue;
			}
			if (opt == "medium") {
				h_font_roman[0] = "ChivoMedium";
				continue;
			}
			if (prefixIs(opt, "oldstyle")) {
				h_font_sans_osf = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_sans_opts = xopts;
		options.clear();
	}

	if (name == "PTSans")
		h_font_sans[0] = "PTSans-TLF";

	if (name == "classico")
		h_font_sans[0] = "uop";

	if (name == "FiraSans") {
		h_font_sans_osf = "true";
		for (auto const & opt : allopts) {
			if (opt == "book") {
				h_font_sans[0] = "FiraSansBook";
				continue;
			}
			if (opt == "thin") {
				continue;
			}
			if (opt == "extralight") {
				h_font_sans[0] = "FiraSansExtralight";
				continue;
			}
			if (opt == "light") {
				h_font_sans[0] = "FiraSansLight";
				continue;
			}
			if (opt == "ultralight") {
				h_font_sans[0] = "FiraSansUltralight";
				continue;
			}
			if (opt == "thin") {
				h_font_sans[0] = "FiraSansThin";
				continue;
			}
			if (opt == "lf" || opt == "lining") {
				h_font_sans_osf = "false";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_sans_opts = xopts;
		options.clear();
	}

	if (name == "plex-sans") {
		h_font_sans[0] = "IBMPlexSans";
		for (auto const & opt : allopts) {
			if (opt == "condensed") {
				h_font_sans[0] = "IBMPlexSansCondensed";
				continue;
			}
			if (opt == "thin") {
				h_font_sans[0] = "IBMPlexSansThin";
				continue;
			}
			if (opt == "extralight") {
				h_font_sans[0] = "IBMPlexSansExtraLight";
				continue;
			}
			if (opt == "light") {
				h_font_sans[0] = "IBMPlexSansLight";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_sans_opts = xopts;
		options.clear();
	}

	if (name == "noto-sans") {
		h_font_sans[0] = "NotoSansRegular";
		for (auto const & opt : allopts) {
			if (opt == "regular")
				continue;
			if (opt == "medium") {
				h_font_sans[0] = "NotoSansMedium";
				continue;
			}
			if (opt == "thin") {
				h_font_sans[0] = "NotoSansThin";
				continue;
			}
			if (opt == "extralight") {
				h_font_sans[0] = "NotoSansExtralight";
				continue;
			}
			if (opt == "light") {
				h_font_sans[0] = "NotoSansLight";
				continue;
			}
			if (opt == "osf") {
				h_font_sans_osf = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_sans_opts = xopts;
		options.clear();
	}

	if (name == "sourcesanspro") {
		h_font_sans[0] = "ADOBESourceSansPro";
		for (auto const & opt : allopts) {
			if (opt == "osf") {
				h_font_sans_osf = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_sans_opts = xopts;
		options.clear();
	}

	//
	// typewriter fonts
	//

	// By default, we use the package name as LyX font name,
	// so this only needs to be reset if these names differ.
	// Also, we handle the scaling option here generally.
	if (is_known(name, known_typewriter_font_packages)) {
		h_font_typewriter[0] = name;
		if (contains(opts, "scale")) {
			vector<string>::iterator it = allopts.begin();
			for (; it != allopts.end() ; ++it) {
				string const opt = *it;
				if (prefixIs(opt, "scaled=") || prefixIs(opt, "scale=")) {
					if (scale_as_percentage(opt, h_font_tt_scale[0])) {
						allopts.erase(it);
						break;
					}
				}
			}
		}
	}

	if (name == "libertineMono" || name == "libertineMono-type1")
		h_font_typewriter[0] = "libertine-mono";

	if (name == "FiraMono") {
		h_font_typewriter_osf = "true";
		for (auto const & opt : allopts) {
			if (opt == "lf" || opt == "lining") {
				h_font_typewriter_osf = "false";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_typewriter_opts = xopts;
		options.clear();
	}

	if (name == "PTMono")
		h_font_typewriter[0] = "PTMono-TLF";

	if (name == "plex-mono") {
		h_font_typewriter[0] = "IBMPlexMono";
		for (auto const & opt : allopts) {
			if (opt == "thin") {
				h_font_typewriter[0] = "IBMPlexMonoThin";
				continue;
			}
			if (opt == "extralight") {
				h_font_typewriter[0] = "IBMPlexMonoExtraLight";
				continue;
			}
			if (opt == "light") {
				h_font_typewriter[0] = "IBMPlexMonoLight";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_typewriter_opts = xopts;
		options.clear();
	}

	if (name == "noto-mono") {
		h_font_typewriter[0] = "NotoMonoRegular";
		for (auto const & opt : allopts) {
			if (opt == "regular")
				continue;
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_typewriter_opts = xopts;
		options.clear();
	}

	if (name == "sourcecodepro") {
		h_font_typewriter[0] = "ADOBESourceCodePro";
		for (auto const & opt : allopts) {
			if (opt == "osf") {
				h_font_typewriter_osf = "true";
				continue;
			}
			if (!xopts.empty())
				xopts += ", ";
			xopts += opt;
		}
		if (!xopts.empty())
			h_font_typewriter_opts = xopts;
		options.clear();
	}

	//
	// math fonts
	//

	// By default, we use the package name as LyX font name,
	// so this only needs to be reset if these names differ.
	if (is_known(name, known_math_font_packages))
		h_font_math[0] = name;

	if (name == "newtxmath") {
		if (opts.empty())
			h_font_math[0] = "newtxmath";
		else if (opts == "garamondx")
			h_font_math[0] = "garamondx-ntxm";
		else if (opts == "libertine")
			h_font_math[0] = "libertine-ntxm";
		else if (opts == "minion")
			h_font_math[0] = "minion-ntxm";
		else if (opts == "cochineal")
			h_font_math[0] = "cochineal-ntxm";
	}

	if (name == "iwona")
		if (opts == "math")
			h_font_math[0] = "iwona-math";

	if (name == "kurier")
		if (opts == "math")
			h_font_math[0] = "kurier-math";

	// after the detection and handling of special cases, we can remove the
	// fonts, otherwise they would appear in the preamble, see bug #7856
	if (is_known(name, known_roman_font_packages) || is_known(name, known_sans_font_packages)
		||	is_known(name, known_typewriter_font_packages) || is_known(name, known_math_font_packages))
		;
	//"On". See the enum Package in BufferParams.h if you thought that "2" should have been "42"
	else if (name == "amsmath" || name == "amssymb" || name == "cancel" ||
	         name == "esint" || name == "mhchem" || name == "mathdots" ||
	         name == "mathtools" || name == "stackrel" ||
		 name == "stmaryrd" || name == "undertilde") {
		h_use_packages[name] = "2";
		registerAutomaticallyLoadedPackage(name);
	}

	else if (name == "babel") {
		h_language_package = "default";
		// One might think we would have to do nothing if babel is loaded
		// without any options to prevent pollution of the preamble with this
		// babel call in every roundtrip.
		// But the user could have defined babel-specific things afterwards. So
		// we need to keep it in the preamble to prevent cases like bug #7861.
		if (!opts.empty()) {
			// check if more than one option was used - used later for inputenc
			if (options.begin() != options.end() - 1)
				one_language = false;
			// babel takes the last language of the option of its \usepackage
			// call as document language. If there is no such language option, the
			// last language in the documentclass options is used.
			handle_opt(options, known_languages, h_language);
			// translate the babel name to a LyX name
			h_language = babel2lyx(h_language);
			if (h_language == "japanese") {
				// For Japanese, the encoding isn't indicated in the source
				// file, and there's really not much we can do. We could
				// 1) offer a list of possible encodings to choose from, or
				// 2) determine the encoding of the file by inspecting it.
				// For the time being, we leave the encoding alone so that
				// we don't get iconv errors when making a wrong guess, and
				// we will output a note at the top of the document
				// explaining what to do.
				Encoding const * const enc = encodings.fromIconvName(
					p.getEncoding(), Encoding::japanese, false);
				if (enc)
					h_inputencoding = enc->name();
				is_nonCJKJapanese = true;
				// in this case babel can be removed from the preamble
				registerAutomaticallyLoadedPackage("babel");
			} else {
				// If babel is called with options, LyX puts them by default into the
				// document class options. This works for most languages, except
				// for Latvian, Lithuanian, Mongolian, Turkmen and Vietnamese and
				// perhaps in future others.
				// Therefore keep the babel call as it is as the user might have
				// reasons for it.
				string const babelcall = "\\usepackage[" + opts + "]{babel}\n";
				if (!contains(h_preamble.str(), babelcall))
					h_preamble << babelcall;
			}
			delete_opt(options, known_languages);
		} else {
			if (!contains(h_preamble.str(), "\\usepackage{babel}\n"))
				h_preamble << "\\usepackage{babel}\n";
			explicit_babel = true;
		}
	}

	else if (name == "polyglossia") {
		h_language_package = "default";
		h_default_output_format = "pdf4";
		h_use_non_tex_fonts = true;
		xetex = true;
		registerAutomaticallyLoadedPackage("xunicode");
		if (h_inputencoding == "auto-legacy")
			p.setEncoding("UTF-8");
	}

	else if (name == "CJK") {
		// set the encoding to "auto-legacy" because it might be set to "auto-legacy-plain" by the babel handling
		// and this would not be correct for CJK
		if (h_inputencoding == "auto-legacy-plain")
			h_inputencoding = "auto-legacy";
		registerAutomaticallyLoadedPackage("CJK");
	}

	else if (name == "CJKutf8") {
		h_inputencoding = "utf8-cjk";
		p.setEncoding("UTF-8");
		registerAutomaticallyLoadedPackage("CJKutf8");
	}

	else if (name == "fontenc") {
		h_fontencoding = getStringFromVector(options, ",");
		options.clear();
	}

	else if (name == "inputenc" || name == "luainputenc") {
		// h_inputencoding is only set when there is not more than one
		// inputenc option because otherwise h_inputencoding must be
		// set to "auto-legacy" (the default encodings of the document's languages)
		// Therefore check that exactly one option is passed to inputenc.
		// It is also only set when there is not more than one babel
		// language option.
		if (!options.empty()) {
			string const encoding = options.back();
			Encoding const * const enc = encodings.fromLaTeXName(
				encoding, Encoding::inputenc, true);
			if (!enc) {
				if (!detectEncoding)
					warning_message("Unknown encoding " + encoding + ". Ignoring.");
			} else {
				if (!enc->unsafe() && options.size() == 1 && one_language == true) {
					h_inputencoding = enc->name();
					docencoding = enc->iconvName();
				}
				p.setEncoding(enc->iconvName());
			}
			options.clear();
		}
	}

	else if (name == "srcltx") {
		h_output_sync = "1";
		if (!opts.empty()) {
			h_output_sync_macro = "\\usepackage[" + opts + "]{srcltx}";
			options.clear();
		} else
			h_output_sync_macro = "\\usepackage{srcltx}";
	}

	else if (is_known(name, known_old_language_packages)) {
		// known language packages from the times before babel
		// if they are found and not also babel, they will be used as
		// custom language package
		h_language_package = "\\usepackage{" + name + "}";
	}

	else if (name == "lyxskak") {
		// ignore this and its options
		const char * const o[] = {"ps", "mover", 0};
		delete_opt(options, o);
	}

	else if (name == "parskip" && options.size() < 2 && (opts.empty() || prefixIs(opts, "skip="))) {
		if (opts.empty())
			h_paragraph_separation = "halfline";
		else {
			if (opts == "skip=\\smallskipamount")
				h_defskip = "smallskip";
			else if (opts == "skip=\\medskipamount")
				h_defskip = "medskip";
			else if (opts == "skip=\\bigskipamount")
				h_defskip = "bigskip";
			else if (opts == "skip=\\baselineskip")
				h_defskip = "fullline";
			else {
				// get value, unbraced
				string length = rtrim(ltrim(token(opts, '=', 1), "{"), "}");
				// transform glue length if we have one
				is_glue_length(length);
				h_defskip = length;
			}
			h_paragraph_separation = "skip";
		}
	}

	else if (is_known(name, known_lyx_packages) && options.empty()) {
		if (name == "splitidx")
			h_use_indices = "true";
		else if (name == "minted")
			h_use_minted = true;
		else if (name == "refstyle"
			 || name == "prettyref"
			 || name == "cleveref")
			h_crossref_package = name;
		else if (name == "zref-clever"
			 || name == "zref-vario")
			h_crossref_package = "zref";

		if (!in_lyx_preamble) {
			h_preamble << package_beg_sep << name
			           << package_mid_sep << "\\usepackage{"
			           << name << '}';
			if (p.next_token().cat() == catNewline ||
			    (p.next_token().cat() == catSpace &&
			     p.next_next_token().cat() == catNewline))
				h_preamble << '\n';
			h_preamble << package_end_sep;
		}
	}

	else if (name == "nomencl") {
		h_nomencl_options = join(options, ",");
		options.clear();
	}

	else if (name == "geometry")
		handle_geometry(options);

	else if (name == "subfig")
		; // ignore this FIXME: Use the package separator mechanism instead

	else if (char const * const * where = is_known(name, known_languages))
		h_language = known_coded_languages[where - known_languages];

	else if (name == "natbib") {
		h_biblio_style = "plainnat";
		h_cite_engine = "natbib";
		h_cite_engine_type = "authoryear";
		vector<string>::iterator it =
			find(options.begin(), options.end(), "authoryear");
		if (it != options.end())
			options.erase(it);
		else {
			it = find(options.begin(), options.end(), "numbers");
			if (it != options.end()) {
				h_cite_engine_type = "numerical";
				options.erase(it);
			}
		}
		if (!options.empty())
			h_biblio_options = join(options, ",");
	}

	else if (name == "biblatex" || name == "biblatex-chicago") {
		bool const chicago = name == "biblatex-chicago";
		h_biblio_style = "plainnat";
		h_cite_engine = "biblatex";
		h_cite_engine_type = "authoryear";
		string opt;
		if (chicago) {
			h_cite_engine = "biblatex-chicago";
			vector<string>::iterator it =
				find(options.begin(), options.end(), "authordate");
			if (it == options.end())
				h_cite_engine_type = "notes";
		} else {
			vector<string>::iterator it =
				find(options.begin(), options.end(), "natbib");
			if (it != options.end()) {
				options.erase(it);
				h_cite_engine = "biblatex-natbib";
			} else {
				opt = process_keyval_opt(options, "natbib");
				if (opt == "true")
					h_cite_engine = "biblatex-natbib";
			}
		}
		opt = process_keyval_opt(options, "style");
		if (!opt.empty()) {
			h_biblatex_citestyle = opt;
			h_biblatex_bibstyle = opt;
		} else {
			opt = process_keyval_opt(options, "citestyle");
			if (!opt.empty())
				h_biblatex_citestyle = opt;
			opt = process_keyval_opt(options, "bibstyle");
			if (!opt.empty())
				h_biblatex_bibstyle = opt;
		}
		opt = process_keyval_opt(options, "refsection");
		if (!opt.empty()) {
			if (opt == "none" || opt == "part"
			    || opt == "chapter" || opt == "section"
			    || opt == "subsection")
				h_multibib = opt;
			else
				warning_message("Ignoring unknown refsection value '" + opt + "'.");
		}
		opt = process_keyval_opt(options, "bibencoding");
		if (!opt.empty())
			bibencoding = opt;
		if (!options.empty()) {
			h_biblio_options = join(options, ",");
			options.clear();
		}
	}

	else if (name == "jurabib") {
		h_biblio_style = "jurabib";
		h_cite_engine = "jurabib";
		h_cite_engine_type = "authoryear";
		if (!options.empty())
			h_biblio_options = join(options, ",");
	}

	else if (name == "bibtopic")
		h_use_bibtopic = "true";

	else if (name == "chapterbib")
		h_multibib = "child";

	else if (name == "hyperref")
		handle_hyperref(options);

	else if (name == "algorithm2e") {
		// Load "algorithm2e" module
		addModule("algorithm2e");
		// Add the package options to the global document options
		if (!options.empty()) {
			if (h_options.empty())
				h_options = join(options, ",");
			else
				h_options += ',' + join(options, ",");
		}
	}
	else if (name == "microtype") {
		//we internally support only microtype without params
		if (options.empty())
			h_use_microtype = "true";
		else
			h_preamble << "\\usepackage[" << opts << "]{microtype}";
	}

	else if (name == "lineno") {
		h_use_lineno = "true";
		if (!options.empty()) {
			h_lineno_options = join(options, ",");
			options.clear();
		}
	}

	else if (name == "changebar")
		h_output_changes = "true";

	else if (name == "xcolor") {
		vector<string>::iterator it =
			find(options.begin(), options.end(), "svgnames");
		if (it != options.end()) {
			h_use_xcolor_svgnames = true;
			options.erase(it);
		}
		it = find(options.begin(), options.end(), "x11names");
		if (it != options.end())
			options.erase(it);
		it = find(options.begin(), options.end(), "dvipsnames");
		if (it != options.end())
			options.erase(it);
	}

	else if (!in_lyx_preamble) {
		if (options.empty())
			h_preamble << "\\usepackage{" << name << '}';
		else {
			h_preamble << "\\usepackage[" << opts << "]{"
				   << name << '}';
			options.clear();
		}
		if (p.next_token().cat() == catNewline ||
		    (p.next_token().cat() == catSpace &&
		     p.next_next_token().cat() == catNewline))
			h_preamble << '\n';
	}

	// We need to do something with the options...
	if (!options.empty() && !detectEncoding)
		warning_message("Ignoring options '" + join(options, ",")
				+ "' of package " + name + '.');

	// remove the whitespace
	p.skip_spaces();
}


void Preamble::handle_if(Parser & p, bool in_lyx_preamble)
{
	while (p.good()) {
		Token t = p.get_token();
		if (t.cat() == catEscape &&
		    is_known(t.cs(), known_if_commands))
			handle_if(p, in_lyx_preamble);
		else {
			if (!in_lyx_preamble)
				h_preamble << t.asInput();
			if (t.cat() == catEscape && t.cs() == "fi")
				return;
		}
	}
}


bool Preamble::writeLyXHeader(ostream & os, bool subdoc, string const & outfiledir)
{
	if (contains(h_float_placement, "H"))
		registerAutomaticallyLoadedPackage("float");
	if (h_spacing != "single" && h_spacing != "default")
		registerAutomaticallyLoadedPackage("setspace");
	if (h_use_packages["amsmath"] == "2") {
		// amsbsy and amstext are already provided by amsmath
		registerAutomaticallyLoadedPackage("amsbsy");
		registerAutomaticallyLoadedPackage("amstext");
	}

	// output the LyX file settings
	// Important: Keep the version formatting in sync with LyX and
	//            lyx2lyx (bug 7951)
	string const origin = roundtripMode() ? "roundtrip" : outfiledir;
	os << "#LyX file created by tex2lyx " << lyx_version_major << '.'
	   << lyx_version_minor << '\n'
	   << "\\lyxformat " << LYX_FORMAT << '\n'
	   << "\\begin_document\n"
	   << "\\begin_header\n"
	   << "\\save_transient_properties " << h_save_transient_properties << "\n"
	   << "\\origin " << origin << "\n"
	   << "\\textclass " << h_textclass << "\n";
	if (!h_doc_metadata.empty()) {
		os << "\\begin_metadata\n"
		   << h_doc_metadata
		   << "\n\\end_metadata\n";
	}
	string const raw = subdoc ? empty_string() : h_preamble.str();
	if (!raw.empty()) {
		os << "\\begin_preamble\n";
		for (string::size_type i = 0; i < raw.size(); ++i) {
			if (raw[i] == package_beg_sep) {
				// Here follows some package loading code that
				// must be skipped if the package is loaded
				// automatically.
				string::size_type j = raw.find(package_mid_sep, i);
				if (j == string::npos)
					return false;
				string::size_type k = raw.find(package_end_sep, j);
				if (k == string::npos)
					return false;
				string const package = raw.substr(i + 1, j - i - 1);
				string const replacement = raw.substr(j + 1, k - j - 1);
				if (auto_packages.find(package) == auto_packages.end())
					os << replacement;
				i = k;
			} else
				os.put(raw[i]);
		}
		os << "\n\\end_preamble\n";
	}
	if (!h_options.empty())
		os << "\\options " << h_options << "\n";
	os << "\\use_default_options " << h_use_default_options << "\n";
	if (!used_modules.empty()) {
		os << "\\begin_modules\n";
		vector<string>::const_iterator const end = used_modules.end();
		vector<string>::const_iterator it = used_modules.begin();
		for (; it != end; ++it)
			os << *it << '\n';
		os << "\\end_modules\n";
	}
	if (!h_includeonlys.empty()) {
		os << "\\begin_includeonly\n";
		for (auto const & iofile : h_includeonlys)
			os << iofile << '\n';
		os << "\\end_includeonly\n";
	}
	os << "\\maintain_unincluded_children " << h_maintain_unincluded_children << "\n"
	   << "\\language " << h_language << "\n"
	   << "\\language_package " << h_language_package << "\n"
	   << "\\inputencoding " << h_inputencoding << "\n"
	   << "\\fontencoding " << h_fontencoding << "\n"
	   << "\\font_roman \"" << h_font_roman[0]
	   << "\" \"" << h_font_roman[1] << "\"\n"
	   << "\\font_sans \"" << h_font_sans[0] << "\" \"" << h_font_sans[1] << "\"\n"
	   << "\\font_typewriter \"" << h_font_typewriter[0]
	   << "\" \"" << h_font_typewriter[1] << "\"\n"
	   << "\\font_math \"" << h_font_math[0] << "\" \"" << h_font_math[1] << "\"\n"
	   << "\\font_default_family " << h_font_default_family << "\n"
	   << "\\use_non_tex_fonts " << (h_use_non_tex_fonts ? "true" : "false") << '\n'
	   << "\\font_sc " << h_font_sc << "\n"
	   << "\\font_roman_osf " << h_font_roman_osf << "\n"
	   << "\\font_sans_osf " << h_font_sans_osf << "\n"
	   << "\\font_typewriter_osf " << h_font_typewriter_osf << "\n";
	if (!h_font_roman_opts.empty())
		os << "\\font_roman_opts \"" << h_font_roman_opts << "\"" << '\n';
	os << "\\font_sf_scale " << h_font_sf_scale[0]
	   << ' ' << h_font_sf_scale[1] << '\n';
	if (!h_font_sans_opts.empty())
		os << "\\font_sans_opts \"" << h_font_sans_opts << "\"" << '\n';
	os << "\\font_tt_scale " << h_font_tt_scale[0]
	   << ' ' << h_font_tt_scale[1] << '\n';
	if (!h_font_cjk.empty())
		os << "\\font_cjk " << h_font_cjk << '\n';
	if (!h_font_typewriter_opts.empty())
		os << "\\font_typewriter_opts \"" << h_font_typewriter_opts << "\"" << '\n';
	os << "\\use_microtype " << h_use_microtype << '\n'
	   << "\\use_dash_ligatures " << h_use_dash_ligatures << '\n'
	   << "\\graphics " << h_graphics << '\n'
	   << "\\default_output_format " << h_default_output_format << "\n"
	   << "\\output_sync " << h_output_sync << "\n";
	if (h_output_sync == "1")
		os << "\\output_sync_macro \"" << h_output_sync_macro << "\"\n";
	os << "\\bibtex_command " << h_bibtex_command << "\n"
	   << "\\index_command " << h_index_command << "\n";
	if (!h_float_placement.empty())
		os << "\\float_placement " << h_float_placement << "\n";
	os << "\\paperfontsize " << h_paperfontsize << "\n"
	   << "\\spacing " << h_spacing << "\n"
	   << "\\use_hyperref " << h_use_hyperref << '\n';
	if (h_use_hyperref == "true") {
		if (!h_pdf_title.empty())
			os << "\\pdf_title " << Lexer::quoteString(h_pdf_title) << '\n';
		if (!h_pdf_author.empty())
			os << "\\pdf_author " << Lexer::quoteString(h_pdf_author) << '\n';
		if (!h_pdf_subject.empty())
			os << "\\pdf_subject " << Lexer::quoteString(h_pdf_subject) << '\n';
		if (!h_pdf_keywords.empty())
			os << "\\pdf_keywords " << Lexer::quoteString(h_pdf_keywords) << '\n';
		os << "\\pdf_bookmarks " << h_pdf_bookmarks << "\n"
		      "\\pdf_bookmarksnumbered " << h_pdf_bookmarksnumbered << "\n"
		      "\\pdf_bookmarksopen " << h_pdf_bookmarksopen << "\n"
		      "\\pdf_bookmarksopenlevel " << h_pdf_bookmarksopenlevel << "\n"
		      "\\pdf_breaklinks " << h_pdf_breaklinks << "\n"
		      "\\pdf_pdfborder " << h_pdf_pdfborder << "\n"
		      "\\pdf_colorlinks " << h_pdf_colorlinks << "\n"
		      "\\pdf_backref " << h_pdf_backref << "\n"
		      "\\pdf_pdfusetitle " << h_pdf_pdfusetitle << '\n';
		if (!h_pdf_pagemode.empty())
			os << "\\pdf_pagemode " << h_pdf_pagemode << '\n';
		if (!h_pdf_quoted_options.empty())
			os << "\\pdf_quoted_options " << Lexer::quoteString(h_pdf_quoted_options) << '\n';
	}
	os << "\\papersize " << h_papersize << "\n"
	   << "\\use_geometry " << h_use_geometry << '\n';
	for (map<string, string>::const_iterator it = h_use_packages.begin();
	     it != h_use_packages.end(); ++it)
		os << "\\use_package " << it->first << ' ' << it->second << '\n';
	os << "\\cite_engine " << h_cite_engine << '\n'
	   << "\\cite_engine_type " << h_cite_engine_type << '\n'
	   << "\\biblio_style " << h_biblio_style << "\n"
	   << "\\use_bibtopic " << h_use_bibtopic << "\n";
	if (!h_biblio_options.empty())
		os << "\\biblio_options " << h_biblio_options << "\n";
	if (!h_biblatex_bibstyle.empty())
		os << "\\biblatex_bibstyle " << h_biblatex_bibstyle << "\n";
	if (!h_biblatex_citestyle.empty())
		os << "\\biblatex_citestyle " << h_biblatex_citestyle << "\n";
	if (!h_multibib.empty())
		os << "\\multibib " << h_multibib << "\n";
	os << "\\use_indices " << h_use_indices << "\n"
	   << "\\paperorientation " << h_paperorientation << '\n'
	   << "\\suppress_date " << h_suppress_date << '\n'
	   << "\\justification " << h_justification << '\n'
	   << "\\crossref_package " << h_crossref_package << '\n'
	   << "\\use_minted " << h_use_minted << '\n'
	   << "\\use_lineno " << h_use_lineno << '\n';
	if (!h_lineno_options.empty())
		os << "\\lineno_options " << h_lineno_options << '\n';
	if (!h_nomencl_options.empty())
		os << "\\nomencl_options " << h_nomencl_options << '\n';
	for (map<string, string>::const_iterator it = h_custom_colors.begin();
	     it != h_custom_colors.end(); ++it)
		os << "\\customcolor " << it->first << ' ' << it->second << '\n';
	if (!h_fontcolor.empty())
		os << "\\fontcolor " << h_fontcolor << '\n';
	if (!h_notefontcolor.empty())
		os << "\\notefontcolor " << h_notefontcolor << '\n';
	if (!h_backgroundcolor.empty())
		os << "\\backgroundcolor " << h_backgroundcolor << '\n';
	if (!h_boxbgcolor.empty())
		os << "\\boxbgcolor " << h_boxbgcolor << '\n';
	if (!h_table_bordercolor.empty())
		os << "\\table_border_color " << h_table_bordercolor << '\n';
	if (!h_table_odd_row_color.empty())
		os << "\\table_odd_row_color " << h_table_odd_row_color << '\n';
	if (!h_table_even_row_color.empty())
		os << "\\table_even_row_color " << h_table_even_row_color << '\n';
	if (h_table_alt_row_colors_start != 0)
		os << "\\table_alt_row_colors_start " << h_table_alt_row_colors_start << '\n';
	if (index_number != 0)
		for (int i = 0; i < index_number; i++) {
			os << "\\index " << h_index[i] << '\n'
			   << "\\shortcut " << h_shortcut[i] << '\n'
			   << "\\color " << h_color << '\n'
			   << "\\end_index\n";
		}
	else {
		os << "\\index " << h_index[0] << '\n'
		   << "\\shortcut " << h_shortcut[0] << '\n'
		   << "\\color " << h_color << '\n'
		   << "\\end_index\n";
	}
	os << h_margins
	   << "\\secnumdepth " << h_secnumdepth << "\n"
	   << "\\tocdepth " << h_tocdepth << "\n"
	   << "\\paragraph_separation " << h_paragraph_separation << "\n";
	if (h_paragraph_separation == "skip")
		os << "\\defskip " << h_defskip << "\n";
	else
		os << "\\paragraph_indentation " << h_paragraph_indentation << "\n";
	os << "\\is_math_indent " << h_is_mathindent << "\n";
	if (!h_mathindentation.empty())
		os << "\\math_indentation " << h_mathindentation << "\n";
	os << "\\math_numbering_side " << h_math_numbering_side << "\n";
	os << "\\quotes_style " << h_quotes_style << "\n"
	   << "\\dynamic_quotes " << h_dynamic_quotes << "\n"
	   << "\\papercolumns " << h_papercolumns << "\n"
	   << "\\papersides " << h_papersides << "\n"
	   << "\\paperpagestyle " << h_paperpagestyle << "\n";
	if (!h_listings_params.empty())
		os << "\\listings_params " << h_listings_params << "\n";
	os << "\\tracking_changes " << h_tracking_changes << "\n"
	   << "\\output_changes " << h_output_changes << "\n"
	   << "\\change_bars " << h_change_bars << "\n"
	   << "\\html_math_output " << h_html_math_output << "\n"
	   << "\\html_css_as_file " << h_html_css_as_file << "\n"
	   << "\\html_be_strict " << h_html_be_strict << "\n"
	   << "\\docbook_table_output " << h_docbook_table_output << "\n"
	   << "\\docbook_mathml_prefix " << h_docbook_mathml_prefix << "\n"
	   << "\\docbook_mathml_version " << h_docbook_mathml_version << "\n"
	   << authors_
	   << "\\end_header\n\n"
	   << "\\begin_body\n";
	return true;
}


void Preamble::parse(Parser & p, string const & forceclass,
                     TeX2LyXDocClass & tc)
{
	// initialize fixed types
	special_columns_['D'] = 3;
	parse(p, forceclass, false, tc);
}


void Preamble::parse(Parser & p, string const & forceclass,
                     bool detectEncoding, TeX2LyXDocClass & tc)
{
	bool is_full_document = false;
	bool is_lyx_file = false;
	bool in_lyx_preamble = false;
	bool class_set = false;

	// determine whether this is a full document or a fragment for inclusion
	while (p.good()) {
		Token const & t = p.get_token();

		if (t.cat() == catEscape && t.cs() == "documentclass") {
			is_full_document = true;
			break;
		}
	}
	p.reset();

	if (detectEncoding && !is_full_document)
		return;

	// Force textclass if the user wanted it
	if (!forceclass.empty()) {
		setTextClass(forceclass, tc);
		class_set = true;
	}

	while (is_full_document && p.good()) {
		if (detectEncoding && h_inputencoding != "auto-legacy" &&
		    h_inputencoding != "auto-legacy-plain")
			return;

		Token const & t = p.get_token();

		if (!detectEncoding)
			debug_message("t: " + t.asInput());

		//
		// cat codes
		//
		if (!in_lyx_preamble &&
		    (t.cat() == catLetter ||
		     t.cat() == catSuper ||
		     t.cat() == catSub ||
		     t.cat() == catOther ||
		     t.cat() == catMath ||
		     t.cat() == catActive ||
		     t.cat() == catBegin ||
		     t.cat() == catEnd ||
		     t.cat() == catAlign ||
		     t.cat() == catParameter)) {
			h_preamble << t.cs();
			continue;
		}

		if (!in_lyx_preamble &&
		    (t.cat() == catSpace || t.cat() == catNewline)) {
			h_preamble << t.asInput();
			continue;
		}

		if (t.cat() == catComment) {
			static regex const islyxfile("%% LyX .* created this file");
			static regex const usercommands("User specified LaTeX commands");

			string const comment = t.asInput();

			// magically switch encoding default if it looks like XeLaTeX
			static string const magicXeLaTeX =
				"% This document must be compiled with XeLaTeX ";
			if (comment.size() > magicXeLaTeX.size()
				  && comment.substr(0, magicXeLaTeX.size()) == magicXeLaTeX
				  && h_inputencoding == "auto-legacy") {
				if (!detectEncoding)
					warning_message("XeLaTeX comment found, switching to UTF8");
				h_inputencoding = "utf8";
			}
			smatch sub;
			if (regex_search(comment, sub, islyxfile)) {
				is_lyx_file = true;
				in_lyx_preamble = true;
			} else if (is_lyx_file
				   && regex_search(comment, sub, usercommands))
				in_lyx_preamble = false;
			else if (!in_lyx_preamble)
				h_preamble << t.asInput();
			continue;
		}

		if (t.cs() == "PassOptionsToPackage") {
			string const poptions = p.getArg('{', '}');
			string const package = p.verbatim_item();
			extra_package_options_.insert(make_pair(package, poptions));
			continue;
		}

		if (t.cs() == "pagestyle") {
			h_paperpagestyle = p.verbatim_item();
			continue;
		}

		if (t.cs() == "setdefaultlanguage") {
			xetex = true;
			// We don't yet care about non-language variant options
			// because LyX doesn't support this yet, see bug #8214
			if (p.hasOpt()) {
				string langopts = p.getOpt();
				// check if the option contains a variant, if yes, extract it
				string::size_type pos_var = langopts.find("variant");
				string::size_type i = langopts.find(',', pos_var);
				string::size_type k = langopts.find('=', pos_var);
				if (pos_var != string::npos){
					string variant;
					if (i == string::npos)
						variant = langopts.substr(k + 1, langopts.length() - k - 2);
					else
						variant = langopts.substr(k + 1, i - k - 1);
					h_language = variant;
				}
				p.verbatim_item();
			} else
				h_language = p.verbatim_item();
			//finally translate the poyglossia name to a LyX name
			h_language = polyglossia2lyx(h_language);
			continue;
		}

		if (t.cs() == "setotherlanguage") {
			// We don't yet care about the option because LyX doesn't
			// support this yet, see bug #8214
			p.hasOpt() ? p.getOpt() : string();
			p.verbatim_item();
			continue;
		}

		if (t.cs() == "setmainfont") {
			string fontopts = p.hasOpt() ? p.getArg('[', ']') : string();
			h_font_roman[1] = p.getArg('{', '}');
			if (!fontopts.empty()) {
				vector<string> opts = getVectorFromString(fontopts);
				fontopts.clear();
				for (auto const & opt : opts) {
					if (opt == "Mapping=tex-text" || opt == "Ligatures=TeX")
						// ignore
						continue;
					if (!fontopts.empty())
						fontopts += ", ";
					fontopts += opt;
				}
				h_font_roman_opts = fontopts;
			}
			continue;
		}

		if (t.cs() == "setsansfont" || t.cs() == "setmonofont") {
			// LyX currently only supports the scale option
			string scale, fontopts;
			if (p.hasOpt()) {
				fontopts = p.getArg('[', ']');
				if (!fontopts.empty()) {
					vector<string> opts = getVectorFromString(fontopts);
					fontopts.clear();
					for (auto const & opt : opts) {
						if (opt == "Mapping=tex-text" || opt == "Ligatures=TeX")
							// ignore
							continue;
						if (prefixIs(opt, "Scale=")) {
							scale_as_percentage(opt, scale);
							continue;
						}
						if (!fontopts.empty())
							fontopts += ", ";
						fontopts += opt;
					}
				}
			}
			if (t.cs() == "setsansfont") {
				if (!scale.empty())
					h_font_sf_scale[1] = scale;
				h_font_sans[1] = p.getArg('{', '}');
				if (!fontopts.empty())
					h_font_sans_opts = fontopts;
			} else {
				if (!scale.empty())
					h_font_tt_scale[1] = scale;
				h_font_typewriter[1] = p.getArg('{', '}');
				if (!fontopts.empty())
					h_font_typewriter_opts = fontopts;
			}
			continue;
		}

		if (t.cs() == "babelfont") {
			xetex = true;
			h_use_non_tex_fonts = true;
			h_language_package = "babel";
			if (h_inputencoding == "auto-legacy")
				p.setEncoding("UTF-8");
			// we don't care about the lang option
			string const lang = p.hasOpt() ? p.getArg('[', ']') : string();
			string const family = p.getArg('{', '}');
			string fontopts = p.hasOpt() ? p.getArg('[', ']') : string();
			string const fontname = p.getArg('{', '}');
			if (lang.empty() && family == "rm") {
				h_font_roman[1] = fontname;
				if (!fontopts.empty()) {
					vector<string> opts = getVectorFromString(fontopts);
					fontopts.clear();
					for (auto const & opt : opts) {
						if (opt == "Mapping=tex-text" || opt == "Ligatures=TeX")
							// ignore
							continue;
						if (!fontopts.empty())
							fontopts += ", ";
						fontopts += opt;
					}
					h_font_roman_opts = fontopts;
				}
 				continue;
			} else if (lang.empty() && (family == "sf" || family == "tt")) {
				string scale;
				if (!fontopts.empty()) {
					vector<string> opts = getVectorFromString(fontopts);
					fontopts.clear();
					for (auto const & opt : opts) {
						if (opt == "Mapping=tex-text" || opt == "Ligatures=TeX")
							// ignore
							continue;
						if (prefixIs(opt, "Scale=")) {
							scale_as_percentage(opt, scale);
							continue;
						}
						if (!fontopts.empty())
							fontopts += ", ";
						fontopts += opt;
					}
				}
				if (family == "sf") {
					if (!scale.empty())
						h_font_sf_scale[1] = scale;
					h_font_sans[1] = fontname;
					if (!fontopts.empty())
						h_font_sans_opts = fontopts;
				} else {
					if (!scale.empty())
						h_font_tt_scale[1] = scale;
					h_font_typewriter[1] = fontname;
					if (!fontopts.empty())
						h_font_typewriter_opts = fontopts;
				}
				continue;
			} else {
				// not rm, sf or tt or lang specific
				h_preamble << '\\' << t.cs();
				if (!lang.empty())
					h_preamble << '[' << lang << ']';
				h_preamble << '{' << family << '}';
				if (!fontopts.empty())
					h_preamble << '[' << fontopts << ']';
				h_preamble << '{' << fontname << '}' << '\n';
				continue;
			}
		}

		if (t.cs() == "date") {
			string argument = p.getArg('{', '}');
			if (argument.empty())
				h_suppress_date = "true";
			else
				h_preamble << t.asInput() << '{' << argument << '}';
			continue;
		}

		if (t.cs() == "color") {
			string const space =
				(p.hasOpt() ? p.getOpt() : string());
			string argument = p.getArg('{', '}');
			if (space.empty())
				h_fontcolor = getLyXColor(argument, true);
			if (h_fontcolor.empty()) {
				h_preamble << t.asInput();
				if (!space.empty())
					h_preamble << space;
				h_preamble << '{' << argument << '}';
			}
			continue;
		}

		if (t.cs() == "pagecolor") {
			string argument = p.getArg('{', '}');
			h_backgroundcolor = getLyXColor(argument, true);
			if (h_backgroundcolor.empty())
				h_preamble << t.asInput() << '{' << argument << '}';
			continue;
		}

		if (t.cs() == "makeatletter") {
			// LyX takes care of this
			p.setCatcode('@', catLetter);
			continue;
		}

		if (t.cs() == "makeatother") {
			// LyX takes care of this
			p.setCatcode('@', catOther);
			continue;
		}

		if (t.cs() == "makeindex") {
			// LyX will re-add this if a print index command is found
			p.skip_spaces();
			continue;
		}

		if (t.cs() == "newindex") {
			string const indexname = p.getArg('[', ']');
			string const shortcut = p.verbatim_item();
			if (!indexname.empty())
				h_index[index_number] = indexname;
			else
				h_index[index_number] = shortcut;
			h_shortcut[index_number] = shortcut;
			index_number += 1;
			p.skip_spaces();
			continue;
		}

		if (t.cs() == "addbibresource") {
			string const options =  p.getArg('[', ']');
			string const arg = removeExtension(p.getArg('{', '}'));
			if (!options.empty()) {
				// check if the option contains a bibencoding, if yes, extract it
				string::size_type pos = options.find("bibencoding=");
				string encoding;
				if (pos != string::npos) {
					string::size_type i = options.find(',', pos);
					if (i == string::npos)
						encoding = options.substr(pos + 1);
					else
						encoding = options.substr(pos, i - pos);
					pos = encoding.find('=');
					if (pos == string::npos)
						encoding.clear();
					else
						encoding = encoding.substr(pos + 1);
				}
				if (!encoding.empty())
					biblatex_encodings.push_back(normalize_filename(arg) + ' ' + encoding);
			}
			biblatex_bibliographies.push_back(arg);
			continue;
		}

		if (t.cs() == "bibliography") {
			vector<string> bibs = getVectorFromString(p.getArg('{', '}'));
			biblatex_bibliographies.insert(biblatex_bibliographies.end(), bibs.begin(), bibs.end());
			continue;
		}

		if (t.cs() == "RS@ifundefined") {
			string const name = p.verbatim_item();
			string const body1 = p.verbatim_item();
			string const body2 = p.verbatim_item();
			// only non-lyxspecific stuff
			if (in_lyx_preamble &&
			    (name == "subsecref" || name == "thmref" || name == "lemref"))
				p.skip_spaces();
			else {
				ostringstream ss;
				ss << '\\' << t.cs();
				ss << '{' << name << '}'
				   << '{' << body1 << '}'
				   << '{' << body2 << '}';
				h_preamble << ss.str();
			}
			continue;
		}

		if (t.cs() == "AtBeginDocument") {
			string const name = p.verbatim_item();
			// only non-lyxspecific stuff
			if (in_lyx_preamble &&
			    (name == "\\providecommand\\partref[1]{\\ref{part:#1}}"
				|| name == "\\providecommand\\chapref[1]{\\ref{chap:#1}}"
				|| name == "\\providecommand\\secref[1]{\\ref{sec:#1}}"
				|| name == "\\providecommand\\subsecref[1]{\\ref{subsec:#1}}"
				|| name == "\\providecommand\\parref[1]{\\ref{par:#1}}"
				|| name == "\\providecommand\\figref[1]{\\ref{fig:#1}}"
				|| name == "\\providecommand\\tabref[1]{\\ref{tab:#1}}"
				|| name == "\\providecommand\\algref[1]{\\ref{alg:#1}}"
				|| name == "\\providecommand\\fnref[1]{\\ref{fn:#1}}"
				|| name == "\\providecommand\\enuref[1]{\\ref{enu:#1}}"
				|| name == "\\providecommand\\eqref[1]{\\ref{eq:#1}}"
				|| name == "\\providecommand\\lemref[1]{\\ref{lem:#1}}"
				|| name == "\\providecommand\\thmref[1]{\\ref{thm:#1}}"
				|| name == "\\providecommand\\corref[1]{\\ref{cor:#1}}"
				|| name == "\\providecommand\\propref[1]{\\ref{prop:#1}}"))
				p.skip_spaces();
			else {
				ostringstream ss;
				ss << '\\' << t.cs();
				ss << '{' << name << '}';
				h_preamble << ss.str();
			}
			continue;
		}

		if (t.cs() == "newcommand" || t.cs() == "newcommandx"
		    || t.cs() == "renewcommand" || t.cs() == "renewcommandx"
		    || t.cs() == "providecommand" || t.cs() == "providecommandx"
		    || t.cs() == "DeclareRobustCommand"
		    || t.cs() == "DeclareRobustCommandx"
		    || t.cs() == "ProvideTextCommandDefault"
		    || t.cs() == "DeclareMathAccent") {
			bool star = false;
			if (p.next_token().character() == '*') {
				p.get_token();
				star = true;
			}
			string const name = p.verbatim_item();
			string const opt1 = p.getFullOpt();
			string const opt2 = p.getFullOpt();
			string const body = p.verbatim_item();
			// store the in_lyx_preamble setting
			bool const was_in_lyx_preamble = in_lyx_preamble;
			// font settings
			if (name == "\\rmdefault")
				if (is_known(body, known_roman_font_packages)) {
					h_font_roman[0] = body;
					p.skip_spaces();
					in_lyx_preamble = true;
				}
			if (name == "\\sfdefault") {
				if (is_known(body, known_sans_font_packages)) {
					h_font_sans[0] = body;
					p.skip_spaces();
					in_lyx_preamble = true;
				}
				if (body == "LibertinusSans-OsF") {
					h_font_sans[0] = "LibertinusSans-LF";
					h_font_sans_osf = "true";
					p.skip_spaces();
					in_lyx_preamble = true;
				}
			}
			if (name == "\\ttdefault")
				if (is_known(body, known_typewriter_font_packages)) {
					h_font_typewriter[0] = body;
					p.skip_spaces();
					in_lyx_preamble = true;
				}
			if (name == "\\familydefault") {
				string family = body;
				// remove leading "\"
				h_font_default_family = family.erase(0,1);
				p.skip_spaces();
				in_lyx_preamble = true;
			}
			if (name == "\\LibertinusSans@scale") {
				if (isStrDbl(body)) {
					h_font_sf_scale[0] = convert<string>(
						static_cast<int>(100 * convert<double>(body)));
				}
			}
			if (name == "\\LibertinusMono@scale") {
				if (isStrDbl(body)) {
					h_font_tt_scale[0] = convert<string>(
						static_cast<int>(100 * convert<double>(body)));
				}
			}

			// remove LyX-specific definitions that are re-added by LyX
			// if necessary
			// \lyxline is an ancient command that is converted by tex2lyx into
			// a \rule therefore remove its preamble code
			if (name == "\\lyxdot" || name == "\\lyxarrow"
			    || name == "\\lyxline" || name == "\\LyX") {
				p.skip_spaces();
				in_lyx_preamble = true;
			}

			// Add the command to the known commands
			add_known_command(name, opt1, !opt2.empty(), from_utf8(body));

			// only non-lyxspecific stuff
			if (!in_lyx_preamble) {
				ostringstream ss;
				ss << '\\' << t.cs();
				if (star)
					ss << '*';
				ss << '{' << name << '}' << opt1 << opt2
				   << '{' << body << "}";
				if (prefixIs(t.cs(), "renew") || !contains(h_preamble.str(), ss.str()))
					h_preamble << ss.str();
/*
				ostream & out = in_preamble ? h_preamble : os;
				out << "\\" << t.cs() << "{" << name << "}"
				    << opts << "{" << body << "}";
*/
			}
			// restore the in_lyx_preamble setting
			in_lyx_preamble = was_in_lyx_preamble;
			continue;
		}

		if (t.cs() == "documentclass") {
			vector<string>::iterator it;
			vector<string> opts = split_options(p.getArg('[', ']'));
			// FIXME This does not work for classes that have a
			//       different name in LyX than in LaTeX
			string tclass = p.getArg('{', '}');
			if (contains(tclass, '/')) {
				// It's considered bad practice, but it is still
				// sometimes done (and possible) to enter the documentclass
				// as a path, e.g. \documentclass{subdir/class} (#12284)
				// we strip the name in this case.
				string dummy;
				tclass = rsplit(tclass, dummy, '/');
			}
			p.skip_spaces();
			// Only set text class if a class hasn't been forced
			// (this was set above)
			if (!class_set) {
				// textclass needs to be set at this place (if not already done)
				// as we need to know it for other parameters
				// (such as class-dependent paper size)
				setTextClass(tclass, tc);
				class_set = true;
			}

			// Font sizes.
			// Try those who are (most likely) known to all packages first
			handle_opt(opts, known_fontsizes, h_paperfontsize);
			delete_opt(opts, known_fontsizes);
			// delete "pt" at the end
			string::size_type i = h_paperfontsize.find("pt");
			if (i != string::npos)
				h_paperfontsize.erase(i);
			// Now those known specifically to the class
			vector<string> class_fsizes = getVectorFromString(tc.opt_fontsize(), "|");
			string const fsize_format = tc.fontsizeformat();
			for (auto const & fsize : class_fsizes) {
				string latexsize = subst(fsize_format, "$$s", fsize);
				vector<string>::iterator it = find(opts.begin(), opts.end(), latexsize);
				if (it != opts.end()) {
					h_paperfontsize = fsize;
					opts.erase(it);
					break;
				}
			}

			// The documentclass options are always parsed before the options
			// of the babel call so that a language cannot overwrite the babel
			// options.
			handle_opt(opts, known_languages, h_language);
			delete_opt(opts, known_languages);

			// math indentation
			if ((it = find(opts.begin(), opts.end(), "fleqn"))
				 != opts.end()) {
				h_is_mathindent = "1";
				opts.erase(it);
			}
			// formula numbering side
			if ((it = find(opts.begin(), opts.end(), "leqno"))
				 != opts.end()) {
				h_math_numbering_side = "left";
				opts.erase(it);
			}
			else if ((it = find(opts.begin(), opts.end(), "reqno"))
				 != opts.end()) {
				h_math_numbering_side = "right";
				opts.erase(it);
			}

			// paper orientation
			if ((it = find(opts.begin(), opts.end(), "landscape")) != opts.end()) {
				h_paperorientation = "landscape";
				opts.erase(it);
			}
			// paper sides
			if ((it = find(opts.begin(), opts.end(), "oneside"))
				 != opts.end()) {
				h_papersides = "1";
				opts.erase(it);
			}
			if ((it = find(opts.begin(), opts.end(), "twoside"))
				 != opts.end()) {
				h_papersides = "2";
				opts.erase(it);
			}
			// paper columns
			if ((it = find(opts.begin(), opts.end(), "onecolumn"))
				 != opts.end()) {
				h_papercolumns = "1";
				opts.erase(it);
			}
			if ((it = find(opts.begin(), opts.end(), "twocolumn"))
				 != opts.end()) {
				h_papercolumns = "2";
				opts.erase(it);
			}
			// paper sizes
			// some size options are known by the document class, other sizes
			// are handled by the \geometry command of the geometry package
			vector<string> class_psizes = getVectorFromString(tc.opt_pagesize(), "|");
			string const psize_format = tc.pagesizeformat();
			for (auto const & psize : class_psizes) {
				string latexsize = subst(psize_format, "$$s", psize);
				vector<string>::iterator it = find(opts.begin(), opts.end(), latexsize);
				if (it != opts.end()) {
					h_papersize = psize;
					opts.erase(it);
					break;
				}
				if (psize_format == "$$spaper")
					continue;
				// Also try with the default format since this is understood by
				// most classes
				latexsize = psize + "paper";
				it = find(opts.begin(), opts.end(), latexsize);
				if (it != opts.end()) {
					h_papersize = psize;
					opts.erase(it);
					break;
				}
			}
			// the remaining options
			h_options = join(opts, ",");
			continue;
		}

		if (t.cs() == "DocumentMetadata") {
			h_doc_metadata = trimSpaceAndEol(p.getArg('{', '}'));
			continue;
		}

		if (t.cs() == "usepackage") {
			string const options = p.getArg('[', ']');
			string const name = p.getArg('{', '}');
			vector<string> vecnames;
			split(name, vecnames, ',');
			vector<string>::const_iterator it  = vecnames.begin();
			vector<string>::const_iterator end = vecnames.end();
			for (; it != end; ++it)
				handle_package(p, trimSpaceAndEol(*it), options,
					       in_lyx_preamble, detectEncoding);
			continue;
		}

		if (t.cs() == "inputencoding") {
			string const encoding = p.getArg('{','}');
			Encoding const * const enc = encodings.fromLaTeXName(
				encoding, Encoding::inputenc, true);
			if (!enc) {
				if (!detectEncoding)
					warning_message("Unknown encoding " + encoding + ". Ignoring.");
			} else {
				if (!enc->unsafe())
					h_inputencoding = enc->name();
				p.setEncoding(enc->iconvName());
			}
			continue;
		}

		if (t.cs() == "newenvironment") {
			string const name = p.getArg('{', '}');
			string const opt1 = p.getFullOpt();
			string const opt2 = p.getFullOpt();
			string const beg = p.verbatim_item();
			string const end = p.verbatim_item();
			if (!in_lyx_preamble) {
				h_preamble << "\\newenvironment{" << name
				           << '}' << opt1 << opt2 << '{'
				           << beg << "}{" << end << '}';
			}
			add_known_environment(name, opt1, !opt2.empty(),
			                      from_utf8(beg), from_utf8(end));
			continue;
		}

		if (t.cs() == "newtheorem") {
			bool star = false;
			if (p.next_token().character() == '*') {
				p.get_token();
				star = true;
			}
			string const name = p.getArg('{', '}');
			string const opt1 = p.getFullOpt();
			string const opt2 = p.getFullOpt();
			string const body = p.verbatim_item();
			string const opt3 = p.getFullOpt();
			string const cmd = star ? "\\newtheorem*" : "\\newtheorem";

			string const complete = cmd + "{" + name + '}' +
				          opt1 + opt2 + '{' + body + '}' + opt3;

			add_known_theorem(name, opt1, !opt2.empty(), from_utf8(complete));

			if (!in_lyx_preamble)
				h_preamble << complete;
			continue;
		}

		if (t.cs() == "def") {
			string name = p.get_token().cs();
			// In fact, name may be more than the name:
			// In the test case of bug 8116
			// name == "csname SF@gobble@opt \endcsname".
			// Therefore, we need to use asInput() instead of cs().
			while (p.next_token().cat() != catBegin)
				name += p.get_token().asInput();
			if (!in_lyx_preamble)
				h_preamble << "\\def\\" << name << '{'
					   << p.verbatim_item() << "}";
			continue;
		}

		if (t.cs() == "newcolumntype") {
			string const name = p.getArg('{', '}');
			trimSpaceAndEol(name);
			int nargs = 0;
			string opts = p.getOpt();
			if (!opts.empty()) {
				istringstream is(string(opts, 1));
				is >> nargs;
			}
			special_columns_[name[0]] = nargs;
			h_preamble << "\\newcolumntype{" << name << "}";
			if (nargs)
				h_preamble << "[" << nargs << "]";
			h_preamble << "{" << p.verbatim_item() << "}";
			continue;
		}

		if (t.cs() == "setcounter") {
			string const name = p.getArg('{', '}');
			string const content = p.getArg('{', '}');
			if (name == "secnumdepth")
				h_secnumdepth = content;
			else if (name == "tocdepth")
				h_tocdepth = content;
			else
				h_preamble << "\\setcounter{" << name << "}{" << content << "}";
			continue;
		}

		if (t.cs() == "setlength") {
			string const name = p.verbatim_item();
			string const content = p.verbatim_item();
			// the paragraphs are only not indented when \parindent is set to zero
			if (name == "\\parindent" && content != "")
				h_paragraph_indentation = translate_len(content);
			else if (name == "\\parskip" && isPackageUsed("parskip")) {
				if (content == "\\smallskipamount")
					h_defskip = "smallskip";
				else if (content == "\\medskipamount")
					h_defskip = "medskip";
				else if (content == "\\bigskipamount")
					h_defskip = "bigskip";
				else if (content == "\\baselineskip")
					h_defskip = "fullline";
				else
					h_defskip = translate_len(content);
			} else if (name == "\\mathindent") {
				h_mathindentation = translate_len(content);
			} else
				h_preamble << "\\setlength{" << name << "}{" << content << "}";
			continue;
		}

		if (t.cs() == "onehalfspacing") {
			h_spacing = "onehalf";
			continue;
		}

		if (t.cs() == "doublespacing") {
			h_spacing = "double";
			continue;
		}

		if (t.cs() == "setstretch") {
			h_spacing = "other " + p.verbatim_item();
			continue;
		}

		if (t.cs() == "synctex") {
			// the scheme is \synctex=value
			// where value can only be "1" or "-1"
			h_output_sync = "1";
			// there can be any character behind the value (e.g. a linebreak or a '\'
			// therefore we extract it char by char
			p.get_token();
			string value = p.get_token().asInput();
			if (value == "-")
				value += p.get_token().asInput();
			h_output_sync_macro = "\\synctex=" + value;
			continue;
		}

		if (t.cs() == "begin") {
			string const name = p.getArg('{', '}');
			if (name == "document")
				break;
			h_preamble << "\\begin{" << name << "}";
			continue;
		}

		if (t.cs() == "geometry") {
			vector<string> opts = split_options(p.getArg('{', '}'));
			handle_geometry(opts);
			continue;
		}

		if (t.cs() == "arrayrulecolor") {
			string const color = p.getArg('{', '}');
			h_table_bordercolor = getLyXColor(color, true);
			if (h_table_bordercolor.empty())
				h_preamble << "\\arrayrulecolor{" << color << '}';
		}

		if (t.cs() == "rowcolors") {
			string const startrow = p.getArg('{', '}');
			string const oddrowcolor = p.getArg('{', '}');
			string const evenrowcolor = p.getArg('{', '}');
			if (isStrInt(startrow))
				h_table_alt_row_colors_start = convert<int>(startrow);
			else
				h_table_alt_row_colors_start = 1;
			h_table_odd_row_color = getLyXColor(oddrowcolor, true);
			h_table_even_row_color = getLyXColor(evenrowcolor, true);
			if (h_table_odd_row_color.empty() || h_table_even_row_color.empty()) {
				h_table_odd_row_color = "default";
				h_table_even_row_color = "default";
				h_preamble << "\\rowcolors{" << startrow << "}{" << oddrowcolor << "}{" << evenrowcolor << '}';
			}
		}

		if (t.cs() == "definecolor") {
			string const color = p.getArg('{', '}');
			string const space = p.getArg('{', '}');
			string const value = p.getArg('{', '}');
			// try to convert value to rgb
			string const conv_value = convert_color_value(space, value);
			if (!conv_value.empty() || space == "HTML") {
				if (color == "document_fontcolor")
					h_fontcolor = color;
				else if (color == "note_fontcolor")
					h_notefontcolor = color;
				else if (color == "page_backgroundcolor")
					h_backgroundcolor = color;
				else if (color == "shadecolor")
					h_boxbgcolor = color;
				if (space == "HTML")
					// here we already have the (hex) value we want
					h_custom_colors[color] = value;
				else {
					// convert from rgb to hex
					RGBColor c(RGBColorFromLaTeX(conv_value));
					h_custom_colors[color] = ltrim(X11hexname(c), "#");
				}
			} else {
				h_preamble << "\\definecolor{" << color
				           << "}{" << space << "}{" << value
				           << '}';
			}
			continue;
		}

		if (t.cs() == "colorlet") {
			string const color1 = p.getArg('{', '}');
			string const color2 = p.getArg('{', '}');
			if (color1 == "document_fontcolor")
				h_fontcolor = getLyXColor(color2, true);
			else if (color1 == "note_fontcolor")
				h_notefontcolor = getLyXColor(color2, true);
			else if (color1 == "page_backgroundcolor")
				h_backgroundcolor = getLyXColor(color2, true);
			else if (color1 == "shadecolor")
				h_boxbgcolor = getLyXColor(color2, true);
			else {
				h_preamble << "\\colorlet{" << color1
				           << "}{" << color2 << '}';
			}
			continue;
		}

		if (t.cs() == "bibliographystyle") {
			h_biblio_style = p.verbatim_item();
			continue;
		}

		if (t.cs() == "jurabibsetup") {
			// FIXME p.getArg('{', '}') is most probably wrong (it
			//       does not handle nested braces).
			//       Use p.verbatim_item() instead.
			vector<string> jurabibsetup =
				split_options(p.getArg('{', '}'));
			// add jurabibsetup to the jurabib package options
			add_package("jurabib", jurabibsetup);
			if (!jurabibsetup.empty()) {
				h_preamble << "\\jurabibsetup{"
					   << join(jurabibsetup, ",") << '}';
			}
			continue;
		}

		if (t.cs() == "hypersetup") {
			vector<string> hypersetup =
				split_options(p.verbatim_item());
			// add hypersetup to the hyperref package options
			handle_hyperref(hypersetup);
			if (!hypersetup.empty()) {
				h_preamble << "\\hypersetup{"
				           << join(hypersetup, ",") << '}';
			}
			continue;
		}

		if (t.cs() == "includeonly") {
			vector<string> includeonlys = getVectorFromString(p.getArg('{', '}'));
			for (auto & iofile : includeonlys) {
				string filename(normalize_filename(iofile));
				string const path = getMasterFilePath(true);
				// We want to preserve relative/absolute filenames,
				// therefore path is only used for testing
				if (!makeAbsPath(filename, path).exists()) {
					// The file extension is probably missing.
					// Now try to find it out.
					string const tex_name =
						find_file(filename, path,
							  known_tex_extensions);
					if (!tex_name.empty())
						filename = tex_name;
				}
				if (makeAbsPath(filename, path).exists())
					fix_child_filename(filename);
				else
					warning_message("Warning: Could not find included file '"
							+ filename + "'.");
				h_includeonlys.push_back(changeExtension(filename, "lyx"));
			}
			continue;
		}

		if (is_known(t.cs(), known_if_3arg_commands)) {
			// prevent misparsing of \usepackage if it is used
			// as an argument (see e.g. our own output of
			// \@ifundefined above)
			string const arg1 = p.verbatim_item();
			string const arg2 = p.verbatim_item();
			string const arg3 = p.verbatim_item();
			// test case \@ifundefined{date}{}{\date{}}
			if (t.cs() == "@ifundefined" && arg1 == "date" &&
			    arg2.empty() && arg3 == "\\date{}") {
				h_suppress_date = "true";
			// older tex2lyx versions did output
			// \@ifundefined{definecolor}{\usepackage{color}}{}
			} else if (t.cs() == "@ifundefined" &&
			           arg1 == "definecolor" &&
			           arg2 == "\\usepackage{color}" &&
			           arg3.empty()) {
				if (!in_lyx_preamble)
					h_preamble << package_beg_sep
					           << "color"
					           << package_mid_sep
					           << "\\@ifundefined{definecolor}{color}{}"
					           << package_end_sep;
			// test for case
			//\@ifundefined{showcaptionsetup}{}{%
			// \PassOptionsToPackage{caption=false}{subfig}}
			// that LyX uses for subfloats
			} else if (t.cs() == "@ifundefined" &&
			           arg1 == "showcaptionsetup" && arg2.empty()
				&& arg3 == "%\n \\PassOptionsToPackage{caption=false}{subfig}") {
				; // do nothing
			} else if (!in_lyx_preamble) {
				h_preamble << t.asInput()
				           << '{' << arg1 << '}'
				           << '{' << arg2 << '}'
				           << '{' << arg3 << '}';
			}
			continue;
		}

		if (is_known(t.cs(), known_if_commands)) {
			// must not parse anything in conditional code, since
			// LyX would output the parsed contents unconditionally
			if (!in_lyx_preamble)
				h_preamble << t.asInput();
			handle_if(p, in_lyx_preamble);
			continue;
		}

		if (!t.cs().empty() && !in_lyx_preamble) {
			h_preamble << '\\' << t.cs();
			continue;
		}
	}

	// set textclass if not yet done (snippets without \documentclass and forced class)
	if (!class_set)
		setTextClass(h_textclass, tc);

	// remove the whitespace
	p.skip_spaces();

	if (h_papersides.empty()) {
		ostringstream ss;
		ss << tc.sides();
		h_papersides = ss.str();
	}

	// If the CJK package is used we cannot set the document language from
	// the babel options. Instead, we guess which language is used most
	// and set this one.
	default_language = h_language;
	if (is_full_document &&
	    (auto_packages.find("CJK") != auto_packages.end() ||
	     auto_packages.find("CJKutf8") != auto_packages.end())) {
		p.pushPosition();
		h_language = guessLanguage(p, default_language);
		p.popPosition();
		if (explicit_babel && h_language != default_language) {
			// We set the document language to a CJK language,
			// but babel is explicitly called in the user preamble
			// without options. LyX will not add the default
			// language to the document options if it is either
			// english, or no text is set as default language.
			// Therefore we need to add a language option explicitly.
			// FIXME: It would be better to remove all babel calls
			//        from the user preamble, but this is difficult
			//        without re-introducing bug 7861.
			if (h_options.empty())
				h_options = lyx2babel(default_language);
			else
				h_options += ',' + lyx2babel(default_language);
		}
	}

	// Finally, set the quote style.
	// LyX knows the following quotes styles:
	// british, cjk, cjkangle, danish, english, french, german,
	// polish, russian, swedish, swiss, and hebrew
	// conversion list taken from
	// https://en.wikipedia.org/wiki/Quotation_mark,_non-English_usage
	// (quotes for kazakh are unknown)
	// british
	if (is_known(h_language, known_british_quotes_languages))
		h_quotes_style = "british";
	// cjk
	else if (is_known(h_language, known_cjk_quotes_languages))
		h_quotes_style = "cjk";
	// cjkangle
	else if (is_known(h_language, known_cjkangle_quotes_languages))
		h_quotes_style = "cjkangle";
	// danish
	else if (is_known(h_language, known_danish_quotes_languages))
		h_quotes_style = "danish";
	// french
	else if (is_known(h_language, known_french_quotes_languages))
		h_quotes_style = "french";
	// german
	else if (is_known(h_language, known_german_quotes_languages))
		h_quotes_style = "german";
	// polish
	else if (is_known(h_language, known_polish_quotes_languages))
		h_quotes_style = "polish";
	// hungarian
	else if (is_known(h_language, known_hungarian_quotes_languages))
		h_quotes_style = "hungarian";
	// russian
	else if (is_known(h_language, known_russian_quotes_languages))
		h_quotes_style = "russian";
	// swedish
	else if (is_known(h_language, known_swedish_quotes_languages))
		h_quotes_style = "swedish";
	// swiss
	else if (is_known(h_language, known_swiss_quotes_languages))
		h_quotes_style = "swiss";
	// hebrew
	else if (is_known(h_language, known_hebrew_quotes_languages))
		h_quotes_style = "hebrew";
	
	// english
	else if (is_known(h_language, known_english_quotes_languages))
		h_quotes_style = "english";
}


string Preamble::parseEncoding(Parser & p, string const & forceclass)
{
	TeX2LyXDocClass dummy;
	parse(p, forceclass, true, dummy);
	if (h_inputencoding != "auto-legacy" && h_inputencoding != "auto-legacy-plain")
		return h_inputencoding;
	return "";
}


string babel2lyx(string const & language)
{
	char const * const * where = is_known(language, known_languages);
	if (where)
		return known_coded_languages[where - known_languages];
	return language;
}


string lyx2babel(string const & language)
{
	char const * const * where = is_known(language, known_coded_languages);
	if (where)
		return known_languages[where - known_coded_languages];
	return language;
}


string Preamble::polyglossia2lyx(string const & language)
{
	char const * const * where = is_known(language, polyglossia_languages);
	if (where)
		return coded_polyglossia_languages[where - polyglossia_languages];
	return language;
}

// }])


} // namespace lyx
