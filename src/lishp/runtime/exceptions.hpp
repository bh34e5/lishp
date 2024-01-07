#ifndef lishp_runtime_exceptions_h
#define lishp_runtime_exceptions_h

#include <stdexcept>

class RuntimeException : public std::runtime_error {
public:
  // constructors defined in super class
  RuntimeException(const std::string &what_arg)
      : std::runtime_error(what_arg) {}
  RuntimeException(const char *what_arg) : std::runtime_error(what_arg) {}
  RuntimeException(const RuntimeException &other)
      : std::runtime_error(other.what()) {}
};

#endif
