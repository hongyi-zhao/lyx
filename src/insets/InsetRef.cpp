/**
 * \file InsetRef.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author José Matos
 *
 * Full author contact details are available in file CREDITS.
 */
#include <config.h>

#include "InsetRef.h"

#include "Buffer.h"
#include "BufferList.h"
#include "BufferParams.h"
#include "Cursor.h"
#include "FuncStatus.h"
#include "InsetLabel.h"
#include "Language.h"
#include "LaTeXFeatures.h"
#include "LyX.h"
#include "output_xhtml.h"
#include "Paragraph.h"
#include "ParIterator.h"
#include "PDFOptions.h"
#include "Statistics.h"
#include "xml.h"
#include "texstream.h"
#include "TocBackend.h"

#include "Session.h"

#include "frontends/alert.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/FileName.h"
#include "support/filetools.h"
#include "support/gettext.h"
#include "support/lstrings.h"
#include "support/Messages.h"
#include "support/textutils.h"

using namespace lyx::support;
using namespace std;

namespace lyx {


InsetRef::InsetRef(Buffer * buf, InsetCommandParams const & p)
	: InsetCommand(buf, p), broken_(false), active_(true)
{}


InsetRef::InsetRef(InsetRef const & ir)
	: InsetCommand(ir), broken_(false), active_(true)
{}


bool InsetRef::isCompatibleCommand(string const & s) {
	//FIXME This is likely not the best way to handle this.
	//But this stuff is hardcoded elsewhere already.
	return s == "ref"
		|| s == "pageref"
		|| s == "cpageref"
		|| s == "vref"
		|| s == "vpageref"
		|| s == "formatted"
		|| s == "prettyref" // for InsetMathRef FIXME
		|| s == "eqref"
		|| s == "nameref"
		|| s == "labelonly";
}


ParamInfo const & InsetRef::findInfo(string const & /* cmdName */)
{
	static ParamInfo param_info_;
	if (param_info_.empty()) {
		param_info_.add("name", ParamInfo::LATEX_OPTIONAL);
		param_info_.add("reference", ParamInfo::LATEX_REQUIRED,
				ParamInfo::HANDLING_ESCAPE);
		param_info_.add("plural", ParamInfo::LYX_INTERNAL);
		param_info_.add("caps", ParamInfo::LYX_INTERNAL);
		param_info_.add("noprefix", ParamInfo::LYX_INTERNAL);
		param_info_.add("nolink", ParamInfo::LYX_INTERNAL);
		param_info_.add("options", ParamInfo::LYX_INTERNAL);
		param_info_.add("tuple", ParamInfo::LYX_INTERNAL);
		param_info_.add("filenames", ParamInfo::LYX_INTERNAL);
	}
	return param_info_;
}


docstring InsetRef::layoutName() const
{
	return from_ascii("Ref");
}


void InsetRef::changeTarget(docstring const & new_label)
{
	// With change tracking, we insert a new ref
	// and delete the old one
	if (buffer().masterParams().track_changes) {
		InsetCommandParams icp(REF_CODE, "ref");
		icp["reference"] = new_label;
		string const data = InsetCommand::params2string(icp);
		lyx::dispatch(FuncRequest(LFUN_INSET_INSERT, data));
		lyx::dispatch(FuncRequest(LFUN_CHAR_DELETE_FORWARD));
	} else
		setParam("reference", new_label);
}



void InsetRef::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	// Ctrl + click: go to label
	if (cmd.action() == LFUN_MOUSE_RELEASE && cmd.modifier() == ControlModifier) {
			lyx::dispatch(FuncRequest(LFUN_BOOKMARK_SAVE, "0"));
			lyx::dispatch(FuncRequest(LFUN_LABEL_GOTO, getParam("reference")));
			return;
		}

	string const inset = cmd.getArg(0);
	string const arg   = cmd.getArg(1);
	string pstring;
	if (cmd.action() == LFUN_INSET_MODIFY && inset == "ref") {
		if (arg == "toggle-plural")
			pstring = "plural";
		else if (arg == "toggle-caps")
			pstring = "caps";
		else if (arg == "toggle-noprefix")
			pstring = "noprefix";
		else if (arg == "toggle-nolink")
			pstring = "nolink";
		else if (arg == "changetarget") {
			string const oldtarget = cmd.getArg(2);
			string const newtarget = cmd.getArg(3);
			if (!oldtarget.empty() && !newtarget.empty()
			    && getParam("reference") == from_utf8(oldtarget))
				changeTarget(from_utf8(newtarget));
			cur.forceBufferUpdate();
			return;
		}
	}

	// otherwise not for us
	if (pstring.empty()) {
		InsetCommand::doDispatch(cur, cmd);
		if (cmd.action() == LFUN_INSET_MODIFY && cmd.getArg(0) == "changetype")
			// store last used type in session
			theSession().uiSettings().insert("default_crossreftype", getCmdName());
		return;
	}

	bool const isSet = (getParam(pstring) == "true");
	setParam(pstring, from_ascii(isSet ? "false"  : "true"));
	cur.forceBufferUpdate();
}


bool InsetRef::getStatus(Cursor & cur, FuncRequest const & cmd,
	FuncStatus & status) const
{
	if (cmd.action() != LFUN_INSET_MODIFY)
		return InsetCommand::getStatus(cur, cmd, status);

	// this is not allowed
	if ((cmd.argument() == "changetype vref" || cmd.argument() == "changetype vpageref")
	     && getLabels().size() > 2) {
		status.setEnabled(false);
		return true;
	}
	if (cmd.argument() == "changetype cpageref"
	     && buffer().params().xref_package != "cleveref"
	     && buffer().params().xref_package != "zref") {
		status.setEnabled(false);
		return true;
	}

	if (cmd.getArg(0) != "ref")
		return InsetCommand::getStatus(cur, cmd, status);

	string const arg = cmd.getArg(1);
	string pstring;
	if (arg == "changetarget")
		return true;
	if (arg == "toggle-plural")
		pstring = "plural";
	else if (arg == "toggle-caps")
		pstring = "caps";
	if (!pstring.empty()) {
		status.setEnabled(buffer().params().xref_package == "refstyle" &&
			params().getCmdName() == "formatted");
		bool const isSet = (getParam(pstring) == "true");
		status.setOnOff(isSet);
		return true;
	}
	if (arg == "toggle-noprefix") {
		status.setEnabled(params().getCmdName() == "labelonly");
		bool const isSet = (getParam("noprefix") == "true");
		status.setOnOff(isSet);
		return true;
	}
	if (arg == "toggle-nolink") {
		status.setEnabled(params().getCmdName() != "formatted" && params().getCmdName() != "labelonly");
		bool const isSet = (getParam("nolink") == "true");
		status.setOnOff(isSet);
		return true;
	}
	// otherwise not for us
	return InsetCommand::getStatus(cur, cmd, status);
}


// the ref argument is the label name we are referencing.
// we expect ref to be in the form: pfx:suffix.
//
// if it isn't, then we can't produce a formatted reference,
// so we return "\ref" and put ref into label.
//
// for refstyle, we return "\pfxcmd", and put suffix into
// label and pfx into prefix. this is because refstyle expects
// the command: \pfxcmd{suffix}.
//
// for prettyref, we return "\prettyref" and put ref into label
// and pfx into prefix. this is because prettyref uses the whole
// label, thus: \prettyref{pfx:suffix}.
//
docstring InsetRef::getFormattedCmd(docstring const & ref,
	vector<docstring> & label, docstring & prefix, string const xref_package,
	bool use_caps, bool use_range)
{
	docstring defcmd = from_ascii("\\ref");
	if (xref_package == "cleveref") {
		defcmd = use_caps ? from_ascii("\\Cref") : from_ascii("\\cref");
		if (use_range)
			defcmd += "range";
	} else if (xref_package == "zref")
		defcmd = from_ascii("\\zcref");
	else if (xref_package == "prettyref")
		defcmd = from_ascii("\\prettyref");

	bool have_cmd = false;
	bool have_prefix = false;
	vector<docstring> refs = getVectorFromString(ref);
	for (auto const & r : refs ) {
		docstring pr;
		docstring l = split(r, pr, ':');

		// except for cleveref and zref, we have to have xxx:xxxxx...
		if (l.empty()) {
			if (xref_package != "cleveref" && xref_package != "zref")
				LYXERR0("Label `" << r << "' contains no `:' separator.");
			label.push_back(r);
			if (!have_prefix)
				prefix = from_ascii("");
			have_cmd = true;
			continue;
		}
	
		if (pr.empty()) {
			// we have ":xxxx"
			if (xref_package != "cleveref" && xref_package != "zref")
				LYXERR0("Label `" << ref << "' contains nothing before `:'.");
			label.push_back(r);
			have_cmd = true;
			continue;
		}
	
		if (xref_package != "refstyle") {
			// \prettyref uses the whole label
			label.push_back(r);
			have_cmd = true;
			continue;
		}
	
		// make sure the prefix is legal for a latex command
		size_t const len = pr.size();
		bool invalid_prefix = false;
		for (size_t i = 0; i < len; i++) {
			char_type const c = pr[i];
			if (!isAlphaASCII(c)) {
				LYXERR0("Prefix `" << prefix << "' is invalid for LaTeX.");
				// restore the label
				label.push_back(r);
				invalid_prefix = true;
			}
		}
		if (!have_prefix && !invalid_prefix) {
			prefix = pr;
			have_prefix = true;
		}
		if (!invalid_prefix)
			label.push_back(l);
	}
	if (have_cmd)
		return defcmd;

	if (prefix.empty()) {
		label = getVectorFromString(ref);
		return defcmd;
	}

	if (use_caps) {
		prefix = support::capitalize(prefix);
	}
	defcmd = from_ascii("\\") + prefix + from_ascii("ref");
	return (use_range) ? from_ascii("\\") + prefix + from_ascii("rangeref")
			   : from_ascii("\\") + prefix + from_ascii("ref");
}


docstring InsetRef::getEscapedLabel(OutputParams const & rp) const
{
	InsetCommandParams const & p = params();
	ParamInfo const & pi = p.info();
	ParamInfo::ParamData const & pd = pi["reference"];
	return p.prepareCommand(rp, getParam("reference"), pd.handling());
}


void InsetRef::latex(otexstream & os, OutputParams const & rp) const
{
	string const & cmd = getCmdName();
	docstring const & data = getEscapedLabel(rp);
	vector<docstring> labels = getVectorFromString(data);
	int const nlabels = labels.size();
	bool const hyper_on = buffer().masterParams().pdfoptions().use_hyperref;
	bool const use_nolink = hyper_on && getParam("nolink") == "true";
	bool const use_caps   = getParam("caps") == "true";
	bool const use_plural = getParam("plural") == "true";
	bool const use_cleveref = buffer().masterParams().xref_package == "cleveref";
	bool const use_zref = buffer().masterParams().xref_package == "zref";

	if (rp.inulemcmd > 0)
		os << "\\mbox{";

	if (buffer().masterParams().xref_package == "refstyle" && cmd == "eqref") {
		// we advertise this as printing "(n)", so we'll do that, at least
		// for refstyle, since refstlye's own \eqref prints, by default,
		// "equation n". if one wants \eqref, one can get it by using a
		// formatted label in this case.
		os << '(' << from_ascii("\\ref")   +
			// no hyperlink version?
			(use_nolink ? from_utf8("*") : from_utf8("")) +
			from_ascii("{") << data << from_ascii("})");
	} else if (cmd == "formatted") {
		vector<docstring> flabels;
		docstring prefix;
		os << getFormattedCmd(data, flabels, prefix, buffer().masterParams().xref_package, use_caps, useRange());;
		if ((use_cleveref || use_zref) && use_nolink)
			os << "*";
		if (buffer().masterParams().xref_package == "refstyle" && use_plural)
			os << "[s]";
		else if (use_zref) {
			docstring opts = getParam("options");
			if (use_caps) {
				if (!opts.empty())
					opts +=", ";
				opts += "S";
			}
			if (useRange()) {
				if (!opts.empty())
					opts +=", ";
				opts += "range";
			}
			if (!opts.empty())
				os << "[" << opts << "]";
		}
		os << "{";
		bool first = true;
		vector<docstring>::const_iterator it = flabels.begin();
		vector<docstring>::const_iterator en = flabels.end();
		for (size_t i = 0; it != en; ++it, ++i) {
			if (!first) {
				if (buffer().masterParams().xref_package == "prettyref") {
					if (!first) {
						os << "}";
						if (flabels.size() == 2)
							os << buffer().B_("[[reference 1]] and [[reference2]]");
						else if (i > 0 && i == flabels.size() - 1)
							os << buffer().B_("[[reference 1, ...]], and [[reference n]]");
						else
							os << buffer().B_("[[reference 1]], [[reference2, ...]]");
					}
					os << "\\ref{";
				} else if (useRange() && !use_zref)
					os << "}{";
				else
					os << ",";
			}
			if (contains(*it, ' ') && buffer().masterParams().xref_package != "prettyref")
				// refstyle bug: labels with blanks need to be grouped
				// otherwise the blanks will be gobbled
				os << "{" << *it << "}";
			else {
				if (buffer().masterParams().xref_package == "prettyref" && !contains(*it, ':'))
					// warn on invalid label
					frontend::Alert::warning(_("Invalid label!"),
								 bformat(_("The label `%1$s' does not have a prefix (e.g., `sec:'), "
									   "which is needed for formatted references with prettyref.\n"
									   "You will most likely run into a LaTeX error."),
									 *it), true);
				os << *it;
			}
			first = false;
		}
		os << "}";
	} else if (cmd == "nameref" && use_cleveref && (use_nolink || !hyper_on)) {
		docstring const crefcmd = use_caps ? from_ascii("Cref") : from_ascii("cref");
		os << "\\name" << crefcmd;
		if (use_plural)
			os << "s";
		os << '{' << data << '}';
	} else if (cmd == "labelonly") {
		docstring const & ref = getParam("reference");
		if (getParam("noprefix") != "true")
			os << ref;
		else {
			docstring prefix;
			vector <docstring> slrefs;
			for (auto const & r : labels) {
				docstring suffix = split(r, prefix, ':');
				if (suffix.empty()) {
					LYXERR0("Label `" << r << "' contains no `:' separator.");
					slrefs.push_back(r);
				} else {
					slrefs.push_back(suffix);
				}
			}
			os << getStringFromVector(slrefs);
		}
	} else if (cmd == "vref" || cmd == "vpageref") {
		os << "\\";
		if (use_zref)
			os << "z";
		os << cmd;
		if (useRange())
			os << "range";
		if (use_nolink)
			os << "*";
		docstring opts = getParam("options");
		if (use_zref && use_caps) {
			if (!opts.empty())
				opts +=", ";
			opts += "S";
		}
		if (use_zref && !opts.empty())
			os << "[" << opts << "]";
		bool first = true;
		os << "{";
		for (auto const & l : labels) {
			if (!first) {
				if (useRange())
					os << "}{";
				else
					os << ",";
			}
			os << l;
			first = false;
		}
		os << "}";
	} else if (cmd == "cpageref" && use_cleveref) {
		if (use_caps)
			os << "\\Cpageref";
		else
			os << "\\cpageref";
		if (useRange())
			os << "range";
		bool first = true;
		os << "{";
		for (auto const & l : labels) {
			if (!first) {
				if (useRange())
					os << "}{";
				else
					os << ",";
			}
			os << l;
			first = false;
		}
		os << "}";
	} else if (cmd == "cpageref" && use_zref) {
		os << "\\zcpageref";
		if (use_nolink)
			os << "*";
		docstring opts = getParam("options");
		if (use_caps) {
			if (!opts.empty())
				opts +=", ";
			opts += "S";
		}
		if (useRange()) {
			if (!opts.empty())
				opts +=", ";
			opts += "range";
		}
		if (!opts.empty())
			os << "[" << opts << "]";
		bool first = true;
		os << "{";
		for (auto const & l : labels) {
			if (!first)
				os << ",";
			os << l;
			first = false;
		}
		os << "}";
	} else if (cmd == "cpageref") {
		bool first = true;
		for (auto const & label : labels) {
			if (!first)
				os << ", ";
			os << "\\pageref";
			if (use_nolink)
				os << "*";
			os << '{' << label << '}';
			first = false;
		}
	} else if (nlabels > 1 && cmd == "ref" && use_cleveref) {
		os << "\\labelcref" << '{' << data << '}';
	} else if (nlabels > 1 && cmd == "pageref" && use_cleveref) {
		os << "\\labelcpageref" << '{' << data << '}';
	} else if (nlabels > 1 && cmd == "ref" && use_zref) {
		os << "\\zcref";
		if (use_nolink)
			os << "*";
		docstring opts = getParam("options");
		if (use_zref && use_caps) {
			if (!opts.empty())
				opts +=", ";
			opts += "noname";
		}
		if (use_zref && !opts.empty())
			os << "[" << opts << "]";
		os << '{' << data << '}';
	} else if (nlabels > 1 && cmd == "pageref" && use_zref) {
		os << "\\zcref";
		if (use_nolink)
			os << "*";
		docstring opts = getParam("options");
		if (use_zref && use_caps) {
			if (!opts.empty())
				opts +=", ";
			opts += "noname, page";
		}
		if (use_zref && !opts.empty())
			os << "[" << opts << "]";
		os << '{' << data << '}';
	} else if (nlabels == 1) {
		InsetCommandParams p(REF_CODE, cmd);
		bool const use_nolink = hyper_on && getParam("nolink") == "true";
		p["reference"] = getParam("reference");
		os << p.getCommand(rp, use_nolink);
	} else {
		bool first = true;
		vector<docstring>::const_iterator it = labels.begin();
		vector<docstring>::const_iterator en = labels.end();
		for (size_t i = 0; it != en; ++it, ++i) {
			if (!first) {
				if (labels.size() == 2)
					os << buffer().B_("[[reference 1]] and [[reference2]]");
				else if (i > 0 && i == labels.size() - 1)
					os << buffer().B_("[[reference 1, ...]], and [[reference n]]");
				else
					os << buffer().B_("[[reference 1]], [[reference2, ...]]");
			}
			os << "\\" << cmd;
			if (use_nolink)
				os << "*";
			os << '{' << *it << '}';
			first = false;
		}
	}

	if (rp.inulemcmd > 0)
		os << "}";
}


int InsetRef::plaintext(odocstringstream & os,
        OutputParams const &, size_t) const
{
	docstring const str = getParam("reference");
	os << '[' << str << ']';
	return 2 + int(str.size());
}


void InsetRef::docbook(XMLStream & xs, OutputParams const &) const
{
	docstring const & ref = getParam("reference");
	InsetLabel const * il = buffer().insetLabel(ref, true);
	string const & cmd = params().getCmdName();
	docstring linkend = xml::cleanID(ref);

	// A name is provided, LyX will provide it. This is supposed to be a very rare case.
	// Link with linkend, as is it within the document (not outside, in which case xlink:href is better suited).
	docstring const & name = getParam("name");
	if (!name.empty()) {
		docstring attr = from_utf8("linkend=\"") + linkend + from_utf8("\"");

		xs << xml::StartTag("link", to_utf8(attr));
		xs << name;
		xs << xml::EndTag("link");
		return;
	}

	// The DocBook processor will generate the name when required.
	docstring display_before;
	docstring display_after;
	docstring role;

	if (il && !il->counterValue().empty()) {
		// Try to construct a label from the InsetLabel we reference.
		if (cmd == "vref" || cmd == "pageref" || cmd == "vpageref" || cmd == "nameref" || cmd == "formatted") {
			// "ref on page #", "on page #", etc. The DocBook processor deals with generating the right text,
			// including in the right language.
			role = from_ascii(cmd);

			if (cmd == "formatted") {
				// A formatted reference may have many parameters. Generate all of them as roles, the only
				// way arbitrary parameters can be passed into DocBook.
				if (buffer().params().xref_package == "refstyle" && getParam("caps") == "true")
					role += " refstyle-caps";
				if (buffer().params().xref_package == "refstyle" && getParam("plural") == "true")
					role += " refstyle-plural";
			}
		} else if (cmd == "eqref") {
			display_before = from_ascii("(");
			display_after = from_ascii(")");
		}
		// TODO: what about labelonly? I don't get how this is supposed to work...
	}

	// No name, ask DocBook to generate one.
	docstring attr = from_utf8("linkend=\"") + xml::cleanID(ref) + from_utf8("\"");
	if (!role.empty())
		attr += " role=\"" + role + "\"";
	xs << display_before;
	xs << xml::CompTag("xref", to_utf8(attr));
	xs << display_after;
}


docstring InsetRef::displayString(docstring const & ref, string const & cmd,
		string const & language) const

{
	vector<docstring> display_string;
	vector<docstring> labels = getVectorFromString(ref);

	bool first = true;
	for (auto const & label : labels) {
		InsetLabel const * il = buffer().insetLabel(label, true);
		if (il && !il->counterValue().empty()) {
			// Try to construct a label from the InsetLabel we reference.
			docstring const & value = il->counterValue();
			if (cmd == "ref")
				display_string.push_back(value);
			else if (cmd == "vref")
				// normally, would be "ref on page #", but we have no pages
				display_string.push_back(value);
			else if (cmd == "pageref" || cmd == "vpageref" || cmd == "cpageref") {
				// normally would be "on page #", but we have no pages.
				display_string.push_back(language.empty() ? buffer().B_("elsewhere")
					: getMessages(language).get("elsewhere"));
			} else if (cmd == "eqref")
				display_string.push_back('(' + value + ')');
			else if (cmd == "formatted") {
				if (first)
					display_string.push_back(il->formattedCounter());
				else
					display_string.push_back(value);
			}
			else if (cmd == "nameref")
				// FIXME We don't really have the ability to handle these
				// properly in XHTML output yet (bug #8599).
				// It might not be that hard to do. We have the InsetLabel,
				// and we can presumably find its paragraph using the TOC.
				// But the label might be referencing a section, yet not be
				// in that section. So this is not trivial.
				display_string.push_back(il->prettyCounter());
		} else
			display_string.push_back(ref);
		first = false;
	}
	docstring res = (useRange()) ? getStringFromVector(display_string, from_utf8("–"))
				     : getStringFromVector(display_string, from_ascii(", "));

	if (cmd == "formatted" && buffer().params().xref_package != "prettyref" && getParam("caps") == "true")
		return capitalize(res);
		// it is hard to see what to do about plurals...

	return res;
}


docstring InsetRef::xhtml(XMLStream & xs, OutputParams const & op) const
	{
	docstring const & ref = getParam("reference");
	string const & cmd = params().getCmdName();
	// FIXME What we'd really like to do is to be able to output some
	// appropriate sort of text here. But to do that, we need to associate
	// some sort of counter with the label, and we don't have that yet.
	docstring const attr = "href=\"#" + xml::cleanAttr(ref) + '"';
	xs << xml::StartTag("a", to_utf8(attr));
	xs << displayString(ref, cmd, getLocalOrDefaultLang(op)->lang());
	xs << xml::EndTag("a");
	return docstring();
}


void InsetRef::toString(odocstream & os) const
{
	odocstringstream ods;
	plaintext(ods, OutputParams(nullptr));
	os << ods.str();
}


void InsetRef::forOutliner(docstring & os, size_t const, bool const) const
{
	// It's hard to know what to do here. Should we show XREF in the TOC?
	// Or should we just show that there is one? For now, we do the former.
	odocstringstream ods;
	plaintext(ods, OutputParams(nullptr));
	os += ods.str();
}


void InsetRef::updateBuffer(ParIterator const & it, UpdateType, bool const /*deleted*/)
{
	docstring const & ref = getParam("reference");

	// Check if this one is active (i.e., neither deleted with change-tracking
	// nor in an inset that does not produce output, such as notes or inactive branches)
	Paragraph const & para = it.paragraph();
	active_ = !para.isDeleted(it.pos()) && para.inInset().producesOutput();
	// If not, check whether we are in a deleted/non-outputting inset
	if (active_) {
		for (size_type sl = 0 ; sl < it.depth() ; ++sl) {
			Paragraph const & outer_par = it[sl].paragraph();
			if (outer_par.isDeleted(it[sl].pos())
			    || !outer_par.inInset().producesOutput()) {
				active_ = false;
				break;
			}
		}
	}

	for (auto const & fn : externalFilenames())
		// register externally referred files
		buffer().registerExternalRefs(fn);

	for (auto const & label : getLabels()) {
		// register this inset into the buffer reference cache.
		buffer().addReference(label, this, it);
	}

	docstring label;
	string const & cmd = getCmdName();
	for (int i = 0; !types[i].latex_name.empty(); ++i) {
		if (cmd == types[i].latex_name) {
			label = _(types[i].short_gui_name);
			// indicate no hyperlink (starred)
			if (cmd != "formatted" && cmd != "labelonly") {
				bool const isNoLink = getParam("nolink") == "true";
				if (isNoLink)
					label += from_ascii("*");
			}
			// indicate plural and caps
			if (cmd == "formatted") {
				bool const isPlural = getParam("plural") == "true";
				bool const isCaps = getParam("caps") == "true";
				if (isCaps) {
					// up arrow (shift key) symbol
					label += docstring(1, char_type(0x21E7));
				}
				if (isPlural)
					label += from_ascii("+");
			}
			label += from_ascii(": ");
			break;
		}
	}

	if (cmd != "labelonly")
		label += ref;
	else {
		if (getParam("noprefix") != "true")
			label += ref;
		else {
			docstring prefix;
			vector <docstring> slrefs;
			for (auto const & r : getLabels()) {
				docstring suffix = split(r, prefix, ':');
				if (suffix.empty())
					slrefs.push_back(ref);
				else
					slrefs.push_back(suffix);
			}
			label += getStringFromVector(slrefs);
		}
	}

	if (!buffer().params().isLatex() && !getParam("name").empty()) {
		label += "||";
		label += getParam("name");
	}
	
	// The tooltip will be over-written later, in addToToc, if need be.
	tooltip_ = label;
	toc_string_ = label;
	
	unsigned int const maxLabelChars = 24;
	support::truncateWithEllipsis(label, maxLabelChars);

	// Note: This could also be changed later, in addToToc, if we are using
	// fomatted references in the work area.
	screen_label_ = label;
	// This also can be overwritten in addToToc. (We can't do it now
	// because it might be a forward-reference and so the reference might
	// not be in the label cache yet.)
	broken_ = false;
	setBroken(broken_);
	cleanUpExternalFileNames();
}


docstring InsetRef::screenLabel() const
{
	return (broken_ ? _("BROKEN: ") : docstring()) + screen_label_;
}


void InsetRef::addToToc(DocIterator const & cpit, bool output_active,
			UpdateType, TocBackend & backend) const
{
	active_ = output_active;
	for (auto const & label : getLabels()) {
		if (buffer().insetLabel(label)) {
			broken_ = isBroken(label);
			setBroken(broken_);
			if (broken_ && output_active) {
				shared_ptr<Toc> toc2 = backend.toc("brokenrefs");
				toc2->push_back(TocItem(cpit, 0, screenLabel(), output_active));
			}
	
			// Code for display of formatted references
			string const & cmd = getCmdName();
			if (cmd != "pageref" && cmd != "vpageref" &&
				cmd != "vref"    && cmd != "labelonly")
			{
				bool const use_formatted_ref = buffer().params().use_formatted_ref;
				// We will put the value of the reference either into the tooltip
				// or the screen label, depending.
				docstring & target = use_formatted_ref ? screen_label_ : tooltip_;
				docstring const & ref = getParam("reference");
					target = displayString(ref, cmd);
			}
			return;
		}
		// It seems that this reference does not point to any valid label.
		broken_ = isBroken(label);
		setBroken(broken_);
		shared_ptr<Toc> toc = backend.toc("label");
		if (TocBackend::findItem(*toc, 0, label) == toc->end())
			toc->push_back(TocItem(cpit, 0, label, output_active, true));
		toc->push_back(TocItem(cpit, 1, screenLabel(), output_active));
		shared_ptr<Toc> toc2 = backend.toc("brokenrefs");
		toc2->push_back(TocItem(cpit, 0, screenLabel(), output_active));
	}
}


void InsetRef::validate(LaTeXFeatures & features) const
{
	string const & cmd = getCmdName();
	if (cmd == "vref" || cmd == "vpageref") {
		if (buffer().masterParams().xref_package == "zref")
			features.require("zref-vario");
		else
			features.require("varioref");
	} else if (cmd == "formatted") {
		docstring const data = getEscapedLabel(features.runparams());
		vector<docstring> label;
		docstring prefix;
		bool const use_caps   = getParam("caps") == "true";
		docstring const fcmd =
			getFormattedCmd(data, label, prefix, buffer().masterParams().xref_package, use_caps, useRange());
		if (buffer().masterParams().xref_package == "refstyle") {
			features.require("refstyle");
			if (prefix == "cha")
				features.addPreambleSnippet(from_ascii("\\let\\charef=\\chapref"));
			else if (prefix == "subsec")
				features.require("refstyle:subsecref");
			else if (!prefix.empty()) {
				docstring lcmd = "\\AtBeginDocument{\\providecommand" +
						fcmd + "[1]{\\ref{" + prefix + ":#1}}}";
				features.addPreambleSnippet(lcmd);
			}
		} else if (buffer().masterParams().xref_package == "cleveref") {
			features.require("cleveref");
		} else if (buffer().masterParams().xref_package == "zref") {
			features.require("zref-clever");
		} else {
			features.require("prettyref");
			// prettyref uses "cha" for chapters, so we provide a kind of
			// translation.
			if (prefix == "chap")
				features.addPreambleSnippet(from_ascii("\\let\\pr@chap=\\pr@cha"));
		}
	} else if (cmd == "eqref" && buffer().params().xref_package != "refstyle")
		// with refstyle, we simply output "(\ref{label})"
		features.require("amsmath");
	else if (cmd == "nameref") {
		bool const nr_clever = !buffer().masterParams().pdfoptions().use_hyperref
			|| getParam("nolink") == "true";
		if (buffer().masterParams().xref_package == "cleveref" && nr_clever)
			features.require("cleveref");
		else
			features.require("nameref");
	} else if (cmd == "cpageref") {
		if (buffer().masterParams().xref_package == "cleveref")
			features.require("cleveref");
		else if (buffer().masterParams().xref_package == "zref")
			features.require("zref-clever");
	} else if (getLabels().size() > 1 && (cmd == "ref" || cmd == "pageref")) {
		if (buffer().masterParams().xref_package == "cleveref")
			features.require("cleveref");
		if (buffer().masterParams().xref_package == "zref")
			features.require("zref-clever");
	}
	if (!externalFilenames(true).empty())
		features.require("xr");
}

bool InsetRef::forceLTR(OutputParams const & rp) const
{
	// We force LTR for references. However,
	// * Namerefs are output in the scripts direction
	//   at least with fontspec/bidi and luabidi, though (see #11518).
	// * Parentheses are automatically swapped with XeTeX/bidi 
	//   [not with LuaTeX/luabidi] (see #11626).
	// FIXME: Re-Audit all other RTL cases.
	if (buffer().masterParams().useBidiPackage(rp))
		return false;
	return (getCmdName() != "nameref" || !buffer().masterParams().useNonTeXFonts);
}


InsetRef::type_info const InsetRef::types[] = {
	{ "ref",       N_("Standard"),              N_("Ref")},
	{ "eqref",     N_("Equation"),              N_("EqRef")},
	{ "pageref",   N_("Page Number"),           N_("Page")},
	{ "cpageref",  N_("Prefixed Page Number"),  N_("PrefPage")},
	{ "vpageref",  N_("Textual Page Number"),   N_("TextPage")},
	{ "vref",      N_("Standard+Textual Page"), N_("Ref+Text")},
	{ "nameref",   N_("Reference to Name"),     N_("NameRef")},
	{ "formatted", N_("Formatted"),             N_("Format")},
	{ "labelonly", N_("Label Only"),            N_("Label")},
	{ "", "", "" }
};


docstring InsetRef::getTOCString() const
{
	broken_ = false;
	for (auto const & label : getLabels()) {
		broken_ &= isBroken(label, active_);
	}
	return (broken_ ? _("BROKEN: ") : docstring()) + toc_string_;
}


bool InsetRef::isBroken(docstring const & label, bool const preset) const
{
	FileName fn = getExternalFileName(label);
	if (fn.empty() || !fn.exists())
		return !buffer().activeLabel(label) && preset;
	Buffer const * buf = theBufferList().getBuffer(fn);
	if (buf)
		return !buf->activeLabel(label) && preset;
	return preset;
}


void InsetRef::updateStatistics(Statistics & stats) const
{
	docstring const & ref = getParam("reference");
	string const & cmd = params().getCmdName();
	// best we can do here
	string const & lang = buffer().params().language->lang();
	docstring const refstring = displayString(ref, cmd, lang);
	stats.update(refstring);
}


bool InsetRef::useRange() const
{
	if (getLabels().size() != 2 || getParam("tuple") == "list")
		return false;
	string const & cmd = getCmdName();
	return cmd == "vref" || cmd == "vpageref"
		|| (cmd == "formatted" && buffer().masterParams().xref_package != "prettyref")
		|| (cmd == "cpageref");
}


vector<FileName> InsetRef::externalFilenames(bool const warn) const
{
	vector<FileName> res;
	if (buffer().isInternal())
		// internal buffers (with their own filenames)
		// are excluded from this.
		return res;

	// check whether the included file exist
	vector<string> incFileNames = getVectorFromString(ltrim(to_utf8(params()["filenames"])));
	ListOfBuffers const children = buffer().masterBuffer()->getDescendants();
	for (auto const & ifn : incFileNames) {
		string label;
		string const incFileName = split(ifn, label, '@');
		FileName fn =
			support::makeAbsPath(incFileName,
					     support::onlyPath(buffer().absFileName()));
		if (buffer().fileName() == fn)
			// ignore own file
			continue;
		if (fn.exists()) {
			bool is_family = false;
			for (auto const * b : children) {
				if (b->fileName() == fn) {
					is_family = true;
					break;
				}
			}
			if (!is_family)
				res.push_back(fn);
		} else if (warn)
			frontend::Alert::warning(_("Unknown external file!"),
						 bformat(_("The label `%1$s' refers to an external file\n"
							   "(`%2$s')\n"
							   "which could not be found. Output will be broken!"),
							 from_utf8(label), from_utf8(fn.absFileName())));
	}
	return res;
}


FileName InsetRef::getExternalFileName(docstring const & inlabel) const
{
	vector<string> incFileNames = getVectorFromString(ltrim(to_utf8(params()["filenames"])));
	ListOfBuffers const children = buffer().masterBuffer()->getDescendants();
	for (auto const & ifn : incFileNames) {
		string label;
		string const incFileName = split(ifn, label, '@');
		if (label != to_utf8(inlabel))
			continue;
		FileName fn =
			support::makeAbsPath(incFileName,
					     support::onlyPath(buffer().absFileName()));
		if (fn.exists()) {
			bool is_family = false;
			for (auto const * b : children) {
				if (b->fileName() == fn) {
					is_family = true;
					break;
				}
			}
			if (!is_family)
				return fn;
		} else
			return FileName();
	}
	return FileName();
}


void InsetRef::cleanUpExternalFileNames()
{
	// remove file names from document and relatives
	// and make all paths relative
	if (params()["filenames"].empty())
		return;
	vector<string> incFileNames = getVectorFromString(ltrim(to_utf8(params()["filenames"])));
	vector<string> cleanedFN;
	ListOfBuffers const children = buffer().masterBuffer()->getDescendants();
	for (auto const & ifn : incFileNames) {
		string label;
		string const incFileName = split(ifn, label, '@');
		FileName fn =
			support::makeAbsPath(incFileName,
					     support::onlyPath(buffer().absFileName()));
		if (buffer().fileName() == fn)
			continue;
		if (fn.exists()) {
			bool is_family = false;
			for (auto const * b : children) {
				if (b->fileName() == fn) {
					is_family = true;
					break;
				}
			}
			if (!is_family)
				cleanedFN.push_back(label + "@" + to_utf8(fn.relPath(buffer().filePath())));
		} else
			cleanedFN.push_back(label + "@" + to_utf8(fn.relPath(buffer().filePath())));
	}
	setParam("filenames", from_utf8(getStringFromVector(cleanedFN)));
}


} // namespace lyx
