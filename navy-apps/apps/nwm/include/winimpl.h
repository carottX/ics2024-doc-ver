#ifndef __WINIMPL_H__
#define __WINIMPL_H__

#include <nwm.h>
#include <string.h>

static const char *nterm_argv[] = { "/bin/nterm", NULL };
static const char *nslider_argv[] = { "/bin/nslider", NULL };
static const char *fceux_argv[] = { "/bin/fceux", NULL };
static const char *pal_argv[] = { "/bin/pal", NULL };
static const char *nplayer_argv[] = { "/bin/nplayer", NULL };
static const char *typing_argv[] = { "/bin/typing-game", NULL };
static const char *onscripter_argv[] = { "/bin/onscripter", "-r", "/share/games/onscripter/clannad", NULL };

static const struct {
  const char *name;
  const char *bin;
  const char *const *argv;
} default_apps [] = {
  { "Terminal", nterm_argv[0], nterm_argv },
  { "NSlider", nslider_argv[0], nslider_argv },
  { "Typing Game", typing_argv[0], typing_argv },
  { "FCEUX", fceux_argv[0], fceux_argv },
  { "NPlayer", nplayer_argv[0], nplayer_argv },
  { "Pal", pal_argv[0], pal_argv },
  { "ONScripter", onscripter_argv[0], onscripter_argv },
};

/**
 * @brief Constructs a BgImage object, which is a specialized Window with a gradient background.
 *
 * The BgImage constructor initializes a Window with the specified width and height, 
 * and then fills the window with a gradient color pattern. The gradient is calculated 
 * based on the pixel's position (x, y) within the window. The red, green, and blue 
 * components of the color are determined by quadratic functions of the pixel's 
 * coordinates, creating a smooth transition of colors across the window.
 *
 * @param wm Pointer to the WindowManager that manages this window.
 * @param width The width of the window in pixels.
 * @param height The height of the window in pixels.
 */
class BgImage: public Window {
public:
  BgImage(WindowManager *wm, int width, int height): Window(wm, nullptr, nullptr, nullptr) {
    move(0, 0);
    resize(width, height);

    for (int x = 0; x < w; x ++)
      for (int y = 0; y < h; y ++) {
        uint8_t r = 255 - x * x * 256 / w / w;
        uint8_t g = 255 - y * y * 256 / h / h;
        uint8_t b = 255 - (x * x + y * y) * 256 / (w + h) / (w + h);

        uint32_t col = (r << 16) | (g << 8) | b;
        draw_px(x, y, col);
      }
  }
};;

/**
 * @brief Projects and renders a given window onto the current window's canvas.
 *
 * This method takes a window and projects it onto the current window's canvas at the specified
 * base coordinates (basex, basey) with the given width (w) and height (h). The projection
 * maintains the aspect ratio of the original window. The method calculates the scaled dimensions
 * (w1, h1) based on the aspect ratio of the input window and the target dimensions. It then
 * iterates over each pixel in the scaled dimensions, samples the corresponding pixel from the
 * input window's canvas, and draws it onto the current window's canvas at the appropriate position.
 * The drawn pixels are masked to 24-bit color (RGB) by ignoring the alpha channel.
 *
 * @param win Pointer to the window to be projected.
 * @param basex The x-coordinate of the base position for the projection.
 * @param basey The y-coordinate of the base position for the projection.
 * @param w The target width for the projection.
 * @param h The target height for the projection.
 */
void project(Window *win, int basex, int basey, int w, int h) {
  int w1, h1;
  if (win->h * w <= h * win->w) {
    w1 = w;
    h1 = win->h * w / win->w;
  } else {
    w1 = win->w * h / win->h;
    h1 = h;
  }
  for (int x = 0; x < w1; x ++)
    for (int y = 0; y < h1; y ++) {
      uint32_t col = win->canvas[
          ( y * win->h / h1 ) * win->w + 
          ( x * win->w / w1 )
        ];
      col &= 0xffffff;
      //if (col & 0xff000000) {
      //  col = border_col;
      //}
      draw_px(basex + x + (w - w1) / 2, basey + y + (h - h1) / 2, col);
    }
};

/**
 * @brief Moves the selection cursor to the previous item in the AppFinder window.
 *
 * This method decrements the current selection index (`cur`) in a circular manner,
 * ensuring it wraps around to the last item if it goes below 0. After updating the
 * selection, it calls the `sync()` method to redraw the window and reflect the new
 * selection state.
 */
void prev() {
    cur = (cur - 1 + n) % n;
    sync();
};

#endif
