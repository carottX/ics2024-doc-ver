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

#include <utils.h>
#include <device/map.h>

/* http://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming */
// NOTE: this is compatible to 16550

#define CH_OFFSET 0

static uint8_t *serial_base = NULL;


/**
 * Outputs a single character to the appropriate output stream based on the configuration.
 * 
 * If the target is configured as AM (CONFIG_TARGET_AM), the character is sent to the AM-specific 
 * output handler using `putch`. Otherwise, the character is sent to the standard error stream 
 * (stderr) using `putc`.
 * 
 * @param ch The character to be output.
 */
static void serial_putc(char ch) {
  MUXDEF(CONFIG_TARGET_AM, putch(ch), putc(ch, stderr));
}

/**
 * Handles serial I/O operations based on the provided offset and length.
 * This function is responsible for managing read and write operations to the serial port.
 * It ensures that the length of the operation is exactly 1 byte, as serial I/O typically
 * operates on a byte-by-byte basis. The function binds the serial port to the host's stderr
 * in the NEMU (NJU Emulator) environment.
 *
 * @param offset The offset within the serial port's address space to perform the I/O operation.
 * @param len The length of the data to be read or written. Must be exactly 1 byte.
 * @param is_write A boolean flag indicating whether the operation is a write (true) or read (false).
 * 
 * @note This function asserts that the length of the operation is exactly 1 byte. If the length
 *       is not 1, the program will terminate with an assertion failure.
 * @note If the operation is a write, the function writes the byte from the serial_base array to the
 *       serial port. If the operation is a read, the function panics, as read operations are not supported.
 * @note If the provided offset is not recognized, the function panics with an error message indicating
 *       the unsupported offset.
 */
static void serial_io_handler(uint32_t offset, int len, bool is_write) {
  assert(len == 1);
  switch (offset) {
    /* We bind the serial port with the host stderr in NEMU. */
    case CH_OFFSET:
      if (is_write) serial_putc(serial_base[0]);
      else panic("do not support read");
      break;
    default: panic("do not support offset = %d", offset);
  }
}

/**
 * Initializes the serial communication interface by allocating memory for the serial base
 * and mapping it to either a port I/O or memory-mapped I/O address, depending on the system
 * configuration. If the system supports port I/O (CONFIG_HAS_PORT_IO is defined), the serial
 * interface is mapped to the specified port address (CONFIG_SERIAL_PORT). Otherwise, it is
 * mapped to a memory-mapped I/O address (CONFIG_SERIAL_MMIO). The serial_io_handler function
 * is registered as the handler for I/O operations on the serial interface.
 */
void init_serial() {
  serial_base = new_space(8);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("serial", CONFIG_SERIAL_PORT, serial_base, 8, serial_io_handler);
#else
  add_mmio_map("serial", CONFIG_SERIAL_MMIO, serial_base, 8, serial_io_handler);
#endif
}
