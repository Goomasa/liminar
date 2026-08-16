#pragma once

#include "util/utils.h"
#include <cstdio>
#include <stdio.h>

inline bool is_valid(const Vec3 &v) { return !v.is_nan() && (v.min() >= 0.); }

inline int gamma_(const double x) {
  return int(pow(clamp01(x), 1. / 2.2) * 255 + 0.5);
}

inline void save_ppm(const std::string &filename,
                     const std::vector<Color> &image, const uint32_t width,
                     const uint32_t height) {
  FILE *f;

#ifdef _MSC_VER
  auto error = fopen_s(&f, filename.c_str(), "wb");
  if (error != 0) {
    std::cout << "failed to write " << filename << std::endl;
    std::exit(1);
  }
#else
  f = fopen(filename.c_str(), "wb");
  if (!f) {
    std::cout << "failed to write " << filename << std::endl;
    std::exit(1);
  }
#endif

  fprintf(f, "P3\n%d %d\n%d\n", width, height, 256);

  for (int i = 0; i < width * height; i++) {
    fprintf(f, "%d %d %d ", gamma_(image[i].x), gamma_(image[i].y),
            gamma_(image[i].z));
  }
  fclose(f);

  std::cout << "saved" << std::endl;
}
