#ifndef util_
#define util_

#include <stdint.h>

#define INITIAL_CAPACITY 8
#define NEXT_CAPACITY(cap) ((cap) == 0 ? INITIAL_CAPACITY : (2 * (cap)))

void shift_items(void *items, uint32_t size, uint32_t count, int direction);

typedef struct {
  uint32_t size;
  uint32_t cap;
  void *items;
} List;

typedef int (*Iterator)(void *arg, void *obj);

int list_init(List *l);
int list_clear(List *l);
int list_push(List *l, uint32_t size, void *item);
int list_append(List *l, uint32_t size, List *other);
int list_pop(List *l, uint32_t size, void *item);
int list_remove(List *l, uint32_t size, uint32_t index, void *item);
int list_get(List *l, uint32_t size, uint32_t index, void *item);
int list_get_last(List *, uint32_t size, void *item);
int list_ref(List *l, uint32_t size, uint32_t index, void **pitem);
int list_ref_last(List *l, uint32_t size, void **pitem);
int list_foreach(List *l, uint32_t size, Iterator it_fn, void *arg);
int list_find(List *l, uint32_t size, Iterator it_fn, void *arg, void **pitem);

typedef int (*Comparator)(void *l, void *r);

typedef struct {
  Comparator cmp_fn;
  uint32_t size;
  uint32_t cap;
  void *items;
} OrderedMap;

int map_init(OrderedMap *m, Comparator cmp_fn);
int map_clear(OrderedMap *m);
int map_insert(OrderedMap *m, uint32_t key_size, uint32_t val_size, void *key,
               void *value);
int map_remove(OrderedMap *m, uint32_t key_size, uint32_t val_size, void *key,
               void *value);
int map_get(OrderedMap *m, uint32_t key_size, uint32_t val_size, void *key,
            void *value);
int map_ref(OrderedMap *m, uint32_t key_size, uint32_t val_size, void *key,
            void **pvalue);

#endif
