#include <sstream>

#include "../lishp.hpp"
#include "../primitives/primitive_helpers.hpp"
#include "../runtime/exceptions.hpp"
#include "types.hpp"

auto LispForm::is_nil() -> bool {
  return this->type == LispFormType::FormNil && this->as.obj == NULL;
}

auto LispForm::is_t() -> bool {
  return this->type == LispFormType::FormT && this->as.obj == NULL;
}

auto LispCons::rest() -> LispCons * {
  if (cdr.type != LispFormType::FormObj) {
    throw RuntimeException("Unexpected arguments");
  }

  LispObj *cdrObj = cdr.as.obj;

  if (cdrObj->type != LispObjType::ObjCons) {
    throw RuntimeException("Unexpected arguments");
  }

  LispCons *cdrCons = (LispCons *)cdrObj;
  return cdrCons;
}

static auto eval_args(LishpRuntime *rt, LispCons *args) -> LispCons {
  LispForm first = eval_form(rt, args->car);
  LispForm rest = LispForm::nil();
  if (!args->cdr.is_nil()) {
    LispCons *rest_cons = new LispCons(eval_args(rt, args->rest()));
    rest = LispForm{LispFormType::FormObj, {.obj = rest_cons}};
  }

  return LispCons{
      {LispObjType::ObjCons}, false, std::move(first), std::move(rest)};
}

auto LispFunction::call(LishpRuntime *rt, LispCons args) -> LispForm {
  LispCons evaled_args = eval_args(rt, &args);
  if (this->primitive) {
    PrimitiveFunction *inherent = this->body.inherent;
    return inherent(rt, evaled_args);
  }
  throw RuntimeException("Can't run user-defined functions");
}

auto LispForm::to_string() -> std::string {
  switch (this->type) {
  case FormT:
    return "T";
  case FormNil:
    return "NIL";
  case FormNumber:
    return std::to_string(this->as.number);
  case FormObj:
    return this->as.obj->to_string();
  }
}

auto LispObj::to_string() -> std::string {
  switch (this->type) {
  case ObjCons:
    return ((LispCons *)this)->to_string();
  case ObjFunction:
    return ((LispFunction *)this)->to_string();
  case ObjString:
    return ((LispString *)this)->to_string();
  case ObjSymbol:
    return ((LispSymbol *)this)->to_string();
  }
}

static auto build_list_string(LispCons *cons, std::ostringstream &str) -> void {
  LispForm first = cons->car;
  str << first.to_string();

  LispForm second = cons->cdr;
  if (second.is_nil()) {
    // made it to the last entry in the list; break out
    str << ")";
    return;
  }

  if (second.type != LispFormType::FormObj ||
      second.as.obj->type != LispObjType::ObjCons) {
    // second not nil or cons, so this is a dotted pair.
    str << " . ";
    str << second.to_string();
    str << ")";
    return;
  }

  // second is cons, so keep building
  str << " ";
  build_list_string(cons->rest(), str);
}

auto LispCons::to_string() -> std::string {
  if (this->nil) {
    return "()";
  }

  std::ostringstream str{"(", std::stringstream::ate};
  build_list_string(this, str);

  return str.str();
}

auto LispFunction::to_string() -> std::string {
  // FIXME: make this better at some point.
  return "<function>";
}

auto LispString::to_string() -> std::string { return "\"" + this->str + "\""; }

auto LispSymbol::to_string() -> std::string { return this->lexeme; }
