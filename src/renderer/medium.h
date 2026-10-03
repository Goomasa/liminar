#pragma once

#include "util/random.h"
#include "util/utils.h"
#include <cmath>
#include <cstdint>
#include <exception>
#include <map>
#include <sstream>
#include <string>

struct Medium {
  // homogeneous medium, no emission
  Color sigma_a, sigma_s, sigma_t;
  Color scattering_albedo;
  double g;
  uint64_t id;

  Medium(const Color &sigma_a, const Color &sigma_s, const double g)
      : sigma_a(sigma_a), sigma_s(sigma_s), g(g) {
    sigma_t = sigma_a + sigma_s;
    scattering_albedo = sigma_s / sigma_t;
  }

  double sample_distance(const Color &throughput, Random *rnd) const {
    Vec3 discrete_pdf = throughput / throughput.sum();
    double r = rnd->gen();
    uint8_t sampled_idx;
    if (r < discrete_pdf.x) {
      sampled_idx = 0;
    } else if (r < discrete_pdf.x + discrete_pdf.y) {
      sampled_idx = 1;
    } else {
      sampled_idx = 2;
    }

    return -1. * log(1. - rnd->gen()) / sigma_t[sampled_idx];
  }

  inline double pdf_sample_disctance(const double st,
                                     const double distance) const {
    return st * exp(-1. * st * distance);
  }

  double pdf_sample_distance(const Color &throughput, const double d) const {
    Vec3 discrete_pdf = throughput / throughput.sum();
    return discrete_pdf.x * pdf_sample_disctance(sigma_t.x, d) +
           discrete_pdf.y * pdf_sample_disctance(sigma_t.y, d) +
           discrete_pdf.z * pdf_sample_disctance(sigma_t.z, d);
  }

  double calc_transmittance(const Color &throughput, const double d) const {
    Vec3 discrete_pdf = throughput / throughput.sum();
    return discrete_pdf.x * exp(-sigma_t.x * d) +
           discrete_pdf.y * exp(-sigma_t.y * d) +
           discrete_pdf.z * exp(-sigma_t.z * d);
  }

  Vec3 sample_phasefn(const Vec3 &dir_prev, Random *rnd) const {
    // Henyey–Greenstein phase function
    double r = rnd->gen();
    double tmp = (1. - g * g) / (1. + g - 2. * g * r);
    double cos_theta =
        g == 0. ? 1. - 2 * r : (-0.5 / g) * (1. + g * g - tmp * tmp);
    double sin_theta = sqrt(1. - cos_theta * cos_theta);
    double phi = 2. * PI * rnd->gen();

    Vec3 w = dir_prev, u, v;
    if (fabs(w.x) > EPS) {
      u = normalize(cross(Vec3(0, 1, 0), w));
    } else {
      u = normalize(cross(Vec3(1, 0, 0), w));
    }
    v = cross(w, u);

    return normalize(u * sin_theta * cos(phi) + v * sin_theta * sin(phi) +
                     w * cos_theta);
  }

  inline double eval_phasefn(const Vec3 &dir_prev, const Vec3 &dir_next) const {
    double cos_theta = dot(dir_prev, dir_next);
    double denom = pow(1. + g * g + 2 * g * cos_theta, 1.5);
    return 0.25 * PI_INV * (1 - g * g) / denom;
  }

  void display() const { sigma_t.display(); }
};

static bool
get_medium_parameter(std::map<std::string, std::string> &unknown_params,
                     const std::string &name, Color *value) {
  const std::string &param_str = unknown_params[name];
  std::string word;
  std::stringstream param_ss{param_str};

  std::vector<double> v(3, 0.1);
  for (uint32_t i = 0; i < 3; i++) {
    if (std::getline(param_ss, word, ' ')) {
      try {
        v[i] = std::stod(word);
      } catch (std::exception e) {
        std::cout << e.what() << std::endl;
        return false;
      }
    } else {
      return false;
    }
  }

  *value = Color(v[0], v[1], v[2]);
  return true;
}

static bool
get_medium_parameter(std::map<std::string, std::string> &unknown_params,
                     const std::string &name, double *value) {
  const std::string &param_str = unknown_params[name];

  try {
    *value = std::stod(param_str);
    return true;
  } catch (std::exception e) {
    std::cout << e.what() << std::endl;
    return false;
  }
}
