#include <common.h>

extern uint8_t ramdisk_start;
extern uint8_t ramdisk_end;
#define RAMDISK_SIZE ((&ramdisk_end) - (&ramdisk_start))

/* The kernel is monolithic, therefore we do not need to
 * translate the address `buf' from the user process to
 * a physical one, which is necessary for a microkernel.
 */

/* read `len' bytes starting from `offset' of ramdisk into `buf' */
size_t ramdisk_read(void *buf, size_t offset, size_t len) {
  assert(offset + len <= RAMDISK_SIZE);
  memcpy(buf, &ramdisk_start + offset, len);
  return len;
}

/* write `len' bytes starting from `buf' into the `offset' of ramdisk */
size_t ramdisk_write(const void *buf, size_t offset, size_t len) {
  assert(offset + len <= RAMDISK_SIZE);
  memcpy(&ramdisk_start + offset, buf, len);
  return len;
}

/**
 * Initializes the RAM disk by logging its information to the system log.
 * This function logs the starting address, ending address, and the total size
 * of the RAM disk. The information is useful for debugging and understanding
 * the memory layout of the RAM disk during system initialization.
 *
 * The logged information includes:
 * - The starting address of the RAM disk (`ramdisk_start`).
 * - The ending address of the RAM disk (`ramdisk_end`).
 * - The total size of the RAM disk in bytes (`RAMDISK_SIZE`).
 *
 * This function does not perform any actual initialization of the RAM disk
 * but serves as a diagnostic tool to verify its configuration.
 */
void init_ramdisk() {
  Log("ramdisk info: start = %p, end = %p, size = %d bytes",
      &ramdisk_start, &ramdisk_end, RAMDISK_SIZE);
}

/**
 * @brief Returns the size of the RAM disk.
 *
 * This function retrieves the size of the RAM disk, which is defined by the
 * constant `RAMDISK_SIZE`. The size is returned in bytes as a value of type
 * `size_t`.
 *
 * @return The size of the RAM disk in bytes.
 */
size_t get_ramdisk_size() {
  return RAMDISK_SIZE;
}
