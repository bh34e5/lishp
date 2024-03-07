#include <stdio.h>

#include "lishp.h"

static void usage();

int main(int argc, const char *argv[]) {
  switch (argc) {
  case 1: {
    repl();
  } break;
  case 2: {
    compile_file(argv[1]);
  } break;
  default: {
    usage();
    return 1;
  } break;
  }
  return 0;
}

static void usage() { fprintf(stderr, "Usage: ./lishp [filename]\n"); }
