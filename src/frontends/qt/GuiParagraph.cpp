/**
 * \file GuiParagraph.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Edwin Leuven
 * \author Richard Kimberly Heck
 * \author Abdelrazak Younes
 * \author Angus Leeming
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiParagraph.h"

#include "qt_helpers.h"

#include "Buffer.h"
#include "BufferParams.h"
#include "BufferView.h"
#include "Cursor.h"
#include "FuncRequest.h"
#include "GuiView.h"
#include "Layout.h"
#include "Paragraph.h"
#include "ParagraphParameters.h"
#include "Spacing.h"

#include "support/debug.h"
#include "support/Lexer.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVariant>

#include <sstream>

using namespace std;

namespace lyx {
namespace frontend {

GuiParagraph::GuiParagraph(GuiView & lv)
	: DialogView(lv, "paragraph", qt_("Paragraph Settings"))
{
	setupUi(this);

	// fix height to minimum
	setFixedHeight(sizeHint().height());

	connect(alignDefaultRB, SIGNAL(clicked()), this, SLOT(changed()));
	connect(alignJustRB, SIGNAL(clicked()), this, SLOT(changed()));
	connect(alignLeftRB, SIGNAL(clicked()), this, SLOT(changed()));
	connect(alignRightRB, SIGNAL(clicked()), this, SLOT(changed()));
	connect(alignCenterRB, SIGNAL(clicked()), this, SLOT(changed()));
	connect(linespacing, SIGNAL(activated(int)), this, SLOT(changed()));
	connect(linespacingValue, SIGNAL(textChanged(QString)),
		this, SLOT(changed()));
	connect(noindentCB, SIGNAL(clicked()), this, SLOT(changed()));
	connect(labelWidth, SIGNAL(textChanged(QString)),
		this, SLOT(changed()));

#ifdef Q_OS_MAC
	// On Mac it's common to have tool windows which are always in the
	// foreground and are hidden when the main window is not focused.
	setWindowFlags(Qt::Tool);
	synchronizedViewCB->setChecked(true);
#else
	synchronizedViewCB->setChecked(false);
#endif

	setButtons(synchronizedViewCB->isChecked());
	QDoubleValidator * val = new QDoubleValidator(linespacingValue);
	val->setNotation(QDoubleValidator::StandardNotation);
	linespacingValue->setValidator(val);

	labelWidth->setWhatsThis(qt_(
		"As described in the User Guide, the width of"
		" this text determines the width of the label part"
		" of each item in environments like List and"
		" Description.\n"
		"\n"
		" Normally, you won't need to set this,"
		" since the largest label width of all the"
		" items is used."
	));

	radioMap_[LYX_ALIGN_LAYOUT] = alignDefaultRB;
	radioMap_[LYX_ALIGN_BLOCK]  = alignJustRB;
	radioMap_[LYX_ALIGN_LEFT]   = alignLeftRB;
	radioMap_[LYX_ALIGN_RIGHT]  = alignRightRB;
	radioMap_[LYX_ALIGN_CENTER] = alignCenterRB;

	alignDefaultLabel_ = alignDefaultRB->text();
}


void GuiParagraph::on_linespacing_activated(int index)
{
	linespacingValue->setEnabled(index == 4);
}


void GuiParagraph::checkAlignmentRadioButtons()
{
	static std::map<LyXAlignment, QString> labelMap_;
	if (labelMap_.empty()) {
		labelMap_[LYX_ALIGN_BLOCK]  = qt_("Justified");
		labelMap_[LYX_ALIGN_LEFT]   = qt_("Left");
		labelMap_[LYX_ALIGN_RIGHT]  = qt_("Right");
		labelMap_[LYX_ALIGN_CENTER] = qt_("Center");
	}

	RadioMap::iterator it = radioMap_.begin();
	for (; it != radioMap_.end(); ++it) {
		LyXAlignment const align = it->first;
		it->second->setEnabled(align & alignPossible());
	}
	if (haveMultiParSelection())
		alignDefaultRB->setText(alignDefaultLabel_);
	else
		alignDefaultRB->setText(alignDefaultLabel_ + " ("
			+ labelMap_[bufferview()->cursor().innerParagraph().getDefaultAlign(buffer().params())] + ")");
}


void GuiParagraph::alignmentToRadioButtons(LyXAlignment align)
{
	RadioMap::const_iterator it = radioMap_.begin();
	for (;it != radioMap_.end(); ++it) {
		it->second->blockSignals(true);
		it->second->setChecked(align == it->first);
		it->second->blockSignals(false);
	}
}


LyXAlignment GuiParagraph::getAlignmentFromDialog() const
{
	LyXAlignment alignment = LYX_ALIGN_NONE;
	RadioMap::const_iterator it = radioMap_.begin();
	for (; it != radioMap_.end(); ++it) {
		if (it->second->isChecked()) {
			alignment = it->first;
			break;
		}
	}
	return alignment;
}


void GuiParagraph::on_synchronizedViewCB_stateChanged(int state)
{
	bool in_sync = state == Qt::Checked;
	setButtons(in_sync);

	// Apply pending changes
	if (in_sync) 
		changed();
}


void GuiParagraph::setButtons(bool const in_sync)
{
	buttonBox->button(QDialogButtonBox::Reset)->setEnabled(!in_sync);
	buttonBox->button(QDialogButtonBox::Apply)->setEnabled(!in_sync);
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!in_sync);
	if (in_sync) 
		buttonBox->button(QDialogButtonBox::Cancel)->setText(qt_("&Close"));
	else
		buttonBox->button(QDialogButtonBox::Cancel)->setText(qt_("&Cancel"));
}


void GuiParagraph::changed()
{
	QLocale loc;
	// We apply immediately, except if we have custom line spacing
	// with an intermediate result (trailing decimal separator) or
	// an invalid value (which might as well be intermediate)
	if (synchronizedViewCB->isChecked()
	    && (linespacing->currentIndex() != 4
		|| (!linespacingValue->text().endsWith(loc.decimalPoint())
		    && linespacingValue->hasAcceptableInput())))
		applyView();
}


void GuiParagraph::on_buttonBox_clicked(QAbstractButton * button)
{
	switch (buttonBox->standardButton(button)) {
	case QDialogButtonBox::Ok:
		applyView();
		hide();
		break;
	case QDialogButtonBox::Apply:
		applyView();
		break;
	case QDialogButtonBox::Cancel:
		hide();
		break;
	case QDialogButtonBox::Reset:
		updateView();
		break;
	default:
		break;
	}
}


void GuiParagraph::applyView()
{
	params_ = params();

	params_.align(getAlignmentFromDialog());

	// get spacing
	Spacing::Space ls = Spacing::Default;
	string other;
	switch (linespacing->currentIndex()) {
	case 0:
		ls = Spacing::Default;
		break;
	case 1:
		ls = Spacing::Single;
		break;
	case 2:
		ls = Spacing::Onehalf;
		break;
	case 3:
		ls = Spacing::Double;
		break;
	case 4:
		ls = Spacing::Other;
		other = widgetToDoubleStr(linespacingValue);
		break;
	}

	Spacing const spacing(ls, other);
	params_.spacing(spacing);

	// label width
	params_.labelWidthString(qstring_to_ucs4(labelWidth->text()));
	// indentation
	params_.noindent(noindentCB->isChecked());

	dispatchParams();
}


void GuiParagraph::updateView()
{
	setButtons(synchronizedViewCB->isChecked());

	ParagraphParameters const & pp = params();

	// label width
	docstring const & labelwidth = pp.labelWidthString();
	if (hasLabelwidth()) {
		labelwidthGB->setEnabled(true);
		labelWidth->setText(toqstr(labelwidth));
	} else {
		labelwidthGB->setEnabled(false);
		labelWidth->setText(QString());
	}

	// alignment
	checkAlignmentRadioButtons();
	alignmentToRadioButtons(pp.align());

	//indentation
	bool const canindent = canIndent();
	noindentCB->setEnabled(canindent);
	noindentCB->setChecked(canindent && pp.noindent());

	// linespacing
	int ls;
	bool pending_input = false;
	Spacing const & space = pp.spacing();
	if (synchronizedViewCB->isChecked() && linespacingValue->hasFocus()) {
		// The user is about to enter custom spacing.
		// We thus stay in Custom mode.
		// This prevents the combo from e.g. immediately
		// switching to single if a user enters "1" in the
		// linespacingValue widget while aiming at e.g. "1.3"
		ls = 4;
		pending_input = true;
	} else {
		switch (space.getSpace()) {
		case Spacing::Single:
			ls = 1;
			break;
		case Spacing::Onehalf:
			ls = 2;
			break;
		case Spacing::Double:
			ls = 3;
			break;
		case Spacing::Other:
			ls = 4;
			break;
		default:
			ls = 0;
			break;
		}
	}
	linespacing->setCurrentIndex(ls);
	if (space.getSpace() == Spacing::Other || pending_input) {
		doubleToWidget(linespacingValue, space.getValue());
		linespacingValue->setEnabled(true);
	} else {
		linespacingValue->setText(QString());
		linespacingValue->setEnabled(false);
	}
	// Somewhere in the chain this can lose default status (#11417)
	buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
}


void GuiParagraph::enableView(bool enable)
{
	noindentCB->setEnabled(canIndent() && enable);
	linespacing->setEnabled(enable);
	labelWidth->setEnabled(enable);
	synchronizedViewCB->setEnabled(enable);
	buttonBox->button(QDialogButtonBox::Apply)->setEnabled(enable);
	buttonBox->button(QDialogButtonBox::Reset)->setEnabled(enable);
	if (!enable)
		synchronizedViewCB->setChecked(true);
	RadioMap::const_iterator it = radioMap_.begin();
	for (; it != radioMap_.end(); ++it)
		it->second->setEnabled(enable);
}


ParagraphParameters const & GuiParagraph::params() const
{
	if (haveMultiParSelection()) {
		// FIXME: in case of multi-paragraph selection, it would be nice to
		// initialise the parameters that are common to all paragraphs.
		static ParagraphParameters const empty;
		return empty;
	}
	return bufferview()->cursor().innerParagraph().params();
}


void GuiParagraph::dispatchParams()
{
	ostringstream os;
	params_.write(os);
	FuncRequest const fr(getLfun(), os.str());
	dispatch(fr);
}


bool GuiParagraph::haveMultiParSelection() const
{
	Cursor const & cur = bufferview()->cursor();
	return cur.selection() && cur.selBegin().pit() != cur.selEnd().pit();
}


bool GuiParagraph::canIndent() const
{
	Layout const lay = bufferview()->cursor().innerParagraph().layout();
	if (buffer().params().paragraph_separation
		== BufferParams::ParagraphIndentSeparation)
		return (lay.toggle_indent != ITOGGLE_NEVER);
	return (lay.toggle_indent == ITOGGLE_ALWAYS);
}


LyXAlignment GuiParagraph::alignPossible() const
{
	return bufferview()->cursor().innerParagraph().layout().alignpossible;
}


bool GuiParagraph::hasLabelwidth() const
{
	Layout layout = bufferview()->cursor().innerParagraph().layout();
	return (layout.margintype == MARGIN_MANUAL
		|| layout.latextype == LATEX_BIB_ENVIRONMENT);
}


void GuiParagraph::saveSession(QSettings & settings) const
{
	Dialog::saveSession(settings);
	settings.setValue(sessionKey() + "/autoapply", synchronizedViewCB->isChecked());
}


void GuiParagraph::restoreSession()
{
	Dialog::restoreSession();
	QSettings settings;
	synchronizedViewCB->setChecked(
		settings.value(sessionKey() + "/autoapply").toBool());
}


} // namespace frontend
} // namespace lyx

#include "moc_GuiParagraph.cpp"
