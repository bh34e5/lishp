#include <string.h>

#include "util.h"

void shift_items(void *items, uint32_t size, uint32_t count, int direction) {
  while (count > 0) {
    void *target = ((char *)items) + (direction * ((int)((count - 1) * size)));
    void *dest = ((char *)target) + (direction * ((int)size));

    memmove(dest, target, size);

    --count;
  }
}
