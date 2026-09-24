#include "renderer/material.h"
#include "scene.h"
#include "scene/primitive.h"
#include "scene/texture.h"
#include "util/image.h"
#include "util/random.h"
#include "util/utils.h"
#include <cstdint>
#include <memory>

constexpr uint32_t LOBBY_GRID_NUM = 21;
constexpr double LOBBY_WALL_WIDTH = 40.;
constexpr double LOBBY_WALL_HEIGHT = 50.;
constexpr double LOBBY_LIGHT_LEN = 8.;
constexpr double LOBBY_TOTAL_WIDTH = LOBBY_WALL_WIDTH * (double)LOBBY_GRID_NUM;
constexpr double LOBBY_FLOOR_TEX_LEN = 40.;
constexpr double LOBBY_WALL_TEX_LEN = 4;
constexpr double LOBBY_CEILING_TEX_LEN = 40;

void Scene::model_lobby() {
  Random rnd(0);
  Point3 floor_minp(LOBBY_TOTAL_WIDTH * -0.5, 0, LOBBY_TOTAL_WIDTH * -0.5);
  Point3 floor_maxp =
      floor_minp + Vec3(LOBBY_TOTAL_WIDTH, 0, LOBBY_TOTAL_WIDTH);
  Point3 ceiling_minp = floor_minp + Vec3(0, LOBBY_WALL_HEIGHT, 0);
  Point3 ceiling_maxp = floor_maxp + Vec3(0, LOBBY_WALL_HEIGHT, 0);

  std::unique_ptr<Texture> wall_texture, floor_texture, ceiling_texture;
  load_image("lobby_wall.jpg", &wall_texture);
  load_image("carpet.jpg", &floor_texture);
  load_image("ceiling.jpg", &ceiling_texture);

  std::unique_ptr<Bxdf> bxdf_wall = std::make_unique<Lambertian>(
      Color(1.), wall_texture ? wall_texture.get() : nullptr);
  std::unique_ptr<Bxdf> bxdf_floor = std::make_unique<Lambertian>(
      Color(1.), floor_texture ? floor_texture.get() : nullptr);
  std::unique_ptr<Bxdf> bxdf_ceiling = std::make_unique<Lambertian>(
      Color(1.), ceiling_texture ? ceiling_texture.get() : nullptr);

  std::unique_ptr<Shape> shape_floor = std::make_unique<Square>(
      floor_minp, floor_maxp, Vec3(0, 1, 0), 1, LOBBY_FLOOR_TEX_LEN);
  Primitive floor =
      Primitive(shape_floor.get(), bxdf_floor.get(), primitives.size());
  primitives.emplace_back(std::move(floor));
  shapes.emplace_back(std::move(shape_floor));

  std::unique_ptr<Shape> shape_ceiling = std::make_unique<Square>(
      ceiling_minp, ceiling_maxp, Vec3(0, -1, 0), 1, LOBBY_CEILING_TEX_LEN);
  Primitive ceiling =
      Primitive(shape_ceiling.get(), bxdf_ceiling.get(), primitives.size());
  primitives.emplace_back(std::move(ceiling));
  shapes.emplace_back(std::move(shape_ceiling));

  for (uint32_t x = 0; x < LOBBY_GRID_NUM; x++) {
    for (uint32_t z = 0; z < LOBBY_GRID_NUM; z++) {
      double dl = (LOBBY_WALL_WIDTH - LOBBY_LIGHT_LEN) * 0.5;
      Point3 light_minp = floor_minp +
                          Vec3(LOBBY_WALL_WIDTH * x, 0, LOBBY_WALL_WIDTH * z) +
                          Vec3(dl, LOBBY_WALL_HEIGHT - 0.1, dl);

      std::unique_ptr<Shape> shape_light = std::make_unique<Square>(
          light_minp, light_minp + Vec3(LOBBY_LIGHT_LEN, 0, LOBBY_LIGHT_LEN),
          Vec3(0, -1, 0), 1, 10);
      Primitive light = Primitive(shape_light.get(), bxdf_wall.get(),
                                  Color(30.) * rnd.gen(), primitives.size());

      double light_prob =
          x == LOBBY_GRID_NUM / 2 && z < LOBBY_GRID_NUM / 2 ? 0.3 : 0.;

      if (rnd.gen() < light_prob) {
        primitives.emplace_back(std::move(light));
        shapes.emplace_back(std::move(shape_light));
      }
    }
  }

  std::unique_ptr<Shape> shape_edge_wall1 = std::make_unique<Square>(
      floor_minp, floor_minp + Vec3(LOBBY_TOTAL_WIDTH, LOBBY_WALL_HEIGHT, 0),
      Vec3(0, 0, 1), 2, LOBBY_WALL_TEX_LEN);
  Primitive edge_wall1 =
      Primitive(shape_edge_wall1.get(), bxdf_wall.get(), primitives.size());
  primitives.emplace_back(std::move(edge_wall1));
  shapes.emplace_back(std::move(shape_edge_wall1));

  std::unique_ptr<Shape> shape_edge_wall2 = std::make_unique<Square>(
      floor_minp + Vec3(0, 0, LOBBY_TOTAL_WIDTH), ceiling_maxp, Vec3(0, 0, 1),
      2, LOBBY_WALL_TEX_LEN);
  Primitive edge_wall2 =
      Primitive(shape_edge_wall2.get(), bxdf_wall.get(), primitives.size());
  primitives.emplace_back(std::move(edge_wall2));
  shapes.emplace_back(std::move(shape_edge_wall2));

  std::unique_ptr<Shape> shape_edge_wall3 = std::make_unique<Square>(
      floor_minp, floor_minp + Vec3(0, LOBBY_WALL_HEIGHT, LOBBY_TOTAL_WIDTH),
      Vec3(1, 0, 0), 0, LOBBY_WALL_TEX_LEN);
  Primitive edge_wall3 =
      Primitive(shape_edge_wall3.get(), bxdf_wall.get(), primitives.size());
  primitives.emplace_back(std::move(edge_wall3));
  shapes.emplace_back(std::move(shape_edge_wall3));

  std::unique_ptr<Shape> shape_edge_wall4 = std::make_unique<Square>(
      floor_minp + Vec3(LOBBY_TOTAL_WIDTH, 0, 0), ceiling_maxp, Vec3(-1, 0, 0),
      0, LOBBY_WALL_TEX_LEN);
  Primitive edge_wall4 =
      Primitive(shape_edge_wall4.get(), bxdf_wall.get(), primitives.size());
  primitives.emplace_back(std::move(edge_wall4));
  shapes.emplace_back(std::move(shape_edge_wall4));

  bool grids[LOBBY_GRID_NUM][LOBBY_GRID_NUM];

  for (uint32_t z = 0; z < LOBBY_GRID_NUM; z++) {
    for (uint32_t x = 0; x < LOBBY_GRID_NUM; x++) {
      grids[z][x] = (x % 2 == 1 && z % 2 == 1 && rnd.gen() < 0.7) ||
                    ((x % 2 == 0 || z % 2 == 0) && rnd.gen() < 0.2);

      if (x == LOBBY_GRID_NUM / 2) {
        grids[z][x] = false;
      }
      std::cout << grids[z][x] << " ";
    }
    std::cout << std::endl;
  }

  for (uint32_t z = 1; z < LOBBY_GRID_NUM; z++) {
    double width = 0;
    for (uint32_t x = 0; x < LOBBY_GRID_NUM; x++) {
      bool has_wall = grids[z][x] ^ grids[z - 1][x];
      if (has_wall) {
        width++;
      } else if (width > 0 && !has_wall) {
        Point3 wall_minp = floor_minp + Vec3(LOBBY_WALL_WIDTH * (x - width), 0,
                                             LOBBY_WALL_WIDTH * z);
        Point3 wall_maxp =
            wall_minp + Vec3(LOBBY_WALL_WIDTH * width, LOBBY_WALL_HEIGHT, 0);
        std::unique_ptr<Shape> shape_wall = std::make_unique<Square>(
            wall_minp, wall_maxp, Vec3(0, 0, 1), 2, LOBBY_WALL_TEX_LEN);
        Primitive wall =
            Primitive(shape_wall.get(), bxdf_wall.get(), primitives.size());
        primitives.emplace_back(std::move(wall));
        shapes.emplace_back(std::move(shape_wall));
        width = 0;
      }
    }
  }

  for (uint32_t x = 1; x < LOBBY_GRID_NUM; x++) {
    double width = 0;
    for (uint32_t z = 0; z < LOBBY_GRID_NUM; z++) {
      bool has_wall = grids[z][x] ^ grids[z][x - 1];
      if (has_wall) {
        width++;
      } else if (width > 0 && !has_wall) {
        Point3 wall_minp = floor_minp + Vec3(LOBBY_WALL_WIDTH * x, 0,
                                             LOBBY_WALL_WIDTH * (z - width));
        Point3 wall_maxp =
            wall_minp + Vec3(0, LOBBY_WALL_HEIGHT, LOBBY_WALL_WIDTH * width);
        std::unique_ptr<Shape> shape_wall = std::make_unique<Square>(
            wall_minp, wall_maxp, Vec3(1, 0, 0), 0, LOBBY_WALL_TEX_LEN);
        Primitive wall =
            Primitive(shape_wall.get(), bxdf_wall.get(), primitives.size());
        primitives.emplace_back(std::move(wall));
        shapes.emplace_back(std::move(shape_wall));
        width = 0;
      }
    }
  }

  bxdfs.emplace_back(std::move(bxdf_wall));
  bxdfs.emplace_back(std::move(bxdf_floor));
  bxdfs.emplace_back(std::move(bxdf_ceiling));
  textures.emplace_back(std::move(wall_texture));
  textures.emplace_back(std::move(floor_texture));
  textures.emplace_back(std::move(ceiling_texture));
}
