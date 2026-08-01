#include "parser.h"
#include "config/lexer.h"
#include <exception>
#include <queue>
#include <string>
#include <vector>

inline double str_to_double(const std::string &str) {
  try {
    return std::stod(str);
  } catch (std::exception e) {
    return exit_with_error<double>(e.what());
  }
}

Data parse_primary(std::queue<Token> &tokens) {
  Token token = pop_front(tokens);
  switch (token.type) {
  case TokenType::True: {
    return true;
  } break;
  case TokenType::False: {
    return false;
  } break;
  case TokenType::Double: {
    return str_to_double(token.str);
  } break;
  case TokenType::String: {
    return token.str;
  } break;
  default: {
    return exit_with_error<Data>("syntax error in primary: ", token.str);
  } break;
  }
}

Data parse_unary(std::queue<Token> &tokens) {
  switch (tokens.front().type) {
  case TokenType::Minus: {
    tokens.pop();
    Data data = parse_primary(tokens);
    return -1. * get_data<double>(data);
  } break;
  default: {
    return parse_primary(tokens);
  } break;
  }
}

Data parse_array(std::queue<Token> &tokens) {
  consume(tokens, TokenType::LBracket);

  std::vector<double> array;
  while (true) {
    array.emplace_back(get_data<double>(parse_unary(tokens)));
    if (tokens.front().type == TokenType::Comma) {
      tokens.pop();
      continue;
    } else {
      break;
    }
  }

  consume(tokens, TokenType::RBracket);
  return array;
}

Data parse_item(std::queue<Token> &tokens) {
  switch (tokens.front().type) {
  case TokenType::LBrace: {
    return parse_object(tokens);
  } break;
  case TokenType::LBracket: {
    return parse_array(tokens);
  } break;
  default: {
    return parse_unary(tokens);
  } break;
  }
}

ObjectNode parse_object(std::queue<Token> &tokens) {
  ObjectNode node;
  consume(tokens, TokenType::LBrace);
  while (true) {
    std::string id = consume_str(tokens);
    consume(tokens, TokenType::Colon);
    Data data = parse_item(tokens);
    node.data[id] = data;

    if (tokens.front().type == TokenType::Comma) {
      tokens.pop();
      continue;
    } else {
      break;
    }
  }

  consume(tokens, TokenType::RBrace);
  return node;
}

void test_parse(std::string code) {
  std::queue<Token> tokens = tokenize(code);
  ObjectNode node = parse_object(tokens);

  for (auto itr = node.data.begin(); itr != node.data.end(); itr++) {
    std::cout << itr->first << " : " << get_data_safe<double>(itr->second, 0)
              << std::endl;
  }
}
