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

#ifndef __RISCV_REG_H__
#define __RISCV_REG_H__

#include <common.h>

/**
 * @brief Checks if the given register index is within the valid range.
 *
 * This method validates that the provided register index `idx` is within the
 * acceptable bounds for the current configuration. If the configuration
 * `CONFIG_RT_CHECK` is enabled, it asserts that the index is non-negative and
 * less than the maximum number of registers, which is determined by the
 * configuration. Specifically, the maximum number of registers is 16 if
 * `CONFIG_RVE` is defined, otherwise it is 32.
 *
 * @param idx The register index to be checked.
 * @return The original register index `idx` if it is valid.
 */
static inline int check_reg_idx(int idx) {
  IFDEF(CONFIG_RT_CHECK, assert(idx >= 0 && idx < MUXDEF(CONFIG_RVE, 16, 32)));
  return idx;
}

#define gpr(idx) (cpu.gpr[check_reg_idx(idx)])

/**
 * Retrieves the name of the register corresponding to the given index.
 *
 * This function takes an integer index and returns the name of the register
 * associated with that index. The index is first validated using the
 * `check_reg_idx` function to ensure it is within the valid range of register
 * indices. If the index is valid, the function returns the corresponding
 * register name from the `regs` array. If the index is invalid, the behavior
 * depends on the implementation of `check_reg_idx`.
 *
 * @param idx The index of the register whose name is to be retrieved.
 * @return A pointer to a string containing the name of the register.
 */
static inline const char* reg_name(int idx) {
  extern const char* regs[];
  return regs[check_reg_idx(idx)];
}

#endif
