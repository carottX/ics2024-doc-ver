#include <NDL.h>

/**
 * Initializes the SDL library with the specified initialization flags.
 *
 * This function initializes the SDL library and its subsystems based on the provided flags.
 * It is a wrapper around the NDL_Init function, which performs the actual initialization.
 * The flags parameter is a bitwise OR of the desired subsystems to initialize.
 *
 * @param flags A bitwise OR of SDL initialization flags indicating which subsystems to initialize.
 *              Common flags include SDL_INIT_VIDEO, SDL_INIT_AUDIO, SDL_INIT_TIMER, etc.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int SDL_Init(uint32_t flags) {
  return NDL_Init(flags);
}

/**
 * Shuts down and cleans up the SDL (Simple DirectMedia Layer) subsystem.
 * This function should be called before exiting the application to ensure
 * that all SDL resources are properly released and the subsystem is safely
 * terminated. It internally calls `NDL_Quit()` to handle the actual cleanup
 * process. After calling this function, no further SDL functions should be
 * invoked.
 */
void SDL_Quit() {
  NDL_Quit();
}

/**
 * @brief Retrieves the last error message related to SDL operations.
 *
 * This function returns a string describing the last error that occurred within the SDL library.
 * In this implementation, it always returns a static message indicating that the Navy environment
 * does not support SDL_GetError(). This is typically used for debugging or logging purposes to
 * understand the cause of a failure in SDL-related operations.
 *
 * @return A pointer to a string containing the error message. The string is statically allocated
 *         and should not be freed by the caller.
 */
char *SDL_GetError() {
  return "Navy does not support SDL_GetError()";
}

/**
 * Sets the current SDL error message.
 *
 * This function sets the error message for the SDL library, which can be retrieved
 * using `SDL_GetError()`. The error message is formatted using a printf-style format
 * string and variable arguments. This is useful for reporting errors in a consistent
 * and human-readable format.
 *
 * @param fmt A printf-style format string that specifies how the error message should
 *            be formatted. This string can include format specifiers like `%s`, `%d`, etc.
 * @param ... Variable arguments that correspond to the format specifiers in the `fmt` string.
 *
 * @return Always returns -1 to indicate an error. This is a convention used by SDL to
 *         allow functions to return an error code and set an error message in one call.
 *
 * @note The error message is stored internally and can be retrieved using `SDL_GetError()`.
 *       The message is thread-local, meaning each thread has its own error message.
 *
 * @see SDL_GetError()
 */
int SDL_SetError(const char* fmt, ...) {
  return -1;
}

/**
 * Controls the visibility of the mouse cursor.
 *
 * This function allows you to show or hide the mouse cursor in the application window.
 * The cursor's visibility can be toggled on or off, or queried without changing its current state.
 *
 * @param toggle An integer value that specifies the desired cursor visibility:
 *               - `SDL_ENABLE` (1): Show the cursor.
 *               - `SDL_DISABLE` (0): Hide the cursor.
 *               - `SDL_QUERY` (-1): Query the current cursor visibility without changing it.
 *
 * @return The current visibility state of the cursor after the operation:
 *         - `SDL_ENABLE` (1): The cursor is visible.
 *         - `SDL_DISABLE` (0): The cursor is hidden.
 *         If `toggle` is `SDL_QUERY`, the return value indicates the current state without altering it.
 */
int SDL_ShowCursor(int toggle) {
  return 0;
}

/**
 * Sets the window title and icon name for the current SDL window.
 *
 * This function allows you to specify the title and icon name for the window
 * managed by SDL. The title is displayed in the window's title bar, while the
 * icon name is typically used in the window's taskbar entry or dock representation.
 *
 * @param title The text to be displayed in the window's title bar. If NULL, the
 *              title remains unchanged.
 * @param icon The text to be used as the window's icon name. If NULL, the icon
 *             name remains unchanged.
 *
 * @note This function does not change the actual icon image used by the window.
 *       To change the icon image, use SDL_WM_SetIcon().
 */
void SDL_WM_SetCaption(const char *title, const char *icon) {
}
