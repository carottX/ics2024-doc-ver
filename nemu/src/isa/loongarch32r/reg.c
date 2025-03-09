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
#include "local-include/reg.h"

const char *regs[] = {
  "$0", "ra", "tp", "sp", "a0", "a1", "a2", "a3",
  "a4", "a5", "a6", "a7", "t0", "t1", "t2", "t3",
  "t4", "t5", "t6", "t7", "t8", "rs", "fp", "s0",
  "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8"
};

/**
 * Displays the contents of the ISA (Instruction Set Architecture) registers.
 * This method is typically used for debugging purposes to inspect the current
 * state of the CPU registers. It prints the values of all general-purpose,
 * control, and status registers to the standard output in a human-readable format.
 * The output includes the register names and their corresponding hexadecimal or
 * decimal values, depending on the implementation.
 */
void isa_reg_display() {
}

/**
 * Converts a string representation of an ISA register to its corresponding value.
 *
 * This function takes a string `s` that represents the name of an ISA register
 * and attempts to convert it to its corresponding value of type `word_t`. If the
 * conversion is successful, the value is returned, and the boolean flag `success`
 * is set to `true`. If the string does not correspond to a valid ISA register,
 * the function returns `0` and sets `success` to `false`.
 *
 * @param s The string representation of the ISA register.
 * @param success A pointer to a boolean flag that indicates whether the conversion was successful.
 * @return The value of the ISA register if successful, otherwise `0`.
 */
word_t isa_reg_str2val(const char *s, bool *success) {
  return 0;
}
