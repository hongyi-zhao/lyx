// -*- C++ -*-
/**
 * \file BufferView.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Alfredo Braustein
 * \author Lars Gullik Bjønnes
 * \author John Levon
 * \author Jürgen Vigna
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef BUFFER_VIEW_H
#define BUFFER_VIEW_H

#include "CoordCache.h"
#include "DocumentClassPtr.h"
#include "TexRow.h"
#include "update_flags.h"

#include "support/types.h"

namespace lyx {

namespace support { class FileName; }

namespace frontend { struct CaretGeometry; }
namespace frontend { class Painter; }
namespace frontend { class GuiBufferViewDelegate; }
namespace frontend { class InputMethod; }

class Buffer;
class Change;
class Cursor;
class CursorSlice;
class Dimension;
class DispatchResult;
class DocIterator;
class FuncRequest;
class FuncStatus;
class Intl;
class Inset;
class InsetMathNest;
class Length;
class MathData;
class MathRow;
class Point;
class Text;
class TextMetrics;

enum CursorStatus {
	CUR_INSIDE,
	CUR_ABOVE,
	CUR_BELOW
};

/// How to show cursor
enum ScrollType {
	// Make sure row if visible (do nothing if it is visible already)
	SCROLL_VISIBLE,
	// Cursor on top of screen
	SCROLL_TOP,
	// Cursor at center of screen
	SCROLL_CENTER,
	// Cursor at bottom of the screen
	SCROLL_BOTTOM,
	// Alternate between center, top, bottom, center, etc.
	SCROLL_TOGGLE
};

/// Scrollbar Parameters.
struct ScrollbarParameters
{
	// These parameters are normalized against the screen geometry and pixel
	// coordinates. Position 0 corresponds to the top the the screen.
	ScrollbarParameters()
		: min(0), max(0), single_step(1), page_step(1)
	{}
	/// Minimum scrollbar position in pixels.
	int min;
	/// Maximum scrollbar position in pixels.
	int max;
	/// Line-scroll amount in pixels.
	int single_step;
	/// Page-scroll amount in pixels.
	int page_step;
};

/// Screen view of a Buffer.
/**
 * A BufferView encapsulates a view onto a particular
 * buffer, and allows access to operate upon it. A view
 * is a sliding window of the entire document rendering.
 * It is the official interface between the LyX core and
 * the frontend WorkArea.
 *
 * \sa WorkArea
 * \sa Buffer
 * \sa CoordCache
 */
class BufferView {
public:
	///
	explicit BufferView(Buffer & buffer);
	///
	~BufferView();

	/// return the buffer being viewed.
	Buffer & buffer();
	Buffer const & buffer() const;

	/// Copy cursor and vertical offset information from \c bv
	void copySettingsFrom(BufferView const & bv);

	///
	void setFullScreen(bool full_screen) { full_screen_ = full_screen; }

	/// default value for the margins
	int defaultMargin() const;
	/// right margin
	int rightMargin() const;
	/// left margin
	int leftMargin() const;
	/// top margin
	int topMargin() const;
	/// bottom margin
	int bottomMargin() const;

	docstring const & searchRequestCache() const;
	void setSearchRequestCache(docstring const & text);

	/// return the on-screen size of this length
	/*
	 *  This is a wrapper around Length::inPixels that uses the
	 *  bufferview width as width and the EM value of the default
	 *  document font.
	 */
	int inPixels(Length const & len) const;

	/** Return the number of pixels equivalent to \c pix pixels at
	 * 100dpi and 100% zoom.
	 */
	int zoomedPixels(int pix) const;

	/// \return true if the BufferView is at the top of the document.
	bool isTopScreen() const;

	/// \return true if the BufferView is at the bottom of the document.
	bool isBottomScreen() const;

	/// Add \p flags to current update flags and trigger an update.
	/* If this method is invoked several times before the update
	 * actually takes place, the effect is cumulative.
	 * \c Update::FitCursor means first to do a FitCursor, and to
	 * force an update if screen position changes.
	 * \c Update::Force means to force an update in any case.
	 */
	void processUpdateFlags(Update::flags flags);

	/// return true if one shall move the screen to fit the cursor.
	/// Only to be called with good y coordinates (after a bv::metrics)
	bool needsFitCursor() const;

	// Returns the amount of horizontal scrolling applied to the
	// top-level row where the cursor lies
	int horizScrollOffset() const;
	// Returns the amount of horizontal scrolling applied to the
	// row of text starting at (pit, pos)
	int horizScrollOffset(Text const * text,
	                      pit_type pit, pos_type pos) const;

	/// reset the scrollbar parameters to reflect current view position.
	void updateScrollbarParameters();
	/// return the Scrollbar Parameters.
	ScrollbarParameters const & scrollbarParameters() const;
	/// \return Tool tip for the given position.
	docstring toolTip(int x, int y) const;
	/// \return the context menu for the given position.
	std::string contextMenu(int x, int y) const;
	/// \return the math inset with a context menu for the given position
	Inset const * mathContextMenu(InsetMathNest const * inset,
		CoordCache::Insets const & inset_cache, int x, int y) const;
	/// \return the clickable math inset for the given position
	Inset const * clickableMathInset(InsetMathNest const * inset,
		CoordCache::Insets const & inset_cache, int x, int y) const;

	/// Save the current position as bookmark.
	/// if idx == 0, save to temp_bookmark
	void saveBookmark(unsigned int idx);
	/// goto a specified position, try top_id first, and then bottom_pit.
	/// \return true if success
	bool moveToPosition(
		pit_type bottom_pit, ///< Paragraph pit, used when par_id is zero or invalid.
		pos_type bottom_pos, ///< Paragraph pit, used when par_id is zero or invalid.
		int top_id, ///< Paragraph ID, \sa Paragraph
		pos_type top_pos ///< Position in the \c Paragraph
		);
	/// return the current change at the cursor.
	Change const getCurrentChange() const;

	/// move cursor to the named label.
	void gotoLabel(docstring const & label);

	/// set the cursor based on the given TeX source row.
	bool setCursorFromRow(int row);
	/// set the cursor based on the given start and end TextEntries.
	bool setCursorFromEntries(TexRow::TextEntry start, TexRow::TextEntry end);

	/// set cursor to the given inset. Return true if found.
	bool setCursorFromInset(Inset const *);
	/// Recenters the BufferView such that the passed cursor
	/// is in the center.
	void recenter();
	/// Ensure that the BufferView cursor is visible.
	/// This method will automatically scroll and update the BufferView
	/// (metrics+drawing) if needed.
	/// \param how: where the cursor should appear (visible, top, center...)
	void showCursor(ScrollType how = SCROLL_VISIBLE);

	/// Ensure the passed cursor \p dit is visible.
	/// This method will automatically scroll and update the BufferView
	/// (metrics+drawing) if needed.
	/// \param how: where the cursor should appear (visible, top, center...)
	void showCursor(DocIterator const & dit, ScrollType how);
	/// Scroll to the cursor.
	/// This only updates the anchor vertical position, but does not
	/// recompute metrics nor trigger a screen refresh.
	/// \param how: where the cursor should appear (visible, top, center...)
	bool scrollToCursor(DocIterator const & dit, ScrollType how);
	/// scroll the view by the given number of pixels. This only
	/// updates the anchor vertical position, but does not recompute
	/// metrics nor trigger a screen refresh.
	void scroll(int pixels);
	/// Scroll the view by a number of pixels.
	void scrollDocView(int pixels);
	/// Set the cursor position based on the scrollbar one.
	void setCursorFromScrollbar();

	/// return the pixel width of the document view.
	int workWidth() const;
	/// return the pixel height of the document view.
	int workHeight() const;

	/// return the inline completion postfix.
	docstring const & inlineCompletion() const;
	/// return the number of unique characters in the inline completion.
	size_t inlineCompletionUniqueChars() const;
	/// return the position in the buffer of the inline completion postfix.
	DocIterator const & inlineCompletionPos() const;
	/// make sure inline completion position is OK
	void resetInlineCompletionPos();
	/// set the inline completion postfix and its position in the buffer.
	/// Updates the updateFlags in \c cur.
	void setInlineCompletion(Cursor const & cur, DocIterator const & pos,
		docstring const & completion, size_t uniqueChars = 0);

	void setInputMethod(frontend::InputMethod * im);
	frontend::InputMethod * inputMethod() const;

	/// translate and insert a character, using the correct keymap.
	void translateAndInsert(char_type c, Text * t, Cursor & cur);

	/// \return true if we've made a decision
	bool getStatus(FuncRequest const & cmd, FuncStatus & flag);
	/// execute the given function.
	void dispatch(FuncRequest const & cmd, DispatchResult & dr);

	/// request an X11 selection.
	/// \return the selected string.
	docstring requestSelection();
	/// clear the X11 selection.
	void clearSelection();

	/// resize the BufferView.
	/// \sa WorkArea
	void resize(int width, int height);

	/// dispatch method helper for \c WorkArea
	/// \sa WorkArea
	void mouseEventDispatch(FuncRequest const & ev);

	///
	CursorStatus cursorStatus(DocIterator const & dit) const;
	/// access to full cursor.
	Cursor & cursor();
	/// access to full cursor.
	Cursor const & cursor() const;
	/// sets cursor.
	/// This will also open all relevant collapsible insets.
	void setCursor(DocIterator const &);
	/// set the selection up to dit.
	void setCursorSelectionTo(DocIterator const & dit);
	/// Check deleteEmptyParagraphMechanism and update metrics if needed.
	/// \retval true if an update was needed.
	bool checkDepm(Cursor & cur, Cursor & old);
	/// sets cursor.
	/// This is used when handling LFUN_MOUSE_PRESS.
	bool mouseSetCursor(Cursor & cur, bool select = false);

	/// sets the selection.
	/* When \c backwards == false, set anchor
	 * to \c cur and cursor to \c cur + \c length. When \c
	 * backwards == true, set anchor to \c cur and cursor to \c
	 * cur + \c length.
	 */
	void putSelectionAt(DocIterator const & cur,
		int length, bool backwards);
	/// set a selection between \p from and \p to
	void setSelection(DocIterator const & from,
			 DocIterator const & to);

	/// selects the item at cursor if its paragraph is empty.
	bool selectIfEmpty(DocIterator & cur);

	/// Ditch all metrics information and rebuild it. Set the update
	/// flags and the draw strategy flags accordingly.
	void updateMetrics();

	// this is the "nodraw" drawing stage: only set the positions of the
	// insets in metrics cache.
	void updatePosCache();

	///
	TextMetrics const & textMetrics(Text const * t) const;
	TextMetrics & textMetrics(Text const * t);

	///
	CoordCache & coordCache();
	///
	CoordCache const & coordCache() const;

	///
	bool hasMathRow(MathData const * cell) const;
	///
	MathRow const & mathRow(MathData const * cell) const;
	///
	void setMathRow(MathData const * cell, MathRow const & mrow);

	///
	Point getPos(DocIterator const & dit) const;
	/// is there enough metrics information to compute iterator position?
	bool hasPosition(DocIterator const & dit) const;
	/// is the caret currently visible in the view
	bool caretInView() const;
	/// get the position and height of the caret
	void caretPosAndDim(Point & p, Dimension & dim) const;
	/// compute the shape of the caret with coordinate shift parameter
	void buildCaretGeometry(bool complet, Point shift = {});
	/// the shape of the caret
	frontend::CaretGeometry const & caretGeometry() const;

	/// Returns true when metrics have been computed at least once
	bool ready() const { return width_ > 0 && height_ > 0; }
	/// Returns true when the BufferView is not ready for drawing
	bool busy() const;
	///
	void draw(frontend::Painter & pain, bool paint_caret);

	/// get this view's keyboard map handler.
	Intl & getIntl();
	///
	Intl const & getIntl() const;

	//
	// Messages to the GUI
	//
	/// This signal is emitted when some message shows up.
	void message(docstring const & msg);

	/// This signal is emitted when some dialog needs to be shown.
	void showDialog(std::string const & name);

	/// This signal is emitted when some dialog needs to be shown with
	/// some data.
	void showDialog(std::string const & name, std::string const & data,
		Inset * inset = nullptr);

	/// This signal is emitted when some dialogs needs to be updated.
	void updateDialog(std::string const & name, std::string const & data);

	///
	void setGuiDelegate(frontend::GuiBufferViewDelegate *);

	///
	docstring contentsOfPlaintextFile(support::FileName const & f);
	// Insert plain text file (if filename is empty, prompt for one)
	void insertPlaintextFile(support::FileName const & f, bool asParagraph);
	///
	void insertLyXFile(support::FileName const & f, bool const ignorelang = false);
	/// save temporary bookmark for jump back navigation
	void bookmarkEditPosition();
	/// Find and return the inset associated with given dialog name.
	Inset * editedInset(std::string const & name) const;
	/// Associate an inset associated with given dialog name.
	void editInset(std::string const & name, Inset * inset);
	///
	void clearLastInset(Inset * inset) const;
	/// Is the mouse hovering a clickable inset or element?
	bool clickableInset() const;
	///
	void makeDocumentClass();
	/// Are we currently performing a selection with the mouse?
	bool mouseSelecting() const;

	/// Reference value for statistics (essentially subtract this from the actual value to see relative counts)
	/// (words/chars/chars no blanks)
	int stats_ref_value_w() const;
	int stats_ref_value_c() const;
	int stats_ref_value_nb() const;
	//signals need for update in gui
	bool stats_update_trigger();

	/// Inserted label from ref dialog
	std::string const & insertedLabel() const { return inserted_label_; }

private:
	/// noncopyable
	BufferView(BufferView const &);
	void operator=(BufferView const &);

	/// the position relative to (0, baseline) of outermost paragraph
	Point coordOffset(DocIterator const & dit) const;
	/// Update current paragraph metrics.
	/// \return true if no further update is needed.
	bool singleParUpdate();
	/** Helper for the public updateMetrics() and for processUpdateFlags().
	 * This does also set the anchor paragraph and its position correctly.
	 * \param force when true, get rid of all paragraph metrics and
	 *    rebuild them anew. Otherwise keep the paragraphs that are
	 *    still visible in work area and rebuild the missing ones.
	 * \return the correction needed (same sign as anchor vertical
	 * position change) when hitting top or bottom constraints.
	*/
	int updateMetrics(bool force);

	// Set the row on which the cursor lives.
	void setCurrentRowSlice(CursorSlice const & rowSlice);

	// Check whether the row where the cursor lives needs to be scrolled.
	// Update the drawing strategy if needed.
	void checkCursorScrollOffset();

	/// The minimal size of the document that is visible. Used
	/// when it is allowed to scroll below the document.
	int minVisiblePart();

	/// Search recursively for the innermost inset in text that covers (x, y) position.
	/// \retval 0 if no inset is found.
	Inset const * getCoveringInset(
		Text const & text, //< The Text where we start searching.
		int x, //< x-coordinate on screen
		int y  //< y-coordinate on screen
		) const;

	/// Update the hovering status of the insets. This is called when
	/// either the screen is updated or when the buffer has scolled.
	void updateHoveredInset() const;

	///
	void updateDocumentClass(DocumentClassConstPtr const & olddc);
	///
	int width_;
	///
	int height_;
	///
	bool full_screen_;
	///
	std::string inserted_label_;
	///
	Buffer & buffer_;

	struct Private;
	Private * const d;
};

/// some space for drawing the 'nested' markers (in pixel)
inline int nestMargin() { return 15; }

/// margin for changebar
inline int changebarMargin() { return 12; }

} // namespace lyx

#endif // BUFFERVIEW_H
