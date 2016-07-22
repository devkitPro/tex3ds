#include "etc1.h"
#include <Magick++.h>

#if 0
namespace
{

static const short table[][4] =
{
  {  2,   8,  -2,   -8, },
  {  5,  17,  -5,  -17, },
  {  9,  29,  -9,  -29, },
  { 13,  42, -13,  -42, },
  { 18,  60, -18,  -60, },
  { 24,  80, -24,  -80, },
  { 33, 106, -33, -106, },
  { 47, 183, -47, -183, },
};

template<class T>
T clamp(T min, T max, T v)
{
  return std::max(min, std::min(max, v));
}

int index(uint64_t v, int cw, int i)
{
  return table[cw][((v >> i) & 1) | ((v >> (i+15)) & 2)];
}

unsigned char extend_4(unsigned int v)
{
  return (v << 4) | v;
}

unsigned char extend_5(unsigned int v)
{
  return (v << 3) | (v >> 2);
}

Magick::Image etc1_decode(uint64_t v)
{
  Magick::Image img(Magick::Geometry(4, 4), Magick::Color(0, 0, 0, 0));

  img.modifyImage();

  Magick::Pixels      cache(img);
  Magick::PixelPacket *p = cache.get(0, 0, 4, 4);

  unsigned int r1, g1, b1, r2, g2, b2;
  if(v & (1ULL << 33))
  {
    static const int signed3bit[] = { 0, 1, 2, 3, -4, -3, -2, -1 };

    unsigned int r = (v >> 59) & 0x1F;
    unsigned int g = (v >> 51) & 0x1F;
    unsigned int b = (v >> 43) & 0x1F;

    int dr = signed3bit[(v >> 56) & 0x7];
    int dg = signed3bit[(v >> 48) & 0x7];
    int db = signed3bit[(v >> 40) & 0x7];

    r1 = extend_5(r);
    g1 = extend_5(g);
    b1 = extend_5(b);

    r2 = extend_5(clamp(0U, 31U, r + dr));
    g2 = extend_5(clamp(0U, 31U, g + dg));
    b2 = extend_5(clamp(0U, 31U, b + db));
  }
  else
  {
    r1 = extend_4((v >> 60) & 0xF);
    g1 = extend_4((v >> 52) & 0xF);
    b1 = extend_4((v >> 44) & 0xF);

    r2 = extend_4((v >> 56) & 0xF);
    g2 = extend_4((v >> 48) & 0xF);
    b2 = extend_4((v >> 40) & 0xF);
  }

  int cw1 = (v >> 37) & 0x7;
  int cw2 = (v >> 34) & 0x7;

  if(v & (1ULL << 32))
  {
    unsigned int r = clamp(0U, 255U, r1 + index(v, cw1, 0));
    unsigned int g = clamp(0U, 255U, g1 + index(v, cw1, 0));
    unsigned int b = clamp(0U, 255U, b1 + index(v, cw1, 0));
  }
  else
  {
  }
}

#if 0
Magick::Color average_color(const Magick::Color block[16], bool flip, bool half)
{
  Magick::Image       img;
  const Magick::Color *c = &block[0];

  if(!flip)
  {
    img.resize(Magick::Geometry(2, 4));
    if(half)
      c = &block[2];
  }
  else
  {
    img.resize(Magick::Geometry(4, 2));
    if(half)
      c = &block[8];
  }

  Magick::Pixels      cache(img);
  Magick::PixelPacket *p = cache.get(0, 0, img.columns(), img.rows());

  for(size_t j = 0; j < img.rows(); ++j)
  {
    for(size_t i = 0; i < img.columns(); ++i)
      p[j*img.columns() + i] = *c++;

    if(flip)
      c += 2;
  }

  cache.sync();

  img.resize(Magick::Geometry(1,1));
  return *img.getConstPixels(0, 0, 1, 1);
}
#endif

}
#endif

uint64_t etc1_block(const Magick::Color block[16])
{
#if 0
  Magick::Color avg[4] =
  {
    average_color(block, false, false),
    average_color(block, false, true),
    average_color(block, true,  false),
    average_color(block, true,  true),
  };
#endif

  return 0;
}
