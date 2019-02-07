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
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <queue>
#include <thread>

namespace
{
void doneFaces (const std::vector<FT_Face> &faces)
{
	for (auto &face : faces)
	{
		if (face)
			FT_Done_Face (face);
	}
}

std::vector<FT_Face> newFaces (FT_Library library, const std::string &path, const double ptSize)
{
	const auto numFaces = std::max (1u, std::thread::hardware_concurrency ()) + 1;
	std::vector<FT_Face> faces (numFaces);

	for (auto &face : faces)
	{
		FT_Error error = FT_New_Face (library, path.c_str (), 0, &face);
		if (error)
		{
			std::fprintf (stderr, "FT_New_Face: %s\n", ft_error (error));
			doneFaces (faces);
			FT_Done_FreeType (library);
			return {};
		}

		error = FT_Select_Charmap (face, FT_ENCODING_UNICODE);
		if (error)
		{
			std::fprintf (stderr, "FT_Select_Charmap: %s\n", ft_error (error));
			doneFaces (faces);
			FT_Done_FreeType (library);
			return {};
		}

		error = FT_Set_Char_Size (face, ptSize * (1 << 6), 0, 96, 0);
		if (error)
		{
			std::fprintf (stderr, "FT_Set_Pixel_Sizes: %s\n", ft_error (error));
			doneFaces (faces);
			FT_Done_FreeType (library);
			return {};
		}
	}

	return faces;
}

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
				    i, static_cast<std::uint16_t> (i - cmap->codeBegin + direct.offset)});

			cmap = cmaps.erase (cmap);
		}
		else
			++cmap;
	}
	cmaps.push_back ({codeBegin,
	    codeEnd,
	    static_cast<std::uint16_t> (bcfnt::CMAPData::CMAP_TYPE_SCAN),
	    0,
	    0,
	    std::move (scanMap)});
}

Magick::Image getImage (const unsigned char *buffer, unsigned width, unsigned height)
{
	Magick::Image img (Magick::Geometry (width, height), transparent ());

	Pixels cache (img);
	PixelPacket p = cache.get (0, 0, width, height);

	for (unsigned y = 0; y < height; ++y)
	{
		for (unsigned x = 0; x < width; ++x)
		{
			const std::uint8_t v = *buffer++;

			Magick::Color c;
			quantumRed (c, bits_to_quantum<8> (0));
			quantumGreen (c, bits_to_quantum<8> (0));
			quantumBlue (c, bits_to_quantum<8> (0));
			quantumAlpha (c, bits_to_quantum<8> (v));

			*p++ = c;
		}
	}

	return img;
}

Magick::Image getImage (const FT_Bitmap &bitmap)
{
	return getImage (bitmap.buffer, bitmap.width, bitmap.rows);
}

template <std::size_t ALIGN, typename T>
inline T align (T offset)
{
	static_assert ((ALIGN & (ALIGN - 1)) == 0, "Alignment must be a power-of-two");
	return (offset + ALIGN - 1) & ~(ALIGN - 1);
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

std::vector<std::uint8_t> &operator<< (std::vector<std::uint8_t> &o, const bcfnt::CMAPDirect &v)
{
	o.reserve (o.size () + 0x4);

	o << static_cast<std::uint16_t> (v.offset);

	// padding
	o << static_cast<std::uint16_t> (0);

	return o;
}

std::vector<std::uint8_t> &operator<< (std::vector<std::uint8_t> &o, const bcfnt::CMAPTable &v)
{
	const auto size = align<0x4> (o.size () + v.table.size () * 0x2);
	o.reserve (size);

	for (const auto &entry : v.table)
		o << static_cast<std::uint16_t> (entry);

	// padding
	if (v.table.size () % 2)
		o << static_cast<std::uint16_t> (0);

	assert (o.size () == size);

	return o;
}

std::vector<std::uint8_t> &operator<< (std::vector<std::uint8_t> &o, const bcfnt::CMAPScan &v)
{
	const auto size = align<0x4> (o.size () + 0x2 + v.entries.size () * 0x4);
	o.reserve (size);

	o << static_cast<std::uint16_t> (v.entries.size ());

	for (const auto &entry : v.entries)
		o << static_cast<std::uint16_t> (entry.code)
		  << static_cast<std::uint16_t> (entry.glyphIndex);

	// padding
	o << static_cast<std::uint16_t> (0);

	assert (o.size () == size);

	return o;
}

std::vector<std::uint8_t> &operator<< (std::vector<std::uint8_t> &o,
    const std::vector<bcfnt::CharWidthInfo> &v)
{
	const auto size = align<0x4> (o.size () + 0x3 * v.size ());
	o.reserve (size);

	for (const auto &info : v)
	{
		o << static_cast<std::uint8_t> (info.left) << static_cast<std::uint8_t> (info.glyphWidth)
		  << static_cast<std::uint8_t> (info.charWidth);
	}

	assert (align<0x4> (o.size ()) == size);
	o.resize (size);

	return o;
}
}

namespace bcfnt
{
struct CharMap
{
	CharMap () : code (0), faceIndex (0), bitmapTop (0), cfntIndex (0)
	{
	}

	CharMap (const FT_Glyph_Metrics &metrics,
	    Magick::Image img,
	    FT_ULong code,
	    FT_UInt faceIndex,
	    FT_Int bitmapTop)
	    : metrics (metrics),
	      img (img),
	      code (code),
	      faceIndex (faceIndex),
	      bitmapTop (bitmapTop),
	      cfntIndex (0)
	{
	}

	FT_Glyph_Metrics metrics; ///< Glyph metrics.
	Magick::Image img;        ///< Glyph image.
	const FT_ULong code;      ///< Code point.
	const FT_UInt faceIndex;  ///< FreeType face index.
	const FT_Int bitmapTop;   ///< Bitmap top.
	std::uint16_t cfntIndex;  ///< CFNT glyph index.
};

BCFNT::BCFNT ()
{
}

bool BCFNT::addFont (const std::string &path, const double ptSize)
{
	FT_Library library;
	FT_Error error = FT_Init_FreeType (&library);
	if (error)
	{
		std::fprintf (stderr, "FT_Init_FreeType: %s\n", ft_error (error));
		return false;
	}

	auto faces = newFaces (library, path, ptSize);
	if (faces.empty ())
		return false;

	lineFeed    = faces[0]->size->metrics.height >> 6;
	height      = (faces[0]->bbox.yMax - faces[0]->bbox.yMin) >> 6;
	width       = (faces[0]->bbox.xMax - faces[0]->bbox.xMin) >> 6;
	maxWidth    = faces[0]->size->metrics.max_advance >> 6;
	ascent      = faces[0]->size->metrics.ascender >> 6;
	int descent = faces[0]->size->metrics.descender >> 6;

	std::map<FT_ULong, CharMap> faceMap;

	std::mutex mutex;
	std::queue<std::pair<FT_ULong, FT_UInt>> codeQueue;

	// extract mappings from font face
	FT_UInt faceIndex;
	FT_ULong code = FT_Get_First_Char (faces[0], &faceIndex);
	while (faceIndex != 0)
	{
		// only supports 16-bit code points; also 0xFFFF is explicitly a non-character
		if (code >= std::numeric_limits<std::uint16_t>::max ())
		{
			code = FT_Get_Next_Char (faces[0], code, &faceIndex);
			continue;
		}

		codeQueue.emplace (code, faceIndex);

		code = FT_Get_Next_Char (faces[0], code, &faceIndex);
	}

	auto func = [&](FT_Face face) {
		std::unique_lock<std::mutex> lock (mutex);

		while (!codeQueue.empty ())
		{
			auto pair = codeQueue.front ();
			codeQueue.pop ();
			lock.unlock ();

			auto code      = pair.first;
			auto faceIndex = pair.second;

			FT_Error error = FT_Load_Char (face, code, FT_LOAD_RENDER);
			if (error)
			{
				std::fprintf (stderr, "FT_Load_Char: %s\n", ft_error (error));
				lock.lock ();
				continue;
			}

			auto img = getImage (face->glyph->bitmap);
			lock.lock ();

			if (face->glyph->bitmap_top > ascent)
				ascent = face->glyph->bitmap_top;

			if (static_cast<int> (face->glyph->bitmap_top) -
			        static_cast<int> (face->glyph->bitmap.rows) <
			    descent)
				descent = face->glyph->bitmap_top - face->glyph->bitmap.rows;

			if (face->glyph->bitmap.width > maxWidth)
				maxWidth = face->glyph->bitmap.width;

			faceMap.emplace (code, CharMap (face->glyph->metrics, img, code, faceIndex, face->glyph->bitmap_top));
		}
	};

#if 1
	std::vector<std::thread> threads;
	for (auto &face : faces)
	{
		// reserve first face for main thread
		if (&face == &faces[0])
			continue;

		threads.emplace_back (func, face);
	}

	std::printf ("Using %zu threads\n", threads.size ());

	for (auto &thread : threads)
		thread.join ();
	threads.clear ();
#elif 0
	std::thread thread (func, faces[1]);
	thread.join ();
#else
	func (faces[1]);
#endif
	doneFaces (faces);

	cellWidth      = maxWidth + 1;
	cellHeight     = ascent - descent;
	glyphWidth     = cellWidth + 1;
	glyphHeight    = cellHeight + 1;
	glyphsPerRow   = SHEET_WIDTH / glyphWidth;
	glyphsPerCol   = SHEET_HEIGHT / glyphHeight;
	glyphsPerSheet = glyphsPerRow * glyphsPerCol;

	if (faceMap.empty ())
		return false;

	{
		// fill in CFNT index
		std::uint16_t cfntIndex = 0;
		for (auto &pair : faceMap)
			pair.second.cfntIndex = cfntIndex++;
	}

	// try to provide a replacement character
	if (faceMap.count (0xFFFD))
		altIndex = faceMap[0xFFFD].cfntIndex;
	else if (faceMap.count ('?'))
		altIndex = faceMap['?'].cfntIndex;
	else if (faceMap.count (' '))
		altIndex = faceMap[' '].cfntIndex;
	else
		altIndex = 0;

	// collect character mappings
	for (const auto &pair : faceMap)
	{
		const FT_ULong &code   = pair.first;
		const CharMap &charMap = pair.second;

		assert (code == charMap.code);

		if (code == 0 || cmaps.empty () || cmaps.back ().codeEnd != code - 1)
		{
			cmaps.emplace_back ();
			auto &cmap     = cmaps.back ();
			cmap.codeBegin = cmap.codeEnd = code;
			cmap.data                     = future::make_unique<CMAPDirect> (charMap.cfntIndex);
			cmap.mappingMethod            = cmap.data->type ();
		}
		else
			cmaps.back ().codeEnd = code;
	}

	// extract cwdh and sheet data
	std::unique_ptr<Magick::Image> sheet;
	for (const auto &cmap : cmaps)
	{
		for (std::uint16_t code = cmap.codeBegin; code <= cmap.codeEnd; ++code)
		{
			CharMap &charMap          = faceMap[code];
			FT_Glyph_Metrics &metrics = charMap.metrics;

			// convert from 26.6 fixed-point format
			const std::int8_t left        = metrics.horiBearingX >> 6;
			const std::uint8_t glyphWidth = metrics.width >> 6;
			const std::uint8_t charWidth  = metrics.horiAdvance >> 6;

			// add char width info to cwdh
			widths.emplace_back (CharWidthInfo{left, glyphWidth, charWidth});
			if (charMap.cfntIndex == altIndex)
				defaultWidth = CharWidthInfo{left, glyphWidth, charWidth};

			if (charMap.cfntIndex % glyphsPerSheet == 0)
			{
				if (sheet)
				{
					appendSheet (sheetData, *sheet);
					++numSheets;
				}

				sheet = future::make_unique<Magick::Image> (
				    Magick::Geometry (SHEET_WIDTH, SHEET_HEIGHT), transparent ());
			}

			assert (sheet);

			const unsigned sheetIndex = charMap.cfntIndex % glyphsPerSheet;
			const unsigned sheetX     = (sheetIndex % glyphsPerRow) * this->glyphWidth + 1;
			const unsigned sheetY     = (sheetIndex / glyphsPerRow) * this->glyphHeight + 1;

			sheet->composite (charMap.img,
			    Magick::Geometry (0, 0, sheetX, sheetY + ascent - charMap.bitmapTop),
			    Magick::OverCompositeOp);
		}
	}

	if (sheet)
	{
		appendSheet (sheetData, *sheet);
		++numSheets;
	}

	assert (widths.size () == faceMap.size ());
	coalesceCMAP (cmaps);

	return true;
}

bool BCFNT::serialize (const std::string &path)
{
	std::uint32_t fileSize = 0;
	fileSize += 0x14; // CFNT header

	const std::uint32_t finfOffset = align<0x4> (fileSize);
	fileSize                       = finfOffset + 0x20; // FINF header

	const std::uint32_t tglpOffset = align<0x4> (fileSize);
	fileSize                       = tglpOffset + 0x20; // TGLP header

	const std::uint32_t sheetOffset = align<0x80> (fileSize);
	fileSize                        = sheetOffset + sheetData.size ();

	// CWDH headers + data
	const std::uint32_t cwdhOffset = align<0x4> (fileSize);
	fileSize                       = cwdhOffset + 0x10;                            // CWDH header
	fileSize                       = align<0x4> (fileSize + 0x3 * widths.size ()); // CWDH data

	// CMAP headers + data
	std::uint32_t cmapOffset = fileSize;
	for (const auto &cmap : cmaps)
	{
		fileSize += 0x14; // CMAP header

		switch (cmap.mappingMethod)
		{
		case CMAPData::CMAP_TYPE_DIRECT:
			fileSize = align<0x4> (fileSize + 0x2);
			break;

		case CMAPData::CMAP_TYPE_TABLE:
			fileSize = align<0x4> (
			    fileSize + dynamic_cast<const CMAPTable &> (*cmap.data).table.size () * 0x2);
			break;

		case CMAPData::CMAP_TYPE_SCAN:
			fileSize = align<0x4> (
			    fileSize + 0x2 + dynamic_cast<const CMAPScan &> (*cmap.data).entries.size () * 0x4);
			break;

		default:
			abort ();
		}
	}

	assert (fileSize == align<0x4> (fileSize));

	std::vector<std::uint8_t> output;
	output.reserve (fileSize);

	// FINF, TGLP, CWDH, CMAPs
	std::uint32_t numBlocks = 3 + cmaps.size ();

	// CFNT header
	output << "CFNT"                                  // magic
	       << static_cast<std::uint16_t> (0xFEFF)     // byte-order-mark
	       << static_cast<std::uint16_t> (0x14)       // header size
	       << static_cast<std::uint8_t> (0x3)         // version
	       << static_cast<std::uint8_t> (0x0)         // version
	       << static_cast<std::uint8_t> (0x0)         // version
	       << static_cast<std::uint8_t> (0x0)         // version
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
	const std::uint32_t sheetSize = sheetData.size () / numSheets;
	assert (output.size () == tglpOffset);
	output << "TGLP"                                    // magic
	       << static_cast<std::uint32_t> (0x20)         // section size
	       << static_cast<std::uint8_t> (cellWidth)     // cell width
	       << static_cast<std::uint8_t> (cellHeight)    // cell height
	       << static_cast<std::uint8_t> (ascent)        // cell baseline
	       << static_cast<std::uint8_t> (maxWidth)      // max character width
	       << static_cast<std::uint32_t> (sheetSize)    // sheet data size
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
	output.reserve (output.size () + sheetData.size ());
	output.insert (output.end (), sheetData.begin (), sheetData.end ());

	// CWDH header + data
	assert (output.size () == cwdhOffset);

	output << "CWDH"                                                 // magic
	       << static_cast<std::uint32_t> (0x10 + 3 * widths.size ()) // section size
	       << static_cast<std::uint16_t> (0)                         // start index
	       << static_cast<std::uint16_t> (widths.size ())            // end index
	       << static_cast<std::uint32_t> (0);                        // next CWDH offset

	output << widths;

	for (const auto &cmap : cmaps)
	{
		assert (output.size () == cmapOffset);

		std::uint32_t size = 0x14;
		switch (cmap.mappingMethod)
		{
		case bcfnt::CMAPData::CMAP_TYPE_DIRECT:
			size = align<0x4> (size + 0x2);
			break;

		case bcfnt::CMAPData::CMAP_TYPE_TABLE:
			size = align<0x4> (
			    size + dynamic_cast<bcfnt::CMAPTable &> (*cmap.data).table.size () * 0x2);
			break;

		case bcfnt::CMAPData::CMAP_TYPE_SCAN:
			size = align<0x4> (
			    size + 0x2 + dynamic_cast<bcfnt::CMAPScan &> (*cmap.data).entries.size () * 0x4);
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
			output << dynamic_cast<const CMAPDirect &> (*cmap.data);
			break;

		case CMAPData::CMAP_TYPE_TABLE:
			output << dynamic_cast<const CMAPTable &> (*cmap.data);
			break;

		case CMAPData::CMAP_TYPE_SCAN:
			output << dynamic_cast<const CMAPScan &> (*cmap.data);
			break;

		default:
			abort ();
		}

		cmapOffset += size;
	}

	assert (output.size () == align<0x4> (fileSize));

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

	std::printf ("Generated font with %zu glyphs\n", widths.size ());
	return true;
}
}
