#include <assert.h>
#include <stdlib.h>

#include "common.h"
#include "runtime/interpreter.h"

typedef enum {
  kOpNop,
  kOpPush,
  kOpPop,
  kOpRetForm,
  kOpBindTag,
  kOpBindValue,
  kOpPushLexicalEnv,
  kOpPopLexicalEnv,
  kOpLookupSymbol,
  kOpLookupFunction,
  kOpFuncall,
  kOpGoReturn,
} Opcode;

int32_t net_effects[] = {
    [kOpNop] = 0,           [kOpPush] = 1,          [kOpPop] = -1,
    [kOpRetForm] = -1,      [kOpBindTag] = -1,      [kOpPushLexicalEnv] = 0,
    [kOpPopLexicalEnv] = 0, [kOpLookupSymbol] = -1, [kOpLookupFunction] = -1,
    [kOpFuncall] = -1,      [kOpGoReturn] = -1,
};

typedef struct {
  Opcode op;
  union {
    LishpForm target;
    uint32_t index;
    uint32_t arg_count;
  };
} Bytecode;

typedef enum {
  kSourceFuncall,
  kSourceBytes,
  kSourceBase,
} FrameSource;

typedef struct frame {
  Environment *env;
  FrameSource source;
  struct frame *prev;
} Frame;

struct interpreter {
  Runtime *rt;
  List last_return_value;
  List form_stack;
  List frame_stack;

  OrderedMap function_environments; // LishpFunction * -> Environment *
  OrderedMap environment_bindings;  // Environment * -> LishpForm -> uint32_t
};

static void get_top_frame_ref(Interpreter *interpreter, Frame **pframe) {
  list_ref_last(&interpreter->frame_stack, sizeof(Frame), (void **)pframe);
}

#define FORM_COUNT (sizeof(special_forms) / sizeof(special_forms[0]))

typedef enum {
  kSpNone,
  kSpGo,
  kSpLetStar,
  kSpProgn,
  kSpQuote,
  kSpTagbody,
} SpecialForm;

const char *special_forms[] = {
    // NOTE: make sure this is alphabetic
    [kSpGo] = "GO",       [kSpLetStar] = "LET*",    [kSpProgn] = "PROGN",
    [kSpQuote] = "QUOTE", [kSpTagbody] = "TAGBODY",
};

static SpecialForm is_special_form(LishpSymbol *sym) {
  const char *lexeme = sym->lexeme;

  int cmp;
  uint32_t i = 1;
  while (i < FORM_COUNT && (cmp = strcmp(lexeme, special_forms[i])) > 0) {
    ++i;
  }

  if (cmp == 0) {
    return (SpecialForm)i;
  }
  return kSpNone;
}

static int push_environment(Interpreter *interpreter, FrameSource source);
static void pop_environment(Interpreter *interpreter);

static int incrementer(void *arg, void *obj) {
  uint32_t *pinc = arg;
  Bytecode *pbyte = obj;

  if (pbyte->op == kOpBindTag) {
    pbyte->index += *pinc;
  }
  return 0;
}

static void increment_indices_by(List *list, uint32_t inc) {
  list_foreach(list, sizeof(Bytecode), incrementer, &inc);
}

#define PUSH_BYTE_1(list, opcode)                                              \
  do {                                                                         \
    Bytecode byte = (Bytecode){.op = (opcode)};                                \
    list_push((list), sizeof(Bytecode), &byte);                                \
  } while (0)

#define _PUSH_BYTE_2(list, opcode, f, t)                                       \
  do {                                                                         \
    Bytecode byte = (Bytecode){.op = (opcode), {.f = (t)}};                    \
    list_push((list), sizeof(Bytecode), &byte);                                \
  } while (0)
#define PUSH_BYTE_2_TARGET(list, opcode, t)                                    \
  _PUSH_BYTE_2(list, opcode, target, t)
#define PUSH_BYTE_2_INDEX(list, opcode, t) _PUSH_BYTE_2(list, opcode, index, t)
#define PUSH_BYTE_2_ARG_COUNT(list, opcode, t)                                 \
  _PUSH_BYTE_2(list, opcode, arg_count, t)

static int analyze_form(List *res, LishpForm form);
static int analyze_form_rec(List *res, LishpForm form, int with_ret);

static int analyze_tagbody(List *res, LishpForm args) {
  if (NIL_P(args)) {
    PUSH_BYTE_2_TARGET(res, kOpPush, NIL);
    return 0;
  }

  assert(IS_OBJECT_TYPE(args, kCons) && "Cannot handle dotted pair in tagbody");

  List tag_bytes;
  List form_bytes;
  list_init(&tag_bytes);
  list_init(&form_bytes);

  PUSH_BYTE_1(res, kOpPushLexicalEnv);

  LishpCons *cons = AS_OBJECT(LishpCons, args);
  LishpForm cdr;
  do {
    LishpForm car = cons->car;

    cdr = cons->cdr;

    if (IS_OBJECT_TYPE(car, kCons)) {
      // car is a cons, so analyze the form
      TEST_CALL_LABEL(cleanup, analyze_form(&form_bytes, car));
    } else {
      // car is an atom, so tag it

      // TODO: figure out what I want to do about popping things off the stack,
      // because we could have just pushed something, and then jumped to some
      // instruction that was expecting nothing on the stack (I think)

      uint32_t instruction_ind = form_bytes.size;
      PUSH_BYTE_2_TARGET(&tag_bytes, kOpPush, car);
      PUSH_BYTE_2_INDEX(&tag_bytes, kOpBindTag, instruction_ind);
    }

    if (!(NIL_P(cdr) || IS_OBJECT_TYPE(cdr, kCons))) {
      assert(0 && "Cannot handle dotted pair in tagbody!");
    }

    cons = AS_OBJECT(LishpCons, cdr);
  } while (!NIL_P(cdr));

  uint32_t bump_count = tag_bytes.size + res->size;
  increment_indices_by(&tag_bytes, bump_count);

  list_append(res, sizeof(Bytecode), &tag_bytes);
  list_append(res, sizeof(Bytecode), &form_bytes);

  // return NIL in tagbody?
  PUSH_BYTE_2_TARGET(res, kOpPush, NIL);
  PUSH_BYTE_1(res, kOpRetForm);
  PUSH_BYTE_1(res, kOpPopLexicalEnv);

cleanup:
  list_clear(&tag_bytes);
  list_clear(&form_bytes);

  return 0;
}

static int analyze_progn(List *res, LishpForm args) {
  while (!NIL_P(args)) {
    assert(IS_OBJECT_TYPE(args, kCons) && "Unexpected dotted pair in PROGN");

    LishpCons *form_args = AS_OBJECT(LishpCons, args);
    LishpForm form = form_args->car;
    args = form_args->cdr;

    TEST_CALL(analyze_form(res, form));
  }
  return 0;
}

static uint32_t analyze_let_star_bindings(List *res, LishpForm vars) {
  uint32_t bindings_added = 0;

  while (!NIL_P(vars)) {
    assert(IS_OBJECT_TYPE(vars, kCons) &&
           "Unexpected dotted pair in LET* bindings");

    LishpCons *var_vars = AS_OBJECT(LishpCons, vars);
    LishpForm var = var_vars->car;
    vars = var_vars->cdr;

    assert(IS_OBJECT_TYPE(var, kCons) &&
           "Unexpected dotted pair in LET* bindings");

    LishpCons *name_value = AS_OBJECT(LishpCons, var);

    LishpForm name = name_value->car;
    LishpForm value = name_value->cdr;

    if (!NIL_P(value)) {
      assert(IS_OBJECT_TYPE(value, kCons) &&
             "Unexpected dotted pair in LET* bindings");

      LishpCons *value_nil = AS_OBJECT(LishpCons, value);
      value = value_nil->car;
    }

    PUSH_BYTE_1(res, kOpPushLexicalEnv);
    PUSH_BYTE_2_TARGET(res, kOpPush, name);
    int analyze_response = analyze_form_rec(res, value, 0);
    PUSH_BYTE_1(res, kOpBindValue);
    PUSH_BYTE_1(res, kOpPop);

    ++bindings_added;
  }

  return bindings_added;
}

static int analyze_let_star(List *res, LishpForm args) {
  if (NIL_P(args)) {
    // TODO: error message
    assert(0 && "Apparently this is an error?");
    return -1;
  }

  assert(IS_OBJECT_TYPE(args, kCons) && "Cannot handle dotted pair in LET*");
  LishpCons *vars_body = AS_OBJECT(LishpCons, args);

  LishpForm vars = vars_body->car;
  LishpForm body = vars_body->cdr;

  uint32_t bindings_count = analyze_let_star_bindings(res, vars);

  int analyze_response = analyze_progn(res, body);

  while (bindings_count > 0) {
    PUSH_BYTE_1(res, kOpPopLexicalEnv);
    --bindings_count;
  }

  return 0;
}

static int analyze_special_form(List *res, SpecialForm sf, LishpForm args) {
  switch (sf) {
  case kSpGo: {
    assert(IS_OBJECT_TYPE(args, kCons) && "Expected cons in go");
    LishpForm car = AS_OBJECT(LishpCons, args)->car;
    PUSH_BYTE_2_TARGET(res, kOpGoReturn, car);
    return 0;
  } break;
  case kSpLetStar: {
    return analyze_let_star(res, args);
  } break;
  case kSpProgn: {
    return analyze_progn(res, args);
  } break;
  case kSpQuote: {
    assert(IS_OBJECT_TYPE(args, kCons) && "Expected cons in quote");
    LishpForm car = AS_OBJECT(LishpCons, args)->car;
    PUSH_BYTE_2_TARGET(res, kOpPush, car);
    PUSH_BYTE_1(res, kOpRetForm);
    return 0;
  } break;
  case kSpTagbody: {
    return analyze_tagbody(res, args);
  } break;
  case kSpNone:
    assert(0 && "Unreachable");
  }

  return -1;
}

static int push_args(List *res, LishpForm args, uint32_t *arg_count) {
  *arg_count = 0;
  while (!NIL_P(args)) {
    assert(IS_OBJECT_TYPE(args, kCons));

    LishpCons *cur = AS_OBJECT(LishpCons, args);
    PUSH_BYTE_2_TARGET(res, kOpPush, cur->car);
    ++(*arg_count);

    args = cur->cdr;
  }
  return 0;
}

static int analyze_cons(List *res, LishpCons *cons, int with_ret) {
  LishpForm car = cons->car;
  if (car.type != kObject) {
    return -1;
  }

  LishpObject *object = car.object;
  switch (object->type) {
  case kCons: {
    assert(0 && "Unimplemented!");
  } break;
  case kSymbol: {
    LishpSymbol *sym = AS(LishpSymbol, object);
    SpecialForm sf = is_special_form(sym);

    if (sf != kSpNone) {
      return analyze_special_form(res, sf, cons->cdr);
    }

    PUSH_BYTE_2_TARGET(res, kOpPush, car);
    PUSH_BYTE_1(res, kOpLookupFunction);

    uint32_t arg_count;
    int push_response = push_args(res, cons->cdr, &arg_count);

    PUSH_BYTE_2_ARG_COUNT(res, kOpFuncall, arg_count);
  } break;
  default: {
    return -1;
  } break;
  }

  if (with_ret) {
    PUSH_BYTE_1(res, kOpRetForm);
  }
  return 0;
}

static int analyze_object(List *res, LishpObject *object, int with_ret) {
  switch (object->type) {
  case kCons: {
    return analyze_cons(res, AS(LishpCons, object), with_ret);
  } break;
  case kSymbol: {
    PUSH_BYTE_2_TARGET(res, kOpPush, FROM_OBJ(object));
    PUSH_BYTE_1(res, kOpLookupSymbol);
  } break;
  case kStream:
  case kString:
  case kFunction:
  case kReadtable: {
    PUSH_BYTE_2_TARGET(res, kOpPush, FROM_OBJ(object));
  }
  }

  if (with_ret) {
    PUSH_BYTE_1(res, kOpRetForm);
  }
  return 0;
}

static int analyze_form_rec(List *res, LishpForm form, int with_ret) {
  switch (form.type) {
  case kT:
  case kNil:
  case kChar:
  case kFixnum: {
    PUSH_BYTE_2_TARGET(res, kOpPush, form);
  } break;
  case kObject: {
    return analyze_object(res, form.object, with_ret);
  } break;
  }

  if (with_ret) {
    PUSH_BYTE_1(res, kOpRetForm);
  }
  return 0;
}

static int analyze_form(List *res, LishpForm form) {
  return analyze_form_rec(res, form, 1);
}

static void set_last_return(Interpreter *interpreter, LishpForm result) {
  LishpForm *pform = NULL;
  list_ref_last(&interpreter->last_return_value, sizeof(LishpForm),
                (void **)&pform);

  *pform = result;
}

static int _form_cmp(void *l, void *r) {
  LishpForm *lptr = l;
  LishpForm *rptr = r;

  return form_cmp(*lptr, *rptr);
}

static int check_valid_tag(Interpreter *interpreter, LishpForm target) {
  Frame *cur_frame;
  get_top_frame_ref(interpreter, &cur_frame);

  // NOTE: we are checking up the call stack, not the environment stack. that
  // way you can't jump to a tag binding that no longer exists

  do {
    Environment *env = cur_frame->env;

    OrderedMap *form_to_index;
    int binding_ret =
        map_ref(&interpreter->environment_bindings, sizeof(Environment *),
                sizeof(OrderedMap), &env, (void **)&form_to_index);

    if (binding_ret == 0) {
      // there is a map of bindings for this environment...
      int form_ret = map_ref(form_to_index, sizeof(LishpForm), sizeof(uint32_t),
                             &target, NULL);
      if (form_ret == 0) {
        // we have the target in the bindings somewhere up the stream
        return 1;
      }
    }

    cur_frame = cur_frame->prev;
  } while (cur_frame != NULL);

  return 0;
}

static int interpret_byte(Interpreter *interpreter, Bytecode *byte_v) {
  Bytecode *byte = byte_v;

  Runtime *rt = interpreter->rt;

  switch (byte->op) {
  case kOpNop: {
    // NOP
  } break;
  case kOpPush: {
    LishpForm *target_ptr = &byte->target;
    list_push(&interpreter->form_stack, sizeof(LishpForm), target_ptr);
  } break;
  case kOpPop: {
    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
  } break;
  case kOpRetForm: {
    LishpForm result;
    list_pop(&interpreter->form_stack, sizeof(LishpForm), &result);

    set_last_return(interpreter, result);
  } break;
  case kOpBindTag: {
    LishpForm *ptag_form;
    list_ref_last(&interpreter->form_stack, sizeof(LishpForm),
                  (void **)&ptag_form);

    uint32_t index = byte->index;

    Environment *cur_env = get_current_environment(interpreter);
    OrderedMap *form_to_index;

    int ref_res =
        map_ref(&interpreter->environment_bindings, sizeof(Environment *),
                sizeof(OrderedMap), &cur_env, (void **)&form_to_index);

    if (ref_res < 0) {
      // this environment has no bindings, so create a new map...
      OrderedMap new_map;
      map_init(&new_map, _form_cmp);

      // ... insert the binding...
      map_insert(&new_map, sizeof(LishpForm), sizeof(uint32_t), ptag_form,
                 &index);

      // ... then put that new map in the environment bindings map
      map_insert(&interpreter->environment_bindings, sizeof(Environment *),
                 sizeof(OrderedMap), &cur_env, &new_map);
    } else {
      // we already had some bindings, so add them to the existing map
      map_insert(form_to_index, sizeof(LishpForm), sizeof(uint32_t), ptag_form,
                 &index);
    }

    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
  } break;
  case kOpBindValue: {
    LishpForm *pvalue_form;
    LishpForm *pname_form;

    uint32_t stack_size = interpreter->form_stack.size;
    assert(stack_size > 1 && "Stack does not have the right size!");

    list_ref(&interpreter->form_stack, sizeof(LishpForm), stack_size - 1,
             (void **)&pvalue_form);
    list_ref(&interpreter->form_stack, sizeof(LishpForm), stack_size - 2,
             (void **)&pname_form);

    assert(IS_OBJECT_TYPE(*pname_form, kSymbol) &&
           "Cannot bind non-symbol value");

    LishpSymbol *sym = AS_OBJECT(LishpSymbol, *pname_form);

    Environment *cur_env = get_current_environment(interpreter);
    bind_value(cur_env, sym, *pvalue_form);

    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
    list_push(&interpreter->form_stack, sizeof(LishpForm), pvalue_form);
  } break;
  case kOpPushLexicalEnv: {
    TEST_CALL(push_environment(interpreter, kSourceBytes));
  } break;
  case kOpPopLexicalEnv: {
    pop_environment(interpreter);
  } break;
  case kOpFuncall: {
    LishpForm *fn_form_ptr = NULL;

    uint32_t stack_size = interpreter->form_stack.size;
    assert(stack_size >= 1 + byte->arg_count &&
           "Stack does not have the right size!");

    list_ref(&interpreter->form_stack, sizeof(LishpForm),
             stack_size - (1 + byte->arg_count), (void **)&fn_form_ptr);

    assert(IS_OBJECT_TYPE(*fn_form_ptr, kFunction) &&
           "Cannot call non-function form!");

    LishpForm funcall_result;
    LishpFunction *fn = AS_OBJECT(LishpFunction, *fn_form_ptr);

    switch (fn->type) {
    case kInherent: {
      // interpret function call pops the args and the function
      LishpFunctionReturn ret_val =
          interpret_function_call(interpreter, byte->arg_count);

      assert(ret_val.return_count <= 1);
      funcall_result = ret_val.first_return;
    } break;
    case kUserDefined: {
      assert(0 && "Unimplemented!");

      // TODO: when this gets implemented, remember to pop the args and the
      // function form
      //
      // for (uint32_t arg_i = 0; arg_i < byte->arg_count; ++arg_i) {
      //   list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
      // }
      // list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
    } break;
    }

    list_push(&interpreter->form_stack, sizeof(LishpForm), &funcall_result);
  } break;
  case kOpLookupSymbol: {
    LishpForm *form_ptr = NULL;

    list_ref_last(&interpreter->form_stack, sizeof(LishpForm),
                  (void **)&form_ptr);

    assert(IS_OBJECT_TYPE(*form_ptr, kSymbol) &&
           "Cannot lookup form that isn't a symbol!");

    LishpSymbol *sym = AS_OBJECT(LishpSymbol, *form_ptr);

    Frame *frame_ptr = NULL;
    list_ref_last(&interpreter->frame_stack, sizeof(Frame),
                  (void **)&frame_ptr);

    LishpForm form_val = symbol_value(rt, frame_ptr->env, sym);

    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
    list_push(&interpreter->form_stack, sizeof(LishpForm), &form_val);
  } break;
  case kOpLookupFunction: {
    LishpForm *form_ptr = NULL;

    list_ref_last(&interpreter->form_stack, sizeof(LishpForm),
                  (void **)&form_ptr);

    assert(IS_OBJECT_TYPE(*form_ptr, kSymbol) &&
           "Cannot lookup form that isn't a symbol!");

    LishpSymbol *sym = AS_OBJECT(LishpSymbol, *form_ptr);

    Frame *frame_ptr = NULL;
    list_ref_last(&interpreter->frame_stack, sizeof(Frame),
                  (void **)&frame_ptr);

    LishpFunction *func_val = symbol_function(rt, frame_ptr->env, sym);
    LishpForm func_form = FROM_OBJ(func_val);

    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
    list_push(&interpreter->form_stack, sizeof(LishpForm), &func_form);
  } break;
  case kOpGoReturn: {
    int is_valid = check_valid_tag(interpreter, byte->target);
    if (!is_valid) {
      // TODO: when restarts and other stuff gets built, add it in here.
      assert(0 && "Tag is unreachable!");
    }
    list_push(&interpreter->form_stack, sizeof(LishpForm), &byte->target);
    return 1;
  } break;
  }
  return 0;
}

typedef struct {
  int return_result;
  union {
    LishpForm result;
    uint32_t index;
  };
} GoResultHandleResponse;

static void handle_go_result(Interpreter *interpreter,
                             GoResultHandleResponse *response) {

  LishpForm target;
  list_pop(&interpreter->form_stack, sizeof(LishpForm), &target);

  Frame *cur_frame;
  get_top_frame_ref(interpreter, &cur_frame);

  int decision_made = 0;
  while (!decision_made && cur_frame != NULL) {
    Environment *cur_env = cur_frame->env;

    OrderedMap *form_to_bytecode_offset;
    if (map_ref(&interpreter->environment_bindings, sizeof(Environment *),
                sizeof(OrderedMap), &cur_env,
                (void **)&form_to_bytecode_offset) < 0) {

      // there were no bindings for this environment, so it should be a
      // binding higher up, either return or continue up the environment chain

      if (cur_frame->source != kSourceBytes) {
        // current environment is not from the bytecode loop, so thread
        // back up the call stack
        response->result = target;
        response->return_result = 1;
        return;
      } else {
        // environment is from bytecode loop, so pop and environment and
        // try again
        pop_environment(interpreter);
        cur_frame = cur_frame->prev;
        continue;
      }
    }

    // there were bindings, so check them now

    uint32_t go_index;
    if (map_get(form_to_bytecode_offset, sizeof(LishpForm), sizeof(uint32_t),
                &target, &go_index) < 0) {

      // form not found in the current environment bindings, so check in an
      // earlier one

      if (cur_frame->source != kSourceBytes) {
        // current environment is not from the bytecode loop, so thread
        // back up the call stack
        response->result = target;
        response->return_result = 1;
        return;
      } else {
        // environment is from bytecode loop, so pop and environment and
        // try again
        pop_environment(interpreter);
        cur_frame = cur_frame->prev;
        continue;
      }
    }

    // we found the binding, so set the index and return

    response->index = go_index;
    response->return_result = 0;
    return;
  }
}

static LishpFunctionReturn interpret_bytes(Interpreter *interpreter,
                                           List bytes) {

  uint32_t index = 0;
  while (index < bytes.size) {
    Bytecode *byte;
    list_ref(&bytes, sizeof(Bytecode), index, (void **)&byte);
    int foreach_res = interpret_byte(interpreter, byte);

    if (foreach_res != 0) {
      if (foreach_res == 1) {
        // This was a go
        GoResultHandleResponse handle_response;
        handle_go_result(interpreter, &handle_response);

        if (handle_response.return_result) {
          return GO_RETURN(handle_response.result);
        } else {
          // if we aren't returning the result, we should set the index and not
          // increment at the bottom of this
          index = handle_response.index;
          continue;
        }
      } else {
        assert(0 && "Unhandled return code");
      }
    }
    ++index;
  }

  LishpForm result;
  list_get_last(&interpreter->last_return_value, sizeof(LishpForm), &result);

  return SINGLE_RETURN(result);
}

static int ptr_diff(void *l, void *r) {
  char **lptr = l;
  char **rptr = r;

  return *lptr - *rptr;
}

int initialize_interpreter(Interpreter **pinterpreter, Runtime *rt,
                           Environment *initial_env) {

  Interpreter *interpreter = ALLOCATE_OBJ(Interpreter, rt);
  if (interpreter == NULL) {
    return -1;
  }
  *pinterpreter = interpreter;

  interpreter->rt = rt;

  list_init(&interpreter->last_return_value);
  list_init(&interpreter->form_stack);
  list_init(&interpreter->frame_stack);
  map_init(&interpreter->function_environments, ptr_diff);
  map_init(&interpreter->environment_bindings, ptr_diff);

  Frame first =
      (Frame){.env = initial_env, .source = kSourceBase, .prev = NULL};
  list_push(&interpreter->frame_stack, sizeof(Frame), &first);

  LishpForm nil = NIL;
  list_push(&interpreter->last_return_value, sizeof(LishpForm), &nil);

  return 0;
}

int cleanup_interpreter(Interpreter **interpreter) {
  list_clear(&(*interpreter)->form_stack);
  list_clear(&(*interpreter)->frame_stack);
  return 0;
}

static int form_mark_used_it(void *arg, void *obj) {
  Runtime *rt = arg;
  LishpForm *form = obj;

  FORM_MARK_USED(rt, *form);

  return 0;
}

static void list_of_forms_mark_used(Runtime *rt, List *list) {
  list_foreach(list, sizeof(LishpForm), form_mark_used_it, rt);
}

static int frame_mark_used_it(void *arg, void *obj) {
  Runtime *rt = arg;
  Frame *frame = obj;

  environment_mark_used(rt, frame->env);

  return 0;
}

static int func_envs_mark_used_it(void *arg, void *key, void *val) {
  Runtime *rt = arg;
  LishpFunction **fn = key;
  Environment **env = val;

  OBJ_MARK_USED(rt, *fn);
  environment_mark_used(rt, *env);

  return 0;
}

static int binding_map_mark_used_it(void *arg, void *key, void *val) {
  (void)val;

  Runtime *rt = arg;
  LishpForm *form = key;

  FORM_MARK_USED(rt, *form);

  return 0;
}

static int env_bindings_mark_used_it(void *arg, void *key, void *val) {
  Runtime *rt = arg;
  Environment **env = key;
  OrderedMap *binding_map = val;

  environment_mark_used(rt, *env);
  map_foreach(binding_map, sizeof(LishpForm), sizeof(uint32_t),
              binding_map_mark_used_it, arg);

  return 0;
}

void interpreter_mark_used_objs(Interpreter *interpreter) {
  Runtime *rt = interpreter->rt;

  other_mark_used(rt, interpreter);

  list_of_forms_mark_used(rt, &interpreter->form_stack);
  list_of_forms_mark_used(rt, &interpreter->last_return_value);
  list_foreach(&interpreter->frame_stack, sizeof(Frame), frame_mark_used_it,
               rt);

  map_foreach(&interpreter->function_environments, sizeof(LishpFunction *),
              sizeof(Environment *), func_envs_mark_used_it, rt);

  map_foreach(&interpreter->environment_bindings, sizeof(Environment *),
              sizeof(OrderedMap), env_bindings_mark_used_it, rt);
}

LishpFunctionReturn interpret(Interpreter *interpreter, LishpForm form) {
  List bytes;
  list_init(&bytes);

  int analyze_res = analyze_form(&bytes, form);
  if (analyze_res < 0) {
    assert(0 && "Error while analyzing form");
  }

  LishpFunctionReturn result = interpret_bytes(interpreter, bytes);

  list_clear(&bytes);

  return result;
}

int push_function(Interpreter *interpreter, LishpFunction *fn) {
  LishpForm fn_form = FROM_OBJ(fn);
  return list_push(&interpreter->form_stack, sizeof(LishpForm), &fn_form);
}

int push_argument(Interpreter *interpreter, LishpForm form) {
  return list_push(&interpreter->form_stack, sizeof(LishpForm), &form);
}

LishpFunctionReturn interpret_function_call(Interpreter *interpreter,
                                            uint32_t arg_count) {

  Runtime *rt = interpreter->rt;
  uint32_t fn_index = interpreter->form_stack.size - (1 + arg_count);

  int push_env_result = push_environment(interpreter, kSourceFuncall);

  LishpForm *fn_form;
  list_ref(&interpreter->form_stack, sizeof(LishpForm), fn_index,
           (void **)&fn_form);

  LishpFunction *fn = AS_OBJECT(LishpFunction, *fn_form);

  List bytes;
  list_init(&bytes);

  LishpForm evaled_args_form = NIL;
  list_push(&interpreter->form_stack, sizeof(LishpForm), &evaled_args_form);

  LishpCons **cur_cons = NULL;
  uint32_t pevaled_index = interpreter->form_stack.size - 1;

  for (uint32_t arg_i = 0; arg_i < arg_count; ++arg_i) {
    LishpForm *pform;
    list_ref(&interpreter->form_stack, sizeof(LishpForm), fn_index + 1 + arg_i,
             (void **)&pform);

    int analyze_res = analyze_form(&bytes, *pform);
    if (analyze_res < 0) {
      assert(0 && "Error while analyzing form");
    }

    LishpFunctionReturn arg_val = interpret_bytes(interpreter, bytes);
    list_clear(&bytes);

    CHECK_GO_RET(arg_val);

    // cur_pevaled is the pointer to the form that is about to be turned from
    // NIL to a Cons

    // cur_cons _will_ get assigned to be a pointer to the address of the newly
    // created cons, so coming in, it's pointer to the address of the
    // _previously_ allocated cons

    LishpForm *cur_pevaled;
    if (cur_cons == NULL) {
      // first iteration, get the form from the stack
      list_ref(&interpreter->form_stack, sizeof(LishpForm), pevaled_index,
               (void **)&cur_pevaled);

      // get a valid reference to the object field of the form on the stack
      cur_cons = (LishpCons **)&cur_pevaled->object;
    } else {
      cur_pevaled = &(*cur_cons)->cdr;
    }

    LishpCons *new_alloc = ALLOCATE_OBJ(LishpCons, rt);

    // change it from NIL to an object
    *cur_pevaled = FROM_OBJ(NULL);

    // get the pointer of the address of the cons to be created
    cur_cons = (LishpCons **)&cur_pevaled->object;
    // set the address of the cons to be what comes from the allocator
    *cur_cons = new_alloc;
    // fill in the cons fields
    **cur_cons = CONS(arg_val.first_return, NIL);
  }

  LishpList arg_list = NIL_LIST;
  if (arg_count != 0) {
    LishpForm *pevaled;
    list_ref(&interpreter->form_stack, sizeof(LishpForm), pevaled_index,
             (void **)&pevaled);

    LishpCons *arg_cons = AS_OBJECT(LishpCons, *pevaled);
    arg_list = LIST_OF(arg_cons);
  }

  LishpFunctionReturn result = fn->inherent_fn(interpreter, arg_list);
  set_last_return(interpreter, result.first_return);

  // pop off the evaled_args_form
  list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
  // pop the arguments
  for (uint32_t arg_i = 0; arg_i < arg_count; ++arg_i) {
    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
  }
  // pop the function form
  list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);

  pop_environment(interpreter);

  return result;
}

int push_form_return(Interpreter *interpreter, LishpForm **pform) {
  LishpForm nil_form = NIL;

  TEST_CALL(list_push(&interpreter->form_stack, sizeof(LishpForm), &nil_form));
  TEST_CALL(list_ref_last(&interpreter->form_stack, sizeof(LishpForm),
                          (void **)pform));

  return 0;
}

int pop_form_return(Interpreter *interpreter, LishpForm *result) {
  TEST_CALL(list_pop(&interpreter->form_stack, sizeof(LishpForm), result));
  return 0;
}

int bind_symbol_value(Interpreter *interpreter, LishpSymbol *sym,
                      LishpForm value) {

  Environment *env = get_current_environment(interpreter);
  bind_value(env, sym, value);
  // TODO: make this function void, or make bind return an int?
  return 0;
}

Runtime *get_runtime(Interpreter *interpreter) { return interpreter->rt; }

Environment *get_current_environment(Interpreter *interpreter) {
  Frame *ptop_frame;
  get_top_frame_ref(interpreter, &ptop_frame);

  Environment *cur_env = ptop_frame->env;
  return cur_env;
}

static int push_environment(Interpreter *interpreter, FrameSource source) {
  Frame *ptop_frame;
  get_top_frame_ref(interpreter, &ptop_frame);

  Environment *new_env = allocate_env(interpreter->rt, ptop_frame->env);
  if (new_env == NULL) {
    return -1;
  }

  Frame new_frame = (Frame){
      .env = new_env,
      .source = source,
      .prev = ptop_frame,
  };

  LishpForm nil = NIL;
  TEST_CALL_LABEL(cleanup, list_push(&interpreter->last_return_value,
                                     sizeof(LishpForm), &nil));

  TEST_CALL_LABEL(
      cleanup, list_push(&interpreter->frame_stack, sizeof(Frame), &new_frame));

  return 0;

cleanup:
  // TODO: deallocate environment
  return -1;
}

static void pop_environment(Interpreter *interpreter) {
  LishpForm last_return;
  list_pop(&interpreter->last_return_value, sizeof(LishpForm), &last_return);
  list_pop(&interpreter->frame_stack, sizeof(Frame), NULL);

  // TODO: verify that every time I pop a lexical environment I want to be
  // setting the previous last return to the return of the popped environment...
  LishpForm *prev_last_return;
  list_ref_last(&interpreter->last_return_value, sizeof(LishpForm),
                (void **)&prev_last_return);

  *prev_last_return = last_return;
}
