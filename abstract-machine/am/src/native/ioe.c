#include <am.h>
#include <klib-macros.h>

bool __am_has_ioe = false;
static bool ioe_init_done = false;

void __am_timer_init();
void __am_gpu_init();
void __am_input_init();
void __am_uart_init();
void __am_audio_init();
void __am_disk_init();
void __am_input_config(AM_INPUT_CONFIG_T *);
void __am_timer_config(AM_TIMER_CONFIG_T *);
void __am_timer_rtc(AM_TIMER_RTC_T *);
void __am_timer_uptime(AM_TIMER_UPTIME_T *);
void __am_input_keybrd(AM_INPUT_KEYBRD_T *);
void __am_gpu_config(AM_GPU_CONFIG_T *);
void __am_gpu_status(AM_GPU_STATUS_T *);
void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *);
void __am_uart_config(AM_UART_CONFIG_T *);
void __am_uart_tx(AM_UART_TX_T *);
void __am_uart_rx(AM_UART_RX_T *);
void __am_audio_config(AM_AUDIO_CONFIG_T *);
void __am_audio_ctrl(AM_AUDIO_CTRL_T *);
void __am_audio_status(AM_AUDIO_STATUS_T *);
void __am_audio_play(AM_AUDIO_PLAY_T *);
void __am_disk_config(AM_DISK_CONFIG_T *cfg);
void __am_disk_status(AM_DISK_STATUS_T *stat);
void __am_disk_blkio(AM_DISK_BLKIO_T *io);
/**
 * @brief Configures the network settings by initializing the presence flag.
 *
 * This method sets the `present` field of the provided `AM_NET_CONFIG_T` structure
 * to `false`, indicating that the network is not currently available or configured.
 * It is typically used to reset or initialize network configuration settings
 * before applying new configurations or checking network availability.
 *
 * @param cfg Pointer to the `AM_NET_CONFIG_T` structure to be configured.
 */
static void __am_net_config (AM_NET_CONFIG_T *cfg) {
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
  [AM_UART_TX     ] = __am_uart_tx,
  [AM_UART_RX     ] = __am_uart_rx,
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
 * Initializes the I/O emulation subsystem.
 * 
 * This function ensures that the I/O emulation subsystem is initialized only once
 * and only by the first CPU (CPU 0). It sets the global flag `__am_has_ioe` to true,
 * indicating that the I/O emulation subsystem is now active.
 * 
 * @return Always returns `true` to indicate successful initialization.
 * 
 * @note This function must be called only by CPU 0. If called by any other CPU,
 *       it will trigger a panic with the message "call ioe_init() in other CPUs".
 * @note If the I/O emulation subsystem has already been initialized, calling this
 *       function will trigger a panic with the message "double-initialization".
 */
bool ioe_init() {
  panic_on(cpu_current() != 0, "call ioe_init() in other CPUs");
  panic_on(ioe_init_done, "double-initialization");
  __am_has_ioe = true;
  return true;
}

/**
 * Triggers a system panic with an error message indicating an attempt to access
 * a nonexistent register. This function is typically called when a critical
 * error occurs, such as accessing an invalid or unsupported hardware register.
 * The panic will halt the system and provide debugging information.
 *
 * @param buf A pointer to the buffer or register being accessed. This parameter
 *            is unused in the current implementation but may be included for
 *            future debugging or logging purposes.
 */
static void fail(void *buf) { panic("access nonexist register"); }

/**
 * Initializes all I/O devices by setting up their respective configurations.
 * This function performs the following operations:
 * 1. Iterates through the lookup table (lut) and initializes any uninitialized entries
 *    to a default failure state (fail).
 * 2. Calls initialization functions for various I/O devices in sequence:
 *    - Timer: Initializes the timer device.
 *    - GPU: Initializes the graphics processing unit.
 *    - Input: Initializes the input device (e.g., keyboard, mouse).
 *    - UART: Initializes the Universal Asynchronous Receiver-Transmitter.
 *    - Audio: Initializes the audio device.
 *    - Disk: Initializes the disk device.
 * 3. Sets the `ioe_init_done` flag to true, indicating that I/O device initialization is complete.
 * This function must be called before any I/O operations are performed to ensure proper device setup.
 */
void __am_ioe_init() {
  for (int i = 0; i < LENGTH(lut); i++)
    if (!lut[i]) lut[i] = fail;
  __am_timer_init();
  __am_gpu_init();
  __am_input_init();
  __am_uart_init();
  __am_audio_init();
  __am_disk_init();
  ioe_init_done = true;
}

/**
 * @brief Performs I/O operations based on the specified register and buffer.
 *
 * This method checks if the I/O environment has been initialized. If not, it initializes
 * the I/O environment by calling `__am_ioe_init()`. It then uses the provided register
 * index to look up a corresponding handler function from the lookup table (`lut`). The
 * handler function is invoked with the provided buffer as an argument.
 *
 * @param reg The register index used to look up the handler function in the lookup table.
 * @param buf A pointer to the buffer that will be passed to the handler function.
 */
static void do_io(int reg, void *buf) {
  if (!ioe_init_done) {
    __am_ioe_init();
  }
  ((handler_t)lut[reg])(buf);
}

/**
 * Reads data from a specified I/O register into a buffer.
 *
 * This function performs an I/O read operation by calling the `do_io` function,
 * which is responsible for the actual hardware interaction. The data read from
 * the I/O register is stored in the provided buffer.
 *
 * @param reg The I/O register address from which to read the data.
 * @param buf A pointer to the buffer where the read data will be stored.
 *            The buffer must be large enough to hold the data being read.
 */
void ioe_read (int reg, void *buf) { do_io(reg, buf); }
/**
 * Writes data to a specified I/O register.
 *
 * This function performs an I/O write operation by sending the contents of the provided buffer
 * to the specified register. The actual I/O operation is delegated to the `do_io` function.
 *
 * @param reg The I/O register address to which the data will be written.
 * @param buf A pointer to the buffer containing the data to be written. The size and format
 *            of the data are determined by the caller and should match the expectations of
 *            the target register.
 */
void ioe_write(int reg, void *buf) { do_io(reg, buf); }
