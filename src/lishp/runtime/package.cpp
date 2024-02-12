#include <assert.h>

#include "functions/inherents.hpp"
#include "package.hpp"

namespace runtime {

auto Package::InternSymbol(std::string &&lexeme) -> types::LishpSymbol * {
  using it_t = decltype(interned_symbols_)::iterator;

  it_t sym_it = interned_symbols_.begin();
  while (sym_it != interned_symbols_.end()) {
    if (sym_it->first == lexeme) {
      return sym_it->second;
    }
    sym_it = std::next(sym_it);
  }

  // ran through and didn't find a match.. make a new symbol and insert it
  types::LishpSymbol *sym = manager_->Allocate<types::LishpSymbol>(lexeme);
  interned_symbols_.insert({std::move(lexeme), sym});

  return sym;
}

auto Package::SymbolValue(types::LishpSymbol *sym) -> types::LishpForm {
  return global_->SymbolValue(sym);
}

auto Package::SymbolFunction(types::LishpSymbol *sym)
    -> types::LishpFunction * {
  return global_->SymbolFunction(sym);
}

#define DEFINE_PACKAGE_FUNCTION(manager, package_ns, package_var,              \
                                func_sym_lex, func_name)                       \
  do {                                                                         \
    types::LishpFunction *func = manager->Allocate<types::LishpFunction>(      \
        &inherents::package_ns::func_name);                                    \
                                                                               \
    types::LishpSymbol *sym = package_var->InternSymbol(func_sym_lex);         \
    package_var->BindFunction(sym, func);                                      \
  } while (0)

#define ESTABLISH_BINDING(package_var, sym_lex, sym_val)                       \
  do {                                                                         \
    types::LishpSymbol *sym = package_var->InternSymbol(sym_lex);              \
    package_var->BindValue(sym, sym_val);                                      \
  } while (0)

auto BuildSystemPackage(memory::MemoryManager *manager) -> Package * {
  Package *sys = manager->Allocate<Package>("SYSTEM", manager);

  DEFINE_PACKAGE_FUNCTION(manager, system, sys, "REPL", Repl);
  DEFINE_PACKAGE_FUNCTION(manager, system, sys, "READ-OPEN-PAREN",
                          ReadOpenParen);
  DEFINE_PACKAGE_FUNCTION(manager, system, sys, "READ-CLOSE-PAREN",
                          ReadCloseParen);
  DEFINE_PACKAGE_FUNCTION(manager, system, sys, "READ-DOUBLE-QUOTE",
                          ReadDoubleQuote);

  return sys;
}

auto BuildUserPackage(memory::MemoryManager *manager) -> Package * {
  Package *user = manager->Allocate<Package>("USER", manager);

  // TODO: fill this up with a lot of functions...

  DEFINE_PACKAGE_FUNCTION(manager, user, user, "READ", Read);
  DEFINE_PACKAGE_FUNCTION(manager, user, user, "READ-CHAR", ReadChar);
  DEFINE_PACKAGE_FUNCTION(manager, user, user, "FORMAT", Format);

  // special forms

  DEFINE_PACKAGE_FUNCTION(manager, special_forms, user, "TAGBODY", Tagbody);

  // root bindings of values

  ESTABLISH_BINDING(user, "NIL", types::LishpForm::Nil());
  ESTABLISH_BINDING(user, "T", types::LishpForm::T());

  return user;
}

#undef ESTABLISH_BINDING
#undef DEFINE_PACKAGE_FUNCTION

auto BuildSystemReadtable(memory::MemoryManager *manager)
    -> types::LishpReadtable * {
#define INSTALL_MACRO_CHAR(c, ns, func_name)                                   \
  do {                                                                         \
    types::LishpFunction *macro_func =                                         \
        manager->Allocate<types::LishpFunction>(inherents::ns::func_name);     \
    rt->InstallMacroChar(c, macro_func);                                       \
  } while (0)

  types::LishpReadtable *rt = manager->Allocate<types::LishpReadtable>();

  INSTALL_MACRO_CHAR('(', system, ReadOpenParen);
  INSTALL_MACRO_CHAR(')', system, ReadCloseParen);
  INSTALL_MACRO_CHAR('"', system, ReadDoubleQuote);

  return rt;
#undef INSTALL_MACRO_CHAR
}

} // namespace runtime
