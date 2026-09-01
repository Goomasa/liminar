#pragma once

#include "renderer/ray.h"
#include "scene/scene.h"
#include "util/image.h"
#include "util/random.h"
#include "util/utils.h"

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

  void mis_integrate(Scene *scene, Ray ray, Random *rnd);

private:
  Color value;
};
