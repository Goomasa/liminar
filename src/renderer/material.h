#pragma once

#include "util/random.h"
#include "util/utils.h"
#include <algorithm>
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

  virtual bool is_specular() const = 0;

  virtual void display() const = 0;

protected:
  static Vec3 sample_cosine_hemisphere(const Vec3 &wo, const Vec3 &normal,
                                       Random *rnd) {
    Vec3 w = normal, u, v;
    if (fabs(w.x) > EPS) {
      u = normalize(cross(Vec3(0, 1, 0), w));
    } else {
      u = normalize(cross(Vec3(1, 0, 0), w));
    }
    v = cross(w, u);

    double phi = 2 * PI * rnd->gen();
    double sin_theta2 = rnd->gen();
    double sin_theta = sqrt(sin_theta2);

    return normalize(u * sin_theta * cos(phi) + v * sin_theta * sin(phi) +
                     w * sqrt(1 - sin_theta2));
  }

  inline static Vec3 reflection_dir(const Vec3 &wo, const Vec3 &normal) {
    return normalize(normal * 2. * dot(normal, wo) - wo);
  }

  inline static Vec3 refraction_dir(const Vec3 &wo, const Vec3 &normal,
                                    const double ior_o, const double ior_i) {
    // ior_o -> ior_i in pt
    double r = ior_o / ior_i, d = dot(wo, normal);
    return normalize(-r * (wo - d * normal) -
                     normal * std::sqrt(1. - r * r * (1. - d * d)));
  }

  inline static double schlick_fresnel(const Vec3 &wo, const Vec3 &wi,
                                       const Vec3 &normal, const double ior_o,
                                       const double ior_i) {
    double a = ior_o - ior_i, b = ior_o + ior_i;
    double f0 = (a * a) / (b * b);
    double d = ior_o > ior_i ? fabs(dot(wi, normal)) : fabs(dot(wo, normal));
    return f0 + (1. - f0) * pow(1. - d, 5);
  }

  inline static Color schlick_fresnel(const Color &f0, const Vec3 &wi,
                                      const Vec3 &wm) {
    return f0 + (Vec3(1) - f0) * pow(1. - dot(wi, wm), 5);
  }

  inline static Color fresnel_gaussian(const Color &f0, const Vec3 &wi,
                                       const Vec3 &normal) {
    double d = dot(wi, normal);
    return f0 + (Vec3(1.) - f0) * exp2((-5.55473 * d - 6.98316) * d);
  }
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

class MicroBrdf : public Bxdf {
  // isotropic
public:
  Color color;
  double alpha;

  MicroBrdf(const Color &color, const double roughness)
      : color(color), alpha(std::clamp(roughness * roughness, 0.001, 1.)) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
             const bool into) const override;

  Vec3 sample(const Vec3 &wo, const Vec3 &normal, const bool into,
              Random *rnd) const override;

  double sampling_pdf(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                      const bool into) const override {
    Vec3 wm = normalize(wi + wo);
    return ggx_g1(wo, normal) * ggx_d(wm, normal) * 0.25 /
           fabs(dot(wo, normal));
  }

  bool is_specular() const override { return alpha < EPS; }

  void display() const override {
    std::cout << "Microfacet brdf, alpha: " << alpha << " , color: ";
    color.display();
  }

private:
  double ggx_lambda(const Vec3 &w, const Vec3 &n) const {
    double tan_theta2 = 1. / pow(dot(w, n), 2) - 1;
    return (-1 + sqrt(1 + alpha * alpha * tan_theta2)) * 0.5;
  }

  inline double ggx_g1(const Vec3 &w, const Vec3 &n) const {
    return 1. / (1 + ggx_lambda(w, n));
  }

  inline double ggx_g2(const Vec3 &wi, const Vec3 &wo, const Vec3 &n) const {
    return 1. / (1 + ggx_lambda(wi, n) + ggx_lambda(wo, n));
  }

  double ggx_d(const Vec3 &wm, const Vec3 &n) const {
    double cos_theta2 = pow(dot(wm, n), 2);
    double tan_theta2 = 1. / cos_theta2 - 1;
    double tmp1 = PI * pow(alpha * cos_theta2, 2);
    double tmp2 = 1 + tan_theta2 / (alpha * alpha);
    return 1. / (tmp1 * tmp2 * tmp2);
  }
};

class MicroBtdf : public Bxdf {
public:
  Color color;
  double ior;
  double alpha;

  MicroBtdf(const Color &color, const double ior, const double roughness)
      : color(color), ior(ior),
        alpha(std::clamp(roughness * roughness, 0.001, 1.)) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
             const bool into) const override;

  Vec3 sample(const Vec3 &wo, const Vec3 &normal, const bool into,
              Random *rnd) const override;

  double sampling_pdf(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                      const bool into) const override;

  bool is_specular() const override { return alpha < EPS; }

  void display() const override {
    std::cout << "Microfacet btdf, ior: " << ior << " , alpha: " << alpha
              << " , color: ";
    color.display();
  }

private:
  double ggx_lambda(const Vec3 &w, const Vec3 &n) const {
    double tan_theta2 = 1. / pow(dot(w, n), 2) - 1;
    return (-1 + sqrt(1 + alpha * alpha * tan_theta2)) * 0.5;
  }

  inline double ggx_g1(const Vec3 &w, const Vec3 &n) const {
    return 1. / (1 + ggx_lambda(w, n));
  }

  inline double ggx_g2(const Vec3 &wi, const Vec3 &wo, const Vec3 &n) const {
    return 1. / (1 + ggx_lambda(wi, n) + ggx_lambda(wo, n));
  }

  double ggx_d(const Vec3 &wm, const Vec3 &n) const {
    double cos_theta2 = pow(dot(wm, n), 2);
    double tan_theta2 = 1. / cos_theta2 - 1;
    double tmp1 = PI * pow(alpha * cos_theta2, 2);
    double tmp2 = 1 + tan_theta2 / (alpha * alpha);
    return 1. / (tmp1 * tmp2 * tmp2);
  }

  double ggx_j(const Vec3 &wi, const Vec3 &wo, const Vec3 &wm,
               const double r) const {
    double denom = pow(dot(wo, wm) + dot(wi, wm) / r, 2) * r * r;
    return fabs(dot(wo, wm)) / denom;
  }

  Vec3 get_half_vec(const Vec3 &wi, const Vec3 &wo, const double r) const {
    return -1. * normalize(wi + r * wo);
  }

  double ggx_vndf(const Vec3 &wi, const Vec3 &wo, const Vec3 &wm,
                  const Vec3 &normal) const {
    return ggx_g1(wo, normal) * fabs(dot(wo, wm)) * ggx_d(wm, normal) /
           fmax(fabs(dot(wo, normal)), EPS);
  }

  Vec3 sample_vndf(const Vec3 &wo, const Vec3 &normal, const bool into,
                   Random *rnd) const {
    Vec3 w = normal;
    Vec3 u = fabs(w.x) > EPS ? normalize(cross(Vec3(0, 1, 0), w))
                             : normalize(cross(Vec3(1, 0, 0), w));
    Vec3 v = cross(w, u);

    Vec3 ve = Vec3(dot(u, wo), dot(v, wo), dot(normal, wo));
    Vec3 vh = normalize(Vec3(alpha * ve.x, alpha * ve.y, ve.z));

    while (true) {
      double phi = 2.0 * PI * rnd->gen();
      double z = fma((1.0 - rnd->gen()), (1.0 + vh.z), -vh.z);
      double sin_theta = sqrt(clamp01(1.0 - z * z));
      double x = sin_theta * cos(phi);
      double y = sin_theta * sin(phi);
      Vec3 c = Vec3(x, y, z);
      Vec3 h = c + vh;

      Vec3 wm_tmp = Vec3(alpha * h.x, alpha * h.y, h.z);
      Vec3 wm = normalize(u * wm_tmp.x + v * wm_tmp.y + w * wm_tmp.z);

      if (dot(reflection_dir(wo, wm), normal) > 0.) {
        return wm;
      }
    }
  }
};

class MicroBrdfAniso : public Bxdf {
public:
  Color color;
  double alpha_x, alpha_y;

  MicroBrdfAniso(const Color &color, const double roughness_x,
                 const double roughness_y)
      : color(color), alpha_x(std::clamp(roughness_x * roughness_x, 0.001, 1.)),
        alpha_y(std::clamp(roughness_y * roughness_y, 0.001, 1.)) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
             const bool into) const override;

  Vec3 sample(const Vec3 &wo, const Vec3 &normal, const bool into,
              Random *rnd) const override;

  double sampling_pdf(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                      const bool into) const override {
    Vec3 wm = normalize(wi + wo);
    return ggx_g1(wo, normal) * ggx_d(wm, normal) * 0.25 /
           fabs(dot(wo, normal));
  }

  bool is_specular() const override { return alpha_x < EPS || alpha_y < EPS; }

  void display() const override {
    std::cout << "Microfacet brdf, alpha-x: " << alpha_x
              << " , alpha-y: " << alpha_y << " , color: ";
    color.display();
  }

private:
  double ggx_lambda(const Vec3 &w, const Vec3 &n) const {
    double cos_theta = dot(w, n);
    double sin_theta2 = 1 - cos_theta * cos_theta;
    double tan_theta2 = 1. / cos_theta * cos_theta - 1;

    Vec3 z = n;
    Vec3 x = fabs(z.x) > EPS ? normalize(cross(Vec3(0, 1, 0), z))
                             : normalize(cross(Vec3(1, 0, 0), z));
    Vec3 y = cross(z, x);

    Vec3 w_tmp = w - z * cos_theta;
    double cos_phi2 = pow(dot(w_tmp, x), 2) / sin_theta2;
    double sin_phi2 = pow(dot(w_tmp, y), 2) / sin_theta2;

    return (-1 + sqrt(1 + tan_theta2 * (alpha_x * alpha_x * cos_phi2 +
                                        alpha_y * alpha_y * sin_phi2))) *
           0.5;
  }

  inline double ggx_g1(const Vec3 &w, const Vec3 &n) const {
    return 1. / (1 + ggx_lambda(w, n));
  }

  inline double ggx_g2(const Vec3 &wi, const Vec3 &wo, const Vec3 &n) const {
    return 1. / (1 + ggx_lambda(wi, n) + ggx_lambda(wo, n));
  }

  double ggx_d(const Vec3 &wm, const Vec3 &n) const {
    double cos_theta = dot(wm, n);
    double cos_theta2 = cos_theta * cos_theta;
    double sin_theta2 = 1 - cos_theta2;
    double tan_theta2 = 1. / cos_theta2 - 1;

    Vec3 z = n;
    Vec3 x = fabs(z.x) > EPS ? normalize(cross(Vec3(0, 1, 0), z))
                             : normalize(cross(Vec3(1, 0, 0), z));
    Vec3 y = cross(z, x);

    Vec3 w_tmp = wm - z * cos_theta;
    double cos_phi2 = pow(dot(w_tmp, x), 2) / sin_theta2;
    double sin_phi2 = pow(dot(w_tmp, y), 2) / sin_theta2;

    double tmp1 = PI * alpha_x * alpha_y * cos_theta2 * cos_theta2;
    double tmp2 = 1 + tan_theta2 * (cos_phi2 / (alpha_x * alpha_x) +
                                    sin_phi2 / (alpha_y * alpha_y));
    return 1. / (tmp1 * tmp2 * tmp2);
  }
};

class MixtureBrdf : public Bxdf {
public:
  Color base_color;
  double metalic, specular, alpha;

  MixtureBrdf(const Color &color, const double metalic, const double specular,
              const double roughness)
      : base_color(color), metalic(metalic), specular(specular),
        alpha(std::clamp(roughness * roughness, 0.001, 1.)) {
    diffuse_prob = 1. / (2. - metalic);
  }

  MixtureBrdf(const Color &color, const double metalic, const double roughness)
      : base_color(color), metalic(metalic), specular(0.5),
        alpha(std::clamp(roughness * roughness, 0.001, 1.)) {
    diffuse_prob = (1. - metalic) / (2. - metalic);
  }

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
             const bool into) const override;

  Vec3 sample(const Vec3 &wo, const Vec3 &normal, const bool into,
              Random *rnd) const override;

  double sampling_pdf(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                      const bool into) const override;

  bool is_specular() const override { return false; }

  void display() const override {
    std::cout << "Mixture , metalic: " << metalic << " , specular: " << specular
              << " , alpha: " << alpha << " , color: ";
    base_color.display();
  }

private:
  double diffuse_prob;

  bool sample_diffuse(Random *rnd) const {
    // true -> diffuse , false -> specular
    return rnd->gen() < diffuse_prob;
  }

  double ggx_lambda(const Vec3 &w, const Vec3 &n) const {
    double tan_theta2 = 1. / pow(dot(w, n), 2) - 1;
    return (-1 + sqrt(1 + alpha * alpha * tan_theta2)) * 0.5;
  }

  inline double ggx_g1(const Vec3 &w, const Vec3 &n) const {
    return 1. / (1 + ggx_lambda(w, n));
  }

  inline double ggx_g2(const Vec3 &wi, const Vec3 &wo, const Vec3 &n) const {
    return 1. / (1 + ggx_lambda(wi, n) + ggx_lambda(wo, n));
  }

  double ggx_d(const Vec3 &wm, const Vec3 &n) const {
    double cos_theta2 = pow(dot(wm, n), 2);
    double tan_theta2 = 1. / cos_theta2 - 1;
    double tmp1 = PI * pow(alpha * cos_theta2, 2);
    double tmp2 = 1 + tan_theta2 / (alpha * alpha);
    return 1. / (tmp1 * tmp2 * tmp2);
  }

  Vec3 sample_vndf(const Vec3 &wo, const Vec3 &normal, Random *rnd) const {
    Vec3 w = normal;
    Vec3 u = fabs(w.x) > EPS ? normalize(cross(Vec3(0, 1, 0), w))
                             : normalize(cross(Vec3(1, 0, 0), w));
    Vec3 v = cross(w, u);

    Vec3 ve = Vec3(dot(u, wo), dot(v, wo), dot(normal, wo));
    Vec3 vh = normalize(Vec3(alpha * ve.x, alpha * ve.y, ve.z));

    while (true) {
      double phi = 2.0 * PI * rnd->gen();
      double z = fma((1.0 - rnd->gen()), (1.0 + vh.z), -vh.z);
      double sin_theta = sqrt(clamp01(1.0 - z * z));
      double x = sin_theta * cos(phi);
      double y = sin_theta * sin(phi);
      Vec3 c = Vec3(x, y, z);
      Vec3 h = c + vh;

      Vec3 wm_tmp = Vec3(alpha * h.x, alpha * h.y, h.z);
      Vec3 wm = normalize(u * wm_tmp.x + v * wm_tmp.y + w * wm_tmp.z);

      if (dot(reflection_dir(wo, wm), normal) > 0.) {
        return wm;
      }
    }
  }

  Color fs0() const {
    double t = 0.08 * specular;
    return base_color * (1. - t) + Vec3(metalic) * t;
  }
};
