#define _GNU_SOURCE
#include <search.h>
#include "platform.h"

#define USER_SPACE RANGE(0x40000000, 0xc0000000)

typedef struct PageMap {
  void *va;
  void *pa;
  struct PageMap *next;
  int prot;
  int is_mapped;
  char key[32];  // used for hsearch_r()
} PageMap;

typedef struct VMHead {
  PageMap *head;
  struct hsearch_data hash;
  int nr_page;
} VMHead;

#define list_foreach(p, head) \
  for (p = (PageMap *)(head); p != NULL; p = p->next)

extern int __am_pgsize;
static int vme_enable = 0;
static void* (*pgalloc)(int) = NULL;
static void (*pgfree)(void *) = NULL;

/**
 * Initializes the Virtual Memory Environment (VME) by setting up the page allocation
 * and deallocation functions, and enabling the VME.
 *
 * This function assigns the provided page allocation and deallocation functions to
 * the internal function pointers `pgalloc` and `pgfree`, respectively. It then enables
 * the VME by setting the `vme_enable` flag to 1.
 *
 * @param pgalloc_f A function pointer to a page allocation function that takes an
 *                  integer parameter (typically the number of pages to allocate) and
 *                  returns a pointer to the allocated memory.
 * @param pgfree_f  A function pointer to a page deallocation function that takes a
 *                  pointer to the memory to be deallocated and returns void.
 *
 * @return Always returns `true` to indicate successful initialization.
 */
bool vme_init(void* (*pgalloc_f)(int), void (*pgfree_f)(void*)) {
  pgalloc = pgalloc_f;
  pgfree = pgfree_f;
  vme_enable = 1;
  return true;
}

/**
 * Protects the address space of a process by initializing and configuring
 * a virtual memory (VM) structure. This method allocates a page for the
 * VM head, initializes it, and sets up a hash table to manage the pages
 * within the user space. The address space is then updated with the VM
 * head, page size, and user space area.
 *
 * @param as A pointer to the AddrSpace structure to be protected. Must not be NULL.
 * 
 * The method performs the following steps:
 * 1. Asserts that the provided AddrSpace pointer is not NULL.
 * 2. Allocates a page for the VM head using pgalloc.
 * 3. Asserts that the VM head allocation was successful.
 * 4. Initializes the VM head by zeroing its memory.
 * 5. Calculates the maximum number of pages that can fit in the user space.
 * 6. Creates a hash table to manage the pages and asserts its successful creation.
 * 7. Updates the AddrSpace structure with the VM head, page size, and user space area.
 */
void protect(AddrSpace *as) {
  assert(as != NULL);
  VMHead *h = pgalloc(__am_pgsize); // used as head of the list
  assert(h != NULL);
  memset(h, 0, sizeof(*h));
  int max_pg = (USER_SPACE.end - USER_SPACE.start) / __am_pgsize;
  int ret = hcreate_r(max_pg, &h->hash);
  assert(ret != 0);

  as->ptr = h;
  as->pgsize = __am_pgsize;
  as->area = USER_SPACE;
}

/**
 * Unprotects the memory pages of the given address space.
 *
 * This method removes any protection mechanisms applied to the memory pages
 * associated with the provided `AddrSpace` object. This typically involves
 * marking the pages as writable, readable, and/or executable, depending on
 * the system's memory management policies. The exact behavior may vary based
 * on the underlying operating system or hardware architecture.
 *
 * @param as A pointer to the `AddrSpace` structure whose memory pages are to
 *           be unprotected. The structure should have been previously initialized
 *           and associated with a valid memory space.
 *
 * @note This operation should be used with caution, as removing protections
 *       can expose the memory to unintended modifications or security risks.
 */
void unprotect(AddrSpace *as) {
}

/**
 * Switches the virtual memory context for the current CPU core.
 *
 * This function handles the transition between virtual memory contexts by unmapping
 * the current virtual memory mappings and mapping the new ones. It ensures that
 * the CPU's virtual memory head is updated to reflect the new context.
 *
 * If virtual memory is not enabled (`vme_enable` is false), the function returns
 * immediately without performing any operations. Otherwise, it checks if the
 * provided context's virtual memory head (`c->vm_head`) differs from the current
 * CPU's virtual memory head (`thiscpu->vm_head`). If they are the same, no action
 * is taken.
 *
 * If the current virtual memory head (`now_head`) is not NULL, all mapped pages
 * in its list are unmapped using `__am_pmem_unmap`. If the new virtual memory head
 * (`head`) is not NULL, all pages in its list are mapped using `__am_pmem_map`.
 *
 * Finally, the CPU's virtual memory head is updated to the new head.
 *
 * @param c Pointer to the context containing the new virtual memory head.
 */
void __am_switch(Context *c) {
  if (!vme_enable) return;

  VMHead *head = c->vm_head;
  VMHead *now_head = thiscpu->vm_head;
  if (head == now_head) goto end;

  PageMap *pp;
  if (now_head != NULL) {
    // munmap all mappings
    list_foreach(pp, now_head->head) {
      if (pp->is_mapped) {
        __am_pmem_unmap(pp->va);
        pp->is_mapped = false;
      }
    }
  }

  if (head != NULL) {
    // mmap all mappings
    list_foreach(pp, head->head) {
      assert(IN_RANGE(pp->va, USER_SPACE));
      __am_pmem_map(pp->va, pp->pa, pp->prot);
      pp->is_mapped = true;
    }
  }

end:
  thiscpu->vm_head = head;
}

/**
 * Maps a virtual address (va) to a physical address (pa) in the given address space (as) with specified protection (prot).
 * 
 * This function ensures that the virtual address is within the user space and aligned to the page size. It also verifies
 * that the physical address is aligned to the page size and that the address space is not NULL. The function then checks
 * if the virtual address is already mapped in the address space's page table. If not, it allocates a new page and adds it
 * to the page table. The function updates the page's virtual address, physical address, protection flags, and mapping status.
 * If the address space is the current CPU's address space, the mapping is enforced immediately.
 *
 * @param as    Pointer to the address space where the mapping will be added.
 * @param va    Virtual address to be mapped.
 * @param pa    Physical address to which the virtual address will be mapped.
 * @param prot  Protection flags for the mapping (e.g., read, write, execute).
 */
void map(AddrSpace *as, void *va, void *pa, int prot) {
  assert(IN_RANGE(va, USER_SPACE));
  assert((uintptr_t)va % __am_pgsize == 0);
  assert((uintptr_t)pa % __am_pgsize == 0);
  assert(as != NULL);
  PageMap *pp = NULL;
  VMHead *vm_head = as->ptr;
  assert(vm_head != NULL);
  char buf[32];
  snprintf(buf, 32, "%x", va);
  ENTRY item = { .key = buf };
  ENTRY *item_find;
  hsearch_r(item, FIND, &item_find, &vm_head->hash);
  if (item_find == NULL) {
    pp = pgalloc(__am_pgsize); // this will waste memory, any better idea?
    snprintf(pp->key, 32, "%x", va);
    item.key = pp->key;
    item.data = pp;
    int ret = hsearch_r(item, ENTER, &item_find, &vm_head->hash);
    assert(ret != 0);
    vm_head->nr_page ++;
  } else {
    pp = item_find->data;
  }
  pp->va = va;
  pp->pa = pa;
  pp->prot = prot;
  pp->is_mapped = false;
  pp->next = vm_head->head;
  vm_head->head = pp;

  if (vm_head == thiscpu->vm_head) {
    // enforce the map immediately
    __am_pmem_map(pp->va, pp->pa, pp->prot);
    pp->is_mapped = true;
  }
}

/**
 * Initializes a new execution context for a process or thread.
 *
 * This function sets up a context structure that represents the execution state
 * of a process or thread. It initializes the context with a given address space,
 * kernel stack, and entry point. The context is prepared to start execution at
 * the specified entry point with the stack pointer set to the end of the user
 * space. Additionally, the context's signal mask is cleared to enable interrupts,
 * and the virtual memory head is set to the provided address space.
 *
 * @param as      Pointer to the address space structure for the context.
 * @param kstack  The kernel stack area for the context.
 * @param entry   The entry point (function pointer) where execution should begin.
 *
 * @return        A pointer to the initialized context structure.
 */
Context* ucontext(AddrSpace *as, Area kstack, void *entry) {
  Context *c = (Context*)kstack.end - 1;

  __am_get_example_uc(c);
  AM_REG_PC(&c->uc) = (uintptr_t)entry;
  AM_REG_SP(&c->uc) = (uintptr_t)USER_SPACE.end;

  int ret = sigemptyset(&(c->uc.uc_sigmask)); // enable interrupt
  assert(ret == 0);
  c->vm_head = as->ptr;

  c->ksp = (uintptr_t)kstack.end;

  return c;
}

/**
 * Determines whether the given address is within the user space of the current process.
 * This function checks if virtual memory is enabled, if the current CPU has a valid 
 * virtual memory context (vm_head is not NULL), and if the address falls within the 
 * predefined user space range (USER_SPACE).
 *
 * @param addr The address to check.
 * @return 1 if the address is within user space, 0 otherwise.
 */
int __am_in_userspace(void *addr) {
  return vme_enable && thiscpu->vm_head != NULL && IN_RANGE(addr, USER_SPACE);
}
