#include <NDL.h>
#include <sdl-video.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/**
 * Blits (copies) a portion of the source surface onto the destination surface.
 *
 * This function copies a rectangular area from the source surface (`src`) to the
 * destination surface (`dst`). The area to be copied from the source surface is
 * defined by the `srcrect` parameter, and the location where it will be placed
 * on the destination surface is defined by the `dstrect` parameter.
 *
 * If `srcrect` is NULL, the entire source surface is copied. If `dstrect` is NULL,
 * the copied area is placed at the top-left corner of the destination surface.
 *
 * Both surfaces must have the same bits per pixel (BPP) format. If the formats
 * differ, the function will assert and terminate the program.
 *
 * @param src      The source surface from which to copy the pixels.
 * @param srcrect  A pointer to the rectangle defining the area to copy from the
 *                 source surface. If NULL, the entire source surface is used.
 * @param dst      The destination surface where the pixels will be copied to.
 * @param dstrect  A pointer to the rectangle defining the location on the
 *                 destination surface where the pixels will be placed. If NULL,
 *                 the pixels are placed at the top-left corner of the destination.
 */
void SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
  assert(dst && src);
  assert(dst->format->BitsPerPixel == src->format->BitsPerPixel);
}

/**
 * Fills a rectangular area of a surface with a specified color.
 *
 * This function fills the rectangular area defined by `dstrect` on the surface `dst` with the color `color`.
 * If `dstrect` is NULL, the entire surface is filled with the specified color.
 *
 * @param dst The destination surface to be filled. This must be a valid surface pointer.
 * @param dstrect A pointer to the SDL_Rect structure defining the rectangular area to fill.
 *               If NULL, the entire surface is filled.
 * @param color The color to fill the rectangle with, in the format of the surface's pixel format.
 *              This is typically a 32-bit value representing the color in ARGB format.
 *
 * @note The color value should be in the format of the destination surface's pixel format.
 *       Use SDL_MapRGB() or SDL_MapRGBA() to convert RGB or RGBA values to the correct format.
 *       If the surface has a palette, the color index is used directly.
 *
 * @return void
 */
void SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, uint32_t color) {
}

/**
 * Updates a rectangular region of the screen or surface.
 *
 * This function updates the specified rectangular region of the screen or surface
 * by copying the contents of the surface's pixels to the display. It is typically
 * used to refresh a portion of the screen after modifying the surface's pixel data.
 *
 * @param s Pointer to the SDL_Surface structure representing the surface to update.
 * @param x The x-coordinate of the top-left corner of the rectangle to update.
 * @param y The y-coordinate of the top-left corner of the rectangle to update.
 * @param w The width of the rectangle to update. If 0, the entire width of the
 *          surface is updated.
 * @param h The height of the rectangle to update. If 0, the entire height of the
 *          surface is updated.
 *
 * @note If both `w` and `h` are 0, the entire surface is updated. This function
 *       is useful for optimizing screen updates by limiting the refresh to only
 *       the modified regions of the surface.
 */
void SDL_UpdateRect(SDL_Surface *s, int x, int y, int w, int h) {
}

// APIs below are already implemented.

static inline int maskToShift(uint32_t mask) {
  switch (mask) {
    case 0x000000ff: return 0;
    case 0x0000ff00: return 8;
    case 0x00ff0000: return 16;
    case 0xff000000: return 24;
    case 0x00000000: return 24; // hack
    default: assert(0);
  }
}

/**
 * Creates a new SDL_Surface with the specified parameters.
 *
 * This function allocates and initializes a new SDL_Surface with the given width, height, and bit depth.
 * The surface can be either 8-bit (palettized) or 32-bit (true color) depending on the `depth` parameter.
 * For 8-bit surfaces, a palette with 256 colors is automatically allocated and initialized to zero.
 * For 32-bit surfaces, the provided color masks (Rmask, Gmask, Bmask, Amask) are used to define the pixel format.
 *
 * @param flags     Flags to control surface creation. Use SDL_PREALLOC to indicate that the pixel data is pre-allocated.
 * @param width     The width of the surface in pixels.
 * @param height    The height of the surface in pixels.
 * @param depth     The bit depth of the surface. Must be either 8 or 32.
 * @param Rmask     The red mask for 32-bit surfaces.
 * @param Gmask     The green mask for 32-bit surfaces.
 * @param Bmask     The blue mask for 32-bit surfaces.
 * @param Amask     The alpha mask for 32-bit surfaces.
 *
 * @return          A pointer to the newly created SDL_Surface on success, or NULL on failure.
 *                  The caller is responsible for freeing the surface using SDL_FreeSurface.
 */
SDL_Surface* SDL_CreateRGBSurface(uint32_t flags, int width, int height, int depth,
    uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask) {
  assert(depth == 8 || depth == 32);
  SDL_Surface *s = malloc(sizeof(SDL_Surface));
  assert(s);
  s->flags = flags;
  s->format = malloc(sizeof(SDL_PixelFormat));
  assert(s->format);
  if (depth == 8) {
    s->format->palette = malloc(sizeof(SDL_Palette));
    assert(s->format->palette);
    s->format->palette->colors = malloc(sizeof(SDL_Color) * 256);
    assert(s->format->palette->colors);
    memset(s->format->palette->colors, 0, sizeof(SDL_Color) * 256);
    s->format->palette->ncolors = 256;
  } else {
    s->format->palette = NULL;
    s->format->Rmask = Rmask; s->format->Rshift = maskToShift(Rmask); s->format->Rloss = 0;
    s->format->Gmask = Gmask; s->format->Gshift = maskToShift(Gmask); s->format->Gloss = 0;
    s->format->Bmask = Bmask; s->format->Bshift = maskToShift(Bmask); s->format->Bloss = 0;
    s->format->Amask = Amask; s->format->Ashift = maskToShift(Amask); s->format->Aloss = 0;
  }

  s->format->BitsPerPixel = depth;
  s->format->BytesPerPixel = depth / 8;

  s->w = width;
  s->h = height;
  s->pitch = width * depth / 8;
  assert(s->pitch == width * s->format->BytesPerPixel);

  if (!(flags & SDL_PREALLOC)) {
    s->pixels = malloc(s->pitch * height);
    assert(s->pixels);
  }

  return s;
}

/**
 * Creates an SDL_Surface from an existing pixel buffer.
 *
 * This function creates a new SDL_Surface using the provided pixel data, width, height, depth,
 * pitch, and color masks. The pixel data is not copied; instead, the surface directly references
 * the provided pixel buffer. The pitch must match the expected pitch for the given width, height,
 * and depth, otherwise an assertion will fail.
 *
 * @param pixels   A pointer to the pixel data to be used for the surface. This data is not copied.
 * @param width    The width of the surface in pixels.
 * @param height   The height of the surface in pixels.
 * @param depth    The depth of the surface in bits per pixel.
 * @param pitch    The pitch of the surface (the number of bytes per row, including padding).
 * @param Rmask    The red component mask for the pixel format.
 * @param Gmask    The green component mask for the pixel format.
 * @param Bmask    The blue component mask for the pixel format.
 * @param Amask    The alpha component mask for the pixel format.
 *
 * @return A pointer to the newly created SDL_Surface, or NULL on failure.
 */
SDL_Surface* SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth,
    int pitch, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask) {
  SDL_Surface *s = SDL_CreateRGBSurface(SDL_PREALLOC, width, height, depth,
      Rmask, Gmask, Bmask, Amask);
  assert(pitch == s->pitch);
  s->pixels = pixels;
  return s;
}

/**
 * Frees the resources associated with an SDL_Surface.
 *
 * This function safely deallocates the memory allocated for the given SDL_Surface
 * and its associated resources, including the pixel data, color palette, and format.
 * If the surface is NULL, the function does nothing.
 *
 * The function performs the following steps:
 * 1. Checks if the surface (s) is not NULL.
 * 2. If the surface has a format (s->format), it checks for and frees the palette
 *    and its colors if they exist.
 * 3. Frees the format structure itself.
 * 4. If the surface has pixel data (s->pixels) and the pixel data was not preallocated
 *    (i.e., the SDL_PREALLOC flag is not set), it frees the pixel data.
 * 5. Finally, frees the surface structure itself.
 *
 * @param s Pointer to the SDL_Surface to be freed. If NULL, the function does nothing.
 */
void SDL_FreeSurface(SDL_Surface *s) {
  if (s != NULL) {
    if (s->format != NULL) {
      if (s->format->palette != NULL) {
        if (s->format->palette->colors != NULL) free(s->format->palette->colors);
        free(s->format->palette);
      }
      free(s->format);
    }
    if (s->pixels != NULL && !(s->flags & SDL_PREALLOC)) free(s->pixels);
    free(s);
  }
}

/**
 * Sets up the video mode with the specified width, height, bits per pixel (bpp), and flags.
 * This function initializes a display surface with the given parameters, which can be used for rendering graphics.
 * If the `SDL_HWSURFACE` flag is set in the `flags` parameter, the function will attempt to open a hardware-accelerated canvas
 * using the `NDL_OpenCanvas` function, which may adjust the provided width and height to fit the hardware constraints.
 * The function then creates and returns an `SDL_Surface` object with the specified dimensions, color depth, and pixel format masks.
 *
 * @param width The desired width of the display surface in pixels.
 * @param height The desired height of the display surface in pixels.
 * @param bpp The desired bits per pixel (color depth) for the display surface.
 * @param flags A combination of flags that control the type of surface to be created. Common flags include `SDL_HWSURFACE`,
 *              `SDL_SWSURFACE`, `SDL_FULLSCREEN`, and `SDL_DOUBLEBUF`.
 *
 * @return A pointer to the newly created `SDL_Surface` object. If the function fails to create the surface, it returns NULL.
 */
SDL_Surface* SDL_SetVideoMode(int width, int height, int bpp, uint32_t flags) {
  if (flags & SDL_HWSURFACE) NDL_OpenCanvas(&width, &height);
  return SDL_CreateRGBSurface(flags, width, height, bpp,
      DEFAULT_RMASK, DEFAULT_GMASK, DEFAULT_BMASK, DEFAULT_AMASK);
}

/**
 * @brief Stretches or copies a portion of a source surface to a destination surface.
 *
 * This function takes a source surface (`src`) and a destination surface (`dst`) and
 * performs a stretch or copy operation based on the provided source and destination
 * rectangles (`srcrect` and `dstrect`). If the source and destination rectangles are
 * of the same size, the function performs a direct copy using `SDL_BlitSurface`. 
 * Otherwise, the function asserts, as stretching is not implemented in this version.
 *
 * @param src Pointer to the source `SDL_Surface` from which to copy or stretch.
 * @param srcrect Pointer to the source rectangle defining the portion of `src` to copy.
 *                If NULL, the entire source surface is used.
 * @param dst Pointer to the destination `SDL_Surface` where the copied or stretched
 *            content will be placed.
 * @param dstrect Pointer to the destination rectangle defining where the content
 *                will be placed on `dst`. This parameter must not be NULL.
 *
 * @note This function assumes that both the source and destination surfaces have the
 *       same bits per pixel (8 bits per pixel in this case). If the source and destination
 *       rectangles have different dimensions, the function asserts, as stretching is not
 *       implemented.
 */
void SDL_SoftStretch(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect) {
  assert(src && dst);
  assert(dst->format->BitsPerPixel == src->format->BitsPerPixel);
  assert(dst->format->BitsPerPixel == 8);

  int x = (srcrect == NULL ? 0 : srcrect->x);
  int y = (srcrect == NULL ? 0 : srcrect->y);
  int w = (srcrect == NULL ? src->w : srcrect->w);
  int h = (srcrect == NULL ? src->h : srcrect->h);

  assert(dstrect);
  if(w == dstrect->w && h == dstrect->h) {
    /* The source rectangle and the destination rectangle
     * are of the same size. If that is the case, there
     * is no need to stretch, just copy. */
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_BlitSurface(src, &rect, dst, dstrect);
  }
  else {
    assert(0);
  }
}

/**
 * Sets the color palette for an SDL surface.
 *
 * This function updates the color palette of the specified SDL surface. The palette is defined by
 * an array of SDL_Color structures, which specify the RGB values for each color in the palette.
 * The function assumes that the surface uses a palette-based format and that the `firstcolor`
 * parameter is 0, meaning the palette starts at the first index.
 *
 * If the surface is hardware-accelerated (SDL_HWSURFACE), the function asserts that the palette
 * contains exactly 256 colors and updates the surface's display rectangle to reflect the changes.
 *
 * @param s         Pointer to the SDL_Surface whose palette is to be set. Must not be NULL.
 * @param flags     Flags to control the operation (currently unused in this implementation).
 * @param colors    Pointer to an array of SDL_Color structures defining the new palette colors.
 *                  Must not be NULL.
 * @param firstcolor The index of the first color to set in the palette. This implementation
 *                  assumes `firstcolor` is 0.
 * @param ncolors   The number of colors in the `colors` array to copy into the palette.
 *
 * @note This function asserts that the surface, its format, and its palette are valid.
 * @note For hardware-accelerated surfaces, the palette must contain exactly 256 colors.
 */
void SDL_SetPalette(SDL_Surface *s, int flags, SDL_Color *colors, int firstcolor, int ncolors) {
  assert(s);
  assert(s->format);
  assert(s->format->palette);
  assert(firstcolor == 0);

  s->format->palette->ncolors = ncolors;
  memcpy(s->format->palette->colors, colors, sizeof(SDL_Color) * ncolors);

  if(s->flags & SDL_HWSURFACE) {
    assert(ncolors == 256);
    for (int i = 0; i < ncolors; i ++) {
      uint8_t r = colors[i].r;
      uint8_t g = colors[i].g;
      uint8_t b = colors[i].b;
    }
    SDL_UpdateRect(s, 0, 0, 0, 0);
  }
}

/**
 * Converts an array of pixels from ARGB format to ABGR format.
 * 
 * This function processes an array of pixels, where each pixel is represented
 * as a 32-bit value in ARGB format (Alpha, Red, Green, Blue), and converts it
 * to ABGR format (Alpha, Blue, Green, Red). The conversion is performed by
 * swapping the Red and Blue components of each pixel.
 *
 * The function operates on 16 pixels at a time for efficiency, processing the
 * bulk of the array in chunks of 16 pixels, and then handles any remaining
 * pixels individually.
 *
 * @param dst Pointer to the destination buffer where the converted pixels will
 *            be stored. The buffer must have enough space to hold `len` pixels.
 * @param src Pointer to the source buffer containing the ARGB pixels to be
 *            converted. The buffer must contain at least `len` pixels.
 * @param len The number of pixels to convert. This is the length of both the
 *            source and destination buffers.
 */
static void ConvertPixelsARGB_ABGR(void *dst, void *src, int len) {
  int i;
  uint8_t (*pdst)[4] = dst;
  uint8_t (*psrc)[4] = src;
  union {
    uint8_t val8[4];
    uint32_t val32;
  } tmp;
  int first = len & ~0xf;
  for (i = 0; i < first; i += 16) {
#define macro(i) \
    tmp.val32 = *((uint32_t *)psrc[i]); \
    *((uint32_t *)pdst[i]) = tmp.val32; \
    pdst[i][0] = tmp.val8[2]; \
    pdst[i][2] = tmp.val8[0];

    macro(i + 0); macro(i + 1); macro(i + 2); macro(i + 3);
    macro(i + 4); macro(i + 5); macro(i + 6); macro(i + 7);
    macro(i + 8); macro(i + 9); macro(i +10); macro(i +11);
    macro(i +12); macro(i +13); macro(i +14); macro(i +15);
  }

  for (; i < len; i ++) {
    macro(i);
  }
}

/**
 * Converts the pixel format of a given SDL_Surface to a specified format.
 * 
 * This function takes an existing SDL_Surface (`src`) and converts its pixel format
 * to the format specified by the `fmt` parameter. The conversion is performed by
 * creating a new SDL_Surface with the desired format and copying the pixel data
 * from the source surface to the new surface, ensuring the pixel values are
 * correctly mapped to the new format.
 *
 * The function assumes the source surface uses a 32-bit pixel format (as enforced
 * by the `assert` checks). It also ensures that the source surface's pitch (bytes
 * per row) is consistent with its width and pixel format. Additionally, it verifies
 * that the source and destination formats have the same bits per pixel and that
 * the green and alpha masks are compatible.
 *
 * @param src   The source SDL_Surface to convert. Must be a 32-bit surface.
 * @param fmt   The desired SDL_PixelFormat for the converted surface.
 * @param flags The flags to use when creating the new surface (e.g., SDL_HWSURFACE).
 *
 * @return      A new SDL_Surface with the converted pixel format, or NULL if an
 *              error occurs during surface creation or pixel conversion.
 */
SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt, uint32_t flags) {
  assert(src->format->BitsPerPixel == 32);
  assert(src->w * src->format->BytesPerPixel == src->pitch);
  assert(src->format->BitsPerPixel == fmt->BitsPerPixel);

  SDL_Surface* ret = SDL_CreateRGBSurface(flags, src->w, src->h, fmt->BitsPerPixel,
    fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);

  assert(fmt->Gmask == src->format->Gmask);
  assert(fmt->Amask == 0 || src->format->Amask == 0 || (fmt->Amask == src->format->Amask));
  ConvertPixelsARGB_ABGR(ret->pixels, src->pixels, src->w * src->h);

  return ret;
}

/**
 * Maps a set of RGBA color components to a 32-bit pixel value based on the specified pixel format.
 *
 * This function takes individual 8-bit red (r), green (g), blue (b), and alpha (a) components
 * and combines them into a single 32-bit pixel value according to the provided `SDL_PixelFormat`.
 * The pixel format defines the bit shifts and masks for each color component, ensuring the
 * resulting pixel value is compatible with the format.
 *
 * @param fmt Pointer to the `SDL_PixelFormat` structure that describes the target pixel format.
 * @param r   The 8-bit red component of the color.
 * @param g   The 8-bit green component of the color.
 * @param b   The 8-bit blue component of the color.
 * @param a   The 8-bit alpha (transparency) component of the color.
 * @return    A 32-bit pixel value that represents the color in the specified format.
 *
 * @note This function assumes the pixel format uses 4 bytes per pixel (32 bits) and asserts
 *       this condition. If the pixel format includes an alpha mask, the alpha component is
 *       included in the result; otherwise, it is ignored.
 */
uint32_t SDL_MapRGBA(SDL_PixelFormat *fmt, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  assert(fmt->BytesPerPixel == 4);
  uint32_t p = (r << fmt->Rshift) | (g << fmt->Gshift) | (b << fmt->Bshift);
  if (fmt->Amask) p |= (a << fmt->Ashift);
  return p;
}

/**
 * Locks the surface for direct pixel access.
 *
 * This function locks the given surface, ensuring that the pixel data is
 * directly accessible and preventing modifications by other operations.
 * Locking is necessary before performing direct pixel manipulation to
 * ensure thread safety and proper rendering.
 *
 * @param s The surface to lock. Must not be NULL.
 * @return 0 on success or a negative error code on failure.
 */
int SDL_LockSurface(SDL_Surface *s) {
  return 0;
}

/**
 * Unlocks a previously locked SDL_Surface, allowing it to be modified or accessed by other threads.
 *
 * This function releases the lock on the specified surface, which was previously acquired using
 * SDL_LockSurface(). After calling this function, the surface's pixel data can be safely accessed
 * or modified by other threads or operations. It is essential to call this function after completing
 * any direct pixel manipulation to ensure proper synchronization and avoid potential data corruption.
 *
 * @param s A pointer to the SDL_Surface to unlock. If the surface is not locked, this function has no effect.
 *
 * @note Always ensure that SDL_UnlockSurface() is called for every corresponding SDL_LockSurface() to
 *       maintain proper surface state and avoid undefined behavior.
 */
void SDL_UnlockSurface(SDL_Surface *s) {
}
