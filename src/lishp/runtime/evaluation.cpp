#include "evaluation.hpp"
#include "functions/inherents.hpp"
#include "package.hpp"

static bool IsSpecialForm(environment::Environment *env,
                          types::LishpSymbol *sym) {
#define CHECK_SYM(name)                                                        \
  do {                                                                         \
    types::LishpSymbol *test_sym = env->package()->InternSymbol(name);         \
    if (sym == test_sym) {                                                     \
      return true;                                                             \
    }                                                                          \
  } while (0);

  // TODO: umm this is incredibly stupid :)
  CHECK_SYM("TAGBODY");
  CHECK_SYM("GO");
  CHECK_SYM("QUOTE");
  return false;

#undef CHECK_SYM
}

static auto HandleSpecialForm(environment::Environment *env,
                              types::LishpSymbol *form_sym,
                              types::LishpList &args) {
#define HANDLE_FORM(form_name, FormHandler)                                    \
  do {                                                                         \
    types::LishpSymbol *handler_sym = env->package()->InternSymbol(form_name); \
    if (form_sym == handler_sym) {                                             \
      return inherents::special_forms::FormHandler(env, args);                 \
    }                                                                          \
  } while (0)

  HANDLE_FORM("TAGBODY", Tagbody);
  HANDLE_FORM("GO", Go);
  HANDLE_FORM("QUOTE", Quote);

  assert(0 && "Form passed is not a special form");

#undef HANDLE_FORM
}

static auto EvalCons(environment::Environment *env, types::LishpCons *cons) {
  types::LishpForm car = cons->car;
  types::LishpSymbol *func_sym = car.AssertAs<types::LishpSymbol>();

  types::LishpForm cdr = cons->cdr;
  types::LishpList args_list = types::LishpList::Nil();
  if (!cdr.NilP()) {
    args_list = types::LishpList::Of(cdr.AssertAs<types::LishpCons>());
  }

  if (IsSpecialForm(env, func_sym)) {
    return HandleSpecialForm(env, func_sym, args_list);
  }

  types::LishpFunction *func = env->SymbolFunction(func_sym);

  return func->Call(env, args_list);
}

static auto EvalSymbol(environment::Environment *env, types::LishpSymbol *sym) {
  types::LishpForm value = env->SymbolValue(sym);
  return types::LishpFunctionReturn::FromValues(value);
}

static auto EvalObject(environment::Environment *env,
                       types::LishpObject *object) {
  switch (object->type) {
  case types::LishpObject::kCons: {
    return EvalCons(env, object->As<types::LishpCons>());
  }
  case types::LishpObject::kSymbol: {
    return EvalSymbol(env, object->As<types::LishpSymbol>());
  }
  case types::LishpObject::kStream:
  case types::LishpObject::kString:
  case types::LishpObject::kFunction:
  case types::LishpObject::kReadtable: {
    // evaluate to themselves...
    types::LishpForm copy = types::LishpForm::FromObj(object);
    return types::LishpFunctionReturn::FromValues(std::move(copy));
  }
  }
}

auto EvalForm(environment::Environment *env, types::LishpForm form)
    -> types::LishpFunctionReturn {
  switch (form.type) {
  case types::LishpForm::kT:
  case types::LishpForm::kNil:
  case types::LishpForm::kChar:
  case types::LishpForm::kFixnum: {
    // evaluate to themselves...
    types::LishpForm copy(form);
    return types::LishpFunctionReturn::FromValues(std::move(copy));
  }
  case types::LishpForm::kObject:
    return EvalObject(env, form.object);
  }
}

auto EvalArgs(environment::Environment *env, types::LishpList &args)
    -> types::LishpList {
  if (args.nil) {
    return types::LishpList::Nil();
  }

  types::LishpFunctionReturn evaled_first_fr = EvalForm(env, args.first());
  // TODO: this will maybe change?
  // I _think_ the way that I'm using this, this is where I would add nil if
  // empty, and trim to just the first if not... I don't think this is getting
  // called in a context in which it will be used by multiple-value-bind, but
  // that's a concern that is a bit too early for the moment...
  assert(evaled_first_fr.values.size() == 1);
  types::LishpForm evaled_first = evaled_first_fr.values.at(0);

  types::LishpList rest_list = args.rest();
  types::LishpList evaled_rest = EvalArgs(env, rest_list);

  memory::MemoryManager *manager_ = env->package()->manager();
  return types::LishpList::Push(manager_, evaled_first, evaled_rest);
}
