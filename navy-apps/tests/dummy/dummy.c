#include <stdint.h>

#ifdef __ISA_NATIVE__
#error can not support ISA=native
#endif

#define SYS_yield 1
extern int _syscall_(int, uintptr_t, uintptr_t, uintptr_t);

/**
 * The main function serves as the entry point of the program. It makes a system call
 * to yield the CPU, allowing other processes to run. This is achieved by invoking
 * the `_syscall_` function with the `SYS_yield` system call number and three
 * arguments, all set to 0. The function returns the result of the system call,
 * which typically indicates the success or failure of the operation.
 *
 * @return int The return value of the `_syscall_` function, which is the result
 *             of the `SYS_yield` system call.
 */
int main() {
  return _syscall_(SYS_yield, 0, 0, 0);
}
