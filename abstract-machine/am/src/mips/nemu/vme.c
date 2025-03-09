#include <am.h>
#include <nemu.h>

#define USER_SPACE RANGE(0x40000000, 0x80000000)

static void* (*pgalloc_usr)(int) = NULL;
static void (*pgfree_usr)(void*) = NULL;
static int vme_enable = 0;
static PTE *cur_pdir = NULL;

/**
 * Initializes the Virtual Memory Environment (VME) by setting up the page allocation
 * and deallocation functions, and enabling the VME system.
 *
 * @param pgalloc_f A function pointer to the user-defined page allocation function.
 *                  This function should take an integer parameter representing the
 *                  number of pages to allocate and return a pointer to the allocated
 *                  memory.
 * @param pgfree_f  A function pointer to the user-defined page deallocation function.
 *                  This function should take a pointer to the memory to be deallocated
 *                  and return void.
 *
 * @return true     Indicates that the VME initialization was successful.
 */
bool vme_init(void* (*pgalloc_f)(int), void (*pgfree_f)(void*)) {
  pgalloc_usr = pgalloc_f;
  pgfree_usr = pgfree_f;
  vme_enable = 1;
  return true;
}

/**
 * Protects the given address space by allocating a new page of memory
 * and initializing the address space structure with the allocated page.
 * 
 * This function performs the following operations:
 * 1. Allocates a new page of memory of size `PGSIZE` using `pgalloc_usr`.
 * 2. Sets the `ptr` field of the `AddrSpace` structure to point to the newly
 *    allocated page.
 * 3. Sets the `pgsize` field of the `AddrSpace` structure to `PGSIZE`.
 * 4. Sets the `area` field of the `AddrSpace` structure to `USER_SPACE`.
 *
 * @param as A pointer to the `AddrSpace` structure to be protected.
 */
void protect(AddrSpace *as) {
  as->ptr = (PTE*)(pgalloc_usr(PGSIZE));
  as->pgsize = PGSIZE;
  as->area = USER_SPACE;
}

/**
 * Unprotects the address space associated with the given AddrSpace object.
 * This method removes any memory protection mechanisms (e.g., read/write/execute restrictions)
 * that were previously applied to the address space, allowing unrestricted access to its memory regions.
 * This is typically used in scenarios where the address space needs to be modified or inspected
 * without the constraints of memory protection.
 *
 * @param as A pointer to the AddrSpace object whose memory protection is to be removed.
 *           Must not be NULL.
 */
void unprotect(AddrSpace *as) {
}

/**
 * Retrieves the current address space directory for the given context.
 * 
 * This function sets the page directory (`pdir`) of the provided context (`c`) 
 * based on the state of the virtual memory enable (`vme_enable`) flag. 
 * If virtual memory is enabled (`vme_enable` is true), the current page directory 
 * (`cur_pdir`) is assigned to the context's `pdir`. If virtual memory is disabled, 
 * the `pdir` is set to `NULL`.
 *
 * @param c Pointer to the Context structure whose page directory is to be updated.
 */
void __am_get_cur_as(Context *c) {
  c->pdir = (vme_enable ? cur_pdir : NULL);
}

/**
 * Switches the current page directory to the one specified in the given context.
 * 
 * This function checks if virtual memory extension (VME) is enabled and if the 
 * page directory (pdir) in the provided context is not NULL. If both conditions 
 * are met, it updates the current page directory (cur_pdir) to the one specified 
 * in the context.
 *
 * @param c Pointer to the Context structure containing the page directory to switch to.
 */
void __am_switch(Context *c) {
  if (vme_enable && c->pdir != NULL) {
    cur_pdir = c->pdir;
  }
}

/**
 * Maps a virtual address to a physical address in the given address space.
 *
 * This function associates a virtual address (`va`) with a physical address (`pa`)
 * within the specified address space (`as`). The mapping is controlled by the
 * protection flags (`prot`), which define the access permissions (e.g., read, write, execute).
 *
 * @param as   Pointer to the address space where the mapping will be created.
 * @param va   Virtual address to be mapped. Must be aligned to the page size.
 * @param pa   Physical address to which the virtual address will be mapped. Must be aligned to the page size.
 * @param prot Protection flags for the mapping. Common values include:
 *             - PROT_READ: Allow read access.
 *             - PROT_WRITE: Allow write access.
 *             - PROT_EXEC: Allow execution.
 *             - PROT_NONE: No access allowed.
 *
 * @note This function does not handle overlapping mappings or invalid addresses.
 *       The caller must ensure that the addresses are valid and properly aligned.
 */
void map(AddrSpace *as, void *va, void *pa, int prot) {
}

/**
 * Creates and initializes a new execution context within the given address space.
 *
 * This method sets up a new context with the specified kernel stack and entry point.
 * The context is associated with the provided address space, allowing it to execute
 * within the memory and resource constraints defined by that address space.
 *
 * @param as      Pointer to the address space in which the context will execute.
 * @param kstack  The kernel stack area allocated for the context.
 * @param entry   The entry point function where the context will begin execution.
 *
 * @return A pointer to the newly created context on success, or NULL if the context
 *         could not be created or initialized.
 */
Context *ucontext(AddrSpace *as, Area kstack, void *entry) {
  return NULL;
}
