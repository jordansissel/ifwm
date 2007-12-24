#include <stdio.h>

#include "tsawm.h"
#include "wmlib/wmlib.h"

#define BORDER 1
#define TITLE_HEIGHT 10

Bool addwin(wm_t *wm, wm_event_t *event);

int main(int argc, char **argv) {
  wm_t *wm = NULL;
  wm = wm_init(NULL);
  wm_set_log_level(wm, LOG_INFO);

  wm_listener_add(wm, WM_EVENT_MAPREQUEST, addwin);

  /* Create workspace */
  wm_main(wm);

  return 0;
}

Bool addwin(wm_t *wm, wm_event_t *event) {
  Window new_window;
  Window frame;
  wm_log(wm, LOG_INFO, "addwin!");

  new_window = event->window;

  XGrabServer(wm->dpy);
  frame = mkframe(wm, new_window);
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

Window mkframe(wm_t *wm, Window child) {
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

