#ifndef lishp_reader_readtable_h
#define lishp_reader_readtable_h

#include <map>
#include <string>

#include "../types/types.hpp"

// TODO: is this where the symbols should be stored? Maybe add an instance
// method, `read_symbol` or something like that?
class Readtable {
public:
  enum Readcase {
    UPPER,
  };

  auto cur_case() const -> Readcase { return case_; }

  auto intern_symbol(std::string &lexeme) -> LispSymbol *;
  auto intern_symbol(std::string &&lexeme) -> LispSymbol *;

private:
  Readcase case_ = Readcase::UPPER;

  std::map<std::string, LispSymbol *> interned_symbols_;
};

#endif
