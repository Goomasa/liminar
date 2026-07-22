#pragma once

#include "renderer/material.h"
#include "scene/primitive.h"
#include "util/utils.h"
#include <memory>
#include <vector>

struct Scene {
  std::vector<std::unique_ptr<Shape>> shapes;
  std::vector<std::unique_ptr<Bxdf>> materials;
  std::vector<Primitive> primitives;

  Scene() {}

  // test function
  void set_sphere(const double radius, const Point3 &center,
                  const Color color) {
    std::unique_ptr<Shape> sphere = std::make_unique<Sphere>(radius, center);
    std::unique_ptr<Bxdf> bxdf = std::make_unique<Lambertian>(color);

    primitives.emplace_back(
        Primitive(sphere.get(), bxdf.get(), primitives.size()));
    shapes.emplace_back(std::move(sphere));
    materials.emplace_back(std::move(bxdf));
  }
};
