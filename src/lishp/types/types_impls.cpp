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
