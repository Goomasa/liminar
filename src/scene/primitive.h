#pragma once

#include "renderer/material.h"
#include "renderer/ray.h"
#include "scene/aabb.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>

class Triangle {
public:
  Point3 p, q, r;
  Vec3 normal; // wip: normals of each vertices

  Triangle(const Point3 &p, const Point3 &q, const Point3 &r)
      : p(p), q(q), r(r) {
    normal = normalize(cross(q - p, r - p));
  }

  Triangle(const Point3 &p, const Point3 &q, const Point3 &r,
           const Vec3 &normal)
      : p(p), q(q), r(r), normal(normal) {}

  AABB get_bbox() const {
    return AABB(vmin(p, vmin(q, r)), vmax(p, vmax(q, r)));
  }

  bool hit(Ray &ray, HitRecord *record) const {
    double dn = dot(ray.dir, normal);
    if (fabs(dn) < EPS) {
      return false;
    }

    double t = dot(normal, p - ray.org) / dn;
    if (t > record->distance || t < 0) {
      return false;
    }

    Point3 pos = ray.org + t * ray.dir;
    Vec3 p_pos = pos - p, pq = q - p, pr = r - p;
    double u = dot(cross(pr, p_pos), normal) / dot(cross(pr, pq), normal);
    double v = dot(cross(pq, p_pos), normal) / dot(cross(pq, pr), normal);
    if (u + v > 1. || u < 0 || v < 0) {
      return false;
    }

    record->distance = t;
    record->hitpoint = pos;
    record->normal = normal;

    return true;
  }
};

class Primitive {
public:
  Triangle shape;
  Bxdf *bxdf;
  Color emission;
  uint32_t id;

  Primitive(Triangle shape, const uint32_t id)
      : shape(shape), emission(Color()), id(id) {}

  Primitive(Triangle shape, Bxdf *bxdf, const uint32_t id)
      : shape(shape), bxdf(bxdf), emission(Color()), id(id) {}

  Primitive(Triangle shape, Bxdf *bxdf, const Color &emission,
            const uint32_t id)
      : shape(shape), bxdf(bxdf), emission(emission), id(id) {}

  void hit(Ray &ray, HitRecord *record) const {
    if (shape.hit(ray, record)) {
      record->id = id;
    }
  }

  bool is_light() const { return emission.len() > EPS; }
};
