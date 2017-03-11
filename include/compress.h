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
/** @file compress.h
 *  @brief Compression routines
 */
#pragma once

/** @brief Compression header size */
#define COMPRESSION_HEADER_SIZE 5

#include "compat.h"
#ifdef __cplusplus
extern "C"
{
#else
#include <stddef.h>
#include <stdint.h>
#endif

/** @brief LZSS/LZ10 compression
 *  @param[in]  src    Source buffer
 *  @param[in]  len    Source length
 *  @param[out] outlen Output length
 *  @returns Compressed buffer
 *  @note Caller must `free()` the output buffer
 */
void* lzss_encode(const void *src, size_t len, size_t *outlen);

/** @brief LZSS/LZ10 decompression
 *  @param[in]  src Source buffer
 *  @param[out] dst Destination buffer
 *  @param[in]  len Source length
 *  @note The output buffer must be large enough to hold the decompressed data
 */
void  lzss_decode(const void *src, void *dst, size_t len);

/** @brief LZ11 compression
 *  @param[in]  src    Source buffer
 *  @param[in]  len    Source length
 *  @param[out] outlen Output length
 *  @returns Compressed buffer
 *  @note Caller must `free()` the output buffer
 */
void* lz11_encode(const void *src, size_t len, size_t *outlen);

/** @brief LZ11 decompression
 *  @param[in]  src Source buffer
 *  @param[out] dst Destination buffer
 *  @param[in]  len Source length
 *  @note The output buffer must be large enough to hold the decompressed data
 */
void  lz11_decode(const void *src, void *dst, size_t len);

/** @brief Run-length encoding compression
 *  @param[in]  src    Source buffer
 *  @param[in]  len    Source length
 *  @param[out] outlen Output length
 *  @returns Compressed buffer
 *  @note Caller must `free()` the output buffer
 */
void* rle_encode(const void *src, size_t len, size_t *outlen);

/** @brief Run-length encoding decompression
 *  @param[in]  src Source buffer
 *  @param[out] dst Destination buffer
 *  @param[in]  len Source length
 *  @note The output buffer must be large enough to hold the decompressed data
 */
void  rle_decode(const void *src, void *dst, size_t len);

/** @brief Huffman compression
 *  @param[in]  src    Source buffer
 *  @param[in]  len    Source length
 *  @param[out] outlen Output length
 *  @returns Compressed buffer
 *  @note Caller must `free()` the output buffer
 */
void* huff_encode(const void *src, size_t len, size_t *outlen);

/** @brief Huffman decompression
 *  @param[in]  src Source buffer
 *  @param[out] dst Destination buffer
 *  @param[in]  len Source length
 *  @note The output buffer must be large enough to hold the decompressed data
 */
void  huff_decode(const void *src, void *dst, size_t len);

/** @brief Output a GBA-style compression header
 *  @param[out] header Output header
 *  @param[in]  type   Compression type
 *  @param[in]  size   Uncompressed data size
 */
static inline void
compression_header(uint8_t header[COMPRESSION_HEADER_SIZE], uint8_t type, size_t size)
{
  header[0] = type;
  header[1] = size;
  header[2] = size >> 8;
  header[3] = size >> 16;
  header[4] = size >> 24;
}

#ifdef COMPRESSION_INTERNAL
#include <stdlib.h>
#include <string.h>

/** @brief Output buffer */
typedef struct
{
  uint8_t *data; ///< Output data
  size_t  len;   ///< Data length
  size_t  limit; ///< Maximum data length
} buffer_t;

/** @brief Initialize buffer_t
 *  @param[out] buffer Output buffer
 */
static inline void
buffer_init(buffer_t *buffer)
{
  buffer->data  = NULL;
  buffer->len   = 0;
  buffer->limit = 0;
}

/** @brief Append to a buffer
 *  @param[in,out] buffer Output buffer
 *  @param[in]     data   Data to append
 *  @param[in]     len    Length of data to append
 *  @retval 0  success
 *  @retval -1 failure
 */
static inline int
buffer_push(buffer_t *buffer, const uint8_t *data, size_t len)
{
  if(len + buffer->len > buffer->limit)
  {
    size_t limit = (len + buffer->len + 0x0FFF) & ~0x0FFF;
    uint8_t *tmp = realloc(buffer->data, limit);
    if(tmp)
    {
      buffer->limit = limit;
      buffer->data  = tmp;
    }
    else
      return -1;
  }

  if(data != NULL)
    memcpy(buffer->data + buffer->len, data, len);
  else
    memset(buffer->data + buffer->len, 0, len);

  buffer->len += len;
  return 0;
}

/** @brief Pad an output buffer
 *  @param[in,out] buffer  Output buffer to pad
 *  @param[in]     padding Alignment to pad
 *  @retval 0  success
 *  @retval -1 failure
 */
static inline int
buffer_pad(buffer_t *buffer, size_t padding)
{
  if(buffer->len % padding != 0)
  {
    size_t len = padding - (buffer->len % padding);
    return buffer_push(buffer, NULL, len);
  }

  return 0;
}

/** @brief Destroy an output buffer
 *  @param[in] buffer Output buffer to destroy
 */
static inline void
buffer_destroy(buffer_t *buffer)
{
  free(buffer->data);
}
#endif

#ifdef __cplusplus
}
#endif
