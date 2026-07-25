#include "material.h"
#include "util/utils.h"
#include <cmath>

Vec3 Lambertian::sample(const Vec3 &in, const Vec3 &normal, Random *rnd) const {
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

double Lambertian::sampling_pdf(const Vec3 &out, const Vec3 &normal) const {
  return fmax(dot(out, normal), 0.) * PI_INV;
}
