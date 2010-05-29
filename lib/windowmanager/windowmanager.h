
#ifndef _WINDOWMANAGER_H_
#define _WINDOWMANAGER_H_

#include <X11/extensions/shape.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

//include "event_list.h"

#define LOG_FATAL 0
#define LOG_ERROR 1
#define LOG_WARN  2
#define LOG_INFO  3

struct wm;
typedef struct wm wm_t;
typedef struct wm_event wm_event_t;

typedef void (*x_event_handler)(wm_t *wm, XEvent *ev);
typedef Bool (*wm_event_handler)(wm_t *wm, wm_event_t *event);

struct wm {
  Display *dpy;
  int log_level;
  Screen **screens;
  int num_screens;
  x_event_handler *x_event_handlers;
  wm_event_handler *listeners;
  XContext context;
};

typedef struct client {
  Window window;
  XWindowAttributes attr;
  Screen *screen;
  unsigned int flags;
} client_t;

typedef unsigned int wm_event_id;
struct wm_event {
  wm_event_id event_id;
  client_t *client;
  XEvent *xevent;
};

#define ButtonEventMask ButtonPressMask|ButtonReleaseMask
#define MouseEventMask ButtonPressMask|ButtonReleaseMask|PointerMotionMask


// :'<,'>!sort | awk '{print $1, $2, NR"U"}'
#define WM_EVENT_MIN 1U
#define WM_EVENT_ENTERNOTIFY 1U
#define WM_EVENT_EXPOSE 2U
#define WM_EVENT_KEYDOWN 3U
#define WM_EVENT_KEYUP 4U
#define WM_EVENT_MAPNOTIFY 5U
#define WM_EVENT_MAPREQUEST 6U
#define WM_EVENT_MOUSE 7U
#define WM_EVENT_UNMAPNOTIFY 8U
#define WM_EVENT_MAX 9U

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

void wm_listener_add(wm_t *wm, wm_event_id event, wm_event_handler callback);
void wm_listener_call(wm_t *wm, unsigned int event_id, client_t *client, XEvent *ev);

void wm_get_mouse_position(wm_t *wm, int *x, int *y, Window window);
Bool wm_grab_button(wm_t *wm, Window window, unsigned int mask, unsigned int button);
client_t *wm_get_client(wm_t *wm, Window window, Bool create_if_necessary);
void wm_remove_client(wm_t *wm, client_t *client);

#endif /* _WINDOWMANAGER_H_ */
