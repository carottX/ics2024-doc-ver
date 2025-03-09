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

#ifndef __X86_REG_H__
#define __X86_REG_H__

#include <isa.h>

enum { PRIV_IRET };

/**
 * Checks if the given register index is within the valid range.
 * 
 * This method ensures that the provided index is a valid register index by
 * verifying that it is greater than or equal to 0 and less than 8. If the
 * `CONFIG_RT_CHECK` macro is defined, an assertion is used to enforce this
 * condition at runtime. If the index is valid, it is returned unchanged.
 *
 * @param index The register index to be checked.
 * @return The same index if it is valid.
 */
static inline int check_reg_index(int index) {
  IFDEF(CONFIG_RT_CHECK, assert(index >= 0 && index < 8));
  return index;
}

#define reg_l(index) (cpu.gpr[check_reg_index(index)]._32)
#define reg_w(index) (cpu.gpr[check_reg_index(index)]._16)
#define reg_b(index) (cpu.gpr[check_reg_index(index) & 0x3]._8[index >> 2])

/**
 * Returns the name of a register based on its index and width.
 *
 * This method takes an index and a width as input and returns the corresponding
 * register name. The register names are stored in three external arrays:
 * - `regsl[]` for 4-byte (32-bit) registers,
 * - `regsw[]` for 2-byte (16-bit) registers,
 * - `regsb[]` for 1-byte (8-bit) registers.
 *
 * The index must be a non-negative integer less than 8, as there are 8 registers
 * in each category. If the index is out of bounds, an assertion will be triggered
 * when `CONFIG_RT_CHECK` is defined.
 *
 * The width parameter specifies the size of the register and must be one of the
 * following values:
 * - 1: 8-bit register,
 * - 2: 16-bit register,
 * - 4: 32-bit register.
 *
 * If an invalid width is provided, the method triggers an assertion.
 *
 * @param index The index of the register (must be between 0 and 7).
 * @param width The width of the register (1, 2, or 4).
 * @return The name of the register as a string.
 */
static inline const char* reg_name(int index, int width) {
  extern const char* regsl[];
  extern const char* regsw[];
  extern const char* regsb[];
  IFDEF(CONFIG_RT_CHECK, assert(index >= 0 && index < 8));

  switch (width) {
    case 4: return regsl[index];
    case 1: return regsb[index];
    case 2: return regsw[index];
    default: assert(0);
  }
}

/**
 * @brief Retrieves the name of a segment register based on the given index.
 *
 * This function returns the name of the segment register corresponding to the specified index.
 * The segment registers are represented by the following names: "es", "cs", "ss", "ds", "fs", "gs".
 * The function performs an assertion check (if CONFIG_RT_CHECK is defined) to ensure that the index
 * is within the valid range of segment register names.
 *
 * @param index The index of the segment register. Must be a non-negative integer and less than the
 *              number of segment register names available.
 * @return A pointer to a string representing the name of the segment register. If the index is
 *         out of bounds, the behavior is undefined (assertion failure if CONFIG_RT_CHECK is defined).
 */
static inline const char* sreg_name(int index) {
  const char *name[] = { "es", "cs", "ss", "ds", "fs", "gs" };
  IFDEF(CONFIG_RT_CHECK, assert(index >= 0 && index < ARRLEN(name)));
  return name[index];
}

#endif
