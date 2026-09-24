#include "primitive.h"
#include "renderer/ray.h"
#include "util/random.h"
#include "util/utils.h"
#include <cmath>

bool Triangle::hit(Ray &ray, HitRecord *record) const {
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

  double geom_u = dot(cross(pr, p_pos), geometry_normal) /
                  dot(cross(pr, pq), geometry_normal);
  double geom_v = dot(cross(pq, p_pos), geometry_normal) /
                  dot(cross(pq, pr), geometry_normal);
  if (geom_u + geom_v > 1. || geom_u < 0 || geom_v < 0) {
    return false;
  }

  record->distance = t;
  record->hitpoint = pos;
  record->normal = normalize(
      tlerp<Vec3>(p.normal, q.normal, r.normal, Vec2(geom_u, geom_v)));
  record->geometry_normal = geometry_normal;
  record->uv = tlerp<Vec2>(p.uv, q.uv, r.uv, Vec2(geom_u, geom_v));

  return true;
}

Point3 Triangle::sample_surface(Random *rnd, double *pdf) const {
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

bool Sphere::hit(Ray &ray, HitRecord *record) const {
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
  record->geometry_normal = record->normal;
  record->uv = get_texcoord(record->hitpoint);
  return true;
}

bool Cylinder::hit(Ray &ray, HitRecord *record) const {
  Vec3 a = dot(ray.dir, dir) * dir - ray.dir;

  if (a.len2() < EPS) {
    return false;
  }

  Vec3 b = bottom + dot(ray.org, dir) * dir - dot(bottom, dir) * dir - ray.org;
  double ab = dot(a, b);
  double d = ab * ab - a.len2() * (b.len2() - radius * radius);

  if (d < 0.) {
    return false;
  }

  double t1 = (-ab - sqrt(d)) / a.len2(), t2 = (-ab + sqrt(d)) / a.len2(), t;
  auto check_range = [this, &ray](double t) -> bool {
    double k = dot(dir, t * ray.dir + ray.org - bottom);
    return k >= 0 && k <= height;
  };

  if (t1 > 0. && check_range(t1)) {
    t = t1;
  } else if (t2 > 0. && check_range(t2)) {
    t = t2;
  } else {
    return false;
  }

  if (t > record->distance) {
    return false;
  }

  record->distance = t;
  record->hitpoint = ray.org + ray.dir * t;
  record->normal = get_normal(record->hitpoint);
  record->geometry_normal = record->normal;
  record->uv = get_texcoord(record->hitpoint);
  return true;
}

bool Disk::hit(Ray &ray, HitRecord *record) const {
  double dn = dot(ray.dir, normal);
  if (fabs(dn) < EPS) {
    return false;
  }

  double t = dot(normal, center - ray.org) / dn;
  Vec3 pos = ray.org + t * ray.dir;
  if (t < 0 || t > record->distance || (center - pos).len() > radius) {
    return false;
  }

  record->distance = t;
  record->hitpoint = pos;
  record->normal = normal;
  record->geometry_normal = record->normal;
  record->uv = get_texcoord(pos);
  return true;
}

Point3 Disk::sample_surface(Random *rnd, double *pdf) const {
  double theta = 2. * PI * rnd->gen();
  double r = rnd->gen() * radius;

  Vec3 u = fabs(normal.x) > EPS ? normalize(cross(Vec3(0, 1, 0), normal))
                                : normalize(cross(Vec3(1, 0, 0), normal));
  Vec3 v = cross(normal, u);

  *pdf = 1. / (PI * radius * radius);
  return center + u * r * cos(theta) + v * r * sin(theta);
}

bool Square::hit(Ray &ray, HitRecord *record) const {
  if (fabs(ray.dir[vertical_axis]) < EPS) {
    return false;
  }

  double t =
      (minp[vertical_axis] - ray.org[vertical_axis]) / ray.dir[vertical_axis];
  if (t > record->distance) {
    return false;
  }

  Point3 pos = ray.dir * t + ray.org;
  uint8_t a1 = (vertical_axis + 1) % 3, a2 = (vertical_axis + 2) % 3;

  if (pos[a1] < minp[a1] || pos[a1] > maxp[a1] || pos[a2] < minp[a2] ||
      pos[a2] > maxp[a2]) {
    return false;
  }

  record->distance = t;
  record->hitpoint = pos;
  record->geometry_normal = normal;
  record->normal = normal;
  record->uv = get_texcoord(pos);
  return true;
}
