#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <nwm.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#ifdef __ISA_NATIVE__
#include <sys/prctl.h>
#include <signal.h>
#endif

/**
 * @brief Draws a single pixel on the window's canvas at the specified coordinates with the given color.
 * 
 * This method checks if the provided coordinates (x, y) are within the bounds of the window's canvas.
 * If the coordinates are valid, it marks the corresponding pixel as dirty in the window manager (wm)
 * and updates the canvas with the specified color. The pixel is only drawn if the coordinates are within
 * the window's width (w) and height (h).
 * 
 * @param x The x-coordinate of the pixel to draw, relative to the window's origin.
 * @param y The y-coordinate of the pixel to draw, relative to the window's origin.
 * @param color The color to set the pixel to, represented as a 32-bit unsigned integer.
 */
void Window::draw_raw_px(int x, int y, uint32_t color) {
  if (x >= 0 && y >= 0 && x < w && y < h) {
    wm->mark_dirty(this->x + x, this->y + y);
    canvas[x + y * w] = color;
  }
}

/**
 * @brief Draws a pixel at the specified coordinates with the given color.
 * 
 * This method translates the provided coordinates (x, y) by adding the internal
 * offsets (dx, dy) and then delegates the actual drawing operation to the
 * `draw_raw_px` method. The color is passed directly to `draw_raw_px` without
 * modification.
 * 
 * @param x The x-coordinate of the pixel to draw.
 * @param y The y-coordinate of the pixel to draw.
 * @param color The color of the pixel, represented as a 32-bit unsigned integer.
 */
void Window::draw_px(int x, int y, uint32_t color) {
  draw_raw_px(x + dx, y + dy, color);
}

/**
 * @brief Draws a single character on the window using a specified BDF font.
 *
 * This method renders a character `ch` at the specified coordinates `(x, y)` on the window.
 * The character is drawn using the provided `font`, which is a BDF (Bitmap Distribution Format) font.
 * The character is rendered in the specified `color`.
 *
 * The method retrieves the bitmap data for the character from the font and iterates over each pixel
 * in the bitmap. If a pixel in the bitmap is set (i.e., the corresponding bit is 1), the method
 * draws that pixel on the window at the appropriate position using the specified color.
 *
 * @param font Pointer to the BDF_Font object containing the font data.
 * @param x The x-coordinate on the window where the character should be drawn.
 * @param y The y-coordinate on the window where the character should be drawn.
 * @param ch The character to be drawn.
 * @param color The color to use for drawing the character.
 */
void Window::draw_ch(BDF_Font *font, int x, int y, char ch, uint32_t color) {
  uint32_t *bm = font->font[ch];
  if (!bm) return;
  for (int j = 0; j < font->h; j ++)
    for (int i = 0; i < font->w; i ++)
      if ((bm && ((bm[j] >> i) & 1))) {
        draw_px(x + i, y + j, color);
      }
}

/**
 * @brief Draws a single character using a raw bitmap font at the specified position.
 *
 * This method takes a character `ch` and renders it on the window using the provided
 * bitmap font (`font`). The character is drawn at the coordinates `(x, y)` with the
 * specified `color`. The method iterates over the bitmap data of the character, checking
 * each pixel to determine if it should be drawn. If a pixel in the bitmap is set (1),
 * it is rendered at the corresponding position on the window using the `draw_raw_px`
 * method.
 *
 * @param font Pointer to the BDF_Font object containing the bitmap font data.
 * @param x The x-coordinate where the character will be drawn.
 * @param y The y-coordinate where the character will be drawn.
 * @param ch The character to be drawn.
 * @param color The color to use for drawing the character.
 */
void Window::draw_raw_ch(BDF_Font *font, int x, int y, char ch, uint32_t color) {
  uint32_t *bm = font->font[ch];
  if (!bm) return;
  for (int j = 0; j < font->h; j ++)
    for (int i = 0; i < font->w; i ++)
      if ((bm && ((bm[j] >> i) & 1))) {
        draw_raw_px(x + i, y + j, color);
      }
}

/**
 * @brief Marks all pixels within the window as dirty in the window manager.
 *
 * This method iterates over the entire area of the window, defined by its width (`w`) and height (`h`),
 * and marks each pixel as dirty in the window manager (`wm`). The dirty marking is done by calling
 * `wm->mark_dirty()` with the coordinates of each pixel relative to the window's position (`x`, `y`).
 * This is typically used to signal that the window's content has changed and needs to be redrawn.
 */
void Window::draw() {
  for (int i = 0; i < w; i ++)
    for (int j = 0; j < h; j ++) {
      wm->mark_dirty(this->x + i, this->y + j);
    }
}

/**
 * @brief Constructs a Window object and initializes its properties.
 *
 * This constructor initializes a Window object with the provided WindowManager,
 * command, arguments, and environment variables. It sets up the window's
 * properties such as position, size, and canvas. If a command is provided, it
 * creates a child process to execute the command, sets up communication pipes
 * between the window manager and the application, and configures the frame
 * buffer. The window can have a title bar and alpha transparency based on the
 * provided command. If no command is provided, the window is treated as an
 * internal window without a title bar and with alpha transparency enabled.
 *
 * @param wm Pointer to the WindowManager instance managing this window.
 * @param cmd The command to be executed in the child process. If nullptr,
 *            the window is treated as an internal window.
 * @param argv Array of arguments for the command. The first element should
 *             be the command itself.
 * @param envp Array of environment variables for the command.
 */
Window::Window(WindowManager *wm, const char *cmd, const char * const *argv, const char **envp) {
  this->wm = wm;
  x = y = w = h = 0;
  canvas = nullptr;
  fb = nullptr;

  if (cmd) {
    has_titlebar = true;
    has_alpha = false;
    const char *title = cmd;
    for (const char *p = cmd; *p; p ++) {
      if (*p == '/') title = p + 1;
    }
    strcpy(this->title, title);

    // create child process
    assert(0 == pipe2(nwm_to_app, O_NONBLOCK));
    assert(0 == pipe2(app_to_nwm, O_NONBLOCK));

    read_fd = app_to_nwm[0];
    write_fd = nwm_to_app[1];
    fbdev_fd = memfd_create("nwm-fbdev", 0);

    pid_t p = fork();
    if (p == 0) { // child
#ifdef __ISA_NATIVE__
      // install a parent death signal in the chlid
      int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
      if (r == -1) {
        perror("prctl error");
        assert(0);
      }
#endif

      // FIXME: what if they are overlapped
      dup2(nwm_to_app[0], 3);
      dup2(app_to_nwm[1], 4);
      dup2(fbdev_fd, 5);

      // close enough files, which fixes the fork-after-SDL issue
      for (int i = 6; i < 20; i ++) close(i);

      execve(argv[0], (char**)argv, (char**)envp);
      assert(0);
    } else {
    }
  } else {
    // an internal window (without title bar)
    has_alpha = true;
    has_titlebar = false;
    read_fd = write_fd = -1;
    fbdev_fd = -1;
  }
}

/**
 * @brief Moves the window to the specified coordinates (x, y) on the screen.
 * 
 * This method updates the window's position by setting its internal x and y 
 * coordinates to the provided values. Before and after updating the position, 
 * the window is redrawn to reflect the changes visually. This ensures that the 
 * window's appearance is updated immediately after the move operation.
 * 
 * @param x The new horizontal coordinate (x-axis) for the window's position.
 * @param y The new vertical coordinate (y-axis) for the window's position.
 */
void Window::move(int x, int y) {
  draw();
  this->x = x;
  this->y = y;
  draw();
}

/**
 * @brief Centers the window within its parent window or screen.
 *
 * This method calculates the new position for the window such that it is centered
 * relative to its parent window or screen. The new position is determined by
 * subtracting the window's width (`w`) and height (`h`) from the parent window's
 * width (`wm->w`) and height (`wm->h`), respectively, and then dividing the result
 * by 2. The window is then moved to this calculated position using the `move` method.
 */
void Window::center() {
  move((wm->w - w) / 2, (wm->h - h) / 2);
}

/**
 * @brief Resizes the window to the specified width and height.
 *
 * This method handles the resizing of the window, including updating the internal
 * dimensions, reallocating the canvas, and redrawing the window contents. If the
 * window has a title bar, it adjusts the dimensions to account for the border and
 * title bar height. The method also manages the frame buffer memory, unmapping
 * and remapping it if necessary. After resizing, the window is redrawn to reflect
 * the new dimensions and any associated UI elements (e.g., title bar, borders).
 *
 * @param width The new width of the window.
 * @param height The new height of the window.
 */
void Window::resize(int width, int height) {
  draw();

  if (canvas) {
    delete [] canvas;
    canvas = nullptr;
  }

  if (fbdev_fd != -1) munmap(fb, fw * fh * sizeof(uint32_t));

  if (has_titlebar) {
    this->w = width + border_px * 2;
    this->h = height + border_px * 2 + title_px;
    this->dx = border_px;
    this->dy = title_px + border_px;
  } else {
    this->w = width;
    this->h = height;
    this->dx = 0;
    this->dy = 0;
  }
  this->fw = width; // frame buffer w/h
  this->fh = height;

  if (fbdev_fd != -1) {
    int fbsize = fw * fh * sizeof(uint32_t);
    ftruncate(fbdev_fd, fbsize);
    fb = (uint32_t *)mmap(NULL, fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev_fd, 0);
    assert(fb != (void *) -1);
    write(write_fd, "mmap ok", 7);
  }

  canvas = new uint32_t[w * h];

  for (int i = 0; i < w; i ++)
    for (int j = 0; j < h; j ++)
      draw_raw_px(i, j, border_col);

  if (has_titlebar) {
    for (int i = 0; i < w; i ++) {
      for (int j = 0; j < title_px; j ++) {
        int cx = 100 - 100 * i / w;
        int cy = 100 - 100 * j / title_px;
        uint8_t c = (cx + cy) / 10 + 200;
        draw_raw_px(i, j, (c << 16) | (c << 8) | c);
      }
    }

    int x = (w - strlen(title) * wm->title_font->w) / 2;
    int y = 2;
    for (const char *p = title; *p; p ++) {
      draw_raw_ch(wm->title_font, x, y, *p, title_col);
      x += wm->title_font->w;
    }

    draw_raw_px(0, 0, 0xff000000);
    draw_raw_px(w - 1, 0, 0xff000000);
  }

  draw();
}

/**
 * @brief Destructor for the Window class.
 * 
 * This method is responsible for cleaning up resources allocated by the Window object.
 * It performs the following tasks:
 * 1. Deallocates the canvas memory if it was previously allocated, and sets the canvas pointer to nullptr.
 * 2. If the framebuffer device file descriptor (fbdev_fd) is valid, it unmaps the framebuffer memory,
 *    sets the framebuffer pointer to nullptr, and closes the file descriptor.
 * 3. If the read file descriptor (read_fd) is valid, it closes the pipes used for communication
 *    between the window manager and the application by closing both ends of the pipes.
 * 
 * This ensures that all dynamically allocated resources are properly released and file descriptors
 * are closed to prevent resource leaks.
 */
Window::~Window() {
  if (canvas) {
    delete [] canvas;
    canvas = nullptr;
  }
  if (fbdev_fd != -1) {
    munmap(fb, fw * fh * sizeof(uint32_t));
    fb = nullptr;
    close(fbdev_fd);
    fbdev_fd = -1;
  }
  if (read_fd != -1) {
     // close pipes
    for (int i = 0; i < 2; i ++) {
      close(nwm_to_app[i]);
      close(app_to_nwm[i]);
    }
  }
}

/**
 * @brief Updates the window by reading data from a file descriptor and resizing the window if necessary.
 * 
 * This method performs a non-blocking read from the file descriptor `read_fd`. If the read is successful,
 * it attempts to parse the data as two integers representing width and height. If both values are successfully
 * parsed, the window is resized accordingly. After processing the read data, the method updates the window's
 * canvas by copying the contents of the framebuffer (`fb`) to the canvas at the specified position (`dx`, `dy`).
 * Finally, the `draw()` method is called to render the updated canvas.
 * 
 * The method continues to read and process data in a loop until no more data is available or an error occurs.
 * The canvas update involves copying rows of pixel data from the framebuffer to the canvas, ensuring that the
 * window's display is updated with the latest content.
 */
void Window::update() {
  if (read_fd != -1) {
    do {
      char buf[64];
      int nread = read(read_fd, buf, sizeof(buf) - 1); // this a non-blocking read
      if (nread == -1) break;
      buf[nread] = '\0';
      int w, h;
      int ret = sscanf(buf, "%d %d", &w, &h);
      if (ret == 2) resize(w, h);
    } while (1);
    int y;
    for (y = 0; y < fh; y ++) {
      memcpy(&canvas[(dy + y) * w + dx], &fb[y * fw], fw * sizeof(uint32_t));
    }
    draw();
  }
}
