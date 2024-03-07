#ifndef runtime_reader_h
#define runtime_reader_h

#include <stdio.h>

#include "runtime/interpreter.h"
#include "runtime/types.h"

typedef struct reader Reader;

int initialize_reader(Reader **reader, Interpreter *interpreter, FILE *in);
LishpForm read_form(Reader *reader);

#endif
