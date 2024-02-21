#ifndef memory_h
#define memory_h

#include <cstdio>
#include <functional>
#include <iostream>
#include <type_traits>
#include <vector>

namespace memory {

// marker class
struct MarkedObject {
  enum Type {
    kLishpObject,
    kEnvironment,
    kPackage,
  };

  enum Color {
    kWhite,
    kGrey,
    kBlack,
  };

  MarkedObject(Type type) : type(type), color(kWhite) {}

  Type type;
  Color color;
  std::size_t size;

  auto MarkUsed() -> void;
};

class MemoryManager {

#ifdef DEBUG_MEMORY
private:
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
  }
#endif

public:
  MemoryManager(std::function<void(void)> mark_first)
      : mark_first_(mark_first){};
  MemoryManager(const MemoryManager &other) = delete;
  MemoryManager(MemoryManager &&other) = delete;
  ~MemoryManager() {
#ifdef DEBUG_MEMORY
    if (allocated_objs_.size() != 0 || allocated_bytes_ != 0) {
      std::cerr << "Destroying MemoryManager with objects that haven't been "
                   "deallocated: "
                << allocated_objs_.size() << " objects still allocated."
                << std::endl
                << "Still have " << allocated_bytes_ << " bytes allocated."
                << std::endl;
    }
#endif
  }

  template <typename R,
            std::enable_if_t<std::is_base_of_v<MarkedObject, R>, bool> = true,
            typename... Args>
  inline auto Allocate(Args &&...args) {
#ifdef DEBUG_MEMORY
    print_args(std::forward<Args>(args)...);
#endif

    R *obj = new R(std::forward<Args>(args)...);

#ifdef DEBUG_MEMORY
    std::cout << "\t\tReturning " << obj << std::endl;
#endif

    std::size_t size = sizeof(R);
    allocated_bytes_ += size;

    MarkedObject *mo = (MarkedObject *)obj;
    mo->color = MarkedObject::kWhite;
    mo->size = size;
    allocated_objs_.push_back(obj);

    // if we have reached a certain capacity, then we should run the GC, and
    // free objects that aren't being referenced. to start the process, we call
    // the registered starting point. this should recursively mark all the
    // referenced objects
    // TODO: should the entry point be recursively marking objects as used, or
    // should it be marking them as gray, at which point, we loop over the list
    // and mark all the gray objects...?
    // NOTE: right now the condition is always false while the GC is still being
    // built
    if (false) {
      mark_first_();
    }

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

    allocated_bytes_ -= sizeof(R);
    delete obj;
  }

  inline auto GetObjects() const { return allocated_objs_.cbegin(); }

  auto RunGC() -> void;

private:
  auto DestroyObject(MarkedObject *obj) -> void;

  std::size_t allocated_bytes_ = 0;
  std::function<void(void)> mark_first_;
  std::vector<MarkedObject *> allocated_objs_;
};

} // namespace memory

#endif
