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

  LispSymbol *format_sym = table_.intern_symbol("FORMAT");
  LispSymbol *t_sym = table_.intern_symbol("T");
  LispSymbol *loop_sym = table_.intern_symbol("LOOP");
  LispSymbol *eval_sym = table_.intern_symbol("EVAL");
  LispSymbol *read_sym = table_.intern_symbol("READ");

  std::string output = "Testing the output from running eval\n";
  LispString os{{LispObjType::ObjString}, std::move(output)};
  LispForm osf = {LispFormType::FormObj, {.obj = &os}};

  LispCons *read_cons = build_cons(read_sym);
  LispCons *first_run = build_cons(format_sym, t_sym, osf, read_cons);

  LispForm rc = {LispFormType::FormObj, {.obj = first_run}};

  LispString prompt{{LispObjType::ObjString}, "> "};
  LispString obj_print{{LispObjType::ObjString}, "~a~%"};

  LispForm prompt_form{LispFormType::FormObj, {.obj = &prompt}};
  LispForm obj_print_form{LispFormType::FormObj, {.obj = &obj_print}};

  LispCons *loop_cons =
      build_cons(loop_sym, build_cons(format_sym, t_sym, prompt_form),
                 build_cons(format_sym, t_sym, obj_print_form,
                            build_cons(eval_sym, build_cons(read_sym))));

  LispForm loop_form{LispFormType::FormObj, {.obj = loop_cons}};

  // FIXME: eventually this will turn into something like
  // (loop
  //   (format t "> ")
  //   (format t (eval (read))))
  run_program({rc, loop_form});
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

  LispSymbol *cons_sym = table_.intern_symbol("CONS");
  LispSymbol *format_sym = table_.intern_symbol("FORMAT");
  LispSymbol *read_sym = table_.intern_symbol("READ");
  LispSymbol *eval_sym = table_.intern_symbol("EVAL");
  LispSymbol *loop_sym = table_.intern_symbol("LOOP");

  LispSymbol *car = table_.intern_symbol("CAR");
  LispSymbol *cdr = table_.intern_symbol("CDR");
  LispSymbol *stream_ = table_.intern_symbol("STREAM");
  LispSymbol *format_string_ = table_.intern_symbol("FORMAT-STR");
  LispSymbol *amp_rest = table_.intern_symbol("&REST");
  LispSymbol *rest = table_.intern_symbol("REST");
  LispSymbol *form = table_.intern_symbol("FORM");

  args_decl = build_cons(car, cdr);
  define_primitive_function(cons_sym, args_decl, cons);

  args_decl = build_cons(stream_, format_string_, amp_rest, rest);
  define_primitive_function(format_sym, args_decl, format_);

  args_decl = new LispCons{{LispObjType::ObjCons}, true, {}, {}};
  define_primitive_function(read_sym, args_decl, read);

  args_decl = build_cons(form);
  define_primitive_function(eval_sym, args_decl, eval);

  args_decl = build_cons(amp_rest, rest);
  define_primitive_function(loop_sym, args_decl, loop);

  // symbols
  LispSymbol *t_sym = table_.intern_symbol("T");
  LispSymbol *nil_sym = table_.intern_symbol("NIL");

  bind(t_sym, LispForm::t());
  bind(nil_sym, LispForm::nil());
}

auto LishpRuntime::bind(LispSymbol *sym, LispForm val) -> void {
  this->symbol_assocs_.insert({sym, val});
}
