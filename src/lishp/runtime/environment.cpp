#include <assert.h>

#include "environment.hpp"

namespace environment {

auto Environment::SymbolValue(types::LishpSymbol *sym) -> types::LishpForm {
  using it_t = decltype(symbol_values_)::iterator;

  it_t sym_it = symbol_values_.begin();
  while (sym_it != symbol_values_.end()) {
    if (sym_it->first == sym) {
      return sym_it->second;
    }
    sym_it = std::next(sym_it);
  }

  // if parent is not null, check it
  if (parent_ != nullptr) {
    return parent_->SymbolValue(sym);
  }

  // TODO: turn this into an exception
  assert(0 && "Symbol is not bound to a value in this package.");
}

auto Environment::SymbolFunction(types::LishpSymbol *sym)
    -> types::LishpFunction * {
  using it_t = decltype(symbol_functions_)::iterator;

  it_t sym_it = symbol_functions_.begin();
  while (sym_it != symbol_functions_.end()) {
    if (sym_it->first == sym) {
      return sym_it->second;
    }
    sym_it = std::next(sym_it);
  }

  // if parent is not null, check it
  if (parent_ != nullptr) {
    return parent_->SymbolFunction(sym);
  }

  // TODO: turn this into an exception
  assert(0 && "Symbol is not bound to a value in this package.");
}

auto Environment::MarkTag(types::LishpForm form, types::LishpList head)
    -> void {
  tag_markers_.insert({form, head});
}

auto Environment::FindTag(types::LishpForm form, types::LishpList *res)
    -> bool {
  using it_t = decltype(tag_markers_)::iterator;

  it_t found = tag_markers_.find(form);
  if (found == tag_markers_.end()) {
    return false;
  }

  *res = found->second;
  return true;
}

auto Environment::BindValue(types::LishpSymbol *sym, types::LishpForm value,
                            bool bind_here) -> bool {
  using it_t = decltype(symbol_values_)::iterator;

  it_t sym_it = symbol_values_.begin();
  while (sym_it != symbol_values_.end()) {
    if (sym_it->first == sym) {
      sym_it->second = value;
      return true;
    }
    sym_it = std::next(sym_it);
  }

  bool bound_above = false;
  if (parent_ != nullptr) {
    bound_above = parent_->BindValue(sym, value, false);
  }

  if (!bound_above && bind_here) {
    symbol_values_.insert({sym, value});
    return true;
  }
  return bound_above;
}

auto Environment::BindFunction(types::LishpSymbol *sym,
                               types::LishpFunction *func, bool bind_here)
    -> bool {
  using it_t = decltype(symbol_functions_)::iterator;

  it_t sym_it = symbol_functions_.begin();
  while (sym_it != symbol_functions_.end()) {
    if (sym_it->first == sym) {
      sym_it->second = func;
      return true;
    }
    sym_it = std::next(sym_it);
  }

  bool bound_above = false;
  if (parent_ != nullptr) {
    bound_above = parent_->BindFunction(sym, func, false);
  }

  if (!bound_above && bind_here) {
    symbol_functions_.insert({sym, func});
    return true;
  }
  return bound_above;
}

} // namespace environment
