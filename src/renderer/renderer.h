#pragma once

#include "camera.h"
#include "renderer/integrator.h"
#include "scene/scene.h"
#include "util/image.h"

class Renderer {
public:
  Scene *scene;
  Camera *camera;
  uint32_t supersamples, samples_per_spx;

  Renderer(Scene *scene, Camera *camera, const uint32_t supersamples,
           const uint32_t samples_per_spx)
      : scene(scene), camera(camera), supersamples(supersamples),
        samples_per_spx(samples_per_spx) {
    scene->construct_bvh();
  }

  void render() const {
    auto [resolution_u, resolution_v] = camera->get_resolution();
    std::vector<Color> image(resolution_u * resolution_v);
    uint32_t pixel_num = resolution_u * resolution_v;
    double total_inv =
        1. / (double)(supersamples * supersamples * samples_per_spx);

    HitRecord record = HitRecord();

    for (uint32_t v = 0; v < resolution_v; v++) {
      for (uint32_t u = 0; u < resolution_u; u++) {
        Color accumlated = Color();
        uint32_t idx = pixel_num - resolution_u * v - u - 1;
        Random rnd = Random(u * v);

        for (uint32_t sv = 0; sv < supersamples; sv++) {
          for (uint32_t su = 0; su < supersamples; su++) {
            Integrator integrator = Integrator();
            Point3 pixel_pos =
                camera->get_pixel_pos(u, v, su, sv, supersamples);
            Ray ray = camera->get_first_ray(&rnd, pixel_pos);

            for (uint32_t s = 0; s < samples_per_spx; s++) {
              integrator.reset();
              integrator.integrate(scene, ray, &rnd);
              accumlated =
                  accumlated + integrator.get_value() *
                                   camera->get_coefficient(ray, pixel_pos);
            }
          }
        }
        image[idx] = accumlated * total_inv;
      }
      std::cout << v << std::endl;
    }

    save_ppm("render.ppm", image, resolution_u, resolution_v);
  }

  void test_render() const {
    auto [resolution_u, resolution_v] = camera->get_resolution();
    std::vector<Color> image(resolution_u * resolution_v);
    uint32_t pixel_num = resolution_u * resolution_v;

    HitRecord record = HitRecord();

    for (uint32_t v = 0; v < resolution_v; v++) {
      for (uint32_t u = 0; u < resolution_u; u++) {
        Color accumlated = Color();
        uint32_t idx = pixel_num - resolution_u * v - u - 1;

        for (uint32_t sv = 0; sv < 2; sv++) {
          for (uint32_t su = 0; su < 2; su++) {
            Random rnd = Random(u * v);
            record.reset();
            Point3 pixel_pos =
                camera->get_pixel_pos(u, v, su, sv, supersamples);
            Ray ray = camera->get_first_ray(&rnd, pixel_pos);
            if (scene->test_intersect(ray, &record)) {
              accumlated = accumlated + record.normal / 2. + Color(0.5);
            } else {
              accumlated = accumlated + Color();
            }
          }
        }

        image[idx] = accumlated / 4.;
      }
      std::cout << v << std::endl;
    }

    save_ppm("test.ppm", image, resolution_u, resolution_v);
  }
};
