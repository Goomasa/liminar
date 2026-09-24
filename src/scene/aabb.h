#pragma once

#include "renderer/ray.h"
#include "util/utils.h"

struct AABB {
  Point3 bmin, bmax;

  AABB() : bmin(INF), bmax(-INF) {}
  AABB(const Point3 &bmin, const Point3 &bmax) : bmin(bmin), bmax(bmax) {}

  inline AABB operator+(const AABB &box) const {
    return AABB(vmin(bmin, box.bmin), vmax(bmax, box.bmax));
  }

  double get_area() const {
    Vec3 d = bmax - bmin;
    return 2. * (d.x * d.y + d.y * d.z + d.z * d.x);
  }

  inline Point3 get_center() const { return (bmin + bmax) * 0.5; }

  bool hit(Ray &ray, const HitRecord *record) const {
    double tmin = 0., tmax = INF;
    for (int a = 0; a < 3; a++) {
      double d_inv = 1.0f / ray.dir[a];
      double t0 = (bmin[a] - ray.org[a]) * d_inv;
      double t1 = (bmax[a] - ray.org[a]) * d_inv;

      if (d_inv < 0.) {
        std::swap(t0, t1);
      }

      tmin = t0 > tmin ? t0 : tmin;
      tmax = t1 < tmax ? t1 : tmax;
      if (tmax <= tmin || record->distance < tmin)
        return false;
    }
    return true;
  }

  AABB fix() const {
    Point3 bmin_ = bmin, bmax_ = bmax;
    for (uint8_t i = 0; i < 3; i++) {
      if (bmin[i] == bmax[i]) {
        bmin_.set(bmin[i] - 0.01, i);
        bmax_.set(bmax[i] + 0.01, i);
      }
    }
    return AABB(bmin_, bmax_);
  }
};
