/*------------------------------------------------------------------------------
 * Copyright (c) 2017
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
/** @file tex3ds.h
 *  @brief tex3ds import/export support
 */
#pragma once
#include <stdint.h>

/** @brief Sub-Texture
 *  @note If top > bottom, the sub-texture is rotated 1/4 revolution counter-clockwise
 */
typedef struct Tex3DS_SubTexture
{
  uint16_t width;   ///< Sub-texture width (pixels)
  uint16_t height;  ///< Sub-texture height (pixels)
  float    left;    ///< Left u-coordinate
  float    top;     ///< Top v-coordinate
  float    right;   ///< Right u-coordinate
  float    bottom;  ///< Bottom v-coordinate
} Tex3DS_SubTexture;

/** @brief Data header
 */
typedef struct Tex3DS_Header
{
  uint16_t          width;          ///< Texture width
  uint16_t          height;         ///< Texture height
  uint8_t           format;         ///< Texture format
  uint8_t           mipmapLevels;   ///< Number of mipmaps
  uint16_t          numSubTextures; ///< Number of sub-textures
  Tex3DS_SubTexture subTextures[];  ///< Sub-textures
} Tex3DS_Header;

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Import Tex3DS texture
 *  @param[in] input Input data
 *  @returns Tex3DS texture
 */
Tex3DS_Header* Tex3DS_Import(const void *input);

/** @brief Get pointer to texture data
 *  @param[in] header Tex3DS header
 *  @returns Pointer to texture data
 */
void* Tex3DS_TextureData(const Tex3DS_Header *header);

/** @brief Export Tex3DS texture
 *  @param[in] header Tex3DS header
 *  @returns Export buffer
 */
void* Tex3DS_Export(const Tex3DS_Header *header);

#ifdef __cplusplus
}
#endif
