#include "util/image.h"
#include "util/utils.h"

int main() {
  uint32_t w = 300, h = 300;
  std::vector<Color> vec(w * h);

  for (uint32_t v = 0; v < h; v++) {
    for (uint32_t u = 0; u < w; u++) {
      vec[w * v + u] = Color((double)u / w, (double)v / h, 1.);
    }
  }

  save_ppm("sample.ppm", vec, w, h);
}
