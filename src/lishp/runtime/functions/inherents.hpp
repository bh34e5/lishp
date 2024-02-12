#ifndef inherents_h
#define inherents_h

#include "../types.hpp"

#define DECLARE_FUNCTION(f_name)                                               \
  auto f_name(environment::Environment *env, types::LishpList &args)           \
      ->types::LishpFunctionReturn

namespace inherents {

namespace system {

DECLARE_FUNCTION(Repl);
DECLARE_FUNCTION(ReadOpenParen);
DECLARE_FUNCTION(ReadCloseParen);
DECLARE_FUNCTION(ReadDoubleQuote);

} // namespace system

namespace user {

DECLARE_FUNCTION(Read);
DECLARE_FUNCTION(ReadChar);
DECLARE_FUNCTION(Format);

} // namespace user

} // namespace inherents

#endif
