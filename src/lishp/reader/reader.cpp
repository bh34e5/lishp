#include "reader.hpp"

#include <map>
#include <string>

#include "../runtime/exceptions.hpp"

static auto read_list_rec(Reader *reader, Readtable &table,
                          std::istream &in_stream) -> LispForm {
  char c = in_stream.peek();
  if (c == EOF) {
    throw RuntimeException("Reached EOF while reading list");
  }

  if (c == ')') {
    in_stream.get(); // read close paren
    return LispForm::nil();
  } else if (c == '.') {
    throw RuntimeException("Cannot parse dotted pairs/lists");
  }

  LispForm f = reader->read_form(table, in_stream);
  LispForm rest = read_list_rec(reader, table, in_stream);

  LispCons *cons = new LispCons{
      {LispObjType::ObjCons}, false, std::move(f), std::move(rest)};

  return LispForm{LispFormType::FormObj, {.obj = cons}};
}

static auto read_list(Reader *reader, Readtable &table, std::istream &in_stream)
    -> LispForm {
  in_stream.get(); // read open paren
  return read_list_rec(reader, table, in_stream);
}

// TODO: complete this list of escape characters...
static std::map<char, char> escapes = {{'t', '\t'}, {'n', '\n'}, {'b', '\b'},
                                       {'r', '\r'}, {'"', '\"'}, {'\\', '\\'}};

static auto read_string(Reader *reader, Readtable &table,
                        std::istream &in_stream) -> LispForm {
  std::string cur_string;

  in_stream.get(); // read open "

  char c = in_stream.peek();
  while (c != EOF) {
    c = in_stream.get(); // read the character if not EOF

    if (c == '"') {
      LispString *s =
          new LispString{{LispObjType::ObjString}, std::move(cur_string)};

      return LispForm{LispFormType::FormObj, {.obj = s}};
    }

    if (c == '\\') {
      char next = in_stream.peek();

      using it_t = decltype(escapes)::iterator;
      it_t esc = escapes.find(next);

      if (esc != escapes.end()) {
        in_stream.get(); // read the second character in the escape sequence
        c = esc->second; // then assign the escapee to be the character added to
                         // the string.
      }
    }
    cur_string.push_back(c);
  }
  throw RuntimeException("Reached EOF while reading string");
}

auto lisp_whitespace(char c) -> bool {
  switch (c) {
  case '\t':
  case '\n':
  case '\f':
  case '\r':
  case ' ':
    // FIXME: there may be others
    return true;

  default:
    return false;
  }
}

auto read_whitespace(std::istream &in_stream) -> void {
  while (lisp_whitespace(in_stream.peek())) {
    in_stream.get();
  }
}

auto caseable(const char c) { return std::isalpha(c); }

auto to_case(Readtable::Readcase r_case, const char c) -> char {
  switch (r_case) {
  case Readtable::UPPER:
    return std::toupper(c);
  }
}

auto read_token(Readtable &table, std::istream &in_stream) -> std::string {
  std::string str;

  char c = in_stream.peek();
  while (c != EOF && !lisp_whitespace(c)) {
    c = in_stream.get();
    if (caseable(c)) {
      Readtable::Readcase r_case = table.cur_case();
      c = to_case(r_case, c);
    }

    str.push_back(c);
    c = in_stream.peek();
  }
  return str;
}

enum TokenType { PossibleNum, Num, Symbol };
auto token_type(std::string token) -> TokenType {
  // FIXME: right now, assuming all tokens are symbols.
  return TokenType::Symbol;
}

auto Reader::read_form(Readtable &table, std::istream &in_stream) -> LispForm {
  char c = in_stream.peek();
  while (c != EOF) {
    switch (c) {
    case '(':
      return read_list(this, table, in_stream);
    case '"':
      return read_string(this, table, in_stream);
    default:
      if (lisp_whitespace(c)) {
        read_whitespace(in_stream);
        goto continue_loop;
      }

      std::string token = read_token(table, in_stream);
      switch (token_type(token)) {
      case Symbol: {
        // do stuff
        LispSymbol *symbol = table.intern_symbol(token);
        return {LispFormType::FormObj, {.obj = symbol}};
      } break;
      default:
        throw RuntimeException(
            "Cannot read anything other than symbols at the moment");
      }
    }
  continue_loop:
    c = in_stream.peek();
  }
  throw RuntimeException("Reached EOF while reading form");
}
