/*
 * A small header-only library to load an image into a RGB(A) SDL_Surface*,
 * like a stripped down version of SDL_Image, but using stb_image.h to decode
 * images and thus without any further external dependencies.
 * Supports all filetypes supported by stb_image (JPEG, PNG, TGA, BMP, PSD, ...
 * See stb_image.h for details).
 *
 * (C) 2015 Daniel Gibson
 *
 * Homepage: https://github.com/DanielGibson/Snippets/
 *
 * Dependencies:
 *     libSDL2      http://www.libsdl.org
 *     stb_image.h  https://github.com/nothings/stb
 *
 * Usage:
 *   Put this file and stb_image.h somewhere in your project.
 *   In *one* of your .c/.cpp files, do
 *     #define SDL_STBIMAGE_IMPLEMENTATION
 *     #include "SDL_stbimage.h"
 *   to create the implementation of this library in that file.
 *   You can just #include "SDL_stbimage.h" (without the #define) in other source
 *   files to use it there. (See also below this comment for an usage example)
 *   This header implicitly #includes <SDL.h> and "stb_image.h".
 *
 *   You can #define SDL_STBIMG_DEF before including this header if you want to
 *   prepend anything to the function signatures (like "static", "inline",
 *   "__declspec(dllexport)", ...)
 *     Example: #define SDL_STBIMG_DEF static inline
 *
 *   By default, this deactivates stb_image's load from file functions via
 *   #define STBI_NO_STDIO, as they use stdio.h  and that adds a dependency to the
 *   CRT on windows and with SDL you're better off using SDL_RWops, incl. SDL_RWFromFile()
 *   If you wanna use stbi_load(), stbi_info(), stbi_load_from_file() etc anyway, do
 *     #define SDL_STBIMG_ALLOW_STDIO
 *   before including this header.
 *   (Note that all the STBIMG_* functions of this lib will work without it)
 *
 *   stb_image.h uses assert.h by default. You can #define STBI_ASSERT(x)
 *   before the implementation-#include of SDL_stbimage.h to avoid that.
 *   By default stb_image supports HDR images, for that it needs pow() from libm.
 *   If you don't need HDR (it can't be loaded into a SDL_Surface anyway),
 *   #define STBI_NO_LINEAR and #define STBI_NO_HDR before including this header.
 *
 * License:
 *   This software is dual-licensed to the public domain and under the following
 *   license: you are granted a perpetual, irrevocable license to copy, modify,
 *   publish, and distribute this file as you see fit.
 *   No warranty implied; use at your own risk.
 *
 * So you can do whatever you want with this code, including copying it
 * (or parts of it) into your own source.
 * No need to mention me or this "license" in your code or docs, even though
 * it would be appreciated, of course.
 */

#if 0 // Usage Example:
  #define SDL_STBIMAGE_IMPLEMENTATION
  #include "SDL_stbimage.h"

  /**
   * Loads an image file from the specified file path using the STBIMG library and
   * processes it as an SDL_Surface. If the image fails to load, an error message
   * is printed to the console, and the program exits with a status code of 1.
   * The loaded SDL_Surface is freed after processing to avoid memory leaks.
   *
   * @param imageFilePath A null-terminated string representing the path to the
   *                      image file to be loaded. Must not be NULL.
   *
   * @note This function assumes that the STBIMG and SDL libraries are properly
   *       initialized before calling. The program will terminate if the image
   *       cannot be loaded.
   *
   * @warning The caller must ensure that the provided file path is valid and
   *          accessible. Failure to do so will result in program termination.
   */
  void yourFunction(const char* imageFilePath)
  {
      SDL_Surface* surf = STBIMG_Load(imageFilePath);
      if(surf == NULL) {
          printf("ERROR: Couldn't load %s, reason: %s\n", imageFilePath, SDL_GetError());
          exit(1);
      }
  
      // ... do something with surf ...
  
      SDL_FreeSurface(surf);
  }
#endif // 0 (usage example)


#ifndef SDL__STBIMAGE_H
#define SDL__STBIMAGE_H

// if you really think you need <SDL2/SDL.h> here instead.. feel free to change it,
// but the cool kids have path/to/include/SDL2/ in their compilers include path.
#include <SDL.h>

#ifndef SDL_STBIMG_ALLOW_STDIO
  #define STBI_NO_STDIO // don't need STDIO, will use SDL_RWops to open files
#endif
#include "stb_image.h"

// this allows you to prepend stuff to function signatures, e.g. "static"
#ifndef SDL_STBIMG_DEF
  // by default it's empty
  #define SDL_STBIMG_DEF
#endif // DG_MISC_DEF


#ifdef __cplusplus
extern "C" {
#endif

// loads the image file at the given path into a RGB(A) SDL_Surface
// Returns NULL on error, use SDL_GetError() to get more information.
SDL_STBIMG_DEF SDL_Surface* STBIMG_Load(const char* file);

// loads the image file in the given memory buffer into a RGB(A) SDL_Surface
// Returns NULL on error, use SDL_GetError() to get more information.
SDL_STBIMG_DEF SDL_Surface* STBIMG_LoadFromMemory(const unsigned char* buffer, int length);


// Creates an SDL_Surface* using the raw RGB(A) pixelData with given width/height
// (this doesn't use stb_image and is just a simple SDL_CreateSurfaceFrom()-wrapper)
// ! It must be byte-wise 24bit RGB ("888", bytesPerPixel=3) !
// !  or byte-wise 32bit RGBA ("8888", bytesPerPixel=4) data !
// If freeWithSurface is SDL_TRUE, SDL_FreeSurface() will free the pixelData
//  you passed with SDL_free() - NOTE that you should only do that if pixelData
//  was allocated with SDL_malloc(), SDL_calloc() or SDL_realloc()!
// Returns NULL on error (in that case pixelData won't be freed!),
//  use SDL_GetError() to get more information.
SDL_STBIMG_DEF SDL_Surface* STBIMG_CreateSurface(unsigned char* pixelData, int width, int height,
                                                 int bytesPerPixel, SDL_bool freeWithSurface);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SDL__STBIMAGE_H


// ############# Below: Implementation ###############


#ifdef SDL_STBIMAGE_IMPLEMENTATION

// make stb_image use SDL_malloc etc, so SDL_FreeSurface() can SDL_free()
// the data allocated by stb_image
#define STBI_MALLOC SDL_malloc
#define STBI_REALLOC SDL_realloc
#define STBI_FREE SDL_free
#define STB_IMAGE_IMPLEMENTATION
#ifndef SDL_STBIMG_ALLOW_STDIO
  #define STBI_NO_STDIO // don't need STDIO, will use SDL_RWops to open files
#endif
#include "stb_image.h"

typedef struct {
	unsigned char* data;
	int w;
	int h;
	int format; // 3: RGB, 4: RGBA
} STBIMG__image;

/**
 * Creates an SDL_Surface from an STBIMG__image structure.
 *
 * This function takes an STBIMG__image structure and converts it into an SDL_Surface.
 * The STBIMG__image structure contains the raw pixel data, width, height, and format
 * of the image. The function uses SDL_CreateRGBSurfaceFrom() to create the surface,
 * specifying the appropriate masks for the red, green, blue, and alpha channels.
 *
 * If the `origin_has_alpha` parameter is non-zero, the alpha channel mask is set to
 * 0xff000000, otherwise it is set to 0. This allows the function to handle both
 * images with and without an alpha channel.
 *
 * If the `freeWithSurface` parameter is non-zero, the function modifies the SDL_Surface
 * flags to ensure that the pixel data is freed when the surface is destroyed. This is
 * done by clearing the SDL_PREALLOC flag, which indicates that the pixel data is
 * preallocated and should not be freed by SDL_FreeSurface().
 *
 * @param img The STBIMG__image structure containing the image data.
 * @param origin_has_alpha A flag indicating whether the original image has an alpha channel.
 * @param freeWithSurface A flag indicating whether the pixel data should be freed with the surface.
 *
 * @return A pointer to the newly created SDL_Surface, or NULL if the surface could not be created.
 */
static SDL_Surface* STBIMG__CreateSurfaceImpl(STBIMG__image img, int origin_has_alpha, int freeWithSurface)
{
	SDL_Surface* surf = NULL;
	Uint32 rmask, gmask, bmask, amask;
	// ok, the following is pretty stupid.. SDL_CreateRGBSurfaceFrom() pretends to use
	// a void* for the data, but it's really treated as endian-specific Uint32*
	// and there isn't even an SDL_PIXELFORMAT_* for 32bit byte-wise RGBA
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = origin_has_alpha ? 0xff000000 : 0;

	surf = SDL_CreateRGBSurfaceFrom((void*)img.data, img.w, img.h,
	                                img.format*8, img.format*img.w,
	                                rmask, gmask, bmask, amask);

	if(surf == NULL)
	{
		// hopefully SDL_CreateRGBSurfaceFrom() has set an sdl error
		return NULL;
	}

	if(freeWithSurface)
	{
		// SDL_Surface::flags is documented to be read-only.. but if the pixeldata
		// has been allocated with SDL_malloc()/SDL_calloc()/SDL_realloc() this
		// should work (and it currently does) + @icculus said it's reasonably safe:
		//  https://twitter.com/icculus/status/667036586610139137 :-)
		// clear the SDL_PREALLOC flag, so SDL_FreeSurface() free()s the data passed from img.data
		surf->flags &= ~SDL_PREALLOC;
	}

	return surf;
}


/**
 * @brief Loads an image from a memory buffer into an SDL_Surface.
 *
 * This function takes a memory buffer containing image data and its length, and attempts to load
 * the image into an SDL_Surface. The image data is decoded using the stb_image library, and the
 * resulting surface is returned for use in SDL applications.
 *
 * The function first checks if the provided buffer and length are valid. If not, it sets an SDL
 * error and returns NULL. It then retrieves image information (width, height, and format) from
 * the buffer using `stbi_info_from_memory`. If this step fails, an error is set and NULL is returned.
 *
 * Based on the image format, the function determines whether the image has an alpha channel and
 * sets the appropriate bits per pixel (bpp) for decoding. The image is then loaded into memory
 * using `stbi_load_from_memory`. If loading fails, an error is set and NULL is returned.
 *
 * Finally, the function creates an SDL_Surface from the decoded image data using
 * `STBIMG__CreateSurfaceImpl`. If surface creation fails, the allocated image data is freed, and
 * NULL is returned. On success, the SDL_Surface is returned, and the caller is responsible for
 * freeing it using `SDL_FreeSurface`.
 *
 * @param buffer Pointer to the memory buffer containing the image data.
 * @param length Length of the memory buffer in bytes.
 * @return On success, returns a pointer to the newly created SDL_Surface. On failure, returns NULL
 *         and sets an SDL error message that can be retrieved with `SDL_GetError()`.
 */
SDL_STBIMG_DEF SDL_Surface* STBIMG_LoadFromMemory(const unsigned char* buffer, int length)
{
	STBIMG__image img = {0};
	int bppToUse = 0;
	int inforet = 0;
	SDL_Surface* ret = NULL;
	int origin_has_alpha;

	if(buffer == NULL)
	{
		SDL_SetError("STBIMG_LoadFromMemory(): passed buffer was NULL!");
		return NULL;
	}
	if(length <= 0)
	{
		SDL_SetError("STBIMG_LoadFromMemory(): passed invalid length: %d!", length);
		return NULL;
	}

	inforet = stbi_info_from_memory(buffer, length, &img.w, &img.h, &img.format);
	if(!inforet)
	{
		SDL_SetError("STBIMG_LoadFromMemory(): Couldn't get image info: %s!\n", stbi_failure_reason());
		return NULL;
	}

	// no alpha => use RGB, else use RGBA
	origin_has_alpha = !(img.format == STBI_grey || img.format == STBI_rgb);
	bppToUse = STBI_rgb_alpha;

	img.data = stbi_load_from_memory(buffer, length, &img.w, &img.h, &img.format, bppToUse);
	if(img.data == NULL)
	{
		SDL_SetError("STBIMG_LoadFromMemory(): Couldn't load image: %s!\n", stbi_failure_reason());
		return NULL;
	}
	img.format = bppToUse;

	ret = STBIMG__CreateSurfaceImpl(img, origin_has_alpha, 1);

	if(ret == NULL)
	{
		// no need to log an error here, it was an SDL error which should still be available through SDL_GetError()
		SDL_free(img.data);
		return NULL;
	}

	return ret;
}


/**
 * @brief Creates an SDL_Surface from raw pixel data.
 *
 * This function takes raw pixel data and creates an SDL_Surface object from it.
 * The pixel data must be in either 24-bit RGB (3 bytes per pixel) or 32-bit RGBA
 * (4 bytes per pixel) format. The function performs validation on the input parameters
 * and returns NULL if any of the checks fail, setting an appropriate error message
 * using SDL_SetError.
 *
 * @param pixelData A pointer to the raw pixel data. Must not be NULL.
 * @param width The width of the image in pixels. Must be greater than 0.
 * @param height The height of the image in pixels. Must be greater than 0.
 * @param bytesPerPixel The number of bytes per pixel. Must be either 3 (for RGB) or 4 (for RGBA).
 * @param freeWithSurface A boolean flag indicating whether the pixel data should be freed
 *                        when the surface is destroyed. Use SDL_TRUE to free the data, or
 *                        SDL_FALSE to leave it untouched.
 *
 * @return A pointer to the newly created SDL_Surface on success, or NULL on failure.
 *         If NULL is returned, an error message can be retrieved using SDL_GetError().
 */
SDL_STBIMG_DEF SDL_Surface* STBIMG_CreateSurface(unsigned char* pixelData, int width, int height, int bytesPerPixel, SDL_bool freeWithSurface)
{
	STBIMG__image img;

	if(pixelData == NULL)
	{
		SDL_SetError("STBIMG_CreateSurface(): passed pixelData was NULL!");
		return NULL;
	}
	if(bytesPerPixel != 3 && bytesPerPixel != 4)
	{
		SDL_SetError("STBIMG_CreateSurface(): passed bytesPerPixel = %d, only 3 (24bit RGB) and 4 (32bit RGBA) are allowed!", bytesPerPixel);
		return NULL;
	}
	if(width <= 0 || height <= 0)
	{
		SDL_SetError("STBIMG_CreateSurface(): width and height must be > 0!");
		return NULL;
	}

	img.data = pixelData;
	img.w = width;
	img.h = height;
	img.format = bytesPerPixel;

	return STBIMG__CreateSurfaceImpl(img, img.format == 4, freeWithSurface);
}

#endif // SDL_STBIMAGE_IMPLEMENTATION
