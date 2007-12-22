
#include "wmlib/wmlib.h"
#include <stdio.h>

#include "tsawm.h"

Bool addwin(wm_t *wm, wm_event_t *event);

int main(int argc, char **argv) {
  wm_t *wm = NULL;
  wm = wm_init(NULL);
  wm_set_log_level(wm, LOG_INFO);

  wm_listener_add(wm, WM_EVENT_MAPREQUEST, addwin);
  wm_main(wm);

  return 0;
}

Bool addwin(wm_t *wm, wm_event_t *event) {
  wm_log(wm, LOG_INFO, "addwin!");
  return True;
}
