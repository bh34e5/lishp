#ifndef lishp_compiler_h
#define lishp_compiler_h

class CompileResult {};

class Compiler {
public:
  CompileResult compile(char const *filename);
};

auto Compiler::compile(const char *filename) -> CompileResult {
  throw "Unimplemented";
}

#endif
