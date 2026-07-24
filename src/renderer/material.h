#pragma once

#include "util/utils.h"

class Bxdf {
public:
  virtual ~Bxdf() = default;
  virtual Color eval() const = 0;
};

class Lambertian : public Bxdf {
public:
  Color color; // wip: use texture

  Lambertian(const Color &color) : color(color) {}

  Color eval() const override { return color * PI_INV; }
};
