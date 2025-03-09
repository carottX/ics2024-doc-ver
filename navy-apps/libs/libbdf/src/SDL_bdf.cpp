#include <SDL_bdf.h>
#include <stdlib.h>
#include <assert.h>

/**
 * @brief Creates an SDL_Surface for a specific character using a BDF font.
 *
 * This method generates an SDL_Surface containing the bitmap representation of the specified
 * character `ch` using the provided BDF font. The character is rendered with the given foreground
 * color `fg` and background color `bg`. The method allocates memory for the pixel data and
 * constructs the surface using SDL_CreateRGBSurfaceFrom.
 *
 * @param font Pointer to the BDF_Font structure containing the font data.
 * @param ch The character to render on the surface.
 * @param fg The foreground color (in 32-bit format) for the character.
 * @param bg The background color (in 32-bit format) for the surface.
 *
 * @return Pointer to the created SDL_Surface. Returns NULL if the character is not found in the font.
 *
 * @note The caller is responsible for freeing the returned SDL_Surface using SDL_FreeSurface.
 * @warning The method asserts that memory allocation for pixel data succeeds. If allocation fails,
 *          the program will terminate.
 */
SDL_Surface* BDF_CreateSurface(BDF_Font *font, char ch, uint32_t fg, uint32_t bg) {
  uint32_t *bm = font->font[ch];
  if (!bm) return NULL;
  int w = font->w, h = font->h;
  uint32_t *pixels = (uint32_t *)malloc(w * h * sizeof(uint32_t));
  assert(pixels);
  for (int j = 0; j < h; j ++) {
    for (int i = 0; i < w; i ++) {
      pixels[j * w + i] = ((bm[j] >> i) & 1) ? fg : bg;
    }
  }
  SDL_Surface *s = SDL_CreateRGBSurfaceFrom(pixels, w, h, 32, w * sizeof(uint32_t),
      DEFAULT_RMASK, DEFAULT_GMASK, DEFAULT_BMASK, DEFAULT_AMASK);
  s->flags &= ~SDL_PREALLOC;
  return s;
}
