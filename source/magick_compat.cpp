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
#include "magick_compat.h"

#if MagickLibVersion >= 0x700
/*------------------------------------------------------------------------------
 * PixelPacket::Reference
 *----------------------------------------------------------------------------*/
PixelPacket::Reference::Reference(const Pixels *cache, Magick::Quantum *pixel)
: cache(cache),
  pixel(pixel)
{ }

PixelPacket::Reference::Reference(const PixelPacket::Reference &other)
: cache(other.cache),
  pixel(other.pixel)
{ }

PixelPacket::Reference& PixelPacket::Reference::operator=(const PixelPacket::Reference &other)
{
  if(&other != this)
  {
    pixel[cache->red]   = other.pixel[other.cache->red];
    pixel[cache->green] = other.pixel[other.cache->green];
    pixel[cache->blue]  = other.pixel[other.cache->blue];
    pixel[cache->alpha] = other.pixel[other.cache->alpha];
  }

  return *this;
}

#if __cplusplus >= 201103L
PixelPacket::Reference::Reference(PixelPacket::Reference &&other)
: cache(other.cache),
  pixel(other.pixel)
{ }

PixelPacket::Reference& PixelPacket::Reference::operator=(PixelPacket::Reference &&other)
{
  pixel[cache->red]   = other.pixel[other.cache->red];
  pixel[cache->green] = other.pixel[other.cache->green];
  pixel[cache->blue]  = other.pixel[other.cache->blue];
  pixel[cache->alpha] = other.pixel[other.cache->alpha];

  return *this;
}
#endif

PixelPacket::Reference& PixelPacket::Reference::operator=(const Magick::Color &c)
{
  pixel[cache->red]   = quantumRed(c);
  pixel[cache->green] = quantumGreen(c);
  pixel[cache->blue]  = quantumBlue(c);
  pixel[cache->alpha] = quantumAlpha(c);

  return *this;
}

PixelPacket::Reference::operator Magick::Color() const
{
  return Magick::Color(pixel[cache->red],
                       pixel[cache->green],
                       pixel[cache->blue],
                       pixel[cache->alpha]);
}
/*------------------------------------------------------------------------------
 * PixelPacket
 *----------------------------------------------------------------------------*/
PixelPacket::PixelPacket(const Pixels *cache, Magick::Quantum *pixels)
: cache(cache),
  pixels(pixels)
{
}

PixelPacket::PixelPacket(const PixelPacket &other)
: cache(other.cache),
  pixels(other.pixels)
{
}

PixelPacket& PixelPacket::operator=(const PixelPacket &other)
{
  if(&other != this)
  {
    cache  = other.cache;
    pixels = other.pixels;
  }

  return *this;
}

#if __cplusplus >= 201103L
PixelPacket::PixelPacket(PixelPacket &&other)
: cache(other.cache),
  pixels(other.pixels)
{
}

PixelPacket& PixelPacket::operator=(PixelPacket &&other)
{
  cache  = other.cache;
  pixels = other.pixels;

  return *this;
}
#endif

PixelPacket::Reference PixelPacket::operator[](size_t index) const
{
  return PixelPacket::Reference(cache, pixels + (index * cache->stride));
}

PixelPacket::Reference PixelPacket::operator*() const
{
  return PixelPacket::Reference(cache, pixels);
}

PixelPacket PixelPacket::operator+(size_t i)
{
  return PixelPacket(cache, pixels + (i * cache->stride));
}

PixelPacket& PixelPacket::operator++()
{
  pixels += cache->stride;
  return *this;
}

PixelPacket PixelPacket::operator++(int)
{
  PixelPacket tmp(*this);
  pixels += cache->stride;
  return tmp;
}
/*------------------------------------------------------------------------------
 * Pixels
 *----------------------------------------------------------------------------*/
Pixels::Pixels(Magick::Image &img)
: img(img),
  cache(img),
  red(cache.offset(Magick::RedPixelChannel)),
  green(cache.offset(Magick::GreenPixelChannel)),
  blue(cache.offset(Magick::BluePixelChannel)),
  alpha(cache.offset(Magick::AlphaPixelChannel)),
  stride(img.channels())
{ }

PixelPacket Pixels::get(ssize_t x, ssize_t y, size_t w, size_t h)
{
  return PixelPacket(this, cache.get(x, y, w, h));
}

void Pixels::sync()
{
  cache.sync();
}
#endif
