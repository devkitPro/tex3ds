#include "atlas.h"
#include "subimage.h"
#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

namespace
{

inline size_t calcPOT(size_t x)
{
  if(x < 8)
    return 8;

  return std::pow(2.0, std::ceil(std::log(static_cast<double>(x)) / std::log(2.0)));
}

typedef std::pair<size_t,size_t> XY;

struct Block
{
  Magick::Image img;
  XY            xy;
  size_t        x, y, w, h;

  Block(const Magick::Image &img, size_t x, size_t y, size_t w, size_t h)
  : img(img), x(x), y(y), w(w), h(h)
  { }

  Block(const Magick::Image &img)
  : img(img), x(0), y(0), w(img.columns()), h(img.rows())
  { }

  SubImage subImage(const Magick::Image &atlas) const
  {
    float left   = static_cast<float>(x) / atlas.columns();
    float top    = 1.0f - (static_cast<float>(y) / atlas.rows());
    float right  = static_cast<float>(x+w) / atlas.columns();
    float bottom = 1.0f - (static_cast<float>(y+h) / atlas.rows());

    if(img.columns() == w && img.rows() == h)
      return SubImage(img.fileName(), left, top, right, bottom);

    // rotated
    return SubImage(img.fileName(), bottom, left, top, right);
  }

  bool operator<(const Block &other) const
  {
    if(x != other.x)
      return x < other.x;
    return y < other.y;
  }

  bool operator<(const XY &xy) const
  {
    if(x != xy.first)
      return x < xy.first;
    return y < xy.second;
  }
};

struct Packer
{
  std::set<Block>    placed;
  std::vector<Block> next;
  std::set<XY>       free;

  size_t width, height;

  Packer(std::vector<Magick::Image> images, size_t width, size_t height);

  Magick::Image composite() const
  {
    Magick::Image img(Magick::Geometry(width, height), transparent());

    for(auto &it: placed)
    {
      if(it.img.columns() == it.w && it.img.rows() == it.h)
        img.composite(it.img, Magick::Geometry(0, 0, it.x, it.y), Magick::OverCompositeOp);
      else
      {
        Magick::Image copy = it.img;
        copy.rotate(-90);
        img.composite(copy, Magick::Geometry(0, 0, it.x, it.y), Magick::OverCompositeOp);
      }
    }

    return img;
  }

  void draw() const
  {
    Magick::Image img = composite();
  }

  void   pack(size_t &x, size_t &y, size_t w, size_t h);
  size_t calc_score(size_t x, size_t y, size_t w, size_t h);
  bool solve();

  bool intersects_placed(size_t x, size_t y) const
  {
    for(const auto &it: placed)
    {
      if(x >= it.x && x < it.x + it.w
      && y >= it.y && y < it.y + it.h)
        return true;
    }

    return false;
  }

  bool intersects_placed(const XY &xy) const
  {
    return intersects_placed(xy.first, xy.second);
  }

  void add_free(size_t x, size_t y, bool vert)
  {
    if(x >= width || y >= height)
      return;

    if(intersects_placed(x, y))
      return;

    free.insert(XY(x, y));
  }

  void fixup()
  {
    std::set<XY>::iterator it = free.begin();

    while(it != free.end())
    {
      if(intersects_placed(*it))
        it = free.erase(it);
      else
        ++it;
    }
  }
};

Packer::Packer(std::vector<Magick::Image> images, size_t width, size_t height)
: placed(), next(), free(), width(width), height(height)
{
  next.assign(images.begin(), images.end());
  free.insert(XY(0, 0));
}

bool Packer::solve()
{
  while(!next.empty())
  {
    Block block = next.back();
    next.pop_back();

    std::set<XY>::iterator best         = free.end();
    size_t                 best_score   = 0;
    bool                   best_flipped = false;
    for(std::set<XY>::iterator it = free.begin(); it != free.end(); ++it)
    {
      block.x = it->first;
      block.y = it->second;

      size_t score = calc_score(it->first, it->second, block.w, block.h);
      if(score > best_score)
      {
        best         = it;
        best_score   = score;
        best_flipped = false;
      }

      if(block.w != block.h)
      {
        size_t score = calc_score(it->first, it->second, block.h, block.w);
        if(score > best_score)
        {
          best         = it;
          best_score   = score;
          best_flipped = true;
        }
      }
    }

    if(best_score == 0)
      return false;

    block.x = best->first;
    block.y = best->second;
    if(best_flipped)
      std::swap(block.w, block.h);

    pack(block.x, block.y, block.w, block.h);
    placed.insert(block);
    free.erase(best);

    add_free(block.x + block.w, block.y, true);
    add_free(block.x, block.y + block.h, false);

    fixup();

    draw();
  }

  return true;
}

void Packer::pack(size_t &x, size_t &y, size_t w, size_t h)
{
  bool intersects_left = (x == 0) || intersects_placed(x-1, y);
  bool intersects_up   = (y == 0) || intersects_placed(x, y-1);

  if(!intersects_left && !intersects_up)
    std::abort(); // should be adjacent to a placed block

  if(intersects_left && intersects_up)
    return;

  if(intersects_left)
  {
    // move up as far as possible
    --y;
    while(y > 0 && !intersects_placed(x, y-1))
      --y;
  }
  else
  {
    // move left as far as possible
    --x;
    while(x > 0 && !intersects_placed(x-1, y))
      --x;
  }
}

size_t Packer::calc_score(size_t x, size_t y, size_t w, size_t h)
{
  size_t score = 0;

  pack(x, y, w, h);

  if(x + w > width)
    return 0;
  if(y + h > height)
    return 0;

  for(std::set<Block>::iterator it = placed.begin(); it != placed.end(); ++it)
  {
    if(x+w < it->x)
      break;

    if(x < it->x + it->w
    && x + w > it->x
    && y < it->y + it->h
    && y + h > it->y)
      return 0;

    if(x == it->x + it->w
    || x + w == it->x)
    {
      size_t start = std::max(y, it->y);
      size_t end   = std::min(y + h, it->y + it->h);
      if(end > start)
        score += end - start;
    }

    if(y == it->y + it->h
    || y + h == it->y)
    {
      size_t start = std::max(x, it->x);
      size_t end   = std::min(x + w, it->x + it->w);
      if(end > start)
        score += end - start;
    }
  }

  if(x == 0)
    score += h;
  if(x + w == width)
    score += h;

  if(y == 0)
    score += w;
  if(y + h == height)
    score += w;

  return score;
}

struct AreaSizeComparator
{
  bool compare(size_t w1, size_t h1, size_t w2, size_t h2) const
  {
    size_t area1 = w1 * h1;
    size_t area2 = w2 * h2;

    if(area1 != area2)
      return area1 < area2;

    if(std::max(w1, h1) == std::max(w2, h2))
      return w1 < w2;

    return std::max(w1, h1) < std::max(w2, h2);
  }

  bool operator()(const Magick::Image &lhs, const Magick::Image &rhs) const
  {
    return compare(lhs.columns(), lhs.rows(), rhs.columns(), rhs.rows());
  }

  bool operator()(const Packer &lhs, const Packer &rhs) const
  {
    return compare(lhs.width, lhs.height, rhs.width, rhs.height);
  }
};

}

Atlas Atlas::build(const std::vector<std::string> &paths)
{
  std::vector<Magick::Image> images;

  for(size_t i = 0; i < paths.size(); ++i)
    images.push_back(Magick::Image(paths[i]));

  std::sort(images.begin(), images.end(), AreaSizeComparator());

  size_t totalArea = 0;
  for(std::vector<Magick::Image>::iterator it = images.begin(); it != images.end(); ++it)
    totalArea += it->rows() * it->columns();

  std::vector<Packer> packers;
  for(size_t h = calcPOT(std::min(images.back().columns(), images.back().rows())); h <= 1024; h *= 2)
  {
    for(size_t w = calcPOT(std::min(images.back().columns(), images.back().rows())); w <= 1024; w *= 2)
    {
      if(w*h >= totalArea)
        packers.push_back(Packer(images, w, h));
    }
  }

  std::sort(packers.begin(), packers.end(), AreaSizeComparator());

  for(size_t i = 0; i < packers.size(); ++i)
  {
    if(packers[i].solve())
    {
      Atlas atlas;

      atlas.img = packers[i].composite();
      for(auto &it: packers[i].placed)
        atlas.subs.push_back(it.subImage(atlas.img));

      atlas.img.write("/tmp/atlas.png");
      return atlas;
    }
  }

  throw std::runtime_error("No atlas solution found.\n");
}
