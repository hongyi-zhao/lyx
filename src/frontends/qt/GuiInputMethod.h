// -*- C++ -*-
/**
 * \file GuiInputMethod.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef GUIINPUTMETHOD_H
#define GUIINPUTMETHOD_H

#include "BufferView.h"
#include "GuiWorkArea.h"
#include "ParagraphMetrics.h"

#include "frontends/InputMethod.h"
#include "support/docstring.h"

#include <QTextCharFormat>
#include <QInputMethodEvent>
#include <QObject>
#include <QLocale>
#include <QString>

namespace lyx {
namespace frontend {

class GuiInputMethod : public QObject, public InputMethod
{
	Q_OBJECT

public:
	struct PreeditSegment {
		pos_type        start_;
		size_type       length_;
		QTextCharFormat char_format_;

		bool operator<(const PreeditSegment & another) const {
			return start_ < another.start_;
		}
	};

	struct PreeditStyle {
		/// Vector of PreeditSegment having following properties:
		/// 1. starting position 2. length 3. character format
		std::vector<PreeditSegment> segments_;

		/// whether the caret should be visible or not at the virtual cursor
		/// position
		bool    caret_visible_;
		/// color of the caret
		QColor  caret_color_;

		QLocale lang_;
		QString ruby_;
	};

	struct PreeditRow {
		pos_type pos;
		pos_type index;
	};

	struct InputMethodState {
		bool      enabled_ = true;
		bool      preediting_ = false; // either in edit or completing mode
		bool      edit_mode_ = true;  // i.e. not completing mode
		QRectF    cursor_rect_;
		QRectF    anchor_rect_;
		docstring surrounding_text_;
		docstring text_before_;
		docstring text_after_;
	};

	explicit GuiInputMethod(GuiWorkArea *parent = nullptr);

	~GuiInputMethod();

	/// Returns preedit string
	docstring & preeditString() const override;

	/// Starting position of preedit segments
	pos_type & segmentStart(size_type seg_id) const override;

	/// Lengths of each preedit segment
	size_type & segmentLength(size_type seg_id) const override;

	size_type segmentSize() const override;

	/// Character format of the given index in the char_formats_ vector
	QTextCharFormat & charFormat(pos_type index) const;
	/// the index in char_formats_ vector for given pos in preedit string
	pos_type charFormatIndex(pos_type pos) const override;

	/// Sets pixel offsets of the caret from real cursor position
	std::array<int,2> setCaretOffset(pos_type caret_pos);
	/// Returns pixel offsets of the caret from real cursor position
	std::array<int,2> preeditCaretOffset() const;
	///
	void setParagraphMetrics(ParagraphMetrics &) override;
	/// Sets surrounding text of the cursor within the paragraph
	void setSurroundingText(const Cursor & cur);
	/// Sets the absolute cursor position in the document
	void setAbsolutePosition(Cursor & cur) const;
	/// Returns locale of system input method
	QLocale & locale() const;
	/// Whether the virtual caret in the preedit string is visible
	bool isCaretVisible() const;
	/// Horizontal width of a string when QCharFormat is as specified by the index
	int horizontalAdvance(docstring const &, pos_type const) override;
	/// Horizontal width of a string using surrounding font
	int horizontalAdvance(docstring const &);
	/// Whether the segment contains language that allows wrapping anywhere
	bool canWrapAnywhere(pos_type const) override;
Q_SIGNALS:
	void preeditProcessed(QInputMethodEvent* ev);
	void queryProcessed(QVariant response);
	void inputMethodStateChanged(Qt::InputMethodQueries);

public Q_SLOT:
	/// Process incoming preedit string
	void processPreedit(QInputMethodEvent* ev);
	/// Process incoming input method query
	void processQuery(Qt::InputMethodQuery query);
	/// Turn off IM in math mode and command phase and turn it on otherwise
	void toggleInputMethodAcceptance() override;
#ifdef Q_DEBUG
	///
	void setHint(InputMethod::Hint hint) override;
#endif // Q_DEBUG

private:
	/// Initialize cursor position
	pos_type initializePositions(Cursor * cur);
	/// Initialize the coordinates of cursor and anchor rectangles
	Point initializeCaretCoords(pos_type const cur_row_idx, bool const boundary);
	///
	void updateMetrics(Cursor * cur);
	///
	ParagraphMetrics * resetParagraphMetrics(Cursor * cur);

	///
	pos_type setCaretPos(size_type preedit_length);
	/// Aquire and set character style of each preedit segment from
	/// attributes of the incoming input method event
	void setPreeditStyle(const QList<QInputMethodEvent::Attribute> & attr);
	/// Sets TextFormat
	void setTextFormat(const QInputMethodEvent::Attribute & it,
	                   const QInputMethodEvent::Attribute * focus_style,
	                   pos_type & max_start);
	/// Set QTextCharFormat to fit the font used in the surrounding text
	void conformToSurroundingFont(QTextCharFormat & char_format);
	/// Returns index of the focused segment
	pos_type focusedSegmentIndex();
	/// x shift factor from the caret to the selection segment's head
	int shiftFromCaretToSegmentHead();
	///
	PreeditRow getCaretInfo(const bool real_boundary, const bool virtual_boundary);

	/// Returns enum Qt::InputMethodQuery constant from its value
	docstring inputMethodQueryFlagsAsString(unsigned long int query) const;

	struct Private;
	Private * const d;
};

} // namespace frontend
} // namespace lyx

Q_DECLARE_METATYPE(QTextCharFormat)

#endif // GUIINPUTMETHOD_H
