/**
 * \file GuiLabel.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author John Levon
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiLabel.h"

#include "qt_helpers.h"

#include "insets/InsetLabel.h"

#include <QRegularExpressionValidator>

using namespace std;

namespace lyx {
namespace frontend {

/////////////////////////////////////////////////////////////////
//
// GuiLabel
//
/////////////////////////////////////////////////////////////////

GuiLabel::GuiLabel(QWidget * parent) : InsetParamsWidget(parent)
{
	setupUi(this);

	connect(keywordED, SIGNAL(textChanged(const QString &)),
		this, SIGNAL(changed()));

	// disallow the input of , and @ as they are used as separators
	QRegularExpression nocomma_re("[^,@]+");
	keywordED->setValidator(new QRegularExpressionValidator(nocomma_re, keywordED));

	setFocusProxy(keywordED);
}


void GuiLabel::paramsToDialog(Inset const * inset)
{
	InsetLabel const * label = static_cast<InsetLabel const *>(inset);
	InsetCommandParams const & params = label->params();
	setKeyword(toqstr(params["name"]));
}


docstring GuiLabel::dialogToParams() const
{
	InsetCommandParams params(insetCode());
	params["name"] = qstring_to_ucs4(keywordED->text());
	return from_utf8(InsetLabel::params2string(params));
}


bool GuiLabel::initialiseParams(std::string const & sdata)
{
	InsetCommandParams p(insetCode());
	if (!InsetCommand::string2params(sdata, p))
		return false;
	setKeyword(toqstr(p["name"]));
	return true;
}


bool GuiLabel::checkWidgets(bool readonly) const
{
	keywordED->setReadOnly(readonly);
	if (!InsetParamsWidget::checkWidgets())
		return false;
	return !keywordED->text().isEmpty();
}


void GuiLabel::setKeyword(QString const & keyword)
{
	keywordED->setText(keyword);
	// select without prefix
	int const colonPos = keyword.indexOf(':');
	if (colonPos == -1)
		keywordED->selectAll();
	else
		keywordED->setSelection(colonPos + 1, keyword.length() - colonPos + 1);
}

} // namespace frontend
} // namespace lyx

#include "moc_GuiLabel.cpp"
