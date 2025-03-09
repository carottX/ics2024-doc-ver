#include <am.h>

Area heap = RANGE(NULL, NULL);

/**
 * Writes a single character to the standard output.
 * This function outputs the specified character `ch` to the console or
 * terminal. It is typically used for low-level character output operations.
 *
 * @param ch The character to be written to the output.
 */
void putch(char ch) {
}

/**
 * @brief Halts the program execution indefinitely.
 * 
 * This method enters an infinite loop, effectively halting the program's execution
 * and preventing it from progressing further. The method does not return and will
 * continue to loop indefinitely until the program is terminated externally.
 * 
 * @param code An integer parameter that is unused in the current implementation.
 *             It may be intended for future use to specify a halt condition or
 *             error code.
 */
void halt(int code) {
  while (1);
}
