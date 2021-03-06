#!/usr/bin/env python

import os
import sys
import time

from Xlib import X, display, Xutil, XK, Xatom
import Xlib.error as xerror
from Xlib.protocol import event as xevent

dpy = display.Display()
prog = ""

class WMActions(object):
  root_context = 1
  container_context = 2

  @staticmethod
  def runcmd(cmd):
    print "Running %s" % cmd
    if os.fork() == 0:
      os.setsid()
      if os.fork() == 0:
        os.execlp("/bin/sh", "/bin/sh", "-c", cmd)
      else:
        exit(0)
  
  @staticmethod
  def split_frame( wm, horizontal=False, vertical=False):
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

class TitleWindow(object):
  def __init__(self, wm, parent):
    mask = (X.ButtonPressMask | X.ButtonReleaseMask 
            | X.EnterWindowMask | X.LeaveWindowMask
            | X.KeyPressMask | X.KeyReleaseMask
            | X.ExposureMask)
    self.wm = wm
    self.is_active = False
    self.parent = parent

    screen = wm.get_screen_of_window(parent)

    self.text = "<new frame>"

    self.width = 1
    self.height = self.wm.title_font_extents.font_ascent
    self.window = parent.create_window(-100, -100, self.width, self.height, 0, 
                                       X.CopyFromParent, X.InputOutput,
                                       event_mask=mask)
    self.window.map()
    self.paint()

  def resize(self, width=None, height=None):
    if width:
      self.width = width
    if height:
      self.height = height
    if width or height:
      self.window.configure(width=self.width, height=self.height)
    
  def paint(self, x=0, y=0, width=None, height=None): 
    if self.is_active:
      gc_bg = self.wm.gc_title_active_bg
      gc_fg = self.wm.gc_title_active_fg
    else:
      gc_bg = self.wm.gc_title_inactive_bg
      gc_fg = self.wm.gc_title_inactive_fg

    self.window.fill_rectangle(gc_bg,  0, 0,self.width, self.height)
    self.window.poly_rectangle(self.wm.gc_title_border, [(0, 0, self.width - 1, self.height - 1)])

    text_extents = self.wm.title_font.query_text_extents(self.text)
    text_width = text_extents.overall_width
    width_per_char = text_width / len(self.text)

    text = self.text
    if text_width > self.width:
      num_chars = int(self.width / width_per_char) - 6
      text_width = int(num_chars * width_per_char)
      text = self.text[:num_chars] + "..."

    xpos = int((self.width - text_width) / 2)
    self.window.draw_text(gc_fg,
                          xpos, 1 + self.wm.title_font_extents.font_ascent, 
                          text) 

  def set_text(self, text):
    self.text = text

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
    self.title_windows = {}
    self.wm = wm
    self.parent = parent
    self.title_height = wm.title_font_extents.font_ascent + 4
    self.border_width = 0
    self.x = x
    self.y = y
    self.width = width
    self.height = height

    self.calculate_client_area()

    self.client_list = []
    self.current_client = None

    mask = (X.ButtonPressMask | X.ButtonReleaseMask 
            | X.EnterWindowMask | X.LeaveWindowMask
            | X.KeyPressMask | X.KeyReleaseMask
            | X.ExposureMask)

    self.colormap = parent.get_attributes().colormap

    self.pixel_border = self.colormap.alloc_named_color("#FFFF00")
    self.pixel_bg = self.colormap.alloc_color(0,0,0)

    self.window = parent.create_window(x, y, self.width, self.height, 
                                       self.border_width,
                                       X.CopyFromParent, X.InputOutput,
                                       event_mask=mask, 
                                       border_pixel=self.pixel_border.pixel)

    self.gc_frame_bg = self.window.create_gc()
    self.gc_frame_border= self.window.create_gc()
    self.gc_border = self.window.create_gc()
    self.window.map()
    self.paint()

  def trim_border(self, pixels):
    return pixels # - (2 * self.border_width)

  def calculate_client_area(self):
    self.title_x = self.border_width
    self.title_y = self.border_width
    self.title_width = self.width
    self.title_height = self.title_height
    self.client_x = self.border_width 
    self.client_y = self.title_height
    self.client_width = self.width - (2 * self.border_width)
    self.client_height = self.height - (2 * self.border_width) - self.title_height

  def focus(self):
    self.window.set_input_focus(X.RevertToParent, X.CurrentTime)
    #print "Focused new container"
    if self.client_list:
      #print "Focusing window in stack: %r" % self.client_list[0]

      # The onerror here is to remove a window when it's gone bad. Not sure why
      # it happens, perhaps a race.
      client = self.current_client
      error_func = lambda *unused: self.remove_client(client)
      client.window.map(onerror = error_func)
      client.window.set_input_focus(X.RevertToParent, 
                                    X.CurrentTime, onerror = error_func)
      client.title_window.paint()

  def set_current_client(self, client):
    if client not in self.client_list:
      print "Attempt to currentify client %r which is not in this container %r" % (client, self)
      return

    if self.current_client:
      if self.current_client == client:
        return
      self.current_client.window.unmap()
      self.current_client.title_window.is_active = False
      self.current_client.title_window.paint()
    self.current_client = client
    client.title_window.is_active = True
    self.focus()

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
    #print "paint(%d, %d, %d, %d)" % (x, y, width, height)

    # Frame background
    self.window.fill_rectangle(self.gc_frame_bg, x, y, width, height)

  def resize(self, width=0, height=0):
    if width:
      self.width = self.trim_border(width)
    if height:
      self.height = self.trim_border(height)

    self.calculate_client_area()
    self.window.configure(width=self.width, height=self.height)
    for client in self.children:
      client.window.configure(width=self.client_width, height=self.client_height)
    self.reposition_titles()
      
  def add_client(self, client):
    if client not in self.children:
      self.children[client] = 1

    #client_event_mask = X.DestroyNotify | X.UnmapNotify
    client.window.unmap()
    client.window.change_save_set(X.SetModeInsert)
    client.window.reparent(self.window, self.client_x, self.client_y)
    client.window.configure(width=self.client_width,
                           height=self.client_height,
                           border_width=0)
    mask= (X.StructureNotifyMask | X.EnterWindowMask 
           | X.PropertyChangeMask)
    client.window.change_attributes(event_mask=mask)
    if client in self.client_list:
      self.client_list.remove(client)
    self.client_list.append(client)
    self.set_current_client(client)
    self.reposition_titles()
    
  def remove_client(self, client):
    self.wm.display.flush()
    if client in self.children:
      del self.children[client]
      if client in self.client_list:
        self.client_list.remove(client)
    if self.client_list:
      self.set_current_client(self.client_list[0])
    self.reposition_titles()

  def reposition_titles(self):
    if not self.client_list:
      return

    title_width = self.width / len(self.client_list)
    for (index, client) in enumerate(self.client_list):
      tw = client.title_window
      xpos = index * title_width
      print "Positioning client %d in %r at %d" % (index, self, xpos)
      tw.window.unmap()
      tw.window.reparent(self.window, xpos, 0)
      tw.window.map()
      tw.resize(width=title_width, height=self.title_height)
      tw.is_active = (client == self.current_client)
      tw.paint()
      

class Client(object):
  def __init__(self, wm, window, screen):
    self.window = window
    self.screen = screen
    self.wm = wm
    self.title = None
    self.title_window = TitleWindow(wm, screen.root)
    self.refresh_title()

  def refresh_title(self):
    data = self.window.get_full_property(Xatom.WM_NAME, Xatom.STRING)
    if data:
      self.title = data.value
      self.title_window.set_text(self.title)
      self.title_window.paint()

class WindowManager(object):
  maskmap_index = (X.ShiftMask, X.LockMask, X.ControlMask, X.Mod1Mask,
                   X.Mod2Mask, X.Mod3Mask, X.Mod4Mask, X.Mod5Mask)

  def __init__(self, display_name=None):
    self.display = display.Display(display_name)
    self.screens = {}
    self.containers = {}
    self.clients = {}
    self.events = {}
    self.client_container = {}
    self.keybindings = {}
    self.container_stack = []
    self.window_container_map = {}
    self.modifier_map = {}

    self.init_screens()
    self.init_keyboard()
    self.init_graphics()
    for screen in self.screens.itervalues():
      self.init_root(screen.root, screen)

    self.property_dispatch = {
      self.display.intern_atom("WM_NAME"): self.update_window_title,

      # Ignore these
      self.display.intern_atom("WM_ICON_NAME"): lambda ev: True,
    }

  def init_graphics(self):
    screen = self.screens[0]
    white = screen.default_colormap.alloc_named_color("white")
    black = screen.default_colormap.alloc_named_color("black")
    active = screen.default_colormap.alloc_named_color("#7F7F44")
    root_win = self.screens[0].root
    self.gc_title_border = root_win.create_gc(foreground=white.pixel)

    self.gc_title_active_fg = root_win.create_gc(foreground=white.pixel)
    self.gc_title_active_bg = root_win.create_gc(foreground=active.pixel)
    self.gc_title_inactive_fg = root_win.create_gc(foreground=white.pixel)
    self.gc_title_inactive_bg = root_win.create_gc(foreground=black.pixel)
    self.title_font = self.display.open_font("fixed")
    self.title_font_extents = self.title_font.query_text_extents("M")
    self.gc_title_font = root_win.create_gc(font=self.title_font, 
                                            foreground=white.pixel)

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

  def init_root(self, root_win, screen):
    mask = (X.SubstructureNotifyMask | X.SubstructureRedirectMask 
            | X.KeyPressMask | X.KeyReleaseMask)
    root_win.change_attributes(event_mask = mask)
    self.add_container(parent=root_win)

    for window in root_win.query_tree().children:
      if window in self.containers:
        continue
      self.add_client(window)

  def add_event(self, event, window, callback, conditional = lambda : True):
    key = (event, window)
    self.events.setdefault(key, [])
    self.events[key].append((conditional, callback))

  def add_client(self, window):
    print "Add client: %r" % window
    screen = self.get_screen_of_window(window)
    client = Client(self, window, screen)
    self.clients[window] = client
    self.container_stack[0].add_client(client)
    self.client_container[client] = self.container_stack[0]

    self.add_event(X.ButtonRelease, client.title_window.window,
                   lambda ev: self.focus_client(client),
                   conditional = lambda ev: ev.detail == 1)

  def focus_client(self, client):
    if client not in self.client_container:
      print "Attempt to focus client not in a container?"
      return

    container = self.client_container[client]
    container.set_current_client(client)

  def remove_client(self, client):
    window = client.window
    if window in self.clients:
      del self.clients[window]
    if client in self.client_container:
      self.client_container[client].remove_client(client)
      del self.client_container[client]

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
      X.ButtonPress: self.handle_button_press,
      X.ButtonRelease: self.handle_button_release,
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
        print "Unhandled event: %r" % ev

  def handle_configure_request(self, ev):
    print "config request: %r" % ev
    data = {}
    for i in ("value_mask", "x", "y", "width", "height"):
      data[i] = getattr(ev, i)
    data["border_width"] = 0
    ev.window.configure(**data)

  def handle_map_request(self, ev):
    print "Map request: %r" % ev.window
    #if ev.window in self.client_container:
      #self.client_container[ev.window].remove_client(ev.window)
      #del self.client_container[ev.window]
    screen = self.get_screen_of_window(ev.window)
    self.add_client(ev.window)

  def handle_map_notify(self, ev):
    pass

  def handle_unmap_notify(self, ev):
    pass

  def handle_destroy_notify(self, ev):
    print "Destroy notify on %r" % ev.window
    if ev.window in self.clients:
      self.remove_client(self.clients[ev.window])
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

  def handle_button_press(self, ev):
    #print ev
    pass

  def handle_button_release(self, ev):
    print ev
    self.process_event(ev)

  def process_event(self, ev):
    key = (ev.type, ev.window)
    handlers = self.events.get(key, [])
    for (condition, callback) in handlers:
      if condition(ev):
        callback(ev)

  def handle_enter_notify(self, ev):
    if ev.window not in self.containers:
      return
    self.focus_container(self.containers[ev.window])

  def handle_expose(self, ev):
    if ev.window not in self.containers:
      return
    self.containers[ev.window].paint(x=ev.x, y=ev.y, width=ev.width, height=ev.height)

  def handle_property_notify(self, ev):
    if ev.atom in self.property_dispatch:
      self.property_dispatch[ev.atom](ev)
    else:
      print "Unhandled property notify atom %s" % (self.display.get_atom_name(ev.atom))

  def get_screen_of_window(self, window):
    geom = window.get_geometry()
    for screen in self.screens.itervalues():
      if screen.root == geom.root:
        return screen

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

  def update_window_title(self, ev):
    if ev.window not in self.clients:
      return
    print "update: %r" % self

    client = self.clients[ev.window]
    try:
      client.refresh_title()
      container = self.client_container[client]
      container.paint()
    except xerror.BadWindow:
      self.remove_client(client)

def main(args):
  wm = WindowManager()
  wm.register_keystroke_handler("alt+j", 
        lambda : WMActions.split_frame(wm, horizontal=True))
  wm.register_keystroke_handler("alt+h", 
        lambda : WMActions.split_frame(wm, vertical=True))
  wm.register_keystroke_handler("Return", 
        lambda : WMActions.runcmd("run-xterm"),
        context = WMActions.container_context)
  wm.run_forever()

if __name__ == "__main__":
  prog = sys.argv[0]
  main(sys.argv[1:])
