#include <sys/time.h>
#include <string.h>
#include "platform.h"

#define TIMER_HZ 100
#define SYSCALL_INSTR_LEN 7

static Context* (*user_handler)(Event, Context*) = NULL;

void __am_kcontext_start();
void __am_switch(Context *c);
int __am_in_userspace(void *addr);
void __am_pmem_protect();
void __am_pmem_unprotect();

/**
 * @brief Triggers a panic state with a specific error message indicating that 
 *        the program execution should not reach this point.
 *
 * This function is typically used as a safeguard in code paths that are 
 * expected to be unreachable under normal circumstances. When invoked, it 
 * immediately halts the program's execution and outputs the error message 
 * "should not reach here\n" to indicate that an unexpected or erroneous 
 * execution path has been taken.
 *
 * @note This function does not return and will terminate the program.
 */
void __am_panic_on_return() { panic("should not reach here\n"); }

/**
 * Handles an interrupt request (IRQ) by saving the current context and 
 * processing the event associated with the interrupt. 
 *
 * This function performs the following steps:
 * 1. Updates the context `c` with the current virtual machine head (`vm_head`) 
 *    and kernel stack pointer (`ksp`) from the current CPU (`thiscpu`).
 * 2. Checks if the event type is `EVENT_ERROR`. If so, it prints an error message 
 *    containing details such as the event message, program counter, bad address, 
 *    and cause, and then triggers an assertion failure.
 * 3. Calls the `user_handler` function to process the event and update the context. 
 *    The resulting context is validated to ensure it is not `NULL`.
 * 4. Switches to the updated context using `__am_switch`.
 * 5. Performs a magic call to a function at address `0x100008` to restore the context.
 * 6. Calls `__am_panic_on_return` to ensure the function does not return unexpectedly.
 *
 * @param c Pointer to the context structure to be updated and restored.
 */
static void irq_handle(Context *c) {
  c->vm_head = thiscpu->vm_head;
  c->ksp = thiscpu->ksp;

  if (thiscpu->ev.event == EVENT_ERROR) {
    printf("Unhandle signal '%s' at pc = %p, badaddr = %p, cause = 0x%x\n",
      thiscpu->ev.msg, AM_REG_PC(&c->uc), thiscpu->ev.ref, thiscpu->ev.cause);
    assert(0);
  }
  c = user_handler(thiscpu->ev, c);
  assert(c != NULL);

  __am_switch(c);

  // magic call to restore context
  void (*p)(Context *c) = (void *)(uintptr_t)0x100008;
  p(c);
  __am_panic_on_return();
}

/**
 * @brief Configures the stack and context for handling interrupts and system calls.
 *
 * This method prepares the stack and context for handling various events such as interrupts,
 * system calls, and signals. It ensures that the execution context is saved and modified
 * appropriately to handle the event safely. The method also checks if the current execution
 * context is in a signal-safe state before proceeding with the event handling.
 *
 * @param event The type of event to handle (e.g., EVENT_IRQ_IODEV, EVENT_IRQ_TIMER, EVENT_SYSCALL).
 * @param uc Pointer to the user context (ucontext_t) that contains the current execution state.
 *
 * The method performs the following steps:
 * 1. Determines if the current program counter (PC) is in user space or within a signal-safe range.
 * 2. If the event is an interrupt and the current context is not signal-safe, the method returns
 *    without handling the event to avoid undefined behavior.
 * 3. If the event is a system call, the PC is adjusted to skip the syscall instruction.
 * 4. The stack pointer (SP) is switched to the kernel stack if the event originated from user space.
 * 5. The current context is saved on the stack, and the interrupt signal mask is updated to disable
 *    interrupts.
 * 6. The context is modified to call the `irq_handle` function after returning from the signal handler.
 *
 * @note This method contains architecture-specific adjustments for x86_64 to ensure proper stack
 * alignment for SSE instructions.
 */
static void setup_stack(uintptr_t event, ucontext_t *uc) {
  void *pc = (void *)AM_REG_PC(uc);
  extern uint8_t _start, _etext;
  int trap_from_user = __am_in_userspace(pc);
  int signal_safe = IN_RANGE(pc, RANGE(&_start, &_etext)) || trap_from_user ||
    // Hack here: "+13" points to the instruction after syscall. This is the
    // instruction which will trigger the pending signal if interrupt is enabled.
    // FIXME: should change 13 for aarch and riscv
    (pc == (void *)&sigprocmask + 13);

  if (((event == EVENT_IRQ_IODEV) || (event == EVENT_IRQ_TIMER)) && !signal_safe) {
    // Shared libraries contain code which are not reenterable.
    // If the signal comes when executing code in shared libraries,
    // the signal handler can not call any function which is not signal-safe,
    // else the behavior is undefined (may be dead lock).
    // To handle this, we just refuse to handle the signal and return directly
    // to pretend missing the interrupt.
    // See man 7 signal-safety for more information.
    return;
  }

  if (trap_from_user) __am_pmem_unprotect();

  // skip the instructions causing SIGSEGV for syscall
  if (event == EVENT_SYSCALL) { pc += SYSCALL_INSTR_LEN; }
  AM_REG_PC(uc) = (uintptr_t)pc;

  // switch to kernel stack if we were previously in user space
  uintptr_t sp = trap_from_user ? thiscpu->ksp : AM_REG_SP(uc);
  sp -= sizeof(Context);
#ifdef __x86_64__
  // keep (sp + 8) % 16 == 0 to support SSE
  if ((sp + 8) % 16 != 0) sp -= 8;
#endif
  Context *c = (void *)sp;

  // save the context on the stack
  c->uc = *uc;

  // disable interrupt
  __am_get_intr_sigmask(&uc->uc_sigmask);

  // call irq_handle after returning from the signal handler
  AM_REG_GPR1(uc) = (uintptr_t)c;
  AM_REG_PC(uc)   = (uintptr_t)irq_handle;
  AM_REG_SP(uc)   = (uintptr_t)c;
}

/**
 * Restores the execution context from a saved context structure.
 *
 * This function retrieves the saved context from the provided `Context` structure
 * and restores it into the given `ucontext_t` object. It also updates the kernel
 * stack pointer (`ksp`) of the current CPU to the saved value. If the program
 * counter (PC) indicates that the execution is in user space, it invokes memory
 * protection to ensure proper isolation between user and kernel spaces.
 *
 * @param uc Pointer to the `ucontext_t` structure where the context will be restored.
 */
static void iret(ucontext_t *uc) {
  Context *c = (void *)AM_REG_GPR1(uc);
  // restore the context
  *uc = c->uc;
  thiscpu->ksp = c->ksp;
  if (__am_in_userspace((void *)AM_REG_PC(uc))) __am_pmem_protect();
}

/**
 * @brief Signal handler function that processes various signals and updates the CPU event state accordingly.
 *
 * This function is a signal handler that is invoked when a signal is received. It processes the signal
 * and updates the event state (`thiscpu->ev`) based on the type of signal received. The function handles
 * the following signals:
 * - SIGUSR1: Sets the event to `EVENT_IRQ_IODEV`.
 * - SIGUSR2: Sets the event to `EVENT_YIELD`.
 * - SIGVTALRM: Sets the event to `EVENT_IRQ_TIMER`.
 * - SIGSEGV: Processes segmentation faults, distinguishing between different types of memory access errors
 *   and system calls. If the fault occurs in user space, it sets the event to `EVENT_PAGEFAULT` and records
 *   the cause (e.g., read or write access violation) and the faulting address.
 *
 * For other signals, the event is set to `EVENT_ERROR`, and additional information such as the signal number,
 * faulting address, and signal message is recorded.
 *
 * After processing the signal, the function sets up the stack for the event using `setup_stack`.
 *
 * @param sig The signal number received.
 * @param info Pointer to a `siginfo_t` structure containing additional information about the signal.
 * @param ucontext Pointer to a `ucontext_t` structure containing the context of the thread that received the signal.
 */
static void sig_handler(int sig, siginfo_t *info, void *ucontext) {
  thiscpu->ev = (Event) {0};
  thiscpu->ev.event = EVENT_ERROR;
  switch (sig) {
    case SIGUSR1: thiscpu->ev.event = EVENT_IRQ_IODEV; break;
    case SIGUSR2: thiscpu->ev.event = EVENT_YIELD; break;
    case SIGVTALRM: thiscpu->ev.event = EVENT_IRQ_TIMER; break;
    case SIGSEGV:
      if (info->si_code == SEGV_ACCERR) {
        switch ((uintptr_t)info->si_addr) {
          case 0x100000: thiscpu->ev.event = EVENT_SYSCALL; break;
          case 0x100008: iret(ucontext); return;
        }
      }
      if (__am_in_userspace(info->si_addr)) {
        assert(thiscpu->ev.event == EVENT_ERROR);
        thiscpu->ev.event = EVENT_PAGEFAULT;
        switch (info->si_code) {
          case SEGV_MAPERR: thiscpu->ev.cause = MMAP_READ; break;
          // we do not support mapped user pages with MMAP_NONE
          case SEGV_ACCERR: thiscpu->ev.cause = MMAP_WRITE; break;
          default: assert(0);
        }
        thiscpu->ev.ref = (uintptr_t)info->si_addr;
      }
      break;
  }

  if (thiscpu->ev.event == EVENT_ERROR) {
    thiscpu->ev.ref = (uintptr_t)info->si_addr;
    thiscpu->ev.cause = (uintptr_t)info->si_code;
    thiscpu->ev.msg = strsignal(sig);
  }
  setup_stack(thiscpu->ev.event, ucontext);
}

// signal handlers are inherited across fork()
static void install_signal_handler() {
  struct sigaction s;
  memset(&s, 0, sizeof(s));
  s.sa_sigaction = sig_handler;
  s.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
  __am_get_intr_sigmask(&s.sa_mask);

  int ret = sigaction(SIGVTALRM, &s, NULL);
  assert(ret == 0);
  ret = sigaction(SIGUSR1, &s, NULL);
  assert(ret == 0);
  ret = sigaction(SIGUSR2, &s, NULL);
  assert(ret == 0);
  ret = sigaction(SIGSEGV, &s, NULL);
  assert(ret == 0);
}

// setitimer() are inherited across fork(), should be called again from children
void __am_init_timer_irq() {
  iset(0);

  struct itimerval it = {};
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = 1000000 / TIMER_HZ;
  it.it_interval = it.it_value;
  int ret = setitimer(ITIMER_VIRTUAL, &it, NULL);
  assert(ret == 0);
}

/**
 * Initializes the context tracking environment (CTE) by setting up the user-defined
 * event handler and installing necessary signal handlers and timer interrupts.
 *
 * This function assigns the provided event handler to the global `user_handler` variable,
 * which will be invoked to process events. It then installs a signal handler to manage
 * asynchronous signals and initializes the timer interrupt to enable periodic event
 * processing.
 *
 * @param handler A function pointer to the user-defined event handler. This handler
 *                takes an `Event` and a `Context*` as input and returns a `Context*`.
 *                The handler is responsible for processing events and updating the
 *                context as needed.
 *
 * @return Returns `true` to indicate successful initialization.
 */
bool cte_init(Context*(*handler)(Event, Context*)) {
  user_handler = handler;

  install_signal_handler();
  __am_init_timer_irq();
  return true;
}

/**
 * Initializes a new kernel context within the provided stack area.
 *
 * This function sets up a context for kernel execution, including the program counter,
 * stack pointer, and general-purpose registers. It also initializes the signal mask to
 * enable interrupts and prepares the context for execution by setting the entry point
 * and argument for the kernel function.
 *
 * @param kstack The memory area allocated for the kernel stack.
 * @param entry  A pointer to the function that will be executed in the new context.
 * @param arg    A pointer to the argument that will be passed to the entry function.
 *
 * @return A pointer to the initialized Context structure.
 */
Context* kcontext(Area kstack, void (*entry)(void *), void *arg) {
  Context *c = (Context*)kstack.end - 1;

  __am_get_example_uc(c);
  AM_REG_PC(&c->uc) = (uintptr_t)__am_kcontext_start;
  AM_REG_SP(&c->uc) = (uintptr_t)kstack.end;

  int ret = sigemptyset(&(c->uc.uc_sigmask)); // enable interrupt
  assert(ret == 0);

  c->vm_head = NULL;

  c->GPR1 = (uintptr_t)arg;
  c->GPR2 = (uintptr_t)entry;
  return c;
}

/**
 * Sends a SIGUSR2 signal to the calling process, effectively yielding control
 * to the operating system or another process. This can be used to implement
 * cooperative multitasking or to pause the current process temporarily.
 * The SIGUSR2 signal is a user-defined signal that can be caught and handled
 * by a custom signal handler if one is registered.
 */
void yield() {
  raise(SIGUSR2);
}

/**
 * Checks whether interrupts are currently enabled in the calling thread.
 *
 * This function retrieves the current signal mask of the calling thread using
 * `sigprocmask` and then checks if interrupts are enabled by examining the
 * signal mask. The function uses `__am_is_sigmask_sti` to determine the
 * interrupt state based on the signal mask.
 *
 * @return `true` if interrupts are enabled, `false` otherwise.
 *
 * @note This function asserts that the call to `sigprocmask` succeeds. If the
 *       call fails, the program will terminate with an assertion failure.
 */
bool ienabled() {
  sigset_t set;
  int ret = sigprocmask(0, NULL, &set);
  assert(ret == 0);
  return __am_is_sigmask_sti(&set);
}

/**
 * Enables or disables the interrupt signal mask for the current process.
 * 
 * This function modifies the signal mask of the current process to either block or unblock
 * the signals specified in the global `__am_intr_sigmask` variable. The behavior is controlled
 * by the `enable` parameter:
 * - If `enable` is `true`, the signals in `__am_intr_sigmask` are unblocked (SIG_UNBLOCK).
 * - If `enable` is `false`, the signals in `__am_intr_sigmask` are blocked (SIG_BLOCK).
 * 
 * The function uses `sigprocmask` to modify the signal mask. Note that `sigprocmask` is not
 * supported in multithreaded environments, and its use should be limited to single-threaded
 * contexts.
 * 
 * @param enable A boolean value that determines whether to enable (unblock) or disable (block)
 *               the interrupt signals.
 * 
 * @note The function asserts that the call to `sigprocmask` succeeds (i.e., returns 0). If the
 *       call fails, the program will terminate with an assertion error.
 */
void iset(bool enable) {
  extern sigset_t __am_intr_sigmask;
  // NOTE: sigprocmask does not supported in multithreading
  int ret = sigprocmask(enable ? SIG_UNBLOCK : SIG_BLOCK, &__am_intr_sigmask, NULL);
  assert(ret == 0);
}
