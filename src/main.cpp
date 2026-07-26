#include "renderer/camera.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include <chrono>

int main() {
  Scene scene = Scene(Color(0.9));
  // scene.load_obj("models/cornell-box/CornellBox-Sphere.obj", 20.,
  //               Vec3(0, 5, 0));
  scene.load_obj("models/test-box/test-box.obj");

  PinholeModel camera =
      PinholeModel(300, 300, Point3(0, 25, 120), 30, Vec3(0, 0, -1), 40, 500.);

  /*
  LensModel camera = LensModel(400, 400, Point3(0, 25, 120), 30, Vec3(0, 0, -1),
                               42, 2, 96, 100);
  */

  Renderer renderer = Renderer(&scene, &camera, 8, 4);

  std::chrono::system_clock::time_point start, end;
  start = std::chrono::system_clock::now();

  renderer.render();
  // renderer.test_render();

  end = std::chrono::system_clock::now();
  double elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count();
  std::cout << elapsed / 1000 << " s" << std::endl;
}
