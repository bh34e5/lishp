#ifndef environment_h
#define environment_h

#include <map>
#include <set>

#include "types.hpp"

namespace runtime {
class Package;
}

namespace environment {

class Environment : public memory::MarkedObject {
public:
  Environment() = delete;
  Environment(runtime::Package *package)
      : memory::MarkedObject(kEnvironment), package_(package) {}
  Environment(runtime::Package *package, Environment *parent)
      : memory::MarkedObject(kEnvironment), parent_(parent), package_(package) {
  }
  Environment(const Environment &other) = delete;
  Environment(Environment &&other) = delete;
  ~Environment() = default;

  auto SymbolValue(types::LishpSymbol *sym) -> types::LishpForm;
  auto SymbolFunction(types::LishpSymbol *sym) -> types::LishpFunction *;
  auto MarkTag(types::LishpForm form, types::LishpList head) -> void;
  auto FindTag(types::LishpForm form, types::LishpList *res) -> bool;

  inline auto BindValue(types::LishpSymbol *sym, types::LishpForm value) {
    BindValue(sym, value, true);
  }

  inline auto BindFunction(types::LishpSymbol *sym,
                           types::LishpFunction *func) {
    BindFunction(sym, func, true);
  }

  inline auto package() { return package_; }

  auto MarkUsed() -> void {
    if (color != kWhite) {
      // already marked
      return;
    }

    color = kGrey;

    for (auto &sym_val_pair : symbol_values_) {
      sym_val_pair.first->MarkUsed();

      types::LishpForm form = sym_val_pair.second;
      if (form.type == types::LishpForm::kObject) {
        form.object->MarkUsed();
      }
    }

    for (auto &sym_func_pair : symbol_functions_) {
      sym_func_pair.first->MarkUsed();

      types::LishpFunction *func = sym_func_pair.second;
      func->MarkUsed();
    }

    for (auto &sym_val_pair : tag_markers_) {
      types::LishpForm form = sym_val_pair.first;
      if (form.type == types::LishpForm::kObject) {
        form.object->MarkUsed();
      }

      if (!sym_val_pair.second.nil) {
        sym_val_pair.second.cons->MarkUsed();
      }
    }

    for (types::LishpSymbol *sym : dynamic_symbols_) {
      sym->MarkUsed();
    }

    if (parent_ != nullptr) {
      parent_->MarkUsed();
    }
  }

private:
  auto BindValue(types::LishpSymbol *sym, types::LishpForm value,
                 bool bind_here) -> bool;
  auto BindFunction(types::LishpSymbol *sym, types::LishpFunction *value,
                    bool bind_here) -> bool;

  std::map<types::LishpSymbol *, types::LishpForm> symbol_values_;
  std::map<types::LishpSymbol *, types::LishpFunction *> symbol_functions_;

  std::map<types::LishpForm, types::LishpList> tag_markers_;
  std::set<types::LishpSymbol *> dynamic_symbols_;

  Environment *parent_;
  runtime::Package *package_;
};

} // namespace environment

#endif
