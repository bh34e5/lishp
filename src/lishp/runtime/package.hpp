#ifndef runtime_package_h
#define runtime_package_h

#include <map>
#include <string>

#include "../memory/memory.hpp"
#include "environment.hpp"
#include "types.hpp"

namespace runtime {

class Package {
public:
  Package() = delete;
  Package(std::string &&name, memory::MemoryManager *manager)
      : name_(std::move(name)), manager_(manager) {
    global_ = manager->Allocate<environment::Environment>(this);
  }
  Package(const Package &other) = delete;
  Package(Package &&other) {
    std::swap(name_, other.name_);
    std::swap(interned_symbols_, other.interned_symbols_);
    manager_ = other.manager_;
    global_ = other.global_;

    other.manager_ = nullptr;
    other.global_ = nullptr;
  };
  ~Package() {
    if (manager_ != nullptr && global_ != nullptr) {
      manager_->Deallocate(global_);
      global_ = nullptr;
    }
  };

  inline auto name() const -> const std::string & { return name_; }
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

private:
  std::string name_;
  std::map<std::string, types::LishpSymbol *> interned_symbols_;

  memory::MemoryManager *manager_;
  environment::Environment *global_;
};

auto BuildSystemPackage(memory::MemoryManager *manager) -> Package *;
auto BuildUserPackage(memory::MemoryManager *manager) -> Package *;
auto BuildSystemReadtable(memory::MemoryManager *manager)
    -> types::LishpReadtable *;

} // namespace runtime

#endif
