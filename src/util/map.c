#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

uint32_t n;

static int find_item(OrderedMap *m, uint32_t item_size, void *key,
                     uint32_t *ind);

int map_init(OrderedMap *m, Comparator cmp_fn) {
  *m = (OrderedMap){.cmp_fn = cmp_fn, .size = 0, .cap = 0, .items = NULL};
  return 0;
}

int map_clear(OrderedMap *m) {
  if (m->items != NULL) {
    free(m->items);
  }
  *m = (OrderedMap){.cmp_fn = m->cmp_fn, .size = 0, .cap = 0, .items = NULL};
  return 0;
}

int map_insert(OrderedMap *m, uint32_t key_size, uint32_t val_size, void *key,
               void *value) {

  uint32_t cur_ind;
  int found_item = find_item(m, key_size + val_size, key, &cur_ind);

  if (!found_item) {
    // inserting new pair, check the size to see if we need to raise it
    if (m->size + 1 > m->cap) {
      uint32_t next_cap = NEXT_CAPACITY(m->cap);
      void *res = realloc(m->items, ((val_size + key_size) * next_cap));
      if (res == NULL) {
        return -1;
      }

      m->items = res;
      m->cap = next_cap;
    }

    void *key_target = m->items + (cur_ind * (key_size + val_size));
    void *val_target = key_target + key_size;

    // shift the items up (if ind == size, then it doesn't do anything)
    shift_items(key_target, key_size + val_size, m->size - cur_ind, 1);

    // then add the key as well as incrementing size
    memmove(key_target, key, key_size);
    memmove(val_target, value, val_size);

    ++m->size;
  } else {
    // replacing a value, don't rewrite the key
    void *key_target = m->items + (cur_ind * (key_size + val_size));
    void *val_target = key_target + key_size;

    memmove(val_target, value, val_size);
  }

  return 0;
}

int map_remove(OrderedMap *m, uint32_t key_size, uint32_t val_size, void *key,
               void *value) {

  uint32_t cur_ind;
  int found_item = find_item(m, key_size + val_size, key, &cur_ind);

  if (!found_item) {
    return -1;
  }

  void *key_target = m->items + (cur_ind * (key_size + val_size));
  void *val_target = key_target + key_size;

  if (value != NULL) {
    memmove(value, val_target, val_size);
  }

  --m->size;

  void *end = m->items + (m->size * (key_size + val_size));
  shift_items(end, key_size + val_size, m->size - cur_ind, -1);

  return 0;
}

int map_get(OrderedMap *m, uint32_t key_size, uint32_t val_size, void *key,
            void *value) {

  uint32_t cur_ind;
  int found_item = find_item(m, key_size + val_size, key, &cur_ind);

  if (!found_item) {
    return -1;
  }

  void *key_target = m->items + (cur_ind * (key_size + val_size));
  void *val_target = key_target + key_size;

  memmove(value, val_target, val_size);

  return 0;
}

int map_ref(OrderedMap *m, uint32_t key_size, uint32_t val_size, void *key,
            void **pvalue) {

  uint32_t cur_ind;
  int found_item = find_item(m, key_size + val_size, key, &cur_ind);

  if (!found_item) {
    return -1;
  }

  void *key_target = m->items + (cur_ind * (key_size + val_size));
  void *val_target = key_target + key_size;

  if (pvalue != NULL) {
    *pvalue = val_target;
  }

  return 0;
}

static int find_item(OrderedMap *m, uint32_t item_size, void *key,
                     uint32_t *ind) {

  int found_item = 0;
  uint32_t cur_ind = 0;
  while (cur_ind < m->size) {
    void *item = m->items + (cur_ind * item_size);

    int cmp = m->cmp_fn(key, item);
    if (cmp == 0) {
      found_item = 1;
      break;
    } else if (cmp > 0) {
      break;
    }

    ++cur_ind;
  }

  *ind = cur_ind;
  return found_item;
}
