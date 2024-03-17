#include <assert.h>

#include "runtime.h"
#include "runtime/functions.h"
#include "runtime/interpreter.h"
#include "runtime/reader.h"
#include "runtime/types.h"

LishpFunctionReturn common_lisp_read(Interpreter *interpreter, LishpList args) {
  Runtime *rt = get_runtime(interpreter);

  FILE *in = stdin;

  if (!args.nil) {
    LishpForm stream_form = args.cons->car;

    assert(IS_OBJECT_TYPE(stream_form, kStream) && "Expected stream!");

    LishpStream *stream_obj = AS_OBJECT(LishpStream, stream_form);
    assert((stream_obj->type == kInput || stream_obj->type == kInputOutput) &&
           "Expected input stream!");

    in = stream_obj->file;
  }

  Reader reader;
  int init_response = initialize_reader(&reader, rt, interpreter, in);

  LishpForm form_in = read_form(&reader);

  int cleanup_response = cleanup_reader(&reader);

  printf("\nJust got: ");
  print_form(form_in);
  printf("\n");
  return SINGLE_RETURN(form_in);
}

LishpFunctionReturn common_lisp_format(Interpreter *interpreter,
                                       LishpList args) {

  assert(!args.nil && "Arguments expected!");

  LishpForm rest;

  LishpCons *stream_rest_cons = args.cons;
  LishpForm stream_form = stream_rest_cons->car;
  rest = stream_rest_cons->cdr;

  assert(IS_OBJECT_TYPE(rest, kCons) && "Format string expected!");
  LishpCons *fmt_str_rest_cons = AS_OBJECT(LishpCons, rest);
  LishpForm fmt_str_form = fmt_str_rest_cons->car;
  rest = fmt_str_rest_cons->cdr;

  FILE *out = stdout;
  assert(T_P(stream_form) && "Cannot write to custom stream!");
  assert(IS_OBJECT_TYPE(fmt_str_form, kString) &&
         "Format string should be a string!");

  fprintf(out, "\nArguments: ");
  if (args.nil) {
    print_form(NIL);
  } else {
    print_form(FROM_OBJ(args.cons));
  }
  fprintf(out, "\n");

  fprintf(out, "%s\n", AS_OBJECT(LishpString, fmt_str_form)->lexeme);

  // TODO: print out arguments

  return EMPTY_RETURN;
}
