// -*- C++ -*-
/**
 * \file mute_warning.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Lars Gullik Bjønnes
 * \author Georg Baum
 *
 * Full author contact details are available in file CREDITS.
 */

#ifndef LYX_MUTE_WARNING_H
#define LYX_MUTE_WARNING_H

#if defined(__GNUC__) && __GNUC__ > 7 && !defined(__clang__)
/* This macro can be used to stipulate that a given GCC warning is not
 * relevant in a given block.
 *
 * The -Wpragmas bit takes care of the case where -W<warn> is not implemented
 *
 * The idea for PRAGMA_IGNORE has been stolen from
 * https://stackoverflow.com/questions/45762357/how-to-concatenate-strings-in-the-arguments-of-pragma#comment124444258_45783809
 * The difficulty is to put the <warn> value inside nested quotes; it is done
 * using nested macros.
 */
#  define PRAGMA_IGNORE(x) PRAGMA_IGNORE_1(-W##x)
#  define PRAGMA_IGNORE_1(x) PRAGMA_IGNORE_2(#x)
#  define PRAGMA_IGNORE_2(x) PRAGMA_IGNORE_3(GCC diagnostic ignored x)
#  define PRAGMA_IGNORE_3(x) _Pragma(#x)
#  define LYX_BEGIN_MUTE_GCC_WARNING(warn)		\
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wpragmas\"") \
  PRAGMA_IGNORE(warn)
#  define LYX_END_MUTE_GCC_WARNING \
  _Pragma("GCC diagnostic pop")
#else
#  define LYX_BEGIN_MUTE_GCC_WARNING(warn)
#  define LYX_END_MUTE_GCC_WARNING
#endif


#endif
