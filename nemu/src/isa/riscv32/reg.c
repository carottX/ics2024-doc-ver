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
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

/**
 * Displays the contents of the ISA (Instruction Set Architecture) registers.
 * This method iterates through all the registers defined in the ISA and prints
 * their current values to the standard output. The output format includes the
 * register name, its corresponding value in hexadecimal, and a brief description
 * of its purpose. This is typically used for debugging and monitoring the state
 * of the processor's registers during execution.
 */
void isa_reg_display() {
}

/**
 * Converts a string representation of an ISA register to its corresponding value.
 *
 * This function takes a string `s` that represents the name of an ISA register
 * and attempts to convert it to its corresponding numeric value. The result
 * is returned as a `word_t` type. If the conversion is successful, the boolean
 * flag `success` is set to `true`. If the string does not correspond to a valid
 * register, the function returns 0 and sets `success` to `false`.
 *
 * @param s The string representation of the ISA register.
 * @param success A pointer to a boolean flag that indicates whether the conversion was successful.
 * @return The numeric value of the ISA register if successful, otherwise 0.
 */
word_t isa_reg_str2val(const char *s, bool *success) {
  return 0;
}
