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

#ifndef __LOONGARCH32R_REG_H__
#define __LOONGARCH32R_REG_H__

#include <common.h>

/**
 * @brief Checks if the given register index is within the valid range.
 *
 * This function ensures that the provided register index is within the valid range
 * of 0 to 31 (inclusive). If the `CONFIG_RT_CHECK` macro is defined, it performs
 * an assertion check to verify that the index is valid. If the index is valid, it
 * returns the index unchanged. This function is typically used to validate register
 * indices before accessing them in a hardware or emulator context.
 *
 * @param idx The register index to be checked.
 * @return int The original index if it is valid.
 */
static inline int check_reg_idx(int idx) {
  IFDEF(CONFIG_RT_CHECK, assert(idx >= 0 && idx < 32));
  return idx;
}

#define gpr(idx) cpu.gpr[check_reg_idx(idx)]

/**
 * Retrieves the name of the register corresponding to the given index.
 *
 * This function takes an integer index, validates it using `check_reg_idx`, and returns
 * the name of the register from the `regs` array. The `regs` array is an externally defined
 * array of strings where each element represents the name of a register.
 *
 * @param idx The index of the register whose name is to be retrieved.
 * @return A pointer to a string representing the name of the register. If the index is
 *         invalid, the behavior depends on the implementation of `check_reg_idx`.
 */
static inline const char* reg_name(int idx) {
  extern const char* regs[];
  return regs[check_reg_idx(idx)];
}

#endif
