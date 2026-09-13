#include "scene.h"
#include "renderer/material.h"
#include "scene/primitive.h"
#include "tiny_gltf_v3.h"
#include "util/utils.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

inline Color load_mtl_emission(const auto &material) {
  return Vec3(material.emission[0], material.emission[1], material.emission[2]);
}

void Scene::load_obj(const std::string &filename, const std::string &dir_path,
                     const double scale, const Vec3 &tr) {
  tinyobj::ObjReaderConfig config;
  config.mtl_search_path = dir_path;
  config.triangulate = true;

  tinyobj::ObjReader reader;
  if (!reader.ParseFromFile(filename, config)) {
    std::cout << "failed to read " << filename << std::endl;
    std::exit(1);
  }

  if (!reader.Warning().empty()) {
    std::cout << reader.Warning() << std::endl;
  }

  const auto &obj_attrib = reader.GetAttrib();
  const auto &obj_shapes = reader.GetShapes();
  const auto &obj_materials = reader.GetMaterials();

  uint32_t start = primitives.size();

  for (const tinyobj::shape_t &obj_shape : obj_shapes) {
    uint32_t offset = 0;

    for (uint32_t f = 0; f < obj_shape.mesh.num_face_vertices.size(); f++) {
      Vertex vertices[3];

      for (uint8_t v = 0; v < 3; v++) {
        const auto idx = obj_shape.mesh.indices[offset + v];

        const float vx = obj_attrib.vertices[3 * idx.vertex_index + 0];
        const float vy = obj_attrib.vertices[3 * idx.vertex_index + 1];
        const float vz = obj_attrib.vertices[3 * idx.vertex_index + 2];

        const double nx = (double)obj_attrib.normals[3 * idx.normal_index + 0];
        const double ny = (double)obj_attrib.normals[3 * idx.normal_index + 1];
        const double nz = (double)obj_attrib.normals[3 * idx.normal_index + 2];

        vertices[v] = Vertex(Point3(vx, vy, vz) * scale + tr, Vec3(nx, ny, nz));
      }

      offset += 3;

      if (vertices[0].position == vertices[1].position ||
          vertices[1].position == vertices[2].position ||
          vertices[2].position == vertices[0].position) {
        continue;
      }

      std::unique_ptr<Shape> shape =
          std::make_unique<Triangle>(vertices[0], vertices[1], vertices[2]);

      Primitive prim = Primitive(shape.get(), primitives.size());
      shapes.emplace_back(std::move(shape));

      int32_t mtl_idx = obj_shape.mesh.material_ids[f];
      uint64_t hashed_bxdf_id =
          std::hash<std::string>()(filename + std::to_string(mtl_idx));
      auto corresponding_bxdf = search_bxdf(hashed_bxdf_id);

      if (corresponding_bxdf) {
        prim.bxdf = corresponding_bxdf->get();
        Color emission =
            mtl_idx < 0 ? Color() : load_mtl_emission(obj_materials[mtl_idx]);
        if (emission.max() > EPS) {
          prim.emission = emission;
        }
      } else {
        if (mtl_idx < 0) {
          std::unique_ptr<Bxdf> bxdf = std::make_unique<Lambertian>(Color(1.));
          prim.bxdf = bxdf.get();
          bxdfs.emplace_back(std::move(bxdf));
        } else {
          load_mtl(obj_materials[mtl_idx], &prim);
        }

        prim.bxdf->id = hashed_bxdf_id;
      }

      if (prim.is_light()) {
        lights.emplace_back(prim.id);
      }

      primitives.emplace_back(std::move(prim));
    }
  }
  std::cout << "loaded: " << filename << std::endl;
}

void Scene::load_mtl(const auto &material, Primitive *prim) {
  const Color kd =
      Vec3(material.diffuse[0], material.diffuse[1], material.diffuse[2]);

  const Color ke =
      Vec3(material.emission[0], material.emission[1], material.emission[2]);

  std::unique_ptr<Bxdf> bxdf;

  if (ke.max() > EPS) {
    prim->emission = ke;
    bxdf = std::make_unique<Lambertian>(kd);
    prim->bxdf = bxdf.get();
    bxdfs.emplace_back(std::move(bxdf));
    return;
  }

  switch (material.illum) {
  case 3: {
    bxdf = std::make_unique<Mirror>(kd);
  } break;
  case 5: {
    auto ns = material.shininess / 1000;
    bxdf = std::make_unique<MicroBrdf>(kd, ns);
  } break;
  case 7: {
    const double ior = material.ior;
    bxdf = std::make_unique<Glass>(kd, ior);
  } break;
  default: {
    bxdf = std::make_unique<Lambertian>(kd);
  } break;
  case 9: {
    auto ns = material.shininess / 1000;
    const double ior = material.ior;
    bxdf = std::make_unique<MicroBtdf>(kd, ior, ns);
  } break;
  }

  prim->bxdf = bxdf.get();
  bxdfs.emplace_back(std::move(bxdf));

  return;
}

/* --- gltf --- */

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

void Scene::load_gltf(const std::string &filename, const std::string &dir_path,
                      const double scale, const Vec3 &tr) {
  tg3_parse_options tg_options;
  tg3_error_stack tg_errors;
  tg3_model tg_model{};

  tg3_parse_options_init(&tg_options);
  tg3_error_stack_init(&tg_errors);

  auto tg_result = tg3_parse_file(
      &tg_model, &tg_errors, filename.c_str(),
      static_cast<uint32_t>(std::strlen(filename.c_str())), &tg_options);

  if (tg_result != TG3_OK) {
    for (uint32_t i = 0; i < tg_errors.count; ++i) {
      std::cerr << (tg_errors.entries[i].message ? tg_errors.entries[i].message
                                                 : "unknown error")
                << std::endl;
    }
  } else {
    std::cout << "meshes: " << tg_model.meshes_count << std::endl;
    std::cout << "lights: " << tg_model.lights_count << std::endl;
  }

  for (uint32_t mesh_i = 0; mesh_i < tg_model.meshes_count; mesh_i++) {
    const tg3_mesh &tg_mesh = tg_model.meshes[mesh_i];

    for (uint32_t prim_i = 0; prim_i < tg_mesh.primitives_count; prim_i++) {
      const tg3_primitive &tg_prim = tg_mesh.primitives[prim_i];
      const int32_t mode =
          tg_prim.mode == TG3_INDEX_NONE ? TG3_MODE_TRIANGLES : tg_prim.mode;
      if (mode != TG3_MODE_TRIANGLES) {
        std::cerr << "unsupported mode: " << filename << std::endl;
        continue;
      }

      int32_t position_accessor_idx = find_attrib_accessor(tg_prim, "POSITION");
      if (position_accessor_idx == TG3_INDEX_NONE) {
        std::cerr << "invalid POSITION accessor" << filename << std::endl;
        continue;
      }

      const tg3_accessor &tg_positions =
          tg_model.accessors[position_accessor_idx];
      if (tg_positions.component_type != TG3_COMPONENT_TYPE_FLOAT ||
          tg_positions.type != TG3_TYPE_VEC3) {
        std::cerr << "unsupported POSITION format: " << filename << std::endl;
        continue;
      }

      const uint8_t *tg_position_data = accessor_data(tg_model, tg_positions);
      if (!tg_position_data) {
        std::cerr << "no POSITION accessor: " << filename << std::endl;
        continue;
      }

      const tg3_buffer_view &tg_position_view =
          tg_model.buffer_views[tg_positions.buffer_view];
      const uint32_t tg_position_stride = tg_position_view.byte_stride != 0
                                              ? tg_position_view.byte_stride
                                              : sizeof(float) * 3;

      int32_t normal_accessor_idx = find_attrib_accessor(tg_prim, "NORMAL");
      if (normal_accessor_idx == TG3_INDEX_NONE) {
        continue;
      }

      const tg3_accessor &tg_normals = tg_model.accessors[normal_accessor_idx];
      if (tg_normals.component_type != TG3_COMPONENT_TYPE_FLOAT ||
          tg_normals.type != TG3_TYPE_VEC3) {
        std::cerr << "unsupported NORMAL format: " << filename << std::endl;
        continue;
      }

      const uint8_t *tg_normal_data = accessor_data(tg_model, tg_normals);
      if (!tg_normal_data) {
        std::cerr << "no NORMAL accessor: " << filename << std::endl;
        continue;
      }

      const tg3_buffer_view &tg_normal_view =
          tg_model.buffer_views[tg_normals.buffer_view];
      const uint32_t tg_normal_stride = tg_normal_view.byte_stride != 0
                                            ? tg_normal_view.byte_stride
                                            : sizeof(float) * 3;

      std::vector<Vertex> triangle_verts;
      // no indices
      if (tg_prim.indices == TG3_INDEX_NONE) {
        Vertex vertices[3];

        for (uint64_t i = 0; i + 2 < tg_positions.count; i += 3) {
          float tmp_vertices[3], tmp_normals[3];

          for (uint32_t j = 0; j < 3; ++j) {
            std::memcpy(&tmp_vertices,
                        tg_position_data + (i + j) * tg_position_stride,
                        sizeof(float) * 3);

            std::memcpy(&tmp_normals,
                        tg_normal_data + (i + j) * tg_normal_stride,
                        sizeof(float) * 3);

            Vec3 v = Vec3((double)tmp_vertices[0], (double)tmp_vertices[1],
                          (double)tmp_vertices[2]) *
                         scale +
                     tr;

            Vec3 n =
                normalize(Vec3((double)tmp_normals[0], (double)tmp_normals[1],
                               (double)tmp_normals[2]));
            triangle_verts.emplace_back(Vertex(v, n));
          }
        }
      } else {
        const tg3_accessor &tg_indices = tg_model.accessors[tg_prim.indices];
        if (tg_indices.type != TG3_TYPE_SCALAR) {
          std::cerr << "unsupported index type: " << filename << std::endl;
          continue;
        }

        const uint8_t *tg_index_data = accessor_data(tg_model, tg_indices);
        if (!tg_index_data) {
          std::cerr << "no index accessor: " << filename << std::endl;
          continue;
        }

        const tg3_buffer_view &tg_index_view =
            tg_model.buffer_views[tg_indices.buffer_view];

        const uint32_t tg_index_size =
            tg_indices.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE    ? 1
            : tg_indices.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT ? 2
            : tg_indices.component_type == TG3_COMPONENT_TYPE_UNSIGNED_INT   ? 4
                                                                           : 0;
        if (tg_index_size == 0) {
          std::cerr << "unsupported index component type: " << filename
                    << std::endl;
          continue;
        }

        const uint32_t tg_index_stride = tg_index_view.byte_stride != 0
                                             ? tg_index_view.byte_stride
                                             : tg_index_size;

        for (uint64_t i = 0; i + 2 < tg_indices.count; i += 3) {
          float tmp_vertices[3], tmp_normals[3];
          const uint32_t vertex_indices[3] = {
              read_tg_index(tg_index_data, i, tg_index_stride,
                            tg_indices.component_type),
              read_tg_index(tg_index_data, i + 1, tg_index_stride,
                            tg_indices.component_type),
              read_tg_index(tg_index_data, i + 2, tg_index_stride,
                            tg_indices.component_type),
          };

          for (uint32_t j = 0; j < 3; ++j) {
            if (vertex_indices[j] >= tg_positions.count ||
                vertex_indices[j] >= tg_normals.count) {
              std::cerr << "invalid index" << std::endl;
              break;
            }

            std::memcpy(&tmp_vertices,
                        tg_position_data +
                            vertex_indices[j] * tg_position_stride,
                        sizeof(float) * 3);

            std::memcpy(&tmp_normals,
                        tg_normal_data + vertex_indices[j] * tg_normal_stride,
                        sizeof(float) * 3);

            Vec3 v = Vec3((double)tmp_vertices[0], (double)tmp_vertices[1],
                          (double)tmp_vertices[2]) *
                         scale +
                     tr;

            Vec3 n =
                normalize(Vec3((double)tmp_normals[0], (double)tmp_normals[1],
                               (double)tmp_normals[2]));
            triangle_verts.emplace_back(Vertex(v, n));
          }
        }
      }

      for (uint32_t tidx = 0; tidx < triangle_verts.size() / 3; tidx++) {
        std::unique_ptr<Shape> shape = std::make_unique<Triangle>(
            triangle_verts[tidx * 3], triangle_verts[tidx * 3 + 1],
            triangle_verts[tidx * 3 + 2]);
        std::unique_ptr<Bxdf> bxdf = std::make_unique<Lambertian>(Color(1.));
        Primitive prim = Primitive(shape.get(), bxdf.get(), primitives.size());

        primitives.emplace_back(std::move(prim));
        bxdfs.emplace_back(std::move(bxdf));
        shapes.emplace_back(std::move(shape));
      }
    }
  }

  tg3_model_free(&tg_model);
  tg3_error_stack_free(&tg_errors);
}

bool Scene::intersect(Ray &ray, HitRecord *record,
                      const uint32_t node_id) const {
  if (bvh.tree[node_id].bbox.hit(ray, record)) {
    uint32_t left = bvh.tree[node_id].left;
    uint32_t right = bvh.tree[node_id].right;
    if (left & 1) {
      primitives[left >> 1].hit(ray, record);
    } else {
      intersect(ray, record, left >> 1);
    }

    if (right & 1) {
      primitives[right >> 1].hit(ray, record);
    } else {
      intersect(ray, record, right >> 1);
    }
  }

  return record->id != -1;
}

/* --- */

void Scene::set_test_primitive() {
  // std::unique_ptr<Shape> sphere =
  //     std::make_unique<Sphere>(Point3(12, 7, -15), 7);
  std::unique_ptr<Shape> sphere =
      std::make_unique<Sphere>(Point3(0, 25, 0), 15);
  std::unique_ptr<Bxdf> bxdf =
      std::make_unique<MixtureBrdf>(Color(1.), 0.1, 0.2);
  Primitive prim = Primitive(sphere.get(), bxdf.get(), primitives.size());
  // primitives.emplace_back(std::move(prim));
  // shapes.emplace_back(std::move(sphere));
  // bxdfs.emplace_back(std::move(bxdf));

  std::unique_ptr<Shape> cylinder =
      std::make_unique<Cylinder>(Point3(12, 7, -12), Vec3(-0.3, 0, -1), 7, 10);
  std::unique_ptr<Bxdf> bxdf2 = std::make_unique<Lambertian>(Color(1.));
  Primitive prim2 = Primitive(cylinder.get(), bxdf2.get(), primitives.size());
  // primitives.emplace_back(std::move(prim2));
  // shapes.emplace_back(std::move(cylinder));
  // bxdfs.emplace_back(std::move(bxdf2));
}

void Scene::set_sample_scene1() {
  Random rnd(1);
  for (uint32_t i = 0; i <= 20; i++) {
    for (uint32_t j = 0; j <= 20; j++) {
      Vec3 tr = Vec3((double)i * 20 - 200, -65 + rnd.gen() * 10,
                     (double)j * 20 - 200);
      load_obj("models/cube.obj", "models/", 10, tr);
    }
  }
}
