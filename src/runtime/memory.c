#include <stdlib.h>

#include "runtime/memory_manager.h"

#define MEGABYTES(n) ((n) << 20)
#define BLOCK_SIZE MEGABYTES(4)

typedef enum {
  kMarkWhite,
  kMarkGrey,
  kMarkBlack,
} Marking;

typedef struct {
  uint32_t size;
  Marking mark;
} MarkingInfo;

struct manager {
  char *block;
  uint32_t index;
  uint32_t freed_bytes;
  RuntimeMarker runtime_marker;
};

int initialize_manager(MemoryManager **pmanager, RuntimeMarker runtime_marker) {
  char *block = calloc(1, BLOCK_SIZE);
  if (block == NULL) {
    return -1;
  }

  MemoryManager *manager = calloc(1, sizeof(MemoryManager));
  if (manager == NULL) {
    free(block);
    return -1;
  }

  manager->runtime_marker = runtime_marker;
  manager->block = block;
  manager->index = 0;
  manager->freed_bytes = 0;

  *pmanager = manager;

  return 0;
}
int cleanup_manager(MemoryManager **pmanager) {
  MemoryManager *manager = *pmanager;

  char *block = manager->block;

  free(manager);
  free(block);

  return 0;
}

void *allocate(MemoryManager *manager, uint32_t size) {
  if (manager->index + size + sizeof(MarkingInfo) >= BLOCK_SIZE) {
    return NULL;
  }

  void *ptr = manager->block + manager->index + sizeof(MarkingInfo);
  manager->index += size;

  return ptr;
}

void deallocate(MemoryManager *manager, void *ptr, uint32_t size) {
  (void)ptr;  // FIXME: no way to deallocate at the moment, so keep size the
  (void)size; // allocated amount the same
  manager->freed_bytes += size;
}

void mark_used(MemoryManager *manager, void *ptr) {
  // TODO: should I check that this pointer corresponds to this manager somehow?
  MarkingInfo *header = ptr - sizeof(MarkingInfo);
  header->mark = kMarkBlack;
}

uint32_t inspect_allocation(MemoryManager *manager) {
  (void)manager;
  return manager->index - manager->freed_bytes;
}
