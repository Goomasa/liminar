#pragma once

#include "renderer/material.h"
#include "renderer/ray.h"
#include "scene/aabb.h"
#include "util/utils.h"
#include <cstdint>

struct Vertex {
  Point3 position;
  Vec3 normal;
  Vec2 uv;

  Vertex() : position(Point3()), normal(Vec3(1, 0, 0)) {}

  Vertex(const Point3 &pos, const Vec3 &normal)
      : position(pos), normal(normalize(normal)), uv(0.5) {}

  Vertex(const Point3 &pos, const Vec3 &normal, const Vec2 &uv)
      : position(pos), normal(normal), uv(uv) {}
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
  virtual Vec2 get_uv(const Point3 &pos) const = 0;
};

class Triangle : public Shape {
public:
  Vertex p, q, r;
  Vec3 geometry_normal;

  Triangle(const Vec3 &vp, const Vec3 &vq, const Vec3 &vr) {
    geometry_normal = normalize(cross(vq - vp, vr - vp));
    p = Vertex(vp, geometry_normal);
    q = Vertex(vq, geometry_normal);
    r = Vertex(vr, geometry_normal);
  }

  Triangle(const Vertex &vp, const Vertex &vq, const Vertex &vr)
      : p(vp), q(vq), r(vr) {
    Vec3 pq = vq.position - vp.position;
    Vec3 pr = vr.position - vp.position;
    geometry_normal = normalize(cross(pq, pr));

    if (vp.normal.is_nan() || vq.normal.is_nan() || vr.normal.is_nan()) {
      p.normal = geometry_normal;
      q.normal = geometry_normal;
      r.normal = geometry_normal;
    }
  }

  AABB get_bbox() const override {
    return AABB(vmin(p.position, vmin(q.position, r.position)),
                vmax(p.position, vmax(q.position, r.position)))
        .fix();
  }

  bool hit(Ray &ray, HitRecord *record) const override;

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

  Point3 sample_surface(Random *rnd, double *pdf) const override;

  Vec3 get_normal(const Point3 &pos) const override {
    return normalize(tlerp<Vec3>(p.normal, q.normal, r.normal, pos));
  }

  Vec2 get_geometry_uv(const Point3 &pos) const {
    Vec3 p_pos = pos - p.position;
    Vec3 pq = q.position - p.position;
    Vec3 pr = r.position - p.position;

    double u = dot(cross(pr, p_pos), geometry_normal) /
               dot(cross(pr, pq), geometry_normal);
    double v = dot(cross(pq, p_pos), geometry_normal) /
               dot(cross(pq, pr), geometry_normal);
    return Vec2(u, v);
  }

  Vec2 get_uv(const Point3 &pos) const override {
    // todo: normal map
    Vec2 geom_uv = get_geometry_uv(pos);
    return tlerp<Vec2>(p.uv, q.uv, r.uv, geom_uv);
  }

private:
  template <typename T>
  inline T tlerp(const T pv, const T qv, const T rv, const Point3 &pos) const {
    Vec2 uv = get_geometry_uv(pos);
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

  bool hit(Ray &ray, HitRecord *record) const override;

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

  Vec2 get_uv(const Vec3 &pos) const override {
    // wip
    return Vec2(0.5);
  }
};

class Cylinder : public Shape {
public:
  Point3 bottom;
  Vec3 dir;
  double radius, height;

  Cylinder(const Point3 &bottom, const Vec3 &dir, const double r,
           const double h)
      : bottom(bottom), dir(normalize(dir)), radius(r), height(h) {}

  AABB get_bbox() const override {
    Point3 top = bottom + height * dir;
    return AABB(bottom - Vec3(radius), bottom + Vec3(radius)) +
           AABB(top - Vec3(radius), top + Vec3(radius));
  }

  bool hit(Ray &ray, HitRecord *record) const override;

  Point3 get_center() const override { return bottom + dir * height * 0.5; }

  double get_area() const override { return 2. * PI * radius * height; }

  double pdf_sample_surface_area() const override {
    // do not use
    return 1.;
  }

  Point3 sample_surface(Random *rnd, double *pdf) const override {
    // do not use
    return 1.;
  }

  Vec3 get_normal(const Point3 &pos) const override {
    double k = dot(dir, pos - bottom);
    return normalize(pos - bottom - k * dir);
  }

  Vec2 get_uv(const Vec3 &pos) const override { return Vec2(0.5); }
};

class Disk : public Shape {
public:
  Point3 center;
  Vec3 normal;
  double radius;

  Disk(const Point3 &c, const Vec3 &n, const double r)
      : center(c), normal(normalize(n)), radius(r) {}

  AABB get_bbox() const override {
    return AABB(center - Vec3(radius), center + Vec3(radius)).fix();
  }

  bool hit(Ray &ray, HitRecord *record) const override;

  Point3 get_center() const override { return center; }

  double get_area() const override { return PI * radius * radius; }

  double pdf_sample_surface_area() const override {
    return 1. / (PI * radius * radius);
  }

  Point3 sample_surface(Random *rnd, double *pdf) const override;

  Vec3 get_normal(const Point3 &pos) const override { return normal; }

  Vec2 get_uv(const Vec3 &pos) const override { return Vec2(0.5); }
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
    if (shape->hit(ray, record)) {
      record->id = id;
    }
  }

  inline bool is_light() const { return emission.len() > EPS; }
};
