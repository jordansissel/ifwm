#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tsawm.h"

#define BORDER 0
#define TITLE_HEIGHT 15

static void *xmalloc(size_t size) {
  void *ptr;
  ptr = malloc(size);
  if (ptr == NULL) {
    fprintf(stderr, "malloc(%td) failed\n", size);
    exit(1);
  }
  memset(ptr, 0, size);
  return ptr;
}

container_t *current_container;
XContext container_context;
XContext client_container_context;

int main(int argc, char **argv) {
  wm_t *wm = NULL;
  int i;
  wm = wm_create(NULL);
  wm_set_log_level(wm, LOG_INFO);

  container_context = XUniqueContext();
  client_container_context = XUniqueContext();

  wm_log(wm, LOG_INFO, "== num screens: %d", wm->num_screens);
  for (i = 0; i < wm->num_screens; i++) {
    XWindowAttributes attr;
    Window root = wm->screens[i]->root;;
    container_t *root_container;
    XGetWindowAttributes(wm->dpy, root, &attr);
    root_container = container_new(wm, root, attr.x, attr.y, attr.width, attr.height);
    container_show(root_container);
    wm_log(wm, LOG_INFO, "Setting current container to %tx", root_container);
    current_container = root_container;

    /* Grab keys */
    XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, XK_j), Mod1Mask, root, False, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm->dpy, XKeysymToKeycode(wm->dpy, XK_h), Mod1Mask, root, False, GrabModeAsync, GrabModeAsync);
  }

  container_focus(current_container);
  wm_listener_add(wm, WM_EVENT_MAPREQUEST, addwin);
  wm_listener_add(wm, WM_EVENT_MAPNOTIFY, addwin);
  wm_listener_add(wm, WM_EVENT_UNMAPNOTIFY, unmap);
  wm_listener_add(wm, WM_EVENT_ENTERNOTIFY, focus_container);
  wm_listener_add(wm, WM_EVENT_EXPOSE, expose_container);
  wm_listener_add(wm, WM_EVENT_KEYDOWN, keydown);
  wm_listener_add(wm, WM_EVENT_KEYUP, keyup);

  /* Start main loop. At this point, our code will only execute when events
   * happen */
  XSync(wm->dpy, False);
  wm_main(wm);

  return 0;
}

container_t *container_new(wm_t *wm, Window parent, int x, int y,
                           int width, int height) {
  XWindowAttributes attr;
  container_t *container;
  container = xmalloc(sizeof(container_t));
  container->context = XUniqueContext();
  container->frame = mkframe(wm, parent, x, y, width, height);
  //container->title = mktitle(wm, parent, x, y, width, height);
  container->wm = wm;
  container->focused = False;

  XGetWindowAttributes(wm->dpy, parent, &attr);
  container->screen = attr.screen;

  container_create_gc(container);
  XSaveContext(wm->dpy, container->frame, container_context, (XPointer)container);
  return container;
}

Bool container_create_gc(container_t *container) {
  unsigned long valuemask;
  XGCValues gcv;
  XColor bg_color, fg_color;

  XParseColor(container->wm->dpy, container->screen->cmap, "#000000", &bg_color);
  XAllocColor(container->wm->dpy, container->screen->cmap, &bg_color);

  XParseColor(container->wm->dpy, container->screen->cmap, "#FFFFFF", &fg_color);
  XAllocColor(container->wm->dpy, container->screen->cmap, &fg_color);
  gcv.line_style = LineSolid;
  gcv.line_width = 1;
  gcv.fill_style = FillSolid;
  gcv.background = bg_color.pixel;
  //gcv.foreground = fg_color.pixel;
  gcv.foreground = bg_color.pixel;
  valuemask = (GCLineStyle | GCLineWidth | GCFillStyle \
               | GCForeground | GCBackground);
  container->gc = XCreateGC(container->wm->dpy, container->frame, valuemask, &gcv);
  return True;
}

Bool container_show(container_t *container) {
  XEvent ev;
  XMapWindow(container->wm->dpy, container->frame);
  XCheckTypedWindowEvent(container->wm->dpy, container->frame, MapNotify, &ev);
  container_paint(container);
  wm_log(container->wm, LOG_INFO, "%s: finished waiting for map event", __func__);
  XFlush(container->wm->dpy);
  return True;
}

Bool container_client_add(container_t *container, client_t *client) {
  XWindowAttributes attr;
  int ret;
  container_t *tmp = NULL;

  ret = XFindContext(container->wm->dpy, client->window, container_context,
                     (XPointer*)&tmp);
  if (ret != XCNOENT) {
    wm_log(container->wm, LOG_INFO, "%s: ignoring attempt to add container as a client: %d", __func__, client->window);
    return False;
  }

  /* Ignore the add if this window already belongs to a container */
  ret = XFindContext(container->wm->dpy, client->window, client_container_context,
                     (XPointer*)&tmp);
  if (ret != XCNOENT) {
    wm_log(container->wm, LOG_INFO, "%s: window %d already in a container, ignoring mapnotify", __func__, client->window);
    return False;
  }

  wm_log(container->wm, LOG_INFO, "%s: client add window %d", __func__, client->window);
  XAddToSaveSet(container->wm->dpy, client->window);
  XGetWindowAttributes(container->wm->dpy, container->frame, &attr);
  XSetWindowBorderWidth(container->wm->dpy, client->window, 0);
  XSelectInput(container->wm->dpy, client->window, CLIENT_EVENT_MASK);
  XReparentWindow(container->wm->dpy, client->window, container->frame, 0, TITLE_HEIGHT);
  XResizeWindow(container->wm->dpy, client->window, attr.width, attr.height - TITLE_HEIGHT);
  XSaveContext(container->wm->dpy, client->window, client_container_context, (XPointer)container);

  //container_paint(container);
  container_client_show(container, client);
  return True;
}

Bool container_paint(container_t *container) {
  XWindowAttributes frame_attr;

  //wm_log(container->wm, LOG_INFO, "%s: Painting container window %d", __func__, container->frame);

  XGetWindowAttributes(container->wm->dpy, container->frame, &frame_attr);
  XFillRectangle(container->wm->dpy, container->frame, container->gc,
                 0, 0, frame_attr.width, frame_attr.height);
  XFlush(container->wm->dpy);
  return True;
}

Bool container_client_show(container_t *container, client_t *client) {
  XMapRaised(container->wm->dpy, client->window);
  XSetInputFocus(container->wm->dpy, client->window, RevertToParent, CurrentTime);
  XFlush(container->wm->dpy);
  return True;
}

Bool addwin(wm_t *wm, wm_event_t *event) {
  container_client_add(current_container, event->client);
  XMapWindow(wm->dpy, event->client->window);
  return True;
}

Bool focus_container(wm_t *wm, wm_event_t *event) {
  container_t *container;
  int ret;

  //wm_log(wm, LOG_INFO, "%s", __func__);

  ret = XFindContext(wm->dpy, event->client->window, client_container_context,
                     (XPointer*)&container);
  if (ret == XCNOENT) {
    /* See if this window is a container */
    ret = XFindContext(wm->dpy, event->client->window, container_context,
                       (XPointer*)&container);
    if (ret == XCNOENT) {
      wm_log(wm, LOG_INFO, "no container found for window %d, can't focus.", event->client->window);
      return False;
    }
  }

  if (current_container == container) {
    //wm_log(wm, LOG_INFO, "%s: ignoring attempt to focus and blur the same container at the same time.", __func__);
    //return True;
  }
  container_blur(current_container);
  current_container = container;
  container_focus(container);

  return True;
}

Bool expose_container(wm_t *wm, wm_event_t *event) {
  container_t *container;
  int ret;

  //wm_log(wm, LOG_INFO, "%s!!!", __func__);

  ret = XFindContext(wm->dpy, event->client->window, container_context, 
                     (XPointer*)&container);
  if (ret == XCNOENT) {
    wm_log(wm, LOG_INFO, "no container for window %d, can't expose.", event->client->window);
    return False;
  }

  if (event->xevent->xexpose.count == 0)
    container_paint(container);
  return True;
}

Bool keydown(wm_t *wm, wm_event_t *event) {
  XKeyEvent kev = event->xevent->xkey;
  KeySym sym;

  wm_log(wm, LOG_INFO, "%s", __func__);
  sym = XKeycodeToKeysym(wm->dpy, kev.keycode, 0);
  wm_log(wm, LOG_INFO, "%s: key %d / %d", __func__, sym, XK_j);
  if (kev.state == Mod1Mask) {
    switch (sym) {
      case XK_j:
        container_split(current_container, SPLIT_VERTICAL);
        break;
      case XK_h:
        container_split(current_container, SPLIT_HORIZONTAL);
        break;
      default:
        wm_log(wm, LOG_WARN, "%s: unexpected keysym %d", __func__, sym);
    }
  }
  switch (sym) {
    case XK_Return:
      run("xterm -bg black -fg white");
      break;
  }
  XUngrabServer(wm->dpy);
  return True;
}

Bool keyup(wm_t *wm, wm_event_t *event) {
  XUngrabServer(wm->dpy);
  return True;
}

Bool unmap(wm_t *wm, wm_event_t *event) {
  client_t *client = event->client;
  container_t *container = NULL;
  wm_log(wm, LOG_INFO, "%s; unmap on %d", __func__, client->window);
  XFindContext(wm->dpy, client->window, client_container_context, (XPointer *)&container);
  XDeleteContext(wm->dpy, client->window, client_container_context);
  if (container != NULL) {
    wm_log(wm, LOG_INFO, "%s; unmap window", __func__);
  }

  //XGrabServer(wm->dpy);
  //XReparentWindow(wm->dpy, client->window, client->screen->root, 0, 0);
  //XUngrabServer(wm->dpy);
  return True;
}

Window mkframe(wm_t *wm, Window parent, int x, int y, int width, int height) {
  Window frame;
  XSetWindowAttributes frame_attr;
  XWindowAttributes parent_attr;
  unsigned long valuemask;
  XColor border_color;
  Visual *visual;

  XGetWindowAttributes(wm->dpy, parent, &parent_attr);
  visual = parent_attr.screen->root_visual;

  XParseColor(wm->dpy, parent_attr.screen->cmap, "#999933", &border_color);
  XAllocColor(wm->dpy, parent_attr.screen->cmap, &border_color);
  frame_attr.border_pixel = border_color.pixel;
  frame_attr.event_mask = (ButtonPressMask | ButtonReleaseMask \
                           | EnterWindowMask | LeaveWindowMask);

  valuemask = CWEventMask | CWBorderPixel;

  frame = XCreateWindow(wm->dpy, parent,
                        x, y, width, height,
                        BORDER, CopyFromParent, CopyFromParent,
                        visual, valuemask, &frame_attr);
  wm_log(wm, LOG_INFO, "%s; Created window %d", __func__, frame);

  XSelectInput(wm->dpy, frame, FRAME_EVENT_MASK);
  return frame;
}


Bool container_blur(container_t *container) {
  container->focused = False;
  return True;
}

Bool container_focus(container_t *container) {
  Window dummy;
  Window *children;
  unsigned int nchildren;
  //int ret;

  container->focused = True;

  XQueryTree(container->wm->dpy, container->frame, &dummy, &dummy, &children, &nchildren);
  wm_log(container->wm, LOG_INFO, "%s: num children of container: %ud", __func__, nchildren);
  XSetInputFocus(container->wm->dpy, container->frame, RevertToParent, CurrentTime);
  if (nchildren > 0) {
    client_t *client = NULL;
    int i = 1;
    for (i = nchildren - 1; i >= 0; i--) {
      client = wm_get_client(container->wm, children[i], False);
      wm_log(container->wm, LOG_INFO, "%s: child[%d] = %d", __func__, i, client->window);
      if (client != NULL && client->flags & CLIENT_VISIBLE)
        break;
    }
    if (client != NULL) {
      XMapRaised(container->wm->dpy, client->window);
      XSetInputFocus(container->wm->dpy, client->window, RevertToParent, CurrentTime);
    }
  }
  return True;
}

Bool container_split(container_t *container, unsigned int split_type) {
  XWindowAttributes attr;
  int new_x, new_y;
  unsigned int width, height;
  container_t *new_container;

  wm_log(container->wm, LOG_INFO, "%s: horizontal split", __func__);

  XGetWindowAttributes(container->wm->dpy, container->frame, &attr);
  if (split_type == SPLIT_VERTICAL) {
    width = attr.width / 2;
    height = attr.height;
    new_x = attr.x + width;
    new_y = attr.y;
  } else { /* SPLIT_HORIZONTAL */
    width = attr.width;
    height = attr.height / 2;
    new_x = attr.x;
    new_y = attr.y + height;
  }

  container_moveresize(container, attr.x, attr.y, width, height);
  new_container = container_new(container->wm, attr.root,
                                new_x, new_y, width, height);
  container_show(new_container);
  XFlush(container->wm->dpy);

  container_relocate_top_client(container, new_container);
  container_paint(container);
  container_paint(new_container);
  return True;
}

Bool container_relocate_top_client(container_t *src, container_t *dest) {
  Window *children = NULL;
  Window dummy;
  unsigned int nchildren;

  /* Move the top window on container to new_container */
  XQueryTree(src->wm->dpy, src->frame, &dummy, &dummy, &children, &nchildren);
  if (nchildren == 0)
    return True;

  client_t *client = NULL;
  client = wm_get_client(src->wm, children[nchildren - 1], False);
  if (client == NULL) {
    wm_log(src->wm, LOG_ERROR, "%s: XFindContext for top child window of container %d failed?", __func__, src->frame);
    return False;
  }

  container_client_add(dest, client);
  XFree(children);
  return True;
}

Bool container_moveresize(container_t *container, int x, int y, unsigned int width, unsigned int height) {
  Window *children = NULL;
  Window dummy;
  unsigned int nchildren;
  unsigned int i;
  XMoveResizeWindow(container->wm->dpy, container->frame, x, y, width, height);

  XQueryTree(container->wm->dpy, container->frame, &dummy, &dummy, &children, &nchildren);

  for (i = 0; i < nchildren; i++)
    XResizeWindow(container->wm->dpy, children[i], width, height);

  if (children != NULL)
    XFree(children);
  return True;
}

Bool run(const char *cmd) {
  char *args[4];
  args[0] = "/bin/sh";
  args[1] = "-c";
  args[2] = (char *)cmd;
  args[3] = NULL;

  if (fork() == 0)
    execvp(args[0], args);

  return True;
}
