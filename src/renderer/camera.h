#pragma once

#include "renderer/ray.h"
#include "util/random.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>

constexpr double W_SCALE_COEFF = 0.05;

class Camera {
public:
  uint32_t resolution_w, resolution_h;
  Vec3 sensor_dir;

  Camera(uint32_t reso_w, uint32_t reso_h, const Vec3 &sensor_dir_)
      : resolution_w(reso_w), resolution_h(reso_h),
        sensor_dir(normalize(sensor_dir_)) {}
  virtual ~Camera() = default;
  virtual double get_pt_coefficient(const Ray &ray,
                                    const Point3 &pixel_pos) const = 0;
  virtual Ray get_first_ray(Random *rnd, const Point3 &pixel_pos) const = 0;
  virtual Point3 get_pixel_pos(uint32_t px_u, uint32_t px_v, uint32_t spx_u,
                               uint32_t spx_v, uint32_t superpx_num,
                               Random *rnd) const = 0;

  virtual double get_bpt_coefficient(const Point3 &px_pos,
                                     const Point3 &lens_pos,
                                     const Point3 &objplane_pos) const = 0;
  virtual double pdf_sample_x1(const Point3 &px_pos, const Point3 &lens_pos,
                               const Point3 &objplane_pos, const Point3 &x1,
                               const Vec3 &normal) const = 0;

  virtual double pdf_sample_lens_hole() const = 0;
  virtual bool intersect_lens(Ray &ray, Point3 *px_pos, Point3 *lens_pos,
                              Point3 *objplane_pos, uint32_t *px_u,
                              uint32_t *px_v) const = 0;
  virtual Point3 get_objplane_position(const Point3 &lens_pos,
                                       const Point3 &x1) const = 0;
};

class PinholeModel : public Camera {
public:
  PinholeModel(const uint32_t reso_w, const uint32_t reso_h, const Vec3 &up,
               const Point3 &sensor_center, const double sensor_w,
               const Vec3 &sensor_dir_, const double sensor_to_hole,
               const double w_scale)
      : Camera(reso_w, reso_h, sensor_dir_), sensor_center(sensor_center),
        sensor_width(sensor_w) {
    sensor_height = sensor_w * ((double)resolution_h / (double)resolution_w);
    hole_pos = sensor_center + sensor_dir * sensor_to_hole;

    pixel_width = sensor_width / (double)resolution_w;
    pixel_height = sensor_height / (double)resolution_h;

    sensor_u = normalize(cross(sensor_dir, up));
    sensor_v = normalize(cross(sensor_dir, sensor_u));
    sensor_corner = sensor_center - sensor_u * sensor_width / 2. -
                    sensor_v * sensor_height / 2.;

    sensitivity = w_scale * W_SCALE_COEFF * (sensor_center - hole_pos).len2() /
                  (pixel_width * pixel_height);
  }

  double get_pt_coefficient(const Ray &ray,
                            const Point3 &pixel_pos) const override {
    double pixel_area = pixel_width * pixel_height;
    double g_term = pow(dot(sensor_dir, normalize(hole_pos - pixel_pos)), 2) /
                    (hole_pos - pixel_pos).len2();

    return sensitivity * g_term * pixel_area;
  }

  Ray get_first_ray(Random *rnd, const Point3 &pixel_pos) const override {
    Vec3 dir = normalize(hole_pos - pixel_pos);
    return Ray(hole_pos, dir);
  }

  Point3 get_pixel_pos(uint32_t px_u, uint32_t px_v, uint32_t spx_u,
                       uint32_t spx_v, uint32_t superpx_num,
                       Random *rnd) const override {
    double du = pixel_width *
                (px_u + ((double)spx_u + rnd->gen()) / (double)superpx_num);
    double dv = pixel_height *
                (px_v + ((double)spx_v + rnd->gen()) / (double)superpx_num);
    return sensor_corner + sensor_u * du + sensor_v * dv;
  }

  double get_bpt_coefficient(const Point3 &px_pos, const Point3 &lens_pos,
                             const Point3 &objplane_pos) const override {
    return sensitivity;
  }

  double pdf_sample_x1(const Point3 &px_pos, const Point3 &lens_pos,
                       const Point3 &objplane_pos, const Point3 &x1,
                       const Vec3 &normal) const override {
    // lens_pos=hole_pos
    double pdf_sample_px = 1. / (pixel_width * pixel_height);
    double cos1 = dot(sensor_dir, normalize(hole_pos - px_pos));
    double cos2 = dot(normal, normalize(hole_pos - x1));
    return pdf_sample_px * (cos2 / cos1) * (hole_pos - px_pos).len2() /
           (x1 - hole_pos).len2();
  }

  double pdf_sample_lens_hole() const override { return 1.; }

  bool intersect_lens(Ray &ray, Point3 *px_pos, Point3 *lens_pos,
                      Point3 *objplane_pos, uint32_t *px_u,
                      uint32_t *px_v) const override {
    return false;
  }

  Point3 get_objplane_position(const Point3 &lens_pos,
                               const Point3 &x1) const override {
    return INVALID_VEC;
  }

private:
  Point3 sensor_center, sensor_corner;
  double sensor_width, sensor_height;
  double pixel_width, pixel_height;
  Vec3 sensor_u, sensor_v;

  Point3 hole_pos;
  double sensitivity;
};

class LensModel : public Camera {
public:
  LensModel(const uint32_t reso_w, const uint32_t reso_h, const Vec3 &up,
            const Point3 &sensor_center, const double sensor_w,
            const Vec3 &sensor_dir_, const double sensor_to_lens,
            const double lens_radius, const double focal_dist,
            const double w_scale)
      : Camera(reso_w, reso_h, sensor_dir_), sensor_center(sensor_center),
        sensor_width(sensor_w), sensor_to_lens(sensor_to_lens),
        lens_radius(lens_radius), focal_distance(focal_dist) {
    sensor_height = sensor_w * ((double)resolution_h / (double)resolution_w);

    pixel_width = sensor_width / (double)resolution_w;
    pixel_height = sensor_height / (double)resolution_h;

    sensor_u = normalize(cross(sensor_dir, up));
    sensor_v = normalize(cross(sensor_dir, sensor_u));
    sensor_corner = sensor_center - sensor_u * sensor_width / 2. -
                    sensor_v * sensor_height / 2.;

    lens_center = sensor_center + sensor_dir * sensor_to_lens;

    sensitivity = w_scale * W_SCALE_COEFF *
                  (sensor_center - lens_center).len2() /
                  (pixel_width * pixel_height);
  }

  double get_pt_coefficient(const Ray &ray,
                            const Point3 &pixel_pos) const override {
    double lens_area = PI * lens_radius * lens_radius;
    double pixel_area = pixel_width * pixel_height;
    double g_term = pow(dot(sensor_dir, normalize(ray.org - pixel_pos)), 2) /
                    (ray.org - pixel_pos).len2();

    return sensitivity * g_term * lens_area * pixel_area;
  }

  Ray get_first_ray(Random *rnd, const Point3 &pixel_pos) const override {
    Point3 lens_pos = sample_lens(rnd);
    Point3 objplane_pos = lens_center + (lens_center - pixel_pos) *
                                            (focal_distance / sensor_to_lens);
    Vec3 dir = normalize(objplane_pos - lens_pos);

    return Ray(lens_pos, dir);
  }

  Point3 get_pixel_pos(uint32_t px_u, uint32_t px_v, uint32_t spx_u,
                       uint32_t spx_v, uint32_t superpx_num,
                       Random *rnd) const override {
    double du = pixel_width *
                (px_u + ((double)spx_u + rnd->gen()) / (double)superpx_num);
    double dv = pixel_height *
                (px_v + ((double)spx_v + rnd->gen()) / (double)superpx_num);
    return sensor_corner + sensor_u * du + sensor_v * dv;
  }

  double get_bpt_coefficient(const Point3 &px_pos, const Point3 &lens_pos,
                             const Point3 &objplane_pos) const override {
    double cos1 = dot(sensor_dir, normalize(lens_pos - px_pos));
    double cos2 = dot(sensor_dir, normalize(objplane_pos - lens_pos));
    return sensitivity *
           pow(sensor_to_lens * cos1 / (focal_distance * cos2), 2) *
           (objplane_pos - lens_pos).len2() / (lens_pos - px_pos).len2();
  }

  double pdf_sample_x1(const Point3 &px_pos, const Point3 &lens_pos,
                       const Point3 &objplane_pos, const Point3 &x1,
                       const Vec3 &normal) const override {
    // lens_pos=hole_pos
    double pdf_sample_px = 1. / (pixel_width * pixel_height);
    double cos1 = dot(sensor_dir, normalize(x1 - lens_pos));
    double cos2 = dot(normal, normalize(lens_pos - x1));
    double r =
        (sensor_to_lens * sensor_to_lens) / (focal_distance * focal_distance);
    return pdf_sample_px * r * (cos2 / cos1) *
           (objplane_pos - lens_pos).len2() / (x1 - lens_pos).len2();
  }

  double pdf_sample_lens_hole() const override {
    return 1. / (PI * lens_radius * lens_radius);
  }

  bool intersect_lens(Ray &ray, Point3 *px_pos, Point3 *lens_pos,
                      Point3 *objplane_pos, uint32_t *px_u,
                      uint32_t *px_v) const override {
    double lens_t = intersect_plane(ray, lens_center, sensor_dir);
    if (lens_t < EPS) {
      return false;
    }

    *lens_pos = ray.org + ray.dir * lens_t;
    if ((*lens_pos - lens_center).len() > lens_radius ||
        dot(sensor_dir, ray.dir) > 0.) {
      return false;
    }

    Vec3 objplane_center = lens_center + sensor_dir * focal_distance;
    double objplane_t = intersect_plane(ray, objplane_center, sensor_dir);
    if (fabs(objplane_t) < EPS) {
      return false;
    }

    *objplane_pos = ray.org + ray.dir * objplane_t;

    *px_pos = lens_center +
              (lens_center - *objplane_pos) * (sensor_to_lens / focal_distance);
    Vec3 cp = *px_pos - sensor_corner;
    double u = dot(cp, sensor_u), v = dot(cp, sensor_v);
    if (u < 0. || v < 0. || u > sensor_width || v > sensor_height) {
      return false;
    }

    *px_u = static_cast<uint32_t>(u / pixel_width);
    *px_v = static_cast<uint32_t>(v / pixel_height);

    return true;
  }

  Point3 get_objplane_position(const Point3 &lens_pos,
                               const Point3 &x1) const override {
    Ray to_objplane = Ray(lens_pos, normalize(x1 - lens_pos));
    Vec3 objplane_center = lens_center + sensor_dir * focal_distance;
    double objplane_t =
        intersect_plane(to_objplane, objplane_center, sensor_dir);
    return lens_pos + to_objplane.dir * objplane_t;
  }

private:
  Point3 sensor_center, sensor_corner;
  double sensor_width, sensor_height;
  double pixel_width, pixel_height;
  Vec3 sensor_u, sensor_v;

  double sensor_to_lens;
  Point3 lens_center;
  double lens_radius;

  double focal_distance;

  double sensitivity;

  inline Point3 sample_lens(Random *rnd) const {
    double theta = 2. * PI * rnd->gen();
    double r = sqrt(rnd->gen()) * lens_radius;
    return lens_center + sensor_u * r * cos(theta) + sensor_v * r * sin(theta);
  }

  inline double intersect_plane(Ray &ray, const Point3 &pos_on_plane,
                                const Vec3 &normal) const {
    double dn = dot(ray.dir, normal);
    if (fabs(dn) < EPS) {
      return 0.;
    }

    return dot(normal, pos_on_plane - ray.org) / dn;
  }
};
