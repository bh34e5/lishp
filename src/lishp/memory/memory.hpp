#ifndef memory_h
#define memory_h

#include <cstdio>
#include <iostream>
#include <vector>

#ifdef DEBUG_MEMORY
#include <type_traits>

template <typename T> struct writer {
  static auto DoWrite(std::ostream &stm, const T &arg) {
    DoWriteImpl<T>(stm, arg, 0);
  }

private:
  template <typename U>
  static auto DoWriteImpl(std::ostream &stm, const U &arg, int)
      -> decltype(stm << arg, void()) {
    stm << arg;
  }

  template <typename U>
  static auto DoWriteImpl(std::ostream &stm, const U &arg, long)
      -> decltype(stm << &arg, void()) {
    stm << "<Unprintable>";
  }
};

static auto print_args_r(bool first) {
  if (first) {
    std::cout << "<NO ARGS>";
  }
}

template <typename Arg, typename... Args>
static auto print_args_r(bool first, const Arg &arg, const Args &...args) {
  if (!first) {
    std::cout << ", ";
  }

  writer<Arg>::DoWrite(std::cout, arg);
  print_args_r(false, args...);
}

template <typename... Args> static auto print_args(const Args &...args) {
  std::cout << "Allocating with args: ";
  print_args_r(true, args...);
  std::cout << std::endl;
}
#endif

namespace memory {

class MemoryManager {
public:
  MemoryManager() = default;
  MemoryManager(const MemoryManager &other) = delete;
  MemoryManager(MemoryManager &&other) = delete;
  ~MemoryManager() {
#ifdef DEBUG_MEMORY
    if (allocated_objs_.size() != 0) {
      std::cerr << "Destroying MemoryManager with objects that haven't been "
                   "deallocated: "
                << allocated_objs_.size() << " objects still allocated."
                << std::endl;
    }
#endif
  }

  template <typename R, typename... Args> inline auto Allocate(Args &&...args) {
#ifdef DEBUG_MEMORY
    print_args(std::forward<Args>(args)...);
#endif

    R *obj = new R(std::forward<Args>(args)...);
    allocated_objs_.push_back(obj);

    return obj;
  }

  template <typename R> inline auto Deallocate(R *obj) {
#ifdef DEBUG_MEMORY
    std::cout << "Deallocating " << obj << std::endl;
#endif
    using it_t = decltype(allocated_objs_)::const_iterator;

    it_t alloc_obj_it = allocated_objs_.cbegin();
    while (alloc_obj_it != allocated_objs_.cend()) {
      if (*alloc_obj_it == (void *)obj) {
        break;
      }
      alloc_obj_it = std::next(alloc_obj_it);
    }

    if (alloc_obj_it == allocated_objs_.cend()) {
      std::printf("Invalid pointer received: (0x%p)\n", (void *)obj);
      throw std::invalid_argument("Object was not allocated with this manager");
    }

    allocated_objs_.erase(alloc_obj_it);
    delete obj;
  }

  inline auto GetObjects() const { return allocated_objs_.cbegin(); }

private:
  std::vector<void *> allocated_objs_;
};

} // namespace memory

#endif
