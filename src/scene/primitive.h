#pragma once

#include "renderer/material.h"
#include "renderer/ray.h"
#include "scene/aabb.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>

struct Vertex {
  Point3 position;
  Vec3 normal;

  Vertex() : position(Point3()), normal(Vec3(1, 0, 0)) {}

  Vertex(const Point3 &pos, const Vec3 &normal)
      : position(pos), normal(normalize(normal)) {}
};

class Shape {
public:
  virtual ~Shape() = default;
  virtual AABB get_bbox() const = 0;
  virtual bool hit(Ray &ray, HitRecord *record) const = 0;
  virtual Point3 get_center() const = 0;
  virtual double get_area() const = 0;
  virtual double pdf_sample_surface_area() const = 0;
  virtual Point3 sample_surface(Random *rnd, double *pdf) const = 0;
  virtual Vec3 get_normal(const Point3 &pos) const = 0;
};

class Triangle : public Shape {
public:
  Vertex p, q, r;
  Vec3 geometry_normal;

  Triangle(const Vertex &p, const Vertex &q, const Vertex &r)
      : p(p), q(q), r(r) {
    Vec3 pq = q.position - p.position;
    Vec3 pr = r.position - p.position;
    geometry_normal = normalize(cross(pq, pr));
  }

  AABB get_bbox() const override {
    return AABB(vmin(p.position, vmin(q.position, r.position)),
                vmax(p.position, vmax(q.position, r.position)))
        .fix();
  }

  bool hit(Ray &ray, HitRecord *record) const override {
    double dn = dot(ray.dir, geometry_normal);
    if (fabs(dn) < EPS) {
      return false;
    }

    double t = dot(geometry_normal, p.position - ray.org) / dn;
    if (t > record->distance || t < 0) {
      return false;
    }

    Point3 pos = ray.org + t * ray.dir;
    Vec3 p_pos = pos - p.position;
    Vec3 pq = q.position - p.position;
    Vec3 pr = r.position - p.position;

    double u = dot(cross(pr, p_pos), geometry_normal) /
               dot(cross(pr, pq), geometry_normal);
    double v = dot(cross(pq, p_pos), geometry_normal) /
               dot(cross(pq, pr), geometry_normal);
    if (u + v > 1. || u < 0 || v < 0) {
      return false;
    }

    record->distance = t;
    record->hitpoint = pos;
    record->normal = tlerp<Vec3>(p.normal, q.normal, r.normal, Vec2(u, v));

    return true;
  }

  Point3 get_center() const override {
    return (p.position + q.position + r.position) / 3.;
  }

  double get_area() const override {
    Vec3 pq = q.position - p.position;
    Vec3 pr = r.position - p.position;
    return 0.5 * cross(pq, pr).len();
  }

  double pdf_sample_surface_area() const override {
    Vec3 pq = q.position - p.position;
    Vec3 pr = r.position - p.position;
    return 1. / (cross(pq, pr).len() * 0.5);
  }

  Point3 sample_surface(Random *rnd, double *pdf) const override {
    Vec3 pq = q.position - p.position;
    Vec3 pr = r.position - p.position;

    double r1 = rnd->gen();
    double r2 = rnd->gen();
    if (r1 + r2 > 1.) {
      r1 = 1. - r1;
      r2 = 1. - r2;
    }

    *pdf = 1. / (cross(pq, pr).len() * 0.5);
    return p.position + pq * r1 + pr * r2;
  }

  Vec3 get_normal(const Point3 &pos) const override {
    return tlerp<Vec3>(p.normal, q.normal, r.normal, pos);
  }

private:
  Vec2 get_uv(const Point3 pos) const {
    Vec3 p_pos = pos - p.position;
    Vec3 pq = q.position - p.position;
    Vec3 pr = r.position - p.position;

    double u = dot(cross(pr, p_pos), geometry_normal) /
               dot(cross(pr, pq), geometry_normal);
    double v = dot(cross(pq, p_pos), geometry_normal) /
               dot(cross(pq, pr), geometry_normal);
    return Vec2(u, v);
  }

  template <typename T>
  inline T tlerp(const T pv, const T qv, const T rv, const Point3 &pos) const {
    Vec2 uv = get_uv(pos);
    return (1 - uv.u - uv.v) * pv + uv.u * qv + uv.v * rv;
  }

  template <typename T>
  inline T tlerp(const T pv, const T qv, const T rv, const Vec2 &uv) const {
    return (1 - uv.u - uv.v) * pv + uv.u * qv + uv.v * rv;
  }
};

class Sphere : public Shape {
public:
  Point3 center;
  double radius;

  Sphere(const Point3 &center, const double r) : center(center), radius(r) {}

  AABB get_bbox() const override {
    return AABB(center - Vec3(radius), center + Vec3(radius));
  }

  bool hit(Ray &ray, HitRecord *record) const override {
    Vec3 oc = center - ray.org;
    double b = dot(oc, ray.dir);
    double d = b * b - oc.len2() + radius * radius;

    if (d < 0) {
      return false;
    }

    double t1 = b - sqrt(d), t2 = b + sqrt(d), t;
    if (t1 > 0.) {
      t = t1;
    } else if (t2 > 0.) {
      t = t2;
    } else {
      return false;
    }

    if (t > record->distance) {
      return false;
    }

    record->distance = t;
    record->hitpoint = ray.org + ray.dir * t;
    record->normal = normalize(ray.dir * t - oc);
    return true;
  }

  Point3 get_center() const override { return center; }

  double get_area() const override { return 4. * PI * radius * radius; }

  double pdf_sample_surface_area() const override {
    // do not use
    return 1.;
  }

  Point3 sample_surface(Random *rnd, double *pdf) const override {
    // do not use
    return 1.;
  }

  Vec3 get_normal(const Point3 &pos) const override {
    return normalize(pos - center);
  }
};

class Primitive {
public:
  Shape *shape;
  Bxdf *bxdf;
  Color emission;
  uint32_t id;

  Primitive(Shape *shape, const uint32_t id)
      : shape(shape), emission(Color()), id(id) {}

  Primitive(Shape *shape, Bxdf *bxdf, const uint32_t id)
      : shape(shape), bxdf(bxdf), emission(Color()), id(id) {}

  Primitive(Shape *shape, Bxdf *bxdf, const Color &emission, const uint32_t id)
      : shape(shape), bxdf(bxdf), emission(emission), id(id) {}

  inline void hit(Ray &ray, HitRecord *record) const {
    if (ray.prev_id != id && shape->hit(ray, record)) {
      record->id = id;
    }
  }

  inline bool is_light() const { return emission.len() > EPS; }
};
