#include "../environment.hpp"
#include "../evaluation.hpp"
#include "inherents.hpp"

namespace inherents {

namespace special_forms {

auto Tagbody(environment::Environment *env, types::LishpList &args)
    -> types::LishpFunctionReturn {
  environment::Environment cur_env(env->package(), env);

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

      if (ret.type == types::LishpFunctionReturn::kGoto) {
        // check the current environment for the current tag, if found, set the
        // current `rest` to the tag target. if not found, return the same ret,
        // because maybe somewhere up the line will have the tag and will then
        // be able to handle it

        types::LishpForm target = ret.goto_tag;
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

} // namespace special_forms

} // namespace inherents
