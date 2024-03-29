#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "runtime.h"
#include "runtime/functions.h"
#include "runtime/interpreter.h"
#include "runtime/memory_manager.h"
#include "runtime/types.h"
#include "util.h"

static int symbol_cmp(void *l, void *r) {
  LishpSymbol **l_sym = l;
  LishpSymbol **r_sym = r;

  return *l_sym < *r_sym ? -1 : *l_sym > *r_sym ? 1 : 0;
}

static int char_cmp(void *l, void *r) {
  char *lptr = l;
  char *rptr = r;

  return *lptr - *rptr;
}

static int names_equal(void *arg, void *item) {
  const char *name = arg;
  Package *p = item;

  if (strcmp(name, p->name) == 0) {
    return 1;
  }

  return 0;
}

static int initialize_environment(Environment *env, Environment *parent,
                                  Package *p) {

  env->parent = parent;
  env->package = p->name;

  map_init(&env->symbol_values, symbol_cmp);
  map_init(&env->symbol_functions, symbol_cmp);

  return 0;
}

static void cleanup_environment(Environment *env) {
  map_clear(&env->symbol_functions);
  map_clear(&env->symbol_values);
}

static int empty_name_cmp(void *l, void *r) {
  const char **l_strp = l;
  const char **r_strp = r;

  if (*l_strp == NULL) {
    if (*r_strp == NULL) {
      return 0;
    }
    return -1;
  }
  if (*r_strp == NULL) {
    return 1;
  }

  int cmp = strcmp(*l_strp, *r_strp);

  return cmp;
}

static int initialize_package(Package *p, Runtime *rt, const char *name) {
  p->name = name;
  p->global = ALLOCATE_OBJ(Environment, rt);
  p->current_readtable = rt->system_readtable;

  if (p->global == NULL) {
    return -1;
  }

  TEST_CALL(initialize_environment(p->global, NULL, p));
  TEST_CALL(map_init(&p->interned_symbols, empty_name_cmp));
  TEST_CALL(list_init(&p->exported_symbols));

  LishpSymbol *nil_sym = intern_symbol(rt, p, "NIL");
  LishpSymbol *t_sym = intern_symbol(rt, p, "T");

  bind_value(p->global, nil_sym, NIL);
  bind_value(p->global, t_sym, T);

  return 0;
}

static void cleanup_package(Package *p) {
  list_clear(&p->exported_symbols);
  map_clear(&p->interned_symbols);
  cleanup_environment(p->global);
}

LishpSymbol *intern_symbol(Runtime *rt, Package *p, const char *lexeme) {
  LishpSymbol *sym = NULL;
  int err = map_get(&p->interned_symbols, sizeof(const char *),
                    sizeof(LishpSymbol *), &lexeme, &sym);

  if (err == 0) {
    return sym;
  }

  // not found, so allocate a new one and insert it

  uint32_t len = strlen(lexeme);
  sym = ALLOCATE_OBJ(LishpSymbol, rt);
  char *copied_lexeme = allocate(rt->memory_manager, 1 + len);
  if (sym == NULL) {
    return NULL;
  }
  if (copied_lexeme == NULL) {
    DEALLOCATE_OBJ(LishpSymbol, sym, rt);
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

LishpSymbol *gensym(Runtime *rt, Package *p, const char *lexeme) {
  LishpSymbol *sym = NULL;
  int err = map_get(&p->interned_symbols, sizeof(const char *),
                    sizeof(LishpSymbol *), &lexeme, &sym);

  uint32_t next_id = 1;
  if (err == 0) {
    next_id = sym->id + 1;
    // not returning, since we always want a unique symbol
  }

  // allocate a new symbol and insert it

  sym = ALLOCATE_OBJ(LishpSymbol, rt);
  if (sym == NULL) {
    return NULL;
  }

  char *new_str = NULL;
  if (lexeme != NULL) {
    uint32_t len = strlen(lexeme);
    new_str = allocate(rt->memory_manager, 1 + len);

    if (new_str == NULL) {
      DEALLOCATE_OBJ(LishpSymbol, sym, rt);
      return NULL;
    }

    // to make sure the string lasts
    new_str[len] = '\0';
    strncpy(new_str, lexeme, len);
  }
  *sym = GENSYM(new_str, p->name, next_id);

  map_insert(&p->interned_symbols, sizeof(const char *), sizeof(LishpSymbol *),
             &new_str, &sym);

  return sym;
}

static int intern_symbol_from(Package *target, LishpSymbol *sym) {
  return map_insert(&target->interned_symbols, sizeof(const char *),
                    sizeof(LishpSymbol *), &sym->lexeme, &sym);
}

static int import_package(Package *target, Package *source) {
  int ret = 0;
  for (uint32_t sym_ind = 0;
       ret == 0 && sym_ind < source->exported_symbols.size; ++sym_ind) {
    LishpSymbol *exported;
    list_get(&source->exported_symbols, sizeof(LishpSymbol *), sym_ind,
             &exported);

    ret = intern_symbol_from(target, exported);
  }
  return ret;
}

static int initialize_packages(Runtime *rt) {
#define INSTALL_INHERENT(name, package, lexeme, export)                        \
  do {                                                                         \
    LishpFunction *name##_fn = ALLOCATE_OBJ(LishpFunction, rt);                \
    if (name##_fn == NULL) {                                                   \
      return -1;                                                               \
    }                                                                          \
    *name##_fn = FUNCTION_INHERENT(name);                                      \
                                                                               \
    LishpSymbol *name##_sym = intern_symbol(rt, &package, lexeme);             \
    bind_function(package.global, name##_sym, name##_fn);                      \
                                                                               \
    if (export) {                                                              \
      TEST_CALL(list_push(&package.exported_symbols, sizeof(LishpSymbol *),    \
                          &name##_sym));                                       \
    }                                                                          \
  } while (0)

  const char *system_name = "SYSTEM";
  const char *common_lisp_name = "COMMON-LISP";
  const char *user_name = "USER";

  Package system;
  Package common_lisp;
  Package user;

  TEST_CALL(initialize_package(&system, rt, system_name));
  TEST_CALL(initialize_package(&common_lisp, rt, common_lisp_name));
  TEST_CALL(initialize_package(&user, rt, user_name));

  int no_export = 0;
  int export = 1;

  INSTALL_INHERENT(system_repl, system, "REPL", no_export);
  INSTALL_INHERENT(system_read_open_paren, system, "READ-OPEN-PAREN",
                   no_export);
  INSTALL_INHERENT(system_read_close_paren, system, "READ-CLOSE-PAREN",
                   no_export);
  INSTALL_INHERENT(system_read_double_quote, system, "READ-DOUBLE-QUOTE",
                   no_export);
  INSTALL_INHERENT(system_read_single_quote, system, "READ-SINGLE-QUOTE",
                   no_export);

  INSTALL_INHERENT(common_lisp_read, common_lisp, "READ", export);
  INSTALL_INHERENT(common_lisp_format, common_lisp, "FORMAT", export);

  TEST_CALL(import_package(&user, &common_lisp));

  TEST_CALL(list_push(&rt->packages, sizeof(Package), &system));
  TEST_CALL(list_push(&rt->packages, sizeof(Package), &common_lisp));
  TEST_CALL(list_push(&rt->packages, sizeof(Package), &user));

  return 0;
}

Package *find_package(Runtime *rt, const char *name) {
  Package *ptr = NULL;

  list_find(&rt->packages, sizeof(Package), names_equal, (void *)name,
            (void **)&ptr);

  return ptr;
}

static int initialize_system_readtable(Runtime *rt, LishpReadtable *readtable) {
#define INSTALL_READER_MACRO(ch, name)                                         \
  do {                                                                         \
    char c = ch;                                                               \
    LishpSymbol *fn_sym = intern_symbol(rt, system_package, name);             \
    LishpFunction *fn = symbol_function(rt, system_package->global, fn_sym);   \
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

static void repl(Runtime *rt) {
  Package *system_package = find_package(rt, "SYSTEM");

  LishpSymbol *repl_sym = intern_symbol(rt, system_package, "REPL");
  LishpFunction *repl_fn =
      symbol_function(rt, system_package->global, repl_sym);

  int push_result = push_function(rt->interpreter, repl_fn);

  interpret_function_call(rt->interpreter, 0);
}

void _obj_mark_used(Runtime *rt, LishpObject *obj) {
  mark_used(rt->memory_manager, obj);

  switch (obj->type) {
  case kCons: {
    FORM_MARK_USED(rt, AS(LishpCons, obj)->car);
    FORM_MARK_USED(rt, AS(LishpCons, obj)->cdr);
  } break;
  case kString: {
    const char *lexeme = AS(LishpString, obj)->lexeme;
    if (lexeme != NULL) {
      other_mark_used(rt, (void *)lexeme);
    }
  } break;
  case kSymbol: {
    const char *lexeme = AS(LishpSymbol, obj)->lexeme;
    if (lexeme != NULL) {
      other_mark_used(rt, (void *)lexeme);
    }
  } break;
  case kStream: {
  } break;
  case kFunction: {
  } break;
  case kReadtable: {
  } break;
  }
}

void other_mark_used(Runtime *rt, void *obj) {
  mark_used(rt->memory_manager, obj);
}

static int sym_val_mark_used_it(void *arg, void *key, void *val) {
  Runtime *rt = arg;
  LishpSymbol **sym = key;
  LishpForm *form = val;

  OBJ_MARK_USED(rt, *sym);
  FORM_MARK_USED(rt, *form);

  return 0;
}

static int sym_func_mark_used_it(void *arg, void *key, void *val) {
  Runtime *rt = arg;
  LishpSymbol **sym = key;
  LishpFunction **fn = val;

  OBJ_MARK_USED(rt, *sym);
  OBJ_MARK_USED(rt, *fn);

  return 0;
}

void environment_mark_used(Runtime *rt, Environment *env) {
  mark_used(rt->memory_manager, env);
  if (env->parent != NULL) {
    environment_mark_used(rt, env->parent);
  }

  map_foreach(&env->symbol_values, sizeof(LishpSymbol *), sizeof(LishpForm),
              sym_val_mark_used_it, rt);
  map_foreach(&env->symbol_functions, sizeof(LishpSymbol *),
              sizeof(LishpFunction *), sym_func_mark_used_it, rt);
}

static int interned_syms_mark_used_it(void *arg, void *key, void *val) {
  (void)key;

  Runtime *rt = arg;
  LishpSymbol **sym = val;

  OBJ_MARK_USED(rt, *sym);

  return 0;
}

static int exported_syms_mark_used_it(void *arg, void *obj) {
  Runtime *rt = arg;
  LishpSymbol **sym = obj;

  OBJ_MARK_USED(rt, *sym);

  return 0;
}

static int package_mark_used_it(void *arg, void *obj) {
  Runtime *rt = arg;
  Package *p = obj;

  environment_mark_used(rt, p->global);
  OBJ_MARK_USED(rt, p->current_readtable);
  map_foreach(&p->interned_symbols, sizeof(const char *), sizeof(LishpSymbol *),
              interned_syms_mark_used_it, rt);
  list_foreach(&p->exported_symbols, sizeof(LishpSymbol *),
               exported_syms_mark_used_it, rt);

  return 0;
}

static void objs_mark_used(Runtime *rt) {
  if (rt->system_readtable != NULL) {
    OBJ_MARK_USED(rt, rt->system_readtable);
  }

  if (rt->interpreter != NULL) {
    interpreter_mark_used_objs(rt->interpreter);
  }

  list_foreach(&rt->packages, sizeof(Package), package_mark_used_it, rt);
}

int initialize_runtime(Runtime *rt) {
  rt->repl = repl;

  TEST_CALL(initialize_manager(&rt->memory_manager, objs_mark_used, rt));

  rt->system_readtable = ALLOCATE_OBJ(LishpReadtable, rt);

  if (rt->system_readtable == NULL) {
    return -1;
  }

  TEST_CALL(list_init(&rt->packages));
  TEST_CALL(initialize_packages(rt));

  TEST_CALL(initialize_system_readtable(rt, rt->system_readtable));

  Package *user_package = find_package(rt, "USER");
  Environment *initial_env = user_package->global;
  TEST_CALL(initialize_interpreter(&rt->interpreter, rt, initial_env));

  return 0;
}

static int cleanup_package_it(void *arg, void *obj) {
  (void)arg;
  cleanup_package((Package *)obj);
  return 0;
}

int cleanup_runtime(Runtime *rt) {
  int cleanup_interpreter_result = cleanup_interpreter(&rt->interpreter);
  rt->interpreter = NULL;

  // TODO: cleanup system readtable?

  list_foreach(&rt->packages, sizeof(Package), cleanup_package_it, NULL);
  list_clear(&rt->packages);

  uint32_t remaining_bytes;
  int cleanup_manager_result =
      cleanup_manager(&rt->memory_manager, &remaining_bytes);

  fprintf(stdout, "[runtime]: Cleanup with %u bytes still allocated\n",
          remaining_bytes);

  return 0;
}

void *_allocate_obj(Runtime *rt, uint32_t size) {
  return allocate(rt->memory_manager, size);
}

const char *allocate_str(Runtime *rt, const char *to_copy) {
  uint32_t len = strlen(to_copy);

  char *dest = allocate(rt->memory_manager, 1 + len);
  strncpy(dest, to_copy, len);
  dest[len] = '\0';

  return dest;
}

void _deallocate_obj(Runtime *rt, void *ptr, uint32_t size) {
  deallocate(rt->memory_manager, ptr, size);
}

Environment *allocate_env(Runtime *rt, Environment *parent) {
  Package *p = find_package(rt, parent->package);

  Environment *new_env = ALLOCATE_OBJ(Environment, rt);
  if (new_env == NULL) {
    return NULL;
  }
  TEST_CALL_LABEL(failure, initialize_environment(new_env, parent, p));

  return new_env;

failure:
  DEALLOCATE_OBJ(Environment, new_env, rt);

  return NULL;
}
