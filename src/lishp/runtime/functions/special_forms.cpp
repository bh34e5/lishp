#include "../environment.hpp"
#include "../evaluation.hpp"
#include "../package.hpp"
#include "inherents.hpp"

namespace inherents {

namespace impl {

auto BindLambdaForm(environment::Environment *lexical,
                    types::LishpCons *lambda_expr) {
  memory::MemoryManager *manager = lexical->package()->manager();

  types::LishpList lambda_list = types::LishpList::Of(lambda_expr);

  types::LishpForm lambda_sym_form = lambda_list.first();
  assert(lambda_sym_form.AssertAs<types::LishpSymbol>()->lexeme == "LAMBDA");

  types::LishpList rest = lambda_list.rest();

  types::LishpForm args_form = rest.first();
  types::LishpCons *args_cons = args_form.AssertAs<types::LishpCons>();
  rest = rest.rest();

  types::LishpFunction *bound_func = manager->Allocate<types::LishpFunction>(
      lexical, types::LishpList::Of(args_cons), rest);

  return types::LishpFunctionReturn::FromValues(
      types::LishpForm::FromObj(bound_func));
}

} // namespace impl

namespace special_forms {

// FIXME: I may need to change the way that I create these... maybe they
// aren't actually "functions" since they don't have a closure scope... they
// are only lexical. For now, I think I can get by with just ignoring the
// closure environment arg and letting it still be the user global...

auto Tagbody(environment::Environment *lexical, types::LishpList &args)
    -> types::LishpFunctionReturn {
  environment::Environment cur_env(lexical->package(), lexical);

  // mark all the tags (so that jumping forward works as well)

  types::LishpList rest = args;
  while (!rest.nil) {
    types::LishpForm form = rest.first();
    if (form.AtomP()) {
      cur_env.MarkTag(form, rest);
    }
    rest = rest.rest();
  }

  // execute all the forms until running out of the end of the block, and then
  // return nil at the end

  rest = args;
  while (!rest.nil) {
    types::LishpForm form = rest.first();
    if (form.ConsP()) {
      types::LishpFunctionReturn ret = EvalForm(&cur_env, form);

      if (ret.type == types::LishpFunctionReturn::kGo) {
        // check the current environment for the current tag, if found, set the
        // current `rest` to the tag target. if not found, return the same ret,
        // because maybe somewhere up the line will have the tag and will then
        // be able to handle it

        types::LishpForm target = ret.go_tag;
        types::LishpList next_list;
        bool found = cur_env.FindTag(target, &next_list);

        if (found) {
          rest = next_list;
          continue;
        } else {
          return ret;
        }
      }
    }
    rest = rest.rest();
  }

  return types::LishpFunctionReturn::FromValues(types::LishpForm::Nil());
}

auto Go(environment::Environment *, types::LishpList &args)
    -> types::LishpFunctionReturn {
  types::LishpForm first = args.first();
  // TODO: maybe some check that this go is an atom?
  return types::LishpFunctionReturn::FromGo(first);
}

auto Quote(environment::Environment *, types::LishpList &args)
    -> types::LishpFunctionReturn {
  // return the argument unevaluated
  return types::LishpFunctionReturn::FromValues(args.first());
}

auto Function(environment::Environment *lexical, types::LishpList &args)
    -> types::LishpFunctionReturn {
  // return the argument unevaluated
  types::LishpForm first = args.first();

  if (first.AtomP()) {
    types::LishpSymbol *first_sym = first.AssertAs<types::LishpSymbol>();
    types::LishpFunction *func = lexical->SymbolFunction(first_sym);
    return types::LishpFunctionReturn::FromValues(
        types::LishpForm::FromObj(func));
  }

  types::LishpCons *lambda_expr = first.AssertAs<types::LishpCons>();
  return impl::BindLambdaForm(lexical, lambda_expr);
}

auto Progn(environment::Environment *lexical, types::LishpList &args)
    -> types::LishpFunctionReturn {
  types::LishpList rest = args;
  while (true) {
    types::LishpForm first = rest.first();

    types::LishpFunctionReturn ret = EvalForm(lexical, first);

    rest = rest.rest();

    if (rest.nil) {
      return ret;
    }
  }
}

} // namespace special_forms

} // namespace inherents
