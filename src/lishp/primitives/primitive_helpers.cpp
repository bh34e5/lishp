#include "primitive_helpers.hpp"

auto eval_symbol(LishpRuntime *rt, LispSymbol *sym) -> LispForm {
  return rt->lookup_symbol(sym);
}

auto eval_cons(LishpRuntime *rt, LispCons *cons) -> LispForm {
  LispForm car = cons->car;
  if (car.type != LispFormType::FormObj) {
    throw RuntimeException("Can't evaluate anything aside from functions");
  }

  LispObj *obj = car.as.obj;
  if (obj->type != LispObjType::ObjSymbol) {
    // FIXME: figure out what this actually does...
    throw RuntimeException("Can't call something that isn't a symbol");
  }

  LispSymbol *sym = (LispSymbol *)obj;
  LispFunction func = rt->lookup_function(sym);

  LispCons rest = LispCons{{LispObjType::ObjCons}, true, {}, {}};
  if (!cons->cdr.is_nil()) {
    rest = *cons->rest();
  }
  LispForm res = func.call(rt, rest);

  return res;
}

auto eval_form(LishpRuntime *rt, LispForm form) -> LispForm {
  switch (form.type) {
  case FormNil:
  case FormT:
  case FormNumber:
    // numbers and nil evaluate to themself
    return form;
  case FormObj: {
    LispObj *obj = form.as.obj;
    switch (obj->type) {
    case ObjString:
    case ObjFunction:
      // function objects and strings evaluate to themself
      return form;

    case ObjSymbol:
      return eval_symbol(rt, (LispSymbol *)obj);
    case ObjCons:
      return eval_cons(rt, (LispCons *)obj);
    }
  }
  }
}
