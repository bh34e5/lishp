#include <functional>
#include <iostream>

#include "../lishp.hpp"
#include "../runtime/exceptions.hpp"
#include "primitive_helpers.hpp"
#include "primitives.hpp"

auto cons(LishpRuntime *rt, LispCons args) -> LispForm {
  LispCons evaled_args = eval_args(rt, &args);
  LispForm first = evaled_args.car;
  LispForm second = evaled_args.cdr;

  LispCons *c = new LispCons{{LispObjType::ObjCons}, false, first, second};
  return {LispFormType::FormObj, {.obj = c}};
}

// FIXME: This should probably stop being a primitive at some point. Rather, the
// stream operations should be primitive, while the format function is built off
// those primitives. That is, something like looping over the format string,
// swapping args as necessary, and then writing out to the passed stream using
// primitives.
auto format_(LishpRuntime *rt, LispCons args) -> LispForm {
  LispCons evaled_args = eval_args(rt, &args);
  std::cout << evaled_args.to_string() << std::endl;

  // FIXME: for now, we are not dealing with format strings, and also, the only
  // stream that is valid, is `T`, so that it goes to standard out.

  LispForm stream_ = evaled_args.car;

  // TODO: add the T object and make the assert that the stream is equal to
  // that...

  LispCons *rest = evaled_args.rest();

  LispForm format_str = rest->car;

  if (format_str.type != LispFormType::FormObj) {
    // TODO: figure out how I want to do exceptions and what kind of information
    // I want to give
    throw RuntimeException("Expected argument of type String");
  }

  LispObj *obj = format_str.as.obj;

  if (obj->type != LispObjType::ObjString) {
    throw RuntimeException("Expected argument of type String");
  }

  LispString *str = (LispString *)obj;

  std::cout << str->str;

  return LispForm::nil();
}

auto read(LishpRuntime *rt, LispCons args) -> LispForm {
  LispCons evaled_args = eval_args(rt, &args);
  return rt->get_reader().read_form(rt->cur_readtable(), std::cin);
}

auto eval(LishpRuntime *rt, LispCons args) -> LispForm {
  // To match other primitives, the args are packaged into a single element list
  LispCons evaled_args = eval_args(rt, &args);
  LispForm car = evaled_args.car;
  return eval_form(rt, car);
};

auto loop(LishpRuntime *rt, LispCons args) -> LispForm {
  while (true) {
    // if they passed in nil, well, I guess we're looping...
    if (args.nil)
      continue;

    LispCons *cur_cons = &args;
    while (true) {
      LispForm car = cur_cons->car;

      if (car.type != LispFormType::FormObj ||
          car.as.obj->type != LispObjType::ObjCons) {
        throw RuntimeException("Expected cons to execute in fake loop");
      }

      eval_form(rt, car);

      if (cur_cons->cdr.is_nil()) {
        break;
      }

      cur_cons = cur_cons->rest();
    }
  }

  return LispForm::nil();
}

auto tagbody(LishpRuntime *rt, LispCons args) -> LispForm {
  // FIXME: these are actually lexically bound tags... because `go` needs to be
  // able to reference them when jumping...
  //
  // The body is its own lexical environment anyway, so I can just extend off
  // where I am (from runtime?) and bind them there. I'll have a map for
  // variable bindings, function bindings, and tag bodies?
  std::map<LispForm *, LispCons *> control_points;

  std::function<void(LispCons *)> add_syms = [&](LispCons *cons) {
    if (cons->nil) {
      // nothing to do
      return;
    }

    LispForm *car = &cons->car;
    if (car->type == LispFormType::FormObj &&
        car->as.obj->type == LispObjType::ObjCons) {
      // skip conses as they are not tags
      return;
    }

    // car is an atom (FIXME: make sure this is actually true.. turn that into
    // a function on the form class?)

    control_points.insert({car, cons});
  };

  args.for_each(add_syms);

  // TODO: execute the body one form at at time, not executing the tags, because
  // there's no need; I think

  return LispForm::nil();
}
