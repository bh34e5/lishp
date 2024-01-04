#ifndef lishp_interpreter_h
#define lishp_interpreter_h

#include "../compiler/compiler.hpp"

class Interpreter {
public:
  void run_loop();
  void interpret(CompileResult comp);
};

auto Interpreter::run_loop() -> void { throw "Unimplemented"; }

auto Interpreter::interpret(CompileResult comp) -> void {
  throw "Unimplemented";
}

#endif
