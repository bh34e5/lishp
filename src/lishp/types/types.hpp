#ifndef lishp_types_h
#define lishp_types_h

#include <functional>
#include <string>

/*-----------------*
 * Dynamic Objects *
 *-----------------*/

enum LispObjType {
  ObjCons,
  ObjFunction,
  ObjString,
  ObjSymbol,
};

struct LispObj {
  LispObjType type;

  auto to_string() -> std::string;
};

/*-----------*
 * All Forms *
 *-----------*/

enum LispFormType {
  FormNil,
  FormT,
  FormNumber,
  FormObj,
};

struct LispForm {
  LispFormType type;
  union {
    long number;
    LispObj *obj;
  } as;

  static auto nil() -> LispForm {
    return LispForm{LispFormType::FormNil, {.obj = NULL}};
  }

  static auto t() -> LispForm {
    return LispForm{LispFormType::FormT, {.obj = NULL}};
  }

  auto is_nil() -> bool;
  auto is_t() -> bool;
  auto to_string() -> std::string;
};

/*--------------------------------*
 * Full declarations of obj types *
 *--------------------------------*/

struct LispCons : public LispObj {
  bool nil;
  LispForm car;
  LispForm cdr;

  auto rest() -> LispCons *;
  auto list_ish_p() -> bool;
  auto for_each(std::function<void(LispCons *)> &func) -> void;
  auto to_string() -> std::string;
};

class LishpRuntime;
typedef LispForm(PrimitiveFunction)(LishpRuntime *rt, LispCons args);

struct LispFunction : public LispObj {
  bool primitive;
  LispCons *args_decl;
  union {
    struct {
      LispCons *args;
      LispCons *definition;
    } declared;
    PrimitiveFunction *inherent;
  } body;

  auto call(LishpRuntime *rt, LispCons args) -> LispForm;
  auto to_string() -> std::string;
};

struct LispString : public LispObj {
  std::string str;
  auto to_string() -> std::string;
};

struct LispSymbol : public LispObj {
  std::string lexeme;

  friend auto operator<(const LispSymbol &l, const LispSymbol &r) -> bool;
  auto to_string() -> std::string;
};

inline auto operator<(const LispSymbol &l, const LispSymbol &r) -> bool {
  return l.lexeme < r.lexeme;
}

#endif
