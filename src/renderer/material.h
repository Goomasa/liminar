#pragma once

#include "util/random.h"
#include "util/utils.h"
#include <cmath>

class Bxdf {
public:
  virtual ~Bxdf() = default;
  virtual Color eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                     const bool into) const = 0;
  virtual Vec3 sample(const Vec3 &wo, const Vec3 &normal, const bool into,
                      Random *rnd) const = 0;
  virtual double sampling_pdf(const Vec3 &wo, const Vec3 &wi,
                              const Vec3 &normal, const bool into) const = 0;

  static Vec3 reflection_dir(const Vec3 &wo, const Vec3 &normal) {
    return normalize(normal * 2. * dot(normal, wo) - wo);
  }

  static Vec3 refraction_dir(const Vec3 &wo, const Vec3 &normal,
                             const double ior_o, const double ior_i) {
    // ior_o -> ior_i in pt
    double r = ior_o / ior_i, d = dot(wo, normal);
    return normalize(-r * (wo - d * normal) -
                     normal * std::sqrt(1. - r * r * (1. - d * d)));
  }

  static double schlick_fresnel(const Vec3 &wo, const Vec3 &wi,
                                const Vec3 &normal, const double ior_o,
                                const double ior_i) {
    double a = ior_o - ior_i, b = ior_o + ior_i;
    double f0 = (a * a) / (b * b);
    double d = ior_o > ior_i ? fabs(dot(wi, normal)) : fabs(dot(wo, normal));
    return f0 + (1. - f0) * pow(1. - d, 5);
  }

  virtual bool is_specular() const = 0;

  virtual void display() const = 0;
};

class Lambertian : public Bxdf {
public:
  Color color; // wip: use texture

  Lambertian(const Color &color) : color(color) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
             const bool into) const override {
    return color * PI_INV;
  }

  Vec3 sample(const Vec3 &wo, const Vec3 &normal, const bool into,
              Random *rnd) const override;

  double sampling_pdf(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                      const bool into) const override;

  bool is_specular() const override { return false; }

  void display() const override {
    std::cout << "Lambertian, color: ";
    color.display();
  }
};

class Mirror : public Bxdf {
public:
  Color color;

  Mirror(const Color &color) : color(color) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
             const bool into) const override {
    return color / fabs(dot(wi, normal));
  }

  Vec3 sample(const Vec3 &wo, const Vec3 &normal, const bool into,
              Random *rnd) const override {
    return reflection_dir(wo, normal);
  }

  double sampling_pdf(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                      const bool into) const override {
    return 1.; // Dirac-deita
  }

  bool is_specular() const override { return true; }

  void display() const override {
    std::cout << "Mirror, color: ";
    color.display();
  }
};

class Glass : public Bxdf {
public:
  Color color;
  double ior;

  Glass(const Color &color, const double ior) : color(color), ior(ior) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
             const bool into) const override;

  Vec3 sample(const Vec3 &wo, const Vec3 &normal, const bool into,
              Random *rnd) const override;

  double sampling_pdf(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                      const bool into) const override;

  bool is_specular() const override { return true; }

  void display() const override {
    std::cout << "Mirror, ior: " << ior << " , color: ";
    color.display();
  }
};
