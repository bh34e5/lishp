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

static int symbol_value_int(Runtime *rt, Environment *env, LishpSymbol *sym,
                            LishpForm *result) {
  if (strcmp(env->package, sym->package) != 0) {
    // TODO: look for the symbol exported in the package
    Package *p = find_package(rt, sym->package);
    *result = symbol_value(rt, p->global, sym);
    return 1;
  }

  LishpForm ret;
  if (map_get(&env->symbol_values, sizeof(LishpSymbol *), sizeof(LishpForm),
              &sym, &ret) == 0) {
    *result = ret;
    return 1;
  }

  if (env->parent != NULL) {
    *result = symbol_value(rt, env->parent, sym);
    return 1;
  }

  return 0;
}

static int symbol_function_int(Runtime *rt, Environment *env, LishpSymbol *sym,
                               LishpFunction **result) {
  if (strcmp(env->package, sym->package) != 0) {
    // TODO: look for the symbol exported in the package
    Package *p = find_package(rt, sym->package);
    *result = symbol_function(rt, p->global, sym);
    return 1;
  }

  LishpFunction *ret = NULL;
  if (map_get(&env->symbol_functions, sizeof(LishpSymbol *),
              sizeof(LishpFunction *), &sym, &ret) == 0) {
    *result = ret;
    return 1;
  }

  if (env->parent != NULL) {
    *result = symbol_function(rt, env->parent, sym);
    return 1;
  }

  return 0;
}

LishpForm symbol_value(Runtime *rt, Environment *env, LishpSymbol *sym) {
  LishpForm result;

  int found = symbol_value_int(rt, env, sym, &result);

  if (!found) {
    assert(0 && "Symbol not bound in current environment!");
  }

  return result;
}

LishpFunction *symbol_function(Runtime *rt, Environment *env,
                               LishpSymbol *sym) {
  LishpFunction *result;

  int found = symbol_function_int(rt, env, sym, &result);

  if (!found) {
    assert(0 && "Symbol not bound in current environment!");
  }

  return result;
}
