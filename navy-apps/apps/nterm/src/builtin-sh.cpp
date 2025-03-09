#include <nterm.h>
#include <stdarg.h>
#include <unistd.h>
#include <SDL.h>

char handle_key(SDL_Event *ev);

/**
 * @brief Formats and prints a string to the terminal using a static buffer.
 *
 * This method takes a format string and a variable number of arguments, similar to the standard `printf` function.
 * It formats the output string using `vsnprintf` and stores the result in a static internal buffer of size 256.
 * The formatted string is then written to the terminal using the `term->write` method.
 *
 * @param format A format string that specifies how the subsequent arguments are converted for output.
 *               This follows the same syntax as the standard `printf` function.
 * @param ...    A variable number of arguments to be formatted according to the format string.
 *
 * @note The internal buffer is shared across all calls to this method, so it is not thread-safe.
 *       Additionally, if the formatted string exceeds 255 characters, it will be truncated.
 */
static void sh_printf(const char *format, ...) {
  static char buf[256] = {};
  va_list ap;
  va_start(ap, format);
  int len = vsnprintf(buf, 256, format, ap);
  va_end(ap);
  term->write(buf, len);
}

/**
 * @brief Displays the banner for the built-in shell in NTerm (NJU Terminal).
 * 
 * This method prints a welcome message to the terminal, indicating that the user 
 * is interacting with the built-in shell of NTerm. The message is displayed using 
 * the `sh_printf` function, which handles the actual output to the terminal.
 */
static void sh_banner() {
  sh_printf("Built-in Shell in NTerm (NJU Terminal)\n\n");
}

/**
 * @brief Displays the shell prompt to the user.
 * 
 * This method outputs the shell prompt "sh> " to the standard output,
 * indicating that the shell is ready to accept user input. It uses the
 * `sh_printf` function to print the prompt.
 */
static void sh_prompt() {
  sh_printf("sh> ");
}

/**
 * @brief Handles the execution of a shell command.
 *
 * This method is responsible for processing and executing a given shell command.
 * It takes a null-terminated string representing the command and performs the
 * necessary actions to execute it. The method is static, meaning it can be called
 * without an instance of the containing class.
 *
 * @param cmd A null-terminated string representing the shell command to be executed.
 *            The command should be in a format that can be directly interpreted
 *            by the shell.
 *
 * @note The method does not return any value. It is expected to handle all
 *       execution logic internally, including any error handling or output
 *       generation.
 */
static void sh_handle_cmd(const char *cmd) {
}

/**
 * @brief Executes the built-in shell run loop.
 *
 * This method initializes the shell by displaying a banner and a prompt. It then enters
 * an infinite loop where it continuously polls for SDL events, particularly key press
 * and release events. When a key event is detected, it processes the key press using
 * the `handle_key` function and passes the result to the terminal's `keypress` method.
 * If the `keypress` method returns a non-null command string, the command is handled
 * by `sh_handle_cmd`, and a new prompt is displayed. The terminal is refreshed after
 * each event processing to ensure the display is up-to-date.
 *
 * The loop continues indefinitely, making this method the core of the interactive
 * shell's event handling and command execution process.
 */
void builtin_sh_run() {
  sh_banner();
  sh_prompt();

  while (1) {
    SDL_Event ev;
    if (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_KEYUP || ev.type == SDL_KEYDOWN) {
        const char *res = term->keypress(handle_key(&ev));
        if (res) {
          sh_handle_cmd(res);
          sh_prompt();
        }
      }
    }
    refresh_terminal();
  }
}
