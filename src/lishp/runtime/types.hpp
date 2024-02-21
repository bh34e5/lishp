#ifndef runtime_types_h
#define runtime_types_h

#include <assert.h>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "../memory/memory.hpp"

// forward declaration...
// TODO: try to figure out if I can remove the cyclical dependency
namespace environment {
class Environment;
}

namespace runtime {
class Package;
}

namespace types {

struct LishpObject : public memory::MarkedObject {
  enum Types {
    kCons,
    kString,
    kSymbol,
    kFunction,
    kReadtable,
    kStream,
  };

  LishpObject(Types type) : MarkedObject(kLishpObject), type(type) {}

  Types type;

  template <typename T,
            std::enable_if_t<std::is_base_of_v<LishpObject, T>, int> = 0>
  inline auto As() {
    return static_cast<T *>(this);
  }

  auto MarkUsed() -> void;
};

struct LishpForm {
  enum Types {
    kFixnum,
    kChar,
    kNil,
    kObject,
    kT,
  };

  Types type;
  union {
    std::uint32_t fixnum;
    char ch;
    LishpObject *object;
  };

  static inline auto Nil() { return LishpForm{kNil, {.fixnum = 0}}; }
  static inline auto T() { return LishpForm{kT, {.fixnum = 0}}; }
  static inline auto FromFixnum(uint32_t fixnum) {
    return LishpForm{kFixnum, {.fixnum = fixnum}};
  }
  static inline auto FromChar(char ch) { return LishpForm{kChar, {.ch = ch}}; }
  static inline auto FromObj(LishpObject *obj) {
    return LishpForm{kObject, {.object = obj}};
  }

  template <typename T> inline auto AssertAs() -> T *;

  inline auto NilP() { return type == kNil && fixnum == 0; }
  inline auto TP() { return type == kT && fixnum == 0; }

  inline auto ConsP() {
    return type == kObject && object->type == LishpObject::kCons;
  };
  inline auto AtomP() { return !ConsP(); }

  auto ToString() -> std::string;

  friend bool operator<(const LishpForm &lhs, const LishpForm &rhs) {
    if (lhs.type != rhs.type) {
      return lhs.type - rhs.type;
    }

    switch (lhs.type) {
    case LishpForm::kNil:
    case LishpForm::kT:
      return true;
    case LishpForm::kChar:
      return lhs.ch < rhs.ch;
    case LishpForm::kFixnum:
      return lhs.fixnum - rhs.fixnum;
    case LishpForm::kObject:
      return lhs.object < rhs.object;
    }
  }
};

struct LishpCons : public LishpObject {
  LishpCons(LishpForm car, LishpForm cdr)
      : LishpObject(kCons), car(car), cdr(cdr) {}

  auto MarkUsed() -> void;

  LishpForm car;
  LishpForm cdr;
};

template <typename T> inline auto LishpForm::AssertAs() -> T * {
  assert(type == kObject);
  T *obj = object->As<T>();
  return obj;
}

// A list is not an object, I'm using it as a wrapper for either NIL or a
// cons, so that functions that expect a list, can recieve either a cons or
// nil
struct LishpList {
  bool nil;
  LishpCons *cons;

  static inline auto Nil() { return LishpList{true, nullptr}; }
  static inline auto Of(LishpCons *cons) { return LishpList{false, cons}; }

  inline auto first() {
    if (nil) {
      return LishpForm::Nil();
    }
    return cons->car;
  }

  inline auto rest() {
    if (nil) {
      return *this;
    }

    LishpForm cdr = cons->cdr;
    if (cdr.NilP()) {
      return LishpList::Nil();
    }

    assert(cdr.type == LishpForm::Types::kObject);

    LishpObject *obj = cdr.object;
    assert(obj->type == LishpObject::Types::kCons);

    LishpCons *cdr_cons = obj->As<LishpCons>();
    return LishpList::Of(cdr_cons);
  }

  inline auto to_form() {
    if (nil) {
      return LishpForm::Nil();
    }
    return LishpForm::FromObj(cons);
  }

  template <typename T>
  static inline auto Push(memory::MemoryManager *manager, T first,
                          LishpList rest) {
    LishpForm rest_form = rest.to_form();
    LishpForm first_form = ToLishpForm(first);
    LishpCons *new_cons = manager->Allocate<LishpCons>(first_form, rest_form);

    return LishpList::Of(new_cons);
  }

  template <typename Arg, typename... Args>
  static auto Build(memory::MemoryManager *manager, Arg first, Args... rest) {
    LishpList rest_list = Build(manager, rest...);
    return LishpList::Push(manager, first, rest_list);
  }

  template <typename Arg>
  static inline auto Build(memory::MemoryManager *manager, Arg first) {
    LishpList rest_list = LishpList::Nil();
    return LishpList::Push(manager, first, rest_list);
  }

  inline auto Reverse(memory::MemoryManager *manager) {
    return ReverseR(manager, *this, LishpList::Nil());
  }

private:
  static inline auto ToLishpForm(LishpForm arg) -> LishpForm { return arg; }

  static inline auto ToLishpForm(LishpObject *arg) -> LishpForm {
    return LishpForm::FromObj(arg);
  }

  static auto ReverseR(memory::MemoryManager *manager, LishpList target,
                       LishpList cur) -> LishpList {
    if (target.nil) {
      return cur;
    }
    return ReverseR(manager, target.rest(), Push(manager, target.first(), cur));
  }
};

struct LishpString : public LishpObject {
  LishpString(std::string &&lexeme)
      : LishpObject(kString), lexeme(std::move(lexeme)) {}

  auto MarkUsed() -> void;

  std::string lexeme;
};

struct LishpSymbol : public LishpObject {
  LishpSymbol(const std::string &lexeme, runtime::Package *package)
      : LishpObject(kSymbol), lexeme(lexeme), package(package) {}

  auto MarkUsed() -> void;

  std::string lexeme;
  runtime::Package *package;
};

struct LishpFunctionReturn {
  enum Types {
    kGo,
    kValues,
  };

  LishpFunctionReturn(std::vector<LishpForm> &&values)
      : type(kValues), go_tag(LishpForm::Nil()), values(std::move(values)) {}
  LishpFunctionReturn(LishpForm go_tag) : type(kGo), go_tag(go_tag) {}
  LishpFunctionReturn(LishpFunctionReturn &&other) {
    std::swap(type, other.type);
    std::swap(go_tag, other.go_tag);
    std::swap(values, other.values);
  }

  Types type;

  LishpForm go_tag;
  std::vector<LishpForm> values;

  static inline auto FromValues() {
    return LishpFunctionReturn(std::vector<LishpForm>{});
  }

  static inline auto FromValues(LishpForm args...) {
    return LishpFunctionReturn(std::vector<LishpForm>{args});
  }

  static inline auto FromGo(LishpForm go_tag) {
    return LishpFunctionReturn(go_tag);
  }
};

typedef LishpFunctionReturn (*InherentFunctionPtr)(
    environment::Environment *closure, environment::Environment *lexical,
    LishpList &args);

typedef LishpFunctionReturn (*SpecialFormPtr)(environment::Environment *lexical,
                                              LishpList &args);

struct LishpFunction : public LishpObject {
  enum FuncTypes {
    kInherent,
    kSpecialForm,
    kUserDefined,
  };

  LishpFunction(environment::Environment *closure, LishpList args,
                LishpList body)
      : LishpObject(kFunction), function_type(kUserDefined), closure(closure),
        user_defined({args, body}) {}
  LishpFunction(environment::Environment *closure, InherentFunctionPtr inherent)
      : LishpObject(kFunction), function_type(kInherent), closure(closure),
        inherent(inherent) {}
  LishpFunction(SpecialFormPtr special_form)
      : LishpObject(kFunction), function_type(kSpecialForm), closure(nullptr),
        special_form(special_form) {}

  auto MarkUsed() -> void;

  FuncTypes function_type;
  environment::Environment *closure;
  union {
    InherentFunctionPtr inherent;
    SpecialFormPtr special_form;
    struct {
      LishpList args;
      LishpList body;
    } user_defined;
  };

  auto Call(environment::Environment *lexical, LishpList &args)
      -> LishpFunctionReturn;
};

struct LishpReadtable : public LishpObject {
  enum Case {
    kUpcase,
    kDowncase,
    kPreserve,
    kInvert,
  };

  LishpReadtable(Case readcase) : LishpObject(kReadtable), readcase(readcase) {}
  LishpReadtable() : LishpObject(kReadtable), readcase(kUpcase) {}

  auto MarkUsed() -> void;

  static inline auto CasedChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  }

  static inline auto CharCase(char c) {
    // Assumes the input is a cased character
    if (c >= 'A' && c <= 'Z') {
      return kUpcase;
    }
    return kDowncase;
  }

  static inline auto ToUpper(char c) {
    if (c >= 'a' && c <= 'z') {
      return (char)(c - 'a' + 'A');
    }
    return c;
  }

  static inline auto ToLower(char c) {
    if (c >= 'A' && c <= 'Z') {
      return (char)(c - 'A' + 'a');
    }
    return c;
  }

  inline auto ConvertCase(char c) {
    if (!CasedChar(c)) {
      return c;
    }

    switch (readcase) {
    case kUpcase:
      return ToUpper(c);
    case kDowncase:
      return ToLower(c);
    case kPreserve:
      return c;
    case kInvert:
      switch (CharCase(c)) {
      case kUpcase:
        return ToLower(c);
      case kDowncase:
        return ToUpper(c);
      default:
        assert(0 && "Unreachable");
      }
    }
  }

  inline auto InstallMacroChar(char c, LishpFunction *macro_func) -> void {
    reader_macro_functions.insert({c, macro_func});
  }

  std::map<char, LishpFunction *> reader_macro_functions;
  Case readcase;
};

struct LishpStream : public LishpObject {
  enum StreamType {
    kInput,
    kOutput,
    kInputOutput,
  };

  LishpStream(StreamType stream_type)
      : LishpObject(kStream), stream_type(stream_type) {}

  auto MarkUsed() -> void;

  StreamType stream_type;
};

struct LishpIStream : public LishpStream {
  LishpIStream(std::istream &stm) : LishpStream(kInput), stm(stm) {}

  std::istream &stm;

protected:
  LishpIStream(StreamType stream_type, std::istream &stm)
      : LishpStream(stream_type), stm(stm) {}
};

struct LishpOStream : public LishpStream {
  LishpOStream(std::ostream &stm) : LishpStream(kOutput), stm(stm) {}

  std::ostream &stm;

protected:
  LishpOStream(StreamType stream_type, std::ostream &stm)
      : LishpStream(stream_type), stm(stm) {}
};

// TODO: pray this doesn't break. I don't really know how to deal with diamond
// pattern...
struct LishpIOStream : public LishpIStream, public LishpOStream {
  LishpIOStream(std::iostream &stm)
      : LishpIStream(kInputOutput, stm), LishpOStream(kInputOutput, stm),
        stm(stm) {}

  std::iostream &stm;
};

} // namespace types

#endif
