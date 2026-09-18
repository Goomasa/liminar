#pragma once

#include "util/utils.h"
#include <cstdint>

struct Ray {
  Point3 org;
  Vec3 dir;
  int32_t prev_id;

  Ray(const Point3 &org, const Vec3 &dir, const uint32_t prev = -1)
      : org(org), dir(dir), prev_id(prev) {}
};

struct HitRecord {
  double distance;
  Point3 hitpoint;
  Vec3 normal;
  Vec2 uv;
  uint32_t id;

  HitRecord(const double d = INF)
      : distance(d), hitpoint(Point3()), normal(Vec3()), uv(Vec2(0.5)), id(-1) {
  }

  void reset(const double d = INF) {
    distance = d;
    id = -1;
  }
};
