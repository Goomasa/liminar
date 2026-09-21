#include "material.h"
#include "util/utils.h"
#include <cmath>

inline void get_ior(const double ior, double *ior_i, double *ior_o,
                    double *ior_prev, double *ior_next, const bool into,
                    const IntegrateMode &mode) {
  if (into) {
    *ior_prev = 1., *ior_next = ior;
    if (mode == IntegrateMode::PT) {
      *ior_i = ior, *ior_o = 1.;
    } else {
      *ior_i = 1., *ior_o = ior;
    }
  } else {
    *ior_prev = ior, *ior_next = 1.;
    if (mode == IntegrateMode::PT) {
      *ior_i = 1., *ior_o = ior;
    } else {
      *ior_i = ior, *ior_o = 1.;
    }
  }
}

inline void get_ior(const double ior, double *ior_prev, double *ior_next,
                    const bool into) {
  if (into) {
    *ior_prev = 1., *ior_next = ior;
  } else {
    *ior_prev = ior, *ior_next = 1.;
  }
}

Vec3 Lambertian::sample(const Vec3 &dir_prev, const Vec3 &normal,
                        const bool into, Random *rnd,
                        const IntegrateMode &mode) const {
  return sample_cosine_hemisphere(normal, rnd);
}

double Lambertian::sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                                const Vec3 &normal, const bool into,
                                const IntegrateMode &mode) const {
  return fmax(dot(dir_next, normal), EPS) * PI_INV;
}

Color Glass::eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv,
                  const Vec3 &normal, const Vec3 &geometry_normal,
                  const bool into, const IntegrateMode &mode) const {
  double ior_o, ior_i, ior_prev, ior_next;
  get_ior(ior, &ior_i, &ior_o, &ior_prev, &ior_next, into, mode);

  Vec3 dir_prev = mode == IntegrateMode::PT ? wo : wi;
  Vec3 dir_next = mode == IntegrateMode::PT ? wi : wo;

  double r_pn = ior_prev / ior_next, r_io = ior_o / ior_i;
  double reflectance =
      schlick_fresnel(dir_prev, dir_next, normal, ior_prev, ior_next);

  if (dot(dir_next, normal) < 0.) {
    // refract
    if (check_light_leak(wi, wo, normal, geometry_normal) &&
        !check_block_surface(wi, wo, geometry_normal)) {
      return r_io * r_io * color / fmax(fabs(dot(wi, normal)), EPS) *
             (1. - reflectance);
    } else {
      return 0.;
    }
  } else {
    // reflect
    bool is_refract = check_refraction(dir_prev, normal, r_pn);
    reflectance = is_refract ? reflectance : 1.;

    if (check_light_leak(wi, wo, normal, geometry_normal) &&
        check_block_surface(wi, wo, geometry_normal)) {
      return color / fmax(fabs(dot(wi, normal)), EPS) * reflectance;
    } else {
      return 0.;
    }
  }
}

Vec3 Glass::sample(const Vec3 &dir_prev, const Vec3 &normal, const bool into,
                   Random *rnd, const IntegrateMode &mode) const {
  double ior_prev, ior_next;
  get_ior(ior, &ior_prev, &ior_next, into);

  double r = ior_prev / ior_next;
  if (!check_refraction(dir_prev, normal, r)) {
    return reflection_dir(dir_prev, normal);
  }

  Vec3 refraction_vec = refraction_dir(dir_prev, normal, ior_prev, ior_next);
  double reflectance =
      schlick_fresnel(dir_prev, refraction_vec, normal, ior_prev, ior_next);

  if (rnd->gen() < reflectance) {
    return reflection_dir(dir_prev, normal);
  } else {
    return refraction_vec;
  }
}

double Glass::sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                           const Vec3 &normal, const bool into,
                           const IntegrateMode &mode) const {
  double ior_prev, ior_next;
  get_ior(ior, &ior_prev, &ior_next, into);

  double r = ior_prev / ior_next;

  if (!check_refraction(dir_prev, normal, r)) {
    return 1.;
  }

  if (dot(dir_next, normal) < 0.) {
    return 1. - schlick_fresnel(dir_prev, dir_next, normal, ior_prev, ior_next);
  } else {
    if (!check_refraction(dir_prev, normal, r)) {
      return 1.;
    } else {
      return schlick_fresnel(dir_prev, dir_next, normal, ior_prev, ior_next);
    }
  }
}

Color MicroBrdf::eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv,
                      const Vec3 &normal, const Vec3 &geometry_normal,
                      const bool into, const IntegrateMode &mode) const {
  Vec3 wm = normalize(wi + wo);
  double denom = fmax(4 * fabs(dot(wo, normal) * dot(wi, normal)), EPS);
  Color fresnel = schlick_fresnel(color, wi, wm);

  if (check_light_leak(wi, wo, normal, geometry_normal) &&
      check_block_surface(wi, wo, geometry_normal)) {
    return fresnel * ggx_g2(wi, wo, normal) * ggx_d(wm, normal) / denom;
  } else {
    return 0.;
  }
}

Vec3 MicroBrdf::sample(const Vec3 &dir_prev, const Vec3 &normal,
                       const bool into, Random *rnd,
                       const IntegrateMode &mode) const {
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
  Vec3 dir_next = reflection_dir(dir_prev, wm);

  if (dot(dir_next, normal) > 0.) {
    return dir_next;
  } else {
    return INVALID_VEC;
  }
}

Color MicroBtdf::eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv,
                      const Vec3 &normal, const Vec3 &geometry_normal,
                      const bool into, const IntegrateMode &mode) const {
  double ior_o, ior_i, ior_prev, ior_next;
  get_ior(ior, &ior_i, &ior_o, &ior_prev, &ior_next, into, mode);

  Vec3 dir_prev = mode == IntegrateMode::PT ? wo : wi;
  Vec3 dir_next = mode == IntegrateMode::PT ? wi : wo;

  double r_io = ior_o / ior_i, r_pn = ior_prev / ior_next;

  if (dot(dir_next, normal) < 0.) {
    // refract
    Vec3 wm = get_half_vec(dir_prev, dir_next, 1. / r_pn);

    double reflectance =
        schlick_fresnel(dir_prev, dir_next, wm, ior_prev, ior_next);

    double c = fabs(dot(wi, wm) * dot(wo, wm)) /
               fabs(dot(wi, normal) * dot(wo, normal));
    double denom = fmax(pow(dot(wi, wm) + r_io * dot(wo, wm), 2), EPS);

    if (check_light_leak(wi, wo, normal, geometry_normal) &&
        !check_block_surface(wi, wo, geometry_normal)) {
      return color * c * (1 - reflectance) * ggx_g2(wi, wo, normal) *
             ggx_d(wm, normal) * r_io * r_io / denom;
    } else {
      return 0.;
    }
  } else {
    // reflect
    Vec3 wm = normalize(wi + wo);
    double reflectance =
        schlick_fresnel(dir_prev, dir_next, wm, ior_prev, ior_next);

    bool is_refract = check_refraction(dir_prev, wm, r_pn);
    reflectance = is_refract ? reflectance : 1.;
    double denom = fmax(4 * fabs(dot(wo, normal) * dot(wi, normal)), EPS);

    if (check_light_leak(wi, wo, normal, geometry_normal) &&
        check_block_surface(wi, wo, geometry_normal)) {
      return color * reflectance * ggx_g2(wi, wo, normal) * ggx_d(wm, normal) /
             denom;
    } else {
      return 0.;
    }
  }
}

double MicroBtdf::sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                               const Vec3 &normal, const bool into,
                               const IntegrateMode &mode) const {
  double ior_prev, ior_next;
  get_ior(ior, &ior_prev, &ior_next, into);

  double r = ior_prev / ior_next;

  if (dot(dir_next, normal) < 0.) {
    // refract
    Vec3 wm = get_half_vec(dir_prev, dir_next, 1. / r);

    return (1. - schlick_fresnel(dir_prev, dir_next, wm, ior_prev, ior_next)) *
           ggx_vndf(dir_prev, dir_next, wm, normal) *
           ggx_j(dir_prev, dir_next, wm, 1. / r);
  } else {
    // reflect
    Vec3 wm = normalize(dir_next + dir_prev);

    if (!check_refraction(dir_prev, wm, r)) {
      return ggx_g1(dir_prev, normal) * ggx_d(wm, normal) * 0.25 /
             fabs(dot(dir_prev, normal));
    } else {
      return schlick_fresnel(dir_prev, dir_next, wm, ior_prev, ior_next) *
             ggx_g1(dir_prev, normal) * ggx_d(wm, normal) * 0.25 /
             fabs(dot(dir_prev, normal));
    }
  }
}

Vec3 MicroBtdf::sample(const Vec3 &dir_prev, const Vec3 &normal,
                       const bool into, Random *rnd,
                       const IntegrateMode &mode) const {
  double ior_prev, ior_next;
  get_ior(ior, &ior_prev, &ior_next, into);

  double r = ior_prev / ior_next;
  Vec3 wm = sample_vndf(dir_prev, normal, into, rnd);

  if (wm == INVALID_VEC) {
    return INVALID_VEC;
  }

  if (!check_refraction(dir_prev, wm, r)) {
    return reflection_dir(dir_prev, wm);
  }

  Vec3 refraction_vec = refraction_dir(dir_prev, wm, ior_prev, ior_next);
  double reflectance =
      schlick_fresnel(dir_prev, refraction_vec, wm, ior_prev, ior_next);

  if (rnd->gen() < reflectance) {
    return reflection_dir(dir_prev, wm);
  } else {
    return refraction_vec;
  }
}

Color MicroBrdfAniso::eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv,
                           const Vec3 &normal, const Vec3 &geometry_normal,
                           const bool into, const IntegrateMode &mode) const {
  Vec3 wm = normalize(wi + wo);
  double denom = fmax(4 * fabs(dot(wo, normal) * dot(wi, normal)), EPS);
  Color fresnel = schlick_fresnel(color, wi, wm);

  if (check_light_leak(wi, wo, normal, geometry_normal) &&
      check_block_surface(wi, wo, geometry_normal)) {
    return fresnel * ggx_g2(wi, wo, normal) * ggx_d(wm, normal) / denom;
  } else {
    return 0.;
  }
}

Vec3 MicroBrdfAniso::sample(const Vec3 &dir_prev, const Vec3 &normal,
                            const bool into, Random *rnd,
                            const IntegrateMode &mode) const {
  Vec3 w = normal;
  Vec3 u = fabs(w.x) > EPS ? normalize(cross(Vec3(0, 1, 0), w))
                           : normalize(cross(Vec3(1, 0, 0), w));
  Vec3 v = cross(w, u);

  Vec3 ve = Vec3(dot(u, dir_prev), dot(v, dir_prev), dot(normal, dir_prev));
  Vec3 vh = normalize(Vec3(alpha_x * ve.x, alpha_y * ve.y, ve.z));

  double phi = 2.0 * PI * rnd->gen();
  double z = fma((1.0 - rnd->gen()), (1.0 + vh.z), -vh.z);
  double sin_theta = sqrt(clamp01(1.0 - z * z));
  double x = sin_theta * cos(phi);
  double y = sin_theta * sin(phi);
  Vec3 c = Vec3(x, y, z);
  Vec3 h = c + vh;

  Vec3 wm_tmp = Vec3(alpha_x * h.x, alpha_y * h.y, h.z);
  Vec3 wm = normalize(u * wm_tmp.x + v * wm_tmp.y + w * wm_tmp.z);
  Vec3 wi = reflection_dir(dir_prev, wm);

  if (dot(wi, normal) > 0.) {
    return wi;
  } else {
    return INVALID_VEC;
  }
}

Color MixtureBrdf::eval(const Vec3 &wo, const Vec3 &wi, const Vec2 &uv,
                        const Vec3 &normal, const Vec3 &geometry_normal,
                        const bool into, const IntegrateMode &mode) const {
  Color color = get_color(uv);
  Vec3 wm = normalize(wi + wo);
  double denom = fmax(4 * fabs(dot(wo, normal) * dot(wi, normal)), EPS);
  Color fresnel = fresnel_gaussian(fs0(color), wi, wm);
  Color f_spec = fresnel * ggx_g2(wi, wo, normal) * ggx_d(wm, normal) / denom;

  Color f_diff = color * PI_INV;

  if (check_light_leak(wi, wo, normal, geometry_normal) &&
      check_block_surface(wi, wo, geometry_normal)) {
    return (1. - metalic) * (Vec3(1.) - fresnel) * f_diff + f_spec;
  } else {
    return 0.;
  }
}

Vec3 MixtureBrdf::sample(const Vec3 &dir_prev, const Vec3 &normal,
                         const bool into, Random *rnd,
                         const IntegrateMode &mode) const {
  if (sample_diffuse(rnd)) {
    return sample_cosine_hemisphere(normal, rnd);
  } else {
    Vec3 wm = sample_vndf(dir_prev, normal, rnd);
    return wm == INVALID_VEC ? INVALID_VEC : reflection_dir(dir_prev, wm);
  }
}

double MixtureBrdf::sampling_pdf(const Vec3 &dir_prev, const Vec3 &dir_next,
                                 const Vec3 &normal, const bool into,
                                 const IntegrateMode &mode) const {
  Vec3 wm = normalize(dir_next + dir_prev);
  double pdf_spec = ggx_g1(dir_prev, normal) * ggx_d(wm, normal) * 0.25 /
                    fabs(dot(dir_prev, normal));
  double pdf_diff = fmax(dot(dir_next, normal), EPS) * PI_INV;

  return diffuse_prob * pdf_diff + (1. - diffuse_prob) * pdf_spec;
}
