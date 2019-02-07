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
/** @file bcfnt.cpp
 *  @brief BCFNT definitions
 */

#include "magick_compat.h"

#include "bcfnt.h"
#include "ft_error.h"
#include "future.h"
#include "quantum.h"
#include "swizzle.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>

namespace
{
void appendSheet (std::vector<std::uint8_t> &data, Magick::Image &sheet)
{
	swizzle (sheet, false);

	const unsigned w = sheet.columns ();
	const unsigned h = sheet.rows ();

	data.reserve (data.size () + w * h / 2);

	Pixels cache (sheet);
	for (unsigned y = 0; y < h; y += 8)
	{
		for (unsigned x = 0; x < w; x += 8)
		{
			PixelPacket p = cache.get (x, y, 8, 8);
			for (unsigned i = 0; i < 8 * 8; i += 2)
			{
				data.emplace_back ((quantum_to_bits<4> (quantumAlpha (p[i + 1])) << 4) |
				                   (quantum_to_bits<4> (quantumAlpha (p[i + 0])) << 0));
			}
		}
	}
}

Magick::Image unpackSheet (std::vector<std::uint8_t>::const_iterator &data,
    const unsigned WIDTH,
    const unsigned HEIGHT)
{
	Magick::Image ret (Magick::Geometry (WIDTH, HEIGHT), transparent ());
	Pixels cache (ret);
	for (unsigned y = 0; y < HEIGHT; y += 8)
	{
		for (unsigned x = 0; x < WIDTH; x += 8)
		{
			PixelPacket p = cache.get (x, y, 8, 8);
			for (unsigned i = 0; i < 8 * 8 / 2; ++i)
			{
				Magick::Color opacity;
				opacity.alphaQuantum (bits_to_quantum<4> ((*data) & 0xF));
				p[2 * i].opacity = quantumAlpha (opacity);
				opacity.alphaQuantum (bits_to_quantum<4> ((*data) >> 4 & 0xF));
				p[2 * i + 1].opacity = quantumAlpha (opacity);
				++data;
			}
			cache.sync ();
		}
	}

	swizzle (ret, true);

	return ret;
}

void coalesceCMAP (std::vector<bcfnt::CMAP> &cmaps)
{
	static constexpr auto MIN_CHARS          = 7;
	std::uint16_t codeBegin                  = 0xFFFF;
	std::uint16_t codeEnd                    = 0;
	std::unique_ptr<bcfnt::CMAPScan> scanMap = future::make_unique<bcfnt::CMAPScan> ();
	auto cmap                                = cmaps.begin ();
	while (cmap != cmaps.end ())
	{
		if (cmap->mappingMethod == bcfnt::CMAPData::CMAP_TYPE_DIRECT &&
		    cmap->codeEnd - cmap->codeBegin < MIN_CHARS - 1)
		{
			if (cmap->codeBegin < codeBegin)
				codeBegin = cmap->codeBegin;

			if (cmap->codeEnd > codeEnd)
				codeEnd = cmap->codeEnd;

			const auto &direct = dynamic_cast<bcfnt::CMAPDirect &> (*cmap->data);
			for (std::uint16_t i = cmap->codeBegin; i <= cmap->codeEnd; i++)
				scanMap->entries.emplace_back (bcfnt::CMAPScan::Entry{
				    i, static_cast<uint16_t> (i - cmap->codeBegin + direct.offset)});

			cmap = cmaps.erase (cmap);
		}
		else
			++cmap;
	}
	cmaps.push_back ({codeBegin,
	    codeEnd,
	    static_cast<uint16_t> (bcfnt::CMAPData::CMAP_TYPE_SCAN),
	    0,
	    0,
	    std::move (scanMap)});
}

std::vector<std::uint8_t> &operator<< (std::vector<std::uint8_t> &o, const char *str)
{
	const std::size_t len = std::strlen (str);

	o.reserve (o.size () + len);
	o.insert (o.end (), str, str + len);

	return o;
}

std::vector<std::uint8_t> &operator<< (std::vector<std::uint8_t> &o, std::uint8_t v)
{
	o.emplace_back (v);

	return o;
}

std::vector<std::uint8_t> &operator<< (std::vector<std::uint8_t> &o, std::uint16_t v)
{
	o.reserve (o.size () + 2);
	o.emplace_back ((v >> 0) & 0xFF);
	o.emplace_back ((v >> 8) & 0xFF);

	return o;
}

std::vector<std::uint8_t> &operator<< (std::vector<std::uint8_t> &o, std::uint32_t v)
{
	o.reserve (o.size () + 4);
	o.emplace_back ((v >> 0) & 0xFF);
	o.emplace_back ((v >> 8) & 0xFF);
	o.emplace_back ((v >> 16) & 0xFF);
	o.emplace_back ((v >> 24) & 0xFF);

	return o;
}

std::vector<std::uint8_t> &operator<< (std::vector<std::uint8_t> &o, const bcfnt::CMAPScan &v)
{
	o << static_cast<uint16_t> (v.entries.size ());
	for (const auto &entry : v.entries)
	{
		o << static_cast<uint16_t> (entry.code) << static_cast<uint16_t> (entry.glyphIndex);
	}

	return o;
}

std::vector<std::uint8_t> &operator<< (std::vector<std::uint8_t> &o, const bcfnt::CMAPTable &v)
{
	for (const auto &entry : v.table)
	{
		o << static_cast<uint16_t> (entry);
	}
	if (v.table.size () % 2)
		o << static_cast<uint16_t> (0);

	return o;
}

std::vector<std::uint8_t>::const_iterator &operator>> (std::vector<std::uint8_t>::const_iterator &i,
    std::uint32_t &v)
{
	v = *(i++) | *(i++) << 8 | *(i++) << 16 | *(i++) << 24;
	return i;
}

std::vector<std::uint8_t>::const_iterator &operator>> (std::vector<std::uint8_t>::const_iterator &i,
    std::uint16_t &v)
{
	v = *(i++) | *(i++) << 8;
	return i;
}

std::vector<std::uint8_t>::const_iterator &operator>> (std::vector<std::uint8_t>::const_iterator &i,
    std::uint8_t &v)
{
	v = *(i++);
	return i;
}

std::vector<std::uint8_t>::const_iterator &operator>> (std::vector<std::uint8_t>::const_iterator &i,
    bcfnt::CharWidthInfo &v)
{
	v.left       = *(i++);
	v.glyphWidth = *(i++);
	v.charWidth  = *(i++);
	return i;
}

std::vector<std::uint8_t>::const_iterator &operator>> (std::vector<std::uint8_t>::const_iterator &i,
    bcfnt::CMAPScan::Entry &v)
{
	i >> v.code;
	i >> v.glyphIndex;
	return i;
}
}

namespace bcfnt
{
struct CharMap
{
	CharMap () : code (0), faceIndex (0), cfntIndex (0)
	{
	}

	CharMap (FT_ULong code, FT_UInt faceIndex, FT_Face face)
	    : code (code), faceIndex (faceIndex), cfntIndex (0), face (face)
	{
	}

	const FT_ULong code;     ///< Code point.
	const FT_UInt faceIndex; ///< FreeType face index.
	std::uint16_t cfntIndex; ///< CFNT glyph index.
	FT_Face face;
};

std::uint16_t CMAP::codePointFromIndex (std::uint16_t index) const
{
	switch (mappingMethod)
	{
	case bcfnt::CMAPData::CMAP_TYPE_DIRECT:
	{
		std::uint16_t chars = codeEnd - codeBegin + 1;
		CMAPDirect &direct  = dynamic_cast<CMAPDirect &> (*data);
		if (index - direct.offset < chars)
		{
			return codeBegin + index - direct.offset;
		}
		return 0xFFFF;
	}

	case bcfnt::CMAPData::CMAP_TYPE_TABLE:
	{
		CMAPTable &table = dynamic_cast<CMAPTable &> (*data);
		auto found       = std::find (table.table.begin (), table.table.end (), index);
		if (found != table.table.end ())
		{
			return std::distance (table.table.begin (), found) + codeBegin;
		}
		return 0xFFFF;
	}

	case bcfnt::CMAPData::CMAP_TYPE_SCAN:
	{
		CMAPScan &scan = dynamic_cast<CMAPScan &> (*data);
		auto found     = std::find_if (scan.entries.begin (),
            scan.entries.end (),
            [index](const CMAPScan::Entry &e) { return e.glyphIndex == index; });
		if (found != scan.entries.end ())
		{
			return found->code;
		}
		return 0xFFFF;
	}

	default:
		abort ();
	}
}

BCFNT::BCFNT (std::vector<FT_Face> &faces)
{
	int descent = std::numeric_limits<int>::max ();

	for (auto &face : faces)
	{
		lineFeed = std::max (lineFeed, static_cast<uint8_t> (face->size->metrics.height >> 6));
		height = std::max (height, static_cast<uint8_t> ((face->bbox.yMax - face->bbox.yMin) >> 6));
		width  = std::max (width, static_cast<uint8_t> ((face->bbox.xMax - face->bbox.xMin) >> 6));
		maxWidth = std::max (maxWidth, static_cast<uint8_t> (face->size->metrics.max_advance >> 6));
		ascent   = std::max (ascent, static_cast<uint8_t> (face->size->metrics.ascender >> 6));
		descent  = std::min (descent, static_cast<int> (face->size->metrics.descender) >> 6);

		// extract mappings from font face
		FT_UInt faceIndex;
		FT_ULong code = FT_Get_First_Char (face, &faceIndex);
		while (faceIndex != 0)
		{
			// only supports 16-bit code points; also 0xFFFF is explicitly a non-character
			if (code >= std::numeric_limits<std::uint16_t>::max () || glyphs.count (code))
			{
				code = FT_Get_Next_Char (face, code, &faceIndex);
				continue;
			}

			FT_Error error = FT_Load_Glyph (face, faceIndex, FT_LOAD_RENDER);
			if (error)
			{
				std::fprintf (stderr, "FT_Load_Glyph: %s\n", ft_error (error));
				code = FT_Get_Next_Char (face, code, &faceIndex);
				continue;
			}

			if (face->glyph->bitmap_top > ascent)
				ascent = face->glyph->bitmap_top;

			if (static_cast<int> (face->glyph->bitmap_top) -
			        static_cast<int> (face->glyph->bitmap.rows) <
			    descent)
				descent = face->glyph->bitmap_top - face->glyph->bitmap.rows;

			if (face->glyph->bitmap.width > maxWidth)
				maxWidth = face->glyph->bitmap.width;

			cellWidth   = maxWidth + 1;
			cellHeight  = ascent - descent;
			glyphWidth  = cellWidth + 1;
			glyphHeight = cellHeight + 1;

			glyphs.emplace (code, currentGlyphImage (face));

			code = FT_Get_Next_Char (face, code, &faceIndex);
		}
	}

	cellWidth      = maxWidth + 1;
	cellHeight     = ascent - descent;
	glyphWidth     = cellWidth + 1;
	glyphHeight    = cellHeight + 1;
	glyphsPerRow   = SHEET_WIDTH / glyphWidth;
	glyphsPerCol   = SHEET_HEIGHT / glyphHeight;
	glyphsPerSheet = glyphsPerRow * glyphsPerCol;

	if (glyphs.empty ())
		return;

	// try to provide a replacement character
	if (glyphs.count (0xFFFD))
		altIndex = std::distance (glyphs.begin (), glyphs.find (0xFFFD));
	else if (glyphs.count ('?'))
		altIndex = std::distance (glyphs.begin (), glyphs.find ('?'));
	else if (glyphs.count (' '))
		altIndex = std::distance (glyphs.begin (), glyphs.find (' '));
	else
		altIndex = 0;

	// collect character mappings
	refreshCMAPs ();

	numSheets = glyphs.size () / glyphsPerSheet + (glyphs.size () % glyphsPerSheet ? 1 : 0);

	coalesceCMAP (cmaps);
}

BCFNT::BCFNT (const std::vector<std::uint8_t> &data)
{
	assert (data.size () >= 0x10);
	auto i = data.begin ();
	std::uint16_t in16;
	std::uint32_t in32;
	i += 4;    // CFNT magic
	i >> in16; // BOM
	if (in16 != 0xFEFF)
	{
		fprintf (stderr, "No support for big-endian BCFNTs yet");
		return;
	}
	i += 2;                        // header size
	i += 4;                        // version
	i >> in32;                     // file size
	assert (in32 <= data.size ()); // Check the file size
	i += 4;                        // number of blocks

	i += 4; // FINF magic
	i += 4; // section size
	i += 1; // font type (still not sure about this thing)
	i >> lineFeed;
	i >> altIndex;
	i >> defaultWidth;
	i += 1; // encoding
	std::uint32_t tglpOffset;
	std::uint32_t cwdhOffset;
	std::uint32_t cmapOffset;
	i >> tglpOffset;
	i >> cwdhOffset;
	i >> cmapOffset;
	i >> height;
	i >> width;
	i >> ascent;
	i += 1; // padding

	// Do the CMAP stuff first
	while (cmapOffset != 0)
	{
		i = data.begin () + cmapOffset - 4; // Skip to right after CMAP magic, on section size
		i >> in32;                          // CMAP size
		in32 -= 0x14;                       // size without CMAP header
		assert (in32 % 4 == 0); // If it's not a multiple of two, something is very wrong
		bcfnt::CMAP cmap;
		i >> cmap.codeBegin; // Start codepoint
		i >> cmap.codeEnd;   // End codepoint
		i >> cmap.mappingMethod;
		i >> cmap.reserved;
		i >> cmapOffset;

		switch (cmap.mappingMethod)
		{
		case bcfnt::CMAPData::CMAP_TYPE_DIRECT:
			assert (in32 == 0x4);
			i >> in16;
			cmap.data = future::make_unique<bcfnt::CMAPDirect> (in16);
			break;

		case bcfnt::CMAPData::CMAP_TYPE_TABLE:
			assert (in32 == static_cast<std::uint16_t> (cmap.codeEnd - cmap.codeBegin + 1) * 2 +
			                    ((cmap.codeEnd - cmap.codeBegin + 1) % 2) * 2);
			cmap.data = future::make_unique<bcfnt::CMAPTable> ();
			for (std::uint16_t code = cmap.codeBegin; code <= cmap.codeEnd; code++)
			{
				i >> in16;
				dynamic_cast<CMAPTable &> (*cmap.data).table.emplace_back (in16);
			}
			break;

		case bcfnt::CMAPData::CMAP_TYPE_SCAN:
			i >> in16; // number of entries
			assert (in32 == (in16 + 1) * 4);
			cmap.data = future::make_unique<bcfnt::CMAPScan> ();
			dynamic_cast<CMAPScan &> (*cmap.data).entries.resize (in16);
			for (std::uint16_t entry = 0; entry < in16; entry++)
			{
				i >> dynamic_cast<CMAPScan &> (*cmap.data).entries[entry];
			}
			break;

		default:
			abort ();
		}

		cmaps.emplace_back (std::move (cmap));
	}

	i = data.begin () + tglpOffset; // Fast forward to TGLP
	i >> cellWidth;
	i >> cellHeight;
	glyphWidth  = cellWidth + 1;
	glyphHeight = cellHeight + 1;
	i += 1; // baseline (same as ascent)
	i >> maxWidth;
	i >> SHEET_SIZE;
	i >> numSheets;
	i >> in16; // sheet format
	if (in16 != 0xB)
	{
		fprintf (stderr, "No formats except for 4-bit alpha currently supported");
		return;
	}
	i >> glyphsPerRow;
	i >> glyphsPerCol;
	glyphsPerSheet = glyphsPerRow * glyphsPerCol;
	i >> SHEET_WIDTH;
	i >> SHEET_HEIGHT;
	assert (SHEET_WIDTH * SHEET_HEIGHT / 2 == SHEET_SIZE);
	assert (SHEET_WIDTH / glyphWidth == glyphsPerRow);
	assert (SHEET_HEIGHT / glyphHeight == glyphsPerCol);
	i >> in32; // Sheet Offset
	i = data.begin () + in32;
	readGlyphImages (i, numSheets);
	printf ("Glyphs: %i", glyphs.size ());

	while (cwdhOffset != 0)
	{
		i = data.begin () + cwdhOffset - 4; // Skip to right after CWDH magic, on section size
		i >> in32;                          // CWDH size
		in32 -= 0x10;                       // size without CWDH header
		// assert(in32 % 3 == 0); // If it's not a multiple of three, something is very wrong
		std::uint16_t startIndex;
		i >> startIndex; // start index
		i >> in16;       // end index
		// assert(in32 == static_cast<std::uint16_t>(in16 - startIndex) * 3);
		i >> cwdhOffset;
		assert (in16 <= glyphs.size ());
		for (std::uint16_t glyph = startIndex; glyph < in16; glyph++)
		{
			i >> glyphs[codepoint (glyph)].info;
		}
	}
}

bool BCFNT::serialize (const std::string &path)
{
	if (glyphs.empty ())
	{
		std::fprintf (stderr, "Empty font\n");
		return false;
	}
	std::vector<Magick::Image> sheetImages = sheetify ();
	for (size_t i = 0; i < sheetImages.size (); i++)
	{
		sheetImages[i].magick ("PNG");
		sheetImages[i].write ("sheet" + std::to_string (i) + ".png");
	}
	std::vector<std::uint8_t> output;

	std::uint32_t fileSize = 0;
	fileSize += 0x14; // CFNT header

	const std::uint32_t finfOffset = fileSize;
	fileSize += 0x20; // FINF header

	const std::uint32_t tglpOffset = fileSize;
	fileSize += 0x20; // TGLP header

	constexpr std::uint32_t ALIGN   = 0x80;
	constexpr std::uint32_t MASK    = ALIGN - 1;
	const std::uint32_t sheetOffset = (fileSize + MASK) & ~MASK;
	fileSize                        = sheetOffset + sheetImages.size () * SHEET_SIZE;

	// CWDH headers + data
	const std::uint32_t cwdhOffset = fileSize;
	fileSize += 0x10;                          // CWDH header
	fileSize += (3 * glyphs.size () + 3) & ~3; // CWDH data

	// CMAP headers + data
	std::uint32_t cmapOffset = fileSize;
	for (const auto &cmap : cmaps)
	{
		fileSize += 0x14; // CMAP header

		switch (cmap.mappingMethod)
		{
		case CMAPData::CMAP_TYPE_DIRECT:
			fileSize += 0x4;
			break;

		case CMAPData::CMAP_TYPE_TABLE:
			fileSize += dynamic_cast<const CMAPTable &> (*cmap.data).table.size () * 2 +
			            (dynamic_cast<const CMAPTable &> (*cmap.data).table.size () % 2) * 2;
			break;

		case CMAPData::CMAP_TYPE_SCAN:
			fileSize += 4 + dynamic_cast<const CMAPScan &> (*cmap.data).entries.size () * 4;
			break;

		default:
			abort ();
		}
	}

	// FINF, TGLP, CWDH, CMAPs
	std::uint32_t numBlocks = 3 + cmaps.size ();

	// CFNT header
	output << "CFNT"                                  // magic
	       << static_cast<std::uint16_t> (0xFEFF)     // byte-order-mark
	       << static_cast<std::uint16_t> (0x14)       // header size
	       << static_cast<std::uint8_t> (0x0)         // version (?)
	       << static_cast<std::uint8_t> (0x0)         // version (?)
	       << static_cast<std::uint8_t> (0x0)         // version (?)
	       << static_cast<std::uint8_t> (0x3)         // version
	       << static_cast<std::uint32_t> (fileSize)   // file size
	       << static_cast<std::uint32_t> (numBlocks); // number of blocks

	// FINF header
	assert (output.size () == finfOffset);
	output << "FINF"                                              // magic
	       << static_cast<std::uint32_t> (0x20)                   // section size
	       << static_cast<std::uint8_t> (0x1)                     // font type
	       << static_cast<std::uint8_t> (lineFeed)                // line feed
	       << static_cast<std::uint16_t> (altIndex)               // alternate char index
	       << static_cast<std::uint8_t> (defaultWidth.left)       // default width (left)
	       << static_cast<std::uint8_t> (defaultWidth.glyphWidth) // default width (glyph width)
	       << static_cast<std::uint8_t> (defaultWidth.charWidth)  // default width (char width)
	       << static_cast<std::uint8_t> (0x1)                     // encoding
	       << static_cast<std::uint32_t> (tglpOffset + 8)         // TGLP offset
	       << static_cast<std::uint32_t> (cwdhOffset + 8)         // CWDH offset
	       << static_cast<std::uint32_t> (cmapOffset + 8)         // CMAP offset
	       << static_cast<std::uint8_t> (height)                  // font height
	       << static_cast<std::uint8_t> (width)                   // font width
	       << static_cast<std::uint8_t> (ascent)                  // font ascent
	       << static_cast<std::uint8_t> (0x0);                    // padding

	// TGLP header
	assert (output.size () == tglpOffset);
	output << "TGLP"                                    // magic
	       << static_cast<std::uint32_t> (0x20)         // section size
	       << static_cast<std::uint8_t> (cellWidth)     // cell width
	       << static_cast<std::uint8_t> (cellHeight)    // cell height
	       << static_cast<std::uint8_t> (ascent)        // cell baseline
	       << static_cast<std::uint8_t> (maxWidth)      // max character width
	       << static_cast<std::uint32_t> (SHEET_SIZE)   // sheet data size
	       << static_cast<std::uint16_t> (numSheets)    // number of sheets
	       << static_cast<std::uint16_t> (0xB)          // 4-bit alpha format
	       << static_cast<std::uint16_t> (glyphsPerRow) // num columns
	       << static_cast<std::uint16_t> (glyphsPerCol) // num rows
	       << static_cast<std::uint16_t> (SHEET_WIDTH)  // sheet width
	       << static_cast<std::uint16_t> (SHEET_HEIGHT) // sheet height
	       << static_cast<std::uint32_t> (sheetOffset); // sheet data offset

	assert (output.size () <= sheetOffset);
	output.resize (sheetOffset);
	assert (output.size () == sheetOffset);
	for (auto sheet : sheetImages)
	{
		appendSheet (output, sheet);
	}

	// CWDH header + data
	assert (output.size () == cwdhOffset);

	output << "CWDH"                                                            // magic
	       << static_cast<std::uint32_t> (0x10 + (3 * glyphs.size () + 3) & ~3) // section size
	       << static_cast<std::uint16_t> (0)                                    // start index
	       << static_cast<std::uint16_t> (glyphs.size ())                       // end index
	       << static_cast<std::uint32_t> (0);                                   // next CWDH offset

	for (const auto &info : glyphs)
	{
		output << static_cast<std::uint8_t> (info.second.info.left)
		       << static_cast<std::uint8_t> (info.second.info.glyphWidth)
		       << static_cast<std::uint8_t> (info.second.info.charWidth);
	}
	for (int i = 0; i < (4 - (3 * glyphs.size () % 4)) % 4; i++)
		output << static_cast<std::uint8_t> (0);

	for (const auto &cmap : cmaps)
	{
		assert (output.size () == cmapOffset);

		std::uint32_t size;
		switch (cmap.mappingMethod)
		{
		case bcfnt::CMAPData::CMAP_TYPE_DIRECT:
			size = 0x14 + 0x4;
			break;

		case bcfnt::CMAPData::CMAP_TYPE_TABLE:
			size = 0x14 + dynamic_cast<bcfnt::CMAPTable &> (*cmap.data).table.size () * 2 +
			       (dynamic_cast<bcfnt::CMAPTable &> (*cmap.data).table.size () % 2) * 2;
			break;

		case bcfnt::CMAPData::CMAP_TYPE_SCAN:
			size = 0x14 + 4 + dynamic_cast<bcfnt::CMAPScan &> (*cmap.data).entries.size () * 4;
			break;

		default:
			abort ();
		}

		output << "CMAP"                                          // magic
		       << static_cast<std::uint32_t> (size)               // section size
		       << static_cast<std::uint16_t> (cmap.codeBegin)     // code begin
		       << static_cast<std::uint16_t> (cmap.codeEnd)       // code end
		       << static_cast<std::uint16_t> (cmap.mappingMethod) // mapping method
		       << static_cast<std::uint16_t> (0x0);               // padding

		// next CMAP offset
		if (&cmap == &cmaps.back ())
			output << static_cast<std::uint32_t> (0);
		else
			output << static_cast<std::uint32_t> (cmapOffset + size + 8);

		switch (cmap.mappingMethod)
		{
		case CMAPData::CMAP_TYPE_DIRECT:
		{
			const auto &direct = dynamic_cast<const CMAPDirect &> (*cmap.data);
			output << static_cast<std::uint16_t> (direct.offset);
			output << static_cast<std::uint16_t> (0); // alignment
			break;
		}

		case CMAPData::CMAP_TYPE_TABLE:
		{
			const auto &table = dynamic_cast<const CMAPTable &> (*cmap.data);
			output << table;
			break;
		}

		case CMAPData::CMAP_TYPE_SCAN:
		{
			const auto &scan = dynamic_cast<const CMAPScan &> (*cmap.data);
			output << scan;
			output << static_cast<std::uint16_t> (0); // alignment
			break;
		}

		default:
			abort ();
		}

		cmapOffset += size;
	}

	assert (output.size () == fileSize);

	FILE *fp = std::fopen (path.c_str (), "wb");
	if (!fp)
		return false;

	std::size_t offset = 0;
	while (offset < output.size ())
	{
		std::size_t rc = std::fwrite (&output[offset], 1, output.size () - offset, fp);
		if (rc != output.size () - offset)
		{
			if (rc == 0 || std::ferror (fp))
			{
				if (std::ferror (fp))
					std::fprintf (stderr, "fwrite: %s\n", std::strerror (errno));
				else
					std::fprintf (stderr, "fwrite: Unknown write failure\n");

				std::fclose (fp);
				return false;
			}
		}

		offset += rc;
	}

	if (std::fclose (fp) != 0)
	{
		std::fprintf (stderr, "fclose: %s\n", std::strerror (errno));
		return false;
	}

	std::printf ("Generated font with %zu glyphs\n", glyphs.size ());
	return true;
}

std::vector<Magick::Image> BCFNT::sheetify ()
{
	std::map<std::uint16_t, bcfnt::Glyph>::iterator currentGlyph = glyphs.begin ();

	std::vector<Magick::Image> ret;

	for (unsigned sheet = 0; sheet * glyphsPerSheet < glyphs.size (); sheet++)
	{
		Magick::Image sheetData (Magick::Geometry (SHEET_WIDTH, SHEET_HEIGHT), transparent ());
		Pixels cache (sheetData);
		for (unsigned y = 0; y < glyphsPerCol; y++)
		{
			for (unsigned x = 0; x < glyphsPerRow; x++)
			{
				const unsigned glyphIndex =
				    sheet * glyphsPerRow * glyphsPerCol + y * glyphsPerRow + x;
				if (glyphIndex >= glyphs.size ())
				{
					ret.emplace_back (std::move (sheetData));
					return ret;
				}

				sheetData.composite (currentGlyph->second.img,
				    x * glyphWidth + 1,
				    y * glyphHeight + 1 + ascent - currentGlyph->second.ascent,
				    Magick::OverCompositeOp);

				currentGlyph++;
			}
		}

		ret.emplace_back (std::move (sheetData));
	}

	return ret;
}

Glyph BCFNT::currentGlyphImage (FT_Face face) const
{
	Glyph ret{Magick::Image (Magick::Geometry (glyphWidth, glyphHeight), transparent ()),
	    CharWidthInfo{static_cast<std::uint16_t> (face->glyph->metrics.horiBearingX >> 6),
	        static_cast<std::uint16_t> (face->glyph->metrics.width >> 6),
	        static_cast<std::uint16_t> (face->glyph->metrics.horiAdvance >> 6)},
	    ascent};
	Pixels cache (ret.img);
	PixelPacket p = cache.get (1, 1, cellWidth, cellHeight);
	for (unsigned y = 0; y < face->glyph->bitmap.rows; ++y)
	{
		for (unsigned x = 0; x < face->glyph->bitmap.width; ++x)
		{
			const int py = y + (ascent - face->glyph->bitmap_top);

			if (x >= cellWidth || py < 0 || py >= cellHeight)
				continue;

			const std::uint8_t v = face->glyph->bitmap.buffer[y * face->glyph->bitmap.width + x];

			Magick::Color c;
			quantumRed (c, bits_to_quantum<8> (0));
			quantumGreen (c, bits_to_quantum<8> (0));
			quantumBlue (c, bits_to_quantum<8> (0));
			quantumAlpha (c, bits_to_quantum<8> (v));

			p[py * cellWidth + x] = c;
		}
	}
	cache.sync ();

	return ret;
}

std::uint16_t BCFNT::codepoint (std::uint16_t index) const
{
	std::vector<CMAP>::const_iterator i = cmaps.begin ();
	std::uint16_t code                  = 0xFFFF;
	while (code == 0xFFFF && i != cmaps.end ())
	{
		code = i->codePointFromIndex (index);
		i++;
	}
	return code;
}

void BCFNT::readGlyphImages (std::vector<std::uint8_t>::const_iterator &i, int numSheets)
{
	for (int sheet = 0; sheet < numSheets; sheet++)
	{
		Magick::Image sheetData = unpackSheet (i, SHEET_WIDTH, SHEET_HEIGHT);
		Pixels cache (sheetData);
		for (unsigned row = 0; row < glyphsPerCol; row++)
		{
			for (unsigned column = 0; column < glyphsPerRow; column++)
			{
				PixelPacket glyphData = cache.get (
				    column * glyphWidth + 1, row * glyphHeight + 1, cellWidth, cellHeight);
				Magick::Image glyph (Magick::Geometry (glyphWidth, glyphHeight), transparent ());
				Pixels glyphPixels (glyph);
				PixelPacket outData = glyphPixels.get (0, 0, cellWidth, cellHeight);

				for (unsigned pixel = 0; pixel < cellWidth * cellHeight; pixel++)
				{
					outData[pixel] = glyphData[pixel];
				}
				glyphPixels.sync ();

				const std::uint16_t code =
				    codepoint (sheet * glyphsPerSheet + row * glyphsPerRow + column);
				if (code != 0xFFFF)
					glyphs.emplace (code, Glyph{glyph, bcfnt::CharWidthInfo{0, 0, 0}, ascent});
			}
		}
	}
}

void BCFNT::refreshCMAPs ()
{
	cmaps.clear ();
	for (auto it = glyphs.begin (); it != glyphs.end (); it++)
	{
		if (cmaps.empty () || cmaps.back ().codeEnd != it->first - 1)
		{
			cmaps.emplace_back ();
			auto &cmap     = cmaps.back ();
			cmap.codeBegin = cmap.codeEnd = it->first;
			cmap.data = future::make_unique<CMAPDirect> (std::distance (glyphs.begin (), it));
			cmap.mappingMethod = cmap.data->type ();
		}
		else
			cmaps.back ().codeEnd = it->first;
	}
}

BCFNT &BCFNT::operator+= (const BCFNT &other)
{
	std::uint8_t newAscent = std::max (other.ascent, ascent);
	std::uint8_t newCellHeight =
	    newAscent + std::max (other.cellHeight - other.ascent, cellHeight - ascent);
	std::uint8_t newCellWidth = std::max (other.cellWidth, cellWidth);

	for (auto &i : other.glyphs)
	{
		if (i.first != 0xFFFF && !glyphs.count (i.first))
		{
			glyphs.emplace (i);
		}
	}

	refreshCMAPs ();

	ascent         = newAscent;
	cellHeight     = newCellHeight;
	cellWidth      = newCellWidth;
	glyphHeight    = cellHeight + 1;
	glyphWidth     = cellWidth + 1;
	glyphsPerRow   = SHEET_WIDTH / glyphWidth;
	glyphsPerCol   = SHEET_HEIGHT / glyphHeight;
	glyphsPerSheet = glyphsPerRow * glyphsPerCol;
	lineFeed       = std::max (lineFeed, other.lineFeed);
	height         = std::max (height, other.height);
	width          = std::max (width, other.width);
	maxWidth       = cellWidth;
	numSheets      = glyphs.size () / glyphsPerSheet + (glyphs.size () % glyphsPerSheet ? 1 : 0);
	return *this;
}
}
