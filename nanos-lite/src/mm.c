#include <memory.h>

static void *pf = NULL;

/**
 * Allocates a new page or a contiguous block of pages in memory.
 *
 * This function attempts to allocate a contiguous block of memory pages based on the specified
 * number of pages (`nr_page`). The size of each page is system-dependent. If the allocation is
 * successful, a pointer to the beginning of the allocated memory block is returned. If the
 * allocation fails, the function returns `NULL`.
 *
 * @param nr_page The number of pages to allocate. Must be a positive integer.
 * @return A pointer to the allocated memory block on success, or `NULL` on failure.
 */
void* new_page(size_t nr_page) {
  return NULL;
}

#ifdef HAS_VME
/**
 * Allocates a block of memory of the specified size.
 *
 * This function attempts to allocate a contiguous block of memory of size `n` bytes.
 * If the allocation is successful, it returns a pointer to the beginning of the allocated block.
 * If the allocation fails, it returns NULL.
 *
 * @param n The number of bytes to allocate.
 * @return A pointer to the allocated memory block, or NULL if the allocation fails.
 */
static void* pg_alloc(int n) {
  return NULL;
}
#endif

/**
 * Frees a page of memory previously allocated by the system.
 * This function is intended to release the memory page pointed to by `p`,
 * making it available for future allocations. The function currently 
 * triggers a panic with the message "not implement yet" as the 
 * implementation is incomplete.
 *
 * @param p A pointer to the memory page to be freed. This pointer must 
 *          have been previously returned by a memory allocation function.
 *          If `p` is NULL, the function should do nothing (though the 
 *          current implementation does not handle this case).
 *
 * @note This function is a placeholder and does not yet perform any 
 *       actual memory deallocation. It is expected to be implemented 
 *       in future versions of the system.
 */
void free_page(void *p) {
  panic("not implement yet");
}

/* The brk() system call handler. */
int mm_brk(uintptr_t brk) {
  return 0;
}

/**
 * Initializes the memory management system.
 * 
 * This function sets up the memory management by aligning the start of the heap
 * to the nearest page boundary and logging the starting address of the free physical pages.
 * If the system supports Virtual Memory Extension (VME), it also initializes the VME
 * subsystem by providing it with the necessary page allocation and deallocation functions.
 * 
 * The function performs the following steps:
 * 1. Aligns the start of the heap to the nearest page boundary using the `ROUNDUP` macro.
 * 2. Logs the starting address of the free physical pages.
 * 3. If the system has VME support, initializes the VME subsystem by calling `vme_init`
 *    with the `pg_alloc` and `free_page` functions as arguments.
 */
void init_mm() {
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
