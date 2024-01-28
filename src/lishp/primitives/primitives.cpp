#include <iostream>

#include "../lishp.hpp"
#include "../runtime/exceptions.hpp"
#include "primitive_helpers.hpp"
#include "primitives.hpp"

auto cons(LishpRuntime *rt, LispCons args) -> LispForm {
  LispForm first = args.car;
  LispForm second = args.cdr;

  LispCons *c = new LispCons{{LispObjType::ObjCons}, false, first, second};
  return {LispFormType::FormObj, {.obj = c}};
}

auto format_(LishpRuntime *rt, LispCons args) -> LispForm {
  std::cout << args.to_string() << std::endl;

  // FIXME: for now, we are not dealing with format strings, and also, the only
  // stream that is valid, is `T`, so that it goes to standard out.

  LispForm stream_ = args.car;

  // TODO: add the T object and make the assert that the stream is equal to
  // that...

  LispCons *rest = args.rest();

  LispForm format_str = rest->car;

  if (format_str.type != LispFormType::FormObj) {
    // TODO: figure out how I want to do exceptions and what kind of information
    // I want to give
    throw RuntimeException("Expected argument of type String");
  }

  LispObj *obj = format_str.as.obj;

  if (obj->type != LispObjType::ObjString) {
    throw RuntimeException("Expected argument of type String");
  }

  LispString *str = (LispString *)obj;

  std::cout << str->str;

  return LispForm::nil();
}

auto read(LishpRuntime *rt, LispCons args) -> LispForm {
  return rt->get_reader().read_form(rt->cur_readtable(), std::cin);
}

auto eval(LishpRuntime *rt, LispCons args) -> LispForm {
  // To match other primitives, the args are packaged into a single element list
  LispForm car = args.car;
  return eval_form(rt, car);
}