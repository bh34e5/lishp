#ifndef runtime_h
#define runtime_h

#include <assert.h>
#include <vector>

#include "package.hpp"

namespace runtime {

class LishpRuntime {
public:
  LishpRuntime() { Initialize(); }
  LishpRuntime(const LishpRuntime &other) = delete;
  LishpRuntime(LishpRuntime &&other) = delete;
  ~LishpRuntime() { Uninitialize(); }

  auto Repl() -> void;
  auto FindPackageByName(const std::string &name) -> Package *;

private:
  auto Initialize() -> void;
  auto Uninitialize() -> void;

  auto InitializePackage(Package *package) -> void;

  types::LishpReadtable *default_readtable_;

  // free manager_ before free packages_...
  memory::MemoryManager manager_;
  std::vector<Package *> packages_;
}; // namespace runtime

} // namespace runtime

#endif
