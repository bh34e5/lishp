#ifndef environment_h
#define environment_h

#include <map>
#include <set>

#include "types.hpp"

namespace runtime {
class Package;
}

namespace environment {

class Environment {
public:
  Environment() = delete;
  Environment(runtime::Package *package) : package_(package) {}
  Environment(runtime::Package *package, Environment *parent)
      : parent_(parent), package_(package) {}
  Environment(const Environment &other) = delete;
  Environment(Environment &&other) = delete;
  ~Environment() = default;

  auto SymbolValue(types::LishpSymbol *sym) -> types::LishpForm;
  auto SymbolFunction(types::LishpSymbol *sym) -> types::LishpFunction *;

  inline auto BindValue(types::LishpSymbol *sym, types::LishpForm value) {
    BindValue(sym, value, true);
  }

  inline auto BindFunction(types::LishpSymbol *sym,
                           types::LishpFunction *func) {
    BindFunction(sym, func, true);
  }

  inline auto package() { return package_; }

private:
  auto BindValue(types::LishpSymbol *sym, types::LishpForm value,
                 bool bind_here) -> bool;
  auto BindFunction(types::LishpSymbol *sym, types::LishpFunction *value,
                    bool bind_here) -> bool;

  std::map<types::LishpSymbol *, types::LishpForm> symbol_values_;
  std::map<types::LishpSymbol *, types::LishpFunction *> symbol_functions_;

  std::map<types::LishpSymbol *, types::LishpList> tag_markers_;
  std::set<types::LishpSymbol *> dynamic_symbols_;

  Environment *parent_;
  runtime::Package *package_;
};

} // namespace environment

#endif
