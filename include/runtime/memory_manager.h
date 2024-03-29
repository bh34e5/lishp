#ifndef runtime_memory_
#define runtime_memory_

#include <stdint.h>

struct runtime;
typedef void (*RuntimeMarker)(struct runtime *);

typedef struct manager MemoryManager;

int initialize_manager(MemoryManager **pmanager, RuntimeMarker runtime_marker,
                       struct runtime *rt);
int cleanup_manager(MemoryManager **pmanager, uint32_t *final_allocated);

void *allocate(MemoryManager *manager, uint32_t size);
void deallocate(MemoryManager *manager, void *ptr, uint32_t size);

void mark_used(MemoryManager *manager, void *ptr);

uint32_t inspect_allocation(MemoryManager *manager);

#endif
