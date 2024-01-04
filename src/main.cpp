#include <iostream>

#include "common.hpp"

void interpret();
void compile_file(char const *filename);
void usage();

int main(int argc, char const *argv[]) {
  switch (argc) {
  case 1:
    interpret();
    break;
  case 2:
    compile_file(argv[1]);
    break;
  default:
    usage();
    return 1;
  }
  return 0;
}

void interpret() {
  Interpreter interpreter;

  interpreter.run_loop();
}

void compile_file(char const *filename) {
  Compiler compiler;

  // TODO: test the file exists, otherwise call usage here...? Or maybe check
  // file comes out of this function and into the switch
  CompileResult res = compiler.compile(filename);

  Interpreter interpreter;
  interpreter.interpret(res);
}

void usage() { std::cerr << "Usage: ./lishp [filename]" << std::endl; }
