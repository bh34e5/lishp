#include "analyzer.hpp"
#include "../evaluation.hpp"

namespace bytecode {

auto Analyzer::TransformForm(types::LishpForm &form) -> std::vector<Bytecode> {
  std::vector<Bytecode> result;
  switch (form.type) {
  case types::LishpForm::kT:
  case types::LishpForm::kNil:
  case types::LishpForm::kChar:
  case types::LishpForm::kFixnum: {
    // self-evaluating forms; just push it onto the stack
    result.push_back({Bytecode::Type::kPush, {.form = form}});
  } break;
  case types::LishpForm::kObject: {
    result = TransformObject(form.object);
  } break;
  }
  return result;
}

auto Analyzer::TransformObject(types::LishpObject *obj)
    -> std::vector<Bytecode> {
  std::vector<Bytecode> result;
  switch (obj->type) {
  case types::LishpObject::kStream:
  case types::LishpObject::kString:
  case types::LishpObject::kSymbol:
  case types::LishpObject::kFunction:
  case types::LishpObject::kReadtable: {
    // self-evaluating forms; just push it onto the stack
    result.push_back(
        {Bytecode::Type::kPush, {.form = types::LishpForm::FromObj(obj)}});
  } break;
  case types::LishpObject::kCons: {
    result = TransformCons(obj->As<types::LishpCons>());
  } break;
  }
  return result;
}

auto Analyzer::TransformCons(types::LishpCons *cons) -> std::vector<Bytecode> {
  std::vector<Bytecode> result;

  types::LishpForm first = cons->car;
  switch (first.type) {
  case types::LishpForm::kT:
  case types::LishpForm::kNil:
  case types::LishpForm::kChar:
  case types::LishpForm::kFixnum: {
    // invalid car...
  } break;
  case types::LishpForm::kObject: {
    types::LishpObject *obj = first.object;
    switch (obj->type) {
    case types::LishpObject::kStream:
    case types::LishpObject::kString:
    case types::LishpObject::kFunction:
    case types::LishpObject::kReadtable: {
      // invalid car...
    } break;
    case types::LishpObject::kCons: {
      // check it's a lambda expression
    } break;
    case types::LishpObject::kSymbol: {
      // check special form, macro call, function call
      types::LishpSymbol *sym = obj->As<types::LishpSymbol>();
      if (IsSpecialForm(sym)) {
        // handle special form
        result = AnalyzeSpecialForm(sym, cons->cdr);
      } else {
        // TODO: when we start handling macros, this is where it goes?
        // handling function call, need to lookup function and then call it

        result.push_back({Bytecode::kPush, {.form = cons->cdr}});
        result.push_back(
            {Bytecode::kPush, {.form = types::LishpForm::FromObj(sym)}});
        result.push_back({Bytecode::kCallFunction});
      }
    } break;
    }
  } break;
  }

  return result;
}

auto Analyzer::AnalyzeTagbody(types::LishpForm args) -> std::vector<Bytecode> {
  std::vector<Bytecode> result;
  // TODO: impl
  return result;
}

auto Analyzer::AnalyzeGo(types::LishpForm args) -> std::vector<Bytecode> {
  std::vector<Bytecode> result;
  // TODO: impl
  return result;
}

auto Analyzer::AnalyzeQuote(types::LishpForm args) -> std::vector<Bytecode> {
  return {{Bytecode::kPush, {.form = args}}};
}

auto Analyzer::AnalyzeFunction(types::LishpForm args) -> std::vector<Bytecode> {
  std::vector<Bytecode> result;
  // TODO: impl
  return result;
}

auto Analyzer::AnalyzeProgn(types::LishpForm args) -> std::vector<Bytecode> {
  std::vector<Bytecode> result;
  // TODO: impl
  return result;
}

auto Analyzer::AnalyzeLabels(types::LishpForm args) -> std::vector<Bytecode> {
  std::vector<Bytecode> result;
  // TODO: impl
  return result;
}

auto Analyzer::AnalyzeLetStar(types::LishpForm args) -> std::vector<Bytecode> {
  std::vector<Bytecode> result;
  // TODO: impl
  return result;
}

auto Analyzer::AnalyzeSpecialForm(types::LishpSymbol *sym,
                                  types::LishpForm args)
    -> std::vector<Bytecode> {
#define ANALYZE_SPECIAL_FORM(name, handler)                                    \
  do {                                                                         \
    if (sym_lex == name) {                                                     \
      result = handler(args);                                                  \
      return result;                                                           \
    }                                                                          \
  } while (0)

  std::vector<Bytecode> result;

  const std::string &sym_lex = sym->lexeme;

  ANALYZE_SPECIAL_FORM("TAGBODY", AnalyzeTagbody);
  ANALYZE_SPECIAL_FORM("GO", AnalyzeGo);
  ANALYZE_SPECIAL_FORM("QUOTE", AnalyzeQuote);
  ANALYZE_SPECIAL_FORM("FUNCTION", AnalyzeFunction);
  ANALYZE_SPECIAL_FORM("PROGN", AnalyzeProgn);
  ANALYZE_SPECIAL_FORM("LABELS", AnalyzeLabels);
  ANALYZE_SPECIAL_FORM("LET*", AnalyzeLetStar);

  assert(0 && "Unhandled special form");
#undef ANALYZE_SPECIAL_FORM
}

} // namespace bytecode
