#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "htif.h"

extern char _heap_start;
int main(const char *args);

extern char _pmem_start;
#define PMEM_SIZE (128 * 1024 * 1024)
#define PMEM_END ((uintptr_t)0x80000000 + PMEM_SIZE)

Area heap = RANGE(&_heap_start, PMEM_END);
#ifndef MAINARGS
#define MAINARGS ""
#endif
static const char mainargs[] = MAINARGS;

/**
 * Writes a single character to the console.
 * 
 * This function sends the specified character `ch` to the console output
 * by invoking the `htif_console_putchar` function. It is typically used
 * for low-level character output operations.
 *
 * @param ch The character to be written to the console.
 */
void putch(char ch) {
  htif_console_putchar(ch);
}

/**
 * Halts the program execution and powers off the system.
 * 
 * This function prints the provided exit code to the standard output, 
 * indicating the reason for termination. It then invokes the `htif_poweroff` 
 * function to power off the system. If the system does not power off 
 * successfully, the function enters an infinite loop to prevent further 
 * execution.
 * 
 * @param code The exit code to be printed, typically indicating the reason 
 *             for the program termination.
 */
void halt(int code) {
  printf("Exit with code = %d\n", code);
  htif_poweroff();

  // should not reach here
  while (1);
}

/**
 * Initializes the terminal runtime environment by invoking the main function
 * with the provided main arguments. After the main function completes, the
 * runtime halts with the return value of the main function.
 *
 * This function is responsible for setting up the environment required for
 * the main function to execute. It passes the `mainargs` to the main function
 * and ensures that the program terminates gracefully by calling `halt` with
 * the return value from main.
 *
 * @note The `mainargs` parameter is expected to be defined and initialized
 *       elsewhere in the program.
 * @note The `halt` function is used to terminate the program and should be
 *       implemented to handle the program's exit logic.
 */
void _trm_init() {
  int ret = main(mainargs);
  halt(ret);
}
