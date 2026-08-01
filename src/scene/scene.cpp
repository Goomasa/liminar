#include "scene.h"

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

  const auto &attrib = reader.GetAttrib();
  const auto &shapes = reader.GetShapes();
  const auto &materials = reader.GetMaterials();

  for (const tinyobj::shape_t &shape : shapes) {
    uint32_t offset = 0;

    for (uint32_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
      Point3 vertices[3];
      Vec3 normals[3] = {Vec3(-INF), Vec3(-INF), Vec3(-INF)};

      for (uint8_t v = 0; v < 3; v++) {
        const auto idx = shape.mesh.indices[offset + v];

        const float vx = attrib.vertices[3 * idx.vertex_index + 0];
        const float vy = attrib.vertices[3 * idx.vertex_index + 1];
        const float vz = attrib.vertices[3 * idx.vertex_index + 2];

        const double nx = (double)attrib.normals[3 * idx.normal_index + 0];
        const double ny = (double)attrib.normals[3 * idx.normal_index + 1];
        const double nz = (double)attrib.normals[3 * idx.normal_index + 2];

        vertices[v] = Point3((double)vx, (double)vy, (double)vz) * scale + tr;
        normals[v] = Vec3(nx, ny, nz);
      }

      Vec3 normal = (normals[0] + normals[1] + normals[2]) / 3;
      if (normal.x < 0) {
        normal = normalize(
            cross(vertices[1] - vertices[0], vertices[2] - vertices[0]));
      }

      Primitive prim =
          Primitive(Triangle(vertices[0], vertices[1], vertices[2], normal),
                    primitives.size());

      int32_t material_id = shape.mesh.material_ids[f];
      if (material_id < 0) {
        std::unique_ptr<Bxdf> bxdf = std::make_unique<Lambertian>(Color(1.));
        prim.bxdf = bxdf.get();
        bxdfs.emplace_back(std::move(bxdf));
      } else {
        load_mtl(materials[material_id], &prim);
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

  if (ke.max() > 0) {
    prim->emission = ke;
    return;
  }

  switch (material.illum) {
  case 5: {

  } break;
  case 7: {
    const double ior = material.ior;

  } break;
  default: {
    std::unique_ptr<Bxdf> bxdf = std::make_unique<Lambertian>(kd);
    prim->bxdf = bxdf.get();
    bxdfs.emplace_back(std::move(bxdf));
  } break;
  }

  return;
}
