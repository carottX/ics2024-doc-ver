#include <nwm.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define DECL(n, h) {n, sizeof(n) - 1, &WindowManager::evt_##h},

static struct EventHandler {
  const char *pattern;
  int length;
  void (WindowManager::*handler)(const char *evt);
} handlers[] = {
  FOREACH_EVENT(DECL)
};

/**
 * @brief Handles an event by iterating through the registered event handlers.
 *
 * This method processes the given event by comparing it against the patterns
 * of all registered handlers. If the event matches a handler's pattern, the
 * corresponding handler function is invoked with the event data (excluding the
 * matched pattern). The method stops processing after the first matching handler
 * is found and invoked.
 *
 * @param evt A pointer to the event string to be handled. The event string is
 *            expected to contain a pattern followed by optional data.
 *
 * @details The method performs a comparison between the event and each handler's
 *          pattern using `strncmp`. If a match is found, the handler function
 *          is called with the remaining part of the event string (after the
 *          matched pattern). The loop terminates after the first match.
 */
void WindowManager::handle_event(const char *evt) {
  for (auto &h: handlers) {
    if (strncmp(evt, h.pattern, h.length) == 0) {
      (this->*h.handler)(evt + h.length);
      break;
    }
  }
}

// ---------- EVENT HANDLERS ----------

void WindowManager::evt_timer(const char *evt) {
  for (Window *w: windows) {
    if (w) w->update();
  }
  render();
}

static int ctrl = 0, alt = 0, shift = 0;

/**
 * @brief Sends a key event to the application associated with the given window.
 *
 * This method constructs a message representing a key event (either a key press or release)
 * and writes it to the write file descriptor of the specified window. The message format
 * is "k[d|u] [key]\n", where 'd' indicates a key down event, 'u' indicates a key up event,
 * and [key] is the string representation of the key.
 *
 * @param win Pointer to the Window object representing the application window.
 * @param key The string representation of the key being sent.
 * @param down A boolean flag indicating whether the key is pressed (true) or released (false).
 *
 * @pre The write file descriptor of the window (`win->write_fd`) must be valid and open.
 * @post The key event message is written to the window's write file descriptor.
 */
static void send_key_to_apps(Window *win, const char *key, int down) {
  assert(win->write_fd);
  char buf[64];
  sprintf(buf, "k%c %s\n", (down ? 'd' : 'u'), key);
  write(win->write_fd, buf, strlen(buf));
}

/**
 * @brief Handles keydown events and updates the state of modifier keys (ALT, CTRL, SHIFT).
 * 
 * This method processes the keydown event string to determine which key was pressed. It updates
 * the state of modifier keys (ALT, CTRL, SHIFT) based on the event. If a modifier key is active,
 * it constructs a combined event string and passes it to the `handle_event` method. If the
 * `display_appfinder` flag is set, it handles navigation and selection within the appfinder
 * interface. Otherwise, it sends the key event to the currently focused application.
 * 
 * @param evt A string representing the keydown event, containing the key that was pressed.
 */
void WindowManager::evt_keydown(const char *evt) {
  char key[20];
  sscanf(evt, "%s", key);
  if (strcmp(key, "LALT") == 0) { alt = 1; return; }
  if (strcmp(key, "LCTRL") == 0) { ctrl = 1; return; }
  if (strcmp(key, "LSHIFT") == 0 || strcmp(key, "RSHIFT") == 0) { shift ^= 1; return; }

  if (ctrl || alt) {
    char event[20];
    sprintf(event, "%s%s%s", ctrl ? "C-" : "", alt ? "A-" : "", key);
    handle_event(event);
  } else {
    if (display_appfinder) {
      if (strcmp(key, "LEFT") == 0) appfinder->prev();
      else if (strcmp(key, "TAB") == 0) appfinder->next();
      else if (strcmp(key, "RIGHT") == 0) appfinder->next();
      else if (strcmp(key, "RETURN") == 0) {
        spawn(appfinder->getcmd(), (const char **)appfinder->getargv()); // fall through
        display_appfinder = false;
        appfinder->draw();
      }
      else if (strcmp(key, "ESCAPE") == 0) {
        display_appfinder = false;
        appfinder->draw();
      }
    } else if (focus) {
      send_key_to_apps(focus, key, true);
    }
  }
}

/**
 * @brief Handles the key-up event for the window manager.
 * 
 * This method processes the key-up event by extracting the key from the event string
 * and updating the internal state of the window manager accordingly. It checks for
 * specific modifier keys (LALT, LCTRL, LSHIFT, RSHIFT) and updates their state. If
 * the ALT key is released and the display switcher is active, it triggers the switcher
 * to redraw and then hides it. If the focus is on an application and neither the app
 * finder nor the ALT key is active, the key-up event is forwarded to the focused application.
 * 
 * @param evt The event string containing the key that was released.
 */
void WindowManager::evt_keyup(const char *evt) {
  char key[20];
  sscanf(evt, "%s", key);
  if (strcmp(key, "LALT") == 0) {
    alt = 0;
    if (display_switcher) switcher->draw();
    display_switcher = false;
    return;
  }
  if (strcmp(key, "LCTRL") == 0) { ctrl = 0; return; }
  if (strcmp(key, "LSHIFT") == 0 || strcmp(key, "RSHIFT") == 0) { shift ^= 1; return; }

  if (focus && !display_appfinder && !alt) {
    send_key_to_apps(focus, key, false);
  }
}

/**
 * @brief Handles the event to switch the focus to the next available window.
 * 
 * This method is triggered by an event to switch the focus to the next window in the window manager's list.
 * If no window is currently focused, the method exits early. The method ensures that the display switcher is
 * enabled and, if the app finder is currently displayed, it hides the app finder and redraws it.
 * The method then iterates through the list of windows, starting from the currently focused window, and
 * selects the next available window to set as the new focus. If the end of the list is reached, it wraps
 * around to the beginning. The method ensures that only valid (non-null) windows are considered for focus.
 * 
 * @param evt The event that triggered the window switch. This parameter is currently unused in the method.
 */
void WindowManager::evt_switch_window(const char *evt) {
  int i = 0;
  if (!focus) return;
  display_switcher = true;
  if (display_appfinder) {
    display_appfinder = false;
    appfinder->draw();
  }
  while (windows[i] != focus) i ++;
  while (1) {
    i ++;
    if (i >= 16) i = 0;
    if (windows[i]) break;
  }
  set_focus(windows[i]);
}

/**
 * @brief Moves the currently focused window upwards by 10 units.
 * 
 * This method checks if there is a currently focused window and ensures that it is not the appfinder window.
 * If both conditions are met, the method moves the focused window upwards by 10 units on the y-axis.
 * 
 * @param evt A pointer to the event that triggered this method. This parameter is not used within the method.
 */
void WindowManager::evt_moveup_window(const char *evt) {
  if (focus && focus != appfinder) {
    focus->move(focus->x, focus->y - 10);
  }
}

/**
 * @brief Moves the currently focused window down by 10 units.
 * 
 * This method is triggered by an event and checks if there is a currently focused window
 * and that the focused window is not the application finder. If both conditions are met,
 * the method moves the focused window down by 10 units along the y-axis while keeping
 * the x-coordinate unchanged.
 * 
 * @param evt The event that triggered this method. This parameter is currently unused.
 */
void WindowManager::evt_movedown_window(const char *evt) {
  if (focus && focus != appfinder) {
    focus->move(focus->x, focus->y + 10);
  }
}

/**
 * @brief Moves the currently focused window to the left by 10 pixels.
 *
 * This method checks if there is a currently focused window and ensures it is not
 * the application finder window. If these conditions are met, the method moves
 * the focused window to the left by decrementing its x-coordinate by 10 pixels,
 * while keeping the y-coordinate unchanged.
 *
 * @param evt A pointer to the event that triggered this method. This parameter
 *            is currently unused in the method implementation.
 */
void WindowManager::evt_moveleft_window(const char *evt) {
  if (focus && focus != appfinder) {
    focus->move(focus->x - 10, focus->y);
  }
}

/**
 * @brief Moves the currently focused window to the right by 10 pixels.
 * 
 * This method checks if there is a currently focused window and ensures that it is not the appfinder window.
 * If both conditions are met, the method moves the focused window to the right by 10 pixels by updating its x-coordinate.
 * 
 * @param evt A pointer to the event that triggered this method. This parameter is not used in the method's implementation.
 */
void WindowManager::evt_moveright_window(const char *evt) {
  if (focus && focus != appfinder) {
    focus->move(focus->x + 10, focus->y);
  }
}

/**
 * @brief Handles the application finder event.
 *
 * This method is triggered when an application finder event occurs. It checks if the display switcher is not active.
 * If the display switcher is inactive, it sets the `display_appfinder` flag to true and calls the `draw()` method 
 * of the `appfinder` object to render the application finder interface.
 *
 * @param evt The event string that triggered this method. This parameter is currently unused in the implementation.
 */
void WindowManager::evt_appfinder(const char *evt) {
  if (!display_switcher) {
    display_appfinder = true;
    appfinder->draw();
  }
}
