#include "primitives.hpp"

auto cons(LispCons args) -> LispForm {
  if (args.nil) {
  }

  LispForm first = args.car;
  LispForm second = args.cdr;

  LispCons *c = new LispCons{{LispObjType::ObjCons}, false, first, second};
  return {LispFormType::FormObj, {.obj = (LispObj *)c}};
}

auto format_(LispCons args) -> LispForm {
  if (false)
    ;
}
