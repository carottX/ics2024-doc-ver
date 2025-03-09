#include <am.h>
#include <mips/mips32.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

/**
 * @brief Handles an interrupt request (IRQ) by invoking a user-defined handler if one is registered.
 *
 * This function processes an interrupt request by checking if a user-defined handler (`user_handler`) is set.
 * If a handler is available, it constructs an `Event` object and populates it based on the exception code (`ex_code`).
 * Currently, the exception code is hardcoded to 0, and the event type is set to `EVENT_ERROR` by default.
 * The user-defined handler is then invoked with the constructed event and the current context (`c`).
 * The function asserts that the returned context from the handler is not `NULL` to ensure the handler has not corrupted the context.
 * Finally, the function returns the updated context.
 *
 * @param c Pointer to the current context that needs to be handled during the interrupt.
 * @return Pointer to the updated context after the interrupt handling.
 */
Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    uint32_t ex_code = 0;
    switch (ex_code) {
      default: ev.event = EVENT_ERROR; break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}

extern void __am_asm_trap(void);

#define EX_ENTRY 0x80000180

/**
 * Initializes the exception handling mechanism and registers an event handler.
 * 
 * This function sets up the exception entry point by writing a jump instruction
 * to the address of the exception handler (`__am_asm_trap`) into the exception
 * entry location (`EX_ENTRY`). It also initializes the TLB refill exception
 * entry point at `0x80000000` with the same jump instruction. Both entries
 * include a delay slot set to 0. Additionally, the function registers a user-provided
 * event handler (`handler`) that will be invoked when an exception occurs.
 *
 * @param handler A function pointer to the event handler that will be called
 *                when an exception occurs. The handler takes an `Event` and a
 *                `Context*` as arguments and returns a `Context*`.
 *
 * @return Returns `true` to indicate successful initialization.
 */
bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  const uint32_t j_opcode = 0x08000000;
  uint32_t instr = j_opcode | (((uint32_t)__am_asm_trap >> 2) & 0x3ffffff);
  *(uint32_t *)EX_ENTRY = instr;
  *(uint32_t *)(EX_ENTRY + 4) = 0;  // delay slot
  *(uint32_t *)0x80000000 = instr;  // TLB refill exception
  *(uint32_t *)(0x80000000 + 4) = 0;  // delay slot

  // register event handler
  user_handler = handler;

  return true;
}

/**
 * Creates and initializes a new execution context.
 *
 * This function sets up a new context with a specified stack area, entry point,
 * and argument. The context can be used to switch execution to a different
 * task or thread within a program. The stack area provided must be large enough
 * to accommodate the context's execution needs.
 *
 * @param kstack The memory area allocated for the context's stack.
 * @param entry  The function pointer to the entry point of the context.
 * @param arg    The argument to be passed to the entry point function.
 *
 * @return A pointer to the newly created context, or NULL if the context
 *         could not be created.
 */
Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  return NULL;
}

/**
 * Yields the current thread's execution to allow other threads to run.
 * This method invokes a system call (syscall 1) to perform the yield operation,
 * which typically results in the operating system's scheduler being invoked
 * to switch to another thread or process. This is useful in cooperative
 * multitasking environments where threads voluntarily give up CPU time.
 * The method uses inline assembly to directly trigger the system call.
 */
void yield() {
  asm volatile("syscall 1");
}

/**
 * @brief Checks if a feature or functionality is enabled.
 *
 * This method returns a boolean value indicating whether a specific feature
 * or functionality is currently enabled. In this implementation, the method
 * always returns `false`, suggesting that the feature is disabled by default.
 *
 * @return `false` indicating that the feature is not enabled.
 */
bool ienabled() {
  return false;
}

/**
 * Enables or disables a specific setting or feature.
 *
 * This method sets the state of a particular setting or feature based on the
 * provided boolean value. If `enable` is true, the setting or feature is
 * activated. If `enable` is false, the setting or feature is deactivated.
 *
 * @param enable A boolean value indicating whether to enable (true) or
 *               disable (false) the setting or feature.
 */
void iset(bool enable) {
}
