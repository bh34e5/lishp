#include "evaluation.hpp"
#include "package.hpp"

static auto EvalCons(environment::Environment *env, types::LishpCons *cons) {
  return types::LishpFunctionReturn::FromValues(types::LishpForm::Nil());
}

static auto EvalSymbol(environment::Environment *env, types::LishpCons *cons) {
  return types::LishpFunctionReturn::FromValues(types::LishpForm::Nil());
}

static auto EvalObject(environment::Environment *env,
                       types::LishpObject *object) {
  switch (object->type) {
  case types::LishpObject::kCons: {
    return EvalCons(env, object->As<types::LishpCons>());
  }
  case types::LishpObject::kSymbol: {
    return EvalSymbol(env, object->As<types::LishpCons>());
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
