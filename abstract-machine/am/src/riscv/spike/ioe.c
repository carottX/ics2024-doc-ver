#include <am.h>
#include <klib-macros.h>

void __am_timer_init();
void __am_timer_rtc(AM_TIMER_RTC_T *);
void __am_timer_uptime(AM_TIMER_UPTIME_T *);

/**
 * Configures the timer module by setting its presence and RTC (Real-Time Clock) capabilities.
 * This method initializes the `AM_TIMER_CONFIG_T` structure by setting the `present` field to `true`
 * to indicate that the timer module is available, and the `has_rtc` field to `true` to indicate
 * that the timer module includes RTC functionality.
 *
 * @param cfg A pointer to an `AM_TIMER_CONFIG_T` structure that will be configured by this method.
 */
static void __am_timer_config(AM_TIMER_CONFIG_T *cfg) { cfg->present = true; cfg->has_rtc = true; }

typedef void (*handler_t)(void *buf);
static void *lut[128] = {
  [AM_TIMER_CONFIG] = __am_timer_config,
  [AM_TIMER_RTC   ] = __am_timer_rtc,
  [AM_TIMER_UPTIME] = __am_timer_uptime,
};

/**
 * Triggers a system panic with a predefined error message indicating an attempt
 * to access a non-existent register.
 *
 * This function is called when an invalid memory access is detected, specifically
 * when a buffer or pointer is expected to reference a valid register but does not.
 * It immediately halts the system and logs the error message "access nonexist register".
 *
 * @param buf A pointer to the buffer or memory location that was accessed. This
 *            parameter is unused in the function but is provided for context or
 *            future debugging purposes.
 */
static void fail(void *buf) { panic("access nonexist register"); }

/**
 * Initializes the IOE (Input/Output Engine) by ensuring all entries in the 
 * lookup table (LUT) are set to a predefined fail state if they are not 
 * already initialized. Additionally, it initializes the timer module 
 * by calling __am_timer_init().
 *
 * This function iterates over the LUT and checks if each entry is NULL. 
 * If an entry is NULL, it assigns the `fail` value to that entry. After 
 * initializing the LUT, the function proceeds to initialize the timer 
 * module to ensure proper timing functionality.
 *
 * @return Always returns `true` to indicate successful initialization.
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
 * for the given register. The handler is then invoked with the buffer as an
 * argument, allowing the handler to populate the buffer with the data read from
 * the register.
 * 
 * @param reg The index of the register to read from. This index is used to
 *            look up the corresponding handler in the LUT.
 * @param buf A pointer to the buffer where the read data will be stored. The
 *            buffer must be large enough to hold the data read from the register.
 */
void ioe_read (int reg, void *buf) { ((handler_t)lut[reg])(buf); }
/**
 * Writes data to a specified register using a handler from the lookup table.
 *
 * This function retrieves the handler associated with the given register from
 * the lookup table (lut) and invokes it with the provided buffer as an argument.
 * The handler is expected to process the data in the buffer and perform the
 * necessary write operation to the register.
 *
 * @param reg The register to which the data should be written. This value is
 *            used to index into the lookup table (lut) to retrieve the appropriate
 *            handler.
 * @param buf A pointer to the buffer containing the data to be written. This
 *            buffer is passed to the handler for processing.
 */
void ioe_write(int reg, void *buf) { ((handler_t)lut[reg])(buf); }
