#pragma once

#include <cmath>
#include <cstdint>
#include <iostream>

constexpr double PI = 3.14159265358979323;
constexpr double PI_INV = 1.0 / PI;
constexpr double INF = 1e128;
constexpr double EPS = 1e-6;

struct Vec2 {
  double u, v;

  Vec2() : u(0.), v(0.) {}

  Vec2(const double d) : u(d), v(d) {}

  Vec2(const double u, const double v) : u(u), v(v) {}

  inline Vec2 operator+(const Vec2 &vec) const {
    return Vec2(u + vec.u, v + vec.v);
  }

  inline Vec2 operator-(const Vec2 &vec) const {
    return Vec2(u - vec.u, v - vec.v);
  }

  inline Vec2 operator*(const double d) const { return Vec2(u * d, v * d); }

  inline Vec2 operator/(const double d) const { return Vec2(u / d, v / d); }

  inline Vec2 operator*(const Vec2 &vec) const {
    return Vec2(u * vec.u, v * vec.v);
  }

  inline Vec2 operator/(const Vec2 &vec) const {
    return Vec2(u / vec.u, v / vec.v);
  }

  inline double operator[](const uint8_t i) const {
    // i=0,1
    return (&u)[i % 2];
  }

  inline void display() const { std::cout << u << ", " << v << std::endl; }

  inline double max() const { return fmax(u, v); }

  inline double min() const { return fmin(u, v); }
};

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

  inline bool is_nan() const {
    return std::isnan(x) || std::isnan(y) || std::isnan(z);
  }

  inline void set(const double d, const uint8_t i) { (&x)[i] = d; }
};

inline Vec3 operator*(const double d, const Vec3 &v) { return v * d; }

inline bool operator==(const Vec3 &v1, const Vec3 &v2) {
  return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z;
}

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

inline double lerp1d(const double a, const double b, const double r) {
  // a <--- r ---> ? <--- 1-r ---> b
  return (1 - r) * a + r * b;
}

struct TexColor3f {
  float x, y, z;

  TexColor3f() : x(0), y(0), z(0) {}

  TexColor3f(const float f) : x(f), y(f), z(f) {}

  TexColor3f(const float x, const float y, const float z) : x(x), y(y), z(z) {}

  inline TexColor3f operator+(const TexColor3f &t) const {
    return TexColor3f(x + t.x, y + t.y, z + t.z);
  }

  inline TexColor3f operator-(const TexColor3f &t) const {
    return TexColor3f(x - t.x, y - t.y, z - t.z);
  }

  inline TexColor3f operator*(const float f) const {
    return TexColor3f(x * f, y * f, z * f);
  }

  inline TexColor3f operator/(const float f) const {
    return TexColor3f(x / f, y / f, z / f);
  }

  inline TexColor3f operator*(const TexColor3f &t) const {
    return TexColor3f(x * t.x, y * t.y, z * t.z);
  }

  inline TexColor3f operator/(const TexColor3f &t) const {
    return TexColor3f(x / t.x, y / t.y, z / t.z);
  }

  inline float len2() const { return x * x + y * y + z * z; }

  inline float len() const { return std::sqrt(this->len2()); }

  inline float max() const { return fmax(x, fmax(y, z)); }

  inline float min() const { return fmin(x, fmin(y, z)); }

  inline Vec3 to_color() const { return Vec3((double)x, (double)y, (double)z); }

  inline void display() const {
    std::cout << x << ", " << y << ", " << z << std::endl;
  }
};

inline TexColor3f operator*(const float d, const TexColor3f &t) {
  return t * d;
}

struct Mat4 {
  double e[4][4];

  Mat4(const double c) {
    for (uint8_t row = 0; row < 4; row++) {
      for (uint8_t col = 0; col < 4; col++) {
        e[row][col] = c;
      }
    }
  }

  inline Mat4 operator*(const Mat4 &m) const {
    Mat4 mat(0.);
    for (uint8_t row = 0; row < 4; row++) {
      for (uint8_t col = 0; col < 4; col++) {
        mat.e[row][col] = e[row][0] * m.e[0][col] + e[row][1] * m.e[1][col] +
                          e[row][2] * m.e[2][col] + e[row][3] * m.e[3][col];
      }
    }
    return mat;
  }
};

inline Vec3 transform(const Vec3 &v, const Mat4 &m) {
  double x, y, z;
  x = v.x * m.e[0][0] + v.y * m.e[0][1] + v.z * m.e[0][2] + m.e[0][3];
  y = v.x * m.e[1][0] + v.y * m.e[1][1] + v.z * m.e[1][2] + m.e[1][3];
  z = v.x * m.e[2][0] + v.y * m.e[2][1] + v.z * m.e[2][2] + m.e[2][3];
  return Vec3(x, y, z);
}
