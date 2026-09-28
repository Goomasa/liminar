#pragma once

#include "camera.h"
#include "config/parser.h"
#include "renderer/material.h"
#include "scene/scene.h"
#include <chrono>
#include <iostream>
#include <memory>

class Renderer {
public:
  std::unique_ptr<Scene> scene;
  std::unique_ptr<Camera> camera;
  uint32_t supersamples, samples_per_spx, threads_num;
  IntegrateMode mode = IntegrateMode::PT;

  Renderer(const std::string &filename = "settings/test-setting.json") {
    ObjectNode node = load_json(filename);
    set_camera(node);
    set_scene(node);
    set_renderer(node);

    scene->setup(mode == IntegrateMode::TEST);

    std::chrono::system_clock::time_point start, end;
    start = std::chrono::system_clock::now();

    switch (mode) {
    case IntegrateMode::TEST: {
      test_render();
    } break;
    case IntegrateMode::BPT: {
      bpt_render();
    } break;
    case IntegrateMode::LT: {
      lt_render();
    } break;
    default: {
      pt_render();
    } break;
    }

    end = std::chrono::system_clock::now();
    double elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    std::cout << elapsed / 1000 << " s" << std::endl;
  }

private:
  void pt_render() const;

  void lt_render() const;

  void bpt_render() const;

  void test_render() const;

  void set_scene(ObjectNode &node);

  void set_camera(ObjectNode &node);

  void set_renderer(ObjectNode &node);
};
