#include <iostream>
#include <type_traits>

#include "../lishp.hpp"
#include "../primitives/primitives.hpp"
#include "exceptions.hpp"

template <typename L, typename R,
          std::enable_if_t<std::is_convertible_v<L *, LispObj *>, bool> = true,
          std::enable_if_t<std::is_convertible_v<R *, LispObj *>, bool> = true>
static auto make_obj_cons(L *l, R *r) -> LispCons {
  return {{LispObjType::ObjCons},
          false,
          {LispFormType::FormObj, {.obj = l}},
          {LispFormType::FormObj, {.obj = r}}};
}

template <typename T, std::enable_if_t<std::is_same_v<T, LispForm> ||
                                           std::is_convertible_v<T, LispObj *>,
                                       bool> = true>
static auto make_obj_form(T t) -> LispForm {
  return {LispFormType::FormObj, {.obj = t}};
}

template <> auto make_obj_form<LispForm>(LispForm t) -> LispForm { return t; }

template <typename First, typename... Rest>
static auto build_cons(First first, Rest... forms) -> LispCons * {
  LispForm first_form = make_obj_form(first);

  LispCons *rest = build_cons<Rest...>(forms...);
  LispForm rest_form = make_obj_form(rest);

  return new LispCons{{LispObjType::ObjCons},
                      false,
                      std::move(first_form),
                      std::move(rest_form)};
}

template <typename First> static auto build_cons(First first) -> LispCons * {
  LispForm first_form = make_obj_form(first);
  return new LispCons{
      {LispObjType::ObjCons}, false, std::move(first_form), LispForm::nil()};
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

  std::string read_str = "READ";
  LispSymbol *read_sym = intern_symbol(read_str);
  LispCons *read_cons = build_cons(read_sym);
  LispCons *first_run = build_cons(fs, ts, osf, read_cons);

  LispForm rc = {LispFormType::FormObj, {.obj = first_run}};

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

auto LishpRuntime::lookup_symbol(LispSymbol *sym) -> LispForm {
  using it_t = decltype(this->symbol_assocs_)::iterator;

  it_t map_it = this->symbol_assocs_.find(sym);
  if (map_it == this->symbol_assocs_.end()) {
    std::cout << "in reading \"" << sym->lexeme << "\"" << std::endl;
    throw RuntimeException("Symbol unbound in current context");
  }
  return map_it->second;
}

auto LishpRuntime::get_reader() -> Reader & { return this->reader_; }

auto LishpRuntime::cur_readtable() -> Readtable & { return this->table_; }

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
  // functions
  LispCons *args_decl = nullptr;

  std::string cons_str = "CONS";
  std::string format_str = "FORMAT";
  std::string read_str = "READ";

  std::string car_str = "CAR";
  std::string cdr_str = "CDR";
  std::string stream_str = "STREAM";
  std::string format_string_str = "FORMAT-STRING";
  std::string amp_rest_str = "&REST";
  std::string rest_str = "REST";

  LispSymbol *cons_sym = intern_symbol(cons_str);
  LispSymbol *format_sym = intern_symbol(format_str);
  LispSymbol *read_sym = intern_symbol(read_str);

  LispSymbol *car = intern_symbol(car_str);
  LispSymbol *cdr = intern_symbol(cdr_str);
  LispSymbol *stream_ = intern_symbol(stream_str);
  LispSymbol *format_string_ = intern_symbol(format_string_str);
  LispSymbol *amp_rest = intern_symbol(amp_rest_str);
  LispSymbol *rest = intern_symbol(rest_str);

  args_decl = build_cons(car, cdr);
  define_primitive_function(cons_sym, args_decl, cons);

  args_decl = build_cons(stream_, format_string_, amp_rest, rest);
  define_primitive_function(format_sym, args_decl, format_);

  args_decl = new LispCons{{LispObjType::ObjCons}, true, {}, {}};
  define_primitive_function(read_sym, args_decl, read);

  // symbols
  std::string t_str = "T";
  std::string nil_str = "NIL";

  LispSymbol *t_sym = intern_symbol(t_str);
  LispSymbol *nil_sym = intern_symbol(nil_str);

  bind(t_sym, LispForm::t());
  bind(nil_sym, LispForm::nil());
}

auto LishpRuntime::bind(LispSymbol *sym, LispForm val) -> void {
  this->symbol_assocs_.insert({sym, val});
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
