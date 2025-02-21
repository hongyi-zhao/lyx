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

#include "CategorizedCombo.h"
#include "BufferParams.h"

#include <QComboBox>


namespace lyx {
namespace frontend {

class CCItemDelegate;

/**
 * A combo box with categorization
 */
class ColorsCombo : public CategorizedCombo
{
	Q_OBJECT
public:
	ColorsCombo(QWidget * parent);
	~ColorsCombo();

	/// Update combobox.
	void updateCombo();
	/// Add item to combo with optional color icon
	void addItemSort(QString const & item, QString const & guiname,
			 QString const & category, QString color = QString());
	/// Set BufferParams to access custom colors
	void setCustomColors(std::map<std::string, std::string> const custom_colors);
	/// Add "ignore" color entry?
	void hasIgnore(bool const b) { has_ignore_ = b; }
	/// Add "inherit" color entry?
	void hasInherit(bool const b) { has_inherit_ = b; }

private:
	///
	void setColorIcon(int const i, QString const color);
	///
	void fillComboColor();
	///
	std::map<std::string, std::string> custom_colors_;
	///
	bool has_ignore_;
	///
	bool has_inherit_;
};


} // namespace frontend
} // namespace lyx

#endif // LYX_CATEGORIZEDCOMBO_H
