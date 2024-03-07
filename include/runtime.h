#ifndef runtime_
#define runtime_

#include "runtime/types.h"
#include "util.h"

typedef struct runtime Runtime;
typedef struct interpreter Interpreter;

typedef struct environment {
  Runtime *rt;
  struct environment *parent;
  const char *package;

  OrderedMap symbol_values;
  OrderedMap symbol_functions;
} Environment;

typedef struct {
  Runtime *rt;
  const char *name;
  Environment *global;
  OrderedMap interned_symbols;
  LishpReadtable *current_readtable;
} Package;

struct runtime {
  void (*repl)(struct runtime *);

  LishpReadtable *system_readtable;
  List packages;
  Interpreter *interpreter;
};

int initialize_runtime(Runtime *rt);
Package *find_package(Runtime *rt, const char *name);

#define ALLOCATE_OBJ(t, rt) ((t *)_allocate_obj(rt, sizeof(t)))
void *_allocate_obj(Runtime *rt, uint32_t size);

LishpSymbol *intern_symbol(Package *p, const char *lexeme);

void bind_value(Environment *env, LishpSymbol *sym, LishpForm val);
void bind_function(Environment *env, LishpSymbol *sym, LishpFunction *fn);
LishpForm symbol_value(Environment *env, LishpSymbol *sym);
LishpFunction *symbol_function(Environment *env, LishpSymbol *sym);

#endif
