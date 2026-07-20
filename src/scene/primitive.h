#pragma once

#include "renderer/ray.h"
#include "scene/aabb.h"
#include "util/utils.h"
#include <cmath>

class Shape {
public:
  virtual bool hit(const Ray &ray, HitRecord *record) const = 0;
  virtual AABB get_bbox() const = 0;
};

class Sphere : public Shape {
public:
  double radius;
  Point3 center;

  Sphere(const double radius, const Point3 &center)
      : radius(radius), center(center) {}

  bool hit(const Ray &ray, HitRecord *record) const override {
    Vec3 oc = center - ray.org;
    double b = dot(oc, ray.dir);
    double disc = b * b - oc.len2() + radius * radius;

    if (disc < 0.) {
      return false;
    }

    double t1 = b - sqrt(disc), t2 = b + sqrt(disc), t;

    if (t1 > 0.) {
      t = t1;
    } else if (t2 > 0.) {
      t = t2;
    } else {
      return false;
    }

    record->distance = t;
    record->hitpoint = ray.org + ray.dir * t;
    record->normal = normalize(ray.dir * t - oc);

    return true;
  }

  AABB get_bbox() const override {
    return AABB(center - Vec3(radius), center + Vec3(radius));
  }
};
