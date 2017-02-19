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
#include <algorithm>
#include <Magick++.h>

// ImageMagick compatibility
#if MagickLibVersion >= 0x700
typedef Magick::FilterType FilterType;

namespace
{

inline Magick::Quantum quantumRed(const Magick::Color &c)
{
  return c.quantumRed();
}

inline void quantumRed(Magick::Color &c, Magick::Quantum v)
{
  c.quantumRed(v);
}

inline Magick::Quantum quantumGreen(const Magick::Color &c)
{
  return c.quantumGreen();
}

inline void quantumGreen(Magick::Color &c, Magick::Quantum v)
{
  c.quantumGreen(v);
}

inline Magick::Quantum quantumBlue(const Magick::Color &c)
{
  return c.quantumBlue();
}

inline void quantumBlue(Magick::Color &c, Magick::Quantum v)
{
  c.quantumBlue(v);
}

inline Magick::Quantum quantumAlpha(const Magick::Color &c)
{
  return c.quantumAlpha();
}

inline void quantumAlpha(Magick::Color &c, Magick::Quantum v)
{
  c.quantumAlpha(v);
}

}

class Pixels;

class PixelPacket
{
private:
  const Pixels    *cache;
  Magick::Quantum *pixels;

public:
  class Reference
  {
  private:
    const Pixels    *cache;
    Magick::Quantum *pixel;

    Reference(const Pixels *cache, Magick::Quantum *pixel);

    friend class PixelPacket;

  public:
    Reference(const Reference &other);
    Reference& operator=(const Reference &other);
#if __cplusplus >= 201103L
    Reference(Reference &&other);
    Reference& operator=(Reference &&other);
#endif

    Reference& operator=(const Magick::Color &c);
    operator Magick::Color() const;
  };

  PixelPacket(const Pixels *cache, Magick::Quantum *pixels);
  PixelPacket(const PixelPacket &other);
  PixelPacket& operator=(const PixelPacket &other);
#if __cplusplus >= 201103L
  PixelPacket(PixelPacket &&other);
  PixelPacket& operator=(PixelPacket &&other);
#endif

  Reference    operator[](size_t index) const;
  Reference    operator*() const;
  PixelPacket  operator+(size_t i);
  PixelPacket& operator++();
  PixelPacket  operator++(int);
};

class Pixels
{
private:
  Magick::Image  &img;
  Magick::Pixels cache;
  const ssize_t  red;
  const ssize_t  green;
  const ssize_t  blue;
  const ssize_t  alpha;
  const size_t   stride;

  friend class PixelPacket;
  friend class PixelPacket::Reference;

public:
  Pixels(Magick::Image &img);

  PixelPacket get(ssize_t x, ssize_t y, size_t w, size_t h);
  void sync();
};

namespace
{

inline void swapPixel(PixelPacket::Reference p1, PixelPacket::Reference p2)
{
  Magick::Color tmp = p1;
  p1 = p2;
  p2 = tmp;
}

inline Magick::Color transparent()
{
  return Magick::Color(0, 0, 0, 0);
}

inline bool has_rgb(Magick::Image &img)
{
  Magick::Pixels cache(img);

  if(img.hasChannel(Magick::RedPixelChannel)
  && img.hasChannel(Magick::GreenPixelChannel)
  && img.hasChannel(Magick::BluePixelChannel))
    return true;

  assert(cache.offset(Magick::RedPixelChannel) >= 0);
  assert(cache.offset(Magick::GreenPixelChannel) >= 0);
  assert(cache.offset(Magick::BluePixelChannel) >= 0);

  return false;
}

}
#else /* MagickLibVersion < 0x700 */
typedef Magick::FilterTypes FilterType;
typedef Magick::Pixels      Pixels;
typedef Magick::PixelPacket *PixelPacket;

namespace
{

inline Magick::Quantum quantumRed(const Magick::Color &c)
{
  return c.redQuantum();
}

inline void quantumRed(Magick::Color &c, Magick::Quantum v)
{
  c.redQuantum(v);
}

inline Magick::Quantum quantumGreen(const Magick::Color &c)
{
  return c.greenQuantum();
}

inline void quantumGreen(Magick::Color &c, Magick::Quantum v)
{
  c.greenQuantum(v);
}

inline Magick::Quantum quantumBlue(const Magick::Color &c)
{
  return c.blueQuantum();
}

inline void quantumBlue(Magick::Color &c, Magick::Quantum v)
{
  c.blueQuantum(v);
}

inline Magick::Quantum quantumAlpha(const Magick::Color &c)
{
  using Magick::Quantum;
  return QuantumRange - c.alphaQuantum();
}

inline void quantumAlpha(Magick::Color &c, Magick::Quantum v)
{
  using Magick::Quantum;
  c.alphaQuantum(QuantumRange - v);
}

inline void swapPixel(Magick::PixelPacket &p1, Magick::PixelPacket &p2)
{
  std::swap(p1, p2);
}

inline Magick::Color transparent()
{
  using Magick::Quantum;
  return Magick::Color(0, 0, 0, QuantumRange);
}

inline bool has_rgb(Magick::Image &img)
{
  return true;
}

}
#endif
