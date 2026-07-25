#include "renderer/camera.h"
#include "renderer/renderer.h"
#include "scene/scene.h"

int main() {
  Scene scene = Scene();
  scene.load_obj("models/test-box.obj");

  PinholeModel camera =
      PinholeModel(300, 300, Point3(0, 25, 120), 30, Vec3(0, 0, -1), 40, 500.);

  /*
  LensModel camera = LensModel(400, 400, Point3(0, 25, 120), 30, Vec3(0, 0, -1),
                               42, 2, 96, 100);
  */

  Renderer renderer = Renderer(&scene, &camera, 4, 2);
  renderer.render();
}
