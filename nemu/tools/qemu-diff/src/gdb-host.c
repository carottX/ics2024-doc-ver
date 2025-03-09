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

#include "common.h"

static struct gdb_conn *conn;

/**
 * Connects to a QEMU GDB server running on localhost at the specified port.
 * This method continuously attempts to establish a connection to the GDB server
 * using the `gdb_begin_inet` function. If the connection fails, it waits for
 * a short period (1 microsecond) before retrying. The method only returns once
 * a successful connection is established.
 *
 * @param port The port number on which the QEMU GDB server is listening.
 * @return Returns `true` once a connection to the GDB server is successfully established.
 */
bool gdb_connect_qemu(int port) {
  // connect to gdbserver on localhost port 1234
  while ((conn = gdb_begin_inet("127.0.0.1", port)) == NULL) {
    usleep(1);
  }

  return true;
}

/**
 * Copies a block of memory from the host to the QEMU emulator using the GDB protocol.
 *
 * This function allocates a buffer to format the memory copy command and data according to the GDB protocol.
 * The command is constructed as "M<address>,<length>:<data>", where <address> is the destination address in QEMU,
 * <length> is the number of bytes to copy, and <data> is the hexadecimal representation of the memory block.
 *
 * The function sends the formatted command to QEMU via the GDB connection and waits for a response.
 * If the response is "OK", the memory copy operation is considered successful.
 *
 * @param dest The destination address in QEMU where the memory block should be copied.
 * @param src A pointer to the source memory block on the host.
 * @param len The number of bytes to copy from the source to the destination.
 *
 * @return true if the memory copy operation was successful (QEMU responded with "OK"), false otherwise.
 */
static bool gdb_memcpy_to_qemu_small(uint32_t dest, void *src, int len) {
  char *buf = malloc(len * 2 + 128);
  assert(buf != NULL);
  int p = sprintf(buf, "M0x%x,%x:", dest, len);
  int i;
  for (i = 0; i < len; i ++) {
    p += sprintf(buf + p, "%c%c", hex_encode(((uint8_t *)src)[i] >> 4), hex_encode(((uint8_t *)src)[i] & 0xf));
  }

  gdb_send(conn, (const uint8_t *)buf, strlen(buf));
  free(buf);

  size_t size;
  uint8_t *reply = gdb_recv(conn, &size);
  bool ok = !strcmp((const char*)reply, "OK");
  free(reply);

  return ok;
}

/**
 * Copies a block of memory from the host to a specified destination in QEMU.
 * The method handles memory transfers in chunks, ensuring that each chunk does
 * not exceed the Maximum Transmission Unit (MTU) size of 1500 bytes. If the
 * total length of the memory block to be copied exceeds the MTU, the method
 * divides the block into smaller chunks and copies each chunk sequentially.
 *
 * @param dest The destination address in QEMU where the memory will be copied.
 * @param src A pointer to the source memory block in the host.
 * @param len The total length of the memory block to be copied.
 * @return Returns `true` if all memory chunks were successfully copied to QEMU,
 *         otherwise returns `false` if any chunk fails to copy.
 */
bool gdb_memcpy_to_qemu(uint32_t dest, void *src, int len) {
  const int mtu = 1500;
  bool ok = true;
  while (len > mtu) {
    ok &= gdb_memcpy_to_qemu_small(dest, src, mtu);
    dest += mtu;
    src += mtu;
    len -= mtu;
  }
  ok &= gdb_memcpy_to_qemu_small(dest, src, len);
  return ok;
}

/**
 * @brief Retrieves the register values from the GDB server and decodes them into the provided union.
 *
 * This method sends a 'g' command to the GDB server to request the current register values. 
 * It then receives the response, which is expected to be a hexadecimal string representing the 
 * register values. The method decodes this hexadecimal string and stores the values in the 
 * provided union `isa_gdb_regs`. The union is assumed to be an array of 32-bit integers, and 
 * the method processes the response in 8-byte chunks, decoding each chunk into a 32-bit integer 
 * and storing it in the corresponding position in the union's array.
 *
 * @param r A pointer to the union `isa_gdb_regs` where the decoded register values will be stored.
 * @return Always returns `true` to indicate successful completion of the operation.
 */
bool gdb_getregs(union isa_gdb_regs *r) {
  gdb_send(conn, (const uint8_t *)"g", 1);
  size_t size;
  uint8_t *reply = gdb_recv(conn, &size);

  int i;
  uint8_t *p = reply;
  uint8_t c;
  for (i = 0; i < sizeof(union isa_gdb_regs) / sizeof(uint32_t); i ++) {
    c = p[8];
    p[8] = '\0';
    r->array[i] = gdb_decode_hex_str(p);
    p[8] = c;
    p += 8;
  }

  free(reply);

  return true;
}

/**
 * Sets the registers of the target process using the GDB protocol.
 *
 * This function serializes the contents of the provided `isa_gdb_regs` union into a GDB-compatible
 * format and sends it to the GDB server. The registers are encoded as a hexadecimal string prefixed
 * with 'G', which is the GDB command for setting registers. After sending the command, the function
 * waits for a response from the GDB server and checks if the operation was successful by comparing
 * the response to "OK".
 *
 * @param r A pointer to a union `isa_gdb_regs` containing the register values to be set.
 * @return `true` if the GDB server responded with "OK", indicating success; otherwise, `false`.
 */
bool gdb_setregs(union isa_gdb_regs *r) {
  int len = sizeof(union isa_gdb_regs);
  char *buf = malloc(len * 2 + 128);
  assert(buf != NULL);
  buf[0] = 'G';

  void *src = r;
  int p = 1;
  int i;
  for (i = 0; i < len; i ++) {
    p += sprintf(buf + p, "%c%c", hex_encode(((uint8_t *)src)[i] >> 4), hex_encode(((uint8_t *)src)[i] & 0xf));
  }

  gdb_send(conn, (const uint8_t *)buf, strlen(buf));
  free(buf);

  size_t size;
  uint8_t *reply = gdb_recv(conn, &size);
  bool ok = !strcmp((const char*)reply, "OK");
  free(reply);

  return ok;
}

/**
 * Sends a 'vCont;s:1' command to the GDB server to request a single-step execution
 * of the target process. This command is used to step one machine instruction at
 * a time in the debugged program. The method sends the command to the GDB server
 * via the specified connection, waits for a response, and then frees the received
 * reply buffer. The method always returns `true`, indicating successful execution
 * of the command, regardless of the server's response.
 *
 * @return bool Always returns `true` to indicate successful execution of the command.
 */
bool gdb_si() {
  char buf[] = "vCont;s:1";
  gdb_send(conn, (const uint8_t *)buf, strlen(buf));
  size_t size;
  uint8_t *reply = gdb_recv(conn, &size);
  free(reply);
  return true;
}

/**
 * @brief Terminates the GDB connection and exits the program.
 *
 * This function calls the `gdb_end` method to properly close the connection
 * to the GDB server, ensuring that all resources are released and the session
 * is cleanly terminated. It is typically used to gracefully exit the program
 * after completing debugging operations.
 */
void gdb_exit() {
  gdb_end(conn);
}
