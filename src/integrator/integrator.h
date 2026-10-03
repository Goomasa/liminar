#pragma once

#include "renderer/camera.h"
#include "renderer/medium.h"
#include "renderer/ray.h"
#include "scene/scene.h"
#include "util/image.h"
#include "util/random.h"
#include "util/utils.h"
#include <cstdint>
#include <vector>

constexpr uint32_t DEPTH = 10;
constexpr uint32_t MAX_DEPTH = 15;

struct PixelResult {
  uint32_t px_u, px_v;
  Color value;
  bool from_light;

  PixelResult() : px_u(0), px_v(0), value(INVALID_VEC), from_light(false) {}

  PixelResult(const uint32_t px_u, const uint32_t px_v, const Color &value,
              const bool from_light)
      : px_u(px_u), px_v(px_v), value(value), from_light(from_light) {}

  inline Color get_value() const {
    if (is_valid(value) && value != INVALID_VEC) {
      return value;
    } else {
      return Color();
    }
  }
};

class Integrator {
public:
  Integrator() : value(Color()) {}

  inline Color get_value() const {
    if (is_valid(value)) {
      return value;
    } else {
      return Color();
    }
  }

  void reset() { value = Color(); }

  void pt_integrate(Scene *scene, const Ray &ray, Random *rnd);

  void mis_integrate(Scene *scene, const Ray &ray, Random *rnd);

  void volpath_mis_integrate(Scene *scene, const Ray &ray, Random *rnd);

  std::vector<PixelResult> bpt_integrate(Scene *scene, Camera *camera,
                                         const Point3 &px_pos,
                                         const uint32_t px_u,
                                         const uint32_t px_v, const Ray &ray,
                                         Random *rnd);

  std::vector<PixelResult> volbpt_integrate(Scene *scene, Camera *camera,
                                            const Point3 &px_pos,
                                            const uint32_t px_u,
                                            const uint32_t px_v, const Ray &ray,
                                            Random *rnd);

private:
  Color value;
};

inline double roullete(const uint32_t depth) {
  if (depth < DEPTH) {
    return 1.0;
  } else if (depth > MAX_DEPTH) {
    return 0.;
  } else {
    return 0.8;
  }
}

inline double correct_shading_normal(const Vec3 &wi, const Vec3 &wo,
                                     const Vec3 &geometry_normal,
                                     const Vec3 &shading_normal) {
  double numer = dot(wi, shading_normal) * dot(wo, geometry_normal);
  double denom = dot(wi, geometry_normal) * dot(wo, shading_normal);
  return denom < EPS ? 0. : fabs(numer / denom);
}

struct HitpointInfo {
  Point3 hitpoint;
  Color throughput;
  double pdf;
  int32_t id;
  Vec3 orienting_normal, geometry_normal, obj_normal;
  Vec2 uv;
  ObjectType type;
  Medium *medium;

  HitpointInfo() : id(-1) {}

  HitpointInfo(const Point3 &pos, const Color &thr, const double pdf,
               const int32_t id, const Vec3 &normal, const Vec3 &geom_normal,
               const Vec3 &obj_normal, const Vec2 &uv, const ObjectType &type,
               Medium *medium = nullptr)
      : hitpoint(pos), throughput(thr), pdf(pdf), id(id),
        orienting_normal(normal), geometry_normal(geom_normal),
        obj_normal(obj_normal), uv(uv), type(type), medium(medium) {}
};

std::vector<HitpointInfo> gen_eye_path(Scene *scene, Camera *camera,
                                       const Ray &ray, const Point3 &px_pos,
                                       Color *final_value, Random *rnd);

std::vector<HitpointInfo> gen_light_path(Scene *scene, Camera *camera,
                                         PixelResult *result, Random *rnd);

std::vector<HitpointInfo> gen_eye_volpath(Scene *scene, Camera *camera,
                                          const Ray &ray, const Point3 &px_pos,
                                          Color *final_value, Random *rnd);

std::vector<HitpointInfo> gen_light_volpath(Scene *scene, Camera *camera,
                                            PixelResult *result, Random *rnd);
