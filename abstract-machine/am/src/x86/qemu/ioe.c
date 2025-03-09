#include "x86-qemu.h"
#include <klib.h> // TODO: delete

// UART
// ====================================================

#define COM1 0x3f8

/**
 * Initializes the UART (Universal Asynchronous Receiver/Transmitter) for COM1.
 * This function configures the UART by setting up the necessary control and data registers.
 * The steps include:
 * 1. Disabling interrupts by writing 0 to the Interrupt Enable Register (IER).
 * 2. Setting the Divisor Latch Access Bit (DLAB) to 1 to configure the baud rate.
 * 3. Writing the baud rate divisor value to the Divisor Latch registers.
 * 4. Clearing the DLAB bit and setting the data format (8 data bits, 1 stop bit, no parity).
 * 5. Disabling FIFO (First In First Out) buffers by writing to the FIFO Control Register (FCR).
 * 6. Enabling the Data Ready Interrupt by setting the appropriate bit in the IER.
 * 7. Reading the Line Status Register (LSR) and Receiver Buffer Register (RBR) to clear any pending data.
 * 
 * @return Returns 0 upon successful initialization.
 */
static int uart_init() {
  outb(COM1 + 2, 0);
  outb(COM1 + 3, 0x80);
  outb(COM1 + 0, 115200 / 9600);
  outb(COM1 + 1, 0);
  outb(COM1 + 3, 0x03);
  outb(COM1 + 4, 0);
  outb(COM1 + 1, 0x01);
  inb (COM1 + 2);
  inb (COM1 + 0);
  return 0;
}

/**
 * @brief Configures the UART settings by initializing the provided configuration structure.
 *
 * This function sets the `present` field of the `AM_UART_CONFIG_T` structure to `true`,
 * indicating that the UART module is present and configured. This is typically used to
 * enable or acknowledge the UART module in the system.
 *
 * @param cfg Pointer to the `AM_UART_CONFIG_T` structure that will be configured.
 */
static void uart_config(AM_UART_CONFIG_T *cfg) {
  cfg->present = true;
}

/**
 * Sends a single byte of data to the UART (Universal Asynchronous Receiver-Transmitter) 
 * device at the COM1 port. The data to be transmitted is provided in the `send` parameter.
 *
 * @param send A pointer to an `AM_UART_TX_T` structure containing the data byte to be sent.
 *             The `data` field of this structure holds the byte to transmit.
 */
static void uart_tx(AM_UART_TX_T *send) {
  outb(COM1, send->data);
}

/**
 * Reads data from the UART (Universal Asynchronous Receiver/Transmitter) COM1 port.
 * This method checks the Line Status Register (LSR) at offset 5 from the COM1 base address
 * to determine if data is available in the UART's receive buffer. If data is available,
 * it reads the data from the UART's data register at the COM1 base address and stores it
 * in the `data` field of the provided `AM_UART_RX_T` structure. If no data is available,
 * it sets the `data` field to -1.
 *
 * @param recv Pointer to an `AM_UART_RX_T` structure where the received data will be stored.
 */
static void uart_rx(AM_UART_RX_T *recv) {
  recv->data = (inb(COM1 + 5) & 0x1) ? inb(COM1) : -1;
}

// Timer
// ====================================================

static AM_TIMER_RTC_T boot_date;
static uint32_t freq_mhz = 2000;
static uint64_t uptsc;
static void timer_rtc(AM_TIMER_RTC_T *rtc);

/**
 * Reads a value from the Real-Time Clock (RTC) register specified by `reg`.
 * The RTC is accessed through I/O ports 0x70 (address port) and 0x71 (data port).
 * The value read from the RTC is in Binary-Coded Decimal (BCD) format, which is
 * converted to a standard integer before returning.
 *
 * @param reg The RTC register to read from (e.g., 0x00 for seconds, 0x02 for minutes).
 * @return The integer value read from the RTC register, converted from BCD format.
 */
static inline int read_rtc(int reg) {
  outb(0x70, reg);
  int ret = inb(0x71);
  return (ret & 0xf) + (ret >> 4) * 10;
}

/**
 * @brief Reads the current Real-Time Clock (RTC) values asynchronously and stores them in the provided structure.
 *
 * This function reads the RTC registers for seconds, minutes, hours, day, month, and year, and then
 * populates the `AM_TIMER_RTC_T` structure with these values. The year value is adjusted by adding 2000
 * to the value read from the RTC register, assuming the RTC stores the year as a two-digit number.
 *
 * @param rtc Pointer to an `AM_TIMER_RTC_T` structure where the RTC values will be stored.
 *            The structure fields are populated as follows:
 *            - `second`: The current second value (0-59) read from RTC register 0.
 *            - `minute`: The current minute value (0-59) read from RTC register 2.
 *            - `hour`: The current hour value (0-23) read from RTC register 4.
 *            - `day`: The current day of the month (1-31) read from RTC register 7.
 *            - `month`: The current month value (1-12) read from RTC register 8.
 *            - `year`: The current year value, adjusted by adding 2000 to the value read from RTC register 9.
 */
static void read_rtc_async(AM_TIMER_RTC_T *rtc) {
  *rtc = (AM_TIMER_RTC_T) {
    .second = read_rtc(0),
    .minute = read_rtc(2),
    .hour   = read_rtc(4),
    .day    = read_rtc(7),
    .month  = read_rtc(8),
    .year   = read_rtc(9) + 2000,
  };
}

/**
 * Waits until the Real-Time Clock (RTC) advances by one second.
 *
 * This function continuously reads the current time from the RTC and compares
 * it with a previous reading. It waits in a loop until the second value of the
 * RTC changes, indicating that one second has elapsed. The function uses a busy-wait
 * loop with a delay to reduce the frequency of RTC reads.
 *
 * @param t1 Pointer to an AM_TIMER_RTC_T structure where the final RTC time
 *           (after the second has advanced) will be stored.
 */
static void wait_sec(AM_TIMER_RTC_T *t1) {
  AM_TIMER_RTC_T t0;
  while (1) {
    read_rtc_async(&t0);
    for (int volatile i = 0; i < 100000; i++) ;
    read_rtc_async(t1);
    if (t0.second != t1->second) {
      return;
    }
  }
}

/**
 * Estimates the CPU frequency by comparing the Time Stamp Counter (TSC) values
 * against the Real-Time Clock (RTC) over a short time interval.
 * 
 * The method works by:
 * 1. Capturing the current RTC time and TSC value.
 * 2. Waiting for a second to elapse (using the RTC).
 * 3. Capturing the updated RTC time and TSC value.
 * 4. Calculating the difference in TSC values and the corresponding time difference
 *    in seconds based on the RTC.
 * 5. If the RTC time difference is non-positive (e.g., due to an hour boundary),
 *    the method retries the measurement.
 * 6. Returns the estimated CPU frequency in MHz by dividing the TSC difference
 *    (shifted right by 20 bits to convert to MHz) by the time difference in seconds.
 * 
 * @return The estimated CPU frequency in MHz as a 32-bit unsigned integer.
 */
static uint32_t estimate_freq() {
  AM_TIMER_RTC_T rtc1, rtc2;
  uint64_t tsc1, tsc2, t1, t2;
  wait_sec(&rtc1); tsc1 = rdtsc(); t1 = rtc1.minute * 60 + rtc1.second;
  wait_sec(&rtc2); tsc2 = rdtsc(); t2 = rtc2.minute * 60 + rtc2.second;
  if (t1 >= t2) return estimate_freq(); // passed an hour; try again
  return ((tsc2 - tsc1) >> 20) / (t2 - t1);
}

/**
 * Initializes the timer by performing the following steps:
 * 1. Estimates the CPU frequency in MHz and stores it in the global variable `freq_mhz`.
 * 2. Initializes the Real-Time Clock (RTC) by storing the boot date in the global variable `boot_date`.
 * 3. Reads the current value of the Time Stamp Counter (TSC) and stores it in the global variable `uptsc`.
 * This function is essential for setting up timing and clock-related functionalities in the system.
 */
static void timer_init() {
  freq_mhz = estimate_freq();
  timer_rtc(&boot_date);
  uptsc = rdtsc();
}

/**
 * Configures the timer settings by initializing the provided AM_TIMER_CONFIG_T structure.
 * This method sets the `present` and `has_rtc` fields of the configuration structure to `true`,
 * indicating that the timer is present and that it includes a Real-Time Clock (RTC) feature.
 *
 * @param cfg A pointer to an AM_TIMER_CONFIG_T structure that will be configured.
 */
static void timer_config(AM_TIMER_CONFIG_T *cfg) {
  cfg->present = cfg->has_rtc = true;
}

/**
 * Synchronizes the provided RTC (Real-Time Clock) structure with the current RTC time.
 * 
 * This method continuously reads the RTC asynchronously and compares the read value
 * with the `second` field of the provided `AM_TIMER_RTC_T` structure. The loop ensures
 * that the `rtc` structure is updated with the correct RTC time by waiting until the
 * `second` field matches the value read directly from the RTC. This synchronization
 * ensures that the `rtc` structure reflects the most accurate and up-to-date RTC time.
 *
 * @param rtc Pointer to an `AM_TIMER_RTC_T` structure that will be synchronized with the RTC.
 */
static void timer_rtc(AM_TIMER_RTC_T *rtc) {
  int tmp;
  do {
    read_rtc_async(rtc);
    tmp = read_rtc(0);
  } while (tmp != rtc->second);
}

/**
 * Calculates the uptime in microseconds based on the current timestamp counter (TSC) value.
 * 
 * This method computes the elapsed time since the system started by comparing the current TSC value
 * with a previously recorded TSC value (`uptsc`). The difference is then divided by the CPU frequency
 * in MHz (`freq_mhz`) to convert the TSC ticks into microseconds. The result is stored in the `us`
 * field of the provided `AM_TIMER_UPTIME_T` structure.
 * 
 * @param upt Pointer to an `AM_TIMER_UPTIME_T` structure where the calculated uptime in microseconds
 *            will be stored.
 */
static void timer_uptime(AM_TIMER_UPTIME_T *upt) {
  upt->us = (rdtsc() - uptsc) / freq_mhz;
}

// Input
// ====================================================

static int keylut[128] = {
  [0x01] = AM_KEY_ESCAPE,               [0x02] = AM_KEY_1, [0x03] = AM_KEY_2,
  [0x04] = AM_KEY_3, [0x05] = AM_KEY_4, [0x06] = AM_KEY_5, [0x07] = AM_KEY_6,
  [0x08] = AM_KEY_7, [0x09] = AM_KEY_8, [0x0a] = AM_KEY_9, [0x0b] = AM_KEY_0,
  [0x0c] = AM_KEY_MINUS,                [0x0d] = AM_KEY_EQUALS,
  [0x0e] = AM_KEY_BACKSPACE,            [0x0f] = AM_KEY_TAB,
  [0x10] = AM_KEY_Q, [0x11] = AM_KEY_W, [0x12] = AM_KEY_E, [0x13] = AM_KEY_R,
  [0x14] = AM_KEY_T, [0x15] = AM_KEY_Y, [0x16] = AM_KEY_U, [0x17] = AM_KEY_I,
  [0x18] = AM_KEY_O, [0x19] = AM_KEY_P, [0x1a] = AM_KEY_LEFTBRACKET,
  [0x1b] = AM_KEY_RIGHTBRACKET,         [0x1c] = AM_KEY_RETURN,
  [0x1d] = AM_KEY_LCTRL,                [0x1e] = AM_KEY_A, [0x1f] = AM_KEY_S,
  [0x20] = AM_KEY_D, [0x21] = AM_KEY_F, [0x22] = AM_KEY_G, [0x23] = AM_KEY_H,
  [0x24] = AM_KEY_J, [0x25] = AM_KEY_K, [0x26] = AM_KEY_L,
  [0x27] = AM_KEY_SEMICOLON,            [0x28] = AM_KEY_APOSTROPHE,
  [0x29] = AM_KEY_GRAVE,                [0x2a] = AM_KEY_LSHIFT,
  [0x2b] = AM_KEY_BACKSLASH,            [0x2c] = AM_KEY_Z, [0x2d] = AM_KEY_X,
  [0x2e] = AM_KEY_C, [0x2f] = AM_KEY_V, [0x30] = AM_KEY_B, [0x31] = AM_KEY_N,
  [0x32] = AM_KEY_M,     [0x33] = AM_KEY_COMMA,  [0x34] = AM_KEY_PERIOD,
  [0x35] = AM_KEY_SLASH, [0x36] = AM_KEY_RSHIFT, [0x38] = AM_KEY_LALT,
  [0x38] = AM_KEY_RALT,  [0x39] = AM_KEY_SPACE,  [0x3a] = AM_KEY_CAPSLOCK,
  [0x3b] = AM_KEY_F1,    [0x3c] = AM_KEY_F2,     [0x3d] = AM_KEY_F3,
  [0x3e] = AM_KEY_F4,    [0x3f] = AM_KEY_F5,     [0x40] = AM_KEY_F6,
  [0x41] = AM_KEY_F7,    [0x42] = AM_KEY_F8,     [0x43] = AM_KEY_F9,
  [0x44] = AM_KEY_F10,   [0x48] = AM_KEY_INSERT,
  [0x4b] = AM_KEY_HOME,  [0x4d] = AM_KEY_END,    [0x50] = AM_KEY_DELETE,
  [0x57] = AM_KEY_F11,   [0x58] = AM_KEY_F12,    [0x5b] = AM_KEY_APPLICATION,
};

/**
 * @brief Configures the input settings by marking the input as present.
 *
 * This method initializes the input configuration structure by setting the
 * `present` field to `true`. It is typically used to indicate that the input
 * is available and ready for use.
 *
 * @param cfg Pointer to the input configuration structure of type `AM_INPUT_CONFIG_T`.
 */
static void input_config(AM_INPUT_CONFIG_T *cfg) {
  cfg->present = true;
}

/**
 * Processes keyboard input by reading the keyboard status and scan code from the
 * hardware ports. The method checks if there is a pending keyboard event by
 * reading the status register at port 0x64. If a key event is detected, it reads
 * the scan code from port 0x60 and determines whether the key is pressed or
 * released based on the scan code value. The key event is then stored in the
 * provided `AM_INPUT_KEYBRD_T` structure, setting the `keydown` flag and
 * translating the scan code to a key code using the `keylut` lookup table. If
 * no key event is pending, the structure is updated to indicate no key event
 * (`AM_KEY_NONE`).
 *
 * @param ev Pointer to the `AM_INPUT_KEYBRD_T` structure where the keyboard
 *           event details will be stored.
 */
static void input_keybrd(AM_INPUT_KEYBRD_T *ev) {
  if (inb(0x64) & 0x1) {
    int code = inb(0x60) & 0xff;
    ev->keydown = code < 128;
    ev->keycode = keylut[code & 0x7f];
  } else {
    ev->keydown = false;
    ev->keycode = AM_KEY_NONE;
  }
}

// GPU (Frame Buffer and 2D Accelerated Graphics)
// ====================================================

#define VMEM_SIZE (512 << 10)

struct vbe_info {
  uint8_t  ignore[18];
  uint16_t width;
  uint16_t height;
  uint8_t  ignore1[18];
  uint32_t framebuffer;
} __attribute__ ((packed));

/**
 * Extracts the red component from a 32-bit packed RGB color value.
 *
 * This function takes a 32-bit unsigned integer representing a packed RGB color
 * value and returns the red component by shifting the value 16 bits to the right.
 * The input value is assumed to be in the format 0x00RRGGBB, where RR represents
 * the red component, GG represents the green component, and BB represents the
 * blue component.
 *
 * @param p A 32-bit unsigned integer representing the packed RGB color value.
 * @return The red component as an 8-bit unsigned integer.
 */
static inline uint8_t R(uint32_t p) { return p >> 16; }
/**
 * Extracts the second byte (bits 8 to 15) from a 32-bit unsigned integer.
 * 
 * This function shifts the input value `p` right by 8 bits and returns the result
 * as an 8-bit unsigned integer. This effectively extracts the second byte from
 * the 32-bit integer.
 *
 * @param p The 32-bit unsigned integer from which to extract the second byte.
 * @return The second byte of `p` as an 8-bit unsigned integer.
 */
static inline uint8_t G(uint32_t p) { return p >> 8; }
/**
 * Extracts the least significant byte (8 bits) from a 32-bit unsigned integer.
 *
 * This function takes a 32-bit unsigned integer as input and returns its least
 * significant byte. The operation is equivalent to masking the input with 0xFF
 * and casting it to an 8-bit unsigned integer.
 *
 * @param p The 32-bit unsigned integer from which to extract the byte.
 * @return The least significant byte of the input integer as an 8-bit unsigned integer.
 */
static inline uint8_t B(uint32_t p) { return p; }

struct pixel {
  uint8_t b, g, r;
} __attribute__ ((packed));

static struct pixel *fb;
static uint8_t vmem[VMEM_SIZE], vbuf[VMEM_SIZE], *vbuf_head;

static struct gpu_canvas display;
/**
 * Converts a GPU pointer to a host pointer.
 *
 * This method takes a GPU pointer (`ptr`) and converts it to a corresponding host pointer.
 * If the GPU pointer is `AM_GPU_NULL`, the method returns `NULL`. Otherwise, it calculates
 * the host pointer by adding the GPU pointer offset (`ptr`) to the base host memory address (`vmem`).
 *
 * @param ptr The GPU pointer to be converted. If `ptr` is `AM_GPU_NULL`, the method returns `NULL`.
 * @return A host pointer corresponding to the GPU pointer, or `NULL` if `ptr` is `AM_GPU_NULL`.
 */
static inline void *to_host(gpuptr_t ptr) { return ptr == AM_GPU_NULL ? NULL : vmem + ptr; }

/**
 * Initializes the GPU by retrieving the display information from the VBE (VESA BIOS Extensions) 
 * info structure located at the fixed memory address 0x00004000. The method sets the display 
 * width and height based on the retrieved information and maps the framebuffer address to the 
 * global `fb` pointer for future use.
 *
 * The VBE info structure is expected to contain the following fields:
 * - `width`: The width of the display in pixels.
 * - `height`: The height of the display in pixels.
 * - `framebuffer`: The physical address of the framebuffer.
 *
 * This method assumes that the VBE info structure is correctly populated and accessible at the 
 * specified memory address. It does not perform any error checking or validation.
 */
static void gpu_init() {
  struct vbe_info *info = (struct vbe_info *)0x00004000;
  display.w = info->width;
  display.h = info->height;
  fb = (void *)((intptr_t)(info->framebuffer));
}

/**
 * Configures the GPU settings by initializing the provided AM_GPU_CONFIG_T structure.
 * The structure is populated with the following values:
 * - `present`: Set to `true` to indicate that the GPU is present and active.
 * - `width`: Set to the width of the display, obtained from `display.w`.
 * - `height`: Set to the height of the display, obtained from `display.h`.
 * - `vmemsz`: Set to the size of the video memory, calculated as `sizeof(vmem)`.
 *
 * @param cfg A pointer to an AM_GPU_CONFIG_T structure that will be populated with the GPU configuration.
 */
static void gpu_config(AM_GPU_CONFIG_T *cfg) {
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true,
    .width = display.w, .height = display.h,
    .vmemsz  = sizeof(vmem),
  };
}

/**
 * @brief Draws a rectangular region of pixels onto the framebuffer using GPU framebuffer draw data.
 *
 * This function takes a pointer to an `AM_GPU_FBDRAW_T` structure containing the pixel data and
 * drawing parameters. It draws a rectangular region of pixels onto the framebuffer, starting at the
 * specified coordinates (`x`, `y`) with the specified width (`w`) and height (`h`). The function
 * ensures that the drawing operation stays within the bounds of the display.
 *
 * @param draw Pointer to an `AM_GPU_FBDRAW_T` structure containing the following fields:
 *   - `x`: The x-coordinate of the top-left corner of the rectangle.
 *   - `y`: The y-coordinate of the top-left corner of the rectangle.
 *   - `w`: The width of the rectangle.
 *   - `h`: The height of the rectangle.
 *   - `pixels`: A pointer to the pixel data array, where each pixel is represented as a 32-bit integer.
 *
 * The function performs the following steps:
 * 1. Determines the effective width of the rectangle to ensure it does not exceed the display width.
 * 2. Iterates over each row of the rectangle, checking if the row is within the display height.
 * 3. For each pixel in the row, extracts the RGB components from the pixel data and writes them to
 *    the corresponding location in the framebuffer.
 *
 * The framebuffer is assumed to be a 2D array of `struct pixel` elements, where each `struct pixel`
 * contains `r`, `g`, and `b` fields representing the red, green, and blue components of the pixel.
 */
static void gpu_fbdraw(AM_GPU_FBDRAW_T *draw) {
  int x = draw->x, y = draw->y, w = draw->w, h = draw->h;
  int W = display.w, H = display.h;
  uint32_t *pixels = draw->pixels;
  int len = (x + w >= W) ? W - x : w;
  for (int j = 0; j < h; j ++, pixels += w) {
    if (y + j < H) {
      struct pixel *px = &fb[x + (j + y) * W];
      for (int i = 0; i < len; i ++, px ++) {
        uint32_t p = pixels[i];
        *px = (struct pixel) { .r = R(p), .g = G(p), .b = B(p) };
      }
    }
  }
}

/**
 * @brief Updates the GPU status to indicate that it is ready.
 *
 * This function sets the `ready` field of the provided `AM_GPU_STATUS_T` structure
 * to `true`, indicating that the GPU is in a ready state. This can be used to
 * signal that the GPU has completed initialization or is available for processing.
 *
 * @param stat Pointer to an `AM_GPU_STATUS_T` structure where the GPU status
 *             will be updated.
 */
static void gpu_status(AM_GPU_STATUS_T *stat) {
  stat->ready = true;
}

/**
 * Copies a block of memory from a GPU source to a host destination.
 *
 * This function takes a pointer to an `AM_GPU_MEMCPY_T` structure, which contains
 * the source memory address (`src`), the destination memory address (`dest`), and
 * the size of the memory block to be copied (`size`). The function maps the GPU
 * destination address to a host address using the `to_host` function and then
 * performs a byte-by-byte copy from the source to the destination.
 *
 * @param params Pointer to an `AM_GPU_MEMCPY_T` structure containing the source
 *               address, destination address, and size of the memory block to copy.
 */
static void gpu_memcpy(AM_GPU_MEMCPY_T *params) {
  char *src = params->src, *dst = to_host(params->dest);
  for (int i = 0; i < params->size; i++)
    dst[i] = src[i];
}

/**
 * Allocates a block of memory of the specified size from a pre-defined buffer.
 * 
 * This function reserves a contiguous block of memory of the given size from a global buffer
 * (`vbuf`). The memory is zero-initialized before being returned. The function maintains a
 * pointer (`vbuf_head`) to track the next available memory location in the buffer. If the
 * requested size exceeds the remaining space in the buffer, the function triggers a panic
 * with the message "no memory".
 *
 * @param size The size of the memory block to allocate, in bytes.
 * @return A pointer to the allocated memory block. The memory is guaranteed to be zero-initialized.
 * @note The function assumes that `vbuf` and `vbuf_head` are properly initialized elsewhere.
 *       It does not handle memory fragmentation or reallocation.
 */
static void *vbuf_alloc(int size) {
  void *ret = vbuf_head;
  vbuf_head += size;
  panic_on(vbuf_head > vbuf + sizeof(vbuf), "no memory");
  for (int i = 0; i < size; i++)
    ((char *)ret)[i] = 0;
  return ret;
}

/**
 * Renders a GPU canvas onto a pixel buffer.
 *
 * This function takes a GPU canvas (`cv`) and renders its contents onto a pixel buffer (`px`). 
 * The canvas can be either a texture or a subtree of canvases. If it is a texture, the texture's 
 * pixels are directly copied to the pixel buffer. If it is a subtree, the function recursively 
 * renders each child canvas onto a local pixel buffer, which is then composited into the final 
 * pixel buffer.
 *
 * The rendering process involves mapping the local canvas dimensions (`w`, `h`) to the target 
 * pixel buffer dimensions (`W`, `H`), ensuring proper scaling and positioning based on the 
 * canvas's coordinates (`x1`, `y1`, `w1`, `h1`).
 *
 * @param cv     The GPU canvas to render. This can be a texture or a subtree of canvases.
 * @param parent The parent GPU canvas, used to determine the dimensions of the target pixel buffer.
 * @param px     The pixel buffer where the rendered canvas will be stored.
 *
 * @return Always returns `0`. The rendered result is stored in the `px` buffer.
 */
static struct pixel *render(struct gpu_canvas *cv, struct gpu_canvas *parent, struct pixel *px) {
  struct pixel *px_local;
  int W = parent->w, w, h;

  switch (cv->type) {
    case AM_GPU_TEXTURE: {
      w = cv->texture.w; h = cv->texture.h;
      px_local = to_host(cv->texture.pixels);
      break;
    }
    case AM_GPU_SUBTREE: {
      w = cv->w; h = cv->h;
      px_local = vbuf_alloc(w * h * sizeof(struct pixel));
      for (struct gpu_canvas *ch = to_host(cv->child); ch; ch = to_host(ch->sibling)) {
        render(ch, cv, px_local);
      }
      break;
    }
    default:
      panic("invalid node");
  }

  // draw local canvas (w * h) -> px (x1, y1) - (x1 + w1, y1 + h1)
  for (int i = 0; i < cv->w1; i++)
    for (int j = 0; j < cv->h1; j++) {
      int x = cv->x1 + i, y = cv->y1 + j;
      px[W * y + x] = px_local[w * (j * h / cv->h1) + (i * w / cv->w1)];
    }
  return 0;
}

/**
 * @brief Renders the scene using the GPU based on the provided render configuration.
 *
 * This function initializes the vertex buffer head to the start of the vertex buffer
 * and then proceeds to render the scene. The rendering process involves converting
 * the root node of the scene graph to a host-compatible format and then passing it,
 * along with the display configuration and framebuffer, to the render function.
 *
 * @param ren Pointer to the AM_GPU_RENDER_T structure containing the render configuration,
 *            including the root node of the scene graph.
 */
static void gpu_render(AM_GPU_RENDER_T *ren) {
  vbuf_head = vbuf;
  render(to_host(ren->root), &display, fb);
}

// Disk (ATA0)
// ====================================================

#define BLKSZ  512
#define DISKSZ (64 << 20)

/**
 * Configures the disk settings by initializing the provided AM_DISK_CONFIG_T structure.
 * This method sets the disk as present, assigns the block size (blksz) to the predefined
 * value BLKSZ, and calculates the block count (blkcnt) based on the total disk size (DISKSZ)
 * divided by the block size (BLKSZ).
 *
 * @param cfg Pointer to the AM_DISK_CONFIG_T structure to be configured.
 */
static void disk_config(AM_DISK_CONFIG_T *cfg) {
  cfg->present = true;
  cfg->blksz   = BLKSZ;
  cfg->blkcnt  = DISKSZ / BLKSZ;
}

/**
 * @brief Updates the disk status to indicate that the disk is ready.
 *
 * This function sets the `ready` field of the provided `AM_DISK_STATUS_T` structure
 * to `true`, indicating that the disk is in a ready state and available for operations.
 *
 * @param status Pointer to an `AM_DISK_STATUS_T` structure where the disk status
 *               will be updated.
 */
static void disk_status(AM_DISK_STATUS_T *status) {
  status->ready = true;
}

/**
 * Waits until the disk is ready for the next command.
 * 
 * This function continuously polls the status register of the disk controller
 * (located at I/O port 0x1F7) until the disk is ready. The disk is considered
 * ready when the status register's bits 7 and 6 (0xC0 mask) are equal to 0x40,
 * indicating that the disk is not busy and is ready to accept commands.
 * 
 * The function uses the `inb` instruction to read the status register and
 * loops until the desired condition is met. This is typically used before
 * issuing a command to the disk to ensure the disk is in a ready state.
 */
static inline void wait_disk(void) {
  while ((inb(0x1f7) & 0xc0) != 0x40);
}

/**
 * @brief Performs block I/O operations on a disk.
 *
 * This function handles reading from or writing to a disk in blocks. It takes a pointer
 * to an AM_DISK_BLKIO_T structure, which contains the necessary information for the I/O
 * operation, such as the block number, block count, buffer, and whether the operation is
 * a write or read.
 *
 * The function iterates over the specified number of blocks, performing the following steps:
 * 1. Waits for the disk to be ready.
 * 2. Sends the block number and operation command to the disk controller via I/O ports.
 * 3. Waits again for the disk to be ready.
 * 4. If the operation is a write, it sends the data from the buffer to the disk.
 * 5. If the operation is a read, it retrieves the data from the disk into the buffer.
 *
 * @param bio Pointer to an AM_DISK_BLKIO_T structure containing the block I/O details.
 */
static void disk_blkio(AM_DISK_BLKIO_T *bio) {
  uint32_t blkno = bio->blkno, remain = bio->blkcnt;
  uint32_t *ptr = bio->buf;
  for (remain = bio->blkcnt; remain; remain--, blkno++) {
    wait_disk();
    outb(0x1f2, 1);
    outb(0x1f3, blkno);
    outb(0x1f4, blkno >> 8);
    outb(0x1f5, blkno >> 16);
    outb(0x1f6, (blkno >> 24) | 0xe0);
    outb(0x1f7, bio->write? 0x30 : 0x20);
    wait_disk();
    if (bio->write) {
      for (int i = 0; i < BLKSZ / 4; i ++)
        outl(0x1f0, *ptr++);
    } else {
      for (int i = 0; i < BLKSZ / 4; i ++)
        *ptr++ = inl(0x1f0);
    }
  }
}

// ====================================================

static void audio_config(AM_AUDIO_CONFIG_T *cfg) { cfg->present = false; }
/**
 * @brief Configures the network settings by initializing the provided configuration structure.
 *
 * This function sets the `present` field of the `AM_NET_CONFIG_T` structure to `false`,
 * indicating that the network configuration is not currently active or available.
 *
 * @param cfg Pointer to the `AM_NET_CONFIG_T` structure to be configured.
 */
static void net_config(AM_NET_CONFIG_T *cfg) { cfg->present = false; }
/**
 * Triggers a system panic with a message indicating an attempt to access a 
 * non-existent register. This function is typically called when an invalid 
 * memory or hardware register access is detected, signaling a critical error 
 * in the system.
 *
 * @param buf A pointer to the buffer or memory location that was accessed. 
 *            This parameter is not used in the function but is provided for 
 *            context or debugging purposes.
 */
static void fail(void *buf) { panic("access nonexist register"); }

typedef void (*handler_t)(void *buf);
static void *lut[128] = {
  [AM_UART_CONFIG ] = uart_config,
  [AM_UART_TX     ] = uart_tx,
  [AM_UART_RX     ] = uart_rx,
  [AM_TIMER_CONFIG] = timer_config,
  [AM_TIMER_RTC   ] = timer_rtc,
  [AM_TIMER_UPTIME] = timer_uptime,
  [AM_INPUT_CONFIG] = input_config,
  [AM_INPUT_KEYBRD] = input_keybrd,
  [AM_GPU_CONFIG  ] = gpu_config,
  [AM_GPU_FBDRAW  ] = gpu_fbdraw,
  [AM_GPU_STATUS  ] = gpu_status,
  [AM_GPU_MEMCPY  ] = gpu_memcpy,
  [AM_GPU_RENDER  ] = gpu_render,
  [AM_AUDIO_CONFIG] = audio_config,
  [AM_DISK_CONFIG ] = disk_config,
  [AM_DISK_STATUS ] = disk_status,
  [AM_DISK_BLKIO  ] = disk_blkio,
  [AM_NET_CONFIG  ] = net_config,
};


/**
 * Initializes the Input/Output Environment (IOE) for the system.
 * This function ensures that the initialization is performed only by the bootstrap CPU (CPU 0).
 * If called by any other CPU, it triggers a panic to prevent unintended behavior.
 * 
 * The function performs the following steps:
 * 1. Verifies that the current CPU is the bootstrap CPU (CPU 0). If not, it panics with an error message.
 * 2. Iterates over a lookup table (lut) and initializes any uninitialized entries to a predefined failure state (fail).
 * 3. Initializes the UART (Universal Asynchronous Receiver-Transmitter) for serial communication.
 * 4. Initializes the system timer for timekeeping and scheduling purposes.
 * 5. Initializes the GPU (Graphics Processing Unit) for graphical operations.
 * 
 * @return bool Returns `true` upon successful initialization of the IOE.
 */
bool ioe_init() {
  panic_on(cpu_current() != 0, "init IOE in non-bootstrap CPU");

  for (int i = 0; i < LENGTH(lut); i++)
    if (!lut[i]) lut[i] = fail;

  uart_init();
  timer_init();
  gpu_init();

  return true;
}

/**
 * Reads data from a specified register and stores it in the provided buffer.
 *
 * This method uses a lookup table (LUT) to determine the appropriate handler function
 * based on the register number. The handler function is then invoked with the buffer
 * as an argument, allowing the data from the register to be read and stored in the buffer.
 *
 * @param reg The register number from which to read the data.
 * @param buf A pointer to the buffer where the read data will be stored.
 */
void ioe_read (int reg, void *buf) { ((handler_t)lut[reg])(buf); }
/**
 * Writes data to a specified register using a handler function from the lookup table.
 * 
 * This function retrieves the appropriate handler function from the lookup table `lut` 
 * based on the provided register index `reg`. It then invokes the handler function, 
 * passing the data buffer `buf` as an argument. The handler function is responsible 
 * for performing the actual write operation to the register.
 *
 * @param reg The index of the register in the lookup table `lut`.
 * @param buf A pointer to the data buffer containing the data to be written.
 */
void ioe_write(int reg, void *buf) { ((handler_t)lut[reg])(buf); }

// LAPIC/IOAPIC (from xv6)

#define ID      (0x0020/4)   // ID
#define VER     (0x0030/4)   // Version
#define TPR     (0x0080/4)   // Task Priority
#define EOI     (0x00B0/4)   // EOI
#define SVR     (0x00F0/4)   // Spurious Interrupt Vector
  #define ENABLE     0x00000100   // Unit Enable
#define ESR     (0x0280/4)   // Error Status
#define ICRLO   (0x0300/4)   // Interrupt Command
  #define INIT       0x00000500   // INIT/RESET
  #define STARTUP    0x00000600   // Startup IPI
  #define DELIVS     0x00001000   // Delivery status
  #define ASSERT     0x00004000   // Assert interrupt (vs deassert)
  #define DEASSERT   0x00000000
  #define LEVEL      0x00008000   // Level triggered
  #define BCAST      0x00080000   // Send to all APICs, including self.
  #define BUSY       0x00001000
  #define FIXED      0x00000000
#define ICRHI   (0x0310/4)   // Interrupt Command [63:32]
#define TIMER   (0x0320/4)   // Local Vector Table 0 (TIMER)
  #define X1         0x0000000B   // divide counts by 1
  #define PERIODIC   0x00020000   // Periodic
#define PCINT   (0x0340/4)   // Performance Counter LVT
#define LINT0   (0x0350/4)   // Local Vector Table 1 (LINT0)
#define LINT1   (0x0360/4)   // Local Vector Table 2 (LINT1)
#define ERROR   (0x0370/4)   // Local Vector Table 3 (ERROR)
  #define MASKED     0x00010000   // Interrupt masked
#define TICR    (0x0380/4)   // Timer Initial Count
#define TCCR    (0x0390/4)   // Timer Current Count
#define TDCR    (0x03E0/4)   // Timer Divide Configuration

#define IOAPIC_ADDR  0xFEC00000   // Default physical address of IO APIC
#define REG_ID     0x00  // Register index: ID
#define REG_VER    0x01  // Register index: version
#define REG_TABLE  0x10  // Redirection table base

#define INT_DISABLED   0x00010000  // Interrupt disabled
#define INT_LEVEL      0x00008000  // Level-triggered (vs edge-)
#define INT_ACTIVELOW  0x00002000  // Active low (vs high)
#define INT_LOGICAL    0x00000800  // Destination is CPU id (vs APIC ID)

volatile unsigned int *__am_lapic = NULL;  // Initialized in mp.c
struct IOAPIC {
    uint32_t reg, pad[3], data;
} __attribute__((packed));
typedef struct IOAPIC IOAPIC;

static volatile IOAPIC *ioapic;

/**
 * Writes a value to the Local APIC (Advanced Programmable Interrupt Controller) register
 * specified by the index and then reads the ID register of the Local APIC.
 *
 * This function is used to configure or modify the Local APIC registers by writing
 * the provided value to the register at the specified index. After writing the value,
 * it reads the ID register of the Local APIC, which may be used to ensure the write
 * operation is completed or to trigger specific behavior in the APIC.
 *
 * @param index The index of the Local APIC register to which the value will be written.
 * @param value The value to be written to the specified Local APIC register.
 */
static void lapicw(int index, int value) {
  __am_lapic[index] = value;
  __am_lapic[ID];
}

/**
 * Initializes the Local APIC (Advanced Programmable Interrupt Controller) for the current CPU.
 * This function configures the APIC by setting up various registers to enable and configure
 * its operation. Specifically, it:
 * - Enables the APIC by setting the Spurious Interrupt Vector Register (SVR).
 * - Configures the Timer Divide Configuration Register (TDCR) to divide the clock by 1.
 * - Sets up the APIC timer in periodic mode and assigns an interrupt vector for the timer.
 * - Initializes the Timer Initial Count Register (TICR) with a specific value to start the timer.
 * - Masks the Local Interrupts (LINT0 and LINT1) to disable them.
 * - Masks the Performance Counter Interrupt (PCINT) if the APIC version is 4 or higher.
 * - Configures the Error Interrupt Register (ERROR) to handle APIC errors.
 * - Clears the Error Status Register (ESR) by writing to it twice.
 * - Sends an End of Interrupt (EOI) signal to acknowledge any pending interrupts.
 * - Initializes the Interrupt Command Register (ICR) to broadcast an INIT signal to all APICs.
 * - Waits for the delivery status to clear in the ICR.
 * - Sets the Task Priority Register (TPR) to 0, allowing all interrupts to be processed.
 */
void __am_percpu_initlapic(void) {
  lapicw(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));
  lapicw(TDCR, X1);
  lapicw(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
  lapicw(TICR, 10000000);
  lapicw(LINT0, MASKED);
  lapicw(LINT1, MASKED);
  if (((__am_lapic[VER]>>16) & 0xFF) >= 4)
    lapicw(PCINT, MASKED);
  lapicw(ERROR, T_IRQ0 + IRQ_ERROR);
  lapicw(ESR, 0);
  lapicw(ESR, 0);
  lapicw(EOI, 0);
  lapicw(ICRHI, 0);
  lapicw(ICRLO, BCAST | INIT | LEVEL);
  while(__am_lapic[ICRLO] & DELIVS) ;
  lapicw(TPR, 0);
}

/**
 * __am_lapic_eoi - Sends an End of Interrupt (EOI) signal to the Local APIC.
 *
 * This function checks if the Local APIC (__am_lapic) is available and, if so,
 * sends an EOI signal to it by writing a value of 0 to the EOI register. This
 * is typically done to acknowledge the completion of an interrupt handling
 * routine, allowing the APIC to process further interrupts.
 *
 * Note: This function assumes that the Local APIC is initialized and accessible.
 */
void __am_lapic_eoi(void) {
  if (__am_lapic)
    lapicw(EOI, 0);
}

/**
 * Boots an Application Processor (AP) using the Local APIC (Advanced Programmable Interrupt Controller).
 * This method initializes the AP by sending INIT and STARTUP IPIs (Inter-Processor Interrupts) to the target APIC ID.
 * The AP will start executing code from the specified memory address.
 *
 * @param apicid The APIC ID of the target processor to boot.
 * @param addr   The memory address (physical address) where the AP should begin execution.
 *
 * The method performs the following steps:
 * 1. Writes to the CMOS shutdown code port to prepare for the AP boot process.
 * 2. Sets up the warm reset vector (WRV) to point to the specified address.
 * 3. Sends an INIT IPI to the target APIC ID to reset the processor.
 * 4. Sends a STARTUP IPI to the target APIC ID to begin execution at the specified address.
 * 5. Repeats the STARTUP IPI to ensure the AP starts successfully.
 */
void __am_lapic_bootap(uint32_t apicid, void *addr) {
  int i;
  uint16_t *wrv;
  outb(0x70, 0xF);
  outb(0x71, 0x0A);
  wrv = (unsigned short*)((0x40<<4 | 0x67));
  wrv[0] = 0;
  wrv[1] = (uintptr_t)addr >> 4;

  lapicw(ICRHI, apicid<<24);
  lapicw(ICRLO, INIT | LEVEL | ASSERT);
  lapicw(ICRLO, INIT | LEVEL);

  for (i = 0; i < 2; i++){
    lapicw(ICRHI, apicid<<24);
    lapicw(ICRLO, STARTUP | ((uintptr_t)addr>>12));
  }
}

/**
 * Reads a value from the I/O Advanced Programmable Interrupt Controller (IOAPIC) register.
 * 
 * This function sets the specified register in the IOAPIC by writing the `reg` parameter
 * to the `ioapic->reg` field. It then reads and returns the value from the `ioapic->data`
 * field, which contains the data associated with the specified register.
 * 
 * @param reg The register index to read from the IOAPIC.
 * @return The value read from the specified IOAPIC register.
 */
static unsigned int ioapicread(int reg) {
  ioapic->reg = reg;
  return ioapic->data;
}

/**
 * Writes a value to a specified register in the I/O Advanced Programmable Interrupt Controller (IOAPIC).
 * 
 * This function sets the register address in the IOAPIC's register field and writes the provided data 
 * to the corresponding data field. The IOAPIC uses these fields to manage interrupt routing and configuration.
 *
 * @param reg  The register address within the IOAPIC where the data will be written.
 * @param data The value to be written to the specified register.
 */
static void ioapicwrite(int reg, unsigned int data) {
  ioapic->reg = reg;
  ioapic->data = data;
}

/**
 * Initializes the I/O Advanced Programmable Interrupt Controller (IOAPIC).
 * 
 * This function sets up the IOAPIC by configuring its interrupt table. It first 
 * retrieves the maximum number of interrupts supported by the IOAPIC by reading 
 * the version register. Then, it iterates over each interrupt entry in the table, 
 * disabling the interrupt and setting its vector to a predefined base value (T_IRQ0) 
 * plus the interrupt number. The high bits of each entry are cleared to ensure 
 * proper configuration.
 * 
 * The IOAPIC is accessed through a memory-mapped I/O address (IOAPIC_ADDR), and 
 * the configuration is performed by writing to specific registers (REG_TABLE) 
 * within the IOAPIC.
 * 
 * @note This function assumes that the IOAPIC is accessible at the predefined 
 *       address (IOAPIC_ADDR) and that the necessary register definitions 
 *       (REG_VER, REG_TABLE, INT_DISABLED, T_IRQ0) are available.
 */
void __am_ioapic_init(void) {
  int i, maxintr;

  ioapic = (volatile IOAPIC*)IOAPIC_ADDR;
  maxintr = (ioapicread(REG_VER) >> 16) & 0xFF;

  for (i = 0; i <= maxintr; i++){
    ioapicwrite(REG_TABLE+2*i, INT_DISABLED | (T_IRQ0 + i));
    ioapicwrite(REG_TABLE+2*i+1, 0);
  }
}

/**
 * Enables a specific IRQ (Interrupt Request) on the I/O Advanced Programmable Interrupt Controller (IOAPIC)
 * for a given CPU. This method configures the IOAPIC's interrupt redirection table to route the specified IRQ
 * to the designated CPU.
 *
 * @param irq    The interrupt request number to be enabled. This value is used to determine the entry in the
 *               IOAPIC's redirection table that will be configured.
 * @param cpunum The CPU number to which the IRQ should be routed. This value is shifted and used to set the
 *               destination field in the redirection table entry.
 *
 * The method performs the following steps:
 * 1. Writes the interrupt vector (T_IRQ0 + irq) to the first register of the redirection table entry for the
 *    specified IRQ. This sets the interrupt vector that will be used when the IRQ is triggered.
 * 2. Writes the CPU number (shifted left by 24 bits) to the second register of the redirection table entry.
 *    This sets the destination CPU for the IRQ, ensuring that the interrupt is routed to the correct processor.
 */
void __am_ioapic_enable(int irq, int cpunum) {
  ioapicwrite(REG_TABLE+2*irq, T_IRQ0 + irq);
  ioapicwrite(REG_TABLE+2*irq+1, cpunum << 24);
}
