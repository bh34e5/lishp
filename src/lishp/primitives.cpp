#include <iostream>

#include "primitives.hpp"
#include "runtime/exceptions.hpp"

auto cons(LispCons args) -> LispForm {
  LispForm first = args.car;
  LispForm second = args.cdr;

  LispCons *c = new LispCons{{LispObjType::ObjCons}, false, first, second};
  return {LispFormType::FormObj, {.obj = (LispObj *)c}};
}

auto format_(LispCons args) -> LispForm {
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
