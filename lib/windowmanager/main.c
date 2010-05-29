#include <stdio.h>
#include <xdo.h>
#include "windowmanager.h"

static wm_t *wm;
static xdo_t *xdo;

Bool focus_on_windowenter(wm_t *wm, wm_event_t *event) {
  printf("Focus: %ld\n", event->client->window);
  xdo_window_focus(xdo, event->client->window);
  return True;
} /* Bool focus_on_windowenter */

Bool map_when_requested(wm_t *wm, wm_event_t *event) {
  xdo_window_map(xdo, event->client->window);
  return True;
} /* Bool map_when_requested */

int main() {
  wm = wm_new();
  xdo = xdo_new_with_opened_display(wm_x_get_display(wm), NULL, True);

  wm_listener_add(wm, WM_EVENT_ENTERNOTIFY, focus_on_windowenter);
  wm_listener_add(wm, WM_EVENT_MAPREQUEST, map_when_requested);

  wm_main(wm);

  return 0;
}
