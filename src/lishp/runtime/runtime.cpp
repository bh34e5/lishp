#include "runtime.hpp"
#include "interpreter/analyzer.hpp"

namespace runtime {

static auto OldRepl(LishpRuntime *rt) -> void {
  // run the internal function `system::repl`
  // FIXME: does this need to be upper case because of readcase?
  // FIXME: or maybe should this take a symbol instead of a string... probably
  Package *system_package = rt->FindPackageByName("SYSTEM");
  Package *user_package = rt->FindPackageByName("USER");

  types::LishpFunction *repl_function = system_package->SymbolFunction("REPL");

  types::LishpList args_list = types::LishpList::Nil();
  // NOTE: This is kinda scuffed... calling a system function in the context of
  // user package... not sure if this should work like this...
  repl_function->Call(user_package->global_env(), args_list);
}

auto LishpRuntime::Repl() -> void {
  bool use_old_way = false;

  // The old way, that was working...
  if (use_old_way) {
    OldRepl(this);
    return;
  }

  Package *system_package = FindPackageByName("SYSTEM");
  Package *user_package = FindPackageByName("USER");

  types::LishpSymbol *repl_symbol = system_package->InternSymbol("REPL");

  types::LishpForm car = types::LishpForm::FromObj(repl_symbol);
  types::LishpForm cdr = types::LishpForm::Nil();

  types::LishpCons cons(car, cdr);
  types::LishpForm cons_form = types::LishpForm::FromObj(&cons);

  bytecode::Analyzer analyzer;
  std::vector<bytecode::Bytecode> bytecode = analyzer.TransformForm(cons_form);

  bytecode::Evaluator evaluator(user_package->global_env());
  evaluator.EvaluateBytecode(bytecode);
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

  using it_t = decltype(packages_)::iterator;
  it_t pack_it = packages_.begin();
  while (pack_it != packages_.end()) {
    manager_.Deallocate(*pack_it);
    pack_it = packages_.erase(pack_it);
  }

  default_readtable_ = nullptr;

  manager_.RunGC();
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

  packages_.push_back(package);
}

auto LishpRuntime::MarkUsedObjects() -> void {
  if (default_readtable_ != nullptr) {
    default_readtable_->MarkUsed();
  }

  for (Package *package : packages_) {
    package->MarkUsed();
  }
}

} // namespace runtime
