#include "../lishp.hpp"
#include "../runtime/exceptions.hpp"
#include "types.hpp"

auto LispCons::rest() -> LispCons {
  if (cdr.type != LispFormType::FormObj) {
    throw RuntimeException("Unexpected arguments");
  }

  LispObj *cdrObj = (LispObj *)cdr.as.obj;

  if (cdrObj->type != LispObjType::ObjCons) {
    throw RuntimeException("Unexpected arguments");
  }

  LispCons *cdrCons = (LispCons *)cdrObj;
  return *cdrCons;
}

auto LispFunction::call(LishpRuntime *rt, LispCons args) -> LispForm {
  if (this->primitive) {
    PrimitiveFunction *inherent = this->body.inherent;
    return inherent(rt, args);
  }
  throw RuntimeException("Can't run user-defined functions");
}
