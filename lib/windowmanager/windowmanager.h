#ifndef _WINDOWMANAGER_H_
#define _WINDOWMANAGER_H_

#include <X11/extensions/shape.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <xdo.h>

enum log_values {
  LOG_FATAL = 0,
  LOG_ERROR = 1,
  LOG_WARN = 2,
  LOG_INFO = 3
};

struct wm;
typedef struct wm wm_t;
typedef struct wm_event wm_event_t;

typedef void (*x_event_handler_func)(wm_t *wm, XEvent *ev);
typedef Bool (*wm_event_handler_func)(wm_t *wm, wm_event_t *event, gpointer data);

struct wm {
  Display *dpy;
  xdo_t *xdo;

  Screen **screens;
  int num_screens;

  int log_level;

  x_event_handler_func *x_event_handlers;
  GPtrArray **listeners;
  XContext context;
};

typedef struct wm_event_handler {
  wm_event_handler_func callback;
  gpointer data; /* aka 'void *' */
} wm_event_handler_t;

typedef struct client {
  Window window;
  XWindowAttributes attr;
  Screen *screen;
  unsigned int flags;
} client_t;

typedef unsigned int wm_event_id;
struct wm_event {
  wm_t *wm;
  wm_event_id event_id;
  client_t *client;
  XEvent *xevent;
};

#define ButtonEventMask ButtonPressMask | ButtonReleaseMask
#define MouseEventMask ButtonPressMask | ButtonReleaseMask | PointerMotionMask
#define ClientWindowMask \
  (EnterWindowMask | LeaveWindowMask | PropertyChangeMask | StructureNotifyMask)

/* 
 * Mapping of X11 events to libwindowmanager events:
 * WM_EVENT_KEY_DOWN => ButtonPress
 * WM_EVENT_KEY_UP => ButtonRelease
 * WM_EVENT_WINDOW_ENTER => EnterNotify
 * WM_EVENT_WINDOW_LEAVE => LeaveNotify
 * WM_EVENT_WINDOW_MAP => MapNotify
 * WM_EVENT_WINDOW_UNMAP => UnmapNotify
 * WM_EVENT_WINDOW_MAP_REQUEST => MapRequest
 * WM_EVENT_WINDOW_PROPERTY_CHANGE => PropertyNotify with state PropertyNewValue
 * WM_EVENT_WINDOW_PROPERTY_DELETE => PropertyNotify with state PropertyDelete
 */

// :!sort | awk '{print $1, $2, NR"U"}; END { print "\#define WM_EVENT_MAX "NR"U" }'
#define WM_EVENT_MIN 1U
#define WM_EVENT_EXPOSE 1U
#define WM_EVENT_KEY_DOWN 2U
#define WM_EVENT_KEY_UP 3U
#define WM_EVENT_MOUSE_MOTION 4U
#define WM_EVENT_WINDOW_ENTER 5U
#define WM_EVENT_WINDOW_LEAVE 6U
#define WM_EVENT_WINDOW_MAP 7U
#define WM_EVENT_WINDOW_MAP_REQUEST 8U
#define WM_EVENT_WINDOW_PROPERTY_CHANGE 9U
#define WM_EVENT_WINDOW_PROPERTY_DELETE 10U
#define WM_EVENT_WINDOW_UNMAP 11U
#define WM_EVENT_MAX 11U

/* Client flags */
#define CLIENT_VISIBLE 1U

/* TODO(sissel): Check if we have __FUNCTION__, this requires GCC, I think. */
#define __func__ __FUNCTION__

wm_t *wm_new();
wm_t *wm_new2(char *display_name);

void wm_main(wm_t *wm);

Display *wm_x_get_display(wm_t *wm);
void wm_log(wm_t *wm, int log_level, char *format, ...);
int wm_get_log_level(wm_t *wm);
void wm_set_log_level(wm_t *wm, int log_level);

void wm_x_open(wm_t *wm, char *display_name);
void wm_x_init_screens(wm_t *wm);
void wm_x_init_handlers(wm_t *wm);
void wm_x_init_windows(wm_t *wm);

void wm_map_window(wm_t *wm, Window w);

void wm_fake_maprequest(wm_t *wm, Window w);

void wm_event_keypress(wm_t *wm, XEvent *ev);
void wm_event_keyrelease(wm_t *wm, XEvent *ev);
void wm_event_buttonpress(wm_t *wm, XEvent *ev);
void wm_event_buttonrelease(wm_t *wm, XEvent *ev);
void wm_event_configurerequest(wm_t *wm, XEvent *ev);
void wm_event_configurenotify(wm_t *wm, XEvent *ev);
void wm_event_maprequest(wm_t *wm, XEvent *ev);
void wm_event_mapnotify(wm_t *wm, XEvent *ev);
void wm_event_clientmessage(wm_t *wm, XEvent *ev);
void wm_event_enternotify(wm_t *wm, XEvent *ev);
void wm_event_leavenotify(wm_t *wm, XEvent *ev);
void wm_event_propertynotify(wm_t *wm, XEvent *ev);
void wm_event_unmapnotify(wm_t *wm, XEvent *ev);
void wm_event_destroynotify(wm_t *wm, XEvent *ev);
void wm_event_expose(wm_t *wm, XEvent *ev);
void wm_event_unknown(wm_t *wm, XEvent *ev);

void wm_listener_add(wm_t *wm, wm_event_id event, wm_event_handler_func callback,
                     gpointer data);
void wm_listener_call(wm_t *wm, unsigned int event_id, client_t *client, XEvent *ev);
void wm_listener_call_each(gpointer data, gpointer g_wmevent);

void wm_get_mouse_position(wm_t *wm, int *x, int *y, Window window);
Bool wm_grab_button(wm_t *wm, Window window, unsigned int mask, unsigned int button);
client_t *wm_get_client(wm_t *wm, Window window, Bool create_if_necessary);
void wm_remove_client(wm_t *wm, client_t *client);

#endif /* _WINDOWMANAGER_H_ */
