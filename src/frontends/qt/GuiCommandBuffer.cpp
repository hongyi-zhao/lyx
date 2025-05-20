/**
 * \file GuiCommandBuffer.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars
 * \author Asger and Jürgen
 * \author John Levon
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "GuiCommandBuffer.h"

#include "GuiApplication.h"
#include "GuiCommandEdit.h"
#include "GuiView.h"
#include "qt_helpers.h"

#include "Cursor.h"
#include "LyX.h"
#include "LyXAction.h"
#include "FuncRequest.h"
#include "Session.h"

#include "support/lstrings.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListWidget>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

using namespace std;
using namespace lyx::support;

namespace lyx {
namespace frontend {

namespace {

// Simple wrapper around the LastCommand session stuff.

// The whole last commands list
vector<string> const & history() { return theSession().lastCommands().getcommands(); }

// add command to the list (and remove duplicates)
void addHistory(string const & cmd) { theSession().lastCommands().add(cmd); }


class QTempListBox : public QListWidget {
public:
	QTempListBox() {
		//setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setWindowModality(Qt::WindowModal);
		setWindowFlags(Qt::Popup);
		setAttribute(Qt::WA_DeleteOnClose);
	}
protected:
	bool event(QEvent * ev) override {
		if (ev->type() == QEvent::MouseButtonPress) {
			QMouseEvent * me = static_cast<QMouseEvent *>(ev);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
			if (me->position().x() < 0 || me->position().y() < 0
			    || me->position().x() > width() || me->position().y() > height())
#else
			if (me->x() < 0 || me->y() < 0
			    || me->x() > width() || me->y() > height())
#endif
				hide();
			return true;
		}
		return QListWidget::event(ev);
	}

	void keyPressEvent(QKeyEvent * ev) override {
		if (ev->key() == Qt::Key_Escape) {
			hide();
			return;
		} else if (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Space) {
			// emit signal
			itemClicked(currentItem());
		} else
			QListWidget::keyPressEvent(ev);
	}
};

} // namespace


GuiCommandBuffer::GuiCommandBuffer(GuiView * view)
	: view_(view)
{
	QPixmap qpup = getPixmap("images/", "up", "svgz,png");
	QPixmap qpdown = getPixmap("images/", "down", "svgz,png");

	QVBoxLayout * top = new QVBoxLayout(this);
	QHBoxLayout * layout = new QHBoxLayout(0);

	edit_ = new GuiCommandEdit(this);
	edit_->setMinimumSize(edit_->sizeHint());
	edit_->setFocusPolicy(Qt::ClickFocus);

	int height = max(24, 2 * (edit_->sizeHint().height() / 2));
	QSize button_size = QSize(height, height);
	QSize icon_size = button_size - QSize(4, 4);

	upPB = new QPushButton(qpup, "", this);
	upPB->setToolTip(qt_("List of previous commands"));
	upPB->setMaximumSize(button_size);
	upPB->setIconSize(icon_size);
	downPB = new QPushButton(qpdown, "", this);
	downPB->setToolTip(qt_("Next command"));
	downPB->setMaximumSize(button_size);
	downPB->setIconSize(icon_size);
	downPB->setEnabled(false);
	connect(downPB, SIGNAL(clicked()), this, SLOT(down()));
	connect(upPB, SIGNAL(pressed()), this, SLOT(listHistoryUp()));

	connect(edit_, SIGNAL(returnPressed()), this, SLOT(dispatch()));
	connect(edit_, SIGNAL(tabPressed()), this, SLOT(complete()));
	connect(edit_, SIGNAL(upPressed()), this, SLOT(up()));
	connect(edit_, SIGNAL(downPressed()), this, SLOT(down()));
	connect(edit_, SIGNAL(escapePressed()), this, SLOT(hideParent()));

	layout->addWidget(upPB, 0);
	layout->addWidget(downPB, 0);
	layout->addWidget(edit_, 10);
	layout->setContentsMargins(0, 0, 0, 0);
	top->addLayout(layout);
	top->setContentsMargins(0, 0, 0, 0);
	setFocusProxy(edit_);

	upPB->setEnabled(!history().empty());

	history_pos_ = history().end();
}


void GuiCommandBuffer::dispatch()
{
	std::string const cmd = fromqstr(edit_->text());
	DispatchResult const & dr = dispatch(cmd);
	if (!dr.error()) {
		view_->setFocus();
		edit_->setText(QString());
		edit_->clearFocus();
		// If the toolbar was "auto", it is not needed anymore
		view_->resetCommandExecute();
	}
}


void GuiCommandBuffer::listHistoryUp()
{
	if (history().size() == 1) {
		edit_->setText(toqstr(history().back()));
		upPB->setEnabled(false);
		return;
	}
	QPoint const & pos = upPB->mapToGlobal(QPoint(0, 0));
	showList(history(), pos, true);
}


void GuiCommandBuffer::complete()
{
	string const input = fromqstr(edit_->text());
	string new_input;
	vector<string> const & comp = completions(input, new_input);

	if (comp.empty() || comp.size() == 1) {
		if (new_input != input)
			edit_->setText(toqstr(new_input));
		// If there is only one match, indicate this by adding a space
		if (comp.size() == 1)
			edit_->setText(edit_->text() + " ");
		return;
	}

	edit_->setText(toqstr(new_input));
	QPoint const & pos = edit_->mapToGlobal(QPoint(0, 0));
	showList(comp, pos);
}

void GuiCommandBuffer::showList(vector<string> const & list,
	QPoint const & pos, bool reversed) const
{
	QTempListBox * listBox = new QTempListBox;

	// For some reason the scrollview's contents are larger
	// than the number of actual items...
	for (auto const & item : list) {
		if (reversed)
			listBox->insertItem(0, toqstr(item));
		else
			listBox->addItem(toqstr(item));
	}
	// Select the first item
	listBox->setCurrentItem(listBox->item(0));

	listBox->resize(listBox->sizeHint());

	int const y = max(0, pos.y() - listBox->height());
	listBox->move(pos.x(), y);

	connect(listBox, SIGNAL(itemClicked(QListWidgetItem *)),
		this, SLOT(itemSelected(QListWidgetItem *)));
	connect(listBox, SIGNAL(itemActivated(QListWidgetItem *)),
		this, SLOT(itemSelected(QListWidgetItem *)));

	listBox->show();
	listBox->setFocus();
}


void GuiCommandBuffer::itemSelected(QListWidgetItem * item)
{
	QWidget const * widget = static_cast<QWidget const *>(sender());
	const_cast<QWidget *>(widget)->hide();
	edit_->setText(item->text()+ ' ');
	edit_->activateWindow();
	edit_->setFocus();
}


void GuiCommandBuffer::up()
{
	string const h = historyUp();

	if (!h.empty())
		edit_->setText(toqstr(h));

	upPB->setEnabled(history_pos_ != history().begin());
	downPB->setEnabled(history_pos_ != history().end());
}


void GuiCommandBuffer::down()
{
	string const h = historyDown();

	if (!h.empty())
		edit_->setText(toqstr(h));

	downPB->setEnabled(!history().empty()
			   && history_pos_ != history().end() - 1);
	upPB->setEnabled(history_pos_ != history().begin());
}


void GuiCommandBuffer::hideParent()
{
	view_->setFocus();
	view_->resetCommandExecute();
	edit_->setText(QString());
	edit_->clearFocus();
}


string const GuiCommandBuffer::historyUp()
{
	if (history_pos_ == history().begin())
		return string();

	return *(--history_pos_);
}


string const GuiCommandBuffer::historyDown()
{
	if (history_pos_ == history().end())
		return string();
	if (history_pos_ + 1 == history().end())
		return string();

	return *(++history_pos_);
}


vector<string> const
GuiCommandBuffer::completions(string const & prefix, string & new_prefix)
{
	vector<string> comp;
	for (auto const & act : lyxaction) {
		if (prefixIs(act.first, prefix))
			comp.push_back(act.first);
	}
	// now add all the other items that contain the prefix
	for (auto const & act : lyxaction) {
		if (!prefixIs(act.first, prefix) && contains(act.first, prefix))
			comp.push_back(act.first);
	}

	if (comp.empty()) {
		new_prefix = prefix;
		return comp;
	}

	if (comp.size() == 1) {
		new_prefix = comp[0];
		return comp;
	}

	// find maximal available prefix
	string const tmp = comp[0];
	string test = prefix;
	if (tmp.length() > test.length())
		test += tmp[test.length()];
	while (test.length() < tmp.length()) {
		vector<string> vtmp;
		for (auto const & cmd : comp) {
			if (prefixIs(cmd, test))
				vtmp.push_back(cmd);
		}
		if (vtmp.size() != comp.size()) {
			test.erase(test.length() - 1);
			break;
		}
		test += tmp[test.length()];
	}

	new_prefix = test;
	return comp;
}


DispatchResult const & GuiCommandBuffer::dispatch(string const & str)
{
	if (str.empty()) {
		static DispatchResult empty_dr;
		return empty_dr;
	}

	addHistory(trim(str));
	history_pos_ = history().end();
	upPB->setEnabled(history_pos_ != history().begin());
	downPB->setEnabled(history_pos_ != history().end());
	FuncRequest func = lyxaction.lookupFunc(str);
	func.setOrigin(FuncRequest::COMMANDBUFFER);
	return lyx::dispatch(func);
}

} // namespace frontend
} // namespace lyx

#include "moc_GuiCommandBuffer.cpp"
