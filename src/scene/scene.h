#pragma once

#define TINYOBJLOADER_IMPLEMENTATION

#include "renderer/material.h"
#include "renderer/ray.h"
#include "scene/primitive.h"
#include "tinyobjloader/tiny_obj_loader.h"
#include "util/utils.h"

#include <memory>
#include <utility>
#include <vector>

struct Scene {
  std::vector<std::unique_ptr<Bxdf>> bxdfs;
  std::vector<Primitive> primitives;

  Scene() {}

  void load_obj(const std::string &filename) {
    tinyobj::ObjReaderConfig config;
    config.mtl_search_path = "models/";
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
        Vec3 normals[3] = {Vec3(-INF, -INF, -INF), Vec3(-INF, -INF, -INF),
                           Vec3(-INF, -INF, -INF)};

        for (uint8_t v = 0; v < 3; v++) {
          const auto idx = shape.mesh.indices[offset + v];

          const float vx = attrib.vertices[3 * idx.vertex_index + 0];
          const float vy = attrib.vertices[3 * idx.vertex_index + 1];
          const float vz = attrib.vertices[3 * idx.vertex_index + 2];

          const double nx = (double)attrib.normals[3 * idx.normal_index + 0];
          const double ny = (double)attrib.normals[3 * idx.normal_index + 1];
          const double nz = (double)attrib.normals[3 * idx.normal_index + 2];

          vertices[v] = Point3((double)vx, (double)vy, (double)vz);
          normals[v] = Vec3(nx, ny, nz);
        }

        Vec3 normal = (normals[0] + normals[1] + normals[2]) / 3;
        if (normal.x < 0) {
          normal = normalize(
              cross(vertices[1] - vertices[0], vertices[2] - vertices[0]));
        }

        std::unique_ptr<Bxdf> bxdf = std::make_unique<Lambertian>(Color());

        Primitive prim =
            Primitive(Triangle(vertices[0], vertices[1], vertices[2], normal),
                      bxdf.get(), primitives.size());

        bxdfs.emplace_back(std::move(bxdf));
        primitives.emplace_back(std::move(prim));

        offset += 3;
      }
    }
  }

  bool test_intersect(Ray &ray, HitRecord *record) const {
    for (Primitive prim : primitives) {
      prim.hit(ray, record);
    }

    return record->id != -1;
  }
};
