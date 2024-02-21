#ifndef runtime_package_h
#define runtime_package_h

#include <map>
#include <string>

#include "../memory/memory.hpp"
#include "environment.hpp"
#include "types.hpp"

namespace runtime {

class LishpRuntime;

class Package : public memory::MarkedObject {
public:
  Package() = delete;
  Package(std::string &&name, LishpRuntime *runtime,
          memory::MemoryManager *manager)
      : memory::MarkedObject(kPackage), name_(std::move(name)),
        runtime_(runtime), manager_(manager) {
    global_ = manager->Allocate<environment::Environment>(this);
  }
  Package(const Package &other) = delete;
  Package(Package &&other) : memory::MarkedObject(other.type) {
    std::swap(name_, other.name_);
    std::swap(interned_symbols_, other.interned_symbols_);
    runtime_ = other.runtime_;
    manager_ = other.manager_;
    global_ = other.global_;

    other.runtime_ = nullptr;
    other.manager_ = nullptr;
    other.global_ = nullptr;
  };
  ~Package() {
    if (manager_ != nullptr && global_ != nullptr) {
      runtime_ = nullptr;
      manager_->Deallocate(global_);
      global_ = nullptr;
    }
  };

  inline auto name() const -> const std::string & { return name_; }
  inline auto runtime() { return runtime_; }
  inline auto manager() { return manager_; }
  inline auto global_env() { return global_; }

  auto InternSymbol(std::string &&lexeme) -> types::LishpSymbol *;
  inline auto InternSymbol(const std::string &lexeme) {
    std::string copy = lexeme;
    return InternSymbol(std::move(copy));
  }

  inline auto BindValue(types::LishpSymbol *sym, types::LishpForm form) {
    global_->BindValue(sym, form);
  }

  inline auto BindFunction(types::LishpSymbol *sym,
                           types::LishpFunction *func) {
    global_->BindFunction(sym, func);
  }

  // TODO: Question--Do I even need these functions? Or should I just go
  // through the global environment when I need anything? Maybe leave these here
  // just as convenience fucntions?

  auto SymbolValue(types::LishpSymbol *sym) -> types::LishpForm;
  auto SymbolFunction(types::LishpSymbol *sym) -> types::LishpFunction *;

  // utility inlines
  inline auto SymbolValue(std::string &&lexeme) {
    types::LishpSymbol *sym = InternSymbol(std::move(lexeme));
    return SymbolValue(sym);
  }

  inline auto SymbolFunction(std::string &&lexeme) {
    types::LishpSymbol *sym = InternSymbol(std::move(lexeme));
    return SymbolFunction(sym);
  }

  auto MarkUsed() -> void {
    if (color != kWhite) {
      // already marked
      return;
    }

    color = kGrey;

    for (auto &sym_pair : interned_symbols_) {
      sym_pair.second->MarkUsed();
    }

    global_->MarkUsed();
  }

private:
  std::string name_;
  std::map<std::string, types::LishpSymbol *> interned_symbols_;

  LishpRuntime *runtime_;
  memory::MemoryManager *manager_;
  environment::Environment *global_;
};

auto BuildSystemPackage(LishpRuntime *runtime, memory::MemoryManager *manager)
    -> Package *;
auto BuildUserPackage(LishpRuntime *runtime, memory::MemoryManager *manager)
    -> Package *;
auto BuildDefaultReadtable(Package *package) -> types::LishpReadtable *;

} // namespace runtime

#endif
