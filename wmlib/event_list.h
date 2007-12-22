#ifndef EVENT_LIST_H
#define EVENT_LIST_H

typedef void* item_t;

typedef struct list {
  item_t *items;
  int nitems;
  int max_items;
} list_t;

list_t* list_new();
void list_free(list_t *list);
void list_append(list_t *list, item_t item);

#endif /* EVENT_LIST_H */
