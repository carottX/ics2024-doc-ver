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

#ifndef __MEMORY_HOST_H__
#define __MEMORY_HOST_H__

#include <common.h>

/**
 * Reads a value of a specified length from a given memory address.
 * 
 * This function reads a value from the memory location pointed to by `addr` and returns it.
 * The length of the value to be read is specified by the `len` parameter. The function supports
 * reading 1-byte, 2-byte, 4-byte, and optionally 8-byte values depending on the configuration.
 * 
 * @param addr The memory address from which to read the value.
 * @param len The length of the value to read, in bytes. Supported values are 1, 2, 4, and optionally 8.
 * 
 * @return The value read from the memory address. The return type is `word_t`, which is expected to
 *         be a type that can hold the largest value read by this function (e.g., `uint64_t` for 8-byte reads).
 *         If `len` is not a supported value, the behavior depends on the configuration:
 *         - If `CONFIG_RT_CHECK` is defined, the function asserts and terminates the program.
 *         - Otherwise, the function returns 0.
 */
static inline word_t host_read(void *addr, int len) {
  switch (len) {
    case 1: return *(uint8_t  *)addr;
    case 2: return *(uint16_t *)addr;
    case 4: return *(uint32_t *)addr;
    IFDEF(CONFIG_ISA64, case 8: return *(uint64_t *)addr);
    default: MUXDEF(CONFIG_RT_CHECK, assert(0), return 0);
  }
}

/**
 * @brief Writes a value of specified length to a memory address.
 *
 * This function writes the provided `data` value to the memory location pointed to by `addr`.
 * The length of the data to be written is specified by the `len` parameter, which determines
 * the size of the memory operation (1, 2, 4, or 8 bytes). The function uses a switch statement
 * to handle different lengths and performs the appropriate type casting to ensure the correct
 * memory operation. If the `CONFIG_ISA64` macro is defined, the function supports writing
 * 8-byte values. If the `CONFIG_RT_CHECK` macro is defined and an unsupported length is provided,
 * the function triggers an assertion failure.
 *
 * @param addr Pointer to the memory location where the data will be written.
 * @param len Length of the data to be written (1, 2, 4, or 8 bytes).
 * @param data The value to be written to the memory location.
 */
static inline void host_write(void *addr, int len, word_t data) {
  switch (len) {
    case 1: *(uint8_t  *)addr = data; return;
    case 2: *(uint16_t *)addr = data; return;
    case 4: *(uint32_t *)addr = data; return;
    IFDEF(CONFIG_ISA64, case 8: *(uint64_t *)addr = data; return);
    IFDEF(CONFIG_RT_CHECK, default: assert(0));
  }
}

#endif
