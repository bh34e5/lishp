#ifndef lishp_primitives_primitive_helpers_h
#define lishp_primitives_primitive_helpers_h

#include "../lishp.hpp"
#include "../runtime/exceptions.hpp"

auto eval_symbol(LishpRuntime *rt, LispSymbol *sym) -> LispForm;
auto eval_cons(LishpRuntime *rt, LispCons *cons) -> LispForm;
auto eval_form(LishpRuntime *rt, LispForm form) -> LispForm;

#endif
