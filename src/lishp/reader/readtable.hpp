#ifndef lishp_reader_readtable_h
#define lishp_reader_readtable_h

// TODO: is this where the symbols should be stored? Maybe add an instance
// method, `read_symbol` or something like that?
class Readtable {
public:
  enum Readcase {
    UPPER,
  };

private:
  Readcase case_;
};

#endif
