#include <am.h>

/**
 * Initializes the Virtual Memory Environment (VME) with the provided page allocation and deallocation functions.
 *
 * This function sets up the virtual memory environment by registering the page allocation and deallocation
 * functions that will be used to manage physical memory pages. The `pgalloc_f` function is responsible for
 * allocating a physical page of memory, while `pgfree_f` is used to free a previously allocated page.
 *
 * @param pgalloc_f A function pointer to the page allocation function. This function takes an integer parameter
 *                  representing the number of pages to allocate and returns a pointer to the allocated memory.
 * @param pgfree_f  A function pointer to the page deallocation function. This function takes a pointer to the
 *                  memory to be freed and returns void.
 *
 * @return Returns `true` if the VME was successfully initialized, otherwise returns `false`.
 */
bool vme_init(void* (*pgalloc_f)(int), void (*pgfree_f)(void*)) {
  return false;
}

/**
 * Protects the address space of a process by applying necessary security measures.
 * This method ensures that the address space specified by the `as` parameter is
 * safeguarded against unauthorized access or modifications. It may involve setting
 * up memory protection mechanisms, such as marking certain memory regions as
 * read-only, enforcing access control policies, or isolating the address space
 * from other processes. The specific protection mechanisms depend on the
 * underlying system architecture and security requirements.
 *
 * @param as A pointer to the `AddrSpace` structure representing the address space
 *           to be protected. This structure typically contains information about
 *           the memory layout, permissions, and other attributes of the address space.
 */
void protect(AddrSpace *as) {
}

/**
 * Unprotects the memory regions associated with the given address space.
 * This method removes any memory protection mechanisms (e.g., read-only, 
 * write-only, or execute-only permissions) that were previously applied 
 * to the memory regions of the address space. After calling this method, 
 * the memory regions will have unrestricted access, allowing read, write, 
 * and execute operations as needed.
 *
 * @param as A pointer to the AddrSpace structure representing the address 
 *           space whose memory regions are to be unprotected.
 */
void unprotect(AddrSpace *as) {
}

/**
 * Maps a virtual address (va) to a physical address (pa) within the given address space (as).
 * The mapping is created with the specified protection flags (prot) that define the access
 * permissions for the mapped region (e.g., read, write, execute).
 *
 * @param as   Pointer to the AddrSpace structure representing the address space where the
 *             mapping will be created.
 * @param va   Pointer to the virtual address that will be mapped.
 * @param pa   Pointer to the physical address to which the virtual address will be mapped.
 * @param prot Protection flags specifying the access permissions for the mapped region.
 *             Common flags include PROT_READ, PROT_WRITE, and PROT_EXEC.
 */
void map(AddrSpace *as, void *va, void *pa, int prot) {
}

/**
 * Creates a new execution context for a user-level process.
 *
 * This function initializes a new context for a user-level process within the provided
 * address space. The context includes a dedicated kernel stack and an entry point where
 * execution will begin. The function is typically used during process creation or context
 * switching.
 *
 * @param as      The address space in which the context will operate. This defines the
 *                memory layout and permissions for the process.
 * @param kstack  The kernel stack area allocated for the context. This stack is used
 *                during system calls and interrupts.
 * @param entry   The entry point for the context. This is the starting address where
 *                execution will begin when the context is activated.
 *
 * @return        A pointer to the newly created context. If the context cannot be created,
 *                the function returns NULL.
 */
Context *ucontext(AddrSpace *as, Area kstack, void *entry) {
  return NULL;
}
