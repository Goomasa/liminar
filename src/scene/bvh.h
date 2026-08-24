#pragma once

#include "scene/aabb.h"
#include "scene/primitive.h"
#include <iostream>
#include <vector>

struct BVHNode {
  AABB bbox;
  uint32_t left; // the first bit is set -> leaf

  BVHNode() : bbox(AABB()), left(-1) {}

  BVHNode(const AABB &bbox, const int32_t left) : bbox(bbox), left(left) {}
};

class BVH {
public:
  std::vector<BVHNode> tree;

  BVH() {}

  BVH(const std::vector<Primitive> &prims) {
    sah_builder(prims);

    tree.shrink_to_fit();
    std::cout << "SAH: " << get_sah() << std::endl;
  }

  void basic_builder(const std::vector<Primitive> &prims);

  void sah_builder(const std::vector<Primitive> &prims);

  double get_sah() const;
};
