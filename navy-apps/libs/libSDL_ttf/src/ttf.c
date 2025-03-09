#include <SDL_ttf.h>
#include <fixedptc.h>
#include <assert.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

/**
 * Initializes the TTF (TrueType Font) library.
 * 
 * This function must be called before using any other TTF functions to ensure
 * the library is properly set up. It initializes internal data structures and
 * prepares the library for font rendering operations.
 * 
 * @return Returns 0 on success, or a negative error code if initialization fails.
 */
int TTF_Init() {
  return 0;
}

/**
 * Opens a TrueType font file and initializes a TTF_Font structure for rendering.
 *
 * This function reads the specified font file, loads it into memory, and initializes
 * the necessary data structures for rendering text using the font. It calculates
 * various font metrics such as ascent, descent, and height based on the provided
 * point size. The function also precomputes scaling factors and other rendering-related
 * data to optimize text rendering performance.
 *
 * @param file The path to the TrueType font file to be opened.
 * @param ptsize The desired point size for the font. This determines the size of the
 *               rendered text.
 *
 * @return A pointer to the initialized TTF_Font structure on success. If the file
 *         cannot be opened or the font cannot be initialized, NULL is returned.
 *
 * @note The caller is responsible for freeing the returned TTF_Font structure using
 *       the appropriate cleanup function when it is no longer needed.
 */
TTF_Font* TTF_OpenFont(const char *file, int ptsize) {
  SDL_RWops *f = SDL_RWFromFile(file, "r");
  if (f == NULL) return NULL;
  size_t size = SDL_RWsize(f);
  void *buf = malloc(size);
  assert(buf);
  size_t nread = SDL_RWread(f, buf, size, 1);
  assert(nread == 1);
  SDL_RWclose(f);

  stbtt_fontinfo *finfo = malloc(sizeof(*finfo));
  assert(finfo);
  int ret = stbtt_InitFont(finfo, buf, stbtt_GetFontOffsetForIndex(buf,0));
  assert(ret == 1);

  TTF_Font *font = malloc(sizeof(*font));
  font->finfo = finfo;
  font->file.buf = buf;
  font->file.size = size;
  font->ptsize = ptsize;

  // pre-computed data
  fixedpt pixel = fixedpt_muli(fixedpt_rconst(1.333333), ptsize);
  font->factor = stbtt_ScaleForPixelHeight(finfo, pixel);
  stbtt_GetFontVMetrics(finfo, &font->ascent, &font->descent, NULL);
  font->height = fixedpt_toint(fixedpt_muli(font->factor, font->ascent - font->descent));
  font->ascent = fixedpt_toint(fixedpt_muli(font->factor, font->ascent));
  font->descent = fixedpt_toint(fixedpt_muli(font->factor, font->descent));

  return font;
}

/**
 * Opens a font from an SDL_RWops source and returns a TTF_Font object.
 *
 * This function loads a font from a provided SDL_RWops source, which can be
 * a file, memory buffer, or other data stream. The font is loaded at the specified
 * point size. The caller can specify whether the SDL_RWops source should be
 * automatically freed when the font is closed.
 *
 * @param src A pointer to an SDL_RWops structure that provides the font data.
 * @param freesrc A flag indicating whether the SDL_RWops source should be freed
 *                when the font is closed. Non-zero means the source will be freed,
 *                zero means it will not.
 * @param ptsize The desired point size for the font.
 * @return A pointer to the loaded TTF_Font object on success, or NULL on failure.
 *         If the function fails, it returns NULL and no font is loaded.
 */
TTF_Font *TTF_OpenFontRW(SDL_RWops *src, int freesrc, int ptsize) {
  assert(0);
  return NULL;
}

/**
 * Retrieves the glyph metrics for a specified character in a TrueType font.
 * 
 * This function calculates the bounding box and other metrics for a glyph corresponding
 * to the given character code in the specified font. The bounding box is returned in
 * terms of the font's coordinate system and is scaled by the font's scaling factor.
 * 
 * @param font    A pointer to the TTF_Font structure representing the font.
 * @param ch      The character code for which to retrieve the glyph metrics.
 * @param minx    A pointer to store the leftmost x-coordinate of the glyph's bounding box.
 *                If NULL, this value is not computed.
 * @param maxx    A pointer to store the rightmost x-coordinate of the glyph's bounding box.
 *                If NULL, this value is not computed.
 * @param miny    A pointer to store the bottommost y-coordinate of the glyph's bounding box.
 *                If NULL, this value is not computed.
 * @param maxy    A pointer to store the topmost y-coordinate of the glyph's bounding box.
 *                If NULL, this value is not computed.
 * @param advance A pointer to store the advance width of the glyph. This parameter is
 *                currently not implemented and must be NULL.
 * 
 * @return Returns 0 on success. If the glyph index for the character is not found or
 *         if the glyph box cannot be retrieved, the function returns -1.
 */
int TTF_GlyphMetrics(TTF_Font *font, Uint16 ch, int *minx, int *maxx, int *miny, int *maxy, int *advance) {
  stbtt_fontinfo *finfo = font->finfo;
  int glyphIndex = stbtt_FindGlyphIndex(finfo, ch);
  if (glyphIndex == 0) return -1;
  int ret = stbtt_GetGlyphBox(finfo, glyphIndex, minx, miny, maxx, maxy);
  if (ret == 0) return -1;
  if (minx) *minx = fixedpt_toint(fixedpt_muli(font->factor, *minx));
  if (miny) *miny = fixedpt_toint(fixedpt_muli(font->factor, *miny));
  if (maxx) *maxx = fixedpt_toint(fixedpt_muli(font->factor, *maxx));
  if (maxy) *maxy = fixedpt_toint(fixedpt_muli(font->factor, *maxy));
  assert(advance == NULL); // not implemented
  return 0;
}

/**
 * Retrieves the ascent of the specified font.
 * 
 * The ascent is the distance from the baseline to the highest position 
 * characters extend to within the font. This value is typically positive 
 * and is used in text rendering to determine the vertical positioning 
 * of characters.
 *
 * @param font A pointer to the TTF_Font structure representing the font.
 * @return The ascent of the font in pixels.
 */
int TTF_FontAscent(TTF_Font *font) {
  return font->ascent;
}

/**
 * Retrieves the height of the specified font.
 *
 * This function returns the height of the font in pixels. The height is a measure of the vertical
 * space that the font occupies, typically including the ascent, descent, and any additional spacing
 * defined by the font. This value is useful for determining the vertical space required to render
 * text using this font.
 *
 * @param font A pointer to the TTF_Font structure representing the font.
 * @return The height of the font in pixels.
 */
int TTF_FontHeight(TTF_Font *font) {
  return font->height;
}

static struct {
  SDL_Color fg, bg;
  SDL_Color palette[256];
} palCache = {};

/**
 * Renders a glyph as a shaded SDL_Surface using the specified font and colors.
 * 
 * This function takes a glyph represented by the Unicode character `ch` and renders it
 * as an 8-bit SDL_Surface with a shaded effect. The shading is achieved by interpolating
 * between the foreground color (`fg`) and the background color (`bg`). The resulting
 * surface uses a custom palette to represent the gradient between the two colors.
 *
 * @param font  Pointer to the TTF_Font structure containing the font data.
 * @param ch    The Unicode character (glyph) to render.
 * @param fg    The foreground color (SDL_Color) for the glyph.
 * @param bg    The background color (SDL_Color) for the shading.
 * 
 * @return      A pointer to the rendered SDL_Surface, or NULL if the glyph is not found
 *              or rendering fails. The caller is responsible for freeing the surface
 *              using SDL_FreeSurface().
 */
SDL_Surface *TTF_RenderGlyph_Shaded(TTF_Font *font, Uint16 ch, SDL_Color fg, SDL_Color bg) {
  stbtt_fontinfo *finfo = font->finfo;
  int glyphIndex = stbtt_FindGlyphIndex(finfo, ch);
  if (glyphIndex == 0) return NULL;
  int w, h;
  uint8_t *pixels = stbtt_GetGlyphBitmap(finfo, 0, font->factor, glyphIndex, &w, &h, NULL, NULL);

  SDL_Surface *s = SDL_CreateRGBSurfaceFrom(pixels, w, h, 8, w, 0, 0, 0, 0);
  s->flags &= ~SDL_PREALLOC;
  if (!(fg.val == palCache.fg.val && bg.val == palCache.bg.val)) {
    // cache miss
    int rdiff = fg.r - bg.r;
    int gdiff = fg.g - bg.g;
    int bdiff = fg.b - bg.b;
    int adiff = fg.a - bg.a;
    for (int i = 0; i < 256; i ++) {
      palCache.palette[i].r = bg.r + (i*rdiff) / 255;
      palCache.palette[i].g = bg.g + (i*gdiff) / 255;
      palCache.palette[i].b = bg.b + (i*bdiff) / 255;
      palCache.palette[i].a = bg.a + (i*adiff) / 255;
    }
    palCache.palette[0].a = bg.a;
    palCache.fg.val = fg.val;
    palCache.bg.val = bg.val;
  }
  memcpy(s->format->palette->colors, palCache.palette, sizeof(palCache.palette));
  return s;
}
