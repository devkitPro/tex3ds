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
/** @file rle.c
 *  @brief Run-length encoding compression routines
 */

/** @brief Enable internal compression routines */
#define COMPRESSION_INTERNAL
#include "compress.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** @brief Minimum run length */
#define RLE_MIN_RUN  3

/** @brief Maximum run length */
#define RLE_MAX_RUN  130

/** @brief Maximum copy length */
#define RLE_MAX_COPY 128

void*
rle_encode(const void *source,
           size_t     len,
           size_t     *outlen)
{
  const uint8_t *src = (const uint8_t*)source;
  const uint8_t *save = src, *end = src + len;
  buffer_t      result;
  size_t        save_len = 0, run;
  uint8_t       header[4];

  // initialize output buffer
  buffer_init(&result);

  // fill compression header
  compression_header(header, 0x30, len);

  // append compression header to output data
  if(buffer_push(&result, header, sizeof(header)) != 0)
  {
    buffer_destroy(&result);
    return NULL;
  }

  // encode all bytes
  while(src < end)
  {
    // calculate current run
    for(run = 1; src+run < end && run < RLE_MAX_RUN; ++run)
    {
      if(src[run] != *src)
        break;
    }

    if(run < RLE_MIN_RUN)
    {
      // run not long enough to encode
      ++src;
      ++save_len;
    }

    // check if we need to encode a copy
    if(save_len == RLE_MAX_COPY || (save_len > 0 && run > 2))
    {
      // encode copy length
      uint8_t byte = save_len - 1;

      // append copy length followed by copy buffer
      if(buffer_push(&result, &byte, 1) != 0
      || buffer_push(&result, save, save_len) != 0)
      {
        buffer_destroy(&result);
        return NULL;
      }

      // reset save point
      save     += save_len;
      save_len = 0;
    }

    // check if run is long enough to encode
    if(run > 2)
    {
      // encode run
      uint8_t bytes[2] = { 0x80 | (run - 3), *src, };

      // append run to output buffer
      if(buffer_push(&result, bytes, 2) != 0)
      {
        buffer_destroy(&result);
        return NULL;
      }

      // reset save point
      src  += run;
      save =  src;
      assert(save_len == 0);
    }
  }

  assert(save + save_len == end);

  // check if there is data left to copy
  if(save_len)
  {
    // encode copy length
    uint8_t byte = save_len - 1;

    // append copy length followed by copy buffer
    if(buffer_push(&result, &byte, 1) != 0
    || buffer_push(&result, save, save_len) != 0)
    {
      buffer_destroy(&result);
      return NULL;
    }
  }

  // pad the output buffer to 4 bytes
  if(buffer_pad(&result, 4) != 0)
  {
    buffer_destroy(&result);
    return NULL;
  }

  // set the output length
  *outlen = result.len;

  // return the output data
  return result.data;
}

void
rle_decode(const void *source,
           void       *dest,
           size_t     size)
{
  const uint8_t *src = (const uint8_t*)source;
  uint8_t       *dst = (uint8_t*)dest;
  uint8_t       byte;
  size_t        len;

  while(size > 0)
  {
    // read in the data header
    byte = *src++;

    if(byte & 0x80) // compressed block
    {
      // read the length of the run
      len = (byte & 0x7F) + 3;

      if(len > size)
        len = size;

      size -= len;

      // read in the byte used for the run
      byte = *src++;

      // for len, copy byte into output
      memset(dst, byte, len);
      dst += len;
    }
    else // uncompressed block
    {
      // read the length of uncompressed bytes
      len = (byte & 0x7F) + 1;

      if(len > size)
        len = size;

      size -= len;

      // for len, copy from input to output
      memcpy(dst, src, len);
      dst += len;
      src += len;
    }
  }
}
