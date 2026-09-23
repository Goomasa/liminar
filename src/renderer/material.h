#pragma once

#include "scene/texture.h"
#include "util/random.h"
#include "util/utils.h"
#include <algorithm>
#include <cmath>

enum class IntegrateMode { PT };

class Bxdf {
public:
  uint64_t id;
  Color base_color;
  Texture *color_texture;

  Bxdf(const Color &base_color, Texture *texture)
      : base_color(base_color), color_texture(texture) {}

  virtual ~Bxdf() = default;
  virtual Color eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv,
                     const Vec3 &normal, const Vec3 &geometry_normal,
                     const bool into, const IntegrateMode &mode) const = 0;
  virtual Vec3 sample(const Vec3 &dir_prev, const Vec3 &normal, const bool into,
                      Random *rnd, const IntegrateMode &mode) const = 0;
  virtual double sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                              const Vec3 &normal, const bool into,
                              const IntegrateMode &mode) const = 0;

  virtual bool is_specular() const = 0;

  virtual void display() const = 0;

protected:
  inline Color get_color(const Vec2 &uv) const {
    if (!color_texture) {
      return base_color;
    } else {
      return color_texture->get_color(uv) * base_color;
    }
  }

  static Vec3 sample_cosine_hemisphere(const Vec3 &normal, Random *rnd) {
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

  inline static Vec3 reflection_dir(const Vec3 &dir_prev, const Vec3 &normal) {
    return normalize(normal * 2. * dot(normal, dir_prev) - dir_prev);
  }

  inline static Vec3 refraction_dir(const Vec3 &dir_prev, const Vec3 &normal,
                                    const double ior_prev,
                                    const double ior_next) {
    double r = ior_prev / ior_next, d = dot(dir_prev, normal);
    return normalize(-r * (dir_prev - d * normal) -
                     normal * std::sqrt(1. - r * r * (1. - d * d)));
  }

  inline static bool check_refraction(const Vec3 &dir_prev, const Vec3 &normal,
                                      const double r_pn) {
    double d = dot(dir_prev, normal);
    return 1. - r_pn * r_pn * (1. - d * d) >= 0.;
  }

  inline static double schlick_fresnel(const Vec3 &dir_prev,
                                       const Vec3 &dir_next, const Vec3 &normal,
                                       const double ior_prev,
                                       const double ior_next) {
    double a = ior_prev - ior_next, b = ior_prev + ior_next;
    double f0 = (a * a) / (b * b);
    double d = ior_prev > ior_next ? fabs(dot(dir_next, normal))
                                   : fabs(dot(dir_prev, normal));
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

  inline static bool check_light_leak(const Vec3 &wi, const Vec3 &wo,
                                      const Vec3 &shading_normal,
                                      const Vec3 &geometry_normal) {
    // true -> ok
    return dot(wo, shading_normal) * dot(wo, geometry_normal) > 0. &&
           dot(wi, shading_normal) * dot(wi, geometry_normal) > 0.;
  }

  inline static bool check_block_surface(const Vec3 &wi, const Vec3 &wo,
                                         const Vec3 &geometry_normal) {
    // true -> reflect
    return dot(wo, geometry_normal) * dot(wi, geometry_normal) > 0.;
  }
};

class Lambertian : public Bxdf {
public:
  Lambertian(const Color &base_color, Texture *texture = nullptr)
      : Bxdf(base_color, texture) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv, const Vec3 &normal,
             const Vec3 &geometry_normal, const bool into,
             const IntegrateMode &mode) const override {
    return check_light_leak(wi, wo, normal, geometry_normal) &&
                   check_block_surface(wi, wo, geometry_normal)
               ? get_color(uv) * PI_INV
               : 0.;
  }

  Vec3 sample(const Vec3 &dir_prev, const Vec3 &normal, const bool into,
              Random *rnd, const IntegrateMode &mode) const override;

  double sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                      const Vec3 &normal, const bool into,
                      const IntegrateMode &mode) const override;

  bool is_specular() const override { return false; }

  void display() const override {
    std::cout << "Lambertian, color: ";
    base_color.display();
  }
};

class Mirror : public Bxdf {
public:
  Mirror(const Color &base_color, Texture *texture = nullptr)
      : Bxdf(base_color, texture) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv, const Vec3 &normal,
             const Vec3 &geometry_normal, const bool into,
             const IntegrateMode &mode) const override {
    return check_light_leak(wi, wo, normal, geometry_normal) &&
                   check_block_surface(wi, wo, geometry_normal)
               ? get_color(uv) / fabs(dot(wi, normal))
               : 0.;
  }

  Vec3 sample(const Vec3 &dir_prev, const Vec3 &normal, const bool into,
              Random *rnd, const IntegrateMode &mode) const override {
    return reflection_dir(dir_prev, normal);
  }

  double sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                      const Vec3 &normal, const bool into,
                      const IntegrateMode &mode) const override {
    return 1.; // Dirac-deita
  }

  bool is_specular() const override { return true; }

  void display() const override {
    std::cout << "Mirror, color: ";
    base_color.display();
  }
};

class Glass : public Bxdf {
public:
  double ior;

  Glass(const Color &base_color, const double ior, Texture *texture = nullptr)
      : Bxdf(base_color, texture), ior(ior) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv, const Vec3 &normal,
             const Vec3 &geometry_normal, const bool into,
             const IntegrateMode &mode) const override;

  Vec3 sample(const Vec3 &dir_prev, const Vec3 &normal, const bool into,
              Random *rnd, const IntegrateMode &mode) const override;

  double sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                      const Vec3 &normal, const bool into,
                      const IntegrateMode &mode) const override;

  bool is_specular() const override { return true; }

  void display() const override {
    std::cout << "Glass, ior: " << ior << " , color: ";
    base_color.display();
  }
};

class MicroBrdf : public Bxdf {
  // isotropic
public:
  double alpha;

  MicroBrdf(const Color &base_color, const double roughness,
            Texture *texture = nullptr)
      : Bxdf(base_color, texture),
        alpha(std::clamp(roughness * roughness, 0.001, 1.)) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv, const Vec3 &normal,
             const Vec3 &geometry_normal, const bool into,
             const IntegrateMode &mode) const override;

  Vec3 sample(const Vec3 &dir_prev, const Vec3 &normal, const bool into,
              Random *rnd, const IntegrateMode &mode) const override;

  double sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                      const Vec3 &normal, const bool into,
                      const IntegrateMode &mode) const override {
    Vec3 wm = normalize(dir_next + dir_prev);
    return ggx_g1(dir_prev, normal) * ggx_d(wm, normal) * 0.25 /
           fabs(dot(dir_prev, normal));
  }

  bool is_specular() const override { return alpha < EPS; }

  void display() const override {
    std::cout << "Microfacet brdf, alpha: " << alpha << " , color: ";
    base_color.display();
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
  double ior;
  double alpha;

  MicroBtdf(const Color &base_color, const double ior, const double roughness,
            Texture *texture = nullptr)
      : Bxdf(base_color, texture), ior(ior),
        alpha(std::clamp(roughness * roughness, 0.001, 1.)) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv, const Vec3 &normal,
             const Vec3 &geometry_normal, const bool into,
             const IntegrateMode &mode) const override;

  Vec3 sample(const Vec3 &dir_prev, const Vec3 &normal, const bool into,
              Random *rnd, const IntegrateMode &mode) const override;

  double sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                      const Vec3 &normal, const bool into,
                      const IntegrateMode &mode) const override;

  bool is_specular() const override { return alpha < EPS; }

  void display() const override {
    std::cout << "Microfacet btdf, ior: " << ior << " , alpha: " << alpha
              << " , color: ";
    base_color.display();
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

  double ggx_j(const Vec3 &dir_prev, const Vec3 &dir_next, const Vec3 &wm,
               const double r) const {
    double denom = pow(dot(dir_next, wm) * r + dot(dir_prev, wm), 2);
    return fabs(dot(dir_next, wm)) * r * r / denom;
  }

  Vec3 get_half_vec(const Vec3 &dir_prev, const Vec3 &dir_next,
                    const double r) const {
    return -1. * normalize(dir_prev + r * dir_next);
  }

  double ggx_vndf(const Vec3 &dir_prev, const Vec3 &dir_next, const Vec3 &wm,
                  const Vec3 &normal) const {
    return ggx_g1(dir_prev, normal) * fabs(dot(dir_prev, wm)) *
           ggx_d(wm, normal) / fmax(fabs(dot(dir_prev, normal)), EPS);
  }

  Vec3 sample_vndf(const Vec3 &dir_prev, const Vec3 &normal, const bool into,
                   Random *rnd) const {
    Vec3 w = normal;
    Vec3 u = fabs(w.x) > EPS ? normalize(cross(Vec3(0, 1, 0), w))
                             : normalize(cross(Vec3(1, 0, 0), w));
    Vec3 v = cross(w, u);

    Vec3 ve = Vec3(dot(u, dir_prev), dot(v, dir_prev), dot(normal, dir_prev));
    Vec3 vh = normalize(Vec3(alpha * ve.x, alpha * ve.y, ve.z));

    double phi = 2.0 * PI * rnd->gen();
    double z = fma((1.0 - rnd->gen()), (1.0 + vh.z), -vh.z);
    double sin_theta = sqrt(clamp01(1.0 - z * z));
    double x = sin_theta * cos(phi);
    double y = sin_theta * sin(phi);
    Vec3 c = Vec3(x, y, z);
    Vec3 h = c + vh;

    Vec3 wm_tmp = Vec3(alpha * h.x, alpha * h.y, h.z);
    Vec3 wm = normalize(u * wm_tmp.x + v * wm_tmp.y + w * wm_tmp.z);

    if (dot(reflection_dir(dir_prev, wm), normal) > 0.) {
      return wm;
    } else {
      return INVALID_VEC;
    }
  }
};

class MicroBrdfAniso : public Bxdf {
public:
  double alpha_x, alpha_y;

  MicroBrdfAniso(const Color &base_color, const double roughness_x,
                 const double roughness_y, Texture *texture = nullptr)
      : Bxdf(base_color, texture),
        alpha_x(std::clamp(roughness_x * roughness_x, 0.001, 1.)),
        alpha_y(std::clamp(roughness_y * roughness_y, 0.001, 1.)) {}

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv, const Vec3 &normal,
             const Vec3 &geometry_normal, const bool into,
             const IntegrateMode &mode) const override;

  Vec3 sample(const Vec3 &dir_prev, const Vec3 &normal, const bool into,
              Random *rnd, const IntegrateMode &mode) const override;

  double sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                      const Vec3 &normal, const bool into,
                      const IntegrateMode &mode) const override {
    Vec3 wm = normalize(dir_next + dir_prev);
    return ggx_g1(dir_prev, normal) * ggx_d(wm, normal) * 0.25 /
           fabs(dot(dir_prev, normal));
  }

  bool is_specular() const override { return alpha_x < EPS || alpha_y < EPS; }

  void display() const override {
    std::cout << "Microfacet brdf, alpha-x: " << alpha_x
              << " , alpha-y: " << alpha_y << " , color: ";
    base_color.display();
  }

private:
  double ggx_lambda(const Vec3 &w, const Vec3 &n) const {
    double cos_theta = dot(w, n);
    double sin_theta2 = 1 - cos_theta * cos_theta;
    double tan_theta2 = 1. / (cos_theta * cos_theta) - 1;

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
  double metallic, specular, alpha;

  MixtureBrdf(const Color &base_color, const double metallic,
              const double specular, const double roughness,
              Texture *col_tex = nullptr)
      : Bxdf(base_color, col_tex), metallic(clamp01(metallic)),
        specular(clamp01(specular)),
        alpha(std::clamp(roughness * roughness, 0.001, 1.)) {
    diffuse_prob = (1. - metallic) / (2. - metallic);
  }

  MixtureBrdf(const Color &base_color, const double metallic,
              const double roughness, Texture *col_tex = nullptr)
      : Bxdf(base_color, col_tex), metallic(clamp01(metallic)), specular(0.5),
        alpha(std::clamp(roughness * roughness, 0.001, 1.)) {
    diffuse_prob = (1. - metallic) / (2. - metallic);
  }

  Color eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv, const Vec3 &normal,
             const Vec3 &geometry_normal, const bool into,
             const IntegrateMode &mode) const override;

  Vec3 sample(const Vec3 &dir_prev, const Vec3 &normal, const bool into,
              Random *rnd, const IntegrateMode &mode) const override;

  double sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                      const Vec3 &normal, const bool into,
                      const IntegrateMode &mode) const override;

  bool is_specular() const override { return false; }

  void display() const override {
    std::cout << "Mixture , metallic: " << metallic
              << " , specular: " << specular << " , alpha: " << alpha
              << " , color: ";
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

  Vec3 sample_vndf(const Vec3 &dir_prev, const Vec3 &normal,
                   Random *rnd) const {
    Vec3 w = normal;
    Vec3 u = fabs(w.x) > EPS ? normalize(cross(Vec3(0, 1, 0), w))
                             : normalize(cross(Vec3(1, 0, 0), w));
    Vec3 v = cross(w, u);

    Vec3 ve = Vec3(dot(u, dir_prev), dot(v, dir_prev), dot(normal, dir_prev));
    Vec3 vh = normalize(Vec3(alpha * ve.x, alpha * ve.y, ve.z));

    double phi = 2.0 * PI * rnd->gen();
    double z = fma((1.0 - rnd->gen()), (1.0 + vh.z), -vh.z);
    double sin_theta = sqrt(clamp01(1.0 - z * z));
    double x = sin_theta * cos(phi);
    double y = sin_theta * sin(phi);
    Vec3 c = Vec3(x, y, z);
    Vec3 h = c + vh;

    Vec3 wm_tmp = Vec3(alpha * h.x, alpha * h.y, h.z);
    Vec3 wm = normalize(u * wm_tmp.x + v * wm_tmp.y + w * wm_tmp.z);

    if (dot(reflection_dir(dir_prev, wm), normal) > 0.) {
      return wm;
    } else {
      return INVALID_VEC;
    }
  }

  Color fs0(const Color &color) const {
    double t = 0.08 * specular;
    return metallic * color + (1. - metallic) * Vec3(t);
  }
};
