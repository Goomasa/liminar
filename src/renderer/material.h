#pragma once

#include "util/random.h"
#include "util/utils.h"

class Bxdf {
public:
  virtual ~Bxdf() = default;
  virtual Color eval(const Vec3 &out, const Vec3 &normal) const = 0;
  virtual Vec3 sample(const Vec3 &in, const Vec3 &normal,
                      Random *rnd) const = 0;
  virtual double sampling_pdf(const Vec3 &out, const Vec3 &normal) const = 0;

  static Vec3 reflection_dir(const Vec3 &in, const Vec3 &normal) {
    return normalize(in - normal * 2. * dot(normal, in));
  }

  virtual bool is_specular() const = 0;
};

class Lambertian : public Bxdf {
public:
  Color color; // wip: use texture

  Lambertian(const Color &color) : color(color) {}

  Color eval(const Vec3 &out, const Vec3 &normal) const override {
    return color * PI_INV;
  }

  Vec3 sample(const Vec3 &in, const Vec3 &normal, Random *rnd) const override;

  double sampling_pdf(const Vec3 &out, const Vec3 &normal) const override;

  bool is_specular() const override { return false; }
};

class Mirror : public Bxdf {
public:
  Color color;

  Mirror(const Color &color) : color(color) {}

  Color eval(const Vec3 &out, const Vec3 &normal) const override {
    return color / fabs(dot(out, normal));
  }

  Vec3 sample(const Vec3 &in, const Vec3 &normal, Random *rnd) const override {
    return reflection_dir(in, normal);
  }

  double sampling_pdf(const Vec3 &out, const Vec3 &normal) const override {
    return 1.; // Dirac-deita
  }

  bool is_specular() const override { return true; }
};
