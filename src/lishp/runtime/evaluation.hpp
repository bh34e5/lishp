#ifndef evaluation_h
#define evaluation_h

#include "environment.hpp"
#include "types.hpp"

auto IsSpecialForm(types::LishpSymbol *sym) -> bool;

auto BindLambdaForm(environment::Environment *lexical,
                    types::LishpCons *lambda_expr)
    -> types::LishpFunctionReturn;
auto EvalForm(environment::Environment *lexical, types::LishpForm form)
    -> types::LishpFunctionReturn;
auto EvalArgs(environment::Environment *lexical, types::LishpList &args)
    -> types::LishpList;

#endif
