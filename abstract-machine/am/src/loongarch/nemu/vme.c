#include <am.h>
#include <nemu.h>

#define USER_SPACE RANGE(0x40000000, 0x80000000)

static void* (*pgalloc_usr)(int) = NULL;
static void (*pgfree_usr)(void*) = NULL;
static int vme_enable = 0;
static PTE *cur_pdir = NULL;

/**
 * Initializes the Virtual Memory Environment (VME) by setting up the page allocation
 * and deallocation functions, and enabling the VME.
 *
 * This function assigns the provided page allocation and deallocation functions to
 * the internal pointers `pgalloc_usr` and `pgfree_usr`, respectively. It then enables
 * the VME by setting the `vme_enable` flag to 1.
 *
 * @param pgalloc_f A function pointer to a page allocation function that takes an
 *                  integer argument (presumably the number of pages to allocate) and
 *                  returns a pointer to the allocated memory.
 * @param pgfree_f  A function pointer to a page deallocation function that takes a
 *                  pointer to the memory to be deallocated and returns void.
 *
 * @return Always returns `true` to indicate successful initialization.
 */
bool vme_init(void* (*pgalloc_f)(int), void (*pgfree_f)(void*)) {
  pgalloc_usr = pgalloc_f;
  pgfree_usr = pgfree_f;
  vme_enable = 1;
  return true;
}

/**
 * Protects the given address space by initializing its page table entry (PTE),
 * page size, and memory area. This function allocates a new page of memory
 * of size PGSIZE for the address space's PTE and sets the page size to PGSIZE.
 * It also marks the address space as belonging to the USER_SPACE area.
 *
 * @param as A pointer to the AddrSpace structure to be protected. The structure
 *           must have a valid pointer to store the allocated PTE.
 *
 * @note This function assumes that pgalloc_usr is a valid function that allocates
 *       a page of memory of the specified size and returns a pointer to it.
 *       The PGSIZE and USER_SPACE constants must be defined appropriately.
 */
void protect(AddrSpace *as) {
  as->ptr = (PTE*)(pgalloc_usr(PGSIZE));
  as->pgsize = PGSIZE;
  as->area = USER_SPACE;
}

/**
 * Unprotects the given address space by removing any memory protection mechanisms
 * that have been applied to it. This method ensures that all memory regions within
 * the address space are accessible for read, write, and execute operations, effectively
 * disabling any restrictions that were previously enforced. This is typically used
 * when modifications to the memory layout or contents are required, and protection
 * is no longer necessary or desired.
 *
 * @param as A pointer to the AddrSpace structure representing the address space
 *           to be unprotected. Must not be NULL.
 */
void unprotect(AddrSpace *as) {
}

/**
 * Retrieves the current page directory for the given context.
 * 
 * This function sets the page directory (`pdir`) of the provided context (`c`) 
 * based on the state of the virtual memory enable flag (`vme_enable`). If virtual 
 * memory is enabled (`vme_enable` is true), the current page directory (`cur_pdir`) 
 * is assigned to the context's `pdir`. Otherwise, the `pdir` is set to `NULL`.
 *
 * @param c Pointer to the Context structure whose page directory is to be updated.
 */
void __am_get_cur_as(Context *c) {
  c->pdir = (vme_enable ? cur_pdir : NULL);
}

/**
 * Switches the current page directory to the one specified in the given context.
 * 
 * This function checks if the virtual memory extension (VME) is enabled and if the 
 * page directory pointer (`pdir`) in the provided context (`c`) is not NULL. If both 
 * conditions are met, it updates the global variable `cur_pdir` to point to the page 
 * directory specified in the context. This effectively switches the active page 
 * directory, which is used for translating virtual addresses to physical addresses.
 *
 * @param c Pointer to the context containing the new page directory to switch to.
 */
void __am_switch(Context *c) {
  if (vme_enable && c->pdir != NULL) {
    cur_pdir = c->pdir;
  }
}

/**
 * Maps a virtual address to a physical address in the given address space.
 *
 * This function associates the specified virtual address (`va`) with the
 * provided physical address (`pa`) in the address space (`as`). The mapping
 * is created with the specified protection flags (`prot`), which define the
 * access permissions (e.g., read, write, execute) for the mapped region.
 *
 * @param as   Pointer to the address space where the mapping will be created.
 * @param va   Virtual address to be mapped. Must be aligned to the page size.
 * @param pa   Physical address to which the virtual address will be mapped.
 *             Must be aligned to the page size.
 * @param prot Protection flags for the mapping. Possible values include:
 *             - PROT_READ: Allow read access.
 *             - PROT_WRITE: Allow write access.
 *             - PROT_EXEC: Allow execution.
 *             - PROT_NONE: No access allowed.
 *
 * @note The function assumes that the virtual and physical addresses are valid
 *       and properly aligned. It does not perform any validation checks.
 */
void map(AddrSpace *as, void *va, void *pa, int prot) {
}

/**
 * Creates a new execution context within the specified address space.
 * 
 * This function initializes a new context that can be used to execute code
 * in the provided address space. The context is associated with a kernel stack
 * and an entry point where execution will begin when the context is activated.
 * 
 * @param as      Pointer to the address space in which the context will execute.
 * @param kstack  The kernel stack area allocated for the context.
 * @param entry   The entry point function where execution will start.
 * 
 * @return        A pointer to the newly created context, or NULL if the
 *                context could not be created.
 */
Context *ucontext(AddrSpace *as, Area kstack, void *entry) {
  return NULL;
}
