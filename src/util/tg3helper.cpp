#include "tg3helper.h"
#include "renderer/material.h"
#include "scene/texture.h"
#include "tiny_gltf_v3.h"
#include "util/image.h"
#include "util/utils.h"
#include <cstdint>
#include <memory>
#include <vector>

inline bool is_transparent(const tg3_material &tg_mat, double *ior) {
  constexpr std::string KHR_EXT_NAME = "KHR_materials_ior";
  constexpr std::string EXT_OBJECT_KEY = "ior";

  *ior = 0.;

  for (uint32_t i = 0; i < tg_mat.ext.extensions_count; i++) {
    if (tg_mat.ext.extensions[i].name.data == KHR_EXT_NAME) {
      const tg3_extension tg_ext = tg_mat.ext.extensions[i];

      for (uint32_t j = 0; j < tg_ext.value.object_count; j++) {
        if (tg_ext.value.object_data[j].key.data == EXT_OBJECT_KEY) {
          *ior = tg_ext.value.object_data[j].value.real_val;
          break;
        }
      }
    }
  }

  return *ior > 1.;
}

bool get_image_buffer_data(const tg3_buffer_view &tex_buffer_view,
                           const tg3_buffer &tg_buffer, const uint8_t *&data,
                           size_t &size) {
  const size_t offset = static_cast<size_t>(tex_buffer_view.byte_offset);
  const size_t length = static_cast<size_t>(tex_buffer_view.byte_length);

  if (offset > tg_buffer.data.count) {
    return false;
  }

  if (length > tg_buffer.data.count - offset) {
    return false;
  }

  data = reinterpret_cast<const uint8_t *>(tg_buffer.data.data) + offset;
  size = length;

  return true;
}

inline void get_texcoord_sampler(const tg3_model &tg_model,
                                 const tg3_texture &tg_tex,
                                 TexcoordSampler *sampler_u,
                                 TexcoordSampler *sampler_v) {
  int32_t sampler_idx = tg_tex.sampler;
  if (sampler_idx == TG3_INDEX_NONE || sampler_idx >= tg_model.samplers_count) {
    *sampler_u = TexcoordSampler::REPEAT;
    *sampler_v = TexcoordSampler::REPEAT;
  }

  const tg3_sampler &tg_sampler = tg_model.samplers[sampler_idx];

  auto to_sampler = [](int32_t key) -> TexcoordSampler {
    if (key == 33648) {
      return TexcoordSampler::MIRRORED_REPEAT;
    } else if (key == 33071) {
      return TexcoordSampler::CLAMP_TO_EDGE;
    } else {
      return TexcoordSampler::REPEAT;
    }
  };

  *sampler_u = to_sampler(tg_sampler.wrap_s);
  *sampler_v = to_sampler(tg_sampler.wrap_t);
}

void load_gltf_texture(const tg3_model &tg_model, const int32_t tex_idx,
                       std::unique_ptr<Texture> *texture) {
  if (tex_idx < 0 || tex_idx >= tg_model.textures_count) {
    return;
  }

  const tg3_texture &tg_texture = tg_model.textures[tex_idx];

  if (tg_texture.source < 0 && tg_texture.source >= tg_model.images_count) {
    return;
  }

  const tg3_image &tg_image = tg_model.images[tg_texture.source];
  uint32_t width = tg_image.width, height = tg_image.height;
  int32_t channels = tg_image.component >= 0 ? tg_image.component : 3;
  const unsigned char *data = tg_image.image.data;

  TexcoordSampler sampler_u, sampler_v;
  get_texcoord_sampler(tg_model, tg_texture, &sampler_u, &sampler_v);

  if (data) {
    std::vector<TexColor3f> pixels(width * height);

    for (uint32_t h = 0; h < height; h++) {
      for (uint32_t w = 0; w < width; w++) {
        uint64_t idx = (h * width + w) * channels;
        float r = tg_image.image.data[idx] * UINT8_INV;
        float g = tg_image.image.data[idx + 1] * UINT8_INV;
        float b = tg_image.image.data[idx + 2] * UINT8_INV;

        pixels[h * width + w] = TexColor3f(r, g, b);
      }
    }

    *texture = std::move(std::make_unique<ImageTexture>(pixels, width, height,
                                                        sampler_u, sampler_v));
  } else {
    int32_t tex_buffer_view_idx = tg_image.buffer_view;
    if (tex_buffer_view_idx == TG3_INDEX_NONE) {
      return;
    }

    const tg3_buffer_view &tex_buffer_view =
        tg_model.buffer_views[tex_buffer_view_idx];
    const tg3_buffer &tg_buffer = tg_model.buffers[tex_buffer_view.buffer];

    const uint8_t *data = nullptr;
    size_t size = 0;

    if (!get_image_buffer_data(tex_buffer_view, tg_buffer, data, size)) {
      return;
    }

    auto pixels = load_image(data, size, &width, &height);
    if (pixels.has_value()) {
      *texture = std::move(std::make_unique<ImageTexture>(
          pixels.value(), width, height, sampler_u, sampler_v));
    }
  }
}

void load_gltf_material(const tg3_model &tg_model, const tg3_primitive &tg_prim,
                        std::unique_ptr<Bxdf> *bxdf, Color *emission,
                        std::unique_ptr<Texture> *texture) {
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
  double ior;

  if (is_transparent(tg_material, &ior)) {
    if (roughness == 0.) {
      *bxdf = std::move(std::make_unique<Glass>(base_color, ior));
    } else {
      *bxdf =
          std::move(std::make_unique<MicroBtdf>(base_color, ior, roughness));
    }
  } else {
    load_gltf_texture(tg_model, tg_pbr_info.base_color_texture.index, texture);

    if (metalic == 1. && roughness == 0.) {
      *bxdf = std::move(std::make_unique<Mirror>(base_color));
    } else if (metalic == 0. && roughness == 1.) {
      *bxdf = std::move(std::make_unique<Lambertian>(
          base_color, texture ? texture->get() : nullptr));
    } else if (metalic == 1.) {
      *bxdf = std::move(std::make_unique<MicroBrdf>(base_color, roughness));
    } else {
      *bxdf = std::move(std::make_unique<MixtureBrdf>(
          base_color, metalic, roughness, texture ? texture->get() : nullptr));
    }
  }

  // bxdf->get()->display();
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
