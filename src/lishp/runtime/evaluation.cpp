#include "evaluation.hpp"
#include "functions/inherents.hpp"
#include "package.hpp"

// forward decls

static auto EvalCons(environment::Environment *lexical, types::LishpCons *cons)
    -> types::LishpFunctionReturn;

// impls

static auto HandleSpecialForm(environment::Environment *lexical,
                              types::LishpSymbol *form_sym,
                              types::LishpList &args) {
#define HANDLE_FORM(form_name, FormHandler)                                    \
  do {                                                                         \
    types::LishpSymbol *handler_sym =                                          \
        lexical->package()->InternSymbol(form_name);                           \
                                                                               \
    if (form_sym == handler_sym) {                                             \
      return inherents::special_forms::FormHandler(lexical, args);             \
    }                                                                          \
  } while (0)

  HANDLE_FORM("TAGBODY", Tagbody);
  HANDLE_FORM("GO", Go);
  HANDLE_FORM("QUOTE", Quote);
  HANDLE_FORM("FUNCTION", Function);
  HANDLE_FORM("PROGN", Progn);
  HANDLE_FORM("LABELS", Labels);
  HANDLE_FORM("LET*", LetStar);

  assert(0 && "Form passed is not a special form");

#undef HANDLE_FORM
}

static auto EvalLambdaForm(environment::Environment *lexical,
                           types::LishpCons *lambda, types::LishpList &args)
    -> types::LishpFunctionReturn {
  // TODO: maybe this needs to change, not sure..
  types::LishpFunctionReturn lambda_ret = BindLambdaForm(lexical, lambda);
  assert(lambda_ret.values.size() == 1);

  types::LishpFunction *func_form =
      lambda_ret.values.at(0).AssertAs<types::LishpFunction>();

  return func_form->Call(lexical, args);
}

static auto EvalCons(environment::Environment *lexical, types::LishpCons *cons)
    -> types::LishpFunctionReturn {
  types::LishpForm car = cons->car;

  if (car.ConsP()) {
    types::LishpList cons_list = types::LishpList::Of(cons);
    types::LishpList lambda_args = cons_list.rest();

    return EvalLambdaForm(lexical, car.AssertAs<types::LishpCons>(),
                          lambda_args);
  }

  types::LishpSymbol *func_sym = car.AssertAs<types::LishpSymbol>();

  types::LishpForm cdr = cons->cdr;
  types::LishpList args_list = types::LishpList::Nil();
  if (!cdr.NilP()) {
    args_list = types::LishpList::Of(cdr.AssertAs<types::LishpCons>());
  }

  if (IsSpecialForm(func_sym)) {
    return HandleSpecialForm(lexical, func_sym, args_list);
  }

  types::LishpFunction *func = lexical->SymbolFunction(func_sym);

  return func->Call(lexical, args_list);
}

static auto EvalSymbol(environment::Environment *lexical,
                       types::LishpSymbol *sym) {
  types::LishpForm value = lexical->SymbolValue(sym);
  return types::LishpFunctionReturn::FromValues(value);
}

static auto EvalObject(environment::Environment *lexical,
                       types::LishpObject *object) {
  switch (object->type) {
  case types::LishpObject::kCons: {
    return EvalCons(lexical, object->As<types::LishpCons>());
  }
  case types::LishpObject::kSymbol: {
    return EvalSymbol(lexical, object->As<types::LishpSymbol>());
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

auto IsSpecialForm(types::LishpSymbol *sym) -> bool {
#define CHECK_SYM(name)                                                        \
  do {                                                                         \
    const std::string &sym_lex = sym->lexeme;                                  \
    if (sym_lex == name) {                                                     \
      return true;                                                             \
    }                                                                          \
  } while (0);

  // TODO: umm this is incredibly stupid :)
  CHECK_SYM("TAGBODY");
  CHECK_SYM("GO");
  CHECK_SYM("QUOTE");
  CHECK_SYM("FUNCTION");
  CHECK_SYM("PROGN");
  CHECK_SYM("LABELS");
  CHECK_SYM("LET*");

  return false;

#undef CHECK_SYM
}

auto BindLambdaForm(environment::Environment *lexical,
                    types::LishpCons *lambda_expr)
    -> types::LishpFunctionReturn {
  memory::MemoryManager *manager = lexical->package()->manager();

  types::LishpList lambda_list = types::LishpList::Of(lambda_expr);

  types::LishpForm lambda_sym_form = lambda_list.first();
  assert(lambda_sym_form.AssertAs<types::LishpSymbol>()->lexeme == "LAMBDA");

  types::LishpList rest = lambda_list.rest();

  types::LishpForm args_form = rest.first();

  types::LishpList args_list = types::LishpList::Nil();
  if (!args_form.NilP()) {
    types::LishpCons *args_cons = args_form.AssertAs<types::LishpCons>();
    args_list = types::LishpList::Of(args_cons);
  }
  rest = rest.rest();

  types::LishpFunction *bound_func =
      manager->Allocate<types::LishpFunction>(lexical, args_list, rest);

  return types::LishpFunctionReturn::FromValues(
      types::LishpForm::FromObj(bound_func));
}

auto EvalForm(environment::Environment *lexical, types::LishpForm form)
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
    return EvalObject(lexical, form.object);
  }
}

auto EvalArgs(environment::Environment *lexical, types::LishpList &args)
    -> types::LishpList {
  if (args.nil) {
    return types::LishpList::Nil();
  }

  types::LishpFunctionReturn evaled_first_fr = EvalForm(lexical, args.first());
  // TODO: this will maybe change?
  // I _think_ the way that I'm using this, this is where I would add nil if
  // empty, and trim to just the first if not... I don't think this is getting
  // called in a context in which it will be used by multiple-value-bind, but
  // that's a concern that is a bit too early for the moment...
  assert(evaled_first_fr.values.size() == 1);
  types::LishpForm evaled_first = evaled_first_fr.values.at(0);

  types::LishpList rest_list = args.rest();
  types::LishpList evaled_rest = EvalArgs(lexical, rest_list);

  memory::MemoryManager *manager_ = lexical->package()->manager();
  return types::LishpList::Push(manager_, evaled_first, evaled_rest);
}
