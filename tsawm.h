
#include <X11/extensions/shape.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h> 
#include <X11/Xutil.h>

#include "wmlib/wmlib.h"

typedef  struct container {
  Window frame;
  XContext context;
  wm_t *wm;
  client_t *clients;
  int num_clients;
} container_t;

Bool addwin(wm_t *wm, wm_event_t *event);
Window mkframe(wm_t *wm, Window parent, int x, int y, int width, int height);

container_t *container_new(wm_t *wm, Window parent, int x, int y, int width, int height);

Bool container_show(container_t *container);
Bool container_client_add(container_t *container, client_t *client);
Bool container_client_show(container_t *container, client_t *client);

