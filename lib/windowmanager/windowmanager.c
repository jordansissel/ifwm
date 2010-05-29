/*
 * provide a "window manager" library.
 * + accept all necessary events
 * + Allow registry of events
 *   | Register events.
 *   | Store list of windows, state, etc
 * + Provide querying of window list without hitting the X server?
 * + Store additional properties for all windows via optional struct.
 */

/**
 * TODO(sissel): wm_add_window should really be a callback registered by the
 * consumer of this library.
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

#include "windowmanager.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#define DISPLAY_TO_WM_XID (0)
static int wm_x_event_error(Display *dpy, XErrorEvent *ev);

static int global_init = 0;
static wm_t *global_wm;

static int tracecount = 0;
static int xtracer() {
  tracecount++;
  fprintf(stderr, "LIB CALL: %d\n", tracecount);
  return 0;
}
 
static void *xmalloc(size_t size) {
  void *ptr;
  ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, "malloc(%td) failed\n", size);
    exit(1);
  }
  memset(ptr, 0, size);
  return ptr;
} /* static void *xmalloc */

wm_t *wm_new(void) {
  return wm_new2(NULL);
} /* wm_t *wm_new */

wm_t *wm_new2(char *display_name) {
  wm_t *wm = NULL;

  int i;
  wm = xmalloc(sizeof(wm_t));
  wm_x_open(wm, display_name);
  wm_x_init_screens(wm);
  //_Xdebug = 1;
  //XSynchronize(wm->dpy, True);
  wm->xdo = xdo_new_with_opened_display(wm_x_get_display(wm), NULL, True);

  if (global_init == 0) {
    global_init = 1;
    global_wm = wm;
  }

  /* Initialize the listeners lists */
  wm->listeners = malloc(WM_EVENT_MAX * sizeof(GPtrArray *));
  for (i = WM_EVENT_MIN; i < WM_EVENT_MAX; i++)
    wm->listeners[i] = g_ptr_array_new();

  return wm;
} /* wm_t *wm_create(char *display_name) */

void wm_x_init_screens(wm_t *wm) {
  int num_screens;
  int i;
  XSetWindowAttributes attr;

  attr.event_mask = SubstructureNotifyMask | SubstructureRedirectMask \
                    | EnterWindowMask | LeaveWindowMask;

  num_screens = ScreenCount(wm->dpy);

  wm_log(wm, LOG_INFO, "setting num screens: %d", num_screens);
  wm->num_screens = num_screens;
  wm->screens = xmalloc(num_screens * sizeof(Screen*));
  for (i = 0; i < num_screens; i++) {
    int ret;
    wm->screens[i] = ScreenOfDisplay(wm->dpy, i);
    XChangeWindowAttributes(wm->dpy, wm->screens[i]->root, CWEventMask, &attr);
    XSelectInput(wm->dpy, wm->screens[i]->root, attr.event_mask);
  }
} /* void wm_x_init_screens */

void wm_x_init_handlers(wm_t *wm) {
  int i;
  wm->x_event_handlers = xmalloc(LASTEvent * sizeof(x_event_handler_func));

  /* Default event handler is 'unknown' */
  /* LASTEvent from X11/X.h is the max event value */
  for (i = 0; i < LASTEvent; i++)
    wm->x_event_handlers[i] = wm_event_unknown;

  wm->x_event_handlers[KeyPress] = wm_event_keypress;
  wm->x_event_handlers[KeyRelease] = wm_event_keyrelease;
  wm->x_event_handlers[ButtonPress] = wm_event_buttonpress;
  wm->x_event_handlers[ButtonRelease] = wm_event_buttonrelease;
  wm->x_event_handlers[ConfigureRequest] = wm_event_configurerequest;
  wm->x_event_handlers[ConfigureNotify] = wm_event_configurenotify;
  wm->x_event_handlers[MapRequest] = wm_event_maprequest;
  wm->x_event_handlers[CreateNotify] = wm_event_createnotify;
  wm->x_event_handlers[MapNotify] = wm_event_mapnotify;
  wm->x_event_handlers[ClientMessage] = wm_event_clientmessage;
  wm->x_event_handlers[EnterNotify] = wm_event_enternotify;
  wm->x_event_handlers[LeaveNotify] = wm_event_leavenotify;
  wm->x_event_handlers[PropertyNotify] = wm_event_propertynotify;
  wm->x_event_handlers[UnmapNotify] = wm_event_unmapnotify;
  wm->x_event_handlers[DestroyNotify] = wm_event_destroynotify;
  wm->x_event_handlers[Expose] = wm_event_expose;

  global_wm = wm;
  //XSetErrorHandler(wm_x_event_error);
} /* void wm_x_init_handlers */

int wm_x_event_error(Display *dpy, XErrorEvent *ev) {
  wm_log(global_wm, LOG_ERROR, "x11 error for request %lu. resource %lu\n",
         ev->serial, ev->resourceid);
} /* int wm_x_event_error */

void wm_x_init_windows(wm_t *wm) {
  Window root, parent, *wins;
  unsigned int nwins;
  int screen;
  int nscreens;
  int i;

  XGrabServer(wm->dpy);
  nscreens = ScreenCount(wm->dpy);
  for (screen = 0; screen < nscreens; screen++) {
    wm_log(wm, LOG_INFO, "Querying window tree for screen %d", screen);
    XQueryTree(wm->dpy, wm->screens[screen]->root, &root, &parent, &wins, &nwins);
    for (i = 0; i < nwins; i++) {
      wm_fake_maprequest(wm, wins[i]);
    }
  }
  XUngrabServer(wm->dpy);
} /* void wm_x_init_windows */

void wm_set_log_level(wm_t *wm, int log_level) {
  if (log_level < 0)
    log_level = 0;
  wm->log_level = log_level;
} /* void wm_set_log_level */

int wm_get_log_level(wm_t *wm) {
  return wm->log_level;
} /* int wm_get_log_level */

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

  if (log_level == LOG_FATAL) {
    exit(1);
  }
} /* void wm_log */

void wm_x_open(wm_t *wm, char *display_name) {
  wm_log(wm, LOG_INFO, "Opening display '%s'", display_name);
  wm->dpy = XOpenDisplay(display_name);
  if (wm->dpy == NULL)
    wm_log(wm, LOG_FATAL, "Failed opening display: '%s'", display_name);

  wm->context = XUniqueContext();
} /* void wm_x_open */

void wm_main(wm_t *wm) {
  XEvent ev;

  wm_x_init_handlers(wm);
  wm_x_init_windows(wm);

  for (;;) {
    XNextEvent(wm->dpy, &ev);
    wm->x_event_handlers[ev.type](wm, &ev);
  }
}

void wm_event_keypress(wm_t *wm, XEvent *ev) {
  XKeyEvent kev = ev->xkey;
  client_t *client;
  wm_log(wm, LOG_INFO, "%s", __func__);
  client = wm_get_client(wm, kev.window, True);
  wm_listener_call(wm, WM_EVENT_KEY_DOWN, client, ev);
}

void wm_event_keyrelease(wm_t *wm, XEvent *ev) {
  XKeyEvent kev = ev->xkey;
  client_t *client;
  wm_log(wm, LOG_INFO, "%s", __func__);
  client = wm_get_client(wm, kev.window, True);
  wm_listener_call(wm, WM_EVENT_KEY_UP, client, ev);
}

void wm_get_mouse_position(wm_t *wm, int *x, int *y, Window window) {
  Window unused_root, unused_child;
  unsigned int unused_mask;
  int unused_root_x, unused_root_y;

  XQueryPointer(wm->dpy, window, &unused_root, &unused_child,
                &unused_root_x, &unused_root_y, x, y, &unused_mask);
} /* void wm_get_mouse_position */

void wm_event_buttonpress(wm_t *wm, XEvent *ev) {
  XButtonEvent bev = ev->xbutton;
  XWindowAttributes attr;
  int offset_x, offset_y;
  int old_win_x, old_win_y;
  wm_log(wm, LOG_INFO, "%s", __func__);
  XGetWindowAttributes(wm->dpy, bev.window, &attr);

  // GrabPointer for mousemask
  XGrabPointer(wm->dpy, attr.root, False, MouseEventMask,
               GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

  old_win_x = attr.x;
  old_win_x = attr.y;
  wm_get_mouse_position(wm, &offset_x, &offset_y, attr.root);

  offset_x = offset_x - attr.x;
  offset_y = offset_y - attr.y;

  if (attr.screen->root != bev.window) {
    /* Window button event */
    for (;;) {
      XEvent ev;
      XMaskEvent(wm->dpy, MouseEventMask | ExposureMask, &ev);
      switch (ev.type) {
        case MotionNotify:
          XMoveWindow(wm->dpy, bev.window,
                      ev.xmotion.x - offset_x,
                      ev.xmotion.y - offset_y);
          break;
        case ButtonRelease:
          XUngrabPointer(wm->dpy, CurrentTime);
          return;
          break;
        case Expose:
          wm->x_event_handlers[ev.type](wm, &ev);
          break;
      }
    }
  } else {
    /* Root button event */
  }
} /* void wm_event_buttonpress */

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
} /* wm_event_configurenotify */

void wm_event_configurenotify(wm_t *wm, XEvent *ev) {
  XConfigureEvent cev;
  XWindowChanges changes;
  unsigned long valuemask = 0;
  //wm_log(wm, LOG_INFO, "%s: %d reconfigured.", __func__, cev.window);

  if (cev.border_width > 0) {
    changes.border_width = 0;
    valuemask |= CWBorderWidth;
  }
}

void wm_event_createnotify(wm_t *wm, XEvent *ev) {
  XCreateWindowEvent xcwe = ev->xcreatewindow;
  wm_log(wm, LOG_INFO, "===> CREATE NOTIFY");
  XGrabServer(wm->dpy);
  wm_get_client(wm, xcwe.window, True);
  XUngrabServer(wm->dpy);
}

void wm_event_maprequest(wm_t *wm, XEvent *ev) {
  XMapRequestEvent mrev = ev->xmaprequest;
  XWindowAttributes attr;
  client_t *client;
  wm_log(wm, LOG_INFO, "%s: window %d", __func__, mrev.window);

  /* I grab here because everyone else seems to do this */
  XGrabServer(wm->dpy);

  client = wm_get_client(wm, mrev.window, True);
  if (client == NULL)
    return;

  client->flags |= CLIENT_VISIBLE;

  if (client->attr.override_redirect) {
    wm_log(wm, LOG_INFO, "%s: skipping window %d, override_redirect is set",
           __func__, mrev.window);
    XMapWindow(wm->dpy, client->window);
    XUngrabServer(wm->dpy);
    return;
  }

  wm_listener_call(wm, WM_EVENT_WINDOW_MAP_REQUEST, client, ev);
  XUngrabServer(wm->dpy);
}

void wm_event_mapnotify(wm_t *wm, XEvent *ev) {
  XMapEvent mev = ev->xmap;
  client_t *client;
  wm_log(wm, LOG_INFO, "%s: mapnotify %d", __func__, mev.window);
  client = wm_get_client(wm, mev.window, True);

  if (client == NULL) {
    wm_log(wm, LOG_ERROR, 
           "%s: got mapnotify for a client (window: %ld) that is null, input only?",
           __func__, mev.window);
    return;
  }

  client->flags |= CLIENT_VISIBLE;
  wm_listener_call(wm, WM_EVENT_WINDOW_MAP, client, ev);
}

void wm_event_clientmessage(wm_t *wm, XEvent *ev) {
  XClientMessageEvent cmev = ev->xclient;
  wm_log(wm, LOG_INFO, "%s: Window %ld, atom %ld(%s), format %ld",
         __func__, cmev.window, cmev.message_type,
         XGetAtomName(wm->dpy, cmev.message_type), cmev.format);
}

void wm_event_enternotify(wm_t *wm, XEvent *ev) {
  XEnterWindowEvent ewev = ev->xcrossing;
  client_t *client;
  wm_log(wm, LOG_INFO, "%s: window %ld", __func__, ewev.window);

  client = wm_get_client(wm, ewev.window, False);
  wm_listener_call(wm, WM_EVENT_WINDOW_ENTER, client, ev);
}

void wm_event_leavenotify(wm_t *wm, XEvent *ev) {
  XEnterWindowEvent ewev = ev->xcrossing;
  //wm_log(wm, LOG_INFO, "%s", __func__);
}

void wm_event_propertynotify(wm_t *wm, XEvent *ev) {
  XPropertyEvent pev = ev->xproperty;
  client_t *client;
  wm_log(wm, LOG_INFO, "%s: window %d changed property atom %d (%s)", __func__,
         pev.window, pev.atom, 
         (pev.state == PropertyDelete ? "deleted" : "changed"));
  client = wm_get_client(wm, pev.window, False);

  /* XInternAtom has an internal cache local to the client, so we don't
   * need to worry about caching these ourselves. */
  if ((pev.atom == XInternAtom(wm->dpy, "WM_NAME", False))
      || (pev.atom == XInternAtom(wm->dpy, "_NET_WM_NAME", False))
      || (pev.atom == XInternAtom(wm->dpy, "_NET_WM_VISIBLE_NAME", False))) {
    wm_log(wm, LOG_INFO, "%s: WINDOW NAME CHANGED", __func__);
  }

  switch (pev.state) {
    case PropertyNewValue:
      wm_listener_call(wm, WM_EVENT_WINDOW_PROPERTY_CHANGE, client, ev);
      break;
    case PropertyDelete:
      wm_listener_call(wm, WM_EVENT_WINDOW_PROPERTY_DELETE, client, ev);
      break;
  }
}

void wm_event_unmapnotify(wm_t *wm, XEvent *ev) {
  XUnmapEvent uev = ev->xunmap;
  client_t *client;

  /* Ignore unmaps of subwindows for our clients */
  if (uev.event != uev.window)
    return;
  wm_log(wm, LOG_INFO, "%s: Unmap %d", __func__, uev.window);
  client = wm_get_client(wm, uev.window, False);

  client->flags &= ~(CLIENT_VISIBLE);
  wm_listener_call(wm, WM_EVENT_WINDOW_UNMAP, client, ev);
  wm_remove_client(wm, client);
}

void wm_event_destroynotify(wm_t *wm, XEvent *ev) {
  XDestroyWindowEvent dev = ev->xdestroywindow;
  Window parent = dev.event;
  wm_log(wm, LOG_INFO, "%s: Window %ld (parent %ld) was destroyed.",
         __func__, dev.window, parent);

  //XDestroyWindow(wm->dpy, parent);
}

void wm_event_expose(wm_t *wm, XEvent *ev) {
  XExposeEvent eev = ev->xexpose;
  client_t *client;

  //wm_log(wm, LOG_INFO, "%s: expose event on window '%d'", __func__, eev.window);
  client = wm_get_client(wm, eev.window, False);
  wm_listener_call(wm, WM_EVENT_EXPOSE, client, ev);
}

void wm_event_unknown(wm_t *wm, XEvent *ev) {
  //wm_log(wm, LOG_INFO, "%s: Unknown event type '%d'", __func__, ev->type);
}

void wm_listener_add(wm_t *wm, wm_event_id event, wm_event_handler_func callback,
                     gpointer data) {
  wm_log(wm, LOG_INFO, "Adding listener for event %d: %016tx", 
         event, callback);

  if (event >= WM_EVENT_MAX) {
    wm_log(wm, LOG_FATAL, 
           "Attempt to register for event '%d' when max event is '%d'",
           event, WM_EVENT_MAX);
  }

  wm_event_handler_t *handler = xmalloc(sizeof(wm_event_handler_t));
  handler->callback = callback;
  handler->data = data;
  g_ptr_array_add(wm->listeners[event], handler);
}

void wm_listener_call(wm_t *wm, unsigned int event_id, client_t *client, XEvent *ev) {
  wm_event_t event;
  wm_event_handler_t callback;

  //wm_log(wm, LOG_ERROR, "could not find client for window '%d'", mrev.window);

  if (client == NULL) {
    wm_log(wm, LOG_WARN, "%s: Rejecting call for event %d because client is null", __func__, event_id);
    return;
  }

  wm_log(wm, LOG_WARN, "%s: event %d for window %ld", __func__, event_id, client->window);

  event.event_id = event_id;
  event.xevent = ev;
  event.client = client;
  event.wm = wm;

  if (event_id >= WM_EVENT_MAX) {
    wm_log(wm, LOG_FATAL, 
           "Attempt to call listener for event '%d' when max event is '%d'",
           event, WM_EVENT_MAX);
  }

  GPtrArray *listeners;
  listeners = wm->listeners[event_id];
  if (listeners->len == 0) {
    wm_log(wm, LOG_INFO, "No callback registered for event %d", event_id);

    /* TODO(sissel): Why do we do this? */
    switch (event_id) {
      case ButtonPress:
      case ButtonRelease:
      case KeyPress:
      case KeyRelease:
        XUngrabServer(wm->dpy);
        break;
    }
    return;
  }

  g_ptr_array_foreach(listeners, wm_listener_call_each, &event);
} /* void wm_listener_call */

void wm_listener_call_each(gpointer data, gpointer g_wmevent) {
  wm_event_t *event = g_wmevent;
  wm_event_handler_t *handler = data;
  wm_log(event->wm, LOG_INFO, "Calling func %016tx", handler->callback);
  handler->callback(event->wm, event, handler->data);
}

/* Fake map requests are mainly to capture windows we don't know about that exist
 * prior to the startup of the window manager */
void wm_fake_maprequest(wm_t *wm, Window w) {
  XEvent e;
  XWindowAttributes attr;

  XGetWindowAttributes(wm->dpy, w, &attr);
  if (attr.map_state == IsViewable /* && attr.class != InputOnly */) {
    e.xmap.window = w;
    wm_log(wm, LOG_INFO, "fake map for: %d", w);
    wm_event_createnotify(wm, &e);
  }
}

Bool wm_grab_button(wm_t *wm, Window window, unsigned int mask, unsigned int button) {
  unsigned int event_mask = ButtonPressMask | ButtonReleaseMask;
  XGrabButton(wm->dpy, button, mask, window, False, event_mask,
              GrabModeAsync, GrabModeSync, None, None);
  XGrabButton(wm->dpy, button, LockMask|mask, window, False, event_mask,
              GrabModeAsync, GrabModeSync, None, None);
  /* handle numlock mask */
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

client_t *wm_get_client(wm_t *wm, Window window, Bool create_if_necessary) {
  client_t *c = NULL;
  int ret;
  wm_log(wm, LOG_INFO, "%s: window %ld", __func__, window);
  ret = XFindContext(wm->dpy, window, wm->context, (XPointer*)&c);

  if (ret == XCNOENT && create_if_necessary) { /* window not found */
    XWindowAttributes attr;
    XGrabServer(wm->dpy);
    Status ret;
    wm_log(wm, LOG_INFO, "New client window: %d", window);
    //ret = XGetWindowAttributes(wm->dpy, window, &attr);
    //if (attr.class == InputOnly) {
      //wm_log(wm, LOG_INFO, 
             //"%s: Window class is InputOnly, should we ignore this?", __func__);
      //return NULL;
    //}

    //wm_log(wm, LOG_INFO, "window: %d = %dx%d@%d,%d", 
           //window, attr.width, attr.height, attr.x, attr.y);

    c = xmalloc(sizeof(client_t));
    memset(c, 0, sizeof(client_t));
    c->window = window;
    c->screen = attr.screen;
    c->flags = 0;
    memcpy(&(c->attr), &attr, sizeof(XWindowAttributes));
    ret = XSaveContext(wm->dpy, window, wm->context, (XPointer)c);
    //wm_log(wm, LOG_INFO, "%s: XSaveContext(%ld) returned %d.", __func__, window, ret);
    if (ret != 0) {
      wm_log(wm, LOG_ERROR, "%s: XSaveContext failed.", __func__);
    }
    XSelectInput(wm->dpy, window, ClientWindowMask);
    XSync(wm->dpy, False);
    XUngrabServer(wm->dpy);
  }

  return c;
}

void wm_remove_client(wm_t *wm, client_t *client) {
  XDeleteContext(wm->dpy, client->window, wm->context);
}

Display *wm_x_get_display(wm_t *wm) {
  return wm->dpy;
}
