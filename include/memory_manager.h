#ifndef memory_
#define memory_

#include <stdint.h>

void *allocate(uint32_t size);
void deallocate(void *ptr, uint32_t size);

uint32_t inspect_allocation();

#endif
