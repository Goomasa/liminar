#include "scene.h"
#include "renderer/material.h"
#include "scene/primitive.h"
#include "util/utils.h"
#include <memory>

#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

void Scene::load_obj(const std::string &filename, const double scale,
                     const Vec3 &tr) {
  tinyobj::ObjReaderConfig config;
  config.mtl_search_path = "";
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

        vertices[v] =
            Vertex(Point3((double)vx, (double)vy, (double)vz) * scale + tr,
                   Vec3(nx, ny, nz));
      }

      std::unique_ptr<Shape> shape =
          std::make_unique<Triangle>(vertices[0], vertices[1], vertices[2]);

      Primitive prim = Primitive(shape.get(), primitives.size());
      shapes.emplace_back(std::move(shape));

      int32_t material_id = obj_shape.mesh.material_ids[f];
      if (material_id < 0) {
        std::unique_ptr<Bxdf> bxdf = std::make_unique<Lambertian>(Color(1.));
        prim.bxdf = bxdf.get();
        bxdfs.emplace_back(std::move(bxdf));
      } else {
        load_mtl(obj_materials[material_id], &prim);
      }

      if (prim.is_light()) {
        lights.emplace_back(prim.id);
      }

      primitives.emplace_back(std::move(prim));

      offset += 3;
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

  if (ke.max() > 0) {
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
  }

  prim->bxdf = bxdf.get();
  bxdfs.emplace_back(std::move(bxdf));

  return;
}

void Scene::set_sphere() {
  std::unique_ptr<Shape> sphere =
      std::make_unique<Sphere>(Point3(12, 7, -15), 7);
  std::unique_ptr<Bxdf> bxdf = std::make_unique<MicroBrdf>(Color(1.), 0.3);
  Primitive prim = Primitive(sphere.get(), bxdf.get(), primitives.size());
  primitives.emplace_back(std::move(prim));
  shapes.emplace_back(std::move(sphere));
  bxdfs.emplace_back(std::move(bxdf));
}
