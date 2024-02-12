#ifndef reader_h
#define reader_h

#include <iostream>

#include "../environment.hpp"
#include "../package.hpp"

namespace reader {

class Reader {
public:
  Reader(std::istream &stm, environment::Environment *env)
      : stm_(stm), env_(env) {}

  auto ReadForm() -> types::LishpForm;

private:
  inline auto GetReadtable() {
    runtime::Package *package = env_->package();

    types::LishpSymbol *rt_sym = package->InternSymbol("*READTABLE*");
    types::LishpForm rt_form = package->SymbolValue(rt_sym);

    assert(rt_form.type == types::LishpForm::Types::kObject);

    types::LishpObject *obj = rt_form.object;
    assert(obj->type == types::LishpObject::kReadtable);

    types::LishpReadtable *rt = obj->As<types::LishpReadtable>();
    return rt;
  }

  std::istream &stm_;
  environment::Environment *env_;
};

} // namespace reader

#endif
