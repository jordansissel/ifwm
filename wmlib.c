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

#include "wmlib.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

wm_t *wm_init(char *display_name) {
  wm_t *wm = NULL;
  wm = malloc(sizeof(wm_t));
  memset(wm, 0, sizeof(wm_t));

  wm_x_open(wm, display_name);
  wm_x_init_screens(wm);
  return wm;
}

void wm_x_init_screens(wm_t *wm) {
  int num_screens = 0;
  int i;
  XSetWindowAttributes attr;

  attr.event_mask = SubstructureNotifyMask | SubstructureRedirectMask \
                    | EnterWindowMask | LeaveWindowMask;

  num_screens = ScreenCount(wm->dpy);
  wm->screens = malloc(num_screens * sizeof(Screen*));
  for (i = 0; i < num_screens; i++) {
    wm->screens[i] = ScreenOfDisplay(wm->dpy, i);
    XChangeWindowAttributes(wm->dpy, wm->screens[i]->root, CWEventMask, &attr);
    XSelectInput(wm->dpy, wm->screens[i]->root, attr.event_mask);
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
    wm_log(wm, LOG_INFO, "XNextEvent...");
    XNextEvent(wm->dpy, &ev);
    switch (ev.type) {
      case KeyPress:
        wm_event_keypress(wm, &ev); break;
      case ButtonPress:
        wm_event_buttonpress(wm, &ev); break;
      case ConfigureRequest:
        wm_event_configurerequest(wm, &ev); break;
      case MapRequest:
        wm_event_maprequest(wm, &ev); break;
      case ClientMessage:
        wm_event_clientmessage(wm, &ev); break;
      case EnterNotify:
        wm_event_enternotify(wm, &ev); break;
      case PropertyNotify:
        wm_event_propertynotify(wm, &ev); break;
      case UnmapNotify:
        wm_event_unmapnotify(wm, &ev); break;
      defult:
        wm_log(wm, LOG_INFO, "Unknown event: %d", ev.type);
        break;
    }
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

void wm_event_configurerequest(wm_t *wm, XEvent *ev) {
  XConfigureRequestEvent crev = ev->xconfigurerequest;
  XWindowChanges wc;
  wm_log(wm, LOG_INFO, "%s", __func__);

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

  if (!XGetWindowAttributes(wm->dpy, mrev.window, &attr)) {
    wm_log(wm, LOG_ERROR, "%s: XGetWindowAttributes failed", __func__);
    return;
  }

  if (attr.override_redirect) {
    wm_log(wm, LOG_INFO, "%s: skipping window %d, override_redirect is set",
           __func__, mrev.window);
    return;
  }

  XMapWindow(wm->dpy, mrev.window);
}

void wm_event_clientmessage(wm_t *wm, XEvent *ev) {
  XClientMessageEvent cmev = ev->xclient;
  wm_log(wm, LOG_INFO, "%s", __func__);

}

void wm_event_enternotify(wm_t *wm, XEvent *ev) {
  XEnterWindowEvent ewev = ev->xcrossing;
  wm_log(wm, LOG_INFO, "%s", __func__);

}

void wm_event_propertynotify(wm_t *wm, XEvent *ev) {
  XPropertyEvent pev = ev->xproperty;
  wm_log(wm, LOG_INFO, "%s", __func__);

}

void wm_event_unmapnotify(wm_t *wm, XEvent *ev) {
  XUnmapEvent uev = ev->xunmap;
  wm_log(wm, LOG_INFO, "%s", __func__);

}

