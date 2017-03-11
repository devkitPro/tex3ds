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
/** @file lzss.c
 *  @brief LZSS/LZ10/LZ11 compression routines
 */

/** @brief Enable internal compression routines */
#define COMPRESSION_INTERNAL
#include "compress.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief LZSS/LZ10 maximum match length */
#define LZ10_MAX_LEN  18

/** @brief LZSS/LZ10 maximum displacement */
#define LZ10_MAX_DISP 4096

/** @brief LZ11 maximum match length */
#define LZ11_MAX_LEN  65808

/** @brief LZ11 maximum displacement */
#define LZ11_MAX_DISP 4096

/** @brief LZ compression mode */
typedef enum
{
  LZ10, ///< LZSS/LZ10 compression
  LZ11, ///< LZ11 compression
} lzss_mode_t;

/** @brief Find best buffer match
 *  @param[in]  start     Input buffer
 *  @param[in]  buffer    Encoding buffer
 *  @param[in]  len       Length of encoding buffer
 *  @param[in]  max_disp  Maximum displacement
 *  @param[out] outlen    Length of match
 *  @returns Best match
 *  @retval NULL no match
 */
const uint8_t*
find_best_match(const uint8_t *start,
                const uint8_t *buffer,
                size_t        len,
                size_t        max_disp,
                size_t        *outlen)
{
  const uint8_t *best_start = NULL;
  size_t        best_len    = 0;

  // clamp start to maximum displacement from buffer
  if(start + max_disp < buffer)
    start = buffer - max_disp;

  // find nearest matching start byte
  uint8_t *p = memrchr(start, *buffer, buffer - start);
  while(p != NULL)
  {
    // find length of match
    size_t test_len = 1;
    for(size_t i = 1; i < len; ++i)
    {
      if(p[i] == buffer[i])
        ++test_len;
      else
        break;
    }

    if(test_len >= best_len)
    {
      // this match is the best so far, so save it
      best_start = p;
      best_len   = test_len;
    }

    // if we maximized the match, stop here
    if(best_len == len)
      break;

    // find next nearest matching byte and try again
    p = memrchr(start, *buffer, p - start);
  }

  if(best_len)
  {
    // we found a match, so return it
    *outlen = best_len;
    return best_start;
  }

  // no match found
  *outlen = 0;
  return NULL;
}

/** @brief LZSS/LZ10/LZ11 compression
 *  @param[in]  buffer Source buffer
 *  @param[in]  len    Source length
 *  @param[out] outlen Output length
 *  @param[in]  mode   LZ mode
 *  @returns Compressed buffer
 *  @note Caller must `free()` the output buffer
 */
static void*
lzss_common_encode(const uint8_t *buffer,
                   size_t        len,
                   size_t        *outlen,
                   lzss_mode_t   mode)
{
  buffer_t      result;
  const uint8_t *start = buffer;
#ifndef NDEBUG
  const uint8_t *end   = buffer + len;
#endif
  size_t        shift = 7, code_pos;
  uint8_t       header[COMPRESSION_HEADER_SIZE];

  // get maximum match length
  const size_t max_len  = mode == LZ10 ? LZ10_MAX_LEN  : LZ11_MAX_LEN;

  // get maximum displacement
  const size_t max_disp = mode == LZ10 ? LZ10_MAX_DISP : LZ11_MAX_DISP;

  assert(mode == LZ10 || mode == LZ11);

  // fill compression header
  if(mode == LZ10)
    compression_header(header, 0x10, len);
  else
    compression_header(header, 0x11, len);

  // initialize output buffer
  buffer_init(&result);

  // append compression header to output buffer
  if(buffer_push(&result, header, sizeof(header)) != 0)
  {
    buffer_destroy(&result);
    return NULL;
  }

  // reserve an encode byte in output buffer
  code_pos = result.len;
  if(buffer_push(&result, NULL, 1) != 0)
  {
    buffer_destroy(&result);
    return NULL;
  }

  // set encode byte to 0
  result.data[code_pos] = 0;

  // encode every byte
  while(len > 0)
  {
    assert(buffer < end);
    assert(buffer + len == end);

    const uint8_t *tmp;
    size_t        tmplen;

    if(buffer != start)
    {
      // find best match
      if(len < max_len)
        tmp = find_best_match(start, buffer, len, max_disp, &tmplen);
      else
        tmp = find_best_match(start, buffer, max_len, max_disp, &tmplen);

      if(tmp != NULL)
      {
        assert(tmp >= start);
        assert(tmp < buffer);
        assert(buffer - tmp <= max_disp);
        assert(tmplen <= max_len);
        assert(tmplen <= len);
        assert(memcmp(buffer, tmp, tmplen) == 0);
      }
    }
    else
    {
      // beginning of stream must be primed with at least one value
      tmplen = 1;
    }

    if(tmplen > 2 && tmplen < len)
    {
      // this match is long enough to be compressed; let's check if it's
      // cheaper to encode this byte as a copy and start compression at the
      // next byte
      size_t skip_len, next_len;

      // get best match starting at the next byte
      if(len+1 < max_len)
        find_best_match(start, buffer+1, len-1, max_disp, &skip_len);
      else
        find_best_match(start, buffer+1, max_len, max_disp, &skip_len);

      // check if the match is too small to compress
      if(skip_len < 3)
        skip_len = 1;

      // get best match for data following the current compressed chunk
      if(len+tmplen < max_len)
        find_best_match(start, buffer+tmplen, len-tmplen, max_disp, &next_len);
      else
        find_best_match(start, buffer+tmplen, max_len, max_disp, &next_len);

      // check if the match is too small to compress
      if(next_len < 3)
        next_len = 1;

      // if compressing this chunk and the next chunk is less valuable than
      // skipping this byte and starting compression at the next byte, mark
      // this byte as being needed to copy
      if(tmplen + next_len <= skip_len + 1)
        tmplen = 1;
    }

    if(tmplen < 3)
    {
      // this is a copy chunk; append this byte to the output buffer
      if(buffer_push(&result, buffer, 1) != 0)
      {
        buffer_destroy(&result);
        return NULL;
      }

      // only one byte is copied
      tmplen = 1;
    }
    else if(mode == LZ10)
    {
      // this is a compressed chunk in LZSS/LZ10 mode; reserve two bytes in the
      // output buffer
      if(buffer_push(&result, NULL, 2) != 0)
      {
        buffer_destroy(&result);
        return NULL;
      }

      // mark this chunk as compressed
      result.data[code_pos] |= (1 << shift);

      // encode the displacement and length
      size_t disp = buffer - tmp - 1;
      result.data[result.len-2] = ((tmplen-3) << 4) | (disp >> 8);
      result.data[result.len-1] = disp;
    }
    else if(tmplen <= 0x10)
    {
      // this is a compressed chunk in LZ11 mode; reserve two bytes in the
      // output buffer
      if(buffer_push(&result, NULL, 2) != 0)
      {
        buffer_destroy(&result);
        return NULL;
      }

      // mark this chunk as compressed
      result.data[code_pos] |= (1 << shift);

      // encode the displacement and length
      size_t disp = buffer - tmp - 1;
      result.data[result.len-2] = ((tmplen-0x1) << 4) | (disp >> 8);
      result.data[result.len-1] = disp;
    }
    else if(tmplen <= 0x110)
    {
      // this is a compressed chunk in LZ11 mode; reserve three bytes in
      // the output buffer
      if(buffer_push(&result, NULL, 3) != 0)
      {
        buffer_destroy(&result);
        return NULL;
      }

      // mark this chunk as compressed
      result.data[code_pos] |= (1 << shift);

      // encode the displacement and length
      size_t disp = buffer - tmp - 1;
      result.data[result.len-3] = (tmplen-0x11) >> 4;
      result.data[result.len-2] = ((tmplen-0x11) << 4) | (disp >> 8);
      result.data[result.len-1] = disp;
    }
    else
    {
      // this is a compressed chunk in LZ11 mode; reserve four bytes in
      // the output buffer
      if(buffer_push(&result, NULL, 4) != 0)
      {
        buffer_destroy(&result);
        return NULL;
      }

      // mark this chunk as compressed
      result.data[code_pos] |= (1 << shift);

      // encode the displacement and length
      size_t disp = buffer - tmp - 1;
      result.data[result.len-4] = (1 << 4) | (tmplen-0x111) >> 12;
      result.data[result.len-3] = (tmplen-0x111) >> 4;
      result.data[result.len-2] = ((tmplen-0x111) << 4) | (disp >> 8);
      result.data[result.len-1] = disp;
    }

    // advance input buffer
    buffer += tmplen;
    len    -= tmplen;

    if(shift == 0 && len != 0)
    {
      // we need to encode more data, so reserve a new code byte
      shift = 8;
      if(buffer_push(&result, NULL, 1) != 0)
      {
        buffer_destroy(&result);
        return NULL;
      }

      // update code byte position and clear its byte
      code_pos = result.len - 1;
      result.data[code_pos] = 0;
    }

    // advance code byte bit position
    if(shift != 0)
      --shift;
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

void*
lzss_encode(const void *src,
            size_t     len,
            size_t     *outlen)
{
  return lzss_common_encode(src, len, outlen, LZ10);
}

void*
lz11_encode(const void *src,
            size_t     len,
            size_t     *outlen)
{
  return lzss_common_encode(src, len, outlen, LZ11);
}

void lzss_decode(const void *source, void *dest, size_t size)
{
  const uint8_t *src = (const uint8_t*)source;
  uint8_t       *dst = (uint8_t*)dest;
  uint8_t flags = 0;
  uint8_t mask  = 0;
  unsigned int  len;
  unsigned int  disp;

  while(size > 0)
  {
    if(mask == 0)
    {
      // read in the flags data
      // from bit 7 to bit 0:
      //     0: raw byte
      //     1: compressed block
      flags = *src++;
      mask  = 0x80;
    }

    if(flags & mask) // compressed block
    {
      // disp: displacement
      // len:  length
      len  = (((*src)&0xF0)>>4)+3;
      disp = ((*src++)&0x0F);
      disp = disp<<8 | (*src++);

      if(len > size)
        len = size;

      size -= len;

      // for len, copy data from the displacement
      // to the current buffer position
      for(uint8_t *p = dst-disp-1; len > 0; --len)
        *dst++ = *p++;
    }
    else { // uncompressed block
      // copy a raw byte from the input to the output
      *dst++ = *src++;
      size--;
    }

    mask >>= 1;
  }
}

void
lz11_decode(const void *source, void *dest, size_t size)
{
  const uint8_t *src = (const uint8_t*)source;
  uint8_t       *dst = (uint8_t*)dest;
  int           i;
  uint8_t       flags;

  while(size > 0)
  {
    // read in the flags data
    // from bit 7 to bit 0, following blocks:
    //     0: raw byte
    //     1: compressed block
    flags = *src++;
    for(i = 0; i < 8 && size > 0; i++, flags <<= 1)
    {
      if(flags&0x80) // compressed block
      {
        size_t len;  // length
        size_t disp; // displacement
        switch((*src)>>4)
        {
          case 0: // extended block
            len   = (*src++)<<4;
            len  |= ((*src)>>4);
            len  += 0x11;
            break;
          case 1: // extra extended block
            len   = ((*src++)&0x0F)<<12;
            len  |= (*src++)<<4;
            len  |= ((*src)>>4);
            len  += 0x111;
            break;
          default: // normal block
            len   = ((*src)>>4)+1;
            break;
        }

        disp  = ((*src++)&0x0F)<<8;
        disp |= *src++;

        if(len > size)
          len = size;

        size -= len;

        // for len, copy data from the displacement
        // to the current buffer position
        for(uint8_t *p = dst-disp-1; len > 0; --len)
          *dst++ = *p++;
      }

      else { // uncompressed block
        // copy a raw byte from the input to the output
        *dst++ = *src++;
        --size;
      }
    }
  }
}
