#include "tg3helper.h"
#include "renderer/material.h"
#include "tiny_gltf_v3.h"
#include "util/utils.h"
#include <cstdint>
#include <memory>

void load_gltf_material(const tg3_model &tg_model, const tg3_primitive &tg_prim,
                        std::unique_ptr<Bxdf> *bxdf, Color *emission) {
  if (tg_prim.material < 0 || tg_prim.material >= tg_model.materials_count) {
    *bxdf = std::move(std::make_unique<Lambertian>(Color(1.)));
    *emission = Color();
    return;
  }

  const tg3_material &tg_material = tg_model.materials[tg_prim.material];

  *emission =
      Color(tg_material.emissive_factor[0], tg_material.emissive_factor[1],
            tg_material.emissive_factor[2]);

  const tg3_pbr_metallic_roughness &tg_pbr_info =
      tg_material.pbr_metallic_roughness;

  Color base_color =
      Color(tg_pbr_info.base_color_factor[0], tg_pbr_info.base_color_factor[1],
            tg_pbr_info.base_color_factor[2]);
  double roughness = tg_pbr_info.roughness_factor;
  double metalic = tg_pbr_info.metallic_factor;

  if (metalic == 1. && roughness == 0.) {
    *bxdf = std::move(std::make_unique<Mirror>(base_color));
  } else if (metalic == 0. && roughness == 1.) {
    *bxdf = std::move(std::make_unique<Lambertian>(base_color));
  } else if (metalic == 1.) {
    *bxdf = std::move(std::make_unique<MicroBrdf>(base_color, roughness));
  } else {
    *bxdf = std::move(
        std::make_unique<MixtureBrdf>(base_color, metalic, roughness));
  }

  bxdf->get()->display();
}

Mat4 get_node_transform_mat(const tg3_node &node) {
  Mat4 m = Mat4();
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      m.e[row][col] = node.matrix[row * 4 + col];
    }
  }

  if (m != Mat4()) {
    return m;
  }

  Mat4 tm, sm, rm;

  tm = translation_mat(node.translation[0], node.translation[1],
                       node.translation[2]);

  rm = rotation_mat(node.rotation[0], node.rotation[1], node.rotation[2],
                    node.rotation[3]);

  sm = scale_mat(node.scale[0], node.scale[1], node.scale[2]);

  return tm * rm * sm;
}

Mat4 get_transform_mat(const tg3_model &tg_model, const uint32_t node_idx) {
  Mat4 m = get_node_transform_mat(tg_model.nodes[node_idx]);

  for (uint32_t i = 0; i < tg_model.nodes_count; i++) {
    tg3_node parent = tg_model.nodes[i];
    if (parent.children_count <= 0) {
      continue;
    }

    for (uint32_t j = 0; j < parent.children_count; j++) {
      if (parent.children[j] == node_idx) {
        m = get_transform_mat(tg_model, i) * m;
      }
    }
  }

  return m;
}

Mat4 get_normal_transform_mat(const Mat4 &m) {
  // m[0-3,0-3] (-1)(T)
  double a = m.e[0][0], b = m.e[0][1], c = m.e[0][2];
  double d = m.e[1][0], e = m.e[1][1], f = m.e[1][2];

  double g = m.e[2][0], h = m.e[2][1], i = m.e[2][2];

  double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);

  if (std::abs(det) < EPS) {
    return m;
  }

  double invDet = 1.0 / det;
  Mat4 inv = Mat4();

  inv.e[0][0] = (e * i - f * h) * invDet;
  inv.e[0][1] = (c * h - b * i) * invDet;
  inv.e[0][2] = (b * f - c * e) * invDet;

  inv.e[1][0] = (f * g - d * i) * invDet;
  inv.e[1][1] = (a * i - c * g) * invDet;
  inv.e[1][2] = (c * d - a * f) * invDet;

  inv.e[2][0] = (d * h - e * g) * invDet;
  inv.e[2][1] = (b * g - a * h) * invDet;
  inv.e[2][2] = (a * e - b * d) * invDet;

  return inv.transpose();
}
