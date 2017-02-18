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
#include "encode.h"
#include "quantum.h"
#include "rg_etc1.h"

namespace encode
{

void rgba8888(WorkUnit &work)
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

void rgb888(WorkUnit &work)
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

void rgba5551(WorkUnit &work)
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

void rgb565(WorkUnit &work)
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

void rgba4444(WorkUnit &work)
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

void la88(WorkUnit &work)
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

void hilo88(WorkUnit &work)
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

void l8(WorkUnit &work)
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

void a8(WorkUnit &work)
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

void la44(WorkUnit &work)
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

void l4(WorkUnit &work)
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

void a4(WorkUnit &work)
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

void etc1(WorkUnit &work)
{
  rg_etc1::etc1_pack_params params;
  params.clear();
  params.m_quality = work.etc1_quality;

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

void etc1a4(WorkUnit &work)
{
  rg_etc1::etc1_pack_params params;
  params.clear();
  params.m_quality = work.etc1_quality;

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

}
