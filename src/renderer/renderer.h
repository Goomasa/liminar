#pragma once

#include "camera.h"
#include "renderer/material.h"
#include "scene/scene.h"
#include "setting/parser.h"
#include "util/utils.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>

class Renderer {
public:
  std::unique_ptr<Scene> scene;
  std::unique_ptr<Camera> camera;
  uint32_t supersamples, samples_per_spx, threads_num;
  IntegrateMode mode = IntegrateMode::PT;
  bool has_medium = false;

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
      has_medium ? volbpt_render() : bpt_render();
    } break;
    default: {
      has_medium ? volpt_render() : pt_render();
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
  void volpt_render() const;
  void bpt_render() const;
  void volbpt_render() const;
  void test_render() const;

  void set_scene(ObjectNode &node);
  void set_camera(ObjectNode &node);
  void set_renderer(ObjectNode &node);
};

struct AtomicColor {
  std::atomic<double> r{0.0};
  std::atomic<double> g{0.0};
  std::atomic<double> b{0.0};

  void add(const Color &c) {
    r.fetch_add(c.x, std::memory_order_relaxed);
    g.fetch_add(c.y, std::memory_order_relaxed);
    b.fetch_add(c.z, std::memory_order_relaxed);
  }

  Color to_color() const { return Color(r, g, b); }
};
