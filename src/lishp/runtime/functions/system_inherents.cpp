#include <sstream>

#include "../evaluation.hpp"
#include "../package.hpp"
#include "inherents.hpp"

namespace inherents {

namespace system {

auto Repl(environment::Environment *env, types::LishpList &args)
    -> types::LishpFunctionReturn {
  // NOTE: I think, since this is a system function, redefinition is probably
  // either disabled, or it's undefined behavior, so I think I can just make a
  // call to the user read function?

  memory::MemoryManager *manager = env->package()->manager();

  types::LishpString *string_arg =
      manager->Allocate<types::LishpString>("~%~a");

  types::LishpList nil_args = types::LishpList::Nil();

  bool keep_running = true;

  while (keep_running) {
    types::LishpFunctionReturn form_ret = user::Read(env, nil_args);

    // FIXME: need to check all my asserts...
    assert(form_ret.values.size() == 1);
    types::LishpForm form_val = form_ret.values.at(0);
    types::LishpFunctionReturn evaled_result = EvalForm(env, form_val);

    assert(evaled_result.values.size() == 1);
    types::LishpForm evaled_form = evaled_result.values.at(0);

    types::LishpList format_args_list = types::LishpList::Build(
        manager, types::LishpForm::T(), string_arg, evaled_form);

    user::Format(env, format_args_list);
    std::cout << std::endl;

    // FIXME: allow infinite looping
    keep_running = false;
  }

  return {std::vector{types::LishpForm::Nil()}};
}

static bool IsWhitespace(const std::string &s, std::streampos pos) {
  for (std::string::size_type index = pos; index < s.length(); index++) {
    // TODO: change this to check against LISP whitespace, and not just c++
    // whitespace
    if (!std::isspace(s[index])) {
      return false;
    }
  }
  return true;
}

auto ReadOpenParen(environment::Environment *env, types::LishpList &args)
    -> types::LishpFunctionReturn {
  types::LishpForm stream_form = args.first();
  types::LishpIStream *stm_obj = stream_form.AssertAs<types::LishpIStream>();

  memory::MemoryManager *manager = env->package()->manager();

  std::stringstream sstream(std::ios_base::in | std::ios_base::out);
  types::LishpIStream *sstream_obj =
      manager->Allocate<types::LishpIStream>(sstream);
  types::LishpList rec_read_call_args =
      types::LishpList::Build(manager, sstream_obj);

  std::istream &stm = stm_obj->stm;

  int open_paren_count = 1;
  while (true) {
    char c = stm.get();
    if (stm.eof()) {
      // TODO: handle EOF
    }

    if (c == '(') {
      ++open_paren_count;
    } else if (c == ')') {
      --open_paren_count;
      if (!open_paren_count) {
        break;
      }
    }

    sstream << c;
  }

  types::LishpSymbol *read_sym = env->package()->InternSymbol("READ");
  types::LishpFunction *read_function =
      env->package()->SymbolFunction(read_sym);

  types::LishpList cur_list = types::LishpList::Nil();

  while (true) {
    std::streampos pos = sstream.tellg();
    if (IsWhitespace(sstream.str(), pos)) {
      break;
    }

    types::LishpFunctionReturn read_res =
        read_function->Call(env, rec_read_call_args);
    assert(read_res.values.size() == 1);

    types::LishpForm form = read_res.values.at(0);
    cur_list = types::LishpList::Push(manager, form, cur_list);
  }
  return types::LishpFunctionReturn::FromValues(
      cur_list.Reverse(manager).to_form());
}

auto ReadCloseParen(environment::Environment *, types::LishpList &)
    -> types::LishpFunctionReturn {
  // FIXME: figure out the proper way to handle this as well...
  throw std::runtime_error("Cannot have ')' in a form... or something");
}

} // namespace system

} // namespace inherents
