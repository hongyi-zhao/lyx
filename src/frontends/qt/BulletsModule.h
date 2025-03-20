// -*- C++ -*-
/**
 * \file BulletsModule.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Edwin Leuven
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef BULLETSMODULE_H
#define BULLETSMODULE_H

#include "ui_BulletsUi.h"
#include "Bullet.h"

#include <QWidget>


namespace lyx {

namespace frontend {

class BulletsModule : public QWidget, public Ui::BulletsUi
{
	Q_OBJECT

public:
	///
	BulletsModule(QWidget * parent = nullptr);

	/// set a bullet
	void setBullet(int level, Bullet const & bullet);
	/// get bullet setting
	Bullet const & bullet(int level) const;
	/// update 1st level
	void init();

Q_SIGNALS:
	void changed();

protected Q_SLOTS:
	void on_bulletsizeCO_activated(int level);
	void on_customCB_clicked(bool);
	void on_customLE_textEdited(const QString &);
	void bulletSelected(QListWidgetItem *);
	void showLevel(int);
	void setCurrentPane(int);

private:
	void selectItem(int font, int character, bool select);
	void setupPanel(QListWidget * lw, QString const & panelname);
	void fillPanel(int const font);
	std::string getFolder(int const font);
	QListWidget * getListWidget(int const font);

	/// store results
	Bullet bullets_[4];
	int current_font_;
	int current_char_;

	/// List widgets
	QListWidget * standardLW;
	QListWidget * mathsLW;
	QListWidget * dings1LW;
	QListWidget * dings2LW;
	QListWidget * dings3LW;
	QListWidget * dings4LW;
};

} // namespace frontend

} // namespace lyx

#endif // BULLETSMODULE_H
