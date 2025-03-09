/* Simple implementation of a GDB remote protocol client.
 * Copyright (C) 2015 Red Hat Inc.
 *
 * This file is part of gdb-toys.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include <ctype.h>
#include <err.h>

#include <arpa/inet.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/socket.h>
#include <sys/types.h>

struct gdb_conn {
  FILE *in;
  FILE *out;
  bool ack;
};


/**
 * Converts a single hexadecimal character to its corresponding 4-bit integer value.
 * 
 * This function takes a single character representing a hexadecimal digit (0-9, a-f, A-F)
 * and returns its integer value. For example, 'A' or 'a' returns 10, 'F' or 'f' returns 15,
 * and '3' returns 3. The function handles both lowercase and uppercase hexadecimal characters.
 * 
 * @param hex The hexadecimal character to convert. It must be a valid hexadecimal digit
 *            (0-9, a-f, A-F). Behavior is undefined for invalid input.
 * 
 * @return The integer value of the hexadecimal character, ranging from 0 to 15.
 */
static uint8_t
hex_nibble(uint8_t hex) {
  return isdigit(hex) ? hex - '0' : tolower(hex) - 'a' + 10;
}

/**
 * Encodes a single digit (0-15) into its hexadecimal character representation.
 * 
 * @param digit The digit to encode. Must be in the range 0-15.
 * @return The hexadecimal character corresponding to the input digit. 
 *         For digits 0-9, returns '0' to '9'. 
 *         For digits 10-15, returns 'a' to 'f'.
 */
uint8_t hex_encode(uint8_t digit) {
  return digit > 9 ? 'a' + digit - 10 : '0' + digit;
}

/**
 * Decodes two hexadecimal characters into a 16-bit unsigned integer.
 * 
 * This function takes two 8-bit unsigned integers representing the most significant
 * byte (msb) and the least significant byte (lsb) of a hexadecimal value. It first
 * checks if both characters are valid hexadecimal digits using `isxdigit`. If either
 * character is not a valid hexadecimal digit, the function returns `UINT16_MAX` to
 * indicate an error. Otherwise, it converts each character to its corresponding 4-bit
 * nibble using `hex_nibble`, combines them into a single 16-bit value, and returns
 * the result.
 *
 * @param msb The most significant byte (first hexadecimal character).
 * @param lsb The least significant byte (second hexadecimal character).
 * @return The decoded 16-bit unsigned integer, or `UINT16_MAX` if either input is
 *         not a valid hexadecimal character.
 */
uint16_t gdb_decode_hex(uint8_t msb, uint8_t lsb) {
  if (!isxdigit(msb) || !isxdigit(lsb))
    return UINT16_MAX;
  return 16 * hex_nibble(msb) + hex_nibble(lsb);
}

/**
 * Decodes a hexadecimal string into a 64-bit unsigned integer.
 * 
 * This function takes a pointer to a sequence of bytes representing a hexadecimal string
 * and decodes it into a single 64-bit unsigned integer. The function processes the string
 * in pairs of characters (each pair representing a single byte in hexadecimal) and 
 * accumulates the result in a 64-bit integer. The decoding stops when a non-hexadecimal
 * character is encountered.
 * 
 * @param bytes A pointer to the start of the hexadecimal string to be decoded.
 * @return The decoded 64-bit unsigned integer value.
 */
uint64_t gdb_decode_hex_str(uint8_t *bytes) {
  uint64_t value = 0;
  uint64_t weight = 1;
  while (isxdigit(bytes[0]) && isxdigit(bytes[1])) {
    value += weight * gdb_decode_hex(bytes[0], bytes[1]);
    bytes += 2;
    weight *= 16 * 16;
  }
  return value;
}


/**
 * Initializes a GDB connection structure for communication over a file descriptor.
 *
 * This function allocates and initializes a `gdb_conn` structure, which is used to manage
 * the state of a GDB remote protocol connection. It duplicates the provided file descriptor
 * to separate read and write operations, and opens `FILE*` streams for both reading and writing.
 * The connection is initialized with acknowledgment (`ack`) enabled, and any pending input
 * is acknowledged by sending a '+' character to the GDB client.
 *
 * @param fd The file descriptor representing the connection to the GDB client.
 * @return A pointer to the newly allocated and initialized `gdb_conn` structure.
 * @throws Exits the program with an error message if memory allocation, file descriptor
 *         duplication, or `FILE*` stream creation fails.
 */
static struct gdb_conn* gdb_begin(int fd) {
  struct gdb_conn *conn = calloc(1, sizeof(struct gdb_conn));
  if (conn == NULL)
    err(1, "calloc");

  conn->ack = true;

  // duplicate the handle to separate read/write state
  int fd2 = dup(fd);
  if (fd2 < 0)
    err(1, "dup");

  // open a FILE* for reading
  conn->in = fdopen(fd, "rb");
  if (conn->in == NULL)
    err(1, "fdopen");

  // open a FILE* for writing
  conn->out = fdopen(fd2, "wb");
  if (conn->out == NULL)
    err(1, "fdopen");

  // reset line state by acking any earlier input
  fputc('+', conn->out);
  fflush(conn->out);

  return conn;
}

/**
 * @brief Establishes a TCP connection to a GDB server at the specified address and port.
 *
 * This function initializes a socket and connects to a GDB server using the provided
 * IPv4 address and port. It configures the socket with `SO_KEEPALIVE` and `TCP_NODELAY`
 * options to ensure efficient communication. If the connection is successful, it
 * initializes a GDB connection using the socket file descriptor.
 *
 * @param addr The IPv4 address of the GDB server as a null-terminated string.
 * @param port The port number of the GDB server.
 * @return A pointer to a `gdb_conn` structure representing the GDB connection on success,
 *         or `NULL` if the connection fails.
 * @note The function terminates the program with an error message if the address is invalid
 *       or if the socket cannot be created.
 */
struct gdb_conn* gdb_begin_inet(const char *addr, uint16_t port) {
  // fill the socket information
  struct sockaddr_in sa = {
    .sin_family = AF_INET,
    .sin_port = htons(port),
  };
  if (inet_aton(addr, &sa.sin_addr) == 0)
    errx(1, "Invalid address: %s", addr);

  // open the socket and start the tcp connection
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    err(1, "socket");
  if (connect(fd, (const struct sockaddr *)&sa, sizeof(sa)) != 0) {
    close(fd);
    return NULL;
  }

  socklen_t tmp;
  tmp = 1;
  int r = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&tmp, sizeof(tmp));
  if (r) {
    perror("setsockopt");
    assert(0);
  }
  tmp = 1;
  r = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&tmp, sizeof(tmp));
  if (r) {
    perror("setsockopt");
    assert(0);
  }

  // initialize the rest of gdb on this handle
  return gdb_begin(fd);
}


/**
 * Closes the connection to the GDB server and frees the associated resources.
 * This function closes the input and output file streams used for communication
 * with the GDB server and then releases the memory allocated for the connection
 * structure.
 *
 * @param conn A pointer to the gdb_conn structure representing the connection
 *             to the GDB server. This structure must have been previously
 *             initialized and used for communication.
 */
void gdb_end(struct gdb_conn *conn) {
  fclose(conn->in);
  fclose(conn->out);
  free(conn);
}

/**
 * Sends a packet to the specified output stream in a format compatible with the GDB remote protocol.
 * 
 * The packet consists of:
 * - A start character '$'.
 * - The raw command data provided in the `command` buffer.
 * - A checksum computed as the sum of all bytes in the command modulo 256, formatted as a two-digit hexadecimal value prefixed by '#'.
 * 
 * The packet is immediately flushed to the output stream. If an error occurs during writing or if the
 * output stream is closed, the function will terminate the program with an appropriate error message.
 * 
 * @param out     The output stream to which the packet is written. Must be a valid FILE pointer.
 * @param command The buffer containing the command data to be sent. Must not be NULL.
 * @param size    The size of the command data in bytes. Must be a non-negative value.
 * 
 * @note This function does not perform any escaping or run-length encoding (RLE) on the command data.
 *       Such transformations, if required, should be handled at a higher level.
 */
static void send_packet(FILE *out, const uint8_t *command, size_t size) {
  // compute the checksum -- simple mod256 addition
  uint8_t sum = 0;
  size_t i;
  for (i = 0; i < size; ++i)
    sum += command[i];

  // NB: seems neither escaping nor RLE is generally expected by
  // gdbserver.  e.g. giving "invalid hex digit" on an RLE'd address.
  // So just write raw here, and maybe let higher levels escape/RLE.

  fputc('$', out); // packet start
  fwrite(command, 1, size, out); // payload
  fprintf(out, "#%02X", sum); // packet end, checksum
  fflush(out);

  if (ferror(out))
    err(1, "send");
  else if (feof(out))
    errx(0, "send: Connection closed");
}

/**
 * Sends a command to the GDB server and waits for an acknowledgment if required.
 *
 * This function sends a command packet to the GDB server through the specified connection.
 * If the connection is configured to require acknowledgment (conn->ack is true), the function
 * will wait for a response from the server. The server is expected to respond with either
 * a '+' character to acknowledge the packet or a '-' character to request a resend. The
 * function will continue to resend the packet until it receives a '+' acknowledgment or
 * the connection is configured to not require acknowledgment.
 *
 * @param conn Pointer to the GDB connection structure containing the input and output streams.
 * @param command Pointer to the command data to be sent.
 * @param size The size of the command data in bytes.
 */
void gdb_send(struct gdb_conn *conn, const uint8_t *command, size_t size) {
  bool acked = false;
  do {
    send_packet(conn->out, command, size);

    if (!conn->ack)
      break;

    // look for '+' ACK or '-' NACK/resend
    acked = fgetc(conn->in) == '+';
  } while (!acked);
}

/**
 * @brief Receives a packet from the input stream and processes it according to a custom protocol.
 *
 * This function reads data from the provided FILE stream (`in`) and processes it to extract a packet.
 * The packet is expected to start with a '$' character and end with a '#' character. The function
 * handles escape sequences (marked by '}'), run-length encoding (marked by '*'), and checksum validation.
 * The packet is stored in a dynamically allocated buffer, and its size and checksum validity are returned
 * via the `ret_size` and `ret_sum_ok` parameters, respectively.
 *
 * @param in The input FILE stream from which the packet is read.
 * @param ret_size A pointer to a size_t variable where the size of the received packet will be stored.
 * @param ret_sum_ok A pointer to a bool variable where the checksum validation result will be stored.
 *
 * @return A pointer to the dynamically allocated buffer containing the received packet. The buffer is
 *         null-terminated for safety. The caller is responsible for freeing this memory.
 *
 * @note The function handles the following special characters:
 *       - '$': Marks the start of a new packet.
 *       - '#': Marks the end of the current packet and triggers checksum validation.
 *       - '}': Indicates that the next character is escaped (XORed with 0x20).
 *       - '*': Indicates run-length encoding, where the next character specifies the repeat count.
 *
 * @warning The function terminates the program with an error message if any of the following occur:
 *          - Memory allocation fails.
 *          - An invalid run-length encoding count is encountered.
 *          - An I/O error occurs while reading from the input stream.
 *          - The connection is closed unexpectedly.
 */
static uint8_t* recv_packet(FILE *in, size_t *ret_size, bool* ret_sum_ok) {
  size_t i = 0;
  size_t size = 4096;
  uint8_t *reply = malloc(size);
  if (reply == NULL)
    err(1, "malloc");

  int c;
  uint8_t sum = 0;
  bool escape = false;

  // fast-forward to the first start of packet
  while ((c = fgetc(in)) != EOF && c != '$');

  while ((c = fgetc(in)) != EOF) {
    sum += c;
    switch (c) {
      case '$': // new packet?  start over...
        i = 0;
        sum = 0;
        escape = false;
        continue;

      case '#': // end of packet
        sum -= c; // not part of the checksum
        {
          uint8_t msb = fgetc(in);
          uint8_t lsb = fgetc(in);
          *ret_sum_ok = sum == gdb_decode_hex(msb, lsb);
        }
        *ret_size = i;

        // terminate it for good measure
        if (i == size) {
          reply = realloc(reply, size + 1);
          if (reply == NULL)
            err(1, "realloc");
        }
        reply[i] = '\0';

        return reply;

      case '}': // escape: next char is XOR 0x20
        escape = true;
        continue;

      case '*': // run-length-encoding
        // The next character tells how many times to repeat the last
        // character we saw.  The count is added to 29, so that the
        // minimum-beneficial RLE 3 is the first printable character ' '.
        // The count character can't be >126 or '$'/'#' packet markers.

        if (i > 0) { // need something to repeat!
          int c2 = fgetc(in);
          if (c2 < 29 || c2 > 126 || c2 == '$' || c2 == '#') {
            // invalid count character!
            ungetc(c2, in);
          } else {
            int count = c2 - 29;

            // get a bigger buffer if needed
            if (i + count > size) {
              size *= 2;
              reply = realloc(reply, size);
              if (reply == NULL)
                err(1, "realloc");
            }

            // fill the repeated character
            memset(&reply[i], reply[i - 1], count);
            i += count;
            sum += c2;
            continue;
          }
        }
    }

    // XOR an escaped character
    if (escape) {
      c ^= 0x20;
      escape = false;
    }

    // get a bigger buffer if needed
    if (i == size) {
      size *= 2;
      reply = realloc(reply, size);
      if (reply == NULL)
        err(1, "realloc");
    }

    // add one character
    reply[i++] = c;
  }

  if (ferror(in))
    err(1, "recv");
  else if (feof(in))
    errx(0, "recv: Connection closed");
  else
    errx(1, "recv: Unknown connection error");
}

/**
 * Receives a packet from the GDB connection and handles acknowledgment.
 *
 * This function reads a packet from the GDB connection's input stream and verifies its checksum.
 * If acknowledgment is enabled (`conn->ack`), it sends a '+' to acknowledge a valid packet or a '-'
 * to indicate a checksum error, retrying until a valid packet is received. If acknowledgment is
 * disabled, the function returns the packet immediately without any retries.
 *
 * @param conn Pointer to the GDB connection structure.
 * @param size Pointer to a variable where the size of the received packet will be stored.
 * @return A pointer to the received packet data, or NULL if an error occurs.
 */
uint8_t* gdb_recv(struct gdb_conn *conn, size_t *size) {
  uint8_t *reply;
  bool acked = false;
  do {
    reply = recv_packet(conn->in, size, &acked);

    if (!conn->ack)
      break;

    // send +/- depending on checksum result, retry if needed
    fputc(acked ? '+' : '-', conn->out);
    fflush(conn->out);
  } while (!acked);

  return reply;
}

/**
 * @brief Initiates the NoAck mode in the GDB connection.
 *
 * This method sends the "QStartNoAckMode" command to the GDB server to disable
 * the acknowledgment mechanism for subsequent commands. If the server responds
 * with "OK", the acknowledgment mode is disabled in the connection object.
 *
 * @param conn Pointer to the GDB connection structure.
 * @return Returns "OK" if the NoAck mode was successfully enabled, otherwise
 *         returns an empty string.
 */
const char* gdb_start_noack(struct gdb_conn *conn) {
  static const char cmd[] = "QStartNoAckMode";
  gdb_send(conn, (const uint8_t *)cmd, sizeof(cmd) - 1);

  size_t size;
  uint8_t *reply = gdb_recv(conn, &size);
  bool ok = size == 2 && !strcmp((const char*)reply, "OK");
  free(reply);

  if (ok)
    conn->ack = false;
  return ok ? "OK" : "";
}
