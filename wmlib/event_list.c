#include "event_list.h"

/* Note: item_t is the wm_event_handler */

list_t *list_new() {
  list_t *list;
  list = malloc(sizeof(list_t));

  list->max_items = 10;
  list->nitems = 0;
  list->items = malloc(list->max_items * sizeof(item_t));
}

void list_free(list_t *list) {
  free(list->items);
  free(list);
}

void list_append(list_t *list, item_t item) {
  list->items[list->nitems] = item;

  list->nitems++;
  if (list->nitems == list->max_items) {
    list->max_items *= 2;
    list->items = (item_t *)realloc(list->items, list->max_items * sizeof(item_t));
  }
}
