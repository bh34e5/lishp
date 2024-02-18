#ifndef inherents_h
#define inherents_h

#include "../types.hpp"

#define DECLARE_FUNCTION(f_name)                                               \
  auto f_name(environment::Environment *closure,                               \
              environment::Environment *lexical, types::LishpList &args)       \
      ->types::LishpFunctionReturn

#define DECLARE_SPECIAL_FORM(sf_name)                                          \
  auto sf_name(environment::Environment *lexical, types::LishpList &args)      \
      ->types::LishpFunctionReturn

namespace inherents {

namespace system {

DECLARE_FUNCTION(Repl);
DECLARE_FUNCTION(ReadOpenParen);
DECLARE_FUNCTION(ReadCloseParen);
DECLARE_FUNCTION(ReadDoubleQuote);
DECLARE_FUNCTION(ReadSingleQuote);

} // namespace system

namespace user {

DECLARE_FUNCTION(Read);
DECLARE_FUNCTION(ReadChar);
DECLARE_FUNCTION(Format);
DECLARE_FUNCTION(Plus);
DECLARE_FUNCTION(Star);

} // namespace user

namespace special_forms {

DECLARE_SPECIAL_FORM(Tagbody);
DECLARE_SPECIAL_FORM(Go);
DECLARE_SPECIAL_FORM(Quote);
DECLARE_SPECIAL_FORM(Function);
DECLARE_SPECIAL_FORM(Progn);
DECLARE_SPECIAL_FORM(Labels);

} // namespace special_forms

} // namespace inherents

#endif
