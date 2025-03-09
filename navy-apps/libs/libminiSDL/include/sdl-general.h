#ifndef __SDL_GENERAL_H__
#define __SDL_GENERAL_H__

#include <stdint.h>
#include <stdio.h>

#define SDL_INIT_VIDEO       0x01
#define SDL_INIT_TIMER       0x02
#define SDL_INIT_AUDIO       0x04
#define SDL_INIT_NOPARACHUTE 0x08
#define SDL_INIT_JOYSTICK    0x10

int SDL_Init(uint32_t flags);
void SDL_Quit();
char *SDL_GetError();
int SDL_SetError(const char* fmt, ...);
int SDL_ShowCursor(int toggle);
void SDL_WM_SetCaption(const char *title, const char *icon);

typedef struct SDL_mutex {} SDL_mutex;
/**
 * @brief Creates a new mutex object.
 *
 * This function allocates and initializes a new mutex object, which can be used
 * for thread synchronization. A mutex (mutual exclusion) allows only one thread
 * to access a shared resource at a time, preventing race conditions.
 *
 * @return A pointer to the newly created SDL_mutex object on success. If the
 *         mutex could not be created, NULL is returned and the error can be
 *         retrieved using SDL_GetError().
 */
static inline SDL_mutex* SDL_CreateMutex() { return NULL; }
/**
 * @brief Destroys a mutex previously created by SDL_CreateMutex.
 *
 * This function safely deallocates the resources associated with the mutex.
 * It is important to ensure that the mutex is not locked by any thread
 * before calling this function, as doing so may result in undefined behavior.
 * After the mutex is destroyed, the provided `mutex` pointer should no longer
 * be used.
 *
 * @param mutex A pointer to the SDL_mutex structure to be destroyed.
 *              If NULL, the function does nothing.
 */
static inline void SDL_DestroyMutex(SDL_mutex* mutex) { }
/**
 * Locks a mutex.
 *
 * This function attempts to lock the specified mutex. If the mutex is already
 * locked by another thread, the calling thread will block until the mutex
 * becomes available. If the mutex is successfully locked, the function returns
 * 0. If an error occurs, a non-zero value is returned.
 *
 * @param mutex A pointer to the mutex to lock.
 * @return 0 on success or a non-zero error code on failure.
 */
static inline int SDL_mutexP(SDL_mutex* mutex) { return 0; }
/**
 * @brief Releases a mutex.
 *
 * This function releases a mutex that was previously locked by SDL_mutexP().
 * It allows other threads to acquire the mutex and proceed with their execution.
 * The mutex must be held by the calling thread; otherwise, the behavior is undefined.
 *
 * @param mutex A pointer to the mutex to be released.
 * @return Always returns 0, indicating success. In case of an error, the return value
 *         is not defined, and the behavior depends on the underlying implementation.
 */
static inline int SDL_mutexV(SDL_mutex* mutex) { return 0; }

#endif
