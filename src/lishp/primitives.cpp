#include <iostream>

#include "lishp.hpp"
#include "primitives.hpp"
#include "runtime/exceptions.hpp"

auto cons(LishpRuntime *rt, LispCons args) -> LispForm {
  LispForm first = args.car;
  LispForm second = args.cdr;

  LispCons *c = new LispCons{{LispObjType::ObjCons}, false, first, second};
  return {LispFormType::FormObj, {.obj = (LispObj *)c}};
}

auto format_(LishpRuntime *rt, LispCons args) -> LispForm {
  // FIXME: for now, we are not dealing with format strings, and also, the only
  // stream that is valid, is `T`, so that it goes to standard out.

  LispForm stream_ = args.car;

  // TODO: add the T object and make the assert that the stream is equal to
  // that...

  LispCons rest = args.rest();

  LispForm formatStr = rest.car;

  if (formatStr.type != LispFormType::FormObj) {
    // TODO: figure out how I want to do exceptions and what kind of information
    // I want to give
    throw RuntimeException("Expected argument of type String");
  }

  LispObj *obj = formatStr.as.obj;

  if (obj->type != LispObjType::ObjString) {
    throw RuntimeException("Expected argument of type String");
  }

  LispString *str = (LispString *)obj;

  std::cout << str->str;

  return LispForm::nil;
}

auto read(LishpRuntime *rt, LispCons args) -> LispForm { return LispForm::nil; }

auto eval(LishpRuntime *rt, LispCons args) -> LispForm {
  // TODO: figure out why I need the double cons...
  LispForm car = args.car;
  if (car.type != LispFormType::FormObj) {
    throw RuntimeException("Expected first arg to be cons");
  }

  LispObj *obj = car.as.obj;
  if (obj->type != LispObjType::ObjCons) {
    throw RuntimeException("Expected first arg to be cons");
  }

  LispCons *actual_args = (LispCons *)obj;

  car = actual_args->car;
  if (car.type != LispFormType::FormObj) {
    throw RuntimeException("Can't evaluate anything aside from functions");
  }

  obj = car.as.obj;
  if (obj->type != LispObjType::ObjSymbol) {
    // FIXME: figure out what this actually does...
    throw RuntimeException("Can't call something that isn't a symbol");
  }

  LispSymbol *sym = (LispSymbol *)obj;
  LispFunction func = rt->lookup_function(sym);

  LispCons rest = actual_args->rest();
  LispForm res = func.call(rt, rest);

  return res;
}
