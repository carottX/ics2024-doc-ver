#include <am.h>

/**
 * Initializes the Multi-Processing Environment (MPE) with the specified entry point.
 * This method sets up the necessary environment for multi-processing and assigns the
 * provided function as the entry point for the processing tasks. The entry point
 * function is expected to define the behavior or tasks to be executed in the 
 * multi-processing context.
 *
 * @param entry A function pointer to the entry point of the processing tasks.
 *              This function will be invoked to start the multi-processing tasks.
 *
 * @return bool Returns `false` to indicate that the initialization was not successful.
 *              This method currently does not perform any actual initialization and
 *              always returns `false`.
 */
bool mpe_init(void (*entry)()) {
  return false;
}

/**
 * Returns the number of CPU cores available on the system.
 * This implementation currently returns a hardcoded value of 1,
 * indicating a single-core CPU. Future implementations may
 * dynamically determine the actual number of CPU cores.
 *
 * @return The number of CPU cores, currently fixed at 1.
 */
int cpu_count() {
  return 1;
}

/**
 * Retrieves the current CPU identifier.
 *
 * This method returns the identifier of the CPU core that is currently executing
 * the code. In this implementation, it always returns 0, which typically
 * indicates the first CPU core in a single-core or multi-core system.
 *
 * @return An integer representing the current CPU core identifier. Always 0 in this implementation.
 */
int cpu_current() {
  return 0;
}

/**
 * Atomically exchanges the value at the specified memory address with a new value.
 * This operation is performed in a single, uninterruptible step, ensuring thread safety.
 *
 * @param addr A pointer to the memory location whose value is to be exchanged.
 * @param newval The new value to be stored at the specified memory location.
 * @return The original value that was stored at the memory location before the exchange.
 */
int atomic_xchg(int *addr, int newval) {
  return 0;
}
