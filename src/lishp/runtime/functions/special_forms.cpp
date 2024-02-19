#include "../environment.hpp"
#include "../evaluation.hpp"
#include "../package.hpp"
#include "inherents.hpp"

namespace inherents {

namespace special_forms {

// FIXME: I may need to change the way that I create these... maybe they
// aren't actually "functions" since they don't have a closure scope... they
// are only lexical. For now, I think I can get by with just ignoring the
// closure environment arg and letting it still be the user global...

auto Tagbody(environment::Environment *lexical, types::LishpList &args)
    -> types::LishpFunctionReturn {
  memory::MemoryManager *manager = lexical->package()->manager();
  environment::Environment *cur_env =
      manager->Allocate<environment::Environment>(lexical->package(), lexical);

  // mark all the tags (so that jumping forward works as well)

  types::LishpList rest = args;
  while (!rest.nil) {
    types::LishpForm form = rest.first();
    if (form.AtomP()) {
      cur_env->MarkTag(form, rest);
    }
    rest = rest.rest();
  }

  // execute all the forms until running out of the end of the block, and then
  // return nil at the end

  rest = args;
  while (!rest.nil) {
    types::LishpForm form = rest.first();
    if (form.ConsP()) {
      types::LishpFunctionReturn ret = EvalForm(cur_env, form);

      if (ret.type == types::LishpFunctionReturn::kGo) {
        // check the current environment for the current tag, if found, set the
        // current `rest` to the tag target. if not found, return the same ret,
        // because maybe somewhere up the line will have the tag and will then
        // be able to handle it

        types::LishpForm target = ret.go_tag;
        types::LishpList next_list;
        bool found = cur_env->FindTag(target, &next_list);

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
  return BindLambdaForm(lexical, lambda_expr);
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

auto Labels(environment::Environment *lexical, types::LishpList &args)
    -> types::LishpFunctionReturn {
  memory::MemoryManager *manager = lexical->package()->manager();
  environment::Environment *cur_env =
      manager->Allocate<environment::Environment>(lexical->package(), lexical);

  types::LishpForm func_bindings_form = args.first();
  types::LishpList body = args.rest();

  // NOTE: all the bindings are visible to all the functions, so we just need
  // one lexical scope containing everything. For flet, since I don't think they
  // see each other, that will probably need nested environments
  if (!func_bindings_form.NilP()) {
    types::LishpList bindings_list =
        types::LishpList::Of(func_bindings_form.AssertAs<types::LishpCons>());

    while (!bindings_list.nil) {
      types::LishpForm func_binding = bindings_list.first();

      if (func_binding.NilP()) {
        bindings_list = bindings_list.rest();
        continue;
      }

      types::LishpList func_binding_list =
          types::LishpList::Of(func_binding.AssertAs<types::LishpCons>());

      types::LishpForm func_name = func_binding_list.first();
      types::LishpSymbol *func_name_sym =
          func_name.AssertAs<types::LishpSymbol>();

      func_binding_list = func_binding_list.rest();

      types::LishpList func_args = types::LishpList::Nil();
      types::LishpForm func_args_form = func_binding_list.first();
      if (!func_args_form.NilP()) {
        func_args =
            types::LishpList::Of(func_args_form.AssertAs<types::LishpCons>());
      }

      func_binding_list = func_binding_list.rest();

      types::LishpFunction *func = manager->Allocate<types::LishpFunction>(
          cur_env, func_args, func_binding_list);

      cur_env->BindFunction(func_name_sym, func);

      bindings_list = bindings_list.rest();
    }
  }

  // build implicit progn for the body and evaluate it
  types::LishpSymbol *progn_sym = cur_env->package()->InternSymbol("PROGN");
  types::LishpList block = types::LishpList::Push(manager, progn_sym, body);
  return EvalForm(cur_env, block.to_form());
}

} // namespace special_forms

} // namespace inherents
