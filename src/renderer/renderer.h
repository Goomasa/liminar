#pragma once

#include "camera.h"
#include "config/parser.h"
#include "scene/scene.h"
#include <chrono>
#include <iostream>
#include <memory>

class Renderer {
public:
  std::unique_ptr<Scene> scene;
  std::unique_ptr<Camera> camera;
  uint32_t supersamples, samples_per_spx, threads_num;
  bool is_test = false;
  bool has_gui = false;

  Renderer(const std::string &filename = "settings/test-setting.json") {
    ObjectNode node = load_json(filename);
    set_camera(node);
    set_scene(node);
    set_renderer(node);
    exit(0);

    scene->construct_bvh();

    std::chrono::system_clock::time_point start, end;
    start = std::chrono::system_clock::now();

    if (is_test) {
      test_render();
    } else {
      if (has_gui) {
        render_with_gui();
      } else {
        render();
      }
    }

    end = std::chrono::system_clock::now();
    double elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    std::cout << elapsed / 1000 << " s" << std::endl;
  }

private:
  void render() const;

  void render_with_gui() const;

  void test_render() const;

  void set_scene(ObjectNode &node);

  void set_camera(ObjectNode &node);

  void set_renderer(ObjectNode &node);
};
