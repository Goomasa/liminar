#pragma once

#include "camera.h"
#include "scene/scene.h"
#include "util/image.h"

class Renderer {
public:
  Scene *scene;
  Camera *camera;
  uint32_t supersamples, samples_per_spx;

  Renderer(Scene *scene, Camera *camera) : scene(scene), camera(camera) {}

  void test_render() const {
    auto [resolution_u, resolution_v] = camera->get_resolution();
    std::vector<Color> image(resolution_u * resolution_v);

    HitRecord record = HitRecord();

    for (uint32_t v = 0; v < resolution_v; v++) {
      for (uint32_t u = 0; u < resolution_u; u++) {
        Color accumlated = Color();
        for (uint32_t sv = 0; sv < 2; sv++) {
          for (uint32_t su = 0; su < 2; su++) {
            Random rnd = Random(u * v);
            record.reset();
            Ray ray = camera->get_first_ray(&rnd, u, v, su, sv, 2);
            if (scene->test_intersect(ray, &record)) {
              accumlated = accumlated + record.normal / 2. + Color(0.5);
            } else {
              accumlated = accumlated + Color();
            }
          }
        }
        image[resolution_u * v + u] = accumlated / 4.;
      }
    }

    save_ppm("test.ppm", image, resolution_u, resolution_v);
  }
};
