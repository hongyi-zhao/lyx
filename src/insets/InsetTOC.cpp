/**
 * \file InsetTOC.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetTOC.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Cursor.h"
#include "DispatchResult.h"
#include "Font.h"
#include "FuncRequest.h"
#include "Language.h"
#include "LaTeXFeatures.h"
#include "output_xhtml.h"
#include "Paragraph.h"
#include "ParagraphParameters.h"
#include "TextClass.h"
#include "TocBackend.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/gettext.h"
#include "support/lassert.h"

#include <ostream>

using namespace std;

namespace lyx {

namespace {
string cmd2type(string const & cmd)
{
	if (cmd == "lstlistoflistings")
		return "listing";
	return cmd;
}
} // namespace


InsetTOC::InsetTOC(Buffer * buf, InsetCommandParams const & p)
	: InsetCommand(buf, p)
{}


ParamInfo const & InsetTOC::findInfo(string const & /* cmdName */)
{
	static ParamInfo param_info_;
	if (param_info_.empty()) {
		param_info_.add("type", ParamInfo::LATEX_REQUIRED);
	}
	return param_info_;
}


bool InsetTOC::isCompatibleCommand(string const & cmd)
{
	return cmd == defaultCommand() || cmd == "lstlistoflistings";
}


docstring InsetTOC::screenLabel() const
{
	if (getCmdName() == "tableofcontents")
		return buffer().B_("Table of Contents");
	if (getCmdName() == "lstlistoflistings")
		return buffer().B_("List of Listings");
	return _("Unknown TOC type");
}


void InsetTOC::doDispatch(Cursor & cur, FuncRequest & cmd) {
	switch (cmd.action()) {
	case LFUN_MOUSE_RELEASE:
		if (!cur.selection() && cmd.button() == mouse_button::button1) {
			cur.bv().showDialog("toc", params2string(params()));
			cur.dispatched();
		}
		break;

	default:
		InsetCommand::doDispatch(cur, cmd);
	}
}


docstring InsetTOC::layoutName() const
{
	if (getCmdName() == "lstlistoflistings") {
		if (buffer().params().use_minted)
			return from_ascii("TOC:MintedListings");
		else
			return from_ascii("TOC:Listings");
	}
	return from_ascii("TOC");
}


void InsetTOC::validate(LaTeXFeatures & features) const
{
	InsetCommand::validate(features);
	features.useInsetLayout(getLayout());
	if (getCmdName() == "lstlistoflistings") {
		if (buffer().params().use_minted)
			features.require("minted");
		else
			features.require("listings");
	}
}


int InsetTOC::plaintext(odocstringstream & os,
        OutputParams const &, size_t max_length) const
{
	os << screenLabel() << "\n\n";
	buffer().tocBackend().writePlaintextTocList(cmd2type(getCmdName()), os, max_length);
	return PLAINTEXT_NEWLINE;
}


void InsetTOC::docbook(XMLStream &, OutputParams const &) const
{
	// TOC are generated automatically by the DocBook processor.
}


void InsetTOC::makeTOCEntry(XMLStream & xs,
		Paragraph const & par, OutputParams const & op) const
{
	string const attr = "href='#" + par.magicLabel() + "' class='tocentry'";
	xs << xml::StartTag("a", attr);

	// First the label, if there is one
	docstring const & label = par.params().labelString();
	if (!label.empty())
		xs << label << " ";
	// Now the content of the TOC entry, taken from the paragraph itself
	OutputParams ours = op;
	ours.for_toc = true;
	Font const dummy;
	par.simpleLyXHTMLOnePar(buffer(), xs, ours, dummy);

	xs << xml::EndTag("a") << xml::CR();
}


void InsetTOC::makeTOCWithDepth(XMLStream & xs,
		Toc const & toc, OutputParams const & op) const
{
	int lastdepth = 0;
	for (auto const & tocitem : toc) {
		// do not output entries that are not actually included in the output,
		// e.g., stuff in non-active branches or notes or whatever.
		if (!tocitem.isOutput())
			continue;

		if (!tocitem.dit().paragraph().layout().htmlintoc())
			continue;

		// First, we need to manage increases and decreases of depth
		// If there's no depth to deal with, we artificially set it to 1.
		int const depth = tocitem.depth();

		// Ignore stuff above the tocdepth
		if (depth > buffer().params().tocdepth)
			continue;

		if (depth > lastdepth) {
			xs << xml::CR();
			// open as many tags as we need to open to get to this level
			// this includes the tag for the current level
			for (int i = lastdepth + 1; i <= depth; ++i) {
				stringstream attr;
				attr << "class='lyxtoc-" << i << "'";
				xs << xml::StartTag("div", attr.str()) << xml::CR();
			}
			lastdepth = depth;
		}
		else if (depth < lastdepth) {
			// close as many as we have to close to get back to this level
			// this includes closing the last tag at this level
			for (int i = lastdepth; i >= depth; --i)
				xs << xml::EndTag("div") << xml::CR();
			// now open our tag
			stringstream attr;
			attr << "class='lyxtoc-" << depth << "'";
			xs << xml::StartTag("div", attr.str()) << xml::CR();
			lastdepth = depth;
		} else {
			// no change of level, so close and open
			xs << xml::EndTag("div") << xml::CR();
			stringstream attr;
			attr << "class='lyxtoc-" << depth << "'";
			xs << xml::StartTag("div", attr.str()) << xml::CR();
		}

		// Now output TOC info for this entry
		Paragraph const & par = tocitem.dit().innerParagraph();
		makeTOCEntry(xs, par, op);
	}
	for (int i = lastdepth; i > 0; --i)
		xs << xml::EndTag("div") << xml::CR();
}


void InsetTOC::makeTOCNoDepth(XMLStream & xs,
		Toc const & toc, const OutputParams & op) const
{
	for (auto const & tocitem : toc) {
		// do not output entries that are not actually included in the output,
		// e.g., stuff in non-active branches or notes or whatever.
		if (!tocitem.isOutput())
			continue;

		if (!tocitem.dit().paragraph().layout().htmlintoc())
			continue;

		xs << xml::StartTag("div", "class='lyxtoc-flat'") << xml::CR();

		Paragraph const & par = tocitem.dit().innerParagraph();
		makeTOCEntry(xs, par, op);

		xs << xml::EndTag("div");
	}
}


docstring InsetTOC::xhtml(XMLStream &, OutputParams const & op) const
{
	string const & command = getCmdName();
	if (command != "tableofcontents" && command != "lstlistoflistings") {
		LYXERR0("TOC type " << command << " not yet implemented.");
		LASSERT(false, return docstring());
	}

	shared_ptr<Toc const> toc =
		buffer().masterBuffer()->tocBackend().toc(cmd2type(command));
	if (toc->empty())
		return docstring();

	// we'll use our own stream, because we are going to defer everything.
	// that's how we deal with the fact that we're probably inside a standard
	// paragraph, and we don't want to be.
	odocstringstream ods;
	XMLStream xs(ods);

	xs << xml::StartTag("div", "class='toc'");

	// Title of TOC
	InsetLayout const & il = getLayout();
	string const & tag = il.htmltag();
	docstring title = screenLabel();
	Layout const & lay = buffer().params().documentClass().htmlTOCLayout();
	string const & tocclass = lay.defaultCSSClass();
	string const tocattr = "class='tochead " + tocclass + "'";
	xs << xml::StartTag(tag, tocattr)
		 << title
		 << xml::EndTag(tag);

	// with lists of listings, at least, there is no depth
	// to worry about. so the code can be simpler.
	bool const use_depth = (command == "tableofcontents");

	// Output of TOC
	if (use_depth)
		makeTOCWithDepth(xs, *toc, op);
	else
		makeTOCNoDepth(xs, *toc, op);

	xs << xml::EndTag("div") << xml::CR();
	return ods.str();
}


} // namespace lyx
