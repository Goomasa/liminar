#pragma once

#include "util/utils.h"

struct Ray {
  Point3 org;
  Vec3 dir;

  Ray(const Point3 &org, const Vec3 &dir) : org(org), dir(dir) {}
};
