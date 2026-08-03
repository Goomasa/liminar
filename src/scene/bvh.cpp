#include "bvh.h"
#include "scene/aabb.h"
#include <algorithm>
#include <vector>

inline AABB scene_bbox(const std::vector<Primitive> &prims) {
  AABB bbox = AABB();
  for (const Primitive &prim : prims) {
    bbox = bbox + prim.shape.get_bbox();
  }
  return bbox;
}

inline void sort_primitives(std::vector<Primitive> *prims, const uint8_t axis) {
  std::sort(prims->begin(), prims->end(),
            [axis](const Primitive &a, const Primitive &b) {
              return a.shape.get_centroid()[axis] <
                     b.shape.get_centroid()[axis];
            });
  return;
}

void BVH::basic_builder(const std::vector<Primitive> &prims) {
  if (prims.size() == 0) {
    return;
  }

  std::vector<std::vector<Primitive>> tmp_prims_tree = {prims};
  tree.emplace_back(BVHNode(scene_bbox(prims), -1, 0));

  for (uint32_t idx = 0; idx < prims.size() * 2 - 1; idx++) {
    std::vector<Primitive> *prims_in_box = &tmp_prims_tree[idx];

    if (prims_in_box->size() == 1) {
      tree[idx].prim_id = prims_in_box->at(0).id;
      continue;
    }

    sort_primitives(prims_in_box, idx % 3);
    uint32_t partition = prims_in_box->size() / 2;

    std::vector<Primitive> left_prims(prims_in_box->begin(),
                                      prims_in_box->begin() + partition);
    std::vector<Primitive> right_prims(prims_in_box->begin() + partition,
                                       prims_in_box->end());
    prims_in_box->clear();
    prims_in_box->shrink_to_fit();
    tree[idx].left = tree.size();

    tree.emplace_back(BVHNode(scene_bbox(left_prims), -1, 0));
    tree.emplace_back(scene_bbox(right_prims), -1, 0);
    tmp_prims_tree.emplace_back(std::move(left_prims));
    tmp_prims_tree.emplace_back(std::move(right_prims));
  }
}

void BVH::sah_builder(const std::vector<Primitive> &prims) {
  if (prims.size() == 0) {
    return;
  }

  std::vector<std::vector<Primitive>> tmp_prims_tree = {prims};
  tree.emplace_back(BVHNode(scene_bbox(prims), -1, 0));

  for (uint32_t idx = 0; idx < prims.size() * 2 - 1; idx++) {
    std::vector<Primitive> *prims_in_box = &tmp_prims_tree[idx];

    if (prims_in_box->size() == 1) {
      tree[idx].prim_id = prims_in_box->at(0).id;
      continue;
    }

    uint32_t partition = 1;
    uint8_t best_axis = 0;
    double min_area = INF;
    std::vector<double> area_left(prims_in_box->size() - 1);

    for (uint8_t axis = 0; axis < 3; axis++) {
      sort_primitives(prims_in_box, axis);

      AABB left_bbox = AABB(), right_bbox = AABB();
      for (uint32_t i = 0; i < prims_in_box->size() - 1; i++) {
        left_bbox = left_bbox + prims_in_box->at(i).shape.get_bbox();
        area_left[i] = left_bbox.get_area();
      }

      for (uint32_t i = prims_in_box->size() - 1; i >= 1; i--) {
        right_bbox = right_bbox + prims_in_box->at(i).shape.get_bbox();
        double total_area = area_left[i - 1] + right_bbox.get_area();
        if (abs((int)(i - prims_in_box->size() / 2)) <= 1 &&
            total_area < min_area) {
          min_area = total_area;
          partition = i;
          best_axis = axis;
        }
      }
    }

    sort_primitives(prims_in_box, best_axis);

    std::vector<Primitive> left_prims(prims_in_box->begin(),
                                      prims_in_box->begin() + partition);
    std::vector<Primitive> right_prims(prims_in_box->begin() + partition,
                                       prims_in_box->end());
    prims_in_box->clear();
    prims_in_box->shrink_to_fit();
    tree[idx].left = tree.size();

    tree.emplace_back(BVHNode(scene_bbox(left_prims), -1, 0));
    tree.emplace_back(scene_bbox(right_prims), -1, 0);
    tmp_prims_tree.emplace_back(std::move(left_prims));
    tmp_prims_tree.emplace_back(std::move(right_prims));
  }
}

double BVH::get_area(const uint32_t id) const {
  if (tree[id].left == -1) {
    return tree[id].bbox.get_area();
  } else {
    return tree[id].bbox.get_area() + get_area(tree[id].left) +
           get_area(tree[id].left + 1);
  }
}
