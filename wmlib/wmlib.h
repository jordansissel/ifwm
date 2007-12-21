#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/shape.h>

struct wm;
typedef struct wm wm_t;

typedef void (*event_handler)(wm_t *wm, XEvent *ev);
struct wm {
  Display *dpy;
  int log_level;
  Screen **screens;
  event_handler *event_handlers;
};

#define LOG_FATAL 0
#define LOG_ERROR 1
#define LOG_WARN  2
#define LOG_INFO  3

/* XXX: Check if we have __FUNCTION__ */
#define __func__ __FUNCTION__

wm_t *wm_init(char *display_name);

int wm_get_log_level(wm_t *wm, int log_level);
void wm_log(wm_t *wm, int log_level, char *format, ...);
void wm_set_log_level(wm_t *wm, int log_level);

void wm_x_open(wm_t *wm, char *display_name);
void wm_x_init_screens(wm_t *wm);
void wm_x_init_handlers(wm_t *wm);
void wm_x_init_windows(wm_t *wm);

void wm_map_window(wm_t *wm, Window w);

void wm_event_keypress(wm_t *wm, XEvent *ev);
void wm_event_buttonpress(wm_t *wm, XEvent *ev);
void wm_event_buttonrelease(wm_t *wm, XEvent *ev);
void wm_event_configurerequest(wm_t *wm, XEvent *ev);
void wm_event_maprequest(wm_t *wm, XEvent *ev); 
void wm_event_clientmessage(wm_t *wm, XEvent *ev);
void wm_event_enternotify(wm_t *wm, XEvent *ev);
void wm_event_leavenotify(wm_t *wm, XEvent *ev);
void wm_event_propertynotify(wm_t *wm, XEvent *ev);
void wm_event_unmapnotify(wm_t *wm, XEvent *ev);
void wm_event_destroynotify(wm_t *wm, XEvent *ev);
void wm_event_unknown(wm_t *wm, XEvent *ev);
