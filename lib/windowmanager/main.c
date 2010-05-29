#include <stdio.h>
#include <xdo.h>
#include <glib.h>
#include "windowmanager.h"

static wm_t *wm;
static xdo_t *xdo;

Bool focus_on_windowenter(wm_t *wm, wm_event_t *event, gpointer data) {
  printf("Focus: %ld\n", event->client->window);
  xdo_window_focus(wm->xdo, event->client->window);
  return True;
} /* Bool focus_on_windowenter */

Bool map_when_requested(wm_t *wm, wm_event_t *event, gpointer data) {
  xdo_t *xdo = data;
  printf("Map requested of window %ld\n", event->client->window);
  xdo_window_map(wm->xdo, event->client->window);
  return True;
} /* Bool map_when_requested */

Bool property_change(wm_t *wm, wm_event_t *event, gpointer data) {
  Atom atom_type;
  long nitems;
  int size;
  xdo_getwinprop(wm->xdo, event->client->window, event->xevent->xproperty.atom,
                 &nitems, &atom_type, &size);
  printf("Atom type: %ld\n", atom_type);
  printf("Atom '%s' changed\n", XGetAtomName(wm->dpy, event->xevent->xproperty.atom));
  return True;
}

int main() {
  wm = wm_new();

  wm_listener_add(wm, WM_EVENT_WINDOW_ENTER, focus_on_windowenter, NULL);
  wm_listener_add(wm, WM_EVENT_WINDOW_MAP_REQUEST, map_when_requested, NULL);
  wm_listener_add(wm, WM_EVENT_WINDOW_PROPERTY_CHANGE, property_change, NULL);
  //wm_listener_add(wm, WM_EVENT_WINDOW_NAME, property_change, NULL);

  wm_main(wm);

  return 0;
}
