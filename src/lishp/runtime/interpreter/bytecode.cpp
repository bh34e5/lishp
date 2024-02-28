#include "bytecode.hpp"
#include "../environment.hpp"
#include "../evaluation.hpp"
#include "../package.hpp"
#include "analyzer.hpp"

namespace bytecode {

auto Evaluator::EvaluateBytecode(std::vector<Bytecode> &code) -> void {
  for (Bytecode &bc : code) {
    EvaluateBytecode(bc);
  }
}

auto Evaluator::EvaluateBytecode(Bytecode &code) -> void {
  switch (code.type) {
  case Bytecode::kNop: {
    // Do nothing
  } break;
  case Bytecode::kPop: {
    if (stack_.size() == 0) {
      assert(0 && "Pop called on empty stack");
    }
    stack_.pop();
  } break;
  case Bytecode::kEval: {
    types::LishpForm form_to_eval = stack_.top();
    stack_.pop();
    types::LishpFunctionReturn ret = EvalForm(nullptr, form_to_eval);

    // FIXME: I don't know what to do with this... because I need to have _some_
    // way of doing the multiple value bind. It's almost like I want to have the
    // stack be a union type...?
    // But then again, multiple-value-bind is a macro that could expand to a
    // call to multiple-value-call, so in that case, the multiple values of the
    // LishpFunctionReturn only _really_ needs to be handled in that case, so
    // everywhere else can just assume to take the first (or nil)? Maybe

    types::LishpForm result_form = types::LishpForm::Nil();
    if (ret.values.size() > 0) {
      result_form = ret.values.at(0);
    }

    stack_.push(result_form);
  } break;
  case Bytecode::kPush: {
    stack_.push(code.form);
  } break;
  case Bytecode::kCallFunction: {
    types::LishpForm sym_form = stack_.top();
    stack_.pop();

    types::LishpForm args_form = stack_.top();
    stack_.pop();

    types::LishpSymbol *func_sym = sym_form.AssertAs<types::LishpSymbol>();

    types::LishpList args_list = types::LishpList::Nil();
    if (!args_form.NilP()) {
      args_list = types::LishpList::Of(args_form.AssertAs<types::LishpCons>());
    }

    types::LishpFunction *func = current_env_->SymbolFunction(func_sym);

    switch (func->function_type) {
    case types::LishpFunction::kInherent:
    case types::LishpFunction::kSpecialForm: {
      // for these two, just call the function as we have been at the moment
      types::LishpFunctionReturn ret = func->Call(current_env_, args_list);

      types::LishpForm result_form = types::LishpForm::Nil();
      if (ret.values.size() > 0) {
        result_form = ret.values.at(0);
      }

      stack_.push(result_form);
    } break;
    case types::LishpFunction::kUserDefined: {
      EvaluateUserDefinedFunction(func, args_list);
    } break;
    }
  } break;
  case Bytecode::kPopEnv: {
    if (current_env_ == nullptr) {
      assert(0 && "Cannot pop null environment");
    }
    current_env_ = current_env_->parent();
  } break;
  case Bytecode::kPushEnv: {
    runtime::Package *package = current_env_->package();

    current_env_ = package->manager()->Allocate<environment::Environment>(
        package, current_env_);
  } break;
  case Bytecode::kBindSymbolValue: {
    types::LishpForm form = stack_.top();
    stack_.pop();

    types::LishpForm sym_form = stack_.top();
    stack_.pop();

    types::LishpSymbol *sym = sym_form.AssertAs<types::LishpSymbol>();

    current_env_->BindValue(sym, form);
  } break;
  }
}

auto Evaluator::EvaluateUserDefinedFunction(types::LishpFunction *func,
                                            types::LishpList &args_list)
    -> void {
  types::LishpList param_list = func->user_defined.args;

  // analyze the argument evaluations and the function body
  Analyzer analyzer(current_env_);
  std::vector<Bytecode> funcall_bytes;

  while (!args_list.nil) {
    types::LishpForm first_form = args_list.first();

    std::vector<Bytecode> form_eval_code = analyzer.TransformForm(first_form);
    funcall_bytes.insert(funcall_bytes.end(), form_eval_code.begin(),
                         form_eval_code.end());

    args_list = args_list.rest();
  }

  // analyze the function body in the new environment and then run it
  // THOUGHTS: I run with a "push_env", then eval all the args intermingled
  // with bind value calls. Then I get the environment from the evaluator, and
  // use that to analyze the function body (so that the params are bound?)
  //
  // Or maybe I just do something else where it doesn't know what the bindings
  // are, it just needs to know _where_ the bindings are... that's what the
  // crafting interpreters book did...
}

} // namespace bytecode
