#pragma once

#include "lexer.h"
#include "util/utils.h"
#include <fstream>
#include <queue>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct ObjectNode {
  // Arrays must include double values
  std::unordered_map<std::string, std::variant<double, bool, std::string,
                                               std::vector<double>, ObjectNode>>
      data;

  ObjectNode() {}

  template <typename T> T search_key(const std::string &key) {
    auto d = data[key];
    return get_data<T>(d);
  }

  template <typename T>
  T search_key_safe(const std::string &key, const T &default_) {
    auto d = data[key];
    return get_data_safe<T>(d, default_);
  }
};

typedef std::variant<double, bool, std::string, std::vector<double>, ObjectNode>
    Data;

ObjectNode parse_object(std::queue<Token> &tokens);

void test_parse(std::string code);

template <typename T> inline T get_data(const Data &data) {
  if (std::holds_alternative<T>(data)) {
    return std::get<T>(data);
  } else {
    return exit_with_error<T>("failed to get data");
  }
}

template <typename T>
inline T get_data_safe(const Data &data, const T &default_) {
  if (std::holds_alternative<T>(data)) {
    return std::get<T>(data);
  } else {
    return default_;
  }
}

inline ObjectNode load_json(const std::string &filename) {
  std::ifstream ifs(filename, std::ios::in);
  if (!ifs) {
    exit_with_error<void>("failed to open ", filename);
  }

  std::string tmp, code;
  while (std::getline(ifs, tmp)) {
    code += tmp;
  }

  std::queue<Token> tokens = tokenize(code);
  return parse_object(tokens);
}

inline Vec3 vector_to_vec3(const std::vector<double> v) {
  if (v.size() != 3) {
    return Vec3(1.);
  } else {
    return Vec3(v[0], v[1], v[2]);
  }
}
