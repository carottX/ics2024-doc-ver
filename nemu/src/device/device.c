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

#include <common.h>
#include <utils.h>
#include <device/alarm.h>
#ifndef CONFIG_TARGET_AM
#include <SDL2/SDL.h>
#endif

void init_map();
void init_serial();
void init_timer();
void init_vga();
void init_i8042();
void init_audio();
void init_disk();
void init_sdcard();
void init_alarm();

void send_key(uint8_t, bool);
void vga_update_screen();

/**
 * Updates the device state based on the elapsed time and handles input events.
 * 
 * This method ensures that device updates occur at a frequency defined by `TIMER_HZ`.
 * It calculates the time elapsed since the last update and only proceeds if the elapsed
 * time exceeds the interval defined by `1000000 / TIMER_HZ`. If the condition is met,
 * the method updates the last update timestamp and performs the following actions:
 * 
 * - If `CONFIG_HAS_VGA` is defined, it updates the VGA screen.
 * - If `CONFIG_TARGET_AM` is not defined, it polls SDL events and handles them:
 *   - If the event is `SDL_QUIT`, it sets the NEMU state to `NEMU_QUIT`.
 *   - If `CONFIG_HAS_KEYBOARD` is defined, it processes keyboard events (`SDL_KEYDOWN`
 *     and `SDL_KEYUP`) by sending the key code and its state (pressed or released) to
 *     the appropriate handler.
 * 
 * This method is designed to be called periodically to ensure smooth device operation
 * and responsive input handling.
 */
void device_update() {
  static uint64_t last = 0;
  uint64_t now = get_time();
  if (now - last < 1000000 / TIMER_HZ) {
    return;
  }
  last = now;

  IFDEF(CONFIG_HAS_VGA, vga_update_screen());

#ifndef CONFIG_TARGET_AM
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        nemu_state.state = NEMU_QUIT;
        break;
#ifdef CONFIG_HAS_KEYBOARD
      // If a key was pressed
      case SDL_KEYDOWN:
      case SDL_KEYUP: {
        uint8_t k = event.key.keysym.scancode;
        bool is_keydown = (event.key.type == SDL_KEYDOWN);
        send_key(k, is_keydown);
        break;
      }
#endif
      default: break;
    }
  }
#endif
}

/**
 * Clears the SDL event queue by continuously polling events until the queue is empty.
 * This function is particularly useful for ensuring that no pending events are left
 * in the queue before proceeding with further operations. It is conditionally compiled
 * and will only execute if the macro `CONFIG_TARGET_AM` is not defined.
 * 
 * @note This function does not handle or process the events; it simply removes them
 * from the queue. If event handling is required, additional logic should be implemented
 * outside of this function.
 */
void sdl_clear_event_queue() {
#ifndef CONFIG_TARGET_AM
  SDL_Event event;
  while (SDL_PollEvent(&event));
#endif
}

/**
 * Initializes the device by setting up various hardware components based on the 
 * configuration options. This function checks for the presence of specific 
 * hardware features (e.g., serial, timer, VGA, keyboard, audio, disk, SD card) 
 * and initializes them if they are enabled in the configuration. Additionally, 
 * it initializes the I/O emulation if the target is Abstract Machine (AM) and 
 * sets up an alarm if the target is not AM.
 *
 * The function uses conditional compilation directives (IFDEF and IFNDEF) to 
 * include or exclude initialization code for specific hardware components based 
 * on the configuration settings. This allows the function to be flexible and 
 * adaptable to different hardware setups.
 *
 * Supported initialization steps include:
 * - I/O emulation (if CONFIG_TARGET_AM is defined)
 * - Memory mapping
 * - Serial port (if CONFIG_HAS_SERIAL is defined)
 * - Timer (if CONFIG_HAS_TIMER is defined)
 * - VGA display (if CONFIG_HAS_VGA is defined)
 * - Keyboard (if CONFIG_HAS_KEYBOARD is defined)
 * - Audio (if CONFIG_HAS_AUDIO is defined)
 * - Disk (if CONFIG_HAS_DISK is defined)
 * - SD card (if CONFIG_HAS_SDCARD is defined)
 * - Alarm (if CONFIG_TARGET_AM is not defined)
 */
void init_device() {
  IFDEF(CONFIG_TARGET_AM, ioe_init());
  init_map();

  IFDEF(CONFIG_HAS_SERIAL, init_serial());
  IFDEF(CONFIG_HAS_TIMER, init_timer());
  IFDEF(CONFIG_HAS_VGA, init_vga());
  IFDEF(CONFIG_HAS_KEYBOARD, init_i8042());
  IFDEF(CONFIG_HAS_AUDIO, init_audio());
  IFDEF(CONFIG_HAS_DISK, init_disk());
  IFDEF(CONFIG_HAS_SDCARD, init_sdcard());

  IFNDEF(CONFIG_TARGET_AM, init_alarm());
}
