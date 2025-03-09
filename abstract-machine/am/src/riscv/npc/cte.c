#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

/**
 * __am_irq_handle - Handles an interrupt by delegating to a user-defined handler.
 *
 * This function processes an interrupt by checking if a user-defined handler (`user_handler`) 
 * is registered. If a handler is available, it constructs an `Event` object based on the 
 * interrupt cause (`c->mcause`). The event is then passed to the user handler along with 
 * the current context. The handler is expected to process the event and return a modified 
 * context. The function asserts that the returned context is not NULL.
 *
 * If no user handler is registered, the function simply returns the original context 
 * without any modifications.
 *
 * @param c Pointer to the current context, which contains information about the 
 *          interrupt, including the cause (`mcause`).
 * @return  Pointer to the updated context after the interrupt handling. If no user 
 *          handler is registered, the original context is returned.
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
 * Initializes the context and exception handling mechanism.
 *
 * This function performs two main tasks:
 * 1. Sets the Machine Trap Vector Base Address Register (mtvec) to the address of the
 *    trap handler function (`__am_asm_trap`), which is responsible for handling exceptions
 *    and interrupts in the system.
 * 2. Registers a user-provided event handler function that will be invoked to process
 *    events in the context of the current execution environment.
 *
 * @param handler A function pointer to the event handler. The event handler takes two
 *                parameters: an `Event` type representing the event to be handled, and a
 *                `Context*` type representing the current execution context. It returns
 *                a pointer to the updated context after handling the event.
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
 * Creates a new execution context with the specified stack area, entry point, and argument.
 *
 * This function initializes a new context that can be used for cooperative multitasking or
 * lightweight threading. The context is associated with the provided stack area, and when
 * the context is activated, execution will begin at the specified entry point function.
 *
 * @param kstack The memory area to be used as the stack for the new context. The stack must
 *               be large enough to support the context's execution.
 * @param entry  A pointer to the function that will be executed when the context is activated.
 *               This function takes a single void pointer argument and returns void.
 * @param arg    A pointer to the argument that will be passed to the entry function when the
 *               context is activated.
 *
 * @return A pointer to the newly created context. If the context cannot be created, NULL is
 *         returned.
 */
Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  return NULL;
}

/**
 * Yields the current thread or process, allowing the scheduler to switch to another thread or process.
 * This method uses an assembly instruction to perform a system call (ecall) with a specific register value
 * to indicate the yield operation. The register used depends on the architecture:
 * - On RISC-V with the 'E' extension (__riscv_e), the `a5` register is used.
 * - On other architectures, the `a7` register is used.
 * The value `-1` is loaded into the respective register before invoking the system call.
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
 * Enables or disables a specific feature or functionality based on the provided boolean value.
 * If the 'enable' parameter is set to true, the feature is activated; if false, the feature is deactivated.
 * This method does not return any value.
 *
 * @param enable A boolean value indicating whether to enable (true) or disable (false) the feature.
 */
void iset(bool enable) {
}
