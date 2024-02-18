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
  types::LishpSymbol *sym =
      manager_->Allocate<types::LishpSymbol>(lexeme, this);
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

#define DEFINE_PACKAGE_FUNCTION(manager, package_ns, package_var, global_env,  \
                                func_sym_lex, func_name)                       \
  do {                                                                         \
    types::LishpFunction *func = manager->Allocate<types::LishpFunction>(      \
        global_env, &inherents::package_ns::func_name);                        \
                                                                               \
    types::LishpSymbol *sym = package_var->InternSymbol(func_sym_lex);         \
    package_var->BindFunction(sym, func);                                      \
  } while (0)

#define DEFINE_PACKAGE_SPECIAL_FORM(manager, package_ns, package_var,          \
                                    func_sym_lex, sf_name)                     \
  do {                                                                         \
    types::LishpFunction *func = manager->Allocate<types::LishpFunction>(      \
        &inherents::package_ns::sf_name);                                      \
                                                                               \
    types::LishpSymbol *sym = package_var->InternSymbol(func_sym_lex);         \
    package_var->BindFunction(sym, func);                                      \
  } while (0)

#define ESTABLISH_BINDING(package_var, sym_lex, sym_val)                       \
  do {                                                                         \
    types::LishpSymbol *sym = package_var->InternSymbol(sym_lex);              \
    package_var->BindValue(sym, sym_val);                                      \
  } while (0)

auto BuildSystemPackage(LishpRuntime *runtime, memory::MemoryManager *manager)
    -> Package * {
  Package *sys = manager->Allocate<Package>("SYSTEM", runtime, manager);
  environment::Environment *global = sys->global_env();

  DEFINE_PACKAGE_FUNCTION(manager, system, sys, global, "REPL", Repl);
  DEFINE_PACKAGE_FUNCTION(manager, system, sys, global, "READ-OPEN-PAREN",
                          ReadOpenParen);
  DEFINE_PACKAGE_FUNCTION(manager, system, sys, global, "READ-CLOSE-PAREN",
                          ReadCloseParen);
  DEFINE_PACKAGE_FUNCTION(manager, system, sys, global, "READ-DOUBLE-QUOTE",
                          ReadDoubleQuote);
  DEFINE_PACKAGE_FUNCTION(manager, system, sys, global, "READ-SINGLE-QUOTE",
                          ReadSingleQuote);

  return sys;
}

auto BuildUserPackage(LishpRuntime *runtime, memory::MemoryManager *manager)
    -> Package * {
  Package *user = manager->Allocate<Package>("USER", runtime, manager);
  environment::Environment *global = user->global_env();

  // TODO: fill this up with a lot of functions...

  DEFINE_PACKAGE_FUNCTION(manager, user, user, global, "READ", Read);
  DEFINE_PACKAGE_FUNCTION(manager, user, user, global, "READ-CHAR", ReadChar);
  DEFINE_PACKAGE_FUNCTION(manager, user, user, global, "FORMAT", Format);
  DEFINE_PACKAGE_FUNCTION(manager, user, user, global, "+", Plus);
  DEFINE_PACKAGE_FUNCTION(manager, user, user, global, "*", Star);
  DEFINE_PACKAGE_FUNCTION(manager, user, user, global, "FUNCALL", Funcall);

  // FIXME: I think these also need to be moved out...

  // root bindings of values

  ESTABLISH_BINDING(user, "NIL", types::LishpForm::Nil());
  ESTABLISH_BINDING(user, "T", types::LishpForm::T());

  return user;
}

#undef ESTABLISH_BINDING
#undef DEFINE_PACKAGE_SPECIAL_FORM
#undef DEFINE_PACKAGE_FUNCTION

auto BuildDefaultReadtable(Package *package) -> types::LishpReadtable * {
#define INSTALL_MACRO_CHAR(c, ns, func_name)                                   \
  do {                                                                         \
    types::LishpFunction *macro_func = package->SymbolFunction(func_name);     \
    rt->InstallMacroChar(c, macro_func);                                       \
  } while (0)

  memory::MemoryManager *manager = package->manager();
  types::LishpReadtable *rt = manager->Allocate<types::LishpReadtable>();

  INSTALL_MACRO_CHAR('(', system, "READ-OPEN-PAREN");
  INSTALL_MACRO_CHAR(')', system, "READ-CLOSE-PAREN");
  INSTALL_MACRO_CHAR('"', system, "READ-DOUBLE-QUOTE");
  INSTALL_MACRO_CHAR('\'', system, "READ-SINGLE-QUOTE");

  return rt;
#undef INSTALL_MACRO_CHAR
}

} // namespace runtime
