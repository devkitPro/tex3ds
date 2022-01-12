/*------------------------------------------------------------------------------
 * Copyright (c) 2022
 *     Michael Theall (mtheall)
 *
 * This file is part of tex3ds.
 *
 * tex3ds is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tex3ds is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tex3ds.  If not, see <http://www.gnu.org/licenses/>.
 *----------------------------------------------------------------------------*/
/** @file utility.cpp
 *  @brief Utility functions.
 */

#include "utility.h"

Magick::Image applyTrim (Magick::Image &img)
{
	Magick::Image copy = img;

	try
	{
		img.trim ();
		img.page (Magick::Geometry (img.columns (), img.rows ()));
		return img;
	}
	catch (...)
	{
		// image was solid
		return copy;
	}
}

void applyEdge (Magick::Image &img)
{
	Magick::Image edged (Magick::Geometry (img.columns () + 2, img.rows () + 2), transparent ());
	edged.fileName (img.fileName ());

	edged.composite (img, Magick::Geometry (0, 0, 1, 1), Magick::OverCompositeOp);

	Pixels cache (edged);
	PixelPacket p = cache.get (0, 0, edged.columns (), edged.rows ());

	for (unsigned x = 1; x < edged.columns () - 1; ++x)
	{
		p[x] = p[edged.columns () + x];
		p[(edged.rows () - 1) * edged.columns () + x] =
		    p[(edged.rows () - 2) * edged.columns () + x];
	}

	for (unsigned y = 0; y < edged.rows (); ++y)
	{
		p[y * edged.columns ()]           = p[y * edged.columns () + 1];
		p[(y + 1) * edged.columns () - 1] = p[(y + 1) * edged.columns () - 2];
	}

	cache.sync ();

	img = edged;
}
