/**
 * \file InsetInfo.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Bo Peng
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */
#include <config.h>

#include "InsetInfo.h"
#include "LyX.h"
#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Changes.h"
#include "Cursor.h"
#include "CutAndPaste.h"
#include "Font.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "InsetGraphics.h"
#include "InsetSpecialChar.h"
#include "KeyMap.h"
#include "LaTeXFeatures.h"
#include "Language.h"
#include "LayoutFile.h"
#include "LyXAction.h"
#include "LyXRC.h"
#include "LyXVC.h"
#include "output_docbook.h"
#include "Paragraph.h"
#include "ParIterator.h"
#include "ParagraphParameters.h"
#include "version.h"

#include "frontends/Application.h"

#include "support/Changer.h"
#include "support/convert.h"
#include "support/debug.h"
#include "support/docstream.h"
#include "support/docstring_list.h"
#include "support/ExceptionMessage.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/Length.h"
#include "support/Lexer.h"
#include "support/Messages.h"
#include "support/lstrings.h"
#include "support/qstring_helpers.h"
#include "support/Translator.h"

#include <sstream>
#include <tuple>

#include <QtGui/QImage>
#include <QDate>
#include <QLocale>

using namespace std;
using namespace lyx::support;

namespace lyx {

namespace {

typedef Translator<InsetInfoParams::info_type, string> NameTranslator;

NameTranslator const initTranslator()
{
	NameTranslator translator(InsetInfoParams::UNKNOWN_INFO, "unknown");

	translator.addPair(InsetInfoParams::SHORTCUTS_INFO, "shortcuts");
	translator.addPair(InsetInfoParams::SHORTCUT_INFO, "shortcut");
	translator.addPair(InsetInfoParams::LYXRC_INFO, "lyxrc");
	translator.addPair(InsetInfoParams::PACKAGE_INFO, "package");
	translator.addPair(InsetInfoParams::TEXTCLASS_INFO, "textclass");
	translator.addPair(InsetInfoParams::MENU_INFO, "menu");
	translator.addPair(InsetInfoParams::L7N_INFO, "l7n");
	translator.addPair(InsetInfoParams::ICON_INFO, "icon");
	translator.addPair(InsetInfoParams::BUFFER_INFO, "buffer");
	translator.addPair(InsetInfoParams::LYX_INFO, "lyxinfo");
	translator.addPair(InsetInfoParams::VCS_INFO, "vcs");
	translator.addPair(InsetInfoParams::DATE_INFO, "date");
	translator.addPair(InsetInfoParams::MODDATE_INFO, "moddate");
	translator.addPair(InsetInfoParams::FIXDATE_INFO, "fixdate");
	translator.addPair(InsetInfoParams::TIME_INFO, "time");
	translator.addPair(InsetInfoParams::MODTIME_INFO, "modtime");
	translator.addPair(InsetInfoParams::FIXTIME_INFO, "fixtime");

	return translator;
}

/// The translator between the information type enum and corresponding string.
NameTranslator const & nameTranslator()
{
	static NameTranslator const translator = initTranslator();
	return translator;
}


typedef Translator<InsetInfoParams::info_type, string> DefaultValueTranslator;

DefaultValueTranslator const initDVTranslator()
{
	DefaultValueTranslator translator(InsetInfoParams::UNKNOWN_INFO, "");

	translator.addPair(InsetInfoParams::SHORTCUTS_INFO, "info-insert");
	translator.addPair(InsetInfoParams::SHORTCUT_INFO, "info-insert");
	translator.addPair(InsetInfoParams::LYXRC_INFO, "user_name");
	translator.addPair(InsetInfoParams::PACKAGE_INFO, "graphics");
	translator.addPair(InsetInfoParams::TEXTCLASS_INFO, "article");
	translator.addPair(InsetInfoParams::MENU_INFO, "info-insert");
	translator.addPair(InsetInfoParams::L7N_INFO, "");
	translator.addPair(InsetInfoParams::ICON_INFO, "info-insert");
	translator.addPair(InsetInfoParams::BUFFER_INFO, "name-noext");
	translator.addPair(InsetInfoParams::LYX_INFO, "version");
	translator.addPair(InsetInfoParams::VCS_INFO, "revision");
	translator.addPair(InsetInfoParams::DATE_INFO, "loclong");
	translator.addPair(InsetInfoParams::MODDATE_INFO, "loclong");
	translator.addPair(InsetInfoParams::FIXDATE_INFO, "loclong");
	translator.addPair(InsetInfoParams::TIME_INFO, "long");
	translator.addPair(InsetInfoParams::MODTIME_INFO, "long");
	translator.addPair(InsetInfoParams::FIXTIME_INFO, "long");

	return translator;
}

/// The translator between the information type enum and some sensible default value.
DefaultValueTranslator const & defaultValueTranslator()
{
	static DefaultValueTranslator const translator = initDVTranslator();
	return translator;
}

} // namespace


/////////////////////////////////////////////////////////////////////
//
// InsetInfoParams
//
///////////////////////////////////////////////////////////////////////

InsetInfoParams infoparams;

namespace{
set<string> getTexFileList(string const & filename)
{
	set<string> list;
	FileName const file = libFileSearch(string(), filename);
	if (file.empty())
		return list;

	// FIXME Unicode.
	vector<docstring> doclist =
		getVectorFromString(file.fileContents("UTF-8"), from_ascii("\n"));

	// Normalise paths like /foo//bar ==> /foo/bar
	// No "auto const &" because doc is modified later
	// coverity[auto_causes_copy]
	for (auto doc : doclist) {
		doc = subst(doc, from_ascii("\r"), docstring());
		while (contains(doc, from_ascii("//")))
			doc = subst(doc, from_ascii("//"), from_ascii("/"));
		if (!doc.empty())
			list.insert(removeExtension(onlyFileName(to_utf8(doc))));
	}

	// remove duplicates
	return list;
}

bool translateString(docstring const & in, docstring & out, string const & lcode)
{
	out = translateIfPossible(in, lcode);
	return in != out;
}


docstring getDate(string const & iname, QDate const date, Language const * lang)
{
	QLocale loc;
	if (lang)
		loc = QLocale(toqstr(lang->code()));
	if (iname == "long")
		return qstring_to_ucs4(loc.toString(date, QLocale::LongFormat));
	else if (iname == "short")
		return qstring_to_ucs4(loc.toString(date, QLocale::ShortFormat));
	else if (iname == "ISO")
		return qstring_to_ucs4(date.toString(Qt::ISODate));
	else if (iname == "loclong")
		return lang ? qstring_to_ucs4(loc.toString(date, toqstr(lang->dateFormat(0))))
			    : _("No long date format (language unknown)!");
	else if (iname == "locmedium")
		return lang ? qstring_to_ucs4(loc.toString(date, toqstr(lang->dateFormat(1))))
			    : _("No medium date format (language unknown)!");
	else if (iname == "locshort")
		return lang ? qstring_to_ucs4(loc.toString(date, toqstr(lang->dateFormat(2))))
				: _("No short date format (language unknown)!");
	else
		return qstring_to_ucs4(loc.toString(date, toqstr(iname)));
}


docstring getTime(string const & iname, QTime const time, Language const * lang)
{
	QLocale loc;
	if (lang)
		loc = QLocale(toqstr(lang->code()));
	if (iname == "long")
		return qstring_to_ucs4(loc.toString(time, QLocale::LongFormat));
	else if (iname == "short")
		return qstring_to_ucs4(loc.toString(time, QLocale::ShortFormat));
	else if (iname == "ISO")
		return qstring_to_ucs4(time.toString(Qt::ISODate));
	else
		return qstring_to_ucs4(loc.toString(time, toqstr(iname)));
}
} // namespace anon


vector<pair<string,docstring>> InsetInfoParams::getArguments(Buffer const * buf,
							     string const & itype) const
{
	vector<pair<string,docstring>> result;

	switch (nameTranslator().find(itype)) {
	case UNKNOWN_INFO:
		result.push_back(make_pair("invalid", _("Please select a valid type!")));
		break;

	case SHORTCUT_INFO:
	case SHORTCUTS_INFO:
	case MENU_INFO:
	case ICON_INFO: {
		result.push_back(make_pair("custom", _("Custom")));
		for (auto const & name_code : lyxaction) {
			string const lfun = name_code.first;
			if (!lfun.empty())
				result.push_back(make_pair(lfun, from_ascii(lfun)));
		}
		break;
	}

	case L7N_INFO:
		result.push_back(make_pair("custom", _("Custom")));
		break;

	case LYXRC_INFO: {
		result.push_back(make_pair("custom", _("Custom")));
		set<string> rcs = lyxrc.getRCs();
		for (auto const & rc : rcs)
			result.push_back(make_pair(rc, from_ascii(rc)));
		break;
	}

	case PACKAGE_INFO:
	case TEXTCLASS_INFO: {
		result.push_back(make_pair("custom", _("Custom")));
		string const filename = (itype == "package") ? "styFiles.lst"
							    : "clsFiles.lst";
		set<string> flist = getTexFileList(filename);
		for (auto const & f : flist)
			result.push_back(make_pair(f, from_utf8(f)));
		break;
	}

	case BUFFER_INFO:
		result.push_back(make_pair("name", _("File name (with extension)")));
		result.push_back(make_pair("name-noext", _("File name (without extension)")));
		result.push_back(make_pair("path", _("File path")));
		result.push_back(make_pair("class", _("Used text class")));
		break;

	case VCS_INFO: {
		if (!buf->lyxvc().inUse()) {
			result.push_back(make_pair("invalid", _("No version control!")));
			break;
		}
		result.push_back(make_pair("revision", _("Revision[[Version Control]]")));
		result.push_back(make_pair("revision-abbrev", _("Abbreviated revision[[Version Control]]")));
		result.push_back(make_pair("tree-revision", _("Tree revision")));
		result.push_back(make_pair("author", _("Author")));
		result.push_back(make_pair("date", _("Date")));
		result.push_back(make_pair("time", _("Time[[of day]]")));
		break;
	}

	case LYX_INFO:
		result.push_back(make_pair("version", _("LyX version")));
		result.push_back(make_pair("layoutformat", _("LyX layout format")));
		break;

	case FIXDATE_INFO:
	case DATE_INFO:
	case MODDATE_INFO: {
		// TODO: away from a release, use parseDate instead.
		string const dt = split(name, '@');
		QDate date;
		if (itype == "moddate")
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
			date = QDateTime::fromSecsSinceEpoch(buf->fileName().lastModified()).date();
#else
			date = QDateTime::fromTime_t(buf->fileName().lastModified()).date();
#endif
		else if (itype == "fixdate" && !dt.empty()) {
			QDate const gdate = QDate::fromString(toqstr(dt), Qt::ISODate);
			date = (gdate.isValid()) ? gdate : QDate::currentDate();
		} else
			date = QDate::currentDate();
		result.push_back(make_pair("long",getDate("long", date, lang)));
		result.push_back(make_pair("short", getDate("short", date, lang)));
		result.push_back(make_pair("loclong", getDate("loclong", date, lang)));
		result.push_back(make_pair("locmedium", getDate("locmedium", date, lang)));
		result.push_back(make_pair("locshort", getDate("locshort", date, lang)));
		result.push_back(make_pair("ISO", getDate("ISO", date, lang)));
		result.push_back(make_pair("yyyy", getDate("yyyy", date, lang)));
		result.push_back(make_pair("MMMM", getDate("MMMM", date, lang)));
		result.push_back(make_pair("MMM", getDate("MMM", date, lang)));
		result.push_back(make_pair("dddd", getDate("dddd", date, lang)));
		result.push_back(make_pair("ddd", getDate("ddd", date, lang)));
		result.push_back(make_pair("custom", _("Custom")));
		break;
	}
	case FIXTIME_INFO:
	case TIME_INFO:
	case MODTIME_INFO: {
		// TODO: away from a release, use parseTime instead.
		string const tt = split(name, '@');
		QTime time;
		if (itype == "modtime")
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
			time = QDateTime::fromSecsSinceEpoch(buf->fileName().lastModified()).time();
#else
			time = QDateTime::fromTime_t(buf->fileName().lastModified()).time();
#endif
		else if (itype == "fixtime" && !tt.empty()) {
			QTime const gtime = QTime::fromString(toqstr(tt), Qt::ISODate);
			time = (gtime.isValid()) ? gtime : QTime::currentTime();
		} else
			time = QTime::currentTime();
		result.push_back(make_pair("long",getTime("long", time, lang)));
		result.push_back(make_pair("short", getTime("short", time, lang)));
		result.push_back(make_pair("ISO", getTime("ISO", time, lang)));
		result.push_back(make_pair("custom", _("Custom")));
		break;
	}
	}

	return result;
}


bool InsetInfoParams::validateArgument(Buffer const * buf, docstring const & arg,
				       bool const usedefaults) const
{
	string type;
	string name = trim(split(to_utf8(arg), type, ' '));
	if (name.empty() && usedefaults)
		name = defaultValueTranslator().find(type);

	switch (nameTranslator().find(type)) {
	case UNKNOWN_INFO:
		return false;

	case SHORTCUT_INFO:
	case SHORTCUTS_INFO:
	case MENU_INFO: {
		FuncRequest func = lyxaction.lookupFunc(name);
		return func.action() != LFUN_UNKNOWN_ACTION;
	}

	case L7N_INFO:
		return !name.empty();

	case ICON_INFO: {
		FuncCode const action = lyxaction.lookupFunc(name).action();
		if (action == LFUN_UNKNOWN_ACTION) {
			string dir = "images";
			return !imageLibFileSearch(dir, name, "svgz,png").empty();
		}
		return true;
	}

	case LYXRC_INFO: {
		set<string> rcs = lyxrc.getRCs();
		return rcs.find(name) != rcs.end();
	}

	case PACKAGE_INFO:
	case TEXTCLASS_INFO:
		return true;

	case BUFFER_INFO:
		return (name == "name" || name == "name-noext"
			|| name == "path" || name == "class");

	case VCS_INFO:
		if (name == "revision" || name == "revision-abbrev" || name == "tree-revision"
		    || name == "author" || name == "date" || name == "time")
			return buf->lyxvc().inUse();
		return false;

	case LYX_INFO:
		return name == "version" || name == "layoutformat";

	case FIXDATE_INFO: {
		string date;
		string piece;
		date = split(name, piece, '@');
		if (!date.empty() && !QDate::fromString(toqstr(date), Qt::ISODate).isValid())
			return false;
		if (!piece.empty())
			name = piece;
	}
	// fall through
	case DATE_INFO:
	case MODDATE_INFO: {
		if (name == "long" || name == "short" || name == "ISO")
			return true;
		else {
			QDate date = QDate::currentDate();
			return !date.toString(toqstr(name)).isEmpty();
		}
	}
	case FIXTIME_INFO: {
		string time;
		string piece;
		time = split(name, piece, '@');
		if (!time.empty() && !QTime::fromString(toqstr(time), Qt::ISODate).isValid())
			return false;
		if (!piece.empty())
			name = piece;
	}
	// fall through
	case TIME_INFO:
	case MODTIME_INFO: {
		if (name == "long" || name == "short" || name == "ISO")
			return true;
		else {
			QTime time = QTime::currentTime();
			return !time.toString(toqstr(name)).isEmpty();
		}
	}
	}

	return false;
}




string InsetInfoParams::infoType() const
{
	return nameTranslator().find(type);
}



/////////////////////////////////////////////////////////////////////////
//
// InsetInfo
//
/////////////////////////////////////////////////////////////////////////


namespace {

class InsetGraphicsTight : public InsetGraphics
{
public:
	///
	explicit InsetGraphicsTight(Buffer * buf) : InsetGraphics(buf) {}

	///
	int leftOffset(BufferView const *) const override { return 0; }
	///
	int rightOffset(BufferView const *) const override { return 0; }
};

}


InsetInfo::InsetInfo(Buffer * buf, string const & info)
	: InsetCollapsible(buf), initialized_(false)
{
	params_.type = InsetInfoParams::UNKNOWN_INFO;
	params_.force_ltr = false;
	setInfo(info);
	status_ = Collapsed;
}


Inset * InsetInfo::editXY(Cursor & cur, int x, int y)
{
	// do not allow the cursor to be set in this Inset
	return Inset::editXY(cur, x, y);
}


docstring InsetInfo::layoutName() const
{
	return from_ascii("Info:" + params_.infoType());
}


docstring InsetInfo::toolTip(BufferView const &, int, int) const
{
	docstring result;
	switch (nameTranslator().find(params_.infoType())) {
	case InsetInfoParams::UNKNOWN_INFO:
		result = _("Invalid information inset");
		break;
	case InsetInfoParams::SHORTCUT_INFO:
		result = bformat(_("The keybard shortcut for the function '%1$s'"),
				from_utf8(params_.name));
		break;
	case InsetInfoParams::SHORTCUTS_INFO:
		result = bformat(_("The keybard shortcuts for the function '%1$s'"),
				from_utf8(params_.name));
		break;
	case InsetInfoParams::MENU_INFO: 
		result = bformat(_("The menu location for the function '%1$s'"),
				from_utf8(params_.name));
		break;
	case InsetInfoParams::L7N_INFO: 
		result = bformat(_("The localization for the string '%1$s'"),
				from_utf8(params_.name));
		break;
	case InsetInfoParams::ICON_INFO:
		result = bformat(_("The toolbar icon for the function '%1$s'"),
				from_utf8(params_.name));
		break;
	case InsetInfoParams::LYXRC_INFO:
		result = bformat(_("The preference setting for the preference key '%1$s'"),
				from_utf8(params_.name));
		break;
	case InsetInfoParams::PACKAGE_INFO:
		result = bformat(_("Availability of the LaTeX package '%1$s'"),
				from_utf8(params_.name));
		break;
	case InsetInfoParams::TEXTCLASS_INFO:
		result = bformat(_("Availability of the LaTeX class '%1$s'"),
				from_utf8(params_.name));
		break;
	case InsetInfoParams::BUFFER_INFO:
		if (params_.name == "name")
			result = _("The name of this file (incl. extension)");
		else if (params_.name == "name-noext")
			result = _("The name of this file (without extension)");
		else if (params_.name == "path")
			result = _("The path where this file is saved");
		else if (params_.name == "class")
			result = _("The class this document uses");
		break;
	case InsetInfoParams::VCS_INFO:
		if (params_.name == "revision")
			result = _("Version control revision");
		else if (params_.name == "revision-abbrev")
			result = _("Version control abbreviated revision");
		else if (params_.name == "tree-revision")
			result = _("Version control tree revision");
		else if (params_.name == "author")
			 result = _("Version control author");
		else if (params_.name == "date")
			result = _("Version control date");
		else if (params_.name == "time")
			result = _("Version control time");
		break;
	case InsetInfoParams::LYX_INFO:
		if (params_.name == "version")
			result = _("The current LyX version");
		else if (params_.name == "layoutformat")
			result = _("The current LyX layout format");
		break;
	case InsetInfoParams::DATE_INFO:
		result = _("The current date");
		break;
	case InsetInfoParams::MODDATE_INFO:
		result = _("The date of last save");
		break;
	case InsetInfoParams::FIXDATE_INFO:
		result = _("A static date");
		break;
	case InsetInfoParams::TIME_INFO:
		result = _("The current time");
		break;
	case InsetInfoParams::MODTIME_INFO:
		result = _("The time of last save");
		break;
	case InsetInfoParams::FIXTIME_INFO:
		result = _("A static time");
		break;
	}

	return result;
}


void InsetInfo::read(Lexer & lex)
{
	string token;
	while (lex.isOK()) {
		lex.next();
		token = lex.getString();
		if (token == "type") {
			lex.next();
			token = lex.getString();
			params_.type = nameTranslator().find(token);
		} else if (token == "arg") {
			lex.next(true);
			params_.name = lex.getString();
		} else if (token == "\\end_inset")
			break;
	}
	if (token != "\\end_inset") {
		lex.printError("Missing \\end_inset at this point");
		throw ExceptionMessage(WarningException,
			_("Missing \\end_inset at this point."),
			from_utf8(token));
	}
}


void InsetInfo::write(ostream & os) const
{
	os << "Info\ntype  \"" << params_.infoType()
	   << "\"\narg   " << Lexer::quoteString(params_.name);
}


bool InsetInfo::showInsetDialog(BufferView * bv) const
{
	bv->showDialog("info");
	return true;
}


bool InsetInfo::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	switch (cmd.action()) {
	case LFUN_INSET_SETTINGS:
		return InsetCollapsible::getStatus(cur, cmd, flag);

	case LFUN_INSET_DIALOG_UPDATE:
	case LFUN_INSET_COPY_AS:
	case LFUN_INSET_DISSOLVE:
		flag.setEnabled(true);
		return true;

	case LFUN_INSET_MODIFY:
		if (nameTranslator().find(cmd.getArg(0)) == InsetInfoParams::UNKNOWN_INFO)
			return Inset::getStatus(cur, cmd, flag);
		if (params_.validateArgument(&buffer(), cmd.argument())) {
			flag.setEnabled(true);
			string typestr;
			string name = trim(split(to_utf8(cmd.argument()), typestr, ' '));
			InsetInfoParams::info_type type = nameTranslator().find(typestr);
			string origname = params_.name;
			if (type == InsetInfoParams::FIXDATE_INFO
			    || type == InsetInfoParams::FIXTIME_INFO)
				split(params_.name, origname, '@');
			flag.setOnOff(type == params_.type && name == origname);
			return true;
		}
		//fall through

	default:
		return false;
	}
}


void InsetInfo::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {
	case LFUN_INSET_MODIFY:
		if (nameTranslator().find(cmd.getArg(0)) == InsetInfoParams::UNKNOWN_INFO) {
			cur.undispatched();
			break;
		}
		cur.recordUndo();
		setInfo(to_utf8(cmd.argument()));
		cur.forceBufferUpdate();
		initialized_ = false;
		break;

	case LFUN_INSET_COPY_AS: {
		cap::clearSelection();
		Cursor copy(cur);
		copy.pushBackward(*this);
		copy.pit() = 0;
		copy.pos() = 0;
		copy.resetAnchor();
		copy.pit() = copy.lastpit();
		copy.pos() = copy.lastpos();
		copy.setSelection();
		cap::copySelection(copy);
		break;
	}

	default:
		InsetCollapsible::doDispatch(cur, cmd);
		break;
	}
}


void InsetInfo::setInfo(string const & info)
{
	if (info.empty())
		return;

	string saved_date_specifier;
	// Store old date specifier for potential re-use
	if (!params_.name.empty())
		saved_date_specifier = split(params_.name, '@');
	// info_type name
	string type;
	params_.name = trim(split(info, type, ' '));
	params_.type = nameTranslator().find(type);
	if (params_.name.empty())
		params_.name = defaultValueTranslator().find(params_.type);
	if (params_.type == InsetInfoParams::FIXDATE_INFO) {
		string const date_specifier = split(params_.name, '@');
		// If an explicit new fix date is specified, use that
		// Otherwise, use the old one or, if there is none,
		// the current date
		if (date_specifier.empty()) {
			if (saved_date_specifier.empty())
				params_.name += "@" + fromqstr(QDate::currentDate().toString(Qt::ISODate));
			else
				params_.name += "@" + saved_date_specifier;
		}
	}
	else if (params_.type == InsetInfoParams::FIXTIME_INFO) {
		string const time_specifier = split(params_.name, '@');
		// If an explicit new fix time is specified, use that
		// Otherwise, use the old one or, if there is none,
		// the current time
		if (time_specifier.empty()) {
			if (saved_date_specifier.empty())
				params_.name += "@" + fromqstr(QTime::currentTime().toString(Qt::ISODate));
			else
				params_.name += "@" + saved_date_specifier;
		}
	}
}


void InsetInfo::error(docstring const & err, Language const * lang)
{
	docstring const res = translateIfPossible(err, lang->code());
	bool const translated = res != err;
	// If the string is not translated, we use default lang (English)
	Font const f = translated ? Font(inherit_font, lang) : Font(inherit_font);
	setText(bformat(res, from_utf8(params_.name)), f, false);
}


void InsetInfo::info(docstring const & err, Language const * lang)
{
	docstring const res = translateIfPossible(err, lang->code());
	bool const translated = res != err;
	// If the string is not translated, we use default lang (English)
	Font const f = translated ? Font(inherit_font, lang) : Font(inherit_font);
	setText(translateIfPossible(err, lang->code()), f, false);
}


void InsetInfo::setText(docstring const & str, Language const * lang)
{
	setText(str, Font(inherit_font, lang), false);
}


bool InsetInfo::forceLTR(OutputParams const &) const
{
	return params_.force_ltr;
}


bool InsetInfo::forceLocalFontSwitch() const
{
	return params_.type == InsetInfoParams::MENU_INFO
		|| params_.type == InsetInfoParams::SHORTCUTS_INFO
		|| params_.type == InsetInfoParams::SHORTCUT_INFO
		|| params_.type == InsetInfoParams::L7N_INFO;
}


void InsetInfo::metrics(MetricsInfo & mi, Dimension & dim) const
{
	const_cast<InsetInfo *>(this)->build();
	InsetCollapsible::metrics(mi, dim);
}


void InsetInfo::draw(PainterInfo & pi, int x, int y) const
{
	Changer chg = changeVar(lyxrc.mark_foreign_language, false);
	InsetCollapsible::draw(pi, x, y);
}

void InsetInfo::updateBuffer(ParIterator const & it, UpdateType utype, bool const deleted)

{
	// If the Buffer is a clone, then we neither need nor want to do any
	// of what follows. We want, rather, just to inherit how things were
	// in the original Buffer. This is especially important for VCS.
	// Otherwise, we could in principle have different settings here
	// than in the Buffer we were exporting.
	// However, we need to check whether the inset is in an intitle
	// context.
	if (buffer().isClone()) {
		InsetText::checkIntitleContext(it);
		return;
	}
	BufferParams const & bp = buffer().params();
	params_.lang = it.paragraph().getFontSettings(bp, it.pos()).language();
	InsetCollapsible::updateBuffer(it, utype, deleted);
}


void InsetInfo::build()
{
	// If the Buffer is a clone, then we neither need nor want to do any
	// of what follows. We want, rather, just to inherit how things were
	// in the original Buffer. This is especially important for VCS.
	// Otherwise, we could in principle have different settings here
	// than in the Buffer we were exporting.
	if (buffer().isClone())
		return;

	Language const * tryguilang = languages.getFromCode(Messages::guiLanguage());
	// Some info insets use the language of the GUI (if available)
	Language const * guilang = tryguilang ? tryguilang : params_.lang;

	params_.force_ltr = !params_.lang->rightToLeft();
	// This is just to get the string into the po files
	docstring gui;
	switch (params_.type) {
	case InsetInfoParams::UNKNOWN_INFO:
		gui = _("Unknown Info!");
		info(from_ascii("Unknown Info!"), params_.lang);
		initialized_ = false;
		break;
	case InsetInfoParams::SHORTCUT_INFO:
	case InsetInfoParams::SHORTCUTS_INFO: {
		// shortcuts can change, so we need to re-do this each time
		FuncRequest const func = lyxaction.lookupFunc(params_.name);
		if (func.action() == LFUN_UNKNOWN_ACTION) {
			gui = _("Unknown action %1$s");
			error(from_ascii("Unknown action %1$s"), params_.lang);
			break;
		}
		KeyMap::Bindings bindings = theTopLevelKeymap().findBindings(func);
		if (bindings.empty()) {
			gui = _("undefined");
			info(from_ascii("undefined"), params_.lang);
			break;
		}
		docstring sequence;
		docstring seq_untranslated;
		if (params_.type == InsetInfoParams::SHORTCUT_INFO) {
			sequence = bindings.begin()->print(KeySequence::ForGui);
			seq_untranslated = bindings.begin()->print(KeySequence::ForGui, true);
		} else {
			sequence = theTopLevelKeymap().printBindings(func, KeySequence::ForGui);
			seq_untranslated = theTopLevelKeymap().printBindings(func, KeySequence::ForGui, true);
		}
		// QKeySequence returns special characters for keys on the mac
		// Since these are not included in many fonts, we
		// re-translate them to textual names (see #10641)
		odocstringstream ods;
		string const lcode = params_.lang->code();
		docstring trans;
		bool is_translated = sequence != seq_untranslated;
		for (char_type const c : sequence) {
			switch(c) {
			case 0x21b5://Return
				gui = _("Return[[Key]]");
				is_translated = translateString(from_ascii("Return[[Key]]"), trans, lcode);
				ods << trans;
				break;
			case 0x21b9://Tab both directions (Win)
				gui = _("Tab[[Key]]");
				is_translated = translateString(from_ascii("Tab[[Key]]"), trans, lcode);
				ods << trans;
				break;
			case 0x21de://Qt::Key_PageUp
				gui = _("PgUp");
				is_translated = translateString(from_ascii("PgUp"), trans, lcode);
				ods << trans;
				break;
			case 0x21df://Qt::Key_PageDown
				gui = _("PgDown");
				is_translated = translateString(from_ascii("PgDown"), trans, lcode);
				ods << trans;
				break;
			case 0x21e4://Qt::Key_Backtab
				gui = _("Backtab");
				is_translated = translateString(from_ascii("Backtab"), trans, lcode);
				ods << trans;
				break;
			case 0x21e5://Qt::Key_Tab
				gui = _("Tab");
				is_translated = translateString(from_ascii("Tab"), trans, lcode);
				ods << trans;
				break;
			case 0x21e7://Shift
				gui = _("Shift");
				is_translated = translateString(from_ascii("Shift"), trans, lcode);
				ods << trans;
				break;
			case 0x21ea://Qt::Key_CapsLock
				gui = _("CapsLock");
				is_translated = translateString(from_ascii("CapsLock"), trans, lcode);
				ods << trans;
				break;
			case 0x2303://Control
				gui = _("Control[[Key]]");
				is_translated = translateString(from_ascii("Control[[Key]]"), trans, lcode);
				ods << trans;
				break;
			case 0x2318://CMD
				gui = _("Command[[Key]]");
				is_translated = translateString(from_ascii("Command[[Key]]"), trans, lcode);
				ods << trans;
				break;
			case 0x2324://Qt::Key_Enter
				gui = _("Return[[Key]]");
				is_translated = translateString(from_ascii("Return[[Key]]"), trans, lcode);
				ods << trans;
				break;
			case 0x2325://Option key
				gui = _("Option[[Key]]");
				is_translated = translateString(from_ascii("Option[[Key]]"), trans, lcode);
				ods << trans;
				break;
			case 0x2326://Qt::Key_Delete
				gui = _("Delete[[Key]]");
				is_translated = translateString(from_ascii("Delete[[Key]]"), trans, lcode);
				ods << trans;
				break;
			case 0x232b://Qt::Key_Backspace
				gui = _("Fn+Del");
				is_translated = translateString(from_ascii("Fn+Del"), trans, lcode);
				ods << trans;
				break;
			case 0x238b://Qt::Key_Escape
				gui = _("Esc");
				is_translated = translateString(from_ascii("Esc"), trans, lcode);
				ods << trans;
				break;
			default:
				ods.put(c);
			}
		}
		setText(ods.str(), is_translated ? guilang : nullptr);
		params_.force_ltr = !is_translated || (!guilang->rightToLeft() && !params_.lang->rightToLeft());
		break;
	}
	case InsetInfoParams::LYXRC_INFO: {
		// this information could change, if the preferences are changed,
		// so we will recalculate each time through.
		ostringstream oss;
		if (params_.name.empty()) {
			gui = _("undefined");
			info(from_ascii("undefined"), params_.lang);
			break;
		}
		// FIXME this uses the serialization mechanism to get the info
		// we want, which i guess works but is a bit strange.
		lyxrc.write(oss, true, params_.name);
		string result = oss.str();
		if (result.size() < 2) {
			gui = _("undefined");
			info(from_ascii("undefined"), params_.lang);
			break;
		}
		string::size_type loc = result.rfind("\n", result.size() - 2);
		loc = loc == string::npos ? 0 : loc + 1;
		if (result.size() < loc + params_.name.size() + 1
			  || result.substr(loc + 1, params_.name.size()) != params_.name) {
			gui = _("undefined");
			info(from_ascii("undefined"), params_.lang);
			break;
		}
		// remove leading comments and \\name and space
		result = result.substr(loc + params_.name.size() + 2);

		// remove \n and ""
		result = rtrim(result, "\n");
		result = trim(result, "\"");
		gui = _("not set");
		if (result.empty())
			result = "not set";
		setText(from_utf8(result), params_.lang);
		break;
	}
	case InsetInfoParams::PACKAGE_INFO:
		// TODO: away from a release, replace with getPackageInfo.
		// only need to do this once.
		if (initialized_)
			break;
		// check in packages.lst
		bool available;
		// we also allow version check with version separated by blank
		if (contains(params_.name, ' ')) {
			string name;
			string const version = split(params_.name, name, ' ');
			int const y = convert<int>(version.substr(0,4));
			int const m = convert<int>(version.substr(4,2));
			int const d = convert<int>(version.substr(6,2));
			available = LaTeXFeatures::isAvailableAtLeastFrom(name, y, m, d);
		} else
			available = LaTeXFeatures::isAvailable(params_.name);

		if (available) {
			gui = _("yes");
			info(from_ascii("yes"), params_.lang);
		} else {
			gui = _("no");
			info(from_ascii("no"), params_.lang);
		}
		initialized_ = true;
		break;

	case InsetInfoParams::TEXTCLASS_INFO: {
		// TODO: when away from a release, replace with getTextClassInfo.
		// the TextClass can change
		LayoutFileList const & list = LayoutFileList::get();
		bool available = false;
		// params_.name is the class name
		if (list.haveClass(params_.name))
			available = list[params_.name].isTeXClassAvailable();
		if (available) {
			gui = _("yes");
			info(from_ascii("yes"), params_.lang);
		} else {
			gui = _("no");
			info(from_ascii("no"), params_.lang);
		}
		break;
	}
	case InsetInfoParams::MENU_INFO: {
		// only need to do this once.
		if (initialized_)
			break;
		docstring_list names;
		FuncRequest func = lyxaction.lookupFunc(params_.name);
		if (func.action() == LFUN_UNKNOWN_ACTION) {
			gui = _("Unknown action %1$s");
			error(from_ascii("Unknown action %1$s"), params_.lang);
			break;
		}
		if (func.action() == LFUN_BUFFER_VIEW || func.action() == LFUN_BUFFER_UPDATE)
			// The default output format is in the menu without argument,
			// so strip it here.
			if (func.argument() == from_ascii(buffer().params().getDefaultOutputFormat()))
				func = FuncRequest(func.action());
		// iterate through the menubackend to find it
		if (!theApp()) {
			gui = _("Can't determine menu entry for action %1$s in batch mode");
			error(from_ascii("Can't determine menu entry for action %1$s in batch mode"), params_.lang);
			initialized_ = true;
			break;
		}
		// and we will not keep trying if we fail
		initialized_ = theApp()->hasBufferView();
		if (!theApp()->searchMenu(func, names)) {
			gui = _("No menu entry for action %1$s");
			error(from_ascii("No menu entry for action %1$s"), params_.lang);
			break;
		}
		// if found, return its path.
		clear();
		Paragraph & par = paragraphs().front();
		Font const f(inherit_font, guilang);
		params_.force_ltr = !guilang->rightToLeft();
		//Font fu = f;
		//fu.fontInfo().setUnderbar(FONT_ON);
		for (docstring const & name : names) {
			// do not insert > for the top level menu item
			if (&name != &names.front())
				par.insertInset(par.size(), new InsetSpecialChar(InsetSpecialChar::MENU_SEPARATOR),
						f, Change(Change::UNCHANGED));
			//FIXME: add proper underlines here. This
			// involves rewriting searchMenu used above to
			// return a vector of menus. If we do not do
			// that, we might as well use below
			// Paragraph::insert on each string (JMarc)
			for (char_type c : name)
				par.insertChar(par.size(), c, f, Change(Change::UNCHANGED));
		}
		break;
	}
	case InsetInfoParams::L7N_INFO: {
		// TODO: away from a release, use getNormalizedL7N instead.
		docstring locstring = _(params_.name);
		// Remove trailing colons
		locstring = rtrim(locstring, ":");
		// Remove menu accelerators
		if (contains(locstring, from_ascii("|"))) {
			docstring nlocstring;
			rsplit(locstring, nlocstring, '|');
			locstring = nlocstring;
		}
		// Remove Qt accelerators, but keep literal ampersands
		locstring = subst(locstring, from_ascii(" & "), from_ascii("</amp;>"));
		locstring = subst(locstring, from_ascii("&"), docstring());
		locstring = subst(locstring, from_ascii("</amp;>"), from_ascii(" & "));
		setText(locstring, guilang);
		params_.force_ltr = !guilang->rightToLeft() && !params_.lang->rightToLeft();
		break;
	}
	case InsetInfoParams::ICON_INFO: {
		// only need to do this once.
		if (initialized_)
			break;
		// and we will not keep trying if we fail
		initialized_ = true;
		FuncRequest func = lyxaction.lookupFunc(params_.name);
		docstring icon_name = frontend::Application::iconName(func, true);
		FileName file(to_utf8(icon_name));
		if (file.onlyFileNameWithoutExt() == "unknown") {
			string dir = "images";
			FileName file2(imageLibFileSearch(dir, params_.name, "svgz,png"));
			if (!file2.empty())
				file = file2;
		}
		if (!file.exists())
			break;
		int percent_scale = 100;
		if (use_gui) {
			// Compute the scale factor for the icon such that its
			// width on screen is equal to 1em in pixels.
			// The scale factor is rounded to the integer nearest
			// to the float value of the ratio 100*iconsize/imgsize.
			int imgsize = QImage(toqstr(file.absFileName())).width();
			if (imgsize > 0) {
				int iconsize = Length(1, Length::EM).inPixels(1);
				percent_scale = (100 * iconsize + imgsize / 2)/imgsize;
			}
		}
		InsetGraphicsTight * inset = new InsetGraphicsTight(buffer_);
		InsetGraphicsParams igp;
		igp.filename = file;
		igp.lyxscale = percent_scale;
		igp.scale = string();
		igp.width = Length(1, Length::EM);
		if (contains(file.absoluteFilePath(), from_ascii("math"))
		    || contains(file.absoluteFilePath(), from_ascii("ert-insert"))
		    || suffixIs(file.onlyPath().absoluteFilePath(), from_ascii("ipa")))
			igp.darkModeSensitive = true;
		inset->setParams(igp);
		clear();
		Font const f(inherit_font, params_.lang);
		paragraphs().front().insertInset(0, inset, f,
						 Change(Change::UNCHANGED));
		break;
	}
	case InsetInfoParams::BUFFER_INFO: {
		// TODO: away from a release, replace by getBufferInfo.
		// this could all change, so we will recalculate each time
		if (params_.name == "name")
			setText(from_utf8(buffer().fileName().onlyFileName()), params_.lang);
		else if (params_.name == "name-noext")
			setText(from_utf8(buffer().fileName().onlyFileNameWithoutExt()), params_.lang);
		else if (params_.name == "path")
			setText(from_utf8(os::latex_path(buffer().filePath())), params_.lang);
		else if (params_.name == "class")
			setText(from_utf8(buffer().params().documentClass().name()), params_.lang);
		break;
	}
	case InsetInfoParams::VCS_INFO: {
		// TODO: away from a release, replace by getVCSInfo.
		// this information could change, in principle, so we will 
		// recalculate each time through
		if (!buffer().lyxvc().inUse()) {
			gui = _("No version control!");
			info(from_ascii("No version control!"), params_.lang);
			break;
		}
		LyXVC::RevisionInfo itype = LyXVC::Unknown;
		if (params_.name == "revision")
			itype = LyXVC::File;
		else if (params_.name == "revision-abbrev")
			itype = LyXVC::FileAbbrev;
		else if (params_.name == "tree-revision")
			itype = LyXVC::Tree;
		else if (params_.name == "author")
			itype = LyXVC::Author;
		else if (params_.name == "time")
			itype = LyXVC::Time;
		else if (params_.name == "date")
			itype = LyXVC::Date;
		string binfo = buffer().lyxvc().revisionInfo(itype);
		if (binfo.empty()) {
			gui = _("%1$s[[vcs data]] unknown");
			error(from_ascii("%1$s[[vcs data]] unknown"), params_.lang);
		} else
			setText(from_utf8(binfo), params_.lang);
		break;
	}
	case InsetInfoParams::LYX_INFO:
		// only need to do this once.
		if (initialized_)
			break;
		if (params_.name == "version")
			setText(from_ascii(lyx_version), params_.lang);
		else if (params_.name == "layoutformat")
			setText(convert<docstring>(LAYOUT_FORMAT), params_.lang);
		initialized_ = true;
		break;
	case InsetInfoParams::DATE_INFO:
	case InsetInfoParams::MODDATE_INFO:
	case InsetInfoParams::FIXDATE_INFO: {
		// TODO: away from a release, use parseDate instead.
		string date_format = params_.name;
		string const date_specifier = (params_.type == InsetInfoParams::FIXDATE_INFO
					       && contains(params_.name, '@'))
				? split(params_.name, date_format, '@') : string();
		QDate date;
		if (params_.type == InsetInfoParams::MODDATE_INFO)
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
			date = QDateTime::fromSecsSinceEpoch(buffer().fileName().lastModified()).date();
#else
			date = QDateTime::fromTime_t(buffer().fileName().lastModified()).date();
#endif
		else if (params_.type == InsetInfoParams::FIXDATE_INFO && !date_specifier.empty())
			date = QDate::fromString(toqstr(date_specifier), Qt::ISODate);
		else
			date = QDate::currentDate();
		setText(getDate(date_format, date, params_.lang), params_.lang);
		break;
	}
	case InsetInfoParams::TIME_INFO:
	case InsetInfoParams::MODTIME_INFO:
	case InsetInfoParams::FIXTIME_INFO: {
		// TODO: away from a release, use parseTime instead.
		string time_format = params_.name;
		string const time_specifier = (params_.type == InsetInfoParams::FIXTIME_INFO
					       && contains(params_.name, '@'))
				? split(params_.name, time_format, '@') : string();
		QTime time;
		if (params_.type == InsetInfoParams::MODTIME_INFO)
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
			time = QDateTime::fromSecsSinceEpoch(buffer().fileName().lastModified()).time();
#else
			time = QDateTime::fromTime_t(buffer().fileName().lastModified()).time();
#endif
		else if (params_.type == InsetInfoParams::FIXTIME_INFO && !time_specifier.empty())
			time = QTime::fromString(toqstr(time_specifier), Qt::ISODate);
		else
			time = QTime::currentTime();
		setText(getTime(time_format, time, params_.lang), params_.lang);
		break;
	}
	}

	// Just to do something with that string
	LYXERR(Debug::INFO, "info inset text: " << gui);
}


void InsetInfo::validate(LaTeXFeatures & features) const
{
	const_cast<InsetInfo *>(this)->build();
	InsetCollapsible::validate(features);
}


string InsetInfo::contextMenu(BufferView const &, int, int) const
{
	//FIXME: We override the implementation of InsetCollapsible,
	//because this inset is not a collapsible inset.
	return contextMenuName();
}


string InsetInfo::contextMenuName() const
{
	return "context-info";
}

namespace {

// TODO: away from a release, use these functions in InsetInfo::build and InsetInfoParams::getArguments.

std::pair<QDate, std::string> parseDate(Buffer const & buffer, const InsetInfoParams & params) {
	std::string date_format = params.name;
	std::string const date_specifier = (params.type == InsetInfoParams::FIXDATE_INFO
	                                    && contains(params.name, '@'))
	                                   ? split(params.name, date_format, '@') : string();

	QDate date;
	if (params.type == InsetInfoParams::MODDATE_INFO)
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
		date = QDateTime::fromSecsSinceEpoch(buffer.fileName().lastModified()).date();
#else
		date = QDateTime::fromTime_t(buffer.fileName().lastModified()).date();
#endif
	else if (params.type == InsetInfoParams::FIXDATE_INFO && !date_specifier.empty()) {
		QDate date = QDate::fromString(toqstr(date_specifier), Qt::ISODate);
		date = (date.isValid()) ? date : QDate::currentDate();
	} else {
		if (params.type != InsetInfoParams::DATE_INFO && params.type != InsetInfoParams::FIXDATE_INFO)
			lyxerr << "Unexpected InsetInfoParams::info_type in parseDate: " << params.type;
		date = QDate::currentDate();
	}

	return {date, date_format};
}

std::pair<QTime, std::string> parseTime(Buffer const & buffer, const InsetInfoParams & params) {
	std::string time_format = params.name;
	std::string const date_specifier = (params.type == InsetInfoParams::FIXTIME_INFO
	                                    && contains(params.name, '@'))
	                                   ? split(params.name, time_format, '@') : string();

	QTime time;
	if (params.type == InsetInfoParams::MODTIME_INFO)
#if (QT_VERSION >= QT_VERSION_CHECK(5, 8, 0))
		time = QDateTime::fromSecsSinceEpoch(buffer.fileName().lastModified()).time();
#else
		time = QDateTime::fromTime_t(buffer.fileName().lastModified()).time();
#endif
	else if (params.type == InsetInfoParams::FIXTIME_INFO && !date_specifier.empty()) {
		time = QTime::fromString(toqstr(date_specifier), Qt::ISODate);
		time = (time.isValid()) ? time : QTime::currentTime();
	} else {
		if (params.type != InsetInfoParams::TIME_INFO && params.type != InsetInfoParams::FIXTIME_INFO)
			lyxerr << "Unexpected InsetInfoParams::info_type in parseTime: " << params.type;
		time = QTime::currentTime();
	}

	return {time, time_format};
}

docstring getBufferInfo(Buffer const & buffer, const InsetInfoParams & params) {
	if (params.name == "name")
		return from_utf8(buffer.fileName().onlyFileName());
	else if (params.name == "name-noext")
		return from_utf8(buffer.fileName().onlyFileNameWithoutExt());
	else if (params.name == "path")
		return from_utf8(os::latex_path(buffer.filePath()));
	else if (params.name == "class")
		return from_utf8(buffer.params().documentClass().name());
	else {
		lyxerr << "Unexpected name for InsetInfoParams::BUFFER_INFO: " << params.name;
		return from_ascii("");
	}
}

docstring getVCSInfo(Buffer const & buffer, const InsetInfoParams & params) {
	if (!buffer.lyxvc().inUse())
		return _("No version control!");

	LyXVC::RevisionInfo itype = LyXVC::Unknown;
	if (params.name == "revision")
		itype = LyXVC::File;
	else if (params.name == "revision-abbrev")
		itype = LyXVC::FileAbbrev;
	else if (params.name == "tree-revision")
		itype = LyXVC::Tree;
	else if (params.name == "author")
		itype = LyXVC::Author;
	else if (params.name == "time")
		itype = LyXVC::Time;
	else if (params.name == "date")
		itype = LyXVC::Date;

	string binfo = buffer.lyxvc().revisionInfo(itype);
	if (binfo.empty())
		return from_ascii("VCS info unknown!");
	else
		return from_utf8(binfo);
}

docstring getPackageInfo(const InsetInfoParams & params) {
	// check in packages.lst
	bool available;
	// we also allow version check with version separated by blank
	if (contains(params.name, ' ')) {
		string name;
		string const version = split(params.name, name, ' ');
		int const y = convert<int>(version.substr(0,4));
		int const m = convert<int>(version.substr(4,2));
		int const d = convert<int>(version.substr(6,2));
		available = LaTeXFeatures::isAvailableAtLeastFrom(name, y, m, d);
	} else
		available = LaTeXFeatures::isAvailable(params.name);

	return from_ascii(available ? "yes" : "no");
}

docstring getTextClassInfo(const InsetInfoParams & params) {
	LayoutFileList const & list = LayoutFileList::get();
	// params_.name is the class name
	const bool available = list.haveClass(params.name) && list[params.name].isTeXClassAvailable();
	return from_ascii(available ? "yes" : "no");
}

// With C++17, it would be better to have a std::string_view instead of const char *.
const static std::map<char_type, const char *> keyToString {
		{0x21b5, "Return[[Key]]"}, // Return
		{0x21b9, "Tab[[Key]]"}, // Tab both directions (Win)
		{0x21de, "PgUp"}, // Qt::Key_PageUp
		{0x21df, "PgDown"}, // Qt::Key_PageDown
		{0x21e4, "Backtab"}, // Qt::Key_Backtab
		{0x21e5, "Tab"}, // Qt::Key_Tab
		{0x21e7, "Shift"}, // Shift
		{0x21ea, "CapsLock"}, // Qt::Key_CapsLock
		{0x2303, "Control[[Key]]"}, // Control
		{0x2318, "Command[[Key]]"}, // CMD
		{0x2324, "Return[[Key]]"}, // Qt::Key_Enter
		{0x2325, "Option[[Key]]"}, // Option key
		{0x2326, "Delete[[Key]]"}, // Qt::Key_Delete
		{0x232b, "Fn+Del"}, // Qt::Key_Backspace
		{0x238b, "Esc"}, // Qt::Key_Escape
};

bool canTranslateKeySequence(const InsetInfoParams & params, const docstring & sequence,
							 const docstring & seq_untranslated) {
	bool is_translated = sequence != seq_untranslated;
	std::string const lcode = params.lang->code();
	docstring trans;

	for (char_type const c : sequence) {
		const auto keyMapping = keyToString.find(c);
		if (keyMapping != keyToString.end()) {
			is_translated = translateString(from_ascii(keyMapping->second), trans, lcode);
		}
	}

	return is_translated;
}

void docbookShortcutInfo(XMLStream & xs, const InsetInfoParams & params) {
	// Usually, a keyboard shortcut should be encoded as db:shortcut. However, this element doesn't accept text, hence
	// the use of db:accel for error cases (not the right semantics, though).

	std::string attr;
	if (params.type == InsetInfoParams::SHORTCUTS_INFO)
		attr = R"(role="shortcuts")";
	else if (params.type == InsetInfoParams::SHORTCUT_INFO)
		attr = R"(role="shortcut")";
	else {
		// Only check for this assertion that exits this function.
		lyxerr << "Assertion failed! InsetInfoParams::info_type: " << params.type;
		return;
	}

	// shortcuts can change, so we need to re-do this each time
	FuncRequest const func = lyxaction.lookupFunc(params.name);
	if (func.action() == LFUN_UNKNOWN_ACTION) {
		xml::openTag(xs, "accel", attr, "inline");
		xs << _("Unknown action %1$s");
		xml::closeTag(xs, "accel", "inline");
		return;
	}

	KeyMap::Bindings bindings = theTopLevelKeymap().findBindings(func);
	if (bindings.empty()) {
		xml::openTag(xs, "accel", attr, "inline");
		xs << _("undefined");
		xml::closeTag(xs, "accel", "inline");
		return;
	}

	docstring sequence;
	docstring seq_untranslated;
	if (params.type == InsetInfoParams::SHORTCUT_INFO) {
		sequence = bindings.begin()->print(KeySequence::ForGui);
		seq_untranslated = bindings.begin()->print(KeySequence::ForGui, true);
	} else if (params.type == InsetInfoParams::SHORTCUTS_INFO) {
		sequence = theTopLevelKeymap().printBindings(func, KeySequence::ForGui);
		seq_untranslated = theTopLevelKeymap().printBindings(func, KeySequence::ForGui, true);
	}
	// No other possible case.

	Language const * tryguilang = languages.getFromCode(Messages::guiLanguage());
	// Some info insets use the language of the GUI (if available)
	Language const * guilang = tryguilang ? tryguilang : params.lang;
	const bool isTranslated = canTranslateKeySequence(params, sequence, seq_untranslated);
	const bool isLtr = !isTranslated || (!guilang->rightToLeft() && !params.lang->rightToLeft());
	attr += std::string(" dir=\"") + (isLtr ? "ltr" : "rtl") + "\"";
	attr += " action=\"simul\"";
	xml::openTag(xs, "shortcut", attr, "inline");
	xml::openTag(xs, "keycombo", "", "inline");

	// QKeySequence returns special characters for keys on the mac
	// Since these are not included in many fonts, we
	// re-translate them to textual names (see #10641)
	odocstringstream ods;
	string const lcode = params.lang->code();
	docstring trans;
	for (char_type const c : sequence) {
		const auto keyMapping = keyToString.find(c);
		if (keyMapping != keyToString.end()) {
			(void) translateString(from_ascii(keyMapping->second), trans, lcode);

			// db:keysym: symbolic name (like Page Up), unlike what is printed on the key (like
			// ⇞, ↑, ▲, PgUp, Page Up, etc.)
			xml::openTag(xs, "keysym", "", "inline");
			xs << trans;
			xml::closeTag(xs, "keysym", "inline");
		} else {
			// db:keycap: this is not a special key, c is really what is printed on the key.
			xml::openTag(xs, "keycap", "", "inline");
			xs << c;
			xml::closeTag(xs, "keycap", "inline");
		}
	}

	xml::closeTag(xs, "keycombo", "inline");
	xml::closeTag(xs, "shortcut", "inline");
}

void xhtmlShortcutInfo(XMLStream & xs, const InsetInfoParams & params) {
	std::string attr;
	if (params.type == InsetInfoParams::SHORTCUTS_INFO)
		attr = R"(class="shortcuts")";
	else if (params.type == InsetInfoParams::SHORTCUT_INFO)
		attr = R"(class="shortcut")";
	else {
		// Only check for this assertion that exits this function.
		lyxerr << "Assertion failed! InsetInfoParams::info_type: " << params.type;
		return;
	}

	// shortcuts can change, so we need to re-do this each time
	FuncRequest const func = lyxaction.lookupFunc(params.name);
	if (func.action() == LFUN_UNKNOWN_ACTION) {
		xml::openTag(xs, "span", attr, "inline");
		xs << _("Unknown action %1$s");
		xml::closeTag(xs, "span", "inline");
		return;
	}

	KeyMap::Bindings bindings = theTopLevelKeymap().findBindings(func);
	if (bindings.empty()) {
		xml::openTag(xs, "span", attr, "inline");
		xs << _("undefined");
		xml::closeTag(xs, "span", "inline");
		return;
	}

	docstring sequence;
	docstring seq_untranslated;
	if (params.type == InsetInfoParams::SHORTCUT_INFO) {
		sequence = bindings.begin()->print(KeySequence::ForGui);
		seq_untranslated = bindings.begin()->print(KeySequence::ForGui, true);
	} else if (params.type == InsetInfoParams::SHORTCUTS_INFO) {
		sequence = theTopLevelKeymap().printBindings(func, KeySequence::ForGui);
		seq_untranslated = theTopLevelKeymap().printBindings(func, KeySequence::ForGui, true);
	}
	// No other possible case (`return` at the beginning of the function otherwise).

	Language const * tryguilang = languages.getFromCode(Messages::guiLanguage());
	// Some info insets use the language of the GUI (if available)
	Language const * guilang = tryguilang ? tryguilang : params.lang;
	const bool isTranslated = canTranslateKeySequence(params, sequence, seq_untranslated);
	const bool isLtr = !isTranslated || (!guilang->rightToLeft() && !params.lang->rightToLeft());
	attr += std::string(" dir=\"") + (isLtr ? "ltr" : "rtl") + "\"";
	// Use bdo instead of span to specify the text direction (dir is only allowed globally or on bdo).
	xml::openTag(xs, "bdo", attr, "inline");

	// QKeySequence returns special characters for keys on the mac
	// Since these are not included in many fonts, we
	// re-translate them to textual names (see #10641)
	odocstringstream ods;
	string const lcode = params.lang->code();
	docstring trans;
	for (size_t i = 0; i < sequence.length(); ++i) {
	    char_type const c = sequence[i];
		const auto keyMapping = keyToString.find(c);
		if (keyMapping != keyToString.end()) {
			(void) translateString(from_ascii(keyMapping->second), trans, lcode);
			xs << trans;
		} else {
			xs << c;
		}
	}

	xml::closeTag(xs, "bdo", "inline");
}

docstring getLyxRCInfo(const InsetInfoParams & params) {
	if (params.name.empty())
		return _("undefined");

	// this information could change, if the preferences are changed,
	// so we will recalculate each time through.
	// FIXME this uses the serialization mechanism to get the info
	// we want, which i guess works but is a bit strange.
	ostringstream oss;
	lyxrc.write(oss, true, params.name);
	string result = oss.str();
	if (result.size() < 2) {
		return _("undefined");
	}

	string::size_type loc = result.rfind('\n', result.size() - 2);
	loc = loc == string::npos ? 0 : loc + 1;
	if (result.size() < loc + params.name.size() + 1
	    || result.substr(loc + 1, params.name.size()) != params.name) {
		return _("undefined");
	}

	// remove leading comments and \\name and space
	result = result.substr(loc + params.name.size() + 2);

	// remove \n and ""
	result = rtrim(result, "\n");
	result = trim(result, "\"");

	if (result.empty())
		return from_ascii("not set");
	else
		return from_utf8(result);
}

void docbookMenuInfo(XMLStream & xs, Buffer const & buffer, const InsetInfoParams & params) {
	docstring_list names;
	FuncRequest func = lyxaction.lookupFunc(params.name);
	if (func.action() == LFUN_UNKNOWN_ACTION) {
		xml::openTag(xs, "guimenuitem", "", "inline");
		xs << _("Unknown action %1$s");
		xml::closeTag(xs, "guimenuitem", "inline");
		return;
	}

	if (func.action() == LFUN_BUFFER_VIEW || func.action() == LFUN_BUFFER_UPDATE) {
		// The default output format is in the menu without argument,
		// so strip it here.
		if (func.argument() == from_ascii(buffer.params().getDefaultOutputFormat()))
			func = FuncRequest(func.action());
	}

	// iterate through the menubackend to find it
	if (!theApp()) {
		xml::openTag(xs, "guimenuitem", "", "inline");
		xs << _("Can't determine menu entry for action %1$s in batch mode");
		xml::closeTag(xs, "guimenuitem", "inline");
		return;
	}

	// and we will not keep trying if we fail
	if (!theApp()->searchMenu(func, names)) {
		xml::openTag(xs, "guimenuitem", "", "inline");
		xs << _("No menu entry for action %1$s");
		xml::closeTag(xs, "guimenuitem", "inline");
		return;
	}

	// if found, return its path.
	Language const * tryguilang = languages.getFromCode(Messages::guiLanguage());
	// Some info insets use the language of the GUI (if available)
	Language const * guilang = tryguilang ? tryguilang : params.lang;
	const bool isLtr = !guilang->rightToLeft();
	const std::string attr = std::string("dir=\"") + (isLtr ? "ltr" : "rtl") + "\"";

	xml::openTag(xs, "menuchoice", attr, "inline"); // More of an inline tag in this case, as there is no db:shortcut to
	// accompany the succession of menus.

	for (size_t i = 0; i < names.size(); ++i) {
	    docstring const & name = names[i];

		std::string tag;
		if (i == 0) {
			tag = "guimenu";
		} else if (i == names.size() - 1) {
			tag = "guimenuitem";
		} else {
			tag = "guisubmenu";
		}

		xml::openTag(xs, tag, "", "inline");

		//FIXME: add proper underlines here. This
		// involves rewriting searchMenu used above to
		// return a vector of menus. If we do not do
		// that, we might as well use below
		// Paragraph::insert on each string (JMarc)
		// TODO: for DocBook, underlining corresponds to adding db:accel around the letter to underline.
		xs << name;

		xml::closeTag(xs, tag, "inline");
	}

	xml::closeTag(xs, "menuchoice", "inline");
}


void xhtmlMenuInfo(XMLStream & xs, Buffer const & buffer, const InsetInfoParams & params) {
	docstring_list names;
	FuncRequest func = lyxaction.lookupFunc(params.name);
	if (func.action() == LFUN_UNKNOWN_ACTION) {
		xml::openTag(xs, "span", "", "inline");
		xs << _("Unknown action %1$s");
		xml::closeTag(xs, "span", "inline");
		return;
	}

	if (func.action() == LFUN_BUFFER_VIEW || func.action() == LFUN_BUFFER_UPDATE) {
		// The default output format is in the menu without argument,
		// so strip it here.
		if (func.argument() == from_ascii(buffer.params().getDefaultOutputFormat()))
			func = FuncRequest(func.action());
	}

	// iterate through the menubackend to find it
	if (!theApp()) {
		xml::openTag(xs, "span", "", "inline");
		xs << _("Can't determine menu entry for action %1$s in batch mode");
		xml::closeTag(xs, "span", "inline");
		return;
	}

	// and we will not keep trying if we fail
	if (!theApp()->searchMenu(func, names)) {
		xml::openTag(xs, "span", "", "inline");
		xs << _("No menu entry for action %1$s");
		xml::closeTag(xs, "span", "inline");
		return;
	}

	// if found, return its path.
	Language const * tryguilang = languages.getFromCode(Messages::guiLanguage());
	// Some info insets use the language of the GUI (if available)
	Language const * guilang = tryguilang ? tryguilang : params.lang;
	const bool isLtr = !guilang->rightToLeft();
	const std::string attr = std::string("dir=\"") + (isLtr ? "ltr" : "rtl") + "\"";
	// Use bdo instead of span to specify the text direction (dir is only allowed globally or on bdo).
	xml::openTag(xs, "bdo", attr, "inline");

	for (size_t i = 0; i < names.size(); ++i) {
	    docstring const & name = names[i];

		//FIXME: add proper underlines here. This
		// involves rewriting searchMenu used above to
		// return a vector of menus. If we do not do
		// that, we might as well use below
		// Paragraph::insert on each string (JMarc)
		// TODO: for DocBook, underlining corresponds to adding db:accel around the letter to underline.
		xs << name;

		if (i > 0 && i + 1 < names.size())
			xs << "&#x21D2;"; // InsetSpecialChar::MENU_SEPARATOR
	}

	xml::closeTag(xs, "bdo", "inline");
}

void docbookIconInfo(XMLStream & xs, const OutputParams & rp, Buffer * buffer, const InsetInfoParams & params) {
	FuncRequest func = lyxaction.lookupFunc(params.name);
	docstring icon_name = frontend::Application::iconName(func, true);
	FileName file(to_utf8(icon_name));
	if (file.onlyFileNameWithoutExt() == "unknown") {
		std::string dir = "images";
		FileName file2(imageLibFileSearch(dir, params.name, "svgz,png"));
		if (!file2.empty())
			file = file2;
	}

	if (!file.exists())
		return;

	int percent_scale = 100;
	if (use_gui) {
		// Compute the scale factor for the icon such that its
		// width on screen is equal to 1em in pixels.
		// The scale factor is rounded to the integer nearest
		// to the float value of the ratio 100*iconsize/imgsize.
		int imgsize = QImage(toqstr(file.absFileName())).width();
		if (imgsize > 0) {
			int iconsize = Length(1, Length::EM).inPixels(1);
			percent_scale = (100 * iconsize + imgsize / 2) / imgsize;
		}
	}

	InsetGraphicsTight * inset = new InsetGraphicsTight(buffer);
	InsetGraphicsParams igp;
	igp.filename = file;
	igp.lyxscale = percent_scale;
	igp.scale = string();
	igp.width = Length(1, Length::EM);
	if (contains(file.absoluteFilePath(), from_ascii("math"))
	    || contains(file.absoluteFilePath(), from_ascii("ert-insert"))
	    || suffixIs(file.onlyPath().absoluteFilePath(), from_ascii("ipa")))
		igp.darkModeSensitive = true;
	inset->setParams(igp);

	xml::openTag(xs, "guiicon", "", "inline");
	inset->docbook(xs, rp);
	xml::closeTag(xs, "guiicon", "inline");
}

void xhtmlIconInfo(XMLStream & xs, const OutputParams & rp, Buffer * buffer, const InsetInfoParams & params) {
	FuncRequest func = lyxaction.lookupFunc(params.name);
	docstring icon_name = frontend::Application::iconName(func, true);
	FileName file(to_utf8(icon_name));
	if (file.onlyFileNameWithoutExt() == "unknown") {
		std::string dir = "images";
		FileName file2(imageLibFileSearch(dir, params.name, "svgz,png"));
		if (!file2.empty())
			file = file2;
	}

	if (!file.exists())
		return;

	int percent_scale = 100;
	if (use_gui) {
		// Compute the scale factor for the icon such that its
		// width on screen is equal to 1em in pixels.
		// The scale factor is rounded to the integer nearest
		// to the float value of the ratio 100*iconsize/imgsize.
		int imgsize = QImage(toqstr(file.absFileName())).width();
		if (imgsize > 0) {
			int iconsize = Length(1, Length::EM).inPixels(1);
			percent_scale = (100 * iconsize + imgsize / 2) / imgsize;
		}
	}

	InsetGraphicsTight * inset = new InsetGraphicsTight(buffer);
	InsetGraphicsParams igp;
	igp.filename = file;
	igp.lyxscale = percent_scale;
	igp.scale = string();
	igp.width = Length(1, Length::EM);
	if (contains(file.absoluteFilePath(), from_ascii("math"))
	    || contains(file.absoluteFilePath(), from_ascii("ert-insert"))
	    || suffixIs(file.onlyPath().absoluteFilePath(), from_ascii("ipa")))
		igp.darkModeSensitive = true;
	inset->setParams(igp);

	xml::openTag(xs, "span", "class=\"guiicon\"", "inline");
	inset->xhtml(xs, rp);
	xml::closeTag(xs, "span", "inline");
}

docstring getLyXInfo(const InsetInfoParams & params) {
	if (params.name == "version")
		return from_ascii(lyx_version);
	else if (params.name == "layoutformat")
		return convert<docstring>(LAYOUT_FORMAT);
	else {
		lyxerr << "Unexpected name for InsetInfoParams::BUFFER_INFO: " << params.name;
		return from_ascii("");
	}
}

docstring getNormalizedL7N(const InsetInfoParams & params) {
	docstring locstring = _(params.name);

	// Remove trailing colons
	locstring = rtrim(locstring, ":");

	// Remove menu accelerators
	if (contains(locstring, from_ascii("|"))) {
		docstring nlocstring;
		rsplit(locstring, nlocstring, '|');
		locstring = nlocstring;
	}

	// Remove Qt accelerators, but keep literal ampersands
	locstring = subst(locstring, from_ascii(" & "), from_ascii("</amp;>"));
	locstring = subst(locstring, from_ascii("&"), docstring());
	locstring = subst(locstring, from_ascii("</amp;>"), from_ascii(" & "));

	return locstring;
}

} // namespace


void InsetInfo::docbook(XMLStream & xs, OutputParams const & rp) const
{
	// TODO: away from a release, merge some of this code with InsetInfo::build and InsetInfoParams::getArguments.
	switch (params_.type) {
	case InsetInfoParams::DATE_INFO:
	case InsetInfoParams::MODDATE_INFO:
	case InsetInfoParams::FIXDATE_INFO: {
		std::string role;
		switch (params_.type) {
		case InsetInfoParams::DATE_INFO:
			role = "current-date";
			break;
		case InsetInfoParams::MODDATE_INFO:
			role = "last-modification-date";
			break;
		case InsetInfoParams::FIXDATE_INFO:
			role = "fix-date";
			break;
		default:
			lyxerr << "Assertion failed! InsetInfoParams::info_type: " << params().type;
			break;
		}

		// A db:date cannot be nested within a db:date. This case typically happens when the document class defines a
		// Date layout. In this case, avoid outputting a new db:date. This means that InsetInfo cannot add a role on top
		// of the previous db:date, hence add it as a comment. (Another solution would be an XML processing instruction,
		// but this case is not common enough.) Adding the role to the already output tag might have consequences for
		// some document classes where the layout already has a role or uses the same role for another purpose.
		const bool isWithinDate = buffer().getParFromID(rp.lastid).top().paragraph().layout().docbooktag() == "date";

		if (!isWithinDate)
			xml::openTag(xs, "date", "role=\"" + role + "\"", "inline");
		else
			xs << XMLStream::ESCAPE_NONE << from_ascii(std::string("<!-- ") + role + " -->");
		xs << qstring_to_ucs4(std::get<0>(parseDate(buffer(), params_)).toString(Qt::ISODate));
		if (!isWithinDate)
			xml::closeTag(xs, "date", "inline");
		break;
	}

	case InsetInfoParams::TIME_INFO:
	case InsetInfoParams::MODTIME_INFO:
	case InsetInfoParams::FIXTIME_INFO: {
		std::string role;
		switch (params_.type) {
		case InsetInfoParams::TIME_INFO:
			role = "current-time";
			break;
		case InsetInfoParams::MODTIME_INFO:
			role = "last-modification-time";
			break;
		case InsetInfoParams::FIXTIME_INFO:
			role = "fix-time";
			break;
		default:
			lyxerr << "Assertion failed! InsetInfoParams::info_type: " << params().type;
			break;
		}

		// DocBook has no specific element for time, so use a date.
		// See the discussion above (DATE_INFO, MODDATE_INFO, and FIXDATE_INFO) for a discussion about the choices that
		// have been made.
		const bool isWithinDate = buffer().getParFromID(rp.lastid).top().paragraph().layout().docbooktag() == "date";

		if (!isWithinDate)
			xml::openTag(xs, "date", "role=\"" + role + "\"", "inline");
		else
			xs << XMLStream::ESCAPE_NONE << from_ascii(std::string("<!-- ") + role + " -->");
		xs << qstring_to_ucs4(std::get<0>(parseTime(buffer(), params_)).toString(Qt::ISODate));
		if (!isWithinDate)
			xml::closeTag(xs, "date", "inline");
		break;
	}

	case InsetInfoParams::BUFFER_INFO:
		xml::openTag(xs, "phrase", "role=\"buffer-info " + params_.name + "\"", "inline");
		xs << getBufferInfo(buffer(), params_);
		xml::closeTag(xs, "phrase", "inline");
		break;
	case InsetInfoParams::VCS_INFO:
		xml::openTag(xs, "phrase", "role=\"vcs-info " + params_.name + "\"", "inline");
		xs << getVCSInfo(buffer(), params_);
		xml::closeTag(xs, "phrase", "inline");
		break;
	case InsetInfoParams::PACKAGE_INFO:
		xml::openTag(xs, "phrase", "role=\"package-availability " + params_.name + "\"", "inline");
		xs << getPackageInfo(params_);
		xml::closeTag(xs, "phrase", "inline");
		break;
	case InsetInfoParams::TEXTCLASS_INFO:
		xml::openTag(xs, "phrase", "role=\"textclass-availability " + params_.name + "\"", "inline");
		xs << getTextClassInfo(params_);
		xml::closeTag(xs, "phrase", "inline");
		break;

	case InsetInfoParams::SHORTCUTS_INFO:
	case InsetInfoParams::SHORTCUT_INFO:
		docbookShortcutInfo(xs, params_);
		break;
		
	case InsetInfoParams::LYXRC_INFO:
		xml::openTag(xs, "phrase", "role=\"lyxrc-entry " + params_.name + "\"", "inline");
		xs << getLyxRCInfo(params_);
		xml::closeTag(xs, "phrase", "inline");
		break;

	case InsetInfoParams::MENU_INFO:
		docbookMenuInfo(xs, buffer(), params_);
		break;
	case InsetInfoParams::ICON_INFO:
		docbookIconInfo(xs, rp, buffer_, params_);
		break;
	case InsetInfoParams::LYX_INFO:
		xml::openTag(xs, "phrase", "role=\"lyx-info " + params_.name + "\"", "inline");
		xs << getLyXInfo(params_);
		xml::closeTag(xs, "phrase", "inline");
		break;

	case InsetInfoParams::L7N_INFO:
		// TODO: add "its:translate="no"" in the attributes if ITS is globally enabled for LyX (quite rare to have ITS
		// for DocBook documents).
		xml::openTag(xs, "phrase", R"(role="localized")", "inline");
		xs << getNormalizedL7N(params_);
		xml::closeTag(xs, "phrase", "inline");
		break;

	case InsetInfoParams::UNKNOWN_INFO:
		xml::openTag(xs, "phrase", R"(role="unknown")", "inline");
		xs << from_ascii("Unknown Info!");
		xml::closeTag(xs, "phrase", "inline");
		break;
	default:
		lyxerr << "Unrecognised InsetInfoParams::info_type: " << params().type;

		xml::openTag(xs, "phrase", R"(role="unrecognized")", "inline");
		xs << from_ascii("Unrecognized Info!");
		xml::closeTag(xs, "phrase", "inline");
		break;
	}
}


docstring InsetInfo::xhtml(XMLStream & xs, OutputParams const & rp) const
{
	// TODO: away from a release, merge some of this code with InsetInfo::build and InsetInfoParams::getArguments.
	switch (params_.type) {
	case InsetInfoParams::DATE_INFO:
	case InsetInfoParams::MODDATE_INFO:
	case InsetInfoParams::FIXDATE_INFO: {
		std::string cssClass;
		switch (params_.type) {
		case InsetInfoParams::DATE_INFO:
			cssClass = "current-date";
			break;
		case InsetInfoParams::MODDATE_INFO:
			cssClass = "last-modification-date";
			break;
		case InsetInfoParams::FIXDATE_INFO:
			cssClass = "fix-date";
			break;
		default:
			lyxerr << "Assertion failed! InsetInfoParams::info_type: " << params().type;
			break;
		}

		QDate date;
		std::string date_format;
		std::tie(date, date_format) = parseDate(buffer(), params_);

		xml::openTag(xs, "span", std::string("class=\"infodate-") + cssClass + "\"", "inline");
		xs << getDate(date_format, date, params_.lang);
		xml::closeTag(xs, "span", "inline");
		break;
	}

	case InsetInfoParams::TIME_INFO:
	case InsetInfoParams::MODTIME_INFO:
	case InsetInfoParams::FIXTIME_INFO: {
		std::string cssClass;
		switch (params_.type) {
		case InsetInfoParams::TIME_INFO:
			cssClass = "current-time";
			break;
		case InsetInfoParams::MODTIME_INFO:
			cssClass = "last-modification-time";
			break;
		case InsetInfoParams::FIXTIME_INFO:
			cssClass = "fix-time";
			break;
		default:
			lyxerr << "Assertion failed! InsetInfoParams::info_type: " << params().type;
			break;
		}

		QTime time;
		std::string time_format;
		std::tie(time, time_format) = parseTime(buffer(), params_);

		xml::openTag(xs, "span", std::string("class=\"infotime-") + cssClass + "\"", "inline");
		xs << getTime(time_format, time, params_.lang);
		xml::closeTag(xs, "span", "inline");
		break;
	}

	case InsetInfoParams::BUFFER_INFO:
		xml::openTag(xs, "span", "class=\"buffer-info " + params_.name + "\"", "inline");
		xs << getBufferInfo(buffer(), params_);
		xml::closeTag(xs, "span", "inline");
		break;
	case InsetInfoParams::VCS_INFO:
		xml::openTag(xs, "span", "class=\"vcs-info " + params_.name + "\"", "inline");
		xs << getVCSInfo(buffer(), params_);
		xml::closeTag(xs, "span", "inline");
		break;
	case InsetInfoParams::PACKAGE_INFO:
		xml::openTag(xs, "span", "class=\"package-availability " + params_.name + "\"", "inline");
		xs << getPackageInfo(params_);
		xml::closeTag(xs, "span", "inline");
		break;
	case InsetInfoParams::TEXTCLASS_INFO:
		xml::openTag(xs, "span", "class=\"textclass-availability " + params_.name + "\"", "inline");
		xs << getTextClassInfo(params_);
		xml::closeTag(xs, "span", "inline");
		break;

	case InsetInfoParams::SHORTCUTS_INFO:
	case InsetInfoParams::SHORTCUT_INFO:
		xhtmlShortcutInfo(xs, params_);
		break;

	case InsetInfoParams::LYXRC_INFO:
		xml::openTag(xs, "span", "class=\"lyxrc-entry " + params_.name + "\"", "inline");
		xs << getLyxRCInfo(params_);
		xml::closeTag(xs, "span", "inline");
		break;

	case InsetInfoParams::MENU_INFO:
		xhtmlMenuInfo(xs, buffer(), params_);
		break;
	case InsetInfoParams::ICON_INFO:
		xhtmlIconInfo(xs, rp, buffer_, params_);
		break;
	case InsetInfoParams::LYX_INFO:
		xml::openTag(xs, "span", "class=\"lyx-info " + params_.name + "\"", "inline");
		xs << getLyXInfo(params_);
		xml::closeTag(xs, "span", "inline");
		break;

	case InsetInfoParams::L7N_INFO:
		xml::openTag(xs, "span", R"(class="localized" translate="no")", "inline");
		xs << getNormalizedL7N(params_);
		xml::closeTag(xs, "span", "inline");
		break;

	case InsetInfoParams::UNKNOWN_INFO:
		xml::openTag(xs, "span", R"(class="unknown")", "inline");
		xs << from_ascii("Unknown Info!");
		xml::closeTag(xs, "span", "inline");
		break;
	default:
		lyxerr << "Unrecognised InsetInfoParams::info_type: " << params().type;

		xml::openTag(xs, "span", R"(class="unrecognized")", "inline");
		xs << from_ascii("Unrecognized Info!");
		xml::closeTag(xs, "span", "inline");
		break;
	}

	return from_ascii("");
}


} // namespace lyx
