#include <memory.h>
#include <stdlib.h>

#include "util.h"

int list_init(List *l) {
  // zero initialize
  *l = (List){.size = 0, .cap = 0, .items = NULL};
  return 0;
}

int list_clear(List *l) {
  // zero initialize
  if (l->items != NULL) {
    free(l->items);
  }
  *l = (List){.size = 0, .cap = 0, .items = NULL};
  return 0;
}

int list_push(List *l, uint32_t size, void *item) {
  if (l->size + 1 > l->cap) {
    uint32_t next_cap = NEXT_CAPACITY(l->cap);
    void *res = realloc(l->items, (size * next_cap));
    if (res == NULL) {
      // realloc failed
      return -1;
    }

    l->items = res;
    l->cap = next_cap;
  }

  void *target = l->items + (l->size * size);
  memmove(target, item, size);

  ++l->size;

  return 0;
}

int list_append(List *l, uint32_t size, List *other) {
  if (other->size == 0) {
    return 0;
  }

  if (l->size + other->size > l->cap) {
    uint32_t next_cap = l->cap;
    do {
      next_cap = NEXT_CAPACITY(next_cap);
    } while (l->size + other->size > next_cap);

    void *res = realloc(l->items, (size * next_cap));
    if (res == NULL) {
      return -1;
    }

    l->items = res;
    l->cap = next_cap;
  }

  void *target = l->items + (l->size * size);
  memmove(target, other->items, (other->size * size));

  l->size += other->size;
  other->size = 0;

  return 0;
}

int list_pop(List *l, uint32_t size, void *item) {
  if (l->size == 0) {
    return -1;
  }

  --l->size;

  void *target = l->items + (l->size * size);
  if (item != NULL) {
    memmove(item, target, size);
  }

  return 0;
}

int list_popn(List *l, uint32_t size, uint32_t n) {
  (void)size; // keeping the arg for symmetry

  if (l->size < n) {
    return -1;
  }

  l->size -= n;

  return 0;
}

int list_remove(List *l, uint32_t size, uint32_t index, void *item) {
  if (index >= l->size) {
    return -1;
  }

  void *target = l->items + (index * size);
  if (item != NULL) {
    memmove(item, target, size);
  }

  --l->size;

  void *end = l->items + (l->size * size);
  shift_items(end, size, l->size - index, -1);

  return 0;
}

int list_get(List *l, uint32_t size, uint32_t index, void *item) {
  if (index >= l->size) {
    return -1;
  }

  void *target = l->items + (index * size);
  memmove(item, target, size);

  return 0;
}

int list_get_last(List *l, uint32_t size, void *item) {
  void *target = l->items + ((l->size - 1) * size);
  memmove(item, target, size);

  return 0;
}

int list_ref(List *l, uint32_t size, uint32_t index, void **pitem) {
  if (index >= l->size) {
    return -1;
  }

  void *target = l->items + (index * size);
  *pitem = target;

  return 0;
}

int list_ref_last(List *l, uint32_t size, void **pitem) {
  void *target = l->items + ((l->size - 1) * size);
  *pitem = target;

  return 0;
}

int list_foreach(List *l, uint32_t size, Iterator it_fn, void *arg) {
  uint32_t ind = 0;
  while (ind < l->size) {
    void *item = l->items + (ind * size);

    int res = it_fn(arg, item);
    if (res != 0) {
      return res;
    }

    ++ind;
  }
  return 0;
}

int list_find(List *l, uint32_t size, Iterator it_fn, void *arg, void **pitem) {
  uint32_t ind = 0;
  while (ind < l->size) {
    void *item = l->items + (ind * size);

    int found = it_fn(arg, item);
    if (found) {
      if (pitem != NULL) {
        *pitem = item;
      }
      return 0;
    }

    ++ind;
  }
  return -1;
}
