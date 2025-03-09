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
#include <device/alarm.h>
#include <utils.h>

static uint32_t *rtc_port_base = NULL;

/**
 * Handles I/O operations for the RTC (Real-Time Clock) device.
 * 
 * This method processes read and write requests to the RTC device's memory-mapped I/O ports.
 * It ensures that the offset is valid (either 0 or 4) and, if the operation is a read request
 * to offset 4, it retrieves the current time in microseconds and writes it to the RTC port base.
 * The lower 32 bits of the time value are written to `rtc_port_base[0]`, and the upper 32 bits
 * are written to `rtc_port_base[1]`.
 *
 * @param offset The memory offset within the RTC device's I/O port (must be 0 or 4).
 * @param len The length of the data being read or written (not used in this implementation).
 * @param is_write Indicates whether the operation is a write (true) or read (false).
 */
static void rtc_io_handler(uint32_t offset, int len, bool is_write) {
  assert(offset == 0 || offset == 4);
  if (!is_write && offset == 4) {
    uint64_t us = get_time();
    rtc_port_base[0] = (uint32_t)us;
    rtc_port_base[1] = us >> 32;
  }
}

#ifndef CONFIG_TARGET_AM
/**
 * @brief Handles the timer interrupt.
 *
 * This function is invoked when a timer interrupt occurs. It checks if the NEMU (NJU Emulator)
 * state is set to NEMU_RUNNING. If so, it raises a device interrupt by calling the `dev_raise_intr()`
 * function. This is typically used to simulate hardware interrupts in an emulated environment.
 * The function does nothing if the NEMU state is not NEMU_RUNNING.
 */
static void timer_intr() {
  if (nemu_state.state == NEMU_RUNNING) {
    extern void dev_raise_intr();
    dev_raise_intr();
  }
}
#endif

/**
 * @brief Initializes the timer by setting up the RTC (Real-Time Clock) port or MMIO (Memory-Mapped I/O) 
 *        mapping and configuring the timer interrupt handler.
 *
 * This function allocates memory for the RTC port base and maps it either as a port I/O or MMIO 
 * depending on the configuration. If the system supports port I/O (CONFIG_HAS_PORT_IO is defined), 
 * it maps the RTC port using the `add_pio_map` function. Otherwise, it uses the `add_mmio_map` function 
 * for MMIO mapping. Additionally, if the target is not the AM (Abstract Machine), it sets up an alarm 
 * handler for the timer interrupt using `add_alarm_handle`.
 *
 * The RTC port or MMIO is mapped with a size of 8 bytes, and the `rtc_io_handler` function is specified 
 * as the I/O handler for the mapped region.
 */
void init_timer() {
  rtc_port_base = (uint32_t *)new_space(8);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("rtc", CONFIG_RTC_PORT, rtc_port_base, 8, rtc_io_handler);
#else
  add_mmio_map("rtc", CONFIG_RTC_MMIO, rtc_port_base, 8, rtc_io_handler);
#endif
  IFNDEF(CONFIG_TARGET_AM, add_alarm_handle(timer_intr));
}
