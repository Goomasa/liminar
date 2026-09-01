#include "material.h"
#include "util/utils.h"
#include <cmath>

Vec3 Lambertian::sample(const Vec3 &wo, const Vec3 &normal, const bool into,
                        Random *rnd) const {
  return sample_cosine_hemisphere(wo, normal, rnd);
}

double Lambertian::sampling_pdf(const Vec3 &wo, const Vec3 &wi,
                                const Vec3 &normal, const bool into) const {
  return fmax(dot(wi, normal), EPS) * PI_INV;
}

Color Glass::eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                  const bool into) const {
  double ior_o, ior_i;
  if (into) {
    ior_o = 1., ior_i = ior;
  } else {
    ior_o = ior, ior_i = 1.;
  }

  double r = ior_o / ior_i;

  if (dot(wi, normal) < 0.) {
    // refract
    double reflectance = schlick_fresnel(wo, wi, normal, ior_o, ior_i);
    return r * r * color / fmax(-dot(wi, normal), EPS) * (1. - reflectance);
  } else {
    // reflect
    double d = dot(wo, normal);
    double reflectance = 1. - r * r * (1. - d * d) < 0.
                             ? 1.
                             : schlick_fresnel(wo, wi, normal, ior_o, ior_i);
    return color / fmax(dot(wi, normal), EPS) * reflectance;
  }
}

Vec3 Glass::sample(const Vec3 &wo, const Vec3 &normal, const bool into,
                   Random *rnd) const {
  double ior_o, ior_i;
  if (into) {
    ior_o = 1., ior_i = ior;
  } else {
    ior_o = ior, ior_i = 1.;
  }

  double r = ior_o / ior_i, d = dot(wo, normal);
  if (1. - r * r * (1. - d * d) < 0.) {
    return reflection_dir(wo, normal);
  }

  Vec3 refraction_vec = refraction_dir(wo, normal, ior_o, ior_i);
  double reflectance =
      schlick_fresnel(wo, refraction_vec, normal, ior_o, ior_i);

  if (rnd->gen() < reflectance) {
    return reflection_dir(wo, normal);
  } else {
    return refraction_vec;
  }
}

double Glass::sampling_pdf(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                           const bool into) const {
  double ior_o, ior_i;
  if (into) {
    ior_o = 1., ior_i = ior;
  } else {
    ior_o = ior, ior_i = 1.;
  }

  double r = ior_o / ior_i, d = dot(wo, normal);

  if (1. - r * r * (1. - d * d) < 0.) {
    return 1.;
  }

  if (dot(wi, normal) < 0.) {
    return 1. - schlick_fresnel(wo, wi, normal, ior_o, ior_i);
  } else {
    return schlick_fresnel(wo, wi, normal, ior_o, ior_i);
  }
}

Color MicroBrdf::eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                      const bool into) const {
  Vec3 wm = normalize(wi + wo);
  double denom = fmax(4 * fabs(dot(wo, normal) * dot(wi, normal)), EPS);
  Color fresnel = schlick_fresnel(color, wi, wm);
  return fresnel * ggx_g2(wi, wo, normal) * ggx_d(wm, normal) / denom;
}

Vec3 MicroBrdf::sample(const Vec3 &wo, const Vec3 &normal, const bool into,
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
    Vec3 wi = reflection_dir(wo, wm);

    if (dot(wi, normal) > 0.) {
      return wi;
    }
  }
}

Color MicroBtdf::eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                      const bool into) const {
  double ior_o, ior_i;
  if (into) {
    ior_o = 1., ior_i = ior;
  } else {
    ior_o = ior, ior_i = 1.;
  }

  double r = ior_o / ior_i;

  if (dot(wi, normal) < 0.) {
    // refract
    Vec3 wm = get_half_vec(wi, wo, r);
    double reflectance = schlick_fresnel(wo, wi, normal, ior_o, ior_i);
    double c = fabs(dot(wi, wm) * dot(wo, wm)) /
               fabs(dot(wi, normal) * dot(wo, normal));
    double denom = fmax(pow(dot(wi, wm) + r * dot(wo, wm), 2), EPS);

    return c * (1 - reflectance) * ggx_g2(wi, wo, normal) * ggx_d(wm, normal) *
           r * r / denom;
  } else {
    // reflect
    Vec3 wm = normalize(wi + wo);
    double d = dot(wo, wm);
    double reflectance = 1. - r * r * (1. - d * d) < 0.
                             ? 1.
                             : schlick_fresnel(wo, wi, wm, ior_o, ior_i);

    double denom = fmax(4 * fabs(dot(wo, normal) * dot(wi, normal)), EPS);
    return reflectance * ggx_g2(wi, wo, normal) * ggx_d(wm, normal) / denom;
  }
}

double MicroBtdf::sampling_pdf(const Vec3 &wo, const Vec3 &wi,
                               const Vec3 &normal, const bool into) const {
  double ior_o, ior_i;
  if (into) {
    ior_o = 1., ior_i = ior;
  } else {
    ior_o = ior, ior_i = 1.;
  }

  double r = ior_o / ior_i;

  if (dot(wi, normal) < 0.) {
    // refract
    Vec3 wm = get_half_vec(wi, wo, r);
    double d = dot(wo, wm);

    if (1. - r * r * (1. - d * d) < 0.) {
      return 0.;
    } else {
      return (1. - schlick_fresnel(wo, wi, wm, ior_o, ior_i)) *
             ggx_vndf(wi, wo, wm, normal) * ggx_j(wi, wo, wm, r);
    }
  } else {
    // reflect
    Vec3 wm = normalize(wi + wo);
    double d = dot(wo, wm);

    if (1. - r * r * (1. - d * d) < 0.) {
      return ggx_g1(wo, normal) * ggx_d(wm, normal) * 0.25 /
             fabs(dot(wo, normal));
    } else {
      return schlick_fresnel(wo, wi, wm, ior_o, ior_i) * ggx_g1(wo, normal) *
             ggx_d(wm, normal) * 0.25 / fabs(dot(wo, normal));
    }
  }
}

Vec3 MicroBtdf::sample(const Vec3 &wo, const Vec3 &normal, const bool into,
                       Random *rnd) const {
  double ior_o, ior_i;
  if (into) {
    ior_o = 1., ior_i = ior;
  } else {
    ior_o = ior, ior_i = 1.;
  }

  double r = ior_o / ior_i;
  Vec3 wm = sample_vndf(wo, normal, into, rnd);

  double d = dot(wo, wm);
  if (1. - r * r * (1. - d * d) < 0.) {
    return reflection_dir(wo, wm);
  }

  Vec3 refraction_vec = refraction_dir(wo, wm, ior_o, ior_i);
  double reflectance = schlick_fresnel(wo, refraction_vec, wm, ior_o, ior_i);

  if (rnd->gen() < reflectance) {
    return reflection_dir(wo, wm);
  } else {
    return refraction_vec;
  }
}

Color MicroBrdfAniso::eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                           const bool into) const {
  Vec3 wm = normalize(wi + wo);
  double denom = fmax(4 * fabs(dot(wo, normal) * dot(wi, normal)), EPS);
  Color fresnel = schlick_fresnel(color, wi, wm);
  return fresnel * ggx_g2(wi, wo, normal) * ggx_d(wm, normal) / denom;
}

Vec3 MicroBrdfAniso::sample(const Vec3 &wo, const Vec3 &normal, const bool into,
                            Random *rnd) const {
  Vec3 w = normal;
  Vec3 u = fabs(w.x) > EPS ? normalize(cross(Vec3(0, 1, 0), w))
                           : normalize(cross(Vec3(1, 0, 0), w));
  Vec3 v = cross(w, u);

  Vec3 ve = Vec3(dot(u, wo), dot(v, wo), dot(normal, wo));
  Vec3 vh = normalize(Vec3(alpha_x * ve.x, alpha_y * ve.y, ve.z));

  while (true) {
    double phi = 2.0 * PI * rnd->gen();
    double z = fma((1.0 - rnd->gen()), (1.0 + vh.z), -vh.z);
    double sin_theta = sqrt(clamp01(1.0 - z * z));
    double x = sin_theta * cos(phi);
    double y = sin_theta * sin(phi);
    Vec3 c = Vec3(x, y, z);
    Vec3 h = c + vh;

    Vec3 wm_tmp = Vec3(alpha_x * h.x, alpha_y * h.y, h.z);
    Vec3 wm = normalize(u * wm_tmp.x + v * wm_tmp.y + w * wm_tmp.z);
    Vec3 wi = reflection_dir(wo, wm);

    if (dot(wi, normal) > 0.) {
      return wi;
    }
  }
}

Color MixtureBrdf::eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                        const bool into) const {
  Vec3 wm = normalize(wi + wo);
  double denom = fmax(4 * fabs(dot(wo, normal) * dot(wi, normal)), EPS);
  Color fresnel = fresnel_gaussian(fs0(), wi, wm);
  Color f_spec = fresnel * ggx_g2(wi, wo, normal) * ggx_d(wm, normal) / denom;

  Color f_diff = base_color * PI_INV;

  return (1. - metalic) * f_diff + f_spec;
}

Vec3 MixtureBrdf::sample(const Vec3 &wo, const Vec3 &normal, const bool into,
                         Random *rnd) const {
  if (sample_diffuse(rnd)) {
    return sample_cosine_hemisphere(wo, normal, rnd);
  } else {
    return sample_vndf(wo, normal, rnd);
  }
}

double MixtureBrdf::sampling_pdf(const Vec3 &wo, const Vec3 &wi,
                                 const Vec3 &normal, const bool into) const {
  Vec3 wm = normalize(wi + wo);
  double pdf_spec =
      ggx_g1(wo, normal) * ggx_d(wm, normal) * 0.25 / fabs(dot(wo, normal));
  double pdf_diff = fmax(dot(wi, normal), EPS) * PI_INV;

  return diffuse_prob * pdf_diff + (1. - diffuse_prob) * pdf_spec;
}
