#include <am.h>
#include <klib-macros.h>

void __am_timer_init();

void __am_timer_rtc(AM_TIMER_RTC_T *);
void __am_timer_uptime(AM_TIMER_UPTIME_T *);
void __am_input_keybrd(AM_INPUT_KEYBRD_T *);

/**
 * Configures the timer module by setting the `present` and `has_rtc` fields
 * of the provided `AM_TIMER_CONFIG_T` structure to `true`. This indicates
 * that the timer module is present and supports real-time clock (RTC)
 * functionality.
 *
 * @param cfg Pointer to an `AM_TIMER_CONFIG_T` structure that will be
 *            updated to reflect the timer module's configuration.
 */
static void __am_timer_config(AM_TIMER_CONFIG_T *cfg) {
    cfg->present = true;
    cfg->has_rtc = true;
}
/**
 * Configures the input device by setting its presence status.
 *
 * This method initializes the input device configuration by setting the `present`
 * field of the provided `AM_INPUT_CONFIG_T` structure to `true`. This indicates
 * that the input device is present and available for use.
 *
 * @param cfg Pointer to an `AM_INPUT_CONFIG_T` structure representing the input
 *            device configuration to be updated.
 */
static void __am_input_config(AM_INPUT_CONFIG_T *cfg) { cfg->present = true; }
/**
 * Configures the UART input configuration by setting the 'present' field to false.
 * This method is typically used to initialize or reset the UART configuration,
 * indicating that the UART device is not currently present or active.
 *
 * @param cfg Pointer to an AM_INPUT_CONFIG_T structure representing the UART input configuration.
 */
static void __am_uart_config(AM_INPUT_CONFIG_T *cfg) { cfg->present = false; }

typedef void (*handler_t)(void *buf);
static void *lut[128] = {
  [AM_TIMER_CONFIG] = __am_timer_config,
  [AM_TIMER_RTC   ] = __am_timer_rtc,
  [AM_TIMER_UPTIME] = __am_timer_uptime,
  [AM_INPUT_CONFIG] = __am_input_config,
  [AM_INPUT_KEYBRD] = __am_input_keybrd,
  [AM_UART_CONFIG]  = __am_uart_config,
};

/**
 * Triggers a system panic with a message indicating an attempt to access a
 * non-existent register. This function is typically called when an invalid
 * memory or register access is detected, and it immediately halts the system
 * to prevent further undefined behavior or corruption.
 *
 * @param buf A pointer to the buffer or memory location that was being accessed.
 *            This parameter is currently unused in the implementation but may
 *            be useful for debugging or logging purposes in the future.
 */
static void fail(void *buf) { panic("access nonexist register"); }

/**
 * Initializes the IOE (Input/Output Engine) by performing the following steps:
 * 1. Iterates through the lookup table (lut) and initializes any uninitialized entries
 *    to the 'fail' value.
 * 2. Calls the '__am_timer_init()' function to initialize the timer.
 * 
 * @return Returns 'true' to indicate successful initialization.
 */
bool ioe_init() {
  for (int i = 0; i < LENGTH(lut); i++)
    if (!lut[i]) lut[i] = fail;
  __am_timer_init();
  return true;
}

/**
 * Reads data from a specified register and stores it in the provided buffer.
 * 
 * This function uses a lookup table (LUT) to determine the appropriate handler
 * function associated with the given register. The handler function is then
 * invoked with the buffer as an argument, allowing the data from the register
 * to be read and stored in the buffer.
 * 
 * @param reg The register number from which to read the data. This value is
 *            used as an index into the lookup table (LUT) to find the
 *            corresponding handler function.
 * @param buf A pointer to the buffer where the read data will be stored. The
 *            buffer must be appropriately sized to accommodate the data
 *            being read from the register.
 */
void ioe_read (int reg, void *buf) { ((handler_t)lut[reg])(buf); }
/**
 * Writes data to a specified register using a handler function from the lookup table.
 *
 * This function retrieves the handler function associated with the given register
 * from the lookup table (lut) and invokes it with the provided buffer as an argument.
 * The handler function is responsible for performing the actual write operation.
 *
 * @param reg The register number to which the data should be written. This value is
 *            used to index into the lookup table to retrieve the appropriate handler.
 * @param buf A pointer to the buffer containing the data to be written. The buffer
 *            is passed directly to the handler function.
 */
void ioe_write(int reg, void *buf) { ((handler_t)lut[reg])(buf); }
