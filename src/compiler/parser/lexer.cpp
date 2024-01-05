#include "parser.hpp"

static bool is_valid_ident_char(char);
static bool is_escaped(std::string &, int);

Lexer::Lexer(char const *filename)
    : fstream_(filename), res_(), current_(0), start_(0) {}

auto Lexer::read_tokens() -> std::vector<Token> {
  if (!fstream_.is_open()) {
    throw 0;
  }

  while (fstream_.good()) {
    Token t = read_token();
    res_.push_back(t);
  }

  return res_;
}

auto Lexer::read_token() -> Token {
  // for multi-char tokens, set the beginning to the current index
  start_ = current_;

  while (!is_at_end()) {
    char prev = advance();
    // push the character onto the current string
    cur_string_.push_back(prev);

    switch (prev) {
    case '(':
      return build_token(LEFT_PAREN);
    case ')':
      return build_token(RIGHT_PAREN);
    case ';':
      return read_comment();
    case ':':
      return read_keyword();
    case '"':
      return read_string();
    default:
      if (std::isspace(prev)) {
        return build_token(WHITESPACE);
      } else if (std::isalpha(prev)) {
        return read_identifier();
      } else if (std::isdigit(prev)) {
        return read_number();
      }
      throw "Unimplemented";
    }
  }

  return {EOF_, std::string()};
}

auto Lexer::read_keyword() -> Token {
  while (!is_at_end() && is_valid_ident_char(peek())) {
    cur_string_.push_back(advance());
  }
  return build_token(KEYWORD);
}

auto Lexer::read_identifier() -> Token {
  while (!is_at_end() && is_valid_ident_char(peek())) {
    cur_string_.push_back(advance());
  }
  return build_token(IDENTIFIER);
}

auto Lexer::read_number() -> Token {
  // read every digit, including a possible period
  while (!is_at_end() && (std::isdigit(peek()) || peek() == '.')) {
    cur_string_.push_back(advance());
  }
  // either we read all the digits we could and this next test will fail, or we
  // hit a period, so we read up until we stop reading digits.
  while (!is_at_end() && std::isdigit(peek())) {
    cur_string_.push_back(advance());
  }
  return build_token(NUMBER);
}

auto Lexer::read_string() -> Token {
  while (!is_at_end()) {
    char prev = advance();
    cur_string_.push_back(prev);

    // cur token length = current_ - start_
    // therefore the last character read is given by
    // cur_string_.at(current_ - start_ - 1)
    if (prev == '"' && !is_escaped(cur_string_, current_ - start_ - 1)) {
      break;
    }
  }
  return build_token(STRING);
}

auto Lexer::read_whitespace() -> Token {
  while (!is_at_end() && std::isspace(peek())) {
    cur_string_.push_back(advance());
  }
  return build_token(WHITESPACE);
}

auto Lexer::read_comment() -> Token {
  while (!is_at_end() && (peek() != '\n')) {
    cur_string_.push_back(advance());
  }
  return build_token(COMMENT);
}

static bool is_valid_ident_char(char c) {
  // FIXME: figure out what other characters are allowed... probably a lot more
  return std::isalnum(c) || c == '_' || c == '-' || c == '*' || c == '<' ||
         c == '>' || c == '?' || c == '!' || c == '+' || c == '.';
}

static bool is_escaped(std::string &str, int loc) {
  // NOTE: this expects strings that have the quotes in them...

  // or should this be true?
  if (loc < 1)
    return false;

  return (str.at(loc - 1) == '\\') && !is_escaped(str, loc - 1);
}
