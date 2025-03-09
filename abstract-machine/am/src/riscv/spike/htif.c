// See LICENSE for license details.

#include "htif.h"
#include "atomic.h"
#include <klib.h>

extern uint64_t __htif_base;
volatile uint64_t tohost __attribute__((section(".htif")));
volatile uint64_t fromhost __attribute__((section(".htif")));
volatile int htif_console_buf;
static spinlock_t htif_lock = SPINLOCK_INIT;

#define TOHOST(base_int)	(uint64_t *)(base_int + TOHOST_OFFSET)
#define FROMHOST(base_int)	(uint64_t *)(base_int + FROMHOST_OFFSET)

#define TOHOST_OFFSET		((uintptr_t)tohost - (uintptr_t)__htif_base)
#define FROMHOST_OFFSET		((uintptr_t)fromhost - (uintptr_t)__htif_base)

/**
 * @brief Processes the `fromhost` value and handles the associated command.
 *
 * This method checks the `fromhost` value, which is expected to contain a command
 * and data from the host system. If `fromhost` is non-zero, it is cleared to indicate
 * that the command has been processed. The method asserts that the command originates
 * from the console device (device ID 1). Depending on the command (extracted using
 * `FROMHOST_CMD`), it performs the following actions:
 * - For command 0: Updates the `htif_console_buf` with the data from `fromhost`.
 * - For command 1: No action is taken.
 * - For any other command: Triggers an assertion failure, indicating an invalid command.
 *
 * @note This method assumes that `fromhost` is a 64-bit value containing a device ID,
 * command, and data, which are extracted using the `FROMHOST_DEV`, `FROMHOST_CMD`,
 * and `FROMHOST_DATA` macros, respectively.
 */
static void __check_fromhost()
{
  uint64_t fh = fromhost;
  if (!fh)
    return;
  fromhost = 0;

  // this should be from the console
  assert(FROMHOST_DEV(fh) == 1);
  switch (FROMHOST_CMD(fh)) {
    case 0:
      htif_console_buf = 1 + (uint8_t)FROMHOST_DATA(fh);
      break;
    case 1:
      break;
    default:
      assert(0);
  }
}

/**
 * Sets the `tohost` register with the specified device, command, and data values.
 * 
 * This function first waits until the `tohost` register is cleared by continuously
 * checking the `fromhost` register using the `__check_fromhost()` function. Once
 * `tohost` is cleared, it sets the `tohost` register by combining the provided
 * `dev`, `cmd`, and `data` values using the `TOHOST_CMD` macro.
 *
 * @param dev  The device identifier to be included in the `tohost` register.
 * @param cmd  The command to be included in the `tohost` register.
 * @param data The data to be included in the `tohost` register.
 */
static void __set_tohost(uintptr_t dev, uintptr_t cmd, uintptr_t data)
{
  while (tohost)
    __check_fromhost();
  tohost = TOHOST_CMD(dev, cmd, data);
}

/**
 * Reads a character from the HTIF (Host Target Interface) console buffer.
 *
 * This function retrieves a character from the HTIF console buffer if available.
 * On RV32 systems, HTIF devices are not supported, and the function returns -1.
 * On RV64 systems, the function locks the HTIF spinlock to ensure thread-safe access
 * to the console buffer. It checks the buffer for a character, and if one is found,
 * it clears the buffer and signals the host via the `tohost` register. The function
 * then unlocks the spinlock and returns the character. If no character is available,
 * it returns -1.
 *
 * @return The character read from the console buffer, or -1 if no character is
 *         available or if HTIF is not supported (RV32).
 */
int htif_console_getchar()
{
#if __riscv_xlen == 32
  // HTIF devices are not supported on RV32
  return -1;
#endif

  spinlock_lock(&htif_lock);
    __check_fromhost();
    int ch = htif_console_buf;
    if (ch >= 0) {
      htif_console_buf = -1;
      __set_tohost(1, 0, 0);
    }
  spinlock_unlock(&htif_lock);

  return ch - 1;
}

/**
 * @brief Sends a command to the host and waits for a response.
 *
 * This method locks the HTIF (Host-Target Interface) spinlock to ensure exclusive access
 * to the host-target communication mechanism. It then sends a command to the host using
 * the `__set_tohost` function, specifying the device, command, and data. The method
 * enters a busy-wait loop, continuously checking the `fromhost` variable for a response.
 * If a response is received that matches the specified device and command, the method
 * clears the `fromhost` variable and exits the loop. If the response does not match,
 * the method calls `__check_fromhost` to handle the unexpected response. Finally, the
 * HTIF spinlock is unlocked to allow other threads to access the host-target interface.
 *
 * @param dev The device identifier to which the command is directed.
 * @param cmd The command to be executed by the host.
 * @param data The data associated with the command.
 */
static void do_tohost_fromhost(uintptr_t dev, uintptr_t cmd, uintptr_t data)
{
  spinlock_lock(&htif_lock);
    __set_tohost(dev, cmd, data);

    while (1) {
      uint64_t fh = fromhost;
      if (fh) {
        if (FROMHOST_DEV(fh) == dev && FROMHOST_CMD(fh) == cmd) {
          fromhost = 0;
          break;
        }
        __check_fromhost();
      }
    }
  spinlock_unlock(&htif_lock);
}

/**
 * @brief Executes a system call by invoking the host interface (HTIF).
 *
 * This method is used to perform a system call by interacting with the host
 * interface. It delegates the actual system call operation to the
 * `do_tohost_fromhost` function, passing the provided argument along with
 * two zero values as parameters.
 *
 * @param arg The argument to be passed to the system call. This typically
 *            represents the specific request or data needed for the system
 *            call operation.
 */
void htif_syscall(uintptr_t arg)
{
  do_tohost_fromhost(0, 0, arg);
}

/**
 * @brief Outputs a single character to the HTIF console.
 *
 * This function writes a character to the HTIF (Host Target Interface) console.
 * On RV32 systems, HTIF devices are not supported directly, so the function
 * proxies a write system call using a magic memory region to simulate the
 * operation. On RV64 systems, the function directly interacts with the HTIF
 * device by setting the tohost register with the character to be printed.
 * The function ensures thread safety by acquiring a spinlock before modifying
 * the HTIF device on RV64 systems.
 *
 * @param ch The character to be printed to the console.
 */
void htif_console_putchar(uint8_t ch)
{
#if __riscv_xlen == 32
  // HTIF devices are not supported on RV32, so proxy a write system call
  volatile uint64_t magic_mem[8];
  magic_mem[0] = SYS_write;
  magic_mem[1] = 1;
  magic_mem[2] = (uintptr_t)&ch;
  magic_mem[3] = 1;
  do_tohost_fromhost(0, 0, (uintptr_t)magic_mem);
#else
  spinlock_lock(&htif_lock);
    __set_tohost(1, 1, ch);
  spinlock_unlock(&htif_lock);
#endif
}

/**
 * @brief Continuously powers off the system by entering an infinite loop.
 *
 * This method is designed to simulate a power-off sequence by entering an
 * infinite loop where it repeatedly sets the `fromhost` variable to 0 and
 * the `tohost` variable to 1. This behavior effectively halts further
 * execution and can be used to signal a shutdown or power-off state in
 * systems that monitor these variables.
 */
void htif_poweroff()
{
  while (1) {
    fromhost = 0;
    tohost = 1;
  }
}
