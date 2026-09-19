#include "image.h"
#include "util/utils.h"
#include <cstdint>
#include <filesystem>

#define STBI_NO_TGA
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

constexpr uint32_t RGB_CH = 3;
constexpr uint32_t MAX_WIDTH_HEIGHT = 16384;

std::optional<std::vector<TexColor3f>>
load_image(const std::string &filename, uint32_t *width, uint32_t *height) {
  std::filesystem::path project_dir = ".";
  std::string filepath;
  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(project_dir)) {
    if (entry.is_regular_file() && entry.path().filename() == filename) {
      filepath = entry.path().string<char>();
    }
  }

  int channels, w_, h_;

  if (stbi_is_hdr(filepath.c_str())) {
    float *data = stbi_loadf(filepath.c_str(), &w_, &h_, &channels, RGB_CH);
    if (!data || w_ > MAX_WIDTH_HEIGHT || h_ > MAX_WIDTH_HEIGHT || w_ < 0 ||
        h_ < 0) {
      std::cerr << "failed to load " << filepath << std::endl;
      return std::nullopt;
    }

    *width = w_;
    *height = h_;
    uint64_t data_size = w_ * h_;
    std::vector<TexColor3f> pixels(data_size);

    for (uint64_t i = 0; i < data_size; i++) {
      pixels[i] = TexColor3f(data[3 * i], data[3 * i + 1], data[3 * i + 2]);
    }

    std::cout << "loaded: " << filepath << std::endl;
    stbi_image_free(data);
    return pixels;
  } else {
    // png, jpeg
    unsigned char *data =
        stbi_load(filepath.c_str(), &w_, &h_, &channels, RGB_CH);
    if (!data || w_ > MAX_WIDTH_HEIGHT || h_ > MAX_WIDTH_HEIGHT || w_ < 0 ||
        h_ < 0) {
      std::cerr << "failed to load " << filepath << std::endl;
      return std::nullopt;
    }

    *width = w_;
    *height = h_;
    uint64_t data_size = w_ * h_;
    std::vector<TexColor3f> pixels(data_size);

    for (uint64_t i = 0; i < data_size; i++) {
      pixels[i] = TexColor3f(static_cast<float>(data[3 * i]),
                             static_cast<float>(data[3 * i + 1]),
                             static_cast<float>(data[3 * i + 2])) *
                  UINT8_INV;
    }

    std::cout << "loaded: " << filepath << std::endl;
    stbi_image_free(data);
    return pixels;
  }
}

std::optional<std::vector<TexColor3f>> load_image(const uint8_t *buffer,
                                                  const size_t size,
                                                  uint32_t *width,
                                                  uint32_t *height) {
  int w_, h_, channels;
  unsigned char *data =
      stbi_load_from_memory(buffer, size, &w_, &h_, &channels, RGB_CH);
  if (!data || w_ > MAX_WIDTH_HEIGHT || h_ > MAX_WIDTH_HEIGHT || w_ < 0 ||
      h_ < 0) {
    return std::nullopt;
  }

  *width = w_;
  *height = h_;
  uint64_t data_size = w_ * h_;
  std::vector<TexColor3f> pixels(data_size);

  for (uint64_t i = 0; i < data_size; i++) {
    pixels[i] = TexColor3f(static_cast<float>(data[3 * i]),
                           static_cast<float>(data[3 * i + 1]),
                           static_cast<float>(data[3 * i + 2])) *
                UINT8_INV;
  }

  stbi_image_free(data);
  return pixels;
}

void test_load_image(const std::string &filename) {
  uint32_t width, height;
  auto pixels = load_image(filename, &width, &height);

  if (pixels.has_value()) {
    std::cout << "width: " << width << ", height: " << height << std::endl;
    pixels.value()[0].display();
  }
}
