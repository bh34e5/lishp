#ifndef lishp_h
#define lishp_h

#include <string>

namespace lishp {

auto RuntimeRepl() -> void;
auto CompileFile(const std::string &filename) -> void;

} // namespace lishp

#endif
