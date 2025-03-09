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
#include <memory/vaddr.h>
#include <memory/paddr.h>

/**
 * Translates a virtual address to a physical address using the ISA MMU (Memory Management Unit).
 *
 * This method attempts to translate a given virtual address (`vaddr`) to its corresponding physical address
 * based on the specified length (`len`) and memory access type (`type`). The translation is performed using
 * the ISA-specific MMU, which handles address mapping and memory protection.
 *
 * @param vaddr The virtual address to be translated.
 * @param len The length of the memory region to be translated (in bytes).
 * @param type The type of memory access (e.g., read, write, execute) to determine the appropriate translation.
 *
 * @return The translated physical address (`paddr_t`) on success. If the translation fails (e.g., due to an
 *         invalid virtual address or permission violation), the method returns `MEM_RET_FAIL`.
 */
paddr_t isa_mmu_translate(vaddr_t vaddr, int len, int type) {
  return MEM_RET_FAIL;
}
