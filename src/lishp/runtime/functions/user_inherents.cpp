#include "../environment.hpp"
#include "../reader/reader.hpp"
#include "inherents.hpp"

namespace inherents {

namespace user {

namespace impl {

static auto IStreamFromForm(types::LishpForm form) -> std::istream & {
  if (form.NilP()) {
    return std::cin;
  }

  types::LishpIStream *stream_obj = form.AssertAs<types::LishpIStream>();
  return stream_obj->stm;
}

} // namespace impl

auto Read(environment::Environment *env, types::LishpList &args)
    -> types::LishpFunctionReturn {
  types::LishpForm first_form = args.first();
  std::istream &stm = impl::IStreamFromForm(first_form);

  reader::Reader reader(stm, env);
  return types::LishpFunctionReturn::FromValues(reader.ReadForm());
}

auto ReadChar(environment::Environment *env, types::LishpList &args)
    -> types::LishpFunctionReturn {
  types::LishpForm stream_form = args.first();
  types::LishpIStream *stm_obj = stream_form.AssertAs<types::LishpIStream>();

  char x = stm_obj->stm.get();
  // TODO: check for EOF here...
  return types::LishpFunctionReturn::FromValues(types::LishpForm::FromChar(x));
}

auto Format(environment::Environment *env, types::LishpList &args)
    -> types::LishpFunctionReturn {
  types::LishpForm o_stream = args.first();
  types::LishpList rest = args.rest();

  // FIXME: change this -- right now can only print to stdout
  assert(o_stream.TP());

  types::LishpForm fmt_string = rest.first();
  rest = rest.rest();
  assert(fmt_string.type == types::LishpForm::Types::kObject);

  types::LishpObject *obj = fmt_string.object;
  assert(obj->type == types::LishpObject::Types::kString);

  types::LishpString *l_string = obj->As<types::LishpString>();
  std::string &lexeme = l_string->lexeme;

  // FIXME: should this be a dynamic variable lookup? That is, does T refer to
  // something like *stdout* in the runtime?
  std::cout << lexeme;

  while (!rest.nil) {
    std::cout << " " << rest.first().ToString();
    rest = rest.rest();
  }

  return types::LishpFunctionReturn::FromValues(types::LishpForm::Nil());
}

} // namespace user

} // namespace inherents
