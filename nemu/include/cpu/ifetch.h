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

#ifndef __CPU_IFETCH_H__

#include <memory/vaddr.h>

/**
 * Fetches an instruction from memory at the specified virtual address and updates the program counter.
 *
 * This function retrieves an instruction of a given length from memory at the address pointed to by `pc`.
 * After fetching the instruction, the program counter (`pc`) is incremented by the length of the instruction,
 * effectively moving it to the next instruction in the sequence.
 *
 * @param pc A pointer to the virtual address (vaddr_t) of the program counter. This address is used to fetch
 *           the instruction and is updated to point to the next instruction after the fetch.
 * @param len The length of the instruction to fetch, in bytes.
 * @return The fetched instruction as a 32-bit unsigned integer.
 */
static inline uint32_t inst_fetch(vaddr_t *pc, int len) {
  uint32_t inst = vaddr_ifetch(*pc, len);
  (*pc) += len;
  return inst;
}

#endif
