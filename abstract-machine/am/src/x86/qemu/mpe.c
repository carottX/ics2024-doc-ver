#include "x86-qemu.h"

struct cpu_local __am_cpuinfo[MAX_CPU] = {};
static void (* volatile user_entry)();
static int ap_ready = 0;

/**
 * Calls the user-defined entry point function `user_entry()` and ensures that
 * the program does not return from it. If the `user_entry()` function returns,
 * this method triggers a panic with the message "MPE entry should not return".
 * This is typically used in systems where the user entry point is expected to
 * run indefinitely or terminate the program in a controlled manner, and a return
 * from it indicates an unexpected or erroneous condition.
 */
static void call_user_entry() {
  user_entry();
  panic("MPE entry should not return");
}

/**
 * Initializes the Multi-Processor Environment (MPE) by setting up the user entry point
 * and booting all Application Processors (APs) in the system. The method performs the
 * following steps:
 * 
 * 1. Sets the user entry point function to be called after initialization.
 * 2. Configures the boot record with a jump code (0x000bfde9) to start execution at
 *    the specified memory address (0x7c00).
 * 3. Iterates over all CPUs (excluding the Bootstrap Processor (BSP)) and marks them
 *    as APs in the boot record.
 * 4. Boots each AP using the Local APIC (Advanced Programmable Interrupt Controller)
 *    and waits until the AP signals that it is ready by setting the `ap_ready` flag.
 * 5. Calls the user-provided entry point function after all APs are ready.
 * 
 * @param entry A pointer to the user-defined entry function that will be called after
 *              the MPE initialization is complete.
 * 
 * @return Returns `true` to indicate successful initialization of the MPE.
 */
bool mpe_init(void (*entry)()) {
  user_entry = entry;
  boot_record()->jmp_code = 0x000bfde9; // (16-bit) jmp (0x7c00)
  for (int cpu = 1; cpu < __am_ncpu; cpu++) {
    boot_record()->is_ap = 1;
    __am_lapic_bootap(cpu, (void *)boot_record());
    while (xchg(&ap_ready, 0) != 1) {
      pause();
    }
  }
  call_user_entry();
  return true;
}

/**
 * Initializes the current CPU and transitions to user mode.
 *
 * This function performs the following steps:
 * 1. Initializes the per-CPU state by calling `__am_percpu_init()`.
 * 2. Signals that the current CPU is ready by atomically setting the `ap_ready` flag to 1.
 * 3. Transitions to user mode by invoking `call_user_entry()`.
 *
 * This function is typically called during the boot process to prepare a secondary CPU
 * for execution and switch to the user environment.
 */
static void othercpu_entry() {
  __am_percpu_init();
  xchg(&ap_ready, 1);
  call_user_entry();
}

/**
 * @brief Switches the stack to the top of the CPU's stack and calls the `othercpu_entry` function.
 * 
 * This function is responsible for switching the current stack to the top of the CPU's stack and then 
 * invoking the `othercpu_entry` function with a null argument. It is typically used in multi-core or 
 * multi-CPU environments to handle tasks or interrupts on a secondary CPU. The stack switch ensures 
 * that the secondary CPU operates with its own dedicated stack space.
 * 
 * @note The `stack_top` function retrieves the top of the CPU's stack, and `stack_switch_call` performs 
 * the actual stack switch and function call.
 */
void __am_othercpu_entry() {
  stack_switch_call(stack_top(&CPU->stack), othercpu_entry, 0);
}

/**
 * Returns the number of CPUs available in the system.
 *
 * This method retrieves the count of CPUs by accessing the global variable
 * `__am_ncpu`, which is expected to be initialized with the number of
 * available CPUs in the system.
 *
 * @return The number of CPUs as an integer.
 */
int cpu_count() {
  return __am_ncpu;
}

/**
 * Retrieves the current CPU ID by accessing the Local APIC (Advanced Programmable Interrupt Controller) 
 * register. The method reads the value at the 8th index of the __am_lapic array, which corresponds to 
 * the APIC ID register. The APIC ID is stored in the upper 8 bits (bits 24-31) of this register. 
 * The method shifts the value right by 24 bits to extract the CPU ID and returns it.
 *
 * @return The current CPU ID as an integer.
 */
int cpu_current(void) {
  return __am_lapic[8] >> 24;
}

/**
 * Performs an atomic exchange operation on the specified memory location.
 * This function atomically replaces the value at the memory address `addr` 
 * with `newval` and returns the original value that was stored at `addr`.
 *
 * @param addr Pointer to the memory location where the atomic exchange will occur.
 * @param newval The new value to be stored at the memory location.
 * @return The original value that was stored at the memory location before the exchange.
 */
int atomic_xchg(int *addr, int newval) {
  return xchg(addr, newval);
}

/**
 * Stops all CPUs except the current one by forcing them into an infinite loop.
 * This is achieved by modifying the boot record's jump code to a 16-bit jump
 * instruction that jumps to itself (0x0000feeb), effectively halting the CPU.
 * The method then iterates over all CPUs (excluding the current one) and uses
 * the Local APIC (Advanced Programmable Interrupt Controller) to boot them with
 * the modified boot record, causing them to enter the infinite loop.
 * This function is typically used to halt all other CPUs during system shutdown
 * or critical operations where only the current CPU should remain active.
 */
void __am_stop_the_world() {
  boot_record()->jmp_code = 0x0000feeb; // (16-bit) jmp .
  for (int cpu_ = 0; cpu_ < __am_ncpu; cpu_++) {
    if (cpu_ != cpu_current()) {
      __am_lapic_bootap(cpu_, (void *)boot_record());
    }
  }
}
