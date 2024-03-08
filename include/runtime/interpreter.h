#ifndef runtime_interpreter_
#define runtime_interpreter_

#include "runtime.h"
#include "runtime/types.h"

int initialize_interpreter(Interpreter **interpreter, Runtime *rt,
                           Environment *initial_env);
int cleanup_interpreter(Interpreter **interpreter);

LishpFunctionReturn interpret(Interpreter *interpreter, LishpForm form);
LishpFunctionReturn interpret_function_call(Interpreter *interpreter,
                                            LishpFunction *fn, LishpList args);
Runtime *get_runtime(Interpreter *interpreter);
Environment *get_current_environment(Interpreter *interpreter);

#endif
