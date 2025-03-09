#include "x86-qemu.h"

const struct mmu_config mmu = {
  .pgsize = 4096,
#if __x86_64__
  .ptlevels = 4,
  .pgtables = {
    { "CR3",  0x000000000000,  0,  0 },
    { "PML4", 0xff8000000000, 39,  9 },
    { "PDPT", 0x007fc0000000, 30,  9 },
    { "PD",   0x00003fe00000, 21,  9 },
    { "PT",   0x0000001ff000, 12,  9 },
  },
#else
  .ptlevels = 2,
  .pgtables = {
    { "CR3",      0x00000000,  0,  0 },
    { "PD",       0xffc00000, 22, 10 },
    { "PT",       0x003ff000, 12, 10 },
  },
#endif
};

static const struct vm_area vm_areas[] = {
#ifdef __x86_64__
  { RANGE(0x100000000000, 0x108000000000), 0 }, // 512 GiB user space
  { RANGE(0x000000000000, 0x008000000000), 1 }, // 512 GiB kernel
#else
  { RANGE(    0x40000000,     0x80000000), 0 }, // 1 GiB user space
  { RANGE(    0x00000000,     0x40000000), 1 }, // 1 GiB kernel
  { RANGE(    0xfd000000,     0x00000000), 1 }, // memory-mapped I/O
#endif
};
#define uvm_area (vm_areas[0].area)

static uintptr_t *kpt;
static void *(*pgalloc)(int size);
static void (*pgfree)(void *);

/**
 * Allocates a page of memory and initializes it to zero.
 *
 * This function allocates a page of memory of size `mmu.pgsize` using the `pgalloc` function.
 * If the allocation fails, the function panics with the message "cannot allocate page".
 * After successful allocation, the function initializes the entire page to zero by setting
 * each `uintptr_t`-sized element in the allocated memory to zero.
 *
 * @return A pointer to the base of the zero-initialized page of memory.
 *         The returned pointer is of type `void*`, allowing it to be cast to any other pointer type.
 */
static void *pgallocz() {
  uintptr_t *base = pgalloc(mmu.pgsize);
  panic_on(!base, "cannot allocate page");
  for (int i = 0; i < mmu.pgsize / sizeof(uintptr_t); i++) {
    base[i] = 0;
  }
  return base;
}

/**
 * Computes the index of a given address within a specified memory region.
 *
 * This function calculates the index of the address `addr` within the memory region
 * described by `info`. The index is determined by applying a bitmask and shifting
 * the address according to the `mask` and `shift` values provided in the `ptinfo`
 * structure.
 *
 * @param addr The address for which to compute the index.
 * @param info A pointer to a `ptinfo` structure containing the `mask` and `shift`
 *             values used to compute the index.
 *
 * @return The computed index as an integer.
 */
static int indexof(uintptr_t addr, const struct ptinfo *info) {
  return ((uintptr_t)addr & info->mask) >> info->shift;
}

/**
 * Returns the base address of the page containing the given virtual address.
 * 
 * This method calculates the base address of the memory page that contains the
 * specified virtual address `addr`. It does this by masking off the lower bits
 * of the address using the page size (`mmu.pgsize`), effectively aligning the
 * address to the nearest lower page boundary.
 * 
 * @param addr The virtual address whose page base address is to be determined.
 * @return The base address of the page containing `addr`.
 */
static uintptr_t baseof(uintptr_t addr) {
  return addr & ~(mmu.pgsize - 1);
}

/**
 * Walks the page table hierarchy for a given address in a specified address space.
 *
 * This function traverses the page table levels starting from the root page table
 * of the provided address space (`as`). It follows the page table entries (PTEs)
 * corresponding to the given virtual address (`addr`) until it reaches the final
 * level or finds an invalid PTE.
 *
 * If an invalid PTE (i.e., not present) is encountered at any level, a new page is
 * allocated and mapped with the specified `flags`. The function returns a pointer
 * to the PTE at the final level that corresponds to the given address.
 *
 * @param as    Pointer to the address space structure containing the root page table.
 * @param addr  The virtual address to traverse the page tables for.
 * @param flags The flags to set for newly allocated PTEs (e.g., permissions).
 *
 * @return A pointer to the PTE at the final level corresponding to `addr`. If the
 *         address is invalid or the traversal fails, the function calls `bug()`.
 */
static uintptr_t *ptwalk(AddrSpace *as, uintptr_t addr, int flags) {
  uintptr_t cur = (uintptr_t)&as->ptr;

  for (int i = 0; i <= mmu.ptlevels; i++) {
    const struct ptinfo *ptinfo = &mmu.pgtables[i];
    uintptr_t *pt = (uintptr_t *)cur, next_page;
    int index = indexof(addr, ptinfo);
    if (i == mmu.ptlevels) return &pt[index];

    if (!(pt[index] & PTE_P)) {
      next_page = (uintptr_t)pgallocz();
      pt[index] = next_page | PTE_P | flags;
    } else {
      next_page = baseof(pt[index]);
    }
    cur = next_page;
  }
  bug();
}

/**
 * Recursively tears down page tables starting from the specified level.
 *
 * This function traverses the page table hierarchy, starting at the given level,
 * and recursively frees all valid user-accessible page tables. It checks each entry
 * in the current page table to determine if it points to a valid user-accessible
 * page table at the next level. If so, it recursively calls itself to tear down
 * that page table. Once all child page tables have been freed, it frees the current
 * page table if it is not the root level (level 0).
 *
 * @param level The current level in the page table hierarchy to start tearing down.
 * @param pt    A pointer to the page table at the current level.
 */
static void teardown(int level, uintptr_t *pt) {
  if (level > mmu.ptlevels) return;
  for (int index = 0; index < (1 << mmu.pgtables[level].bits); index++) {
    if ((pt[index] & PTE_P) && (pt[index] & PTE_U)) {
      teardown(level + 1, (void *)baseof(pt[index]));
    }
  }
  if (level >= 1) {
    pgfree(pt);
  }
}

/**
 * Initializes the Virtual Memory Environment (VME) for the system.
 *
 * This function sets up the virtual memory environment by configuring the page
 * allocator and page deallocator functions, initializing the kernel page table,
 * and enabling paging. It ensures that the initialization is performed only on
 * the bootstrap CPU (CPU 0) and panics if called on any other CPU.
 *
 * On x86_64 systems, the function directly uses the PML4 address for the kernel
 * page table. On other architectures, it iterates over the predefined virtual
 * memory areas (VMAs) and sets up the page table entries (PTEs) for kernel
 * memory regions.
 *
 * After setting up the page table, the function updates the CR3 register to
 * point to the kernel page table and enables paging by setting the PG bit in
 * the CR0 register.
 *
 * @param _pgalloc A function pointer to the page allocator. This function
 *                 should allocate a page of the specified size.
 * @param _pgfree  A function pointer to the page deallocator. This function
 *                 should deallocate a previously allocated page.
 *
 * @return true if the initialization is successful. This function always
 *         returns true unless it panics due to being called on a non-bootstrap
 *         CPU.
 */
bool vme_init(void *(*_pgalloc)(int size), void (*_pgfree)(void *)) {
  panic_on(cpu_current() != 0, "init VME in non-bootstrap CPU");
  pgalloc = _pgalloc;
  pgfree  = _pgfree;

#if __x86_64__
  kpt = (void *)PML4_ADDR;
#else
  AddrSpace as;
  as.ptr = NULL;
  for (int i = 0; i < LENGTH(vm_areas); i++) {
    const struct vm_area *vma = &vm_areas[i];
    if (vma->kernel) {
      for (uintptr_t cur = (uintptr_t)vma->area.start;
           cur != (uintptr_t)vma->area.end;
           cur += mmu.pgsize) {
        *ptwalk(&as, cur, PTE_W) = cur | PTE_P | PTE_W;
      }
    }
  }
  kpt = (void *)baseof((uintptr_t)as.ptr);
#endif

  set_cr3(kpt);
  set_cr0(get_cr0() | CR0_PG);
  return true;
}

/**
 * Protects the address space of a given process by setting up page tables.
 * 
 * This function allocates a new page table using `pgallocz()` and initializes it
 * to protect the address space of the process represented by `as`. It iterates
 * over the virtual memory areas (`vm_areas`) and, for each kernel-mapped area,
 * copies the corresponding entries from the kernel page table (`kpt`) to the
 * newly allocated page table (`upt`). This ensures that the kernel's memory
 * mappings are preserved in the process's address space.
 *
 * The function then updates the `AddrSpace` structure (`as`) with the new page
 * table's properties, including the page size (`pgsize`), the user virtual
 * memory area (`uvm_area`), and the page table pointer with the appropriate
 * permissions (PTE_P and PTE_U).
 *
 * @param as Pointer to the `AddrSpace` structure representing the process's
 *           address space to be protected.
 */
void protect(AddrSpace *as) {
  uintptr_t *upt = pgallocz();

  for (int i = 0; i < LENGTH(vm_areas); i++) {
    const struct vm_area *vma = &vm_areas[i];
    if (vma->kernel) {
      const struct ptinfo *info = &mmu.pgtables[1]; // level-1 page table
      for (uintptr_t cur = (uintptr_t)vma->area.start;
           cur != (uintptr_t)vma->area.end;
           cur += (1L << info->shift)) {
        int index = indexof(cur, info);
        upt[index] = kpt[index];
      }
    }
  }
  as->pgsize = mmu.pgsize;
  as->area   = uvm_area;
  as->ptr    = (void *)((uintptr_t)upt | PTE_P | PTE_U);
}

/**
 * Unprotects the memory associated with the given address space.
 * This method invokes the `teardown` function to release or unmap the memory
 * region pointed to by the `ptr` field of the `AddrSpace` structure.
 * The `teardown` function is called with a flag of `0` and a pointer to the
 * `ptr` field, ensuring that the memory is properly deallocated or unmapped.
 *
 * @param as A pointer to the `AddrSpace` structure whose memory is to be unprotected.
 */
void unprotect(AddrSpace *as) {
  teardown(0, (void *)&as->ptr);
}

/**
 * Maps a virtual address to a physical address in the given address space with specified protection flags.
 *
 * This function ensures that the virtual address (va) and physical address (pa) are valid and aligned to the page boundary.
 * It then updates the page table entry (PTE) corresponding to the virtual address to point to the physical address.
 * The protection flags (prot) determine the access permissions for the mapped page.
 *
 * @param as    Pointer to the address space where the mapping will be applied.
 * @param va    Virtual address to be mapped. Must be within the valid user virtual memory range and aligned to the page boundary.
 * @param pa    Physical address to which the virtual address will be mapped. Must be aligned to the page boundary.
 * @param prot  Protection flags for the mapping. MMAP_NONE indicates unmapping, MMAP_WRITE allows write access.
 *
 * @note This function will panic if:
 *       - The virtual address is outside the valid user virtual memory range.
 *       - The virtual or physical address is not aligned to the page boundary.
 *       - An attempt is made to unmap a non-mapped page.
 *       - An attempt is made to remap an already mapped page.
 */
void map(AddrSpace *as, void *va, void *pa, int prot) {
  panic_on(!IN_RANGE(va, uvm_area), "mapping an invalid address");
  panic_on((uintptr_t)va != ROUNDDOWN(va, mmu.pgsize) ||
           (uintptr_t)pa != ROUNDDOWN(pa, mmu.pgsize), "non-page-boundary address");

  uintptr_t *ptentry = ptwalk(as, (uintptr_t)va, PTE_W | PTE_U);
  if (prot == MMAP_NONE) {
    panic_on(!(*ptentry & PTE_P), "unmapping a non-mapped page");
    *ptentry = 0;
  } else {
    panic_on(*ptentry & PTE_P, "remapping a mapped page");
    uintptr_t pte = (uintptr_t)pa | PTE_P | PTE_U | ((prot & MMAP_WRITE) ? PTE_W : 0);
    *ptentry = pte;
  }
  ptwalk(as, (uintptr_t)va, PTE_W | PTE_U);
}

/**
 * Creates and initializes a new execution context for a user-level process.
 * 
 * This function sets up a Context structure at the top of the provided kernel stack (`kstack`).
 * The context is initialized with default values, including segment selectors, instruction pointer,
 * stack pointer, and control registers. The context is tailored for the given address space (`as`)
 * and the entry point (`entry`) where execution should begin.
 *
 * On x86_64 architecture, the context includes the following:
 * - Code segment (`cs`) set to the user code segment selector.
 * - Stack segment (`ss`) set to the user data segment selector.
 * - Instruction pointer (`rip`) set to the provided entry point.
 * - Flags register (`rflags`) set to enable interrupts (`FL_IF`).
 * - Stack pointer (`rsp`) set to the end of the user virtual memory area (`uvm_area`).
 * - Kernel stack pointer (`rsp0`) set to the end of the provided kernel stack.
 *
 * On other architectures (e.g., x86), the context includes:
 * - Code segment (`cs`) set to the user code segment selector.
 * - Data segment (`ds`) set to the user data segment selector.
 * - Stack segment (`ss3`) set to the user data segment selector.
 * - Instruction pointer (`eip`) set to the provided entry point.
 * - Flags register (`eflags`) set to enable interrupts (`FL_IF`).
 * - Stack pointer (`esp`) set to the end of the user virtual memory area (`uvm_area`).
 * - Kernel stack pointer (`esp0`) set to the end of the provided kernel stack.
 *
 * The `cr3` register is set to the page table base address of the provided address space (`as`).
 *
 * @param as      The address space associated with the new context.
 * @param kstack  The kernel stack area where the context will be stored.
 * @param entry   The entry point where execution should begin in the new context.
 * @return        A pointer to the initialized Context structure.
 */
Context *ucontext(AddrSpace *as, Area kstack, void *entry) {
  Context *ctx = kstack.end - sizeof(Context);
  *ctx = (Context) { 0 };

#if __x86_64__
  ctx->cs     = USEL(SEG_UCODE);
  ctx->ss     = USEL(SEG_UDATA);
  ctx->rip    = (uintptr_t)entry;
  ctx->rflags = FL_IF;
  ctx->rsp    = (uintptr_t)uvm_area.end;
  ctx->rsp0   = (uintptr_t)kstack.end;
#else
  ctx->cs     = USEL(SEG_UCODE);
  ctx->ds     = USEL(SEG_UDATA);
  ctx->ss3    = USEL(SEG_UDATA);
  ctx->eip    = (uintptr_t)entry;
  ctx->eflags = FL_IF;
  ctx->esp    = (uintptr_t)uvm_area.end;
  ctx->esp0   = (uintptr_t)kstack.end;
#endif
  ctx->cr3 = as->ptr;

  return ctx;
}
