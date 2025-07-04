// -*- C++ -*-
/**
 * \file qt_helpers.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Dekel Tsur
 * \author Richard Kimberly Heck
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef QTHELPERS_H
#define QTHELPERS_H

#include "ColorSet.h"
#include "support/Length.h"
#include "support/qstring_helpers.h"
#include "support/filetools.h"
#include "qt_i18n.h"

#include <list>

class QComboBox;
class QLineEdit;
class QString;
class QWidget;
class QAbstractItemView;

namespace lyx {

class Buffer;

namespace support { class FileName; }

namespace frontend {

class LengthCombo;

/// method to get a Length from widgets (LengthCombo)
std::string widgetsToLength(QLineEdit const * input, LengthCombo const * combo);
/// method to get a Length from widgets (QComboBox)
Length widgetsToLength(QLineEdit const * input, QComboBox const * combo);

/// method to set widgets from a Length
//FIXME Remove default_unit argument for the first form. FIXME Change
// all the code to remove default_unit argument when equal to the
// default.
void lengthToWidgets(QLineEdit * input, LengthCombo * combo,
		     Length const & len,
		     Length::UNIT default_unit = Length::defaultUnit());
/// method to set widgets from a string
void lengthToWidgets(QLineEdit * input, LengthCombo * combo,
		     std::string const & len,
		     Length::UNIT default_unit = Length::defaultUnit());
/// method to set widgets from a docstring
void lengthToWidgets(QLineEdit * input, LengthCombo * combo,
docstring const & len, Length::UNIT default_unit);

/// method to get a double value from a localized widget (QLineEdit)
double widgetToDouble(QLineEdit const * input);
/// method to get a double value from a localized widget (QLineEdit)
std::string widgetToDoubleStr(QLineEdit const * input);
/// method to set a (localized) double value in a widget (QLineEdit)
void doubleToWidget(QLineEdit * input, double value,
	char f = 'g', int prec = 6);
/// method to set a (localized) double value in a widget (QLineEdit)
void doubleToWidget(QLineEdit * input, std::string const & value,
	char f = 'g', int prec = 6);
/**
 * method to format localized floating point numbers without
 * ever using scientific notation
 */
QString formatLocFPNumber(double d);

/// Method to sort QStrings locale-aware (e.g. in combo widgets)
bool SortLocaleAware(QString const & lhs, QString const & rhs);

/// Method to sort colors by GUI name in combo widgets
bool ColorSorter(ColorCode lhs, ColorCode rhs);

/// colors a widget red if invalid
void setValid(QWidget * widget, bool valid);

// set focus and highlight the current item if there is no selection already
void focusAndHighlight(QAbstractItemView * w);

// Sets all widget in highlighted in highlighted colour, and their children in
// plain in standard colours.
void setMessageColour(std::list<QWidget *> const & highlighted,
                      std::list<QWidget *> const & plain);

/// Shows a directory in OSs file browser
void showDirectory(support::FileName const & directory);
/// handle request for showing citation content - shows pdf/ps or
/// web page in target; external script can be used for pdf/ps view
/// \p docpath holds the document path
void showTarget(std::string const & target, Buffer const & buf);

} // namespace frontend


/**
 * qt_ - i18nize string and convert to QString
 *
 * Use this in qt/ instead of _()
 */
QString const qt_(std::string const & str);
QString const qt_(QString const & qstr);


///
support::FileName libFileSearch(QString const & dir, QString const & name,
				QString const & ext = QString(),
				support::search_mode mode = support::must_exist);

///
support::FileName imageLibFileSearch(QString & dir, QString const & name,
				QString const & ext = QString(),
				support::search_mode mode = support::must_exist,
				bool dark_mode = false);

/** Build filelists of all available bst/cls/sty-files. Done through
*  kpsewhich and an external script, saved in *Files.lst.
*  \param arg: cls, sty, bst, or bib, as required by TeXFiles.py.
*         Can be a list of these, too.
*/
void rescanTexStyles(std::string const & arg = empty_string());

/** Fill \c contents from one of the three texfiles.
 *  Each entry in the file list is returned as a name_with_path
 */
QStringList texFileList(QString const & filename);

/// Convert internal line endings to line endings as expected by the OS
QString const externalLineEnding(docstring const & str);

/// Convert line endings in any formnat to internal line endings
docstring const internalLineEnding(QString const & str);

// wrapper around the docstring versions
QString internalPath(QString const &);
QString onlyFileName(QString const & str);
QString onlyPath(QString const & str);
QStringList fileFilters(QString const & description);
/// all files wildcard for filter in qt dialogs (crossplatform)
QString wildcardAllFiles();

/// Remove the extension from \p name
QString removeExtension(QString const & name);

/** Add the extension \p ext to \p name.
 Use this instead of changeExtension if you know that \p name is without
 extension, because changeExtension would wrongly interpret \p name if it
 contains a dot.
 */
QString addExtension(QString const & name, QString const & extension);

/// Return the extension of the file (not including the .)
QString getExtension(QString const & name);
QString makeAbsPath(QString const & relpath, QString const & base);
QString changeExtension(QString const & oldname, QString const & ext);

/// Format \param text for display as a ToolTip, breaking at lines of \param
/// width ems. Note: this function is expensive. Better call it in a delayed
/// manner, i.e. not to fill in a model (see for instance the function
/// ToolTipFormatter::eventFilter).
///
/// When is it called automatically? Whenever the tooltip is not already rich
/// text beginning with <html>, and is defined by the following functions:
///  - QWidget::setToolTip(),
///  - QAbstractItemModel::setData(..., Qt::ToolTipRole),
///  - Inset::toolTip()
///
/// In other words, tooltips can use Qt html, and the tooltip will still be
/// correctly broken. Moreover, it is possible to specify an entirely custom
/// tooltip (not subject to automatic formatting) by giving it in its entirety,
/// i.e. starting with <html>.
QString formatToolTip(QString text, int width = 30);


// Check if text is understood as rich text (Qt HTML) and if so, produce a
// rendering in plain text.
QString qtHtmlToPlainText(QString const & text);


} // namespace lyx

#endif // QTHELPERS_H
