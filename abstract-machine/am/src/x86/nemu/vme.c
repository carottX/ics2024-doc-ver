#include <am.h>
#include <nemu.h>
#include <klib.h>

static AddrSpace kas = {};
static void* (*pgalloc_usr)(int) = NULL;
static void (*pgfree_usr)(void*) = NULL;
static int vme_enable = 0;

static Area segments[] = {      // Kernel memory mappings
  NEMU_PADDR_SPACE
};

#define USER_SPACE RANGE(0x40000000, 0xc0000000)

/**
 * Initializes the Virtual Memory Environment (VME) by setting up page allocation and
 * deallocation functions, creating a kernel address space (KAS), and mapping memory segments.
 *
 * @param pgalloc_f A function pointer to a page allocation function that takes an integer
 *                  parameter (size) and returns a pointer to the allocated memory.
 * @param pgfree_f  A function pointer to a page deallocation function that takes a pointer
 *                  to the memory to be freed and returns void.
 *
 * @return Returns `true` upon successful initialization of the VME.
 *
 * The method performs the following steps:
 * 1. Assigns the provided page allocation and deallocation functions to global variables
 *    `pgalloc_usr` and `pgfree_usr`.
 * 2. Allocates a page for the kernel address space (KAS) using the provided page allocation
 *    function.
 * 3. Iterates over predefined memory segments and maps each virtual address to a physical
 *    address using the `map` function.
 * 4. Sets the CR3 register to point to the KAS page directory.
 * 5. Enables paging by setting the PG bit in the CR0 register.
 * 6. Sets the `vme_enable` flag to indicate that the VME is active.
 */
bool vme_init(void* (*pgalloc_f)(int), void (*pgfree_f)(void*)) {
  pgalloc_usr = pgalloc_f;
  pgfree_usr = pgfree_f;

  kas.ptr = pgalloc_f(PGSIZE);

  int i;
  for (i = 0; i < LENGTH(segments); i ++) {
    void *va = segments[i].start;
    for (; va < segments[i].end; va += PGSIZE) {
      map(&kas, va, va, 0);
    }
  }

  set_cr3(kas.ptr);
  set_cr0(get_cr0() | CR0_PG);
  vme_enable = 1;

  return true;
}

/**
 * Protects the given address space by initializing it with a user page directory
 * and mapping the kernel space into it. This function allocates a new page
 * directory for the user space, sets the address space properties, and copies
 * the kernel page directory entries into the user page directory to ensure
 * that the kernel space is properly mapped.
 *
 * @param as A pointer to the AddrSpace structure to be protected. This structure
 *           will be updated with the new page directory, user space area, and
 *           page size.
 *
 * The function performs the following steps:
 * 1. Allocates a new page directory for the user space using `pgalloc_usr`.
 * 2. Sets the `ptr` field of the address space to the newly allocated page directory.
 * 3. Sets the `area` field of the address space to `USER_SPACE`.
 * 4. Sets the `pgsize` field of the address space to `PGSIZE`.
 * 5. Copies the kernel page directory entries into the user page directory using `memcpy`.
 */
void protect(AddrSpace *as) {
  PTE *updir = (PTE*)(pgalloc_usr(PGSIZE));
  as->ptr = updir;
  as->area = USER_SPACE;
  as->pgsize = PGSIZE;
  // map kernel space
  memcpy(updir, kas.ptr, PGSIZE);
}

/**
 * Unprotects the memory regions associated with the given address space.
 * This method removes any memory protection mechanisms (e.g., read-only, write-only, or execute-only)
 * that were previously applied to the address space, allowing unrestricted access to its memory regions.
 * This is typically used when the address space needs to be modified or freed, ensuring that no
 * protection constraints prevent such operations.
 *
 * @param as A pointer to the AddrSpace structure representing the address space to unprotect.
 *           Must not be NULL.
 */
void unprotect(AddrSpace *as) {
}

/**
 * Retrieves the current address space context and updates the CR3 register value in the provided Context structure.
 * If virtual memory is enabled (vme_enable is true), the method calls get_cr3() to obtain the current CR3 register value
 * and assigns it to the cr3 field of the Context structure. If virtual memory is disabled, the cr3 field is set to NULL.
 *
 * @param c A pointer to the Context structure where the CR3 register value will be stored.
 */
void __am_get_cur_as(Context *c) {
  c->cr3 = (vme_enable ? (void *)get_cr3() : NULL);
}

/**
 * Switches the current execution context to the provided context `c`.
 * If virtual memory extension (VME) is enabled and the context's CR3 register
 * (page directory base register) is not NULL, this method updates the CR3
 * register to the value stored in the context. This ensures that the memory
 * mapping for the new context is correctly set up.
 *
 * @param c Pointer to the context to switch to.
 */
void __am_switch(Context *c) {
  if (vme_enable && c->cr3 != NULL) {
    set_cr3(c->cr3);
  }
}

/**
 * Maps a virtual address (va) to a physical address (pa) in the given address space (as)
 * with the specified protection flags (prot).
 *
 * This function is responsible for creating a mapping between a virtual address and a
 * physical address within the provided address space. The protection flags determine
 * the access permissions (e.g., read, write, execute) for the mapped region.
 *
 * @param as   Pointer to the AddrSpace structure representing the address space where
 *             the mapping will be created.
 * @param va   Pointer to the virtual address that will be mapped to the physical address.
 * @param pa   Pointer to the physical address that the virtual address will map to.
 * @param prot Protection flags specifying the access permissions for the mapped region.
 *             Common flags include PROT_READ, PROT_WRITE, and PROT_EXEC.
 */
void map(AddrSpace *as, void *va, void *pa, int prot) {
}

/**
 * Creates a new execution context within the specified address space.
 *
 * This function initializes a new context that can be used to execute code in the given
 * address space. The context is associated with a kernel stack and an entry point for
 * execution. The function returns a pointer to the newly created context.
 *
 * @param as      A pointer to the address space in which the context will be created.
 * @param kstack  The kernel stack area allocated for the context.
 * @param entry   The entry point function where execution will begin in the new context.
 *
 * @return        A pointer to the newly created context, or NULL if the context could
 *                not be created.
 */
Context* ucontext(AddrSpace *as, Area kstack, void *entry) {
  return NULL;
}
