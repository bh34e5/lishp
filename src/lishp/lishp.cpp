#ifndef lishp_h
#define lishp_h

#include <assert.h>
#include <string>

#include "runtime/runtime.hpp"

namespace lishp {

auto RuntimeRepl() -> void {
  runtime::LishpRuntime rt;
  rt.Repl();
}

auto CompileFile(const std::string &filename) -> void {
  (void)filename;
  assert(0 && "Unimplemented: lishp::CompileFile");
}

} // namespace lishp

#endif
