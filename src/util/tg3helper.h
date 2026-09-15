#pragma once

#include "renderer/material.h"
#include "tiny_gltf_v3.h"
#include "util/utils.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

inline int32_t find_attrib_accessor(const tg3_primitive &tg_prim,
                                    const std::string &semantic) {
  for (uint32_t i = 0; i < tg_prim.attributes_count; i++) {
    const tg3_str_int_pair &tg_attrib = tg_prim.attributes[i];

    if (tg_attrib.key.len == semantic.size() &&
        std::memcmp(tg_attrib.key.data, semantic.data(), semantic.size()) ==
            0) {
      return tg_attrib.value;
    }
  }
  return TG3_INDEX_NONE;
}

inline const uint8_t *accessor_data(const tg3_model &tg_model,
                                    const tg3_accessor &tg_accessor) {
  if (tg_accessor.buffer_view == TG3_INDEX_NONE) {
    return nullptr;
  }

  const tg3_buffer_view &view = tg_model.buffer_views[tg_accessor.buffer_view];
  const tg3_buffer &buffer = tg_model.buffers[view.buffer];

  return buffer.data.data + view.byte_offset + tg_accessor.byte_offset;
}

static uint32_t read_tg_index(const uint8_t *data, uint64_t index,
                              uint32_t stride, int32_t component_type) {
  const uint8_t *p = data + index * stride;

  switch (component_type) {
  case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
    return *p;

  case TG3_COMPONENT_TYPE_UNSIGNED_SHORT: {
    uint16_t value;
    std::memcpy(&value, p, sizeof(value));
    return value;
  }

  case TG3_COMPONENT_TYPE_UNSIGNED_INT: {
    uint32_t value;
    std::memcpy(&value, p, sizeof(value));
    return value;
  }

  default:
    return UINT32_MAX;
  }
}

inline Color load_gltf_emission(const tg3_model &tg_model,
                                const tg3_primitive &tg_prim) {
  if (tg_prim.material < 0 ||
      tg_prim.material >= static_cast<int>(tg_model.materials_count)) {
    return Color();
  }

  const tg3_material &tg_material = tg_model.materials[tg_prim.material];

  return Color(tg_material.emissive_factor[0], tg_material.emissive_factor[1],
               tg_material.emissive_factor[2]);
}

void load_gltf_material(const tg3_model &tg_model, const tg3_primitive &tg_prim,
                        std::unique_ptr<Bxdf> *bxdf, Color *emission);

Mat4 get_node_transform_mat(const tg3_node &node);

Mat4 get_transform_mat(const tg3_model &tg_model, const uint32_t node_idx);
