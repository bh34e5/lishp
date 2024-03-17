#include <assert.h>
#include <stdlib.h>

#include "runtime/interpreter.h"
#include "runtime/reader.h"
#include "util.h"

typedef enum {
  kTokenNumber,
  kTokenSymbol,
  kTokenInvalid,
} TokenType;

typedef struct {
  TokenType type;
  List characters;
} Token;

typedef enum {
  kCharConstituent,
  kCharInvalid,
  kCharTerminatingMacroCharacter,
  kCharNonTerminatingMacroCharacter,
  kCharMultipleEscape,
  kCharSingleEscape,
  kCharWhitespace,
} CharTraits;

static ReadtableCase char_case(char c) {
  // Assumes the input is a cased character
  if (c >= 'A' && c <= 'Z') {
    return kUpcase;
  }
  return kDowncase;
}

static int cased_char(const char c) {
  return (c >= 'a' && c < 'z') || (c >= 'A' && c <= 'Z');
}

static char to_upper(char c) {
  if (c >= 'a' && c <= 'z') {
    return (char)(c - 'a' + 'A');
  }
  return c;
}

static char to_lower(char c) {
  if (c >= 'A' && c <= 'Z') {
    return (char)(c - 'A' + 'a');
  }
  return c;
}

static char convert_case(LishpReadtable *readtable, char c) {
  if (!cased_char(c)) {
    return c;
  }

  switch (readtable->readcase) {
  case kUpcase:
    return to_upper(c);
  case kDowncase:
    return to_lower(c);
  case kPreserve:
    return c;
  case kInvert:
    switch (char_case(c)) {
    case kUpcase:
      return to_lower(c);
    case kDowncase:
      return to_upper(c);
    default:
      assert(0 && "Unreachable");
    }
  }
}

static void add_character(LishpReadtable *readtable, Token *token, char c) {
  char converted = convert_case(readtable, c);
  list_push(&token->characters, sizeof(char), &converted);
}

static int is_digit(const char c) { return '0' <= c && c <= '9'; }

static int is_potential_number(Token token) {
  // FIXME; this is a completely broken implementation that is just here for
  // testing at the moment.

  uint32_t ind = 0;
  const char *c = token.characters.items;
  while (ind < token.characters.size) {
    if (!is_digit(*(c + ind))) {
      return 0;
    }
    ++ind;
  }
  return 1;
}

static TokenType get_type(Token token, int had_escape) {
  if (had_escape) {
    return kTokenSymbol;
  }

  if (is_potential_number(token)) {
    // TODO: figure out what the actual thing to do here is.
    return kTokenNumber;
  }

  // There should be a way to have an invalid token, but I forget what that is
  return kTokenSymbol;
}

int check_non_terminating_macro_char(char c) { return c == '#'; }

int check_macro_char(LishpReadtable *readtable, char c, CharTraits *type) {
  if (check_non_terminating_macro_char(c)) {
    *type = kCharNonTerminatingMacroCharacter;
    return 1;
  }

  if (map_ref(&readtable->reader_macros, sizeof(char), sizeof(LishpFunction *),
              &c, NULL) < 0) {

    return 0;
  }

  *type = kCharTerminatingMacroCharacter;
  return 1;
}

CharTraits get_char_traits(LishpReadtable *readtable, char c) {
  CharTraits type;
  if (check_macro_char(readtable, c, &type)) {
    return type;
  }

  switch (c) {
  case '\t':
  case '\n':
  case '\f':
  case '\r':
  case ' ': {
    // TODO: figure out difference between "page" and "linefeed"
    type = kCharWhitespace;
  } break;
  case '\'': {
    type = kCharSingleEscape;
  } break;
  case '|': {
    type = kCharMultipleEscape;
  } break;
  case '\b':
  case '!':
  case '$':
  case '%':
  case '&':
  case '*':
  case '+':
  case '-':
  case '.':
  case '/':
  case ':':
  case '<':
  case '=':
  case '>':
  case '?':
  case '@':
  case '[':
  case ']':
  case '^':
  case '_':
  case '{':
  case '}':
  case '~': {
    type = kCharConstituent;
  } break;
  default: {
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z')) {
      type = kCharConstituent;
    } else {
      type = kCharInvalid;
    }
  } break;
  }

  return type;
}

int initialize_reader(Reader *reader, Runtime *rt, Interpreter *interpreter,
                      FILE *in) {

  reader->rt = rt;
  reader->interpreter = interpreter;
  reader->in = in;

  return 0;
}

int cleanup_reader(Reader *reader) {
  (void)reader;
  return 0;
}

LishpForm read_form(Reader *reader) {
  Runtime *rt = reader->rt;
  // FIXME: get the value of *readtable* in the current dynamic environment
  LishpReadtable *readtable = rt->system_readtable;

  LishpForm result = NIL;

  Token cur_token;
  list_init(&cur_token.characters);

  int had_escape = 0;

  char x;

step_1:
  x = fgetc(reader->in);

  if (feof(reader->in)) {
    // TODO: handle EOF
    assert(0 && "Reached EOF while reading form");
  }

  switch (get_char_traits(readtable, x)) {
  case kCharInvalid:
    assert(0 && "Error Type: reader-error");
  case kCharWhitespace:
    goto step_1;
  case kCharTerminatingMacroCharacter:
  case kCharNonTerminatingMacroCharacter: {
    Interpreter *interpreter = reader->interpreter;

    LishpFunction *macro_fn = NULL;
    map_get(&readtable->reader_macros, sizeof(char), sizeof(LishpFunction *),
            &x, &macro_fn);

    int push_result0 = push_function(interpreter, macro_fn);

    LishpStream *stream_obj = ALLOCATE_OBJ(LishpStream, rt);
    *stream_obj = STREAM(kInput, reader->in);
    int push_result1 = push_argument(interpreter, FROM_OBJ(stream_obj));

    int push_result2 = push_argument(interpreter, FROM_CHAR(x));

    LishpFunctionReturn ret = interpret_function_call(interpreter, 2);
    assert(ret.type != kGoReturn &&
           "How did we get here? A GO result from macro function");

    if (ret.return_count == 0) {
      goto step_1;
    } else if (ret.return_count == 1) {
      result = ret.first_return;
      goto cleanup;
    } else {
      assert(0 && "Someone did an oopsie and returned too many values");
    }
  }
  case kCharSingleEscape: {
    had_escape = 1;
    goto step_8;
  }
  case kCharMultipleEscape: {
    had_escape = 1;
    goto step_9;
  }
  case kCharConstituent: {
    add_character(readtable, &cur_token, x);

  step_8: {
    char y = fgetc(reader->in);
    if (feof(reader->in)) {
      goto step_10;
    }

    switch (get_char_traits(readtable, y)) {
    case kCharConstituent:
    case kCharNonTerminatingMacroCharacter:
      add_character(readtable, &cur_token, y);
      goto step_8;
    case kCharSingleEscape: {
      char z = fgetc(reader->in);
      if (feof(reader->in)) {
        assert(0 && "Error Type: end-of-file");
      }
      add_character(readtable, &cur_token, z);
      had_escape = 1;
      goto step_8;
    }
    case kCharMultipleEscape:
      had_escape = 1;
      goto step_9;
    case kCharInvalid:
      assert(0 && "Error Type: reader-error");
    case kCharTerminatingMacroCharacter:
      ungetc(y, reader->in);
      goto step_10;
    case kCharWhitespace:
      // TODO: read about `read-preserving-whitespace`
      if (1) {
        ungetc(y, reader->in);
      }
      goto step_10;
    }
  }

  step_9: {
    char y = fgetc(reader->in);
    if (feof(reader->in)) {
      assert(0 && "Error Type: end-of-file");
    }

    switch (get_char_traits(readtable, y)) {
    case kCharConstituent:
    case kCharTerminatingMacroCharacter:
    case kCharNonTerminatingMacroCharacter:
    case kCharWhitespace:
      add_character(readtable, &cur_token, y);
      goto step_9;
    case kCharSingleEscape: {
      char z = fgetc(reader->in);
      if (feof(reader->in)) {
        assert(0 && "Error Type: end-of-file");
      }
      add_character(readtable, &cur_token, z);
      had_escape = 1;
      goto step_9;
    }
    case kCharMultipleEscape:
      had_escape = 1;
      goto step_8;
    case kCharInvalid:
      assert(0 && "Error Type: reader-error");
    }
  }
  }
  }

  // if invalid syntax, throw an error.

step_10:
  switch (get_type(cur_token, had_escape)) {
  case kTokenSymbol: {
    // TODO: It should probably intern into the package into which it is being
    // read? Actually, this reminds me that I am not handling the package
    // qualified symbols either... tbf, I'm not handling really anything
    // correctly yet :P

    // zero-terminate so that the interning and copying and stuff goes correctly
    add_character(readtable, &cur_token, '\0');

    Environment *cur_env = get_current_environment(reader->interpreter);
    Package *cur_package = find_package(rt, cur_env->package);
    LishpSymbol *new_symbol =
        intern_symbol(rt, cur_package, cur_token.characters.items);

    result = FROM_OBJ(new_symbol);
    goto cleanup;
  }
  case kTokenNumber: {
    // TODO: double check, right now just assuming we have integers
    // TODO: get the number from the token string (but better!)
    long parsed = strtol(cur_token.characters.items, NULL, 10);
    result = FROM_FIXNUM(parsed);
    goto cleanup;
  }
  default:
    assert(0 && "Unimplemented: read_form");
  }

cleanup:
  list_clear(&cur_token.characters);
  return result;
}
