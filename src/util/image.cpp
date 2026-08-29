#include "image.h"
#include "util/utils.h"

#define STBI_NO_TGA
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

constexpr uint32_t RGB_CH = 3;
constexpr uint32_t MAX_WIDTH_HEIGHT = 16384;
constexpr float UINT8_INV = 1. / 255;

std::optional<std::vector<TexColor3f>>
load_image(const std::string &filename, uint32_t *width, uint32_t *height) {
  int channels, w_, h_;

  if (stbi_is_hdr(filename.c_str())) {
    float *data = stbi_loadf(filename.c_str(), &w_, &h_, &channels, RGB_CH);
    if (!data || w_ > MAX_WIDTH_HEIGHT || h_ > MAX_WIDTH_HEIGHT || w_ < 0 ||
        h_ < 0) {
      std::cerr << "failed to load " << filename << std::endl;
      return std::nullopt;
    }

    *width = w_;
    *height = h_;
    uint64_t data_size = w_ * h_;
    std::vector<TexColor3f> pixels(data_size);

    for (uint64_t i = 0; i < data_size; i++) {
      pixels[i] = TexColor3f(data[3 * i], data[3 * i + 1], data[3 * i + 2]);
    }

    stbi_image_free(data);
    return pixels;
  } else {
    // png, jpeg
    unsigned char *data =
        stbi_load(filename.c_str(), &w_, &h_, &channels, RGB_CH);
    if (!data || w_ > MAX_WIDTH_HEIGHT || h_ > MAX_WIDTH_HEIGHT || w_ < 0 ||
        h_ < 0) {
      std::cerr << "failed to load " << filename << std::endl;
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
}
