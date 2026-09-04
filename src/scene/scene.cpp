#include "scene.h"
#include "renderer/material.h"
#include "scene/primitive.h"
#include "util/utils.h"
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
      auto correspoing_bxdf = search_bxdf(hashed_bxdf_id);

      if (correspoing_bxdf) {
        prim.bxdf = correspoing_bxdf->get();
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
