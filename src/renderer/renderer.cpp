#include "renderer.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_timer.h"
#include "SDL3/SDL_video.h"
#include "config/parser.h"
#include "renderer/camera.h"
#include "renderer/integrator.h"
#include "scene/scene.h"
#include "scene/texture.h"
#include "util/image.h"
#include "util/utils.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <omp.h>
#include <string>
#include <vector>

void Renderer::set_renderer(ObjectNode &node) {
  ObjectNode renderer_node = node.search_key<ObjectNode>("renderer");
  supersamples =
      (uint32_t)renderer_node.search_key_safe<double>("supersamples", 2);
  samples_per_spx = (uint32_t)renderer_node.search_key_safe<double>(
      "samples-per-superpixel", 2);
  threads_num =
      (uint32_t)renderer_node.search_key_safe<double>("threads-num", 1);
  is_test = renderer_node.search_key_safe<bool>("is-test", false);
  has_gui = renderer_node.search_key_safe<bool>("gui", false);
}

void Renderer::set_camera(ObjectNode &node) {
  ObjectNode camera_node = node.search_key<ObjectNode>("camera");
  if (camera_node.search_key_safe<std::string>("model", "pinhole") == "lens") {
    ObjectNode lens_node = camera_node.search_key<ObjectNode>("lens");
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

    double w_scale = lens_node.search_key_safe<double>("w-scale", 100);

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
  std::string background_type =
      scene_node.search_key_safe<std::string>("background-type", "solid");
  std::unique_ptr<Texture> background;

  if (background_type == "image") {
    // image texture
    std::string filename =
        scene_node.search_key_safe<std::string>("background", "");
    uint32_t width, height;
    auto pixels = load_image(filename, &width, &height);

    if (pixels.has_value()) {
      background = std::make_unique<ImageTexture>(filename, pixels.value(),
                                                  width, height);
    } else {
      background = std::make_unique<SolidColor>(Color());
    }
  } else if (background_type == "hgrad") {
    Color bottom_color =
        vector_to_vec3(scene_node.search_key_safe<std::vector<double>>(
            "bottom-color", {0, 0, 0}));
    Color top_color =
        vector_to_vec3(scene_node.search_key_safe<std::vector<double>>(
            "top-color", {0, 0, 0}));
    background = std::make_unique<HGradationColor>(bottom_color, top_color);
  } else {
    // solid color
    Color bcolor =
        vector_to_vec3(scene_node.search_key_safe<std::vector<double>>(
            "background", {0, 0, 0}));
    background = std::make_unique<SolidColor>(bcolor);
  }

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
        scene->load_obj(entry.path().string<char>(), models_dir, scale, tr);
      }
    }
  } catch (const std::filesystem::filesystem_error err) {
    std::cerr << "filesystem error: " << err.what() << std::endl;
    return;
  }

  scene->primitives.shrink_to_fit();
  scene->bxdfs.shrink_to_fit();
  scene->shapes.shrink_to_fit();

  std::cout << "size of primitives: " << scene->primitives.size() << std::endl;
  std::cout << "size of bxdfs: " << scene->bxdfs.size() << std::endl;
}

void Renderer::render() const {
  uint32_t resolution_w = camera->get_resolution_w();
  uint32_t resolution_h = camera->get_resolution_h();
  std::vector<Color> image(resolution_w * resolution_h);
  uint32_t pixel_num = resolution_w * resolution_h;
  double total_inv =
      1. / (double)(supersamples * supersamples * samples_per_spx);

  omp_set_num_threads(threads_num);

#pragma omp parallel for schedule(dynamic, 1)
  for (int32_t v = 0; v < resolution_h; v++) {
    Integrator integrator = Integrator();

    for (uint32_t u = 0; u < resolution_w; u++) {
      Color accumlated = Color();
      uint32_t idx = pixel_num - resolution_w * v - u - 1;
      Random rnd = Random(u * v);

      for (uint32_t sv = 0; sv < supersamples; sv++) {
        for (uint32_t su = 0; su < supersamples; su++) {
          Point3 pixel_pos =
              camera->get_pixel_pos(u, v, su, sv, supersamples, &rnd);
          Ray ray = camera->get_first_ray(&rnd, pixel_pos);

          for (uint32_t s = 0; s < samples_per_spx; s++) {
            integrator.reset();
            integrator.mis_integrate(scene.get(), ray, &rnd);
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

  save_ppm("render.ppm", image, resolution_w, resolution_h);
}

void Renderer::test_render() const {
  uint32_t resolution_w = camera->get_resolution_w();
  uint32_t resolution_h = camera->get_resolution_h();
  std::vector<Color> image(resolution_w * resolution_h);
  uint32_t pixel_num = resolution_w * resolution_h;

  HitRecord record = HitRecord();

  for (uint32_t v = 0; v < resolution_h; v++) {
    for (uint32_t u = 0; u < resolution_w; u++) {
      Color accumlated = Color();
      uint32_t idx = pixel_num - resolution_w * v - u - 1;

      for (uint32_t sv = 0; sv < 2; sv++) {
        for (uint32_t su = 0; su < 2; su++) {
          Random rnd = Random(u * v);
          record.reset();
          Point3 pixel_pos =
              camera->get_pixel_pos(u, v, su, sv, supersamples, &rnd);
          Ray ray = camera->get_first_ray(&rnd, pixel_pos);
          if (scene->intersect(ray, &record)) {
            accumlated = accumlated + record.normal / 2. + Color(0.5);
          } else {
            accumlated = accumlated + Color(1.);
          }
        }
      }

      image[idx] = accumlated / 4.;
    }
    std::cout << v << std::endl;
  }

  save_ppm("test.ppm", image, resolution_w, resolution_h);
};

inline void update_buffer(const std::vector<Color> &image,
                          std::vector<uint32_t> &buffer,
                          const double total_inv) {
  for (uint32_t i = 0; i < image.size(); i++) {
    uint8_t r = static_cast<uint8_t>(gamma_(image[i].x * total_inv));
    uint8_t g = static_cast<uint8_t>(gamma_(image[i].y * total_inv));
    uint8_t b = static_cast<uint8_t>(gamma_(image[i].z * total_inv));
    uint8_t a = 255;
    buffer[i] = (a << 24) | (b << 16) | (g << 8) | r;
  }
}

void Renderer::render_with_gui() const {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << "failed to initialize SDL3: " << SDL_GetError() << std::endl;
    return;
  }

  uint32_t resolution_w = camera->get_resolution_w();
  uint32_t resolution_h = camera->get_resolution_h();
  SDL_Window *sdl_window = SDL_CreateWindow("Image", resolution_w, resolution_h,
                                            SDL_WINDOW_RESIZABLE);
  if (!sdl_window) {
    std::cerr << "failed to create window: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return;
  }

  SDL_Renderer *sdl_renderer = SDL_CreateRenderer(sdl_window, nullptr);
  if (!sdl_renderer) {
    std::cerr << "failed to create renderer: " << SDL_GetError() << std::endl;
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
    return;
  }

  SDL_Texture *sdl_texture = SDL_CreateTexture(
      sdl_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
      resolution_w, resolution_h);
  if (!sdl_texture) {
    std::cerr << "failed to create texture: " << SDL_GetError() << std::endl;
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
    return;
  }

  uint32_t pixel_num = resolution_w * resolution_h;
  std::vector<Color> image_buffer(pixel_num, Color());
  std::vector<uint32_t> sdl_buffer(pixel_num, 0);
  uint32_t samples_count = 1;

  bool running = true;
  omp_set_num_threads(threads_num);

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
      }
    }

    SDL_Delay(33);
    if (samples_count > samples_per_spx) {
      continue;
    }

    double total_inv =
        1. / (double)(supersamples * supersamples * samples_count);

#pragma omp parallel for schedule(dynamic, 1)
    for (int32_t v = 0; v < resolution_h; v++) {
      Integrator integrator = Integrator();

      for (uint32_t u = 0; u < resolution_w; u++) {
        uint32_t idx = pixel_num - resolution_w * v - u - 1;
        Random rnd = Random(u * v * samples_count);

        for (uint32_t sv = 0; sv < supersamples; sv++) {
          for (uint32_t su = 0; su < supersamples; su++) {
            Point3 pixel_pos =
                camera->get_pixel_pos(u, v, su, sv, supersamples, &rnd);
            Ray ray = camera->get_first_ray(&rnd, pixel_pos);

            integrator.reset();
            integrator.mis_integrate(scene.get(), ray, &rnd);
            image_buffer[idx] =
                image_buffer[idx] + integrator.get_value() *
                                        camera->get_coefficient(ray, pixel_pos);
          }
        }
      }
    }

    update_buffer(image_buffer, sdl_buffer, total_inv);
    SDL_UpdateTexture(sdl_texture, nullptr, sdl_buffer.data(),
                      resolution_w * sizeof(uint32_t));

    SDL_RenderClear(sdl_renderer);
    SDL_RenderTexture(sdl_renderer, sdl_texture, nullptr, nullptr);
    SDL_RenderPresent(sdl_renderer);

    std::cout << samples_count << std::endl;
    samples_count++;
  }

  for (uint32_t i = 0; i < pixel_num; i++) {
    image_buffer[i] = image_buffer[i] * 1. /
                      (double)(supersamples * supersamples * samples_count);
  }
  save_ppm("render.ppm", image_buffer, resolution_w, resolution_h);

  SDL_DestroyTexture(sdl_texture);
  SDL_DestroyRenderer(sdl_renderer);
  SDL_DestroyWindow(sdl_window);
  SDL_Quit();
}
