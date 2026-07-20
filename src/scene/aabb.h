#pragma once

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
};
