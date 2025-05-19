/**
 * \file GuiShowFile.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 * \author Herbert Voß
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiShowFile.h"

#include "qt_helpers.h"
#include "support/filetools.h"


using namespace std;
using namespace lyx::support;

namespace lyx {
namespace frontend {


GuiShowFile::GuiShowFile(GuiView & lv)
	: GuiDialog(lv, "file", qt_("Show File"))
{
	setupUi(this);

	connect(buttonBox, SIGNAL(clicked(QAbstractButton *)),
		this, SLOT(slotButtonBox(QAbstractButton *)));

	bc().setPolicy(ButtonPolicy::OkCancelPolicy);
	bc().setCancel(buttonBox->button(QDialogButtonBox::Cancel));
}


void GuiShowFile::updateContents()
{
	setWindowTitle(onlyFileName(toqstr(filename_.absFileName())));

	QString contents = toqstr(filename_.fileContents("UTF-8"));
	if (contents.isEmpty())
		contents = qt_("Error -> Cannot load file!");

	textTB->setPlainText(contents);
}


bool GuiShowFile::initialiseParams(string const & sdata)
{
	filename_ = FileName(sdata);
	return true;
}


void GuiShowFile::clearParams()
{
	filename_.erase();
}


} // namespace frontend
} // namespace lyx

#include "moc_GuiShowFile.cpp"
