#include <exception>
#include <string>

#include "reader.hpp"

namespace reader {

class Token {
public:
  enum Type {
    kNumber,
    kSymbol,
    kInvalid,
  };

  Token(types::LishpReadtable *rt) : rt_(rt) {}

  inline auto AddCharacter(char c) {
    if (rt_->CasedChar(c)) {
      buf_.push_back(rt_->ConvertCase(c));
      return;
    }
    buf_.push_back(c);
  }
  inline auto GetBuffer() { return std::move(buf_); }

  inline auto GetType(bool had_escape) {
    if (had_escape) {
      // TODO: confirm this... it looks like it from about 30 seconds of
      // experimentation
      return kSymbol;
    }
    // FIXME: right now only reading symbols
    return kSymbol;
  }

private:
  types::LishpReadtable *rt_;
  std::string buf_;
};

enum CharTraits {
  kConstituent,
  kInvalid,
  kTerminatingMacroCharacter,
  kNonTerminatingMacroCharacter,
  kMultipleEscape,
  kSingleEscape,
  kWhitespace,
};

auto CheckNonTerminatingMacroChar(char c) { return c == '#'; }

auto CheckMacroCharacter(types::LishpReadtable *rt, char c, CharTraits *type) {
  if (CheckNonTerminatingMacroChar(c)) {
    *type = kNonTerminatingMacroCharacter;
    return true;
  }

  using it_t = decltype(rt->reader_macro_functions)::iterator;

  it_t macro_chars = rt->reader_macro_functions.find(c);
  if (macro_chars == rt->reader_macro_functions.end()) {
    return false;
  }
  *type = kTerminatingMacroCharacter;
  return true;
}

auto GetCharTraits(types::LishpReadtable *rt, char c) {
  CharTraits type;
  if (CheckMacroCharacter(rt, c, &type)) {
    return type;
  }

  switch (c) {
  case '\t':
  case '\n':
  case '\f':
  case '\r':
  case ' ': {
    // TODO: figure out difference between "page" and "linefeed"
    type = kWhitespace;
  } break;
  case '\'': {
    type = kSingleEscape;
  } break;
  case '|': {
    type = kMultipleEscape;
  } break;
  case '\b':
  case '!':
  case '$':
  case '%':
  case '&':
  case '*':
  case '+':
  case '-':
  case '.':
  case '/':
  case ':':
  case '<':
  case '=':
  case '>':
  case '?':
  case '@':
  case '[':
  case ']':
  case '^':
  case '_':
  case '{':
  case '}':
  case '~': {
    type = kConstituent;
  } break;
  default: {
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z')) {
      type = kConstituent;
    } else {
      type = CharTraits::kInvalid;
    }
  } break;
  }

  return type;
}

auto Reader::ReadForm() -> types::LishpForm {
  types::LishpReadtable *rt = GetReadtable();

  Token cur_token(rt);
  bool had_escape = false;

step_1:
  char x = stm_.get();

  if (stm_.eof()) {
    // TODO: handle EOF
    throw std::runtime_error("Reached EOF while reading form");
  }

  switch (GetCharTraits(rt, x)) {
  case kInvalid:
    throw std::runtime_error("Error Type: reader-error");
  case kWhitespace:
    goto step_1;
  case kTerminatingMacroCharacter:
  case kNonTerminatingMacroCharacter: {
    // TODO: lookup macro function and call it

    memory::MemoryManager *manager = closure_->package()->manager();

    types::LishpIStream *stream_obj =
        manager->Allocate<types::LishpIStream>(stm_);

    types::LishpList args = types::LishpList::Build(
        manager, stream_obj, types::LishpForm::FromChar(x));

    types::LishpFunction *macro_function = rt->reader_macro_functions.at(x);
    types::LishpFunctionReturn ret = macro_function->Call(lexical_, args);

    if (ret.values.size() == 0) {
      goto step_1;
    } else if (ret.values.size() == 1) {
      return ret.values.at(0);
    } else {
      throw std::runtime_error(
          "Someone did an oopsie and returned too many values");
    }
  }
  case kSingleEscape: {
    had_escape = true;
    goto step_8;
  }
  case kMultipleEscape: {
    had_escape = true;
    goto step_9;
  }
  case kConstituent: {
    cur_token.AddCharacter(x);

  step_8 : {
    char y = stm_.get();
    if (stm_.eof()) {
      goto step_10;
    }

    switch (GetCharTraits(rt, y)) {
    case kConstituent:
    case kNonTerminatingMacroCharacter:
      cur_token.AddCharacter(y);
      goto step_8;
    case kSingleEscape: {
      char z = stm_.get();
      if (stm_.eof()) {
        throw std::runtime_error("Error Type: end-of-file");
      }
      cur_token.AddCharacter(z);
      had_escape = true;
      goto step_8;
    }
    case kMultipleEscape:
      had_escape = true;
      goto step_9;
    case kInvalid:
      throw std::runtime_error("Error Type: reader-error");
    case kTerminatingMacroCharacter:
      stm_.unget();
      goto step_10;
    case kWhitespace:
      // TODO: read about `read-preserving-whitespace`
      if (true) {
        stm_.unget();
      }
      goto step_10;
    }
  }

  step_9 : {
    char y = stm_.get();
    if (stm_.eof()) {
      throw std::runtime_error("Error Type: end-of-file");
    }

    switch (GetCharTraits(rt, y)) {
    case kConstituent:
    case kTerminatingMacroCharacter:
    case kNonTerminatingMacroCharacter:
    case kWhitespace:
      cur_token.AddCharacter(y);
      goto step_9;
    case kSingleEscape: {
      char z = stm_.get();
      if (stm_.eof()) {
        throw std::runtime_error("Error Type: end-of-file");
      }
      cur_token.AddCharacter(z);
      had_escape = true;
      goto step_9;
    }
    case kMultipleEscape:
      had_escape = true;
      goto step_8;
    case kInvalid:
      throw std::runtime_error("Error Type: reader-error");
    }
  }
  }
  }

  // if invalid syntax, throw an error.

step_10:
  switch (cur_token.GetType(had_escape)) {
  case Token::kSymbol: {
    // TODO: It should probably intern into the package into which it is being
    // read? Actually, this reminds me that I am not handling the package
    // qualified symbols either... tbf, I'm not handling really anything
    // correctly yet :P
    runtime::Package *package = lexical_->package();
    types::LishpSymbol *new_symbol =
        package->InternSymbol(cur_token.GetBuffer());

    return types::LishpForm::FromObj(new_symbol);
  }
  default:
    assert(0 && "Unimplemented: reader::Reader::ReadForm");
  }

  return types::LishpForm::Nil();
}

} // namespace reader
