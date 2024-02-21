#include "memory.hpp"
#include "../runtime/environment.hpp"
#include "../runtime/package.hpp"
#include "../runtime/types.hpp"

namespace memory {

auto MarkedObject::MarkUsed() -> void {
  switch (type) {
  case kPackage: {
    static_cast<runtime::Package *>(this)->MarkUsed();
  } break;
  case kEnvironment: {
    static_cast<environment::Environment *>(this)->MarkUsed();
  } break;
  case kLishpObject: {
    static_cast<types::LishpObject *>(this)->MarkUsed();
  } break;
  }
}

auto MemoryManager::RunGC() -> void {
#ifdef DEBUG_MEMORY
  std::cout << std::endl << "Running GC" << std::endl;
#endif
  for (MarkedObject *marked_obj : allocated_objs_) {
    marked_obj->color = MarkedObject::kWhite;
  }

  mark_first_();

  std::vector<MarkedObject *> to_free;

  using it_t = decltype(allocated_objs_)::iterator;
  it_t cur_obj = allocated_objs_.begin();
  while (cur_obj != allocated_objs_.end()) {
    if ((*cur_obj)->color == MarkedObject::kWhite) {
      to_free.push_back(*cur_obj);
      cur_obj = allocated_objs_.erase(cur_obj);
    } else {
      cur_obj = std::next(cur_obj);
    }
  }

  for (MarkedObject *obj : to_free) {
    DestroyObject(obj);
  }
}

static auto DestroyLishpObject(types::LishpObject *obj) {
  switch (obj->type) {
  case types::LishpObject::kCons: {
    delete static_cast<types::LishpCons *>(obj);
  } break;
  case types::LishpObject::kStream: {
    delete static_cast<types::LishpStream *>(obj);
  } break;
  case types::LishpObject::kString: {
    delete static_cast<types::LishpString *>(obj);
  } break;
  case types::LishpObject::kSymbol: {
    delete static_cast<types::LishpSymbol *>(obj);
  } break;
  case types::LishpObject::kFunction: {
    delete static_cast<types::LishpFunction *>(obj);
  } break;
  case types::LishpObject::kReadtable: {
    delete static_cast<types::LishpReadtable *>(obj);
  } break;
  }
}

auto MemoryManager::DestroyObject(MarkedObject *obj) -> void {
#ifdef DEBUG_MEMORY
  std::cout << "Destroying ptr " << obj << std::endl;
#endif

  allocated_bytes_ -= obj->size;

  switch (obj->type) {
  case MarkedObject::kPackage: {
    runtime::Package *package = static_cast<runtime::Package *>(obj);
    delete package;
  } break;
  case MarkedObject::kEnvironment: {
    environment::Environment *environment =
        static_cast<environment::Environment *>(obj);
    delete environment;
  } break;
  case MarkedObject::kLishpObject: {
    types::LishpObject *lishp_object = static_cast<types::LishpObject *>(obj);
    DestroyLishpObject(lishp_object);
  } break;
  }
}

} // namespace memory
