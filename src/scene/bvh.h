#pragma once

#include "scene/aabb.h"
#include "scene/primitive.h"
#include <cstdint>
#include <iostream>
#include <vector>

struct BVHNode {
  AABB bbox;
  uint32_t left; // the first bit is set -> leaf
  uint32_t right;

  BVHNode() : bbox(AABB()), left(-1), right(-1) {}

  BVHNode(const AABB &bbox, const uint32_t left, uint32_t right)
      : bbox(bbox), left(left), right(right) {}
};

class BVH {
public:
  std::vector<BVHNode> tree;

  BVH() {}

  BVH(const std::vector<Primitive> &prims) {
    std::vector<const Primitive *> prims_ptr(prims.size());
    for (uint32_t i = 0; i < prims.size(); i++) {
      prims_ptr[i] = &prims[i];
    }
    sah_builder(prims_ptr);
    // basic_builder(prims_ptr);

    tree.shrink_to_fit();
    std::cout << "SAH: " << get_sah() << std::endl;
  }

  void basic_builder(const std::vector<const Primitive *> &prims);

  void sah_builder(const std::vector<const Primitive *> &prims);

  double get_sah() const;
};
