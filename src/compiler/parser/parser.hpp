#ifndef lishp_parser_h
#define lishp_parser_h

#include <fstream>
#include <string>
#include <vector>

enum TokenType {
  LEFT_PAREN,
  RIGHT_PAREN,
};

struct Token {
  TokenType type;
  std::string lexeme;
};

class Lexer {
public:
  Lexer(char const *filename);

  std::vector<Token> read_tokens();

private:
  Token read_token();

  inline bool is_at_end() {
    return _fstream.peek() == std::char_traits<char>::eof();
  }

  inline char advance() {
    char c = _fstream.get();
    ++_current;
    return c;
  }

  std::ifstream _fstream;
  std::vector<Token> _res;

  int _current;
  int _start;
};

class AST {};

class Parser {
public:
  AST parse_file(char const *filename);
};

#endif
