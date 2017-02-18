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
#pragma once

#include "compat.h"
#ifdef __cplusplus
extern "C"
{
#else
#include <stddef.h>
#include <stdint.h>
#endif

void* lzss_encode(const void *src, size_t len, size_t *outlen);
void  lzss_decode(const void *src, void *dst, size_t len);

void* lz11_encode(const void *src, size_t len, size_t *outlen);
void  lz11_decode(const void *src, void *dst, size_t len);

void* rle_encode(const void *src, size_t len, size_t *outlen);
void  rle_decode(const void *src, void *dst, size_t len);

void* huff_encode(const void *src, size_t len, size_t *outlen);
void  huff_decode(const void *src, void *dst, size_t len);

static inline void
compression_header(uint8_t header[4], uint8_t type, size_t size)
{
  header[0] = type;
  header[1] = size;
  header[2] = size >> 8;
  header[3] = size >> 16;
}

#ifdef COMPRESSION_INTERNAL
#include <stdlib.h>
#include <string.h>

typedef struct
{
  uint8_t *data;
  size_t  len;
  size_t  limit;
} buffer_t;

static inline void
buffer_init(buffer_t *buffer)
{
  buffer->data  = NULL;
  buffer->len   = 0;
  buffer->limit = 0;
}

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

static inline void
buffer_destroy(buffer_t *buffer)
{
  free(buffer->data);
}
#endif

#ifdef __cplusplus
}
#endif
