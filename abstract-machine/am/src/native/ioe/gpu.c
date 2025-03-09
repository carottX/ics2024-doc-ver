#include <am.h>
#include <SDL.h>
#include <fenv.h>

//#define MODE_800x600
#define WINDOW_W 800
#define WINDOW_H 600
#ifdef MODE_800x600
const int disp_w = WINDOW_W, disp_h = WINDOW_H;
#else
const int disp_w = 400, disp_h = 300;
#endif

#define FPS   60

#define RMASK 0x00ff0000
#define GMASK 0x0000ff00
#define BMASK 0x000000ff
#define AMASK 0x00000000

static SDL_Window *window = NULL;
static SDL_Surface *surface = NULL;

/**
 * Synchronizes the texture by blitting the source surface onto the window's surface
 * and updating the window surface. This function is typically used as a callback
 * for a timer to periodically refresh the window's display.
 *
 * @param interval The time interval (in milliseconds) at which the function is called.
 *                This value is returned unchanged, allowing the timer to continue
 *                with the same interval.
 * @param param A pointer to user-defined data. This parameter is not used in this
 *              function but is required for compatibility with the timer callback
 *              signature.
 * @return The same interval value that was passed in, ensuring the timer continues
 *         to operate with the same interval.
 */
static Uint32 texture_sync(Uint32 interval, void *param) {
  SDL_BlitScaled(surface, NULL, SDL_GetWindowSurface(window), NULL);
  SDL_UpdateWindowSurface(window);
  return interval;
}

/**
 * Initializes the GPU and related components for the application.
 * This function performs the following tasks:
 * 1. Initializes the SDL library with video and timer subsystems.
 * 2. Creates a window with the specified dimensions and OpenGL support.
 * 3. Creates a RGB surface for rendering with the specified display dimensions and color masks.
 * 4. Sets up a timer to synchronize texture updates at the specified frame rate (FPS).
 * The window title is set to "Native Application", and the window position is undefined.
 * The surface is created with 32-bit color depth and custom red, green, blue, and alpha masks.
 * The timer triggers the `texture_sync` callback function at the interval determined by the FPS.
 */
void __am_gpu_init() {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
  window = SDL_CreateWindow("Native Application",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      WINDOW_W, WINDOW_H, SDL_WINDOW_OPENGL);
  surface = SDL_CreateRGBSurface(SDL_SWSURFACE, disp_w, disp_h, 32,
      RMASK, GMASK, BMASK, AMASK);
  SDL_AddTimer(1000 / FPS, texture_sync, NULL);
}

/**
 * Configures the GPU settings by populating the provided AM_GPU_CONFIG_T structure.
 * This function sets the GPU configuration to indicate that the GPU is present,
 * but does not support hardware acceleration. It also sets the display width and
 * height based on the global variables `disp_w` and `disp_h`, and initializes
 * the video memory size to 0.
 *
 * @param cfg A pointer to the AM_GPU_CONFIG_T structure where the GPU configuration
 *            will be stored. The structure will be updated with the following values:
 *            - present: Set to true, indicating the GPU is present.
 *            - has_accel: Set to false, indicating no hardware acceleration support.
 *            - width: Set to the value of `disp_w`, representing the display width.
 *            - height: Set to the value of `disp_h`, representing the display height.
 *            - vmemsz: Set to 0, indicating no video memory is allocated.
 */
void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = disp_w, .height = disp_h,
    .vmemsz = 0
  };
}

/**
 * Updates the GPU status to indicate that it is ready.
 *
 * This function sets the `ready` field of the provided `AM_GPU_STATUS_T` structure
 * to `true`, indicating that the GPU is in a ready state and available for use.
 *
 * @param stat A pointer to an `AM_GPU_STATUS_T` structure where the GPU status
 *             will be updated.
 */
void __am_gpu_status(AM_GPU_STATUS_T *stat) {
  stat->ready = true;
}

/**
 * @brief Draws a rectangular region of pixels onto the GPU framebuffer.
 *
 * This function takes a control structure `ctl` that specifies the position, dimensions, 
 * and pixel data of the region to be drawn. It creates an SDL surface from the provided 
 * pixel data, then blits (copies) this surface onto the global `surface` at the specified 
 * coordinates. If the width `w` or height `h` of the region is zero, the function returns 
 * immediately without performing any operations.
 *
 * @param ctl Pointer to an `AM_GPU_FBDRAW_T` structure containing the following fields:
 *            - `x`: The x-coordinate of the top-left corner of the region.
 *            - `y`: The y-coordinate of the top-left corner of the region.
 *            - `w`: The width of the region.
 *            - `h`: The height of the region.
 *            - `pixels`: Pointer to the pixel data to be drawn.
 * @note The global `surface` variable must be initialized and valid before calling this function.
 * @note The function clears all floating-point exceptions before proceeding.
 */
void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  int x = ctl->x, y = ctl->y, w = ctl->w, h = ctl->h;
  if (w == 0 || h == 0) return;
  feclearexcept(-1);
  SDL_Surface *s = SDL_CreateRGBSurfaceFrom(ctl->pixels, w, h, 32, w * sizeof(uint32_t),
      RMASK, GMASK, BMASK, AMASK);
  SDL_Rect rect = { .x = x, .y = y };
  SDL_BlitSurface(s, NULL, surface, &rect);
  SDL_FreeSurface(s);
}
