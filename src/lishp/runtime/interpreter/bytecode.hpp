#ifndef lishp_bytecode_h
#define lishp_bytecode_h

#include <stack>
#include <vector>

#include "../types.hpp"

namespace bytecode {

struct Bytecode {
  // I think for now, the way I will do this is function calls will be specified
  // as such:
  //
  // 1. push all the arguments (which is actually just either a cons or nil?)
  // 2. push the function symbol
  // 3. push a function call
  //
  // Then the function call will pop the symbol, look up the function, pop the
  // arg(s), and then can step into the runtime evaluator?

  enum Type {
    kNop,
    kEval,         // escape hatch?? (to call back into the runtime to use the
                   // eval code that we already have)
    kPush,         // push what? anything?
    kPop,          // ditto
    kCallFunction, // I think this should be calling _any_ function; i.e., user
                   // defined, special form, etc.
    kPushEnv,
    kPopEnv,
    kBindSymbolValue,
  };

  Type type;
  union {
    types::LishpForm form;
  };
};

class Evaluator {
public:
  Evaluator(environment::Environment *current_env)
      : current_env_(current_env) {}

  auto EvaluateBytecode(std::vector<Bytecode> &code) -> void;

private:
  auto EvaluateBytecode(Bytecode &code) -> void;
  auto EvaluateUserDefinedFunction(types::LishpFunction *func,
                                   types::LishpList &args_list) -> void;

  environment::Environment *current_env_;
  std::stack<types::LishpForm> stack_;
};

} // namespace bytecode

#endif
