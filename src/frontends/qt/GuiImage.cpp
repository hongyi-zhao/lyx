/**
 * \file GuiImage.cpp
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * \author Angus Leeming
 * \author John Levon
 *
 * Full author contact details are available in file CREDITS.
 */

#include <config.h>
#include <math.h> /* ceil */

#include "GuiImage.h"
#include "qt_helpers.h"

#include "graphics/GraphicsParams.h"

#include "support/debug.h"
#include "support/FileName.h"
#include "support/lstrings.h"       // ascii_lowercase

using namespace std;
using namespace lyx::support;

namespace lyx {
namespace graphics {

/// Implement factory method defined in GraphicsImage.h
Image * newImage()
{
	return new GuiImage;
}


GuiImage::GuiImage() : is_transformed_(false)
{}


GuiImage::GuiImage(GuiImage const & other)
	: Image(other), original_(other.original_),
	transformed_(other.transformed_), is_transformed_(other.is_transformed_),
	fname_(other.fname_)
{}


Image * GuiImage::clone() const
{
	return new GuiImage(*this);
}


QImage const & GuiImage::image() const
{
	return is_transformed_ ? transformed_ : original_;
}


unsigned int GuiImage::width() const
{
	return static_cast<unsigned int>(ceil(is_transformed_ ?
		(transformed_.width() / transformed_.devicePixelRatio()) :
		(original_.width() / original_.devicePixelRatio())));
}


unsigned int GuiImage::height() const
{
	return static_cast<unsigned int>(ceil(is_transformed_ ?
		(transformed_.height() / transformed_.devicePixelRatio()) :
		(original_.height() / original_.devicePixelRatio())));
}


bool GuiImage::load(FileName const & filename)
{
	if (!original_.isNull()) {
		LYXERR(Debug::GRAPHICS, "Image is loaded already!");
		return false;
	}
	fname_ = toqstr(filename.absFileName());
	return load();
}


bool GuiImage::load()
{
	if (!original_.load(fname_)) {
		LYXERR(Debug::GRAPHICS, "Unable to open image");
		return false;
	}
	return true;
}


bool GuiImage::setPixmap(Params const & params)
{
	if (!params.display)
		return false;

	if (original_.isNull()) {
		if (!load())
			return false;
	}

	original_.setDevicePixelRatio(params.pixel_ratio);

	is_transformed_ = clip(params);
	is_transformed_ |= rotate(params);
	is_transformed_ |= scale(params);

	// Clear the pixmap to save some memory.
	if (is_transformed_)
		original_ = QImage();
	else
		transformed_ = QImage();

	return true;
}


bool GuiImage::clip(Params const & params)
{
	if (params.bb.empty())
		// No clipping is necessary.
		return false;

	double const pixelRatio = is_transformed_ ? transformed_.devicePixelRatio() : original_.devicePixelRatio();
	int const new_width  = static_cast<int>((params.bb.xr.inBP() - params.bb.xl.inBP()) * pixelRatio);
	int const new_height = static_cast<int>((params.bb.yt.inBP() - params.bb.yb.inBP()) * pixelRatio);

	QImage const & image = is_transformed_ ? transformed_ : original_;

	// No need to check if the width, height are > 0 because the
	// Bounding Box would be empty() in this case.
	if (new_width > image.width() || new_height > image.height()) {
		// Bounds are invalid.
		return false;
	}

	if (new_width == image.width() && new_height == image.height())
		return false;

	int const xoffset_l = params.bb.xl.inBP();
	int const yoffset_t = (image.height() > params.bb.yt.inBP())
		? image.height() - params.bb.yt.inBP() : 0;

	transformed_ = image.copy(xoffset_l, yoffset_t, new_width, new_height);
	return true;
}


bool GuiImage::rotate(Params const & params)
{
	if (params.angle == 0)
		return false;

	QImage const & image = is_transformed_ ? transformed_ : original_;
	QTransform m;
	m.rotate(- params.angle);
	transformed_ = image.transformed(m);
	return true;
}


bool GuiImage::scale(Params const & params)
{
	QImage const & image = is_transformed_ ? transformed_ : original_;

	if (params.scale == 100)
		return false;

	double const pixelRatio = is_transformed_ ? transformed_.devicePixelRatio() : original_.devicePixelRatio();
	qreal const scale = qreal(params.scale) / 100.0 * pixelRatio;

	QTransform m;
	m.scale(scale, scale);
	// Bilinear filtering is used to scale graphics preview
	transformed_ = image.transformed(m, Qt::SmoothTransformation);
	return true;
}

} // namespace graphics
} // namespace lyx
