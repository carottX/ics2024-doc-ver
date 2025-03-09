#include <am.h>
#include <loongarch/loongarch32r.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

/**
 * __am_irq_handle - Handles an interrupt request (IRQ) by invoking a user-defined handler if available.
 *
 * This function processes an interrupt request by checking if a user-defined handler (`user_handler`) is registered.
 * If a handler is available, it constructs an `Event` object based on the error code (`ecode`) and passes it to the
 * handler along with the current execution context (`c`). The handler is expected to process the event and return
 * an updated context. The function asserts that the returned context is not `NULL` to ensure the handler behaves
 * correctly. If no user handler is registered, the function simply returns the original context.
 *
 * @param c Pointer to the current execution context.
 * @return Pointer to the updated execution context after handling the IRQ. If no user handler is registered,
 *         the original context is returned.
 */
Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    uintptr_t ecode = 0;
    switch (ecode) {
      default: ev.event = EVENT_ERROR; break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}

extern void __am_asm_trap(void);

/**
 * Initializes the Context-Trap-Entry (CTE) mechanism by setting up the exception
 * entry point and registering an event handler.
 *
 * This function performs two main tasks:
 * 1. It sets the exception entry point in the Control and Status Register (CSR)
 *    to the address of the `__am_asm_trap` function, which is responsible for
 *    handling exceptions and traps. The CSR address `0xc` corresponds to the
 *    `eentry` register, which defines the entry point for exception handling.
 * 2. It registers a user-defined event handler function that will be invoked
 *    when an event occurs. The handler is responsible for processing the event
 *    and returning the appropriate context.
 *
 * @param handler A function pointer to the event handler. The handler takes
 *                an `Event` and a `Context*` as arguments and returns a `Context*`.
 *                The `Event` represents the type of event that occurred, and the
 *                `Context*` represents the current execution context.
 *
 * @return Always returns `true` to indicate successful initialization.
 */
bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  asm volatile("csrwr %0, 0xc" : : "r"(__am_asm_trap));  // 0xc = eentry

  // register event handler
  user_handler = handler;

  return true;
}

/**
 * Creates a new execution context with the specified stack, entry point, and argument.
 *
 * This function initializes a new context that can be used for executing code in a separate
 * execution environment. The context is created with the provided stack area, an entry point
 * function that will be executed when the context is activated, and an argument that will be
 * passed to the entry point function.
 *
 * @param kstack The memory area to be used as the stack for the new context. The stack must
 *               be large enough to support the execution of the context.
 * @param entry  A pointer to the function that will be executed when the context is activated.
 *               This function should take a single void pointer argument and return void.
 * @param arg    A pointer to the argument that will be passed to the entry point function
 *               when the context is activated.
 *
 * @return A pointer to the newly created context. If the context cannot be created, NULL is
 *         returned.
 */
Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  return NULL;
}

/**
 * Yields the current thread's execution, allowing the operating system to 
 * schedule another thread or process to run. This method achieves this by 
 * invoking a system call with a specific argument (`-1` in register `$a7`) 
 * that signals the OS to perform a context switch. The `asm volatile` 
 * instruction ensures that the assembly code is executed exactly as written 
 * and is not optimized away by the compiler.
 */
void yield() {
  asm volatile("li.w $a7, -1; syscall 0");
}

/**
 * @brief Checks if a feature or functionality is enabled.
 * 
 * This method returns a boolean value indicating whether a specific feature or 
 * functionality is currently enabled. In this implementation, it always returns 
 * `false`, suggesting that the feature is disabled by default.
 * 
 * @return bool Returns `false` indicating the feature is not enabled.
 */
bool ienabled() {
  return false;
}

/**
 * Enables or disables a specific setting or feature.
 *
 * This method sets the state of a particular setting or feature based on the
 * provided boolean value. If `enable` is true, the setting is activated; if
 * false, the setting is deactivated.
 *
 * @param enable A boolean value indicating whether to enable (true) or
 *               disable (false) the setting.
 */
void iset(bool enable) {
}
