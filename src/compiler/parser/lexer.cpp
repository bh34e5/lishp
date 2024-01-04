#include "parser.hpp"

Lexer::Lexer(char const *filename)
    : _fstream(filename), _res(), _current(0), _start(0) {}

auto Lexer::read_tokens() -> std::vector<Token> {
  if (!_fstream.is_open()) {
    throw 0;
  }

  while (_fstream.good() && !is_at_end()) {
    Token t = read_token();
    _res.push_back(t);
  }

  return _res;
}

auto Lexer::read_token() -> Token {
  // for multi-char tokens, set the beginning to the current index
  _start = _current;

  char prev = advance();

  switch (prev) {
  case '(':
    return {LEFT_PAREN, std::string()};
  case ')':
    return {RIGHT_PAREN, std::string()};
  default:
    throw "Unimplemented";
  }
}
