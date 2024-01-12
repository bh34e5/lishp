#ifndef lishp_types_h
#define lishp_types_h

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
};

/*-----------*
 * All Forms *
 *-----------*/

enum LispFormType {
  FormNil,
  FormNumber,
  FormObj,
};

struct LispForm {
  LispFormType type;
  union {
    long number;
    LispObj *obj;
  } as;

  static LispForm nil;
};

/*--------------------------------*
 * Full declarations of obj types *
 *--------------------------------*/

struct LispCons : public LispObj {
  bool nil;
  LispForm car;
  LispForm cdr;

  auto rest() -> LispCons;
};

class LishpRuntime;
typedef LispForm(PrimitiveFunction)(LishpRuntime *rt, LispCons args);

struct LispFunction : public LispObj {
  bool primitive;
  LispCons *argsDecl;
  union {
    struct {
      LispCons *args;
      LispCons *definition;
    } declared;
    PrimitiveFunction *inherent;
  } body;

  auto call(LishpRuntime *rt, LispCons args) -> LispForm;
};

struct LispString : public LispObj {
  std::string str;
};

struct LispSymbol : public LispObj {
  std::string lexeme;

  friend auto operator<(const LispSymbol &l, const LispSymbol &r) -> bool;
};

inline auto operator<(const LispSymbol &l, const LispSymbol &r) -> bool {
  return l.lexeme < r.lexeme;
}

#endif
