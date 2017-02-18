#include "compat.h"
#include "encode.h"
#include "quantum.h"
#include "thread.h"
#include <algorithm>
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

#define ARRAY_COUNT(x) (sizeof(x)/sizeof(x[0]))

namespace
{

std::string output_path;
std::string preview_path;

enum ProcessFormat
{
  RGBA8888,
  RGB888,
  RGBA5551,
  RGB565,
  RGBA4444,
  LA88,
  HILO88,
  L8,
  A8,
  LA44,
  L4,
  A4,
  ETC1,
  ETC1A4,
  AUTO_L8,
  AUTO_L4,
  AUTO_ETC1,
} process_format = RGBA8888;

struct ProcessFormatString
{
  const char    *str;
  ProcessFormat fmt;

  bool operator<(const char *str) const
  {
    return strcasecmp(this->str, str) < 0;
  }
} output_format_strings[] =
{
  { "a",         A8,       },
  { "a4",        A4,       },
  { "a8",        A8,       },
  { "auto-etc1", AUTO_L8,  },
  { "auto-l4",   AUTO_L8,  },
  { "auto-l8",   AUTO_L8,  },
  { "etc1",      ETC1,     },
  { "etc1a4",    ETC1A4,   },
  { "hilo",      HILO88,   },
  { "hilo8",     HILO88,   },
  { "hilo88",    HILO88,   },
  { "l",         L8,       },
  { "l4",        L4,       },
  { "l8",        L8,       },
  { "la",        LA88,     },
  { "la4",       LA44,     },
  { "la44",      LA44,     },
  { "la8",       LA88,     },
  { "la88",      LA88,     },
  { "rgb",       RGB888,   },
  { "rgb565",    RGB565,   },
  { "rgb8",      RGB888,   },
  { "rgb888",    RGB888,   },
  { "rgba",      RGBA8888, },
  { "rgba4",     RGBA4444, },
  { "rgba4444",  RGBA4444, },
  { "rgba5551",  RGBA5551, },
  { "rgba8",     RGBA8888, },
  { "rgba8888",  RGBA8888, },
};
ProcessFormatString *output_format_strings_end =
  output_format_strings + ARRAY_COUNT(output_format_strings);

rg_etc1::etc1_quality etc1_quality = rg_etc1::cMediumQuality;

enum CompressionFormat
{
  COMPRESSION_NONE,
  COMPRESSION_FAKE,
  COMPRESSION_LZ10,
  COMPRESSION_LZ11,
  COMPRESSION_RLE,
  COMPRESSION_HUFF,
} compression_format = COMPRESSION_NONE;

struct CompressionFormatString
{
  const char        *str;
  CompressionFormat fmt;

  bool operator<(const char *str) const
  {
    return strcasecmp(this->str, str) < 0;
  }
} compression_format_strings[] =
{
  { "fake",     COMPRESSION_FAKE, },
  { "huff",     COMPRESSION_HUFF, },
  { "huffman",  COMPRESSION_HUFF, },
  { "lz10",     COMPRESSION_LZ10, },
  { "lz11",     COMPRESSION_LZ11, },
  { "lzss",     COMPRESSION_LZ10, },
  { "none",     COMPRESSION_NONE, },
  { "rle",      COMPRESSION_RLE,  },
};
CompressionFormatString *compression_format_strings_end =
  compression_format_strings + ARRAY_COUNT(compression_format_strings);

struct FilterTypeString
{
  const char          *str;
  Magick::FilterTypes type;

  bool operator<(const char *str) const
  {
    return strcasecmp(this->str, str) < 0;
  }
} filter_type_strings[] =
{
  { "bartlett",       Magick::BartlettFilter,      },
  { "bessel",         Magick::BesselFilter,        },
  { "blackman",       Magick::BlackmanFilter,      },
  { "bohman",         Magick::BohmanFilter,        },
  { "box",            Magick::BoxFilter,           },
  { "catrom",         Magick::CatromFilter,        },
  { "cosine",         Magick::CosineFilter,        },
  { "cubic",          Magick::CubicFilter,         },
  { "gaussian",       Magick::GaussianFilter,      },
  { "hamming",        Magick::HammingFilter,       },
  { "hanning",        Magick::HanningFilter,       },
  { "hermite",        Magick::HermiteFilter,       },
  { "jinc",           Magick::JincFilter,          },
  { "kaiser",         Magick::KaiserFilter,        },
  { "lagrange",       Magick::LagrangeFilter,      },
  { "lanczos",        Magick::LanczosFilter,       },
  { "lanczos-radius", Magick::LanczosRadiusFilter, },
  { "lanczos-sharp",  Magick::LanczosSharpFilter,  },
  { "lanczos2",       Magick::LanczosFilter,       },
  { "lanczos2-sharp", Magick::LanczosSharpFilter,  },
  { "mitchell",       Magick::MitchellFilter,      },
  { "parzen",         Magick::ParzenFilter,        },
  { "point",          Magick::PointFilter,         },
  { "quadratic",      Magick::QuadraticFilter,     },
  { "robidoux",       Magick::RobidouxFilter,      },
  { "robidoux-sharp", Magick::RobidouxSharpFilter, },
  { "sinc",           Magick::SincFilter,          },
  { "spline",         Magick::SplineFilter,        },
  { "triangle",       Magick::TriangleFilter,      },
  { "welsh",          Magick::WelshFilter,         },
};
FilterTypeString *filter_type_strings_end =
  filter_type_strings + ARRAY_COUNT(filter_type_strings);
 
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

  for(size_t i = 0; i < ARRAY_COUNT(table); ++i)
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

std::queue<encode::WorkUnit>          work_queue;
std::priority_queue<encode::WorkUnit> result_queue;

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

    encode::WorkUnit work = work_queue.front();
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

template<int bits>
bool has_alpha(Magick::Image &img)
{
  Magick::Pixels cache(img);
  const Magick::PixelPacket *p = cache.getConst(0, 0, img.columns(), img.rows());

  size_t num = img.rows() * img.columns();
  for(size_t i = 0; i < num; ++i)
  {
    Magick::Color c = *p++;
    if(quantum_to_bits<bits>(c.alphaQuantum()))
      return true;
  }

  return false;
}

void process_image(Magick::Image img)
{
  void (*process)(encode::WorkUnit&);
  void* (*compress)(const void*,size_t,size_t*);

  switch(process_format)
  {
    case RGBA8888:
      process = encode::rgba8888;
      break;

    case RGB888:
      process = encode::rgb888;
      break;

    case RGBA5551:
      process = encode::rgba5551;
      break;

    case RGB565:
      process = encode::rgb565;
      break;

    case RGBA4444:
      process = encode::rgba4444;
      break;

    case LA88:
      process = encode::la88;
      break;

    case HILO88:
      process = encode::hilo88;
      break;

    case L8:
      process = encode::l8;
      break;

    case A8:
      process = encode::a8;
      break;

    case LA44:
      process = encode::la44;
      break;

    case L4:
      process = encode::l4;
      break;

    case A4:
      process = encode::a4;
      break;

    case ETC1:
      rg_etc1::pack_etc1_block_init();
      process = encode::etc1;
      break;

    case ETC1A4:
      rg_etc1::pack_etc1_block_init();
      process = encode::etc1a4;
      break;

    case AUTO_L8:
      process = encode::l8;
      if(has_alpha<8>(img))
        process = encode::la88;
      break;

    case AUTO_L4:
      process = encode::l4;
      if(has_alpha<4>(img))
        process = encode::la44;
      break;

    case AUTO_ETC1:
      rg_etc1::pack_etc1_block_init();
      process = encode::etc1;
      if(has_alpha<4>(img))
        process = encode::etc1a4;
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

  if(filter_type != Magick::UndefinedFilter && img.columns() > 8 && img.rows() > 8)
  {
    preview_width *= 1.5;

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

  Magick::Image preview(Magick::Geometry(preview_width, img.rows()), transparent);

  std::vector<std::thread> workers;
  for(size_t i = 0; i < NUM_THREADS; ++i)
    workers.push_back(std::thread(work_thread, nullptr));

  encode::Buffer buf;
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
        encode::WorkUnit work;

        work.sequence     = num_work++;
        work.p            = p + j*img.columns() + i;
        work.stride       = img.columns();
        work.etc1_quality = etc1_quality;
        work.output       = !output_path.empty();
        work.preview      = !preview_path.empty();
        work.process      = process;

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

      encode::WorkUnit work = result_queue.top();
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
  {
    try
    {
        preview.write(preview_path);
    }
    catch(...)
    {
      try
      {
        preview.magick("PNG");
        preview.write(preview_path);
      }
      catch(...)
      {
        std::fprintf(stderr, "Failed to output preview\n");
      }
    }
  }

  while(!workers.empty())
  {
    workers.back().join();
    workers.pop_back();
  }

  if(output_path.empty())
    return;

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
  std::printf("Usage: %s [-f <format>] [-m <filter>] [-o <output>] [-p <preview>] [-q <etc1-quality>] [-z <compression>] <input>\n", prog);

  std::printf(
    "    -f <format>       See \"Format Options\"\n"
    "    -m <filter>       Generate mipmaps. See \"Mipmap Filter Options\"\n"
    "    -o <output>       Output file\n"
    "    -p <preview>      Output preview file\n"
    "    -q <etc1-quality> ETC1 quality. Valid options: low, medium (default), high\n"
    "    -z <compression>  Compress output. See \"Compression Options\"\n"
    "    <input>           Input file\n\n"

    "  Format Options:\n"
    "    -f rgba, -f rgba8, -f rgba8888\n"
    "      32-bit RGBA (8-bit components) (default)\n\n"

    "    -f rgb, -f rgb8, -f rgb888\n"
    "      24-bit RGB (8-bit components)\n\n"

    "    -f rgba5551\n"
    "      16-bit RGBA (5-bit RGB, 1-bit Alpha)\n\n"

    "    -f rgb565\n"
    "      16-bit RGB (5-bit Red/Blue, 6-bit Green)\n\n"

    "    -f rgba4, -f rgba444\n"
    "      16-bit RGBA (4-bit components)\n\n"

    "    -f la, -f la8, -f la88\n"
    "      16-bit Luminance/Alpha (8-bit components)\n\n"

    "    -f hilo, -f hilo8, -f hilo88\n"
    "      16-bit HILO (8-bit components)\n"
    "      Note: HI comes from Red channel, LO comes from Green channel\n\n"

    "    -f l, -f l8\n"
    "      8-bit Luminance\n\n"

    "    -f a, -f a8\n"
    "      8-bit Alpha\n\n"

    "    -f la4, -f la44\n"
    "      8-bit Luminance/Alpha (4-bit components)\n\n"

    "    -f l4\n"
    "      4-bit Luminance\n\n"

    "    -f a4\n"
    "      4-bit Alpha\n\n"

    "    -f etc1\n"
    "      ETC1\n\n"

    "    -f etc1a4\n"
    "      ETC1 with 4-bit Alpha\n\n"

    "    -f auto-l8\n"
    "      L8 when input has no alpha, otherwise LA8\n\n"

    "    -f auto-l4\n"
    "      L4 when input has no alpha, otherwise LA4\n\n"

    "    -f auto-etc1\n"
    "      ETC1 when input has no alpha, otherwise ETC1A4\n\n"

    "  Mipmap Filter Options:\n");
    for(size_t i = 0; i < ARRAY_COUNT(filter_type_strings); ++i)
      std::printf("    -m %s\n", filter_type_strings[i].str);

    std::printf("\n"
    "  Compression Options:\n"
    "    -z none              No compression (default)\n"
    "    -z fake              Fake compression header\n"
    "    -z huff, -z huffman  Huffman encoding (possible to produce garbage)\n"
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
  { "format",   required_argument, nullptr, 'f', },
  { "output",   required_argument, nullptr, 'o', },
  { "quality",  required_argument, nullptr, 'q', },
  { "compress", required_argument, nullptr, 'z', },
  { nullptr,    no_argument,       nullptr,   0, },
};

}

int main(int argc, char *argv[])
{
  const char *prog = argv[0];

  int c;

  while((c = ::getopt_long(argc, argv, "f:hm:o:p:q:s:z:", long_options, nullptr)) != -1)
  {
    switch(c)
    {
      case 'f':
      {
        ProcessFormatString *it =
          std::lower_bound(output_format_strings,
                           output_format_strings_end,
                           optarg);
        if(it != output_format_strings_end && strcasecmp(it->str, optarg) == 0)
          process_format = it->fmt;
        else
        {
          std::fprintf(stderr, "Invalid format option '%s'\n", optarg);
          return EXIT_FAILURE;
        }

        break;
      }

      case 'h':
        print_usage(prog);
        return EXIT_SUCCESS;

      case 'm':
      {
        FilterTypeString *it =
          std::lower_bound(filter_type_strings,
                           filter_type_strings_end,
                           optarg);
        if(it != filter_type_strings_end && strcasecmp(it->str, optarg) == 0)
          filter_type = it->type;
        else
        {
          std::fprintf(stderr, "Invalid mipmap filter type '%s'\n", optarg);
          return EXIT_FAILURE;
        }

        break;
      }

      case 'o':
        output_path = optarg;
        break;

      case 'p':
        preview_path = optarg;
        break;

      case 'q':
        if(strcasecmp("low", optarg) == 0)
          etc1_quality = rg_etc1::cLowQuality;
        else if(strcasecmp("medium", optarg) == 0
             || strcasecmp("med", optarg) == 0)
          etc1_quality = rg_etc1::cMediumQuality;
        else if(strcasecmp("high", optarg) == 0)
          etc1_quality = rg_etc1::cHighQuality;
        else
          std::fprintf(stderr, "Invalid ETC1 quality '%s'\n", optarg);
        break;

      case 'z':
      {
        CompressionFormatString *it =
          std::lower_bound(compression_format_strings,
                           compression_format_strings_end,
                           optarg);
        if(it != compression_format_strings_end && strcasecmp(it->str, optarg) == 0)
          compression_format = it->fmt;
        else
        {
          std::fprintf(stderr, "Invalid compression option '%s'\n", optarg);
          return EXIT_FAILURE;
        }

        break;
      }

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
