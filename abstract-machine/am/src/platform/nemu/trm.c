#include <am.h>
#include <nemu.h>

extern char _heap_start;
int main(const char *args);

Area heap = RANGE(&_heap_start, PMEM_END);
static const char mainargs[MAINARGS_MAX_LEN] = MAINARGS_PLACEHOLDER; // defined in CFLAGS

void putch(char ch) {
  outb(SERIAL_PORT, ch);
}

/**
 * Halts the program execution by triggering a trap with the specified exit code.
 * This function is typically used to terminate the program in a controlled manner
 * when an unrecoverable error or condition is encountered. After invoking the trap,
 * the function enters an infinite loop to ensure that the program does not proceed
 * further.
 *
 * @param code The exit code to be passed to the trap mechanism, indicating the reason
 *             for halting the program.
 *
 * @note This function does not return. Once called, the program will be halted
 *       indefinitely.
 */
void halt(int code) {
  nemu_trap(code);

  // should not reach here
  while (1);
}

/**
 * Initializes the terminal runtime environment by invoking the main function
 * with the provided arguments and then halting the execution with the return
 * value from the main function.
 *
 * This function serves as the entry point for the terminal runtime, setting up
 * the environment and executing the main logic of the program. After the main
 * function completes, it halts the execution, ensuring the program terminates
 * cleanly with the appropriate exit code.
 *
 * @param mainargs The arguments to be passed to the main function.
 * @return void
 */
void _trm_init() {
  int ret = main(mainargs);
  halt(ret);
}
