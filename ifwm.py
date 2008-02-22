#!/usr/bin/env python

import os
import sys
import time

from Xlib import X, display, Xutil, XK
from Xlib.protocol import event as xevent
from Xlib.keysymdef import latin1

dpy = display.Display()
prog = ""

class WMActions(object):
  root_context = 1
  container_context = 2

  @staticmethod
  def runcmd(self, cmd):
    print "Running %s" % cmd
    os.spawnlp(os.P_NOWAIT, "/bin/sh", "/bin/sh", "-c", cmd)
  
  @staticmethod
  def split_frame(self, wm, horizontal=False, vertical=False):
    if not horizontal ^ vertical:
      print "Only one of horizontal or vertical split type permitted."
      return

    container = wm.container_stack[0]
    new_x = container.x
    new_y = container.y
    new_height = container.height
    new_width = container.width

    if horizontal:
      new_height = int(new_height / 2)
      new_y += new_height + container.border_width * 2
    if vertical:
      new_width = int(new_width / 2)
      new_x += new_width + container.border_width * 2

    if new_width < 30 or new_height < 30:
      print "Split is too small, refusing."
      return

    container.resize(width=new_width, height=new_height)
    wm.add_container(container.parent, 
                     x=new_x, y=new_y, width=new_width, height=new_height)
    print "Split horiz:%d / vert:%d" % (horizontal, vertical)

class Container(object):
  """ Window frame container. """

  def __init__(self, wm, parent, x, y, width, height):
    """ Create a Container (for other windows)
      Args:
        x: x position relative to parent
        y: y position relative to parent
        width: width
        height: height
    """
    self.children = {}
    self.wm = wm
    self.parent = parent
    self.title_height = 20
    self.border_width = 1
    self.x = x
    self.y = y
    self.width = width
    self.height = height

    self.calculate_client_area()

    self.client_stack = []

    mask = (X.ButtonPressMask | X.ButtonReleaseMask 
            | X.EnterWindowMask | X.LeaveWindowMask
            | X.KeyPressMask | X.KeyReleaseMask
            | X.ExposureMask)

    self.colormap = parent.get_attributes().colormap

    self.pixel_border = self.colormap.alloc_named_color("#FFFF00")
    self.pixel_bg = self.colormap.alloc_color(0,0,0)

    self.window = parent.create_window(x, y, self.width, self.height, self.border_width,
                                       X.CopyFromParent, X.InputOutput,
                                       event_mask=mask, border_pixel=self.pixel_border.pixel)
    self.gc_frame_bg = self.window.create_gc()
    self.gc_frame_border= self.window.create_gc()
    self.gc_title_fg = self.window.create_gc()
    self.gc_title_border = self.window.create_gc()
    self.gc_title_bg = self.window.create_gc()
    # XXX: Actually tell containers what screen and parent they have.
    self.title_font = self.wm.display.open_font("fixed")

    title_color = self.wm.display.screen(0).default_colormap.alloc_named_color("white")
    self.gc_title_font = self.window.create_gc(font=self.title_font, foreground=title_color.pixel)
    self.gc_border = self.window.create_gc()
    self.window.map()
    self.paint()

  def trim_border(self, pixels):
    return pixels# - (2 * self.border_width)

  def calculate_client_area(self):
    self.client_x = self.border_width 
    self.client_y = self.title_height
    self.client_width = self.width - (2 * self.border_width)
    self.client_height = self.height - (2 * self.border_width) - self.title_height

  def focus(self):
    self.window.set_input_focus(X.RevertToParent, X.CurrentTime)
    #print "Focused new container"
    if self.client_stack:
      #print "Focusing window in stack: %r" % self.client_stack[0]

      # The onerror here is to remove a window when it's gone bad. Not sure why
      # it happens, perhaps a race.
      self.client_stack[0].window.set_input_focus(X.RevertToParent, X.CurrentTime, 
                                                  onerror = lambda *unused: self.client_stack.pop(0))

  def set_graphics(self, **kwds):
    """ Set graphics theme options.

    Args:
      kwds:
        frame_border
        frame_border_color
        title_border
        title_border_color
        frame_bg
        title_fg
        title_bg
        title_font
    """
    # Use gc.change() to modify the GC values for each kwd argument passed in.
    tmpgc = self.window.create_gc(foreground=self.pixel_bg.pixel, line_width=1, 
                                  fill_style=X.FillSolid, line_style=X.LineSolid)
    print "tmpgc; %r" % tmpgc
    print dir(tmpgc)

  def paint(self, x=0, y=0, width=None, height=None): 
    if width is None:
      width = self.width
    if height is None:
      height = self.height
    print "paint(%d, %d, %d, %d)" % (x, y, width, height)

    # Frame background
    self.window.fill_rectangle(self.gc_frame_bg, x, y, width, height)

    # Title
    self.window.fill_rectangle(self.gc_title_bg, 0, 0,self.width, self.title_height)
    self.window.poly_rectangle(self.gc_title_border, [(0, 0, self.width, self.title_height)])
    self.window.draw_text(self.gc_title_font, 10, 10, "Hello there")

  def resize(self, width=0, height=0):
    if width:
      self.width = self.trim_border(width)
    if height:
      self.height = self.trim_border(height)

    self.calculate_client_area()
    self.window.configure(width=self.width, height=self.height)
    for client in self.children:
      client.window.configure(width=self.client_width, height=self.client_height)
    
  def add_client(self, client):
    if client not in self.children:
      self.children[client] = 1
    client.window.unmap()
    client.window.change_save_set(X.SetModeInsert)
    client.window.reparent(self.window, self.client_x, self.client_y)
    client.window.configure(width=self.client_width,
                           height=self.client_height,
                           border_width=0)
    client.window.map()
    client.window.set_input_focus(X.RevertToParent, X.CurrentTime)
    mask = X.EnterWindowMask | X.PropertyChangeMask
    client.window.change_attributes(event_mask=mask)
    if client in self.client_stack:
      self.client_stack.remove(client)
    self.client_stack.insert(0, client)

  def remove_client(self, client):
    if client in self.children:
      del self.children[client]
      if client in self.client_stack:
        self.client_stack.remove(client)
    if self.client_stack:
      client.map()
      client.set_input_focus(X.RevertToParent, X.CurrentTime)

class Client(object):
  def __init__(self, wm, window, screen):
    self.window = window
    self.screen = screen
    self.wm = wm
    self.title = None

class WindowManager(object):
  maskmap_index = (X.ShiftMask, X.LockMask, X.ControlMask, X.Mod1Mask,
                   X.Mod2Mask, X.Mod3Mask, X.Mod4Mask, X.Mod5Mask)

  def __init__(self, display_name=None):
    self.display = display.Display(display_name)
    self.screens = {}
    self.containers = {}
    self.clients = {}
    self.client_container = {}
    self.keybindings = {}
    self.container_stack = []
    self.window_container_map = {}
    self.modifier_map = {}
    self.init_screens()
    self.init_keyboard()

  def init_keyboard(self):
    # Ensure we're called after init_screens
    assert self.screens

    m = self.display.get_modifier_mapping()
    for (marray, modmask) in zip(m, WindowManager.maskmap_index) :
      for keycode in marray:
        if keycode == 0:
          continue
        keysym = self.display.keycode_to_keysym(keycode, 0)
        self.modifier_map[keycode] = modmask

  def init_screens(self):
    for num in range(0, self.display.screen_count()):
      screen = self.display.screen(num)
      self.screens[num] = screen
      self.init_root(screen.root, screen)

  def init_root(self, root_win, screen):
    mask = (X.SubstructureNotifyMask | X.SubstructureRedirectMask 
            | X.KeyPressMask | X.KeyReleaseMask)
    root_win.change_attributes(event_mask = mask)
    self.add_container(parent=root_win)

    for window in root_win.query_tree().children:
      if window in self.containers:
        continue
      self.add_client(window, screen)

  def add_client(self, window, screen):
    client = Client(self, window, screen)
    self.clients[window] = client
    self.container_stack[0].add_client(client)

  def grab_key(self, window, keysym, modifier):
    keycode = self.display.keysym_to_keycode(keysym)
    if keycode == X.NoSymbol:
      print "No keycode found for keysym: %r" % keycode 
    ret = window.grab_key(keycode, modifier, 0, False, X.GrabModeAsync, X.GrabModeAsync)
    #print "Grab key(%r, %r, %r): %r" % (window, keysym, modifier, ret)

  def string_to_grab_key(self, key_string):
    data = key_string.split("+")
    # XXX: Implement me.
    
  def add_container(self, parent, **kwds):
    """ Add a new container.

    Args:
      parent: a Window
      kwds:
        x: x coordinate in pixels
        y: y coordinate in pixels
        width: width in pixels
        height: height in pixels
    """
    parent_geom = parent.get_geometry()
    x = kwds.get("x", 0)
    y = kwds.get("y", 0)
    width = kwds.get("width", parent_geom.width)
    height = kwds.get("height", parent_geom.height)
    container = Container(self, parent, x, y, width, height)
    self.containers[container.window] = container
    self.container_stack.insert(0, container)
    container.set_graphics()
    container.paint()
    
  def run_forever(self):
    dispatch = {
      X.ConfigureRequest: self.handle_configure_request,
      X.MapRequest: self.handle_map_request,
      X.MapNotify: self.handle_map_notify,
      X.DestroyNotify: self.handle_destroy_notify,
      X.UnmapNotify: self.handle_unmap_notify,
      X.KeyPress: self.handle_key_press,
      X.KeyRelease: self.handle_key_release,
      X.EnterNotify: self.handle_enter_notify,
      X.Expose: self.handle_expose,
      X.PropertyNotify: self.handle_property_notify,

      X.ReparentNotify: lambda ev: True, # Ignore
      X.LeaveNotify: lambda ev: True, # Ignore
      #X.ConfigureNotify: lambda ev: True, # Ignore
      X.CreateNotify: lambda ev: True, # Ignore
    }

    while True:
      ev = self.display.next_event()
      if ev.type in dispatch:
        dispatch[ev.type](ev)
      else:
        pass
        #print "Unhandled event: %r" % ev

  def handle_configure_request(self, ev):
    print "config request: %r" % ev
    data = {}
    for i in ("value_mask", "x", "y", "width", "height"):
      data[i] = getattr(ev, i)
    data["border_width"] = 0
    ev.window.configure(**data)

  def handle_map_request(self, ev):
    print "Map request: %r" % ev.window
    if ev.window in self.client_container:
      self.client_container[ev.window].remove_client(ev.window)
      del self.client_container[ev.window]
    #print "Mapreq: %r" % ev.window.get_attributes()
    #print ev.window.get_attributes()
    #client = self.clients.get(wv.window, Client(???)
    screen = self.get_screen_of_window(ev.window)
    self.add_client(ev.window, screen)
    #self.client_container[ev.window] = self.container_stack[0]
    #self.container_stack[0].add_client(ev.window)

  def handle_map_notify(self, ev):
    #print "Map notify: %r" % ev.window
    pass

  def handle_unmap_notify(self, ev):
    self.handle_destroy_notify(ev)

  def handle_destroy_notify(self, ev):
    #print "Destroy notify on %r" % ev.window
    if ev.window in self.clients:
      del self.clients[ev.window]
      if ev.window in self.client_container:
        self.client_container[ev.window].remove_client(ev.window)
        del self.client_container[ev.window]
    else:
      print "DestroyNotify on window I don't know about: %r" % ev.window

  def handle_key_press(self, ev):
    keysym = self.display.keycode_to_keysym(ev.detail, 0)
    is_container = ev.window in self.containers

    context = WMActions.root_context
    if is_container:
      data = (WMActions.container_context, keysym, ev.state)
      if data in self.keybindings:
        context = WMActions.container_context
    else:
      context = WMActions.root_context
    data = (context, keysym, ev.state)

    method = self.keybindings.get(data, None)
    if method:
      print "Calling method: %r" % method
      method()
    else:
      pass
      #print "No callback for: %r" % (data,)
      #print ev

  def handle_key_release(self, ev):
    pass

  def handle_enter_notify(self, ev):
    if ev.window not in self.containers:
      return
    self.focus_container(self.containers[ev.window])

  def handle_expose(self, ev):
    if ev.window not in self.containers:
      return
    self.containers[ev.window].paint(x=ev.x, y=ev.y, width=ev.width, height=ev.height)

  def handle_property_notify(self, ev):
    pass
    #print ev

  def get_screen_of_window(self, window):
    geom = window.get_geometry()
    for screen in self.screens.iteritems():
      print dir(screen)
      #if screen.root == geom.root:
        #return screen

  def focus_container(self, container):
    self.container_stack.remove(container)
    self.container_stack.insert(0,container)
    container.focus()

  def register_keystroke_handler(self, keystring, method, context=WMActions.root_context):
    """ Register a callback for a keystroke
    Args:
      keystring: string, format is 'mod1+mod2+modN+key'
         such as 'alt+j'
      method: method to be called
    """
    data = keystring.split("+")
    mod_list = data[:-1]
    key = data[-1]
    modifier_mask = 0
    #keysym = getattr(latin1, "XK_%s" % key, None)
    keysym = XK.string_to_keysym(key)
    #print "%s => %s" % (key, keysym)
    if keysym == X.NoSymbol:
      print "%r is not a valid keysym" % key
      return
    keycode = self.display.keysym_to_keycode(keysym)
    for mod in mod_list:
      # Rename "alt" to "Alt_L" and such.
      if mod in ("alt", "super", "meta"):
        mod = mod.capitalize() + "_L"
      modkey = self.display.keysym_to_keycode(XK.string_to_keysym(mod))
      if modkey in self.modifier_map:
        modifier_mask |= self.modifier_map[modkey]
      else:
        print "Invalid modifier key: '%s' (check xmodmap output)" % mod

    if context == WMActions.root_context:
      for screen in self.screens.itervalues():
        self.grab_key(screen.root, keysym, modifier_mask)
    keybinding = (context, keysym, modifier_mask)
    print "Registered keystroke '%s' as %r)" % (keystring, keybinding,)
    self.keybindings[keybinding] = method
    #self.keybindings.setdefault(context, {})
    #self.keybindings[context][keybinding] = method


def main(args):
  wm = WindowManager()
  wm.register_keystroke_handler("alt+j", 
                                lambda : WMActions.split_frame(WMActions, wm, horizontal=True))
  wm.register_keystroke_handler("alt+h", 
                                lambda : WMActions.split_frame(WMActions, wm, vertical=True))
  wm.register_keystroke_handler("Return", 
                                lambda : WMActions.runcmd(WMActions, "run-xterm"),
                                context = WMActions.container_context)
  wm.run_forever()

if __name__ == "__main__":
  prog = sys.argv[0]
  main(sys.argv[1:])
