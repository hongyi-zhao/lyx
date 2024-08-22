/**
 * \file InsetNomencl.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author O. U. Baran
 * \author Uwe Stöhr
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */
#include <config.h>

#include "InsetNomencl.h"
#include "InsetArgument.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "Cursor.h"
#include "DispatchResult.h"
#include "Font.h"
#include "Encoding.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "InsetLayout.h"
#include "InsetList.h"
#include "Language.h"
#include "LaTeXFeatures.h"
#include "LyX.h"
#include "xml.h"
#include "texstream.h"
#include "TocBackend.h"

#include "frontends/FontMetrics.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/gettext.h"
#include "support/Length.h"
#include "support/lstrings.h"

using namespace std;
using namespace lyx::support;

namespace lyx {


/////////////////////////////////////////////////////////////////////
//
// InsetNomencl
//
/////////////////////////////////////////////////////////////////////

InsetNomencl::InsetNomencl(Buffer * buf)
	: InsetCollapsible(buf)
{}


docstring InsetNomencl::toolTip(BufferView const & /*bv*/, int /*x*/, int /*y*/) const
{
	docstring tip = _("Nomenclature Symbol: ") + getSymbol();
	docstring const desc = getDescription();
	if (!desc.empty())
		tip += "\n" + _("Description: ") + "\t" + getDescription();
	docstring const prefix = getPrefix();
	if (!prefix.empty())
		tip += "\n" + _("Sorting: ") + prefix;
	return tip;
}


void InsetNomencl::write(ostream & os) const
{
	os << "Nomenclature" << endl;
	InsetCollapsible::write(os);
}


docstring InsetNomencl::getSymbol() const
{
	return text().asString();
}


docstring InsetNomencl::getPrefix() const
{
	docstring res;
	for (auto const & elem : paragraphs()[0].insetList()) {
		if (InsetArgument const * x = elem.inset->asInsetArgument())
			if (x->name() == "1")
				res = x->text().asString(AS_STR_INSETS);
	}
	return res;
}


docstring InsetNomencl::getDescription() const
{
	docstring res;
	for (auto const & elem : paragraphs()[0].insetList()) {
		if (InsetArgument const * x = elem.inset->asInsetArgument())
			if (x->name() == "post:1")
				res = x->text().asString(AS_STR_INSETS);
	}
	return res;
}


int InsetNomencl::plaintext(odocstringstream & os,
			    OutputParams const & rp, size_t) const
{
	docstring const desc = getDescription();
	os << "[";
	InsetText::plaintext(os, rp);
	if (!desc.empty())
		os << ": " << desc;
	os << "]";
	return os.str().size();
}


void InsetNomencl::docbook(XMLStream & xs, OutputParams const &) const
{
	docstring const symbol = getSymbol();
	docstring attr = "linkend=\"" + xml::cleanID(from_ascii("nomen") + symbol) + "\"";
	xs << xml::StartTag("glossterm", attr);
	xs << xml::escapeString(symbol);
	xs << xml::EndTag("glossterm");
}


docstring InsetNomencl::xhtml(XMLStream &, OutputParams const &) const
{
	return docstring();
}


void InsetNomencl::validate(LaTeXFeatures & features) const
{
	features.require("nomencl");
	InsetCollapsible::validate(features);
}


void InsetNomencl::addToToc(DocIterator const & cpit, bool output_active,
			    UpdateType, TocBackend & backend) const
{
	TocBuilder & b = backend.builder("nomencl");
	b.pushItem(cpit, getSymbol(), output_active);
	b.pop();
}


docstring InsetNomencl::layoutName() const
{
	return (contains(buffer().params().nomencl_opts, "nomentbl")) ?
				from_ascii("Nomenclature:nomentbl")
			      : from_ascii("Nomenclature");
}


/////////////////////////////////////////////////////////////////////
//
// InsetPrintNomencl
//
/////////////////////////////////////////////////////////////////////

InsetPrintNomencl::InsetPrintNomencl(Buffer * buf, InsetCommandParams const & p)
	: InsetCommand(buf, p)
{}


ParamInfo const & InsetPrintNomencl::findInfo(string const & /* cmdName */)
{
	// The symbol width is set via nomencl's \nomlabelwidth in
	// InsetPrintNomencl::latex and not as optional parameter of
	// \printnomenclature
	static ParamInfo param_info_;
	if (param_info_.empty()) {
		// how is the width set?
		// values: none|auto|custom|textwidth
		param_info_.add("set_width", ParamInfo::LYX_INTERNAL);
		// custom width
		param_info_.add("width", ParamInfo::LYX_INTERNAL);
	}
	return param_info_;
}


docstring InsetPrintNomencl::screenLabel() const
{
	return _("Nomenclature");
}


struct NomenclEntry {
	NomenclEntry() : par(nullptr) {}
	NomenclEntry(docstring s, docstring d, Paragraph const * p)
	  : symbol(s), desc(d), par(p)
	{}

	docstring symbol;
	docstring desc;
	Paragraph const * par;
};


typedef map<docstring, NomenclEntry > EntryMap;


docstring InsetPrintNomencl::xhtml(XMLStream &, OutputParams const & op) const
{
	shared_ptr<Toc const> toc = buffer().tocBackend().toc("nomencl");

	EntryMap entries;
	Toc::const_iterator it = toc->begin();
	Toc::const_iterator const en = toc->end();
	for (; it != en; ++it) {
		DocIterator dit = it->dit();
		Paragraph const & par = dit.innerParagraph();
		Inset const * inset = par.getInset(dit.top().pos());
		if (!inset)
			return docstring();
		InsetCommand const * ic = inset->asInsetCommand();
		if (!ic)
			return docstring();

		// FIXME We need a link to the paragraph here, so we
		// need some kind of struct.
		docstring const symbol = ic->getParam("symbol");
		docstring const desc = ic->getParam("description");
		docstring const prefix = ic->getParam("prefix");
		docstring const sortas = prefix.empty() ? symbol : prefix;

		entries[sortas] = NomenclEntry(symbol, desc, &par);
	}

	if (entries.empty())
		return docstring();

	// we'll use our own stream, because we are going to defer everything.
	// that's how we deal with the fact that we're probably inside a standard
	// paragraph, and we don't want to be.
	odocstringstream ods;
	XMLStream xs(ods);

	InsetLayout const & il = getLayout();
	string const & tag = il.htmltag();
	docstring toclabel = translateIfPossible(from_ascii("Nomenclature"),
		getLocalOrDefaultLang(op)->lang());

	xs << xml::StartTag("div", "class='nomencl'")
	   << xml::StartTag(tag, "class='nomencl'")
		 << toclabel
		 << xml::EndTag(tag)
	   << xml::CR()
	   << xml::StartTag("dl")
	   << xml::CR();

	EntryMap::const_iterator eit = entries.begin();
	EntryMap::const_iterator const een = entries.end();
	for (; eit != een; ++eit) {
		NomenclEntry const & ne = eit->second;
		string const parid = ne.par->magicLabel();
		xs << xml::StartTag("dt")
		   << xml::StartTag("a", "href='#" + parid + "' class='nomencl'")
		   << ne.symbol
		   << xml::EndTag("a")
		   << xml::EndTag("dt")
		   << xml::CR()
		   << xml::StartTag("dd")
		   << ne.desc
		   << xml::EndTag("dd")
		   << xml::CR();
	}

	xs << xml::EndTag("dl")
	   << xml::CR()
	   << xml::EndTag("div")
	   << xml::CR();

	return ods.str();
}


void InsetPrintNomencl::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY: {
		InsetCommandParams p(NOMENCL_PRINT_CODE);
		// FIXME UNICODE
		InsetCommand::string2params(to_utf8(cmd.argument()), p);
		if (p.getCmdName().empty()) {
			cur.noScreenUpdate();
			break;
		}

		cur.recordUndo();
		setParams(p);
		break;
	}

	default:
		InsetCommand::doDispatch(cur, cmd);
		break;
	}
}


bool InsetPrintNomencl::getStatus(Cursor & cur, FuncRequest const & cmd,
	FuncStatus & status) const
{
	switch (cmd.action()) {

	case LFUN_INSET_DIALOG_UPDATE:
	case LFUN_INSET_MODIFY:
		status.setEnabled(true);
		return true;

	default:
		return InsetCommand::getStatus(cur, cmd, status);
	}
}


void InsetPrintNomencl::docbook(XMLStream & xs, OutputParams const & runparams) const
{
	shared_ptr<Toc const> toc = buffer().tocBackend().toc("nomencl");

	EntryMap entries;
	Toc::const_iterator it = toc->begin();
	Toc::const_iterator const en = toc->end();
	for (; it != en; ++it) {
		DocIterator dit = it->dit();
		Paragraph const & par = dit.innerParagraph();
		Inset const * inset = par.getInset(dit.top().pos());
		if (!inset)
			return;
		InsetNomencl const * in = inset->asInsetNomencl();
		if (!in)
			return;

		// FIXME We need a link to the paragraph here, so we
		// need some kind of struct.
		docstring const symbol = in->getSymbol();
		docstring const desc = in->getDescription();
		docstring const prefix = in->getPrefix();
		// lowercase sortkey since map is case sensitive
		docstring sortas = prefix.empty() ? lowercase(symbol) : lowercase(prefix);
		// assure key uniqueness
		while (entries.find(sortas) != entries.end())
			sortas += "a";

		entries[sortas] = NomenclEntry(symbol, desc, &par);
	}

	if (entries.empty())
		return;

	// As opposed to XHTML, no need to defer everything until the end of time, so write directly to xs.
	// TODO: At least, that's what was done before...

	docstring toclabel = translateIfPossible(from_ascii("Nomenclature"),
						 getLocalOrDefaultLang(runparams)->lang());

	xs << xml::StartTag("glossary");
	xs << xml::CR();
	xs << xml::StartTag("title");
	xs << toclabel;
	xs << xml::EndTag("title");
	xs << xml::CR();

	EntryMap::const_iterator eit = entries.begin();
	EntryMap::const_iterator const een = entries.end();
	for (; eit != een; ++eit) {
		NomenclEntry const & ne = eit->second;

		xs << xml::StartTag("glossentry", "xml:id=\"" + xml::cleanID(from_ascii("nomen") + ne.symbol) + "\"");
		xs << xml::CR();
		xs << xml::StartTag("glossterm");
		xs << ne.symbol;
		xs << xml::EndTag("glossterm");
		xs << xml::CR();
		xs << xml::StartTag("glossdef");
		xs << xml::CR();
		xs << xml::StartTag("para");
		xs << ne.desc;
		xs << xml::EndTag("para");
		xs << xml::CR();
		xs << xml::EndTag("glossdef");
		xs << xml::CR();
		xs << xml::EndTag("glossentry");
		xs << xml::CR();
	}

	xs << xml::EndTag("glossary");
	xs << xml::CR();
}


namespace {
docstring nomenclWidest(Buffer const & buffer)
{
	// nomenclWidest() determines and returns the widest used
	// nomenclature symbol in the document

	int w = 0;
	docstring symb;
	InsetNomencl const * nomencl = nullptr;
	ParagraphList::const_iterator it = buffer.paragraphs().begin();
	ParagraphList::const_iterator end = buffer.paragraphs().end();

	for (; it != end; ++it) {
		if (it->insetList().empty())
			continue;
		InsetList::const_iterator iit = it->insetList().begin();
		InsetList::const_iterator eend = it->insetList().end();
		for (; iit != eend; ++iit) {
			Inset * inset = iit->inset;
			if (inset->lyxCode() != NOMENCL_CODE)
				continue;
			nomencl = static_cast<InsetNomencl const *>(inset);
			// Use proper formatting. We do not escape makeindex chars here
			docstring symbol = nomencl ?
						nomencl->getSymbol()
					      : docstring();
			// strip out % characters which are used as escape in nomencl
			// but act as comment in our context here
			symbol = subst(symbol, from_ascii("%"), docstring());
			// for measurement, convert to a somewhat more output-like string
			docstring msymb = Encodings::convertLaTeXCommands(symbol);
			// This is only an approximation,
			// but the best we can get.
			// FIXME: this might produce different results with
			// and without GUI (#10634)
			int const wx = use_gui ?
				theFontMetrics(Font()).width(msymb) :
				msymb.size();
			if (wx > w) {
				w = wx;
				symb = symbol;
			}
		}
	}
	// return the widest (or an empty) string
	return symb;
}
} // namespace


void InsetPrintNomencl::latex(otexstream & os, OutputParams const & runparams_in) const
{
	OutputParams runparams = runparams_in;
	bool const autowidth = getParam("set_width") == "auto";
	if (autowidth || getParam("set_width") == "textwidth") {
		docstring widest = autowidth ? nomenclWidest(buffer())
					     : getParam("width");
		// We need to validate that all characters are representable
		// in the current encoding. If not try the LaTeX macro which might
		// or might not be a good choice, and issue a warning.
		pair<docstring, docstring> widest_latexed =
				runparams.encoding->latexString(widest, runparams.dryrun);
		if (!widest_latexed.second.empty())
			LYXERR0("Uncodable character in nomencl entry. List width might be wrong!");
		// Set the label width via nomencl's command \nomlabelwidth.
		// This must be output before the command \printnomenclature
		if (!widest_latexed.first.empty()) {
			os << "\\settowidth{\\nomlabelwidth}{"
			   << widest_latexed.first
			   << "}\n";
		}
	} else if (getParam("set_width") == "custom") {
		// custom length as optional arg of \printnomenclature
		string const width =
			Length(to_ascii(getParam("width"))).asLatexString();
		os << '\\'
		   << from_ascii(getCmdName())
		   << '['
		   << from_ascii(width)
		   << "]"
		   << termcmd;
		return;
	}
	// output the command \printnomenclature
	os << getCommand(runparams);
}


void InsetPrintNomencl::validate(LaTeXFeatures & features) const
{
	features.useInsetLayout(getLayout());
	features.require("nomencl");
}


InsetCode InsetPrintNomencl::lyxCode() const
{
	return NOMENCL_PRINT_CODE;
}


string InsetPrintNomencl::contextMenuName() const
{
	return "context-nomenclprint";
}


} // namespace lyx
