/**
 * \file InsetBox.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 * \author Martin Vermeer
 * \author Jürgen Spitzmüller
 * \author Uwe Stöhr
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "InsetBox.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "ColorSet.h"
#include "Cursor.h"
#include "FuncStatus.h"
#include "FuncRequest.h"
#include "LaTeXColors.h"
#include "LaTeXFeatures.h"
#include "MetricsInfo.h"
#include "output_docbook.h"
#include "TexRow.h"
#include "texstream.h"

#include "support/debug.h"
#include "support/FileName.h"
#include "support/gettext.h"
#include "support/Lexer.h"
#include "support/lstrings.h"
#include "support/Translator.h"

#include <sstream>

using namespace std;
using namespace lyx::support;

namespace lyx {

namespace {

typedef Translator<string, InsetBox::BoxType> BoxTranslator;
typedef Translator<docstring, InsetBox::BoxType> BoxTranslatorLoc;

BoxTranslator initBoxtranslator()
{
	BoxTranslator translator("Boxed", InsetBox::Boxed);
	translator.addPair("Frameless", InsetBox::Frameless);
	translator.addPair("Framed", InsetBox::Framed);
	translator.addPair("ovalbox", InsetBox::ovalbox);
	translator.addPair("Ovalbox", InsetBox::Ovalbox);
	translator.addPair("Shadowbox", InsetBox::Shadowbox);
	translator.addPair("Shaded", InsetBox::Shaded);
	translator.addPair("Doublebox",InsetBox::Doublebox);
	return translator;
}


BoxTranslatorLoc initBoxtranslatorLoc()
{
	BoxTranslatorLoc translator(_("simple frame"), InsetBox::Boxed);
	translator.addPair(_("frameless"), InsetBox::Frameless);
	translator.addPair(_("simple frame, page breaks"), InsetBox::Framed);
	translator.addPair(_("oval, thin"), InsetBox::ovalbox);
	translator.addPair(_("oval, thick"), InsetBox::Ovalbox);
	translator.addPair(_("drop shadow"), InsetBox::Shadowbox);
	translator.addPair(_("shaded background"), InsetBox::Shaded);
	translator.addPair(_("double frame"), InsetBox::Doublebox);
	return translator;
}


BoxTranslator const & boxtranslator()
{
	static BoxTranslator const translator = initBoxtranslator();
	return translator;
}


BoxTranslatorLoc const & boxtranslator_loc()
{
	static BoxTranslatorLoc const translator = initBoxtranslatorLoc();
	return translator;
}

} // namespace


/////////////////////////////////////////////////////////////////////////
//
// InsetBox
//
/////////////////////////////////////////////////////////////////////////

InsetBox::InsetBox(Buffer * buffer, string const & label)
	: InsetCollapsible(buffer), params_(label)
{}


docstring InsetBox::layoutName() const
{
	// FIXME: UNICODE
	return from_ascii("Box:" + params_.type);
}


void InsetBox::write(ostream & os) const
{
	params_.write(os);
	InsetCollapsible::write(os);
}


void InsetBox::read(Lexer & lex)
{
	params_.read(lex);
	InsetCollapsible::read(lex);
}


void InsetBox::setButtonLabel()
{
	BoxType const btype = boxtranslator().find(params_.type);

	docstring const type = _("Box");

	docstring inner;
	if (params_.inner_box) {
		if (params_.use_parbox)
			inner = _("Parbox");
		else if (params_.use_makebox)
			inner = _("Makebox");
		else
			inner = _("Minipage");
	}

	docstring frame;
	if (btype != Frameless)
		frame = boxtranslator_loc().find(btype);

	docstring label;
	if (inner.empty() && frame.empty())
		label = type;
	else if (inner.empty())
		label = bformat(_("%1$s (%2$s)"),
			type, frame);
	else if (frame.empty())
		label = bformat(_("%1$s (%2$s)"),
			type, inner);
	else
		label = bformat(_("%1$s (%2$s, %3$s)"),
			type, inner, frame);
	setLabel(label);

	// set the frame color for the inset if the type is Boxed
	if (btype == Boxed)
		setFrameColor(lcolor.getFromLyXName(getFrameColor(true)));
	else
		setFrameColor(Color_collapsibleframe);
}


bool InsetBox::hasFixedWidth() const
{
	return !params_.width.empty() && params_.special == "none";
}


bool InsetBox::allowMultiPar() const
{
	return (params_.inner_box && !params_.use_makebox)
		|| params_.type == "Shaded" || params_.type == "Framed";
}


void InsetBox::metrics(MetricsInfo & mi, Dimension & dim) const
{
	// back up textwidth.
	int textwidth_backup = mi.base.textwidth;
	if (hasFixedWidth())
		mi.base.textwidth = mi.base.inPixels(params_.width);
	InsetCollapsible::metrics(mi, dim);
	// restore textwidth.
	mi.base.textwidth = textwidth_backup;
}


bool InsetBox::forcePlainLayout(idx_type) const
{
	return (!params_.inner_box || params_.use_makebox)
		&& params_.type != "Shaded" && params_.type != "Framed";
}


bool InsetBox::needsCProtection(bool const maintext, bool const fragile) const
{
	// We need to cprotect boxes that use minipages as inner box
	// in fragile context
	if (fragile && params_.inner_box && !params_.use_parbox && !params_.use_makebox)
		return true;

	return InsetText::needsCProtection(maintext, fragile);
}


ColorCode InsetBox::backgroundColor(PainterInfo const &) const
{
	// we only support background color for 3 types
	if (params_.type != "Shaded" && params_.type != "Frameless" && params_.type != "Boxed")
		return getLayout().bgcolor();

	if (params_.type == "Shaded") {
		// This is the local color (not overridden by other documents)
		// the color might not yet be initialized for new documents
		ColorCode c = lcolor.getFromLyXName("boxbgcolor@" + buffer().fileName().absFileName(), false);
		if (c == Color_none)
			return getLayout().bgcolor();
		return c;
	}

	if (params_.backgroundcolor != "none")
		return lcolor.getFromLyXName(getBackgroundColor(true));

	return getLayout().bgcolor();
}


LyXAlignment InsetBox::contentAlignment() const
{
	// Custom horizontal alignment is only allowed with a fixed width
	// and if either makebox or no inner box are used
	if (params_.width.empty() || !(params_.use_makebox || !params_.inner_box))
		return LYX_ALIGN_NONE;

	// The default value below is actually irrelevant
	LyXAlignment align = LYX_ALIGN_NONE;
	switch (params_.hor_pos) {
	case 'l':
		align = LYX_ALIGN_LEFT;
		break;
	case 'c':
		align = LYX_ALIGN_CENTER;
		break;
	case 'r':
		align = LYX_ALIGN_RIGHT;
		break;
	case 's':
		align = LYX_ALIGN_BLOCK;
		break;
	}
	return align;
}


void InsetBox::doDispatch(Cursor & cur, FuncRequest & cmd)
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY: {
		//lyxerr << "InsetBox::dispatch MODIFY" << endl;
		string const first_arg = cmd.getArg(0);
		bool const change_type = first_arg == "changetype";
		bool const for_box = first_arg == "box";
		if (!change_type && !for_box) {
			// not for us
			// this will not be handled higher up
			cur.undispatched();
			return;
		}
		cur.recordUndoInset(this);
		if (change_type) {
			params_.type = cmd.getArg(1);
			// set a makebox if there is no inner box but Frameless was executed
			// otherwise the result would be a non existent box (no inner AND outer box)
			// (this was LyX bug 8712)
			if (params_.type == "Frameless" && !params_.inner_box) {
				params_.use_makebox = true;
				params_.inner_box = true;
			}
			// handle the opposite case
			if (params_.type == "Boxed" && params_.use_makebox) {
				params_.use_makebox = false;
				params_.inner_box = false;
			}
		} else
			string2params(to_utf8(cmd.argument()), params_);
		setButtonLabel();
		break;
	}

	default:
		InsetCollapsible::doDispatch(cur, cmd);
		break;
	}
}


bool InsetBox::getStatus(Cursor & cur, FuncRequest const & cmd,
		FuncStatus & flag) const
{
	switch (cmd.action()) {

	case LFUN_INSET_MODIFY: {
		string const first_arg = cmd.getArg(0);
		if (first_arg == "changetype") {
			string const type = cmd.getArg(1);
			flag.setOnOff(type == params_.type);
			flag.setEnabled(!params_.inner_box || type != "Framed");
			return true;
		}
		if (first_arg == "box") {
			flag.setEnabled(true);
			return true;
		}
		return InsetCollapsible::getStatus(cur, cmd, flag);
	}

	case LFUN_INSET_DIALOG_UPDATE:
		flag.setEnabled(true);
		return true;

	default:
		return InsetCollapsible::getStatus(cur, cmd, flag);
	}
}


const string defaultThick = "0.4pt";
const string defaultSep = "3pt";
const string defaultShadow = "4pt";

void InsetBox::latex(otexstream & os, OutputParams const & runparams) const
{
	BoxType btype = boxtranslator().find(params_.type);

	string width_string = params_.width.asLatexString();
	string thickness_string = params_.thickness.asLatexString();
	string separation_string = params_.separation.asLatexString();
	string shadowsize_string = params_.shadowsize.asLatexString();
	bool stdwidth = false;
	string const cprotect = hasCProtectContent(runparams.moving_arg) ? "\\cprotect" : string();
	// Colored boxes in RTL need to be wrapped into \beginL...\endL
	string maybeBeginL;
	string maybeEndL;
	bool needEndL = false;
	if (!runparams.isFullUnicode()
	    && runparams.local_font && runparams.local_font->isRightToLeft()) {
		maybeBeginL = "\\beginL";
		maybeEndL = "\\endL";
	}
	// in general the overall width of some decorated boxes is wider thean the inner box
	// we could therefore calculate the real width for all sizes so that if the user wants
	// e.g. 0.1\columnwidth or 2cm he gets exactly this size
	// however this makes problems when importing TeX code
	// therefore only recalculate for the most common case that the box should not protrude
	// the page margins
	if (params_.inner_box
		&& ((width_string.find("1\\columnwidth") != string::npos
			|| width_string.find("1\\textwidth") != string::npos)
			|| width_string.find("1\\paperwidth") != string::npos
			|| width_string.find("1\\linewidth") != string::npos)) {
		stdwidth = true;
		switch (btype) {
		case Frameless:
			break;
		case Framed:
			width_string += " - 2\\FrameSep - 2\\FrameRule";
			break;
		case Boxed:
			width_string += " - 2\\fboxsep - 2\\fboxrule";
			break;
		case Shaded:
			break;
		case ovalbox:
			width_string += " - 2\\fboxsep - 0.8pt";
			break;
		case Ovalbox:
			width_string += " - 2\\fboxsep - 1.6pt";
			break;
		case Shadowbox:
			width_string += " - 2\\fboxsep - 2\\fboxrule - \\shadowsize";
			break;
		case Doublebox:
			width_string += " - 2\\fboxsep - 7.5\\fboxrule - 1pt";
			break;
		}
	}

	os << safebreakln;
	if (runparams.lastid != -1)
		os.texrow().start(runparams.lastid, runparams.lastpos);

	// adapt column/text width correctly also if paragraphs indented
	if (stdwidth && !(buffer().params().paragraph_separation))
		os << "\\noindent";

	bool needendgroup = false;
	switch (btype) {
	case Frameless:
		break;
	case Framed:
		if (thickness_string != defaultThick) {
			os << "{\\FrameRule " << from_ascii(thickness_string);
			if (separation_string != defaultSep)
				os << "\\FrameSep " << from_ascii(separation_string);
		}
		if (separation_string != defaultSep && thickness_string == defaultThick)
			os << "{\\FrameSep " << from_ascii(separation_string);

		os << "\\begin{framed}%\n";
		break;
	case Boxed:
		if (thickness_string != defaultThick) {
			os << "{\\fboxrule " << from_ascii(thickness_string);
			if (separation_string != defaultSep)
				os << "\\fboxsep " << from_ascii(separation_string);
		}
		if (separation_string != defaultSep && thickness_string == defaultThick)
			os << "{\\fboxsep " << from_ascii(separation_string);
		if (!params_.inner_box && !width_string.empty()) {
			if (useFColorBox()) {
				os << maybeBeginL
				   << "\\fcolorbox{" << getFrameColor()
				   << "}{" << getBackgroundColor()
				   << "}{" << "\\makebox";
				needEndL = !maybeBeginL.empty();
			} else
				os << "\\framebox";
			// Special widths, see usrguide sec. 3.5
			// FIXME UNICODE
			if (params_.special != "none") {
				os << "[" << params_.width.value()
				   << '\\' << from_utf8(params_.special)
				   << ']';
			} else
				os << '[' << from_ascii(width_string)
				   << ']';
			// default horizontal alignment is 'c'
			if (params_.hor_pos != 'c')
				os << "[" << params_.hor_pos << "]";
		} else {
			if (useFColorBox()) {
				os << maybeBeginL
				   << "\\fcolorbox{" << getFrameColor()
				   << "}{" << getBackgroundColor() << "}";
				needEndL = !maybeBeginL.empty();
			} else {
				if (!cprotect.empty() && contains(runparams.active_chars, '^')) {
					// cprotect relies on ^ being on catcode 7
					os << "\\begingroup\\catcode`\\^=7";
					needendgroup = true;
				}
				os << cprotect << "\\fbox";
			}
		}
		os << "{";
		break;
	case ovalbox:
		if (!separation_string.empty() && separation_string != defaultSep)
			os << "{\\fboxsep " << from_ascii(separation_string);
		os << "\\ovalbox{";
		break;
	case Ovalbox:
		if (!separation_string.empty() && separation_string != defaultSep)
			os << "{\\fboxsep " << from_ascii(separation_string);
		os << "\\Ovalbox{";
		break;
	case Shadowbox:
		if (thickness_string != defaultThick) {
			os << "{\\fboxrule " << from_ascii(thickness_string);
			if (separation_string != defaultSep) {
				os << "\\fboxsep " << from_ascii(separation_string);
				if (shadowsize_string != defaultShadow)
					os << "\\shadowsize " << from_ascii(shadowsize_string);
			}
			if (shadowsize_string != defaultShadow	&& separation_string == defaultSep)
				os << "\\shadowsize " << from_ascii(shadowsize_string);
		}
		if (separation_string != defaultSep && thickness_string == defaultThick) {
				os << "{\\fboxsep " << from_ascii(separation_string);
				if (shadowsize_string != defaultShadow)
					os << "\\shadowsize " << from_ascii(shadowsize_string);
		}
		if (shadowsize_string != defaultShadow
				&& separation_string == defaultSep
				&& thickness_string == defaultThick)
				os << "{\\shadowsize " << from_ascii(shadowsize_string);
		os << "\\shadowbox{";
		break;
	case Shaded:
		// must be set later because e.g. the width settings only work when
		// it is inside a minipage or parbox
		os << maybeBeginL;
		needEndL = !maybeBeginL.empty();
		break;
	case Doublebox:
		if (thickness_string != defaultThick) {
			os << "{\\fboxrule " << from_ascii(thickness_string);
			if (separation_string != defaultSep)
				os << "\\fboxsep " << from_ascii(separation_string);
		}
		if (separation_string != defaultSep && thickness_string == defaultThick)
			os << "{\\fboxsep " << from_ascii(separation_string);
		os << "\\doublebox{";
		break;
	}

	if (params_.inner_box) {
		if (params_.use_parbox) {
			if (params_.backgroundcolor != "none" && btype == Frameless) {
				os << maybeBeginL << "\\colorbox{" << params_.backgroundcolor << "}{";
				needEndL = !maybeBeginL.empty();
			}
			os << "\\parbox";
		} else if (params_.use_makebox) {
			if (!width_string.empty()) {
				if (params_.backgroundcolor != "none") {
					os << maybeBeginL << "\\colorbox{" << params_.backgroundcolor << "}{";
					needEndL = !maybeBeginL.empty();
				}
				os << "\\makebox";
				// FIXME UNICODE
				// output the width and horizontal position
				if (params_.special != "none") {
					os << "[" << params_.width.value()
					   << '\\' << from_utf8(params_.special)
					   << ']';
				} else
					os << '[' << from_ascii(width_string)
					   << ']';
				if (params_.hor_pos != 'c')
					os << "[" << params_.hor_pos << "]";
			} else {
				if (params_.backgroundcolor != "none") {
					os << maybeBeginL << "\\colorbox{" << params_.backgroundcolor << "}";
					needEndL = !maybeBeginL.empty();
				}
				else
					os << "\\mbox";
			}
			os << "{";
		}
		else {
			if (params_.backgroundcolor != "none" && btype == Frameless) {
				os << maybeBeginL << "\\colorbox{" << params_.backgroundcolor << "}{";
				needEndL = !maybeBeginL.empty();
			}
			os << "\\begin{minipage}";
		}

		// output parameters for parbox and minipage
		if (!params_.use_makebox) {
			os << "[" << params_.pos << "]";
			if (params_.height_special == "none") {
				// FIXME UNICODE
				os << "[" << from_ascii(params_.height.asLatexString()) << "]";
			} else {
				// Special heights
				// set no optional argument when the value is the default "1\height"
				// (special units like \height are handled as "in")
				// but when the user has chosen a non-default inner_pos, the height
				// must be given: \minipage[pos][height][inner-pos]{width}
				if ((params_.height != Length("1in") ||
					params_.height_special != "totalheight") ||
					params_.inner_pos != params_.pos) {
						// FIXME UNICODE
						os << "[" << params_.height.value()
							<< "\\" << from_utf8(params_.height_special) << "]";
				}
			}
			if (params_.inner_pos != params_.pos)
				os << "[" << params_.inner_pos << "]";
			// FIXME UNICODE
			os << '{' << from_ascii(width_string) << '}';
			if (params_.use_parbox)
				os << "{";
		}

		os << "%\n";
	} // end if inner_box

	if (btype == Shaded) {
		os << "\\begin{shaded}%\n";
	}

	InsetText::latex(os, runparams);

	if (btype == Shaded)
		os << "\\end{shaded}";

	if (params_.inner_box) {
		if (params_.use_parbox || params_.use_makebox)
			os << "%\n}";
		else
			os << "%\n\\end{minipage}";
		if (params_.backgroundcolor != "none" && btype == Frameless
			&& !(params_.use_makebox && width_string.empty()))
			os << "}";
	}

	switch (btype) {
	case Frameless:
		break;
	case Framed:
		os << "\\end{framed}";
		if (separation_string != defaultSep || thickness_string != defaultThick)
			os << "}";
		break;
	case Boxed:
		os << "}";
		if (!params_.inner_box && !width_string.empty() && useFColorBox())
			os << "}";
		if (separation_string != defaultSep || thickness_string != defaultThick)
			os << "}";
		if (needendgroup)
			os << "\\endgroup";
		break;
	case ovalbox:
		os << "}";
		if (separation_string != defaultSep)
			os << "}";
		break;
	case Ovalbox:
		os << "}";
		if (separation_string != defaultSep)
			os << "}";
		break;
	case Doublebox:
		os << "}";
		if (separation_string != defaultSep || thickness_string != defaultThick)
			os << "}";
		break;
	case Shadowbox:
		os << "}";
		if (separation_string != defaultSep
			|| thickness_string != defaultThick
			|| shadowsize_string != defaultShadow)
			os << "}";
		break;
	case Shaded:
		// already done
		break;
	}
	if (needEndL)
		os << maybeEndL;
}


int InsetBox::plaintext(odocstringstream & os,
       OutputParams const & runparams, size_t max_length) const
{
	BoxType const btype = boxtranslator().find(params_.type);

	switch (btype) {
		case Frameless:
			break;
		case Framed:
		case Boxed:
			os << "[\n";
			break;
		case ovalbox:
			os << "(\n";
			break;
		case Ovalbox:
			os << "((\n";
			break;
		case Shadowbox:
		case Shaded:
			os << "[/\n";
			break;
		case Doublebox:
			os << "[[\n";
			break;
	}

	InsetText::plaintext(os, runparams, max_length);

	int len = 0;
	switch (btype) {
		case Frameless:
			os << "\n";
			break;
		case Framed:
		case Boxed:
			os << "\n]";
			len = 1;
			break;
		case ovalbox:
			os << "\n)";
			len = 1;
			break;
		case Ovalbox:
			os << "\n))";
			len = 2;
			break;
		case Shadowbox:
		case Shaded:
			os << "\n/]";
			len = 2;
			break;
		case Doublebox:
			os << "\n]]";
			len = 2;
			break;
	}

	return PLAINTEXT_NEWLINE + len; // len chars on a separate line
}


void InsetBox::docbook(XMLStream & xs, OutputParams const & runparams) const
{
	// There really should be a wrapper tag for this layout.
	bool hasBoxTag = !getLayout().docbookwrappertag().empty();
	if (!hasBoxTag)
		LYXERR0("Assertion failed: box layout " + getLayout().name() + " missing DocBookWrapperTag.");

	// Avoid nesting boxes in DocBook, it's not allowed. Only make the check for <sidebar> to avoid destroying
	// tags if this is not the wrapper tag for this layout (unlikely).
	bool isAlreadyInBox = hasBoxTag && xs.isTagOpen(xml::StartTag(getLayout().docbookwrappertag()));

	bool outputBoxTag = hasBoxTag && !isAlreadyInBox;

	// Generate the box tag (typically, <sidebar>).
	if (outputBoxTag) {
		if (!xs.isLastTagCR())
			xs << xml::CR();

		xs << xml::StartTag(getLayout().docbookwrappertag(), getLayout().docbookwrapperattr());
		xs << xml::CR();
	}

	// If the box starts with a sectioning item, use as box title.
	auto current_par = paragraphs().begin();
	if (current_par->layout().category() == from_utf8("Sectioning")) {
		// Only generate the first paragraph.
		current_par = makeAny(text(), buffer(), xs, runparams, paragraphs().begin());
	}

	// Don't call InsetText::docbook, as this would generate all paragraphs in the inset, not the ones we are
	// interested in. The best solution would be to call docbookParagraphs with an updated OutputParams object to only
	// generate paragraphs after the title, but it leads to strange crashes, as if text().paragraphs() then returns
	// a smaller set of paragrphs.
	// Elements in the box must keep their paragraphs.
	auto rp = runparams;
	rp.docbook_in_par = false;
	rp.docbook_force_pars = true;

	xs.startDivision(false);
	while (current_par != paragraphs().end())
		current_par = makeAny(text(), buffer(), xs, rp, current_par);
	xs.endDivision();

	// Close the box.
	if (outputBoxTag) {
		if (!xs.isLastTagCR())
			xs << xml::CR();

		xs << xml::EndTag(getLayout().docbookwrappertag());
		xs << xml::CR();
	}
}


docstring InsetBox::xhtml(XMLStream & xs, OutputParams const & runparams) const
{
	// construct attributes
	string attrs = "class='" + params_.type + "'";
	string style;
	if (!params_.width.empty()) {
		string w = params_.width.asHTMLString();
		if (w != "100%")
			style += ("width: " + params_.width.asHTMLString() + "; ");
	}
	// The special heights don't really mean anything for us.
	if (!params_.height.empty() && params_.height_special == "none")
		style += ("height: " + params_.height.asHTMLString() + "; ");
	if (!style.empty())
		attrs += " style='" + style + "'";

	xs << xml::StartTag("div", attrs);
	XHTMLOptions const opts = InsetText::WriteLabel | InsetText::WriteInnerTag;
	InsetText::insetAsXHTML(xs, runparams, opts);
	xs << xml::EndTag("div");
	return docstring();
}


void InsetBox::validate(LaTeXFeatures & features) const
{
	BoxType btype = boxtranslator().find(params_.type);
	switch (btype) {
	case Frameless:
		if (params_.backgroundcolor != "none") {
			if (theLaTeXColors().isLaTeXColor(params_.backgroundcolor)) {
				LaTeXColor const lc = theLaTeXColors().getLaTeXColor(params_.backgroundcolor);
				for (auto const & r : lc.req())
					features.require(r);
				features.require("xcolor");
				if (!lc.model().empty())
					features.require("xcolor:" + lc.model());
			} else
				features.require("color");
		}
		break;
	case Framed:
		features.require("calc");
		features.require("framed");
		break;
	case Boxed:
		features.require("calc");
		if (useFColorBox()) {
			bool need_xcolor = false;
			if (theLaTeXColors().isLaTeXColor(params_.backgroundcolor)) {
				LaTeXColor const lc = theLaTeXColors().getLaTeXColor(params_.backgroundcolor);
				for (auto const & r : lc.req())
					features.require(r);
				features.require("xcolor");
				need_xcolor = true;
				if (!lc.model().empty())
					features.require("xcolor:" + lc.model());
			}
			if (theLaTeXColors().isLaTeXColor(params_.framecolor)) {
				LaTeXColor const lc = theLaTeXColors().getLaTeXColor(params_.framecolor);
				for (auto const & r : lc.req())
					features.require(r);
				features.require("xcolor");
				need_xcolor = true;
				if (!lc.model().empty())
					features.require("xcolor:" + lc.model());
			}
			if (!need_xcolor)
				features.require("color");
		}
		break;
	case ovalbox:
	case Ovalbox:
	case Shadowbox:
	case Doublebox:
		features.require("calc");
		features.require("fancybox");
		break;
	case Shaded: {
		features.require("xcolor");
		if (theLaTeXColors().isLaTeXColor(buffer().params().boxbgcolor)) {
			LaTeXColor const lc = theLaTeXColors().getLaTeXColor(buffer().params().boxbgcolor);
			for (auto const & r : lc.req())
				features.require(r);
			if (!lc.model().empty())
				features.require("xcolor:" + lc.model());
		}
		features.require("framed");
		break;
	}
	}
	InsetCollapsible::validate(features);
}


string InsetBox::contextMenuName() const
{
	return "context-box";
}


string InsetBox::params2string(InsetBoxParams const & params)
{
	ostringstream data;
	data << "box" << ' ';
	params.write(data);
	return data.str();
}


void InsetBox::string2params(string const & in, InsetBoxParams & params)
{
	if (in.empty())
		return;

	istringstream data(in);
	Lexer lex;
	lex.setStream(data);

	string name;
	lex >> name;
	if (!lex || name != "box") {
		LYXERR0("InsetBox::string2params(" << in << ")\n"
					  "Expected arg 1 to be \"box\"\n");
		return;
	}

	// This is part of the inset proper that is usually swallowed
	// by Text::readInset
	string id;
	lex >> id;
	if (!lex || id != "Box") {
		LYXERR0("InsetBox::string2params(" << in << ")\n"
					  "Expected arg 2 to be \"Box\"\n");
	}

	params = InsetBoxParams(string());
	params.read(lex);
}


void InsetBox::registerLyXColor(string const & value) const
{
	if (!lcolor.isKnownLyXName(value)) {
		if (theLaTeXColors().isLaTeXColor(value)) {
			LaTeXColor const lc = theLaTeXColors().getLaTeXColor(value);
			string const lyxname = lc.name();
			lcolor.setColor(lyxname, lc.hexname());
			lcolor.setLaTeXName(lyxname, lc.latex());
			lcolor.setGUIName(lyxname, to_ascii(lc.guiname()));
		}
	}
}


string const InsetBox::getFrameColor(bool const gui) const
{
	if (params_.framecolor == "default")
		return gui ? "foreground" : "black";
	
	registerLyXColor(params_.framecolor);
	if (!lcolor.isKnownLyXName(params_.framecolor))
		return gui ? "foreground" : "black";
	return gui ? params_.framecolor
		   : lcolor.getLaTeXName(lcolor.getFromLyXName(params_.framecolor));
}


string const InsetBox::getBackgroundColor(bool const gui) const
{
	if (params_.backgroundcolor == "none")
		return (gui) ? "page_backgroundcolor"
			     : "white";
	registerLyXColor(params_.backgroundcolor);
	if (!lcolor.isKnownLyXName(params_.backgroundcolor))
		return (gui) ? "page_backgroundcolor"
			     : "white";
	return gui ? params_.backgroundcolor
		   : lcolor.getLaTeXName(lcolor.getFromLyXName(params_.backgroundcolor));
}


bool InsetBox::useFColorBox() const
{
	// we need an \fcolorbox if the framecolor or the backgroundcolor
	// is non-default. We also do it with black and white for consistency.
	return params_.framecolor != "default" || params_.backgroundcolor != "none";
}


/////////////////////////////////////////////////////////////////////////
//
// InsetBoxParams
//
/////////////////////////////////////////////////////////////////////////

InsetBoxParams::InsetBoxParams(string const & label)
	: type(label),
	  use_parbox(false),
	  use_makebox(false),
	  inner_box(true),
	  width(Length("100col%")),
	  special("none"),
	  pos('t'),
	  hor_pos('c'),
	  inner_pos('t'),
	  height(Length("1in")),
	  height_special("totalheight"), // default is 1\\totalheight
	  thickness(Length(defaultThick)),
	  separation(Length(defaultSep)),
	  shadowsize(Length(defaultShadow)),
	  framecolor("default"),
	  backgroundcolor("none")
{}


void InsetBoxParams::write(ostream & os) const
{
	os << "Box " << type << "\n";
	os << "position \"" << pos << "\"\n";
	os << "hor_pos \"" << hor_pos << "\"\n";
	os << "has_inner_box " << inner_box << "\n";
	os << "inner_pos \"" << inner_pos << "\"\n";
	os << "use_parbox " << use_parbox << "\n";
	os << "use_makebox " << use_makebox << "\n";
	os << "width \"" << width.asString() << "\"\n";
	os << "special \"" << special << "\"\n";
	os << "height \"" << height.asString() << "\"\n";
	os << "height_special \"" << height_special << "\"\n";
	os << "thickness \"" << thickness.asString() << "\"\n";
	os << "separation \"" << separation.asString() << "\"\n";
	os << "shadowsize \"" << shadowsize.asString() << "\"\n";
	os << "framecolor \"" << framecolor << "\"\n";
	os << "backgroundcolor \"" << backgroundcolor << "\"\n";
}


void InsetBoxParams::read(Lexer & lex)
{
	lex.setContext("InsetBoxParams::read");
	lex >> type;
	lex >> "position" >> pos;
	lex >> "hor_pos" >> hor_pos;
	lex >> "has_inner_box" >> inner_box;
	if (type == "Framed")
		inner_box = false;
	lex >> "inner_pos" >> inner_pos;
	lex >> "use_parbox" >> use_parbox;
	lex >> "use_makebox" >> use_makebox;
	lex >> "width" >> width;
	lex >> "special" >> special;
	lex >> "height" >> height;
	lex >> "height_special" >> height_special;
	lex >> "thickness" >> thickness;
	lex >> "separation" >> separation;
	lex >> "shadowsize" >> shadowsize;
	lex >> "framecolor" >> framecolor;
	lex >> "backgroundcolor" >> backgroundcolor;
}


} // namespace lyx
