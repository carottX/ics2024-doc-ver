#include <am.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

/**
 * Initializes the UART (Universal Asynchronous Receiver-Transmitter) by setting the standard input
 * file descriptor (STDIN_FILENO) to non-blocking mode. This ensures that read operations on the
 * standard input do not block the program execution if no data is available.
 *
 * The method performs the following steps:
 * 1. Retrieves the current file status flags of the standard input using `fcntl` with the `F_GETFL` command.
 * 2. Adds the `O_NONBLOCK` flag to the retrieved flags to enable non-blocking mode.
 * 3. Sets the modified flags back to the standard input using `fcntl` with the `F_SETFL` command.
 *
 * If any of the `fcntl` operations fail (returning -1), the program will terminate with an assertion error.
 */
void __am_uart_init() {
  int ret = fcntl(STDIN_FILENO, F_GETFL);
  assert(ret != -1);
  int flag = ret | O_NONBLOCK;
  ret = fcntl(STDIN_FILENO, F_SETFL, flag);
  assert(ret != -1);
}

/**
 * Configures the UART (Universal Asynchronous Receiver/Transmitter) settings
 * by setting the 'present' field of the provided AM_UART_CONFIG_T structure to true.
 * This method is typically used to indicate that the UART hardware is present and
 * available for use in the system.
 *
 * @param cfg Pointer to an AM_UART_CONFIG_T structure that holds the UART configuration.
 *            The 'present' field of this structure will be set to true to indicate
 *            that the UART is present and ready for configuration or operation.
 */
void __am_uart_config(AM_UART_CONFIG_T *cfg) {
  cfg->present = true;
}

/**
 * Transmits a single character through the UART (Universal Asynchronous Receiver-Transmitter).
 * 
 * This function takes a pointer to an AM_UART_TX_T structure, which contains the data to be transmitted.
 * The character stored in the `data` field of the structure is sent to the standard output using the `putchar` function.
 * 
 * @param uart Pointer to an AM_UART_TX_T structure containing the character to be transmitted.
 */
void __am_uart_tx(AM_UART_TX_T *uart) {
  putchar(uart->data);
}

/**
 * Reads a single character from the standard input (stdin) and stores it in the provided
 * AM_UART_RX_T structure. If the end-of-file (EOF) is encountered, the data field in the
 * structure is set to -1.
 *
 * @param uart A pointer to an AM_UART_RX_T structure where the read character (or -1 in case
 *             of EOF) will be stored.
 */
void __am_uart_rx(AM_UART_RX_T *uart) {
  int ret = fgetc(stdin);
  if (ret == EOF) ret = -1;
  uart->data = ret;
}
