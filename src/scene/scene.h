#pragma once

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

#include "renderer/material.h"
#include "renderer/ray.h"
#include "scene/primitive.h"

#include <memory>
#include <vector>

struct Scene {
  std::vector<std::unique_ptr<Bxdf>> bxdfs;
  std::vector<Primitive> primitives;

  Scene() {}

  void load_obj(const std::string &filename);

  void load_mtl(const tinyobj::material_t &material, Primitive *prim);

  bool test_intersect(Ray &ray, HitRecord *record) const {
    for (Primitive prim : primitives) {
      prim.hit(ray, record);
    }

    return record->id != -1;
  }
};
