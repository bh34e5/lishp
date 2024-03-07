#ifndef runtime_types_
#define runtime_types_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "util.h"

#define NIL ((LishpForm){.type = kNil, {.object = NULL}})
#define T ((LishpForm){.type = kT, {.object = NULL}})

#define FROM_FIXNUM(f) ((LishpForm){.type = kFixnum, {.fixnum = (f)}})
#define FROM_CHAR(c) ((LishpForm){.type = kChar, {.ch = (c)}})
#define FROM_OBJ(o)                                                            \
  ((LishpForm){.type = kObject, {.object = ((LishpObject *)o)}})

#define NIL_P(form) ((form).type == kNil)
#define T_P(form) ((form).type == kT)
#define OBJECT_P(form) ((form).type == kObject)

#define IS_OBJECT_TYPE(f, t) (OBJECT_P(f) && ((f).object->type == t))

#define AS_OBJECT(type, form) ((type *)((form).object))
#define AS(type, object) ((type *)(object))

// constructors

#define CONS(a, d) ((LishpCons){.obj = {.type = kCons}, .car = (a), .cdr = (d)})
#define STRING(l) ((LishpString){.obj = {.type = kString}, .lexeme = (l)})
#define SYMBOL(l, p)                                                           \
  ((LishpSymbol){                                                              \
      .obj = {.type = kSymbol}, .lexeme = (l), .package = (p), .id = 0})
#define FUNCTION_INHERENT(p)                                                   \
  ((LishpFunction){                                                            \
      .obj = {.type = kFunction}, .type = kInherent, {.inherent_fn = (p)}})
#define READTABLE(c)                                                           \
  ((LishpReadtable){                                                           \
      .obj = {.type = kReadtable}, .readcase = (c), .reader_macros = {0}})
#define STREAM(t, s)                                                           \
  ((LishpStream){.obj = {.type = kStream}, .type = (t), .file = (s)})

#define GO_RETURN(f)                                                           \
  ((LishpFunctionReturn){                                                      \
      .type = kGoReturn, .multiple_return = 0, {.go_target = (f)}})
#define EMPTY_RETURN                                                           \
  ((LishpFunctionReturn){                                                      \
      .type = kValuesReturn, .return_count = 0, {.first_return = NIL}})
#define SINGLE_RETURN(f)                                                       \
  ((LishpFunctionReturn){                                                      \
      .type = kValuesReturn, .return_count = 1, {.first_return = (f)}})

#define CHECK_GO_RET(r)                                                        \
  do {                                                                         \
    if ((r).type == kGoReturn) {                                               \
      return (r);                                                              \
    }                                                                          \
  } while (0);
#define CHECK_GO_RET_LABEL(l, r)                                               \
  do {                                                                         \
    if ((r).type == kGoReturn) {                                               \
      goto l;                                                                  \
    }                                                                          \
  } while (0);

#define NIL_LIST ((LishpList){.nil = 1, .cons = NULL})
#define LIST_OF(c) ((LishpList){.nil = 0, .cons = (c)})

// enums

typedef enum {
  kCons,
  kString,
  kSymbol,
  kFunction,
  kReadtable,
  kStream,
} ObjectType;

typedef enum {
  kFixnum,
  kChar,
  kNil,
  kT,
  kObject,
} FormType;

typedef enum {
  kGoReturn,
  kValuesReturn,
} FunctionReturnType;

typedef enum {
  kInherent,
  kUserDefined,
} FunctionType;

typedef enum {
  kUpcase,
  kDowncase,
  kPreserve,
  kInvert,
} ReadtableCase;

typedef enum {
  kInput,
  kOutput,
  kInputOutput,
} StreamType;

// types

typedef struct {
  ObjectType type;
} LishpObject;

typedef struct {
  FormType type;
  union {
    uint32_t fixnum;
    char ch;
    LishpObject *object;
  };
} LishpForm;

typedef struct {
  LishpObject obj;
  LishpForm car;
  LishpForm cdr;
} LishpCons;

typedef struct {
  int nil;
  LishpCons *cons;
} LishpList;

typedef struct {
  LishpObject obj;
  const char *lexeme;
} LishpString;

typedef struct {
  LishpObject obj;
  const char *lexeme;
  const char *package; // FIXME: how should I reference packages?
  uint32_t id; // NOTE: if id == 0, it's not unique, but if it's non-zero it has
               // been gensym'ed.
} LishpSymbol;

typedef struct {
  FunctionReturnType type;
  uint32_t return_count; // flag to indicate whether the other_returns list is
                         // initialized and used
  union {
    LishpForm go_target;
    LishpForm first_return; // alias the first value
  };
  List other_returns; // a list of the rest of the return values if necessary
} LishpFunctionReturn;

// forward declare interpreter so it can be used in the inherent function
// pointer

struct interpreter;
typedef LishpFunctionReturn (*InherentFnPtr)(struct interpreter *,
                                             LishpList args);
#define INHERENT_FN(name)                                                      \
  LishpFunctionReturn name(struct interpreter *interpreter, LishpList args)

typedef struct {
  LishpObject obj;
  FunctionType type;
  union {
    InherentFnPtr inherent_fn;
  };
} LishpFunction;

typedef struct {
  LishpObject obj;
  ReadtableCase readcase;
  OrderedMap reader_macros;
} LishpReadtable;

typedef struct {
  LishpObject obj;
  StreamType type;
  FILE *file;
} LishpStream;

#include <stdio.h>
static inline void print_form(LishpForm);

static inline void print_cons_rec(LishpCons *cons, int first) {
  if (!first) {
    printf(" ");
  }
  print_form(cons->car);

  if (NIL_P(cons->cdr)) {
    // nothing else to add
    return;
  }

  if (IS_OBJECT_TYPE(cons->cdr, kCons)) {
    LishpCons *next = AS_OBJECT(LishpCons, cons->cdr);
    print_cons_rec(next, 0);
    return;
  }

  printf(" . ");
  print_form(cons->cdr);
}

static inline void print_cons(LishpCons *cons) {
  printf("(");
  print_cons_rec(cons, 1);
  printf(")");
}

static inline void print_stream(LishpStream *stm) {
  switch (stm->type) {
  case kInput: {
    printf("<INPUT_STREAM>");
  } break;
  case kOutput: {
    printf("<OUTPUT_STREAM>");
  } break;
  case kInputOutput: {
    printf("<INPUT_OUTPUT_STREAM>");
  } break;
  }
}

static inline void print_symbol(LishpSymbol *sym) {
  if (sym->id > 0) {
    // TODO: actually figure out how these are printed
    printf("#%s:%u", sym->lexeme, sym->id);
    return;
  }
  printf("%s", sym->lexeme);
}

static inline void print_function(LishpFunction *func) {
  switch (func->type) {
  case kInherent: {
    printf("<INHERENT_FN>");
  } break;
  // case kSpecialForm: {
  //   printf("<SPECIAL_FORM>");
  // } break;
  case kUserDefined: {
    printf("<USER_DEFINED_FN>");
  } break;
  }
}

static inline void print_object(LishpObject *obj) {
  switch (obj->type) {
  case kCons: {
    print_cons(AS(LishpCons, obj));
  } break;
  case kStream: {
    print_stream(AS(LishpStream, obj));
  } break;
  case kString: {
    printf("\"%s\"", AS(LishpString, obj)->lexeme);
  } break;
  case kSymbol: {
    print_symbol(AS(LishpSymbol, obj));
  } break;
  case kFunction: {
    print_function(AS(LishpFunction, obj));
  } break;
  case kReadtable: {
    printf("<READTABLE>");
  } break;
  }
}

static inline void print_form(LishpForm f) {
  switch (f.type) {
  case kT: {
    printf("T");
  } break;
  case kNil: {
    printf("NIL");
  } break;
  case kChar: {
    printf("CHAR(%c)", f.ch);
  } break;
  case kFixnum: {
    printf("FIXNUM(%u)", f.fixnum);
  } break;
  case kObject: {
    print_object(f.object);
  } break;
  }
}

#endif
