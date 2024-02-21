#ifndef runtime_h
#define runtime_h

#include <assert.h>
#include <vector>

#include "package.hpp"

namespace runtime {

class LishpRuntime {
public:
  LishpRuntime() : manager_([this]() { MarkUsedObjects(); }) { Initialize(); }
  LishpRuntime(const LishpRuntime &other) = delete;
  LishpRuntime(LishpRuntime &&other) = delete;
  ~LishpRuntime() { Uninitialize(); }

  auto Repl() -> void;
  auto FindPackageByName(const std::string &name) -> Package *;
  inline auto CollectGarbage() { manager_.RunGC(); }

private:
  auto Initialize() -> void;
  auto Uninitialize() -> void;

  auto InitializePackage(Package *package) -> void;
  auto MarkUsedObjects() -> void;

  types::LishpReadtable *default_readtable_;

  memory::MemoryManager manager_;
  std::vector<Package *> packages_;
}; // namespace runtime

} // namespace runtime

#endif
