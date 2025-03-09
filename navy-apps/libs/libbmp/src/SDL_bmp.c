#include <SDL_bmp.h>
#include <BMP.h>
#include <assert.h>

/**
 * Loads a BMP image file and creates an SDL_Surface from it.
 *
 * This function reads a BMP image file specified by the `filename` parameter,
 * decodes its pixel data, and creates an SDL_Surface object that can be used
 * for rendering in SDL. The function internally uses `BMP_Load` to decode the
 * BMP file and extract the pixel data, width, and height. It then creates an
 * SDL_Surface using `SDL_CreateRGBSurfaceFrom` with the decoded pixel data.
 *
 * The surface is configured with 32-bit color depth (8 bits per channel) and
 * uses default RGBA masks (`DEFAULT_RMASK`, `DEFAULT_GMASK`, `DEFAULT_BMASK`,
 * `DEFAULT_AMASK`) for pixel format. The `SDL_PREALLOC` flag is cleared to
 * indicate that the pixel data is not preallocated by SDL.
 *
 * @param filename The path to the BMP file to load.
 * @return A pointer to the newly created SDL_Surface on success, or NULL on failure.
 *         The caller is responsible for freeing the surface using `SDL_FreeSurface`.
 */
SDL_Surface* SDL_LoadBMP(const char *filename) {
  int w, h;
  void *pixels = BMP_Load(filename, &w, &h);
  assert(pixels);
  SDL_Surface *s = SDL_CreateRGBSurfaceFrom(pixels, w, h, 32, w * sizeof(uint32_t),
      DEFAULT_RMASK, DEFAULT_GMASK, DEFAULT_BMASK, DEFAULT_AMASK);
  s->flags &= ~SDL_PREALLOC;
  return s;
}
