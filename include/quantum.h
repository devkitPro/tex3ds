#pragma once

namespace
{

template <int bits>
inline uint8_t quantum_to_bits(Magick::Quantum v)
{
  using Magick::Quantum;
  return (1<<bits) * v / (QuantumRange+1);
}

template <int bits>
inline Magick::Quantum bits_to_quantum(uint8_t v)
{
  using Magick::Quantum;
  return v * QuantumRange / ((1<<bits)-1);
}

template <int bits>
inline Magick::Quantum quantize(Magick::Quantum v)
{
  using Magick::Quantum;
  return quantum_to_bits<bits>(v) * QuantumRange / ((1<<bits)-1);
}

inline double gamma_inverse(double v)
{
  if(v <= 0.04045)
    return v / 12.92;
  return std::pow((v + 0.055) / 1.055, 2.4);
}

inline double gamma(double v)
{
  if(v <= 0.0031308)
    return v * 12.92;
  return 1.055 * std::pow(v, 1.0/2.4) - 0.055;
}

inline Magick::Quantum luminance(const Magick::Color &c)
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

inline Magick::Quantum alpha(const Magick::Color &c)
{
  using Magick::Quantum;
  return QuantumRange - c.alphaQuantum();
}

}
