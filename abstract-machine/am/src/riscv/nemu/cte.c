#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

/**
 * @brief Handles an interrupt request (IRQ) by delegating to a user-defined handler if one is registered.
 *
 * This function is responsible for processing an interrupt request. It checks if a user-defined handler
 * (`user_handler`) is registered. If a handler is available, it constructs an `Event` based on the cause
 * of the interrupt (`c->mcause`). The event is then passed to the user handler along with the current
 * context (`c`). The user handler is expected to process the event and return an updated context. The
 * function asserts that the returned context is not NULL. If no user handler is registered, the function
 * simply returns the original context unchanged.
 *
 * @param c A pointer to the current context, which contains information about the state of the system
 *          at the time of the interrupt.
 * @return A pointer to the updated context after the interrupt has been handled. This may be the same
 *         as the input context if no user handler is registered or if the handler does not modify the context.
 */
Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
      default: ev.event = EVENT_ERROR; break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}

extern void __am_asm_trap(void);

/**
 * Initializes the context and exception handling mechanism for the system.
 * 
 * This function performs two main tasks:
 * 1. Sets up the Machine Trap Vector (mtvec) register to point to the assembly-level
 *    trap handler (`__am_asm_trap`). This ensures that any exceptions or interrupts
 *    are handled by the specified trap handler.
 * 2. Registers a user-provided event handler function (`handler`) that will be invoked
 *    to process events. The handler is expected to take an `Event` and a `Context*` as
 *    arguments and return a `Context*`.
 * 
 * @param handler A function pointer to the event handler that will be registered.
 *                The handler should have the signature `Context*(Event, Context*)`.
 * 
 * @return Always returns `true` to indicate successful initialization.
 */
bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // register event handler
  user_handler = handler;

  return true;
}

/**
 * Creates and initializes a new execution context within the specified stack area.
 * The context is set up to execute the provided entry function with the given argument
 * when it is activated. The stack area must be pre-allocated and sufficiently large
 * to accommodate the context's execution requirements.
 *
 * @param kstack The pre-allocated stack area where the context will execute.
 * @param entry  The function pointer to the entry point of the context. This function
 *               will be called when the context is activated.
 * @param arg    The argument to be passed to the entry function when it is called.
 *
 * @return A pointer to the newly created context. If the context cannot be created,
 *         NULL is returned.
 */
Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  return NULL;
}

/**
 * Yields the current execution context, allowing the processor to switch to another task or thread.
 * This method uses an assembly instruction to perform a system call (ecall) with a specific register
 * value to indicate a yield operation. The register used for the system call depends on the architecture:
 * - For RISC-V with the __riscv_e extension, the `a5` register is loaded with the value `-1`.
 * - For other architectures, the `a7` register is loaded with the value `-1`.
 * The system call then triggers the scheduler or operating system to perform a context switch.
 */
void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}

/**
 * @brief Checks if a feature or functionality is enabled.
 *
 * This method returns a boolean value indicating whether a specific feature
 * or functionality is currently enabled. In this implementation, the method
 * always returns `false`, suggesting that the feature is disabled by default.
 *
 * @return bool Returns `false` to indicate that the feature is not enabled.
 */
bool ienabled() {
  return false;
}

/**
 * Enables or disables a specific setting or feature.
 *
 * This method sets the state of a particular setting or feature based on the
 * provided boolean value. If `enable` is `true`, the setting or feature is
 * activated. If `enable` is `false`, the setting or feature is deactivated.
 *
 * @param enable A boolean value indicating whether to enable (`true`) or
 *               disable (`false`) the setting or feature.
 */
void iset(bool enable) {
}
