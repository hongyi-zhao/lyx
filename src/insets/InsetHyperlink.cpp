/**
 * \file InsetHyperlink.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author José Matos
 * \author Uwe Stöhr
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>
#include "InsetHyperlink.h"

#include "Buffer.h"
#include "Format.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "LaTeXFeatures.h"
#include "LyX.h"
#include "Statistics.h"
#include "texstream.h"
#include "xml.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/lstrings.h"
#include "support/qstring_helpers.h"

#include <QtGui/QDesktopServices>
#include <QUrl>

using namespace std;
using namespace lyx::support;

namespace lyx {


InsetHyperlink::InsetHyperlink(Buffer * buf, InsetCommandParams const & p)
	: InsetCommand(buf, p)
{}


ParamInfo const & InsetHyperlink::findInfo(string const & /* cmdName */)
{
	static ParamInfo param_info_;
	if (param_info_.empty()) {
		param_info_.add("name", ParamInfo::LATEX_OPTIONAL,
		                ParamInfo::HANDLING_LATEXIFY);
		param_info_.add("target", ParamInfo::LATEX_REQUIRED);
		param_info_.add("type", ParamInfo::LATEX_REQUIRED);
		param_info_.add("literal", ParamInfo::LYX_INTERNAL);
	}
	return param_info_;
}


docstring InsetHyperlink::screenLabel() const
{
	// TODO: replace with unicode hyperlink character = U+1F517
	docstring const temp = _("Hyperlink: ");

	docstring url;

	url += getParam("name");
	if (url.empty()) {
		url += getParam("target");

		// elide if long and no name was provided
		if (url.length() > 30) {
			docstring end = url.substr(url.length() - 17, url.length());
			support::truncateWithEllipsis(url, 13);
			url += end;
		}
	} else {
		// elide if long (approx number of chars in line of article class)
		if (url.length() > 80) {
			docstring end = url.substr(url.length() - 67, url.length());
			support::truncateWithEllipsis(url, 13);
			url += end;
		}
	}
	return temp + url;
}


void InsetHyperlink::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	// Ctrl + click: open hyperlink
	if (cmd.action() == LFUN_MOUSE_RELEASE && cmd.modifier() == ControlModifier) {
		lyx::dispatch(FuncRequest(LFUN_INSET_EDIT));
		return;
	}

	switch (cmd.action()) {

	case LFUN_INSET_EDIT:
		viewTarget();
		break;

	default:
		InsetCommand::doDispatch(cur, cmd);
		break;
	}
}


bool InsetHyperlink::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	switch (cmd.action()) {
	case LFUN_INSET_EDIT: {
		docstring const & utype = getParam("type");
		QUrl url(toqstr(getParam("target")),QUrl::StrictMode);
		bool url_valid = utype.empty() && url.isValid();
		flag.setEnabled(url_valid || utype == "file:");
		return true;
		}

	default:
		return InsetCommand::getStatus(cur, cmd, flag);
	}
}


void InsetHyperlink::viewTarget() const
{
	if (getParam("type").empty()) { //==Web
		QUrl url(toqstr(getParam("target")),QUrl::StrictMode);
		if (!QDesktopServices::openUrl(url))
			LYXERR0("Unable to open URL!");
	} else if (getParam("type") == "file:") {
		FileName url = makeAbsPath(to_utf8(getParam("target")), buffer().filePath());
		string const format = theFormats().getFormatFromFile(url);
		theFormats().view(buffer(), url, format);
	}
}


docstring makeURL(docstring const & url, docstring const & type) {
	if (type == "other" ||
			(!type.empty() && url.find(type) == 0))
		return url;
	return type + url;
}


void InsetHyperlink::latex(otexstream & os,
			   OutputParams const & runparams) const
{
	docstring url   = getParam("target");
	docstring name  = getParam("name");
	docstring const & utype = getParam("type");
	static char_type const chars_url[2] = {'%', '#'};

	// For the case there is no name given, the target is set as name.
	// Do this before !url.empty() and !name.empty() to handle characters
	// such as % correctly.
	if (name.empty())
		name = url;

	if (!url.empty()) {
		// Use URI/URL-style percent-encoded string (hexadecimal).
		// We exclude some characters that must not be transformed
		// in hrefs: % # / : ? = & ! * ' ( ) ; @ + $ , [ ]
		// or that we need to treat manually: \.
		url = to_percent_encoding(url, from_ascii("%#\\/:?=&!*'();@+$,[]"));
		// We handle \ manually since \\ is valid
		for (size_t i = 0, pos;
			(pos = url.find('\\', i)) != string::npos;
			i = pos + 2) {
			if (url[pos + 1] != '\\')
				url.replace(pos, 1, from_ascii("%5C"));
		}

		// The characters in chars_url[] need to be escaped in the url
		// field because otherwise LaTeX will fail when the hyperlink is
		// within an argument of another command, e.g. in a \footnote. It
		// is important that they are escaped as "\#" and not as "\#{}".
		// FIXME this is not necessary in outside of commands.
		for (int k = 0; k < 2; k++)
			for (size_t i = 0, pos;
				(pos = url.find(chars_url[k], i)) != string::npos;
				i = pos + 2)
				url.replace(pos, 1, from_ascii("\\") + chars_url[k]);

		// add "http://" when the type is web (type = empty)
		// and no "://" is given
		if (url.find(from_ascii("://")) == string::npos
			&& utype.empty())
			url = from_ascii("http://") + url;

	} // end if (!url.empty())

	if (!name.empty()) {
		name = params().prepareCommand(runparams, name,
					ParamInfo::HANDLING_LATEXIFY);
		// replace the tilde by the \sim character as suggested in the
		// LaTeX FAQ for URLs
		if (getParam("literal") != "true") {
			docstring const sim = from_ascii("$\\sim$");
			for (size_t i = 0, pos;
				(pos = name.find('~', i)) != string::npos;
				i = pos + 1)
				name.replace(pos, 1, sim);
		}
	}

	if (runparams.moving_arg)
		os << "\\protect";

	// output the ready \href command
	os << "\\href{" << makeURL(url, utype) << "}{" << name << '}';
}


int InsetHyperlink::plaintext(odocstringstream & os,
        OutputParams const &, size_t) const
{
	odocstringstream oss;

	oss << '[' << getParam("target");
	if (getParam("name").empty())
		oss << ']';
	else
		oss << "||" << getParam("name") << ']';

	docstring const str = oss.str();
	os << str;
	return str.size();
}


void InsetHyperlink::docbook(XMLStream & xs, OutputParams const &) const
{
	docstring target = subst(getParam("target"), from_ascii("&"), from_ascii("&amp;")) ;
	xs << xml::StartTag("link", "xlink:href=\"" + makeURL(target, getParam("type")) + "\"");
	xs << xml::escapeString(getParam("name"));
	xs << xml::EndTag("link");
}


docstring InsetHyperlink::xhtml(XMLStream & xs, OutputParams const &) const
{
	docstring const & target =
		xml::escapeString(getParam("target"), XMLStream::ESCAPE_AND);
	docstring const & name = getParam("name");
	xs << xml::StartTag("a", to_utf8("href=\"" + makeURL(target, getParam("type")) + "\""));
	xs << (name.empty() ? target : name);
	xs << xml::EndTag("a");
	return docstring();
}


void InsetHyperlink::toString(odocstream & os) const
{
	odocstringstream ods;
	plaintext(ods, OutputParams(0), INT_MAX);
	os << ods.str();
}


void InsetHyperlink::forOutliner(docstring & os, size_t const, bool const) const
{
	docstring const & n = getParam("name");
	if (!n.empty()) {
		os += n;
		return;
	}
	os += getParam("target");
}


docstring InsetHyperlink::toolTip(BufferView const & /*bv*/, int /*x*/, int /*y*/) const
{
	docstring const & url = getParam("target");
	docstring const & type = getParam("type");
	docstring guitype = _("www");
	if (type == "mailto:")
		guitype = _("email");
	else if (type == "file:")
		guitype = _("file");
	else if (type == "other")
		guitype = _("other[[Hyperlink Type]]");
	return bformat(_("Hyperlink (%1$s) to %2$s"), guitype, url);
}


void InsetHyperlink::validate(LaTeXFeatures & features) const
{
	features.require("hyperref");
	InsetCommand::validate(features);
}


void InsetHyperlink::updateStatistics(Statistics & stats) const
{
	stats.update(getParam("name"));
}


string InsetHyperlink::contextMenuName() const
{
	return "context-hyperlink";
}


} // namespace lyx
