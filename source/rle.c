/*------------------------------------------------------------------------------
 * Copyright (c) 2017
 *     Michael Theall (mtheall)
 *
 * This file is part of 3dstex.
 *
 * 3dstex is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * 3dstex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 3dstex.  If not, see <http://www.gnu.org/licenses/>.
 *----------------------------------------------------------------------------*/
#define COMPRESSION_INTERNAL
#include "compress.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define RLE_MAX_RUN  130
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

  buffer_init(&result);

  compression_header(header, 0x30, len);

  if(buffer_push(&result, header, sizeof(header)) != 0)
  {
    buffer_destroy(&result);
    return NULL;
  }

  while(src < end)
  {
    for(run = 1; src+run < end && run < RLE_MAX_RUN; ++run)
    {
      if(src[run] != *src)
        break;
    }

    if(run < 3)
    {
      ++src;
      ++save_len;
    }

    if(save_len == RLE_MAX_COPY || (save_len > 0 && run > 2))
    {
      uint8_t byte = save_len - 1;
      if(buffer_push(&result, &byte, 1) != 0
      || buffer_push(&result, save, save_len) != 0)
      {
        buffer_destroy(&result);
        return NULL;
      }

      save     += save_len;
      save_len = 0;
    }

    if(run > 2)
    {
      uint8_t bytes[2] = { 0x80 | (run - 3), *src, };
      if(buffer_push(&result, bytes, 2) != 0)
      {
        buffer_destroy(&result);
        return NULL;
      }

      src  += run;
      save =  src;
      assert(save_len == 0);
    }
  }

  assert(save + save_len == end);

  if(save_len)
  {
    uint8_t byte = save_len - 1;
    if(buffer_push(&result, &byte, 1) != 0
    || buffer_push(&result, save, save_len) != 0)
    {
      buffer_destroy(&result);
      return NULL;
    }
  }

  if(buffer_pad(&result, 4) != 0)
  {
    buffer_destroy(&result);
    return NULL;
  }

  *outlen = result.len;
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
