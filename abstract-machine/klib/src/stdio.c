#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

/**
 * Prints formatted output to the standard output stream.
 *
 * This function formats and prints a sequence of characters and values to the
 * standard output stream (stdout) based on the provided format string. The format
 * string specifies how subsequent arguments are converted for output. It supports
 * a variety of format specifiers (e.g., %d, %s, %f) to control the formatting of
 * the output.
 *
 * @param fmt A format string that contains text and format specifiers. The format
 *            specifiers are replaced by the corresponding arguments provided in
 *            the variadic argument list.
 * @param ... A variadic argument list containing the values to be formatted and
 *            printed according to the format string.
 *
 * @return On success, the function returns the number of characters printed. If
 *         an error occurs, a negative value is returned.
 *
 * @note This implementation currently panics with the message "Not implemented"
 *       as it is a placeholder and not fully implemented.
 */
int printf(const char *fmt, ...) {
  panic("Not implemented");
}

/**
 * Formats and writes a variable argument list to a character buffer.
 *
 * This function formats a string using a format specifier `fmt` and a variable
 * argument list `ap`, then writes the resulting string to the buffer `out`.
 * The behavior is similar to `sprintf`, but it accepts a `va_list` instead of
 * a variable number of arguments.
 *
 * @param out Pointer to the buffer where the formatted string will be stored.
 * @param fmt Pointer to the format string that specifies how the data is formatted.
 * @param ap  Variable argument list containing the data to be formatted.
 *
 * @return The number of characters written to the buffer, excluding the null
 *         terminator. If an error occurs, a negative value may be returned.
 *
 * @note This function is not implemented and will trigger a panic if called.
 */
int vsprintf(char *out, const char *fmt, va_list ap) {
  panic("Not implemented");
}

/**
 * Writes formatted data to a string.
 *
 * This function formats and stores a series of characters and values in the buffer
 * pointed to by `out`. The format string `fmt` specifies how subsequent arguments
 * are converted for output. The function behaves similarly to `printf`, but instead
 * of printing the output to the standard output, it stores the result in the buffer
 * `out`.
 *
 * @param out Pointer to the buffer where the resulting string is stored. The buffer
 *            must be large enough to hold the formatted string, including the null
 *            terminator.
 * @param fmt Format string that specifies how the data should be formatted. It
 *            can contain format specifiers starting with `%` (e.g., `%d`, `%s`).
 * @param ... Additional arguments corresponding to the format specifiers in `fmt`.
 *
 * @return On success, the function returns the number of characters written to the
 *         buffer (excluding the null terminator). On failure, a negative value is
 *         returned.
 *
 * @note This implementation currently panics with "Not implemented" as it is a
 *       placeholder and not yet implemented.
 */
int sprintf(char *out, const char *fmt, ...) {
  panic("Not implemented");
}

/**
 * Writes formatted output to a character buffer, ensuring that the output does not exceed the specified buffer size.
 *
 * This function formats a string according to the provided format string `fmt` and writes the result to the buffer `out`.
 * The function ensures that no more than `n` characters are written to the buffer, including the null-terminator.
 * If the formatted output exceeds the buffer size, the output is truncated to fit within the buffer, and the buffer is
 * null-terminated. The function returns the number of characters that would have been written if the buffer had been
 * sufficiently large, excluding the null-terminator. If an encoding error occurs, a negative value is returned.
 *
 * @param out Pointer to the buffer where the formatted string will be stored.
 * @param n Maximum number of characters to write to the buffer, including the null-terminator.
 * @param fmt Format string that specifies how the subsequent arguments are converted for output.
 * @param ... Additional arguments to be formatted according to the format string.
 * @return The number of characters that would have been written if the buffer had been large enough, excluding the
 *         null-terminator. If an encoding error occurs, a negative value is returned.
 */
int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

/**
 * Formats and writes a string to a buffer with a specified maximum length, using a variable argument list.
 * This function is similar to snprintf, but it takes a va_list instead of a variable number of arguments.
 * It formats the string according to the format specifier `fmt` and stores the result in the buffer `out`,
 * ensuring that no more than `n` characters are written, including the null terminator.
 *
 * @param out Pointer to the buffer where the formatted string will be stored.
 * @param n Maximum number of characters to be written to the buffer, including the null terminator.
 * @param fmt Format string that specifies how the subsequent arguments are converted for output.
 * @param ap Variable argument list containing the arguments to be formatted.
 * @return On success, returns the number of characters that would have been written if `n` were sufficiently large,
 *         not counting the null terminator. If an encoding error occurs, a negative value is returned.
 */
int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
