#pragma once

#include "renderer/material.h"
#include "renderer/medium.h"
#include "renderer/ray.h"
#include "scene/aabb.h"
#include "scene/texture.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>

struct Vertex {
  Point3 position;
  Vec3 normal, tangent, bitangent;
  Vec2 uv;

  Vertex()
      : position(Point3()), normal(Vec3(1, 0, 0)), tangent(Vec3(0, 1, 0)),
        bitangent(Vec3(0, 0, 1)) {}

  Vertex(const Point3 &pos, const Vec3 &normal)
      : position(pos), normal(normalize(normal)), uv(0.5) {
    tangent = Vec3(0, 1, 0);
    bitangent = cross(normal, tangent);
  }

  Vertex(const Point3 &pos, const Vec3 &normal, const Vec2 &uv)
      : position(pos), normal(normal), uv(uv) {
    tangent = Vec3(0, 1, 0);
    bitangent = cross(normal, tangent);
  }

  Vertex(const Point3 &pos, const Vec3 &normal, const Vec3 &tangent,
         const Vec3 &bitangent, const Vec2 &uv)
      : position(pos), normal(normal), tangent(tangent), bitangent(bitangent),
        uv(uv) {}
};

class Shape {
public:
  double area;

  virtual ~Shape() = default;
  virtual AABB get_bbox() const = 0;
  virtual bool hit(Ray &ray, HitRecord *record) const = 0;
  virtual Point3 get_center() const = 0;
  virtual Point3 sample_surface(Random *rnd, double *pdf) const = 0;
  virtual Vec3 get_normal(const Point3 &pos) const = 0;
  virtual Vec2 get_texcoord(const Point3 &pos) const = 0;

  inline double pdf_sample_surface_area() const { return 1. / area; }
};

class Triangle : public Shape {
public:
  Vertex p, q, r;
  Vec3 geometry_normal;
  Texture *normal_map;

  Triangle(const Vec3 &vp, const Vec3 &vq, const Vec3 &vr,
           Texture *map = nullptr)
      : normal_map(map) {
    geometry_normal = normalize(cross(vq - vp, vr - vp));
    p = Vertex(vp, geometry_normal);
    q = Vertex(vq, geometry_normal);
    r = Vertex(vr, geometry_normal);

    area = 0.5 * cross(vq - vp, vr - vp).len();
  }

  Triangle(const Vertex &vp, const Vertex &vq, const Vertex &vr,
           Texture *map = nullptr)
      : p(vp), q(vq), r(vr), normal_map(map) {
    Vec3 pq = vq.position - vp.position;
    Vec3 pr = vr.position - vp.position;
    geometry_normal = normalize(cross(pq, pr));

    if (vp.normal.is_nan() || vq.normal.is_nan() || vr.normal.is_nan()) {
      p.normal = geometry_normal;
      q.normal = geometry_normal;
      r.normal = geometry_normal;
      normal_map = nullptr;
    }

    area = 0.5 * cross(pq, pr).len();
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

  Vec2 get_texcoord(const Point3 &pos) const override {
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

  inline Vec3 get_normal_from_uv(const Vec2 &geom_uv,
                                 const Vec2 &texcoord) const {
    if (normal_map) {
      Vec3 w = normalize(tlerp<Vec3>(p.normal, q.normal, r.normal, texcoord));
      Vec3 u =
          normalize(tlerp<Vec3>(p.tangent, q.tangent, r.tangent, texcoord));
      Vec3 v = normalize(
          tlerp<Vec3>(p.bitangent, q.bitangent, r.bitangent, texcoord));
      Vec3 map_col = normal_map->get_color(texcoord) * 2. - 1.;

      return normalize(u * map_col.x + v * map_col.y + w * map_col.z);
    } else {
      return normalize(tlerp<Vec3>(p.normal, q.normal, r.normal, geom_uv));
    }
  }
};

class Sphere : public Shape {
public:
  Point3 center;
  double radius;

  Sphere(const Point3 &center, const double r) : center(center), radius(r) {
    area = 4. * PI * radius * radius;
  }

  AABB get_bbox() const override {
    return AABB(center - Vec3(radius), center + Vec3(radius));
  }

  bool hit(Ray &ray, HitRecord *record) const override;

  Point3 get_center() const override { return center; }

  Point3 sample_surface(Random *rnd, double *pdf) const override {
    double phi = 2. * PI * rnd->gen();
    double cos_theta = -2. * rnd->gen() + 1.;
    double sin_theta = sqrt(1. - cos_theta * cos_theta);
    Vec3 d =
        normalize(Vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta));

    *pdf = 1. / area;
    return center + d;
  }

  Vec3 get_normal(const Point3 &pos) const override {
    return normalize(pos - center);
  }

  Vec2 get_texcoord(const Vec3 &pos) const override {
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
      : bottom(bottom), dir(normalize(dir)), radius(r), height(h) {
    area = 2. * PI * radius * height;
  }

  AABB get_bbox() const override {
    Point3 top = bottom + height * dir;
    return AABB(bottom - Vec3(radius), bottom + Vec3(radius)) +
           AABB(top - Vec3(radius), top + Vec3(radius));
  }

  bool hit(Ray &ray, HitRecord *record) const override;

  Point3 get_center() const override { return bottom + dir * height * 0.5; }

  Point3 sample_surface(Random *rnd, double *pdf) const override {
    *pdf = 1. / area;

    Vec3 u, v;
    if (fabs(dir.x) > EPS) {
      u = normalize(cross(Vec3(0, 1, 0), dir));
    } else {
      u = normalize(cross(Vec3(1, 0, 0), dir));
    }
    v = cross(dir, u);

    double phi = 2. * rnd->gen() * PI;
    double h = height * rnd->gen();
    Vec3 d = radius * normalize(u * cos(phi) + v * sin(phi));

    return bottom + d + h * dir;
  }

  Vec3 get_normal(const Point3 &pos) const override {
    double k = dot(dir, pos - bottom);
    return normalize(pos - bottom - k * dir);
  }

  Vec2 get_texcoord(const Vec3 &pos) const override { return Vec2(0.5); }
};

class Disk : public Shape {
public:
  Point3 center;
  Vec3 normal;
  double radius;

  Disk(const Point3 &c, const Vec3 &n, const double r)
      : center(c), normal(normalize(n)), radius(r) {
    area = PI * radius * radius;
  }

  AABB get_bbox() const override {
    return AABB(center - Vec3(radius), center + Vec3(radius)).fix();
  }

  bool hit(Ray &ray, HitRecord *record) const override;

  Point3 get_center() const override { return center; }

  Point3 sample_surface(Random *rnd, double *pdf) const override;

  Vec3 get_normal(const Point3 &pos) const override { return normal; }

  Vec2 get_texcoord(const Vec3 &pos) const override { return Vec2(0.5); }
};

// for modeling lobby
class Square : public Shape {
public:
  Point3 minp, maxp;
  Vec3 normal;
  uint8_t vertical_axis; // 0 -> x, 1 -> y, 2 -> z
  double l1, l2, tex_len;

  Square(const Point3 &minp_, const Point3 &maxp_, const Vec3 &normal,
         const uint8_t axis, const double tex_len)
      : normal(normalize(normal)), vertical_axis(axis), tex_len(tex_len) {
    minp = vmin(minp_, maxp_);
    maxp = vmax(minp_, maxp_);
    maxp.set(minp[vertical_axis], vertical_axis);

    l1 = (maxp - minp)[(vertical_axis + 1) % 3];
    l2 = (maxp - minp)[(vertical_axis + 2) % 3];

    area = l1 * l2;
  }

  AABB get_bbox() const override { return AABB(minp, maxp).fix(); }

  bool hit(Ray &ray, HitRecord *record) const override;

  Point3 get_center() const override { return (minp + maxp) * 0.5; }

  Point3 sample_surface(Random *rnd, double *pdf) const override {
    *pdf = 1. / area;
    double du = l1 * rnd->gen(), dv = l2 * rnd->gen();
    if (vertical_axis == 0) {
      return minp + Vec3(0, du, dv);
    } else if (vertical_axis == 1) {
      return minp + Vec3(dv, 0, du);
    } else {
      return minp + Vec3(du, dv, 0);
    }
  }

  Vec3 get_normal(const Point3 &pos) const override { return normal; }

  Vec2 get_texcoord(const Vec3 &pos) const override;
};

class Primitive {
public:
  Shape *shape;
  Bxdf *bxdf;
  Color emission;
  Medium *medium;
  uint32_t id;

  Primitive(Shape *shape, const uint32_t id, Medium *medium = nullptr)
      : shape(shape), emission(Color()), medium(medium), id(id) {}

  Primitive(Shape *shape, Bxdf *bxdf, const uint32_t id,
            Medium *medium = nullptr)
      : shape(shape), bxdf(bxdf), emission(Color()), medium(medium), id(id) {}

  Primitive(Shape *shape, Bxdf *bxdf, const Color &emission, const uint32_t id,
            Medium *medium = nullptr)
      : shape(shape), bxdf(bxdf), emission(emission), medium(medium), id(id) {}

  inline void hit(Ray &ray, HitRecord *record) const {
    if (shape->hit(ray, record)) {
      record->id = id;
    }
  }

  inline bool is_light() const { return emission.len() > EPS; }
};
