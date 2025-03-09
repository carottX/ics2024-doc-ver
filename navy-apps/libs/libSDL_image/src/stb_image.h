/* stb_image - v2.26 - public domain image loader - http://nothings.org/stb
                                  no warranty implied; use at your own risk

   Do this:
      #define STB_IMAGE_IMPLEMENTATION
   before you include this file in *one* C or C++ file to create the implementation.

   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define STB_IMAGE_IMPLEMENTATION
   #include "stb_image.h"

   You can #define STBI_ASSERT(x) before the #include to avoid using assert.h.
   And #define STBI_MALLOC, STBI_REALLOC, and STBI_FREE to avoid using malloc,realloc,free


   QUICK NOTES:
      Primarily of interest to game developers and other people who can
          avoid problematic images and only need the trivial interface

      JPEG baseline & progressive (12 bpc/arithmetic not supported, same as stock IJG lib)
      PNG 1/2/4/8/16-bit-per-channel

      TGA (not sure what subset, if a subset)
      BMP non-1bpp, non-RLE
      PSD (composited view only, no extra channels, 8/16 bit-per-channel)

      GIF (*comp always reports as 4-channel)
      HDR (radiance rgbE format)
      PIC (Softimage PIC)
      PNM (PPM and PGM binary only)

      Animated GIF still needs a proper API, but here's one way to do it:
          http://gist.github.com/urraka/685d9a6340b26b830d49

      - decode from memory or through FILE (define STBI_NO_STDIO to remove code)
      - decode from arbitrary I/O callbacks
      - SIMD acceleration on x86/x64 (SSE2) and ARM (NEON)

   Full documentation under "DOCUMENTATION" below.


LICENSE

  See end of file for license information.

RECENT REVISION HISTORY:

      2.26  (2020-07-13) many minor fixes
      2.25  (2020-02-02) fix warnings
      2.24  (2020-02-02) fix warnings; thread-local failure_reason and flip_vertically
      2.23  (2019-08-11) fix clang static analysis warning
      2.22  (2019-03-04) gif fixes, fix warnings
      2.21  (2019-02-25) fix typo in comment
      2.20  (2019-02-07) support utf8 filenames in Windows; fix warnings and platform ifdefs
      2.19  (2018-02-11) fix warning
      2.18  (2018-01-30) fix warnings
      2.17  (2018-01-29) bugfix, 1-bit BMP, 16-bitness query, fix warnings
      2.16  (2017-07-23) all functions have 16-bit variants; optimizations; bugfixes
      2.15  (2017-03-18) fix png-1,2,4; all Imagenet JPGs; no runtime SSE detection on GCC
      2.14  (2017-03-03) remove deprecated STBI_JPEG_OLD; fixes for Imagenet JPGs
      2.13  (2016-12-04) experimental 16-bit API, only for PNG so far; fixes
      2.12  (2016-04-02) fix typo in 2.11 PSD fix that caused crashes
      2.11  (2016-04-02) 16-bit PNGS; enable SSE2 in non-gcc x64
                         RGB-format JPEG; remove white matting in PSD;
                         allocate large structures on the stack;
                         correct channel count for PNG & BMP
      2.10  (2016-01-22) avoid warning introduced in 2.09
      2.09  (2016-01-16) 16-bit TGA; comments in PNM files; STBI_REALLOC_SIZED

   See end of file for full revision history.


 ============================    Contributors    =========================

 Image formats                          Extensions, features
    Sean Barrett (jpeg, png, bmp)          Jetro Lauha (stbi_info)
    Nicolas Schulz (hdr, psd)              Martin "SpartanJ" Golini (stbi_info)
    Jonathan Dummer (tga)                  James "moose2000" Brown (iPhone PNG)
    Jean-Marc Lienher (gif)                Ben "Disch" Wenger (io callbacks)
    Tom Seddon (pic)                       Omar Cornut (1/2/4-bit PNG)
    Thatcher Ulrich (psd)                  Nicolas Guillemot (vertical flip)
    Ken Miller (pgm, ppm)                  Richard Mitton (16-bit PSD)
    github:urraka (animated gif)           Junggon Kim (PNM comments)
    Christopher Forseth (animated gif)     Daniel Gibson (16-bit TGA)
                                           socks-the-fox (16-bit PNG)
                                           Jeremy Sawicki (handle all ImageNet JPGs)
 Optimizations & bugfixes                  Mikhail Morozov (1-bit BMP)
    Fabian "ryg" Giesen                    Anael Seghezzi (is-16-bit query)
    Arseny Kapoulkine
    John-Mark Allen
    Carmelo J Fdez-Aguera

 Bug & warning fixes
    Marc LeBlanc            David Woo          Guillaume George     Martins Mozeiko
    Christpher Lloyd        Jerry Jansson      Joseph Thomson       Blazej Dariusz Roszkowski
    Phil Jordan                                Dave Moore           Roy Eltham
    Hayaki Saito            Nathan Reed        Won Chun
    Luke Graham             Johan Duparc       Nick Verigakis       the Horde3D community
    Thomas Ruf              Ronny Chevalier                         github:rlyeh
    Janez Zemva             John Bartholomew   Michal Cichon        github:romigrou
    Jonathan Blow           Ken Hamada         Tero Hanninen        github:svdijk
                            Laurent Gomila     Cort Stratton        github:snagar
    Aruelien Pocheville     Sergio Gonzalez    Thibault Reuille     github:Zelex
    Cass Everitt            Ryamond Barbiero                        github:grim210
    Paul Du Bois            Engin Manap        Aldo Culquicondor    github:sammyhw
    Philipp Wiesemann       Dale Weiler        Oriol Ferrer Mesia   github:phprus
    Josh Tobin                                 Matthew Gregan       github:poppolopoppo
    Julian Raschke          Gregory Mullen     Christian Floisand   github:darealshinji
    Baldur Karlsson         Kevin Schmidt      JR Smith             github:Michaelangel007
                            Brad Weinberger    Matvey Cherevko      [reserved]
    Luca Sas                Alexander Veselov  Zack Middleton       [reserved]
    Ryan C. Gordon          [reserved]                              [reserved]
                     DO NOT ADD YOUR NAME HERE

  To add your name to the credits, pick a random blank space in the middle and fill it.
  80% of merge conflicts on stb PRs are due to people adding their name at the end
  of the credits.
*/

#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STBI_INCLUDE_STB_IMAGE_H

// DOCUMENTATION
//
// Limitations:
//    - no 12-bit-per-channel JPEG
//    - no JPEGs with arithmetic coding
//    - GIF always returns *comp=4
//
// Basic usage (see HDR discussion below for HDR usage):
//    int x,y,n;
//    unsigned char *data = stbi_load(filename, &x, &y, &n, 0);
//    // ... process data if not NULL ...
//    // ... x = width, y = height, n = # 8-bit components per pixel ...
//    // ... replace '0' with '1'..'4' to force that many components per pixel
//    // ... but 'n' will always be the number that it would have been if you said 0
//    stbi_image_free(data)
//
// Standard parameters:
//    int *x                 -- outputs image width in pixels
//    int *y                 -- outputs image height in pixels
//    int *channels_in_file  -- outputs # of image components in image file
//    int desired_channels   -- if non-zero, # of image components requested in result
//
// The return value from an image loader is an 'unsigned char *' which points
// to the pixel data, or NULL on an allocation failure or if the image is
// corrupt or invalid. The pixel data consists of *y scanlines of *x pixels,
// with each pixel consisting of N interleaved 8-bit components; the first
// pixel pointed to is top-left-most in the image. There is no padding between
// image scanlines or between pixels, regardless of format. The number of
// components N is 'desired_channels' if desired_channels is non-zero, or
// *channels_in_file otherwise. If desired_channels is non-zero,
// *channels_in_file has the number of components that _would_ have been
// output otherwise. E.g. if you set desired_channels to 4, you will always
// get RGBA output, but you can check *channels_in_file to see if it's trivially
// opaque because e.g. there were only 3 channels in the source image.
//
// An output image with N components has the following components interleaved
// in this order in each pixel:
//
//     N=#comp     components
//       1           grey
//       2           grey, alpha
//       3           red, green, blue
//       4           red, green, blue, alpha
//
// If image loading fails for any reason, the return value will be NULL,
// and *x, *y, *channels_in_file will be unchanged. The function
// stbi_failure_reason() can be queried for an extremely brief, end-user
// unfriendly explanation of why the load failed. Define STBI_NO_FAILURE_STRINGS
// to avoid compiling these strings at all, and STBI_FAILURE_USERMSG to get slightly
// more user-friendly ones.
//
// Paletted PNG, BMP, GIF, and PIC images are automatically depalettized.
//
// ===========================================================================
//
// UNICODE:
//
//   If compiling for Windows and you wish to use Unicode filenames, compile
//   with
//       #define STBI_WINDOWS_UTF8
//   and pass utf8-encoded filenames. Call stbi_convert_wchar_to_utf8 to convert
//   Windows wchar_t filenames to utf8.
//
// ===========================================================================
//
// Philosophy
//
// stb libraries are designed with the following priorities:
//
//    1. easy to use
//    2. easy to maintain
//    3. good performance
//
// Sometimes I let "good performance" creep up in priority over "easy to maintain",
// and for best performance I may provide less-easy-to-use APIs that give higher
// performance, in addition to the easy-to-use ones. Nevertheless, it's important
// to keep in mind that from the standpoint of you, a client of this library,
// all you care about is #1 and #3, and stb libraries DO NOT emphasize #3 above all.
//
// Some secondary priorities arise directly from the first two, some of which
// provide more explicit reasons why performance can't be emphasized.
//
//    - Portable ("ease of use")
//    - Small source code footprint ("easy to maintain")
//    - No dependencies ("ease of use")
//
// ===========================================================================
//
// I/O callbacks
//
// I/O callbacks allow you to read from arbitrary sources, like packaged
// files or some other source. Data read from callbacks are processed
// through a small internal buffer (currently 128 bytes) to try to reduce
// overhead.
//
// The three functions you must define are "read" (reads some bytes of data),
// "skip" (skips some bytes of data), "eof" (reports if the stream is at the end).
//
// ===========================================================================
//
// SIMD support
//
// The JPEG decoder will try to automatically use SIMD kernels on x86 when
// supported by the compiler. For ARM Neon support, you must explicitly
// request it.
//
// (The old do-it-yourself SIMD API is no longer supported in the current
// code.)
//
// On x86, SSE2 will automatically be used when available based on a run-time
// test; if not, the generic C versions are used as a fall-back. On ARM targets,
// the typical path is to have separate builds for NEON and non-NEON devices
// (at least this is true for iOS and Android). Therefore, the NEON support is
// toggled by a build flag: define STBI_NEON to get NEON loops.
//
// If for some reason you do not want to use any of SIMD code, or if
// you have issues compiling it, you can disable it entirely by
// defining STBI_NO_SIMD.
//
// ===========================================================================
//
// HDR image support   (disable by defining STBI_NO_HDR)
//
// stb_image supports loading HDR images in general, and currently the Radiance
// .HDR file format specifically. You can still load any file through the existing
// interface; if you attempt to load an HDR file, it will be automatically remapped
// to LDR, assuming gamma 2.2 and an arbitrary scale factor defaulting to 1;
// both of these constants can be reconfigured through this interface:
//
//     stbi_hdr_to_ldr_gamma(2.2f);
//     stbi_hdr_to_ldr_scale(1.0f);
//
// (note, do not use _inverse_ constants; stbi_image will invert them
// appropriately).
//
// Additionally, there is a new, parallel interface for loading files as
// (linear) floats to preserve the full dynamic range:
//
//    float *data = stbi_loadf(filename, &x, &y, &n, 0);
//
// If you load LDR images through this interface, those images will
// be promoted to floating point values, run through the inverse of
// constants corresponding to the above:
//
//     stbi_ldr_to_hdr_scale(1.0f);
//     stbi_ldr_to_hdr_gamma(2.2f);
//
// Finally, given a filename (or an open file or memory block--see header
// file for details) containing image data, you can query for the "most
// appropriate" interface to use (that is, whether the image is HDR or
// not), using:
//
//     stbi_is_hdr(char *filename);
//
// ===========================================================================
//
// iPhone PNG support:
//
// By default we convert iphone-formatted PNGs back to RGB, even though
// they are internally encoded differently. You can disable this conversion
// by calling stbi_convert_iphone_png_to_rgb(0), in which case
// you will always just get the native iphone "format" through (which
// is BGR stored in RGB).
//
// Call stbi_set_unpremultiply_on_load(1) as well to force a divide per
// pixel to remove any premultiplied alpha *only* if the image file explicitly
// says there's premultiplied data (currently only happens in iPhone images,
// and only if iPhone convert-to-rgb processing is on).
//
// ===========================================================================
//
// ADDITIONAL CONFIGURATION
//
//  - You can suppress implementation of any of the decoders to reduce
//    your code footprint by #defining one or more of the following
//    symbols before creating the implementation.
//
//        STBI_NO_JPEG
//        STBI_NO_PNG
//        STBI_NO_BMP
//        STBI_NO_PSD
//        STBI_NO_TGA
//        STBI_NO_GIF
//        STBI_NO_HDR
//        STBI_NO_PIC
//        STBI_NO_PNM   (.ppm and .pgm)
//
//  - You can request *only* certain decoders and suppress all other ones
//    (this will be more forward-compatible, as addition of new decoders
//    doesn't require you to disable them explicitly):
//
//        STBI_ONLY_JPEG
//        STBI_ONLY_PNG
//        STBI_ONLY_BMP
//        STBI_ONLY_PSD
//        STBI_ONLY_TGA
//        STBI_ONLY_GIF
//        STBI_ONLY_HDR
//        STBI_ONLY_PIC
//        STBI_ONLY_PNM   (.ppm and .pgm)
//
//   - If you use STBI_NO_PNG (or _ONLY_ without PNG), and you still
//     want the zlib decoder to be available, #define STBI_SUPPORT_ZLIB
//
//  - If you define STBI_MAX_DIMENSIONS, stb_image will reject images greater
//    than that size (in either width or height) without further processing.
//    This is to let programs in the wild set an upper bound to prevent
//    denial-of-service attacks on untrusted data, as one could generate a
//    valid image of gigantic dimensions and force stb_image to allocate a
//    huge block of memory and spend disproportionate time decoding it. By
//    default this is set to (1 << 24), which is 16777216, but that's still
//    very big.


#define STBI_VERSION 1

enum
{
   STBI_default = 0, // only used for desired_channels

   STBI_grey       = 1,
   STBI_grey_alpha = 2,
   STBI_rgb        = 3,
   STBI_rgb_alpha  = 4
};

#include <stdlib.h>
typedef unsigned char stbi_uc;
typedef unsigned short stbi_us;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STBIDEF
#ifdef STB_IMAGE_STATIC
#define STBIDEF static
#else
#define STBIDEF extern
#endif
#endif

//////////////////////////////////////////////////////////////////////////////
//
// PRIMARY API - works on images of any type
//

////////////////////////////////////
//
// 8-bits-per-channel interface
//

STBIDEF stbi_uc *stbi_load_from_memory   (stbi_uc           const *buffer, int len   , int *x, int *y, int *channels_in_file, int desired_channels);


#ifndef STBI_NO_GIF
STBIDEF stbi_uc *stbi_load_gif_from_memory(stbi_uc const *buffer, int len, int **delays, int *x, int *y, int *z, int *comp, int req_comp);
#endif


////////////////////////////////////
//
// 16-bits-per-channel interface
//

STBIDEF stbi_us *stbi_load_16_from_memory   (stbi_uc const *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels);


////////////////////////////////////
//
// float-per-channel interface
//



// stbi_is_hdr is always defined, but always returns false if STBI_NO_HDR
STBIDEF int    stbi_is_hdr_from_memory(stbi_uc const *buffer, int len);


// get a VERY brief reason for failure
// on most compilers (and ALL modern mainstream compilers) this is threadsafe
STBIDEF const char *stbi_failure_reason  (void);

// free the loaded image -- this is just free()
STBIDEF void     stbi_image_free      (void *retval_from_stbi_load);

// get image dimensions & components without fully decoding
STBIDEF int      stbi_info_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp);
STBIDEF int      stbi_is_16_bit_from_memory(stbi_uc const *buffer, int len);




// for image formats that explicitly notate that they have premultiplied alpha,
// we just return the colors as stored in the file. set this flag to force
// unpremultiplication. results are undefined if the unpremultiply overflow.
STBIDEF void stbi_set_unpremultiply_on_load(int flag_true_if_should_unpremultiply);

// indicate whether we should process iphone images back to canonical format,
// or just pass them through "as-is"
STBIDEF void stbi_convert_iphone_png_to_rgb(int flag_true_if_should_convert);

// flip the image vertically, so the first pixel in the output array is the bottom left
STBIDEF void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip);

// as above, but only applies to images loaded on the thread that calls the function
// this function is only available if your compiler supports thread-local variables;
// calling it will fail to link if your compiler doesn't
STBIDEF void stbi_set_flip_vertically_on_load_thread(int flag_true_if_should_flip);

// ZLIB client - used by PNG, available for other purposes

STBIDEF char *stbi_zlib_decode_malloc_guesssize(const char *buffer, int len, int initial_size, int *outlen);
STBIDEF char *stbi_zlib_decode_malloc_guesssize_headerflag(const char *buffer, int len, int initial_size, int *outlen, int parse_header);
STBIDEF char *stbi_zlib_decode_malloc(const char *buffer, int len, int *outlen);
STBIDEF int   stbi_zlib_decode_buffer(char *obuffer, int olen, const char *ibuffer, int ilen);

STBIDEF char *stbi_zlib_decode_noheader_malloc(const char *buffer, int len, int *outlen);
STBIDEF int   stbi_zlib_decode_noheader_buffer(char *obuffer, int olen, const char *ibuffer, int ilen);


#ifdef __cplusplus
}
#endif

//
//
////   end header file   /////////////////////////////////////////////////////
#endif // STBI_INCLUDE_STB_IMAGE_H

#ifdef STB_IMAGE_IMPLEMENTATION

#if defined(STBI_ONLY_JPEG) || defined(STBI_ONLY_PNG) || defined(STBI_ONLY_BMP) \
  || defined(STBI_ONLY_TGA) || defined(STBI_ONLY_GIF) || defined(STBI_ONLY_PSD) \
  || defined(STBI_ONLY_HDR) || defined(STBI_ONLY_PIC) || defined(STBI_ONLY_PNM) \
  || defined(STBI_ONLY_ZLIB)
   #ifndef STBI_ONLY_JPEG
   #define STBI_NO_JPEG
   #endif
   #ifndef STBI_ONLY_PNG
   #define STBI_NO_PNG
   #endif
   #ifndef STBI_ONLY_BMP
   #define STBI_NO_BMP
   #endif
   #ifndef STBI_ONLY_PSD
   #define STBI_NO_PSD
   #endif
   #ifndef STBI_ONLY_TGA
   #define STBI_NO_TGA
   #endif
   #ifndef STBI_ONLY_GIF
   #define STBI_NO_GIF
   #endif
   #ifndef STBI_ONLY_HDR
   #define STBI_NO_HDR
   #endif
   #ifndef STBI_ONLY_PIC
   #define STBI_NO_PIC
   #endif
   #ifndef STBI_ONLY_PNM
   #define STBI_NO_PNM
   #endif
#endif

#if defined(STBI_NO_PNG) && !defined(STBI_SUPPORT_ZLIB) && !defined(STBI_NO_ZLIB)
#define STBI_NO_ZLIB
#endif


#include <stdarg.h>
#include <stddef.h> // ptrdiff_t on osx
#include <stdlib.h>
#include <string.h>
#include <limits.h>



#ifndef STBI_ASSERT
#include <assert.h>
#define STBI_ASSERT(x) assert(x)
#endif

#ifdef __cplusplus
#define STBI_EXTERN extern "C"
#else
#define STBI_EXTERN extern
#endif


   #ifdef __cplusplus
   #define stbi_inline inline
   #else
   #define stbi_inline
   #endif


#include <stdint.h>
typedef uint16_t stbi__uint16;
typedef int16_t  stbi__int16;
typedef uint32_t stbi__uint32;
typedef int32_t  stbi__int32;

// should produce compiler error if size is wrong
typedef unsigned char validate_uint32[sizeof(stbi__uint32)==4 ? 1 : -1];

#define STBI_NOTUSED(v)  (void)sizeof(v)

#if defined(STBI_MALLOC) && defined(STBI_FREE) && (defined(STBI_REALLOC) || defined(STBI_REALLOC_SIZED))
// ok
#elif !defined(STBI_MALLOC) && !defined(STBI_FREE) && !defined(STBI_REALLOC) && !defined(STBI_REALLOC_SIZED)
// ok
#else
#error "Must define all or none of STBI_MALLOC, STBI_FREE, and STBI_REALLOC (or STBI_REALLOC_SIZED)."
#endif

#ifndef STBI_MALLOC
#define STBI_MALLOC(sz)           malloc(sz)
#define STBI_REALLOC(p,newsz)     realloc(p,newsz)
#define STBI_FREE(p)              free(p)
#endif

#ifndef STBI_REALLOC_SIZED
#define STBI_REALLOC_SIZED(p,oldsz,newsz) STBI_REALLOC(p,newsz)
#endif

#define STBI_SIMD_ALIGN(type, name) type name

#ifndef STBI_MAX_DIMENSIONS
#define STBI_MAX_DIMENSIONS (1 << 24)
#endif

///////////////////////////////////////////////
//
//  stbi__context struct and start_xxx functions

// stbi__context structure is our basic context used by all images, so it
// contains all the IO context, plus some basic image information
typedef struct
{
   stbi__uint32 img_x, img_y;
   int img_n, img_out_n;

   int buflen;

   stbi_uc *img_buffer, *img_buffer_end;
   stbi_uc *img_buffer_original, *img_buffer_original_end;
} stbi__context;


// initialize a memory-decode context
static void stbi__start_mem(stbi__context *s, stbi_uc const *buffer, int len)
{
   s->img_buffer = s->img_buffer_original = (stbi_uc *) buffer;
   s->img_buffer_end = s->img_buffer_original_end = (stbi_uc *) buffer+len;
}

/**
 * Rewinds the stream context to the beginning of the initial buffer.
 *
 * This function resets the `img_buffer` and `img_buffer_end` pointers in the
 * `stbi__context` structure to their original values (`img_buffer_original` and
 * `img_buffer_original_end`). Conceptually, this operation is intended to rewind
 * the stream to its beginning. However, in practice, it only rewinds to the
 * start of the initial buffer. This is because the function is typically used
 * after performing a 'test' operation, which only examines the first 92 bytes
 * of the stream.
 *
 * @param s Pointer to the `stbi__context` structure to be rewound.
 */
static void stbi__rewind(stbi__context *s)
{
   // conceptually rewind SHOULD rewind to the beginning of the stream,
   // but we just rewind to the beginning of the initial buffer, because
   // we only use it after doing 'test', which only ever looks at at most 92 bytes
   s->img_buffer = s->img_buffer_original;
   s->img_buffer_end = s->img_buffer_original_end;
}

enum
{
   STBI_ORDER_RGB,
   STBI_ORDER_BGR
};

typedef struct
{
   int bits_per_channel;
   int num_channels;
   int channel_order;
} stbi__result_info;

#ifndef STBI_NO_JPEG
static int      stbi__jpeg_test(stbi__context *s);
static void    *stbi__jpeg_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__jpeg_info(stbi__context *s, int *x, int *y, int *comp);
#endif

#ifndef STBI_NO_PNG
static int      stbi__png_test(stbi__context *s);
static void    *stbi__png_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__png_info(stbi__context *s, int *x, int *y, int *comp);
static int      stbi__png_is16(stbi__context *s);
#endif

#ifndef STBI_NO_BMP
static int      stbi__bmp_test(stbi__context *s);
static void    *stbi__bmp_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__bmp_info(stbi__context *s, int *x, int *y, int *comp);
#endif

#ifndef STBI_NO_TGA
static int      stbi__tga_test(stbi__context *s);
static void    *stbi__tga_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__tga_info(stbi__context *s, int *x, int *y, int *comp);
#endif




#ifndef STBI_NO_GIF
static int      stbi__gif_test(stbi__context *s);
static void    *stbi__gif_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static void    *stbi__load_gif_main(stbi__context *s, int **delays, int *x, int *y, int *z, int *comp, int req_comp);
static int      stbi__gif_info(stbi__context *s, int *x, int *y, int *comp);
#endif

#ifndef STBI_NO_PNM
static int      stbi__pnm_test(stbi__context *s);
static void    *stbi__pnm_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri);
static int      stbi__pnm_info(stbi__context *s, int *x, int *y, int *comp);
#endif

static
const char *stbi__g_failure_reason;

/**
 * @brief Retrieves the last error message generated by the STB image library.
 *
 * This function returns a pointer to a string that describes the most recent error
 * that occurred during an image loading or processing operation. The error message
 * is stored in a global variable and is updated whenever an error is encountered.
 * If no error has occurred, the returned string may be NULL or contain an empty string.
 *
 * @return A pointer to a null-terminated string containing the error message.
 *         The string is owned by the STB image library and should not be freed.
 */
STBIDEF const char *stbi_failure_reason(void)
{
   return stbi__g_failure_reason;
}

#ifndef STBI_NO_FAILURE_STRINGS
/**
 * Sets the global failure reason string and returns 0 to indicate an error.
 *
 * This function is used to handle error conditions by storing a descriptive
 * error message in the global variable `stbi__g_failure_reason`. It then
 * returns 0, which typically signifies a failure in the context of the
 * calling function.
 *
 * @param str A string describing the error that occurred. This string is
 *            stored in the global failure reason variable.
 *
 * @return Always returns 0 to indicate an error condition.
 */
static int stbi__err(const char *str)
{
   stbi__g_failure_reason = str;
   return 0;
}
#endif

/**
 * Allocates a block of memory of the specified size.
 *
 * This function is a wrapper around the `STBI_MALLOC` macro, which is used to allocate
 * memory dynamically. It is typically used internally by the STB image library for
 * memory management. The allocated memory is not initialized.
 *
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory block, or NULL if the allocation fails.
 */
static void *stbi__malloc(size_t size)
{
    return STBI_MALLOC(size);
}

// stb_image uses ints pervasively, including for offset calculations.
// therefore the largest decoded image size we can support with the
// current code, even on 64-bit targets, is INT_MAX. this is not a
// significant limitation for the intended use case.
//
// we do, however, need to make sure our size calculations don't
// overflow. hence a few helper functions for size calculations that
// multiply integers together, making sure that they're non-negative
// and no overflow occurs.

// return 1 if the sum is valid, 0 on overflow.
// negative terms are considered invalid.
static int stbi__addsizes_valid(int a, int b)
{
   if (b < 0) return 0;
   // now 0 <= b <= INT_MAX, hence also
   // 0 <= INT_MAX - b <= INTMAX.
   // And "a + b <= INT_MAX" (which might overflow) is the
   // same as a <= INT_MAX - b (no overflow)
   return a <= INT_MAX - b;
}

// returns 1 if the product is valid, 0 on overflow.
// negative factors are considered invalid.
static int stbi__mul2sizes_valid(int a, int b)
{
   if (a < 0 || b < 0) return 0;
   if (b == 0) return 1; // mul-by-0 is always safe
   // portable way to check for no overflows in a*b
   return a <= INT_MAX/b;
}

#if !defined(STBI_NO_JPEG) || !defined(STBI_NO_PNG) || !defined(STBI_NO_TGA) || !defined(STBI_NO_HDR)
// returns 1 if "a*b + add" has no negative terms/factors and doesn't overflow
static int stbi__mad2sizes_valid(int a, int b, int add)
{
   return stbi__mul2sizes_valid(a, b) && stbi__addsizes_valid(a*b, add);
}
#endif

// returns 1 if "a*b*c + add" has no negative terms/factors and doesn't overflow
static int stbi__mad3sizes_valid(int a, int b, int c, int add)
{
   return stbi__mul2sizes_valid(a, b) && stbi__mul2sizes_valid(a*b, c) &&
      stbi__addsizes_valid(a*b*c, add);
}

// returns 1 if "a*b*c*d + add" has no negative terms/factors and doesn't overflow

#if !defined(STBI_NO_JPEG) || !defined(STBI_NO_PNG) || !defined(STBI_NO_TGA) || !defined(STBI_NO_HDR)
// mallocs with size overflow checking
static void *stbi__malloc_mad2(int a, int b, int add)
{
   if (!stbi__mad2sizes_valid(a, b, add)) return NULL;
   return stbi__malloc(a*b + add);
}
#endif

/**
 * Allocates memory for a 3-dimensional array with an additional buffer.
 *
 * This function calculates the total size required for a 3D array with dimensions
 * `a`, `b`, and `c`, and adds an extra buffer of size `add`. It first checks if the
 * sizes are valid using `stbi__mad3sizes_valid`. If the sizes are valid, it allocates
 * memory of size `a * b * c + add` using `stbi__malloc`. If the sizes are invalid or
 * memory allocation fails, it returns `NULL`.
 *
 * @param a The size of the first dimension.
 * @param b The size of the second dimension.
 * @param c The size of the third dimension.
 * @param add The additional buffer size to allocate.
 * @return A pointer to the allocated memory, or `NULL` if the sizes are invalid or
 *         memory allocation fails.
 */
static void *stbi__malloc_mad3(int a, int b, int c, int add)
{
   if (!stbi__mad3sizes_valid(a, b, c, add)) return NULL;
   return stbi__malloc(a*b*c + add);
}


// stbi__err - error
// stbi__errpf - error returning pointer to float
// stbi__errpuc - error returning pointer to unsigned char

#ifdef STBI_NO_FAILURE_STRINGS
   #define stbi__err(x,y)  0
#elif defined(STBI_FAILURE_USERMSG)
   #define stbi__err(x,y)  stbi__err(y)
#else
   #define stbi__err(x,y)  stbi__err(x)
#endif

#define stbi__errpf(x,y)   ((float *)(size_t) (stbi__err(x,y)?NULL:NULL))
#define stbi__errpuc(x,y)  ((unsigned char *)(size_t) (stbi__err(x,y)?NULL:NULL))

/**
 * @brief Frees the memory allocated for an image loaded by `stbi_load`.
 *
 * This function is used to release the memory allocated by the `stbi_load` function
 * or other similar functions from the stb_image library. It is essential to call this
 * function to avoid memory leaks when the image data is no longer needed.
 *
 * @param retval_from_stbi_load A pointer to the image data returned by `stbi_load`.
 *                              This pointer must have been allocated by the stb_image
 *                              library. If `NULL` is passed, the function does nothing.
 *
 * @note The function uses the `STBI_FREE` macro to free the memory, which is typically
 *       defined as `free` in most environments but can be customized if needed.
 */
STBIDEF void stbi_image_free(void *retval_from_stbi_load)
{
   STBI_FREE(retval_from_stbi_load);
}



static int stbi__vertically_flip_on_load_global = 0;

/**
 * @brief Sets whether images should be flipped vertically when loaded.
 *
 * This function configures the global behavior of the STB image library when loading images.
 * When the flag is set to a non-zero value, images will be flipped vertically upon loading.
 * This is useful for correcting the orientation of images that are stored with the origin
 * at the bottom-left corner (e.g., OpenGL textures). By default, this behavior is disabled.
 *
 * @param flag_true_if_should_flip A non-zero value to enable vertical flipping, or zero to disable it.
 */
STBIDEF void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip)
{
   stbi__vertically_flip_on_load_global = flag_true_if_should_flip;
}

#define stbi__vertically_flip_on_load  stbi__vertically_flip_on_load_global

/**
 * @brief Loads an image from the given context and populates the provided parameters.
 *
 * This function attempts to identify the image format from the given context `s` and
 * delegates the loading to the appropriate format-specific loader. It supports multiple
 * image formats, including JPEG, PNG, BMP, GIF, PNM, and TGA. The function initializes
 * the `stbi__result_info` structure `ri` with default values and updates it based on
 * the loaded image properties.
 *
 * @param s The context from which the image data is read.
 * @param x Pointer to store the width of the loaded image.
 * @param y Pointer to store the height of the loaded image.
 * @param comp Pointer to store the number of components (channels) in the loaded image.
 * @param req_comp The desired number of components in the output image. If 0, the
 *                 original number of components is preserved.
 * @param ri Pointer to an `stbi__result_info` structure to store additional metadata
 *           about the loaded image, such as bits per channel and channel order.
 * @param bpc Bits per channel (unused in this implementation).
 * @return A pointer to the loaded image data on success, or NULL on failure. The caller
 *         is responsible for freeing the returned memory using `stbi_image_free`.
 */
static void *stbi__load_main(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri, int bpc)
{
   memset(ri, 0, sizeof(*ri)); // make sure it's initialized if we add new fields
   ri->bits_per_channel = 8; // default is 8 so most paths don't have to be changed
   ri->channel_order = STBI_ORDER_RGB; // all current input & output are this, but this is here so we can add BGR order
   ri->num_channels = 0;

   #ifndef STBI_NO_JPEG
   if (stbi__jpeg_test(s)) return stbi__jpeg_load(s,x,y,comp,req_comp, ri);
   #endif
   #ifndef STBI_NO_PNG
   if (stbi__png_test(s))  return stbi__png_load(s,x,y,comp,req_comp, ri);
   #endif
   #ifndef STBI_NO_BMP
   if (stbi__bmp_test(s))  return stbi__bmp_load(s,x,y,comp,req_comp, ri);
   #endif
   #ifndef STBI_NO_GIF
   if (stbi__gif_test(s))  return stbi__gif_load(s,x,y,comp,req_comp, ri);
   #endif
   STBI_NOTUSED(bpc);
   #ifndef STBI_NO_PNM
   if (stbi__pnm_test(s))  return stbi__pnm_load(s,x,y,comp,req_comp, ri);
   #endif


   #ifndef STBI_NO_TGA
   // test tga last because it's a crappy test!
   if (stbi__tga_test(s))
      return stbi__tga_load(s,x,y,comp,req_comp, ri);
   #endif

   return stbi__errpuc("unknown image type", "Image not of any known type, or corrupt");
}

/**
 * Converts a 16-bit image buffer to an 8-bit image buffer.
 *
 * This function takes a 16-bit image buffer and reduces it to an 8-bit image buffer
 * by discarding the lower 8 bits of each 16-bit value. The resulting 8-bit values
 * are stored in a newly allocated buffer, and the original 16-bit buffer is freed.
 *
 * @param orig     Pointer to the original 16-bit image buffer.
 * @param w        Width of the image in pixels.
 * @param h        Height of the image in pixels.
 * @param channels Number of color channels in the image (e.g., 3 for RGB, 4 for RGBA).
 *
 * @return A pointer to the newly allocated 8-bit image buffer. If memory allocation fails,
 *         the function returns a pointer to an error message buffer (see `stbi__errpuc`).
 *
 * @note The caller is responsible for freeing the returned buffer using `STBI_FREE`.
 */
static stbi_uc *stbi__convert_16_to_8(stbi__uint16 *orig, int w, int h, int channels)
{
   int i;
   int img_len = w * h * channels;
   stbi_uc *reduced;

   reduced = (stbi_uc *) stbi__malloc(img_len);
   if (reduced == NULL) return stbi__errpuc("outofmem", "Out of memory");

   for (i = 0; i < img_len; ++i)
      reduced[i] = (stbi_uc)((orig[i] >> 8) & 0xFF); // top half of each byte is sufficient approx of 16->8 bit scaling

   STBI_FREE(orig);
   return reduced;
}

/**
 * Converts an 8-bit per channel image to a 16-bit per channel image.
 * 
 * This function takes an 8-bit per channel image represented as an array of unsigned chars (`stbi_uc`)
 * and converts it to a 16-bit per channel image represented as an array of unsigned shorts (`stbi__uint16`).
 * The conversion is done by replicating each 8-bit value to both the high and low byte of the 16-bit value,
 * effectively mapping 0 to 0 and 255 to 0xFFFF.
 *
 * @param orig      Pointer to the original 8-bit image data.
 * @param w         Width of the image in pixels.
 * @param h         Height of the image in pixels.
 * @param channels  Number of color channels in the image (e.g., 3 for RGB, 4 for RGBA).
 *
 * @return          Pointer to the newly allocated 16-bit image data. If memory allocation fails,
 *                  the function returns a pointer to an error message and frees the original image data.
 */
static stbi__uint16 *stbi__convert_8_to_16(stbi_uc *orig, int w, int h, int channels)
{
   int i;
   int img_len = w * h * channels;
   stbi__uint16 *enlarged;

   enlarged = (stbi__uint16 *) stbi__malloc(img_len*2);
   if (enlarged == NULL) return (stbi__uint16 *) stbi__errpuc("outofmem", "Out of memory");

   for (i = 0; i < img_len; ++i)
      enlarged[i] = (stbi__uint16)((orig[i] << 8) + orig[i]); // replicate to high and low byte, maps 0->0, 255->0xffff

   STBI_FREE(orig);
   return enlarged;
}

/**
 * Flips an image vertically in place. The image is represented as a contiguous block of memory,
 * where each pixel is composed of a specified number of bytes. The method swaps the top and bottom
 * rows of the image, working its way towards the center.
 *
 * @param image A pointer to the image data in memory. The image is assumed to be stored in a
 *              contiguous block, with each row following the previous one.
 * @param w The width of the image in pixels.
 * @param h The height of the image in pixels.
 * @param bytes_per_pixel The number of bytes used to represent each pixel. This determines the
 *                        size of each pixel in the image data.
 *
 * @note The method modifies the image data in place. The image data must be writable.
 * @note The method uses a temporary buffer of 2048 bytes to swap rows efficiently.
 */
static void stbi__vertical_flip(void *image, int w, int h, int bytes_per_pixel)
{
   int row;
   size_t bytes_per_row = (size_t)w * bytes_per_pixel;
   stbi_uc temp[2048];
   stbi_uc *bytes = (stbi_uc *)image;

   for (row = 0; row < (h>>1); row++) {
      stbi_uc *row0 = bytes + row*bytes_per_row;
      stbi_uc *row1 = bytes + (h - row - 1)*bytes_per_row;
      // swap row0 with row1
      size_t bytes_left = bytes_per_row;
      while (bytes_left) {
         size_t bytes_copy = (bytes_left < sizeof(temp)) ? bytes_left : sizeof(temp);
         memcpy(temp, row0, bytes_copy);
         memcpy(row0, row1, bytes_copy);
         memcpy(row1, temp, bytes_copy);
         row0 += bytes_copy;
         row1 += bytes_copy;
         bytes_left -= bytes_copy;
      }
   }
}

#ifndef STBI_NO_GIF
/**
 * Flips each slice of a 3D image vertically in place.
 *
 * This function processes a 3D image represented as a contiguous block of memory.
 * The image is divided into `z` slices, each of size `w * h * bytes_per_pixel`.
 * For each slice, the function calls `stbi__vertical_flip` to flip the slice vertically.
 * The flipping is performed in place, modifying the original image data.
 *
 * @param image Pointer to the start of the image data. The data is expected to be
 *              stored in a contiguous block, with slices stored sequentially.
 * @param w     The width of each slice in pixels.
 * @param h     The height of each slice in pixels.
 * @param z     The number of slices in the 3D image.
 * @param bytes_per_pixel The number of bytes used to represent each pixel.
 */
static void stbi__vertical_flip_slices(void *image, int w, int h, int z, int bytes_per_pixel)
{
   int slice;
   int slice_size = w * h * bytes_per_pixel;

   stbi_uc *bytes = (stbi_uc *)image;
   for (slice = 0; slice < z; ++slice) {
      stbi__vertical_flip(bytes, w, h, bytes_per_pixel);
      bytes += slice_size;
   }
}
#endif

/**
 * Loads an image from the given `stbi__context` and applies post-processing to ensure
 * the result is an 8-bit image. The method handles the following steps:
 *
 * 1. Loads the image using `stbi__load_main`, which decodes the image data and
 *    populates the width (`x`), height (`y`), and number of components (`comp`).
 *    The `req_comp` parameter specifies the desired number of components in the
 *    output image (0 means use the original number of components).
 *
 * 2. Asserts that the loaded image has either 8 or 16 bits per channel.
 *
 * 3. If the image has 16 bits per channel, it converts the image to 8 bits per
 *    channel using `stbi__convert_16_to_8`.
 *
 * 4. If `stbi__vertically_flip_on_load` is enabled, the image is vertically flipped
 *    using `stbi__vertical_flip`.
 *
 * 5. Returns the processed image as an 8-bit unsigned char array. If loading fails,
 *    NULL is returned.
 *
 * @param s The `stbi__context` from which to load the image.
 * @param x Pointer to store the width of the loaded image.
 * @param y Pointer to store the height of the loaded image.
 * @param comp Pointer to store the number of components in the loaded image.
 * @param req_comp The desired number of components in the output image (0 for original).
 * @return A pointer to the processed 8-bit image data, or NULL if loading fails.
 */
static unsigned char *stbi__load_and_postprocess_8bit(stbi__context *s, int *x, int *y, int *comp, int req_comp)
{
   stbi__result_info ri;
   void *result = stbi__load_main(s, x, y, comp, req_comp, &ri, 8);

   if (result == NULL)
      return NULL;

   // it is the responsibility of the loaders to make sure we get either 8 or 16 bit.
   STBI_ASSERT(ri.bits_per_channel == 8 || ri.bits_per_channel == 16);

   if (ri.bits_per_channel != 8) {
      result = stbi__convert_16_to_8((stbi__uint16 *) result, *x, *y, req_comp == 0 ? *comp : req_comp);
      ri.bits_per_channel = 8;
   }

   // @TODO: move stbi__convert_format to here

   if (stbi__vertically_flip_on_load) {
      int channels = req_comp ? req_comp : *comp;
      stbi__vertical_flip(result, *x, *y, channels * sizeof(stbi_uc));
   }

   return (unsigned char *) result;
}

/**
 * @brief Loads an image from the given context and post-processes it to ensure it is in 16-bit format.
 *
 * This function loads an image from the provided `stbi__context` and performs necessary post-processing
 * to ensure the resulting image data is in 16-bit format. The function handles the following steps:
 * 1. Loads the image data using `stbi__load_main`, which returns the raw image data along with metadata.
 * 2. Validates that the loaded image has either 8 or 16 bits per channel.
 * 3. If the image is in 8-bit format, it converts the image data to 16-bit using `stbi__convert_8_to_16`.
 * 4. If vertical flipping is enabled (`stbi__vertically_flip_on_load`), the image data is flipped vertically.
 * 5. Returns the processed 16-bit image data as a pointer to `stbi__uint16`.
 *
 * @param s Pointer to the `stbi__context` from which the image is loaded.
 * @param x Pointer to an integer where the width of the image will be stored.
 * @param y Pointer to an integer where the height of the image will be stored.
 * @param comp Pointer to an integer where the number of components (channels) in the image will be stored.
 * @param req_comp The desired number of components (channels) in the output image. If 0, the original number of components is used.
 * @return Pointer to the 16-bit image data, or NULL if the image could not be loaded or processed.
 */
static stbi__uint16 *stbi__load_and_postprocess_16bit(stbi__context *s, int *x, int *y, int *comp, int req_comp)
{
   stbi__result_info ri;
   void *result = stbi__load_main(s, x, y, comp, req_comp, &ri, 16);

   if (result == NULL)
      return NULL;

   // it is the responsibility of the loaders to make sure we get either 8 or 16 bit.
   STBI_ASSERT(ri.bits_per_channel == 8 || ri.bits_per_channel == 16);

   if (ri.bits_per_channel != 16) {
      result = stbi__convert_8_to_16((stbi_uc *) result, *x, *y, req_comp == 0 ? *comp : req_comp);
      ri.bits_per_channel = 16;
   }

   // @TODO: move stbi__convert_format16 to here
   // @TODO: special case RGB-to-Y (and RGBA-to-YA) for 8-bit-to-16-bit case to keep more precision

   if (stbi__vertically_flip_on_load) {
      int channels = req_comp ? req_comp : *comp;
      stbi__vertical_flip(result, *x, *y, channels * sizeof(stbi__uint16));
   }

   return (stbi__uint16 *) result;
}



/**
 * @brief Loads a 16-bit image from a memory buffer.
 *
 * This function decodes an image from a memory buffer containing image data and returns a pointer to the
 * decoded 16-bit image data. The image data is stored in a contiguous block of memory, with each pixel
 * represented by 16-bit values. The function supports specifying the desired number of channels in the
 * output image, allowing for conversion between different channel formats (e.g., RGB to RGBA).
 *
 * @param buffer Pointer to the memory buffer containing the image data.
 * @param len Length of the memory buffer in bytes.
 * @param x Pointer to an integer where the width of the image will be stored.
 * @param y Pointer to an integer where the height of the image will be stored.
 * @param channels_in_file Pointer to an integer where the number of channels in the original image file will be stored.
 * @param desired_channels The number of channels desired in the output image. If set to 0, the original number of channels is used.
 *
 * @return A pointer to the decoded 16-bit image data. The data is stored in row-major order, with each pixel
 *         represented by 16-bit values. The caller is responsible for freeing the memory using STBI_FREE().
 *         Returns NULL if the image could not be loaded or decoded.
 */
STBIDEF stbi_us *stbi_load_16_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels)
{
   stbi__context s;
   stbi__start_mem(&s,buffer,len);
   return stbi__load_and_postprocess_16bit(&s,x,y,channels_in_file,desired_channels);
}

/**
 * @brief Loads an image from a memory buffer and decodes it into a raw pixel array.
 *
 * This function takes a memory buffer containing an image file and decodes it into a raw array of pixels.
 * The image format is automatically detected based on the content of the buffer. The decoded image is returned
 * as an array of 8-bit unsigned integers (stbi_uc), where the pixel data is stored in a contiguous block of memory.
 *
 * @param buffer Pointer to the memory buffer containing the image data.
 * @param len Length of the memory buffer in bytes.
 * @param x Pointer to an integer where the width of the image will be stored.
 * @param y Pointer to an integer where the height of the image will be stored.
 * @param comp Pointer to an integer where the number of components (channels) per pixel will be stored.
 * @param req_comp The desired number of components (channels) per pixel in the output. If 0, the original number of components is used.
 *
 * @return A pointer to the decoded image data as an array of stbi_uc (8-bit unsigned integers). The caller is responsible
 *         for freeing this memory using STBI_FREE(). Returns NULL if the image could not be loaded or decoded.
 */
STBIDEF stbi_uc *stbi_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp, int req_comp)
{
   stbi__context s;
   stbi__start_mem(&s,buffer,len);
   return stbi__load_and_postprocess_8bit(&s,x,y,comp,req_comp);
}

#ifndef STBI_NO_GIF
/**
 * @brief Loads a GIF image from a memory buffer.
 *
 * This function decodes a GIF image stored in a memory buffer and returns the decoded image data.
 * It also optionally returns the frame delays (for animated GIFs) and the dimensions, number of
 * frames, and components of the image. The image can be vertically flipped if the global flag
 * `stbi__vertically_flip_on_load` is set.
 *
 * @param buffer Pointer to the memory buffer containing the GIF image data.
 * @param len Length of the buffer in bytes.
 * @param delays Pointer to an array that will store the frame delays (in milliseconds) for
 *               animated GIFs. Pass NULL if not needed.
 * @param x Pointer to store the width of the image in pixels.
 * @param y Pointer to store the height of the image in pixels.
 * @param z Pointer to store the number of frames in the GIF (1 for non-animated GIFs).
 * @param comp Pointer to store the number of components per pixel (e.g., 3 for RGB, 4 for RGBA).
 * @param req_comp Number of desired components per pixel. If 0, the original number of components
 *                 is used. Otherwise, the image is converted to the specified number of components.
 *
 * @return A pointer to the decoded image data. The data is stored as a sequence of frames, each
 *         with dimensions `x` by `y` and `comp` components per pixel. The caller is responsible
 *         for freeing the memory using `STBI_FREE()`.
 *
 * @note If `stbi__vertically_flip_on_load` is true, the image will be vertically flipped.
 * @note The `delays` array is allocated internally and must be freed by the caller if provided.
 */
STBIDEF stbi_uc *stbi_load_gif_from_memory(stbi_uc const *buffer, int len, int **delays, int *x, int *y, int *z, int *comp, int req_comp)
{
   unsigned char *result;
   stbi__context s;
   stbi__start_mem(&s,buffer,len);

   result = (unsigned char*) stbi__load_gif_main(&s, delays, x, y, z, comp, req_comp);
   if (stbi__vertically_flip_on_load) {
      stbi__vertical_flip_slices( result, *x, *y, *z, *comp );
   }

   return result;
}
#endif


// these is-hdr-or-not is defined independent of whether STBI_NO_LINEAR is
// defined, for API simplicity; if STBI_NO_LINEAR is defined, it always
// reports false!

STBIDEF int stbi_is_hdr_from_memory(stbi_uc const *buffer, int len)
{
   STBI_NOTUSED(buffer);
   STBI_NOTUSED(len);
   return 0;
}


static float stbi__h2l_gamma_i=1.0f/2.2f, stbi__h2l_scale_i=1.0f;

/**
 * Sets the gamma correction value for converting HDR (High Dynamic Range) images to LDR (Low Dynamic Range) images.
 * This function modifies the internal gamma correction factor used during the conversion process.
 * The gamma value is inverted and stored internally, meaning a higher input gamma results in a lower internal gamma correction factor.
 * This affects the brightness and contrast of the resulting LDR image.
 *
 * @param gamma The gamma value to be used for the HDR to LDR conversion. Must be a positive floating-point number.
 */
STBIDEF void   stbi_hdr_to_ldr_gamma(float gamma) { stbi__h2l_gamma_i = 1/gamma; }
/**
 * Sets the scaling factor for converting HDR (High Dynamic Range) images to LDR (Low Dynamic Range) images.
 * This function adjusts the internal scaling factor used during the conversion process. The scaling factor
 * is the reciprocal of the provided `scale` parameter, meaning a higher `scale` value will result in a darker
 * LDR image, while a lower `scale` value will result in a brighter LDR image.
 *
 * @param scale The scaling factor to apply during HDR to LDR conversion. Must be a positive non-zero value.
 *              The internal scaling factor is set to `1 / scale`.
 */
STBIDEF void   stbi_hdr_to_ldr_scale(float scale) { stbi__h2l_scale_i = 1/scale; }


//////////////////////////////////////////////////////////////////////////////
//
// Common code used by all image loaders
//

enum
{
   STBI__SCAN_load=0,
   STBI__SCAN_type,
   STBI__SCAN_header
};


/**
 * @brief Reads and returns the next byte from the image buffer.
 *
 * This function increments the buffer pointer to the next byte after reading.
 * It assumes that the caller has already ensured the buffer is not exhausted.
 * If the buffer is exhausted, the behavior is undefined (commented-out code suggests
 * returning 0 in such cases, but this is not currently implemented).
 *
 * @param s Pointer to the stbi__context structure containing the image buffer and its state.
 * @return The byte read from the buffer as an unsigned char (stbi_uc).
 */
stbi_inline static stbi_uc stbi__get8(stbi__context *s)
{
   //if (s->img_buffer < s->img_buffer_end)
      return *s->img_buffer++;
   //return 0;
}

/**
 * @brief Peek at the next 8-bit unsigned character from the input buffer without advancing the buffer pointer.
 *
 * This function returns the value of the next byte in the input buffer pointed to by `s->img_buffer`.
 * It does not modify the buffer pointer, allowing the caller to inspect the next byte without consuming it.
 * 
 * @param s Pointer to the stbi__context structure containing the input buffer and its state.
 * @return The next 8-bit unsigned character (stbi_uc) in the input buffer. If the buffer is empty, the behavior is undefined.
 */
stbi_inline static stbi_uc stbi__peep8(stbi__context *s)
{
   //if (s->img_buffer < s->img_buffer_end)
      return *s->img_buffer;
   //return 0;
}

#if defined(STBI_NO_JPEG) && defined(STBI_NO_HDR) && defined(STBI_NO_PIC) && defined(STBI_NO_PNM)
// nothing
#else
/**
 * Checks if the image buffer has reached the end of the data.
 *
 * This function compares the current position of the image buffer (`s->img_buffer`)
 * with the end of the buffer (`s->img_buffer_end`). If the current position is
 * greater than or equal to the end of the buffer, it indicates that the buffer
 * has been fully processed and the end of the data has been reached.
 *
 * @param s A pointer to the `stbi__context` structure containing the image buffer
 *          and its metadata.
 * @return Returns 1 if the end of the buffer has been reached (i.e., `s->img_buffer`
 *         is greater than or equal to `s->img_buffer_end`), otherwise returns 0.
 */
stbi_inline static int stbi__at_eof(stbi__context *s)
{
   return s->img_buffer >= s->img_buffer_end;
}
#endif

#if defined(STBI_NO_JPEG) && defined(STBI_NO_PNG) && defined(STBI_NO_BMP) && defined(STBI_NO_PSD) && defined(STBI_NO_TGA) && defined(STBI_NO_GIF) && defined(STBI_NO_PIC)
// nothing
#else
/**
 * Skips a specified number of bytes in the image buffer.
 *
 * This function adjusts the current position in the image buffer by skipping
 * `n` bytes. If `n` is zero, the function does nothing. If `n` is negative,
 * the buffer position is set to the end of the buffer. If `n` is positive,
 * the buffer position is advanced by `n` bytes.
 *
 * @param s Pointer to the stbi__context structure containing the image buffer.
 * @param n The number of bytes to skip. If negative, the buffer is set to the end.
 */
static void stbi__skip(stbi__context *s, int n)
{
   if (n == 0) return;  // already there!
   if (n < 0) {
      s->img_buffer = s->img_buffer_end;
      return;
   }
   s->img_buffer += n;
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_TGA) && defined(STBI_NO_HDR) && defined(STBI_NO_PNM)
// nothing
#else
/**
 * Reads a specified number of bytes from the image buffer and copies them into the provided buffer.
 * 
 * This function checks if there are enough bytes remaining in the image buffer to fulfill the request.
 * If sufficient bytes are available, it copies 'n' bytes from the image buffer into 'buffer' and advances
 * the image buffer pointer by 'n' bytes. If there are not enough bytes remaining, the function does not
 * modify the buffer and returns 0.
 *
 * @param s Pointer to the stbi__context structure containing the image buffer and its state.
 * @param buffer Pointer to the destination buffer where the bytes will be copied.
 * @param n The number of bytes to read from the image buffer.
 * @return Returns 1 if the operation is successful (i.e., 'n' bytes were read and copied), otherwise returns 0.
 */
static int stbi__getn(stbi__context *s, stbi_uc *buffer, int n)
{
   if (s->img_buffer+n <= s->img_buffer_end) {
      memcpy(buffer, s->img_buffer, n);
      s->img_buffer += n;
      return 1;
   } else
      return 0;
}
#endif

#if defined(STBI_NO_JPEG) && defined(STBI_NO_PNG) && defined(STBI_NO_PSD) && defined(STBI_NO_PIC)
// nothing
#else
/**
 * Reads a 16-bit big-endian integer from the given stbi__context.
 *
 * This function reads two bytes from the provided stbi__context and interprets
 * them as a 16-bit integer in big-endian format. The first byte read is the
 * most significant byte (MSB), and the second byte is the least significant
 * byte (LSB). The function combines these two bytes into a single 16-bit
 * integer and returns the result.
 *
 * @param s The stbi__context from which to read the bytes.
 * @return The 16-bit integer read from the context, interpreted as big-endian.
 */
static int stbi__get16be(stbi__context *s)
{
   int z = stbi__get8(s);
   return (z << 8) + stbi__get8(s);
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_PSD) && defined(STBI_NO_PIC)
// nothing
#else
/**
 * Reads a 32-bit unsigned integer in big-endian format from the given context.
 *
 * This function reads two 16-bit unsigned integers in big-endian format from the
 * provided context `s`, combines them into a single 32-bit unsigned integer, and
 * returns the result. The first 16-bit value is shifted left by 16 bits and added
 * to the second 16-bit value to form the final 32-bit integer.
 *
 * @param s Pointer to the stbi__context structure from which to read the data.
 * @return The 32-bit unsigned integer read from the context in big-endian format.
 */
static stbi__uint32 stbi__get32be(stbi__context *s)
{
   stbi__uint32 z = stbi__get16be(s);
   return (z << 16) + stbi__get16be(s);
}
#endif

#if defined(STBI_NO_BMP) && defined(STBI_NO_TGA) && defined(STBI_NO_GIF)
// nothing
#else
/**
 * Reads a 16-bit little-endian integer from the provided stbi__context.
 *
 * This function reads two bytes from the input stream and interprets them as a
 * 16-bit little-endian integer. The first byte read is the least significant byte (LSB),
 * and the second byte is the most significant byte (MSB). The function combines these
 * two bytes to form the final 16-bit integer value.
 *
 * @param s Pointer to the stbi__context from which to read the bytes.
 * @return The 16-bit little-endian integer read from the stream.
 */
static int stbi__get16le(stbi__context *s)
{
   int z = stbi__get8(s);
   return z + (stbi__get8(s) << 8);
}
#endif

#ifndef STBI_NO_BMP
/**
 * Reads a 32-bit unsigned integer in little-endian format from the given stbi__context.
 * The method reads two 16-bit unsigned integers in little-endian format and combines them
 * to form a 32-bit unsigned integer. The first 16-bit integer represents the lower 16 bits,
 * and the second 16-bit integer represents the upper 16 bits.
 *
 * @param s Pointer to the stbi__context from which to read the data.
 * @return The 32-bit unsigned integer read from the context in little-endian format.
 */
static stbi__uint32 stbi__get32le(stbi__context *s)
{
   stbi__uint32 z = stbi__get16le(s);
   return z + (stbi__get16le(s) << 16);
}
#endif

#define STBI__BYTECAST(x)  ((stbi_uc) ((x) & 255))  // truncate int to byte without warnings

#if defined(STBI_NO_JPEG) && defined(STBI_NO_PNG) && defined(STBI_NO_BMP) && defined(STBI_NO_PSD) && defined(STBI_NO_TGA) && defined(STBI_NO_GIF) && defined(STBI_NO_PIC) && defined(STBI_NO_PNM)
// nothing
#else
//////////////////////////////////////////////////////////////////////////////
//
//  generic converter from built-in img_n to req_comp
//    individual types do this automatically as much as possible (e.g. jpeg
//    does all cases internally since it needs to colorspace convert anyway,
//    and it never has alpha, so very few cases ). png can automatically
//    interleave an alpha=255 channel, but falls back to this for other cases
//
//  assume data buffer is malloced, so malloc a new one and free that one
//  only failure mode is malloc failing

static stbi_uc stbi__compute_y(int r, int g, int b)
{
   return (stbi_uc) (((r*77) + (g*150) +  (29*b)) >> 8);
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_BMP) && defined(STBI_NO_PSD) && defined(STBI_NO_TGA) && defined(STBI_NO_GIF) && defined(STBI_NO_PIC) && defined(STBI_NO_PNM)
// nothing
#else
/**
 * Converts the format of an image data buffer from one component count to another.
 *
 * This function takes an image data buffer and converts it from its current format
 * (with `img_n` components per pixel) to a new format with `req_comp` components per pixel.
 * The conversion is performed for an image of size `x` (width) by `y` (height).
 *
 * The function handles various combinations of source and destination component counts,
 * including but not limited to grayscale to RGB, RGB to RGBA, and vice versa. If the
 * requested component count is the same as the source component count, the original
 * data is returned without modification.
 *
 * If the conversion is not supported or if memory allocation fails, the function
 * returns an error pointer and frees any allocated memory.
 *
 * @param data     Pointer to the source image data buffer.
 * @param img_n    Number of components per pixel in the source image (1 to 4).
 * @param req_comp Number of components per pixel in the destination image (1 to 4).
 * @param x        Width of the image in pixels.
 * @param y        Height of the image in pixels.
 *
 * @return         Pointer to the converted image data buffer on success. If an error
 *                 occurs, returns an error pointer (e.g., "outofmem" or "unsupported").
 */
static unsigned char *stbi__convert_format(unsigned char *data, int img_n, int req_comp, unsigned int x, unsigned int y)
{
   int i,j;
   unsigned char *good;

   if (req_comp == img_n) return data;
   STBI_ASSERT(req_comp >= 1 && req_comp <= 4);

   good = (unsigned char *) stbi__malloc_mad3(req_comp, x, y, 0);
   if (good == NULL) {
      STBI_FREE(data);
      return stbi__errpuc("outofmem", "Out of memory");
   }

   for (j=0; j < (int) y; ++j) {
      unsigned char *src  = data + j * x * img_n   ;
      unsigned char *dest = good + j * x * req_comp;

      #define STBI__COMBO(a,b)  ((a)*8+(b))
      #define STBI__CASE(a,b)   case STBI__COMBO(a,b): for(i=x-1; i >= 0; --i, src += a, dest += b)
      // convert source image with img_n components to one with req_comp components;
      // avoid switch per pixel, so use switch per scanline and massive macros
      switch (STBI__COMBO(img_n, req_comp)) {
         STBI__CASE(1,2) { dest[0]=src[0]; dest[1]=255;                                     } break;
         STBI__CASE(1,3) { dest[0]=dest[1]=dest[2]=src[0];                                  } break;
         STBI__CASE(1,4) { dest[0]=dest[1]=dest[2]=src[0]; dest[3]=255;                     } break;
         STBI__CASE(2,1) { dest[0]=src[0];                                                  } break;
         STBI__CASE(2,3) { dest[0]=dest[1]=dest[2]=src[0];                                  } break;
         STBI__CASE(2,4) { dest[0]=dest[1]=dest[2]=src[0]; dest[3]=src[1];                  } break;
         STBI__CASE(3,4) { dest[0]=src[0];dest[1]=src[1];dest[2]=src[2];dest[3]=255;        } break;
         STBI__CASE(3,1) { dest[0]=stbi__compute_y(src[0],src[1],src[2]);                   } break;
         STBI__CASE(3,2) { dest[0]=stbi__compute_y(src[0],src[1],src[2]); dest[1] = 255;    } break;
         STBI__CASE(4,1) { dest[0]=stbi__compute_y(src[0],src[1],src[2]);                   } break;
         STBI__CASE(4,2) { dest[0]=stbi__compute_y(src[0],src[1],src[2]); dest[1] = src[3]; } break;
         STBI__CASE(4,3) { dest[0]=src[0];dest[1]=src[1];dest[2]=src[2];                    } break;
         default: STBI_ASSERT(0); STBI_FREE(data); STBI_FREE(good); return stbi__errpuc("unsupported", "Unsupported format conversion");
      }
      #undef STBI__CASE
   }

   STBI_FREE(data);
   return good;
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_PSD)
// nothing
#else
/**
 * Computes the luminance (Y) value for a given RGB color in 16-bit format.
 * The luminance is calculated using the standard formula for converting RGB to grayscale:
 * Y = (R * 77 + G * 150 + B * 29) >> 8
 * This formula approximates the perceived brightness of the color by weighting the RGB components
 * according to their contribution to luminance. The result is shifted right by 8 bits to normalize
 * the value to a 16-bit range.
 *
 * @param r The red component of the color (0-255).
 * @param g The green component of the color (0-255).
 * @param b The blue component of the color (0-255).
 * @return The computed luminance value as a 16-bit unsigned integer.
 */
static stbi__uint16 stbi__compute_y_16(int r, int g, int b)
{
   return (stbi__uint16) (((r*77) + (g*150) +  (29*b)) >> 8);
}
#endif

#if defined(STBI_NO_PNG) && defined(STBI_NO_PSD)
// nothing
#else
/**
 * Converts a 16-bit image data buffer from one format to another.
 *
 * This function takes a 16-bit image data buffer and converts it from a source format
 * with `img_n` components per pixel to a destination format with `req_comp` components
 * per pixel. The function handles various combinations of source and destination formats,
 * including grayscale, RGB, and RGBA. If the requested format is the same as the source
 * format, the original data is returned without modification. Otherwise, a new buffer
 * is allocated to store the converted data.
 *
 * @param data      Pointer to the source image data buffer (16-bit per component).
 * @param img_n     Number of components per pixel in the source image (1 to 4).
 * @param req_comp  Number of components per pixel in the destination image (1 to 4).
 * @param x         Width of the image in pixels.
 * @param y         Height of the image in pixels.
 *
 * @return          Pointer to the converted image data buffer (16-bit per component).
 *                  If the conversion is not supported or memory allocation fails, the
 *                  function returns a pointer to an error message buffer.
 */
static stbi__uint16 *stbi__convert_format16(stbi__uint16 *data, int img_n, int req_comp, unsigned int x, unsigned int y)
{
   int i,j;
   stbi__uint16 *good;

   if (req_comp == img_n) return data;
   STBI_ASSERT(req_comp >= 1 && req_comp <= 4);

   good = (stbi__uint16 *) stbi__malloc(req_comp * x * y * 2);
   if (good == NULL) {
      STBI_FREE(data);
      return (stbi__uint16 *) stbi__errpuc("outofmem", "Out of memory");
   }

   for (j=0; j < (int) y; ++j) {
      stbi__uint16 *src  = data + j * x * img_n   ;
      stbi__uint16 *dest = good + j * x * req_comp;

      #define STBI__COMBO(a,b)  ((a)*8+(b))
      #define STBI__CASE(a,b)   case STBI__COMBO(a,b): for(i=x-1; i >= 0; --i, src += a, dest += b)
      // convert source image with img_n components to one with req_comp components;
      // avoid switch per pixel, so use switch per scanline and massive macros
      switch (STBI__COMBO(img_n, req_comp)) {
         STBI__CASE(1,2) { dest[0]=src[0]; dest[1]=0xffff;                                     } break;
         STBI__CASE(1,3) { dest[0]=dest[1]=dest[2]=src[0];                                     } break;
         STBI__CASE(1,4) { dest[0]=dest[1]=dest[2]=src[0]; dest[3]=0xffff;                     } break;
         STBI__CASE(2,1) { dest[0]=src[0];                                                     } break;
         STBI__CASE(2,3) { dest[0]=dest[1]=dest[2]=src[0];                                     } break;
         STBI__CASE(2,4) { dest[0]=dest[1]=dest[2]=src[0]; dest[3]=src[1];                     } break;
         STBI__CASE(3,4) { dest[0]=src[0];dest[1]=src[1];dest[2]=src[2];dest[3]=0xffff;        } break;
         STBI__CASE(3,1) { dest[0]=stbi__compute_y_16(src[0],src[1],src[2]);                   } break;
         STBI__CASE(3,2) { dest[0]=stbi__compute_y_16(src[0],src[1],src[2]); dest[1] = 0xffff; } break;
         STBI__CASE(4,1) { dest[0]=stbi__compute_y_16(src[0],src[1],src[2]);                   } break;
         STBI__CASE(4,2) { dest[0]=stbi__compute_y_16(src[0],src[1],src[2]); dest[1] = src[3]; } break;
         STBI__CASE(4,3) { dest[0]=src[0];dest[1]=src[1];dest[2]=src[2];                       } break;
         default: STBI_ASSERT(0); STBI_FREE(data); STBI_FREE(good); return (stbi__uint16*) stbi__errpuc("unsupported", "Unsupported format conversion");
      }
      #undef STBI__CASE
   }

   STBI_FREE(data);
   return good;
}
#endif



//////////////////////////////////////////////////////////////////////////////
//
//  "baseline" JPEG/JFIF decoder
//
//    simple implementation
//      - doesn't support delayed output of y-dimension
//      - simple interface (only one output format: 8-bit interleaved RGB)
//      - doesn't try to recover corrupt jpegs
//      - doesn't allow partial loading, loading multiple at once
//      - still fast on x86 (copying globals into locals doesn't help x86)
//      - allocates lots of intermediate memory (full size of all components)
//        - non-interleaved case requires this anyway
//        - allows good upsampling (see next)
//    high-quality
//      - upsampled channels are bilinearly interpolated, even across blocks
//      - quality integer IDCT derived from IJG's 'slow'
//    performance
//      - fast huffman; reasonable integer IDCT
//      - some SIMD kernels for common paths on targets with SSE2/NEON
//      - uses a lot of intermediate memory, could cache poorly

#ifndef STBI_NO_JPEG

// huffman decoding acceleration
#define FAST_BITS   9  // larger handles more cases; smaller stomps less cache

typedef struct
{
   stbi_uc  fast[1 << FAST_BITS];
   // weirdly, repacking this into AoS is a 10% speed loss, instead of a win
   stbi__uint16 code[256];
   stbi_uc  values[256];
   stbi_uc  size[257];
   unsigned int maxcode[18];
   int    delta[17];   // old 'firstsymbol' - old 'firstcode'
} stbi__huffman;

typedef struct img_comp
{
  int id;
  int h,v;
  int tq;
  int hd,ha;
  int dc_pred;

  int x,y,w2,h2;
  stbi_uc *data;
  void *raw_data, *raw_coeff;
  stbi_uc *linebuf;
  short   *coeff;   // progressive only
  int      coeff_w, coeff_h; // number of 8x8 coefficient blocks
} img_comp;

typedef struct
{
   stbi__context *s;
   stbi__huffman huff_dc[4];
   stbi__huffman huff_ac[4];
   stbi__uint16 dequant[4][64];
   stbi__int16 fast_ac[4][1 << FAST_BITS];

// sizes for components, interleaved MCUs
   int img_h_max, img_v_max;
   int img_mcu_x, img_mcu_y;
   int img_mcu_w, img_mcu_h;

// definition of jpeg image component
   img_comp img_comp[4];

   stbi__uint32   code_buffer; // jpeg entropy-coded buffer
   int            code_bits;   // number of valid bits
   unsigned char  marker;      // marker seen while filling entropy buffer
   int            more;        // flag if we saw a marker so must stop

   int            progressive;
   int            spec_start;
   int            spec_end;
   int            succ_high;
   int            succ_low;
   int            eob_run;
   int            jfif;
   int            app14_color_transform; // Adobe APP14 tag
   int            rgb;

   int scan_n, order[4];
   int restart_interval, todo;
} stbi__jpeg;

/**
 * Builds a Huffman table from the given count of Huffman code lengths.
 *
 * This function constructs a Huffman table based on the count of Huffman code lengths
 * provided in the `count` array. The `count` array specifies the number of Huffman codes
 * for each possible code length (from 1 to 16). The function populates the `stbi__huffman`
 * structure with the computed Huffman codes, sizes, and an acceleration table for faster
 * decoding.
 *
 * @param h Pointer to the `stbi__huffman` structure where the Huffman table will be stored.
 * @param count Array of 16 integers, where each element represents the number of Huffman
 *              codes for the corresponding code length (1 to 16).
 *
 * @return Returns 1 on success. If an error occurs (e.g., invalid code lengths), the
 *         function returns an error code (via `stbi__err`) indicating a corrupt JPEG.
 */
static int stbi__build_huffman(stbi__huffman *h, int *count)
{
   int i,j,k=0;
   unsigned int code;
   // build size list for each symbol (from JPEG spec)
   for (i=0; i < 16; ++i)
      for (j=0; j < count[i]; ++j)
         h->size[k++] = (stbi_uc) (i+1);
   h->size[k] = 0;

   // compute actual symbols (from jpeg spec)
   code = 0;
   k = 0;
   for(j=1; j <= 16; ++j) {
      // compute delta to add to code to compute symbol id
      h->delta[j] = k - code;
      if (h->size[k] == j) {
         while (h->size[k] == j)
            h->code[k++] = (stbi__uint16) (code++);
         if (code-1 >= (1u << j)) return stbi__err("bad code lengths","Corrupt JPEG");
      }
      // compute largest code + 1 for this size, preshifted as needed later
      h->maxcode[j] = code << (16-j);
      code <<= 1;
   }
   h->maxcode[j] = 0xffffffff;

   // build non-spec acceleration table; 255 is flag for not-accelerated
   memset(h->fast, 255, 1 << FAST_BITS);
   for (i=0; i < k; ++i) {
      int s = h->size[i];
      if (s <= FAST_BITS) {
         int c = h->code[i] << (FAST_BITS-s);
         int m = 1 << (FAST_BITS-s);
         for (j=0; j < m; ++j) {
            h->fast[c+j] = (stbi_uc) i;
         }
      }
   }
   return 1;
}

// build a table that decodes both magnitude and value of small ACs in
// one go.
static void stbi__build_fast_ac(stbi__int16 *fast_ac, stbi__huffman *h)
{
   int i;
   for (i=0; i < (1 << FAST_BITS); ++i) {
      stbi_uc fast = h->fast[i];
      fast_ac[i] = 0;
      if (fast < 255) {
         int rs = h->values[fast];
         int run = (rs >> 4) & 15;
         int magbits = rs & 15;
         int len = h->size[fast];

         if (magbits && len + magbits <= FAST_BITS) {
            // magnitude code followed by receive_extend code
            int k = ((i << len) & ((1 << FAST_BITS) - 1)) >> (FAST_BITS - magbits);
            int m = 1 << (magbits - 1);
            if (k < m) k += (~0U << magbits) + 1;
            // if the result is small enough, we can fit it in fast_ac table
            if (k >= -128 && k <= 127)
               fast_ac[i] = (stbi__int16) ((k * 256) + (run * 16) + (len + magbits));
         }
      }
   }
}

/**
 * Expands the JPEG decoder's internal buffer to ensure sufficient data is available
 * for decoding. This function is unsafe because it assumes the buffer can grow
 * without bounds and does not perform error checking.
 *
 * The function processes the buffer by peeking at the next byte and determining
 * if it is a marker (0xFF). If a marker is found, it consumes any fill bytes
 * (0xFF) and identifies the marker type. If the marker is not a fill byte,
 * it sets the marker in the JPEG context and stops further processing.
 * Otherwise, it shifts the byte into the code buffer and continues until
 * the code buffer has at least 24 bits of data.
 *
 * @param j Pointer to the stbi__jpeg context containing the buffer and state
 *          information for decoding.
 */
static void stbi__grow_buffer_unsafe(stbi__jpeg *j)
{
   do {
      unsigned int b = j->more & stbi__peep8(j->s);
      j->s->img_buffer -= j->more;
      if (b == 0xff) {
         int c = stbi__get8(j->s);
         while (c == 0xff) c = stbi__get8(j->s); // consume fill bytes
         if (c != 0) {
            j->marker = (unsigned char) c;
            j->more = 0;
            return;
         }
      }
      j->code_buffer |= b << (24 - j->code_bits);
      j->code_bits += 8;
   } while (j->code_bits <= 24);
}

// decode a jpeg huffman value from the bitstream
stbi_inline static int stbi__jpeg_huff_decode(stbi__jpeg *j, stbi__huffman *h)
{
   unsigned int temp;
   int c,k;

   if (j->code_bits < 16) stbi__grow_buffer_unsafe(j);

   // look at the top FAST_BITS and determine what symbol ID it is,
   // if the code is <= FAST_BITS
   c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS)-1);
   k = h->fast[c];
   if (k < 255) {
      int s = h->size[k];
      if (s > j->code_bits)
         return -1;
      j->code_buffer <<= s;
      j->code_bits -= s;
      return h->values[k];
   }

   // naive test is to shift the code_buffer down so k bits are
   // valid, then test against maxcode. To speed this up, we've
   // preshifted maxcode left so that it has (16-k) 0s at the
   // end; in other words, regardless of the number of bits, it
   // wants to be compared against something shifted to have 16;
   // that way we don't need to shift inside the loop.
   temp = j->code_buffer >> 16;
   for (k=FAST_BITS+1 ; ; ++k)
      if (temp < h->maxcode[k])
         break;
   if (k == 17) {
      // error! code not found
      j->code_bits -= 16;
      return -1;
   }

   if (k > j->code_bits)
      return -1;

   // convert the huffman code to the symbol id
   c = (j->code_buffer >> (32 - k)) + h->delta[k];
   STBI_ASSERT((j->code_buffer >> (32 - h->size[c])) == h->code[c]);

   // convert the id to a symbol
   j->code_bits -= k;
   j->code_buffer <<= k;
   return h->values[c];
}

// bias[n] = (-1<<n) + 1
static const int stbi__jbias[16] = {0,-1,-3,-7,-15,-31,-63,-127,-255,-511,-1023,-2047,-4095,-8191,-16383,-32767};

// combined JPEG 'receive' and JPEG 'extend', since baseline
// always extends everything it receives.
stbi_inline static int stbi__extend_receive(stbi__jpeg *j, int n)
{
   unsigned int k;
   int sgn;
   if (j->code_bits < n) stbi__grow_buffer_unsafe(j);

   sgn = (stbi__int32)j->code_buffer >> 31; // sign bit is always in MSB
   k = j->code_buffer >> (32 - n);
   j->code_buffer = j->code_buffer << n;
   j->code_bits -= n;
   return k + (stbi__jbias[n] & ~sgn);
}

// get some unsigned bits
stbi_inline static int stbi__jpeg_get_bits(stbi__jpeg *j, int n)
{
   unsigned int k;
   if (j->code_bits < n) stbi__grow_buffer_unsafe(j);
   k = j->code_buffer >> (32 - n);
   j->code_buffer = j->code_buffer << n;
   j->code_bits -= n;
   return k;
}

/**
 * @brief Retrieves the next bit from the JPEG code buffer.
 *
 * This method extracts the next bit from the JPEG code buffer. If there are no bits left in the buffer,
 * it first ensures the buffer is grown to contain more bits by calling `stbi__grow_buffer_unsafe`. 
 * The bit is extracted from the most significant bit (MSB) of the `code_buffer`. The buffer is then 
 * shifted left by one bit, and the `code_bits` counter is decremented.
 *
 * @param j Pointer to the `stbi__jpeg` structure containing the code buffer and bit count.
 * @return The extracted bit as an integer (1 if the MSB is set, 0 otherwise).
 */
stbi_inline static int stbi__jpeg_get_bit(stbi__jpeg *j)
{
   unsigned int k;
   if (j->code_bits < 1) stbi__grow_buffer_unsafe(j);
   k = j->code_buffer;
   j->code_buffer <<= 1;
   --j->code_bits;
   return k & 0x80000000;
}

// given a value that's at position X in the zigzag stream,
// where does it appear in the 8x8 matrix coded as row-major?
static const stbi_uc stbi__jpeg_dezigzag[64+15] =
{
    0,  1,  8, 16,  9,  2,  3, 10,
   17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34,
   27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36,
   29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46,
   53, 60, 61, 54, 47, 55, 62, 63,
   // let corrupt input sample past end
   63, 63, 63, 63, 63, 63, 63, 63,
   63, 63, 63, 63, 63, 63, 63
};

// decode one 64-entry block--
static int stbi__jpeg_decode_block(stbi__jpeg *j, short data[64], stbi__huffman *hdc, stbi__huffman *hac, stbi__int16 *fac, int b, stbi__uint16 *dequant)
{
   int diff,dc,k;
   int t;

   if (j->code_bits < 16) stbi__grow_buffer_unsafe(j);
   t = stbi__jpeg_huff_decode(j, hdc);
   if (t < 0) return stbi__err("bad huffman code","Corrupt JPEG");

   // 0 all the ac values now so we can do it 32-bits at a time
   memset(data,0,64*sizeof(data[0]));

   diff = t ? stbi__extend_receive(j, t) : 0;
   dc = j->img_comp[b].dc_pred + diff;
   j->img_comp[b].dc_pred = dc;
   data[0] = (short) (dc * dequant[0]);

   // decode AC components, see JPEG spec
   k = 1;
   do {
      unsigned int zig;
      int c,r,s;
      if (j->code_bits < 16) stbi__grow_buffer_unsafe(j);
      c = j->code_buffer >> (32 - FAST_BITS);
      r = fac[c];
      if (r) { // fast-AC path
         k += (r >> 4) & 15; // run
         s = r & 15; // combined length
         j->code_buffer <<= s;
         j->code_bits -= s;
         // decode into unzigzag'd location
         zig = stbi__jpeg_dezigzag[k++];
         data[zig] = (short) ((r >> 8) * dequant[zig]);
      } else {
         int rs = stbi__jpeg_huff_decode(j, hac);
         if (rs < 0) return stbi__err("bad huffman code","Corrupt JPEG");
         s = rs & 15;
         r = rs >> 4;
         if (s == 0) {
            if (rs != 0xf0) break; // end block
            k += 16;
         } else {
            k += r;
            // decode into unzigzag'd location
            zig = stbi__jpeg_dezigzag[k++];
            data[zig] = (short) (stbi__extend_receive(j,s) * dequant[zig]);
         }
      }
   } while (k < 64);
   return 1;
}

/**
 * Decodes a single DC coefficient in a progressive JPEG image block.
 *
 * This method handles both the initial scan and refinement scan for the DC coefficient.
 * During the initial scan, it decodes the DC coefficient using the provided Huffman table,
 * computes the difference from the previous DC value, and stores the result in the data array.
 * During the refinement scan, it adjusts the DC coefficient based on the refinement bit.
 *
 * @param j Pointer to the stbi__jpeg structure containing the JPEG decoding context.
 * @param data Array where the decoded DC coefficient is stored. The array is zeroed during the initial scan.
 * @param hdc Pointer to the Huffman table used for decoding the DC coefficient.
 * @param b Index of the image component being processed.
 *
 * @return Returns 1 on success. If an error occurs (e.g., invalid Huffman code or attempt to merge DC and AC),
 *         an error message is logged, and the function returns 0.
 */
static int stbi__jpeg_decode_block_prog_dc(stbi__jpeg *j, short data[64], stbi__huffman *hdc, int b)
{
   int diff,dc;
   int t;
   if (j->spec_end != 0) return stbi__err("can't merge dc and ac", "Corrupt JPEG");

   if (j->code_bits < 16) stbi__grow_buffer_unsafe(j);

   if (j->succ_high == 0) {
      // first scan for DC coefficient, must be first
      memset(data,0,64*sizeof(data[0])); // 0 all the ac values now
      t = stbi__jpeg_huff_decode(j, hdc);
      if (t == -1) return stbi__err("can't merge dc and ac", "Corrupt JPEG");
      diff = t ? stbi__extend_receive(j, t) : 0;

      dc = j->img_comp[b].dc_pred + diff;
      j->img_comp[b].dc_pred = dc;
      data[0] = (short) (dc << j->succ_low);
   } else {
      // refinement scan for DC coefficient
      if (stbi__jpeg_get_bit(j))
         data[0] += (short) (1 << j->succ_low);
   }
   return 1;
}

// @OPTIMIZE: store non-zigzagged during the decode passes,
// and only de-zigzag when dequantizing
static int stbi__jpeg_decode_block_prog_ac(stbi__jpeg *j, short data[64], stbi__huffman *hac, stbi__int16 *fac)
{
   int k;
   if (j->spec_start == 0) return stbi__err("can't merge dc and ac", "Corrupt JPEG");

   if (j->succ_high == 0) {
      int shift = j->succ_low;

      if (j->eob_run) {
         --j->eob_run;
         return 1;
      }

      k = j->spec_start;
      do {
         unsigned int zig;
         int c,r,s;
         if (j->code_bits < 16) stbi__grow_buffer_unsafe(j);
         c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS)-1);
         r = fac[c];
         if (r) { // fast-AC path
            k += (r >> 4) & 15; // run
            s = r & 15; // combined length
            j->code_buffer <<= s;
            j->code_bits -= s;
            zig = stbi__jpeg_dezigzag[k++];
            data[zig] = (short) ((r >> 8) << shift);
         } else {
            int rs = stbi__jpeg_huff_decode(j, hac);
            if (rs < 0) return stbi__err("bad huffman code","Corrupt JPEG");
            s = rs & 15;
            r = rs >> 4;
            if (s == 0) {
               if (r < 15) {
                  j->eob_run = (1 << r);
                  if (r)
                     j->eob_run += stbi__jpeg_get_bits(j, r);
                  --j->eob_run;
                  break;
               }
               k += 16;
            } else {
               k += r;
               zig = stbi__jpeg_dezigzag[k++];
               data[zig] = (short) (stbi__extend_receive(j,s) << shift);
            }
         }
      } while (k <= j->spec_end);
   } else {
      // refinement scan for these AC coefficients

      short bit = (short) (1 << j->succ_low);

      if (j->eob_run) {
         --j->eob_run;
         for (k = j->spec_start; k <= j->spec_end; ++k) {
            short *p = &data[stbi__jpeg_dezigzag[k]];
            if (*p != 0)
               if (stbi__jpeg_get_bit(j))
                  if ((*p & bit)==0) {
                     if (*p > 0)
                        *p += bit;
                     else
                        *p -= bit;
                  }
         }
      } else {
         k = j->spec_start;
         do {
            int r,s;
            int rs = stbi__jpeg_huff_decode(j, hac); // @OPTIMIZE see if we can use the fast path here, advance-by-r is so slow, eh
            if (rs < 0) return stbi__err("bad huffman code","Corrupt JPEG");
            s = rs & 15;
            r = rs >> 4;
            if (s == 0) {
               if (r < 15) {
                  j->eob_run = (1 << r) - 1;
                  if (r)
                     j->eob_run += stbi__jpeg_get_bits(j, r);
                  r = 64; // force end of block
               } else {
                  // r=15 s=0 should write 16 0s, so we just do
                  // a run of 15 0s and then write s (which is 0),
                  // so we don't have to do anything special here
               }
            } else {
               if (s != 1) return stbi__err("bad huffman code", "Corrupt JPEG");
               // sign bit
               if (stbi__jpeg_get_bit(j))
                  s = bit;
               else
                  s = -bit;
            }

            // advance by r
            while (k <= j->spec_end) {
               short *p = &data[stbi__jpeg_dezigzag[k++]];
               if (*p != 0) {
                  if (stbi__jpeg_get_bit(j))
                     if ((*p & bit)==0) {
                        if (*p > 0)
                           *p += bit;
                        else
                           *p -= bit;
                     }
               } else {
                  if (r == 0) {
                     *p = (short) s;
                     break;
                  }
                  --r;
               }
            }
         } while (k <= j->spec_end);
      }
   }
   return 1;
}

// take a -128..127 value and stbi__clamp it and convert to 0..255
stbi_inline static stbi_uc stbi__clamp(int x)
{
   // trick to use a single test to catch both cases
#if 0
   if ((unsigned int) x > 255) {
      if (x < 0) return 0;
      if (x > 255) return 255;
   }
   return (stbi_uc) x;
#else
   if ((unsigned) x <= 255) return x;
   return ((unsigned)x >> 31) + 255;
#endif
}

#define stbi__f2f(x)  ((int) (((x) * 4096 + 0.5)))
#define stbi__fsh(x)  ((x) * 4096)

// derived from jidctint -- DCT_ISLOW
#define STBI__IDCT_1D(s0,s1,s2,s3,s4,s5,s6,s7) \
   int t0,t1,t2,t3,p1,p2,p3,p4,p5,x0,x1,x2,x3; \
   p2 = s2;                                    \
   p3 = s6;                                    \
   p1 = (p2+p3) * stbi__f2f(0.5411961f);       \
   t2 = p1 + p3*stbi__f2f(-1.847759065f);      \
   t3 = p1 + p2*stbi__f2f( 0.765366865f);      \
   p2 = s0;                                    \
   p3 = s4;                                    \
   t0 = stbi__fsh(p2+p3);                      \
   t1 = stbi__fsh(p2-p3);                      \
   x0 = t0+t3;                                 \
   x3 = t0-t3;                                 \
   x1 = t1+t2;                                 \
   x2 = t1-t2;                                 \
   t0 = s7;                                    \
   t1 = s5;                                    \
   t2 = s3;                                    \
   t3 = s1;                                    \
   p3 = t0+t2;                                 \
   p4 = t1+t3;                                 \
   p1 = t0+t3;                                 \
   p2 = t1+t2;                                 \
   p5 = (p3+p4)*stbi__f2f( 1.175875602f);      \
   t0 = t0*stbi__f2f( 0.298631336f);           \
   t1 = t1*stbi__f2f( 2.053119869f);           \
   t2 = t2*stbi__f2f( 3.072711026f);           \
   t3 = t3*stbi__f2f( 1.501321110f);           \
   p1 = p5 + p1*stbi__f2f(-0.899976223f);      \
   p2 = p5 + p2*stbi__f2f(-2.562915447f);      \
   p3 = p3*stbi__f2f(-1.961570560f);           \
   p4 = p4*stbi__f2f(-0.390180644f);           \
   t3 += p1+p4;                                \
   t2 += p2+p3;                                \
   t1 += p2+p4;                                \
   t0 += p1+p3;

/**
 * Performs an Inverse Discrete Cosine Transform (IDCT) on an 8x8 block of data.
 * This function is used to transform quantized frequency domain data back into
 * spatial domain pixel values, typically as part of JPEG decoding.
 *
 * The method processes the input data in two passes:
 * 1. Column-wise IDCT: Applies the 1D IDCT to each column of the 8x8 block.
 *    If a column contains all zeroes (except for the DC term), it uses a shortcut
 *    to avoid unnecessary computation.
 * 2. Row-wise IDCT: Applies the 1D IDCT to each row of the intermediate result
 *    from the first pass. The final pixel values are clamped to the range [0, 255]
 *    and stored in the output buffer.
 *
 * @param out        Pointer to the output buffer where the resulting pixel values
 *                   will be stored. The buffer should have enough space for 8x8 pixels.
 * @param out_stride Stride (in bytes) between rows in the output buffer. This allows
 *                   the function to work with non-contiguous memory layouts.
 * @param data       Pointer to an 8x8 block of quantized frequency domain data.
 *                   This data is typically obtained from the JPEG decoding process.
 */
static void stbi__idct_block(stbi_uc *out, int out_stride, short data[64])
{
   int i,val[64],*v=val;
   stbi_uc *o;
   short *d = data;

   // columns
   for (i=0; i < 8; ++i,++d, ++v) {
      // if all zeroes, shortcut -- this avoids dequantizing 0s and IDCTing
      if (d[ 8]==0 && d[16]==0 && d[24]==0 && d[32]==0
           && d[40]==0 && d[48]==0 && d[56]==0) {
         //    no shortcut                 0     seconds
         //    (1|2|3|4|5|6|7)==0          0     seconds
         //    all separate               -0.047 seconds
         //    1 && 2|3 && 4|5 && 6|7:    -0.047 seconds
         int dcterm = d[0]*4;
         v[0] = v[8] = v[16] = v[24] = v[32] = v[40] = v[48] = v[56] = dcterm;
      } else {
         STBI__IDCT_1D(d[ 0],d[ 8],d[16],d[24],d[32],d[40],d[48],d[56])
         // constants scaled things up by 1<<12; let's bring them back
         // down, but keep 2 extra bits of precision
         x0 += 512; x1 += 512; x2 += 512; x3 += 512;
         v[ 0] = (x0+t3) >> 10;
         v[56] = (x0-t3) >> 10;
         v[ 8] = (x1+t2) >> 10;
         v[48] = (x1-t2) >> 10;
         v[16] = (x2+t1) >> 10;
         v[40] = (x2-t1) >> 10;
         v[24] = (x3+t0) >> 10;
         v[32] = (x3-t0) >> 10;
      }
   }

   for (i=0, v=val, o=out; i < 8; ++i,v+=8,o+=out_stride) {
      // no fast case since the first 1D IDCT spread components out
      STBI__IDCT_1D(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7])
      // constants scaled things up by 1<<12, plus we had 1<<2 from first
      // loop, plus horizontal and vertical each scale by sqrt(8) so together
      // we've got an extra 1<<3, so 1<<17 total we need to remove.
      // so we want to round that, which means adding 0.5 * 1<<17,
      // aka 65536. Also, we'll end up with -128 to 127 that we want
      // to encode as 0..255 by adding 128, so we'll add that before the shift
      x0 += 65536 + (128<<17);
      x1 += 65536 + (128<<17);
      x2 += 65536 + (128<<17);
      x3 += 65536 + (128<<17);
      // tried computing the shifts into temps, or'ing the temps to see
      // if any were out of range, but that was slower
      o[0] = stbi__clamp((x0+t3) >> 17);
      o[7] = stbi__clamp((x0-t3) >> 17);
      o[1] = stbi__clamp((x1+t2) >> 17);
      o[6] = stbi__clamp((x1-t2) >> 17);
      o[2] = stbi__clamp((x2+t1) >> 17);
      o[5] = stbi__clamp((x2-t1) >> 17);
      o[3] = stbi__clamp((x3+t0) >> 17);
      o[4] = stbi__clamp((x3-t0) >> 17);
   }
}



#define STBI__MARKER_none  0xff
// if there's a pending marker from the entropy stream, return that
// otherwise, fetch from the stream and get a marker. if there's no
// marker, return 0xff, which is never a valid marker value
static stbi_uc stbi__get_marker(stbi__jpeg *j)
{
   stbi_uc x;
   if (j->marker != STBI__MARKER_none) { x = j->marker; j->marker = STBI__MARKER_none; return x; }
   x = stbi__get8(j->s);
   if (x != 0xff) return STBI__MARKER_none;
   while (x == 0xff)
      x = stbi__get8(j->s); // consume repeated 0xff fill bytes
   return x;
}

// in each scan, we'll have scan_n components, and the order
// of the components is specified by order[]
#define STBI__RESTART(x)     ((x) >= 0xd0 && (x) <= 0xd7)

// after a restart interval, stbi__jpeg_reset the entropy decoder and
// the dc prediction
static void stbi__jpeg_reset(stbi__jpeg *j)
{
   j->code_bits = 0;
   j->code_buffer = 0;
   j->more = 0xffffffff;
   j->img_comp[0].dc_pred = j->img_comp[1].dc_pred = j->img_comp[2].dc_pred = j->img_comp[3].dc_pred = 0;
   j->marker = STBI__MARKER_none;
   j->todo = j->restart_interval ? j->restart_interval : 0x7fffffff;
   j->eob_run = 0;
   // no more than 1<<31 MCUs if no restart_interal? that's plenty safe,
   // since we don't even allow 1<<30 pixels
}

/**
 * Parses the entropy-coded data in a JPEG image.
 *
 * This method processes the entropy-coded data in a JPEG image, decoding it into
 * pixel data. It handles both non-interleaved and interleaved scans, as well as
 * progressive and non-progressive JPEGs. The method decodes the data block by block,
 * applying the appropriate Huffman decoding and inverse discrete cosine transform (IDCT)
 * to reconstruct the image.
 *
 * For non-progressive JPEGs, the method processes the data in a straightforward manner,
 * decoding each block and applying the IDCT to produce the final pixel values. For
 * progressive JPEGs, it handles the progressive decoding process, which may involve
 * multiple scans to refine the image.
 *
 * The method also handles restart markers, which are used to allow for error recovery
 * in the JPEG stream. If a restart marker is encountered, the decoder resets its state
 * and continues processing. If a non-restart marker is encountered, the method returns
 * early, indicating that the decoding process should stop.
 *
 * @param z A pointer to the `stbi__jpeg` structure containing the JPEG decoding state.
 * @return Returns 1 if the entropy-coded data was successfully parsed, or 0 if an
 *         error occurred during decoding.
 */
static int stbi__parse_entropy_coded_data(stbi__jpeg *z)
{
   stbi__jpeg_reset(z);
   if (!z->progressive) {
      if (z->scan_n == 1) {
         int i,j;
         STBI_SIMD_ALIGN(short, data[64]);
         int n = z->order[0];
         // non-interleaved data, we just need to process one block at a time,
         // in trivial scanline order
         // number of blocks to do just depends on how many actual "pixels" this
         // component has, independent of interleaved MCU blocking and such
         int w = (z->img_comp[n].x+7) >> 3;
         int h = (z->img_comp[n].y+7) >> 3;
         for (j=0; j < h; ++j) {
            for (i=0; i < w; ++i) {
               int ha = z->img_comp[n].ha;
               if (!stbi__jpeg_decode_block(z, data, z->huff_dc+z->img_comp[n].hd, z->huff_ac+ha, z->fast_ac[ha], n, z->dequant[z->img_comp[n].tq])) return 0;
               stbi__idct_block(z->img_comp[n].data+z->img_comp[n].w2*j*8+i*8, z->img_comp[n].w2, data);
               // every data block is an MCU, so countdown the restart interval
               if (--z->todo <= 0) {
                  if (z->code_bits < 24) stbi__grow_buffer_unsafe(z);
                  // if it's NOT a restart, then just bail, so we get corrupt data
                  // rather than no data
                  if (!STBI__RESTART(z->marker)) return 1;
                  stbi__jpeg_reset(z);
               }
            }
         }
         return 1;
      } else { // interleaved
         int i,j,k,x,y;
         STBI_SIMD_ALIGN(short, data[64]);
         for (j=0; j < z->img_mcu_y; ++j) {
            for (i=0; i < z->img_mcu_x; ++i) {
               // scan an interleaved mcu... process scan_n components in order
               for (k=0; k < z->scan_n; ++k) {
                  int n = z->order[k];
                  img_comp *comp = &z->img_comp[n];
                  int ha = comp->ha;
                  int h  = comp->h;
                  int v  = comp->v;
                  int w2 = comp->w2;
                  int w2_8 = w2 * 8;
                  stbi_uc *data_ptr = comp->data+w2_8*j*v + i*h*8;
                  stbi__huffman *hdc = z->huff_dc+comp->hd;
                  stbi__huffman *hac = z->huff_ac+ha;
                  stbi__int16 *fac = z->fast_ac[ha];
                  stbi__uint16 *dequant = z->dequant[comp->tq];
                  // scan out an mcu's worth of this component; that's just determined
                  // by the basic H and V specified for the component
                  for (y=0; y < v; ++y) {
                     for (x=0; x < h; ++x) {
                        int x2 = x*8;
                        if (!stbi__jpeg_decode_block(z, data, hdc, hac, fac, n, dequant)) return 0;
                        stbi__idct_block(data_ptr+x2, w2, data);
                     }
                    data_ptr += w2_8;
                  }
               }
               // after all interleaved components, that's an interleaved MCU,
               // so now count down the restart interval
               if (--z->todo <= 0) {
                  if (z->code_bits < 24) stbi__grow_buffer_unsafe(z);
                  if (!STBI__RESTART(z->marker)) return 1;
                  stbi__jpeg_reset(z);
               }
            }
         }
         return 1;
      }
   } else {
      if (z->scan_n == 1) {
         int i,j;
         int n = z->order[0];
         // non-interleaved data, we just need to process one block at a time,
         // in trivial scanline order
         // number of blocks to do just depends on how many actual "pixels" this
         // component has, independent of interleaved MCU blocking and such
         int w = (z->img_comp[n].x+7) >> 3;
         int h = (z->img_comp[n].y+7) >> 3;
         for (j=0; j < h; ++j) {
            for (i=0; i < w; ++i) {
               short *data = z->img_comp[n].coeff + 64 * (i + j * z->img_comp[n].coeff_w);
               if (z->spec_start == 0) {
                  if (!stbi__jpeg_decode_block_prog_dc(z, data, &z->huff_dc[z->img_comp[n].hd], n))
                     return 0;
               } else {
                  int ha = z->img_comp[n].ha;
                  if (!stbi__jpeg_decode_block_prog_ac(z, data, &z->huff_ac[ha], z->fast_ac[ha]))
                     return 0;
               }
               // every data block is an MCU, so countdown the restart interval
               if (--z->todo <= 0) {
                  if (z->code_bits < 24) stbi__grow_buffer_unsafe(z);
                  if (!STBI__RESTART(z->marker)) return 1;
                  stbi__jpeg_reset(z);
               }
            }
         }
         return 1;
      } else { // interleaved
         int i,j,k,x,y;
         for (j=0; j < z->img_mcu_y; ++j) {
            for (i=0; i < z->img_mcu_x; ++i) {
               // scan an interleaved mcu... process scan_n components in order
               for (k=0; k < z->scan_n; ++k) {
                  int n = z->order[k];
                  // scan out an mcu's worth of this component; that's just determined
                  // by the basic H and V specified for the component
                  for (y=0; y < z->img_comp[n].v; ++y) {
                     for (x=0; x < z->img_comp[n].h; ++x) {
                        int x2 = (i*z->img_comp[n].h + x);
                        int y2 = (j*z->img_comp[n].v + y);
                        short *data = z->img_comp[n].coeff + 64 * (x2 + y2 * z->img_comp[n].coeff_w);
                        if (!stbi__jpeg_decode_block_prog_dc(z, data, &z->huff_dc[z->img_comp[n].hd], n))
                           return 0;
                     }
                  }
               }
               // after all interleaved components, that's an interleaved MCU,
               // so now count down the restart interval
               if (--z->todo <= 0) {
                  if (z->code_bits < 24) stbi__grow_buffer_unsafe(z);
                  if (!STBI__RESTART(z->marker)) return 1;
                  stbi__jpeg_reset(z);
               }
            }
         }
         return 1;
      }
   }
}

/**
 * Dequantizes the given 8x8 block of DCT coefficients by multiplying each
 * coefficient with its corresponding quantization value.
 *
 * This function is typically used in JPEG decoding to reverse the quantization
 * step applied during compression. Each element in the `data` array, which
 * represents the DCT coefficients of an 8x8 block, is multiplied by the
 * corresponding element in the `dequant` array, which contains the quantization
 * values.
 *
 * @param data     A pointer to an array of 64 short integers representing the
 *                 quantized DCT coefficients of an 8x8 block.
 * @param dequant  A pointer to an array of 64 stbi__uint16 values representing
 *                 the quantization table used to dequantize the coefficients.
 */
static void stbi__jpeg_dequantize(short *data, stbi__uint16 *dequant)
{
   int i;
   for (i=0; i < 64; ++i)
      data[i] *= dequant[i];
}

/**
 * Finalizes the JPEG decoding process, specifically handling progressive JPEG images.
 * If the JPEG image is progressive, this method performs dequantization and inverse
 * discrete cosine transform (IDCT) on the image data. It processes each component
 * of the image (e.g., Y, Cb, Cr) separately, iterating over the blocks of coefficients,
 * dequantizing them, and applying the IDCT to reconstruct the image data.
 *
 * @param z A pointer to the `stbi__jpeg` structure containing the JPEG decoding context,
 *          including the image components, quantization tables, and other relevant data.
 *          The `progressive` flag in this structure determines whether the image is
 *          progressive and requires additional processing.
 *
 * The method operates as follows:
 * 1. Checks if the image is progressive (`z->progressive`).
 * 2. For each image component (e.g., Y, Cb, Cr):
 *    a. Calculates the number of blocks (width and height) in the component.
 *    b. Iterates over each block, retrieves the coefficient data, dequantizes it,
 *       and applies the IDCT to reconstruct the image data.
 * 3. The reconstructed image data is stored in the `data` field of each component.
 */
static void stbi__jpeg_finish(stbi__jpeg *z)
{
   if (z->progressive) {
      // dequantize and idct the data
      int i,j,n;
      for (n=0; n < z->s->img_n; ++n) {
         int w = (z->img_comp[n].x+7) >> 3;
         int h = (z->img_comp[n].y+7) >> 3;
         for (j=0; j < h; ++j) {
            for (i=0; i < w; ++i) {
               short *data = z->img_comp[n].coeff + 64 * (i + j * z->img_comp[n].coeff_w);
               stbi__jpeg_dequantize(data, z->dequant[z->img_comp[n].tq]);
               stbi__idct_block(z->img_comp[n].data+z->img_comp[n].w2*j*8+i*8, z->img_comp[n].w2, data);
            }
         }
      }
   }
}

/**
 * Processes a JPEG marker in the given JPEG context.
 *
 * This function handles various JPEG markers, including:
 * - STBI__MARKER_none: Indicates no marker found, returns an error.
 * - 0xDD (DRI): Specifies the restart interval.
 * - 0xDB (DQT): Defines the quantization table.
 * - 0xC4 (DHT): Defines the Huffman table.
 * - 0xE0-0xEF (APPn): Handles application-specific markers, including JFIF (APP0) and Adobe (APP14).
 * - 0xFE (COM): Handles comment blocks.
 *
 * For each marker, the function performs the necessary operations, such as reading data from the
 * input stream, updating the JPEG context, and validating the marker's integrity. If the marker
 * is invalid or unsupported, the function returns an error.
 *
 * @param z Pointer to the JPEG context.
 * @param m The marker to process.
 * @return Returns 1 on success, 0 on failure (with an error message set).
 */
static int stbi__process_marker(stbi__jpeg *z, int m)
{
   int L;
   switch (m) {
      case STBI__MARKER_none: // no marker found
         return stbi__err("expected marker","Corrupt JPEG");

      case 0xDD: // DRI - specify restart interval
         if (stbi__get16be(z->s) != 4) return stbi__err("bad DRI len","Corrupt JPEG");
         z->restart_interval = stbi__get16be(z->s);
         return 1;

      case 0xDB: // DQT - define quantization table
         L = stbi__get16be(z->s)-2;
         while (L > 0) {
            int q = stbi__get8(z->s);
            int p = q >> 4, sixteen = (p != 0);
            int t = q & 15,i;
            if (p != 0 && p != 1) return stbi__err("bad DQT type","Corrupt JPEG");
            if (t > 3) return stbi__err("bad DQT table","Corrupt JPEG");

            for (i=0; i < 64; ++i)
               z->dequant[t][stbi__jpeg_dezigzag[i]] = (stbi__uint16)(sixteen ? stbi__get16be(z->s) : stbi__get8(z->s));
            L -= (sixteen ? 129 : 65);
         }
         return L==0;

      case 0xC4: // DHT - define huffman table
         L = stbi__get16be(z->s)-2;
         while (L > 0) {
            stbi_uc *v;
            int sizes[16],i,n=0;
            int q = stbi__get8(z->s);
            int tc = q >> 4;
            int th = q & 15;
            if (tc > 1 || th > 3) return stbi__err("bad DHT header","Corrupt JPEG");
            for (i=0; i < 16; ++i) {
               sizes[i] = stbi__get8(z->s);
               n += sizes[i];
            }
            L -= 17;
            if (tc == 0) {
               if (!stbi__build_huffman(z->huff_dc+th, sizes)) return 0;
               v = z->huff_dc[th].values;
            } else {
               if (!stbi__build_huffman(z->huff_ac+th, sizes)) return 0;
               v = z->huff_ac[th].values;
            }
            for (i=0; i < n; ++i)
               v[i] = stbi__get8(z->s);
            if (tc != 0)
               stbi__build_fast_ac(z->fast_ac[th], z->huff_ac + th);
            L -= n;
         }
         return L==0;
   }

   // check for comment block or APP blocks
   if ((m >= 0xE0 && m <= 0xEF) || m == 0xFE) {
      L = stbi__get16be(z->s);
      if (L < 2) {
         if (m == 0xFE)
            return stbi__err("bad COM len","Corrupt JPEG");
         else
            return stbi__err("bad APP len","Corrupt JPEG");
      }
      L -= 2;

      if (m == 0xE0 && L >= 5) { // JFIF APP0 segment
         static const unsigned char tag[5] = {'J','F','I','F','\0'};
         int ok = 1;
         int i;
         for (i=0; i < 5; ++i)
            if (stbi__get8(z->s) != tag[i])
               ok = 0;
         L -= 5;
         if (ok)
            z->jfif = 1;
      } else if (m == 0xEE && L >= 12) { // Adobe APP14 segment
         static const unsigned char tag[6] = {'A','d','o','b','e','\0'};
         int ok = 1;
         int i;
         for (i=0; i < 6; ++i)
            if (stbi__get8(z->s) != tag[i])
               ok = 0;
         L -= 6;
         if (ok) {
            stbi__get8(z->s); // version
            stbi__get16be(z->s); // flags0
            stbi__get16be(z->s); // flags1
            z->app14_color_transform = stbi__get8(z->s); // color transform
            L -= 6;
         }
      }

      stbi__skip(z->s, L);
      return 1;
   }

   return stbi__err("unknown marker","Corrupt JPEG");
}

// after we see SOS
static int stbi__process_scan_header(stbi__jpeg *z)
{
   int i;
   int Ls = stbi__get16be(z->s);
   z->scan_n = stbi__get8(z->s);
   if (z->scan_n < 1 || z->scan_n > 4 || z->scan_n > (int) z->s->img_n) return stbi__err("bad SOS component count","Corrupt JPEG");
   if (Ls != 6+2*z->scan_n) return stbi__err("bad SOS len","Corrupt JPEG");
   for (i=0; i < z->scan_n; ++i) {
      int id = stbi__get8(z->s), which;
      int q = stbi__get8(z->s);
      for (which = 0; which < z->s->img_n; ++which)
         if (z->img_comp[which].id == id)
            break;
      if (which == z->s->img_n) return 0; // no match
      z->img_comp[which].hd = q >> 4;   if (z->img_comp[which].hd > 3) return stbi__err("bad DC huff","Corrupt JPEG");
      z->img_comp[which].ha = q & 15;   if (z->img_comp[which].ha > 3) return stbi__err("bad AC huff","Corrupt JPEG");
      z->order[i] = which;
   }

   {
      int aa;
      z->spec_start = stbi__get8(z->s);
      z->spec_end   = stbi__get8(z->s); // should be 63, but might be 0
      aa = stbi__get8(z->s);
      z->succ_high = (aa >> 4);
      z->succ_low  = (aa & 15);
      if (z->progressive) {
         if (z->spec_start > 63 || z->spec_end > 63  || z->spec_start > z->spec_end || z->succ_high > 13 || z->succ_low > 13)
            return stbi__err("bad SOS", "Corrupt JPEG");
      } else {
         if (z->spec_start != 0) return stbi__err("bad SOS","Corrupt JPEG");
         if (z->succ_high != 0 || z->succ_low != 0) return stbi__err("bad SOS","Corrupt JPEG");
         z->spec_end = 63;
      }
   }

   return 1;
}

/**
 * Frees the memory allocated for the components of a JPEG image.
 * 
 * This function iterates over the specified number of components (`ncomp`) in the
 * JPEG structure (`z`) and frees the memory associated with each component's raw data,
 * raw coefficients, and line buffer. After freeing the memory, the corresponding pointers
 * are set to `NULL` or `0` to avoid dangling pointers.
 *
 * @param z Pointer to the `stbi__jpeg` structure containing the JPEG image components.
 * @param ncomp The number of components to free.
 * @param why An integer value that is returned as-is, typically used to propagate
 *            an error code or status.
 * @return The value of `why`, which is passed through unchanged.
 */
static int stbi__free_jpeg_components(stbi__jpeg *z, int ncomp, int why)
{
   int i;
   for (i=0; i < ncomp; ++i) {
      if (z->img_comp[i].raw_data) {
         STBI_FREE(z->img_comp[i].raw_data);
         z->img_comp[i].raw_data = NULL;
         z->img_comp[i].data = NULL;
      }
      if (z->img_comp[i].raw_coeff) {
         STBI_FREE(z->img_comp[i].raw_coeff);
         z->img_comp[i].raw_coeff = 0;
         z->img_comp[i].coeff = 0;
      }
      if (z->img_comp[i].linebuf) {
         STBI_FREE(z->img_comp[i].linebuf);
         z->img_comp[i].linebuf = NULL;
      }
   }
   return why;
}

/**
 * Processes the frame header of a JPEG image.
 *
 * This function reads and validates the frame header of a JPEG image, ensuring it meets the required
 * specifications for decoding. It extracts critical information such as image dimensions, component
 * count, and sampling factors, and initializes the necessary structures for decoding.
 *
 * @param z Pointer to the stbi__jpeg structure containing the JPEG context.
 * @param scan Indicates the current scan mode (e.g., STBI__SCAN_load).
 *
 * @return Returns 1 on success. On failure, returns an error code and sets the appropriate error
 *         message in the context.
 *
 * The function performs the following steps:
 * 1. Reads and validates the frame header length (Lf).
 * 2. Ensures the image is 8-bit (JPEG baseline).
 * 3. Reads and validates image dimensions (width and height).
 * 4. Checks that the component count is valid (1, 3, or 4).
 * 5. Initializes component data structures.
 * 6. Validates sampling factors (H and V) for each component.
 * 7. Computes interleaved MCU (Minimum Coded Unit) information.
 * 8. Allocates memory for raw data and coefficients (if progressive).
 * 9. Aligns memory blocks for efficient IDCT (Inverse Discrete Cosine Transform) using MMX/SSE.
 *
 * If any validation fails, the function returns an error and sets an appropriate error message.
 */
static int stbi__process_frame_header(stbi__jpeg *z, int scan)
{
   stbi__context *s = z->s;
   int Lf,p,i,q, h_max=1,v_max=1,c;
   Lf = stbi__get16be(s);         if (Lf < 11) return stbi__err("bad SOF len","Corrupt JPEG"); // JPEG
   p  = stbi__get8(s);            if (p != 8) return stbi__err("only 8-bit","JPEG format not supported: 8-bit only"); // JPEG baseline
   s->img_y = stbi__get16be(s);   if (s->img_y == 0) return stbi__err("no header height", "JPEG format not supported: delayed height"); // Legal, but we don't handle it--but neither does IJG
   s->img_x = stbi__get16be(s);   if (s->img_x == 0) return stbi__err("0 width","Corrupt JPEG"); // JPEG requires
   if (s->img_y > STBI_MAX_DIMENSIONS) return stbi__err("too large","Very large image (corrupt?)");
   if (s->img_x > STBI_MAX_DIMENSIONS) return stbi__err("too large","Very large image (corrupt?)");
   c = stbi__get8(s);
   if (c != 3 && c != 1 && c != 4) return stbi__err("bad component count","Corrupt JPEG");
   s->img_n = c;
   for (i=0; i < c; ++i) {
      z->img_comp[i].data = NULL;
      z->img_comp[i].linebuf = NULL;
   }

   if (Lf != 8+3*s->img_n) return stbi__err("bad SOF len","Corrupt JPEG");

   z->rgb = 0;
   for (i=0; i < s->img_n; ++i) {
      static const unsigned char rgb[3] = { 'R', 'G', 'B' };
      z->img_comp[i].id = stbi__get8(s);
      if (s->img_n == 3 && z->img_comp[i].id == rgb[i])
         ++z->rgb;
      q = stbi__get8(s);
      z->img_comp[i].h = (q >> 4);  if (!z->img_comp[i].h || z->img_comp[i].h > 4) return stbi__err("bad H","Corrupt JPEG");
      z->img_comp[i].v = q & 15;    if (!z->img_comp[i].v || z->img_comp[i].v > 4) return stbi__err("bad V","Corrupt JPEG");
      z->img_comp[i].tq = stbi__get8(s);  if (z->img_comp[i].tq > 3) return stbi__err("bad TQ","Corrupt JPEG");
   }

   if (scan != STBI__SCAN_load) return 1;

   if (!stbi__mad3sizes_valid(s->img_x, s->img_y, s->img_n, 极)) return stbi__err("too large", "Image too large to decode");

   for (i=0; i < s->img_n; ++i) {
      if (z->img_comp[i].h > h_max) h_max = z->img_comp[i].h;
      if (z->img_comp[i].v > v_max) v_max = z->img_comp[i].v;
   }

   // compute interleaved mcu info
   z->img_h_max = h_max;
   z->img_v_max = v_max;
   z->img_mcu_w = h_max * 8;
   z->img_mcu_h = v_max * 8;
   // these sizes can't be more than 17 bits
   z->img_mcu_x = (s->img_x + z->img_mcu_w-1) / z->img_mcu_w;
   z->img_mcu_y = (s->img_y + z->img_mcu_h-1) / z->img_mcu_h;

   for (i=0; i < s->img_n; ++i) {
      // number of effective pixels (e.g. for non-interleaved MCU)
      z->img_comp[i].x = (s->img_x * z->img_comp[i].h + h_max-1) / h_max;
      z->img_comp[i].y = (s->img_y * z->img_comp[i].v + v_max-1) / v_max;
      // to simplify generation, we'll allocate enough memory to decode
      // the bogus oversized data from using interleaved MCUs and their
      // big blocks (e.g. a 16x16 iMCU on an image of width 33); we won't
      // discard the extra data until colorspace conversion
      //
      // img_mcu_x, img_mcu_y: <=17 bits; comp[i].h and .v are <=4 (checked earlier)
      // so these muls can't overflow with 32-bit ints (which we require)
      z->img_comp[i].w2 = z->img_mcu_x * z->img_comp[i].h * 8;
      z->img_comp[i].h2 = z->img_mcu_y * z->img_comp[i].v * 8;
      z->img_comp[i].coeff = 0;
      z->img_comp[i].raw_coeff = 0;
      z->img_comp[i].linebuf = NULL;
      z->img_comp[i].raw_data = stbi__malloc_mad2(z->img_comp[i].w2, z->img_comp[i].h2, 15);
      if (z->img_comp[i].raw_data == NULL)
         return stbi__free_jpeg_components(z, i+1, stbi__err("outofmem", "Out of memory"));
      // align blocks for idct using mmx/sse
      z->img_comp[i].data = (stbi_uc*) (((size_t) z->img_comp[i].raw_data + 15) & ~15);
      if (z->progressive) {
         // w2, h2 are multiples of 8 (see above)
         z->img_comp[i].coeff_w = z->img_comp[i].w2 / 8;
         z->img_comp[i].coeff_h = z->img_comp[i].h2 / 8;
         z->img_comp[i].raw_coeff = stbi__malloc_mad3(z->img_comp[i].w2, z->img_comp[i].h2, sizeof(short), 15);
         if (z->img_comp[i].raw_coeff == NULL)
            return stbi__free_jpeg_components(z, i+1, stbi__err("outofmem", "Out of memory"));
         z->img_comp[i].coeff = (short*) (((size_t) z->img_comp[i].raw_coeff + 15) & ~15);
      }
   }

   return 1;
}

// use comparisons since in some cases we handle more than one case (e.g. SOF)
#define stbi__DNL(x)         ((x) == 0xdc)
#define stbi__SOI(x)         ((x) == 0xd8)
#define stbi__EOI(x)         ((x) == 0xd9)
#define stbi__SOF(x)         ((x) == 0xc0 || (x) == 0xc1 || (x) == 0xc2)
#define stbi__SOS(x)         ((x) == 0xda)

#define stbi__SOF_progressive(x)   ((x) == 0xc2)

/**
 * Decodes the header of a JPEG image file.
 *
 * This method initializes the JPEG decoder context, processes the Start of Image (SOI) marker,
 * and then processes subsequent markers until it encounters the Start of Frame (SOF) marker.
 * It handles JFIF and Adobe APP14 markers, and checks for progressive encoding.
 *
 * @param z Pointer to the JPEG decoder context (`stbi__jpeg` structure).
 * @param scan The type of scan to perform. If `STBI__SCAN_type`, the method returns early after
 *             verifying the SOI marker without processing further markers.
 *
 * @return Returns 1 on success. If the SOI marker is missing or the JPEG is corrupt, it returns 0
 *         and sets an error message. If the frame header processing fails, it also returns 0.
 */
static int stbi__decode_jpeg_header(stbi__jpeg *z, int scan)
{
   int m;
   z->jfif = 0;
   z->app14_color_transform = -1; // valid values are 0,1,2
   z->marker = STBI__MARKER_none; // initialize cached marker to empty
   m = stbi__get_marker(z);
   if (!stbi__SOI(m)) return stbi__err("no SOI","Corrupt JPEG");
   if (scan == STBI__SCAN_type) return 1;
   m = stbi__get_marker(z);
   while (!stbi__SOF(m)) {
      if (!stbi__process_marker(z,m)) return 0;
      m = stbi__get_marker(z);
      while (m == STBI__MARKER_none) {
         // some files have extra padding after their blocks, so ok, we'll scan
         if (stbi__at_eof(z->s)) return stbi__err("no SOF", "Corrupt JPEG");
         m = stbi__get_marker(z);
      }
   }
   z->progressive = stbi__SOF_progressive(m);
   if (!stbi__process_frame_header(z, scan)) return 0;
   return 1;
}

// decode image to YCbCr format
static int stbi__decode_jpeg_image(stbi__jpeg *j)
{
   int m;
   for (m = 0; m < 4; m++) {
      j->img_comp[m].raw_data = NULL;
      j->img_comp[m].raw_coeff = NULL;
   }
   j->restart_interval = 0;
   if (!stbi__decode_jpeg_header(j, STBI__SCAN_load)) return 0;
   m = stbi__get_marker(j);
   while (!stbi__EOI(m)) {
      if (stbi__SOS(m)) {
         if (!stbi__process_scan_header(j)) return 0;
         if (!stbi__parse_entropy_coded_data(j)) return 0;
         if (j->marker == STBI__MARKER_none ) {
            // handle 0s at the end of image data from IP Kamera 9060
            while (!stbi__at_eof(j->s)) {
               int x = stbi__get8(j->s);
               if (x == 255) {
                  j->marker = stbi__get8(j->s);
                  break;
               }
            }
            // if we reach eof without hitting a marker, stbi__get_marker() below will fail and we'll eventually return 0
         }
      } else if (stbi__DNL(m)) {
         int Ld = stbi__get16be(j->s);
         stbi__uint32 NL = stbi__get16be(j->s);
         if (Ld != 4) return stbi__err("bad DNL len", "Corrupt JPEG");
         if (NL != j->s->img_y) return stbi__err("bad DNL height", "Corrupt JPEG");
      } else {
         if (!stbi__process_marker(j, m)) return 0;
      }
      m = stbi__get_marker(j);
   }
   if (j->progressive)
      stbi__jpeg_finish(j);
   return 1;
}

// static jfif-centered resampling (across block boundaries)

typedef stbi_uc *(*resample_row_func)(stbi_uc *out, stbi_uc *in0, stbi_uc *in1,
                                    int w, int hs);

#define stbi__div4(x) ((stbi_uc) ((x) >> 2))

/**
 * Resamples a single row of image data by returning the input row as-is.
 * This function is a placeholder or no-op implementation that does not perform any actual resampling.
 * It is typically used in scenarios where resampling is not required or when the input row should be
 * passed through unchanged.
 *
 * @param out      A pointer to the output buffer where the resampled row would be stored. This parameter is unused.
 * @param in_near  A pointer to the input row that is to be resampled. This row is returned as-is.
 * @param in_far   A pointer to a second input row that could be used in more complex resampling algorithms. This parameter is unused.
 * @param w        The width of the row in pixels. This parameter is unused.
 * @param hs       The horizontal scaling factor. This parameter is unused.
 *
 * @return         A pointer to the input row `in_near`, indicating that no resampling was performed.
 */
static stbi_uc *resample_row_1(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   STBI_NOTUSED(out);
   STBI_NOTUSED(in_far);
   STBI_NOTUSED(w);
   STBI_NOTUSED(hs);
   return in_near;
}

/**
 * Resamples a row of image data vertically by a factor of 2.
 * 
 * This method generates two vertical samples for every one input sample by blending
 * the pixel values from two adjacent rows (`in_near` and `in_far`). The blending is
 * performed using a weighted average formula: `(3 * in_near[i] + in_far[i] + 2) / 4`.
 * The result is stored in the output buffer `out`.
 *
 * @param out      The output buffer where the resampled row will be stored.
 * @param in_near  The input buffer representing the "near" row (closer to the output row).
 * @param in_far   The input buffer representing the "far" row (further from the output row).
 * @param w        The width of the row in pixels.
 * @param hs       The horizontal scale factor (unused in this implementation).
 * @return         The output buffer `out` containing the resampled row.
 */
static stbi_uc* stbi__resample_row_v_2(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   // need to generate two samples vertically for every one in input
   int i;
   STBI_NOTUSED(hs);
   for (i=0; i < w; ++i)
      out[i] = stbi__div4(3*in_near[i] + in_far[i] + 2);
   return out;
}

/**
 * Resamples a single row of image data horizontally by a factor of 2.
 * This method generates two output samples for every one input sample,
 * using linear interpolation to compute the intermediate values.
 *
 * @param out      Pointer to the output buffer where the resampled row will be stored.
 * @param in_near  Pointer to the input row of image data to be resampled.
 * @param in_far   Pointer to another input row (unused in this implementation).
 * @param w        The width of the input row (number of input samples).
 * @param hs       Horizontal scaling factor (unused in this implementation).
 *
 * @return         Pointer to the output buffer containing the resampled row.
 *
 * The method handles the following cases:
 * - If the input width `w` is 1, the output is simply duplicated.
 * - For the first and last samples, a weighted average is used to compute the intermediate values.
 * - For the remaining samples, linear interpolation is applied to generate the resampled values.
 */
static stbi_uc*  stbi__resample_row_h_2(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   // need to generate two samples horizontally for every one in input
   int i;
   stbi_uc *input = in_near;

   if (w == 1) {
      // if only one sample, can't do any interpolation
      out[0] = out[1] = input[0];
      return out;
   }

   out[0] = input[0];
   out[1] = stbi__div4(input[0]*3 + input[1] + 2);
   for (i=1; i < w-1; ++i) {
      int n = 3*input[i]+2;
      out[i*2+0] = stbi__div4(n+input[i-1]);
      out[i*2+1] = stbi__div4(n+input[i+1]);
   }
   out[i*2+0] = stbi__div4(input[w-2]*3 + input[w-1] + 2);
   out[i*2+1] = input[w-1];

   STBI_NOTUSED(in_far);
   STBI_NOTUSED(hs);

   return out;
}

#define stbi__div16(x) ((stbi_uc) ((x) >> 4))

/**
 * Resamples a row of pixels horizontally and vertically by a factor of 2.
 * This method generates 2x2 output samples for every input sample, effectively
 * doubling the resolution of the input row. The resampling is performed using
 * a weighted average of the input pixels from the current row (`in_near`) and
 * the adjacent row (`in_far`).
 *
 * @param out      The output buffer where the resampled row will be stored.
 *                 Must have space for at least `2 * w` elements.
 * @param in_near  The input row of pixels to be resampled.
 * @param in_far   The adjacent row of pixels used for vertical interpolation.
 * @param w        The width of the input row in pixels.
 * @param hs       Horizontal scaling factor (unused in this implementation).
 *
 * @return         A pointer to the output buffer (`out`) containing the
 *                 resampled row.
 */
static stbi_uc *stbi__resample_row_hv_2(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   // need to generate 2x2 samples for every one in input
   int i,t0,t1;
   if (w == 1) {
      out[0] = out[1] = stbi__div4(3*in_near[0] + in_far[0] + 2);
      return out;
   }

   t1 = 3*in_near[0] + in_far[0];
   out[0] = stbi__div4(t1+2);
   for (i=1; i < w; ++i) {
      t0 = t1;
      t1 = 3*in_near[i]+in_far[i];
      out[i*2-1] = stbi__div16(3*t0 + t1 + 8);
      out[i*2  ] = stbi__div16(3*t1 + t0 + 8);
   }
   out[w*2-1] = stbi__div4(t1+2);

   STBI_NOTUSED(hs);

   return out;
}


/**
 * Resamples a row of image data using nearest-neighbor interpolation.
 *
 * This function takes an input row of image data (`in_near`) and generates an output row (`out`)
 * by resampling it to a different width or height scale. The resampling is performed using the
 * nearest-neighbor method, which replicates the nearest input pixel value for each output pixel.
 *
 * The `in_far` parameter is present for compatibility but is unused in this implementation.
 *
 * @param out     Pointer to the output buffer where the resampled row will be stored.
 * @param in_near Pointer to the input row of image data to be resampled.
 * @param in_far  Unused parameter, included for compatibility with other resampling methods.
 * @param w       The width of the input row (number of pixels).
 * @param hs      The horizontal scale factor, determining the number of output pixels per input pixel.
 *
 * @return        Returns a pointer to the output buffer (`out`) containing the resampled row.
 */
static stbi_uc *stbi__resample_row_generic(stbi_uc *out, stbi_uc *in_near, stbi_uc *in_far, int w, int hs)
{
   // resample with nearest-neighbor
   int i,j;
   STBI_NOTUSED(in_far);
   for (i=0; i < w; ++i)
      for (j=0; j < hs; ++j)
         out[i*hs+j] = in_near[i];
   return out;
}

// this is a reduced-precision calculation of YCbCr-to-RGB introduced
// to make sure the code produces the same results in both SIMD and scalar
#define stbi__float2fixed(x)  (((int) ((x) * 4096.0f + 0.5f)) << 8)
/**
 * Converts a row of YCbCr pixel data to RGB pixel data.
 *
 * This method processes a row of YCbCr (luma and chroma) data and converts it to RGB format.
 * The conversion is performed using fixed-point arithmetic for efficiency. The method handles
 * a specified number of pixels (`count`) and writes the resulting RGB values to the output buffer.
 * The output buffer is advanced by `step` bytes after each pixel is written, allowing for
 * flexible output formats (e.g., packed RGB or RGBA).
 *
 * @param out   Pointer to the output buffer where the RGB data will be stored.
 * @param y     Pointer to the luma (Y) component of the input YCbCr data.
 * @param pcb   Pointer to the blue-difference chroma (Cb) component of the input YCbCr data.
 * @param pcr   Pointer to the red-difference chroma (Cr) component of the input YCbCr data.
 * @param count The number of pixels to process in the row.
 * @param step  The number of bytes to advance in the output buffer after writing each pixel.
 *              For example, a step of 4 would write RGB values in an RGBA format.
 *
 * The method performs the following steps for each pixel:
 * 1. Scales the luma (Y) value and adds a rounding factor.
 * 2. Adjusts the chroma (Cb and Cr) values by subtracting 128 to center them around zero.
 * 3. Computes the RGB values using fixed-point arithmetic and predefined conversion coefficients.
 * 4. Clamps the RGB values to the range [0, 255] to ensure they are valid 8-bit values.
 * 5. Writes the RGB values to the output buffer and advances the buffer pointer by `step` bytes.
 */
static void stbi__YCbCr_to_RGB_row(stbi_uc *out, const stbi_uc *y, const stbi_uc *pcb, const stbi_uc *pcr, int count, int step)
{
   int i;
   for (i=0; i < count; ++i) {
      int y_fixed = (y[i] << 20) + (1<<19); // rounding
      int r,g,b;
      int cr = pcr[i] - 128;
      int cb = pcb[i] - 128;
      r = y_fixed +  cr* stbi__float2fixed(1.40200f);
      g = y_fixed + (cr*-stbi__float2fixed(0.71414f)) + ((cb*-stbi__float2fixed(0.34414f)) & 0xffff0000);
      b = y_fixed                                     +   cb* stbi__float2fixed(1.77200f);
      r >>= 20;
      g >>= 20;
      b >>= 20;
      if ((unsigned) r > 255) { if (r < 0) r = 0; else r = 255; }
      if ((unsigned) g > 255) { if (g < 0) g = 0; else g = 255; }
      if ((unsigned) b > 255) { if (b < 0) b = 0; else b = 255; }
      out[0] = (stbi_uc)r;
      out[1] = (stbi_uc)g;
      out[2] = (stbi_uc)b;
      out[3] = 255;
      out += step;
   }
}


// clean up the temporary component buffers
static void stbi__cleanup_jpeg(stbi__jpeg *j)
{
   stbi__free_jpeg_components(j, j->s->img_n, 0);
}

typedef struct
{
   resample_row_func resample;
   stbi_uc *line0,*line1;
   int hs,vs;   // expansion factor in each axis
   int w_lores; // horizontal pixels pre-expansion
   int ystep;   // how far through vertical expansion we are
   int ypos;    // which pre-expansion row we're on
} stbi__resample;

// fast 0..255 * 0..255 => 0..255 rounded multiplication
static stbi_uc stbi__blinn_8x8(stbi_uc x, stbi_uc y)
{
   unsigned int t = x*y + 128;
   return (stbi_uc) ((t + (t >>8)) >> 8);
}

/**
 * Loads and decodes a JPEG image from the provided `stbi__jpeg` context, optionally converting it to a specified number of components.
 *
 * This function handles the entire process of loading a JPEG image, including decoding, resampling, and color conversion.
 * It supports various color spaces (e.g., RGB, YCbCr, CMYK) and allows the caller to request a specific number of output
 * components (e.g., 1 for grayscale, 3 for RGB, 4 for RGBA).
 *
 * @param z Pointer to the `stbi__jpeg` context containing the JPEG image data.
 * @param out_x Pointer to an integer where the width of the decoded image will be stored.
 * @param out_y Pointer to an integer where the height of the decoded image will be stored.
 * @param comp Pointer to an integer where the number of components in the original image will be stored.
 * @param req_comp The number of components requested in the output image. If 0, the original number of components is used.
 *
 * @return A pointer to the decoded image data in the requested format. The caller is responsible for freeing this memory.
 *         Returns NULL if an error occurs during decoding or memory allocation.
 */
static stbi_uc *load_jpeg_image(stbi__jpeg *z, int *out_x, int *out_y, int *comp, int req_comp)
{
   int n, decode_n, is_rgb;
   z->s->img_n = 0; // make stbi__cleanup_jpeg safe

   // validate req_comp
   if (req_comp < 0 || req_comp > 4) return stbi__errpuc("bad req_comp", "Internal error");

   // load a jpeg image from whichever source, but leave in YCbCr format
   if (!stbi__decode_jpeg_image(z)) { stbi__cleanup_jpeg(z); return NULL; }

   // determine actual number of components to generate
   n = req_comp ? req_comp : z->s->img_n >= 3 ? 3 : 1;

   is_rgb = z->s->img_n == 3 && (z->rgb == 3 || (z->app14_color_transform == 0 && !z->jfif));

   if (z->s->img_n == 3 && n < 3 && !is_rgb)
      decode_n = 1;
   else
      decode_n = z->s->img_n;

   // resample and color-convert
   {
      int k;
      unsigned int i,j;
      stbi_uc *output;
      stbi_uc *coutput[4] = { NULL, NULL, NULL, NULL };

      stbi__resample res_comp[4];

      for (k=0; k < decode_n; ++k) {
         stbi__resample *r = &res_comp[k];

         // allocate line buffer big enough for upsampling off the edges
         // with upsample factor of 4
         z->img_comp[k].linebuf = (stbi_uc *) stbi__malloc(z->s->img_x + 3);
         if (!z->img_comp[k].linebuf) { stbi__cleanup_jpeg(z); return stbi__errpuc("outofmem", "Out of memory"); }

         r->hs      = z->img_h_max / z->img_comp[k].h;
         r->vs      = z->img_v_max / z->img_comp[k].v;
         r->ystep   = r->vs >> 1;
         r->w_lores = (z->s->img_x + r->hs-1) / r->hs;
         r->ypos    = 0;
         r->line0   = r->line1 = z->img_comp[k].data;

         if      (r->hs == 1 && r->vs == 1) r->resample = resample_row_1;
         else if (r->hs == 1 && r->vs == 2) r->resample = stbi__resample_row_v_2;
         else if (r->hs == 2 && r->vs == 1) r->resample = stbi__resample_row_h_2;
         else if (r->hs == 2 && r->vs == 2) r->resample = stbi__resample_row_hv_2;
         else                               r->resample = stbi__resample_row_generic;
      }

      // can't error after this so, this is safe
      output = (stbi_uc *) stbi__malloc_mad3(n, z->s->img_x, z->s->img_y, 1);
      if (!output) { stbi__cleanup_jpeg(z); return stbi__errpuc("outofmem", "Out of memory"); }

      // now go ahead and resample
      for (j=0; j < z->s->img_y; ++j) {
         stbi_uc *out = output + n * z->s->img_x * j;
         for (k=0; k < decode_n; ++k) {
            stbi__resample *r = &res_comp[k];
            int y_bot = r->ystep >= (r->vs >> 1);
            coutput[k] = r->resample(z->img_comp[k].linebuf,
                                     y_bot ? r->line1 : r->line0,
                                     y_bot ? r->line0 : r->line1,
                                     r->w_lores, r->hs);
            if (++r->ystep >= r->vs) {
               r->ystep = 0;
               r->line0 = r->line1;
               if (++r->ypos < z->img_comp[k].y)
                  r->line1 += z->img_comp[k].w2;
            }
         }
         if (n >= 3) {
            stbi_uc *y = coutput[0];
            if (z->s->img_n == 3) {
               if (is_rgb) {
                  for (i=0; i < z->s->img_x; ++i) {
                     out[0] = y[i];
                     out[1] = coutput[1][i];
                     out[2] = coutput[2][i];
                     out[3] = 255;
                     out += n;
                  }
               } else {
                  stbi__YCbCr_to_RGB_row(out, y, coutput[1], coutput[2], z->s->img_x, n);
               }
            } else if (z->s->img_n == 4) {
               if (z->app14_color_transform == 0) { // CMYK
                  for (i=0; i < z->s->img_x; ++i) {
                     stbi_uc m = coutput[3][i];
                     out[0] = stbi__blinn_8x8(coutput[0][i], m);
                     out[1] = stbi__blinn_8x8(coutput[1][i], m);
                     out[2] = stbi__blinn_8x8(coutput[2][i], m);
                     out[3] = 255;
                     out += n;
                  }
               } else if (z->app14_color_transform == 2) { // YCCK
                  stbi__YCbCr_to_RGB_row(out, y, coutput[1], coutput[2], z->s->img_x, n);
                  for (i=0; i < z->s->img_x; ++i) {
                     stbi_uc m = coutput[3][i];
                     out[0] = stbi__blinn_8x8(255 - out[0], m);
                     out[1] = stbi__blinn_8x8(255 - out[1], m);
                     out[2] = stbi__blinn_8x8(255 - out[2], m);
                     out += n;
                  }
               } else { // YCbCr + alpha?  Ignore the fourth channel for now
                  stbi__YCbCr_to_RGB_row(out, y, coutput[1], coutput[2], z->s->img_x, n);
               }
            } else
               for (i=0; i < z->s->img_x; ++i) {
                  out[0] = out[1] = out[2] = y[i];
                  out[3] = 255; // not used if n==3
                  out += n;
               }
         } else {
            if (is_rgb) {
               if (n == 1)
                  for (i=0; i < z->s->img_x; ++i)
                     *out++ = stbi__compute_y(coutput[0][i], coutput[1][i], coutput[2][i]);
               else {
                  for (i=0; i < z->s->img_x; ++i, out += 2) {
                     out[0] = stbi__compute_y(coutput[0][i], coutput[1][i], coutput[2][i]);
                     out[1] = 255;
                  }
               }
            } else if (z->s->img_n == 4 && z->app14_color_transform == 0) {
               for (i=0; i < z->s->img_x; ++i) {
                  stbi_uc m = coutput[3][i];
                  stbi_uc r = stbi__blinn_8x8(coutput[0][i], m);
                  stbi_uc g = stbi__blinn_8x8(coutput[1][i], m);
                  stbi_uc b = stbi__blinn_8x8(coutput[2][i], m);
                  out[0] = stbi__compute_y(r, g, b);
                  out[1] = 255;
                  out += n;
               }
            } else if (z->s->img_n == 4 && z->app14_color_transform == 2) {
               for (i=0; i < z->s->img_x; ++i) {
                  out[0] = stbi__blinn_8x8(255 - coutput[0][i], coutput[3][i]);
                  out[1] = 255;
                  out += n;
               }
            } else {
               stbi_uc *y = coutput[0];
               if (n == 1)
                  for (i=0; i < z->s->img_x; ++i) out[i] = y[i];
               else
                  for (i=0; i < z->s->img_x; ++i) { *out++ = y[i]; *out++ = 255; }
            }
         }
      }
      stbi__cleanup_jpeg(z);
      *out_x = z->s->img_x;
      *out_y = z->s->img_y;
      if (comp) *comp = z->s->img_n >= 3 ? 3 : 1; // report original components, not output
      return output;
   }
}

/**
 * Loads a JPEG image from the given input context and decodes it into a raw pixel buffer.
 *
 * This function allocates memory for a JPEG decoder context, initializes it with the provided
 * input context, and then decodes the JPEG image into a raw pixel buffer. The dimensions of
 * the image, the number of components (e.g., RGB or grayscale), and the requested number of
 * components in the output buffer are returned via the provided pointers. The function frees
 * the decoder context after decoding is complete.
 *
 * @param s         The input context from which the JPEG image is read.
 * @param x         Pointer to store the width of the decoded image.
 * @param y         Pointer to store the height of the decoded image.
 * @param comp      Pointer to store the number of components in the decoded image.
 * @param req_comp  The number of components requested in the output buffer (e.g., 3 for RGB).
 * @param ri        Pointer to a result info structure (unused in this function).
 *
 * @return          A pointer to the decoded pixel buffer, or NULL if decoding fails.
 *                  The caller is responsible for freeing this buffer using STBI_FREE.
 */
static void *stbi__jpeg_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   unsigned char* result;
   stbi__jpeg* j = (stbi__jpeg*) stbi__malloc(sizeof(stbi__jpeg));
   STBI_NOTUSED(ri);
   j->s = s;
   result = load_jpeg_image(j, x,y,comp,req_comp);
   STBI_FREE(j);
   return result;
}

/**
 * @brief Tests if the given context contains a valid JPEG image.
 *
 * This function attempts to decode the header of a JPEG image from the provided
 * context `s`. It allocates memory for a `stbi__jpeg` structure, associates it
 * with the context, and then attempts to decode the JPEG header. After the
 * header is decoded, the context is rewound to its original state, and the
 * allocated memory is freed. The function returns a non-zero value if the
 * context contains a valid JPEG image, and zero otherwise.
 *
 * @param s Pointer to the `stbi__context` structure containing the image data.
 * @return int Non-zero if the context contains a valid JPEG image, zero otherwise.
 */
static int stbi__jpeg_test(stbi__context *s)
{
   int r;
   stbi__jpeg* j = (stbi__jpeg*)stbi__malloc(sizeof(stbi__jpeg));
   j->s = s;
   r = stbi__decode_jpeg_header(j, STBI__SCAN_type);
   stbi__rewind(s);
   STBI_FREE(j);
   return r;
}

/**
 * Retrieves the basic information about a JPEG image without fully decoding it.
 * This function decodes the JPEG header to extract the image dimensions and 
 * the number of color components. If the header cannot be decoded, the function
 * rewinds the stream and returns 0. Otherwise, it populates the provided pointers
 * with the image width, height, and number of components (either 1 for grayscale
 * or 3 for RGB), and returns 1.
 *
 * @param j    Pointer to the stbi__jpeg structure representing the JPEG image.
 * @param x    Pointer to an integer where the image width will be stored. Can be NULL.
 * @param y    Pointer to an integer where the image height will be stored. Can be NULL.
 * @param comp Pointer to an integer where the number of color components will be stored. Can be NULL.
 * @return     1 if the header was successfully decoded and information was retrieved, 
 *             0 otherwise.
 */
static int stbi__jpeg_info_raw(stbi__jpeg *j, int *x, int *y, int *comp)
{
   if (!stbi__decode_jpeg_header(j, STBI__SCAN_header)) {
      stbi__rewind( j->s );
      return 0;
   }
   if (x) *x = j->s->img_x;
   if (y) *y = j->s->img_y;
   if (comp) *comp = j->s->img_n >= 3 ? 3 : 1;
   return 1;
}

/**
 * Retrieves the dimensions and number of components of a JPEG image without fully decoding it.
 *
 * This function initializes a temporary `stbi__jpeg` structure to parse the JPEG header and extract
 * the image's width, height, and number of components (e.g., RGB or grayscale). It avoids decoding
 * the entire image, making it efficient for obtaining metadata.
 *
 * @param s        Pointer to the `stbi__context` structure representing the input JPEG data stream.
 * @param x        Pointer to an integer where the width of the image will be stored.
 * @param y        Pointer to an integer where the height of the image will be stored.
 * @param comp     Pointer to an integer where the number of components (channels) will be stored.
 * @return         Returns 1 on success, indicating the metadata was successfully extracted.
 *                 Returns 0 on failure, typically due to invalid or unsupported JPEG data.
 */
static int stbi__jpeg_info(stbi__context *s, int *x, int *y, int *comp)
{
   int result;
   stbi__jpeg* j = (stbi__jpeg*) (stbi__malloc(sizeof(stbi__jpeg)));
   j->s = s;
   result = stbi__jpeg_info_raw(j, x, y, comp);
   STBI_FREE(j);
   return result;
}
#endif

// public domain zlib decode    v0.2  Sean Barrett 2006-11-18
//    simple implementation
//      - all input must be provided in an upfront buffer
//      - all output is written to a single output buffer (can malloc/realloc)
//    performance
//      - fast huffman

#ifndef STBI_NO_ZLIB

// fast-way is faster to check than jpeg huffman, but slow way is slower
#define STBI__ZFAST_BITS  9 // accelerate all cases in default tables
#define STBI__ZFAST_MASK  ((1 << STBI__ZFAST_BITS) - 1)

// zlib-style huffman encoding
// (jpegs packs from left, zlib from right, so can't share code)
typedef struct
{
   stbi__uint16 fast[1 << STBI__ZFAST_BITS];
   stbi__uint16 firstcode[16];
   int maxcode[17];
   stbi__uint16 firstsymbol[16];
   stbi_uc  size[288];
   stbi__uint16 value[288];
} stbi__zhuffman;

/**
 * @brief Reverses the bits of a 16-bit integer.
 *
 * This function takes a 16-bit integer and reverses the order of its bits. 
 * The function uses a lookup table (LUT) for efficient bit reversal. The LUT
 * is initialized on the first call and reused in subsequent calls to avoid
 * repeated computation. The LUT is created by iterating over all possible
 * 16-bit values and computing their bit-reversed counterparts using bitwise
 * operations. The function returns the bit-reversed value of the input integer.
 *
 * @param n The 16-bit integer whose bits are to be reversed.
 * @return The 16-bit integer with its bits reversed.
 */
stbi_inline static int stbi__bitreverse16(int n)
{
#if 1
  static unsigned short *lut = NULL;
  if (!lut) {
    lut = malloc(65536 * sizeof(*lut));
    assert(lut);
    int i;
    for (i = 0; i < 65536; i ++) {
      int n = i;
      n = ((n & 0xAAAA) >>  1) | ((n & 0x5555) << 1);
      n = ((n & 0xCCCC) >>  2) | ((n & 0x3333) << 2);
      n = ((n & 0xF0F0) >>  4) | ((n & 0x0F0F) << 4);
      n = ((n & 0xFF00) >>  8) | ((n & 0x00FF) << 8);
      lut[i] = n;
    }
  }
  return lut[(unsigned short)n];
#else
  n = ((n & 0xAAAA) >>  1) | ((n & 0x5555) << 1);
  n = ((n & 0xCCCC) >>  2) | ((n & 0x3333) << 2);
  n = ((n & 0xF0F0) >>  4) | ((n & 0x0F0F) << 4);
  n = ((n & 0xFF00) >>  8) | ((n & 0x00FF) << 8);
  return n;
#endif
}

/**
 * Reverses the bits of a given integer value, limited to a specified number of bits.
 *
 * This function takes an integer value `v` and reverses its bits, considering only the
 * least significant `bits` number of bits. The function asserts that `bits` is less than
 * or equal to 16. The bit reversal is performed by first reversing all 16 bits of the value
 * using `stbi__bitreverse16`, and then shifting the result to discard the excess bits.
 *
 * For example, if `bits` is 11, the function reverses the 11 least significant bits of `v`
 * and shifts the result right by (16 - 11) = 5 bits to remove the unused bits.
 *
 * @param v The integer value whose bits are to be reversed.
 * @param bits The number of bits to consider for reversal. Must be <= 16.
 * @return The bit-reversed value, limited to the specified number of bits.
 */
stbi_inline static int stbi__bit_reverse(int v, int bits)
{
   STBI_ASSERT(bits <= 16);
   // to bit reverse n bits, reverse 16 and shift
   // e.g. 11 bits, bit reverse and shift away 5
   return stbi__bitreverse16(v) >> (16-bits);
}

/**
 * Builds a Huffman table for decoding DEFLATE-compressed data.
 *
 * This function constructs a Huffman table based on the provided size list, which specifies
 * the lengths of the Huffman codes for each symbol. The table is stored in the `stbi__zhuffman`
 * structure for use during decompression.
 *
 * The function follows the DEFLATE specification for generating Huffman codes. It validates
 * the code lengths to ensure they are consistent and do not exceed the maximum allowed length.
 * If the code lengths are invalid, the function returns an error.
 *
 * @param z        Pointer to the `stbi__zhuffman` structure where the Huffman table will be stored.
 * @param sizelist Array of code lengths for each symbol. `sizelist[i]` is the length of the
 *                 Huffman code for symbol `i`.
 * @param num      Number of symbols in the `sizelist` array.
 *
 * @return Returns 1 on success. If the code lengths are invalid, returns 0 and sets an error
 *         message indicating the corruption in the PNG data.
 */
static int stbi__zbuild_huffman(stbi__zhuffman *z, const stbi_uc *sizelist, int num)
{
   int i,k=0;
   int code, next_code[16], sizes[17];

   // DEFLATE spec for generating codes
   memset(sizes, 0, sizeof(sizes));
   memset(z->fast, 0, sizeof(z->fast));
   for (i=0; i < num; ++i)
      ++sizes[sizelist[i]];
   sizes[0] = 0;
   for (i=1; i < 16; ++i)
      if (sizes[i] > (1 << i))
         return stbi__err("bad sizes", "Corrupt PNG");
   code = 0;
   for (i=1; i < 16; ++i) {
      next_code[i] = code;
      z->firstcode[i] = (stbi__uint16) code;
      z->firstsymbol[i] = (stbi__uint16) k;
      code = (code + sizes[i]);
      if (sizes[i])
         if (code-1 >= (1 << i)) return stbi__err("bad codelengths","Corrupt PNG");
      z->maxcode[i] = code << (16-i); // preshift for inner loop
      code <<= 1;
      k += sizes[i];
   }
   z->maxcode[16] = 0x10000; // sentinel
   for (i=0; i < num; ++i) {
      int s = sizelist[i];
      if (s) {
         int c = next_code[s] - z->firstcode[s] + z->firstsymbol[s];
         stbi__uint16 fastv = (stbi__uint16) ((s << 9) | i);
         z->size [c] = (stbi_uc     ) s;
         z->value[c] = (stbi__uint16) i;
         if (s <= STBI__ZFAST_BITS) {
            int j = stbi__bit_reverse(next_code[s],s);
            while (j < (1 << STBI__ZFAST_BITS)) {
               z->fast[j] = fastv;
               j += (1 << s);
            }
         }
         ++next_code[s];
      }
   }
   return 1;
}

// zlib-from-memory implementation for PNG reading
//    because PNG allows splitting the zlib stream arbitrarily,
//    and it's annoying structurally to have PNG call ZLIB call PNG,
//    we require PNG read all the IDATs and combine them into a single
//    memory buffer

typedef struct
{
   stbi_uc *zbuffer, *zbuffer_end;
   int num_bits;
   stbi__uint32 code_buffer;

   char *zout;
   char *zout_start;
   char *zout_end;
   int   z_expandable;

   stbi__zhuffman z_length, z_distance;
} stbi__zbuf;

/**
 * Checks if the end of the buffer has been reached in the given `stbi__zbuf` structure.
 * 
 * This function compares the current position of the buffer pointer (`z->zbuffer`) with the 
 * end of the buffer (`z->zbuffer_end`). If the current position is greater than or equal to 
 * the end of the buffer, it indicates that the end of the buffer has been reached.
 * 
 * @param z Pointer to the `stbi__zbuf` structure containing the buffer and its metadata.
 * @return Returns 1 if the end of the buffer has been reached, otherwise returns 0.
 */
stbi_inline static int stbi__zeof(stbi__zbuf *z)
{
   return (z->zbuffer >= z->zbuffer_end);
}

/**
 * Reads and returns the next byte from the input buffer in the stbi__zbuf structure.
 * This function increments the buffer pointer to the next byte after reading.
 * 
 * @param z Pointer to the stbi__zbuf structure containing the input buffer and its state.
 * @return The next byte (stbi_uc) from the input buffer.
 */
stbi_inline static stbi_uc stbi__zget8(stbi__zbuf *z)
{
   return *z->zbuffer++;
}

/**
 * Fills the code buffer in the stbi__zbuf structure with bits from the input stream.
 * 
 * This method reads bytes from the input stream and appends them to the code buffer,
 * shifting the buffer to accommodate the new bits. The process continues until the
 * code buffer contains at least 25 bits or until the input stream is exhausted.
 * 
 * The method operates in a loop, reading one byte at a time from the input stream
 * using `stbi__zget8` and combining it with the existing bits in the code buffer.
 * The `num_bits` field is incremented by 8 for each byte read, reflecting the
 * number of valid bits in the code buffer.
 * 
 * If the code buffer exceeds its capacity (not explicitly checked in this version),
 * the method would typically treat it as an end-of-file condition and terminate.
 * However, the commented-out code suggests that this check was previously considered
 * but is currently disabled.
 * 
 * @param z Pointer to the stbi__zbuf structure containing the code buffer and
 *          related state information.
 */
static void stbi__fill_bits(stbi__zbuf *z)
{
   do {
      //if (z->code_buffer >= (1U << z->num_bits)) {
      //  z->zbuffer = z->zbuffer_end;  /* treat this as EOF so we fail. */
      //  return;
      //}
      z->code_buffer |= (unsigned int) stbi__zget8(z) << z->num_bits;
      z->num_bits += 8;
   } while (z->num_bits <= 24);
}

stbi_inline static unsigned /**
stbi_inline static unsigned  * Receives `n` bits from the compressed data stream.
stbi_inline static unsigned  *
stbi_inline static unsigned  * This function extracts `n` bits from the code buffer, which is part of the
stbi_inline static unsigned  * decompression process. If there are not enough bits available in the code
stbi_inline static unsigned  * buffer, it first calls `stbi__fill_bits` to refill the buffer. The extracted
stbi_inline static unsigned  * bits are then removed from the code buffer, and the number of available bits
stbi_inline static unsigned  * is decremented by `n`.
stbi_inline static unsigned  *
stbi_inline static unsigned  * @param z Pointer to the `stbi__zbuf` structure containing the code buffer
stbi_inline static unsigned  *          and the number of available bits.
stbi_inline static unsigned  * @param n The number of bits to extract from the code buffer.
stbi_inline static unsigned  * @return The extracted `n` bits as an unsigned integer.
stbi_inline static unsigned  */
stbi_inline static unsigned int stbi__zreceive(stbi__zbuf *z, int n)
/**
 * Extracts 'n' bits from the code buffer and returns them as an unsigned integer.
 * 
 * This method ensures that the code buffer has at least 'n' bits available by calling
 * `stbi__fill_bits` if necessary. It then extracts the 'n' bits from the buffer, updates
 * the buffer by shifting it right by 'n' bits, and decreases the bit count accordingly.
 * 
 * @param z A pointer to the structure containing the code buffer and bit count.
 * @param n The number of bits to extract from the code buffer.
 * @return The extracted 'n' bits as an unsigned integer.
 */
stbi_inline static unsigned int stbi__get_bits(stbi__zbuf *z, int n) {
    unsigned int k;
    if (z->num_bits < n) stbi__fill_bits(z);
    k = z->code_buffer & ((1 << n) - 1);
    z->code_buffer >>= n;
    z->num_bits -= n;
    return k;
}

/**
 * Decodes a Huffman code from the input buffer using a slow path approach.
 * This method is used when the code cannot be resolved by the fast table lookup.
 * It employs a bit-reversal technique and a loop to determine the code size 's'.
 * The method then calculates the symbol index 'b' using the code size and the Huffman table.
 * If the calculated symbol index is out of bounds or the code size does not match the expected size,
 * the method returns -1 indicating an error (invalid or corrupt data).
 * If successful, the method updates the code buffer and the number of remaining bits,
 * and returns the decoded symbol value.
 *
 * @param a Pointer to the input buffer containing the encoded data.
 * @param z Pointer to the Huffman table used for decoding.
 * @return The decoded symbol value if successful, or -1 if an error occurs (invalid code or corrupt data).
 */
static int stbi__zhuffman_decode_slowpath(stbi__zbuf *a, stbi__zhuffman *z)
{
   int b,s,k;
   // not resolved by fast table, so compute it the slow way
   // use jpeg approach, which requires MSbits at top
   k = stbi__bit_reverse(a->code_buffer, 16);
   for (s=STBI__ZFAST_BITS+1; ; ++s)
      if (k < z->maxcode[s])
         break;
   if (s >= 16) return -1; // invalid code!
   // code size is s, so:
   b = (k >> (16-s)) - z->firstcode[s] + z->firstsymbol[s];
   if (b >= sizeof (z->size)) return -1; // some data was corrupt somewhere!
   if (z->size[b] != s) return -1;  // was originally an assert, but report failure instead.
   a->code_buffer >>= s;
   a->num_bits -= s;
   return z->value[b];
}

/**
 * Decodes a Huffman-encoded symbol from the input buffer using the provided Huffman table.
 * This function attempts to decode a symbol quickly using a fast lookup table. If the fast
 * lookup fails, it falls back to a slower, more comprehensive decoding process.
 *
 * @param a Pointer to the input buffer (`stbi__zbuf`) containing the compressed data.
 * @param z Pointer to the Huffman table (`stbi__zhuffman`) used for decoding.
 * @return The decoded symbol as an integer. If the end of the input buffer is reached
 *         unexpectedly, the function returns -1 to indicate an error.
 *
 * The function first checks if there are enough bits in the buffer to perform decoding. If not,
 * it attempts to fill the buffer with more bits. It then uses the fast lookup table to decode
 * the symbol. If the fast lookup is successful, it adjusts the buffer and returns the decoded
 * symbol. If the fast lookup fails, it calls `stbi__zhuffman_decode_slowpath` for a more
 * thorough decoding process.
 */
stbi_inline static int stbi__zhuffman_decode(stbi__zbuf *a, stbi__zhuffman *z)
{
   int b,s;
   if (a->num_bits < 16) {
      if (stbi__zeof(a)) {
         return -1;   /* report error for unexpected end of data. */
      }
      stbi__fill_bits(a);
   }
   b = z->fast[a->code_buffer & STBI__ZFAST_MASK];
   if (b) {
      s = b >> 9;
      a->code_buffer >>= s;
      a->num_bits -= s;
      return b & 511;
   }
   return stbi__zhuffman_decode_slowpath(a, z);
}

/**
 * Expands the output buffer to accommodate `n` additional bytes.
 * 
 * This function ensures that the output buffer has enough space to store `n` more bytes.
 * If the buffer is not expandable, it returns an error indicating a corrupt PNG. Otherwise,
 * it calculates the current position within the buffer and the current buffer limit. If the
 * required space exceeds the buffer limit, the buffer is dynamically resized by doubling its
 * size until it can accommodate the additional bytes. If memory allocation fails, an out-of-memory
 * error is returned. Upon successful expansion, the buffer pointers are updated to reflect the
 * new buffer size and the current position.
 *
 * @param z Pointer to the `stbi__zbuf` structure containing the buffer and related state.
 * @param zout Pointer to the current position in the output buffer.
 * @param n Number of additional bytes needed in the output buffer.
 * @return Returns 1 on success, or an error code if the buffer is not expandable or if memory allocation fails.
 */
static int stbi__zexpand(stbi__zbuf *z, char *zout, int n)  // need to make room for n bytes
{
   char *q;
   unsigned int cur, limit, old_limit;
   z->zout = zout;
   if (!z->z_expandable) return stbi__err("output buffer limit","Corrupt PNG");
   cur   = (unsigned int) (z->zout - z->zout_start);
   limit = old_limit = (unsigned) (z->zout_end - z->zout_start);
   if (UINT_MAX - cur < (unsigned) n) return stbi__err("outofmem", "Out of memory");
   while (cur + n > limit) {
      if(limit > UINT_MAX / 2) return stbi__err("outofmem", "Out of memory");
      limit *= 2;
   }
   q = (char *) STBI_REALLOC_SIZED(z->zout_start, old_limit, limit);
   STBI_NOTUSED(old_limit);
   if (q == NULL) return stbi__err("outofmem", "Out of memory");
   z->zout_start = q;
   z->zout       = q + cur;
   z->zout_end   = q + limit;
   return 1;
}

static const int stbi__zlength_base[31] = {
   3,4,5,6,7,8,9,10,11,13,
   15,17,19,23,27,31,35,43,51,59,
   67,83,99,115,131,163,195,227,258,0,0 };

static const int stbi__zlength_extra[31]=
{ 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,0,0 };

static const int stbi__zdist_base[32] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,0,0};

static const int stbi__zdist_extra[32] =
{ 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

/**
 * Decodes a Huffman-encoded block from the input buffer and writes the decoded data to the output buffer.
 *
 * This method processes a Huffman-encoded block using the provided zlib buffer (`stbi__zbuf`). It decodes
 * symbols using the Huffman tree and handles two types of symbols:
 * - Literal bytes (0-255): These are directly written to the output buffer.
 * - Length-distance pairs (257-285): These represent a sequence of bytes that should be copied from a
 *   previously decoded position in the output buffer.
 *
 * The method expands the output buffer dynamically if it runs out of space. It handles overlapping regions
 * when copying data from previous positions in the output buffer.
 *
 * @param a Pointer to the zlib buffer (`stbi__zbuf`) containing the input data and output buffer state.
 * @return Returns 1 on successful decoding of the block, 0 if the output buffer expansion fails, or an
 *         error code if the Huffman codes or distances are invalid.
 */
static int stbi__parse_huffman_block(stbi__zbuf *a)
{
   char *zout = a->zout;
   for(;;) {
      int z = stbi__zhuffman_decode(a, &a->z_length);
      if (z < 256) {
         if (z < 0) return stbi__err("bad huffman code","Corrupt PNG"); // error in huffman codes
         if (zout >= a->zout_end) {
            if (!stbi__zexpand(a, zout, 1)) return 0;
            zout = a->zout;
         }
         *zout++ = (char) z;
      } else {
         stbi_uc *p;
         int len,dist;
         if (z == 256) {
            a->zout = zout;
            return 1;
         }
         z -= 257;
         len = stbi__zlength_base[z];
         if (stbi__zlength_extra[z]) len += stbi__zreceive(a, stbi__zlength_extra[z]);
         z = stbi__zhuffman_decode(a, &a->z_distance);
         if (z < 0) return stbi__err("bad huffman code","Corrupt PNG");
         dist = stbi__zdist_base[z];
         if (stbi__zdist_extra[z]) dist += stbi__zreceive(a, stbi__zdist_extra[z]);
         if (zout - a->zout_start < dist) return stbi__err("bad dist","Corrupt PNG");
         if (zout + len > a->zout_end) {
            if (!stbi__zexpand(a, zout, len)) return 0;
            zout = a->zout;
         }
         p = (stbi_uc *) (zout - dist);

         if (len) {
           if (dist == 1) { // run of one byte; common in images.
             memset(zout, *p, len);
             zout += len;
           } else {
             if (p + len >= (stbi_uc *)zout) {
               // overlap region
               do *zout++ = *p++; while (--len);
             } else {
               memcpy(zout, p, len);
               zout += len;
             }
           }
         }
      }
   }
}

/**
 * Computes Huffman codes for the given compressed data buffer.
 *
 * This method reads the Huffman code lengths from the compressed data buffer `a` and constructs
 * the corresponding Huffman trees for literal/length and distance codes. The method follows the
 * DEFLATE algorithm specification for decoding Huffman codes.
 *
 * The method performs the following steps:
 * 1. Reads the number of literal/length codes (hlit), distance codes (hdist), and code length codes (hclen).
 * 2. Initializes the code length sizes array using a predefined dezigzag order.
 * 3. Builds a Huffman tree for the code lengths.
 * 4. Decodes the literal/length and distance codes using the code length Huffman tree.
 * 5. Validates the decoded codes and constructs the final Huffman trees for literal/length and distance codes.
 *
 * @param a Pointer to the compressed data buffer (`stbi__zbuf` structure).
 * @return Returns 1 on success, 0 on failure (e.g., invalid data or corrupted PNG).
 */
static int stbi__compute_huffman_codes(stbi__zbuf *a)
{
   static const stbi_uc length_dezigzag[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
   stbi__zhuffman z_codelength;
   stbi_uc lencodes[286+32+137];//padding for maximum single op
   stbi_uc codelength_sizes[19];
   int i,n;

   int hlit  = stbi__zreceive(a,5) + 257;
   int hdist = stbi__zreceive(a,5) + 1;
   int hclen = stbi__zreceive(a,4) + 4;
   int ntot  = hlit + hdist;

   memset(codelength_sizes, 0, sizeof(codelength_sizes));
   for (i=0; i < hclen; ++i) {
      int s = stbi__zreceive(a,3);
      codelength_sizes[length_dezigzag[i]] = (stbi_uc) s;
   }
   if (!stbi__zbuild_huffman(&z_codelength, codelength_sizes, 19)) return 0;

   n = 0;
   while (n < ntot) {
      int c = stbi__zhuffman_decode(a, &z_codelength);
      if (c < 0 || c >= 19) return stbi__err("bad codelengths", "Corrupt PNG");
      if (c < 16)
         lencodes[n++] = (stbi_uc) c;
      else {
         stbi_uc fill = 0;
         if (c == 16) {
            c = stbi__zreceive(a,2)+3;
            if (n == 0) return stbi__err("bad codelengths", "Corrupt PNG");
            fill = lencodes[n-1];
         } else if (c == 17) {
            c = stbi__zreceive(a,3)+3;
         } else if (c == 18) {
            c = stbi__zreceive(a,7)+11;
         } else {
            return stbi__err("bad codelengths", "Corrupt PNG");
         }
         if (ntot - n < c) return stbi__err("bad codelengths", "Corrupt PNG");
         memset(lencodes+n, fill, c);
         n += c;
      }
   }
   if (n != ntot) return stbi__err("bad codelengths","Corrupt PNG");
   if (!stbi__zbuild_huffman(&a->z_length, lencodes, hlit)) return 0;
   if (!stbi__zbuild_huffman(&a->z_distance, lencodes+hlit, hdist)) return 0;
   return 1;
}

/**
 * Parses an uncompressed block of data from a zlib stream.
 *
 * This method reads an uncompressed block from the zlib stream and copies it to the output buffer.
 * The block is expected to have a specific format: a 4-byte header containing the length of the
 * uncompressed data and its one's complement. The method verifies the integrity of the block by
 * checking the one's complement of the length. If the block is valid, the data is copied to the
 * output buffer, and the buffer pointers are updated accordingly.
 *
 * @param a Pointer to the zlib buffer structure containing the input data and output buffer.
 * @return Returns 1 if the block was successfully parsed and copied, 0 if an error occurred
 *         (e.g., corrupt data or insufficient buffer space), or an error code if the block is invalid.
 *
 * @note This method assumes that the zlib buffer structure (`stbi__zbuf`) is properly initialized
 *       and that the input data is in a valid zlib format. If the block is corrupted or the buffer
 *       is too small to hold the uncompressed data, an error is returned.
 */
static int stbi__parse_uncompressed_block(stbi__zbuf *a)
{
   stbi_uc header[4];
   int len,nlen,k;
   if (a->num_bits & 7)
      stbi__zreceive(a, a->num_bits & 7); // discard
   // drain the bit-packed data into header
   k = 0;
   while (a->num_bits > 0) {
      header[k++] = (stbi_uc) (a->code_buffer & 255); // suppress MSVC run-time check
      a->code_buffer >>= 8;
      a->num_bits -= 8;
   }
   if (a->num_bits < 0) return stbi__err("zlib corrupt","Corrupt PNG");
   // now fill header the normal way
   while (k < 4)
      header[k++] = stbi__zget8(a);
   len  = header[1] * 256 + header[0];
   nlen = header[3] * 256 + header[2];
   if (nlen != (len ^ 0xffff)) return stbi__err("zlib corrupt","Corrupt PNG");
   if (a->zbuffer + len > a->zbuffer_end) return stbi__err("read past buffer","Corrupt PNG");
   if (a->zout + len > a->zout_end)
      if (!stbi__zexpand(a, a->zout, len)) return 0;
   memcpy(a->zout, a->zbuffer, len);
   a->zbuffer += len;
   a->zout += len;
   return 1;
}

/**
 * Parses the zlib header from the given zlib buffer and validates its contents.
 *
 * This method reads the first two bytes of the zlib buffer, which represent the
 * CMF (Compression Method and Flags) and FLG (Flags) fields. It then performs
 * the following validations:
 * 1. Ensures the buffer is not at EOF.
 * 2. Validates the header checksum by ensuring (CMF * 256 + FLG) is divisible by 31.
 * 3. Ensures no preset dictionary is used (FLG bit 5 is not set).
 * 4. Ensures the compression method is DEFLATE (CM == 8).
 *
 * If any validation fails, an error is returned. Otherwise, the method returns 1
 * to indicate a valid zlib header.
 *
 * @param a Pointer to the zlib buffer to parse.
 * @return 1 if the header is valid, otherwise an error code indicating the failure.
 */
static int stbi__parse_zlib_header(stbi__zbuf *a)
{
   int cmf   = stbi__zget8(a);
   int cm    = cmf & 15;
   /* int cinfo = cmf >> 4; */
   int flg   = stbi__zget8(a);
   if (stbi__zeof(a)) return stbi__err("bad zlib header","Corrupt PNG"); // zlib spec
   if ((cmf*256+flg) % 31 != 0) return stbi__err("bad zlib header","Corrupt PNG"); // zlib spec
   if (flg & 32) return stbi__err("no preset dict","Corrupt PNG"); // preset dictionary not allowed in png
   if (cm != 8) return stbi__err("bad compression","Corrupt PNG"); // DEFLATE required for png
   // window = 1 << (8 + cinfo)... but who cares, we fully buffer output
   return 1;
}

static const stbi_uc stbi__zdefault_length[288] =
{
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
   8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, 9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
   7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, 7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8
};
static const stbi_uc stbi__zdefault_distance[32] =
{
   5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5
};
/*
Init algorithm:
{
   int i;   // use <= to match clearly with spec
   for (i=0; i <= 143; ++i)     stbi__zdefault_length[i]   = 8;
   for (   ; i <= 255; ++i)     stbi__zdefault_length[i]   = 9;
   for (   ; i <= 279; ++i)     stbi__zdefault_length[i]   = 7;
   for (   ; i <= 287; ++i)     stbi__zdefault_length[i]   = 8;

   for (i=0; i <=  31; ++i)     stbi__zdefault_distance[i] = 5;
}
*/

static int stbi__parse_zlib(stbi__zbuf *a, int parse_header)
{
   int final, type;
   if (parse_header)
      if (!stbi__parse_zlib_header(a)) return 0;
   a->num_bits = 0;
   a->code_buffer = 0;
   do {
      final = stbi__zreceive(a,1);
      type = stbi__zreceive(a,2);
      if (type == 0) {
         if (!stbi__parse_uncompressed_block(a)) return 0;
      } else if (type == 3) {
         return 0;
      } else {
         if (type == 1) {
            // use fixed code lengths
            if (!stbi__zbuild_huffman(&a->z_length  , stbi__zdefault_length  , 288)) return 0;
            if (!stbi__zbuild_huffman(&a->z_distance, stbi__zdefault_distance,  32)) return 0;
         } else {
            if (!stbi__compute_huffman_codes(a)) return 0;
         }
         if (!stbi__parse_huffman_block(a)) return 0;
      }
   } while (!final);
   return 1;
}

/**
 * @brief Decompresses zlib-compressed data into the provided output buffer.
 *
 * This function initializes the zlib buffer (`stbi__zbuf`) with the provided output buffer
 * and its length, and then proceeds to parse and decompress the zlib data. The function
 * supports optional header parsing based on the `parse_header` parameter.
 *
 * @param a Pointer to the zlib buffer (`stbi__zbuf`) used for decompression.
 * @param obuf Pointer to the output buffer where the decompressed data will be stored.
 * @param olen Length of the output buffer.
 * @param exp Flag indicating whether the output buffer is expandable (non-zero if expandable).
 * @param parse_header Flag indicating whether to parse the zlib header (non-zero to parse).
 *
 * @return Returns the result of the zlib parsing operation, typically indicating success or failure.
 */
static int stbi__do_zlib(stbi__zbuf *a, char *obuf, int olen, int exp, int parse_header)
{
   a->zout_start = obuf;
   a->zout       = obuf;
   a->zout_end   = obuf + olen;
   a->z_expandable = exp;

   return stbi__parse_zlib(a, parse_header);
}

/**
 * @brief Decodes a zlib-compressed buffer into a newly allocated memory block.
 *
 * This function takes a zlib-compressed buffer and decodes it into a newly allocated
 * memory block. The function guesses the initial size of the output buffer based on
 * the provided `initial_size` parameter. If the decoding is successful, the function
 * returns a pointer to the decoded data and optionally provides the length of the
 * decoded data via the `outlen` parameter. If the decoding fails, the function returns
 * NULL and frees any allocated memory.
 *
 * @param buffer Pointer to the zlib-compressed input buffer.
 * @param len Length of the input buffer in bytes.
 * @param initial_size Initial guess for the size of the output buffer.
 * @param outlen Pointer to an integer where the length of the decoded data will be
 *               stored. Can be NULL if the length is not needed.
 *
 * @return A pointer to the decoded data if successful, or NULL if the decoding fails.
 *         The caller is responsible for freeing the returned memory using STBI_FREE.
 */
STBIDEF char *stbi_zlib_decode_malloc_guesssize(const char *buffer, int len, int initial_size, int *outlen)
{
   stbi__zbuf a;
   char *p = (char *) stbi__malloc(initial_size);
   if (p == NULL) return NULL;
   a.zbuffer = (stbi_uc *) buffer;
   a.zbuffer_end = (stbi_uc *) buffer + len;
   if (stbi__do_zlib(&a, p, initial_size, 1, 1)) {
      if (outlen) *outlen = (int) (a.zout - a.zout_start);
      return a.zout_start;
   } else {
      STBI_FREE(a.zout_start);
      return NULL;
   }
}

/**
 * Decodes a zlib-compressed buffer into a newly allocated memory block.
 *
 * This function takes a zlib-compressed buffer and decodes it into a newly allocated
 * memory block. The caller is responsible for freeing the returned memory block using
 * `STBI_FREE()` or a compatible memory deallocation function.
 *
 * @param buffer Pointer to the zlib-compressed input buffer.
 * @param len Length of the input buffer in bytes.
 * @param outlen Pointer to an integer where the length of the decoded output will be stored.
 *               This value is set by the function upon successful decoding.
 *
 * @return A pointer to the decoded data, or NULL if decoding fails or memory allocation fails.
 *         If NULL is returned, `outlen` will not be modified.
 */
STBIDEF char *stbi_zlib_decode_malloc(char const *buffer, int len, int *outlen)
{
   return stbi_zlib_decode_malloc_guesssize(buffer, len, 16384, outlen);
}

/**
 * @brief Decodes a zlib-compressed buffer into a newly allocated memory block.
 *
 * This function decodes a zlib-compressed buffer and stores the result in a newly allocated
 * memory block. The caller can specify an initial size for the output buffer, and the function
 * will dynamically resize it if necessary. The function also allows for optional parsing of
 * the zlib header.
 *
 * @param buffer Pointer to the zlib-compressed input buffer.
 * @param len Length of the input buffer in bytes.
 * @param initial_size Initial size of the output buffer. If the decoded data exceeds this size,
 *                     the buffer will be resized.
 * @param outlen Pointer to an integer where the length of the decoded data will be stored.
 *               If NULL, the length will not be returned.
 * @param parse_header Flag indicating whether to parse the zlib header. If non-zero, the header
 *                     will be parsed; if zero, it will be skipped.
 *
 * @return On success, returns a pointer to the decoded data. The caller is responsible for
 *         freeing this memory using STBI_FREE. On failure, returns NULL.
 *
 * @note The function allocates memory for the output buffer using stbi__malloc. If the decoding
 *       process fails, any allocated memory is freed automatically.
 */
STBIDEF char *stbi_zlib_decode_malloc_guesssize_headerflag(const char *buffer, int len, int initial_size, int *outlen, int parse_header)
{
   stbi__zbuf a;
   char *p = (char *) stbi__malloc(initial_size);
   if (p == NULL) return NULL;
   a.zbuffer = (stbi_uc *) buffer;
   a.zbuffer_end = (stbi_uc *) buffer + len;
   if (stbi__do_zlib(&a, p, initial_size, 1, parse_header)) {
      if (outlen) *outlen = (int) (a.zout - a.zout_start);
      return a.zout_start;
   } else {
      STBI_FREE(a.zout_start);
      return NULL;
   }
}

/**
 * Decodes a zlib-compressed buffer into an output buffer.
 *
 * This function takes a zlib-compressed input buffer (`ibuffer`) and decodes it into
 * the provided output buffer (`obuffer`). The function returns the number of bytes
 * written to the output buffer on success, or -1 on failure.
 *
 * @param obuffer Pointer to the output buffer where the decompressed data will be stored.
 * @param olen    The size of the output buffer in bytes.
 * @param ibuffer Pointer to the input buffer containing the zlib-compressed data.
 * @param ilen    The size of the input buffer in bytes.
 *
 * @return The number of bytes written to the output buffer if successful, or -1 if
 *         decompression fails (e.g., due to invalid input data or insufficient output space).
 *
 * @note The function uses the `stbi__do_zlib` internal function to perform the actual
 *       decompression. The output buffer must be large enough to hold the decompressed data.
 */
STBIDEF int stbi_zlib_decode_buffer(char *obuffer, int olen, char const *ibuffer, int ilen)
{
   stbi__zbuf a;
   a.zbuffer = (stbi_uc *) ibuffer;
   a.zbuffer_end = (stbi_uc *) ibuffer + ilen;
   if (stbi__do_zlib(&a, obuffer, olen, 0, 1))
      return (int) (a.zout - a.zout_start);
   else
      return -1;
}

/**
 * @brief Decodes a zlib-compressed buffer without a header and allocates memory for the result.
 *
 * This function takes a buffer containing zlib-compressed data (without a header) and decodes it.
 * The decoded data is stored in a newly allocated memory block. The caller is responsible for freeing
 * the returned memory using `STBI_FREE`.
 *
 * @param buffer Pointer to the buffer containing the zlib-compressed data.
 * @param len Length of the compressed data in the buffer.
 * @param outlen Pointer to an integer where the length of the decoded data will be stored.
 *              If NULL, the length is not returned.
 *
 * @return A pointer to the decoded data on success, or NULL if the decoding fails or memory allocation fails.
 *         The caller must free the returned pointer using `STBI_FREE`.
 */
STBIDEF char *stbi_zlib_decode_noheader_malloc(char const *buffer, int len, int *outlen)
{
   stbi__zbuf a;
   char *p = (char *) stbi__malloc(16384);
   if (p == NULL) return NULL;
   a.zbuffer = (stbi_uc *) buffer;
   a.zbuffer_end = (stbi_uc *) buffer+len;
   if (stbi__do_zlib(&a, p, 16384, 1, 0)) {
      if (outlen) *outlen = (int) (a.zout - a.zout_start);
      return a.zout_start;
   } else {
      STBI_FREE(a.zout_start);
      return NULL;
   }
}

/**
 * Decodes a zlib-compressed buffer without a header into an output buffer.
 *
 * This function takes a zlib-compressed input buffer and decodes it into the provided output buffer.
 * The function does not expect or handle a zlib header, making it suitable for raw deflate streams.
 *
 * @param obuffer Pointer to the output buffer where the decompressed data will be stored.
 * @param olen    The size of the output buffer in bytes.
 * @param ibuffer Pointer to the input buffer containing the zlib-compressed data.
 * @param ilen    The size of the input buffer in bytes.
 *
 * @return The number of bytes written to the output buffer if successful. 
 *         If the decompression fails, the function returns -1.
 *
 * @note The function assumes the input buffer contains a valid zlib-compressed stream without a header.
 *       Ensure the output buffer is large enough to hold the decompressed data to avoid buffer overflow.
 */
STBIDEF int stbi_zlib_decode_noheader_buffer(char *obuffer, int olen, const char *ibuffer, int ilen)
{
   stbi__zbuf a;
   a.zbuffer = (stbi_uc *) ibuffer;
   a.zbuffer_end = (stbi_uc *) ibuffer + ilen;
   if (stbi__do_zlib(&a, obuffer, olen, 0, 0))
      return (int) (a.zout - a.zout_start);
   else
      return -1;
}
#endif

// public domain "baseline" PNG decoder   v0.10  Sean Barrett 2006-11-18
//    simple implementation
//      - only 8-bit samples
//      - no CRC checking
//      - allocates lots of intermediate memory
//        - avoids problem of streaming data between subsystems
//        - avoids explicit window management
//    performance
//      - uses stb_zlib, a PD zlib implementation with fast huffman decoding

#ifndef STBI_NO_PNG
typedef struct
{
   stbi__uint32 length;
   stbi__uint32 type;
} stbi__pngchunk;

/**
 * Reads and returns the header of a PNG chunk from the given context.
 * 
 * A PNG chunk header consists of two parts:
 * - A 4-byte length field, which specifies the length of the chunk's data (not including the header itself).
 * - A 4-byte type field, which identifies the type of the chunk (e.g., IHDR, IDAT, IEND).
 * 
 * This function reads these two fields from the provided `stbi__context` and stores them in an `stbi__pngchunk` structure.
 * The length and type fields are read in big-endian format, as specified by the PNG standard.
 * 
 * @param s Pointer to the `stbi__context` from which the chunk header is read.
 * @return An `stbi__pngchunk` structure containing the length and type of the chunk.
 */
static stbi__pngchunk stbi__get_chunk_header(stbi__context *s)
{
   stbi__pngchunk c;
   c.length = stbi__get32be(s);
   c.type   = stbi__get32be(s);
   return c;
}

/**
 * @brief Checks if the given stbi__context contains a valid PNG header.
 *
 * This function reads the first 8 bytes from the provided stbi__context and compares them
 * against the standard PNG signature. The PNG signature is a sequence of 8 bytes: 
 * 137, 80, 78, 71, 13, 10, 26, 10. If the bytes match the signature, the function returns 1,
 * indicating that the file is a valid PNG. If the bytes do not match, the function returns an
 * error using stbi__err with the message "bad png sig" and "Not a PNG".
 *
 * @param s Pointer to the stbi__context containing the file data to be checked.
 * @return int Returns 1 if the header is a valid PNG signature, otherwise returns an error code.
 */
static int stbi__check_png_header(stbi__context *s)
{
   static const stbi_uc png_sig[8] = { 137,80,78,71,13,10,26,10 };
   int i;
   for (i=0; i < 8; ++i)
      if (stbi__get8(s) != png_sig[i]) return stbi__err("bad png sig","Not a PNG");
   return 1;
}

typedef struct
{
   stbi__context *s;
   stbi_uc *idata, *expanded, *out;
   int depth;
} stbi__png;


enum {
   STBI__F_none=0,
   STBI__F_sub=1,
   STBI__F_up=2,
   STBI__F_avg=3,
   STBI__F_paeth=4,
   // synthetic filters used for first scanline to avoid needing a dummy row of 0s
   STBI__F_avg_first,
   STBI__F_paeth_first
};

static stbi_uc first_row_filter[5] =
{
   STBI__F_none,
   STBI__F_sub,
   STBI__F_none,
   STBI__F_avg_first,
   STBI__F_paeth_first
};

/**
 * Computes the Paeth predictor for a given set of three values (a, b, c).
 * The Paeth predictor is used in PNG image filtering to predict the value of a pixel
 * based on its neighboring pixels. The method calculates the differences between the
 * values and selects the value (a, b, or c) that minimizes the prediction error.
 *
 * The algorithm works as follows:
 * 1. Compute the differences t0 = b - c and t1 = a - c.
 * 2. Compute t2 = t0 + t1, which represents the difference between the predicted value and c.
 * 3. Calculate the squared differences pa = t0 * t0, pb = t1 * t1, and pc = t2 * t2.
 * 4. Return the value (a, b, or c) that corresponds to the smallest squared difference.
 *
 * @param a The first value (typically the pixel to the left of the current pixel).
 * @param b The second value (typically the pixel above the current pixel).
 * @param c The third value (typically the pixel diagonally above and to the left of the current pixel).
 * @return The value (a, b, or c) that minimizes the prediction error.
 */
static int stbi__paeth(int a, int b, int c)
{
  int t0 = b - c; // p - a
  int t1 = a - c; // p - b
  int t2 = t0 + t1; // p - c
  int pa = t0*t0;
  int pb = t1*t1;
  int pc = t2*t2;

   //int p = a + b - c;
   //int pa = abs(p-a);
   //int pb = abs(p-b);
   //int pc = abs(p-c);
   if (pa <= pb && pa <= pc) return a;
   if (pb <= pc) return b;
   return c;
}

static const stbi_uc stbi__depth_scale_table[9] = { 0, 0xff, 0x55, 0, 0x11, 0,0,0, 0x01 };

// create the png data from post-deflated data
static int stbi__create_png_image_raw(stbi__png *a, stbi_uc *raw, stbi__uint32 raw_len, int out_n, stbi__uint32 x, stbi__uint32 y, int depth, int color)
{
   int bytes = (depth == 16? 2 : 1);
   stbi__context *s = a->s;
   stbi__uint32 i,j,stride = x*out_n*bytes;
   stbi__uint32 img_len, img_width_bytes;
   int k;
   int img_n = s->img_n; // copy it into a local for later

   int output_bytes = out_n*bytes;
   int filter_bytes = img_n*bytes;
   int width = x;

   STBI_ASSERT(out_n == s->img_n || out_n == s->img_n+1);
   a->out = (stbi_uc *) stbi__malloc_mad3(x, y, output_bytes, 0); // extra bytes to write off the end into
   if (!a->out) return stbi__err("outofmem", "Out of memory");

   if (!stbi__mad3sizes_valid(img_n, x, depth, 7)) return stbi__err("too large", "Corrupt PNG");
   img_width_bytes = (((img_n * x * depth) + 7) >> 3);
   img_len = (img_width_bytes + 1) * y;

   // we used to check for exact match between raw_len and img_len on non-interlaced PNGs,
   // but issue #276 reported a PNG in the wild that had extra data at the end (all zeros),
   // so just check for raw_len < img_len always.
   if (raw_len < img_len) return stbi__err("not enough pixels","Corrupt PNG");

   for (j=0; j < y; ++j) {
      stbi_uc *cur = a->out + stride*j;
      stbi_uc *prior;
      int filter = *raw++;

      if (filter > 4)
         return stbi__err("invalid filter","Corrupt PNG");

      if (depth < 8) {
         if (img_width_bytes > x) return stbi__err("invalid width","Corrupt PNG");
         cur += x*out_n - img_width_bytes; // store output to the rightmost img_len bytes, so we can decode in place
         filter_bytes = 1;
         width = img_width_bytes;
      }
      prior = cur - stride; // bugfix: need to compute this after 'cur +=' computation above

      // if first row, use special filter that doesn't sample previous row
      if (j == 0) filter = first_row_filter[filter];

      // handle first byte explicitly
      for (k=0; k < filter_bytes; ++k) {
         switch (filter) {
            case STBI__F_none       : cur[k] = raw[k]; break;
            case STBI__F_sub        : cur[k] = raw[k]; break;
            case STBI__F_up         : cur[k] = STBI__BYTECAST(raw[k] + prior[k]); break;
            case STBI__F_avg        : cur[k] = STBI__BYTECAST(raw[k] + (prior[k]>>1)); break;
            case STBI__F_paeth      : cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(0,prior[k],0)); break;
            case STBI__F_avg_first  : cur[k] = raw[k]; break;
            case STBI__F_paeth_first: cur[k] = raw[k]; break;
         }
      }

      if (depth == 8) {
         if (img_n != out_n)
            cur[img_n] = 255; // first pixel
         raw += img_n;
         cur += out_n;
         prior += out_n;
      } else if (depth == 16) {
         if (img_n != out_n) {
            cur[filter_bytes]   = 255; // first pixel top byte
            cur[filter_bytes+1] = 255; // first pixel bottom byte
         }
         raw += filter_bytes;
         cur += output_bytes;
         prior += output_bytes;
      } else {
         raw += 1;
         cur += 1;
         prior += 1;
      }

      // this is a little gross, so that we don't switch per-pixel or per-component
      if (depth < 8 || img_n == out_n) {
         int nk = (width - 1)*filter_bytes;
         if (filter == STBI__F_none) {
           memcpy(cur, raw, nk);
           raw += nk;
           continue;
         }

         k = 0;
         int total = nk;
         int pad = nk % 16;
         nk -= pad;
         // loop unroll
         #define STBI__CASE(f, body) \
             case f:     \
                for (; k < nk; k+=16) { \
                  body(k + 0); body(k + 1); body(k + 2); body(k + 3); \
                  body(k + 4); body(k + 5); body(k + 6); body(k + 7); \
                  body(k + 8); body(k + 9); body(k +10); body(k +11); \
                  body(k +12); body(k +13); body(k +14); body(k +15); \
                } break;
         switch (filter) {
#define CASE_BODY_sub(k) cur[k] = STBI__BYTECAST(raw[k] + cur[(k)-filter_bytes])
#define CASE_BODY_avg(k) cur[k] = STBI__BYTECAST(raw[k] + ((prior[k] + cur[k-filter_bytes])>>1))
#define CASE_BODY_up(k) cur[k] = STBI__BYTECAST(raw[k] + prior[k])
#define CASE_BODY_paeth(k) cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k-filter_bytes],prior[k],prior[k-filter_bytes]))
#define CASE_BODY_avg_first(k) cur[k] = STBI__BYTECAST(raw[k] + (cur[k-filter_bytes] >> 1))
#define CASE_BODY_paeth_first(k) cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k-filter_bytes],0,0))
            STBI__CASE(STBI__F_sub, CASE_BODY_sub)
            STBI__CASE(STBI__F_up, CASE_BODY_up)
            STBI__CASE(STBI__F_avg, CASE_BODY_avg)
            STBI__CASE(STBI__F_paeth, CASE_BODY_paeth)
            STBI__CASE(STBI__F_avg_first, CASE_BODY_avg_first)
            STBI__CASE(STBI__F_paeth_first, CASE_BODY_paeth_first)
         }
         #undef STBI__CASE

         // handle padding
         #define STBI__CASE(f) \
             case f:     \
                for (; k < total; ++k)
         switch (filter) {
            STBI__CASE(STBI__F_sub)          { cur[k] = STBI__BYTECAST(raw[k] + cur[k-filter_bytes]); } break;
            STBI__CASE(STBI__F_up)           { cur[k] = STBI__BYTECAST(raw[k] + prior[k]); } break;
            STBI__CASE(STBI__F_avg)          { cur[k] = STBI__BYTECAST(raw[k] + ((prior[k] + cur[k-filter_bytes])>>1)); } break;
            STBI__CASE(STBI__F_paeth)        { cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k-filter_bytes],prior[k],prior[k-filter_bytes])); } break;
            STBI__CASE(STBI__F_avg_first)    { cur[k] = STBI__BYTECAST(raw[k] + (cur[k-filter_bytes] >> 1)); } break;
            STBI__CASE(STBI__F_paeth_first)  { cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k-filter_bytes],0,0)); } break;
         }
         #undef STBI__CASE
         raw += total;
      } else {
         STBI_ASSERT(img_n+1 == out_n);
         #define STBI__CASE(f) \
             case f:     \
                for (i=x-1; i >= 1; --i, cur[filter_bytes]=255,raw+=filter_bytes,cur+=output_bytes,prior+=output_bytes) \
                   for (k=0; k < filter_bytes; ++k)
         switch (filter) {
            STBI__CASE(STBI__F_none)         { cur[k] = raw[k]; } break;
            STBI__CASE(STBI__F_sub)          { cur[k] = STBI__BYTECAST(raw[k] + cur[k- output_bytes]); } break;
            STBI__CASE(STBI__F_up)           { cur[k] = STBI__BYTECAST(raw[k] + prior[k]); } break;
            STBI__CASE(STBI__F_avg)          { cur[k] = STBI__BYTECAST(raw[k] + ((prior[k] + cur[k- output_bytes])>>1)); } break;
            STBI__CASE(STBI__F_paeth)        { cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k- output_bytes],prior[k],prior[k- output_bytes])); } break;
            STBI__CASE(STBI__F_avg_first)    { cur[k] = STBI__BYTECAST(raw[k] + (cur[k- output_bytes] >> 1)); } break;
            STBI__CASE(STBI__F_paeth_first)  { cur[k] = STBI__BYTECAST(raw[k] + stbi__paeth(cur[k- output_bytes],0,0)); } break;
         }
         #undef STBI__CASE

         // the loop above sets the high byte of the pixels' alpha, but for
         // 16 bit png files we also need the low byte set. we'll do that here.
         if (depth == 16) {
            cur = a->out + stride*j; // start at the beginning of the row again
            for (i=0; i < x; ++i,cur+=output_bytes) {
               cur[filter_bytes+1] = 255;
            }
         }
      }
   }

   // we make a separate pass to expand bits to pixels; for performance,
   // this could run two scanlines behind the above code, so it won't
   // intefere with filtering but will still be in the cache.
   if (depth < 8) {
      for (j=0; j < y; ++j) {
         stbi_uc *cur = a->out + stride*j;
         stbi_uc *in  = a->out + stride*j + x*out_n - img_width_bytes;
         // unpack 1/2/4-bit into a 8-bit buffer. allows us to keep the common 8-bit path optimal at minimal cost for 1/2/4-bit
         // png guarante byte alignment, if width is not multiple of 8/4/2 we'll decode dummy trailing data that will be skipped in the later loop
         stbi_uc scale = (color == 0) ? stbi__depth_scale_table[depth] : 1; // scale grayscale values to 0..255 range

         // note that the final byte might overshoot and write more data than desired.
         // we can allocate enough data that this never writes out of memory, but it
         // could also overwrite the next scanline. can it overwrite non-empty data
         // on the next scanline? yes, consider 1-pixel-wide scanlines with 1-bit-per-pixel.
         // so we need to explicitly clamp the final ones

         if (depth == 4) {
            for (k=x*img_n; k >= 2; k-=2, ++in) {
               *cur++ = scale * ((*in >> 4)       );
               *cur++ = scale * ((*in     ) & 0x0f);
            }
            if (k > 0) *cur++ = scale * ((*in >> 4)       );
         } else if (depth == 2) {
            for (k=x*img_n; k >= 4; k-=4, ++in) {
               *cur++ = scale * ((*in >> 6)       );
               *cur++ = scale * ((*in >> 4) & 0x03);
               *cur++ = scale * ((*in >> 2) & 0x03);
               *cur++ = scale * ((*in     ) & 0x03);
            }
            if (k > 0) *cur++ = scale * ((*in >> 6)       );
            if (k > 1) *cur++ = scale * ((*in >> 4) & 0x03);
            if (k > 2) *cur++ = scale * ((*in >> 2) & 0x03);
         } else if (depth == 1) {
            for (k=x*img_n; k >= 8; k-=8, ++in) {
               *cur++ = scale * ((*in >> 7)       );
               *cur++ = scale * ((*in >> 6) & 0x01);
               *cur++ = scale * ((*in >> 5) & 0x01);
               *cur++ = scale * ((*in >> 4) & 0x01);
               *cur++ = scale * ((*in >> 3) & 0x01);
               *cur++ = scale * ((*in >> 2) & 0x01);
               *cur++ = scale * ((*in >> 1) & 0x01);
               *cur++ = scale * ((*in     ) & 0x01);
            }
            if (k > 0) *cur++ = scale * ((*in >> 7)       );
            if (k > 1) *cur++ = scale * ((*in >> 6) & 0x01);
            if (k > 2) *cur++ = scale * ((*in >> 5) & 0x01);
            if (k > 3) *cur++ = scale * ((*in >> 4) & 0x01);
            if (k > 4) *cur++ = scale * ((*in >> 3) & 0x01);
            if (k > 5) *cur++ = scale * ((*in >> 2) & 0x01);
            if (k > 6) *cur++ = scale * ((*in >> 1) & 0x01);
         }
         if (img_n != out_n) {
            int q;
            // insert alpha = 255
            cur = a->out + stride*j;
            if (img_n == 1) {
               for (q=x-1; q >= 0; --q) {
                  cur[q*2+1] = 255;
                  cur[q*2+0] = cur[q];
               }
            } else {
               STBI_ASSERT(img_n == 3);
               for (q=x-1; q >= 0; --q) {
                  cur[q*4+3] = 255;
                  cur[q*4+2] = cur[q*3+2];
                  cur[q*4+1] = cur[q*3+1];
                  cur[q*4+0] = cur[q*3+0];
               }
            }
         }
      }
   } else if (depth == 16) {
      // force the image data from big-endian to platform-native.
      // this is done in a separate pass due to the decoding relying
      // on the data being untouched, but could probably be done
      // per-line during decode if care is taken.
      stbi_uc *cur = a->out;
      stbi__uint16 *cur16 = (stbi__uint16*)cur;

      for(i=0; i < x*y*out_n; ++i,cur16++,cur+=2) {
         *cur16 = (cur[0] << 8) | cur[1];
      }
   }

   return 1;
}

/**
 * Creates a PNG image from the provided image data, handling both interlaced and non-interlaced formats.
 * 
 * This function processes the raw image data to construct a PNG image. If the image is interlaced,
 * it performs de-interlacing by reconstructing the image from multiple passes. For non-interlaced images,
 * it directly delegates to `stbi__create_png_image_raw` for processing.
 *
 * @param a A pointer to the `stbi__png` structure containing PNG decoding state and metadata.
 * @param image_data A pointer to the raw image data to be processed.
 * @param image_data_len The length of the raw image data in bytes.
 * @param out_n The number of output components per pixel (e.g., 3 for RGB, 4 for RGBA).
 * @param depth The bit depth of the image (e.g., 8, 16).
 * @param color The color type of the image (e.g., grayscale, RGB, palette).
 * @param interlaced A flag indicating whether the image is interlaced (1) or not (0).
 *
 * @return Returns 1 on success, 0 on failure (e.g., memory allocation error or invalid image data).
 */
static int stbi__create_png_image(stbi__png *a, stbi_uc *image_data, stbi__uint32 image_data_len, int out_n, int depth, int color, int interlaced)
{
   int bytes = (depth == 16 ? 2 : 1);
   int out_bytes = out_n * bytes;
   stbi_uc *final;
   int p;
   if (!interlaced)
      return stbi__create_png_image_raw(a, image_data, image_data_len, out_n, a->s->img_x, a->s->img_y, depth, color);

   // de-interlacing
   final = (stbi_uc *) stbi__malloc_mad3(a->s->img_x, a->s->img_y, out_bytes, 0);
   for (p=0; p < 7; ++p) {
      int xorig[] = { 0,4,0,2,0,1,0 };
      int yorig[] = { 0,0,4,0,2,0,1 };
      int xspc[]  = { 8,8,4,4,2,2,1 };
      int yspc[]  = { 8,8,8,4,4,2,2 };
      int i,j,x,y;
      // pass1_x[4] = 0, pass1_x[5] = 1, pass1_x[12] = 1
      x = (a->s->img_x - xorig[p] + xspc[p]-1) / xspc[p];
      y = (a->s->img_y - yorig[p] + yspc[p]-1) / yspc[p];
      if (x && y) {
         stbi__uint32 img_len = ((((a->s->img_n * x * depth) + 7) >> 3) + 1) * y;
         if (!stbi__create_png_image_raw(a, image_data, image_data_len, out_n, x, y, depth, color)) {
            STBI_FREE(final);
            return 0;
         }
         for (j=0; j < y; ++j) {
            for (i=0; i < x; ++i) {
               int out_y = j*yspc[p]+yorig[p];
               int out_x = i*xspc[p]+xorig[p];
               memcpy(final + out_y*a->s->img_x*out_bytes + out_x*out_bytes,
                      a->out + (j*x+i)*out_bytes, out_bytes);
            }
         }
         STBI_FREE(a->out);
         image_data += img_len;
         image_data_len -= img_len;
      }
   }
   a->out = final;

   return 1;
}

/**
 * Computes transparency for the image data based on a specified transparent color.
 *
 * This function processes the image data stored in the `z->out` buffer and applies
 * transparency based on the provided transparent color `tc`. The function assumes that
 * the output buffer already has an alpha channel initialized to 255 (fully opaque).
 *
 * The function supports two output formats:
 * - For `out_n == 2` (grayscale with alpha), the function checks if the grayscale value
 *   matches the transparent color `tc[0]`. If it matches, the alpha value is set to 0
 *   (fully transparent); otherwise, it remains 255.
 * - For `out_n == 4` (RGB with alpha), the function checks if the RGB values match the
 *   transparent color `tc[0]`, `tc[1]`, and `tc[2]`. If they match, the alpha value is
 *   set to 0; otherwise, it remains 255.
 *
 * @param z Pointer to the `stbi__png` structure containing the image data and context.
 * @param tc Array of 3 bytes representing the transparent color (RGB or grayscale).
 * @param out_n The number of output channels (must be 2 or 4).
 * @return Returns 1 on success. The function assumes valid input and does not handle errors.
 */
static int stbi__compute_transparency(stbi__png *z, stbi_uc tc[3], int out_n)
{
   stbi__context *s = z->s;
   stbi__uint32 i, pixel_count = s->img_x * s->img_y;
   stbi_uc *p = z->out;

   // compute color-based transparency, assuming we've
   // already got 255 as the alpha value in the output
   STBI_ASSERT(out_n == 2 || out_n == 4);

   if (out_n == 2) {
      for (i=0; i < pixel_count; ++i) {
         p[1] = (p[0] == tc[0] ? 0 : 255);
         p += 2;
      }
   } else {
      for (i=0; i < pixel_count; ++i) {
         if (p[0] == tc[0] && p[1] == tc[1] && p[2] == tc[2])
            p[3] = 0;
         p += 4;
      }
   }
   return 1;
}

/**
 * Computes transparency for a 16-bit PNG image based on a specified transparent color.
 * 
 * This function processes the image data to apply transparency by comparing each pixel's
 * color components to the provided transparent color (`tc`). If the pixel matches the
 * transparent color, its alpha value is set to 0 (fully transparent). Otherwise, the alpha
 * value is set to 65535 (fully opaque). The function assumes that the output buffer already
 * contains an alpha channel initialized to 65535.
 *
 * @param z Pointer to the `stbi__png` structure containing the PNG image data and context.
 * @param tc Array of three 16-bit values representing the transparent color (RGB components).
 * @param out_n The number of components per pixel in the output buffer (must be 2 or 4).
 *              - If `out_n` is 2, the output is grayscale with an alpha channel.
 *              - If `out_n` is 4, the output is RGBA.
 *
 * @return Returns 1 on success. The function assumes valid input and does not handle errors.
 */
static int stbi__compute_transparency16(stbi__png *z, stbi__uint16 tc[3], int out_n)
{
   stbi__context *s = z->s;
   stbi__uint32 i, pixel_count = s->img_x * s->img_y;
   stbi__uint16 *p = (stbi__uint16*) z->out;

   // compute color-based transparency, assuming we've
   // already got 65535 as the alpha value in the output
   STBI_ASSERT(out_n == 2 || out_n == 4);

   if (out_n == 2) {
      for (i = 0; i < pixel_count; ++i) {
         p[1] = (p[0] == tc[0] ? 0 : 65535);
         p += 2;
      }
   } else {
      for (i = 0; i < pixel_count; ++i) {
         if (p[0] == tc[0] && p[1] == tc[1] && p[2] == tc[2])
            p[3] = 0;
         p += 4;
      }
   }
   return 1;
}

/**
 * Expands a PNG palette into a full-color image.
 *
 * This function takes a PNG image that uses a palette (indexed color) and converts it into
 * a full-color image by replacing each palette index with the corresponding RGB or RGBA color
 * from the provided palette. The resulting image is stored in the `a->out` buffer.
 *
 * @param a Pointer to the stbi__png structure containing the PNG image data.
 * @param palette Pointer to the palette array, which contains RGB or RGBA color values.
 * @param len Length of the palette array (unused in this function).
 * @param pal_img_n Number of color channels in the palette (3 for RGB, 4 for RGBA).
 *
 * @return Returns 1 on success. If memory allocation fails, returns 0 and sets an error message
 *         using `stbi__err`.
 *
 * @note The function allocates memory for the expanded image. If the function exits early
 *       (e.g., due to an error), the allocated memory must be freed to avoid memory leaks.
 */
static int stbi__expand_png_palette(stbi__png *a, stbi_uc *palette, int len, int pal_img_n)
{
   stbi__uint32 i, pixel_count = a->s->img_x * a->s->img_y;
   stbi_uc *p, *temp_out, *orig = a->out;

   p = (stbi_uc *) stbi__malloc_mad2(pixel_count, pal_img_n, 0);
   if (p == NULL) return stbi__err("outofmem", "Out of memory");

   // between here and free(out) below, exitting would leak
   temp_out = p;

   if (pal_img_n == 3) {
      for (i=0; i < pixel_count; ++i) {
         int n = orig[i]*4;
         p[0] = palette[n  ];
         p[1] = palette[n+1];
         p[2] = palette[n+2];
         p += 3;
      }
   } else {
      for (i=0; i < pixel_count; ++i) {
         int n = orig[i]*4;
         p[0] = palette[n  ];
         p[1] = palette[n+1];
         p[2] = palette[n+2];
         p[3] = palette[n+3];
         p += 4;
      }
   }
   STBI_FREE(a->out);
   a->out = temp_out;

   STBI_NOTUSED(len);

   return 1;
}

static int stbi__unpremultiply_on_load = 0;
static int stbi__de_iphone_flag = 0;

/**
 * @brief Sets whether images should be unpremultiplied upon loading.
 *
 * This function configures the behavior of the image loading process regarding
 * premultiplied alpha. When the flag is set to a non-zero value, images loaded
 * by the library will have their color channels divided by the alpha channel
 * (if present) to undo premultiplication. This is useful for images that were
 * saved with premultiplied alpha, as it restores the original color values.
 *
 * @param flag_true_if_should_unpremultiply A non-zero value to enable unpremultiplication,
 *                                          or zero to disable it.
 */
STBIDEF void stbi_set_unpremultiply_on_load(int flag_true_if_should_unpremultiply)
{
   stbi__unpremultiply_on_load = flag_true_if_should_unpremultiply;
}

/**
 * @brief Sets a flag to control whether iPhone-formatted PNG images should be converted to RGB.
 *
 * This function sets an internal flag that determines whether PNG images formatted for iPhone
 * should be converted to standard RGB format during decoding. iPhone-formatted PNGs may have
 * different color handling (e.g., premultiplied alpha), and this flag allows the caller to
 * specify whether such conversion should occur.
 *
 * @param flag_true_if_should_convert An integer flag where a non-zero value enables the
 *                                    conversion of iPhone-formatted PNGs to RGB, and a zero
 *                                    value disables it.
 */
STBIDEF void stbi_convert_iphone_png_to_rgb(int flag_true_if_should_convert)
{
   stbi__de_iphone_flag = flag_true_if_should_convert;
}

/**
 * Converts an image from BGR (Blue-Green-Red) to RGB (Red-Green-Blue) format,
 * which is commonly used to adjust images loaded from certain formats (e.g., iPhone images).
 * Additionally, if the image has an alpha channel and unpremultiplied alpha is enabled,
 * it unpremultiplies the RGB values by the alpha channel.
 *
 * @param z A pointer to the stbi__png structure containing the image data and context.
 *          The image data is expected to be in BGR format, and the output will be modified in-place.
 *          The structure must include the image dimensions (img_x, img_y), the number of output
 *          channels (img_out_n), and the output buffer (out).
 *
 * The function processes the image as follows:
 * - For 3-channel images (RGB without alpha), it swaps the first and third channels (BGR to RGB).
 * - For 4-channel images (RGBA), it swaps the first and third channels and, if unpremultiplied alpha
 *   is enabled, it divides the RGB values by the alpha channel to restore the original colors.
 * - The function operates directly on the output buffer (z->out) and modifies it in-place.
 */
static void stbi__de_iphone(stbi__png *z)
{
   stbi__context *s = z->s;
   stbi__uint32 i, pixel_count = s->img_x * s->img_y;
   stbi_uc *p = z->out;

   if (s->img_out_n == 3) {  // convert bgr to rgb
      for (i=0; i < pixel_count; ++i) {
         stbi_uc t = p[0];
         p[0] = p[2];
         p[2] = t;
         p += 3;
      }
   } else {
      STBI_ASSERT(s->img_out_n == 4);
      if (stbi__unpremultiply_on_load) {
         // convert bgr to rgb and unpremultiply
         for (i=0; i < pixel_count; ++i) {
            stbi_uc a = p[3];
            stbi_uc t = p[0];
            if (a) {
               stbi_uc half = a / 2;
               p[0] = (p[2] * 255 + half) / a;
               p[1] = (p[1] * 255 + half) / a;
               p[2] = ( t   * 255 + half) / a;
            } else {
               p[0] = p[2];
               p[2] = t;
            }
            p += 4;
         }
      } else {
         // convert bgr to rgb
         for (i=0; i < pixel_count; ++i) {
            stbi_uc t = p[0];
            p[0] = p[2];
            p[2] = t;
            p += 4;
         }
      }
   }
}

#define STBI__PNG_TYPE(a,b,c,d)  (((unsigned) (a) << 24) + ((unsigned) (b) << 16) + ((unsigned) (c) << 8) + (unsigned) (d))

/**
 * Parses a PNG file and decodes its contents into a usable image format.
 *
 * This method reads and processes the PNG file chunk by chunk, handling critical
 * chunks such as IHDR, PLTE, tRNS, IDAT, and IEND. It validates the PNG header,
 * extracts image metadata (e.g., dimensions, color type, depth), and decodes
 * the image data. It also handles transparency, interlacing, and palette-based
 * images. The decoded image is stored in the provided `stbi__png` structure.
 *
 * @param z Pointer to the `stbi__png` structure that will store the parsed PNG data.
 * @param scan Specifies the scanning mode (e.g., header-only, full load).
 * @param req_comp The number of desired output components (e.g., 3 for RGB, 4 for RGBA).
 * @return Returns 1 on success, 0 on failure. Failure can occur due to invalid PNG
 *         data, unsupported features, or memory allocation errors.
 */
static int stbi__parse_png_file(stbi__png *z, int scan, int req_comp)
{
   stbi_uc palette[1024], pal_img_n=0;
   stbi_uc has_trans=0, tc[3]={0};
   stbi__uint16 tc16[3];
   stbi__uint32 ioff=0, idata_limit=0, i, pal_len=0;
   int first=1,k,interlace=0, color=0, is_iphone=0;
   stbi__context *s = z->s;

   z->expanded = NULL;
   z->idata = NULL;
   z->out = NULL;

   if (!stbi__check_png_header(s)) return 0;

   if (scan == STBI__SCAN_type) return 1;

   for (;;) {
      stbi__pngchunk c = stbi__get_chunk_header(s);
      switch (c.type) {
         case STBI__PNG_TYPE('C','g','B','I'):
            is_iphone = 1;
            stbi__skip(s, c.length);
            break;
         case STBI__PNG_TYPE('I','H','D','R'): {
            int comp,filter;
            if (!first) return stbi__err("multiple IHDR","Corrupt PNG");
            first = 0;
            if (c.length != 13) return stbi__err("bad IHDR len","Corrupt PNG");
            s->img_x = stbi__get32be(s);
            s->img_y = stbi__get32be(s);
            if (s->img_y > STBI_MAX_DIMENSIONS) return stbi__err("too large","Very large image (corrupt?)");
            if (s->img_x > STBI_MAX_DIMENSIONS) return stbi__err("too large","Very large image (corrupt?)");
            z->depth = stbi__get8(s);  if (z->depth != 1 && z->depth != 2 && z->depth != 4 && z->depth != 8 && z->depth != 16)  return stbi__err("1/2/4/8/16-bit only","PNG not supported: 1/2/4/8/16-bit only");
            color = stbi__get8(s);  if (color > 6)         return stbi__err("bad ctype","Corrupt PNG");
            if (color == 3 && z->depth == 16)                  return stbi__err("bad ctype","Corrupt PNG");
            if (color == 3) pal_img_n = 3; else if (color & 1) return stbi__err("bad ctype","Corrupt PNG");
            comp  = stbi__get8(s);  if (comp) return stbi__err("bad comp method","Corrupt PNG");
            filter= stbi__get8(s);  if (filter) return stbi__err("bad filter method","Corrupt PNG");
            interlace = stbi__get8(s); if (interlace>1) return stbi__err("bad interlace method","Corrupt PNG");
            if (!s->img_x || !s->img_y) return stbi__err("0-pixel image","Corrupt PNG");
            if (!pal_img_n) {
               s->img_n = (color & 2 ? 3 : 1) + (color & 4 ? 1 : 0);
               if ((1 << 30) / s->img_x / s->img_n < s->img_y) return stbi__err("too large", "Image too large to decode");
               if (scan == STBI__SCAN_header) return 1;
            } else {
               // if paletted, then pal_n is our final components, and
               // img_n is # components to decompress/filter.
               s->img_n = 1;
               if ((1 << 30) / s->img_x / 4 < s->img_y) return stbi__err("too large","Corrupt PNG");
               // if SCAN_header, have to scan to see if we have a tRNS
            }
            break;
         }

         case STBI__PNG_TYPE('P','L','T','E'):  {
            if (first) return stbi__err("first not IHDR", "Corrupt PNG");
            if (c.length > 256*3) return stbi__err("invalid PLTE","Corrupt PNG");
            pal_len = c.length / 3;
            if (pal_len * 3 != c.length) return stbi__err("invalid PLTE","Corrupt PNG");
            for (i=0; i < pal_len; ++i) {
               palette[i*4+0] = stbi__get8(s);
               palette[i*4+1] = stbi__get8(s);
               palette[i*4+2] = stbi__get8(s);
               palette[i*4+3] = 255;
            }
            break;
         }

         case STBI__PNG_TYPE('t','R','N','S'): {
            if (first) return stbi__err("first not IHDR", "Corrupt PNG");
            if (z->idata) return stbi__err("tRNS after IDAT","Corrupt PNG");
            if (pal_img_n) {
               if (scan == STBI__SCAN_header) { s->img_n = 4; return 1; }
               if (pal_len == 0) return stbi__err("tRNS before PLTE","Corrupt PNG");
               if (c.length > pal_len) return stbi__err("bad tRNS len","Corrupt PNG");
               pal_img_n = 4;
               for (i=0; i < c.length; ++i)
                  palette[i*4+3] = stbi__get8(s);
            } else {
               if (!(s->img_n & 1)) return stbi__err("tRNS with alpha","Corrupt PNG");
               if (c.length != (stbi__uint32) s->img_n*2) return stbi__err("bad tRNS len","Corrupt PNG");
               has_trans = 1;
               if (z->depth == 16) {
                  for (k = 0; k < s->img_n; ++k) tc16[k] = (stbi__uint16)stbi__get16be(s); // copy the values as-is
               } else {
                  for (k = 0; k < s->img_n; ++k) tc[k] = (stbi_uc)(stbi__get16be(s) & 255) * stbi__depth_scale_table[z->depth]; // non 8-bit images will be larger
               }
            }
            break;
         }

         case STBI__PNG_TYPE('I','D','A','T'): {
            if (first) return stbi__err("first not IHDR", "Corrupt PNG");
            if (pal_img_n && !pal_len) return stbi__err("no PLTE","Corrupt PNG");
            if (scan == STBI__SCAN_header) { s->img_n = pal_img_n; return 1; }
            if ((int)(ioff + c.length) < (int)ioff) return 0;
            if (ioff + c.length > idata_limit) {
               stbi__uint32 idata_limit_old = idata_limit;
               stbi_uc *p;
               if (idata_limit == 0) idata_limit = c.length > 4096 ? c.length : 4096;
               while (ioff + c.length > idata_limit)
                  idata_limit *= 2;
               STBI_NOTUSED(idata_limit_old);
               p = (stbi_uc *) STBI_REALLOC_SIZED(z->idata, idata_limit_old, idata_limit); if (p == NULL) return stbi__err("outofmem", "Out of memory");
               z->idata = p;
            }
            if (!stbi__getn(s, z->idata+ioff,c.length)) return stbi__err("outofdata","Corrupt PNG");
            ioff += c.length;
            break;
         }

         case STBI__PNG_TYPE('I','E','N','D'): {
            stbi__uint32 raw_len, bpl;
            if (first) return stbi__err("first not IHDR", "Corrupt PNG");
            if (scan != STBI__SCAN_load) return 1;
            if (z->idata == NULL) return stbi__err("no IDAT","Corrupt PNG");
            // initial guess for decoded data size to avoid unnecessary reallocs
            bpl = (s->img_x * z->depth + 7) / 8; // bytes per line, per component
            raw_len = bpl * s->img_y * s->img_n /* pixels */ + s->img_y /* filter mode per row */;
            z->expanded = (stbi_uc *) stbi_zlib_decode_malloc_guesssize_headerflag((char *) z->idata, ioff, raw_len, (int *) &raw_len, !is_iphone);
            if (z->expanded == NULL) return 0; // zlib should set error
            STBI_FREE(z->idata); z->idata = NULL;
            if ((req_comp == s->img_n+1 && req_comp != 3 && !pal_img_n) || has_trans)
               s->img_out_n = s->img_n+1;
            else
               s->img_out_n = s->img_n;
            if (!stbi__create_png_image(z, z->expanded, raw_len, s->img_out_n, z->depth, color, interlace)) return 0;
            if (has_trans) {
               if (z->depth == 16) {
                  if (!stbi__compute_transparency16(z, tc16, s->img_out_n)) return 0;
               } else {
                  if (!stbi__compute_transparency(z, tc, s->img_out_n)) return 0;
               }
            }
            if (is_iphone && stbi__de_iphone_flag && s->img_out_n > 2)
               stbi__de_iphone(z);
            if (pal_img_n) {
               // pal_img_n == 3 or 4
               s->img_n = pal_img_n; // record the actual colors we had
               s->img_out_n = pal_img_n;
               if (req_comp >= 3) s->img_out_n = req_comp;
               if (!stbi__expand_png_palette(z, palette, pal_len, s->img_out_n))
                  return 0;
            } else if (has_trans) {
               // non-paletted image with tRNS -> source image has (constant) alpha
               ++s->img_n;
            }
            STBI_FREE(z->expanded); z->expanded = NULL;
            // end of PNG chunk, read and skip CRC
            stbi__get32be(s);
            return 1;
         }

         default:
            // if critical, fail
            if (first) return stbi__err("first not IHDR", "Corrupt PNG");
            if ((c.type & (1 << 29)) == 0) {
               #ifndef STBI_NO_FAILURE_STRINGS
               // not threadsafe
               static char invalid_chunk[] = "XXXX PNG chunk not known";
               invalid_chunk[0] = STBI__BYTECAST(c.type >> 24);
               invalid_chunk[1] = STBI__BYTECAST(c.type >> 16);
               invalid_chunk[2] = STBI__BYTECAST(c.type >>  8);
               invalid_chunk[3] = STBI__BYTECCAST(c.type >>  0);
               #endif
               return stbi__err(invalid_chunk, "PNG not supported: unknown PNG chunk type");
            }
            stbi__skip(s, c.length);
            break;
      }
      // end of PNG chunk, read and skip CRC
      stbi__get32be(s);
   }
}

/**
 * @brief Processes a PNG file and decodes it into a raw image buffer.
 *
 * This function reads and decodes a PNG file represented by the `stbi__png` structure.
 * It extracts the image dimensions, number of channels, and pixel data, and optionally
 * converts the image to a specified number of channels (`req_comp`). The function also
 * updates the provided result info (`ri`) with details such as the bits per channel.
 *
 * @param p Pointer to the `stbi__png` structure containing the PNG file data.
 * @param x Pointer to store the width of the decoded image.
 * @param y Pointer to store the height of the decoded image.
 * @param n Pointer to store the number of channels in the decoded image (can be NULL).
 * @param req_comp The desired number of channels for the output image (0 to 4). If 0,
 *                 the original number of channels is preserved.
 * @param ri Pointer to the `stbi__result_info` structure to store metadata about the
 *           decoded image (e.g., bits per channel).
 *
 * @return A pointer to the decoded image buffer on success, or NULL on failure. The
 *         caller is responsible for freeing the returned buffer.
 */
static void *stbi__do_png(stbi__png *p, int *x, int *y, int *n, int req_comp, stbi__result_info *ri)
{
   void *result=NULL;
   if (req_comp < 0 || req_comp > 4) return stbi__errpuc("bad req_comp", "Internal error");
   if (stbi__parse_png_file(p, STBI__SCAN_load, req_comp)) {
      if (p->depth <= 8)
         ri->bits_per_channel = 8;
      else if (p->depth == 16)
         ri->bits_per_channel = 16;
      else
         return stbi__errpuc("bad bits_per_channel", "PNG not supported: unsupported color depth");
      result = p->out;
      p->out = NULL;
      if (req_comp && req_comp != p->s->img_out_n) {
         if (ri->bits_per_channel == 8)
            result = stbi__convert_format((unsigned char *) result, p->s->img_out_n, req_comp, p->s->img_x, p->s->img_y);
         else
            result = stbi__convert_format16((stbi__uint16 *) result, p->s->img_out_n, req_comp, p->s->img_x, p->s->img_y);
         p->s->img_out_n = req_comp;
         if (result == NULL) return result;
      }
      *x = p->s->img_x;
      *y = p->s->img_y;
      if (n) *n = p->s->img_n;
   }
   STBI_FREE(p->out);      p->out      = NULL;
   STBI_FREE(p->expanded); p->expanded = NULL;
   STBI_FREE(p->idata);    p->idata    = NULL;

   return result;
}

/**
 * @brief Loads a PNG image from the given input context and decodes it into a raw pixel buffer.
 *
 * This function initializes a PNG decoder context and delegates the actual decoding process to
 * `stbi__do_png`. It reads the PNG data from the provided `stbi__context`, decodes it, and returns
 * a pointer to the raw pixel data. The dimensions, number of components, and requested output
 * format are passed to the decoder, and the resulting image information is stored in the provided
 * parameters.
 *
 * @param s Pointer to the `stbi__context` containing the PNG image data.
 * @param x Pointer to an integer where the width of the decoded image will be stored.
 * @param y Pointer to an integer where the height of the decoded image will be stored.
 * @param comp Pointer to an integer where the number of components in the decoded image will be stored.
 * @param req_comp The desired number of components in the output image (e.g., 3 for RGB, 4 for RGBA).
 * @param ri Pointer to a `stbi__result_info` structure where additional decoding information will be stored.
 * @return A pointer to the decoded pixel data on success, or NULL if an error occurs.
 */
static void *stbi__png_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   stbi__png p;
   p.s = s;
   return stbi__do_png(&p, x,y,comp,req_comp, ri);
}

/**
 * Tests whether the given data stream contains a valid PNG image.
 *
 * This function checks the header of the data stream to determine if it matches
 * the PNG file signature. It reads the first few bytes of the stream to verify
 * the PNG header and then rewinds the stream to its original position, ensuring
 * that subsequent reads start from the beginning.
 *
 * @param s Pointer to the stbi__context structure representing the data stream.
 * @return Returns 1 if the data stream contains a valid PNG header, 0 otherwise.
 */
static int stbi__png_test(stbi__context *s)
{
   int r;
   r = stbi__check_png_header(s);
   stbi__rewind(s);
   return r;
}

/**
 * Retrieves the basic image information from a PNG file without fully decoding it.
 * This function parses the PNG file header to extract the image width, height, and number of components.
 * If the provided pointers are non-null, the corresponding values are assigned to them.
 *
 * @param p     Pointer to the stbi__png structure representing the PNG file.
 * @param x     Pointer to an integer where the image width will be stored. Can be NULL.
 * @param y     Pointer to an integer where the image height will be stored. Can be NULL.
 * @param comp  Pointer to an integer where the number of components (channels) will be stored. Can be NULL.
 * @return      Returns 1 if the PNG header was successfully parsed and the information was retrieved.
 *              Returns 0 if the PNG header could not be parsed or if the file is not a valid PNG.
 */
static int stbi__png_info_raw(stbi__png *p, int *x, int *y, int *comp)
{
   if (!stbi__parse_png_file(p, STBI__SCAN_header, 0)) {
      stbi__rewind( p->s );
      return 0;
   }
   if (x) *x = p->s->img_x;
   if (y) *y = p->s->img_y;
   if (comp) *comp = p->s->img_n;
   return 1;
}

/**
 * Retrieves the dimensions and number of components of a PNG image.
 *
 * This function parses the PNG header to extract the width, height, and number of color components
 * of the image. It does not decode the actual image data, making it efficient for obtaining
 * metadata about the image.
 *
 * @param s       A pointer to the stbi__context structure containing the PNG data stream.
 * @param x       A pointer to an integer where the width of the image will be stored.
 * @param y       A pointer to an integer where the height of the image will be stored.
 * @param comp    A pointer to an integer where the number of color components will be stored.
 * @return        Returns 1 on success, indicating that the image metadata was successfully
 *                retrieved. Returns 0 if the PNG header is invalid or if the metadata could not
 *                be extracted.
 */
static int stbi__png_info(stbi__context *s, int *x, int *y, int *comp)
{
   stbi__png p;
   p.s = s;
   return stbi__png_info_raw(&p, x, y, comp);
}

/**
 * @brief Checks if the PNG image loaded in the given context has a bit depth of 16.
 *
 * This function initializes a `stbi__png` structure with the provided `stbi__context` and
 * attempts to retrieve the PNG image's raw information using `stbi__png_info_raw`. If the
 * information retrieval fails, the function returns 0. If the PNG image's bit depth is not 16,
 * the function rewinds the context and returns 0. If the bit depth is 16, the function returns 1.
 *
 * @param s Pointer to the `stbi__context` containing the PNG image data.
 * @return int Returns 1 if the PNG image has a bit depth of 16, otherwise returns 0.
 */
static int stbi__png_is16(stbi__context *s)
{
   stbi__png p;
   p.s = s;
   if (!stbi__png_info_raw(&p, NULL, NULL, NULL))
	   return 0;
   if (p.depth != 16) {
      stbi__rewind(p.s);
      return 0;
   }
   return 1;
}
#endif

// Microsoft/Windows BMP image

#ifndef STBI_NO_BMP
/**
 * @brief Tests if the given input stream contains raw BMP data.
 *
 * This function reads the initial bytes of the input stream to determine if it
 * contains a valid BMP (Bitmap) file. It checks for the BMP signature ('BM')
 * at the beginning of the stream and validates the size of the BMP header.
 * The function discards certain fields (file size, reserved fields, and data offset)
 * as they are not needed for the test. It then checks if the header size is one
 * of the valid BMP header sizes (12, 40, 56, 108, or 124).
 *
 * @param s Pointer to the stbi__context structure representing the input stream.
 * @return int Returns 1 if the input stream contains valid raw BMP data,
 *         otherwise returns 0.
 */
static int stbi__bmp_test_raw(stbi__context *s)
{
   int r;
   int sz;
   if (stbi__get8(s) != 'B') return 0;
   if (stbi__get8(s) != 'M') return 0;
   stbi__get32le(s); // discard filesize
   stbi__get16le(s); // discard reserved
   stbi__get16le(s); // discard reserved
   stbi__get32le(s); // discard data offset
   sz = stbi__get32le(s);
   r = (sz == 12 || sz == 40 || sz == 56 || sz == 108 || sz == 124);
   return r;
}

/**
 * Tests whether the given data represents a valid BMP image.
 *
 * This function checks if the data provided by the `stbi__context` object `s`
 * is a valid BMP image by calling `stbi__bmp_test_raw`. After the test, it
 * rewinds the context to its original position using `stbi__rewind` so that
 * subsequent operations can process the data from the beginning.
 *
 * @param s The `stbi__context` object containing the data to be tested.
 * @return Returns a non-zero value if the data is a valid BMP image, otherwise
 *         returns 0.
 */
static int stbi__bmp_test(stbi__context *s)
{
   int r = stbi__bmp_test_raw(s);
   stbi__rewind(s);
   return r;
}


// returns 0..31 for the highest set bit
static int stbi__high_bit(unsigned int z)
{
   int n=0;
   if (z == 0) return -1;
   if (z >= 0x10000) { n += 16; z >>= 16; }
   if (z >= 0x00100) { n +=  8; z >>=  8; }
   if (z >= 0x00010) { n +=  4; z >>=  4; }
   if (z >= 0x00004) { n +=  2; z >>=  2; }
   if (z >= 0x00002) { n +=  1;/* >>=  1;*/ }
   return n;
}

/**
 * Counts the number of set bits (1s) in the binary representation of an unsigned integer.
 * This method uses a series of bitwise operations to efficiently count the bits.
 * 
 * The algorithm works by summing adjacent bits in stages, reducing the problem size at each step:
 * 1. Sum pairs of bits (max 2 per pair).
 * 2. Sum groups of 2 bits into groups of 4 (max 4 per group).
 * 3. Sum groups of 4 bits into groups of 8 (max 8 per group).
 * 4. Sum groups of 8 bits into groups of 16 (max 16 per group).
 * 5. Sum groups of 16 bits into a single 32-bit value (max 32).
 * 
 * The final result is masked to return only the lower 8 bits, which contains the total count.
 * 
 * @param a The unsigned integer whose bits are to be counted.
 * @return The number of set bits in the binary representation of `a`.
 */
static int stbi__bitcount(unsigned int a)
{
   a = (a & 0x55555555) + ((a >>  1) & 0x55555555); // max 2
   a = (a & 0x33333333) + ((a >>  2) & 0x33333333); // max 4
   a = (a + (a >> 4)) & 0x0f0f0f0f; // max 8 per 4, now 8 bits
   a = (a + (a >> 8)); // max 16 per 8 bits
   a = (a + (a >> 16)); // max 32 per 8 bits
   return a & 0xff;
}

// extract an arbitrarily-aligned N-bit value (N=bits)
// from v, and then make it 8-bits long and fractionally
// extend it to full full range.
static int stbi__shiftsigned(unsigned int v, int shift, int bits)
{
   static unsigned int mul_table[9] = {
      0,
      0xff/*0b11111111*/, 0x55/*0b01010101*/, 0x49/*0b01001001*/, 0x11/*0b00010001*/,
      0x21/*0b00100001*/, 0x41/*0b01000001*/, 0x81/*0b10000001*/, 0x01/*0b00000001*/,
   };
   static unsigned int shift_table[9] = {
      0, 0,0,1,0,2,4,6,0,
   };
   if (shift < 0)
      v <<= -shift;
   else
      v >>= shift;
   STBI_ASSERT(v < 256);
   v >>= (8-bits);
   STBI_ASSERT(bits >= 0 && bits <= 8);
   return (int) ((unsigned) v * mul_table[bits]) >> shift_table[bits];
}

typedef struct
{
   int bpp, offset, hsz;
   unsigned int mr,mg,mb,ma, all_a;
   int extra_read;
} stbi__bmp_data;

/**
 * Parses the header of a BMP image file and extracts relevant information.
 * 
 * This method reads and validates the BMP file header, ensuring it starts with the correct
 * signature ('BM'). It then extracts and stores various metadata such as the image dimensions,
 * bit depth, and color masks. The method supports BMP headers of different sizes (12, 40, 56,
 * 108, and 124 bytes) and handles specific cases like RLE compression and alpha channels.
 * 
 * @param s A pointer to the `stbi__context` structure, which holds the input stream and
 *          related state for reading the BMP file.
 * @param info A pointer to the `stbi__bmp_data` structure, where the parsed BMP header
 *             information will be stored.
 * 
 * @return Returns a pointer to indicate success (non-NULL) or NULL if the BMP header is
 *         invalid, unsupported, or corrupted. The caller should check the return value
 *         to determine if the parsing was successful.
 */
static void *stbi__bmp_parse_header(stbi__context *s, stbi__bmp_data *info)
{
   int hsz;
   if (stbi__get8(s) != 'B' || stbi__get8(s) != 'M') return stbi__errpuc("not BMP", "Corrupt BMP");
   stbi__get32le(s); // discard filesize
   stbi__get16le(s); // discard reserved
   stbi__get16le(s); // discard reserved
   info->offset = stbi__get32le(s);
   info->hsz = hsz = stbi__get32le(s);
   info->mr = info->mg = info->mb = info->ma = 0;
   info->extra_read = 14;

   if (info->offset < 0) return stbi__errpuc("bad BMP", "bad BMP");

   if (hsz != 12 && hsz != 40 && hsz != 56 && hsz != 108 && hsz != 124) return stbi__errpuc("unknown BMP", "BMP type not supported: unknown");
   if (hsz == 12) {
      s->img_x = stbi__get16le(s);
      s->img_y = stbi__get16le(s);
   } else {
      s->img_x = stbi__get32le(s);
      s->img_y = stbi__get32le(s);
   }
   if (stbi__get16le(s) != 1) return stbi__errpuc("bad BMP", "bad BMP");
   info->bpp = stbi__get16le(s);
   if (hsz != 12) {
      int compress = stbi__get32le(s);
      if (compress == 1 || compress == 2) return stbi__errpuc("BMP RLE", "BMP type not supported: RLE");
      stbi__get32le(s); // discard sizeof
      stbi__get32le(s); // discard hres
      stbi__get32le(s); // discard vres
      stbi__get32le(s); // discard colorsused
      stbi__get32le(s); // discard max important
      if (hsz == 40 || hsz == 56) {
         if (hsz == 56) {
            stbi__get32le(s);
            stbi__get32le(s);
            stbi__get32le(s);
            stbi__get32le(s);
         }
         if (info->bpp == 16 || info->bpp == 32) {
            if (compress == 0) {
               if (info->bpp == 32) {
                  info->mr = 0xffu << 16;
                  info->mg = 0xffu <<  8;
                  info->mb = 0xffu <<  0;
                  info->ma = 0xffu << 24;
                  info->all_a = 0; // if all_a is 0 at end, then we loaded alpha channel but it was all 0
               } else {
                  info->mr = 31u << 10;
                  info->mg = 31u <<  5;
                  info->mb = 31u <<  0;
               }
            } else if (compress == 3) {
               info->mr = stbi__get32le(s);
               info->mg = stbi__get32le(s);
               info->mb = stbi__get32le(s);
               info->extra_read += 12;
               // not documented, but generated by photoshop and handled by mspaint
               if (info->mr == info->mg && info->mg == info->mb) {
                  // ?!?!?
                  return stbi__errpuc("bad BMP", "bad BMP");
               }
            } else
               return stbi__errpuc("bad BMP", "bad BMP");
         }
      } else {
         int i;
         if (hsz != 108 && hsz != 124)
            return stbi__errpuc("bad BMP", "bad BMP");
         info->mr = stbi__get32le(s);
         info->mg = stbi__get32le(s);
         info->mb = stbi__get32le(s);
         info->ma = stbi__get32le(s);
         stbi__get32le(s); // discard color space
         for (i=0; i < 12; ++i)
            stbi__get32le(s); // discard color space parameters
         if (hsz == 124) {
            stbi__get32le(s); // discard rendering intent
            stbi__get32le(s); // discard offset of profile data
            stbi__get32le(s); // discard size of profile data
            stbi__get32le(s); // discard reserved
         }
      }
   }
   return (void *) 1;
}


```c
/**
 * @brief Loads and decodes a BMP image from the provided stbi__context.
 *
 * This function reads and decodes a BMP image from the given stbi__context stream.
 * It supports various BMP formats, including 1-bit, 4-bit, 8-bit, 16-bit, 24-bit,
 * and 32-bit color depths. The function handles palette-based images, bit masks,
 * and alpha channels. It also supports flipping the image vertically if necessary
 * and converting the output to a requested number of components.
 *
 * @param s Pointer to the stbi__context containing the BMP image data.
 * @param x Pointer to store the width of the decoded image.
 * @param y Pointer to store the height of the decoded image.
 * @param comp Pointer to store the number of components in the decoded image.
 * @param req_comp The number of components requested for the output image.
 *                 If 0, the original number of components is used.
 * @param ri Pointer to stbi__result_info (unused in this function).
 *
 * @return A pointer to the decoded image data as an array of stbi_uc (unsigned char).
 *         The caller is responsible for freeing this memory using STBI_FREE.
 *         Returns NULL if the image cannot be decoded due to corruption, unsupported
 *         format, or memory allocation failure.
 */
static void *stbi__bmp_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   stbi_uc *out;
   unsigned int mr=0,mg=0,mb=0,ma=0, all_a;
   stbi_uc pal[256][4];
   int psize=0,i,j,width;
   int flip_vertically, pad, target;
   stbi__bmp_data info;
   STBI_NOTUSED(ri);

   info.all_a = 255;
   if (stbi__bmp_parse_header(s, &info) == NULL)
      return NULL; // error code already set

   flip_vertically = ((int) s->img_y) > 0;
   s->img_y = abs((int) s->img_y);

   if (s->img_y > STBI_MAX_DIMENSIONS) return stbi__errpuc("too large","Very large image (corrupt?)");
   if (s->img_x > STBI_MAX_DIMENSIONS) return stbi__errpuc("too large","Very large image (corrupt?)");

   mr = info.mr;
   mg = info.mg;
   mb = info.mb;
   ma = info.ma;
   all_a = info.all_a;

   if (info.hsz == 12) {
      if (info.bpp < 24)
         psize = (info.offset - info.extra_read - 24) / 3;
   } else {
      if (info.bpp < 16)
         psize = (info.offset - info.extra_read - info.hsz) >> 2;
   }
   if (psize == 0) {
      STBI_ASSERT(info.offset == (int) (s->img_buffer - s->img_buffer_original));
   }

   if (info.bpp == 24 && ma == 0xff000000)
      s->img_n = 3;
   else
      s->img_n = ma ? 4 : 3;
   if (req_comp && req_comp >= 3) // we can directly decode 3 or 4
      target = req_comp;
   else
      target = s->img_n; // if they want monochrome, we'll post-convert

   // sanity-check size
   if (!stbi__mad3sizes_valid(target, s->img_x, s->img_y, 0))
      return stbi__errpuc("too large", "Corrupt BMP");

   out = (stbi_uc *) stbi__malloc_mad3(target, s->img_x, s->img_y, 0);
   if (!out) return stbi__errpuc("outofmem", "Out of memory");
   if (info.bpp < 16) {
      int z=0;
      if (psize == 0 || psize > 256) { STBI_FREE(out); return stbi__errpuc("invalid", "Corrupt BMP"); }
      for (i=0; i < psize; ++i) {
         pal[i][2] = stbi__get8(s);
         pal[i][1] = stbi__get8(s);
         pal[i][0] = stbi__get8(s);
         if (info.hsz != 12) stbi__get8(s);
         pal[i][3] = 255;
      }
      stbi__skip(s, info.offset - info.extra_read - info.hsz - psize * (info.hsz == 12 ? 3 : 4));
      if (info.bpp == 1) width = (s->img_x + 7) >> 3;
      else if (info.bpp == 4) width = (s->img_x + 1) >> 1;
      else if (info.bpp == 8) width = s->img_x;
      else { STBI_FREE(out); return stbi__errpuc("bad bpp", "Corrupt BMP"); }
      pad = (-width)&3;
      if (info.bpp == 1) {
         for (j=0; j < (int) s->img_y; ++j) {
            int bit_offset = 7, v = stbi__get8(s);
            for (i=0; i < (int) s->img_x; ++i) {
               int color = (v>>bit_offset)&0x1;
               out[z++] = pal[color][0];
               out[z++] = pal[color][1];
               out[z++] = pal[color][2];
               if (target == 4) out[z++] = 255;
               if (i+1 == (int) s->img_x) break;
               if((--bit_offset) < 0) {
                  bit_offset = 7;
                  v = stbi__get8(s);
               }
            }
            stbi__skip(s, pad);
         }
      } else {
         for (j=0; j < (int) s->img_y; ++j) {
            for (i=0; i < (int) s->img_x; i += 2) {
               int v=stbi__get8(s),v2=0;
               if (info.bpp == 4) {
                  v2 = v & 15;
                  v >>= 4;
               }
               out[z++] = pal[v][0];
               out[z++] = pal[v][1];
               out[z++] = pal[v][2];
               if (target == 4) out[z++] = 255;
               if (i+1 == (int) s->img_x) break;
               v = (info.bpp == 8) ? stbi__get8(s) : v2;
               out[z++] = pal[v][0];
               out[z++] = pal[v][1];
               out[z++] = pal[v][2];
               if (target == 4) out[z++] = 255;
            }
            stbi__skip(s, pad);
         }
      }
   } else {
      int rshift=0,gshift=0,bshift=0,ashift=0,rcount=0,gcount=0,bcount=0,acount=0;
      int z = 0;
      int easy=0;
      stbi__skip(s, info.offset - info.extra_read - info.hsz);
      if (info.bpp == 24) width = 3 * s->img_x;
      else if (info.bpp == 16) width = 2*s->img_x;
      else /* bpp = 32 and pad = 0 */ width=0;
      pad = (-width) & 3;
      if (info.bpp == 24) {
         easy = 1;
      } else if (info.bpp == 32) {
         if (mb == 0xff && mg =
#endif

// Targa Truevision - TGA
// by Jonathan Dummer
#ifndef STBI_NO_TGA
// returns STBI_rgb or whatever, 0 on error
static int stbi__tga_get_comp(int bits_per_pixel, int is_grey, int* is_rgb16)
{
   // only RGB or RGBA (incl. 16bit) or grey allowed
   if (is_rgb16) *is_rgb16 = 0;
   switch(bits_per_pixel) {
      case 8:  return STBI_grey;
      case 16: if(is_grey) return STBI_grey_alpha;
               // fallthrough
      case 15: if(is_rgb16) *is_rgb16 = 1;
               return STBI_rgb;
      case 24: // fallthrough
      case 32: return bits_per_pixel/8;
      default: return 0;
   }
}

/**
 * @brief Retrieves information about a TGA image file.
 *
 * This function reads the header of a TGA image file and extracts its dimensions,
 * color components, and other relevant information. It validates the TGA file format
 * and ensures that the image type is supported (e.g., RGB, indexed, grayscale, with or
 * without Run-Length Encoding (RLE)). If the file is valid, the function populates
 * the provided pointers with the image's width, height, and number of color components.
 *
 * @param s Pointer to the stbi__context structure representing the TGA file stream.
 * @param x Pointer to an integer where the width of the image will be stored.
 *          Can be NULL if the width is not needed.
 * @param y Pointer to an integer where the height of the image will be stored.
 *          Can be NULL if the height is not needed.
 * @param comp Pointer to an integer where the number of color components will be stored.
 *             Can be NULL if the number of components is not needed.
 *
 * @return Returns 1 if the TGA file is valid and the information was successfully retrieved.
 *         Returns 0 if the file is invalid or unsupported.
 */
static int stbi__tga_info(stbi__context *s, int *x, int *y, int *comp)
{
    int tga_w, tga_h, tga_comp, tga_image_type, tga_bits_per_pixel, tga_colormap_bpp;
    int sz, tga_colormap_type;
    stbi__get8(s);                   // discard Offset
    tga_colormap_type = stbi__get8(s); // colormap type
    if( tga_colormap_type > 1 ) {
        stbi__rewind(s);
        return 0;      // only RGB or indexed allowed
    }
    tga_image_type = stbi__get8(s); // image type
    if ( tga_colormap_type == 1 ) { // colormapped (paletted) image
        if (tga_image_type != 1 && tga_image_type != 9) {
            stbi__rewind(s);
            return 0;
        }
        stbi__skip(s,4);       // skip index of first colormap entry and number of entries
        sz = stbi__get8(s);    //   check bits per palette color entry
        if ( (sz != 8) && (sz != 15) && (sz != 16) && (sz != 24) && (sz != 32) ) {
            stbi__rewind(s);
            return 0;
        }
        stbi__skip(s,4);       // skip image x and y origin
        tga_colormap_bpp = sz;
    } else { // "normal" image w/o colormap - only RGB or grey allowed, +/- RLE
        if ( (tga_image_type != 2) && (tga_image_type != 3) && (tga_image_type != 10) && (tga_image_type != 11) ) {
            stbi__rewind(s);
            return 0; // only RGB or grey allowed, +/- RLE
        }
        stbi__skip(s,9); // skip colormap specification and image x/y origin
        tga_colormap_bpp = 0;
    }
    tga_w = stbi__get16le(s);
    if( tga_w < 1 ) {
        stbi__rewind(s);
        return 0;   // test width
    }
    tga_h = stbi__get16le(s);
    if( tga_h < 1 ) {
        stbi__rewind(s);
        return 0;   // test height
    }
    tga_bits_per_pixel = stbi__get8(s); // bits per pixel
    stbi__get8(s); // ignore alpha bits
    if (tga_colormap_bpp != 0) {
        if((tga_bits_per_pixel != 8) && (tga_bits_per_pixel != 16)) {
            // when using a colormap, tga_bits_per_pixel is the size of the indexes
            // I don't think anything but 8 or 16bit indexes makes sense
            stbi__rewind(s);
            return 0;
        }
        tga_comp = stbi__tga_get_comp(tga_colormap_bpp, 0, NULL);
    } else {
        tga_comp = stbi__tga_get_comp(tga_bits_per_pixel, (tga_image_type == 3) || (tga_image_type == 11), NULL);
    }
    if(!tga_comp) {
      stbi__rewind(s);
      return 0;
    }
    if (x) *x = tga_w;
    if (y) *y = tga_h;
    if (comp) *comp = tga_comp;
    return 1;                   // seems to have passed everything
}

/**
 * @brief Tests if the given stbi__context contains a valid TGA image.
 *
 * This method reads and validates the header of a TGA (Truevision Graphics Adapter) image
 * from the provided stbi__context. It checks the color type, image type, and other header
 * fields to ensure they conform to the expected TGA format. The method supports both
 * colormapped (paletted) and non-colormapped images, as well as various bit depths.
 *
 * @param s Pointer to the stbi__context containing the image data.
 * @return int Returns 1 if the image is a valid TGA image, otherwise returns 0.
 */
static int stbi__tga_test(stbi__context *s)
{
   int res = 0;
   int sz, tga_color_type;
   stbi__get8(s);      //   discard Offset
   tga_color_type = stbi__get8(s);   //   color type
   if ( tga_color_type > 1 ) goto errorEnd;   //   only RGB or indexed allowed
   sz = stbi__get8(s);   //   image type
   if ( tga_color_type == 1 ) { // colormapped (paletted) image
      if (sz != 1 && sz != 9) goto errorEnd; // colortype 1 demands image type 1 or 9
      stbi__skip(s,4);       // skip index of first colormap entry and number of entries
      sz = stbi__get8(s);    //   check bits per palette color entry
      if ( (sz != 8) && (sz != 15) && (sz != 16) && (sz != 24) && (sz != 32) ) goto errorEnd;
      stbi__skip(s,4);       // skip image x and y origin
   } else { // "normal" image w/o colormap
      if ( (sz != 2) && (sz != 3) && (sz != 10) && (sz != 11) ) goto errorEnd; // only RGB or grey allowed, +/- RLE
      stbi__skip(s,9); // skip colormap specification and image x/y origin
   }
   if ( stbi__get16le(s) < 1 ) goto errorEnd;      //   test width
   if ( stbi__get16le(s) < 1 ) goto errorEnd;      //   test height
   sz = stbi__get8(s);   //   bits per pixel
   if ( (tga_color_type == 1) && (sz != 8) && (sz != 16) ) goto errorEnd; // for colormapped images, bpp is size of an index
   if ( (sz != 8) && (sz != 15) && (sz != 16) && (sz != 24) && (sz != 32) ) goto errorEnd;

   res = 1; // if we got this far, everything's good and we can return 1 instead of 0

errorEnd:
   stbi__rewind(s);
   return res;
}

// read 16bit value and convert to 24bit RGB
static void stbi__tga_read_rgb16(stbi__context *s, stbi_uc* out)
{
   stbi__uint16 px = (stbi__uint16)stbi__get16le(s);
   stbi__uint16 fiveBitMask = 31;
   // we have 3 channels with 5bits each
   int r = (px >> 10) & fiveBitMask;
   int g = (px >> 5) & fiveBitMask;
   int b = px & fiveBitMask;
   // Note that this saves the data in RGB(A) order, so it doesn't need to be swapped later
   out[0] = (stbi_uc)((r * 255)/31);
   out[1] = (stbi_uc)((g * 255)/31);
   out[2] = (stbi_uc)((b * 255)/31);

   // some people claim that the most significant bit might be used for alpha
   // (possibly if an alpha-bit is set in the "image descriptor byte")
   // but that only made 16bit test images completely translucent..
   // so let's treat all 15 and 16bit TGAs as RGB with no alpha.
}

/**
 * Loads a TGA image from the provided context and decodes it into a raw pixel buffer.
 *
 * This function reads the TGA header, processes the image data, and decodes it into a format
 * suitable for further use. It supports various TGA features, including paletted images,
 * run-length encoding (RLE), and different pixel formats. The function also handles image
 * inversion and component reordering (e.g., swapping RGB to BGR) as needed.
 *
 * @param s The stbi__context object representing the input stream or file.
 * @param x Pointer to store the width of the loaded image.
 * @param y Pointer to store the height of the loaded image.
 * @param comp Pointer to store the number of components per pixel (e.g., 3 for RGB, 4 for RGBA).
 * @param req_comp The desired number of components per pixel (0 to preserve the original format).
 * @param ri Pointer to stbi__result_info for additional result metadata (currently unused).
 *
 * @return A pointer to the decoded pixel data on success, or NULL on failure. The caller is
 *         responsible for freeing the returned buffer using STBI_FREE.
 *
 * @note The function performs basic validation on the TGA header and data. If the image is
 *       too large or the format is unsupported, it returns NULL with an appropriate error message.
 *       The function also handles memory allocation failures gracefully.
 */
static void *stbi__tga_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   //   read in the TGA header stuff
   int tga_offset = stbi__get8(s);
   int tga_indexed = stbi__get8(s);
   int tga_image_type = stbi__get8(s);
   int tga_is_RLE = 0;
   int tga_palette_start = stbi__get16le(s);
   int tga_palette_len = stbi__get16le(s);
   int tga_palette_bits = stbi__get8(s);
   int tga_x_origin = stbi__get16le(s);
   int tga_y_origin = stbi__get16le(s);
   int tga_width = stbi__get16le(s);
   int tga_height = stbi__get16le(s);
   int tga_bits_per_pixel = stbi__get8(s);
   int tga_comp, tga_rgb16=0;
   int tga_inverted = stbi__get8(s);
   // int tga_alpha_bits = tga_inverted & 15; // the 4 lowest bits - unused (useless?)
   //   image data
   unsigned char *tga_data;
   unsigned char *tga_palette = NULL;
   int i, j;
   unsigned char raw_data[4] = {0};
   int RLE_count = 0;
   int RLE_repeating = 0;
   int read_next_pixel = 1;
   STBI_NOTUSED(ri);
   STBI_NOTUSED(tga_x_origin); // @TODO
   STBI_NOTUSED(tga_y_origin); // @TODO

   if (tga_height > STBI_MAX_DIMENSIONS) return stbi__errpuc("too large","Very large image (corrupt?)");
   if (tga_width > STBI_MAX_DIMENSIONS) return stbi__errpuc("too large","Very large image (corrupt?)");

   //   do a tiny bit of precessing
   if ( tga_image_type >= 8 )
   {
      tga_image_type -= 8;
      tga_is_RLE = 1;
   }
   tga_inverted = 1 - ((tga_inverted >> 5) & 1);

   //   If I'm paletted, then I'll use the number of bits from the palette
   if ( tga_indexed ) tga_comp = stbi__tga_get_comp(tga_palette_bits, 0, &tga_rgb16);
   else tga_comp = stbi__tga_get_comp(tga_bits_per_pixel, (tga_image_type == 3), &tga_rgb16);

   if(!tga_comp) // shouldn't really happen, stbi__tga_test() should have ensured basic consistency
      return stbi__errpuc("bad format", "Can't find out TGA pixelformat");

   //   tga info
   *x = tga_width;
   *y = tga_height;
   if (comp) *comp = tga_comp;

   if (!stbi__mad3sizes_valid(tga_width, tga_height, tga_comp, 0))
      return stbi__errpuc("too large", "Corrupt TGA");

   tga_data = (unsigned char*)stbi__malloc_mad3(tga_width, tga_height, tga_comp, 0);
   if (!tga_data) return stbi__errpuc("outofmem", "Out of memory");

   // skip to the data's starting position (offset usually = 0)
   stbi__skip(s, tga_offset );

   if ( !tga_indexed && !tga_is_RLE && !tga_rgb16 ) {
      for (i=0; i < tga_height; ++i) {
         int row = tga_inverted ? tga_height -i - 1 : i;
         stbi_uc *tga_row = tga_data + row*tga_width*tga_comp;
         stbi__getn(s, tga_row, tga_width * tga_comp);
      }
   } else  {
      //   do I need to load a palette?
      if ( tga_indexed)
      {
         if (tga_palette_len == 0) {  /* you have to have at least one entry! */
            STBI_FREE(tga_data);
            return stbi__errpuc("bad palette", "Corrupt TGA");
         }

         //   any data to skip? (offset usually = 0)
         stbi__skip(s, tga_palette_start );
         //   load the palette
         tga_palette = (unsigned char*)stbi__malloc_mad2(tga_palette_len, tga_comp, 0);
         if (!tga_palette) {
            STBI_FREE(tga_data);
            return stbi__errpuc("outofmem", "Out of memory");
         }
         if (tga_rgb16) {
            stbi_uc *pal_entry = tga_palette;
            STBI_ASSERT(tga_comp == STBI_rgb);
            for (i=0; i < tga_palette_len; ++i) {
               stbi__tga_read_rgb16(s, pal_entry);
               pal_entry += tga_comp;
            }
         } else if (!stbi__getn(s, tga_palette, tga_palette_len * tga_comp)) {
               STBI_FREE(tga_data);
               STBI_FREE(tga_palette);
               return stbi__errpuc("bad palette", "Corrupt TGA");
         }
      }
      //   load the data
      for (i=0; i < tga_width * tga_height; ++i)
      {
         //   if I'm in RLE mode, do I need to get a RLE stbi__pngchunk?
         if ( tga_is_RLE )
         {
            if ( RLE_count == 0 )
            {
               //   yep, get the next byte as a RLE command
               int RLE_cmd = stbi__get8(s);
               RLE_count = 1 + (RLE_cmd & 127);
               RLE_repeating = RLE_cmd >> 7;
               read_next_pixel = 1;
            } else if ( !RLE_repeating )
            {
               read_next_pixel = 1;
            }
         } else
         {
            read_next_pixel = 1;
         }
         //   OK, if I need to read a pixel, do it now
         if ( read_next_pixel )
         {
            //   load however much data we did have
            if ( tga_indexed )
            {
               // read in index, then perform the lookup
               int pal_idx = (tga_bits_per_pixel == 8) ? stbi__get8(s) : stbi__get16le(s);
               if ( pal_idx >= tga_palette_len ) {
                  // invalid index
                  pal_idx = 0;
               }
               pal_idx *= tga_comp;
               for (j = 0; j < tga_comp; ++j) {
                  raw_data[j] = tga_palette[pal_idx+j];
               }
            } else if(tga_rgb16) {
               STBI_ASSERT(tga_comp == STBI_rgb);
               stbi__tga_read_rgb16(s, raw_data);
            } else {
               //   read in the data raw
               for (j = 0; j < tga_comp; ++j) {
                  raw_data[j] = stbi__get8(s);
               }
            }
            //   clear the reading flag for the next pixel
            read_next_pixel = 0;
         } // end of reading a pixel

         // copy data
         for (j = 0; j < tga_comp; ++j)
           tga_data[i*tga_comp+j] = raw_data[j];

         //   in case we're in RLE mode, keep counting down
         --RLE_count;
      }
      //   do I need to invert the image?
      if ( tga_inverted )
      {
         for (j = 0; j*2 < tga_height; ++j)
         {
            int index1 = j * tga_width * tga_comp;
            int index2 = (tga_height - 1 - j) * tga_width * tga_comp;
            for (i = tga_width * tga_comp; i > 0; --i)
            {
               unsigned char temp = tga_data[index1];
               tga_data[index1] = tga_data[index2];
               tga_data[index2] = temp;
               ++index1;
               ++index2;
            }
         }
      }
      //   clear my palette, if I had one
      if ( tga_palette != NULL )
      {
         STBI_FREE( tga_palette );
      }
   }

   // swap RGB - if the source data was RGB16, it already is in the right order
   if (tga_comp >= 3 && !tga_rgb16)
   {
      unsigned char* tga_pixel = tga_data;
      for (i=0; i < tga_width * tga_height; ++i)
      {
         unsigned char temp = tga_pixel[0];
         tga_pixel[0] = tga_pixel[2];
         tga_pixel[2] = temp;
         tga_pixel += tga_comp;
      }
   }

   // convert to target component count
   if (req_comp && req_comp != tga_comp)
      tga_data = stbi__convert_format(tga_data, tga_comp, req_comp, tga_width, tga_height);

   //   the things I do to get rid of an error message, and yet keep
   //   Microsoft's C compilers happy... [8^(
   tga_palette_start = tga_palette_len = tga_palette_bits =
         tga_x_origin = tga_y_origin = 0;
   STBI_NOTUSED(tga_palette_start);
   //   OK, done
   return tga_data;
}
#endif

// *************************************************************************************************
// Photoshop PSD loader -- PD by Thatcher Ulrich, integration by Nicolas Schulz, tweaked by STB


// *************************************************************************************************
// Softimage PIC loader
// by Tom Seddon
//
// See http://softimage.wiki.softimage.com/index.php/INFO:_PIC_file_format
// See http://ozviz.wasp.uwa.edu.au/~pbourke/dataformats/softimagepic/


// *************************************************************************************************
// GIF loader -- public domain by Jean-Marc Lienher -- simplified/shrunk by stb

#ifndef STBI_NO_GIF
typedef struct
{
   stbi__int16 prefix;
   stbi_uc first;
   stbi_uc suffix;
} stbi__gif_lzw;

typedef struct
{
   int w,h;
   stbi_uc *out;                 // output buffer (always 4 components)
   stbi_uc *background;          // The current "background" as far as a gif is concerned
   stbi_uc *history;
   int flags, bgindex, ratio, transparent, eflags;
   stbi_uc  pal[256][4];
   stbi_uc lpal[256][4];
   stbi__gif_lzw codes[8192];
   stbi_uc *color_table;
   int parse, step;
   int lflags;
   int start_x, start_y;
   int max_x, max_y;
   int cur_x, cur_y;
   int line_size;
   int delay;
} stbi__gif;

/**
 * @brief Tests whether the given data represents a raw GIF image.
 *
 * This function checks if the provided data starts with the GIF signature.
 * The GIF signature consists of the characters 'G', 'I', 'F', '8', followed by either '7' or '9',
 * and then the character 'a'. If the data matches this pattern, the function assumes it is a valid
 * raw GIF image.
 *
 * @param s A pointer to the stbi__context structure that contains the data to be tested.
 * @return Returns 1 if the data matches the GIF signature, otherwise returns 0.
 */
static int stbi__gif_test_raw(stbi__context *s)
{
   int sz;
   if (stbi__get8(s) != 'G' || stbi__get8(s) != 'I' || stbi__get8(s) != 'F' || stbi__get8(s) != '8') return 0;
   sz = stbi__get8(s);
   if (sz != '9' && sz != '7') return 0;
   if (stbi__get8(s) != 'a') return 0;
   return 1;
}

/**
 * @brief Tests whether the given data represents a valid GIF image.
 *
 * This function checks if the provided data stream contains a valid GIF image
 * by calling `stbi__gif_test_raw` to perform the actual test. After the test,
 * the stream is rewound to its original position using `stbi__rewind` so that
 * subsequent operations can start reading from the beginning of the stream.
 *
 * @param s A pointer to the `stbi__context` structure representing the data stream.
 * @return Returns a non-zero value if the data is a valid GIF image, otherwise returns 0.
 */
static int stbi__gif_test(stbi__context *s)
{
   int r = stbi__gif_test_raw(s);
   stbi__rewind(s);
   return r;
}

/**
 * Parses a color table from a GIF file and stores it in the provided palette array.
 * 
 * This function reads a sequence of RGB values from the input context `s` and stores them
 * in the `pal` array. Each entry in the color table is represented as an array of 4 bytes:
 * [R, G, B, A], where R, G, and B are the red, green, and blue components, and A is the
 * alpha (transparency) component. The alpha value is set to 0 for the entry specified by
 * `transp` (indicating full transparency) and 255 for all other entries (indicating full
 * opacity).
 *
 * @param s             The input context from which the color table is read.
 * @param pal           A 256x4 array where the parsed color table will be stored.
 * @param num_entries   The number of entries in the color table to parse.
 * @param transp        The index of the transparent color in the color table, or -1 if
 *                      there is no transparent color.
 */
static void stbi__gif_parse_colortable(stbi__context *s, stbi_uc pal[256][4], int num_entries, int transp)
{
   int i;
   for (i=0; i < num_entries; ++i) {
      pal[i][2] = stbi__get8(s);
      pal[i][1] = stbi__get8(s);
      pal[i][0] = stbi__get8(s);
      pal[i][3] = transp == i ? 0 : 255;
   }
}

/**
 * Parses the header of a GIF image file and extracts relevant information.
 * 
 * This function reads the initial bytes of a GIF file to verify its validity and
 * extracts key metadata such as width, height, flags, background index, and aspect ratio.
 * It also handles the color table if present. The function is designed to work with
 * the stb_image library's internal context and GIF structure.
 *
 * @param s Pointer to the stbi__context structure, which holds the input stream and state.
 * @param g Pointer to the stbi__gif structure, where the parsed GIF header data will be stored.
 * @param comp Pointer to an integer where the number of components (channels) will be stored.
 *             This is typically set to 4 (RGBA) since GIFs can have transparency.
 * @param is_info An integer flag indicating whether only the header information should be parsed.
 *                If non-zero, the function returns after parsing the header without further processing.
 *
 * @return Returns 1 on success, indicating that the header was parsed correctly.
 *         Returns 0 on failure, and sets the appropriate error message in stbi__g_failure_reason.
 *
 * @note The function checks for the GIF signature ("GIF8") and version ("7" or "9").
 *       It also validates the image dimensions against STBI_MAX_DIMENSIONS to prevent
 *       processing excessively large images, which may indicate corruption.
 */
static int stbi__gif_header(stbi__context *s, stbi__gif *g, int *comp, int is_info)
{
   stbi_uc version;
   if (stbi__get8(s) != 'G' || stbi__get8(s) != 'I' || stbi__get8(s) != 'F' || stbi__get8(s) != '8')
      return stbi__err("not GIF", "Corrupt GIF");

   version = stbi__get8(s);
   if (version != '7' && version != '9')    return stbi__err("not GIF", "Corrupt GIF");
   if (stbi__get8(s) != 'a')                return stbi__err("not GIF", "Corrupt GIF");

   stbi__g_failure_reason = "";
   g->w = stbi__get16le(s);
   g->h = stbi__get16le(s);
   g->flags = stbi__get8(s);
   g->bgindex = stbi__get8(s);
   g->ratio = stbi__get8(s);
   g->transparent = -1;

   if (g->w > STBI_MAX_DIMENSIONS) return stbi__err("too large","Very large image (corrupt?)");
   if (g->h > STBI_MAX_DIMENSIONS) return stbi__err("too large","Very large image (corrupt?)");

   if (comp != 0) *comp = 4;  // can't actually tell whether it's 3 or 4 until we parse the comments

   if (is_info) return 1;

   if (g->flags & 0x80)
      stbi__gif_parse_colortable(s,g->pal, 2 << (g->flags & 7), -1);

   return 1;
}

/**
 * Retrieves the dimensions and number of components of a GIF image without fully decoding it.
 *
 * This function reads the header of a GIF image from the provided `stbi__context` and extracts
 * the width, height, and number of components (channels) of the image. The function is designed
 * to be lightweight and efficient, as it does not decode the entire image data.
 *
 * @param s       Pointer to the `stbi__context` object that contains the GIF image data.
 * @param x       Pointer to an integer where the width of the image will be stored. Can be NULL.
 * @param y       Pointer to an integer where the height of the image will be stored. Can be NULL.
 * @param comp    Pointer to an integer where the number of components (channels) will be stored. Can be NULL.
 *
 * @return        Returns 1 if the GIF header was successfully read and the information was extracted.
 *                Returns 0 if the header is invalid or if an error occurred during reading.
 */
static int stbi__gif_info_raw(stbi__context *s, int *x, int *y, int *comp)
{
   stbi__gif* g = (stbi__gif*) stbi__malloc(sizeof(stbi__gif));
   if (!stbi__gif_header(s, g, comp, 1)) {
      STBI_FREE(g);
      stbi__rewind( s );
      return 0;
   }
   if (x) *x = g->w;
   if (y) *y = g->h;
   STBI_FREE(g);
   return 1;
}

/**
 * @brief Outputs a GIF code to the decoded image buffer.
 *
 * This function decodes a GIF code and writes the corresponding pixel data to the output buffer.
 * It recursively processes the prefix of the code to handle the linked-list structure of GIF codes,
 * which is stored in reverse order. The function also handles transparency by skipping pixels with
 * an alpha value less than or equal to 128. Additionally, it manages the current position in the
 * image buffer and adjusts the position when the end of a line is reached, handling interlacing
 * if necessary.
 *
 * @param g Pointer to the stbi__gif structure containing the GIF data and state.
 * @param code The GIF code to be decoded and output.
 */
static void stbi__out_gif_code(stbi__gif *g, stbi__uint16 code)
{
   stbi_uc *p, *c;
   int idx;

   // recurse to decode the prefixes, since the linked-list is backwards,
   // and working backwards through an interleaved image would be nasty
   if (g->codes[code].prefix >= 0)
      stbi__out_gif_code(g, g->codes[code].prefix);

   if (g->cur_y >= g->max_y) return;

   idx = g->cur_x + g->cur_y;
   p = &g->out[idx];
   g->history[idx / 4] = 1;

   c = &g->color_table[g->codes[code].suffix * 4];
   if (c[3] > 128) { // don't render transparent pixels;
      p[0] = c[2];
      p[1] = c[1];
      p[2] = c[0];
      p[3] = c[3];
   }
   g->cur_x += 4;

   if (g->cur_x >= g->max_x) {
      g->cur_x = g->start_x;
      g->cur_y += g->step;

      while (g->cur_y >= g->max_y && g->parse > 0) {
         g->step = (1 << g->parse) * g->line_size;
         g->cur_y = g->start_y + (g->step >> 1);
         --g->parse;
      }
   }
}

/**
 * Processes the raster data of a GIF image using LZW (Lempel-Ziv-Welch) decompression.
 * This method reads and decodes the compressed image data from the provided `stbi__context`
 * and writes the decompressed pixel data to the `stbi__gif` structure.
 *
 * The method handles the following:
 * - Initialization of the LZW code table based on the code size specified in the GIF.
 * - Reading and processing the compressed data in blocks.
 * - Handling clear codes, which reset the LZW code table.
 * - Handling end-of-stream codes, which indicate the end of the image data.
 * - Detecting and reporting errors such as illegal codes or exceeding the maximum code table size.
 *
 * @param s Pointer to the `stbi__context` structure containing the input data stream.
 * @param g Pointer to the `stbi__gif` structure where the decompressed image data will be stored.
 * @return A pointer to the decompressed pixel data on success, or NULL on failure.
 */
static stbi_uc *stbi__process_gif_raster(stbi__context *s, stbi__gif *g)
{
   stbi_uc lzw_cs;
   stbi__int32 len, init_code;
   stbi__uint32 first;
   stbi__int32 codesize, codemask, avail, oldcode, bits, valid_bits, clear;
   stbi__gif_lzw *p;

   lzw_cs = stbi__get8(s);
   if (lzw_cs > 12) return NULL;
   clear = 1 << lzw_cs;
   first = 1;
   codesize = lzw_cs + 1;
   codemask = (1 << codesize) - 1;
   bits = 0;
   valid_bits = 0;
   for (init_code = 0; init_code < clear; init_code++) {
      g->codes[init_code].prefix = -1;
      g->codes[init_code].first = (stbi_uc) init_code;
      g->codes[init_code].suffix = (stbi_uc) init_code;
   }

   // support no starting clear code
   avail = clear+2;
   oldcode = -1;

   len = 0;
   for(;;) {
      if (valid_bits < codesize) {
         if (len == 0) {
            len = stbi__get8(s); // start new block
            if (len == 0)
               return g->out;
         }
         --len;
         bits |= (stbi__int32) stbi__get8(s) << valid_bits;
         valid_bits += 8;
      } else {
         stbi__int32 code = bits & codemask;
         bits >>= codesize;
         valid_bits -= codesize;
         // @OPTIMIZE: is there some way we can accelerate the non-clear path?
         if (code == clear) {  // clear code
            codesize = lzw_cs + 1;
            codemask = (1 << codesize) - 1;
            avail = clear + 2;
            oldcode = -1;
            first = 0;
         } else if (code == clear + 1) { // end of stream code
            stbi__skip(s, len);
            while ((len = stbi__get8(s)) > 0)
               stbi__skip(s,len);
            return g->out;
         } else if (code <= avail) {
            if (first) {
               return stbi__errpuc("no clear code", "Corrupt GIF");
            }

            if (oldcode >= 0) {
               p = &g->codes[avail++];
               if (avail > 8192) {
                  return stbi__errpuc("too many codes", "Corrupt GIF");
               }

               p->prefix = (stbi__int16) oldcode;
               p->first = g->codes[oldcode].first;
               p->suffix = (code == avail) ? p->first : g->codes[code].first;
            } else if (code == avail)
               return stbi__errpuc("illegal code in raster", "Corrupt GIF");

            stbi__out_gif_code(g, (stbi__uint16) code);

            if ((avail & codemask) == 0 && avail <= 0x0FFF) {
               codesize++;
               codemask = (1 << codesize) - 1;
            }

            oldcode = code;
         } else {
            return stbi__errpuc("illegal code in raster", "Corrupt GIF");
         }
      }
   }
}

// this function is designed to support animated gifs, although stb_image doesn't support it
// two back is the image from two frames ago, used for a very specific disposal format
static stbi_uc *stbi__gif_load_next(stbi__context *s, stbi__gif *g, int *comp, int req_comp, stbi_uc *two_back)
{
   int dispose;
   int first_frame;
   int pi;
   int pcount;
   STBI_NOTUSED(req_comp);

   // on first frame, any non-written pixels get the background colour (non-transparent)
   first_frame = 0;
   if (g->out == 0) {
      if (!stbi__gif_header(s, g, comp,0)) return 0; // stbi__g_failure_reason set by stbi__gif_header
      if (!stbi__mad3sizes_valid(4, g->w, g->h, 0))
         return stbi__errpuc("too large", "GIF image is too large");
      pcount = g->w * g->h;
      g->out = (stbi_uc *) stbi__malloc(4 * pcount);
      g->background = (stbi_uc *) stbi__malloc(4 * pcount);
      g->history = (stbi_uc *) stbi__malloc(pcount);
      if (!g->out || !g->background || !g->history)
         return stbi__errpuc("outofmem", "Out of memory");

      // image is treated as "transparent" at the start - ie, nothing overwrites the current background;
      // background colour is only used for pixels that are not rendered first frame, after that "background"
      // color refers to the color that was there the previous frame.
      memset(g->out, 0x00, 4 * pcount);
      memset(g->background, 0x00, 4 * pcount); // state of the background (starts transparent)
      memset(g->history, 0x00, pcount);        // pixels that were affected previous frame
      first_frame = 1;
   } else {
      // second frame - how do we dispose of the previous one?
      dispose = (g->eflags & 0x1C) >> 2;
      pcount = g->w * g->h;

      if ((dispose == 3) && (two_back == 0)) {
         dispose = 2; // if I don't have an image to revert back to, default to the old background
      }

      if (dispose == 3) { // use previous graphic
         for (pi = 0; pi < pcount; ++pi) {
            if (g->history[pi]) {
               memcpy( &g->out[pi * 4], &two_back[pi * 4], 4 );
            }
         }
      } else if (dispose == 2) {
         // restore what was changed last frame to background before that frame;
         for (pi = 0; pi < pcount; ++pi) {
            if (g->history[pi]) {
               memcpy( &g->out[pi * 4], &g->background[pi * 4], 4 );
            }
         }
      } else {
         // This is a non-disposal case eithe way, so just
         // leave the pixels as is, and they will become the new background
         // 1: do not dispose
         // 0:  not specified.
      }

      // background is what out is after the undoing of the previou frame;
      memcpy( g->background, g->out, 4 * g->w * g->h );
   }

   // clear my history;
   memset( g->history, 0x00, g->w * g->h );        // pixels that were affected previous frame

   for (;;) {
      int tag = stbi__get8(s);
      switch (tag) {
         case 0x2C: /* Image Descriptor */
         {
            stbi__int32 x, y, w, h;
            stbi_uc *o;

            x = stbi__get16le(s);
            y = stbi__get16le(s);
            w = stbi__get16le(s);
            h = stbi__get16le(s);
            if (((x + w) > (g->w)) || ((y + h) > (g->h)))
               return stbi__errpuc("bad Image Descriptor", "Corrupt GIF");

            g->line_size = g->w * 4;
            g->start_x = x * 4;
            g->start_y = y * g->line_size;
            g->max_x   = g->start_x + w * 4;
            g->max_y   = g->start_y + h * g->line_size;
            g->cur_x   = g->start_x;
            g->cur_y   = g->start_y;

            // if the width of the specified rectangle is 0, that means
            // we may not see *any* pixels or the image is malformed;
            // to make sure this is caught, move the current y down to
            // max_y (which is what out_gif_code checks).
            if (w == 0)
               g->cur_y = g->max_y;

            g->lflags = stbi__get8(s);

            if (g->lflags & 0x40) {
               g->step = 8 * g->line_size; // first interlaced spacing
               g->parse = 3;
            } else {
               g->step = g->line_size;
               g->parse = 0;
            }

            if (g->lflags & 0x80) {
               stbi__gif_parse_colortable(s,g->lpal, 2 << (g->lflags & 7), g->eflags & 0x01 ? g->transparent : -1);
               g->color_table = (stbi_uc *) g->lpal;
            } else if (g->flags & 0x80) {
               g->color_table = (stbi_uc *) g->pal;
            } else
               return stbi__errpuc("missing color table", "Corrupt GIF");

            o = stbi__process_gif_raster(s, g);
            if (!o) return NULL;

            // if this was the first frame,
            pcount = g->w * g->h;
            if (first_frame && (g->bgindex > 0)) {
               // if first frame, any pixel not drawn to gets the background color
               for (pi = 0; pi < pcount; ++pi) {
                  if (g->history[pi] == 0) {
                     g->pal[g->bgindex][3] = 255; // just in case it was made transparent, undo that; It will be reset next frame if need be;
                     memcpy( &g->out[pi * 4], &g->pal[g->bgindex], 4 );
                  }
               }
            }

            return o;
         }

         case 0x21: // Comment Extension.
         {
            int len;
            int ext = stbi__get8(s);
            if (ext == 0xF9) { // Graphic Control Extension.
               len = stbi__get8(s);
               if (len == 4) {
                  g->eflags = stbi__get8(s);
                  g->delay = 10 * stbi__get16le(s); // delay - 1/100th of a second, saving as 1/1000ths.

                  // unset old transparent
                  if (g->transparent >= 0) {
                     g->pal[g->transparent][3] = 255;
                  }
                  if (g->eflags & 0x01) {
                     g->transparent = stbi__get8(s);
                     if (g->transparent >= 0) {
                        g->pal[g->transparent][3] = 0;
                     }
                  } else {
                     // don't need transparent
                     stbi__skip(s, 1);
                     g->transparent = -1;
                  }
               } else {
                  stbi__skip(s, len);
                  break;
               }
            }
            while ((len = stbi__get8(s)) != 0) {
               stbi__skip(s, len);
            }
            break;
         }

         case 0x3B: // gif stream termination code
            return (stbi_uc *) s; // using '1' causes warning on some compilers

         default:
            return stbi__errpuc("unknown code", "Corrupt GIF");
      }
   }
}

/**
 * @brief Loads a GIF image from the provided input context and returns the decoded image data.
 *
 * This function reads and decodes a GIF image from the given `stbi__context` input stream. It supports
 * both single-frame and multi-frame (animated) GIFs. For animated GIFs, it returns the concatenated
 * image data for all frames and optionally provides the frame delays.
 *
 * @param s The input context containing the GIF data.
 * @param delays If not NULL, this will be populated with an array of frame delays (in milliseconds)
 *              for animated GIFs. The caller is responsible for freeing this memory.
 * @param x If not NULL, this will be populated with the width of the GIF image.
 * @param y If not NULL, this will be populated with the height of the GIF image.
 * @param z If not NULL, this will be populated with the number of layers (frames) in the GIF.
 * @param comp If not NULL, this will be populated with the number of components per pixel in the
 *             decoded image (e.g., 4 for RGBA).
 * @param req_comp The desired number of components per pixel in the output image. If 0, the original
 *                 format is preserved. Otherwise, the image is converted to the specified format.
 *
 * @return A pointer to the decoded image data. For animated GIFs, this is a concatenation of all
 *         frames. The caller is responsible for freeing this memory. Returns NULL on failure.
 */
static void *stbi__load_gif_main(stbi__context *s, int **delays, int *x, int *y, int *z, int *comp, int req_comp)
{
   if (stbi__gif_test(s)) {
      int layers = 0;
      stbi_uc *u = 0;
      stbi_uc *out = 0;
      stbi_uc *two_back = 0;
      stbi__gif g;
      int stride;
      int out_size = 0;
      int delays_size = 0;
      memset(&g, 0, sizeof(g));
      if (delays) {
         *delays = 0;
      }

      do {
         u = stbi__gif_load_next(s, &g, comp, req_comp, two_back);
         if (u == (stbi_uc *) s) u = 0;  // end of animated gif marker

         if (u) {
            *x = g.w;
            *y = g.h;
            ++layers;
            stride = g.w * g.h * 4;

            if (out) {
               void *tmp = (stbi_uc*) STBI_REALLOC_SIZED( out, out_size, layers * stride );
               if (NULL == tmp) {
                  STBI_FREE(g.out);
                  STBI_FREE(g.history);
                  STBI_FREE(g.background);
                  return stbi__errpuc("outofmem", "Out of memory");
               }
               else {
                   out = (stbi_uc*) tmp;
                   out_size = layers * stride;
               }

               if (delays) {
                  *delays = (int*) STBI_REALLOC_SIZED( *delays, delays_size, sizeof(int) * layers );
                  delays_size = layers * sizeof(int);
               }
            } else {
               out = (stbi_uc*)stbi__malloc( layers * stride );
               out_size = layers * stride;
               if (delays) {
                  *delays = (int*) stbi__malloc( layers * sizeof(int) );
                  delays_size = layers * sizeof(int);
               }
            }
            memcpy( out + ((layers - 1) * stride), u, stride );
            if (layers >= 2) {
               two_back = out - 2 * stride;
            }

            if (delays) {
               (*delays)[layers - 1U] = g.delay;
            }
         }
      } while (u != 0);

      // free temp buffer;
      STBI_FREE(g.out);
      STBI_FREE(g.history);
      STBI_FREE(g.background);

      // do the final conversion after loading everything;
      if (req_comp && req_comp != 4)
         out = stbi__convert_format(out, 4, req_comp, layers * g.w, g.h);

      *z = layers;
      return out;
   } else {
      return stbi__errpuc("not GIF", "Image was not as a gif type.");
   }
}

/**
 * Loads a GIF image from the provided stbi__context and decodes it into a pixel buffer.
 *
 * This function reads and decodes a GIF image from the given input context `s`. It extracts
 * the image dimensions, components, and pixel data, and optionally converts the pixel format
 * to the requested number of components (`req_comp`). The function handles both single-frame
 * and multi-frame GIFs, but only returns the first frame's pixel data.
 *
 * @param s The input context containing the GIF data.
 * @param x Pointer to store the width of the decoded image.
 * @param y Pointer to store the height of the decoded image.
 * @param comp Pointer to store the number of components in the decoded image.
 * @param req_comp The desired number of components in the output pixel buffer. If 0, the
 *                 original format is preserved.
 * @param ri Pointer to a stbi__result_info structure (unused in this function).
 *
 * @return A pointer to the decoded pixel buffer on success, or NULL if the GIF could not be
 *         decoded. The caller is responsible for freeing the returned buffer using STBI_FREE.
 */
static void *stbi__gif_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   stbi_uc *u = 0;
   stbi__gif g;
   memset(&g, 0, sizeof(g));
   STBI_NOTUSED(ri);

   u = stbi__gif_load_next(s, &g, comp, req_comp, 0);
   if (u == (stbi_uc *) s) u = 0;  // end of animated gif marker
   if (u) {
      *x = g.w;
      *y = g.h;

      // moved conversion to after successful load so that the same
      // can be done for multiple frames.
      if (req_comp && req_comp != 4)
         u = stbi__convert_format(u, 4, req_comp, g.w, g.h);
   } else if (g.out) {
      // if there was an error and we allocated an image buffer, free it!
      STBI_FREE(g.out);
   }

   // free buffers needed for multiple frame loading;
   STBI_FREE(g.history);
   STBI_FREE(g.background);

   return u;
}

/**
 * Retrieves the dimensions and number of components of a GIF image.
 * 
 * This function reads the header of a GIF image and extracts its width, height,
 * and the number of color components. The information is returned via the provided
 * pointers. It internally calls `stbi__gif_info_raw` to perform the actual parsing.
 * 
 * @param s     Pointer to the stbi__context structure representing the image data.
 * @param x     Pointer to an integer where the width of the image will be stored.
 * @param y     Pointer to an integer where the height of the image will be stored.
 * @param comp  Pointer to an integer where the number of color components will be stored.
 * 
 * @return      Returns 1 on success, indicating that the information was successfully
 *              retrieved. Returns 0 on failure, typically due to invalid or unsupported
 *              image data.
 */
static int stbi__gif_info(stbi__context *s, int *x, int *y, int *comp)
{
   return stbi__gif_info_raw(s,x,y,comp);
}
#endif

// *************************************************************************************************
// Radiance RGBE HDR loader
// originally by Nicolas Schulz

#ifndef STBI_NO_BMP
/**
 * Retrieves the basic information about a BMP image, including its dimensions and number of components.
 *
 * This function parses the header of a BMP image to extract its width, height, and the number of color
 * components (e.g., RGB or RGBA). The information is stored in the provided output parameters.
 *
 * @param s       A pointer to the `stbi__context` structure that holds the image data and state.
 * @param x       A pointer to an integer where the width of the image will be stored. Can be NULL if not needed.
 * @param y       A pointer to an integer where the height of the image will be stored. Can be NULL if not needed.
 * @param comp    A pointer to an integer where the number of color components will be stored. Can be NULL if not needed.
 *
 * @return        Returns 1 if the BMP header was successfully parsed and the information was retrieved.
 *                Returns 0 if the header parsing failed (e.g., invalid BMP format).
 */
static int stbi__bmp_info(stbi__context *s, int *x, int *y, int *comp)
{
   void *p;
   stbi__bmp_data info;

   info.all_a = 255;
   p = stbi__bmp_parse_header(s, &info);
   stbi__rewind( s );
   if (p == NULL)
      return 0;
   if (x) *x = s->img_x;
   if (y) *y = s->img_y;
   if (comp) {
      if (info.bpp == 24 && info.ma == 0xff000000)
         *comp = 3;
      else
         *comp = info.ma ? 4 : 3;
   }
   return 1;
}
#endif



// *************************************************************************************************
// Portable Gray Map and Portable Pixel Map loader
// by Ken Miller
//
// PGM: http://netpbm.sourceforge.net/doc/pgm.html
// PPM: http://netpbm.sourceforge.net/doc/ppm.html
//
// Known limitations:
//    Does not support comments in the header section
//    Does not support ASCII image data (formats P2 and P3)
//    Does not support 16-bit-per-channel

#ifndef STBI_NO_PNM

/**
 * @brief Tests if the given stbi__context contains a PNM (Portable Any Map) file.
 *
 * This function reads the first two bytes from the provided stbi__context and checks
 * if they match the magic number for a PNM file. The first byte must be 'P', and the
 * second byte must be either '5' (for PGM grayscale) or '6' (for PPM color). If the
 * bytes do not match, the function rewinds the context and returns 0. If they match,
 * the function returns 1, indicating that the file is likely a valid PNM file.
 *
 * @param s Pointer to the stbi__context to be tested.
 * @return int Returns 1 if the context contains a PNM file, otherwise returns 0.
 */
static int      stbi__pnm_test(stbi__context *s)
{
   char p, t;
   p = (char) stbi__get8(s);
   t = (char) stbi__get8(s);
   if (p != 'P' || (t != '5' && t != '6')) {
       stbi__rewind( s );
       return 0;
   }
   return 1;
}

/**
 * Loads a PNM (Portable Any Map) image from the given input context and decodes it into a raw pixel buffer.
 * 
 * This function reads the PNM image data from the provided `stbi__context` and decodes it into a dynamically
 * allocated buffer of raw pixel values. The image dimensions, number of components, and requested output format
 * are handled accordingly. The function supports both PBM, PGM, and PPM formats.
 *
 * @param s          The input context containing the PNM image data.
 * @param x          Pointer to store the width of the decoded image.
 * @param y          Pointer to store the height of the decoded image.
 * @param comp       Pointer to store the number of components in the decoded image (e.g., 1 for grayscale, 3 for RGB).
 * @param req_comp   The desired number of components in the output image. If 0, the original number of components is used.
 * @param ri         Pointer to a result info structure (unused in this function).
 *
 * @return           A pointer to the decoded image buffer on success, or NULL on failure. The caller is responsible
 *                   for freeing the returned buffer using `STBI_FREE`.
 *
 * @note             If the image dimensions exceed `STBI_MAX_DIMENSIONS`, the function returns NULL with an error.
 *                   If memory allocation fails, the function returns NULL with an "Out of memory" error.
 *                   If the requested number of components differs from the original, the image is converted to the
 *                   desired format, and the original buffer is freed.
 */
static void *stbi__pnm_load(stbi__context *s, int *x, int *y, int *comp, int req_comp, stbi__result_info *ri)
{
   stbi_uc *out;
   STBI_NOTUSED(ri);

   if (!stbi__pnm_info(s, (int *)&s->img_x, (int *)&s->img_y, (int *)&s->img_n))
      return 0;

   if (s->img_y > STBI_MAX_DIMENSIONS) return stbi__errpuc("too large","Very large image (corrupt?)");
   if (s->img_x > STBI_MAX_DIMENSIONS) return stbi__errpuc("too large","Very large image (corrupt?)");

   *x = s->img_x;
   *y = s->img_y;
   if (comp) *comp = s->img_n;

   if (!stbi__mad3sizes_valid(s->img_n, s->img_x, s->img_y, 0))
      return stbi__errpuc("too large", "PNM too large");

   out = (stbi_uc *) stbi__malloc_mad3(s->img_n, s->img_x, s->img_y, 0);
   if (!out) return stbi__errpuc("outofmem", "Out of memory");
   stbi__getn(s, out, s->img_n * s->img_x * s->img_y);

   if (req_comp && req_comp != s->img_n) {
      out = stbi__convert_format(out, s->img_n, req_comp, s->img_x, s->img_y);
      if (out == NULL) return out; // stbi__convert_format frees input on failure
   }
   return out;
}

/**
 * @brief Checks if the given character is a whitespace character.
 *
 * This function determines whether the provided character is a whitespace character.
 * It checks against the following characters: space (' '), horizontal tab ('\t'),
 * newline ('\n'), vertical tab ('\v'), form feed ('\f'), and carriage return ('\r').
 *
 * @param c The character to be checked.
 * @return Returns 1 if the character is a whitespace character, otherwise returns 0.
 */
static int      stbi__pnm_isspace(char c)
{
   return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/**
 * Skips whitespace and comments in a PNM (Portable Any Map) file.
 *
 * This function reads characters from the input stream `s` and skips over any whitespace characters
 * and comments. In PNM files, comments start with the '#' character and continue until the end of
 * the line. The function updates the character pointed to by `c` with the next non-whitespace,
 * non-comment character.
 *
 * The function continues processing until it encounters a non-whitespace, non-comment character or
 * reaches the end of the file.
 *
 * @param s A pointer to the `stbi__context` structure representing the input stream.
 * @param c A pointer to a character that will be updated with the next non-whitespace,
 *          non-comment character from the stream.
 */
static void     stbi__pnm_skip_whitespace(stbi__context *s, char *c)
{
   for (;;) {
      while (!stbi__at_eof(s) && stbi__pnm_isspace(*c))
         *c = (char) stbi__get8(s);

      if (stbi__at_eof(s) || *c != '#')
         break;

      while (!stbi__at_eof(s) && *c != '\n' && *c != '\r' )
         *c = (char) stbi__get8(s);
   }
}

/**
 * @brief Checks if a given character is a digit.
 * 
 * This function determines whether the provided character is a numeric digit 
 * by checking if it falls within the range of ASCII values for '0' to '9'.
 * 
 * @param c The character to be checked.
 * @return int Returns 1 if the character is a digit (0-9), otherwise returns 0.
 */
static int      stbi__pnm_isdigit(char c)
{
   return c >= '0' && c <= '9';
}

/**
 * Reads and parses an integer from the input stream.
 *
 * This function reads characters from the input stream `s` and constructs an integer
 * by interpreting consecutive digit characters. The parsing stops when a non-digit
 * character is encountered or the end of the stream is reached. The last character
 * read (which is not a digit or EOF) is stored in `c`.
 *
 * @param s Pointer to the `stbi__context` structure representing the input stream.
 * @param c Pointer to a character buffer. On entry, it should contain the first
 *          character to be parsed. On exit, it contains the first non-digit character
 *          encountered or EOF.
 *
 * @return The parsed integer value.
 */
static int      stbi__pnm_getinteger(stbi__context *s, char *c)
{
   int value = 0;

   while (!stbi__at_eof(s) && stbi__pnm_isdigit(*c)) {
      value = value*10 + (*c - '0');
      *c = (char) stbi__get8(s);
   }

   return value;
}

/**
 * @brief Extracts information from a PNM (Portable Any Map) image file.
 *
 * This function reads the header of a PNM image file to determine its dimensions,
 * number of components, and maximum pixel value. It supports PGM (Portable Gray Map)
 * and PPM (Portable Pix Map) formats, identified by 'P5' and 'P6' respectively.
 *
 * @param s Pointer to the stbi__context structure representing the image file.
 * @param x Pointer to an integer where the width of the image will be stored.
 *          If NULL, the width is not stored.
 * @param y Pointer to an integer where the height of the image will be stored.
 *          If NULL, the height is not stored.
 * @param comp Pointer to an integer where the number of components will be stored.
 *             If NULL, the number of components is not stored.
 *
 * @return Returns 1 if the PNM header is successfully parsed and the image is 8-bit.
 *         Returns 0 if the file is not a valid PNM image or if the maximum pixel value
 *         exceeds 255 (indicating a non-8-bit image).
 */
static int      stbi__pnm_info(stbi__context *s, int *x, int *y, int *comp)
{
   int maxv, dummy;
   char c, p, t;

   if (!x) x = &dummy;
   if (!y) y = &dummy;
   if (!comp) comp = &dummy;

   stbi__rewind(s);

   // Get identifier
   p = (char) stbi__get8(s);
   t = (char) stbi__get8(s);
   if (p != 'P' || (t != '5' && t != '6')) {
       stbi__rewind(s);
       return 0;
   }

   *comp = (t == '6') ? 3 : 1;  // '5' is 1-component .pgm; '6' is 3-component .ppm

   c = (char) stbi__get8(s);
   stbi__pnm_skip_whitespace(s, &c);

   *x = stbi__pnm_getinteger(s, &c); // read width
   stbi__pnm_skip_whitespace(s, &c);

   *y = stbi__pnm_getinteger(s, &c); // read height
   stbi__pnm_skip_whitespace(s, &c);

   maxv = stbi__pnm_getinteger(s, &c);  // read max value

   if (maxv > 255)
      return stbi__err("max value > 255", "PPM image not 8-bit");
   else
      return 1;
}
#endif

/**
 * Determines the dimensions and number of components of an image file.
 *
 * This function attempts to identify the type of image file provided in the
 * `stbi__context` and retrieves its width, height, and number of components.
 * It sequentially checks for supported image formats (JPEG, PNG, GIF, BMP, PNM, TGA)
 * by calling their respective info functions. If the image type is recognized,
 * the function populates the provided pointers `x`, `y`, and `comp` with the
 * image's width, height, and number of components, respectively.
 *
 * @param s     Pointer to the `stbi__context` containing the image data.
 * @param x     Pointer to store the width of the image.
 * @param y     Pointer to store the height of the image.
 * @param comp  Pointer to store the number of components in the image.
 * @return      Returns 1 if the image type is recognized and its info is successfully
 *              retrieved. Returns 0 and sets an error message if the image type is
 *              unknown or the data is corrupt.
 */
static int stbi__info_main(stbi__context *s, int *x, int *y, int *comp)
{
   #ifndef STBI_NO_JPEG
   if (stbi__jpeg_info(s, x, y, comp)) return 1;
   #endif

   #ifndef STBI_NO_PNG
   if (stbi__png_info(s, x, y, comp))  return 1;
   #endif

   #ifndef STBI_NO_GIF
   if (stbi__gif_info(s, x, y, comp))  return 1;
   #endif

   #ifndef STBI_NO_BMP
   if (stbi__bmp_info(s, x, y, comp))  return 1;
   #endif

   #ifndef STBI_NO_PNM
   if (stbi__pnm_info(s, x, y, comp))  return 1;
   #endif

   // test tga last because it's a crappy test!
   #ifndef STBI_NO_TGA
   if (stbi__tga_info(s, x, y, comp))
       return 1;
   #endif
   return stbi__err("unknown image type", "Image not of any known type, or corrupt");
}

/**
 * @brief Checks if the image data in the provided context is in a 16-bit format.
 *
 * This function determines whether the image data being read by the `stbi__context` 
 * is in a 16-bit format. It currently supports checking for 16-bit PNG images 
 * when the `STBI_NO_PNG` macro is not defined. If the image is a 16-bit PNG, 
 * the function returns 1; otherwise, it returns 0.
 *
 * @param s Pointer to the `stbi__context` structure containing the image data.
 * @return int Returns 1 if the image is in a 16-bit format (e.g., 16-bit PNG), 
 *         otherwise returns 0.
 */
static int stbi__is_16_main(stbi__context *s)
{
   #ifndef STBI_NO_PNG
   if (stbi__png_is16(s))  return 1;
   #endif

   return 0;
}


/**
 * Retrieves image information from a memory buffer.
 *
 * This function parses the image data stored in the provided memory buffer and extracts
 * basic information such as width, height, and number of components (e.g., RGB, RGBA).
 * The function does not decode the actual image data, making it efficient for obtaining
 * metadata without full image processing.
 *
 * @param buffer Pointer to the memory buffer containing the image data.
 * @param len Length of the memory buffer in bytes.
 * @param x Pointer to an integer where the width of the image will be stored.
 * @param y Pointer to an integer where the height of the image will be stored.
 * @param comp Pointer to an integer where the number of components (channels) in the image will be stored.
 * @return Returns 1 if the image information was successfully retrieved, and 0 if the data is invalid
 *         or the image format is unsupported.
 */
STBIDEF int stbi_info_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *comp)
{
   stbi__context s;
   stbi__start_mem(&s,buffer,len);
   return stbi__info_main(&s,x,y,comp);
}

/**
 * @brief Determines if the image data in memory is in a 16-bit format.
 *
 * This function checks whether the image data provided in memory is encoded in a 16-bit format.
 * It initializes a memory-based context with the given buffer and length, then delegates the
 * actual check to the internal `stbi__is_16_main` function.
 *
 * @param buffer A pointer to the image data in memory.
 * @param len The length of the image data in bytes.
 * @return Returns 1 if the image is in a 16-bit format, 0 otherwise.
 */
STBIDEF int stbi_is_16_bit_from_memory(stbi_uc const *buffer, int len)
{
   stbi__context s;
   stbi__start_mem(&s,buffer,len);
   return stbi__is_16_main(&s);
}

#endif // STB_IMAGE_IMPLEMENTATION

/*
   revision history:
      2.20  (2019-02-07) support utf8 filenames in Windows; fix warnings and platform ifdefs
      2.19  (2018-02-11) fix warning
      2.18  (2018-01-30) fix warnings
      2.17  (2018-01-29) change sbti__shiftsigned to avoid clang -O2 bug
                         1-bit BMP
                         *_is_16_bit api
                         avoid warnings
      2.16  (2017-07-23) all functions have 16-bit variants;
                         STBI_NO_STDIO works again;
                         compilation fixes;
                         fix rounding in unpremultiply;
                         optimize vertical flip;
                         disable raw_len validation;
                         documentation fixes
      2.15  (2017-03-18) fix png-1,2,4 bug; now all Imagenet JPGs decode;
                         warning fixes; disable run-time SSE detection on gcc;
                         uniform handling of optional "return" values;
                         thread-safe initialization of zlib tables
      2.14  (2017-03-03) remove deprecated STBI_JPEG_OLD; fixes for Imagenet JPGs
      2.13  (2016-11-29) add 16-bit API, only supported for PNG right now
      2.12  (2016-04-02) fix typo in 2.11 PSD fix that caused crashes
      2.11  (2016-04-02) allocate large structures on the stack
                         remove white matting for transparent PSD
                         fix reported channel count for PNG & BMP
                         re-enable SSE2 in non-gcc 64-bit
                         support RGB-formatted JPEG
                         read 16-bit PNGs (only as 8-bit)
      2.10  (2016-01-22) avoid warning introduced in 2.09 by STBI_REALLOC_SIZED
      2.09  (2016-01-16) allow comments in PNM files
                         16-bit-per-pixel TGA (not bit-per-component)
                         info() for TGA could break due to .hdr handling
                         info() for BMP to shares code instead of sloppy parse
                         can use STBI_REALLOC_SIZED if allocator doesn't support realloc
                         code cleanup
      2.08  (2015-09-13) fix to 2.07 cleanup, reading RGB PSD as RGBA
      2.07  (2015-09-13) fix compiler warnings
                         partial animated GIF support
                         limited 16-bpc PSD support
                         #ifdef unused functions
                         bug with < 92 byte PIC,PNM,HDR,TGA
      2.06  (2015-04-19) fix bug where PSD returns wrong '*comp' value
      2.05  (2015-04-19) fix bug in progressive JPEG handling, fix warning
      2.04  (2015-04-15) try to re-enable SIMD on MinGW 64-bit
      2.03  (2015-04-12) extra corruption checking (mmozeiko)
                         stbi_set_flip_vertically_on_load (nguillemot)
                         fix NEON support; fix mingw support
      2.02  (2015-01-19) fix incorrect assert, fix warning
      2.01  (2015-01-17) fix various warnings; suppress SIMD on gcc 32-bit without -msse2
      2.00b (2014-12-25) fix STBI_MALLOC in progressive JPEG
      2.00  (2014-12-25) optimize JPG, including x86 SSE2 & NEON SIMD (ryg)
                         progressive JPEG (stb)
                         PGM/PPM support (Ken Miller)
                         STBI_MALLOC,STBI_REALLOC,STBI_FREE
                         GIF bugfix -- seemingly never worked
                         STBI_NO_*, STBI_ONLY_*
      1.48  (2014-12-14) fix incorrectly-named assert()
      1.47  (2014-12-14) 1/2/4-bit PNG support, both direct and paletted (Omar Cornut & stb)
                         optimize PNG (ryg)
                         fix bug in interlaced PNG with user-specified channel count (stb)
      1.46  (2014-08-26)
              fix broken tRNS chunk (colorkey-style transparency) in non-paletted PNG
      1.45  (2014-08-16)
              fix MSVC-ARM internal compiler error by wrapping malloc
      1.44  (2014-08-07)
              various warning fixes from Ronny Chevalier
      1.43  (2014-07-15)
              fix MSVC-only compiler problem in code changed in 1.42
      1.42  (2014-07-09)
              don't define _CRT_SECURE_NO_WARNINGS (affects user code)
              fixes to stbi__cleanup_jpeg path
              added STBI_ASSERT to avoid requiring assert.h
      1.41  (2014-06-25)
              fix search&replace from 1.36 that messed up comments/error messages
      1.40  (2014-06-22)
              fix gcc struct-initialization warning
      1.39  (2014-06-15)
              fix to TGA optimization when req_comp != number of components in TGA;
              fix to GIF loading because BMP wasn't rewinding (whoops, no GIFs in my test suite)
              add support for BMP version 5 (more ignored fields)
      1.38  (2014-06-06)
              suppress MSVC warnings on integer casts truncating values
              fix accidental rename of 'skip' field of I/O
      1.37  (2014-06-04)
              remove duplicate typedef
      1.36  (2014-06-03)
              convert to header file single-file library
              if de-iphone isn't set, load iphone images color-swapped instead of returning NULL
      1.35  (2014-05-27)
              various warnings
              fix broken STBI_SIMD path
              fix bug where stbi_load_from_file no longer left file pointer in correct place
              fix broken non-easy path for 32-bit BMP (possibly never used)
              TGA optimization by Arseny Kapoulkine
      1.34  (unknown)
              use STBI_NOTUSED in stbi__resample_row_generic(), fix one more leak in tga failure case
      1.33  (2011-07-14)
              make stbi_is_hdr work in STBI_NO_HDR (as specified), minor compiler-friendly improvements
      1.32  (2011-07-13)
              support for "info" function for all supported filetypes (SpartanJ)
      1.31  (2011-06-20)
              a few more leak fixes, bug in PNG handling (SpartanJ)
      1.30  (2011-06-11)
              added ability to load files via callbacks to accomidate custom input streams (Ben Wenger)
              removed deprecated format-specific test/load functions
              removed support for installable file formats (stbi_loader) -- would have been broken for IO callbacks anyway
              error cases in bmp and tga give messages and don't leak (Raymond Barbiero, grisha)
              fix inefficiency in decoding 32-bit BMP (David Woo)
      1.29  (2010-08-16)
              various warning fixes from Aurelien Pocheville
      1.28  (2010-08-01)
              fix bug in GIF palette transparency (SpartanJ)
      1.27  (2010-08-01)
              cast-to-stbi_uc to fix warnings
      1.26  (2010-07-24)
              fix bug in file buffering for PNG reported by SpartanJ
      1.25  (2010-07-17)
              refix trans_data warning (Won Chun)
      1.24  (2010-07-12)
              perf improvements reading from files on platforms with lock-heavy fgetc()
              minor perf improvements for jpeg
              deprecated type-specific functions so we'll get feedback if they're needed
              attempt to fix trans_data warning (Won Chun)
      1.23    fixed bug in iPhone support
      1.22  (2010-07-10)
              removed image *writing* support
              stbi_info support from Jetro Lauha
              GIF support from Jean-Marc Lienher
              iPhone PNG-extensions from James Brown
              warning-fixes from Nicolas Schulz and Janez Zemva (i.stbi__err. Janez (U+017D)emva)
      1.21    fix use of 'stbi_uc' in header (reported by jon blow)
      1.20    added support for Softimage PIC, by Tom Seddon
      1.19    bug in interlaced PNG corruption check (found by ryg)
      1.18  (2008-08-02)
              fix a threading bug (local mutable static)
      1.17    support interlaced PNG
      1.16    major bugfix - stbi__convert_format converted one too many pixels
      1.15    initialize some fields for thread safety
      1.14    fix threadsafe conversion bug
              header-file-only version (#define STBI_HEADER_FILE_ONLY before including)
      1.13    threadsafe
      1.12    const qualifiers in the API
      1.11    Support installable IDCT, colorspace conversion routines
      1.10    Fixes for 64-bit (don't use "unsigned long")
              optimized upsampling by Fabian "ryg" Giesen
      1.09    Fix format-conversion for PSD code (bad global variables!)
      1.08    Thatcher Ulrich's PSD code integrated by Nicolas Schulz
      1.07    attempt to fix C++ warning/errors again
      1.06    attempt to fix C++ warning/errors again
      1.05    fix TGA loading to return correct *comp and use good luminance calc
      1.04    default float alpha is 1, not 255; use 'void *' for stbi_image_free
      1.03    bugfixes to STBI_NO_STDIO, STBI_NO_HDR
      1.02    support for (subset of) HDR files, float interface for preferred access to them
      1.01    fix bug: possible bug in handling right-side up bmps... not sure
              fix bug: the stbi__bmp_load() and stbi__tga_load() functions didn't work at all
      1.00    interface to zlib that skips zlib header
      0.99    correct handling of alpha in palette
      0.98    TGA loader by lonesock; dynamically add loaders (untested)
      0.97    jpeg errors on too large a file; also catch another malloc failure
      0.96    fix detection of invalid v value - particleman@mollyrocket forum
      0.95    during header scan, seek to markers in case of padding
      0.94    STBI_NO_STDIO to disable stdio usage; rename all #defines the same
      0.93    handle jpegtran output; verbose errors
      0.92    read 4,8,16,24,32-bit BMP files of several formats
      0.91    output 24-bit Windows 3.0 BMP files
      0.90    fix a few more warnings; bump version number to approach 1.0
      0.61    bugfixes due to Marc LeBlanc, Christopher Lloyd
      0.60    fix compiling as c++
      0.59    fix warnings: merge Dave Moore's -Wall fixes
      0.58    fix bug: zlib uncompressed mode len/nlen was wrong endian
      0.57    fix bug: jpg last huffman symbol before marker was >9 bits but less than 16 available
      0.56    fix bug: zlib uncompressed mode len vs. nlen
      0.55    fix bug: restart_interval not initialized to 0
      0.54    allow NULL for 'int *comp'
      0.53    fix bug in png 3->4; speedup png decoding
      0.52    png handles req_comp=3,4 directly; minor cleanup; jpeg comments
      0.51    obey req_comp requests, 1-component jpegs return as 1-component,
              on 'test' only check type, not whether we support this variant
      0.50  (2006-11-19)
              first released version
*/


/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
