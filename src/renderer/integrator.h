#pragma once

#include "renderer/ray.h"
#include "scene/scene.h"
#include "util/image.h"
#include "util/utils.h"

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

  void integrate(Scene *scene, Ray ray, Random *rnd);

private:
  Color value;
};
