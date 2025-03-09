#include <common.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

/**
 * Writes data to a serial output buffer starting at a specified offset.
 *
 * This method is intended to write a specified number of bytes (`len`) from a source buffer (`buf`)
 * to a serial output buffer, beginning at the given `offset`. The method returns the number of bytes
 * successfully written. In the current implementation, it always returns 0, indicating no bytes
 * were written. This behavior should be updated in a concrete implementation to reflect actual
 * serial writing logic.
 *
 * @param buf    Pointer to the source buffer containing the data to be written.
 * @param offset The starting position in the serial output buffer where writing should begin.
 * @param len    The number of bytes to write from the source buffer.
 * @return       The number of bytes successfully written. Currently, this always returns 0.
 */
size_t serial_write(const void *buf, size_t offset, size_t len) {
  return 0;
}

/**
 * Reads events from a source into the provided buffer.
 *
 * This function attempts to read up to `len` bytes of event data from a source,
 * starting at the specified `offset`, and stores the data in the buffer pointed
 * to by `buf`. The function returns the number of bytes successfully read.
 *
 * @param buf    A pointer to the buffer where the read data will be stored.
 * @param offset The offset in the source from where to start reading.
 * @param len    The maximum number of bytes to read.
 *
 * @return The number of bytes successfully read. If the return value is less
 *         than `len`, it indicates that fewer bytes were read than requested.
 *         A return value of 0 typically indicates that no data was available
 *         to read or that the end of the source has been reached.
 */
size_t events_read(void *buf, size_t offset, size_t len) {
  return 0;
}

/**
 * Reads display information into the provided buffer.
 *
 * This function is intended to read a specified amount of display information
 * from a given offset into the provided buffer. The function currently returns
 * 0, indicating that no data is read. This could be a placeholder or a stub
 * implementation that needs to be completed.
 *
 * @param buf    Pointer to the buffer where the display information will be stored.
 * @param offset The offset from which to start reading the display information.
 * @param len    The number of bytes to read from the display information.
 *
 * @return The number of bytes successfully read. Currently, this function always returns 0.
 */
size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  return 0;
}

/**
 * Writes data to a framebuffer at a specified offset.
 *
 * This function attempts to write `len` bytes of data from the buffer `buf` to the framebuffer
 * starting at the specified `offset`. The framebuffer is typically a memory-mapped region
 * representing a display or similar output device.
 *
 * @param buf    A pointer to the buffer containing the data to be written. Must not be NULL.
 * @param offset The offset in the framebuffer where the data should be written. This is typically
 *               a byte offset from the start of the framebuffer.
 * @param len    The number of bytes to write from the buffer to the framebuffer.
 *
 * @return The number of bytes successfully written to the framebuffer. If the operation fails,
 *         this function returns 0. Implementations should ensure that the return value does not
 *         exceed `len`.
 */
size_t fb_write(const void *buf, size_t offset, size_t len) {
  return 0;
}

/**
 * Initializes the devices by performing the necessary setup steps.
 * This function logs a message indicating the start of the initialization process
 * and then calls the `ioe_init()` function to initialize the input/output devices.
 * It is typically called at the start of the program to ensure all devices are
 * ready for operation.
 */
void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
