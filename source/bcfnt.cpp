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

#include <cassert>
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
void appendSheet(std::vector<std::uint8_t> &data, Magick::Image &sheet)
{
  swizzle(sheet, false);

  const unsigned w = sheet.columns();
  const unsigned h = sheet.rows();

  data.reserve(data.size() + w*h/2);

  Pixels cache(sheet);
  for(unsigned y = 0; y < h; y += 8)
  {
    for(unsigned x = 0; x < w; x += 8)
    {
      PixelPacket p = cache.get(x, y, 8, 8);
      for(unsigned i = 0; i < 8*8; i += 2)
      {
        data.emplace_back((quantum_to_bits<4>(quantumAlpha(p[i+1])) << 4)
                        | (quantum_to_bits<4>(quantumAlpha(p[i+0])) << 0));
      }
    }
  }
}

void coalesceCMAP(std::vector<bcfnt::CMAP> &cmaps)
{
  static constexpr auto MIN_CHARS = 7;
  std::uint16_t codeBegin = 0xFFFF;
  std::uint16_t codeEnd = 0;
  std::unique_ptr<bcfnt::CMAPScan> scanMap = future::make_unique<bcfnt::CMAPScan>();
  auto cmap = cmaps.begin();
  while (cmap != cmaps.end())
  {
    if (cmap->mappingMethod == bcfnt::CMAPData::CMAP_TYPE_DIRECT && cmap->codeEnd - cmap->codeBegin < MIN_CHARS - 1)
    {
      if(cmap->codeBegin < codeBegin)
        codeBegin = cmap->codeBegin;

      if(cmap->codeEnd > codeEnd)
        codeEnd = cmap->codeEnd;

      const auto &direct = dynamic_cast<bcfnt::CMAPDirect&>(*cmap->data);
      for(std::uint16_t i = cmap->codeBegin; i <= cmap->codeEnd; i++)
        scanMap->entries.emplace_back(bcfnt::CMAPScan::Entry{i, static_cast<uint16_t>(i - cmap->codeBegin + direct.offset)});

      cmap = cmaps.erase(cmap);
    }
    else
      ++cmap;
  }
  cmaps.push_back({codeBegin, codeEnd, static_cast<uint16_t>(bcfnt::CMAPData::CMAP_TYPE_SCAN), 0, 0, std::move(scanMap)});
}

std::vector<std::uint8_t>& operator<<(std::vector<std::uint8_t> &o, const char *str)
{
  const std::size_t len = std::strlen(str);

  o.reserve(o.size() + len);
  o.insert(o.end(), str, str+len);

  return o;
}

std::vector<std::uint8_t>& operator<<(std::vector<std::uint8_t> &o, std::uint8_t v)
{
  o.emplace_back(v);

  return o;
}

std::vector<std::uint8_t>& operator<<(std::vector<std::uint8_t> &o, std::uint16_t v)
{
  o.reserve(o.size() + 2);
  o.emplace_back((v >> 0) & 0xFF);
  o.emplace_back((v >> 8) & 0xFF);

  return o;
}

std::vector<std::uint8_t>& operator<<(std::vector<std::uint8_t> &o, std::uint32_t v)
{
  o.reserve(o.size() + 4);
  o.emplace_back((v >>  0) & 0xFF);
  o.emplace_back((v >>  8) & 0xFF);
  o.emplace_back((v >> 16) & 0xFF);
  o.emplace_back((v >> 24) & 0xFF);

  return o;
}

std::vector<std::uint8_t>& operator<<(std::vector<std::uint8_t> &o, const bcfnt::CMAPScan& v)
{
  o << static_cast<uint16_t>(v.entries.size());
  for(const auto &entry : v.entries)
  {
    o << static_cast<uint16_t>(entry.code)
      << static_cast<uint16_t>(entry.glyphIndex);
  }

  return o;
}

std::vector<std::uint8_t>& operator<<(std::vector<std::uint8_t> &o, const bcfnt::CMAPTable& v)
{
  for(const auto &entry : v.table)
  {
    o << static_cast<uint16_t>(entry);
  }

  return o;
}
}

namespace bcfnt
{
struct CharMap
{
  CharMap()
  : code (0), faceIndex (0), cfntIndex (0)
  {
  }

  CharMap(FT_ULong code, FT_UInt faceIndex)
  : code (code), faceIndex (faceIndex), cfntIndex (0)
  {
  }

  const FT_ULong code;      ///< Code point.
  const FT_UInt  faceIndex; ///< FreeType face index.
  std::uint16_t  cfntIndex; ///< CFNT glyph index.
};

BCFNT::BCFNT(FT_Face face)
{
  lineFeed = face->size->metrics.height >> 6;
  height = (face->bbox.yMax - face->bbox.yMin) >> 6;
  width = (face->bbox.xMax - face->bbox.xMin) >> 6;
  maxWidth = face->size->metrics.max_advance >> 6;
  ascent = face->size->metrics.ascender >> 6;
  int descent = face->size->metrics.descender >> 6;

  std::map<FT_ULong, CharMap> faceMap;

  {
    // extract mappings from font face
    FT_UInt faceIndex;
    FT_ULong code = FT_Get_First_Char(face, &faceIndex);
    while(faceIndex != 0)
    {
      // only supports 16-bit code points; also 0xFFFF is explicitly a non-character
      if(code >= std::numeric_limits<std::uint16_t>::max())
        continue;

      FT_Error error = FT_Load_Glyph(face, faceIndex, FT_LOAD_RENDER);
      if(error)
      {
        std::fprintf(stderr, "FT_Load_Glyph: %s\n", ft_error(error));
        continue;
      }

      if(face->glyph->bitmap_top > ascent)
        ascent = face->glyph->bitmap_top;

      if(static_cast<int>(face->glyph->bitmap_top) - static_cast<int>(face->glyph->bitmap.rows) < descent)
        descent = face->glyph->bitmap_top - face->glyph->bitmap.rows;

      if(face->glyph->bitmap.width > maxWidth)
        maxWidth = face->glyph->bitmap.width;

      faceMap.emplace(code, CharMap(code, faceIndex));
      code = FT_Get_Next_Char(face, code, &faceIndex);
    }
  }

  cellWidth = maxWidth + 1;
  cellHeight = ascent - descent;
  glyphWidth     = cellWidth + 1;
  glyphHeight    = cellHeight + 1;
  glyphsPerRow   = SHEET_WIDTH / glyphWidth;
  glyphsPerCol   = SHEET_HEIGHT / glyphHeight;
  glyphsPerSheet = glyphsPerRow * glyphsPerCol;

  if(faceMap.empty())
    return;

  {
    // fill in CFNT index
    std::uint16_t cfntIndex = 0;
    for(auto &pair: faceMap)
      pair.second.cfntIndex = cfntIndex++;
  }

  // try to provide a replacement character
  if(faceMap.count(0xFFFD))
    altIndex = faceMap[0xFFFD].cfntIndex;
  else if(faceMap.count('?'))
    altIndex = faceMap['?'].cfntIndex;
  else if(faceMap.count(' '))
    altIndex = faceMap[' '].cfntIndex;
  else
    altIndex = 0;

  // collect character mappings
  for(const auto &pair: faceMap)
  {
    const FT_ULong &code    = pair.first;
    const CharMap  &charMap = pair.second;

    if(code == 0 || cmaps.empty() || cmaps.back().codeEnd != code-1)
    {
      cmaps.emplace_back();
      auto &cmap = cmaps.back();
      cmap.codeBegin = cmap.codeEnd = code;
      cmap.data = future::make_unique<CMAPDirect>(charMap.cfntIndex);
      cmap.mappingMethod = cmap.data->type();
    }
    else
      cmaps.back().codeEnd = code;
  }

  // extract cwdh and sheet data
  std::unique_ptr<Magick::Image> sheet;
  for(const auto &cmap: cmaps)
  {
    for(std::uint16_t code = cmap.codeBegin; code <= cmap.codeEnd; ++code)
    {
      // load glyph and render
      FT_Error error = FT_Load_Glyph(face, faceMap[code].faceIndex, FT_LOAD_RENDER);
      if(error)
      {
        std::fprintf(stderr, "FT_Load_Glyph: %s\n", ft_error(error));
        continue;
      }

      // convert from 26.6 fixed-point format
      const std::int8_t  left       = face->glyph->metrics.horiBearingX >> 6;
      const std::uint8_t glyphWidth = face->glyph->metrics.width >> 6;
      const std::uint8_t charWidth  = face->glyph->metrics.horiAdvance >> 6;

      // add char width info to cwdh
      widths.emplace_back(CharWidthInfo{left, glyphWidth, charWidth});
      if (faceMap[code].cfntIndex == altIndex)
        defaultWidth = CharWidthInfo{left, glyphWidth, charWidth};

      if(faceMap[code].cfntIndex % glyphsPerSheet == 0)
      {
        if(sheet)
        {
          appendSheet(sheetData, *sheet);
          ++numSheets;
        }

        sheet = future::make_unique<Magick::Image>(
          Magick::Geometry(SHEET_WIDTH, SHEET_HEIGHT), transparent());
      }

      assert(sheet);

      const unsigned sheetIndex = faceMap[code].cfntIndex % glyphsPerSheet;
      const unsigned sheetX = (sheetIndex % glyphsPerRow) * this->glyphWidth + 1;
      const unsigned sheetY = (sheetIndex / glyphsPerRow) * this->glyphHeight + 1;

      Pixels cache(*sheet);
      assert(sheetX + cellWidth < sheet->columns());
      assert(sheetY + cellHeight < sheet->rows());
      PixelPacket p = cache.get(sheetX, sheetY, cellWidth, cellHeight);
      for(unsigned y = 0; y < face->glyph->bitmap.rows; ++y)
      {
        for(unsigned x = 0; x < face->glyph->bitmap.width; ++x)
        {
          const int px = x;
          const int py = y + (ascent - face->glyph->bitmap_top);

          if(px < 0 || px >= cellWidth || py < 0 || py >= cellHeight)
            continue;

          const std::uint8_t v = face->glyph->bitmap.buffer[y * face->glyph->bitmap.width + x];

          Magick::Color c;
          quantumRed(c,   bits_to_quantum<8>(0));
          quantumGreen(c, bits_to_quantum<8>(0));
          quantumBlue(c,  bits_to_quantum<8>(0));
          quantumAlpha(c, bits_to_quantum<8>(v));

          p[py*cellWidth + px] = c;
        }
      }
      cache.sync();
    }
  }

  if(sheet)
  {
    appendSheet(sheetData, *sheet);
    ++numSheets;
  }

  coalesceCMAP(cmaps);
}

bool BCFNT::serialize(const std::string &path)
{
  std::vector<std::uint8_t> output;

  std::uint32_t fileSize = 0;
  fileSize += 0x14; // CFNT header

  const std::uint32_t finfOffset = fileSize;
  fileSize += 0x20; // FINF header

  const std::uint32_t tglpOffset = fileSize;
  fileSize += 0x20; // TGLP header

  constexpr std::uint32_t ALIGN = 0x80;
  constexpr std::uint32_t MASK  = ALIGN - 1;
  const std::uint32_t sheetOffset = (fileSize + MASK) & ~MASK;
  fileSize = sheetOffset + sheetData.size();

  // CWDH headers + data
  const std::uint32_t cwdhOffset = fileSize;
  fileSize += 0x10;              // CWDH header
  fileSize += 3 * widths.size(); // CWDH data

  // CMAP headers + data
  std::uint32_t cmapOffset = fileSize;
  for(const auto &cmap: cmaps)
  {
    fileSize += 0x14; // CMAP header

    switch(cmap.mappingMethod)
    {
    case CMAPData::CMAP_TYPE_DIRECT:
      fileSize += 0x2;
      break;

    case CMAPData::CMAP_TYPE_TABLE:
      fileSize += dynamic_cast<const CMAPTable&>(*cmap.data).table.size() * 2;
      break;

    case CMAPData::CMAP_TYPE_SCAN:
      fileSize += 2 + dynamic_cast<const CMAPScan&>(*cmap.data).entries.size() * 4;
      break;

    default:
      abort();
    }
  }

  // FINF, TGLP, CWDH, CMAPs
  std::uint32_t numBlocks = 3 + cmaps.size();

  // CFNT header
  output << "CFNT"                                 // magic
         << static_cast<std::uint16_t>(0xFEFF)     // byte-order-mark
         << static_cast<std::uint16_t>(0x14)       // header size
         << static_cast<std::uint8_t>(0x3)         // version
         << static_cast<std::uint8_t>(0x0)         // version
         << static_cast<std::uint8_t>(0x0)         // version
         << static_cast<std::uint8_t>(0x0)         // version
         << static_cast<std::uint32_t>(fileSize)   // file size
         << static_cast<std::uint32_t>(numBlocks); // number of blocks

  // FINF header
  assert(output.size() == finfOffset);
  output << "FINF"                                   // magic
         << static_cast<std::uint32_t>(0x20)         // section size
         << static_cast<std::uint8_t>(0x1)           // font type
         << static_cast<std::uint8_t>(lineFeed)      // line feed
         << static_cast<std::uint16_t>(altIndex)     // alternate char index
         << static_cast<std::uint8_t>(defaultWidth.left)           // default width (left)
         << static_cast<std::uint8_t>(defaultWidth.glyphWidth)           // default width (glyph width)
         << static_cast<std::uint8_t>(defaultWidth.charWidth)           // default width (char width)
         << static_cast<std::uint8_t>(0x1)           // encoding
         << static_cast<std::uint32_t>(tglpOffset+8) // TGLP offset
         << static_cast<std::uint32_t>(cwdhOffset+8) // CWDH offset
         << static_cast<std::uint32_t>(cmapOffset+8) // CMAP offset
         << static_cast<std::uint8_t>(height)        // font height
         << static_cast<std::uint8_t>(width)         // font width
         << static_cast<std::uint8_t>(ascent)        // font ascent
         << static_cast<std::uint8_t>(0x0);          // padding

  // TGLP header
  const std::uint32_t sheetSize = sheetData.size() / numSheets;
  assert(output.size() == tglpOffset);
  output << "TGLP"                                     // magic
         << static_cast<std::uint32_t>(0x20)           // section size
         << static_cast<std::uint8_t>(cellWidth)      // cell width
         << static_cast<std::uint8_t>(cellHeight)     // cell height
         << static_cast<std::uint8_t>(ascent)          // cell baseline
         << static_cast<std::uint8_t>(maxWidth)        // max character width
         << static_cast<std::uint32_t>(sheetSize)      // sheet data size
         << static_cast<std::uint16_t>(numSheets)      // number of sheets
         << static_cast<std::uint16_t>(0xB)            // 4-bit alpha format
         << static_cast<std::uint16_t>(glyphsPerRow) // num columns
         << static_cast<std::uint16_t>(glyphsPerCol) // num rows
         << static_cast<std::uint16_t>(SHEET_WIDTH)    // sheet width
         << static_cast<std::uint16_t>(SHEET_HEIGHT)   // sheet height
         << static_cast<std::uint32_t>(sheetOffset);   // sheet data offset

  assert(output.size() <= sheetOffset);
  output.resize(sheetOffset);
  assert(output.size() == sheetOffset);
  output.reserve(output.size() + sheetData.size());
  output.insert(output.end(), sheetData.begin(), sheetData.end());

  // CWDH header + data
  assert(output.size() == cwdhOffset);

  output << "CWDH" // magic
         << static_cast<std::uint32_t>(0x10 + 3*widths.size()) // section size
         << static_cast<std::uint16_t>(0)                      // start index
         << static_cast<std::uint16_t>(widths.size())          // end index
         << static_cast<std::uint32_t>(0);                     // next CWDH offset

  for(const auto &info: widths)
  {
    output << static_cast<std::uint8_t>(info.left)
           << static_cast<std::uint8_t>(info.glyphWidth)
           << static_cast<std::uint8_t>(info.charWidth);
  }

  for(const auto &cmap: cmaps)
  {
    assert(output.size() == cmapOffset);

    std::uint32_t size;
    switch (cmap.mappingMethod)
    {
      case bcfnt::CMAPData::CMAP_TYPE_DIRECT:
        size = 0x14 + 0x2;
        break;

      case bcfnt::CMAPData::CMAP_TYPE_TABLE:
        size = 0x14 + dynamic_cast<bcfnt::CMAPTable&>(*cmap.data).table.size() * 2;
        break;

      case bcfnt::CMAPData::CMAP_TYPE_SCAN:
        size = 0x14 + 2 + dynamic_cast<bcfnt::CMAPScan&>(*cmap.data).entries.size() * 4;
        break;

      default:
        abort();
    }

    output << "CMAP"                                         // magic
           << static_cast<std::uint32_t>(size)               // section size
           << static_cast<std::uint16_t>(cmap.codeBegin)     // code begin
           << static_cast<std::uint16_t>(cmap.codeEnd)       // code end
           << static_cast<std::uint16_t>(cmap.mappingMethod) // mapping method
           << static_cast<std::uint16_t>(0x0);               // padding

    // next CMAP offset
    if(&cmap == &cmaps.back())
      output << static_cast<std::uint32_t>(0);
    else
      output << static_cast<std::uint32_t>(cmapOffset + size + 8);

    switch(cmap.mappingMethod)
    {
    case CMAPData::CMAP_TYPE_DIRECT:
    {
      const auto &direct = dynamic_cast<const CMAPDirect&>(*cmap.data);
      output << static_cast<std::uint16_t>(direct.offset);
      break;
    }

    case CMAPData::CMAP_TYPE_TABLE:
    {
      const auto &table = dynamic_cast<const CMAPTable&>(*cmap.data);
      output << table;
      break;
    }

    case CMAPData::CMAP_TYPE_SCAN:
    {
      const auto &scan = dynamic_cast<const CMAPScan&>(*cmap.data);
      output << scan;
      break;
    }

    default:
      abort();
    }

    cmapOffset += size;
  }

  assert(output.size() == fileSize);

  FILE *fp = std::fopen(path.c_str(), "wb");
  if(!fp)
    return false;

  std::size_t offset = 0;
  while(offset < output.size())
  {
    std::size_t rc = std::fwrite(&output[offset], 1, output.size() - offset, fp);
    if(rc != output.size() - offset)
    {
      if(rc == 0 || std::ferror(fp))
      {
        if(std::ferror(fp))
          std::fprintf(stderr, "fwrite: %s\n", std::strerror(errno));
        else
          std::fprintf(stderr, "fwrite: Unknown write failure\n");

        std::fclose(fp);
        return false;
      }
    }

    offset += rc;
  }

  if(std::fclose(fp) != 0)
  {
     std::fprintf(stderr, "fclose: %s\n", std::strerror(errno));
     return false;
  }

  std::printf("Generated font with %zu glyphs\n", widths.size());
  return true;
}
}
