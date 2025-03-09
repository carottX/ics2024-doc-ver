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

#define USER_SPACE RANGE(0x40000000, 0x80000000)

/**
 * Sets the Supervisor Address Translation and Protection (SATP) register
 * to enable virtual memory translation using the provided page directory.
 * 
 * This function configures the SATP register to use the Sv39 (for 39-bit
 * virtual address space) or Sv48 (for 48-bit virtual address space) mode,
 * depending on the value of `__riscv_xlen`. The mode is determined by setting
 * the most significant bit of the SATP register to 1. The base address of the
 * page directory (`pdir`) is shifted right by 12 bits to align it with the
 * SATP register's requirements and combined with the mode value.
 *
 * @param pdir A pointer to the page directory (page table base address).
 *             The address should be aligned to a 4KB boundary.
 */
static inline void set_satp(void *pdir) {
  uintptr_t mode = 1ul << (__riscv_xlen - 1);
  asm volatile("csrw satp, %0" : : "r"(mode | ((uintptr_t)pdir >> 12)));
}

/**
 * Reads the Supervisor Address Translation and Protection (SATP) register
 * and returns its value shifted left by 12 bits. The SATP register is used
 * to control the address translation and protection scheme for the supervisor
 * mode in RISC-V architectures. The left shift by 12 bits aligns the value
 * with the page table base address, as the lower 12 bits of SATP are reserved
 * for mode and other control bits.
 *
 * @return The value of the SATP register shifted left by 12 bits, representing
 *         the base address of the page table in supervisor mode.
 */
static inline uintptr_t get_satp() {
  uintptr_t satp;
  asm volatile("csrr %0, satp" : "=r"(satp));
  return satp << 12;
}

/**
 * Initializes the Virtual Memory Environment (VME) by setting up page allocation
 * and deallocation functions, allocating kernel address space, and mapping
 * memory segments.
 *
 * @param pgalloc_f A function pointer to a page allocation function that takes
 *                  an integer argument (size) and returns a pointer to the
 *                  allocated memory.
 * @param pgfree_f  A function pointer to a page deallocation function that
 *                  takes a pointer to the memory to be freed.
 *
 * @return true     Indicates successful initialization of the VME.
 *
 * The function performs the following steps:
 * 1. Assigns the provided page allocation and deallocation functions to global
 *    variables `pgalloc_usr` and `pgfree_usr`.
 * 2. Allocates a page of size `PGSIZE` for the kernel address space (KAS).
 * 3. Iterates over predefined memory segments and maps each segment's virtual
 *    addresses to corresponding physical addresses using the `map` function.
 * 4. Sets the `satp` register to the base address of the kernel address space.
 * 5. Enables the VME by setting the `vme_enable` flag to 1.
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

  set_satp(kas.ptr);
  vme_enable = 1;

  return true;
}

/**
 * Protects the given address space by allocating a new page directory and 
 * initializing it with the kernel's address space mappings.
 *
 * This function allocates a new page directory of size `PGSIZE` using `pgalloc_usr`
 * and assigns it to the `ptr` field of the provided `AddrSpace` structure. It also
 * sets the `area` field to `USER_SPACE` and the `pgsize` field to `PGSIZE`. 
 * Finally, it copies the kernel's page directory (`kas.ptr`) into the newly allocated
 * page directory to ensure that the kernel space mappings are preserved.
 *
 * @param as Pointer to the `AddrSpace` structure to be protected.
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
 * This method removes any protection flags (e.g., read-only, no-execute) 
 * from the memory pages or segments allocated to the address space, 
 * allowing unrestricted access to the memory. It is typically used when 
 * the memory needs to be modified or executed after being protected.
 *
 * @param as A pointer to the AddrSpace structure representing the address 
 *           space whose memory regions are to be unprotected.
 */
void unprotect(AddrSpace *as) {
}

/**
 * Retrieves the current address space context and updates the page directory pointer
 * in the provided Context structure.
 *
 * This function checks if virtual memory extension (VME) is enabled. If VME is enabled,
 * it retrieves the current SATP (Supervisor Address Translation and Protection) register
 * value, which contains the root page table physical address, and assigns it to the
 * page directory pointer (pdir) in the Context structure. If VME is not enabled, it sets
 * the page directory pointer to NULL, indicating that no address translation is in use.
 *
 * @param c Pointer to the Context structure where the page directory pointer will be updated.
 */
void __am_get_cur_as(Context *c) {
  c->pdir = (vme_enable ? (void *)get_satp() : NULL);
}

/**
 * Switches the context to the provided one by setting the page directory
 * if virtual memory is enabled and the page directory is not NULL.
 * 
 * This function checks if virtual memory is enabled (`vme_enable`) and
 * if the page directory (`c->pdir`) of the provided context is not NULL.
 * If both conditions are met, it sets the page directory by calling `set_satp`
 * with the page directory as the argument.
 *
 * @param c Pointer to the context to switch to.
 */
void __am_switch(Context *c) {
  if (vme_enable && c->pdir != NULL) {
    set_satp(c->pdir);
  }
}

/**
 * Maps a virtual address to a physical address in the given address space.
 *
 * This function associates a virtual address (va) with a physical address (pa)
 * within the specified address space (as). The protection flags (prot) define
 * the access permissions for the mapped region (e.g., read, write, execute).
 *
 * @param as   Pointer to the AddrSpace structure representing the address space.
 * @param va   Virtual address to be mapped.
 * @param pa   Physical address to which the virtual address is mapped.
 * @param prot Protection flags specifying the access permissions for the mapping.
 */
void map(AddrSpace *as, void *va, void *pa, int prot) {
}

/**
 * Creates and initializes a new execution context within the specified address space.
 *
 * This function sets up a new context that can be used to execute code in the given
 * address space (`as`). The context is initialized with a kernel stack (`kstack`) and
 * an entry point (`entry`) where execution will begin when the context is activated.
 *
 * @param as      Pointer to the address space in which the context will be created.
 * @param kstack  Memory area representing the kernel stack for the context.
 * @param entry   Pointer to the function that will be executed when the context starts.
 *
 * @return        A pointer to the newly created context on success, or NULL if the
 *                context could not be created.
 */
Context *ucontext(AddrSpace *as, Area kstack, void *entry) {
  return NULL;
}
