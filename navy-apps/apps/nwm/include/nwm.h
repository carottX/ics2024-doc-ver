#ifndef __NWM_H__
#define __NWM_H__

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <BDF.h>

class Window;
class WindowManager;

#define FOREACH_EVENT(_) \
  _("t", timer) \
  _("ku", keyup) \
  _("kd", keydown) \
  _("A-GRAVE", switch_window) \
  _("A-UP", moveup_window) \
  _("A-DOWN", movedown_window) \
  _("A-LEFT", moveleft_window) \
  _("A-RIGHT", moveright_window) \
  _("A-R", appfinder) \

/**
 * @brief Draws a single pixel on the window's canvas at the specified coordinates with the given color.
 * 
 * This method sets the color of the pixel located at (x, y) on the window's canvas to the specified color.
 * The coordinates are relative to the window's internal canvas, and the color is represented as a 32-bit
 * unsigned integer in the format 0xAARRGGBB (alpha, red, green, blue). If the coordinates are outside the
 * bounds of the canvas, the method does nothing.
 * 
 * @param x The x-coordinate of the pixel to draw, relative to the window's canvas.
 * @param y The y-coordinate of the pixel to draw, relative to the window's canvas.
 * @param color The color to set the pixel to, represented as a 32-bit unsigned integer in 0xAARRGGBB format.
 */
void Window::draw_px(int x, int y, uint32_t color) {
  if (x >= 0 && x < w && y >= 0 && y < h) {
    canvas[y * w + x] = color;
  }
};

class BgImage;
class WindowSwitcher;
class AppFinder;

/**
 * @brief Handles an event by processing the event string and updating the window manager's state accordingly.
 *
 * This method is responsible for interpreting the event string and performing the necessary actions
 * based on the event type. It may involve updating the focus window, redrawing windows, or triggering
 * other window manager functionalities. The event string is expected to be in a specific format that
 * the window manager can parse and act upon.
 *
 * @param evt A string representing the event to be handled. The format of this string is dependent
 *            on the event type and should be compatible with the window manager's event handling logic.
 */
void WindowManager::handle_event(const char *evt) {
    // Implementation of the event handling logic
};

#include <winimpl.h>

#endif
