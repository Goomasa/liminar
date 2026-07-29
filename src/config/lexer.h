#pragma once

#include <optional>
#include <queue>
#include <string>

enum TokenType {
  String,
  Double,
  True,
  False,
  Comma,
  LBrace,
  RBrace,
  LBracket,
  RBracket,
  Minus,
};

struct Token {
  TokenType type;
  std::string str;

  Token(const TokenType type) : type(type) {}

  Token(const TokenType type, const std::string &s) : type(type), str(s) {}
};

std::queue<Token> tokenize(std::string &code);

void test_tokenize(std::string &code);

template <typename T> inline std::optional<T> pop_front(std::queue<T> &q) {
  if (q.empty()) {
    return std::nullopt;
  } else {
    T t = q.front();
    q.pop();
    return t;
  }
}
