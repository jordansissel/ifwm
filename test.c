
#include "wmlib.h"
#include <stdio.h>

int main(int argc, char **argv) {
  wm_t *wm = NULL;
  wm = wm_init(NULL);
  wm_set_log_level(wm, LOG_INFO);

  wm_main(wm);

  return 0;
}
