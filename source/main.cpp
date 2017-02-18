#include "compat.h"
#include "thread.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <queue>
#include <stdexcept>
#include <vector>
#include <Magick++.h>
#include <getopt.h>
#include "compress.h"
#include "rg_etc1.h"

typedef std::vector<uint8_t> Buffer;

namespace
{

std::string output_path;
std::string preview_path;

enum OutputFormat
{
  RGBA8888 = '0',
  RGB888   = '1',
  RGBA5551 = '2',
  RGB565   = '3',
  RGBA4444 = '4',
  LA88     = '5',
  HILO88   = '6',
  L8       = '7',
  A8       = '8',
  LA44     = '9',
  L4       = 'a',
  A4       = 'b',
  ETC1     = 'c',
  ETC1A4   = 'd',
} process_format = RGBA8888;

enum CompressionFormat
{
  COMPRESSION_NONE,
  COMPRESSION_FAKE,
  COMPRESSION_LZ10,
  COMPRESSION_LZ11,
  COMPRESSION_RLE,
  COMPRESSION_HUFF,
} compression_format = COMPRESSION_NONE;

Magick::FilterTypes filter_type = Magick::UndefinedFilter;

Magick::Image load_image(const char *path)
{
  Magick::Image img(path);
  img.comment(path);

  switch(img.columns())
  {
    case    8: case   16: case   32: case   64:
    case  128: case  256: case  512: case 1024:
      break;

    default:
      std::fprintf(stderr, "%s: invalid width '%zu'\n",
                   path, img.columns());
  }

  switch(img.rows())
  {
    case    8: case   16: case   32: case   64:
    case  128: case  256: case  512: case 1024:
      break;

    default:
      std::fprintf(stderr, "%s: invalid height '%zu'\n",
                   path, img.rows());
  }

  img.page(Magick::Geometry(img.columns(), img.rows()));

  return img;
}

void swizzle(Magick::PixelPacket *p)
{
  static const unsigned char table[][4] =
  {
    {  2,  8, 16,  4, },
    {  3,  9, 17,  5, },
    {  6, 10, 24, 20, },
    {  7, 11, 25, 21, },
    { 14, 26, 28, 22, },
    { 15, 27, 29, 23, },
    { 34, 40, 48, 36, },
    { 35, 41, 49, 37, },
    { 38, 42, 56, 52, },
    { 39, 43, 57, 53, },
    { 46, 58, 60, 54, },
    { 47, 59, 61, 55, },
  };

  for(size_t i = 0; i < sizeof(table)/sizeof(table[0]); ++i)
  {
    Magick::PixelPacket tmp = p[table[i][0]];
    p[table[i][0]]          = p[table[i][1]];
    p[table[i][1]]          = p[table[i][2]];
    p[table[i][2]]          = p[table[i][3]];
    p[table[i][3]]          = tmp;
  }

  std::swap(p[12], p[18]);
  std::swap(p[13], p[19]);
  std::swap(p[44], p[50]);
  std::swap(p[45], p[51]);
}

void swizzle(Magick::Image &img)
{
  Magick::Pixels cache(img);

  for(size_t j = 0; j < img.rows(); j += 8)
  {
    for(size_t i = 0; i < img.columns(); i += 8)
    {
      Magick::PixelPacket *p = cache.get(i, j, 8, 8);
      swizzle(p);
    }
  }

  cache.sync();
}

template <int bits>
uint8_t quantum_to_bits(Magick::Quantum v)
{
  using Magick::Quantum;
  return (1<<bits) * v / (QuantumRange+1);
}

template <int bits>
Magick::Quantum bits_to_quantum(uint8_t v)
{
  using Magick::Quantum;
  return v * QuantumRange / ((1<<bits)-1);
}

template <int bits>
Magick::Quantum quantize(Magick::Quantum v)
{
  using Magick::Quantum;
  return quantum_to_bits<bits>(v) * QuantumRange / ((1<<bits)-1);
}

double gamma_inverse(double v)
{
  if(v <= 0.04045)
    return v / 12.92;
  return std::pow((v + 0.055) / 1.055, 2.4);
}

double gamma(double v)
{
  if(v <= 0.0031308)
    return v * 12.92;
  return 1.055 * std::pow(v, 1.0/2.4) - 0.055;
}

Magick::Quantum luminance(const Magick::Color &c)
{
  const double r = 0.212655;
  const double g = 0.715158;
  const double b = 0.072187;

  using Magick::Quantum;

  double v = gamma(r * gamma_inverse(static_cast<double>(c.redQuantum())   / QuantumRange)
                 + g * gamma_inverse(static_cast<double>(c.greenQuantum()) / QuantumRange)
                 + b * gamma_inverse(static_cast<double>(c.blueQuantum())  / QuantumRange));

  return std::max(0.0, std::min(1.0, v)) * QuantumRange;
}

Magick::Quantum alpha(const Magick::Color &c)
{
  using Magick::Quantum;
  return QuantumRange - c.alphaQuantum();
}

struct WorkUnit
{
  Buffer              result;
  uint64_t            sequence;
  Magick::PixelPacket *p;
  size_t              stride;
  bool                output;
  bool                preview;
  void (*process)(WorkUnit&);

  bool operator<(const WorkUnit &other) const
  {
    /* greater-than for min-heap */
    return sequence > other.sequence;
  }
};

void process_rgba8888(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c = work.p[j*work.stride + i];

      if(work.output)
      {
        work.result.push_back(quantum_to_bits<8>(alpha(c)));
        work.result.push_back(quantum_to_bits<8>(c.blueQuantum()));
        work.result.push_back(quantum_to_bits<8>(c.greenQuantum()));
        work.result.push_back(quantum_to_bits<8>(c.redQuantum()));
      }

      if(work.preview)
      {
        c.redQuantum(quantize<8>(c.redQuantum()));
        c.greenQuantum(quantize<8>(c.greenQuantum()));
        c.blueQuantum(quantize<8>(c.blueQuantum()));
        c.alphaQuantum(quantize<8>(c.alphaQuantum()));

        work.p[j*work.stride + i] = c;
      }
    }
  }
}

void process_rgb888(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c = work.p[j*work.stride + i];

      if(work.output)
      {
        work.result.push_back(quantum_to_bits<8>(c.blueQuantum()));
        work.result.push_back(quantum_to_bits<8>(c.greenQuantum()));
        work.result.push_back(quantum_to_bits<8>(c.redQuantum()));
      }

      if(work.preview)
      {
        c.redQuantum(quantize<8>(c.redQuantum()));
        c.greenQuantum(quantize<8>(c.greenQuantum()));
        c.blueQuantum(quantize<8>(c.blueQuantum()));
        c.alphaQuantum(0);

        work.p[j*work.stride + i] = c;
      }
    }
  }
}

void process_rgba5551(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c = work.p[j*work.stride + i];

      if(work.output)
      {
        unsigned int v = (quantum_to_bits<5>(c.redQuantum())   << 11)
                       | (quantum_to_bits<5>(c.greenQuantum()) <<  6)
                       | (quantum_to_bits<5>(c.blueQuantum())  <<  1)
                       | (quantum_to_bits<1>(alpha(c))         <<  0);

        work.result.push_back(v >> 0);
        work.result.push_back(v >> 8);
      }

      if(work.preview)
      {
        c.redQuantum(quantize<5>(c.redQuantum()));
        c.greenQuantum(quantize<5>(c.greenQuantum()));
        c.blueQuantum(quantize<5>(c.blueQuantum()));
        c.alphaQuantum(quantize<1>(c.alphaQuantum()));

        work.p[j*work.stride + i] = c;
      }
    }
  }
}

void process_rgb565(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c = work.p[j*work.stride + i];

      if(work.output)
      {
        unsigned int v = (quantum_to_bits<5>(c.redQuantum())   << 11)
                       | (quantum_to_bits<6>(c.greenQuantum()) <<  5)
                       | (quantum_to_bits<5>(c.blueQuantum())  <<  0);

        work.result.push_back(v >> 0);
        work.result.push_back(v >> 8);
      }

      if(work.preview)
      {
        c.redQuantum(quantize<5>(c.redQuantum()));
        c.greenQuantum(quantize<6>(c.greenQuantum()));
        c.blueQuantum(quantize<5>(c.blueQuantum()));
        c.alphaQuantum(0);

        work.p[j*work.stride + i] = c;
      }
    }
  }
}

void process_rgba4444(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c = work.p[j*work.stride + i];

      if(work.output)
      {
        unsigned int v = (quantum_to_bits<4>(alpha(c))         <<  0)
                       | (quantum_to_bits<4>(c.blueQuantum())  <<  4)
                       | (quantum_to_bits<4>(c.greenQuantum()) <<  8)
                       | (quantum_to_bits<4>(c.redQuantum())   << 12);

        work.result.push_back(v >> 0);
        work.result.push_back(v >> 8);
      }

      if(work.preview)
      {
        c.redQuantum(quantize<4>(c.redQuantum()));
        c.greenQuantum(quantize<4>(c.greenQuantum()));
        c.blueQuantum(quantize<4>(c.blueQuantum()));
        c.alphaQuantum(quantize<4>(c.alphaQuantum()));

        work.p[j*work.stride + i] = c;
      }
    }
  }
}

void process_la88(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c = work.p[j*work.stride + i];

      if(work.output)
      {
        work.result.push_back(quantum_to_bits<8>(alpha(c)));
        work.result.push_back(quantum_to_bits<8>(luminance(c)));
      }

      if(work.preview)
      {
        Magick::Quantum l = quantize<8>(luminance(c));

        c.redQuantum(l);
        c.greenQuantum(l);
        c.blueQuantum(l);
        c.alphaQuantum(quantize<8>(c.alphaQuantum()));

        work.p[j*work.stride + i] = c;
      }
    }
  }
}

void process_hilo88(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c = work.p[j*work.stride + i];

      if(work.output)
      {
        work.result.push_back(quantum_to_bits<8>(c.greenQuantum()));
        work.result.push_back(quantum_to_bits<8>(c.redQuantum()));
      }

      if(work.preview)
      {
        c.redQuantum(quantize<8>(c.redQuantum()));
        c.greenQuantum(quantize<8>(c.greenQuantum()));
        c.blueQuantum(0);
        c.alphaQuantum(0);

        work.p[j*work.stride + i] = c;
      }
    }
  }
}

void process_l8(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c = work.p[j*work.stride + i];

      if(work.output)
        work.result.push_back(quantum_to_bits<8>(luminance(c)));

      if(work.preview)
      {
        Magick::Quantum l = quantize<8>(luminance(c));

        c.redQuantum(l);
        c.greenQuantum(l);
        c.blueQuantum(l);
        c.alphaQuantum(0);

        work.p[j*work.stride + i] = c;
      }
    }
  }
}

void process_a8(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c = work.p[j*work.stride + i];

      if(work.output)
        work.result.push_back(quantum_to_bits<8>(alpha(c)));

      if(work.preview)
      {
        c.redQuantum(0);
        c.greenQuantum(0);
        c.blueQuantum(0);
        c.alphaQuantum(quantize<8>(c.alphaQuantum()));

        work.p[j*work.stride + i] = c;
      }
    }
  }
}

void process_la44(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c = work.p[j*work.stride + i];

      if(work.output)
      {
        work.result.push_back((quantum_to_bits<4>(luminance(c)) << 4)
                            | (quantum_to_bits<4>(alpha(c))     << 0));
      }

      if(work.preview)
      {
        Magick::Quantum l = quantize<4>(luminance(c));

        c.redQuantum(l);
        c.greenQuantum(l);
        c.blueQuantum(l);
        c.alphaQuantum(quantize<4>(c.alphaQuantum()));

        work.p[j*work.stride + i] = c;
      }
    }
  }
}

void process_l4(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c1 = work.p[j*work.stride + i+0],
                    c2 = work.p[j*work.stride + i+1];

      if(work.output)
      {
        work.result.push_back((quantum_to_bits<4>(luminance(c1)) << 0)
                            | (quantum_to_bits<4>(luminance(c2)) << 4));
      }

      if(work.preview)
      {
        Magick::Quantum l = quantize<4>(luminance(c1));

        c1.redQuantum(l);
        c1.greenQuantum(l);
        c1.blueQuantum(l);
        c1.alphaQuantum(0);

        l = quantize<4>(luminance(c2));

        c2.redQuantum(l);
        c2.greenQuantum(l);
        c2.blueQuantum(l);
        c2.alphaQuantum(0);

        work.p[j*work.stride + i+0] = c1;
        work.p[j*work.stride + i+1] = c2;
      }
    }
  }
}

void process_a4(WorkUnit &work)
{
  for(size_t j = 0; j < 8; ++j)
  {
    for(size_t i = 0; i < 8; ++i)
    {
      Magick::Color c1 = work.p[j*work.stride + i+0],
                    c2 = work.p[j*work.stride + i+1];

      if(work.output)
      {
        work.result.push_back((quantum_to_bits<8>(alpha(c1)) << 0)
                            | (quantum_to_bits<8>(alpha(c2)) << 4));
      }

      if(work.preview)
      {
        c1.redQuantum(0);
        c1.greenQuantum(0);
        c1.blueQuantum(0);
        c1.alphaQuantum(quantize<4>(c1.alphaQuantum()));

        c2.redQuantum(0);
        c2.greenQuantum(0);
        c2.blueQuantum(0);
        c2.alphaQuantum(quantize<4>(c2.alphaQuantum()));

        work.p[j*work.stride + i+0] = c1;
        work.p[j*work.stride + i+1] = c2;
      }
    }
  }
}

void process_etc1(WorkUnit &work)
{
  rg_etc1::etc1_pack_params params;
  params.clear();

  for(size_t j = 0; j < 8; j += 4)
  {
    for(size_t i = 0; i < 8; i += 4)
    {
      uint8_t in_block[4*4*4];
      uint8_t out_block[8];

      if(work.output || work.preview)
      {
        for(size_t y = 0; y < 4; ++y)
        {
          for(size_t x = 0; x < 4; ++x)
          {
            Magick::Color c = work.p[(j+y)*work.stride + i + x];

            in_block[y*16 + x*4 + 0] = quantum_to_bits<8>(c.redQuantum());
            in_block[y*16 + x*4 + 1] = quantum_to_bits<8>(c.greenQuantum());
            in_block[y*16 + x*4 + 2] = quantum_to_bits<8>(c.blueQuantum());
            in_block[y*16 + x*4 + 3] = 0xFF;
          }
        }

        rg_etc1::pack_etc1_block(out_block, reinterpret_cast<unsigned int*>(in_block), params);
      }

      if(work.output)
      {
        for(size_t i = 0; i < 8; ++i)
          work.result.push_back(out_block[8-i-1]);
      }

      if(work.preview)
      {
        rg_etc1::unpack_etc1_block(out_block, reinterpret_cast<unsigned int*>(in_block));

        for(size_t y = 0; y < 4; ++y)
        {
          for(size_t x = 0; x < 4; ++x)
          {
            Magick::Color c;

            c.redQuantum(bits_to_quantum<8>(in_block[y*16 + x*4 + 0]));
            c.greenQuantum(bits_to_quantum<8>(in_block[y*16 + x*4 + 1]));
            c.blueQuantum(bits_to_quantum<8>(in_block[y*16 + x*4 + 2]));
            c.alphaQuantum(0);

            work.p[(j+y)*work.stride + i + x] = c;
          }
        }
      }
    }
  }
}

void process_etc1a4(WorkUnit &work)
{
  rg_etc1::etc1_pack_params params;
  params.clear();

  for(size_t j = 0; j < 8; j += 4)
  {
    for(size_t i = 0; i < 8; i += 4)
    {
      uint8_t in_block[4*4*4];
      uint8_t out_block[8];
      uint8_t out_alpha[8] = {0,0,0,0,0,0,0,0};

      if(work.output || work.preview)
      {
        for(size_t y = 0; y < 4; ++y)
        {
          for(size_t x = 0; x < 4; ++x)
          {
            Magick::Color c = work.p[(j+y)*work.stride + i + x];

            in_block[y*16 + x*4 + 0] = quantum_to_bits<8>(c.redQuantum());
            in_block[y*16 + x*4 + 1] = quantum_to_bits<8>(c.greenQuantum());
            in_block[y*16 + x*4 + 2] = quantum_to_bits<8>(c.blueQuantum());
            in_block[y*16 + x*4 + 3] = 0xFF;

            if(work.output)
            {
              if(y & 1)
                out_alpha[2*x + y/2] |= (quantum_to_bits<4>(alpha(c)) << 4);
              else
                out_alpha[2*x + y/2] |= quantum_to_bits<4>(alpha(c));
            }
          }
        }

        rg_etc1::pack_etc1_block(out_block, reinterpret_cast<unsigned int*>(in_block), params);
      }

      if(work.output)
      {
        for(size_t i = 0; i < 8; ++i)
          work.result.push_back(out_alpha[i]);

        for(size_t i = 0; i < 8; ++i)
          work.result.push_back(out_block[8-i-1]);
      }

      if(work.preview)
      {
        rg_etc1::unpack_etc1_block(out_block, reinterpret_cast<unsigned int*>(in_block));

        for(size_t y = 0; y < 4; ++y)
        {
          for(size_t x = 0; x < 4; ++x)
          {
            Magick::Color c = work.p[(j+y)*work.stride + i + x];

            c.redQuantum(bits_to_quantum<8>(in_block[y*16 + x*4 + 0]));
            c.greenQuantum(bits_to_quantum<8>(in_block[y*16 + x*4 + 1]));
            c.blueQuantum(bits_to_quantum<8>(in_block[y*16 + x*4 + 2]));
            c.alphaQuantum(quantize<4>(c.alphaQuantum()));

            work.p[(j+y)*work.stride + i + x] = c;
          }
        }
      }
    }
  }
}

std::queue<WorkUnit>          work_queue;
std::priority_queue<WorkUnit> result_queue;

std::condition_variable work_cond;
std::condition_variable result_cond;
std::mutex              work_mutex;
std::mutex              result_mutex;
bool                    work_done = false;

THREAD_RETURN_T work_thread(void *param)
{
  std::unique_lock<std::mutex> mutex(work_mutex);
  while(true)
  {
    while(!work_done && work_queue.empty())
      work_cond.wait(mutex);

    if(work_done && work_queue.empty())
      THREAD_EXIT;

    WorkUnit work = work_queue.front();
    work_queue.pop();
    mutex.unlock();

    work.process(work);

    result_mutex.lock();
    result_queue.push(work);
    result_cond.notify_one();
    result_mutex.unlock();

    mutex.lock();
  }
}

void process_image(Magick::Image img)
{
  void (*process)(WorkUnit&);
  void* (*compress)(const void*,size_t,size_t*);

  switch(process_format)
  {
    case RGBA8888:
      process = process_rgba8888;
      break;

    case RGB888:
      process = process_rgb888;
      break;

    case RGBA5551:
      process = process_rgba5551;
      break;

    case RGB565:
      process = process_rgb565;
      break;

    case RGBA4444:
      process = process_rgba4444;
      break;

    case LA88:
      process = process_la88;
      break;

    case HILO88:
      process = process_hilo88;
      break;

    case L8:
      process = process_l8;
      break;

    case A8:
      process = process_a8;
      break;

    case LA44:
      process = process_la44;
      break;

    case L4:
      process = process_l4;
      break;

    case A4:
      process = process_a4;
      break;

    case ETC1:
      rg_etc1::pack_etc1_block_init();
      process = process_etc1;
      break;

    case ETC1A4:
      rg_etc1::pack_etc1_block_init();
      process = process_etc1a4;
      break;
  }

  switch(compression_format)
  {
    case COMPRESSION_NONE:
    case COMPRESSION_FAKE:
      compress = nullptr;
      break;

    case COMPRESSION_LZ10:
      compress = lzss_encode;
      break;

    case COMPRESSION_LZ11:
      compress = lz11_encode;
      break;

    case COMPRESSION_RLE:
      compress = rle_encode;
      break;

    case COMPRESSION_HUFF:
      compress = huff_encode;
      break;
  }

  std::queue<Magick::Image> img_queue;
  img_queue.push(img);

  using Magick::Quantum;
  static const Magick::Color transparent(0, 0, 0, QuantumRange);
  size_t preview_width = img.columns();
  if(!preview_path.empty())
    preview_width = preview_width * 1.5;
  Magick::Image preview(Magick::Geometry(preview_width, img.rows()), transparent);

  if(filter_type != Magick::UndefinedFilter && img.columns() > 8 && img.rows() > 8)
  {
    size_t width  = img.columns();
    size_t height = img.rows();

    while(width > 8 && height > 8)
    {
      img = img_queue.front();
      img.modifyImage();
      img.filterType(filter_type);

      width  = width / 2;
      height = height / 2;
      img.resize(Magick::Geometry(width, height));
      img_queue.push(img);
    }
  }

  std::vector<std::thread> workers;
  for(size_t i = 0; i < NUM_THREADS; ++i)
    workers.push_back(std::thread(work_thread, nullptr));

  Buffer buf;
  size_t hoff = 0;
  size_t woff = 0;
  while(!img_queue.empty())
  {
    img = img_queue.front();
    img_queue.pop();

    size_t width  = img.columns();
    size_t height = img.rows();

    img.modifyImage();
    swizzle(img);
    Magick::Pixels cache(img);
    Magick::PixelPacket *p = cache.get(0, 0, img.columns(), img.rows());

    uint64_t num_work = 0;
    for(size_t j = 0; j < height; j += 8)
    {
      for(size_t i = 0; i < width; i += 8)
      {
        WorkUnit work;

        work.sequence = num_work++;
        work.p        = p + j*img.columns() + i;
        work.stride   = img.columns();
        work.output   = !output_path.empty();
        work.preview  = !preview_path.empty();
        work.process  = process;

        work_mutex.lock();
        work_queue.push(work);
        work_cond.notify_one();
        work_mutex.unlock();
      }
    }

    if(img_queue.empty())
    {
      work_mutex.lock();
      work_done = true;
      work_cond.notify_all();
      work_mutex.unlock();
    }

    for(uint64_t num_result = 0; num_result < num_work; ++num_result)
    {
      std::unique_lock<std::mutex> mutex(result_mutex);
      while(result_queue.empty() || result_queue.top().sequence != num_result)
        result_cond.wait(mutex);

      WorkUnit work = result_queue.top();
      result_queue.pop();
      mutex.unlock();

      buf.insert(buf.end(), work.result.begin(), work.result.end());

      mutex.lock();
    }

    cache.sync();

    if(!preview_path.empty())
    {
      preview.composite(img, Magick::Geometry(0, 0, woff, hoff), Magick::OverCompositeOp);
      hoff += img.rows();
      if(woff == 0)
      {
        hoff = 0;
        woff = img.columns();
      }
    }
  }

  if(!preview_path.empty())
    preview.write(preview_path);

  while(!workers.empty())
  {
    workers.back().join();
    workers.pop_back();
  }

  if(compression_format != COMPRESSION_NONE && buf.size() > 0xFFFFFF)
    std::fprintf(stderr, "Warning: output size exceeds compression header limit\n");

  FILE *fp = std::fopen(output_path.c_str(), "wb");

  if(compression_format == COMPRESSION_FAKE)
  {
    uint8_t header[4];
    compression_header(header, 0x00, buf.size());
    std::fwrite(header, 1, sizeof(header), fp);
  }

  size_t  outlen  = buf.size();
  uint8_t *buffer = &buf[0];
  if(compress)
  {
    buffer = reinterpret_cast<uint8_t*>(compress(&buf[0], buf.size(), &outlen));
    if(!buffer)
    {
      std::fclose(fp);
      throw std::runtime_error("Failed to compress data");
    }
  }

  size_t  pos = 0;
  while(pos < outlen)
  {
    ssize_t rc = std::fwrite(buffer+pos, 1, outlen-pos, fp);
    if(rc <= 0)
    {
      std::fclose(fp);
      throw std::runtime_error("Failed to output data");
    }

    pos += rc;
  }

  std::fclose(fp);

  if(buffer != &buf[0])
    std::free(buffer);
}

void print_usage(const char *prog)
{
  std::printf("Usage: %s [<format>] [-m <filter>] [-o <output>] [-p <preview>] [-z <compression>] <input>\n", prog);
  std::printf(
    "    <format>         See \"Format options\"\n"
    "    -m <filter>      Generate mipmaps. See \"Filter options\"\n"
    "    -o <output>      Output file\n"
    "    -p <preview>     Output preview file\n"
    "    -z <compression> Compress output. See \"Compression options\"\n"
    "    <input>          Input file\n\n"

    "  Format options:\n"
    "    -0, --rgba, --rgba8, --rgba8888\n"
    "      32-bit RGBA (8-bit components) (default)\n\n"

    "    -1, --rgb, --rgb8, --rgb888\n"
    "      24-bit RGB (8-bit components)\n\n"

    "    -2, --rgba5551\n"
    "      16-bit RGBA (5-bit RGB, 1-bit Alpha)\n\n"

    "    -3, --rgb565\n"
    "      16-bit RGB (5-bit Red/Blue, 6-bit Green)\n\n"

    "    -4, --rgba4, --rgba444\n"
    "      16-bit RGBA (4-bit components)\n\n"

    "    -5, --la, --la8, --la88\n"
    "      16-bit Luminance/Alpha (8-bit components)\n\n"

    "    -6, --hilo, --hilo8, --hilo88\n"
    "      16-bit HILO (8-bit components)\n"
    "      Note: HI comes from Red channel, LO comes from Green channel\n\n"

    "    -7, --l, --l8\n"
    "      8-bit Luminance\n\n"

    "    -8, --a, --a8\n"
    "      8-bit Alpha\n\n"

    "    -9, --la4, --la44\n"
    "      8-bit Luminance/Alpha (4-bit components)\n\n"

    "    -a, --l4\n"
    "      4-bit Luminance\n\n"

    "    -b, --a4\n"
    "      4-bit Alpha\n\n"

    "    -c, --etc1\n"
    "      ETC1\n\n"

    "    -d, --etc1a4\n"
    "      ETC1 with 4-bit Alpha\n\n"

    "  Filter options:\n"
    "    -m bessel     Bessel filter\n"
    "    -m blackman   Blackman filter\n"
    "    -m box        Box filter\n"
    "    -m catrom     Catrom filter\n"
    "    -m cubic      Cubic filter\n"
    "    -m gaussian   Gaussian filter\n"
    "    -m hamming    Hamming filter\n"
    "    -m hanning    Hanning filter\n"
    "    -m hermite    Hermite filter\n"
    "    -m lanczos    Lanczos filter\n"
    "    -m mitchell   Mitchell filter\n"
    "    -m point      Point filter\n"
    "    -m quadratic  Quadratic filter\n"
    "    -m sinc       Sinc filter\n"
    "    -m triangle   Triangle filter\n\n"

    "  Compression options:\n"
    "    -z none              No compression (default)\n"
    "    -z fake              Fake compression header\n"
    "    -z huff, -z huffman  Huffman encoding (unsupported, possible to produce garbage)\n"
    "    -z lzss, -z lz10     LZSS compression\n"
    "    -z lz11              LZ11 compression\n"
    "    -z rle               Run-length encoding\n\n"

    "    NOTE: All compression types (except 'none') use a GBA-style compression header: a single byte which denotes the compression type, followed by three bytes (little-endian) which specify the size of the uncompressed data.\n"
    "    Types:\n"
    "      0x00: Fake (uncompressed)\n"
    "      0x10: LZSS\n"
    "      0x11: LZ11\n"
    "      0x28: Huffman encoding\n"
    "      0x30: Run-length encoding\n"
  );
}

const struct option long_options[] =
{
  { "rgba",     no_argument,       nullptr, '0', },
  { "rgba8",    no_argument,       nullptr, '0', },
  { "rgba8888", no_argument,       nullptr, '0', },
  { "rgb",      no_argument,       nullptr, '1', },
  { "rgb8",     no_argument,       nullptr, '1', },
  { "rgb888",   no_argument,       nullptr, '1', },
  { "rgba5551", no_argument,       nullptr, '2', },
  { "rgb565",   no_argument,       nullptr, '3', },
  { "rgba4",    no_argument,       nullptr, '4', },
  { "rgba4444", no_argument,       nullptr, '4', },
  { "la",       no_argument,       nullptr, '5', },
  { "la8",      no_argument,       nullptr, '5', },
  { "la88",     no_argument,       nullptr, '5', },
  { "hilo",     no_argument,       nullptr, '6', },
  { "hilo8",    no_argument,       nullptr, '6', },
  { "hilo88",   no_argument,       nullptr, '6', },
  { "l",        no_argument,       nullptr, '7', },
  { "l8",       no_argument,       nullptr, '7', },
  { "a",        no_argument,       nullptr, '8', },
  { "a8",       no_argument,       nullptr, '8', },
  { "la4",      no_argument,       nullptr, '9', },
  { "la44",     no_argument,       nullptr, '9', },
  { "l4",       no_argument,       nullptr, 'a', },
  { "a4",       no_argument,       nullptr, 'b', },
  { "etc1",     no_argument,       nullptr, 'c', },
  { "etc1a4",   no_argument,       nullptr, 'd', },
  { "output",   required_argument, nullptr, 'o', },
  { "compress", required_argument, nullptr, 'z', },
  { nullptr,    no_argument,       nullptr,   0, },
};

}

int main(int argc, char *argv[])
{
  const char *prog = argv[0];

  int c;

  while((c = ::getopt_long(argc, argv, "0123456789abcdhm:o:p:s:z:ABCD", long_options, nullptr)) != -1)
  {
    switch(c)
    {
      case 'A' ... 'D':
        c = (c - 'A') + 'a';
      case '0' ... '9':
      case 'a' ... 'd':
        process_format = static_cast<OutputFormat>(c);
        break;

      case 'h':
        print_usage(prog);
        return EXIT_SUCCESS;

      case 'm':
        if(strcasecmp(optarg, "bessel") == 0)
          filter_type = Magick::BesselFilter;
        else if(strcasecmp(optarg, "blackman") == 0)
          filter_type = Magick::BlackmanFilter;
        else if(strcasecmp(optarg, "box") == 0)
          filter_type = Magick::BoxFilter;
        else if(strcasecmp(optarg, "catrom") == 0)
          filter_type = Magick::CatromFilter;
        else if(strcasecmp(optarg, "cubic") == 0)
          filter_type = Magick::CubicFilter;
        else if(strcasecmp(optarg, "gaussian") == 0)
          filter_type = Magick::GaussianFilter;
        else if(strcasecmp(optarg, "hamming") == 0)
          filter_type = Magick::HammingFilter;
        else if(strcasecmp(optarg, "hanning") == 0)
          filter_type = Magick::HanningFilter;
        else if(strcasecmp(optarg, "hermite") == 0)
          filter_type = Magick::HermiteFilter;
        else if(strcasecmp(optarg, "lanczos") == 0)
          filter_type = Magick::LanczosFilter;
        else if(strcasecmp(optarg, "mitchell") == 0)
          filter_type = Magick::MitchellFilter;
        else if(strcasecmp(optarg, "point") == 0)
          filter_type = Magick::PointFilter;
        else if(strcasecmp(optarg, "quadratic") == 0)
          filter_type = Magick::QuadraticFilter;
        else if(strcasecmp(optarg, "sinc") == 0)
          filter_type = Magick::SincFilter;
        else if(strcasecmp(optarg, "triangle") == 0)
          filter_type = Magick::TriangleFilter;
        else
        {
          std::fprintf(stderr, "Invalid mipmap filter option '%s'\n", optarg);
          return EXIT_FAILURE;
        }
        break;

      case 'o':
        output_path = optarg;
        break;

      case 'p':
        preview_path = optarg;
        break;

      case 'z':
        if(strcasecmp(optarg, "none") == 0)
          compression_format = COMPRESSION_NONE;
        else if(strcasecmp(optarg, "fake") == 0)
          compression_format = COMPRESSION_FAKE;
        else if(strcasecmp(optarg, "huff") == 0 || strcasecmp(optarg, "huffman") == 0)
          compression_format = COMPRESSION_HUFF;
        else if(strcasecmp(optarg, "lzss") == 0 || strcasecmp(optarg, "lz10") == 0)
          compression_format = COMPRESSION_LZ10;
        else if(strcasecmp(optarg, "lz11") == 0)
          compression_format = COMPRESSION_LZ11;
        else if(strcasecmp(optarg, "rle") == 0)
          compression_format = COMPRESSION_RLE;
        else
        {
          std::fprintf(stderr, "Invalid compression option '%s'\n", optarg);
          return EXIT_FAILURE;
        }
        break;

      default:
        std::fprintf(stderr, "Invalid option '%c'\n", optopt);
        return EXIT_FAILURE;
    }
  }

  if(optind == argc)
  {
    std::fprintf(stderr, "No image provided\n");
    print_usage(prog);
    return EXIT_FAILURE;
  }

  try
  {
    process_image(load_image(argv[optind]));
  }
  catch(const std::exception &e)
  {
    std::fprintf(stderr, "%s: %s\n", argv[optind], e.what());
    return EXIT_FAILURE;
  }
  catch(...)
  {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
