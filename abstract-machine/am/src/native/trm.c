#include <am.h>
#include <stdio.h>
#include <klib-macros.h>

void __am_platform_dummy();
void __am_exit_platform(int code);

/**
 * Initializes the terminal emulator by calling the platform-specific dummy function.
 * This function is typically used to set up the necessary environment or resources
 * required for terminal operations. It ensures that the terminal is in a valid state
 * before any further operations are performed.
 */
void trm_init() {
  __am_platform_dummy();
}

/**
 * Outputs a single character to the standard output.
 *
 * This function takes a single character as input and writes it to the standard output
 * using the `putchar` function. It is a wrapper around `putchar` and provides a simple
 * interface for printing individual characters.
 *
 * @param ch The character to be printed.
 */
void putch(char ch) {
  putchar(ch);
}

/**
 * Halts the program execution and prints an exit code in a formatted string.
 * 
 * This method prints a formatted string to the console, where the characters '0' and '4' 
 * in the string are replaced with hexadecimal digits derived from the provided `code` 
 * parameter. Specifically, '0' is replaced with the least significant nibble (4 bits) 
 * of the `code`, and '4' is replaced with the next nibble. After printing the formatted 
 * string, the method calls `__am_exit_platform` with the `code` to terminate the program 
 * on the platform. If the program continues execution unexpectedly, it prints a message 
 * indicating that it should not reach that point and enters an infinite loop.
 *
 * @param code The exit code to be printed and passed to `__am_exit_platform`.
 */
void halt(int code) {
  const char *fmt = "Exit code = 40h\n";
  for (const char *p = fmt; *p; p++) {
    char ch = *p;
    if (ch == '0' || ch == '4') {
      ch = "0123456789abcdef"[(code >> (ch - '0')) & 0xf];
    }
    putch(ch);
  }
  __am_exit_platform(code);
  putstr("Should not reach here!\n");
  while (1);
}

Area heap = {};
