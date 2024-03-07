#include <assert.h>
#include <string.h>

#include "runtime.h"
#include "runtime/types.h"
#include "util.h"

static int bind_value_rec(Environment *env, LishpSymbol *sym, LishpForm val,
                          int bind_here) {

  LishpForm *value_ptr = NULL;
  if (map_ref(&env->symbol_values, sizeof(LishpSymbol *), sizeof(LishpForm),
              &sym, (void **)&value_ptr) == 0) {

    // symbol is bound in this environment, so just replace the value
    *value_ptr = val;
    return 1;
  }

  if (env->parent != NULL) {
    int bound_higher = bind_value_rec(env->parent, sym, val, 0);
    if (bound_higher) {
      return 1;
    }
  }

  if (bind_here) {
    map_insert(&env->symbol_values, sizeof(LishpSymbol *), sizeof(LishpForm),
               &sym, &val);
  }

  return bind_here;
}

static int bind_function_rec(Environment *env, LishpSymbol *sym,
                             LishpFunction *fn, int bind_here) {

  LishpFunction **value_ptr;
  if (map_ref(&env->symbol_functions, sizeof(LishpSymbol *),
              sizeof(LishpFunction *), &sym, (void **)&value_ptr) == 0) {

    // symbol is bound in this environment, so just replace the value
    *value_ptr = fn;
    return 1;
  }

  if (env->parent != NULL) {
    int bound_higher = bind_function_rec(env->parent, sym, fn, 0);
    if (bound_higher) {
      return 1;
    }
  }

  if (bind_here) {
    map_insert(&env->symbol_functions, sizeof(LishpSymbol *),
               sizeof(LishpFunction *), &sym, &fn);
  }

  return bind_here;
}

void bind_value(Environment *env, LishpSymbol *sym, LishpForm val) {
  bind_value_rec(env, sym, val, 1);
}

void bind_function(Environment *env, LishpSymbol *sym, LishpFunction *fn) {
  bind_function_rec(env, sym, fn, 1);
}

LishpForm symbol_value(Environment *env, LishpSymbol *sym) {
  if (strcmp(env->package, sym->package) != 0) {
    // TODO: look for the symbol exported in the package
    Package *p = find_package(env->rt, sym->package);
    return symbol_value(p->global, sym);
  }

  LishpForm ret;
  if (map_get(&env->symbol_values, sizeof(LishpSymbol *), sizeof(LishpForm),
              &sym, &ret) == 0) {
    return ret;
  }

  if (env->parent != NULL) {
    return symbol_value(env->parent, sym);
  }

  assert(0 && "Symbol not bound in environment!");
  return ret;
}

LishpFunction *symbol_function(Environment *env, LishpSymbol *sym) {
  if (strcmp(env->package, sym->package) != 0) {
    // TODO: look for the symbol exported in the package
    Package *p = find_package(env->rt, sym->package);
    return symbol_function(p->global, sym);
  }

  LishpFunction *ret = NULL;
  if (map_get(&env->symbol_functions, sizeof(LishpSymbol *),
              sizeof(LishpFunction *), &sym, &ret) == 0) {
    return ret;
  }

  if (env->parent != NULL) {
    return symbol_function(env->parent, sym);
  }

  assert(0 && "Symbol not bound in environment!");
  return ret;
}
