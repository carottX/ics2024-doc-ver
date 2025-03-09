// See LICENSE for license details.

#ifndef _RISCV_ATOMIC_H
#define _RISCV_ATOMIC_H

//#include "config.h"
//#include "encoding.h"

// Currently, interrupts are always disabled in M-mode.
#define disable_irqsave() (0)
#define enable_irqrestore(flags) ((void) (flags))

typedef struct { int lock; } spinlock_t;
#define SPINLOCK_INIT {0}

#define mb() asm volatile ("fence" ::: "memory")
#define atomic_set(ptr, val) (*(volatile typeof(*(ptr)) *)(ptr) = val)
#define atomic_read(ptr) (*(volatile typeof(*(ptr)) *)(ptr))

#ifdef __riscv_atomic
# define atomic_add(ptr, inc) __sync_fetch_and_add(ptr, inc)
# define atomic_or(ptr, inc) __sync_fetch_and_or(ptr, inc)
# define atomic_swap(ptr, swp) __sync_lock_test_and_set(ptr, swp)
# define atomic_cas(ptr, cmp, swp) __sync_val_compare_and_swap(ptr, cmp, swp)
#else
# define atomic_binop(ptr, inc, op) ({ \
  long flags = disable_irqsave(); \
  typeof(*(ptr)) res = atomic_read(ptr); \
  atomic_set(ptr, op); \
  enable_irqrestore(flags); \
  res; })
# define atomic_add(ptr, inc) atomic_binop(ptr, inc, res + (inc))
# define atomic_or(ptr, inc) atomic_binop(ptr, inc, res | (inc))
# define atomic_swap(ptr, inc) atomic_binop(ptr, inc, (inc))
# define atomic_cas(ptr, cmp, swp) ({ \
  long flags = disable_irqsave(); \
  typeof(*(ptr)) res = *(volatile typeof(*(ptr)) *)(ptr); \
  if (res == (cmp)) *(volatile typeof(ptr))(ptr) = (swp); \
  enable_irqrestore(flags); \
  res; })
#endif

/**
 * Attempts to acquire a spinlock.
 *
 * This function tries to acquire the spinlock pointed to by `lock` by atomically
 * swapping the lock's value with -1. If the lock was previously 0 (unlocked), the
 * function successfully acquires the lock and returns 0. If the lock was already
 * held (i.e., its value was -1), the function does not acquire the lock and returns
 * -1.
 *
 * After the atomic swap, a memory barrier (`mb()`) is issued to ensure that all
 * memory operations are completed before continuing.
 *
 * @param lock Pointer to the spinlock to be acquired.
 * @return 0 if the lock was successfully acquired, -1 if the lock was already held.
 */
static inline int spinlock_trylock(spinlock_t* lock)
{
  int res = atomic_swap(&lock->lock, -1);
  mb();
  return res;
}

/**
 * Acquires a spinlock by busy-waiting until the lock is available.
 * 
 * This function continuously checks the state of the spinlock in a loop. If the lock is already
 * held by another thread (i.e., `lock->lock` is non-zero), it waits in a busy loop until the lock
 * is released. Once the lock is free, it attempts to acquire the lock using `spinlock_trylock`.
 * If the attempt fails (e.g., due to contention), the process repeats until the lock is successfully
 * acquired.
 * 
 * @param lock A pointer to the spinlock_t structure representing the lock to be acquired.
 * @note This function uses busy-waiting, which can lead to high CPU usage. It is suitable for
 *       short critical sections where blocking is undesirable.
 */
static inline void spinlock_lock(spinlock_t* lock)
{
  do
  {
    while (atomic_read(&lock->lock))
      ;
  } while (spinlock_trylock(lock));
}

/**
 * Releases the specified spinlock by setting its lock value to 0, indicating that it is no longer held.
 * This function ensures proper memory ordering by invoking a memory barrier (`mb()`) before releasing the lock.
 * The memory barrier guarantees that all previous memory operations are completed before the lock is released,
 * preventing potential race conditions or inconsistent states in multi-threaded environments.
 *
 * @param lock A pointer to the spinlock_t structure representing the lock to be released.
 */
static inline void spinlock_unlock(spinlock_t* lock)
{
  mb();
  atomic_set(&lock->lock,0);
}

/**
 * Acquires a spinlock while disabling interrupts and saving the current interrupt state.
 *
 * This function first disables interrupts and saves the current interrupt flags into a local
 * variable. It then acquires the specified spinlock, ensuring mutual exclusion. The saved
 * interrupt flags are returned, allowing the caller to restore the interrupt state after
 * releasing the spinlock.
 *
 * @param lock A pointer to the spinlock to be acquired.
 * @return The saved interrupt flags, which can be used to restore the interrupt state later.
 */
static inline long spinlock_lock_irqsave(spinlock_t* lock)
{
  long flags = disable_irqsave();
  spinlock_lock(lock);
  return flags;
}

/**
 * @brief Unlocks a spinlock and restores the interrupt state.
 *
 * This function unlocks the given spinlock and restores the interrupt state
 * to the value specified by the `flags` parameter. It is typically used in
 * conjunction with `spinlock_lock_irqsave` to ensure that interrupts are
 * disabled while holding the spinlock and then restored to their previous state
 * after releasing the lock.
 *
 * @param lock A pointer to the spinlock to be unlocked.
 * @param flags The interrupt state to be restored, as previously saved by
 *              `spinlock_lock_irqsave`.
 */
static inline void spinlock_unlock_irqrestore(spinlock_t* lock, long flags)
{
  spinlock_unlock(lock);
  enable_irqrestore(flags);
}

#endif
