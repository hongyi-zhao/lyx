// -*- C++ -*-
/**
 * \file types.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * Provide a set of typedefs for commonly used things like sizes and
 * indices wile trying to stay compatible with types used
 * by the standard containers.
 *
 * \author André Pönitz
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef LYX_TYPES_H
#define LYX_TYPES_H

#include <cstddef>

namespace lyx {

	/*!
	 * A type for positions used in paragraphs.
	 * Each position is either occupied by a single character or an inset.
	 * For insets, the placeholder META_INSET is stored in the paragraph
	 * text, and the actual insets are maintained separately.
	 */
	// FIXME: should be unsigned, but needs to be signed for a while to
	// hold the special value -1 that is used somewhere
	// Specifically, TexRow::getDocIteratorsFromEntries uses negative pos
	// the way Python does: counting from the end. So maybe this should just
	// be signed.
	// Note that the signed property is also used in loops counting to zero.
	typedef ptrdiff_t  pos_type;

	/*!
	 * A type for paragraph offsets.
	 * This is used to address paragraphs in ParagraphList, Text etc.
	 */
	// FIXME: should be unsigned as well.
	// however, simply changing it breaks a downward loop somewhere...
	typedef ptrdiff_t  pit_type;

	/// a type for the nesting depth of a paragraph
	typedef size_t     depth_type;

	/// type for cell indices in inset
	typedef size_t     idx_type;
	/// type for row indices
	typedef size_t     row_type;
	/// type for col indices
	typedef size_t     col_type;

	/// type for cells and math insets
	typedef void const * uid_type;

// set this to '0' if you want to have really safe types
#if 1

	/// a type for sizes
	typedef size_t     size_type;

#else

	// These structs wrap simple things to make them distinguishible
	// to the compiler.
	// It's a shame that different typedefs are not "really" different

	struct size_type {
		///
		typedef size_t base_type;
		///
		size_type(base_type t) { data_ = t; }
		///
		operator base_type() const { return data_; }
		///
	private:
		base_type data_;
	};

#endif

	/// a 'not found' value
	inline const size_type npos = static_cast<size_type>(-1);

	///
	enum word_location {
		/// the word around the cursor, only if the cursor is
		/// not at a boundary
		WHOLE_WORD_STRICT,
		// the word around the cursor
		WHOLE_WORD,
		/// the word beginning from the cursor position
		PARTIAL_WORD,
		/// the word around the cursor or before the cursor
		PREVIOUS_WORD,
		/// the next word (not yet used)
		NEXT_WORD
	};

	///
	enum PageSides {
		///
		OneSide,
		///
		TwoSides
	};

} // namespace lyx

#endif // LYX_TYPES_H
