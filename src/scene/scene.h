#pragma once

#include "renderer/material.h"
#include "renderer/ray.h"
#include "scene/bvh.h"
#include "scene/primitive.h"
#include "scene/texture.h"
#include "util/random.h"
#include "util/utils.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

struct Scene {
  std::vector<std::unique_ptr<Bxdf>> bxdfs;
  std::vector<std::unique_ptr<Shape>> shapes;
  std::vector<Primitive> primitives;
  std::vector<uint32_t> lights;
  std::unique_ptr<Texture> background;
  BVH bvh;

  Scene(std::unique_ptr<Texture> &background)
      : background(std::move(background)) {
    set_test_primitive();
  }

  void load_obj(const std::string &filename, const std::string &dir_path,
                const double scale = 1., const Vec3 &tr = Vec3());

  void load_mtl(const auto &material, Primitive *prim);

  inline void construct_bvh() {
    std::cout << primitives.size() << " primitives" << std::endl;
    bvh = BVH(primitives);
  }

  bool test_intersect(Ray &ray, HitRecord *record) const {
    for (const Primitive &prim : primitives) {
      prim.hit(ray, record);
    }

    return record->id != -1;
  }

  bool intersect(Ray &ray, HitRecord *record, const uint32_t node_id) const {
    if (bvh.tree[node_id].left & 1) {
      primitives[bvh.tree[node_id].left >> 1].hit(ray, record);
    } else if (bvh.tree[node_id].bbox.hit(ray, record)) {
      uint32_t left_id = bvh.tree[node_id].left >> 1;
      intersect(ray, record, left_id);
      intersect(ray, record, left_id + 1);
    }

    return record->id != -1;
  }

  inline bool intersect(Ray &ray, HitRecord *record) const {
    if (bvh.tree.empty()) {
      return false;
    }

    return intersect(ray, record, 0);
  }

  inline int32_t sample_light_uniform(Random *rnd) const {
    if (lights.empty()) {
      return -1;
    }

    int32_t sampled = rnd->gen_uint() % lights.size();
    return lights[sampled];
  }

  inline double pdf_sample_light_uniform() const {
    return lights.size() == 0 ? INF : 1. / (double)lights.size();
  }

  Vec2 get_skysphere_uv(const Vec3 &dir) const {
    double cos_theta = dir.y;
    double sin_theta = sqrt(1 - cos_theta * cos_theta);
    double v = acos(cos_theta) * PI_INV;

    if (sin_theta < EPS) {
      return Vec2(0, clamp01(v));
    }

    double sin_phi = dir.x / sin_theta;
    double cos_phi = dir.z / sin_theta;
    double u = sin_phi > 0. ? 1. - acos(cos_phi) * PI_INV * 0.5
                            : acos(cos_phi) * PI_INV * 0.5;

    return Vec2(clamp01(u), clamp01(v));
  }

private:
  void set_test_primitive();

  void set_sample_scene1();
};
