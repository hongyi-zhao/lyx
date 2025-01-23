// -*- C++ -*-
/**
 * \file InputMethod.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Koji Yokota
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef INPUTMETHOD_H
#define INPUTMETHOD_H

#include "support/docstring.h"
#include "support/types.h"

namespace lyx {

class ParagraphMetrics;

namespace frontend {

class InputMethod {
public:

#ifdef Q_DEBUG
	/// Input method hint (sync with Qt::ImHint)
	enum Hint {
		ImhNone                     = 0x0,
		ImhHiddenText               = 0x1,
		ImhSensitiveData            = 0x2,
		ImhNoAutoUppercase          = 0x4,
		ImhPreferNumbers            = 0x8,
		ImhPreferUppercase          = 0x10,
		ImhPreferLowercase          = 0x20,
		ImhNoPredictiveText         = 0x40,
		ImhDate                     = 0x80,
		ImhTime                     = 0x100,
		ImhPreferLatin              = 0x200,
		ImhMultiLine                = 0x400,
		ImhNoEditMenu               = 0x800,
		ImhNoTextHandles            = 0x1000,
		ImhDigitsOnly               = 0x10000,
		ImhFormattedNumbersOnly     = 0x20000,
		ImhUppercaseOnly            = 0x40000,
		ImhLowercaseOnly            = 0x80000,
		ImhDialableCharactersOnly   = 0x100000,
		ImhEmailCharactersOnly      = 0x200000,
		ImhUrlCharactersOnly        = 0x400000,
		ImhLatinOnly                = 0x800000,
		ImhExclusiveInputMask       = 0xffff0000
	};
#endif //Q_DEBUG
	///
	virtual ~InputMethod(){}

	/// Returns preedit string
	virtual docstring & preeditString() const = 0;

	/// Starting position of preedit snippets
	virtual pos_type & segmentStart(size_type seg_id) const = 0;

	/// Lengths of each preedit snippet
	virtual size_type & segmentLength(size_type seg_id) const = 0;

	virtual size_type segmentSize() const = 0;

	/// the index in char_formats_ vector for given pos in preedit string
	virtual pos_type charFormatIndex(pos_type pos) const = 0;

	///
	virtual void setParagraphMetrics(ParagraphMetrics &) = 0;

	///
	virtual int horizontalAdvance(docstring const & s,
	                              pos_type const char_format_index) = 0;
	///
	virtual bool canWrapAnywhere(pos_type const char_format_index) = 0;
	///
	virtual void toggleInputMethodAcceptance() = 0;
#ifdef Q_DEBUG
	///
	virtual void setHint(Hint) = 0;
#endif // Q_DEBUG
};

} // namespace frontend
} // namespace lyx

#endif // INPUTMETHOD_H
