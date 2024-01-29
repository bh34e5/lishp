#include <iostream>

#include "lishp/lishp.hpp"

void interpret();
void compile_file(char const *filename);
void usage();

int main(int argc, char const *argv[]) {
  switch (argc) {
  case 1: {
    interpret();
  } break;
  case 2: {
    compile_file(argv[1]);
  } break;
  default: {
    usage();
    return 1;
  }
  }
  return 0;
}

void interpret() {
  LishpRuntime rt;
  // run the REPL until quit by the user.
  rt.repl();
}

void compile_file(char const *filename) {}

void usage() { std::cerr << "Usage: ./lishp [filename]" << std::endl; }
