#ifndef lishp_lishp_h
#define lishp_lishp_h

#include <map>
#include <vector>

#include "reader/reader.hpp"
#include "reader/readtable.hpp"
#include "types/types.hpp"

class LishpRuntime {
public:
  auto repl() -> void;
  auto lookup_function(LispSymbol *sym) -> LispFunction;
  auto lookup_symbol(LispSymbol *sym) -> LispForm;
  auto get_reader() -> Reader &;
  auto cur_readtable() -> Readtable &;

private:
  auto run_program(std::vector<LispForm> forms) -> void;
  auto initialize_primitives() -> void;
  auto bind(LispSymbol *sym, LispForm val) -> void;

  template <typename T, LispObjType objType> auto make_obj() -> T *;

  inline auto define_primitive_function(LispSymbol *name, LispCons *argsDecl,
                                        PrimitiveFunction &&fn) -> void {
    LispFunction functionObj{
        {LispObjType::ObjFunction}, true, argsDecl, {.inherent = fn}};
    function_assocs_.insert({name, functionObj});
  }

  Reader reader_;

  Readtable table_;

  std::map<LispSymbol *, LispForm> symbol_assocs_;
  std::map<LispSymbol *, LispFunction> function_assocs_;
};

#endif
