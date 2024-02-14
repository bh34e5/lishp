#include "../environment.hpp"
#include "../evaluation.hpp"
#include "inherents.hpp"

namespace inherents {

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

} // namespace special_forms

} // namespace inherents
