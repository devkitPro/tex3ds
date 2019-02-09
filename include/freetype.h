/*------------------------------------------------------------------------------
 * Copyright (c) 2019
 *     Michael Theall (mtheall)
 *     piepie62
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
/** @file freetype.h
 *  @brief FreeType wrappers
 */
#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <memory>
#include <string>

namespace freetype
{
class Library : public std::enable_shared_from_this<Library>
{
public:
	~Library ();

	static std::shared_ptr<Library> makeLibrary ();

	FT_Library library () const;

private:
	Library ();

	FT_Library m_library;
};

class Face
{
public:
	~Face ();

	static std::unique_ptr<Face>
	    makeFace (std::shared_ptr<Library> library, const std::string &path, FT_Long index);

	FT_Face operator-> ();

	FT_ULong getFirstChar (FT_UInt &faceIndex);
	FT_ULong getNextChar (FT_ULong charCode, FT_UInt &faceIndex);
	FT_Error loadGlyph (FT_UInt glyphIndex, FT_Int32 loadFlags);

	FT_Error selectCharmap (FT_Encoding encoding);
	FT_Error setCharSize (double ptSize);

private:
	Face ();

	std::shared_ptr<Library> m_library;
	FT_Face m_face;
};

const char *strerror (FT_Error error);
}
