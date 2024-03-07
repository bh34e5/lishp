#include <stdint.h>
#include <string.h>

#include "common.h"
#include "memory_manager.h"
#include "runtime.h"
#include "runtime/functions.h"
#include "runtime/interpreter.h"
#include "runtime/types.h"
#include "util.h"

static int symbol_cmp(void *l, void *r) {
  LishpSymbol **l_sym = l;
  LishpSymbol **r_sym = r;

  return *l_sym < *r_sym ? -1 : *l_sym > *r_sym ? 1 : 0;
}

static int initialize_environment(Environment *env, Package *p) {
  env->rt = p->rt;
  env->parent = NULL;
  env->package = p->name;

  map_init(&env->symbol_values, symbol_cmp);
  map_init(&env->symbol_functions, symbol_cmp);

  return 0;
}

static int cleanup_environment(Environment *env) {
  map_clear(&env->symbol_values);
  map_clear(&env->symbol_functions);

  return 0;
}

static int name_cmp(void *l, void *r) {
  const char **l_strp = l;
  const char **r_strp = r;

  int cmp = strcmp(*l_strp, *r_strp);

  return cmp;
}

static int initialize_package(Package *p, Runtime *rt, const char *name) {
  p->rt = rt;
  p->name = name;
  p->global = allocate(sizeof(Environment));

  if (p->global == NULL) {
    return -1;
  }

  TEST_CALL(initialize_environment(p->global, p));
  TEST_CALL(map_init(&p->interned_symbols, name_cmp));

  p->current_readtable = rt->system_readtable;

  return 0;
}

static int cleanup_package(Package *p) {
  map_clear(&p->interned_symbols);
  return 0;
}

static int initialize_packages(Runtime *rt) {
#define INSTALL_INHERENT(name, package, lexeme)                                \
  do {                                                                         \
    LishpFunction *name##_fn = allocate(sizeof(LishpFunction));                \
    if (name##_fn == NULL) {                                                   \
      return -1;                                                               \
    }                                                                          \
    *name##_fn = FUNCTION_INHERENT(name);                                      \
                                                                               \
    LishpSymbol *name##_sym = intern_symbol(&package, lexeme);                 \
    bind_function(package.global, name##_sym, name##_fn);                      \
  } while (0)

  Package system;
  Package common_lisp;
  Package user;

  TEST_CALL(initialize_package(&system, rt, "SYSTEM"));
  TEST_CALL(initialize_package(&common_lisp, rt, "COMMON-LISP"));
  TEST_CALL(initialize_package(&user, rt, "USER"));

  INSTALL_INHERENT(system_repl, system, "REPL");
  INSTALL_INHERENT(system_read_open_paren, system, "READ-OPEN-PAREN");
  INSTALL_INHERENT(system_read_close_paren, system, "READ-CLOSE-PAREN");
  INSTALL_INHERENT(system_read_double_quote, system, "READ-DOUBLE-QUOTE");
  INSTALL_INHERENT(system_read_single_quote, system, "READ-SINGLE-QUOTE");

  INSTALL_INHERENT(common_lisp_read, common_lisp, "READ");
  INSTALL_INHERENT(common_lisp_format, common_lisp, "FORMAT");

  TEST_CALL(list_push(&rt->packages, sizeof(Package), &system));
  TEST_CALL(list_push(&rt->packages, sizeof(Package), &common_lisp));
  TEST_CALL(list_push(&rt->packages, sizeof(Package), &user));

  return 0;
}

static int names_equal(void *arg, void *item) {
  const char *name = arg;
  Package *p = item;

  if (strcmp(name, p->name) == 0) {
    return 1;
  }

  return 0;
}

Package *find_package(Runtime *rt, const char *name) {
  Package *ptr = NULL;

  list_find(&rt->packages, sizeof(Package), names_equal, (void *)name,
            (void **)&ptr);

  return ptr;
}

LishpSymbol *intern_symbol(Package *p, const char *lexeme) {
  LishpSymbol *sym = NULL;
  int found = map_get(&p->interned_symbols, sizeof(const char *),
                      sizeof(LishpSymbol *), &lexeme, &sym);

  if (found == 0) {
    return sym;
  }

  // not found, so allocate a new one and insert it

  uint32_t len = strlen(lexeme);
  sym = allocate(sizeof(LishpSymbol));
  char *copied_lexeme = allocate(1 + len);
  if (sym == NULL) {
    return NULL;
  }
  if (copied_lexeme == NULL) {
    deallocate(sym, sizeof(LishpSymbol));
    return NULL;
  }
  *sym = SYMBOL(copied_lexeme, p->name);

  // to make sure the string lasts
  copied_lexeme[len] = '\0';
  strncpy(copied_lexeme, lexeme, len);

  map_insert(&p->interned_symbols, sizeof(const char *), sizeof(LishpSymbol *),
             &copied_lexeme, &sym);

  return sym;
}

static void repl(Runtime *rt) {
  Package *system_package = find_package(rt, "SYSTEM");

  LishpSymbol *repl_sym = intern_symbol(system_package, "REPL");
  LishpFunction *repl_fn = symbol_function(system_package->global, repl_sym);

  interpret_function_call(rt->interpreter, repl_fn, NIL_LIST);
}

static int char_cmp(void *l, void *r) {
  char *lptr = l;
  char *rptr = r;

  return *lptr - *rptr;
}

static int initialize_system_readtable(Runtime *rt, LishpReadtable *readtable) {
#define INSTALL_READER_MACRO(ch, name)                                         \
  do {                                                                         \
    char c = ch;                                                               \
    LishpSymbol *fn_sym = intern_symbol(system_package, name);                 \
    LishpFunction *fn = symbol_function(system_package->global, fn_sym);       \
    map_insert(&readtable->reader_macros, sizeof(char),                        \
               sizeof(LishpFunction *), &c, &fn);                              \
  } while (0)

  Package *system_package = find_package(rt, "SYSTEM");

  *readtable = READTABLE(kUpcase);
  TEST_CALL(map_init(&readtable->reader_macros, char_cmp));

  INSTALL_READER_MACRO('(', "READ-OPEN-PAREN");
  INSTALL_READER_MACRO(')', "READ-CLOSE-PAREN");
  INSTALL_READER_MACRO('"', "READ-DOUBLE-QUOTE");
  INSTALL_READER_MACRO('\'', "READ-SINGLE-QUOTE");

  return 0;
#undef INSTALL_READER_MACRO
}

int initialize_runtime(Runtime *rt) {
  rt->repl = repl;
  rt->system_readtable = allocate(sizeof(LishpReadtable));

  if (rt->system_readtable == NULL) {
    return -1;
  }

  TEST_CALL(list_init(&rt->packages));
  TEST_CALL(initialize_packages(rt));

  TEST_CALL(initialize_system_readtable(rt, rt->system_readtable));

  Package *user_package = find_package(rt, "USER");
  Environment *initial_env = user_package->global;
  TEST_CALL(initialize_interpreter(&rt->interpreter, initial_env));

  return 0;
}

static int cleanup_runtime(Runtime *rt) {
  list_clear(&rt->packages);
  return 0;
}

void *_allocate_obj(Runtime *rt, uint32_t size) {
  (void)rt; // FIXME: should this do anything? Or is it not needed at all?
  return allocate(size);
}
