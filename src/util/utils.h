#pragma once

#include <cmath>
#include <cstdint>
#include <iostream>

constexpr double PI = 3.14159265358979323;
constexpr double PI_INV = 1.0 / PI;
constexpr double INF = 1e128;
constexpr double EPS = 1e-6;

struct Vec3 {
  double x, y, z;

  Vec3() : x(0.), y(0.), z(0.) {}

  Vec3(const double d) : x(d), y(d), z(d) {}

  Vec3(const double x, const double y, const double z) : x(x), y(y), z(z) {}

  inline Vec3 operator+(const Vec3 &v) const {
    return Vec3(x + v.x, y + v.y, z + v.z);
  }

  inline Vec3 operator-(const Vec3 &v) const {
    return Vec3(x - v.x, y - v.y, z - v.z);
  }

  inline Vec3 operator*(const double d) const {
    return Vec3(x * d, y * d, z * d);
  }

  inline Vec3 operator/(const double d) const {
    return Vec3(x / d, y / d, z / d);
  }

  inline Vec3 operator*(const Vec3 &v) const {
    return Vec3(x * v.x, y * v.y, z * v.z);
  }

  inline Vec3 operator/(const Vec3 &v) const {
    return Vec3(x / v.x, y / v.y, z / v.z);
  }

  inline double operator[](const uint8_t i) const {
    // i=0,1,2
    return (&x)[i];
  }

  inline const double len2() const { return x * x + y * y + z * z; }

  inline const double len() const { return std::sqrt(this->len2()); }

  inline void display() const {
    std::cout << x << ", " << y << ", " << z << std::endl;
  }

  inline double max() const { return fmax(x, fmax(y, z)); }

  inline double min() const { return fmin(x, fmin(y, z)); }

  inline bool is_nan() const { return isnan(x) || isnan(y) || isnan(z); }

  inline void set(const double d, const uint8_t i) { (&x)[i] = d; }
};

inline Vec3 operator*(const double d, const Vec3 &v) { return v * d; }

inline Vec3 normalize(const Vec3 &v) { return v / (v.len()); }

inline double dot(const Vec3 &v1, const Vec3 &v2) {
  return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

inline Vec3 cross(const Vec3 &v1, const Vec3 &v2) {
  return Vec3(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z,
              v1.x * v2.y - v1.y * v2.x);
}

inline Vec3 vmin(const Vec3 &v1, const Vec3 &v2) {
  return Vec3(fmin(v1.x, v2.x), fmin(v1.y, v2.y), fmin(v1.z, v2.z));
}

inline Vec3 vmax(const Vec3 &v1, const Vec3 &v2) {
  return Vec3(fmax(v1.x, v2.x), fmax(v1.y, v2.y), fmax(v1.z, v2.z));
}

typedef Vec3 Color;
typedef Vec3 Point3;

inline double clamp01(const double d) { return fmin(1., fmax(d, 0.)); }
