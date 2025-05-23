/**
 * \file InsetBranch.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Martin Vermeer
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetBranch.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "BranchList.h"
#include "ColorSet.h"
#include "Cursor.h"
#include "DispatchResult.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "Inset.h"
#include "LaTeXFeatures.h"
#include "LyX.h"
#include "output_docbook.h"
#include "output_xhtml.h"
#include "TextClass.h"

#include "support/convert.h"
#include "support/debug.h"
#include "support/gettext.h"
#include "support/Lexer.h"
#include "support/lstrings.h"

#include "frontends/Application.h"

#include <sstream>

using namespace std;


namespace lyx {

using support::Lexer;

InsetBranch::InsetBranch(Buffer * buf, InsetBranchParams const & params)
	: InsetCollapsible(buf, InsetText::DefaultLayout), params_(params)
{}


void InsetBranch::write(ostream & os) const
{
	os << "Branch ";
	params_.write(os);
	os << '\n';
	InsetCollapsible::write(os);
}


void InsetBranch::read(Lexer & lex)
{
	params_.read(lex);
	InsetCollapsible::read(lex);
}


docstring InsetBranch::toolTip(BufferView const & bv, int, int) const
{
	docstring const masterstatus = isBranchSelected() ?
		_("active") : _("non-active");
	docstring const childstatus = isBranchSelected(true) ?
		_("active") : _("non-active");
	docstring const status = (masterstatus == childstatus) ?
		masterstatus :
		support::bformat(_("master %1$s, child %2$s"),
						 masterstatus, childstatus);

	docstring const masteron = producesOutput() ?
		_("on") : _("off");
	docstring const childon =
		(isBranchSelected(true) != params_.inverted) ?
			_("on") : _("off");
	docstring const onoff = (masteron == childon) ?
		masteron :
		support::bformat(_("master %1$s, child %2$s"),
						 masteron, childon);

	docstring const heading =
		support::bformat(_("Branch Name: %1$s\nBranch Status: %2$s\nInset Status: %3$s"),
						 params_.branch, status, onoff);

	if (isOpen(bv))
		return heading;
	return toolTipText(heading + from_ascii("\n"));
}


docstring const InsetBranch::buttonLabel(BufferView const &) const
{
	static char_type const tick = 0x2714; // ✔ U+2714 HEAVY CHECK MARK
	static char_type const cross = 0x2716; // ✖ U+2716 HEAVY MULTIPLICATION X

	Buffer const & realbuffer = *buffer().masterBuffer();
	BranchList const & branchlist = realbuffer.params().branchlist();
	bool const inmaster = branchlist.find(params_.branch);
	bool const inchild = buffer().params().branchlist().find(params_.branch);

	bool const master_selected = producesOutput();
	bool const child_selected = isBranchSelected(true) != params_.inverted;

	docstring symb = docstring(1, master_selected ? tick : cross);
	if (inchild && master_selected != child_selected)
		symb += (child_selected ? tick : cross);

	docstring inv_symb = from_ascii(params_.inverted ? "~" : "");

	if (decoration() == InsetDecoration::MINIMALISTIC)
		return symb + inv_symb + params_.branch;

	bool const has_layout =
		buffer().params().documentClass().hasInsetLayout(layoutName());
	if (has_layout) {
		InsetLayout const & il = getLayout();
		docstring const & label_string = il.labelstring();
		if (!label_string.empty())
			return symb + inv_symb + label_string;
	}
	docstring s;
	if (inmaster && inchild)
		s = _("Branch: ");
	else if (inchild) // && !inmaster
		s = _("Branch (child): ");
	else if (inmaster) // && !inchild
		s = _("Branch (master): ");
	else // !inmaster && !inchild
		s = _("Branch (undefined): ");
	s += inv_symb + params_.branch;

	return symb + s;
}


ColorCode InsetBranch::backgroundColor(PainterInfo const & pi) const
{
	if (params_.branch.empty())
		return Inset::backgroundColor(pi);
	string const branch_id = (buffer().masterParams().branchlist().find(params_.branch))
			? convert<string>(buffer().masterParams().branchlist().id())
			: convert<string>(buffer().params().branchlist().id());
	// FIXME UNICODE
	string const branchcol = "branch" + branch_id + to_utf8(params_.branch);
	ColorCode c = lcolor.getFromLyXName(branchcol);
	// if we have background color, set to semantic value, as system colors
	// might vary
	if (lcolor.getX11HexName(c, (theApp() && theApp()->isInDarkMode())) == "background")
		c = Color_background;
	if (c == Color_none)
		c = Color_error;
	return c;
}


void InsetBranch::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {
	case LFUN_INSET_MODIFY: {
		InsetBranchParams params;
		InsetBranch::string2params(to_utf8(cmd.argument()), params);

		cur.recordUndoInset(this);
		params_.branch = params.branch;
		params_.inverted = params.inverted;
		// what we really want here is a TOC update, but that means
		// a full buffer update
		cur.forceBufferUpdate();
		break;
	}
	case LFUN_BRANCH_ACTIVATE:
	case LFUN_BRANCH_DEACTIVATE:
	case LFUN_BRANCH_MASTER_ACTIVATE:
	case LFUN_BRANCH_MASTER_DEACTIVATE:
		buffer().branchActivationDispatch(cmd.action(), params_.branch);
		break;
	case LFUN_BRANCH_INVERT:
		cur.recordUndoInset(this);
		params_.inverted = !params_.inverted;
		// what we really want here is a TOC update, but that means
		// a full buffer update
		cur.forceBufferUpdate();
		break;
	case LFUN_BRANCH_ADD:
		lyx::dispatch(FuncRequest(LFUN_BRANCH_ADD, params_.branch));
		break;
	case LFUN_BRANCH_SYNC_ALL:
		lyx::dispatch(FuncRequest(LFUN_INSET_FORALL, "Branch:" + params_.branch + " inset-toggle assign"));
		break;
	case LFUN_INSET_TOGGLE:
		if (cmd.argument() == "assign")
			setStatus(cur, (isBranchSelected(true) != params_.inverted) ? Open : Collapsed);
		else
			InsetCollapsible::doDispatch(cur, cmd);
		break;

	default:
		InsetCollapsible::doDispatch(cur, cmd);
		break;
	}
}


bool InsetBranch::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	bool const known_branch =
		buffer().params().branchlist().find(params_.branch);

	switch (cmd.action()) {
	case LFUN_INSET_MODIFY:
		flag.setEnabled(true);
		break;

	case LFUN_BRANCH_ACTIVATE:
	case LFUN_BRANCH_DEACTIVATE:
	case LFUN_BRANCH_MASTER_ACTIVATE:
	case LFUN_BRANCH_MASTER_DEACTIVATE:
		flag.setEnabled(buffer().branchActivationEnabled(cmd.action(), params_.branch));
		break;

	case LFUN_BRANCH_INVERT:
		flag.setEnabled(true);
		flag.setOnOff(params_.inverted);
		break;

	case LFUN_BRANCH_ADD:
		flag.setEnabled(!known_branch);
		break;

	case LFUN_BRANCH_SYNC_ALL:
		flag.setEnabled(known_branch);
		break;

	case LFUN_INSET_TOGGLE:
		if (cmd.argument() == "assign")
			flag.setEnabled(true);
		else
			return InsetCollapsible::getStatus(cur, cmd, flag);
		break;

	default:
		return InsetCollapsible::getStatus(cur, cmd, flag);
	}
	return true;
}


bool InsetBranch::isBranchSelected(bool const child) const
{
	Buffer const & realbuffer = child ? buffer() : *buffer().masterBuffer();
	BranchList const & branchlist = realbuffer.params().branchlist();
	Branch const * ourBranch = branchlist.find(params_.branch);

	if (!ourBranch) {
		// this branch is defined in child only
		ourBranch = buffer().params().branchlist().find(params_.branch);
		if (!ourBranch)
			return false;
	}
	return ourBranch->isSelected();
}


bool InsetBranch::producesOutput() const
{
	return isBranchSelected() != params_.inverted;
}


void InsetBranch::latex(otexstream & os, OutputParams const & runparams) const
{
	if (producesOutput() || runparams.find_with_non_output()) {
		OutputParams rp = runparams;
		rp.inbranch = true;
		InsetText::latex(os, rp);
		// These need to be passed upstream
		runparams.need_maketitle = rp.need_maketitle;
		runparams.have_maketitle = rp.have_maketitle;
	}
}


int InsetBranch::plaintext(odocstringstream & os,
			   OutputParams const & runparams, size_t max_length) const
{
	if (!producesOutput() && !runparams.find_with_non_output())
		return 0;

	int len = InsetText::plaintext(os, runparams, max_length);
	return len;
}


void InsetBranch::docbook(XMLStream & xs, OutputParams const & runparams) const
{
	if (producesOutput()) {
		OutputParams rp = runparams;
		rp.par_begin = 0;
		rp.par_end = text().paragraphs().size();
		docbookParagraphs(text(), buffer(), xs, rp);
	}
}


docstring InsetBranch::xhtml(XMLStream & xs, OutputParams const & rp) const
{
	if (producesOutput()) {
		OutputParams newrp = rp;
		newrp.par_begin = 0;
		newrp.par_end = text().paragraphs().size();
		xhtmlParagraphs(text(), buffer(), xs, newrp);
	}
	return docstring();
}


void InsetBranch::toString(odocstream & os) const
{
	if (producesOutput())
		InsetCollapsible::toString(os);
}


void InsetBranch::forOutliner(docstring & os, size_t const maxlen,
							  bool const shorten) const
{
	if (producesOutput())
		InsetCollapsible::forOutliner(os, maxlen, shorten);
}


void InsetBranch::validate(LaTeXFeatures & features) const
{
	// Showing previews in a disabled branch may require stuff
	if (producesOutput() || features.runparams().for_preview)
		InsetCollapsible::validate(features);
}


string InsetBranch::contextMenuName() const
{
	return "context-branch";
}


docstring InsetBranch::layoutName() const
{
	docstring const name = support::subst(branch(), '_', ' ');
	return from_ascii("Branch:") + name;
}



bool InsetBranch::isMacroScope() const
{
	// Its own scope if not selected by buffer
	return !producesOutput();
}


string InsetBranch::params2string(InsetBranchParams const & params)
{
	ostringstream data;
	params.write(data);
	return data.str();
}


void InsetBranch::string2params(string const & in, InsetBranchParams & params)
{
	params = InsetBranchParams();
	if (in.empty())
		return;

	istringstream data(in);
	Lexer lex;
	lex.setStream(data);
	lex.setContext("InsetBranch::string2params");
	params.read(lex);
}


void InsetBranch::updateBuffer(ParIterator const & it, UpdateType utype, bool const deleted)
{
	setLabel(params_.branch + (params_.inverted ? " (-)" : ""));
	InsetCollapsible::updateBuffer(it, utype, deleted);
}


void InsetBranchParams::write(ostream & os) const
{
	os << to_utf8(branch)
	   << '\n'
	   << "inverted "
	   << inverted;
}


void InsetBranchParams::read(Lexer & lex)
{
	// There may be a space in branch name
	// if we wanted to use lex>>, the branch name should be properly in quotes
	lex.eatLine();
	branch = lex.getDocString();
	lex >> "inverted" >> inverted;
}

} // namespace lyx
