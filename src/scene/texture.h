#pragma once

#include "util/utils.h"
#include <cstdint>
#include <string>
#include <vector>

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
  std::string filename;
  std::vector<Color> pixels;
  uint32_t width, height;

  ImageTexture(const std::string &name, const std::vector<Color> &px,
               const uint32_t width, const uint32_t height)
      : filename(name), pixels(px), width(width), height(height) {}

  Color get_color(const Vec2 &uv) const override {
    uint32_t idx = width * uv.v + uv.u;
    return pixels[idx];
  }
};
