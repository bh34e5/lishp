#include <assert.h>
#include <stdio.h>

#include "lishp.h"
#include "runtime.h"

void repl() {
  Runtime rt;
  initialize_runtime(&rt);

  rt.repl(&rt);

  cleanup_runtime(&rt);
}

void compile_file(const char *filename) {
  printf("Wanting to do something with %s\n", filename);
  assert(0 && "Unimplemented!");
}
