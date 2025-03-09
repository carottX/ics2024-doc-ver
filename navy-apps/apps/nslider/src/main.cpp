#include <SDL.h>
#include <SDL_bmp.h>
#include <stdio.h>
#include <assert.h>

#define W 400
#define H 300

// USAGE:
//   j/down - page down
//   k/up - page up
//   gg - first page

// number of slides
const int N = 10;
// slides path pattern (starts from 0)
const char *path = "/share/slides/slides-%d.bmp";

static SDL_Surface *screen = NULL;
static int cur = 0;

/**
 * @brief Renders an image to the screen.
 *
 * This method constructs a file name using the provided `path` format string and the current 
 * index `cur`, then loads the corresponding BMP image from the constructed file path. The 
 * loaded image is then blitted (copied) onto the main screen surface, and the screen is 
 * updated to display the rendered image. Finally, the method frees the memory allocated for 
 * the loaded image surface.
 *
 * @note The method assumes that the `path` format string and `cur` index are valid and that 
 * the corresponding BMP file exists. If the file cannot be loaded, the program will terminate 
 * due to the `assert` statement.
 */
void render() {
  char fname[256];
  sprintf(fname, path, cur);
  SDL_Surface *slide = SDL_LoadBMP(fname);
  assert(slide);
  SDL_BlitSurface(slide, NULL, screen, NULL);
  SDL_UpdateRect(screen, 0, 0, 0, 0);
  SDL_FreeSurface(slide);
}

/**
 * @brief Moves the current position backward by the specified number of repetitions.
 * 
 * This method adjusts the current position (`cur`) by subtracting the given number of repetitions (`rep`).
 * If `rep` is 0, it is treated as 1 to ensure at least a minimal backward movement.
 * The current position is clamped to a minimum value of 0 to prevent it from becoming negative.
 * After updating the position, the `render()` method is called to update the display or state accordingly.
 * 
 * @param rep The number of repetitions to move backward. If 0, it is treated as 1.
 */
void prev(int rep) {
  if (rep == 0) rep = 1;
  cur -= rep;
  if (cur < 0) cur = 0;
  render();
}

/**
 * Advances the current position by the specified number of repetitions and ensures it stays within bounds.
 * If the repetition count is zero, it is set to 1 to ensure progress.
 * After updating the current position, the method ensures it does not exceed the maximum limit (N - 1).
 * Finally, it triggers a rendering operation to reflect the updated state.
 *
 * @param rep The number of repetitions to advance the current position. If zero, it is treated as 1.
 */
void next(int rep) {
  if (rep == 0) rep = 1;
  cur += rep;
  if (cur >= N) cur = N - 1;
  render();
}

/**
 * @brief Main entry point for the application.
 * 
 * This method initializes the SDL library and sets up the video mode for the application.
 * It then enters an infinite loop where it waits for SDL events, particularly keyboard input.
 * 
 * The application responds to numeric key presses (0-9) by appending the corresponding digit
 * to an internal representation (`rep`). This representation is used to navigate through
 * a sequence of items or pages in the application.
 * 
 * The following key bindings are supported:
 * - 'J' or 'DOWN' key: Moves to the next item/page based on the current `rep` value, then resets `rep` and `g`.
 * - 'K' or 'UP' key: Moves to the previous item/page based on the current `rep` value, then resets `rep` and `g`.
 * - 'G' key: When pressed once, increments the `g` counter. When pressed twice in succession,
 *   moves to the 100,000th previous item/page, then resets `rep` and `g`.
 * 
 * The loop continues indefinitely until the application is terminated externally.
 * 
 * @return int Returns 0 upon successful execution.
 */
int main() {
  SDL_Init(0);
  screen = SDL_SetVideoMode(W, H, 32, SDL_HWSURFACE);

  int rep = 0, g = 0;

  render();

  while (1) {
    SDL_Event e;
    SDL_WaitEvent(&e);

    if (e.type == SDL_KEYDOWN) {
      switch(e.key.keysym.sym) {
        case SDLK_0: rep = rep * 10 + 0; break;
        case SDLK_1: rep = rep * 10 + 1; break;
        case SDLK_2: rep = rep * 10 + 2; break;
        case SDLK_3: rep = rep * 10 + 3; break;
        case SDLK_4: rep = rep * 10 + 4; break;
        case SDLK_5: rep = rep * 10 + 5; break;
        case SDLK_6: rep = rep * 10 + 6; break;
        case SDLK_7: rep = rep * 10 + 7; break;
        case SDLK_8: rep = rep * 10 + 8; break;
        case SDLK_9: rep = rep * 10 + 9; break;
        case SDLK_J:
        case SDLK_DOWN: next(rep); rep = 0; g = 0; break;
        case SDLK_K:
        case SDLK_UP: prev(rep); rep = 0; g = 0; break;
        case SDLK_G:
          g ++;
          if (g > 1) {
            prev(100000);
            rep = 0; g = 0;
          }
          break;
      }
    }
  }

  return 0;
}
