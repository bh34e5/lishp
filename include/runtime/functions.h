#ifndef runtime_functions_
#define runtime_functions_

#include "runtime/types.h"

INHERENT_FN(system_repl);
INHERENT_FN(system_read_open_paren);
INHERENT_FN(system_read_close_paren);
INHERENT_FN(system_read_double_quote);
INHERENT_FN(system_read_single_quote);
INHERENT_FN(common_lisp_read);
INHERENT_FN(common_lisp_format);

#endif
