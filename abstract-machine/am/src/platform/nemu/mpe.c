#include <am.h>
#include <stdatomic.h>
#include <klib-macros.h>

/**
 * Initializes the MPE (Multi-Processing Environment) by invoking the provided entry function.
 * This function is intended to be the starting point for MPE execution. After calling the entry
 * function, it triggers a panic with the message "MPE entry returns" to indicate that the entry
 * function has completed execution, which is unexpected behavior in this context.
 *
 * @param entry A function pointer to the entry point of the MPE. This function is expected to
 *              handle the initialization and execution of the MPE.
 * @return This function does not return a meaningful value as it triggers a panic after invoking
 *         the entry function.
 */
bool mpe_init(void (*entry)()) {
  entry();
  panic("MPE entry returns");
}

/**
 * Returns the number of CPU cores available on the system.
 * 
 * This method currently returns a hardcoded value of 1, indicating that
 * the system is assumed to have a single CPU core. Future implementations
 * may dynamically determine the actual number of CPU cores available.
 * 
 * @return The number of CPU cores, currently always 1.
 */
int cpu_count() {
  return 1;
}

/**
 * @brief Returns the current CPU identifier.
 *
 * This method is a placeholder implementation that always returns 0,
 * indicating the current CPU. In a real-world scenario, this method
 * would retrieve and return the identifier of the CPU on which the
 * calling thread is currently executing.
 *
 * @return int The current CPU identifier. Always returns 0 in this implementation.
 */
int cpu_current() {
  return 0;
}

/**
 * Atomically exchanges the value at the specified memory address with a new value.
 * This function ensures that the exchange operation is performed atomically, meaning
 * that it is completed in a single, uninterruptible step, preventing race conditions
 * in multi-threaded environments.
 *
 * @param addr Pointer to the memory location whose value is to be exchanged.
 * @param newval The new value to be stored at the specified memory location.
 * @return The original value that was stored at the memory location before the exchange.
 */
int atomic_xchg(int *addr, int newval) {
  return atomic_exchange(addr, newval);
}
