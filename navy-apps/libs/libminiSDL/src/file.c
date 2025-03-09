#include <sdl-file.h>

/**
 * Creates an SDL_RWops structure for reading from or writing to a file.
 *
 * This function opens the file specified by `filename` with the given `mode` and
 * returns an SDL_RWops structure that can be used to perform read/write operations
 * on the file. The `mode` parameter follows the same format as the standard C library's
 * fopen() function (e.g., "r" for reading, "w" for writing, "a" for appending, etc.).
 *
 * If the file cannot be opened, this function returns NULL. The caller is responsible
 * for closing the SDL_RWops structure using SDL_RWclose() when it is no longer needed.
 *
 * @param filename The path to the file to open.
 * @param mode The access mode for the file (e.g., "r", "w", "a", "rb", "wb", etc.).
 * @return A pointer to an SDL_RWops structure on success, or NULL on failure.
 */
SDL_RWops* SDL_RWFromFile(const char *filename, const char *mode) {
  return NULL;
}

/**
 * Creates a new SDL_RWops structure that reads from and writes to a memory buffer.
 *
 * This function initializes an SDL_RWops structure that allows reading from and writing to
 * a specified memory buffer. The memory buffer is provided by the caller and must remain
 * valid for the lifetime of the SDL_RWops structure.
 *
 * @param mem  A pointer to the memory buffer to use for reading and writing.
 * @param size The size of the memory buffer in bytes.
 *
 * @return A pointer to the newly created SDL_RWops structure on success, or NULL on failure.
 *         The caller is responsible for freeing the SDL_RWops structure using SDL_RWclose()
 *         when it is no longer needed.
 */
SDL_RWops* SDL_RWFromMem(void *mem, int size) {
  return NULL;
}
