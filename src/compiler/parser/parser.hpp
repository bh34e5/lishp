#ifndef lishp_parser_h
#define lishp_parser_h

#include <fstream>
#include <string>
#include <vector>

enum TokenType {
  EOF_, // adding '_' so because EOF wasn't allowed
  LEFT_PAREN,
  RIGHT_PAREN,
  KEYWORD,
  IDENTIFIER,
  NUMBER,
  STRING,
  WHITESPACE,
  COMMENT,
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
  Token read_keyword();
  Token read_identifier();
  Token read_number();
  Token read_string();
  Token read_whitespace();
  Token read_comment();

  inline char peek() { return fstream_.peek(); }

  inline bool is_at_end() { return peek() == std::char_traits<char>::eof(); }

  inline char advance() {
    char c = fstream_.get();
    ++current_;
    return c;
  }

  inline Token build_token(TokenType type) {
    return {type, std::move(cur_string_)};
  }

  std::ifstream fstream_;
  std::vector<Token> res_;

  std::string cur_string_;

  int current_;
  int start_;
};

class AST {};

class Parser {
public:
  AST parse_file(char const *filename);
};

#endif
