/**
 * \file qstring_helper.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Dekel Tsur
 *
 * Full author contact details are available in file CREDITS.
 *
 * A collection of unicode conversion functions, using iconv.
 */

#include <config.h>

#include "support/qstring_helpers.h"

#include "support/debug.h"
#include "support/docstring.h"
#include "support/qstring_helpers.h"

#include <QRegularExpression>
#include <QLocale>
#include <QString>
#include <QVector>

namespace lyx {

LyXErr & operator<<(LyXErr & err, QString const & str)
{
	return err << fromqstr(str);
}


QString toqstr(char const * str)
{
	return QString::fromUtf8(str);
}

QString toqstr(std::string const & str)
{
	return toqstr(str.c_str());
}


QString toqstr(docstring const & ucs4)
{
	// If possible we let qt do the work, since this version does not
	// need to be superfast.
	if (ucs4.empty())
		return QString();
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	return QString::fromStdU32String(reinterpret_cast<std::u32string const &>(ucs4));
#else
	return QString::fromUcs4((uint const *)ucs4.data(), ucs4.length());
#endif
}

QString toqstr(char_type ucs4)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
	return QString::fromStdU32String(std::u32string(1, ucs4));
#else
	union { char_type c; uint i; } u = { ucs4 };
	return QString::fromUcs4(&u.i, 1);
#endif
}

docstring qstring_to_ucs4(QString const & qstr)
{
	if (qstr.isEmpty())
		return docstring();
	QVector<uint> const ucs4 = qstr.toUcs4();
	return docstring((char_type const *)(ucs4.constData()), ucs4.size());
}

std::string fromqstr(QString const & str)
{
	return str.isEmpty() ? std::string() : std::string(str.toUtf8());
}

QString charFilterRegExp(QString const & filter)
{
	QString re = ".*";
	for (QChar const & c : filter) {
		if (c.isLower())
			re +=  "[" + QRegularExpression::escape(c)
			           + QRegularExpression::escape(c.toUpper()) + "]";
		else
			re +=  QRegularExpression::escape(c);
	}
	return re;
}

QString charFilterRegExpC(QString const & filter)
{
	QString re = "(";
	for (QChar const & c : filter) {
		if (c.isLower())
			re +=  "[" + QRegularExpression::escape(c)
		               + QRegularExpression::escape(c.toUpper()) + "]";
		else
			re +=  QRegularExpression::escape(c);
	}
	return re + ")";
}

QString locLengthString(QString const & str)
{
	QLocale loc;
	QString res = str;
	return res.replace(QString("."), loc.decimalPoint());
}


docstring locLengthDocString(docstring const & str)
{
	return qstring_to_ucs4(locLengthString(toqstr(str)));
}


QString unlocLengthString(QString const & str)
{
	QLocale loc;
	QString res = str;
	return res.replace(loc.decimalPoint(), QString("."));
}


} // namespace lyx
