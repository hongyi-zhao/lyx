// -*- C++ -*-
/**
 * \file convert.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author André Pönitz
 * \author Lars Gullik Bjønnes
 *
 * Full author contact details are available in file CREDITS.
 *
 * A collection of string helper functions that works with string.
 * Some of these would certainly benefit from a rewrite/optimization.
 */

#ifndef CONVERT_H
#define CONVERT_H

#include "support/docstring.h"

namespace lyx {

template <class Target, class Source>
Target convert(Source const & arg);


template<> std::string convert<std::string>(bool const & b);
template<> std::string convert<std::string>(char const & c);
template<> std::string convert<std::string>(short unsigned int const & sui);
template<> std::string convert<std::string>(int const & i);
template<> docstring convert<docstring>(int const & i);
template<> std::string convert<std::string>(unsigned int const & ui);
template<> docstring convert<docstring>(unsigned int const & ui);
template<> std::string convert<std::string>(unsigned long const & ul);
template<> docstring convert<docstring>(unsigned long const & ul);
#ifdef HAVE_LONG_LONG_INT
template<> std::string convert<std::string>(unsigned long long const & ull);
template<> docstring convert<docstring>(unsigned long long const & ull);
template<> std::string convert<std::string>(long long const & ll);
template<> docstring convert<docstring>(long long const & ll);
template<> unsigned long long convert<unsigned long long>(std::string const & s);
// not presently needed
// template<> long long convert<long long>(std::string const & s);
#endif
template<> std::string convert<std::string>(long const & l);
template<> docstring convert<docstring>(long const & l);
template<> std::string convert<std::string>(float const & f);
template<> std::string convert<std::string>(double const & d);
template<> int convert<int>(std::string const & s);
template<> int convert<int>(docstring const & s);
template<> unsigned int convert<unsigned int>(std::string const & s);
template<> unsigned long convert<unsigned long>(std::string const & s);
template<> double convert<double>(std::string const & s);

template <class Target>
Target convert(char const * arg);

template<> int convert<int>(char const * cptr);
template<> double convert<double>(char const * cptr);

int convert(std::string const & s, int base);
} // namespace lyx

#endif
