// use bdb
// store things in an in-memory bdb?

/*
 * provide a "window manager" library.
 * - accept all necessary events
 * + Allow registry of events
 *  | Register events.
 *  | Store list of windows, state, etc
 * + Provide querying of window list without hitting the X server?
 * + Store additional properties for all windows via optional struct.
 */

/**
 * XXX: wm_add_window should really be a callback registered by the consumer of
 * this library.
 *
 * MapRequest handler will call listeners.
 * One listener can provide a Window which will be used as the parent?
 * Save a map of window -> client_info? (everyone else does)
 *
 * Library consumer should provide:
 *  Listener for window additions
 *  Listener for window destructions
 *  Listener for window state changes (unmap/map notify)
 *  Listener for metadata updates (resizes, name changes, etc)
 *  others?
 *
 */
/* for vasprintf in linux */
#define _GNU_SOURCE 

#include "wmlib.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void *xmalloc(size_t size) {
  void *ptr;
  ptr = malloc(size);
  if (ptr == NULL) {
    /* XXX: size == size_t, not int... */
    fprintf(stderr, "malloc(%d) failed\n", size);
    exit(1);
  }
  memset(ptr, 0, size);
  return ptr;
}

wm_t *wm_init(char *display_name) {
  wm_t *wm = NULL;
  wm = xmalloc(sizeof(wm_t));

  wm_x_open(wm, display_name);
  wm_x_init_handlers(wm);
  wm_x_init_screens(wm);
  wm_x_init_windows(wm);
  return wm;
}

void wm_x_init_screens(wm_t *wm) {
  int num_screens;
  int i;
  XSetWindowAttributes attr;

  attr.event_mask = SubstructureNotifyMask | SubstructureRedirectMask \
                    | EnterWindowMask | LeaveWindowMask;

  num_screens = ScreenCount(wm->dpy);
  wm->screens = xmalloc(num_screens * sizeof(Screen*));
  for (i = 0; i < num_screens; i++) {
    wm->screens[i] = ScreenOfDisplay(wm->dpy, i);
    XChangeWindowAttributes(wm->dpy, wm->screens[i]->root, CWEventMask, &attr);
    XSelectInput(wm->dpy, wm->screens[i]->root, attr.event_mask);
  }
}

void wm_x_init_handlers(wm_t *wm) {
  /* LASTEvent from X11/X.h is the max event value */
  int i;
  wm->event_handlers = xmalloc(LASTEvent * sizeof(event_handler));

  for (i = 0; i < LASTEvent; i++)
    wm->event_handlers[i] = wm_event_unknown;

  wm->event_handlers[KeyPress] = wm_event_keypress;
  wm->event_handlers[ButtonPress] = wm_event_buttonpress;
  wm->event_handlers[ButtonRelease] = wm_event_buttonrelease;
  wm->event_handlers[ConfigureRequest] = wm_event_configurerequest;
  wm->event_handlers[MapRequest] = wm_event_maprequest;
  wm->event_handlers[ClientMessage] = wm_event_clientmessage;
  wm->event_handlers[EnterNotify] = wm_event_enternotify;
  wm->event_handlers[LeaveNotify] = wm_event_leavenotify;
  wm->event_handlers[PropertyNotify] = wm_event_propertynotify;
  wm->event_handlers[UnmapNotify] = wm_event_unmapnotify;
  wm->event_handlers[DestroyNotify] = wm_event_destroynotify;
}

void wm_x_init_windows(wm_t *wm) {
  Window root, parent, *wins;
  unsigned int nwins;
  int screen;
  int nscreens;
  int i;

  nscreens = ScreenCount(wm->dpy);
  for (screen = 0; screen < nscreens; screen++) {
    wm_log(wm, LOG_INFO, "Querying window tree for screen %d", screen);
    XQueryTree(wm->dpy, wm->screens[screen]->root, &root, &parent, &wins, &nwins);
    for (i = 0; i < nwins; i++)
      wm_map_window(wm, wins[i]);
  }
}

void wm_set_log_level(wm_t *wm, int log_level) {
  if (log_level < 0)
    log_level = 0;
  wm->log_level = log_level;
}

int wm_get_log_level(wm_t *wm, int log_level) {
  return wm->log_level;
}

void wm_log(wm_t *wm, int log_level, char *format, ...) {
  va_list args;
  char *msg;
  //static const char *logprefix = "FEWI";
  //if (log_level > wm->log_level)
    //return;

  /** XXX: add datestamping */
  va_start(args, format);
  vasprintf(&msg, format, args);
  va_end(args);

  fprintf(stderr, "%s\n", msg);
  free(msg);

  if (log_level == LOG_FATAL)
    exit(1);
}

void wm_x_open(wm_t *wm, char *display_name) {
  wm_log(wm, LOG_INFO, "Opening display '%s'", display_name);
  wm->dpy = XOpenDisplay(display_name);
  if (wm->dpy == NULL)
    wm_log(wm, LOG_FATAL, "Failed opening display: '%s'", display_name);
}

void wm_main(wm_t *wm) {
  XEvent ev;

  for (;;) {
    XNextEvent(wm->dpy, &ev);
    wm->event_handlers[ev.type](wm, &ev);
  }
}

void wm_event_keypress(wm_t *wm, XEvent *ev) {
  XKeyEvent kev = ev->xkey;
  wm_log(wm, LOG_INFO, "%s", __func__);
}

void wm_event_buttonpress(wm_t *wm, XEvent *ev) {
  XButtonEvent bev = ev->xbutton;
  wm_log(wm, LOG_INFO, "%s", __func__);
}

void wm_event_buttonrelease(wm_t *wm, XEvent *ev) {
  XButtonEvent bev = ev->xbutton;
  wm_log(wm, LOG_INFO, "%s", __func__);
}

void wm_event_configurerequest(wm_t *wm, XEvent *ev) {
  XConfigureRequestEvent crev = ev->xconfigurerequest;
  XWindowChanges wc;
  wm_log(wm, LOG_INFO, "%s: %d wants to be %dx%d@%d,%d", __func__,
         crev.window, crev.width, crev.height, crev.x, crev.y);

  wc.sibling = crev.above;
  wc.stack_mode = crev.detail;
  wc.x = crev.x;
  wc.y = crev.y;
  wc.width = crev.width;
  wc.height = crev.height;
  XConfigureWindow(wm->dpy, crev.window, crev.value_mask, &wc);
}

void wm_event_maprequest(wm_t *wm, XEvent *ev) {
  XMapRequestEvent mrev = ev->xmaprequest;
  XWindowAttributes attr;
  wm_log(wm, LOG_INFO, "%s", __func__);

  XGrabServer(wm->dpy);
  if (!XGetWindowAttributes(wm->dpy, mrev.window, &attr)) {
    wm_log(wm, LOG_ERROR, "%s: XGetWindowAttributes failed", __func__);
    XUngrabServer(wm->dpy);
    return;
  }

  if (attr.override_redirect) {
    wm_log(wm, LOG_INFO, "%s: skipping window %d, override_redirect is set",
           __func__, mrev.window);
    XUngrabServer(wm->dpy);
    return;
  }

  // Call handlers?
  wm_map_window(wm, mrev.window);
  XUngrabServer(wm->dpy);

}

void wm_event_clientmessage(wm_t *wm, XEvent *ev) {
  XClientMessageEvent cmev = ev->xclient;
  wm_log(wm, LOG_INFO, "%s", __func__);
}

void wm_event_enternotify(wm_t *wm, XEvent *ev) {
  XEnterWindowEvent ewev = ev->xcrossing;
  wm_log(wm, LOG_INFO, "%s", __func__);
}

void wm_event_leavenotify(wm_t *wm, XEvent *ev) {
  XEnterWindowEvent ewev = ev->xcrossing;
  wm_log(wm, LOG_INFO, "%s", __func__);
}

void wm_event_propertynotify(wm_t *wm, XEvent *ev) {
  XPropertyEvent pev = ev->xproperty;
  wm_log(wm, LOG_INFO, "%s", __func__);

}

void wm_event_unmapnotify(wm_t *wm, XEvent *ev) {
  XUnmapEvent uev = ev->xunmap;
  wm_log(wm, LOG_INFO, "%s: Unmap %d", __func__, uev.window);
}

void wm_event_destroynotify(wm_t *wm, XEvent *ev) {
  XDestroyWindowEvent dev = ev->xdestroywindow;
  Window parent = dev.event;
  wm_log(wm, LOG_INFO, "%s: Window %d/%d was destroyed.", __func__, dev.event, dev.window);

  XDestroyWindow(wm->dpy, parent);
}

void wm_event_unknown(wm_t *wm, XEvent *ev) {
  wm_log(wm, LOG_INFO, "%s: Unknown event type '%d'", __func__, ev->type);
}

void wm_map_window(wm_t *wm, Window win) {
  Window frame;
  //Window root;
  XWindowAttributes new_win_attr;
  XSetWindowAttributes frame_attr;

  if (!XGetWindowAttributes(wm->dpy, win, &new_win_attr)) {
    wm_log(wm, LOG_ERROR, "%s: XGetWindowAttributes failed", __func__);
    return;
  }

#define BORDER  3
#define TITLE_HEIGHT 20
  int x, y, width, height;
  unsigned long valuemask;
  XColor border_color;
  //int status;
  char *colorstring = "#999933";

  x = new_win_attr.x - BORDER;
  y = new_win_attr.y - TITLE_HEIGHT - BORDER;
  width = new_win_attr.width + (BORDER * 2);
  height = new_win_attr.width + (BORDER * 2) + TITLE_HEIGHT;

  frame_attr.event_mask = (SubstructureNotifyMask | SubstructureRedirectMask \
                           | ButtonPressMask | ButtonReleaseMask \
                           | EnterWindowMask | LeaveWindowMask);
  XParseColor(wm->dpy, wm->screens[0]->cmap, colorstring, &border_color);
  XAllocColor(wm->dpy, wm->screens[0]->cmap, &border_color);
  wm_log(wm, LOG_INFO, "%s: color: %d.%d.%d = %d", __func__, border_color.red, border_color.green, border_color.blue, border_color.pixel);

  frame_attr.border_pixel = border_color.pixel;
  //frame_attr.background_pixel = border_color.pixel;

  valuemask = CWEventMask | CWBorderPixel;

  frame = XCreateWindow(wm->dpy, new_win_attr.root,
                        x, y, width, height,
                        BORDER,
                        CopyFromParent,
                        CopyFromParent,
                        new_win_attr.visual,
                        valuemask,
                        &frame_attr);

  /* AddToSaveSet tells X to remember the last parenting
   * so if we die, the client window we're reparenting doesn't die too. */
  XAddToSaveSet(wm->dpy, win);
  XReparentWindow(wm->dpy, win, frame, BORDER, TITLE_HEIGHT);

  XMapWindow(wm->dpy, win);
  XMapWindow(wm->dpy, frame);
  XRaiseWindow(wm->dpy, frame);
}

