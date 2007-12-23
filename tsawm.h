
#include <X11/X.h>
#include "wmlib/wmlib.h"

typedef  struct container {
  Window frame;
  client_t **clients;
} container_t;

Bool addwin(wm_t *wm, wm_event_t *event);
Window mkframe(wm_t *wm, Window child);
