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
/** @file bcfnt.h
 *  @brief BCFNT definitions
 */
#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <memory>
#include <vector>

namespace bcfnt
{
/** @brief Character width information. */
struct CharWidthInfo
{
  std::int8_t left;        ///< Horizontal offset to draw the glyph with.
  std::uint8_t glyphWidth; ///< Width of the glyph.
  std::uint8_t charWidth;  ///< Width of the character, that is, horizontal distance to advance.
};

class CMAPData
{
public:
  /** @brief Font character map methods. */
  enum Type : std::uint8_t
  {
    CMAP_TYPE_DIRECT = 0, ///< Identity mapping.
    CMAP_TYPE_TABLE  = 1, ///< Mapping using a table.
    CMAP_TYPE_SCAN   = 2, ///< Mapping using a list of mapped characters.
  };

  virtual Type type () const = 0;
};

class CMAPDirect : public CMAPData
{
public:
  CMAPDirect (std::uint16_t offset) : offset (offset)
  {
  }

  Type type () const override
  {
    return CMAP_TYPE_DIRECT;
  }

  const std::uint16_t offset;
};

class CMAPTable : public CMAPData
{
public:
  Type type () const override
  {
    return CMAP_TYPE_TABLE;
  }

  std::vector<std::uint16_t> table;
};

class CMAPScan : public CMAPData
{
public:
  Type type () const override
  {
    return CMAP_TYPE_SCAN;
  }

  struct Entry
  {
    std::uint16_t code;
    std::uint16_t glyphIndex;
  };

  std::vector<Entry> entries;
};

/** @brief Font character map structure. */
struct CMAP
{
  std::uint16_t codeBegin;     ///< First Unicode codepoint the block applies to.
  std::uint16_t codeEnd;       ///< Last Unicode codepoint the block applies to.
  std::uint16_t mappingMethod; ///< Mapping method.
  std::uint16_t reserved;
  std::uint32_t next;          ///< Pointer to the next map.

  std::unique_ptr<CMAPData> data; ///< Character map data.
};

class BCFNT
{
public:
  BCFNT(FT_Face face);

  bool serialize(const std::string &path);

private:
  std::vector<CMAP> cmaps;
  std::vector<CharWidthInfo> widths;
  std::vector<std::uint8_t> sheetData;

  std::size_t numSheets;
  std::uint16_t altIndex;
  std::uint8_t lineFeed;
  std::uint8_t height;
  std::uint8_t width;
  std::uint8_t maxWidth;
  std::uint8_t ascent;
  
  /* todo: make configurable */
  int cellWidth;
  int cellHeight;
  static constexpr int SHEET_WIDTH  = 256;
  static constexpr int SHEET_HEIGHT = 512;

  /* DO NOT EDIT */
  int glyphWidth;
  int glyphHeight;
  int glyphsPerRow;
  int glyphsPerCol;
  int glyphsPerSheet;
};
}
