#ifndef common_
#define common_

#define TEST_CALL_LABEL(label, call)                                           \
  do {                                                                         \
    if ((call) < 0) {                                                          \
      goto label;                                                              \
    }                                                                          \
  } while (0)

#define TEST_CALL(call)                                                        \
  do {                                                                         \
    if ((call) < 0) {                                                          \
      return -1;                                                               \
    }                                                                          \
  } while (0)

#endif
