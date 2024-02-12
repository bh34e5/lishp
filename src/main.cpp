#include <iostream>

#include "lishp/lishp.hpp"

auto Interpret() -> void;
auto CompileFile(char const *filename) -> void;
auto Usage() -> void;

int main(int argc, char const *argv[]) {
  switch (argc) {
  case 1: {
    Interpret();
  } break;
  case 2: {
    CompileFile(argv[1]);
  } break;
  default: {
    Usage();
    return 1;
  }
  }
  return 0;
}

auto Interpret() -> void {
  // run the REPL until quit by the user.
  lishp::RuntimeRepl();
}

auto CompileFile(char const *filename) -> void {
  std::string wrapped_filename(filename);
  lishp::CompileFile(wrapped_filename);
}

auto Usage() -> void { std::cerr << "Usage: ./lishp [filename]" << std::endl; }
