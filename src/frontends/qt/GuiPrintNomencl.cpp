/**
 * \file GuiPrintNomencl.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jürgen Spitzmüller
 * \author Uwe Stöhr
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiPrintNomencl.h"

#include "LengthCombo.h"
#include "qt_helpers.h"
#include "Validator.h"

#include "FuncRequest.h"

#include "insets/InsetNomencl.h"

#include "support/debug.h"
#include "support/gettext.h"
#include "support/lstrings.h"

using namespace std;

namespace lyx {
namespace frontend {

GuiPrintNomencl::GuiPrintNomencl(QWidget * parent) : InsetParamsWidget(parent)
{
	setupUi(this);

	connect(valueLE, SIGNAL(textChanged(QString)),
		this, SIGNAL(changed()));
	connect(unitLC, SIGNAL(selectionChanged(lyx::Length::UNIT)),
		this, SIGNAL(changed()));

	valueLE->setValidator(unsignedLengthValidator(valueLE));

	// initialize the length validator
	addCheckedWidget(valueLE, valueLA);

	setWidthCO->addItem(qt_("Default"),
		QVariant(toqstr("none")));
	setWidthCO->addItem(qt_("Longest label width (guessed)"),
		QVariant(toqstr("auto")));
	setWidthCO->addItem(qt_("Longest label width (set)"),
		QVariant(toqstr("textwidth")));
	setWidthCO->addItem(qt_("Custom"),
		QVariant(toqstr("custom")));
}


void GuiPrintNomencl::on_setWidthCO_activated(int /*i*/)
{
	changed();
}


void GuiPrintNomencl::paramsToDialog(InsetCommandParams const & params)
{
	QString const set_width = toqstr(params["set_width"]);

	setWidthCO->setCurrentIndex(setWidthCO->findData(set_width));

	if (set_width == "textwidth")
		valueLE->setText(toqstr(params["width"]));
	else
		lengthToWidgets(valueLE, unitLC,
				params["width"], Length::defaultUnit());
}


void GuiPrintNomencl::paramsToDialog(Inset const * inset)
{
	InsetNomencl const * nomencl = static_cast<InsetNomencl const *>(inset);
	paramsToDialog(nomencl->params());
}


docstring GuiPrintNomencl::dialogToParams() const
{
	InsetCommandParams params(insetCode());
	docstring const set_width = qstring_to_ucs4(setWidthCO->itemData(
		setWidthCO->currentIndex()).toString());
	params["set_width"] = set_width;
	docstring width;
	if (set_width == from_ascii("custom"))
		width = from_utf8(widgetsToLength(valueLE, unitLC));
	else if (set_width == from_ascii("textwidth"))
		width = qstring_to_ucs4(valueLE->text());
	params["width"] = width;
	return from_ascii(InsetNomencl::params2string(params));
}


bool GuiPrintNomencl::checkWidgets(bool readonly) const
{
	QString const set_width =
		setWidthCO->itemData(setWidthCO->currentIndex()).toString();
	bool const custom = (set_width == "custom");
	bool const textwidth = (set_width == "textwidth");
	valueLE->setReadOnly(readonly);
	if (readonly) {
		setWidthCO->setEnabled(false);
		unitLC->setEnabled(false);
		valueLA->setEnabled(false);
	} else {
		valueLE->setEnabled(custom || textwidth);
		unitLC->setEnabled(custom);
		valueLA->setEnabled(custom || textwidth);
	}
	if (textwidth)
		// opt-out validator
		valueLE->setValidator(nullptr);
	else
		valueLE->setValidator(unsignedLengthValidator(valueLE));

	if (!InsetParamsWidget::checkWidgets())
		return false;
	return (!custom && !textwidth)
		|| !valueLE->text().isEmpty();
}

} // namespace frontend
} // namespace lyx


#include "moc_GuiPrintNomencl.cpp"
