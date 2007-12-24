#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tsawm.h"

#define BORDER 1
#define TITLE_HEIGHT 15

static void *xmalloc(size_t size) {
  void *ptr;
  ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, "malloc(%td) failed\n", size);
    exit(1);
  }
  memset(ptr, 0, size);
  return ptr;
}

container_t *current_container;

int main(int argc, char **argv) {
  wm_t *wm = NULL;
  int i;
  wm = wm_create(NULL);
  wm_set_log_level(wm, LOG_INFO);

  wm_log(wm, LOG_INFO, "== num screens: %d", wm->num_screens);
  for (i = 0; i < wm->num_screens; i++) {
    XWindowAttributes attr;
    Window root = wm->screens[i]->root;;
    container_t *root_container;
    XGetWindowAttributes(wm->dpy, root, &attr);
    root_container = container_new(wm, root, attr.x, attr.y, 
                                   attr.width, attr.height);
    container_show(root_container);
    wm_log(wm, LOG_INFO, "Setting current container to %tx", root_container);
    current_container = root_container;
  }

  wm_listener_add(wm, WM_EVENT_MAPREQUEST, addwin);
  wm_init(wm);
  wm_main(wm);

  return 0;
}

container_t *container_new(wm_t *wm, Window parent, int x, int y,
                           int width, int height) {
  container_t *container;
  container = xmalloc(sizeof(container_t));
  container->context = XUniqueContext();
  container->frame = mkframe(wm, parent, x, y, width, height);
  container->wm = wm;
  return container;
}

Bool container_show(container_t *container) {
  XMapRaised(container->wm->dpy, container->frame);
  XFlush(container->wm->dpy);
  return True;
}

Bool container_client_add(container_t *container, client_t *client) {
  XWindowAttributes attr;
  wm_log(container->wm, LOG_INFO, "%s: client add window %d", __func__, client);
  XGetWindowAttributes(container->wm->dpy, client->window, &attr);
  XReparentWindow(container->wm->dpy, client->window, container->frame, 0, TITLE_HEIGHT);
  XSetWindowBorderWidth(container->wm->dpy, client->window, 0);
  XResizeWindow(container->wm->dpy, client->window, attr.width, attr.height);
  XMapRaised(container->wm->dpy, client->window);
  return True;
}

Bool addwin(wm_t *wm, wm_event_t *event) {
  wm_log(wm, LOG_INFO, "%s: %d / %d", __func__, event->event_id, event->client->window);
  container_client_add(current_container, event->client);
  return True;
}

Bool _addwin(wm_t *wm, wm_event_t *event) {
  Window new_window;
  Window frame;
  wm_log(wm, LOG_INFO, "addwin!");

  new_window = event->client->window;

  XGrabServer(wm->dpy);
  //frame = mkframe(wm, new_window);
  if (frame == 0)
    return False;

  XAddToSaveSet(wm->dpy, new_window);
  XSetWindowBorderWidth(wm->dpy, new_window, 0);
  XReparentWindow(wm->dpy, new_window, frame, 0, TITLE_HEIGHT);
  XMapWindow(wm->dpy, frame);
  XMapWindow(wm->dpy, new_window);
  XRaiseWindow(wm->dpy, frame);

  /* XXX: this should be configurable */
  wm_grab_button(wm, frame, Mod1Mask, AnyButton);
  XUngrabServer(wm->dpy);
  XFlush(wm->dpy);

  return True;
}

Window mkframe(wm_t *wm, Window parent, int x, int y, int width, int height) {
  Window frame;
  XSetWindowAttributes frame_attr;
  XWindowAttributes parent_attr;
  unsigned long valuemask;
  XColor border_color;
  Visual *visual;

  frame_attr.event_mask = (SubstructureNotifyMask | SubstructureRedirectMask \
                           | ButtonPressMask | ButtonReleaseMask \
                           | EnterWindowMask | LeaveWindowMask);
  XGetWindowAttributes(wm->dpy, parent, &parent_attr);
  visual = parent_attr.screen->root_visual;

  XParseColor(wm->dpy, parent_attr.screen->cmap, "#999933", &border_color);
  XAllocColor(wm->dpy, parent_attr.screen->cmap, &border_color);
  frame_attr.border_pixel = border_color.pixel;

  valuemask = CWEventMask | CWBorderPixel;

  frame = XCreateWindow(wm->dpy, parent,
                        x, y, width, height,
                        BORDER,
                        CopyFromParent,
                        CopyFromParent,
                        visual,
                        valuemask,
                        &frame_attr);
  return frame;
}

Window _mkframe(wm_t *wm, Window child) {
  Window frame;
  int x, y, width, height;
  XWindowAttributes child_attr;
  XSetWindowAttributes frame_attr;
  unsigned long valuemask;
  XColor border_color;

  if (!XGetWindowAttributes(wm->dpy, child, &child_attr)) {
    wm_log(wm, LOG_ERROR, "%s: XGetWindowAttributes failed", __func__);
    return 0;
  }

  x = child_attr.x;
  y = child_attr.y;
  width = child_attr.width;
  height = child_attr.height + TITLE_HEIGHT;

  frame_attr.event_mask = (SubstructureNotifyMask | SubstructureRedirectMask \
                           | ButtonPressMask | ButtonReleaseMask \
                           | EnterWindowMask | LeaveWindowMask);

  XParseColor(wm->dpy, child_attr.screen->cmap, "#999933", &border_color);
  XAllocColor(wm->dpy, child_attr.screen->cmap, &border_color);

  frame_attr.border_pixel = border_color.pixel;

  valuemask = CWEventMask | CWBorderPixel;

  frame = XCreateWindow(wm->dpy, child_attr.root,
                        x, y, width, height,
                        BORDER,
                        CopyFromParent,
                        CopyFromParent,
                        child_attr.visual,
                        valuemask,
                        &frame_attr);
  return frame;
}

