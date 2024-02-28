#ifndef lishp_analyzer_h
#define lishp_analyzer_h

#include <vector>

#include "../types.hpp"
#include "bytecode.hpp"

namespace bytecode {

// NOTE: From
// https://www.lispworks.com/documentation/HyperSpec/Body/03_abab.htm
//
// If the car of that compound form is a symbol, that symbol is the name of an
// operator, and the form is either a special form, a macro form, or a
// function form, depending on the function binding of the operator in the
// current lexical environment. If the operator is neither a special operator
// nor a macro name, it is assumed to be a function name (even if there is no
// definition for such a function).
//
// So it seems like it's legal to always evaluate `(if condition then else)` as
// a special form, even if the `if` symbol has a function binding in the local
// scope?

class Analyzer {
public:
  Analyzer() : current_env_(nullptr) {}
  Analyzer(environment::Environment *current_env) : current_env_(current_env) {}

  auto TransformForm(types::LishpForm &form) -> std::vector<Bytecode>;

private:
  auto TransformObject(types::LishpObject *obj) -> std::vector<Bytecode>;
  auto TransformCons(types::LishpCons *cons) -> std::vector<Bytecode>;

  auto AnalyzeSpecialForm(types::LishpSymbol *sym, types::LishpForm args)
      -> std::vector<Bytecode>;

  auto AnalyzeTagbody(types::LishpForm args) -> std::vector<Bytecode>;
  auto AnalyzeGo(types::LishpForm args) -> std::vector<Bytecode>;
  auto AnalyzeQuote(types::LishpForm args) -> std::vector<Bytecode>;
  auto AnalyzeFunction(types::LishpForm args) -> std::vector<Bytecode>;
  auto AnalyzeProgn(types::LishpForm args) -> std::vector<Bytecode>;
  auto AnalyzeLabels(types::LishpForm args) -> std::vector<Bytecode>;
  auto AnalyzeLetStar(types::LishpForm args) -> std::vector<Bytecode>;

  environment::Environment *current_env_;
};

} // namespace bytecode

#endif
