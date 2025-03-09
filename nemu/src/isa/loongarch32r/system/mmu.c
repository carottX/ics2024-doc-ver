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
#include <memory/vaddr.h>

/**
 * Translates a virtual memory address to a physical memory address using the MMU (Memory Management Unit).
 * This function is typically used in systems with virtual memory to convert a virtual address (vaddr) to its
 * corresponding physical address (paddr) based on the memory management unit's translation rules.
 *
 * @param vaddr The virtual address to be translated.
 * @param len The length of the memory region to be translated (in bytes). This parameter is used to ensure
 *            that the entire memory range is valid and can be translated.
 * @param type The type of memory access being performed (e.g., read, write, execute). This can influence
 *             the translation process, especially in systems with memory protection or different memory
 *             access permissions.
 *
 * @return The physical address corresponding to the given virtual address. If the translation fails (e.g.,
 *         due to an invalid virtual address or a memory protection violation), the function returns
 *         MEM_RET_FAIL, indicating that the translation was unsuccessful.
 */
paddr_t isa_mmu_translate(vaddr_t vaddr, int len, int type) {
  return MEM_RET_FAIL;
}
