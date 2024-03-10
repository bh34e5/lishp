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
#define GENSYM(l, p, i)                                                        \
  ((LishpSymbol){                                                              \
      .obj = {.type = kSymbol}, .lexeme = (l), .package = (p), .id = (i)})
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
      .type = kGoReturn, .return_count = 0, {.go_target = (f)}})
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

void print_form(LishpForm);
int form_cmp(LishpForm l, LishpForm r);

#endif
