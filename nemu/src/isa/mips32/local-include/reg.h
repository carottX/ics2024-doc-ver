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

#ifndef __MIPS32_REG_H__
#define __MIPS32_REG_H__

#include <common.h>

/**
 * @brief Checks if the given register index is within the valid range.
 *
 * This method ensures that the provided register index `idx` is within the
 * valid range of 0 to 31 (inclusive). If the configuration macro `CONFIG_RT_CHECK`
 * is defined, it will assert that the index is valid. If the index is valid,
 * the method returns the index unchanged.
 *
 * @param idx The register index to be checked.
 * @return int The original register index if it is valid.
 */
static inline int check_reg_idx(int idx) {
  IFDEF(CONFIG_RT_CHECK, assert(idx >= 0 && idx < 32));
  return idx;
}

#define gpr(idx) cpu.gpr[check_reg_idx(idx)]

/**
 * @brief Retrieves the name of the register corresponding to the given index.
 *
 * This function takes an integer index, validates it using `check_reg_idx`, and 
 * returns the name of the register from the global `regs` array. The `regs` array 
 * is expected to be defined externally and contain the names of all registers.
 *
 * @param idx The index of the register whose name is to be retrieved.
 * @return A pointer to a string containing the name of the register.
 */
static inline const char* reg_name(int idx) {
  extern const char* regs[];
  return regs[check_reg_idx(idx)];
}

#endif
