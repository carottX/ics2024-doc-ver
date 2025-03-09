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

#include <isa.h>
#include <memory/host.h>
#include <memory/vaddr.h>
#include <device/map.h>

#define IO_SPACE_MAX (32 * 1024 * 1024)

static uint8_t *io_space = NULL;
static uint8_t *p_space = NULL;

/**
 * Allocates a new block of memory from a pre-defined memory space.
 *
 * This function allocates a block of memory of the specified size, ensuring that the
 * allocated block is page-aligned. The size is rounded up to the nearest multiple of
 * the page size. The function updates the global pointer `p_space` to point to the
 * next available memory location after the allocated block. It also ensures that the
 * total allocated memory does not exceed the maximum allowed memory space (`IO_SPACE_MAX`).
 *
 * @param size The size of the memory block to allocate. This size is rounded up to the
 *             nearest multiple of the page size.
 *
 * @return A pointer to the beginning of the allocated memory block. The returned pointer
 *         is page-aligned.
 *
 * @note This function assumes that `p_space` and `io_space` are global variables that
 *       track the current position in the memory space and the base of the memory space,
 *       respectively. It also assumes that `PAGE_SIZE` and `PAGE_MASK` are defined constants
 *       representing the page size and the page mask, respectively.
 *
 * @warning This function asserts that the allocated memory does not exceed the maximum
 *          allowed memory space (`IO_SPACE_MAX`). If this condition is violated, the
 *          program will terminate with an assertion failure.
 */
uint8_t* new_space(int size) {
  uint8_t *p = p_space;
  // page aligned;
  size = (size + (PAGE_SIZE - 1)) & ~PAGE_MASK;
  p_space += size;
  assert(p_space - io_space < IO_SPACE_MAX);
  return p;
}

/**
 * @brief Checks if the given physical address is within the bounds of the specified I/O map.
 *
 * This function verifies whether the provided physical address `addr` falls within the valid
 * range defined by the `low` and `high` fields of the `IOMap` structure. If the `map` pointer
 * is `NULL`, the function asserts that the address is out of bounds. If the `map` is not `NULL`,
 * the function asserts that the address is within the range `[map->low, map->high]`. If the
 * address is out of bounds, an assertion failure is triggered, and an error message is printed
 * indicating the out-of-bound address, the map's name (if applicable), and the program counter
 * (pc) at the time of the check.
 *
 * @param map Pointer to the IOMap structure defining the valid address range. If `NULL`, the
 *            address is considered out of bounds.
 * @param addr The physical address to be checked against the bounds of the I/O map.
 */
static void check_bound(IOMap *map, paddr_t addr) {
  if (map == NULL) {
    Assert(map != NULL, "address (" FMT_PADDR ") is out of bound at pc = " FMT_WORD, addr, cpu.pc);
  } else {
    Assert(addr <= map->high && addr >= map->low,
        "address (" FMT_PADDR ") is out of bound {%s} [" FMT_PADDR ", " FMT_PADDR "] at pc = " FMT_WORD,
        addr, map->name, map->low, map->high, cpu.pc);
  }
}

/**
 * Invokes the specified I/O callback function if it is not NULL.
 *
 * This method is used to trigger an I/O callback, which is typically used to handle
 * I/O operations such as reading from or writing to a memory-mapped device. The callback
 * is invoked with the provided parameters: the memory offset, the length of the operation,
 * and a flag indicating whether the operation is a write.
 *
 * @param c        The I/O callback function to invoke. If NULL, the method does nothing.
 * @param offset   The memory offset where the I/O operation is to be performed.
 * @param len      The length of the data involved in the I/O operation.
 * @param is_write A boolean flag indicating whether the operation is a write (true) or a read (false).
 */
static void invoke_callback(io_callback_t c, paddr_t offset, int len, bool is_write) {
  if (c != NULL) { c(offset, len, is_write); }
}

/**
 * Initializes the I/O space map by allocating memory for the I/O operations.
 * This function allocates a block of memory of size `IO_SPACE_MAX` and assigns
 * the starting address of this block to the global pointer `io_space`. It also
 * initializes the pointer `p_space` to point to the beginning of the allocated
 * memory. If the memory allocation fails, the program will terminate via an
 * assertion failure.
 *
 * @note The caller is responsible for ensuring that `IO_SPACE_MAX` is defined
 *       and represents the maximum size of the I/O space required.
 * @note The allocated memory should be freed by the caller when no longer needed
 *       to avoid memory leaks.
 */
void init_map() {
  io_space = malloc(IO_SPACE_MAX);
  assert(io_space);
  p_space = io_space;
}

/**
 * Reads a word of specified length from a memory-mapped I/O region.
 *
 * This function reads a word of length `len` (in bytes) from the memory-mapped I/O region
 * defined by the `map` parameter, starting at the physical address `addr`. The function
 * ensures that the read operation is within the bounds of the mapped region and invokes
 * a callback to prepare the data for reading. The actual read operation is performed
 * by the `host_read` function, which reads from the mapped memory space.
 *
 * @param addr The physical address from which to read the data.
 * @param len The length of the data to read, in bytes. Must be between 1 and 8, inclusive.
 * @param map A pointer to the IOMap structure defining the memory-mapped region.
 *
 * @return The word read from the specified address. The size of the word is determined by `len`.
 *
 * @note This function asserts that `len` is within the valid range (1 to 8).
 * @note The function checks that the address `addr` is within the bounds of the mapped region.
 * @note The callback function associated with the map is invoked to prepare the data for reading.
 */
word_t map_read(paddr_t addr, int len, IOMap *map) {
  assert(len >= 1 && len <= 8);
  check_bound(map, addr);
  paddr_t offset = addr - map->low;
  invoke_callback(map->callback, offset, len, false); // prepare data to read
  word_t ret = host_read(map->space + offset, len);
  return ret;
}

/**
 * Writes data to a mapped I/O memory region.
 *
 * This function writes a specified amount of data (`len` bytes) to a memory region
 * defined by the I/O map (`map`). The write operation is performed at the given
 * physical address (`addr`) within the mapped region. The function ensures that
 * the write operation is within the bounds of the mapped region and that the length
 * of the data is valid (between 1 and 8 bytes). After writing the data, the function
 * invokes a callback to notify the completion of the write operation.
 *
 * @param addr The physical address within the mapped region where the data will be written.
 * @param len The number of bytes to write (must be between 1 and 8 inclusive).
 * @param data The data to be written to the memory region.
 * @param map A pointer to the IOMap structure defining the memory region and its properties.
 *
 * @note The function asserts that `len` is within the valid range (1 to 8 bytes).
 * @note The function checks that the address is within the bounds of the mapped region.
 * @note The callback is invoked with the offset within the mapped region, the length of the
 *       write operation, and a boolean indicating that the operation was a write.
 */
void map_write(paddr_t addr, int len, word_t data, IOMap *map) {
  assert(len >= 1 && len <= 8);
  check_bound(map, addr);
  paddr_t offset = addr - map->low;
  host_write(map->space + offset, len, data);
  invoke_callback(map->callback, offset, len, true);
}
