#include <am.h>
#include <x86/x86.h>
#include <klib.h>

#define NR_IRQ         256     // IDT size
#define SEG_KCODE      1
#define SEG_KDATA      2

static Context* (*user_handler)(Event, Context*) = NULL;

void __am_irq0();
void __am_vecsys();
void __am_vectrap();
void __am_vecnull();


/**
 * __am_irq_handle - Handles an interrupt request (IRQ) by invoking a user-defined handler.
 *
 * This function processes an interrupt request by checking if a user-defined handler is registered.
 * If a handler is available, it constructs an event based on the interrupt type and passes it to
 * the handler along with the current context. The handler is expected to return a modified context,
 * which is then validated to ensure it is not NULL.
 *
 * @param c A pointer to the current context containing information about the interrupt.
 * @return A pointer to the updated context after the interrupt handling is complete.
 */
Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    switch (c->irq) {
      default: ev.event = EVENT_ERROR; break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}

/**
 * Initializes the interrupt descriptor table (IDT) and registers an event handler.
 * 
 * This function sets up the IDT with default gate descriptors for all interrupts,
 * and then configures specific gates for critical interrupts and system calls.
 * 
 * The IDT is initialized with null handlers for all interrupts (NR_IRQ), and then
 * specific interrupts are configured:
 * - Interrupt 32 (IRQ0) is set to handle timer interrupts.
 * - Interrupt 0x80 is set to handle system calls from user mode.
 * - Interrupt 0x81 is set to handle traps in kernel mode.
 * 
 * After configuring the IDT, the function registers a user-provided event handler
 * that will be called in response to events.
 * 
 * @param handler A function pointer to the event handler. This handler takes an
 *                Event and a Context* as arguments and returns a Context*.
 * 
 * @return Returns true if the initialization was successful.
 */
bool cte_init(Context*(*handler)(Event, Context*)) {
  static GateDesc32 idt[NR_IRQ];

  // initialize IDT
  for (unsigned int i = 0; i < NR_IRQ; i ++) {
    idt[i]  = GATE32(STS_TG, KSEL(SEG_KCODE), __am_vecnull, DPL_KERN);
  }

  // ----------------------- interrupts ----------------------------
  idt[32]   = GATE32(STS_IG, KSEL(SEG_KCODE), __am_irq0,    DPL_KERN);
  // ---------------------- system call ----------------------------
  idt[0x80] = GATE32(STS_TG, KSEL(SEG_KCODE), __am_vecsys,  DPL_USER);
  idt[0x81] = GATE32(STS_TG, KSEL(SEG_KCODE), __am_vectrap, DPL_KERN);

  set_idt(idt, sizeof(idt));

  // register event handler
  user_handler = handler;

  return true;
}


/**
 * Creates a new execution context with the specified stack, entry point, and argument.
 *
 * This function initializes a new context that can be used to switch execution
 * to a different thread or coroutine. The context is created with the provided
 * stack area, entry point function, and argument that will be passed to the
 * entry point when the context is activated.
 *
 * @param kstack The memory area to be used as the stack for the new context.
 *               The stack must be properly aligned and large enough to support
 *               the execution of the context.
 * @param entry  The entry point function that will be executed when the context
 *               is activated. This function should accept a single void* argument
 *               and return void.
 * @param arg    The argument to be passed to the entry point function when the
 *               context is activated.
 *
 * @return A pointer to the newly created context. If the context cannot be created,
 *         NULL is returned.
 */
Context* kcontext(Area kstack, void (*entry)(void *), void *arg) {
  return NULL;
}

/**
 * Yields the current thread's execution to the operating system or scheduler.
 * This method triggers a software interrupt (int $0x81) to signal the OS to
 * perform a context switch, allowing other threads or processes to run.
 * It is typically used in cooperative multitasking environments where threads
 * voluntarily yield CPU time to others.
 */
void yield() {
  asm volatile("int $0x81");
}

/**
 * @brief Checks if a feature or functionality is enabled.
 *
 * This method returns a boolean value indicating whether a specific feature
 * or functionality is currently enabled. In this implementation, it always
 * returns `false`, suggesting that the feature is disabled by default.
 *
 * @return bool Returns `false` to indicate that the feature is disabled.
 */
bool ienabled() {
  return false;
}

/**
 * @brief Enables or disables a specific feature or setting.
 *
 * This method sets the state of a particular feature or setting based on the
 * provided boolean value. If `enable` is true, the feature is activated; if
 * false, the feature is deactivated. The exact behavior depends on the
 * implementation of the feature or setting being controlled.
 *
 * @param enable A boolean value indicating whether to enable (true) or
 *               disable (false) the feature or setting.
 */
void iset(bool enable) {
}
