#include <assert.h>
#include <stdlib.h>

#include "runtime/memory_manager.h"

#define MEGABYTES(n) ((n) << 20)
#define BLOCK_SIZE MEGABYTES(4)

typedef enum {
  kMarkWhite,
  kMarkGrey,
  kMarkBlack,
} Marking;

typedef struct marking_info {
  uint32_t size;
  Marking mark;
  int allocated;
  struct marking_info *next;
} MarkingInfo;

typedef struct {
  char *bytes;
  MarkingInfo *first_free;
  MarkingInfo *first_allocated;
} Block;

struct manager {
  Block block;
  uint32_t allocated_bytes;
  uint32_t freed_bytes;
  RuntimeMarker runtime_marker;
};

int initialize_manager(MemoryManager **pmanager, RuntimeMarker runtime_marker) {
  char *block_bytes = calloc(1, BLOCK_SIZE);
  if (block_bytes == NULL) {
    return -1;
  }

  MarkingInfo *first_free = (MarkingInfo *)block_bytes;
  first_free->allocated = 0;
  first_free->mark = kMarkWhite;
  first_free->next = NULL;
  first_free->size = BLOCK_SIZE;

  MemoryManager *manager = calloc(1, sizeof(MemoryManager));
  if (manager == NULL) {
    free(block_bytes);
    return -1;
  }

  manager->block.bytes = block_bytes;
  manager->block.first_free = first_free;
  manager->block.first_allocated = NULL;
  manager->allocated_bytes = 0;
  manager->freed_bytes = 0;
  manager->runtime_marker = runtime_marker;

  *pmanager = manager;

  return 0;
}

int cleanup_manager(MemoryManager **pmanager) {
  MemoryManager *manager = *pmanager;

  char *block_bytes = manager->block.bytes;

  free(manager);
  free(block_bytes);

  return 0;
}

void *allocate(MemoryManager *manager, uint32_t size) {
  MarkingInfo **ptr = &manager->block.first_free;

look_for_match:
  while ((*ptr != NULL) && ((*ptr)->size < sizeof(MarkingInfo) + size)) {
    ptr = &(*ptr)->next;
  }

  if (*ptr == NULL) {
    // Couldn't find anything of adequate size
    return NULL;
  }

  MarkingInfo *result = *ptr;

  if (result->size == sizeof(MarkingInfo) + size) {
    // a perfect match, just remove this from the free list, don't split
    *ptr = result->next;
  } else {
    // this block is larger than necessary
    if (result->size < 2 * sizeof(MarkingInfo) + size) {
      // we don't have enough extra space to allocate a new header, so keep
      // looking for a match
      goto look_for_match;
    }

    // we have enough extra space to split off a new entry in the free list

    MarkingInfo *right_half =
        (MarkingInfo *)((char *)*ptr + sizeof(MarkingInfo) + size);

    right_half->allocated = 0;
    right_half->mark = kMarkWhite;
    right_half->next = (*ptr)->next;
    right_half->size = (*ptr)->size - (sizeof(MarkingInfo) + size);

    *ptr = right_half;
  }

  result->allocated = 1;
  result->mark = kMarkWhite;
  result->next = manager->block.first_allocated;
  result->size = sizeof(MarkingInfo) + size;

  manager->block.first_allocated = result;
  manager->allocated_bytes += size;

  return (void *)result + sizeof(MarkingInfo);
}

void deallocate(MemoryManager *manager, void *ptr, uint32_t size) {
  MarkingInfo *freed = ptr - sizeof(MarkingInfo);

  assert(freed->allocated && "Double free!");
  assert(freed->size == size + sizeof(MarkingInfo) &&
         "Not freeing the same size as allocated!");

  MarkingInfo **allocated = &manager->block.first_allocated;
  while ((*allocated != NULL) && (*allocated != freed)) {
    allocated = &(*allocated)->next;
  }

  if (*allocated == NULL) {
    assert(0 && "Could not find allocation!");
  }

  // remove the allocation from the allocated linked list and add it to the
  // freed list

  *allocated = (*allocated)->next;

  freed->next = manager->block.first_free;
  freed->allocated = 0;
  freed->mark = kMarkWhite;

  manager->block.first_free = freed;
  manager->freed_bytes += size;
}

void mark_used(MemoryManager *manager, void *ptr) {
  // TODO: should I check that this pointer corresponds to this manager somehow?
  MarkingInfo *header = ptr - sizeof(MarkingInfo);
  header->mark = kMarkBlack;
}

uint32_t inspect_allocation(MemoryManager *manager) {
  (void)manager;
  return manager->allocated_bytes - manager->freed_bytes;
}
