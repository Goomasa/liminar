#include "renderer.h"
#include "config/parser.h"
#include "renderer/camera.h"
#include "renderer/integrator.h"
#include "scene/scene.h"
#include "util/image.h"
#include "util/utils.h"
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

void Renderer::set_renderer(ObjectNode &node) {
  ObjectNode renderer_node = node.search_key<ObjectNode>("renderer");
  supersamples =
      (uint32_t)renderer_node.search_key_safe<double>("supersamples", 2);
  samples_per_spx = (uint32_t)renderer_node.search_key_safe<double>(
      "samples-per-superpixel", 2);
  is_test = renderer_node.search_key_safe<bool>("is-test", false);
}

void Renderer::set_camera(ObjectNode &node) {
  ObjectNode camera_node = node.search_key<ObjectNode>("camera");
  if (camera_node.search_key_safe<std::string>("model", "pinhole") == "lens") {
    ObjectNode lens_node = camera_node.search_key<ObjectNode>("pinhole");
    uint32_t reso_w =
        (uint32_t)lens_node.search_key_safe<double>("resolution-w", 400);
    uint32_t reso_h =
        (uint32_t)lens_node.search_key_safe<double>("resolution-h", 400);
    Point3 sensor_center =
        vector_to_vec3(lens_node.search_key_safe<std::vector<double>>(
            "sensor-center", {1, 1, 1}));
    Vec3 sensor_dir =
        vector_to_vec3(lens_node.search_key_safe<std::vector<double>>(
            "sensor-dir", {0, 0, -1}));
    double sensor_w = lens_node.search_key_safe<double>("sensor-width", 30);

    double sensor_to_lens =
        lens_node.search_key_safe<double>("sensor-to-lens", 40);
    double lens_radius = lens_node.search_key_safe<double>("lens-radius", 2);
    double focal_distance =
        lens_node.search_key_safe<double>("focal-distance", 90);

    double w_scale = lens_node.search_key_safe<double>("w_scale", 100);

    camera = std::make_unique<LensModel>(reso_w, reso_h, sensor_center,
                                         sensor_w, sensor_dir, sensor_to_lens,
                                         lens_radius, focal_distance, w_scale);
  } else {
    ObjectNode pinhole_node = camera_node.search_key<ObjectNode>("pinhole");
    uint32_t reso_w =
        (uint32_t)pinhole_node.search_key_safe<double>("resolution-w", 400);
    uint32_t reso_h =
        (uint32_t)pinhole_node.search_key_safe<double>("resolution-h", 400);
    Point3 sensor_center =
        vector_to_vec3(pinhole_node.search_key_safe<std::vector<double>>(
            "sensor-center", {0, 0, 0}));
    double sensor_to_hole =
        pinhole_node.search_key_safe<double>("sensor-to-hole", 30);
    Vec3 sensor_dir =
        vector_to_vec3(pinhole_node.search_key_safe<std::vector<double>>(
            "sensor-dir", {0, 0, -1}));
    double sensor_w = pinhole_node.search_key_safe<double>("sensor-width", 30);
    double w_scale = pinhole_node.search_key_safe<double>("w-scale", 500);

    camera =
        std::make_unique<PinholeModel>(reso_w, reso_h, sensor_center, sensor_w,
                                       sensor_dir, sensor_to_hole, w_scale);
  }
}

void Renderer::set_scene(ObjectNode &node) {
  ObjectNode scene_node = node.search_key<ObjectNode>("scene");
  Color background = vector_to_vec3(
      scene_node.search_key_safe<std::vector<double>>("background", {0, 0, 0}));

  scene = std::make_unique<Scene>(background);

  ObjectNode model_node = scene_node.search_key<ObjectNode>("models");
  std::string models_dir =
      model_node.search_key_safe<std::string>("path", "models/test-box/");
  double scale = model_node.search_key_safe<double>("scale", 1);
  Vec3 tr = vector_to_vec3(model_node.search_key_safe<std::vector<double>>(
      "translation", {0, 0, 0}));

  std::filesystem::path p(models_dir);

  try {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(p)) {
      if (entry.is_regular_file() && entry.path().extension() == ".obj") {
        scene->load_obj(entry.path(), scale, tr);
      }
    }
  } catch (const std::filesystem::filesystem_error err) {
    std::cerr << "filesystem error: " << err.what() << std::endl;
    return;
  }
}

void Renderer::render() const {
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
          Point3 pixel_pos = camera->get_pixel_pos(u, v, su, sv, supersamples);
          Ray ray = camera->get_first_ray(&rnd, pixel_pos);

          for (uint32_t s = 0; s < samples_per_spx; s++) {
            integrator.reset();
            integrator.integrate(scene.get(), ray, &rnd);
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

void Renderer::test_render() const {
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
          Point3 pixel_pos = camera->get_pixel_pos(u, v, su, sv, supersamples);
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
};
