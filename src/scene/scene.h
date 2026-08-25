#pragma once

#include "renderer/material.h"
#include "renderer/ray.h"
#include "scene/bvh.h"
#include "scene/primitive.h"
#include "util/random.h"
#include "util/utils.h"

#include <cstdint>
#include <memory>
#include <vector>

struct Scene {
  std::vector<std::unique_ptr<Bxdf>> bxdfs;
  std::vector<std::unique_ptr<Shape>> shapes;
  std::vector<Primitive> primitives;
  std::vector<uint32_t> lights;
  Color background;
  BVH bvh;

  Scene(const Color &background) : background(background) { set_primitive(); }

  void load_obj(const std::string &filename, const double scale = 1.,
                const Vec3 &tr = Vec3());

  void load_mtl(const auto &material, Primitive *prim);

  void construct_bvh() {
    std::cout << primitives.size() << " primitives" << std::endl;
    bvh = BVH(primitives);
  }

  bool test_intersect(Ray &ray, HitRecord *record) const {
    for (const Primitive &prim : primitives) {
      prim.hit(ray, record);
    }

    return record->id != -1;
  }

  bool intersect(Ray &ray, HitRecord *record,
                 const uint32_t node_id = 0) const {
    if (bvh.tree[node_id].left & 1) {
      primitives[bvh.tree[node_id].left >> 1].hit(ray, record);
    } else if (bvh.tree[node_id].bbox.hit(ray, record)) {
      uint32_t left_id = bvh.tree[node_id].left >> 1;
      intersect(ray, record, left_id);
      intersect(ray, record, left_id + 1);
    }

    return record->id != -1;
  }

  int32_t sample_light_uniform(Random *rnd) const {
    if (lights.empty()) {
      return -1;
    }

    int32_t sampled = rnd->gen_uint() % lights.size();
    return lights[sampled];
  }

  double pdf_sample_light_uniform() const {
    return lights.size() == 0 ? INF : 1. / (double)lights.size();
  }

private:
  void set_primitive();
};
