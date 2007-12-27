
#include <X11/extensions/shape.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h> 
#include <X11/Xutil.h>

#include "wmlib/wmlib.h"

#define SPLIT_VERTICAL 0U
#define SPLIT_HORIZONTAL 1U

#define FRAME_EVENT_MASK (\
  ExposureMask | EnterWindowMask | LeaveWindowMask \
  | MapNotify \
  | ButtonPressMask | ButtonReleaseMask \
  )
  //| StructureNotifyMask | SubstructureRedirectMask 

#define CLIENT_EVENT_MASK (\
  EnterWindowMask | LeaveWindowMask | PropertyChangeMask \
  | ColormapChangeMask | FocusChangeMask | StructureNotifyMask \
  )

typedef  struct container {
  XContext context;
  Screen *screen;
  GC gc;
  Window frame;
  wm_t *wm;
  client_t *clients;
  int num_clients;
  int focused;
} container_t;


/* wmlib event handlers */
Bool maprequest(wm_t *wm, wm_event_t *event);
Bool addwin(wm_t *wm, wm_event_t *event);
Bool focus_container(wm_t *wm, wm_event_t *event);
Bool expose_container(wm_t *wm, wm_event_t *event);
Bool keydown(wm_t *wm, wm_event_t *event);
Bool keyup(wm_t *wm, wm_event_t *event);
Bool unmap(wm_t *wm, wm_event_t *event);
Bool run(const char *cmd);

Window mkframe(wm_t *wm, Window parent, int x, int y, int width, int height);

container_t *container_new(wm_t *wm, Window parent, int x, int y, int width, int height);

Bool container_show(container_t *container);
Bool container_client_add(container_t *container, client_t *client);
Bool container_client_show(container_t *container, client_t *client);
Bool container_blur(container_t *container);
Bool container_focus(container_t *container);
Bool container_create_gc(container_t *container);
Bool container_paint(container_t *container);
Bool container_relocate_top_client(container_t *from, container_t *to);

Bool container_split(container_t *container, unsigned int split_type);
Bool container_moveresize(container_t *container, int x, int y, unsigned int width, unsigned int height);

