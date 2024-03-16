#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "runtime.h"
#include "runtime/functions.h"
#include "runtime/interpreter.h"
#include "runtime/types.h"

LishpFunctionReturn system_repl(Interpreter *interpreter, LishpList args) {
  (void)args;

  Runtime *rt = get_runtime(interpreter);
  Package *common_lisp = find_package(rt, "COMMON-LISP");

  LishpSymbol *read_sym = intern_symbol(rt, common_lisp, "READ");
  LishpSymbol *format_sym = intern_symbol(rt, common_lisp, "FORMAT");

  LishpFunction *read_fn = symbol_function(rt, common_lisp->global, read_sym);
  LishpFunction *format_fn =
      symbol_function(rt, common_lisp->global, format_sym);

  while (1) {
    push_function(interpreter, read_fn);

    LishpFunctionReturn read_ret = interpret_function_call(interpreter, 0);
    CHECK_GO_RET(read_ret);

    LishpForm read_form = read_ret.first_return;

    // check if the user input `(quit)` to leave the REPL

    if (IS_OBJECT_TYPE(read_form, kCons)) {
      LishpForm car = AS_OBJECT(LishpCons, read_form)->car;
      if (IS_OBJECT_TYPE(car, kSymbol)) {
        if (strcmp("QUIT", AS_OBJECT(LishpSymbol, car)->lexeme) == 0) {
          return EMPTY_RETURN;
        }
      }
    }

    // NOTE: don't need to call eval, because the form gets evaluated as a
    // result of calling the FORMAT function

    push_function(interpreter, format_fn);

    push_argument(interpreter, T);

    LishpString *output_str = ALLOCATE_OBJ(LishpString, rt);
    *output_str = STRING("~A~%");
    push_argument(interpreter, FROM_OBJ(output_str));

    push_argument(interpreter, read_form);

    LishpFunctionReturn format_ret = interpret_function_call(interpreter, 3);

    CHECK_GO_RET(format_ret);
  }
  // bye bye
}

static int is_whitespace(List *l, long pos) {
  const char *s = l->items;
  for (uint32_t index = pos; index < l->size; ++index) {
    // TODO: change this to check against LISP whitespace, and not just c++
    // whitespace
    if (!isspace(s[index])) {
      return 0;
    }
  }
  return 1;
}

static void add_character(List *l, char c) { list_push(l, sizeof(char), &c); }

LishpFunctionReturn system_read_open_paren(Interpreter *interpreter,
                                           LishpList args) {
  Runtime *rt = get_runtime(interpreter);

  Package *common_lisp = find_package(rt, "COMMON-LISP");
  LishpSymbol *read_sym = intern_symbol(rt, common_lisp, "READ");
  LishpFunction *user_read = symbol_function(rt, common_lisp->global, read_sym);

  assert(!args.nil && "Expected arguments!");

  LishpForm stream_form = args.cons->car;

  assert(IS_OBJECT_TYPE(stream_form, kStream) && "Expected stream!");
  LishpStream *stm_obj = AS_OBJECT(LishpStream, stream_form);
  FILE *stm = stm_obj->file;

  List string_buf;
  list_init(&string_buf);

  int open_paren_count = 1;
  while (1) {
    char c = fgetc(stm);
    if (feof(stm)) {
      // TODO: handle EOF
    }

    if (c == '(') {
      ++open_paren_count;
    } else if (c == ')') {
      --open_paren_count;
      if (!open_paren_count) {
        break;
      }
    }

    add_character(&string_buf, c);
  }

  LishpForm res_form = NIL;
  LishpCons *last_cons = NULL;

  FILE *temp_file_stream = tmpfile();
  fwrite(string_buf.items, sizeof(char), string_buf.size, temp_file_stream);
  rewind(temp_file_stream);

  while (1) {
    long pos = ftell(temp_file_stream);
    if (is_whitespace(&string_buf, pos)) {
      break;
    }

    push_function(interpreter, user_read);

    // Yes! Lets allocate a new stream object every time! /s
    //
    // This should eventually probably be a bound variable so it's _somewhere_
    // non-local stack.
    LishpStream *sstream_obj = ALLOCATE_OBJ(LishpStream, rt);
    *sstream_obj = STREAM(kInputOutput, temp_file_stream);
    push_argument(interpreter, FROM_OBJ(sstream_obj));

    LishpFunctionReturn read_res = interpret_function_call(interpreter, 1);

    if (read_res.type == kGoReturn) {
      list_clear(&string_buf);
      return read_res;
    }

    assert(read_res.return_count == 1); // FIXME: This may be able to be 0??

    LishpForm form = read_res.first_return;

    LishpCons *next_cons = ALLOCATE_OBJ(LishpCons, rt);
    *next_cons = CONS(form, NIL);

    if (last_cons != NULL) {
      last_cons->cdr = FROM_OBJ(next_cons);
    } else {
      res_form = FROM_OBJ(next_cons);
    }

    last_cons = next_cons;
  }

  list_clear(&string_buf);

  return SINGLE_RETURN(res_form);
}

LishpFunctionReturn system_read_close_paren(Interpreter *interpreter,
                                            LishpList args) {
  (void)interpreter;
  (void)args;
  // FIXME: figure out the proper way to handle this as well...
  assert(0 && "Cannot have ')' in a form... or something");
}

static char escape_char(char c) {
  switch (c) {
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case 't':
    return '\t';
  case 'v':
    return '\v';
  case '0':
    return '\0';
  case '1':
    return '\1';
  case '2':
    return '\2';
  case '3':
    return '\3';
  case '4':
    return '\4';
  case '5':
    return '\5';
  case '6':
    return '\6';
  case '7':
    return '\7';
  case '?':
    return '\?';
  default:
    return c;
  }
}

LishpFunctionReturn system_read_double_quote(Interpreter *interpreter,
                                             LishpList args) {
  // FIXME: there is a possible bug... when I ran the interpreter with the
  // following, I appear to have gotten stuck in a loop:
  //
  // ((lambda (a1) a1) "argstr)
  // )
  // ")
  //
  // We may not be handling newlines correctly? I'm not sure.

  // get the stream
  Runtime *rt = get_runtime(interpreter);

  assert(!args.nil && "Expected arguments!");

  LishpForm stream_form = args.cons->car;

  assert(IS_OBJECT_TYPE(stream_form, kStream) && "Expected stream!");
  LishpStream *stm_obj = AS_OBJECT(LishpStream, stream_form);

  FILE *stm = stm_obj->file;

  List builder;
  list_init(&builder);

  int escaped_next = 0;
  while (1) {
    char c = fgetc(stm);

    if (c == '\"' && !escaped_next) {
      break;
    } else if (c == '\\' && !escaped_next) {
      escaped_next = 1;
      continue;
    } else if (escaped_next) {
      // handle escape characters
      add_character(&builder, escape_char(c));
    } else {
      add_character(&builder, c);
    }

    escaped_next = 0;
  }
  // add null terminator to string
  add_character(&builder, '\0');

  LishpString *str_obj = ALLOCATE_OBJ(LishpString, rt);
  *str_obj = STRING(builder.items); // FIXME: this is no longer being tracked!
                                    // Actually, I suppose it kind of is...? The
                                    // only problem is it's tracked through the
                                    // realloc / free, not the custom allocator.
  // list_clear(&builder);

  return SINGLE_RETURN(FROM_OBJ(str_obj));
}

LishpFunctionReturn system_read_single_quote(Interpreter *interpreter,
                                             LishpList args) {
  Runtime *rt = get_runtime(interpreter);

  Package *common_lisp = find_package(rt, "COMMON-LISP");
  LishpSymbol *read_sym = intern_symbol(rt, common_lisp, "READ");
  LishpSymbol *quote_sym = intern_symbol(rt, common_lisp, "QUOTE");

  LishpFunction *user_read = symbol_function(rt, common_lisp->global, read_sym);

  assert(!args.nil && "Expected arguments!");

  LishpForm stream_form = args.cons->car;

  assert(IS_OBJECT_TYPE(stream_form, kStream) && "Expected stream!");

  push_function(interpreter, user_read);
  push_argument(interpreter, stream_form);
  LishpFunctionReturn read_func_ret = interpret_function_call(interpreter, 1);

  assert(read_func_ret.return_count == 1);
  LishpForm read_form = read_func_ret.first_return;

  LishpForm *pret_form;
  push_form_return(interpreter, &pret_form);

  LishpCons *quote_rest = ALLOCATE_OBJ(LishpCons, rt);
  *quote_rest = CONS(FROM_OBJ(quote_sym), NIL);
  *pret_form = FROM_OBJ(quote_rest);

  LishpCons *form_nil = ALLOCATE_OBJ(LishpCons, rt);
  *form_nil = CONS(read_form, NIL);
  quote_rest->cdr = FROM_OBJ(form_nil);

  LishpForm ret_form;
  pop_form_return(interpreter, &ret_form);

  return SINGLE_RETURN(ret_form);
}
