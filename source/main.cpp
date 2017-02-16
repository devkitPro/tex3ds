#include "compat.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
#include <Magick++.h>
#include <getopt.h>
#include "compress.h"

typedef std::vector<uint8_t> Buffer;

namespace
{

std::string output_path;

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
} output_format = RGBA8888;

enum CompressionFormat
{
  COMPRESSION_NONE,
  COMPRESSION_FAKE,
  COMPRESSION_LZ10,
  COMPRESSION_LZ11,
  COMPRESSION_RLE,
  COMPRESSION_HUFF,
} compression_format = COMPRESSION_NONE;

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

template <int bits>
unsigned int value(Magick::Quantum v)
{
  using Magick::Quantum;
  return (1<<bits) * v / (QuantumRange+1);
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

void output_rgba8888(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; ++i)
  {
    Magick::Color c = p[i];

    output.push_back(value<8>(alpha(c)));
    output.push_back(value<8>(c.blueQuantum()));
    output.push_back(value<8>(c.greenQuantum()));
    output.push_back(value<8>(c.redQuantum()));
  }
}

void output_rgb888(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; ++i)
  {
    Magick::Color c = p[i];

    output.push_back(value<8>(c.blueQuantum()));
    output.push_back(value<8>(c.greenQuantum()));
    output.push_back(value<8>(c.redQuantum()));
  }
}

void output_rgba5551(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; ++i)
  {
    unsigned int  v;
    Magick::Color c = p[i];

    v = (value<5>(c.redQuantum())   << 11)
      | (value<5>(c.greenQuantum()) <<  6)
      | (value<5>(c.blueQuantum())  <<  1)
      | (value<1>(alpha(c))         <<  0);

    output.push_back(v >> 0);
    output.push_back(v >> 8);
  }
}

void output_rgb565(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; ++i)
  {
    unsigned int  v;
    Magick::Color c = p[i];

    v = (value<5>(c.redQuantum())   << 11)
      | (value<6>(c.greenQuantum()) <<  5)
      | (value<5>(c.blueQuantum())  <<  0);

    output.push_back(v >> 0);
    output.push_back(v >> 8);
  }
}

void output_rgba4444(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; ++i)
  {
    unsigned int  v;
    Magick::Color c = p[i];

    v = (value<4>(alpha(c))         <<  0)
      | (value<4>(c.blueQuantum())  <<  4)
      | (value<4>(c.greenQuantum()) <<  8) 
      | (value<4>(c.redQuantum())   << 12);

    output.push_back(v >> 0);
    output.push_back(v >> 8);
  }
}

void output_la88(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; ++i)
  {
    Magick::Color c = p[i];

    output.push_back(value<8>(alpha(c)));
    output.push_back(value<8>(luminance(c)));
  }
}

void output_hilo88(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; ++i)
  {
    Magick::Color c = p[i];

    output.push_back(value<8>(c.greenQuantum()));
    output.push_back(value<8>(c.redQuantum()));
  }
}

void output_l8(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; ++i)
  {
    Magick::Color c = p[i];

    output.push_back(value<8>(luminance(c)));
  }
}

void output_a8(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; ++i)
  {
    Magick::Color c = p[i];

    output.push_back(value<8>(alpha(c)));
  }
}

void output_la44(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; ++i)
  {
    Magick::Color c = p[i];

    output.push_back((value<4>(luminance(c)) << 4)
                   | (value<4>(alpha(c))     << 0));
  }
}

void output_l4(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; i += 2)
  {
    Magick::Color c1 = p[i], c2 = p[i+1];

    output.push_back((value<4>(luminance(c1)) << 0)
                   | (value<4>(luminance(c2)) << 4));
  }
}

void output_a4(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t i = 0; i < 64; i += 2)
  {
    Magick::Color c1 = p[i], c2 = p[i+1];

    output.push_back((value<8>(alpha(c1)) << 0)
                   | (value<8>(alpha(c2)) << 4));
  }
}

void output_etc1(Magick::PixelPacket *p, Buffer &output)
{
  for(size_t j = 0; j < 8; j += 4)
  {
    for(size_t i = 0; i < 8; i += 4)
    {
      Magick::Color block[16];
      for(size_t y = 0; y < 4; ++y)
      {
        for(size_t x = 0; x < 4; ++x)
        {
          block[y*4+x] = p[(j+y)*8+i+x];
        }
      }

      //etc1_block(block);
    }
  }
}

void output_etc1a4(Magick::PixelPacket *p, Buffer &output)
{
}

void process_image(Magick::Image img)
{
  Buffer buf;
  void (*output)(Magick::PixelPacket*,Buffer&);
  void* (*compress)(const void*,size_t,size_t*);

  switch(output_format)
  {
    case RGBA8888: output = output_rgba8888; break;
    case RGB888:   output = output_rgb888;   break;
    case RGBA5551: output = output_rgba5551; break;
    case RGB565:   output = output_rgb565;   break;
    case RGBA4444: output = output_rgba4444; break;
    case LA88:     output = output_la88;     break;
    case HILO88:   output = output_hilo88;   break;
    case L8:       output = output_l8;       break;
    case A8:       output = output_a8;       break;
    case LA44:     output = output_la44;     break;
    case L4:       output = output_l4;       break;
    case A4:       output = output_a4;       break;
    case ETC1:     output = output_etc1;     break;
    case ETC1A4:   output = output_etc1a4;   break;
  }

  switch(compression_format)
  {
    case COMPRESSION_NONE:
    case COMPRESSION_FAKE: compress = nullptr;     break;
    case COMPRESSION_LZ10: compress = lzss_encode; break;
    case COMPRESSION_LZ11: compress = lz11_encode; break;
    case COMPRESSION_RLE:  compress = rle_encode;  break;
    case COMPRESSION_HUFF: compress = huff_encode; break;
  }

  size_t width  = img.columns();
  size_t height = img.rows();

  img.modifyImage();
  Magick::Pixels cache(img);

  for(size_t j = 0; j < height; j += 8)
  {
    for(size_t i = 0; i < width; i += 8)
    {
      Magick::PixelPacket *p = cache.get(i, j, 8, 8);
      swizzle(p);
      output(p, buf);
      cache.sync();
    }
  }

  if(compression_format != COMPRESSION_NONE && buf.size() > 0xFFFFFF)
    std::fprintf(stderr, "Warning: output size exceeds header limit\n");

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
  std::printf("Usage: %s [<format>] [-z <compression>] [-o <output>] <input>\n", prog);
  std::printf(
    "  Format options:\n"
    "    -0, --rgba, --rgba8, --rgba888\n"
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
    "      ETC1 (unsupported)\n\n"

    "    -d, --etc1a4\n"
    "      ETC1 with 4-bit Alpha (unsupported)\n\n"

    "  Compression options:\n"
    "    -z none              No compression (default)\n"
    "    -z fake              Fake compression header\n"
    "    -z huff, -z huffman  Huffman encoding (unsupported)\n"
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
  int index;

  while((c = ::getopt_long(argc, argv, "0123456789abcdho:s:ABCD", long_options, &index)) != -1)
  {
    switch(c)
    {
      case 'A' ... 'D':
        c = (c - 'A') + 'a';
      case '0' ... '9':
      case 'a' ... 'd':
        output_format = static_cast<OutputFormat>(c);
        break;

      case 'h':
        print_usage(prog);
        return EXIT_SUCCESS;

      case 'o':
        output_path = optarg;
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

  const char *error_type = nullptr;
  if(output_format == ETC1)
    error_type = "etc1";
  else if(output_format == ETC1A4)
    error_type = "etc1a4";

  if(error_type)
  {
    std::fprintf(stderr, "%s not supported yet\n", error_type);
    return EXIT_FAILURE;
  }

  if(optind == argc)
  {
    std::fprintf(stderr, "No image provided\n");
    print_usage(prog);
    return EXIT_FAILURE;
  }

  if(output_path.empty())
    output_path = std::string(argv[optind]) + ".bin";

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
