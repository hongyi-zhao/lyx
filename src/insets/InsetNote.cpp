/**
 * \file InsetNote.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 * \author Martin Vermeer
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetNote.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "ColorSet.h"
#include "Cursor.h"
#include "Exporter.h"
#include "FontInfo.h"
#include "FuncRequest.h"
#include "FuncStatus.h"
#include "InsetLayout.h"
#include "LaTeXColors.h"
#include "LaTeXFeatures.h"
#include "LyXRC.h"
#include "output_docbook.h"
#include "output_latex.h"

#include "support/debug.h"
#include "support/docstream.h"
#include "support/Lexer.h"
#include "support/Translator.h"

#include "frontends/Application.h"

#include <algorithm>
#include <sstream>

using namespace std;

namespace lyx {

using support::Lexer;

namespace {

typedef Translator<string, InsetNoteParams::Type> NoteTranslator;

NoteTranslator const init_notetranslator()
{
	NoteTranslator translator("Note", InsetNoteParams::Note);
	translator.addPair("Comment", InsetNoteParams::Comment);
	translator.addPair("Greyedout", InsetNoteParams::Greyedout);
	return translator;
}


NoteTranslator const & notetranslator()
{
	static NoteTranslator const translator = init_notetranslator();
	return translator;
}


} // namespace


InsetNoteParams::InsetNoteParams()
	: type(Note)
{}


void InsetNoteParams::write(ostream & os) const
{
	string const label = notetranslator().find(type);
	os << "Note " << label << "\n";
}


void InsetNoteParams::read(Lexer & lex)
{
	string label;
	lex >> label;
	if (lex)
		type = notetranslator().find(label);
}


/////////////////////////////////////////////////////////////////////
//
// InsetNote
//
/////////////////////////////////////////////////////////////////////

InsetNote::InsetNote(Buffer * buf, string const & label)
	: InsetCollapsible(buf)
{
	params_.type = notetranslator().find(label);
}


InsetNote::~InsetNote()
{
	hideDialogs("note", this);
}


docstring InsetNote::layoutName() const
{
	return from_ascii("Note:" + notetranslator().find(params_.type));
}


void InsetNote::write(ostream & os) const
{
	params_.write(os);
	InsetCollapsible::write(os);
}


void InsetNote::read(Lexer & lex)
{
	params_.read(lex);
	InsetCollapsible::read(lex);
}


bool InsetNote::showInsetDialog(BufferView * bv) const
{
	bv->showDialog("note", params2string(params()),
		const_cast<InsetNote *>(this));
	return true;
}


void InsetNote::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY: {
		if (cmd.getArg(0) != "note") {
			// not for us; might be handled higher up
			cur.undispatched();
			return;
		}
		// Do not do anything if converting to the same type of Note.
		// A quick break here is done instead of disabling the LFUN
		// because disabling the LFUN would lead to a greyed out
		// entry, which might confuse users.
		// In the future, we might want to have a radio button for
		// switching between notes.
		InsetNoteParams params;
		string2params(to_utf8(cmd.argument()), params);
		if (params_.type == params.type)
			break;

		cur.recordUndoInset(this);
		string2params(to_utf8(cmd.argument()), params_);
		setButtonLabel();
		// what we really want here is a TOC update, but that means
		// a full buffer update
		cur.forceBufferUpdate();
		break;
	}

	case LFUN_INSET_DIALOG_UPDATE:
		cur.bv().updateDialog("note", params2string(params()));
		break;

	default:
		InsetCollapsible::doDispatch(cur, cmd);
		break;
	}
}


bool InsetNote::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY:
		if (cmd.getArg(0) == "note") {
			InsetNoteParams params;
			string2params(to_utf8(cmd.argument()), params);
			flag.setOnOff(params_.type == params.type);
		}
		return true;

	case LFUN_INSET_DIALOG_UPDATE:
		flag.setEnabled(true);
		return true;

	default:
		return InsetCollapsible::getStatus(cur, cmd, flag);
	}
}


bool InsetNote::isMacroScope() const
{
	// LyX note has no latex output
	if (params_.type == InsetNoteParams::Note)
		return true;

	return InsetCollapsible::isMacroScope();
}


void InsetNote::latex(otexstream & os, OutputParams const & runparams_in) const
{
	if (params_.type != InsetNoteParams::Greyedout
	    && runparams_in.find_effective()
	    && !runparams_in.find_with_non_output())
		return;

	if (params_.type == InsetNoteParams::Note) {
		if (runparams_in.find_with_non_output()) {
			OutputParams runparams(runparams_in);
			InsetCollapsible::latex(os, runparams);
			runparams_in.encoding = runparams.encoding;
		}
		return;
	}

	OutputParams runparams(runparams_in);
	if (params_.type == InsetNoteParams::Comment) {
		if (runparams_in.inComment) {
			// Nested comments should just output the contents.
			latexParagraphs(buffer(), text(), os, runparams);
			return;
		}

		runparams.inComment = true;
		// Ignore files that are exported inside a comment
		runparams.exportdata.reset(new ExportData);
	}

	// the space after the comment in 'a[comment] b' will be eaten by the
	// comment environment since the space before b is ignored with the
	// following latex output:
	//
	// a%
	// \begin{comment}
	// comment
	// \end{comment}
	//  b
	//
	// Adding {} before ' b' fixes this.
	// The {} will be automatically added, but only if needed, for all
	// insets whose InsetLayout Display tag is false. This is achieved
	// by telling otexstream to protect an immediately following space
	// and is done for both comment and greyedout insets.
	InsetCollapsible::latex(os, runparams);

	runparams_in.encoding = runparams.encoding;
}


int InsetNote::plaintext(odocstringstream & os,
			 OutputParams const & runparams_in, size_t max_length) const
{
	if (!runparams_in.find_with_non_output()) {
		if (params_.type == InsetNoteParams::Note)
			return 0;
		else if (params_.type == InsetNoteParams::Comment
		    && runparams_in.find_effective())
			return 0;
	}

	OutputParams runparams(runparams_in);
	if (params_.type != InsetNoteParams::Greyedout) {
		runparams.inComment = true;
		// Ignore files that are exported inside a comment
		runparams.exportdata.reset(new ExportData);
	}
	if (!runparams_in.find_with_non_output())
		os << '[' << buffer().B_("note") << ":\n";
	InsetText::plaintext(os, runparams, max_length);
	if (!runparams_in.find_with_non_output())
		os << "\n]";

	return PLAINTEXT_NEWLINE + 1; // one char on a separate line
}


void InsetNote::docbook(XMLStream & xs, OutputParams const & runparams_in) const
{
	if (params_.type == InsetNoteParams::Note)
		return;

	OutputParams runparams(runparams_in);
	if (params_.type == InsetNoteParams::Comment) {
		xs << xml::StartTag("remark");
		xs << xml::CR();
		runparams.inComment = true;
		// Ignore files that are exported inside a comment
		runparams.exportdata.reset(new ExportData);
	}
	// Greyed out text is output as such (no way to mark text as greyed out with DocBook).

	InsetText::docbook(xs, runparams);

	if (params_.type == InsetNoteParams::Comment) {
		xs << xml::CR();
		xs << xml::EndTag("remark");
		xs << xml::CR();
	}
}


docstring InsetNote::xhtml(XMLStream & xs, OutputParams const & rp) const
{
	if (params_.type == InsetNoteParams::Note)
		return docstring();

	return InsetCollapsible::xhtml(xs, rp);
}


void InsetNote::validate(LaTeXFeatures & features) const
{
	switch (params_.type) {
	case InsetNoteParams::Comment:
		if (features.runparams().flavor == Flavor::Html)
			// we do output this but set display to "none" by default,
			// but people might want to use it.
			InsetCollapsible::validate(features);
		else
			// Only do the requires
			features.useInsetLayout(getLayout());
		break;
	case InsetNoteParams::Greyedout: {
		features.require("xcolor");
		if (theLaTeXColors().isLaTeXColor(buffer().params().notefontcolor)) {
			LaTeXColor const lc = theLaTeXColors().getLaTeXColor(buffer().params().notefontcolor);
			for (auto const & r : lc.req())
				features.require(r);
			if (!lc.model().empty())
				features.require("xcolor:" + lc.model());
		}
		InsetCollapsible::validate(features);
		break;
	}
	case InsetNoteParams::Note:
		// Showing previews in this inset may require stuff
		if (features.runparams().for_preview)
			InsetCollapsible::validate(features);
		break;
	}
}


string InsetNote::contextMenuName() const
{
	return "context-note";
}

bool InsetNote::allowSpellCheck() const
{
	return (params_.type == InsetNoteParams::Greyedout || lyxrc.spellcheck_notes);
}

FontInfo InsetNote::getFont() const
{
	FontInfo font = getLayout().font();
	if (params_.type == InsetNoteParams::Greyedout) {
		ColorCode c = lcolor.getFromLyXName("notefontcolor");
		// This is the local color (not overridden by other documents)
		// the color might not yet be initialized for new documents
		ColorCode lc = lcolor.getFromLyXName("notefontcolor@" + buffer().fileName().absFileName(), false);
		if (lc != Color_none) {
			font.setColor(lc);
			font.setPaintColor(lc);
		} else if (c != Color_none)
			font.setColor(c);
	}
	return font;
}


string InsetNote::params2string(InsetNoteParams const & params)
{
	ostringstream data;
	data << "note" << ' ';
	params.write(data);
	return data.str();
}


void InsetNote::string2params(string const & in, InsetNoteParams & params)
{
	params = InsetNoteParams();

	if (in.empty())
		return;

	istringstream data(in);
	Lexer lex;
	lex.setStream(data);
	lex.setContext("InsetNote::string2params");
	lex >> "note";
	// There are cases, such as when we are called via getStatus() from
	// Dialog::canApply(), where we are just called with "note" rather
	// than a full "note Note TYPE".
	if (!lex.isOK())
		return;
	lex >> "Note";

	params.read(lex);
}


} // namespace lyx
