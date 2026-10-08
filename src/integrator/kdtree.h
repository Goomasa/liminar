#pragma once

#include "integrator/integrator.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>
#include <vector>

struct KdTreeNode {
  VcmVertexInfo *photon;
  uint8_t axis;
  int32_t left;
  int32_t right;

  KdTreeNode() : photon(nullptr), axis(0), left(-1), right(-1) {}

  KdTreeNode(VcmVertexInfo *photon, const uint8_t axis, const int32_t left,
             const int32_t right)
      : photon(photon), axis(axis), left(left), right(right) {}
};

typedef std::vector<KdTreeNode> PhotonMap;

inline void sort_photons(std::vector<VcmVertexInfo> *photons,
                         const uint8_t axis) {
  std::sort(photons->begin(), photons->end(),
            [axis](const VcmVertexInfo &a, const VcmVertexInfo &b) {
              return a.hitpoint[axis] < b.hitpoint[axis];
            });
  return;
}

static void construct_photonmap(PhotonMap *photon_map,
                                std::vector<VcmVertexInfo> &photons,
                                const uint32_t depth = 0) {
  uint8_t axis = depth % 3;
  uint32_t size = photons.size();
  if (size == 0) {
    return;
  } else if (size == 1) {
    photon_map->emplace_back(KdTreeNode(&photons[0], axis, -1, -1));
    return;
  }

  uint32_t median = (size - 1) / 2;
  std::vector<VcmVertexInfo> left(photons.begin(), photons.begin() + median);
  std::vector<VcmVertexInfo> right(photons.begin() + median, photons.end());
  construct_photonmap(photon_map, left, depth + 1);
  construct_photonmap(photon_map, right, depth + 1);
}

static std::vector<VcmVertexInfo *> search_radius(const PhotonMap &photon_map,
                                                  const double radius,
                                                  const Point3 &pos,
                                                  const uint32_t idx = 0) {
  std::vector<VcmVertexInfo *> result;

  const KdTreeNode *node = &photon_map[idx];
  if ((node->photon->hitpoint - pos).len() <= radius) {
    result.emplace_back(node->photon);
  }

  double d = pos[node->axis] - node->photon->hitpoint[node->axis];
  if (fabs(d) > radius && d >= 0) {
    if (node->right >= 0) {
      result.append_range(search_radius(photon_map, radius, pos, node->right));
    }
  } else if (fabs(d) > radius && d < 0) {
    if (node->left >= 0) {
      result.append_range(search_radius(photon_map, radius, pos, node->left));
    }
  } else {
    if (node->right >= 0) {
      result.append_range(search_radius(photon_map, radius, pos, node->right));
    }

    if (node->left >= 0) {
      result.append_range(search_radius(photon_map, radius, pos, node->left));
    }
  }

  return result;
}
