#pragma once

#include "scene/aabb.h"
#include "scene/primitive.h"
#include <iostream>
#include <vector>

struct BVHNode {
  AABB bbox;
  int32_t left;
  uint32_t prim_id;

  BVHNode() : bbox(AABB()), left(-1), prim_id(0) {}

  BVHNode(const AABB &bbox, const int32_t left, const uint32_t prim)
      : bbox(bbox), left(left), prim_id(prim) {}
};

class BVH {
public:
  std::vector<BVHNode> tree;

  BVH() {}

  BVH(const std::vector<Primitive> &prims) {
    sah_builder(prims);

    tree.shrink_to_fit();
    std::cout << "constructed" << std::endl;
  }

  void basic_builder(const std::vector<Primitive> &prims);

  void sah_builder(const std::vector<Primitive> &prims);

  double get_area(const uint32_t id = 0) const;
};
