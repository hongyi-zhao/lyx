// -*- C++ -*-
/**
 * \file docstring.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author Georg Baum
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef LYX_DOCSTRING_H
#define LYX_DOCSTRING_H

#ifdef USE_WCHAR_T

// Prefer this if possible because GNU libstdc++ has usable
// std::ctype<wchar_t> locale facets but maybe not
// std::ctype<std::uint32_t>.
namespace lyx { typedef wchar_t char_type; }

#else

#include <cstdint>

#if defined(_MSC_VER) && (_MSC_VER >= 1600)
namespace lyx { typedef uint32_t char_type; }
#include "support/numpunct_lyx_char_type.h" // implementation for our char_type needed
#else
namespace lyx { typedef std::uint32_t char_type; }
#endif

#endif

#include <string>

namespace lyx {

/**
 * String type for storing the main text in UCS4 encoding.
 * Use std::string only in cases 7-bit ASCII is to be manipulated
 * within the variable.
 */
typedef std::basic_string<char_type, std::char_traits<char_type>,
	std::allocator<char_type> > docstring;

/// Base class for UCS4 input streams
typedef std::basic_istream<char_type, std::char_traits<char_type> > idocstream;

/// Base class for UCS4 output streams
typedef std::basic_ostream<char_type, std::char_traits<char_type> > odocstream;

/// UCS4 output stringstream
typedef std::basic_ostringstream<char_type, std::char_traits<char_type>, std::allocator<char_type> > odocstringstream;

#if ! defined(USE_WCHAR_T)
extern odocstream & operator<<(odocstream &, char);
#endif

// defined in lstrings.cpp
docstring const & empty_docstring();
std::string const & empty_string();
// defined in docstring.cpp
bool operator==(docstring const &, char const *);

//trivstring signalizes thread-safety request; should not be needed for any
//C++11 conformant std::basic_string, e.g. GCC >= 5.
//Once properly tested we can ditch these two in favour of std::string/docstring.
typedef std::string trivstring;
typedef docstring trivdocstring;

} // namespace lyx

namespace lyx {

/// Creates a docstring from a C string of ASCII characters
docstring const from_ascii(char const *);

/// Creates a docstring from a std::string of ASCII characters
docstring const from_ascii(std::string const &);

/// Creates a std::string of ASCII characters from a docstring
std::string const to_ascii(docstring const &);

/// Creates a docstring from a UTF8 string. This should go eventually.
docstring const from_utf8(std::string const &);

/// Creates a UTF8 string from a docstring. This should go eventually.
std::string const to_utf8(docstring const &);

/// convert \p s from the encoding of the locale to ucs4.
docstring const from_local8bit(std::string const & s);

/**
 * Convert \p s from ucs4 to the encoding of the locale.
 * This may fail and throw an exception, the caller is expected to act
 * appropriately.
 */
std::string const to_local8bit(docstring const & s);

/// convert \p s from the encoding of the file system to ucs4.
docstring const from_filesystem8bit(std::string const & s);

/// convert \p s from ucs4 to the encoding of the file system.
std::string const to_filesystem8bit(docstring const & s);

/// convert \p s from ucs4 to the \p encoding.
std::string const to_iconv_encoding(docstring const & s,
				    std::string const & encoding);

/// convert \p s from \p encoding to ucs4.
docstring const from_iconv_encoding(std::string const & s,
				    std::string const & encoding);

/// normalize \p s to precomposed form c
docstring const normalize_c(docstring const & s);

/// Compare a docstring with a C string of ASCII characters
bool operator==(docstring const &, char const *);

/// Compare a C string of ASCII characters with a docstring
inline bool operator==(char const * l, docstring const & r) { return r == l; }

/// Compare a docstring with a C string of ASCII characters
inline bool operator!=(docstring const & l, char const * r) { return !(l == r); }

/// Compare a C string of ASCII characters with a docstring
inline bool operator!=(char const * l, docstring const & r) { return !(r == l); }

/// Concatenate a docstring and a C string of ASCII characters
docstring operator+(docstring const &, char const *);

/// Concatenate a C string of ASCII characters and a docstring
docstring operator+(char const *, docstring const &);

/// Concatenate a docstring and a single ASCII character
docstring operator+(docstring const & l, char r);

/// Concatenate a single ASCII character and a docstring
docstring operator+(char l, docstring const & r);

/// Append a C string of ASCII characters to a docstring
docstring & operator+=(docstring &, char const *);

/// Append a single ASCII character to a docstring
docstring & operator+=(docstring & l, char r);

} // namespace lyx

#endif
