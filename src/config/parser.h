#pragma once

#include "lexer.h"
#include <queue>
#include <unordered_map>
#include <variant>
#include <vector>

struct ObjectNode {
  // Arrays must include double values
  std::unordered_map<std::string, std::variant<double, bool, std::string,
                                               std::vector<double>, ObjectNode>>
      data;

  ObjectNode() {}
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
