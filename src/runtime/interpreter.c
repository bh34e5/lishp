#include <assert.h>
#include <stdlib.h>

#include "common.h"
#include "memory_manager.h"
#include "runtime/interpreter.h"

typedef enum {
  kNop,
  kPush,
  kLookupSymbol,
  kLookupFunction,
  kFuncall,
} Opcode;

typedef struct {
  Opcode op;
  union {
    LishpForm target;
  };
} Bytecode;

typedef struct frame {
  Environment *env;
  struct frame *prev;
} Frame;

struct interpreter {
  Runtime *rt;
  List form_stack;
  List frame_stack;
};

#define PUSH_BYTE_1(list, opcode)                                              \
  do {                                                                         \
    Bytecode byte = (Bytecode){.op = (opcode)};                                \
    list_push((list), sizeof(Bytecode), &byte);                                \
  } while (0)

#define PUSH_BYTE_2(list, opcode, t)                                           \
  do {                                                                         \
    Bytecode byte = (Bytecode){.op = (opcode), {.target = (t)}};               \
    list_push((list), sizeof(Bytecode), &byte);                                \
  } while (0)

static int analyze_cons(List *res, LishpCons *cons) {
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

    PUSH_BYTE_2(res, kPush, car);
    PUSH_BYTE_1(res, kLookupFunction);
    PUSH_BYTE_2(res, kPush, cons->cdr);
    PUSH_BYTE_1(res, kFuncall);
  } break;
  default: {
    return -1;
  } break;
  }
  return 0;
}

static int analyze_object(List *res, LishpObject *object) {
  switch (object->type) {
  case kCons: {
    return analyze_cons(res, AS(LishpCons, object));
  } break;
  case kSymbol: {
    PUSH_BYTE_2(res, kPush, FROM_OBJ(object));
    PUSH_BYTE_1(res, kLookupSymbol);
  } break;
  case kStream:
  case kString:
  case kFunction:
  case kReadtable: {
    PUSH_BYTE_2(res, kPush, FROM_OBJ(object));
  }
  }
  return 0;
}

static int analyze_form(List *res, LishpForm form) {
  switch (form.type) {
  case kT:
  case kNil:
  case kChar:
  case kFixnum: {
    PUSH_BYTE_2(res, kPush, form);
  } break;
  case kObject: {
    return analyze_object(res, form.object);
  } break;
  }
  return 0;
}

typedef struct interpret_arg {
  Interpreter *interpreter;
} InterpreterArg;

static int interpret_byte(void *arg_v, void *byte_v) {
  InterpreterArg *arg = arg_v;

  Interpreter *interpreter = arg->interpreter;
  Bytecode *byte = byte_v;

  switch (byte->op) {
  case kNop: {
    // NOP
  } break;
  case kPush: {
    LishpForm *target_ptr = &byte->target;
    list_push(&interpreter->form_stack, sizeof(LishpForm), target_ptr);
  } break;
  case kFuncall: {
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
  case kLookupSymbol: {
    LishpForm *form_ptr = NULL;

    uint32_t form_stack_size = interpreter->form_stack.size;
    list_ref(&interpreter->form_stack, sizeof(LishpForm), form_stack_size - 1,
             (void **)&form_ptr);

    assert(IS_OBJECT_TYPE(*form_ptr, kSymbol) &&
           "Cannot lookup form that isn't a symbol!");

    LishpSymbol *sym = AS_OBJECT(LishpSymbol, *form_ptr);

    Frame *frame_ptr = NULL;
    list_ref(&interpreter->frame_stack, sizeof(Frame),
             interpreter->frame_stack.size - 1, (void **)&frame_ptr);

    LishpForm form_val = symbol_value(frame_ptr->env, sym);

    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
    list_push(&interpreter->form_stack, sizeof(LishpForm), &form_val);
  } break;
  case kLookupFunction: {
    LishpForm *form_ptr = NULL;

    uint32_t form_stack_size = interpreter->form_stack.size;
    list_ref(&interpreter->form_stack, sizeof(LishpForm), form_stack_size - 1,
             (void **)&form_ptr);

    assert(IS_OBJECT_TYPE(*form_ptr, kSymbol) &&
           "Cannot lookup form that isn't a symbol!");

    LishpSymbol *sym = AS_OBJECT(LishpSymbol, *form_ptr);

    Frame *frame_ptr = NULL;
    list_ref(&interpreter->frame_stack, sizeof(Frame),
             interpreter->frame_stack.size - 1, (void **)&frame_ptr);

    LishpFunction *func_val = symbol_function(frame_ptr->env, sym);
    LishpForm func_form = FROM_OBJ(func_val);

    list_pop(&interpreter->form_stack, sizeof(LishpForm), NULL);
    list_push(&interpreter->form_stack, sizeof(LishpForm), &func_form);
  } break;
  }
  return 0;
}

static LishpFunctionReturn interpret_bytes(Interpreter *interpreter,
                                           List bytes) {
  InterpreterArg arg = (InterpreterArg){
      .interpreter = interpreter,
  };

  int foreach_res =
      list_foreach(&bytes, sizeof(Bytecode), interpret_byte, &arg);
  if (foreach_res != 0) {
    // hmm
    assert(0 && "Unimplemented!");
  }

  LishpForm result;
  list_pop(&interpreter->form_stack, sizeof(LishpForm), &result);

  return SINGLE_RETURN(result);
}

int initialize_interpreter(Interpreter **interpreter, Runtime *rt,
                           Environment *initial_env) {
  *interpreter = allocate(sizeof(Interpreter));
  if (*interpreter == NULL) {
    return -1;
  }

  (*interpreter)->rt = rt;
  list_init(&(*interpreter)->form_stack);
  list_init(&(*interpreter)->frame_stack);

  Frame first = (Frame){.env = initial_env, .prev = NULL};
  list_push(&(*interpreter)->frame_stack, sizeof(Frame), &first);

  return 0;
}

static int cleanup_interpreter(Interpreter *interpreter) {
  list_clear(&interpreter->form_stack);
  list_clear(&interpreter->frame_stack);
  return 0;
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

      int analyze_res = analyze_form(&bytes, car);
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

Environment *get_current_environment(Interpreter *interpreter) {
  Frame top_frame;
  list_get(&interpreter->frame_stack, sizeof(Frame),
           interpreter->frame_stack.size - 1, &top_frame);

  Environment *cur_env = top_frame.env;
  return cur_env;
}
