#pragma once

#include "util/utils.h"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

constexpr float UINT8_INV = 1. / 255;

inline bool is_valid(const Vec3 &v) { return !v.is_nan() && (v.min() >= 0.); }

inline int gamma_(const double x) {
  return int(pow(clamp01(x), 1. / 2.2) * 255 + 0.5);
}

inline TexColor3f gamma_inv(const TexColor3f c) {
  return TexColor3f(clamp01f(powf(c.x * UINT8_INV, 2.2f)),
                    clamp01f(powf(c.y * UINT8_INV, 2.2f)),
                    clamp01f(powf(c.z * UINT8_INV, 2.2f)));
}

inline void save_ppm(const std::string &filename,
                     const std::vector<Color> &image, const uint32_t width,
                     const uint32_t height) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    std::cerr << "failed to open file: " << filename << std::endl;
    exit(1);
  }

  file << "P3\n" << width << " " << height << "\n" << 256 << std::endl;

  for (int i = 0; i < width * height; i++) {
    file << gamma_(image[i].x) << " " << gamma_(image[i].y) << " "
         << gamma_(image[i].z) << std::endl;
  }

  file.close();
  std::cout << "saved: " << filename << std::endl;
}

std::optional<std::vector<TexColor3f>>
load_image(const std::string &filename, uint32_t *width, uint32_t *height);

std::optional<std::vector<TexColor3f>> load_image(const uint8_t *buffer,
                                                  const size_t size,
                                                  uint32_t *width,
                                                  uint32_t *height);

void test_load_image(const std::string &filename);
