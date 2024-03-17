#ifndef runtime_reader_h
#define runtime_reader_h

#include <stdio.h>

#include "runtime/interpreter.h"
#include "runtime/types.h"

typedef struct reader {
  Runtime *rt;
  Interpreter *interpreter;
  FILE *in;
} Reader;

int initialize_reader(Reader *reader, Runtime *rt, Interpreter *interpreter,
                      FILE *in);

int cleanup_reader(Reader *reader);
LishpForm read_form(Reader *reader);

#endif
