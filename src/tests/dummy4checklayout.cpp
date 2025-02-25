/**
 * \file dummy_impl.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Jean-Marc Lasgouttes
 *
 * Full author contact details are available in file CREDITS.
 */

/**
 * This file contains dummy implementation of some methods that are
 * needed by classes used by tex2lyx. This allows to reduce the number
 * of classes we have to link against.
*/

// {[(

#include <config.h>

#include "../tex2lyx/tex2lyx.h"
#include "LaTeXColors.h"
#include "LaTeXFeatures.h"
#include "LyXRC.h"
#include "output_xhtml.h"
#include "xml.h"

#include "support/Messages.h"

#include <iostream>

using namespace std;

namespace lyx {

// Make linker happy

LaTeXColors & theLaTeXColors()
{
	LaTeXColors * lc = new LaTeXColors;
	return * lc;
}

//
// Dummy translation support (needed at many places)
//

bool LaTeXColors::isLaTeXColor(string const & /* name */)
{
	return(false);
}

LaTeXColors::TexColorMap LaTeXColors::getLaTeXColors()
{
	static TexColorMap texcolormap_;
	return(texcolormap_);
}

LaTeXColor LaTeXColors::getLaTeXColor(string const & /* name */)
{
	return LaTeXColor();
}
} // namespace lyx
