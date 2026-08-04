#include "material.h"
#include "util/utils.h"
#include <cmath>

Vec3 Lambertian::sample(const Vec3 &wo, const Vec3 &normal, const bool into,
                        Random *rnd) const {
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

double Lambertian::sampling_pdf(const Vec3 &wi, const Vec3 &normal) const {
  return fmax(dot(wi, normal), EPS) * PI_INV;
}

Color Glass::eval(const Vec3 &wo, const Vec3 &wi, const Vec3 &normal,
                  const bool into) const {
  if (dot(wi, normal) < 0.) {
    // refract
    double r = into ? 1. / ior : ior;
    return r * r * color / fabs(dot(wi, normal));
  } else {
    // reflect
    return color / fabs(dot(wi, normal));
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

  double reflectance = schlick_fresnel(wo, normal, ior_i, ior_o);
  if (rnd->gen() < reflectance) {
    return reflection_dir(wo, normal);
  } else {
    return refraction_dir(wo, normal, ior_o, ior_i);
  }
}
