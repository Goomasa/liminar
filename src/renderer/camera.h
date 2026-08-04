#pragma once

#include "renderer/ray.h"
#include "util/random.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>
#include <tuple>

class Camera {
public:
  virtual ~Camera() = default;
  virtual uint32_t get_resolution_w() const = 0;
  virtual uint32_t get_resolution_h() const = 0;
  virtual double get_coefficient(const Ray &ray,
                                 const Point3 &pixel_pos) const = 0;
  virtual Ray get_first_ray(Random *rnd, const Point3 &pixel_pos) const = 0;
  virtual Point3 get_pixel_pos(uint32_t px_u, uint32_t px_v, uint32_t spx_u,
                               uint32_t spx_v, uint32_t superpx_num) const = 0;
};

class PinholeModel : public Camera {
public:
  PinholeModel(const uint32_t reso_w, const uint32_t reso_h,
               const Point3 &sensor_center, const double sensor_w,
               const Vec3 &sensor_dir_, const double sensor_to_hole,
               const double w_scale)
      : resolution_w(reso_w), resolution_h(reso_h),
        sensor_center(sensor_center), sensor_width(sensor_w),
        sensor_dir(normalize(sensor_dir_)) {
    sensor_height = sensor_w * ((double)resolution_h / (double)resolution_w);
    hole_pos = sensor_center + sensor_dir * sensor_to_hole;

    pixel_width = sensor_width / (double)resolution_w;
    pixel_height = sensor_height / (double)resolution_h;

    sensor_u = normalize(cross(sensor_dir, Vec3(0, 1, 0)));
    sensor_v = normalize(cross(sensor_dir, sensor_u));
    sensor_corner = sensor_center - sensor_u * sensor_width / 2. -
                    sensor_v * sensor_height / 2.;

    sensitivity = w_scale / (pixel_width * pixel_height);
  }

  uint32_t get_resolution_w() const override { return resolution_w; }

  uint32_t get_resolution_h() const override { return resolution_h; }

  double get_coefficient(const Ray &ray,
                         const Point3 &pixel_pos) const override {
    double pixel_area = pixel_width * pixel_height;
    double g_term = pow(dot(sensor_dir, normalize(hole_pos - pixel_pos)), 2) /
                    (hole_pos - pixel_pos).len2();

    return sensitivity * g_term * pixel_area;
  }

  Ray get_first_ray(Random *rnd, const Point3 &pixel_pos) const override {
    Point3 org = pixel_pos;
    Vec3 dir = normalize(hole_pos - org);
    return Ray(org, dir);
  }

  Point3 get_pixel_pos(uint32_t px_u, uint32_t px_v, uint32_t spx_u,
                       uint32_t spx_v, uint32_t superpx_num) const override {
    double du =
        pixel_width * (px_u + ((double)spx_u + 0.5) / (double)superpx_num);
    double dv =
        pixel_height * (px_v + ((double)spx_v + 0.5) / (double)superpx_num);
    return sensor_corner + sensor_u * du + sensor_v * dv;
  }

private:
  uint32_t resolution_w, resolution_h;

  Point3 sensor_center, sensor_corner;
  double sensor_width, sensor_height;
  Vec3 sensor_dir;
  double pixel_width, pixel_height;
  Vec3 sensor_u, sensor_v;

  Point3 hole_pos;
  double sensitivity;
};

class LensModel : public Camera {
public:
  LensModel(const uint32_t reso_w, const uint32_t reso_h,
            const Point3 &sensor_center, const double sensor_w,
            const Vec3 &sensor_dir_, const double sensor_to_lens,
            const double lens_radius, const double focal_dist,
            const double w_scale)
      : resolution_w(reso_w), resolution_h(reso_h),
        sensor_center(sensor_center), sensor_width(sensor_w),
        sensor_dir(normalize(sensor_dir_)), sensor_to_lens(sensor_to_lens),
        lens_radius(lens_radius), focal_distance(focal_dist) {
    sensor_height = sensor_w * ((double)resolution_h / (double)resolution_w);

    pixel_width = sensor_width / (double)resolution_w;
    pixel_height = sensor_height / (double)resolution_h;

    sensor_u = normalize(cross(sensor_dir, Vec3(0, 1, 0)));
    sensor_v = normalize(cross(sensor_dir, sensor_u));
    sensor_corner = sensor_center - sensor_u * sensor_width / 2. -
                    sensor_v * sensor_height / 2.;

    lens_center = sensor_center + sensor_dir * sensor_to_lens;

    sensitivity = w_scale / (pixel_width * pixel_height);
  }

  uint32_t get_resolution_w() const override { return resolution_w; }

  uint32_t get_resolution_h() const override { return resolution_h; }

  double get_coefficient(const Ray &ray,
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
                       uint32_t spx_v, uint32_t superpx_num) const override {
    double du =
        pixel_width * (px_u + ((double)spx_u + 0.5) / (double)superpx_num);
    double dv =
        pixel_height * (px_v + ((double)spx_v + 0.5) / (double)superpx_num);
    return sensor_corner + sensor_u * du + sensor_v * dv;
  }

private:
  uint32_t resolution_w, resolution_h;

  Point3 sensor_center, sensor_corner;
  double sensor_width, sensor_height;
  Vec3 sensor_dir;
  double pixel_width, pixel_height;
  Vec3 sensor_u, sensor_v;

  double sensor_to_lens;
  Point3 lens_center;
  double lens_radius;

  double focal_distance;

  double sensitivity;

  Point3 sample_lens(Random *rnd) const {
    double theta = 2. * PI * rnd->gen();
    double r = rnd->gen() * lens_radius;
    return lens_center + sensor_u * r * cos(theta) + sensor_v * r * sin(theta);
  }
};
