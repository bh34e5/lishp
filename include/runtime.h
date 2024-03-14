#ifndef runtime_
#define runtime_

#include "runtime/memory_manager.h"
#include "runtime/types.h"
#include "util.h"

typedef struct interpreter Interpreter;

typedef struct environment {
  struct environment *parent;
  const char *package;

  OrderedMap symbol_values;
  OrderedMap symbol_functions;
} Environment;

typedef struct {
  const char *name;
  Environment *global;
  LishpReadtable *current_readtable;

  OrderedMap interned_symbols;
  List exported_symbols;
} Package;

typedef struct runtime {
  void (*repl)(struct runtime *);

  MemoryManager *memory_manager;
  LishpReadtable *system_readtable;
  List packages;
  Interpreter *interpreter;
} Runtime;

int initialize_runtime(Runtime *rt);
int cleanup_runtime(Runtime *rt);
Package *find_package(Runtime *rt, const char *name);

#define ALLOCATE_OBJ(t, rt) ((t *)_allocate_obj(rt, sizeof(t)))
#define DEALLOCATE_OBJ(t, o, rt) (_deallocate_obj(rt, (o), sizeof(t)))
void *_allocate_obj(Runtime *rt, uint32_t size);
void _deallocate_obj(Runtime *rt, void *ptr, uint32_t size);

Environment *allocate_env(Runtime *rt, Environment *parent);

LishpSymbol *intern_symbol(Runtime *rt, Package *p, const char *lexeme);
LishpSymbol *gensym(Runtime *rt, Package *p, const char *lexeme);

void bind_value(Environment *env, LishpSymbol *sym, LishpForm val);
void bind_function(Environment *env, LishpSymbol *sym, LishpFunction *fn);
LishpForm symbol_value(Runtime *rt, Environment *env, LishpSymbol *sym);
LishpFunction *symbol_function(Runtime *rt, Environment *env, LishpSymbol *sym);

#endif
