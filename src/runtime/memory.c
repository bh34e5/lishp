#include <stdlib.h>

#include "runtime/memory_manager.h"

#define MEGABYTES(n) ((n) << 20)
#define BLOCK_SIZE MEGABYTES(4)

static char BLOCK[BLOCK_SIZE];
static uint32_t index = 0;
static uint32_t freed_bytes = 0;

typedef enum {
  kMarkWhite,
  kMarkGrey,
  kMarkBlack,
} Marking;

typedef struct {
  uint32_t size;
  Marking mark;
} MarkingInfo;

void *allocate(uint32_t size) {
  if (index + size + sizeof(MarkingInfo) >= BLOCK_SIZE) {
    return NULL;
  }

  void *ptr = BLOCK + index + sizeof(MarkingInfo);
  index += size;

  return ptr;
}

void deallocate(void *ptr, uint32_t size) {
  (void)ptr;  // FIXME: no way to deallocate at the moment, so keep size the
  (void)size; // allocated amount the same
  freed_bytes += size;
}

uint32_t inspect_allocation() { return index - freed_bytes; }
