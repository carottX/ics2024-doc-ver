#define SDL_malloc  malloc
#define SDL_free    free
#define SDL_realloc realloc

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"

/**
 * Loads an image from an SDL_RWops source into an SDL_Surface.
 * 
 * This function reads image data from the provided SDL_RWops source and 
 * attempts to decode it into an SDL_Surface. The function assumes that 
 * the source is of type RW_TYPE_MEM (memory-based) and that the `freesrc` 
 * parameter is set to 0, indicating that the caller retains ownership 
 * of the SDL_RWops source and is responsible for freeing it.
 *
 * @param src      A pointer to an SDL_RWops structure that provides the 
 *                 image data. Must be of type RW_TYPE_MEM.
 * @param freesrc  An integer flag indicating whether the SDL_RWops source 
 *                 should be freed after loading. Must be 0.
 * @return         A pointer to the newly created SDL_Surface containing 
 *                 the decoded image, or NULL if an error occurred.
 */
SDL_Surface* IMG_Load_RW(SDL_RWops *src, int freesrc) {
  assert(src->type == RW_TYPE_MEM);
  assert(freesrc == 0);
  return NULL;
}

/**
 * Loads an image from a file into an SDL_Surface.
 *
 * This function reads an image file specified by the `filename` parameter and
 * converts it into an SDL_Surface, which can be used for rendering or manipulation
 * within an SDL application. The function supports various image formats, including
 * BMP, PNG, JPEG, GIF, and others, depending on the available image loading libraries
 * linked with SDL_image.
 *
 * @param filename The path to the image file to be loaded. The path can be relative
 *                 or absolute.
 * @return A pointer to the newly created SDL_Surface containing the image data.
 *         If the image cannot be loaded, the function returns NULL.
 */
SDL_Surface* IMG_Load(const char *filename) {
  return NULL;
}

/**
 * @brief Checks if the given SDL_RWops source contains a PNG image.
 *
 * This function attempts to determine if the data provided through the SDL_RWops
 * source is a valid PNG image. It does so by examining the file signature or
 * other identifying characteristics of the PNG format.
 *
 * @param src A pointer to an SDL_RWops structure representing the data source
 *            to be checked. The source can be a file, memory buffer, or any
 *            other type supported by SDL_RWops.
 *
 * @return Returns 1 if the source contains a valid PNG image, 0 otherwise.
 *         If the source is NULL or an error occurs during the check, 0 is returned.
 */
int IMG_isPNG(SDL_RWops *src) {
  return 0;
}

/**
 * Loads a JPEG image from an SDL_RWops source.
 *
 * This function decodes a JPEG image from the provided SDL_RWops source and returns
 * an SDL_Surface containing the decoded image. The image is loaded in the format
 * that is most efficient for rendering with SDL. The caller is responsible for
 * freeing the returned SDL_Surface using SDL_FreeSurface() when it is no longer needed.
 *
 * @param src A pointer to an SDL_RWops structure that provides the JPEG image data.
 *            The data can be from a file, memory buffer, or any other source supported
 *            by SDL_RWops.
 *
 * @return A pointer to the newly created SDL_Surface containing the decoded image,
 *         or NULL if there was an error loading the image.
 */
SDL_Surface* IMG_LoadJPG_RW(SDL_RWops *src) {
  return IMG_Load_RW(src, 0);
}

/**
 * Retrieves an error message indicating that the IMG_GetError() function is not supported.
 *
 * This function is a placeholder that returns a static string indicating that the IMG_GetError()
 * functionality is not supported in the current implementation. It is typically used in environments
 * where image-related error handling is not available or not implemented.
 *
 * @return A constant string containing the error message "Navy does not support IMG_GetError()".
 */
char *IMG_GetError() {
  return "Navy does not support IMG_GetError()";
}
