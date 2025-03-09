#include <stdatomic.h>
#include "platform.h"

int __am_mpe_init = 0;
extern bool __am_has_ioe;
void __am_ioe_init();

/**
 * Initializes the Multi-Processor Environment (MPE) by setting up multiple CPU cores
 * and executing the provided entry function on each core. This method performs the
 * following steps:
 * 
 * 1. Sets the `__am_mpe_init` flag to indicate that MPE initialization has started.
 * 2. Creates a synchronization pipe to coordinate the startup of child processes.
 * 3. For each CPU core (excluding the first), forks a child process that:
 *    - Waits for a synchronization signal from the parent process.
 *    - Sets the CPU ID for the child process.
 *    - Initializes the timer interrupt and executes the provided entry function.
 * 4. If the system has I/O emulation (`__am_has_ioe`), initializes the I/O emulation.
 * 5. Sends a synchronization signal to all child processes to start execution.
 * 6. Executes the entry function on the parent (first) CPU core.
 * 7. Panics if the entry function returns, as it is expected to run indefinitely.
 * 
 * @param entry A function pointer to the entry function that will be executed on each CPU core.
 * @return This method does not return a meaningful value. It either runs indefinitely or panics.
 */
bool mpe_init(void (*entry)()) {
  __am_mpe_init = 1;

  int sync_pipe[2];
  assert(0 == pipe(sync_pipe));

  for (int i = 1; i < cpu_count(); i++) {
    if (fork() == 0) {
      char ch;
      assert(read(sync_pipe[0], &ch, 1) == 1);
      assert(ch == '+');
      close(sync_pipe[0]); close(sync_pipe[1]);

      thiscpu->cpuid = i;
      __am_init_timer_irq();
      entry();
    }
  }

  if (__am_has_ioe) {
    __am_ioe_init();
  }

  for (int i = 1; i < cpu_count(); i++) {
    assert(write(sync_pipe[1], "+", 1) == 1);
  }
  close(sync_pipe[0]); close(sync_pipe[1]);
  
  entry();
  panic("MP entry should not return\n");
}

/**
 * Retrieves the number of available CPUs in the system.
 *
 * This function returns the value of the global variable `__am_ncpu`, which is expected
 * to be initialized with the number of CPUs detected in the system. The function does not
 * perform any CPU detection itself but relies on the external variable to provide the count.
 *
 * @return The number of CPUs available in the system as an integer.
 */
int cpu_count() {
  extern int __am_ncpu;
  return __am_ncpu;
}

/**
 * Retrieves the CPU ID of the current CPU.
 *
 * This method returns the identifier (cpuid) of the CPU on which the current thread
 * is executing. The cpuid is obtained from the `thiscpu` structure, which represents
 * the current CPU's context.
 *
 * @return The integer value representing the current CPU's ID.
 */
int cpu_current() {
  return thiscpu->cpuid;
}

/**
 * Performs an atomic exchange operation on the integer value at the specified memory address.
 * 
 * This function atomically replaces the value at the given address with the new value and 
 * returns the original value that was stored at the address before the exchange. The operation 
 * is performed in a way that guarantees no other threads or processes can interrupt or interfere 
 * with the exchange, ensuring thread safety.
 *
 * @param addr A pointer to the integer value to be exchanged.
 * @param newval The new value to store at the specified address.
 * @return The original value that was stored at the address before the exchange.
 */
int atomic_xchg(int *addr, int newval) {
  return atomic_exchange((int *)addr, newval);
}
