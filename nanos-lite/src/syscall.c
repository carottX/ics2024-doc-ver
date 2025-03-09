#include <common.h>
#include "syscall.h"
/**
 * @brief Executes a system call based on the provided context.
 *
 * This method retrieves the system call identifier from the first general-purpose
 * register (GPR1) in the given context and attempts to handle it. If the system
 * call identifier is not recognized, the method triggers a panic with an error
 * message indicating the unhandled system call ID.
 *
 * @param c Pointer to the Context structure containing the system call context,
 *          including the general-purpose registers.
 */
void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;

  switch (a[0]) {
    default: panic("Unhandled syscall ID = %d", a[0]);
  }
}
