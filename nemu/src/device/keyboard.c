/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <device/map.h>
#include <utils.h>

#define KEYDOWN_MASK 0x8000

#ifndef CONFIG_TARGET_AM
#include <SDL2/SDL.h>

// Note that this is not the standard
#define NEMU_KEYS(f) \
  f(ESCAPE) f(F1) f(F2) f(F3) f(F4) f(F5) f(F6) f(F7) f(F8) f(F9) f(F10) f(F11) f(F12) \
f(GRAVE) f(1) f(2) f(3) f(4) f(5) f(6) f(7) f(8) f(9) f(0) f(MINUS) f(EQUALS) f(BACKSPACE) \
f(TAB) f(Q) f(W) f(E) f(R) f(T) f(Y) f(U) f(I) f(O) f(P) f(LEFTBRACKET) f(RIGHTBRACKET) f(BACKSLASH) \
f(CAPSLOCK) f(A) f(S) f(D) f(F) f(G) f(H) f(J) f(K) f(L) f(SEMICOLON) f(APOSTROPHE) f(RETURN) \
f(LSHIFT) f(Z) f(X) f(C) f(V) f(B) f(N) f(M) f(COMMA) f(PERIOD) f(SLASH) f(RSHIFT) \
f(LCTRL) f(APPLICATION) f(LALT) f(SPACE) f(RALT) f(RCTRL) \
f(UP) f(DOWN) f(LEFT) f(RIGHT) f(INSERT) f(DELETE) f(HOME) f(END) f(PAGEUP) f(PAGEDOWN)

#define NEMU_KEY_NAME(k) NEMU_KEY_ ## k,

enum {
  NEMU_KEY_NONE = 0,
  MAP(NEMU_KEYS, NEMU_KEY_NAME)
};

#define SDL_KEYMAP(k) keymap[SDL_SCANCODE_ ## k] = NEMU_KEY_ ## k;
static uint32_t keymap[256] = {};

/**
 * Initializes the keymap by mapping NEMU keys to their corresponding SDL keycodes.
 * This function uses the `MAP` macro to associate the predefined NEMU keys with
 * the SDL keymap. The keymap is essential for translating keyboard inputs into
 * the appropriate actions or commands within the NEMU environment.
 */
static void init_keymap() {
  MAP(NEMU_KEYS, SDL_KEYMAP)
}

#define KEY_QUEUE_LEN 1024
static int key_queue[KEY_QUEUE_LEN] = {};
static int key_f = 0, key_r = 0;

/**
 * Enqueues a scancode into the key queue.
 * 
 * This method adds the provided scancode to the key queue at the position
 * indicated by `key_r`. The `key_r` index is then incremented and wrapped around
 * using modulo arithmetic to ensure it stays within the bounds of the queue.
 * If the queue is full (i.e., `key_r` equals `key_f`), an assertion failure
 * is triggered with the message "key queue overflow!".
 * 
 * @param am_scancode The scancode to be enqueued, represented as a 32-bit unsigned integer.
 */
static void key_enqueue(uint32_t am_scancode) {
  key_queue[key_r] = am_scancode;
  key_r = (key_r + 1) % KEY_QUEUE_LEN;
  Assert(key_r != key_f, "key queue overflow!");
}

/**
 * Dequeues a key from the key queue and returns it.
 *
 * This method retrieves the next key from the key queue if the queue is not empty.
 * The queue is considered empty if the front index (`key_f`) is equal to the rear index (`key_r`).
 * If the queue is not empty, the key at the front of the queue is returned, and the front index is
 * incremented, wrapping around to the start of the queue if necessary.
 *
 * If the queue is empty, the method returns `NEMU_KEY_NONE`, indicating no key is available.
 *
 * @return The dequeued key as a `uint32_t` value. Returns `NEMU_KEY_NONE` if the queue is empty.
 */
static uint32_t key_dequeue() {
  uint32_t key = NEMU_KEY_NONE;
  if (key_f != key_r) {
    key = key_queue[key_f];
    key_f = (key_f + 1) % KEY_QUEUE_LEN;
  }
  return key;
}

/**
 * Sends a key event to the NEMU (NJU Emulator) based on the provided scancode and key state.
 * 
 * This method checks if the NEMU is in a running state and if the scancode maps to a valid key
 * in the `keymap`. If both conditions are met, it constructs a key event by combining the mapped
 * key value with a key state mask (indicating whether the key is pressed or released). The
 * constructed key event is then enqueued for processing by the NEMU.
 *
 * @param scancode The scancode of the key event, representing the physical key pressed or released.
 * @param is_keydown A boolean indicating whether the key is being pressed (`true`) or released (`false`).
 */
void send_key(uint8_t scancode, bool is_keydown) {
  if (nemu_state.state == NEMU_RUNNING && keymap[scancode] != NEMU_KEY_NONE) {
    uint32_t am_scancode = keymap[scancode] | (is_keydown ? KEYDOWN_MASK : 0);
    key_enqueue(am_scancode);
  }
}
#else // !CONFIG_TARGET_AM
#define NEMU_KEY_NONE 0

/**
 * Dequeues and processes a keyboard event from the input device.
 *
 * This method reads a keyboard event from the input device using the `io_read` function,
 * which returns an event of type `AM_INPUT_KEYBRD_T`. The method then processes the event
 * by combining the keycode with a mask that indicates whether the key was pressed or released.
 * The keycode is combined with the `KEYDOWN_MASK` if the key was pressed (`ev.keydown` is true),
 * otherwise, the keycode is returned as is. The resulting value is a 32-bit unsigned integer
 * that encodes both the keycode and the key state (down or up).
 *
 * @return A 32-bit unsigned integer representing the processed keyboard event. The lower bits
 *         contain the keycode, and the highest bit is set to 1 if the key was pressed, or 0 if
 *         the key was released.
 */
static uint32_t key_dequeue() {
  AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
  uint32_t am_scancode = ev.keycode | (ev.keydown ? KEYDOWN_MASK : 0);
  return am_scancode;
}
#endif

static uint32_t *i8042_data_port_base = NULL;

/**
 * Handles I/O operations for the i8042 data port.
 * 
 * This method is responsible for processing read operations from the i8042 data port.
 * It ensures that the operation is a read (not a write) and that the offset is 0.
 * If these conditions are met, it dequeues a key from the keyboard buffer and 
 * stores it in the first element of the `i8042_data_port_base` array.
 *
 * @param offset The offset within the i8042 data port. Must be 0.
 * @param len The length of the data to be read. Not used in this implementation.
 * @param is_write Indicates whether the operation is a write. Must be false.
 */
static void i8042_data_io_handler(uint32_t offset, int len, bool is_write) {
  assert(!is_write);
  assert(offset == 0);
  i8042_data_port_base[0] = key_dequeue();
}

/**
 * Initializes the i8042 keyboard controller by allocating memory for the data port
 * and setting up the appropriate I/O mapping based on the configuration.
 *
 * This function performs the following steps:
 * 1. Allocates 4 bytes of memory for the i8042 data port base and assigns it to
 *    `i8042_data_port_base`.
 * 2. Initializes the first byte of the allocated memory to `NEMU_KEY_NONE` to
 *    indicate no key is pressed initially.
 * 3. Depending on the configuration (`CONFIG_HAS_PORT_IO`), it either maps the
 *    keyboard data port to a port I/O address or to a memory-mapped I/O address
 *    using `add_pio_map` or `add_mmio_map`, respectively. The mapping includes
 *    the handler `i8042_data_io_handler` for processing I/O operations.
 * 4. If the target is not Abstract Machine (`CONFIG_TARGET_AM`), it initializes
 *    the keymap by calling `init_keymap()`.
 *
 * This function is essential for setting up the keyboard controller for
 * subsequent input handling in the system.
 */
void init_i8042() {
  i8042_data_port_base = (uint32_t *)new_space(4);
  i8042_data_port_base[0] = NEMU_KEY_NONE;
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("keyboard", CONFIG_I8042_DATA_PORT, i8042_data_port_base, 4, i8042_data_io_handler);
#else
  add_mmio_map("keyboard", CONFIG_I8042_DATA_MMIO, i8042_data_port_base, 4, i8042_data_io_handler);
#endif
  IFNDEF(CONFIG_TARGET_AM, init_keymap());
}
