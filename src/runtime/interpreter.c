#include <assert.h>
#include <stdlib.h>

#include "common.h"
#include "memory_manager.h"
#include "runtime/interpreter.h"

typedef enum {
  kOpNop,
  kOpPush,
  kOpPop,
  kOpRetForm,
  kOpBindTag,
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
  };
} Bytecode;

typedef struct frame {
  Environment *env;
  struct frame *prev;
} Frame;

typedef struct {
  int32_t push_count;
} AnalyzeState;

struct interpreter {
  Runtime *rt;
  List last_return_value;
  List form_stack;
  List frame_stack;

  AnalyzeState analyze_state;

  OrderedMap function_environments; // LishpFunction * -> Environment *
  OrderedMap environment_bindings;  // Environment * -> LishpForm -> uint32_t
};

#define FORM_COUNT (sizeof(special_forms) / sizeof(special_forms[0]))

typedef enum {
  kSpNone,
  kSpGo,
  kSpTagbody,
} SpecialForm;

const char *special_forms[] = {
    // NOTE: make sure this is alphabetic
    [kSpGo] = "GO",
    [kSpTagbody] = "TAGBODY",
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

static int push_environment(Interpreter *interpreter);
static int pop_environment(Interpreter *interpreter);

static int incrementer(void *arg, void *obj) {
  uint32_t *pinc = arg;
  Bytecode *pbyte = obj;

  if (pbyte->op == kOpBindTag) {
    pbyte->index += *pinc;
  }
  return 0;
}

static int increment_indices_by(List *list, uint32_t inc) {
  list_foreach(list, sizeof(Bytecode), incrementer, &inc);
  return 0;
}

#define PUSH_BYTE_1(state, list, opcode)                                       \
  do {                                                                         \
    Bytecode byte = (Bytecode){.op = (opcode)};                                \
    list_push((list), sizeof(Bytecode), &byte);                                \
    (state)->push_count += net_effects[opcode];                                \
  } while (0)

#define PUSH_BYTE_2(state, list, opcode, f, t)                                 \
  do {                                                                         \
    Bytecode byte = (Bytecode){.op = (opcode), {.f = (t)}};                    \
    list_push((list), sizeof(Bytecode), &byte);                                \
    (state)->push_count += net_effects[opcode];                                \
  } while (0)
#define PUSH_BYTE_2_TARGET(state, list, opcode, t)                             \
  PUSH_BYTE_2(state, list, opcode, target, t)
#define PUSH_BYTE_2_INDEX(state, list, opcode, t)                              \
  PUSH_BYTE_2(state, list, opcode, index, t)

static int analyze_form(AnalyzeState *state, List *res, LishpForm form);

static int analyze_tagbody(AnalyzeState *state, List *res, LishpForm args) {
  if (NIL_P(args)) {
    PUSH_BYTE_2_TARGET(state, res, kOpPush, NIL);
    return 0;
  }

  assert(IS_OBJECT_TYPE(args, kCons) && "Cannot handle dotted pair in tagbody");

  List tag_bytes;
  List form_bytes;
  list_init(&tag_bytes);
  list_init(&form_bytes);

  PUSH_BYTE_1(state, res, kOpPushLexicalEnv);

  LishpCons *cons = AS_OBJECT(LishpCons, args);
  LishpForm cdr;
  do {
    LishpForm car = cons->car;

    cdr = cons->cdr;

    if (IS_OBJECT_TYPE(car, kCons)) {
      // car is a cons, so analyze the form
      TEST_CALL_LABEL(cleanup, analyze_form(state, &form_bytes, car));
    } else {
      // car is an atom, so tag it

      // TODO: figure out what I want to do about popping things off the stack,
      // because we could have just pushed something, and then jumped to some
      // instruction that was expecting nothing on the stack (I think)

      uint32_t instruction_ind = form_bytes.size;
      PUSH_BYTE_2_TARGET(state, &tag_bytes, kOpPush, car);
      PUSH_BYTE_2_INDEX(state, &tag_bytes, kOpBindTag, instruction_ind);
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
  PUSH_BYTE_2_TARGET(state, res, kOpPush, NIL);
  PUSH_BYTE_1(state, res, kOpRetForm);
  PUSH_BYTE_1(state, res, kOpPopLexicalEnv);

cleanup:
  list_clear(&tag_bytes);
  list_clear(&form_bytes);

  return 0;
}

static int analyze_special_form(AnalyzeState *state, List *res, SpecialForm sf,
                                LishpForm args) {
  switch (sf) {
  case kSpGo: {
    assert(IS_OBJECT_TYPE(args, kCons) && "Expected cons in go");
    LishpForm car = AS_OBJECT(LishpCons, args)->car;
    PUSH_BYTE_2_TARGET(state, res, kOpPush, car);
    PUSH_BYTE_1(state, res, kOpGoReturn);
    return 0;
  } break;
  case kSpTagbody: {
    return analyze_tagbody(state, res, args);
  } break;
  default:
    assert(0 && "Unreachable");
  }

  return -1;
}

static int analyze_cons(AnalyzeState *state, List *res, LishpCons *cons) {
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
    // TODO: check if this is a special form!
    LishpSymbol *sym = AS(LishpSymbol, object);
    SpecialForm sf = is_special_form(sym);

    if (sf != kSpNone) {
      return analyze_special_form(state, res, sf, cons->cdr);
    }

    PUSH_BYTE_2_TARGET(state, res, kOpPush, car);
    PUSH_BYTE_1(state, res, kOpLookupFunction);
    PUSH_BYTE_2_TARGET(state, res, kOpPush, cons->cdr);
    PUSH_BYTE_1(state, res, kOpFuncall);
  } break;
  default: {
    return -1;
  } break;
  }

  PUSH_BYTE_1(state, res, kOpRetForm);
  return 0;
}

static int analyze_object(AnalyzeState *state, List *res, LishpObject *object) {
  switch (object->type) {
  case kCons: {
    return analyze_cons(state, res, AS(LishpCons, object));
  } break;
  case kSymbol: {
    PUSH_BYTE_2_TARGET(state, res, kOpPush, FROM_OBJ(object));
    PUSH_BYTE_1(state, res, kOpLookupSymbol);
  } break;
  case kStream:
  case kString:
  case kFunction:
  case kReadtable: {
    PUSH_BYTE_2_TARGET(state, res, kOpPush, FROM_OBJ(object));
  }
  }

  PUSH_BYTE_1(state, res, kOpRetForm);
  return 0;
}

static int analyze_form(AnalyzeState *state, List *res, LishpForm form) {
  switch (form.type) {
  case kT:
  case kNil:
  case kChar:
  case kFixnum: {
    PUSH_BYTE_2_TARGET(state, res, kOpPush, form);
  } break;
  case kObject: {
    return analyze_object(state, res, form.object);
  } break;
  }

  PUSH_BYTE_1(state, res, kOpRetForm);
  return 0;
}

static int set_last_return(Interpreter *interpreter, LishpForm result) {
  LishpForm *pform = NULL;
  if (list_ref_last(&interpreter->last_return_value, sizeof(LishpForm),
                    (void **)&pform) < 0) {
    return -1;
  }

  *pform = result;
  return 0;
}

static int _form_cmp(void *l, void *r) {
  LishpForm *lptr = l;
  LishpForm *rptr = r;

  return form_cmp(*lptr, *rptr);
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
      OrderedMap new_map;
      map_init(&new_map, _form_cmp);
      map_insert(&interpreter->environment_bindings, sizeof(Environment *),
                 sizeof(OrderedMap), &cur_env, &new_map);

      // This is a bit inefficient, but it's just checking pointer diffs, so
      // it's probably not that bad.
      map_ref(&interpreter->environment_bindings, sizeof(Environment *),
              sizeof(OrderedMap), &cur_env, (void **)&form_to_index);
    }

    map_insert(form_to_index, sizeof(LishpForm), sizeof(uint32_t), ptag_form,
               &index);

    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
  } break;
  case kOpPushLexicalEnv: {
    push_environment(interpreter);
  } break;
  case kOpPopLexicalEnv: {
    pop_environment(interpreter);
  } break;
  case kOpFuncall: {
    LishpForm *args_ptr = NULL;
    LishpForm *form_ptr = NULL;

    uint32_t stack_size = interpreter->form_stack.size;
    assert(stack_size > 1 && "Stack does not have the right size!");

    list_ref(&interpreter->form_stack, sizeof(LishpForm), stack_size - 1,
             (void **)&args_ptr);
    list_ref(&interpreter->form_stack, sizeof(LishpForm), stack_size - 2,
             (void **)&form_ptr);

    assert(IS_OBJECT_TYPE(*form_ptr, kFunction) &&
           "Cannot call non-function form!");

    LishpForm funcall_result;
    LishpFunction *fn = AS_OBJECT(LishpFunction, *form_ptr);

    LishpList args_list = NIL_LIST;
    if (args_ptr->type != kNil) {
      assert(IS_OBJECT_TYPE(*args_ptr, kCons) &&
             "Arguments to function call must be a cons");
      args_list = LIST_OF(AS_OBJECT(LishpCons, *args_ptr));
    }

    switch (fn->type) {
    case kInherent: {
      LishpFunctionReturn ret_val =
          interpret_function_call(interpreter, fn, args_list);

      assert(ret_val.return_count <= 1);
      funcall_result = ret_val.first_return;
    } break;
    case kUserDefined: {
      assert(0 && "Unimplemented!");
    } break;
    }

    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);

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
    return 1;
  } break;
  }
  return 0;
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
        // This was a go, so break out of the interpreting
        LishpForm *target;
        list_ref_last(&interpreter->form_stack, sizeof(LishpForm),
                      (void **)&target);

        Environment *cur_env = get_current_environment(interpreter);

        OrderedMap *form_to_bytecode_offset;
        if (map_ref(&interpreter->environment_bindings, sizeof(Environment *),
                    sizeof(OrderedMap), &cur_env,
                    (void **)&form_to_bytecode_offset) == 0) {

          uint32_t go_index;
          if (map_get(form_to_bytecode_offset, sizeof(LishpForm),
                      sizeof(uint32_t), target, &go_index) < 0) {
            // form not found in the current environment bindings, so it's an
            // earlier one, thread back up the call stack
            return GO_RETURN(*target);
          }

          // found the item, so set the correct index
          index = go_index;
          continue; // skip the increment at the bottom of the loop
        } else {
          // there were no bindings for this environment, so it should be a
          // binding higher up
          return GO_RETURN(*target);
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
  void **lptr = l;
  void **rptr = r;

  return *lptr - *rptr;
}

int initialize_interpreter(Interpreter **pinterpreter, Runtime *rt,
                           Environment *initial_env) {

  Interpreter *interpreter = allocate(sizeof(Interpreter));
  if (interpreter == NULL) {
    return -1;
  }
  *pinterpreter = interpreter;

  interpreter->rt = rt;
  interpreter->analyze_state = (AnalyzeState){0};

  list_init(&interpreter->last_return_value);
  list_init(&interpreter->form_stack);
  list_init(&interpreter->frame_stack);
  map_init(&interpreter->function_environments, ptr_diff);
  map_init(&interpreter->environment_bindings, ptr_diff);

  Frame first = (Frame){.env = initial_env, .prev = NULL};
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

LishpFunctionReturn interpret(Interpreter *interpreter, LishpForm form) {
  List bytes;
  list_init(&bytes);

  int analyze_res = analyze_form(&interpreter->analyze_state, &bytes, form);
  if (analyze_res < 0) {
    assert(0 && "Error while analyzing form");
  }

  LishpFunctionReturn result = interpret_bytes(interpreter, bytes);

  list_clear(&bytes);

  return result;
}

LishpFunctionReturn interpret_function_call(Interpreter *interpreter,
                                            LishpFunction *fn, LishpList args) {

  Runtime *rt = interpreter->rt;

  List bytes;
  list_init(&bytes);

  LishpList args_list = NIL_LIST;
  LishpCons *last_cons = NULL;

  if (!args.nil) {
    LishpForm cdr;
    LishpCons *cur_cons = args.cons;
    do {
      LishpForm car = cur_cons->car;

      int analyze_res = analyze_form(&interpreter->analyze_state, &bytes, car);
      if (analyze_res < 0) {
        assert(0 && "Error while analyzing form");
      }

      LishpFunctionReturn arg_val = interpret_bytes(interpreter, bytes);
      list_clear(&bytes);

      CHECK_GO_RET(arg_val);

      LishpCons *next_cons = ALLOCATE_OBJ(LishpCons, rt);
      *next_cons = CONS(arg_val.first_return, NIL);

      if (last_cons != NULL) {
        last_cons->cdr = FROM_OBJ(next_cons);
      } else {
        args_list = LIST_OF(next_cons);
      }

      last_cons = next_cons;

      cdr = cur_cons->cdr;
      assert((NIL_P(cdr) || IS_OBJECT_TYPE(cdr, kCons)) &&
             "Cannot handle dotted pair in function call!");

      cur_cons = AS_OBJECT(LishpCons, cdr);
    } while (!NIL_P(cdr));
  }

  LishpFunctionReturn result = fn->inherent_fn(interpreter, args_list);

  return result;
}

Runtime *get_runtime(Interpreter *interpreter) { return interpreter->rt; }

static int get_top_frame_ref(Interpreter *interpreter, Frame **pframe) {
  Frame *ptop_frame;
  list_ref_last(&interpreter->frame_stack, sizeof(Frame), (void **)&ptop_frame);

  *pframe = ptop_frame;

  return 0;
}

Environment *get_current_environment(Interpreter *interpreter) {
  Frame *ptop_frame;
  get_top_frame_ref(interpreter, &ptop_frame);

  Environment *cur_env = ptop_frame->env;
  return cur_env;
}

static int push_environment(Interpreter *interpreter) {
  Frame *ptop_frame;
  get_top_frame_ref(interpreter, &ptop_frame);

  Environment *new_env = allocate_env(interpreter->rt, ptop_frame->env);
  Frame new_frame = (Frame){
      .env = new_env,
      .prev = ptop_frame,
  };

  LishpForm nil = NIL;
  list_push(&interpreter->last_return_value, sizeof(LishpForm), &nil);
  list_push(&interpreter->frame_stack, sizeof(Frame), &new_frame);

  return 0;
}

static int pop_environment(Interpreter *interpreter) {
  list_pop(&interpreter->last_return_value, sizeof(LishpForm), NULL);
  list_pop(&interpreter->frame_stack, sizeof(Frame), NULL);
  return 0;
}
