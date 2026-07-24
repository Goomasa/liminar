#pragma once

#include "util/utils.h"
#include <cstdint>

struct Ray {
  Point3 org;
  Vec3 dir;

  Ray(const Point3 &org, const Vec3 &dir) : org(org), dir(dir) {}
};

struct HitRecord {
  double distance;
  Point3 hitpoint;
  Vec3 normal;
  uint32_t id;

  HitRecord() : distance(INF), hitpoint(Point3()), normal(Vec3()), id(-1) {}

  void reset() {
    distance = INF;
    id = -1;
  }
};
