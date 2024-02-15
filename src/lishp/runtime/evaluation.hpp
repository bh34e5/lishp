#ifndef evaluation_h
#define evaluation_h

#include "environment.hpp"
#include "types.hpp"

auto EvalForm(environment::Environment *lexical, types::LishpForm form)
    -> types::LishpFunctionReturn;
auto EvalArgs(environment::Environment *lexical, types::LishpList &args)
    -> types::LishpList;

#endif
