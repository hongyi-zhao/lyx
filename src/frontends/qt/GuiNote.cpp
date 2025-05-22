/**
 * \file GuiNote.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiNote.h"
#include "FuncRequest.h"

using namespace std;

namespace lyx {
namespace frontend {

GuiNote::GuiNote(GuiView & lv)
	: GuiDialog(lv, "note", qt_("Note Settings"))
{
	setupUi(this);

	connect(buttonBox, SIGNAL(clicked(QAbstractButton *)),
		this, SLOT(slotButtonBox(QAbstractButton *)));

	connect(noteRB, SIGNAL(clicked()), this, SLOT(change_adaptor()));
	connect(greyedoutRB, SIGNAL(clicked()), this, SLOT(change_adaptor()));
	connect(commentRB, SIGNAL(clicked()), this, SLOT(change_adaptor()));

	bc().setPolicy(ButtonPolicy::NoRepeatedApplyReadOnlyPolicy);
	bc().setOK(buttonBox->button(QDialogButtonBox::Ok));
	bc().setCancel(buttonBox->button(QDialogButtonBox::Cancel));
}


void GuiNote::change_adaptor()
{
	changed();
}


void GuiNote::updateContents()
{
	switch (params_.type) {
	case InsetNoteParams::Note:
		noteRB->setChecked(true);
		break;
	case InsetNoteParams::Comment:
		commentRB->setChecked(true);
		break;
	case InsetNoteParams::Greyedout:
		greyedoutRB->setChecked(true);
		break;
	}
}


void GuiNote::applyView()
{
	if (greyedoutRB->isChecked())
		params_.type = InsetNoteParams::Greyedout;
	else if (commentRB->isChecked())
		params_.type = InsetNoteParams::Comment;
	else
		params_.type = InsetNoteParams::Note;
}


bool GuiNote::initialiseParams(string const & sdata)
{
	InsetNote::string2params(sdata, params_);
	return true;
}


void GuiNote::clearParams()
{
	params_ = InsetNoteParams();
}


void GuiNote::dispatchParams()
{
	dispatch(FuncRequest(getLfun(), InsetNote::params2string(params_)));
}


} // namespace frontend
} // namespace lyx

#include "moc_GuiNote.cpp"
