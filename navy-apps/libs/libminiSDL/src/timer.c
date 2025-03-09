#include <NDL.h>
#include <sdl-timer.h>
#include <stdio.h>

/**
 * @brief Adds a new timer to the SDL timer subsystem.
 *
 * This function creates a new timer that will call the specified callback function
 * at the given interval. The timer will continue to trigger until it is removed
 * using SDL_RemoveTimer().
 *
 * @param interval The time delay (in milliseconds) between each call to the callback.
 * @param callback The function to call when the timer elapses. This function should
 *                 have the signature `uint32_t callback(uint32_t interval, void *param)`.
 * @param param A pointer to user-defined data that will be passed to the callback function.
 *
 * @return Returns a timer ID that can be used to remove the timer with SDL_RemoveTimer().
 *         If the timer could not be created, this function returns 0.
 */
SDL_TimerID SDL_AddTimer(uint32_t interval, SDL_NewTimerCallback callback, void *param) {
  return NULL;
}

/**
 * Removes a timer previously created with SDL_AddTimer().
 *
 * This function stops the timer associated with the given `id` from running
 * and frees any resources allocated for it. If the timer has already been
 * removed or if the `id` is invalid, this function has no effect.
 *
 * @param id The ID of the timer to remove. This ID is returned by SDL_AddTimer().
 * @return Returns 1 if the timer was successfully removed, or 0 if the timer
 *         was not found or could not be removed. Note that this implementation
 *         always returns 1, indicating success.
 */
int SDL_RemoveTimer(SDL_TimerID id) {
  return 1;
}

/**
 * @brief Retrieves the number of milliseconds since the SDL library initialization.
 *
 * This function returns the number of milliseconds that have elapsed since the SDL library
 * was initialized. It is commonly used for timing purposes, such as measuring the duration
 * of events or controlling frame rates in applications. The value returned is a 32-bit
 * unsigned integer, which wraps around after approximately 49.7 days.
 *
 * @return A 32-bit unsigned integer representing the number of milliseconds since SDL
 *         initialization. If the function is called before SDL is initialized, the return
 *         value is undefined.
 */
uint32_t SDL_GetTicks() {
  return 0;
}

/**
 * Delays the execution of the program for a specified number of milliseconds.
 * This function is typically used to introduce a pause in the program, allowing
 * for timing control or to reduce CPU usage during idle periods. The actual
 * delay may be longer than the specified time due to system scheduling and
 * other factors.
 *
 * @param ms The number of milliseconds to delay the execution. Must be a
 *           non-negative value.
 */
void SDL_Delay(uint32_t ms) {
}
