// -*- C++ -*-
/**
 * \file ColorsCombo.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
  * \author Jürgen Spitzmüller
  *
 * Full author contact details are available in file CREDITS.
 */

#ifndef LYX_COLORSCOMBO_H
#define LYX_COLORSCOMBO_H

#include "support/qstring_helpers.h"

#include <QComboBox>
#include <QStandardItemModel>


namespace lyx {
namespace frontend {

class ColItemDelegate;

/**
 * A combo box with categorization
 */
class ColorsCombo : public QComboBox
{
	Q_OBJECT
public:
	ColorsCombo(QWidget * parent);
	~ColorsCombo();

	/// select an item in the combobox. Returns false if item does not exist
	bool set(QString const & cc, bool const report_missing = true);
	/// Reset the combobox filter.
	void resetFilter();
	/// Update combobox.
	void updateCombo();
	///
	QString getData(int row) const;
	/// Add "ignore" color entry?
	void hasIgnore(bool const b) { has_ignore_ = b; }
	/// Add "inherit" color entry?
	void hasInherit(bool const b) { has_inherit_ = b; }
	/// Add dedicated "none" color entry if default_value_ != "none"
	void hasNone(bool const b) { has_none_ = b; }
	/// Flag a color as default. This will also omit the "none" entry
	void setDefaultColor(std::string const & col);
	/// Set the value of the default entry. Preset is "none"
	void setDefaultValue(std::string const & val) { default_value_ = toqstr(val); }
	///
	bool isDefaultColor(QString const & guiname);
	///
	void showPopup() override;

	///
	bool eventFilter(QObject * o, QEvent * e) override;
	///
	QString const & filter() const;
	///
	QStandardItemModel * model();
	///
	void setLeftMargin(int const);

private Q_SLOTS:
	///
	void modelChanged();

private:
	///
	bool has_ignore_;
	///
	bool has_inherit_;
	///
	bool has_none_;
	///
	QString default_color_;
	///
	QString default_value_;
	///
	bool need_default_color_;
	///
	friend class ColItemDelegate;
	///
	struct Private;
	///
	Private * const d;
	///
	int lastCurrentIndex_;
	///
	int left_margin_ = -1;
	///
	QString last_item_;
};


} // namespace frontend
} // namespace lyx

#endif // LYX_CATEGORIZEDCOMBO_H
