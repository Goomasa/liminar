#pragma once

#include "renderer/camera.h"
#include "renderer/ray.h"
#include "scene/scene.h"
#include "util/image.h"
#include "util/random.h"
#include "util/utils.h"
#include <cstdint>
#include <vector>

constexpr uint32_t DEPTH = 5;
constexpr uint32_t MAX_DEPTH = 10;

class Integrator {
public:
  Integrator() : value(Color()) {}

  Color get_value() const {
    if (is_valid(value)) {
      return value;
    } else {
      return Color();
    }
  }

  void reset() { value = Color(); }

  void pt_integrate(Scene *scene, Ray ray, Random *rnd);

  void mis_integrate(Scene *scene, Ray ray, Random *rnd);

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

enum class ObjectType { LIGHT, LENS, PRIMITIVE };

struct HitpointInfo {
  Point3 pos;
  Color throughput;
  double pdf;
  int32_t id;
  Vec3 orienting_normal, geometry_normal;
  ObjectType type;

  HitpointInfo(const Point3 &pos, const Color &thr, const double pdf,
               const int32_t id, const Vec3 &normal, const Vec3 &geom_normal,
               const ObjectType &type)
      : pos(pos), throughput(thr), pdf(pdf), id(id), orienting_normal(normal),
        geometry_normal(geom_normal), type(type) {}
};

std::vector<HitpointInfo> gen_eye_path(Scene *scene, Camera *camera, Ray ray,
                                       const Point3 &px_pos, Color *final_value,
                                       Random *rnd);
