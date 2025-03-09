#include <am.h>
#include <klib-macros.h>

void __am_timer_init();
void __am_gpu_init();
void __am_audio_init();
void __am_input_keybrd(AM_INPUT_KEYBRD_T *);
void __am_timer_rtc(AM_TIMER_RTC_T *);
void __am_timer_uptime(AM_TIMER_UPTIME_T *);
void __am_gpu_config(AM_GPU_CONFIG_T *);
void __am_gpu_status(AM_GPU_STATUS_T *);
void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *);
void __am_audio_config(AM_AUDIO_CONFIG_T *);
void __am_audio_ctrl(AM_AUDIO_CTRL_T *);
void __am_audio_status(AM_AUDIO_STATUS_T *);
void __am_audio_play(AM_AUDIO_PLAY_T *);
void __am_disk_config(AM_DISK_CONFIG_T *cfg);
void __am_disk_status(AM_DISK_STATUS_T *stat);
void __am_disk_blkio(AM_DISK_BLKIO_T *io);

/**
 * Configures the timer device by setting its presence and Real-Time Clock (RTC) capabilities.
 * This function initializes the provided `AM_TIMER_CONFIG_T` structure to indicate that the timer
 * device is present and supports RTC functionality.
 *
 * @param cfg A pointer to an `AM_TIMER_CONFIG_T` structure that will be configured.
 *            The `present` field is set to `true` to indicate the timer device is available,
 *            and the `has_rtc` field is set to `true` to indicate RTC support.
 */
static void __am_timer_config(AM_TIMER_CONFIG_T *cfg) { cfg->present = true; cfg->has_rtc = true; }
/**
 * Configures the input device by setting the 'present' field to true.
 * This method is used to indicate that the input device is currently
 * available and ready for use. The configuration is applied directly
 * to the provided AM_INPUT_CONFIG_T structure.
 *
 * @param cfg Pointer to an AM_INPUT_CONFIG_T structure that will be
 *            configured to indicate the presence of the input device.
 */
static void __am_input_config(AM_INPUT_CONFIG_T *cfg) { cfg->present = true; }
/**
 * Configures the UART (Universal Asynchronous Receiver-Transmitter) by setting its presence status.
 * This method initializes the UART configuration structure by marking the UART as not present.
 *
 * @param cfg Pointer to the AM_UART_CONFIG_T structure that holds the UART configuration.
 *            The 'present' field of this structure is set to 'false' to indicate that the UART
 *            is not available or not initialized.
 */
static void __am_uart_config(AM_UART_CONFIG_T *cfg) {
    cfg->present = false;
}
/**
 * Configures the network settings by initializing the provided configuration
 * structure. This method sets the 'present' field of the AM_NET_CONFIG_T
 * structure to 'false', indicating that the network is not currently available
 * or configured.
 *
 * @param cfg Pointer to the AM_NET_CONFIG_T structure to be initialized.
 */
static void __am_net_config(AM_NET_CONFIG_T *cfg) {
    cfg->present = false;
}

typedef void (*handler_t)(void *buf);
static void *lut[128] = {
  [AM_TIMER_CONFIG] = __am_timer_config,
  [AM_TIMER_RTC   ] = __am_timer_rtc,
  [AM_TIMER_UPTIME] = __am_timer_uptime,
  [AM_INPUT_CONFIG] = __am_input_config,
  [AM_INPUT_KEYBRD] = __am_input_keybrd,
  [AM_GPU_CONFIG  ] = __am_gpu_config,
  [AM_GPU_FBDRAW  ] = __am_gpu_fbdraw,
  [AM_GPU_STATUS  ] = __am_gpu_status,
  [AM_UART_CONFIG ] = __am_uart_config,
  [AM_AUDIO_CONFIG] = __am_audio_config,
  [AM_AUDIO_CTRL  ] = __am_audio_ctrl,
  [AM_AUDIO_STATUS] = __am_audio_status,
  [AM_AUDIO_PLAY  ] = __am_audio_play,
  [AM_DISK_CONFIG ] = __am_disk_config,
  [AM_DISK_STATUS ] = __am_disk_status,
  [AM_DISK_BLKIO  ] = __am_disk_blkio,
  [AM_NET_CONFIG  ] = __am_net_config,
};

/**
 * Triggers a system panic with an error message indicating an attempt to access
 * a nonexistent register. This function is typically called when an invalid
 * memory or hardware register access is detected.
 *
 * @param buf A pointer to the buffer or memory location that was being accessed
 *            when the failure occurred. This parameter is not used in the
 *            function but is provided for context or debugging purposes.
 */
static void fail(void *buf) { panic("access nonexist register"); }

/**
 * Initializes the IOE (Input/Output Engine) subsystem.
 * 
 * This method performs the following tasks:
 * 1. Iterates through the lookup table (lut) and initializes any uninitialized entries with a predefined failure value.
 * 2. Initializes the GPU subsystem by calling `__am_gpu_init()`.
 * 3. Initializes the timer subsystem by calling `__am_timer_init()`.
 * 4. Initializes the audio subsystem by calling `__am_audio_init()`.
 * 
 * @return Always returns `true` to indicate successful initialization.
 */
bool ioe_init() {
  for (int i = 0; i < LENGTH(lut); i++)
    if (!lut[i]) lut[i] = fail;
  __am_gpu_init();
  __am_timer_init();
  __am_audio_init();
  return true;
}

/**
 * Reads data from a specified register and stores it in the provided buffer.
 *
 * This method uses a lookup table (LUT) to determine the appropriate handler function
 * based on the register index. The handler function is then invoked with the buffer
 * as its argument, allowing the data from the register to be read and stored in the buffer.
 *
 * @param reg The index of the register to read from.
 * @param buf A pointer to the buffer where the read data will be stored.
 */
void ioe_read (int reg, void *buf) { ((handler_t)lut[reg])(buf); }
/**
 * Writes data to a specified register using a handler from the lookup table (LUT).
 *
 * This function retrieves the appropriate handler function from the LUT based on the provided
 * register index (`reg`). The handler function is then invoked with the provided buffer (`buf`)
 * as its argument, effectively writing the data to the register.
 *
 * @param reg The index of the register in the LUT to which the data will be written.
 * @param buf A pointer to the buffer containing the data to be written.
 */
void ioe_write(int reg, void *buf) { ((handler_t)lut[reg])(buf); }
