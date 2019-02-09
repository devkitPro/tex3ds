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
/** @file freetype.cpp
 *  @brief FreeType wrappers
 */

#include "freetype.h"
using namespace freetype;

///////////////////////////////////////////////////////////////////////////
Library::~Library ()
{
	if (m_library)
		FT_Done_FreeType (m_library);
}

Library::Library () : m_library ()
{
}

std::shared_ptr<Library> Library::makeLibrary ()
{
	auto library = std::shared_ptr<Library> ();
	library.reset (new Library ());

	FT_Error error = FT_Init_FreeType (&library->m_library);
	if (error)
	{
		std::fprintf (stderr, "FT_Init_FreeType: %s\n", freetype::strerror (error));
		return nullptr;
	}

	return library;
}

FT_Library Library::library () const
{
	return m_library;
}

///////////////////////////////////////////////////////////////////////////
Face::~Face ()
{
	if (m_face)
		FT_Done_Face (m_face);
}

Face::Face () : m_library (), m_face ()
{
}

std::unique_ptr<Face>
    Face::makeFace (std::shared_ptr<Library> library, const std::string &path, FT_Long index)
{
	auto face = std::unique_ptr<Face> ();
	face.reset (new Face ());

	face->m_library = std::move (library);

	FT_Error error = FT_New_Face (face->m_library->library (), path.c_str (), index, &face->m_face);
	if (error)
	{
		std::fprintf (stderr, "FT_New_Face: %s\n", freetype::strerror (error));
		return nullptr;
	}

	return face;
}

FT_Face Face::operator-> ()
{
	return m_face;
}

FT_ULong Face::getFirstChar (FT_UInt &faceIndex)
{
	return FT_Get_First_Char (m_face, &faceIndex);
}

FT_ULong Face::getNextChar (FT_ULong charCode, FT_UInt &faceIndex)
{
	return FT_Get_Next_Char (m_face, charCode, &faceIndex);
}

FT_Error Face::loadGlyph (FT_UInt glyphIndex, FT_Int32 loadFlags)
{
	FT_Error error = FT_Load_Glyph (m_face, glyphIndex, loadFlags);
	if (error)
		std::fprintf (stderr, "FT_Load_Glyph: %s\n", freetype::strerror (error));

	return error;
}

FT_Error Face::selectCharmap (FT_Encoding encoding)
{
	FT_Error error = FT_Select_Charmap (m_face, encoding);
	if (error)
		std::fprintf (stderr, "FT_Select_Charmap: %s\n", freetype::strerror (error));

	return error;
}

FT_Error Face::setCharSize (double ptSize)
{
	FT_Error error = FT_Set_Char_Size (m_face, ptSize * (1 << 6), 0, 96, 0);
	if (error)
		std::fprintf (stderr, "FT_Set_Char_Size: %s\n", freetype::strerror (error));

	return error;
}

///////////////////////////////////////////////////////////////////////////
const char *freetype::strerror (FT_Error error)
{
#undef __FTERRORS_H__
#define FT_ERRORDEF(e, v, s)                                                                       \
	case e:                                                                                        \
		return s;
#define FT_ERROR_START_LIST                                                                        \
	switch (error)                                                                                 \
	{
#define FT_ERROR_END_LIST }
#include FT_ERRORS_H

	return "(Unknown error)";
}
