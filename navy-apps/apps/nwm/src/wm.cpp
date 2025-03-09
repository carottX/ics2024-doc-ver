#include <nwm.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <NDL.h>

const int tile_shift = 4;

/**
 * @brief Constructs a WindowManager object with the specified width and height.
 *
 * This constructor initializes the WindowManager with the given dimensions. It calculates
 * the number of tiles required based on the width and height, taking into account the tile
 * shift value. It allocates memory for the changed flags and framebuffer, and initializes
 * the fonts for rendering. The constructor also initializes the background image, window
 * switcher, and application finder components. Finally, it sets the initial state of the
 * window manager, including the focus window, display switcher, and display appfinder flags,
 * and performs an initial render of the window manager's state.
 *
 * @param width The width of the window manager's display area.
 * @param height The height of the window manager's display area.
 */
WindowManager::WindowManager(int width, int height): w(width), h(height) {
  tw = w / (1 << tile_shift) + 1;
  th = h / (1 << tile_shift) + 1;
  changed = new bool [tw * th];
  fb = new uint32_t[w * h];
  font = new BDF_Font("/share/fonts/Courier-7.bdf");
  title_font = new BDF_Font("/share/fonts/Courier-8.bdf");
  focus = nullptr;
  display_switcher = false;
  display_appfinder = true;
  memset(windows, 0, sizeof(windows));
  background = new BgImage(this, width, height);
  switcher = new WindowSwitcher(this, width, height);
  appfinder = new AppFinder(this);
  render();
}

/**
 * @brief Destructor for the WindowManager class.
 * 
 * This method is responsible for cleaning up dynamically allocated resources
 * associated with the WindowManager instance. It ensures that all allocated
 * memory is properly released to prevent memory leaks. Specifically, it:
 * - Deletes the dynamically allocated framebuffer array (`fb`).
 * - Deletes the dynamically allocated `changed` array.
 * - Deletes the dynamically allocated `font` object.
 * - Deletes the dynamically allocated `title_font` object.
 * - Deletes the dynamically allocated `background` object.
 * - Deletes the dynamically allocated `switcher` object.
 * 
 * This destructor is automatically called when a WindowManager object goes out
 * of scope or is explicitly deleted.
 */
WindowManager::~WindowManager() {
  delete [] fb;
  delete [] changed;
  delete font;
  delete title_font;
  delete background;
  delete switcher;
}

/**
 * @brief Spawns a new window with the specified executable path and arguments.
 *
 * This method creates a new window by launching the executable located at `path` with the
 * provided arguments `argv`. The window is managed by the WindowManager and is assigned a
 * position on the screen. The position is determined by static variables `wx` and `wy`, which
 * are incremented after each window creation to avoid overlapping.
 *
 * The method sets up the environment for the new window, including the `NAVY_HOME` environment
 * variable and, if compiled for the native ISA, the `LD_PRELOAD` environment variable to load
 * the native shared library.
 *
 * If there is an available slot in the `windows` list, the new window is created, focused, and
 * returned. If no slots are available, the method returns `nullptr`.
 *
 * @param path The path to the executable to be launched in the new window.
 * @param argv The arguments to pass to the executable.
 * @return A pointer to the newly created Window object, or `nullptr` if no slots are available.
 */
Window *WindowManager::spawn(const char *path, const char *argv[]) {
  static int wx = 0, wy = 0;
  char navyhome[128];
  snprintf(navyhome, sizeof(navyhome), "NAVY_HOME=%s", getenv("NAVY_HOME"));
#ifdef __ISA_NATIVE__
  char ld_preload[256];
  snprintf(ld_preload, sizeof(ld_preload), "LD_PRELOAD=%s/libs/libos/build/native.so", getenv("NAVY_HOME"));
#endif
  const char *envp[] = {
    "NWM_APP=1",
#ifdef __ISA_NATIVE__
    ld_preload,
#endif
    navyhome,
    NULL,
  };
  for (Window *&win: windows) {
    if (!win) {
      win = new Window(this, path, argv, envp);
      focus = win;
      win->move(wx, wy);
      wx += 30; wy += 20;
      return win;
    }
  }
  return nullptr;
}

/**
 * @brief Renders the current state of the WindowManager.
 *
 * This method handles the rendering of all visible windows and UI elements managed by the WindowManager.
 * It first draws the background window. If a window is in focus, it renders all non-focused windows first,
 * followed by the focused window. If the window switcher is active, it synchronizes and renders the switcher.
 * If the app finder is displayed, it renders the app finder window.
 *
 * The method also handles the rendering of changed tiles in the framebuffer. It iterates over the tile grid,
 * identifies contiguous regions of changed tiles, and updates the corresponding regions in the framebuffer
 * using the NDL_DrawRect function. After rendering, it resets the changed flags for all tiles.
 *
 * @note The method assumes that the framebuffer and tile grid dimensions are properly initialized.
 * The tile size is determined by the tile_shift parameter, which defines the tile size as 2^tile_shift.
 */
void WindowManager::render() {
  draw_window(background); // TODO: more gracefully handle these
  if (focus) {
    for (auto *win: windows) {
      if (win && win != focus) {
        draw_window(win);
      }
    }
    draw_window(focus);
    if (display_switcher) {
      ((WindowSwitcher*)switcher)->sync();
      draw_window(switcher);
    }
  }

  if (display_appfinder) {
    draw_window(appfinder);
  }

  const int T = 1 << tile_shift;
  for (int y = 0; y < th; y ++) {
    for (int x = 0; x < tw; x ++)
      if (changed[x + y * tw]) {
        int n = 1;
        while (x + n < tw && changed[x + n + y * tw]) n ++;
        for (int i = 0; i < T; i ++) {
          int x1 = x * T, y1 = y * T + i;
          if (y1 >= h) continue;
          int sz = T * n;
          if (x1 + T * n > w) {
            sz -= x1 + T * n - w;
          }
          NDL_DrawRect(&fb[y1 * w + x1], x1, y1, sz, 1);
        }
        x += n - 1;
      }
  }
  memset(changed, false, tw * th);
}

/**
 * @brief Sets the focus to the specified window.
 *
 * This method updates the currently focused window to the provided window `win`.
 * Before changing the focus, it ensures the current focused window (if any) is
 * redrawn to reflect its state. After updating the focus, the new focused window
 * is redrawn to indicate that it is now in focus.
 *
 * @param win A pointer to the Window object to set as the new focus. Must not be null.
 */
void WindowManager::set_focus(Window *win) {
  if (focus) focus->draw();
  focus = win;
  focus->draw();
}

/**
 * @brief Draws a pixel at the specified coordinates with the given color.
 *
 * This method sets the color of the pixel at the specified (x, y) position in the framebuffer.
 * The method checks if the coordinates are within the bounds of the window (0 <= x < w and 0 <= y < h).
 * If the `has_alpha` flag is true and the color has an alpha channel (i.e., the most significant byte is non-zero),
 * the pixel is not drawn. Otherwise, the pixel is set to the specified color.
 *
 * @param x The x-coordinate of the pixel.
 * @param y The y-coordinate of the pixel.
 * @param color The color to set the pixel to, represented as a 32-bit unsigned integer.
 * @param has_alpha A flag indicating whether the color includes an alpha channel.
 */
void WindowManager::draw_px(int x, int y, uint32_t color, bool has_alpha) {
  if (x >= 0 && x < w && y >= 0 && y < h) {
    if (!(has_alpha && (color >> 24) != 0)) {
      fb[y * w + x] = color;
    }
  }
}

/**
 * @brief Draws a window onto the screen by iterating over its tiles and pixels.
 *
 * This method draws the contents of the provided `Window` object onto the screen. It uses a tiled approach,
 * where the window is divided into tiles of size `T x T` (where `T = 1 << tile_shift`). Only tiles marked as
 * "changed" in the `changed` array are processed. For each tile, the method checks if it intersects with the
 * window's bounds. If it does, the method iterates over the pixels within the tile and draws them onto the screen
 * using the `draw_px` function. The pixel values are taken from the window's `canvas` array, and the method
 * respects the window's alpha channel if `has_alpha` is true.
 *
 * @param win Pointer to the `Window` object to be drawn. Must not be null.
 */
void WindowManager::draw_window(Window *win) {
  const int T = (1 << tile_shift);
  for (int x = 0; x < tw; x ++)
    for (int y = 0; y < th; y ++) 
      if (changed[x + y * tw]) {
        int basex = x * T, basey = y * T;
        if (basex + T < win->x) continue;
        if (basey + T < win->y) continue;
        if (basex >= win->x + win->w) continue;
        if (basey >= win->y + win->h) continue;
        for (int i = 0; i < T; i ++)
          for (int j = 0; j < T; j ++) {
            int x1 = basex + i - win->x, y1 = basey + j - win->y;
            if (x1 >= 0 && x1 < win->w && y1 >= 0 && y1 < win->h) {
              draw_px(basex + i, basey + j, win->canvas[y1 * win->w + x1], win->has_alpha);
            }
          }
      }
}

/**
 * @brief Marks a specific tile as dirty within the window.
 *
 * This method checks if the given coordinates (x, y) are within the bounds of the window.
 * If the coordinates are valid, it calculates the corresponding tile index based on the
 * tile size (determined by `tile_shift`) and marks that tile as dirty in the `changed` array.
 * The `changed` array is used to track which tiles need to be redrawn.
 *
 * @param x The x-coordinate of the pixel within the window.
 * @param y The y-coordinate of the pixel within the window.
 */
void WindowManager::mark_dirty(int x, int y) {
  if (x >= 0 && y >= 0 && x < w && y < h) {
    changed[tw * (y >> tile_shift) + (x >> tile_shift)] = true;
  }
}
