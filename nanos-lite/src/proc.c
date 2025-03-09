#include <proc.h>

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

/**
 * Switches the current process control block (PCB) to the boot PCB.
 * This function updates the global 'current' pointer to point to the
 * 'pcb_boot' structure, effectively setting the boot PCB as the
 * currently active process control block.
 */
void switch_boot_pcb() {
  current = &pcb_boot;
}

/**
 * This function continuously prints a "Hello World" message to the log, 
 * including the provided argument and a counter indicating the number of 
 * times the message has been printed. The function runs in an infinite loop, 
 * yielding control after each iteration to allow other tasks to execute.
 *
 * @param arg A pointer to an arbitrary argument that is included in the log 
 *            message. The argument is cast to a uintptr_t type for printing.
 *
 * The function increments a counter `j` starting from 1 and logs the message 
 * in the following format:
 * "Hello World from Nanos-lite with arg '<arg>' for the <j>th time!"
 *
 * The function does not return and runs indefinitely.
 */
void hello_fun(void *arg) {
  int j = 1;
  while (1) {
    Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (uintptr_t)arg, j);
    j ++;
    yield();
  }
}

/**
 * Initializes the process management system by performing the following steps:
 * 1. Switches to the boot process control block (PCB) to set up the initial context.
 * 2. Logs a message indicating the start of process initialization.
 * 3. Loads the initial program into memory (implementation details for loading the program are not provided in this method).
 * 
 * This method is typically called during system startup to prepare the environment
 * for process execution and ensure that the system is ready to handle processes.
 */
void init_proc() {
  switch_boot_pcb();

  Log("Initializing processes...");

  // load program here

}

/**
 * Schedules the next context to be executed after the given context.
 * This method is typically used in a scheduling algorithm to determine
 * which context should run next in a multitasking or multithreading environment.
 * The method takes a pointer to the previous context that was executed and
 * returns a pointer to the next context to be executed. If there is no context
 * to schedule, it returns NULL.
 *
 * @param prev A pointer to the previous context that was executed.
 * @return A pointer to the next context to be executed, or NULL if no context
 *         is available to schedule.
 */
Context* schedule(Context *prev) {
  return NULL;
}
