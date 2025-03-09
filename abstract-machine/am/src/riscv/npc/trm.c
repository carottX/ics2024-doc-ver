#include <am.h>
#include <klib-macros.h>

extern char _heap_start;
int main(const char *args);

extern char _pmem_start;
#define PMEM_SIZE (128 * 1024 * 1024)
#define PMEM_END  ((uintptr_t)&_pmem_start + PMEM_SIZE)

Area heap = RANGE(&_heap_start, PMEM_END);
static const char mainargs[MAINARGS_MAX_LEN] = MAINARGS_PLACEHOLDER; // defined in CFLAGS

void putch(char ch) {
}

/**
 * @brief Enters an infinite loop, effectively halting the program.
 *
 * This method causes the program to enter an infinite loop, which prevents
 * further execution of code. It is typically used to halt the program
 * intentionally, often in scenarios where an unrecoverable error has occurred.
 * The `code` parameter is currently unused but could be utilized in future
 * implementations to indicate the reason for halting.
 *
 * @param code An integer that could represent the reason for halting (unused).
 */
void halt(int code) {
  while (1);
}

/**
 * Initializes the terminal by invoking the main function with the provided main arguments
 * and halts the program with the return value from the main function.
 * 
 * This method serves as the entry point for the terminal's execution. It calls the `main`
 * function, passing the `mainargs` as its arguments, and captures the return value. 
 * The program is then halted using the `halt` function, with the return value from `main`
 * as the halt status.
 * 
 * @note The `mainargs` variable is expected to be defined and initialized elsewhere in 
 * the program, containing the arguments to be passed to the `main` function.
 * 
 * @see main()
 * @see halt()
 */
void _trm_init() {
  int ret = main(mainargs);
  halt(ret);
}
