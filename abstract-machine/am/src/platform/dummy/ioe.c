#include <am.h>
#include <klib-macros.h>

/**
 * Triggers a system panic with an error message indicating an attempt to access
 * a nonexistent register. This function is typically used in scenarios where
 * invalid memory or register access is detected, halting the system to prevent
 * further undefined behavior.
 *
 * @param buf A pointer to the memory or register that was attempted to be accessed.
 *            This parameter is unused in the current implementation but may be
 *            provided for future extensions or debugging purposes.
 */
static void fail(void *buf) { panic("access nonexist register"); }

/**
 * @brief Initializes the Input/Output Engine (IOE).
 *
 * This method is responsible for initializing the Input/Output Engine (IOE),
 * which may include setting up necessary hardware or software components
 * required for input/output operations. The method returns a boolean value
 * indicating the success or failure of the initialization process.
 *
 * @return bool - Returns `true` if the initialization is successful, 
 *                otherwise returns `false`.
 */
bool ioe_init() {
  return false;
}

/**
 * Reads data from a specified register into a buffer.
 *
 * This method is intended to read data from a hardware or virtual register
 * identified by `reg` and store the result in the memory location pointed to
 * by `buf`. The buffer must be pre-allocated and large enough to hold the data
 * being read. If the read operation fails, the `fail` function is called with
 * the buffer as an argument.
 *
 * @param reg The register from which to read the data.
 * @param buf A pointer to the buffer where the read data will be stored.
 */
void ioe_read (int reg, void *buf) { fail(buf); }
/**
 * Writes data to a specified I/O register.
 *
 * This function is intended to write data from a buffer to a specific I/O register.
 * The function takes two parameters: the register address `reg` and a pointer to
 * the buffer `buf` containing the data to be written. The function internally calls
 * `fail(buf)` to handle the write operation, which may involve error handling or
 * other processing.
 *
 * @param reg The address of the I/O register to which the data will be written.
 * @param buf A pointer to the buffer containing the data to be written to the register.
 *            The buffer must be properly allocated and initialized before calling this function.
 */
void ioe_write(int reg, void *buf) { fail(buf); }
