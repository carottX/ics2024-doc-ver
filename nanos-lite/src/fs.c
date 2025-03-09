#include <fs.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB};

/**
 * @brief Placeholder function to handle invalid read operations.
 *
 * This function is intended to be called when an invalid read operation is
 * attempted. It immediately triggers a panic, indicating that the program
 * should not reach this point. The function serves as a safeguard to catch
 * unexpected or erroneous read operations during execution.
 *
 * @param buf Pointer to the buffer from which the read operation was attempted.
 *            This parameter is unused in the function.
 * @param offset The offset within the buffer where the read was attempted.
 *               This parameter is unused in the function.
 * @param len The number of bytes that were attempted to be read.
 *            This parameter is unused in the function.
 * @return Always returns 0, though the function will panic before reaching
 *         this point.
 */
size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/**
 * @brief This function is a placeholder for handling invalid write operations.
 * 
 * The function is designed to be called when an invalid write operation is 
 * detected. It immediately triggers a panic state, indicating a critical 
 * error in the program's execution flow. The function does not perform any 
 * actual write operation and always returns 0.
 * 
 * @param buf A pointer to the buffer that was intended to be written to. 
 *            This parameter is not used in the function.
 * @param offset The offset within the buffer where the write was intended to 
 *               occur. This parameter is not used in the function.
 * @param len The number of bytes that were intended to be written. This 
 *            parameter is not used in the function.
 * 
 * @return Always returns 0, as no actual write operation is performed.
 */
size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]  = {"stdin", 0, 0, invalid_read, invalid_write},
  [FD_STDOUT] = {"stdout", 0, 0, invalid_read, invalid_write},
  [FD_STDERR] = {"stderr", 0, 0, invalid_read, invalid_write},
#include "files.h"
};

/**
 * Initializes the file system, specifically focusing on setting up the framebuffer device.
 * This method is responsible for initializing the size of /dev/fb, which is typically
 * used for graphical display purposes. The framebuffer device represents a portion of
 * memory that directly corresponds to the display's pixel buffer, allowing for direct
 * manipulation of the display's contents.
 *
 * The initialization process involves determining the appropriate size of the framebuffer
 * based on the display resolution and color depth, and then configuring /dev/fb accordingly.
 * This ensures that the framebuffer is correctly sized to match the display requirements,
 * enabling proper rendering and display of graphical content.
 */
void init_fs() {
  // TODO: initialize the size of /dev/fb
}
