#include <iostream>
#include <type_traits>

#include "../lishp.hpp"
#include "../primitives.hpp"
#include "exceptions.hpp"

template <typename L, typename R,
          std::enable_if_t<std::is_convertible_v<L *, LispObj *>, bool> = true,
          std::enable_if_t<std::is_convertible_v<R *, LispObj *>, bool> = true>
static auto make_obj_cons(L *l, R *r) -> LispCons {
  return {{LispObjType::ObjCons},
          false,
          {LispFormType::FormObj, {.obj = (LispObj *)l}},
          {LispFormType::FormObj, {.obj = (LispObj *)r}}};
}

template <typename T, std::enable_if_t<std::is_same_v<T, LispForm> ||
                                           std::is_convertible_v<T, LispObj *>,
                                       bool> = true>
static auto make_obj_form(T t) -> LispForm {
  return {LispFormType::FormObj, {.obj = (LispObj *)t}};
}

template <> auto make_obj_form<LispForm>(LispForm t) -> LispForm { return t; }

template <typename First, typename... Rest>
static auto build_cons(First first, Rest... forms) -> LispCons * {
  LispForm firstForm = make_obj_form(first);

  LispCons *rest = build_cons<Rest...>(forms...);
  LispForm restForm = make_obj_form(rest);

  return new LispCons{
      {LispObjType::ObjCons}, false, std::move(firstForm), std::move(restForm)};
}

template <typename First> static auto build_cons(First first) -> LispCons * {
  LispForm firstForm = make_obj_form(first);
  return new LispCons{
      {LispObjType::ObjCons}, false, std::move(firstForm), LispForm::nil};
}

auto LishpRuntime::repl() -> void {
  std::cout << "Called repl" << std::endl;

  initialize_primitives();

  std::string f = "FORMAT";
  LispSymbol *fs = intern_symbol(f);

  std::string t = "T";
  LispSymbol *ts = intern_symbol(t);

  std::string output = "Testing the output from running eval\n";
  LispString os{{LispObjType::ObjString}, std::move(output)};
  LispForm osf = {LispFormType::FormObj, {.obj = &os}};

  LispCons *firstRun = build_cons(fs, ts, osf);
  LispForm rc = {LispFormType::FormObj, {.obj = firstRun}};

  // FIXME: eventually this will turn into something like
  // (loop
  //   (format t "> ")
  //   (print (eval (read))))
  run_program({rc});
}

auto LishpRuntime::lookup_function(LispSymbol *sym) -> LispFunction {
  using it_t = decltype(this->function_assocs_)::iterator;

  it_t map_it = this->function_assocs_.find(sym);
  if (map_it == this->function_assocs_.end()) {
    throw RuntimeException(
        "Unable to find function associated with the symbol");
  }
  return map_it->second;
}

auto LishpRuntime::run_program(std::vector<LispForm> forms) -> void {
  using it_t = decltype(forms)::iterator;

  it_t form_it = forms.begin();

  while (form_it != forms.end()) {
    LispCons *args = build_cons(*form_it);
    eval(this, *args);

    form_it = std::next(form_it);
  }
}

auto LishpRuntime::initialize_primitives() -> void {
  LispCons *argsDecl = nullptr;

  std::string consStr = "CONS";
  std::string formatStr = "FORMAT";
  std::string readStr = "READ";

  std::string carStr = "CAR";
  std::string cdrStr = "CDR";
  std::string streamStr = "STREAM";
  std::string formatStringStr = "FORMAT-STRING";
  std::string ampRestStr = "&REST";
  std::string restStr = "REST";

  LispSymbol *consSym = intern_symbol(consStr);
  LispSymbol *formatSym = intern_symbol(formatStr);
  LispSymbol *readSym = intern_symbol(readStr);

  LispSymbol *car = intern_symbol(carStr);
  LispSymbol *cdr = intern_symbol(cdrStr);
  LispSymbol *stream_ = intern_symbol(streamStr);
  LispSymbol *formatString = intern_symbol(formatStringStr);
  LispSymbol *ampRest = intern_symbol(ampRestStr);
  LispSymbol *rest = intern_symbol(restStr);

  argsDecl = build_cons(car, cdr);
  define_primitive_function(consSym, argsDecl, cons);

  argsDecl = build_cons(stream_, formatString, ampRest, rest);
  define_primitive_function(formatSym, argsDecl, format_);

  argsDecl = new LispCons{{LispObjType::ObjCons}, true, {}, {}};
  define_primitive_function(readSym, argsDecl, read);
}

auto LishpRuntime::intern_symbol(std::string &lexeme) -> LispSymbol * {
  using it_t = decltype(interned_symbols_)::iterator;

  it_t map_it = interned_symbols_.begin();
  while (map_it != interned_symbols_.end()) {
    if (map_it->first == lexeme) {
      return map_it->second;
    }
    map_it = std::next(map_it);
  }

  LispSymbol *interned = new LispSymbol{{LispObjType::ObjSymbol}, lexeme};
  interned_symbols_.insert(map_it, {lexeme, interned});

  return interned;
}
