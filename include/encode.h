#pragma once
#include <cstdint>
#include <vector>
#include <Magick++.h>
#include "rg_etc1.h"

namespace encode
{

typedef std::vector<uint8_t> Buffer;

struct WorkUnit
{
  Buffer                result;
  uint64_t              sequence;
  Magick::PixelPacket   *p;
  size_t                stride;
  rg_etc1::etc1_quality etc1_quality;
  bool                  output;
  bool                  preview;
  void                  (*process)(WorkUnit&);

  bool operator<(const WorkUnit &other) const
  {
    /* greater-than for min-heap */
    return sequence > other.sequence;
  }
};


void rgba8888(WorkUnit &work);
void rgb888(WorkUnit &work);
void rgb565(WorkUnit &work);
void rgba5551(WorkUnit &work);
void rgba4444(WorkUnit &work);
void la88(WorkUnit &work);
void hilo88(WorkUnit &work);
void l8(WorkUnit &work);
void a8(WorkUnit &work);
void la44(WorkUnit &work);
void l4(WorkUnit &work);
void a4(WorkUnit &work);
void etc1(WorkUnit &work);
void etc1a4(WorkUnit &work);

}
