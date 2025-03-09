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
#include <memory/paddr.h>

/**
 * Fetches an instruction from the specified virtual address.
 *
 * This function is responsible for fetching an instruction of a given length
 * from the provided virtual address. It internally translates the virtual
 * address to a physical address and reads the instruction from memory.
 *
 * @param addr The virtual address from which to fetch the instruction.
 * @param len The length of the instruction to fetch, in bytes.
 * @return The fetched instruction as a word_t value.
 */
word_t vaddr_ifetch(vaddr_t addr, int len) {
  return paddr_read(addr, len);
}

/**
 * Reads a value from a virtual address in memory.
 *
 * This function reads a value of a specified length from a given virtual address.
 * It internally converts the virtual address to a physical address and performs
 * the read operation using the `paddr_read` function.
 *
 * @param addr The virtual address from which to read the value.
 * @param len The length of the value to read, typically in bytes.
 * @return The value read from the virtual address.
 */
word_t vaddr_read(vaddr_t addr, int len) {
  return paddr_read(addr, len);
}

/**
 * Writes a word of data to a specified virtual address.
 *
 * This function writes a word of data to the given virtual address by translating
 * the virtual address to a physical address and then performing the write operation.
 * The length of the data to be written is specified by the `len` parameter.
 *
 * @param addr The virtual address where the data will be written.
 * @param len  The length of the data to be written, in bytes.
 * @param data The word of data to be written to the specified address.
 */
void vaddr_write(vaddr_t addr, int len, word_t data) {
  paddr_write(addr, len, data);
}
