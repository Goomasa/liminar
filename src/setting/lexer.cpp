#include "lexer.h"
#include <cstdlib>
#include <iostream>

inline std::string get_string(std::string &code) {
  std::string s = "";
  while (!code.empty()) {
    if (code[0] == '"') {
      code = code.substr(1);
      break;
    } else {
      s += code[0];
      code = code.substr(1);
    }
  }

  return s;
}

inline bool is_digit(const char c) { return c >= 48 && c <= 57; }

inline std::string get_double(std::string &code) {
  std::string s = "";
  while (!code.empty()) {
    if (is_digit(code[0]) || code[0] == '.') {
      s += code[0];
      code = code.substr(1);
    } else {
      break;
    }
  }

  return s;
}

std::queue<Token> tokenize(std::string &code) {
  std::queue<Token> tokens;

  while (!code.empty()) {
    switch (code[0]) {
    case ' ': {
      code = code.substr(1);
    } break;
    case '{': {
      tokens.push(Token(TokenType::LBrace, "{"));
      code = code.substr(1);
    } break;
    case '}': {
      tokens.push(Token(TokenType::RBrace, "}"));
      code = code.substr(1);
    } break;
    case '-': {
      tokens.push(Token(TokenType::Minus, "-"));
      code = code.substr(1);
    } break;
    case ',': {
      tokens.push(Token(TokenType::Comma, ","));
      code = code.substr(1);
    } break;
    case '[': {
      tokens.push(Token(TokenType::LBracket, "["));
      code = code.substr(1);
    } break;
    case ']': {
      tokens.push(Token(TokenType::RBracket, "]"));
      code = code.substr(1);
    } break;
    case ':': {
      tokens.push(Token(TokenType::Colon, ":"));
      code = code.substr(1);
    } break;
    case '"': {
      code = code.substr(1);
      Token token = Token(TokenType::String, get_string(code));
      tokens.push(token);
    } break;
    default: {
      if (code.substr(0, 4) == "true") {
        tokens.push(Token(TokenType::True, "true"));
        code = code.substr(4);
      } else if (code.substr(0, 5) == "false") {
        tokens.push(Token(TokenType::False, "false"));
        code = code.substr(5);
      } else if (is_digit(code[0])) {
        std::string d = get_double(code);
        tokens.push(Token(TokenType::Double, d));
      } else {
        exit_with_error<void>("failed to tokenize from ", code);
      }
    } break;
    }
  }

  return tokens;
}

void test_tokenize(std::string &code) {
  std::queue<Token> tokens = tokenize(code);
  while (!tokens.empty()) {
    Token token = pop_front(tokens);
    std::cout << token.str << std::endl;
  }
}
