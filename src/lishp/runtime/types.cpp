#include <assert.h>

#include "evaluation.hpp"
#include "package.hpp"
#include "types.hpp"

namespace types {

static auto EvalUserDefinedFunction(environment::Environment *closure,
                                    LishpList defined_args, LishpList body,
                                    LishpList &evaled_args)
    -> LishpFunctionReturn {
  // create a new lexical scope in which the parameters are bound to the args
  memory::MemoryManager *manager = closure->package()->manager();
  environment::Environment *call_env =
      manager->Allocate<environment::Environment>(closure->package(), closure);

  LishpList def_rest = defined_args;
  LishpList eval_rest = evaled_args;

  while (!def_rest.nil) {
    // TODO: handle the &command forms for rest, optional, key, etc.
    LishpForm def_first = def_rest.first();
    LishpForm eval_first = eval_rest.first();

    LishpSymbol *def_sym = def_first.AssertAs<LishpSymbol>();

    call_env->BindValue(def_sym, eval_first);

    def_rest = def_rest.rest();
    eval_rest = eval_rest.rest();
  }

  LishpSymbol *progn_sym = closure->package()->InternSymbol("PROGN");
  LishpList block = LishpList::Push(manager, progn_sym, body);
  return EvalForm(call_env, block.to_form());
}

auto LishpFunction::Call(environment::Environment *lexical, LishpList &args)
    -> LishpFunctionReturn {
  // TODO: need to add an args check at some point. And I _think_ that should be
  // the responsibility of this call method. I should store information on the
  // arguments, and do some quick matching on them (and filling in the defaults
  // / setting others to nil / etc.) here before passing on to the actual
  // function implementation code.

  // If the function is a special form, then it may not required evaluating the
  // arguments first; the form gets to decide. If not, then we need to evaluate
  // all the arguments first, and then pass that to the function body

  switch (function_type) {
  case kInherent: {
    LishpList evaled_args = EvalArgs(lexical, args);
    return (inherent)(closure, lexical, evaled_args);
  } break;
  case kSpecialForm:
    return (special_form)(lexical, args);
  case kUserDefined: {
    LishpList evaled_args = EvalArgs(lexical, args);
    return EvalUserDefinedFunction(closure, user_defined.args,
                                   user_defined.body, evaled_args);
  } break;
  }
}

static auto ConsToStringR(std::string &cur, LishpCons *cons) {
  std::string car_str = cons->car.ToString();

  bool first = cur.empty();
  if (!first) {
    cur += " ";
  }
  cur += car_str;

  if (cons->cdr.NilP()) {
    // nothing else to add
    return;
  }

  if (cons->cdr.type == LishpForm::kObject &&
      cons->cdr.object->type == LishpObject::kCons) {
    LishpCons *next = cons->cdr.object->As<LishpCons>();
    ConsToStringR(cur, next);
    return;
  }

  cur += " . ";
  cur += cons->cdr.ToString();
}

static auto ConsToString(LishpCons *cons) -> std::string {
  std::string rec = "";
  ConsToStringR(rec, cons);
  return "(" + rec + ")";
}

static auto StreamToString(LishpStream *stm) -> std::string {
  switch (stm->stream_type) {
  case LishpStream::kInput:
    return "<INPUT_STREAM>";
  case LishpStream::kOutput:
    return "<OUTPUT_STREAM>";
  case LishpStream::kInputOutput:
    return "<INPUT_OUTPUT_STREAM>";
  }
}

static auto SymbolToString(LishpSymbol *sym) -> std::string {
  if (sym->unique) {
    // TODO: actually figure out how these are printed
    return "#" + sym->lexeme + ":" + std::to_string(sym->id);
  }
  return sym->lexeme;
}

static auto FunctionToString(LishpFunction *func) -> std::string {
  switch (func->function_type) {
  case LishpFunction::kInherent:
    return "<INHERENT_FN>";
  case LishpFunction::kSpecialForm:
    return "<SPECIAL_FORM>";
  case LishpFunction::kUserDefined:
    return "<USER_DEFINED_FN>";
  }
}

static auto ObjectToString(LishpObject *obj) -> std::string {
  switch (obj->type) {
  case LishpObject::kCons:
    return ConsToString(obj->As<LishpCons>());
  case LishpObject::kStream:
    return StreamToString(obj->As<LishpStream>());
  case LishpObject::kString:
    return "\"" + obj->As<LishpString>()->lexeme + "\"";
  case LishpObject::kSymbol:
    return SymbolToString(obj->As<LishpSymbol>());
  case LishpObject::kFunction:
    return FunctionToString(obj->As<LishpFunction>());
  case LishpObject::kReadtable:
    return "<READTABLE>";
  }
}

auto LishpForm::ToString() -> std::string {
  switch (type) {
  case kT:
    return "T";
  case kNil:
    return "NIL";
  case kChar:
    return "CHAR(" + std::to_string(ch) + ")";
  case kFixnum:
    return "FIXNUM(" + std::to_string(fixnum) + ")";
  case kObject:
    return ObjectToString(object);
  }
}

auto LishpObject::MarkUsed() -> void {
  switch (type) {
  case kCons: {
    this->As<LishpCons>()->MarkUsed();
  } break;
  case kString: {
    this->As<LishpString>()->MarkUsed();
  } break;
  case kSymbol: {
    this->As<LishpSymbol>()->MarkUsed();
  } break;
  case kFunction: {
    this->As<LishpFunction>()->MarkUsed();
  } break;
  case kReadtable: {
    this->As<LishpReadtable>()->MarkUsed();
  } break;
  case kStream: {
    this->As<LishpStream>()->MarkUsed();
  } break;
  }
}

auto LishpCons::MarkUsed() -> void {
  if (color != kWhite) {
    // already marked
    return;
  }

  color = kGrey;

  if (car.type == LishpForm::kObject) {
    car.object->MarkUsed();
  }

  if (cdr.type == LishpForm::kObject) {
    cdr.object->MarkUsed();
  }
}

auto LishpString::MarkUsed() -> void {
  if (color != kWhite) {
    // already marked
    return;
  }

  color = kGrey;
}

auto LishpSymbol::MarkUsed() -> void {
  if (color != kWhite) {
    // already marked
    return;
  }

  color = kGrey;
}

auto LishpFunction::MarkUsed() -> void {
  if (color != kWhite) {
    // already marked
    return;
  }

  color = kGrey;

  switch (function_type) {
  case kInherent: {
    closure->MarkUsed();
  } break;
  case kUserDefined: {
    closure->MarkUsed();

    if (!user_defined.args.nil) {
      user_defined.args.cons->MarkUsed();
    }

    if (!user_defined.body.nil) {
      user_defined.body.cons->MarkUsed();
    }
  } break;
  case kSpecialForm:
    return;
  }
}

auto LishpReadtable::MarkUsed() -> void {
  if (color != kWhite) {
    // already marked
    return;
  }

  color = kGrey;

  for (auto &macro_func_pair : reader_macro_functions) {
    macro_func_pair.second->MarkUsed();
  }
}

auto LishpStream::MarkUsed() -> void {
  if (color != kWhite) {
    // already marked
    return;
  }

  color = kGrey;
}

} // namespace types
