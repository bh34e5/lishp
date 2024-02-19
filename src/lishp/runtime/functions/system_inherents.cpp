#include <sstream>

#include "../evaluation.hpp"
#include "../package.hpp"
#include "../runtime.hpp"
#include "inherents.hpp"

namespace inherents {

namespace system {

auto Repl(environment::Environment *closure, environment::Environment *lexical,
          types::LishpList &) -> types::LishpFunctionReturn {
  // NOTE: I think, since this is a system function, redefinition is probably
  // either disabled, or it's undefined behavior, so I think I can just make a
  // call to the user read function?
  runtime::Package *closure_pkg = closure->package();

  memory::MemoryManager *manager = closure_pkg->manager();
  runtime::LishpRuntime *runtime = closure_pkg->runtime();

  runtime::Package *user_package = runtime->FindPackageByName("USER");
  types::LishpFunction *user_read = user_package->SymbolFunction("READ");
  types::LishpFunction *user_format = user_package->SymbolFunction("FORMAT");

  types::LishpString *string_arg =
      manager->Allocate<types::LishpString>("~%~a");

  types::LishpList nil_args = types::LishpList::Nil();

  while (true) {
    types::LishpFunctionReturn form_ret = user_read->Call(lexical, nil_args);

    // FIXME: need to check all my asserts...
    assert(form_ret.values.size() == 1);
    types::LishpForm form_val = form_ret.values.at(0);

    if (form_val.ConsP()) {
      types::LishpForm cons_first = form_val.AssertAs<types::LishpCons>()->car;
      if (cons_first.type == types::LishpForm::kObject &&
          cons_first.object->type == types::LishpObject::kSymbol) {
        types::LishpSymbol *sym = cons_first.AssertAs<types::LishpSymbol>();

        // TODO: add eqls ignore case here
        if (sym->lexeme == "QUIT") {
          return {std::vector{types::LishpForm::Nil()}};
        }
      }
    }

    types::LishpList format_args_list = types::LishpList::Build(
        manager, types::LishpForm::T(), string_arg, form_val);

    user_format->Call(lexical, format_args_list);
    std::cout << std::endl;
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

auto ReadOpenParen(environment::Environment *closure,
                   environment::Environment *lexical, types::LishpList &args)
    -> types::LishpFunctionReturn {
  types::LishpForm stream_form = args.first();
  types::LishpIStream *stm_obj = stream_form.AssertAs<types::LishpIStream>();

  runtime::Package *closure_pkg = closure->package();
  memory::MemoryManager *manager = closure_pkg->manager();
  runtime::LishpRuntime *runtime = closure_pkg->runtime();

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

  runtime::Package *user_package = runtime->FindPackageByName("USER");
  types::LishpFunction *user_read = user_package->SymbolFunction("READ");

  types::LishpList cur_list = types::LishpList::Nil();

  while (true) {
    std::streampos pos = sstream.tellg();
    if (IsWhitespace(sstream.str(), pos)) {
      break;
    }

    types::LishpFunctionReturn read_res =
        user_read->Call(lexical, rec_read_call_args);
    assert(read_res.values.size() == 1);

    types::LishpForm form = read_res.values.at(0);
    cur_list = types::LishpList::Push(manager, form, cur_list);
  }
  return types::LishpFunctionReturn::FromValues(
      cur_list.Reverse(manager).to_form());
}

auto ReadCloseParen(environment::Environment *, environment::Environment *,
                    types::LishpList &) -> types::LishpFunctionReturn {
  // FIXME: figure out the proper way to handle this as well...
  throw std::runtime_error("Cannot have ')' in a form... or something");
}

static auto EscapeChar(char c) {
  switch (c) {
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case 't':
    return '\t';
  case 'v':
    return '\v';
  case '0':
    return '\0';
  case '1':
    return '\1';
  case '2':
    return '\2';
  case '3':
    return '\3';
  case '4':
    return '\4';
  case '5':
    return '\5';
  case '6':
    return '\6';
  case '7':
    return '\7';
  case '?':
    return '\?';
  default:
    return c;
  }
}

auto ReadDoubleQuote(environment::Environment *closure,
                     environment::Environment *, types::LishpList &args)
    -> types::LishpFunctionReturn {
  // FIXME: there is a possible bug... when I ran the interpreter with the
  // following, I appear to have gotten stuck in a loop:
  //
  // ((lambda (a1) a1) "argstr)
  // )
  // ")
  //
  // We may not be handling newlines correctly? I'm not sure.

  // get the stream
  types::LishpForm stream_form = args.first();
  types::LishpIStream *stm_obj = stream_form.AssertAs<types::LishpIStream>();

  std::istream &stm = stm_obj->stm;
  std::string builder;

  bool escaped_next = false;
  while (true) {
    char c = stm.get();

    if (c == '\"' && !escaped_next) {
      break;
    } else if (c == '\\' && !escaped_next) {
      escaped_next = true;
      continue;
    } else if (escaped_next) {
      // handle escape characters
      builder.push_back(EscapeChar(c));
    } else {
      builder.push_back(c);
    }

    escaped_next = false;
  }

  memory::MemoryManager *manager = closure->package()->manager();

  types::LishpString *str_obj =
      manager->Allocate<types::LishpString>(std::move(builder));

  return types::LishpFunctionReturn::FromValues(
      types::LishpForm::FromObj(str_obj));
}

auto ReadSingleQuote(environment::Environment *closure,
                     environment::Environment *lexical, types::LishpList &args)
    -> types::LishpFunctionReturn {
  runtime::Package *closure_pkg = closure->package();

  memory::MemoryManager *manager = closure_pkg->manager();
  runtime::LishpRuntime *runtime = closure_pkg->runtime();

  runtime::Package *user_package = runtime->FindPackageByName("USER");
  types::LishpFunction *user_read = user_package->SymbolFunction("READ");

  types::LishpForm stream_form = args.first();
  types::LishpList read_args = types::LishpList::Build(manager, stream_form);

  types::LishpFunctionReturn read_func_ret =
      user_read->Call(lexical, read_args);
  types::LishpForm read_form = read_func_ret.values.at(0);

  types::LishpSymbol *read_sym = user_package->InternSymbol("QUOTE");
  types::LishpList quote_list =
      types::LishpList::Build(manager, read_sym, read_form);

  return types::LishpFunctionReturn::FromValues(quote_list.to_form());
}

} // namespace system

} // namespace inherents
