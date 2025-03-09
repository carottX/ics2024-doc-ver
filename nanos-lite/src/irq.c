#include <common.h>

/**
 * Handles an event and updates the provided context accordingly.
 * 
 * This method processes the given event `e` and modifies the context `c` based on the event type.
 * Currently, the method only handles a default case where it panics if an unhandled event ID is encountered.
 * 
 * @param e The event to be processed.
 * @param c The context to be updated based on the event.
 * @return The updated context after processing the event.
 */
static Context* do_event(Event e, Context* c) {
  switch (e.event) {
    default: panic("Unhandled event ID = %d", e.event);
  }

  return c;
}

/**
 * Initializes the interrupt/exception handler for the system.
 * This function logs a message indicating the start of the initialization process
 * and then calls the `cte_init` function, passing the `do_event` function as an argument.
 * The `cte_init` function is responsible for setting up the core trap/exception handling
 * mechanism, which will use `do_event` to process incoming interrupts or exceptions.
 */
void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}
