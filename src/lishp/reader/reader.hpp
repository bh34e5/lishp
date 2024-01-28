#ifndef lishp_reader_reader_h
#define lishp_reader_reader_h

#include <iostream>

#include "../types/types.hpp"
#include "readtable.hpp"

class Reader {
public:
  auto read_form(Readtable &table, std::istream &in_stream) -> LispForm;
};

#endif
