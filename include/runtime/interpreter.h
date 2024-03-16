#ifndef runtime_interpreter_
#define runtime_interpreter_

#include "runtime.h"
#include "runtime/types.h"

int initialize_interpreter(Interpreter **pinterpreter, Runtime *rt,
                           Environment *initial_env);
int cleanup_interpreter(Interpreter **interpreter);

LishpFunctionReturn interpret(Interpreter *interpreter, LishpForm form);

int push_function(Interpreter *interpreter, LishpFunction *fn);
int push_argument(Interpreter *interpreter, LishpForm form);
LishpFunctionReturn interpret_function_call(Interpreter *interpreter,
                                            uint32_t arg_count);

int push_form_return(Interpreter *interpreter, LishpForm **pform);
int pop_form_return(Interpreter *interpreter, LishpForm *result);

Runtime *get_runtime(Interpreter *interpreter);
Environment *get_current_environment(Interpreter *interpreter);

#endif
