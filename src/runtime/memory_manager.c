#include <assert.h>
#include <stdlib.h>

#include "runtime/memory_manager.h"

#define MEGABYTES(n) ((n) << 20)
#define BLOCK_SIZE MEGABYTES(4)

#define INITIAL_GC_CHECK 1024
#define NEXT_GC_CHECK(cur) (2 * (cur))

#define CHECK_FOR_GARBAGE

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
  uint32_t next_gc_size;
  uint32_t allocated_bytes;
  uint32_t freed_bytes;
  RuntimeMarker runtime_marker;
  struct runtime *rt;
};

int initialize_manager(MemoryManager **pmanager, RuntimeMarker runtime_marker,
                       struct runtime *rt) {

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
  manager->next_gc_size = INITIAL_GC_CHECK;
  manager->allocated_bytes = 0;
  manager->freed_bytes = 0;
  manager->runtime_marker = runtime_marker;
  manager->rt = rt;

  *pmanager = manager;

  return 0;
}

static void run_gc(MemoryManager *manager, MarkingInfo *trigger) {
#ifdef CHECK_FOR_GARBAGE
  MarkingInfo *cur;

  // mark all allocated as gray
  cur = manager->block.first_allocated;
  while (cur != NULL) {
    cur->mark = kMarkGrey;
    cur = cur->next;
  }

  // mark the trigger as used, and then mark everything reachable from the
  // runtime as used
  if (trigger != NULL) {
    trigger->mark = kMarkBlack;
  }
  manager->runtime_marker(manager->rt);

  // remove all the allocations that are still marked grey (not used)
  cur = manager->block.first_allocated;
  while (cur != NULL) {
    if (cur->mark == kMarkGrey) {
      // TODO: maybe add a `deallocate_internal` and make deallocate a wrapper
      // for that call
      deallocate(manager, (void *)cur + sizeof(MarkingInfo),
                 cur->size - sizeof(MarkingInfo));
    }
    cur = cur->next;
  }

  // recalculate the time to do a garbage collection
  manager->next_gc_size = NEXT_GC_CHECK(manager->next_gc_size);
#endif
}

int cleanup_manager(MemoryManager **pmanager, uint32_t *final_allocated) {
  MemoryManager *manager = *pmanager;

  run_gc(manager, NULL);
  if (final_allocated != NULL) {
    *final_allocated = inspect_allocation(manager);
  }

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
      ptr = &(*ptr)->next;
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

  if (manager->allocated_bytes >= manager->next_gc_size) {
    run_gc(manager, result);
  }

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
