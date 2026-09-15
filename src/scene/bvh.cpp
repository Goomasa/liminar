#include "bvh.h"
#include "scene/aabb.h"
#include <algorithm>
#include <vector>

constexpr double CI = 2;
constexpr double CT = 3;

inline AABB scene_bbox(const std::vector<const Primitive *> &prims) {
  AABB bbox = AABB();
  for (const Primitive *prim : prims) {
    bbox = bbox + prim->shape->get_bbox();
  }
  return bbox;
}

inline void sort_primitives(std::vector<const Primitive *> *prims,
                            const uint8_t axis) {
  std::sort(prims->begin(), prims->end(),
            [axis](const Primitive *a, const Primitive *b) {
              return a->shape->get_center()[axis] <
                     b->shape->get_center()[axis];
            });
  return;
}

void BVH::basic_builder(const std::vector<const Primitive *> &prims) {
  if (prims.size() <= 1) {
    return;
  }

  std::vector<std::vector<const Primitive *>> tmp_prims_tree = {prims};
  tree.emplace_back(BVHNode(scene_bbox(prims), 0, 0));

  for (uint32_t idx = 0; idx < prims.size() - 1; idx++) {
    std::vector<const Primitive *> *prims_in_box = &tmp_prims_tree[idx];

    sort_primitives(prims_in_box, idx % 3);
    uint32_t partition = prims_in_box->size() / 2;

    std::vector<const Primitive *> left_prims(
        prims_in_box->begin(), prims_in_box->begin() + partition);
    std::vector<const Primitive *> right_prims(
        prims_in_box->begin() + partition, prims_in_box->end());
    prims_in_box->clear();
    prims_in_box->shrink_to_fit();

    if (left_prims.size() == 1) {
      tree[idx].left = left_prims[0]->id << 1 | 1;
    } else {
      tree[idx].left = tree.size() << 1;
      tree.emplace_back(BVHNode(scene_bbox(left_prims), 0, 0));
      tmp_prims_tree.emplace_back(std::move(left_prims));
    }

    if (right_prims.size() == 1) {
      tree[idx].right = right_prims[0]->id << 1 | 1;
    } else {
      tree[idx].right = tree.size() << 1;
      tree.emplace_back(scene_bbox(right_prims), 0, 0);
      tmp_prims_tree.emplace_back(std::move(right_prims));
    }
  }
}

void BVH::sah_builder(const std::vector<const Primitive *> &prims) {
  if (prims.size() <= 1) {
    return;
  }

  std::vector<std::vector<const Primitive *>> tmp_prims_tree = {prims};
  tree.emplace_back(BVHNode(scene_bbox(prims), 0, 0));

  for (uint32_t idx = 0; idx < prims.size() - 1; idx++) {
    std::vector<const Primitive *> *prims_in_box = &tmp_prims_tree[idx];
    uint32_t prims_box_size = prims_in_box->size();

    uint32_t partition = 1;
    uint8_t best_axis = 0;
    double min_cost = INF;
    std::vector<double> area_left(prims_box_size - 1);

    for (uint8_t axis = 0; axis < 3; axis++) {
      sort_primitives(prims_in_box, axis);

      AABB left_bbox = AABB(), right_bbox = AABB();
      for (uint32_t i = 0; i < prims_box_size - 1; i++) {
        left_bbox = left_bbox + prims_in_box->at(i)->shape->get_bbox();
        area_left[i] = left_bbox.get_area();
      }

      for (uint32_t i = prims_box_size - 1; i >= 1; i--) {
        right_bbox = right_bbox + prims_in_box->at(i)->shape->get_bbox();
        double cost =
            area_left[i - 1] * i + right_bbox.get_area() * (prims_box_size - i);
        if (cost < min_cost) {
          min_cost = cost;
          partition = i;
          best_axis = axis;
        }
      }
    }

    sort_primitives(prims_in_box, best_axis);

    std::vector<const Primitive *> left_prims(
        prims_in_box->begin(), prims_in_box->begin() + partition);
    std::vector<const Primitive *> right_prims(
        prims_in_box->begin() + partition, prims_in_box->end());
    prims_in_box->clear();
    prims_in_box->shrink_to_fit();

    if (left_prims.size() == 1) {
      tree[idx].left = left_prims[0]->id << 1 | 1;
    } else {
      tree[idx].left = tree.size() << 1;
      tree.emplace_back(BVHNode(scene_bbox(left_prims), 0, 0));
      tmp_prims_tree.emplace_back(std::move(left_prims));
    }

    if (right_prims.size() == 1) {
      tree[idx].right = right_prims[0]->id << 1 | 1;
    } else {
      tree[idx].right = tree.size() << 1;
      tree.emplace_back(scene_bbox(right_prims), 0, 0);
      tmp_prims_tree.emplace_back(std::move(right_prims));
    }
  }
}

double BVH::get_sah() const {
  double cost = 0;
  double root_area = tree[0].bbox.get_area();

  for (BVHNode node : tree) {
    if (node.left == -1) {
      cost += node.bbox.get_area() * CI / root_area;
    } else {
      cost += node.bbox.get_area() * CT / root_area;
    }
  }

  return cost;
}
