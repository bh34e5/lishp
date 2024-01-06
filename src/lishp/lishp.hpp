#ifndef lishp_lishp_h
#define lishp_lishp_h

#include <map>

#include "types.hpp"

class LishpRuntime {
public:
  auto repl() -> void;

private:
  auto initialize_primitives() -> void;

  template <typename T, LispObjType objType> auto make_obj() -> T *;

  inline auto define_primitive_function(LispSymbol *name, LispCons *argsDecl,
                                        PrimitiveFunction &&fn) -> void {
    LispFunction functionObj{
        {LispObjType::ObjFunction}, true, argsDecl, {.inherent = fn}};
    function_assocs_.insert({name, functionObj});
  }

  auto intern_symbol(std::string &lexeme) -> LispSymbol *;

  // TODO: eventually this will get pulled out to a namespace class
  std::map<LispSymbol *, LispObj> symbol_assocs_;
  std::map<LispSymbol *, LispFunction> function_assocs_;

  std::map<std::string, LispSymbol *> interned_symbols_;
};

#endif
