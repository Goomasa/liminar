#pragma once

#include <iostream>
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
  Colon
};

struct Token {
  TokenType type;
  std::string str;

  Token(const TokenType type) : type(type) {}

  Token(const TokenType type, const std::string &s) : type(type), str(s) {}
};

std::queue<Token> tokenize(std::string &code);

void test_tokenize(std::string &code);

template <typename T>
inline T exit_with_error(const std::string &err, const std::string &info = "") {
  std::cout << err << info << std::endl;
  exit(1);
}

template <typename T> inline T pop_front(std::queue<T> &q) {
  if (q.empty()) {
    return exit_with_error<T>("Queue is empty");
  } else {
    T t = q.front();
    q.pop();
    return t;
  }
}

inline void consume(std::queue<Token> &tokens, const TokenType type) {
  if (tokens.front().type == type) {
    tokens.pop();
    return;
  } else {
    exit_with_error<void>("no expected token");
  }
}

inline std::string consume_str(std::queue<Token> &tokens) {
  if (tokens.front().type == TokenType::String) {
    Token token = pop_front<Token>(tokens);
    return token.str;
  } else {
    return exit_with_error<std::string>("no expected token");
  }
}
