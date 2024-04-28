/**
 * \file InsetWrap.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Dekel Tsur
 * \author Uwe Stöhr
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetWrap.h"
#include "InsetCaption.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Counters.h"
#include "Cursor.h"
#include "DispatchResult.h"
#include "Floating.h"
#include "FloatList.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "LaTeXFeatures.h"
#include "xml.h"
#include "texstream.h"
#include "TextClass.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/gettext.h"
#include "support/Lexer.h"

#include "frontends/Application.h"

#include <climits>

using namespace std;


namespace lyx {

using support::Lexer;

InsetWrap::InsetWrap(Buffer * buf, string const & type)
	: InsetCaptionable(buf)
{
	setCaptionType(type);
}


InsetWrap::~InsetWrap()
{
	hideDialogs("wrap", this);
}


// Enforce equality of float type and caption type.
void InsetWrap::setCaptionType(std::string const & type)
{
	InsetCaptionable::setCaptionType(type);
	params_.type = captionType();
	setLabel(_("Wrap: ") + floatName(type));
}


docstring InsetWrap::layoutName() const
{
	return "Wrap:" + from_utf8(params_.type);
}


docstring InsetWrap::toolTip(BufferView const & bv, int x, int y) const
{
	if (isOpen(bv))
		return InsetCaptionable::toolTip(bv, x, y);
	OutputParams rp(&buffer().params().encoding());
	docstring caption_tip = getCaptionText(rp);
	if (!caption_tip.empty())
		caption_tip += from_ascii("\n");
	return toolTipText(caption_tip);
}


void InsetWrap::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {
	case LFUN_INSET_MODIFY: {
		cur.recordUndoInset(this);
		InsetWrapParams params;
		InsetWrap::string2params(to_utf8(cmd.argument()), params);
		params_.lines = params.lines;
		params_.placement = params.placement;
		params_.overhang = params.overhang;
		params_.width = params.width;
		break;
	}

	case LFUN_INSET_DIALOG_UPDATE:
		cur.bv().updateDialog("wrap", params2string(params()));
		break;

	default:
		InsetCaptionable::doDispatch(cur, cmd);
		break;
	}
}


bool InsetWrap::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	switch (cmd.action()) {
	case LFUN_INSET_MODIFY:
	case LFUN_INSET_DIALOG_UPDATE:
		flag.setEnabled(true);
		return true;

	default:
		return InsetCaptionable::getStatus(cur, cmd, flag);
	}
}


void InsetWrap::updateBuffer(ParIterator const & it, UpdateType utype, bool const deleted)
{
	InsetCaptionable::updateBuffer(it, utype, deleted);
}


void InsetWrapParams::write(ostream & os) const
{
	os << "Wrap " << type << '\n';
	os << "lines " << lines << '\n';
	os << "placement " << placement << '\n';
	os << "overhang " << overhang.asString() << '\n';
	os << "width \"" << width.asString() << "\"\n";
}


void InsetWrapParams::read(Lexer & lex)
{
	lex.setContext("InsetWrapParams::read");
	lex >> "lines" >> lines;
	lex >> "placement" >> placement;
	lex >> "overhang" >> overhang;
	lex >> "width" >> width;
}


void InsetWrap::write(ostream & os) const
{
	params_.write(os);
	InsetCaptionable::write(os);
}


void InsetWrap::read(Lexer & lex)
{
	params_.read(lex);
	InsetCaptionable::read(lex);
}


void InsetWrap::validate(LaTeXFeatures & features) const
{
	features.require("wrapfig");
	features.inFloat(true);
	InsetCaptionable::validate(features);
	features.inFloat(false);
}


void InsetWrap::latex(otexstream & os, OutputParams const & runparams_in) const
{
	OutputParams runparams(runparams_in);
	runparams.inFloat = OutputParams::MAINFLOAT;
	os << "\\begin{wrap" << from_ascii(params_.type) << '}';
	// no optional argument when lines are zero
	if (params_.lines != 0)
		os << '[' << params_.lines << ']';
	os << '{' << from_ascii(params_.placement) << '}';
	Length over(params_.overhang);
	// no optional argument when the value is zero
	if (over.value() != 0)
		os << '[' << from_ascii(params_.overhang.asLatexString()) << ']';
	os << '{' << from_ascii(params_.width.asLatexString()) << "}%\n";
	InsetText::latex(os, runparams);
	os << "\\end{wrap" << from_ascii(params_.type) << "}%\n";
}


int InsetWrap::plaintext(odocstringstream & os,
        OutputParams const & runparams, size_t max_length) const
{
	os << '[' << buffer().B_("wrap") << ' '
		<< floatName(params_.type) << ":\n";
	InsetText::plaintext(os, runparams, max_length);
	os << "\n]";

	return PLAINTEXT_NEWLINE + 1; // one char on a separate line
}


void InsetWrap::docbook(XMLStream & xs, OutputParams const & runparams) const
{
	InsetLayout const & il = getLayout();

	xs << xml::StartTag(il.docbooktag(), il.docbookattr()); // TODO: Detect when there is a title.
	InsetText::docbook(xs, runparams);
	xs << xml::EndTag(il.docbooktag());
}


docstring InsetWrap::xhtml(XMLStream & xs, OutputParams const & rp) const
{
	string const len = params_.width.asHTMLString();
	string const width = len.empty() ? "50%" : len;
	InsetLayout const & il = getLayout();
	string const & tag = il.htmltag();
	string const attr = il.htmlGetAttrString() + " style='width:" + width + ";'";
	xs << xml::StartTag(tag, attr);
	docstring const deferred =
		InsetText::insetAsXHTML(xs, rp, InsetText::WriteInnerTag);
	xs << xml::EndTag(tag);
	return deferred;
}


bool InsetWrap::insetAllowed(InsetCode code) const
{
	switch(code) {
	case WRAP_CODE:
	case FOOT_CODE:
	case MARGIN_CODE:
		return false;
	default:
		return InsetCaptionable::insetAllowed(code);
	}
}


bool InsetWrap::showInsetDialog(BufferView * bv) const
{
	if (!InsetText::showInsetDialog(bv))
		bv->showDialog("wrap", params2string(params()),
			const_cast<InsetWrap *>(this));
	return true;
}


void InsetWrap::string2params(string const & in, InsetWrapParams & params)
{
	params = InsetWrapParams();
	istringstream data(in);
	Lexer lex;
	lex.setStream(data);
	lex.setContext("InsetWrap::string2params");
	lex >> "wrap";
	lex >> "Wrap";  // Part of the inset proper, swallowed by Text::readInset
	lex >> params.type; // We have to read the type here!
	params.read(lex);
}


string InsetWrap::params2string(InsetWrapParams const & params)
{
	ostringstream data;
	data << "wrap" << ' ';
	params.write(data);
	return data.str();
}


} // namespace lyx
