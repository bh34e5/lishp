#include "runtime.hpp"

namespace runtime {

auto LishpRuntime::Repl() -> void {
  // run the internal function `system::repl`
  // FIXME: does this need to be upper case because of readcase?
  // FIXME: or maybe should this take a symbol instead of a string... probably
  Package *system_package = FindPackageByName("SYSTEM");
  Package *user_package = FindPackageByName("USER");

  types::LishpFunction *repl_function = system_package->SymbolFunction("REPL");

  types::LishpList args_list = types::LishpList::Nil();
  // NOTE: This is kinda scuffed... calling a system function in the context of
  // user package... not sure if this should work like this...
  repl_function->Call(user_package->global_env(), args_list);
}

auto LishpRuntime::Initialize() -> void {
  Package *system = BuildSystemPackage(this, &manager_);
  Package *user = BuildUserPackage(this, &manager_);

  default_readtable_ = BuildDefaultReadtable(system);

  InitializePackage(system);
  InitializePackage(user);
}

auto LishpRuntime::Uninitialize() -> void {
  // TODO: free necessary things
  manager_.Deallocate(default_readtable_);
  default_readtable_ = nullptr;
}

auto LishpRuntime::FindPackageByName(const std::string &name) -> Package * {
  using it_t = decltype(packages_)::iterator;

  it_t package_it = packages_.begin();
  while (package_it != packages_.end()) {
    if ((*package_it)->name() == name) {
      break;
    }
    package_it = std::next(package_it);
  }

  if (package_it == packages_.end()) {
    // TODO: turn this into an exception
    assert(0 &&
           "Invalid name passed; runtime::LishpRuntime::FindPackageByName");
  }

  return *package_it;
}

auto LishpRuntime::InitializePackage(Package *package) -> void {
  // TODO: this will probably need to be refactored when I figure out how
  // readtables actually work :)
  types::LishpSymbol *rt_sym = package->InternSymbol("*READTABLE*");
  package->global_env()->BindValue(
      rt_sym, types::LishpForm::FromObj(default_readtable_));

  packages_.push_back(std::move(package));
}

} // namespace runtime
