#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/auxv.h>
#include <dlfcn.h>
#include <elf.h>
#include <stdlib.h>
#include <stdio.h>
#include "platform.h"

#define MAX_CPU 16
#define TRAP_PAGE_START (void *)0x100000
#define PMEM_START (void *)0x1000000  // for nanos-lite with vme disabled
#define PMEM_SIZE (128 * 1024 * 1024) // 128MB
static int pmem_fd = 0;
static void *pmem = NULL;
static ucontext_t uc_example = {};
static void *(*memcpy_libc)(void *, const void *, size_t) = NULL;
sigset_t __am_intr_sigmask = {};
__am_cpu_t *__am_cpu_struct = NULL;
int __am_ncpu = 0;
int __am_pgsize = 0;

/**
 * @brief Saves the current execution context of the process when a signal is received.
 *
 * This function is a signal handler that captures the current execution context (including CPU registers, 
 * program counter, and stack pointer) when a signal is delivered to the process. It uses the `memcpy_libc` 
 * function to copy the context information from the provided `ucontext` structure into a globally accessible 
 * structure `uc_example`. This is useful for debugging, crash recovery, or context switching in signal handling.
 *
 * @param sig The signal number that triggered this handler.
 * @param info A pointer to a `siginfo_t` structure containing additional information about the signal.
 * @param ucontext A pointer to a `ucontext_t` structure that holds the execution context of the process 
 *                 at the time the signal was received.
 */
static void save_context_handler(int sig, siginfo_t *info, void *ucontext) {
  memcpy_libc(&uc_example, ucontext, sizeof(uc_example));
}

/**
 * Saves the example context by setting up a signal handler for SIGUSR1 and raising the signal.
 * This method ensures that the saved context includes valid segment registers, which are not
 * preserved by the `getcontext()` function. When `getcontext()` is used, restoring the context
 * in a signal handler can lead to a segmentation fault due to invalid segment registers. This
 * method addresses this issue by saving the context during signal handling, where the segment
 * registers are valid.
 *
 * The method performs the following steps:
 * 1. Initializes a `sigaction` structure and sets its `sa_sigaction` field to the
 *    `save_context_handler` function, which is responsible for saving the context.
 * 2. Configures the `sigaction` structure to use the `SA_SIGINFO` flag, enabling the signal
 *    handler to receive additional information about the signal.
 * 3. Registers the signal handler for SIGUSR1 using the `sigaction` system call.
 * 4. Raises the SIGUSR1 signal, triggering the signal handler and saving the context.
 * 5. Restores the default signal handler for SIGUSR1 after the context has been saved.
 *
 * This method is typically used in scenarios where a valid context, including segment registers,
 * is required for proper signal handling or context restoration.
 */
static void save_example_context() {
  // getcontext() does not save segment registers. In the signal
  // handler, restoring a context previously saved by getcontext()
  // will trigger segmentation fault because of the invalid segment
  // registers. So we save the example context during signal handling
  // to get a context with everything valid.
  struct sigaction s;
  void *(*memset_libc)(void *, int, size_t) = dlsym(RTLD_NEXT, "memset");
  memset_libc(&s, 0, sizeof(s));
  s.sa_sigaction = save_context_handler;
  s.sa_flags = SA_SIGINFO;
  int ret = sigaction(SIGUSR1, &s, NULL);
  assert(ret == 0);

  raise(SIGUSR1);

  s.sa_flags = 0;
  s.sa_handler = SIG_DFL;
  ret = sigaction(SIGUSR1, &s, NULL);
  assert(ret == 0);
}

/**
 * Sets up an alternate signal stack for the current CPU.
 * 
 * This function configures an alternate stack for handling signals, which is useful
 * in scenarios where the default stack may be insufficient or unavailable (e.g., in
 * low-memory conditions or when handling stack overflow signals). The alternate stack
 * is allocated from the `sigstack` field of the `thiscpu` structure.
 * 
 * The function ensures that the size of the provided stack is at least `SIGSTKSZ`, the
 * minimum size required for an alternate signal stack as defined by the system. It then
 * initializes a `stack_t` structure with the stack pointer, size, and flags, and calls
 * `sigaltstack` to set the alternate stack. The function asserts that the `sigaltstack`
 * call succeeds (returns 0).
 * 
 * @note This function assumes that `thiscpu->sigstack` is a valid memory region of at
 *       least `SIGSTKSZ` bytes.
 * @note The function terminates the program with an assertion failure if the stack size
 *       is insufficient or if the `sigaltstack` call fails.
 */
static void setup_sigaltstack() {
  assert(sizeof(thiscpu->sigstack) >= SIGSTKSZ);
  stack_t ss;
  ss.ss_sp = thiscpu->sigstack;
  ss.ss_size = sizeof(thiscpu->sigstack);
  ss.ss_flags = 0;
  int ret = sigaltstack(&ss, NULL);
  assert(ret == 0);
}

int main(const char *args);

static void init_platform() __attribute__((constructor));
/**
 * Initializes the platform by setting up memory mappings, CPU structures, and signal handling.
 * This function performs the following tasks:
 * 1. Creates a memory object using `memfd_create` and maps it to simulate physical memory.
 * 2. Dynamically links to `ftruncate` to resize the memory object to the specified size.
 * 3. Maps the memory object to a fixed address using `mmap`.
 * 4. Allocates a private per-CPU structure and initializes its fields.
 * 5. Creates a trap page to handle syscalls and yield signals via `SIGSEGV`.
 * 6. Saves the address of `memcpy` from glibc to avoid conflicts with klib.
 * 7. Remaps writable sections (data and bss) as `MAP_SHARED` to ensure they are shared across `fork()`.
 * 8. Sets up the AM heap within the simulated physical memory.
 * 9. Initializes the signal mask for interrupts and sets up an alternative signal stack.
 * 10. Saves a context template for future use and disables interrupts by default.
 * 11. Configures the number of CPUs and page size based on environment variables.
 * 12. Sets `stdout` to unbuffered mode.
 * 13. Calls the `main` function with arguments from the environment, or an empty string if no arguments are provided.
 */
static void init_platform() {
  // create memory object and set up mapping to simulate the physical memory
  pmem_fd = memfd_create("pmem", 0);
  assert(pmem_fd != -1);
  // use dynamic linking to avoid linking to the same function in RT-Thread
  int (*ftruncate_libc)(int, off_t) = dlsym(RTLD_NEXT, "ftruncate");
  assert(ftruncate_libc != NULL);
  int ret2 = ftruncate_libc(pmem_fd, PMEM_SIZE);
  assert(ret2 == 0);

  pmem = mmap(PMEM_START, PMEM_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
      MAP_SHARED | MAP_FIXED, pmem_fd, 0);
  assert(pmem != (void *)-1);

  // allocate private per-cpu structure
  thiscpu = mmap(NULL, sizeof(*thiscpu), PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(thiscpu != (void *)-1);
  thiscpu->cpuid = 0;
  thiscpu->vm_head = NULL;

  // create trap page to receive syscall and yield by SIGSEGV
  int sys_pgsz = sysconf(_SC_PAGESIZE);
  void *ret = mmap(TRAP_PAGE_START, sys_pgsz, PROT_NONE,
      MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  assert(ret != (void *)-1);

  // save the address of memcpy() in glibc, since it may be linked with klib
  memcpy_libc = dlsym(RTLD_NEXT, "memcpy");
  assert(memcpy_libc != NULL);

  // remap writable sections as MAP_SHARED
  Elf64_Phdr *phdr = (void *)getauxval(AT_PHDR);
  int phnum = (int)getauxval(AT_PHNUM);
  int i;
  for (i = 0; i < phnum; i ++) {
    if (phdr[i].p_type == PT_LOAD && (phdr[i].p_flags & PF_W)) {
      // allocate temporary memory
      extern char end;
      void *vaddr = (void *)&end - phdr[i].p_memsz;
      uintptr_t pad = (uintptr_t)vaddr & 0xfff;
      void *vaddr_align = vaddr - pad;
      uintptr_t size = phdr[i].p_memsz + pad;
      void *temp_mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      assert(temp_mem != (void *)-1);

      // save data and bss sections
      memcpy_libc(temp_mem, vaddr_align, size);

      // save the address of mmap() which will be used after munamp(),
      // since calling the library functions requires accessing GOT, which will be unmapped
      void *(*mmap_libc)(void *, size_t, int, int, int, off_t) = dlsym(RTLD_NEXT, "mmap");
      assert(mmap_libc != NULL);
      // load the address of memcpy() on stack, which can still be accessed
      // after the data section is unmapped
      void *(*volatile memcpy_libc_temp)(void *, const void *, size_t) = memcpy_libc;

      // unmap the data and bss sections
      ret2 = munmap(vaddr_align, size);
      assert(ret2 == 0);

      // map the sections again with MAP_SHARED, which will be shared across fork()
      ret = mmap_libc(vaddr_align, size, PROT_READ | PROT_WRITE | PROT_EXEC,
          MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
      assert(ret == vaddr_align);

      // restore the data in the sections
      memcpy_libc_temp(vaddr_align, temp_mem, size);

      // unmap the temporary memory
      ret2 = munmap(temp_mem, size);
      assert(ret2 == 0);
    }
  }

  // set up the AM heap
  heap = RANGE(pmem, pmem + PMEM_SIZE);

  // initialize sigmask for interrupts
  ret2 = sigemptyset(&__am_intr_sigmask);
  assert(ret2 == 0);
  ret2 = sigaddset(&__am_intr_sigmask, SIGVTALRM);
  assert(ret2 == 0);
  ret2 = sigaddset(&__am_intr_sigmask, SIGUSR1);
  assert(ret2 == 0);

  // setup alternative signal stack
  setup_sigaltstack();

  // save the context template
  save_example_context();
#ifdef __x86_64__
  uc_example.uc_mcontext.fpregs = NULL; // clear the FPU context
#endif
  __am_get_intr_sigmask(&uc_example.uc_sigmask);

  // disable interrupts by default
  iset(0);

  // set ncpu
  const char *smp = getenv("smp");
  __am_ncpu = smp ? atoi(smp) : 1;
  assert(0 < __am_ncpu && __am_ncpu <= MAX_CPU);

  // set pgsize
  const char *pgsize = getenv("pgsize");
  __am_pgsize = pgsize ? atoi(pgsize) : sys_pgsz;
  assert(__am_pgsize > 0 && __am_pgsize % sys_pgsz == 0);

  // set stdout unbuffered
  setbuf(stdout, NULL);

  const char *args = getenv("mainargs");
  halt(main(args ? args : "")); // call main here!
}

/**
 * @brief Terminates the platform-specific execution and cleans up resources.
 *
 * This function is responsible for exiting the platform-specific execution context.
 * It ensures that all resources are properly cleaned up, especially in a multi-processing
 * environment. If the platform is initialized in a multi-processing environment (MPE) and
 * there are multiple CPUs active, it sends a SIGKILL signal to all processes in the process
 * group to terminate them. Finally, it calls the standard `exit` function to terminate the
 * program with the specified exit code.
 *
 * @param code The exit code to be returned to the operating system upon termination.
 */
void __am_exit_platform(int code) {
  // let Linux clean up other resource
  extern int __am_mpe_init;
  if (__am_mpe_init && cpu_count() > 1) kill(0, SIGKILL);
  exit(code);
}

/**
 * Maps a virtual address to a physical address with specified protection flags.
 * 
 * This function translates the provided abstract machine (AM) protection flags 
 * to the corresponding `mmap` protection flags and maps the virtual address `va` 
 * to the physical address `pa` using the `mmap` system call. The mapping is done 
 * with the specified protection flags and ensures that the mapping is fixed at 
 * the provided virtual address. 
 * 
 * The function does not support the executable bit directly, so it marks all 
 * readable pages as executable as well. The mapping is performed with the 
 * `MAP_SHARED` and `MAP_FIXED` flags, ensuring that the mapping is shared and 
 * fixed at the specified virtual address.
 * 
 * @param va The virtual address to map.
 * @param pa The physical address to map to.
 * @param prot The protection flags in AM format, which will be translated to `mmap` flags.
 * 
 * @note The function asserts that the `mmap` call succeeds. If `mmap` fails, the 
 *       program will terminate with an assertion failure.
 */
void __am_pmem_map(void *va, void *pa, int prot) {
  // translate AM prot to mmap prot
  int mmap_prot = PROT_NONE;
  // we do not support executable bit, so mark
  // all readable pages executable as well
  if (prot & MMAP_READ) mmap_prot |= PROT_READ | PROT_EXEC;
  if (prot & MMAP_WRITE) mmap_prot |= PROT_WRITE;
  void *ret = mmap(va, __am_pgsize, mmap_prot,
      MAP_SHARED | MAP_FIXED, pmem_fd, (uintptr_t)(pa - pmem));
  assert(ret != (void *)-1);
}

/**
 * Unmaps a memory region previously mapped with `mmap`.
 *
 * This function unmaps the memory region starting at the virtual address `va`
 * with a size equal to the page size (`__am_pgsize`). It uses the `munmap`
 * system call to perform the unmapping. If the unmapping fails, the function
 * asserts and terminates the program.
 *
 * @param va The starting virtual address of the memory region to unmap.
 *           This address must be page-aligned.
 *
 * @note The function assumes that `__am_pgsize` is set to the correct page size
 *       and that `va` is a valid address previously returned by `mmap`.
 */
void __am_pmem_unmap(void *va) {
  int ret = munmap(va, __am_pgsize);
  assert(ret == 0);
}

/**
 * Copies the contents of the example user context (`uc_example`) into the 
 * user context of the provided `Context` structure (`r->uc`).
 *
 * This function uses the `memcpy_libc` function to perform the copy operation,
 * ensuring that the entire structure `uc_example` is copied into `r->uc`.
 * The size of the copy operation is determined by `sizeof(uc_example)`.
 *
 * @param r Pointer to the `Context` structure where the user context will be updated.
 */
void __am_get_example_uc(Context *r) {
  memcpy_libc(&r->uc, &uc_example, sizeof(uc_example));
}

/**
 * Copies the interrupt signal mask to the provided sigset_t structure.
 *
 * This function is used to retrieve the current interrupt signal mask and store
 * it in the memory location pointed to by the parameter `s`. The signal mask
 * is copied from the internal variable `__am_intr_sigmask`, which represents
 * the set of signals that are currently blocked or ignored by the system.
 *
 * @param s A pointer to a sigset_t structure where the interrupt signal mask
 *          will be stored. The structure must be pre-allocated by the caller.
 *
 * @note The function uses `memcpy_libc` to perform the copy operation, ensuring
 *       that the signal mask is accurately duplicated into the provided structure.
 */
void __am_get_intr_sigmask(sigset_t *s) {
  memcpy_libc(s, &__am_intr_sigmask, sizeof(__am_intr_sigmask));
}

/**
 * Checks whether the SIGVTALRM signal is not a member of the specified signal set.
 *
 * This function determines if the SIGVTALRM signal is not included in the signal set
 * pointed to by `s`. It uses the `sigismember` function to check the membership of
 * SIGVTALRM in the set. If SIGVTALRM is not a member of the set, the function returns
 * a non-zero value (true); otherwise, it returns 0 (false).
 *
 * @param s Pointer to a signal set of type `sigset_t` to be checked.
 * @return Returns 1 if SIGVTALRM is not a member of the signal set, otherwise returns 0.
 */
int __am_is_sigmask_sti(sigset_t *s) {
  return !sigismember(s, SIGVTALRM);
}

/**
 * Sends a SIGUSR1 signal to the current process to simulate a keyboard interrupt.
 * This function uses the `kill` system call to send the SIGUSR1 signal to the
 * process identified by its own process ID (PID), obtained via `getpid()`.
 * This can be used to trigger specific behavior in the process that handles
 * the SIGUSR1 signal, such as interrupting a task or updating a state.
 */
void __am_send_kbd_intr() {
  kill(getpid(), SIGUSR1);
}

/**
 * Protects the physical memory region by setting its protection to `PROT_NONE`,
 * which prevents any access (read, write, or execute) to the memory region.
 * This function uses the `mprotect` system call to apply the protection.
 * The memory region is defined by `PMEM_START` (the starting address) and
 * `PMEM_SIZE` (the size of the region). If the `mprotect` call fails, the
 * program will terminate with an assertion error.
 *
 * Note: The actual `mprotect` call and assertion are currently commented out,
 * so this function does not perform any action in its current state.
 */
void __am_pmem_protect() {
//  int ret = mprotect(PMEM_START, PMEM_SIZE, PROT_NONE);
//  assert(ret == 0);
}

/**
 * Unprotects a region of memory by setting the protection flags to allow read, write, and execute operations.
 * This method is intended to modify the memory protection of a specific memory range, typically starting at
 * `PMEM_START` and spanning `PMEM_SIZE` bytes. The protection flags are set to `PROT_READ | PROT_WRITE | PROT_EXEC`,
 * which allows the memory to be read from, written to, and executed as code. 
 * 
 * The method internally uses the `mprotect` system call to apply the new protection flags. If the call fails,
 * an assertion will trigger, indicating a critical error in the memory protection modification process.
 * 
 * Note: The actual implementation of `mprotect` is currently commented out, so this method does not perform
 * any operation when executed. Uncomment the `mprotect` call to enable the functionality.
 */
void __am_pmem_unprotect() {
//  int ret = mprotect(PMEM_START, PMEM_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC);
//  assert(ret == 0);
}

// This dummy function will be called in trm.c.
// The purpose of this dummy function is to let linker add this file to the object
// file set. Without it, the constructor of @_init_platform will not be linked.
void __am_platform_dummy() {
}
