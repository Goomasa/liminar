#pragma once

#include <cstdint>

struct Random {
  uint32_t x;

  Random(const uint32_t seed) { x = seed ^ 123456789; }

  double gen() {
    uint32_t x_ = this->x;
    x_ = x_ ^ (x_ << 13);
    x_ = x_ ^ (x_ >> 7);
    x_ = x_ ^ (x_ << 17);
    this->x = x_;
    return (double)x_ / (double)UINT32_MAX;
  }

  uint32_t gen_uint() {
    uint32_t x_ = this->x;
    x_ = x_ ^ (x_ << 13);
    x_ = x_ ^ (x_ >> 7);
    x_ = x_ ^ (x_ << 17);
    this->x = x_;
    return x_;
  }
};
