#pragma once

#include "renderer/material.h"
#include "renderer/ray.h"
#include "scene/bvh.h"
#include "scene/primitive.h"

#include <memory>
#include <vector>

struct Scene {
  std::vector<std::unique_ptr<Bxdf>> bxdfs;
  std::vector<Primitive> primitives;
  Color background;
  BVH bvh;

  Scene(const Color &background) : background(background) {}

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
    if (bvh.tree[node_id].left == -1) {
      primitives[bvh.tree[node_id].prim_id].hit(ray, record);
    } else if (bvh.tree[node_id].bbox.hit(ray, record)) {
      uint32_t left_id = bvh.tree[node_id].left;
      intersect(ray, record, left_id);
      intersect(ray, record, left_id + 1);
    }

    return record->id != -1;
  }
};
