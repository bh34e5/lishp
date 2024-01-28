#ifndef lishp_primitives_h
#define lishp_primitives_h

#include "../types/types.hpp"

PrimitiveFunction cons;
PrimitiveFunction format_;
PrimitiveFunction read;
PrimitiveFunction eval;

// FIXME: this is not actually primitive, it can be written with tagbody and
// goto
PrimitiveFunction loop;

#endif
