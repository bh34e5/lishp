#ifndef reader_h
#define reader_h

#include <iostream>

#include "../environment.hpp"
#include "../package.hpp"

namespace reader {

class Reader {
public:
  Reader(std::istream &stm, environment::Environment *closure,
         environment::Environment *lexical)
      : stm_(stm), closure_(closure), lexical_(lexical) {}

  auto ReadForm() -> types::LishpForm;

private:
  inline auto GetReadtable() {
    // TODO: when I get dynamic variables set up correctly, this is where I am
    // going to need to pass the lexical scope so that the SymbolValue can
    // properly find the readtable var
    runtime::Package *package = closure_->package();

    types::LishpForm rt_form = package->SymbolValue("*READTABLE*");
    assert(rt_form.type == types::LishpForm::Types::kObject);

    types::LishpObject *obj = rt_form.object;
    assert(obj->type == types::LishpObject::kReadtable);

    types::LishpReadtable *rt = obj->As<types::LishpReadtable>();
    return rt;
  }

  std::istream &stm_;
  environment::Environment *closure_;
  environment::Environment *lexical_;
};

} // namespace reader

#endif
