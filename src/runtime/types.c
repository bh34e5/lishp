#include <stdio.h>

#include "runtime/types.h"

int form_cmp(LishpForm l, LishpForm r) {
  if (l.type != r.type) {
    return l.type - r.type;
  }

  switch (l.type) {
  case kT: {
    return 0;
  } break;
  case kNil: {
    return 0;
  } break;
  case kChar: {
    return l.ch - r.ch;
  } break;
  case kFixnum: {
    return l.fixnum - r.fixnum;
  } break;
  case kObject: {
    // objects are eq only if they are the _same_ object
    return l.object - r.object;
  } break;
  }
}

static void print_cons_rec(LishpCons *cons, int first) {
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

static void print_cons(LishpCons *cons) {
  printf("(");
  print_cons_rec(cons, 1);
  printf(")");
}

static void print_stream(LishpStream *stm) {
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

static void print_symbol(LishpSymbol *sym) {
  if (sym->id > 0) {
    // TODO: actually figure out how these are printed
    printf("#%s:%u", sym->lexeme, sym->id);
    return;
  }
  printf("%s", sym->lexeme);
}

static void print_function(LishpFunction *func) {
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

static void print_object(LishpObject *obj) {
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

void print_form(LishpForm f) {
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
