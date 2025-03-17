/**
 * \file qt/ColorsCombo.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jürgen Spitzmüller
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>

#include "ColorsCombo.h"
#include "GuiView.h"

#include "support/debug.h"
#include "support/gettext.h"
#include "support/lassert.h"
#include "support/lstrings.h"
#include "support/qstring_helpers.h"

#include "qt_helpers.h"

#include <QAbstractTextDocumentLayout>
#include <QComboBox>
#include <QHeaderView>
#include <QItemDelegate>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTextFrame>

#include "GuiApplication.h"

#include <QPainter>
#include <QListView>

using namespace lyx::support;

namespace lyx {
namespace frontend {

class ColItemDelegate : public QItemDelegate {
public:
	///
	explicit ColItemDelegate(ColorsCombo * cc)
		: QItemDelegate(cc), cc_(cc)
	{}
	///
	void paint(QPainter * painter, QStyleOptionViewItem const & option,
		QModelIndex const & index) const override;
	///
	void drawDisplay(QPainter * painter, QStyleOptionViewItem const & opt,
		const QRect & /*rect*/, const QString & text ) const override;
	///
	QSize sizeHint(QStyleOptionViewItem const & opt,
		QModelIndex const & index) const override;

private:
	///
	void drawCategoryHeader(QPainter * painter, QStyleOptionViewItem const & opt,
		QString const & category) const;
	///
	QString underlineFilter(QString const & s) const;
	///
	ColorsCombo * cc_;
};


class CCFilterModel : public QSortFilterProxyModel {
public:
	///
	CCFilterModel(QObject * parent = nullptr)
		: QSortFilterProxyModel(parent)
	{}
};


/////////////////////////////////////////////////////////////////////
//
// ColorsCombo::Private
//
/////////////////////////////////////////////////////////////////////

struct ColorsCombo::Private
{
	Private(ColorsCombo * parent) : p(parent),
		// set the model with five columns
		// 1st: translated item names
		// 2nd: raw names
		// 3rd: category
		// 4th: availability (bool)
		model_(guiApp->currentView()->viewColorsModel()),
		filterModel_(new CCFilterModel(p)),
		lastSel_(-1),
		ColItemDelegate_(new ColItemDelegate(parent)),
		visibleCategories_(0), inShowPopup_(false)
	{
		filterModel_->setSourceModel(model_);
	}

	void resetFilter() { setFilter(QString()); }
	///
	void setFilter(QString const & s);
	///
	void countCategories();
	///
	void toggleRows(QString const data, bool const toggle);
	///
	ColorsCombo * p;

	/** the layout model:
	 * 1st column: translated GUI name,
	 * 2nd column: raw item name,
	 * 3rd column: category,
	 * 4th column: custom color?
	**/
	QStandardItemModel * model_;
	/// the proxy model filtering \c model_
	CCFilterModel * filterModel_;
	/// the (model-) index of the last successful selection
	int lastSel_;
	/// the character filter
	QString filter_;
	///
	ColItemDelegate * ColItemDelegate_;
	///
	unsigned visibleCategories_;
	///
	bool inShowPopup_;
};


static QString categoryCC(QAbstractItemModel const & model, int row)
{
	return model.data(model.index(row, 2), Qt::DisplayRole).toString();
}


static int headerHeightCC(QStyleOptionViewItem const & opt)
{
	return opt.fontMetrics.height();
}


void ColItemDelegate::paint(QPainter * painter, QStyleOptionViewItem const & option,
			   QModelIndex const & index) const
{
	QStyleOptionViewItem opt = option;

	// default background
	painter->fillRect(opt.rect, opt.palette.color(QPalette::Base));

	QString cat = categoryCC(*index.model(), index.row());

	// not the same as in the previous line?
	if (cc_->d->visibleCategories_ > 0
	    && (index.row() == 0 || cat != categoryCC(*index.model(), index.row() - 1))) {
		painter->save();

		// draw unselected background
		QStyle::State state = opt.state;
		opt.state = opt.state & ~QStyle::State_Selected;
		drawBackground(painter, opt, index);
		opt.state = state;

		// draw category header
		drawCategoryHeader(painter, opt,
			categoryCC(*index.model(), index.row()));

		// move rect down below header
		opt.rect.setTop(opt.rect.top() + headerHeightCC(opt));

		painter->restore();
	}

	QItemDelegate::paint(painter, opt, index);
}


void ColItemDelegate::drawDisplay(QPainter * painter, QStyleOptionViewItem const & opt,
				 const QRect & /*rect*/, const QString & text_in) const
{
	QString text(text_in);
	// Mark default color
	if (cc_->need_default_color_ && cc_->isDefaultColor(text))
		text = toqstr(bformat(_("%1$s (= Default[[color]])"), qstring_to_ucs4(text)));

	QString utext = underlineFilter(text);

	// Draw the rich text.
	painter->save();
	QColor col = opt.palette.text().color();
	if (opt.state & QStyle::State_Selected)
		col = opt.palette.highlightedText().color();
	QAbstractTextDocumentLayout::PaintContext context;
	context.palette.setColor(QPalette::Text, col);

	QTextDocument doc;
	doc.setDefaultFont(opt.font);
	doc.setHtml(utext);

	QTextFrameFormat fmt = doc.rootFrame()->frameFormat();
	fmt.setMargin(0);
	if (cc_->left_margin_ != -1)
		fmt.setLeftMargin(cc_->left_margin_);
		
	doc.rootFrame()->setFrameFormat(fmt);

	painter->translate(opt.rect.x() + 5,
		opt.rect.y() + (opt.rect.height() - opt.fontMetrics.height()) / 2);
	doc.documentLayout()->draw(painter, context);
	painter->restore();
}


QSize ColItemDelegate::sizeHint(QStyleOptionViewItem const & opt,
			       QModelIndex const & index) const
{
	QSize size = QItemDelegate::sizeHint(opt, index);

	/// QComboBox uses the first row height to estimate the
	/// complete popup height during QComboBox::showPopup().
	/// To avoid scrolling we have to sneak in space for the headers.
	/// So we tweak this value accordingly. It's not nice, but the
	/// only possible way it seems.
	// Add space for the category headers here
	QString cat = categoryCC(*index.model(), index.row());
	if (index.row() == 0 || cat != categoryCC(*index.model(), index.row() - 1)) {
		size.setHeight(size.height() + headerHeightCC(opt));
	}

	return size;
}


void ColItemDelegate::drawCategoryHeader(QPainter * painter, QStyleOptionViewItem const & opt,
					QString const & category) const
{
	// slightly blended color
	QColor lcol = opt.palette.text().color();
	lcol.setAlpha(127);
	painter->setPen(lcol);

	// set 80% scaled, bold font
	QFont font = opt.font;
	font.setBold(true);
	font.setWeight(QFont::Black);
	font.setPointSize(opt.font.pointSize() * 8 / 10);
	painter->setFont(font);

	// draw the centered text
	QFontMetrics fm(font);
	int w = fm.boundingRect(category).width();
	int x = opt.rect.x() + (opt.rect.width() - w) / 2;
	int y = opt.rect.y() + 3 * fm.ascent() / 2;
	int left = x;
	int right = x + w;
	painter->drawText(x, y, category);

	// the vertical position of the line: middle of lower case chars
	int ymid = y - 1 - fm.xHeight() / 2; // -1 for the baseline

	// draw the horizontal line
	if (!category.isEmpty()) {
		painter->drawLine(opt.rect.x(), ymid, left - 1, ymid);
		painter->drawLine(right + 1, ymid, opt.rect.right(), ymid);
	} else
		painter->drawLine(opt.rect.x(), ymid, opt.rect.right(), ymid);
}


QString ColItemDelegate::underlineFilter(QString const & s) const
{
	QString const & f = cc_->filter();
	if (f.isEmpty())
		return s;
	QString r(s);
	QRegularExpression pattern(charFilterRegExpC(f));
	r.replace(pattern, "<u><b>\\1</b></u>");
	return r;
}


void ColorsCombo::Private::setFilter(QString const & s)
{
	bool enabled = p->view()->updatesEnabled();
	p->view()->setUpdatesEnabled(false);

	// remember old selection
	int sel = p->currentIndex();
	if (sel != -1)
		lastSel_ = filterModel_->mapToSource(filterModel_->index(sel, 0)).row();

	filter_ = s;

	// toggle context-dependent entries
	toggleRows("ignore", p->has_ignore_);
	toggleRows("inherit", p->has_inherit_);
	toggleRows("none", p->has_none_ && p->default_color_.isEmpty());

#if QT_VERSION < QT_VERSION_CHECK(5, 12, 0)
	filterModel_->setFilterRegExp(charFilterRegExp(filter_));
#else
	filterModel_->setFilterRegularExpression(charFilterRegExp(filter_));
#endif
	countCategories();

	// restore old selection
	if (lastSel_ != -1) {
		QModelIndex i = filterModel_->mapFromSource(model_->index(lastSel_, 0));
		if (i.isValid())
			p->setCurrentIndex(i.row());
	}

	// Workaround to resize to content size
	// FIXME: There must be a better way. The QComboBox::AdjustToContents)
	//        does not help.
	if (p->view()->isVisible()) {
		// call QComboBox::showPopup. But set the inShowPopup_ flag to switch on
		// the hack in the item delegate to make space for the headers.
		// We do not call our implementation of showPopup because that
		// would reset the filter again. This is only needed if the user clicks
		// on the QComboBox.
		LASSERT(!inShowPopup_, /**/);
		inShowPopup_ = true;
		p->QComboBox::showPopup();
		inShowPopup_ = false;
	}

	p->view()->setUpdatesEnabled(enabled);
}


void ColorsCombo::Private::toggleRows(QString const data, bool const toggle)
{
	QList<QStandardItem *> cis = model_->findItems(data, Qt::MatchExactly, 1);
	if (cis.empty())
		return;

	QListView * view = qobject_cast<QListView *>(p->view());
	QStandardItem * item = cis.front();
	view->setRowHidden(item->row(), !toggle);
	if (toggle)
		item->setFlags(item->flags() & Qt::ItemIsEnabled);
	else
		item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
}


void ColorsCombo::Private::countCategories()
{
	int n = filterModel_->rowCount();
	visibleCategories_ = 0;
	if (n == 0)
		return;

	QString prevCat = model_->index(0, 2).data().toString();

	// count categories
	for (int i = 1; i < n; ++i) {
		QString cat = filterModel_->index(i, 2).data().toString();
		if (cat != prevCat)
			++visibleCategories_;
		prevCat = cat;
	}
}


/////////////////////////////////////////////////////////////////////
//
// ColorsCombo
//
/////////////////////////////////////////////////////////////////////


ColorsCombo::ColorsCombo(QWidget * parent)
	: QComboBox(parent),
	  has_ignore_(false), has_inherit_(false),
	  has_none_(false), default_value_("none"),
	  need_default_color_(false),
	  d(new Private(this))
{
	setLeftMargin(32);
	setToolTip(qt_("Type on the list to filter on color names."));

	setSizeAdjustPolicy(QComboBox::AdjustToContents);
	setMinimumWidth(sizeHint().width());
	setMaxVisibleItems(100);

	setModel(d->filterModel_);
	connect(guiApp->currentView(), SIGNAL(colorsModelChanged()),
		this, SLOT(modelChanged()));

	// for the filtering we have to intercept characters
	view()->installEventFilter(this);
	view()->setItemDelegateForColumn(0, d->ColItemDelegate_);

	updateCombo();
}


ColorsCombo::~ColorsCombo()
{
	delete d;
}


void ColorsCombo::showPopup()
{
	lastCurrentIndex_ = currentIndex();
	bool enabled = view()->updatesEnabled();
	view()->setUpdatesEnabled(false);

	d->resetFilter();

	// call QComboBox::showPopup. But set the inShowPopup_ flag to switch on
	// the hack in the item delegate to make space for the headers.
	LASSERT(!d->inShowPopup_, /**/);
	d->inShowPopup_ = true;
	QComboBox::showPopup();
	d->inShowPopup_ = false;

	view()->setUpdatesEnabled(enabled);
}


bool ColorsCombo::eventFilter(QObject * o, QEvent * e)
{
	if (e->type() != QEvent::KeyPress)
		return QComboBox::eventFilter(o, e);

	QKeyEvent * ke = static_cast<QKeyEvent*>(e);
	bool modified = (ke->modifiers() == Qt::ControlModifier)
		|| (ke->modifiers() == Qt::AltModifier)
		|| (ke->modifiers() == Qt::MetaModifier);

	switch (ke->key()) {
	case Qt::Key_Escape:
		if (!modified && !d->filter_.isEmpty()) {
			d->resetFilter();
			setCurrentIndex(lastCurrentIndex_);
			return true;
		}
		break;
	case Qt::Key_Backspace:
		if (!modified) {
			// cut off one character
			d->setFilter(d->filter_.left(d->filter_.length() - 1));
		}
		break;
	default:
		if (modified || ke->text().isEmpty())
			break;
		// find chars for the filter string
		QString s;
		for (int i = 0; i < ke->text().length(); ++i) {
			QChar c = ke->text()[i];
			if (c.isLetterOrNumber()
			    || c.isSymbol()
			    || c.isPunct()
			    || c.category() == QChar::Separator_Space) {
				s += c;
			}
		}
		if (!s.isEmpty()) {
			// append new chars to the filter string
			d->setFilter(d->filter_ + s);
			return true;
		}
		break;
	}

	return QComboBox::eventFilter(o, e);
}


bool ColorsCombo::set(QString const & item_in, bool const report_missing)
{
	d->resetFilter();

	int const curItem = currentIndex();
	QModelIndex const mindex =
		d->filterModel_->mapToSource(d->filterModel_->index(curItem, 1));
	QString const & currentItem = d->model_->itemFromIndex(mindex)->text();
	QString const item = (item_in == default_value_) ? "default" : item_in;
	last_item_ = item;
	if (item == currentItem) {
		LYXERR(Debug::GUI, "Already had " << item << " selected.");
		return true;
	}

	QList<QStandardItem *> r = d->model_->findItems(item, Qt::MatchExactly, 1);
	if (r.empty()) {
		if (report_missing)
			LYXERR0("Trying to select non existent entry " << item);
		return false;
	}

	setCurrentIndex(d->filterModel_->mapFromSource(r.first()->index()).row());
	return true;
}


QString ColorsCombo::getData(int row) const
{
	int srow = d->filterModel_->mapToSource(d->filterModel_->index(row, 1)).row();
	QString const result = d->model_->data(d->model_->index(srow, 1), Qt::DisplayRole).toString();
	return (result == "default") ? default_value_ : result;
}


void ColorsCombo::resetFilter()
{
	d->resetFilter();
}


void ColorsCombo::setLeftMargin(int const i)
{
	left_margin_ = i;
}


void ColorsCombo::setDefaultColor(std::string const & col)
{
	default_color_ = toqstr(col);
	need_default_color_ = !col.empty();
}


void ColorsCombo::updateCombo()
{
	d->countCategories();

	// needed to recalculate size hint
	hide();
	setMinimumWidth(sizeHint().width());
	show();
}


QString const & ColorsCombo::filter() const
{
	return d->filter_;
}


QStandardItemModel * ColorsCombo::model()
{
	return d->model_;
}


void ColorsCombo::modelChanged()
{
	// restore last setting
	set(last_item_, false);
}


bool ColorsCombo::isDefaultColor(QString const & guiname)
{
	if (default_color_.isEmpty())
		return false;
	QList<QStandardItem *> cis = d->model_->findItems(guiname, Qt::MatchExactly, 0);
	bool const res = d->model_->data(d->model_->index(cis.front()->row(), 1),
					 Qt::DisplayRole).toString() == default_color_;
	if (res)
		// no need to look further
		need_default_color_ = false;
	return res;
}


} // namespace frontend
} // namespace lyx


#include "moc_ColorsCombo.cpp"
