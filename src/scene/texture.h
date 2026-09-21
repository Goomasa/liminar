#pragma once

#include "util/utils.h"
#include <cmath>
#include <cstdint>
#include <vector>

enum class TexcoordSampler { REPEAT, MIRRORED_REPEAT, CLAMP_TO_EDGE };

inline double apply_wrap(const double texcoord,
                         const TexcoordSampler &sampler) {
  double new_texcoord;
  switch (sampler) {
  case TexcoordSampler::REPEAT: {
    new_texcoord = texcoord - std::floor(texcoord);
  } break;
  case TexcoordSampler::MIRRORED_REPEAT: {
    new_texcoord = 1. - texcoord + std::floor(texcoord);
  } break;
  case TexcoordSampler::CLAMP_TO_EDGE: {
    new_texcoord = clamp01(texcoord);
  } break;
  }

  return new_texcoord;
}

class Texture {
public:
  virtual ~Texture() = default;
  virtual Color get_color(const Vec2 &uv) const = 0;
};

class SolidColor : public Texture {
public:
  Color color;

  SolidColor(const Color &color) : color(color) {}

  Color get_color(const Vec2 &uv) const override { return color; }
};

class HGradationColor : public Texture {
  // horizontal gradation
public:
  Color bottom_color, top_color;

  HGradationColor(const Color &bottom, const Color &top)
      : bottom_color(bottom), top_color(top) {}

  Color get_color(const Vec2 &uv) const override {
    return (1. - uv.v) * bottom_color + uv.v * top_color;
  }
};

class ImageTexture : public Texture {
public:
  std::vector<TexColor3f> pixels;
  uint32_t width, height;
  TexcoordSampler sampler_u, sampler_v;

  ImageTexture(const std::vector<TexColor3f> &px, const uint32_t width,
               const uint32_t height,
               const TexcoordSampler &sampler_u = TexcoordSampler::REPEAT,
               const TexcoordSampler &sampler_v = TexcoordSampler::REPEAT)
      : pixels(px), width(width), height(height), sampler_u(sampler_u),
        sampler_v(sampler_v) {}

  Color get_color(const Vec2 &uv) const override {
    double u = apply_wrap(uv.u, sampler_u);
    double v = apply_wrap(uv.v, sampler_v);

    uint32_t w = uv.u == 1. ? width - 1 : static_cast<uint32_t>(u * width);
    uint32_t h = uv.v == 1 ? height - 1 : static_cast<uint32_t>(v * height);
    uint64_t idx = static_cast<uint64_t>(width * h + w);

    return pixels[idx].to_color();
  }
};
