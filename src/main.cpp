#include "renderer/camera.h"
#include "renderer/renderer.h"
#include "scene/scene.h"

int main() {
  Scene scene = Scene();
  scene.load_obj("models/test-box.obj");
  PinholeModel camera =
      PinholeModel(400, 400, Point3(0, 25, 100), 30, Vec3(0, 0, -1), 30, 100);

  Renderer renderer = Renderer(&scene, &camera);
  renderer.test_render();
}
